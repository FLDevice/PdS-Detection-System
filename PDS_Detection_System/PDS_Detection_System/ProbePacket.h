#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <cstdint>
#include <iostream>
#include <sstream> 
#include <iomanip>

#include <mysqlx/xdevapi.h>


class ProbePacket {

public:

	unsigned timestamp : 32;	//4  bytes +
	unsigned channel : 8;		//1  bytes +
	unsigned seq_ctl : 16;		//2  bytes +
	signed rssi : 8;			//1  bytes +
	uint8_t addr[6];			//6  bytes +
	uint8_t ssid_length;		//1  bytes +
	uint8_t ssid[32];			//32 bytes +
	uint8_t crc[4];				//4  bytes +
	unsigned long hash;			//4  bytes =
								//_________
								//55 bytes
	ProbePacket();

	void print();
	void print(long int last_update); // ADDED
	void storeInDB(mysqlx::Table packetTable, long int last_update, uint8_t espid); // ADDED
	bool checkSSID(const std::string& string);
};
