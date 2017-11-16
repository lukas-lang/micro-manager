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
	return QueryParameter("TUNING STATUS") != "0"
	    || QueryParameter("LIGHT REG STATUS") == "2"
	    || QueryParameter("DIODE1 SERVO STATUS") == "2"
	    || QueryParameter("DIODE2 SERVO STATUS") == "2"
	    || QueryParameter("VANADATE SERVO STATUS") == "2"
	    || QueryParameter("LBO SERVO STATUS") == "2"
	    || QueryParameter("ETALON SERVO STATUS") == "2";
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
	
	SetPropertyNames(MapProperty("LBO HEATER", "Enable LBO heater"), offOn);
	SetPropertyNames(MapProperty("SEARCH MODELOCK", "Search for modelock"), disEn);
	SetPropertyNames(MapProperty("SHUTTER", "Shutter"), vector_of("Closed")("Open"));

	MapProperty("TUNING LIMIT MIN", "Minimum wavelength", true, MM::Float);
	MapProperty("TUNING LIMIT MAX", "Maximum wavelength", true, MM::Float);
	MapNumProperty("WAVELENGTH", "Wavelength", stoi(QueryParameter("TUNING LIMIT MIT")), stoi(QueryParameter("TUNING LIMIT MAX")), MM::Float);

	MapTriggerProperty("FLASH", "Flash Verdi laser output below threshold to recenter mode");
	MapTriggerProperty("LBO OPTIMIZE", "Begin LBO optimization routine");
	MapTriggerProperty("HOME STEPPER", "Home the tuning motor (takes 3-30s)");
	MapTriggerProperty("RECOVERY", "Initiate recovery (takes up to 2min)");

	SetPropertyNames(MapProperty("ALIGN", "Alignment mode"), disEn);
	MapProperty("ALIGNP", "Alignment mode power (mW)", true, MM::Float);
	MapProperty("ALIGNW", "Alignment mode wavelength (nm)", true, MM::Float);

	int laserId = MapProperty("LASER", "Laser status", true);
	SetPropertyNames(laserId, vector_of("Off")("On")("Off due to fault"));
	SetAllowedValues("LASER", offOn); //Set name for state 2 without allowing it to be set manually

	SetPropertyNames(MapProperty("KEYSWITCH", "Keyswitch status", true), offOn);

	MapProperty("UF POWER", "Actual UF (Chameleon) power (mW)", true, MM::Float);
	MapProperty("CAVITY PEAK HOLD", "Cavity peak hold status", true);
	SetPropertyNames(MapProperty("CAVITY PZT MODE", "Cavity PZT mode", true), vector_of("Auto")("Manual"));
	MapProperty("CAVITY PZT X", "Cavity PZT X (Rd) voltage (V)", true, MM::Float);
	MapProperty("CAVITY PZT Y", "Cavity PZT Y (Rd) voltage (V)", true, MM::Float);
	SetPropertyNames(MapProperty("PUMP PEAK HOLD", "Pump peak hold status", true), offOn);
	SetPropertyNames(MapProperty("PUMP PZT MODE", "Pump PZT mode", true), vector_of("Auto")("Manual"));
	MapProperty("PUMP PZT X", "Pump PZT X (Rd) voltage (V)", true, MM::Float);
	MapProperty("PUMP PZT Y", "Pump PZT Y (Rd) voltage (V)", true, MM::Float);
	SetPropertyNames(MapProperty("POWER TRACK", "PowerTrack state", true), offOn);
	SetPropertyNames(MapProperty("MODELOCKED", "Chameleon Ultra state", true), vector_of("Off (Standby)")("Modelocked")("CW"));
	MapProperty("PUMP SETTING", "Pump power setpoint (fraction of QS to CW pump band)", true, MM::Float);
	SetPropertyNames(MapProperty("TUNING STATUS", "Tuning state", true), vector_of("Ready")("Tuning in progress")("Searching for modelock")("Recovery operation in progress"));
	SetPropertyNames(MapProperty("SEARCH MODELOCK", "Modelock search status", true), disEn);
	SetPropertyNames(MapProperty("HOMED", "Tuning motor homing status", true), vector_of("Has not been homed")("Has been homed"));
	MapProperty("STEPPER POSITION", "Stepper motor position (counts)", true, MM::Integer);
	MapProperty("CURRENT", "Average diode current (A)", true, MM::Float);
	MapProperty("DIODE1 CURRENT", "Diode #1 current (A)", true, MM::Float);
	MapProperty("DIODE2 CURRENT", "Diode #2 current (A)", true, MM::Float);
	MapProperty("BASEPLATE TEMP", "Head baseplate temperature (°C)", true, MM::Float);
	MapProperty("DIODE1 TEMP", "Diode #1 temperature (°C)", true, MM::Float);
	MapProperty("DIODE2 TEMP", "Diode #2 temperature (°C)", true, MM::Float);
	MapProperty("VANADATE TEMP", "Vanadate temperature (°C)", true, MM::Float);
	MapProperty("LBO TEMP", "LBO temperature (°C)", true, MM::Float);
	MapProperty("ETALON TEMP", "Etalon temperature (°C)", true, MM::Float);
	MapProperty("DIODE1 SET TEMP", "Diode #1 set temperature (°C)", true, MM::Float);
	MapProperty("DIODE2 SET TEMP", "Diode #2 set temperature (°C)", true, MM::Float);
	MapProperty("VANADATE SET TEMP", "Vanadate set temperature (°C)", true, MM::Float);
	MapProperty("LBO SET TEMP", "LBO set temperature (°C)", true, MM::Float);
	MapProperty("ETALON SET TEMP", "Etalon set temperature (°C)", true, MM::Float);
	MapProperty("DIODE1 TEMP DRIVE", "Diode #1 temperature servo drive setting", true);
	MapProperty("DIODE2 TEMP DRIVE", "Diode #2 temperature servo drive setting", true);
	MapProperty("VANADATE TEMP DRIVE", "Vanadate temperature servo drive setting", true);
	MapProperty("LBO TEMP DRIVE", "LBO temperature servo drive setting", true);
	MapProperty("ETALON TEMP DRIVE", "Etalon temperature servo drive setting", true);
	MapProperty("DIODE1 HEATSINK TEMP", "Diode #1 heat sink temperature (°C)", true, MM::Float);
	MapProperty("DIODE2 HEATSINK TEMP", "Diode #2 heat sink temperature (°C)", true, MM::Float);
	SetPropertyNames(MapProperty("LIGHT REG STATUS", "Light loop status", true), olsf);
	SetPropertyNames(MapProperty("DIODE1 SERVO STATUS", "Diode #1 temperature servo status", true), olsf);
	SetPropertyNames(MapProperty("DIODE2 SERVO STATUS", "Diode #2 temperature servo status", true), olsf);
	SetPropertyNames(MapProperty("VANADATE SERVO STATUS", "Vanadate temperature servo status", true), olsf);
	SetPropertyNames(MapProperty("LBO SERVO STATUS", "LBO temperature servo status", true), olsf);
	SetPropertyNames(MapProperty("ETALON SERVO STATUS", "Etalon temperature servo status", true), olsf);
	MapProperty("DIODE1 HOURS", "Diode #1 operating time (h)", true, MM::Float);
	MapProperty("DIODE2 HOURS", "Diode #2 operating time (h)", true, MM::Float);
	MapProperty("HEAD HOURS", "Head operating time (h)", true, MM::Float);
	MapProperty("DIODE1 VOLTAGE", "Diode #1 voltage (V)", true, MM::Float);
	MapProperty("DIODE2 VOLTAGE", "Diode #2 voltage (V)", true, MM::Float);
	MapProperty("SOFTWARE", "Power supply software version", true);
	MapProperty("BAT VOLTS", "Battery voltage (V)", true, MM::Float);
	SetPropertyNames(MapProperty("AUTOMODELOCK", "Automodelock routing status", true), disEn);
	MapProperty("PZT CONTROL STATE", "PZT control state", true);

	MapProperty("PZTXCM", "Last power map result for cavity X PZT position (% of available range)", true, MM::Float);
	MapProperty("PZTXCP", "Current cavity X PZT position (% of available range)", true, MM::Float);
	MapProperty("PZTXPM", "Last power map result for pump X PZT position (% of available range)", true, MM::Float);
	MapProperty("PZTXPP", "Current pump X PZT position (% of available range)", true, MM::Float);
	MapProperty("PZTYCM", "Last power map result for cavity Y PZT position (% of available range)", true, MM::Float);
	MapProperty("PZTYCP", "Current cavity Y PZT position (% of available range)", true, MM::Float);
	MapProperty("PZTYPM", "Last power map result for pump Y PZT position (% of available range)", true, MM::Float);
	MapProperty("PZTYPP", "Current pump Y PZT position (% of available range)", true, MM::Float);
	MapProperty("RH", "Relative humidity (%)", true, MM::Float);
	MapProperty("SN", "Serial number", true);
	MapProperty("ST", "Operating status", true);

	CreateProperty("Active faults", "No faults", MM::String, false, new CPropertyActionEx(this, &CoherentChameleon::OnFaults, 0));
	CreateProperty("Fault history", "No faults", MM::String, false, new CPropertyActionEx(this, &CoherentChameleon::OnFaults, 1));
	
	initialized_ = true;
ERRH_END
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
		if (answer.substr(0, 11) == "RANGE ERROR")
			throw error_code(CONTROLLER_ERROR, "Parameter out of range: " + answer.substr(11));
		if (answer.substr(0, 13) == "Command Error")
			throw error_code(CONTROLLER_ERROR, "Illegal instruction: " + answer.substr(13));
		if (answer.substr(0, 11) == "Query Error")
			throw error_code(CONTROLLER_ERROR, "Illegal instruction: " + answer.substr(11));
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

	std::istringstream iss(QueryParameter(history ? "FH" : "F"));

	std::string fault;

	while (std::getline(iss, fault, '&'))
		faults.push_back(fault_codes.at(stoi(fault)));

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

int CoherentChameleon::Shutdown()
{
	if (initialized_)
		initialized_ = false;
	
	return DEVICE_OK;
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
	SetParameter("SHUTTER", open ? "1" : "0");
ERRH_END
}

int CoherentChameleon::GetOpen(bool& open)
{
ERRH_START
	return QueryParameter("SHUTTER") == "1";
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
