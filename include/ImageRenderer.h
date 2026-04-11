#pragma once

/*
 * ImageRenderer — Qt Graphics View scene for displaying large industrial images.
 *
 * Zero-copy path from cv::Mat to screen:
 *   QImage can be constructed with a pointer to external (non-owned) data:
 *       QImage(uchar* data, int w, int h, int bytesPerLine, Format)
 *   We exploit this to point the QImage directly at the cv::Mat buffer.
 *   This avoids one deep copy per frame (typically 6–25 MB at 1080p–4K).
 *
 *   IMPORTANT lifetime rule:
 *   The QImage does NOT own the cv::Mat buffer.  We keep the FramePtr alive
 *   in m_currentFrame for the duration of the paint cycle.  When a new frame
 *   arrives we atomically swap m_currentFrame and only then allow the old
 *   FramePtr to be released (which triggers the pool return via custom deleter).
 *
 * Features:
 *   • Smooth zoom (Ctrl+Scroll) and pan (drag)
 *   • Fit-to-view on first image load
 *   • Overlay layer painted by DefectOverlay on top of the image item
 */

#include "Types.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

class DefectOverlay;

class ImageRenderer : public QGraphicsView {
    Q_OBJECT
public:
    explicit ImageRenderer(QWidget* parent = nullptr);

    // Called from GUI thread when InferenceEngine emits frameReady
    void displayFrame(FramePtr frame);

    // Access overlay layer for ROI drawing
    DefectOverlay* overlay() const { return m_overlay; }

    // Zoom API
    void zoomIn();
    void zoomOut();
    void fitInView();

signals:
    void viewportChanged(QRectF visibleRect);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QGraphicsScene*       m_scene    = nullptr;
    QGraphicsPixmapItem*  m_imgItem  = nullptr;
    DefectOverlay*        m_overlay  = nullptr;

    // Keep FramePtr alive while QImage points into its buffer
    FramePtr              m_currentFrame;

    double                m_scaleFactor = 1.0;
    bool                  m_firstImage  = true;
};
