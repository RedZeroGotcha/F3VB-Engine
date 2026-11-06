"""Gera docs/NATIVES.md (a API que o jogo oferece aos scripts) a partir do 03_report.json.

Uso:  py -3 scripts\\gerar_natives_md.py private\\relatorio\\03_report.json
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent

GRUPOS = [
    ("Ações de personagem (A_*)", lambda n: n.startswith("A_")),
    ("Diálogo e texto", lambda n: n in {
        "RegisterReply", "RegisterNodeText", "SetNodeTextFunction", "SetCurrentNodeName", "PCSpeaker",
        "FloatingSpeechText", "FloatingDamageText", "DisplayStringRef", "MessageGetArg", "AddEntry", "CloseTask"}),
    ("Biblioteca de ponto flutuante do Pawn", lambda n: n.startswith("float") or n in {"strfloat", "fround", "strlen"}),
    ("Leitura de estado (Get*, Is*, Has*, Find*, Distance...)", lambda n: n.startswith(
        ("Get", "Is", "Has", "Find", "Distance", "Myself", "Protagonist", "Hour", "Minute", "Day"))),
    ("Alteração de estado (Set*, Add*, Remove*, Create*, Destroy*...)", lambda n: n.startswith(
        ("Set", "Add", "Remove", "Create", "Destroy", "Attach", "Clear", "Equip", "Activate", "Deactivate", "Kill"))),
]


def main(path: str) -> None:
    rep = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    nat: dict = {}
    publics = set()
    ndec = ncand = 0
    for g in rep["grps"]:
        m = (g.get("aggregates") or {}).get("amx") or {}
        if m.get("candidates"):
            nat = m["natives_called"]
            publics = set(m.get("publics", []))
            ndec, ncand = m["decoded_ok"], m["candidates"]
    if not nat:
        sys.exit("Nenhum dado de AMX no relatório.")
    usados, linhas = set(), []
    linhas += ["# API de scripts (natives) do jogo", "",
               f"Fonte: relatório da demo real ({ndec}/{ncand} scripts decodificados por completo). "
               f"São **{len(nat)} funções nativas** que a engine precisa oferecer à VM; a coluna de chamadas é a contagem de "
               "pontos de chamada no código de todos os scripts (indica prioridade).", ""]
    for titulo, pred in GRUPOS:
        itens = [(n, c) for n, c in nat.items() if n not in usados and pred(n)]
        if not itens:
            continue
        usados.update(n for n, _ in itens)
        linhas += [f"## {titulo} ({len(itens)})", "", "| Função | Chamadas |", "|---|---:|"]
        linhas += [f"| `{n}` | {c} |" for n, c in sorted(itens, key=lambda x: -x[1])]
        linhas.append("")
    resto = [(n, c) for n, c in nat.items() if n not in usados]
    if resto:
        linhas += [f"## Outras ({len(resto)})", "", "| Função | Chamadas |", "|---|---:|"]
        linhas += [f"| `{n}` | {c} |" for n, c in sorted(resto, key=lambda x: -x[1])]
        linhas.append("")
    linhas += ["## Como implementar", "",
               "1. Registre cada função em `f3vb::amx::Machine::register_native(nome, callback)`; `params[0]` é o nº de bytes de argumentos e `params[1..]` os argumentos (inteiros de 32 bits; floats do Pawn são o mesmo padrão de bits).",
               "2. Comece pelas mais chamadas e pelo grupo de diálogo; as de ponto flutuante são triviais.",
               "3. Strings chegam como endereços na memória do script (`read_string`).", ""]
    destino = RAIZ / "docs" / "NATIVES.md"
    destino.write_text("\n".join(linhas), encoding="utf-8")
    print(f"{destino.name}: {len(nat)} natives, {len(publics)} publics distintas")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else str(RAIZ / "private" / "relatorio" / "03_report.json"))
