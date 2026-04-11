#pragma once

/*
 * StatisticsPanel — live dashboard widget showing detection metrics.
 *
 * Displays:
 *   • Total frames processed / defect frames / total defects detected
 *   • Average inference latency (ms) and throughput (fps)
 *   • Per-defect-type bar chart (Qt Charts)
 *   • Recent 60-frame defect rate trend line
 */

#include "Types.h"

#include <QWidget>
#include <QLabel>
#include <QChartView>
#include <QBarSeries>
#include <QBarSet>
#include <QLineSeries>
#include <QValueAxis>
#include <QBarCategoryAxis>
#include <QQueue>

QT_CHARTS_USE_NAMESPACE

class StatisticsPanel : public QWidget {
    Q_OBJECT
public:
    explicit StatisticsPanel(QWidget* parent = nullptr);

public slots:
    void updateStats(const PipelineStats& stats);
    void onFrameResult(FramePtr frame);

private:
    void buildUI();
    void rebuildBarChart(const std::map<DefectType, int>& counts);
    void updateTrendLine(int defectsThisFrame);

    // Metric labels
    QLabel* m_lblFrames    = nullptr;
    QLabel* m_lblDefects   = nullptr;
    QLabel* m_lblFps       = nullptr;
    QLabel* m_lblLatency   = nullptr;

    // Bar chart — defect type breakdown
    QChartView*      m_barView    = nullptr;
    QBarSeries*      m_barSeries  = nullptr;
    QBarSet*         m_barSet     = nullptr;
    QBarCategoryAxis*m_barAxisX   = nullptr;
    QValueAxis*      m_barAxisY   = nullptr;

    // Trend line — defect count per frame (rolling 60)
    QChartView*  m_trendView   = nullptr;
    QLineSeries* m_trendSeries = nullptr;
    QValueAxis*  m_trendAxisX  = nullptr;
    QValueAxis*  m_trendAxisY  = nullptr;

    QQueue<int> m_trendBuffer;
    static constexpr int TREND_WINDOW = 60;

    PipelineStats m_stats;

    // Rolling average for inference time
    QQueue<double> m_latencyBuffer;
    static constexpr int LATENCY_WINDOW = 20;
};
