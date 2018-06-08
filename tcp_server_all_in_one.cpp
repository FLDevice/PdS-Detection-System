//#include "stdafx.h"

#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
//#include "ProbePacket.h"

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define PACKET_SIZE 52
#define DEFAULT_PORT "3010"

#pragma once

#include <cstdint>
#include <vector>

class ProbePacket {

public:

	unsigned timestamp : 32;	//4  bytes +
	unsigned channel : 8;		//1  bytes +
	unsigned seq_ctl : 16;		//2  bytes +
	signed rssi : 8;			//1  bytes +
	uint8_t addr[6];			//6  bytes +
	uint8_t ssid_length;		//1  bytes +
	uint8_t ssid[32];			//32 bytes +
	uint8_t crc[4];				//4  bytes =
								//_________
								//51 bytes	
	ProbePacket();
	~ProbePacket();

	void print();
};

ProbePacket::ProbePacket(){
}


ProbePacket::~ProbePacket(){
}

void ProbePacket::print(){
	printf("%08d  PROBE  CHAN=%02d,  SEQ=%04x,  RSSI=%02d, "
		" ADDR=%02x:%02x:%02x:%02x:%02x:%02x,  ",
		timestamp,
		channel,
		seq_ctl/*[0], seq_ctl[1]*/,
		rssi,
		addr[0], addr[1], addr[2],
		addr[3], addr[4], addr[5]
	);
	printf("SSID=");
	for (int i = 0; i<ssid_length; i++)
		printf("%c", (char)ssid[i]);
	printf("  CRC=");
	for (int i = 0; i<4; i++)
		printf("%02x", crc[i]);
	printf("\n");
}


std::vector<ProbePacket> pp_vector;

int __cdecl main(void)
{
	WSADATA wsaData;
	int iResult;

	SOCKET ListenSocket = INVALID_SOCKET;
	SOCKET ClientSocket = INVALID_SOCKET;

	struct addrinfo *result = NULL;
	struct addrinfo hints;

	int iSendResult;
	char *recvbuf;

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		return 1;
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port
	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		printf("getaddrinfo failed with error: %d\n", iResult);
		WSACleanup();
		return 1;
	}

	// Create a SOCKET for connecting to server
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// Setup the TCP listening socket
	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	iResult = listen(ListenSocket, SOMAXCONN);
	if (iResult == SOCKET_ERROR) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}
	printf("TCP server running\n");
	
	/* Loop to client's connection requests */
	while (1) {
		// Accept a client socket
		ClientSocket = accept(ListenSocket, NULL, NULL);
		if (ClientSocket == INVALID_SOCKET) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}
		printf("--- New request accepted from client\n");

		char c;
		uint8_t count;
		/* receive the bytes that contains the number of incoming packets */
		recv(ClientSocket, &c, 1, 0);
		count = c - '0';
		printf("Receiving %u packets\n", count);

		/* create space for the incoming packets */
		recvbuf = (char*)malloc(count*PACKET_SIZE);
		int recvbuflen = count * PACKET_SIZE;

		/* receive the effective packets */
		for (int i = 0; i < recvbuflen ; i = i + iResult)
			iResult = recv(ClientSocket, recvbuf+i, recvbuflen-i, 0);
			
		//iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
		if (iResult > 0) {
			printf("Bytes received: %d, buffer contains: \n", iResult);
			printf("Buffer size: %d \n", recvbuflen);
			
			/* store and print them */
			for (int i = 0; i < count; i++) {
				ProbePacket pp;
				memcpy(&pp, recvbuf+(i*PACKET_SIZE), PACKET_SIZE);
				pp_vector.push_back(pp);
				printf("%d \t", i);
				pp.print();
			}
				
			/* Just send back a packet to the client as ack */
			iSendResult = send(ClientSocket, recvbuf, PACKET_SIZE, 0);
			if (iSendResult == SOCKET_ERROR) {
				int err = WSAGetLastError();
				printf("send failed with error: %d\n", err);
				closesocket(ClientSocket);
				/* connection reset by peer */
				if (err == 10054)
					continue;
				WSACleanup();
				return 1;
			}
			printf("Bytes sent back: %d\n", iSendResult);
		}
		else if (iResult == 0)
			printf("Connection closing\n");
		else {
			int err = WSAGetLastError();
			printf("recv failed with error: %d\n", err);
			closesocket(ClientSocket);
			/* connection reset by peer */
			if (err == 10054)
				continue;
			WSACleanup();
			return 1;
		}

		free(recvbuf);

	
	}
	// shutdown the connection since we're done
	iResult = shutdown(ClientSocket, SD_SEND);
	if (iResult == SOCKET_ERROR) {
		printf("shutdown failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		WSACleanup();
		return 1;
	}

	// cleanup
	closesocket(ClientSocket);
	WSACleanup();

	return 0;
}
