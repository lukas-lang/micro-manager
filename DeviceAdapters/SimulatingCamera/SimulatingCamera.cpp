///////////////////////////////////////////////////////////////////////////////
// FILE:          SimulatingCamera.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   ...
//                
// AUTHOR:        Christian C. Sachs
//
// COPYRIGHT:     
// LICENSE:       

#include "SimulatingCamera.h"
#include <cstdio>
#include <string>
#include <math.h>
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>
#include <algorithm>
#include <iostream>



using namespace std;
const double CDemoCamera::nominalPixelSizeUm_ = 1.0;
double g_IntensityFactor_ = 1.0;

// External names used used by the rest of the system
// to load particular device from the "DemoCamera.dll" library
const char* g_CameraDeviceName = "SimulatingCamera";


// constants for naming pixel types (allowed values of the "PixelType" property)
const char* g_PixelType_8bit = "8bit";
const char* g_PixelType_16bit = "16bit";
const char* g_PixelType_32bitRGB = "32bitRGB";
const char* g_PixelType_64bitRGB = "64bitRGB";
const char* g_PixelType_32bit = "32bit";  // floating point greyscale

// constants for naming camera modes
const char* g_Sine_Wave = "Artificial Waves";
const char* g_Norm_Noise = "Noise";

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////

MODULE_API void InitializeModuleData()
{
   RegisterDevice(g_CameraDeviceName, MM::CameraDevice, "SimulatingCamera");
   RegisterDevice("TestProcessor", MM::ImageProcessorDevice, "TestProcessor");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

   // decide which device class to create based on the deviceName parameter
   if (strcmp(deviceName, g_CameraDeviceName) == 0)
   {
      // create camera
      return new CDemoCamera();
   }

   else if(strcmp(deviceName, "TestProcessor") == 0)
   {
      return new TransposeProcessor();
   }
   
   // ...supplied name not recognized
   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}


CDemoCamera::CDemoCamera() :
   CCameraBase<CDemoCamera> (),
   exposureMaximum_(10000.0),
   dPhase_(0),
   initialized_(false),
   readoutUs_(0.0),
   scanMode_(1),
   bitDepth_(8),
   roiX_(0),
   roiY_(0),
   sequenceStartTime_(0),
   isSequenceable_(false),
   sequenceMaxLength_(100),
   sequenceRunning_(false),
   sequenceIndex_(0),
   binSize_(1),
   cameraCCDXSize_(512),
   cameraCCDYSize_(512),
   ccdT_ (0.0),
   triggerDevice_(""),
   stopOnOverflow_(false),
   dropPixels_(false),
   fastImage_(false),
   saturatePixels_(false),
   fractionOfPixelsToDropOrSaturate_(0.002),
   shouldRotateImages_(false),
   shouldDisplayImageNumber_(false),
   stripeWidth_(1.0),
   nComponents_(1),
   mode_(0)
{
   memset(testProperty_,0,sizeof(testProperty_));

   // call the base class method to set-up default error codes/messages
   InitializeDefaultErrorMessages();
   readoutStartTime_ = GetCurrentMMTime();
   thd_ = new MySequenceThread(this);

   // parent ID display
   CreateHubIDProperty();

   CreateFloatProperty("MaximumExposureMs", exposureMaximum_, false,
         new CPropertyAction(this, &CDemoCamera::OnMaxExposure),
         true);
}

CDemoCamera::~CDemoCamera()
{
   StopSequenceAcquisition();
   delete thd_;
}


void CDemoCamera::GetName(char* name) const
{
   // Return the name used to referr to this device adapte
   CDeviceUtils::CopyLimitedString(name, g_CameraDeviceName);
}


int CDemoCamera::Initialize()
{
   if (initialized_)
      return DEVICE_OK;
   
   // set property list
   // -----------------

   // Name
   int nRet = CreateStringProperty(MM::g_Keyword_Name, g_CameraDeviceName, true);
   if (DEVICE_OK != nRet)
      return nRet;

   // Description
   nRet = CreateStringProperty(MM::g_Keyword_Description, "Demo Camera Device Adapter", true);
   if (DEVICE_OK != nRet)
      return nRet;

   // CameraName
   nRet = CreateStringProperty(MM::g_Keyword_CameraName, "DemoCamera-MultiMode", true);
   assert(nRet == DEVICE_OK);

   // CameraID
   nRet = CreateStringProperty(MM::g_Keyword_CameraID, "V1.0", true);
   assert(nRet == DEVICE_OK);

   // binning
   CPropertyAction *pAct = new CPropertyAction (this, &CDemoCamera::OnBinning);
   nRet = CreateIntegerProperty(MM::g_Keyword_Binning, 1, false, pAct);
   assert(nRet == DEVICE_OK);

   nRet = SetAllowedBinning();
   if (nRet != DEVICE_OK)
      return nRet;

   // pixel type
   pAct = new CPropertyAction (this, &CDemoCamera::OnPixelType);
   nRet = CreateStringProperty(MM::g_Keyword_PixelType, g_PixelType_8bit, false, pAct);
   assert(nRet == DEVICE_OK);

   vector<string> pixelTypeValues;
   pixelTypeValues.push_back(g_PixelType_8bit);
   pixelTypeValues.push_back(g_PixelType_16bit); 
   pixelTypeValues.push_back(g_PixelType_32bitRGB);
   pixelTypeValues.push_back(g_PixelType_64bitRGB);
   //pixelTypeValues.push_back(::g_PixelType_32bit);

   nRet = SetAllowedValues(MM::g_Keyword_PixelType, pixelTypeValues);
   if (nRet != DEVICE_OK)
      return nRet;

   // Bit depth
   pAct = new CPropertyAction (this, &CDemoCamera::OnBitDepth);
   nRet = CreateIntegerProperty("BitDepth", 8, false, pAct);
   assert(nRet == DEVICE_OK);

   vector<string> bitDepths;
   bitDepths.push_back("8");
   bitDepths.push_back("10");
   bitDepths.push_back("12");
   bitDepths.push_back("14");
   bitDepths.push_back("16");
   bitDepths.push_back("32");
   nRet = SetAllowedValues("BitDepth", bitDepths);
   if (nRet != DEVICE_OK)
      return nRet;

   // exposure
   nRet = CreateFloatProperty(MM::g_Keyword_Exposure, 10.0, false);
   assert(nRet == DEVICE_OK);
   SetPropertyLimits(MM::g_Keyword_Exposure, 0.0, exposureMaximum_);
 

   
   // scan mode
   pAct = new CPropertyAction (this, &CDemoCamera::OnScanMode);
   nRet = CreateIntegerProperty("ScanMode", 1, false, pAct);
   assert(nRet == DEVICE_OK);
   AddAllowedValue("ScanMode","1");
   AddAllowedValue("ScanMode","2");
   AddAllowedValue("ScanMode","3");

   // camera gain
   nRet = CreateIntegerProperty(MM::g_Keyword_Gain, 0, false);
   assert(nRet == DEVICE_OK);
   SetPropertyLimits(MM::g_Keyword_Gain, -5, 8);

   // camera offset
   nRet = CreateIntegerProperty(MM::g_Keyword_Offset, 0, false);
   assert(nRet == DEVICE_OK);

   // camera temperature
   pAct = new CPropertyAction (this, &CDemoCamera::OnCCDTemp);
   nRet = CreateFloatProperty(MM::g_Keyword_CCDTemperature, 0, false, pAct);
   assert(nRet == DEVICE_OK);
   SetPropertyLimits(MM::g_Keyword_CCDTemperature, -100, 10);

   // camera temperature RO
   pAct = new CPropertyAction (this, &CDemoCamera::OnCCDTemp);
   nRet = CreateFloatProperty("CCDTemperature RO", 0, true, pAct);
   assert(nRet == DEVICE_OK);

   // readout time
   pAct = new CPropertyAction (this, &CDemoCamera::OnReadoutTime);
   nRet = CreateFloatProperty(MM::g_Keyword_ReadoutTime, 0, false, pAct);
   assert(nRet == DEVICE_OK);

   // CCD size of the camera we are modeling
   pAct = new CPropertyAction (this, &CDemoCamera::OnCameraCCDXSize);
   CreateIntegerProperty("OnCameraCCDXSize", 512, false, pAct);
   pAct = new CPropertyAction (this, &CDemoCamera::OnCameraCCDYSize);
   CreateIntegerProperty("OnCameraCCDYSize", 512, false, pAct);

   // Trigger device
   pAct = new CPropertyAction (this, &CDemoCamera::OnTriggerDevice);
   CreateStringProperty("TriggerDevice", "", false, pAct);

   pAct = new CPropertyAction (this, &CDemoCamera::OnDropPixels);
   CreateIntegerProperty("DropPixels", 0, false, pAct);
   AddAllowedValue("DropPixels", "0");
   AddAllowedValue("DropPixels", "1");

   pAct = new CPropertyAction (this, &CDemoCamera::OnSaturatePixels);
   CreateIntegerProperty("SaturatePixels", 0, false, pAct);
   AddAllowedValue("SaturatePixels", "0");
   AddAllowedValue("SaturatePixels", "1");

   pAct = new CPropertyAction (this, &CDemoCamera::OnFastImage);
   CreateIntegerProperty("FastImage", 0, false, pAct);
   AddAllowedValue("FastImage", "0");
   AddAllowedValue("FastImage", "1");

   pAct = new CPropertyAction (this, &CDemoCamera::OnFractionOfPixelsToDropOrSaturate);
   CreateFloatProperty("FractionOfPixelsToDropOrSaturate", 0.002, false, pAct);
   SetPropertyLimits("FractionOfPixelsToDropOrSaturate", 0., 0.1);

   pAct = new CPropertyAction(this, &CDemoCamera::OnShouldRotateImages);
   CreateIntegerProperty("RotateImages", 0, false, pAct);
   AddAllowedValue("RotateImages", "0");
   AddAllowedValue("RotateImages", "1");

   pAct = new CPropertyAction(this, &CDemoCamera::OnShouldDisplayImageNumber);
   CreateIntegerProperty("DisplayImageNumber", 0, false, pAct);
   AddAllowedValue("DisplayImageNumber", "0");
   AddAllowedValue("DisplayImageNumber", "1");

   pAct = new CPropertyAction(this, &CDemoCamera::OnStripeWidth);
   CreateFloatProperty("StripeWidth", 0, false, pAct);
   SetPropertyLimits("StripeWidth", 0, 10);

   // Whether or not to use exposure time sequencing
   pAct = new CPropertyAction (this, &CDemoCamera::OnIsSequenceable);
   std::string propName = "UseExposureSequences";
   CreateStringProperty(propName.c_str(), "No", false, pAct);
   AddAllowedValue(propName.c_str(), "Yes");
   AddAllowedValue(propName.c_str(), "No");

   // Camera mode: 
   pAct = new CPropertyAction (this, &CDemoCamera::OnMode);
   propName = "Mode";
   CreateStringProperty(propName.c_str(), g_Sine_Wave, false, pAct);
   AddAllowedValue(propName.c_str(), g_Sine_Wave);
   AddAllowedValue(propName.c_str(), g_Norm_Noise);

   // Simulate application crash
   pAct = new CPropertyAction(this, &CDemoCamera::OnCrash);
   CreateStringProperty("SimulateCrash", "", false, pAct);
   AddAllowedValue("SimulateCrash", "");
   AddAllowedValue("SimulateCrash", "Dereference Null Pointer");
   AddAllowedValue("SimulateCrash", "Divide by Zero");

   // synchronize all properties
   // --------------------------
   nRet = UpdateStatus();
   if (nRet != DEVICE_OK)
      return nRet;


   // setup the buffer
   // ----------------
   nRet = ResizeImageBuffer();
   if (nRet != DEVICE_OK)
      return nRet;

#ifdef TESTRESOURCELOCKING
   TestResourceLocking(true);
   LogMessage("TestResourceLocking OK",true);
#endif


   initialized_ = true;

   // initialize image buffer
   GenerateEmptyImage(img_);
   return DEVICE_OK;


}

/**
* Shuts down (unloads) the device.
* Required by the MM::Device API.
* Ideally this method will completely unload the device and release all resources.
* Shutdown() may be called multiple times in a row.
* After Shutdown() we should be allowed to call Initialize() again to load the device
* without causing problems.
*/
int CDemoCamera::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

/**
* Performs exposure and grabs a single image.
* This function should block during the actual exposure and return immediately afterwards 
* (i.e., before readout).  This behavior is needed for proper synchronization with the shutter.
* Required by the MM::Camera API.
*/
int CDemoCamera::SnapImage()
{
   static int callCounter = 0;
   ++callCounter;
   
   
   double x, y, z;
   
   GetCoreCallback()->GetXYPosition(x, y);
   GetCoreCallback()->GetFocusPosition(z);
   
   // This works ... properly fetches <x,y,z> coordinates
   

   MM::MMTime startTime = GetCurrentMMTime();
   double exp = GetExposure();
   if (sequenceRunning_ && IsCapturing()) 
   {
      exp = GetSequenceExposure();
   }

   if (!fastImage_)
   {
      GenerateSyntheticImage(img_, exp);
   }

   MM::MMTime s0(0,0);
   if( s0 < startTime )
   {
      while (exp > (GetCurrentMMTime() - startTime).getMsec())
      {
         CDeviceUtils::SleepMs(1);
      }      
   }
   else
   {
      std::cerr << "You are operating this device adapter without setting the core callback, timing functions aren't yet available" << std::endl;
      // called without the core callback probably in off line test program
      // need way to build the core in the test program

   }
   readoutStartTime_ = GetCurrentMMTime();

   return DEVICE_OK;
}


/**
* Returns pixel data.
* Required by the MM::Camera API.
* The calling program will assume the size of the buffer based on the values
* obtained from GetImageBufferSize(), which in turn should be consistent with
* values returned by GetImageWidth(), GetImageHight() and GetImageBytesPerPixel().
* The calling program allso assumes that camera never changes the size of
* the pixel buffer on its own. In other words, the buffer can change only if
* appropriate properties are set (such as binning, pixel type, etc.)
*/
const unsigned char* CDemoCamera::GetImageBuffer()
{
   MMThreadGuard g(imgPixelsLock_);
   MM::MMTime readoutTime(readoutUs_);
   while (readoutTime > (GetCurrentMMTime() - readoutStartTime_)) {}      
   unsigned char *pB = (unsigned char*)(img_.GetPixels());
   return pB;
}

/**
* Returns image buffer X-size in pixels.
* Required by the MM::Camera API.
*/
unsigned CDemoCamera::GetImageWidth() const
{
   return img_.Width();
}

/**
* Returns image buffer Y-size in pixels.
* Required by the MM::Camera API.
*/
unsigned CDemoCamera::GetImageHeight() const
{
   return img_.Height();
}

/**
* Returns image buffer pixel depth in bytes.
* Required by the MM::Camera API.
*/
unsigned CDemoCamera::GetImageBytesPerPixel() const
{
   return img_.Depth();
} 

/**
* Returns the bit depth (dynamic range) of the pixel.
* This does not affect the buffer size, it just gives the client application
* a guideline on how to interpret pixel values.
* Required by the MM::Camera API.
*/
unsigned CDemoCamera::GetBitDepth() const
{
   return bitDepth_;
}

/**
* Returns the size in bytes of the image buffer.
* Required by the MM::Camera API.
*/
long CDemoCamera::GetImageBufferSize() const
{
   return img_.Width() * img_.Height() * GetImageBytesPerPixel();
}

/**
* Sets the camera Region Of Interest.
* Required by the MM::Camera API.
* This command will change the dimensions of the image.
* Depending on the hardware capabilities the camera may not be able to configure the
* exact dimensions requested - but should try do as close as possible.
* If the hardware does not have this capability the software should simulate the ROI by
* appropriately cropping each frame.
* This demo implementation ignores the position coordinates and just crops the buffer.
* @param x - top-left corner coordinate
* @param y - top-left corner coordinate
* @param xSize - width
* @param ySize - height
*/
int CDemoCamera::SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize)
{
   if (xSize == 0 && ySize == 0)
   {
      // effectively clear ROI
      ResizeImageBuffer();
      roiX_ = 0;
      roiY_ = 0;
   }
   else
   {
      // apply ROI
      img_.Resize(xSize, ySize);
      roiX_ = x;
      roiY_ = y;
   }
   return DEVICE_OK;
}

/**
* Returns the actual dimensions of the current ROI.
* Required by the MM::Camera API.
*/
int CDemoCamera::GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize)
{
   x = roiX_;
   y = roiY_;

   xSize = img_.Width();
   ySize = img_.Height();

   return DEVICE_OK;
}

/**
* Resets the Region of Interest to full frame.
* Required by the MM::Camera API.
*/
int CDemoCamera::ClearROI()
{
   ResizeImageBuffer();
   roiX_ = 0;
   roiY_ = 0;
      
   return DEVICE_OK;
}

/**
* Returns the current exposure setting in milliseconds.
* Required by the MM::Camera API.
*/
double CDemoCamera::GetExposure() const
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty(MM::g_Keyword_Exposure, buf);
   if (ret != DEVICE_OK)
      return 0.0;
   return atof(buf);
}

/**
 * Returns the current exposure from a sequence and increases the sequence counter
 * Used for exposure sequences
 */
double CDemoCamera::GetSequenceExposure() 
{
   if (exposureSequence_.size() == 0) 
      return this->GetExposure();

   double exposure = exposureSequence_[sequenceIndex_];

   sequenceIndex_++;
   if (sequenceIndex_ >= exposureSequence_.size())
      sequenceIndex_ = 0;

   return exposure;
}

/**
* Sets exposure in milliseconds.
* Required by the MM::Camera API.
*/
void CDemoCamera::SetExposure(double exp)
{
   SetProperty(MM::g_Keyword_Exposure, CDeviceUtils::ConvertToString(exp));
   GetCoreCallback()->OnExposureChanged(this, exp);;
}

/**
* Returns the current binning factor.
* Required by the MM::Camera API.
*/
int CDemoCamera::GetBinning() const
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty(MM::g_Keyword_Binning, buf);
   if (ret != DEVICE_OK)
      return 1;
   return atoi(buf);
}

/**
* Sets binning factor.
* Required by the MM::Camera API.
*/
int CDemoCamera::SetBinning(int binF)
{
   return SetProperty(MM::g_Keyword_Binning, CDeviceUtils::ConvertToString(binF));
}

int CDemoCamera::IsExposureSequenceable(bool& isSequenceable) const
{
   isSequenceable = false; //isSequenceable_;
   return DEVICE_OK;
}

int CDemoCamera::GetExposureSequenceMaxLength(long& nrEvents) const
{
   if (!isSequenceable_) {
      return DEVICE_UNSUPPORTED_COMMAND;
   }

   nrEvents = sequenceMaxLength_;
   return DEVICE_OK;
}

int CDemoCamera::StartExposureSequence()
{
   if (!isSequenceable_) {
      return DEVICE_UNSUPPORTED_COMMAND;
   }

   // may need thread lock
   sequenceRunning_ = true;
   return DEVICE_OK;
}

int CDemoCamera::StopExposureSequence()
{
   if (!isSequenceable_) {
      return DEVICE_UNSUPPORTED_COMMAND;
   }

   // may need thread lock
   sequenceRunning_ = false;
   sequenceIndex_ = 0;
   return DEVICE_OK;
}

/**
 * Clears the list of exposures used in sequences
 */
int CDemoCamera::ClearExposureSequence()
{
   if (!isSequenceable_) {
      return DEVICE_UNSUPPORTED_COMMAND;
   }

   exposureSequence_.clear();
   return DEVICE_OK;
}

/**
 * Adds an exposure to a list of exposures used in sequences
 */
int CDemoCamera::AddToExposureSequence(double exposureTime_ms) 
{
   if (!isSequenceable_) {
      return DEVICE_UNSUPPORTED_COMMAND;
   }

   exposureSequence_.push_back(exposureTime_ms);
   return DEVICE_OK;
}

int CDemoCamera::SendExposureSequence() const {
   if (!isSequenceable_) {
      return DEVICE_UNSUPPORTED_COMMAND;
   }

   return DEVICE_OK;
}

int CDemoCamera::SetAllowedBinning() 
{
   vector<string> binValues;
   binValues.push_back("1");
   binValues.push_back("2");
   if (scanMode_ < 3)
      binValues.push_back("4");
   if (scanMode_ < 2)
      binValues.push_back("8");
   if (binSize_ == 8 && scanMode_ == 3) {
      SetProperty(MM::g_Keyword_Binning, "2");
   } else if (binSize_ == 8 && scanMode_ == 2) {
      SetProperty(MM::g_Keyword_Binning, "4");
   } else if (binSize_ == 4 && scanMode_ == 3) {
      SetProperty(MM::g_Keyword_Binning, "2");
   }
      
   LogMessage("Setting Allowed Binning settings", true);
   return SetAllowedValues(MM::g_Keyword_Binning, binValues);
}


/**
 * Required by the MM::Camera API
 * Please implement this yourself and do not rely on the base class implementation
 * The Base class implementation is deprecated and will be removed shortly
 */
int CDemoCamera::StartSequenceAcquisition(double interval)
{
   return StartSequenceAcquisition(LONG_MAX, interval, false);            
}

/**                                                                       
* Stop and wait for the Sequence thread finished                                   
*/                                                                        
int CDemoCamera::StopSequenceAcquisition()                                     
{
   if (!thd_->IsStopped()) {
      thd_->Stop();                                                       
      thd_->wait();                                                       
   }                                                                     
                                                                          
   return DEVICE_OK;                                                      
} 

/**
* Simple implementation of Sequence Acquisition
* A sequence acquisition should run on its own thread and transport new images
* coming of the camera into the MMCore circular buffer.
*/
int CDemoCamera::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
{
   if (IsCapturing())
      return DEVICE_CAMERA_BUSY_ACQUIRING;

   int ret = GetCoreCallback()->PrepareForAcq(this);
   if (ret != DEVICE_OK)
      return ret;
   sequenceStartTime_ = GetCurrentMMTime();
   imageCounter_ = 0;
   thd_->Start(numImages,interval_ms);
   stopOnOverflow_ = stopOnOverflow;
   
   return DEVICE_OK;
}

/*
 * Inserts Image and MetaData into MMCore circular Buffer
 */
int CDemoCamera::InsertImage()
{
   MM::MMTime timeStamp = this->GetCurrentMMTime();
   char label[MM::MaxStrLength];
   this->GetLabel(label);
 
   // Important:  metadata about the image are generated here:
   Metadata md;
   md.put("Camera", label);
   md.put(MM::g_Keyword_Metadata_StartTime, CDeviceUtils::ConvertToString(sequenceStartTime_.getMsec()));
   md.put(MM::g_Keyword_Elapsed_Time_ms, CDeviceUtils::ConvertToString((timeStamp - sequenceStartTime_).getMsec()));
   md.put(MM::g_Keyword_Metadata_ROI_X, CDeviceUtils::ConvertToString( (long) roiX_)); 
   md.put(MM::g_Keyword_Metadata_ROI_Y, CDeviceUtils::ConvertToString( (long) roiY_)); 

   imageCounter_++;

   char buf[MM::MaxStrLength];
   GetProperty(MM::g_Keyword_Binning, buf);
   md.put(MM::g_Keyword_Binning, buf);

   MMThreadGuard g(imgPixelsLock_);

   const unsigned char* pI;
   pI = GetImageBuffer();

   unsigned int w = GetImageWidth();
   unsigned int h = GetImageHeight();
   unsigned int b = GetImageBytesPerPixel();

   int ret = GetCoreCallback()->InsertImage(this, pI, w, h, b, md.Serialize().c_str());
   if (!stopOnOverflow_ && ret == DEVICE_BUFFER_OVERFLOW)
   {
      // do not stop on overflow - just reset the buffer
      GetCoreCallback()->ClearImageBuffer(this);
      // don't process this same image again...
      return GetCoreCallback()->InsertImage(this, pI, w, h, b, md.Serialize().c_str(), false);
   }
   else
   {
      return ret;
   }
}

/*
 * Do actual capturing
 * Called from inside the thread  
 */
int CDemoCamera::RunSequenceOnThread(MM::MMTime startTime)
{
   int ret=DEVICE_ERR;
   
   // Trigger
   if (triggerDevice_.length() > 0) {
      MM::Device* triggerDev = GetDevice(triggerDevice_.c_str());
      if (triggerDev != 0) {
         LogMessage("trigger requested");
         triggerDev->SetProperty("Trigger","+");
      }
   }

   double exposure = GetSequenceExposure();

   if (!fastImage_)
   {
      GenerateSyntheticImage(img_, exposure);
   }

   // Simulate exposure duration
   double finishTime = exposure * (imageCounter_ + 1);
   while ((GetCurrentMMTime() - startTime).getMsec() < finishTime)
   {
      CDeviceUtils::SleepMs(1);
   }

   ret = InsertImage();

   if (ret != DEVICE_OK)
   {
      return ret;
   }
   return ret;
};

bool CDemoCamera::IsCapturing() {
  return !thd_->IsStopped();
  //return false;
}

/*
 * called from the thread function before exit 
 */
void CDemoCamera::OnThreadExiting() throw()
{
   try
   {
      LogMessage(g_Msg_SEQUENCE_ACQUISITION_THREAD_EXITING);
      GetCoreCallback()?GetCoreCallback()->AcqFinished(this,0):DEVICE_OK;
   }
   catch(...)
   {
      LogMessage(g_Msg_EXCEPTION_IN_ON_THREAD_EXITING, false);
   }
}


MySequenceThread::MySequenceThread(CDemoCamera* pCam)
   :intervalMs_(default_intervalMS)
   ,numImages_(default_numImages)
   ,imageCounter_(0)
   ,stop_(true)
   ,suspend_(false)
   ,camera_(pCam)
   ,startTime_(0)
   ,actualDuration_(0)
   ,lastFrameTime_(0)
{};

MySequenceThread::~MySequenceThread() {};

void MySequenceThread::Stop() {
   MMThreadGuard g(this->stopLock_);
   stop_=true;
}

void MySequenceThread::Start(long numImages, double intervalMs)
{
   MMThreadGuard g1(this->stopLock_);
   MMThreadGuard g2(this->suspendLock_);
   numImages_=numImages;
   intervalMs_=intervalMs;
   imageCounter_=0;
   stop_ = false;
   suspend_=false;
   activate();
   actualDuration_ = 0;
   startTime_= camera_->GetCurrentMMTime();
   lastFrameTime_ = 0;
}

bool MySequenceThread::IsStopped(){
   MMThreadGuard g(this->stopLock_);
   return stop_;
}

void MySequenceThread::Suspend() {
   MMThreadGuard g(this->suspendLock_);
   suspend_ = true;
}

bool MySequenceThread::IsSuspended() {
   MMThreadGuard g(this->suspendLock_);
   return suspend_;
}

void MySequenceThread::Resume() {
   MMThreadGuard g(this->suspendLock_);
   suspend_ = false;
}

int MySequenceThread::svc(void) throw()
{
   int ret=DEVICE_ERR;
   try 
   {
      do
      {  
         ret = camera_->RunSequenceOnThread(startTime_);
      } while (DEVICE_OK == ret && !IsStopped() && imageCounter_++ < numImages_-1);
      if (IsStopped())
         camera_->LogMessage("SeqAcquisition interrupted by the user\n");
   }catch(...){
      camera_->LogMessage(g_Msg_EXCEPTION_IN_THREAD, false);
   }
   stop_=true;
   actualDuration_ = camera_->GetCurrentMMTime() - startTime_;
   camera_->OnThreadExiting();
   return ret;
}



///////////////////////////////////////////////////////////////////////////////
// CDemoCamera Action handlers
///////////////////////////////////////////////////////////////////////////////

int CDemoCamera::OnMaxExposure(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(exposureMaximum_);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(exposureMaximum_);
   }
   return DEVICE_OK;
}


/*
* this Read Only property will update whenever any property is modified
*/

int CDemoCamera::OnTestProperty(MM::PropertyBase* pProp, MM::ActionType eAct, long indexx)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(testProperty_[indexx]);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(testProperty_[indexx]);
   }
   return DEVICE_OK;

}


/**
* Handles "Binning" property.
*/
int CDemoCamera::OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   int ret = DEVICE_ERR;
   switch(eAct)
   {
   case MM::AfterSet:
      {
         if(IsCapturing())
            return DEVICE_CAMERA_BUSY_ACQUIRING;

         // the user just set the new value for the property, so we have to
         // apply this value to the 'hardware'.
         long binFactor;
         pProp->Get(binFactor);
         if(binFactor > 0 && binFactor < 10)
         {
            // calculate ROI using the previous bin settings
            double factor = (double) binFactor / (double) binSize_;
            roiX_ /= factor;
            roiY_ /= factor;
            img_.Resize(img_.Width()/factor, img_.Height()/factor);
            binSize_ = binFactor;
            std::ostringstream os;
            os << binSize_;
            OnPropertyChanged("Binning", os.str().c_str());
            ret=DEVICE_OK;
         }
      }break;
   case MM::BeforeGet:
      {
         ret=DEVICE_OK;
         pProp->Set(binSize_);
      }break;
   default:
      break;
   }
   return ret; 
}

/**
* Handles "PixelType" property.
*/
int CDemoCamera::OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   int ret = DEVICE_ERR;
   switch(eAct)
   {
   case MM::AfterSet:
      {
         if(IsCapturing())
            return DEVICE_CAMERA_BUSY_ACQUIRING;

         string pixelType;
         pProp->Get(pixelType);

         if (pixelType.compare(g_PixelType_8bit) == 0)
         {
            nComponents_ = 1;
            img_.Resize(img_.Width(), img_.Height(), 1);
            bitDepth_ = 8;
            ret=DEVICE_OK;
         }
         else if (pixelType.compare(g_PixelType_16bit) == 0)
         {
            nComponents_ = 1;
            img_.Resize(img_.Width(), img_.Height(), 2);
            bitDepth_ = 16;
            ret=DEVICE_OK;
         }
         else if ( pixelType.compare(g_PixelType_32bitRGB) == 0)
         {
            nComponents_ = 4;
            img_.Resize(img_.Width(), img_.Height(), 4);
            bitDepth_ = 8;
            ret=DEVICE_OK;
         }
         else if ( pixelType.compare(g_PixelType_64bitRGB) == 0)
         {
            nComponents_ = 4;
            img_.Resize(img_.Width(), img_.Height(), 8);
            bitDepth_ = 16;
            ret=DEVICE_OK;
         }
         else if ( pixelType.compare(g_PixelType_32bit) == 0)
         {
            nComponents_ = 1;
            img_.Resize(img_.Width(), img_.Height(), 4);
            bitDepth_ = 32;
            ret=DEVICE_OK;
         }
         else
         {
            // on error switch to default pixel type
            nComponents_ = 1;
            img_.Resize(img_.Width(), img_.Height(), 1);
            pProp->Set(g_PixelType_8bit);
            bitDepth_ = 8;
            ret = ERR_UNKNOWN_MODE;
         }
      }
      break;
   case MM::BeforeGet:
      {
         long bytesPerPixel = GetImageBytesPerPixel();
         if (bytesPerPixel == 1)
         {
            pProp->Set(g_PixelType_8bit);
         }
         else if (bytesPerPixel == 2)
         {
            pProp->Set(g_PixelType_16bit);
         }
         else if (bytesPerPixel == 4)
         {
            if (nComponents_ == 4)
            {
               pProp->Set(g_PixelType_32bitRGB);
            }
            else if (nComponents_ == 1)
            {
               pProp->Set(::g_PixelType_32bit);
            }
         }
         else if (bytesPerPixel == 8)
         {
            pProp->Set(g_PixelType_64bitRGB);
         }
       else
         {
            pProp->Set(g_PixelType_8bit);
         }
         ret = DEVICE_OK;
      } break;
   default:
      break;
   }
   return ret; 
}

/**
* Handles "BitDepth" property.
*/
int CDemoCamera::OnBitDepth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   int ret = DEVICE_ERR;
   switch(eAct)
   {
   case MM::AfterSet:
      {
         if(IsCapturing())
            return DEVICE_CAMERA_BUSY_ACQUIRING;

         long bitDepth;
         pProp->Get(bitDepth);

         unsigned int bytesPerComponent;

         switch (bitDepth) {
            case 8:
               bytesPerComponent = 1;
               bitDepth_ = 8;
               ret=DEVICE_OK;
            break;
            case 10:
               bytesPerComponent = 2;
               bitDepth_ = 10;
               ret=DEVICE_OK;
            break;
            case 12:
               bytesPerComponent = 2;
               bitDepth_ = 12;
               ret=DEVICE_OK;
            break;
            case 14:
               bytesPerComponent = 2;
               bitDepth_ = 14;
               ret=DEVICE_OK;
            break;
            case 16:
               bytesPerComponent = 2;
               bitDepth_ = 16;
               ret=DEVICE_OK;
            break;
            case 32:
               bytesPerComponent = 4;
               bitDepth_ = 32; 
               ret=DEVICE_OK;
            break;
            default: 
               // on error switch to default pixel type
               bytesPerComponent = 1;

               pProp->Set((long)8);
               bitDepth_ = 8;
               ret = ERR_UNKNOWN_MODE;
            break;
         }
         char buf[MM::MaxStrLength];
         GetProperty(MM::g_Keyword_PixelType, buf);
         std::string pixelType(buf);
         unsigned int bytesPerPixel = 1;
         

         // automagickally change pixel type when bit depth exceeds possible value
         if (pixelType.compare(g_PixelType_8bit) == 0)
         {
            if( 2 == bytesPerComponent)
            {
               SetProperty(MM::g_Keyword_PixelType, g_PixelType_16bit);
               bytesPerPixel = 2;
            }
            else if ( 4 == bytesPerComponent)
            {
               SetProperty(MM::g_Keyword_PixelType, g_PixelType_32bit);
               bytesPerPixel = 4;

            }else
            {
               bytesPerPixel = 1;
            }
         }
         else if (pixelType.compare(g_PixelType_16bit) == 0)
         {
            bytesPerPixel = 2;
         }
         else if ( pixelType.compare(g_PixelType_32bitRGB) == 0)
         {
            bytesPerPixel = 4;
         }
         else if ( pixelType.compare(g_PixelType_32bit) == 0)
         {
            bytesPerPixel = 4;
         }
         else if ( pixelType.compare(g_PixelType_64bitRGB) == 0)
         {
            bytesPerPixel = 8;
         }
         img_.Resize(img_.Width(), img_.Height(), bytesPerPixel);

      } break;
   case MM::BeforeGet:
      {
         pProp->Set((long)bitDepth_);
         ret=DEVICE_OK;
      } break;
   default:
      break;
   }
   return ret; 
}
/**
* Handles "ReadoutTime" property.
*/
int CDemoCamera::OnReadoutTime(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      double readoutMs;
      pProp->Get(readoutMs);

      readoutUs_ = readoutMs * 1000.0;
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(readoutUs_ / 1000.0);
   }

   return DEVICE_OK;
}

int CDemoCamera::OnDropPixels(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
      dropPixels_ = (0==tvalue)?false:true;
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(dropPixels_?1L:0L);
   }

   return DEVICE_OK;
}

int CDemoCamera::OnFastImage(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
      fastImage_ = (0==tvalue)?false:true;
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(fastImage_?1L:0L);
   }

   return DEVICE_OK;
}

int CDemoCamera::OnSaturatePixels(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
      saturatePixels_ = (0==tvalue)?false:true;
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(saturatePixels_?1L:0L);
   }

   return DEVICE_OK;
}

int CDemoCamera::OnFractionOfPixelsToDropOrSaturate(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      double tvalue = 0;
      pProp->Get(tvalue);
      fractionOfPixelsToDropOrSaturate_ = tvalue;
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(fractionOfPixelsToDropOrSaturate_);
   }

   return DEVICE_OK;
}

int CDemoCamera::OnShouldRotateImages(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
      shouldRotateImages_ = (tvalue != 0);
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set((long) shouldRotateImages_);
   }

   return DEVICE_OK;
}

int CDemoCamera::OnShouldDisplayImageNumber(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      long tvalue = 0;
      pProp->Get(tvalue);
      shouldDisplayImageNumber_ = (tvalue != 0);
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set((long) shouldDisplayImageNumber_);
   }

   return DEVICE_OK;
}

int CDemoCamera::OnStripeWidth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::AfterSet)
   {
      pProp->Get(stripeWidth_);
   }
   else if (eAct == MM::BeforeGet)
   {
      pProp->Set(stripeWidth_);
   }

   return DEVICE_OK;
}
/*
* Handles "ScanMode" property.
* Changes allowed Binning values to test whether the UI updates properly
*/
int CDemoCamera::OnScanMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{ 
   if (eAct == MM::AfterSet) {
      pProp->Get(scanMode_);
      SetAllowedBinning();
      if (initialized_) {
         int ret = OnPropertiesChanged();
         if (ret != DEVICE_OK)
            return ret;
      }
   } else if (eAct == MM::BeforeGet) {
      LogMessage("Reading property ScanMode", true);
      pProp->Set(scanMode_);
   }
   return DEVICE_OK;
}




int CDemoCamera::OnCameraCCDXSize(MM::PropertyBase* pProp , MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(cameraCCDXSize_);
   }
   else if (eAct == MM::AfterSet)
   {
      long value;
      pProp->Get(value);
      if ( (value < 16) || (33000 < value))
         return DEVICE_ERR;  // invalid image size
      if( value != cameraCCDXSize_)
      {
         cameraCCDXSize_ = value;
         img_.Resize(cameraCCDXSize_/binSize_, cameraCCDYSize_/binSize_);
      }
   }
   return DEVICE_OK;

}

int CDemoCamera::OnCameraCCDYSize(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(cameraCCDYSize_);
   }
   else if (eAct == MM::AfterSet)
   {
      long value;
      pProp->Get(value);
      if ( (value < 16) || (33000 < value))
         return DEVICE_ERR;  // invalid image size
      if( value != cameraCCDYSize_)
      {
         cameraCCDYSize_ = value;
         img_.Resize(cameraCCDXSize_/binSize_, cameraCCDYSize_/binSize_);
      }
   }
   return DEVICE_OK;

}

int CDemoCamera::OnTriggerDevice(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(triggerDevice_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(triggerDevice_);
   }
   return DEVICE_OK;
}


int CDemoCamera::OnCCDTemp(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(ccdT_);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(ccdT_);
   }
   return DEVICE_OK;
}

int CDemoCamera::OnIsSequenceable(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   std::string val = "Yes";
   if (eAct == MM::BeforeGet)
   {
      if (!isSequenceable_) 
      {
         val = "No";
      }
      pProp->Set(val.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      isSequenceable_ = false;
      pProp->Get(val);
      if (val == "Yes") 
      {
         isSequenceable_ = true;
      }
   }

   return DEVICE_OK;
}


int CDemoCamera::OnMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   std::string val;
   if (eAct == MM::BeforeGet)
   {
      if (mode_ == 0)
      {
         val = g_Sine_Wave;
      } else 
      {
         val = g_Norm_Noise;
      }
      pProp->Set(val.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(val);
      if (val == g_Sine_Wave) 
      {
         mode_ = 0;
      } else
      {
         mode_ = 1;
      }
   }
   return DEVICE_OK;
}


int CDemoCamera::OnCrash(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   AddAllowedValue("SimulateCrash", "");
   AddAllowedValue("SimulateCrash", "Dereference Null Pointer");
   AddAllowedValue("SimulateCrash", "Divide by Zero");
   if (eAct == MM::BeforeGet)
   {
      pProp->Set("");
   }
   else if (eAct == MM::AfterSet)
   {
      std::string choice;
      pProp->Get(choice);
      if (choice == "Dereference Null Pointer")
      {
         int* p = 0;
         volatile int i = *p;
      }
      else if (choice == "Divide by Zero")
      {
         volatile int i = 1, j = 0, k;
         k = i / j;
      }
   }
   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Private CDemoCamera methods
///////////////////////////////////////////////////////////////////////////////

/**
* Sync internal image buffer size to the chosen property values.
*/
int CDemoCamera::ResizeImageBuffer()
{
   char buf[MM::MaxStrLength];
   //int ret = GetProperty(MM::g_Keyword_Binning, buf);
   //if (ret != DEVICE_OK)
   //   return ret;
   //binSize_ = atol(buf);

   int ret = GetProperty(MM::g_Keyword_PixelType, buf);
   if (ret != DEVICE_OK)
      return ret;

   std::string pixelType(buf);
   int byteDepth = 0;

   if (pixelType.compare(g_PixelType_8bit) == 0)
   {
      byteDepth = 1;
   }
   else if (pixelType.compare(g_PixelType_16bit) == 0)
   {
      byteDepth = 2;
   }
   else if ( pixelType.compare(g_PixelType_32bitRGB) == 0)
   {
      byteDepth = 4;
   }
   else if ( pixelType.compare(g_PixelType_32bit) == 0)
   {
      byteDepth = 4;
   }
   else if ( pixelType.compare(g_PixelType_64bitRGB) == 0)
   {
      byteDepth = 8;
   }

   img_.Resize(cameraCCDXSize_/binSize_, cameraCCDYSize_/binSize_, byteDepth);
   return DEVICE_OK;
}

void CDemoCamera::GenerateEmptyImage(ImgBuffer& img)
{
   MMThreadGuard g(imgPixelsLock_);
   if (img.Height() == 0 || img.Width() == 0 || img.Depth() == 0)
      return;
   unsigned char* pBuf = const_cast<unsigned char*>(img.GetPixels());
   memset(pBuf, 0, img.Height()*img.Width()*img.Depth());
}



/**
* Generates an image.
*
* Options:
* 1. a spatial sine wave.
* 2. Gaussian noise
*/
void CDemoCamera::GenerateSyntheticImage(ImgBuffer& img, double exp)
{ 
  
   MMThreadGuard g(imgPixelsLock_);

   if (mode_ == 1)
   {
      double max = 1 << GetBitDepth();
      int mean = 10;
      if (max > 256)
      {
         mean = 100;
      }
      AddBackgroundAndNoise(img, mean, 3.0);
      return;
   }

   //std::string pixelType;
   char buf[MM::MaxStrLength];
   GetProperty(MM::g_Keyword_PixelType, buf);
   std::string pixelType(buf);

   if (img.Height() == 0 || img.Width() == 0 || img.Depth() == 0)
      return;

   double lSinePeriod = 3.14159265358979 * stripeWidth_;
   unsigned imgWidth = img.Width();
   unsigned int* rawBuf = (unsigned int*) img.GetPixelsRW();
   double maxDrawnVal = 0;
   long lPeriod = (long) imgWidth / 2;
   double dLinePhase = 0.0;
   const double dAmp = exp;
   double cLinePhaseInc = 2.0 * lSinePeriod / 4.0 / img.Height();
   if (shouldRotateImages_) {
      // Adjust the angle of the sin wave pattern based on how many images
      // we've taken, to increase the period (i.e. time between repeat images).
      cLinePhaseInc *= (((int) dPhase_ / 6) % 24) - 12;
   }

   static bool debugRGB = false;
#ifdef TIFFDEMO
   debugRGB = true;
#endif
   static  unsigned char* pDebug  = NULL;
   static unsigned long dbgBufferSize = 0;
   static long iseq = 1;

 

   // for integer images: bitDepth_ is 8, 10, 12, 16 i.e. it is depth per component
   long maxValue = (1L << bitDepth_)-1;

   long pixelsToDrop = 0;
   if( dropPixels_)
      pixelsToDrop = (long)(0.5 + fractionOfPixelsToDropOrSaturate_*img.Height()*imgWidth);
   long pixelsToSaturate = 0;
   if( saturatePixels_)
      pixelsToSaturate = (long)(0.5 + fractionOfPixelsToDropOrSaturate_*img.Height()*imgWidth);

   unsigned j, k;
   if (pixelType.compare(g_PixelType_8bit) == 0)
   {
      double pedestal = 127 * exp / 100.0 * GetBinning() * GetBinning();
      unsigned char* pBuf = const_cast<unsigned char*>(img.GetPixels());
      for (j=0; j<img.Height(); j++)
      {
         for (k=0; k<imgWidth; k++)
         {
            long lIndex = imgWidth*j + k;
            unsigned char val = (unsigned char) (g_IntensityFactor_ * min(255.0, (pedestal + dAmp * sin(dPhase_ + dLinePhase + (2.0 * lSinePeriod * k) / lPeriod))));
            if (val > maxDrawnVal) {
                maxDrawnVal = val;
            }
            *(pBuf + lIndex) = val;
         }
         dLinePhase += cLinePhaseInc;
      }
      for(int snoise = 0; snoise < pixelsToSaturate; ++snoise)
      {
         j = (unsigned)( (double)(img.Height()-1)*(double)rand()/(double)RAND_MAX);
         k = (unsigned)( (double)(imgWidth-1)*(double)rand()/(double)RAND_MAX);
         *(pBuf + imgWidth*j + k) = (unsigned char)maxValue;
      }
      int pnoise;
      for(pnoise = 0; pnoise < pixelsToDrop; ++pnoise)
      {
         j = (unsigned)( (double)(img.Height()-1)*(double)rand()/(double)RAND_MAX);
         k = (unsigned)( (double)(imgWidth-1)*(double)rand()/(double)RAND_MAX);
         *(pBuf + imgWidth*j + k) = 0;
      }

   }
   else if (pixelType.compare(g_PixelType_16bit) == 0)
   {
      double pedestal = maxValue/2 * exp / 100.0 * GetBinning() * GetBinning();
      double dAmp16 = dAmp * maxValue/255.0; // scale to behave like 8-bit
      unsigned short* pBuf = (unsigned short*) const_cast<unsigned char*>(img.GetPixels());
      for (j=0; j<img.Height(); j++)
      {
         for (k=0; k<imgWidth; k++)
         {
            long lIndex = imgWidth*j + k;
            unsigned short val = (unsigned short) (g_IntensityFactor_ * min((double)maxValue, pedestal + dAmp16 * sin(dPhase_ + dLinePhase + (2.0 * lSinePeriod * k) / lPeriod)));
            if (val > maxDrawnVal) {
                maxDrawnVal = val;
            }
            *(pBuf + lIndex) = val;
         }
         dLinePhase += cLinePhaseInc;
      }         
      for(int snoise = 0; snoise < pixelsToSaturate; ++snoise)
      {
         j = (unsigned)(0.5 + (double)img.Height()*(double)rand()/(double)RAND_MAX);
         k = (unsigned)(0.5 + (double)imgWidth*(double)rand()/(double)RAND_MAX);
         *(pBuf + imgWidth*j + k) = (unsigned short)maxValue;
      }
      int pnoise;
      for(pnoise = 0; pnoise < pixelsToDrop; ++pnoise)
      {
         j = (unsigned)(0.5 + (double)img.Height()*(double)rand()/(double)RAND_MAX);
         k = (unsigned)(0.5 + (double)imgWidth*(double)rand()/(double)RAND_MAX);
         *(pBuf + imgWidth*j + k) = 0;
      }
   
   }
   else if (pixelType.compare(g_PixelType_32bit) == 0)
   {
      double pedestal = 127 * exp / 100.0 * GetBinning() * GetBinning();
      float* pBuf = (float*) const_cast<unsigned char*>(img.GetPixels());
      float saturatedValue = 255.;
      memset(pBuf, 0, img.Height()*imgWidth*4);
      // static unsigned int j2;
      for (j=0; j<img.Height(); j++)
      {
         for (k=0; k<imgWidth; k++)
         {
            long lIndex = imgWidth*j + k;
            double value =  (g_IntensityFactor_ * min(255.0, (pedestal + dAmp * sin(dPhase_ + dLinePhase + (2.0 * lSinePeriod * k) / lPeriod))));
            if (value > maxDrawnVal) {
                maxDrawnVal = value;
            }
            *(pBuf + lIndex) = (float) value;
            if( 0 == lIndex)
            {
               std::ostringstream os;
               os << " first pixel is " << (float)value;
               LogMessage(os.str().c_str(), true);

            }
         }
         dLinePhase += cLinePhaseInc;
      }

      for(int snoise = 0; snoise < pixelsToSaturate; ++snoise)
      {
         j = (unsigned)(0.5 + (double)img.Height()*(double)rand()/(double)RAND_MAX);
         k = (unsigned)(0.5 + (double)imgWidth*(double)rand()/(double)RAND_MAX);
         *(pBuf + imgWidth*j + k) = saturatedValue;
      }
      int pnoise;
      for(pnoise = 0; pnoise < pixelsToDrop; ++pnoise)
      {
         j = (unsigned)(0.5 + (double)img.Height()*(double)rand()/(double)RAND_MAX);
         k = (unsigned)(0.5 + (double)imgWidth*(double)rand()/(double)RAND_MAX);
         *(pBuf + imgWidth*j + k) = 0;
      }
   
   }
   else if (pixelType.compare(g_PixelType_32bitRGB) == 0)
   {
      double pedestal = 127 * exp / 100.0;
      unsigned int * pBuf = (unsigned int*) rawBuf;

      unsigned char* pTmpBuffer = NULL;

      if(debugRGB)
      {
         const unsigned long bfsize = img.Height() * imgWidth * 3;
         if(  bfsize != dbgBufferSize)
         {
            if (NULL != pDebug)
            {
               free(pDebug);
               pDebug = NULL;
            }
            pDebug = (unsigned char*)malloc( bfsize);
            if( NULL != pDebug)
            {
               dbgBufferSize = bfsize;
            }
         }
      }

      // only perform the debug operations if pTmpbuffer is not 0
      pTmpBuffer = pDebug;
      unsigned char* pTmp2 = pTmpBuffer;
      if( NULL!= pTmpBuffer)
         memset( pTmpBuffer, 0, img.Height() * imgWidth * 3);

      for (j=0; j<img.Height(); j++)
      {
         unsigned char theBytes[4];
         for (k=0; k<imgWidth; k++)
         {
            long lIndex = imgWidth*j + k;
            unsigned char value0 =   (unsigned char) min(255.0, (pedestal + dAmp * sin(dPhase_ + dLinePhase + (2.0 * lSinePeriod * k) / lPeriod)));
            theBytes[0] = value0;
            if( NULL != pTmpBuffer)
               pTmp2[2] = value0;
            unsigned char value1 =   (unsigned char) min(255.0, (pedestal + dAmp * sin(dPhase_ + dLinePhase*2 + (2.0 * lSinePeriod * k) / lPeriod)));
            theBytes[1] = value1;
            if( NULL != pTmpBuffer)
               pTmp2[1] = value1;
            unsigned char value2 = (unsigned char) min(255.0, (pedestal + dAmp * sin(dPhase_ + dLinePhase*4 + (2.0 * lSinePeriod * k) / lPeriod)));
            theBytes[2] = value2;

            if( NULL != pTmpBuffer){
               pTmp2[0] = value2;
               pTmp2+=3;
            }
            theBytes[3] = 0;
            unsigned long tvalue = *(unsigned long*)(&theBytes[0]);
            if (tvalue > maxDrawnVal) {
                maxDrawnVal = tvalue;
            }
            *(pBuf + lIndex) =  tvalue ;  //value0+(value1<<8)+(value2<<16);
         }
         dLinePhase += cLinePhaseInc;
      }


      // ImageJ's AWT images are loaded with a Direct Color processor which expects BGRA, that's why we swapped the Blue and Red components in the generator above.
      if(NULL != pTmpBuffer)
      {
         // write the compact debug image...
         char ctmp[12];
         snprintf(ctmp,12,"%ld",iseq++);
         //writeCompactTiffRGB(imgWidth, img.Height(), pTmpBuffer, ("democamera" + std::string(ctmp)).c_str());
      }

   }

   // generate an RGB image with bitDepth_ bits in each color
   else if (pixelType.compare(g_PixelType_64bitRGB) == 0)
   {
      double pedestal = maxValue/2 * exp / 100.0 * GetBinning() * GetBinning();
      double dAmp16 = dAmp * maxValue/255.0; // scale to behave like 8-bit
      
      double maxPixelValue = (1<<(bitDepth_))-1;
      unsigned long long * pBuf = (unsigned long long*) rawBuf;
      for (j=0; j<img.Height(); j++)
      {
         for (k=0; k<imgWidth; k++)
         {
            long lIndex = imgWidth*j + k;
            unsigned long long value0 = (unsigned short) min(maxPixelValue, (pedestal + dAmp16 * sin(dPhase_ + dLinePhase + (2.0 * lSinePeriod * k) / lPeriod)));
            unsigned long long value1 = (unsigned short) min(maxPixelValue, (pedestal + dAmp16 * sin(dPhase_ + dLinePhase*2 + (2.0 * lSinePeriod * k) / lPeriod)));
            unsigned long long value2 = (unsigned short) min(maxPixelValue, (pedestal + dAmp16 * sin(dPhase_ + dLinePhase*4 + (2.0 * lSinePeriod * k) / lPeriod)));
            unsigned long long tval = value0+(value1<<16)+(value2<<32);
            if (tval > maxDrawnVal) {
                maxDrawnVal = static_cast<double>(tval);
            }
            *(pBuf + lIndex) = tval;
            }
         dLinePhase += cLinePhaseInc;
      }
   }

    
   dPhase_ += lSinePeriod / 4.;
}


void CDemoCamera::TestResourceLocking(const bool recurse)
{
   if(recurse)
      TestResourceLocking(false);
}

/**
* Generate an image with offsetplus noise
*/
void CDemoCamera::AddBackgroundAndNoise(ImgBuffer& img, double mean, double stdDev)
{ 
   char buf[MM::MaxStrLength];
   GetProperty(MM::g_Keyword_PixelType, buf);
   std::string pixelType(buf);

   int maxValue = 1 << GetBitDepth();
   long nrPixels = img.Width() * img.Height();
   if (pixelType.compare(g_PixelType_8bit) == 0)
   {
      unsigned char* pBuf = (unsigned char*) const_cast<unsigned char*>(img.GetPixels());
      for (long i = 0; i < nrPixels; i++) 
      {
         double value = GaussDistributedValue(mean, stdDev);
         if (value < 0) 
         {
            value = 0;
         }
         else if (value > maxValue)
         {
            value = maxValue;
         }
         *(pBuf + i) = (unsigned char) value;
      }
   }
   else if (pixelType.compare(g_PixelType_16bit) == 0)
   {
      unsigned short* pBuf = (unsigned short*) const_cast<unsigned char*>(img.GetPixels());
      for (long i = 0; i < nrPixels; i++) 
      {
         double value = GaussDistributedValue(mean, stdDev);
         if (value < 0) 
         {
            value = 0;
         }
         else if (value > maxValue)
         {
            value = maxValue;
         }
         *(pBuf + i) = (unsigned short) value;
      }
   }
}

/**
 * Uses Marsaglia polar method to generate Gaussian distributed value.  
 * Then distributes this around mean with the desired std
 */
double CDemoCamera::GaussDistributedValue(double mean, double std)
{
   double s = 2;
   double u;
   double v;
   double halfRandMax = RAND_MAX / 2;
   while (s >= 1 || s <= 0) 
   {
      // get random values between -1 and 1
      u = rand() / halfRandMax - 1.0;
      v = rand() / halfRandMax - 1.0;
      s = u * u + v * v;
   }
   double tmp = sqrt( -2 * log(s) / s);
   double x = u * tmp;

   return mean + std * x;
}

int TransposeProcessor::Initialize()
{
   if( NULL != this->pTemp_)
   {
      free(pTemp_);
      pTemp_ = NULL;
      this->tempSize_ = 0;
   }
    CPropertyAction* pAct = new CPropertyAction (this, &TransposeProcessor::OnInPlaceAlgorithm);
   (void)CreateIntegerProperty("InPlaceAlgorithm", 0, false, pAct);
   return DEVICE_OK;
}

   // action interface
   // ----------------
int TransposeProcessor::OnInPlaceAlgorithm(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(this->inPlace_?1L:0L);
   }
   else if (eAct == MM::AfterSet)
   {
      long ltmp;
      pProp->Get(ltmp);
      inPlace_ = (0==ltmp?false:true);
   }

   return DEVICE_OK;
}


int TransposeProcessor::Process(unsigned char *pBuffer, unsigned int width, unsigned int height, unsigned int byteDepth)
{
   int ret = DEVICE_OK;
   // 
   if( width != height)
      return DEVICE_NOT_SUPPORTED; // problem with tranposing non-square images is that the image buffer
   // will need to be modified by the image processor.
   if(busy_)
      return DEVICE_ERR;
 
   busy_ = true;

   if( inPlace_)
   {
      if(  sizeof(unsigned char) == byteDepth)
      {
         TransposeSquareInPlace( (unsigned char*)pBuffer, width);
      }
      else if( sizeof(unsigned short) == byteDepth)
      {
         TransposeSquareInPlace( (unsigned short*)pBuffer, width);
      }
      else if( sizeof(unsigned long) == byteDepth)
      {
         TransposeSquareInPlace( (unsigned long*)pBuffer, width);
      }
      else if( sizeof(unsigned long long) == byteDepth)
      {
         TransposeSquareInPlace( (unsigned long long*)pBuffer, width);
      }
      else 
      {
         ret = DEVICE_NOT_SUPPORTED;
      }
   }
   else
   {
      if( sizeof(unsigned char) == byteDepth)
      {
         ret = TransposeRectangleOutOfPlace( (unsigned char*)pBuffer, width, height);
      }
      else if( sizeof(unsigned short) == byteDepth)
      {
         ret = TransposeRectangleOutOfPlace( (unsigned short*)pBuffer, width, height);
      }
      else if( sizeof(unsigned long) == byteDepth)
      {
         ret = TransposeRectangleOutOfPlace( (unsigned long*)pBuffer, width, height);
      }
      else if( sizeof(unsigned long long) == byteDepth)
      {
         ret =  TransposeRectangleOutOfPlace( (unsigned long long*)pBuffer, width, height);
      }
      else
      {
         ret =  DEVICE_NOT_SUPPORTED;
      }
   }
   busy_ = false;

   return ret;
}
