#ifndef RUNE_VEC_H
#define RUNE_VEC_H

#include "types.h"

namespace rune {

template <typename T>
struct Vec4 {
  public:
    constexpr explicit Vec4(T value = T{}) : Vec4(value, value, value, value) {}
    constexpr Vec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}

    union {
        struct {
            T x, y, z, w;
        };
        T data[4];
    };

    T& operator[](u32 idx) {
        return data[idx];
    }
};

using Vec4f = Vec4<f32>;

}

#endif // RUNE_VEC_H
