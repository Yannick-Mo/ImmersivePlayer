#pragma once
#include <QWidget>

class VideoProgressBar : public QWidget {
    Q_OBJECT
public:
    explicit VideoProgressBar(QWidget *parent = nullptr);

    void setProgress(double ratio);     // 0.0~1.0
    void setBuffered(double ratio);
    void setPreviewText(const QString &text);

signals:
    void seekRequested(double ratio);
    void previewAt(double ratio);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    double m_progress  = 0.0;
    double m_buffered  = 0.0;
    double m_hoverPos  = -1.0;
    bool m_dragging    = false;
    QString m_previewText;
};
