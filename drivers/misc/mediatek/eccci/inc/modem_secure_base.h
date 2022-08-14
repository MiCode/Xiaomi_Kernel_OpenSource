/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __MODEM_SECURE_BASE_H__
#define __MODEM_SECURE_BASE_H__

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

enum CCCI_SECURE_REQ_ID {
	MD_DBGSYS_REG_DUMP = 0,
	MD_BANK0_HW_REMAP,
	MD_BANK1_HW_REMAP,
	MD_BANK4_HW_REMAP,
	MD_SIB_HW_REMAP,
	MD_CLOCK_REQUEST,
	MD_POWER_CONFIG,
	MD_FLIGHT_MODE_SET,
	MD_HW_REMAP_LOCKED, /* 8 */
	MD_DEBUG_DUMP,
	SCP_INFO_TO_SAVE = 12, /* save scp smem addr in tfa*/
	SCP_CLK_SET_DONE,
};


enum MD_CLOCK_REG_ID {
	MD_REG_AP_MDSRC_REQ = 0,
	MD_REG_AP_MDSRC_ACK,
	MD_REG_AP_MDSRC_SETTLE,
	MD_WAKEUP_AP_SRC,
	MD_GET_SLEEP_MODE,
};

enum MD_POWER_CONFIG_ID {
	MD_KERNEL_BOOT_UP,
	MD_LK_BOOT_UP,
	MD_CHECK_FLAG,
	MD_CHECK_DONE,
	MD_BOOT_STATUS,
	MD_LK_BOOT_PLAT = 8,
};

#endif				/* __MODEM_SECURE_BASE_H__ */
