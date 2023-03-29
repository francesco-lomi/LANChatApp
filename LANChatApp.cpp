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
#include <conio.h>
#include <exception>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

class wrongIP : public exception {
	virtual const char* what() const throw() {
		return "Wrong IP address!";
	}
};

void setCursor(int x, int y) {
	COORD coords = {};
	coords.X = x;
	coords.Y = y;
	SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coords);
}

void clear() {
	system("cls");
}

COORD gcsbi() {
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	return csbi.dwSize;
}

int main(int argc, char const* argv[]) {
	WSADATA wsaData;
	SOCKET defSocket = INVALID_SOCKET;
	sockaddr_in service{};
	wstring IPAddr = L"127.0.0.1";
	int port = 54321;
	enum { unchoosen, client, server } mode = unchoosen;

	SetWindowText(GetForegroundWindow(), L"LANChatApp");

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

	// Prompt the user to select operation mode
	wcout << L"Select whether to connect to a known host or wait for incoming connections:" << endl;
	wcout << L"\tPress 1 - Connect to an IP address" << endl;
	wcout << L"\tPress 2 - Accept any incoming connection" << endl;

	while (mode == unchoosen) {
		switch (_getch()) {
		case '1':
			mode = client;
			clear();
			wcout << L"Type in the desired IP address to connect to: ";
			wcin >> IPAddr;
			break;
		case '2':
			mode = server;
			break;
		default:
			clear();
			wcout << L"Select whether to connect to a known host or wait for incoming connections:" << endl;
			wcout << L"\tPress 1 - Connect to an IP address" << endl;
			wcout << L"\tPress 2 - Accept any incoming connection" << endl;

			wcout << L"\a\nChoose a correct option!";
		}
	}

	try {
		// The sockaddr_in structure specifies the address family,
		// IP address, and port for the socket that is being bound or connected to.
		service.sin_family = AF_INET;
		if (InetPton(AF_INET, IPAddr.c_str(), &service.sin_addr.s_addr) != 1) { throw wrongIP(); }
		service.sin_port = htons(port);

		switch (mode) {
		case client:
			if (connect(defSocket, (sockaddr*)&service, sizeof(service))) { throw - 1; }
			break;
		case server:
			if (bind(defSocket, (sockaddr*)&service, sizeof(service))) { throw - 1; }
			if (listen(defSocket, 1)) { throw - 1; }
			SOCKET serverSocket = accept(defSocket, NULL, NULL);
			if (serverSocket == INVALID_SOCKET) { throw 1; }
			break;
		}
	}
	catch (int e) {
		wcerr << L"Error (" << e << L"): " << WSAGetLastError();

		WSACleanup();
		closesocket(defSocket);
		return EXIT_FAILURE;
	}
	catch (exception& e) {
		wcerr << L"Error: " << e.what();

		WSACleanup();
		closesocket(defSocket);
		return EXIT_FAILURE;
	}

	WSACleanup();
	closesocket(defSocket);
	return EXIT_SUCCESS;
}