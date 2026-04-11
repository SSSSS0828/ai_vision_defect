#pragma once

#include "Types.h"
#include <QMainWindow>
#include <QFile>
#include <memory>

class ImageRenderer;
class StatisticsPanel;
class ImagePipeline;
class InferenceEngine;
class FramePool;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onOpenImage();
    void onStartCamera();
    void onStartSimulation();
    void onStopPipeline();
    void onLoadModel();
    void onFrameReady(FramePtr frame);
    void onPipelineError(const QString& msg);
    void onZoomIn();
    void onZoomOut();
    void onFitView();

private:
    void buildUI();
    void buildToolbar();
    void wireSignals();
    void tryLoadEngine(const QString& modelPath);
    std::unique_ptr<ImagePipeline> makePipeline();

    std::shared_ptr<FramePool>       m_pool;
    std::shared_ptr<InferenceEngine> m_engine;   // nullptr = 无模型模式
    std::unique_ptr<ImagePipeline>   m_pipeline;

    ImageRenderer*   m_renderer   = nullptr;
    StatisticsPanel* m_statsPanel = nullptr;
};
