// =====================================================================
//  common.h - tipos basicos, matematica, ruido procedural e RNG
//
//  Convencoes de mundo (metros, segundos):
//    X = leste (+), Y = cima, Z = SUL (+)  ->  NORTE = -Z.
//    yaw = 0 olha para o norte (-Z); yaw positivo gira para a ESQUERDA
//    (anti-horario visto de cima).  Facing(yaw) = (-sin, 0, -cos).
//    Mapa 2D: x cresce para a direita, z cresce para baixo (norte no topo).
// =====================================================================
#pragma once
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

static const float PI = 3.14159265358979f;

struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x*s, y*s, z*s); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
};
static inline float Dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline Vec3  Cross(const Vec3& a, const Vec3& b) { return Vec3(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x); }
static inline float Len(const Vec3& a) { return sqrtf(Dot(a, a)); }
static inline float Len2D(const Vec3& a) { return sqrtf(a.x*a.x + a.z*a.z); }
static inline Vec3  Norm(const Vec3& a) { float l = Len(a); return l > 1e-8f ? a*(1.0f/l) : Vec3(0, 0, 0); }

static inline float Clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline int   Clampi(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }
static inline float Sat(float v) { return Clampf(v, 0.0f, 1.0f); }
static inline float Minf(float a, float b) { return a < b ? a : b; }
static inline float Maxf(float a, float b) { return a > b ? a : b; }
static inline int   Mini(int a, int b) { return a < b ? a : b; }
static inline int   Maxi(int a, int b) { return a > b ? a : b; }
static inline float Lerpf(float a, float b, float t) { return a + (b - a)*t; }
static inline float Sqr(float v) { return v*v; }
static inline float Smoothstep(float a, float b, float v) { float t = Sat((v - a)/(b - a)); return t*t*(3.0f - 2.0f*t); }
static inline float WrapAngle(float a) { while (a > PI) a -= 2*PI; while (a < -PI) a += 2*PI; return a; }
static inline float AngleDiff(float from, float to) { return WrapAngle(to - from); }
static inline float TurnToward(float cur, float target, float maxStep) {
    float d = AngleDiff(cur, target); if (d > maxStep) d = maxStep; else if (d < -maxStep) d = -maxStep; return WrapAngle(cur + d);
}
static inline Vec3  Facing(float yaw) { return Vec3(-sinf(yaw), 0, -cosf(yaw)); }
static inline float YawOf(float dx, float dz) { return atan2f(-dx, -dz); }
static inline float Approach(float cur, float target, float rate, float dt) { return cur + (target - cur)*(1.0f - expf(-rate*dt)); }

// ---------------------------------------------------------------------
//  Hash / ruido (puros e deterministicos -> seguros em qualquer thread)
// ---------------------------------------------------------------------
static inline u32 Hash32(u32 x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static inline u32 HashXZ(int x, int z, u32 seed) { return Hash32((u32)x*0x1B873593u ^ Hash32((u32)z*0x85EBCA6Bu + seed*0x9E3779B9u + 0x1234567u)); }
static inline float Hash01(int x, int z, u32 seed) { return (float)(HashXZ(x, z, seed) & 0xFFFFFF)/16777216.0f; }

static inline float VNoise(float x, float z, u32 seed) {
    int xi = (int)floorf(x), zi = (int)floorf(z);
    float fx = x - (float)xi, fz = z - (float)zi;
    fx = fx*fx*(3.0f - 2.0f*fx); fz = fz*fz*(3.0f - 2.0f*fz);
    float a = Hash01(xi, zi, seed), b = Hash01(xi + 1, zi, seed), c = Hash01(xi, zi + 1, seed), d = Hash01(xi + 1, zi + 1, seed);
    return Lerpf(Lerpf(a, b, fx), Lerpf(c, d, fx), fz);
}
static inline float Fbm(float x, float z, int oct, u32 seed) {
    float amp = 0.5f, sum = 0, norm = 0;
    for (int i = 0; i < oct; i++) { sum += amp*VNoise(x, z, seed + (u32)i*131u); norm += amp; x *= 2.03f; z *= 2.03f; amp *= 0.5f; }
    return sum/norm;
}

// RNG local (cada thread/entidade tem o seu -> sem estado global)
struct Rng {
    u32 s;
    Rng(u32 seed = 1) : s(seed ? seed : 0x9E3779B9u) { next(); next(); }
    u32 next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float f() { return (float)(next() & 0xFFFFFF)/16777216.0f; }
    float r(float a, float b) { return a + (b - a)*f(); }
    int   i(int n) { return n <= 1 ? 0 : (int)(next() % (u32)n); }
    bool  chance(float p) { return f() < p; }
};

// cor 8 bits
struct Col { u8 r, g, b; Col() : r(128), g(128), b(128) {} Col(int R, int G, int B) : r((u8)Clampi(R, 0, 255)), g((u8)Clampi(G, 0, 255)), b((u8)Clampi(B, 0, 255)) {} };
static inline Col Shade(Col c, float k) { return Col((int)(c.r*k), (int)(c.g*k), (int)(c.b*k)); }
static inline Col Mix(Col a, Col b, float t) { return Col((int)Lerpf(a.r, b.r, t), (int)Lerpf(a.g, b.g, t), (int)Lerpf(a.b, b.b, t)); }
