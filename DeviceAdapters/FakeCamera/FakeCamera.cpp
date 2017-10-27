#include "FakeCamera.h"

#include "boost/algorithm/string/replace.hpp"

#include "Util.h"

const char* cameraName = "FakeCamera";
const char* g_None = "None";

FakeCamera::FakeCamera() :
	initialized_(false),
	path_(""),
	roiX_(0),
	roiY_(0),
	capturing_(false),
	emptyImg(1, 1, CV_8UC1),
	initSize_(false),
	byteCount_(1),
	curPath_(""),
	exposure_(10)
{
	*(emptyImg.data) = 0;

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

	CreateStringProperty(MM::g_Keyword_PixelType, "8bit", false, new CPropertyAction(this, &FakeCamera::OnPixelType));

	std::vector<std::string> pixelTypeValues;
	pixelTypeValues.push_back("8bit");
	pixelTypeValues.push_back("16bit");

	SetAllowedValues(MM::g_Keyword_PixelType, pixelTypeValues);
	
	SetErrorText(ERR_INVALID_DEVICE_NAME, "Specified stage name is invalid");
	SetErrorText(OUT_OF_RANGE, "Parameters out of range");

	InitializeDefaultErrorMessages();
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

	return roiWidth_ * roiHeight_ * byteCount_;
}

unsigned FakeCamera::GetBitDepth() const
{
	initSize();

	return 8 * byteCount_;
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
	initSize();

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
	{
		assert(width_ * height_ * byteCount_ == 1);
		return emptyImg.data;
	}

	roi_ = curImg_(cv::Range(roiY_, roiY_ + roiHeight_), cv::Range(roiX_, roiX_ + roiWidth_));

	if (!roi_.isContinuous())
		roi_ = roi_.clone();

	assert(roiWidth_ * roiHeight_ * byteCount_ == roi_.total() * roi_.elemSize());
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
	return byteCount_;
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

int FakeCamera::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
{
	capturing_ = true;
	return CCameraBase::StartSequenceAcquisition(numImages, interval_ms, stopOnOverflow);
}

int FakeCamera::StopSequenceAcquisition()
{
	capturing_ = false;
	return CCameraBase::StopSequenceAcquisition();
}

void FakeCamera::OnThreadExiting() throw()
{
	capturing_ = false;
	CCameraBase::OnThreadExiting();
}

int FakeCamera::OnPath(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(path_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		std::string oldPath = path_;
		pProp->Get(path_);
		initSize_ = false;
		curPath_ = "";
		curImg_ = emptyImg;

		if (initialized_)
		{
			ERRH_START
				try
				{
					getImg();
				}
				catch (error_code ex)
				{
					pProp->Set(oldPath.c_str());
					path_ = oldPath;
					throw ex;
				}
			ERRH_END
		}
	}

	return DEVICE_OK;
}

double scaleFac(int bef, int aft)
{
	return (double)(1 << (8 * aft)) / (1 << (8 * bef));
}

int FakeCamera::OnPixelType(MM::PropertyBase * pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(byteCount_ == 1 ? "8bit" : "16bit");
	}
	else if (eAct == MM::AfterSet)
	{
		if (capturing_)
			return DEVICE_CAMERA_BUSY_ACQUIRING;

		std::string val;
		pProp->Get(val);

		byteCount_ = val == "16bit" ? 2 : 1;

		emptyImg.convertTo(emptyImg, byteCount_ == 1 ? CV_8U : CV_16U, scaleFac(emptyImg.elemSize(), byteCount_));
		if (curImg_.data != NULL)
			curImg_.convertTo(curImg_, byteCount_ == 1 ? CV_8U : CV_16U, scaleFac(curImg_.elemSize(), byteCount_));
	}

	return DEVICE_OK;
}

std::string FakeCamera::buildPath() const throw (error_code)
{
	std::ostringstream path;

	path << std::fixed;

	int mode = 0;
	unsigned long int prec = 0;

	for (const char* it = path_.data(); it != path_.data() + path_.size(); ++it)
	{
		switch (mode)
		{
		case 0:
			if (*it == '?')
				mode = 1;
			else
				path << *it;
			break;
		case 1:
			if (*it == '{')
			{
				mode = 2;
				break;
			}
		case 3:
			if (*it == '?')
			{
				double pos;
				int ret = GetCoreCallback()->GetFocusPosition(pos);
				if (ret != 0)
					pos = 0;

				if (prec == 0)
					path << (int)pos;
				else
					path << std::setprecision(prec) << pos;

				prec = 0;
				mode = 0;
				break;
			}
			else if (*it == '[')
				mode = 4;
			else
				throw error_code(CONTROLLER_ERROR, "Invalid path specification. No stage name specified. (format: ?? for focus stage, ?[name] for any stage, and ?{prec}[name]/?{prec}? for precision other than 0)");
			break;
		case 2:
			char* end;
			prec = strtoul(it, &end, 10);
			it = end;

			if (prec < 0)
				prec = 0;

			if (*it == '}')
				mode = 3;
			else
				throw error_code(CONTROLLER_ERROR, "Invalid precision specification. (format: ?? for focus stage, ?[name] for any stage, and ?{prec}[name]/?{prec}? for precision other than 0)");
			break;
		case 4:
			std::ostringstream name;
			for (; *it != ']' && it != path_.data() + path_.size() - 1; name << *(it++));

			if (*it == ']')
			{
				MM::Stage* stage = (MM::Stage*)GetCoreCallback()->GetDevice(this, name.str().c_str());
				if (!stage)
					throw error_code(CONTROLLER_ERROR, "Invalid stage name '"+ name.str() + "'. (format: ?? for focus stage, ?[name] for any stage, and ?{prec}[name]/?{prec}? for precision other than 0)");

				double pos;
				int ret = stage->GetPositionUm(pos);
				if (ret != 0)
					pos = 0;

				if (prec == 0)
					path << (int)pos;
				else
					path << std::setprecision(prec) << pos;

				prec = 0;
				mode = 0;
			}
			else
				throw error_code(CONTROLLER_ERROR, "Invalid name specification. (format: ?? for focus stage, ?[name] for any stage, and ?{prec}[name]/?{prec}? for precision other than 0)");
		}
	}

	return path.str();
}

void FakeCamera::getImg() const throw (error_code)
{
	std::string path = buildPath();

	if (path == curPath_)
		return;

	cv::Mat img = path == lastFailedPath_ ? lastFailedImg_ : cv::imread(path, cv::IMREAD_GRAYSCALE | cv::IMREAD_ANYDEPTH);

	if (img.data == NULL)
		if (curImg_.data != NULL)
		{
			LogMessage("Could not find image '" + path + "', reusing last valid image");
			curPath_ = path;
			return;
		}
		else
			throw error_code(CONTROLLER_ERROR, "Could not find image '" + path + "'. Please specify a valid path mask (format: ?? for focus stage, ?[name] for any stage, and ?{prec}[name]/?{prec}? for precision other than 0)");

	if (img.elemSize() != byteCount_)
		img.convertTo(img, byteCount_ == 1 ? CV_8U : CV_16U, scaleFac(img.elemSize(), byteCount_));

	if (img.cols != width_ || img.rows != height_)
		if (capturing_)
		{
			lastFailedPath_ = path;
			lastFailedImg_ = img;
			throw error_code(DEVICE_CAMERA_BUSY_ACQUIRING);
		}
		else
		{
			curPath_ = path;
			curImg_ = img;

			initSize_ = false;
			initSize(false);
		}

	curPath_ = path;
	curImg_ = img;
}

void FakeCamera::initSize(bool loadImg) const
{
	if (initSize_)
		return;

	initSize_ = true;

	try
	{
		if (loadImg)
			getImg();

		roiWidth_ = width_ = curImg_.cols;
		roiHeight_ = height_ = curImg_.rows;
	}
	catch (error_code)
	{
		roiWidth_ = width_ = 1;
		roiHeight_ = height_ = 1;

		initSize_ = false;
	}
}
