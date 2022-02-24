// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/workqueue.h>
#include <linux/delay.h>

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <sspm_helper.h>
#include <sspm_mbox.h>
#endif

#include <mtk_lp_plat_apmcu_mbox.h>
#ifdef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
#include "mcupm_driver.h"
#endif

struct mbox_ops {
	void (*write)(int id, int *buf, unsigned int len);
	void (*read)(int id, int *buf, unsigned int len);
};

static void apmcu_sspm_mailbox_write(int id, int *buf, unsigned int len)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
//	if (is_sspm_ready())
//		sspm_mbox_write(APMCU_SSPM_MBOX_ID, id, (void *)buf, len);
#endif
}

static void apmcu_sspm_mailbox_read(int id, int *buf, unsigned int len)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
//	if (is_sspm_ready())
//		sspm_mbox_read(APMCU_SSPM_MBOX_ID, id, (void *)&buf, len);
#endif
}

static void apmcu_mcupm_mailbox_write(int id, int *buf, unsigned int len)
{
#ifdef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
	mcupm_mbox_write(APMCU_MCUPM_MBOX_ID, id, (void *)buf, len);
#endif
}

static void apmcu_mcupm_mailbox_read(int id, int *buf, unsigned int len)
{
#ifdef CONFIG_MTK_TINYSYS_MCUPM_SUPPORT
	mcupm_mbox_read(APMCU_MCUPM_MBOX_ID, id, (void *)buf, len);
#endif
}

static struct mbox_ops mbox[NF_MBOX] = {
	[MBOX_SSPM] = {
		.write = apmcu_sspm_mailbox_write,
		.read = apmcu_sspm_mailbox_read
	},
	[MBOX_MCUPM] = {
		.write = apmcu_mcupm_mailbox_write,
		.read = apmcu_mcupm_mailbox_read
	},
};


void mtk_set_sspm_lp_cmd(void *buf)
{
	mbox[MBOX_SSPM].write(APMCU_SSPM_MBOX_SPM_CMD,
			(int *)buf,
			APMCU_SSPM_MBOX_SPM_CMD_SIZE);
}

void mtk_clr_sspm_lp_cmd(void)
{
	int buf[APMCU_SSPM_MBOX_SPM_CMD_SIZE] = {0};

	mbox[MBOX_SSPM].write(APMCU_SSPM_MBOX_SPM_CMD,
			(int *)buf,
			APMCU_SSPM_MBOX_SPM_CMD_SIZE);
}

static void mtk_mcupm_pwr_ctrl_setting(int dev)
{
	mbox[MBOX_MCUPM].write(APMCU_MCUPM_MBOX_PWR_CTRL_EN, &dev, 1);
}

bool mtk_mcupm_cm_is_notified(void)
{
	unsigned int en_mask = 0;

	mbox[MBOX_MCUPM].read(APMCU_MCUPM_MBOX_PWR_CTRL_EN, &en_mask, 1);

	return !!(en_mask & MCUPM_CM_CTRL);
}
EXPORT_SYMBOL(mtk_mcupm_cm_is_notified);


void mtk_set_mcupm_pll_mode(unsigned int mode)
{
	if (mode < NF_MCUPM_ARMPLL_MODE)
		mbox[MBOX_MCUPM].write(APMCU_MCUPM_MBOX_ARMPLL_MODE, &mode, 1);
}
EXPORT_SYMBOL(mtk_set_mcupm_pll_mode);

int mtk_get_mcupm_pll_mode(void)
{
	int mode = 0;

	mbox[MBOX_MCUPM].read(APMCU_MCUPM_MBOX_ARMPLL_MODE, &mode, 1);

	return mode;
}
EXPORT_SYMBOL(mtk_get_mcupm_pll_mode);

void mtk_set_mcupm_buck_mode(unsigned int mode)
{
	if (mode < NF_MCUPM_BUCK_MODE)
		mbox[MBOX_MCUPM].write(APMCU_MCUPM_MBOX_BUCK_MODE, &mode, 1);
}
EXPORT_SYMBOL(mtk_set_mcupm_buck_mode);

int mtk_get_mcupm_buck_mode(void)
{
	int mode = 0;

	mbox[MBOX_MCUPM].read(APMCU_MCUPM_MBOX_BUCK_MODE, &mode, 1);

	return mode;
}
EXPORT_SYMBOL(mtk_get_mcupm_buck_mode);

void mtk_set_preferred_cpu_wakeup(int cpu)
{
	if (cpu_online(cpu))
		mbox[MBOX_MCUPM].write(APMCU_MCUPM_MBOX_WAKEUP_CPU, &cpu, 1);
}

int mtk_get_preferred_cpu_wakeup(void)
{
	int cpu = -1;

	mbox[MBOX_MCUPM].read(APMCU_MCUPM_MBOX_WAKEUP_CPU, &cpu, 1);

	return cpu;
}

bool mtk_mcupm_is_ready(void)
{
	int sta = MCUPM_TASK_INIT_FINISH;

	mbox[MBOX_MCUPM].read(APMCU_MCUPM_MBOX_TASK_STA, &sta, 1);

	return sta == MCUPM_TASK_WAIT || sta == MCUPM_TASK_INIT_FINISH;
}

void mtk_wait_mbox_init_done(void)
{
	int sta = MCUPM_TASK_INIT;

	while (1) {
		mbox[MBOX_MCUPM].read(APMCU_MCUPM_MBOX_TASK_STA, &sta, 1);

		if (sta == MCUPM_TASK_INIT)
			break;

		msleep(1000);
	}

	mtk_set_mcupm_pll_mode(MCUPM_ARMPLL_OFF);
	mtk_set_mcupm_buck_mode(MCUPM_BUCK_OFF_MODE);

	mtk_mcupm_pwr_ctrl_setting(
			 MCUPM_MCUSYS_CTRL |
#ifdef CONFIG_MTK_CM_MGR_LEGACY
			 MCUPM_CM_CTRL |
#endif
			 MCUPM_BUCK_CTRL |
			 MCUPM_ARMPLL_CTRL);
}

void mtk_notify_subsys_ap_ready(void)
{
	int ready = 1;

	mbox[MBOX_SSPM].write(APMCU_SSPM_MBOX_AP_READY, &ready, 1);
	mbox[MBOX_MCUPM].write(APMCU_MCUPM_MBOX_AP_READY, &ready, 1);
}

