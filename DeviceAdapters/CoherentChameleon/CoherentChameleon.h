#pragma once

#include <string>
#include <vector>

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/DeviceUtils.h"

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_PORT_CHANGE_FORBIDDEN    10004
#define ERR_DEVICE_NOT_FOUND         10005

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

	std::string queryLaser(std::string thisToken);

	std::string setLaser(std::string thisToken, std::string thisValue);


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
