#!/usr/bin/env python3
"""Recreates gcc_build/src/source from the pristine checkout + patches/*.patch (read-only on the original)."""
import os, shutil, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
V = sys.argv[sys.argv.index("--variant") + 1] if "--variant" in sys.argv else "base"
ORIG = Path({"base": os.environ.get("KNE_BASE_DIR", str(ROOT.parent / "upstream_nuclear")) + "/source",
             "nuclear": str(ROOT.parent / "firmware" / "source")}[V])
SFX = "" if V == "base" else "_nuclear"
DST = ROOT / ("src" + SFX) / "source"
if DST.exists(): shutil.rmtree(DST)
shutil.copytree(ORIG, DST)
(ROOT / ("src" + SFX) / "workspace").mkdir(exist_ok=True)
for p in sorted((ROOT / "patches").glob("*.patch")):
    r = subprocess.run(["patch", "-p1", "-d", str(DST), "-i", str(p), "--binary", "-N"], capture_output=True, text=True)
    print(p.name, r.returncode, r.stdout.strip()[-200:], r.stderr.strip()[-200:])
    if r.returncode: sys.exit(1)
