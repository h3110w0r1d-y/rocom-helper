#pragma once

#include "data/data_center.h"

#include <QTableWidget>
#include <QWidget>

namespace app {

class CatchWindow : public QWidget {
    Q_OBJECT

public:
    explicit CatchWindow(DataCenter *dataCenter, QWidget *parent = nullptr);

public slots:
    void applyState(const app::CatchState &state);

private:
    void renderTable();

    DataCenter *m_dataCenter = nullptr;
    CatchState m_state;
    QTableWidget *m_table = nullptr;
};

} // namespace app
