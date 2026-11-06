"""v19: leitor do formato .int (layouts de tela da interface). Dados SINTETICOS montados pelo proprio escritor (build_int)."""
import random
import unittest

from f3vb.intfile import Fragment, IntError, Widget, build_int, describe, read_int


def frag(tex="", rect=(0, 0, 8, 8), flags=0, color=(128, 128, 128, 255), w=8, h=8):
    return Fragment(flags, w, h, 0, 0, 1, tex, 0.0, color, rect)


def sample_screen():
    """Janela com uma imagem que contem um botao e um rotulo; o rotulo tem um filho (caso real do Inventory)."""
    lbl_child = Widget(3, "lblInner", (10, 10, 50, 20), (0, 0, 40, 10), "zzmainmenu.ini", (128, 128, 128, 255), (0, 0, 0), 0,
                       fragments=[frag()] * 9, text_id=3, font="Fallouty", font_size=10, text_color=(0, 128, 0, 255), align=2)
    lbl = Widget(3, "lblTitle", (10, 10, 200, 40), (0, 0, 190, 30), "zzmainmenu.ini", (128, 128, 128, 255), (0, 0, 0), 0,
                 fragments=[frag()] * 9, children=[lbl_child], text_id=16, font="Gothic 821 Condensed BT", font_size=20,
                 text_color=(20, 78, 96, 255), align=1)
    btn = Widget(1, "QuitBtn", (27, 270, 211, 298), (-5, 242, 179, 270), "zzmainmenu.ini", (128, 128, 128, 0), (0, 0, 1), 1,
                 fragments=[frag("zzmainbutton-middle.tga", (27, 270, 211, 298), flags=3)] * 9,
                 sounds=("f3_button_down_01.wav", "f3_button_up_01.wav", "f3_button_up_01.wav"))
    pic = Widget(2, "Buttons", (24, 24, 216, 300), (24, 24, 216, 300), "zzmainmenu.ini", (128, 128, 128, 255), (0, 0, 1), 0,
                 fragments=[frag("f3_front_end_buttons.tga", (24, 24, 216, 300), w=192, h=276)] * 9, children=[btn, lbl],
                 tip_text=72, tip_texture="f3_tooltip_skills.tga")
    win = Widget(0, "MainmenuWin", (0, 0, 1024, 768), (0, 0, 1024, 768), "zzmainmenu.ini", (128, 128, 128, 0), (0, 0, 1), 1,
                 fragments=[frag("zzmain-middle.tga", (0, 0, 1024, 768), flags=3, w=32, h=32)] * 9, children=[pic])
    return win


class IntFileTests(unittest.TestCase):
    def test_roundtrip_v13(self):
        data = build_int("Teste.INT", [sample_screen()], 13)
        f = read_int(data)
        self.assertEqual((f.version, f.name, f.count()), (13, "Teste.INT", 5))
        self.assertEqual(build_int(f.name, f.roots, f.version), data)            # releitura e regravacao identicas, byte a byte
        names = [(d, w.name, w.type_name) for d, w in f.walk()]
        self.assertEqual(names, [(0, "MainmenuWin", "janela"), (1, "Buttons", "imagem"), (2, "QuitBtn", "botao"), (2, "lblTitle", "rotulo"), (3, "lblInner", "rotulo")])
        btn = f.find("quitbtn")
        self.assertEqual(btn.rect, (27, 270, 211, 298))
        self.assertEqual(btn.sounds[0], "f3_button_down_01.wav")
        self.assertEqual(btn.fragments[4].flags, 3)
        lbl = f.find("lblTitle")
        self.assertEqual((lbl.text_id, lbl.font, lbl.font_size, lbl.align, lbl.text_color), (16, "Gothic 821 Condensed BT", 20, 1, (20, 78, 96, 255)))
        self.assertEqual(f.find("lblInner").text_id, 3)                            # o filho e lido ANTES do sufixo do rotulo pai
        self.assertEqual(f.find("Buttons").tip_texture, "f3_tooltip_skills.tga")

    def test_versions_differ_where_measured(self):
        w = sample_screen()
        d12, d13, d11 = build_int("a", [w], 12), build_int("a", [w], 13), build_int("a", [w], 11)
        self.assertLess(len(d12), len(d13))                                        # V1.2: botao sem u8 e sem os 3 sons
        self.assertLess(len(d11), len(d12))                                        # V1.1: corpo sem os campos de dica
        for d, v in ((d11, 11), (d12, 12), (d13, 13)):
            f = read_int(d)
            self.assertEqual((f.version, f.count()), (v, 5))
        self.assertEqual(read_int(d12).find("QuitBtn").sounds, ())
        self.assertEqual(read_int(d11).find("Buttons").tip_texture, "")

    def test_errors_are_clean(self):
        good = build_int("a", [sample_screen()], 13)
        with self.assertRaises(IntError):
            read_int(b"XXXXXXXX" + good[8:])
        with self.assertRaises(IntError):
            read_int(good[:-3])                                                    # cortado no meio
        with self.assertRaises(IntError):
            read_int(good + b"\0")                                                 # byte sobrando
        with self.assertRaises(IntError):
            read_int(b"GUI V1.9" + good[8:])                                       # versao desconhecida

    def test_fuzz_never_crashes(self):
        good = build_int("a", [sample_screen()], 13)
        rnd = random.Random(19)
        ok = 0
        for _ in range(1500):
            b = bytearray(good)
            for _k in range(rnd.randint(1, 6)):
                b[rnd.randrange(len(b))] = rnd.randrange(256)
            try:
                read_int(bytes(b))
                ok += 1
            except IntError:
                pass
        self.assertGreater(ok, 0)

    def test_describe_with_stf(self):
        f = read_int(build_int("a", [sample_screen()], 13))
        texts = {16: "Quit", 3: "BODY"}
        lines = describe(f, lambda i: texts.get(i, ""))
        joined = "\n".join(lines)
        self.assertIn("rotulo lblTitle", joined)
        self.assertIn("'Quit'", joined)
        self.assertIn("botao QuitBtn [27,270 184x28]", joined)
        self.assertIn("dica#72 f3_tooltip_skills.tga", joined)


if __name__ == "__main__":
    unittest.main()
