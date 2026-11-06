// v20: tocador dos videos Bink (ver video_player.h).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>

#include "video_player.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#include "win_platform.h"

#ifndef GL_BGRA_EXT
#define GL_BGRA_EXT 0x80E1
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace f3v {

namespace {
bool read_all(HANDLE h, void* p, DWORD n) {
    std::uint8_t* b = static_cast<std::uint8_t*>(p);
    while (n) {
        DWORD r = 0;
        if (!ReadFile(h, b, n, &r, nullptr) || r == 0) return false;
        b += r; n -= r;
    }
    return true;
}
int pow2(int v) { int p = 1; while (p < v) p <<= 1; return p; }
}  // namespace

bool VideoPlayer::start(const std::string& helper, const std::string& demo, const std::string& movie, bool mute, std::string& err, void* hwnd, const std::string& log, bool waveout) {
    stop();
    finished_ = false;
    fresh_ = false;
    w_ = h_ = 0; nframes_ = 0; cur_ = 0; shown_ = 0;
    SECURITY_ATTRIBUTES sa;
    std::memset(&sa, 0, sizeof sa);
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 4 << 20)) { err = "CreatePipe falhou"; return false; }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si;
    std::memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi;
    std::memset(&pi, 0, sizeof pi);
    std::wstring cmd = L"\"" + widen(helper) + L"\" \"" + widen(demo) + L"\" \"" + widen(movie) + L"\"" + (mute ? L" --mudo" : L"");
    if (hwnd) cmd += L" --hwnd " + std::to_wstring(reinterpret_cast<std::uintptr_t>(hwnd));
    if (waveout) cmd += L" --waveout";
    if (!log.empty()) cmd += L" --log \"" + widen(log) + L"\"";
    std::vector<wchar_t> cbuf(cmd.begin(), cmd.end());
    cbuf.push_back(0);
    const BOOL ok = CreateProcessW(nullptr, cbuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); err = "nao consegui abrir " + helper; return false; }
    CloseHandle(pi.hThread);
    proc_ = pi.hProcess;
    pipe_ = rd;
    // cabecalho (bloqueia pouco: o ajudante escreve logo depois de abrir o video)
    char magic[8];
    std::uint32_t head[5];
    if (!read_all(rd, magic, 8) || std::memcmp(magic, "F3VBVID1", 8) != 0 || !read_all(rd, head, sizeof head)) {
        DWORD code = 0;
        WaitForSingleObject(pi.hProcess, 2000);
        GetExitCodeProcess(pi.hProcess, &code);
        err = "o ajudante de video nao abriu " + movie + " (codigo " + std::to_string(code) + (code == 3 ? ": binkw32.dll nao esta na pasta da demo" : code == 5 ? ": BinkOpen falhou" : "") + ")";
        stop();
        return false;
    }
    w_ = static_cast<int>(head[0]); h_ = static_cast<int>(head[1]); nframes_ = head[2];
    latest_.assign(static_cast<std::size_t>(w_) * static_cast<std::size_t>(h_) * 4, 0);
    tmp_ = latest_;
    running_ = true;
    th_ = std::thread([this] { reader(); });
    return true;
}

void VideoPlayer::reader() {
    HANDLE rd = static_cast<HANDLE>(pipe_);
    std::uint32_t idx = 0;
    while (running_) {
        if (!read_all(rd, &idx, 4) || !read_all(rd, tmp_.data(), static_cast<DWORD>(tmp_.size()))) break;
        {
            std::lock_guard<std::mutex> l(m_);
            latest_.swap(tmp_);
            fresh_ = true;
        }
        ++shown_;                                   // v21: quadros que chegaram do ajudante
        cur_ = idx;
        if (nframes_ && idx + 1 >= nframes_) break;
    }
    finished_ = true;
}

void VideoPlayer::stop() {
    running_ = false;
    if (proc_) { TerminateProcess(static_cast<HANDLE>(proc_), 0); }
    if (pipe_) { CloseHandle(static_cast<HANDLE>(pipe_)); pipe_ = nullptr; }      // desbloqueia o ReadFile da thread
    if (th_.joinable()) th_.join();
    if (proc_) { WaitForSingleObject(static_cast<HANDLE>(proc_), 2000); CloseHandle(static_cast<HANDLE>(proc_)); proc_ = nullptr; }
    finished_ = true;
}

bool VideoPlayer::take_frame(std::vector<std::uint8_t>& out) {
    std::lock_guard<std::mutex> l(m_);
    if (!fresh_) return false;
    out = latest_;
    fresh_ = false;
    return true;
}

void VideoPlayer::draw(int win_w, int win_h) {
    if (w_ <= 0 || h_ <= 0) return;
    if (!tex_) {
        tex_w_ = pow2(w_); tex_h_ = pow2(h_);
        GLuint id = 0;
        glGenTextures(1, &id);
        tex_ = id;
        glBindTexture(GL_TEXTURE_2D, tex_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        std::vector<std::uint8_t> zero(static_cast<std::size_t>(tex_w_) * static_cast<std::size_t>(tex_h_) * 4, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w_, tex_h_, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, zero.data());
    }
    glBindTexture(GL_TEXTURE_2D, tex_);
    {
        std::lock_guard<std::mutex> l(m_);
        if (fresh_) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w_, h_, GL_BGRA_EXT, GL_UNSIGNED_BYTE, latest_.data());
            fresh_ = false;
        }
    }
    // tela inteira, proporcao do video mantida (faixas pretas)
    const float sx = static_cast<float>(win_w) / static_cast<float>(w_), sy = static_cast<float>(win_h) / static_cast<float>(h_);
    const float s = std::min(sx, sy);
    const float dw = static_cast<float>(w_) * s, dh = static_cast<float>(h_) * s;
    const float x0 = (static_cast<float>(win_w) - dw) * 0.5f, y0 = (static_cast<float>(win_h) - dh) * 0.5f;
    const float u1 = static_cast<float>(w_) / static_cast<float>(tex_w_), v1 = static_cast<float>(h_) / static_cast<float>(tex_h_);
    glViewport(0, 0, win_w, win_h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, win_w, win_h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_BLEND); glDisable(GL_FOG); glDisable(GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(1, 1, 1, 1);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex2f(x0, y0);
    glTexCoord2f(u1, 0); glVertex2f(x0 + dw, y0);
    glTexCoord2f(u1, v1); glVertex2f(x0 + dw, y0 + dh);
    glTexCoord2f(0, v1); glVertex2f(x0, y0 + dh);
    glEnd();
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDisable(GL_TEXTURE_2D);
}

void VideoPlayer::release_gl() {
    if (tex_) { GLuint id = tex_; glDeleteTextures(1, &id); tex_ = 0; }
}

}  // namespace f3v
