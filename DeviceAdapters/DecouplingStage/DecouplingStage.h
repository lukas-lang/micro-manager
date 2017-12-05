#pragma once

#include <string>

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"

#include "DeviceUtil.h"
#include "errorCode.h"

extern const char* stageName;

#define MATRIX_NOT_INVERTIBLE 10001
#define STAGE_NOT_SET         10002

class DecouplingController;

class DecouplingStage : public DeviceUtil<CStageBase<DecouplingStage>, DecouplingStage>
{
public:
	DecouplingStage(std::string axisIndex);
	~DecouplingStage();

	// Device API
	int Initialize();
	int Shutdown();

	void GetName(char* pszName) const;
	bool Busy();

	// Stage API
	int SetPositionUm(double pos);
	int SetRelativePositionUm(double pos);
	int GetPositionUm(double& pos);
	int SetOrigin();
	int SetPositionSteps(long steps);
	int GetPositionSteps(long & steps);
	int GetLimits(double& min, double& max);

	// Property
	int IsStageSequenceable(bool& isSequenceable) const;
	bool IsContinuousFocusDrive() const;

	DecouplingController* getController();

	typedef boost::numeric::ublas::matrix<double> Mat;
	typedef boost::numeric::ublas::vector<double> Vec;

private:
	bool initialized_;
	unsigned axisIndex_;
	DecouplingController* controller_;

	double conversionFactor_;
};