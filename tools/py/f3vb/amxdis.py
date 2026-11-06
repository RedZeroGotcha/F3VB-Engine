"""Desmontador e verificador de scripts AMX no instruction set CLÁSSICO (Pawn/Small file_version 6).

A tabela de opcodes foi VALIDADA nos scripts reais da demo: só esta numeração decodifica o código inteiro
(todas as publics caem em PROC, todos os saltos em início de instrução, todo SYSREQ.C dentro da tabela de
natives). O Pawn 4.x não roda estes arquivos (exige file_version >= 11 e mudou o conjunto de instruções).
"""
from __future__ import annotations

import struct
from collections import Counter

# (nome, nº de parâmetros); -1 = FILE/SYMBOL (size + payload), -2 = CASETBL (num, default, pares)
OPCODES = [
    ("NONE", 0), ("LOAD.pri", 1), ("LOAD.alt", 1), ("LOAD.S.pri", 1), ("LOAD.S.alt", 1), ("LREF.pri", 1),
    ("LREF.alt", 1), ("LREF.S.pri", 1), ("LREF.S.alt", 1), ("LOAD.I", 0), ("LODB.I", 1), ("CONST.pri", 1),
    ("CONST.alt", 1), ("ADDR.pri", 1), ("ADDR.alt", 1), ("STOR.pri", 1), ("STOR.alt", 1), ("STOR.S.pri", 1),
    ("STOR.S.alt", 1), ("SREF.pri", 1), ("SREF.alt", 1), ("SREF.S.pri", 1), ("SREF.S.alt", 1), ("STOR.I", 0),
    ("STRB.I", 1), ("LIDX", 0), ("LIDX.B", 1), ("IDXADDR", 0), ("IDXADDR.B", 1), ("ALIGN.pri", 1),
    ("ALIGN.alt", 1), ("LCTRL", 1), ("SCTRL", 1), ("MOVE.pri", 0), ("MOVE.alt", 0), ("XCHG", 0),
    ("PUSH.pri", 0), ("PUSH.alt", 0), ("PUSH.R", 1), ("PUSH.C", 1), ("PUSH", 1), ("PUSH.S", 1), ("POP.pri", 0),
    ("POP.alt", 0), ("STACK", 1), ("HEAP", 1), ("PROC", 0), ("RET", 0), ("RETN", 0), ("CALL", 1),
    ("CALL.pri", 0), ("JUMP", 1), ("JREL", 1), ("JZER", 1), ("JNZ", 1), ("JEQ", 1), ("JNEQ", 1), ("JLESS", 1),
    ("JLEQ", 1), ("JGRTR", 1), ("JGEQ", 1), ("JSLESS", 1), ("JSLEQ", 1), ("JSGRTR", 1), ("JSGEQ", 1),
    ("SHL", 0), ("SHR", 0), ("SSHR", 0), ("SHL.C.pri", 1), ("SHL.C.alt", 1), ("SHR.C.pri", 1),
    ("SHR.C.alt", 1), ("SMUL", 0), ("SDIV", 0), ("SDIV.alt", 0), ("UMUL", 0), ("UDIV", 0), ("UDIV.alt", 0),
    ("ADD", 0), ("SUB", 0), ("SUB.alt", 0), ("AND", 0), ("OR", 0), ("XOR", 0), ("NOT", 0), ("NEG", 0),
    ("INVERT", 0), ("ADD.C", 1), ("SMUL.C", 1), ("ZERO.pri", 0), ("ZERO.alt", 0), ("ZERO", 1), ("ZERO.S", 1),
    ("SIGN.pri", 0), ("SIGN.alt", 0), ("EQ", 0), ("NEQ", 0), ("LESS", 0), ("LEQ", 0), ("GRTR", 0), ("GEQ", 0),
    ("SLESS", 0), ("SLEQ", 0), ("SGRTR", 0), ("SGEQ", 0), ("EQ.C.pri", 1), ("EQ.C.alt", 1), ("INC.pri", 0),
    ("INC.alt", 0), ("INC", 1), ("INC.S", 1), ("INC.I", 0), ("DEC.pri", 0), ("DEC.alt", 0), ("DEC", 1),
    ("DEC.S", 1), ("DEC.I", 0), ("MOVS", 1), ("CMPS", 1), ("FILL", 1), ("HALT", 1), ("BOUNDS", 1),
    ("SYSREQ.pri", 0), ("SYSREQ.C", 1), ("FILE", -1), ("LINE", 2), ("SYMBOL", -1), ("SRANGE", 2),
    ("JUMP.pri", 0), ("SWITCH", 1), ("CASETBL", -2), ("SWAP.pri", 0), ("SWAP.alt", 0), ("PUSH.ADR", 1),
    ("NOP", 0), ("SYSREQ.D", 1), ("SYMTAG", 1), ("BREAK", 0),
]
JUMPS = {"JUMP", "JZER", "JNZ", "JEQ", "JNEQ", "JLESS", "JLEQ", "JGRTR", "JGEQ", "JSLESS", "JSLEQ", "JSGRTR",
         "JSGEQ", "CALL", "SWITCH"}


class AmxError(ValueError):
    pass


def parse_header(d: bytes) -> dict:
    if len(d) < 52:
        raise AmxError(f"{len(d)} bytes, menor que o cabeçalho (52)")
    size, magic, fv, av, flags, defsize = struct.unpack_from("<iHbbhh", d, 0)
    cod, dat, hea, stp, cip, pub, nat, lib, pv, tags = struct.unpack_from("<10i", d, 12)
    return dict(size=size, magic=magic, file_version=fv, amx_version=av, flags=flags, defsize=defsize, cod=cod,
                dat=dat, hea=hea, stp=stp, cip=cip, publics=pub, natives=nat, libraries=lib, pubvars=pv, tags=tags)


def _stubs(d: bytes, h: dict, start: int, end: int) -> list[tuple[int, str]]:
    out = []
    for off in range(start, end, h["defsize"]):
        addr = struct.unpack_from("<I", d, off)[0]
        out.append((addr, d[off + 4:off + h["defsize"]].split(b"\0", 1)[0].decode("latin-1")))
    return out


PUSHES = {"PUSH.C", "PUSH.pri", "PUSH.alt", "PUSH", "PUSH.S", "PUSH.ADR", "PUSH.R"}


def native_calls(ins, natives) -> list[tuple[str, int, list]]:
    """Para cada SYSREQ.C: (nome, nº de argumentos, descrição de cada argumento).
    O compilador empilha os argumentos da direita para a esquerda e por último o nº de bytes (PUSH.C n*4);
    voltando pelas instruções PUSH* achamos cada argumento (constante ou "variável")."""
    out = []
    for k, (c, n, p) in enumerate(ins):
        if n != "SYSREQ.C" or not 0 <= p[0] < len(natives):
            continue
        j = k - 1
        while j >= 0 and ins[j][1] not in PUSHES:
            j -= 1
        if j < 0 or ins[j][1] != "PUSH.C" or ins[j][2][0] % 4:
            out.append((natives[p[0]][1], -1, []))
            continue
        nargs = ins[j][2][0] // 4
        args, jj = [], j - 1
        while len(args) < nargs and jj >= 0:
            if ins[jj][1] in PUSHES:
                args.append(("C", ins[jj][2][0]) if ins[jj][1] == "PUSH.C" else ("V", ins[jj][1]))
            elif ins[jj][1] in ("PROC", "CALL", "SYSREQ.C", "RETN", "RET", "HALT", "JUMP", "CASETBL"):
                break
            jj -= 1
        out.append((natives[p[0]][1], nargs, args[::-1]))
    return out


def analyze_amx(d: bytes) -> dict:
    """Decodifica o código inteiro e valida. Nunca levanta exceção: devolve `ok` e `errors`."""
    res: dict = {"ok": False, "errors": []}
    try:
        h = parse_header(d)
    except AmxError as exc:
        res["errors"].append(str(exc))
        return res
    res["header"] = {k: h[k] for k in ("file_version", "amx_version", "flags", "defsize", "magic")}
    if h["magic"] != 0xF1E0 or h["file_version"] != 6 or h["defsize"] <= 4:
        res["errors"].append(f"cabeçalho fora do esperado (magic={h['magic']:#x}, file_version={h['file_version']})")
        return res
    try:
        publics = _stubs(d, h, h["publics"], h["natives"])
        natives = _stubs(d, h, h["natives"], h["libraries"])
        code = d[h["cod"]:h["dat"]]
    except (struct.error, ValueError) as exc:
        res["errors"].append(f"tabelas inválidas: {exc}")
        return res
    cells = len(code) // 4
    i = 0
    ins: list[tuple[int, str, list[int]]] = []
    starts = set()
    while i < cells:
        starts.add(i)
        op = struct.unpack_from("<i", code, 4 * i)[0]
        if not 0 <= op < len(OPCODES):
            res["errors"].append(f"opcode inválido {op} na célula {i}")
            break
        name, np = OPCODES[op]
        if np == -1:
            sz = struct.unpack_from("<i", code, 4 * (i + 1))[0] if i + 1 < cells else -1
            if sz < 0 or sz % 4:
                res["errors"].append(f"{name} com size inválido na célula {i}")
                break
            np = 1 + sz // 4
        elif np == -2:
            n = struct.unpack_from("<i", code, 4 * (i + 1))[0] if i + 1 < cells else -1
            if not 0 <= n <= 100000:
                res["errors"].append(f"CASETBL absurdo na célula {i}")
                break
            np = 2 + 2 * n
        if i + 1 + np > cells:
            res["errors"].append(f"{name} ultrapassa o código na célula {i}")
            break
        ins.append((i, name, list(struct.unpack_from(f"<{np}i", code, 4 * (i + 1)))))
        i += 1 + np
    else:
        pass
    by_cell = {c: (n, p) for c, n, p in ins}
    bad_pub = [nm for a, nm in publics if (a // 4) not in by_cell or by_cell[a // 4][0] != "PROC"]
    bad_jmp = sum(1 for c, n, p in ins if n in JUMPS and (p[0] % 4 or (p[0] // 4) not in starts))
    sysreq = [p[0] for c, n, p in ins if n == "SYSREQ.C"]
    bad_sys = sum(1 for x in sysreq if not 0 <= x < len(natives))
    called = Counter(natives[x][1] for x in sysreq if 0 <= x < len(natives))
    if bad_pub:
        res["errors"].append(f"{len(bad_pub)} public(s) não apontam para PROC")
    if bad_jmp:
        res["errors"].append(f"{bad_jmp} salto(s) para fora de início de instrução")
    if bad_sys:
        res["errors"].append(f"{bad_sys} SYSREQ.C fora da tabela de natives")
    calls = native_calls(ins, natives)
    arity: dict = {}
    consts: dict = {}
    for nm, na, args in calls:
        arity.setdefault(nm, Counter())[na] += 1
        for pos, (kind, val) in enumerate(args):
            if kind == "C":
                consts.setdefault(nm, {}).setdefault(pos, Counter())[val] += 1
    res["native_arity"] = {k: dict(v) for k, v in arity.items()}
    res["native_const_args"] = {k: {pos: dict(c.most_common(4)) for pos, c in v.items()} for k, v in consts.items()}
    res.update(ok=not res["errors"], instructions=len(ins), publics=[n for _, n in publics],
               natives_imported=[n for _, n in natives], natives_called=dict(called),
               opcodes=dict(Counter(n for _, n, _ in ins)), code_cells=cells, data_bytes=h["hea"] - h["dat"],
               has_main=h["cip"] != -1, debug_flag=bool(h["flags"] & 2))
    res["_ins"] = ins
    res["_natives"] = natives
    res["_publics"] = publics
    return res


def disassemble(d: bytes) -> str:
    """Texto legível do código do script (para estudar a lógica; saída é derivada do jogo: não publique)."""
    r = analyze_amx(d)
    if "_ins" not in r:
        return "ERRO: " + "; ".join(r["errors"])
    pub_at = {a // 4: n for a, n in r["_publics"]}
    natives = r["_natives"]
    L = []
    for c, name, p in r["_ins"]:
        if c in pub_at:
            L.append(f"\n; public {pub_at[c]}")
        op = ""
        if name == "SYSREQ.C" and 0 <= p[0] < len(natives):
            op = f"{p[0]}  ; {natives[p[0]][1]}"
        elif name in ("FILE", "SYMBOL"):
            payload = struct.pack(f"<{len(p) - 1}i", *p[1:]) if len(p) > 1 else b""
            op = repr(payload.split(b"\0", 1)[0].decode("latin-1"))
        elif name == "CASETBL":
            op = f"{p[0]} casos, default {p[1]}"
        else:
            op = " ".join(str(x) for x in p)
        L.append(f"{c:6d}  {name:<11} {op}")
    return "\n".join(L)
