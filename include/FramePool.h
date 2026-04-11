#pragma once

/*
 * FramePool — Object pool for pre-allocated image frame buffers.
 *
 * Rationale:
 *   Industrial cameras produce frames at high frequency (30-120 fps).
 *   Allocating / deallocating large cv::Mat buffers on every frame triggers
 *   heap fragmentation and adds latency spikes.  The pool pre-allocates N
 *   buffers of (width x height x channels) bytes and recycles them via a
 *   mutex-protected stack with a condition_variable for blocking acquire().
 *
 * Zero-copy contract:
 *   acquire() returns a FramePtr whose cv::Mat points directly into the
 *   pre-allocated buffer.  The shared_ptr carries a custom deleter that calls
 *   returnBuffer() when the last owner releases the frame — no explicit
 *   release() call is ever required by callers.
 */

#include "Types.h"

#include <vector>
#include <stack>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <cstdint>

class FramePool {
public:
    FramePool(int capacity, int width, int height, int cvType = CV_8UC3);
    ~FramePool();

    // Block until a buffer is available, return zero-copy FramePtr.
    FramePtr acquire();

    // Non-blocking; returns nullptr if pool is exhausted.
    FramePtr tryAcquire();

    int capacity()  const { return m_capacity; }
    int available() const;

private:
    // Called with lock held — pops a buffer, builds FramePtr with custom deleter.
    FramePtr makeFrame(std::unique_lock<std::mutex>& lk);

    // Called by the custom deleter (lock-free push + notify).
    void returnBuffer(uint8_t* buf);

    int  m_capacity;
    int  m_width;
    int  m_height;
    int  m_cvType;
    int  m_elemSize;

    std::vector<std::vector<uint8_t>> m_storage;   // backing memory
    std::stack<uint8_t*>              m_free;       // available buffer pointers
    mutable std::mutex                m_mutex;
    std::condition_variable           m_cv;

    uint64_t m_nextId = 0;
};
