// stl includes
#include <stdexcept>

// Qt includes
#include <QRgb>
#include <QRegExp>

// protoserver includes
#include "protoserver/ProtoConnection.h"

ProtoConnection::ProtoConnection(const std::string & a) :
  _addr(a.c_str()),
	_skipReply(false),
	_sockConnected(false)
{
	_timer.setInterval(5000);
	_timer.setSingleShot(false);

  QRegExp hostPortRegExp(":[0-9]{1,5}$");
  if(_addr.contains(hostPortRegExp))
  {
    _socket.io = new QTcpSocket();
    connectToTcpHost();

	  connect(&_timer,SIGNAL(timeout()), this, SLOT(connectToTcpHost()));
  }
  else
  {
    _socket.io = new QLocalSocket();
    connectToLocalHost();

	  connect(&_timer,SIGNAL(timeout()), this, SLOT(connectToLocalHost()));
  }

	connect(_socket.io, SIGNAL(readyRead()), this, SLOT(readData()));
	_timer.start();
}

ProtoConnection::~ProtoConnection()
{
	_timer.stop();
	_socket.io->close();
  delete _socket.io;
}

void ProtoConnection::readData()
{	
	_receiveBuffer += _socket.io->readAll();

	// check if we can read a message size
	if (_receiveBuffer.size() <= 4)
	{
		return;
	}

	// read the message size
	uint32_t messageSize =
			((_receiveBuffer[0]<<24) & 0xFF000000) |
			((_receiveBuffer[1]<<16) & 0x00FF0000) |
			((_receiveBuffer[2]<< 8) & 0x0000FF00) |
			((_receiveBuffer[3]    ) & 0x000000FF);

	// check if we can read a complete message
	if ((uint32_t) _receiveBuffer.size() < messageSize + 4)
	{
		return;
	}
	
	// read a message
	proto::HyperionReply reply;
	
	if (!reply.ParseFromArray(_receiveBuffer.data() + 4, messageSize))
	{
		std::cerr << "PROTOCONNECTION ERROR: Unable to parse message" << std::endl;
		return;
	}
	
	parseReply(reply);
	
	// remove message data from buffer
	_receiveBuffer = _receiveBuffer.mid(messageSize + 4);
}

void ProtoConnection::setSkipReply(bool skip)
{
	_skipReply = skip;
}

void ProtoConnection::setColor(const ColorRgb & color, int priority, int duration)
{
	proto::HyperionRequest request;
	request.set_command(proto::HyperionRequest::COLOR);
	proto::ColorRequest * colorRequest = request.MutableExtension(proto::ColorRequest::colorRequest);
	colorRequest->set_rgbcolor((color.red << 16) | (color.green << 8) | color.blue);
	colorRequest->set_priority(priority);
	colorRequest->set_duration(duration);

	// send command message
	sendMessage(request);
}

void ProtoConnection::setImage(const Image<ColorRgb> &image, int priority, int duration)
{
	proto::HyperionRequest request;
	request.set_command(proto::HyperionRequest::IMAGE);
	proto::ImageRequest * imageRequest = request.MutableExtension(proto::ImageRequest::imageRequest);
	imageRequest->set_imagedata(image.memptr(), image.width() * image.height() * 3);
	imageRequest->set_imagewidth(image.width());
	imageRequest->set_imageheight(image.height());
	imageRequest->set_priority(priority);
	imageRequest->set_duration(duration);

	// send command message
	sendMessage(request);
}

void ProtoConnection::clear(int priority)
{
	proto::HyperionRequest request;
	request.set_command(proto::HyperionRequest::CLEAR);
	proto::ClearRequest * clearRequest = request.MutableExtension(proto::ClearRequest::clearRequest);
	clearRequest->set_priority(priority);

	// send command message
	sendMessage(request);
}

void ProtoConnection::clearAll()
{
	proto::HyperionRequest request;
	request.set_command(proto::HyperionRequest::CLEARALL);

	// send command message
	sendMessage(request);
}

void ProtoConnection::connectToTcpHost()
{
	// try connection only when 
	if (!_socket.io->isOpen())
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

    // try to connect to host
    std::cout << "PROTOCONNECTION INFO: Connecting to Hyperion: " << host.toStdString() << ":" << port << std::endl;

    _socket.tcp->connectToHost(host, port);
	}
}

void ProtoConnection::connectToLocalHost()
{
	// try connection only when 
	if (!_socket.io->isOpen())
	{
    // try to connect to host
    std::cout << "PROTOCONNECTION INFO: Connecting to Hyperion: " << _addr.toStdString() << std::endl;

    _socket.local->connectToServer(_addr);
	}
}

void ProtoConnection::sendMessage(const proto::HyperionRequest &message)
{
	// print out connection message only when state is changed
	if (_socket.io->isOpen() != _sockConnected )
	{
	  if (!_socket.io->isOpen())
    {
		  std::cout << "PROTOCONNECTION INFO: No connection to Hyperion: " << _addr.toStdString() << std::endl;
    }
    else
    {
		  std::cout << "PROTOCONNECTION INFO: Connected to Hyperion: " << _addr.toStdString() << std::endl;
    }

	  _sockConnected = _socket.io->isOpen();
	}


	if (!_socket.io->isOpen())
	{
		return;
	}

	// We only get here if we are connected

	// serialize message (FastWriter already appends a newline)
	std::string serializedMessage = message.SerializeAsString();

	int length = serializedMessage.size();
	const uint8_t header[] = {
		uint8_t((length >> 24) & 0xFF),
		uint8_t((length >> 16) & 0xFF),
		uint8_t((length >>  8) & 0xFF),
		uint8_t((length	  ) & 0xFF)};

	// write message
	int count = 0;
	count += _socket.io->write(reinterpret_cast<const char *>(header), 4);
	count += _socket.io->write(reinterpret_cast<const char *>(serializedMessage.data()), length);
	_socket.io->waitForBytesWritten(count);
	if (count != length + 4)
	{
		std::cerr << "PROTOCONNECTION ERROR: Error while writing data to host" << std::endl;
		return;
	}
}

bool ProtoConnection::parseReply(const proto::HyperionReply &reply)
{
	bool success = false;
	
	switch (reply.type())
	{
		case proto::HyperionReply::REPLY:
		{
			if (!_skipReply)
			{
				if (!reply.success())
				{
					if (reply.has_error())
					{
						throw std::runtime_error("PROTOCONNECTION ERROR: " + reply.error());
					}
					else
					{
						throw std::runtime_error("PROTOCONNECTION ERROR: No error info");
					}
				}
				else
				{
					success = true;
				}
			}
			break;
		}
		case proto::HyperionReply::GRABBING:
		{
			int grabbing = reply.has_grabbing() ? reply.grabbing() : 6;
			GrabbingMode gMode = (GrabbingMode)grabbing;
			emit setGrabbingMode(gMode);
			break;
		}
		case proto::HyperionReply::VIDEO:
		{
			int video = reply.has_video() ? reply.video() : 0;
			VideoMode vMode = (VideoMode)video;
			emit setVideoMode(vMode);
			break;
		}
	}
	
	return success;
}
