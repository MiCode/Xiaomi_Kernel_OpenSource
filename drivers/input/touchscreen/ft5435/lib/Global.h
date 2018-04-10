/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: Global.c
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: global function for test
*
************************************************************************/
#ifndef _GLOBAL_H
#define _GLOBAL_H

#include <linux/kernel.h>
#include "DetailThreshold.h"

/*-----------------------------------------------------------
Error Code for Comm
-----------------------------------------------------------*/
#define ERROR_CODE_OK								0x00
#define ERROR_CODE_CHECKSUM_ERROR				0x01
#define ERROR_CODE_INVALID_COMMAND				0x02
#define ERROR_CODE_INVALID_PARAM					0x03
#define ERROR_CODE_IIC_WRITE_ERROR				0x04
#define ERROR_CODE_IIC_READ_ERROR					0x05
#define ERROR_CODE_WRITE_USB_ERROR				0x06
#define ERROR_CODE_WAIT_RESPONSE_TIMEOUT		0x07
#define ERROR_CODE_PACKET_RE_ERROR				0x08
#define ERROR_CODE_NO_DEVICE						0x09
#define ERROR_CODE_WAIT_WRITE_TIMEOUT			0x0a
#define ERROR_CODE_READ_USB_ERROR				0x0b
#define ERROR_CODE_COMM_ERROR					0x0c
#define ERROR_CODE_ALLOCATE_BUFFER_ERROR		0x0d
#define ERROR_CODE_DEVICE_OPENED					0x0e
#define ERROR_CODE_DEVICE_CLOSED					0x0f

/*-----------------------------------------------------------
Test Status
-----------------------------------------------------------*/
#define		RESULT_NULL			0
#define		RESULT_PASS			1
#define		RESULT_NG		    		2
#define		RESULT_TESTING		3
#define		RESULT_TBD				4
#define		RESULT_REPLACE		5
#define		RESULT_CONNECTING		6

enum IC_Type {
	IC_FT5X36 = 0x10,
	IC_FT5X36i = 0x11,
	IC_FT3X16 = 0x12,
	IC_FT3X26 = 0x13,

	IC_FT5X46 = 0x21,
	IC_FT5X46i = 0x22,
	IC_FT5526 = 0x23,
	IC_FT3X17 = 0x24,
	IC_FT5436 = 0x25,
	IC_FT3X27 = 0x26,
	IC_FT5526I = 0x27,
	IC_FT5416 = 0x28,
	IC_FT5426 = 0x29,
	IC_FT5435 = 0x2A,
	IC_FT6X06 = 0x30,
	IC_FT3X06 = 0x31,
	IC_FT6X36 = 0x40,
	IC_FT3X07 = 0x41,
	IC_FT6416 = 0x42,
	IC_FT6426 = 0x43,
	IC_FT5X16 = 0x50,
	IC_FT5X12 = 0x51,
	IC_FT5506 = 0x60,
	IC_FT5606 = 0x61,
	IC_FT5816 = 0x62,
	IC_FT5822 = 0x70,
	IC_FT5626 = 0x71,
	IC_FT5726 = 0x72,
	IC_FT5826B = 0x73,
	IC_FT5826S = 0x74,
	IC_FT5306  = 0x80,
	IC_FT5406  = 0x81,
	IC_FT8606  = 0x90,
	IC_FT8716  = 0xA0,
	IC_FT3C47U = 0xB0,
};
#define MAX_IC_TYPE		32

struct StruScreenSeting {
	int iSelectedIC;
	int iTxNum;
	int iRxNum;
	int isNormalize;
	int iUsedMaxTxNum;
	int iUsedMaxRxNum;

	unsigned char iChannelsNum;
	unsigned char iKeyNum;

};

struct stTestItem {
	unsigned char ItemType;
	unsigned char TestNum;
	unsigned char TestResult;
	unsigned char ItemCode;
};

enum NORMALIZE_Type {
	Overall_Normalize = 0,
	Auto_Normalize = 1,
};

enum PROOF_TYPE {
	Proof_Normal,
	Proof_Level0,
	Proof_NoWaterProof,
};

extern struct stCfg_MCap_DetailThreshold g_stCfg_MCap_DetailThreshold;
extern struct stCfg_SCap_DetailThreshold g_stCfg_SCap_DetailThreshold;

extern struct StruScreenSeting g_ScreenSetParam;
extern struct stTestItem g_stTestItem[1][MAX_TEST_ITEM];

extern int g_TestItemNum;
extern char g_strIcName[20];


int GetPrivateProfileString(char *section, char *ItemName, char *defaultvalue, char *returnValue, char *IniFile);
void focal_msleep(int ms);
void SysDelay(int ms);
int focal_abs(int value);
unsigned char get_ic_code(char *strIcName);
void get_ic_name(unsigned char ucIcCode, char *strIcName);

void OnInit_InterfaceCfg(char *strIniFile);

int ReadReg(unsigned char RegAddr, unsigned char *RegData);
int WriteReg(unsigned char RegAddr, unsigned char RegData);
unsigned char Comm_Base_IIC_IO(unsigned char *pWriteBuffer, int  iBytesToWrite, unsigned char *pReadBuffer, int iBytesToRead);

unsigned char EnterWork(void);
unsigned char EnterFactory(void);



#endif
