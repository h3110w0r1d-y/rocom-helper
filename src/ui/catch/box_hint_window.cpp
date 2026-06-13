#include "box_hint_window.h"

#include "data/data_center.h"
#include "ui/overlay/overlay_host_controller.h"
#include "ui/window_flags.h"

#include <QCloseEvent>
#include <QFont>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

namespace app {

BoxHintWindow::BoxHintWindow(DataCenter *dataCenter, QWidget *parent)
    : QWidget(parent)
    , m_kindLabel(new QLabel(QStringLiteral("等待盒子结果"), this))
    , m_attrLabel(new QLabel(QStringLiteral("打掉盒子后会显示当前属性"), this))
    , m_countLabel(new QLabel(QStringLiteral("已打掉: 0"), this))
    , m_resetCountButton(new QPushButton(QStringLiteral("重置统计"), this))
{
    setWindowTitle(QStringLiteral("盒子提示"));

#ifdef Q_OS_WIN
    OverlayHostOptions overlayOptions;
    overlayOptions.title = QStringLiteral("盒子提示");
    overlayOptions.staysOnTop = true;
    overlayOptions.requireStaysOnTopForOverlay = true;
    m_overlayHost = new OverlayHostController(this, overlayOptions, this);
#else
    setCloseOnlyWindowControls(this, true);
#endif

    QFont kindFont = m_kindLabel->font();
    kindFont.setPointSize(kindFont.pointSize() + 12);
    kindFont.setBold(true);
    m_kindLabel->setFont(kindFont);
    m_kindLabel->setAlignment(Qt::AlignCenter);

    m_attrLabel->setAlignment(Qt::AlignCenter);
    m_countLabel->setAlignment(Qt::AlignCenter);

    connect(m_resetCountButton, &QPushButton::clicked, this, &BoxHintWindow::resetCount);

    auto *countRow = new QHBoxLayout;
    countRow->addStretch();
    countRow->addWidget(m_countLabel);
    countRow->addWidget(m_resetCountButton);
    countRow->addStretch();

    auto *body = new QWidget(this);
    auto *bodyLayout = new QVBoxLayout(body);
    bodyLayout->setSizeConstraint(QLayout::SetFixedSize);
    bodyLayout->setContentsMargins(18, 14, 18, 14);
    bodyLayout->setSpacing(6);
    bodyLayout->addWidget(m_kindLabel);
    bodyLayout->addWidget(m_attrLabel);
    bodyLayout->addLayout(countRow);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
#ifdef Q_OS_WIN
    m_overlayHost->install();
    layout->addWidget(m_overlayHost->captionBar());
#endif
    layout->addWidget(body, 0, Qt::AlignTop);

    if (dataCenter != nullptr) {
        connect(dataCenter, &DataCenter::boxHintUpdated, this, &BoxHintWindow::updateHint);
    }

    adjustSize();
}

void BoxHintWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    emit opcodeConsumerVisibilityChanged();
}

void BoxHintWindow::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit opcodeConsumerVisibilityChanged();
}

void BoxHintWindow::closeEvent(QCloseEvent *event)
{
#ifdef Q_OS_WIN
    if (m_overlayHost != nullptr && m_overlayHost->isOverlayEnabled()) {
        m_overlayHost->setOverlayEnabled(false);
    }
#endif
    event->ignore();
    hide();
}

#ifdef Q_OS_WIN
bool BoxHintWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (m_overlayHost != nullptr && m_overlayHost->handleNativeEvent(eventType, message, result)) {
        return true;
    }
    return QWidget::nativeEvent(eventType, message, result);
}
#endif

void BoxHintWindow::updateHint(const QJsonObject &payload)
{
    if (payload.value(QStringLiteral("clear")).toBool(false)) {
        clearHint();
        return;
    }

    const QString kind = payload.value(QStringLiteral("kind")).toString(QStringLiteral("普通"));
    const QString attrName = payload.value(QStringLiteral("attr_name")).toString(QStringLiteral("unknow"));

    m_kindLabel->setText(kind);
    m_attrLabel->setText(QStringLiteral("+%1").arg(attrName));

    ++m_boxBreakCount;
    m_countLabel->setText(QStringLiteral("已打掉: %1").arg(m_boxBreakCount));
}

void BoxHintWindow::clearHint()
{
    m_kindLabel->setText(QStringLiteral("等待盒子结果"));
    m_attrLabel->setText(QStringLiteral("打掉盒子后会显示当前属性"));
}

void BoxHintWindow::resetCount()
{
    m_boxBreakCount = 0;
    m_countLabel->setText(QStringLiteral("已打掉: 0"));
}

} // namespace app
