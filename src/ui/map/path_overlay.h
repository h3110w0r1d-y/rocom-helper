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

ParsedSvgPath parseSvgPath(const QString &pathData, bool *ok = nullptr);

} // namespace app
