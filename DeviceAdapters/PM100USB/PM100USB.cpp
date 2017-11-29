#include <vector>
#include <string>

#include "PM100USB.h"

const char* device_name = "ThorLabs PM100USB power meter";

PM100USB::PM100USB() : 
	initialized_(false),
	deviceID_(""),
	deviceHandle_(VI_NULL)
{
	CreateProperty("Device", "-", MM::String, false, new CPropertyAction(this, &PM100USB::OnDevice), true);

	ViUInt32 deviceCount;
	ViStatus err = PM100D_findRsrc(0, &deviceCount);

	if (err != VI_SUCCESS)
		deviceCount = 0;

	std::vector<std::string> devices;

	ViChar device[PM100D_BUFFER_SIZE];

	for (int i = 0; i < deviceCount; ++i)
	{
		err = PM100D_getRsrcName(0, i, device);

		if (err != VI_SUCCESS)
			continue;

		devices.push_back(device);
	}

	SetAllowedValues("Device", devices);

	SetErrorText(ERR_DEVICE_CHANGE_FORBIDDEN, "Can't change device after the adapter has been initialized");
	SetErrorText(ERR_COMMUNICATION, "Communication error occured");
}

bool PM100USB::Busy()
{
	return false;
}

int PM100USB::Initialize()
{
	if (!initialized_)
	{
		PM100D_init(deviceID_, VI_OFF, VI_ON, &deviceHandle_);
	
		ViStatus err = VI_SUCCESS;
		ViChar sensor_name[PM100D_BUFFER_SIZE], serial_number[PM100D_BUFFER_SIZE], cal_message[PM100D_BUFFER_SIZE];
		ViInt16 sens_type, sens_subtype, flags;

		err = PM100D_getSensorInfo(deviceHandle_, sensor_name, serial_number, cal_message, &sens_type, &sens_subtype, &flags);

		CreateProperty("Sensor name", sensor_name, MM::String, true);
		CreateProperty("Serial number", serial_number, MM::String, true);
		CreateProperty("Message", cal_message, MM::String, true);

		CreateProperty("Power (mW)", "", MM::Float, true, new CPropertyAction(this, &PM100USB::OnPower));
		CreateProperty("Wavelength (nm)", "", MM::Float, false, new CPropertyAction(this, &PM100USB::OnWavelength));

		initialized_ = true;
	}

	return DEVICE_OK;
}

int PM100USB::Shutdown()
{
	if (initialized_)
	{
		initialized_ = false;
	}

	return DEVICE_OK;
}

void PM100USB::GetName(char * name) const
{
	CDeviceUtils::CopyLimitedString(name, device_name);
}

int PM100USB::OnDevice(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(deviceID_);
		return DEVICE_OK;
	}
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			pProp->Set(deviceID_);
			return ERR_DEVICE_CHANGE_FORBIDDEN;
		}

		std::string val;
		pProp->Get(val);

		deviceID_ = new char[val.size() + 1];
		strcpy(deviceID_, val.c_str());

		return DEVICE_OK;
	}

	return DEVICE_OK;
}

int PM100USB::OnPower(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		ViStatus err = VI_SUCCESS;
		ViReal64 power = 0.0;
		ViInt16 power_unit;

		err = PM100D_getPowerUnit(deviceHandle_, &power_unit);

		if (err != VI_SUCCESS)
			return ERR_COMMUNICATION;

		err = PM100D_measPower(deviceHandle_, &power);

		if (err != VI_SUCCESS)
			return ERR_COMMUNICATION;

		if (power_unit == PM100D_POWER_UNIT_DBM)
			power = pow(10, power / 10);
		else
			power *= 1000;

		std::ostringstream os;
		os << power;

		pProp->Set(os.str().c_str());
	}
}
int PM100USB::OnWavelength(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		double wavelength;

		ViStatus err = PM100D_getWavelength(deviceHandle_, PM100D_ATTR_SET_VAL, &wavelength);

		if (err != VI_SUCCESS)
			return ERR_COMMUNICATION;

		std::ostringstream os;
		os << wavelength;

		pProp->Set(os.str().c_str());
		return DEVICE_OK;
	}
	else if (eAct == MM::AfterSet)
	{
		std::string val;
		pProp->Get(val);

		ViStatus err = PM100D_setWavelength(deviceHandle_, atof(val.c_str()));

		if (err != VI_SUCCESS)
			return ERR_COMMUNICATION;

		return DEVICE_OK;
	}

	return DEVICE_OK;
}