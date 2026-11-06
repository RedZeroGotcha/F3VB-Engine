# Exporta funções e strings do programa aberto no Ghidra para um JSON LOCAL.
# @category F3VB
# NÃO TESTADO em uma instalação real do Ghidra: ajuste a API se sua versão reclamar.
# Rode com PyGhidra (Python 3, versões recentes do Ghidra) ou pelo Script Manager.
# A saída é derivada do F3.exe: guarde em private/ (ignorada pelo git), nunca commite.
import json

fm = currentProgram.getFunctionManager()
out = {"program": currentProgram.getName(), "functions": [], "strings": []}

for f in fm.getFunctions(True):
    out["functions"].append({
        "name": f.getName(),
        "addr": str(f.getEntryPoint()),
        "size": int(f.getBody().getNumAddresses()),
    })

try:
    from ghidra.program.util import DefinedDataIterator
    for d in DefinedDataIterator.definedStrings(currentProgram):
        out["strings"].append({
            "addr": str(d.getAddress()),
            "value": str(d.getDefaultValueRepresentation()),
        })
except Exception as exc:  # a API pode variar entre versões
    out["strings_error"] = str(exc)

target = askFile("Salvar JSON em (use private/)", "Salvar")
with open(target.getAbsolutePath(), "w") as fh:
    json.dump(out, fh, indent=2)
print("Exportado: %d funções, %d strings" % (len(out["functions"]), len(out["strings"])))
