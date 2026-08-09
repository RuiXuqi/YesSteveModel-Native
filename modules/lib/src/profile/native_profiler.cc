#include <java/entry.h>

#include <array>
#include <cstdint>

#include "profile.h"

namespace ysm::lib::profile {
namespace {
enum class JavaZone : size_t {
    kAnimatableEntityUpdate,
    kFallbackVertexWriterWrite,
};

constexpr std::array kJavaZoneLocations{
    ysm::profile::SourceLocation{
        "YSM/Java/AnimatableEntity.update", "AnimatableEntity.update",
        "AnimatableEntity.java", 0},
    ysm::profile::SourceLocation{
        "YSM/Java/FallbackVertexWriter.write", "FallbackVertexWriter.write",
        "FallbackVertexWriter.java", 0},
};

constexpr char kFrameName[] = "YSM/Frame/GameRenderer.render";

absl::StatusOr<JavaZone> ParseJavaZone(jint zone_id) {
    switch (zone_id) {
        case 0:
            return JavaZone::kAnimatableEntityUpdate;
        case 1:
            return JavaZone::kFallbackVertexWriterWrite;
        default:
            return absl::InvalidArgumentError("Unknown Java profile zone");
    }
}
}  // namespace

YSM_JNI_ENTRY("Lcom/elfmcys/ysm/natives/NativeProfiler;nBeginZone(I)J",
              (zone_id), jlong{0}) {
    YSM_DECLARE_OR_RETURN(zone, ParseJavaZone(zone_id));
    const auto& source_location =
        kJavaZoneLocations[static_cast<size_t>(zone)];
    return static_cast<jlong>(YSM_PROFILE_ZONE_BEGIN(source_location));
}

YSM_JNI_ENTRY("Lcom/elfmcys/ysm/natives/NativeProfiler;nEndZone(J)V",
              (token)) {
    YSM_ASSERT(YSM_PROFILE_ZONE_END(static_cast<uint64_t>(token)),
               absl::InvalidArgumentError("Invalid Java profile zone token"));
    return OkStatus();
}

YSM_JNI_ENTRY("Lcom/elfmcys/ysm/natives/NativeProfiler;nBeginFrame()J", (),
              jlong{0}) {
    return static_cast<jlong>(YSM_PROFILE_FRAME_BEGIN(kFrameName));
}

YSM_JNI_ENTRY("Lcom/elfmcys/ysm/natives/NativeProfiler;nEndFrame()V", ()) {
    YSM_PROFILE_FRAME_END(kFrameName);
    return OkStatus();
}
}  // namespace ysm::lib::profile
