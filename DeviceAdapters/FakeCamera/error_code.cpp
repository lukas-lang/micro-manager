#include "error_code.h"

error_code::error_code(int code, std::string msg) : code(code), msg(msg) {}

void error_code::ThrowErr(int code) throw (error_code)
{
	if (code != DEVICE_OK)
		throw error_code(code);
}