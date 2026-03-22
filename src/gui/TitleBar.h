#pragma once

#include <QWidget>

class QPushButton;
class QSlider;
class QLabel;
class QMenuBar;
class QHBoxLayout;

namespace AetherSDR {

class TitleBar : public QWidget {
    Q_OBJECT

public:
    explicit TitleBar(QWidget* parent = nullptr);

    // Embed the menu bar into the left side of the title bar
    void setMenuBar(QMenuBar* mb);

    void setPcAudioEnabled(bool on);
    void setMasterVolume(int pct);
    void setHeadphoneVolume(int pct);

signals:
    void pcAudioToggled(bool on);
    void masterVolumeChanged(int pct);
    void headphoneVolumeChanged(int pct);

private:
    QHBoxLayout* m_hbox{nullptr};
    QPushButton* m_pcBtn{nullptr};
    QSlider*     m_masterSlider{nullptr};
    QSlider*     m_hpSlider{nullptr};
    QLabel*      m_masterLabel{nullptr};
    QLabel*      m_hpLabel{nullptr};
};

} // namespace AetherSDR
