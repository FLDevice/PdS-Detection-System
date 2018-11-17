#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <cstring>
#include <string>
#include <cstdint>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <vector>
#include <exception>
#include <thread>
#include <mutex>
#include <condition_variable>
	#include <ctime> // ADDED
#include "utilities.cpp"
#include "probepacket.h"
#include "probepacket.cpp"
#include "BlockingQueue.h"
#include "BlockingQueue.cpp"

#pragma comment (lib, "Ws2_32.lib")

#define PACKET_SIZE 52
#define MAX_ESP32_NUM 8
#define MAX_RETRY_TIMES 3
#define DEFAULT_PORT "3010"

class TCPServer_exception : public virtual std::runtime_error {

public:
	TCPServer_exception(const char* s) : std::runtime_error(s) {}
};

/** represents a esp32 client as seen as the server */
struct ESP32{
public:

	uint8_t id;
	uint8_t mac_address[6];
	int coordinate_x;
	int coordinate_y;
	/* port used to create the ready channel */
	int port;
	
	ESP32(int i, uint8_t* mac, int x, int y, int p){
		id = i;
		for(int j = 0; j < 6; j++){
			mac_address[j] = *(mac+j);
		}
		coordinate_x = x;
		coordinate_y = y;
		port = p;
	}
};

/** To use, just create a TCPServer object. */
class TCPServer{

protected:
	
	const int FIRST_READY_PORT = 3011;
	const char INIT_MSG_H[5]= "INIT";
	const char READY_MSG_H[6] = "READY";
	
	int esp_number;
	std::vector<ESP32> esp_list;
	
	int threads_to_wait_for;
	std::mutex mtx;
	std::condition_variable cvar;
	
	// thread-safe queue to store esp32 sniffed packets
	BlockingQueue<ProbePacket> pp_vector;

	WSADATA wsadata;

	// sockets
	SOCKET listen_socket;
	SOCKET client_socket;
	BlockingQueue<SOCKET> ready_sockets;

	addrinfo *aresult = NULL;
	addrinfo *aresult1 = NULL;
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

	long int time_since_last_update;


public:

	TCPServer();

private:
	/** initialize winsock, may throw exception */
	void TCPS_initialize(addrinfo* a);

	/** calls socket(), may throw exception */
	void TCPS_socket(addrinfo* a);

	/** calls bind(), may throw exception */
	void TCPS_bind(addrinfo* a);

	/** calls listen(), may throw exception */
	void TCPS_listen();
	
	/** listen for ESP32 joining requests */
	void TCPS_ask_participation();
	
	/** create a connection with the ESP32 to notify when the server is ready */
	void TCPS_ready_channel(int esp_id);

	/** loops to client's connection requests, may throw exception */
	void TCPS_requests_loop();

	/** calls shutdown(), may throw exception */
	void TCPS_shutdown();
	
	/** calls closesocket() on the listen socket, may throw exception */
	void TCPS_close_listen_socket();

	/** stores in the blocking queue client's sniffed packets,
		may throw exceptions */
	void TCPS_service();
};
