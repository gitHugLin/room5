//
// Created by linqi on 16-6-8.
//
#include "include/wdrBase.h"

using namespace wdr;

static double work_begin = 0;
static double work_end = 0;
static double gTime = 0;
//开始计时
static void workEnd(char *tag = "TimeCounts" );
static void workBegin();
static void workBegin()
{
    work_begin = getTickCount();
}
//结束计时
static void workEnd(char *tag)
{
    work_end = getTickCount() - work_begin;
    gTime = work_end /((double)getTickFrequency() )* 1000.0;
    LOGE("[TAG: %s ]:TIME = %lf ms \n",tag,gTime);
}

wdrBase::wdrBase()
{
    mWidth = 0;
    mHeight = 0;
    mBlkWidth = 0;
    mBlkHeight = 0;
    mGainOffset = 410;

    mAvgLumiChannel = NULL;
    mMaxLumiChannel = NULL;
    mBlockLumiBuff = NULL;
}

wdrBase::~wdrBase()
{
    if(NULL != mAvgLumiChannel)
        delete mAvgLumiChannel;
    if(NULL != mMaxLumiChannel)
        delete mMaxLumiChannel;
    if(NULL != mBlockLumiBuff)
        delete mBlockLumiBuff;

    mAvgLumiChannel = NULL;
    mMaxLumiChannel = NULL;
    mBlockLumiBuff = NULL;
}

bool wdrBase::loadData(string imagePath,bool pgm)
{
    if(pgm) {
        Mat bayer;
        bayer = imread(imagePath,0);
        cvtColor(bayer, mSrcImage, CV_BayerBG2RGB);
    } else {
        mSrcImage = imread(imagePath);
    }
    mWidth = mSrcImage.cols;
    mHeight = mSrcImage.rows;

    mAvgLumiChannel = new Mat(mHeight, mWidth, CV_16UC1);
    mMaxLumiChannel = new Mat(mHeight, mWidth, CV_16UC1);

    //TONE_MAP_BLK_SIZEBITS = 8
    mBlkWidth = mWidth >> TONE_MAP_BLK_SIZEBITS;
    mBlkHeight = mHeight >> TONE_MAP_BLK_SIZEBITS;
    if (mWidth - (mBlkWidth << TONE_MAP_BLK_SIZEBITS) > 0)
        mBlkWidth += 1;
    if (mHeight - (mBlkHeight << TONE_MAP_BLK_SIZEBITS) > 0)
    	mBlkHeight += 1;
    mBlockLumiBuff = new Mat(mBlkHeight, mBlkWidth, CV_32SC1);
    memset(mBlockLumiBuff->data,0,mBlkHeight * mBlkWidth * sizeof(int));

    //分离后各通道
    //mSrcImage.convertTo(mSrcImage,CV_16UC1);
    //split(mSrcImage,mBgrChannel);

    return 0;
}

int wdrBase::initNonlinearCurve()
{
    UINT32 y16;
	FILE *fp = NULL;
	fp = fopen(IMAGE_FILE_NONLINEAR_NAME0, "rb");
	if(NULL == fp) {
	    LOGE("initNonlinearCurve Error : Can not open %s file!\n",IMAGE_FILE_NONLINEAR_NAME1);
	    return -1;
	}

    //NONLIN_SEG_COUNT 16
	fread(mNonlCurve.dxBitDepth, sizeof(INT8), NONLIN_SEG_COUNT, fp);
	fread(mNonlCurve.y, sizeof(INT32), NONLIN_SEG_COUNT + 1, fp);
	y16 = mNonlCurve.y[16];
	if((y16 & 0xf00) == 0xf00)
	{
	    mNonlCurve.y[16]=0x1000;
	}
	fclose(fp);

	//for(int i = 0;i < 16; i++){
	    //LOGE("mNonlCurve.y[%d] = %d,mNonlCurve.dxBitDepth[%d] = %d",
	            //i,mNonlCurve.y[i],i,mNonlCurve.dxBitDepth[i]);
	//}
    //LOGE("mNonlCurve.y[16] = %d",mNonlCurve.y[16]);
	return 0;
}

int wdrBase::nonlinearCurveTransfer(Mat *avgLumiBuff)
{
	int lumi;
    int channels = avgLumiBuff->channels();
    int nRows = avgLumiBuff->rows;
    //图像数据列需要考虑通道数的影响；
    int nCols = avgLumiBuff->cols * channels;

    if (avgLumiBuff->isContinuous())//连续存储的数据，按一行处理
    {
        nCols *= nRows;
        nRows = 1;
    }

    int i,j;
    UINT16* p = NULL;
    for( i = 0; i < nRows; ++i)
    {
        p = avgLumiBuff->ptr<UINT16>(i);
        for ( j = 0; j < nCols; j++)
        {
            lumi = *p;
            lumi = nonlinearCurveLut(lumi);
            *p++ = lumi;
        }
    }

	return 0;
}

int wdrBase::nonlinearCurveLut(int lumiVal)
{
    /*
    float nonlCurve[17] = {0.0,887.0,1360.0,1714.0,2007.0,2261.0,2489.0
    ,2697.0,2889.0,3069.0,3237.0,3397.0,3549.0,3694.0,3833.0,3967.0,4096.0};
    for(int i = 0;i < 16; i++){
    	mNonlCurve.y[i] = nonlCurve[i],
    	mNonlCurve.dxBitDepth[i] = 8;
    }
    mNonlCurve.y[16] = 4096.0;*/
	int cumXVal = 0;
	int xDeltN = 0;
	INT16 interpWeight;
	int cumYLeft, cumYRight;
	int newLumi = 0;
	INT8 i;
	//NONLIN_SEG_COUNT 16
	for(i = 0; i < NONLIN_SEG_COUNT; i++)
	{
		xDeltN = 1 << mNonlCurve.dxBitDepth[i];
		cumXVal += xDeltN;
		if (lumiVal < cumXVal)
		{
			cumYLeft = mNonlCurve.y[i];
			cumYRight = mNonlCurve.y[i+1];
			interpWeight = cumXVal - lumiVal;
			newLumi = interpWeight*cumYLeft + (xDeltN - interpWeight)*cumYRight;
	        newLumi = FIXPOINT_REVERT(newLumi, mNonlCurve.dxBitDepth[i]);
			newLumi = newLumi>>2;
			newLumi = newLumi<<2;
			//if(newLumi > 255)
            	//LOGE("newLumi = %d",newLumi);
			break;
		}
	}
	return newLumi;
}


int wdrBase::getAvgLumiChannel()
{
    int interval = mSrcImage.channels();
    if(3 != interval) {
        LOGE("ERROR : input rgb image's channel is not 3!");
        return -1;
    }
    UCHAR channelR, channelG, channelB;
	UCHAR coeChannelR, coeChannelG, coeChannelB;
	int lumi;
	UINT32 avgL = 0;

	coeChannelR = 54     ; //channel R   0.2126*256
	coeChannelG = 183    ; //channel G	 0.7152*256
	coeChannelB = 18     ; //channel B	 0.0722*256
    int channels = mAvgLumiChannel->channels();
    int nRows = mAvgLumiChannel->rows;
    //图像数据列需要考虑通道数的影响；
    int nCols = mAvgLumiChannel->cols * channels;

    if (mAvgLumiChannel->isContinuous())//连续存储的数据，按一行处理
    {
        nCols *= nRows;
        nRows = 1;
    }

    int i,j;
    UINT16* pMaxLum = NULL;
    UINT16* pAvgLum = NULL;
    UCHAR* pRgb = NULL;
    for( i = 0; i < nRows; ++i)
    {
        pMaxLum = mMaxLumiChannel->ptr<UINT16>(i);
        pAvgLum = mAvgLumiChannel->ptr<UINT16>(i);
        pRgb = mSrcImage.ptr<UCHAR>(i);
        for ( j = 0; j < nCols; j++)
        {
            //the default Mat's order is BGR
        	channelR = *(pRgb+3*j+2);
            channelG = *(pRgb+3*j+1);
            channelB = *(pRgb+3*j);
        	lumi = coeChannelR * channelR + coeChannelG * channelG + coeChannelB * channelB;
        	lumi = FIXPOINT_CLIP(lumi, 8);
        	lumi = MIN(lumi, 0xff);
        	*pMaxLum++ = MAX(MAX(channelR, channelG), channelB);
        	//nonlinearCurveTransfer here
        	lumi = nonlinearCurveLut(lumi);
            *pAvgLum++ = lumi;
            avgL += lumi;
        }
    }
    avgL = avgL/(mWidth*mHeight);

    return avgL;
}

int wdrBase::initWdrData()
{
    int interval = mSrcImage.channels();
    if(3 != interval) {
        LOGE("ERROR : input rgb image's channel is not 3!");
        return -1;
    }
    UCHAR channelR, channelG, channelB;
	UCHAR coeChannelR, coeChannelG, coeChannelB;
	int lumi;
	UINT32 avgL = 0;

	coeChannelR = 54     ; //channel R   0.2126*256
	coeChannelG = 183    ; //channel G	 0.7152*256
	coeChannelB = 18     ; //channel B	 0.0722*256
    int channels = mAvgLumiChannel->channels();
    int nRows = mAvgLumiChannel->rows;
    //图像数据列需要考虑通道数的影响；
    int nCols = mAvgLumiChannel->cols * channels;

    if (mAvgLumiChannel->isContinuous())//连续存储的数据，按一行处理
    {
        nCols *= nRows;
        nRows = 1;
    }

    UINT16* pMaxLum = NULL;
    UINT16* pAvgLum = NULL;
    UCHAR* pRgb = NULL;
    INT32* pBlkLum = NULL;
    pRgb = mSrcImage.ptr<UCHAR>(0);
    pBlkLum = mBlockLumiBuff->ptr<INT32>(0);
    pMaxLum = mMaxLumiChannel->ptr<UINT16>(0);
    pAvgLum = mAvgLumiChannel->ptr<UINT16>(0);
    int xRest = 0, yRest = 0,i = 0,j = 0,x = 0,y = 0;
    UINT16 curAvgLumiVal;
    xRest = (mBlkWidth << TONE_MAP_BLK_SIZEBITS) - mWidth;
    yRest = (mBlkHeight << TONE_MAP_BLK_SIZEBITS) - mHeight;
    INT8 blkX, blkY, blkOffset;

    for( i = 0; i < nRows; ++i)
    {
        for ( j = 0; j < nCols; j++)
        {
            //the default Mat's order is BGR
        	channelR = *(pRgb+3*j+2);
            channelG = *(pRgb+3*j+1);
            channelB = *(pRgb+3*j);
        	lumi = coeChannelR * channelR + coeChannelG * channelG + coeChannelB * channelB;
        	lumi = FIXPOINT_CLIP(lumi, 8);
        	lumi = MIN(lumi, 0xff);
        	*pMaxLum++ = MAX(MAX(channelR, channelG), channelB);
        	//nonlinearCurveTransfer here
        	lumi = nonlinearCurveLut(lumi);
            *pAvgLum++ = lumi;
            avgL += lumi;
            x = nCols % mWidth;
            y = nCols / mHeight;
            blkX = x >> TONE_MAP_BLK_SIZEBITS;
            blkY = y >> TONE_MAP_BLK_SIZEBITS;
            blkOffset = blkY * mBlkWidth + blkX;
            curAvgLumiVal = lumi>>2<<2;
            *(pBlkLum + blkOffset) += curAvgLumiVal;

            if(x == mWidth-1) {
                *(pBlkLum + blkOffset) += xRest * curAvgLumiVal;
            }
            if(y == mHeight-1) {
            	*(pBlkLum + blkOffset) += yRest * curAvgLumiVal;
            }
            if(x == mWidth-1 && y == mHeight-1) {
            	*(pBlkLum + mBlkWidth*mBlkHeight-1) += xRest*yRest * curAvgLumiVal;
            }
        }
    }
    for(blkY = 0; blkY < mBlkHeight; blkY++)
    {
    	for(blkX = 0; blkX < mBlkWidth; blkX ++)
    	{
    		blkOffset = blkY * mBlkWidth + blkX;
    	    *(pBlkLum + blkOffset) = ((*(pBlkLum+blkOffset))>> TONE_MAP_BLK_SIZEBITS) >> TONE_MAP_BLK_SIZEBITS; //blockSumLumi/blkSize/blkSize
    		*(pBlkLum + blkOffset) = ((*(pBlkLum+blkOffset))>>2)<<2;
    	}
    }
    avgL = avgL/(mWidth*mHeight);

    return avgL;
}

int wdrBase::getMaxLumiChannel()
{
    int interval = mSrcImage.channels();
    if(3 != interval) {
        LOGE("ERROR : input rgb image's channel is not 3!");
        return -1;
    }
    UCHAR channelR, channelG, channelB;
	UINT32 avgL = 0;

    int channels = mMaxLumiChannel->channels();
    int nRows = mMaxLumiChannel->rows;
    //图像数据列需要考虑通道数的影响；
    int nCols = mMaxLumiChannel->cols * channels;

    if (mMaxLumiChannel->isContinuous())//连续存储的数据，按一行处理
    {
        nCols *= nRows;
        nRows = 1;
    }

    int i,j;
    UINT16* pMaxLum = NULL;
    UCHAR* pRgb = NULL;
    for( i = 0; i < nRows; ++i)
    {
        pMaxLum = mMaxLumiChannel->ptr<UINT16>(i);
        pRgb = mSrcImage.ptr<UCHAR>(i);
        for ( j = 0; j < nCols; j++)
        {
            //the default Mat's order is BGR
            channelR = *(pRgb+3*j+2);
            channelG = *(pRgb+3*j+1);
            channelB = *(pRgb+3*j);
            *pMaxLum++ = MAX(MAX(channelR, channelG), channelB);
            avgL += *(pMaxLum+j);
        }
    }
    avgL = avgL/(mWidth*mHeight);

    return avgL;
}

int wdrBase::getBlockLumi(bool noise,bool light)
{
    int xRest, yRest;
    UINT16 curAvgLumiVal;
    //int noiseRatio = ((0x00c0)>>4)<<4; //clip low 4bits
	//int bestLight  = ((0x0cf0)>>4)<<4; //clip low 4bits
	xRest = (mBlkWidth << TONE_MAP_BLK_SIZEBITS) - mWidth;
    yRest = (mBlkHeight << TONE_MAP_BLK_SIZEBITS) - mHeight;

    int x, y, offset;
    INT8 blkX, blkY, blkOffset;
    UINT16* pAvgLum = NULL;
    INT32* pBlkLum = NULL;
    pAvgLum = mAvgLumiChannel->ptr<UINT16>(0);
    pBlkLum = mBlockLumiBuff->ptr<INT32>(0);

    for( y = 0; y < mHeight; y++) {
		for(x = 0; x < mWidth; x++) {

			offset = y * mWidth + x;
			blkX = x >> TONE_MAP_BLK_SIZEBITS;
			blkY = y >> TONE_MAP_BLK_SIZEBITS;
			blkOffset = blkY * mBlkWidth + blkX;

	        curAvgLumiVal = ((*(pAvgLum + offset))>>2)<<2;
		    *(pBlkLum + blkOffset) += curAvgLumiVal;

	        if(x == mWidth-1) {
	            *(pBlkLum + blkOffset) += xRest * curAvgLumiVal;
		    }
		    if(y == mHeight-1) {
			    *(pBlkLum + blkOffset) += yRest * curAvgLumiVal;
	        }
	        if(x == mWidth-1 && y == mHeight-1) {
		        *(pBlkLum + mBlkWidth*mBlkHeight-1) += xRest*yRest * curAvgLumiVal;
		    }
		}

	}

	for(blkY = 0; blkY < mBlkHeight; blkY++)
	{
		for(blkX = 0; blkX < mBlkWidth; blkX ++)
		{
			blkOffset = blkY * mBlkWidth + blkX;
			*(pBlkLum + blkOffset) = ((*(pBlkLum+blkOffset))>> TONE_MAP_BLK_SIZEBITS) >> TONE_MAP_BLK_SIZEBITS; //blockSumLumi/blkSize/blkSize
			*(pBlkLum + blkOffset) = ((*(pBlkLum+blkOffset))>>2)<<2;
			//if(noise)
				//pBlkLum[blkOffset] = CLIP(pBlkLum[blkOffset], noiseRatio,0xffffffff);
            //if(light)
				//pBlkLum[blkOffset] = CLIP(pBlkLum[blkOffset], 0x0, bestLight);
			LOGD("pBlkLum = %d ",pBlkLum[blkOffset]);
		}
	}

    return 0;
}


int wdrBase::blkMeansGain(int blkLumi, UINT16 lpLumi)
{
	int lumiGain, newLumi;

    //figure out gain = (1 + blkLumi*lpLumi + gainOffset1)/(blkLumi + lpLumi + gainOffset2)
    //newLumi =  (4096 + (blkLumi*lpLumi >> TONE_BLK_FIXPOINT_BITS) );
    //lumiGain = TONE_GAIN_FIXPOINT_FACTOR(newLumi)/(blkLumi + lpLumi + mGainOffset);
	newLumi =  (TONE_BLK_FIXPOINT_FACTOR(1) + (blkLumi*lpLumi >> TONE_BLK_FIXPOINT_BITS) );
	lumiGain = TONE_GAIN_FIXPOINT_FACTOR(newLumi)/(blkLumi + lpLumi + mGainOffset);

	//newLumi =  1 + blkLumi*lpLumi ;
    //lumiGain = newLumi/(blkLumi + lpLumi + mGainOffset);
	lumiGain = MIN(lumiGain, 0x3ffff); //gain 18bit

	return lumiGain;
}

int wdrBase::blockCenterIndexUL(int x, int blkCenter, int blkRadius)
{

	int blkSizeOffset;
	int blkIndex;

	//x = TONE_RADIUS_FIXPOINT_FACTOR(x);
	blkSizeOffset = x - blkCenter;//blkCenter = 255

	if (blkSizeOffset < 0)
		blkIndex = -(blkRadius+1);
	else
	{
	    //TONE_WEIGHT_FIXPOINT_BITS = 9
		blkSizeOffset = blkSizeOffset>>TONE_WEIGHT_FIXPOINT_BITS;
		blkIndex = (blkSizeOffset<<TONE_WEIGHT_FIXPOINT_BITS) + blkCenter;
	}

	return blkIndex;
}

int wdrBase::toneMapping()
{
    LOGD("toneMapping is begin! ");
	int x, y,row,col;
	int channelR, channelG, channelB;
	int channelOR, channelOG, channelOB;
	LONG  oldGain;
	LONG  blkDWeightUL, blkDWeightUR, blkDWeightDL, blkDWeightDR;//64bit
	int rgbOffset = 0, gain;
	int blkRadius, blkCenter;
	int blkIndexUY, blkIndexLX, blkIndexDY, blkIndexRX;
	int blkDWeightX, blkDWeightY;
	int blkLumiUL, blkLumiUR, blkLumiDL, blkLumiDR;
	int blkMapUL, blkMapUR, blkMapDL, blkMapDR;
	//int gainMax = (0xffff>>10)<<10; //sw_wdr_gain_max = 0xffff; gainMax = 64512

	blkRadius = TONE_MAP_BLK_SIZE; //256
	blkCenter = (TONE_MAP_BLK_SIZE>>1)-1 + (TONE_MAP_BLK_SIZE>>1); //xCenter,yCenter = TONE_RADIUS_FIXPOINT_FACTOR( (blkRadius-1 + blkRadius)/2 )

    int offset = 0;
    UINT16 lpLumi;
    UINT16* pMaxLum = mMaxLumiChannel->ptr<UINT16>(0);
    INT32* pBlockLum = mBlockLumiBuff->ptr<INT32>(0);
    UCHAR* pRgb = mSrcImage.ptr<UCHAR>(0);

    for( y = 0; y < mHeight; y++)
    {
        for ( x = 0; x < mWidth; x++)
        {
            //the default Mat's order is BGR
            offset = y * mWidth + x;
        	channelR = *(pRgb+3*offset);
            channelG = *(pRgb+3*offset+1);
            channelB = *(pRgb+3*offset+2);
            lpLumi = *(pMaxLum+offset);

			row = TONE_RADIUS_FIXPOINT_FACTOR(y);   //y*2
			col = TONE_RADIUS_FIXPOINT_FACTOR(x);   //x*2
			// block center coordinates
			blkIndexUY = blockCenterIndexUL(row, blkCenter, blkRadius);
			blkIndexLX = blockCenterIndexUL(col, blkCenter, blkRadius);
			blkIndexDY = blkIndexUY + TONE_RADIUS_FIXPOINT_FACTOR(TONE_MAP_BLK_SIZE);
			blkIndexRX = blkIndexLX + TONE_RADIUS_FIXPOINT_FACTOR(TONE_MAP_BLK_SIZE);

            //find out distance between current pixel and each block;
			blkDWeightY = row - blkIndexUY; //distance from upleft = y(or x) - blockCenter_upleft
			blkDWeightX = col - blkIndexLX;
		    //calculate weight factors of these four blocks (8bit * 8bit)
		    blkDWeightUL = (TONE_WEIGHT_FIXPOINT_FACTOR(1) - blkDWeightY)*(TONE_WEIGHT_FIXPOINT_FACTOR(1) - blkDWeightX);
		    blkDWeightUR = (TONE_WEIGHT_FIXPOINT_FACTOR(1) - blkDWeightY)*(blkDWeightX);
		    blkDWeightDL = (blkDWeightY)*(TONE_WEIGHT_FIXPOINT_FACTOR(1) - blkDWeightX);
	        blkDWeightDR = (blkDWeightY)*(blkDWeightX);

			// block index for finding out corresponding block lumi average
			blkIndexUY = MAX(blkCenter, blkIndexUY) >> TONE_WEIGHT_FIXPOINT_BITS; //boundary clip to get average lumi of each block
			blkIndexLX = MAX(blkCenter, blkIndexLX) >> TONE_WEIGHT_FIXPOINT_BITS;
			blkIndexDY = MIN(((mBlkHeight<<TONE_WEIGHT_FIXPOINT_BITS) - blkCenter), blkIndexDY) >> TONE_WEIGHT_FIXPOINT_BITS;
			blkIndexRX = MIN(((mBlkWidth<<TONE_WEIGHT_FIXPOINT_BITS) - blkCenter), blkIndexRX) >> TONE_WEIGHT_FIXPOINT_BITS;

			//get average luminance of each block( or use data from last frame)
			blkLumiUL = *(pBlockLum + blkIndexUY*mBlkWidth + blkIndexLX);
			blkLumiUR = *(pBlockLum + blkIndexUY*mBlkWidth + blkIndexRX);
			blkLumiDL = *(pBlockLum + blkIndexDY*mBlkWidth + blkIndexLX);
			blkLumiDR = *(pBlockLum + blkIndexDY*mBlkWidth + blkIndexRX);

			blkMapUL = blkMeansGain(blkLumiUL, lpLumi);
			blkMapUR = blkMeansGain(blkLumiUR, lpLumi);
			blkMapDL = blkMeansGain(blkLumiDL, lpLumi);
			blkMapDR = blkMeansGain(blkLumiDR, lpLumi);

            oldGain = ((blkDWeightUL*blkMapUL)>>16) + ((blkDWeightUR*blkMapUR)>>16) +
                    ((blkDWeightDL*blkMapDL)>>16) + ((blkDWeightDR*blkMapDR)>>16);
			gain = (int)oldGain;
			//if(channelR)
			//gain = MIN(gain, gainMax);
            //get current pixel's luminance after tone mapping
			channelOR = MIN(FIXPOINT_REVERT((channelR+rgbOffset)*gain
			        , TONE_GAIN_FIXPOINT_BITS), BIT_MASK(12));
            channelOG = MIN(FIXPOINT_REVERT((channelG+rgbOffset)*gain
                    , TONE_GAIN_FIXPOINT_BITS), BIT_MASK(12));
            channelOB = MIN(FIXPOINT_REVERT((channelB+rgbOffset)*gain
                    , TONE_GAIN_FIXPOINT_BITS), BIT_MASK(12));
            channelOR = (channelOR>>4);
            channelOG = (channelOG>>4);
            channelOB = (channelOB>>4);
            *(pRgb+3*offset) = (UINT8)channelOR;
            *(pRgb+3*offset+1) = (UINT8)channelOG;
            *(pRgb+3*offset+2) = (UINT8)channelOB;

        }
    }
    LOGD("toneMapping is end! ");

    return 0;
}



void wdrBase::process()
{
    loadData("/sdcard/night.pgm",true);
    imwrite("/sdcard/wdrSrc.jpg",mSrcImage);
    initNonlinearCurve();
    workBegin();
    getAvgLumiChannel();
    getBlockLumi();
    //initWdrData();
    toneMapping();
    workEnd("end of process in wdrBase object!");
    imwrite("/sdcard/wdrDst.jpg",*mAvgLumiChannel);
}