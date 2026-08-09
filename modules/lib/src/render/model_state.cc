#include <array>
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
    "Lcom/elfmcys/ysm/natives/render/NativeModelState;nExtract(JJ[F[S[J)Z",
    (state_ptr, baked_model_ptr, bone_attributes_array,
     locator_bone_indices_array, output_array)) {
    YSM_DECLARE_OR_RETURN(
        state, java::CastOpaquePtr<renderer::ModelState>(state_ptr));
    YSM_DECLARE_OR_RETURN(
        baked_model,
        java::CastOpaquePtr<bake::BakedModel>(baked_model_ptr));
    YSM_ASSERT(env != nullptr && locator_bone_indices_array != nullptr &&
                   output_array != nullptr,
               absl::InvalidArgumentError("Invalid ModelState output."));
    const auto locator_capacity =
        env->GetArrayLength(locator_bone_indices_array);
    renderer::ModelState::ExtractOutput output;
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
            extracted,
            magic_enum::enum_switch(
                [&](auto kSimdType) ->
                    absl::StatusOr<renderer::ModelState::ExtractOutput> {
                    static constexpr simd::Tag<kSimdType> tag;
                    return state->Extract(
                        tag, baked_model, {attributes, attribute_count},
                        static_cast<size_t>(locator_capacity));
                },
                simd::kSupported));
        output = extracted;
        locator_count = extracted.locator_count;
    }
    YSM_ASSERT(locator_count <= static_cast<size_t>(locator_capacity),
               absl::InternalError("Invalid locator count."));
    YSM_RETURN_IF_ERROR(java::WriteShortArray<true>(
        env, locator_bone_indices_array,
        state->StagedLocatorBoneIndices(locator_count)));
    const auto pose_view = state->PoseView();
    const auto bone_poses = pose_view.bone_poses;
    const auto render_bone_indices = pose_view.render_bone_indices;
    const auto pack_counts = [](uint32_t low, uint32_t high) {
        return static_cast<jlong>((static_cast<uint64_t>(high) << 32) | low);
    };
    const std::array<jlong, 4> packed_output{
        reinterpret_cast<jlong>(bone_poses.data()),
        reinterpret_cast<jlong>(render_bone_indices.data()),
        pack_counts(static_cast<uint32_t>(render_bone_indices.size()),
                    output.locator_count),
        pack_counts(output.vertex_count,
                    state->Schedule().translucent_vertex_count),
    };
    return java::WriteLongArray<true>(env, output_array, packed_output);
}
}  // namespace ysm::lib::render
