// v20: toca os videos Bink da demo pelo ajudante F3VBVideo32.exe (video_helper32.cpp): abre o processo com um pipe, uma thread
// le os quadros BGRA e o jogo mostra o mais recente numa textura do OpenGL, escalado para a janela (proporcao mantida).
// O som do video sai pelo DirectSound da propria binkw32.dll da demo (no processo do ajudante).
#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace f3v {

class VideoPlayer {
public:
    ~VideoPlayer() { stop(); }
    // helper: caminho do F3VBVideo32.exe; demo: pasta da demo (tem binkw32.dll); movie: caminho do .mve
    // v21: hwnd = janela do jogo (o som do Bink usa o DirectSound preso a ela); log = arquivo de log do ajudante ("" = sem); waveout = forcar waveOut
    bool start(const std::string& helper, const std::string& demo, const std::string& movie, bool mute, std::string& err,
               void* hwnd = nullptr, const std::string& log = std::string(), bool waveout = false);
    void stop();                                 // para (pular): encerra o ajudante
    bool running() const { return running_; }
    bool finished() const { return finished_; }  // acabou (ou o ajudante saiu)
    int width() const { return w_; }
    int height() const { return h_; }
    std::uint32_t frames() const { return nframes_; }
    std::uint32_t shown() const { return shown_.load(); }   // v21: quadros que chegaram do ajudante (autoteste)
    std::uint32_t current() const { return cur_; }
    // copia o quadro mais novo se mudou desde a ultima chamada (BGRA, linha de cima primeiro)
    bool take_frame(std::vector<std::uint8_t>& out);
    std::string error() const { std::lock_guard<std::mutex> l(m_); return err_; }

    // desenho (OpenGL): sobe o quadro novo e desenha centrado, escalado para caber em w x h pixels
    void draw(int win_w, int win_h);
    void release_gl();

private:
    void reader();
    void* proc_ = nullptr; void* pipe_ = nullptr;
    std::thread th_;
    mutable std::mutex m_;
    std::vector<std::uint8_t> latest_, tmp_;
    bool fresh_ = false;
    std::atomic<bool> running_{false}, finished_{false};
    std::atomic<std::uint32_t> cur_{0};
    int w_ = 0, h_ = 0;
    std::uint32_t nframes_ = 0;
    std::string err_;
    unsigned tex_ = 0; int tex_w_ = 0, tex_h_ = 0;
    std::atomic<std::uint32_t> shown_{0};
};

}  // namespace f3v
