#!/usr/bin/env python3
"""Build Kobra Neo firmware (Marlin / HC32F460) with Arm GNU Toolchain.
Usage:  python -I scripts/build.py [-j N] [--clean] [--opt -O2]
Reads file list / defines / include paths from the Keil project (uvprojx) of
the pristine checkout, compiles the patched COPY under gcc_build/src."""
import re, os, sys, subprocess, concurrent.futures, argparse, shutil, time, xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent            # gcc_build
VARIANTS = {"base": ("base_original", ""), "nuclear": ("nuclear", "_nuclear")}
VARIANT = sys.argv[sys.argv.index("--variant") + 1] if "--variant" in sys.argv else "base"
sys.argv = [a for i, a in enumerate(sys.argv) if a != "--variant" and not (i and sys.argv[i-1] == "--variant")]
SFX = VARIANTS[VARIANT][1]
CHECKOUT = {"base": os.environ.get("KNE_BASE_DIR", str(ROOT.parent / "upstream_nuclear")),   # your own clone of NuclearPhoenixx/Kobra_Neo (unmodified)
            "nuclear": str(ROOT.parent / "firmware")}[VARIANT]                                    # the KNE firmware in this repository
UVPROJ = Path(CHECKOUT) / "workspace" / "anycubic.uvprojx"   # file list / defines / includes come from here (read-only)
SRC = ROOT / ("src" + SFX)                               # patched copy ("source" + dummy "workspace")
WS = SRC / "workspace"
OBJ = ROOT / ("obj" + SFX)
OUT = ROOT / ("out" + SFX)
TC = ROOT / "toolchain" / "bin"
EXE = ".exe" if os.name == "nt" else ""
GCC, GXX, OBJCOPY, SIZE = [str(TC / (t + EXE)) for t in ("arm-none-eabi-gcc", "arm-none-eabi-g++", "arm-none-eabi-objcopy", "arm-none-eabi-size")]

ap = argparse.ArgumentParser()
ap.add_argument("-j", type=int, default=os.cpu_count() or 4)
ap.add_argument("--clean", action="store_true")
ap.add_argument("--opt", default="-O2")
ap.add_argument("--only", nargs="*", help="compile only sources whose name contains one of these (no link)")
args = ap.parse_args()

def resolve(p):
    return (WS / p.replace("\\", "/")).resolve()

tree = ET.parse(UVPROJ)
tgt = tree.getroot().find(".//Target")
cads = tgt.find(".//Cads/VariousControls")
defines = [d.strip() for d in cads.findtext("Define").replace(",", " ").split() if d.strip()]
incs = [resolve(i) for i in cads.findtext("IncludePath").split(";") if i.strip()]
# __CC_ARM/__TARGET_FPU_VFP are ARMCC-only markers; GCC provides its own (see patches/)
defines = [d for d in defines if d not in ("__CC_ARM", "__TARGET_FPU_VFP")]

sources = []
for g in tgt.iter("Group"):
    for f in g.iter("File"):
        fp = f.findtext("FilePath"); ft = f.findtext("FileType")
        if ft in ("1", "8") and fp.lower().endswith((".c", ".cpp")):
            if f.findtext(".//IncludeInBuild") == "0": continue
            sources.append(resolve(fp))
# project-specific extras for GCC
extras = [ROOT / "gcc_support" / "startup_hc32f460.S", ROOT / "gcc_support" / "syscalls.c", ROOT / "gcc_support" / "randomseed.cpp"]
sources += extras
names = {}
for s in sources:
    names.setdefault(s.name.lower(), []).append(s)
dups = {k: v for k, v in names.items() if len(v) > 1}
if dups: print("ERROR duplicate basenames:", dups); sys.exit(2)

CPU = ["-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=hard", "-mfpu=fpv4-sp-d16"]
COMMON = CPU + [args.opt, "-ffunction-sections", "-fdata-sections", "-w", "-fno-common", "-fno-strict-aliasing", "-ffp-contract=off", "-MMD", "-MP"] \
    + ["-D" + d for d in defines] + ["-D__nop()=__asm volatile (\"nop\")"] + ["-I" + str(i) for i in incs if i.exists()] + ["-I" + str(ROOT / "gcc_support"), "-I" + str(ROOT / "gcc_support" / "cmsis")]
CFLAGS = COMMON + ["-std=gnu11"]
CXXFLAGS = COMMON + ["-std=gnu++11", "-fno-exceptions", "-fno-rtti", "-fno-threadsafe-statics", "-fno-use-cxa-atexit"]
ASFLAGS = CPU + ["-x", "assembler-with-cpp"]

if args.clean and OBJ.exists(): shutil.rmtree(OBJ)
OBJ.mkdir(exist_ok=True); (ROOT / "logs").mkdir(exist_ok=True)

def up_to_date(src, o):
    if not o.exists(): return False
    t = o.stat().st_mtime
    if src.stat().st_mtime > t: return False
    d = o.with_suffix(".d")
    if d.exists():
        txt = d.read_text(errors="replace").split(": ", 1)[-1].replace(chr(92)+chr(10), " ").replace(chr(92)+" ", chr(0))
        for dep in txt.split():
            dep = dep.replace(chr(0), " ")
            if os.path.exists(dep) and os.path.getmtime(dep) > t: return False
    return True

def compile_one(src):
    o = OBJ / (src.stem + ".o")
    if up_to_date(src, o): return src, 0, ""
    ext = src.suffix.lower()
    if ext == ".cpp": cmd = [GXX] + CXXFLAGS
    elif ext == ".c" and src.name != "syscalls.c": cmd = [GXX] + CXXFLAGS + ["-x", "c++"]   # Keil: --cpp11 in MiscControls compiles .c as C++
    elif ext == ".c": cmd = [GCC] + CFLAGS
    else: cmd = [GCC] + ASFLAGS
    cmd += ["-c", str(src), "-o", str(o)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    return src, r.returncode, r.stdout + r.stderr

SRCALL = sources
if args.only: sources = [x for x in sources if any(o in x.name for o in args.only)]
t0 = time.time(); fails = []
(ROOT/"logs"/"compile_errors.log").write_text("")
with concurrent.futures.ThreadPoolExecutor(args.j) as ex:
    for src, rc, msg in ex.map(compile_one, sources):
        if rc:
            fails.append(src); print("FAIL", src.name); print(msg[:700])
            with open(ROOT / "logs" / "compile_errors.log", "a") as lf: lf.write("=== " + str(src) + chr(10) + msg + chr(10))
print(f"compiled {len(sources)} units, {len(fails)} failed, {time.time()-t0:.0f}s")
if fails: sys.exit(1)
if args.only: sys.exit(0)
sources = SRCALL

OUT.mkdir(exist_ok=True)
objs = [str(OBJ / (s.stem + ".o")) for s in sources]
(OUT / "objs.rsp").write_text(chr(10).join(chr(34) + o.replace(chr(92), "/") + chr(34) for o in objs))
link = [GXX] + CPU + [args.opt, "--specs=nano.specs", "--specs=nosys.specs", "-nostartfiles",
        "-T", str(ROOT / "gcc_support" / "hc32f460.ld"), "-Wl,--gc-sections", "-Wl,--print-memory-usage",
        "-Wl,-Map=" + str(OUT / "firmware.map"), "-Wl,--cref", "-Wl,--no-warn-rwx-segments",
        "-u", "_printf_float", "-u", "_scanf_float",
        "@" + str(OUT / "objs.rsp"), "-lm", "-o", str(OUT / "firmware.elf")]
r = subprocess.run(link, capture_output=True, text=True)
(ROOT / "logs" / "link.log").write_text(r.stdout + r.stderr)
print((r.stdout + r.stderr)[-4000:])
if r.returncode: sys.exit(3)
subprocess.run([OBJCOPY, "-O", "binary", str(OUT / "firmware.elf"), str(OUT / "firmware.bin")], check=True)
subprocess.run([SIZE, str(OUT / "firmware.elf")])
print("bin bytes:", (OUT / "firmware.bin").stat().st_size)
# Optional: image padded with zeros to a multiple of 512 bytes (the stock 1.3.3 .bin is 512-aligned with 3 trailing zero bytes;
# probably what bin_expand.exe does - deduced, never executed). Keil-published firmware.bin is NOT padded.
b = (OUT / "firmware.bin").read_bytes()
(OUT / "firmware_pad512.bin").write_bytes(b + b"\x00" * (-len(b) % 512))
