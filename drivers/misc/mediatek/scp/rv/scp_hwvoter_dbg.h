/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SCP_HW_VOTER_DBG_H__
#define __SCP_HW_VOTER_DBG_H__

enum {
	HW_VOTER_DBG_CMD_TEST,
};

enum {
	HW_VOTER_TEST_TYPE_PD,
	HW_VOTER_TEST_TYPE_PLL,
	HW_VOTER_TEST_TYPE_MUX,
	HW_VOTER_TEST_TYPE_CG,
};

enum {
	HW_VOTER_TEST_OP_OFF,
	HW_VOTER_TEST_OP_ON,
	HW_VOTER_TEST_OP_SET_RATE,
};

extern int scp_hw_voter_dbg_init(void);

#endif  /* __SCP_HW_VOTER_DBG_H__ */
