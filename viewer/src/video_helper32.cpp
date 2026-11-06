// v20: F3VBVideo32.exe — ajudante de 32 bits que toca os videos da demo (Movies\IPlogo.mve, BISlogo.mve).
//
// Por que existe: os ".mve" da demo do Van Buren sao Bink 1 (assinatura "BIKi"), codec proprietario da RAD Game Tools. Nao ha
// especificacao publica completa (faltam as tabelas do codec) e a unica implementacao aberta e LGPL, que a regra do projeto nao
// deixa copiar (CLEANROOM.md). Entao usamos o decodificador que A PROPRIA DEMO traz (binkw32.dll, 32 bits, o mesmo que o F3.exe
// usa), lido da pasta da demo do jogador: nada e distribuido com o F3VB. Como o F3VB e 64 bits e a DLL e 32, este programa
// pequeno (codigo nosso) carrega a DLL e manda os quadros pela saida padrao (pipe) para o F3VB, que desenha em resolucao moderna.
// O som do video e tocado pela propria DLL (waveOut), sincronizado por ela (BinkWait).
//
// Uso: F3VBVideo32.exe <pasta da demo> <arquivo .mve>
// Saida (stdout, binaria): "F3VBVID1" u32 largura, u32 altura, u32 quadros, u32 fps_num, u32 fps_den;
//                          depois, por quadro: u32 numero, largura*altura*4 bytes BGRA (linha de cima primeiro).
// Codigo de saida: 0 ok, 2 argumentos, 3 sem binkw32.dll, 4 funcao ausente, 5 BinkOpen falhou, 6 pipe fechado (F3VB pulou o video).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <cstdarg>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

typedef void* HBINK;
typedef HBINK(__stdcall* PFN_BinkOpen)(const char* name, unsigned flags);
typedef int(__stdcall* PFN_BinkDoFrame)(HBINK);
typedef void(__stdcall* PFN_BinkNextFrame)(HBINK);
typedef int(__stdcall* PFN_BinkWait)(HBINK);
typedef void(__stdcall* PFN_BinkClose)(HBINK);
typedef int(__stdcall* PFN_BinkCopyToBuffer)(HBINK, void* dest, int pitch, unsigned height, unsigned x, unsigned y, unsigned flags);
typedef void*(__stdcall* PFN_BinkOpenWaveOut)(unsigned param);
typedef void*(__stdcall* PFN_BinkOpenDirectSound)(unsigned param);
typedef int(__stdcall* PFN_BinkSetSoundSystem)(void* open_fn, unsigned param);

const unsigned kSurface32 = 3;          // BINKSURFACE32 (BGRA de 32 bits)

bool put(HANDLE out, const void* p, DWORD n) {
    const std::uint8_t* b = static_cast<const std::uint8_t*>(p);
    while (n) {
        DWORD w = 0;
        if (!WriteFile(out, b, n, &w, nullptr) || w == 0) return false;
        b += w; n -= w;
    }
    return true;
}

std::uint32_t rd32(const std::uint8_t* p) { std::uint32_t v; std::memcpy(&v, p, 4); return v; }

}  // namespace

FILE* g_log = nullptr;
void L(const char* fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(g_log, fmt, ap);
    va_end(ap);
    std::fputc('\n', g_log);
    std::fflush(g_log);
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "uso: F3VBVideo32 <pasta da demo> <video.mve>\n"); return 2; }
    const std::string demo = argv[1], file = argv[2];
    // cabecalho do Bink (para o tamanho e o numero de quadros, sem depender da estrutura interna da DLL)
    std::uint8_t hdr[44] = {0};
    {
        FILE* f = std::fopen(file.c_str(), "rb");
        if (!f) { std::fprintf(stderr, "nao abri %s\n", file.c_str()); return 5; }
        const std::size_t n = std::fread(hdr, 1, sizeof hdr, f);
        std::fclose(f);
        if (n < 44 || hdr[0] != 'B' || hdr[1] != 'I' || hdr[2] != 'K') { std::fprintf(stderr, "nao e Bink\n"); return 5; }
    }
    const std::uint32_t frames = rd32(hdr + 8), w = rd32(hdr + 20), h = rd32(hdr + 24), fnum = rd32(hdr + 28), fden = rd32(hdr + 32);
    if (!frames || !w || !h || w > 4096 || h > 4096) { std::fprintf(stderr, "cabecalho invalido\n"); return 5; }

    SetDllDirectoryA(demo.c_str());
    HMODULE dll = LoadLibraryA((demo + "\\binkw32.dll").c_str());
    if (!dll) { std::fprintf(stderr, "binkw32.dll nao achado em %s\n", demo.c_str()); return 3; }
    auto f_open = reinterpret_cast<PFN_BinkOpen>(GetProcAddress(dll, "_BinkOpen@8"));
    auto f_do = reinterpret_cast<PFN_BinkDoFrame>(GetProcAddress(dll, "_BinkDoFrame@4"));
    auto f_next = reinterpret_cast<PFN_BinkNextFrame>(GetProcAddress(dll, "_BinkNextFrame@4"));
    auto f_wait = reinterpret_cast<PFN_BinkWait>(GetProcAddress(dll, "_BinkWait@4"));
    auto f_close = reinterpret_cast<PFN_BinkClose>(GetProcAddress(dll, "_BinkClose@4"));
    auto f_copy = reinterpret_cast<PFN_BinkCopyToBuffer>(GetProcAddress(dll, "_BinkCopyToBuffer@28"));
    auto f_wave = reinterpret_cast<PFN_BinkOpenWaveOut>(GetProcAddress(dll, "_BinkOpenWaveOut@4"));
    auto f_sound = reinterpret_cast<PFN_BinkSetSoundSystem>(GetProcAddress(dll, "_BinkSetSoundSystem@8"));
    auto f_ds = reinterpret_cast<PFN_BinkOpenDirectSound>(GetProcAddress(dll, "_BinkOpenDirectSound@4"));
    if (!f_open || !f_do || !f_next || !f_wait || !f_close || !f_copy) { std::fprintf(stderr, "funcao ausente na binkw32.dll\n"); return 4; }
    // opcoes: --mudo | --waveout | --hwnd <janela do jogo> | --log <arquivo>
    bool mute = false, waveout = false;
    HWND game_wnd = nullptr;
    FILE* lg = nullptr;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--mudo") mute = true;
        else if (a == "--waveout") waveout = true;
        else if (a == "--hwnd" && i + 1 < argc) game_wnd = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(std::strtoull(argv[++i], nullptr, 10)));
        else if (a == "--log" && i + 1 < argc) lg = std::fopen(argv[++i], "ab");
    }
    g_log = lg;
    L("== %s: %ux%u, %u quadros", file.c_str(), w, h, frames);
    // v21: som. O v20 chamava BinkOpenDirectSound(0): o Bink cria o DirectSound preso a uma janela DESTE processo (que nao tem janela).
    // No Windows o DirectSound silencia o som quando essa janela nao esta em primeiro plano, e a janela em primeiro plano e a do F3VB.
    // Por isso saia mudo no Windows (no wine nao). Agora o DirectSound e criado aqui com a janela DO JOGO (--hwnd) e entregue ao Bink.
    // Se falhar, cai para o waveOut do Bink (nao depende de foco).
    if (!mute && f_sound) {
        bool ok = false;
        if (!waveout && f_ds && game_wnd) {
            using PFN_DSC = HRESULT(WINAPI*)(LPCGUID, LPDIRECTSOUND*, LPUNKNOWN);
            HMODULE dsdll = LoadLibraryA("dsound.dll");
            auto dsc = dsdll ? reinterpret_cast<PFN_DSC>(GetProcAddress(dsdll, "DirectSoundCreate")) : nullptr;
            LPDIRECTSOUND ds = nullptr;
            const HRESULT h1 = dsc ? dsc(nullptr, &ds, nullptr) : E_FAIL;
            const HRESULT h2 = SUCCEEDED(h1) && ds ? ds->SetCooperativeLevel(game_wnd, DSSCL_PRIORITY) : E_FAIL;
            L("som: DirectSoundCreate 0x%08lx, SetCooperativeLevel(janela do jogo) 0x%08lx", static_cast<unsigned long>(h1), static_cast<unsigned long>(h2));
            if (SUCCEEDED(h2)) { ok = f_sound(reinterpret_cast<void*>(f_ds), static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(ds))) != 0; L("som: Bink com o DirectSound do jogo: %s", ok ? "ok" : "FALHOU"); }
            else if (ds) ds->Release();
        }
        if (!ok && f_wave) { ok = f_sound(reinterpret_cast<void*>(f_wave), 0) != 0; L("som: Bink com waveOut: %s", ok ? "ok" : "FALHOU"); }
        if (!ok && f_ds) { ok = f_sound(reinterpret_cast<void*>(f_ds), 0) != 0; L("som: Bink com DirectSound proprio: %s", ok ? "ok" : "FALHOU"); }
    } else L("som: %s", mute ? "mudo (pedido)" : "sem BinkSetSoundSystem");
    HBINK b = f_open(file.c_str(), 0);
    if (!b) { std::fprintf(stderr, "BinkOpen falhou\n"); L("BinkOpen falhou"); return 5; }

    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    std::uint32_t head[5] = {w, h, frames, fnum, fden ? fden : 1};
    if (!put(out, "F3VBVID1", 8) || !put(out, head, sizeof head)) { f_close(b); return 6; }
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(w) * h * 4);
    int rc = 0;
    for (std::uint32_t i = 0; i < frames; ++i) {
        while (f_wait(b)) Sleep(1);                     // o Bink espera a hora certa do quadro (e sincroniza o som)
        f_do(b);
        f_copy(b, buf.data(), static_cast<int>(w * 4), h, 0, 0, kSurface32);
        if (!put(out, &i, 4) || !put(out, buf.data(), static_cast<DWORD>(buf.size()))) { rc = 6; break; }
        if (i + 1 < frames) f_next(b);
    }
    f_close(b);
    FreeLibrary(dll);
    return rc;
}
