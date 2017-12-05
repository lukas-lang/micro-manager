#include "Util.h"

#include <algorithm>
#include <limits>

#include "DecouplingStage.h"
#include "DecouplingController.h"

extern const char* stageName = "Decoupling stage";

DecouplingStage::DecouplingStage(std::string axisIndex) :
	axisIndex_(from_string<unsigned>(axisIndex)),
	initialized_(false),
	conversionFactor_(1000)
{
	InitializeDefaultErrorMessages();
	SetErrorText(MATRIX_NOT_INVERTIBLE, "Transformation matrix not invertible. Can't determine position");
	SetErrorText(STAGE_NOT_SET, "Invalid/No stage selected");

	CreateProperty("Axis index", axisIndex.c_str(), MM::Integer, true);
	MapProperty(vref(conversionFactor_), "Step to um conversion factor", false, MM::Float);
}

DecouplingStage::~DecouplingStage()
{
}

int DecouplingStage::Initialize()
{
ERRH_START
	if (initialized_)
		return DEVICE_OK;
	
	controller_ = getController();

	initialized_ = true;
ERRH_END
}

int DecouplingStage::Shutdown()
{
ERRH_START
	if (!initialized_)
		return DEVICE_OK;

	initialized_ = false;
ERRH_END
}

DecouplingController* DecouplingStage::getController()
{
	MM::Hub* hub = GetParentHub();
	if (!hub)
		throw errorCode(1);

	DecouplingController* controller = dynamic_cast<DecouplingController*>(hub);
	if (!controller)
		throw errorCode(1);

	return controller;
}

void DecouplingStage::GetName(char * pszName) const
{
	std::ostringstream os;
	os << stageName << " (Axis " << axisIndex_ << ")";

	CDeviceUtils::CopyLimitedString(pszName, os.str().c_str());
}

/*
Read motion status. 0: motion is NOT done, 1: motion is done.
*/
bool DecouplingStage::Busy()
{
ERRH_START
	for(int i = 0; i < controller_->stageCount_; ++i)
	{
		MM::Stage* stage = controller_->stages_[i];

		if (stage && stage->Busy())
			return true;
	}

	return false;
ERRH_END
}


int DecouplingStage::SetPositionUm(double pos)
{
ERRH_START
	Vec vPositions = controller_->GetPositionsUm();

	vPositions[axisIndex_] = pos;

	controller_->SetPositionsUm(vPositions);
ERRH_END
}


int DecouplingStage::SetRelativePositionUm(double pos)
{
ERRH_START
	Vec vPositions = controller_->GetPositionsUm();

	vPositions[axisIndex_] += pos;

	controller_->SetPositionsUm(vPositions);
ERRH_END
}

int DecouplingStage::GetPositionUm(double& pos)
{
ERRH_START
	pos = controller_->GetPositionsUm()[axisIndex_];
ERRH_END
}

/*
Set position in default unit of axis (° or mm)
*/
int DecouplingStage::SetPositionSteps(long steps)
{
	return SetPositionUm(steps / conversionFactor_);
}

/*
Get position in default unit of axis (° or mm)
*/
int DecouplingStage::GetPositionSteps(long& steps)
{
ERRH_START
	steps = controller_->GetPositionsUm()[axisIndex_] * conversionFactor_;
ERRH_END
}

int DecouplingStage::SetOrigin()
{
	return DEVICE_OK;
}

int DecouplingStage::GetLimits(double& min, double& max)
{
ERRH_START
	min = (std::numeric_limits<double>::min)(); // parentheses since VS defines min/max macros
	max = (std::numeric_limits<double>::max)();
ERRH_END
}

int DecouplingStage::IsStageSequenceable(bool & isSequenceable) const
{
	isSequenceable = false;
	return DEVICE_OK;
}

bool DecouplingStage::IsContinuousFocusDrive() const
{
	return false;
}