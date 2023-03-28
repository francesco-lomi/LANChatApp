#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#ifdef UNICODE
#define SetWindowText  SetWindowTextW
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

void setCursor(int x, int y) {
	COORD coords;
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
	PCWSTR IPAddr = L"127.0.0.1";
	int port = 54321;
	enum {unchoosen, connect, listen, filter} mode = unchoosen;

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
	wcout << L"\tPress 3 - Wait for and accept a connection only from a chosen IP address" << endl;

	while(mode == unchoosen) {
		switch (_getch()) {
		case '1':
			mode = connect;
			clear();
			wcout << L"Type in the desired IP address to connect to: ";
			break;
		case '2':
			mode = listen;

			break;
		case '3':
			mode = filter;

			break;
		default:
			setCursor(0, 5);
			wcout << L"\aChoose a correct option!";
		}
	}

	try {
		// The sockaddr_in structure specifies the address family,
		// IP address, and port for the socket that is being bound or connected to.
		service.sin_family = AF_INET;
		errorCode = InetPton(AF_INET, IPAddr, &service.sin_addr.s_addr);
		if (!errorCode)
			throw errorCode;
		service.sin_port = htons(port);
	}
	catch (int e) {
		wcerr << L"Error (" << e << L"): " << WSAGetLastError();

		WSACleanup();
		closesocket(defSocket);
		return EXIT_SUCCESS;
	}
	catch (exception& e) {
		wcerr << L"Unknown error: " << e.what();

		WSACleanup();
		closesocket(defSocket);
		return EXIT_SUCCESS;
	}

	WSACleanup();
	closesocket(defSocket);
	return EXIT_SUCCESS;
}