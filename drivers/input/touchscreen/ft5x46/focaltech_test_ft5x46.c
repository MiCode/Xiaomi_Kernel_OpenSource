/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)ㄛAll Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Test_FT5X46.c
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
*
************************************************************************/

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input/ft5x46_ts.h>
#include "focaltech_test.h"

#define DEVIDE_MODE_ADDR            0x00
#define REG_LINE_NUM                0x01
#define REG_TX_NUM                  0x02
#define REG_RX_NUM                  0x03
#define REG_MAPPING_SWITCH          0x54
#define REG_TX_NOMAPPING_NUM        0x55
#define REG_RX_NOMAPPING_NUM        0x56
#define REG_NORMALIZE_TYPE          0x16
#define REG_RawBuf0                 0x36
static int m_RawData[TX_NUM_MAX][RX_NUM_MAX] = {{0} };
static int m_iTempRawData[TX_NUM_MAX * RX_NUM_MAX] = {0};
static unsigned char m_ucTempData[TX_NUM_MAX * RX_NUM_MAX*2] = {0};
static bool m_bV3TP;
struct StruScreenSeting {
	int iTxNum;
	int iRxNum;
	int isNormalize;
	int iUsedMaxTxNum;
	int iUsedMaxRxNum;
	unsigned char iChannelsNum;
	unsigned char iKeyNum;
} g_ScreenSetParam;

static int StartScan(void);
static unsigned char ReadRawData(unsigned char Freq, unsigned char LineNum,
		int ByteNum, int *pRevBuffer);
static unsigned char GetPanelRows(unsigned char *pPanelRows);
static unsigned char GetPanelCols(unsigned char *pPanelCols);
static unsigned char GetRawData(void);
static unsigned char GetChannelNum(void);
static void ShowRawData(void);
static unsigned char GetChannelNumNoMapping(void);
static unsigned char WeakShort_GetAdcData(int AllAdcDataLen, int *pRevBuffer);
void set_max_channel_num(void)
{
	g_ScreenSetParam.iUsedMaxTxNum = TX_NUM_MAX;
	g_ScreenSetParam.iUsedMaxRxNum = RX_NUM_MAX;

}
static int InitTest(void)
{
	set_max_channel_num();
	return 0;
}
unsigned char FT5X46_TestItem_RawDataTest(struct i2c_client *client)
{

	unsigned char ReCode = 0;
	bool btmpresult = true;
	int RawDataMin;
	unsigned char strSwitch = 0;
	unsigned char OriginValue = 0xff;
	int index = 0;
	int iRow, iCol;
	int iValue = 0;
	struct ft5x46_data *ft5x46 = i2c_get_clientdata(client);
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;

	m_bV3TP = false;
	FTS_TEST_DBG("\n\nTest Item:Raw Data Test\n");
	InitTest();

	ReCode = EnterFactory();
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERR("\nFailed to Enter factory.%d",
				ReCode);
		goto TEST_ERR;
	}

	if (m_bV3TP) {
		ReCode = ReadReg(REG_MAPPING_SWITCH, &strSwitch);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_ERR("\n Read REG_MAPPING_SWITCH error%d", ReCode);
			goto TEST_ERR;
		}
		if (strSwitch != 0) {
			ReCode = WriteReg(REG_MAPPING_SWITCH, 0);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_ERR("\n Write REG_MAPPING_SWITCH error%d",	ReCode);
				goto TEST_ERR;
			}
			ReCode = GetChannelNum();
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_ERR("\n GetChannelNum error%d", ReCode);
				goto TEST_ERR;
			}
		}
	} else {
		ReCode = GetChannelNum();
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_ERR("\n GetChannelNum error%d", ReCode);
			goto TEST_ERR;
			}
	}

	ReCode = ReadReg(REG_NORMALIZE_TYPE, &OriginValue);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERR("\n Read  REG_NORMALIZE_TYPE error. %d", ReCode);
		goto TEST_ERR;
	}

	if (OriginValue != 1) {
		ReCode = WriteReg(REG_NORMALIZE_TYPE, 0x01);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_ERR("\n write  REG_NORMALIZE_TYPE error.%d", ReCode);
			goto TEST_ERR;
		}
	}

	FTS_TEST_DBG("\nSet Frequecy High\n");
	ReCode = WriteReg(0x0A, 0x81);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERR("\n Set Frequecy High error. %d", ReCode);
		goto TEST_ERR;
	}

	FTS_TEST_DBG("\nFIR State: OFF");
	ReCode = WriteReg(0xFB, 0);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERR("\n FIR on error:%d", ReCode);
		goto TEST_ERR;
	}

	for (index = 0; index < 3; ++index)
		ReCode = GetRawData();

	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERR("\n Get Rawdata failed,Error Code: 0x%x", ReCode);
		goto TEST_ERR;
	}

	ShowRawData();
	RawDataMin = pdata->open_min;
	if (pdata->has_key) {
		for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
			for (iCol = 0; iCol < (g_ScreenSetParam.iRxNum - 1); iCol++) {
				iValue = m_RawData[iRow][iCol];
				if (iValue < RawDataMin) {
					btmpresult = false;
					FTS_TEST_PRINT("rawdata failure.Node=(%d,%d),value=%d",
							iRow+1, iCol+1, iValue);
				}
			}
		}
		iValue = m_RawData[pdata->key_rx1 - 1][iCol];
		if (iValue < RawDataMin) {
			btmpresult = false;
			FTS_TEST_PRINT("rawdata failure.Node=(%d,%d),Get_value=%d",
					4, iCol+1, iValue);
		}
		iValue = m_RawData[pdata->key_rx2 - 1][iCol];
		if (iValue < RawDataMin) {
			btmpresult = false;
			FTS_TEST_PRINT("rawdata failure.Node=(%d,%d),Get_value=%d",
					8, iCol+1, iValue);
		}
		iValue = m_RawData[pdata->key_rx3 - 1][iCol];
		if (iValue < RawDataMin) {
			btmpresult = false;
			FTS_TEST_PRINT("rawdata test failure.Node=(%d,%d),Get_value=%d",
					12, iCol+1, iValue);
		}
	} else {
		for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
			for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++) {
				iValue = m_RawData[iRow][iCol];
				if (iValue < RawDataMin) {
					btmpresult = false;
					FTS_TEST_PRINT("rawdata failure.Node=(%d,%d),value=%d",
							iRow+1, iCol+1, iValue);
				}
			}
		}
	}
	ReCode = WriteReg(REG_NORMALIZE_TYPE, OriginValue);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERR("\n Write REG_NORMALIZE_TYPE error:%d", ReCode);
		goto TEST_ERR;
	}

	if (m_bV3TP) {
		ReCode = WriteReg(REG_MAPPING_SWITCH, strSwitch);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_ERR("\n Write REG_MAPPING_SWITCH errore: %d", ReCode);
			goto TEST_ERR;
		}
	}

	if (btmpresult) {
		FTS_TEST_ERR("\n\n//RawData Test is OK!");
		return RESULT_PASS;
	}
	if (!btmpresult) {
		FTS_TEST_ERR("\n\n//RawData Test is NG!");
		return RESULT_NG;
	}
	return RESULT_INVALID;

TEST_ERR:
	FTS_TEST_DBG("\n\n//RawData Test is NG!");
	return RESULT_INVALID;
}

static unsigned char GetPanelRows(unsigned char *pPanelRows)
{
	return ReadReg(REG_TX_NUM, pPanelRows);
}
static unsigned char GetPanelCols(unsigned char *pPanelCols)
{
	return ReadReg(REG_RX_NUM, pPanelCols);
}
static int StartScan(void)
{
	unsigned char RegVal = 0;
	unsigned char times = 0;
	const unsigned char MaxTimes = 20;
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;

	ReCode = ReadReg(DEVIDE_MODE_ADDR, &RegVal);
	if (ReCode == ERROR_CODE_OK) {
		RegVal |= 0x80;
		ReCode = WriteReg(DEVIDE_MODE_ADDR, RegVal);
		if (ReCode == ERROR_CODE_OK) {
			while (times++ < MaxTimes) {
				SysDelay(8);
				ReCode = ReadReg(DEVIDE_MODE_ADDR, &RegVal);
				if (ReCode == ERROR_CODE_OK) {
					if ((RegVal>>7) == 0)
						break;
				} else {
					FTS_TEST_DBG("StartScan read DEVIDE_MODE_ADDR error.");
					break;
				}
			}
			if (times < MaxTimes)
				ReCode = ERROR_CODE_OK;
			else {
				ReCode = ERROR_CODE_COMM_ERROR;
				FTS_TEST_DBG("times NOT < MaxTimes. error.");
			}
		} else
			FTS_TEST_DBG("StartScan write DEVIDE_MODE_ADDR error.");
	} else
		FTS_TEST_DBG("StartScan read DEVIDE_MODE_ADDR error.");
	return ReCode;
}
unsigned char ReadRawData(unsigned char Freq, unsigned char LineNum, int ByteNum, int *pRevBuffer)
{
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;
	unsigned char I2C_wBuffer[3];
	int i, iReadNum;
	unsigned short BytesNumInTestMode1 = 0;

	iReadNum = ByteNum/BYTES_PER_TIME;
	if (0 != (ByteNum%BYTES_PER_TIME))
		iReadNum++;

	if (ByteNum <= BYTES_PER_TIME)
		BytesNumInTestMode1 = ByteNum;
	else
		BytesNumInTestMode1 = BYTES_PER_TIME;
	ReCode = WriteReg(REG_LINE_NUM, LineNum);

	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_DBG("Failed to write REG_LINE_NUM! ");
		goto READ_ERR;
	}

	I2C_wBuffer[0] = REG_RawBuf0;
	if (ReCode == ERROR_CODE_OK) {
		focal_msleep(10);
		ReCode = Comm_Base_IIC_IO(I2C_wBuffer, 1,
				m_ucTempData, BytesNumInTestMode1);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_DBG("read rawdata Comm_Base_IIC_IO Failed!1 ");
			goto READ_ERR;
		}
	}

	for (i = 1; i < iReadNum; i++) {
		if (ReCode != ERROR_CODE_OK)
			break;

		if (i == iReadNum-1) {
			focal_msleep(10);
			ReCode = Comm_Base_IIC_IO(NULL, 0,
					m_ucTempData+BYTES_PER_TIME*i, ByteNum-BYTES_PER_TIME*i);
			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_DBG("read rawdata Comm_Base_IIC_IO Failed!2 ");
				goto READ_ERR;
			}
		} else {
			focal_msleep(10);
			ReCode = Comm_Base_IIC_IO(NULL, 0,
					m_ucTempData+BYTES_PER_TIME*i, BYTES_PER_TIME);

			if (ReCode != ERROR_CODE_OK) {
				FTS_TEST_DBG("read rawdata Comm_Base_IIC_IO Failed!3 ");
				goto READ_ERR;
			}
		}

	}

	if (ReCode == ERROR_CODE_OK)
		for (i = 0; i < (ByteNum>>1); i++)
			pRevBuffer[i] = (m_ucTempData[i<<1]<<8)+m_ucTempData[(i<<1)+1];

READ_ERR:
	return ReCode;

}


static unsigned char GetChannelNum(void)
{
	unsigned char ReCode;
	unsigned char rBuffer[1];

	ReCode = GetPanelRows(rBuffer);
	if (ReCode == ERROR_CODE_OK) {
		g_ScreenSetParam.iTxNum = rBuffer[0];
		if (g_ScreenSetParam.iTxNum > g_ScreenSetParam.iUsedMaxTxNum) {
			FTS_TEST_DBG("Failed to get Tx number, Get num = %d, UsedMaxNum = %d",
				g_ScreenSetParam.iTxNum, g_ScreenSetParam.iUsedMaxTxNum);
			g_ScreenSetParam.iTxNum = 0;
			return ERROR_CODE_INVALID_PARAM;
		}
	} else
		FTS_TEST_DBG("Failed to get Tx number");

	ReCode = GetPanelCols(rBuffer);
	if (ReCode == ERROR_CODE_OK) {
		g_ScreenSetParam.iRxNum = rBuffer[0];
		if (g_ScreenSetParam.iRxNum > g_ScreenSetParam.iUsedMaxRxNum) {
			FTS_TEST_DBG("Failed to get Rx number, Get num = %d, UsedMaxNum = %d",
				g_ScreenSetParam.iRxNum, g_ScreenSetParam.iUsedMaxRxNum);
			g_ScreenSetParam.iRxNum = 0;
			return ERROR_CODE_INVALID_PARAM;
		}
	} else
		FTS_TEST_DBG("Failed to get Rx number");

	return ReCode;

}
static unsigned char GetRawData(void)
{
	unsigned char ReCode = ERROR_CODE_OK;
	int iRow = 0;
	int iCol = 0;

	ReCode = EnterFactory();
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_DBG("Failed to Enter Factory Mode...");
		return ReCode;
	}
	if (0 == (g_ScreenSetParam.iTxNum + g_ScreenSetParam.iRxNum)) {
		ReCode = GetChannelNum();
		if (ERROR_CODE_OK != ReCode) {
			FTS_TEST_DBG("Error Channel Num...");
			return ERROR_CODE_INVALID_PARAM;
		}
	}

	FTS_TEST_DBG("Start Scan ...");
	ReCode = StartScan();
	if (ERROR_CODE_OK != ReCode) {
		FTS_TEST_DBG("Failed to Scan ...");
		return ReCode;
	}

	memset(m_RawData, 0, sizeof(m_RawData));
	ReCode = ReadRawData(1, 0xAA,
			(g_ScreenSetParam.iTxNum * g_ScreenSetParam.iRxNum)*2, m_iTempRawData);
	for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++)
		for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++)
			m_RawData[iRow][iCol] = m_iTempRawData[iRow*g_ScreenSetParam.iRxNum + iCol];
	return ReCode;
}
static void ShowRawData(void)
{
	int iRow, iCol;

	for (iRow = 0; iRow < g_ScreenSetParam.iTxNum; iRow++) {
		FTS_TEST_PRINT("Tx%2d:  ", iRow+1);
		for (iCol = 0; iCol < g_ScreenSetParam.iRxNum; iCol++)
			FTS_TEST_PRINT("%5d    ", m_RawData[iRow][iCol]);
		FTS_TEST_PRINT("\n ");
	}
}

static unsigned char GetChannelNumNoMapping(void)
{
	unsigned char ReCode;
	unsigned char rBuffer[1];

	FTS_TEST_DBG("Get Tx Num...");
	ReCode = ReadReg(REG_TX_NOMAPPING_NUM, rBuffer);
	if (ReCode == ERROR_CODE_OK)
		g_ScreenSetParam.iTxNum = rBuffer[0];
	else
		FTS_TEST_DBG("Failed to get Tx number");

	FTS_TEST_DBG("Get Rx Num...");
	ReCode = ReadReg(REG_RX_NOMAPPING_NUM, rBuffer);
	if (ReCode == ERROR_CODE_OK)
		g_ScreenSetParam.iRxNum = rBuffer[0];
	else
		FTS_TEST_DBG("Failed to get Rx number");

	return ReCode;
}
unsigned char FT5X46_TestItem_WeakShortTest(struct i2c_client *client)
{
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;
	int i = 0;
	int iAllAdcDataNum = 63;
	int iMaxTx = 35;
	unsigned char iTxNum, iRxNum, iChannelNum;
	int iClbData_Ground, iClbData_Mutual, iOffset;
	unsigned char IcValue = 0;
	unsigned char strSwitch = 1;
	bool  bCapShortTest = false;
	int *iAdcData  = NULL;
	bool btmpresult = true;
	int fKcal = 0;
	int *fMShortResistance = NULL, *fGShortResistance = NULL;
	int iDoffset = 0, iDsen = 0, iDrefn = 0;
	int iMin_CG = 0;
	int iCount = 0;
	int iMin_CC = 0;
	int iDCal = 0;
	int iMa = 0;
	int iRsen = 57;
	int iCCRsen = 57;
	struct ft5x46_data *ft5x46 = i2c_get_clientdata(client);
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;

	FTS_TEST_DBG("\nTest Item:Weak Short-Circuit Test\n");
	InitTest();
	ReCode = EnterWork();
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERR(" EnterWork failed.. Error Code: %d", ReCode);
		goto TEST_ERR;
	}
	SysDelay(200);
	ReCode = ReadReg(0xB1, &IcValue);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERR("\n Read 0xB1 IcValue error. Error Code: %d\n", ReCode);
		goto TEST_ERR;
	} else
		FTS_TEST_DBG(" IcValue:0x%02x\n", IcValue);
	ReCode = EnterFactory();
	SysDelay(100);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERR(" EnterFactory failed.. Error Code: %d", ReCode);
		goto TEST_ERR;
	}

	if (m_bV3TP) {
		ReCode = ReadReg(REG_MAPPING_SWITCH, &strSwitch);
		if (strSwitch != 1) {
			ReCode = WriteReg(REG_MAPPING_SWITCH, 1);
			SysDelay(20);
			if (ReCode != ERROR_CODE_OK)
				FTS_TEST_ERR("\r\nFailed to restore mapping type!\r\n ");
			GetChannelNumNoMapping();
			iTxNum = g_ScreenSetParam.iTxNum;
			iRxNum = g_ScreenSetParam.iRxNum;
		}
	} else {
		ReCode = ReadReg(0x02, &iTxNum);
		ReCode = ReadReg(0x03, &iRxNum);
		FTS_TEST_DBG("Newly acquired TxNum:%d, RxNum:%d", iTxNum, iRxNum);
	}

	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_ERR("ReCode  error. Error Code: %d\n", ReCode);
		goto TEST_ERR;
	}

	iChannelNum = iTxNum + iRxNum;
	iMaxTx = iTxNum;
	iAllAdcDataNum = 1 + (1 + iTxNum + iRxNum)*2;
	for (i = 0; i < 5; i++) {
		ReCode = StartScan();
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_ERR("StartScan Failed!\n");
			SysDelay(100);
		} else {
			FTS_TEST_DBG("StartScan OK!\n");
			break;
		}
	}
	if (i >= 5) {
		FTS_TEST_ERR("StartScan Failed for several times.!\n");
		goto TEST_ERR;
	}
	iAdcData = fts_malloc(iAllAdcDataNum*sizeof(int));
	memset(iAdcData, 0, iAllAdcDataNum);
	for (i = 0; i < 5; i++) {
		memset(iAdcData, 0, iAllAdcDataNum);

		FTS_TEST_DBG("WeakShort_GetAdcData times: %d", i);

		ReCode = WeakShort_GetAdcData(iAllAdcDataNum*2, iAdcData);
		SysDelay(50);
		if (ReCode != ERROR_CODE_OK)
			continue;
		else {
			if (0 == iAdcData[0] && 0 == iAdcData[1])
				continue;
			else
				break;
		}
	}
	if (i >= 5)	{
		FTS_TEST_ERR("WeakShort_GetAdcData or ADC data error. tried times: %d", i);
		goto TEST_ERR;
	}

	FTS_TEST_DBG("");

	iOffset = iAdcData[0];
	iClbData_Ground = iAdcData[1];
	iClbData_Mutual = iAdcData[2 + iChannelNum];

	for (i = 0; i < iAllAdcDataNum; i++) {
		if (i <= (iChannelNum + 1)) {
			if (i == 0)
				FTS_TEST_PRINT("\n\n\nOffset %02d: %4d,\n", i, iAdcData[i]);
			else if (i == 1)
				FTS_TEST_PRINT("Ground %02d: %4d,\n", i, iAdcData[i]);
			else if (i <= (iMaxTx + 1))
				FTS_TEST_PRINT("Tx%02d: %4d,      ", i-1, iAdcData[i]);
			else if (i <= (iChannelNum + 1))
				FTS_TEST_PRINT("Rx%02d: %4d,      ", i - iMaxTx-1, iAdcData[i]);
			if (i % 10 == 0)
				FTS_TEST_PRINT("\n");
		} else {
			if (i == (iChannelNum + 2))
				FTS_TEST_PRINT("\n\n\nMultual %02d: %4d,\n", i, iAdcData[i]);
			else if (i <= (iMaxTx)+(iChannelNum + 2))
				FTS_TEST_PRINT("Tx%02d: %4d,       ", i - (iChannelNum + 2), iAdcData[i]);
			else if (i < iAllAdcDataNum)
				FTS_TEST_PRINT("Rx%02d: %4d,       ", i - iMaxTx - (iChannelNum + 2), iAdcData[i]);
			if (i % 10 == 0)
				FTS_TEST_PRINT("\n");
		}
	}
	FTS_TEST_PRINT("\r\n");
	FTS_TEST_DBG("");

	fMShortResistance = fts_malloc(iChannelNum*sizeof(int));
	memset(fMShortResistance, 0, iChannelNum);
	fGShortResistance = fts_malloc(iChannelNum*sizeof(int));
	memset(fGShortResistance, 0, iChannelNum);

	iMin_CG = pdata->imin_cg;
	iDoffset = iOffset - 1024;
	iDrefn = iClbData_Ground;

	FTS_TEST_DBG("Drefp:%5d	\r\n", iDrefn);
	FTS_TEST_DBG("Doffset:%5d\r\n", iDoffset);
	FTS_TEST_DBG("Rshort(Ground):\n\n\n");
	fKcal = 1;
	FTS_TEST_DBG("Short Circuit (Channel and Ground):\r\n");
	for (i = 0; i < iChannelNum; i++) {
		iDsen = iAdcData[i+2];
		FTS_TEST_PRINT("%5d	", iDsen);
		if (i+1 == iMaxTx)
			FTS_TEST_PRINT("\n");
		if ((2047+iDoffset) - iDsen <= 0)
			continue;
		if (i == iMaxTx)
			FTS_TEST_PRINT("\n");
		if (IcValue <= 0x05 || IcValue == 0xff)
			fGShortResistance[i] = (iDsen - iDoffset + 410) * 25 * fKcal / (2047 + iDoffset - iDsen) - 3;
		else {
			if (iDrefn - iDsen <= 0) {
				fGShortResistance[i] = iMin_CG;
				FTS_TEST_PRINT("%02d  ", fGShortResistance[i]);
					continue;
			}
			fGShortResistance[i] = (((iDsen - iDoffset + 384) / (iDrefn - iDsen) * 57) - 1);
		}
		if (fGShortResistance[i] < 0)
			fGShortResistance[i] = 0;
		FTS_TEST_PRINT("%02d  ", fGShortResistance[i]);
		if ((iMin_CG > fGShortResistance[i]) || (iDsen - iDoffset < 0)) {
			iCount++;
			if (i+1 <= iMaxTx)
				FTS_TEST_PRINT("Tx%02d: %02d (k次),	", i+1, fGShortResistance[i]);
			else
				FTS_TEST_PRINT("Rx%02d: %02d (k次),	", i+1 - iMaxTx, fGShortResistance[i]);
			if (iCount % 10 == 0)
				FTS_TEST_PRINT("\n");
		}
	}
	FTS_TEST_PRINT("\n");

	if (iCount > 0)
		btmpresult = false;
	iMin_CC = pdata->imin_cc;
	if ((IcValue == 0x06 || IcValue < 0xff) && iRsen != iCCRsen)
		iRsen = iCCRsen;
	iDoffset = iOffset - 1024;
	iDrefn = iClbData_Mutual;
	fKcal = 1.0;

	FTS_TEST_DBG("\n\nShort Circuit (Channel and Channel):\n");
	iCount = 0;


	FTS_TEST_DBG("Drefp:    %5d	\r\n", iDrefn);
	FTS_TEST_DBG("Doffset:  %5d	\r\n", iDoffset);
	FTS_TEST_DBG("Rshort(Channel):");
	iDCal = max(iDrefn, 116 + iDoffset);

	for (i = 0; i < iChannelNum; i++) {
		iDsen = iAdcData[i+iChannelNum + 3];

		FTS_TEST_PRINT("%5d   ", iDsen);
		if (i+1 == iMaxTx)
			FTS_TEST_PRINT("\n");
		if (IcValue <= 0x05 || IcValue == 0xff)
			if (iDsen - iDrefn < 0)
				continue;

		if (i == iMaxTx)
			FTS_TEST_PRINT("\n");

		if (IcValue <= 0x05 || IcValue == 0xff) {
			iMa = iDsen - iDCal;
			iMa = iMa ? iMa : 1;
			fMShortResistance[i] = ((2047 + iDoffset - iDCal) * 24 / iMa - 27) * fKcal - 6;
		} else {
			if (iDrefn - iDsen <= 0) {
				fMShortResistance[i] = iMin_CC;
				FTS_TEST_PRINT("%02d  ", fMShortResistance[i]);
				continue;
			}

			fMShortResistance[i] = (iDsen - iDoffset - 123) * iRsen * fKcal / (iDrefn - iDsen) - 2;
		}


		FTS_TEST_PRINT("%02d  ", fMShortResistance[i]);

		if (fMShortResistance[i] < 0 && fMShortResistance[i] >= -240)
			fMShortResistance[i] = 0;
		else if (fMShortResistance[i] < -240)
			continue;

		if (fMShortResistance[i] <= 0  || fMShortResistance[i] < iMin_CC) {
			iCount++;
			if (i+1 <= iMaxTx)
				FTS_TEST_PRINT("Tx%02d: %02d(k次),	", i+1, fMShortResistance[i]);
			else
				FTS_TEST_PRINT("Rx%02d: %02d(k次),	", i+1 - iMaxTx, fMShortResistance[i]);
			if (iCount % 10 == 0)
				FTS_TEST_PRINT("\n");
		}
	}
	FTS_TEST_PRINT("\n");

	if (iCount > 0 && !bCapShortTest)
		btmpresult = false;

	if (bCapShortTest && iCount)
		FTS_TEST_DBG(" bCapShortTest && iCount.  need to add ......");

	if (m_bV3TP) {
		ReCode = WriteReg(REG_MAPPING_SWITCH, strSwitch);
		SysDelay(50);
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_ERR("Failed to restore mapping type!\r\n");
			goto TEST_ERR;
		}
		ReCode = GetChannelNum();
		if (ReCode != ERROR_CODE_OK) {
			FTS_TEST_ERR("\nGetChannelNum error.Code: %d", ReCode);
			goto TEST_ERR;
		}
		ReCode = GetRawData();
	}
	if (NULL != iAdcData) {
		fts_free(iAdcData);
		iAdcData = NULL;
	}

	if (NULL != fMShortResistance) {
		fts_free(fMShortResistance);
		fMShortResistance = NULL;
	}

	if (NULL != fGShortResistance) {
		fts_free(fGShortResistance);
		fGShortResistance = NULL;
	}
	if (btmpresult) {
		FTS_TEST_DBG("\r\n\r\n//Weak Short Test is OK.");
		return RESULT_PASS;
	}

	FTS_TEST_DBG("\r\n\r\n//Weak Short Test is NG.");
	return RESULT_NG;

TEST_ERR:
	if (NULL != iAdcData) {
		fts_free(iAdcData);
		iAdcData = NULL;
	}
	if (NULL != fMShortResistance) {
		fts_free(fMShortResistance);
		fMShortResistance = NULL;
	}
	if (NULL != fGShortResistance) {
		fts_free(fGShortResistance);
		fGShortResistance = NULL;
	}
	FTS_TEST_DBG("%s invalid\n\n\n\n", __func__);
	return RESULT_INVALID;
}

static unsigned char WeakShort_GetAdcData(int AllAdcDataLen, int *pRevBuffer)
{
	unsigned char ReCode = ERROR_CODE_COMM_ERROR;
	int iReadDataLen = AllAdcDataLen;
	unsigned char *pDataSend = NULL;
	unsigned char Data = 0xff;
	int i = 0;
	bool bAdcOK = false;

	FTS_TEST_DBG("\n");
	pDataSend = fts_malloc(iReadDataLen + 1);
	if (pDataSend == NULL)
		return ERROR_CODE_ALLOCATE_BUFFER_ERROR;
	memset(pDataSend, 0, iReadDataLen + 1);
	ReCode = WriteReg(0x07, 0x01);
	if (ReCode != ERROR_CODE_OK) {
		FTS_TEST_DBG("WriteReg error.\n");
		return ReCode;
	}
	SysDelay(100);
	for (i = 0; i < 100*5; i++) {
		SysDelay(10);
		ReCode = ReadReg(0x07, &Data);
		if (ReCode == ERROR_CODE_OK)
			if (Data == 0) {
				bAdcOK = true;
				break;
			}
	}
	if (!bAdcOK) {
		FTS_TEST_ERR("ADC data NOT ready.  error.\n");
		ReCode = ERROR_CODE_COMM_ERROR;
		goto EndGetAdc;
	}
	SysDelay(300);
	pDataSend[0] = 0xF4;
	ReCode = Comm_Base_IIC_IO(pDataSend, 1, pDataSend + 1, iReadDataLen);
	if (ReCode == ERROR_CODE_OK) {
		FTS_TEST_PRINT("\n Adc Data:\n");
		for (i = 0; i < iReadDataLen/2; i++) {
			pRevBuffer[i] = (pDataSend[1 + 2*i]<<8) + pDataSend[1 + 2*i + 1];
			FTS_TEST_PRINT("%d,   ", pRevBuffer[i]);
		}
		FTS_TEST_PRINT("\n");
	} else
		FTS_TEST_DBG("Comm_Base_IIC_IO error. error:%d.\n", ReCode);
EndGetAdc:
	if (pDataSend != NULL) {
		fts_free(pDataSend);
		pDataSend = NULL;
	}
	FTS_TEST_DBG(" END.\n");
	return ReCode;
}
