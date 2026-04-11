#include "DefectOverlay.h"

#include <QPen>
#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QPainterPath>

// ─── Construction ─────────────────────────────────────────────────────────────
DefectOverlay::DefectOverlay(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
    setZValue(10); // always on top
}

// ─── updateDetections ─────────────────────────────────────────────────────────
void DefectOverlay::updateDetections(const std::vector<DetectionResult>& results) {
    prepareGeometryChange();
    m_items.clear();
    m_bounds = QRectF();

    for (const auto& r : results) {
        RenderItem item;
        item.rect  = QRectF(r.bbox.x, r.bbox.y, r.bbox.width, r.bbox.height);
        item.label = QString("%1 %2%")
                     .arg(defectTypeName(r.type))
                     .arg(static_cast<int>(r.confidence * 100));

        QColor base = colorForType(r.type);
        item.borderColor = base;
        item.fillColor   = base;
        item.fillColor.setAlpha(40);

        for (const auto& pt : r.contour) {
            item.contour.emplace_back(pt.x, pt.y);
        }

        m_bounds = m_bounds.united(item.rect);
        m_items.push_back(std::move(item));
    }

    update();
}

// ─── boundingRect ─────────────────────────────────────────────────────────────
QRectF DefectOverlay::boundingRect() const {
    return m_bounds.adjusted(-2, -20, 2, 2); // headroom for label badge
}

// ─── paint ────────────────────────────────────────────────────────────────────
void DefectOverlay::paint(QPainter* painter,
                          const QStyleOptionGraphicsItem* /*option*/,
                          QWidget* /*widget*/)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    QFont font("Monospace", 9);
    font.setStyleHint(QFont::TypeWriter);
    painter->setFont(font);
    QFontMetrics fm(font);

    for (const auto& item : m_items) {
        // ── Fill ──
        painter->setBrush(QBrush(item.fillColor));
        painter->setPen(Qt::NoPen);
        painter->drawRect(item.rect);

        // ── Border ──
        QPen borderPen(item.borderColor, 1.5);
        painter->setPen(borderPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(item.rect);

        // ── Contour polygon (if available) ──
        if (item.contour.size() >= 3) {
            QPolygonF poly;
            for (const auto& pt : item.contour) poly << pt;
            QPen contourPen(item.borderColor, 1.0, Qt::DashLine);
            painter->setPen(contourPen);
            painter->drawPolygon(poly);
        }

        // ── Label badge ──
        QRectF textBound = fm.boundingRect(item.label);
        QRectF badge(item.rect.left(),
                     item.rect.top() - textBound.height() - 4,
                     textBound.width() + 8,
                     textBound.height() + 4);

        painter->setBrush(item.borderColor);
        painter->setPen(Qt::NoPen);
        painter->drawRect(badge);

        painter->setPen(Qt::white);
        painter->drawText(badge, Qt::AlignCenter, item.label);
    }

    painter->restore();
}

// ─── colorForType ─────────────────────────────────────────────────────────────
QColor DefectOverlay::colorForType(DefectType t) {
    switch (t) {
    case DefectType::Scratch:  return QColor(255, 80,  80);  // red
    case DefectType::Crack:    return QColor(255, 165, 0);   // orange
    case DefectType::Dent:     return QColor(80,  160, 255); // blue
    case DefectType::Stain:    return QColor(180, 100, 220); // purple
    case DefectType::Missing:  return QColor(50,  200, 130); // green
    default:                   return QColor(200, 200, 200); // gray
    }
}
