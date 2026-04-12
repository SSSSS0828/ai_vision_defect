#pragma once

#include "Types.h"

#include <vector>
#include <stack>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <cstdint>

// 对象池 解决高频场景下，频繁动态分配和释放大型 cv::Mat 内存导致的堆碎片化和延迟抖动问题
class FramePool {
public:
    FramePool(int capacity, int width, int height, int cvType = CV_8UC3);   // 参数为 对象池容量，宽，高，数据类型 压入m_free
    ~FramePool();

    // 阻塞式获取对象
    FramePtr acquire();

    // 非阻塞式获取对象
    FramePtr tryAcquire();

    // 监控 池状态
    int capacity()  const { return m_capacity; }
    int available() const;

private:
    
    FramePtr makeFrame(std::unique_lock<std::mutex>& lk);

    void returnBuffer(uint8_t* buf);

    int  m_capacity;        // 池容量
    int  m_width;           // 宽
    int  m_height;          // 高   
    int  m_cvType;          // 数据类型
    int  m_elemSize;        // 数据类型大小

    std::vector<std::vector<uint8_t>> m_storage;   
    std::stack<uint8_t*>              m_free;       
    mutable std::mutex                m_mutex;
    std::condition_variable           m_cv;

    uint64_t m_nextId = 0;
};
