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
#include <iostream>
#include <fstream>
#include <locale>
#include <thread>
#include <exception>
#include <string>
#include <conio.h>

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
	sockaddr_in service;
	int port = 54321;
	wstring IPAddr = L"";
	service.sin_family = AF_INET;
	service.sin_port = htons(port);
	SOCKET acceptSocket = INVALID_SOCKET;

	SetWindowText(GetForegroundWindow(), L"LANChatApp");

	// Initialize Winsock
	int errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (errorCode != 0) {
		auto err = WSAGetLastError();
		LPWSTR bufPtr = NULL;

		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&bufPtr), 0, NULL);
		wcerr << L"Error (" << err << L"): " << bufPtr << endl;

		return EXIT_FAILURE;
	}

	// Prompt the user to select operation mode
	wcout << L"Select whether to connect to a known host or wait for incoming connections:" << endl;
	wcout << L"\tPress 1 - Connect to an IP address" << endl;
	wcout << L"\tPress 2 - Accept any incoming connection" << endl;

	try {
		while (acceptSocket == INVALID_SOCKET) {
			switch (_getch()) {
			case '1':
				acceptSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (acceptSocket == INVALID_SOCKET) { throw INVALID_SOCKET; }

				clear();

				wcout << L"Type in the desired IP address to connect to: ";
				wcin >> IPAddr;

				if (InetPton(AF_INET, IPAddr.c_str(), &service.sin_addr.s_addr) != 1) { throw wrongIP(); }

				wcout << L"Trying to connect to " << IPAddr << "..." << endl;
				if (connect(acceptSocket, reinterpret_cast<sockaddr*>(&service), sizeof(service))) { throw - 1; }

				break;
			case '2': {
				service.sin_addr.s_addr = INADDR_ANY;
				SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (listenSocket == INVALID_SOCKET) { throw INVALID_SOCKET; }

				if (bind(listenSocket, reinterpret_cast<sockaddr*>(&service), sizeof(service))) { throw - 1; }
				if (listen(listenSocket, 1)) { throw - 1; }

				sockaddr_in clientAddr;
				int clientAddrLen = sizeof(clientAddr);

				wcout << endl << L"Waiting for connections..." << endl;
				acceptSocket = accept(listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
				closesocket(listenSocket);
				if (acceptSocket == INVALID_SOCKET) { throw INVALID_SOCKET; }

				break;
				}
			default:
				clear();
				wcout << L"Select whether to connect to a known host or wait for incoming connections:" << endl;
				wcout << L"\tPress 1 - Connect to an IP address" << endl;
				wcout << L"\tPress 2 - Accept any incoming connection" << endl;

				wcout << L"\a\nChoose a correct option!";
				break;
			}
		}
	}
	catch (int e) {
		int err = WSAGetLastError();
		LPWSTR bufPtr = NULL;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&bufPtr), 0, NULL);

		wcerr << endl << L"Error (" << e << L"): " << bufPtr << L"Code: " << err << endl;

		closesocket(acceptSocket);
		WSACleanup();
		return EXIT_FAILURE;
	}
	catch (const exception& e) {
		wcerr << endl << L"Error: " << e.what() << endl;

		closesocket(acceptSocket);
		WSACleanup();
		return EXIT_FAILURE;
	}

	WSACleanup();
	closesocket(acceptSocket);
	return EXIT_SUCCESS;
}