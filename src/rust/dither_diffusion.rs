//! Experimental Rust equivalent of the C error-diffusion kernel.
//! The OBS graphics/readback/upload path remains native C.
//! Build only when ENABLE_RUST_DITHER is explicitly enabled.

use std::ffi::c_int;

type Weight = (isize, isize, f32);
const FLOYD: &[Weight] = &[
    (1, 0, 7.0 / 16.0),
    (-1, 1, 3.0 / 16.0),
    (0, 1, 5.0 / 16.0),
    (1, 1, 1.0 / 16.0),
];
const ATKINSON: &[Weight] = &[
    (1, 0, 1.0 / 8.0),
    (2, 0, 1.0 / 8.0),
    (-1, 1, 1.0 / 8.0),
    (0, 1, 1.0 / 8.0),
    (1, 1, 1.0 / 8.0),
    (0, 2, 1.0 / 8.0),
];
const SIERRA_LITE: &[Weight] = &[
    (1, 0, 2.0 / 4.0),
    (-1, 1, 1.0 / 4.0),
    (0, 1, 1.0 / 4.0),
];

fn diffuse(
    source: &[u8],
    output: &mut [u8],
    width: usize,
    height: usize,
    source_stride: usize,
    output_stride: usize,
    levels: u32,
    weights: &[Weight],
) -> bool {
    let pixels = width * height;
    let work_len = pixels * 3;
    let mut work = Vec::<f32>::new();
    if work.try_reserve_exact(work_len).is_err() {
        return false;
    }
    work.resize(work_len, 0.0);

    for y in 0..height {
        for x in 0..width {
            let from = y * source_stride + x * 4;
            let to = (y * width + x) * 3;
            work[to..to + 3].copy_from_slice(&[
                source[from] as f32,
                source[from + 1] as f32,
                source[from + 2] as f32,
            ]);
        }
    }

    let step = 255.0f32 / levels as f32;
    for y in 0..height {
        for x in 0..width {
            let from = y * source_stride + x * 4;
            let to = y * output_stride + x * 4;
            output[to..to + 4].copy_from_slice(&source[from..from + 4]);
            if source[from + 3] < 5 {
                continue;
            }
            let index = (y * width + x) * 3;
            for channel in 0..3 {
                let prior = work[index + channel].clamp(0.0, 255.0);
                let quantized = ((prior / step).round() * step).clamp(0.0, 255.0);
                output[to + channel] = quantized.round() as u8;
                let error = prior - quantized;
                for &(dx, dy, weight) in weights {
                    let nx = x as isize + dx;
                    let ny = y as isize + dy;
                    if nx < 0 || nx >= width as isize || ny < 0 || ny >= height as isize {
                        continue;
                    }
                    let nx = nx as usize;
                    let ny = ny as usize;
                    if source[ny * source_stride + nx * 4 + 3] != 255 {
                        continue;
                    }
                    work[(ny * width + nx) * 3 + channel] += error * weight;
                }
            }
        }
    }
    true
}

/// # Safety
///
/// Caller supplies valid readable RGBA rows at `source` and writable RGBA rows at
/// `output`, including all bytes up to the last used pixel of the final row.
/// Buffers must not overlap, and must remain alive for the duration of the call.
/// Unlike the C entrypoint, partial overlap is explicitly rejected here.
#[no_mangle]
pub unsafe extern "C" fn retro_dither_diffuse_rs(
    source: *const u8,
    width: c_int,
    height: c_int,
    source_stride: c_int,
    output: *mut u8,
    output_stride: c_int,
    levels: c_int,
    method: c_int,
) -> bool {
    if source.is_null()
        || output.is_null()
        || !(1..=4096).contains(&width)
        || !(1..=4096).contains(&height)
        || !(1..=255).contains(&levels)
    {
        return false;
    }
    let weights = match method {
        1 => FLOYD,
        2 => ATKINSON,
        3 => SIERRA_LITE,
        _ => return false,
    };
    let width = width as usize;
    let height = height as usize;
    let pixels = width * height;
    if pixels > 16_777_216 || pixels.checked_mul(3 * std::mem::size_of::<f32>()).is_none() {
        return false;
    }
    let row = width * 4;
    if source_stride < row as c_int || output_stride < row as c_int {
        return false;
    }
    let source_stride = source_stride as usize;
    let output_stride = output_stride as usize;
    let Some(input_len) = (height - 1)
        .checked_mul(source_stride)
        .and_then(|v| v.checked_add(row))
    else {
        return false;
    };
    let Some(output_len) = (height - 1)
        .checked_mul(output_stride)
        .and_then(|v| v.checked_add(row))
    else {
        return false;
    };
    let Some(source_end) = (source as usize).checked_add(input_len) else {
        return false;
    };
    let Some(output_end) = (output as usize).checked_add(output_len) else {
        return false;
    };
    if (source as usize) < output_end && (output as usize) < source_end {
        return false;
    }
    // SAFETY: All dimensions, pointer ranges, non-overlap and offset arithmetic
    // were validated above. Actual backing allocations are the caller's contract.
    let src = unsafe { std::slice::from_raw_parts(source, input_len) };
    let dst = unsafe { std::slice::from_raw_parts_mut(output, output_len) };
    diffuse(
        src,
        dst,
        width,
        height,
        source_stride,
        output_stride,
        levels as u32,
        weights,
    )
}
