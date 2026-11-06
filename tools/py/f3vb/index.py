"""Índice de recursos da demo: resolve NOME -> bytes usando resource.rht + .grp (sem extrair nada para o disco)."""
from __future__ import annotations

from pathlib import Path

from .catalog import find_grps
from .grp import GrpFile
from .rht import read_rht


class ResourceIndex:
    def __init__(self, game_dir, rht_path=None):
        root = Path(game_dir)
        rp = Path(rht_path) if rht_path else next(root.rglob("resource.rht"), None)
        if not rp:
            raise FileNotFoundError("resource.rht não encontrado")
        self.rht = read_rht(rp)
        self.grps, _dups = find_grps(root)
        self._open: dict = {}
        self.by_name: dict = {}                    # (tipo, nome minúsculo) -> (pack, número)
        self.entries: dict = {}                    # tipo -> [(nome, pack, número)]
        for e in self.rht.entries:
            nm = self.rht.resource_name(e.offset)
            if nm is None:
                continue
            pack = self.rht.pack_label(e.pack)
            self.by_name.setdefault((e.type_name, nm.lower()), (pack, e.number))
            self.entries.setdefault(e.type_name, []).append((nm, pack, e.number))

    def _grp(self, pack: str) -> GrpFile:
        g = self._open.get(pack)
        if g is None:
            path = self.grps.get(pack.lower())
            if path is None:
                raise FileNotFoundError(f".grp do pack '{pack}' não encontrado")
            g = self._open[pack] = GrpFile(path)
        return g

    def read(self, pack: str, number: int) -> bytes:
        return self._grp(pack).read(number)

    def find(self, name: str, type_name: str):
        return self.by_name.get((type_name, name.lower()))

    def read_named(self, name: str, type_name: str) -> bytes | None:
        hit = self.find(name, type_name)
        return self.read(*hit) if hit else None

    def search(self, type_name: str, prefix: str = "") -> list:
        p = prefix.lower()
        return sorted(e for e in self.entries.get(type_name, []) if e[0].lower().startswith(p))

    def close(self):
        for g in self._open.values():
            g.close()
        self._open.clear()

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()


class RetailIndex:
    """Mesma interface do ResourceIndex (read / read_named / search), mas lendo os ARQUIVOS da pasta RETAIL.

    Serve para converter (png/f3m/glb) sem precisar dos .grp: `rows` sao as linhas do index.csv
    (pack, numero, tipo, nome, caminho relativo a RETAIL, tamanho).
    """

    def __init__(self, retail_dir, rows):
        self.retail = Path(retail_dir)
        self.rows = list(rows)
        self._path: dict = {}
        self.by_name: dict = {}
        self.entries: dict = {}
        for pack, number, type_name, name, rel, _size in self.rows:
            self._path[(pack.lower(), int(number))] = rel
            self.by_name.setdefault((type_name, name.lower()), (pack, int(number)))
            self.entries.setdefault(type_name, []).append((name, pack, int(number)))

    @classmethod
    def from_csv(cls, assets_root):
        import csv
        root = Path(assets_root)
        with open(root / "index.csv", newline="", encoding="utf-8") as f:
            rd = csv.reader(f)
            next(rd, None)
            rows = [(r[0], int(r[1]), r[2], r[3], r[4], int(r[5])) for r in rd if len(r) >= 6]
        return cls(root / "RETAIL", rows)

    def read(self, pack: str, number: int) -> bytes:
        rel = self._path.get((pack.lower(), int(number)))
        if rel is None:
            raise FileNotFoundError(f"{pack}#{number} nao esta no index.csv")
        return (self.retail / rel).read_bytes()

    def find(self, name: str, type_name: str):
        return self.by_name.get((type_name, name.lower()))

    def read_named(self, name: str, type_name: str) -> bytes | None:
        hit = self.find(name, type_name)
        return self.read(*hit) if hit else None

    def search(self, type_name: str, prefix: str = "") -> list:
        p = prefix.lower()
        return sorted(e for e in self.entries.get(type_name, []) if e[0].lower().startswith(p))

    def close(self):
        pass

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self.close()
