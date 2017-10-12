#include <algorithm>
#include <math.h>
#include <sstream>
#include <string>

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/ModuleInterface.h"
#include "../../MMDevice/DeviceUtils.h"

#include "CoherentChameleon.h"

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

	setParameter("ECHO", 0, false);
	setParameter("PROMPT", 0, false);


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

std::string CoherentChameleon::queryParameter(std::string param, bool checkError) throw (error_code)
{
	return SendCommand("?" + param, checkError);
}
std::string CoherentChameleon::setParameter(std::string param, std::string value, bool checkError) throw (error_code)
{
	SendCommand(param + "=" + value, checkError);

	return queryParameter(param, checkError);
}

void CoherentChameleon::initLimits()
{
	std::string val = queryParameter(maxPowerToken_);
	minlp_ = (atof(val.c_str()) * 1000);
	val = queryParameter(maxPowerToken_);
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
		pProp->Set((this->queryParameter(headSerialNoToken_)).c_str());
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
		std::string svalue = this->queryParameter(headUsageHoursToken_);
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
		pProp->Set(atof((this->queryParameter(minPowerToken_)).c_str()));
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
		pProp->Set(atof((this->queryParameter(maxPowerToken_)).c_str()));
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
		pProp->Set(atof((this->queryParameter(wavelengthToken_)).c_str()));
	}
	else if (eAct == MM::AfterSet)
	{
		// never do anything!!
	}
	return HandleErrors();
}


void CoherentChameleon::GetPowerReadback(double& value)
{
	std::string ans = this->queryParameter(powerReadbackToken_);
	value = POWERCONVERSION*atof(ans.c_str());
}

void CoherentChameleon::SetPowerSetpoint(double requestedPowerSetpoint, double& achievedPowerSetpoint)
{
	std::string result;
	std::ostringstream setpointString;
	// number like 100.00
	setpointString << std::setprecision(6) << requestedPowerSetpoint / POWERCONVERSION;
	result = this->setParameter(powerSetpointToken_, setpointString.str());
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
	std::string ans = this->queryParameter(powerSetpointToken_);
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
	this->setParameter(laserOnToken_, atoken.str());
	// Set timer for the Busy signal
	changedTime_ = GetCurrentMMTime();
}

void CoherentChameleon::GetState(long &value)
{
	std::string ans = this->queryParameter(laserOnToken_);
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
	this->setParameter(externalPowerControlToken_, atoken.str());
}

void CoherentChameleon::GetExternalLaserPowerControl(int& value)
{
	std::string ans = this->queryParameter(externalPowerControlToken_);
	value = atol(ans.c_str());
}

int CoherentChameleon::HandleErrors()
{
	int lastError = error_;
	error_ = 0;
	return lastError;
}


//********************
// Shutter API
//********************

int CoherentChameleon::SetOpen(bool open)
{
	SetState((long)open);
	return HandleErrors();
}

int CoherentChameleon::GetOpen(bool& open)
{
	long state;
	GetState(state);
	if (state == 1)
		open = true;
	else if (state == 0)
		open = false;
	else
		error_ = DEVICE_UNKNOWN_POSITION;

	return HandleErrors();
}

// ON for deltaT milliseconds
// other implementations of Shutter don't implement this
// is this perhaps because this blocking call is not appropriate
int CoherentChameleon::Fire(double deltaT)
{
	SetOpen(true);
	CDeviceUtils::SleepMs((long)(deltaT + .5));
	SetOpen(false);
	return HandleErrors();
}
