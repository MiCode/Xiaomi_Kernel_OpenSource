/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _S5K3P8SP_PDAF_H
#define _S5K3P8SP_PDAF_H

extern int iReadRegI2C(u8 *a_pSendData,
			    u16 a_sizeSendData,
			    u8 *a_pRecvData,
			    u16 a_sizeRecvData,
			    u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);

#endif
