#pragma once
#include <Arduino.h>

class TrendDetector {
public:
  enum class Phase { Idle, Down, Up };

  struct Params {
    uint16_t noise_mm;
    uint16_t max_range_mm;
    // 기본값은 생성자에서 지정
    constexpr Params(uint16_t noise = 20, uint16_t maxr = 2000)
      : noise_mm(noise), max_range_mm(maxr) {}
  };

  struct Snapshot {
    Phase    phase = Phase::Idle;
    uint16_t last  = 0;
    uint16_t minv  = 0;
    uint16_t maxv  = 0;
  };

  TrendDetector();                       // ← 디폴트 생성자 추가
  explicit TrendDetector(const Params& p);

  void reset();
  bool step(uint16_t d);                 // 최저점에서 +noise 이상 반등 시 true
  const Snapshot& state() const { return snap_; }

private:
  Params   params_{20, 2000};            // ← 기본 파라미터
  Snapshot snap_;
};
