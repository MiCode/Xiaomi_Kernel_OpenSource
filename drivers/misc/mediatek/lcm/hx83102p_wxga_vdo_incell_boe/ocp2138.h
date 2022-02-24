/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _ocp2138_SW_H_
#define _ocp2138_SW_H_

#ifndef BUILD_LK
struct ocp2138_setting_table {
	unsigned char cmd;
	unsigned char data;
};

extern int ocp2138_read_byte(unsigned char cmd, unsigned char *returnData);
extern int ocp2138_write_byte(unsigned char cmd, unsigned char writeData);
#endif

#endif
