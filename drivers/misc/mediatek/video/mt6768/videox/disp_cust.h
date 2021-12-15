/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#ifndef _DISP_CUST_H_
#define _DISP_CUST_H_

extern void set_lcm(struct LCM_setting_table_V3 *para_tbl,
			unsigned int size, bool hs);
extern int read_lcm(unsigned char cmd, unsigned char *buf,
		unsigned char buf_size, bool sendhs);

#endif
