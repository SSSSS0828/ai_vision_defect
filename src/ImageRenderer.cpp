#include <opencv2/imgproc.hpp>
#include "ImageRenderer.h"
#include "DefectOverlay.h"

#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QWheelEvent>
#include <QScrollBar>
#include <QPixmap>
#include <QImage>

// ─── Construction ─────────────────────────────────────────────────────────────
ImageRenderer::ImageRenderer(QWidget* parent)
    : QGraphicsView(parent)
{
    setRenderHint(QPainter::Antialiasing);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setOptimizationFlag(QGraphicsView::DontAdjustForAntialiasing, true);
    setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);
    setBackgroundBrush(QBrush(QColor(30, 30, 30)));

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_imgItem = m_scene->addPixmap(QPixmap());
    m_imgItem->setTransformationMode(Qt::SmoothTransformation);

    m_overlay = new DefectOverlay(m_imgItem);
}

// ─── displayFrame ─────────────────────────────────────────────────────────────
// Zero-copy cv::Mat → QImage → QPixmap path.
//
// Step 1: Wrap cv::Mat data in a QImage WITHOUT copying.
//         QImage format = BGR → we need RGB, so channel swap is required.
//         (cv::cvtColor is the only copy we do here; the subsequent QPixmap
//          conversion in Qt's internals also copies, but that is unavoidable.)
//
// Step 2: Keep m_currentFrame alive to ensure the underlying buffer is not
//         returned to the pool while Qt still references it during painting.
void ImageRenderer::displayFrame(FramePtr frame) {
    if (!frame || !frame->mat) return;

    const cv::Mat& mat = *frame->mat;
    int w = mat.cols, h = mat.rows;

    // Convert BGR → RGB in-place on a temporary view (no pool buffer modified)
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);

    // Point QImage at rgb data — rgb is stack-local, lives for duration of call
    QImage img(rgb.data, w, h,
               static_cast<int>(rgb.step),
               QImage::Format_RGB888);

    // Update scene item — QPixmap takes a deep copy of QImage here
    m_imgItem->setPixmap(QPixmap::fromImage(img));
    m_scene->setSceneRect(0, 0, w, h);

    // After setPixmap the QImage/rgb are no longer needed.
    // Now swap in the new frame — old frame released, buffer returns to pool.
    m_currentFrame = std::move(frame);

    // Draw ROI overlays
    m_overlay->updateDetections(m_currentFrame->results);

    if (m_firstImage) {
        fitInView();
        m_firstImage = false;
    }
}

// ─── Zoom ─────────────────────────────────────────────────────────────────────
void ImageRenderer::zoomIn()  { scale(1.2, 1.2); m_scaleFactor *= 1.2; }
void ImageRenderer::zoomOut() { scale(1.0/1.2, 1.0/1.2); m_scaleFactor /= 1.2; }

void ImageRenderer::fitInView() {
    if (m_imgItem->pixmap().isNull()) return;
    QGraphicsView::fitInView(m_imgItem, Qt::KeepAspectRatio);
    m_scaleFactor = transform().m11();
}

void ImageRenderer::wheelEvent(QWheelEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) zoomIn();
        else                             zoomOut();
        event->accept();
    } else {
        QGraphicsView::wheelEvent(event);
    }
}

void ImageRenderer::resizeEvent(QResizeEvent* event) {
    QGraphicsView::resizeEvent(event);
    emit viewportChanged(mapToScene(viewport()->rect()).boundingRect());
}
