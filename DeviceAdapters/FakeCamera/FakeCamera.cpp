#include "FakeCamera.h"

#include "boost/algorithm/string/replace.hpp"

#include "Util.h"

const char* cameraName = "FakeCamera";

FakeCamera::FakeCamera() :
	initialized_(false),
	path_(""),
	roiX_(0),
	roiY_(0),
	emptyImg(new uchar[1]),
	initSize_(false),
	curPath_(""),
	exposure_(10)
{
	CreateProperty("Path Mask", "", MM::String, false, new CPropertyAction(this, &FakeCamera::OnPath));

	CreateProperty(MM::g_Keyword_Name, cameraName, MM::String, true);

	// Description
	CreateProperty(MM::g_Keyword_Description, "Loads images from disk according to position of focusing stage", MM::String, true);

	// CameraName
	CreateProperty(MM::g_Keyword_CameraName, "Fake camera adapter", MM::String, true);

	// CameraID
	CreateProperty(MM::g_Keyword_CameraID, "V1.0", MM::String, true);

	// binning
	CreateProperty(MM::g_Keyword_Binning, "1", MM::Integer, false);

	SetErrorText(OUT_OF_RANGE, "Parameters out of range");
}

FakeCamera::~FakeCamera()
{
}

int FakeCamera::Initialize()
{
	if (initialized_)
		return DEVICE_OK;

	initSize_ = false;

	initialized_ = true;

	return DEVICE_OK;
}

int FakeCamera::Shutdown()
{
	initialized_ = false;

	return DEVICE_OK;
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
	return DEVICE_OK;
}

void FakeCamera::SetExposure(double exposure)
{
	exposure_ = exposure;
}

double FakeCamera::GetExposure() const
{
	return exposure_;
}

int FakeCamera::SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize)
{
	if (x + xSize > width_ || y + ySize > height_)
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
	if (!initSize_)
		return emptyImg;

	roi_ = curImg_(cv::Range(roiY_, roiY_ + roiHeight_), cv::Range(roiX_, roiX_ + roiWidth_));

	if (!roi_.isContinuous())
		roi_ = roi_.clone();

	return roi_.data;
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

	MM::MMTime start = GetCoreCallback()->GetCurrentMMTime();
	initSize();

	getImg();

	MM::MMTime end = GetCoreCallback()->GetCurrentMMTime();

	double rem = exposure_ - (end - start).getMsec();

	if (rem > 0)
		CDeviceUtils::SleepMs(rem);

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
		initSize_ = false;
		curPath_ = "";
		curImg_ = cv::Mat(0, (int*)0, 0, (void*)0, (size_t*)0);

		if (initialized_)
		{
			ERRH_START
				getImg();
			ERRH_END
		}
	}

	return DEVICE_OK;
}

void FakeCamera::getImg() const throw (error_code)
{
	double dPos;

	int ret = GetCoreCallback()->GetFocusPosition(dPos);

	if (ret != 0)
		throw error_code(CONTROLLER_ERROR, "Focus device not found");

	std::string path = boost::replace_all_copy(path_, "#", to_string((int)dPos));

	if (path == curPath_)
		return;

	cv::Mat img = cv::imread(path, cv::IMREAD_GRAYSCALE);

	if (img.data == NULL)
		if (curImg_.data != NULL)
		{
			LogMessage("Could not find image '" + path + "', reusing last valid image");
			curPath_ = path;
			return;
		}
		else
			throw error_code(CONTROLLER_ERROR, "Could not find image '" + path + "'. Please specify a valid path mask (# is used as placeholder for stage position)");

	curPath_ = path;
	curImg_ = img;
}

void FakeCamera::initSize() const
{
	if (initSize_)
		return;

	try
	{
		getImg();

		roiWidth_ = width_ = curImg_.cols;
		roiHeight_ = height_ = curImg_.rows;

		initSize_ = true;
	}
	catch (error_code)
	{
		roiWidth_ = width_ = 1;
		roiHeight_ = height_ = 1;
	}
}
