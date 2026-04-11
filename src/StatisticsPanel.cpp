#include "StatisticsPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFrame>
#include <QChart>
#include <QBarCategoryAxis>

QT_CHARTS_USE_NAMESPACE

// ─── Construction ─────────────────────────────────────────────────────────────
StatisticsPanel::StatisticsPanel(QWidget* parent)
    : QWidget(parent)
{
    buildUI();
    setMinimumWidth(260);
    setMaximumWidth(340);
}

// ─── buildUI ─────────────────────────────────────────────────────────────────
void StatisticsPanel::buildUI() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(10);

    // ── Metric cards ─────────────────────────────────────────────────────────
    auto* metricsBox = new QGroupBox("Live Metrics", this);
    auto* grid = new QGridLayout(metricsBox);
    grid->setSpacing(6);

    auto makeLabel = [&](const QString& text, int ptSize = 11) {
        auto* l = new QLabel(text, this);
        QFont f = l->font();
        f.setPointSize(ptSize);
        l->setFont(f);
        return l;
    };

    grid->addWidget(makeLabel("Frames", 9),   0, 0);
    grid->addWidget(makeLabel("Defects", 9),  1, 0);
    grid->addWidget(makeLabel("FPS", 9),      2, 0);
    grid->addWidget(makeLabel("Latency", 9),  3, 0);

    m_lblFrames  = makeLabel("0",       13);
    m_lblDefects = makeLabel("0",       13);
    m_lblFps     = makeLabel("0.0",     13);
    m_lblLatency = makeLabel("-- ms",   13);

    grid->addWidget(m_lblFrames,  0, 1, Qt::AlignRight);
    grid->addWidget(m_lblDefects, 1, 1, Qt::AlignRight);
    grid->addWidget(m_lblFps,     2, 1, Qt::AlignRight);
    grid->addWidget(m_lblLatency, 3, 1, Qt::AlignRight);

    root->addWidget(metricsBox);

    // ── Bar chart ─────────────────────────────────────────────────────────────
    m_barSet    = new QBarSet("Count");
    m_barSeries = new QBarSeries();
    m_barSeries->append(m_barSet);

    auto* barChart = new QChart();
    barChart->addSeries(m_barSeries);
    barChart->setTitle("Defects by Type");
    barChart->setAnimationOptions(QChart::SeriesAnimations);
    barChart->legend()->setVisible(false);
    barChart->setMargins(QMargins(4, 4, 4, 4));

    m_barAxisX = new QBarCategoryAxis();
    m_barAxisX->append({"Scratch","Crack","Dent","Stain","Missing","Unknown"});
    barChart->addAxis(m_barAxisX, Qt::AlignBottom);
    m_barSeries->attachAxis(m_barAxisX);

    m_barAxisY = new QValueAxis();
    m_barAxisY->setMin(0);
    m_barAxisY->setMax(10);
    barChart->addAxis(m_barAxisY, Qt::AlignLeft);
    m_barSeries->attachAxis(m_barAxisY);

    m_barView = new QChartView(barChart, this);
    m_barView->setRenderHint(QPainter::Antialiasing);
    m_barView->setFixedHeight(180);
    root->addWidget(m_barView);

    // ── Trend line ────────────────────────────────────────────────────────────
    m_trendSeries = new QLineSeries();
    m_trendSeries->setName("Defects/frame");

    auto* trendChart = new QChart();
    trendChart->addSeries(m_trendSeries);
    trendChart->setTitle("Defect Trend (last 60 frames)");
    trendChart->setAnimationOptions(QChart::NoAnimation);
    trendChart->legend()->setVisible(false);
    trendChart->setMargins(QMargins(4, 4, 4, 4));

    m_trendAxisX = new QValueAxis();
    m_trendAxisX->setRange(0, TREND_WINDOW);
    m_trendAxisX->setLabelFormat("%d");
    trendChart->addAxis(m_trendAxisX, Qt::AlignBottom);
    m_trendSeries->attachAxis(m_trendAxisX);

    m_trendAxisY = new QValueAxis();
    m_trendAxisY->setRange(0, 10);
    trendChart->addAxis(m_trendAxisY, Qt::AlignLeft);
    m_trendSeries->attachAxis(m_trendAxisY);

    m_trendView = new QChartView(trendChart, this);
    m_trendView->setRenderHint(QPainter::Antialiasing);
    m_trendView->setFixedHeight(160);
    root->addWidget(m_trendView);

    root->addStretch();
}

// ─── onFrameResult ────────────────────────────────────────────────────────────
void StatisticsPanel::onFrameResult(FramePtr frame) {
    if (!frame) return;

    int defectsThisFrame = static_cast<int>(frame->results.size());

    m_stats.totalFrames++;
    if (defectsThisFrame > 0) m_stats.defectFrames++;
    m_stats.totalDefects += defectsThisFrame;

    for (const auto& r : frame->results) {
        m_stats.defectCounts[r.type]++;
    }

    // Latency rolling average (use frame age as proxy if no explicit field)
    auto now = std::chrono::steady_clock::now();
    double ageMs = std::chrono::duration<double, std::milli>(
        now - frame->timestamp).count();

    m_latencyBuffer.enqueue(ageMs);
    if (m_latencyBuffer.size() > LATENCY_WINDOW) m_latencyBuffer.dequeue();

    double sumLat = 0;
    for (double v : m_latencyBuffer) sumLat += v;
    m_stats.avgInferenceMs = sumLat / m_latencyBuffer.size();

    // FPS from inter-frame interval
    static std::chrono::steady_clock::time_point lastFrame;
    auto now2 = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now2 - lastFrame).count();
    lastFrame = now2;
    if (dt > 0) m_stats.fps = 1.0 / dt;

    updateStats(m_stats);
    updateTrendLine(defectsThisFrame);
}

// ─── updateStats ─────────────────────────────────────────────────────────────
void StatisticsPanel::updateStats(const PipelineStats& stats) {
    m_lblFrames->setText(QString::number(stats.totalFrames));
    m_lblDefects->setText(QString::number(stats.totalDefects));
    m_lblFps->setText(QString::number(stats.fps, 'f', 1));
    m_lblLatency->setText(QString::number(stats.avgInferenceMs, 'f', 1) + " ms");

    rebuildBarChart(stats.defectCounts);
}

// ─── rebuildBarChart ─────────────────────────────────────────────────────────
void StatisticsPanel::rebuildBarChart(const std::map<DefectType, int>& counts) {
    auto get = [&](DefectType t) -> qreal {
        auto it = counts.find(t);
        return it != counts.end() ? it->second : 0;
    };

    m_barSet->remove(0, m_barSet->count());
    *m_barSet << get(DefectType::Scratch)
              << get(DefectType::Crack)
              << get(DefectType::Dent)
              << get(DefectType::Stain)
              << get(DefectType::Missing)
              << get(DefectType::Unknown);

    qreal maxVal = 1;
    for (int i = 0; i < m_barSet->count(); ++i)
        maxVal = std::max(maxVal, m_barSet->at(i));
    m_barAxisY->setMax(maxVal * 1.2);
}

// ─── updateTrendLine ──────────────────────────────────────────────────────────
void StatisticsPanel::updateTrendLine(int defectsThisFrame) {
    m_trendBuffer.enqueue(defectsThisFrame);
    if (m_trendBuffer.size() > TREND_WINDOW) m_trendBuffer.dequeue();

    m_trendSeries->clear();
    int maxVal = 1;
    for (int i = 0; i < m_trendBuffer.size(); ++i) {
        int v = m_trendBuffer[i];
        m_trendSeries->append(i, v);
        maxVal = std::max(maxVal, v);
    }
    m_trendAxisY->setMax(maxVal + 1);
}
