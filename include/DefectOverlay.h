#pragma once

/*
 * DefectOverlay — QGraphicsItem layer drawn on top of the image.
 *
 * For each DetectionResult it renders:
 *   • A semi-transparent filled bounding rectangle
 *   • A solid border rectangle
 *   • A label badge showing defect type + confidence score
 *   • (Optional) the fine contour polygon if contour points are available
 *
 * All geometry is in image-pixel coordinates so it scales correctly with zoom.
 */

#include "Types.h"

#include <QGraphicsObject>
#include <QPainter>
#include <QRectF>
#include <vector>

class DefectOverlay : public QGraphicsObject {
    Q_OBJECT
public:
    explicit DefectOverlay(QGraphicsItem* parent = nullptr);

    void updateDetections(const std::vector<DetectionResult>& results);

    // QGraphicsItem interface
    QRectF boundingRect() const override;
    void   paint(QPainter* painter,
                 const QStyleOptionGraphicsItem* option,
                 QWidget* widget) override;

private:
    struct RenderItem {
        QRectF       rect;
        QString      label;
        QColor       fillColor;
        QColor       borderColor;
        std::vector<QPointF> contour;
    };

    static QColor colorForType(DefectType t);

    std::vector<RenderItem> m_items;
    QRectF                  m_bounds;
};
