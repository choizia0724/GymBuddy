#include "DistanceSensor.h"

DistanceSensor::DistanceSensor(const Pins& pins, const Config& cfg, TwoWire& bus)
: pins_(pins), cfg_(cfg), bus_(&bus) {}

bool DistanceSensor::begin() {
  // 0) XSHUT 하드리셋
  if (pins_.xshut >= 0) {
    pinMode(pins_.xshut, OUTPUT);
    digitalWrite(pins_.xshut, LOW);
    delay(10);
    digitalWrite(pins_.xshut, HIGH);
    delay(10);
  }

  Serial.printf("[DIST] SDA=%d SCL=%d\n", pins_.sda, pins_.scl);

  // 1) I2C 시작 — 먼저 100kHz로
  const uint32_t startHz = 100000;
  if (!bus_->begin(pins_.sda, pins_.scl, startHz)) {
    Serial.println("[DIST] I2C begin failed");
    return false;
  }
  delay(2);

  // 2) 센서 핑(선택, 디버그용)
  bus_->beginTransmission(0x29);
  uint8_t rc = bus_->endTransmission();
  if (rc != 0) {
    Serial.printf("[DIST] Ping 0x29 failed rc=%u (0=OK,2=addrNACK,3=dataNACK)\n", rc);
    return false;
  }

  // 3) VL53L0X 초기화(가능하면 같은 버스 포인터 전달)
  bool ok = false;
  // 최신 Adafruit_VL53L0X는 아래 시그니처 지원: begin(addr=0x29, debug=false, TwoWire* theWire=&Wire)
  ok = lox_.begin(VL53L0X_I2C_ADDR, false, bus_);
  // 만약 위 줄이 컴파일 에러라면 (구버전):
  //   - 라이브러리 업데이트 권장
  //   - 임시 우회: 전역 Wire를 같은 핀으로 시작시킨 뒤 lox_.begin() 호출
  //     Wire.begin(pins_.sda, pins_.scl, startHz);
  //     ok = lox_.begin();

  if (!ok) {
    Serial.println("[DIST] VL53L0X begin() failed");
    initialized_ = false;
    return false;
  }

  if (cfg_.i2cHz > startHz) {
    bus_->setClock(cfg_.i2cHz);
    delay(1);
  }

  initialized_ = true;
  Serial.println("[DIST] init OK");
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
