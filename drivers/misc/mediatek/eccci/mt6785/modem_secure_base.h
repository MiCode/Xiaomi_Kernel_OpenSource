/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __MODEM_SECURE_BASE_H__
#define __MODEM_SECURE_BASE_H__
/* mark for fix build error */
//#include <mt-plat/mtk_secure_api.h>

enum CCCI_SECURE_REQ_ID {
	MD_DBGSYS_REG_DUMP = 0,
	MD_BANK0_HW_REMAP,
	MD_BANK1_HW_REMAP,
	MD_BANK4_HW_REMAP,
	MD_SIB_HW_REMAP,
	MD_CLOCK_REQUEST,
	MD_POWER_CONFIG,
	MD_FLIGHT_MODE_SET,
};


enum MD_CLOCK_REG_ID {
	MD_REG_AP_MDSRC_REQ = 0,
	MD_REG_AP_MDSRC_ACK,
	MD_REG_AP_MDSRC_SETTLE,
};

enum MD_POWER_CONFIG_ID {
	MD_KERNEL_BOOT_UP,
	MD_LK_BOOT_UP,
	MD_CHECK_FLAG,
	MD_CHECK_DONE,
	MD_BOOT_STATUS,
};

#endif				/* __MODEM_SECURE_BASE_H__ */
