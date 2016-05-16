/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_hotknot.h
 *
 * @brief   This file defines the hotknot functions
 *
 *
 */

#ifndef __MSTAR_DRV_HOTKNOT_H__
#define __MSTAR_DRV_HOTKNOT_H__

#include "mstar_drv_common.h"
#include "mstar_drv_hotknot_queue.h"


#ifdef CONFIG_ENABLE_HOTKNOT
extern void CreateHotKnotMem(void);
extern void DeleteHotKnotMem(void);

typedef struct {
	u8		  nCmdId;
	u8		 *pSndData;
	u16		 nSndLen;
	u8		 *pRcvData;
	u16		 nRcvLen;
	u16		*pFwMode;
	s32		 nTimeOut;
} DrvCmd_t;



typedef struct {
	u8		 nHeader;
	u8		 nInstruction;
	u8		 szReserved[2];
} HotKnotCmd_t;

typedef struct {
	u8		 nHeader;
	u8		 nInstruction;
	u8		 nResult;
	u8		 szReserved[38];
	u8		 nIdentify;
	u8		 nCheckSum;
} DemoHotKnotCmdRet_t;

typedef struct {
	u8		 nHeader;
	u8		 nPacketLen_H;
	u8		 nPacketLen_L;
	u8		 nType;
	u8		 nInstruction;
	u8		 nResult;


} DebugHotKnotCmdRet_t;



typedef struct {
	u8		 nHeader;
	u8		 nInstruction;
	u8		 szReserved[2];
} HotKnotAuth_t;



typedef struct {
	u8		 nHeader;
	u8		 nInstruction;
	u16		nSMBusAdr;
	u8		 szData[16];
	u8		 nCheckSum;
} HotKnotWriteCipher_t;




typedef struct {
	u8		 nHeader;
	u8		 nInstruction;
	u16		nSMBusAdr;
	u8		 szData[129];
	u8		 nDataLen_H;
	u8		 nDataLen_L;
	u8		 nCheckSum;
} HotKnotSnd_t;

typedef struct {
	u8		 nHeader;
	u8		 nPacketLen_H;
	u8		 nPacketLen_L;
	u8		 nType;
	u8		 nInstruction;
	u8		 nResult;
	u8		 szReserved[143];
	u8		 nCheckSum;
} DemoHotKnotSndRet_t;

typedef struct {
	u8		 nHeader;
	u8		 nPacketLen_H;
	u8		 nPacketLen_L;
	u8		 nType;
	u8		 nInstruction;
	u8		 nResult;
	u8		 szReserved[143];
	u8		 szDebug[100];
	u8		 nCheckSum;
} DebugHotKnotSndRet_t;



typedef struct {
	u8		 nHeader;
	u8		 nInstruction;
	u8		 nRequireDataLen_H;
	u8		 nRequireDataLen_L;
} HotKnotRcv_t;

typedef struct {
	u8		 nHeader;
	u8		 nActualHotKnotLen_H;
	u8		 nActualHotKnotLen_L;
	u8		 szData[146];
	u8		 nCheckSum;
} DemoHotKnotLibRcvRet_t;

typedef struct {
	u8		 nHeader;
	u8		 nPacketLen_H;
	u8		 nPacketLen_L;
	u8		 nType;
	u8		 szData[143];
	u8		 nActualDataLen_H;
	u8		 nActualDataLen_L;
	u8		 nCheckSum;
} DemoHotKnotRcvRet_t;

typedef struct {
	u8		 nHeader;
	u8		 nPacketLen_H;
	u8		 nPacketLen_L;
	u8		 nType;
	u8		 szData[143];
	u8		 nActualDataLen_H;
	u8		 nActualDataLen_L;
	u8		 szDebug[100];
	u8		 nCheckSum;
} DebugHotKnotRcvRet_t;



typedef struct {
	u8		 nHeader;
	u8		 nInstruction;
	u8		 nRequireDataLen_H;
	u8		 nRequireDataLen_L;
} HotKnotGetQ_t;

typedef struct {
	u8		 nHeader;
	u16		nFront;
	u16		nRear;
	u8		 szData[HOTKNOT_QUEUE_SIZE];
	u8		 nCheckSum;
} DemoHotKnotGetQRet_t;


#define HOTKNOT_IOCTL_BASE			   99
#define HOTKNOT_IOCTL_RUN_CMD			_IOWR(HOTKNOT_IOCTL_BASE, 0, long)
#define HOTKNOT_IOCTL_QUERY_VENDOR	   _IOR('G', 28, char[30])


#define HOTKNOT_CMD					  0x60
#define HOTKNOT_AUTH					 0x61
#define HOTKNOT_SEND					 0x62
#define HOTKNOT_RECEIVE				  0x63



#define ENABLE_HOTKNOT				   0xA0
#define DISABLE_HOTKNOT				  0xA1
#define ENTER_MASTER_MODE				0xA2
#define EXIT_MASTER_MODE				 0xA3
#define ENTER_SLAVE_MODE				 0xA4
#define EXIT_SLAVE_MODE				  0xA5
#define READ_PAIR_STATE				  0xA6
#define EXIT_READ_PAIR_STATE			 0xA7
#define ENTER_TRANSFER_MODE			  0xA8
#define EXIT_TRANSFER_MODE			   0xA9
#define READ_DEPART_STATE				0xAA


#define AUTH_INIT						0xB0
#define AUTH_GETKEYINDEX				 0xB1
#define AUTH_READSCRAMBLECIPHER		  0xB2


#define QUERY_VERSION					0xB3


#define AUTH_WRITECIPHER				 0xC0
#define SEND_DATA						0xC1
#define ADAPTIVEMOD_BEGIN				0xC3


#define RECEIVE_DATA					 0xD0


#define SEND_DATA_TEST				   0xC2


#define GET_QUEUE						0xD1

#define DEMO_PD_PACKET_ID				0x5A
#define DEMO_PD_PACKET_IDENTIFY		  0xC0
#define HOTKNOT_PACKET_ID				0xA7
#define HOTKNOT_PACKET_TYPE			  0x41
#define HOTKNOT_RECEIVE_PACKET_TYPE	  0x40


#define HOTKNOT_CMD_LEN				  4
#define HOTKNOT_AUTH_LEN				 4
#define HOTKNOT_WRITECIPHER_LEN		  21
#define HOTKNOT_SEND_LEN				 136
#define HOTKNOT_RECEIVE_LEN			  4
#define HOTKNOT_MAX_DATA_LEN			 128
#define HOTKNOT_GETQUEUE_LEN			 4


#define KEYINDEX_LEN					 4
#define QUERYVERSION_LEN				 4
#define CIPHER_LEN					   16
#define DEMO_PD_PACKET_RET_LEN		   43
#define MAX_PD_PACKET_RET_LEN			100
#define DEMO_HOTKNOT_SEND_RET_LEN		150
#define DEMO_HOTKNOT_RECEIVE_RET_LEN	 150
#define DEBUG_HOTKNOT_SEND_RET_LEN	   250
#define DEBUG_HOTKNOT_RECEIVE_RET_LEN	250


#define RESULT_OK						0
#define RESULT_FAIL					  1
#define RESULT_TIMEOUT				   2


#define HOTKNOT_BEFORE_TRANS_STATE	   0x91
#define HOTKNOT_TRANS_STATE			  0x92
#define HOTKNOT_AFTER_TRANS_STATE		0x93
#define HOTKNOT_NOT_TRANS_STATE		  0x94


extern void ReportHotKnotCmd(u8 *pPacket, u16 nLength);
extern long HotKnotIoctl(struct file *pFile, unsigned int nCmd, unsigned long nArg);


#endif
#endif
