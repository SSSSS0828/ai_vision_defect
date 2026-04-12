#include "MainWindow.h"
#include "ImageRenderer.h"
#include "StatisticsPanel.h"
#include "ImagePipeline.h"
#include "InferenceEngine.h"
#include "FramePool.h"
#include "YoloDetector.h"

#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QLabel>
#include <QInputDialog>
#include <QCoreApplication>
#include <QDir>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("AI Vision Defect Detection");
    resize(1400, 860);

    buildUI();
    buildToolbar();

    // 预分配帧池：8 帧，1920×1080，BGR
    m_pool = std::make_shared<FramePool>(8, 1920, 1080, CV_8UC3);

    // 尝试加载默认模型（找不到进入无推理模式）
    QString defaultModel = QCoreApplication::applicationDirPath()
                           + "/models/defect_yolov5s.onnx";
    tryLoadEngine(defaultModel);

    wireSignals();
    statusBar()->showMessage("就绪 — 打开图片、启动相机或仿真模式");
}

MainWindow::~MainWindow() {
    onStopPipeline();
    if (m_engine) m_engine->stop();
}

void MainWindow::buildUI() {
    // 水平分割布局
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    m_renderer   = new ImageRenderer(this);
    m_statsPanel = new StatisticsPanel(this);
    splitter->addWidget(m_renderer);
    splitter->addWidget(m_statsPanel);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);
}

void MainWindow::buildToolbar() {
    auto* tb = addToolBar("Main");
    tb->setMovable(false);

    auto* actOpen    = tb->addAction("Open Image");
    auto* actCamera  = tb->addAction("Start Camera");
    auto* actSim     = tb->addAction("Simulation");
    auto* actStop    = tb->addAction("Stop");
    tb->addSeparator();
    auto* actModel   = tb->addAction("Load Model");
    tb->addSeparator();
    auto* actZoomIn  = tb->addAction("Zoom In");
    auto* actZoomOut = tb->addAction("Zoom Out");
    auto* actFit     = tb->addAction("Fit View");

    connect(actOpen,    &QAction::triggered, this, &MainWindow::onOpenImage);
    connect(actCamera,  &QAction::triggered, this, &MainWindow::onStartCamera);
    connect(actSim,     &QAction::triggered, this, &MainWindow::onStartSimulation);
    connect(actStop,    &QAction::triggered, this, &MainWindow::onStopPipeline);
    connect(actModel,   &QAction::triggered, this, &MainWindow::onLoadModel);
    connect(actZoomIn,  &QAction::triggered, this, &MainWindow::onZoomIn);
    connect(actZoomOut, &QAction::triggered, this, &MainWindow::onZoomOut);
    connect(actFit,     &QAction::triggered, this, &MainWindow::onFitView);
}

// ─── 尝试加载推理引擎（失败时降级为纯显示模式）─────────────────────────
void MainWindow::tryLoadEngine(const QString& modelPath) {
    if (!QFile::exists(modelPath)) {
        statusBar()->showMessage("无模型 — 仅显示模式（可通过 Load Model 加载）");
        return;
    }

    YoloDetector::Config cfg;
    cfg.modelPath   = modelPath.toStdString();
    cfg.classNames  = {"scratch", "crack", "dent", "stain", "missing"};
    // 检测阈值
    cfg.confThreshold = 0.45f;
    cfg.nmsThreshold  = 0.45f;

    try {
        // 创建推理引擎检测器实例
        auto det = std::make_unique<YoloDetector>(cfg);
        // 传递检测器到推理引擎实例
        m_engine = std::make_shared<InferenceEngine>(std::move(det));
        // 启动推理
        m_engine->start();
        // 绑定信号
        wireSignals();
        statusBar()->showMessage("模型已加载: " + modelPath);
    } catch (const std::exception& e) {
        statusBar()->showMessage(QString("模型加载失败: %1").arg(e.what()));
    }
}

void MainWindow::wireSignals() {
    // 检测是否有推理引擎
    if (!m_engine) return;
    // 断开全部连接 避免重复连接
    disconnect(m_engine.get(), nullptr, this, nullptr);
    // 当推理完成并有新真可用时触发
    connect(m_engine.get(), &InferenceEngine::frameReady,
            this, &MainWindow::onFrameReady, Qt::QueuedConnection);
    // 当推理发生错误时触发
    connect(m_engine.get(), &InferenceEngine::inferenceError,
            this, &MainWindow::onPipelineError, Qt::QueuedConnection);
}

// ─── 创建流水线（有无模型都能工作）──────────────────────────────────────────
// 无模型时：直接把帧送给 renderer，跳过推理
std::unique_ptr<ImagePipeline> MainWindow::makePipeline() {
    if (m_engine) {
        // 有模型：走正常推理流水线
        auto p = std::make_unique<ImagePipeline>(m_pool, m_engine, this);
        connect(p.get(), &ImagePipeline::acquisitionError,
                this, &MainWindow::onPipelineError, Qt::QueuedConnection);
        return p;
    } else {
        // 无模型：走直通流水线（帧直接发给 renderer）
        auto p = std::make_unique<ImagePipeline>(m_pool, nullptr, this);
        connect(p.get(), &ImagePipeline::acquisitionError,
                this, &MainWindow::onPipelineError, Qt::QueuedConnection);
        connect(p.get(), &ImagePipeline::framePassthrough,
                this, &MainWindow::onFrameReady, Qt::QueuedConnection);
        return p;
    }
}

// ─── Slots ───────────────────────────────────────────────────────────────────
void MainWindow::onOpenImage() {
    QString path = QFileDialog::getOpenFileName(
        this, "打开图片", QDir::homePath(),
        "图片文件 (*.jpg *.jpeg *.png *.bmp *.tiff *.tif)");
    if (path.isEmpty()) return;

    onStopPipeline();
    m_pipeline = makePipeline();
    m_pipeline->loadImage(path);
    statusBar()->showMessage("已加载: " + path);
}

void MainWindow::onStartCamera() {
    bool ok;
    int idx = QInputDialog::getInt(this, "摄像头", "设备编号 (通常为 0):", 0, 0, 10, 1, &ok);
    if (!ok) return;

    onStopPipeline();
    m_pipeline = makePipeline();
    m_pipeline->startCamera(idx);
    statusBar()->showMessage(QString("摄像头 %1 运行中").arg(idx));
}

void MainWindow::onStartSimulation() {
    QString dir = QFileDialog::getExistingDirectory(
        this, "选择图片文件夹", QDir::homePath());
    if (dir.isEmpty()) return;

    onStopPipeline();
    m_pipeline = makePipeline();
    m_pipeline->startSimulation(dir, 10);
    statusBar()->showMessage("仿真模式: " + dir);
}

void MainWindow::onStopPipeline() {
    if (m_pipeline) {
        m_pipeline->stop();
        m_pipeline.reset();
    }
}

void MainWindow::onLoadModel() {
    QString path = QFileDialog::getOpenFileName(
        this, "选择 ONNX 模型", QDir::homePath(), "ONNX 模型 (*.onnx)");
    if (path.isEmpty()) return;
    if (m_engine) m_engine->stop();
    m_engine.reset();
    tryLoadEngine(path);
}

void MainWindow::onFrameReady(FramePtr frame) {
    m_renderer->displayFrame(frame);
    m_statsPanel->onFrameResult(frame);
}

void MainWindow::onPipelineError(const QString& msg) {
    statusBar()->showMessage("错误: " + msg);
}

void MainWindow::onZoomIn()  { m_renderer->zoomIn();    }
void MainWindow::onZoomOut() { m_renderer->zoomOut();   }
void MainWindow::onFitView() { m_renderer->fitInView(); }
