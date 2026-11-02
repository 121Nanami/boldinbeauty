#!/bin/zsh
# Transcodes the bb-Showreel videos (owner 2026-10-09) to game-friendly H.264 / AAC files in Content/Movies (git-ignored; staged as
# non-UFS files for the media player and the startup movie). Usage: Tools/motion_movies.sh [path/to/bb-Showreel]
set -e
DEMO="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${1:-$HOME/Downloads/motion-studio/videos/bb-Showreel}"
OUT="$DEMO/Content/Movies"
mkdir -p "$OUT"
enc() {  # in out height video-kbps
  [ -f "$2" ] && [ "$2" -nt "$1" ] && { echo "up to date: $(basename "$2")"; return; }
  ffmpeg -v error -y -i "$1" -vf "scale=-2:$3,fps=30" -c:v libx264 -profile:v high -pix_fmt yuv420p -b:v "${4}k" -maxrate "$(( $4 * 2 ))k" -bufsize "$(( $4 * 4 ))k" \
    -movflags +faststart -af "volume=0.4" -c:a aac -b:a 128k -ac 2 "$2"   # owner 2026-10-09: music 60% quieter at baseline
  echo "$(basename "$2") $(du -h "$2" | cut -f1)"
}
enc "$SRC/04_Download_Trailer_4K/BB_Download_Trailer_2min_4K.mp4" "$OUT/BB_Download_Trailer.mp4" 1080 6000
for f in "$SRC"/01_In-Game_Ads_4K/BB_Ad0*_4K.mp4; do n=$(basename "$f" _4K.mp4); enc "$f" "$OUT/$n.mp4" 720 3000; done
for f in "$SRC"/03_UI_Pack_Sheets_4K/BB_UI_Sheet_0*_4K.mp4; do n=$(basename "$f" _4K.mp4); enc "$f" "$OUT/$n.mp4" 720 1500; done
enc "$SRC/02_Page_Transitions_4K/BB_Transitions_Showcase_4K.mp4" "$OUT/BB_Transitions_Showcase.mp4" 720 2000
enc "$SRC/BoldInBeauty_bb-Showreel_Master_4K.mp4" "$OUT/BB_Showreel_Master.mp4" 720 2000
# Set 2 (2026-10-09): the VFX / missing-screens sheets and the Set 2 master reel, as they are rendered (skipped until they exist)
for f in "$SRC"/05_Set2_Sheets_4K/BB_Set2_*_4K.mp4(N); do n=$(basename "$f" _4K.mp4); enc "$f" "$OUT/$n.mp4" 720 1500; done
[ -f "$SRC/BoldInBeauty_bb-Showreel_Set2_Master_4K.mp4" ] && enc "$SRC/BoldInBeauty_bb-Showreel_Set2_Master_4K.mp4" "$OUT/BB_Showreel_Set2_Master.mp4" 720 2000
du -sh "$OUT"
