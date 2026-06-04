#include "path_overlay.h"

#include <QRegularExpression>
#include <QtMath>

#include <cmath>

namespace app {
namespace {

constexpr int CurveSampleSteps = 16;
constexpr double MinSegmentLength = 1.0;

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

} // namespace app
