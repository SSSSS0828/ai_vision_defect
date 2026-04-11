#include "ImagePipeline.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <QDir>
#include <QStringList>
#include <QThread>
#include <QDebug>
#include <chrono>
#include <thread>

// ═══════════════════════════════════════════════════════════════════════════
// PipelineWorker — 运行在独立 QThread 中
// ═══════════════════════════════════════════════════════════════════════════

void PipelineWorker::runCamera() {
    cv::VideoCapture cap(camIndex);
    if (!cap.isOpened()) {
        emit error(QString("无法打开摄像头 %1").arg(camIndex));
        return;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH,  1920);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 1080);

    while (running->load()) {
        cv::Mat frame;
        if (!cap.read(frame) || frame.empty()) {
            emit error("摄像头读取失败");
            break;
        }
        emit frameReady(frame.clone());   // clone: worker→pipeline 跨线程传值

        int sleepMs = 1000 / std::max(1, fps->load());
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}

void PipelineWorker::runSimulation() {
    QDir dir(simDir);
    QStringList filters{"*.jpg","*.jpeg","*.png","*.bmp","*.tiff","*.tif"};
    QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);

    if (files.isEmpty()) {
        emit error("文件夹中没有图片: " + simDir);
        return;
    }

    int idx = 0;
    while (running->load()) {
        QString path = dir.absoluteFilePath(files[idx % files.size()]);
        cv::Mat img  = cv::imread(path.toStdString(), cv::IMREAD_COLOR);
        if (!img.empty()) {
            emit frameReady(img);   // img 本身就是新分配的，不需要 clone
        }
        ++idx;

        int sleepMs = 1000 / std::max(1, fps->load());
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// ImagePipeline
// ═══════════════════════════════════════════════════════════════════════════

ImagePipeline::ImagePipeline(std::shared_ptr<FramePool>      pool,
                             std::shared_ptr<InferenceEngine> engine,
                             QObject* parent)
    : QObject(parent)
    , m_pool(std::move(pool))
    , m_engine(std::move(engine))
{}

ImagePipeline::~ImagePipeline() {
    stop();
}

void ImagePipeline::stop() {
    m_running.store(false);
    if (m_thread) {
        m_thread->quit();
        m_thread->wait(2000);
        // m_thread 和 m_worker 由 deleteLater 自动清理
        m_thread = nullptr;
        m_worker = nullptr;
    }
}

// ─── pushMat: 把 cv::Mat 写入帧池，再路由到推理引擎或直通 UI ───────────────
void ImagePipeline::pushMat(cv::Mat&& raw) {
    if (raw.empty()) return;

    FramePtr frame = m_pool->tryAcquire();
    if (!frame) {
        qWarning() << "[ImagePipeline] 帧池耗尽，丢弃帧";
        return;
    }

    // 确保 mat 尺寸与原始图一致（第一帧或分辨率变化时重建头部）
    if (frame->mat->rows != raw.rows || frame->mat->cols != raw.cols ||
        frame->mat->type() != raw.type()) {
        *frame->mat = cv::Mat(raw.size(), raw.type(), frame->mat->data);
    }
    raw.copyTo(*frame->mat);

    if (m_engine) {
        m_engine->enqueue(std::move(frame));
    } else {
        emit framePassthrough(std::move(frame));
    }
}

// ─── loadImage: 直接在调用线程同步执行（无需工作线程）───────────────────────
void ImagePipeline::loadImage(const QString& path) {
    cv::Mat img = cv::imread(path.toStdString(), cv::IMREAD_COLOR);
    if (img.empty()) {
        emit acquisitionError("无法加载图片: " + path);
        return;
    }
    pushMat(std::move(img));
}

// ─── startWorker: 启动工作线程（camera / simulation 共用）───────────────────
void ImagePipeline::startWorker(bool camera) {
    m_running.store(true);

    m_worker = new PipelineWorker();
    m_worker->running  = &m_running;
    m_worker->fps      = &m_targetFps;
    m_worker->simDir   = m_simDir;
    m_worker->camIndex = 0;   // camera 模式由 startCamera 覆写

    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);   // worker 迁移到工作线程，无 QSocketNotifier 警告

    // worker 的信号通过队列连接回到主线程的 ImagePipeline slot
    connect(m_worker, &PipelineWorker::frameReady,
            this, &ImagePipeline::onWorkerFrame, Qt::QueuedConnection);
    connect(m_worker, &PipelineWorker::error,
            this, &ImagePipeline::acquisitionError, Qt::QueuedConnection);

    // 线程启动时调用对应的 slot（在工作线程执行）
    if (camera) {
        connect(m_thread, &QThread::started, m_worker, &PipelineWorker::runCamera);
    } else {
        connect(m_thread, &QThread::started, m_worker, &PipelineWorker::runSimulation);
    }

    // 线程结束后自动清理
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_thread->start();
}

void ImagePipeline::startCamera(int deviceIndex) {
    stop();
    m_running.store(true);

    m_worker = new PipelineWorker();
    m_worker->running  = &m_running;
    m_worker->fps      = &m_targetFps;
    m_worker->camIndex = deviceIndex;

    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);

    connect(m_worker, &PipelineWorker::frameReady,
            this, &ImagePipeline::onWorkerFrame, Qt::QueuedConnection);
    connect(m_worker, &PipelineWorker::error,
            this, &ImagePipeline::acquisitionError, Qt::QueuedConnection);
    connect(m_thread, &QThread::started,  m_worker, &PipelineWorker::runCamera);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_thread->start();
}

void ImagePipeline::startSimulation(const QString& imageDir, int fpsTarget) {
    stop();
    m_simDir = imageDir;
    m_targetFps.store(fpsTarget);

    m_worker = new PipelineWorker();
    m_worker->running = &m_running;
    m_worker->fps     = &m_targetFps;
    m_worker->simDir  = m_simDir;

    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);

    connect(m_worker, &PipelineWorker::frameReady,
            this, &ImagePipeline::onWorkerFrame, Qt::QueuedConnection);
    connect(m_worker, &PipelineWorker::error,
            this, &ImagePipeline::acquisitionError, Qt::QueuedConnection);
    connect(m_thread, &QThread::started,  m_worker, &PipelineWorker::runSimulation);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_running.store(true);
    m_thread->start();
}

// ─── onWorkerFrame: 在主线程中接收来自工作线程的帧，写池并路由 ───────────────
void ImagePipeline::onWorkerFrame(cv::Mat mat) {
    pushMat(std::move(mat));
}
