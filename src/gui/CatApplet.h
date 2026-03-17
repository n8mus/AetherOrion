#pragma once

#include <QWidget>

class QPushButton;
class QLabel;
class QLineEdit;
class QComboBox;
class QSlider;
class QProgressBar;

namespace AetherSDR {

class RadioModel;
class SliceModel;
class RigctlServer;
class RigctlPty;
class AudioEngine;

// CAT Applet — settings panel for rigctld TCP server, virtual serial port,
// and DAX audio channel management.
class CatApplet : public QWidget {
    Q_OBJECT

public:
    explicit CatApplet(QWidget* parent = nullptr);

    void setRadioModel(RadioModel* model);
    void setRigctlServer(RigctlServer* server);
    void setRigctlPty(RigctlPty* pty);
    void setAudioEngine(AudioEngine* audio);

    // Sync Enable button state (called by MainWindow on autostart)
    void setTcpEnabled(bool on);
    void setPtyEnabled(bool on);

private:
    void buildUI();
    void updateTcpStatus();
    void updatePtyStatus();
    void onConnectionStateChanged(bool connected);
    void wireSliceDax(SliceModel* s);
    void updateDaxSliceAssignments();
    void updateDaxTxStatus();

    RadioModel*    m_model{nullptr};
    RigctlServer*  m_server{nullptr};
    RigctlPty*     m_pty{nullptr};
    AudioEngine*   m_audio{nullptr};

    // TCP section
    QPushButton* m_tcpEnable{nullptr};
    QLineEdit*   m_tcpPort{nullptr};
    QLabel*      m_tcpStatus{nullptr};

    // PTY section
    QPushButton* m_ptyEnable{nullptr};
    QLineEdit*   m_ptyPath{nullptr};

    // Slice selector
    QComboBox*   m_sliceSelect{nullptr};

    // DAX section
    QPushButton*  m_daxEnable{nullptr};
    QProgressBar* m_daxRxLevel[4]{};
    QLabel*       m_daxRxStatus[4]{};
    QProgressBar* m_daxTxLevel{nullptr};
    QLabel*       m_daxTxStatus{nullptr};
};

} // namespace AetherSDR
