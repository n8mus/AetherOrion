#pragma once

#include <QObject>
#include <QString>

#ifdef HAVE_SERIALPORT
#include <QSerialPort>
#include <QTimer>
#endif

namespace AetherSDR {

// Controls DTR/RTS lines on a USB-serial adapter for hardware PTT
// and CW keying. Two modes:
//
// Output: Assert DTR/RTS when transmitting (amplifier keying, sequencer)
// Input:  Poll CTS/DSR for external foot switch / PTT button (future)
//
// Requires Qt6::SerialPort. Compiles to a no-op stub without HAVE_SERIALPORT.

class SerialPortController : public QObject {
    Q_OBJECT

public:
    enum class PinFunction { None, PTT, CwKey, CwPTT };

    explicit SerialPortController(QObject* parent = nullptr);
    ~SerialPortController() override;

    bool open(const QString& portName, int baudRate = 9600,
              int dataBits = 8, int parity = 0, int stopBits = 1);
    void close();
    bool isOpen() const;
    QString portName() const;

    void setDtrFunction(PinFunction fn) { m_dtrFn = fn; }
    void setRtsFunction(PinFunction fn) { m_rtsFn = fn; }
    void setDtrPolarity(bool activeHigh) { m_dtrActiveHigh = activeHigh; }
    void setRtsPolarity(bool activeHigh) { m_rtsActiveHigh = activeHigh; }

    PinFunction dtrFunction() const { return m_dtrFn; }
    PinFunction rtsFunction() const { return m_rtsFn; }
    bool dtrPolarity() const { return m_dtrActiveHigh; }
    bool rtsPolarity() const { return m_rtsActiveHigh; }

    // Load/save configuration from AppSettings
    void loadSettings();
    void saveSettings();

public slots:
    // Called when TX state changes — asserts pins configured for PTT
    void setTransmitting(bool tx);

    // Called for CW keying — asserts pins configured for CwKey
    void setCwKeyDown(bool down);

signals:
    // Emitted when an external PTT input is detected (future: poll CTS/DSR)
    void externalPttChanged(bool active);
    void errorOccurred(const QString& msg);

private:
    void applyPin(PinFunction targetFn, bool active);

    PinFunction m_dtrFn{PinFunction::None};
    PinFunction m_rtsFn{PinFunction::None};
    bool m_dtrActiveHigh{true};
    bool m_rtsActiveHigh{true};

#ifdef HAVE_SERIALPORT
    QSerialPort m_port;
#endif
};

} // namespace AetherSDR
