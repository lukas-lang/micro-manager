#pragma once

#include <exception>
#include <string>

#include "..\MMDevice\DeviceBase.h"

#define ERRH_START try {
#define ERRH_END } catch (error_code e) { if(e.msg != "") SetErrorText(CONTROLLER_ERROR, e.msg.c_str()); return e.code;} return DEVICE_OK;

class error_code : public std::exception
{
public:
	error_code(int code, std::string msg = "");

	int code;
	std::string msg;

	static void ThrowErr(int code) throw (error_code);
}; 
