#!/bin/bash
# renders every BB Dream deliverable at 4K / 13 fps in 3 lanes; log in out/render_all.log
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"
sweet=$(ls src | grep '^ui-' | grep -v linger | sed 's/.html//'); linger=$(ls src | grep -- '-linger.html' | sed 's/.html//')
{ for i in $(for j in $sweet tr1 tr2 tr3 tr4 tr5 $linger; do [ -f out/alpha/$j.mov ] || echo $j; done) sheet-01 sheet-02 sheet-03 sheet-04 sheet-05 sheet-06 sheet-07 sheet-08 tr-show; do echo $i; done; } |
  xargs -P 3 -I{} "$ROOT/tools/render.sh" {} >> "$ROOT/out/render_all.log" 2>&1
echo "ALL DONE $(grep -c '^OK' out/render_all.log) ok, $(grep -c -E '^(FAIL|BUILD FAIL)' out/render_all.log) failed" >> out/render_all.log
