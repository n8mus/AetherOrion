// SPDX-License-Identifier: GPL-2.0-or-later
#include "gui/orion/OrionCwDeck.h"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "core/AppSettings.h"
#include "core/CallsignInfo.h"
#include "core/CallsignLookupService.h"
#include "core/MaidenheadLocator.h"
#include "core/backends/orion/log/QsoContext.h"
#include "gui/orion/OrionCwWindow.h"
#include "gui/orion/OrionDeckStyle.h"

namespace AetherSDR {

using orion::logbook::QsoContext;

namespace {

const char* kCap = "color:#8798a8; font-size:10px;";

// The band map's worked-before ramp, as the Log QSO window paints it.
const char* statusColor(QChar st)
{
    switch (st.toLatin1()) {
    case 'N': return "#ff5c46";
    case 'B': return "#ffbe3c";
    case 'W': return "#82de8c";
    case 'C': return "#96a2b2";
    default: return "#ffffff";
    }
}

// Callsign shape incl. portable forms (K2J/4, F/N8EM): the CW window's.
const QRegularExpression& callRe()
{
    static const QRegularExpression re(QStringLiteral("^(?:[A-Z0-9]{1,3}/)?"
                                                      "(?:[A-Z]{1,2}|[0-9][A-Z]|[A-Z][0-9])"
                                                      "[0-9]{1,2}[A-Z]{1,4}"
                                                      "(?:/[A-Z0-9]{1,4})?$"));
    return re;
}

// The band the deck SHOWS ("40m"); worked-before asks the host with the Hz.
QString bandText(qint64 hz)
{
    struct Band { qint64 lo, hi; const char* name; };
    static const Band bands[] = {
        {1'800'000, 2'000'000, "160m"},   {3'500'000, 4'000'000, "80m"},
        {5'250'000, 5'450'000, "60m"},    {7'000'000, 7'300'000, "40m"},
        {10'100'000, 10'150'000, "30m"},  {14'000'000, 14'350'000, "20m"},
        {18'068'000, 18'168'000, "17m"},  {21'000'000, 21'450'000, "15m"},
        {24'890'000, 24'990'000, "12m"},  {28'000'000, 29'700'000, "10m"},
        {50'000'000, 54'000'000, "6m"},   {144'000'000, 148'000'000, "2m"},
    };
    for (const Band& b : bands) {
        if (hz >= b.lo && hz <= b.hi) return QLatin1String(b.name);
    }
    return QString();
}

QVariant setting(const QString& key, const QVariant& fallback)
{
    return AppSettings::instance().value(QStringLiteral("OrionCwDeck/") + key, fallback);
}

void saveSetting(const QString& key, const QVariant& value)
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("OrionCwDeck/") + key, value);
    s.save();
}

// A deck check box: a push button that wears the deck's lit look, bound
// both ways to the CW window's own box so the two never disagree.
QPushButton* bindToggle(const QString& text, QAbstractButton* src, const QString& tip,
                        QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setCheckable(true);
    b->setFocusPolicy(Qt::NoFocus);
    b->setToolTip(tip);
    b->setFixedHeight(20);
    if (src) {
        b->setChecked(src->isChecked());
        b->setEnabled(src->isEnabled());
        QObject::connect(src, &QAbstractButton::toggled, b, &QAbstractButton::setChecked);
        QObject::connect(b, &QAbstractButton::toggled, src, &QAbstractButton::setChecked);
    } else {
        b->setEnabled(false);
    }
    return b;
}

}  // namespace

OrionCwDeck::OrionCwDeck(OrionCwWindow* cw, QsoContext* ctx, QWidget* parent)
    : QWidget(parent)
    , m_cw(cw)
    , m_ctx(ctx)
{
    setObjectName(QStringLiteral("orionCwDeck"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QLatin1String(orion::kDeckStyle));
    buildUi();

    // The callbook: AetherSDR's own service. Its answer fills the shared
    // record's name/QTH/grid (the NAME box is HIS word and is never
    // touched), so [his qth] keys and the heading sharpens.
    auto& svc = CallsignLookupService::instance();
    connect(&svc, &CallsignLookupService::infoReady, this,
            [this](const CallsignInfo& info, bool) { applyCallbook(info); });

    if (m_ctx) {
        struct Link { QLineEdit* e; void (QsoContext::*set)(const QString&); };
        const Link links[] = {
            {m_call, &QsoContext::setCall},
            {m_name, &QsoContext::setHandle},
            {m_rstS, &QsoContext::setRstSent},
            {m_rstR, &QsoContext::setRstRcvd},
        };
        for (const Link& l : links) {
            connect(l.e, &QLineEdit::textChanged, this, [this, l](const QString& t) {
                if (m_ctxEcho) return;
                m_ctxEcho = true;
                (m_ctx->*l.set)(t);
                m_ctxEcho = false;
            });
        }
        connect(m_ctx, &QsoContext::changed, this, [this] {
            if (m_ctxEcho) return;
            m_ctxEcho = true;
            const auto put = [](QLineEdit* e, const QString& v) {
                if (e->text() != v) e->setText(v);
            };
            put(m_call, m_ctx->call());
            put(m_name, m_ctx->handle());
            put(m_rstS, m_ctx->rstSent());
            put(m_rstR, m_ctx->rstRcvd());
            m_ctxEcho = false;
            onCallEdited();
        });
        // What is on screen is the record.
        m_ctxEcho = true;
        if (!m_ctx->call().isEmpty()) m_call->setText(m_ctx->call());
        if (!m_ctx->handle().isEmpty()) m_name->setText(m_ctx->handle());
        if (!m_ctx->rstSent().isEmpty()) m_rstS->setText(m_ctx->rstSent());
        if (!m_ctx->rstRcvd().isEmpty()) m_rstR->setText(m_ctx->rstRcvd());
        m_ctxEcho = false;
        m_ctx->setCall(m_call->text());
        m_ctx->setHandle(m_name->text());
        m_ctx->setRstSent(m_rstS->text());
        m_ctx->setRstRcvd(m_rstR->text());
    }
    onCallEdited();
}

void OrionCwDeck::setStation(Station station)
{
    m_station = std::move(station);
    refreshInfo();
}

void OrionCwDeck::buildUi()
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(6);

    // ---- CW READ (left third) ---------------------------------------------
    {
        m_readPane = new QWidget(this);
        auto* box = new QVBoxLayout(m_readPane);
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(2);
        auto* trow = new QHBoxLayout;
        auto* t = new QLabel(tr("CW READ"), this);
        t->setStyleSheet(QLatin1String(kCap));
        trow->addWidget(t);
        trow->addStretch(1);
        auto* clr = new QPushButton(tr("clear"), this);
        clr->setFlat(true);
        clr->setFocusPolicy(Qt::NoFocus);
        clr->setStyleSheet(QStringLiteral("color:#8798a8; font-size:10px; border:0;"));
        clr->setCursor(Qt::PointingHandCursor);
        trow->addWidget(clr);
        box->addLayout(trow);
        m_read = new QPlainTextEdit(this);
        m_read->setObjectName(QStringLiteral("orionCwDeckRead"));
        m_read->setReadOnly(true);
        m_read->setMaximumBlockCount(200);
        m_read->setStyleSheet(QStringLiteral("QPlainTextEdit#orionCwDeckRead { background:#2b5c8a;"
                                             " color:#ffffff; border:1px solid #3d7ab6;"
                                             " selection-background-color:#1c2430; }"));
        m_read->setMinimumWidth(120);
        m_read->setToolTip(tr("Decoded CW. Double-click a call to work it ·"
                              " right-click: text size"));
        sizeMenu(m_read, QStringLiteral("read"));
        connect(clr, &QPushButton::clicked, m_read, &QPlainTextEdit::clear);
        m_read->viewport()->installEventFilter(this);
        box->addWidget(m_read, 1);
        lay->addWidget(m_readPane, 3);
        addPopout(QStringLiteral("read"), tr("CW READ"), m_readPane, trow, [this](QWidget* w) {
            static_cast<QHBoxLayout*>(layout())->insertWidget(0, w, 3);
        });
    }

    // ---- middle third: the QSO ----------------------------------------------
    {
        auto* mid = new QVBoxLayout;
        mid->setSpacing(3);

        // Row 1: CALL · NAME · S · R · 0-BEAT · LOG IT. Big type: the call
        // must read from across the desk while hand-sending.
        auto* r1 = new QHBoxLayout;
        r1->setSpacing(4);
        QFont big;
        big.setPointSize(16);
        big.setBold(true);
        const auto cap = [this](const QString& t) {
            auto* l = new QLabel(t, this);
            l->setStyleSheet(QLatin1String(kCap));
            l->setObjectName(QStringLiteral("cap"));   // keeps its text width
            return l;
        };
        r1->addWidget(cap(tr("CALL")));
        m_call = new QLineEdit(this);
        m_call->setObjectName(QStringLiteral("orionCwDeckCall"));
        m_call->setFont(big);
        m_call->setMaxLength(16);
        m_call->setMinimumWidth(118);
        m_call->setMaximumWidth(150);
        m_call->setToolTip(tr("The station you're working — what [his call] keys.\n"
                              "Enter moves to RST R · ←/→ (empty) walk the spots.\n"
                              "A call from the panadapter lands here on click or arrow."));
        m_call->installEventFilter(this);
        r1->addWidget(m_call);
        r1->addWidget(cap(tr("NAME")));
        m_name = new QLineEdit(this);
        m_name->setObjectName(QStringLiteral("orionCwDeckName"));
        m_name->setFont(big);
        m_name->setMaxLength(14);
        m_name->setMinimumWidth(56);
        m_name->setToolTip(tr("The name he GIVES you — what [his name] keys.\nEnter logs."));
        m_name->installEventFilter(this);
        r1->addWidget(m_name, 1);
        const auto rst = [&](const QString& t, QLineEdit** e, const QString& tip,
                             const char* name) {
            r1->addWidget(cap(t));
            *e = new QLineEdit(this);
            (*e)->setObjectName(QLatin1String(name));
            (*e)->setFont(big);
            (*e)->setMaxLength(5);
            (*e)->setFixedWidth(52);
            (*e)->setText(QStringLiteral("599"));
            (*e)->setToolTip(tip);
            (*e)->installEventFilter(this);
            r1->addWidget(*e);
        };
        rst(QStringLiteral("S"), &m_rstS, tr("RST you SENT — what [rst sent] keys. Enter logs."),
            "orionCwDeckRstS");
        rst(QStringLiteral("R"), &m_rstR, tr("RST he GAVE you. Enter logs."), "orionCwDeckRstR");
        auto* zb = new QPushButton(QStringLiteral("0-BEAT"), this);
        zb->setObjectName(QStringLiteral("orionCwDeckZeroBeat"));
        zb->setFocusPolicy(Qt::NoFocus);
        zb->setToolTip(tr("Zero-beat the station you are copying (AetherSDR's own)"));
        connect(zb, &QPushButton::clicked, this, [this] { emit zeroBeatRequested(); });
        r1->addWidget(zb);
        auto* logBtn = new QPushButton(tr("LOG IT"), this);
        logBtn->setObjectName(QStringLiteral("orionCwDeckLog"));
        logBtn->setFocusPolicy(Qt::NoFocus);
        logBtn->setStyleSheet(QStringLiteral("QPushButton { background:#2b5c8a; color:#ffffff;"
                                             " border:1px solid #5db2f0; font-weight:bold;"
                                             " padding:2px 8px; }"));
        logBtn->setToolTip(tr("Log the QSO to the station log: call, both reports, name,\n"
                              "dial, mode and UTC now. Name/QTH/grid fill from the callbook"
                              " after.\nEnter in S, R or NAME does the same."));
        connect(logBtn, &QPushButton::clicked, this, [this] { emit logRequested(); });
        r1->addWidget(logBtn);
        for (QLineEdit* e : {m_call, m_name, m_rstS, m_rstR}) e->setFixedHeight(30);
        mid->addLayout(r1);

        // Row 2: country · worked-before, then Spot / SP / LP.
        auto* r2 = new QHBoxLayout;
        r2->setSpacing(4);
        m_info = new QLabel(this);
        m_info->setObjectName(QStringLiteral("orionCwDeckInfo"));
        m_info->setStyleSheet(QStringLiteral("color:#c8d4e0; font-size:11px;"));
        m_info->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        r2->addWidget(m_info, 1);
        auto* spot = new QPushButton(tr("Spot"), this);
        spot->setObjectName(QStringLiteral("orionCwDeckSpot"));
        spot->setFocusPolicy(Qt::NoFocus);
        spot->setToolTip(tr("Send this call to the cluster as a DX spot at the dial"));
        connect(spot, &QPushButton::clicked, this, [this] {
            const QString c = call();
            if (!c.isEmpty()) emit spotDxRequested(c, m_rigHz);
        });
        r2->addWidget(spot);
        m_spBtn = new QPushButton(QStringLiteral("SP"), this);
        m_lpBtn = new QPushButton(QStringLiteral("LP"), this);
        m_spBtn->setObjectName(QStringLiteral("orionCwDeckSp"));
        m_lpBtn->setObjectName(QStringLiteral("orionCwDeckLp"));
        for (QPushButton* b : {m_spBtn, m_lpBtn}) {
            b->setFocusPolicy(Qt::NoFocus);
            b->setFixedWidth(34);
            r2->addWidget(b);
        }
        connect(m_spBtn, &QPushButton::clicked, this, [this] {
            if (m_station.turnRotor && m_spAz >= 0) {
                m_station.turnRotor(m_spAz);
                emit rotorAimed(m_spAz, m_distKm, call());
            }
        });
        connect(m_lpBtn, &QPushButton::clicked, this, [this] {
            if (m_station.turnRotor && m_lpAz >= 0) {
                m_station.turnRotor(m_lpAz);
                emit rotorAimed(m_lpAz, -1.0, call() + QStringLiteral(" LP"));
            }
        });
        mid->addLayout(r2);

        // Rows 3-4: F1-F12, two rows of six, the CW window's own macros.
        auto* fg = new QGridLayout;
        fg->setSpacing(3);
        for (int i = 0; i < 12; ++i) {
            m_fk[i] = new QPushButton(this);
            m_fk[i]->setObjectName(QStringLiteral("orionCwDeckF") + QString::number(i + 1));
            m_fk[i]->setFocusPolicy(Qt::NoFocus);
            m_fk[i]->setFixedHeight(22);
            m_fk[i]->setMinimumWidth(40);
            m_fk[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            {
                QFont f = m_fk[i]->font();
                f.setPointSize(std::max(7, f.pointSize() - 1));
                m_fk[i]->setFont(f);
            }
            connect(m_fk[i], &QPushButton::clicked, this, [this, i] { keyFkey(i); });
            m_fk[i]->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(m_fk[i], &QPushButton::customContextMenuRequested, this,
                    [this, i](const QPoint&) {
                        if (i == 11) return;
                        m_cw->editFkey(i);
                        applyFkeyLabels();
                    });
            fg->addWidget(m_fk[i], i / 6, i % 6);
            fg->setColumnStretch(i % 6, 1);
        }
        mid->addLayout(fg);
        connect(m_cw, &OrionCwWindow::fkeysChanged, this, &OrionCwDeck::applyFkeyLabels);
        applyFkeyLabels();

        // Row 5: WPM · TUNE · STOP · · · RX · reader · CW WIN
        auto* r5 = new QHBoxLayout;
        r5->setSpacing(4);
        r5->addWidget(cap(tr("WPM")));
        m_wpm = new QSpinBox(this);
        m_wpm->setObjectName(QStringLiteral("orionCwDeckWpm"));
        m_wpm->setRange(5, 60);
        m_wpm->setValue(m_cw->speedWpm());
        m_wpm->setFixedWidth(52);
        m_wpm->setFocusPolicy(Qt::ClickFocus);
        m_wpm->setToolTip(tr("Keying speed (↑/↓ anywhere in the deck; the WinKeyer pot "
                             "moves it too)"));
        connect(m_wpm, &QSpinBox::valueChanged, this, [this](int v) { m_cw->setSpeedWpm(v); });
        connect(m_cw, &OrionCwWindow::speedChanged, this, [this](int v) {
            const QSignalBlocker b(m_wpm);
            m_wpm->setValue(v);
        });
        r5->addWidget(m_wpm);
        m_tuneBtn = new QPushButton(tr("TUNE"), this);
        m_tuneBtn->setObjectName(QStringLiteral("orionCwDeckTune"));
        m_tuneBtn->setCheckable(true);
        m_tuneBtn->setFocusPolicy(Qt::NoFocus);
        m_tuneBtn->setToolTip(tr("Steady key-down for tuning (click again to stop)"));
        m_tuneBtn->setVisible(m_cw->tuneAvailable());
        connect(m_tuneBtn, &QPushButton::toggled, this, [this](bool on) {
            m_cw->setTune(on);
            setStatus(on ? tr("TUNE — key down") : QString());
        });
        r5->addWidget(m_tuneBtn);
        auto* stop = new QPushButton(tr("STOP"), this);
        stop->setObjectName(QStringLiteral("orionCwDeckStop"));
        stop->setFocusPolicy(Qt::NoFocus);
        stop->setToolTip(tr("Dump the keying buffer now (Esc)"));
        connect(stop, &QPushButton::clicked, this, [this] { stopEverything(); });
        r5->addWidget(stop);
        r5->addStretch(1);
        r5->addWidget(bindToggle(tr("RX"), m_cw->toggle(OrionCwWindow::Toggle::RxDecode),
                                 tr("Decode the tuned CW signal into the read pane"), this));
        // Which reader is doing the copy: the CW window's box, mirrored.
        m_decoder = new QComboBox(this);
        m_decoder->setObjectName(QStringLiteral("orionCwDeckDecoder"));
        m_decoder->setFocusPolicy(Qt::NoFocus);
        m_decoder->setToolTip(tr("Which CW decoder reads the air (the CW window's choice)"));
        r5->addWidget(m_decoder);
        if (QComboBox* src = m_cw->decoderBox()) {
            connect(src, &QComboBox::currentIndexChanged, this, [this](int) { syncDecoderBox(); });
            connect(src, &QComboBox::currentTextChanged, this,
                    [this](const QString&) { syncDecoderBox(); });
            connect(m_decoder, &QComboBox::currentIndexChanged, this, [this, src](int idx) {
                if (m_decoderEcho || idx < 0) return;
                m_decoderEcho = true;
                src->setCurrentIndex(idx);
                m_decoderEcho = false;
            });
        }
        syncDecoderBox();
        auto* cwWin = new QPushButton(tr("CW WIN"), this);
        cwWin->setObjectName(QStringLiteral("orionCwDeckCwWindow"));
        cwWin->setFocusPolicy(Qt::NoFocus);
        cwWin->setToolTip(tr("The CW window itself: the reader's full pane, the macro editor,"
                             " the radio's CW box"));
        connect(cwWin, &QPushButton::clicked, this, [this] { emit cwWindowRequested(); });
        r5->addWidget(cwWin);
        mid->addLayout(r5);

        // Rows 6-7: the radio's own CW settings over CAT, mirrors of the CW
        // window's sliders — a drag here moves that slider (which owns the
        // CAT send); that slider moving moves this.
        const auto rigRow = [this](int row) {
            m_rigRow[row] = new QWidget(this);
            auto* h = new QHBoxLayout(m_rigRow[row]);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(4);
            return h;
        };
        const auto mirror = [this](QHBoxLayout* h, const QString& capText,
                                   OrionCwWindow::RigCtl which, QSlider** out, QLabel** val) {
            auto* c = new QLabel(capText, this);
            c->setStyleSheet(QLatin1String(kCap));
            c->setFixedWidth(34);
            h->addWidget(c);
            auto* s = new QSlider(Qt::Horizontal, this);
            s->setFocusPolicy(Qt::NoFocus);
            QSlider* src = m_cw->rigSlider(which);
            if (src) {
                s->setRange(src->minimum(), src->maximum());
                s->setSingleStep(src->singleStep());
                s->setPageStep(src->pageStep());
                s->setValue(src->value());
                s->setToolTip(src->toolTip());
                c->setToolTip(src->toolTip());
                connect(s, &QSlider::valueChanged, src, &QSlider::setValue);
                // The window sends on release; a drag here must release there.
                connect(s, &QSlider::sliderReleased, src, &QSlider::sliderReleased);
                connect(src, &QSlider::valueChanged, this, [s](int v) {
                    const QSignalBlocker b(s);
                    s->setValue(v);
                });
            }
            h->addWidget(s, 1);
            auto* v = new QLabel(QStringLiteral("--"), this);
            v->setFixedWidth(34);
            v->setStyleSheet(QLatin1String(kCap));
            h->addWidget(v);
            const auto show = [v](int nv) { v->setText(QString::number(nv)); };
            connect(s, &QSlider::valueChanged, v, show);
            if (src) {
                connect(src, &QSlider::valueChanged, v, show);
                show(src->value());
            }
            *out = s;
            *val = v;
        };
        auto* r6 = rigRow(0);
        mirror(r6, tr("VOL"), OrionCwWindow::RigCtl::Vol, &m_rigVol, &m_rigVolVal);
        mirror(r6, tr("PITCH"), OrionCwWindow::RigCtl::Pitch, &m_rigPitch, &m_rigPitchVal);
        mid->addWidget(m_rigRow[0]);
        auto* r7 = rigRow(1);
        mirror(r7, tr("QSK"), OrionCwWindow::RigCtl::Qsk, &m_rigQsk, &m_rigQskVal);
        mirror(r7, tr("RISE"), OrionCwWindow::RigCtl::Rise, &m_rigRise, &m_rigRiseVal);
        mid->addWidget(m_rigRow[1]);
        setRigCwAvailable(m_cw->rigCwAvailable());
        connect(m_cw, &OrionCwWindow::rigCwAvailableChanged, this,
                &OrionCwDeck::setRigCwAvailable);

        // Row 8: the reader's wpm · Hz, the status, the rig keyer.
        auto* r8 = new QHBoxLayout;
        r8->setSpacing(6);
        m_rxInfo = new QLabel(this);
        m_rxInfo->setObjectName(QStringLiteral("orionCwDeckRxInfo"));
        m_rxInfo->setTextFormat(Qt::RichText);
        m_rxInfo->setStyleSheet(QStringLiteral("color:#6aa5d8; font-size:11px;"));
        // FIXED at the widest text it ever shows: the Hz readout changes
        // length as the tone wanders, and a label that re-negotiates its
        // width bounces the whole row.
        m_rxInfo->setFixedWidth(
            m_rxInfo->fontMetrics().horizontalAdvance(QStringLiteral("88 WPM · 8888 Hz")) + 12);
        m_rxInfo->setObjectName(QStringLiteral("cap"));
        r8->addWidget(m_rxInfo);
        m_status = new QLabel(this);
        m_status->setObjectName(QStringLiteral("orionCwDeckStatus"));
        m_status->setStyleSheet(QLatin1String(kCap));
        m_status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        r8->addWidget(m_status, 1);
        m_rigKeyer = new QLabel(m_cw->rigKeyerText(), this);
        m_rigKeyer->setStyleSheet(QLatin1String(kCap));
        m_rigKeyer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        m_rigKeyer->setToolTip(tr("The RADIO's built-in keyer, read-only"));
        connect(m_cw, &OrionCwWindow::rigKeyerTextChanged, m_rigKeyer, &QLabel::setText);
        r8->addWidget(m_rigKeyer, 1);
        mid->addLayout(r8);
        mid->addStretch(1);
        lay->addLayout(mid, 4);   // the entry row needs the width
    }

    // ---- CW TYPE (right third) --------------------------------------------
    {
        m_typePane = new QWidget(this);
        auto* box = new QVBoxLayout(m_typePane);
        box->setContentsMargins(0, 0, 0, 0);
        box->setSpacing(2);
        auto* thdr = new QHBoxLayout;
        auto* t = new QLabel(tr("CW TYPE — Enter sends"), this);
        t->setStyleSheet(QLatin1String(kCap));
        thdr->addWidget(t);
        thdr->addStretch(1);
        box->addLayout(thdr);
        m_type = new QPlainTextEdit(this);
        m_type->setObjectName(QStringLiteral("orionCwDeckType"));
        m_type->setStyleSheet(QStringLiteral("QPlainTextEdit#orionCwDeckType { background:#2b5c8a;"
                                             " color:#ffffff; border:1px solid #3d7ab6;"
                                             " selection-background-color:#1c2430; }"));
        m_type->setMaximumBlockCount(100);
        m_type->setMinimumWidth(120);
        m_type->setToolTip(tr("Type CW; Enter keys the line and keeps it as history above.\n"
                              "LIVE: each keystroke goes out as typed (Backspace unsends).\n"
                              "WORD: each word goes out at the space bar.\n"
                              "Esc stops keying · right-click: text size"));
        sizeMenu(m_type, QStringLiteral("type"));
        m_type->installEventFilter(this);
        connect(m_type, &QPlainTextEdit::textChanged, this, [this] { onTypeChanged(); });
        box->addWidget(m_type, 1);
        auto* brow = new QHBoxLayout;
        brow->setSpacing(4);
        brow->addWidget(bindToggle(tr("LIVE"), m_cw->toggle(OrionCwWindow::Toggle::Live),
                                   tr("Stream each keystroke to the keyer as typed"), this));
        brow->addWidget(bindToggle(tr("WORD"), m_cw->toggle(OrionCwWindow::Toggle::Word),
                                   tr("Each word goes out when the space bar lands"), this));
        m_sent = new QLabel(this);
        m_sent->setObjectName(QStringLiteral("orionCwDeckSent"));
        m_sent->setStyleSheet(QStringLiteral("color:#a4cf82; font-size:10px;"));
        m_sent->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        brow->addWidget(m_sent, 1);
        box->addLayout(brow);
        lay->addWidget(m_typePane, 3);
        addPopout(QStringLiteral("type"), tr("CW TYPE"), m_typePane, thdr, [this](QWidget* w) {
            static_cast<QHBoxLayout*>(layout())->addWidget(w, 3);
        });
    }

    // The call box: uppercase, no spaces, the shared record, the arrows.
    connect(m_call, &QLineEdit::textEdited, this, [this](const QString& t) {
        QString up = t.toUpper();
        up.remove(QLatin1Char(' '));
        if (up != t) {
            const int pos = m_call->cursorPosition();
            m_call->setText(up);
            m_call->setCursorPosition(std::min(pos, int(up.size())));
        }
        m_callFromSpot = false;
        onCallEdited();
    });
    // However the call arrives (typed, a spot landing, the shared record
    // from another window, the bridge), the info line follows it.
    connect(m_call, &QLineEdit::textChanged, this, [this] { refreshInfo(); });
    connect(m_call, &QLineEdit::returnPressed, this, [this] {
        requestCallbook(m_call->text());
        m_rstR->setFocus();
        m_rstR->selectAll();
    });
    connect(m_call, &QLineEdit::editingFinished, this,
            [this] { requestCallbook(m_call->text()); });
    for (QLineEdit* e : {m_name, m_rstS, m_rstR}) {
        connect(e, &QLineEdit::returnPressed, this, [this] { emit logRequested(); });
    }

    // Prose labels never widen the window (the deck sits under the pan; the
    // width budget is the main window's floor).
    for (QLabel* l : findChildren<QLabel*>()) {
        if (l->sizePolicy().horizontalPolicy() == QSizePolicy::Ignored) continue;
        if (l->objectName() == QLatin1String("cap")) continue;
        auto sp = l->sizePolicy();
        sp.setHorizontalPolicy(QSizePolicy::Maximum);
        l->setSizePolicy(sp);
        l->setMinimumWidth(0);
    }
    refreshInfo();
    setRxStats(-1.0f, 0.0f);
}

// ---- context -----------------------------------------------------------------

QString OrionCwDeck::call() const
{
    return m_call->text().trimmed().toUpper();
}

QString OrionCwDeck::infoText() const
{
    return m_info->text();
}

void OrionCwDeck::setRig(qint64 hz, const QString& adifMode)
{
    const bool bandMoved = bandText(hz) != bandText(m_rigHz);
    m_rigHz = hz;
    if (!adifMode.isEmpty()) m_rigMode = adifMode;
    if (bandMoved) refreshInfo();
}

void OrionCwDeck::nudgeSpeed(int delta)
{
    m_wpm->setValue(m_wpm->value() + delta);
    setStatus(tr("%1 wpm").arg(m_wpm->value()));
}

void OrionCwDeck::prefillCall(const QString& call, qint64 hz)
{
    Q_UNUSED(hz);
    const QString c = call.trimmed().toUpper();
    if (c.isEmpty()) return;
    const bool fresh = c != this->call();
    m_call->setText(c);   // textChanged -> the record
    if (fresh) {
        m_name->clear();
        if (m_ctx) {
            m_ctx->setName(QString());
            m_ctx->setQth(QString());
            m_ctx->setGrid(QString());
        }
    }
    m_callFromSpot = true;
    onCallEdited();
    if (fresh) requestCallbook(c);
    if (isVisible()) {
        m_call->setFocus();
        m_call->deselect();
    }
}

void OrionCwDeck::setRigCwAvailable(bool on)
{
    for (QWidget* w : m_rigRow) {
        if (w) w->setVisible(on);
    }
}

void OrionCwDeck::syncDecoderBox()
{
    QComboBox* src = m_cw->decoderBox();
    if (!src) {
        m_decoder->setVisible(false);
        return;
    }
    m_decoderEcho = true;
    const QSignalBlocker b(m_decoder);
    m_decoder->clear();
    for (int i = 0; i < src->count(); ++i) {
        m_decoder->addItem(src->itemText(i), src->itemData(i));
    }
    m_decoder->setCurrentIndex(src->currentIndex());
    m_decoder->setVisible(src->count() > 1);
    m_decoderEcho = false;
}

void OrionCwDeck::clearForNext()
{
    m_callFromSpot = false;
    m_call->clear();
    m_name->clear();
    m_rstS->setText(QStringLiteral("599"));
    m_rstR->setText(QStringLiteral("599"));
    if (m_ctx) {
        m_ctx->setRstSent(QStringLiteral("599"));
        m_ctx->setRstRcvd(QStringLiteral("599"));
    }
    onCallEdited();
    if (isVisible()) m_call->setFocus();
}

// ---- reader ------------------------------------------------------------------

void OrionCwDeck::appendRead(const QString& text)
{
    if (text.isEmpty()) return;
    m_read->moveCursor(QTextCursor::End);
    m_read->insertPlainText(text);
    m_read->moveCursor(QTextCursor::End);
}

// "18 WPM · 547 Hz" — green when the tone is within +/-10 Hz of AetherSDR's
// sidetone pitch and amber when off. Both slots always render so the text
// width stays constant, and the shown Hz only moves when the measurement
// really moved (>1.5 Hz) — no last-digit flicker.
void OrionCwDeck::setRxStats(float pitchHz, float wpm)
{
    m_rxWpm = wpm > 0.0f ? static_cast<int>(std::lround(wpm)) : 0;
    m_rxPitch = pitchHz > 0.0f ? static_cast<double>(pitchHz) : -1.0;
    if (m_rxPitch < 0) {
        m_shownHz = -1.0;
    } else if (m_shownHz < 0 || std::abs(m_rxPitch - m_shownHz) > 1.5) {
        m_shownHz = m_rxPitch;
    }
    const QString wpmPart = m_rxWpm > 0 ? QStringLiteral("%1 WPM").arg(m_rxWpm)
                                        : QStringLiteral("— WPM");
    QString hzPart = QStringLiteral("— Hz");
    if (m_shownHz > 0) {
        const int target =
            AppSettings::instance().value(QStringLiteral("CwSidetonePitch"), 600).toInt();
        const bool on = std::abs(m_shownHz - target) <= 10.0;
        hzPart = QStringLiteral("<span style='color:%1'>%2 Hz</span>")
                     .arg(on ? QStringLiteral("#8fd48f") : QStringLiteral("#e0b060"))
                     .arg(qRound(m_shownHz));
    }
    m_rxInfo->setText(wpmPart + QStringLiteral("&nbsp;·&nbsp;") + hzPart);
}

// ---- sending -----------------------------------------------------------------

void OrionCwDeck::keyFkey(int idx0)
{
    if (idx0 == 11) {
        wipe();
        return;
    }
    if (idx0 < 0 || idx0 > 11) return;
    const QString spec = m_cw->fkeySpec(idx0);
    const QString text = spec.section(QLatin1Char('|'), 1);
    if (text.trimmed().isEmpty()) return;
    const QString missing = m_cw->sendTyped(text);
    if (!missing.isEmpty()) {
        setStatus(tr("nothing in %1 — fill it first").arg(missing));
        if (missing == QLatin1String("his call")) m_call->setFocus();
        else if (missing == QLatin1String("his name")) m_name->setFocus();
        return;
    }
    m_sent->setText(QStringLiteral("→ ") + spec.section(QLatin1Char('|'), 0, 0).trimmed());
    setStatus(QStringLiteral("F%1").arg(idx0 + 1));
}

void OrionCwDeck::applyFkeyLabels()
{
    for (int i = 0; i < 12; ++i) {
        if (i == 11) {
            m_fk[i]->setText(tr("F12 WIPE"));
            m_fk[i]->setToolTip(tr("Clear the entry for the next contact"));
            continue;
        }
        const QString spec = m_cw->fkeySpec(i);
        const QString label = spec.section(QLatin1Char('|'), 0, 0).trimmed();
        const QString text = spec.section(QLatin1Char('|'), 1);
        m_fk[i]->setText(QStringLiteral("F%1 %2")
                             .arg(i + 1)
                             .arg(label.isEmpty() ? QStringLiteral("—") : label));
        m_fk[i]->setToolTip(tr("F%1  %2\nsends:  %3\n\nclick or press F%1 · right-click to edit")
                                .arg(i + 1)
                                .arg(label, text));
        m_fk[i]->setEnabled(!text.trimmed().isEmpty());
    }
}

void OrionCwDeck::wipe()
{
    clearForNext();
    setStatus(tr("wiped"));
}

void OrionCwDeck::stopEverything()
{
    m_cw->stopKeying();
    m_tuneBtn->setChecked(false);
    // Drop the line being typed (it may have gone out half way).
    QTextCursor c = m_type->textCursor();
    c.movePosition(QTextCursor::End);
    c.select(QTextCursor::LineUnderCursor);
    c.removeSelectedText();
    m_typeLine.clear();
    m_typeSent = 0;
    setStatus(tr("stopped"));
}

QString OrionCwDeck::currentTypeLine() const
{
    return m_type->document()->lastBlock().text();
}

void OrionCwDeck::sendTypedLine()
{
    const QString line = currentTypeLine();
    const bool live = m_cw->toggle(OrionCwWindow::Toggle::Live)->isChecked();
    const bool word = m_cw->toggle(OrionCwWindow::Toggle::Word)->isChecked();
    const QString rest = word ? line.mid(m_typeSent) : line;
    if (!live && !rest.trimmed().isEmpty()) {
        const QString missing = m_cw->sendTyped(rest);
        if (!missing.isEmpty()) {
            setStatus(tr("nothing in %1 — fill it first").arg(missing));
            return;
        }
        m_sent->setText(rest.right(40));
    }
    // The line stays as history; a fresh line opens below.
    QTextCursor c = m_type->textCursor();
    c.movePosition(QTextCursor::End);
    m_type->setTextCursor(c);
    if (!line.isEmpty()) m_type->insertPlainText(QStringLiteral("\n"));
    m_typeLine.clear();
    m_typeSent = 0;
}

void OrionCwDeck::onTypeChanged()
{
    const QString now = currentTypeLine();
    const bool live = m_cw->toggle(OrionCwWindow::Toggle::Live)->isChecked();
    const bool word = m_cw->toggle(OrionCwWindow::Toggle::Word)->isChecked();
    if (live) {
        if (now.startsWith(m_typeLine)) {
            m_cw->streamChars(now.mid(m_typeLine.size()));
        } else if (m_typeLine.startsWith(now)) {
            m_cw->unsendChars(m_typeLine.size() - now.size());
        }
        if (now.size() > m_typeLine.size()) {
            m_sent->setText((m_sent->text() + now.mid(m_typeLine.size())).right(40));
        }
    } else if (word) {
        if (now.size() > m_typeLine.size() && now.endsWith(QLatin1Char(' '))
            && now.size() > m_typeSent) {
            const QString out = now.mid(m_typeSent);
            if (!out.trimmed().isEmpty()) {
                m_cw->sendTyped(out);
                m_sent->setText((m_sent->text() + out).right(40));
            }
            m_typeSent = now.size();
        }
        if (now.size() < m_typeSent) m_typeSent = now.size();
    }
    m_typeLine = now;
}

// ---- info line ---------------------------------------------------------------

void OrionCwDeck::onCallEdited()
{
    refreshInfo();
}

void OrionCwDeck::requestCallbook(const QString& call)
{
    const QString c = call.trimmed().toUpper();
    if (c.size() < 3 || !callRe().match(c).hasMatch()) return;
    if (c == m_asked || m_callbookGrid.contains(c)) return;
    auto& svc = CallsignLookupService::instance();
    if (!svc.enabled() || (!svc.hasCredentials() && !svc.hasCachedEntry(c))) return;
    m_asked = c;
    svc.lookup(c);
}

void OrionCwDeck::applyCallbook(const CallsignInfo& info)
{
    const QString c = info.call.trimmed().toUpper();
    if (c.isEmpty()) return;
    if (!info.grid.trimmed().isEmpty()) {
        m_callbookGrid.insert(c, info.grid.trimmed().toUpper());
    }
    if (c != call()) return;
    if (m_ctx) {
        QString name = info.nameFmt.trimmed();
        if (name.isEmpty()) {
            name = (info.firstName + QLatin1Char(' ') + info.lastName).trimmed();
        }
        QString qth = info.city.trimmed();
        if (!info.state.trimmed().isEmpty()) {
            qth = qth.isEmpty() ? info.state.trimmed()
                                : qth + QStringLiteral(", ") + info.state.trimmed();
        }
        if (m_ctx->name().isEmpty() && !name.isEmpty()) m_ctx->setName(name);
        if (m_ctx->qth().isEmpty() && !qth.isEmpty()) m_ctx->setQth(qth);
        if (m_ctx->grid().isEmpty() && !info.grid.trimmed().isEmpty()) {
            m_ctx->setGrid(info.grid.trimmed().toUpper());
        }
    }
    refreshInfo();
}

void OrionCwDeck::refreshInfo()
{
    const QString c = call();
    // Call colour: the band map's worked-before ramp.
    const QChar st = (m_station.workedStatus && !c.isEmpty()) ? m_station.workedStatus(c, m_rigHz)
                                                              : QChar('?');
    const bool shaped = c.isEmpty() || callRe().match(c).hasMatch();
    m_call->setStyleSheet(QStringLiteral("QLineEdit { color: %1; }")
                              .arg(shaped ? QLatin1String(statusColor(st))
                                          : QLatin1String("#e0b45d")));
    QString line;
    m_spAz = m_lpAz = m_distKm = -1.0;
    double myLat = 0, myLon = 0, dxLat = 0, dxLon = 0;
    const bool haveMe = m_station.myLatLon && m_station.myLatLon(myLat, myLon);
    bool haveDx = false;
    if (!c.isEmpty()) {
        if (m_station.country) line = m_station.country(c);
        QString grid = m_ctx ? m_ctx->grid() : QString();
        if (grid.isEmpty()) grid = m_callbookGrid.value(c);
        if (MaidenheadLocator::toLatLon(grid, dxLat, dxLon)) {
            haveDx = true;
        } else if (m_station.dxLatLon) {
            haveDx = m_station.dxLatLon(c, dxLat, dxLon);
        }
        const QString band = bandText(m_rigHz);
        switch (st.toLatin1()) {
        case 'W': line += tr(" · wkd b4 %1").arg(band); break;
        case 'C': line += tr(" · cfm %1").arg(band); break;
        case 'B': line += tr(" · new band"); break;
        case 'N': line += tr(" · NEW"); break;
        default: break;
        }
    }
    if (haveMe && haveDx) {
        m_spAz = MaidenheadLocator::bearingDeg(myLat, myLon, dxLat, dxLon);
        m_lpAz = m_spAz >= 180.0 ? m_spAz - 180.0 : m_spAz + 180.0;
        m_distKm = MaidenheadLocator::distanceKm(myLat, myLon, dxLat, dxLon);
        line += tr(" · %1° %2 km").arg(int(m_spAz + 0.5)).arg(int(m_distKm + 0.5));
    }
    m_info->setText(line.trimmed().startsWith(QStringLiteral("·"))
                        ? line.trimmed().mid(1).trimmed()
                        : line);
    const bool can = static_cast<bool>(m_station.turnRotor) && m_spAz >= 0;
    m_spBtn->setEnabled(can);
    m_lpBtn->setEnabled(can);
    m_spBtn->setToolTip(m_spAz >= 0 ? tr("Short path %1°").arg(int(m_spAz + 0.5))
                                    : tr("Turn the rotor to the short path"));
    m_lpBtn->setToolTip(m_lpAz >= 0 ? tr("Long path %1°").arg(int(m_lpAz + 0.5))
                                    : tr("Turn the rotor to the long path"));
}

void OrionCwDeck::setStatus(const QString& s)
{
    m_status->setText(s);
}

// ---- events ------------------------------------------------------------------

void OrionCwDeck::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    // F1-F12 and PgUp/PgDn while the deck is up, wherever focus sits in the
    // main window (F1 must key from the panadapter too). AetherSDR's own
    // CWX set and the CW window's are QShortcuts on the same keys; two
    // shortcuts on one key fire neither, so the deck takes the key before
    // the shortcut map sees it (ShortcutOverride) and handles the press.
    if (!m_keysArmed) {
        qApp->installEventFilter(this);
        m_keysArmed = true;
    }
    for (auto it = m_panes.begin(); it != m_panes.end(); ++it) {
        if (it->fly) it->fly->show();
    }
    applyFkeyLabels();
    m_call->setFocus();
}

void OrionCwDeck::hideEvent(QHideEvent* e)
{
    QWidget::hideEvent(e);
    if (m_keysArmed) {
        qApp->removeEventFilter(this);
        m_keysArmed = false;
    }
    for (auto it = m_panes.begin(); it != m_panes.end(); ++it) {
        if (it->fly) it->fly->hide();
    }
}

bool OrionCwDeck::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() == QEvent::Close) {
        for (auto it = m_panes.begin(); it != m_panes.end(); ++it) {
            if (it->fly == obj) {
                saveSetting(QStringLiteral("float/") + it.key() + QStringLiteral("/geom"),
                            QString::fromLatin1(it->fly->saveGeometry().toBase64()));
                unfloatPane(it.key(), false);
                return true;
            }
        }
    }
    // The deck's keys, application-wide while it is shown in the active
    // window: F1-F12, PgUp/PgDn. Only when the key would land in THIS
    // window (a floated pane counts); a dialog of its own keeps its keys.
    if (m_keysArmed
        && (ev->type() == QEvent::ShortcutOverride || ev->type() == QEvent::KeyPress)) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        const int k = ke->key();
        const bool fkey = k >= Qt::Key_F1 && k <= Qt::Key_F12 && ke->modifiers() == Qt::NoModifier;
        const bool page = (k == Qt::Key_PageUp || k == Qt::Key_PageDown)
                          && ke->modifiers() == Qt::NoModifier;
        if (fkey || page) {
            QWidget* w = qobject_cast<QWidget*>(obj);
            QWidget* top = w ? w->window() : nullptr;
            bool ours = top && top == window();
            if (!ours && top) {
                for (auto it = m_panes.cbegin(); it != m_panes.cend(); ++it) {
                    if (it->fly && it->fly == top) ours = true;
                }
            }
            if (ours) {
                if (ev->type() == QEvent::ShortcutOverride) {
                    ev->accept();
                    return true;
                }
                if (fkey) keyFkey(k - Qt::Key_F1);
                else nudgeSpeed(k == Qt::Key_PageUp ? +1 : -1);
                return true;
            }
        }
    }
    const bool ownBox = obj == m_call || obj == m_name || obj == m_rstS || obj == m_rstR
                        || obj == m_type;
    if (ownBox && ev->type() == QEvent::ShortcutOverride) {
        // Plain Esc must reach the boxes (an app-level Escape would eat it).
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Escape) {
            ev->accept();
            return true;
        }
    }
    if (ownBox && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Escape) {
            stopEverything();
            return true;
        }
        // ↑/↓ = keying speed from ANY box, the type pane included: the
        // hands never leave the keyboard.
        if ((ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down)
            && ke->modifiers() == Qt::NoModifier) {
            nudgeSpeed(ke->key() == Qt::Key_Up ? +1 : -1);
            return true;
        }
        if (obj == m_type) {
            if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
                sendTypedLine();
                return true;
            }
            // Earlier lines are history: typing always lands on the last.
            QTextCursor c = m_type->textCursor();
            if (c.block() != m_type->document()->lastBlock()) {
                c.movePosition(QTextCursor::End);
                m_type->setTextCursor(c);
            }
        }
        if (obj == m_call && (ke->key() == Qt::Key_Left || ke->key() == Qt::Key_Right)
            && (m_call->text().isEmpty() || m_callFromSpot)) {
            // The arrows walk the spots while the box is empty OR still
            // holding an untouched landing — ←/→ ROLL through the band.
            emit walkSpots(ke->key() == Qt::Key_Right ? +1 : -1);
            return true;
        }
    }
    if (m_read && obj == m_read->viewport() && ev->type() == QEvent::MouseButtonDblClick) {
        const QTextCursor c = m_read->cursorForPosition(static_cast<QMouseEvent*>(ev)->pos());
        const QString line = c.block().text();
        const int i = std::min(c.positionInBlock(), int(line.size()) - 1);
        if (line.isEmpty() || i < 0) return true;
        int a = i, b = i;
        while (a > 0 && !line[a - 1].isSpace()) --a;
        while (b < int(line.size()) && !line[b].isSpace()) ++b;
        QString tok = line.mid(a, b - a).toUpper();
        tok.remove(QRegularExpression(QStringLiteral("[^A-Z0-9/]")));
        if (callRe().match(tok).hasMatch()) prefillCall(tok, m_rigHz);
        return true;
    }
    return QWidget::eventFilter(obj, ev);
}

// ---- pop-outs + text size ------------------------------------------------------

void OrionCwDeck::sizeMenu(QPlainTextEdit* pane, const QString& key)
{
    const QString skey = key + QStringLiteral("Pt");
    const auto apply = [pane, skey](int pt) {
        pt = std::clamp(pt, 8, 28);
        QFont mf(QStringLiteral("DejaVu Sans Mono"));
        mf.setPointSize(pt);
        pane->setFont(mf);
        saveSetting(skey, pt);
    };
    apply(setting(skey, 13).toInt());
    pane->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(pane, &QPlainTextEdit::customContextMenuRequested, this,
            [pane, skey, apply](const QPoint& p) {
                QMenu m(pane);
                m.addAction(tr("Clear"), pane, &QPlainTextEdit::clear);
                m.addAction(tr("Text bigger"), pane,
                            [skey, apply] { apply(setting(skey, 13).toInt() + 1); });
                m.addAction(tr("Text smaller"), pane,
                            [skey, apply] { apply(setting(skey, 13).toInt() - 1); });
                m.exec(pane->mapToGlobal(p));
            });
}

void OrionCwDeck::addPopout(const QString& key, const QString& title, QWidget* pane,
                            QHBoxLayout* headerRow, std::function<void(QWidget*)> reinsert)
{
    PaneSlot s;
    s.pane = pane;
    s.title = title;
    s.reinsert = std::move(reinsert);
    m_panes.insert(key, s);
    auto* pop = new QPushButton(QStringLiteral("⧉"), pane);
    pop->setObjectName(QStringLiteral("orionCwDeckPop_") + key);
    pop->setFlat(true);
    pop->setFocusPolicy(Qt::NoFocus);
    pop->setCursor(Qt::PointingHandCursor);
    pop->setStyleSheet(QStringLiteral("color:#8798a8; font-size:11px; border:0;"));
    pop->setToolTip(tr("Pop out as its own window; closing it returns it"));
    connect(pop, &QPushButton::clicked, this, [this, key] { floatPane(key); });
    headerRow->addWidget(pop);
}

void OrionCwDeck::floatPane(const QString& key)
{
    PaneSlot& s = m_panes[key];
    if (!s.pane) return;
    if (s.fly) {
        s.fly->show();
        s.fly->raise();
        return;
    }
    auto* d = new QDialog(window());
    d->setObjectName(QStringLiteral("orionDeckPane"));
    d->setAttribute(Qt::WA_StyledBackground, true);
    d->setStyleSheet(QLatin1String(orion::kDeckStyle));
    d->setWindowTitle(s.title + tr(" — CW"));
    auto* v = new QVBoxLayout(d);
    v->setContentsMargins(6, 6, 6, 6);
    v->addWidget(s.pane);
    const QByteArray g = QByteArray::fromBase64(
        setting(QStringLiteral("float/") + key + QStringLiteral("/geom"), QString())
            .toString().toLatin1());
    if (!g.isEmpty()) d->restoreGeometry(g);
    else d->resize(430, 300);
    d->installEventFilter(this);
    s.fly = d;
    d->show();
}

void OrionCwDeck::unfloatPane(const QString& key, bool saveGeom)
{
    PaneSlot& s = m_panes[key];
    if (!s.fly) return;
    if (saveGeom) {
        saveSetting(QStringLiteral("float/") + key + QStringLiteral("/geom"),
                    QString::fromLatin1(s.fly->saveGeometry().toBase64()));
    }
    s.reinsert(s.pane);
    s.pane->show();
    s.fly->deleteLater();
    s.fly = nullptr;
}

}  // namespace AetherSDR
