#include "renderer/model_state.h"
#include "renderer/render.h"

#include "enum.h"
#include "java/buffer.h"
#include "java/entry.h"
#include "java/opaque_ptr.h"
#include "scope_guard.h"

namespace ysm::lib::render {
YSM_JNI_ENTRY(
    "Lcom/elfmcys/ysm/natives/render/NativeRenderer;nRender(Ljava/lang/Object;IJJJIJJ)Z",
    (vertex_buffer_obj, vertex_buffer_flag, mat_ptr, model_state_ptr,
     light_and_overlay, color, flags, iris_entity_id)) {
    YSM_RETURN_IF_NULL(vertex_buffer_obj);
    YSM_ASSERT(mat_ptr != 0,
               absl::InvalidArgumentError("Render matrices are null."));
    const auto* mat =
        reinterpret_cast<const float*>(static_cast<uintptr_t>(mat_ptr));
    YSM_DECLARE_OR_RETURN(
        model_state,
        java::CastOpaquePtr<renderer::ModelState>(model_state_ptr));

    const auto packed_flags = static_cast<uint64_t>(flags);
    BufferView vertex_buffer;
    YSM_DECLARE_OR_RETURN(
        vertex_kind, EnumCast<renderer::VertexKind>(packed_flags >> 2));
    // ReSharper disable once CppTooWideScope
    java::CriticalFloatArray<false> vertex_buffer_array;
    if (vertex_buffer_flag > 0) {
        YSM_ASSIGN_OR_RETURN(
            vertex_buffer, java::GetDirectBuffer(env, vertex_buffer_obj));
    } else {
        YSM_ASSIGN_OR_RETURN(
            vertex_buffer_array,
            java::CriticalFloatArray<false>::Get(
                env, reinterpret_cast<jfloatArray>(vertex_buffer_obj)));
        vertex_buffer = {
            reinterpret_cast<Byte*>(vertex_buffer_array.data()),
            vertex_buffer_array.size() * 4};
    }

    const auto packed_light_and_overlay =
        static_cast<uint64_t>(light_and_overlay);
    renderer::RenderParameters parameters;
    glm_mat4_make(mat, parameters.model);
    glm_mat4_make(mat + 16, parameters.view);
    glm_mat4_make(mat + 32, parameters.projection);
    glm_mat3_make(mat + 48, parameters.normal);
    YSM_ASSIGN_OR_RETURN(
        parameters.ctx,
        EnumCast<renderer::RenderContext>(packed_flags & 0x3u));
    parameters.light =
        static_cast<uint32_t>(packed_light_and_overlay >> 32);
    parameters.overlay =
        static_cast<uint32_t>(packed_light_and_overlay & 0xFFFFFFFFULL);
    parameters.color =
        std::bit_cast<renderer::Color>(static_cast<uint32_t>(color));
    parameters.iris_entity_id = static_cast<uint64_t>(iris_entity_id);

    return renderer::Render(vertex_buffer, vertex_kind, *model_state,
                            parameters);
}
}  // namespace ysm::lib::render
