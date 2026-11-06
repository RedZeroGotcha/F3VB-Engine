#include "audio_mix.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace f3v {

namespace {
std::uint32_t rd32(const std::uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<std::uint32_t>(p[3]) << 24); }
std::uint16_t rd16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
}  // namespace

bool decode_wav(const std::uint8_t* d, std::size_t size, Sound& out, std::string& err) {
    out.pcm.clear();
    if (!d || size < 12 || std::memcmp(d, "RIFF", 4) || std::memcmp(d + 8, "WAVE", 4)) { err = "nao e RIFF/WAVE"; return false; }
    std::uint16_t fmt = 0, ch = 0, bits = 0;
    std::uint32_t rate = 0;
    const std::uint8_t* data = nullptr;
    std::size_t data_len = 0;
    std::size_t p = 12;
    while (p + 8 <= size) {
        const std::uint32_t len = rd32(d + p + 4);
        const std::size_t body = p + 8;
        const std::size_t avail = std::min<std::size_t>(len, size - body);
        if (!std::memcmp(d + p, "fmt ", 4) && avail >= 16) { fmt = rd16(d + body); ch = rd16(d + body + 2); rate = rd32(d + body + 4); bits = rd16(d + body + 14); }
        else if (!std::memcmp(d + p, "data", 4)) { data = d + body; data_len = avail; }
        p = body + len + (len & 1);
    }
    if (fmt != 1) { err = "formato " + std::to_string(fmt) + " (so PCM)"; return false; }
    if ((ch != 1 && ch != 2) || (bits != 8 && bits != 16) || rate < 4000 || rate > 192000) { err = "WAV nao suportado (" + std::to_string(ch) + " canais, " + std::to_string(bits) + " bits, " + std::to_string(rate) + " Hz)"; return false; }
    if (!data) { err = "WAV sem dados"; return false; }
    const std::size_t bps = bits / 8, in_frames = data_len / (bps * ch);
    if (in_frames == 0) { err = "WAV vazio"; return false; }
    auto sample = [&](std::size_t f, int c) -> float {
        const std::uint8_t* s = data + (f * ch + static_cast<std::size_t>(c < ch ? c : 0)) * bps;
        return bits == 16 ? static_cast<float>(static_cast<std::int16_t>(rd16(s))) : static_cast<float>((static_cast<int>(s[0]) - 128) << 8);
    };
    const double step = static_cast<double>(rate) / kMixRate;
    const std::size_t out_frames = static_cast<std::size_t>(std::floor(static_cast<double>(in_frames - 1) / step)) + 1;
    out.pcm.resize(out_frames * 2);
    for (std::size_t i = 0; i < out_frames; ++i) {
        const double x = static_cast<double>(i) * step;
        const std::size_t f0 = static_cast<std::size_t>(x);
        const std::size_t f1 = std::min(f0 + 1, in_frames - 1);
        const float t = static_cast<float>(x - static_cast<double>(f0));
        for (int c = 0; c < 2; ++c) {
            const float v = sample(f0, c) * (1.f - t) + sample(f1, c) * t;
            out.pcm[2 * i + static_cast<std::size_t>(c)] = static_cast<std::int16_t>(std::clamp(v, -32768.f, 32767.f));
        }
    }
    return true;
}

void Mixer::play(std::shared_ptr<const Sound> s, float volume) {
    if (!s || s->pcm.empty()) return;
    std::lock_guard<std::mutex> l(m_);
    if (sfx_.size() >= 24) sfx_.erase(sfx_.begin());          // limite de vozes: descarta a mais antiga
    sfx_.push_back(Voice{std::move(s), 0, volume});
}

void Mixer::set_music(std::vector<std::shared_ptr<const Sound>> intro, std::shared_ptr<const Sound> loop) {
    std::lock_guard<std::mutex> l(m_);
    intro.erase(std::remove_if(intro.begin(), intro.end(), [](const std::shared_ptr<const Sound>& s) { return !s || s->pcm.empty(); }), intro.end());
    music_seq_ = std::move(intro);
    music_loop_ = (loop && !loop->pcm.empty()) ? std::move(loop) : nullptr;
    music_idx_ = music_pos_ = 0;
    music_on_ = !music_seq_.empty() || music_loop_;
}

void Mixer::stop_music() { std::lock_guard<std::mutex> l(m_); music_on_ = false; music_seq_.clear(); music_loop_.reset(); }
void Mixer::set_ambient(std::shared_ptr<const Sound> s) { std::lock_guard<std::mutex> l(m_); amb_ = Voice{std::move(s), 0, 1.f}; amb_on_ = amb_.s && !amb_.s->pcm.empty(); }
void Mixer::stop_ambient() { std::lock_guard<std::mutex> l(m_); amb_on_ = false; amb_.s.reset(); }
void Mixer::stop_all() { std::lock_guard<std::mutex> l(m_); sfx_.clear(); music_on_ = false; music_seq_.clear(); music_loop_.reset(); amb_on_ = false; amb_.s.reset(); }
void Mixer::set_volumes(float master, float music, float sfx, float ambient) {
    std::lock_guard<std::mutex> l(m_);
    master_ = std::clamp(master, 0.f, 1.f); vmusic_ = std::clamp(music, 0.f, 1.f); vsfx_ = std::clamp(sfx, 0.f, 1.f); vamb_ = std::clamp(ambient, 0.f, 1.f);
}
void Mixer::set_muted(bool m) { std::lock_guard<std::mutex> l(m_); muted_ = m; }
bool Mixer::muted() const { std::lock_guard<std::mutex> l(m_); return muted_; }
std::size_t Mixer::active_voices() const { std::lock_guard<std::mutex> l(m_); return sfx_.size() + (music_on_ ? 1 : 0) + (amb_on_ ? 1 : 0); }

void Mixer::mix(std::int16_t* out, std::size_t frames) {
    std::vector<float> acc(frames * 2, 0.f);
    {
        std::lock_guard<std::mutex> l(m_);
        // efeitos
        const float gs = master_ * vsfx_;
        for (Voice& v : sfx_) {
            const std::size_t n = std::min(frames, v.s->frames() - v.pos);
            const std::int16_t* src = v.s->pcm.data() + 2 * v.pos;
            const float g = gs * v.vol;
            for (std::size_t i = 0; i < 2 * n; ++i) acc[i] += src[i] * g;
            v.pos += n;
        }
        sfx_.erase(std::remove_if(sfx_.begin(), sfx_.end(), [](const Voice& v) { return v.pos >= v.s->frames(); }), sfx_.end());
        // musica: segmentos em sequencia, depois o loop, sem buraco entre eles
        if (music_on_) {
            const float g = master_ * vmusic_;
            std::size_t done = 0;
            int guard = 0;
            while (done < frames && music_on_ && guard++ < 16) {
                const Sound* cur = music_idx_ < music_seq_.size() ? music_seq_[music_idx_].get() : music_loop_.get();
                if (!cur) { music_on_ = false; break; }
                const std::size_t n = std::min(frames - done, cur->frames() - music_pos_);
                const std::int16_t* src = cur->pcm.data() + 2 * music_pos_;
                for (std::size_t i = 0; i < 2 * n; ++i) acc[2 * done + i] += src[i] * g;
                done += n;
                music_pos_ += n;
                if (music_pos_ >= cur->frames()) { music_pos_ = 0; if (music_idx_ < music_seq_.size()) ++music_idx_; }
            }
        }
        // ambiente em loop
        if (amb_on_) {
            const float g = master_ * vamb_;
            std::size_t done = 0;
            while (done < frames) {
                const std::size_t n = std::min(frames - done, amb_.s->frames() - amb_.pos);
                const std::int16_t* src = amb_.s->pcm.data() + 2 * amb_.pos;
                for (std::size_t i = 0; i < 2 * n; ++i) acc[2 * done + i] += src[i] * g;
                done += n;
                amb_.pos += n;
                if (amb_.pos >= amb_.s->frames()) amb_.pos = 0;
            }
        }
        if (muted_) std::fill(acc.begin(), acc.end(), 0.f);
    }
    for (std::size_t i = 0; i < frames * 2; ++i) {
        float v = acc[i];
        if (v > 30000.f || v < -30000.f) v = 32767.f * std::tanh(v / 32767.f);     // limitador suave contra estouro quando muitos sons somam
        out[i] = static_cast<std::int16_t>(std::clamp(v, -32768.f, 32767.f));
        const int a = std::abs(static_cast<int>(out[i]));
        if (a > peak_.load(std::memory_order_relaxed)) peak_.store(a, std::memory_order_relaxed);   // v21: medidor de pico (log: o som sai mesmo?)
    }
}

}  // namespace f3v
