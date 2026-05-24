#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QMouseEvent>

class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

    void updateMaximizeButton(bool maximized);

signals:
    void minimizeClicked();
    void maximizeClicked();
    void closeClicked();
    void mediaLibraryClicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QLabel *m_titleLabel;
    QPushButton *m_minBtn;
    QPushButton *m_maxBtn;
    QPushButton *m_closeBtn;

    bool m_dragging = false;
    QPoint m_dragPos;
};
