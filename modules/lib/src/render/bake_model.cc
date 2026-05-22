#include <cstdint>

#include <bake/baked_model.h>
#include <bake/baked_serializer.h>
#include <cpu.h>
#include <java/array.h>
#include <java/buffer.h>
#include <java/entry.h>
#include <log.h>

#include "java/opaque_ptr.h"

namespace ysm::lib::render {
namespace {
constexpr uint64_t kOriginVerMask = 0xffff;
constexpr uint64_t kForceCullingMask = 1ULL << 16;
constexpr uint64_t kForceTranslucentMask = 1ULL << 17;
constexpr uint64_t kHasPbrMask = 1ULL << 18;
constexpr uint64_t kBakeOptionsMask =
    kOriginVerMask | kForceCullingMask | kForceTranslucentMask | kHasPbrMask;
}  // namespace

YSM_JNI_ENTRY(
    "Lcom/elfmcys/ysm/natives/render/NativeBakedModel;nCapability()I",
    ()) {
    return static_cast<jint>(simd::kSupported);
}

YSM_JNI_ENTRY(
    "Lcom/elfmcys/ysm/natives/render/NativeBakedModel;nBake(Ljava/lang/Object;J[SJIIJ)Ljava/nio/ByteBuffer;",
    (model_data_buf, model_data_flags, sorted_bone_indices_array, pixels_ptr,
     pixels_width, pixels_height, bake_options)) {
    YSM_DECLARE_OR_RETURN(model_data,
                          java::BufferInput<true, true>::Get(
                              env, model_data_buf, model_data_flags));
    if (pixels_ptr == 0 || pixels_width <= 0 || pixels_height <= 0) {
        return absl::InvalidArgumentError("Invalid model texture.");
    }
    const auto packed_options = static_cast<uint64_t>(bake_options);
    if ((packed_options & ~kBakeOptionsMask) != 0) {
        return absl::InvalidArgumentError("Invalid bake options.");
    }
    bake::Texture texture(reinterpret_cast<const bake::Pixel*>(pixels_ptr),
                          static_cast<size_t>(pixels_width),
                          static_cast<size_t>(pixels_height));
    YSM_DECLARE_OR_RETURN(
        baked_model,
        bake::BakeModel(
            model_data, texture,
            {.origin_ver =
                 static_cast<uint16_t>(packed_options & kOriginVerMask),
             .force_culling = (packed_options & kForceCullingMask) != 0,
             .force_translucent =
                 (packed_options & kForceTranslucentMask) != 0,
             .has_pbr = (packed_options & kHasPbrMask) != 0}));
    YSM_RETURN_IF_ERROR(java::WriteShortArray(
        env, sorted_bone_indices_array,
        baked_model->Bones().sorted_bone_indices));
    auto baked_data = bake::SerializeBakedModel(*baked_model);
    YSM_LOG_DEBUG(
        "Baked model: input={} bytes, texture={}x{}, bones={}, "
        "output={} bytes",
        model_data.size(), pixels_width, pixels_height,
        baked_model->Bones().sorted_bone_indices.size(), baked_data.size());

    return java::TryMoveToOutput(env, std::move(baked_data),
                                 java::BufferType::kDirect);
}

YSM_JNI_ENTRY(
    "Lcom/elfmcys/ysm/natives/render/NativeBakedModel;nTryBake(Ljava/lang/Object;JJIIJ)Z",
    (model_data_buf, model_data_flags, pixels_ptr, pixels_width, pixels_height,
     bake_options)) {
    YSM_DECLARE_OR_RETURN(model_data,
                          java::BufferInput<true, true>::Get(
                              env, model_data_buf, model_data_flags));
    if (pixels_ptr == 0 || pixels_width <= 0 || pixels_height <= 0) {
        return absl::InvalidArgumentError("Invalid model texture.");
    }
    const auto packed_options = static_cast<uint64_t>(bake_options);
    if ((packed_options & ~kBakeOptionsMask) != 0) {
        return absl::InvalidArgumentError("Invalid bake options.");
    }
    bake::Texture texture(reinterpret_cast<const bake::Pixel*>(pixels_ptr),
                          static_cast<size_t>(pixels_width),
                          static_cast<size_t>(pixels_height));
    return bake::TryBakeModel(
        model_data, texture,
        {.origin_ver =
             static_cast<uint16_t>(packed_options & kOriginVerMask),
         .force_culling = (packed_options & kForceCullingMask) != 0,
         .force_translucent =
             (packed_options & kForceTranslucentMask) != 0,
         .has_pbr = (packed_options & kHasPbrMask) != 0});
}

YSM_JNI_ENTRY(
    "Lcom/elfmcys/ysm/natives/render/NativeBakedModel;nRead(Ljava/lang/Object;J[S)J",
    (baked_model_buf_obj, baked_model_buf_flags, sorted_bone_indices_array)) {
    YSM_DECLARE_OR_RETURN(
        baked_model_buf,
        java::BufferInput<true, true>::Get(env, baked_model_buf_obj,
                                           baked_model_buf_flags));
    YSM_DECLARE_OR_RETURN(model, bake::ReadBakedModel(baked_model_buf));
    YSM_RETURN_IF_ERROR(java::WriteShortArray(
        env, sorted_bone_indices_array,
        model->Bones().sorted_bone_indices));
    YSM_LOG_DEBUG("Read baked model: input={} bytes, bones={}",
                  baked_model_buf.size(),
                  model->Bones().sorted_bone_indices.size());
    return java::MakeOpaquePtr<bake::BakedModel>(std::move(*model));
}
}  // namespace ysm::lib::render
