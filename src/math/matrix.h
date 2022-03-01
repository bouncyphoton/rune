#ifndef RUNE_MATRIX_H
#define RUNE_MATRIX_H

#include "math/vector.h"

namespace rune {

/**
 * Column-major matrix
 * @tparam T Type of internal elements
 */
template <typename T>
struct Mat4x4 {
  public:
    constexpr explicit Mat4x4(T diag = 0.0f) {
        for (i32 i = 0; i < 4; ++i) {
            columns[i][i] = diag;
        }
    }

    // clang-format off
    static inline constexpr Vec4<T> Identity[4] = {
        Vec4<T>(1, 0, 0, 0),
        Vec4<T>(0, 1, 0, 0),
        Vec4<T>(0, 0, 1, 0),
        Vec4<T>(0, 0, 0, 1)
    };
    // clang-format on

    Vec4<T>& operator[](u32 idx) {
        return columns[idx];
    }

    Vec4<T> columns[4];
};

using Mat4x4f = Mat4x4<f32>;

}

#endif // RUNE_MATRIX_H
