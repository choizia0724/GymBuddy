#pragma once
#include <type_traits>

template <typename T>
constexpr inline T absdiff(T a, T b) {
  static_assert(std::is_integral<T>::value, "absdiff requires integral type");
  return (a > b) ? (a - b) : (b - a);
}