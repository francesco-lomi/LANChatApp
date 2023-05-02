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
#include <mutex>
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

class WSAError : public exception {
	virtual const char* what() const throw() {
		int err = WSAGetLastError();
		LPWSTR wstr = NULL;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&wstr), 0, NULL);

		int size = WideCharToMultiByte(CP_UTF8, NULL, wstr, static_cast<int>(wcslen(wstr)), nullptr, NULL, nullptr, nullptr);
		char* cstr = new char[size + 1];
		cstr[size] = '\0';

		WideCharToMultiByte(CP_UTF8, NULL, wstr, static_cast<int>(wcslen(wstr)), cstr, size, nullptr, nullptr);
		
		return cstr;
	}
};

mutex iostreamMutex{}, WSAMutex{};
char hostName[NI_MAXHOST];
char serviceName[NI_MAXSERV];
WCHAR ipAddress[INET_ADDRSTRLEN];

class dataClass {
public:
	enum { text, file, quit } type = dataClass::text;

	wchar_t contents[1024];

	friend std::wostream& operator<<(std::wostream& wos, const dataClass& obj) {
		wos << "[" << hostName << "] " << obj.contents << endl;
		return wos;
	}
};
void clear() {
	system("cls");
}

void help() {
	wcout << L"[INFO] Press: i - type a message (max 1023 characters), f - send a file, h - display this help info, q - quit" << endl;
}

int main(int argc, char const* argv[]) {
	WSADATA wsaData;
	int port = 54321;
	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_port = htons(port);
	SOCKET acceptSocket = INVALID_SOCKET;

	SetWindowText(GetForegroundWindow(), L"LANChatApp");

	// Initialize Winsock
	int errorCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (errorCode != 0) {
		auto err = WSAGetLastError();
		LPWSTR bufPtr = NULL;

		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&bufPtr), NULL, NULL);
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
			case '1': {
				wstring IPAddr = L"";
				acceptSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (acceptSocket == INVALID_SOCKET) { throw WSAError(); }

				clear();

				wcout << L"Type in the desired IP address to connect to: ";
				wcin >> IPAddr;

				if (InetPton(AF_INET, IPAddr.c_str(), &service.sin_addr.s_addr) != 1) { throw wrongIP(); }

				wcout << L"Trying to connect to " << IPAddr << "..." << endl;
				if (connect(acceptSocket, reinterpret_cast<sockaddr*>(&service), sizeof(service))) { throw WSAError(); }

				InetNtop(AF_INET, &service.sin_addr, ipAddress, INET_ADDRSTRLEN);
				getnameinfo(reinterpret_cast<sockaddr*>(&service), sizeof(service), hostName, NI_MAXHOST, serviceName, NI_MAXSERV, NI_NUMERICSERV);

				break;
			}
			case '2': {
				service.sin_addr.s_addr = INADDR_ANY;
				SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (listenSocket == INVALID_SOCKET) { throw WSAError();; }

				if (bind(listenSocket, reinterpret_cast<sockaddr*>(&service), sizeof(service))) { throw WSAError(); }
				if (listen(listenSocket, 1)) { throw WSAError(); }

				sockaddr_in clientAddr;
				int clientAddrLen = sizeof(clientAddr);

				wcout << endl << L"Waiting for connections..." << endl;
				acceptSocket = accept(listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
				closesocket(listenSocket);
				if (acceptSocket == INVALID_SOCKET) { throw WSAError(); }

				InetNtop(AF_INET, &clientAddr.sin_addr, ipAddress, INET_ADDRSTRLEN);
				getnameinfo(reinterpret_cast<sockaddr*>(&clientAddr), sizeof(clientAddr), hostName, NI_MAXHOST, serviceName, NI_MAXSERV, NI_NUMERICSERV);

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
	catch (const exception& e) {
		wcerr << endl << L"\aError: " << e.what() << endl;

		closesocket(acceptSocket);
		WSACleanup();
		return EXIT_FAILURE;
	}
	catch (...) {
		wcerr << endl << L"\aUnknown error occurred" << endl;

		closesocket(acceptSocket);
		WSACleanup();
		return EXIT_FAILURE;
	}

	clear();
	wcout << L"[INFO] Connected to: " << serviceName << L":" << ipAddress << L" - " << hostName << endl;
	help();

	try {
		unique_lock<mutex> iosLock(iostreamMutex, defer_lock);
		unique_lock<mutex> wsaLock(WSAMutex, defer_lock);
	}
	catch (const exception& e) {
		wcerr << endl << L"\aError: " << e.what() << "Code: " << GetLastError() << endl;

		closesocket(acceptSocket);
		WSACleanup();
		system("pause >nul");
		return 1;
	}
	catch (...) {
		wcerr << endl << L"\aUnknown error occurred" << endl;

		closesocket(acceptSocket);
		WSACleanup();
		system("pause >nul");
		return 1;
	}

	closesocket(acceptSocket);
	WSACleanup();
	system("pause >nul");
	return 0;
}