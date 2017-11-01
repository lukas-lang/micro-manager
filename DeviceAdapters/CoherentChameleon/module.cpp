///////////////////////////////////////////////////////////////////////////////
// FILE:          module.cpp
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

#include "../../MMDevice/ModuleInterface.h"

#include "CoherentChameleon.h"

const char* controllerName = "CoherentChameleon";

// Exported MMDevice API
MODULE_API void InitializeModuleData()
{
	RegisterDevice(controllerName, MM::ShutterDevice, "Coherent Chameleon Laser");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0)
		return 0;

	std::string deviceName_(deviceName);

	if (deviceName_ == controllerName)
	{
		CoherentChameleon* s = new CoherentChameleon();
		return s;
	}
	
	return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}