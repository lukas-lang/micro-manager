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

// Controller
const char* g_Keyword_PowerSetpoint = "PowerSetpoint";
const char* g_Keyword_PowerReadback = "PowerReadback";


///////////////////////////////////////////////////////////////////////////////
// Controller implementation
// ~~~~~~~~~~~~~~~~~~~~

CoherentChameleon::CoherentChameleon() :
	initialized_(false),
	state_(0),
	name_("Coherent chameleon laser"),
	error_(0),
	changedTime_(0.0),
	queryToken_("?"),
	powerSetpointToken_("SOUR1:POW:LEV:IMM:AMPL"),
	powerReadbackToken_("SOUR1:POW:LEV:IMM:AMPL"),
	CDRHToken_("CDRH"),  // if this is on, laser delays 5 SEC before turning on
	CWToken_("CW"),
	laserOnToken_("SOUR1:AM:STATE"),
	TECServoToken_("T"),
	headSerialNoToken_("SYST:INF:SNUM"),
	headUsageHoursToken_("SYST1:DIOD:HOUR"),
	wavelengthToken_("SYST1:INF:WAV"),
	externalPowerControlToken_("SOUR1:POW:LEV:IMM:AMPL"),
	maxPowerToken_("SOUR1:POW:LIM:HIGH"),
	minPowerToken_("SOUR1:POW:LIM:LOW")
{
	InitializeDefaultErrorMessages();
	SetErrorText(ERR_DEVICE_NOT_FOUND, "No answer received.  Is the Coherent Cube connected to this serial port?");
	// create pre-initialization properties
	// ------------------------------------

	// Name
	CreateProperty(MM::g_Keyword_Name, name_.c_str(), MM::String, true);

	// Description
	CreateProperty(MM::g_Keyword_Description, "CoherentObis Laser", MM::String, true);

	// Port
	CPropertyAction* pAct = new CPropertyAction(this, &CoherentChameleon::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

	EnableDelay(); // signals that the delay setting will be used
	UpdateStatus();
}

CoherentChameleon::~CoherentChameleon()
{
}

bool CoherentChameleon::Busy()
{
	MM::MMTime interval = GetCurrentMMTime() - changedTime_;
	MM::MMTime delay(GetDelayMs()*1000.0);
	if (interval < delay)
		return true;
	else
		return false;
}

void CoherentChameleon::GetName(char* name) const
{
	assert(name_.length() < CDeviceUtils::GetMaxStringLength());
	CDeviceUtils::CopyLimitedString(name, name_.c_str());
}


int CoherentChameleon::Initialize()
{
	ERRH_START
		GeneratePowerProperties();
	GeneratePropertyState();
	GenerateReadOnlyIDProperties();
	std::stringstream msg;

	SetParameter("ECHO", "0", false);
	SetParameter("PROMPT", "0", false);

	std::vector<std::string>
		offOn = vector_of("Off")("On"),
		disEn = vector_of("Disabled")("Enabled"),
		olsf = vector_of("Open")("Locked")("Seeking")("Fault");
	
	SetNamedProperty(MapProperty("LBO HEATER", "Enable LBO heater"), offOn);
	SetNamedProperty(MapProperty("SEARCH MODELOCK", "Search for modelock"), disEn);
	SetNamedProperty(MapProperty("SHUTTER", "Shutter"), vector_of("Closed")("Open"));

	MapProperty("TUNING LIMIT MIN", "Minimum wavelength", true, MM::Float);
	MapProperty("TUNING LIMIT MAX", "Maximum wavelength", true, MM::Float);
	MapIntProperty("WAVELENGTH", "Wavelength", stoi(QueryParameter("TUNING LIMIT MIT")), stoi(QueryParameter("TUNING LIMIT MAX")));

	MapTriggerProperty("FLASH", "Flash Verdi laser output below threshold to recenter mode");
	MapTriggerProperty("LBO OPTIMIZE", "Begin LBO optimization routine");
	MapTriggerProperty("HOME STEPPER", "Home the tuning motor (takes 3-30s)");
	MapTriggerProperty("RECOVERY", "Initiate recovery (takes up to 2min)");

	SetNamedProperty(MapIntProperty("ALIGN", "Alignment mode"), disEn);
	MapIntProperty("ALIGNP", "Alignment mode power (mW)", true, MM::Float);
	MapIntProperty("ALIGNW", "Alignment mode wavelength (nm)", true, MM::Float);

	//TODO: Make not readonly
	SetNamedProperty(MapProperty("LASER", "Laser status", true), vector_of("Off")("On")("Off due to fault"));

	SetNamedProperty(MapProperty("KEYSWITCH", "Keyswitch status", true), offOn);

	MapProperty("UF POWER", "Actual UF (Chameleon) power (mW)", true, MM::Float);
	MapProperty("CAVITY PEAK HOLD", "Cavity peak hold status", true);
	SetNamedProperty(MapProperty("CAVITY PZT MODE", "Cavity PZT mode", true), vector_of("Auto")("Manual"));
	MapProperty("CAVITY PZT X", "Cavity PZT X (Rd) voltage (V)", true, MM::Float);
	MapProperty("CAVITY PZT Y", "Cavity PZT Y (Rd) voltage (V)", true, MM::Float);
	SetNamedProperty(MapProperty("PUMP PEAK HOLD", "Pump peak hold status", true), offOn);
	SetNamedProperty(MapProperty("PUMP PZT MODE", "Pump PZT mode", true), vector_of("Auto")("Manual"));
	MapProperty("PUMP PZT X", "Pump PZT X (Rd) voltage (V)", true, MM::Float);
	MapProperty("PUMP PZT Y", "Pump PZT Y (Rd) voltage (V)", true, MM::Float);
	SetNamedProperty(MapProperty("POWER TRACK", "PowerTrack state", true), offOn);
	SetNamedProperty(MapProperty("MODELOCKED", "Chameleon Ultra state", true), vector_of("Off (Standby)")("Modelocked")("CW"));
	MapProperty("PUMP SETTING", "Pump power setpoint (fraction of QS to CW pump band)", true, MM::Float);
	SetNamedProperty(MapProperty("TUNING STATUS", "Tuning state", true), vector_of("Ready")("Tuning in progress")("Searching for modelock")("Recovery operation in progress"));
	SetNamedProperty(MapProperty("SEARCH MODELOCK", "Modelock search status", true), disEn);
	SetNamedProperty(MapProperty("HOMED", "Tuning motor homing status", true), vector_of("Has not been homed")("Has been homed"));
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
	SetNamedProperty(MapProperty("LIGHT REG STATUS", "Light loop status", true), olsf);
	SetNamedProperty(MapProperty("DIODE1 SERVO STATUS", "Diode #1 temperature servo status", true), olsf);
	SetNamedProperty(MapProperty("DIODE2 SERVO STATUS", "Diode #2 temperature servo status", true), olsf);
	SetNamedProperty(MapProperty("VANADATE SERVO STATUS", "Vanadate temperature servo status", true), olsf);
	SetNamedProperty(MapProperty("LBO SERVO STATUS", "LBO temperature servo status", true), olsf);
	SetNamedProperty(MapProperty("ETALON SERVO STATUS", "Etalon temperature servo status", true), olsf);
	MapProperty("DIODE1 HOURS", "Diode #1 operating time (h)", true, MM::Float);
	MapProperty("DIODE2 HOURS", "Diode #2 operating time (h)", true, MM::Float);
	MapProperty("HEAD HOURS", "Head operating time (h)", true, MM::Float);
	MapProperty("DIODE1 VOLTAGE", "Diode #1 voltage (V)", true, MM::Float);
	MapProperty("DIODE2 VOLTAGE", "Diode #2 voltage (V)", true, MM::Float);
	MapProperty("SOFTWARE", "Power supply software version", true);
	MapProperty("BAT VOLTS", "Battery voltage (V)", true, MM::Float);
	SetNamedProperty(MapProperty("AUTOMODELOCK", "Automodelock routing status", true), disEn);
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
	//TODO: Fault and Fault history

	//MapReadOnlyProperty();

	//Initialize laser??
	SendCommand(msg.str());

	// query laser for power limits
	this->initLimits();

	double llimit = minlp_;
	double ulimit = maxlp_;

	// set the limits as interrogated from the laser controller.
	SetPropertyLimits(g_Keyword_PowerSetpoint, llimit, ulimit);  // milliWatts

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

std::string CoherentChameleon::QueryParameter(std::string param, bool checkError) throw (error_code)
{
	return SendCommand("?" + param, checkError);
}
std::string CoherentChameleon::SetParameter(std::string param, std::string value, bool checkError) throw (error_code)
{
	SendCommand(param + "=" + value, checkError);

	return QueryParameter(param, checkError);
}

long CoherentChameleon::MapIntProperty(std::string token, std::string description, int lower, int upper)
{
	long id = MapProperty(token, description);
	
	if (lower != upper)
		SetPropertyLimits(description.c_str(), lower, upper);

	return id;
}

long CoherentChameleon::MapProperty(std::string token, std::string description, bool readOnly, MM::PropertyType propType)
{
	std::string val = QueryParameter(token);

	properties_.push_back(prop_data(token, description));
	long id = properties_.size() - 1;
	CreateProperty(description.c_str(), val.c_str(), propType, readOnly, new CPropertyActionEx(this, &CoherentChameleon::OnProperty, id));

	return id;
}

long CoherentChameleon::MapTriggerProperty(std::string token, std::string description, std::string actionName)
{
	properties_.push_back(prop_data(token, description));
	long id = properties_.size() - 1;
	CreateProperty(description.c_str(), "-", MM::String, false, new CPropertyActionEx(this, &CoherentChameleon::OnTrigger, id));
	SetAllowedValues(description.c_str(), vector_of("-")(actionName));

	return id;
}

void CoherentChameleon::SetNamedProperty(long id, std::vector<std::string> names)
{
	valueNames_[id] = names;
	SetAllowedValues(properties_[id].second.c_str(), names);
}

int CoherentChameleon::OnProperty(MM::PropertyBase* pProp, MM::ActionType eAct, long data)
{
ERRH_START
	std::string val;

	switch (eAct)
	{
	case MM::BeforeGet:
		val = QueryParameter(properties_[data].first);

		if (valueNames_.find(data) != valueNames_.end())
		{
			val = valueNames_[data][atoi(val.c_str())];
		}
		pProp->Set(val.c_str());
		break;
	case MM::AfterSet:
		pProp->Get(val);

		if (valueNames_.find(data) != valueNames_.end())
		{
			std::vector<std::string> names = valueNames_[data];
			val = to_string(find(names.begin(), names.end(), val) - names.begin());
		}

		SetParameter(properties_[data].first, val);
		break;
	}
ERRH_END
}

int CoherentChameleon::OnTrigger(MM::PropertyBase * pProp, MM::ActionType eAct, long data)
{
ERRH_START
	std::string val;

	switch (eAct)
	{
	case MM::BeforeGet:

		pProp->Set("-");
		break;
	case MM::AfterSet:
		pProp->Get(val);

		if (val != "-")
			SetParameter(properties_[data].first, "1");
		break;
	}
ERRH_END
}

void CoherentChameleon::initLimits()
{
	std::string val = QueryParameter(maxPowerToken_);
	minlp_ = (atof(val.c_str()) * 1000);
	val = QueryParameter(maxPowerToken_);
	maxlp_ = (atof(val.c_str()) * 1000);
}
/////////////////////////////////////////////
// Property Generators
/////////////////////////////////////////////

void CoherentChameleon::GeneratePropertyState()
{
	CPropertyAction* pAct = new CPropertyAction(this, &CoherentChameleon::OnState);
	CreateProperty(MM::g_Keyword_State, "0", MM::Integer, false, pAct);
	AddAllowedValue(MM::g_Keyword_State, "0");
	AddAllowedValue(MM::g_Keyword_State, "1");
}


void CoherentChameleon::GeneratePowerProperties()
{
	std::string powerName;

	// Power Setpoint
	CPropertyActionEx* pActEx = new CPropertyActionEx(this, &CoherentChameleon::OnPowerSetpoint, 0);
	powerName = g_Keyword_PowerSetpoint;
	CreateProperty(powerName.c_str(), "0", MM::Float, false, pActEx);

	// Power Setpoint
	pActEx = new CPropertyActionEx(this, &CoherentChameleon::OnPowerReadback, 0);
	powerName = g_Keyword_PowerReadback;
	CreateProperty(powerName.c_str(), "0", MM::Float, true, pActEx);
}


void CoherentChameleon::GenerateReadOnlyIDProperties()
{
	CPropertyAction* pAct;
	pAct = new CPropertyAction(this, &CoherentChameleon::OnHeadID);
	CreateProperty("HeadID", "", MM::String, true, pAct);

	pAct = new CPropertyAction(this, &CoherentChameleon::OnHeadUsageHours);
	CreateProperty("Head Usage Hours", "", MM::String, true, pAct);

	pAct = new CPropertyAction(this, &CoherentChameleon::OnMinimumLaserPower);
	CreateProperty("Minimum Laser Power", "", MM::Float, true, pAct);

	pAct = new CPropertyAction(this, &CoherentChameleon::OnMaximumLaserPower);
	CreateProperty("Maximum Laser Power", "", MM::Float, true, pAct);

	pAct = new CPropertyAction(this, &CoherentChameleon::OnWaveLength);
	CreateProperty("Wavelength", "", MM::Float, true, pAct);
}

int CoherentChameleon::Shutdown()
{
	if (initialized_)
	{
		initialized_ = false;
	}
	return HandleErrors();
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

	return HandleErrors();
}


int CoherentChameleon::OnPowerReadback(MM::PropertyBase* pProp, MM::ActionType eAct, long /*index*/)
{

	double powerReadback;
	if (eAct == MM::BeforeGet)
	{
		GetPowerReadback(powerReadback);
		pProp->Set(powerReadback);
	}
	else if (eAct == MM::AfterSet)
	{
		// never do anything!!
	}
	return HandleErrors();
}

int CoherentChameleon::OnPowerSetpoint(MM::PropertyBase* pProp, MM::ActionType eAct, long  /*index*/)
{

	double powerSetpoint;
	if (eAct == MM::BeforeGet)
	{
		GetPowerSetpoint(powerSetpoint);
		pProp->Set(powerSetpoint);
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(powerSetpoint);
		double achievedSetpoint;
		SetPowerSetpoint(powerSetpoint, achievedSetpoint);
		if (0. != powerSetpoint)
		{
			double fractionError = fabs(achievedSetpoint - powerSetpoint) / powerSetpoint;
			if ((0.05 < fractionError) && (fractionError < 0.10))
				pProp->Set(achievedSetpoint);
		}
	}
	return HandleErrors();
}


int CoherentChameleon::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		GetState(state_);
		pProp->Set(state_);
	}
	else if (eAct == MM::AfterSet)
	{
		long requestedState;
		pProp->Get(requestedState);
		SetState(requestedState);
		if (state_ != requestedState)
		{
			// error
		}
	}

	return HandleErrors();
}


int CoherentChameleon::OnHeadID(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set((this->QueryParameter(headSerialNoToken_)).c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		// never do anything!!
	}
	return HandleErrors();
}


int CoherentChameleon::OnHeadUsageHours(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		std::string svalue = this->QueryParameter(headUsageHoursToken_);
		double dvalue = atof(svalue.c_str());
		pProp->Set(dvalue);
	}
	else if (eAct == MM::AfterSet)
	{
		// never do anything!!
	}
	return HandleErrors();
}


int CoherentChameleon::OnMinimumLaserPower(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(atof((this->QueryParameter(minPowerToken_)).c_str()));
	}
	else if (eAct == MM::AfterSet)
	{
		// never do anything!!
	}
	return HandleErrors();
}

int CoherentChameleon::OnMaximumLaserPower(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(atof((this->QueryParameter(maxPowerToken_)).c_str()));
	}
	else if (eAct == MM::AfterSet)
	{
		// never do anything!!
	}
	return HandleErrors();
}


int CoherentChameleon::OnWaveLength(MM::PropertyBase* pProp, MM::ActionType eAct /* , long */)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(atof((this->QueryParameter(wavelengthToken_)).c_str()));
	}
	else if (eAct == MM::AfterSet)
	{
		// never do anything!!
	}
	return HandleErrors();
}


void CoherentChameleon::GetPowerReadback(double& value)
{
	std::string ans = this->QueryParameter(powerReadbackToken_);
	value = POWERCONVERSION*atof(ans.c_str());
}

void CoherentChameleon::SetPowerSetpoint(double requestedPowerSetpoint, double& achievedPowerSetpoint)
{
	std::string result;
	std::ostringstream setpointString;
	// number like 100.00
	setpointString << std::setprecision(6) << requestedPowerSetpoint / POWERCONVERSION;
	result = this->SetParameter(powerSetpointToken_, setpointString.str());
	//compare quantized setpoint to requested setpoint
	// the difference can be rather large

	achievedPowerSetpoint = POWERCONVERSION*atof(result.c_str());

	// if device echos a setpoint more the 10% of full scale from requested setpoint, log a warning message
	if (maxlp_ / 10. < fabs(achievedPowerSetpoint - POWERCONVERSION*requestedPowerSetpoint))
	{
		std::ostringstream messs;
		messs << "requested setpoint: " << requestedPowerSetpoint << " but echo setpoint is: " << achievedPowerSetpoint;
		LogMessage(messs.str().c_str());
	}
}

void CoherentChameleon::GetPowerSetpoint(double& value)
{
	std::string ans = this->QueryParameter(powerSetpointToken_);
	value = POWERCONVERSION*atof(ans.c_str());
}

void CoherentChameleon::SetState(long state)
{
	std::ostringstream atoken;
	if (state == 1)
	{
		atoken << "On";
	}
	else
	{
		atoken << "Off";
	}
	this->SetParameter(laserOnToken_, atoken.str());
	// Set timer for the Busy signal
	changedTime_ = GetCurrentMMTime();
}

void CoherentChameleon::GetState(long &value)
{
	std::string ans = this->QueryParameter(laserOnToken_);
	std::transform(ans.begin(), ans.end(), ans.begin(), ::tolower);
	if (ans.find("on") == 0)
	{
		value = 1;
	}
	else if (ans.find("off") == 0)
	{
		value = 0;
	}
	else
	{
		value = 2;
	}
}

void CoherentChameleon::SetExternalLaserPowerControl(int value)
{
	std::ostringstream atoken;
	atoken << value;
	this->SetParameter(externalPowerControlToken_, atoken.str());
}

void CoherentChameleon::GetExternalLaserPowerControl(int& value)
{
	std::string ans = this->QueryParameter(externalPowerControlToken_);
	value = atol(ans.c_str());
}

int CoherentChameleon::HandleErrors()
{
	int lastError = error_;
	error_ = 0;
	return lastError;
}

// Shutter API

int CoherentChameleon::SetOpen(bool open)
{
ERRH_START
	SetParameter("LASER", open ? "1" : "0");
ERRH_END
}

int CoherentChameleon::GetOpen(bool& open)
{
ERRH_START
	return QueryParameter("LASER") == "1";
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
