#include <java/entry.h>

#include <magic_enum.hpp>

#include <log.h>

#include "cpu.h"

namespace ysm::lib::log {
YSM_JNI_ENTRY("Lcom/elfmcys/ysm/natives/NativeLogging;nSetLevel(I)Z", (level)) {
    auto status = ConfigureLogLevel(level);
    if (!status.ok()) {
        YSM_LOG(ERROR, "Failed to configure native logging: {}",
                status.message());
        return status;
    }
    YSM_LOG(INFO, "Native runtime initialized: log level {}",
            LogLevelName(static_cast<LogLevel>(level)));
    return OkStatus();
}
}  // namespace ysm::lib
