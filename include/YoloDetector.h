#pragma once

/*
 * YoloDetector — thin wrapper around OpenCV DNN for YOLOv5/v8 ONNX inference.
 *
 * Design notes:
 *   • Stateless per-call: net is loaded once in the constructor; detect() is
 *     re-entrant IF each thread constructs its own YoloDetector instance.
 *   • Input normalisation, NMS, and class-to-DefectType mapping are encapsulated
 *     here so InferenceEngine does not need to know YOLO internals.
 *   • Supports optional CUDA backend (compile with -DOPENCV_DNN_CUDA=ON).
 */

#include "Types.h"
#include <opencv2/dnn.hpp>
#include <string>
#include <vector>

class YoloDetector {
public:
    struct Config {
        std::string modelPath;           // path to .onnx file
        std::vector<std::string> classNames; // ordered list matching model output
        float  confThreshold  = 0.45f;
        float  nmsThreshold   = 0.45f;
        int    inputWidth     = 640;
        int    inputHeight    = 640;
        bool   useCuda        = false;
    };

    explicit YoloDetector(const Config& cfg);

    // Returns detected bounding boxes for one frame (BGR image).
    std::vector<DetectionResult> detect(const cv::Mat& frame) const;

    bool isLoaded() const { return !m_net.empty(); }

private:
    cv::dnn::Net      m_net;
    Config            m_cfg;

    // Map YOLO class index → DefectType
    static DefectType classToDefect(const std::string& className);

    // Post-process raw YOLO output blobs → DetectionResult list
    std::vector<DetectionResult> postProcess(
        const cv::Mat& frame,
        const std::vector<cv::Mat>& outputs) const;
};
