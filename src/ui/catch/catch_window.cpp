#include "catch_window.h"

#include "ui/window_flags.h"

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QHeaderView>
#include <QHideEvent>
#include <QShowEvent>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace app {

CatchWindow::CatchWindow(DataCenter *dataCenter, QWidget *parent)
    : QWidget(parent)
    , m_dataCenter(dataCenter)
    , m_displayCatalog(PetDisplayCatalog::load())
    , m_table(new QTableWidget(0, 8, this))
{
    setWindowTitle(QStringLiteral("捕捉日志"));
    setCloseOnlyWindowControls(this, true);
    resize(640, 420);

    m_table->setHorizontalHeaderLabels({
        QStringLiteral("名称"),
        QStringLiteral("性格"),
        QStringLiteral("天分"),
        QStringLiteral("特长"),
        QStringLiteral("奖牌"),
        QStringLiteral("声音"),
        QStringLiteral("体重"),
        QStringLiteral("时间"),
    });
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 8; ++column) {
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_table, 1);

    if (m_dataCenter != nullptr) {
        connect(m_dataCenter, &DataCenter::catchStateChanged, this, &CatchWindow::applyState);
    }
}

void CatchWindow::applyState(const CatchState &state)
{
    m_state = state;
    renderTable();
}

void CatchWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    emit opcodeConsumerVisibilityChanged();
}

void CatchWindow::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    emit opcodeConsumerVisibilityChanged();
}

void CatchWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}

void CatchWindow::renderTable()
{
    m_table->setRowCount(m_state.records.size());
    for (int row = 0; row < m_state.records.size(); ++row) {
        const CatchRecord &record = m_state.records.at(row);
        const QStringList values = {
            record.name,
            m_displayCatalog.natureName(record.nature),
            m_displayCatalog.talentRankName(record.talentRank),
            m_displayCatalog.specialityName(record.specialityId),
            m_displayCatalog.medalName(record.wearMedalConfId),
            m_displayCatalog.voiceText(record.voice),
            m_displayCatalog.weightText(record.baseConfId, record.weight),
            record.caughtAt,
        };
        for (int column = 0; column < values.size(); ++column) {
            m_table->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
    }
}

} // namespace app
