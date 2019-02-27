#include "stdafx.h"
#include "CoverageArea.h"

/*
	Collect the list of ESP from the DB to define the coverage area.
*/
CoverageArea::CoverageArea()
{
	try {
		mysqlx::Session session("localhost", 33060, "pds_user", "password");

		try {
			mysqlx::Schema myDb = session.getSchema("pds_db");

			mysqlx::Table espTable = myDb.getTable("ESP");

			//Get the coordinates of the ESP who has received the current packet
			mysqlx::RowResult esp_coordinates = espTable.select("x", "y").execute();

			for (mysqlx::Row row : esp_coordinates.fetchAll()) {
				Point p = { row[0], row[1]};
				polygon.push_back(p);
			}
		}
		catch (std::exception &err) {
			//std::cout << "The following error occurred: " << err.what() << std::endl;
			exit(1);
		}
	}
	catch (std::exception &err) {
		//std::cout << "The database session could not be opened: " << err.what() << std::endl;
		exit(1);
	}
}

// Given three colinear points p, q, r, the function checks if 
// point q lies on line segment 'pr' 
bool CoverageArea::onSegment(Point p, Point q, Point r)
{
	if (q.x <= std::max(p.x, r.x) && q.x >= std::min(p.x, r.x) &&
		q.y <= std::max(p.y, r.y) && q.y >= std::min(p.y, r.y))
		return true;
	return false;
}

// To find orientation of ordered triplet (p, q, r). 
// The function returns following values 
// 0 --> p, q and r are colinear 
// 1 --> Clockwise 
// 2 --> Counterclockwise 
int CoverageArea::orientation(Point p, Point q, Point r)
{
	int val = (int) ( (q.y - p.y) * (r.x - q.x) -
		(q.x - p.x) * (r.y - q.y) );

	if (val == 0) return 0;  // colinear 
	return (val > 0) ? 1 : 2; // clock or counterclock wise 
}

// The function that returns true if line segment 'p1q1' 
// and 'p2q2' intersect. 
bool CoverageArea::doIntersect(Point p1, Point q1, Point p2, Point q2)
{
	// Find the four orientations needed for general and 
	// special cases 
	int o1 = orientation(p1, q1, p2);
	int o2 = orientation(p1, q1, q2);
	int o3 = orientation(p2, q2, p1);
	int o4 = orientation(p2, q2, q1);

	// General case 
	if (o1 != o2 && o3 != o4)
		return true;

	// Special Cases 
	// p1, q1 and p2 are colinear and p2 lies on segment p1q1 
	if (o1 == 0 && onSegment(p1, p2, q1)) return true;

	// p1, q1 and p2 are colinear and q2 lies on segment p1q1 
	if (o2 == 0 && onSegment(p1, q2, q1)) return true;

	// p2, q2 and p1 are colinear and p1 lies on segment p2q2 
	if (o3 == 0 && onSegment(p2, p1, q2)) return true;

	// p2, q2 and q1 are colinear and q1 lies on segment p2q2 
	if (o4 == 0 && onSegment(p2, q1, q2)) return true;

	return false; // Doesn't fall in any of the above cases 
}

// Returns true if the point p lies inside the polygon[] with n vertices 
bool CoverageArea::isInside(double x, double y)
{
	int n = polygon.size();
	Point p = { x, y };

	// if the number of vertices is less then 2, the problem is not valid. Return true for debugging purposes.
	if (n < 2)  
		return true;
	// if the number of vertices is equal to 2, the point is surely colinear to the segment, hence we only need to check whether the point is on the segment.
	else if (n == 2)
		return onSegment(polygon[0], p, polygon[1]);

	// Create a point for line segment from p to infinite 
	Point extreme = { INF, p.y };

	// Count intersections of the above line with sides of polygon 
	int count = 0, i = 0;
	do
	{
		int next = (i + 1) % n;

		// Check if the line segment from 'p' to 'extreme' intersects 
		// with the line segment from 'polygon[i]' to 'polygon[next]' 
		if (doIntersect(polygon[i], polygon[next], p, extreme))
		{
			// If the point 'p' is colinear with line segment 'i-next', 
			// then check if it lies on segment. If it lies, return true, 
			// otherwise false 
			if (orientation(polygon[i], p, polygon[next]) == 0)
				return onSegment(polygon[i], p, polygon[next]);

			count++;
		}
		i = next;
	} while (i != 0);

	// Return true if count is odd, false otherwise 
	return count & 1;  // Same as (count%2 == 1) 
}