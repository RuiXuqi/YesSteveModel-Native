#include <java/entry.h>

#include <cstdint>
#include <mutex>
#include <optional>

#include <log.h>

#include "cpu.h"
#include "profile.h"

namespace ysm::lib::runtime {
namespace {
constexpr uint64_t kConfigVersion = 1;
constexpr uint64_t kVersionMask = 0xFF;
constexpr uint64_t kLogLevelShift = 8;
constexpr uint64_t kLogLevelMask = 0x7ULL << kLogLevelShift;
constexpr uint64_t kJavaConfigMask = kVersionMask | kLogLevelMask;
constexpr uint64_t kTracyEnabledMask = 1ULL << 8;
constexpr uint64_t kDebugLoggingEnabledMask = 1ULL << 9;

std::mutex g_initialize_mutex;
std::optional<uint64_t> g_initialized_java_config;

constexpr uint64_t NativeConfig() noexcept {
    uint64_t config = kConfigVersion;
#if YSM_ENABLE_TRACY
    config |= kTracyEnabledMask;
#endif
#if YSM_ENABLE_DEBUG_LOG
    config |= kDebugLoggingEnabledMask;
#endif
    return config;
}
}  // namespace

YSM_JNI_ENTRY("Lcom/elfmcys/ysm/natives/NativeRuntime;nInitialize(J)J",
              (java_config), jlong{0}) {
    const auto packed = static_cast<uint64_t>(java_config);
    YSM_ASSERT((packed & ~kJavaConfigMask) == 0,
               absl::InvalidArgumentError("Unknown Java runtime config bits"));
    YSM_ASSERT((packed & kVersionMask) == kConfigVersion,
               absl::FailedPreconditionError(
                   "Unsupported Java runtime config version"));

    const auto log_level =
        static_cast<int>((packed & kLogLevelMask) >> kLogLevelShift);
    std::scoped_lock lock(g_initialize_mutex);
    if (g_initialized_java_config.has_value()) {
        YSM_ASSERT(*g_initialized_java_config == packed,
                   absl::FailedPreconditionError(
                       "Native runtime was initialized with different config"));
        return static_cast<jlong>(NativeConfig());
    }

    YSM_RETURN_IF_ERROR(ConfigureLogLevel(log_level));
    g_initialized_java_config = packed;
    YSM_LOG(INFO,
            "Native runtime initialized: log level {}, tracy {}, debug logging {}",
            LogLevelName(static_cast<LogLevel>(log_level)),
            YSM_ENABLE_TRACY != 0, YSM_ENABLE_DEBUG_LOG != 0);
    return static_cast<jlong>(NativeConfig());
}
}  // namespace ysm::lib::runtime
