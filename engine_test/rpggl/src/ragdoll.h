// =====================================================================
//  ragdoll.h - ragdoll de CORPOS RIGIDOS por XPBD (Position-Based Dynamics)
//
//  * Cada osso e um CORPO RIGIDO (posicao + quaternion + massa + tensor
//    de inercia) com colisor de CAPSULA.  Nao ha particulas nem Verlet.
//  * JUNTAS (constraints) ligam os corpos:
//      - ESFERA (ball-socket): ancora posicional + limite de CONE (swing)
//        + limite de TORCAO (twist)           -> ombros, quadris, coluna, pescoco, tornozelos
//      - DOBRADICA (hinge): alinhamento de eixo + limites angulares
//                                              -> joelhos e cotovelos
//    Todas resolvidas no nivel de posicao/orientacao (XPBD, com compliance),
//    em subpassos pequenos ("small steps", Macklin et al.).
//  * Contatos: capsulas contra terreno e contra os colisores do mundo
//    (construcoes, arvores, carros), com atrito e sono automatico.
//  * Cada ragdoll so LE o mundo -> varios simulados em paralelo no pool.
// =====================================================================
#pragma once
#include "world.h"

struct Quat {
    float w, x, y, z;
    Quat() : w(1), x(0), y(0), z(0) {}
    Quat(float W, float X, float Y, float Z) : w(W), x(X), y(Y), z(Z) {}
};
Quat  operator*(const Quat& a, const Quat& b);
Quat  Conj(const Quat& q);
Quat  Normalize(const Quat& q);
Vec3  Rotate(const Quat& q, const Vec3& v);
Quat  FromTo(const Vec3& a, const Vec3& b);

enum RagType { RAG_HUMAN, RAG_QUAD };
enum RagPart { RP_PELVIS, RP_TORSO, RP_HEAD, RP_UARM, RP_FARM, RP_THIGH, RP_SHIN, RP_FOOT, RP_TAIL, RP_LEG };
static const int RAG_MAXB = 14, RAG_MAXJ = 14;

struct RBody {
    Vec3 x, xp, v, w; Quat q, qp;          // posicao do centro de massa, velocidade linear/angular, orientacao
    float invM; Vec3 invI;                 // inverso da massa; inverso da inercia (diagonal, no referencial local)
    float r, hl;                           // capsula: raio e meio-comprimento do segmento (eixo local Y)
    float sx, sz; int part; bool contact;  // sx,sz: achatamento so da malha de render
};
struct RJoint {
    short a, b; unsigned char type;        // 0 = esfera (cone+torcao), 1 = dobradica
    Vec3 la, lb;                           // ancoras (locais a cada corpo)
    Vec3 axA, axB, refA, refB;             // eixo de torcao/dobradica e vetor de referencia (locais)
    float swing, twist, lo, hi, compliance;
};

struct Ragdoll {
    int type, nb, nj; float scale, age, calmT, lastSpeed; bool asleep;
    RBody b[RAG_MAXB]; RJoint j[RAG_MAXJ];
    Ragdoll();
    void BuildHuman(const Vec3& feetPos, float yaw, float scale, const Vec3& vel);
    void BuildQuad(const Vec3& feetPos, float yaw, float scale, const Vec3& vel);
    void Impulse(const Vec3& at, const Vec3& dir, float force);     // empurrao linear + giro (braco de alavanca)
    void Step(float dt);
    Vec3 Pelvis() const;
    bool Finite() const;
    float MaxJointError() const;                                   // maior separacao (m) entre as duas ancoras de uma junta
    // construcao
    int  AddBody(const Vec3& A, const Vec3& B, float r, float mass, int part, float sx = 1, float sz = 1);   // A,B = extremos do osso (repouso)
    void AddBall(int a, int bb, const Vec3& anchor, const Vec3& twistDir, const Vec3& refDir, float swing, float twist, float compliance = 1e-6f);
    void AddHinge(int a, int bb, const Vec3& anchor, const Vec3& axis, const Vec3& refDir, float lo, float hi, float compliance = 1e-6f);
    Vec3 Tw(const Vec3& l) const;          // ponto local de repouso -> mundo
    Vec3 Dw(const Vec3& l) const;          // direcao local de repouso -> mundo
    Vec3 org; float yawR;
};
