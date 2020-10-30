// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/workqueue.h>
#include <linux/delay.h>

#include <lpm_plat_apmcu_mbox.h>
#if IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
#include "mcupm_driver.h"
#endif

struct mbox_ops {
	void (*write)(int id, int *buf, unsigned int len);
	void (*read)(int id, int *buf, unsigned int len);
};

static void apmcu_mcupm_mailbox_write(int id, int *buf, unsigned int len)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
	mcupm_mbox_write(APMCU_MCUPM_MBOX_ID, id, (void *)buf, len);
#endif
}

static void apmcu_mcupm_mailbox_read(int id, int *buf, unsigned int len)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_MCUPM_SUPPORT)
	mcupm_mbox_read(APMCU_MCUPM_MBOX_ID, id, (void *)buf, len);
#endif
}

static struct mbox_ops mbox[NF_MBOX] = {
	[MBOX_MCUPM] = {
		.write = apmcu_mcupm_mailbox_write,
		.read = apmcu_mcupm_mailbox_read
	},
};

bool lpm_mcupm_cm_is_notified(void)
{
	unsigned int en_mask = 0;

	mbox[MBOX_MCUPM].read(APMCU_MCUPM_MBOX_PWR_CTRL_EN, &en_mask, 1);

	return !!(en_mask & MCUPM_CM_CTRL);
}
EXPORT_SYMBOL(lpm_mcupm_cm_is_notified);


void lpm_set_mcupm_pll_mode(unsigned int mode)
{
	if (mode < NF_MCUPM_ARMPLL_MODE)
		mbox[MBOX_MCUPM].write(APMCU_MCUPM_MBOX_ARMPLL_MODE, &mode, 1);
}
EXPORT_SYMBOL(lpm_set_mcupm_pll_mode);

int lpm_get_mcupm_pll_mode(void)
{
	int mode = 0;

	mbox[MBOX_MCUPM].read(APMCU_MCUPM_MBOX_ARMPLL_MODE, &mode, 1);

	return mode;
}
EXPORT_SYMBOL(lpm_get_mcupm_pll_mode);

void lpm_set_mcupm_buck_mode(unsigned int mode)
{
	if (mode < NF_MCUPM_BUCK_MODE)
		mbox[MBOX_MCUPM].write(APMCU_MCUPM_MBOX_BUCK_MODE, &mode, 1);
}
EXPORT_SYMBOL(lpm_set_mcupm_buck_mode);

int lpm_get_mcupm_buck_mode(void)
{
	int mode = 0;

	mbox[MBOX_MCUPM].read(APMCU_MCUPM_MBOX_BUCK_MODE, &mode, 1);

	return mode;
}
EXPORT_SYMBOL(lpm_get_mcupm_buck_mode);

bool lpm_mcupm_is_ready(void)
{
	int sta = MCUPM_TASK_INIT_FINISH;

	mbox[MBOX_MCUPM].read(APMCU_MCUPM_MBOX_TASK_STA, &sta, 1);

	return sta == MCUPM_TASK_WAIT || sta == MCUPM_TASK_INIT_FINISH;
}
