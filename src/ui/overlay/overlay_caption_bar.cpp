#include "overlay_caption_bar.h"

#include "overlay_host_controller.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>

namespace app {

OverlayCaptionBar::OverlayCaptionBar(OverlayHostController *controller, QWidget *hostWindow, QWidget *parent)
    : QWidget(parent)
    , m_controller(controller)
    , m_hostWindow(hostWindow)
{
    setFixedHeight(32);
    setAutoFillBackground(true);
    setStyleSheet(QStringLiteral("background-color: palette(window); "
                                 "QPushButton { padding: 0 6px; min-height: 20px; }"));

    auto *titleLabel = new QLabel(this);
    titleLabel->setObjectName(QStringLiteral("overlayCaptionTitle"));
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_overlayButton = new QPushButton(QStringLiteral("悬浮"), this);
    m_overlayButton->setFixedSize(44, 24);
    m_overlayButton->setToolTip(QStringLiteral("进入悬浮穿透模式（需先开启小地图模式并调整好窗口大小）"));
    connect(m_overlayButton, &QPushButton::clicked, this, [this] {
        if (m_controller != nullptr) {
            m_controller->setOverlayEnabled(true);
        }
    });

    m_closeButton = new QPushButton(QStringLiteral("×"), this);
    m_closeButton->setFixedSize(28, 24);
    m_closeButton->setToolTip(QStringLiteral("关闭"));
    connect(m_closeButton, &QPushButton::clicked, m_hostWindow, &QWidget::close);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(titleLabel, 1);
    layout->addWidget(m_overlayButton);
    layout->addWidget(m_closeButton);

    setTitle(m_hostWindow != nullptr ? m_hostWindow->windowTitle() : QString());
}

void OverlayCaptionBar::setTitle(const QString &title)
{
    if (QLabel *titleLabel = findChild<QLabel *>(QStringLiteral("overlayCaptionTitle"))) {
        titleLabel->setText(title);
    }
}

void OverlayCaptionBar::setOverlayButtonEnabled(bool enabled)
{
    if (m_overlayButton != nullptr) {
        m_overlayButton->setEnabled(enabled);
    }
}

void OverlayCaptionBar::setOverlayActive(bool active)
{
    if (m_overlayButton != nullptr) {
        m_overlayButton->setText(active ? QStringLiteral("悬浮●") : QStringLiteral("悬浮"));
    }
}

void OverlayCaptionBar::setOverlayButtonToolTip(const QString &toolTip)
{
    if (m_overlayButton != nullptr) {
        m_overlayButton->setToolTip(toolTip);
    }
}

void OverlayCaptionBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !isDragArea(event->position().toPoint()) || m_hostWindow == nullptr) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_dragging = true;
    m_dragStartGlobal = event->globalPosition().toPoint();
    m_hostStartTopLeft = m_hostWindow->frameGeometry().topLeft();
}

void OverlayCaptionBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging || m_hostWindow == nullptr) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint delta = event->globalPosition().toPoint() - m_dragStartGlobal;
    m_hostWindow->move(m_hostStartTopLeft + delta);
}

void OverlayCaptionBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

void OverlayCaptionBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (isDragArea(event->position().toPoint()) && m_hostWindow != nullptr) {
        if (m_hostWindow->isMaximized()) {
            m_hostWindow->showNormal();
        } else {
            m_hostWindow->showMaximized();
        }
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

bool OverlayCaptionBar::isDragArea(const QPoint &localPos) const
{
    if (m_overlayButton != nullptr && m_overlayButton->geometry().contains(localPos)) {
        return false;
    }
    if (m_closeButton != nullptr && m_closeButton->geometry().contains(localPos)) {
        return false;
    }
    return true;
}

} // namespace app
