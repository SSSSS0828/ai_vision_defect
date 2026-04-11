#include "InferenceEngine.h"

#include <QDebug>
#include <chrono>

// ─── Construction ─────────────────────────────────────────────────────────────
InferenceEngine::InferenceEngine(std::unique_ptr<YoloDetector> detector,
                                 int maxQueueDepth,
                                 QObject* parent)
    : QObject(parent)
    , m_detector(std::move(detector))
    , m_maxQueue(maxQueueDepth)
{}

InferenceEngine::~InferenceEngine() {
    stop();
}

// ─── start / stop ─────────────────────────────────────────────────────────────
void InferenceEngine::start() {
    if (m_running.load()) return;
    m_running.store(true);

    m_thread = std::make_unique<QThread>();
    // Move this QObject to the worker thread so signals are emitted there
    this->moveToThread(m_thread.get());

    connect(m_thread.get(), &QThread::started, [this]{ workerLoop(); });
    m_thread->start();
}

void InferenceEngine::stop() {
    if (!m_running.load()) return;
    m_running.store(false);
    m_cv.notify_all();

    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait(3000);
    }
}

// ─── enqueue ──────────────────────────────────────────────────────────────────
void InferenceEngine::enqueue(FramePtr frame) {
    {
        std::lock_guard<std::mutex> lk(m_mutex);

        // Back-pressure: drop oldest frame if queue is full
        if (static_cast<int>(m_queue.size()) >= m_maxQueue) {
            m_queue.pop();
            qWarning() << "[InferenceEngine] queue full — dropped oldest frame";
        }

        // std::move transfers ownership into the queue — no deep copy
        m_queue.push(std::move(frame));
    }
    m_cv.notify_one();
}

// ─── workerLoop ───────────────────────────────────────────────────────────────
void InferenceEngine::workerLoop() {
    while (m_running.load()) {
        FramePtr frame;

        // Wait for work
        {
            std::unique_lock<std::mutex> lk(m_mutex);
            m_cv.wait(lk, [this]{
                return !m_queue.empty() || !m_running.load();
            });

            if (!m_running.load() && m_queue.empty()) break;

            // std::move: transfer ownership out of the queue — zero copy
            frame = std::move(m_queue.front());
            m_queue.pop();
        }

        if (!frame || !frame->mat) continue;

        // ── Inference ────────────────────────────────────────────────────────
        try {
            auto t0 = std::chrono::steady_clock::now();

            frame->results = m_detector->detect(*frame->mat);
            frame->inferenceComplete = true;

            auto t1 = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            Q_UNUSED(ms);

        } catch (const std::exception& ex) {
            emit inferenceError(QString::fromStdString(ex.what()));
            continue;
        }

        // Emit result back to GUI thread via queued connection
        emit frameReady(std::move(frame));
    }
}
