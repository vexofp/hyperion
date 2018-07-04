
// Local-Hyperion includes
#include "LedDeviceLocalSocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>

static int sockfd;
static int ledprotocol;
static unsigned leds_per_pkt;
static int update_number;
static int fragment_number;

LedDeviceLocalSocket::LedDeviceLocalSocket(const std::string& output, const unsigned baudrate, const unsigned protocol, const unsigned maxPacket) 
//LedDeviceLocalSocket::LedDeviceLocalSocket(const std::string& output, const unsigned baudrate) :
//	_ofs(output.empty()?"/home/pi/LedDevice.out":output.c_str())
{
	std::string hostname;
	std::string port;
	ledprotocol = protocol;
	leds_per_pkt = ((maxPacket-4)/3);
	if (leds_per_pkt <= 0) {
		leds_per_pkt = 200;
	}

  sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "talker: failed to create socket\n");
		assert(sockfd>=0);
	}

  struct sockaddr_un sun = {0};
  strncpy(sun.sun_path, output.c_str(), sizeof(sun.sun_path));
  int connectrc = connect(sockfd, (struct sockaddr*)&sun, sizeof(sun));
	if (connectrc) {
    close(sockfd);
		fprintf(stderr, "talker: failed to connect socket\n");
		assert(!connectrc);
	}
}

LedDeviceLocalSocket::~LedDeviceLocalSocket()
{
	// empty
}

int LedDeviceLocalSocket::write(const std::vector<ColorRgb> & ledValues)
{

	char udpbuffer[4096];
	int udpPtr=0;

	update_number++;
	update_number &= 0xf;

	if (ledprotocol == 0) {
		int i=0;
		for (const ColorRgb& color : ledValues)
		{
			if (i<4090) {
				udpbuffer[i++] = color.red;
				udpbuffer[i++] = color.green;
				udpbuffer[i++] = color.blue;
			}
			//printf ("c.red %d sz c.red %d\n", color.red, sizeof(color.red));
		}
		send(sockfd, udpbuffer, i, 0);
	}
	if (ledprotocol == 1) {
#define MAXLEDperFRAG 450
		int mLedCount = ledValues.size();

		for (int frag=0; frag<4; frag++) {
			udpPtr=0;
			udpbuffer[udpPtr++] = 0;
			udpbuffer[udpPtr++] = 0;
			udpbuffer[udpPtr++] = (frag*MAXLEDperFRAG)/256;	// high byte
			udpbuffer[udpPtr++] = (frag*MAXLEDperFRAG)%256;	// low byte
			int ct=0;
			for (int this_led = frag*300; ((this_led<mLedCount) && (ct++<MAXLEDperFRAG)); this_led++) {
				const ColorRgb& color = ledValues[this_led];
				if (udpPtr<4090) {
					udpbuffer[udpPtr++] = color.red;
					udpbuffer[udpPtr++] = color.green;
					udpbuffer[udpPtr++] = color.blue;
				}
			}
			if (udpPtr > 7)
				send(sockfd, udpbuffer, udpPtr, 0);
		}
	}
	if (ledprotocol == 2) {
		udpPtr = 0;
		unsigned int ledCtr = 0;
		fragment_number = 0;
		udpbuffer[udpPtr++] = update_number & 0xf;
		udpbuffer[udpPtr++] = fragment_number++;
		udpbuffer[udpPtr++] = ledCtr/256;	// high byte
		udpbuffer[udpPtr++] = ledCtr%256;	// low byte

		for (const ColorRgb& color : ledValues)
		{
			if (udpPtr<4090) {
				udpbuffer[udpPtr++] = color.red;
				udpbuffer[udpPtr++] = color.green;
				udpbuffer[udpPtr++] = color.blue;
			}
			ledCtr++;
			if ( (ledCtr % leds_per_pkt == 0) || (ledCtr == ledValues.size()) ) {
				send(sockfd, udpbuffer, udpPtr, 0);
				memset(udpbuffer, 0, sizeof udpbuffer);
				udpPtr = 0;
				udpbuffer[udpPtr++] = update_number & 0xf;
				udpbuffer[udpPtr++] = fragment_number++;
				udpbuffer[udpPtr++] = ledCtr/256;	// high byte
				udpbuffer[udpPtr++] = ledCtr%256;	// low byte
			}
		}
	}

	if (ledprotocol == 3) {
		udpPtr = 0;
		unsigned int ledCtr = 0;
		unsigned int fragments = 1;
		unsigned int datasize = ledValues.size() * 3;
		if (ledValues.size() > leds_per_pkt) {
			fragments = (ledValues.size() / leds_per_pkt) + 1;
		}
		fragment_number = 1;
		udpbuffer[udpPtr++] = 0x9C;
		udpbuffer[udpPtr++] = 0xDA;
		udpbuffer[udpPtr++] = datasize/256;	// high byte
		udpbuffer[udpPtr++] = datasize%256;	// low byte
		udpbuffer[udpPtr++] = fragment_number++;
		udpbuffer[udpPtr++] = fragments;

		for (const ColorRgb& color : ledValues)
		{
			if (udpPtr<4090) {
				udpbuffer[udpPtr++] = color.red;
				udpbuffer[udpPtr++] = color.green;
				udpbuffer[udpPtr++] = color.blue;
			}
			ledCtr++;
			if ( (ledCtr % leds_per_pkt == 0) || (ledCtr == ledValues.size()) ) {
				udpbuffer[udpPtr++] = 0x36;
				send(sockfd, udpbuffer, udpPtr, 0);
				memset(udpbuffer, 0, sizeof udpbuffer);
				udpPtr = 0;
				udpbuffer[udpPtr++] = 0x9C;
				udpbuffer[udpPtr++] = 0xDA;
				udpbuffer[udpPtr++] = datasize/256;	// high byte
				udpbuffer[udpPtr++] = datasize%256;	// low byte
				udpbuffer[udpPtr++] = fragment_number++;
				udpbuffer[udpPtr++] = fragments;
			}
		}
	}

	return 0;
}

int LedDeviceLocalSocket::switchOff()
{
//		return write(std::vector<ColorRgb>(mLedCount, ColorRgb{0,0,0}));
	return 0;
}
