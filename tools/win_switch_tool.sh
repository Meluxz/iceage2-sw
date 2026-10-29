#!/bin/bash
# Run a Windows switch-tools exe (C:\devkitPro\tools\bin\<name>.exe) from WSL, where there
# is no switch-tools package: absolute Linux paths in the arguments go through wslpath.
# Installed as /root/sw-deps/bin/nacptool and elf2nro (symlinks to this file).
name=$(basename "$0")
args=()
for a in "$@"; do
  case "$a" in
    /*)    args+=("$(wslpath -w "$a")") ;;
    --*=/*) args+=("${a%%=*}=$(wslpath -w "${a#*=}")") ;;
    *)     args+=("$a") ;;
  esac
done
exec "/mnt/c/devkitPro/tools/bin/$name.exe" "${args[@]}"
