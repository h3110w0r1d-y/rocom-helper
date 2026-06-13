#include "overlay_control_handle.h"

#include <QMouseEvent>
#include <QPainter>
namespace app {

OverlayControlHandle::OverlayControlHandle(QWidget *hostWindow, const QSize &size, QWidget *parent)
    : QWidget(parent)
    , m_hostWindow(hostWindow)
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(size);
}

void OverlayControlHandle::setAnchorOffset(const QPoint &anchorOffset)
{
    m_anchorOffset = anchorOffset;
}

void OverlayControlHandle::syncToHost()
{
    if (m_hostWindow == nullptr) {
        return;
    }
    move(m_hostWindow->frameGeometry().topLeft() + m_anchorOffset);
}

void OverlayControlHandle::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF bounds = rect().adjusted(1, 1, -1, -1);
    painter.setBrush(QColor(32, 32, 32, 210));
    painter.setPen(QColor(120, 120, 120));
    painter.drawRoundedRect(bounds, 4, 4);

    painter.setPen(QColor(220, 220, 220));
    const QRect dragRect = dragAreaRect();
    painter.drawText(dragRect, Qt::AlignCenter, QStringLiteral("⋮⋮"));

    const QRect exitRect = exitButtonRect();
    painter.setPen(QPen(QColor(200, 80, 80), 1));
    painter.drawLine(exitRect.left() + 4, exitRect.top() + 4, exitRect.right() - 4, exitRect.bottom() - 4);
    painter.drawLine(exitRect.right() - 4, exitRect.top() + 4, exitRect.left() + 4, exitRect.bottom() - 4);
}

void OverlayControlHandle::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }

    if (hitExitButton(event->position().toPoint())) {
        emit exitOverlayRequested();
        return;
    }

    if (dragAreaRect().contains(event->position().toPoint()) && m_hostWindow != nullptr) {
        m_dragging = true;
        m_dragStartGlobal = event->globalPosition().toPoint();
        m_hostStartTopLeft = m_hostWindow->frameGeometry().topLeft();
    }
}

void OverlayControlHandle::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging || m_hostWindow == nullptr) {
        return;
    }

    const QPoint delta = event->globalPosition().toPoint() - m_dragStartGlobal;
    m_hostWindow->move(m_hostStartTopLeft + delta);
    syncToHost();
}

void OverlayControlHandle::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
}

bool OverlayControlHandle::hitExitButton(const QPoint &localPos) const
{
    return exitButtonRect().contains(localPos);
}

QRect OverlayControlHandle::exitButtonRect() const
{
    return QRect(width() - 22, 2, 20, height() - 4);
}

QRect OverlayControlHandle::dragAreaRect() const
{
    return QRect(2, 2, width() - 24, height() - 4);
}

} // namespace app
