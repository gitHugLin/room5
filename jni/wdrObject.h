#ifndef WDR_WDROBJECT_H
#define WDR_WDROBJECT_H


#include "iostream"
#include "vector"
#include "string"
#include "log.h"
#include "opencv2/opencv.hpp"
#include <stdio.h>
#include <arm_neon.h>

using namespace cv;
using namespace std;

namespace wdrCpuMode{

typedef int            INT32;
typedef unsigned int   UINT32;
typedef short          INT16;
typedef unsigned short UINT16;
typedef char           INT8;
typedef unsigned char  UINT8;
typedef void           VOID;
typedef unsigned long long  LONG;
typedef unsigned char UCHAR;
//typedef unsigned int size_t;

typedef struct _wdrPoint
{
	UINT8 x;
	UINT8 y;
}wdrPoint;

#define wdrMin(a,b)                        ((a) < (b) ? (a):(b))
#define wdrMax(a,b)                        ((a) < (b) ? (b):(a))

class wdrObject{
public:
    wdrObject();
    ~wdrObject();
private:
    Mat mSrcImage;
    //Mat *mIntegralImage;
	UINT32* mIntegralImage;
	UINT32* mColumnSum;
    Mat mGrayChannel;
    Mat mDstImage;

private:
	float mToneMapLut[256][256];
private:
    UINT32 mWidth;
    UINT32 mHeight;
    INT32 mBlkSize;
    float mGainOffset;
private:
	void fastIntegral();
	void toneMapping();
	void neon_integral_image(const uint8_t *sourceImage, uint32_t *integralImage,
	                         size_t width, size_t height);
	void getInteImageNeon();
public:
    void process();
    bool loadData(string imagePath,bool pgm = false);

private:

};


}

#endif
