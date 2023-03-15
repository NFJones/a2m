#pragma once
#include <algorithm>
#include <functional>
#include <vector>

namespace njones {
namespace audio {
template <typename T, typename U>
concept SameType = std::same_as<T, U>;

template <class SampleType, class ConversionType = double>
class RingBuffer {
   public:
    RingBuffer(
        std::function<void(const int, ConversionType*, const int)> processor =
            [](const int, ConversionType*, const int) -> void {},
        int nchannels = 0,
        int block_size = 0)
        : processor(processor), index(0) {
        resize(nchannels, block_size);
    }

    RingBuffer(const RingBuffer& rhs) { this->operator=(rhs); }

    RingBuffer(RingBuffer&& rhs) { this->operator=(rhs); }

    ~RingBuffer() {}

    RingBuffer& operator=(const RingBuffer& rhs) {
        processor = rhs.processor;
        nchannels = rhs.nchannels;
        block_size = rhs.block_size;
        index = rhs.index;
        buffer = rhs.buffer;

        return *this;
    }

    RingBuffer& operator=(RingBuffer&& rhs) {
        processor = std::move(rhs.processor);
        nchannels = std::move(rhs.nchannels);
        block_size = std::move(rhs.block_size);
        index = std::move(rhs.index);
        buffer = std::move(rhs.buffer);

        return *this;
    }

    void resize(const int nchannels, const int block_size) {
        if (nchannels != this->nchannels) {
            this->nchannels = nchannels;
            buffer = std::vector<std::vector<ConversionType>>(nchannels);
            this->block_size = 0;
            index = 0;
        }
        if (this->block_size != block_size) {
            this->block_size = block_size;
            for (int i = 0; i < nchannels; ++i)
                buffer[i] = std::vector<ConversionType>(block_size);
            index = 0;
        }
    }

    void set_nchannels(const int nchannels) { resize(nchannels, block_size); }

    void set_block_size(const int block_size) { resize(nchannels, block_size); }

    void set_processor(std::function<void(const int, ConversionType*, const int)> processor) {
        this->processor = processor;
    }

    int get_nchannels() const { return nchannels; }
    int get_block_size() const { return block_size; }

    void add(SampleType** samples, const int nsamples) {
        int remaining = nsamples;
        int offset = 0;

        while (remaining > 0) {
            int max_process = block_size - index;
            int to_process = std::min(remaining, max_process);

            for (int channel = 0; channel < nchannels; ++channel) {
                add_impl(buffer[channel], samples[channel] + offset, to_process, index);
            }

            index += to_process;
            remaining -= to_process;
            offset += to_process;

            if (index == block_size) {
                for (int channel = 0; channel < nchannels; ++channel) {
                    processor(channel, buffer[channel].data(), offset - block_size);
                }
                index = 0;
            }
        }
    }

    void clear() { index = 0; }

   protected:
    std::function<void(const int, ConversionType*, const int)> processor;
    int nchannels;
    int block_size;
    int index;

    std::vector<std::vector<ConversionType>> buffer;
    template <SameType<SampleType> T>
    void add_impl(std::vector<ConversionType>& dest, const T* src, int count, int dest_offset) {
        std::copy_n(src, count, dest.begin() + dest_offset);
    }

    template <typename T>
    void add_impl(std::vector<ConversionType>& dest, const T* src, int count, int dest_offset) {
        std::transform(src, src + count, dest.begin() + dest_offset,
                       [](const T& sample) { return static_cast<ConversionType>(sample); });
    }
};
}  // namespace audio
}  // namespace njones
