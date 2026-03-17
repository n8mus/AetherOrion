#include "ConnectionPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QEvent>
#include <QDebug>

namespace AetherSDR {

ConnectionPanel::ConnectionPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(6);

    // Status row
    auto* statusRow = new QHBoxLayout;
    m_indicatorLabel = new QLabel("●", this);
    m_indicatorLabel->setFixedWidth(20);
    m_indicatorLabel->setAlignment(Qt::AlignCenter);
    m_indicatorLabel->setCursor(Qt::PointingHandCursor);
    m_indicatorLabel->installEventFilter(this);

    m_statusLabel = new QLabel("Not connected", this);

    m_collapseBtn = new QPushButton("\u25C0", this);  // ◀ left-pointing triangle
    m_collapseBtn->setFixedSize(16, 16);
    m_collapseBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; "
        "color: #6a8090; font-size: 10px; padding: 0; }"
        "QPushButton:hover { color: #c8d8e8; }");
    m_collapseBtn->setCursor(Qt::PointingHandCursor);

    statusRow->addWidget(m_indicatorLabel);
    statusRow->addWidget(m_statusLabel, 1);
    statusRow->addWidget(m_collapseBtn);
    vbox->addLayout(statusRow);

    // Discovered radios list
    m_radioGroup = new QGroupBox("Discovered Radios", this);
    auto* gbox  = new QVBoxLayout(m_radioGroup);
    m_radioList = new QListWidget(m_radioGroup);
    m_radioList->setSelectionMode(QAbstractItemView::SingleSelection);
    gbox->addWidget(m_radioList);
    vbox->addWidget(m_radioGroup, 1);

    // Connect/disconnect button
    m_connectBtn = new QPushButton("Connect", this);
    m_connectBtn->setEnabled(false);
    vbox->addWidget(m_connectBtn);

    // ── SmartLink section ────────────────────────────────────────────────
    m_smartLinkGroup = new QGroupBox("SmartLink", this);
    auto* slBox = new QVBoxLayout(m_smartLinkGroup);
    slBox->setSpacing(4);

    // Email
    auto* emailRow = new QHBoxLayout;
    emailRow->addWidget(new QLabel("Email:", m_smartLinkGroup));
    m_emailEdit = new QLineEdit(m_smartLinkGroup);
    m_emailEdit->setPlaceholderText("flexradio account email");
    emailRow->addWidget(m_emailEdit, 1);
    slBox->addLayout(emailRow);

    // Password
    auto* passRow = new QHBoxLayout;
    passRow->addWidget(new QLabel("Pass:", m_smartLinkGroup));
    m_passwordEdit = new QLineEdit(m_smartLinkGroup);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("password");
    passRow->addWidget(m_passwordEdit, 1);
    slBox->addLayout(passRow);

    // Login button
    m_loginBtn = new QPushButton("Log In", m_smartLinkGroup);
    slBox->addWidget(m_loginBtn);

    // User info (shown after login)
    m_slUserLabel = new QLabel("", m_smartLinkGroup);
    m_slUserLabel->setStyleSheet("QLabel { color: #00b4d8; font-size: 10px; }");
    m_slUserLabel->setVisible(false);
    slBox->addWidget(m_slUserLabel);

    // WAN radio list
    m_wanRadioList = new QListWidget(m_smartLinkGroup);
    m_wanRadioList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_wanRadioList->setVisible(false);
    slBox->addWidget(m_wanRadioList, 1);

    vbox->addWidget(m_smartLinkGroup);

    // Login button click
    connect(m_loginBtn, &QPushButton::clicked, this, [this] {
        const QString email = m_emailEdit->text().trimmed();
        const QString pass  = m_passwordEdit->text();
        if (email.isEmpty() || pass.isEmpty()) return;
        m_loginBtn->setEnabled(false);
        m_loginBtn->setText("Logging in...");
        emit smartLinkLoginRequested(email, pass);
    });

    // WAN radio list selection enables Connect button
    connect(m_wanRadioList, &QListWidget::itemSelectionChanged, this, [this] {
        if (!m_connected && m_wanRadioList->currentItem())
            m_connectBtn->setEnabled(true);
    });

    // Stretch at the bottom keeps the indicator at the top when collapsed
    vbox->addStretch();

    // All widgets now exist — safe to call setConnected for initial state
    setConnected(false);

    connect(m_radioList, &QListWidget::itemSelectionChanged,
            this, &ConnectionPanel::onListSelectionChanged);
    connect(m_connectBtn, &QPushButton::clicked,
            this, &ConnectionPanel::onConnectClicked);
    connect(m_collapseBtn, &QPushButton::clicked,
            this, [this]{ setCollapsed(true); });
}

void ConnectionPanel::setConnected(bool connected)
{
    m_connected = connected;
    m_indicatorLabel->setStyleSheet(
        connected ? "color: #00e5ff; font-size: 18px;"
                  : "color: #404040; font-size: 18px;");
    m_connectBtn->setText(connected ? "Disconnect" : "Connect");
    m_connectBtn->setEnabled(connected || m_radioList->currentItem() != nullptr);
}

void ConnectionPanel::setStatusText(const QString& text)
{
    m_statusLabel->setText(text);
}

// ─── Radio list management ────────────────────────────────────────────────────

void ConnectionPanel::onRadioDiscovered(const RadioInfo& radio)
{
    m_radios.append(radio);
    m_radioList->addItem(radio.displayName());
}

void ConnectionPanel::onRadioUpdated(const RadioInfo& radio)
{
    for (int i = 0; i < m_radios.size(); ++i) {
        if (m_radios[i].serial == radio.serial) {
            m_radios[i] = radio;
            m_radioList->item(i)->setText(radio.displayName());
            return;
        }
    }
}

void ConnectionPanel::onRadioLost(const QString& serial)
{
    for (int i = 0; i < m_radios.size(); ++i) {
        if (m_radios[i].serial == serial) {
            delete m_radioList->takeItem(i);
            m_radios.removeAt(i);
            return;
        }
    }
}

void ConnectionPanel::onListSelectionChanged()
{
    m_connectBtn->setEnabled(!m_connected && m_radioList->currentItem() != nullptr);
}

void ConnectionPanel::onConnectClicked()
{
    if (m_connected) {
        emit disconnectRequested();
        return;
    }

    // Check WAN radio list first (if a WAN radio is selected)
    const int wanRow = m_wanRadioList->currentRow();
    if (wanRow >= 0 && wanRow < m_wanRadios.size() &&
        m_wanRadioList->currentItem() && m_wanRadioList->currentItem()->isSelected()) {
        emit wanConnectRequested(m_wanRadios[wanRow]);
        return;
    }

    // Fall back to LAN radio
    const int row = m_radioList->currentRow();
    if (row < 0 || row >= m_radios.size()) return;
    emit connectRequested(m_radios[row]);
}

void ConnectionPanel::setSmartLinkClient(SmartLinkClient* client)
{
    m_smartLink = client;
    if (!client) return;

    connect(client, &SmartLinkClient::authenticated, this, [this] {
        m_loginBtn->setText("Log Out");
        m_loginBtn->setEnabled(true);
        m_emailEdit->setVisible(false);
        m_passwordEdit->setVisible(false);
        m_slUserLabel->setText(QString("%1 %2 (%3)")
            .arg(m_smartLink->firstName(), m_smartLink->lastName(),
                 m_smartLink->callsign()));
        m_slUserLabel->setVisible(true);
        m_wanRadioList->setVisible(true);

        // Reconnect login button for logout
        disconnect(m_loginBtn, &QPushButton::clicked, nullptr, nullptr);
        connect(m_loginBtn, &QPushButton::clicked, this, [this] {
            m_smartLink->logout();
            m_loginBtn->setText("Log In");
            m_emailEdit->setVisible(true);
            m_passwordEdit->setVisible(true);
            m_slUserLabel->setVisible(false);
            m_wanRadioList->setVisible(false);
            m_wanRadioList->clear();
            m_wanRadios.clear();

            // Reconnect for login
            disconnect(m_loginBtn, &QPushButton::clicked, nullptr, nullptr);
            connect(m_loginBtn, &QPushButton::clicked, this, [this] {
                const QString email = m_emailEdit->text().trimmed();
                const QString pass  = m_passwordEdit->text();
                if (email.isEmpty() || pass.isEmpty()) return;
                m_loginBtn->setEnabled(false);
                m_loginBtn->setText("Logging in...");
                emit smartLinkLoginRequested(email, pass);
            });
        });
    });

    connect(client, &SmartLinkClient::authFailed, this, [this](const QString& err) {
        m_loginBtn->setText("Log In");
        m_loginBtn->setEnabled(true);
        m_slUserLabel->setText("Login failed: " + err);
        m_slUserLabel->setStyleSheet("QLabel { color: #ff4444; font-size: 10px; }");
        m_slUserLabel->setVisible(true);
    });

    connect(client, &SmartLinkClient::radioListReceived, this,
            [this](const QList<WanRadioInfo>& radios) {
        m_wanRadios = radios;
        m_wanRadioList->clear();
        for (const auto& r : radios) {
            QString display = QString("%1  %2  %3\n%4")
                .arg(r.model, r.nickname, r.callsign, r.status);
            m_wanRadioList->addItem(display);
        }
    });
}

void ConnectionPanel::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    m_radioGroup->setVisible(!collapsed);
    m_connectBtn->setVisible(!collapsed);
    m_statusLabel->setVisible(!collapsed);
    m_collapseBtn->setVisible(!collapsed);
    m_smartLinkGroup->setVisible(!collapsed);

    if (collapsed) {
        m_expandedWidth = width();
        setMinimumWidth(28);
        setMaximumWidth(28);
    } else {
        setMinimumWidth(m_expandedWidth);
        setMaximumWidth(m_expandedWidth);
    }

    emit collapsedChanged(collapsed);
}

bool ConnectionPanel::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == m_indicatorLabel && event->type() == QEvent::MouseButtonPress) {
        if (m_collapsed)
            setCollapsed(false);
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace AetherSDR
