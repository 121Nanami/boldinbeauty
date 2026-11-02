#!/bin/bash
# usage: tools/snapall.sh <times> id...   -> snapshots in parallel (3 lanes), prints contact sheet paths
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; T="$1"; shift
printf "%s\n" "$@" | xargs -P 3 -I{} sh -c "\"$ROOT/tools/snap.sh\" {} $T | tail -1"
