/*
 * Copyright (C) 2017 Spreadtrum Communications Inc.
 *
 * Filename : gnss.h
 * Abstract : This file is a implementation for driver of gnss:
 *                 GNSS chip were integrated with AP chipset.
 *
 * Authors	: zhaohui.chen
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __GNSS_H__
#define __GNSS_H__
#include <linux/regmap.h>
#include "gnss_common.h"

#define FALSE								(0)
#define TRUE								(1)

/* begin: ddr share info */
#define GNSS_CACHE_FLAG_ADDR		(0x0014F000)
#define GNSS_CACHE_FLAG_VALUE		(0xCDCDDCDC)
#define GNSS_CACHE_END_VALUE		(0xEFEFFEFE)

/* gnss status:
 * 1.rf Cali;
 * 2.Init;
 * 3.Init_done;
 * 4.Idleoff;
 * 5.Idle on;
 * 6.sleep
 */
#define GNSS_STATUS_OFFSET		   (0x0014F004)
#define GNSS_STATUS_SIZE		   (4)

#define GNSS_REC_AON_CHIPID_OFFSET (0x00150000) /* sharkle or pike2 */
#define GNSS_REC_AON_CHIPID_SIZE (8)

#define GNSS_EFUSE_DATA_OFFSET (0x00150008)
#define GNSS_EFUSE_DATA_SIZE  12
/* end: ddr share info */

/* Begin: AP GNSS register */
/* AON CLK reg, not find other defination, just define it here */
#define AON_CLK_CORE   0x402d0200
#define CGM_WCN_SHARKLE_CFG    0xd4
#define CGM_WCN_PIKE2_CFG    0xd8
/* End: AP regsiter */

void gnss_file_path_set(char *buf);
#endif
