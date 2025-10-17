#!/usr/bin/env zsh
chmod +x upload.sh
set -euo pipefail

# ===== User args =====
PORT="/dev/cu.usbmodem5AE60253511"
SCHEME="default.csv"
BAUD=115200

while getopts "p:s:b:" opt; do
  case "$opt" in
    p) PORT="$OPTARG" ;;
    s) SCHEME="$OPTARG" ;;
    b) BAUD="$OPTARG" ;;
  esac
done

if [[ -z "${PORT}" ]]; then
  echo "Usage: $0 -p /dev/cu.usbmodem5AE60253511 -s <partition.csv> [-b 115200]"
  echo "Hint: ls /dev/cu.usb*  로 포트 확인"
  exit 1
fi

# ===== Paths (Arduino 2.x / esp32 5.1.0 기준) =====
ARD15="$HOME/Library/Arduino15/packages/esp32"
ESPTOOL_BIN="$ARD15/tools/esptool_py/5.1.0/esptool"

if [[ ! -x "$ESPTOOL_BIN" ]]; then
  echo "esptool 실행파일이 없습니다: $ESPTOOL_BIN"
  echo "Boards Manager에서 esp32 5.1.0 코어가 설치되어 있는지 확인하세요."
  exit 1
fi

# 가장 최신 하드웨어 코어 폴더 추출
CORE_DIRS=( "$HOME/Library/Arduino15/packages/esp32/hardware/esp32"/* )
if (( ${#CORE_DIRS[@]} == 0 )); then
  echo "ESP32 하드웨어 코어가 없습니다. Boards Manager로 설치하세요."
  exit 1
fi
CORE_DIR="${CORE_DIRS[-1]}"

PARTCSV="$CORE_DIR/tools/partitions/$SCHEME"
if [[ ! -f "$PARTCSV" ]]; then
  echo "Partition CSV를 찾을 수 없습니다: $PARTCSV"
  echo "Tools → Partition Scheme 에서 실제 스킴 이름을 확인해 주세요."
  exit 1
fi

echo "Using partition: $PARTCSV"

# ===== data/ → littlefs.bin 생성 =====
if [[ ! -d "./data" ]]; then
  echo "data/ 폴더가 없습니다. 업로드할 파일을 넣고 다시 실행하세요."
  exit 1
fi

# mklittlefs 경로 탐색 (코어 포함 또는 pip)
MKLFS_CAND=( "$ARD15/tools/mklittlefs"/*/mklittlefs )
if [[ -x "${MKLFS_CAND[-1]:-}" ]]; then
  MKLFS="${MKLFS_CAND[-1]}"
else
  if ! command -v mklittlefs >/dev/null 2>&1; then
    echo "mklittlefs가 없습니다. 다음 명령으로 설치:"
    echo "  python3 -m pip install --user mklittlefs"
    exit 1
  fi
  MKLFS="$(command -v mklittlefs)"
fi

# ===== 파티션에서 LittleFS(or SPIFFS) 오프셋/크기 추출 =====
OFFSET_HEX=$(awk -F, 'tolower($1)~/(littlefs|spiffs)/{print $4; exit}' "$PARTCSV")
SIZE_HEX=$(awk   -F, 'tolower($1)~/(littlefs|spiffs)/{print $5; exit}' "$PARTCSV")

if [[ -z "${OFFSET_HEX:-}" || -z "${SIZE_HEX:-}" ]]; then
  echo "CSV에서 LittleFS/SPIFFS 파티션을 찾지 못했습니다."
  exit 1
fi

echo "FS OFFSET = $OFFSET_HEX"
echo "FS SIZE   = $SIZE_HEX"

# 0xNNNNNN → 10진 변환 (zsh 산술확장 사용)
SIZE_DEC=$(( SIZE_HEX ))

echo "Creating littlefs.bin (size=$SIZE_DEC bytes)..."
"$MKLFS" -c ./data -s $SIZE_DEC -p 256 -b 4096 littlefs.bin
ls -lh littlefs.bin

# ===== 포트 점유 확인 =====
if lsof | grep -q "$PORT"; then
  echo "포트를 다른 프로세스가 사용 중입니다: $PORT"
  echo "시리얼 모니터/IDE를 닫고 다시 실행하세요."
  lsof | grep "$PORT" || true
  exit 1
fi

# ===== 보드 플래시 쓰기 =====
echo "Flashing FS to $PORT @ $OFFSET_HEX ..."
"$ESPTOOL_BIN" --chip esp32s3 --port "$PORT" --baud "$BAUD" write-flash "$OFFSET_HEX" littlefs.bin

echo "Done. 스케치에서 LittleFS.begin() 으로 마운트 확인하세요."
