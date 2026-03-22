#include "TitleBar.h"
#include "core/AppSettings.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QDesktopServices>
#include <QClipboard>
#include <QApplication>

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

    m_hbox->addSpacing(12);

    // ── Feature Request button ──────────────────────────────────────────────
    auto* featureBtn = new QPushButton("\xF0\x9F\x92\xA1 Feature Request");  // 💡
    featureBtn->setFixedHeight(24);
    featureBtn->setStyleSheet(
        "QPushButton { background: #3a2a00; color: #ffd060; border: 1px solid #806020; "
        "border-radius: 4px; font-size: 12px; font-weight: bold; padding: 2px 10px; }"
        "QPushButton:hover { background: #504000; color: #ffe080; }");
    connect(featureBtn, &QPushButton::clicked, this, &TitleBar::showFeatureRequestDialog);
    m_hbox->addWidget(featureBtn);
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

void TitleBar::showFeatureRequestDialog()
{
    static const QString kPrompt =
        "Before responding, please read the AetherSDR project context at\n"
        "https://raw.githubusercontent.com/ten9876/AetherSDR/main/CLAUDE.md\n"
        "to understand the project's architecture, existing features, and current roadmap.\n\n"
        "I want to request a feature for AetherSDR, a Linux-native Qt6/C++20 client\n"
        "for FlexRadio transceivers. It uses the FlexLib API over TCP/UDP.\n\n"
        "Before writing the feature request, please check the existing open issues at:\n"
        "https://github.com/ten9876/AetherSDR/issues\n\n"
        "Search for keywords related to my idea. If you find an existing issue that\n"
        "covers the same thing, tell me the issue number and title instead of writing\n"
        "a new one — I'll go add my +1 and comments there.\n\n"
        "If no duplicate exists, please write a GitHub issue for this feature request.\n"
        "Use GitHub-flavored Markdown formatting (headers, code blocks, bullet points).\n"
        "Include:\n"
        "1. A clear title\n"
        "2. A \"What\" section describing what the feature does from the user's perspective\n"
        "3. A \"Why\" section explaining why this is useful (what problem it solves)\n"
        "4. A \"How Other Clients Do It\" section — if this feature exists in other\n"
        "   SDR applications, describe how it works there (screenshots welcome)\n"
        "5. A \"Suggested Behavior\" section with specific details about how it should\n"
        "   work in AetherSDR (what the user clicks, what they see, what happens)\n"
        "6. A \"Protocol Hints\" section — if you know the FlexLib API calls involved,\n"
        "   list them. If not, just say \"Unknown — needs research\"\n\n"
        "Here is my feature idea:\n\n"
        "[Describe your feature here in plain English]";

    QMessageBox dlg(this);
    dlg.setWindowTitle("AI-Assisted Feature Request");
    dlg.setIcon(QMessageBox::Information);
    dlg.setText(
        "<h3>Create a Feature Request with AI</h3>"
        "<p>Use any AI assistant to help you write a detailed, actionable feature request.</p>"
        "<ol>"
        "<li><b>Choose your AI</b> — click one of the buttons below to open it</li>"
        "<li><b>Paste the prompt</b> — it's been copied to your clipboard</li>"
        "<li><b>Describe your idea</b> — edit the [bracketed] section at the end</li>"
        "<li><b>Copy the AI's output</b> and click <b>Submit Your Idea</b> to submit it</li>"
        "</ol>"
        "<p style='color:#8aa8c0; font-size:11px;'>"
        "The prompt asks the AI to check for duplicate issues, reference behavior seen in other applications, "
        "and include protocol hints — everything we need to implement your idea quickly.</p>");

    auto* claudeBtn   = dlg.addButton("Claude", QMessageBox::ActionRole);
    auto* chatgptBtn  = dlg.addButton("ChatGPT", QMessageBox::ActionRole);
    auto* geminiBtn   = dlg.addButton("Gemini", QMessageBox::ActionRole);
    auto* grokBtn     = dlg.addButton("Grok", QMessageBox::ActionRole);
    auto* perplexBtn  = dlg.addButton("Perplexity", QMessageBox::ActionRole);
    auto* issueBtn    = dlg.addButton("Submit Your Idea", QMessageBox::ActionRole);
    dlg.addButton(QMessageBox::Close);

    // Copy prompt to clipboard immediately
    QApplication::clipboard()->setText(kPrompt);

    dlg.exec();

    auto* clicked = dlg.clickedButton();
    if (clicked == claudeBtn) {
        QDesktopServices::openUrl(QUrl("https://claude.ai/new"));
    } else if (clicked == chatgptBtn) {
        QDesktopServices::openUrl(QUrl("https://chat.openai.com/"));
    } else if (clicked == geminiBtn) {
        QDesktopServices::openUrl(QUrl("https://gemini.google.com/"));
    } else if (clicked == grokBtn) {
        QDesktopServices::openUrl(QUrl("https://grok.x.ai/"));
    } else if (clicked == perplexBtn) {
        QDesktopServices::openUrl(QUrl("https://www.perplexity.ai/"));
    } else if (clicked == issueBtn) {
        QDesktopServices::openUrl(QUrl("https://github.com/ten9876/AetherSDR/issues/new"));
    }
}

} // namespace AetherSDR
