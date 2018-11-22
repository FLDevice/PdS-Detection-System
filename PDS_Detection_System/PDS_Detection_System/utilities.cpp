#include "stdafx.h"
#include <iostream>
#include <exception>
#include "Utilities.h"

/** Use std::cin to get an int for the standard input. May throw exceptions. */
int Utilities::getIntFromInput() {

	int x;
	std::cin >> x;

	// if not an int
	if (std::cin.fail()) {
		std::cin.clear();
		std::cin.ignore(32767, '\n');
		throw std::exception("not a number");
	}
	// if an int
	else {
		std::cin.ignore(32767, '\n');
	}

	return x;
}
