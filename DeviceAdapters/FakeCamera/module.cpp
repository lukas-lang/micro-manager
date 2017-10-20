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