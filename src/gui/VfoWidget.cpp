#include "VfoWidget.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QPainter>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMenu>
#include <QSignalBlocker>
#include <QEvent>
#include <cmath>

namespace AetherSDR {

// ── Styles ────────────────────────────────────────────────────────────────────

// Background is painted manually in paintEvent for true alpha transparency.
static const QString kBgStyle =
    "QWidget#VfoWidgetRoot { background: transparent; border: none; }";

static const QString kFlatBtn =
    "QPushButton { background: transparent; border: none; "
    "font-size: 13px; font-weight: bold; padding: 0 6px; margin: 0; }";

static const QString kTabLblNormal =
    "QLabel { background: transparent; border: none; "
    "border-bottom: 2px solid transparent; "
    "color: #6888a0; font-size: 13px; font-weight: bold; padding: 3px 0; }";

static const QString kTabLblActive =
    "QLabel { background: transparent; border: none; "
    "border-bottom: 2px solid #00b4d8; "
    "color: #00b4d8; font-size: 13px; font-weight: bold; padding: 3px 0; }";

static const QString kDspToggle =
    "QPushButton { background: #1a2a3a; border: 1px solid #304050; border-radius: 2px; "
    "color: #c8d8e8; font-size: 13px; font-weight: bold; padding: 2px 4px; }"
    "QPushButton:checked { background: #1a6030; color: #ffffff; border: 1px solid #20a040; }"
    "QPushButton:hover { border: 1px solid #0090e0; }";

static const QString kModeBtn =
    "QPushButton { background: #1a2a3a; border: 1px solid #304050; border-radius: 2px; "
    "color: #c8d8e8; font-size: 13px; font-weight: bold; padding: 3px; }"
    "QPushButton:checked { background: #0070c0; color: #ffffff; border: 1px solid #0090e0; }"
    "QPushButton:hover { border: 1px solid #0090e0; }";

static const QString kSliderStyle =
    "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
    "QSlider::handle:horizontal { background: #c8d8e8; width: 12px; margin: -4px 0; border-radius: 6px; }";

static const QString kLabelStyle =
    "QLabel { background: transparent; border: none; color: #8aa8c0; font-size: 13px; }";

// ── Construction ──────────────────────────────────────────────────────────────

VfoWidget::VfoWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("VfoWidgetRoot");
    setFixedWidth(WIDGET_W);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setStyleSheet(kBgStyle);
    buildUI();
}

void VfoWidget::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 2, 6, 0);
    root->setSpacing(2);

    // ── Header row: ANT1(rx) ANT1(tx) 3.8K  SPLIT TX ──────────────────────
    auto* hdr = new QHBoxLayout;
    hdr->setSpacing(2);
    hdr->setAlignment(Qt::AlignTop);

    m_rxAntBtn = new QPushButton("ANT1");
    m_rxAntBtn->setFlat(true);
    m_rxAntBtn->setStyleSheet(kFlatBtn + "QPushButton { color: #4488ff; }");
    connect(m_rxAntBtn, &QPushButton::clicked, this, [this] {
        if (!m_slice) return;
        QMenu menu(this);
        for (const QString& ant : m_antList) {
            auto* act = menu.addAction(ant);
            act->setCheckable(true);
            act->setChecked(ant == m_slice->rxAntenna());
        }
        if (auto* sel = menu.exec(m_rxAntBtn->mapToGlobal(QPoint(0, m_rxAntBtn->height()))))
            m_slice->setRxAntenna(sel->text());
    });
    hdr->addWidget(m_rxAntBtn);

    m_txAntBtn = new QPushButton("ANT1");
    m_txAntBtn->setFlat(true);
    m_txAntBtn->setStyleSheet(kFlatBtn + "QPushButton { color: #ff4444; }");
    connect(m_txAntBtn, &QPushButton::clicked, this, [this] {
        if (!m_slice) return;
        QMenu menu(this);
        for (const QString& ant : m_antList) {
            auto* act = menu.addAction(ant);
            act->setCheckable(true);
            act->setChecked(ant == m_slice->txAntenna());
        }
        if (auto* sel = menu.exec(m_txAntBtn->mapToGlobal(QPoint(0, m_txAntBtn->height()))))
            m_slice->setTxAntenna(sel->text());
    });
    hdr->addWidget(m_txAntBtn);

    m_filterWidthLbl = new QLabel("2.7K");
    m_filterWidthLbl->setStyleSheet("QLabel { background: transparent; border: none; "
                                     "color: #00c8ff; font-size: 13px; font-weight: bold; "
                                     "margin: 0; padding: 0; }");
    hdr->addWidget(m_filterWidthLbl);

    hdr->addStretch(1);

    m_splitBadge = new QLabel("SPLIT");
    m_splitBadge->setStyleSheet("QLabel { background: transparent; border: none; "
                                 "color: #ffb800; font-size: 12px; font-weight: bold; }");
    m_splitBadge->hide();
    hdr->addWidget(m_splitBadge);

    m_txBadge = new QPushButton("TX");
    m_txBadge->setFixedSize(28, 20);
    m_txBadge->setStyleSheet(
        "QPushButton { background: #cc0000; color: #ffffff; border: none; "
        "border-radius: 2px; font-size: 12px; font-weight: bold; padding: 0; }"
        "QPushButton:hover { background: #ff2222; }");
    m_txBadge->hide();
    hdr->addWidget(m_txBadge);

    m_sliceBadge = new QLabel("A");
    m_sliceBadge->setFixedSize(20, 20);
    m_sliceBadge->setAlignment(Qt::AlignCenter);
    m_sliceBadge->setStyleSheet(
        "QLabel { background: #0070c0; color: #ffffff; "
        "border-radius: 3px; font-weight: bold; font-size: 11px; }");
    hdr->addWidget(m_sliceBadge);

    root->addLayout(hdr);

    // ── Frequency row (right-aligned) ─────────────────────────────────────
    m_freqLabel = new QLabel("14.225.000");
    m_freqLabel->setStyleSheet("QLabel { background: transparent; border: none; "
                                "color: #c8d8e8; font-size: 24px; font-weight: bold; }");
    m_freqLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    root->addWidget(m_freqLabel);

    // ── S-meter + dBm row (75/25 split) ────────────────────────────────────
    // S-meter bar is painted in paintEvent; spacer reserves its space.
    // dBm label sits to the right.
    auto* meterRow = new QHBoxLayout;
    meterRow->setSpacing(4);

    auto* sMeterSpacer = new QWidget;
    sMeterSpacer->setFixedHeight(22);
    sMeterSpacer->setAttribute(Qt::WA_TranslucentBackground);
    sMeterSpacer->setStyleSheet("QWidget { background: transparent; }");
    meterRow->addWidget(sMeterSpacer, 3);  // 75%

    m_dbmLabel = new QLabel("-95 dBm");
    m_dbmLabel->setStyleSheet("QLabel { background: transparent; border: none; "
                               "color: #6888a0; font-size: 11px; }");
    m_dbmLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    meterRow->addWidget(m_dbmLabel, 1);    // 25%

    root->addLayout(meterRow);

    // ── Tab bar ────────────────────────────────────────────────────────────
    m_tabBar = new QWidget;
    m_tabBar->setAttribute(Qt::WA_TranslucentBackground);
    m_tabBar->setStyleSheet("QWidget { background: transparent; }");
    auto* tabLayout = new QHBoxLayout(m_tabBar);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);

    const QStringList tabLabels = {"\xF0\x9F\x94\x8A", "DSP", "USB", "X/RIT", "DAX"};
    for (int i = 0; i < tabLabels.size(); ++i) {
        if (i > 0) {
            auto* sep = new QLabel("|");
            sep->setStyleSheet("QLabel { background: transparent; border: none; "
                               "color: rgba(255, 255, 255, 192); font-size: 13px; padding: 0; }");
            sep->setFixedWidth(6);
            sep->setAlignment(Qt::AlignCenter);
            tabLayout->addWidget(sep);
        }
        auto* lbl = new QLabel(tabLabels[i]);
        lbl->setStyleSheet(kTabLblNormal);
        lbl->setFixedHeight(24);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setCursor(Qt::PointingHandCursor);
        lbl->installEventFilter(this);
        tabLayout->addWidget(lbl, 1);
        m_tabBtns.append(lbl);
    }
    root->addWidget(m_tabBar);

    // ── Tab content (stacked) ──────────────────────────────────────────────
    m_tabStack = new QStackedWidget;
    m_tabStack->hide();
    buildTabContent();
    root->addWidget(m_tabStack);

    adjustSize();
}

// ── Tab content ───────────────────────────────────────────────────────────────

void VfoWidget::buildTabContent()
{
    // Tab 0: Audio
    {
        auto* m_audioTab = new QWidget;
        auto* vb = new QVBoxLayout(m_audioTab);
        vb->setContentsMargins(2, 2, 2, 2);
        vb->setSpacing(2);

        auto* gainRow = new QHBoxLayout;
        gainRow->setSpacing(3);
        auto* gainLbl = new QLabel("AF");
        gainLbl->setStyleSheet(kLabelStyle);
        gainLbl->setFixedWidth(26);
        gainRow->addWidget(gainLbl);
        m_afGainSlider = new QSlider(Qt::Horizontal);
        m_afGainSlider->setRange(0, 100);
        m_afGainSlider->setStyleSheet(kSliderStyle);
        gainRow->addWidget(m_afGainSlider, 1);
        vb->addLayout(gainRow);

        auto* panRow = new QHBoxLayout;
        panRow->setSpacing(3);
        auto* panLbl = new QLabel("Pan");
        panLbl->setStyleSheet(kLabelStyle);
        panLbl->setFixedWidth(26);
        panRow->addWidget(panLbl);
        m_panSlider = new QSlider(Qt::Horizontal);
        m_panSlider->setRange(0, 100);
        m_panSlider->setValue(50);
        m_panSlider->setStyleSheet(kSliderStyle);
        panRow->addWidget(m_panSlider, 1);
        m_muteBtn = new QPushButton("\xF0\x9F\x94\x87");
        m_muteBtn->setCheckable(true);
        m_muteBtn->setFixedSize(30, 24);
        m_muteBtn->setStyleSheet(kDspToggle);
        panRow->addWidget(m_muteBtn);
        vb->addLayout(panRow);

        connect(m_afGainSlider, &QSlider::valueChanged, this, [this](int v) {
            if (!m_updatingFromModel && m_slice) m_slice->setAudioGain(v);
        });
        connect(m_panSlider, &QSlider::valueChanged, this, [this](int v) {
            if (!m_updatingFromModel && m_slice) m_slice->setAudioPan(v);
        });
        connect(m_muteBtn, &QPushButton::toggled, this, [this](bool on) {
            if (!m_updatingFromModel && m_slice) m_slice->setAudioMute(on);
        });

        m_tabStack->addWidget(m_audioTab);
    }

    // Tab 1: DSP
    {
        auto* m_dspTab = new QWidget;
        auto* vb = new QVBoxLayout(m_dspTab);
        vb->setContentsMargins(2, 2, 2, 2);
        vb->setSpacing(2);

        auto* row1 = new QHBoxLayout;
        row1->setSpacing(2);
        auto makeDsp = [&](const QString& text) {
            auto* b = new QPushButton(text);
            b->setCheckable(true);
            b->setFixedHeight(24);
            b->setStyleSheet(kDspToggle);
            return b;
        };
        m_nbBtn  = makeDsp("NB");  row1->addWidget(m_nbBtn);
        m_nrBtn  = makeDsp("NR");  row1->addWidget(m_nrBtn);
        m_anfBtn = makeDsp("ANF"); row1->addWidget(m_anfBtn);
        vb->addLayout(row1);

        auto* row2 = new QHBoxLayout;
        row2->setSpacing(2);
        m_nrlBtn = makeDsp("NRL"); row2->addWidget(m_nrlBtn);
        m_nrsBtn = makeDsp("NRS"); row2->addWidget(m_nrsBtn);
        m_rnnBtn = makeDsp("RNN"); row2->addWidget(m_rnnBtn);
        vb->addLayout(row2);

        connect(m_nbBtn,  &QPushButton::toggled, this, [this](bool on) { if (!m_updatingFromModel && m_slice) m_slice->setNb(on); });
        connect(m_nrBtn,  &QPushButton::toggled, this, [this](bool on) { if (!m_updatingFromModel && m_slice) m_slice->setNr(on); });
        connect(m_anfBtn, &QPushButton::toggled, this, [this](bool on) { if (!m_updatingFromModel && m_slice) m_slice->setAnf(on); });
        connect(m_nrlBtn, &QPushButton::toggled, this, [this](bool on) { if (!m_updatingFromModel && m_slice) m_slice->setNrl(on); });
        connect(m_nrsBtn, &QPushButton::toggled, this, [this](bool on) { if (!m_updatingFromModel && m_slice) m_slice->setNrs(on); });
        connect(m_rnnBtn, &QPushButton::toggled, this, [this](bool on) { if (!m_updatingFromModel && m_slice) m_slice->setRnn(on); });

        m_tabStack->addWidget(m_dspTab);
    }

    // Tab 2: Mode
    {
        auto* m_modeTab = new QWidget;
        auto* grid = new QGridLayout(m_modeTab);
        grid->setContentsMargins(2, 2, 2, 2);
        grid->setSpacing(2);

        const QStringList modes = {"USB", "LSB", "CW", "AM", "SAM", "FM",
                                    "NFM", "DFM", "DIGU", "DIGL", "RTTY"};
        for (int i = 0; i < modes.size(); ++i) {
            auto* btn = new QPushButton(modes[i]);
            btn->setCheckable(true);
            btn->setFixedHeight(24);
            btn->setStyleSheet(kModeBtn);
            connect(btn, &QPushButton::clicked, this, [this, mode = modes[i]] {
                if (m_slice) m_slice->setMode(mode);
            });
            grid->addWidget(btn, i / 4, i % 4);
            m_modeBtns.append(btn);
        }

        m_tabStack->addWidget(m_modeTab);
    }

    // Tab 3: X/RIT
    {
        auto* ritTab = new QWidget;
        auto* vb = new QVBoxLayout(ritTab);
        vb->setContentsMargins(2, 2, 2, 2);
        vb->setSpacing(2);

        // RIT row
        auto* ritRow = new QHBoxLayout;
        ritRow->setSpacing(3);
        m_ritBtn = new QPushButton("RIT");
        m_ritBtn->setCheckable(true);
        m_ritBtn->setFixedHeight(24);
        m_ritBtn->setStyleSheet(kDspToggle);
        ritRow->addWidget(m_ritBtn);
        m_ritLabel = new QLabel("+0 Hz");
        m_ritLabel->setStyleSheet("QLabel { background: transparent; border: none; "
                                   "color: #c8d8e8; font-size: 13px; }");
        m_ritLabel->setAlignment(Qt::AlignCenter);
        ritRow->addWidget(m_ritLabel, 1);
        vb->addLayout(ritRow);

        // XIT row
        auto* xitRow = new QHBoxLayout;
        xitRow->setSpacing(3);
        m_xitBtn = new QPushButton("XIT");
        m_xitBtn->setCheckable(true);
        m_xitBtn->setFixedHeight(24);
        m_xitBtn->setStyleSheet(kDspToggle);
        xitRow->addWidget(m_xitBtn);
        m_xitLabel = new QLabel("+0 Hz");
        m_xitLabel->setStyleSheet("QLabel { background: transparent; border: none; "
                                   "color: #c8d8e8; font-size: 13px; }");
        m_xitLabel->setAlignment(Qt::AlignCenter);
        xitRow->addWidget(m_xitLabel, 1);
        vb->addLayout(xitRow);

        connect(m_ritBtn, &QPushButton::toggled, this, [this](bool on) {
            if (!m_updatingFromModel && m_slice) m_slice->setRit(on, m_slice->ritFreq());
        });
        connect(m_xitBtn, &QPushButton::toggled, this, [this](bool on) {
            if (!m_updatingFromModel && m_slice) m_slice->setXit(on, m_slice->xitFreq());
        });

        m_tabStack->addWidget(ritTab);
    }

    // Tab 4: DAX
    {
        auto* daxTab = new QWidget;
        auto* vb = new QVBoxLayout(daxTab);
        vb->setContentsMargins(2, 2, 2, 2);
        vb->setSpacing(2);

        auto* row = new QHBoxLayout;
        row->setSpacing(3);
        auto* lbl = new QLabel("DAX Ch");
        lbl->setStyleSheet(kLabelStyle);
        row->addWidget(lbl);
        m_daxCmb = new QComboBox;
        m_daxCmb->addItems({"Off", "1", "2", "3", "4"});
        m_daxCmb->setStyleSheet(
            "QComboBox { background: #1a2a3a; border: 1px solid #304050; "
            "border-radius: 2px; color: #c8d8e8; font-size: 9px; padding: 1px 4px; }"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView { background: #1a2a3a; border: 1px solid #304050; "
            "color: #c8d8e8; selection-background-color: #0070c0; }");
        row->addWidget(m_daxCmb, 1);
        vb->addLayout(row);

        m_tabStack->addWidget(daxTab);
    }
}

// ── Tab switching ─────────────────────────────────────────────────────────────

void VfoWidget::showTab(int index)
{
    if (m_activeTab == index) {
        // Toggle off — collapse content
        m_tabStack->hide();
        m_tabBtns[m_activeTab]->setStyleSheet(kTabLblNormal);
        m_activeTab = -1;
    } else {
        if (m_activeTab >= 0)
            m_tabBtns[m_activeTab]->setStyleSheet(kTabLblNormal);
        m_activeTab = index;
        m_tabBtns[index]->setStyleSheet(kTabLblActive);
        m_tabStack->setCurrentIndex(index);
        m_tabStack->show();
    }
    adjustSize();
}

// ── Positioning ───────────────────────────────────────────────────────────────

void VfoWidget::updatePosition(int vfoX, int specTop)
{
    const int w = width();
    int x = vfoX - w;  // default: left of VFO marker

    // Flip to right side if widget would be clipped on the left
    if (x < 0)
        x = vfoX;

    // Let the widget leave the screen naturally on both sides
    move(x, specTop);
}

// ── S-Meter bar (custom paint) ────────────────────────────────────────────────

void VfoWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Semi-transparent white background with rounded corners
    p.setPen(QColor(255, 255, 255, 13));
    p.setBrush(QColor(255, 255, 255, 13));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 3, 3);

    p.setRenderHint(QPainter::Antialiasing, false);

    // Bar rect: drawn in the S-meter row (75% left portion)
    const int barX = 6;
    const int barW = (width() - 12) * 3 / 4;  // 75% of widget width
    const int barY = m_dbmLabel->geometry().center().y() - 3;
    const int barH = 6;

    // Background
    p.fillRect(barX, barY, barW, barH, QColor(0x10, 0x18, 0x20));

    // S-meter scale: S0=-127, S9=-73 (6 dB per S-unit), S9+60=-13
    // S0–S9 occupies left 60%, S9–S9+60 occupies right 40%
    constexpr float S0_DBM  = -127.0f;
    constexpr float S9_DBM  = -73.0f;
    constexpr float S9P60   = -13.0f;
    const int s9X = barX + barW * 60 / 100;  // S9 boundary pixel

    // Signal fill — blue up to S9, red beyond
    float frac;
    if (m_signalDbm <= S0_DBM) {
        frac = 0.0f;
    } else if (m_signalDbm <= S9_DBM) {
        frac = (m_signalDbm - S0_DBM) / (S9_DBM - S0_DBM) * 0.6f;
    } else if (m_signalDbm <= S9P60) {
        frac = 0.6f + (m_signalDbm - S9_DBM) / (S9P60 - S9_DBM) * 0.4f;
    } else {
        frac = 1.0f;
    }
    int fillW = static_cast<int>(frac * barW);

    if (fillW > 0) {
        p.fillRect(barX, barY, fillW, barH, QColor(0x00, 0xc0, 0x40));
    }

    // ── Scale bar with tick marks below the S-meter ────────────────────────
    const int scaleY = barY + barH + 2;
    const int tickH  = 3;

    // Horizontal line: blue from start to S9, red from S9 to end
    p.setPen(QColor(0x30, 0x80, 0xff));
    p.drawLine(barX, scaleY, s9X, scaleY);
    p.setPen(QColor(0xd0, 0x20, 0x20));
    p.drawLine(s9X, scaleY, barX + barW, scaleY);

    p.setPen(QColor(0x30, 0x80, 0xff));  // blue for S1–S9

    // S-unit ticks: S1, S3, S5, S7, S9 — S1 at left edge, S9 at 60%
    for (int s = 1; s <= 9; s += 2) {
        float sf = static_cast<float>(s - 1) / 8.0f * 0.6f;
        int tx = barX + static_cast<int>(sf * barW);
        int h = (s == 9) ? tickH + 1 : tickH;
        p.drawLine(tx, scaleY, tx, scaleY + h);
    }

    p.setPen(QColor(0xd0, 0x20, 0x20));  // red for +20, +40

    // +20 tick
    {
        float sf = 0.6f + (20.0f / 60.0f) * 0.4f;
        int tx = barX + static_cast<int>(sf * barW);
        p.drawLine(tx, scaleY, tx, scaleY + tickH);
    }
    // +40 tick
    {
        float sf = 0.6f + (40.0f / 60.0f) * 0.4f;
        int tx = barX + static_cast<int>(sf * barW);
        p.drawLine(tx, scaleY, tx, scaleY + tickH);
    }

    // Scale labels
    QFont scaleFont = p.font();
    scaleFont.setPixelSize(7);
    scaleFont.setBold(true);
    p.setFont(scaleFont);

    const int lblY = scaleY + tickH + 7;

    // Blue labels: 1, 3, 5, 7, 9 — S1 at left edge, S9 at 60%
    p.setPen(QColor(0x30, 0x80, 0xff));
    for (int s : {1, 3, 5, 7, 9}) {
        float sf = static_cast<float>(s - 1) / 8.0f * 0.6f;
        int tx = barX + static_cast<int>(sf * barW);
        p.drawText(tx - 3, lblY, QString::number(s));
    }

    // Red labels: +20, +40
    p.setPen(QColor(0xd0, 0x20, 0x20));
    {
        float sf = 0.6f + (20.0f / 60.0f) * 0.4f;
        int tx = barX + static_cast<int>(sf * barW);
        p.drawText(tx - 6, lblY, "+20");
    }
    {
        float sf = 0.6f + (40.0f / 60.0f) * 0.4f;
        int tx = barX + static_cast<int>(sf * barW);
        p.drawText(tx - 6, lblY, "+40");
    }
}

// ── Signal level ──────────────────────────────────────────────────────────────

void VfoWidget::setSignalLevel(float dbm)
{
    m_signalDbm = dbm;
    m_dbmLabel->setText(QString("%1 dBm").arg(static_cast<int>(dbm)));
    update();  // repaint S-meter bar
}

// ── Slice connection ──────────────────────────────────────────────────────────

void VfoWidget::setSlice(SliceModel* slice)
{
    if (m_slice)
        m_slice->disconnect(this);
    m_slice = slice;
    if (!m_slice) return;

    // Frequency
    connect(m_slice, &SliceModel::frequencyChanged, this, [this](double) { updateFreqLabel(); });
    // Mode
    connect(m_slice, &SliceModel::modeChanged, this, [this](const QString& mode) {
        m_tabBtns[2]->setText(mode);  // update mode tab label
        updateModeTab();
    });
    // Filter
    connect(m_slice, &SliceModel::filterChanged, this, [this](int, int) { updateFilterLabel(); });
    // Antennas
    connect(m_slice, &SliceModel::rxAntennaChanged, this, [this](const QString& ant) {
        m_updatingFromModel = true; m_rxAntBtn->setText(ant); m_updatingFromModel = false;
    });
    connect(m_slice, &SliceModel::txAntennaChanged, this, [this](const QString& ant) {
        m_updatingFromModel = true; m_txAntBtn->setText(ant); m_updatingFromModel = false;
    });
    // TX slice
    connect(m_slice, &SliceModel::txSliceChanged, this, [this](bool tx) {
        m_txBadge->setVisible(tx);
    });
    // Audio
    connect(m_slice, &SliceModel::audioMuteChanged, this, [this](bool mute) {
        m_updatingFromModel = true;
        QSignalBlocker sb(m_muteBtn);
        m_muteBtn->setChecked(mute);
        m_updatingFromModel = false;
    });
    // DSP toggles
    auto connectDsp = [this](auto signal, QPushButton* btn) {
        connect(m_slice, signal, this, [this, btn](bool on) {
            m_updatingFromModel = true;
            QSignalBlocker sb(btn);
            btn->setChecked(on);
            m_updatingFromModel = false;
        });
    };
    connectDsp(&SliceModel::nbChanged,  m_nbBtn);
    connectDsp(&SliceModel::nrChanged,  m_nrBtn);
    connectDsp(&SliceModel::anfChanged, m_anfBtn);
    connectDsp(&SliceModel::nrlChanged, m_nrlBtn);
    connectDsp(&SliceModel::nrsChanged, m_nrsBtn);
    connectDsp(&SliceModel::rnnChanged, m_rnnBtn);
    // RIT/XIT
    connect(m_slice, &SliceModel::ritChanged, this, [this](bool on, int hz) {
        m_updatingFromModel = true;
        QSignalBlocker sb(m_ritBtn);
        m_ritBtn->setChecked(on);
        m_ritLabel->setText(QString("%1%2 Hz").arg(hz >= 0 ? "+" : "").arg(hz));
        m_updatingFromModel = false;
    });
    connect(m_slice, &SliceModel::xitChanged, this, [this](bool on, int hz) {
        m_updatingFromModel = true;
        QSignalBlocker sb(m_xitBtn);
        m_xitBtn->setChecked(on);
        m_xitLabel->setText(QString("%1%2 Hz").arg(hz >= 0 ? "+" : "").arg(hz));
        m_updatingFromModel = false;
    });

    syncFromSlice();
}

void VfoWidget::syncFromSlice()
{
    if (!m_slice) return;
    m_updatingFromModel = true;

    m_rxAntBtn->setText(m_slice->rxAntenna());
    m_txAntBtn->setText(m_slice->txAntenna());
    m_txBadge->setVisible(m_slice->isTxSlice());
    const char letters[] = "ABCD";
    int id = m_slice->sliceId();
    m_sliceBadge->setText(QString(QChar(id >= 0 && id < 4 ? letters[id] : '?')));
    updateFreqLabel();
    updateFilterLabel();

    // Mode tab
    m_tabBtns[2]->setText(m_slice->mode());
    updateModeTab();

    // Audio
    m_afGainSlider->setValue(static_cast<int>(m_slice->audioGain()));
    m_panSlider->setValue(m_slice->audioPan());
    {
        QSignalBlocker sb(m_muteBtn);
        m_muteBtn->setChecked(m_slice->audioMute());
    }

    // DSP
    auto syncDsp = [](QPushButton* btn, bool on) {
        QSignalBlocker sb(btn); btn->setChecked(on);
    };
    syncDsp(m_nbBtn,  m_slice->nbOn());
    syncDsp(m_nrBtn,  m_slice->nrOn());
    syncDsp(m_anfBtn, m_slice->anfOn());
    syncDsp(m_nrlBtn, m_slice->nrlOn());
    syncDsp(m_nrsBtn, m_slice->nrsOn());
    syncDsp(m_rnnBtn, m_slice->rnnOn());

    // RIT/XIT
    {
        QSignalBlocker sb1(m_ritBtn), sb2(m_xitBtn);
        m_ritBtn->setChecked(m_slice->ritOn());
        m_xitBtn->setChecked(m_slice->xitOn());
    }
    m_ritLabel->setText(QString("%1%2 Hz").arg(m_slice->ritFreq() >= 0 ? "+" : "").arg(m_slice->ritFreq()));
    m_xitLabel->setText(QString("%1%2 Hz").arg(m_slice->xitFreq() >= 0 ? "+" : "").arg(m_slice->xitFreq()));

    m_updatingFromModel = false;
}

void VfoWidget::updateFreqLabel()
{
    if (!m_slice) return;
    long long hz = static_cast<long long>(std::round(m_slice->frequency() * 1e6));
    int mhzPart = static_cast<int>(hz / 1000000);
    int khzPart = static_cast<int>((hz / 1000) % 1000);
    int hzPart  = static_cast<int>(hz % 1000);
    m_freqLabel->setText(QString("%1.%2.%3")
        .arg(mhzPart)
        .arg(khzPart, 3, 10, QChar('0'))
        .arg(hzPart, 3, 10, QChar('0')));
}

void VfoWidget::updateFilterLabel()
{
    if (!m_slice) return;
    int w = m_slice->filterHigh() - m_slice->filterLow();
    if (w >= 1000)
        m_filterWidthLbl->setText(QString("%1K").arg(w / 1000.0, 0, 'f', 1));
    else
        m_filterWidthLbl->setText(QString::number(w));
}

void VfoWidget::updateModeTab()
{
    if (!m_slice) return;
    const QString& cur = m_slice->mode();
    for (auto* btn : m_modeBtns) {
        QSignalBlocker sb(btn);
        btn->setChecked(btn->text() == cur);
    }
}

void VfoWidget::setAntennaList(const QStringList& ants)
{
    m_antList = ants;
}

void VfoWidget::setTransmitModel(TransmitModel* txModel)
{
    m_txModel = txModel;
}

bool VfoWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto* lbl = qobject_cast<QLabel*>(obj);
        if (lbl) {
            int idx = m_tabBtns.indexOf(lbl);
            if (idx >= 0) {
                showTab(idx);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace AetherSDR
