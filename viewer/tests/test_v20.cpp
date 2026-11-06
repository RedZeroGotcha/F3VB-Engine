// v20: testes da leitura direta da demo (Vfs: resource.rht + .grp), do montador de mapas (.map/.rle) e do mixer de som.
// Tudo sintetico, gerado aqui: NENHUM arquivo do jogo (regra CLEANROOM). A conferencia com a demo real e o F3VBCheck.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "audio_mix.h"
#include "f3map.h"
#include "map_build.h"
#include "vfs.h"

using namespace f3v;
namespace fs = std::filesystem;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

using Bytes = std::vector<std::uint8_t>;
static void le(Bytes& b, std::uint32_t v, int n) { for (int i = 0; i < n; ++i) b.push_back(static_cast<std::uint8_t>(v >> (8 * i))); }
static void lef(Bytes& b, float f) { std::uint32_t u; std::memcpy(&u, &f, 4); le(b, u, 4); }
static void s16(Bytes& b, const std::string& s) { le(b, static_cast<std::uint32_t>(s.size()), 2); b.insert(b.end(), s.begin(), s.end()); }
static void write(const fs::path& p, const Bytes& b) { std::ofstream f(p, std::ios::binary); f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size())); }

// ---------------------------------------------------------------- demo falsa: 2 packs, 4 recursos
struct Res { std::string name; std::uint32_t type; int pack; Bytes data; };

static void make_demo(const fs::path& dir, const std::vector<Res>& res, const std::vector<std::string>& packs) {
    fs::create_directories(dir / "data");
    // .grp: cabecalho + tabela + dados, lumps na ordem em que aparecem para cada pack
    std::vector<std::vector<const Res*>> per(packs.size());
    for (const Res& r : res) per[static_cast<std::size_t>(r.pack)].push_back(&r);
    for (std::size_t p = 0; p < packs.size(); ++p) {
        Bytes g;
        le(g, 2, 4); le(g, 1, 4); le(g, static_cast<std::uint32_t>(per[p].size()), 4);
        std::uint32_t off = 12 + 8 * static_cast<std::uint32_t>(per[p].size());
        for (const Res* r : per[p]) { le(g, off, 4); le(g, static_cast<std::uint32_t>(r->data.size()), 4); off += static_cast<std::uint32_t>(r->data.size()); }
        for (const Res* r : per[p]) g.insert(g.end(), r->data.begin(), r->data.end());
        write(dir / "data" / (packs[p] + ".grp"), g);
    }
    // resource.rht
    Bytes packreg, resreg;
    std::vector<std::uint32_t> poff;
    for (const std::string& n : packs) { poff.push_back(static_cast<std::uint32_t>(packreg.size())); packreg.insert(packreg.end(), n.begin(), n.end()); packreg.push_back(0); }
    Bytes rht;
    const std::uint32_t n = static_cast<std::uint32_t>(res.size());
    const std::uint32_t off_packs = 20 + 20 * n, off_res = off_packs + static_cast<std::uint32_t>(packreg.size());
    le(rht, 1, 4); le(rht, 1, 4); le(rht, n, 4); le(rht, off_packs, 4); le(rht, off_res, 4);
    std::vector<std::uint32_t> lump_no(packs.size(), 0);
    for (const Res& r : res) {
        le(rht, 1, 4); le(rht, lump_no[static_cast<std::size_t>(r.pack)]++, 4); le(rht, r.type, 4); le(rht, static_cast<std::uint32_t>(resreg.size()), 4); le(rht, poff[static_cast<std::size_t>(r.pack)], 4);
        resreg.insert(resreg.end(), r.name.begin(), r.name.end()); resreg.push_back(0);
    }
    rht.insert(rht.end(), packreg.begin(), packreg.end());
    rht.insert(rht.end(), resreg.begin(), resreg.end());
    write(dir / "resource.rht", rht);
}

// .map minimo: EMAP (com o rabo de 27 bytes), ECAM, uma entidade EME2 com EEOV, um ponto de entrada EMEP (17 bytes), um gatilho EMTR + ETTR
static void chunk(Bytes& out, const char* tag, std::uint32_t ver, const Bytes& body) {
    out.insert(out.end(), tag, tag + 4); le(out, ver, 4); le(out, static_cast<std::uint32_t>(12 + body.size()), 4); out.insert(out.end(), body.begin(), body.end());
}
static Bytes make_map() {
    Bytes m, b;
    le(b, 0, 4); s16(b, "Teste.8"); s16(b, "Teste.rle"); s16(b, "Teste_AM.dds");
    le(b, 0, 2); b.push_back(83); b.push_back(88); b.push_back(108); b.push_back(0); b.push_back(1); b.push_back(2); b.push_back(3); b.push_back(4);
    b.push_back(0); le(b, 0, 4); lef(b, 250.f); lef(b, 500.f); lef(b, 1.f);
    chunk(m, "EMAP", 5, b);
    b.clear(); lef(b, -10); lef(b, 20); lef(b, 30); lef(b, -5); chunk(m, "ECAM", 0, b);
    Bytes ev;
    s16(ev, "Inst01"); ev.push_back(1); ev.push_back(1); ev.push_back(1);
    for (int i = 0; i < 8; ++i) ev.push_back(0);
    s16(ev, "00 Corporal Teste"); s16(ev, "Teste.amx"); le(ev, 0, 4); ev.push_back(1); lef(ev, 1.f); s16(ev, "Skin_Teste.dds"); ev.push_back(0); ev.push_back(1); ev.push_back(0);
    le(ev, 1, 4); s16(ev, "Ammo9mm.AMO");
    b.clear(); s16(b, "Critters_Teste.CRT"); lef(b, 1); lef(b, 2); lef(b, 3); lef(b, 0.5f); lef(b, 0); lef(b, 0); b.push_back(1);
    chunk(b, "EEOV", 2, ev);
    chunk(m, "EME2", 0, b);
    b.clear(); b.push_back(1); lef(b, 4); lef(b, 0); lef(b, 6); lef(b, 1.5f); chunk(m, "EMEP", 0, b);
    b.clear(); le(b, 6, 4); le(b, 2, 4); lef(b, 0); lef(b, 0); lef(b, 0); lef(b, 1); lef(b, 0); lef(b, 1); chunk(m, "EMTR", 0, b);
    b.clear(); s16(b, "00_04_Tutorial_Vault.map"); b.push_back(1); b.push_back(1); chunk(m, "ETTR", 1, b);
    return m;
}
static Bytes make_rle() {      // 4 x 2 celulas: linha 0 = 2 x 5 + 2 x vazio; linha 1 = 4 x 11
    Bytes r;
    le(r, 4, 4); le(r, 3, 4); le(r, 4, 4); r.push_back(0x2F);
    le(r, 2, 4); r.push_back(5); le(r, 2, 4); r.push_back(0x2F); le(r, 4, 4); r.push_back(11);
    return r;
}
static Bytes make_wav(int rate, int ch, int bits, const std::vector<int>& samples) {
    Bytes d;
    for (int s : samples) { if (bits == 16) le(d, static_cast<std::uint32_t>(static_cast<std::uint16_t>(s)), 2); else d.push_back(static_cast<std::uint8_t>(s)); }
    Bytes w;
    const char* riff = "RIFF"; w.insert(w.end(), riff, riff + 4); le(w, static_cast<std::uint32_t>(36 + d.size()), 4);
    const char* wave = "WAVEfmt "; w.insert(w.end(), wave, wave + 8); le(w, 16, 4);
    le(w, 1, 2); le(w, static_cast<std::uint32_t>(ch), 2); le(w, static_cast<std::uint32_t>(rate), 4); le(w, static_cast<std::uint32_t>(rate * ch * bits / 8), 4); le(w, static_cast<std::uint32_t>(ch * bits / 8), 2); le(w, static_cast<std::uint32_t>(bits), 2);
    const char* data = "data"; w.insert(w.end(), data, data + 4); le(w, static_cast<std::uint32_t>(d.size()), 4);
    w.insert(w.end(), d.begin(), d.end());
    return w;
}

static void test_vfs(const fs::path& tmp) {
    const fs::path demo = tmp / "Demo Falsa Ação";              // espaco e acento no caminho (UTF-8)
    const Bytes tga = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 32, 8, 10, 20, 30, 255};
    std::vector<Res> res = {
        {"f3_logo", RT_TGA, 0, tga},
        {"Mainmenu", RT_INT, 0, Bytes{'G', 'U', 'I', ' ', 'V', '1', '.', '3'}},
        {"Teste", RT_MAP, 1, make_map()},
        {"Teste", RT_RLE, 1, make_rle()},
        {"00 Corporal Teste", RT_SMA, 1, Bytes{1, 2, 3}},
    };
    make_demo(demo, res, {"Interface", "Maps"});
    std::string why;
    CHECK(is_demo_dir(demo.u8string().empty() ? std::string() : std::string(reinterpret_cast<const char*>(demo.u8string().c_str())), why));
    std::string droot(reinterpret_cast<const char*>(demo.u8string().c_str()));
    Vfs v;
    std::string err;
    CHECK(v.open_demo(droot, err));
    if (!err.empty()) std::printf("  vfs: %s\n", err.c_str());
    CHECK(v.size() == 5 && v.packs().size() == 2 && v.demo_mode());
    const ResEntry* e = v.find("F3_LOGO.tga");                 // sem diferenciar maiusculas, com extensao
    CHECK(e && e->type == RT_TGA);
    CHECK(v.find_file("f3_logo.bmp") == e);                    // os dados citam .bmp/.tga/.dds; o recurso e achado pelo nome-base
    CHECK(v.find("Teste", RT_MAP) && v.find("Teste", RT_RLE) && v.find("Teste", RT_MAP) != v.find("Teste", RT_RLE));
    CHECK(v.find("00 Corporal Teste.amx", RT_AMX) != nullptr);   // scripts de dialogo sao SMA_Script mas citados como .amx
    CHECK(v.find("nao_existe") == nullptr);
    Bytes out;
    CHECK(e && v.read(*e, out, err) && out == tga);
    CHECK(v.read("Mainmenu", RT_INT, out) && out.size() == 8 && out[0] == 'G');
    CHECK(e && v.where(*e) == "Interface.grp#0");
    CHECK(v.find_lump("maps", 1) && v.find_lump("maps", 1)->type == RT_RLE);
    // pasta errada: explica o motivo, nunca aceita
    CHECK(!is_demo_dir((tmp / "nao_existe").string(), why) && !why.empty());
    fs::create_directories(tmp / "vazia");
    CHECK(!is_demo_dir((tmp / "vazia").string(), why) && why.find("resource.rht") != std::string::npos);
    Vfs bad;
    CHECK(!bad.open_demo((tmp / "vazia").string(), err) && !err.empty());
    // .grp truncado: os recursos que caem fora ficam de fora, sem travar
    {
        fs::copy(demo, tmp / "trunc", fs::copy_options::recursive);
        fs::resize_file(tmp / "trunc" / "data" / "Maps.grp", 40);
        Vfs t;
        CHECK(t.open_demo((tmp / "trunc").string(), err));
        CHECK(t.size() < 5 && t.find("f3_logo") != nullptr);
    }
    // mapa montado direto da Vfs (sem .8tre: so entidades, grade e gatilhos)
    MapData m;
    MapBuildStats st;
    CHECK(build_map(v, "Teste.map", m, err, &st));
    if (!err.empty() && m.name.empty()) std::printf("  mapa: %s\n", err.c_str());
    CHECK(m.name == "Teste" && m.tre == "Teste.8" && m.rle == "Teste.rle");
    CHECK(m.has_env && m.env_a[0] == 83 && m.env_a[2] == 108 && std::fabs(m.env_f[0] - 250.f) < 1e-3f && std::fabs(m.env_f[1] - 500.f) < 1e-3f);
    CHECK(m.has_camera && m.camera[0] == -10.f && m.camera[3] == -5.f);
    CHECK(m.entities.size() == 1);
    if (!m.entities.empty()) {
        const MapEntity& en = m.entities[0];
        CHECK(en.kind == "CRT" && en.prototype == "Critters_Teste.CRT" && en.instance == "Inst01");
        CHECK(en.name == "00 Corporal Teste" && en.script == "Teste.amx" && en.skin == "Skin_Teste.dds");
        CHECK(en.pos[0] == 1.f && en.pos[2] == 3.f && en.yaw == 0.5f);
    }
    CHECK(m.entries.size() == 1 && !m.entries.empty() && m.entries[0].id == 1 && m.entries[0].pos[0][0] == 4.f);
    CHECK(m.triggers.size() == 1 && !m.triggers.empty() && m.triggers[0].kind == 2 && m.triggers[0].target == "00_04_Tutorial_Vault.map" && m.triggers[0].entry == 1);
    CHECK(m.grid_w == 4 && m.grid_rows == 2 && m.grid.size() == 8 && m.grid[0] == 5 && m.grid[2] == 0x2F && m.grid[7] == 11);
    CHECK(m.blocks.empty() && m.triangle_count() == 0);
    // .map quebrado: erro com mensagem
    MapData m2;
    std::string t2, r2;
    const Bytes broken = {'E', 'M', 'A', 'P', 0, 0, 0, 0, 200, 0, 0, 0};
    CHECK(!parse_map_file(broken.data(), broken.size(), m2, t2, r2, err) && !err.empty());
    const Bytes rle_bad = {4, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0, 0x2F, 9, 0, 0, 0, 1};
    CHECK(!parse_rle(rle_bad.data(), rle_bad.size(), m2, err));
}

static void test_audio() {
    // 22050 Hz mono 16 bits: 3 amostras -> 44100 Hz estereo (reamostragem linear)
    Sound s;
    std::string err;
    const Bytes w = make_wav(22050, 1, 16, {0, 1000, -1000});
    CHECK(decode_wav(w.data(), w.size(), s, err));
    CHECK(s.frames() == 5);
    CHECK(s.pcm.size() == 10 && s.pcm[0] == 0 && s.pcm[2] == 500 && s.pcm[3] == 500 && s.pcm[4] == 1000 && s.pcm[8] == -1000);   // L = R
    // 8 bits estereo 44100 (sem reamostrar)
    const Bytes w8 = make_wav(44100, 2, 8, {128, 255, 0, 128});
    CHECK(decode_wav(w8.data(), w8.size(), s, err) && s.frames() == 2 && s.pcm[0] == 0 && s.pcm[1] > 32000 && s.pcm[2] < -32000);
    // nao suportados
    Bytes adpcm = make_wav(22050, 1, 16, {0, 0});
    adpcm[20] = 2;
    CHECK(!decode_wav(adpcm.data(), adpcm.size(), s, err) && !err.empty());
    CHECK(!decode_wav(w.data(), 10, s, err));
    // mixer: musica = A (3 quadros) + B (2 quadros) e o loop C (2 quadros), sem buraco entre eles
    auto mk = [](std::int16_t v, std::size_t frames) { auto p = std::make_shared<Sound>(); p->pcm.assign(frames * 2, v); return p; };
    Mixer mx;
    mx.set_volumes(1.f, 1.f, 1.f, 1.f);
    mx.set_music({mk(100, 3), mk(200, 2)}, mk(300, 2));
    std::int16_t out[2 * 11];
    mx.mix(out, 11);
    const std::int16_t want[11] = {100, 100, 100, 200, 200, 300, 300, 300, 300, 300, 300};
    bool seq_ok = true;
    for (int i = 0; i < 11; ++i) seq_ok = seq_ok && out[2 * i] == want[i] && out[2 * i + 1] == want[i];
    CHECK(seq_ok);
    // efeito por cima da musica, volume e mudo
    mx.play(mk(1000, 2), 0.5f);
    mx.mix(out, 3);
    CHECK(out[0] == 800 && out[2] == 800 && out[4] == 300);
    CHECK(mx.active_voices() == 1);                               // o efeito acabou; so a musica
    mx.set_muted(true);
    mx.mix(out, 2);
    CHECK(out[0] == 0 && out[3] == 0);
    mx.set_muted(false);
    // ambiente em loop + limitador (soma alta nao estoura)
    mx.stop_music();
    mx.set_ambient(mk(30000, 3));
    for (int i = 0; i < 4; ++i) mx.play(mk(30000, 8), 1.f);
    mx.mix(out, 8);
    bool in_range = true;
    for (int i = 0; i < 16; ++i) in_range = in_range && out[i] > 30000;      // somou ~150000: o limitador segura abaixo do teto int16
    CHECK(in_range);
    mx.stop_all();
    mx.mix(out, 2);
    CHECK(out[0] == 0 && mx.active_voices() == 0);
}

int main() {
    const fs::path tmp = fs::temp_directory_path() / "f3vb_test_v20";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    test_vfs(tmp);
    test_audio();
    fs::remove_all(tmp);
    if (fails == 0) std::puts("v20 tests OK");
    return fails ? 1 : 0;
}
