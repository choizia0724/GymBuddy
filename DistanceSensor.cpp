#include "DistanceSensor.h"

DistanceSensor::DistanceSensor(const Pins& pins, const Config& cfg)
: pins_(pins), cfg_(cfg) {}

bool DistanceSensor::begin() {
  // XSHUT으로 하드리셋(LOW→HIGH)
  if (pins_.xshut >= 0) {
    pinMode(pins_.xshut, OUTPUT);
    digitalWrite(pins_.xshut, LOW);
    delay(10);
    digitalWrite(pins_.xshut, HIGH);
    delay(10);
  }

  // I2C 시작 (ESP32는 SDA/SCL 지정 가능)
  Wire.begin(pins_.sda, pins_.scl);
  Wire.setClock(cfg_.i2cHz);

  // Adafruit VL53L0X 초기화
  if (!lox_.begin()) {
    initialized_ = false;
    return false;
  }

  // ⚠️ Adafruit_VL53L0X에는 setTimeout 없음 → 호출 제거
  // lox_.setTimeout(cfg_.measureTimeoutMs); // (삭제)

  initialized_ = true;
  return true;
}

bool DistanceSensor::singleRead_(uint16_t& mm) {
  if (!initialized_) return false;

  VL53L0X_RangingMeasurementData_t measure;
  lox_.rangingTest(&measure, false); // debug=false

  if (measure.RangeStatus != 4) {    // 4 = out of range
    mm = measure.RangeMilliMeter;
    return true;
  }
  return false;
}

bool DistanceSensor::read(uint16_t& mm) {
  // 간단한 중앙값 필터 (medianN이 홀수일 때)
  uint8_t n = cfg_.medianN;
  if (n <= 1 || (n % 2) == 0) {
    // 샘플 1회
    return singleRead_(mm);
  }

  // n회 샘플링 후 중앙값
  const uint8_t N = (n > 7 ? 7 : n); // 안전상 최대 7로 클립
  uint16_t buf[7];
  uint8_t got = 0;

  for (uint8_t i = 0; i < N; ++i) {
    uint16_t v;
    if (singleRead_(v)) {
      buf[got++] = v;
    } else {
      // 실패 시 살짝 대기하고 재시도
      delay(5);
    }
    // 샘플 간 짧은 텀
    delay(2);
  }

  if (got == 0) return false;

  // 단순 삽입정렬 후 중앙값 선택
  for (uint8_t i = 1; i < got; ++i) {
    uint16_t key = buf[i];
    int j = i - 1;
    while (j >= 0 && buf[j] > key) {
      buf[j + 1] = buf[j];
      j--;
    }
    buf[j + 1] = key;
  }

  mm = buf[got / 2];
  return true;
}
