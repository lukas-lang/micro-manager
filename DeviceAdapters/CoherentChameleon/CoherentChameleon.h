///////////////////////////////////////////////////////////////////////////////
// FILE:          CoherentChameleon.h
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

#include <string>
#include <vector>
#include <map>

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/DeviceUtils.h"

#include "DeviceUtil.h"
#include "Util.h"

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_PORT_CHANGE_FORBIDDEN    10004
#define ERR_DEVICE_NOT_FOUND         10005

#include  "error_code.h"

extern const char* g_device_name;

class CoherentChameleon : public DeviceUtil<CShutterBase<CoherentChameleon>, CoherentChameleon>
{

public:
	CoherentChameleon();
	~CoherentChameleon();

	// MMDevice API
	// ------------
	int Initialize();
	int Shutdown();

	void GetName(char* pszName) const;
	bool Busy();

	// Shutter API
	int SetOpen(bool open = true);
	int GetOpen(bool& open);
	int Fire(double deltaT);

	// action interface
	// ----------------
	int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);

	std::string SendCommand(std::string cmd, bool checkError = true) throw (error_code);
	std::string QueryParameter(std::string param) throw (error_code);
	void SetParameter(std::string param, std::string value) throw (error_code);

	std::vector<std::string> GetFaults(bool history);
	int OnFaults(MM::PropertyBase* pProp, MM::ActionType eAct, long history);
  
private:
   bool initialized_;
   
   std::string port_;
};
