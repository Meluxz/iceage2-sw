#!/bin/bash
# Ice Age 2: XBE -> C with xboxrecomp (Git Bash on Windows, Python from ../venv).
# Toolkit: xboxrecomp-b3, the copy vendored in burnout3-sw (MIT): its translator lifts the hardware
# register accesses of the linked XDK libraries (--mmio-sections) so the runtime routes them to
# device models (APU, USB, NV2A) without trapping faults -- upstream v0.16.0 cannot do that on
# x86-64 Linux, and the title faulted on the APU in DirectSoundCreate (logs/run05, run06).
# Outputs: $A/{xbe,disasm,func_id,abi}, generated C in iceage2-pc/src/recomp/gen.
# Library sections (D3D, DSOUND, XMV, ...) are disassembled too: XMV is linked statically and only
# reachable from game code through those sections. DOLBY is left out: it is marked executable but
# holds code for the APU's DSP, not x86.
#   tools/regen.sh                 -> full pipeline
#   SEEDS=path.json tools/regen.sh -> another --seed-functions file (default: config/seed_functions.json)
#   TK=xboxrecomp A=analysis tools/regen.sh -> the upstream toolkit and its old outputs
set -euo pipefail
R=/d/nx/IceAge2_Switch
TK="$R/${TK:-xboxrecomp-b3}"
SEEDS="${SEEDS:-$R/config/seed_functions.json}"   # entry points reached only through pointers
LIB_SECTIONS="D3DX,XMV,D3D,XGRPH,DSOUND,XPP"
MMIO_SECTIONS="DSOUND,XPP,D3D,XGRPH"               # as burnout3-sw's tools/regen.sh
PY="$R/venv/Scripts/python.exe"
export PYTHONUNBUFFERED=1   # a pipe to the log is block-buffered otherwise: the log lags minutes behind
XBE="$R/game/default.xbe"
A="$R/${A:-analysis-b3}"
GEN="$R/iceage2-pc/src/recomp/gen"
MANUAL="$R/iceage2-pc/src/recomp_manual.c"
SPLIT="${SPLIT:-250}"
mkdir -p "$A/xbe" "$A/disasm" "$A/func_id" "$A/abi" "$GEN"
cd "$TK"

step() { echo; echo "===== $1  $(date +%T)"; }
t0=$SECONDS
echo "toolkit: $TK"

step xbe_parser
"$PY" -m tools.xbe_parser "$XBE" --json "$A/xbe/default_analysis.json" --quiet

step disasm
"$PY" -m tools.disasm "$XBE" --analysis-json "$A/xbe/default_analysis.json" -o "$A/disasm" --force -v \
  --text-only --extra-sections "$LIB_SECTIONS" --seed-functions "$SEEDS"

step func_id
"$PY" -m tools.func_id "$XBE" --functions "$A/disasm/functions.json" --strings "$A/disasm/strings.json" \
  --xrefs "$A/disasm/xrefs.json" -o "$A/func_id" -v

step abi_analysis
"$PY" -m tools.abi_analysis "$XBE" --disasm-dir "$A/disasm" --func-id-dir "$A/func_id" --output-dir "$A/abi" -v

step recomp
rm -f "$GEN"/*.c "$GEN"/*.h   # recomp_types.h too: it is written only when absent
mmio=()
[ "$TK" != "$R/xboxrecomp" ] && mmio=(--mmio-sections "$MMIO_SECTIONS")
"$PY" -m tools.recomp "$XBE" --game-name "Ice Age 2" --all --split "$SPLIT" --gen-dir "$GEN" \
  --disasm-dir "$A/disasm" --func-id-dir "$A/func_id" --abi-dir "$A/abi" \
  --exclude-manual "$MANUAL" "${mmio[@]}"

step done
echo "total $(( (SECONDS - t0) / 60 )) min"
ls "$GEN" | wc -l
du -sh "$GEN"
