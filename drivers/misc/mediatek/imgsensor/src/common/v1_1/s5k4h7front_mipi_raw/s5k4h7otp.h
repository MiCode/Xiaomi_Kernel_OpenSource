/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __S5K4H7_OTP_H__
#define __S5K4H7_OTP_H__

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData,
		       u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);

bool S5K4H7_otp_update(void);
unsigned char s5k4h7_get_module_id(void);
unsigned int s5k4h7_read_otpdata(struct i2c_client *client, unsigned int addr, unsigned char *data, unsigned int size);

#endif
