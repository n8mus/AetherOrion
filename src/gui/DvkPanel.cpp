#include "DvkPanel.h"
#include "models/DvkModel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QShortcut>

namespace AetherSDR {

static const char* kBtnStyle =
    "QPushButton { background: #1a2a3a; color: #c8d8e8; border: 1px solid #203040; "
    "border-radius: 3px; padding: 4px 8px; font-size: 11px; font-weight: bold; }"
    "QPushButton:hover { background: #253545; }"
    "QPushButton:checked { background: #00b4d8; color: #000; }";

static const char* kRecActive =
    "QPushButton:checked { background: #cc3333; color: #fff; }";

static const char* kPlayActive =
    "QPushButton:checked { background: #33aa33; color: #fff; }";

static const char* kPrevActive =
    "QPushButton:checked { background: #3388cc; color: #fff; }";

DvkPanel::DvkPanel(DvkModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    // Title
    auto* title = new QLabel("DVK");
    title->setStyleSheet("QLabel { color: #00b4d8; font-weight: bold; font-size: 14px; }");
    vbox->addWidget(title);

    // Slot list
    m_slotList = new QListWidget;
    m_slotList->setStyleSheet(
        "QListWidget { background: #0a0a14; color: #c8d8e8; border: 1px solid #203040; "
        "font-size: 11px; }"
        "QListWidget::item { padding: 3px 4px; }"
        "QListWidget::item:selected { background: #1a3a5a; }");
    m_slotList->setFixedHeight(300);
    vbox->addWidget(m_slotList);

    // Stretch pushes controls to bottom
    vbox->addStretch();

    // Control buttons
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(3);

    m_recBtn = new QPushButton("● REC");
    m_recBtn->setCheckable(true);
    m_recBtn->setStyleSheet(QString(kBtnStyle) + kRecActive);
    btnRow->addWidget(m_recBtn);

    m_playBtn = new QPushButton("► PLAY");
    m_playBtn->setCheckable(true);
    m_playBtn->setStyleSheet(QString(kBtnStyle) + kPlayActive);
    btnRow->addWidget(m_playBtn);

    m_prevBtn = new QPushButton("🔊 PREV");
    m_prevBtn->setCheckable(true);
    m_prevBtn->setStyleSheet(QString(kBtnStyle) + kPrevActive);
    btnRow->addWidget(m_prevBtn);

    vbox->addLayout(btnRow);

    // Status label
    m_statusLabel = new QLabel("Status: Idle");
    m_statusLabel->setStyleSheet("QLabel { color: #6a8090; font-size: 10px; }");
    vbox->addWidget(m_statusLabel);

    // Wire buttons
    connect(m_recBtn, &QPushButton::clicked, this, [this](bool checked) {
        int slot = selectedSlot();
        if (slot < 0) return;
        if (checked)
            m_model->recStart(slot);
        else
            m_model->recStop(slot);
    });

    connect(m_playBtn, &QPushButton::clicked, this, [this](bool checked) {
        int slot = selectedSlot();
        if (slot < 0) return;
        if (checked)
            m_model->playbackStart(slot);
        else
            m_model->playbackStop(slot);
    });

    connect(m_prevBtn, &QPushButton::clicked, this, [this](bool checked) {
        int slot = selectedSlot();
        if (slot < 0) return;
        if (checked)
            m_model->previewStart(slot);
        else
            m_model->previewStop(slot);
    });

    // Wire model signals
    connect(m_model, &DvkModel::statusChanged, this, &DvkPanel::onStatusChanged);
    connect(m_model, &DvkModel::recordingChanged, this, &DvkPanel::onRecordingChanged);
    connect(m_model, &DvkModel::recordingsLoaded, this, &DvkPanel::rebuildList);

    // F1-F12 hotkeys for playback
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
}

int DvkPanel::selectedSlot() const
{
    auto* item = m_slotList->currentItem();
    if (!item) return -1;
    return item->data(Qt::UserRole).toInt();
}

void DvkPanel::onStatusChanged(int status, int id)
{
    Q_UNUSED(id);
    auto s = static_cast<DvkModel::Status>(status);

    // Update button checked states without triggering clicks
    m_recBtn->blockSignals(true);
    m_playBtn->blockSignals(true);
    m_prevBtn->blockSignals(true);

    m_recBtn->setChecked(s == DvkModel::Recording);
    m_playBtn->setChecked(s == DvkModel::Playback);
    m_prevBtn->setChecked(s == DvkModel::Preview);

    m_recBtn->blockSignals(false);
    m_playBtn->blockSignals(false);
    m_prevBtn->blockSignals(false);

    // Status label
    switch (s) {
    case DvkModel::Idle:      m_statusLabel->setText("Status: Idle"); break;
    case DvkModel::Recording: m_statusLabel->setText(QString("Status: Recording %1").arg(id)); break;
    case DvkModel::Preview:   m_statusLabel->setText(QString("Status: Preview %1").arg(id)); break;
    case DvkModel::Playback:  m_statusLabel->setText(QString("Status: Playback %1").arg(id)); break;
    case DvkModel::Disabled:  m_statusLabel->setText("Status: Disabled (SmartSDR+ required)"); break;
    default:                  m_statusLabel->setText("Status: Unknown"); break;
    }

    updateButtons();
}

void DvkPanel::onRecordingChanged(int id)
{
    // Update the specific item in the list
    for (int i = 0; i < m_slotList->count(); ++i) {
        auto* item = m_slotList->item(i);
        if (item->data(Qt::UserRole).toInt() == id) {
            const auto& recs = m_model->recordings();
            for (const auto& r : recs) {
                if (r.id == id) {
                    QString dur = r.durationMs > 0 ? formatDuration(r.durationMs) : "Empty";
                    item->setText(QString("F%1  %2  [%3]").arg(id).arg(r.name, -20).arg(dur));
                    break;
                }
            }
            break;
        }
    }
}

void DvkPanel::rebuildList()
{
    m_slotList->clear();
    const auto& recs = m_model->recordings();
    for (const auto& r : recs) {
        QString dur = r.durationMs > 0 ? formatDuration(r.durationMs) : "Empty";
        auto* item = new QListWidgetItem(
            QString("F%1  %2  [%3]").arg(r.id).arg(r.name, -20).arg(dur));
        item->setData(Qt::UserRole, r.id);
        if (r.durationMs == 0)
            item->setForeground(QColor("#505060"));
        m_slotList->addItem(item);
    }
    if (m_slotList->count() > 0)
        m_slotList->setCurrentRow(0);
}

void DvkPanel::updateButtons()
{
    bool idle = (m_model->status() == DvkModel::Idle);
    bool hasSlot = selectedSlot() > 0;
    m_recBtn->setEnabled(idle && hasSlot);
    m_playBtn->setEnabled(idle && hasSlot);
    m_prevBtn->setEnabled(idle && hasSlot);
}

QString DvkPanel::formatDuration(int ms)
{
    int secs = ms / 1000;
    int frac = (ms % 1000) / 100;
    return QString("%1.%2s").arg(secs).arg(frac);
}

} // namespace AetherSDR
