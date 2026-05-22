#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <ranges>
#include <span>
#include <vector>

#include "cpu.h"
#include "renderer/render.h"
#include "renderer/render_state.h"

namespace ysm::test {
namespace {
constexpr simd::Tag<simd::Type::kNone> kGenericTag;

float ProjectedArea(mat4 clip_from_local, const vec3 p0, const vec3 p1,
                    const vec3 p2) {
    vec4 clip[3];
    vec4 points[3]{
        {p0[0], p0[1], p0[2], 1.0f},
        {p1[0], p1[1], p1[2], 1.0f},
        {p2[0], p2[1], p2[2], 1.0f},
    };
    for (size_t i = 0; i < 3; ++i) {
        glm_mat4_mulv(clip_from_local, points[i], clip[i]);
    }
    const auto x0 = clip[0][0] / clip[0][3];
    const auto y0 = clip[0][1] / clip[0][3];
    const auto x1 = clip[1][0] / clip[1][3];
    const auto y1 = clip[1][1] / clip[1][3];
    const auto x2 = clip[2][0] / clip[2][3];
    const auto y2 = clip[2][1] / clip[2][3];
    return (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
}

float Determinant3(float a0, float a1, float a2, float b0, float b1, float b2,
                   float c0, float c1, float c2) {
    return a0 * (b1 * c2 - b2 * c1) - a1 * (b0 * c2 - b2 * c0) +
           a2 * (b0 * c1 - b1 * c0);
}

void ReferenceFacingCoefficient(const mat4 matrix, vec4 destination) {
    vec4 x;
    vec4 y;
    vec4 w;
    for (size_t column = 0; column < 4; ++column) {
        x[column] = matrix[column][0];
        y[column] = matrix[column][1];
        w[column] = matrix[column][3];
    }
    destination[0] =
        Determinant3(x[1], x[2], x[3], y[1], y[2], y[3], w[1], w[2], w[3]);
    destination[1] =
        -Determinant3(x[0], x[2], x[3], y[0], y[2], y[3], w[0], w[2], w[3]);
    destination[2] =
        Determinant3(x[0], x[1], x[3], y[0], y[1], y[3], w[0], w[1], w[3]);
    destination[3] =
        -Determinant3(x[0], x[1], x[2], y[0], y[1], y[2], w[0], w[1], w[2]);
}

struct RenderStateInput {
    std::shared_ptr<bake::BakedModel> model;
    std::vector<math::PoseStack::Pose> poses;
    renderer::ModelState model_state;
};

absl::StatusOr<std::unique_ptr<RenderStateInput>> MakeRenderStateInput(
    std::span<const math::PoseStack::Pose> source_poses,
    std::span<const uint16_t> render_bone_indices,
    const bake::BakedModelBones& source_bones, bool has_pbr) {
    if (source_bones.list.size() != source_poses.size()) {
        return absl::InvalidArgumentError("Inconsistent test bone count.");
    }

    auto output = std::make_unique<RenderStateInput>();
    bake::BakedModelCubes<simd::Width::B128> cubes;
    cubes.cutout_no_culling.resize(render_bone_indices.size());
    bake::BakedModelBones bones;
    bones.list.resize(source_poses.size());
    bones.sorted_bone_indices.resize(source_poses.size());
    bones.cube_indices_cache.resize(render_bone_indices.size());
    bones.cube_group_info_cache.resize(render_bone_indices.size());

    for (size_t bone_index = 0; bone_index < bones.list.size(); ++bone_index) {
        bones.sorted_bone_indices[bone_index] =
            static_cast<uint16_t>(bone_index);
        auto& bone = bones.list[bone_index];
        bone.parent_index = UINT32_MAX;
        bone.subtree_end = static_cast<uint32_t>(bone_index + 1);
        bone.depth = 0;
        bone.solid = source_bones.list[bone_index].solid;
    }
    for (size_t position = 0; position < render_bone_indices.size();
         ++position) {
        const auto bone_index = render_bone_indices[position];
        if (bone_index >= bones.list.size()) {
            return absl::InvalidArgumentError("Invalid test render bone.");
        }

        auto& group = cubes.cutout_no_culling[position];
        group.bone_index = bone_index;
        group.cube_count = 1;
        group.cube_attr[0].quad_count = 1;
        group.cube_attr[0].quad_count_after_culling = 1;
        bones.cube_indices_cache[position] = static_cast<uint32_t>(position);
        bones.cube_group_info_cache[position] = {1, 1, 1};

        auto& partition = bones.list[bone_index].cutout_no_culling;
        partition.cube_indices =
            std::span(bones.cube_indices_cache).subspan(position, 1);
        partition.cube_group_info =
            std::span(bones.cube_group_info_cache).subspan(position, 1);
        partition.cube_count = 1;
        partition.full_vertex_count = 4;
        partition.culling_vertex_count = 4;
    }

    output->model = std::make_shared<bake::BakedModel>(
        bake::BakedModelInfo{.has_pbr = has_pbr}, std::move(bones),
        std::move(cubes));
    std::vector<renderer::BoneAttribute> attributes(source_poses.size());
    for (auto& attribute : attributes) {
        attribute.scale[0] = 1.0f;
        attribute.scale[1] = 1.0f;
        attribute.scale[2] = 1.0f;
    }
    output->poses.resize(source_poses.size());
    auto extracted = output->model_state.Extract(
        kGenericTag, output->model, attributes, output->poses.size(),
        output->poses);
    if (!extracted.ok()) {
        return extracted.status();
    }
    std::copy(source_poses.begin(), source_poses.end(), output->poses.begin());
    return output;
}

absl::Status UpdateRenderState(renderer::RenderState& output,
                               const renderer::RenderParameters& params,
                               const RenderStateInput& input) {
    auto status =
        output.UpdateCommon(kGenericTag, params, input.model_state);
    if (!status.ok()) {
        return status;
    }
    return output.Update(kGenericTag, params, input.model_state, 0, 1);
}

}  // namespace

TEST(RenderStateTest, Update) {
    renderer::RenderParameters params{};
    glm_mat4_identity(params.model);
    params.model[0][0] = -2.0f;
    params.model[1][1] = 3.0f;
    params.model[2][2] = 0.5f;
    params.model[3][0] = 1.0f;
    params.model[3][2] = -5.0f;
    glm_mat4_identity(params.view);
    glm_mat4_zero(params.projection);
    params.projection[0][0] = 1.0f;
    params.projection[1][1] = 1.0f;
    params.projection[2][2] = -11.0f / 9.0f;
    params.projection[2][3] = -1.0f;
    params.projection[3][2] = -20.0f / 9.0f;
    params.light = 0x1234abcd;

    std::vector<math::PoseStack::Pose> bone_states(1);
    const std::vector<uint16_t> bone_indices{0};
    renderer::ModelPoseView source{bone_states, bone_indices};
    glm_mat4_identity(bone_states[0].pose);
    bone_states[0].pose[0][0] = 0.0f;
    bone_states[0].pose[0][1] = 1.0f;
    bone_states[0].pose[1][0] = -1.0f;
    bone_states[0].pose[1][1] = 0.0f;
    bone_states[0].pose[3][0] = 0.25f;
    bone_states[0].pose[3][1] = -0.5f;
    glm_mat3_identity(bone_states[0].normal);
    bone_states[0].normal[0][0] = 0.0f;
    bone_states[0].normal[0][1] = 1.0f;
    bone_states[0].normal[1][0] = -1.0f;
    bone_states[0].normal[1][1] = 0.0f;

    bake::BakedModelBones bones;
    bones.list.resize(1);
    bones.list[0].solid = false;
    auto input = MakeRenderStateInput(bone_states, bone_indices, bones, true);
    ASSERT_TRUE(input.ok()) << input.status();
    renderer::RenderState transformed;

    mat4 clip_from_local;
    mat4 view_model;
    mat4 clip_model;
    const vec3 p0{0.0f, 0.0f, 0.0f};
    const vec3 p1{1.0f, 0.0f, 0.0f};
    const vec3 p2{1.0f, 1.0f, 0.0f};
    vec4 plane{0.0f, 0.0f, 1.0f, 0.0f};

    {
        ASSERT_TRUE(UpdateRenderState(transformed, params, **input).ok());
        const auto& state = transformed.GetBoneState(0);
        EXPECT_FLOAT_EQ(state.tangent_orientation, -1.0f);
        EXPECT_EQ(state.packed_light, params.light);
        EXPECT_FALSE(state.uniform_scale);

        glm_mat4_mul(params.view, params.model, view_model);
        glm_mat4_mul(params.projection, view_model, clip_model);
        glm_mat4_mul(clip_model, source.bone_poses[0].pose, clip_from_local);
        const auto area = ProjectedArea(clip_from_local, p0, p1, p2);
        const auto facing = glm_vec4_dot(plane, state.facing_coeff);
        EXPECT_LT(area, 0.0f);
        EXPECT_LT(facing, 0.0f);

        vec4 center{0.5f, 0.5f, 0.0f, 1.0f};
        vec4 clip_center;
        glm_mat4_mulv(clip_from_local, center, clip_center);
        const auto depth = glm_vec4_dot(state.depth_z, center) /
                           glm_vec4_dot(state.depth_w, center);
        EXPECT_NEAR(depth, clip_center[2] / clip_center[3], 0.0001f);
    }

    {
        glm_mat4_identity(params.model);
        glm_mat4_identity(params.projection);
        params.projection[1][1] = -1.0f;
        ASSERT_TRUE(UpdateRenderState(transformed, params, **input).ok());
        const auto& ortho_state = transformed.GetBoneState(0);
        EXPECT_TRUE(ortho_state.uniform_scale);
        glm_mat4_mul(params.view, params.model, view_model);
        glm_mat4_mul(params.projection, view_model, clip_model);
        glm_mat4_mul(clip_model, source.bone_poses[0].pose, clip_from_local);
        const auto ortho_area = ProjectedArea(clip_from_local, p0, p1, p2);
        const auto ortho_facing = glm_vec4_dot(plane, ortho_state.facing_coeff);
        EXPECT_LT(ortho_area, 0.0f);
        EXPECT_LT(ortho_facing, 0.0f);
    }

    {
        glm_mat4_identity(params.model);
        params.model[1][1] = 0.0f;
        ASSERT_TRUE(UpdateRenderState(transformed, params, **input).ok());
        const auto& state = transformed.GetBoneState(0);
        const auto& normal = state.normal;
        EXPECT_FALSE(state.uniform_scale);
        for (const auto& column : normal) {
            for (float value : column) {
                EXPECT_FLOAT_EQ(value, 0.0f);
            }
        }
    }

    {
        params.model[0][0] = std::numeric_limits<float>::infinity();
        EXPECT_EQ(UpdateRenderState(transformed, params, **input).code(),
                  absl::StatusCode::kInvalidArgument);
    }
}

TEST(RenderStateTest, FactorizesFacingAcrossBonePose) {
    renderer::RenderParameters params{};
    glm_mat4_identity(params.model);
    params.model[0][0] = -2.0f;
    params.model[1][1] = 3.0f;
    params.model[2][2] = 0.5f;
    params.model[3][0] = 1.0f;
    params.model[3][2] = -5.0f;
    glm_mat4_identity(params.view);
    params.view[3][0] = -0.25f;
    params.view[3][1] = 0.75f;
    glm_mat4_zero(params.projection);
    params.projection[0][0] = 1.25f;
    params.projection[1][1] = 0.75f;
    params.projection[2][2] = -11.0f / 9.0f;
    params.projection[2][3] = -1.0f;
    params.projection[3][2] = -20.0f / 9.0f;

    simd::GenericTag tag;

    std::vector<math::PoseStack::Pose> bone_states(3);
    {
        math::PoseStack stack;
        stack.Translate(tag, 0.25f, -0.5f, 1.0f);
        stack.RotateZYX(tag, 0.2f, -0.4f, 0.3f);
        stack.Scale(tag, -2.0f, 2.0f, 2.0f);
        stack.Last().CopyTo(bone_states[0]);
    }
    {
        math::PoseStack stack;
        stack.Translate(tag, -1.0f, 0.5f, -0.25f);
        stack.Scale(tag, 3.0f, 3.0f, 3.0f);
        stack.RotateZYX(simd::GenericTag{}, -0.1f, 0.5f, -0.25f);
        stack.Scale(tag, -1.0f, 2.0f, 0.5f);
        stack.Last().CopyTo(bone_states[1]);
    }
    {
        math::PoseStack stack;
        stack.Translate(tag, 0.5f, 1.0f, -2.0f);
        stack.RotateZYX(simd::GenericTag{}, 0.6f, 0.15f, -0.35f);
        stack.Scale(tag, 0.5f, 1.5f, 2.5f);
        stack.Last().CopyTo(bone_states[2]);
    }

    const std::vector<uint16_t> indices{0, 1, 2};
    renderer::ModelPoseView source{bone_states, indices};
    bake::BakedModelBones bones;
    bones.list.resize(bone_states.size());
    for (auto& bone : bones.list) {
        bone.solid = true;
    }
    auto input = MakeRenderStateInput(bone_states, indices, bones, true);
    ASSERT_TRUE(input.ok()) << input.status();
    renderer::RenderState transformed;
    ASSERT_TRUE(UpdateRenderState(transformed, params, **input).ok());

    mat4 view_model;
    mat4 clip_model;
    glm_mat4_mul(params.view, params.model, view_model);
    glm_mat4_mul(params.projection, view_model, clip_model);
    for (const auto bone_index : indices) {
        mat4 clip;
        vec4 expected;
        vec4 actual;
        glm_mat4_mul(clip_model, bone_states[bone_index].pose, clip);
        ReferenceFacingCoefficient(clip, expected);
        glm_vec4_normalize_to(expected, expected);
        glm_vec4_normalize_to(
            transformed.GetBoneState(bone_index).facing_coeff, actual);
        for (size_t component = 0; component < 4; ++component) {
            EXPECT_NEAR(actual[component], expected[component], 0.00001f);
        }
    }
}

TEST(RenderStateTest, DerivesOuterNormalFromPoseMatrix) {
    renderer::RenderParameters params{};
    glm_mat4_identity(params.model);
    params.model[0][0] = 2.0f;
    params.model[1][0] = 0.75f;
    params.model[1][1] = 3.0f;
    params.model[2][2] = -4.0f;
    glm_mat4_identity(params.view);
    glm_mat4_identity(params.projection);

    std::vector<math::PoseStack::Pose> bone_states(1);
    glm_mat4_identity(bone_states[0].pose);
    glm_mat3_identity(bone_states[0].normal);
    const std::vector<uint16_t> indices{0};
    renderer::ModelPoseView source{bone_states, indices};
    bake::BakedModelBones bones;
    bones.list.resize(1);
    auto input = MakeRenderStateInput(bone_states, indices, bones, true);
    ASSERT_TRUE(input.ok()) << input.status();
    renderer::RenderState transformed;
    ASSERT_TRUE(UpdateRenderState(transformed, params, **input).ok());

    mat3 linear;
    mat3 expected;
    glm_mat4_pick3(params.model, linear);
    glm_mat3_inv(linear, expected);
    glm_mat3_transpose(expected);
    for (size_t column = 0; column < 3; ++column) {
        for (size_t row = 0; row < 3; ++row) {
            EXPECT_NEAR(transformed.GetBoneState(0).normal[column][row],
                        expected[column][row], 0.00001f);
        }
    }
}

TEST(RenderStateTest, PreNormalizesConformalOuterDirectionMatrix) {
    renderer::RenderParameters params{};
    glm_mat4_identity(params.model);
    params.model[0][0] = 0.0f;
    params.model[0][1] = 2.0f;
    params.model[1][0] = -2.0f;
    params.model[1][1] = 0.0f;
    params.model[2][2] = 2.0f;
    glm_mat4_identity(params.view);
    glm_mat4_identity(params.projection);

    std::vector<math::PoseStack::Pose> bone_states(1);
    glm_mat4_identity(bone_states[0].pose);
    glm_mat3_identity(bone_states[0].normal);
    const std::vector<uint16_t> indices{0};
    renderer::ModelPoseView source{bone_states, indices};
    bake::BakedModelBones bones;
    bones.list.resize(1);
    auto input = MakeRenderStateInput(bone_states, indices, bones, true);
    ASSERT_TRUE(input.ok()) << input.status();
    renderer::RenderState transformed;

    ASSERT_TRUE(UpdateRenderState(transformed, params, **input).ok());
    const auto& state = transformed.GetBoneState(0);
    EXPECT_TRUE(state.uniform_scale);
    const mat3 expected{
        {0.0f, 1.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
    };
    for (size_t column = 0; column < 3; ++column) {
        for (size_t row = 0; row < 3; ++row) {
            EXPECT_FLOAT_EQ(state.normal[column][row], expected[column][row]);
        }
    }

    (**input).poses[0].uniform_scale = false;
    ASSERT_TRUE(UpdateRenderState(transformed, params, **input).ok());
    EXPECT_FALSE(transformed.GetBoneState(0).uniform_scale);
}

TEST(RenderStateTest, CombinesOuterAndBoneTangentOrientation) {
    renderer::RenderParameters params{};
    glm_mat4_identity(params.model);
    params.model[0][0] = -2.0f;
    params.model[1][1] = 2.0f;
    params.model[2][2] = 2.0f;
    glm_mat4_identity(params.view);
    glm_mat4_identity(params.projection);

    std::vector<math::PoseStack::Pose> bone_states(1);
    glm_mat4_identity(bone_states[0].pose);
    glm_mat3_identity(bone_states[0].normal);
    bone_states[0].tangent_orientation = -1.0f;
    const std::vector<uint16_t> indices{0};
    renderer::ModelPoseView source{bone_states, indices};
    bake::BakedModelBones bones;
    bones.list.resize(1);
    auto input = MakeRenderStateInput(bone_states, indices, bones, true);
    ASSERT_TRUE(input.ok()) << input.status();
    renderer::RenderState transformed;

    ASSERT_TRUE(UpdateRenderState(transformed, params, **input).ok());
    EXPECT_TRUE(transformed.GetBoneState(0).uniform_scale);
    EXPECT_FLOAT_EQ(transformed.GetBoneState(0).tangent_orientation, 1.0f);
}

TEST(RenderStateTest, NonPbrStillTracksNonUniformScaleForNormals) {
    renderer::RenderParameters params{};
    glm_mat4_identity(params.model);
    params.model[0][0] = 2.0f;
    params.model[1][1] = 3.0f;
    glm_mat4_identity(params.view);
    glm_mat4_identity(params.projection);

    std::vector<math::PoseStack::Pose> bone_states(1);
    glm_mat4_identity(bone_states[0].pose);
    glm_mat3_identity(bone_states[0].normal);
    bone_states[0].tangent_orientation = -1.0f;
    const std::vector<uint16_t> indices{0};
    renderer::ModelPoseView source{bone_states, indices};
    bake::BakedModelBones bones;
    bones.list.resize(1);
    auto input = MakeRenderStateInput(bone_states, indices, bones, false);
    ASSERT_TRUE(input.ok()) << input.status();
    renderer::RenderState transformed;

    ASSERT_TRUE(UpdateRenderState(transformed, params, **input).ok());
    EXPECT_FALSE(transformed.GetBoneState(0).uniform_scale);
    EXPECT_FLOAT_EQ(transformed.GetBoneState(0).tangent_orientation, -1.0f);
}

TEST(RenderStateTest, SplitRangesMatchSerialUpdate) {
    renderer::RenderParameters params{};
    glm_mat4_identity(params.model);
    glm_mat4_identity(params.view);
    glm_mat4_identity(params.projection);
    params.model[0][0] = -1.25f;
    params.model[1][1] = 0.75f;
    params.model[3][2] = -2.0f;
    params.light = 0x12345678;

    std::vector<math::PoseStack::Pose> poses(4);
    for (size_t index = 0; index < poses.size(); ++index) {
        math::PoseStack stack;
        stack.Translate(simd::GenericTag{}, static_cast<float>(index) * 0.25f,
                        static_cast<float>(index) * -0.125f,
                        static_cast<float>(index) * 0.0625f);
        stack.RotateZYX(simd::GenericTag{}, static_cast<float>(index) * 0.03f,
                        static_cast<float>(index) * -0.02f,
                        static_cast<float>(index) * 0.01f);
        stack.Last().CopyTo(poses[index]);
    }
    const std::vector<uint16_t> indices{0, 1, 2, 3};
    bake::BakedModelBones bones;
    bones.list.resize(poses.size());
    for (auto& bone : bones.list) {
        bone.solid = false;
    }
    auto input = MakeRenderStateInput(poses, indices, bones, true);
    ASSERT_TRUE(input.ok()) << input.status();

    renderer::RenderState serial;
    ASSERT_TRUE(UpdateRenderState(serial, params, **input).ok());

    renderer::RenderState split;
    ASSERT_TRUE(
        split.UpdateCommon(kGenericTag, params, (**input).model_state).ok());
    ASSERT_TRUE(
        split.Update(kGenericTag, params, (**input).model_state, 0, 2).ok());
    ASSERT_TRUE(
        split.Update(kGenericTag, params, (**input).model_state, 1, 2).ok());

    for (size_t index = 0; index < poses.size(); ++index) {
        const auto& expected = serial.GetBoneState(index);
        const auto& actual = split.GetBoneState(index);
        EXPECT_TRUE(std::ranges::equal(
            std::span(&expected.pose[0][0], 16),
            std::span(&actual.pose[0][0], 16)));
        EXPECT_TRUE(std::ranges::equal(
            std::span(&expected.normal[0][0], 9),
            std::span(&actual.normal[0][0], 9)));
        EXPECT_TRUE(std::ranges::equal(expected.facing_coeff,
                                       actual.facing_coeff));
        EXPECT_TRUE(std::ranges::equal(expected.depth_z, actual.depth_z));
        EXPECT_TRUE(std::ranges::equal(expected.depth_w, actual.depth_w));
        EXPECT_FLOAT_EQ(actual.tangent_orientation,
                        expected.tangent_orientation);
        EXPECT_EQ(actual.packed_light, expected.packed_light);
        EXPECT_EQ(actual.uniform_scale, expected.uniform_scale);
    }
}

}  // namespace ysm::test
