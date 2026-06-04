#pragma once

#include "data/map_types.h"

#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QObject>
#include <QPixmap>

namespace app {

QPixmap makeMarkerPixmap(const MarkerTypeConfig &markerType, double scale = MapIconScale);
QPixmap makePlayerPixmap(double scale = MapIconScale);
QPixmap makeScaledIconPixmap(const QString &resourcePath, double scale = MapIconScale);

class DraggableMarkerItem : public QObject, public QGraphicsPixmapItem {
    Q_OBJECT

public:
    DraggableMarkerItem(QString markerId, const QPixmap &pixmap, QGraphicsItem *parent = nullptr);

    QString markerId() const;
    void centerOnPixmap();

signals:
    void moved(const QString &markerId, double x, double y);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

private:
    QString m_markerId;
};

class TrailMaskItem : public QGraphicsItem {
public:
    TrailMaskItem(int width, int height, QGraphicsItem *parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    void addSegment(const QPointF &start, const QPointF &end, int width);
    void addPoint(const QPointF &point, int width);
    void clear();

private:
    QImage &tile(int tx, int ty);
    QRect tileRect(int tx, int ty) const;
    QList<QPair<int, int>> tileKeysForRect(const QRectF &rect) const;

    int m_mapWidth = 0;
    int m_mapHeight = 0;
    int m_tileSize = 512;
    QMap<QPair<int, int>, QImage> m_tiles;
    QColor m_color;
};

} // namespace app
