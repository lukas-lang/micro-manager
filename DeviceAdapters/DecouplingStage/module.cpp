#include "../../MMDevice/ModuleInterface.h"

#include "DecouplingController.h"
#include "DecouplingStage.h"

// Exported MMDevice API
MODULE_API void InitializeModuleData()
{
	RegisterDevice(controllerName, MM::HubDevice, "Decoupling stage controller");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0)
		return 0;

	std::string deviceName_(deviceName);

	if (deviceName_ == controllerName)
	{
		DecouplingController* s = new DecouplingController();
		return s;
	}
	
	if (deviceName_.find(stageName) == 0)
	{
		size_t start = std::string(stageName).size() + 7;
		DecouplingStage* s = new DecouplingStage(deviceName_.substr(start, deviceName_.find(')') - start));
		return s;
	}
	

	return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}