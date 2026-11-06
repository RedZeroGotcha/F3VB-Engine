import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
VIEWER = ROOT / "viewer"


@unittest.skipUnless(shutil.which("g++"), "g++ não encontrado (teste do núcleo C++ do visualizador)")
class ViewerCoreTests(unittest.TestCase):
    def test_cpp_core_matches_python_reference(self):
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            subprocess.run(["python3" if shutil.which("python3") else "python", str(VIEWER / "tests" / "make_fixture.py"), str(t / "fx")],
                           check=True, capture_output=True)
            exe = t / "test_core"
            comp = subprocess.run(["g++", "-std=c++14", "-O1", "-Wall", "-Wextra", str(VIEWER / "src" / "f3m.cpp"), str(VIEWER / "src" / "scene.cpp"),
                                   str(VIEWER / "tests" / "test_core.cpp"), "-o", str(exe)], capture_output=True, text=True)
            self.assertEqual(comp.returncode, 0, comp.stderr)
            run = subprocess.run([str(exe), str(t / "fx")], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("viewer core tests OK", run.stdout)

    def test_cpp_v12_log_analysis_and_scale(self):
        """Log com carimbo, analise de modelo, estimativa de escala malha x esqueleto, Scene::mesh_scale, fuzz da analise."""
        src = VIEWER / "src"
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            subprocess.run(["python3" if shutil.which("python3") else "python", str(VIEWER / "tests" / "make_fixture.py"), str(t / "fx")],
                           check=True, capture_output=True)
            exe = t / "test_v12"
            comp = subprocess.run(["g++", "-std=c++14", "-O1", "-Wall", "-Wextra", str(src / "f3m.cpp"), str(src / "scene.cpp"), str(src / "analyze.cpp"),
                                   str(src / "log.cpp"), str(VIEWER / "tests" / "test_v12.cpp"), "-o", str(exe)], capture_output=True, text=True)
            self.assertEqual(comp.returncode, 0, comp.stderr)
            run = subprocess.run([str(exe), str(t / "fx")], capture_output=True, text=True, cwd=t)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("v12 core tests OK", run.stdout)

    def test_cpp_f3map_reader_and_log_prefix(self):
        """Leitor .f3map em C++ contra um .f3map gerado pelo Python (sintetico), mutacoes aleatorias e prefixo do log."""
        src = VIEWER / "src"
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            subprocess.run(["python3" if shutil.which("python3") else "python", str(VIEWER / "tests" / "make_map_fixture.py"), str(t / "mfx")],
                           check=True, capture_output=True)
            exe = t / "test_f3map"
            comp = subprocess.run(["g++", "-std=c++20", "-O1", "-Wall", "-Wextra", f"-I{src}", str(src / "f3map.cpp"), str(src / "f3tex.cpp"), str(src / "log.cpp"),
                                   str(VIEWER / "tests" / "test_f3map.cpp"), "-o", str(exe)], capture_output=True, text=True)
            self.assertEqual(comp.returncode, 0, comp.stderr)
            run = subprocess.run([str(exe), str(t / "mfx")], capture_output=True, text=True, cwd=t)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("f3map tests OK", run.stdout)

    def test_cpp_scripts_dialog_and_stf(self):
        """v17: biblioteca de scripts, .stf, sessao de dialogo e levantamento (scripts AMX sinteticos montados no proprio teste) + fuzz."""
        src = VIEWER / "src"
        root = VIEWER.parent
        with tempfile.TemporaryDirectory() as t:
            exe = Path(t) / "test_script"
            comp = subprocess.run(["g++", "-std=c++20", "-O1", "-Wall", "-Wextra", f"-I{src}", str(root / "src" / "amx" / "amx_vm.cpp"), str(root / "src" / "host" / "game_host.cpp"),
                                   str(src / "stf.cpp"), str(src / "script_world.cpp"), str(src / "f3map.cpp"), str(src / "log.cpp"), str(VIEWER / "tests" / "test_script.cpp"),
                                   "-o", str(exe)], capture_output=True, text=True)
            self.assertEqual(comp.returncode, 0, comp.stderr[-3000:])
            self.assertEqual(comp.stderr.strip(), "", comp.stderr[-2000:])
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("script tests OK", run.stdout)
            # a ferramenta de linha de comando (F3VBScript) sobre o script/STF de amostra gravados pelo proprio teste
            subprocess.run([str(exe), "--write-sample", str(Path(t) / "s.amx"), str(Path(t) / "s.stf")], check=True)
            tool = Path(t) / "f3vbscript"
            comp = subprocess.run(["g++", "-std=c++20", "-O1", "-Wall", "-Wextra", f"-I{src}", str(root / "src" / "amx" / "amx_vm.cpp"), str(root / "src" / "host" / "game_host.cpp"),
                                   str(src / "stf.cpp"), str(src / "script_world.cpp"), str(src / "f3map.cpp"), str(src / "log.cpp"), str(src / "script_main.cpp"), "-o", str(tool)],
                                  capture_output=True, text=True)
            self.assertEqual(comp.returncode, 0, comp.stderr[-3000:])
            r = subprocess.run([str(tool), str(Path(t) / "s.amx"), "--stf", str(Path(t) / "s.stf"), "--choices", "1,1"], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            # v19: ids do .stf sao 1-based (id 10 = entrada 9) e toda resposta registrada fica disponivel
            # (a condicao RCn e testada pela funcao de texto ANTES de chamar RegisterReply)
            self.assertIn("NPC: texto 9", r.stdout)
            self.assertIn("acao 'RC2_N1', destino 'RL2_N1'", r.stdout)
            self.assertNotIn("escondida", r.stdout)
            self.assertIn("(fim do dialogo)", r.stdout)
            self.assertIn("native sem implementar: PCSpeaker x1", r.stdout)
            r = subprocess.run([str(tool), str(Path(t) / "s.amx"), "--survey"], capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn("publics: 9 ok, 0 com erro", r.stdout)

    def test_cpp_ui(self):
        """v19: interface (TGA/BMP, .int, skin, TrueType, montagem da tela, clique, criacao de personagem), tudo sintetico."""
        src = VIEWER / "src"
        root = VIEWER.parent
        ui = ["image", "gui_int", "skin", "ttf", "gui", "ui_assets", "ui_raster", "chargen", "stf", "script_world", "f3m", "scene", "analyze", "log", "f3map", "f3tex", "vfs"]   # v20: ui_assets le direto da demo (vfs)
        with tempfile.TemporaryDirectory() as t:
            exe = Path(t) / "test_ui"
            comp = subprocess.run(["g++", "-std=c++20", "-O1", "-Wall", "-Wextra", f"-I{src}", str(VIEWER / "tests" / "test_ui.cpp"),
                                   str(root / "src" / "amx" / "amx_vm.cpp"), str(root / "src" / "host" / "game_host.cpp")] + [str(src / f"{n}.cpp") for n in ui] + ["-o", str(exe)],
                                  capture_output=True, text=True)
            self.assertEqual(comp.returncode, 0, comp.stderr[-3000:])
            self.assertEqual(comp.stderr.strip(), "", comp.stderr[-2000:])
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("ui tests OK", run.stdout)

    def test_game_main_syntax_windows(self):
        """v20: o jogo (F3VB.exe) e seus modulos Windows compilam com os headers REAIS do Windows (MinGW do zig: pip install ziglang).
        Os stubs de tests/stubs ficaram para tras no v20 (APIs W, seletor de pasta, MSAA): sem o zig o teste e pulado."""
        try:
            import ziglang  # noqa: F401
        except ImportError:
            self.skipTest("sem ziglang (pip install ziglang): o build.bat/MSVC e quem confere de verdade")
        src = VIEWER / "src"
        with tempfile.TemporaryDirectory() as t:   # o zig nao aceita -fsyntax-only: compila de verdade para um .o descartavel
            for name in ("game_main.cpp", "map_render.cpp", "gui_gl.cpp", "audio.cpp", "win_platform.cpp", "actor_gl.cpp", "video_player.cpp"):
                comp = subprocess.run([sys.executable, "-m", "ziglang", "c++", "-target", "x86_64-windows-gnu", "-std=c++20", "-c", f"-I{src}", str(src / name),
                                       "-o", str(Path(t) / (name + ".o"))], capture_output=True, text=True)
                self.assertEqual(comp.returncode, 0, name + ": " + comp.stderr[-3000:])
            comp = subprocess.run([sys.executable, "-m", "ziglang", "c++", "-target", "x86-windows-gnu", "-std=c++17", "-c", str(src / "video_helper32.cpp"),
                                   "-o", str(Path(t) / "vh32.o")], capture_output=True, text=True)
            self.assertEqual(comp.returncode, 0, "video_helper32.cpp: " + comp.stderr[-3000:])

    def test_mapviewer_main_syntax_with_stubs(self):
        src = VIEWER / "src"
        comp = subprocess.run(["g++", "-std=c++20", "-fsyntax-only", "-Wall", "-Wextra", f"-I{src}", f"-I{VIEWER / 'tests' / 'stubs'}", str(src / "mapviewer_main.cpp")],
                              capture_output=True, text=True)
        # v16: so o codigo de saida importa. O g++ do MinGW emite dezenas de KB de avisos por causa dos stubs de windows.h/GL contra os
        # headers reais dele; o MSVC compila o programa de verdade (build.bat). Os avisos sao impressos para quem quiser ver.
        self.assertEqual(comp.returncode, 0, comp.stderr[-3000:])
        if comp.stderr.strip():
            print(f"\n[aviso] g++ emitiu {len(comp.stderr)} bytes de avisos no teste de sintaxe do mapviewer (nao e falha)")

    def test_analyze_cli_builds(self):
        src = VIEWER / "src"
        with tempfile.TemporaryDirectory() as t:
            exe = Path(t) / "f3v_analyze"
            comp = subprocess.run(["g++", "-std=c++14", "-O1", "-Wall", "-Wextra", str(src / "analyze_main.cpp"), str(src / "analyze.cpp"), str(src / "scene.cpp"),
                                   str(src / "f3m.cpp"), "-o", str(exe)], capture_output=True, text=True)
            self.assertEqual(comp.returncode, 0, comp.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 2)                              # sem argumentos: imprime o uso
            self.assertIn("uso:", run.stderr)

    def test_viewer_main_syntax_with_stubs(self):
        comp = subprocess.run(["g++", "-std=c++14", "-fsyntax-only", "-Wall", "-Wextra", "-I", str(VIEWER / "tests" / "stubs"),
                               "-I", str(VIEWER / "src"), str(VIEWER / "src" / "viewer_main.cpp")], capture_output=True, text=True)
        self.assertEqual(comp.returncode, 0, comp.stderr)


if __name__ == "__main__":
    unittest.main()
