// v20: mixer de audio proprio (portavel; a saida para a placa de som fica em audio.cpp, com winmm).
// Saida fixa: 44100 Hz, estereo, 16 bits. Entradas: WAV PCM 8/16 bits, mono/estereo, qualquer taxa (reamostragem linear).
// Canais logicos: efeitos (varios ao mesmo tempo), musica (segmentos em sequencia + segmento em loop, como a peca MA1_PS2.pce
// da demo: start MA1_A1, MA1_A2, loop MA1_A3) e ambiente (um som em loop, ex. AMB_WindBlowing).
#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace f3v {

constexpr int kMixRate = 44100;

struct Sound {                       // ja convertido para 44100 Hz estereo int16
    std::vector<std::int16_t> pcm;   // intercalado L, R
    std::size_t frames() const { return pcm.size() / 2; }
};

// Decodifica um .wav (RIFF/WAVE PCM) e converte para o formato do mixer. false + err se nao suportado.
bool decode_wav(const std::uint8_t* data, std::size_t size, Sound& out, std::string& err);

class Mixer {
public:
    void play(std::shared_ptr<const Sound> s, float volume = 1.f);       // efeito (one-shot)
    void set_music(std::vector<std::shared_ptr<const Sound>> intro, std::shared_ptr<const Sound> loop);
    void stop_music();
    void set_ambient(std::shared_ptr<const Sound> s);
    void stop_ambient();
    void stop_all();
    void set_volumes(float master, float music, float sfx, float ambient);
    void set_muted(bool m);
    bool muted() const;
    void mix(std::int16_t* out, std::size_t frames);                     // chamado pela thread de audio
    // v21: maior amostra (0..32767) desde a ultima leitura; zera ao ler. 0 = nada tocou nesse intervalo.
    int take_peak() { return peak_.exchange(0); }
    std::size_t active_voices() const;

private:
    struct Voice { std::shared_ptr<const Sound> s; std::size_t pos = 0; float vol = 1.f; };
    mutable std::mutex m_;
    std::vector<Voice> sfx_;
    std::vector<std::shared_ptr<const Sound>> music_seq_;
    std::shared_ptr<const Sound> music_loop_;
    std::size_t music_idx_ = 0, music_pos_ = 0;
    bool music_on_ = false;
    Voice amb_;
    bool amb_on_ = false;
    float master_ = 1.f, vmusic_ = 0.8f, vsfx_ = 1.f, vamb_ = 0.7f;
    std::atomic<int> peak_{0};
    bool muted_ = false;
};

}  // namespace f3v
