#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <algorithm>

#include <mysqlx/xdevapi.h>

// Define Infinite (Using INT_MAX caused overflow problems) 
#define INF 10000 

struct Point
{
	int x;
	int y;
};

class CoverageArea
{
private:
	std::vector<Point> polygon;

	bool doIntersect(Point p1, Point q1, Point p2, Point q2);
	int orientation(Point p, Point q, Point r);
	bool onSegment(Point p, Point q, Point r);

public:
	CoverageArea();

	bool isInside(int x, int y);
};

