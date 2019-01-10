#include "stdafx.h"
#include "PacketProcessor.h"

std::vector<int> x;
std::vector<int> y;
std::vector<double> d;

PacketProcessor::PacketProcessor(int count)
{
	esp_number = count;
}

//Method that estimates the distance (in meters) starting from the RSSI
double PacketProcessor::getDistanceFromRSSI(double rssi) {
	double rssiAtOneMeter = -55;
	double d = pow(10, (rssiAtOneMeter - rssi) / 22);
	return d;
}

//Method that calculates the distance among two points (x1,y1) , (x2,y2)
double PacketProcessor::dist(double x1, double y1, double x2, double y2) {
	return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

//Method that defines the Mean Square Error (MSE) function
double PacketProcessor::meanSquareError(const column_vector& m) {
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
void PacketProcessor::trilaterate(double * pos_x, double * pos_y) {

	try {
		column_vector starting_point = { 0, 0 };

		dlib::find_min_using_approximate_derivatives(dlib::bfgs_search_strategy(),
			dlib::objective_delta_stop_strategy(1e-7),
			&meanSquareError,
			starting_point, -1);

		*pos_x = starting_point(0);
		*pos_y = starting_point(1);

		//std::cout << "   Coordinates: X=" << *pos_x << ", Y=" << *pos_y << std::endl << std::endl;
	}
	catch (std::exception& e) {
		std::cout << e.what() << std::endl;
	}
}

void PacketProcessor::process() {

	double pos_x = -1;
	double pos_y = -1;

	x.clear();
	y.clear();
	d.clear();

	try {
		mysqlx::Session session("localhost", 33060, "pds_user", "password");

		try {
			mysqlx::Schema myDb = session.getSchema("pds_db");

			mysqlx::Table packetTable = myDb.getTable("Packet");
			mysqlx::Table espTable = myDb.getTable("ESP");
			mysqlx::Table devicesTable = myDb.getTable("Devices");

			mysqlx::RowResult retrievedPackets;
			mysqlx::Row row;

			//Get Hash and MAC address of the current packet
			retrievedPackets = packetTable.select("hash", "addr").execute();

			for (mysqlx::Row packet : retrievedPackets.fetchAll()) {
				uint32_t current_hash = (uint32_t)packet[0];
				std::string current_address = packet[1];				

				//Count how many ESPs have received this packet (this hash)
				mysqlx::RowResult hashCount = packetTable.select("count(DISTINCT(esp_id))").where("hash=:current_hash AND to_be_deleted = 0").bind("current_hash", current_hash).execute();
				row = hashCount.fetchOne();
				int counter = row[0];

				if (counter >= esp_number) { //floor(esp_number / 2) + 1) { //the packet has been received by at least 3 ESPs (Note: change this value in debug/testing)
					uint64_t average_timestamp = 0;
					int count = 0;

					//Get the ESP-ID and the RSSI from *ALL* the ESPs which have received the packet
					//N.B.: this query gives multiple rows --> one row for each ESP which has received the packet
					mysqlx::RowResult multiple_query_result = packetTable.select("esp_id", "rssi", "unix_timestamp(timestamp)").where("hash=:current_hash").bind("current_hash", current_hash).execute();

					std::cout << " Hash " << current_hash << " with MAC " << current_address;
					std::cout << " (received by " << counter << " ESPs)";
					std::cout << std::endl << "  Current ESP values:" << std::endl;

					for (mysqlx::Row rows : multiple_query_result.fetchAll()) {
						uint32_t current_esp_id = (uint32_t)rows[0];
						int current_rssi = (int)rows[1];
						average_timestamp += (uint64_t)rows[2];

						std::cout << "   ESP-ID=" << current_esp_id << ", RSSI=" << current_rssi;

						//Get the coordinates of the ESP who has received the current packet
						mysqlx::RowResult esp_coordinates = espTable.select("x", "y").where("esp_id=:current_esp_id").bind("current_esp_id", current_esp_id).execute();
						row = esp_coordinates.fetchOne();
						int current_esp_x = (int)row[0];
						int current_esp_y = (int)row[1];

						//Estimate the distance from the RSSI
						double current_distance = getDistanceFromRSSI(current_rssi);

						std::cout << ", X=" << current_esp_x << ", Y=" << current_esp_y << ", Distance=" << current_distance << std::endl;

						//Add the values in each vector
						x.push_back(current_esp_x);
						y.push_back(current_esp_y);
						d.push_back(current_distance);

						count++;
					}

					//Trilaterate the device with the current MAC address getting its coordinates pos_x and pos_y
					trilaterate(&pos_x, &pos_y);

					//Count how many packets with this hash has been received
					/*mysqlx::RowResult hashCount = packetTable.select("count(esp_id)").where("hash=:current_hash").bind("current_hash", current_hash).execute();
					row = hashCount.fetchOne();
					counter = (uint32_t)row[0];*/

					// Computing the timestamp
					average_timestamp = floor(average_timestamp / count);
					time_t rawtime = average_timestamp;
					struct tm timeinfo;
					char average_time[20];
					localtime_s(&timeinfo, &rawtime);
					strftime(average_time, 20, "%F %T", &timeinfo);

					if (ca.isInside(pos_x, pos_y)) {
						devicesTable.insert("mac", "x", "y", "timestamp").values(current_address, pos_x, pos_y, average_time).execute();
						std::cout << "    Device within the coverage area." << std::endl;
						std::cout << "    Coordinates of " << current_address << " : X=" << pos_x << ", Y=" << pos_y << std::endl << std::endl;
					}
					else {
						std::cout << "    The device is outside the coverage area, hence it won't be inserted in the device table." << std::endl;
						std::cout << "    Coordinates of " << current_address << " : X=" << pos_x << ", Y=" << pos_y << std::endl << std::endl;
					}
					
					// Remove all the packets that were trilaterated
					packetTable.update().set("to_be_deleted", 1).where("hash=:current_hash").bind("current_hash", current_hash).execute();
					//packetTable.remove().where("hash=:current_hash").bind("current_hash", current_hash).execute();
				}
				else if (counter != 0) {
					std::cout << " Hash " << current_hash << " with MAC " << current_address;
					std::cout << " (received by " << counter << " ESPs)";
					std::cout << " ==> this packet won't be trilaterated" << std::endl;

					mysqlx::RowResult timestamp_result = packetTable.select("unix_timestamp(timestamp)").where("hash=:current_hash").bind("current_hash", current_hash).execute();
					row = timestamp_result.fetchOne();

					uint64_t ts = row[0];

					//If the packet has been received more than 2 minutes ago then delete it
					if ( static_cast<uint64_t> (time(NULL)) - ts > 30) {
						packetTable.update().set("to_be_deleted", 1).where("hash=:current_hash").bind("current_hash", current_hash).execute();
						std::cout << " ==> Packet is too old and it will be deleted." << std::endl << std::endl;
					}
					//packetTable.remove().where("hash=:current_hash AND TIMESTAMPDIFF(MINUTE, '2018-09-10', '2018-05-01') > 2").bind("current_hash", current_hash).execute();
				}
			}

			packetTable.remove().where("to_be_deleted = 1").execute();
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
