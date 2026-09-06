#include "UxSoundM5.h"
#include <assert.h>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <algorithm>
using namespace ux::sound;

struct OwnedSource : Source {
    std::thread::id owner = std::this_thread::get_id();
    size_t position = 0, total = 6 * 2048;
    void check() const { assert(std::this_thread::get_id() == owner); }
    size_t render(int16_t* p, size_t n) override {
        check(); size_t count=std::min(n,total-position);
        for(size_t i=0;i<count;++i)p[i]=static_cast<int16_t>((position+i)%20001-10000);
        position+=count;for(size_t i=count;i<n;++i)p[i]=0;return count;
    }
    bool active() const override {check();return position<total;}
    void cancel() override {check();total=position;}
};

// Two phases that a simple length-two queue misses: publishing leaves flip
// occupied; only consumer adoption makes the next producer slot writable.
struct PublishedSpeaker {
    struct Buffer {const int16_t* data=nullptr;size_t length=0;std::vector<int16_t> saved;};
    mutable std::mutex mutex;std::condition_variable changed;
    Buffer playing,published;
    bool blocked=false;
    std::vector<int16_t> consumed;
    std::thread::id ui=std::this_thread::get_id();
    bool begin(){return true;}bool isRunning(){return true;}bool isEnabled(){return true;}
    size_t isPlaying(uint8_t) const {std::lock_guard<std::mutex> lock(mutex);return !!playing.data+!!published.data;}
    bool playRaw(const int16_t* data,size_t n,uint32_t rate,bool stereo,int repeat,uint8_t channel,bool stop) {
        assert(std::this_thread::get_id()!=ui);assert(rate==16000&&!stereo&&repeat==1&&channel==6&&!stop);
        std::unique_lock<std::mutex> lock(mutex);
        if(published.data){blocked=true;changed.notify_all();changed.wait(lock,[&]{return !published.data;});blocked=false;}
        assert(playing.data!=data);published.data=data;published.length=n;published.saved.assign(data,data+n);return true;
    }
    void waitUntilBlocked() {
        std::unique_lock<std::mutex> lock(mutex);
        assert(changed.wait_for(lock,std::chrono::seconds(2),[&]{return blocked;}));
    }
    void adopt() {
        std::lock_guard<std::mutex> lock(mutex);
        if(!playing.data&&published.data){playing=published;published={};changed.notify_all();}
    }
    void validate() {
        std::lock_guard<std::mutex> lock(mutex);
        for(Buffer* b:{&playing,&published})if(b->data)assert(std::equal(b->saved.begin(),b->saved.end(),b->data));
    }
    void consumePlaying() {
        std::lock_guard<std::mutex> lock(mutex);
        if(!playing.data)return;
        assert(std::equal(playing.saved.begin(),playing.saved.end(),playing.data));
        consumed.insert(consumed.end(),playing.data,playing.data+playing.length);playing={};
    }
};

int main() {
    OwnedSource source;PublishedSpeaker speaker;M5Output<PublishedSpeaker> output;
    assert(output.begin(speaker,source));output.update();assert(source.position==4096);
    std::thread sender([&]{output.transportStep();});
    speaker.waitUntilBlocked();assert(speaker.isPlaying(6)==1);
    // Consumer is deliberately paused with one published request, while the
    // SDK sender waits. UI synthesis/update must remain independent and bounded.
    const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<10000;++i)output.update();
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    assert(elapsed<100);assert(source.position==4096);speaker.validate();
    speaker.adopt();sender.join();assert(speaker.isPlaying(6)==2);
    while(output.busy()) {
        speaker.consumePlaying();speaker.adopt();
        // Schedule transport independently, including completion retirement.
        std::thread retire([&]{output.transportStep();});retire.join();
        output.update();
        std::thread submit([&]{output.transportStep();});submit.join();
        speaker.validate();
    }
    assert(speaker.consumed.size()==source.total);
    for(size_t i=0;i<speaker.consumed.size();++i)assert(speaker.consumed[i]==static_cast<int16_t>(i%20001-10000));
    assert(output.failures()==0);
    // Run actual concurrent producer/sender/consumer ownership transitions.
    OwnedSource stressSource;stressSource.total=200*2048;
    PublishedSpeaker stressSpeaker;M5Output<PublishedSpeaker> stressOutput;
    assert(stressOutput.begin(stressSpeaker,stressSource));std::atomic<bool> done{false};
    std::thread concurrentSender([&]{while(!done.load()){stressOutput.transportStep();std::this_thread::yield();}});
    std::thread consumer([&]{while(!done.load()){stressSpeaker.adopt();stressSpeaker.consumePlaying();std::this_thread::yield();}});
    while(stressOutput.busy()){stressOutput.update();std::this_thread::yield();}
    done.store(true);concurrentSender.join();consumer.join();
    assert(stressSpeaker.consumed.size()==stressSource.total);
    for(size_t i=0;i<stressSpeaker.consumed.size();++i)assert(stressSpeaker.consumed[i]==static_cast<int16_t>(i%20001-10000));
    std::cout<<"published/adoption stall isolated; 10000 UI updates in "<<elapsed<<"ms; Source owner, FIFO bytes, three-buffer lifetime and two-ahead cap passed\n";
}
