import json
import struct
import tempfile
import unittest
from pathlib import Path

from f3vb.amxdis import OPCODES, analyze_amx, disassemble
from f3vb.cli import main as cli_main


def build_amx(ins, publics=(("main", 0),), natives=("print",), defsize=40, data=b"\0" * 16):
    """Monta um AMX file_version 6: ins = lista de células (inteiros) do código."""
    pub_tbl = b"".join(struct.pack("<I", addr * 4) + n.encode().ljust(defsize - 4, b"\0") for n, addr in publics)
    nat_tbl = b"".join(struct.pack("<I", 0) + n.encode().ljust(defsize - 4, b"\0") for n in natives)
    code = b"".join(struct.pack("<i", c) for c in ins)
    publics_off = 52
    natives_off = publics_off + len(pub_tbl)
    libs_off = natives_off + len(nat_tbl)
    cod = libs_off
    dat = cod + len(code)
    hea = dat + len(data)
    stp = hea + 4096
    hdr = struct.pack("<iHbbhh", hea, 0xF1E0, 6, 6, 0, defsize)
    hdr += struct.pack("<10i", cod, dat, hea, stp, -1, publics_off, natives_off, libs_off, libs_off, libs_off)
    return hdr + pub_tbl + nat_tbl + code + data


OP = {n: i for i, (n, _) in enumerate(OPCODES)}


class AmxDisTests(unittest.TestCase):
    def test_table_size_and_key_opcodes(self):
        self.assertEqual(len(OPCODES), 138)
        self.assertEqual(OP["HALT"], 120)
        self.assertEqual(OP["PROC"], 46)
        self.assertEqual(OP["SYSREQ.C"], 123)
        self.assertEqual(OP["PUSH.R"], 38)

    def program(self):
        # cel0: HALT 0 (retorno para 0) ; main: PROC, PUSH.C 5, PUSH.C 4, SYSREQ.C 0, STACK 8, ZERO.pri, RETN
        code = [OP["HALT"], 0,
                OP["PROC"], OP["PUSH.C"], 5, OP["PUSH.C"], 4, OP["SYSREQ.C"], 0, OP["STACK"], 8,
                OP["ZERO.pri"], OP["RETN"]]
        return build_amx(code, publics=(("main", 2),))

    def test_analyze_ok(self):
        r = analyze_amx(self.program())
        self.assertTrue(r["ok"], r["errors"])
        self.assertEqual(r["natives_called"], {"print": 1})
        self.assertEqual(r["publics"], ["main"])
        self.assertIn("SYSREQ.C    0  ; print", disassemble(self.program()))

    def test_analyze_errors(self):
        self.assertFalse(analyze_amx(b"\0" * 10)["ok"])
        bad_op = build_amx([OP["HALT"], 0, 999], publics=(("main", 0),))
        self.assertFalse(analyze_amx(bad_op)["ok"])
        not_proc = build_amx([OP["HALT"], 0, OP["NOP"], OP["RETN"]], publics=(("main", 2),))
        r = analyze_amx(not_proc)
        self.assertFalse(r["ok"])
        self.assertTrue(any("PROC" in e for e in r["errors"]))
        v11 = bytearray(self.program())
        v11[6] = 11
        self.assertFalse(analyze_amx(bytes(v11))["ok"])

    def test_cli(self):
        with tempfile.TemporaryDirectory() as t:
            p = Path(t) / "x.amx"
            p.write_bytes(self.program())
            cli_main(["amxdis", str(p)])
            cli_main(["amxdis", str(p), "--check"])


if __name__ == "__main__":
    unittest.main()
