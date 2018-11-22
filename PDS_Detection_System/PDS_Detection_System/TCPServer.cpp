#include "stdafx.h"
#include "TCPServer.h"


TCPServer::TCPServer() {
	std::cout << " === TCPServer version 0.1 ===" << std::endl;

	setupDB();

	//User inputs ESP32 amount
	while (1) {
		std::cout << "Insert how many ESP32 will join the system: ";

		try {
			esp_number = Utilities::getIntFromInput();
			std::cout << std::endl;

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

	time_since_last_update = static_cast<long int> (time(NULL));; // ADDED
	retry = MAX_RETRY_TIMES;
	threads_to_wait_for = esp_number;

	while (retry > 0) {
		try {
			if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) throw std::runtime_error("WSAStartup() failed with error ");

			//---------------//---------------//---------------
			TCPS_initialize();
			std::cout << "TCP Server is correctly initialized with " << esp_number << " ESP32." << std::endl;

			TCPS_socket();
			TCPS_bind();
			TCPS_listen();
			TCPS_ask_participation();
			TCPS_close_listen_socket();

			TCPS_initialize();
			TCPS_socket();
			TCPS_bind();
			TCPS_listen();

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
			std::cout << e.what() << result << std::endl;
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

void TCPServer::TCPS_initialize() {

	if (aresult) freeaddrinfo(aresult);
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &aresult);
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
	result = bind(listen_socket, aresult->ai_addr, (int)aresult->ai_addrlen);
	if (result == SOCKET_ERROR) {
		throw TCPServer_exception("bind() failed");
	}
}

void TCPServer::TCPS_listen() {
	result = listen(listen_socket, SOMAXCONN);
	if (result == SOCKET_ERROR) {
		throw TCPServer_exception("listen() failed with error ");
	}
}

void TCPServer::TCPS_ask_participation() {

	std::cout << "Please unplug all the ESP32 from the energy source. You will plug them one at a time as requested." << std::endl << std::endl;

	/* create threads with no execution flow */
	std::thread threads[MAX_ESP32_NUM];

	// for each ESP32 ask for its position in the space and its INIT packet
	for (int i = 0; i < esp_number; i++) {
		int posx, posy;
		uint8_t mac[6];

		try {
			std::cout << "ESP32 number " << i << ": insert its spacial position." << std::endl;
			std::cout << "X coordinate: ";
			posx = Utilities::getIntFromInput();

			std::cout << std::endl << "Y coordinate: ";
			posy = Utilities::getIntFromInput();

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
			client_socket = accept(listen_socket, NULL, NULL);
			if (client_socket == INVALID_SOCKET) {
				throw TCPServer_exception("accept() failed with error ");
			}

			std::cout << "Connected to the client" << std::endl;

			recvbuf = (char*)malloc(10);
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
					send_result = send(client_socket, s, 8, 0);
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
					ESP32 espdata(i, mac, posx, posy, port);
					esp_list.push_back(espdata);

					// return to the outer for loop
					break;
				}
				// The ESP32 is asking if the server is ready:
				else if (memcmp(recvbuf, READY_MSG_H, 5) == 0) {
					std::cout << "Ready request arrived." << std::endl;

					// Reply 'NO' to the ESP32
					free(recvbuf);
					char sendbuf[6], no = 0;
					strncpy_s(sendbuf, 6, READY_MSG_H, 5);
					sendbuf[5] = no;

					printf("--- sending %s\n", sendbuf);
					send_result = send(client_socket, sendbuf, 6, 0);
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
					free(recvbuf);
					continue;
				}
				// Incorrect formatting of the request; ignore it.
				else {
					free(recvbuf);
					continue;
				}
			}
			else if (result == 0)
				std::cout << "AAAAAAAAA";
			else if (result < 0) {
				free(recvbuf);
				throw TCPServer_exception("recv() failed with error ");
			}
		} // end while loop for INIT pckt

		std::cout << "Starting child thread for ready channel." << std::endl;

		// Assign to a thread the job to handle a new socket for ready packets.
		try {
			threads[i] = std::thread(&TCPServer::TCPS_ready_channel, this, i);
		}
		catch (std::exception& e) {
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
		std::string s = std::to_string(esp_list[esp_id].port);
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

			std::cout << "*** New request accepted from ESP with id " << esp_id << " on port " << s << ".\n";

			// waitin to receive a READY message
			char recvbuf[5];
			int res;
			for (int i = 0; i < 5; i = i + result)
				res = recv(c_sock, recvbuf + i, 5 - i, 0);


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

					send_result = send(c_sock, sendbuf, 5, 0);
					if (send_result == SOCKET_ERROR) {
						// connection reset by peer
						if (WSAGetLastError() == 10054) {
							continue;
						}
						else {
							throw TCPServer_exception("CHILD THREAD [READY] - send() failed with error ");
						}
					}
					break;
				}
				// Incorrect formatting of the request; ignore it.
				else {
					continue;
				}
			}
			else if (result < 0) {
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
		client_socket = accept(listen_socket, NULL, NULL);
		if (client_socket == INVALID_SOCKET) {
			throw TCPServer_exception("accept() failed with error ");
		}

		std::cout << "--- New request accepted from client" << std::endl;

		/*--------------------------
		***	Receiving Packets from Client
		---------------------------*/
		try {
			std::thread(&TCPServer::TCPS_service, this).detach();
		}
		catch (std::exception& e) {
			throw;
		}

	}

}

void TCPServer::TCPS_service() {

	uint8_t retry_child = MAX_RETRY_TIMES;
	char c;
	uint8_t count;

	std::cout << "Running child thread" << std::endl;

	while (retry_child > 0) {
		try {
			// receiving packet counter from client
			recv(client_socket, &c, 1, 0);
			count = c - '0';
			printf("Receiving %u packets\n", count);

			// create space for the incoming packets
			recvbuf = (char*)malloc(count*PACKET_SIZE);
			int recvbuflen = count * PACKET_SIZE;

			// receive the effective packets */
			for (int i = 0; i < recvbuflen; i = i + result)
				result = recv(client_socket, recvbuf + i, recvbuflen - i, 0);

			if (result > 0) {
				long int curTime = static_cast<long int> (std::time(NULL)); // ADDED

				std::cout << "Bytes received: " << result << ", buffer contains:" << std::endl;
				std::cout << "Buffer size: " << recvbuflen << std::endl;
				std::cout << "Current Time: " << std::time(NULL) << ", passed since last update: " << curTime - time_since_last_update << std::endl; // ADDED

																																					 // store and print them
				storePackets(count);

				/* Old version

				for (int i = 0; i < count; i++) {
				ProbePacket pp;
				memcpy(&pp, recvbuf + (i*PACKET_SIZE), PACKET_SIZE);
				pp_vector.push(pp);
				printf("%d \t", i);
				pp.print(time_since_last_update); // MODIFIED
				pp.storeInDB(time_since_last_update);
				}*/

				// just send back a packet to the client as ack
				send_result = send(client_socket, recvbuf, PACKET_SIZE, 0);
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

			time_since_last_update = static_cast<long int> (time(NULL)); // ADDED

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
	result = closesocket(listen_socket);
	if (result == SOCKET_ERROR) {
		throw TCPServer_exception("closesocket() failed with error ");
	}
}

void TCPServer::TCPS_shutdown() {
	result = shutdown(client_socket, SD_SEND);
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
	create += " crc VARCHAR(8)) ";

	session.sql(create).execute();

	std::cout << "Database correctly initialized." << std::endl;
}

void TCPServer::storePackets(int count) {
	try {
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
				printf("%d \t", i);
				pp.print(time_since_last_update); // MODIFIED
				pp.storeInDB(packetTable, time_since_last_update);
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