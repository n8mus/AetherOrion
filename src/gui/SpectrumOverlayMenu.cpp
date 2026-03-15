#include "SpectrumOverlayMenu.h"
#include "models/SliceModel.h"

#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSignalBlocker>

namespace AetherSDR {

static constexpr int BTN_W = 60;
static constexpr int BTN_H = 22;

// Band button size (slightly smaller for the grid)
static constexpr int BAND_BTN_W = 48;
static constexpr int BAND_BTN_H = 26;

static const QString kPanelStyle =
    "QWidget { background: rgba(15, 15, 26, 220); "
    "border: 1px solid #304050; border-radius: 3px; }";

static const QString kLabelStyle =
    "QLabel { background: transparent; border: none; "
    "color: #8aa8c0; font-size: 10px; font-weight: bold; }";

static const QString kComboStyle =
    "QComboBox { background: #1a2a3a; border: 1px solid #304050; "
    "border-radius: 2px; color: #c8d8e8; font-size: 11px; "
    "font-weight: bold; padding: 1px 4px; }"
    "QComboBox::drop-down { border: none; }"
    "QComboBox QAbstractItemView { background: #1a2a3a; "
    "border: 1px solid #304050; color: #c8d8e8; "
    "selection-background-color: #0070c0; }";

static const QString kSliderStyle =
    "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; "
    "border-radius: 2px; }"
    "QSlider::handle:horizontal { background: #c8d8e8; width: 10px; "
    "margin: -4px 0; border-radius: 5px; }";

static const QString kMenuBtnNormal =
    "QPushButton { background: rgba(15, 15, 26, 200); "
    "border: 1px solid #304050; border-radius: 2px; "
    "color: #c8d8e8; font-size: 11px; font-weight: bold; }"
    "QPushButton:hover { background: rgba(0, 112, 192, 180); "
    "border: 1px solid #0090e0; }";

static const QString kMenuBtnActive =
    "QPushButton { background: rgba(0, 112, 192, 180); "
    "border: 1px solid #0090e0; border-radius: 2px; "
    "color: #ffffff; font-size: 11px; font-weight: bold; }";

static QPushButton* makeMenuBtn(const QString& text, QWidget* parent)
{
    auto* btn = new QPushButton(text, parent);
    btn->setFixedSize(BTN_W, BTN_H);
    btn->setStyleSheet(kMenuBtnNormal);
    return btn;
}

// SSB center frequencies per band (ARRL band plan).
struct BandEntry {
    const char* label;
    double freqMhz;
    const char* mode;
};

static constexpr BandEntry BANDS[] = {
    {"160",   1.900, "LSB"},
    {"80",    3.800, "LSB"},
    {"60",    5.357, "USB"},
    {"40",    7.200, "LSB"},
    {"30",   10.125, "DIGU"},
    {"20",   14.225, "USB"},
    {"17",   18.130, "USB"},
    {"15",   21.300, "USB"},
    {"12",   24.950, "USB"},
    {"10",   28.400, "USB"},
    {"6",    50.150, "USB"},
    {"WWV",  10.000, "AM"},
    {"GEN",   0.500, "AM"},
    {"2200",  0.1375,"CW"},
    {"630",   0.475, "CW"},
    {"XVTR",  0.0,   ""},
};

SpectrumOverlayMenu::SpectrumOverlayMenu(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);

    // Toggle button (arrow)
    m_toggleBtn = new QPushButton(this);
    m_toggleBtn->setFixedSize(BTN_W, BTN_H);
    m_toggleBtn->setStyleSheet(
        "QPushButton { background: rgba(15, 15, 26, 200); "
        "border: 1px solid #304050; border-radius: 2px; "
        "color: #c8d8e8; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(0, 112, 192, 180); "
        "border: 1px solid #0090e0; }");
    connect(m_toggleBtn, &QPushButton::clicked, this, &SpectrumOverlayMenu::toggle);

    // Menu buttons — Band and ANT handled specially
    struct BtnDef { QString text; int specialIdx; void (SpectrumOverlayMenu::*sig)(); };
    const BtnDef defs[] = {
        {"+RX",      -1, &SpectrumOverlayMenu::addRxClicked},
        {"+TNF",     -1, &SpectrumOverlayMenu::addTnfClicked},
        {"Band",      0, nullptr},   // toggleBandPanel
        {"ANT",       1, nullptr},   // toggleAntPanel
        {"Display",  -1, &SpectrumOverlayMenu::displayClicked},
        {"DAX",      -1, &SpectrumOverlayMenu::daxClicked},
    };

    for (const auto& def : defs) {
        auto* btn = makeMenuBtn(def.text, this);
        if (def.specialIdx == 0)
            connect(btn, &QPushButton::clicked, this, &SpectrumOverlayMenu::toggleBandPanel);
        else if (def.specialIdx == 1)
            connect(btn, &QPushButton::clicked, this, &SpectrumOverlayMenu::toggleAntPanel);
        else
            connect(btn, &QPushButton::clicked, this, def.sig);
        m_menuBtns.append(btn);
    }

    buildBandPanel();
    buildAntPanel();
    updateLayout();
}

// ── Band sub-panel ────────────────────────────────────────────────────────────

void SpectrumOverlayMenu::buildBandPanel()
{
    m_bandPanel = new QWidget(parentWidget());
    m_bandPanel->setAttribute(Qt::WA_NoSystemBackground, true);
    m_bandPanel->setAutoFillBackground(false);
    m_bandPanel->hide();

    auto* grid = new QGridLayout(m_bandPanel);
    grid->setContentsMargins(2, 2, 2, 2);
    grid->setSpacing(2);

    const QString bandBtnStyle =
        "QPushButton { background: rgba(30, 40, 55, 220); "
        "border: 1px solid #304050; border-radius: 3px; "
        "color: #c8d8e8; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(0, 112, 192, 180); "
        "border: 1px solid #0090e0; }";

    constexpr int layout[][3] = {
        {0, 1, 2},      // 160, 80, 60
        {3, 4, 5},      // 40, 30, 20
        {6, 7, 8},      // 17, 15, 12
        {9, 10, -1},    // 10, 6
        {11, 12, -1},   // WWV, GEN
        {13, 14, 15},   // 2200, 630, XVTR
    };

    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 3; ++col) {
            int idx = layout[row][col];
            if (idx < 0) continue;

            auto* btn = new QPushButton(BANDS[idx].label, m_bandPanel);
            btn->setFixedSize(BAND_BTN_W, BAND_BTN_H);
            btn->setStyleSheet(bandBtnStyle);

            double freq = BANDS[idx].freqMhz;
            QString mode = QString::fromLatin1(BANDS[idx].mode);
            if (mode.isEmpty()) {
                btn->setEnabled(false);
            } else {
                connect(btn, &QPushButton::clicked, this, [this, freq, mode]() {
                    emit bandSelected(freq, mode);
                });
            }

            grid->addWidget(btn, row, col);
        }
    }

    m_bandPanel->adjustSize();
}

// ── ANT sub-panel ─────────────────────────────────────────────────────────────

void SpectrumOverlayMenu::buildAntPanel()
{
    m_antPanel = new QWidget(parentWidget());
    m_antPanel->setStyleSheet(kPanelStyle);
    m_antPanel->hide();

    auto* vbox = new QVBoxLayout(m_antPanel);
    vbox->setContentsMargins(6, 6, 6, 6);
    vbox->setSpacing(4);

    constexpr int kLabelW = 48;
    constexpr int kValueW = 20;

    // RX ANT row
    auto* antRow = new QHBoxLayout;
    antRow->setSpacing(4);
    auto* antLabel = new QLabel("RX ANT:");
    antLabel->setStyleSheet(kLabelStyle);
    antLabel->setFixedWidth(kLabelW);
    antRow->addWidget(antLabel);
    m_rxAntCmb = new QComboBox;
    m_rxAntCmb->setStyleSheet(kComboStyle);
    antRow->addWidget(m_rxAntCmb, 1);
    vbox->addLayout(antRow);

    connect(m_rxAntCmb, &QComboBox::currentTextChanged,
            this, [this](const QString& ant) {
        if (!m_updatingFromModel && m_slice && !ant.isEmpty())
            m_slice->setRxAntenna(ant);
    });

    // RF Gain row
    auto* gainRow = new QHBoxLayout;
    gainRow->setSpacing(4);
    auto* gainLabel = new QLabel("RF Gain:");
    gainLabel->setStyleSheet(kLabelStyle);
    gainLabel->setFixedWidth(kLabelW);
    gainRow->addWidget(gainLabel);
    m_rfGainSlider = new QSlider(Qt::Horizontal);
    m_rfGainSlider->setRange(-8, 32);
    m_rfGainSlider->setSingleStep(8);
    m_rfGainSlider->setPageStep(8);
    m_rfGainSlider->setTickInterval(8);
    m_rfGainSlider->setTickPosition(QSlider::TicksBelow);
    m_rfGainSlider->setStyleSheet(kSliderStyle);
    gainRow->addWidget(m_rfGainSlider, 1);
    m_rfGainLabel = new QLabel("0");
    m_rfGainLabel->setStyleSheet(kLabelStyle);
    m_rfGainLabel->setFixedWidth(kValueW);
    m_rfGainLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    gainRow->addWidget(m_rfGainLabel);
    vbox->addLayout(gainRow);

    connect(m_rfGainSlider, &QSlider::valueChanged, this, [this](int v) {
        // Snap to nearest multiple of 8
        int snapped = qRound(v / 8.0) * 8;
        if (snapped != v) {
            QSignalBlocker sb(m_rfGainSlider);
            m_rfGainSlider->setValue(snapped);
        }
        m_rfGainLabel->setText(QString::number(snapped));
        if (!m_updatingFromModel)
            emit rfGainChanged(snapped);
    });

    // WNB row: toggle button + level slider
    auto* wnbRow = new QHBoxLayout;
    wnbRow->setSpacing(4);
    m_wnbBtn = new QPushButton("WNB");
    m_wnbBtn->setCheckable(true);
    m_wnbBtn->setFixedSize(48, 22);
    m_wnbBtn->setStyleSheet(
        "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 2px; color: #c8d8e8; font-size: 11px; font-weight: bold; }"
        "QPushButton:checked { background: #0070c0; color: #ffffff; "
        "border: 1px solid #0090e0; }"
        "QPushButton:hover { border: 1px solid #0090e0; }");
    wnbRow->addWidget(m_wnbBtn);
    m_wnbSlider = new QSlider(Qt::Horizontal);
    m_wnbSlider->setRange(0, 100);
    m_wnbSlider->setValue(50);
    m_wnbSlider->setStyleSheet(kSliderStyle);
    wnbRow->addWidget(m_wnbSlider, 1);
    m_wnbLabel = new QLabel("50");
    m_wnbLabel->setStyleSheet(kLabelStyle);
    m_wnbLabel->setFixedWidth(kValueW);
    m_wnbLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    wnbRow->addWidget(m_wnbLabel);
    vbox->addLayout(wnbRow);

    connect(m_wnbBtn, &QPushButton::toggled, this, &SpectrumOverlayMenu::wnbToggled);
    connect(m_wnbSlider, &QSlider::valueChanged, this, [this](int v) {
        m_wnbLabel->setText(QString::number(v));
        emit wnbLevelChanged(v);
    });

    m_antPanel->setFixedWidth(180);
    m_antPanel->adjustSize();
}

void SpectrumOverlayMenu::setAntennaList(const QStringList& ants)
{
    m_antList = ants;
    QSignalBlocker sb(m_rxAntCmb);
    QString cur = m_rxAntCmb->currentText();
    m_rxAntCmb->clear();
    m_rxAntCmb->addItems(ants);
    if (ants.contains(cur))
        m_rxAntCmb->setCurrentText(cur);
}

void SpectrumOverlayMenu::setSlice(SliceModel* slice)
{
    if (m_slice)
        m_slice->disconnect(this);
    m_slice = slice;
    if (!m_slice) return;

    connect(m_slice, &SliceModel::rxAntennaChanged, this, [this](const QString& ant) {
        m_updatingFromModel = true;
        m_rxAntCmb->setCurrentText(ant);
        m_updatingFromModel = false;
    });

    syncAntPanel();
}

void SpectrumOverlayMenu::syncAntPanel()
{
    if (!m_slice) return;
    m_updatingFromModel = true;
    m_rxAntCmb->setCurrentText(m_slice->rxAntenna());
    m_updatingFromModel = false;
}

// ── Sub-panel toggle helpers ──────────────────────────────────────────────────

void SpectrumOverlayMenu::hideAllSubPanels()
{
    if (m_bandPanelVisible) { m_bandPanelVisible = false; m_bandPanel->hide(); }
    if (m_antPanelVisible)  { m_antPanelVisible = false;  m_antPanel->hide(); }
    m_menuBtns[2]->setStyleSheet(kMenuBtnNormal);
    m_menuBtns[3]->setStyleSheet(kMenuBtnNormal);
}

void SpectrumOverlayMenu::toggleBandPanel()
{
    bool wasVisible = m_bandPanelVisible;
    hideAllSubPanels();
    if (!wasVisible) {
        m_bandPanelVisible = true;
        m_bandPanel->move(x() + width(), y());
        m_bandPanel->raise();
        m_bandPanel->show();
        m_menuBtns[2]->setStyleSheet(kMenuBtnActive);
    }
}

void SpectrumOverlayMenu::toggleAntPanel()
{
    bool wasVisible = m_antPanelVisible;
    hideAllSubPanels();
    if (!wasVisible) {
        syncAntPanel();
        m_antPanelVisible = true;
        // Center vertically on the ANT button (index 3)
        int antBtnCenterY = m_menuBtns[3]->y() + m_menuBtns[3]->height() / 2;
        int panelY = y() + antBtnCenterY - m_antPanel->sizeHint().height() / 2;
        m_antPanel->move(x() + width(), std::max(0, panelY));
        m_antPanel->raise();
        m_antPanel->show();
        m_menuBtns[3]->setStyleSheet(kMenuBtnActive);
    }
}

// ── Main menu toggle and layout ───────────────────────────────────────────────

void SpectrumOverlayMenu::toggle()
{
    m_expanded = !m_expanded;
    if (!m_expanded)
        hideAllSubPanels();
    updateLayout();
}

void SpectrumOverlayMenu::updateLayout()
{
    constexpr int pad = 2;
    constexpr int gap = 2;

    m_toggleBtn->setText(m_expanded ? QStringLiteral("\u2190") : QStringLiteral("\u2192"));
    m_toggleBtn->move(pad, pad);

    int y = pad + BTN_H + gap;
    for (auto* btn : m_menuBtns) {
        btn->setVisible(m_expanded);
        if (m_expanded) {
            btn->move(pad, y);
            y += BTN_H + gap;
        }
    }

    int totalH = m_expanded ? (pad + BTN_H + gap + m_menuBtns.size() * (BTN_H + gap))
                            : (pad + BTN_H + pad);
    setFixedSize(pad + BTN_W + pad, totalH);
}

} // namespace AetherSDR
