#include "egg_time_window.h"

#include "ui/window_flags.h"

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QDateTime>
#include <QHeaderView>
#include <QHideEvent>
#include <QJsonArray>
#include <QLabel>
#include <QShowEvent>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace app {
namespace {

QString decodeBase64Name(const QString &rawName)
{
    if (rawName.isEmpty()) {
        return rawName;
    }
    const QByteArray decoded = QByteArray::fromBase64(rawName.toUtf8());
    if (!decoded.isEmpty()) {
        return QString::fromUtf8(decoded);
    }
    return rawName;
}

QString formatPredictedEggTime(qint64 timestampSec)
{
    if (timestampSec <= 0) {
        return QString();
    }
    return QDateTime::fromSecsSinceEpoch(timestampSec).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

} // namespace

EggTimeWindow::EggTimeWindow(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(0, 2, this))
{
    setWindowTitle(QStringLiteral("产蛋时间"));
    setCloseOnlyWindowControls(this, true);
    resize(420, 360);

    auto *hintLabel = new QLabel(
        QStringLiteral("仅能查看别人的产蛋时间，建议登录小号，打开好友列表，选择要查看的玩家（非好友也可），"
                       "点击家园按钮即可获取，不用访问"),
        this);
    hintLabel->setWordWrap(true);
    hintLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_table->setHorizontalHeaderLabels({
        QStringLiteral("名称"),
        QStringLiteral("预计产蛋时间"),
    });
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->setSortingEnabled(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);
    layout->addWidget(hintLabel);
    layout->addWidget(m_table, 1);
}

void EggTimeWindow::applyPayload(const QJsonObject &payload)
{
    renderTable(payload.value(QStringLiteral("pets")).toArray());
}

void EggTimeWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    emit opcodeConsumerVisibilityChanged();
}

void EggTimeWindow::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit opcodeConsumerVisibilityChanged();
}

void EggTimeWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}

void EggTimeWindow::renderTable(const QJsonArray &pets)
{
    m_table->setSortingEnabled(false);
    m_table->clearContents();
    m_table->setRowCount(0);
    m_table->setRowCount(pets.size());

    for (int row = 0; row < pets.size(); ++row) {
        const QJsonObject pet = pets.at(row).toObject();
        const QString name = decodeBase64Name(pet.value(QStringLiteral("name")).toString());
        const qint64 timestampSec = static_cast<qint64>(pet.value(QStringLiteral("predicted_egg_time")).toDouble());

        auto *nameItem = new QTableWidgetItem(name);
        auto *timeItem = new QTableWidgetItem(formatPredictedEggTime(timestampSec));
        timeItem->setData(Qt::UserRole, timestampSec);
        m_table->setItem(row, 0, nameItem);
        m_table->setItem(row, 1, timeItem);
    }

    m_table->setSortingEnabled(true);
    if (pets.size() > 0) {
        m_table->sortItems(1, Qt::AscendingOrder);
    }
}

} // namespace app
