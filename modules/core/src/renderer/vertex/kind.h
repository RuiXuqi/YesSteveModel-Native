#pragma once

#include <cstdint>

namespace ysm::renderer {
enum class VertexKind : uint16_t {
    kFallback,
    kVanilla,
    kIris56,
    kIris56Ar,
    kIris55,
    kIris54
};
}