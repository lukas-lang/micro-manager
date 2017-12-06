#include "Util.h"
#include "DecouplingController.h"

const char* controllerName = "Decoupling stage controller";

DecouplingController::DecouplingController() :
initialized_(false),
stageCount_(0)
{
	InitializeDefaultErrorMessages();
	SetErrorText(INVALID_INPUT, "Invalid input specified");
	SetErrorText(STAGE_NOT_SET, "Invalid/No stage selected");

	CreateProperty(MM::g_Keyword_Name, controllerName, MM::String, true);

	CreateProperty(MM::g_Keyword_Description, "Virtual stage controller to (de)couple stage movements", MM::String, true);

	MapProperty(vref(stageCount_), "Coupled stage count", false, MM::Integer, true);
}

DecouplingController::~DecouplingController()
{
	Shutdown();
}

int DecouplingController::Initialize()
{
ERRH_START
	if (initialized_)
		return DEVICE_OK;

	stages_ = std::vector<MM::Stage*>(stageCount_, nullptr);

	couplingMatrix_ = boost::numeric::ublas::identity_matrix<double>(stageCount_ + 1);
	invCouplingMatrix_ = couplingMatrix_;
	invertible_ = true;

	struct matLink : PropertyAccessor
	{
		unsigned idx;
		bool row;

		matLink(unsigned idx, bool row = true) : idx(idx), row(row) {}

		std::string QueryParameter(DecouplingController* inst)
		{
			std::ostringstream os;

			for (int i = 0; i < inst->stageCount_; ++i)
				os << (i > 0 ? "; " : "") << (row ? inst->couplingMatrix_(idx, i) : inst->couplingMatrix_(i, idx));

			return os.str();
		}

		void SetParameter(DecouplingController*inst , std::string val)
		{
			std::istringstream is(val);

			for (int i = 0; i < inst->stageCount_; ++i)
			{
				std::string num;
				std::getline(is, num, ';');

				if (!is)
					throw errorCode(INVALID_INPUT);

				(row ? inst->couplingMatrix_(idx, i) : inst->couplingMatrix_(i, idx)) = from_string<double>(num);
			}

			inst->updateInverse();
		}
	};

	struct colSetter : PropertyAccessor
	{
		unsigned idx;
		bool rel;

		colSetter(unsigned idx, bool rel = true) : idx(idx), rel(rel) {}

		void SetParameter(DecouplingController* inst, std::string val)
		{
			if (val == "1")
			{
				for (int i = 0; i < inst->stageCount_; ++i)
				{
					double pos;

					if (!inst->stages_[i])
						throw errorCode(STAGE_NOT_SET);

					errorCode::ThrowErr(inst->stages_[i]->GetPositionUm(pos), inst->stages_[i]);

					inst->couplingMatrix_(i, idx) = pos - (rel ? inst->couplingMatrix_(i, inst->stageCount_) : 0);
				}

				inst->updateInverse();
			}
		}
	};



	availableStages_.push_back("-");

	char stageLabel[MM::MaxStrLength];
	unsigned int index = 0;
	for (int i = 0;; ++i)
	{
		GetLoadedDeviceOfType(MM::StageDevice, stageLabel, i);
		if (strlen(stageLabel) > 0)
		{
			DecouplingStage* stage = dynamic_cast<DecouplingStage*>(GetDevice(stageLabel));

			bool isOwn = false;

			if (stage)
				try
				{
					isOwn = stage->getController() == this;
				}
				catch (errorCode)
				{}

			if (!isOwn)
				availableStages_.push_back(stageLabel);
		}
		else break;
	}

	struct stageSelector : PropertyAccessor
	{
		unsigned idx;

		stageSelector(unsigned idx) : idx(idx) {}

		std::string QueryParameter(DecouplingController* inst)
		{
			if (!inst->stages_[idx])
				return "-";

			char name[MM::MaxStrLength];
			inst->stages_[idx]->GetLabel(name);
			return name;
		}

		void SetParameter(DecouplingController* inst, std::string val)
		{
			if (val == "-")
			{
				inst->stages_[idx] = nullptr;
				return;
			}

			MM::Stage* stage = dynamic_cast<MM::Stage*>(inst->GetDevice(val.c_str()));

			if (!stage)
				throw errorCode(INVALID_STAGE_NAME);

			inst->stages_[idx] = stage;
		}
	};

	MapProperty(new matLink(stageCount_, false), "Axis origin");
	MapTriggerProperty(new colSetter(stageCount_, false), "Axis origin: Assign current position", "Assign");

	for (int i = 0; i < stageCount_; ++i)
	{
		std::string propName = "Coupled stage " + to_string(i);
		int propID = MapProperty(new stageSelector(i), propName);
		SetAllowedValues(propName.c_str(), availableStages_);
		MapProperty(new matLink(i), "Coupling matrix (row " + to_string(i) + ")");
		MapTriggerProperty(new colSetter(i), "Assign current positions (rel. to origin) to axis " + to_string(i) + " direction", "Assign");
	}

	initialized_ = true;
ERRH_END
}

int DecouplingController::Shutdown()
{
	if (!initialized_)
		return DEVICE_OK;

	initialized_ = false;
	return DEVICE_OK;
}

void DecouplingController::GetName(char * pszName) const
{
	CDeviceUtils::CopyLimitedString(pszName, stageName);
}

bool DecouplingController::Busy()
{
	return false;
}

int DecouplingController::DetectInstalledDevices()
{
	for (int i = 0; i < stageCount_; ++i)
	{
		AddInstalledDevice(new DecouplingStage(to_string(i)));
	}

	return DEVICE_OK;
}

void DecouplingController::updateInverse()
{
	invertible_ = invertMatrix(couplingMatrix_, invCouplingMatrix_);
}

DecouplingController::Vec DecouplingController::GetPositionsUm()
{
	if (!invertible_)
		throw errorCode(MATRIX_NOT_INVERTIBLE);

	Mat& inv = invCouplingMatrix_;

	std::vector<bool> zeroCols(inv.size1(), true);

	for (int i = 0; i < inv.size1(); ++i)
		for (int j = 0; j < inv.size2(); ++j)
			if (inv(i, j) != 0)
				zeroCols[j] = false;

	Vec positions(inv.size1(), 0);
	positions[inv.size1() - 1] = 1; //translation by origin

	for (int i = 0; i < inv.size1() - 1; ++i)
	{
		if (zeroCols[i])
			continue;

		MM::Stage* stage = stages_[i];

		if (!stage)
			return Vec(inv.size1(), 0); //don't throw, as the position is queried too early

		double pos;
		errorCode::ThrowErr(stage->GetPositionUm(pos), stage);

		positions[i] = pos;
	}

	Vec vPositions = prod(inv, positions);
	return vPositions;
}

void DecouplingController::SetPositionsUm(Vec positions)
{
	Vec pPositions = prod(couplingMatrix_, positions);
	
	for (int i = 0; i < stageCount_; ++i)
	{
		MM::Stage* stage = stages_[i];

		if (!stage)
			throw errorCode(STAGE_NOT_SET);

		errorCode::ThrowErr(stage->SetPositionUm(pPositions[i]), stage);
	}
}

//Action handler