#include "box_hint_window.h"

#include "data/data_center.h"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace app {

BoxHintWindow::BoxHintWindow(DataCenter *dataCenter, QWidget *parent)
    : QWidget(parent)
    , m_kindLabel(new QLabel(QStringLiteral("等待盒子结果"), this))
    , m_attrLabel(new QLabel(QStringLiteral("打掉盒子后会显示当前属性"), this))
{
    setWindowTitle(QStringLiteral("盒子提示"));
    setWindowFlag(Qt::WindowStaysOnTopHint, true);
    resize(260, 150);

    QFont kindFont = m_kindLabel->font();
    kindFont.setPointSize(kindFont.pointSize() + 12);
    kindFont.setBold(true);
    m_kindLabel->setFont(kindFont);
    m_kindLabel->setAlignment(Qt::AlignCenter);

    m_attrLabel->setAlignment(Qt::AlignCenter);

    auto *layout = new QVBoxLayout(this);
    layout->addStretch(1);
    layout->addWidget(m_kindLabel);
    layout->addWidget(m_attrLabel);
    layout->addStretch(1);

    if (dataCenter != nullptr) {
        connect(dataCenter, &DataCenter::boxHintUpdated, this, &BoxHintWindow::updateHint);
    }
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
}

void BoxHintWindow::clearHint()
{
    m_kindLabel->setText(QStringLiteral("等待盒子结果"));
    m_attrLabel->setText(QStringLiteral("打掉盒子后会显示当前属性"));
}

} // namespace app
