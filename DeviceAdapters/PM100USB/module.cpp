#include "../../MMDevice/ModuleInterface.h"

#include "PM100USB.h"

// Exported MMDevice API
MODULE_API void InitializeModuleData()
{
	RegisterDevice(device_name, MM::GenericDevice, "ThorLabs PM100USB power meter device adapter");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0)
		return 0;

	std::string deviceName_(deviceName);

	if (deviceName_ == device_name)
	{
		PM100USB* s = new PM100USB();
		return s;
	}

	return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}