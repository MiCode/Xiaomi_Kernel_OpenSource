/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef _MTK_CPUHP_PRIVATE_H_
#define _MTK_CPUHP_PRIVATE_H_

#include <mt-plat/mtk_secure_api.h>

#define poweron_cluster(i)  mt_secure_call(MTK_SIP_POWER_UP_CLUSTER, i, 0, 0, 0)
#define poweroff_cluster(i) mt_secure_call(MTK_SIP_POWER_DOWN_CLUSTER, \
					   i, 0, 0, 0)
#define poweroff_cpu(i)     mt_secure_call(MTK_SIP_POWER_DOWN_CORE, i, 0, 0, 0)

int cpuhp_platform_cpuon(int cluster, int cpu, int isalone, int action);
int cpuhp_platform_cpuoff(int cluster, int cpu, int isalone, int action);
int cpuhp_platform_init(void);

void ppm_notifier(void);

/* Both APIs are from MTK's CCF */
extern void armpll_control(int id, int on);
extern void mp_enter_suspend(int id, int suspend);

#endif
