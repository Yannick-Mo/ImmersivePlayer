#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPoint>
#include "common/Types.h"

class SessionManager;

class MediaLibraryDialog : public QDialog {
    Q_OBJECT
public:
    explicit MediaLibraryDialog(SessionManager *manager, QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void refresh();
    QWidget* createLibraryItemWidget(const MediaLibraryItem &item, int index);

    SessionManager *m_manager;
    QVBoxLayout *m_listLayout;
    QLineEdit *m_searchInput;
    QPoint m_dragPos;
    bool m_dragging = false;
};
