#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include "audio.h"

#include <cstring>

namespace f3v {

namespace {
constexpr int kBuffers = 4;
constexpr int kFrames = 1024;        // ~23 ms por buffer: clique responde rapido, sem falhas
std::string lower(std::string s) { for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a'); return s; }
std::string strip_ext(const std::string& s) { const std::size_t d = s.find_last_of('.'); return (d != std::string::npos && lower(s.substr(d)) == ".wav") ? s.substr(0, d) : s; }
}  // namespace

bool Audio::init(Loader loader, Log* log) {
    shutdown();
    loader_ = std::move(loader);
    log_ = log;
    WAVEFORMATEX wf;
    std::memset(&wf, 0, sizeof wf);
    wf.wFormatTag = WAVE_FORMAT_PCM;
    wf.nChannels = 2;
    wf.nSamplesPerSec = kMixRate;
    wf.wBitsPerSample = 16;
    wf.nBlockAlign = 4;
    wf.nAvgBytesPerSec = kMixRate * 4;
    HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    HWAVEOUT dev = nullptr;
    const MMRESULT r = waveOutOpen(&dev, WAVE_MAPPER, &wf, reinterpret_cast<DWORD_PTR>(ev), 0, CALLBACK_EVENT);
    if (r != MMSYSERR_NOERROR) {
        CloseHandle(ev);
        if (log_) log_->linef("som: waveOutOpen falhou (codigo %u): jogo sem som", static_cast<unsigned>(r));
        return false;
    }
    dev_ = dev;
    event_ = ev;
    stop_ = false;
    running_ = true;
    th_ = std::thread([this]() { run(); });
    if (log_) log_->line("som: mixer proprio 44100 Hz estereo (waveOut, 4 x 23 ms)");
    return true;
}

void Audio::run() {
    HWAVEOUT dev = static_cast<HWAVEOUT>(dev_);
    std::vector<std::int16_t> mem(static_cast<std::size_t>(kBuffers) * kFrames * 2, 0);
    WAVEHDR hdr[kBuffers];
    for (int i = 0; i < kBuffers; ++i) {
        std::memset(&hdr[i], 0, sizeof(WAVEHDR));
        hdr[i].lpData = reinterpret_cast<LPSTR>(mem.data() + static_cast<std::size_t>(i) * kFrames * 2);
        hdr[i].dwBufferLength = kFrames * 4;
        waveOutPrepareHeader(dev, &hdr[i], sizeof(WAVEHDR));
        mixer_.mix(reinterpret_cast<std::int16_t*>(hdr[i].lpData), kFrames);
        waveOutWrite(dev, &hdr[i], sizeof(WAVEHDR));
    }
    while (!stop_) {
        WaitForSingleObject(static_cast<HANDLE>(event_), 50);
        for (int i = 0; i < kBuffers; ++i) {
            if (!(hdr[i].dwFlags & WHDR_DONE) || stop_) continue;
            mixer_.mix(reinterpret_cast<std::int16_t*>(hdr[i].lpData), kFrames);
            waveOutWrite(dev, &hdr[i], sizeof(WAVEHDR));
        }
    }
    waveOutReset(dev);
    for (int i = 0; i < kBuffers; ++i) waveOutUnprepareHeader(dev, &hdr[i], sizeof(WAVEHDR));
}

void Audio::shutdown() {
    if (!running_) return;
    stop_ = true;
    if (event_) SetEvent(static_cast<HANDLE>(event_));
    if (th_.joinable()) th_.join();
    waveOutClose(static_cast<HWAVEOUT>(dev_));
    CloseHandle(static_cast<HANDLE>(event_));
    dev_ = event_ = nullptr;
    running_ = false;
}

std::shared_ptr<const Sound> Audio::get(const std::string& name_in) {
    const std::string name = strip_ext(name_in);
    const std::string key = lower(name);
    const auto it = cache_.find(key);
    if (it != cache_.end()) return it->second;
    std::shared_ptr<const Sound> s;
    std::vector<std::uint8_t> data;
    if (loader_ && loader_(name, data)) {
        auto snd = std::make_shared<Sound>();
        std::string err;
        if (decode_wav(data.data(), data.size(), *snd, err)) s = snd;
        else if (log_) log_->linef("som %s: %s", name.c_str(), err.c_str());
    } else if (log_) log_->linef("som %s: nao existe na demo", name.c_str());
    cache_[key] = s;
    return s;
}

void Audio::sfx(const std::string& name, float volume) { if (running_) if (auto s = get(name)) mixer_.play(s, volume); }

void Audio::music(const std::vector<std::string>& intro, const std::string& loop) {
    if (!running_) return;
    std::vector<std::shared_ptr<const Sound>> seq;
    for (const std::string& n : intro) seq.push_back(get(n));
    mixer_.set_music(std::move(seq), get(loop));
}

void Audio::ambient(const std::string& name) { if (running_) mixer_.set_ambient(get(name)); }

}  // namespace f3v
