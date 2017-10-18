///////////////////////////////////////////////////////////////////////////////

#ifndef _DEMOCAMERA_H_
#define _DEMOCAMERA_H_

#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/ImgBuffer.h"
#include "../../MMDevice/DeviceThreads.h"
#include <string>
#include <map>
#include <algorithm>
#include <stdint.h>

//////////////////////////////////////////////////////////////////////////////
// Error codes
//
#define ERR_UNKNOWN_MODE         102
#define ERR_UNKNOWN_POSITION     103
#define ERR_IN_SEQUENCE          104
#define ERR_SEQUENCE_INACTIVE    105
#define ERR_STAGE_MOVING         106
#define HUB_NOT_AVAILABLE        107

class MySequenceThread;

class CSimulatingCamera : public CCameraBase<CSimulatingCamera>
{
public:
   CSimulatingCamera();
   ~CSimulatingCamera();

   // MMDevice API
   // ------------
   int Initialize();
   int Shutdown();

   void GetName(char* name) const;

   // MMCamera API
   // ------------
   int SnapImage();
   const unsigned char* GetImageBuffer();
   unsigned GetImageWidth() const;
   unsigned GetImageHeight() const;
   unsigned GetImageBytesPerPixel() const;
   unsigned GetBitDepth() const;
   long GetImageBufferSize() const;
   double GetExposure() const;
   void SetExposure(double exp);
   int SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize);
   int GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize);
   int ClearROI();
   int PrepareSequenceAcqusition() { return DEVICE_OK; }
   int StartSequenceAcquisition(double interval);
   int StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow);
   int StopSequenceAcquisition();
   int InsertImage();
   int RunSequenceOnThread(MM::MMTime startTime);
   bool IsCapturing();
   void OnThreadExiting() throw();
   double GetNominalPixelSizeUm() const {return nominalPixelSizeUm_;}
   double GetPixelSizeUm() const {return nominalPixelSizeUm_ * GetBinning();}
   int GetBinning() const;
   int SetBinning(int bS);

   int IsExposureSequenceable(bool& isSequenceable) const;
   int GetExposureSequenceMaxLength(long& nrEvents) { nrEvents = 0; return DEVICE_OK; };
   int StartExposureSequence() { return DEVICE_ERR; }
   int StopExposureSequence() { return DEVICE_ERR; }
   int ClearExposureSequence() { return DEVICE_ERR; }
   int AddToExposureSequence(double exposureTime_ms) { return DEVICE_ERR; }
   int SendExposureSequence() { return DEVICE_ERR; }

   unsigned  GetNumberOfComponents() const { return nComponents_;};

   // action interface
   // ----------------
   int OnURL(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnChannelDevice(MM::PropertyBase* pProp, MM::ActionType eAct);

   int OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnBitDepth(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnCameraCCDXSize(MM::PropertyBase* , MM::ActionType );
   int OnCameraCCDYSize(MM::PropertyBase* , MM::ActionType );


private:
   bool FetchImageFromUrl(ImgBuffer& img);
   int ResizeImageBuffer();

   static const double nominalPixelSizeUm_;

   std::string url_;
   std::string channelDevice_;

   ImgBuffer img_;
   bool initialized_;
   double readoutUs_;
   MM::MMTime readoutStartTime_;
   int bitDepth_;
   unsigned roiX_;
   unsigned roiY_;
   MM::MMTime sequenceStartTime_;
   long imageCounter_;
   long binSize_;
   long cameraCCDXSize_;
   long cameraCCDYSize_;
   std::string triggerDevice_;

   bool stopOnOverflow_;


   MMThreadLock imgPixelsLock_;
   friend class MySequenceThread;
   int nComponents_;
   MySequenceThread * thd_;
};



class MySequenceThread : public MMDeviceThreadBase
{
   friend class CSimulatingCamera;
   enum { default_numImages=1, default_intervalMS = 100 };
   public:
      MySequenceThread(CSimulatingCamera* pCam);
      ~MySequenceThread();
      void Stop();
      void Start(long numImages, double intervalMs);
      bool IsStopped();
      void Suspend();
      bool IsSuspended();
      void Resume();
      double GetIntervalMs(){return intervalMs_;}
      void SetLength(long images) {numImages_ = images;}
      long GetLength() const {return numImages_;}
      long GetImageCounter(){return imageCounter_;}
      MM::MMTime GetStartTime(){return startTime_;}
      MM::MMTime GetActualDuration(){return actualDuration_;}
   private:
      int svc(void) throw();
      double intervalMs_;
      long numImages_;
      long imageCounter_;
      bool stop_;
      bool suspend_;
      CSimulatingCamera* camera_;
      MM::MMTime startTime_;
      MM::MMTime actualDuration_;
      MM::MMTime lastFrameTime_;
      MMThreadLock stopLock_;
      MMThreadLock suspendLock_;
};

#endif //_DEMOCAMERA_H_

