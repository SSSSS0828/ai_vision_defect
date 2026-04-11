#pragma once

#include "Types.h"
#include "FramePool.h"
#include "InferenceEngine.h"

#include <QObject>
#include <QString>
#include <atomic>
#include <memory>

// ─── PipelineWorker ──────────────────────────────────────────────────────────
// 真正在工作线程中运行的对象。
// 通过 moveToThread() 迁移到 QThread，槽函数在工作线程中执行，
// 不触发 QSocketNotifier 警告。
class PipelineWorker : public QObject {
    Q_OBJECT
public:
    explicit PipelineWorker(QObject* parent = nullptr) : QObject(parent) {}

    std::atomic<bool>* running = nullptr;
    std::atomic<int>*  fps     = nullptr;
    QString            simDir;
    int                camIndex = 0;

public slots:
    void runCamera();
    void runSimulation();

signals:
    void frameReady(cv::Mat mat);   // 把 Mat 发回 ImagePipeline（同进程内拷贝一次）
    void error(const QString& msg);
};

// ─── ImagePipeline ───────────────────────────────────────────────────────────
class ImagePipeline : public QObject {
    Q_OBJECT
public:
    // engine 可以为 nullptr（无模型直通模式）
    explicit ImagePipeline(std::shared_ptr<FramePool>      pool,
                           std::shared_ptr<InferenceEngine> engine,
                           QObject* parent = nullptr);
    ~ImagePipeline() override;

    void loadImage(const QString& path);
    void startCamera(int deviceIndex = 0);
    void startSimulation(const QString& imageDir, int fpsTarget = 10);
    void stop();

    bool isRunning() const { return m_running.load(); }

signals:
    void acquisitionError(const QString& msg);
    void framePassthrough(FramePtr frame);   // 无模型时直接发给 UI

private slots:
    void onWorkerFrame(cv::Mat mat);

private:
    void pushMat(cv::Mat&& raw);
    void startWorker(bool camera);

    std::shared_ptr<FramePool>       m_pool;
    std::shared_ptr<InferenceEngine> m_engine;

    std::atomic<bool> m_running{false};
    std::atomic<int>  m_targetFps{10};
    QString           m_simDir;

    QThread*        m_thread = nullptr;
    PipelineWorker* m_worker = nullptr;
};
