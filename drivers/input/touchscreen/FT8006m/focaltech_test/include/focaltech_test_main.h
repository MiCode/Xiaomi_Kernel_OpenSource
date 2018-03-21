/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2018 XiaoMi, Inc.
*
* File Name: focaltech_test_main.h
*
* Author: Software Development Team, AE
*
* Created: 2016-08-01
*
* Abstract: test entry for all IC
*
************************************************************************/
#ifndef _TEST_LIB_H
#define _TEST_LIB_H

#include "focaltech_test_detail_threshold.h"
#include "../focaltech_test_config.h"

#define boolean unsigned char
#define bool unsigned char
#define BYTE unsigned char
#define false 0
#define true  1

enum NodeType {
	NODE_INVALID_TYPE = 0,
	NODE_VALID_TYPE = 1,
	NODE_KEY_TYPE = 2,
	NODE_AST_TYPE = 3,
};


typedef int (*FTS_I2C_READ_FUNCTION)(unsigned char *, int , unsigned char *, int);
typedef int (*FTS_I2C_WRITE_FUNCTION)(unsigned char *, int);

extern FTS_I2C_READ_FUNCTION ft8006m_i2c_read_test;
extern FTS_I2C_WRITE_FUNCTION ft8006m_i2c_write_test;

extern int ft8006m_init_i2c_read_func(FTS_I2C_READ_FUNCTION fpI2C_Read);
extern int ft8006m_init_i2c_write_func(FTS_I2C_WRITE_FUNCTION fpI2C_Write);






int ft8006m_set_param_data(char *TestParamData);
boolean ft8006m_start_test_tp(void);
int m_get_test_data(char *pTestData);


int ft8006m_focaltech_test_main_init(void);
int ft8006m_focaltech_test_main_exit(void);


#define MIN_HOLE_LEVEL   (-1)
#define MAX_HOLE_LEVEL   0x7F
/*-----------------------------------------------------------
Error Code for Comm
-----------------------------------------------------------*/
#define ERROR_CODE_OK                           0x00
#define ERROR_CODE_CHECKSUM_ERROR               0x01
#define ERROR_CODE_INVALID_COMMAND              0x02
#define ERROR_CODE_INVALID_PARAM                0x03
#define ERROR_CODE_IIC_WRITE_ERROR              0x04
#define ERROR_CODE_IIC_READ_ERROR               0x05
#define ERROR_CODE_WRITE_USB_ERROR              0x06
#define ERROR_CODE_WAIT_RESPONSE_TIMEOUT        0x07
#define ERROR_CODE_PACKET_RE_ERROR              0x08
#define ERROR_CODE_NO_DEVICE                    0x09
#define ERROR_CODE_WAIT_WRITE_TIMEOUT           0x0a
#define ERROR_CODE_READ_USB_ERROR               0x0b
#define ERROR_CODE_COMM_ERROR                   0x0c
#define ERROR_CODE_ALLOCATE_BUFFER_ERROR        0x0d
#define ERROR_CODE_DEVICE_OPENED                0x0e
#define ERROR_CODE_DEVICE_CLOSED                0x0f

/*-----------------------------------------------------------
Test Status
-----------------------------------------------------------*/
#define RESULT_NULL                             0
#define RESULT_PASS                             1
#define RESULT_NG                               2
#define RESULT_TESTING                          3
#define RESULT_TBD                              4
#define RESULT_REPLACE                          5
#define RESULT_CONNECTING                       6

/*-----------------------------------------------------------

read write max bytes per time
-----------------------------------------------------------*/

#define BYTES_PER_TIME      128

extern void ft8006m_test_funcs(void);
struct StTestFuncs {
	void (*OnInit_TestItem)(char *);
	void (*OnInit_BasicThreshold)(char *) ;
	void (*SetTestItem)(void) ;
	boolean (*Start_Test)(void);
	int (*Get_test_data)(char *);
};
extern struct StTestFuncs ft8006m_g_stTestFuncs;

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
extern struct StruScreenSeting ft8006m_g_ScreenSetParam;
struct stTestItem {
	unsigned char ItemType;
	unsigned char TestNum;
	unsigned char TestResult;
	unsigned char ItemCode;


};
extern struct stTestItem ft8006m_g_stTestItem[1][MAX_TEST_ITEM];

struct structSCapConfEx {
	unsigned char ChannelXNum;
	unsigned char ChannelYNum;
	unsigned char KeyNum;
	unsigned char KeyNumTotal;
	bool bLeftKey1;
	bool bLeftKey2;
	bool bLeftKey3;
	bool bRightKey1;
	bool bRightKey2;
	bool bRightKey3;
};
extern struct structSCapConfEx ft8006m_g_stSCapConfEx;

enum NORMALIZE_Type {
	Overall_Normalize = 0,
	Auto_Normalize = 1,
};

enum PROOF_TYPE {
	Proof_Normal,
	Proof_Level0,
	Proof_NoWaterProof,
};

/*-----------------------------------------------------------
IC Capacitance Type  0:Self Capacitance, 1:Mutual Capacitance, 2:IDC
-----------------------------------------------------------*/
enum enum_Report_Protocol_Type {
	Self_Capacitance = 0,
	Mutual_Capacitance = 1,
	IDC_Capacitance = 2,
};

#if (FTS_CHIP_TEST_TYPE == FT6X36_TEST)
#define IC_Capacitance_Type     0
#elif ((FTS_CHIP_TEST_TYPE == FT3C47_TEST) || (FTS_CHIP_TEST_TYPE == FT5822_TEST) || (FTS_CHIP_TEST_TYPE == FT5X46_TEST) || (FTS_CHIP_TEST_TYPE == FT3D47_TEST))
#define IC_Capacitance_Type     1
#elif ((FTS_CHIP_TEST_TYPE == FT8606_TEST) || (FTS_CHIP_TEST_TYPE == FT8607_TEST) || (FTS_CHIP_TEST_TYPE == FT8716_TEST)  || (FTS_CHIP_TEST_TYPE == FT8736_TEST) || (FTS_CHIP_TEST_TYPE == FTE716_TEST) || (FTS_CHIP_TEST_TYPE == FTE736_TEST) || (FTS_CHIP_TEST_TYPE == FT8006_TEST))
#define IC_Capacitance_Type            2
#endif




extern struct stCfg_MCap_DetailThreshold ft8006m_g_stCfg_MCap_DetailThreshold;
extern struct stCfg_SCap_DetailThreshold ft8006m_g_stCfg_SCap_DetailThreshold;
extern struct stCfg_Incell_DetailThreshold ft8006m_g_stCfg_Incell_DetailThreshold;


extern int ft8006m_g_TestItemNum;/*test item num*/
extern char ft8006m_g_strIcName[20];/*IC Name*/
extern char *ft8006m_g_pStoreAllData;

int Ft8006m_GetPrivateProfileString(char *section, char *ItemName, char *defaultvalue, char *returnValue, char *IniFile);
void ft8006m_focal_msleep(int ms);
void Ft8006m_SysDelay(int ms);
int ft8006m_focal_abs(int value);


void Ft8006m_OnInit_InterfaceCfg(char *strIniFile);

int Ft8006m_ReadReg(unsigned char RegAddr, unsigned char *RegData);
int Ft8006m_WriteReg(unsigned char RegAddr, unsigned char RegData);
unsigned char Ft8006m_Comm_Base_IIC_IO(unsigned char *pWriteBuffer, int  iBytesToWrite, unsigned char *pReadBuffer, int iBytesToRead);

unsigned char Ft8006m_EnterWork(void);
unsigned char Ft8006m_EnterFactory(void);

void ft8006m_SetTestItemCodeName(unsigned char ucitemcode);

extern void *Ft8006m_fts_malloc(size_t size);
extern void Ft8006m_fts_free(void *p);

extern int Ft8006m_InitTest(void);
extern void Ft8006m_FinishTest(void);
extern void Ft8006m_InitStoreParamOfTestData(void);
extern  void Ft8006m_MergeAllTestData(void);
extern int Ft8006m_AllocateMemory(void);
extern  void Ft8006m_FreeMemory(void);

extern char *ft8006m_g_pTmpBuff;
extern char *ft8006m_g_pStoreMsgArea;
extern int ft8006m_g_lenStoreMsgArea;
extern char *ft8006m_g_pMsgAreaLine2;
extern int ft8006m_g_lenMsgAreaLine2;
extern char *ft8006m_g_pStoreDataArea;
extern int ft8006m_g_lenStoreDataArea;
extern unsigned char ft8006m_m_ucTestItemCode;
extern int ft8006m_m_iStartLine;
extern int ft8006m_m_iTestDataCount;

extern char *Ft8006m_TestResult ;
extern int Ft8006m_TestResultLen;

#define FOCAL_TEST_DEBUG_EN     0
#if (FOCAL_TEST_DEBUG_EN)
#define FTS_TEST_DBG(fmt, args...) do {printk(KERN_ERR "[FTS] [TEST]%s. line: %d.  "fmt"\n",  __FUNCTION__, __LINE__, ##args); } while (0)
#define FTS_TEST_FUNC_ENTER() printk(KERN_ERR "[FTS][TEST]%s: Enter(%d)\n", __func__, __LINE__)
#define FTS_TEST_FUNC_EXIT()  printk(KERN_ERR "[FTS][TEST]%s: Exit(%d)\n", __func__, __LINE__)
#else
#define FTS_TEST_DBG(fmt, args...) do {} while (0)
#define FTS_TEST_FUNC_ENTER()
#define FTS_TEST_FUNC_EXIT()
#endif

#define FTS_TEST_INFO(fmt, args...) do { printk(KERN_ERR "[FTS][TEST][Info]%s. line: %d.  "fmt"\n",  __FUNCTION__, __LINE__, ##args); } while (0)
#define FTS_TEST_ERROR(fmt, args...) do { printk(KERN_ERR "[FTS][TEST][Error]%s. line: %d.  "fmt"\n",  __FUNCTION__, __LINE__, ##args); } while (0)



#endif
