#!/usr/bin/env zsh
set -e

# 공통 인자 전달:
# -p PORT -b BAUD -f FQBN -S SCHEME_NAME(스케치) -s SCHEME_FILE(CSV) -d SKETCH_DIR
# + b.sh 전용: -m METHOD(ota|serial) -i DEVICE_IP

# 기본값(없으면 각 스크립트의 기본 사용)
PORT=""
BAUD="115200"
FQBN="esp32:esp32:esp32s3"
SCHEME_NAME="default"
SCHEME_FILE="default.csv"
SKETCH_DIR="$(pwd)"
METHOD="ota"
DEVICE_IP=""

while getopts "p:b:f:S:s:d:m:i:" opt; do
  case "$opt" in
    p) PORT="$OPTARG" ;;
    b) BAUD="$OPTARG" ;;
    f) FQBN="$OPTARG" ;;
    S) SCHEME_NAME="$OPTARG" ;;
    s) SCHEME_FILE="$OPTARG" ;;
    d) SKETCH_DIR="$OPTARG" ;;
    m) METHOD="$OPTARG" ;;
    i) DEVICE_IP="$OPTARG" ;;
  esac
done

# 1) 스케치 업로드
sh ./sketch.sh -p "$PORT" -b "$BAUD" -f "$FQBN" -S "$SCHEME_NAME" -d "$SKETCH_DIR"

# 잠깐 대기(포트 재인식 여유)
sleep 1

# 2) LittleFS 업로드
#   - OTA 사용 시: -m ota -i DEVICE_IP 지정
#   - Serial 사용 시: -m serial 만 지정(부트로더 수동 진입 필요)
sh ./littlefs.sh -p "$PORT" -b "$BAUD" -s "$SCHEME_FILE" -d "$SKETCH_DIR" -m "$METHOD" -i "$DEVICE_IP"

echo "[upload.sh] 전체 완료"
