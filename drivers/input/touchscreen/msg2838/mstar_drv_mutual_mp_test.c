/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_mutual_mp_test.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

/*=============================================================*/

/*=============================================================*/

#include "mstar_drv_mutual_mp_test.h"
#include "mstar_drv_utility_adaption.h"
#include "mstar_drv_mutual_fw_control.h"
#include "mstar_drv_platform_porting_layer.h"

#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
#ifdef CONFIG_ENABLE_ITO_MP_TEST

#include "msg26xxm_open_test_X.h"
#include "msg26xxm_short_test_X.h"
#include "msg28xx_mp_test_X.h"

/*=============================================================*/

/*=============================================================*/

extern u32 SLAVE_I2C_ID_DBBUS;
extern u32 SLAVE_I2C_ID_DWI2C;

extern u8 g_ChipType;

/*=============================================================*/

/*=============================================================*/

static u32 _gIsInMpTest;
static u32 _gTestRetryCount = CTP_MP_TEST_RETRY_COUNT;
static ItoTestMode_e _gItoTestMode;

static s32 _gCtpMpTestStatus = ITO_TEST_UNDER_TESTING;

static u16 _gSenseLineNum;
static u16 _gDriveLineNum;
static u16 _gWaterProofNum;

static struct work_struct _gCtpItoTestWork;
static struct workqueue_struct *_gCtpMpTestWorkQueue;

static s32 _gDeltaC[MAX_MUTUAL_NUM] = {0};
static s32 _gResult[MAX_MUTUAL_NUM] = {0};
static s32 _gDeltaCWater[12] = {0};
static s32 _gResultWater[12] = {0};

static s32 _gSenseR[MAX_CHANNEL_NUM] = {0};
static s32 _gDriveR[MAX_CHANNEL_NUM] = {0};
static s32 _gGRR[MAX_CHANNEL_NUM] = {0};
static s32 _gTempDeltaC[MAX_MUTUAL_NUM] = {0};

static u8 _gTestFailChannel[MAX_MUTUAL_NUM] = {0};
static u32 _gTestFailChannelCount;

static u8 _gShortTestChannel[MAX_CHANNEL_NUM] = {0};

TestScopeInfo_t g_TestScopeInfo = {0};

static u8 _gTestAutoSwitchFlag = 1;
static u8 _gTestSwitchMode;

u16 _gMuxMem_20_3E_0_Settings[16] = {0};
u16 _gMuxMem_20_3E_1_Settings[16] = {0};
u16 _gMuxMem_20_3E_2_Settings[16] = {0};
u16 _gMuxMem_20_3E_3_Settings[16] = {0};
u16 _gMuxMem_20_3E_4_Settings[16] = {0};
u16 _gMuxMem_20_3E_5_Settings[16] = {0};
u16 _gMuxMem_20_3E_6_Settings[16] = {0};

/*=============================================================*/

/*=============================================================*/

static void _DrvMpTestItoTestMsg26xxmSetToNormalMode(void)
{
	u16 nRegData = 0;
	u16 nTmpAddr = 0, nAddr = 0;
	u16 nDriveNumGeg = 0, nSenseNumGeg = 0;
	u16 i = 0;

	DBG("*** %s() ***\n", __func__);

	RegSet16BitValue(0x0FE6, 0x0001);

	nRegData = RegGet16BitValue(0x110E);

	if (nRegData == 0x1D08)
		DBG("Wrong mode 0\n");

	nRegData &= 0x0800;

	if (nRegData > 1) {
		DBG("Wrong mode\n");
	}

	RegSet16BitValueOff(0x110E, 0x0800);
	RegSet16BitValueOn(0x1116, 0x0005);
	RegSet16BitValueOn(0x114A, 0x0001);

	for (i = 0; i < 7; i++) {
		nTmpAddr = 0x3C + i;
		nTmpAddr = nTmpAddr * 2;
		nAddr = (0x11 << 8) | nTmpAddr;
		RegSet16BitValue(nAddr, MSG26XXM_open_ANA1_N_X[i]);
	}

	RegSet16BitValue(0x1E66, 0x0000);
	RegSet16BitValue(0x1E67, 0x0000);
	RegSet16BitValue(0x1E68, 0x0000);
	RegSet16BitValue(0x1E69, 0x0000);
	RegSet16BitValue(0x1E6A, 0x0000);
	RegSet16BitValue(0x1E6B, 0x0000);

	for (i = 0; i < 21; i++) {
		nTmpAddr = 3 + i;
		nTmpAddr = nTmpAddr * 2;
		nAddr = (0x10 << 8) | nTmpAddr;
		RegSet16BitValue(nAddr, MSG26XXM_open_ANA3_N_X[i]);
	}

	nDriveNumGeg = ((MSG26XXM_DRIVE_NUM - 1) << 8 & 0xFF00);
	nSenseNumGeg = (MSG26XXM_SENSE_NUM & 0x00FF);

	RegSet16BitValue(0x1216, nDriveNumGeg);
	RegSet16BitValue(0x102E, nSenseNumGeg);

	RegSet16BitValue(0x0FE6, 0x0001);

	DBG("Wrong mode correction\n");
}


void _ItoTestDebugShowArray(void *pBuf, u16 nLen, int nDataType, int nCarry, int nChangeLine)
{
	u8 *pU8Buf;
	s8 *pS8Buf;
	u16 *pU16Buf;
	s16 *pS16Buf;
	u32 *pU32Buf;
	s32 *pS32Buf;
	int i;

	if (nDataType == 8)
		pU8Buf = (u8 *)pBuf;
	else if (nDataType == -8)
		pS8Buf = (s8 *)pBuf;
	else if (nDataType == 16)
		pU16Buf = (u16 *)pBuf;
	else if (nDataType == -16)
		pS16Buf = (s16 *)pBuf;
	else if (nDataType == 32)
		pU32Buf = (u32 *)pBuf;
	else if (nDataType == -32)
		pS32Buf = (s32 *)pBuf;

	for (i = 0; i < nLen; i++) {
		if (nCarry == 16) {
			if (nDataType == 8)
				DBG("%02X ", pU8Buf[i]);
			else if (nDataType == -8)
				DBG("%02X ", pS8Buf[i]);
			else if (nDataType == 16)
				DBG("%04X ", pU16Buf[i]);
			else if (nDataType == -16)
				DBG("%04X ", pS16Buf[i]);
			else if (nDataType == 32)
				DBG("%08X ", pU32Buf[i]);
			else if (nDataType == -32)
				DBG("%08X ", pS32Buf[i]);
		} else if (nCarry == 10) {
			if (nDataType == 8)
				DBG("%6d ", pU8Buf[i]);
			else if (nDataType == -8)
				DBG("%6d ", pS8Buf[i]);
			else if (nDataType == 16)
				DBG("%6d ", pU16Buf[i]);
			else if (nDataType == -16)
				DBG("%6d ", pS16Buf[i]);
			else if (nDataType == 32)
				DBG("%6d ", pU32Buf[i]);
			else if (nDataType == -32)
				DBG("%6d ", pS32Buf[i]);
		}

		if (i%nChangeLine == nChangeLine-1) {
			DBG("\n");
		}
	}
	DBG("\n");
}

void _ItoTestDebugShowS32Array(s32 *pBuf, u16 nRow, u16 nCol)
{
	int i, j;

	for (j = 0; j < nRow; j++) {
		for (i = 0; i < nCol; i++) {
			DBG("%4d ", pBuf[i * nRow + j]);
		}
		DBG("\n");
	}
	DBG("\n");
}

static void _DrvMpTestItoTestMsg26xxmMcuStop(void)
{
	DBG("*** %s() ***\n", __func__);

	RegSet16BitValue(0x0FE6, 0x0001);
}

static void _DrvMpTestItoTestMsg26xxmAnaSwitchToMutual(void)
{
	u16 nTemp = 0;

	DBG("*** %s() ***\n", __func__);

	nTemp = RegGet16BitValue(0x114A);
	nTemp |= BIT0;
	RegSet16BitValue(0x114A, nTemp);
	nTemp = RegGet16BitValue(0x1116);
	nTemp |= (BIT2 | BIT0);
	RegSet16BitValue(0x1116, nTemp);
}

static u16 _DrvMpTestItoTestAnaGetMutualChannelNum(void)
{
	u16 nSenseLineNum = 0;
	u16 nRegData = 0;

	DBG("*** %s() ***\n", __func__);

	nRegData = RegGet16BitValue(0x102E);
	nSenseLineNum = nRegData & 0x000F;

	DBG("nSenseLineNum = %d\n", nSenseLineNum);

	return nSenseLineNum;
}

static u16 _DrvMpTestItoTestAnaGetMutualSubFrameNum(void)
{
	u16 nDriveLineNum = 0;
	u16 nRegData = 0;

	DBG("*** %s() ***\n", __func__);

	nRegData = RegGet16BitValue(0x1216);
	nDriveLineNum = ((nRegData & 0xFF00) >> 8) + 1;

	DBG("nDriveLineNum = %d\n", nDriveLineNum);

	return nDriveLineNum;
}

static void _DrvMpTestItoOpenTestMsg26xxmAnaSetMutualCSub(u16 nCSub)
{
	u16 i = 0;
	u8 szDbBusTxData[256] = {0};

	szDbBusTxData[0] = 0x10;
	szDbBusTxData[1] = 0x15;
	szDbBusTxData[2] = 0x00;

	for (i = 3; i < (3+ANA4_MUTUAL_CSUB_NUMBER); i++) {
		szDbBusTxData[i] = (u8)nCSub;
	}

	IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+ANA4_MUTUAL_CSUB_NUMBER);

	szDbBusTxData[0] = 0x10;
	szDbBusTxData[1] = 0x10;
	szDbBusTxData[2] = 0x40;

	for (i = 3; i < 3+ANA3_MUTUAL_CSUB_NUMBER; i++) {
		szDbBusTxData[i] = (u8)nCSub;
	}

	IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+ANA3_MUTUAL_CSUB_NUMBER);
}

static void _DrvMpTestItoTestMsg26xxmDisableFilterNoiseDetect(void)
{
	u16 nTemp = 0;

	DBG("*** %s() ***\n", __func__);

	nTemp = RegGet16BitValue(0x1302);
	nTemp &= (~(BIT2 | BIT1 | BIT0));
	RegSet16BitValue(0x1302, nTemp);
}

static void _DrvMpTestItoTestMsg26xxmAnaSwReset(void)
{
	DBG("*** %s() ***\n", __func__);

	RegSet16BitValue(0x1100, 0xFFFF);
	RegSet16BitValue(0x1100, 0x0000);
	mdelay(100);
}

static void _DrvMpTestItoTestMsg26xxmEnableAdcOneShot(void)
{
	DBG("*** %s() ***\n", __func__);

	RegSet16BitValue(0x130C, BIT15);
	RegSet16BitValue(0x1214, 0x0031);
}

static void _DrvMpTestItoTestGetMutualOneShotRawIir(u16 wszResultData[][MAX_CHANNEL_DRV], u16 nDriveLineNum, u16 nSenseLineNum)
{
	u16 nRegData;
	u16 i, j;
	u16 nTemp;
	u16 nReadSize;
	u8 szDbBusTxData[3];
	u8 szShotData1[FILTER1_MUTUAL_DELTA_C_NUMBER];
	u8 szShotData2[FILTER2_MUTUAL_DELTA_C_NUMBER];

	DBG("*** %s() ***\n", __func__);

	nTemp = RegGet16BitValue(0x3D08);
	nTemp &= (~(BIT8 | BIT4));
	RegSet16BitValue(0x3D08, nTemp);

	_DrvMpTestItoTestMsg26xxmEnableAdcOneShot();

	nRegData = 0;
	while (0x0000 == (nRegData & BIT8)) {
		nRegData = RegGet16BitValue(0x3D18);
	}

	for (i = 0; i < FILTER1_MUTUAL_DELTA_C_NUMBER; i++) {
		szShotData1[i] = 0;
	}

	for (i = 0; i < FILTER2_MUTUAL_DELTA_C_NUMBER; i++) {
		szShotData2[i] = 0;
	}

	mdelay(100);
	szDbBusTxData[0] = 0x10;
	szDbBusTxData[1] = 0x13;
	szDbBusTxData[2] = 0x42;
	IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3);
	IicReadData(SLAVE_I2C_ID_DBBUS, &szShotData1[0], FILTER1_MUTUAL_DELTA_C_NUMBER);

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	mdelay(100);
	nReadSize = IicSegmentReadDataByDbBus(0x20, 0x00, &szShotData2[0], FILTER2_MUTUAL_DELTA_C_NUMBER, MAX_I2C_TRANSACTION_LENGTH_LIMIT);
	DBG("*** nReadSize = %d ***\n", nReadSize);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM)
	mdelay(100);
	szDbBusTxData[0] = 0x10;
	szDbBusTxData[1] = 0x20;
	szDbBusTxData[2] = 0x00;
	IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3);
	IicReadData(SLAVE_I2C_ID_DBBUS, &szShotData2[0], FILTER2_MUTUAL_DELTA_C_NUMBER);
#endif

	for (j = 0; j < nDriveLineNum; j++) {
		for (i = 0; i < nSenseLineNum; i++) {

			if ((j <= 5) || ((j == 6) && (i <= 10))) {
				nRegData = (u16)(szShotData1[(j * 14 + i) * 2] | szShotData1[(j * 14 + i) * 2 + 1] << 8);
				wszResultData[i][ j] = (short)nRegData;
			} else {

				if ((j == 6) && (i > 10)) {
					nRegData = (u16)(szShotData2[((j - 6) * 14 + (i - 11)) * 2] | szShotData2[((j - 6) * 14 + (i - 11)) * 2 + 1] << 8);
					wszResultData[i][j] = (short)nRegData;
				} else {
					nRegData = (u16)(szShotData2[6 + ((j - 7) * 14 + i) * 2] | szShotData2[6 + ((j - 7) * 14 + i) * 2 + 1] << 8);
					wszResultData[i][j] = (short)nRegData;
				}
			}
		}
	}

	nTemp = RegGet16BitValue(0x3D08);
	nTemp |= (BIT8 | BIT4);
	RegSet16BitValue(0x3D08, nTemp);
}

static void _DrvMpTestItoTestMsg26xxmGetDeltaC(s32 *pTarget)
{
	s16 nTemp;
	u16 wszRawData[MAX_CHANNEL_SEN][MAX_CHANNEL_DRV];
	u16 i, j;
	u16 nDriveLineNum = 0, nSenseLineNum = 0, nShift = 0;

	DBG("*** %s() ***\n", __func__);

	nSenseLineNum = _DrvMpTestItoTestAnaGetMutualChannelNum();
	nDriveLineNum = _DrvMpTestItoTestAnaGetMutualSubFrameNum();

	_DrvMpTestItoTestGetMutualOneShotRawIir(wszRawData, nDriveLineNum, nSenseLineNum);

	for (i = 0; i < nSenseLineNum; i++) {
		for (j = 0; j < nDriveLineNum; j++) {
			nShift = (u16)(i * nDriveLineNum + j);
			nTemp = (s16)wszRawData[i][j];
			pTarget[nShift] = nTemp;

		}
	}
}

static s32 _DrvMpTestItoTestMsg26xxmReadTrunkFwVersion(u32 *pVersion)
{
	u16 nMajor = 0;
	u16 nMinor = 0;
	u8 szDbBusTxData[3] = {0};
	u8 szDbBusRxData[4] = {0};

	DBG("*** %s() ***\n", __func__);

	szDbBusTxData[0] = 0x53;
	szDbBusTxData[1] = 0x00;
	szDbBusTxData[2] = 0x24;

	IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 3);
	IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 4);

	DBG("szDbBusRxData[0] = 0x%x\n", szDbBusRxData[0]);
	DBG("szDbBusRxData[1] = 0x%x\n", szDbBusRxData[1]);
	DBG("szDbBusRxData[2] = 0x%x\n", szDbBusRxData[2]);
	DBG("szDbBusRxData[3] = 0x%x\n", szDbBusRxData[3]);

	nMajor = (szDbBusRxData[1]<<8) + szDbBusRxData[0];
	nMinor = (szDbBusRxData[3]<<8) + szDbBusRxData[2];

	DBG("*** major = %x ***\n", nMajor);
	DBG("*** minor = %x ***\n", nMinor);

	*pVersion = (nMajor << 16) + nMinor;

	return 0;
}

static s32 _DrvMpTestItoTestMsg26xxmGetSwitchFlag(void)
{
	u32 nFwVersion = 0;

	DBG("*** %s() ***\n", __func__);

	if (_DrvMpTestItoTestMsg26xxmReadTrunkFwVersion(&nFwVersion) < 0) {
		_gTestSwitchMode = 0;
		return 0;
	}

	if (nFwVersion >= 0x10030000)
		_gTestSwitchMode = 1;
	else
		_gTestSwitchMode = 0;

	return 0;
}

static s32 _DrvMpTestItoTestMsg26xxmCheckSwitchStatus(void)
{
	u32 nRegData = 0;
	int nTimeOut = 100;
	int nT = 0;

	DBG("*** %s() ***\n", __func__);

	do {
		nRegData = RegGet16BitValue(0x3CE4);
		mdelay(20);
		nT++;
		if (nT > nTimeOut)
			return -EPERM;
		DBG("*** %s() nRegData:%x***\n", __func__ , nRegData);

	} while (nRegData != 0x7447);

	return 0;
}

static s32 _DrvMpTestItoTestMsg26xxmSwitchFwMode(u8 nFWMode)
{
	DBG("*** %s() ***\n", __func__);

	RegSet16BitValue(0x0FE6, 0x0001);
	mdelay(150);

	RegSet16BitValue(0X3C60, 0xAA55);

	RegSet16BitValue(0X3D08, 0xFFFF);
	RegSet16BitValue(0X3D18, 0xFFFF);

	RegSet16BitValue(0x3CE4, 0x7474);


	RegSet16BitValue(0x1E04, 0x829F);
	RegSet16BitValue(0x0FE6, 0x0000);
	mdelay(150);

	if (_DrvMpTestItoTestMsg26xxmCheckSwitchStatus() < 0) {
		DBG("*** Msg26xx MP Test# CheckSwitchStatus failed! ***\n");
		return -EPERM;
	}

	switch (nFWMode) {
	case MUTUAL:
		RegSet16BitValue(0x3CE4, 0x5705);
		break;

	case SELF:
		RegSet16BitValue(0x3CE4, 0x6278);
		break;

	case WATERPROOF:
		RegSet16BitValue(0x3CE4, 0x7992);
		DBG("*** Msg26xx MP Test# WATERPROOF mode***\n");
		break;

	default:
		return -EPERM;
	}
	if (_DrvMpTestItoTestMsg26xxmCheckSwitchStatus() < 0) {
		DBG("*** Msg26xx MP Test# CheckSwitchStatus failed! ***\n");
		return -EPERM;
	}

	RegSet16BitValue(0x0FE6, 0x0001);
	RegSet16BitValue(0x3D08, 0xFEFF);

	return 0;
}

static s32 _DrvMpTestItoTestMsg26xxmSwitchMode(u8 nSwitchMode, u8 nFMode)
{
	if (_gTestSwitchMode != 0) {
		if (_DrvMpTestItoTestMsg26xxmSwitchFwMode(nFMode) < 0) {
			if (nFMode == MUTUAL) {
				_DrvMpTestItoTestMsg26xxmSetToNormalMode();
			} else {

				DBG("*** Msg26xx MP Test# _DrvMpTestItoTestMsg26xxmSwitchMode failed! ***\n");
				return -EPERM;
			}
		}
	} else {
		if (nFMode == MUTUAL) {
			_DrvMpTestItoTestMsg26xxmSetToNormalMode();
		} else {

			DBG("*** Msg26xx MP Test# _DrvMpTestItoTestMsg26xxmSwitchMode failed! ***\n");
			return -EPERM;
		}
	}

	return 0;
}

s32 _DrvMpTestMsg26xxmItoOpenTestEntry(void)
{
	s32 nRetVal = 0;
	s32 nPrev = 0, nDelta = 0;
	u16 i = 0, j = 0;

	DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaReset();
#endif
#endif

	DrvPlatformLyrDisableFingerTouchReport();

	DrvPlatformLyrTouchDeviceResetHw();


	if (_gTestAutoSwitchFlag != 0) {
		_DrvMpTestItoTestMsg26xxmGetSwitchFlag();
	}

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	_DrvMpTestItoTestMsg26xxmSwitchMode(_gTestSwitchMode, MUTUAL);

	_DrvMpTestItoTestMsg26xxmMcuStop();
	mdelay(10);

	for (i = 0; i < MAX_MUTUAL_NUM; i++) {
		_gTestFailChannel[i] = 0;
	}

	_gTestFailChannelCount = 0;

	_gSenseLineNum = _DrvMpTestItoTestAnaGetMutualChannelNum();
	_gDriveLineNum = _DrvMpTestItoTestAnaGetMutualSubFrameNum();

	_DrvMpTestItoOpenTestMsg26xxmAnaSetMutualCSub(MSG26XXM_OPEN_CSUB_REF_X);
	_DrvMpTestItoTestMsg26xxmDisableFilterNoiseDetect();


	RegSet16BitValue(0x1224, 0xFFC0);
	RegSet16BitValue(0x122A, 0x0C0A);

	_DrvMpTestItoTestMsg26xxmAnaSwReset();
	_DrvMpTestItoTestMsg26xxmGetDeltaC(_gDeltaC);

	for (i = 0; i < _gSenseLineNum; i++) {
		DBG("\nSense[%02d]\t", i);

		for (j = 0; j < _gDriveLineNum; j++) {
			_gResult[i * _gDriveLineNum + j] = (4464*MSG26XXM_OPEN_CSUB_REF_X - _gDeltaC[i * _gDriveLineNum + j]);
			DBG("%d  %d  %d\t", _gResult[i * _gDriveLineNum + j], 4464*MSG26XXM_OPEN_CSUB_REF_X, _gDeltaC[i * _gDriveLineNum + j]);
		}
	}

	DBG("\n\n\n");


	for (j = 0; j < (_gDriveLineNum-1); j++) {
		for (i = 0; i < _gSenseLineNum; i++) {
			if (_gResult[i * _gDriveLineNum + j] < FIR_THRESHOLD) {
				_gTestFailChannel[i * _gDriveLineNum + j] = 1;
				_gTestFailChannelCount++;
				nRetVal = -1;
				DBG("\nSense%d, Drive%d, MIN_Threshold = %d\t", i, j, _gResult[i * _gDriveLineNum + j]);
			}

			if (i > 0) {
				nDelta = _gResult[i * _gDriveLineNum + j] > nPrev ? (_gResult[i * _gDriveLineNum + j] - nPrev) : (nPrev - _gResult[i * _gDriveLineNum + j]);
				if (nDelta > nPrev*FIR_RATIO/100) {
					if (0 == _gTestFailChannel[i * _gDriveLineNum + j]) {
						_gTestFailChannel[i * _gDriveLineNum + j] = 1;
						_gTestFailChannelCount++;
					}
					nRetVal = -1;
					DBG("\nSense%d, Drive%d, MAX_Ratio = %d, %d\t", i, j, nDelta, nPrev);
				}
			}
			nPrev = _gResult[i * _gDriveLineNum + j];
		}
	}

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	DrvPlatformLyrTouchDeviceResetHw();
	mdelay(300);

	DrvPlatformLyrEnableFingerTouchReport();

	return nRetVal;
}

static void _DrvMpTestItoOpenTestSetMutualCsubViaDBbus(s16 nCSub)
{
	u8 nBaseLen = 6;
	u16 nFilter = 0x3F;
	u16 nLastFilter = 0xFFF;
	u8 nBasePattern = nCSub & nFilter;
	u8 nPattern;
	u16 n16BitsPattern;
	u16 nCSub16Bits[5] = {0};
	int i;

	DBG("*** %s() ***\n", __func__);

	for (i = 0; i < 5; i++) {
		if (i == 0) {
			nPattern = nBasePattern;
		}

		n16BitsPattern = ((nPattern & 0xF) << nBaseLen*2) | (nPattern << nBaseLen) | nPattern;

		if (i == 4) {
			nCSub16Bits[i] = n16BitsPattern & nLastFilter;
		} else {
			nCSub16Bits[i] = n16BitsPattern;
		}
		nPattern = (u8)((n16BitsPattern >> 4) & nFilter);
	}

	RegSet16BitValue(0x215C, 0x1FFF);

	for (i = 0; i < 5; i++) {
		RegSet16BitValue(0x2148 + 2 * i, nCSub16Bits[i]);
		RegSet16BitValue(0x2152 + 2 * i, nCSub16Bits[i]);
	}
}


static void _DrvMpTestItoOpenTestAFEGainOne(void)
{

	u16 nAfeGain = 0;
	u16 nDriOpening = 0;
	u8 nRegData = 0;
	u16 nAfeCoef = 0;
	u16 i = 0;



	nRegData = RegGetLByteValue(0x1312);
	nDriOpening = nRegData;

	/
	if (nDriOpening == 11 || nDriOpening == 15) {
		RegSet16BitValue(0x1318, 0x4470);
	} else if (nDriOpening == 7) {
		RegSet16BitValue(0x1318, 0x4460);
	}


	RegSet16BitValue(0x131A, 0x4444);

	/

	nRegData = RegGetLByteValue(0x101A);
	nAfeCoef = 0x10000 / nRegData;

	RegSet16BitValue(0x13D6, nAfeCoef);

	/
	if (nDriOpening == 7 || nDriOpening == 15) {
		nAfeGain = 0x0040;
	} else if (nDriOpening == 11) {
		nAfeGain = 0x0055;
	}

	for (i = 0; i < 13; i++) {
		RegSet16BitValue(0x2160 + 2 * i, nAfeGain);
	}

	/
	RegSet16BitValue(0x217A, 0x1FFF);
	RegSet16BitValue(0x217C, 0x1FFF);

	/
	RegSet16BitValue(0x1508, 0x1FFF);
	RegSet16BitValue(0x1550, 0x0000);

	/
	RegSet16BitValue(0x1564, 0x0077);

	/
	RegSet16BitValue(0x1260, 0x1FFF);
}

static void _DrvMpTestItoOpenTestCalibrateMutualCsub(s16 nCSub)
{
	u8 nChipVer;

	DBG("*** %s() ***\n", __func__);

	nChipVer = RegGetLByteValue(0x1ECE);
	DBG("*** Msg28xx Open Test# Chip ID = %d ***\n", nChipVer);

	if (nChipVer != 0)
		RegSet16BitValue(0x10F0, 0x0004);

	_DrvMpTestItoOpenTestSetMutualCsubViaDBbus(nCSub);
	_DrvMpTestItoOpenTestAFEGainOne();
}

static void _DrvMpTestItoTestDBBusReadDQMemStart(void)
{
	u8 nParCmdSelUseCfg = 0x7F;
	u8 nParCmdAdByteEn0 = 0x50;
	u8 nParCmdAdByteEn1 = 0x51;
	u8 nParCmdDaByteEn0 = 0x54;
	u8 nParCmdUSetSelB0 = 0x80;
	u8 nParCmdUSetSelB1 = 0x82;
	u8 nParCmdSetSelB2  = 0x85;
	u8 nParCmdIicUse	= 0x35;


	DBG("*** %s() ***\n", __func__);

	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdSelUseCfg, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdAdByteEn0, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdAdByteEn1, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdDaByteEn0, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdUSetSelB0, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdUSetSelB1, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdSetSelB2, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdIicUse, 1);
}


static void _DrvMpTestItoTestDBBusReadDQMemEnd(void)
{
	u8 nParCmdNSelUseCfg = 0x7E;

	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdNSelUseCfg, 1);
}


static void _DrvMpTestItoTestMsg28xxEnableAdcOneShot(void)
{
	RegSet16BitValueOn(0x100a, BIT0);

	return;
}

static s32 _DrvMpTestItoTestMsg28xxTriggerMutualOneShot(s16 *pResultData, u16 *pSenNum, u16 *pDrvNum)
{
	u16 nAddr = 0x5000, nAddrNextSF = 0x1A4;
	u16 nSF = 0, nAfeOpening = 0, nDriOpening = 0;
	u16 nMaxDataNumOfOneSF = 0;
	u16 nDriMode = 0;
	int nDataShift = -1;
	u16 i, j, k;
	u8 nRegData = 0;
	u8 nShotData[392] = {0};
	u16 nRegDataU16 = 0;
	s16 *pShotDataAll = NULL;

	DBG("*** %s() ***\n", __func__);

	nRegData = RegGetLByteValue(0x130A);
	nSF = nRegData >> 4;
	nAfeOpening = nRegData & 0x0f;

	if (nSF == 0) {
		return -EPERM;
	}

	nRegData = RegGetLByteValue(0x100B);
	nDriMode = nRegData;

	nRegData = RegGetLByteValue(0x1312);
	nDriOpening = nRegData;

	DBG("*** Msg28xx MP Test# TriggerMutualOneShot nSF=%d, nAfeOpening=%d, nDriMode=%d, nDriOpening=%d. ***\n", nSF, nAfeOpening, nDriMode, nDriOpening);

	nMaxDataNumOfOneSF = nAfeOpening * nDriOpening;

	pShotDataAll = kzalloc(sizeof(s16) * nSF * nMaxDataNumOfOneSF, GFP_KERNEL);

	RegSet16BitValueOff(0x3D08, BIT8);	  /

	/
	_DrvMpTestItoTestMsg28xxEnableAdcOneShot();

	while (0x0000 == (nRegDataU16 & BIT8)) {
		nRegDataU16 = RegGet16BitValue(0x3D18);
	}

	if (nDriMode == 2) {
		if (nAfeOpening % 2 == 0)
			nDataShift = -1;
		else
			nDataShift = 0;
		/
		for (i = 0; i < nSF; i++) {
			_DrvMpTestItoTestDBBusReadDQMemStart();
			RegGetXBitValue(nAddr + i * nAddrNextSF, nShotData, 28, MAX_I2C_TRANSACTION_LENGTH_LIMIT);
			_DrvMpTestItoTestDBBusReadDQMemEnd();


			for (j = 0; j < nAfeOpening; j++) {
				pResultData[i*MAX_CHANNEL_DRV+j] = (s16)(nShotData[2 * j] | nShotData[2 * j + 1] << 8);

				if (nDataShift == 0 && (j == nAfeOpening-1)) {
					pResultData[i*MAX_CHANNEL_DRV+j] = (s16)(nShotData[2 * (j + 1)] | nShotData[2 * (j + 1) + 1] << 8);
				}
			}
		}

		*pSenNum = nSF;
		*pDrvNum = nAfeOpening;
	} else {


		if (nAfeOpening % 2 == 0 || nDriOpening % 2 == 0)
			nDataShift = -1;
		else
			nDataShift = 0;

		/
		for (i = 0; i < nSF; i++) {
			_DrvMpTestItoTestDBBusReadDQMemStart();
			RegGetXBitValue(nAddr + i * nAddrNextSF, nShotData, 392, MAX_I2C_TRANSACTION_LENGTH_LIMIT);
			_DrvMpTestItoTestDBBusReadDQMemEnd();


			for (j = 0; j < nMaxDataNumOfOneSF; j++) {
				pShotDataAll[i*nMaxDataNumOfOneSF+j] = (s16)(nShotData[2 * j] | nShotData[2 * j + 1] << 8);

				if (nDataShift == 0 && j == (nMaxDataNumOfOneSF - 1))
					pShotDataAll[i*nMaxDataNumOfOneSF+j] = (s16)(nShotData[2 * (j + 1)] | nShotData[2 * (j + 1) + 1] << 8);
			}
		}


		for (k = 0; k < nSF; k++) {
			for (i = k * nAfeOpening; i < nAfeOpening * (k + 1); i++) {
				for (j = 0; j < nDriOpening; j++) {
					pResultData[i*MAX_CHANNEL_DRV+j] = pShotDataAll[k*nMaxDataNumOfOneSF + (j + (i - nAfeOpening * k) * nDriOpening)];
				}
			}
		}

		*pSenNum = nSF * nAfeOpening;
		*pDrvNum = nDriOpening;
	}
	RegSet16BitValueOn(0x3D08, BIT8);	  /
	RegSet16BitValueOn(0x3D08, BIT4);	  /

	kfree(pShotDataAll);

	return 0;
}

static s32 _DrvMpTestItoTestMsg28xxGetMutualOneShotRawIIR(s16 *nResultData, u16 *pSenNum, u16 *pDrvNum)
{
	return _DrvMpTestItoTestMsg28xxTriggerMutualOneShot(nResultData, pSenNum, pDrvNum);
}

static s32 _DrvMpTestItoTestMsg28xxGetDeltaC(s32 *pDeltaC)
{
	s16 *pRawData = NULL;
	s16 nRawDataOverlapDone[SENSE_NUM][DRIVE_NUM] = {{0} };

	u16 nDrvPos = 0, nSenPos = 0, nShift = 0;
	u16 nSenNumBak = 0;
	u16 nDrvNumBak = 0;
	s16 i, j;

	DBG("*** %s() ***\n", __func__);

	pRawData = kzalloc(sizeof(s16) * MAX_CHANNEL_SEN*2 * MAX_CHANNEL_DRV, GFP_KERNEL);

	if (_DrvMpTestItoTestMsg28xxGetMutualOneShotRawIIR(pRawData, &nSenNumBak, &nDrvNumBak) < 0) {
		DBG("*** Msg28xx Open Test# GetMutualOneShotRawIIR failed! ***\n");
		return -EPERM;
	}

	DBG("*** Msg28xx Open Test# nSenNumBak=%d nDrvNumBak=%d ***\n", nSenNumBak, nDrvNumBak);

	for (i = 0; i < nSenNumBak; i++) {
		for (j = 0; j < nDrvNumBak; j++) {
			nShift = (u16)(i * nDrvNumBak + j);
			nDrvPos = g_MapVaMutual[nShift][1];
			nSenPos = g_MapVaMutual[nShift][0];
			if (nDrvPos != 0xFF && nSenPos != 0xFF)
				nRawDataOverlapDone[nSenPos][nDrvPos] = pRawData[i*MAX_CHANNEL_DRV+j];
		}
	}

	for (i = 0; i < _gSenseLineNum; i++) {
		for (j = 0; j < _gDriveLineNum; j++) {
			nShift = (u16)(i * _gDriveLineNum + j);
			pDeltaC[nShift] = (s32)nRawDataOverlapDone[i][j];
		}
	}

	DBG("*** Msg28xx Open Test# gDeltaC ***\n");
	_ItoTestDebugShowArray(pDeltaC, _gSenseLineNum * _gDriveLineNum, -32, 10, _gSenseLineNum);

	kfree(pRawData);

	return 0;
}

static void _DrvMpTestItoTestMsg28xxAnaSwReset(void)
{
	DBG("*** %s() ***\n", __func__);

	/
	RegSet16BitValueOn(0x1002, (BIT0 | BIT1 | BIT2 | BIT3));	 /
	RegSet16BitValueOff(0x1002, (BIT0 | BIT1 | BIT2 | BIT3));

	/
	mdelay(20);
}

static s32 _DrvMpTestMsg28xxItoOpenTest(void)
{
	DBG("*** %s() ***\n", __func__);


	RegSet16BitValue(0x0FE6, 0x0001);

	_DrvMpTestItoOpenTestCalibrateMutualCsub(CSUB_REF);
	RegSet16BitValue(0x156A, 0x000A); /
	_DrvMpTestItoTestMsg28xxAnaSwReset();


	if (_DrvMpTestItoTestMsg28xxGetDeltaC(_gDeltaC) < 0) {
		DBG("*** Msg28xx Open Test# GetDeltaC failed! ***\n");
		return -EPERM;
	}

	return 0;
}

static u8 _DrvMpTestItoTestCheckValueInRange(s32 nValue, s32 nMax, s32 nMin)
{
	if (nValue <= nMax && nValue >= nMin)
		return 1;
	else
		return 0;
}

static s32 _DrvMpTestItoOpenTestMsg28xxOpenJudge(u16 nItemID, s8 pNormalTestResult[][2], u16 pNormalTestResultCheck[][13]/*, u16 nDriOpening*/)
{
	s32 nRetVal = 0;
	u16 nCSub = CSUB_REF;
	u16 nRowNum = 0, nColumnNum = 0;
	u32 nSum = 0, nAvg = 0, nDelta = 0, nPrev = 0;
	u16 i, j, k;

	DBG("*** %s() ***\n", __func__);

	for (i = 0; i < _gSenseLineNum * _gDriveLineNum; i++) {


		if (_gDeltaC[i] > 31000) {
			return -EPERM;
		}

		_gResult[i] = 1673 * nCSub - _gDeltaC[i];


		if ((MUTUAL_KEY == 1 || MUTUAL_KEY == 2) && (KEY_NUM != 0)) {
			if ((_gSenseLineNum < _gDriveLineNum) && ((i + 1) % _gDriveLineNum == 0)) {
				_gResult[i] = -32000;
				for (k = 0; k < KEY_NUM; k++)
					if ((i + 1) / _gDriveLineNum == KEYSEN[k]) {

						_gResult[i] = 1673 * nCSub - _gDeltaC[i];
					}
			}

			if ((_gSenseLineNum > _gDriveLineNum) && (i > (_gSenseLineNum - 1) * _gDriveLineNum - 1)) {
				_gResult[i] = -32000;
				for (k = 0; k < KEY_NUM; k++)
					if (((i + 1) - (_gSenseLineNum - 1) * _gDriveLineNum) == KEYSEN[k]) {
						_gResult[i] = 1673 * nCSub - _gDeltaC[i];
					}
			}
		}
	}

	if (_gDriveLineNum >= _gSenseLineNum) {
		nRowNum = _gDriveLineNum;
		nColumnNum = _gSenseLineNum;
	} else {
		nRowNum = _gSenseLineNum;
		nColumnNum = _gDriveLineNum;
	}

	DBG("*** Msg28xx Open Test# Show _gResult ***\n");

	_ItoTestDebugShowArray(_gResult, nRowNum*nColumnNum, -32, 10, nColumnNum);


	for (j = 0; j < (nRowNum-1); j++) {
		nSum = 0;
		for (i = 0; i < nColumnNum; i++) {
			 nSum = nSum + _gResult[i * nRowNum + j];
		}

		nAvg = nSum / nColumnNum;

		for (i = 0; i < nColumnNum; i++) {
			if (0 == _DrvMpTestItoTestCheckValueInRange(_gResult[i * nRowNum + j], (s32)(nAvg + nAvg * DC_RANGE/100), (s32)(nAvg - nAvg * DC_RANGE/100))) {
				_gTestFailChannel[i * nRowNum + j] = 1;
				_gTestFailChannelCount++;
				nRetVal = -1;
			}

			if (i > 0) {
				nDelta = _gResult[i * nRowNum + j] > nPrev ? (_gResult[i * nRowNum + j] - nPrev) : (nPrev - _gResult[i * nRowNum + j]);
				if (nDelta > nPrev*FIR_RATIO/100) {
					if (0 == _gTestFailChannel[i * nRowNum + j]) {
						_gTestFailChannel[i * nRowNum + j] = 1;
						_gTestFailChannelCount++;
					}
					nRetVal = -1;
					DBG("\nSense%d, Drive%d, MAX_Ratio = %d, %d\t", i, j, nDelta, nPrev);
				}
			}
			nPrev = _gResult[i * nRowNum + j];
		}
	}


	return nRetVal;
}

static s32 _DrvMpTestItoTestCheckSwitchStatus(void)
{
	u32 nRegData = 0;
	int nTimeOut = 100;
	int nT = 0;

	do {
		nRegData = RegGet16BitValue(0x1402);
		mdelay(20);
		nT++;
		if (nT > nTimeOut) {
			return -EPERM;
		}

	} while (nRegData != 0x7447);

	return 0;
}

static s32 _DrvMpTestMsg28xxItoTestSwitchFwMode(u8 nFMode)
{
	DBG("*** %s() ***\n", __func__);

	mdelay(100);
	RegSet16BitValue(0x0FE6, 0x0001);
	RegSet16BitValue(0X3C60, 0xAA55);

	RegSet16BitValue(0X3D08, 0xFFFF);
	RegSet16BitValue(0X3D18, 0xFFFF);

	RegSet16BitValue(0x1402, 0x7474);


	RegSet16BitValue(0x1E04, 0x829F);
	RegSet16BitValue(0x0FE6, 0x0000);
	mdelay(150);

	if (_DrvMpTestItoTestCheckSwitchStatus() < 0) {
		DBG("*** Msg28xx MP Test# CheckSwitchStatus failed! ***\n");
		return -EPERM;
	}

	switch (nFMode) {
	case MUTUAL:
		RegSet16BitValue(0x1402, 0x5705);
		break;

	case SELF:
		RegSet16BitValue(0x1402, 0x6278);
		break;

	case WATERPROOF:
		RegSet16BitValue(0x1402, 0x7992);
		break;

	case MUTUAL_SINGLE_DRIVE:
		RegSet16BitValue(0x1402, 0x0158);
		break;

	default:
		return -EPERM;
	}
	if (_DrvMpTestItoTestCheckSwitchStatus() < 0) {
		DBG("*** Msg28xx MP Test# CheckSwitchStatus failed! ***\n");
		return -EPERM;
	}

	RegSet16BitValue(0x0FE6, 0x0001);
	RegSet16BitValue(0x3D08, 0xFEFF);

	return 0;
}

s32 _DrvMpTestMsg28xxItoOpenTestEntry(void)
{
	s32 nRetVal = 0;


	s8 nNormalTestResult[8][2] = {{0} };
	u16 nNormalTestResultCheck[6][13] = {{0} };

	DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaReset();
#endif
#endif

	_gSenseLineNum = SENSE_NUM;
	_gDriveLineNum = DRIVE_NUM;

	DrvPlatformLyrDisableFingerTouchReport();
	DrvPlatformLyrTouchDeviceResetHw();


	DbBusResetSlave();
	DbBusEnterSerialDebugMode();

	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);


	RegSet16BitValue(0x0FE6, 0x0001);

	if (_DrvMpTestMsg28xxItoTestSwitchFwMode(MUTUAL) < 0) {
		DBG("*** Msg28xx Open Test# SwitchFwMode failed! ***\n");
		return -EPERM;
	}



	if (_DrvMpTestMsg28xxItoOpenTest() < 0) {
		DBG("*** Msg28xx Open Test# OpenTest failed! ***\n");
		return -EPERM;
	}

	mdelay(10);

	nRetVal = _DrvMpTestItoOpenTestMsg28xxOpenJudge(0, nNormalTestResult, nNormalTestResultCheck/*, nDrvOpening*/);
	DBG("*** Msg28xx Open Test# OpenTestOpenJudge return value = %d ***\n", nRetVal);

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	DrvPlatformLyrTouchDeviceResetHw();
	mdelay(300);

	DrvPlatformLyrEnableFingerTouchReport();

	return nRetVal;
}

s32 _DrvMpTestItoOpenTest(void)
{
	s32 nRetVal = -1;

	DBG("*** %s() ***\n", __func__);

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		nRetVal = _DrvMpTestMsg26xxmItoOpenTestEntry();
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		nRetVal = _DrvMpTestMsg28xxItoOpenTestEntry();
	}

	return nRetVal;
}

static void _DrvMpTestItoTestSendDataIn(u16 nAddr, u16 nLength, u16 *data)
{
	u8 szDbBusTxData[256] = {0};
	int i = 0;

	szDbBusTxData[0] = 0x10;
	szDbBusTxData[1] = (nAddr >> 8) & 0xFF;
	szDbBusTxData[2] = (nAddr & 0xFF);

	for (i = 0; i <= nLength ; i++) {
		szDbBusTxData[3+2*i] = (data[i] & 0xFF);
		szDbBusTxData[4+2*i] = (data[i] >> 8) & 0xFF;
	}

	IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+nLength*2);
}

static void _DrvMpTestItoShortTestMsg26xxmSetPAD2GPO(u8 nItemID)
{
	u16 gpioSetting[MAX_CHANNEL_NUM] = {0};
	u16 gpioEnabling[MAX_CHANNEL_NUM] = {0};
	u16 gpioZero[MAX_CHANNEL_NUM] = {0};
	u8 	gpioNum = 0;
	u16 *gpioPIN = NULL;
	int i = 0;
	int j = 0;

	DBG("*** %s() ***\n", __func__);

	if (nItemID == 1) {
		gpioNum = SHORT_N1_GPO_NUMBER_X;
		gpioPIN = kzalloc(sizeof(u16) * gpioNum, GFP_KERNEL);

		for (i = 0; i < gpioNum; i++) {
			gpioPIN[i] = SHORT_N1_GPO_PIN_X[i];
		}
	} else if (nItemID == 2) {
		gpioNum = SHORT_N2_GPO_NUMBER_X;
		gpioPIN = kzalloc(sizeof(u16) * gpioNum, GFP_KERNEL);

		for (i = 0; i < gpioNum; i++) {
			gpioPIN[i] = SHORT_N2_GPO_PIN_X[i];
		}
	} else if (nItemID == 3) {
		gpioNum = SHORT_S1_GPO_NUMBER_X;
		gpioPIN = kzalloc(sizeof(u16) * gpioNum, GFP_KERNEL);

		for (i = 0; i < gpioNum; i++) {
			gpioPIN[i] = SHORT_S1_GPO_PIN_X[i];
		}
	} else if (nItemID == 4) {
		gpioNum = SHORT_S2_GPO_NUMBER_X;
		gpioPIN = kzalloc(sizeof(u16) * gpioNum, GFP_KERNEL);

		for (i = 0; i < gpioNum; i++) {
			gpioPIN[i] = SHORT_S2_GPO_PIN_X[i];
		}
	}
	DBG("ItemID %d, gpioNum %d", nItemID, gpioNum);

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		gpioEnabling[i] = 0xFFFF;
	}

	for (i = 0; i < gpioNum; i++) {
		gpioSetting[gpioPIN[i] / 16] |= (u16)(1 << (gpioPIN[i] % 16));
		gpioEnabling[gpioPIN[i] / 16] &= (u16)(~(1 << (gpioPIN[i] % 16)));
	}

	/
	{
		_DrvMpTestItoTestSendDataIn(0x1E66, gpioNum, &gpioSetting[0]);   /
		_DrvMpTestItoTestSendDataIn(0x1E6C, gpioNum, &gpioEnabling[0]);   /
		_DrvMpTestItoTestSendDataIn(0x1E72, gpioNum, &gpioZero[0]);	   /
		_DrvMpTestItoTestSendDataIn(0x1E78, gpioNum, &gpioZero[0]);	   /
	}

	for (j = 0; j < gpioNum; j++) {
		if (PIN_GUARD_RING == gpioPIN[j]) {
			u16 u16RegData;
			u16RegData = RegGet16BitValue(0x1E12);
			u16RegData = ((u16RegData & 0xFFF9) | BIT0);
			RegSet16BitValue(0x1E12, u16RegData);
		}
	}

	kfree(gpioPIN);
}

static void _DrvMpTestItoShortTestMsg26xxmChangeANASetting(u8 nItemID)
{
	u16 SHORT_MAP_ANA1[7] = {0};
	u16 SHORT_MAP_ANA2[1] = {0};
	u16 SHORT_MAP_ANA3[21] = {0};
	int i = 0;

	DBG("*** %s() ***\n", __func__);

	if (nItemID == 1) {
		for (i = 0; i < 7; i++) {
			SHORT_MAP_ANA1[i] = short_ANA1_N1_X[i];
		}

		for (i = 0; i < 21; i++) {
			SHORT_MAP_ANA3[i] = short_ANA3_N1_X[i];
		}

		SHORT_MAP_ANA2[0] = short_ANA2_N1_X[0];
	} else if (nItemID == 2) {
		for (i = 0; i < 7; i++) {
			SHORT_MAP_ANA1[i] = short_ANA1_N2_X[i];
		}

		for (i = 0; i < 21; i++) {
			SHORT_MAP_ANA3[i] = short_ANA3_N2_X[i];
		}

		SHORT_MAP_ANA2[0] = short_ANA2_N2_X[0];
	} else if (nItemID == 3) {
		for (i = 0; i < 7; i++) {
			SHORT_MAP_ANA1[i] = short_ANA1_S1_X[i];
		}

		for (i = 0; i < 21; i++) {
			SHORT_MAP_ANA3[i] = short_ANA3_S1_X[i];
		}

		SHORT_MAP_ANA2[0] = short_ANA2_S1_X[0];
	} else if (nItemID == 4) {
		for (i = 0; i < 7; i++) {
			SHORT_MAP_ANA1[i] = short_ANA1_S2_X[i];
		}

		for (i = 0; i < 21; i++) {
			SHORT_MAP_ANA3[i] = short_ANA3_S2_X[i];
		}

		SHORT_MAP_ANA2[0] = short_ANA2_S2_X[0];
	}

	/
	{
		_DrvMpTestItoTestSendDataIn(0x1178, 7, &SHORT_MAP_ANA1[0]);		/
		_DrvMpTestItoTestSendDataIn(0x1216, 1, &SHORT_MAP_ANA2[0]);   	/
		_DrvMpTestItoTestSendDataIn(0x1006, 21, &SHORT_MAP_ANA3[0]);	/
	}
}

static void _DrvMpTestItoShortTestMsg26xxmAnaFixPrs(u16 nOption)
{
	u16 nTemp = 0;

	DBG("*** %s() ***\n", __func__);

	nTemp = RegGet16BitValue(0x1208);
	nTemp &= 0x00F1;
	nTemp |= (u16)((nOption << 1) & 0x000E);
	RegSet16BitValue(0x1208, nTemp);
}

static void _DrvMpTestItoShortTestMsg26xxmSetNoiseSensorMode(u8 nEnable)
{
	DBG("*** %s() ***\n", __func__);

	if (nEnable) {
		RegSet16BitValueOn(0x110E, BIT11);
		RegSet16BitValueOff(0x1116, BIT2);
	} else {
		RegSet16BitValueOff(0x110E, BIT11);
	}
}

static void _DrvMpTestItoShortTestMsg26xxmAndChangeCDtime(u16 nTime1, u16 nTime2)
{
	DBG("*** %s() ***\n", __func__);

	RegSet16BitValue(0x1224, nTime1);
	RegSet16BitValue(0x122A, nTime2);
}

static void _DrvMpTestMsg26xxmItoShortTest(u8 nItemID)
{
	int i;

	DBG("*** %s() ***\n", __func__);

	_DrvMpTestItoTestMsg26xxmMcuStop();
	_DrvMpTestItoShortTestMsg26xxmSetPAD2GPO(nItemID);
	_DrvMpTestItoShortTestMsg26xxmChangeANASetting(nItemID);
	_DrvMpTestItoTestMsg26xxmAnaSwitchToMutual();
	_DrvMpTestItoShortTestMsg26xxmAnaFixPrs(7);
	_DrvMpTestItoTestMsg26xxmDisableFilterNoiseDetect();
	_DrvMpTestItoShortTestMsg26xxmSetNoiseSensorMode(1);
	_DrvMpTestItoShortTestMsg26xxmAndChangeCDtime(SHORT_Charge_X, SHORT_Dump1_X);
	_DrvMpTestItoTestMsg26xxmAnaSwReset();
	_DrvMpTestItoTestMsg26xxmGetDeltaC(_gTempDeltaC);

	_DrvMpTestItoTestMsg26xxmMcuStop();
	_DrvMpTestItoShortTestMsg26xxmSetPAD2GPO(nItemID);
	_DrvMpTestItoShortTestMsg26xxmChangeANASetting(nItemID);
	_DrvMpTestItoTestMsg26xxmAnaSwitchToMutual();
	_DrvMpTestItoShortTestMsg26xxmAnaFixPrs(7);
	_DrvMpTestItoTestMsg26xxmDisableFilterNoiseDetect();
	_DrvMpTestItoShortTestMsg26xxmSetNoiseSensorMode(1);
	_DrvMpTestItoShortTestMsg26xxmAndChangeCDtime(SHORT_Charge_X, SHORT_Dump2_X);
	_DrvMpTestItoTestMsg26xxmAnaSwReset();
	_DrvMpTestItoTestMsg26xxmGetDeltaC(_gDeltaC);

	for (i = 0; i < MAX_MUTUAL_NUM ; i++) {
		if ((_gDeltaC[i] <= -(IIR_MAX)) || (_gTempDeltaC[i] <= -(IIR_MAX)) || (_gDeltaC[i] >= (IIR_MAX)) || (_gTempDeltaC[i] >= (IIR_MAX))) {
			_gDeltaC[i] = 0x7FFF;
		} else {
			_gDeltaC[i] = abs(_gDeltaC[i] - _gTempDeltaC[i]);
		}

	}
	DBG("\n");
}

static s32 _DrvMpTestItoShortTestCovertRValue(s32 nValue)
{
	if (nValue == 0) {
		nValue = 1;
	}

	if (nValue >= IIR_MAX) {
		return 0;
	}

	return (500*11398) / (nValue);
}

static ItoTestResult_e _DrvMpTestItoShortTestMsg26xxmJudge(u8 nItemID)
{
	ItoTestResult_e nRetVal = ITO_TEST_OK;
	u8 nTestPinLength = 0;
	u16 i = 0;
	u8 nGpioNum = 0;
	u8 *nTestGpio = NULL;

	DBG("*** %s() ***\n", __func__);

	if (nItemID == 1) {
		nGpioNum = SHORT_N1_TEST_NUMBER_X;
		nTestGpio = kzalloc(sizeof(u16) * nGpioNum, GFP_KERNEL);

		for (i = 0; i < nGpioNum; i++) {
			nTestGpio[i] = SHORT_N1_TEST_PIN_X[i];
		}
	} else if (nItemID == 2) {
		nGpioNum = SHORT_N2_TEST_NUMBER_X;
		nTestGpio = kzalloc(sizeof(u16) * nGpioNum, GFP_KERNEL);

		for (i = 0; i < nGpioNum; i++) {
			nTestGpio[i] = SHORT_N2_TEST_PIN_X[i];
		}
	} else if (nItemID == 3) {
		nGpioNum = SHORT_S1_TEST_NUMBER_X;
		nTestGpio = kzalloc(sizeof(u16) * nGpioNum, GFP_KERNEL);

		for (i = 0; i < nGpioNum; i++) {
			nTestGpio[i] = SHORT_S1_TEST_PIN_X[i];
		}
	} else if (nItemID == 4) {
		nGpioNum = SHORT_S2_TEST_NUMBER_X;
		nTestGpio = kzalloc(sizeof(u16) * nGpioNum, GFP_KERNEL);

		for (i = 0; i < nGpioNum; i++) {
			nTestGpio[i] = SHORT_S2_TEST_PIN_X[i];
		}
	}

	nTestPinLength = nGpioNum;

	for (i = 0; i < nTestPinLength; i++) {
		_gShortTestChannel[i] = nTestGpio[i];

		if (0 == _DrvMpTestItoTestCheckValueInRange(_gDeltaC[i], SHORT_VALUE, -SHORT_VALUE)) {
			nRetVal = ITO_TEST_FAIL;
			_gTestFailChannelCount++;
			DBG("_gShortTestChannel i = %d, _gDeltaC = %d\t", i, _gDeltaC[i]);
		}
	}
	kfree(nTestGpio);

	return nRetVal;
}

static ItoTestResult_e _DrvMpTestMsg26xxmItoShortTestEntry(void)
{
	ItoTestResult_e nRetVal1 = ITO_TEST_OK, nRetVal2 = ITO_TEST_OK, nRetVal3 = ITO_TEST_OK, nRetVal4 = ITO_TEST_OK, nRetVal5 = ITO_TEST_OK;
	u32 i = 0;
	u32 j = 0;
	u16 nTestPinCount = 0;
	s32 nShortThreshold = 0;

	DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaReset();
#endif
#endif

	DrvPlatformLyrDisableFingerTouchReport();

	DrvPlatformLyrTouchDeviceResetHw();


	if (_gTestAutoSwitchFlag != 0) {
		_DrvMpTestItoTestMsg26xxmGetSwitchFlag();
	}

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	_DrvMpTestItoTestMsg26xxmSwitchMode(_gTestSwitchMode, MUTUAL);

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		_gShortTestChannel[i] = 0;
	}

	for (i = 0; i < MAX_MUTUAL_NUM; i++) {
		_gDeltaC[i] = 0;
	}

	_gSenseLineNum = _DrvMpTestItoTestAnaGetMutualChannelNum();
	_gDriveLineNum = _DrvMpTestItoTestAnaGetMutualSubFrameNum();

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		_gShortTestChannel[i] = 0xff;
	}

	_gTestFailChannelCount = 0;

	nTestPinCount = 0;



	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		_DrvMpTestMsg26xxmItoShortTest(1);
	}

	nRetVal2 = _DrvMpTestItoShortTestMsg26xxmJudge(1);

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		if (_gShortTestChannel[i] != 0) {
			nTestPinCount++;
		}
	}

	for (i = 0; i < nTestPinCount; i++) {
		for (j = 0; j < _gSenseLineNum; j++) {
			if (_gShortTestChannel[i] == SENSE_X[j]) {
				_gSenseR[j] = _DrvMpTestItoShortTestCovertRValue(_gDeltaC[i]);

				DBG("_gSenseR[%d] = %d\t", j , _gSenseR[j]);
			}
		}
	}
	DBG("\n");


	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		_gShortTestChannel[i] = 0xff;
	}

	nTestPinCount = 0;



	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		_DrvMpTestMsg26xxmItoShortTest(2);
	}

	nRetVal3 = _DrvMpTestItoShortTestMsg26xxmJudge(2);

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		if (_gShortTestChannel[i] != 0) {
			nTestPinCount++;
		}
	}

	for (i = 0; i < nTestPinCount; i++) {
		for (j = 0; j < _gSenseLineNum; j++) {
			if (_gShortTestChannel[i] == SENSE_X[j]) {
				_gSenseR[j] = _DrvMpTestItoShortTestCovertRValue(_gDeltaC[i]);

				DBG("_gSenseR[%d] = %d\t", j , _gSenseR[j]);
			}
		}
	}
	DBG("\n");

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		_gShortTestChannel[i] = 0xff;
	}
	nTestPinCount = 0;

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		_DrvMpTestMsg26xxmItoShortTest(3);
	}

	nRetVal4 = _DrvMpTestItoShortTestMsg26xxmJudge(3);

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		if (_gShortTestChannel[i] != 0) {
			nTestPinCount++;
		}
	}

	for (i = 0; i < nTestPinCount; i++) {
		for (j = 0; j < _gDriveLineNum; j++) {
			if (_gShortTestChannel[i] == DRIVE_X[j]) {
				_gDriveR[j] = _DrvMpTestItoShortTestCovertRValue(_gDeltaC[i]);
				DBG("_gDriveR[%d] = %d\t", j , _gDriveR[j]);
			}
		}
	}
	DBG("\n");

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		_gShortTestChannel[i] = 0xff;
	}
	nTestPinCount = 0;

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		_DrvMpTestMsg26xxmItoShortTest(4);
	}

	nRetVal4 = _DrvMpTestItoShortTestMsg26xxmJudge(4);

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		if (_gShortTestChannel[i] != 0) {
			nTestPinCount++;
		}
	}

	for (i = 0; i < nTestPinCount; i++) {
		for (j = 0; j < _gDriveLineNum ; j++) {
			if (_gShortTestChannel[i] == DRIVE_X[j]) {
				_gDriveR[j] = _DrvMpTestItoShortTestCovertRValue(_gDeltaC[i]);
				DBG("_gDriveR[%d] = %d\t", j , _gDriveR[j]);
			}
		}
	}
	DBG("\n");

	for (i = 0; i < MAX_CHANNEL_NUM; i++) {
		_gShortTestChannel[i] = 0xff;
	}
	nTestPinCount = 0;
	nShortThreshold = _DrvMpTestItoShortTestCovertRValue(SHORT_VALUE);

	for (i = 0; i < _gSenseLineNum; i++) {
		_gResult[i] = _gSenseR[i];
	}

	for (i = 0; i < _gDriveLineNum; i++) {
		_gResult[i + _gSenseLineNum] = _gDriveR[i];
	}

	for (i = 0; i < (_gSenseLineNum + _gDriveLineNum); i++) {
		if (_gResult[i] < nShortThreshold) {
			_gTestFailChannel[i] = 1;
		} else {
			_gTestFailChannel[i] = 0;
		}
	}

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	DrvPlatformLyrTouchDeviceResetHw();
	mdelay(300);

	DrvPlatformLyrEnableFingerTouchReport();

	DBG("short test end\n");

	DBG("nRetVal1 = %d, nRetVal2 = %d, nRetVal3 = %d, nRetVal4 = %d, nRetVal5 = %d\n", nRetVal1, nRetVal2, nRetVal3, nRetVal4, nRetVal5);

	if ((nRetVal1 != ITO_TEST_OK) && (nRetVal2 == ITO_TEST_OK) && (nRetVal3 == ITO_TEST_OK) && (nRetVal4 == ITO_TEST_OK) && (nRetVal5 == ITO_TEST_OK)) {
		return ITO_TEST_GET_TP_TYPE_ERROR;
	} else if ((nRetVal1 == ITO_TEST_OK) && ((nRetVal2 != ITO_TEST_OK) || (nRetVal3 != ITO_TEST_OK) || (nRetVal4 != ITO_TEST_OK) || (nRetVal5 != ITO_TEST_OK))) {
		return -EPERM;
	} else {
		return ITO_TEST_OK;
	}
}

static void _DrvMpTestItoShortTestMsg28xxSetNoiseSensorMode(u8 nEnable)
{
	s16 j;

	DBG("*** %s() ***\n", __func__);

	if (nEnable) {
		RegSet16BitValueOn(0x1546, BIT4);
		for (j = 0; j < 10; j++) {
			RegSet16BitValue(0x2148 + 2 * j, 0x0000);
		}
		RegSet16BitValue(0x215C, 0x1FFF);
	}
}

static void _DrvMpTestItoShortTestMsg28xxAnaFixPrs(u16 nOption)
{
	u16 nRegData = 0;

	DBG("*** %s() ***\n", __func__);

	RegSet16BitValue(0x0FE6, 0x0001);

	nRegData = RegGet16BitValue(0x1008);
	nRegData &= 0x00F1;
	nRegData |= (u16)((nOption << 1) & 0x000E);
	RegSet16BitValue(0x1008, nRegData);
}

static void _DrvMpTestItoShortTestMsg28xxAndChangeCDtime(u16 nTime1, u16 nTime2)
{
	DBG("*** %s() ***\n", __func__);

	RegSet16BitValue(0x1026, nTime1);
	RegSet16BitValue(0x1030, nTime2);
}

static void _DrvMpTestItoShortTestMsg28xxChangeANASetting(void)
{
	int i, nMappingItem;
	u8 nChipVer;


	DBG("*** %s() ***\n", __func__);


	RegSet16BitValue(0x0FE6, 0x0001);

	nChipVer = RegGetLByteValue(0x1ECE);


	if (nChipVer != 0)
		RegSetLByteValue(0x131E, 0x01);

	for (nMappingItem = 0; nMappingItem < 6; nMappingItem++) {
		/
		RegSetLByteValue(0x2192, 0x00);
		RegSetLByteValue(0x2102, 0x01);
		RegSetLByteValue(0x2102, 0x00);
		RegSetLByteValue(0x2182, 0x08);
		RegSetLByteValue(0x2180, 0x08 * nMappingItem);
		RegSetLByteValue(0x2188, 0x01);

		for (i = 0; i < 8; i++) {
			if (nMappingItem == 0 && nChipVer == 0x0) {
				RegSet16BitValue(0x218A, _gMuxMem_20_3E_0_Settings[2 * i]);
				RegSet16BitValue(0x218C, _gMuxMem_20_3E_0_Settings[2 * i + 1]);
			}
			if ((nMappingItem == 1 && nChipVer == 0x0) || (nMappingItem == 0 && nChipVer != 0x0)) {
				RegSet16BitValue(0x218A, _gMuxMem_20_3E_1_Settings[2 * i]);
				RegSet16BitValue(0x218C, _gMuxMem_20_3E_1_Settings[2 * i + 1]);
			}
			if ((nMappingItem == 2 && nChipVer == 0x0) || (nMappingItem == 1 && nChipVer != 0x0)) {
				RegSet16BitValue(0x218A, _gMuxMem_20_3E_2_Settings[2 * i]);
				RegSet16BitValue(0x218C, _gMuxMem_20_3E_2_Settings[2 * i + 1]);
			}
			if ((nMappingItem == 3 && nChipVer == 0x0) || (nMappingItem == 2 && nChipVer != 0x0)) {
				RegSet16BitValue(0x218A, _gMuxMem_20_3E_3_Settings[2 * i]);
				RegSet16BitValue(0x218C, _gMuxMem_20_3E_3_Settings[2 * i + 1]);
			}
			if ((nMappingItem == 4 && nChipVer == 0x0) || (nMappingItem == 3 && nChipVer != 0x0)) {
				RegSet16BitValue(0x218A, _gMuxMem_20_3E_4_Settings[2 * i]);
				RegSet16BitValue(0x218C, _gMuxMem_20_3E_4_Settings[2 * i + 1]);
			}
			if ((nMappingItem == 5 && nChipVer == 0x0) || (nMappingItem == 4 && nChipVer != 0x0)) {
				RegSet16BitValue(0x218A, _gMuxMem_20_3E_5_Settings[2 * i]);
				RegSet16BitValue(0x218C, _gMuxMem_20_3E_5_Settings[2 * i + 1]);
			}
			if (nMappingItem == 5 && nChipVer != 0x0) {
				RegSet16BitValue(0x218A, _gMuxMem_20_3E_6_Settings[2 * i]);
				RegSet16BitValue(0x218C, _gMuxMem_20_3E_6_Settings[2 * i + 1]);
			}
		}
	}
}

static void _DrvMpTestItoReadSetting(u16 *pPad2Sense, u16 *pPad2Drive, u16 *pPad2GR)
{
	DBG("*** %s() ***\n", __func__);

	memcpy(_gMuxMem_20_3E_1_Settings, SHORT_N1_MUX_MEM_20_3E, sizeof(u16) * 16);
	memcpy(_gMuxMem_20_3E_2_Settings, SHORT_N2_MUX_MEM_20_3E, sizeof(u16) * 16);
	memcpy(_gMuxMem_20_3E_3_Settings, SHORT_S1_MUX_MEM_20_3E, sizeof(u16) * 16);
	memcpy(_gMuxMem_20_3E_4_Settings, SHORT_S2_MUX_MEM_20_3E, sizeof(u16) * 16);

	if (SHORT_TEST_5_TYPE != 0) {
		memcpy(_gMuxMem_20_3E_5_Settings, SHORT_X_MUX_MEM_20_3E, sizeof(u16) * 16);
	}

	memcpy(pPad2Sense, PAD_TABLE_SENSE, sizeof(u16) * _gSenseLineNum);
	memcpy(pPad2Drive, PAD_TABLE_DRIVE, sizeof(u16) * _gDriveLineNum);

	if (GR_NUM != 0) {
		memcpy(pPad2GR, PAD_TABLE_GR, sizeof(u16) * GR_NUM);
	}
}

static s32 _DrvMpTestItoShortTestMsg28xxGetValueR(s32 *pTarget)
{
	s16 *pRawData = NULL;
	u16 nSenNumBak = 0;
	u16 nDrvNumBak = 0;
	u16 nShift = 0;
	s16 i, j;

	DBG("*** %s() ***\n", __func__);

	pRawData = kzalloc(sizeof(s16) * MAX_CHANNEL_SEN*2 * MAX_CHANNEL_DRV, GFP_KERNEL);

	if (_DrvMpTestItoTestMsg28xxGetMutualOneShotRawIIR(pRawData, &nSenNumBak, &nDrvNumBak) < 0) {
		DBG("*** Msg28xx Short Test# GetMutualOneShotRawIIR failed! ***\n");
		return -EPERM;
	}

	for (i = 0; i < 5; i++) {
		for (j = 0; j < 13; j++) {
			nShift = (u16)(j + 13 * i);
			pTarget[nShift] = pRawData[i*MAX_CHANNEL_DRV+j];
		}
	}

	kfree(pRawData);

	return 0;
}

static s32 _DrvMpTestMsg28xxItoShortTest(u8 nItemID)
{
	s16 i;
	u8 nRegData = 0;
	u16 nAfeCoef = 0;

	DBG("*** %s() ***\n", __func__);


	RegSet16BitValue(0x0FE6, 0x0001);

	/
	RegSetLByteValue(0x130A, 0x6D);
	RegSetLByteValue(0x1103, 0x06);
	RegSetLByteValue(0x1016, 0x0C);

	RegSetLByteValue(0x1104, 0x0C);
	RegSetLByteValue(0x100C, 0x0C);
	RegSetLByteValue(0x1B10, 0x0C);

	/
	RegSetLByteValue(0x102F, 0x60);

	/
	RegSet16BitValue(0x1420, 0xA55A);
	RegSet16BitValue(0x1428, 0xA55A);
	RegSet16BitValue(0x1422, 0xFC4C);

	_DrvMpTestItoShortTestMsg28xxSetNoiseSensorMode(1);
	_DrvMpTestItoShortTestMsg28xxAnaFixPrs(3);
	_DrvMpTestItoShortTestMsg28xxAndChangeCDtime(0x007E, 0x001F);

	/
	RegSet16BitValue(0x150C, 0x80A2);
	RegSet16BitValue(0x1520, 0xFFFF);
	RegSet16BitValue(0x1522, 0xFFFF);
	RegSet16BitValue(0x1524, 0xFFFF);
	RegSet16BitValue(0x1526, 0xFFFF);

	/
	RegSet16BitValue(0x1508, 0x1FFF);
	RegSet16BitValue(0x1550, 0x0000);

	/
	RegSet16BitValue(0x1552, 0x0000);

	/
	RegSet16BitValue(0x1564, 0x0077);

	/
	RegSet16BitValue(0x1260, 0x1FFF);

	/
	RegSet16BitValue(0x156A, 0x0000);

	/
	RegSetLByteValue(0x1221, 0x00);


	/

	nRegData = RegGetLByteValue(0x101A);
	nAfeCoef = 0x10000 / nRegData;

	RegSet16BitValue(0x13D6, nAfeCoef);

	/




	_DrvMpTestItoShortTestMsg28xxChangeANASetting();
	_DrvMpTestItoTestMsg28xxAnaSwReset();

	if (_DrvMpTestItoShortTestMsg28xxGetValueR(_gDeltaC) < 0) {
		DBG("*** Msg28xx Short Test# GetValueR failed! ***\n");
		return -EPERM;
	}
	_ItoTestDebugShowArray(_gDeltaC, 128, -32, 10, 8);

	for (i = 0; i < 65; i++) {
		if (_gDeltaC[i] <= -1000 || _gDeltaC[i] >= (IIR_MAX))
			_gDeltaC[i] = 0x7FFF;
		else
			_gDeltaC[i] = abs(_gDeltaC[i]);
	}
	return 0;
}

static s32 _DrvMpTestItoShortTestReadTestPins(u8 nItemID, u16 *pTestPins)
{
	u16 nCount = 0;
	s16 i;

	DBG("*** %s() ***\n", __func__);

	switch (nItemID) {
	case 1:
	case 11:
		nCount = SHORT_N1_TEST_NUMBER;
		memcpy(pTestPins, SHORT_N1_TEST_PIN, sizeof(u16) * nCount);
		break;
	case 2:
	case 12:
		nCount = SHORT_N2_TEST_NUMBER;
		memcpy(pTestPins, SHORT_N2_TEST_PIN, sizeof(u16) * nCount);
		break;
	case 3:
	case 13:
		nCount = SHORT_S1_TEST_NUMBER;
		memcpy(pTestPins, SHORT_S1_TEST_PIN, sizeof(u16) * nCount);
		break;
	case 4:
	case 14:
		nCount = SHORT_S2_TEST_NUMBER;
		memcpy(pTestPins, SHORT_S2_TEST_PIN, sizeof(u16) * nCount);
		break;
	case 5:
	case 15:
		if (SHORT_TEST_5_TYPE != 0) {
			nCount = SHORT_X_TEST_NUMBER;
			memcpy(pTestPins, SHORT_X_TEST_PIN, sizeof(u16) * nCount);
		}
		break;
	case 0:
	default:
		return 0;
	}

	for (i = nCount; i < MAX_CHANNEL_NUM; i++) {
		pTestPins[i] = 0xFFFF;
	}

	return nCount;
}

static s32 _DrvMpTestItoShortTestMsg28xxJudge(u8 nItemID, /*s8 pNormalTestResult[][2], */ u16 pTestPinMap[][13], u16 *pTestPinCount)
{
	s32 nRetVal = 0;
	u16 nTestPins[MAX_CHANNEL_NUM];
	s16 i;

	DBG("*** %s() ***\n", __func__);

	*pTestPinCount = _DrvMpTestItoShortTestReadTestPins(nItemID, nTestPins);

	if (*pTestPinCount == 0) {
		if (nItemID == 5 && SHORT_TEST_5_TYPE == 0) {

		} else {
			DBG("*** Msg28xx Short Test# TestPinCount = 0 ***\n");
			return -EPERM;
		}
	}


	for (i = (nItemID - 1) * 13; i < (13 * nItemID); i++) {
		_gResult[i] = _gDeltaC[i];
	}

	for (i = 0; i < *pTestPinCount; i++) {
		pTestPinMap[nItemID][i] = nTestPins[i];
		if (0 == _DrvMpTestItoTestCheckValueInRange(_gResult[i + (nItemID - 1) * 13], SHORTVALUE, -1000)) {


			DBG("*** Msg28xx Short Test# ShortTestMsg28xxJudge failed! ***\n");
			nRetVal = -1;
		}
	}

	DBG("*** Msg28xx Short Test# nItemID = %d ***\n", nItemID);


	return nRetVal;
}

static s32 _DrvMpTestItoShortTestMsg28xxCovertRValue(s32 nValue)
{
	if (nValue >= IIR_MAX) {
		return 0;
	}


	return 223 * 32768 / (nValue * 550);
}

static s32 _DrvMpTestMsg28xxItoShortTestEntry(void)
{

	s16 i = 0, j = 0;


	u16 nPad2Drive[DRIVE_NUM] = {0};
	u16 nPad2Sense[SENSE_NUM] = {0};
	u16 nPad2GR[MAX_CHANNEL_NUM] = {0};
	s32 nResultTemp[(MAX_CHANNEL_SEN+MAX_CHANNEL_DRV)*2] = {0};

	/

	u16 nTestItemLoop = 6;
	u16 nTestItem = 0;

	u16 nTestPinMap[6][13] = {{0} };
	u16 nTestPinNum = 0;

	u32 nRetVal = 0;

	DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaReset();
#endif
#endif

	_gSenseLineNum = SENSE_NUM;
	_gDriveLineNum = DRIVE_NUM;

	DrvPlatformLyrDisableFingerTouchReport();
	DrvPlatformLyrTouchDeviceResetHw();


	DbBusResetSlave();
	DbBusEnterSerialDebugMode();
	DbBusWaitMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	if (_DrvMpTestMsg28xxItoTestSwitchFwMode(MUTUAL_SINGLE_DRIVE) < 0) {
		DBG("*** Msg28xx Short Test# Switch FW mode failed! ***\n");
		return -EPERM;
	}

	_DrvMpTestItoReadSetting(nPad2Sense, nPad2Drive, nPad2GR);



	if (_DrvMpTestMsg28xxItoShortTest(1) < 0) {
		DBG("*** Msg28xx Short Test# Get DeltaC failed! ***\n");
		return -EPERM;
	}

	for (nTestItem = 1; nTestItem < nTestItemLoop; nTestItem++) {
		DBG("*** Short test item %d ***\n", nTestItem);
		if (_DrvMpTestItoShortTestMsg28xxJudge(nTestItem, /*nNormalTestResult, */ nTestPinMap, &nTestPinNum) < 0) {
			DBG("*** Msg28xx Short Test# Item %d is failed! ***\n", nTestItem);
			nRetVal = -1;
		}

		if (nTestItem == 1 || nTestItem == 2 || (nTestItem == 5 && SHORT_TEST_5_TYPE == 1)) {
			for (i = 0; i < nTestPinNum; i++) {
				for (j = 0; j < _gSenseLineNum; j++) {
					if (nTestPinMap[nTestItem][i] == nPad2Sense[j]) {
						_gSenseR[j] = _gResult[i + (nTestItem - 1) * 13];
					}
				}
			}
		}

		if (nTestItem == 3 || nTestItem == 4 || (nTestItem == 5 && SHORT_TEST_5_TYPE == 2)) {
			for (i = 0; i < nTestPinNum; i++) {
				for (j = 0; j < _gDriveLineNum; j++) {
					if (nTestPinMap[nTestItem][i] == nPad2Drive[j]) {
						_gDriveR[j] = _gResult[i + (nTestItem - 1) * 13];
					}
				}
			}
		}

		if (nTestItem == 5 && SHORT_TEST_5_TYPE == 3) {
			for (i = 0; i < nTestPinNum; i++) {
				for (j = 0; j < GR_NUM; j++) {
					if (nTestPinMap[nTestItem][i] == nPad2GR[j])
						_gGRR[j] = _gResult[i + (nTestItem - 1) * 13];
				}
			}
		}
	}

	for (i = 0; i < _gSenseLineNum; i++) {
		nResultTemp[i] = _gSenseR[i];
	}


	for (i = 0; i < _gDriveLineNum; i++) {
		nResultTemp[i + _gSenseLineNum] = _gDriveR[i];
	}


	for (i = 0; i < _gSenseLineNum + _gDriveLineNum; i++) {
		if (nResultTemp[i] == 0) {
			_gResult[i] = _DrvMpTestItoShortTestMsg28xxCovertRValue(1);
		} else {
			_gResult[i] = _DrvMpTestItoShortTestMsg28xxCovertRValue(nResultTemp[i]);
		}
	}

	_ItoTestDebugShowArray(_gResult, _gSenseLineNum + _gDriveLineNum, -32, 10, 8);

	for (i = 0; i < (_gSenseLineNum + _gDriveLineNum); i++) {
		if (nResultTemp[i] > SHORTVALUE) {
			_gTestFailChannel[i] = 1;
			_gTestFailChannelCount++;
			nRetVal = -1;
		} else {
			_gTestFailChannel[i] = 0;
		}
	}

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	DrvPlatformLyrTouchDeviceResetHw();
	mdelay(300);

	DrvPlatformLyrEnableFingerTouchReport();
	return nRetVal;
}

static s32 _DrvMpTestItoShortTest(void)
{
	s32 nRetVal = -1;

	DBG("*** %s() ***\n", __func__);

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		return _DrvMpTestMsg26xxmItoShortTestEntry();
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		return _DrvMpTestMsg28xxItoShortTestEntry();
	}

	return nRetVal;
}

static s32 _DrvMpTestItoTestMsg26xxmGetWaterProofOneShotRawIir(u16 wszResultData[])
{
	u16 nRegData;
	u16 i;
	u16 nTemp;
	u8 szDbBusTxData[3];
	u32 nGetdataNum = 12;
	u8 szShotData[24] = {0};

	DBG("*** %s() ***\n", __func__);

	nTemp = RegGet16BitValue(0x3D08);
	nTemp &= (~(BIT8));
	RegSet16BitValue(0x3D08, nTemp);


	_DrvMpTestItoTestMsg26xxmEnableAdcOneShot();

	nRegData = 0;
	while (0x0000 == (nRegData & BIT8)) {
		nRegData = RegGet16BitValue(0x3D18);
	}

	for (i = 0; i < nGetdataNum * 2; i++) {
		szShotData[i] = 0;
	}

	mdelay(200);
	szDbBusTxData[0] = 0x10;
	szDbBusTxData[1] = 0x13;
	szDbBusTxData[2] = 0x42;
	IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3);
	IicReadData(SLAVE_I2C_ID_DBBUS, &szShotData[0], 24);

	for (i = 0; i < nGetdataNum; i++) {
		nRegData = (u16)(szShotData[i * 2] | szShotData[i * 2 + 1] << 8);
		wszResultData[i] = (short)nRegData;


	}

	nTemp = RegGet16BitValue(0x3D08);
	nTemp |= (BIT8 | BIT4);
	RegSet16BitValue(0x3D08, nTemp);

	return 0;
}

static s32 _DrvMpTestItoTestMsg26xxmGetWaterProofDeltaC(s32 *pTarget)
{
	u16 wszRawData[12];
	u16 i;

	DBG("*** %s() ***\n", __func__);

	if (_DrvMpTestItoTestMsg26xxmGetWaterProofOneShotRawIir(wszRawData) < 0) {
		DBG("*** Msg26xxm WaterProof Test# GetMutualOneShotRawIIR failed! ***\n");
		return -EPERM;
	}

	for (i = 0; i < 12; i++) {
		pTarget[i] = (s16)wszRawData[i];
	}

	return 0;
}

static s32 _DrvMpTestMsg26xxmItoWaterProofTest(void)
{
	DBG("*** %s() ***\n", __func__);




	_DrvMpTestItoTestMsg26xxmAnaSwReset();

	_DrvMpTestItoShortTestMsg26xxmAndChangeCDtime(WATERPROOF_Charge_X, WATERPROOF_Dump_X);

	if (_DrvMpTestItoTestMsg26xxmGetWaterProofDeltaC(_gDeltaC) < 0) {
		DBG("*** Msg26xxm WaterProof Test# GetWaterDeltaC failed! ***\n");
		return -EPERM;
	}

	return 0;
}

static s32 _DrvMpTestMsg26xxmItoWaterProofTestJudge(void)
{
	u16 i;
	u32 nGetdataNum = 12;

	DBG("*** %s() ***\n", __func__);

	for (i = 0; i < nGetdataNum; i++) {
		_gResultWater[i] = 0;
	}

	for (i = 0; i < nGetdataNum; i++) {
		_gResultWater[i] = _gDeltaC[i];
	}

	return 0;
}

s32 _DrvMpTestMsg26xxmItoWaterProofTestEntry(void)
{
	s32 nRetVal = 0;
	u32 nRegData = 0;
	u16 i = 0;
	s32 nResultTemp[12] = {0};

	DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaReset();
#endif
#endif

	_gWaterProofNum = 12;

	DrvPlatformLyrDisableFingerTouchReport();

	DrvPlatformLyrTouchDeviceResetHw();


	if (_gTestAutoSwitchFlag != 0) {
		_DrvMpTestItoTestMsg26xxmGetSwitchFlag();
	}

	DbBusResetSlave();
	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	if (_DrvMpTestItoTestMsg26xxmSwitchMode(_gTestSwitchMode, WATERPROOF) < 0) {
		DBG("*** Msg26xxm WaterProof Test# Switch FW mode failed! ***\n");
		return -EPERM;
	}

	_DrvMpTestItoTestMsg26xxmMcuStop();

	nRegData = RegGet16BitValue(0x3CE4);

	if (nRegData == 0x8bbd) {
		DBG("*** Msg26xxm WaterProof Test# No supporting this function! ***\n");
		return -EPERM;
	}

	mdelay(10);

	for (i = 0; i < MAX_MUTUAL_NUM; i++) {
		_gTestFailChannel[i] = 0;
	}

	_gTestFailChannelCount = 0;

	if (_DrvMpTestMsg26xxmItoWaterProofTest() < 0) {
		DBG("*** Msg26xxm WaterProof Test# Get DeltaC failed! ***\n");
		return -EPERM;
	}

	_DrvMpTestMsg26xxmItoWaterProofTestJudge();

	for (i = 0; i < 12; i++) {
		nResultTemp[i] = _gResultWater[i];
	}

	_ItoTestDebugShowArray(_gResultWater, 12, -32, 10, 8);
	for (i = 0; i < 12; i++) {
		if (nResultTemp[i] < WATERPROOFVALUE) {
			_gTestFailChannel[i] = 1;
			_gTestFailChannelCount++;
			nRetVal = -1;
		} else {
			_gTestFailChannel[i] = 0;
		}
	}


	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	DrvPlatformLyrTouchDeviceResetHw();
	mdelay(300);

	DrvPlatformLyrEnableFingerTouchReport();

	return nRetVal;
}

static s32 _DrvMpTestItoWaterProofTestMsg28xxTriggerWaterProofOneShot(s16 *pResultData, u32 nDelay)
{
	u16 nAddr = 0x5000, nAddrNextSF = 0x1A4;
	u16 nSF = 0, nAfeOpening = 0, nDriOpening = 0;
	u16 nMaxDataNumOfOneSF = 0;
	u16 nDriMode = 0;
	u16 i;
	u8 nRegData = 0;
	u8 nShotData[390] = {0};
	u16 nRegDataU16 = 0;

	DBG("*** %s() ***\n", __func__);

	nRegData = RegGetLByteValue(0x130A);
	nSF = nRegData >> 4;
	nAfeOpening = nRegData & 0x0f;

	if (nSF == 0) {
		return -EPERM;
	}

	nRegData = RegGetLByteValue(0x100B);
	nDriMode = nRegData;

	nRegData = RegGetLByteValue(0x1312);
	nDriOpening = nRegData;

	DBG("*** Msg28xx WaterProof Test# TriggerWaterProofOneShot nSF=%d, nAfeOpening=%d, nDriMode=%d, nDriOpening=%d. ***\n", nSF, nAfeOpening, nDriMode, nDriOpening);

	nMaxDataNumOfOneSF = nAfeOpening * nDriOpening;

	RegSet16BitValueOff(0x3D08, BIT8);	  /

	/
	_DrvMpTestItoTestMsg28xxEnableAdcOneShot();

	while (0x0000 == (nRegDataU16 & BIT8)) {
		nRegDataU16 = RegGet16BitValue(0x3D18);
	}

	RegSet16BitValueOn(0x3D08, BIT8);	  /
	RegSet16BitValueOn(0x3D08, BIT4);	  /

	if (PATTERN_TYPE == 1) {

		/
		_DrvMpTestItoTestDBBusReadDQMemStart();
		RegGetXBitValue(nAddr, nShotData, 16, MAX_I2C_TRANSACTION_LENGTH_LIMIT);
		_DrvMpTestItoTestDBBusReadDQMemEnd();


		for (i = 0; i < 8; i++) {
			pResultData[i] = (s16)(nShotData[2 * i] | nShotData[2 * i + 1] << 8);
		}
	} else if (PATTERN_TYPE == 3 || PATTERN_TYPE == 4) {


		if (nSF > 4)
			nSF = 4;
		/
		for (i = 0; i < nSF; i++) {
			_DrvMpTestItoTestDBBusReadDQMemStart();
			RegGetXBitValue(nAddr + i * nAddrNextSF, nShotData, 16, MAX_I2C_TRANSACTION_LENGTH_LIMIT);
			_DrvMpTestItoTestDBBusReadDQMemEnd();


			pResultData[2 * i] = (s16)(nShotData[4 * i] | nShotData[4 * i + 1] << 8);
			pResultData[2 * i + 1] = (s16)(nShotData[4 * i + 2] | nShotData[4 * i + 3] << 8);
		}
	} else {
		return -EPERM;
	}

	return 0;
}

static s32 _DrvMpTestItoWaterProofTesMsg28xxtGetWaterProofOneShotRawIIR(s16 *pRawDataWP, u32 nDelay)
{
	return _DrvMpTestItoWaterProofTestMsg28xxTriggerWaterProofOneShot(pRawDataWP, nDelay);
}

static s32 _DrvMpTestItoWaterProofTestMsg28xxGetDeltaCWP(s32 *pTarget, s8 nSwap, u32 nDelay)
{
	s16 nRawDataWP[12] = {0};
	s16 i;

	DBG("*** %s() ***\n", __func__);

	if (_DrvMpTestItoWaterProofTesMsg28xxtGetWaterProofOneShotRawIIR(nRawDataWP, nDelay) < 0) {
		DBG("*** Msg28xx Open Test# GetMutualOneShotRawIIR failed! ***\n");
		return -EPERM;
	}

	for (i = 0; i < _gWaterProofNum; i++) {
		pTarget[i] = nRawDataWP[i];
	}

	return 0;
}

static s32 _DrvMpTestMsg28xxItoWaterProofTest(u32 nDelay)
{
	DBG("*** %s() ***\n", __func__);


	RegSet16BitValue(0x0FE6, 0x0001);
	_DrvMpTestItoTestMsg28xxAnaSwReset();

	if (_DrvMpTestItoWaterProofTestMsg28xxGetDeltaCWP(_gDeltaCWater, -1, nDelay) < 0) {
		DBG("*** Msg28xx WaterProof Test# GetDeltaCWP failed! ***\n");
		return -EPERM;
	}

	_ItoTestDebugShowArray(_gDeltaCWater, 12, -32, 10, 16);

	return 0;
}

static void _DrvMpTestMsg28xxItoWaterProofTestMsgJudge(void)
{
	int i;

	DBG("*** %s() ***\n", __func__);

	for (i = 0; i < _gWaterProofNum; i++) {
		_gResultWater[i] =  abs(_gDeltaCWater[i]);
	}
}

static s32 _DrvMpTestMsg28xxItoWaterProofTestEntry(void)
{
	s16 i = 0;
	u32 nRetVal = 0;
	u16 nRegDataWP = 0;
	u32 nDelay = 0;

	DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaReset();
#endif
#endif

	_gWaterProofNum = 12;

	DrvPlatformLyrDisableFingerTouchReport();
	DrvPlatformLyrTouchDeviceResetHw();


	DbBusResetSlave();
	DbBusEnterSerialDebugMode();
	DbBusWaitMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	if (_DrvMpTestMsg28xxItoTestSwitchFwMode(WATERPROOF) < 0) {
		DBG("*** Msg28xx WaterProof Test# Switch FW mode failed! ***\n");
		return -EPERM;
	}

	nRegDataWP = RegGet16BitValue(0x1402);
	if (nRegDataWP == 0x8BBD) {
		DBG("*** Msg28xx WaterProof Test# FW don't support waterproof! ***\n");
	}

	if (_DrvMpTestMsg28xxItoWaterProofTest(nDelay) < 0) {
		DBG("*** Msg28xx WaterProof Test# Get DeltaC failed! ***\n");
		return -EPERM;
	}

	_DrvMpTestMsg28xxItoWaterProofTestMsgJudge();

	for (i = 0; i < _gWaterProofNum; i++) {
		if (_gResultWater[i] > WATERVALUE) {
			_gTestFailChannel[i] = 1;
			_gTestFailChannelCount++;
			nRetVal = -1;
		} else {
			_gTestFailChannel[i] = 0;
		}
	}

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	DrvPlatformLyrTouchDeviceResetHw();
	mdelay(300);

	DrvPlatformLyrEnableFingerTouchReport();
	return nRetVal;
}

static s32 _DrvMpTestItoWaterProofTest(void)
{
	s32 nRetVal = -1;

	DBG("*** %s() ***\n", __func__);

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		return _DrvMpTestMsg26xxmItoWaterProofTestEntry();
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		return _DrvMpTestMsg28xxItoWaterProofTestEntry();
	}

	return nRetVal;
}

static void _DrvMpTestItoTestDoWork(struct work_struct *pWork)
{
	s32 nRetVal = 0;

	DBG("*** %s() _gIsInMpTest = %d, _gTestRetryCount = %d ***\n", __func__, _gIsInMpTest, _gTestRetryCount);

	if (_gItoTestMode == ITO_TEST_MODE_OPEN_TEST) {
		nRetVal = _DrvMpTestItoOpenTest();
	} else if (_gItoTestMode == ITO_TEST_MODE_SHORT_TEST) {
		nRetVal = _DrvMpTestItoShortTest();
	} else if (_gItoTestMode == ITO_TEST_MODE_WATERPROOF_TEST) {
		nRetVal = _DrvMpTestItoWaterProofTest();
	} else {
		DBG("*** Undefined Mp Test Mode = %d ***\n", _gItoTestMode);
		return;
	}

	DBG("*** ctp mp test result = %d ***\n", nRetVal);

	if (nRetVal == 0) {
		_gCtpMpTestStatus = ITO_TEST_OK;
		_gIsInMpTest = 0;
		DBG("mp test success\n");
	} else {
		_gTestRetryCount--;
		if (_gTestRetryCount > 0) {
			DBG("_gTestRetryCount = %d\n", _gTestRetryCount);
			queue_work(_gCtpMpTestWorkQueue, &_gCtpItoTestWork);
		} else {
			if (nRetVal == -1) {
				_gCtpMpTestStatus = ITO_TEST_FAIL;
			} else {
				_gCtpMpTestStatus = ITO_TEST_UNDEFINED_ERROR;
			}

			_gIsInMpTest = 0;
			DBG("mp test failed\n");
		}
	}
}

/*=============================================================*/

/*=============================================================*/

s32 DrvMpTestGetTestResult(void)
{
	DBG("*** %s() ***\n", __func__);
	DBG("_gCtpMpTestStatus = %d\n", _gCtpMpTestStatus);

	return _gCtpMpTestStatus;
}

void DrvMpTestGetTestFailChannel(ItoTestMode_e eItoTestMode, u8 *pFailChannel, u32 *pFailChannelCount)
{
	u32 i;

	DBG("*** %s() ***\n", __func__);
	DBG("_gTestFailChannelCount = %d\n", _gTestFailChannelCount);

	for (i = 0; i < MAX_MUTUAL_NUM; i++) {
		  pFailChannel[i] = _gTestFailChannel[i];
	}

	*pFailChannelCount = MAX_MUTUAL_NUM;
}

void DrvMpTestGetTestDataLog(ItoTestMode_e eItoTestMode, u8 *pDataLog, u32 *pLength)
{
	u32 i, j, k;

	DBG("*** %s() ***\n", __func__);

	if (eItoTestMode == ITO_TEST_MODE_OPEN_TEST) {
		k = 0;

		for (j = 0; j < _gDriveLineNum; j++) {
			for (i = 0; i < _gSenseLineNum; i++) {

				if (_gResult[i * _gDriveLineNum + j] >= 0) {
					pDataLog[k*5] = 0;
				} else {
					pDataLog[k*5] = 1;
				}

				pDataLog[k*5+1] = (_gResult[i * _gDriveLineNum + j] >> 24) & 0xFF;
				pDataLog[k*5+2] = (_gResult[i * _gDriveLineNum + j] >> 16) & 0xFF;
				pDataLog[k*5+3] = (_gResult[i * _gDriveLineNum + j] >> 8) & 0xFF;
				pDataLog[k*5+4] = (_gResult[i * _gDriveLineNum + j]) & 0xFF;

				k++;
			}
		}

		DBG("\nk = %d\n", k);

		*pLength = k*5;
	} else if (eItoTestMode == ITO_TEST_MODE_SHORT_TEST) {
		k = 0;

		for (i = 0; i < (_gDriveLineNum-1 + _gSenseLineNum); i++) {
			if (_gResult[i] >= 0) {
				pDataLog[k*5] = 0;
			} else {
				pDataLog[k*5] = 1;
			}

			pDataLog[k*5+1] = (_gResult[i] >> 24) & 0xFF;
			pDataLog[k*5+2] = (_gResult[i] >> 16) & 0xFF;
			pDataLog[k*5+3] = (_gResult[i] >> 8) & 0xFF;
			pDataLog[k*5+4] = (_gResult[i]) & 0xFF;
			k++;
		}

		DBG("\nk = %d\n", k);

		*pLength = k*5;
	} else if (eItoTestMode == ITO_TEST_MODE_WATERPROOF_TEST) {
		k = 0;

		for (i = 0; i < _gWaterProofNum; i++) {
			if (_gResultWater[i] >= 0) {
				pDataLog[k*5] = 0;
			} else {
				pDataLog[k*5] = 1;
			}

			pDataLog[k*5+1] = (_gResultWater[i] >> 24) & 0xFF;
			pDataLog[k*5+2] = (_gResultWater[i] >> 16) & 0xFF;
			pDataLog[k*5+3] = (_gResultWater[i] >> 8) & 0xFF;
			pDataLog[k*5+4] = (_gResultWater[i]) & 0xFF;
			k++;
		}

		DBG("\nk = %d\n", k);

		*pLength = k*5;
	} else {
		DBG("*** Undefined MP Test Mode ***\n");
	}
}

void DrvMpTestGetTestScope(TestScopeInfo_t *pInfo)
{
	DBG("*** %s() ***\n", __func__);

	pInfo->nMy = _gDriveLineNum;
	pInfo->nMx = _gSenseLineNum;

	DBG("*** My = %d ***\n", pInfo->nMy);
	DBG("*** Mx = %d ***\n", pInfo->nMx);
}

void DrvMpTestScheduleMpTestWork(ItoTestMode_e eItoTestMode)
{
	DBG("*** %s() ***\n", __func__);

	if (_gIsInMpTest == 0) {
		DBG("ctp mp test start\n");

		_gItoTestMode = eItoTestMode;
		_gIsInMpTest = 1;
		_gTestRetryCount = CTP_MP_TEST_RETRY_COUNT;
		_gCtpMpTestStatus = ITO_TEST_UNDER_TESTING;

		queue_work(_gCtpMpTestWorkQueue, &_gCtpItoTestWork);
	}
}

void DrvMpTestCreateMpTestWorkQueue(void)
{
	DBG("*** %s() ***\n", __func__);

	_gCtpMpTestWorkQueue = create_singlethread_workqueue("ctp_mp_test");
	INIT_WORK(&_gCtpItoTestWork, _DrvMpTestItoTestDoWork);
}

#endif
#endif
