#pragma once

#include "data/data_center.h"
#include "data/pet_display_catalog.h"

#include <QTableWidget>
#include <QWidget>

namespace app {

class OverlayHostController;

class CatchWindow : public QWidget {
    Q_OBJECT

public:
    explicit CatchWindow(DataCenter *dataCenter, QWidget *parent = nullptr);

public slots:
    void applyState(const app::CatchState &state);

signals:
    void opcodeConsumerVisibilityChanged();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

private:
    void renderTable();

    DataCenter *m_dataCenter = nullptr;
    CatchState m_state;
    PetDisplayCatalog m_displayCatalog;
    QTableWidget *m_table = nullptr;
    OverlayHostController *m_overlayHost = nullptr;
};

} // namespace app
