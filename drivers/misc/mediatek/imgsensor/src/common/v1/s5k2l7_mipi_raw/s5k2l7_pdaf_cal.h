/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *     s5k2l7_pdaf_cal.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor setting file
 *
 * Setting Release Date:
 * ------------
 *     2016.09.01
 *
 ****************************************************************************/
#ifndef _s5k2l7_PDAF_CAL_H_
#define _s5k2l7_PDAF_CAL_H_

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
			    u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);

#endif
