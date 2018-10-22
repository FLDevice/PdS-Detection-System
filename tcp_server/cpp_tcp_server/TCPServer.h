#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <cstdint>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <vector>
#include <exception>
#include "probepacket.h"
#include "probepacket.cpp"

#pragma comment (lib, "Ws2_32.lib")

#define PACKET_SIZE 52
#define MAX_RETRY_TIMES 3
#define DEFAULT_PORT "3010"

class TCPServer_exception : public virtual std::runtime_error {

public:
	TCPServer_exception(const char* s) : std::runtime_error(s) {}
};

/** To use, just create a TCPServer object. */
class TCPServer{
	
protected:
	
	// dynarray to store esp32 sniffed packets
	std::vector<ProbePacket> pp_vector;
	
	WSADATA wsadata;
	
	// sockets
	SOCKET listen_socket;
	SOCKET client_socket;
	
	addrinfo *aresult = NULL;
	addrinfo hints;
	
	// buffer for received packets
	char* recvbuf;
	
	// current operation's result
	int result;
	// send result
	int send_result;
	
	/** how many times after a socket error the connection is retried
	 before launching an exception */
	uint8_t retry;

	
public:

	TCPServer();
	
private:
	/** initialize winsock, may throw exception */
	void TCPS_initialize();
	
	/** calls socket(), may throw exception */
	void TCPS_socket();
	
	/** calls bind(), may throw exception */
	void TCPS_bind();
	
	/** calls listen(), may throw exception */
	void TCPS_listen();
	
	/** loop to client's connection requests, may throw exception */
	void TCPS_requests_loop();

	/** calls shutdown(), may throw exception */
	void TCPS_shutdown();
};