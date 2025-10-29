#!/usr/bin/env zsh
set -e

# 기본값
PORT=""
BAUD="115200"
SCHEME_FILE="default.csv"       # partitions CSV 파일명
SKETCH_DIR="$(pwd)"
METHOD="ota"                    # ota | serial
DEVICE_IP=""                    # METHOD=ota 일 때 필요

# 인자 파싱
while getopts "p:b:s:d:m:i:" opt; do
  case "$opt" in
    p) PORT="$OPTARG" ;;
    b) BAUD="$OPTARG" ;;
    s) SCHEME_FILE="$OPTARG" ;;
    d) SKETCH_DIR="$OPTARG" ;;
    m) METHOD="$OPTARG" ;;         # ota | serial
    i) DEVICE_IP="$OPTARG" ;;
  esac
done

# 경로/툴
ARD15="$HOME/Library/Arduino15/packages/esp32"
CORE_DIR="$(ls -1d "$ARD15"/hardware/esp32/* 2>/dev/null | sort -V | tail -n1)"
[ -d "$CORE_DIR" ] || { echo "[little.sh] ESP32 코어를 찾지 못했습니다."; exit 1; }

PARTCSV="$CORE_DIR/tools/partitions/$SCHEME_FILE"
[ -f "$PARTCSV" ] || { echo "[little.sh] Partition CSV 없음: $PARTCSV"; exit 1; }

ESPTOOL_BIN="$(ls -1 "$ARD15"/tools/esptool_py/*/esptool 2>/dev/null | sort -V | tail -n1)"
[ -x "$ESPTOOL_BIN" ] || { echo "[little.sh] esptool 를 찾지 못했습니다."; exit 1; }

# mklittlefs
if MKLFS="$(ls -1 "$ARD15"/tools/mklittlefs/*/mklittlefs 2>/dev/null | sort -V | tail -n1)"; then
  :
elif command -v mklittlefs >/dev/null 2>&1; then
  MKLFS="$(command -v mklittlefs)"
else
  echo "[little.sh] mklittlefs가 없습니다. (python3 -m pip install --user mklittlefs)"
  exit 1
fi

# 데이터 폴더
DATA_DIR="$SKETCH_DIR/data"
[ -d "$DATA_DIR" ] || { echo "[little.sh] data/ 폴더 없음: $DATA_DIR"; exit 1; }

# CSV에서 FS offset/size 추출(공백 제거)
OFFSET_HEX="$(awk -F, 'tolower($1)~/(littlefs|spiffs)/{o=$4; gsub(/[[:space:]]/,"",o); print o; exit}' "$PARTCSV")"
SIZE_HEX="$(  awk -F, 'tolower($1)~/(littlefs|spiffs)/{s=$5; gsub(/[[:space:]]/,"",s); print s; exit}' "$PARTCSV")"
[ -n "$OFFSET_HEX" ] && [ -n "$SIZE_HEX" ] || { echo "[little.sh] CSV에 LittleFS/SPIFFS 항목 없음"; exit 1; }

# 16진/10진 모두 처리
case "$SIZE_HEX" in
  0x*|0X*) SIZE_DEC=$(( SIZE_HEX )) ;;
  *)       SIZE_DEC=$(( 10#$SIZE_HEX )) ;;
esac

echo "[little.sh] FS size=$SIZE_DEC, offset=$OFFSET_HEX"

# 이미지 생성
TMPDIR="$SKETCH_DIR/_arduino_tmp"
mkdir -p "$TMPDIR"
export TMPDIR
echo "[little.sh] littlefs.bin 생성..."
"$MKLFS" -c "$DATA_DIR" -s "$SIZE_DEC" -p 256 -b 4096 littlefs.bin
ls -lh littlefs.bin

# 업로드 방법 분기
if [ "$METHOD" = "ota" ] && [ -n "$DEVICE_IP" ]; then
  # OTA (/fsupdate 엔드포인트 필요)
  echo "[little.sh] OTA 업로드 → http://$DEVICE_IP/fsupdate"
  # 인증이 필요하면 여기에 -u user:pass 또는 -H 'Authorization: ...' 추가
  curl -sf -F "update=@littlefs.bin" "http://$DEVICE_IP/fsupdate"
  echo "[little.sh] OTA 완료 (기기 재시작 후 LittleFS.begin(false) 확인)"
  exit 0
fi

# Serial 방법: 부트로더 진입 필요
PORT_TTY="$(echo "$PORT" | sed 's#/dev/cu\.#/dev/tty.#')"
echo "[little.sh] Serial 플래시 (포트=$PORT_TTY, baud=$BAUD)"
echo "==> 보드를 부트로더 모드로: BOOT(=GPIO0) 누른채 EN(Reset) 탭 → BOOT 떼기"
printf "Enter 키를 누르면 진행합니다... "; read x

"$ESPTOOL_BIN" --chip esp32s3 \
  --before default_reset --after hard_reset \
  --port "$PORT_TTY" --baud "$BAUD" \
  write-flash "$OFFSET_HEX" littlefs.bin

echo "[little.sh] Serial 플래시 완료"
