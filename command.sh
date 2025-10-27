chmod +x flash_fs.sh
./flash_fs.sh -p /dev/cu.usbmodem5AE60253511 -s default.csv -b 115200


# firmware 빌드
# arduino-cli compile --fqbn esp32:esp32:esp32s3 \
#   --build-property "build.partitions=default" \
#   --export-binaries


# littlefs 파일시스템 이미지 생성
# mklittlefs -c ./data -s <FS_바이트_크기> -p 256 -b 4096 littlefs.bin
