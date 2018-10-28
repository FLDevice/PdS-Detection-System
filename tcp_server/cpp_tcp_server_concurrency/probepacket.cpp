#include "probepacket.h"

ProbePacket::ProbePacket(){
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

// ADDED
void ProbePacket::print(long int last_update){
	last_update += timestamp/1000000;

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

// ADDED
/*void storeInDB(long int last_update) {
	long int receive_time = last_update + timestamp/1000000;

	try {
	  sql::Driver *driver;
	  sql::Connection *con;
	  sql::Statement *stmt;
	  sql::ResultSet *res;

	  /* Create a connection
	  driver = get_driver_instance();
	  con = driver->connect("tcp://127.0.0.1:3306", "root", "root");
	  /* Connect to the MySQL test database
	  con->setSchema("detection_system");

	  stmt = con->createStatement();

		prep_stmt = con->prepareStatement("INSERT INTO detection_system(esp_id, timestamp, channel, seq_ctl, rssi, addr, ssid, crc) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");

		prep_stmt->setInt(1, 1); // Set esp_id
		prep_stmt->setLong(2, receive_time); // Set timestamp
		prep_stmt->setInt(3, channel); // Set channel
		prep_stmt->setInt(4, seq_ctl); // Set seq_ctl
		prep_stmt->setInt(5, rssi); // Set rssi

		// Computing the Address
		std::ostringstream addressStream;
		addressStream << addr[0] << ":" << addr[1] << ":" << addr[2] << ":" << addr[3] << ":" << addr[4] << ":" << addr[5] ;
		std::string address = addressStream.str();
		prep_stmt->setString(6, address); // Set address

		// Computing the ssid
		std::ostringstream ssidStream;
		for (int i = 0; i<ssid_length; i++)
			ssidStream << (char)ssid[i];
		std::string ssid_to_store = ssidStream.str();
		prep_stmt->setString(7, ssid_to_store); // Set ssid

		// Computing the crc
		std::ostringstream crcStream;
		for (int i = 0; i<4; i++)
			crcStream << crc[i];
		std::string crc_to_store = crcStream.str();
		prep_stmt->setString(8, crc_to_store); // Set crc

		prep_stmt->execute();

	  delete stmt;
	  delete con;
	} catch (sql::SQLException &e) {
	  cout << "# ERR: SQLException in " << __FILE__;
	  cout << "(" << __FUNCTION__ << ") on line " »
	     << __LINE__ << endl;
	  cout << "# ERR: " << e.what();
	  cout << " (MySQL error code: " << e.getErrorCode();
	  cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}
}*/
