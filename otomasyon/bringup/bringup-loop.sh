#!/usr/bin/env bash
set -uo pipefail
BRANCH="os/c1-region-backing"; URL="https://github.com/msyrt-sys/kemgu.git"
REPO="$(git rev-parse --show-toplevel)"; VDIR="/tmp/kemgu-bringup-dogrula"; MAX_ITER=6
FLAGS="--permission-mode acceptEdits --output-format json"; D="otomasyon/bringup"
bildir(){ printf '\a[BRINGUP] %s\n' "$1"; command -v notify-send >/dev/null 2>&1 && notify-send "KEMGU" "$1"; }
cd "$REPO"
for ((i=1;i<=MAX_ITER;i++)); do
  git checkout -q "$BRANCH" && git pull -q --ff-only origin "$BRANCH"
  [ "$(git rev-parse HEAD)" = "$(git rev-parse origin/$BRANCH)" ] || { bildir "HEAD sapması — DUR"; exit 1; }
  line="$(grep -vE '^\s*#' "$D/TASKS.txt" | sed -n '1p')"
  id="$(echo "$line" | cut -d'|' -f1 | xargs)"; marker="$(echo "$line" | cut -d'|' -f2 | xargs)"
  [ "$id" = "STOP-FAZ-A" ] && { bildir "Mekanik kuyruk bitti → FAZ-A (insan+stratejist). Loop durdu."; exit 0; }
  echo "=== TUR $i — görev $id ==="
  claude -p $FLAGS "$(cat "$D/PROMPT_EXECUTOR.md")
Güncel görev: $id | beklenen marker: $marker" | jq -r '.result' || true
  grep -qE '🔴|DUR:' "$D/KOMUTA.md" && { bildir "DUR/🔴 — insan kararı ($id)"; exit 2; }
  rm -rf "$VDIR"; git clone -q "$URL" "$VDIR"
  ( cd "$VDIR" && git checkout -q "$BRANCH" && bash "$D/gate.sh" "$id" "$marker" ); GATE=$?
  if [ $GATE -eq 0 ]; then
    sed -i "\|^$id[[:space:]]|d" "$D/TASKS.txt"
    git add "$D/TASKS.txt" && git commit -q -m "bringup: $id yeşil ($marker)" && git push -q origin "$BRANCH"
    bildir "$id YEŞİL (taze-clone gate exit 0, $marker) → sıradaki"
  else
    bildir "$id KIRMIZI (gate exit $GATE) — Executor gate'i geçemedi/aşırı-iddia. DUR."; exit 3
  fi
done
bildir "MAX_ITER doldu — elle bak."
