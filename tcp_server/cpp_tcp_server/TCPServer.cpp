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
	
	retry = MAX_RETRY_TIMES;
	
	while(retry > 0){
		try{
			//---------------//---------------//---------------
			TCPS_initialize();
			std::cout << "TCP Server is correctly initialized" << std::endl;
			
			TCPS_socket();
			TCPS_bind();
			TCPS_listen();
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
		
		// receiving packet counter from client
		char c;
		uint8_t count;
		
		recv(client_socket, &c, 1, 0);
		count = c - '0';
		std::cout << "Receiving " << count << " packets" << std::endl;

		// create space for the incoming packets 
		recvbuf = (char*)malloc(count*PACKET_SIZE);
		int recvbuflen = count * PACKET_SIZE;
		
		// receive the effective packets */
		for (int i = 0; i < recvbuflen ; i = i + result)
			result = recv(client_socket, recvbuf+i, recvbuflen-i, 0);
		
		if(result > 0) {
			
			std::cout << "Bytes received: " << result << ", buffer contains:" << std::endl;
			std::cout << "Buffer size: " << recvbuflen << std::endl;
			
			// store and print them
			for (int i = 0; i < count; i++) {
				ProbePacket pp;
				memcpy(&pp, recvbuf+(i*PACKET_SIZE), PACKET_SIZE);
				pp_vector.push_back(pp);
				printf("%d \t", i);
				pp.print();
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
		
		free(recvbuf);
		
	}
	
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