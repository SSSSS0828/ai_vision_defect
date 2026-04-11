#include "YoloDetector.h"

#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <algorithm>

// ─── Construction ─────────────────────────────────────────────────────────────
YoloDetector::YoloDetector(const Config& cfg) : m_cfg(cfg) {
    m_net = cv::dnn::readNetFromONNX(cfg.modelPath);
    if (m_net.empty()) {
        throw std::runtime_error("YoloDetector: failed to load model from " + cfg.modelPath);
    }

#ifdef USE_CUDA
    if (cfg.useCuda) {
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
    } else
#endif
    {
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    }
}

// ─── detect ───────────────────────────────────────────────────────────────────
std::vector<DetectionResult> YoloDetector::detect(const cv::Mat& frame) const {
    // 1. Letterbox resize to model input size (preserve aspect ratio)
    int origW = frame.cols, origH = frame.rows;
    float scaleX = static_cast<float>(m_cfg.inputWidth)  / origW;
    float scaleY = static_cast<float>(m_cfg.inputHeight) / origH;
    float scale  = std::min(scaleX, scaleY);

    int newW = static_cast<int>(origW * scale);
    int newH = static_cast<int>(origH * scale);
    int padX = (m_cfg.inputWidth  - newW) / 2;
    int padY = (m_cfg.inputHeight - newH) / 2;

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(newW, newH));
    cv::Mat padded(m_cfg.inputHeight, m_cfg.inputWidth, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(padded(cv::Rect(padX, padY, newW, newH)));

    // 2. Blob conversion — NHWC→NCHW, normalize [0,255]→[0,1]
    cv::Mat blob;
    cv::dnn::blobFromImage(padded, blob, 1.0 / 255.0,
                           cv::Size(m_cfg.inputWidth, m_cfg.inputHeight),
                           cv::Scalar(), true, false);

    const_cast<cv::dnn::Net&>(m_net).setInput(blob);

    // 3. Forward pass
    std::vector<cv::Mat> outputs;
    std::vector<std::string> outNames = const_cast<cv::dnn::Net&>(m_net).getUnconnectedOutLayersNames();
    const_cast<cv::dnn::Net&>(m_net).forward(outputs, outNames);

    // 4. Post-process
    auto results = postProcess(frame, outputs);

    // 5. Remap bounding boxes from padded-input space back to original image space
    for (auto& r : results) {
        r.bbox.x      = static_cast<int>((r.bbox.x - padX) / scale);
        r.bbox.y      = static_cast<int>((r.bbox.y - padY) / scale);
        r.bbox.width  = static_cast<int>(r.bbox.width  / scale);
        r.bbox.height = static_cast<int>(r.bbox.height / scale);

        // Clamp to image bounds
        r.bbox &= cv::Rect(0, 0, origW, origH);
    }

    return results;
}

// ─── postProcess ─────────────────────────────────────────────────────────────
// YOLOv5/v8 output shape: [1, num_boxes, 5 + num_classes]
// Each row: [cx, cy, w, h, obj_conf, cls0_conf, cls1_conf, ...]
std::vector<DetectionResult> YoloDetector::postProcess(
    const cv::Mat& /*frame*/,
    const std::vector<cv::Mat>& outputs) const
{
    std::vector<int>       classIds;
    std::vector<float>     confidences;
    std::vector<cv::Rect>  boxes;

    for (const auto& output : outputs) {
        // Reshape to [num_detections, row_size]
        auto* data = reinterpret_cast<const float*>(output.data);
        int rows    = output.size[1];
        int rowSize = output.size[2];

        for (int i = 0; i < rows; ++i) {
            const float* row = data + i * rowSize;
            float objConf = row[4];
            if (objConf < m_cfg.confThreshold) continue;

            // Find best class
            int   bestCls  = -1;
            float bestConf = 0.f;
            for (int c = 0; c < static_cast<int>(m_cfg.classNames.size()); ++c) {
                float cf = row[5 + c] * objConf;
                if (cf > bestConf) { bestConf = cf; bestCls = c; }
            }
            if (bestCls < 0 || bestConf < m_cfg.confThreshold) continue;

            float cx = row[0], cy = row[1], w = row[2], h = row[3];
            int x = static_cast<int>(cx - w / 2);
            int y = static_cast<int>(cy - h / 2);

            classIds.push_back(bestCls);
            confidences.push_back(bestConf);
            boxes.emplace_back(x, y, static_cast<int>(w), static_cast<int>(h));
        }
    }

    // NMS
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, m_cfg.confThreshold, m_cfg.nmsThreshold, indices);

    std::vector<DetectionResult> results;
    results.reserve(indices.size());
    for (int idx : indices) {
        DetectionResult r;
        r.bbox       = boxes[idx];
        r.confidence = confidences[idx];
        r.type       = classToDefect(m_cfg.classNames[classIds[idx]]);
        results.push_back(std::move(r));
    }
    return results;
}

// ─── classToDefect ────────────────────────────────────────────────────────────
DefectType YoloDetector::classToDefect(const std::string& name) {
    if (name == "scratch")  return DefectType::Scratch;
    if (name == "crack")    return DefectType::Crack;
    if (name == "dent")     return DefectType::Dent;
    if (name == "stain")    return DefectType::Stain;
    if (name == "missing")  return DefectType::Missing;
    return DefectType::Unknown;
}
