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
    void onOpenImage();         // 打开图片
    void onStartCamera();       // 启动摄像头
    void onStartSimulation();   // 启动模拟
    void onStopPipeline();      // 停止图像处理管道
    void onLoadModel();         // 加载模型
    void onFrameReady(FramePtr frame);      // 帧处理完成
    void onPipelineError(const QString& msg);   // 处理管道错误
    // 图像缩放控制
    void onZoomIn();
    void onZoomOut();
    void onFitView();

private:
    void buildUI();             // 构建用户界面
    void buildToolbar();        // 构建工具栏
    void wireSignals();         // 信号连接
    void tryLoadEngine(const QString& modelPath);
    std::unique_ptr<ImagePipeline> makePipeline();

    std::shared_ptr<FramePool>       m_pool;        // 帧池
    std::shared_ptr<InferenceEngine> m_engine;   // nullptr = 无模型模式
    std::unique_ptr<ImagePipeline>   m_pipeline;    //图像处理管道

    ImageRenderer*   m_renderer   = nullptr;    // 图像渲染器
    StatisticsPanel* m_statsPanel = nullptr;    // 统计面板
};
