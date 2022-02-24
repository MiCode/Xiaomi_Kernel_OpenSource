/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _sgm37604a_SW_H_
#define _sgm37604a_SW_H_

#ifndef BUILD_LK
struct sgm37604a_setting_table {
	unsigned char cmd;
	unsigned char data;
};

extern int sgm37604a_read_byte(unsigned char cmd, unsigned char *returnData);
extern int sgm37604a_write_byte(unsigned char cmd, unsigned char writeData);
#endif

#endif
