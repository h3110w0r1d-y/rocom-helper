#include "map_viewer.h"

#include "path_overlay.h"

#include <QBrush>
#include <QDebug>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSize>
#include <QTimer>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>

namespace app {
namespace {

constexpr int PathOverlayWidth = 6;
constexpr qint64 PathRotationLogIntervalMs = 250;
const QColor PathOverlayColor(255, 77, 79, 210);

} // namespace

MapViewer::MapViewer(const MapCatalog *catalog, QWidget *parent)
    : QGraphicsView(parent)
    , m_catalog(catalog)
    , m_scene(new QGraphicsScene(this))
    , m_rubberBand(new QRubberBand(QRubberBand::Rectangle, viewport()))
{
    setScene(m_scene);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);

    connect(m_scene, &QGraphicsScene::selectionChanged, this, [this] {
        emit markerSelected(selectedMarkerIds());
    });

    showLoadingScene();
    if (m_catalog != nullptr) {
        const QString firstMapId = m_catalog->firstMapId();
        if (!firstMapId.isEmpty()) {
            QTimer::singleShot(0, this, [this, firstMapId] {
                setMap(firstMapId);
            });
        }
    }
}

void MapViewer::setMap(const QString &mapId)
{
    if (m_catalog == nullptr) {
        showLoadingScene(QStringLiteral("未加载地图目录"));
        return;
    }
    const MapConfig *config = m_catalog->mapById(mapId);
    if (config == nullptr) {
        showLoadingScene(QStringLiteral("未找到地图配置: %1").arg(mapId));
        return;
    }
    if (m_mapLoaded && m_mapConfig != nullptr && m_mapConfig->id == mapId) {
        return;
    }

    m_activeMapId = config->id;
    const MapLayerConfig *topLayer = config->topLayer();
    m_activeLayerId = topLayer != nullptr ? topLayer->id : QString();
    m_mapLoaded = false;
    m_mapConfig = nullptr;
    m_mapRootItem = nullptr;
    m_maskItem = nullptr;
    m_trailItem = nullptr;
    m_pathOverlayItems.clear();
    m_pathOverlayPolylines.clear();
    m_pathRotationLogTimer.invalidate();
    m_layerItems.clear();
    m_markers.clear();
    m_playerItem = nullptr;
    m_hasLastTrailPos = false;

    showLoadingScene();

    QImage fullImage;
    QMap<QString, QList<QPair<MapImageConfig, QImage>>> layerImages;
    if (!loadMapImages(*config, &fullImage, &layerImages)) {
        showLoadingScene(QStringLiteral("地图图片加载失败: %1").arg(config->id));
        return;
    }

    m_scene->clear();
    m_loadingText = nullptr;
    m_mapConfig = config;

    const QColor backgroundColor(config->backgroundColor);
    m_scene->setBackgroundBrush(backgroundColor);

    auto *root = new QGraphicsRectItem(0, 0, config->width(), config->height());
    m_mapRootItem = root;
    root->setPen(QPen(Qt::NoPen));
    root->setBrush(QBrush(backgroundColor));
    root->setZValue(0);
    m_scene->addItem(root);

    auto *mapImageItem = new QGraphicsPixmapItem(QPixmap::fromImage(fullImage), root);
    mapImageItem->setTransformationMode(Qt::SmoothTransformation);
    mapImageItem->setZValue(0);

    auto *maskItem = new QGraphicsRectItem(0, 0, config->width(), config->height(), root);
    m_maskItem = maskItem;
    maskItem->setPen(QPen(Qt::NoPen));
    maskItem->setBrush(QBrush(QColor(0, 0, 0)));
    maskItem->setZValue(10);
    maskItem->setVisible(false);

    for (auto it = layerImages.begin(); it != layerImages.end(); ++it) {
        QList<QGraphicsPixmapItem *> items;
        for (const auto &pair : it.value()) {
            auto *item = new QGraphicsPixmapItem(QPixmap::fromImage(pair.second), root);
            item->setTransformationMode(Qt::SmoothTransformation);
            item->setPos(pair.first.x, pair.first.y);
            item->setZValue(20);
            item->setVisible(false);
            items.append(item);
        }
        m_layerItems.insert(it.key(), items);
    }

    m_trailItem = new TrailMaskItem(config->width(), config->height(), root);
    m_mapLoaded = true;
    renderPathOverlays();
    setActiveLayer(m_activeLayerId);
    m_scene->setSceneRect(root->mapRectToScene(root->boundingRect()));
    fitMapToViewCover();
    updateSceneRect();
}

void MapViewer::setActiveLayer(const QString &layerId)
{
    if (m_mapConfig == nullptr) {
        m_activeLayerId = layerId;
        return;
    }
    const MapLayerConfig *layer = m_mapConfig->layerById(layerId);
    if (layer == nullptr) {
        layer = m_mapConfig->topLayer();
    }
    if (layer == nullptr) {
        return;
    }
    m_activeLayerId = layer->id;

    for (auto it = m_layerItems.begin(); it != m_layerItems.end(); ++it) {
        for (QGraphicsPixmapItem *item : it.value()) {
            item->setVisible(it.key() == m_activeLayerId);
        }
    }

    if (m_maskItem != nullptr) {
        const MapLayerConfig *topLayer = m_mapConfig->topLayer();
        const bool masked = topLayer != nullptr && m_activeLayerId != topLayer->id && layer->order < 0;
        m_maskItem->setOpacity(layer->maskOpacity);
        m_maskItem->setVisible(masked);
    }
}

void MapViewer::setMarkerTypes(const MarkerTypeMap &markerTypes)
{
    m_markerTypes = markerTypes;
    for (DraggableMarkerItem *item : m_markers) {
        const QString markerTypeKey = item->data(0).toString();
        const MarkerTypeConfig markerType = m_markerTypes.value(markerTypeKey, m_markerTypes.value(QString::fromLatin1(DefaultMarkerType)));
        if (!markerType.key.isEmpty()) {
            item->setPixmap(makeMarkerPixmap(markerType));
            item->centerOnPixmap();
        }
    }
}

void MapViewer::setPlayerPosition(const PlayerState &player, bool follow)
{
    if (!m_mapLoaded || m_mapRootItem == nullptr) {
        return;
    }
    if (!player.visible || !player.hasLocation || player.location.mapId != m_activeMapId) {
        if (m_playerItem != nullptr) {
            m_playerItem->setVisible(false);
        }
        return;
    }

    const QPointF playerPos(player.location.mapX, player.location.mapY);
    recordTrailPosition(playerPos);
    logPathRotationSuggestion(playerPos, player.rotation);
    if (player.location.layerId != m_activeLayerId) {
        if (m_playerItem != nullptr) {
            m_playerItem->setVisible(false);
        }
        return;
    }

    if (m_playerItem == nullptr) {
        m_playerItem = new QGraphicsPixmapItem(makePlayerPixmap(), m_mapRootItem);
        centerPixmapItem(m_playerItem);
        m_playerItem->setFlag(QGraphicsItem::ItemIgnoresTransformations);
        m_playerItem->setZValue(50);
    }
    m_playerItem->setVisible(true);
    m_playerItem->setRotation(player.rotation);
    m_playerItem->setPos(playerPos);
    if (follow) {
        centerOnMapPoint(playerPos.x(), playerPos.y());
    }
}

void MapViewer::addOrUpdateMarker(const MapMarker &marker, bool typeVisible)
{
    const MarkerTypeConfig markerType = m_markerTypes.value(
        marker.markerType,
        m_markerTypes.value(QString::fromLatin1(DefaultMarkerType)));
    if (markerType.key.isEmpty() || !m_mapLoaded || m_mapRootItem == nullptr) {
        return;
    }

    DraggableMarkerItem *item = m_markers.value(marker.id, nullptr);
    if (item == nullptr) {
        item = new DraggableMarkerItem(marker.id, makeMarkerPixmap(markerType), m_mapRootItem);
        connect(item, &DraggableMarkerItem::moved, this, [this](const QString &markerId, double x, double y) {
            if (!m_updatingMarkerItem) {
                emit markerMoved(markerId, x, y);
            }
        });
        m_markers.insert(marker.id, item);
    } else {
        item->setPixmap(makeMarkerPixmap(markerType));
        item->centerOnPixmap();
    }

    item->setData(0, marker.markerType);
    item->setData(1, marker.hasLocation ? marker.location.layerId : QString());
    item->setData(2, marker.temporary);
    item->setToolTip(marker.label.isEmpty() ? markerType.name : marker.label);
    if (!marker.hasLocation) {
        item->setVisible(false);
        return;
    }

    m_updatingMarkerItem = true;
    item->setPos(marker.location.mapX, marker.location.mapY);
    m_updatingMarkerItem = false;
    item->setVisible(
        marker.visible
        && typeVisible
        && marker.location.mapId == m_activeMapId
        && marker.location.layerId == m_activeLayerId);
    applyEditFlags(item);
}

void MapViewer::removeMarker(const QString &markerId)
{
    DraggableMarkerItem *item = m_markers.take(markerId);
    if (item != nullptr) {
        m_scene->removeItem(item);
        delete item;
    }
}

void MapViewer::setMarkerVisible(const QString &markerId, bool visible)
{
    if (DraggableMarkerItem *item = m_markers.value(markerId, nullptr)) {
        item->setVisible(visible);
    }
}

void MapViewer::setEditEnabled(bool enabled)
{
    m_editEnabled = enabled;
    if (!enabled) {
        m_manualAddEnabled = false;
    }
    for (DraggableMarkerItem *item : m_markers) {
        applyEditFlags(item);
    }
}

void MapViewer::setManualAddEnabled(bool enabled)
{
    m_manualAddEnabled = enabled && m_editEnabled;
}

void MapViewer::setTrailRecordingEnabled(bool enabled)
{
    m_trailRecordingEnabled = enabled;
    if (!enabled) {
        m_hasLastTrailPos = false;
    }
}

void MapViewer::setTrailWidth(int width)
{
    m_trailWidth = qMax(1, width);
}

void MapViewer::clearTrail()
{
    if (m_trailItem != nullptr) {
        m_trailItem->clear();
    }
    m_hasLastTrailPos = false;
}

int MapViewer::setPathOverlays(const QStringList &pathData)
{
    m_pathOverlayData.clear();
    for (const QString &path : pathData) {
        if (!path.trimmed().isEmpty()) {
            m_pathOverlayData.append(path);
        }
    }
    renderPathOverlays();
    return m_pathOverlayItems.size();
}

void MapViewer::clearPathOverlays()
{
    m_pathOverlayData.clear();
    m_pathOverlayPolylines.clear();
    m_pathRotationLogTimer.invalidate();
    clearPathOverlayItems();
}

void MapViewer::centerOnMapPoint(double x, double y)
{
    if (m_mapRootItem != nullptr) {
        centerOn(m_mapRootItem->mapToScene(QPointF(x, y)));
    }
}

void MapViewer::setSelectedMarkers(const QSet<QString> &markerIds)
{
    QSignalBlocker blocker(m_scene);
    m_scene->clearSelection();
    for (const QString &markerId : markerIds) {
        DraggableMarkerItem *item = m_markers.value(markerId, nullptr);
        if (item != nullptr && item->isVisible()) {
            item->setSelected(true);
        }
    }
}

QSet<QString> MapViewer::selectedMarkerIds() const
{
    QSet<QString> ids;
    for (QGraphicsItem *item : m_scene->selectedItems()) {
        auto *markerItem = dynamic_cast<DraggableMarkerItem *>(item);
        if (markerItem != nullptr && markerItem->isVisible()) {
            ids.insert(markerItem->markerId());
        }
    }
    return ids;
}

bool MapViewer::exportFullImage(const QString &filePath)
{
    if (filePath.trimmed().isEmpty() || !m_mapLoaded || m_mapConfig == nullptr || m_mapRootItem == nullptr) {
        return false;
    }

    const int width = m_mapConfig->width();
    const int height = m_mapConfig->height();
    if (width <= 0 || height <= 0) {
        return false;
    }

    QImage image(QSize(width, height), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHints(renderHints());
    const QRectF targetRect(0, 0, width, height);
    const QRectF sourceRect = m_mapRootItem->mapRectToScene(m_mapRootItem->boundingRect());
    m_scene->render(&painter, targetRect, sourceRect, Qt::IgnoreAspectRatio);
    painter.end();

    return image.save(filePath);
}

const QMap<QString, DraggableMarkerItem *> &MapViewer::markers() const
{
    return m_markers;
}

void MapViewer::wheelEvent(QWheelEvent *event)
{
    if (!m_mapLoaded) {
        event->accept();
        return;
    }
    const QPoint viewPos = event->position().toPoint();
    const QPointF scenePosBefore = mapToScene(viewPos);
    const double currentZoom = transform().m11();
    if (currentZoom <= 0.0) {
        event->accept();
        return;
    }
    double factor = std::pow(1.001, event->angleDelta().y());
    const double minZoom = minimumZoom();
    const double maxZoom = qMax(m_maxZoom, minZoom);
    const double newZoom = currentZoom * factor;
    if (newZoom < minZoom) {
        factor = minZoom / currentZoom;
    } else if (newZoom > maxZoom) {
        factor = maxZoom / currentZoom;
    }
    scale(factor, factor);
    updateSceneRect();
    const QPointF scenePosAfter = mapToScene(viewPos);
    const QPointF anchorDelta = scenePosAfter - scenePosBefore;
    translate(anchorDelta.x(), anchorDelta.y());
    event->accept();
}

void MapViewer::mousePressEvent(QMouseEvent *event)
{
    if (!m_mapLoaded) {
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        m_hasPress = true;
        m_pressViewPos = event->pos();
        m_pressMapPos = mapPosFromViewPos(event->pos());
        m_pressMarkerId = markerIdAt(event->pos());
        m_lastPanPos = event->pos();
        m_pressDragged = false;
        m_pressSelectedMarkerIds = selectedMarkerIds();

        if (shiftSelectEnabled(event)) {
            startRubberBand(event->pos());
            event->accept();
            return;
        }
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void MapViewer::mouseMoveEvent(QMouseEvent *event)
{
    if (m_hasPress && (event->pos() - m_pressViewPos).manhattanLength() > 5) {
        m_pressDragged = true;
    }
    if (m_rubberBandActive) {
        updateRubberBand(event->pos());
        event->accept();
        return;
    }
    if (m_pressDragged && m_hasPress) {
        panViewTo(event->pos());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void MapViewer::mouseReleaseEvent(QMouseEvent *event)
{
    const bool isRightClick = event->button() == Qt::RightButton;
    if (m_rubberBandActive) {
        finishRubberBand(event->pos());
        resetPressState();
        event->accept();
        return;
    }
    if (!m_hasPress) {
        event->accept();
        return;
    }

    const bool dragged = m_pressDragged || (event->pos() - m_pressViewPos).manhattanLength() > 5;
    const QString markerId = markerIdAt(event->pos()).isEmpty() ? m_pressMarkerId : markerIdAt(event->pos());
    if (!dragged) {
        if (!markerId.isEmpty()) {
            toggleMarkerSelection(markerId, false, m_pressSelectedMarkerIds);
            if (isRightClick) {
                emit markerJsonRequested(markerId);
            }
        } else if (m_mapRootItem != nullptr && m_mapRootItem->boundingRect().contains(m_pressMapPos)) {
            emit blankMapClicked(m_pressMapPos.x(), m_pressMapPos.y());
            if (m_manualAddEnabled) {
                emit mapClicked(m_pressMapPos.x(), m_pressMapPos.y());
            } else {
                clearMarkerSelection();
            }
        }
    }
    resetPressState();
    event->accept();
}

void MapViewer::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!m_mapLoaded) {
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        const QString markerId = markerIdAt(event->pos());
        if (!markerId.isEmpty()) {
            toggleMarkerSelection(markerId, shiftSelectEnabled(event));
        }
        m_rubberBand->hide();
        resetPressState();
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void MapViewer::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (!m_mapLoaded || m_mapRootItem == nullptr) {
        if (m_loadingText != nullptr) {
            const QRectF rect = m_loadingText->boundingRect();
            const QRectF viewRect = mapToScene(viewport()->rect()).boundingRect();
            m_loadingText->setPos(viewRect.center().x() - rect.width() / 2.0, viewRect.center().y() - rect.height() / 2.0);
        }
        return;
    }
    ensureMinimumZoom();
    updateSceneRect();
}

void MapViewer::showLoadingScene(const QString &message)
{
    m_scene->clear();
    m_scene->setBackgroundBrush(QColor(QString::fromLatin1(DefaultMapBackgroundColor)));
    m_scene->setSceneRect(0, 0, 900, 600);
    m_loadingText = m_scene->addText(message);
    m_loadingText->setDefaultTextColor(QColor(QStringLiteral("#3c4043")));
    m_loadingText->setZValue(100);
    const QRectF rect = m_loadingText->boundingRect();
    m_loadingText->setPos(450 - rect.width() / 2.0, 300 - rect.height() / 2.0);
}

bool MapViewer::loadMapImages(const MapConfig &config, QImage *fullImage, QMap<QString, QList<QPair<MapImageConfig, QImage>>> *layerImages)
{
    if (fullImage == nullptr || layerImages == nullptr) {
        return false;
    }
    *fullImage = readImage(config.resolvePath(QStringLiteral("full.png")), true);
    if (fullImage->isNull()) {
        return false;
    }
    layerImages->clear();
    for (const MapLayerConfig &layer : config.layers) {
        QList<QPair<MapImageConfig, QImage>> images;
        for (const MapImageConfig &imageConfig : layer.images) {
            const QImage image = readImage(config.resolvePath(imageConfig.path), false);
            if (!image.isNull()) {
                images.append({imageConfig, image});
            }
        }
        layerImages->insert(layer.id, images);
    }
    return true;
}

QImage MapViewer::readImage(const QString &path, bool required) const
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull() && required) {
        qWarning("Failed to read map image");
    }
    return image;
}

void MapViewer::renderPathOverlays()
{
    clearPathOverlayItems();
    m_pathOverlayPolylines.clear();
    m_pathRotationLogTimer.invalidate();
    if (!m_mapLoaded || m_mapRootItem == nullptr) {
        return;
    }

    QPen pen(PathOverlayColor, PathOverlayWidth);
    pen.setCosmetic(true);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);

    for (const QString &pathData : m_pathOverlayData) {
        bool ok = false;
        const ParsedSvgPath parsed = parseSvgPath(pathData, &ok);
        if (!ok || parsed.path.isEmpty()) {
            continue;
        }
        for (const QList<QPointF> &polyline : parsed.polylines) {
            m_pathOverlayPolylines.append(polyline);
        }
        auto *item = new QGraphicsPathItem(parsed.path, m_mapRootItem);
        item->setPen(pen);
        item->setBrush(Qt::NoBrush);
        item->setZValue(35);
        m_pathOverlayItems.append(item);
    }
}

void MapViewer::clearPathOverlayItems()
{
    for (QGraphicsPathItem *item : m_pathOverlayItems) {
        if (item != nullptr) {
            m_scene->removeItem(item);
            delete item;
        }
    }
    m_pathOverlayItems.clear();
}

void MapViewer::applyEditFlags(DraggableMarkerItem *item)
{
    if (item == nullptr) {
        return;
    }
    item->setFlag(QGraphicsItem::ItemIsMovable, false);
    item->setAcceptedMouseButtons(Qt::NoButton);
}

void MapViewer::recordTrailPosition(const QPointF &playerPos)
{
    if (!m_trailRecordingEnabled || m_trailItem == nullptr) {
        return;
    }
    if (!m_hasLastTrailPos) {
        m_trailItem->addPoint(playerPos, m_trailWidth);
    } else {
        m_trailItem->addSegment(m_lastTrailPos, playerPos, m_trailWidth);
    }
    m_lastTrailPos = playerPos;
    m_hasLastTrailPos = true;
}

void MapViewer::logPathRotationSuggestion(const QPointF &playerPos, double playerRotation)
{
    if (m_pathOverlayPolylines.isEmpty()) {
        return;
    }
    if (m_pathRotationLogTimer.isValid() && m_pathRotationLogTimer.elapsed() < PathRotationLogIntervalMs) {
        return;
    }

    PathRotationSuggestion suggestion;
    if (!pathRotationSuggestion(m_pathOverlayPolylines, playerPos, playerRotation, &suggestion)) {
        return;
    }

    m_pathRotationLogTimer.restart();
    qInfo().noquote()
        << QStringLiteral("path方向建议:")
        << QStringLiteral("旋转=%1°").arg(suggestion.rotationDelta, 0, 'f', 1)
        << QStringLiteral("当前=%1°").arg(playerRotation, 0, 'f', 1)
        << QStringLiteral("目标=%1°").arg(suggestion.desiredRotation, 0, 'f', 1)
        << QStringLiteral("距路径=%1").arg(suggestion.pathDistance, 0, 'f', 1)
        << QStringLiteral("前瞻=%1").arg(suggestion.lookaheadDistance, 0, 'f', 1);
}

void MapViewer::centerPixmapItem(QGraphicsPixmapItem *item)
{
    if (item == nullptr) {
        return;
    }
    const QPixmap pixmap = item->pixmap();
    const double ratio = qMax(1.0, pixmap.devicePixelRatio());
    item->setOffset(-pixmap.width() / ratio / 2.0, -pixmap.height() / ratio / 2.0);
}

double MapViewer::minimumZoom() const
{
    if (m_mapRootItem == nullptr) {
        return m_minZoom;
    }
    const QRectF mapRect = m_mapRootItem->boundingRect();
    if (mapRect.width() <= 0 || mapRect.height() <= 0 || viewport()->width() <= 0 || viewport()->height() <= 0) {
        return m_minZoom;
    }
    const double coverZoom = qMax(viewport()->width() / mapRect.width(), viewport()->height() / mapRect.height());
    return qMax(m_minZoom, coverZoom);
}

void MapViewer::fitMapToViewCover()
{
    if (m_mapRootItem == nullptr) {
        return;
    }
    resetTransform();
    const double zoom = minimumZoom();
    scale(zoom, zoom);
    centerOn(m_mapRootItem->boundingRect().center());
}

void MapViewer::ensureMinimumZoom()
{
    const double currentZoom = transform().m11();
    const double minZoom = minimumZoom();
    if (currentZoom >= minZoom || currentZoom <= 0.0) {
        return;
    }
    const QPointF viewCenter = mapToScene(viewport()->rect().center());
    const double factor = minZoom / currentZoom;
    scale(factor, factor);
    centerOn(viewCenter);
}

void MapViewer::updateSceneRect()
{
    if (m_mapRootItem != nullptr) {
        m_scene->setSceneRect(m_mapRootItem->mapRectToScene(m_mapRootItem->boundingRect()));
    }
}

QPointF MapViewer::mapPosFromViewPos(const QPoint &viewPos) const
{
    if (m_mapRootItem == nullptr) {
        return {};
    }
    return m_mapRootItem->mapFromScene(mapToScene(viewPos));
}

QString MapViewer::markerIdAt(const QPoint &viewPos) const
{
    for (QGraphicsItem *item : items(viewPos)) {
        auto *markerItem = dynamic_cast<DraggableMarkerItem *>(item);
        if (markerItem != nullptr && markerItem->isVisible()) {
            return markerItem->markerId();
        }
    }
    return {};
}

void MapViewer::toggleMarkerSelection(const QString &markerId, bool additive, QSet<QString> selection)
{
    if (markerId.isEmpty()) {
        return;
    }
    if (selection.isEmpty()) {
        selection = selectedMarkerIds();
    }
    if (additive) {
        if (selection.contains(markerId)) {
            selection.remove(markerId);
        } else {
            selection.insert(markerId);
        }
        setSelectedMarkers(selection);
        emit markerSelected(selectedMarkerIds());
        return;
    }
    if (selection == QSet<QString>{markerId}) {
        clearMarkerSelection();
        return;
    }
    setSelectedMarkers({markerId});
    emit markerSelected(selectedMarkerIds());
}

void MapViewer::clearMarkerSelection()
{
    setSelectedMarkers({});
    emit markerSelected({});
}

bool MapViewer::shiftSelectEnabled(QMouseEvent *event) const
{
    return event->button() == Qt::LeftButton && event->modifiers().testFlag(Qt::ShiftModifier);
}

void MapViewer::startRubberBand(const QPoint &origin)
{
    m_rubberOrigin = origin;
    m_rubberBand->setGeometry(QRect(origin, QSize()));
    m_rubberBand->show();
    m_rubberBandActive = true;
}

void MapViewer::updateRubberBand(const QPoint &current)
{
    m_rubberBand->setGeometry(QRect(m_rubberOrigin, current).normalized());
}

void MapViewer::finishRubberBand(const QPoint &current)
{
    const QRect rect = QRect(m_rubberOrigin, current).normalized();
    m_rubberBand->hide();
    m_rubberBandActive = false;
    if (rect.width() < 3 || rect.height() < 3) {
        clearMarkerSelection();
        return;
    }

    QSet<QString> selected;
    for (QGraphicsItem *item : items(rect)) {
        auto *markerItem = dynamic_cast<DraggableMarkerItem *>(item);
        if (markerItem != nullptr && markerItem->isVisible()) {
            selected.insert(markerItem->markerId());
        }
    }
    setSelectedMarkers(selected);
    emit markerSelected(selectedMarkerIds());
}

void MapViewer::resetPressState()
{
    m_hasPress = false;
    m_pressViewPos = {};
    m_pressMapPos = {};
    m_pressMarkerId.clear();
    m_lastPanPos = {};
    m_pressDragged = false;
    m_pressSelectedMarkerIds.clear();
    m_rubberOrigin = {};
    m_rubberBandActive = false;
}

void MapViewer::panViewTo(const QPoint &viewPos)
{
    const QPoint delta = viewPos - m_lastPanPos;
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
    m_lastPanPos = viewPos;
}

} // namespace app
