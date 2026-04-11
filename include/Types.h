#pragma once

#include <opencv2/core.hpp>
#include <QRectF>
#include <QString>
#include <vector>
#include <memory>
#include <chrono>
#include <map>

// ─────────────────────────────────────────────────────────────────────────────
// DefectType
// ─────────────────────────────────────────────────────────────────────────────
enum class DefectType {
    Scratch,
    Crack,
    Dent,
    Stain,
    Missing,
    Unknown
};

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

// ─────────────────────────────────────────────────────────────────────────────
// DetectionResult  — one bounding box from YOLO
// ─────────────────────────────────────────────────────────────────────────────
struct DetectionResult {
    DefectType  type       = DefectType::Unknown;
    float       confidence = 0.f;
    cv::Rect    bbox;           // pixel coordinates in the original image
    std::vector<cv::Point> contour; // optional fine contour from mask / approxPoly
};

// ─────────────────────────────────────────────────────────────────────────────
// ImageFrame  — unit of work flowing through the pipeline
//
//   mat: raw BGR image owned by this frame (or shared via zero-copy wrapper).
//       On acquisition the FramePool hands out a pre-allocated cv::Mat.
//       The custom deleter on shared_ptr returns the buffer to the pool.
//
//   results: filled by InferenceEngine after detection.
//   frameId / timestamp: bookkeeping.
// ─────────────────────────────────────────────────────────────────────────────
struct ImageFrame {
    uint64_t    frameId   = 0;
    std::chrono::steady_clock::time_point timestamp;

    // The cv::Mat wraps externally-allocated memory from FramePool.
    // The shared_ptr's custom deleter is responsible for returning the buffer.
    std::shared_ptr<cv::Mat> mat;

    std::vector<DetectionResult> results;
    bool inferenceComplete = false;
};

using FramePtr = std::shared_ptr<ImageFrame>;

// ─────────────────────────────────────────────────────────────────────────────
// PipelineStats  — live metrics displayed in StatisticsPanel
// ─────────────────────────────────────────────────────────────────────────────
struct PipelineStats {
    uint64_t totalFrames    = 0;
    uint64_t defectFrames   = 0;
    uint64_t totalDefects   = 0;
    double   avgInferenceMs = 0.0;
    double   fps            = 0.0;
    std::map<DefectType, int> defectCounts;
};
