#pragma once
#include <QFrame>
#include <QPushButton>
#include <QVector>

class SpeedPopup : public QFrame {
    Q_OBJECT
public:
    explicit SpeedPopup(QWidget *parent = nullptr);

    void showAt(const QPoint &globalPos);
    void setCurrentSpeed(double speed);

signals:
    void speedSelected(double speed);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUI();
    void setSelected(int index);

    QVector<QPushButton*> m_buttons;
    int m_currentIndex = -1;
};
