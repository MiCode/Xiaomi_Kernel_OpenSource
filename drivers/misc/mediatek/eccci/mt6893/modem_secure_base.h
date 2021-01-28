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

size_t mt_secure_call(
		size_t arg0, size_t arg1, size_t arg2,
		size_t arg3, size_t r1, size_t r2, size_t r3);

#define mdreg_write32(reg_id, value)		\
	mt_secure_call(MD_DBGSYS_REG_DUMP, \
			reg_id, value, 0, 0, 0, 0)



#endif				/* __MODEM_SECURE_BASE_H__ */
