#pragma once

#include <string>
#include <vector>

#include <boost/numeric/ublas/matrix.hpp>

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"

#include "errorCode.h"
#include "DecouplingStage.h"

#define INVALID_STAGE_NAME 10001
#define STAGE_NOT_SET      10002

extern const char* controllerName;

class DecouplingController : public DeviceUtil<HubBase<DecouplingController>, DecouplingController>
{
public:
	DecouplingController();
	virtual ~DecouplingController();

	// Device API
	int Initialize();
	int Shutdown();

	void GetName(char* pszName) const;
	bool Busy();

	//Hub API
	int DetectInstalledDevices();
	
	void updateInverse();

	typedef boost::numeric::ublas::matrix<double> Mat;
	typedef boost::numeric::ublas::vector<double> Vec;

	Vec GetPositionsUm();
	void SetPositionsUm(Vec positions);

private:
	friend DecouplingStage;
	bool initialized_;

	unsigned stageCount_;
	bool normalize_;
	std::vector<MM::Stage*> stages_;
	std::vector<std::string> availableStages_;
	Mat couplingMatrix_;
	bool invertible_;
	Mat invCouplingMatrix_;
};