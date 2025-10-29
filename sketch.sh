#!/usr/bin/env zsh
set -e

# 기본값
PORT=""
BAUD="115200"
FQBN="esp32:esp32:esp32s3"      # 필요 시 :USBMode=default 등 확장
SCHEME_NAME="default"           # Arduino Tools > Partition Scheme 이름(확장자X)
SKETCH_DIR="$(pwd)"

# 인자 파싱
while getopts "p:b:f:S:d:" opt; do
  case "$opt" in
    p) PORT="$OPTARG" ;;
    b) BAUD="$OPTARG" ;;
    f) FQBN="$OPTARG" ;;
    S) SCHEME_NAME="$OPTARG" ;;  # ex) default, huge_app
    d) SKETCH_DIR="$OPTARG" ;;
  esac
done

if [ -z "$PORT" ]; then
  echo "Usage: sh sketch.sh -p /dev/cu.usbmodemXXXX [-b 115200] [-f <FQBN>] [-S <scheme_name>] [-d <sketch_dir>]"
  exit 1
fi

# 툴 체크
command -v arduino-cli >/dev/null 2>&1 || { echo "arduino-cli가 필요합니다."; exit 1; }

# 빌드/임시 폴더 고정
BUILD_DIR="$SKETCH_DIR/_arduino_build"
TMPDIR="$SKETCH_DIR/_arduino_tmp"
mkdir -p "$BUILD_DIR" "$TMPDIR"
export TMPDIR

echo "[sketch.sh] 컴파일..."
arduino-cli compile \
  --fqbn "$FQBN" \
  --build-property "upload.speed=$BAUD" \
  --build-property "build.partitions=$SCHEME_NAME" \
  --build-path "$BUILD_DIR" \
  --verbose \
  "$SKETCH_DIR"

# 산출물 찾기
APP_BIN="$(find "$BUILD_DIR" -type f -name '*.ino.bin' -print -quit)"
[ -f "$APP_BIN" ] || { echo "[sketch.sh] .ino.bin 못찾음 (BUILD_DIR=$BUILD_DIR)"; exit 1; }

echo "[sketch.sh] 업로드: $PORT"
arduino-cli upload -p "$PORT" --fqbn "$FQBN" --input-file "$APP_BIN" --verify

echo "[sketch.sh] 완료"
