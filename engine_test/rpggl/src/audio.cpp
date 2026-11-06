// =====================================================================
//  audio.cpp - efeitos sonoros procedurais + mixer de software (waveOut)
//
//  * Nenhum arquivo de audio: tudo e SINTETIZADO na inicializacao
//    (tiros, passos, golpes, interface, vento, Geiger...).
//  * Thread propria alimenta o waveOut (22,05 kHz, mono, 16 bits,
//    4 buffers de 512 amostras) e soma ate 24 vozes + vento em loop.
//  * Se o waveOut nao abrir (sem dispositivo), o jogo segue mudo.
// =====================================================================
#include "gfx.h"

static const int SR = 22050;
static const int NBUF = 4, BUFN = 512, NVOICE = 24;

struct Sample { short* d; int n; };
static Sample g_s[SND_COUNT];
static Sample g_wind;
struct SS { float a, b, c; };                               // estado de sintese entre amostras
static Rng g_nr(4242u);
static float Noise() { return g_nr.r(-1.0f, 1.0f); }

static float fPistol(float t, SS& s)  { s.a += 2*PI*(220.0f*expf(-t*40.0f) + 70.0f)/SR; s.b += (Noise() - s.b)*0.55f; return (sinf(s.a)*expf(-t*30.0f)*0.7f + s.b*expf(-t*34.0f)*1.2f)*0.85f; }
static float fShotgun(float t, SS& s) { s.a += 2*PI*(150.0f*expf(-t*22.0f) + 45.0f)/SR; s.b += (Noise() - s.b)*0.35f; return (sinf(s.a)*expf(-t*14.0f)*0.9f + s.b*expf(-t*13.0f)*1.3f)*0.95f; }
static float fRifle(float t, SS& s)   { s.a += 2*PI*(260.0f*expf(-t*35.0f) + 60.0f)/SR; s.b += (Noise() - s.b)*0.7f; float tail = Noise()*0.15f*expf(-(t - 0.08f)*9.0f)*(t > 0.08f ? 1.0f : 0.0f); return (sinf(s.a)*expf(-t*20.0f)*0.7f + s.b*expf(-t*22.0f)*1.3f + tail)*0.9f; }
static float fSwing(float t, SS& s)   { s.a += (Noise() - s.a)*Lerpf(0.5f, 0.12f, Sat(t/0.2f)); return s.a*sinf(PI*Sat(t/0.22f))*1.6f; }
static float fHit(float t, SS& s)     { s.a += 2*PI*(110.0f*expf(-t*18.0f) + 55.0f)/SR; return (sinf(s.a)*expf(-t*26.0f) + Noise()*0.45f*expf(-t*70.0f))*0.9f; }
static float fStep(float t, SS& s)    { s.a += (Noise() - s.a)*0.18f; return s.a*expf(-t*42.0f)*1.7f; }
static float fPickup(float t, SS&)    { return (sinf(2*PI*880.0f*t) + 0.6f*sinf(2*PI*1320.0f*t))*expf(-t*14.0f)*0.35f*Minf(1.0f, t*300.0f); }
static float fClick(float t, SS&)     { return (sinf(2*PI*1800.0f*t) + Noise()*0.4f)*expf(-t*180.0f)*0.6f; }
static float fEmpty(float t, SS&)     { return (sinf(2*PI*620.0f*t) + Noise()*0.3f)*expf(-t*110.0f)*0.7f; }
static float fReload(float t, SS& s)  { float v = 0; if (t < 0.05f) v += Noise()*expf(-t*120.0f); if (t > 0.18f && t < 0.3f) v += Noise()*expf(-(t - 0.18f)*100.0f)*1.2f; s.a += (Noise() - s.a)*0.5f; return (v*0.8f + s.a*0.0f)*0.9f + sinf(2*PI*900.0f*t)*expf(-t*60.0f)*0.2f; }
static float fHurt(float t, SS& s)    { s.a += 2*PI*(130.0f - 60.0f*t)/SR; s.b += (Noise() - s.b)*0.3f; return (sinf(s.a)*0.6f + s.b*0.5f)*expf(-t*7.0f)*0.9f; }
static float fDeath(float t, SS& s)   { s.a += 2*PI*(200.0f*expf(-t*2.2f) + 40.0f)/SR; s.b += (Noise() - s.b)*0.2f; return (sinf(s.a)*0.7f + s.b*0.4f)*expf(-t*3.2f); }
static float fLevel(float t, SS&)     { const float n[4] = { 523.25f, 659.25f, 783.99f, 1046.5f }; int i = Mini(3, (int)(t/0.11f)); float lt = t - (float)i*0.11f; return sinf(2*PI*n[i]*t)*expf(-lt*7.0f)*0.4f*(t < 0.6f ? 1.0f : expf(-(t - 0.6f)*8.0f)); }
static float fSave(float t, SS&)      { return (sinf(2*PI*660.0f*t) + sinf(2*PI*990.0f*t)*0.7f)*expf(-t*9.0f)*0.3f; }
static float fQuest(float t, SS&)     { float f = t < 0.15f ? 523.0f : (t < 0.3f ? 659.0f : 784.0f); return (sinf(2*PI*f*t) + 0.4f*sinf(4*PI*f*t))*expf(-t*5.0f)*0.3f; }
static float fGeiger(float t, SS&)    { return Noise()*expf(-t*500.0f)*1.6f; }
static float fGrowl(float t, SS& s)   { float f = 68.0f + 14.0f*sinf(2*PI*9.0f*t); s.a += 2*PI*f/SR; float saw = fmodf(s.a/(2*PI), 1.0f)*2.0f - 1.0f; s.b += (Noise() - s.b)*0.1f; return (saw*0.5f + s.b*0.6f)*Minf(1.0f, t*20.0f)*expf(-Maxf(0.0f, t - 0.3f)*6.0f)*0.9f; }
static float fBark(float t, SS& s)    { float f = 520.0f*expf(-t*5.0f) + 180.0f; s.a += 2*PI*f/SR; s.b += (Noise() - s.b)*0.25f; return (sinf(s.a)*0.5f + s.b*0.5f + 0.3f*sinf(2*s.a))*expf(-t*13.0f)*Minf(1.0f, t*200.0f)*0.9f; }
static float fHeal(float t, SS& s)    { s.a += 2*PI*(400.0f + 700.0f*t)/SR; return sinf(s.a)*sinf(PI*Sat(t/0.45f))*0.35f; }
static float fZap(float t, SS& s)     { s.a += (Noise() - s.a)*0.8f; float buzz = sinf(2*PI*(900.0f + 500.0f*sinf(t*60.0f))*t); return (s.a*0.8f + buzz*0.4f)*expf(-t*9.0f)*(0.6f + 0.4f*sinf(t*220.0f)); }
static float fExplode(float t, SS& s) { s.a += 2*PI*(70.0f*expf(-t*3.0f) + 25.0f)/SR; s.b += (Noise() - s.b)*0.12f; return (sinf(s.a)*0.9f + s.b*1.2f)*expf(-t*3.2f); }
static float fCrash(float t, SS& s)   { s.a += 2*PI*(120.0f*expf(-t*20.0f) + 50.0f)/SR; s.b += (Noise() - s.b)*0.5f; return (sinf(s.a)*0.7f*expf(-t*14.0f) + s.b*expf(-t*18.0f)*1.2f + sinf(2*PI*1500.0f*t)*0.1f*expf(-t*30.0f)); }
static float fBulletHit(float t, SS& s) { s.a += 2*PI*(2400.0f - 1800.0f*Sat(t/0.25f))/SR; return (sinf(s.a)*0.5f + Noise()*0.2f)*expf(-t*18.0f)*0.5f; }

static void Synth(int id, float secs, float (*fn)(float, SS&)) {
    int n = (int)(secs*SR); short* d = (short*)malloc((size_t)n*sizeof(short)); SS st; st.a = st.b = st.c = 0;
    for (int i = 0; i < n; i++) d[i] = (short)(Clampf(fn((float)i/SR, st), -1.0f, 1.0f)*30000.0f);
    g_s[id].d = d; g_s[id].n = n;
}
static Sample g_eng; static volatile float g_engTarget = 0, g_engRpm = 0;
static void SynthEngine() {                                  // ciclo de motor com harmonicos inteiros: 15 ciclos de 60 Hz = loop sem emenda
    int n = SR/60*15; short* d = (short*)malloc((size_t)n*sizeof(short));
    for (int i = 0; i < n; i++) { float a = 2*PI*60.0f*(float)i/SR; float v = 0.5f*sinf(a) + 0.35f*sinf(2*a + 0.5f) + 0.22f*sinf(3*a + 1.1f) + 0.14f*sinf(5*a) + 0.1f*sinf(8*a + 2.0f); float pulse = 0.75f + 0.25f*sinf(a*0.5f); d[i] = (short)(Clampf(v*pulse*0.6f, -1.0f, 1.0f)*20000.0f); }
    g_eng.d = d; g_eng.n = n;
}
static void SynthWind() {                                    // vento: ruido filtrado com rajadas lentas, emenda em crossfade
    const int n = SR*5, ov = SR/4; float* tmp = (float*)malloc((size_t)(n + ov)*sizeof(float)); float lp = 0, lp2 = 0;
    for (int i = 0; i < n + ov; i++) {
        float t = (float)i/SR; lp += (Noise() - lp)*0.035f; lp2 += (lp - lp2)*0.25f;
        tmp[i] = lp2*(0.55f + 0.35f*sinf(2*PI*0.2f*t) + 0.2f*sinf(2*PI*0.53f*t + 1.0f))*5.5f;
    }
    short* d = (short*)malloc((size_t)n*sizeof(short));
    for (int i = 0; i < n; i++) { float v = tmp[i]; if (i < ov) { float a = (float)i/ov; v = tmp[i]*a + tmp[n + i]*(1.0f - a); } d[i] = (short)(Clampf(v, -1.0f, 1.0f)*16000.0f); }
    free(tmp); g_wind.d = d; g_wind.n = n;
}

struct Voice { const short* d; int n, pos; float vol; };
static Voice g_v[NVOICE];
static CRITICAL_SECTION g_cs;
static HWAVEOUT g_wo;
static WAVEHDR g_hdr[NBUF];
static short g_buf[NBUF][BUFN];
static HANDLE g_thread;
static volatile LONG g_run;
static volatile float g_windTarget = 0.3f, g_geigerTarget = 0;
static bool g_mixer = false;

static void FillBuffer(short* out, float& windVol, int& windPos, float& engVol, float& engRate, float& engPos) {
    float mix[BUFN]; for (int i = 0; i < BUFN; i++) mix[i] = 0;
    EnterCriticalSection(&g_cs);
    for (int v = 0; v < NVOICE; v++) {
        Voice& vo = g_v[v]; if (!vo.d) continue;
        int cnt = Mini(BUFN, vo.n - vo.pos);
        for (int i = 0; i < cnt; i++) mix[i] += (float)vo.d[vo.pos + i]*vo.vol;
        vo.pos += cnt; if (vo.pos >= vo.n) vo.d = 0;
    }
    LeaveCriticalSection(&g_cs);
    float target = g_windTarget*G.set.volume;
    for (int i = 0; i < BUFN; i++) {
        windVol += (target - windVol)*0.00015f;
        mix[i] += (float)g_wind.d[windPos]*windVol*0.5f;
        engVol += (g_engTarget - engVol)*0.0008f; engRate += ((0.55f + 1.9f*g_engRpm) - engRate)*0.0006f;
        if (engVol > 0.002f && g_eng.d) { int ip = (int)engPos; float fr = engPos - (float)ip; int i2 = (ip + 1) % g_eng.n; mix[i] += ((float)g_eng.d[ip]*(1.0f - fr) + (float)g_eng.d[i2]*fr)*engVol; engPos += engRate; if (engPos >= (float)g_eng.n) engPos -= (float)g_eng.n; }
        if (++windPos >= g_wind.n) windPos = 0;
        out[i] = (short)Clampf(mix[i]*0.8f, -32000.0f, 32000.0f);
    }
}
static DWORD WINAPI MixThread(LPVOID) {
    float windVol = 0.1f; int windPos = 0; float engVol = 0, engRate = 0.6f, engPos = 0;
    while (g_run) {
        bool worked = false;
        for (int b = 0; b < NBUF; b++) {
            if (!(g_hdr[b].dwFlags & WHDR_DONE)) continue;
            FillBuffer(g_buf[b], windVol, windPos, engVol, engRate, engPos); waveOutWrite(g_wo, &g_hdr[b], sizeof(WAVEHDR)); worked = true;
        }
        Sleep(worked ? 1 : 3);
    }
    return 0;
}
static bool StartMixer() {
    WAVEFORMATEX fmt; memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM; fmt.nChannels = 1; fmt.nSamplesPerSec = SR; fmt.wBitsPerSample = 16; fmt.nBlockAlign = 2; fmt.nAvgBytesPerSec = SR*2;
    if (waveOutOpen(&g_wo, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return false;
    InitializeCriticalSection(&g_cs);
    for (int b = 0; b < NBUF; b++) {
        memset(&g_hdr[b], 0, sizeof(WAVEHDR)); g_hdr[b].lpData = (LPSTR)g_buf[b]; g_hdr[b].dwBufferLength = BUFN*2;
        if (waveOutPrepareHeader(g_wo, &g_hdr[b], sizeof(WAVEHDR)) != MMSYSERR_NOERROR) { waveOutClose(g_wo); return false; }
        g_hdr[b].dwFlags |= WHDR_DONE;
    }
    g_run = 1; g_thread = CreateThread(NULL, 0, MixThread, NULL, 0, NULL);
    if (!g_thread) { g_run = 0; waveOutClose(g_wo); return false; }
    SetThreadPriority(g_thread, THREAD_PRIORITY_ABOVE_NORMAL);
    return true;
}

void InitAudio() {
    Synth(SND_PISTOL, 0.35f, fPistol); Synth(SND_SHOTGUN, 0.6f, fShotgun); Synth(SND_RIFLE, 0.7f, fRifle); Synth(SND_SWING, 0.25f, fSwing);
    Synth(SND_HIT, 0.2f, fHit); Synth(SND_STEP, 0.12f, fStep); Synth(SND_PICKUP, 0.3f, fPickup); Synth(SND_CLICK, 0.05f, fClick);
    Synth(SND_EMPTY, 0.08f, fEmpty); Synth(SND_RELOAD, 0.4f, fReload); Synth(SND_HURT, 0.45f, fHurt); Synth(SND_DEATH, 1.2f, fDeath);
    Synth(SND_LEVEL, 0.9f, fLevel); Synth(SND_SAVE, 0.4f, fSave); Synth(SND_QUEST, 0.8f, fQuest); Synth(SND_GEIGER, 0.02f, fGeiger);
    Synth(SND_GROWL, 0.9f, fGrowl); Synth(SND_BARK, 0.3f, fBark); Synth(SND_HEAL, 0.5f, fHeal); Synth(SND_BULLETHIT, 0.3f, fBulletHit); Synth(SND_ZAP, 0.35f, fZap); Synth(SND_EXPLODE, 1.6f, fExplode); Synth(SND_CRASH, 0.4f, fCrash); SynthEngine();
    SynthWind();
    g_mixer = StartMixer();
}
void ShutdownAudio() {
    if (g_mixer) {
        g_run = 0; WaitForSingleObject(g_thread, 1000); CloseHandle(g_thread); waveOutReset(g_wo);
        for (int b = 0; b < NBUF; b++) waveOutUnprepareHeader(g_wo, &g_hdr[b], sizeof(WAVEHDR));
        waveOutClose(g_wo); DeleteCriticalSection(&g_cs); g_mixer = false;
    }
    for (int i = 0; i < SND_COUNT; i++) { free(g_s[i].d); g_s[i].d = 0; }
    free(g_wind.d); g_wind.d = 0; free(g_eng.d); g_eng.d = 0;
}

void PlaySfx(int id, float vol) {
    vol *= G.set.volume;
    if (!g_mixer || id < 0 || id >= SND_COUNT || !g_s[id].d || vol < 0.03f) return;
    EnterCriticalSection(&g_cs);
    int slot = -1, oldest = 0, oldestPos = -1;
    for (int v = 0; v < NVOICE; v++) { if (!g_v[v].d) { slot = v; break; } if (g_v[v].pos > oldestPos) { oldestPos = g_v[v].pos; oldest = v; } }
    if (slot < 0) slot = oldest;
    g_v[slot].d = g_s[id].d; g_v[slot].n = g_s[id].n; g_v[slot].pos = 0; g_v[slot].vol = Clampf(vol, 0.0f, 1.0f);
    LeaveCriticalSection(&g_cs);
}
void SetEngine(bool on, float rpm01) { g_engTarget = on ? 0.55f*G.set.volume : 0.0f; g_engRpm = Clampf(rpm01, 0.0f, 1.0f); }
void SetAmbience(float wind, float geiger) { g_windTarget = Clampf(wind, 0.0f, 1.0f); g_geigerTarget = geiger; }
