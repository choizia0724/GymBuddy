#!/usr/bin/env zsh
set -euo pipefail

# ===== User args =====
PORT="/dev/tty.usbmodem5AE60253511"
BAUD="115200"
FQBN="esp32:esp32:esp32s3"           # 필요 시 :USBMode=default,CDCOnBoot=default 등 추가
SCHEME_FILE="default.csv"            # CSV 파일명 (오탈자 수정!)
SKETCH_DIR="$(pwd)"                  # .ino가 있는 폴더


# ===== 환경 준비 =====
mkdir -p ./_arduino_build ./_arduino_tmp
mkdir -p ./build
export TMPDIR="$(pwd)/_arduino_tmp"     # ctags 등 임시파일 경로
export BUILD_DIR="$(pwd)/_arduino_build" # arduino-cli 빌드 경로
# 필요하면 TEMP/TMP도 같이
export TEMP="$TMPDIR"
export TMP="$TMPDIR"

# ===== 인자 파싱 =====
while getopts "p:b:f:s:d:" opt; do
  case "$opt" in
    p) PORT="$OPTARG" ;;
    b) BAUD="$OPTARG" ;;
    f) FQBN="$OPTARG" ;;
    s) SCHEME_FILE="$OPTARG" ;;
    d) SKETCH_DIR="$OPTARG" ;;
  esac
done

if [[ -z "${PORT}" ]]; then
  echo "Usage: $0 -p /dev/cu.usbmodemXXXX [-b 115200] [-f <FQBN>] [-s <partition.csv>] [-d <sketch_dir>]"
  exit 1
fi

# ===== Paths (Arduino 2.x / esp32 3.3.2 기준; 최신 탐색) =====
ARD15="$HOME/Library/Arduino15/packages/esp32"

ESPTOOL_BIN="$(ls -1 ${ARD15}/tools/esptool_py/*/esptool 2>/dev/null | sort -V | tail -n1)"
[[ -x "${ESPTOOL_BIN:-}" ]] || { echo "esptool 실행파일을 찾지 못했습니다."; exit 1; }

CORE_DIR="$(ls -1d ${ARD15}/hardware/esp32/* 2>/dev/null | sort -V | tail -n1)"
[[ -d "${CORE_DIR:-}" ]] || { echo "ESP32 하드웨어 코어가 없습니다. Boards Manager로 설치하세요."; exit 1; }

PARTCSV="$CORE_DIR/tools/partitions/$SCHEME_FILE"
[[ -f "$PARTCSV" ]] || { echo "Partition CSV 없음: $PARTCSV"; exit 1; }

# mklittlefs
if MKLFS="$(ls -1 ${ARD15}/tools/mklittlefs/*/mklittlefs 2>/dev/null | sort -V | tail -n1)"; then
  :
elif command -v mklittlefs >/dev/null 2>&1; then
  MKLFS="$(command -v mklittlefs)"
else
  echo "mklittlefs가 없습니다. 설치 예:  python3 -m pip install --user mklittlefs"
  exit 1
fi

# arduino-cli
command -v arduino-cli >/dev/null 2>&1 || {
  echo "arduino-cli가 없습니다. 설치 후 PATH에 추가하세요."
  exit 1
}

# ===== 파티션 이름(확장자 제거) 추출 =====
SCHEME_NAME="${SCHEME_FILE%.csv}"    # default.csv -> default

echo "[1/5] FQBN           = $FQBN"
echo "[1/5] Partition CSV  = $PARTCSV"
echo "[1/5] Partition Name = $SCHEME_NAME"

# ===== 스케치 컴파일 (build.partitions=이름) =====
echo "[2/5] 스케치 컴파일..."
arduino-cli compile \
  --fqbn $FQBN \
  --build-path $BUILD_DIR \
  --verbose $SKETCH_DIR

# ===== 산출물(.bin) 정확히 찾기 =====

APP_BIN="$(find "$BUILD_DIR" -type f -name "*.ino.bin" -print -quit)"
[[ -f "${APP_BIN:-}" ]] || { echo "스케치 .bin을 찾지 못했습니다. (검색 경로: $BUILD_DIR)"; exit 1; }

echo "[3/5] 스케치 업로드..."
arduino-cli upload -p "$PORT" --fqbn "$FQBN" --input-file "$APP_BIN" --verify

# ===== LittleFS 이미지 생성 =====
DATA_DIR="$SKETCH_DIR/data"
[[ -d "$DATA_DIR" ]] || { echo "data/ 폴더가 없습니다: $DATA_DIR"; exit 1; }

OFFSET_HEX=$(awk -F, 'tolower($1)~/(littlefs|spiffs)/{o=$4; gsub(/[[:space:]]/,"",o); print o; exit}' "$PARTCSV")
SIZE_HEX=$(awk   -F, 'tolower($1)~/(littlefs|spiffs)/{s=$5; gsub(/[[:space:]]/,"",s); print s; exit}' "$PARTCSV")
[[ -n "${OFFSET_HEX:-}" && -n "${SIZE_HEX:-}" ]] || { echo "CSV에 LittleFS/SPIFFS 항목이 없습니다."; exit 1; }

SIZE_DEC=$(( SIZE_HEX ))
echo "[4/5] littlefs.bin 생성(size=$SIZE_DEC, offset=$OFFSET_HEX)"
"$MKLFS" -c "$DATA_DIR" -s $SIZE_DEC -p 256 -b 4096 littlefs.bin
ls -lh littlefs.bin

# ===== 포트 점유 확인 =====
if lsof | grep -q "$PORT"; then
  echo "포트를 다른 프로세스가 사용 중입니다: $PORT"
  lsof | grep "$PORT" || true
  exit 1
fi

# ===== FS 플래시 =====
echo "[5/5] LittleFS 플래시: $PORT @ $OFFSET_HEX (baud=$BAUD)"
"$ESPTOOL_BIN" --chip esp32s3 --port "$PORT" --baud "$BAUD" write_flash "$OFFSET_HEX" littlefs.bin

echo "완료! (부팅 후 LittleFS.begin(false) & /login.html 존재 로그로 검증하세요)"
