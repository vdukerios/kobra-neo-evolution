#!/usr/bin/env python3
"""One-shot: python -I scripts/build_all.py [-j N] [--opt -O2]
   regenerates startup, recreates patched source copy (src/source), compiles, links, makes firmware.bin"""
import subprocess, sys
from pathlib import Path
R = Path(__file__).resolve().parent
for cmd in ([sys.executable, "-I", str(R / "gen_startup.py")], [sys.executable, "-I", str(R / "prepare_src.py")] + [a for i, a in enumerate(sys.argv) if a == "--variant" or (i and sys.argv[i-1] == "--variant")], [sys.executable, "-I", str(R / "build.py")] + sys.argv[1:]):
    if subprocess.run(cmd).returncode: sys.exit(1)
