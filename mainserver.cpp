#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>//WinSock header files

#pragma comment(lib, "WS2_32.lib")//WinSock library link instruction

void main()
{
	//WinSock object creation
	WSADATA wsaData;
	//Initialising WinSock version 2.2
	int WSResult;
	WSResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (WSResult != 0)
	{
		std::cout << "WinSock startup failed on error code: " << WSResult << std::endl;
		return;
	}
	//Setting up the socket before bind
	struct addrinfo* result = NULL, * ptr = NULL, hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	//Using the defined socket to validate the socket information and set up the socket
	WSResult = getaddrinfo(NULL, "8080", &hints, &result);
	if (WSResult != 0)
	{
		std::cout << "getaddrinfo function failed on error code " << WSResult << std::endl;
		return;
	}
	//Creating a listening socket
	SOCKET ListenSocket = INVALID_SOCKET;
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET)
	{
		//If socket is invalid, free the address and terminates the WinSock API
		std::cout << "Listening socket creation failed " << WSAGetLastError() << std::endl;
		freeaddrinfo(result);
		WSACleanup();
		return;
	}
	//Binding the socket to the server IP address and the port
	WSResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (WSResult == SOCKET_ERROR)
	{
		std::cout << "socket bind failed with error " << WSAGetLastError() << std::endl;
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return;
	}
	//Free up memory being used to store address info
	freeaddrinfo(result);
}
