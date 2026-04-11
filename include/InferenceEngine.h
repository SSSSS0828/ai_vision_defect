#pragma once

/*
 * InferenceEngine — runs YOLO inference in a dedicated worker thread.
 *
 * Sits in the middle of the producer-consumer pipeline:
 *   ImagePipeline (producer) → InferenceEngine → UI (consumer)
 *
 * Thread model:
 *   The engine owns one std::thread.  Frames are submitted via enqueue()
 *   from the acquisition thread.  Results are emitted via frameReady()
 *   signal on the engine's internal thread (Qt::QueuedConnection bridges
 *   it safely to the GUI thread).
 *
 * Back-pressure:
 *   If the inference queue is full (> MAX_QUEUE), the oldest frame is
 *   dropped and a warning is logged.  This prevents unbounded memory growth
 *   under slow inference / fast camera scenarios.
 */

#include "Types.h"
#include "YoloDetector.h"

#include <QObject>
#include <QThread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

class InferenceEngine : public QObject {
    Q_OBJECT
public:
    explicit InferenceEngine(std::unique_ptr<YoloDetector> detector,
                             int maxQueueDepth = 4,
                             QObject* parent = nullptr);
    ~InferenceEngine() override;

    // Thread-safe: called from acquisition/producer thread
    void enqueue(FramePtr frame);

    // Start/stop the worker thread
    void start();
    void stop();

    bool isRunning() const { return m_running.load(); }

signals:
    // Emitted from worker thread; connect with Qt::QueuedConnection to GUI
    void frameReady(FramePtr frame);
    void inferenceError(const QString& msg);

private:
    void workerLoop();

    std::unique_ptr<YoloDetector>  m_detector;
    int                            m_maxQueue;

    std::queue<FramePtr>           m_queue;
    mutable std::mutex             m_mutex;
    std::condition_variable        m_cv;

    std::atomic<bool>              m_running{false};
    std::unique_ptr<QThread>       m_thread;
};
