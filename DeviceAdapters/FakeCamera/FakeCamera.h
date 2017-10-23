#pragma once

#include <string>

#include "DeviceBase.h"

#include "opencv/highgui.h"

#define CONTROLLER_ERROR 10002

#include "error_code.h"

extern const char* cameraName;

#define OUT_OF_RANGE 10001

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

	int OnPath(MM::PropertyBase* pProp, MM::ActionType eAct);

	cv::Mat getImg() const throw (error_code);

	void initSize() const;

private:
	std::string path_;
	mutable unsigned width_;
	mutable unsigned height_;

	mutable unsigned roiX_;
	mutable unsigned roiY_;
	mutable unsigned roiWidth_;
	mutable unsigned roiHeight_;

	mutable bool initSize_;

	cv::Mat curImg_;
};