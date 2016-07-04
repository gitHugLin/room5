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
    mBlkSize = 15;
    mGainOffset = 0.18;
    mColumnSum = NULL;
    mIntegralImage = NULL;

    for(int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++) {
            float lumiBlk = (float)y/255,lumiPixel = (float)x/255;
            mToneMapLut[y][x] = (1 + mGainOffset + lumiBlk*lumiPixel)/(lumiBlk+lumiPixel + mGainOffset);
        }
}

wdrObject::~wdrObject()
{
    if( NULL != mIntegralImage) {
        delete []mIntegralImage;
        mIntegralImage = NULL;
    }
    if( NULL != mColumnSum) {
        delete []mColumnSum;
        mColumnSum = NULL;
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
    //LOGE("loadData: mWidth = %d,mHeight = %d",mWidth,mHeight);
    //mIntegralImage = new Mat(mHeight, mWidth, CV_32SC1);
    mIntegralImage = new UINT32[mHeight*mWidth];
    mColumnSum = new UINT32[mWidth]; // sum of each column
}

void wdrObject::fastIntegral()
{
    workBegin();
    //INT32* pIntegral = NULL;
    UINT8* pGray = NULL;
    //pIntegral = mIntegralImage->ptr<INT32>(0);
    pGray = mGrayChannel.ptr<UINT8>(0);
    UINT32* pIntegral = mIntegralImage;
    UINT32 *columnSum = mColumnSum; // sum of each column
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

void wdrObject::getInteImageNeon()
{
    workBegin();
    UINT8* pGray = NULL;
    pGray = mGrayChannel.ptr<UINT8>(0);
    UINT32* pIntegral = mIntegralImage;
    neon_integral_image(pGray, pIntegral,mWidth, mHeight);
    workEnd("getInteImageNeon TIME");
}

void wdrObject::neon_integral_image(const uint8_t *sourceImage, uint32_t *integralImage,
                         size_t width, size_t height)
{
    // integral images add an extra row and column of 0s to the image
    size_t integralImageWidth = width + 1;

    // 0 out the first row
    memset(integralImage, 0, sizeof(integralImage[0]) * integralImageWidth);

    // pointer to the start of the integral image, skipping past the first row and column
    uint32_t *integralImageStart = integralImage + integralImageWidth + 1;

    //  used to carry over the last prefix sum of a row segment to the next segment
    uint32_t prefixSumLastElement = 0;

    // a vector used later for shifting bytes and replacing with 0 using vector extraction
    uint16x8_t zeroVec = vdupq_n_u16(0);

    // prefix sum for rows
    for (size_t i = 0; i < height; ++i) {

        // vector to carry over last prefix sum value to next chunk
        uint32x4_t carry = vdupq_n_u32(0);

        // get offset to start of row, taking into account that we have one more column and row than the source image
        size_t sourceRowOffset = i * width;

        // from the integral image start, this gives us an offset to the beginning of the next row, skipping the 0 column
        size_t integralRowOffset = i * integralImageWidth;

        // 0 out the start of every row, starting from the 2nd
        integralImage[integralImageWidth + integralRowOffset] = 0;

        // count how many bytes we've passed over
        size_t j = 0;

        // iterate over the row in 16 byte chunks
        for (; j + 16 < width; j += 16) {

            // load 16 bytes/ints from given offset
            uint8x16_t elements = vld1q_u8(sourceImage + sourceRowOffset + j);

            // take lower 8 8-bit ints
            uint8x8_t lowElements8 = vget_low_u8(elements);

            // convert them to 16-bit ints
            uint16x8_t lowElements16 = vmovl_u8(lowElements8);

            // take upper 8 8-bit ints
            uint8x8_t highElements8 = vget_high_u8(elements);

            // convert them to 16-bit ints
            uint16x8_t highElements16 = vmovl_u8(highElements8);

            // add lowElements16 to lowElements16 shifted 2 bytes (1 element) to the right
            lowElements16 = vaddq_u16(lowElements16, vextq_u16(zeroVec, lowElements16, 7));

            // add result to result shifted 4 bytes (2 elements) to the right
            lowElements16 = vaddq_u16(lowElements16, vextq_u16(zeroVec, lowElements16, 6));

            // add result to result shifted 8 bytes (4 elements) to the right, we now have the prefix sums for this section
            lowElements16 = vaddq_u16(lowElements16, vextq_u16(zeroVec, lowElements16, 4));

            // do the same 3 steps above for highElements16
            highElements16 = vaddq_u16(highElements16, vextq_u16(zeroVec, highElements16, 7));
            highElements16 = vaddq_u16(highElements16, vextq_u16(zeroVec, highElements16, 6));
            highElements16 = vaddq_u16(highElements16, vextq_u16(zeroVec, highElements16, 4));

            // take lower 4 16-bit ints of lowElements16, convert to 32 bit, and add to carry (carry is 0s for the first 8 pixels in each row)
            uint32x4_t lowElementsOfLowPrefix32 = vaddq_u32(vmovl_u16(vget_low_u16(lowElements16)), carry);

            // store lower 4 32-bit ints at appropriate offset: (X|O|O|O)
            vst1q_u32(integralImageStart + integralRowOffset + j, lowElementsOfLowPrefix32);

            // take upper 4 16-bit ints of lowElements16, convert to 32 bit, and add to carry
            uint32x4_t highElementsOfLowPrefix32 = vaddq_u32(vmovl_u16(vget_high_u16(lowElements16)), carry);

            // store upper 4 32-bit ints at appropriate offset: (O|X|O|O)
            vst1q_u32(integralImageStart + integralRowOffset + j + 4, highElementsOfLowPrefix32);

            // take the last prefix sum from the second 32-bit vector to be added to the next prefix sums
            prefixSumLastElement = vgetq_lane_u32(highElementsOfLowPrefix32, 3);

            // fill carry vector with 4 32-bit ints, each with the value of the last prefix sum element
            carry = vdupq_n_u32(prefixSumLastElement);

            // take lower 4 16-bit ints of lowElements16, convert to 32 bit, and add to carry (carry is 0s for the first pass of each row)
            uint32x4_t lowElementsOfHighPrefix32 = vaddq_u32(vmovl_u16(vget_low_u16(highElements16)), carry);

            // store lower 4 32-bit ints at appropriate offset: (O|O|X|O)
            vst1q_u32(integralImageStart + integralRowOffset + j + 8, lowElementsOfHighPrefix32);

            // take upper 4 16-bit ints of lowElements16, convert to 32 bit, and add to carry
            uint32x4_t highElementsOfHighPrefix32 = vaddq_u32(vmovl_u16(vget_high_u16(highElements16)), carry);

            // store upper 4 32-bit ints at appropriate offset (O|O|O|X)
            vst1q_u32(integralImageStart + integralRowOffset + j + 12, highElementsOfHighPrefix32);

            // take the last prefix sum from the second 32-bit vector to be added to the next prefix sums
            prefixSumLastElement = vgetq_lane_u32(highElementsOfHighPrefix32, 3);

            // fill carry vector with 4 32-bit ints, each with the value of the last prefix sum element
            carry = vdupq_n_u32(prefixSumLastElement);
        }

        // now handle the remainders (< 16 pixels)
        for (; j < width ; ++j) {

            // take the last prefix sum value and add the 8-bit int value from the source image at the appropriate index
            prefixSumLastElement += sourceImage[sourceRowOffset + j];

            // set the value of the integral image to the last prefix sum, at the same index
            integralImageStart[integralRowOffset + j] = prefixSumLastElement;
        }
    }

    // prefix sum for columns, using height - 1 since we're taking pairs of rows
    for (size_t i = 0; i < height - 1; ++i) {
        size_t j = 0;
        size_t integralRowOffset = i * integralImageWidth;

        for (; j + 16 < width; j += 16) {

            // load 4 32-bit ints from row i (first row)
            uint32x4_t row1Elements32 = vld1q_u32(integralImageStart + integralRowOffset + j);

            // load 4 32-bit ints from row i + 1 (second row)
            uint32x4_t row2Elements32 = vld1q_u32(integralImageStart + integralRowOffset + integralImageWidth + j);

            // add first row to second row, giving the prefix sum for the second row
            row2Elements32 = vqaddq_u32(row1Elements32, row2Elements32);

            // replace the stored second row values with the prefix sum values
            vst1q_u32(integralImageStart + integralRowOffset + integralImageWidth + j, row2Elements32);

            // do the same for the next 3 offsets (4, 8, 12), completing a 128-bit chunk
            row1Elements32 = vld1q_u32(integralImageStart + integralRowOffset + j + 4);
            row2Elements32 = vld1q_u32(integralImageStart + integralRowOffset + integralImageWidth + j + 4);
            row2Elements32 = vqaddq_u32(row1Elements32, row2Elements32);
            vst1q_u32(integralImageStart + integralRowOffset + integralImageWidth + j + 4, row2Elements32);

            row1Elements32 = vld1q_u32(integralImageStart + integralRowOffset + j + 8);
            row2Elements32 = vld1q_u32(integralImageStart + integralRowOffset + integralImageWidth + j + 8);
            row2Elements32 = vqaddq_u32(row1Elements32, row2Elements32);
            vst1q_u32(integralImageStart + integralRowOffset + integralImageWidth + j + 8, row2Elements32);

            row1Elements32 = vld1q_u32(integralImageStart + integralRowOffset + j + 12);
            row2Elements32 = vld1q_u32(integralImageStart + integralRowOffset + integralImageWidth + j + 12);
            row2Elements32 = vqaddq_u32(row1Elements32, row2Elements32);
            vst1q_u32(integralImageStart + integralRowOffset + integralImageWidth + j + 12, row2Elements32);
        }

        // now handle the remainders
        for (; j < width; ++j) {
            integralImageStart[integralRowOffset + integralImageWidth + j] += integralImageStart[integralRowOffset + j];
        }
    }
}

void wdrObject::toneMapping()
{
    workBegin();
    UINT32 nCols = mWidth,nRows = mHeight;
    INT32 x = 0,y = 0;
    //INT32* pIntegral = NULL;
    UINT8* pGray = NULL;
    UINT32 blockAvgLumi = 0;
    //pIntegral = mIntegralImage->ptr<INT32>(0);
    UINT32* pIntegral = mIntegralImage;
    mDstImage = mGrayChannel;
    pGray = mDstImage.ptr<UINT8>(0);

    for( y = 0; y < nRows; y++)
    {
        for ( x = 0; x < nCols; x++)
        {
            UINT32 xMin = wdrMax(x - mBlkSize, 0);
            UINT32 yMin = wdrMax(y - mBlkSize, 0);
            UINT32 xMax = wdrMin(x + mBlkSize, nCols - 1);
            UINT32 yMax = wdrMin(y + mBlkSize, nRows - 1);

            blockAvgLumi = *(pIntegral+xMax+yMax*nCols) - *(pIntegral+xMin+yMax*nCols) -
                        *(pIntegral+xMax+yMin*nCols) + *(pIntegral+xMin+yMin*nCols);

            //blockAvgLumi = blockAvgLumi/((yMax - yMin + 2)*(xMax - xMin + 2));
            blockAvgLumi = blockAvgLumi>>10;//1024 = 32*32
            UINT32 offsetGray = y*nCols+x;
            UINT32 indexX = (int)blockAvgLumi;
            UINT32 indexY = *(pGray+offsetGray);
            float gain = mToneMapLut[indexY][indexX];

            UINT32 curPixel = wdrMin(int(gain*indexY), 255);
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
    fastIntegral();
    //getInteImageNeon();
    toneMapping();
    //workEnd("PROCESS TIME:");
    imwrite("/sdcard/grayImage.jpg",mDstImage);

}
