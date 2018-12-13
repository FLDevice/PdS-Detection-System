#include "stdafx.h"
#include "Triangulation.h"


//Method that estimates the distance (in meters) starting from the RSSI
double Triangulation::getDistanceFromRSSI(double rssi) {
	double rssiAtOneMeter = -59;
	double d = pow(10, (rssiAtOneMeter - rssi) / 20);
	return d;
}

//Method that calculates the distance among two points (x1,y1) , (x2,y2)
double Triangulation::dist(double x1, double y1, double x2, double y2) {
	return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}

//Method that defines the Mean Square Error (MSE) function
double Triangulation::meanSquareError(const column_vector& m) {
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
void Triangulation::getCoordinates(int * pos_x, int * pos_y) {

	try {
		column_vector starting_point = { 0, 0 };

		dlib::find_min_using_approximate_derivatives(dlib::bfgs_search_strategy(),
			dlib::objective_delta_stop_strategy(1e-7),
			meanSquareError,
			starting_point, -1);

		*pos_x = starting_point(0);
		*pos_y = starting_point(1);

		//std::cout << "   Coordinates: X=" << *pos_x << ", Y=" << *pos_y << std::endl << std::endl;
	}
	catch (std::exception& e) {
		std::cout << e.what() << std::endl;
	}
}

void Triangulation::triangulate_data(int esp_number) {

	int pos_x = -1;
	int pos_y = -1;

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

			for (mysqlx::Row row : retrievedPackets.fetchAll()) {
				uint32_t current_hash = (uint32_t)row[0];
				std::string current_address = row[1];

				std::cout << " Hash " << current_hash << " with MAC " << current_address;

				//Count how many ESPs have received this packet (this hash)
				mysqlx::RowResult hashCount = packetTable.select("count(DISTINCT(esp_id))").where("hash=:current_hash").bind("current_hash", current_hash).execute();
				row = hashCount.fetchOne();
				uint32_t counter = (uint32_t)row[0];

				std::cout << " (received by " << counter << " ESPs)";

				if (counter >= floor(esp_number/2) + 1) { //the packet has been received by at least 3 ESPs (Note: change this value in debug/testing)

												//Get the ESP-ID and the RSSI from *ALL* the ESPs which have received the packet
												//N.B.: this query gives multiple rows --> one row for each ESP which has received the packet
					mysqlx::RowResult multiple_query_result = packetTable.select("esp_id", "rssi").where("hash=:current_hash").bind("current_hash", current_hash).execute();

					std::cout << std::endl << "  Current ESP values:" << std::endl;

					for (mysqlx::Row rows : multiple_query_result.fetchAll()) {
						uint32_t current_esp_id = (uint32_t)rows[0];
						int current_rssi = (int)rows[1];

						std::cout << "   ESP-ID=" << current_esp_id << ", RSSI=" << current_rssi;

						//Get the coordinates of the ESP who has received the current packet
						mysqlx::RowResult esp_coordinates = espTable.select("x", "y").where("esp_id=:current_esp_id").bind("current_esp_id", current_esp_id).execute();
						row = esp_coordinates.fetchOne();
						int current_esp_x = (int)row[0];
						int current_esp_y = (int)row[1];

						//Estimate the distance from the RSSI
						double current_distance = getDistanceFromRSSI(current_rssi);

						std::cout << ", X=" << current_esp_x << ", Y=" << current_esp_y << ", Distance=" << current_distance << std::endl;
					}
					//Triangulate the device with the current MAC address getting its coordinates pos_x and pos_y
					getCoordinates(&pos_x, &pos_y);

					packetTable.remove().where("hash=:current_hash").bind("current_hash", current_hash).execute();
				}
				else
					std::cout << " ==> this packet won't be triangulated" << std::endl << std::endl;
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