#include "TrendDetector.h"

TrendDetector::TrendDetector() : params_(Params{}) {}
TrendDetector::TrendDetector(const Params& p) : params_(p) {}

void TrendDetector::reset() {
  snap_ = Snapshot{};
}

bool TrendDetector::step(uint16_t d) {
  if (d == 0 || d > params_.max_range_mm) return false;

  if (snap_.last == 0) {
    snap_.last  = snap_.minv = snap_.maxv = d;
    snap_.phase = Phase::Idle;
    return false;
  }

  switch (snap_.phase) {
    case Phase::Idle:
      if (snap_.last >= d && (snap_.last - d) >= params_.noise_mm) {
        snap_.phase = Phase::Down;
        snap_.minv  = d;
        snap_.maxv  = d;
      }
      break;

    case Phase::Down:
      if (d + params_.noise_mm < snap_.minv) {
        snap_.minv = d;
      } else if (d >= static_cast<uint16_t>(snap_.minv + params_.noise_mm)) {
        snap_.phase = Phase::Up;
        snap_.maxv  = d;
        snap_.last  = d;
        return true; // ← Send 타이밍
      }
      break;

    case Phase::Up:
      if (d > snap_.maxv) {
        snap_.maxv = d;
      } else if (snap_.maxv >= d && (snap_.maxv - d) >= params_.noise_mm) {
        snap_.phase = Phase::Down;
        snap_.minv  = d;
      }
      break;
  }

  snap_.last = d;
  return false;
}
