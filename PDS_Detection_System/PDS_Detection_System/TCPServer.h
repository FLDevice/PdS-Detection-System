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
#include <thread>
#include <ctime> // ADDED
#include <mysqlx/xdevapi.h>
#include <string>
#include <list>
#include "Utilities.h"
#include "ProbePacket.h"
//#include "ProbePacket.cpp"
#include "BlockingQueue.h"
#include "BlockingQueue.cpp"

#pragma comment (lib, "Ws2_32.lib")

#define PACKET_SIZE 56
#define MAX_ESP32_NUM 8
#define MAX_RETRY_TIMES 3
#define DEFAULT_PORT "3010"

class TCPServer_exception : public virtual std::runtime_error {

public:
	TCPServer_exception(const char* s) : std::runtime_error(s) {}
};

/** represents a esp32 client as seen as the server */
struct ESP32 {
private:
	uint8_t id;
	uint8_t mac_address[6];
	int coordinate_x;
	int coordinate_y;
	/* port used to create the ready channel */
	int port;
	long int last_update, previous_update;

public:
	ESP32(int i, uint8_t* mac, int x, int y, int p) {
		id = i;
		for (int j = 0; j < 6; j++) {
			mac_address[j] = *(mac + j);
		}
		coordinate_x = x;
		coordinate_y = y;
		port = p;

		last_update = static_cast<long int> (time(NULL));
		update_time();
	}

	uint8_t *get_mac_address_ptr() {
		return mac_address;
	}

	std::string get_mac_address_string() {
		// Computing the address
		char buff[50];
		snprintf(buff, sizeof(buff), "%02x:%02x:%02x:%02x:%02x:%02x", mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
		std::string address = buff;

		return address;
	}

	uint8_t get_id() {
		return id;
	}

	int get_port() {
		return port;
	}

	/** Store an esp device in the database */
	void store_esp() {
		try {
			// Connect to server using a connection URL
			mysqlx::Session session("localhost", 33060, "pds_user", "password");

			try {
				mysqlx::Schema myDb = session.getSchema("pds_db");

				// Accessing the packet table
				mysqlx::Table espTable = myDb.getTable("ESP");

				// Insert SQL Table data
				espTable.insert("mac", "x", "y")
					.values(get_mac_address_string(), coordinate_x, coordinate_y).execute();
			}
			catch (std::exception &err) {
				std::cout << "The following error occurred: " << err.what() << std::endl;

				// Exit with error code
				exit(1);
			}
		}
		catch (std::exception &err) {
			std::cout << "The database session could not be opened: " << err.what() << std::endl;

			// Exit with error code
			exit(1);
		}
	}

	void update_time() {
		previous_update = last_update;
		last_update = static_cast<long int> (time(NULL));
	}

	long int get_previous_update_time() {
		return previous_update;
	}

	long int get_update_interval() {
		return last_update-previous_update;
	}
};

/** To use, just create a TCPServer object. */
class TCPServer {

protected:

	const int FIRST_READY_PORT = 3011;
	const char INIT_MSG_H[5] = "INIT";
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

	/**
	* Create the table used to store the packets.
	*
	* !!! Currently everytime the program is run the database is reinitialized. */
	void setupDB();

	void storePackets(int count, int esp_id, char* recvbuf);

	int get_esp_instance(uint8_t* mac);
};

