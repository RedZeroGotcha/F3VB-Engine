"""v13: a segunda engine (engine_test/rpggl) compila e passa nos testes headless do proprio autor (sem janela, sem Windows)."""
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

RPGGL = Path(__file__).resolve().parents[3] / "engine_test" / "rpggl"


@unittest.skipUnless(shutil.which("g++"), "g++ nao encontrado")
class RpgglHeadlessTests(unittest.TestCase):
    @unittest.skipIf(sys.platform == "win32", "o headless.cpp do RPGGL usa POSIX (setenv/mkdtemp): rode no Linux/WSL/macOS; no Windows o g++ do MinGW nao tem essas funcoes")
    def test_headless_suite_passes(self):
        src = RPGGL / "src"
        files = [str(RPGGL / "tests" / "headless.cpp"), str(RPGGL / "tests" / "stub_audio.cpp")] + [str(src / f) for f in
                 ("world.cpp", "ragdoll.cpp", "game.cpp", "save.cpp", "ui.cpp")]
        with tempfile.TemporaryDirectory() as t:
            exe = Path(t) / "rpggl_headless"
            comp = subprocess.run(["g++", "-std=c++14", "-O1", "-pthread", f"-I{RPGGL / 'tests' / 'stubs'}", f"-I{src}", *files, "-o", str(exe)],
                                  capture_output=True, text=True)
            self.assertEqual(comp.returncode, 0, comp.stderr[-2000:])
            run = subprocess.run([str(exe)], capture_output=True, text=True, cwd=t, timeout=300)
            self.assertEqual(run.returncode, 0, run.stdout[-2000:] + run.stderr[-500:])
            self.assertIn("TUDO OK (0 falhas)", run.stdout)

    def test_rpggl_has_no_game_assets_or_third_party(self):
        bad = [p.name for p in RPGGL.rglob("*") if p.is_file() and p.suffix.lower() in {".wav", ".ogg", ".mp3", ".tga", ".png", ".bmp", ".b3d", ".gr2", ".grp"}]
        self.assertEqual(bad, [])
        text = " ".join(p.read_text(encoding="utf-8", errors="ignore") for p in (RPGGL / "src").glob("*.[ch]*"))
        for dep in ("stb_image", "glad", "glfw", "SDL_", "miniaudio", "btBulletDynamics", "JPH/"):
            self.assertNotIn(dep, text)


if __name__ == "__main__":
    unittest.main()
