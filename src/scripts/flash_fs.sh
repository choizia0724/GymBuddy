#!/usr/bin/env zsh
set -euo pipefail

# ===== User args =====
PORT=""
SCHEME="default.csv"   # Arduino → Tools > Partition Scheme 이름에 대응
BAUD=115200

while getopts "p:s:b:" opt; do
  case "$opt" in
    p) PORT="$OPTARG" ;;
    s) SCHEME="$OPTARG" ;;
    b) BAUD="$OPTARG" ;;
  esac
done

if [[ -z "${PORT}" ]]; then
  echo "Usage: $0 -p /dev/cu.usbmodemXXXX -s <partition.csv> [-b 115200]"
  echo "Hint: ls /dev/cu.usb*  로 포트 확인"
  exit 1
fi

# ===== Paths (Arduino 2.x / esp32 5.x 기준; 최신 버전 자동 탐색) =====
ARD15="$HOME/Library/Arduino15/packages/esp32"
# esptool
ESPTOOL_BIN="$(ls -1 ${ARD15}/tools/esptool_py/*/esptool 2>/dev/null | sort -V | tail -n1)"
[[ -x "${ESPTOOL_BIN:-}" ]] || { echo "esptool 실행파일을 찾지 못했습니다."; exit 1; }

# 최신 하드웨어 코어 폴더
CORE_DIR="$(ls -1d ${ARD15}/hardware/esp32/* 2>/dev/null | sort -V | tail -n1)"
[[ -d "${CORE_DIR:-}" ]] || { echo "ESP32 하드웨어 코어가 없습니다. Boards Manager로 설치하세요."; exit 1; }

PARTCSV="$CORE_DIR/tools/partitions/$SCHEME"
[[ -f "$PARTCSV" ]] || { echo "Partition CSV 없음: $PARTCSV"; exit 1; }

echo "Using partition: $PARTCSV"

# ===== data/ → littlefs.bin 생성 =====
[[ -d "./data" ]] || { echo "data/ 폴더가 없습니다."; exit 1; }

# mklittlefs 찾기 (코어 동봉 또는 pip)
if MKLFS="$(ls -1 ${ARD15}/tools/mklittlefs/*/mklittlefs 2>/dev/null | sort -V | tail -n1)"; then
  :
elif command -v mklittlefs >/dev/null 2>&1; then
  MKLFS="$(command -v mklittlefs)"
else
  echo "mklittlefs가 없습니다. 설치 예:  python3 -m pip install --user mklittlefs"
  exit 1
fi

# ===== CSV에서 LittleFS(or SPIFFS) 파티션 추출 =====
OFFSET_HEX=$(awk -F, 'tolower($1)~/(littlefs|spiffs)/{print $4; exit}' "$PARTCSV")
SIZE_HEX=$(awk   -F, 'tolower($1)~/(littlefs|spiffs)/{print $5; exit}' "$PARTCSV")

[[ -n "${OFFSET_HEX:-}" && -n "${SIZE_HEX:-}" ]] || { echo "CSV에 LittleFS/SPIFFS 항목이 없습니다."; exit 1; }

echo "FS OFFSET = $OFFSET_HEX"
echo "FS SIZE   = $SIZE_HEX"

# 0xNNNNNN → 10진
SIZE_DEC=$(( SIZE_HEX ))

echo "Creating littlefs.bin (size=$SIZE_DEC bytes)..."
"$MKLFS" -c ./data -s $SIZE_DEC -p 256 -b 4096 littlefs.bin
ls -lh littlefs.bin

# ===== 포트 점유 확인 =====
if lsof | grep -q "$PORT"; then
  echo "포트를 다른 프로세스가 사용 중입니다: $PORT"
  lsof | grep "$PORT" || true
  exit 1
fi

# ===== 보드 플래시 쓰기 (ESP32-S3) =====
echo "Flashing FS to $PORT @ $OFFSET_HEX (baud=$BAUD)..."
"$ESPTOOL_BIN" --chip esp32s3 --port "$PORT" --baud "$BAUD" \
  write_flash "$OFFSET_HEX" littlefs.bin

echo "Done. 스케치에서 LittleFS.begin() 으로 마운트 확인하세요."
