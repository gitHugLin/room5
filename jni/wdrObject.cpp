#include "wdrObject.h"

using namespace wdrCpuMode;

static double gWork_begin = 0;
static double gWork_end = 0;
static double gTime = 0;
//开始计时
static void workEnd(char *tag = "TimeCounts");
static void workBegin();
static void workBegin() { gWork_begin = getTickCount(); }
//结束计时
static void workEnd(char *tag) {
  gWork_end = getTickCount() - gWork_begin;
  gTime = gWork_end / ((double)getTickFrequency()) * 1000.0;
  LOGE("[TAG: %s ]:TIME = %lf ms \n", tag, gTime);
}

wdrObject::wdrObject() {
  mWidth = 0;
  mHeight = 0;
  mBlkSize = 15;
  mGainOffset = 0.18;
  mColumnSum = NULL;
  mIntegralImage = NULL;

  for (int y = 0; y < 256; y++)
    for (int x = 0; x < 256; x++) {
      float lumiBlk = (float)x / 255, lumiPixel = (float)y / 255;
      mToneMapLut[y][x] = y * (1 + mGainOffset + lumiBlk * lumiPixel) /
                          (lumiBlk + lumiPixel + mGainOffset);
    }
}

wdrObject::~wdrObject() {
  if (NULL != mIntegralImage) {
    delete[] mIntegralImage;
    mIntegralImage = NULL;
  }
  if (NULL != mColumnSum) {
    delete[] mColumnSum;
    mColumnSum = NULL;
  }
}

bool wdrObject::loadData(string imagePath, bool pgm) {
  bool ret = true;
  if (pgm) {
    Mat bayer;
    bayer = imread(imagePath, 0);
    cvtColor(bayer, mSrcImage, CV_BayerBG2RGB);
  } else {
    mSrcImage = imread(imagePath);
  }

  if (mSrcImage.empty()) {
    LOGE("Error: can not load image!");
  }
  cvtColor(mSrcImage, mGrayChannel, CV_RGB2GRAY);
  mWidth = mGrayChannel.cols;
  mHeight = mGrayChannel.rows;
  // LOGE("loadData: mWidth = %d,mHeight = %d",mWidth,mHeight);
  // mIntegralImage = new Mat(mHeight, mWidth, CV_32SC1);
  mIntegralImage = new UINT32[mHeight * mWidth];
  mColumnSum = new UINT32[mWidth]; // sum of each column
}

void wdrObject::fastIntegral() {
  workBegin();
  // INT32* pIntegral = NULL;
  UINT8 *pGray = NULL;
  // pIntegral = mIntegralImage->ptr<INT32>(0);
  pGray = mGrayChannel.ptr<UINT8>(0);
  UINT32 *pIntegral = mIntegralImage;
  UINT32 *columnSum = mColumnSum; // sum of each column
  // calculate integral of the first line
  for (int i = 0; i < mWidth; i++) {
    columnSum[i] = *(pGray + i);
    *(pIntegral + i) = *(pGray + i);
    if (i > 0) {
      *(pIntegral + i) += *(pIntegral + i - 1);
    }
  }
  for (int i = 1; i < mHeight; i++) {
    int offset = i * mWidth;
    // first column of each line
    columnSum[0] += *(pGray + offset);
    *(pIntegral + offset) = columnSum[0];
    // other columns
    for (int j = 1; j < mWidth; j++) {
      columnSum[j] += *(pGray + offset + j);
      *(pIntegral + offset + j) = *(pIntegral + offset + j - 1) + columnSum[j];
    }
  }
  int gGainIndex =
      *(pIntegral + (mWidth - 1) * (mHeight - 1)) / (mWidth * mHeight);
  // LOGE("Log:max value in intergralImage pixels = %d", gGainIndex);
  workEnd("end of fastIntegral!");
}

void wdrObject::toneMapping() {
  workBegin();
  UINT32 nCols = mWidth, nRows = mHeight;
  INT32 x = 0, y = 0;
  // INT32* pIntegral = NULL;
  UINT8 *pGray = NULL;
  UINT32 blockAvgLumi = 0;
  // pIntegral = mIntegralImage->ptr<INT32>(0);
  UINT32 *pIntegral = mIntegralImage;
  mDstImage = mGrayChannel;
  pGray = mDstImage.ptr<UINT8>(0);

  for (y = 0; y < nRows; y++) {
    for (x = 0; x < nCols; x++) {

      UINT32 xMin = wdrMax(x - mBlkSize, 0);
      UINT32 yMin = wdrMax(y - mBlkSize, 0);
      UINT32 xMax = wdrMin(x + mBlkSize, nCols - 1);
      UINT32 yMax = wdrMin(y + mBlkSize, nRows - 1);

      INT32 yMaxOffset = yMax * nCols;
      INT32 yMinOffset = yMin * nCols;
      blockAvgLumi =
          *(pIntegral + xMax + yMaxOffset) - *(pIntegral + xMin + yMaxOffset) -
          *(pIntegral + xMax + yMinOffset) + *(pIntegral + xMin + yMinOffset);

      // blockAvgLumi = 100;
      // blockAvgLumi = blockAvgLumi/((yMax - yMin + 1)*(xMax - xMin + 1));
      blockAvgLumi = blockAvgLumi >> 10; // 1024 = 32*32
      UINT32 offsetGray = y * nCols + x;
      UINT32 indexX = (int)blockAvgLumi;
      UINT32 indexY = *(pGray + offsetGray);
      int finalPixel = mToneMapLut[indexY][indexX];
      UINT32 curPixel = wdrMin(finalPixel, 255);
      *(pGray + offsetGray) = curPixel;
    }
  }

  // LOGE("end of toneMapping! blockAvgLumi = %lf",blockAvgLumi);
  workEnd("TONEMAPPING TIME:");
}

void wdrObject::process() {
  loadData("/data/local/tunnel.pgm", true);
  // workBegin();
  fastIntegral();
  // Mat dst;
  // integral(mGrayChannel, dst);
  toneMapping();
  // workEnd("PROCESS TIME:");
  imwrite("/sdcard/grayImage.jpg", mDstImage);
}
