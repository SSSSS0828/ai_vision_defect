#include "FramePool.h"

#include <opencv2/core.hpp>
#include <cassert>
#include <stdexcept>

FramePool::FramePool(int capacity, int width, int height, int cvType)
    : m_capacity(capacity)
    , m_width(width)
    , m_height(height)
    , m_cvType(cvType)
{
    m_elemSize = static_cast<int>(CV_ELEM_SIZE(cvType));

    m_storage.resize(capacity);
    for (int i = 0; i < capacity; ++i) {
        m_storage[i].resize(static_cast<size_t>(width) * height * m_elemSize, 0);
        m_free.push(m_storage[i].data());
    }
}

FramePool::~FramePool() {}

int FramePool::available() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return static_cast<int>(m_free.size());
}

FramePtr FramePool::acquire() {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_cv.wait(lk, [this]{ return !m_free.empty(); });
    return makeFrame(lk);
}

FramePtr FramePool::tryAcquire() {
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_free.empty()) return nullptr;
    return makeFrame(lk);
}

// Zero-copy core:
//   cv::Mat header wraps buf WITHOUT owning it.
//   Custom deleter on shared_ptr<cv::Mat> returns buf to pool when refcount hits 0.
FramePtr FramePool::makeFrame(std::unique_lock<std::mutex>& /*lk*/) {
    uint8_t* buf = m_free.top();
    m_free.pop();

    size_t step = static_cast<size_t>(m_width) * m_elemSize;

    // Mat header allocated on heap; pixel data points to pool buffer (no copy).
    cv::Mat* matHeap = new cv::Mat(m_height, m_width, m_cvType, buf, step);

    FramePool* self = this;
    auto matPtr = std::shared_ptr<cv::Mat>(matHeap, [self, buf](cv::Mat* m) {
        delete m;                 // free Mat header
        self->returnBuffer(buf);  // return pixel buffer to pool
    });

    auto frame      = std::make_shared<ImageFrame>();
    frame->frameId  = m_nextId++;
    frame->timestamp = std::chrono::steady_clock::now();
    frame->mat      = matPtr;
    return frame;
}

void FramePool::returnBuffer(uint8_t* buf) {
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_free.push(buf);
    }
    m_cv.notify_one();
}
