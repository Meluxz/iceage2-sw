#!/bin/bash
# Ice Age 2: default.xbe -> C with the vendored xboxrecomp (~20-30 min).
#   tools/regen.sh                      -> reads game/default.xbe
#   XBE=/path/to/default.xbe tools/regen.sh
#   PYTHON=/path/to/python tools/regen.sh   (needs capstone: pip install capstone)
# Outputs: build/analysis/{xbe,disasm,func_id,abi} and the generated C in src/recomp/gen.
# The translator lifts the hardware register accesses of the linked XDK libraries
# (--mmio-sections) so the runtime routes them to its device models (APU, USB, NV2A).
# Library sections (D3D, DSOUND, XMV, ...) are disassembled too: XMV is linked statically and
# only reachable through them. DOLBY is left out: it holds code for the APU's DSP, not x86.
set -euo pipefail
R="$(cd "$(dirname "$0")/.." && pwd)"
TK="$R/xboxrecomp"
PY="${PYTHON:-python3}"
XBE="${XBE:-$R/game/default.xbe}"
SEEDS="${SEEDS:-$R/config/seed_functions.json}"   # entry points reached only through pointers
A="${A:-$R/build/analysis}"
GEN="${GEN:-$R/src/recomp/gen}"
MANUAL="$R/src/recomp_manual.c"
SPLIT="${SPLIT:-250}"
LIB_SECTIONS="D3DX,XMV,D3D,XGRPH,DSOUND,XPP"
MMIO_SECTIONS="DSOUND,XPP,D3D,XGRPH"
export PYTHONUNBUFFERED=1

[ -f "$XBE" ] || { echo "no $XBE: extract your disc to game/ first (docs/BUILDING.md)"; exit 1; }
"$PY" -c "import capstone" 2>/dev/null || { echo "$PY has no capstone: pip install capstone"; exit 1; }
mkdir -p "$A/xbe" "$A/disasm" "$A/func_id" "$A/abi" "$GEN"
cd "$TK"

step() { echo; echo "===== $1  $(date +%T)"; }
t0=$SECONDS

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
"$PY" -m tools.recomp "$XBE" --game-name "Ice Age 2" --all --split "$SPLIT" --gen-dir "$GEN" \
  --disasm-dir "$A/disasm" --func-id-dir "$A/func_id" --abi-dir "$A/abi" \
  --exclude-manual "$MANUAL" --mmio-sections "$MMIO_SECTIONS"

step done
echo "total $(( (SECONDS - t0) / 60 )) min"
ls "$GEN" | wc -l
du -sh "$GEN"
