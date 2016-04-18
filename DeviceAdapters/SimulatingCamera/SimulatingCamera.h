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

class CDemoCamera : public CCameraBase<CDemoCamera>  
{
public:
   CDemoCamera();
   ~CDemoCamera();
  
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
   int GetExposureSequenceMaxLength(long& nrEvents) const;
   int StartExposureSequence();
   int StopExposureSequence();
   int ClearExposureSequence();
   int AddToExposureSequence(double exposureTime_ms);
   int SendExposureSequence() const;

   unsigned  GetNumberOfComponents() const { return nComponents_;};

   // action interface
   // ----------------
   int OnMaxExposure(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTestProperty(MM::PropertyBase* pProp, MM::ActionType eAct, long);
   int OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnPixelType(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnBitDepth(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnReadoutTime(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnScanMode(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnErrorSimulation(MM::PropertyBase* , MM::ActionType );
   int OnCameraCCDXSize(MM::PropertyBase* , MM::ActionType );
   int OnCameraCCDYSize(MM::PropertyBase* , MM::ActionType );
   int OnTriggerDevice(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnDropPixels(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnFastImage(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnSaturatePixels(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnFractionOfPixelsToDropOrSaturate(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnShouldRotateImages(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnShouldDisplayImageNumber(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnStripeWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnCCDTemp(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnIsSequenceable(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnMode(MM::PropertyBase* pProp, MM::ActionType eAct);
   int OnCrash(MM::PropertyBase* pProp, MM::ActionType eAct);

   // Special public DemoCamera methods
   void AddBackgroundAndNoise(ImgBuffer& img, double mean, double stdDev);
   // this function replace normal_distribution in C++11
   double GaussDistributedValue(double mean, double std);

private:
   int SetAllowedBinning();
   void TestResourceLocking(const bool);
   void GenerateEmptyImage(ImgBuffer& img);
   void GenerateSyntheticImage(ImgBuffer& img, double exp);
   int ResizeImageBuffer();

   static const double nominalPixelSizeUm_;

   double exposureMaximum_;
   double dPhase_;
   ImgBuffer img_;
   bool busy_;
   bool stopOnOverFlow_;
   bool initialized_;
   double readoutUs_;
   MM::MMTime readoutStartTime_;
   long scanMode_;
   int bitDepth_;
   unsigned roiX_;
   unsigned roiY_;
   MM::MMTime sequenceStartTime_;
   bool isSequenceable_;
   long sequenceMaxLength_;
   bool sequenceRunning_;
   unsigned long sequenceIndex_;
   double GetSequenceExposure();
   std::vector<double> exposureSequence_;
   long imageCounter_;
   long binSize_;
   long cameraCCDXSize_;
   long cameraCCDYSize_;
   double ccdT_;
   std::string triggerDevice_;

   bool stopOnOverflow_;

   bool dropPixels_;
   bool fastImage_;
   bool saturatePixels_;
   double fractionOfPixelsToDropOrSaturate_;
   bool shouldRotateImages_;
   bool shouldDisplayImageNumber_;
   double stripeWidth_;

   double testProperty_[10];
   MMThreadLock imgPixelsLock_;
   friend class MySequenceThread;
   int nComponents_;
   MySequenceThread * thd_;
   int mode_;
};



class MySequenceThread : public MMDeviceThreadBase
{
   friend class CDemoCamera;
   enum { default_numImages=1, default_intervalMS = 100 };
   public:
      MySequenceThread(CDemoCamera* pCam);
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
      CDemoCamera* camera_;                                                     
      MM::MMTime startTime_;                                                    
      MM::MMTime actualDuration_;                                               
      MM::MMTime lastFrameTime_;                                                
      MMThreadLock stopLock_;                                                   
      MMThreadLock suspendLock_;                                                
}; 


class TransposeProcessor : public CImageProcessorBase<TransposeProcessor>
{
public:
   TransposeProcessor () : inPlace_ (false), pTemp_(NULL), tempSize_(0), busy_(false)
   {
      // parent ID display
      CreateHubIDProperty();
   }
   ~TransposeProcessor () {if( NULL!= pTemp_) free(pTemp_); tempSize_=0;  }

   int Shutdown() {return DEVICE_OK;}
   void GetName(char* name) const {strcpy(name,"TransposeProcessor");}

   int Initialize();

   bool Busy(void) { return busy_;};

    // really primative image transpose algorithm which will work fine for non-square images... 
   template <typename PixelType>
   int TransposeRectangleOutOfPlace(PixelType* pI, unsigned int width, unsigned int height)
   {
      int ret = DEVICE_OK;
      unsigned long tsize = width*height*sizeof(PixelType);
      if( this->tempSize_ != tsize)
      {
         if( NULL != this->pTemp_)
         {
            free(pTemp_);
            pTemp_ = NULL;
         }
         pTemp_ = (PixelType *)malloc(tsize);
      }
      if( NULL != pTemp_)
      {
         PixelType* pTmpImage = (PixelType *) pTemp_;
         tempSize_ = tsize;
         for( unsigned long ix = 0; ix < width; ++ix)
         {
            for( unsigned long iy = 0; iy < height; ++iy)
            {
               pTmpImage[iy + ix*width] = pI[ ix + iy*height];
            }
         }
         memcpy( pI, pTmpImage, tsize);
      }
      else
      {
         ret = DEVICE_ERR;
      }

      return ret;
   }

   
   template <typename PixelType>
   void TransposeSquareInPlace(PixelType* pI, unsigned int dim)
   { 
      PixelType tmp;
      for( unsigned long ix = 0; ix < dim; ++ix)
      {
         for( unsigned long iy = ix; iy < dim; ++iy)
         {
            tmp = pI[iy*dim + ix];
            pI[iy*dim +ix] = pI[ix*dim + iy];
            pI[ix*dim +iy] = tmp; 
         }
      }

      return;
   }

   int Process(unsigned char* buffer, unsigned width, unsigned height, unsigned byteDepth);

   // action interface
   // ----------------
   int OnInPlaceAlgorithm(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
   bool inPlace_;
   void* pTemp_;
   unsigned long tempSize_;
   bool busy_;
};

#endif //_DEMOCAMERA_H_
