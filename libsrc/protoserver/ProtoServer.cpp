// system includes
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>

// Qt includes
#include <QRegExp>

// project includes
#include <hyperion/MessageForwarder.h>
#include <protoserver/ProtoServer.h>
#include "protoserver/ProtoConnection.h"
#include "ProtoClientConnection.h"

ProtoServer::ProtoServer(Hyperion *hyperion, const std::string &addr) :
	QObject(),
	_hyperion(hyperion),
  _addr(addr.c_str()),
  _tcp(false),
	_openConnections()
{

	MessageForwarder * forwarder = hyperion->getForwarder();
	QStringList slaves = forwarder->getProtoSlaves();

	for (int i = 0; i < slaves.size(); ++i) {
		if ( _addr == slaves.at(i) ) {
			throw std::runtime_error("PROTOSERVER ERROR: Loop between proto server and forwarder detected. Fix your config!");
		}

		ProtoConnection* p = new ProtoConnection(slaves.at(i).toLocal8Bit().constData());
		p->setSkipReply(true);
		_proxy_connections << p;
	}

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

}

ProtoServer::~ProtoServer()
{
	foreach (ProtoClientConnection * connection, _openConnections) {
		delete connection;
	}
	
	while (!_proxy_connections.isEmpty())
		delete _proxy_connections.takeFirst();
}

void ProtoServer::newConnection()
{
	QIODevice *socket = _tcp ? static_cast<QIODevice*>(_tcpServer.nextPendingConnection()) : static_cast<QIODevice *>(_localServer.nextPendingConnection());

	if (socket != nullptr)
	{
		std::cout << "PROTOSERVER INFO: New connection" << std::endl;
		ProtoClientConnection * connection = new ProtoClientConnection(socket, _hyperion);
		_openConnections.insert(connection);

		// register slot for cleaning up after the connection closed
		connect(connection, SIGNAL(connectionClosed(ProtoClientConnection*)), this, SLOT(closedConnection(ProtoClientConnection*)));
		connect(connection, SIGNAL(newMessage(const proto::HyperionRequest*)), this, SLOT(newMessage(const proto::HyperionRequest*)));
		
		// register forward signal for xbmc checker
		connect(this, SIGNAL(grabbingMode(GrabbingMode)), connection, SLOT(setGrabbingMode(GrabbingMode)));
		connect(this, SIGNAL(videoMode(VideoMode)), connection, SLOT(setVideoMode(VideoMode)));

	}
}

void ProtoServer::newMessage(const proto::HyperionRequest * message)
{
	for (int i = 0; i < _proxy_connections.size(); ++i)
		_proxy_connections.at(i)->sendMessage(*message);
}

void ProtoServer::sendImageToProtoSlaves(int priority, const Image<ColorRgb> & image, int duration_ms)
{
	for (int i = 0; i < _proxy_connections.size(); ++i)
		_proxy_connections.at(i)->setImage(image, priority, duration_ms);
}

void ProtoServer::closedConnection(ProtoClientConnection *connection)
{
	std::cout << "PROTOSERVER INFO: Connection closed" << std::endl;
	_openConnections.remove(connection);

	// schedule to delete the connection object
	connection->deleteLater();
}

const QString& ProtoServer::getAddr() const
{
  return _addr;
}
