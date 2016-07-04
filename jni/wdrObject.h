#ifndef WDR_WDROBJECT_H
#define WDR_WDROBJECT_H


#include "iostream"
#include "vector"
#include "string"
#include "log.h"
#include "opencv2/opencv.hpp"

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
    Mat *mIntegralImage;
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
public:
    void process();
    bool loadData(string imagePath,bool pgm = false);

private:

};


}

#endif
