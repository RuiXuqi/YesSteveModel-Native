#include "renderer/model_state.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <ranges>

#include "err.h"
#include "parallel_executor.h"

YSM_FAST_MATH_BEGIN

#ifndef GUARD_MODEL_STATE_IMPL
#define GUARD_MODEL_STATE_IMPL

namespace ysm::renderer {
namespace {
constexpr float kModelUnit = 1.0f / 16.0f;
}  // namespace ysm::renderer
}

#endif

namespace ysm::renderer {
namespace {
YSM_SIMD_TARGET
bool IsFinite(YSM_SIMD_TAG tag, std::span<const float> values) noexcept {
    return std::ranges::all_of(
        values, [](float value) { return std::isfinite(value); });
}

YSM_SIMD_TARGET
void ApplyLocalState(YSM_SIMD_TAG tag, const bake::BakedModelBones::BoneInfo& bone,
                     const BoneAttribute& attribute, math::PoseStack& pose_stack) {
    pose_stack.Translate(tag, (bone.pivot[0] - attribute.position[0]) * kModelUnit,
                         (bone.pivot[1] + attribute.position[1]) * kModelUnit,
                         (bone.pivot[2] + attribute.position[2]) * kModelUnit);
    pose_stack.RotateZYX(tag, attribute.rotation[0], attribute.rotation[1],
                         attribute.rotation[2]);
    pose_stack.Scale(tag, attribute.scale[0], attribute.scale[1],
                     attribute.scale[2]);
    pose_stack.Translate(tag, -bone.pivot[0] * kModelUnit,
                         -bone.pivot[1] * kModelUnit,
                         -bone.pivot[2] * kModelUnit);
}

YSM_SIMD_TARGET
bool IsFinite(YSM_SIMD_TAG tag, const BoneAttribute& attribute) noexcept {
    return IsFinite(tag, attribute.rotation) && IsFinite(tag, attribute.position) &&
           IsFinite(tag, attribute.scale) && std::isfinite(attribute.cubes_hidden) &&
           std::isfinite(attribute.children_hidden) &&
           std::isfinite(attribute.locator_sequence);
}
}  // namespace

template <>
YSM_SIMD_TARGET
absl::StatusOr<ModelState::ExtractOutput> ModelState::Extract<YSM_SIMD_TAG>(
    YSM_SIMD_TAG tag,
    const std::shared_ptr<bake::BakedModel>& baked_model,
    std::span<const BoneAttribute> bone_attributes,
    size_t locator_capacity,
    std::span<math::PoseStack::Pose> bone_pose_buffer) {
    locator_bone_indices_scratch_.resize(locator_capacity);
    valid_ = false;

    const auto worker_count = ParallelExecutor::Get().GetThreadCount();
    const auto previous_schedule_reusable =
        schedule_valid_ && baked_model_ == baked_model &&
        schedule_worker_count_ == worker_count;
    schedule_valid_ = false;

    YSM_ASSERT(baked_model != nullptr,
               absl::InvalidArgumentError("Baked model is null."));
    if (baked_model_ != baked_model) {
        baked_model_ = baked_model;
    }

    const auto& bones = baked_model->Bones();
    const auto bone_count = bones.list.size();
    YSM_ASSERT(bone_count <= static_cast<size_t>(
                                 std::numeric_limits<uint16_t>::max()) + 1ULL &&
                   bone_attributes.size() == bone_count &&
                   locator_capacity >= bone_count &&
                   bone_pose_buffer.size() >= bone_count,
               absl::InvalidArgumentError("Invalid ModelState buffers."));

    const auto previous_bone_index_count = render_bone_indices_.size();
    render_bone_indices_.reserve(bone_count);
    size_t bone_index_count = 0;
    bool bone_indices_unchanged = true;

    thread_local math::PoseStack local_pose_stack;
    auto& pose_stack = local_pose_stack;
    pose_stack.PopPose(0);
    pose_stack.Reserve(bone_count + 1);

    uint32_t locator_count = 0;
    for (size_t bone_index = 0; bone_index < bone_count;) {
        const auto& bone = bones.list[bone_index];
        const auto& attribute = bone_attributes[bone_index];
        pose_stack.PopPose(bone.depth);
        pose_stack.PushPose();
        const float locator_sequence = attribute.locator_sequence;
        if (!IsFinite(tag, attribute) || attribute.scale[0] == 0.0f ||
            attribute.scale[1] == 0.0f || attribute.scale[2] == 0.0f) {
            bone_index = bone.subtree_end;
            continue;
        }
        YSM_ASSERT(locator_sequence >= 0.0f && locator_sequence <= 255.0f &&
                       std::trunc(locator_sequence) == locator_sequence,
                   absl::InvalidArgumentError("Invalid locator sequence."));

        ApplyLocalState(tag, bone, attribute, pose_stack);
        const auto& pose = pose_stack.Last();
        if (!IsFinite(tag, std::span(&pose.pose[0][0], 16)) ||
            !IsFinite(tag, std::span(&pose.normal[0][0], 9))) {
            bone_index = bone.subtree_end;
            continue;
        }
        pose.CopyTo(bone_pose_buffer[bone_index]);

        const bool cubes_hidden = attribute.cubes_hidden != 0.0f;
        if (!cubes_hidden) {
            if (bone.HasGeometry()) {
                const auto render_bone_index =
                    static_cast<uint16_t>(bone_index);
                if (bone_index_count == render_bone_indices_.size()) {
                    render_bone_indices_.push_back(render_bone_index);
                    bone_indices_unchanged = false;
                } else {
                    bone_indices_unchanged &=
                        render_bone_indices_[bone_index_count] ==
                        render_bone_index;
                    render_bone_indices_[bone_index_count] = render_bone_index;
                }
                ++bone_index_count;
            }
            if (locator_sequence != 0.0f) {
                locator_bone_indices_scratch_[locator_count++] =
                    static_cast<uint16_t>(bone_index);
            }
        }
        if (attribute.children_hidden != 0.0f) {
            bone_index = bone.subtree_end;
        } else {
            ++bone_index;
        }
    }

    bone_indices_unchanged &= bone_index_count == previous_bone_index_count;
    render_bone_indices_.resize(bone_index_count);
    const bool update_schedule =
        !previous_schedule_reusable || !bone_indices_unchanged;
    if (update_schedule) {
        YSM_RETURN_IF_ERROR(
            schedule_.Update(bones, render_bone_indices_, worker_count));
    }

    bone_poses_ = bone_pose_buffer.first(bone_count);
    schedule_worker_count_ = worker_count;
    schedule_valid_ = true;
    valid_ = true;
    return ExtractOutput{
        .vertex_count = schedule_.vertex_count,
        .locator_count = locator_count,
        .schedule_updated = update_schedule,
    };
}
}  // namespace ysm::renderer

YSM_FAST_MATH_END