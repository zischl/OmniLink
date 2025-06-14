#include "SessionHandler.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string>

#pragma comment(lib, "Ws2_32.lib")


int sessions::_init_winsock() {
	int wsResult;
	wsResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (wsResult != 0) {
		return 1;
	}
	return 0;
}

sockaddr_in sessions::_create_address(PCSTR IP, unsigned short port) {
	sockaddr_in address;
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	inet_pton(AF_INET, IP, &address.sin_addr);
	return address;
}

SOCKET sessions::_create_socket() {
	int wsResult;
	SOCKET socketR = INVALID_SOCKET;

	socketR = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketR == INVALID_SOCKET) {
		WSACleanup();
		return 1;
	}
	return socketR;


}
int sessions::_requestHandshake() {
	return 0;
}
int sessions::_verifyHandshake() {
	return 0;
}
int sessions::_establishLink() {
	return 0;
}
int  sessions::openSession() {
	return 0;
}