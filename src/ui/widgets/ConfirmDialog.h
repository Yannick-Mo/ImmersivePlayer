#pragma once
#include <QDialog>
#include <QString>
#include <QPoint>

class QLabel;
class QPushButton;

class ConfirmDialog : public QDialog {
    Q_OBJECT
public:
    static bool confirm(QWidget *parent, const QString &title, const QString &text,
                        const QString &confirmText = QStringLiteral("\xe7\xa1\xae\xe8\xae\xa4"),
                        const QString &cancelText = QStringLiteral("\xe5\x8f\x96\xe6\xb6\x88"),
                        bool destructive = true);
    static void info(QWidget *parent, const QString &title, const QString &text,
                     const QString &okText = QStringLiteral("\xe7\x9f\xa5\xe9\x81\x93\xe4\xba\x86"));

    explicit ConfirmDialog(QWidget *parent = nullptr);

    void setMessage(const QString &title, const QString &text);
    void setConfirmText(const QString &text, bool destructive = true);
    void setCancelText(const QString &text);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    QLabel *m_titleLabel;
    QLabel *m_textLabel;
    QPushButton *m_confirmBtn;
    QPushButton *m_cancelBtn;
    QPoint m_dragPos;
    bool m_dragging = false;
};
