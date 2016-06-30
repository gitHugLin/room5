//
// Created by linqi on 16-6-8.
//

#ifndef WDR_WDRBASE_H
#define WDR_WDRBASE_H


#include "iostream"
#include "vector"
#include "string"
#include "log.h"
#include "opencv2/opencv.hpp"

using namespace cv;
using namespace std;

namespace wdr{


#define MAX_FILENAME_LEN                 128

#define MAX_UINT10                       ((1 << 10)-1)
#define MAX_UINT8                        ((1 << 8)-1)

#define CLIP(a, min, max)			    (((a) < (min)) ? (min) : (((a) > (max)) ? (max) : (a)))
#define ABS(A)                          ( (A) > 0 ? (A) : -(A) )
#define MIN(a,b)                        ((a) <= (b) ? (a):(b))
#define MAX(a,b)                        ((a) >= (b) ? (a):(b))
#define SWAP(_T_,A,B)                   { _T_ tmp = (A); (A) = (B); (B) = tmp; }

#define DIV_ROUND(x, n)                 (((x) + (n)/2)/(n))
#define ROUND_F(x)                      (int)((x)+((x) > 0 ? 0.5 : -0.5))
#define ROUND_I(_T_, x, scale_f)        (int)(((_T_)(x) + (1<<((scale_f)-1)))>>(scale_f))
#define ADD1(x)                         ((x)+1)
#define BIT_SET(val, i, bin)	        ((val) = (val) | ((bin) << (i)))
#define BIT_TEST(val, i)		        (((val) >> (i))&0x1)

#define BIT_MASK(i)                     ((1 << (i)) - 1)
#define BITS_EXTRACT(src, offset, count)  (((src) >> (offset)) & (BIT_MASK(count)))  //little endian
#define BITS_SET(dst, val, offset, count) ((dst)|(((val) & (BIT_MASK(count))) << (offset)))
#define BITS_REVERT(src, mask)            ((src) = (~(src))&(mask))                    //extract some bits and revert, clear other bits

/****************************************************************/

typedef int            INT32;
typedef unsigned int   UINT32;
typedef short          INT16;
typedef unsigned short UINT16;
typedef char           INT8;
typedef unsigned char  UINT8;
typedef void           VOID;
typedef unsigned long long  LONG;
typedef unsigned char UCHAR;


#define NONLIN_SEG_COUNT 16
#define FIXPOINT_REVERT(fixval, fixbits)   ( ((fixval) + (1<<((fixbits)-1))) >> (fixbits) )
#define FIXPOINT_CLIP(fixval, fixbits)	(fixval >> fixbits)

#define TONE_BLK_FIXPOINT_BITS 12
#define TONE_BLK_FIXPOINT_FACTOR(x)    ((INT32)((x)*(1<<TONE_BLK_FIXPOINT_BITS)))
#define TONE_GAIN_FIXPOINT_BITS 14
#define TONE_GAIN_FIXPOINT_FACTOR(x)    ((INT32)((x)*(1<<TONE_GAIN_FIXPOINT_BITS)))
#define TONE_WEIGHT_FIXPOINT_BITS 9    // TONE_MAP_BLK_SIZEBITS + TONE_RADIUS_FIXPOINT_BITS = 8+1
#define TONE_WEIGHT_FIXPOINT_FACTOR(x)    ((INT32)((x)*(1<<TONE_WEIGHT_FIXPOINT_BITS)))
#define TONE_RADIUS_FIXPOINT_BITS 1
#define TONE_RADIUS_FIXPOINT_FACTOR(x)     ((INT32)((x)*(1<<TONE_RADIUS_FIXPOINT_BITS)))

typedef struct _NONLINEAR_CURVE
{
	//INT32 xBitDepth;
	INT8 dxBitDepth[NONLIN_SEG_COUNT]; // interval bitdepth
	INT32 y[NONLIN_SEG_COUNT+1];
}NONLINEAR_CURVE;


#define     IMAGE_FILE_NONLINEAR_NAME0    "/sdcard/nonlinearLut0.dat"
#define     IMAGE_FILE_NONLINEAR_NAME1    "/sdcard/nonlinearLut1.dat"

#define     TONE_MAP_BLK_SIZEBITS         8
#define     TONE_MAP_BLK_SIZE             256

class wdrBase
{
public:
    wdrBase();
    ~wdrBase();
public:
    void process();
    bool loadData(string imagePath,bool pgm = false);
private:
    int mWidth;
    int mHeight;
    int mBlkWidth;
    int mBlkHeight;
    int mGainOffset;
private:
    Mat mSrcImage;
    Mat mDstImage;
    Mat *mAvgLumiChannel;
    Mat *mMaxLumiChannel;
    Mat *mBlockLumiBuff;
    vector<Mat> mBgrChannel;
private:
    NONLINEAR_CURVE mNonlCurve;
private:
    int initWdrData();
    int initNonlinearCurve();
    int getMaxLumiChannel();
    int getAvgLumiChannel();
    int getBlockLumi(bool noise = false,bool light = false);
    int nonlinearCurveLut(int lumiVal);
    int nonlinearCurveTransfer(Mat *avgLumiBuff);
private:
    int blockCenterIndexUL(int x, int blkCenter, int blkRadius);
    int blkMeansGain(int blkLumi, UINT16 lpLumi);
    int toneMapping();
};

}

#endif //WDR_WDRBASE_H
