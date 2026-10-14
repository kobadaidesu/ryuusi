#!/bin/zsh
# 静かな計測 (裏の負荷なし・電源接続) の手順。baseline と最終版を交互に走らせ、まとめて比べる。
#
#   bench/quiet_run.sh <baseline の作業ツリー> <作業用ディレクトリ>
#
# 前提 (このスクリプトはビルドしない):
#   - ./bin/particle_flow と ./bin/index.{html,js,wasm} が最終版 (make && make wasm)
#   - <baseline>/bin/particle_flow が baseline、./bin/before/index.{html,js,wasm} が baseline の wasm
#
# 順番:
#   1. native: baseline と最終版を交互に NATIVE_RUNS 回ずつ (既定 5)
#   2. native: ノイズフロア用に最終版を続けて NOISE_RUNS 回 (既定 5)
#   3. wasm: headless Chrome で baseline と最終版を交互に WASM_RUNS 回ずつ (既定 3)
#   4. bench/merge.py でまとめて bench/results/quiet-{baseline,final,noise}-{native,wasm}.json に保存し、
#      bench/compare.py の結果を <作業用ディレクトリ>/compare-*.md に出す
# 1 回ずつの結果と記録は <作業用ディレクトリ> に残る。
set -u
cd "${0:A:h}/.."

if [ $# -lt 2 ]; then
  sed -n '2,15p' "$0"
  exit 2
fi
BASE=${1:A}
WORK=${2:A}
NATIVE_RUNS=${NATIVE_RUNS:-5}
NOISE_RUNS=${NOISE_RUNS:-5}
WASM_RUNS=${WASM_RUNS:-3}
PORT=${PORT:-8126}
PREFIX=${PREFIX:-quiet}
CHROME=${CHROME:-"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"}

RAW=$WORK/raw
mkdir -p "$RAW"
LOG=$WORK/run.log
T0=$SECONDS
log() { print -r -- "[$(date +%H:%M:%S) +$(( SECONDS - T0 ))s] $*" | tee -a "$LOG"; }

# ---- 確認 ----
for f in "$BASE/bin/particle_flow" ./bin/particle_flow ./bin/index.wasm ./bin/before/index.wasm "$CHROME"; do
  [ -x "$f" ] || [ -f "$f" ] || { log "見つからない: $f"; exit 1; }
done
if lsof -nP -iTCP:$PORT -sTCP:LISTEN >/dev/null 2>&1; then
  log "port $PORT が使われている (PORT=... で変える)"; exit 1
fi
pmset -g batt | grep -q "AC Power" || log "注意: 電源につながっていない"
log "baseline: $BASE ($(git -C "$BASE" rev-parse --short HEAD))  final: $(git rev-parse --short HEAD)"
log "native ${NATIVE_RUNS}+${NATIVE_RUNS} 回, noise ${NOISE_RUNS} 回, wasm ${WASM_RUNS}+${WASM_RUNS} 回"
log "load: $(sysctl -n vm.loadavg)"

native() {  # native <バイナリ> <ラベル> <出力>
  if "$1" --bench --bench-label="$2" --bench-out="$3" >>"$WORK/native.log" 2>&1; then
    log "native $2 -> ${3:t}"
  else
    log "native $2 失敗 (native.log を見る)"; exit 1
  fi
}

# ---- 1. native: 交互に ----
for i in $(seq 1 $NATIVE_RUNS); do
  native "$BASE/bin/particle_flow" $PREFIX-baseline "$RAW/nat-baseline-$i.json"
  native ./bin/particle_flow       $PREFIX-final    "$RAW/nat-final-$i.json"
done

# ---- 2. native: ノイズフロア (最終版を続けて) ----
for i in $(seq 1 $NOISE_RUNS); do
  native ./bin/particle_flow $PREFIX-noise "$RAW/nat-noise-$i.json"
done

# ---- 3. wasm: headless Chrome で交互に ----
python3 bench/serve.py --port $PORT --out "$RAW" >"$WORK/serve.log" 2>&1 &
SERVE=$!
PROFILE=$(mktemp -d "${TMPDIR:-/tmp}/pf-chrome.XXXXXX")
cleanup() { kill $SERVE 2>/dev/null; wait $SERVE 2>/dev/null; rm -rf "$PROFILE"; }
trap cleanup EXIT
sleep 1

wasm() {  # wasm <パス (/ か /before/)> <ラベル> <出力>
  local label=$2 saved=$RAW/$2-wasm.json
  rm -f "$saved"
  "$CHROME" --headless=new --user-data-dir="$PROFILE" --window-size=1400,900 \
    "http://127.0.0.1:$PORT$1?bench=1&label=$label&report=/report" >>"$WORK/chrome.log" 2>&1 &
  local cp=$!
  for _ in $(seq 1 200); do
    sleep 3
    [ -f "$saved" ] && break
  done
  sleep 1
  kill $cp 2>/dev/null; wait $cp 2>/dev/null
  if [ -f "$saved" ]; then
    mv "$saved" "$3"
    log "wasm $label -> ${3:t} ($(python3 -c "import json,sys;print(json.load(open(sys.argv[1])).get('gpu_renderer'))" "$3"))"
  else
    log "wasm $label 失敗 (10 分待っても結果が来ない)"; exit 1
  fi
}

for i in $(seq 1 $WASM_RUNS); do
  wasm /before/ $PREFIX-baseline-$i "$RAW/wasm-baseline-$i.json"
  wasm /        $PREFIX-final-$i    "$RAW/wasm-final-$i.json"
done

# ---- 4. まとめと比較 ----
R=bench/results
python3 bench/merge.py $R/$PREFIX-baseline-native.json $PREFIX-baseline "$RAW"/nat-baseline-*.json | tee -a "$LOG"
python3 bench/merge.py $R/$PREFIX-final-native.json    $PREFIX-final    "$RAW"/nat-final-*.json    | tee -a "$LOG"
python3 bench/merge.py $R/$PREFIX-noise-native.json    $PREFIX-noise    "$RAW"/nat-noise-*.json    | tee -a "$LOG"
python3 bench/merge.py $R/$PREFIX-baseline-wasm.json   $PREFIX-baseline "$RAW"/wasm-baseline-*.json | tee -a "$LOG"
python3 bench/merge.py $R/$PREFIX-final-wasm.json      $PREFIX-final    "$RAW"/wasm-final-*.json    | tee -a "$LOG"

python3 bench/compare.py $R/$PREFIX-baseline-native.json $R/$PREFIX-final-native.json \
  --noise $R/$PREFIX-noise-native.json > "$WORK/compare-native.md"
python3 bench/compare.py $R/$PREFIX-final-native.json $R/$PREFIX-noise-native.json > "$WORK/compare-noise-native.md"
python3 bench/compare.py $R/$PREFIX-baseline-wasm.json $R/$PREFIX-final-wasm.json > "$WORK/compare-wasm.md"
log "load: $(sysctl -n vm.loadavg)"
log "終わり ($(( SECONDS - T0 )) 秒)。比較は $WORK/compare-*.md"
