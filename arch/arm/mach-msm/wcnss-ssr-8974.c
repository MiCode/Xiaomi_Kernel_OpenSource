/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/err.h>

#include <mach/subsystem_restart.h>
#include <mach/msm_smsm.h>

#define MODULE_NAME			"wcnss_8974"
#define MAX_SSR_REASON_LEN			0x51

static int ss_restart_inprogress;
static int wcnss_crash;
static struct subsys_device *wcnss_ssr_dev;

#define WCNSS_APSS_WDOG_BITE_RESET_RDY_IRQ		181

static void log_wcnss_sfr(void)
{
	char *smem_reset_reason;
	char buffer[MAX_SSR_REASON_LEN];
	unsigned smem_reset_size;
	unsigned size;

	smem_reset_reason = smem_get_entry(SMEM_SSR_REASON_WCNSS0,
			&smem_reset_size);

	if (!smem_reset_reason || !smem_reset_size) {
		pr_err("%s: wcnss subsystem failure reason: %s\n",
				__func__, "(unknown, smem_get_entry failed)");
	} else if (!smem_reset_reason[0]) {
		pr_err("%s: wcnss subsystem failure reason: %s\n",
				__func__, "(unknown, init string found)");
	} else {
		size = smem_reset_size < MAX_SSR_REASON_LEN ? smem_reset_size :
			(MAX_SSR_REASON_LEN - 1);
		memcpy(buffer, smem_reset_reason, size);
		buffer[size] = '\0';
		pr_err("%s: wcnss subsystem failure reason: %s\n",
				__func__, buffer);
		memset(smem_reset_reason, 0, smem_reset_size);
		wmb();
	}
}

static void restart_wcnss(void)
{
	log_wcnss_sfr();
	subsystem_restart("wcnss");
}

static void smsm_state_cb_hdlr(void *data, uint32_t old_state,
					uint32_t new_state)
{
	wcnss_crash = true;

	pr_err("%s: smsm state changed\n", MODULE_NAME);

	if (!(new_state & SMSM_RESET))
		return;

	if (ss_restart_inprogress) {
		pr_err("%s: Ignoring smsm reset req, restart in progress\n",
						MODULE_NAME);
		return;
	}

	ss_restart_inprogress = true;
	restart_wcnss();
}


static irqreturn_t wcnss_wdog_bite_irq_hdlr(int irq, void *dev_id)
{
	wcnss_crash = true;

	if (ss_restart_inprogress) {
		pr_err("%s: Ignoring wcnss bite irq, restart in progress\n",
						MODULE_NAME);
		return IRQ_HANDLED;
	}

	ss_restart_inprogress = true;
	restart_wcnss();

	return IRQ_HANDLED;
}


static int wcnss_shutdown(const struct subsys_desc *subsys)
{
	return 0;
}

static int wcnss_powerup(const struct subsys_desc *subsys)
{
	return 0;
}

/* wcnss crash handler */
static void wcnss_crash_shutdown(const struct subsys_desc *subsys)
{
	pr_err("%s: crash shutdown : %d\n", MODULE_NAME, wcnss_crash);
	if (wcnss_crash != true)
		smsm_change_state(SMSM_APPS_STATE, SMSM_RESET, SMSM_RESET);
}

static int wcnss_ramdump(int enable,
				const struct subsys_desc *crashed_subsys)
{
	return 0;
}

static struct subsys_desc wcnss_ssr = {
	.name = "wcnss",
	.shutdown = wcnss_shutdown,
	.powerup = wcnss_powerup,
	.ramdump = wcnss_ramdump,
	.crash_shutdown = wcnss_crash_shutdown
};

static int __init wcnss_ssr_init(void)
{
	int ret;

	ret = smsm_state_cb_register(SMSM_WCNSS_STATE, SMSM_RESET,
					smsm_state_cb_hdlr, 0);
	if (ret < 0) {
		pr_err("%s: Unable to register smsm callback for wcnss Reset! %d\n",
				MODULE_NAME, ret);
		goto out;
	}
	ret = request_irq(WCNSS_APSS_WDOG_BITE_RESET_RDY_IRQ,
			wcnss_wdog_bite_irq_hdlr, IRQF_TRIGGER_HIGH,
				"wcnss_wdog", NULL);

	if (ret < 0) {
		pr_err("%s: Unable to register for wcnss bite interrupt (%d)\n",
				MODULE_NAME, ret);
		goto out;
	}
	wcnss_ssr_dev = subsys_register(&wcnss_ssr);
	if (IS_ERR(wcnss_ssr_dev))
		return PTR_ERR(wcnss_ssr_dev);

	pr_info("%s: module initialized\n", MODULE_NAME);
out:
	return ret;
}

arch_initcall(wcnss_ssr_init);
