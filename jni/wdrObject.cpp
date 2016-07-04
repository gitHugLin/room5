#include "wdrObject.h"

using namespace wdrCpuMode;


static double gWork_begin = 0;
static double gWork_end = 0;
static double gTime = 0;
//开始计时
static void workEnd(char *tag = "TimeCounts" );
static void workBegin();
static void workBegin()
{
    gWork_begin = getTickCount();
}
//结束计时
static void workEnd(char *tag)
{
    gWork_end = getTickCount() - gWork_begin;
    gTime = gWork_end /((double)getTickFrequency() )* 1000.0;
    LOGE("[TAG: %s ]:TIME = %lf ms \n",tag,gTime);
}

wdrObject::wdrObject()
{
    mWidth = 0;
    mHeight = 0;
    mBlkSize = 127;
    mGainOffset = 0.18;

    mIntegralImage = NULL;

    for(int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            float n = y,k = x;
            float i = n/255,j = k/255;
            mToneMapLut[y][x] = (1 + mGainOffset + i*j)/(i+j + mGainOffset);
            if(x == 20 &&y == 255)
            LOGE("mToneMapLut[%d][%d] = %f",y,x,mToneMapLut[y][x]);
        }
}

wdrObject::~wdrObject()
{
    if( NULL != mIntegralImage) {
        delete mIntegralImage;
        mIntegralImage = NULL;
    }
}


bool wdrObject::loadData(string imagePath, bool pgm)
{
    bool ret = true;
    if(pgm) {
        Mat bayer;
        bayer = imread(imagePath,0);
        cvtColor(bayer, mSrcImage, CV_BayerBG2RGB);
    } else {
        mSrcImage = imread(imagePath);
    }

    if(mSrcImage.empty()) {
        LOGE("Error: can not load image!");
    }
    cvtColor(mSrcImage, mGrayChannel, CV_RGB2GRAY);
    mWidth = mGrayChannel.cols;
    mHeight = mGrayChannel.rows;
    LOGE("loadData: mWidth = %d,mHeight = %d",mWidth,mHeight);

    mIntegralImage = new Mat(mHeight, mWidth, CV_32SC1);

}

void wdrObject::fastIntegral()
{
    workBegin();
    INT32* pIntegral = NULL;
    UINT8* pGray = NULL;
    pIntegral = mIntegralImage->ptr<INT32>(0);
    pGray = mGrayChannel.ptr<UINT8>(0);
    INT32 *columnSum = new INT32[mWidth]; // sum of each column
    // calculate integral of the first line
    for(int i = 0;i < mWidth;i++) {
        columnSum[i] = *(pGray+i);
        *(pIntegral+i) = *(pGray+i);
        if(i > 0){
            *(pIntegral + i) += *(pIntegral + i - 1);
        }
    }
    for(int i = 1;i < mHeight;i++) {
        int offset = i*mWidth;
        // first column of each line
        columnSum[0] += *(pGray+offset);
        *(pIntegral+offset) = columnSum[0];
         // other columns
        for(int j = 1;j < mWidth;j++) {
            columnSum[j] += *(pGray+offset+j);
            *(pIntegral+offset+j) = *(pIntegral+offset+j-1) + columnSum[j];
        }
    }
    int gGainIndex = *(pIntegral+(mWidth-1)*(mHeight-1))/(mWidth*mHeight);
    LOGE("Log:max value in intergralImage pixels = %d",gGainIndex);
    workEnd("end of fastIntegral!");
}


void wdrObject::toneMapping()
{
    workBegin();
    INT32 nCols = mWidth,nRows = mHeight;
    INT32 x = 0,y = 0;
    INT32* pIntegral = NULL;
    UINT8* pGray = NULL;
    float blockAvgLumi = 0;
    pIntegral = mIntegralImage->ptr<INT32>(0);
    mDstImage = mGrayChannel;
    pGray = mDstImage.ptr<UINT8>(0);

    for( y = 0; y < nRows; y++)
    {
        for ( x = 0; x < nCols; x++)
        {
            INT32 xMin = wdrMax(x - mBlkSize, 0);
            INT32 yMin = wdrMax(y - mBlkSize, 0);
            INT32 xMax = wdrMin(x + mBlkSize, nCols - 1);
            INT32 yMax = wdrMin(y + mBlkSize, nRows - 1);

            blockAvgLumi = *(pIntegral+xMax+yMax*nCols) - *(pIntegral+xMin+yMax*nCols) -
                        *(pIntegral+xMax+yMin*nCols) + *(pIntegral+xMin+yMin*nCols);

            blockAvgLumi = blockAvgLumi/((yMax - yMin)*(xMax - xMin));
            int offsetGray = y*nCols+x;
            int indexX = (int)blockAvgLumi;
            int indexY = *(pGray+offsetGray);
            float gain = mToneMapLut[indexY][indexX];

            INT32 curPixel = wdrMin(int(gain*indexY), 255);
            *(pGray+offsetGray) = curPixel;

        }

    }

    //LOGE("end of toneMapping! blockAvgLumi = %lf",blockAvgLumi);
    workEnd("TONEMAPPING TIME:");
}

void wdrObject::process()
{
    loadData("/sdcard/tunnel.pgm",true);
    //workBegin();
    //getIntegralImage();
    fastIntegral();
    toneMapping();
    //workEnd("PROCESS TIME:");
    imwrite("/sdcard/grayImage.jpg",mDstImage);

}
