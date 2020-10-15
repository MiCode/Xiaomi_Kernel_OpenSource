// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/vmalloc.h>		 /* needed by vmalloc */
#include <linux/sysfs.h>
#include <linux/device.h>	   /* needed by device_* */
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/sched_clock.h>
#include <linux/ratelimit.h>
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_feature_define.h"
#include "scp_l1c.h"
#include <mt-plat/aee.h>

enum scp_l1c_status_t scp_l1c_flua(uint32_t L1C_type)
{
	L1C_SEL(L1C_type)->L1C_OP &= ~L1C_OP_OP_MASK;
	L1C_SEL(L1C_type)->L1C_OP |=
		((L1C_FLUA << L1C_OP_OP_OFFSET) |
		L1C_OP_EN_MASK);
	return SCP_L1C_STATUS_OK;
}

