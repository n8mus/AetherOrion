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

    const int row = m_radioList->currentRow();
    if (row < 0 || row >= m_radios.size()) return;
    emit connectRequested(m_radios[row]);
}

void ConnectionPanel::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    m_radioGroup->setVisible(!collapsed);
    m_connectBtn->setVisible(!collapsed);
    m_statusLabel->setVisible(!collapsed);
    m_collapseBtn->setVisible(!collapsed);

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
