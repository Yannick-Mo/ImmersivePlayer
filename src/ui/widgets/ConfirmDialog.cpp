#include "ConfirmDialog.h"
#include "common/Constants.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMouseEvent>

bool ConfirmDialog::confirm(QWidget *parent, const QString &title, const QString &text,
                            const QString &confirmText, const QString &cancelText, bool destructive) {
    ConfirmDialog dlg(parent);
    dlg.setMessage(title, text);
    dlg.setConfirmText(confirmText, destructive);
    dlg.setCancelText(cancelText);
    dlg.resize(360, dlg.minimumSizeHint().height());
    return dlg.exec() == QDialog::Accepted;
}

ConfirmDialog::ConfirmDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setFixedWidth(360);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto bgStyle = QString("background: %1; border: none;").arg(Constants::ColorBg.name());

    // Title bar (draggable)
    auto *titleBar = new QWidget(this);
    titleBar->setFixedHeight(44);
    titleBar->setStyleSheet(bgStyle);
    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(20, 0, 16, 0);

    m_titleLabel = new QLabel(titleBar);
    m_titleLabel->setStyleSheet("font-size: 13px; font-weight: 500; color: rgba(255,255,255,0.7); border: none; background: transparent;");

    auto *closeBtn = new QPushButton(QStringLiteral("\xe2\x9c\x95"), titleBar);
    closeBtn->setFixedSize(24, 24);
    closeBtn->setFlat(true);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(
        "QPushButton { font-size: 13px; color: rgba(255,255,255,0.15); background: transparent; "
        "border: none; border-radius: 6px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.5); background: rgba(255,255,255,0.06); }");

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(closeBtn);

    titleBar->installEventFilter(this);
    mainLayout->addWidget(titleBar);

    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    // Content
    auto *content = new QWidget(this);
    content->setStyleSheet(bgStyle);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(24, 20, 24, 20);
    contentLayout->setSpacing(0);

    m_textLabel = new QLabel(content);
    m_textLabel->setWordWrap(true);
    m_textLabel->setStyleSheet("font-size: 13px; color: rgba(255,255,255,0.55); border: none; background: transparent;");

    contentLayout->addWidget(m_textLabel);
    contentLayout->addStretch();

    mainLayout->addWidget(content, 1);

    // Button bar
    auto *btnBar = new QWidget(this);
    btnBar->setFixedHeight(56);
    btnBar->setStyleSheet(bgStyle);
    auto *btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(16, 10, 16, 10);
    btnLayout->setSpacing(10);

    m_cancelBtn = new QPushButton(btnBar);
    m_cancelBtn->setFixedHeight(32);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setStyleSheet(
        "QPushButton { font-size: 11px; color: rgba(255,255,255,0.3); background: transparent; "
        "border: 1px solid rgba(255,255,255,0.06); border-radius: 6px; padding: 0 20px; }"
        "QPushButton:hover { color: rgba(255,255,255,0.6); border-color: rgba(255,255,255,0.12); }"
        "QPushButton:pressed { background: rgba(255,255,255,0.04); }");

    m_confirmBtn = new QPushButton(btnBar);
    m_confirmBtn->setFixedHeight(32);
    m_confirmBtn->setCursor(Qt::PointingHandCursor);
    m_confirmBtn->setStyleSheet(
        "QPushButton { font-size: 11px; font-weight: 500; color: #ff4444; background: rgba(255,68,68,0.08); "
        "border: 1px solid rgba(255,68,68,0.15); border-radius: 6px; padding: 0 20px; }"
        "QPushButton:hover { color: white; background: rgba(255,68,68,0.2); border-color: rgba(255,68,68,0.3); }"
        "QPushButton:pressed { background: rgba(255,68,68,0.3); }");

    btnLayout->addStretch();
    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_confirmBtn);

    mainLayout->addWidget(btnBar);

    connect(m_confirmBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ConfirmDialog::info(QWidget *parent, const QString &title, const QString &text,
                         const QString &okText) {
    ConfirmDialog dlg(parent);
    dlg.setMessage(title, text);
    dlg.setConfirmText(okText, false);
    dlg.m_cancelBtn->hide();
    dlg.resize(360, dlg.minimumSizeHint().height());
    dlg.exec();
}

void ConfirmDialog::setMessage(const QString &title, const QString &text) {
    m_titleLabel->setText(title);
    m_textLabel->setText(text);
}

void ConfirmDialog::setConfirmText(const QString &text, bool destructive) {
    m_confirmBtn->setText(text);
    if (!destructive) {
        m_confirmBtn->setStyleSheet(
            "QPushButton { font-size: 11px; font-weight: 500; color: #00d4ff; background: rgba(0,212,255,0.08); "
            "border: 1px solid rgba(0,212,255,0.15); border-radius: 6px; padding: 0 20px; }"
            "QPushButton:hover { color: white; background: rgba(0,212,255,0.2); border-color: rgba(0,212,255,0.3); }"
            "QPushButton:pressed { background: rgba(0,212,255,0.3); }");
    }
}

void ConfirmDialog::setCancelText(const QString &text) {
    m_cancelBtn->setText(text);
}

bool ConfirmDialog::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            m_dragPos = me->globalPosition().toPoint() - frameGeometry().topLeft();
            m_dragging = true;
            return true;
        }
    } else if (event->type() == QEvent::MouseMove && m_dragging) {
        auto *me = static_cast<QMouseEvent*>(event);
        move(me->globalPosition().toPoint() - m_dragPos);
        return true;
    } else if (event->type() == QEvent::MouseButtonRelease) {
        m_dragging = false;
        return true;
    }
    return QDialog::eventFilter(obj, event);
}
