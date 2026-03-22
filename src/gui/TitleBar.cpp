#include "TitleBar.h"
#include "core/AppSettings.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QMenuBar>

namespace AetherSDR {

TitleBar::TitleBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(32);
    setStyleSheet("TitleBar { background: #0a0a14; border-bottom: 1px solid #203040; }");

    m_hbox = new QHBoxLayout(this);
    m_hbox->setContentsMargins(4, 2, 8, 2);
    m_hbox->setSpacing(6);

    // ── Left: menu bar will be inserted here via setMenuBar() ───────────────
    // (placeholder — addStretch pushes center/right into place)
    m_hbox->addStretch(1);

    // ── Center: App name ────────────────────────────────────────────────────
    auto* appName = new QLabel("AetherSDR");
    appName->setStyleSheet("QLabel { color: #00b4d8; font-size: 14px; font-weight: bold; }");
    appName->setAlignment(Qt::AlignCenter);
    m_hbox->addWidget(appName);

    m_hbox->addStretch(1);

    // ── Right: PC Audio + Master Vol + HP Vol ───────────────────────────────
    auto& s = AppSettings::instance();

    // PC Audio toggle
    m_pcBtn = new QPushButton("PC Audio");
    m_pcBtn->setCheckable(true);
    m_pcBtn->setFixedHeight(22);
    m_pcBtn->setFixedWidth(70);

    bool pcOn = s.value("PcAudioEnabled", "True").toString() == "True";
    m_pcBtn->setChecked(pcOn);

    auto updatePcStyle = [this]() {
        m_pcBtn->setStyleSheet(m_pcBtn->isChecked()
            ? "QPushButton { background: #1a6030; color: #40ff80; border: 1px solid #20a040; "
              "border-radius: 3px; font-size: 10px; font-weight: bold; }"
              "QPushButton:hover { background: #207040; }"
            : "QPushButton { background: #1a2a3a; color: #607080; border: 1px solid #304050; "
              "border-radius: 3px; font-size: 10px; font-weight: bold; }"
              "QPushButton:hover { background: #243848; }");
    };
    updatePcStyle();

    connect(m_pcBtn, &QPushButton::toggled, this, [this, updatePcStyle](bool on) {
        updatePcStyle();
        auto& ss = AppSettings::instance();
        ss.setValue("PcAudioEnabled", on ? "True" : "False");
        ss.save();
        emit pcAudioToggled(on);
    });
    m_hbox->addWidget(m_pcBtn);

    m_hbox->addSpacing(8);

    // Master volume
    auto* volIcon = new QLabel("\xF0\x9F\x94\x8A");  // 🔊
    volIcon->setStyleSheet("QLabel { font-size: 14px; }");
    m_hbox->addWidget(volIcon);

    m_masterSlider = new QSlider(Qt::Horizontal);
    m_masterSlider->setRange(0, 100);
    int savedVol = s.value("MasterVolume", "100").toInt();
    m_masterSlider->setValue(savedVol);
    m_masterSlider->setFixedWidth(80);
    m_masterSlider->setFixedHeight(16);
    m_masterSlider->setStyleSheet(
        "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #00b4d8; width: 10px; margin: -3px 0; border-radius: 5px; }"
        "QSlider::sub-page:horizontal { background: #00b4d8; border-radius: 2px; }");
    m_hbox->addWidget(m_masterSlider);

    m_masterLabel = new QLabel(QString::number(savedVol));
    m_masterLabel->setFixedWidth(22);
    m_masterLabel->setStyleSheet("QLabel { color: #8aa8c0; font-size: 10px; }");
    m_masterLabel->setAlignment(Qt::AlignCenter);
    m_hbox->addWidget(m_masterLabel);

    connect(m_masterSlider, &QSlider::valueChanged, this, [this](int v) {
        m_masterLabel->setText(QString::number(v));
        emit masterVolumeChanged(v);
    });

    m_hbox->addSpacing(8);

    // Headphone volume
    auto* hpIcon = new QLabel("\xF0\x9F\x8E\xA7");  // 🎧
    hpIcon->setStyleSheet("QLabel { font-size: 14px; }");
    m_hbox->addWidget(hpIcon);

    m_hpSlider = new QSlider(Qt::Horizontal);
    m_hpSlider->setRange(0, 100);
    m_hpSlider->setValue(50);
    m_hpSlider->setFixedWidth(80);
    m_hpSlider->setFixedHeight(16);
    m_hpSlider->setStyleSheet(
        "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #00b4d8; width: 10px; margin: -3px 0; border-radius: 5px; }"
        "QSlider::sub-page:horizontal { background: #00b4d8; border-radius: 2px; }");
    m_hbox->addWidget(m_hpSlider);

    m_hpLabel = new QLabel("50");
    m_hpLabel->setFixedWidth(22);
    m_hpLabel->setStyleSheet("QLabel { color: #8aa8c0; font-size: 10px; }");
    m_hpLabel->setAlignment(Qt::AlignCenter);
    m_hbox->addWidget(m_hpLabel);

    connect(m_hpSlider, &QSlider::valueChanged, this, [this](int v) {
        m_hpLabel->setText(QString::number(v));
        emit headphoneVolumeChanged(v);
    });
}

void TitleBar::setMenuBar(QMenuBar* mb)
{
    if (!mb) return;
    mb->setStyleSheet(
        "QMenuBar { background: transparent; color: #8aa8c0; font-size: 12px; }"
        "QMenuBar::item { padding: 4px 8px; }"
        "QMenuBar::item:selected { background: #203040; color: #ffffff; }"
        "QMenu { background: #0f0f1a; color: #c8d8e8; border: 1px solid #304050; }"
        "QMenu::item:selected { background: #0070c0; }");
    mb->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    // Insert at position 0 (before the first stretch)
    m_hbox->insertWidget(0, mb);
}

void TitleBar::setPcAudioEnabled(bool on)
{
    QSignalBlocker b(m_pcBtn);
    m_pcBtn->setChecked(on);
    m_pcBtn->setStyleSheet(on
        ? "QPushButton { background: #1a6030; color: #40ff80; border: 1px solid #20a040; "
          "border-radius: 3px; font-size: 10px; font-weight: bold; }"
          "QPushButton:hover { background: #207040; }"
        : "QPushButton { background: #1a2a3a; color: #607080; border: 1px solid #304050; "
          "border-radius: 3px; font-size: 10px; font-weight: bold; }"
          "QPushButton:hover { background: #243848; }");
}

void TitleBar::setMasterVolume(int pct)
{
    QSignalBlocker b(m_masterSlider);
    m_masterSlider->setValue(pct);
    m_masterLabel->setText(QString::number(pct));
}

void TitleBar::setHeadphoneVolume(int pct)
{
    QSignalBlocker b(m_hpSlider);
    m_hpSlider->setValue(pct);
    m_hpLabel->setText(QString::number(pct));
}

} // namespace AetherSDR
