// Matematica minima (float, matrizes 4x4 COLUNA-major como no OpenGL e no glTF).
#pragma once
#include <cmath>
#include <cstring>

namespace f3v {

struct Vec3 { float x = 0, y = 0, z = 0; };

struct Mat4 {
    float m[16];                                   // m[coluna*4 + linha]
    Mat4() { identity(); }
    void identity() { for (int i = 0; i < 16; ++i) m[i] = (i % 5 == 0) ? 1.f : 0.f; }
};

inline Mat4 mul(const Mat4& a, const Mat4& b) {    // a * b
    Mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int rI = 0; rI < 4; ++rI) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[k * 4 + rI] * b.m[c * 4 + k];
            r.m[c * 4 + rI] = s;
        }
    return r;
}

// Matriz de translacao * rotacao(quaternion x,y,z,w) * escala
inline Mat4 compose_trs(const float t[3], const float q[4], const float s[3]) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    const float n = std::sqrt(x * x + y * y + z * z + w * w);
    if (n > 1e-12f) { x /= n; y /= n; z /= n; w /= n; } else { x = y = z = 0; w = 1; }
    const float r00 = 1 - 2 * (y * y + z * z), r01 = 2 * (x * y - z * w), r02 = 2 * (x * z + y * w);
    const float r10 = 2 * (x * y + z * w), r11 = 1 - 2 * (x * x + z * z), r12 = 2 * (y * z - x * w);
    const float r20 = 2 * (x * z - y * w), r21 = 2 * (y * z + x * w), r22 = 1 - 2 * (x * x + y * y);
    Mat4 m;
    m.m[0] = r00 * s[0]; m.m[1] = r10 * s[0]; m.m[2] = r20 * s[0]; m.m[3] = 0;
    m.m[4] = r01 * s[1]; m.m[5] = r11 * s[1]; m.m[6] = r21 * s[1]; m.m[7] = 0;
    m.m[8] = r02 * s[2]; m.m[9] = r12 * s[2]; m.m[10] = r22 * s[2]; m.m[11] = 0;
    m.m[12] = t[0]; m.m[13] = t[1]; m.m[14] = t[2]; m.m[15] = 1;
    return m;
}

inline Vec3 transform_point(const Mat4& a, float x, float y, float z) {
    return {a.m[0] * x + a.m[4] * y + a.m[8] * z + a.m[12], a.m[1] * x + a.m[5] * y + a.m[9] * z + a.m[13],
            a.m[2] * x + a.m[6] * y + a.m[10] * z + a.m[14]};
}
inline Vec3 transform_dir(const Mat4& a, float x, float y, float z) {
    return {a.m[0] * x + a.m[4] * y + a.m[8] * z, a.m[1] * x + a.m[5] * y + a.m[9] * z, a.m[2] * x + a.m[6] * y + a.m[10] * z};
}

inline void slerp(const float a[4], const float b_in[4], float t, float out[4]) {
    float b[4] = {b_in[0], b_in[1], b_in[2], b_in[3]};
    float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    if (d < 0) { for (float& v : b) v = -v; d = -d; }
    float q[4];
    if (d > 0.9995f) {
        for (int i = 0; i < 4; ++i) q[i] = a[i] + t * (b[i] - a[i]);
    } else {
        const float th = std::acos(d > 1.f ? 1.f : d), s = std::sin(th);
        const float wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
        for (int i = 0; i < 4; ++i) q[i] = wa * a[i] + wb * b[i];
    }
    const float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    for (int i = 0; i < 4; ++i) out[i] = n > 1e-12f ? q[i] / n : (i == 3 ? 1.f : 0.f);
}

}  // namespace f3v
