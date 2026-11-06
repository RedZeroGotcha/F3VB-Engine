// v20: coisas do Windows que o jogo precisa, separadas do game_main: seletor de pasta nativo (o do Explorer), caminhos UTF-8 <-> largos,
// pasta do .exe, linha de comando com acentos/espacos, arquivo de configuracao ao lado do .exe e OpenGL "moderno" no contexto 1.x
// (antisserrilhamento MSAA pelo WGL_ARB_pixel_format, vsync pelo WGL_EXT_swap_control, anisotropico pelo GL_EXT_texture_filter_anisotropic).
// Tudo so com bibliotecas do proprio Windows (user32, gdi32, opengl32, shell32, ole32).
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <map>
#include <string>
#include <vector>

namespace f3v {

std::wstring widen(const std::string& utf8);
std::string narrow(const std::wstring& w);
std::string exe_dir_utf8();                      // pasta do F3VB.exe (UTF-8)
std::vector<std::string> command_line_args();    // argumentos (UTF-8), sem o nome do programa
std::string env_utf8(const char* name);

// Abre o seletor de PASTA do Windows. Devolve "" se a pessoa cancelar. start = pasta inicial (opcional).
std::string pick_folder(HWND owner, const std::wstring& title, const std::string& start);

// Configuracao simples "chave=valor" (UTF-8) num arquivo ao lado do .exe. Linhas com ';' ou '#' sao comentario.
struct KeyValueFile {
    std::map<std::string, std::string> values;
    bool load(const std::string& path);
    bool save(const std::string& path, const std::string& header) const;
    std::string get(const std::string& k, const std::string& def = "") const { const auto it = values.find(k); return it == values.end() ? def : it->second; }
    float getf(const std::string& k, float def) const;
};

// OpenGL: escolhe um pixel format com MSAA (samples = 8, 4, 2...) usando um contexto temporario. Devolve o numero de amostras conseguido
// (0 = sem MSAA: usar ChoosePixelFormat normal). Chamar ANTES de SetPixelFormat na janela do jogo.
int choose_msaa_pixel_format(HINSTANCE inst, HDC target_dc, int want_samples, int& format_out);
void set_vsync(bool on);                         // depois de wglMakeCurrent
float max_anisotropy();                          // 1 se a placa nao tem a extensao; depois de wglMakeCurrent
bool gl_has_extension(const char* name);

}  // namespace f3v
