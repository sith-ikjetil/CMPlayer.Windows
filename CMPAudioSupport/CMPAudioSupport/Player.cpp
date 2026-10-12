#include "pch.h"
#include "Support.hpp"
#include <mfmediaengine.h>
#include <shlwapi.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace cwa {
using namespace std::chrono_literals;
/* Callback never calls the engine, Swift, or player worker. A shared event inbox
   outlives both the worker state and any late Media Foundation callback. */
struct Events {
    std::mutex mutex;
    std::condition_variable changed;
    bool ready = false;
    bool ended = false;
    HRESULT error = S_OK;
};
class Notify final : public IMFMediaEngineNotify {
    std::atomic<ULONG> references{1};
    std::shared_ptr<Events> events;
public:
    explicit Notify(std::shared_ptr<Events> e) : events(std::move(e)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) override {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (id == __uuidof(IUnknown) || id == __uuidof(IMFMediaEngineNotify)) {
            *out = static_cast<IMFMediaEngineNotify *>(this); AddRef(); return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG n = --references; if (!n) delete this; return n;
    }
    HRESULT STDMETHODCALLTYPE EventNotify(DWORD event, DWORD_PTR, DWORD detail) override {
        try {
            {
                std::lock_guard lock(events->mutex);
                if (event == MF_MEDIA_ENGINE_EVENT_CANPLAY) events->ready = true;
                if (event == MF_MEDIA_ENGINE_EVENT_ENDED) events->ended = true;
                if (event == MF_MEDIA_ENGINE_EVENT_ERROR)
                    events->error = FAILED(static_cast<HRESULT>(detail)) ? static_cast<HRESULT>(detail) : E_FAIL;
            }
            events->changed.notify_all();
            return S_OK;
        } catch (...) { return E_FAIL; }
    }
};
struct State {
    std::wstring path;
    ComPtr<IMFMediaEngine> engine;
    std::shared_ptr<Events> events;
    cwa_snapshot snapshot{};
    double base = 1.0, target = 1.0;
    uint64_t fade_ms = 0;
    explicit State(std::wstring p) : path(std::move(p)) { snapshot.volume = 1.0f; }
    ~State() { close(); }
    void close() noexcept {
        if (engine) engine->Shutdown();
        engine.Reset(); events.reset();
    }
    void open() {
        close();
        events = std::make_shared<Events>();
        ComPtr<IMFMediaEngineNotify> notify;
        notify.Attach(new Notify(events));
        ComPtr<IMFAttributes> attributes;
        check(MFCreateAttributes(&attributes, 1), "MFCreateAttributes");
        check(attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, notify.Get()), "Set Media Engine callback");
        ComPtr<IMFMediaEngineClassFactory> factory;
        check(CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER,
                               IID_PPV_ARGS(&factory)), "Create Media Engine factory");
        check(factory->CreateInstance(MF_MEDIA_ENGINE_AUDIOONLY, attributes.Get(), &engine), "Create audio Media Engine");
        check(engine->SetAutoPlay(FALSE), "Disable autoplay");
        check(engine->SetLoop(FALSE), "Disable loop");
        // URL conversion quotes spaces, #, %, Unicode, and UNC paths correctly.
        DWORD count = static_cast<DWORD>(path.size()*3 + 128);
        std::vector<wchar_t> url(count);
        auto h = UrlCreateFromPathW(path.c_str(), url.data(), &count, 0);
        if (h == E_POINTER) {
            url.resize(count); h = UrlCreateFromPathW(path.c_str(), url.data(), &count, 0);
        }
        check(h, "Convert filesystem path to file URL");
        BSTR source = SysAllocString(url.data());
        if (!source) throw std::bad_alloc();
        h = engine->SetSource(source);
        SysFreeString(source);
        check(h, "Set Media Engine source");
        // SetSource already initiates loading; calling Load again would restart it.
        {
            std::unique_lock lock(events->mutex);
            if (!events->changed.wait_for(lock, 15s, [&] { return events->ready || FAILED(events->error); }))
                throw Failure(HRESULT_FROM_WIN32(ERROR_TIMEOUT), "Media open timed out after 15 seconds");
            check(events->error, "Media Engine could not load audio");
        }
        require(engine->HasAudio() != FALSE, "File has no playable audio stream");
        snapshot = {};
        snapshot.duration_ms = milliseconds(engine->GetDuration());
        require(snapshot.duration_ms > 0, "Media duration is unknown or zero");
        snapshot.state = CWA_READY;
        check(engine->SetVolume(base), "Set volume");
        snapshot.volume = static_cast<float>(base);
    }
    void tick() {
        if (!engine || snapshot.state == CWA_FAILED) return;
        HRESULT failure; bool ended;
        {
            std::lock_guard lock(events->mutex);
            failure = events->error; ended = events->ended;
            events->ended = false;
        }
        if (FAILED(failure)) {
            snapshot.playback_error = static_cast<int32_t>(failure);
            snapshot.has_played = snapshot.state == CWA_PLAYING || snapshot.state == CWA_PAUSED;
            snapshot.state = CWA_FAILED;
            engine->Pause();
            return;
        }
        if (ended && (snapshot.state == CWA_PLAYING || snapshot.state == CWA_PAUSED)) {
            snapshot.state = CWA_ENDED; snapshot.has_played = 1;
            snapshot.position_ms = snapshot.duration_ms;
        }
        if (snapshot.state == CWA_READY || snapshot.state == CWA_PLAYING || snapshot.state == CWA_PAUSED)
            snapshot.position_ms = (std::min)(milliseconds(engine->GetCurrentTime()), snapshot.duration_ms);
        if (!engine->IsSeeking()) {
            const double v = fade_volume(base, target, fade_ms, snapshot.duration_ms, snapshot.position_ms);
            if (std::abs(v - snapshot.volume) > 0.00001) {
                check(engine->SetVolume(v), "Update fade volume");
                snapshot.volume = static_cast<float>(v);
            }
        }
    }
    void play() {
        tick();
        if (snapshot.state == CWA_PLAYING) return;
        try {
            if (snapshot.state == CWA_STOPPED || snapshot.state == CWA_ENDED || snapshot.state == CWA_FAILED) open();
            check(engine->Play(), "Start playback");
            snapshot.state = CWA_PLAYING; snapshot.has_played = 0;
        } catch (const Failure &e) {
            snapshot.state = CWA_FAILED;
            snapshot.playback_error = static_cast<int32_t>(e.code);
            close();
            throw;
        }
    }
    void stop() {
        if (snapshot.state == CWA_PLAYING || snapshot.state == CWA_PAUSED) {
            check(engine->Pause(), "Stop playback");
            // Preserve original Swift terminal flags/timeElapsed on explicit stop.
            snapshot.state = CWA_STOPPED; snapshot.has_played = 1;
            snapshot.position_ms = snapshot.duration_ms;
            close();
        }
    }
    void pause() {
        tick();
        if (snapshot.state == CWA_PLAYING) {
            check(engine->Pause(), "Pause playback"); snapshot.state = CWA_PAUSED;
        }
    }
    void resume() {
        tick();
        if (snapshot.state == CWA_PAUSED) {
            check(engine->Play(), "Resume playback"); snapshot.state = CWA_PLAYING;
        }
    }
    void seek(uint64_t ms) {
        tick();
        require(ms <= snapshot.duration_ms, "Seek position exceeds track duration");
        require(engine && (snapshot.state == CWA_READY || snapshot.state == CWA_PLAYING || snapshot.state == CWA_PAUSED),
                "Seek requires loaded, nonterminal media");
        check(engine->SetCurrentTime(static_cast<double>(ms)/1000.0), "Seek from start");
    }
};
/* One DLL-owned MTA thread per player. Both commands and fade ticks execute
   there, so no engine pointer crosses apartment or client-thread boundaries. */
class Worker {
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<std::function<void(State &)>> queue;
    bool quitting = false;
    std::thread thread;
public:
    explicit Worker(std::wstring path) {
        std::promise<void> ready;
        auto started = ready.get_future();
        thread = std::thread([this, path=std::move(path), ready=std::move(ready)]() mutable {
            bool announced = false;
            try {
                Runtime runtime;
                State state(std::move(path));
                state.open();
                ready.set_value();
                announced = true;
                for (;;) {
                    std::function<void(State &)> command;
                    {
                        std::unique_lock lock(mutex);
                        changed.wait_for(lock, 20ms, [&] { return quitting || !queue.empty(); });
                        if (quitting && queue.empty()) break;
                        if (!queue.empty()) { command = std::move(queue.front()); queue.pop_front(); }
                    }
                    if (command) command(state);
                    try { state.tick(); }
                    catch (const Failure &e) {
                        state.snapshot.playback_error = static_cast<int32_t>(e.code);
                        state.snapshot.has_played = state.snapshot.state == CWA_PLAYING || state.snapshot.state == CWA_PAUSED;
                        state.snapshot.state = CWA_FAILED; state.close();
                    }
                    catch (...) {
                        state.snapshot.playback_error = static_cast<int32_t>(E_FAIL);
                        state.snapshot.state = CWA_FAILED; state.close();
                    }
                }
            } catch (...) {
                // Startup errors are delivered to create. After startup the loop
                // contains operation/tick failures; an executor infrastructure
                // failure is unrecoverable (avoid a second promise completion).
                if (!announced) ready.set_exception(std::current_exception());
                else std::terminate();
            }
        });
        try { started.get(); }
        catch (...) { thread.join(); throw; }
    }
    ~Worker() {
        { std::lock_guard lock(mutex); quitting = true; }
        changed.notify_all(); thread.join();
    }
    Worker(const Worker &) = delete;
    template<class F> void call(F &&f) {
        auto task = std::make_shared<std::packaged_task<void(State &)>>(std::forward<F>(f));
        auto done = task->get_future();
        { std::lock_guard lock(mutex); queue.emplace_back([task](State &s) { (*task)(s); }); }
        changed.notify_one(); done.get();
    }
};
}
struct cwa_player { cwa::Worker worker; explicit cwa_player(std::wstring p) : worker(std::move(p)) {} };
using namespace cwa;
extern "C" {
uint32_t CWA_CALL cwa_abi_version(void) { return 1; }
int32_t CWA_CALL cwa_create(const char *path, cwa_player **out, cwa_error *error) {
    if (out) *out = nullptr;
    return boundary(error, [&] { require(out, "Output handle is NULL"); *out = new cwa_player(path_from_utf8(path)); });
}
void CWA_CALL cwa_destroy(cwa_player *p) { delete p; }
int32_t CWA_CALL cwa_play(cwa_player *p, cwa_error *e) {
    return boundary(e, [&] { require(p, "Player is NULL"); p->worker.call([](State &s) { s.play(); }); });
}
int32_t CWA_CALL cwa_stop(cwa_player *p, cwa_error *e) {
    return boundary(e, [&] { require(p, "Player is NULL"); p->worker.call([](State &s) { s.stop(); }); });
}
int32_t CWA_CALL cwa_pause(cwa_player *p, cwa_error *e) {
    return boundary(e, [&] { require(p, "Player is NULL"); p->worker.call([](State &s) { s.pause(); }); });
}
int32_t CWA_CALL cwa_resume(cwa_player *p, cwa_error *e) {
    return boundary(e, [&] { require(p, "Player is NULL"); p->worker.call([](State &s) { s.resume(); }); });
}
int32_t CWA_CALL cwa_seek(cwa_player *p, uint64_t ms, cwa_error *e) {
    return boundary(e, [&] { require(p, "Player is NULL"); p->worker.call([ms](State &s) { s.seek(ms); }); });
}
int32_t CWA_CALL cwa_set_volume(cwa_player *p, float v, cwa_error *e) {
    return boundary(e, [&] {
        require(p && std::isfinite(v) && v >= 0 && v <= 1, "Invalid player or volume");
        p->worker.call([v](State &s) { s.base = v; s.tick(); });
    });
}
int32_t CWA_CALL cwa_set_end_fade(cwa_player *p, float v, uint64_t ms, cwa_error *e) {
    return boundary(e, [&] {
        require(p && std::isfinite(v) && v >= 0 && v <= 1, "Invalid player or target volume");
        p->worker.call([v, ms](State &s) { s.target = v; s.fade_ms = ms; s.tick(); });
    });
}
int32_t CWA_CALL cwa_get_snapshot(cwa_player *p, cwa_snapshot *out, cwa_error *e) {
    if (out) *out = {};
    return boundary(e, [&] { require(p && out, "Player or output is NULL");
        p->worker.call([out](State &s) { s.tick(); *out = s.snapshot; }); });
}
}
