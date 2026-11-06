"""F3VB Extractor: interface simples para extrair TODOS os arquivos dos .grp da demo (tamanho real).

Como usar: dê dois cliques neste arquivo (ou rode `py -3 f3vb_extractor_gui.pyw`), escolha a pasta da demo
(onde ficam os .grp e o resource.rht), escolha a pasta de destino e clique em "Extrair".
Requer apenas o Python do Windows (tkinter já vem junto). O conteúdo extraído pertence ao jogo: não publique.
"""
import json
import os
import subprocess
import sys
import traceback
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

try:
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk
except Exception:                                    # tkinter ausente
    print("tkinter não encontrado. Reinstale o Python marcando 'tcl/tk and IDLE', ou use a linha de comando:\n"
          "  py -3 -m f3vb extract-all PASTA_DA_DEMO PASTA_DESTINO")
    raise SystemExit(1)

from f3vb.extractor import ExtractJob, ExtractOptions, fmt_bytes, plan
from f3vb.layout import AssetsLayout, default_assets_dir

CONFIG = Path.home() / ".f3vb_extractor.json"


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("F3VB Extractor — demo Van Buren")
        self.geometry("820x640")
        self.minsize(720, 560)
        self.job = None
        self.cfg = self._load_cfg()
        self.v_game = tk.StringVar(value=self.cfg.get("game", ""))
        self.v_out = tk.StringVar(value=self.cfg.get("assets", str(default_assets_dir())))
        self.v_raw = tk.BooleanVar(value=self.cfg.get("raw", True))
        self.v_png = tk.BooleanVar(value=self.cfg.get("png", True))
        self.v_glb = tk.BooleanVar(value=self.cfg.get("glb", False))
        self.v_f3m = tk.BooleanVar(value=self.cfg.get("f3m", True))
        self.v_anims = tk.IntVar(value=self.cfg.get("anims", 8))
        self._build()
        self.protocol("WM_DELETE_WINDOW", self._close)

    # ---------------------------------------------------------------- interface
    def _build(self):
        pad = {"padx": 8, "pady": 4}
        f = ttk.LabelFrame(self, text="1) Pasta da demo (com os .grp e o resource.rht)")
        f.pack(fill="x", **pad)
        ttk.Entry(f, textvariable=self.v_game).pack(side="left", fill="x", expand=True, padx=6, pady=6)
        ttk.Button(f, text="Procurar...", command=lambda: self._pick(self.v_game)).pack(side="left", padx=6)

        f = ttk.LabelFrame(self, text="2) Pasta original_assets (RETAIL = originais extraídos, CONVERTED = convertidos)")
        f.pack(fill="x", **pad)
        ttk.Entry(f, textvariable=self.v_out).pack(side="left", fill="x", expand=True, padx=6, pady=6)
        ttk.Button(f, text="Procurar...", command=lambda: self._pick(self.v_out)).pack(side="left", padx=6)

        f = ttk.LabelFrame(self, text="3) O que extrair")
        f.pack(fill="x", **pad)
        ttk.Checkbutton(f, text="Extrair os originais do jogo para RETAIL (desmarque para só reconverter a partir do RETAIL já extraído)", variable=self.v_raw).pack(anchor="w", padx=6)
        ttk.Checkbutton(f, text="Texturas convertidas para PNG", variable=self.v_png).pack(anchor="w", padx=6)
        ttk.Checkbutton(f, text="Modelos para o VISUALIZADOR F3VB (.f3m) + esqueletos com animações", variable=self.v_f3m).pack(anchor="w", padx=6)
        ttk.Checkbutton(f, text="Modelos para o Blender (.glb)", variable=self.v_glb).pack(anchor="w", padx=6)
        row = ttk.Frame(f)
        row.pack(anchor="w", padx=6, pady=2)
        ttk.Label(row, text="Animações por modelo (0 = todas):").pack(side="left")
        ttk.Spinbox(row, from_=0, to=999, width=5, textvariable=self.v_anims).pack(side="left", padx=6)

        bar = ttk.Frame(self)
        bar.pack(fill="x", **pad)
        self.b_plan = ttk.Button(bar, text="Analisar (tamanho e espaço)", command=self.on_plan)
        self.b_plan.pack(side="left", padx=4)
        self.b_go = ttk.Button(bar, text="EXTRAIR", command=self.on_extract)
        self.b_go.pack(side="left", padx=4)
        self.b_cancel = ttk.Button(bar, text="Cancelar", command=self.on_cancel, state="disabled")
        self.b_cancel.pack(side="left", padx=4)
        ttk.Button(bar, text="Abrir pasta de destino", command=self.on_open_out).pack(side="left", padx=4)
        ttk.Button(bar, text="Abrir visualizador", command=self.on_viewer).pack(side="left", padx=4)

        self.progress = ttk.Progressbar(self, mode="determinate")
        self.progress.pack(fill="x", **pad)
        self.lbl = ttk.Label(self, text="Pronto.")
        self.lbl.pack(anchor="w", padx=10)
        box = ttk.Frame(self)
        box.pack(fill="both", expand=True, **pad)
        self.log = tk.Text(box, height=14, wrap="word", state="disabled")
        sb = ttk.Scrollbar(box, command=self.log.yview)
        self.log.configure(yscrollcommand=sb.set)
        self.log.pack(side="left", fill="both", expand=True)
        sb.pack(side="left", fill="y")

    # ---------------------------------------------------------------- utilidades
    def _pick(self, var):
        d = filedialog.askdirectory(initialdir=var.get() or str(Path.home()), title="Escolha a pasta")
        if d:
            var.set(d)

    def _say(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def _opts(self):
        try:
            anims = max(0, int(self.v_anims.get()))
        except (ValueError, tk.TclError):
            anims = 8
        return ExtractOptions(raw=self.v_raw.get(), png=self.v_png.get(), glb=self.v_glb.get(), f3m=self.v_f3m.get(),
                              anims_per_model=anims)

    def _validate(self):
        if not self.v_raw.get():                       # só reconverter: não precisa da pasta do jogo
            out = self.v_out.get().strip()
            if not out or not AssetsLayout(out).index_csv.exists():
                messagebox.showerror("Destino", "Não há RETAIL/index.csv nessa pasta. Marque a extração dos originais uma vez.")
                return None
            return Path(self.v_game.get().strip() or "."), Path(out)
        game = Path(self.v_game.get().strip())
        if not game.is_dir():
            messagebox.showerror("Pasta da demo", "Escolha a pasta da demo (a que tem os .grp e o resource.rht).")
            return None
        if not next(game.rglob("resource.rht"), None):
            messagebox.showerror("Pasta da demo", "Não encontrei o resource.rht dentro dessa pasta.")
            return None
        out = self.v_out.get().strip()
        if not out:
            messagebox.showerror("Destino", "Escolha a pasta de destino.")
            return None
        return game, Path(out)

    def _load_cfg(self):
        try:
            return json.loads(CONFIG.read_text(encoding="utf-8"))
        except Exception:
            return {}

    def _save_cfg(self):
        try:
            CONFIG.write_text(json.dumps({"game": self.v_game.get(), "assets": self.v_out.get(), "raw": self.v_raw.get(),
                                          "png": self.v_png.get(), "glb": self.v_glb.get(), "f3m": self.v_f3m.get(),
                                          "anims": self.v_anims.get()}), encoding="utf-8")
        except Exception:
            pass

    # ---------------------------------------------------------------- ações
    def on_plan(self):
        if not self.v_raw.get():
            self._say("Sem extração dos originais: só vou reconverter a partir do RETAIL já extraído (nada a planejar).")
            return
        v = self._validate()
        if not v:
            return
        game, out = v
        self._say("Analisando...")
        self.update_idletasks()
        try:
            p = plan(game)
            import shutil
            probe = out if out.exists() else out.parent if out.parent.exists() else Path.home()
            free = shutil.disk_usage(probe).free
            need = p["bytes"] * (1 + (0.6 if self.v_png.get() else 0) + (0.2 if (self.v_f3m.get() or self.v_glb.get()) else 0))
            self._say(f"{p['files']} arquivos, {fmt_bytes(p['bytes'])} (tamanho real). Estimativa com conversões: {fmt_bytes(need)}.")
            self._say(f"Espaço livre no destino: {fmt_bytes(free)}" + ("  >>> ESPAÇO INSUFICIENTE <<<" if free < need * 1.1 else "  (ok)"))
            top = sorted(p["by_type"].items(), key=lambda kv: -kv[1][1])[:8]
            self._say("Maiores tipos: " + ", ".join(f"{k} {v[0]}x {fmt_bytes(v[1])}" for k, v in top))
        except Exception:
            self._say("ERRO ao analisar:\n" + traceback.format_exc())

    def on_extract(self):
        v = self._validate()
        if not v or (self.job and self.job.alive()):
            return
        game, out = v
        opts = self._opts()
        if not (opts.raw or opts.png or opts.glb or opts.f3m):
            messagebox.showinfo("Nada a fazer", "Marque pelo menos uma opção em '3) O que extrair'.")
            return
        self._save_cfg()
        self._say(f"Extraindo de {game} para {out} ...")
        self.b_go.configure(state="disabled")
        self.b_cancel.configure(state="normal")
        self.progress.configure(value=0, maximum=100)
        self.job = ExtractJob(game, out, opts)
        self.job.start()
        self.after(100, self._poll)

    def on_cancel(self):
        if self.job:
            self.job.cancel()
            self._say("Cancelando (termina o arquivo atual)...")

    def _poll(self):
        last = None
        try:
            while True:
                m = self.job.q.get_nowait()
                if m[0] == "progress":
                    last = m
                elif m[0] == "done":
                    self._finish()
                    return
        except Exception:
            pass
        if last:
            _, d, t, msg = last
            self.progress.configure(maximum=max(t, 1), value=d)
            self.lbl.configure(text=f"{d}/{t}  {msg}")
        self.after(100, self._poll)

    def _finish(self):
        self.b_go.configure(state="normal")
        self.b_cancel.configure(state="disabled")
        if self.job.error:
            self._say("ERRO: " + self.job.error)
            return
        r = self.job.result
        self.progress.configure(value=self.progress.cget("maximum"))
        self._say(f"{'CANCELADO' if r['cancelled'] else 'CONCLUÍDO'} em {r['seconds']} s: {r['written']} arquivos ({fmt_bytes(r['bytes'])}), "
                  f"PNG {r['png']}, F3M {r['f3m_models']} modelos / {r['f3m_skeletons']} esqueletos, GLB {r['glb']}, "
                  f"falhas {len(r['failures'])}.")
        for line in r["failures"][:10]:
            self._say("  falha: " + line)
        self._say("Detalhes em EXTRACAO_RELATORIO.txt e index.csv dentro de original_assets. Os originais ficam em RETAIL e os convertidos em CONVERTED.")

    def on_open_out(self):
        p = Path(self.v_out.get())
        if p.exists():
            os.startfile(str(p)) if hasattr(os, "startfile") else subprocess.Popen(["xdg-open", str(p)])

    def on_viewer(self):
        here = Path(__file__).resolve().parent
        for cand in (here.parent.parent / "viewer" / "build" / "Release" / "F3VBViewer.exe",
                     here.parent.parent / "viewer" / "build" / "F3VBViewer.exe",
                     here.parent.parent / "viewer" / "F3VBViewer.exe"):
            if cand.exists():
                out = Path(self.v_out.get()) / "CONVERTED" / "f3m" / "models"
                args = [str(cand)]
                subprocess.Popen(args, cwd=str(out if out.exists() else cand.parent))
                return
        messagebox.showinfo("Visualizador", "F3VBViewer.exe não encontrado. Compile-o com viewer\\build.bat (veja docs\\VISUALIZADOR.md).")

    def _close(self):
        self._save_cfg()
        self.destroy()


if __name__ == "__main__":
    try:
        App().mainloop()
    except Exception:
        traceback.print_exc()
        input("Erro. Pressione Enter para fechar...")
