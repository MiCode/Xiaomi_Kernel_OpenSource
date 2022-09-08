// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>	   /* needed by device_* */
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/ratelimit.h>
#include <linux/sched_clock.h>
#include <linux/sysfs.h>
#include <linux/vmalloc.h>		 /* needed by vmalloc */
#include <linux/workqueue.h>
#include "scp_ipi.h"
#include "scp_helper.h"
#include "scp_excep.h"
#include "scp_feature_define.h"
#include "scp_l1c.h"
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif

enum scp_l1c_status_t scp_l1c_flua(uint32_t L1C_type)
{
	L1C_SEL(L1C_type)->L1C_OP &= ~L1C_OP_OP_MASK;
	L1C_SEL(L1C_type)->L1C_OP |=
		((L1C_FLUA << L1C_OP_OP_OFFSET) |
		L1C_OP_EN_MASK);
	return SCP_L1C_STATUS_OK;
}

