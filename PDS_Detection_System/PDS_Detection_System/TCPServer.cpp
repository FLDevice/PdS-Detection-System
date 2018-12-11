#include "stdafx.h"
#include "TCPServer.h"

std::vector<int> x;
std::vector<int> y;
std::vector<double> d;

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
			TCPS_ask_participation();
			TCPS_close_listen_socket();

			TCPS_initialize();
			TCPS_socket();
			TCPS_bind();
			TCPS_listen();

			try {
				std::thread(&TCPServer::TCPS_triangulate, this).detach();
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
			SOCKET client_socket = accept(listen_socket, NULL, NULL);
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
			threads[i] = std::thread(&TCPServer::TCPS_ready_channel, this, i);
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

					// Record the time when the ESP starts
					esp_list[esp_id].update_time();
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

void TCPServer::TCPS_shutdown(SOCKET client_socket) {
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
	create += " crc VARCHAR(8), ";
	create += " hash INT UNSIGNED, ";
	create += " triangulated INT )";							// ADDED

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
				printf("%d \t", i);
				pp.print(); // MODIFIED
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

//Method that estimates the distance (in meters) starting from the RSSI
double TCPServer::getDistanceFromRSSI(double rssi) {
	double rssiAtOneMeter = -59;
	double d = pow(10, (rssiAtOneMeter - rssi) / 20);
	return d;
}

//Method that calculates the distance among two points (x1,y1) , (x2,y2)
double dist(double x1, double y1, double x2, double y2) {
	return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

//Method that defines the Mean Square Error (MSE) function
double meanSquareError(const column_vector& m) {
	const double pos_x = m(0);
	const double pos_y = m(1);

	double mse = 0;
	int N = d.size();

	for (int i = 0; i < N; i++)
		mse = mse + pow(d[i] - dist(pos_x, pos_y, x[i], y[i]), 2);

	mse = mse / N;

	return mse;
}

//Method that finds the min (x,y) of the function meanSquareError ==> the (x,y) point will be the position of the device
void TCPServer::getCoordinates(int * pos_x, int * pos_y) {

	try {
		column_vector starting_point = { 0, 0 };

		dlib::find_min_using_approximate_derivatives(dlib::bfgs_search_strategy(),
			dlib::objective_delta_stop_strategy(1e-7),
			meanSquareError,
			starting_point, -1);

		*pos_x = starting_point(0);
		*pos_y = starting_point(1);

		std::cout << "   Coordinates: X=" << *pos_x << ", Y=" << *pos_y << std::endl << std::endl;
	}
	catch (std::exception& e) {
		std::cout << e.what() << std::endl;
	}
}

void TCPServer::triangulation(int first_id, int last_id) {

	int pos_x = -1;
	int pos_y = -1;

	try {
		mysqlx::Session session("localhost", 33060, "pds_user", "password");

		try {
			mysqlx::Schema myDb = session.getSchema("pds_db");

			mysqlx::Table packetTable = myDb.getTable("Packet");
			mysqlx::Table espTable = myDb.getTable("ESP");
			mysqlx::Table devicesTable = myDb.getTable("Devices");

			mysqlx::RowResult myResult;
			mysqlx::Row row;

			//Get the perimeter min(x), max(x), min(y), max(y)
			myResult = espTable.select("MIN(x)").execute();
			row = myResult.fetchOne();
			int min_x = row[0];

			myResult = espTable.select("MAX(x)").execute();
			row = myResult.fetchOne();
			int max_x = row[0];

			myResult = espTable.select("MIN(y)").execute();
			row = myResult.fetchOne();
			int min_y = row[0];

			myResult = espTable.select("MAX(y)").execute();
			row = myResult.fetchOne();
			int max_y = row[0];

			//Pass through all the packets
			for (int current_id = first_id; current_id <= last_id; current_id++) {

				x.clear();
				y.clear();
				d.clear();

				//Check if the current packet has already been triangulated ( 1 => YES  ;  0 ==> NO )
				myResult = packetTable.select("triangulated").where("id=:current_id").bind("current_id", current_id).execute();
				row = myResult.fetchOne();
				uint32_t t = (uint32_t)row[0];

				//std::cout << "Triangulated = " << t << std::endl;

				if (!t) { //the current packet has not been triangulated yet

				    //Get Hash and MAC address of the current packet
					myResult = packetTable.select("hash", "addr").where("id=:current_id").bind("current_id", current_id).execute();
					row = myResult.fetchOne();
					uint32_t current_hash = (uint32_t)row[0];
					std::string current_address = row[1];

					std::cout << " Hash " << current_hash << " with MAC " << current_address << " not checked yet";

					//Count how many ESPs have received this packet (this hash)
					myResult = packetTable.select("count(DISTINCT(esp_id))").where("hash=:current_hash").bind("current_hash", current_hash).execute();
					row = myResult.fetchOne();
					uint32_t counter = (uint32_t)row[0];

					std::cout << " (received by " << counter << " ESPs)" << std::endl;

					if (counter >= 3) { //the packet has been received by at least 3 ESPs (Note: change this value in debug/testing)

										//Get the ESP-ID and the RSSI from *ALL* the ESPs which have received the packet
										//N.B.: this query gives multiple rows --> one row for each ESP which has received the packet
						
						mysqlx::RowResult multipleQueryResult = packetTable.select("esp_id", "rssi").where("hash=:current_hash").bind("current_hash", current_hash).execute();

						//while (mysqlx::Row rows = myResult.fetchOne()) {
						for (mysqlx::Row rows : multipleQueryResult.fetchAll()) {
							uint32_t current_esp_id = (uint32_t)rows[0];
							int current_rssi = (int)rows[1];
							std::cout << "  Current ESP values: ESP-ID=" << current_esp_id << ", RSSI=" << current_rssi;

							//Get the coordinates of the ESP who has received the current packet
							myResult = espTable.select("x", "y").where("esp_id=:current_esp_id").bind("current_esp_id", current_esp_id).execute();
							row = myResult.fetchOne();
							int current_esp_x = (int)row[0];
							int current_esp_y = (int)row[1];

							//Estimate the distance from the RSSI
							double current_distance = getDistanceFromRSSI(current_rssi);

							std::cout << ", X=" << current_esp_x << ", Y=" << current_esp_y << ", Distance=" << current_distance << std::endl;;

							//Add the values in each vector
							x.push_back(current_esp_x);
							y.push_back(current_esp_y);
							d.push_back(current_distance);
						}
						//Triangulate the device with the current MAC address getting its coordinates pos_x and pos_y
						getCoordinates(&pos_x, &pos_y);

						//Check if the device triangulated is inside the perimeter, if so add it in the database
						if ((pos_x > min_x) && (pos_x < max_x) && (pos_y > min_y) && (pos_y < max_y)) 
							devicesTable.insert("mac", "x", "y").values(current_address, pos_x, pos_y).execute();
					}
					//Set as "already triangulated" (triangulated = 1) all the packets with the current hash
					packetTable.update().set("triangulated", 1).where("hash=:current_hash").bind("current_hash", current_hash).execute();
				}
			}
		}
		catch (std::exception &err) {
			std::cout << "The following error occurred: " << err.what() << std::endl;
			exit(1);
		}
	}
	catch (std::exception &err) {
		std::cout << "The database session could not be opened: " << err.what() << std::endl;
		exit(1);
	}
}

// CALLING   TRIANGULATION  METHOD
void TCPServer::TCPS_triangulate() {
	while (1) {
		{
			std::unique_lock<std::mutex> ul(triang_mtx);
			triang_cvar.wait(ul, [this]() { return esp_to_wait == 0; });

			esp_to_wait = esp_number;
		}

		try {
			mysqlx::Session session("localhost", 33060, "pds_user", "password");
			try {
				mysqlx::Schema myDb = session.getSchema("pds_db");

				mysqlx::Table packetTable = myDb.getTable("Packet");

				mysqlx::RowResult myResult;
				mysqlx::Row row;

				//The first ID on which we start the triangulation will be the first of the new capture (last + 1)
				first_id = last_id + 1;

				//Get the last(the maximum) packet-ID of the sequence
				myResult = packetTable.select("MAX(id)").execute();
				row = myResult.fetchOne();
				last_id = (int)row[0];

				std::cout << std::endl << "Current capture in database: First ID = " << first_id << ", Last ID = " << last_id << std::endl << std::endl;
			}
			catch (std::exception &err) {
				std::cout << "The following error occurred: " << err.what() << std::endl;
				exit(1);
			}
		}
		catch (std::exception &err) {
			std::cout << "The database session could not be opened: " << err.what() << std::endl;
			exit(1);
		}

		//call the triangolation method on the packets just stored (new packets)
		triangulation(first_id, last_id);
		//triangulation(1, 5); //per triangolare solo i pacchetti con il primo hash ricevuto 
	}	
}
