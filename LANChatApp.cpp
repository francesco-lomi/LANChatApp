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
#include <mswsock.h>
#include <iostream>
#include <fstream>
#include <locale>
#include <cstring>
#include <thread>
#include <mutex>
#include <exception>
#include <string>
#include <conio.h>
#include <regex>
#include <commdlg.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

#define BYTES_PER_SEND 1024

using namespace std;

class wrongIP : public exception {
	virtual const char* what() const throw() {
		return "Wrong IP address!";
	}
};

class commDlgError : public exception {
	virtual const char* what() const throw() {
		char dwordChar[9];
		sprintf_s(dwordChar, "%08X", CommDlgExtendedError());
		return dwordChar;
	}
};

class fileError : public exception {
	virtual const char* what() const throw() {
		int err = GetLastError();
		LPWSTR wstr = NULL;
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&wstr), 0, NULL);

		int size = WideCharToMultiByte(CP_UTF8, NULL, wstr, static_cast<int>(wcslen(wstr)), nullptr, NULL, nullptr, nullptr);
		char* cstr = new char[size + 1];
		cstr[size] = '\0';

		WideCharToMultiByte(CP_UTF8, NULL, wstr, static_cast<int>(wcslen(wstr)), cstr, size, nullptr, nullptr);

		return cstr;
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

SOCKET acceptSocket = INVALID_SOCKET;
bool isRunning = true;
mutex iostreamMutex{}, WSAMutex{};
exception_ptr tException = nullptr;

char hostName[NI_MAXHOST];
char serviceName[NI_MAXSERV];
WCHAR ipAddress[INET_ADDRSTRLEN];

class dataClass {
public:
	enum typE { text, file, confirmFile, startFile, cancelFile, endFile, quit } type = dataClass::text;
	wchar_t contents[1024];

	dataClass() {
		ZeroMemory(&contents, sizeof(contents));
	}

	dataClass(typE constrType) {
		ZeroMemory(&contents, sizeof(contents));
		type = constrType;
	}

	friend std::wostream& operator<<(std::wostream& wos, const dataClass& obj) {
		wos << "[" << hostName << "] " << obj.contents << endl;
		return wos;
	}
};

class fileClass {
public:
	fileClass() {
		ZeroMemory(&fileTitle, sizeof(fileTitle));
		ZeroMemory(&fileSize, sizeof(fileSize));
	}

	wchar_t fileTitle[64];
	LARGE_INTEGER fileSize;
};

void recvFile(SOCKET& socket) {
	unique_lock<mutex> iosLockF(iostreamMutex);
	unique_lock<mutex> wsaLockF(WSAMutex);

	u_long mode = 0;
	if (ioctlsocket(acceptSocket, FIONBIO, &mode) != NO_ERROR)
		throw WSAError();

	dataClass initData(dataClass::confirmFile);
	if (send(socket, reinterpret_cast<char*>(&initData), sizeof(dataClass), NULL) == SOCKET_ERROR)
		throw WSAError();

	fileClass fileData;
	if (recv(socket, reinterpret_cast<char*>(&fileData), sizeof(fileClass), NULL) == SOCKET_ERROR)
		throw WSAError();

	wcout << L"[FILE] Incoming file transfer..." << endl;
	iosLockF.unlock();

	wchar_t fileName[MAX_PATH];
	ZeroMemory(&fileName, sizeof(fileName));
	wcscpy_s(fileName, L"%userprofile%\\Downloads\\");
	wcscat_s(fileName, fileData.fileTitle);

	wchar_t fileTitle[64];
	ZeroMemory(&fileTitle, sizeof(fileTitle));

	OPENFILENAME commOFN;
	ZeroMemory(&commOFN, sizeof(commOFN));

	commOFN.lStructSize = sizeof(OPENFILENAME);
	commOFN.hwndOwner = GetForegroundWindow();
	commOFN.lpstrFilter = L"All Files\0*.*\0\0";
	commOFN.nFilterIndex = 1;
	commOFN.lpstrFile = fileName;
	commOFN.nMaxFile = MAX_PATH;
	commOFN.lpstrFileTitle = fileTitle;
	commOFN.nMaxFileTitle = 64;
	commOFN.lpstrInitialDir = L"%userprofile%\\Downloads\\";
	commOFN.lpstrTitle = NULL;
	commOFN.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

	if (!GetSaveFileName(&commOFN)) {
		if (!CommDlgExtendedError()) {
			iosLockF.lock();
			wcout << L"[INFO] You closed the \"Save As\" windows without choosing save location, cancelling operation." << endl;
			iosLockF.unlock();
			initData.type = dataClass::cancelFile;
			send(socket, reinterpret_cast<char*>(&initData), sizeof(dataClass), NULL);
			wsaLockF.unlock();
			return;
		}
		else {
			throw commDlgError();
		}
	}

	HANDLE hFile = CreateFile(fileName, GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		throw fileError();

	initData.type = dataClass::startFile;
	if (send(socket, reinterpret_cast<char*>(&initData), sizeof(dataClass), NULL) == SOCKET_ERROR)
		throw WSAError();

	unsigned char buf[BYTES_PER_SEND];
	ZeroMemory(&buf, BYTES_PER_SEND);
	int bytesReceived = 0;
	int bytesToReceive = 0;
	uint64_t bytesWritten = 0;
	DWORD bytesWrittenToFile = 0;

	while (bytesWritten < fileData.fileSize.QuadPart) {
		ZeroMemory(&buf, BYTES_PER_SEND);
		bytesToReceive = static_cast<int>(min<uint64_t>(BYTES_PER_SEND, fileData.fileSize.QuadPart - bytesWritten));
		bytesReceived = recv(socket, reinterpret_cast<char*>(&buf), bytesToReceive, NULL);
		if (bytesReceived <= 0)
			throw WSAError();
		if (!WriteFile(hFile, &buf, bytesReceived, &bytesWrittenToFile, NULL) || bytesWrittenToFile != bytesReceived)
			throw fileError();
		bytesWritten += bytesWrittenToFile;
	}

	if (recv(socket, reinterpret_cast<char*>(&initData), sizeof(dataClass), NULL) == SOCKET_ERROR)
		throw WSAError();

	if (initData.type != dataClass::endFile) {
		if (!CloseHandle(hFile))
			throw fileError();
		iosLockF.lock();
		wcout << L"[ERROR] Error receiving file." << endl;
		iosLockF.unlock();
		wsaLockF.unlock();
		return;
	}

	if (!CloseHandle(hFile))
		throw fileError();

	iosLockF.lock();
	wcout << L"[FILE] \"" << fileData.fileTitle << L"\" received successfully." << endl;
	iosLockF.unlock();

	wsaLockF.unlock();
}

void sendFile(SOCKET& socket) {
	unique_lock<mutex> iosLockF(iostreamMutex, defer_lock);
	unique_lock<mutex> wsaLockF(WSAMutex, defer_lock);

	wchar_t fileName[MAX_PATH];
	ZeroMemory(&fileName, sizeof(fileName));
	wchar_t fileTitle[64];
	ZeroMemory(&fileTitle, sizeof(fileTitle));

	OPENFILENAME commOFN;
	ZeroMemory(&commOFN, sizeof(commOFN));

	commOFN.lStructSize = sizeof(OPENFILENAME);
	commOFN.hwndOwner = GetForegroundWindow();
	commOFN.lpstrFilter = L"All Files\0*.*\0\0";
	commOFN.nFilterIndex = 1;
	commOFN.lpstrFile = fileName;
	commOFN.nMaxFile = MAX_PATH;
	commOFN.lpstrFileTitle = fileTitle;
	commOFN.nMaxFileTitle = 64;
	commOFN.lpstrInitialDir = L"%userprofile%";
	commOFN.lpstrTitle = NULL;
	commOFN.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetOpenFileName(&commOFN)) {
		if (!CommDlgExtendedError()) {
			iosLockF.lock();
			wcout << L"[INFO] You closed the \"Open File\" window without choosing a file to send, cancelling operation." << endl;
			iosLockF.unlock();
			return;
		}
		else {
			throw commDlgError();
		}
	}

	HANDLE hFile = CreateFile(fileName, GENERIC_READ, NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		throw fileError();

	wsaLockF.lock();
	u_long mode = 0;
	if (ioctlsocket(acceptSocket, FIONBIO, &mode) != NO_ERROR)
		throw WSAError();

	dataClass initData(dataClass::file);
	if (send(socket, reinterpret_cast<char*>(&initData), sizeof(dataClass), NULL) == SOCKET_ERROR)
		throw WSAError();

	if (recv(socket, reinterpret_cast<char*>(&initData), sizeof(dataClass), NULL) == SOCKET_ERROR)
		throw WSAError();
	if (initData.type != dataClass::confirmFile) {
		if (!CloseHandle(hFile))
			throw fileError();
		iosLockF.lock();
		wcout << L"[ERROR] Error sending file." << endl;
		iosLockF.unlock();
		wsaLockF.unlock();
		return;
	}

	fileClass fileData;
	wcscpy_s(fileData.fileTitle, fileTitle);
	if (!GetFileSizeEx(hFile, &fileData.fileSize)) {
		throw fileError();
	}

	if (send(socket, reinterpret_cast<char*>(&fileData), sizeof(fileClass), NULL) == SOCKET_ERROR)
		throw WSAError();

	if (recv(socket, reinterpret_cast<char*>(&initData), sizeof(dataClass), NULL) == SOCKET_ERROR)
		throw WSAError();

	if (initData.type == dataClass::cancelFile) {
		if (!CloseHandle(hFile))
			throw fileError();
		iosLockF.lock();
		wcout << L"[INFO] File transfer cancelled by the recipient." << endl;
		iosLockF.unlock();
		wsaLockF.unlock();
		return;
	}
	else if (initData.type != dataClass::startFile) {
		if (!CloseHandle(hFile))
			throw fileError();
		iosLockF.lock();
		wcout << L"[ERROR] Error sending file." << endl;
		iosLockF.unlock();
		wsaLockF.unlock();
		return;
	}

	iosLockF.lock();
	wcout << L"[FILE] Outgoing file transfer..." << endl;
	
	unsigned char buf[BYTES_PER_SEND];
	ZeroMemory(&buf, BYTES_PER_SEND);
	DWORD bytesRead;
	if (!ReadFile(hFile, &buf, BYTES_PER_SEND, &bytesRead, NULL))
		throw fileError();

	while (bytesRead > 0) {
		if (send(socket, reinterpret_cast<char*>(&buf), bytesRead, NULL) == SOCKET_ERROR)
			throw WSAError();
		ZeroMemory(&buf, BYTES_PER_SEND);
		if (!ReadFile(hFile, &buf, BYTES_PER_SEND, &bytesRead, NULL))
			throw fileError();
	}
	
	if (!CloseHandle(hFile))
		throw fileError();

	initData.type = dataClass::endFile;
	if (send(socket, reinterpret_cast<char*>(&initData), sizeof(dataClass), NULL) == SOCKET_ERROR)
		throw WSAError();
	
	wcout << L"[FILE] \"" << fileTitle << L"\" sent successfully." << endl;
	iosLockF.unlock();
	wsaLockF.unlock();
}

void clear() {
	system("cls");
}

void help() {
	wcout << L"[INFO] Press: i - type a message (max 1023 characters), f - send a file, h - display this help info, - clear screen, q - quit" << endl;
}

int prepareMessage(const wstring input, dataClass* data) {
	wstring result;
	for (wchar_t c : input) {
		switch (c) {
		case L'\n':
			result += L"\\n";
			break;
		case L'\r':
			result += L"\\r";
			break;
		case L'\t':
			result += L"\\t";
			break;
		case L'\b':
			result += L"\\b";
			break;
		case L'\f':
			result += L"\\f";
			break;
		//case L'\\':
		//	result += L"\\\\";
		//	break;
		default:
			if (c >= 0 && c <= 31) {
				result += L"\\x";
				result += (c / 16 < 10 ? (c / 16 + L'0') : (c / 16 - 10 + L'A'));
				result += (c % 16 < 10 ? (c % 16 + L'0') : (c % 16 - 10 + L'A'));
			}
			else {
				result += c;
			}
		}
	}

	if (result.length() > 1023)
		return 1;

	wcsncpy_s(data->contents, result.c_str(), 1023);
	data->contents[1023] = L'\0';

	return 0;
}

void recvThreadf() {
	try {
		unique_lock<mutex> iosLockT(iostreamMutex, defer_lock);
		unique_lock<mutex> wsaLockT(WSAMutex, defer_lock);
		u_long mode;
		dataClass recvData;

		while (isRunning) {
			wsaLockT.lock();
			mode = 1;
			if (ioctlsocket(acceptSocket, FIONBIO, &mode) != NO_ERROR)
				throw WSAError();
			if (recv(acceptSocket, reinterpret_cast<char*>(&recvData), sizeof(dataClass), NULL) == SOCKET_ERROR) {
				if (WSAGetLastError() == WSAEWOULDBLOCK) {
					mode = 0;
					if (ioctlsocket(acceptSocket, FIONBIO, &mode) != NO_ERROR)
						throw WSAError();
					wsaLockT.unlock();
				}
				else {
					throw WSAError();
				}
			}
			else {
				mode = 0;
				if (ioctlsocket(acceptSocket, FIONBIO, &mode) != NO_ERROR)
					throw WSAError();
				wsaLockT.unlock();

				switch (recvData.type) {
					case dataClass::quit: {
						isRunning = false;

						iosLockT.lock();
						wcout << L"[QUIT] " << hostName << L" quitted." << endl;
						iosLockT.unlock();
						break;
					}
					case dataClass::file: {
						recvFile(acceptSocket);
						break;
					}
					case dataClass::text: {
						iosLockT.lock();
						wcout << recvData;
						iosLockT.unlock();
						break;
					}
				}
			}
			Sleep(200);
		}
	}
	catch (const exception& e) {
		isRunning = false;
		tException = current_exception();
	}
}

int main(int argc, char const* argv[]) {
	WSADATA wsaData;
	int port = 54321;
	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_port = htons(port);

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
	wcout << L"[INFO] Connected to: " << hostName << L" - " << serviceName << L":" << ipAddress << endl;
	help();

	try {
		thread recvThread(recvThreadf);

		unique_lock<mutex> iosLock(iostreamMutex, defer_lock);
		unique_lock<mutex> wsaLock(WSAMutex, defer_lock);

		while (isRunning) {
			switch (_getch()) {
				case 'i': {
					if (!isRunning) { break; }
					dataClass sendData;
					wstring inputMessage;

					iosLock.lock();
					wcout << L"[YOU] ";
					getline((wcin >> ws), inputMessage);

					if (prepareMessage(inputMessage, &sendData)) {
						wcout << L"[INFO] After securing your message of any exploits its size exceeded the maximum message size of 1023 characters." << endl;
						iosLock.unlock();
						break;
					}
					iosLock.unlock();

					wsaLock.lock();
					if (send(acceptSocket, reinterpret_cast<char*>(&sendData), sizeof(dataClass), NULL) == SOCKET_ERROR)
						throw WSAError();
					wsaLock.unlock();

					break;
				}
				case 'f': {
					if (!isRunning) { break; }
					sendFile(acceptSocket);
					break;
				}
				case 'h': {
					if (!isRunning) { break; }
					
					iosLock.lock();
					help();
					iosLock.unlock();
					break;
				}
				case 'c': {
					iosLock.lock();
					clear();
					iosLock.unlock();
				}
				case 'q': {
					if (!isRunning) { break; }
					isRunning = false;

					dataClass sendData;
					sendData.type = dataClass::quit;

					wsaLock.lock();
					if (send(acceptSocket, reinterpret_cast<char*>(&sendData), sizeof(sendData), NULL) == SOCKET_ERROR)
						throw WSAError();
					wsaLock.unlock();

					wcout << L"[QUIT] You quitted." << endl;
					break;
				}
				default: {
					if (!isRunning) { break; }
					wcout << L"\a[INFO] Press a correct button (h for help)" << endl;
					break;
				}
			}
		}
		recvThread.join();
		if (tException != nullptr) { rethrow_exception(tException); }
	}
	catch (const exception& e) {
		wcerr << endl << L"\aError: " << e.what() << "Windows error code: " << GetLastError() << endl;

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
