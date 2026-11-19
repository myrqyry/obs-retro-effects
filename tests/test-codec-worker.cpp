#undef NDEBUG
#include "../src/codec/codec-worker.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

int main()
{
    auto opts=retro_video_default_options(RETRO_VIDEO_MPEG4_PART2);
    opts.width=64;opts.height=64;opts.fps=30;opts.gop_size=8;opts.bitrate=120000;
    auto *w=codec_worker_create(&opts);assert(w);
    std::vector<uint8_t> input(64*64*4),output(input.size());
    for(size_t i=0;i<input.size();i+=4){
        input[i]=180;input[i+1]=40;input[i+2]=90;input[i+3]=255;
    }
    for(int i=0;i<1000;i++)codec_worker_submit(w,input.data(),64*4);
    bool got=false;
    for(int i=0;i<3000&&!got;i++){
        got=codec_worker_take(w,output.data());
        if(!got)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(got && output[0]>120 && output[1]<100 && output[3]==255);
    auto stats=codec_worker_stats(w);
    assert(stats.submitted>0 && stats.completed>0 && stats.dropped>0 && stats.max_pending==1);
    printf("BOUNDED_WORKER submitted=%llu completed=%llu dropped=%llu\n",
           (unsigned long long)stats.submitted,(unsigned long long)stats.completed,(unsigned long long)stats.dropped);
    codec_worker_destroy(w);
    opts.bitrate=0;
    w=codec_worker_create(&opts);assert(w);
    auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(650);
    while(std::chrono::steady_clock::now()<end){
        codec_worker_submit(w,input.data(),64*4);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stats=codec_worker_stats(w);
    assert(stats.errors>0 && stats.errors<=4 && codec_worker_faulted(w) && !codec_worker_take(w,output.data()));
    codec_worker_destroy(w);
    puts("WORKER_FAULT_BOUNDED");
}
