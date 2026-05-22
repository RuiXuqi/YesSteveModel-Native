#include <cstdint>
#include <span>

#include <java/array.h>
#include <java/entry.h>
#include <java/opaque_ptr.h>
#include <renderer/model_state.h>
#include "err.h"

namespace ysm::lib::render {
YSM_JNI_ENTRY(
    "Lcom/elfmcys/ysm/natives/render/NativeModelState;nCreate()J",
    ()) {
    return java::MakeOpaquePtr<renderer::ModelState>();
}

YSM_JNI_ENTRY(
    "Lcom/elfmcys/ysm/natives/render/NativeModelState;nExtract(JJ[F[SJJ)J",
    (state_ptr, baked_model_ptr, bone_attributes_array,
     locator_bone_indices_array, bone_pose_ptr, bone_pose_capacity),
    jlong{-1}) {
    YSM_DECLARE_OR_RETURN(
        state, java::CastOpaquePtr<renderer::ModelState>(state_ptr));
    YSM_DECLARE_OR_RETURN(
        baked_model,
        java::CastOpaquePtr<bake::BakedModel>(baked_model_ptr));
    YSM_ASSERT(env != nullptr && locator_bone_indices_array != nullptr,
               absl::InvalidArgumentError("Invalid locator buffer."));
    const auto locator_capacity =
        env->GetArrayLength(locator_bone_indices_array);
    YSM_ASSERT(
        bone_pose_ptr != 0 && bone_pose_capacity >= 0 &&
            static_cast<uint64_t>(bone_pose_ptr) %
                    alignof(math::PoseStack::Pose) ==
                0 &&
            static_cast<uint64_t>(bone_pose_capacity) %
                    sizeof(math::PoseStack::Pose) ==
                0,
        absl::InvalidArgumentError("Invalid bone pose buffer."));

    auto* pose = reinterpret_cast<math::PoseStack::Pose*>(
        static_cast<uintptr_t>(bone_pose_ptr));
    const auto pose_count = static_cast<size_t>(bone_pose_capacity) /
                            sizeof(math::PoseStack::Pose);
    jlong packed_output;
    size_t locator_count;
    {
        YSM_DECLARE_OR_RETURN(
            bone_attributes,
            java::CriticalFloatArray<true>::Get(env, bone_attributes_array)); // 这对吗？
        YSM_ASSERT(
            bone_attributes.size() %
                    renderer::BoneAttribute::kFloatCount ==
                0,
            absl::InvalidArgumentError(
                "Invalid bone attribute buffer size."));
        const auto* attributes =
            reinterpret_cast<const renderer::BoneAttribute*>(
                bone_attributes.data());
        const auto attribute_count =
            bone_attributes.size() / renderer::BoneAttribute::kFloatCount;
        YSM_DECLARE_OR_RETURN(
            output,
            magic_enum::enum_switch(
                [&](auto kSimdType) ->
                    absl::StatusOr<renderer::ModelState::ExtractOutput> {
                    static constexpr simd::Tag<kSimdType> tag;
                    return state->Extract(
                        tag, baked_model, {attributes, attribute_count},
                        static_cast<size_t>(locator_capacity),
                        {pose, pose_count});
                },
                simd::kSupported));
        locator_count = output.locator_count;
        packed_output = static_cast<jlong>(output.vertex_count) |
                        (static_cast<jlong>(output.locator_count) << 32);
    }
    YSM_ASSERT(locator_count <= static_cast<size_t>(locator_capacity),
               absl::InternalError("Invalid locator count."));
    YSM_RETURN_IF_ERROR(java::WriteShortArray<true>(
        env, locator_bone_indices_array,
        state->StagedLocatorBoneIndices(locator_count)));
    return packed_output;
}
}  // namespace ysm::lib::render
