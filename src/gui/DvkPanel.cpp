#include "DvkPanel.h"
#include "models/DvkModel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QShortcut>
#include <QScrollArea>

namespace AetherSDR {

static const char* kSlotStyle =
    "QWidget { background: #0f1520; border: 1px solid #203040; border-radius: 3px; }";

static const char* kFKeyStyle =
    "QPushButton { background: #1a2a3a; color: #00b4d8; border: 1px solid #203040; "
    "border-radius: 3px; font-size: 10px; font-weight: bold; min-width: 28px; max-width: 28px; }"
    "QPushButton:hover { background: #253545; }"
    "QPushButton:pressed { background: #00b4d8; color: #000; }";

static const char* kNameStyle =
    "QLabel { color: #c8d8e8; font-size: 10px; background: transparent; border: none; }";

static const char* kDurStyle =
    "QLabel { color: #6a8090; font-size: 9px; background: transparent; border: none; }";

static const char* kBtnStyle =
    "QPushButton { background: #1a2a3a; color: #c8d8e8; border: 1px solid #203040; "
    "border-radius: 3px; padding: 4px 8px; font-size: 11px; font-weight: bold; }"
    "QPushButton:hover { background: #253545; }"
    "QPushButton:checked { background: #00b4d8; color: #000; }";

struct SlotRow {
    QWidget* container;
    QPushButton* fkeyBtn;
    QLabel* nameLabel;
    QLabel* durLabel;
    int id;
};

DvkPanel::DvkPanel(DvkModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    auto* outerVbox = new QVBoxLayout(this);
    outerVbox->setContentsMargins(4, 4, 4, 4);
    outerVbox->setSpacing(4);

    // Title
    auto* title = new QLabel("DVK");
    title->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; font-size: 14px; }");
    outerVbox->addWidget(title);

    // Slot rows — stretch to fill panel
    m_slotLayout = new QVBoxLayout;
    m_slotLayout->setContentsMargins(0, 0, 0, 0);
    m_slotLayout->setSpacing(2);

    // Create 12 slot rows
    for (int i = 0; i < 12; ++i) {
        int id = i + 1;
        auto* row = new QWidget;
        row->setStyleSheet(kSlotStyle);
        row->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto* hbox = new QHBoxLayout(row);
        hbox->setContentsMargins(3, 2, 3, 2);
        hbox->setSpacing(4);

        auto* fkeyBtn = new QPushButton(QString("F%1").arg(id));
        fkeyBtn->setStyleSheet(kFKeyStyle);
        fkeyBtn->setFixedSize(28, 24);
        fkeyBtn->setToolTip(QString("Play recording %1 on-air (F%1)").arg(id));
        hbox->addWidget(fkeyBtn);

        auto* nameLabel = new QLabel(QString("Recording %1").arg(id));
        nameLabel->setStyleSheet(kNameStyle);
        hbox->addWidget(nameLabel, 1);

        auto* durLabel = new QLabel("Empty");
        durLabel->setStyleSheet(kDurStyle);
        durLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        durLabel->setFixedWidth(40);
        hbox->addWidget(durLabel);

        m_slotLayout->addWidget(row);

        // Store references
        m_fkeyBtns.append(fkeyBtn);
        m_nameLabels.append(nameLabel);
        m_durLabels.append(durLabel);

        // F-key button click → playback
        connect(fkeyBtn, &QPushButton::clicked, this, [this, id]() {
            if (m_model->status() == DvkModel::Playback && m_model->activeId() == id)
                m_model->playbackStop(id);
            else
                m_model->playbackStart(id);
        });

        // Click row to select
        row->installEventFilter(this);
    }

    outerVbox->addLayout(m_slotLayout, 1);

    // Control buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(3);

    m_recBtn = new QPushButton(QString::fromUtf8("● REC"));
    m_recBtn->setCheckable(true);
    m_recBtn->setStyleSheet(QString(kBtnStyle) +
        "QPushButton:checked { background: #cc3333; color: #fff; }");
    btnRow->addWidget(m_recBtn);

    m_playBtn = new QPushButton(QString::fromUtf8("► PLAY"));
    m_playBtn->setCheckable(true);
    m_playBtn->setStyleSheet(QString(kBtnStyle) +
        "QPushButton:checked { background: #33aa33; color: #fff; }");
    btnRow->addWidget(m_playBtn);

    m_prevBtn = new QPushButton(QString::fromUtf8("◄ PREV"));
    m_prevBtn->setCheckable(true);
    m_prevBtn->setStyleSheet(QString(kBtnStyle) +
        "QPushButton:checked { background: #3388cc; color: #fff; }");
    btnRow->addWidget(m_prevBtn);

    outerVbox->addLayout(btnRow);

    // Status label
    m_statusLabel = new QLabel("Status: Idle");
    m_statusLabel->setStyleSheet("QLabel { color: #6a8090; font-size: 10px; }");
    outerVbox->addWidget(m_statusLabel);

    // Wire buttons
    connect(m_recBtn, &QPushButton::clicked, this, [this](bool checked) {
        if (m_selectedSlot < 1) return;
        if (checked) m_model->recStart(m_selectedSlot);
        else         m_model->recStop(m_selectedSlot);
    });

    connect(m_playBtn, &QPushButton::clicked, this, [this](bool checked) {
        if (m_selectedSlot < 1) return;
        if (checked) m_model->playbackStart(m_selectedSlot);
        else         m_model->playbackStop(m_selectedSlot);
    });

    connect(m_prevBtn, &QPushButton::clicked, this, [this](bool checked) {
        if (m_selectedSlot < 1) return;
        if (checked) m_model->previewStart(m_selectedSlot);
        else         m_model->previewStop(m_selectedSlot);
    });

    // Wire model signals
    connect(m_model, &DvkModel::statusChanged, this, &DvkPanel::onStatusChanged);
    connect(m_model, &DvkModel::recordingChanged, this, &DvkPanel::onRecordingChanged);

    // F1-F12 hotkeys
    for (int i = 0; i < 12; ++i) {
        auto* sc = new QShortcut(QKeySequence(Qt::Key_F1 + i), this);
        connect(sc, &QShortcut::activated, this, [this, i]() {
            int id = i + 1;
            if (m_model->status() == DvkModel::Playback && m_model->activeId() == id)
                m_model->playbackStop(id);
            else
                m_model->playbackStart(id);
        });
    }

    // Escape to stop
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(esc, &QShortcut::activated, this, [this]() {
        int id = m_model->activeId();
        if (id < 0) return;
        switch (m_model->status()) {
        case DvkModel::Recording: m_model->recStop(id); break;
        case DvkModel::Playback:  m_model->playbackStop(id); break;
        case DvkModel::Preview:   m_model->previewStop(id); break;
        default: break;
        }
    });

    // Select first slot by default
    m_selectedSlot = 1;
}

bool DvkPanel::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        // Find which slot row was clicked
        for (int i = 0; i < 12; ++i) {
            if (obj == m_fkeyBtns[i]->parentWidget()) {
                selectSlot(i + 1);
                return true;
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DvkPanel::selectSlot(int id)
{
    m_selectedSlot = id;
    for (int i = 0; i < 12; ++i) {
        auto* container = m_fkeyBtns[i]->parentWidget();
        bool selected = (i + 1 == id);
        container->setStyleSheet(selected
            ? "QWidget { background: #1a2a4a; border: 1px solid #00b4d8; border-radius: 3px; }"
            : kSlotStyle);
    }
}

int DvkPanel::selectedSlot() const
{
    return m_selectedSlot;
}

void DvkPanel::onStatusChanged(int status, int id)
{
    auto s = static_cast<DvkModel::Status>(status);

    m_recBtn->blockSignals(true);
    m_playBtn->blockSignals(true);
    m_prevBtn->blockSignals(true);

    m_recBtn->setChecked(s == DvkModel::Recording);
    m_playBtn->setChecked(s == DvkModel::Playback);
    m_prevBtn->setChecked(s == DvkModel::Preview);

    m_recBtn->blockSignals(false);
    m_playBtn->blockSignals(false);
    m_prevBtn->blockSignals(false);

    // Highlight active slot's F-key button during playback/recording
    for (int i = 0; i < m_fkeyBtns.size(); ++i) {
        bool active = (i + 1 == id) && (s == DvkModel::Playback || s == DvkModel::Recording || s == DvkModel::Preview);
        m_fkeyBtns[i]->setStyleSheet(active
            ? "QPushButton { background: #00b4d8; color: #000; border: 1px solid #00b4d8; "
              "border-radius: 3px; font-size: 10px; font-weight: bold; min-width: 28px; max-width: 28px; }"
            : kFKeyStyle);
    }

    switch (s) {
    case DvkModel::Idle:      m_statusLabel->setText("Status: Idle"); break;
    case DvkModel::Recording: m_statusLabel->setText(QString("Status: Recording %1").arg(id)); break;
    case DvkModel::Preview:   m_statusLabel->setText(QString("Status: Preview %1").arg(id)); break;
    case DvkModel::Playback:  m_statusLabel->setText(QString("Status: Playback %1").arg(id)); break;
    case DvkModel::Disabled:  m_statusLabel->setText("Status: Disabled (SmartSDR+ required)"); break;
    default:                  m_statusLabel->setText("Status: Unknown"); break;
    }
}

void DvkPanel::onRecordingChanged(int id)
{
    if (id < 1 || id > 12) return;
    int idx = id - 1;
    const auto& recs = m_model->recordings();
    for (const auto& r : recs) {
        if (r.id == id) {
            m_nameLabels[idx]->setText(r.name);
            m_durLabels[idx]->setText(r.durationMs > 0 ? formatDuration(r.durationMs) : "Empty");
            // Dim empty slots
            m_nameLabels[idx]->setStyleSheet(r.durationMs > 0
                ? kNameStyle
                : "QLabel { color: #505060; font-size: 10px; background: transparent; border: none; }");
            break;
        }
    }
}

QString DvkPanel::formatDuration(int ms)
{
    int secs = ms / 1000;
    int frac = (ms % 1000) / 100;
    return QString("%1.%2s").arg(secs).arg(frac);
}

} // namespace AetherSDR
