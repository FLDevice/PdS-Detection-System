#include "stdafx.h"
#include "TCPServer.h"
/*
std::vector<int> x;
std::vector<int> y;
std::vector<double> d;
*/
TCPServer::TCPServer(long int espn, std::vector<long int> vec) {
	std::cout << " === TCPServer version 0.5 ===" << std::endl;
	try {
		TCPS_pipe_send("TCPServer");
	}
	catch (std::exception& e) {
		std::cout << e.what() << std::endl;
		throw;
	}

	setupDB();

	while (1) {

		try {
		
			esp_number = espn;
			if (esp_number > 0 && esp_number <= MAX_ESP32_NUM)
				break;
			else {
				std::string s = std::string("system allows from 1 to ") + std::to_string(MAX_ESP32_NUM)
					+ std::string(" ESP32.");
				throw std::exception(s.c_str());
			}
		}
		catch (std::exception& e) {
			std::cout << "Input number cannot be accepted for the following reason: " << e.what() << std::endl << std::endl;
		}

	}
	std::cout << std::endl;

	retry = MAX_RETRY_TIMES;
	threads_to_wait_for = esp_number;
	esp_to_wait = esp_number; 

	while (retry > 0) {
		try {
			if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) throw std::runtime_error("WSAStartup() failed with error ");

			//---------------//---------------//---------------
			TCPS_initialize();
			std::cout << "TCP Server is correctly initialized with " << esp_number << " ESP32." << std::endl;

			TCPS_socket();
			TCPS_bind();
			TCPS_listen();
			TCPS_ask_participation(vec);
			TCPS_close_listen_socket();

			TCPS_initialize();
			TCPS_socket();
			TCPS_bind();
			TCPS_listen();

			try {
				std::thread(&TCPServer::TCPS_process_packets, this).detach();
			}
			catch (std::exception& e) {
				std::cout << e.what() << std::endl;
				throw;
			}

			std::cout << "TCP Server is running" << std::endl;

			TCPS_requests_loop();
			//---------------//---------------//---------------
		}
		catch (TCPServer_exception& e) {
			std::cout << e.what() << WSAGetLastError() << std::endl;
			freeaddrinfo(aresult);
			closesocket(listen_socket);
			WSACleanup();
			retry--;
			if (!retry) throw;
		}
		catch (std::runtime_error& e) {
			std::cout << e.what() << std::endl;
			WSACleanup();
			retry--;
			if (!retry) throw;
		}
		catch (std::exception& e) {
			std::cout << e.what() << std::endl;
			retry--;
			if (!retry) throw;
		}
	}
}

void TCPServer::TCPS_pipe_send(const char* message) {

	namedPipe = CreateFile(PIPENAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (namedPipe == INVALID_HANDLE_VALUE) {
		throw std::exception("couldn't open pipe");
	}

	char mybuffer[10];
	DWORD bytesWritten;
	strcpy(mybuffer, message);
	BOOL r = WriteFile(namedPipe, mybuffer, strlen(mybuffer), &bytesWritten, NULL);
	if (bytesWritten != strlen(mybuffer)) {
		throw std::exception("failed writing on pipe");
	}
	CloseHandle(namedPipe);
}

void TCPServer::TCPS_initialize() {

	if (aresult) freeaddrinfo(aresult);
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	int result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &aresult);
	if (result != 0) {
		throw std::runtime_error("getaddrinfo() failed with error ");
	}
}

void TCPServer::TCPS_socket() {
	listen_socket = socket(aresult->ai_family, aresult->ai_socktype, aresult->ai_protocol);
	if (listen_socket == INVALID_SOCKET) {
		freeaddrinfo(aresult);
		throw std::runtime_error("socket() failed with error ");
	}
}

void TCPServer::TCPS_bind() {
	int result = bind(listen_socket, aresult->ai_addr, (int)aresult->ai_addrlen);
	if (result == SOCKET_ERROR) {
		throw TCPServer_exception("bind() failed");
	}
}

void TCPServer::TCPS_listen() {
	int result = listen(listen_socket, SOMAXCONN);
	if (result == SOCKET_ERROR) {
		throw TCPServer_exception("listen() failed with error ");
	}
}

void TCPServer::TCPS_ask_participation(std::vector<long int> vec) {

	std::cout << "Please unplug all the ESP32 from the energy source. You will plug them one at a time as requested." << std::endl << std::endl;

	/* create threads with no execution flow */
	std::thread threads[MAX_ESP32_NUM];

	// for each ESP32 ask for its position in the space and its INIT packet
	for (int i = 0; i < esp_number*2; i+=2) {
		int posx, posy;
		uint8_t mac[6];

		try {
			posx = vec[i], posy = vec[i+1];
			std::cout << "ESP32 number " << i/2 << std::endl;
			std::cout << "X coordinate: " << posx;

			std::cout << std::endl << "Y coordinate: " << posy;

			std::cout << std::endl;
		}
		catch (std::exception& e) {
			std::cout << "Input number cannot be accepted for the following reason: " << e.what() << std::endl << std::endl;
			i--;
			continue;
		}
		std::cout << "You can now plug this ESP32. Waiting for ESP32 detection..." << std::endl;

		// waiting for a INIT packet coming from the i-th ESP32. If something else is sent, just ignore it.
		while (1) {
			SOCKET client_socket = accept(listen_socket, NULL, NULL);
			if (client_socket == INVALID_SOCKET) {
				throw TCPServer_exception("accept() failed with error ");
			}

			std::cout << "Connected to the client" << std::endl;

			int result = 0;
			char* recvbuf = (char*)malloc(10);
			for (int i = 0; i < 10; i = i + result)
				result = recv(client_socket, recvbuf + i, 10 - i, 0);

			if (result > 0) {
				// The ESP32 sent an init message:
				if (memcmp(recvbuf, INIT_MSG_H, 4) == 0) {
					std::cout << "A new ESP32 has been detected. Sending confirm for its joining to the system." << std::endl;
					std::cout << "Its MAC address: ";

					memcpy(mac, recvbuf + 4, 6);
					printf("%02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

					char s[8];
					memcpy(s, INIT_MSG_H, 4);
					int port = FIRST_READY_PORT + i;
					std::string str = std::to_string(port);
					memcpy(s + 4, str.c_str(), 4);

					// sending joining confirm
					int send_result = send(client_socket, s, 8, 0);
					if (send_result == SOCKET_ERROR) {
						// connection reset by peer
						if (WSAGetLastError() == 10054) {
							continue;
						}
						else {
							free(recvbuf);
							throw TCPServer_exception("send() failed with error ");
						}
					}
					std::cout << "Confirmation sent." << std::endl;
					free(recvbuf);

					// arguments: id, mac address, x pos, y pos, ready port for socket creation
					ESP32 espdata(i/2, mac, posx, posy, port);
					espdata.store_esp();
					esp_list.push_back(espdata);

					// return to the outer for loop
					break;
				}
				// Incorrect formatting of the request; ignore it.
				else {
					free(recvbuf);
					continue;
				}
			}
			else if (result < 0) {
				free(recvbuf);
				throw TCPServer_exception("recv() failed with error ");
			}
		} // end while loop for INIT pckt

		std::cout << "Starting child thread for ready channel." << std::endl;

		// Assign to a thread the job to handle a new socket for ready packets.
		try {
			threads[i/2] = std::thread(&TCPServer::TCPS_ready_channel, this, i/2);
		}
		catch (std::exception& e) {
			std::cout << e.what() << std::endl;
			throw;
		}

	} // end loop for every esp32

	  /*
	  Before going to the next phase, the main thread will wait all the other threads
	  (that have an execution flow) to terminate their tasks on the ready socket.
	  */
	for (int j = 0; j < MAX_ESP32_NUM; j++) {
		if (threads[j].joinable())
			threads[j].join();
	}
}

void TCPServer::TCPS_ready_channel(int esp_id) {

	SOCKET sock;

	try {
		addrinfo h, *ainfo;
		ZeroMemory(&h, sizeof(h));
		h.ai_family = AF_INET;
		h.ai_socktype = SOCK_STREAM;
		h.ai_protocol = IPPROTO_TCP;
		h.ai_flags = AI_PASSIVE;
		// converting the port number in a format getaddrinfo understands
		std::string s = std::to_string(esp_list[esp_id].get_port());
		// socket operations
		if (((getaddrinfo(NULL, s.c_str(), &h, &ainfo)) != 0))
			throw std::runtime_error("CHILD THREAD [READY] - getaddrinfo() failed with error ");
		sock = socket(ainfo->ai_family, ainfo->ai_socktype, ainfo->ai_protocol);
		if (sock < 0) {
			freeaddrinfo(ainfo);
			throw std::runtime_error("CHILD THREAD [READY] - socket() failed with error ");
		}
		ready_sockets.push(sock);
		if (bind(sock, ainfo->ai_addr, (int)ainfo->ai_addrlen) == SOCKET_ERROR)
			throw TCPServer_exception("CHILD THREAD [READY] - bind() failed");
		if (listen(sock, 1) == SOCKET_ERROR)
			throw TCPServer_exception("CHILD THREAD [READY] - listen() failed with error ");
		while (1) {
			SOCKET c_sock = accept(sock, NULL, NULL);
			if (c_sock == INVALID_SOCKET)
				throw TCPServer_exception("CHILD THREAD [READY] - accept() failed with error ");

			//std::cout << "*** New request accepted from ESP with id " << esp_id << " on port " << s << ".\n";

			// waiting to receive a READY message
			char recvbuf[5];
			int res = 0;
			
			for (int i = 0; i < 5; i = i + res){
				res = recv(c_sock, recvbuf + i, 5 - i, 0);
			}

			if (res > 0) {
				// if it actually is a READY message
				if (memcmp(recvbuf, READY_MSG_H, 5) == 0) {

					/*
					Once the ready packet is arrived, notify all the threads waiting
					to answer READY to the clients.
					*/
					{
						std::unique_lock<std::mutex> ul(mtx);
						threads_to_wait_for--;
						cvar.notify_all();
					}

					/*
					Once every thread has received the ready packet, the threads can tell the
					esp32 to begin sniffing.
					*/
					std::unique_lock<std::mutex> ul(mtx);
					cvar.wait(ul, [this]() { return threads_to_wait_for == 0; });

					char sendbuf[5];
					strncpy_s(sendbuf, 6, READY_MSG_H, 5);

					int send_result = send(c_sock, sendbuf, 5, 0);
					if (send_result == SOCKET_ERROR) {
						// connection reset by peer
						if (WSAGetLastError() == 10054) {
							continue;
						}
						else {
							throw TCPServer_exception("CHILD THREAD [READY] - send() failed with error ");
						}
					}

					// Record the time when the ESP starts
					esp_list[esp_id].update_time();
					break;
				}
				// Incorrect formatting of the request; ignore it.
				else {
					continue;
				}
			}
			else if (res < 0) {
				throw TCPServer_exception("CHILD THREAD [READY] - recv() failed with error ");
			}
		}
	}
	catch (TCPServer_exception& e) {
		std::cout << e.what() << WSAGetLastError() << std::endl;
		freeaddrinfo(aresult);
		closesocket(sock);
	}
	catch (std::exception& e) {
		std::cout << e.what() << std::endl;
	}
}

void TCPServer::TCPS_requests_loop() {

	while (1) {
		/*--------------------------
		***	Accepting Client request
		---------------------------*/
		SOCKET client_socket = accept(listen_socket, NULL, NULL);
		if (client_socket == INVALID_SOCKET) {
			throw TCPServer_exception("accept() failed with error ");
		}

		std::cout << "--- New request accepted from client" << std::endl;

		/*--------------------------
		***	Receiving Packets from Client
		---------------------------*/
		try {
			std::thread(&TCPServer::TCPS_service, this, client_socket).detach();
		}
		catch (std::exception& e) {
			std::cout << e.what() << std::endl;
			throw;
		}

	}

}

void TCPServer::TCPS_service(SOCKET client_socket) {

	uint8_t retry_child = MAX_RETRY_TIMES;
	char c;
	uint8_t count;

	std::cout << "Running child thread" << std::endl;

	while (retry_child > 0) {
		try {
			int result = 0;

			// receiving mac address from client
			uint8_t mac[6];
			char rbuff[6];
			for (int i = 0; i < 6; i = i + result)
				result = recv(client_socket, rbuff + i, 6 - i, 0);

			if (result < 0) throw TCPServer_exception("mac_addr recv() failed with error ");

			memcpy(mac, rbuff, 6);
			printf("Receivin packets from ESP with MAC address %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

			// Get the instance of the device from the list created during setup with the mac received from the socket
			int esp_id = get_esp_instance(mac);
			// Update the time since last update
			esp_list[esp_id].update_time();

			// receiving packet counter from client
			recv(client_socket, &c, 1, 0);
			count = c - '0';
			printf("Receiving %u packets from ESP with MAC address %02x:%02x:%02x:%02x:%02x:%02x\n", count, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

			// create space for the incoming packets
			char* recvbuf = (char*)malloc(count*PACKET_SIZE);
			int recvbuflen = count * PACKET_SIZE;

			// receive the effective packets */
			for (int i = 0; i < recvbuflen; i = i + result)
				result = recv(client_socket, recvbuf + i, recvbuflen - i, 0);

			if (result > 0) {
				std::cout << "Bytes received: " << result << ", buffer contains:" << std::endl;
				std::cout << "Buffer size: " << recvbuflen << std::endl;
				std::cout << "Current Time: " << std::time(NULL) << ", passed since last update: " << esp_list[esp_id].get_update_interval() << std::endl; // ADDED

																																						   // store and print them
				storePackets(count, esp_id, recvbuf);


				// notify an esp has sent its data
				{
					std::unique_lock<std::mutex> ul(triang_mtx);
					esp_to_wait--;
					triang_cvar.notify_all();
				}


				// just send back a packet to the client as ack
				int send_result = send(client_socket, recvbuf, PACKET_SIZE, 0);
				if (send_result == SOCKET_ERROR) {
					// connection reset by peer
					if (WSAGetLastError() == 10054) {
						continue;
					}
					else {
						free(recvbuf);
						throw TCPServer_exception("send() failed with error ");
					}
				}
				std::cout << "Bytes sent back: " << send_result << std::endl;

			}
			else if (result == 0) {
				std::cout << "Connection closing" << std::endl;
			}
			else {
				// connection reset by peer
				if (WSAGetLastError() == 10054) {
					continue;
				}
				else {
					free(recvbuf);
					throw TCPServer_exception("send() failed with error ");
				}
			}

			free(recvbuf);
			break;
		}
		catch (std::exception& e) {
			std::cout << e.what() << std::endl;
			retry_child--;
			if (!retry_child) {
				std::cout << "AN EXCEPTION OCCURRED ON CHILD THREAD, TERMINATE APPLICATION" << std::endl;
				break;
			}
		}
	}

	std::cout << "Child thread correctly ended" << std::endl;
}

void TCPServer::TCPS_close_listen_socket() {
	int result = closesocket(listen_socket);
	if (result == SOCKET_ERROR) {
		throw TCPServer_exception("closesocket() failed with error ");
	}
}

void TCPServer::TCPS_shutdown(SOCKET client_socket) {
	int result = shutdown(client_socket, SD_SEND);
	if (result == SOCKET_ERROR) {
		throw TCPServer_exception("shutdown() failed with error ");
	}
	freeaddrinfo(aresult);
	closesocket(listen_socket);
	WSACleanup();
}

/*
* Create the table used to store the packets.
*
* !!! Currently everytime the program is run the database is reinitialized.
*/
void TCPServer::setupDB()
{
	mysqlx::Session session("localhost", 33060, "pds_user", "password");

	std::string quoted_name = std::string("`pds_db`.`Packet`");

	session.sql(std::string("DROP TABLE IF EXISTS") + quoted_name).execute();
	std::string create = "CREATE TABLE ";
	create += quoted_name;
	create += " (id INT NOT NULL PRIMARY KEY AUTO_INCREMENT, ";
	create += " esp_id INT UNSIGNED, ";
	create += " timestamp TIMESTAMP, ";
	create += " channel TINYINT UNSIGNED, ";
	create += " seq_ctl VARCHAR(4), ";
	create += " rssi TINYINT, ";
	create += " addr VARCHAR(32), ";
	create += " ssid VARCHAR(32), ";
	create += " crc VARCHAR(8), ";
	create += " hash INT UNSIGNED, ";
	create += " to_be_deleted INT )";							// ADDED

	session.sql(create).execute();

	quoted_name = std::string("`pds_db`.`ESP`");

	session.sql(std::string("DROP TABLE IF EXISTS") + quoted_name).execute();
	create = "CREATE TABLE ";
	create += quoted_name;
	create += " (mac VARCHAR(32) NOT NULL PRIMARY KEY,";
	create += " esp_id INT NOT NULL,";							// ADDED
	create += " x INT NOT NULL,";
	create += " y INT NOT NULL)";

	session.sql(create).execute();

	//														***DEVICES TABLE ADDED***
	quoted_name = std::string("`pds_db`.`Devices`");

	session.sql(std::string("DROP TABLE IF EXISTS") + quoted_name).execute();
	create = "CREATE TABLE ";
	create += quoted_name;
	create += " (dev_id INT NOT NULL PRIMARY KEY AUTO_INCREMENT, ";
	create += " mac VARCHAR(32) NOT NULL,";
	create += " x INT NOT NULL,";
	create += " y INT NOT NULL,";
	create += " timestamp TIMESTAMP)";

	session.sql(create).execute();

	std::cout << "Database correctly initialized." << std::endl;
}

void TCPServer::storePackets(int count, int esp_id, char* recvbuf) {
	try {
		long int time_since_last_update = esp_list[esp_id].get_previous_update_time();
		// Connect to server using a connection URL
		mysqlx::Session session("localhost", 33060, "pds_user", "password");

		try {
			mysqlx::Schema myDb = session.getSchema("pds_db");

			// Accessing the packet table
			mysqlx::Table packetTable = myDb.getTable("Packet");

			for (int i = 0; i < count; i++) {
				ProbePacket pp;
				memcpy(&pp, recvbuf + (i*PACKET_SIZE), PACKET_SIZE);
				pp_vector.push(pp);
				//printf("%d \t", i);
				//pp.print(); // MODIFIED
				pp.storeInDB(packetTable, time_since_last_update, esp_id);
			}
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

int TCPServer::get_esp_instance(uint8_t* mac) {
	for (int i = 0; i < esp_number; i++) {
		if (memcmp(mac, esp_list[i].get_mac_address_ptr(), 6) == 0) {
			//printf("ESP LIST:\n id=%i\t, mac=%02x:%02x:%02x:%02x:%02x:%02x\n", );
			return i;
		}
	}

	throw std::exception("No esp with such a MAC has been found.");
}

/***************************************   T   R   I   A   N   G   U   L   A   T   I   O   N   ***************************************/

//Method that calls the triangulation method when needed in a thread-safe modestd::cout << "    Coordinates of " << current_address << " : X=" << pos_x << ", Y=" << pos_y << std::endl << std::endl;
void TCPServer::TCPS_process_packets() {
	PacketProcessor pckt_rfn(esp_number);
	while (1) {
		{
			std::unique_lock<std::mutex> ul(triang_mtx);
			triang_cvar.wait(ul, [this]() { return esp_to_wait == 0; });

			esp_to_wait = esp_number;
		}

		pckt_rfn.process();
	}
}