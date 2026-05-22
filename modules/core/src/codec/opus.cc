#include "codec/opus.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <span>

#include <opus/opus.h>

#include "buffer_managed.h"
#include "mini_ogg.h"

namespace ysm::codec {
namespace {
constexpr uint32_t kTargetSampleRate = 48000;

constexpr BufferFixed<8> kOpusId{'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'};
constexpr BufferFixed<8> kOpusTags{'O', 'p', 'u', 's', 'T', 'a', 'g', 's'};

#pragma pack(push, 1)

struct OpusHead {
    BufferFixed<kOpusId.size()> id;
    uint8_t version;
    uint8_t channels;
    uint16_t pre_skip;
    uint32_t input_sample_rate;
    int16_t output_gain;
};

#pragma pack(pop)
}  // namespace

class OpusAudioStream::Impl {
    enum class HeaderState : uint8_t { kOpusHead, kOpusTags, kComplete };
    enum class PacketStatus : uint8_t { kFull, kEagain, kError };

    struct PacketResult {
        BufferViewR packet;
        uint32_t serial_number = 0;
        PacketStatus status = PacketStatus::kEagain;
    };

    MiniOgg parser_;
    BufferManaged data_buffer_;
    size_t data_position_ = 0;
    HeaderState header_state_ = HeaderState::kOpusHead;
    uint32_t serial_number_ = std::numeric_limits<uint32_t>::max();
    OpusHead head_{};
    BufferManaged packet_buffer_;

    uint32_t pre_skip_remaining_ = 0;
    uint64_t total_samples_decoded_ = 0;
    uint64_t final_sample_count_ = 0;
    bool has_final_sample_count_ = false;

    OpusDecoder* decoder_ = nullptr;
    BufferManaged pcm_buffer_;
    size_t pcm_buffer_position_ = 0;

   public:
    Impl() {
        int error = OPUS_OK;
        decoder_ = opus_decoder_create(kTargetSampleRate, 2, &error);
        if (decoder_ == nullptr || error != OPUS_OK) [[unlikely]] {
            if (decoder_ != nullptr) {
                opus_decoder_destroy(decoder_);
                decoder_ = nullptr;
            }
            throw std::bad_alloc();
        }
    }

    ~Impl() {
        if (decoder_ != nullptr) {
            opus_decoder_destroy(decoder_);
        }
    }

    void Consume(BufferViewR data) {
        if (data.empty()) {
            return;
        }

        CompactInput();
        if (data.size() > BufferManaged::kMaxSize - data_buffer_.size())
            [[unlikely]] {
            throw std::bad_alloc();
        }

        auto offset = data_buffer_.size();
        data_buffer_.resize(offset + data.size());
        std::memcpy(data_buffer_.data() + offset, data.data(), data.size());
    }

    int32_t Decode(BufferView dst) noexcept {
        if (dst.empty()) [[unlikely]] {
            return 0;
        }
        dst = dst.first(dst.size() - dst.size() % sizeof(int16_t));

        auto init_result = EnsureInitialized();
        if (init_result != 0) {
            return FinishDecode(init_result);
        }

        size_t written = 0;
        while (!dst.empty()) {
            if (pcm_buffer_position_ < pcm_buffer_.size()) {
                auto copy_size = std::min(
                    dst.size(), pcm_buffer_.size() - pcm_buffer_position_);
                std::memcpy(dst.data(),
                            pcm_buffer_.data() + pcm_buffer_position_,
                            copy_size);
                dst = dst.subspan(copy_size);
                written += copy_size;
                pcm_buffer_position_ += copy_size;
                if (pcm_buffer_position_ == pcm_buffer_.size()) {
                    pcm_buffer_.resize(0);
                    pcm_buffer_position_ = 0;
                }
                continue;
            }

            if (has_final_sample_count_ &&
                total_samples_decoded_ >= final_sample_count_) {
                break;
            }

            auto packet_result = NextPacket();
            if (packet_result.status == PacketStatus::kEagain) {
                return FinishDecode(written > 0 ? static_cast<int32_t>(written)
                                                : kEagain);
            }
            if (packet_result.status == PacketStatus::kError) [[unlikely]] {
                return FinishDecode(-1);
            }

            auto result = DecodePacket(packet_result.packet);
            packet_buffer_.clear();
            if (result < 0) [[unlikely]] {
                return FinishDecode(result);
            }
        }

        return FinishDecode(static_cast<int32_t>(written));
    }

    uint64_t AvailableSamples() const {
        auto pcm_samples = static_cast<uint64_t>(
            (pcm_buffer_.size() - pcm_buffer_position_) / sizeof(int16_t));

        auto parser = parser_;
        auto data_position = data_position_;
        auto header_state = header_state_;
        auto serial_number = serial_number_;
        auto head = head_;
        auto pre_skip_remaining = pre_skip_remaining_;
        auto final_sample_count = final_sample_count_;
        auto has_final_sample_count = has_final_sample_count_;
        BufferManaged packet_buffer(packet_buffer_);
        uint64_t ogg_samples = 0;

        auto append_packet = [&packet_buffer](BufferViewR packet) {
            auto offset = packet_buffer.size();
            packet_buffer.resize(offset + packet.size());
            std::memcpy(packet_buffer.data() + offset, packet.data(),
                        packet.size());
        };

        while (data_position < data_buffer_.size()) {
            auto input = BufferViewR(data_buffer_).subspan(data_position);
            auto result_or = parser.Process(input);
            if (!result_or.ok()) [[unlikely]] {
                break;
            }
            auto result = *result_or;
            data_position += result.consumed;

            auto page = parser.Page();
            auto target_stream =
                page &&
                (serial_number == std::numeric_limits<uint32_t>::max() ||
                 page->serial_no == serial_number);

            if (target_stream &&
                serial_number != std::numeric_limits<uint32_t>::max() &&
                page->eos && !has_final_sample_count) {
                if (page->granule_pos == std::numeric_limits<uint64_t>::max() ||
                    page->granule_pos < head.pre_skip) [[unlikely]] {
                    break;
                }
                final_sample_count = page->granule_pos - head.pre_skip;
                has_final_sample_count = true;
            }

            if (result.status == MiniOgg::ProcessStatus::kEagain) {
                break;
            }
            if (!page) [[unlikely]] {
                break;
            }
            if (!target_stream) {
                continue;
            }
            if (result.status == MiniOgg::ProcessStatus::kPartial) {
                append_packet(result.packet);
                continue;
            }
            if (result.status != MiniOgg::ProcessStatus::kFull) [[unlikely]] {
                break;
            }

            append_packet(result.packet);
            BufferViewR packet = packet_buffer;
            if (header_state == HeaderState::kOpusHead) {
                if (packet.size() < sizeof(OpusHead)) [[unlikely]] {
                    break;
                }
                std::memcpy(&head, packet.data(), sizeof(head));
                if (!Cmp(kOpusId, head.id) ||
                    (head.channels != 1 && head.channels != 2)) [[unlikely]] {
                    break;
                }
                serial_number = page->serial_no;
                pre_skip_remaining = head.pre_skip;
                header_state = HeaderState::kOpusTags;
            } else if (header_state == HeaderState::kOpusTags) {
                if (packet.size() < kOpusTags.size() ||
                    !Cmp(packet.first<kOpusTags.size()>(), kOpusTags))
                    [[unlikely]] {
                    break;
                }
                header_state = HeaderState::kComplete;
            } else {
                auto samples = opus_packet_get_nb_samples(
                    packet.data(), static_cast<opus_int32>(packet.size()),
                    kTargetSampleRate);
                if (samples < 0) [[unlikely]] {
                    break;
                }
                auto skipped = std::min<uint64_t>(pre_skip_remaining, samples);
                pre_skip_remaining -= static_cast<uint32_t>(skipped);
                ogg_samples += static_cast<uint64_t>(samples) - skipped;
            }
            packet_buffer.clear();
        }

        if (has_final_sample_count) {
            auto remaining = final_sample_count > total_samples_decoded_
                                 ? final_sample_count - total_samples_decoded_
                                 : 0;
            ogg_samples = std::min(ogg_samples, remaining);
        }
        return pcm_samples + ogg_samples;
    }

    void Reset() noexcept {
        parser_.Reset();
        data_buffer_.clear();
        data_position_ = 0;
        header_state_ = HeaderState::kOpusHead;
        serial_number_ = std::numeric_limits<uint32_t>::max();
        head_ = {};
        packet_buffer_.clear();
        pre_skip_remaining_ = 0;
        total_samples_decoded_ = 0;
        final_sample_count_ = 0;
        has_final_sample_count_ = false;
        pcm_buffer_.clear();
        pcm_buffer_position_ = 0;
    }

   private:
    int32_t EnsureInitialized() noexcept {
        while (header_state_ != HeaderState::kComplete) {
            auto result = NextPacket();
            if (result.status == PacketStatus::kEagain) {
                return kEagain;
            }
            if (result.status == PacketStatus::kError) [[unlikely]] {
                return -1;
            }

            if (header_state_ == HeaderState::kOpusHead) {
                if (result.packet.size() < sizeof(OpusHead)) [[unlikely]] {
                    Abort();
                    return -1;
                }

                std::memcpy(&head_, result.packet.data(), sizeof(head_));
                if (!Cmp(kOpusId, head_.id) ||
                    (head_.channels != 1 && head_.channels != 2)) [[unlikely]] {
                    Abort();
                    return -1;
                }

                serial_number_ = result.serial_number;
                if (opus_decoder_init(decoder_, kTargetSampleRate,
                                      head_.channels) != OPUS_OK ||
                    opus_decoder_ctl(decoder_,
                                     OPUS_SET_GAIN(static_cast<opus_int32>(
                                         head_.output_gain))) != OPUS_OK)
                    [[unlikely]] {
                    Abort();
                    return -1;
                }

                pre_skip_remaining_ = head_.pre_skip;
                header_state_ = HeaderState::kOpusTags;
            } else {
                if (result.packet.size() < kOpusTags.size() ||
                    !Cmp(result.packet.first<kOpusTags.size()>(), kOpusTags))
                    [[unlikely]] {
                    Abort();
                    return -1;
                }
                header_state_ = HeaderState::kComplete;
            }
            packet_buffer_.clear();
        }
        return 0;
    }

    int32_t DecodePacket(BufferViewR packet) noexcept {
        auto samples = opus_packet_get_nb_samples(
            packet.data(), static_cast<opus_int32>(packet.size()),
            kTargetSampleRate);
        if (samples == OPUS_BAD_ARG) [[unlikely]] {
            return -3;
        }
        if (samples == OPUS_INVALID_PACKET) [[unlikely]] {
            return -4;
        }
        if (samples < 0) [[unlikely]] {
            return samples - 100;
        }

        auto channels = static_cast<size_t>(head_.channels);
        pcm_buffer_.resize(static_cast<size_t>(samples) * channels *
                           sizeof(int16_t));
        auto* pcm = reinterpret_cast<int16_t*>(pcm_buffer_.data());
        auto decoded = opus_decode(decoder_, packet.data(),
                                   static_cast<opus_int32>(packet.size()), pcm,
                                   samples, 0);
        if (decoded < 0) [[unlikely]] {
            pcm_buffer_.resize(0);
            return decoded - 100;
        }

        auto skipped = std::min<size_t>(pre_skip_remaining_, decoded);
        pre_skip_remaining_ -= static_cast<uint32_t>(skipped);
        auto output_samples = static_cast<size_t>(decoded) - skipped;

        if (has_final_sample_count_) {
            auto remaining = final_sample_count_ > total_samples_decoded_
                                 ? final_sample_count_ - total_samples_decoded_
                                 : 0;
            output_samples = std::min<uint64_t>(output_samples, remaining);
        }

        if (head_.channels == 2) {
            auto* source = pcm + skipped * 2;
            Downmix({source, output_samples * 2});
            if (source != pcm && output_samples > 0) {
                std::memmove(pcm, source, output_samples * sizeof(int16_t));
            }
        } else if (skipped > 0 && output_samples > 0) {
            std::memmove(pcm, pcm + skipped, output_samples * sizeof(int16_t));
        }

        total_samples_decoded_ += output_samples;
        pcm_buffer_.resize(output_samples * sizeof(int16_t));
        pcm_buffer_position_ = 0;
        return 0;
    }

    static void Downmix(std::span<int16_t> pcm) noexcept {
        auto samples = pcm.size() / 2;
        for (size_t i = 0; i < samples; ++i) {
            pcm[i] = static_cast<int16_t>(
                std::round((static_cast<float>(pcm[i * 2]) +
                            static_cast<float>(pcm[i * 2 + 1])) /
                           2.0f));
        }
    }

    PacketResult NextPacket() noexcept {
        while (data_position_ < data_buffer_.size()) {
            auto input = BufferViewR(data_buffer_).subspan(data_position_);
            auto result_or = parser_.Process(input);
            if (!result_or.ok()) [[unlikely]] {
                Abort();
                return {.status = PacketStatus::kError};
            }
            auto result = *result_or;
            data_position_ += result.consumed;

            auto page = parser_.Page();
            auto target_stream =
                page &&
                (serial_number_ == std::numeric_limits<uint32_t>::max() ||
                 page->serial_no == serial_number_);

            if (target_stream &&
                serial_number_ != std::numeric_limits<uint32_t>::max() &&
                page->eos && !has_final_sample_count_) {
                if (page->granule_pos == std::numeric_limits<uint64_t>::max() ||
                    page->granule_pos < head_.pre_skip) [[unlikely]] {
                    Abort();
                    return {.status = PacketStatus::kError};
                }
                final_sample_count_ = page->granule_pos - head_.pre_skip;
                has_final_sample_count_ = true;
            }

            if (result.status == MiniOgg::ProcessStatus::kEagain) {
                return {.status = PacketStatus::kEagain};
            }
            if (!page) [[unlikely]] {
                Abort();
                return {.status = PacketStatus::kError};
            }
            if (!target_stream) {
                continue;
            }
            if (result.status == MiniOgg::ProcessStatus::kPartial) {
                AppendPacket(result.packet);
                continue;
            }
            if (result.status != MiniOgg::ProcessStatus::kFull) [[unlikely]] {
                Abort();
                return {.status = PacketStatus::kError};
            }

            AppendPacket(result.packet);
            return {packet_buffer_, page->serial_no, PacketStatus::kFull};
        }

        return {.status = PacketStatus::kEagain};
    }

    void AppendPacket(BufferViewR packet) {
        auto offset = packet_buffer_.size();
        packet_buffer_.resize(offset + packet.size());
        std::memcpy(packet_buffer_.data() + offset, packet.data(),
                    packet.size());
    }

    void Abort() noexcept { Reset(); }

    int32_t FinishDecode(int32_t result) noexcept {
        CompactInput();
        return result;
    }

    void CompactInput() noexcept {
        if (data_position_ == 0) {
            return;
        }

        auto remaining = data_buffer_.size() - data_position_;
        if (remaining > 0) {
            std::memmove(data_buffer_.data(),
                         data_buffer_.data() + data_position_, remaining);
            data_buffer_.resize(remaining);
        } else {
            data_buffer_.clear();
        }
        data_position_ = 0;
    }
};

YSM_PIMPL_DEFINITION(OpusAudioStream)

OpusAudioStream::OpusAudioStream() {}

OpusAudioStream::~OpusAudioStream() = default;

void OpusAudioStream::Consume(BufferViewR data_buffer) {
    Pimpl().Consume(data_buffer);
}

int32_t OpusAudioStream::Decode(BufferView dst_buffer) noexcept {
    return Pimpl().Decode(dst_buffer);
}

uint64_t OpusAudioStream::AvailableSamples() const {
    return Pimpl().AvailableSamples();
}

void OpusAudioStream::Reset() noexcept {
    Pimpl().Reset();
}

}  // namespace ysm::codec
