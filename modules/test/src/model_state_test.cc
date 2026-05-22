#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numbers>
#include <ranges>
#include <vector>

#include <gtest/gtest.h>

#include "bake/baked_model.h"
#include "buffer.h"
#include "cpu.h"
#include "renderer/model_state.h"
#include "renderer/render.h"
#include "renderer/vertex/iris_56.h"
#include "renderer/vertex/vanilla.h"

namespace ysm::test {
namespace {
using Group = bake::CubeGroup<simd::Width::B128, false>;
constexpr simd::Tag<simd::Type::kNone> kGenericTag;

Group MakeQuadGroup(uint32_t bone_index) {
    Group group;
    group.bone_index = bone_index;
    group.cube_count = 1;
    group.cube_attr[0].quad_count = 1;
    group.cube_attr[0].quad_count_after_culling = 1;
    group.normal[1][0] = 1.0f;
    group.winding_sign[0] = 1.0f;
    const std::array<std::array<float, 3>, 4> positions{{
        {0, 0, 0},
        {1, 0, 0},
        {1, 1, 0},
        {0, 1, 0},
    }};
    for (size_t vertex = 0; vertex < positions.size(); ++vertex) {
        for (size_t axis = 0; axis < 3; ++axis) {
            group.pos[axis][vertex] = positions[vertex][axis];
        }
        group.vertex_index[vertex][0] = static_cast<uint32_t>(vertex);
    }
    return group;
}

std::shared_ptr<bake::BakedModel> MakeModel() {
    bake::BakedModelCubes<simd::Width::B128> cubes;
    cubes.cutout_no_culling = {MakeQuadGroup(1), MakeQuadGroup(3)};

    bake::BakedModelBones bones;
    bones.list.resize(4);
    bones.list[0].parent_index = UINT32_MAX;
    bones.list[0].subtree_end = 4;
    bones.list[0].depth = 0;
    bones.list[1].parent_index = 0;
    bones.list[1].subtree_end = 3;
    bones.list[1].depth = 1;
    bones.list[1].pivot = {16, 0, 0};
    bones.list[2].parent_index = 1;
    bones.list[2].subtree_end = 3;
    bones.list[2].depth = 2;
    bones.list[2].pivot = {0, 16, 0};
    bones.list[3].parent_index = 0;
    bones.list[3].subtree_end = 4;
    bones.list[3].depth = 1;
    for (auto& bone : bones.list) {
        bone.solid = true;
    }

    bones.cube_indices_cache = {0, 1};
    bones.cube_group_info_cache = {{1, 1, 1}, {1, 1, 1}};
    auto& first = bones.list[1].cutout_no_culling;
    first.cube_indices = std::span(bones.cube_indices_cache).subspan(0, 1);
    first.cube_group_info =
        std::span(bones.cube_group_info_cache).subspan(0, 1);
    first.cube_count = 1;
    first.full_vertex_count = 4;
    first.culling_vertex_count = 4;
    auto& second = bones.list[3].cutout_no_culling;
    second.cube_indices = std::span(bones.cube_indices_cache).subspan(1, 1);
    second.cube_group_info =
        std::span(bones.cube_group_info_cache).subspan(1, 1);
    second.cube_count = 1;
    second.full_vertex_count = 4;
    second.culling_vertex_count = 4;
    return std::make_shared<bake::BakedModel>(bake::BakedModelInfo{},
                                              std::move(bones),
                                              std::move(cubes));
}

std::shared_ptr<bake::BakedModel> MakeFlatModel(size_t bone_count) {
    bake::BakedModelCubes<simd::Width::B128> cubes;
    cubes.cutout_no_culling.reserve(bone_count);
    for (size_t bone_index = 0; bone_index < bone_count; ++bone_index) {
        cubes.cutout_no_culling.push_back(
            MakeQuadGroup(static_cast<uint32_t>(bone_index)));
    }

    bake::BakedModelBones bones;
    bones.list.resize(bone_count);
    bones.sorted_bone_indices.reserve(bone_count);
    bones.cube_indices_cache.resize(bone_count);
    bones.cube_group_info_cache.resize(bone_count);
    for (size_t bone_index = 0; bone_index < bone_count; ++bone_index) {
        bones.sorted_bone_indices.push_back(
            static_cast<uint16_t>(bone_index));
        bones.cube_indices_cache[bone_index] =
            static_cast<uint32_t>(bone_index);
        bones.cube_group_info_cache[bone_index] = {1, 1, 1};

        auto& bone = bones.list[bone_index];
        bone.parent_index = UINT32_MAX;
        bone.subtree_end = static_cast<uint32_t>(bone_index + 1);
        bone.depth = 0;
        bone.solid = true;
        auto& partition = bone.cutout_no_culling;
        partition.cube_indices =
            std::span(bones.cube_indices_cache).subspan(bone_index, 1);
        partition.cube_group_info =
            std::span(bones.cube_group_info_cache).subspan(bone_index, 1);
        partition.cube_count = 1;
        partition.full_vertex_count = 4;
        partition.culling_vertex_count = 4;
    }
    return std::make_shared<bake::BakedModel>(bake::BakedModelInfo{},
                                              std::move(bones),
                                              std::move(cubes));
}

std::vector<renderer::BoneAttribute> MakeAttributes() {
    std::vector<renderer::BoneAttribute> attributes(4);
    for (auto& attribute : attributes) {
        attribute.scale[0] = 1.0f;
        attribute.scale[1] = 1.0f;
        attribute.scale[2] = 1.0f;
    }
    attributes[2].locator_sequence = 5.0f;
    return attributes;
}

std::vector<renderer::BoneAttribute> MakeFlatAttributes(size_t bone_count) {
    std::vector<renderer::BoneAttribute> attributes(bone_count);
    for (size_t bone_index = 0; bone_index < bone_count; ++bone_index) {
        auto& attribute = attributes[bone_index];
        attribute.position[0] = static_cast<float>(bone_index) * -16.0f;
        attribute.scale[0] = 1.0f;
        attribute.scale[1] = 1.0f;
        attribute.scale[2] = 1.0f;
    }
    return attributes;
}
}  // namespace

TEST(ModelStateTest, ExtractsPoseLocatorsAndReusesSchedule) {
    InitCpuInfo();
#ifdef YSM_X64
    simd::Mock(simd::Type::SSE41);
#endif
    auto model = MakeModel();
    auto attributes = MakeAttributes();
    attributes[0].position[0] = 16.0f;
    attributes[1].rotation[2] = std::numbers::pi_v<float> * 0.5f;
    attributes[1].scale[0] = 2.0f;
    std::vector<math::PoseStack::Pose> poses(4);
    renderer::ModelState state;

    auto first =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    ASSERT_TRUE(first.ok()) << first.status();
    EXPECT_TRUE(first->schedule_updated);
    EXPECT_EQ(first->vertex_count, 8);
    EXPECT_EQ(first->locator_count, 1);
    EXPECT_EQ(state.StagedLocatorBoneIndices(first->locator_count)[0], 2);
    EXPECT_TRUE(state.IsValid());
    EXPECT_TRUE(std::ranges::equal(state.PoseView().render_bone_indices,
                                   std::array<uint16_t, 2>{1, 3}));

    const auto& child = poses[1];
    EXPECT_NEAR(child.pose[0][0], 0.0f, 0.00001f);
    EXPECT_NEAR(child.pose[0][1], 2.0f, 0.00001f);
    EXPECT_NEAR(child.pose[1][0], -1.0f, 0.00001f);
    EXPECT_NEAR(child.pose[3][0], 0.0f, 0.00001f);
    EXPECT_NEAR(child.pose[3][1], -2.0f, 0.00001f);
    EXPECT_NEAR(child.normal[0][1], 0.5f, 0.00001f);
    EXPECT_NEAR(child.normal[1][0], -1.0f, 0.00001f);
    const auto& sibling = poses[3];
    EXPECT_FLOAT_EQ(sibling.pose[0][0], 1.0f);
    EXPECT_FLOAT_EQ(sibling.pose[0][1], 0.0f);
    EXPECT_FLOAT_EQ(sibling.pose[3][0], -1.0f);
    EXPECT_FLOAT_EQ(sibling.normal[0][0], 1.0f);
    EXPECT_FLOAT_EQ(sibling.normal[0][1], 0.0f);

    attributes[1].position[0] = 4.0f;
    auto second =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    ASSERT_TRUE(second.ok()) << second.status();
    EXPECT_FALSE(second->schedule_updated);
    EXPECT_EQ(second->vertex_count, 8);
    EXPECT_EQ(second->locator_count, 1);

    auto other_model = MakeModel();
    auto switched =
        state.Extract(kGenericTag, other_model, attributes, poses.size(), poses);
    ASSERT_TRUE(switched.ok()) << switched.status();
    EXPECT_TRUE(switched->schedule_updated);
    EXPECT_EQ(state.Model(), other_model);
}

TEST(ModelStateTest, DetectsEqualSizedBoneIndexChange) {
    InitCpuInfo();
#ifdef YSM_X64
    simd::Mock(simd::Type::SSE41);
#endif
    auto model = MakeModel();
    auto attributes = MakeAttributes();
    std::vector<math::PoseStack::Pose> poses(4);
    renderer::ModelState state;

    attributes[1].cubes_hidden = 1.0f;
    auto first =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    ASSERT_TRUE(first.ok()) << first.status();
    EXPECT_TRUE(first->schedule_updated);
    EXPECT_TRUE(std::ranges::equal(state.PoseView().render_bone_indices,
                                   std::array<uint16_t, 1>{3}));

    attributes[1].cubes_hidden = 0.0f;
    attributes[3].cubes_hidden = 1.0f;
    auto changed =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    ASSERT_TRUE(changed.ok()) << changed.status();
    EXPECT_TRUE(changed->schedule_updated);
    EXPECT_TRUE(std::ranges::equal(state.PoseView().render_bone_indices,
                                   std::array<uint16_t, 1>{1}));

    auto unchanged =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    ASSERT_TRUE(unchanged.ok()) << unchanged.status();
    EXPECT_FALSE(unchanged->schedule_updated);
}

TEST(ModelStateTest, PrunesHiddenInvalidAndZeroScaleSubtrees) {
    InitCpuInfo();
#ifdef YSM_X64
    simd::Mock(simd::Type::SSE41);
#endif
    auto model = MakeModel();
    auto attributes = MakeAttributes();
    std::vector<math::PoseStack::Pose> poses(4);
    renderer::ModelState state;

    attributes[1].children_hidden = 1.0f;
    auto children_hidden =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    ASSERT_TRUE(children_hidden.ok()) << children_hidden.status();
    EXPECT_EQ(children_hidden->vertex_count, 8);
    EXPECT_EQ(children_hidden->locator_count, 0);

    attributes[1].children_hidden = 0.0f;
    attributes[1].scale[0] = 0.0f;
    auto zero_scale =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    ASSERT_TRUE(zero_scale.ok()) << zero_scale.status();
    EXPECT_EQ(zero_scale->vertex_count, 4);
    EXPECT_EQ(zero_scale->locator_count, 0);
    EXPECT_TRUE(std::ranges::equal(state.PoseView().render_bone_indices,
                                   std::array<uint16_t, 1>{3}));

    attributes = MakeAttributes();
    attributes[0].rotation[0] = std::numeric_limits<float>::infinity();
    auto non_finite =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    ASSERT_TRUE(non_finite.ok()) << non_finite.status();
    EXPECT_EQ(non_finite->vertex_count, 0);
    EXPECT_EQ(non_finite->locator_count, 0);

    attributes = MakeAttributes();
    attributes[2].locator_sequence = 1.5f;
    auto invalid_locator =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    EXPECT_EQ(invalid_locator.status().code(),
              absl::StatusCode::kInvalidArgument);
    EXPECT_FALSE(state.IsValid());
}

TEST(ModelStateTest, MaintainsBoneNormalInParallel) {
    InitCpuInfo();
#ifdef YSM_X64
    simd::Mock(simd::Type::SSE41);
#endif
    auto model = MakeModel();
    auto attributes = MakeAttributes();
    std::vector<math::PoseStack::Pose> poses(4);
    renderer::ModelState state;

    attributes[0].scale[0] = -2.0f;
    attributes[0].scale[1] = -2.0f;
    attributes[0].scale[2] = -2.0f;
    auto negative_uniform =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    ASSERT_TRUE(negative_uniform.ok()) << negative_uniform.status();
    EXPECT_FLOAT_EQ(poses[0].normal[0][0], -1.0f);
    EXPECT_FLOAT_EQ(poses[0].normal[1][1], -1.0f);
    EXPECT_FLOAT_EQ(poses[0].normal[2][2], -1.0f);
    EXPECT_TRUE(poses[0].uniform_scale);
    EXPECT_FLOAT_EQ(poses[0].tangent_orientation, -1.0f);
    EXPECT_FLOAT_EQ(poses[0].normal_scale, 2.0f);

    attributes[0].scale[0] = -2.0f;
    attributes[0].scale[1] = 2.0f;
    attributes[0].scale[2] = 2.0f;
    auto signed_uniform =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    ASSERT_TRUE(signed_uniform.ok()) << signed_uniform.status();
    EXPECT_FLOAT_EQ(poses[0].normal[0][0], -1.0f);
    EXPECT_FLOAT_EQ(poses[0].normal[1][1], 1.0f);
    EXPECT_FLOAT_EQ(poses[0].normal[2][2], 1.0f);
    EXPECT_TRUE(poses[0].uniform_scale);
    EXPECT_FLOAT_EQ(poses[0].tangent_orientation, -1.0f);
    EXPECT_FLOAT_EQ(poses[0].normal_scale, 2.0f);

    attributes[0].scale[0] = -2.0f;
    attributes[0].scale[1] = 3.0f;
    attributes[0].scale[2] = 4.0f;
    auto non_uniform =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    ASSERT_TRUE(non_uniform.ok()) << non_uniform.status();
    EXPECT_FLOAT_EQ(poses[0].normal[0][0], -0.5f);
    EXPECT_FLOAT_EQ(poses[0].normal[1][1], 1.0f / 3.0f);
    EXPECT_FLOAT_EQ(poses[0].normal[2][2], 0.25f);
    EXPECT_FALSE(poses[0].uniform_scale);
    EXPECT_FLOAT_EQ(poses[0].tangent_orientation, -1.0f);
    EXPECT_FLOAT_EQ(poses[0].normal_scale, 1.0f);
}

TEST(ModelStateTest, SnapshotRendersRepeatedlyWithTrustedNormalizedNormal) {
    InitCpuInfo();
#ifdef YSM_X64
    simd::Mock(simd::Type::SSE41);
#endif
    auto model = MakeModel();
    auto attributes = MakeAttributes();
    std::vector<math::PoseStack::Pose> poses(4);
    renderer::ModelState state;
    auto extracted =
        state.Extract(kGenericTag, model, attributes, poses.size(), poses);
    ASSERT_TRUE(extracted.ok()) << extracted.status();

    renderer::RenderParameters parameters{};
    glm_mat4_identity(parameters.model);
    glm_mat4_identity(parameters.view);
    glm_mat4_identity(parameters.projection);
    parameters.ctx = renderer::RenderContext::kLevel;
    parameters.light = 0x1234abcd;
    parameters.overlay = 0x01020304;
    parameters.color.packed = 0xffffffff;
    std::vector<Byte> first(extracted->vertex_count *
                            renderer::vertex::VanillaVertex::kSize);
    std::vector<Byte> second(first.size());

    ASSERT_TRUE(renderer::Render(first, renderer::VertexKind::kVanilla, state,
                                 parameters)
                    .ok());
    ASSERT_TRUE(renderer::Render(second, renderer::VertexKind::kVanilla, state,
                                 parameters)
                    .ok());
    EXPECT_EQ(first, second);
    const auto* vertices =
        reinterpret_cast<const renderer::vertex::VanillaVertex*>(first.data());
    EXPECT_EQ(vertices[0].light, parameters.light);
    EXPECT_EQ(vertices[0].normal & 0xffU, 0U);
    EXPECT_EQ((vertices[0].normal >> 16) & 0xffU, 0U);
    const auto packed_y = (vertices[0].normal >> 8) & 0xffU;
    EXPECT_TRUE(packed_y == 0x7fU || packed_y == 0x81U);

    parameters.ctx = renderer::RenderContext::kIrisShadow;
    std::vector<Byte> shadow(extracted->vertex_count *
                             renderer::vertex::Iris56Vertex::kSize);
    ASSERT_TRUE(renderer::Render(shadow, renderer::VertexKind::kIris56, state,
                                 parameters)
                    .ok());
    const auto* shadow_vertices =
        reinterpret_cast<const renderer::vertex::Iris56Vertex*>(shadow.data());
    EXPECT_NE(shadow_vertices[0].normal, 0U);
    EXPECT_EQ(shadow_vertices[0].color, 0U);
    EXPECT_EQ(shadow_vertices[0].tangent, 0U);
}

TEST(ModelStateTest, ProductionPolicyRendersPrewakeAndWorkerReadySchedules) {
    InitCpuInfo();
#ifdef YSM_X64
    simd::Mock(simd::Type::SSE41);
#endif
    renderer::RenderParameters parameters{};
    glm_mat4_identity(parameters.model);
    glm_mat4_identity(parameters.view);
    glm_mat4_identity(parameters.projection);
    parameters.ctx = renderer::RenderContext::kLevel;
    parameters.light = 0x1234abcd;
    parameters.color.packed = 0xffffffff;

    const auto run_case =
        [&](size_t bone_count, renderer::RenderSchedulingMode expected_mode,
            size_t repetitions, renderer::ModelState& state,
            std::vector<math::PoseStack::Pose>& poses,
            std::vector<Byte>& output) {
        auto model = MakeFlatModel(bone_count);
        auto attributes = MakeFlatAttributes(bone_count);
        poses.resize(bone_count);
        auto extracted =
            state.Extract(kGenericTag, model, attributes, poses.size(), poses);
        if (!extracted.ok()) {
            ADD_FAILURE() << extracted.status();
            return false;
        }
        if (state.Schedule().mode != expected_mode ||
            extracted->vertex_count != bone_count * 4) {
            ADD_FAILURE() << "Unexpected production schedule.";
            return false;
        }

        output.resize(extracted->vertex_count *
                      renderer::vertex::VanillaVertex::kSize);
        for (size_t repetition = 0; repetition < repetitions; ++repetition) {
            const auto status =
                renderer::Render(output, renderer::VertexKind::kVanilla,
                                 state, parameters);
            if (!status.ok()) {
                ADD_FAILURE() << status;
                return false;
            }
        }

        const auto* vertices =
            reinterpret_cast<const renderer::vertex::VanillaVertex*>(
                output.data());
        for (size_t bone_index = 0; bone_index < bone_count; ++bone_index) {
            const auto& first_vertex = vertices[bone_index * 4];
            EXPECT_FLOAT_EQ(first_vertex.pos[0],
                            static_cast<float>(bone_index));
            EXPECT_FLOAT_EQ(first_vertex.pos[1], 0.0f);
            EXPECT_FLOAT_EQ(first_vertex.pos[2], 0.0f);
            EXPECT_EQ(first_vertex.light, parameters.light);
        }
        return true;
    };

    renderer::ModelState prewake_state;
    std::vector<math::PoseStack::Pose> prewake_poses;
    std::vector<Byte> prewake_output;
    ASSERT_TRUE(run_case(9, renderer::RenderSchedulingMode::kSerialPrewake, 8,
                         prewake_state, prewake_poses, prewake_output));

    renderer::ModelState worker_ready_state;
    std::vector<math::PoseStack::Pose> worker_ready_poses;
    std::vector<Byte> worker_ready_output;
    ASSERT_TRUE(run_case(224, renderer::RenderSchedulingMode::kWorkerReadySpin,
                         32, worker_ready_state, worker_ready_poses,
                         worker_ready_output));

    const auto previous = worker_ready_poses.back().pose[0][0];
    worker_ready_poses.back().pose[0][0] =
        std::numeric_limits<float>::infinity();
    const auto error =
        renderer::Render(worker_ready_output, renderer::VertexKind::kVanilla,
                         worker_ready_state, parameters);
    EXPECT_EQ(error.code(), absl::StatusCode::kInternal);
    worker_ready_poses.back().pose[0][0] = previous;
    EXPECT_TRUE(renderer::Render(worker_ready_output,
                                 renderer::VertexKind::kVanilla,
                                 worker_ready_state, parameters)
                    .ok());
}
}  // namespace ysm::test
