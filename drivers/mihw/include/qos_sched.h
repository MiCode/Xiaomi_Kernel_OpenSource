/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024-2024 XRing Technologies Co., Ltd.
 */
#ifndef __QIS_QOS_SCHED_H__
#define __QIS_QOS_SCHED_H__

struct qs_struct {
	u32 rt_sched : 1;
	u32 rt_sched_reason : 2;
	u32 vip_sched : 1;
	u32 vip_sched_reason : 2;
	u32 policy_backup : 8;
	u32 speedlock_flag : 1;
	u32 rtg_set_flag : 1;
	u32 reserved : 16;
#if IS_ENABLED(CONFIG_XRING_QS_DEBUG)
	unsigned long timeout;
#endif
};

#endif
