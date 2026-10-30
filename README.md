<!-- Title -->
# OBS Retro Effects

<p align="center">
    <i align="center">Bringing the Totally Rad 80s and 90s back to life.</i>
</p>

<h4 align="center">
    <a href="https://github.com/FiniteSingularity/obs-retro-effects/releases">
        <img src="https://img.shields.io/github/v/release/finitesingularity/obs-retro-effects?filter=*&style=flat-square&label=Latest" alt="Latest release">
    </a>
    <img src="https://img.shields.io/badge/OBS-28_|_29_|_30-blue.svg?style=flat-square" alt="supports obs versions 28, 29, and 30">
    <img src="https://img.shields.io/badge/Windows-0078D6?style=flat-square&logo=windows&logoColor=white">
    <img src="https://img.shields.io/badge/mac%20os-000000?style=flat-square&logo=apple&logoColor=white">
    <img src="https://img.shields.io/badge/Linux-FCC624?style=flat-square&logo=linux&logoColor=black"><br>
    <a href="https://twitter.com/FiniteSingulrty">
        <img src="https://img.shields.io/badge/Twitter-1DA1F2?style=flat-square&logo=twitter&logoColor=white">
    </a>
    <a href="https://twitch.tv/finitesingularity">
        <img src="https://img.shields.io/badge/Twitch-9146FF?style=flat-square&logo=twitch&logoColor=white">
    </a>
</h4>

> **Note**
> While we only release new versions of Retro Effects after testing it on local hardware, there are bugs and issues that will slip through. If you happen to run into any issues, please [open an issue](https://github.com/finitesingularity/obs-retro-effects/issues) and we will work to resolve it.

## Introduction

Retro Effects is an OBS plug-in that provides several filters to simulate retro hardware (e.g.- CRTs, NTSC Signals, etc...) giving your OBS sources an authentic retro look/feel. Retro effects provides the following filters.


- [Chromatic Aberration](#chromatic-aberration)
- [Frame Skip](#frame-skip)
- [Interlace](#interlace)
- [Posterize](#posterize)
- [Dither](#dither)
- [CRT](#crt)
- [NTSC](#ntsc)
- [Cathode Boot](#cathode-boot)
- [Matrix Rain](#matrix-rain)
- [Retro Codec](#retro-codec)
- [VHS](#vhs)
- [Bloom](#bloom)
- [Scanlines](#scanlines)
- [Digital Glitch](#digital-glitch)
- [Analog Glitch](#analog-glitch)
- [Conclusion](#conclusion)

## Chromatic Aberration
This filter provides the abilility to shift the red, green, and blue color channels, either manually using the `Manual` type, or by simulating lens CA using the `Lens` type.

In Manual mode, each color channel can be individually shifted by a user specified distance and direction.

In Lens mode, color channels are shifted radially from the source's center by a user specified per-channel amount.

![chromatic aberration filter preview](.github/assets/images/chromatic-aberration.png)

## Frame Skip
The frame skip filter allows the user to replicate skipped frames or lower framerates.  It provides a single slider to specify the number of frames skipped between frame renders.

![frame skip filter preview](.github/assets/images/frame-skip.png)

## Interlace
Interlacing is a technique where alternating lines of an image are displayed in two sequential fields, creating a complete frame that refreshes half of the lines at a time, commonly used in older television broadcasts.  This leads to characteristic combing artifacts that are seen when converting interlaced video to modern progressive scan displays.

The interlacing filter allows the user to specify the interlace line thickness, brightness reduction of the alternating field lines, and alpha reduction of the alternating field lines.

![interlace filter preview](.github/assets/images/interlace.png)

## Posterize
Posterizing is a process to reduce the number of colors in an image to create a simplified, graphic-like effect, often resembling a poster or comic book style.

Users have the ability to adjust the number of color levels used in the posterization process as well as options for how color palette sampling is handled.  Options for sampling include the original image colors, a user specified 2-color gradient, or the horizontal center line of any OBS source or scene.

![posterize filter preview](.github/assets/images/posterize.png)

## Dither
Send your video back to Windows 3.1 with the dithering filter.  Dithering is a technique used on lower bit-depth displays to smooth transitions between different colors or shades by adding noise or patterns, thereby reducing the appearance of banding or color stepping.

The dithering filter now includes all the methods previously used
in the **noita-chat** emote palette pipeline:

- **Ordered, GPU:** traditional Bayer 2×2, 4×4, 8×8, and the exact
  2×2 horizontal, vertical and diagonal threshold distributions from
  noita-chat (utils/colorScience.ts). The latter three are intentionally
  directional patterns with repeated ranks, **not** standard Bayer permutations.
- **Quasi-random, GPU:** the original Hilbert R1/triangular noise mode
  (historically labeled "Unordered"; it is not a sampled blue-noise texture).
- **True sequential error diffusion, CPU:** Floyd–Steinberg, Atkinson, and
  Sierra Lite, using the actual neighbor error propagation kernels from
  noita-chat. These are not ordered-dither shaders relabeled as diffusion.

The noita-chat code quantizes against a selected arbitrary color palette;
this OBS filter instead applies the same distributions/kernels to its
existing **colors-per-channel** uniform RGB palette. Dither size,
monochrome, gamma, contrast and color depth remain configurable. The Bayer
matrix size option only appears for standard ordered Bayer, noise offset
controls appear only for the original Hilbert mode, and pixel rounding is
hidden for the CPU algorithms.

**Performance:** GPU ordered modes are single-pass. Error diffusion
requires an additional GPU-to-CPU readback, a sequential CPU propagation
pass, and a nearest-neighbor texture upload for each frame. Dither size
is at least 1 px in these CPU modes, and processing resolution is capped
to 1024 logical pixels on either axis to limit work on high-resolution
sources. On the development PC, native 1024×576 diffusion alone measured
roughly 18–23 ms per frame (not including OBS readback/upload). Do not
assume 60 fps or zero latency; use larger dither pixels to reduce CPU work.
The GPU-based modes have no CPU readback requirement.

Older ordered/unordered mode IDs and their saved settings are preserved.
These methods are available without enabling the optional FFmpeg codecs.

![dither filter preview](.github/assets/images/dither.png)

## CRT
The CRT Filter simulates various effects/artifacts as seen on older Cathode Ray Tube displays. Taste the static.

This filter provides 24 different phosphor mask layouts, and the ability to change the mask intensity. Also provides phosphor bloom with adjustable bloom size and threshold, CRT Geometry including corner radius, barrel distortion, and vignetting, and black and white level color correction.

![crt filter preview](.github/assets/images/crt.png)

## NTSC
NTSC is a video broadcast standard used in North America and parts of South America and Asia, defining how color and monochrome video signals are transmitted and displayed on analog televisions. NTSC comes with its fair share of issues, leading to some calling it "Never The Same Color."

The NTSC filter simulates encoding and decoding a source introducing adjustable artifacts commonly experienced on older analog TVs.  User adjustable parameters include:

- Tuning Offset: simulates errors in tuning in an NTSC signal
- Luma noise: adds static noise to the NTSC luma signal
- Luma banding: adjustable size, strength and luma band/ring count.
- Chroma bleed size, steps and strength
- Brightness and saturation adjustment performed in the same way as done on an analog TV.
 
![ntsc filter preview](.github/assets/images/ntsc.png)

## Cathode Boot
When turning a CRT on and off, a characteristic vertical and horizontal collapse effect is commonly seen.

The CRT Boot filter provides the user with a progress slider (animatable with the move value plugin) that simulates CRT horizontal and vertical collapse.  The user can adjust geometry through the horizontal thickness, vertical thickness, and glow parameters.  Additionally the horizontal and vertical collapse as well as final fadout timing can be modified.

![cathode boot filter preview](.github/assets/images/cathode-boot.png)

## Matrix Rain
You are now in The Matrix! The matrix rain filter uses the luminosity of a source, and animates the Matrix asthetic as seen in the iconic movie franchise of the same name.

It might not teach the user Kung-Fu but it will definitely make them say "Woahh!" by giving plenty of options to make the perfect rain effect, including:

- Several built-in character/font sets, and the ability to generate your own.
- Character scaling
- Noise shift to horizontally shift the rain
- Option to use source colors for the rain particles, or a user specified font and background color
- Black- and White-level adjustment to increase/decrease contrast.
- Minimum fade value for rain trails
- Active rain character brightness
- Rain Trail fade distance
- Rain drop speed
- Charcater bloom radius, threshold, and intensity.
- If you've read this far, you realize it is not just a movie file running in the background.

![matrix rain filter preview](.github/assets/images/matrix-rain.png)

## Retro Codec
Video conference like its 1995 and you've got a sweet new Connectix QuickCam!  The Retro Codec filter simulates RPZA video codec in all its blocky, noisy, dithered glory.

User adjustable parameters include pixel scale, colors per channel, quality, keyframe interval, and custom thresholds.

![retro codec filter preview](.github/assets/images/retro-codec.png)

### Authentic video codec gallery (this fork; optional)

The **Retro Codec** filter supports eight real FFmpeg encoder/decoder
pipelines, alongside the original shader-based RPZA. Each real mode
actually compresses the source and displays its decoded frames.
These are different algorithms, not random-noise/block-edge shaders.

| Video codec | Starter format | Encoder input | Era |
| --- | --- | --- | --- |
| H.263 Baseline | 176×144, 10 fps | YUV 4:2:0 | Early 3GP phones |
| MPEG-4 Visual Part 2 Simple | 176×144, 10 fps | YUV 4:2:0 | Later 3GP phones |
| Cinepak | 320×240, 15 fps | RGB24 | 1990s CD-ROM and QuickTime |
| Sorenson Video 1 (SVQ1) | 320×240, 15 fps | YUV 4:1:0 | QuickTime web clips |
| Microsoft Video 1 | 160×120, 15 fps | RGB555LE (15-bit color) | Video for Windows / AVI |
| Sorenson Spark / FLV1 | 320×240, 15 fps | YUV 4:2:0 | Early Flash web video |
| MPEG-1 | 352×288, 25 fps | YUV 4:2:0 | PAL Video CD-era |
| Motion JPEG | 320×240, 15 fps | Full-range YUV 4:2:2 | Early digital cameras |

These dimensions are **illustrative starter profiles**, not claims about
every historical device. Each codec now has its own remembered controls,
rather than sharing and overwriting one generic panel:

| Codec | Available controls and practical UI ranges |
| --- | --- |
| H.263 | 5–25 fps, 16–256 kbps target, 1–120 frame GOP |
| MPEG-4 Part 2 | 5–30 fps, 16–512 kbps target, 1–180 frame GOP |
| Cinepak | 5–20 fps, 0–5 codebook refinement passes |
| Sorenson Video 1 | 5–30 fps, motion estimation (Zero / EPZS / X1) |
| Microsoft Video 1 | 5–30 fps; fixed RGB555 video |
| Sorenson Spark | 5–30 fps, 32–1000 kbps target, 1–180 frame GOP |
| MPEG-1 | 5/10/12/15/24/25/30 fps only, 150–2000 kbps target, 1–30 frame GOP |
| Motion JPEG | 5–30 fps, actual JPEG quantizer 2–31 (2 = higher quality) |

The values are bounded to the current experimental capture formats, not
the maximum limits of the underlying video standards. Bitrates are shown
in **kbps** rather than raw bits/second; they are still only encoder targets.
Cinepak, SVQ1, Microsoft Video 1, and Motion JPEG instead expose controls
that FFmpeg actually implements. GOP is absent for independent-frame codecs.
The Cinepak, SVQ1 and JPEG options connect directly to their real encoders.
The encoder's output size still depends on scene complexity, and certain
old formats ignore bitrate settings entirely.

Settings from an earlier version with shared H.263 controls are migrated
once to their original codec, not copied into every other mode. Switching
back and forth preserves each codec's settings. MPEG-1 uses no B-frames
in this low-latency implementation; its PAL dimensions and bitrate do
**not** imply fully VCD-compliant output.

The plugin centers a 4:3 crop from widescreen input, compresses at the
codec's capture resolution and displays the decoded result in the original
OBS canvas with appropriate bars. QCIF MPEG-4 explicitly signals a 12:11
pixel aspect ratio for 4:3 display. Alpha is not preserved.

**3GP, AVI, MOV, FLV and VCD are containers/packaging, not codecs.**
These filters process raw video pictures. They do not mux historical
formats, recreate audio, simulate network errors, or emulate optics,
camera sensors or particular consumer hardware.

The gallery is **opt-in** and requires FFmpeg development libraries
libavcodec, libavutil and libswscale. For a local Linux build:

```bash
cmake --preset ubuntu-x86_64 -DENABLE_VIDEO_CODECS=ON -DBUILD_TESTING=ON
cmake --build --preset ubuntu-x86_64 -j4
ctest --test-dir build_x86_64 --output-on-failure
```

The old ENABLE_H263_CODEC=ON flag is still a supported alias. Native
tests encode and decode real bitstreams; Linux OBS tests also require
xvfb-run and a C++ compiler to test actual plugin loading, changing
decoded frames, 4:3 framing, settings migration, UI selection and RPZA
fallback. The default build has no FFmpeg dependency. The new codecs
are not yet packaged for Windows/macOS.

**Unverified:** continuous streaming performance, latency (especially
Cinepak CPU cost), how close output looks to specific hardware captures,
and whether bitrate controls meaningfully influence each encoder.
GPU readback and encoding currently run synchronously on OBS's render
thread. Passing tests do not establish reliable long-running use.

## VHS
Be kind, please rewind. The VHS filter emulates many artifacts seen when playing VHS tapes including tape wrinkle, frame jitter, pop lines, and head switching noise.  It gives the user many tweakable options:
- Frame Jitter: min/max size, min/max period, min/max random interval.
- Tape Wrinkle: Ocurrence probability, wrinkle size, duration.
- Pop Lines: amount
- Head Swithing Noise: Primary noise thickness, amount, Secondary noise thickness, horizontal offset, and amount.

![vhs filter preview](.github/assets/images/vhs.png)

## Bloom
When is too bright not bright enough? When you want the bloom! The bloom filter will blow out those pesky highlights to give that Glam Cam feel to any OBS source.

Glamourous adjustments include bloom intensity, size, and threshold, a application to luminance, individual color channels, or a custom mix of channels.

![bloom filter preview](.github/assets/images/bloom.png)

## Scanlines
Bring your video back to the terminal displays of old with the scanline filter. Adds animated scanlines to your source, with more adjustable parameters than a DEC VT101 Terminal:

- Scanline profile types including sine wave, square, sawtooth, smoothstep, or triangular.
- Scanline period
- Speed
- Intensity

![scanlines filter preview](.github/assets/images/scanlines.png)

## Digital Glitch
D-d-d-d-digital glitch. Channel your innner Max Headroom and experience the glitchiness of the digital world.  The digital glitch filter emulates artifacts seen in corrupted video formats, including random block displacement and color drift.

User changable fields include:
- Block Geometry: maximum displacement, width and height range, interval range.
- Color Drift: maximum drift distance, height range, interval range.

![digital gitch filter preview](.github/assets/images/digital-glitch.png)

## Analog Glitch
This broadcast has been interrupted by analog glitch! Unlike the block digital glitch effect, analog glitch shifts and adds noise to your source in a smooth/contiunous way, much like noisy broadcasts back in the days of analog.

Adjust to your hearts content:
- Maximum random displacement
- Primary Wave speed, scale, and threshold
- Secondary Wave speed, scale, threshold, and influence
- Interference magnitude, line magnitude, and interference alpha
- Color drift maximum distance
- Desaturation Amount

## Conclusion
Grab the retro effects plugin today (as seen on TV) and party like its 1999.