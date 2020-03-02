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

#include <mt-plat/mtk_secure_api.h>

enum CCCI_SECURE_REQ_ID {
	MD_DBGSYS_REG_DUMP = 0,
	MD_BANK0_HW_REMAP,
	MD_BANK1_HW_REMAP,
	MD_BANK4_HW_REMAP,
	MD_SIB_HW_REMAP,
};

#define mdreg_write32(reg_id, value)		\
	mt_secure_call(MTK_SIP_CCCI_CONTROL, \
					MD_DBGSYS_REG_DUMP, reg_id, value, 0)

#endif				/* __MODEM_SECURE_BASE_H__ */
