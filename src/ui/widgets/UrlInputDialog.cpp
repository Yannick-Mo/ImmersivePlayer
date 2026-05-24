#include "UrlInputDialog.h"
#include "common/Constants.h"
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QClipboard>
#include <QApplication>
#include <QLineEdit>

UrlInputDialog::UrlInputDialog(const QStringList &recentUrls, QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setFixedSize(480, 380);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Title bar
    auto *titleBar = new QWidget(this);
    titleBar->setFixedHeight(44);
    titleBar->setStyleSheet(QString("background: %1; border: none;").arg(Constants::ColorBg.name()));
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(20, 0, 16, 0);

    auto *titleLabel = new QLabel(QStringLiteral("\u7f51\u7edc\u89c6\u9891\u94fe\u63a5"), titleBar);
    titleLabel->setStyleSheet("font-size: 13px; font-weight: 500; color: rgba(255,255,255,0.7); border: none;");

    auto *closeBtn = new QPushButton(QStringLiteral("\u2715"), titleBar);
    closeBtn->setFixedSize(24, 24);
    closeBtn->setFlat(true);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton { font-size: 13px; color: rgba(255,255,255,0.15); background: transparent; "
        "border: none; border-radius: 6px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.5); background: rgba(255,255,255,0.06); }");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(closeBtn);
    mainLayout->addWidget(titleBar);

    // Content area
    auto *content = new QWidget(this);
    content->setStyleSheet(QString("background: %1; border: none;").arg(Constants::ColorBg.name()));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(20, 16, 20, 12);
    contentLayout->setSpacing(12);

    // URL input row
    auto *inputRow = new QHBoxLayout;
    inputRow->setSpacing(8);

    m_urlCombo = new QComboBox(content);
    m_urlCombo->setEditable(true);
    m_urlCombo->lineEdit()->setPlaceholderText(QStringLiteral("\u8f93\u5165\u7f51\u7edc\u89c6\u9891 URL..."));
    m_urlCombo->setStyleSheet(
        "QComboBox { background: rgba(255,255,255,0.04); border: 1px solid rgba(255,255,255,0.06); "
        "border-radius: 8px; padding: 8px 12px; font-size: 12px; color: rgba(255,255,255,0.7); }"
        "QComboBox:hover { border-color: rgba(255,255,255,0.12); }"
        "QComboBox::drop-down { width: 0; border: none; }");
    connect(m_urlCombo->lineEdit(), &QLineEdit::textChanged, this, &UrlInputDialog::updateOkButton);

    auto *pasteBtn = new QPushButton(QStringLiteral("\U0001F4CB"), content);
    pasteBtn->setFixedSize(32, 32);
    pasteBtn->setToolTip(QStringLiteral("\u7c98\u8d34"));
    pasteBtn->setCursor(Qt::PointingHandCursor);
    pasteBtn->setStyleSheet(
        "QPushButton { font-size: 14px; color: rgba(255,255,255,0.25); background: rgba(255,255,255,0.03); "
        "border: 1px solid rgba(255,255,255,0.06); border-radius: 8px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.5); background: rgba(255,255,255,0.06); }");
    connect(pasteBtn, &QPushButton::clicked, this, &UrlInputDialog::pasteFromClipboard);

    inputRow->addWidget(m_urlCombo, 1);
    inputRow->addWidget(pasteBtn);
    contentLayout->addLayout(inputRow);

    // Protocol buttons
    auto *protoRow = new QHBoxLayout;
    protoRow->setSpacing(6);
    QStringList protocols = {"http://", "https://", "rtmp://", "rtsp://"};
    for (const auto &proto : protocols) {
        auto *btn = new QPushButton(proto, content);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { font-size: 10px; color: rgba(0,212,255,0.5); background: rgba(0,212,255,0.04); "
            "border: 1px solid rgba(0,212,255,0.06); border-radius: 4px; padding: 3px 8px; }"
            "QPushButton:hover { color: #00d4ff; background: rgba(0,212,255,0.08); }");
        connect(btn, &QPushButton::clicked, this, [this, proto]() { insertProtocol(proto); });
        protoRow->addWidget(btn);
    }
    protoRow->addStretch();
    contentLayout->addLayout(protoRow);

    // Recent URLs label
    auto *recentLabel = new QLabel(QStringLiteral("\u6700\u8fd1\u4f7f\u7528"), content);
    recentLabel->setStyleSheet("font-size: 10px; color: rgba(255,255,255,0.15); border: none;");
    contentLayout->addWidget(recentLabel);

    // Recent URLs list
    m_recentList = new QListWidget(content);
    m_recentList->setStyleSheet(
        "QListWidget { background: rgba(255,255,255,0.02); border: 1px solid rgba(255,255,255,0.04); "
        "border-radius: 8px; padding: 4px; }"
        "QListWidget::item { font-size: 11px; color: rgba(255,255,255,0.35); padding: 6px 10px; "
        "border-radius: 4px; }"
        "QListWidget::item:hover { color: rgba(255,255,255,0.6); background: rgba(255,255,255,0.04); }"
        "QScrollBar:vertical { width: 4px; background: transparent; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.08); border-radius: 2px; min-height: 30px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(255,255,255,0.14); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        "QScrollBar:horizontal { height: 4px; background: transparent; }"
        "QScrollBar::handle:horizontal { background: rgba(255,255,255,0.08); border-radius: 2px; min-width: 30px; }"
        "QScrollBar::handle:horizontal:hover { background: rgba(255,255,255,0.14); }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }");
    for (const auto &url : recentUrls) {
        m_recentList->addItem(url);
    }
    connect(m_recentList, &QListWidget::itemClicked, this, &UrlInputDialog::onRecentUrlClicked);
    contentLayout->addWidget(m_recentList, 1);

    mainLayout->addWidget(content, 1);

    // Button bar
    auto *btnBar = new QWidget(this);
    btnBar->setFixedHeight(56);
    btnBar->setStyleSheet(QString("background: %1; border: none;").arg(Constants::ColorBg.name()));
    auto *btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(16, 10, 16, 10);
    btnLayout->setSpacing(10);

    m_cancelBtn = new QPushButton(QStringLiteral("\u53d6\u6d88"), btnBar);
    m_cancelBtn->setFixedHeight(32);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setStyleSheet(
        "QPushButton { font-size: 11px; color: rgba(255,255,255,0.3); background: transparent; "
        "border: 1px solid rgba(255,255,255,0.06); border-radius: 6px; padding: 0 20px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.6); border-color: rgba(255,255,255,0.12); }");

    m_okBtn = new QPushButton(QStringLiteral("\u64ad\u653e"), btnBar);
    m_okBtn->setFixedHeight(32);
    m_okBtn->setEnabled(false);
    m_okBtn->setCursor(Qt::PointingHandCursor);
    m_okBtn->setStyleSheet(
        "QPushButton { font-size: 11px; font-weight: 500; color: #00d4ff; background: rgba(0,212,255,0.08); "
        "border: 1px solid rgba(0,212,255,0.15); border-radius: 6px; padding: 0 20px; }"
        "QPushButton:hover { color: white; background: rgba(0,212,255,0.2); border-color: rgba(0,212,255,0.3); }"
        "QPushButton:disabled { color: rgba(255,255,255,0.12); background: transparent; border-color: rgba(255,255,255,0.04); }");

    btnLayout->addStretch();
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_okBtn);
    mainLayout->addWidget(btnBar);

    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QString UrlInputDialog::url() const {
    return m_urlCombo->currentText().trimmed();
}

void UrlInputDialog::pasteFromClipboard() {
    QString clip = QApplication::clipboard()->text().trimmed();
    if (!clip.isEmpty()) {
        m_urlCombo->setEditText(clip);
    }
}

void UrlInputDialog::insertProtocol(const QString &proto) {
    QString cur = m_urlCombo->currentText();
    if (cur.isEmpty() || !cur.contains("://")) {
        m_urlCombo->setEditText(proto + cur);
    } else {
        m_urlCombo->setEditText(cur);
    }
    m_urlCombo->lineEdit()->setFocus();
}

void UrlInputDialog::onRecentUrlClicked(QListWidgetItem *item) {
    if (item) {
        m_urlCombo->setEditText(item->text());
    }
}

void UrlInputDialog::updateOkButton() {
    QString text = m_urlCombo->currentText().trimmed();
    m_okBtn->setEnabled(!text.isEmpty());
}
