///////////////////////////////////////////////////////////////////////////////
// FILE:          CoherentChameleon.cpp
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

#include <algorithm>
#include <math.h>
#include <sstream>
#include <string>
#include <algorithm>

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/ModuleInterface.h"
#include "../../MMDevice/DeviceUtils.h"

#include "CoherentChameleon.h"
#include "Util.h"
#include "fault_codes.h"
#include "commands.h"

// Controller
const char* g_device_name = "Coherent chameleon Ultra laser";

///////////////////////////////////////////////////////////////////////////////
// Controller implementation
// ~~~~~~~~~~~~~~~~~~~~

CoherentChameleon::CoherentChameleon() :
	initialized_(false)
{
	InitializeDefaultErrorMessages();

	// Name
	CreateProperty(MM::g_Keyword_Name, g_device_name, MM::String, true);

	// Description
	CreateProperty(MM::g_Keyword_Description, "Device adapter for the Coherent Chameleon Ultra laser", MM::String, true);

	// Port
	CPropertyAction* pAct = new CPropertyAction(this, &CoherentChameleon::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
}

CoherentChameleon::~CoherentChameleon()
{
}

bool CoherentChameleon::Busy()
{
	ERRH_START
		return QueryParameter(LASER) == "On"
		&& (QueryParameter(TUNING_STATUS) != "0"
		|| QueryParameter(LIGHT_REG_STATUS) == "2"
		|| QueryParameter(DIODE1_SERVO_STATUS) == "2"
		|| QueryParameter(DIODE2_SERVO_STATUS) == "2"
		|| QueryParameter(VANADATE_SERVO_STATUS) == "2"
		|| QueryParameter(LBO_SERVO_STATUS) == "2"
		|| QueryParameter(ETALON_SERVO_STATUS) == "2");
	ERRH_END
}

void CoherentChameleon::GetName(char* name) const
{
	CDeviceUtils::CopyLimitedString(name, g_device_name);
}


int CoherentChameleon::Initialize()
{
	ERRH_START
		SendCommand("ECHO=0", false);
	SendCommand("PROMPT=0", false);

	std::vector<std::string>
		offOn = vector_of("Off")("On"),
		disEn = vector_of("Disabled")("Enabled"),
		olsf = vector_of("Open")("Locked")("Seeking")("Fault");

	SetPropertyNames(MapProperty(LBO_HEATER, "Enable LBO heater"), offOn);
	SetPropertyNames(MapProperty(SEARCH_MODELOCK, "Search for modelock"), disEn);
	SetPropertyNames(MapProperty(SHUTTER, "Shutter", true), vector_of("Closed")("Open"));

	MapProperty(TUNING_LIMIT_MIN, "Minimum wavelength (nm)", true, MM::Float);
	MapProperty(TUNING_LIMIT_MAX, "Maximum wavelength (nm)", true, MM::Float);
	MapNumProperty(WAVELENGTH, "Wavelength (nm)", stoi(QueryParameter(TUNING_LIMIT_MIN)), stoi(QueryParameter(TUNING_LIMIT_MAX)));

	MapTriggerProperty(FLASH, "Flash Verdi laser output below threshold to recenter mode");
	MapTriggerProperty(LBO_OPTIMIZE, "Begin LBO optimization routine");
	MapTriggerProperty(HOME_STEPPER, "Home the tuning motor (takes 3-30s)");
	MapTriggerProperty(RECOVERY, "Initiate recovery (takes up to 2min)");

	SetPropertyNames(MapProperty(ALIGN, "Alignment mode", true), disEn);
	MapProperty(ALIGNP, "Alignment mode power (mW)", true, MM::Float);
	MapProperty(ALIGNW, "Alignment mode wavelength (nm)", true, MM::Float);

	int laserId = MapProperty(LASER, "Laser status", true);
	SetPropertyNames(laserId, vector_of("Off")("On")("Off due to fault"));
	SetAllowedValues(LASER, offOn); //Set name for state 2 without allowing it to be set manually

	SetPropertyNames(MapProperty(KEYSWITCH, "Keyswitch status", true), offOn);

	MapProperty(UF_POWER, "Actual UF (Chameleon) power (mW)", true, MM::Float);
	MapProperty(CAVITY_PEAK_HOLD, "Cavity peak hold status", true);
	SetPropertyNames(MapProperty(CAVITY_PZT_MODE, "Cavity PZT mode", true), vector_of("Auto")("Manual"));
	MapProperty(CAVITY_PZT_X, "Cavity PZT X (Rd) voltage (V)", true, MM::Float);
	MapProperty(CAVITY_PZT_Y, "Cavity PZT Y (Rd) voltage (V)", true, MM::Float);
	SetPropertyNames(MapProperty(PUMP_PEAK_HOLD, "Pump peak hold status", true), offOn);
	SetPropertyNames(MapProperty(PUMP_PZT_MODE, "Pump PZT mode", true), vector_of("Auto")("Manual"));
	MapProperty(PUMP_PZT_X, "Pump PZT X (Rd) voltage (V)", true, MM::Float);
	MapProperty(PUMP_PZT_Y, "Pump PZT Y (Rd) voltage (V)", true, MM::Float);
	//SetPropertyNames(MapProperty(POWER_TRACK, "PowerTrack state", true), offOn); //TODO: Fix command
	SetPropertyNames(MapProperty(MODELOCKED, "Chameleon Ultra state", true), vector_of("Off (Standby)")("Modelocked")("CW"));
	MapProperty(PUMP_SETTING, "Pump power setpoint (fraction of QS to CW pump band)", true, MM::Float);
	SetPropertyNames(MapProperty(TUNING_STATUS, "Tuning state", true), vector_of("Ready")("Tuning in progress")("Searching for modelock")("Recovery operation in progress"));
	SetPropertyNames(MapProperty(SEARCH_MODELOCK, "Modelock search status", true), disEn);
	SetPropertyNames(MapProperty(HOMED, "Tuning motor homing status", true), vector_of("Has not been homed")("Has been homed"));
	MapProperty(STEPPER_POSITION, "Stepper motor position (counts)", true, MM::Integer);
	MapProperty(CURRENT, "Average diode current (A)", true, MM::Float);
	MapProperty(DIODE1_CURRENT, "Diode #1 current (A)", true, MM::Float);
	MapProperty(DIODE2_CURRENT, "Diode #2 current (A)", true, MM::Float);
	MapProperty(BASEPLATE_TEMP, "Head baseplate temperature (deg C)", true, MM::Float);
	MapProperty(DIODE1_TEMP, "Diode #1 temperature (deg C)", true, MM::Float);
	MapProperty(DIODE2_TEMP, "Diode #2 temperature (deg C)", true, MM::Float);
	MapProperty(VANADATE_TEMP, "Vanadate temperature (deg C)", true, MM::Float);
	MapProperty(LBO_TEMP, "LBO temperature (deg C)", true, MM::Float);
	MapProperty(ETALON_TEMP, "Etalon temperature (deg C)", true, MM::Float);
	MapProperty(DIODE1_SET_TEMP, "Diode #1 set temperature (deg C)", true, MM::Float);
	MapProperty(DIODE2_SET_TEMP, "Diode #2 set temperature (deg C)", true, MM::Float);
	MapProperty(VANADATE_SET_TEMP, "Vanadate set temperature (deg C)", true, MM::Float);
	MapProperty(LBO_SET_TEMP, "LBO set temperature (deg C)", true, MM::Float);
	MapProperty(ETALON_SET_TEMP, "Etalon set temperature (deg C)", true, MM::Float);
	MapProperty(DIODE1_TEMP_DRIVE, "Diode #1 temperature servo drive setting", true);
	MapProperty(DIODE2_TEMP_DRIVE, "Diode #2 temperature servo drive setting", true);
	MapProperty(VANADATE_DRIVE, "Vanadate temperature servo drive setting", true);
	MapProperty(LBO_DRIVE, "LBO temperature servo drive setting", true);
	MapProperty(ETALON_DRIVE, "Etalon temperature servo drive setting", true);
	MapProperty(DIODE1_HEATSINK_TEMP, "Diode #1 heat sink temperature (deg C)", true, MM::Float);
	MapProperty(DIODE2_HEATSINK_TEMP, "Diode #2 heat sink temperature (deg C)", true, MM::Float);
	SetPropertyNames(MapProperty(LIGHT_REG_STATUS, "Light loop status", true), olsf);
	SetPropertyNames(MapProperty(DIODE1_SERVO_STATUS, "Diode #1 temperature servo status", true), olsf);
	SetPropertyNames(MapProperty(DIODE2_SERVO_STATUS, "Diode #2 temperature servo status", true), olsf);
	SetPropertyNames(MapProperty(VANADATE_SERVO_STATUS, "Vanadate temperature servo status", true), olsf);
	SetPropertyNames(MapProperty(LBO_SERVO_STATUS, "LBO temperature servo status", true), olsf);
	SetPropertyNames(MapProperty(ETALON_SERVO_STATUS, "Etalon temperature servo status", true), olsf);
	MapProperty(DIODE1_HOURS, "Diode #1 operating time (h)", true, MM::Float);
	MapProperty(DIODE2_HOURS, "Diode #2 operating time (h)", true, MM::Float);
	MapProperty(HEAD_HOURS, "Head operating time (h)", true, MM::Float);
	MapProperty(DIODE1_VOLTAGE, "Diode #1 voltage (V)", true, MM::Float);
	MapProperty(DIODE2_VOLTAGE, "Diode #2 voltage (V)", true, MM::Float);
	MapProperty(SOFTWARE, "Power supply software version", true);
	MapProperty(BAT_VOLTS, "Battery voltage (V)", true, MM::Float);
	SetPropertyNames(MapProperty(AUTOMODELOCK, "Automodelock routing status", true), disEn);
	//MapProperty(PZT_CONTROL_STATE, "PZT control state", true); //TODO: Fix command

	MapProperty(PZTXCM, "Last power map result for cavity X PZT position (% of available range)", true, MM::Float);
	MapProperty(PZTXCP, "Current cavity X PZT position (% of available range)", true, MM::Float);
	MapProperty(PZTXPM, "Last power map result for pump X PZT position (% of available range)", true, MM::Float);
	MapProperty(PZTXPP, "Current pump X PZT position (% of available range)", true, MM::Float);
	MapProperty(PZTYCM, "Last power map result for cavity Y PZT position (% of available range)", true, MM::Float);
	MapProperty(PZTYCP, "Current cavity Y PZT position (% of available range)", true, MM::Float);
	MapProperty(PZTYPM, "Last power map result for pump Y PZT position (% of available range)", true, MM::Float);
	MapProperty(PZTYPP, "Current pump Y PZT position (% of available range)", true, MM::Float);
	MapProperty(RH, "Relative humidity (%)", true, MM::Float);
	MapProperty(SN, "Serial number", true);
	MapProperty(ST, "Operating status", true);

	CreateProperty("Active faults", "No faults", MM::String, false, new CPropertyActionEx(this, &CoherentChameleon::OnFaults, 0));
	CreateProperty("Fault history", "No faults", MM::String, false, new CPropertyActionEx(this, &CoherentChameleon::OnFaults, 1));

	SetProperty("Wavelength (nm)", "800");

	initialized_ = true;
	ERRH_END
}

int CoherentChameleon::Shutdown()
{
	if (initialized_)
	{
		SetProperty("Wavelength (nm)", "800");
		initialized_ = false;
	}
	return DEVICE_OK;
}

std::string CoherentChameleon::SendCommand(std::string cmd, bool checkError) throw (error_code)
{
	int ret = SendSerialCommand(port_.c_str(), cmd.c_str(), "\r\n");

	if (ret != DEVICE_OK)
		throw error_code(ret);

	std::string answer;
	ret = GetSerialAnswer(port_.c_str(), "\r\n", answer);

	if (ret != DEVICE_OK)
		throw error_code(ret);

	if (checkError)
	{
		if (answer.substr(0, 12) == "OUT OF RANGE")
			throw error_code(CONTROLLER_ERROR, "Parameter out of range: " + answer.substr(12));
		if (answer.substr(0, 14) == " Command Error")
			throw error_code(CONTROLLER_ERROR, "Illegal instruction: " + answer.substr(14));
		if (answer.substr(0, 12) == " Query Error")
			throw error_code(CONTROLLER_ERROR, "Illegal instruction: " + answer.substr(12));
	}

	return answer;
}

std::string CoherentChameleon::QueryParameter(std::string param) throw (error_code)
{
	return SendCommand("?" + param);
}
void CoherentChameleon::SetParameter(std::string param, std::string value) throw (error_code)
{
	SendCommand(param + "=" + value);
}

std::vector<std::string> CoherentChameleon::GetFaults(bool history)
{
	std::vector<std::string> faults;

	std::istringstream iss(QueryParameter(history ? FAULT_HISTORY : FAULTS));

	std::string fault;

	while (std::getline(iss, fault, '&'))
		faults.push_back(fault_codes.at(fault));

	return faults;
}

int CoherentChameleon::OnFaults(MM::PropertyBase* pProp, MM::ActionType eAct, long history)
{
	ERRH_START
		if (eAct == MM::BeforeGet || eAct == MM::AfterSet)
		{
			std::vector<std::string> faults = GetFaults(history);

			SetAllowedValues(history ? "Active faults" : "Fault history", faults);
			pProp->Set(faults.size() > 0 ? "Click to expand" : "No faults");
		}
	ERRH_END
}

///////////////////////////////////////////////////////////////////////////////
// Action handlers
///////////////////////////////////////////////////////////////////////////////

int CoherentChameleon::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(port_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			// revert
			pProp->Set(port_.c_str());
			return ERR_PORT_CHANGE_FORBIDDEN;
		}

		pProp->Get(port_);
	}

	return DEVICE_OK;
}

// Shutter API

int CoherentChameleon::SetOpen(bool open)
{
	ERRH_START
		//SetParameter(SHUTTER, open ? "1" : "0");
	ERRH_END
}

int CoherentChameleon::GetOpen(bool& open)
{
	ERRH_START
		return QueryParameter(SHUTTER) == "1";
	ERRH_END
}

// ON for deltaT milliseconds
// other implementations of Shutter don't implement this
// is this perhaps because this blocking call is not appropriate
int CoherentChameleon::Fire(double deltaT)
{
	ERRH_START
		SetOpen(true);
	CDeviceUtils::SleepMs(deltaT + .5);
	SetOpen(false);
	ERRH_END
}
