#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <cstdint>

// ADDED
/*
  Include directly the different
  headers from cppconn/ and mysql_driver.h + mysql_util.h
  (and mysql_connection.h). This will reduce your build time!
*/
/*#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>*/


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

	void print();
  void print(long int last_update); // ADDED
	//void storeInDB(long int last_update); // ADDED
};
