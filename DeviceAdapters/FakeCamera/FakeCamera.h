#pragma once

#include <string>

#include "DeviceBase.h"

#include "opencv/highgui.h"

#define ERR_INVALID_DEVICE_NAME 10000
#define OUT_OF_RANGE 10001
#define CONTROLLER_ERROR 10002

#include "error_code.h"

extern const char* cameraName;
extern const char* g_None;

class FakeCamera : public CCameraBase<FakeCamera>
{
public:
	FakeCamera();
	~FakeCamera();

	// Inherited via CCameraBase
	int Initialize();
	int Shutdown();
	void GetName(char * name) const;
	long GetImageBufferSize() const;
	unsigned GetBitDepth() const;
	int GetBinning() const;
	int SetBinning(int binSize);
	void SetExposure(double exp_ms);
	double GetExposure() const;
	int SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize);
	int GetROI(unsigned & x, unsigned & y, unsigned & xSize, unsigned & ySize);
	int ClearROI();
	int IsExposureSequenceable(bool & isSequenceable) const;
	const unsigned char * GetImageBuffer();
	unsigned GetImageWidth() const;
	unsigned GetImageHeight() const;
	unsigned GetImageBytesPerPixel() const;
	int SnapImage();
	int StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow);
	int StopSequenceAcquisition();
	void OnThreadExiting() throw();

	int OnPath(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct);

	std::string buildPath() const throw(error_code);
	void getImg() const throw (error_code);

	void initSize(bool loadImg = true) const;

private:
	bool initialized_;

	std::string path_;

	bool capturing_;
	mutable bool initSize_;
	mutable unsigned width_;
	mutable unsigned height_;
	mutable unsigned byteCount_;

	mutable unsigned roiX_;
	mutable unsigned roiY_;
	mutable unsigned roiWidth_;
	mutable unsigned roiHeight_;

	cv::Mat emptyImg;

	mutable cv::Mat curImg_;
	mutable cv::Mat lastFailedImg_;
	cv::Mat roi_;
	mutable std::string curPath_;
	mutable std::string lastFailedPath_;

	double exposure_;
};