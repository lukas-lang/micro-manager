#include "../../MMDevice/ModuleInterface.h"

#include "FakeCamera.h"

// Exported MMDevice API
MODULE_API void InitializeModuleData()
{
	RegisterDevice(cameraName, MM::CameraDevice, "Fake camera that loads images from disk");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0)
		return 0;

	std::string deviceName_(deviceName);

	if (deviceName_ == cameraName)
	{
		FakeCamera* s = new FakeCamera();
		return s;
	}
	
	return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}