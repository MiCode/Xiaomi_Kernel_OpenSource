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
