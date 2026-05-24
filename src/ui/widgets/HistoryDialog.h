#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QPoint>
#include "common/Types.h"

class SessionManager;

class HistoryDialog : public QDialog {
    Q_OBJECT
public:
    explicit HistoryDialog(SessionManager *manager, QWidget *parent = nullptr);

signals:
    void historyItemClicked(const HistoryItem &item);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void refresh();
    QWidget* createHistoryItemWidget(const HistoryItem &item);
    void onItemClicked(const HistoryItem &item);

    SessionManager *m_manager;
    QVBoxLayout *m_listLayout;
    QPoint m_dragPos;
    bool m_dragging = false;
};
