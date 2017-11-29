#pragma once

#include "PM100D.h"

#include "../MMDevice/MMDevice.h"
#include "../MMDevice/DeviceBase.h"

const extern char* device_name;

#define ERR_DEVICE_CHANGE_FORBIDDEN 10001
#define ERR_COMMUNICATION 10002

class PM100USB : public CGenericBase<PM100USB>
{
public:
	PM100USB();

	// Device API
	bool Busy() override;
	int Initialize() override;
	int Shutdown() override;
	void GetName(char * name) const override;

	// Action interface
	int OnDevice(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPower(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnWavelength(MM::PropertyBase* pProp, MM::ActionType eAct);
private:
	bool initialized_;

	char* deviceID_;
	ViSession deviceHandle_;
};