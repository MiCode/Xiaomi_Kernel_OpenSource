// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched/clock.h>

#include "apu_ctrl_rpmsg.h"

static u64 timesync_stamp;
static void mdw_timesync_callback(u32 id, void *priv, void *data, u32 len)
{
	timesync_stamp = sched_clock();
	apu_ctrl_send_msg(TRACE_TIMESYNC_SYN,
			&timesync_stamp, sizeof(u64), 0);
	pr_info("%s %d\n", __func__, __LINE__);
}

int apu_timesync_init(void)
{
	apu_ctrl_register_channel(TRACE_TIMESYNC_SYN, mdw_timesync_callback,
			NULL, &timesync_stamp, sizeof(timesync_stamp));
	pr_info("%s %d\n", __func__, __LINE__);
	return 0;
}

int apu_timesync_remove(void)
{
	apu_ctrl_unregister_channel(TRACE_TIMESYNC_SYN);
	pr_info("%s %d\n", __func__, __LINE__);
	return 0;
}
