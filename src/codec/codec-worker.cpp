// Bounded codec worker adapted from framerot's asynchronous codec pipeline.
// GPL-2.0; see LICENSE. Standard C++17 threading for native host portability.
#include "codec-worker.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

struct codec_worker {
    retro_video_options options;
    size_t row;
    std::vector<uint8_t> pending, active, result, scratch;
    std::mutex lock;
    std::condition_variable wake;
    std::thread thread;
    bool stop=false, queued=false, ready=false;
    std::atomic<bool> fault{false};
    std::atomic<uint64_t> dropped{0};
    struct codec_worker_stats stats{};
    explicit codec_worker(const retro_video_options &o)
        : options(o), row(size_t(o.width)*4),
          pending(row*size_t(o.height)), active(pending.size()),
          result(pending.size()), scratch(pending.size()) {}
};

static void run_worker(codec_worker *w)
{
    using clock=std::chrono::steady_clock;
    retro_video *codec=nullptr;
    auto retry=clock::time_point::min();
    bool recovering=false;
    std::unique_lock<std::mutex> guard(w->lock);
    while(!w->stop){
        w->wake.wait(guard,[&]{return w->stop||w->queued;});
        if(w->stop)break;
        w->pending.swap(w->active);w->queued=false;
        guard.unlock();
        bool attempted=codec||clock::now()>=retry;
        bool restarted=false;
        int rc=0;
        if(attempted){
            if(!codec){codec=retro_video_create(&w->options);restarted=codec&&recovering;}
            rc=codec?retro_video_process(codec,w->active.data(),int(w->row),w->scratch.data(),int(w->row)):-1;
            if(rc<0){
                retro_video_destroy(codec);codec=nullptr;
                retry=clock::now()+std::chrono::milliseconds(250);recovering=true;
            }
        }
        guard.lock();
        if(!attempted){++w->dropped;continue;}
        if(rc<0){++w->stats.errors;w->ready=false;w->fault.store(true);}
        else{
            ++w->stats.completed;
            if(restarted){++w->stats.recovered;recovering=false;}
            if(rc==1){w->result.swap(w->scratch);w->ready=true;w->fault.store(false);}
        }
    }
    guard.unlock();
    retro_video_destroy(codec);
}
codec_worker *codec_worker_create(const retro_video_options *o)
{
    if(!o||!retro_video_supported_size(o->codec,o->width,o->height))return nullptr;
    try{
        auto w=std::make_unique<codec_worker>(*o);
        w->thread=std::thread(run_worker,w.get());
        return w.release();
    }catch(...){return nullptr;}
}
void codec_worker_destroy(codec_worker *w)
{
    if(!w)return;
    {std::lock_guard<std::mutex> guard(w->lock);w->stop=true;}
    w->wake.notify_one();w->thread.join();delete w;
}
bool codec_worker_submit(codec_worker *w,const uint8_t *pixels,int stride)
{
    if(!w||!pixels||stride<int(w->row))return false;
    std::unique_lock<std::mutex> guard(w->lock,std::try_to_lock);
    if(!guard.owns_lock()){++w->dropped;return false;}
    if(w->queued)++w->dropped;
    for(int y=0;y<w->options.height;y++)
        std::memcpy(w->pending.data()+size_t(y)*w->row,pixels+size_t(y)*size_t(stride),w->row);
    w->queued=true;++w->stats.submitted;w->stats.max_pending=1;
    guard.unlock();w->wake.notify_one();return true;
}
bool codec_worker_take(codec_worker *w,uint8_t *pixels)
{
    if(!w||!pixels)return false;
    std::unique_lock<std::mutex> guard(w->lock,std::try_to_lock);
    if(!guard.owns_lock()||!w->ready)return false;
    std::memcpy(pixels,w->result.data(),w->result.size());w->ready=false;return true;
}
bool codec_worker_faulted(codec_worker *w){return !w||w->fault.load();}
struct codec_worker_stats codec_worker_stats(codec_worker *w)
{
    if(!w)return {};
    std::lock_guard<std::mutex> guard(w->lock);
    auto s=w->stats;s.dropped=w->dropped.load();return s;
}
