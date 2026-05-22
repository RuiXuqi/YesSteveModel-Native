#pragma once

#include "buffer.h"
#include "non_copyable.h"
#include "pimpl.h"

namespace ysm::codec {
class OpusAudioStream : public NonCopyable {
    YSM_PIMPL_DECLARE(Impl, 512)
   public:
    static constexpr int32_t kEagain = -2;

    OpusAudioStream();
    ~OpusAudioStream();

    void Consume(BufferViewR data_buffer);

    int32_t Decode(BufferView dst_buffer) noexcept;

    [[nodiscard]] uint64_t AvailableSamples() const;

    void Reset() noexcept;
};
}  // namespace ysm::codec
