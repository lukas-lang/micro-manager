#include "FakeCamera.h"

#include "boost/algorithm/string/replace.hpp"

#include "Util.h"

const char* cameraName = "FakeCamera";

FakeCamera::FakeCamera() :
	path_(""),
	roiX_(0),
	roiY_(0),
	initSize_(false)
{
	CreateProperty("Path Mask", "", MM::String, false, new CPropertyAction(this, &FakeCamera::OnPath), true);

	SetErrorText(OUT_OF_RANGE, "Parameters out of range");
}

FakeCamera::~FakeCamera()
{
}

int FakeCamera::Initialize()
{
	initSize_ = false;

	return DEVICE_OK;
}

int FakeCamera::Shutdown()
{
	return 0;
}

void FakeCamera::GetName(char * name) const
{
	CDeviceUtils::CopyLimitedString(name, cameraName);
}

long FakeCamera::GetImageBufferSize() const
{
	initSize();

	return roiWidth_ * roiHeight_;
}

unsigned FakeCamera::GetBitDepth() const
{
	return 8;
}

int FakeCamera::GetBinning() const
{
	return 1;
}

int FakeCamera::SetBinning(int)
{
	return DEVICE_UNSUPPORTED_COMMAND;
}

void FakeCamera::SetExposure(double)
{
}

double FakeCamera::GetExposure() const
{
	return 1;
}

int FakeCamera::SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize)
{
	if (x + xSize > width_ || y + ySize > 0)
	{
		return OUT_OF_RANGE;
	}

	roiX_ = x;
	roiY_ = y;
	roiWidth_ = xSize;
	roiHeight_ = ySize;

	return DEVICE_OK;
}

int FakeCamera::GetROI(unsigned & x, unsigned & y, unsigned & xSize, unsigned & ySize)
{
	initSize();

	x = roiX_;
	y = roiY_;
	xSize = roiWidth_;
	ySize = roiHeight_;

	return DEVICE_OK;
}

int FakeCamera::ClearROI()
{
	initSize();

	SetROI(0, 0, width_, height_);

	return DEVICE_OK;
}

int FakeCamera::IsExposureSequenceable(bool & isSequenceable) const
{
	isSequenceable = false;

	return DEVICE_OK;
}

const unsigned char * FakeCamera::GetImageBuffer()
{
	return curImg_.datastart;
}

unsigned FakeCamera::GetImageWidth() const
{
	initSize();

	return roiWidth_;
}

unsigned FakeCamera::GetImageHeight() const
{
	initSize();

	return roiHeight_;
}

unsigned FakeCamera::GetImageBytesPerPixel() const
{
	return 1;
}

int FakeCamera::SnapImage()
{
ERRH_START
	initSize();

	curImg_ = getImg();

	curImg_.adjustROI(roiY_, roiY_ + roiHeight_, roiX_, roiX_ + roiWidth_);

ERRH_END
}

int FakeCamera::OnPath(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(path_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(path_);
	}

	return DEVICE_OK;
}

cv::Mat FakeCamera::getImg() const throw (error_code)
{
	double dPos;

	int ret = GetCoreCallback()->GetFocusPosition(dPos);

	if (ret != 0)
		throw error_code(CONTROLLER_ERROR, "Focus device not found");
	
	std::string path = boost::replace_all_copy(path_, "#", to_string((int)dPos));

	return cv::imread(path, cv::IMREAD_GRAYSCALE);
}

void FakeCamera::initSize() const
{
	if (initSize_)
		return;

	try
	{
		cv::Mat img = getImg();

		roiWidth_ = width_ = img.rows;
		roiHeight_ = height_ = img.cols;

		initSize_ = true;
	}
	catch (error_code)
	{
		roiWidth_ = width_ = 0;
		roiHeight_ = height_ = 0;
	}
}
