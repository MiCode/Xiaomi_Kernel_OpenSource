/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __GPUEB_HW_VOTER_DBG_H__
#define __GPUEB_HW_VOTER_DBG_H__

enum {
	HW_VOTER_DBG_CMD_TEST,
};

struct hwvoter_ipi_test_t {
	unsigned char cmd;
	unsigned char type;
	unsigned char op;
	unsigned char clk_category;
	unsigned short clk_id;
	unsigned short val;
	unsigned int dummy[2];
};

extern int gpueb_hw_voter_dbg_init(void);

#endif  /* __GPUEB_HW_VOTER_DBG_H__ */
