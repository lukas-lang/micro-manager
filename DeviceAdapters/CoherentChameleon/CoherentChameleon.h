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

extern const char* g_device_name;

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
  
private:
   bool initialized_;

   typedef std::pair<std::string, std::string> prop_data;

   std::vector<prop_data> properties_;
   std::map<long, std::vector<std::string>> valueNames_;
   
   std::string port_;
};
