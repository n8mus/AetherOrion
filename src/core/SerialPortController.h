#pragma once

#include <QObject>
#include <QString>

#ifdef HAVE_SERIALPORT
#include <QSerialPort>
#include <QTimer>
#include <QElapsedTimer>
#endif

namespace AetherSDR {

// Controls DTR/RTS lines on a USB-serial adapter for hardware PTT
// and CW keying, and polls CTS/DSR for external PTT input.
//
// Output: Assert DTR/RTS when transmitting (amplifier keying, sequencer)
// Input:  Poll CTS/DSR for external foot switch / PTT button
//
// Requires Qt6::SerialPort. Compiles to a no-op stub without HAVE_SERIALPORT.

class SerialPortController : public QObject {
    Q_OBJECT

public:
    // Output functions (DTR/RTS)
    enum class PinFunction { None, PTT, CwKey, CwPTT };
    // Input functions (CTS/DSR)
    enum class InputFunction { None, PttInput, CwKeyInput };

    explicit SerialPortController(QObject* parent = nullptr);
    ~SerialPortController() override;

    bool open(const QString& portName, int baudRate = 9600,
              int dataBits = 8, int parity = 0, int stopBits = 1);
    void close();
    bool isOpen() const;
    QString portName() const;

    // Output pin config (DTR/RTS)
    void setDtrFunction(PinFunction fn) { m_dtrFn = fn; }
    void setRtsFunction(PinFunction fn) { m_rtsFn = fn; }
    void setDtrPolarity(bool activeHigh) { m_dtrActiveHigh = activeHigh; }
    void setRtsPolarity(bool activeHigh) { m_rtsActiveHigh = activeHigh; }

    PinFunction dtrFunction() const { return m_dtrFn; }
    PinFunction rtsFunction() const { return m_rtsFn; }
    bool dtrPolarity() const { return m_dtrActiveHigh; }
    bool rtsPolarity() const { return m_rtsActiveHigh; }

    // Input pin config (CTS/DSR)
    void setCtsFunction(InputFunction fn) { m_ctsFn = fn; updatePolling(); }
    void setDsrFunction(InputFunction fn) { m_dsrFn = fn; updatePolling(); }
    void setCtsPolarity(bool activeHigh) { m_ctsActiveHigh = activeHigh; }
    void setDsrPolarity(bool activeHigh) { m_dsrActiveHigh = activeHigh; }

    InputFunction ctsFunction() const { return m_ctsFn; }
    InputFunction dsrFunction() const { return m_dsrFn; }
    bool ctsPolarity() const { return m_ctsActiveHigh; }
    bool dsrPolarity() const { return m_dsrActiveHigh; }

    // Load/save configuration from AppSettings
    void loadSettings();
    void saveSettings();

public slots:
    // Called when TX state changes — asserts pins configured for PTT
    void setTransmitting(bool tx);

    // Called for CW keying — asserts pins configured for CwKey
    void setCwKeyDown(bool down);

signals:
    // Emitted when an external PTT input is detected via CTS/DSR polling
    void externalPttChanged(bool active);
    void errorOccurred(const QString& msg);

private:
    void applyPin(PinFunction targetFn, bool active);
    void updatePolling();

    // Output state
    PinFunction m_dtrFn{PinFunction::None};
    PinFunction m_rtsFn{PinFunction::None};
    bool m_dtrActiveHigh{true};
    bool m_rtsActiveHigh{true};

    // Input state
    InputFunction m_ctsFn{InputFunction::None};
    InputFunction m_dsrFn{InputFunction::None};
    bool m_ctsActiveHigh{false};  // default active-low (foot switch to GND)
    bool m_dsrActiveHigh{false};

#ifdef HAVE_SERIALPORT
    QSerialPort m_port;
    QTimer      m_pollTimer;
    bool        m_lastCtsActive{false};
    bool        m_lastDsrActive{false};
    QElapsedTimer m_debounceTimer;
    static constexpr int POLL_INTERVAL_MS = 10;
    static constexpr int DEBOUNCE_MS = 20;

private slots:
    void pollInputPins();
#endif
};

} // namespace AetherSDR
