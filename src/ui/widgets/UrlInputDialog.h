#pragma once
#include <QDialog>
#include <QStringList>

class QComboBox;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class UrlInputDialog : public QDialog {
    Q_OBJECT
public:
    explicit UrlInputDialog(const QStringList &recentUrls, QWidget *parent = nullptr);

    QString url() const;

private:
    void pasteFromClipboard();
    void insertProtocol(const QString &proto);
    void onRecentUrlClicked(QListWidgetItem *item);
    void updateOkButton();

    QComboBox *m_urlCombo;
    QListWidget *m_recentList;
    QPushButton *m_okBtn;
    QPushButton *m_cancelBtn;
};
