#pragma once

#include <stdlib.h>
#include <stdio.h>

#include <thread>
#include <mysqlx/xdevapi.h>
#include <dlib-19.16\dlib\optimization.h>
#include <dlib-19.16\dlib\global_optimization.h>

#define ESP_THRESHOLD 3

typedef dlib::matrix<double, 0, 1> column_vector;

class Triangulation
{
private:
	std::mutex mtx; 
	std::condition_variable cv;

	double getDistanceFromRSSI(double rssi);

	double dist(double x1, double y1, double x2, double y2);

	double meanSquareError(const column_vector& m);

	void getCoordinates(int * pos_x, int * pos_y);

	void triangulate_data(int esp_number);
public:
	Triangulation();
	void loop();
};

