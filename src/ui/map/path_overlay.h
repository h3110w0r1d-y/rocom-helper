#pragma once

#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QString>

namespace app {

struct ParsedSvgPath {
    QPainterPath path;
    QList<QList<QPointF>> polylines;
};

struct PathRotationSuggestion {
    double rotationDelta = 0.0;
    double desiredRotation = 0.0;
    double pathDistance = 0.0;
    double lookaheadDistance = 0.0;
};

ParsedSvgPath parseSvgPath(const QString &pathData, bool *ok = nullptr);
bool pathRotationSuggestion(
    const QList<QList<QPointF>> &polylines,
    const QPointF &playerPos,
    double playerRotation,
    PathRotationSuggestion *suggestion);

} // namespace app
