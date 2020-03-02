/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __AFSTMV__
#define __AFSTMV__

#define INI_MSSET_211 ((unsigned char)0x00)
#define CHTGX_THRESHOLD ((unsigned short)0x0200)
#define CHTGOKN_TIME ((unsigned char)0x80)
#define CHTGOKN_WAIT 3

#define STMV_SIZE ((unsigned short)0x0180)
#define STMV_INTERVAL ((unsigned char)0x01)

#define STMCHTG_ON ((unsigned char)0x08)
#define STMSV_ON ((unsigned char)0x04)
#define STMLFF_ON ((unsigned char)0x02)
#define STMVEN_ON ((unsigned char)0x01)
#define STMCHTG_OFF ((unsigned char)0x00)
#define STMSV_OFF ((unsigned char)0x00)
#define STMLFF_OFF ((unsigned char)0x00)
#define STMVEN_OFF ((unsigned char)0x00)

#define STMCHTG_SET STMCHTG_ON
#define STMSV_SET STMSV_ON
#define STMLFF_SET STMLFF_OFF

struct stSmvPar {
	unsigned short UsSmvSiz;
	unsigned char UcSmvItv;
	unsigned char UcSmvEnb;
};

/* Step Move Parameter Setting Function */
extern void StmvSet(struct stSmvPar D1);

/* Step Move to Target Position Function */
extern unsigned char StmvTo(short D2);

extern void RamWriteA(unsigned short addr, unsigned short data);

extern void RamReadA(unsigned short addr, unsigned short *data);

extern void RegWriteA(unsigned short addr, unsigned char data);

extern void RegReadA(unsigned short addr, unsigned char *data);

extern void WaitTime(unsigned short msec);

extern int s4AF_WriteReg_LC898212XDAF(unsigned char *a_pSendData,
				      unsigned short a_sizeSendData,
				      unsigned short i2cId);

extern int s4AF_ReadReg_LC898212XDAF(unsigned char *a_pSendData,
				     unsigned short a_sizeSendData,
				     unsigned char *a_pRecvData,
				     unsigned short a_sizeRecvData,
				     unsigned short i2cId);

#endif /* __AFSTMV__ */
