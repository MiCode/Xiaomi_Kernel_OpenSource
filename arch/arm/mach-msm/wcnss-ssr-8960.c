/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/wcnss_wlan.h>
#include <mach/irqs.h>
#include <mach/scm.h>
#include <mach/subsystem_restart.h>
#include <mach/subsystem_notif.h>
#include <mach/peripheral-loader.h>
#include "smd_private.h"
#include "ramdump.h"

#define MODULE_NAME			"wcnss_8960"
#define MAX_BUF_SIZE			0x51



static struct delayed_work cancel_vote_work;
static void *riva_ramdump_dev;
static int riva_crash;
static int ss_restart_inprogress;
static int enable_riva_ssr;

static void smsm_state_cb_hdlr(void *data, uint32_t old_state,
					uint32_t new_state)
{
	char *smem_reset_reason;
	char buffer[MAX_BUF_SIZE];
	unsigned smem_reset_size;
	unsigned size;

	riva_crash = true;

	pr_err("%s: smsm state changed\n", MODULE_NAME);

	if (!(new_state & SMSM_RESET))
		return;

	if (ss_restart_inprogress) {
		pr_err("%s: Ignoring smsm reset req, restart in progress\n",
						MODULE_NAME);
		return;
	}

	if (!enable_riva_ssr)
		panic(MODULE_NAME ": SMSM reset request received from Riva");

	smem_reset_reason = smem_get_entry(SMEM_SSR_REASON_WCNSS0,
			&smem_reset_size);

	if (!smem_reset_reason || !smem_reset_size) {
		pr_err("%s: wcnss subsystem failure reason: %s\n",
				__func__, "(unknown, smem_get_entry failed)");
	} else if (!smem_reset_reason[0]) {
		pr_err("%s: wcnss subsystem failure reason: %s\n",
				__func__, "(unknown, init string found)");
	} else {
		size = smem_reset_size < MAX_BUF_SIZE ? smem_reset_size :
			(MAX_BUF_SIZE - 1);
		memcpy(buffer, smem_reset_reason, size);
		buffer[size] = '\0';
		pr_err("%s: wcnss subsystem failure reason: %s\n",
				__func__, buffer);
		memset(smem_reset_reason, 0, smem_reset_size);
		wmb();
	}

	ss_restart_inprogress = true;
	subsystem_restart("riva");
}

static irqreturn_t riva_wdog_bite_irq_hdlr(int irq, void *dev_id)
{
	riva_crash = true;

	if (ss_restart_inprogress) {
		pr_err("%s: Ignoring riva bite irq, restart in progress\n",
						MODULE_NAME);
		return IRQ_HANDLED;
	}

	if (!enable_riva_ssr)
		panic(MODULE_NAME ": Watchdog bite received from Riva");

	ss_restart_inprogress = true;
	subsystem_restart("riva");

	return IRQ_HANDLED;
}

/* SMSM reset Riva */
static void smsm_riva_reset(void)
{
	/* per SS reset request bit is not available now,
	 * all SS host modules are setting this bit
	 * This is still under discussion*/
	smsm_change_state(SMSM_APPS_STATE, SMSM_RESET, SMSM_RESET);
}

static void riva_post_bootup(struct work_struct *work)
{
	struct platform_device *pdev = wcnss_get_platform_device();
	struct wcnss_wlan_config *pwlanconfig = wcnss_get_wlan_config();

	pr_debug(MODULE_NAME ": Cancel APPS vote for Iris & Riva\n");

	wcnss_wlan_power(&pdev->dev, pwlanconfig,
		WCNSS_WLAN_SWITCH_OFF);
}

/* Subsystem handlers */
static int riva_shutdown(const struct subsys_data *subsys)
{
	pil_force_shutdown("wcnss");
	flush_delayed_work(&cancel_vote_work);
	disable_irq_nosync(RIVA_APSS_WDOG_BITE_RESET_RDY_IRQ);

	return 0;
}

static int riva_powerup(const struct subsys_data *subsys)
{
	struct platform_device *pdev = wcnss_get_platform_device();
	struct wcnss_wlan_config *pwlanconfig = wcnss_get_wlan_config();
	int    ret = -1;

	if (pdev && pwlanconfig)
		ret = wcnss_wlan_power(&pdev->dev, pwlanconfig,
					WCNSS_WLAN_SWITCH_ON);
	/* delay PIL operation, this SSR may be happening soon after kernel
	 * resumes because of a SMSM RESET by Riva when APPS was suspended.
	 * PIL fails to locate the images without this delay */
	if (!ret) {
		msleep(1000);
		pil_force_boot("wcnss");
	}
	ss_restart_inprogress = false;
	enable_irq(RIVA_APSS_WDOG_BITE_RESET_RDY_IRQ);
	schedule_delayed_work(&cancel_vote_work, msecs_to_jiffies(5000));

	return ret;
}

/* 5MB RAM segments for Riva SS */
static struct ramdump_segment riva_segments[] = {{0x8f200000,
						0x8f700000 - 0x8f200000} };

static int riva_ramdump(int enable, const struct subsys_data *subsys)
{
	pr_debug("%s: enable[%d]\n", MODULE_NAME, enable);
	if (enable)
		return do_ramdump(riva_ramdump_dev,
				riva_segments,
				ARRAY_SIZE(riva_segments));
	else
		return 0;
}

/* Riva crash handler */
static void riva_crash_shutdown(const struct subsys_data *subsys)
{
	pr_err("%s: crash shutdown : %d\n", MODULE_NAME, riva_crash);
	if (riva_crash != true)
		smsm_riva_reset();
}

static struct subsys_data riva_8960 = {
	.name = "riva",
	.shutdown = riva_shutdown,
	.powerup = riva_powerup,
	.ramdump = riva_ramdump,
	.crash_shutdown = riva_crash_shutdown
};

static int enable_riva_ssr_set(const char *val, struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret)
		return ret;

	if (enable_riva_ssr)
		pr_info(MODULE_NAME ": Subsystem restart activated for riva.\n");

	return 0;
}

module_param_call(enable_riva_ssr, enable_riva_ssr_set, param_get_int,
			&enable_riva_ssr, S_IRUGO | S_IWUSR);

static int __init riva_restart_init(void)
{
	return ssr_register_subsystem(&riva_8960);
}

static int __init riva_ssr_module_init(void)
{
	int ret;

	ret = smsm_state_cb_register(SMSM_WCNSS_STATE, SMSM_RESET,
					smsm_state_cb_hdlr, 0);
	if (ret < 0) {
		pr_err("%s: Unable to register smsm callback for Riva Reset! %d\n",
				MODULE_NAME, ret);
		goto out;
	}
	ret = request_irq(RIVA_APSS_WDOG_BITE_RESET_RDY_IRQ,
			riva_wdog_bite_irq_hdlr, IRQF_TRIGGER_HIGH,
				"riva_wdog", NULL);

	if (ret < 0) {
		pr_err("%s: Unable to register for Riva bite interrupt (%d)\n",
				MODULE_NAME, ret);
		goto out;
	}
	ret = riva_restart_init();
	if (ret < 0) {
		pr_err("%s: Unable to register with ssr. (%d)\n",
				MODULE_NAME, ret);
		goto out;
	}
	riva_ramdump_dev = create_ramdump_device("riva");
	if (!riva_ramdump_dev) {
		pr_err("%s: Unable to create ramdump device.\n",
				MODULE_NAME);
		ret = -ENOMEM;
		goto out;
	}
	INIT_DELAYED_WORK(&cancel_vote_work, riva_post_bootup);

	pr_info("%s: module initialized\n", MODULE_NAME);
out:
	return ret;
}

static void __exit riva_ssr_module_exit(void)
{
	free_irq(RIVA_APSS_WDOG_BITE_RESET_RDY_IRQ, NULL);
}

module_init(riva_ssr_module_init);
module_exit(riva_ssr_module_exit);

MODULE_LICENSE("GPL v2");
