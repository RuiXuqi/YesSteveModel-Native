#include <cglm/euler.h>

#include "pose_stack.h"

#include <vector>

#include <cglm/mat3.h>
#include <cglm/mat4.h>

#include "fast_math.h"
#include "math/euler.h"
#include "math/mat4_mul.h"

YSM_FAST_MATH_BEGIN

namespace ysm::math {
template <>
YSM_SIMD_TARGET
void PoseStack::RotateZYX<YSM_SIMD_TAG>(YSM_SIMD_TAG tag, float x, float y, float z) noexcept {
    vec3 angles{x, y, z};
    auto& current = poses_[pose_index_];
    mat4 rotation4;
    mat3 rotation;
    EulerZYX(tag, angles, rotation4);
    // glm_euler_zyx(angles, rotation4);
    glm_mat4_pick3(rotation4, rotation);

    Mat4Mul(tag, current.pose, rotation4, current.pose);
    glm_mat3_mul(current.normal, rotation, current.normal);
}

template <>
YSM_SIMD_TARGET
void PoseStack::Translate<YSM_SIMD_TAG>(YSM_SIMD_TAG, float x, float y, float z) noexcept {
    vec3 translation{x, y, z};
    glm_translate(poses_[pose_index_].pose, translation);
}

template <>
YSM_SIMD_TARGET
void PoseStack::Scale<YSM_SIMD_TAG>(YSM_SIMD_TAG, float x, float y, float z) noexcept {
    auto& current = poses_[pose_index_];
    vec3 scale{x, y, z};
    const bool uniform_scale = IsUniformScale(x, y, z);
    current.uniform_scale = current.uniform_scale && uniform_scale;
    if (uniform_scale) {
        current.normal_scale *= std::abs(x);
    }
    if (SignBit(x) ^ SignBit(y) ^ SignBit(z)) {
        current.tangent_orientation = -current.tangent_orientation;
    }

    glm_scale(current.pose, scale);
    for (size_t column = 0; column < 3; ++column) {
        const float normal_scale = uniform_scale
                                       ? std::copysign(1.0f, scale[column])
                                       : 1.0f / scale[column];
        glm_vec3_scale(current.normal[column], normal_scale,
                       current.normal[column]);
    }
}
}  // namespace ysm::renderer

YSM_FAST_MATH_END
