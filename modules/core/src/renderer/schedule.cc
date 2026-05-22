#include "renderer/schedule.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "err.h"

namespace ysm::renderer {
namespace {
struct PartitionGroupCounts {
    size_t cutout = 0;
    size_t cutout_no_culling = 0;
    size_t translucent = 0;
    size_t translucent_culling = 0;
};

struct InlineThreshold {
    size_t bones;
    size_t cube_groups;
};

constexpr InlineThreshold InlineThresholdFor(size_t worker_count) noexcept {
    if (worker_count == 2) {
        return {4, 6};
    }
    if (worker_count < 7) {
        return {6, 6};
    }
    return {6, 8};
}

template <bool kCulling>
uint32_t VertexCount(const bake::BakedModelBones::BonePartitionInfo& bone,
                     size_t begin, size_t end) noexcept {
    const auto count_at = [&](size_t position) {
        if (position == 0) {
            return 0U;
        }
        const auto& info = bone.cube_group_info[position - 1];
        if constexpr (kCulling) {
            return info.quad_count_after_culling;
        } else {
            return info.quad_count;
        }
    };
    return (count_at(end) - count_at(begin)) * 4U;
}

template <bool kCulling>
absl::Status SchedulePartition(
    const bake::BakedModelBones& bones,
    bake::BakedModelBones::BonePartitionInfo bake::BakedModelBones::BoneInfo::*
        partition,
    size_t group_count, std::span<const uint16_t> render_bone_indices,
    RenderTask::Partition RenderTask::* task_partition,
    std::span<RenderTask> tasks, uint64_t& vertex_offset) {
    size_t worker = 0;
    size_t scheduled_group_count = 0;
    size_t task_end = 0;
    const auto prepare_worker = [&] {
        const auto [begin, end] =
            DetermineTaskRange(worker, tasks.size(), group_count);
        auto& output = tasks[worker].*task_partition;

        output.cube_indices.clear();
        output.vertex_offset = 0;
        output.expected_vertex_count = 0;

        output.cube_indices.reserve(end - begin);
        if (vertex_offset > std::numeric_limits<uint32_t>::max()) [[unlikely]] {
            return false;
        }
        output.vertex_offset = static_cast<uint32_t>(vertex_offset);
        task_end = end;
        return true;
    };
    const auto finish_worker = [&] {
        vertex_offset += (tasks[worker].*task_partition).expected_vertex_count;
        return vertex_offset <= std::numeric_limits<uint32_t>::max();
    };
    const auto advance_worker = [&] {
        while (scheduled_group_count >= task_end && worker + 1 < tasks.size()) {
            if (!finish_worker()) [[unlikely]] {
                return false;
            }
            ++worker;
            if (!prepare_worker()) [[unlikely]] {
                return false;
            }
        }
        return true;
    };
    YSM_ASSERT(prepare_worker(), absl::ResourceExhaustedError(
                                     "Render schedule vertex count exceeds "
                                     "uint32_t."));

    for (const auto bone_index : render_bone_indices) {
        const auto& bone = bones.list[bone_index].*partition;
        size_t bone_group_begin = 0;
        while (bone_group_begin < bone.cube_indices.size()) {
            if (!advance_worker()) [[unlikely]] {
                return absl::ResourceExhaustedError(
                    "Render schedule vertex count exceeds uint32_t.");
            }

            const auto group_count_for_task =
                std::min(bone.cube_indices.size() - bone_group_begin,
                         task_end - scheduled_group_count);
            const auto bone_group_end = bone_group_begin + group_count_for_task;
            const auto vertex_count =
                VertexCount<kCulling>(bone, bone_group_begin, bone_group_end);
            auto& output = tasks[worker].*task_partition;
            if (vertex_count > std::numeric_limits<uint32_t>::max() -
                                   output.expected_vertex_count) [[unlikely]] {
                return absl::ResourceExhaustedError(
                    "Render task vertex count exceeds uint32_t.");
            }

            const auto selected = bone.cube_indices.subspan(
                bone_group_begin, group_count_for_task);
            output.cube_indices.insert(output.cube_indices.end(),
                                       selected.begin(), selected.end());
            output.expected_vertex_count += vertex_count;
            scheduled_group_count += group_count_for_task;
            bone_group_begin = bone_group_end;
        }
    }

    while (worker + 1 < tasks.size()) {
        YSM_ASSERT(finish_worker(),
                   absl::ResourceExhaustedError(
                       "Render schedule vertex count exceeds uint32_t."));
        ++worker;
        YSM_ASSERT(prepare_worker(),
                   absl::ResourceExhaustedError(
                       "Render schedule vertex count exceeds uint32_t."));
    }
    YSM_ASSERT(finish_worker(),
               absl::ResourceExhaustedError(
                   "Render schedule vertex count exceeds uint32_t."));
    return OkStatus();
}

absl::Status SchedulePartitions(const bake::BakedModelBones& bones,
                                const PartitionGroupCounts& group_counts,
                                std::span<const uint16_t> render_bone_indices,
                                std::span<RenderTask> tasks,
                                uint64_t& opaque_vertex_count,
                                uint64_t& translucent_vertex_count) {
    YSM_RETURN_IF_ERROR(SchedulePartition<true>(
        bones, &bake::BakedModelBones::BoneInfo::cutout, group_counts.cutout,
        render_bone_indices, &RenderTask::cutout, tasks, opaque_vertex_count));
    YSM_RETURN_IF_ERROR(SchedulePartition<false>(
        bones, &bake::BakedModelBones::BoneInfo::cutout_no_culling,
        group_counts.cutout_no_culling, render_bone_indices,
        &RenderTask::cutout_no_culling, tasks, opaque_vertex_count));
    YSM_RETURN_IF_ERROR(SchedulePartition<false>(
        bones, &bake::BakedModelBones::BoneInfo::translucent,
        group_counts.translucent, render_bone_indices, &RenderTask::translucent,
        tasks, translucent_vertex_count));
    YSM_RETURN_IF_ERROR(SchedulePartition<true>(
        bones, &bake::BakedModelBones::BoneInfo::translucent_culling,
        group_counts.translucent_culling, render_bone_indices,
        &RenderTask::translucent_culling, tasks, translucent_vertex_count));
    return OkStatus();
}
}  // namespace

RenderSchedulingMode DetermineRenderSchedulingMode(
    size_t worker_count, size_t render_bone_count,
    size_t cube_group_count) noexcept {
    if (worker_count <= 1) {
        return RenderSchedulingMode::kInline;
    }
    const auto inline_threshold = InlineThresholdFor(worker_count);
    if (render_bone_count <= inline_threshold.bones &&
        cube_group_count <= inline_threshold.cube_groups) {
        return RenderSchedulingMode::kInline;
    }
    constexpr size_t kMaxWorkerReadyWorkers = 64;
    if (worker_count <= kMaxWorkerReadyWorkers &&
        render_bone_count >= worker_count * 32) {
        return RenderSchedulingMode::kWorkerReadySpin;
    }
    if (worker_count <= 3) {
        return RenderSchedulingMode::kSerialLateWake;
    }
    return RenderSchedulingMode::kSerialPrewake;
}

absl::Status RenderSchedule::Update(
    const bake::BakedModelBones& bones,
    std::span<const uint16_t> render_bone_indices, size_t worker_count) {
    YSM_ASSERT(worker_count != 0,
               absl::InternalError("Renderer has no workers."));
    YSM_ASSERT(worker_count <=
                   static_cast<size_t>(std::numeric_limits<uint16_t>::max()) +
                       1,
               absl::InvalidArgumentError("Too many renderer workers."));
    vertex_count = 0;
    translucent_vertex_count = 0;
    translucent_vertex_offset = 0;

    const auto bone_count = bones.list.size();

    PartitionGroupCounts group_counts;
    for (const auto bone_index : render_bone_indices) {
        YSM_ASSERT(
            bone_index < bone_count,
            absl::InvalidArgumentError("Render bone index out of range."));
        const auto& bone = bones.list[bone_index];
        const auto accumulate = [&](const auto& partition, size_t& count) {
            if (partition.cube_group_info.size() !=
                partition.cube_indices.size()) [[unlikely]] {
                return false;
            }
            count += partition.cube_indices.size();
            return true;
        };
        YSM_ASSERT(accumulate(bone.cutout, group_counts.cutout) &&
                       accumulate(bone.cutout_no_culling,
                                  group_counts.cutout_no_culling) &&
                       accumulate(bone.translucent, group_counts.translucent) &&
                       accumulate(bone.translucent_culling,
                                  group_counts.translucent_culling),
                   absl::DataLossError(
                       "Inconsistent cube group cache in baked model."));
    }
    auto cube_group_count =
        group_counts.cutout + group_counts.cutout_no_culling +
        group_counts.translucent + group_counts.translucent_culling;
    mode = DetermineRenderSchedulingMode(worker_count, render_bone_indices.size(),
                                         cube_group_count);
    tasks.resize(worker_count);
    if (mode == RenderSchedulingMode::kWorkerReadySpin) {
        bone_update_owners.assign(bones.list.size(), UINT8_MAX);
        for (size_t worker_index = 0; worker_index < worker_count;
             ++worker_index) {
            const auto [begin, end] = DetermineTaskRange(
                worker_index, worker_count, render_bone_indices.size());
            for (size_t index = begin; index < end; ++index) {
                bone_update_owners[render_bone_indices[index]] =
                    static_cast<uint8_t>(worker_index);
            }
        }
    } else {
        bone_update_owners.clear();
    }

    uint64_t opaque_vertex_count = 0;
    uint64_t translucent_vertex_count64 = 0;
    YSM_RETURN_IF_ERROR(
        SchedulePartitions(bones, group_counts, render_bone_indices,
                            mode != RenderSchedulingMode::kInline ? tasks :
                                std::span{tasks.data(), 1},
                           opaque_vertex_count, translucent_vertex_count64));
    YSM_ASSERT(opaque_vertex_count + translucent_vertex_count64 <=
                   std::numeric_limits<uint32_t>::max(),
               absl::ResourceExhaustedError(
                   "Render schedule vertex count exceeds uint32_t."));

    vertex_count =
        static_cast<uint32_t>(opaque_vertex_count + translucent_vertex_count64);
    translucent_vertex_count = translucent_vertex_count64;
    translucent_vertex_offset = static_cast<uint32_t>(opaque_vertex_count);

    return OkStatus();
}
}  // namespace ysm::renderer
