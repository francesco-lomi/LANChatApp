#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

int main() {
	WSADATA wsaData;
	SOCKET defSocket = INVALID_SOCKET;
	sockaddr_in service;
	int iResult;


	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		cerr << L"WSAStartup failed, error code: " << iResult;
		return 1;
	}

	// Create a SOCKET for listening for incoming connection requests
	defSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (defSocket == INVALID_SOCKET) {
		cerr << L"Socket function failed with error: " << WSAGetLastError();
		WSACleanup();
		return 1;
	}

	// The sockaddr_in structure specifies the address family,
	// IP address, and port for the socket that is being bound.
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr("127.0.0.1");
	service.sin_port = htons(27015);

	return 0;
}