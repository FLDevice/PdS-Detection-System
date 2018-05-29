#include "stdafx.h"
#include "ProbePacket.h"


ProbePacket::ProbePacket(){
}


ProbePacket::~ProbePacket(){
}

void ProbePacket::print(){
	printf("%08d  PROBE  CHAN=%02d,  SEQ=%04x,  RSSI=%02d, "
		" ADDR=%02x:%02x:%02x:%02x:%02x:%02x,  ",
		timestamp,
		channel,
		seq_ctl/*[0], seq_ctl[1]*/,
		rssi,
		addr[0], addr[1], addr[2],
		addr[3], addr[4], addr[5]
	);
	printf("SSID=");
	for (int i = 0; i<ssid_length; i++)
		printf("%c", (char)ssid[i]);
	printf("  CRC=");
	for (int i = 0; i<4; i++)
		printf("%02x", crc[i]);
	printf("\n");
}
