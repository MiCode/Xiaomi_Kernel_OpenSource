/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _s5k3m3MIPI_PDAF_H_
#define _s5k3m3MIPI_PDAF_H_

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
			u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);

extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iMultiReadReg(u16 a_u2Addr, u8 *a_puBuff, u16 i2cId, u8 number);

#endif
