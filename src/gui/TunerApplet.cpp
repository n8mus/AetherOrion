#include "TunerApplet.h"
#include "HGauge.h"
#include "models/TunerModel.h"
#include "models/MeterModel.h"

#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QTimer>

namespace AetherSDR {

// ── Shared gradient title bar (matches AppletPanel style) ───────────────────

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

// ── TunerApplet ─────────────────────────────────────────────────────────────

TunerApplet::TunerApplet(QWidget* parent)
    : QWidget(parent)
{
    hide();   // hidden by default until toggled on
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    buildUI();
}

void TunerApplet::setAmplifierMode(bool hasAmp)
{
    setPowerScale(100, hasAmp);
}

void TunerApplet::setPowerScale(int maxWatts, bool hasAmplifier)
{
    auto* gauge = static_cast<HGauge*>(m_fwdGauge);
    if (hasAmplifier) {
        // PGXL: 0–2000 W, red > 1500 W
        gauge->setRange(0.0f, 2000.0f, 1500.0f,
            {{0, "0"}, {500, "500"}, {1500, "1.5k"}, {2000, "2k"}});
    } else if (maxWatts > 100) {
        // Aurora (500 W): 0–600 W, red > 500 W
        gauge->setRange(0.0f, 600.0f, 500.0f,
            {{0, "0"}, {100, "100"}, {200, "200"}, {300, "300"},
             {400, "400"}, {500, "500"}, {600, "600"}});
    } else {
        // Barefoot radio: 0–200 W, red > 125 W
        gauge->setRange(0.0f, 200.0f, 125.0f,
            {{0, "0"}, {50, "50"}, {100, "100"}, {150, "150"}, {200, "200"}});
    }
}

void TunerApplet::buildUI()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // Title bar
    outer->addWidget(appletTitleBar("TUNER GENIUS XL"));

    // Body with margins
    auto* body = new QWidget;
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 2);
    vbox->setSpacing(2);

    // Forward Power gauge — default barefoot (0–200 W); switches to
    // 0–2000 W if a PGXL amplifier is detected via setAmplifierMode().
    m_fwdGauge = new HGauge(0.0f, 200.0f, 125.0f, "Fwd Pwr", "W",
        {{0, "0"}, {50, "50"}, {100, "100"}, {150, "150"}, {200, "200"}});
    vbox->addWidget(m_fwdGauge);

    // SWR gauge
    m_swrGauge = new HGauge(1.0f, 3.0f, 2.5f, "SWR", "",
        {{1.0f, "1"}, {1.5f, "1.5"}, {2.5f, "2.5"}, {3.0f, "3"}});
    vbox->addWidget(m_swrGauge);

    // Bottom section: relay bars (75% left) + buttons (25% right)
    auto* bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(4);

    // Left column: relay bars
    auto* relayCol = new QVBoxLayout;
    relayCol->setSpacing(2);
    m_c1Bar = new RelayBar("C1");
    m_lBar  = new RelayBar("L");
    m_c2Bar = new RelayBar("C2");
    relayCol->addWidget(m_c1Bar);
    relayCol->addWidget(m_lBar);
    relayCol->addWidget(m_c2Bar);
    bottomRow->addLayout(relayCol, 7);  // stretch 7 (70%)

    // Right column: buttons
    auto* btnCol = new QVBoxLayout;
    btnCol->setSpacing(2);

    m_tuneBtn = new QPushButton("TUNE");
    m_tuneBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_tuneBtn->setStyleSheet(
        "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
        "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
        "QPushButton:hover { background: #204060; }");
    btnCol->addWidget(m_tuneBtn);

    m_operateBtn = new QPushButton("OPERATE");
    m_operateBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_operateBtn->setStyleSheet(
        "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
        "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
        "QPushButton:hover { background: #204060; }");
    btnCol->addWidget(m_operateBtn);

    bottomRow->addLayout(btnCol, 3);  // stretch 3 (30%)

    vbox->addLayout(bottomRow);
    outer->addWidget(body);

    // TUNE button: send autotune command
    connect(m_tuneBtn, &QPushButton::clicked, this, [this]() {
        if (m_model) m_model->autoTune();
    });

    // OPERATE button: cycle through OPERATE → BYPASS → STANDBY → OPERATE
    connect(m_operateBtn, &QPushButton::clicked, this,
            &TunerApplet::cycleOperateState);
}

void TunerApplet::setTunerModel(TunerModel* model)
{
    if (m_model == model) return;
    m_model = model;
    if (!m_model) return;

    // State changes → refresh UI
    connect(m_model, &TunerModel::stateChanged, this, &TunerApplet::syncFromModel);

    // Tuning state changes → red button + SWR result flash
    connect(m_model, &TunerModel::tuningChanged, this, [this](bool tuning) {
        if (tuning) {
            m_wasTuning = true;
            m_tuneSwr = 999.0f;  // reset high so minimum tracking works
            m_tuneBtn->setStyleSheet(
                "QPushButton { background: #cc2222; border: 1px solid #ff4444; "
                "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }");
            m_tuneBtn->setText("TUNING...");
        } else {
            // Restore normal style
            m_tuneBtn->setStyleSheet(
                "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
                "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
                "QPushButton:hover { background: #204060; }");

            // Show the minimum SWR captured during tuning
            if (m_wasTuning) {
                m_wasTuning = false;
                float result = (m_tuneSwr < 900.0f) ? m_tuneSwr : m_swr;
                m_tuneBtn->setText(QString("SWR %1").arg(result, 0, 'f', 2));
                QTimer::singleShot(2500, this, [this]() {
                    m_tuneBtn->setText("TUNE");
                });
            } else {
                m_tuneBtn->setText("TUNE");
            }
        }
    });

    syncFromModel();
}

void TunerApplet::syncFromModel()
{
    if (!m_model) return;

    // Relay bars
    m_relayC1 = m_model->relayC1();
    m_relayL  = m_model->relayL();
    m_relayC2 = m_model->relayC2();
    static_cast<RelayBar*>(m_c1Bar)->setValue(m_relayC1);
    static_cast<RelayBar*>(m_lBar)->setValue(m_relayL);
    static_cast<RelayBar*>(m_c2Bar)->setValue(m_relayC2);

    // Operate/Bypass/Standby button — 3-state display
    // operate=1, bypass=0 → OPERATE (green)
    // operate=1, bypass=1 → BYPASS  (orange)
    // operate=0            → STANDBY (default)
    if (m_model->isOperate() && !m_model->isBypass()) {
        m_operateBtn->setText("OPERATE");
        m_operateBtn->setStyleSheet(
            "QPushButton { background: #006030; border: 1px solid #008040; "
            "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }"
            "QPushButton:hover { background: #007040; }");
    } else if (m_model->isOperate() && m_model->isBypass()) {
        m_operateBtn->setText("BYPASS");
        m_operateBtn->setStyleSheet(
            "QPushButton { background: #8a6000; border: 1px solid #a07000; "
            "border-radius: 3px; color: #ffffff; font-size: 10px; font-weight: bold; }"
            "QPushButton:hover { background: #9a7000; }");
    } else {
        m_operateBtn->setText("STANDBY");
        m_operateBtn->setStyleSheet(
            "QPushButton { background: #1a3a5a; border: 1px solid #205070; "
            "border-radius: 3px; color: #c8d8e8; font-size: 10px; font-weight: bold; }"
            "QPushButton:hover { background: #204060; }");
    }
}

void TunerApplet::cycleOperateState()
{
    if (!m_model) return;

    // Cycle: OPERATE → BYPASS → STANDBY → OPERATE
    if (m_model->isOperate() && !m_model->isBypass()) {
        // Currently OPERATE → go to BYPASS
        m_model->setBypass(true);
    } else if (m_model->isOperate() && m_model->isBypass()) {
        // Currently BYPASS → go to STANDBY
        m_model->setOperate(false);
    } else {
        // Currently STANDBY → go to OPERATE
        m_model->setBypass(false);
        m_model->setOperate(true);
    }
}

void TunerApplet::updateMeters(float fwdPower, float swr)
{
    m_fwdPower = fwdPower;
    m_swr = swr;
    static_cast<HGauge*>(m_fwdGauge)->setValue(fwdPower);
    static_cast<HGauge*>(m_swrGauge)->setValue(swr);

    // While tuning, track the minimum non-idle SWR — the tuner is minimizing
    // SWR, so the lowest value seen during the search is the result.
    // Once TX stops, SWR drops to ~1.00 (no RF), so we ignore idle values.
    if (m_wasTuning && swr > 1.01f) {
        if (swr < m_tuneSwr)
            m_tuneSwr = swr;
        m_tuneBtn->setText(QString("SWR %1").arg(swr, 0, 'f', 2));
    }
}

} // namespace AetherSDR
