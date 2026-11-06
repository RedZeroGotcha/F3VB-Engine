#include "ragdoll.h"

// ---------------------------------------------------------------------
//  Quaternions
// ---------------------------------------------------------------------
Quat operator*(const Quat& a, const Quat& b) {
    return Quat(a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z, a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y, a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x, a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w);
}
Quat Conj(const Quat& q) { return Quat(q.w, -q.x, -q.y, -q.z); }
Quat Normalize(const Quat& q) { float l = sqrtf(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z); if (l < 1e-12f) return Quat(); float i = 1.0f/l; return Quat(q.w*i, q.x*i, q.y*i, q.z*i); }
Vec3 Rotate(const Quat& q, const Vec3& v) { Vec3 qv(q.x, q.y, q.z); Vec3 t = Cross(qv, v)*2.0f; return v + t*q.w + Cross(qv, t); }
Quat FromTo(const Vec3& a, const Vec3& b) {
    Vec3 u = Norm(a), v = Norm(b); float d = Dot(u, v);
    if (d > 0.999999f) return Quat();
    if (d < -0.999999f) { Vec3 ax = Cross(u, Vec3(1, 0, 0)); if (Len(ax) < 1e-4f) ax = Cross(u, Vec3(0, 0, 1)); ax = Norm(ax); return Quat(0, ax.x, ax.y, ax.z); }
    Vec3 c = Cross(u, v); return Normalize(Quat(1.0f + d, c.x, c.y, c.z));
}

// ---------------------------------------------------------------------
//  Utilidades de corpo rigido
// ---------------------------------------------------------------------
static Vec3 InvIW(const RBody& b, const Vec3& v) {                              // I^-1 (mundo) * v
    Vec3 l = Rotate(Conj(b.q), v); l = Vec3(l.x*b.invI.x, l.y*b.invI.y, l.z*b.invI.z); return Rotate(b.q, l);
}
static void ApplyRot(RBody& b, const Vec3& dth) {                               // q += 1/2 * [0,dtheta] * q
    Quat r = Quat(0, dth.x, dth.y, dth.z)*b.q; b.q = Normalize(Quat(b.q.w + 0.5f*r.w, b.q.x + 0.5f*r.x, b.q.y + 0.5f*r.y, b.q.z + 0.5f*r.z));
}
static bool OkF(float f) { return f == f && fabsf(f) < 1e8f; }

// correcao angular entre dois corpos: C>0 = B girado +C (em torno de n) em relacao a A
static void AngCorrect(RBody& A, RBody& B, const Vec3& n, float C, float compliance, float ih) {
    Vec3 iA = InvIW(A, n), iB = InvIW(B, n); float wA = Dot(n, iA), wB = Dot(n, iB), alpha = compliance*ih*ih, den = wA + wB + alpha;
    if (den < 1e-9f) return; C = Clampf(C, -0.25f, 0.25f);                    // nunca corrige 'a golpes' (evita estalos nos limites)
    float lambda = C/den; if (!OkF(lambda)) return;
    ApplyRot(A, iA*lambda); ApplyRot(B, iB*(-lambda));
}
// restricao posicional: as duas ancoras devem coincidir
static void PosCorrect(const RJoint& J, RBody& A, RBody& B, float ih) {
    Vec3 rA = Rotate(A.q, J.la), rB = Rotate(B.q, J.lb), d = (A.x + rA) - (B.x + rB); float len = Len(d); if (len < 1e-7f) return;
    Vec3 n = d*(1.0f/len), rAn = Cross(rA, n), rBn = Cross(rB, n), iA = InvIW(A, rAn), iB = InvIW(B, rBn);
    float wA = A.invM + Dot(rAn, iA), wB = B.invM + Dot(rBn, iB), alpha = J.compliance*ih*ih, den = wA + wB + alpha; if (den < 1e-9f) return;
    float lambda = -len/den; if (!OkF(lambda)) return;
    A.x += n*(lambda*A.invM); ApplyRot(A, iA*lambda);
    B.x -= n*(lambda*B.invM); ApplyRot(B, iB*(-lambda));
}
static void SolveJoint(const RJoint& J, RBody& A, RBody& B, float ih) {
    PosCorrect(J, A, B, ih);
    if (J.type == 1) {                                                          // ---- DOBRADICA ----
        Vec3 a = Rotate(A.q, J.axA), bb = Rotate(B.q, J.axB), e = Cross(a, bb); float el = Len(e);
        if (el > 1e-6f) AngCorrect(A, B, e*(1.0f/el), asinf(Minf(1.0f, el)), 0, ih);          // alinha os eixos
        Vec3 ax = Norm(Rotate(A.q, J.axA) + Rotate(B.q, J.axB)); if (Len(ax) < 1e-6f) return;
        Vec3 rA = Rotate(A.q, J.refA), rB = Rotate(B.q, J.refB); rA = rA - ax*Dot(rA, ax); rB = rB - ax*Dot(rB, ax);
        float phi = atan2f(Dot(Cross(rA, rB), ax), Dot(rA, rB));                                // angulo da dobradica
        if (phi > J.hi) AngCorrect(A, B, ax, phi - J.hi, J.compliance, ih); else if (phi < J.lo) AngCorrect(A, B, ax, phi - J.lo, J.compliance, ih);
    } else {                                                                    // ---- ESFERA: cone + torcao ----
        Vec3 tA = Rotate(A.q, J.axA), tB = Rotate(B.q, J.axB); float sw = acosf(Clampf(Dot(tA, tB), -1.0f, 1.0f));
        if (sw > J.swing) { Vec3 n = Cross(tA, tB); float nl = Len(n); if (nl > 1e-6f) AngCorrect(A, B, n*(1.0f/nl), sw - J.swing, J.compliance, ih); }
        tA = Rotate(A.q, J.axA); Vec3 rA = Rotate(A.q, J.refA), rB = Rotate(B.q, J.refB); rA = rA - tA*Dot(rA, tA); rB = rB - tA*Dot(rB, tA);
        if (Len(rA) < 1e-5f || Len(rB) < 1e-5f) return;
        float phi = atan2f(Dot(Cross(rA, rB), tA), Dot(rA, rB));
        if (phi > J.twist) AngCorrect(A, B, tA, phi - J.twist, J.compliance, ih); else if (phi < -J.twist) AngCorrect(A, B, tA, phi + J.twist, J.compliance, ih);
    }
}
// contato: empurra o ponto p da capsula ao longo de n por 'depth' (corrige posicao e giro)
static void Contact(RBody& b, const Vec3& p, const Vec3& n, float depth) {
    Vec3 r = p - b.x, rn = Cross(r, n), irn = InvIW(b, rn); float w = b.invM + Dot(rn, irn); if (w < 1e-9f) return;
    float lambda = depth/w; if (!OkF(lambda)) return; b.x += n*(lambda*b.invM); ApplyRot(b, irn*lambda); b.contact = true;
}

// ---------------------------------------------------------------------
//  Construcao
// ---------------------------------------------------------------------
Ragdoll::Ragdoll() : type(RAG_HUMAN), nb(0), nj(0), scale(1), age(0), calmT(0), lastSpeed(10.0f), asleep(false), org(), yawR(0) { }
Vec3 Ragdoll::Dw(const Vec3& l) const { float c = cosf(yawR), s = sinf(yawR); return Vec3(l.x*c + l.z*s, l.y, -l.x*s + l.z*c); }
Vec3 Ragdoll::Tw(const Vec3& l) const { return org + Dw(l*scale); }

int Ragdoll::AddBody(const Vec3& A, const Vec3& B, float r, float mass, int part, float sx, float sz) {
    Vec3 pa = Tw(A), pb = Tw(B), mid = (pa + pb)*0.5f, d = pb - pa; float len = Len(d); RBody& o = b[nb];
    o.x = o.xp = mid; o.v = o.w = Vec3(); o.q = o.qp = FromTo(Vec3(0, 1, 0), len > 1e-5f ? d : Vec3(0, 1, 0));
    o.r = r*scale; o.hl = Maxf(0.0f, len*0.5f - o.r); o.invM = 1.0f/mass; o.part = part; o.sx = sx; o.sz = sz; o.contact = false;
    float L = 2.0f*o.hl + 1.5f*o.r, Iax = 0.5f*mass*o.r*o.r, Ipp = mass*(3.0f*o.r*o.r + L*L)/12.0f;     // inercia de cilindro equivalente
    o.invI = Vec3(1.0f/Maxf(Ipp, 1e-4f), 1.0f/Maxf(Iax, 1e-4f), 1.0f/Maxf(Ipp, 1e-4f));
    return nb++;
}
void Ragdoll::AddBall(int a, int bb, const Vec3& anchor, const Vec3& twistDir, const Vec3& refDir, float swing, float twist, float compliance) {
    RJoint& J = j[nj++]; Vec3 P = Tw(anchor), T = Norm(Dw(twistDir)), R = Norm(Dw(refDir)); J.a = (short)a; J.b = (short)bb; J.type = 0;
    J.la = Rotate(Conj(b[a].q), P - b[a].x); J.lb = Rotate(Conj(b[bb].q), P - b[bb].x);
    J.axA = Rotate(Conj(b[a].q), T); J.axB = Rotate(Conj(b[bb].q), T); J.refA = Rotate(Conj(b[a].q), R); J.refB = Rotate(Conj(b[bb].q), R);
    J.swing = swing; J.twist = twist; J.lo = J.hi = 0; J.compliance = compliance;
}
void Ragdoll::AddHinge(int a, int bb, const Vec3& anchor, const Vec3& axis, const Vec3& refDir, float lo, float hi, float compliance) {
    RJoint& J = j[nj++]; Vec3 P = Tw(anchor), T = Norm(Dw(axis)), R = Norm(Dw(refDir)); J.a = (short)a; J.b = (short)bb; J.type = 1;
    J.la = Rotate(Conj(b[a].q), P - b[a].x); J.lb = Rotate(Conj(b[bb].q), P - b[bb].x);
    J.axA = Rotate(Conj(b[a].q), T); J.axB = Rotate(Conj(b[bb].q), T); J.refA = Rotate(Conj(b[a].q), R); J.refB = Rotate(Conj(b[bb].q), R);
    J.swing = J.twist = 0; J.lo = lo; J.hi = hi; J.compliance = compliance;
}

void Ragdoll::BuildHuman(const Vec3& pos, float yaw, float s, const Vec3& vel) {
    type = RAG_HUMAN; scale = s; org = pos; yawR = yaw; nb = nj = 0; age = calmT = 0; lastSpeed = 10.0f; asleep = false;
    //  Personagem olha para -Z; +X e o lado direito. Cada corpo = um osso (extremo A -> extremo B).
    const Vec3 down(0, -1, 0), up(0, 1, 0), X(1, 0, 0);
    int pelvis = AddBody(Vec3(-0.15f, 0.95f, 0), Vec3(0.15f, 0.95f, 0), 0.13f, 7.0f, RP_PELVIS, 1.0f, 1.0f);
    int torso  = AddBody(Vec3(0, 1.08f, 0), Vec3(0, 1.50f, 0), 0.17f, 10.0f, RP_TORSO, 1.45f, 0.95f);
    int head   = AddBody(Vec3(0, 1.62f, 0), Vec3(0, 1.80f, 0), 0.13f, 4.5f, RP_HEAD);
    int uarm[2], farm[2], thigh[2], shin[2], foot[2];
    for (int sd = 0; sd < 2; sd++) {
        float x = sd ? 0.30f : -0.30f, hx = sd ? 0.13f : -0.13f;
        uarm[sd] = AddBody(Vec3(x, 1.46f, 0), Vec3(x + (sd ? 0.02f : -0.02f), 1.17f, 0), 0.065f, 2.2f, RP_UARM);
        farm[sd] = AddBody(Vec3(x + (sd ? 0.02f : -0.02f), 1.17f, 0), Vec3(x + (sd ? 0.02f : -0.02f), 0.86f, 0), 0.06f, 1.6f, RP_FARM);
        thigh[sd] = AddBody(Vec3(hx, 0.92f, 0), Vec3(hx, 0.50f, 0), 0.10f, 6.0f, RP_THIGH);
        shin[sd] = AddBody(Vec3(hx, 0.50f, 0), Vec3(hx, 0.09f, 0), 0.08f, 4.0f, RP_SHIN);
        foot[sd] = AddBody(Vec3(hx, 0.07f, 0.05f), Vec3(hx, 0.07f, -0.20f), 0.06f, 1.2f, RP_FOOT);
    }
    AddBall(pelvis, torso, Vec3(0, 1.05f, 0), up, Vec3(0, 0, -1), 0.50f, 0.45f);                           // coluna
    AddBall(torso, head, Vec3(0, 1.54f, 0), up, Vec3(0, 0, -1), 0.65f, 0.75f);                              // pescoco
    for (int sd = 0; sd < 2; sd++) {
        float x = sd ? 0.30f : -0.30f, hx = sd ? 0.13f : -0.13f, ex = x + (sd ? 0.02f : -0.02f);
        AddBall(torso, uarm[sd], Vec3(x * 0.97f, 1.46f, 0), down, Vec3(0, 0, -1), 1.75f, 1.0f);              // ombro: movimento amplo
        AddHinge(uarm[sd], farm[sd], Vec3(ex, 1.17f, 0), X, down, 0.0f, 2.5f);                                // cotovelo: so flexiona para a frente
        AddBall(pelvis, thigh[sd], Vec3(hx, 0.93f, 0), down, Vec3(0, 0, -1), 1.15f, 0.55f);                 // quadril
        AddHinge(thigh[sd], shin[sd], Vec3(hx, 0.50f, 0), X, down, -2.4f, 0.0f);                               // joelho: so dobra para tras
        AddBall(shin[sd], foot[sd], Vec3(hx, 0.09f, 0), down, Vec3(0, 0, -1), 0.60f, 0.35f);                // tornozelo
    }
    for (int i = 0; i < nb; i++) b[i].v = vel;
}

void Ragdoll::BuildQuad(const Vec3& pos, float yaw, float s, const Vec3& vel) {
    type = RAG_QUAD; scale = s; org = pos; yawR = yaw; nb = nj = 0; age = calmT = 0; lastSpeed = 10.0f; asleep = false;
    const Vec3 down(0, -1, 0), Z(0, 0, 1), X(1, 0, 0);
    int rear  = AddBody(Vec3(0, 0.60f, 0.46f), Vec3(0, 0.60f, 0.08f), 0.15f, 6.0f, RP_PELVIS, 1.0f, 1.0f);
    int chest = AddBody(Vec3(0, 0.62f, 0.02f), Vec3(0, 0.62f, -0.44f), 0.16f, 6.5f, RP_TORSO, 1.0f, 1.0f);
    int head  = AddBody(Vec3(0, 0.74f, -0.62f), Vec3(0, 0.72f, -0.90f), 0.11f, 2.8f, RP_HEAD);
    int tail  = AddBody(Vec3(0, 0.68f, 0.52f), Vec3(0, 0.66f, 0.82f), 0.04f, 0.5f, RP_TAIL);
    int up_[4], lo_[4];
    const float lx[4] = { -0.12f, 0.12f, -0.12f, 0.12f }, lz[4] = { -0.30f, -0.30f, 0.32f, 0.32f };
    for (int i = 0; i < 4; i++) {
        up_[i] = AddBody(Vec3(lx[i], 0.55f, lz[i]), Vec3(lx[i], 0.30f, lz[i] + (i >= 2 ? 0.03f : 0.0f)), 0.06f, 1.0f, RP_LEG);
        lo_[i] = AddBody(Vec3(lx[i], 0.30f, lz[i] + (i >= 2 ? 0.03f : 0.0f)), Vec3(lx[i], 0.05f, lz[i]), 0.05f, 0.6f, RP_LEG);
    }
    AddBall(rear, chest, Vec3(0, 0.62f, 0.05f), Vec3(0, 0, -1), Vec3(0, 1, 0), 0.55f, 0.40f);              // coluna
    AddBall(chest, head, Vec3(0, 0.72f, -0.55f), Vec3(0, 0, -1), Vec3(0, 1, 0), 0.80f, 0.60f);             // pescoco
    AddBall(rear, tail, Vec3(0, 0.67f, 0.50f), Vec3(0, 0, 1), Vec3(0, 1, 0), 1.0f, 0.8f);
    for (int i = 0; i < 4; i++) {
        int parent = (i < 2) ? chest : rear;
        AddBall(parent, up_[i], Vec3(lx[i], 0.56f, lz[i]), down, Vec3(0, 0, -1), 1.05f, 0.45f);
        AddHinge(up_[i], lo_[i], Vec3(lx[i], 0.30f, lz[i] + (i >= 2 ? 0.03f : 0.0f)), X, down, -1.9f, 0.5f);
    }
    (void)Z;
    for (int i = 0; i < nb; i++) b[i].v = vel;
}

Vec3 Ragdoll::Pelvis() const { return b[0].x; }
void Ragdoll::Impulse(const Vec3& at, const Vec3& dir, float force) {
    for (int i = 0; i < nb; i++) {
        Vec3 d = b[i].x - at; float wgt = 0.30f + 0.70f*expf(-Dot(d, d)/0.45f);
        b[i].v += dir*(force*wgt); Vec3 spin = Cross(d, dir)*(force*wgt*0.35f); float sl = Len(spin); if (sl > 4.0f) spin = spin*(4.0f/sl); b[i].w += spin;   // braco de alavanca -> giro
    }
    asleep = false; calmT = 0;
}

// ---------------------------------------------------------------------
//  Simulacao (XPBD em subpassos)
// ---------------------------------------------------------------------
static void Contacts(Ragdoll& R) {
    for (int i = 0; i < R.nb; i++) {
        RBody& B = R.b[i]; Vec3 ax = Rotate(B.q, Vec3(0, 1, 0))*B.hl; Vec3 pts[3] = { B.x + ax, B.x - ax, B.x }; int np = (B.hl > 0.11f) ? 3 : 2;
        for (int k = 0; k < np; k++) {
            Vec3 p = pts[k]; float gx = p.x, gz = p.z, gy; bool wall = W.Collide(gx, gz, B.r, p.y, gy);
            float floorY = Maxf(TerrainH(p.x, p.z), gy), depth = floorY + B.r - p.y;
            if (depth > 0) Contact(B, p, Vec3(0, 1, 0), depth);
            if (wall) { Vec3 d(gx - p.x, 0, gz - p.z); float l = Len(d); if (l > 1e-5f) Contact(B, p, d*(1.0f/l), l); }
        }
    }
}

void Ragdoll::Step(float dt) {
    if (asleep) return;
    if (dt > 0.04f) dt = 0.04f; age += dt;
    const int SUB = 8; const float h = dt/(float)SUB, ih = 1.0f/h; float speed = 0;
    for (int s = 0; s < SUB; s++) {
        for (int i = 0; i < nb; i++) {                                           // ---- predicao ----
            RBody& B = b[i]; B.xp = B.x; B.qp = B.q; B.contact = false;
            B.v.y -= 14.0f*h; B.v = B.v*(1.0f - 0.25f*h); B.w = B.w*(1.0f - 2.5f*h);
            B.x += B.v*h; Quat r = Quat(0, B.w.x, B.w.y, B.w.z)*B.q; B.q = Normalize(Quat(B.q.w + 0.5f*h*r.w, B.q.x + 0.5f*h*r.x, B.q.y + 0.5f*h*r.y, B.q.z + 0.5f*h*r.z));
        }
        for (int it = 0; it < 2; it++) {                                         // ---- juntas + contatos ----
            for (int k = 0; k < nj; k++) SolveJoint(j[k], b[j[k].a], b[j[k].b], ih);
            for (int k = 0; k < nj; k++) PosCorrect(j[k], b[j[k].a], b[j[k].b], ih);       // fecha as ancoras depois dos angulos
            Contacts(*this);                                                                 // chao e paredes por ultimo: nunca terminam penetrados
        }
        speed = 0;
        for (int i = 0; i < nb; i++) {                                           // ---- velocidades a partir das posicoes ----
            RBody& B = b[i]; B.v = (B.x - B.xp)*ih; Quat dq = B.q*Conj(B.qp); Vec3 w(dq.x, dq.y, dq.z); w = w*(2.0f*ih); if (dq.w < 0) w = w*-1.0f; B.w = w;
            if (B.contact) { float settle = (lastSpeed < 1.3f) ? 0.55f : 1.0f; B.v.x *= 0.88f*settle; B.v.z *= 0.88f*settle; B.v.y *= 0.5f; B.w = B.w*(0.86f*settle); }   // ao assentar, amortece forte                                            // atrito/inelasticidade no chao
            { float vl = Len(B.v), wl = Len(B.w); if (vl > 30.0f) B.v = B.v*(30.0f/vl); if (wl > 25.0f) B.w = B.w*(25.0f/wl); }
            speed += Len(B.v) + 0.25f*Len(B.w);
        }
    }
    speed /= (float)Maxi(nb, 1); lastSpeed = speed;
    if (speed < 0.45f) calmT += dt; else calmT = 0;
    if (calmT > 0.5f || age > 8.0f) { asleep = true; for (int i = 0; i < nb; i++) b[i].v = b[i].w = Vec3(); }
}

bool Ragdoll::Finite() const {
    for (int i = 0; i < nb; i++) { const Vec3& p = b[i].x; if (!OkF(p.x) || !OkF(p.y) || !OkF(p.z) || fabsf(p.y) > 1e5f || !OkF(b[i].q.w)) return false; }
    return true;
}
float Ragdoll::MaxJointError() const {
    float m = 0; for (int k = 0; k < nj; k++) { const RJoint& J = j[k]; Vec3 pa = b[J.a].x + Rotate(b[J.a].q, J.la), pb = b[J.b].x + Rotate(b[J.b].q, J.lb); m = Maxf(m, Len(pa - pb)); }
    return m;
}
