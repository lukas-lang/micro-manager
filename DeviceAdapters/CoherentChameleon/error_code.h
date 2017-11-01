///////////////////////////////////////////////////////////////////////////////
// FILE:          error_code.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   A device adapter for the Coherent Chameleon Ultra laser
//
// AUTHOR:        Lukas Lang
//
// COPYRIGHT:     2017 Lukas Lang
// LICENSE:       Licensed under the Apache License, Version 2.0 (the "License");
//                you may not use this file except in compliance with the License.
//                You may obtain a copy of the License at
//                
//                http://www.apache.org/licenses/LICENSE-2.0
//                
//                Unless required by applicable law or agreed to in writing, software
//                distributed under the License is distributed on an "AS IS" BASIS,
//                WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//                See the License for the specific language governing permissions and
//                limitations under the License.

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
