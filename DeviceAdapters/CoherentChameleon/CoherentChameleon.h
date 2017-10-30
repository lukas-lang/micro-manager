#pragma once

#include <string>
#include <vector>
#include <map>

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/DeviceUtils.h"

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_PORT_CHANGE_FORBIDDEN    10004
#define ERR_DEVICE_NOT_FOUND         10005
#define CONTROLLER_ERROR             20000

#include  "error_code.h"

#define POWERCONVERSION              1000 //convert the power into mW from the W it wants the commands in

class CoherentChameleon : public CShutterBase<CoherentChameleon>
{

public:
	CoherentChameleon();
	~CoherentChameleon();

	// MMDevice API
	// ------------
	int Initialize();
	int Shutdown();

	void GetName(char* pszName) const;
	bool Busy();

	// Shutter API
	int SetOpen(bool open = true);
	int GetOpen(bool& open);
	int Fire(double deltaT);

	// action interface
	// ----------------
	int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPowerSetpoint(MM::PropertyBase* pProp, MM::ActionType eAct, long index);
	int OnPowerReadback(MM::PropertyBase* pProp, MM::ActionType eAct, long index);
	int OnState(MM::PropertyBase* pProp, MM::ActionType eAct);

	// some important read-only properties
	int OnHeadID(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnHeadUsageHours(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnMinimumLaserPower(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnMaximumLaserPower(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnWaveLength(MM::PropertyBase* pProp, MM::ActionType eAct/*, long*/);

	std::string SendCommand(std::string cmd, bool checkError = true) throw (error_code);
	std::string QueryParameter(std::string param, bool checkError = true) throw (error_code);
	std::string SetParameter(std::string param, std::string value, bool checkError = true) throw (error_code);

	long MapIntProperty(std::string token, std::string description, int lower = -1, int upper = -1);
	long MapProperty(std::string token, std::string description, bool readOnly = false, MM::PropertyType propType = MM::String);
	long MapTriggerProperty(std::string token, std::string description, std::string actionName = "Start");
	void SetNamedProperty(long id, std::vector<std::string> names);
	int OnProperty(MM::PropertyBase* pProp, MM::ActionType eAct, long data);
	int OnTrigger(MM::PropertyBase* pProp, MM::ActionType eAct, long data);

	std::vector<std::string> GetFaults(bool history);
	int OnFaults(MM::PropertyBase* pProp, MM::ActionType eAct, long history);

	void initLimits();

	double minlp_;
	double maxlp_;

   const std::string currentIOBuffer() { return buf_string_; }
  
private:
   bool initialized_;
   long state_;
   std::string name_;
   int error_;
   MM::MMTime changedTime_;

   typedef std::pair<std::string, std::string> prop_data;

   std::vector<prop_data> properties_;
   std::map<long, std::vector<std::string>> valueNames_;

   // todo move these to DevImpl for better data hiding
   const std::string queryToken_;
   const std::string powerSetpointToken_;
   const std::string powerReadbackToken_;
   const std::string CDRHToken_;  // if this is on, laser delays 5 SEC before turning on
   const std::string CWToken_;
   const std::string laserOnToken_;
   const std::string TECServoToken_;
   const std::string headSerialNoToken_;
   const std::string headUsageHoursToken_;
   const std::string wavelengthToken_;
   const std::string externalPowerControlToken_;
   const std::string maxPowerToken_;
   const std::string minPowerToken_;

   std::string port_;

   std::string buf_string_;


   void SetPowerSetpoint(double powerSetpoint__, double& achievedSetpoint__);
   void GetPowerSetpoint(double& powerSetpoint__);
   void GetPowerReadback(double& value__);

   void GeneratePowerProperties();
   void GeneratePropertyState();

   void GenerateReadOnlyIDProperties();

   void SetState(long state);
   void GetState(long &state);

   void GetExternalLaserPowerControl(int& control__);
   void SetExternalLaserPowerControl(int control__);

   // todo -- can move these to the implementation
   void Send(std::string cmd);
   int ReceiveOneLine();
   void Purge();
   int HandleErrors();
};
