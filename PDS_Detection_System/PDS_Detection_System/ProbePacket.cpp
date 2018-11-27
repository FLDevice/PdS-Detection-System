#include "stdafx.h"
#include "ProbePacket.h"

ProbePacket::ProbePacket() {
}

void ProbePacket::print() {
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

void ProbePacket::print(long int last_update) {
	last_update += timestamp / 1000000;

	printf("%08d  PROBE  CHAN=%02d,  SEQ=%04x,  RSSI=%02d, "
		" ADDR=%02x:%02x:%02x:%02x:%02x:%02x, timestamp=%ld ",
		timestamp,
		channel,
		seq_ctl/*[0], seq_ctl[1]*/,
		rssi,
		addr[0], addr[1], addr[2],
		addr[3], addr[4], addr[5],
		last_update
	);
	printf("SSID=");
	for (int i = 0; i<ssid_length; i++)
		printf("%c", (char)ssid[i]);
	printf("  CRC=");
	for (int i = 0; i<4; i++)
		printf("%02x", crc[i]);
	printf("\n");
}

/*
*	Connect to the database and store the content of this instance of a packet in it.
*
*	long int last_update: The last time when the local computer timestamp has been properly recorded.
*/
void ProbePacket::storeInDB(mysqlx::Table packetTable, long int last_update) {
	
	// Computing the timestamp
	time_t rawtime = last_update + timestamp / 1000000;
	struct tm timeinfo;
	char receive_time[20];
	localtime_s(&timeinfo, &rawtime);
	strftime(receive_time, 20, "%F %T", &timeinfo);

	// Computing the control sequence
	std::ostringstream ctlStream;
	ctlStream << std::setfill('0') << std::setw(4) << std::hex << seq_ctl << std::endl;
	std::string ctl_to_store = ctlStream.str();

	// Computing the address
	char buff[50];
	snprintf(buff, sizeof(buff), "%02x:%02x:%02x:%02x:%02x:%02x", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	std::string address = buff;

	// Computing the SSID
	std::ostringstream ssidStream;
	for (int i = 0; i < ssid_length; i++) {
		ssidStream << (char)ssid[i];
	}
	std::string ssid_to_store = ssidStream.str();

	// Computing the CRC
	snprintf(buff, sizeof(buff), "%02x%02x%02x%02x", crc[0], crc[1], crc[2], crc[3]);
	std::string crc_to_store = buff;

	// Insert SQL Table data
	packetTable.insert("esp_id", "timestamp", "channel", "seq_ctl", "rssi", "addr", "ssid", "crc")
		.values(1, receive_time, channel, ctl_to_store, rssi, address, ssid_to_store, crc_to_store).execute();
}