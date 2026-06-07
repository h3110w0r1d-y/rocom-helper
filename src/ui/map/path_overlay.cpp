#include "path_overlay.h"

#include <QRegularExpression>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <optional>

namespace app {
namespace {

constexpr int CurveSampleSteps = 16;
constexpr double PathLookaheadDistance = 50.0;
constexpr double MinSegmentLength = 1.0;
constexpr double Pi = 3.14159265358979323846;

struct ClosestPathPoint {
    double distance = 0.0;
    QList<QPointF> polyline;
    int segmentIndex = 0;
    double segmentRatio = 0.0;
};

double pointDistance(const QPointF &start, const QPointF &end)
{
    return std::hypot(end.x() - start.x(), end.y() - start.y());
}

bool isCommand(const QString &token)
{
    return token.size() == 1 && token.at(0).isLetter();
}

QPointF quadraticPoint(const QPointF &start, const QPointF &control, const QPointF &end, double t)
{
    const double inv = 1.0 - t;
    return start * (inv * inv) + control * (2.0 * inv * t) + end * (t * t);
}

QPointF cubicPoint(const QPointF &start, const QPointF &c1, const QPointF &c2, const QPointF &end, double t)
{
    const double inv = 1.0 - t;
    return start * (inv * inv * inv)
        + c1 * (3.0 * inv * inv * t)
        + c2 * (3.0 * inv * t * t)
        + end * (t * t * t);
}

double vectorAngle(double ux, double uy, double vx, double vy)
{
    const double length = std::hypot(ux, uy) * std::hypot(vx, vy);
    if (length <= MinSegmentLength) {
        return 0.0;
    }

    const double value = std::clamp((ux * vx + uy * vy) / length, -1.0, 1.0);
    const double sign = ux * vy - uy * vx < 0.0 ? -1.0 : 1.0;
    return sign * std::acos(value);
}

QList<QPointF> sampleArc(
    const QPointF &startPoint,
    double rx,
    double ry,
    double xAxisRotation,
    double largeArcFlag,
    double sweepFlag,
    const QPointF &end)
{
    if (pointDistance(startPoint, end) <= MinSegmentLength) {
        return {};
    }

    rx = std::abs(rx);
    ry = std::abs(ry);
    if (rx <= MinSegmentLength || ry <= MinSegmentLength) {
        return {end};
    }

    const double phi = qDegreesToRadians(std::fmod(xAxisRotation, 360.0));
    const double cosPhi = std::cos(phi);
    const double sinPhi = std::sin(phi);
    const double dx = (startPoint.x() - end.x()) / 2.0;
    const double dy = (startPoint.y() - end.y()) / 2.0;
    const double x1p = cosPhi * dx + sinPhi * dy;
    const double y1p = -sinPhi * dx + cosPhi * dy;

    double rxSq = rx * rx;
    double rySq = ry * ry;
    const double x1pSq = x1p * x1p;
    const double y1pSq = y1p * y1p;
    const double radiusScale = x1pSq / rxSq + y1pSq / rySq;
    if (radiusScale > 1.0) {
        const double scale = std::sqrt(radiusScale);
        rx *= scale;
        ry *= scale;
        rxSq = rx * rx;
        rySq = ry * ry;
    }

    const double denominator = rxSq * y1pSq + rySq * x1pSq;
    if (denominator <= MinSegmentLength) {
        return {end};
    }

    const double numerator = std::max(0.0, rxSq * rySq - denominator);
    const double sign = (largeArcFlag != 0.0) == (sweepFlag != 0.0) ? -1.0 : 1.0;
    const double coefficient = sign * std::sqrt(numerator / denominator);
    const double cxp = coefficient * (rx * y1p / ry);
    const double cyp = coefficient * (-ry * x1p / rx);

    const double midpointX = (startPoint.x() + end.x()) / 2.0;
    const double midpointY = (startPoint.y() + end.y()) / 2.0;
    const double cx = cosPhi * cxp - sinPhi * cyp + midpointX;
    const double cy = sinPhi * cxp + cosPhi * cyp + midpointY;

    const double ux = (x1p - cxp) / rx;
    const double uy = (y1p - cyp) / ry;
    const double vx = (-x1p - cxp) / rx;
    const double vy = (-y1p - cyp) / ry;
    const double startAngle = vectorAngle(1.0, 0.0, ux, uy);
    double deltaAngle = vectorAngle(ux, uy, vx, vy);
    const bool sweep = sweepFlag != 0.0;
    if (!sweep && deltaAngle > 0.0) {
        deltaAngle -= 2.0 * Pi;
    } else if (sweep && deltaAngle < 0.0) {
        deltaAngle += 2.0 * Pi;
    }

    const int steps = std::max(1, static_cast<int>(std::ceil(CurveSampleSteps * std::abs(deltaAngle) / Pi)));
    QList<QPointF> points;
    for (int step = 1; step <= steps; ++step) {
        const double angle = startAngle + deltaAngle * step / steps;
        const double ellipseX = rx * std::cos(angle);
        const double ellipseY = ry * std::sin(angle);
        const double x = cosPhi * ellipseX - sinPhi * ellipseY + cx;
        const double y = sinPhi * ellipseX + cosPhi * ellipseY + cy;
        points.append(QPointF(x, y));
    }
    return points;
}

std::optional<ClosestPathPoint> closestPathPoint(const QList<QList<QPointF>> &polylines, const QPointF &playerPos)
{
    std::optional<ClosestPathPoint> closest;

    for (const QList<QPointF> &polyline : polylines) {
        for (int index = 0; index < polyline.size() - 1; ++index) {
            const QPointF start = polyline.at(index);
            const QPointF end = polyline.at(index + 1);
            const QPointF segment = end - start;
            const double lengthSq = segment.x() * segment.x() + segment.y() * segment.y();
            if (lengthSq <= MinSegmentLength) {
                continue;
            }

            const QPointF toPlayer = playerPos - start;
            const double ratio = std::clamp(
                (toPlayer.x() * segment.x() + toPlayer.y() * segment.y()) / lengthSq,
                0.0,
                1.0);
            const QPointF projected = start + segment * ratio;
            const double distance = pointDistance(playerPos, projected);
            if (!closest.has_value() || distance < closest->distance) {
                closest = ClosestPathPoint{distance, polyline, index, ratio};
            }
        }
    }

    return closest;
}

std::optional<QPointF> pathLookaheadPoint(const ClosestPathPoint &closest, double lookahead)
{
    const QList<QPointF> &polyline = closest.polyline;
    if (polyline.size() < 2 || closest.segmentIndex < 0 || closest.segmentIndex >= polyline.size() - 1) {
        return std::nullopt;
    }

    double remaining = lookahead + pointDistance(polyline.at(closest.segmentIndex), polyline.at(closest.segmentIndex + 1)) * closest.segmentRatio;
    for (int cursor = closest.segmentIndex; cursor < polyline.size() - 1; ++cursor) {
        const QPointF segmentStart = polyline.at(cursor);
        const QPointF segmentEnd = polyline.at(cursor + 1);
        const double currentLength = pointDistance(segmentStart, segmentEnd);
        if (currentLength <= MinSegmentLength) {
            continue;
        }
        if (remaining <= currentLength) {
            return segmentStart + (segmentEnd - segmentStart) * (remaining / currentLength);
        }
        remaining -= currentLength;
    }

    return polyline.constLast();
}

double normalizeClockwiseDelta(double degrees)
{
    double normalized = std::fmod(degrees + 180.0, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    return normalized - 180.0;
}

} // namespace

ParsedSvgPath parseSvgPath(const QString &pathData, bool *ok)
{
    if (ok != nullptr) {
        *ok = false;
    }

    static const QRegularExpression tokenRe(
        QStringLiteral("[AaCcHhLlMmQqSsTtVvZz]|[-+]?(?:\\d*\\.\\d+|\\d+\\.?)(?:[eE][-+]?\\d+)?"));
    QStringList tokens;
    QRegularExpressionMatchIterator it = tokenRe.globalMatch(pathData);
    while (it.hasNext()) {
        tokens.append(it.next().captured(0));
    }

    ParsedSvgPath parsed;
    QList<QPointF> currentPolyline;
    int index = 0;
    QString command;
    QPointF current(0, 0);
    QPointF start(0, 0);

    auto hasNumber = [&] {
        return index < tokens.size() && !isCommand(tokens.at(index));
    };
    auto readNumber = [&]() -> double {
        if (index >= tokens.size()) {
            return 0.0;
        }
        return tokens.at(index++).toDouble();
    };
    auto readPoint = [&](bool relative) -> QPointF {
        QPointF point(readNumber(), readNumber());
        return relative ? current + point : point;
    };
    auto beginSubpath = [&](const QPointF &point) {
        if (currentPolyline.size() > 1) {
            parsed.polylines.append(currentPolyline);
        }
        currentPolyline = {point};
    };
    auto appendPoint = [&](const QPointF &point) {
        if (currentPolyline.isEmpty()) {
            currentPolyline.append(point);
            return;
        }
        if (pointDistance(currentPolyline.last(), point) > MinSegmentLength) {
            currentPolyline.append(point);
        }
    };
    auto sampleQuadratic = [&](const QPointF &from, const QPointF &control, const QPointF &to) {
        for (int step = 1; step <= CurveSampleSteps; ++step) {
            appendPoint(quadraticPoint(from, control, to, step / static_cast<double>(CurveSampleSteps)));
        }
    };
    auto sampleCubic = [&](const QPointF &from, const QPointF &c1, const QPointF &c2, const QPointF &to) {
        for (int step = 1; step <= CurveSampleSteps; ++step) {
            appendPoint(cubicPoint(from, c1, c2, to, step / static_cast<double>(CurveSampleSteps)));
        }
    };

    while (index < tokens.size()) {
        if (isCommand(tokens.at(index))) {
            command = tokens.at(index++);
        }
        if (command.isEmpty()) {
            break;
        }

        const bool relative = command.at(0).isLower();
        const QChar op = command.at(0).toUpper();
        if (op == QLatin1Char('M')) {
            current = readPoint(relative);
            parsed.path.moveTo(current);
            start = current;
            beginSubpath(current);
            command = relative ? QStringLiteral("l") : QStringLiteral("L");
            while (hasNumber()) {
                current = readPoint(relative);
                parsed.path.lineTo(current);
                appendPoint(current);
            }
        } else if (op == QLatin1Char('L')) {
            while (hasNumber()) {
                current = readPoint(relative);
                parsed.path.lineTo(current);
                appendPoint(current);
            }
        } else if (op == QLatin1Char('H')) {
            while (hasNumber()) {
                const double x = readNumber();
                current = QPointF(relative ? current.x() + x : x, current.y());
                parsed.path.lineTo(current);
                appendPoint(current);
            }
        } else if (op == QLatin1Char('V')) {
            while (hasNumber()) {
                const double y = readNumber();
                current = QPointF(current.x(), relative ? current.y() + y : y);
                parsed.path.lineTo(current);
                appendPoint(current);
            }
        } else if (op == QLatin1Char('C')) {
            while (hasNumber()) {
                const QPointF from = current;
                const QPointF c1 = readPoint(relative);
                const QPointF c2 = readPoint(relative);
                const QPointF to = readPoint(relative);
                parsed.path.cubicTo(c1, c2, to);
                sampleCubic(from, c1, c2, to);
                current = to;
            }
        } else if (op == QLatin1Char('Q')) {
            while (hasNumber()) {
                const QPointF from = current;
                const QPointF control = readPoint(relative);
                const QPointF to = readPoint(relative);
                parsed.path.quadTo(control, to);
                sampleQuadratic(from, control, to);
                current = to;
            }
        } else if (op == QLatin1Char('S')) {
            while (hasNumber()) {
                const QPointF from = current;
                const QPointF c2 = readPoint(relative);
                const QPointF to = readPoint(relative);
                parsed.path.cubicTo(current, c2, to);
                sampleCubic(from, from, c2, to);
                current = to;
            }
        } else if (op == QLatin1Char('T')) {
            while (hasNumber()) {
                const QPointF from = current;
                const QPointF to = readPoint(relative);
                parsed.path.quadTo(current, to);
                sampleQuadratic(from, from, to);
                current = to;
            }
        } else if (op == QLatin1Char('A')) {
            while (hasNumber()) {
                const QPointF from = current;
                const double rx = readNumber();
                const double ry = readNumber();
                const double xAxisRotation = readNumber();
                const double largeArcFlag = readNumber();
                const double sweepFlag = readNumber();
                const QPointF to = readPoint(relative);
                const QList<QPointF> points = sampleArc(from, rx, ry, xAxisRotation, largeArcFlag, sweepFlag, to);
                for (const QPointF &point : points) {
                    parsed.path.lineTo(point);
                    appendPoint(point);
                }
                current = to;
            }
        } else if (op == QLatin1Char('Z')) {
            parsed.path.closeSubpath();
            appendPoint(start);
            if (currentPolyline.size() > 1) {
                parsed.polylines.append(currentPolyline);
            }
            currentPolyline.clear();
            current = start;
            command.clear();
        } else {
            break;
        }
    }

    if (currentPolyline.size() > 1) {
        parsed.polylines.append(currentPolyline);
    }
    if (ok != nullptr) {
        *ok = !parsed.path.isEmpty();
    }
    return parsed;
}

bool pathRotationSuggestion(
    const QList<QList<QPointF>> &polylines,
    const QPointF &playerPos,
    double playerRotation,
    PathRotationSuggestion *suggestion)
{
    if (suggestion == nullptr) {
        return false;
    }

    const std::optional<ClosestPathPoint> closest = closestPathPoint(polylines, playerPos);
    if (!closest.has_value()) {
        return false;
    }

    const double lookaheadDistance = std::max(PathLookaheadDistance, closest->distance / 2.0);
    const std::optional<QPointF> target = pathLookaheadPoint(*closest, lookaheadDistance);
    if (!target.has_value()) {
        return false;
    }

    const double dx = target->x() - playerPos.x();
    const double dy = target->y() - playerPos.y();
    if (std::hypot(dx, dy) <= MinSegmentLength) {
        return false;
    }

    double desiredRotation = std::fmod(qRadiansToDegrees(std::atan2(dy, dx)) + 90.0, 360.0);
    if (desiredRotation < 0.0) {
        desiredRotation += 360.0;
    }

    suggestion->rotationDelta = normalizeClockwiseDelta(desiredRotation - playerRotation);
    suggestion->desiredRotation = desiredRotation;
    suggestion->pathDistance = closest->distance;
    suggestion->lookaheadDistance = lookaheadDistance;
    return true;
}

} // namespace app
