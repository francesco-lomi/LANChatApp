#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

void setCursor(int x, int y) {
	COORD coords;
	coords.X = x;
	coords.Y = y;
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coords);
}

COORD gcsbi() {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	return csbi.dwSize;
}

int main() {

	WSADATA wsaData;
	SOCKET defSocket = INVALID_SOCKET;
	sockaddr_in service;

	try {
		// Initialize Winsock
		int errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (errorCode != 0) {
			cerr << L"WSAStartup failed, error code: " << errorCode;
			return EXIT_FAILURE;
		}

		// Create a SOCKET for listening for incoming connection requests
		defSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (defSocket == INVALID_SOCKET) {
			cerr << L"Socket function failed with error: " << WSAGetLastError();
			WSACleanup();
			return EXIT_FAILURE;
		}


		// The sockaddr_in structure specifies the address family,
		// IP address, and port for the socket that is being bound.
		service.sin_family = AF_INET;
		errorCode = InetPton(AF_INET, L"127.0.0.1", &service.sin_addr.s_addr);
		if (!errorCode)
			throw errorCode;
		service.sin_port = htons(27015);
	}
	catch (int e) {
		cerr << "Error: " << WSAGetLastError;
		WSACleanup();
		closesocket(defSocket);
		return EXIT_FAILURE;
	}
	catch (exception &e) {
		cerr << "Unknown error: " << e.what();
		WSACleanup();
		closesocket(defSocket);
		return EXIT_FAILURE;
	}

	WSACleanup();
	closesocket(defSocket);
	return 0;
}