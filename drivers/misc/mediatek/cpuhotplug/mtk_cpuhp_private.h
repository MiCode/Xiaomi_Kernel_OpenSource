/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef _MTK_CPUHP_PRIVATE_H_
#define _MTK_CPUHP_PRIVATE_H_

#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>

/* CPU operations */
#define MTK_SIP_POWER_DOWN_CLUSTER \
	MTK_SIP_SMC_CMD(0X210)
#define MTK_SIP_POWER_UP_CLUSTER \
	MTK_SIP_SMC_CMD(0X211)
#define MTK_SIP_POWER_DOWN_CORE \
	MTK_SIP_SMC_CMD(0X212)
#define MTK_SIP_POWER_UP_CORE \
	MTK_SIP_SMC_CMD(0X213)
#define MTK_SIP_POWER_FLOW_DEBUG \
	MTK_SIP_SMC_CMD(0X214)


static inline void poweron_cluster(unsigned int cluster)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_POWER_UP_CLUSTER, cluster, 0, 0, 0, 0,
			0, 0, &res);
}

static inline void poweroff_cluster(unsigned int cluster)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_POWER_DOWN_CLUSTER, cluster, 0, 0, 0, 0,
			0, 0, &res);
}

static inline void poweroff_cpu(unsigned int cpu)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_POWER_DOWN_CORE, cpu, 0, 0, 0, 0, 0, 0, &res);
}

int cpuhp_platform_cpuon(int cluster, int cpu, int isalone, int action);
int cpuhp_platform_cpuoff(int cluster, int cpu, int isalone, int action);
int cpuhp_platform_init(void);

void ppm_notifier(void);

/* Both APIs are from MTK's CCF */
extern void armpll_control(int id, int on);
extern void mp_enter_suspend(int id, int suspend);

#endif
