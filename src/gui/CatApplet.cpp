#include "CatApplet.h"
#include "core/RigctlServer.h"
#include "core/RigctlPty.h"
#include "core/AudioEngine.h"
#include "ComboStyle.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QProgressBar>
#include <QApplication>
#include "core/AppSettings.h"
#include <QFrame>
#include <QSlider>

namespace AetherSDR {

static constexpr const char* kSectionStyle =
    "QWidget { background: transparent; }"
    "QLabel { color: #8090a0; font-size: 11px; }"
    "QPushButton { background: #1a2a3a; border: 1px solid #205070;"
    "  border-radius: 3px; padding: 2px 8px; font-size: 11px; font-weight: bold; color: #c8d8e8; }"
    "QPushButton:hover { background: #204060; }";

static QWidget* appletTitleBar(const QString& text)
{
    auto* bar = new QWidget;
    bar->setFixedHeight(16);
    bar->setStyleSheet(
        "QWidget { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "stop:0 #3a4a5a, stop:0.5 #2a3a4a, stop:1 #1a2a38); "
        "border-bottom: 1px solid #0a1a28; }");

    auto* lbl = new QLabel(text, bar);
    lbl->setStyleSheet("QLabel { background: transparent; color: #8aa8c0; "
                       "font-size: 10px; font-weight: bold; }");
    lbl->setGeometry(6, 1, 240, 14);
    return bar;
}

static QFrame* separator()
{
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #203040;");
    line->setFixedHeight(1);
    return line;
}

CatApplet::CatApplet(QWidget* parent) : QWidget(parent)
{
    buildUI();
    hide();  // hidden by default, shown via CAT toggle button
}

void CatApplet::buildUI()
{
    setStyleSheet(kSectionStyle);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    outer->addWidget(appletTitleBar("CAT Control"));

    auto* content = new QWidget;
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(4, 2, 4, 2);
    root->setSpacing(4);
    outer->addWidget(content);

    auto& settings = AppSettings::instance();

    static const QString kGreenToggle =
        "QPushButton { background: #1a2a3a; border: 1px solid #205070; border-radius: 3px;"
        " color: #c8d8e8; font-size: 11px; font-weight: bold; padding: 2px 8px; }"
        "QPushButton:hover { background: #204060; }"
        "QPushButton:checked { background: #006040; color: #00ff88; border: 1px solid #00a060; }";

    static constexpr const char* kDimLabel =
        "QLabel { color: #8090a0; font-size: 11px; }";

    static constexpr int kLabelW = 40;

    static constexpr const char* kInsetStyle =
        "QLineEdit { font-size: 11px; background: #0a0a18; border: 1px solid #1e2e3e;"
        " border-radius: 3px; padding: 1px 4px; color: #c8d8e8; }";

    // ── Slice selector ──────────────────────────────────────────────────────
    auto* sliceRow = new QHBoxLayout;
    sliceRow->setSpacing(4);
    auto* sliceLabel = new QLabel("Slice:");
    sliceLabel->setStyleSheet(kDimLabel);
    sliceLabel->setFixedWidth(kLabelW);
    sliceRow->addWidget(sliceLabel);
    m_sliceSelect = new QComboBox;
    // Populated dynamically in setRadioModel()
    m_sliceSelect->setCurrentIndex(0);
    AetherSDR::applyComboStyle(m_sliceSelect);
    sliceRow->addWidget(m_sliceSelect, 1);
    root->addLayout(sliceRow);

    // ── Virtual Serial Port ──────────────────────────────────────────────────
    auto* ptyRow1 = new QHBoxLayout;
    ptyRow1->setSpacing(4);
    auto* ttyLabel = new QLabel("TTY:");
    ttyLabel->setStyleSheet(kDimLabel);
    ttyLabel->setFixedWidth(kLabelW);
    ptyRow1->addWidget(ttyLabel);

    m_ptyEnable = new QPushButton("Enable");
    m_ptyEnable->setCheckable(true);
    m_ptyEnable->setStyleSheet(kGreenToggle);
    m_ptyEnable->setFixedSize(60, 22);
    ptyRow1->addWidget(m_ptyEnable);

    auto* pathLabel = new QLabel("Path:");
    pathLabel->setStyleSheet(kDimLabel);
    ptyRow1->addWidget(pathLabel);
    m_ptyPath = new QLineEdit(
        settings.value("CatTtyPath", "/tmp/AetherSDR-CAT").toString());
    m_ptyPath->setStyleSheet(
        "QLineEdit { font-size: 10px; background: #0a0a18; border: 1px solid #1e2e3e;"
        " border-radius: 3px; padding: 0px 2px; color: #c8d8e8; }");
    ptyRow1->addWidget(m_ptyPath, 1);

    connect(m_ptyPath, &QLineEdit::editingFinished, this, [this]() {
        auto& ss = AppSettings::instance();
        ss.setValue("CatTtyPath", m_ptyPath->text());
        ss.save();
    });

    root->addLayout(ptyRow1);

    connect(m_ptyEnable, &QPushButton::toggled, this, [this](bool on) {
        if (!m_pty) return;
        if (on)
            m_pty->start();
        else
            m_pty->stop();
        updatePtyStatus();
    });

    // ── rigctld TCP ──────────────────────────────────────────────────────────
    auto* tcpRow = new QHBoxLayout;
    tcpRow->setSpacing(4);
    auto* tcpLabel = new QLabel("rigctld:");
    tcpLabel->setStyleSheet(kDimLabel);
    tcpLabel->setFixedWidth(kLabelW);
    tcpRow->addWidget(tcpLabel);

    m_tcpEnable = new QPushButton("Enable");
    m_tcpEnable->setCheckable(true);
    m_tcpEnable->setStyleSheet(kGreenToggle);
    m_tcpEnable->setFixedSize(60, 22);
    tcpRow->addWidget(m_tcpEnable);

    auto* portLabel = new QLabel("Port:");
    portLabel->setStyleSheet(kDimLabel);
    tcpRow->addWidget(portLabel);

    // Port + status in a single inset container (matches TTY path inset)
    auto* portContainer = new QWidget;
    portContainer->setStyleSheet(
        "QWidget#portInset { background: #0a0a18; border: 1px solid #1e2e3e;"
        " border-radius: 3px; }");
    portContainer->setObjectName("portInset");
    auto* portLayout = new QHBoxLayout(portContainer);
    portLayout->setContentsMargins(2, 0, 4, 0);
    portLayout->setSpacing(0);

    m_tcpPort = new QLineEdit(settings.value("CatTcpPort", "4532").toString());
    m_tcpPort->setFixedWidth(36);
    m_tcpPort->setAlignment(Qt::AlignCenter);
    m_tcpPort->setStyleSheet(
        "QLineEdit { font-size: 10px; background: transparent; border: none;"
        " padding: 0px; color: #c8d8e8; }");
    portLayout->addWidget(m_tcpPort);

    portLayout->addStretch();
    m_tcpStatus = new QLabel("(stopped)");
    m_tcpStatus->setStyleSheet("QLabel { font-size: 10px; color: #506070; }");
    m_tcpStatus->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    portLayout->addWidget(m_tcpStatus);

    tcpRow->addWidget(portContainer, 1);
    root->addLayout(tcpRow);

    connect(m_tcpEnable, &QPushButton::toggled, this, [this](bool on) {
        if (!m_server) return;
        if (on) {
            int port = m_tcpPort->text().toInt();
            if (port < 1024 || port > 65535) port = 4532;
            auto& ss = AppSettings::instance();
            ss.setValue("CatTcpPort", QString::number(port));
            ss.save();
            m_server->start(static_cast<quint16>(port));
        } else {
            m_server->stop();
        }
        updateTcpStatus();
    });

    connect(m_tcpPort, &QLineEdit::editingFinished, this, [this]() {
        int port = m_tcpPort->text().toInt();
        if (port < 1024 || port > 65535) {
            port = 4532;
            m_tcpPort->setText("4532");
        }
        auto& ss = AppSettings::instance();
        ss.setValue("CatTcpPort", QString::number(port));
        ss.save();
        if (m_server && m_server->isRunning()) {
            m_server->stop();
            m_server->start(static_cast<quint16>(port));
            updateTcpStatus();
        }
    });

    // ── DAX Section ─────────────────────────────────────────────────────────
    outer->addWidget(appletTitleBar("DAX Audio Channels"));

    // DAX enable row
    auto* daxEnRow = new QHBoxLayout;
    daxEnRow->setContentsMargins(4, 2, 4, 2);
    auto* daxLabel = new QLabel("DAX:");
    daxLabel->setStyleSheet(kDimLabel);
    daxEnRow->addWidget(daxLabel);
    daxEnRow->addStretch();
    m_daxEnable = new QPushButton("Enable");
    m_daxEnable->setCheckable(true);
    m_daxEnable->setStyleSheet(kGreenToggle);
    m_daxEnable->setFixedSize(60, 22);
    daxEnRow->addWidget(m_daxEnable);
    outer->addLayout(daxEnRow);

    // RX channel meters (DAX 1-4)
    const QString kMeterStyle =
        "QProgressBar { background: #0a0a18; border: 1px solid #1e2e3e;"
        "  border-radius: 2px; max-height: 10px; }"
        "QProgressBar::chunk { background: #00b4d8; border-radius: 1px; }";
    const QString kStatusLabel = "QLabel { color: #506070; font-size: 11px; }";

    for (int i = 0; i < 4; ++i) {
        auto* row = new QHBoxLayout;
        row->setContentsMargins(4, 1, 4, 1);
        row->setSpacing(4);
        auto* chLabel = new QLabel(QString("DAX %1:").arg(i + 1));
        chLabel->setStyleSheet(kDimLabel);
        chLabel->setFixedWidth(40);
        row->addWidget(chLabel);

        m_daxRxStatus[i] = new QLabel("—");
        m_daxRxStatus[i]->setStyleSheet(kStatusLabel);
        m_daxRxStatus[i]->setFixedWidth(40);
        row->addWidget(m_daxRxStatus[i]);

        m_daxRxLevel[i] = new QProgressBar;
        m_daxRxLevel[i]->setRange(0, 100);
        m_daxRxLevel[i]->setValue(0);
        m_daxRxLevel[i]->setTextVisible(false);
        m_daxRxLevel[i]->setStyleSheet(kMeterStyle);
        row->addWidget(m_daxRxLevel[i], 1);

        outer->addLayout(row);
    }

    // TX meter
    auto* txRow = new QHBoxLayout;
    txRow->setContentsMargins(4, 1, 4, 1);
    txRow->setSpacing(4);
    auto* txLabel = new QLabel("TX:");
    txLabel->setStyleSheet(kDimLabel);
    txLabel->setFixedWidth(40);
    txRow->addWidget(txLabel);

    m_daxTxStatus = new QLabel("—");
    m_daxTxStatus->setStyleSheet(kStatusLabel);
    m_daxTxStatus->setFixedWidth(40);
    txRow->addWidget(m_daxTxStatus);

    m_daxTxLevel = new QProgressBar;
    m_daxTxLevel->setRange(0, 100);
    m_daxTxLevel->setValue(0);
    m_daxTxLevel->setTextVisible(false);
    m_daxTxLevel->setStyleSheet(kMeterStyle);
    txRow->addWidget(m_daxTxLevel, 1);

    outer->addLayout(txRow);
}

void CatApplet::setRadioModel(RadioModel* model)
{
    m_model = model;
    if (model) {
        connect(model, &RadioModel::connectionStateChanged,
                this, &CatApplet::onConnectionStateChanged);
        connect(model, &RadioModel::infoChanged,
                this, [this]() {
            // Rebuild slice list from radio's max slices
            int maxSlices = m_model->maxSlices();
            m_sliceSelect->clear();
            static const char letters[] = "ABCDEFGH";
            for (int i = 0; i < maxSlices && i < 8; ++i)
                m_sliceSelect->addItem(QString("Slice %1").arg(letters[i]));
        });

        // Wire slice add/remove for DAX channel tracking
        connect(model, &RadioModel::sliceAdded, this, [this](SliceModel* s) {
            wireSliceDax(s);
        });
        connect(model, &RadioModel::sliceRemoved, this, [this](int) {
            updateDaxSliceAssignments();
        });

        // Wire TX state for TX status label
        connect(model->transmitModel(), &TransmitModel::moxChanged, this, [this]() {
            updateDaxTxStatus();
        });
    }
}

void CatApplet::wireSliceDax(SliceModel* s)
{
    connect(s, &SliceModel::daxChannelChanged, this, [this]() {
        updateDaxSliceAssignments();
    });
    updateDaxSliceAssignments();
}

void CatApplet::updateDaxSliceAssignments()
{
    // Clear all
    for (int i = 0; i < 4; ++i)
        m_daxRxStatus[i]->setText("—");

    if (!m_model) return;

    static const char letters[] = "ABCDEFGH";
    for (auto* s : m_model->slices()) {
        int ch = s->daxChannel();
        if (ch >= 1 && ch <= 4) {
            int idx = ch - 1;
            QString name = QString("Slice %1").arg(letters[s->sliceId()]);
            m_daxRxStatus[idx]->setText(name);
        }
    }
}

void CatApplet::updateDaxTxStatus()
{
    if (!m_model) {
        m_daxTxStatus->setText("—");
        return;
    }

    bool isTx = m_model->transmitModel()->isMox();
    if (isTx) {
        // Find the TX slice
        static const char letters[] = "ABCDEFGH";
        for (auto* s : m_model->slices()) {
            if (s->isTxSlice()) {
                m_daxTxStatus->setText(QString("Slice %1").arg(letters[s->sliceId()]));
                return;
            }
        }
        m_daxTxStatus->setText("TX");
    } else {
        m_daxTxStatus->setText("Ready");
    }
}

void CatApplet::setRigctlServer(RigctlServer* server)
{
    m_server = server;
    if (server) {
        connect(server, &RigctlServer::clientCountChanged, this, &CatApplet::updateTcpStatus);

        // Auto-start if was enabled
        if (m_tcpEnable->isChecked()) {
            server->start(static_cast<quint16>(m_tcpPort->text().toInt()));
            updateTcpStatus();
        }
    }
}

void CatApplet::setRigctlPty(RigctlPty* pty)
{
    m_pty = pty;
    if (pty) {
        connect(pty, &RigctlPty::started, this, [this](const QString& path) {
            m_ptyPath->setText(path);
        });
        connect(pty, &RigctlPty::stopped, this, [this]() {
            m_ptyPath->setText("—");
        });

        // Auto-start handled by MainWindow via Autostart menu items
    }
}

void CatApplet::setAudioEngine(AudioEngine* audio)
{
    m_audio = audio;
}

void CatApplet::onConnectionStateChanged(bool /*connected*/)
{
    // DAX auto-start deferred — needs PipeWire virtual devices (issue #15)
}

void CatApplet::updateTcpStatus()
{
    if (!m_server || !m_server->isRunning()) {
        m_tcpStatus->setText("(stopped)");
        return;
    }
    int n = m_server->clientCount();
    m_tcpStatus->setText(QStringLiteral("(%1 client%2)")
                             .arg(n)
                             .arg(n == 1 ? "" : "s"));
}

void CatApplet::updatePtyStatus()
{
    if (!m_pty || !m_pty->isRunning()) {
        m_ptyPath->setText("—");
    } else {
        m_ptyPath->setText(m_pty->symlinkPath());
    }
}

void CatApplet::setTcpEnabled(bool on)
{
    QSignalBlocker b(m_tcpEnable);
    m_tcpEnable->setChecked(on);
    updateTcpStatus();
}

void CatApplet::setPtyEnabled(bool on)
{
    QSignalBlocker b(m_ptyEnable);
    m_ptyEnable->setChecked(on);
    updatePtyStatus();
}

} // namespace AetherSDR
