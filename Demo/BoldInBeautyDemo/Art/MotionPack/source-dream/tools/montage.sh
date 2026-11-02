#!/bin/bash
# usage: tools/montage.sh <out.jpg> <cols>x<rows> id...   tiles each piece's latest snapshot contact sheet (needs tools/snap.sh first)
SP=/private/tmp/claude-501/-Users-local-user-Downloads-motion-studio/33278d3f-d7a2-4ff7-8126-cd3b3201afec/scratchpad
out="$1"; grid="$2"; shift 2; tmp=$(mktemp -d "$SP/mont-XXXX"); i=0
for id in "$@"; do i=$((i+1)); n=$(printf "%02d" $i)
  ffmpeg -loglevel error -y -i "$SP/snap-$id/contact-sheet.jpg" -vf "scale=900:-2,pad=900:ih:0:0:color=0x1b1b1b" "$tmp/$n.jpg"; done
ffmpeg -loglevel error -y -framerate 1 -i "$tmp/%02d.jpg" -vf "tile=$grid:padding=6:color=0x333333" -frames:v 1 "$out" && echo "$out"
