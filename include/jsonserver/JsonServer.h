#pragma once

// system includes
#include <cstdint>

// Qt includes
#include <QTcpServer>
#include <QLocalServer>
#include <QSet>

// Hyperion includes
#include <hyperion/Hyperion.h>

class JsonClientConnection;

///
/// This class creates a TCP server which accepts connections wich can then send
/// in JSON encoded commands. This interface to Hyperion is used by hyperion-remote
/// to control the leds
///
class JsonServer : public QObject
{
	Q_OBJECT

public:
	///
	/// JsonServer constructor
	/// @param hyperion Hyperion instance
	/// @param port port number on which to start listening for connections
	///
	JsonServer(Hyperion * hyperion, const std::string &addr);
	~JsonServer();

	///
	/// @return the port number on which this TCP listens for incoming connections
	///
	const QString& getAddr() const;

private slots:
	///
	/// Slot which is called when a client tries to create a new connection
	///
	void newConnection();

	///
	/// Slot which is called when a client closes a connection
	/// @param connection The Connection object which is being closed
	///
	void closedConnection(JsonClientConnection * connection);

private:
	/// Hyperion instance
	Hyperion * _hyperion;

  QString _addr;

	/// The TCP server object
  bool _tcp;
	QTcpServer _tcpServer;
  QLocalServer _localServer;

	/// List with open connections
	QSet<JsonClientConnection *> _openConnections;
};
