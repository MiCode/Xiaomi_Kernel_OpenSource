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

static int crash_shutdown;
static struct subsys_device *modem_ssr_dev;

#define MAX_SSR_REASON_LEN 81U
#define Q6SS_WDOG_ENABLE		0xFC802004
#define MSS_Q6SS_WDOG_EXP_IRQ		56

static void log_modem_sfr(void)
{
	u32 size;
	char *smem_reason, reason[MAX_SSR_REASON_LEN];

	smem_reason = smem_get_entry(SMEM_SSR_REASON_MSS0, &size);
	if (!smem_reason || !size) {
		pr_err("modem subsystem failure reason: (unknown, smem_get_entry failed).\n");
		return;
	}
	if (!smem_reason[0]) {
		pr_err("modem subsystem failure reason: (unknown, empty string found).\n");
		return;
	}

	strlcpy(reason, smem_reason, min(size, sizeof(reason)));
	pr_err("modem subsystem failure reason: %s.\n", reason);

	smem_reason[0] = '\0';
	wmb();
}

static void restart_modem(void)
{
	log_modem_sfr();
	subsystem_restart("modem");
}

static void smsm_state_cb(void *data, uint32_t old_state, uint32_t new_state)
{
	/* Ignore if we're the one that set SMSM_RESET */
	if (crash_shutdown)
		return;

	if (new_state & SMSM_RESET) {
		pr_err("Probable fatal error on the modem.\n");
		restart_modem();
	}
}

static int modem_shutdown(const struct subsys_desc *subsys)
{
	return 0;
}

static int modem_powerup(const struct subsys_desc *subsys)
{
	return 0;
}

void modem_crash_shutdown(const struct subsys_desc *subsys)
{
	crash_shutdown = 1;
	smsm_reset_modem(SMSM_RESET);
}

static int modem_ramdump(int enable,
				const struct subsys_desc *crashed_subsys)
{
	return 0;
}

static irqreturn_t modem_wdog_bite_irq(int irq, void *dev_id)
{
	pr_err("Watchdog bite received from modem software!\n");
	restart_modem();
	return IRQ_HANDLED;
}

static struct subsys_desc modem_8974 = {
	.name = "modem",
	.shutdown = modem_shutdown,
	.powerup = modem_powerup,
	.ramdump = modem_ramdump,
	.crash_shutdown = modem_crash_shutdown
};

static int __init modem_8974_init(void)
{
	int ret;

	ret = smsm_state_cb_register(SMSM_MODEM_STATE, SMSM_RESET,
		smsm_state_cb, 0);

	if (ret < 0) {
		pr_err("%s: Unable to register SMSM callback! (%d)\n",
				__func__, ret);
		goto out;
	}

	ret = request_irq(MSS_Q6SS_WDOG_EXP_IRQ, modem_wdog_bite_irq,
			IRQF_TRIGGER_RISING, "modem_wdog_sw", NULL);

	if (ret < 0) {
		pr_err("%s: Unable to request q6sw watchdog IRQ. (%d)\n",
				__func__, ret);
		goto out;
	}

	modem_ssr_dev = subsys_register(&modem_8974);

	if (IS_ERR_OR_NULL(modem_ssr_dev)) {
		pr_err("%s: Unable to reg with subsystem restart. (%ld)\n",
				__func__, PTR_ERR(modem_ssr_dev));
		ret = PTR_ERR(modem_ssr_dev);
		goto out;
	}

	pr_info("%s: modem subsystem restart driver init'ed.\n", __func__);
out:
	return ret;
}

arch_initcall(modem_8974_init);
