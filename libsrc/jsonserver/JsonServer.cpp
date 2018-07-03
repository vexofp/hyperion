// system includes
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>

//Qt includes
#include <QTcpSocket>
#include <QLocalSocket>

// project includes
#include <jsonserver/JsonServer.h>
#include "JsonClientConnection.h"

JsonServer::JsonServer(Hyperion *hyperion, const std::string &addr) :
	QObject(),
	_hyperion(hyperion),
  _addr(addr.c_str()),
	_tcp(false),
	_openConnections()
{

		/*QList<MessageForwarder::JsonSlaveAddress> list = _hyperion->getForwarder()->getJsonSlaves();
		for ( int i=0; i<list.size(); i++ )
		{
			if ( list.at(i).addr == QHostAddress::LocalHost && list.at(i).port == port ) {
				throw std::runtime_error("JSONSERVER ERROR: Loop between proto server and forwarder detected. Fix your config!");
			}
		}*/

  QRegExp portRegExp("^[0-9]{1,5}$");
  QRegExp hostPortRegExp(":[0-9]{1,5}$");

  if(_addr.contains(portRegExp))
  {
    bool ok;
    uint16_t port = _addr.toUShort(&ok);
	  if (!ok)
	  {
	  	throw std::runtime_error("PROTOSERVER ERROR: Could not parse port number");
	  }

	  if (!_tcpServer.listen(QHostAddress::Any, port))
	  {
	  	throw std::runtime_error("PROTOSERVER ERROR: Could not bind to port");
	  }

  	// Set trigger for incoming connections
	  connect(&_tcpServer, SIGNAL(newConnection()), this, SLOT(newConnection()));
    _tcp = true;
  }
  else if(_addr.contains(portRegExp))
  {
    QStringList parts = _addr.split(":");
    if (parts.size() != 2)
    {
      throw std::runtime_error(QString("PROTOCONNECTION ERROR: Wrong address: Unable to parse address (%1)").arg(_addr).toStdString());
    }
    QString host = parts[0];

    bool ok;
    uint16_t port = parts[1].toUShort(&ok);
    if (!ok)
    {
      throw std::runtime_error(QString("PROTOCONNECTION ERROR: Wrong port: Unable to parse the port number (%1)").arg(parts[1]).toStdString());
    }

	  if (!_tcpServer.listen(QHostAddress(host), port))
	  {
	  	throw std::runtime_error("PROTOSERVER ERROR: Could not bind to port");
	  }

  	// Set trigger for incoming connections
	  connect(&_tcpServer, SIGNAL(newConnection()), this, SLOT(newConnection()));
    _tcp = true;
  }
  else
  {
    mode_t oldUmask = umask(0007);
	  if (!_localServer.listen(_addr))
	  {
	  	throw std::runtime_error("PROTOSERVER ERROR: Could not bind to socket");
	  }
    umask(oldUmask);
    //
  	// Set trigger for incoming connections
	  connect(&_localServer, SIGNAL(newConnection()), this, SLOT(newConnection()));
  }

	// make sure the resources are loaded (they may be left out after static linking
	Q_INIT_RESOURCE(JsonSchemas);

}

JsonServer::~JsonServer()
{
	foreach (JsonClientConnection * connection, _openConnections) {
		delete connection;
	}
}

const QString& JsonServer::getAddr() const
{
	return _addr;
}

void JsonServer::newConnection()
{
	QIODevice *socket = _tcp ? static_cast<QIODevice*>(_tcpServer.nextPendingConnection()) : static_cast<QIODevice *>(_localServer.nextPendingConnection());

	if (socket != nullptr)
	{
		std::cout << "JSONSERVER INFO: New connection" << std::endl;
		JsonClientConnection * connection = new JsonClientConnection(socket, _hyperion);
		_openConnections.insert(connection);

		// register slot for cleaning up after the connection closed
		connect(connection, SIGNAL(connectionClosed(JsonClientConnection*)), this, SLOT(closedConnection(JsonClientConnection*)));
	}
}

void JsonServer::closedConnection(JsonClientConnection *connection)
{
	std::cout << "JSONSERVER INFO: Connection closed" << std::endl;
	_openConnections.remove(connection);

	// schedule to delete the connection object
	connection->deleteLater();
}
