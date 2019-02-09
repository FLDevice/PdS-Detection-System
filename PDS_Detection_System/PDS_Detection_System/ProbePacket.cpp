#include "stdafx.h"
#include "ProbePacket.h"

ProbePacket::ProbePacket() {
}

void ProbePacket::print() {
	printf("%08d  PROBE  CHAN=%02d,  SEQ=%04x,  RSSI=%02d, "
		" ADDR=%02x:%02x:%02x:%02x:%02x:%02x, HASH:%lu ",
		timestamp,
		channel,
		seq_ctl/*[0], seq_ctl[1]*/,
		rssi,
		addr[0], addr[1], addr[2],
		addr[3], addr[4], addr[5],
		hash
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
		" ADDR=%02x:%02x:%02x:%02x:%02x:%02x, timestamp=%ld, HASH:%lu ",
		timestamp,
		channel,
		seq_ctl/*[0], seq_ctl[1]*/,
		rssi,
		addr[0], addr[1], addr[2],
		addr[3], addr[4], addr[5],		
		last_update,
		hash
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
void ProbePacket::storeInDB(mysqlx::Table packetTable, mysqlx::Table localPacketsTable, long int last_update, uint8_t espid) {
	
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

	if (!checkSSID(ssid_to_store)) {
		printf("\n\n\n\n\nHAHA! I've finally defeated you, bastard!\n\n\n\n\n\n\n");
		return;
	}

	// Computing the CRC
	snprintf(buff, sizeof(buff), "%02x%02x%02x%02x", crc[0], crc[1], crc[2], crc[3]);
	std::string crc_to_store = buff;

	//Check if (source) MAC address is local and unicast
	int local = 0;
	int firstByteMAC = std::stol(address.substr(0, 2), nullptr, 16);
	int mask1 = 0b00000010; // global/local bit
	int mask2 = 0b00000001; // unicast/multicast bit 
	if ((firstByteMAC & mask1) && !(firstByteMAC & mask2))
		local = 1; //local MAC

	try {
		// Insert SQL Table data
		packetTable.insert("esp_id", "timestamp", "channel", "seq_ctl", "rssi", "addr", "ssid", "crc", "hash", "to_be_deleted") //ADDED "triangulated" = 0
			.values(espid, receive_time, channel, ctl_to_store, rssi, address, ssid_to_store, crc_to_store, hash, 0).execute();

		if (local)
			localPacketsTable.insert("addr", "timestamp", "seq_ctl", "to_be_deleted") //ADDED "triangulated" = 0
			.values(address, receive_time, ctl_to_store, 0).execute();

	}
	catch(...) {
		printf("\n\n\n\n\n\nThis dumbass is still fucking around...\n\n\n\n\n\n\n");
	}
}

/* 
 * Check if the SSID is a valid utf8 string
 */
bool ProbePacket::checkSSID(const std::string& string) {
	int c, i, ix, n, j;
	for (i = 0, ix = string.length(); i < ix; i++)
	{
		c = (unsigned char)string[i];
		//if (c==0x09 || c==0x0a || c==0x0d || (0x20 <= c && c <= 0x7e) ) n = 0; // is_printable_ascii
		if (0x00 <= c && c <= 0x7f) n = 0; // 0bbbbbbb
		else if ((c & 0xE0) == 0xC0) n = 1; // 110bbbbb
		else if (c == 0xed && i<(ix - 1) && ((unsigned char)string[i + 1] & 0xa0) == 0xa0) return false; //U+d800 to U+dfff
		else if ((c & 0xF0) == 0xE0) n = 2; // 1110bbbb
		else if ((c & 0xF8) == 0xF0) n = 3; // 11110bbb
											//else if (($c & 0xFC) == 0xF8) n=4; // 111110bb //byte 5, unnecessary in 4 byte UTF-8
											//else if (($c & 0xFE) == 0xFC) n=5; // 1111110b //byte 6, unnecessary in 4 byte UTF-8
		else return false;
		for (j = 0; j<n && i<ix; j++) { // n bytes matching 10bbbbbb follow ?
			if ((++i == ix) || (((unsigned char)string[i] & 0xC0) != 0x80))
				return false;
		}
	}
	return true;
}