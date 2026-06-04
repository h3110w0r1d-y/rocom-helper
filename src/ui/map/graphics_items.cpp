#include "graphics_items.h"

#include <QBrush>
#include <QGuiApplication>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QScreen>
#include <QStyleOptionGraphicsItem>
#include <QVariant>

#include <utility>

namespace app {
namespace {

double devicePixelRatio()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    return screen == nullptr ? 1.0 : qMax(1.0, screen->devicePixelRatio());
}

QPixmap makeFallbackPlayerPixmap(double scale)
{
    const double logicalSize = qMax(1.0, 64.0 * qMax(scale, 0.01));
    const double ratio = devicePixelRatio();
    const int physicalSize = qMax(1, qRound(logicalSize * ratio));
    QPixmap pixmap(physicalSize, physicalSize);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor(QStringLiteral("#ffffff")), 2));
    painter.setBrush(QColor(QStringLiteral("#34a853")));
    const double size = physicalSize / ratio;
    QPolygonF polygon;
    polygon << QPointF(size / 2.0, 3.0)
            << QPointF(5.0, size - 5.0)
            << QPointF(size / 2.0, size - 11.0)
            << QPointF(size - 5.0, size - 5.0);
    painter.drawPolygon(polygon);
    return pixmap;
}

} // namespace

QPixmap makeMarkerPixmap(const MarkerTypeConfig &markerType, double scale)
{
    return makeScaledIconPixmap(QStringLiteral(":/icon/") + markerType.icon, scale);
}

QPixmap makePlayerPixmap(double scale)
{
    QPixmap pixmap = makeScaledIconPixmap(QStringLiteral(":/icon/player.png"), scale);
    return pixmap.isNull() ? makeFallbackPlayerPixmap(scale) : pixmap;
}

QPixmap makeScaledIconPixmap(const QString &resourcePath, double scale)
{
    const QPixmap source(resourcePath);
    if (source.isNull() || source.width() <= 0) {
        return {};
    }

    const double sourceRatio = static_cast<double>(source.height()) / static_cast<double>(source.width());
    const double logicalWidth = source.width() / qMax(1.0, source.devicePixelRatio());
    const double targetWidth = qMax(1.0, logicalWidth * qMax(scale, 0.01));
    const double ratio = devicePixelRatio();
    const int physicalWidth = qMax(1, qRound(targetWidth * ratio));
    const int physicalHeight = qMax(1, qRound(physicalWidth * sourceRatio));

    QPixmap pixmap(physicalWidth, physicalHeight);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(
        QRectF(0, 0, physicalWidth / ratio, physicalHeight / ratio),
        source,
        QRectF(source.rect()));
    return pixmap;
}

DraggableMarkerItem::DraggableMarkerItem(QString markerId, const QPixmap &pixmap, QGraphicsItem *parent)
    : QGraphicsPixmapItem(pixmap, parent)
    , m_markerId(std::move(markerId))
{
    centerOnPixmap();
    setFlag(QGraphicsItem::ItemIgnoresTransformations);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setFlag(QGraphicsItem::ItemIsMovable, false);
    setAcceptedMouseButtons(Qt::NoButton);
    setZValue(40);
}

QString DraggableMarkerItem::markerId() const
{
    return m_markerId;
}

void DraggableMarkerItem::centerOnPixmap()
{
    const QPixmap currentPixmap = pixmap();
    const double ratio = qMax(1.0, currentPixmap.devicePixelRatio());
    setOffset(-currentPixmap.width() / ratio / 2.0, -currentPixmap.height() / ratio / 2.0);
}

QVariant DraggableMarkerItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    const QVariant result = QGraphicsPixmapItem::itemChange(change, value);
    if (change == QGraphicsItem::ItemPositionHasChanged && scene() != nullptr) {
        const QPointF point = pos();
        emit moved(m_markerId, point.x(), point.y());
    }
    return result;
}

void DraggableMarkerItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    QGraphicsPixmapItem::paint(painter, option, widget);
    if (!isSelected()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRectF rect = boundingRect().adjusted(2, 2, -2, -2);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(QColor(QStringLiteral("#ffffff")), 6));
    painter->drawEllipse(rect);
    painter->setPen(QPen(QColor(QStringLiteral("#00a3ff")), 3));
    painter->drawEllipse(rect);
    painter->restore();
}

TrailMaskItem::TrailMaskItem(int width, int height, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_mapWidth(width)
    , m_mapHeight(height)
    , m_color(0xFF, 0x5C, 0x60, 90)
{
    setZValue(30);
}

QRectF TrailMaskItem::boundingRect() const
{
    return QRectF(0, 0, m_mapWidth, m_mapHeight);
}

void TrailMaskItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *)
{
    const QRect exposed = option->exposedRect.intersected(boundingRect()).toAlignedRect();
    if (exposed.isEmpty()) {
        return;
    }

    const int firstTx = qMax(0, exposed.left() / m_tileSize);
    const int lastTx = qMin((m_mapWidth - 1) / m_tileSize, exposed.right() / m_tileSize);
    const int firstTy = qMax(0, exposed.top() / m_tileSize);
    const int lastTy = qMin((m_mapHeight - 1) / m_tileSize, exposed.bottom() / m_tileSize);

    for (int ty = firstTy; ty <= lastTy; ++ty) {
        for (int tx = firstTx; tx <= lastTx; ++tx) {
            auto it = m_tiles.constFind({tx, ty});
            if (it == m_tiles.constEnd()) {
                continue;
            }
            const QRect tileBounds = tileRect(tx, ty);
            const QRect part = exposed.intersected(tileBounds);
            if (part.isEmpty()) {
                continue;
            }

            const QRect localPart = part.translated(-tileBounds.left(), -tileBounds.top());
            const QImage maskPart = it.value().copy(localPart);
            QImage layer(part.size(), QImage::Format_ARGB32_Premultiplied);
            layer.fill(Qt::transparent);

            QPainter layerPainter(&layer);
            layerPainter.fillRect(layer.rect(), m_color);
            layerPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            layerPainter.drawImage(0, 0, maskPart);
            layerPainter.end();

            painter->drawImage(part.topLeft(), layer);
        }
    }
}

void TrailMaskItem::addSegment(const QPointF &start, const QPointF &end, int width)
{
    const int radius = qMax(width, 1);
    const QRectF dirty = QRectF(start, end).normalized().adjusted(-radius, -radius, radius, radius);
    for (const auto &key : tileKeysForRect(dirty)) {
        const QRect bounds = tileRect(key.first, key.second);
        QImage &image = tile(key.first, key.second);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.translate(-bounds.left(), -bounds.top());
        painter.setPen(QPen(QColor(255, 255, 255, 255), width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(start, end);
    }
    update(dirty);
}

void TrailMaskItem::addPoint(const QPointF &point, int width)
{
    const double radius = qMax(width / 2.0, 0.5);
    const QRectF dirty(point.x() - radius, point.y() - radius, radius * 2.0, radius * 2.0);
    for (const auto &key : tileKeysForRect(dirty)) {
        const QRect bounds = tileRect(key.first, key.second);
        QImage &image = tile(key.first, key.second);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.translate(-bounds.left(), -bounds.top());
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 255));
        painter.drawEllipse(point, radius, radius);
    }
    update(dirty);
}

void TrailMaskItem::clear()
{
    m_tiles.clear();
    update();
}

QImage &TrailMaskItem::tile(int tx, int ty)
{
    const QPair<int, int> key{tx, ty};
    auto it = m_tiles.find(key);
    if (it != m_tiles.end()) {
        return it.value();
    }

    const QRect bounds = tileRect(tx, ty);
    QImage image(bounds.width(), bounds.height(), QImage::Format_Alpha8);
    image.fill(0);
    it = m_tiles.insert(key, image);
    return it.value();
}

QRect TrailMaskItem::tileRect(int tx, int ty) const
{
    const int left = tx * m_tileSize;
    const int top = ty * m_tileSize;
    const int width = qMin(m_tileSize, m_mapWidth - left);
    const int height = qMin(m_tileSize, m_mapHeight - top);
    return QRect(left, top, width, height);
}

QList<QPair<int, int>> TrailMaskItem::tileKeysForRect(const QRectF &rect) const
{
    const QRect clipped = rect.intersected(boundingRect()).toAlignedRect();
    if (clipped.isEmpty()) {
        return {};
    }

    const int firstTx = qMax(0, clipped.left() / m_tileSize);
    const int lastTx = qMin((m_mapWidth - 1) / m_tileSize, clipped.right() / m_tileSize);
    const int firstTy = qMax(0, clipped.top() / m_tileSize);
    const int lastTy = qMin((m_mapHeight - 1) / m_tileSize, clipped.bottom() / m_tileSize);

    QList<QPair<int, int>> keys;
    for (int ty = firstTy; ty <= lastTy; ++ty) {
        for (int tx = firstTx; tx <= lastTx; ++tx) {
            keys.append({tx, ty});
        }
    }
    return keys;
}

} // namespace app
