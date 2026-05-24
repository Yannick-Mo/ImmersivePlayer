#pragma once
#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include "common/Types.h"

class SessionManager;

class HistoryBar : public QWidget {
    Q_OBJECT
public:
    explicit HistoryBar(SessionManager *manager, QWidget *parent = nullptr);

signals:
    void historyItemClicked(const HistoryItem &item);
    void viewAllClicked();

private:
    void refresh();
    QWidget* createHistoryItemWidget(const HistoryItem &item);

    SessionManager *m_manager;
    QHBoxLayout *m_itemsLayout;
};
