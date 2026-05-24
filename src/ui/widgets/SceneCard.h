#pragma once
#include <QFrame>
#include <QLabel>
#include <QEnterEvent>
#include "common/Types.h"

class SceneCard : public QFrame {
    Q_OBJECT
public:
    explicit SceneCard(SceneType type, QWidget *parent = nullptr);

    SceneType sceneType() const { return m_type; }

    void setEnabled(bool enabled);
    bool isCardEnabled() const { return m_enabled; }

signals:
    void clicked(SceneType type);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    SceneType m_type;
    QLabel *m_titleLabel;
    QLabel *m_descLabel;
    QLabel *m_iconLabel;
    QColor m_themeColor;
    QLabel *m_statusLabel;
    bool m_hovered = false;
    bool m_enabled = true;
};
