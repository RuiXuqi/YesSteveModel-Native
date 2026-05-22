#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <absl/status/statusor.h>

#include "math/pose_stack.h"
#include "bake/baked_model.h"
#include "bone_attribute.h"
#include "schedule.h"

namespace ysm::renderer {
struct ModelPoseView {
    std::span<const math::PoseStack::Pose> bone_poses;
    std::span<const uint16_t> render_bone_indices;
};

class ModelState {
   public:
    struct ExtractOutput {
        uint32_t vertex_count = 0;
        uint32_t locator_count = 0;
        bool schedule_updated = false;
    };

    template <typename SimdTag>
    absl::StatusOr<ExtractOutput> Extract(
        SimdTag tag,
        const std::shared_ptr<bake::BakedModel>& baked_model,
        std::span<const BoneAttribute> bone_attributes,
        size_t locator_capacity,
        std::span<math::PoseStack::Pose> bone_pose_buffer);

    [[nodiscard]] std::span<const uint16_t> StagedLocatorBoneIndices(
        size_t count) const {
        return std::span(locator_bone_indices_scratch_).first(count);
    }

    [[nodiscard]] bool IsValid() const noexcept { return valid_; }

    [[nodiscard]] const std::shared_ptr<bake::BakedModel>& Model() const {
        return baked_model_;
    }

    [[nodiscard]] size_t RenderBoneSize() const noexcept {
        return render_bone_indices_.size();
    }

    [[nodiscard]] const RenderSchedule& Schedule() const { return schedule_; }

    [[nodiscard]] ModelPoseView PoseView() const {
        return {bone_poses_, render_bone_indices_};
    }

   private:
    std::shared_ptr<bake::BakedModel> baked_model_;
    std::span<const math::PoseStack::Pose> bone_poses_;
    std::vector<uint16_t> render_bone_indices_;
    std::vector<uint16_t> locator_bone_indices_scratch_;
    RenderSchedule schedule_;
    size_t schedule_worker_count_ = 0;
    bool schedule_valid_ = false;
    bool valid_ = false;
};
}  // namespace ysm::renderer
