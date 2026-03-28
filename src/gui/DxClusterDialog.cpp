#include "DxClusterDialog.h"
#include "core/DxClusterClient.h"
#include "core/AppSettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTabWidget>
#include <QTableView>
#include <QHeaderView>
#include <QSortFilterProxyModel>

namespace AetherSDR {

// ── SpotTableModel ──────────────────────────────────────────────────────────

QVariant SpotTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_spots.size())
        return {};

    const auto& spot = m_spots[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColTime:    return spot.utcTime.toString("HH:mm");
        case ColFreq:    return QString::number(spot.freqMhz * 1000.0, 'f', 1);
        case ColDxCall:  return spot.dxCall;
        case ColComment: return spot.comment;
        case ColSpotter: return spot.spotterCall;
        case ColBand:    return bandForFreq(spot.freqMhz);
        }
    }
    if (role == Qt::TextAlignmentRole) {
        if (index.column() == ColFreq)
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        if (index.column() == ColTime)
            return QVariant(Qt::AlignCenter);
    }
    if (role == Qt::ForegroundRole) {
        if (index.column() == ColDxCall)
            return QColor(0x00, 0xb4, 0xd8);  // accent
        if (index.column() == ColFreq)
            return QColor(0xe0, 0xd0, 0x60);  // yellow-ish
    }
    // Store freq in UserRole for sorting
    if (role == Qt::UserRole && index.column() == ColFreq)
        return spot.freqMhz;

    return {};
}

QVariant SpotTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColTime:    return "Time";
    case ColFreq:    return "Freq (kHz)";
    case ColDxCall:  return "DX Call";
    case ColComment: return "Comment";
    case ColSpotter: return "Spotter";
    case ColBand:    return "Band";
    }
    return {};
}

void SpotTableModel::addSpot(const DxSpot& spot)
{
    // Add to top of list (newest first)
    beginInsertRows({}, 0, 0);
    m_spots.prepend(spot);
    endInsertRows();

    // Trim excess
    if (m_spots.size() > m_maxSpots) {
        beginRemoveRows({}, m_maxSpots, m_spots.size() - 1);
        m_spots.resize(m_maxSpots);
        endRemoveRows();
    }
}

double SpotTableModel::freqAtRow(int row) const
{
    if (row >= 0 && row < m_spots.size())
        return m_spots[row].freqMhz;
    return 0.0;
}

void SpotTableModel::clear()
{
    beginResetModel();
    m_spots.clear();
    endResetModel();
}

QString SpotTableModel::bandForFreq(double mhz)
{
    if (mhz >= 1.8   && mhz <= 2.0)    return "160m";
    if (mhz >= 3.5   && mhz <= 4.0)    return "80m";
    if (mhz >= 5.0   && mhz <= 5.5)    return "60m";
    if (mhz >= 7.0   && mhz <= 7.3)    return "40m";
    if (mhz >= 10.1  && mhz <= 10.15)  return "30m";
    if (mhz >= 14.0  && mhz <= 14.35)  return "20m";
    if (mhz >= 18.068 && mhz <= 18.168) return "17m";
    if (mhz >= 21.0  && mhz <= 21.45)  return "15m";
    if (mhz >= 24.89 && mhz <= 24.99)  return "12m";
    if (mhz >= 28.0  && mhz <= 29.7)   return "10m";
    if (mhz >= 50.0  && mhz <= 54.0)   return "6m";
    if (mhz >= 144.0 && mhz <= 148.0)  return "2m";
    return "";
}

// ── BandFilterProxy ─────────────────────────────────────────────────────────

void BandFilterProxy::setBandVisible(const QString& band, bool visible)
{
    if (visible)
        m_hiddenBands.remove(band);
    else
        m_hiddenBands.insert(band);
    invalidateFilter();
}

bool BandFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    if (m_hiddenBands.isEmpty())
        return true;
    auto idx = sourceModel()->index(sourceRow, SpotTableModel::ColBand, sourceParent);
    QString band = sourceModel()->data(idx, Qt::DisplayRole).toString();
    if (band.isEmpty())
        return true;  // unknown band — always show
    return !m_hiddenBands.contains(band);
}

// ── DxClusterDialog ─────────────────────────────────────────────────────────

DxClusterDialog::DxClusterDialog(DxClusterClient* client, QWidget* parent)
    : QDialog(parent), m_client(client)
{
    setWindowTitle("DX Cluster");
    setMinimumSize(620, 480);
    resize(700, 560);

    auto* root = new QVBoxLayout(this);
    root->setSpacing(0);
    root->setContentsMargins(4, 4, 4, 4);

    auto* tabs = new QTabWidget;
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #203040; }"
        "QTabBar::tab { background: #1a1a2e; color: #808890; border: 1px solid #203040; "
        "  padding: 6px 16px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #0f0f1a; color: #00b4d8; border-bottom: none; }");

    buildSettingsTab(tabs);
    buildSpotListTab(tabs);

    root->addWidget(tabs);

    // ── Live updates from client ────────────────────────────────────────
    connect(client, &DxClusterClient::rawLineReceived, this, [this](const QString& line) {
        m_console->appendPlainText(line);
        auto* sb = m_console->verticalScrollBar();
        sb->setValue(sb->maximum());
    });

    connect(client, &DxClusterClient::spotReceived, this, [this](const DxSpot& spot) {
        m_spotModel->addSpot(spot);
    });

    connect(client, &DxClusterClient::connected, this, [this] {
        m_statusLabel->setText(QString("Connected to %1:%2").arg(m_client->host()).arg(m_client->port()));
        m_statusLabel->setStyleSheet("QLabel { color: #00b4d8; font-size: 11px; }");
        m_connectBtn->setText("Disconnect");
        m_cmdEdit->setEnabled(true);
        m_sendBtn->setEnabled(true);
        m_console->appendPlainText("--- Connected ---");
    });
    connect(client, &DxClusterClient::disconnected, this, [this] {
        m_statusLabel->setText("Disconnected");
        m_statusLabel->setStyleSheet("QLabel { color: #808080; font-size: 11px; }");
        m_connectBtn->setText("Connect");
        m_cmdEdit->setEnabled(false);
        m_sendBtn->setEnabled(false);
        m_console->appendPlainText("--- Disconnected ---");
    });
    connect(client, &DxClusterClient::connectionError, this, [this](const QString& err) {
        m_statusLabel->setText("Error: " + err);
        m_statusLabel->setStyleSheet("QLabel { color: #ff4444; font-size: 11px; }");
        m_console->appendPlainText("--- Error: " + err + " ---");
    });

    updateStatus();
}

void DxClusterDialog::buildSettingsTab(QTabWidget* tabs)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto& s = AppSettings::instance();

    // ── Connection settings ─────────────────────────────────────────────
    auto* connGroup = new QGroupBox("Connection");
    auto* connLayout = new QVBoxLayout(connGroup);
    connLayout->setSpacing(4);

    auto* grid = new QGridLayout;
    grid->setColumnStretch(1, 1);
    int row = 0;

    grid->addWidget(new QLabel("Server:"), row, 0);
    m_hostEdit = new QLineEdit(s.value("DxClusterHost", "dxc.nc7j.com").toString());
    m_hostEdit->setPlaceholderText("dxc.nc7j.com");
    m_hostEdit->setStyleSheet("QLineEdit { background: #1a1a2e; color: #c8d8e8; border: 1px solid #203040; padding: 3px; }");
    grid->addWidget(m_hostEdit, row, 1);
    row++;

    grid->addWidget(new QLabel("Port:"), row, 0);
    m_portSpin = new QSpinBox;
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(s.value("DxClusterPort", 7300).toInt());
    m_portSpin->setStyleSheet("QSpinBox { background: #1a1a2e; color: #c8d8e8; border: 1px solid #203040; padding: 3px; }");
    grid->addWidget(m_portSpin, row, 1);
    row++;

    grid->addWidget(new QLabel("Callsign:"), row, 0);
    m_callEdit = new QLineEdit(s.value("DxClusterCallsign").toString());
    m_callEdit->setPlaceholderText("your callsign");
    m_callEdit->setStyleSheet("QLineEdit { background: #1a1a2e; color: #c8d8e8; border: 1px solid #203040; padding: 3px; }");
    grid->addWidget(m_callEdit, row, 1);
    row++;

    connLayout->addLayout(grid);

    // Button row
    auto* btnRow = new QHBoxLayout;
    m_autoConnectBtn = new QPushButton(
        s.value("DxClusterAutoConnect", "False").toString() == "True" ? "Auto-Connect: ON" : "Auto-Connect: OFF");
    m_autoConnectBtn->setCheckable(true);
    m_autoConnectBtn->setChecked(s.value("DxClusterAutoConnect", "False").toString() == "True");
    m_autoConnectBtn->setStyleSheet(
        "QPushButton { background: #206030; color: white; border: 1px solid #305040; padding: 4px 10px; }"
        "QPushButton:!checked { background: #603020; }");
    connect(m_autoConnectBtn, &QPushButton::toggled, this, [this](bool on) {
        m_autoConnectBtn->setText(on ? "Auto-Connect: ON" : "Auto-Connect: OFF");
        auto& s = AppSettings::instance();
        s.setValue("DxClusterAutoConnect", on ? "True" : "False");
        s.save();
    });
    btnRow->addWidget(m_autoConnectBtn);
    btnRow->addStretch();

    m_statusLabel = new QLabel("Disconnected");
    m_statusLabel->setStyleSheet("QLabel { color: #808080; font-size: 11px; }");
    btnRow->addWidget(m_statusLabel);
    btnRow->addStretch();

    m_connectBtn = new QPushButton(m_client->isConnected() ? "Disconnect" : "Connect");
    m_connectBtn->setFixedWidth(100);
    m_connectBtn->setStyleSheet(
        "QPushButton { background: #00b4d8; color: #0f0f1a; font-weight: bold; "
        "border: 1px solid #008ba8; padding: 4px; border-radius: 3px; }"
        "QPushButton:hover { background: #00c8f0; }"
        "QPushButton:disabled { background: #404060; color: #808080; }");
    connect(m_connectBtn, &QPushButton::clicked, this, [this] {
        if (m_client->isConnected()) {
            emit disconnectRequested();
            return;
        }
        QString host = m_hostEdit->text().trimmed();
        QString call = m_callEdit->text().trimmed().toUpper();
        quint16 port = static_cast<quint16>(m_portSpin->value());
        if (host.isEmpty() || call.isEmpty()) {
            m_statusLabel->setText("Server and callsign are required");
            m_statusLabel->setStyleSheet("QLabel { color: #ff4444; font-size: 11px; }");
            return;
        }
        auto& s = AppSettings::instance();
        s.setValue("DxClusterHost", host);
        s.setValue("DxClusterPort", port);
        s.setValue("DxClusterCallsign", call);
        s.save();
        emit connectRequested(host, port, call);
    });
    btnRow->addWidget(m_connectBtn);
    connLayout->addLayout(btnRow);

    layout->addWidget(connGroup);

    // ── Console output ──────────────────────────────────────────────────
    auto* consoleLabel = new QLabel("Cluster Console");
    consoleLabel->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; }");
    layout->addWidget(consoleLabel);

    m_console = new QPlainTextEdit;
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(2000);
    m_console->setStyleSheet(
        "QPlainTextEdit {"
        "  background: #0a0a14;"
        "  color: #a0b0c0;"
        "  font-family: monospace;"
        "  font-size: 11px;"
        "  border: 1px solid #203040;"
        "  padding: 4px;"
        "}");
    layout->addWidget(m_console, 1);

    // Command input row
    auto* cmdRow = new QHBoxLayout;
    m_cmdEdit = new QLineEdit;
    m_cmdEdit->setPlaceholderText("Type a cluster command (e.g. sh/dx 20, set/filter, bye)");
    m_cmdEdit->setStyleSheet("QLineEdit { background: #1a1a2e; color: #c8d8e8; border: 1px solid #203040; padding: 3px; font-family: monospace; }");
    m_cmdEdit->setEnabled(m_client->isConnected());
    connect(m_cmdEdit, &QLineEdit::returnPressed, this, [this] {
        QString cmd = m_cmdEdit->text().trimmed();
        if (cmd.isEmpty() || !m_client->isConnected()) return;
        m_client->sendCommand(cmd);
        m_console->appendPlainText("> " + cmd);
        m_cmdEdit->clear();
    });
    m_sendBtn = new QPushButton("Send");
    m_sendBtn->setFixedWidth(60);
    m_sendBtn->setEnabled(m_client->isConnected());
    connect(m_sendBtn, &QPushButton::clicked, this, [this] {
        m_cmdEdit->returnPressed();
    });
    cmdRow->addWidget(m_cmdEdit, 1);
    cmdRow->addWidget(m_sendBtn);
    layout->addLayout(cmdRow);

    tabs->addTab(page, "Settings");
}

void DxClusterDialog::buildSpotListTab(QTabWidget* tabs)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(4);

    // Band filter checkboxes
    auto* filterRow = new QHBoxLayout;
    filterRow->setSpacing(2);
    auto* filterLabel = new QLabel("Bands:");
    filterLabel->setStyleSheet("QLabel { color: #808080; font-size: 13px; }");
    filterRow->addWidget(filterLabel);

    // Table model + band filter proxy
    m_spotModel = new SpotTableModel(this);
    m_proxyModel = new BandFilterProxy(this);
    m_proxyModel->setSourceModel(m_spotModel);
    m_proxyModel->setSortRole(Qt::UserRole);

    static constexpr const char* bands[] = {
        "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"
    };
    QString cbStyle =
        "QCheckBox { color: #a0b0c0; font-size: 12px; spacing: 3px; }"
        "QCheckBox::indicator { width: 13px; height: 13px; }";
    for (const char* band : bands) {
        auto* cb = new QCheckBox(band);
        cb->setChecked(true);
        cb->setStyleSheet(cbStyle);
        connect(cb, &QCheckBox::toggled, this, [this, b = QString(band)](bool on) {
            m_proxyModel->setBandVisible(b, on);
        });
        filterRow->addWidget(cb, 1);  // equal stretch across row
    }
    layout->addLayout(filterRow);

    m_spotTable = new QTableView;
    m_spotTable->setModel(m_proxyModel);
    m_spotTable->setSortingEnabled(true);
    m_spotTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_spotTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_spotTable->setAlternatingRowColors(true);
    m_spotTable->verticalHeader()->setVisible(false);
    m_spotTable->verticalHeader()->setDefaultSectionSize(20);
    m_spotTable->horizontalHeader()->setStretchLastSection(true);
    m_spotTable->setStyleSheet(
        "QTableView {"
        "  background: #0a0a14;"
        "  alternate-background-color: #0f0f1e;"
        "  color: #c8d8e8;"
        "  gridline-color: #1a2a3a;"
        "  border: 1px solid #203040;"
        "  font-size: 11px;"
        "}"
        "QTableView::item:selected {"
        "  background: #1a3a5a;"
        "  color: #e0f0ff;"
        "}"
        "QHeaderView::section {"
        "  background: #1a1a2e;"
        "  color: #00b4d8;"
        "  border: 1px solid #203040;"
        "  padding: 3px 6px;"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "}");

    // Column widths
    m_spotTable->setColumnWidth(SpotTableModel::ColTime, 50);
    m_spotTable->setColumnWidth(SpotTableModel::ColFreq, 80);
    m_spotTable->setColumnWidth(SpotTableModel::ColDxCall, 90);
    m_spotTable->setColumnWidth(SpotTableModel::ColComment, 200);
    m_spotTable->setColumnWidth(SpotTableModel::ColSpotter, 80);
    m_spotTable->setColumnWidth(SpotTableModel::ColBand, 45);

    // No default sort — insertion order is newest-first
    m_spotTable->horizontalHeader()->setSortIndicatorShown(false);

    // Double-click to tune
    connect(m_spotTable, &QTableView::doubleClicked, this, [this](const QModelIndex& idx) {
        auto srcIdx = m_proxyModel->mapToSource(idx);
        double freq = m_spotModel->freqAtRow(srcIdx.row());
        if (freq > 0.0)
            emit tuneRequested(freq);
    });

    layout->addWidget(m_spotTable, 1);

    // Bottom bar: spot count + clear
    auto* bottomRow = new QHBoxLayout;
    auto* countLabel = new QLabel("0 spots");
    countLabel->setStyleSheet("QLabel { color: #808080; font-size: 11px; }");
    connect(m_spotModel, &QAbstractTableModel::rowsInserted, this, [this, countLabel] {
        countLabel->setText(QString("%1 spots").arg(m_spotModel->rowCount()));
    });
    bottomRow->addWidget(countLabel);
    bottomRow->addStretch();

    auto* clearBtn = new QPushButton("Clear");
    clearBtn->setFixedWidth(60);
    connect(clearBtn, &QPushButton::clicked, this, [this, countLabel] {
        m_spotModel->clear();
        countLabel->setText("0 spots");
    });
    bottomRow->addWidget(clearBtn);
    layout->addLayout(bottomRow);

    tabs->addTab(page, "Spot List");
}

void DxClusterDialog::updateStatus()
{
    if (m_client->isConnected()) {
        m_statusLabel->setText(QString("Connected to %1:%2").arg(m_client->host()).arg(m_client->port()));
        m_statusLabel->setStyleSheet("QLabel { color: #00b4d8; font-size: 11px; }");
        m_connectBtn->setText("Disconnect");
        m_cmdEdit->setEnabled(true);
        m_sendBtn->setEnabled(true);
    } else {
        m_statusLabel->setText("Disconnected");
        m_statusLabel->setStyleSheet("QLabel { color: #808080; font-size: 11px; }");
        m_connectBtn->setText("Connect");
        m_cmdEdit->setEnabled(false);
        m_sendBtn->setEnabled(false);
    }
}

} // namespace AetherSDR
