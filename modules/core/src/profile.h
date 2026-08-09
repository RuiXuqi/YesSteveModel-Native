#pragma once

#include <cstdint>

#ifndef YSM_ENABLE_TRACY
#define YSM_ENABLE_TRACY 0
#endif

#if YSM_ENABLE_TRACY
#include <tracy/TracyC.h>
#include <tracy/Tracy.hpp>
#endif

namespace ysm::profile {
class SourceLocation final {
   public:
#if YSM_ENABLE_TRACY
    constexpr SourceLocation(const char* name, const char* function,
                             const char* file, uint32_t line,
                             uint32_t color = 0) noexcept
        : data_{name, function, file, line, color} {}
#else
    constexpr SourceLocation(const char*, const char*, const char*, uint32_t,
                             uint32_t = 0) noexcept {}
#endif

   private:
    friend uint64_t BeginZone(const SourceLocation&) noexcept;

#if YSM_ENABLE_TRACY
    ___tracy_source_location_data data_;
#endif
};

[[nodiscard]] uint64_t BeginZone(const SourceLocation& source_location) noexcept;
[[nodiscard]] bool EndZone(uint64_t token) noexcept;
[[nodiscard]] bool BeginFrame(const char* name) noexcept;
void EndFrame(const char* name) noexcept;
}  // namespace ysm::profile

#if YSM_ENABLE_TRACY
#define YSM_PROFILE_ZONE(name) ZoneScopedN(name)
#define YSM_PROFILE_VALUE(value) ZoneValue(value)
#define YSM_PROFILE_ZONE_BEGIN(source_location) \
    (::ysm::profile::BeginZone(source_location))
#define YSM_PROFILE_ZONE_END(token) (::ysm::profile::EndZone(token))
#define YSM_PROFILE_FRAME_BEGIN(name) (::ysm::profile::BeginFrame(name))
#define YSM_PROFILE_FRAME_END(name) (::ysm::profile::EndFrame(name))
#else
#define YSM_PROFILE_ZONE(name) (static_cast<void>(sizeof(name)))
#define YSM_PROFILE_VALUE(value) (static_cast<void>(sizeof(value)))
#define YSM_PROFILE_ZONE_BEGIN(source_location) \
    (static_cast<void>(sizeof(source_location)), uint64_t{0})
#define YSM_PROFILE_ZONE_END(token) \
    (static_cast<void>(sizeof(token)), true)
#define YSM_PROFILE_FRAME_BEGIN(name) \
    (static_cast<void>(sizeof(name)), false)
#define YSM_PROFILE_FRAME_END(name) (static_cast<void>(sizeof(name)))
#endif
