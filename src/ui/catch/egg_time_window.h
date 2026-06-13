#pragma once

#include <QJsonObject>
#include <QWidget>

class QTableWidget;

namespace app {

class EggTimeWindow : public QWidget {
    Q_OBJECT

public:
    explicit EggTimeWindow(QWidget *parent = nullptr);

public slots:
    void applyPayload(const QJsonObject &payload);

signals:
    void opcodeConsumerVisibilityChanged();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void renderTable(const QJsonArray &pets);

    QTableWidget *m_table = nullptr;
};

} // namespace app
