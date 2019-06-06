/************************************************************************
* Copyright (C) 2012-2017, Focaltech Systems (R)£¬All Rights Reserved.
* Copyright (C) 2019 XiaoMi, Inc.
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

enum NodeType
{
    NODE_INVALID_TYPE = 0,
    NODE_VALID_TYPE = 1,
    NODE_KEY_TYPE = 2,
    NODE_AST_TYPE = 3,
};


typedef int (*FTS_I2C_READ_FUNCTION)(unsigned char *, int , unsigned char *, int);
typedef int (*FTS_I2C_WRITE_FUNCTION)(unsigned char *, int);

extern FTS_I2C_READ_FUNCTION fts_i2c_read_test;
extern FTS_I2C_WRITE_FUNCTION fts_i2c_write_test;

extern int init_i2c_read_func(FTS_I2C_READ_FUNCTION fpI2C_Read);
extern int init_i2c_write_func(FTS_I2C_WRITE_FUNCTION fpI2C_Write);






int set_param_data(char *TestParamData);
boolean start_test_tp(void);
int get_test_data(char *pTestData);


int focaltech_test_main_init(void);
int focaltech_test_main_exit(void);


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

extern void fts_test_funcs(void);
struct StTestFuncs
{
    void (*OnInit_TestItem)(char *);
    void (*OnInit_BasicThreshold)(char *) ;
    void (*SetTestItem)(void) ;
    boolean (*Start_Test)(void);
    int (*Get_test_data)(char *);
};
extern struct StTestFuncs g_stTestFuncs;

struct StruScreenSeting
{
    int iSelectedIC;
    int iTxNum;
    int iRxNum;
    int isNormalize;
    int iUsedMaxTxNum;
    int iUsedMaxRxNum;

    unsigned char iChannelsNum;
    unsigned char iKeyNum;
};
extern struct StruScreenSeting g_ScreenSetParam;
struct stTestItem
{
    unsigned char ItemType;
    unsigned char TestNum;
    unsigned char TestResult;
    unsigned char ItemCode;


};
extern struct stTestItem g_stTestItem[1][MAX_TEST_ITEM];

struct structSCapConfEx
{
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
extern struct structSCapConfEx g_stSCapConfEx;

enum NORMALIZE_Type
{
    Overall_Normalize = 0,
    Auto_Normalize = 1,
};

enum PROOF_TYPE
{
    Proof_Normal,
    Proof_Level0,
    Proof_NoWaterProof,
};

/*-----------------------------------------------------------
IC Capacitance Type  0:Self Capacitance, 1:Mutual Capacitance, 2:IDC
-----------------------------------------------------------*/
enum enum_Report_Protocol_Type
{
    Self_Capacitance = 0,
    Mutual_Capacitance= 1,
    IDC_Capacitance = 2,
};

#if (FTS_CHIP_TEST_TYPE == FT6X36_TEST)
#define IC_Capacitance_Type     0
#elif ((FTS_CHIP_TEST_TYPE == FT3C47_TEST) || (FTS_CHIP_TEST_TYPE == FT5822_TEST) || (FTS_CHIP_TEST_TYPE == FT5X46_TEST) || (FTS_CHIP_TEST_TYPE == FT3D47_TEST))
#define IC_Capacitance_Type     1
#elif ((FTS_CHIP_TEST_TYPE == FT8606_TEST) || (FTS_CHIP_TEST_TYPE == FT8607_TEST) || (FTS_CHIP_TEST_TYPE == FT8716_TEST)  || (FTS_CHIP_TEST_TYPE == FT8736_TEST) || (FTS_CHIP_TEST_TYPE == FTE716_TEST) || (FTS_CHIP_TEST_TYPE == FTE736_TEST) || (FTS_CHIP_TEST_TYPE == FT8006_TEST))
#define IC_Capacitance_Type            2
#endif




extern struct stCfg_MCap_DetailThreshold g_stCfg_MCap_DetailThreshold;
extern struct stCfg_SCap_DetailThreshold g_stCfg_SCap_DetailThreshold;
extern struct stCfg_Incell_DetailThreshold g_stCfg_Incell_DetailThreshold;


extern int g_TestItemNum;/*test item num*/
extern char g_strIcName[20];/*IC Name*/
extern char *g_pStoreAllData;

int GetPrivateProfileString(char *section, char *ItemName, char *defaultvalue, char *returnValue, char *IniFile);
void focal_msleep(int ms);
void SysDelay(int ms);
int focal_abs(int value);


void OnInit_InterfaceCfg(char * strIniFile);

int ReadReg(unsigned char RegAddr, unsigned char *RegData);
int WriteReg(unsigned char RegAddr, unsigned char RegData);
unsigned char Comm_Base_IIC_IO(unsigned char *pWriteBuffer, int  iBytesToWrite, unsigned char *pReadBuffer, int iBytesToRead);

unsigned char EnterWork(void);
unsigned char EnterFactory(void);

void fts_SetTestItemCodeName(unsigned char ucitemcode);

extern void *fts_malloc(size_t size);
extern void fts_free(void *p);

extern int InitTest(void);
extern void FinishTest(void);
extern void InitStoreParamOfTestData(void);
extern  void MergeAllTestData(void);
extern int AllocateMemory(void);
extern  void FreeMemory(void);

extern char *g_pTmpBuff;
extern char *g_pStoreMsgArea;
extern int g_lenStoreMsgArea;
extern char *g_pMsgAreaLine2;
extern int g_lenMsgAreaLine2;
extern char *g_pStoreDataArea;
extern int g_lenStoreDataArea;
extern unsigned char m_ucTestItemCode;
extern int m_iStartLine;
extern int m_iTestDataCount;

extern char *TestResult ;
extern int TestResultLen;

#define FOCAL_TEST_DEBUG_EN     1
#if (FOCAL_TEST_DEBUG_EN)
#define FTS_TEST_DBG(fmt, args...) do {printk(KERN_ERR "[FTS] [TEST]%s. line: %d.  "fmt"\n",  __FUNCTION__, __LINE__, ##args);} while (0)
#define FTS_TEST_FUNC_ENTER() printk(KERN_ERR "[FTS][TEST]%s: Enter(%d)\n", __func__, __LINE__)
#define FTS_TEST_FUNC_EXIT()  printk(KERN_ERR "[FTS][TEST]%s: Exit(%d)\n", __func__, __LINE__)
#else
#define FTS_TEST_DBG(fmt, args...) do{}while(0)
#define FTS_TEST_FUNC_ENTER()
#define FTS_TEST_FUNC_EXIT()
#endif

#define FTS_TEST_INFO(fmt, args...) do { printk(KERN_ERR "[FTS][TEST][Info]%s. line: %d.  "fmt"\n",  __FUNCTION__, __LINE__, ##args);} while (0)
#define FTS_TEST_ERROR(fmt, args...) do { printk(KERN_ERR "[FTS][TEST][Error]%s. line: %d.  "fmt"\n",  __FUNCTION__, __LINE__, ##args);} while (0)



#endif
