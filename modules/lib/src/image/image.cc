#include <codec/image.h>
#include <enum.h>
#include <java/buffer.h>
#include <java/entry.h>

#include "log.h"

namespace ysm::lib::image {
YSM_JNI_ENTRY(
    "Lcom/elfmcys/ysm/natives/image/Image$Native;nDecode(Ljava/lang/Object;JIIIJJ)Z",
    (input, input_flags, format_value, width, height, dst, dst_size)) {
    YSM_DECLARE_OR_RETURN(format,
                          EnumCast<codec::ImageFormat>(format_value));
    YSM_ASSERT(width > 0 && height > 0,
               absl::InvalidArgumentError("Invalid image size"sv));
    YSM_ASSERT(dst != 0 && dst_size > 0,
               absl::InvalidArgumentError("Invalid output buf"sv));
    BufferView out_buf{reinterpret_cast<Byte*>(dst),
                       static_cast<size_t>(dst_size)};
    size_t input_size;
    {
        YSM_DECLARE_OR_RETURN(
            in_buf, java::BufferInput<true>::Get(env, input, input_flags));
        input_size = in_buf.size();
        YSM_RETURN_IF_ERROR(codec::ImageDecode(
            in_buf,
            {static_cast<uint32_t>(width), static_cast<uint32_t>(height),
             format, 1},
            out_buf));
    }
    YSM_LOG_DEBUG(
        "Decoded image: format={}, dimensions={}x{}, input={} bytes, "
        "output={} bytes",
        format_value, width, height, input_size, out_buf.size());
    return OkStatus();
}

YSM_JNI_ENTRY(
    "Lcom/elfmcys/ysm/natives/image/Image$Native;nProbe(Ljava/lang/Object;J)J",
    (input, input_flags)) {
    codec::ImageInfo info;
    size_t input_size;
    {
        YSM_DECLARE_OR_RETURN(
            in_buf, java::BufferInput<true>::Get(env, input, input_flags));
        input_size = in_buf.size();
        YSM_ASSIGN_OR_RETURN(info, codec::ImageProbe(in_buf));
    }
    YSM_LOG_DEBUG("Probed image: format={}, dimensions={}x{}, input={} bytes",
                  static_cast<int>(info.format), info.width, info.height,
                  input_size);
    return (static_cast<uint64_t>(info.format) << 32) | (info.width << 16) |
           info.height;
}
}  // namespace ysm::lib::image
