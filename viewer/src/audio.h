// v20: som do jogo. Mixer proprio (audio_mix.*) + saida waveOut do winmm (parte do Windows) numa thread propria.
// Os .wav vem da demo pelo carregador (Vfs, recurso WAV_Audio) e ficam em cache depois do primeiro uso.
#pragma once
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "audio_mix.h"
#include "log.h"

namespace f3v {

class Audio {
public:
    using Loader = std::function<bool(const std::string& name, std::vector<std::uint8_t>& out)>;
    ~Audio() { shutdown(); }
    bool init(Loader loader, Log* log);                 // abre a placa de som (44100 Hz estereo)
    void shutdown();
    bool ok() const { return running_; }
    void sfx(const std::string& name, float volume = 1.f);
    void music(const std::vector<std::string>& intro, const std::string& loop);   // ex.: {"MA1_A1", "MA1_A2"}, "MA1_A3"
    void ambient(const std::string& name);
    void stop_music() { mixer_.stop_music(); }
    void stop_ambient() { mixer_.stop_ambient(); }
    void stop_all() { mixer_.stop_all(); }
    void set_volumes(float master, float music, float sfx, float ambient) { mixer_.set_volumes(master, music, sfx, ambient); }
    void set_muted(bool m) { mixer_.set_muted(m); }
    bool muted() const { return mixer_.muted(); }
    std::size_t cached() const { return cache_.size(); }
    std::shared_ptr<const Sound> get(const std::string& name);   // carrega (e guarda) um .wav; nullptr se faltar

private:
    void run();
    Loader loader_;
    Log* log_ = nullptr;
    Mixer mixer_;
public:
    int take_peak() { return mixer_.take_peak(); }   // v21: pico do som que saiu (para o log/autoteste)
private:
    std::map<std::string, std::shared_ptr<const Sound>> cache_;   // nome minusculo -> som (nullptr = falhou, nao tenta de novo)
    std::thread th_;
    std::atomic<bool> running_{false}, stop_{false};
    void* dev_ = nullptr;      // HWAVEOUT
    void* event_ = nullptr;    // HANDLE
};

}  // namespace f3v
