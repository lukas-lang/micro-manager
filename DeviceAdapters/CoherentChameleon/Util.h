#pragma once

#include <string>

template <typename T>
std::string to_string(const T& expr)
{
	std::ostringstream os;
	os << expr;
	return os.str();
}