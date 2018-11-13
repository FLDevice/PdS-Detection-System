#include "TCPServer.h"

int main(){
	
	try{
		TCPServer tcp_server;
	}
	catch(std::exception& e){
		std::cout << "AN EXCEPTION OCCURRED, TERMINATE APPLICATION" << std::endl;
		exit(-1);
	}
}



TCPServer::TCPServer(){
	
	std::cout << " === TCPServer version 0.1 ===" << std::endl;
	
	//User inputs ESP32 amount
	while(1){
		std::cout << "Insert how many ESP32 will join the system: ";
		
		try{
			esp_number = getIntFromInput();
			std::cout << std::endl;
			
			if (esp_number > 0 && esp_number <= MAX_ESP32_NUM)
				break;
			else{
				std::string s = std::string("system allows from 1 to ") + std::to_string(MAX_ESP32_NUM )
								+ std::string(" ESP32.");
				throw std::exception(s.c_str());
			}
		}
		catch(std::exception& e){
			std::cout << "Input number cannot be accepted for the following reason: " << e.what() << std::endl << std::endl;
		}

	}
	std::cout << std::endl;

	time_since_last_update = 0; // ADDED
	retry = MAX_RETRY_TIMES;

	// Initializing Server Functionality	
	while(retry > 0){
		try{
			//---------------//---------------//---------------
			TCPS_initialize();
			std::cout << "TCP Server is correctly initialized with " << esp_number << " ESP32." << std::endl;

			TCPS_socket();
			TCPS_bind();
			TCPS_listen();
			TCPS_ask_participation();
			
			std::cout << "TCP Server is running" << std::endl;

			TCPS_requests_loop();
			//---------------//---------------//---------------
		}
		catch(TCPServer_exception& e){
			std::cout << e.what() << WSAGetLastError() << std::endl;
			freeaddrinfo(aresult);
			closesocket(listen_socket);
			WSACleanup();
			retry--;
			if(!retry) throw;
		}
		catch(std::runtime_error& e){
			std::cout << e.what() << result << std::endl;
			WSACleanup();
			retry--;
			if(!retry) throw;
		}
		catch(std::exception& e){
			std::cout << e.what() << std::endl;
			retry--;
			if(!retry) throw;
		}
	}
}

void TCPServer::TCPS_initialize(){

	result = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if(result != 0){
		throw std::runtime_error("WSAStartup() failed with error ");
	}

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	result = getaddrinfo(NULL, DEFAULT_PORT, &hints, &aresult);
	if(result != 0){
		throw std::runtime_error("getaddrinfo() failed with error ");
	}
}

void TCPServer::TCPS_socket(){
	listen_socket = socket(aresult->ai_family, aresult->ai_socktype, aresult->ai_protocol);
	if (listen_socket == INVALID_SOCKET){
		freeaddrinfo(aresult);
		throw std::runtime_error("socket() failed with error ");
	}
}

void TCPServer::TCPS_bind(){
	result = bind(listen_socket, aresult->ai_addr, (int)aresult->ai_addrlen);
	if (result == SOCKET_ERROR){
		throw TCPServer_exception("bind() failed");
	}
}

void TCPServer::TCPS_listen(){
	result = listen(listen_socket, SOMAXCONN);
	if(result == SOCKET_ERROR){
		throw TCPServer_exception("listen() failed with error ");
	}
}

void TCPServer::TCPS_ask_participation(){
	
		std::cout << "Please unplug all the ESP32 from the energy source. You will plug them one at a time as requested." << std::endl << std::endl;
		
		// for each ESP32 ask for its position in the space and its INIT packet
		for(int i = 0; i < esp_number; i++){
			int posx, posy;
			
			try{
				std::cout << "ESP32 number " << i << ": insert its spacial position." << std::endl;
				std::cout << "X coordinate: ";
				posx = getIntFromInput();
				
				std::cout << std::endl << "Y coordinate: ";
				posy = getIntFromInput();
				
				std::cout << std::endl;
			}
			catch(std::exception& e){
				std::cout << "Input number cannot be accepted for the following reason: " << e.what() << std::endl << std::endl;
				i--;
				continue;
			}
			std::cout << "You can now plug this ESP32. Waiting for ESP32 detection..." << std::endl;
			
			// waiting for a INIT packet coming from the i-th ESP32. If something else is sent, just ignore it.
			while(1){
				client_socket = accept(listen_socket,NULL, NULL);
				if(client_socket == INVALID_SOCKET){
					throw TCPServer_exception("accept() failed with error ");
				}
				
				std::cout << "Connected to the client" << std::endl;
				
				recvbuf = (char*)malloc(10);
				for (int i = 0; i < 10 ; i = i + result)
					result = recv(client_socket, recvbuf+i, 10-i, 0);
				
				printf("--- recvbuf: %s\n", recvbuf);
				
				if(result > 0){
					// The ESP32 sent an init message:
					if(memcmp(recvbuf, INIT_MSG_H, 4) == 0){
						std::cout << "A new ESP32 has been detected. Sending confirm for its joining to the system." << std::endl;
						
						/*
						MISSING PART: Test to the following 6 bytes of recvbuf and if success save
						this ESP32 MAC address
						*/
						
						// sending joining confirm
						send_result = send(client_socket, INIT_MSG_H, 4, 0);
						if(send_result == SOCKET_ERROR){
							// connection reset by peer
							if(WSAGetLastError() == 10054){
								continue;
							}
							else{
								free(recvbuf);
								throw TCPServer_exception("send() failed with error ");
							}
						}
						std::cout << "Confirmation sent." << std::endl;	
						free(recvbuf);
						// return to the outer for loop
						break;
					}
					// The ESP32 is asking if the server is ready:
					else if(memcmp(recvbuf, READY_MSG_H, 5) == 0){
						std::cout << "Ready request arrived." << std::endl;	

						// Reply 'NO' to the ESP32
						free(recvbuf);
						char sendbuf[6], no = 0;
						strncpy(sendbuf, READY_MSG_H, 5);
						sendbuf[5] = no;
						
						printf("--- sending %s\n", sendbuf);
						send_result = send(client_socket, sendbuf, 6, 0);
						if(send_result == SOCKET_ERROR){
							// connection reset by peer
							if(WSAGetLastError() == 10054){
								continue;
							}
							else{
								free(recvbuf);
								throw TCPServer_exception("send() failed with error ");
							}
						}
						free(recvbuf);
						continue;
					}
					// Incorrect formatting of the request; ignore it.
					else{
						free(recvbuf);
						continue;
					}
				}
				else if (result == 0)
					std::cout << "AAAAAAAAA";
				else if (result < 0){
					free(recvbuf);
					throw TCPServer_exception("recv() failed with error ");
				}
			}
		}
	
}

void TCPServer::TCPS_requests_loop(){

	while(1){
		/*--------------------------
		***	Accepting Client request
		---------------------------*/
		client_socket = accept(listen_socket, NULL, NULL);
		if(client_socket == INVALID_SOCKET){
			throw TCPServer_exception("accept() failed with error ");
		}

		std::cout << "--- New request accepted from client" << std::endl;

		/*--------------------------
		***	Receiving Packets from Client
		---------------------------*/
		try{
			std::thread(&TCPServer::TCPS_service, this).detach();
		}catch(std::exception& e){
			throw;
		}

	}

}

void TCPServer::TCPS_service(){

	uint8_t retry_child = MAX_RETRY_TIMES;
	char c;
	uint8_t count;

	std::cout << "Running child thread" << std::endl;

	while(retry_child > 0){
		try{
			// receiving packet counter from client
			recv(client_socket, &c, 1, 0);
			count = c - '0';
			printf("Receiving %u packets\n", count);

			// create space for the incoming packets
			recvbuf = (char*)malloc(count*PACKET_SIZE);
			int recvbuflen = count * PACKET_SIZE;

			// receive the effective packets */
			for (int i = 0; i < recvbuflen ; i = i + result)
				result = recv(client_socket, recvbuf+i, recvbuflen-i, 0);

			if(result > 0) {
				long int curTime = static_cast<long int> (std::time(NULL)); // ADDED

				std::cout << "Bytes received: " << result << ", buffer contains:" << std::endl;
				std::cout << "Buffer size: " << recvbuflen << std::endl;
				std::cout << "Current Time: " << std::time(NULL) << ", passed since last update: " << curTime - time_since_last_update << std::endl; // ADDED

				// store and print them
				for (int i = 0; i < count; i++) {
					ProbePacket pp;
					memcpy(&pp, recvbuf+(i*PACKET_SIZE), PACKET_SIZE);
					pp_vector.push(pp);
					printf("%d \t", i);
					pp.print(time_since_last_update); // MODIFIED
				}

				// just send back a packet to the client as ack
				send_result = send(client_socket, recvbuf, PACKET_SIZE, 0);
				if(send_result == SOCKET_ERROR){
					// connection reset by peer
					if(WSAGetLastError() == 10054){
						continue;
					}
					else{
						free(recvbuf);
						throw TCPServer_exception("send() failed with error ");
					}
				}
				std::cout << "Bytes sent back: " << send_result << std::endl;

			}
			else if (result == 0){
				std::cout << "Connection closing" << std::endl;
			}
			else{
				// connection reset by peer
				if(WSAGetLastError() == 10054){
					continue;
				}
				else{
					free(recvbuf);
					throw TCPServer_exception("send() failed with error ");
				}
			}

			time_since_last_update = static_cast<long int> (time(NULL)); // ADDED

			free(recvbuf);
			break;
		}
		catch(std::exception& e){
			std::cout << e.what() << std::endl;
			retry_child--;
			if(!retry_child){
				std::cout << "AN EXCEPTION OCCURRED ON CHILD THREAD, TERMINATE THREAD" << std::endl;
				break;
			}
		}
	}

	std::cout << "Child thread correctly ended" << std::endl;
}

void TCPServer::TCPS_shutdown(){
	result = shutdown(client_socket, SD_SEND);
	if(result == SOCKET_ERROR){
		throw TCPServer_exception("shutdown() failed with error ");
	}
	freeaddrinfo(aresult);
	closesocket(listen_socket);
	WSACleanup();
}
