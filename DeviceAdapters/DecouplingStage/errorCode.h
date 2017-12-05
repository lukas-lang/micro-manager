#pragma once

#include "..\MMDevice\DeviceBase.h"
#include <exception>
#include <string>

#define CONTROLLER_ERROR          20000
#define INVALID_INPUT             20001
#define ERRH_START try {
#define ERRH_END } catch (errorCode e) { if(e.msg != "") SetErrorText(CONTROLLER_ERROR, e.msg.c_str()); return e.code;} return DEVICE_OK;

class errorCode : public std::exception
{
public:
	errorCode(int code, std::string msg = "");

	int code;
	std::string msg;

	static void ThrowErr(int code, MM::Device* source = nullptr) throw (errorCode);
}; 
