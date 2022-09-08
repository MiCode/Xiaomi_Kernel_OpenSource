// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/kconfig.h>
#include <linux/sched/clock.h>

#include <mtk_mcdi.h>
#include <mtk_mcdi_util.h>
#include <mtk_mcdi_plat.h>
#include <mtk_mcdi_reg.h>
#include <mtk_mcdi_mcupm.h>

/* #include <mt-plat/mtk_secure_api.h> */

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
#include <sspm_mbox.h>
#endif

/**
 * MCU read/write interface
 */
static inline unsigned int mcdi_sspm_read(int id)
{
	unsigned int val = 0;

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	sspm_mbox_read(MCDI_MBOX, id, &val, 1);
#endif

	return val;
}

static inline void mcdi_sspm_write(int id, unsigned int val)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT)
	sspm_mbox_write(MCDI_MBOX, id, (void *)&val, 1);
#endif
}

static inline int mcdi_sspm_ready(void)
{
	return IS_ENABLED(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) ? true : false;
}

static inline unsigned int mcdi_mcupm_read(int id)
{
	return mcdi_read((uintptr_t)id);

	return 0;

}

static inline void mcdi_mcupm_write(int id, unsigned int val)
{
	mcdi_write((uintptr_t)id, val);
}

static inline int mcdi_mcupm_ready(void)
{
	return false;
}

#if defined(MCDI_SSPM_INTF)

#define __mcdi_mbox_read(id)           mcdi_sspm_read(id)
#define __mcdi_mbox_write(id, val)     mcdi_sspm_write(id, val)
#define __mcdi_fw_is_ready()           mcdi_sspm_ready()

#elif defined(MCDI_MCUPM_INTF)

#define __mcdi_mbox_read(id)           mcdi_mcupm_read(id)
#define __mcdi_mbox_write(id, val)     mcdi_mcupm_write(id, val)
#define __mcdi_fw_is_ready()           mcdi_mcupm_ready()

#else

#define __mcdi_mbox_read(id)           0
#define __mcdi_mbox_write(id, val)
#define __mcdi_fw_is_ready()           0

#endif

unsigned int mcdi_mbox_read(int id)
{
	return __mcdi_mbox_read(id);
}

void mcdi_mbox_write(int id, unsigned int val)
{
	__mcdi_mbox_write(id, val);
}

int mcdi_fw_is_ready(void)
{
	return __mcdi_fw_is_ready();
}

/**
 * SPMC related interface
 */
bool mcdi_is_cpc_mode(void)
{
#if defined(MCDI_CPC_MODE)
	return true;
#else
	return false;
#endif
}

#if defined(MCDI_CPC_MODE)
unsigned int mcdi_get_raw_pwr_sta(void)
{
	return mcdi_read(CPC_SPMC_PWR_STATUS);
}

void mcdi_notify_cluster_off(unsigned int cluster)
{
}

unsigned int mcdi_get_cluster_off_cnt(unsigned int cluster)
{
	unsigned int cnt = mcdi_read(CPC_DORMANT_COUNTER);

	cnt = ((cnt >> 16) & 0xFFFF) + (cnt & 0xFFFF);
	cnt += mcdi_read(SYSRAM_CPC_CLUSTER_CNT);

	return cnt;
}

#else
unsigned int mcdi_get_raw_pwr_sta(void)
{
	return mcdi_mbox_read(MCDI_MBOX_CPU_CLUSTER_PWR_STAT);
}

void mcdi_notify_cluster_off(unsigned int cluster)
{
	mcdi_mbox_write(MCDI_MBOX_CLUSTER_0_CAN_POWER_OFF + cluster, 1);
}

unsigned int mcdi_get_cluster_off_cnt(unsigned int cluster)
{
	return mcdi_mbox_read(MCDI_MBOX_CLUSTER_0_CNT + cluster);
}

#endif

void mcdi_set_cpu_iso_smc(unsigned int iso_mask)
{
	iso_mask &= 0xff;

	/*
	 * If isolation bit of ALL CPU are set, means iso_mask is not reasonable
	 * Do NOT update iso_mask to mcdi controller
	 */
	if (iso_mask == 0xff)
		return;

	mt_secure_call(MTK_SIP_KERNEL_MCDI_ARGS,
			MCDI_SMC_EVENT_GIC_DPG_SET,
			iso_mask,
			0, 0);
}

void mcdi_set_cpu_iso_mbox(unsigned int iso_mask)
{
	iso_mask &= 0xff;

	/*
	 * If isolation bit of ALL CPU are set, means iso_mask is not reasonable
	 * Do NOT update iso_mask to mcdi controller
	 */
	if (iso_mask == 0xff)
		return;

	mcdi_mbox_write(MCDI_MBOX_CPU_ISOLATION_MASK, iso_mask);
}

/**
 * MCDI utility function
 */
unsigned long long idle_get_current_time_us(void)
{
	unsigned long long idle_current_time = sched_clock();

	if (idle_current_time > 0)
		do_div(idle_current_time, 1000);
	return idle_current_time;
}

