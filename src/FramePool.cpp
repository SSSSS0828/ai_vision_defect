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
    // 计算每个像素所占字节数
    m_elemSize = static_cast<int>(CV_ELEM_SIZE(cvType));

    // 预分配存储空间
    m_storage.resize(capacity);
    // 循环分配内存并加入空闲栈
    for (int i = 0; i < capacity; ++i) {
        m_storage[i].resize(static_cast<size_t>(width) * height * m_elemSize, 0);
        m_free.push(m_storage[i].data());
    }
}

FramePool::~FramePool() {}


// 查询可用资源 提供一个线程安全的方法查询当前有多少空闲的缓冲区
int FramePool::available() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return static_cast<int>(m_free.size());
}

// 阻塞式获取 
FramePtr FramePool::acquire() {
    std::unique_lock<std::mutex> lk(m_mutex);
    m_cv.wait(lk, [this]{ return !m_free.empty(); });
    return makeFrame(lk);
}

// 非阻塞式获取
FramePtr FramePool::tryAcquire() {
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_free.empty()) return nullptr;
    return makeFrame(lk);
}

// 内部创建帧
FramePtr FramePool::makeFrame(std::unique_lock<std::mutex>& /*lk*/) {
    // 从空闲栈中获取一个缓冲区指针
    uint8_t* buf = m_free.top();
    m_free.pop();

    // 计算一行像素的字节数
    size_t step = static_cast<size_t>(m_width) * m_elemSize;

    // 在堆上创建 cv::Mat 头，指向池中的 buf，不拷贝数据
    cv::Mat* matHeap = new cv::Mat(m_height, m_width, m_cvType, buf, step);

    FramePool* self = this;

    // 创建 shared_ptr<cv::Mat>，绑定自定义删除器
    auto matPtr = std::shared_ptr<cv::Mat>(matHeap, [self, buf](cv::Mat* m) {
        delete m;                 // 释放 Mat header
        self->returnBuffer(buf);  // 自动归还像素内存
    });

    // 创建 ImageFrame 对象
    auto frame      = std::make_shared<ImageFrame>();
    frame->frameId  = m_nextId++;
    frame->timestamp = std::chrono::steady_clock::now();
    frame->mat      = matPtr;
    return frame;
}

// 归还缓冲区
void FramePool::returnBuffer(uint8_t* buf) {
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_free.push(buf);                           //将指针放回空闲栈中
    }
    m_cv.notify_one();                              //通知一个新的等待的线程
}
