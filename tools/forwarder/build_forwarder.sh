#!/bin/bash
# NSP forwarder: a HOME-menu icon that starts sdmc:/switch/iceage2/iceage2.nro as an application
# (full memory, no title takeover).
#   KEYS=/path/to/prod.keys tools/forwarder/build_forwarder.sh   -> out/forwarder/*.nsp
# Needs devkitPro (devkitA64, libnx, switch-tools: nacptool), gcc, git, python3, and the
# prod.keys of your own console (dumped with Lockpick_RCM). Keys are only read, never copied.
# 1. nx-hbloader (switchbrew, ISC), patched to take the NRO path from its romfs
#    (hbl_forwarder_patch.py) and declared an Application (application_type 1)
# 2. hacBrewPack (The-4n, GPL-2.0; mirror rlaphoenix/hacBrewPack) packs exefs/romfs/control
set -euo pipefail
R="$(cd "$(dirname "$0")/../.." && pwd)"
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export PATH=$DEVKITPRO/devkitA64/bin:$DEVKITPRO/tools/bin:$PATH
KEYS=${KEYS:?set KEYS=/path/to/prod.keys}
TID=${TID:-01A6E2B0C0DE0000}
NAME=${NAME:-"Ice Age 2: The Meltdown"}
AUTHOR=${AUTHOR:-"Meluxz"}
VERSION=${VERSION:-"0.2.1"}
NRO=${NRO:-"sdmc:/switch/iceage2/iceage2.nro"}
ICON=${ICON:-$R/assets/icon.jpg}
F=${WORK:-$R/build/forwarder}
OUT=${OUT:-$R/out/forwarder}

[ -f "$KEYS" ] || { echo "no $KEYS"; exit 1; }
[ -f "$ICON" ] || { echo "no icon at $ICON (256x256 JPEG; ICON=...)"; exit 1; }
mkdir -p "$F" "$OUT"

echo "== sources"
[ -f $F/nx-hbloader/Makefile ] || { rm -rf $F/nx-hbloader; git clone --depth 1 https://github.com/switchbrew/nx-hbloader.git $F/nx-hbloader; }
[ -f $F/hacBrewPack/Makefile ] || { rm -rf $F/hacBrewPack; GIT_TERMINAL_PROMPT=0 git clone --depth 1 https://github.com/rlaphoenix/hacBrewPack.git $F/hacBrewPack; }

echo "== nx-hbloader ($TID)"
cd $F/nx-hbloader
sed -i -E "s/(\"title_id(_range_min|_range_max)?\": \")0x[0-9A-Fa-f]+/\10x$TID/" hbl.json
# hbl.json describes the album applet; installed, it must be an Application or the NRO's
# appletInitialize fails (2345-0031, LibnxError_InitFail_AM).
python3 - hbl.json <<'PY'
import json, sys
p = sys.argv[1]
j = json.load(open(p))
caps = [c for c in j["kernel_capabilities"] if c.get("type") != "application_type"]
caps.append({"type": "application_type", "value": 1})
j["kernel_capabilities"] = caps
json.dump(j, open(p, "w"), indent=4)
PY
python3 "$R/tools/forwarder/hbl_forwarder_patch.py" source/main.c
rm -rf build hbl.nso hbl.npdm hbl.elf
# make also tries to pack its own hbl.nsp (build_pfs0); only hbl.nso and hbl.npdm matter.
make -j4 > $F/hbl-build.log 2>&1 || true
[ -f hbl.nso ] && [ -f hbl.npdm ] || { tail -20 $F/hbl-build.log; exit 1; }

echo "== hacBrewPack"
cd $F/hacBrewPack
[ -f hacbrewpack ] || { [ -f config.mk ] || cp config.mk.template config.mk 2>/dev/null || true; make -j4 > $F/hbp-build.log 2>&1 || { tail -20 $F/hbp-build.log; exit 1; }; }

echo "== NSP"
W=$F/work; rm -rf $W; mkdir -p $W/exefs $W/romfs $W/control $W/nsp
cp $F/nx-hbloader/hbl.nso $W/exefs/main
cp $F/nx-hbloader/hbl.npdm $W/exefs/main.npdm
printf '%s' "$NRO" > $W/romfs/nextNroPath
printf '%s' "$NRO" > $W/romfs/nextArgv
nacptool --create "$NAME" "$AUTHOR" "$VERSION" $W/control/control.nacp --titleid=$TID
cp "$ICON" $W/control/icon_AmericanEnglish.dat
cd $W
$F/hacBrewPack/hacbrewpack -k "$KEYS" --titleid $TID --exefsdir exefs --romfsdir romfs \
  --controldir control --nspdir nsp --nologo
cp nsp/*.nsp "$OUT/IceAge2.Meltdown.Xbox.recomp.$TID.nsp"   # hacbrewpack names it in lower case
ls -la "$OUT"/*.nsp
echo "== done: install it with DBI"
