#include "box_hint_window.h"

#include "ui/overlay/overlay_host_controller.h"
#include "ui/window_flags.h"

#include <QCloseEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace app {

BoxHintWindow::BoxHintWindow(QWidget *parent)
    : QWidget(parent)
    , m_kindLabel(new QLabel(QStringLiteral("等待盒子结果"), this))
    , m_attrLabel(new QLabel(QStringLiteral("打掉盒子后会显示当前属性"), this))
    , m_countLabel(new QLabel(QStringLiteral("已打掉: 0"), this))
    , m_resetCountButton(new QPushButton(QStringLiteral("重置统计"), this))
{
    setWindowTitle(QStringLiteral("盒子提示"));
#ifdef Q_OS_WIN
    OverlayHostOptions overlayOptions;
    overlayOptions.title = windowTitle();
    overlayOptions.staysOnTop = true;
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

    auto *layout = new QVBoxLayout(this);
    layout->setSizeConstraint(QLayout::SetFixedSize);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

#ifdef Q_OS_WIN
    m_overlayHost->install();
    layout->addWidget(m_overlayHost->captionBar());
#endif

    auto *contentLayout = new QVBoxLayout;
    contentLayout->setContentsMargins(18, 14, 18, 14);
    contentLayout->setSpacing(6);
    contentLayout->addWidget(m_kindLabel);
    contentLayout->addWidget(m_attrLabel);
    contentLayout->addLayout(countRow);

    layout->addLayout(contentLayout);

    adjustSize();
}

void BoxHintWindow::updateHint(const QJsonObject &payload)
{
    if (payload.contains(QStringLiteral("box_hint_id"))) {
        m_boxHintId = qMax(0, payload.value(QStringLiteral("box_hint_id")).toInt());
        m_countLabel->setText(QStringLiteral("已打掉: %1").arg(m_boxHintId));
    }
    if (payload.value(QStringLiteral("counter_only")).toBool(false)) {
        return;
    }
    if (payload.value(QStringLiteral("clear")).toBool(false)) {
        clearHint();
        return;
    }

    const QString kind = payload.value(QStringLiteral("kind")).toString(QStringLiteral("普通"));
    const QString attrName = payload.value(QStringLiteral("attr_name")).toString(QStringLiteral("unknow"));

    m_kindLabel->setText(kind);
    m_attrLabel->setText(QStringLiteral("+%1").arg(attrName));

    // 服务端会按同一次盒子去重生成 box_hint_id，插件直接展示服务端计数。
    if (!payload.contains(QStringLiteral("box_hint_id"))) {
        ++m_boxHintId;
        m_countLabel->setText(QStringLiteral("已打掉: %1").arg(m_boxHintId));
    }
}

void BoxHintWindow::clearHint()
{
    m_kindLabel->setText(QStringLiteral("等待盒子结果"));
    m_attrLabel->setText(QStringLiteral("打掉盒子后会显示当前属性"));
}

void BoxHintWindow::resetCount()
{
    m_boxHintId = 0;
    m_countLabel->setText(QStringLiteral("已打掉: 0"));
    emit resetRequested();
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

} // namespace app
