/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef _MTK_CPU_POWER_THROTTLING_H_
#define _MTK_CPU_POWER_THROTTLING_H_

enum cpu_pt_type {
	LBAT_POWER_THROTTLING,
	OC_POWER_THROTTLING,
	POWER_THROTTLING_TYPE_MAX
};

struct cpu_pt_policy {
	enum cpu_pt_type           pt_type;
	unsigned int               cpu;
	s32                        cpu_limit;
	struct freq_qos_request    qos_req;
	struct cpufreq_policy      *policy;
	struct list_head           cpu_pt_list;
};

#endif
