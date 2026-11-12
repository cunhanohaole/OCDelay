#pragma once

#include <array>
#include <atomic>

// Lock-free hand-off of a value from one writer thread to one reader thread.
// The writer fills getWriteBuffer() and calls publish(); the reader calls read() and always
// gets the newest complete value. Neither side blocks or allocates.
template <typename T>
class TripleBuffer
{
public:
    explicit TripleBuffer (const T& initialValue = {})
    {
        buffers.fill (initialValue);
    }

    // Writer thread only
    T& getWriteBuffer() { return buffers[(size_t) writeIndex]; }

    // Writer thread only: makes the write buffer the newest value
    void publish()
    {
        const auto previous = middle.exchange (writeIndex | kFreshBit, std::memory_order_acq_rel);
        writeIndex = previous & kIndexMask;
    }

    // Reader thread only
    const T& read()
    {
        if ((middle.load (std::memory_order_acquire) & kFreshBit) != 0)
        {
            const auto previous = middle.exchange (readIndex, std::memory_order_acq_rel);
            readIndex = previous & kIndexMask;
        }

        return buffers[(size_t) readIndex];
    }

private:
    static constexpr int kIndexMask = 0x3;
    static constexpr int kFreshBit = 0x4;

    std::array<T, 3> buffers;
    int writeIndex = 0; // owned by the writer
    int readIndex = 1; // owned by the reader
    std::atomic<int> middle { 2 }; // index of the spare buffer, plus the fresh bit
};
