#pragma once

// system includes
#include <cstdint>

// Qt includes
#include <QLocalServer>
#include <QTcpServer>
#include <QSet>
#include <QList>
#include <QStringList>

// Hyperion includes
#include <hyperion/Hyperion.h>

// hyperion includes
#include <utils/Image.h>
#include <utils/ColorRgb.h>
#include <utils/GrabbingMode.h>
#include <utils/VideoMode.h>

// forward decl
class ProtoClientConnection;
class ProtoConnection;

namespace proto {
class HyperionRequest;
}

///
/// This class creates a TCP server which accepts connections wich can then send
/// in Protocol Buffer encoded commands. This interface to Hyperion is used by
/// hyperion-remote to control the leds
///
class ProtoServer : public QObject
{
	Q_OBJECT

public:
	///
	/// ProtoServer constructor
	/// @param hyperion Hyperion instance
	/// @param port port number on which to start listening for connections
	///
	ProtoServer(Hyperion * hyperion, const std::string &addr);
	~ProtoServer();

  const QString &getAddr() const;

public slots:
	void sendImageToProtoSlaves(int priority, const Image<ColorRgb> & image, int duration_ms);

signals:
	///
	/// Forwarding XBMC Checker
	///
	void grabbingMode(const GrabbingMode mode);
	void videoMode(const VideoMode VideoMode);

private slots:
	///
	/// Slot which is called when a client tries to create a new connection
	///
	void newConnection();

	///
	/// Slot which is called when a client closes a connection
	/// @param connection The Connection object which is being closed
	///
	void closedConnection(ProtoClientConnection * connection);

	void newMessage(const proto::HyperionRequest * message);

private:
	/// Hyperion instance
	Hyperion * _hyperion;

  QString _addr;

	/// The TCP server object
  bool _tcp;
	QTcpServer _tcpServer;
  QLocalServer _localServer;

	/// List with open connections
	QSet<ProtoClientConnection *> _openConnections;
	QStringList _forwardClients;

	/// Hyperion proto connection object for forwarding
	QList<ProtoConnection*> _proxy_connections;

};
