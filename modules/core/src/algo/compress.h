#pragma once

#include "buffer_managed.h"
#include "err.h"

namespace ysm::algo {
absl::Status ZlibDecompress(BufferViewR input, BufferManaged& dst);
absl::StatusOr<size_t> ZlibDecompress(BufferViewR input, BufferView dst);

absl::StatusOr<size_t> ZstdGetCompressMaxSize(size_t uncompress_size);
absl::StatusOr<size_t> ZstdCompress(BufferViewR input, BufferView dst,
                                    int level);
absl::Status ZstdDecompress(BufferViewR input, BufferManaged& dst);
absl::StatusOr<size_t> ZstdDecompress(BufferViewR input, BufferView dst);
}  // namespace ysm::algo
