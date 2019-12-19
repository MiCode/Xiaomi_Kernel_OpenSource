
/************************************************************************
* Copyright (C) 2012-2015, Focaltech Systems (R)，All Rights Reserved.
* Copyright (C) 2019 XiaoMi, Inc.
*
* File Name: focaltech_test_main.h
*
* Author: Software Development Team, AE
*
* Created: 2015-07-14
*
* Abstract: test entry for all IC
*
************************************************************************/
#ifndef _TEST_LIB_H
#define _TEST_LIB_H
#include <linux/kernel.h>
#define boolean unsigned char
#define bool unsigned char
#define BYTE unsigned char
#define false 0
#define true  1


#define MIN_HOLE_LEVEL   (-1)
#define MAX_HOLE_LEVEL   0x7F
#define ERROR_CODE_OK							0x00
#define ERROR_CODE_CHECKSUM_ERROR				0x01
#define ERROR_CODE_INVALID_COMMAND				0x02
#define ERROR_CODE_INVALID_PARAM				0x03
#define ERROR_CODE_IIC_WRITE_ERROR				0x04
#define ERROR_CODE_IIC_READ_ERROR				0x05
#define ERROR_CODE_WRITE_USB_ERROR				0x06
#define ERROR_CODE_WAIT_RESPONSE_TIMEOUT		0x07
#define ERROR_CODE_PACKET_RE_ERROR				0x08
#define ERROR_CODE_NO_DEVICE					0x09
#define ERROR_CODE_WAIT_WRITE_TIMEOUT			0x0a
#define ERROR_CODE_READ_USB_ERROR				0x0b
#define ERROR_CODE_COMM_ERROR					0x0c
#define ERROR_CODE_ALLOCATE_BUFFER_ERROR		0x0d
#define ERROR_CODE_DEVICE_OPENED				0x0e
#define ERROR_CODE_DEVICE_CLOSED				0x0f

/*-----------------------------------------------------------
Test Status
-----------------------------------------------------------*/
#define		RESULT_INVALID		0
#define		RESULT_PASS			2
#define		RESULT_NG		    1

/*-----------------------------------------------------------
read write max bytes per time
-----------------------------------------------------------*/
#define BYTES_PER_TIME		128
#define DEVIDE_MODE_ADDR	0x00
#define REG_CHIP_ID		0xA3
#define FTS_MALLOC_TYPE		1
#define TX_NUM_MAX			60
#define RX_NUM_MAX			60
enum NodeType {
	NODE_INVALID_TYPE = 0,
	NODE_VALID_TYPE = 1,
	NODE_KEY_TYPE = 2,
	NODE_AST_TYPE = 3,
};


enum enum_malloc_mode {
	kmalloc_mode = 0,
	vmalloc_mode = 1,
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

int fts_test_module_init(struct i2c_client *client);
int fts_test_module_exit(struct i2c_client *client);
extern struct i2c_client *fts_i2c_client;
extern int fts_i2c_write(struct i2c_client *client,
		char *writebuf, int writelen);
extern int fts_i2c_read(struct i2c_client *client,
		char *writebuf, int writelen, char *readbuf, int readlen);
void focal_msleep(int ms);
void SysDelay(int ms);
int focal_abs(int value);
int ReadReg(unsigned char RegAddr, unsigned char *RegData);
int WriteReg(unsigned char RegAddr, unsigned char RegData);
unsigned char Comm_Base_IIC_IO(unsigned char *pWriteBuffer, int iBytesToWrite,
		unsigned char *pReadBuffer, int iBytesToRead);
unsigned char EnterWork(void);
unsigned char EnterFactory(void);
void *fts_malloc(size_t size);
void fts_free(void *p);
#define FOCAL_TEST_DEBUG_EN	1

#if (FOCAL_TEST_DEBUG_EN)
#define FTS_TEST_DBG(fmt, args...) \
	pr_debug("[FTS] %s. line: %d.  "fmt"\n",\
			__func__, __LINE__, ##args)
#define FTS_TEST_PRINT(fmt, args...) pr_err(""fmt, ## args)
#define FTS_TEST_ERR(fmt, args...) \
	pr_err("[FTS] %s. line: %d.  "fmt"\n", \
			__func__, __LINE__, ##args)
#else
#define FTS_TEST_DBG(fmt, args...) do {} while (0)
#define FTS_TEST_PRINT(fmt, args...) do {} while (0)
#endif

#define FTS_TEST_ERROR(fmt, args...) do { printk(KERN_ERR "[FTS][TEST][Error]%s. line: %d.  "fmt"\n",  __FUNCTION__, __LINE__, ##args) ; } while (0)

#endif
