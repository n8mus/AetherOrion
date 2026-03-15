#pragma once

#include "CommandParser.h"
#include "RadioDiscovery.h"

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include <QString>
#include <QHostAddress>
#include <QTimer>
#include <functional>
#include <atomic>

namespace AetherSDR {

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

// Manages the TCP connection to a FlexRadio and provides the
// command/response layer of the SmartSDR API.
//
// Usage:
//   RadioConnection conn;
//   conn.connectToRadio(radioInfo);
//   conn.sendCommand("sub slice all");
//   connect(&conn, &RadioConnection::statusReceived, this, &MyClass::onStatus);
class RadioConnection : public QObject {
    Q_OBJECT
    Q_PROPERTY(ConnectionState state READ state NOTIFY stateChanged)

public:
    explicit RadioConnection(QObject* parent = nullptr);
    ~RadioConnection() override;

    ConnectionState state() const       { return m_state; }
    quint32 clientHandle() const        { return m_handle; }
    bool isConnected() const            { return m_state == ConnectionState::Connected; }
    QHostAddress radioAddress() const   { return m_socket.peerAddress(); }
    quint16      localTcpPort() const   { return m_socket.localPort(); }

    // Connect to a discovered radio
    void connectToRadio(const RadioInfo& info);
    // Connect directly by address/port
    void connectToHost(const QHostAddress& address, quint16 port = 4992);
    void disconnectFromRadio();

    // Send a command, returns the sequence number assigned.
    // Optional callback is called when the R response arrives.
    using ResponseCallback = std::function<void(int resultCode, const QString& body)>;
    quint32 sendCommand(const QString& command,
                        ResponseCallback callback = nullptr);

signals:
    void stateChanged(ConnectionState state);
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);

    // Emitted for every parsed incoming line
    void messageReceived(const ParsedMessage& msg);

    // Convenience signals for common message types
    void statusReceived(const QString& object, const QMap<QString, QString>& kvs);
    void versionReceived(const QString& version);

private slots:
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReadyRead();
    void onHeartbeat();

private:
    void processLine(const QString& line);
    void setState(ConnectionState s);

    QTcpSocket  m_socket;
    QByteArray  m_readBuffer;
    QTimer      m_heartbeat;

    ConnectionState m_state{ConnectionState::Disconnected};
    quint32 m_handle{0};
    std::atomic<quint32> m_seqCounter{1};
    quint32 m_lastPingSeq{0};

    QMap<quint32, ResponseCallback> m_pendingCallbacks;
};

} // namespace AetherSDR
