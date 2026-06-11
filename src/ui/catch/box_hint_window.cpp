#include "box_hint_window.h"

#include "data/data_center.h"
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
    setCloseOnlyWindowControls(this, true);

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
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(6);
    layout->addWidget(m_kindLabel);
    layout->addWidget(m_attrLabel);
    layout->addLayout(countRow);

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
    event->ignore();
    hide();
}

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
