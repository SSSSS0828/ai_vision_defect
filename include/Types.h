#pragma once

#include <opencv2/core.hpp>
#include <QRectF>
#include <QString>
#include <vector>
#include <memory>
#include <chrono>
#include <map>

// 关系
// 组成关系：ImageFrame 拥有 一个 DetectionResult 的列表
// 引用关系：DetectionResult 和 PipelineStats 都依赖 DefectType 来标识缺陷种类
// 传递关系：FramePtr 是 ImageFrame 的“句柄”，用于在系统各模块间高效传递帧数据
// 聚合关系：PipelineStats 是对历史上所有处理过的 ImageFrame 及其内部 DetectionResult 的统计摘要

// 一、缺陷定义与结果

// 枚举值 检测结果类别
enum class DefectType {
    Scratch,
    Crack,
    Dent,
    Stain,
    Missing,
    Unknown
};

// 接收枚举类型转换为字符串
// inline 将函数体直接嵌入调用处 以减少函数调用开销 适合这种短小且可能被频繁调用的工具函数
inline QString defectTypeName(DefectType t) {
    switch (t) {
    case DefectType::Scratch:  return "Scratch";
    case DefectType::Crack:    return "Crack";
    case DefectType::Dent:     return "Dent";
    case DefectType::Stain:    return "Stain";
    case DefectType::Missing:  return "Missing";
    default:                   return "Unknown";
    }
}

// 结构体 用于封装单次缺陷检测结果数据
struct DetectionResult {
    DefectType  type       = DefectType::Unknown;           // 缺陷类别
    float       confidence = 0.f;                           // 置信度
    cv::Rect    bbox;                                       // 缺陷框
    std::vector<cv::Point> contour;                         // 缺陷轮廓
};

// 二、帧数据封装

// 结构体 用于封装图像帧数据
struct ImageFrame {
    uint64_t    frameId   = 0;                                  // 图像帧ID   
    std::chrono::steady_clock::time_point timestamp;            // 时间戳

    std::shared_ptr<cv::Mat> mat;                               // 指向图像数据的智能指针

    std::vector<DetectionResult> results;                       // 缺陷检测结果
    bool inferenceComplete = false;                             // 推理是否完成
};

// 创建一个新的帧对象，返回 shared_ptr
using FramePtr = std::shared_ptr<ImageFrame>;             

// 三、性能与业务统计

// 结构体 用于封装统计数据 收集和统计视觉检测流水线的运行性能指标及业务数据
struct PipelineStats {
    uint64_t totalFrames    = 0;                                 // 总帧数
    uint64_t defectFrames   = 0;                                 // 缺陷帧数
    uint64_t totalDefects   = 0;                                 // 总缺陷数   一帧可能包含多个缺陷
    double   avgInferenceMs = 0.0;                               // 平均推理时间
    double   fps            = 0.0;                               // 帧率
    std::map<DefectType, int> defectCounts;                      // 缺陷类别统计 
};


