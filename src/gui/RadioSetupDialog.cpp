#include "RadioSetupDialog.h"
#include "models/RadioModel.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QDialogButtonBox>

namespace AetherSDR {

static const QString kGroupStyle =
    "QGroupBox { border: 1px solid #304050; border-radius: 4px; "
    "margin-top: 8px; padding-top: 12px; font-weight: bold; color: #8aa8c0; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 10px; "
    "padding: 0 4px; }";

static const QString kLabelStyle =
    "QLabel { color: #c8d8e8; font-size: 12px; }";

static const QString kValueStyle =
    "QLabel { color: #00c8ff; font-size: 12px; font-weight: bold; }";

static const QString kEditStyle =
    "QLineEdit { background: #1a2a3a; border: 1px solid #304050; "
    "border-radius: 3px; color: #c8d8e8; font-size: 12px; padding: 2px 4px; }";

RadioSetupDialog::RadioSetupDialog(RadioModel* model, QWidget* parent)
    : QDialog(parent), m_model(model)
{
    setWindowTitle("Radio Setup");
    setMinimumSize(580, 400);
    setStyleSheet("QDialog { background: #0f0f1a; }");

    auto* layout = new QVBoxLayout(this);

    auto* tabs = new QTabWidget;
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #304050; background: #0f0f1a; }"
        "QTabBar::tab { background: #1a2a3a; color: #8aa8c0; "
        "border: 1px solid #304050; padding: 4px 12px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #0f0f1a; color: #c8d8e8; "
        "border-bottom-color: #0f0f1a; }");

    tabs->addTab(buildRadioTab(), "Radio");
    tabs->addTab(buildNetworkTab(), "Network");
    tabs->addTab(buildGpsTab(), "GPS");
    tabs->addTab(buildTxTab(), "TX");
    tabs->addTab(buildPhoneCwTab(), "Phone/CW");
    tabs->addTab(buildRxTab(), "RX");
    tabs->addTab(buildFiltersTab(), "Filters");
    tabs->addTab(buildXvtrTab(), "XVTR");

    layout->addWidget(tabs);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->setStyleSheet(
        "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 3px; color: #c8d8e8; padding: 4px 16px; }"
        "QPushButton:hover { background: #203040; }");
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    layout->addWidget(buttons);
}

// ── Radio tab ─────────────────────────────────────────────────────────────────

QWidget* RadioSetupDialog::buildRadioTab()
{
    auto* page = new QWidget;
    auto* vbox = new QVBoxLayout(page);
    vbox->setSpacing(8);

    // Toggle button style: green when on, red when off
    static const QString kToggleStyle =
        "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 3px; color: #c8d8e8; font-size: 11px; font-weight: bold; "
        "padding: 3px 10px; }"
        "QPushButton:checked { background: #1a5030; color: #00e060; "
        "border: 1px solid #20a040; }";

    auto makeToggle = [](const QString& text, bool checked) {
        auto* btn = new QPushButton(checked ? "Enabled" : "Disabled");
        btn->setCheckable(true);
        btn->setChecked(checked);
        btn->setStyleSheet(kToggleStyle);
        QObject::connect(btn, &QPushButton::toggled, btn, [btn](bool on) {
            btn->setText(on ? "Enabled" : "Disabled");
        });
        return btn;
    };

    // Radio Information group
    {
        auto* group = new QGroupBox("Radio Information");
        group->setStyleSheet(kGroupStyle);
        auto* grid = new QGridLayout(group);
        grid->setSpacing(6);

        grid->addWidget(new QLabel("Radio SN:"), 0, 0);
        m_serialLabel = new QLabel(m_model->chassisSerial().isEmpty()
            ? m_model->serial() : m_model->chassisSerial());
        m_serialLabel->setStyleSheet(kValueStyle);
        grid->addWidget(m_serialLabel, 0, 1);

        grid->addWidget(new QLabel("Region:"), 0, 2);
        m_regionLabel = new QLabel(m_model->region().isEmpty() ? "USA" : m_model->region());
        m_regionLabel->setStyleSheet(
            "QLabel { background: #1a2a3a; border: 1px solid #304050; "
            "border-radius: 3px; color: #00c8ff; font-size: 11px; font-weight: bold; "
            "padding: 3px 10px; }");
        m_regionLabel->setAlignment(Qt::AlignCenter);
        grid->addWidget(m_regionLabel, 0, 3);

        grid->addWidget(new QLabel("HW Version:"), 1, 0);
        m_hwVersionLabel = new QLabel("v" + m_model->version());
        m_hwVersionLabel->setStyleSheet(kValueStyle);
        grid->addWidget(m_hwVersionLabel, 1, 1);

        grid->addWidget(new QLabel("Remote On:"), 1, 2);
        m_remoteOnBtn = makeToggle("", m_model->remoteOnEnabled());
        connect(m_remoteOnBtn, &QPushButton::toggled, this, [this](bool on) {
            m_model->setRemoteOnEnabled(on);
        });
        grid->addWidget(m_remoteOnBtn, 1, 3);

        grid->addWidget(new QLabel("Options:"), 2, 0);
        m_optionsLabel = new QLabel(m_model->radioOptions().isEmpty()
            ? (m_model->hasAmplifier() ? "GPS, PGXL" : "GPS")
            : m_model->radioOptions());
        m_optionsLabel->setStyleSheet(kValueStyle);
        grid->addWidget(m_optionsLabel, 2, 1);

        grid->addWidget(new QLabel("FlexControl:"), 2, 2);
        auto* fcBtn = makeToggle("", true);
        grid->addWidget(fcBtn, 2, 3);

        grid->addWidget(new QLabel("multiFLEX:"), 3, 2);
        auto* mfBtn = makeToggle("", m_model->multiFlexEnabled());
        connect(mfBtn, &QPushButton::toggled, this, [this](bool on) {
            m_model->setMultiFlexEnabled(on);
        });
        grid->addWidget(mfBtn, 3, 3);

        for (auto* lbl : group->findChildren<QLabel*>()) {
            if (lbl->styleSheet().isEmpty())
                lbl->setStyleSheet(kLabelStyle);
        }

        vbox->addWidget(group);
    }

    // Radio Identification group
    {
        auto* group = new QGroupBox("Radio Identification");
        group->setStyleSheet(kGroupStyle);
        auto* grid = new QGridLayout(group);
        grid->setSpacing(6);

        grid->addWidget(new QLabel("Model:"), 0, 0);
        m_modelLabel = new QLabel(m_model->model());
        m_modelLabel->setStyleSheet(kValueStyle);
        grid->addWidget(m_modelLabel, 0, 1);

        grid->addWidget(new QLabel("Nickname:"), 0, 2);
        m_nicknameEdit = new QLineEdit(m_model->nickname().isEmpty()
            ? m_model->name() : m_model->nickname());
        m_nicknameEdit->setStyleSheet(kEditStyle);
        grid->addWidget(m_nicknameEdit, 0, 3);

        grid->addWidget(new QLabel("Callsign:"), 1, 0);
        m_callsignEdit = new QLineEdit(m_model->callsign());
        m_callsignEdit->setStyleSheet(kEditStyle);
        grid->addWidget(m_callsignEdit, 1, 1);

        connect(m_nicknameEdit, &QLineEdit::editingFinished, this, [this] {
            m_model->connection()->sendCommand("radio name " + m_nicknameEdit->text());
        });
        connect(m_callsignEdit, &QLineEdit::editingFinished, this, [this] {
            m_model->connection()->sendCommand("radio callsign " + m_callsignEdit->text());
        });

        for (auto* lbl : group->findChildren<QLabel*>()) {
            if (lbl->styleSheet().isEmpty())
                lbl->setStyleSheet(kLabelStyle);
        }

        vbox->addWidget(group);
    }

    vbox->addStretch(1);
    return page;
}

// ── Placeholder tabs ──────────────────────────────────────────────────────────

static QWidget* placeholderTab(const QString& name)
{
    auto* page = new QWidget;
    auto* vbox = new QVBoxLayout(page);
    auto* lbl = new QLabel(name + " settings — coming soon");
    lbl->setStyleSheet("QLabel { color: #6888a0; font-size: 14px; }");
    lbl->setAlignment(Qt::AlignCenter);
    vbox->addWidget(lbl);
    return page;
}

QWidget* RadioSetupDialog::buildNetworkTab()
{
    auto* page = new QWidget;
    auto* vbox = new QVBoxLayout(page);
    vbox->setSpacing(8);

    // Model header
    {
        auto* hdr = new QHBoxLayout;
        hdr->addStretch(1);
        auto* modelLbl = new QLabel(m_model->model());
        modelLbl->setStyleSheet("QLabel { color: #00c8ff; font-size: 20px; font-weight: bold; }");
        hdr->addWidget(modelLbl);
        vbox->addLayout(hdr);
    }

    // Network group
    {
        auto* group = new QGroupBox("Network");
        group->setStyleSheet(kGroupStyle);
        auto* grid = new QGridLayout(group);
        grid->setSpacing(6);

        grid->addWidget(new QLabel("IP Address:"), 0, 0);
        auto* ipLbl = new QLabel(m_model->ip());
        ipLbl->setStyleSheet(kValueStyle);
        grid->addWidget(ipLbl, 0, 1);

        grid->addWidget(new QLabel("Mask:"), 0, 2);
        auto* maskLbl = new QLabel(m_model->netmask());
        maskLbl->setStyleSheet(kValueStyle);
        grid->addWidget(maskLbl, 0, 3);

        grid->addWidget(new QLabel("MAC Address:"), 1, 0);
        auto* macLbl = new QLabel(m_model->mac());
        macLbl->setStyleSheet(kValueStyle);
        grid->addWidget(macLbl, 1, 1);

        for (auto* lbl : group->findChildren<QLabel*>())
            if (lbl->styleSheet().isEmpty()) lbl->setStyleSheet(kLabelStyle);

        vbox->addWidget(group);
    }

    // Advanced group
    {
        auto* group = new QGroupBox("Advanced");
        group->setStyleSheet(kGroupStyle);
        auto* grid = new QGridLayout(group);
        grid->setSpacing(6);

        grid->addWidget(new QLabel("Enforce Private IP Connections:"), 0, 0);
        auto* enforceBtn = new QPushButton(m_model->enforcePrivateIp() ? "Enabled" : "Disabled");
        enforceBtn->setCheckable(true);
        enforceBtn->setChecked(m_model->enforcePrivateIp());
        enforceBtn->setStyleSheet(
            "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
            "border-radius: 3px; color: #c8d8e8; font-size: 11px; font-weight: bold; "
            "padding: 3px 10px; }"
            "QPushButton:checked { background: #1a5030; color: #00e060; "
            "border: 1px solid #20a040; }");
        connect(enforceBtn, &QPushButton::toggled, this, [this, enforceBtn](bool on) {
            enforceBtn->setText(on ? "Enabled" : "Disabled");
            m_model->connection()->sendCommand(
                QString("radio set enforce_private_ip_connections=%1").arg(on ? 1 : 0));
        });
        grid->addWidget(enforceBtn, 0, 1);

        for (auto* lbl : group->findChildren<QLabel*>())
            if (lbl->styleSheet().isEmpty()) lbl->setStyleSheet(kLabelStyle);

        vbox->addWidget(group);
    }

    // DHCP / Static IP group
    {
        auto* group = new QGroupBox("IP Configuration");
        group->setStyleSheet(kGroupStyle);
        auto* gvbox = new QVBoxLayout(group);
        gvbox->setSpacing(6);

        // DHCP / Static buttons
        auto* btnRow = new QHBoxLayout;
        btnRow->setSpacing(4);

        const bool isStatic = m_model->hasStaticIp();

        auto* dhcpBtn = new QPushButton("DHCP");
        dhcpBtn->setCheckable(true);
        dhcpBtn->setChecked(!isStatic);
        dhcpBtn->setStyleSheet(
            "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
            "border-radius: 3px; color: #c8d8e8; font-size: 11px; font-weight: bold; "
            "padding: 4px 16px; }"
            "QPushButton:checked { background: #0070c0; color: #ffffff; "
            "border: 1px solid #0090e0; }");
        btnRow->addWidget(dhcpBtn);

        auto* staticBtn = new QPushButton("Static");
        staticBtn->setCheckable(true);
        staticBtn->setChecked(isStatic);
        staticBtn->setStyleSheet(dhcpBtn->styleSheet());
        btnRow->addWidget(staticBtn);

        btnRow->addStretch(1);
        gvbox->addLayout(btnRow);

        // Static IP fields
        auto* fieldsGrid = new QGridLayout;
        fieldsGrid->setSpacing(4);

        fieldsGrid->addWidget(new QLabel("IP Address:"), 0, 0);
        // If static is configured, show those values; otherwise show current DHCP values
        auto* staticIp = new QLineEdit(isStatic ? m_model->staticIp() : m_model->ip());
        staticIp->setStyleSheet(kEditStyle);
        staticIp->setEnabled(isStatic);
        fieldsGrid->addWidget(staticIp, 0, 1);

        fieldsGrid->addWidget(new QLabel("Mask:"), 1, 0);
        auto* staticMask = new QLineEdit(isStatic ? m_model->staticNetmask() : m_model->netmask());
        staticMask->setStyleSheet(kEditStyle);
        staticMask->setEnabled(isStatic);
        fieldsGrid->addWidget(staticMask, 1, 1);

        fieldsGrid->addWidget(new QLabel("Gateway:"), 2, 0);
        auto* staticGw = new QLineEdit(isStatic ? m_model->staticGateway() : m_model->gateway());
        staticGw->setStyleSheet(kEditStyle);
        staticGw->setEnabled(isStatic);
        fieldsGrid->addWidget(staticGw, 2, 1);

        for (auto* lbl : group->findChildren<QLabel*>())
            if (lbl->styleSheet().isEmpty()) lbl->setStyleSheet(kLabelStyle);

        gvbox->addLayout(fieldsGrid);

        // Apply button
        auto* applyBtn = new QPushButton("Apply");
        applyBtn->setEnabled(false);
        applyBtn->setStyleSheet(
            "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
            "border-radius: 3px; color: #c8d8e8; font-size: 11px; font-weight: bold; "
            "padding: 4px 16px; }"
            "QPushButton:hover { background: #203040; }");
        gvbox->addWidget(applyBtn, 0, Qt::AlignLeft);

        // DHCP/Static toggle logic
        connect(dhcpBtn, &QPushButton::clicked, this,
                [dhcpBtn, staticBtn, staticIp, staticMask, staticGw, applyBtn] {
            dhcpBtn->setChecked(true);
            staticBtn->setChecked(false);
            staticIp->setEnabled(false);
            staticMask->setEnabled(false);
            staticGw->setEnabled(false);
            applyBtn->setEnabled(true);
        });
        connect(staticBtn, &QPushButton::clicked, this,
                [dhcpBtn, staticBtn, staticIp, staticMask, staticGw, applyBtn] {
            dhcpBtn->setChecked(false);
            staticBtn->setChecked(true);
            staticIp->setEnabled(true);
            staticMask->setEnabled(true);
            staticGw->setEnabled(true);
            applyBtn->setEnabled(true);
        });

        // Apply sends the command
        connect(applyBtn, &QPushButton::clicked, this,
                [this, dhcpBtn, staticIp, staticMask, staticGw, applyBtn] {
            if (dhcpBtn->isChecked()) {
                m_model->connection()->sendCommand("radio static_net_params reset");
                qDebug() << "RadioSetupDialog: network set to DHCP";
            } else {
                const QString cmd = QString("radio static_net_params ip=%1 gateway=%2 netmask=%3")
                    .arg(staticIp->text(), staticGw->text(), staticMask->text());
                m_model->connection()->sendCommand(cmd);
                qDebug() << "RadioSetupDialog: static IP applied" << cmd;
            }
            applyBtn->setEnabled(false);
        });

        vbox->addWidget(group);
    }

    vbox->addStretch(1);
    return page;
}
QWidget* RadioSetupDialog::buildGpsTab()
{
    auto* page = new QWidget;
    auto* vbox = new QVBoxLayout(page);
    vbox->setSpacing(8);

    // Model header
    {
        auto* hdr = new QHBoxLayout;
        hdr->addStretch(1);
        auto* modelLbl = new QLabel(m_model->model());
        modelLbl->setStyleSheet("QLabel { color: #00c8ff; font-size: 20px; font-weight: bold; }");
        hdr->addWidget(modelLbl);
        vbox->addLayout(hdr);
    }

    // GPS installed status
    {
        const bool installed = (m_model->gpsStatus() != "Not Present"
                                && !m_model->gpsStatus().isEmpty());
        auto* statusLbl = new QLabel(installed ? "GPS is installed" : "GPS is not installed");
        statusLbl->setStyleSheet(installed
            ? "QLabel { color: #00c040; font-size: 16px; font-weight: bold; }"
            : "QLabel { color: #c04040; font-size: 16px; font-weight: bold; }");
        statusLbl->setAlignment(Qt::AlignCenter);
        vbox->addWidget(statusLbl);
        vbox->addSpacing(16);
    }

    // GPS data grid
    {
        auto* grid = new QGridLayout;
        grid->setSpacing(8);

        auto addField = [&](int row, int col, const QString& label, const QString& value) {
            auto* lbl = new QLabel(label);
            lbl->setStyleSheet(kLabelStyle);
            grid->addWidget(lbl, row, col * 2);
            auto* val = new QLabel(value);
            val->setStyleSheet(kValueStyle);
            grid->addWidget(val, row, col * 2 + 1);
        };

        addField(0, 0, "Latitude:",     m_model->gpsLat());
        addField(0, 1, "Longitude:",    m_model->gpsLon());
        addField(1, 0, "Grid Square:",  m_model->gpsGrid());
        addField(1, 1, "Altitude:",     m_model->gpsAltitude());
        addField(2, 0, "Sat Tracked:",  QString::number(m_model->gpsTracked()));
        addField(2, 1, "Sat Visible:",  QString::number(m_model->gpsVisible()));
        addField(3, 0, "Speed:",        m_model->gpsSpeed());
        addField(3, 1, "Freq Error:",   m_model->gpsFreqError());
        addField(4, 0, "Status:",       m_model->gpsStatus());
        addField(4, 1, "UTC Time:",     m_model->gpsTime());

        vbox->addLayout(grid);
    }

    vbox->addStretch(1);
    return page;
}
QWidget* RadioSetupDialog::buildTxTab()
{
    auto* tx = m_model->transmitModel();
    auto* page = new QWidget;
    auto* vbox = new QVBoxLayout(page);
    vbox->setSpacing(8);

    // Model header
    {
        auto* hdr = new QHBoxLayout;
        hdr->addStretch(1);
        auto* modelLbl = new QLabel(m_model->model());
        modelLbl->setStyleSheet("QLabel { color: #00c8ff; font-size: 20px; font-weight: bold; }");
        hdr->addWidget(modelLbl);
        vbox->addLayout(hdr);
    }

    // Timings group
    {
        auto* group = new QGroupBox("Timings (in ms)");
        group->setStyleSheet(kGroupStyle);
        auto* grid = new QGridLayout(group);
        grid->setSpacing(6);

        auto addTimingField = [&](int row, int col, const QString& label, int value) {
            auto* lbl = new QLabel(label);
            lbl->setStyleSheet(kLabelStyle);
            grid->addWidget(lbl, row, col * 2);
            auto* edit = new QLineEdit(QString::number(value));
            edit->setStyleSheet(kEditStyle);
            edit->setFixedWidth(60);
            grid->addWidget(edit, row, col * 2 + 1);
            return edit;
        };

        addTimingField(0, 0, "ACC TX:",  tx->accTxDelay());
        addTimingField(0, 1, "TX Delay:", tx->txDelay());
        addTimingField(1, 0, "RCA TX1:", tx->tx1Delay());
        addTimingField(1, 1, "Timeout(min):", tx->interlockTimeout());
        addTimingField(2, 0, "RCA TX2:", tx->tx2Delay());

        // TX Profile dropdown (below Timeout, right column)
        auto* profCmb = new QComboBox;
        profCmb->addItems(tx->profileList());
        profCmb->setCurrentText(tx->activeProfile());
        profCmb->setStyleSheet(kEditStyle);
        grid->addWidget(profCmb, 2, 2, 1, 2);
        connect(profCmb, &QComboBox::currentTextChanged, this, [this](const QString& name) {
            m_model->transmitModel()->loadProfile(name);
        });

        addTimingField(3, 0, "RCA TX3:", tx->tx3Delay());

        for (auto* lbl : group->findChildren<QLabel*>())
            if (lbl->styleSheet().isEmpty()) lbl->setStyleSheet(kLabelStyle);

        vbox->addWidget(group);
    }

    // Interlocks group
    {
        auto* group = new QGroupBox("Interlocks - TX REQ");
        group->setStyleSheet(kGroupStyle);
        auto* grid = new QGridLayout(group);
        grid->setSpacing(6);

        auto* rcaLbl = new QLabel("RCA:");
        rcaLbl->setStyleSheet(kLabelStyle);
        grid->addWidget(rcaLbl, 0, 0);
        auto* rcaCmb = new QComboBox;
        rcaCmb->addItems({"Active Low", "Active High"});
        rcaCmb->setCurrentIndex(tx->rcaTxReqPolarity());
        rcaCmb->setStyleSheet(kEditStyle);
        grid->addWidget(rcaCmb, 0, 1);

        auto* accLbl = new QLabel("Accessory:");
        accLbl->setStyleSheet(kLabelStyle);
        grid->addWidget(accLbl, 0, 2);
        auto* accCmb = new QComboBox;
        accCmb->addItems({"Active Low", "Active High"});
        accCmb->setCurrentIndex(tx->accTxReqPolarity());
        accCmb->setStyleSheet(kEditStyle);
        grid->addWidget(accCmb, 0, 3);

        vbox->addWidget(group);
    }

    // Max Power / Tune Mode / Show TX in Waterfall
    {
        auto* grid = new QGridLayout;
        grid->setSpacing(6);

        auto* mpLbl = new QLabel("Max Power:");
        mpLbl->setStyleSheet(kLabelStyle);
        grid->addWidget(mpLbl, 0, 0);
        auto* mpRow = new QHBoxLayout;
        auto* mpEdit = new QLineEdit(QString::number(tx->maxPowerLevel()));
        mpEdit->setStyleSheet(kEditStyle);
        mpEdit->setFixedWidth(50);
        mpRow->addWidget(mpEdit);
        auto* mpUnit = new QLabel("%");
        mpUnit->setStyleSheet(kLabelStyle);
        mpRow->addWidget(mpUnit);
        mpRow->addStretch(1);
        grid->addLayout(mpRow, 0, 1);

        connect(mpEdit, &QLineEdit::editingFinished, this, [this, mpEdit] {
            int val = qBound(0, mpEdit->text().toInt(), 100);
            mpEdit->setText(QString::number(val));
            m_model->connection()->sendCommand(
                QString("transmit set max_power_level=%1").arg(val));
        });

        auto* tmLbl = new QLabel("Tune Mode:");
        tmLbl->setStyleSheet(kLabelStyle);
        grid->addWidget(tmLbl, 1, 0);
        auto* tmCmb = new QComboBox;
        tmCmb->addItems({"Single Tone", "Two Tone"});
        tmCmb->setCurrentText(tx->tuneMode() == "single_tone" ? "Single Tone" : "Two Tone");
        tmCmb->setStyleSheet(kEditStyle);
        connect(tmCmb, &QComboBox::currentTextChanged, this, [this](const QString& text) {
            QString mode = (text == "Single Tone") ? "single_tone" : "two_tone";
            m_model->connection()->sendCommand("transmit set tune_mode=" + mode);
        });
        grid->addWidget(tmCmb, 1, 1);

        auto* swLbl = new QLabel("Show TX in Waterfall:");
        swLbl->setStyleSheet(kLabelStyle);
        grid->addWidget(swLbl, 2, 0);
        auto* swBtn = new QPushButton(tx->showTxInWaterfall() ? "Enabled" : "Disabled");
        swBtn->setCheckable(true);
        swBtn->setChecked(tx->showTxInWaterfall());
        swBtn->setStyleSheet(
            "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
            "border-radius: 3px; color: #c8d8e8; font-size: 11px; font-weight: bold; "
            "padding: 3px 10px; }"
            "QPushButton:checked { background: #1a5030; color: #00e060; "
            "border: 1px solid #20a040; }");
        connect(swBtn, &QPushButton::toggled, this, [this, swBtn](bool on) {
            swBtn->setText(on ? "Enabled" : "Disabled");
            m_model->connection()->sendCommand(
                QString("transmit set show_tx_in_waterfall=%1").arg(on ? 1 : 0));
        });
        grid->addWidget(swBtn, 2, 1);

        vbox->addLayout(grid);
    }

    vbox->addStretch(1);
    return page;
}
QWidget* RadioSetupDialog::buildPhoneCwTab()  { return placeholderTab("Phone/CW"); }
QWidget* RadioSetupDialog::buildRxTab()       { return placeholderTab("RX"); }
QWidget* RadioSetupDialog::buildFiltersTab()  { return placeholderTab("Filters"); }
QWidget* RadioSetupDialog::buildXvtrTab()     { return placeholderTab("XVTR"); }

} // namespace AetherSDR
