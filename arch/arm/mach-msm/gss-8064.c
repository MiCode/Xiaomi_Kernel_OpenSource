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
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/stringify.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>

#include <mach/irqs.h>
#include <mach/msm_smsm.h>
#include <mach/scm.h>
#include <mach/peripheral-loader.h>
#include <mach/subsystem_restart.h>
#include <mach/subsystem_notif.h>
#include <mach/socinfo.h>

#include "smd_private.h"
#include "modem_notifier.h"
#include "ramdump.h"

static struct gss_8064_data {
	struct miscdevice gss_dev;
	void *pil_handle;
	void *gss_ramdump_dev;
	void *smem_ramdump_dev;
} gss_data;

static int crash_shutdown;

#define MAX_SSR_REASON_LEN 81U

static void log_gss_sfr(void)
{
	u32 size;
	char *smem_reason, reason[MAX_SSR_REASON_LEN];

	smem_reason = smem_get_entry(SMEM_SSR_REASON_MSS0, &size);
	if (!smem_reason || !size) {
		pr_err("GSS subsystem failure reason: (unknown, smem_get_entry failed).\n");
		return;
	}
	if (!smem_reason[0]) {
		pr_err("GSS subsystem failure reason: (unknown, init string found).\n");
		return;
	}

	size = min(size, MAX_SSR_REASON_LEN-1);
	memcpy(reason, smem_reason, size);
	reason[size] = '\0';
	pr_err("GSS subsystem failure reason: %s.\n", reason);

	smem_reason[0] = '\0';
	wmb();
}

static void restart_gss(void)
{
	log_gss_sfr();
	subsystem_restart("gss");
}

static void smsm_state_cb(void *data, uint32_t old_state, uint32_t new_state)
{
	/* Ignore if we're the one that set SMSM_RESET */
	if (crash_shutdown)
		return;

	if (new_state & SMSM_RESET) {
		pr_err("GSS SMSM state changed to SMSM_RESET.\n"
			"Probable err_fatal on the GSS. "
			"Calling subsystem restart...\n");
		restart_gss();
	}
}

#define Q6_FW_WDOG_ENABLE		0x08882024
#define Q6_SW_WDOG_ENABLE		0x08982024
static int gss_shutdown(const struct subsys_data *subsys)
{
	pil_force_shutdown("gss");
	disable_irq_nosync(GSS_A5_WDOG_EXPIRED);

	return 0;
}

static int gss_powerup(const struct subsys_data *subsys)
{
	pil_force_boot("gss");
	enable_irq(GSS_A5_WDOG_EXPIRED);
	return 0;
}

void gss_crash_shutdown(const struct subsys_data *subsys)
{
	crash_shutdown = 1;
	smsm_reset_modem(SMSM_RESET);
}

/* FIXME: Get address, size from PIL */
static struct ramdump_segment gss_segments[] = {
	{0x89000000, 0x00D00000}
};

static struct ramdump_segment smem_segments[] = {
	{0x80000000, 0x00200000},
};

static int gss_ramdump(int enable,
				const struct subsys_data *crashed_subsys)
{
	int ret = 0;

	if (enable) {
		ret = do_ramdump(gss_data.gss_ramdump_dev, gss_segments,
			ARRAY_SIZE(gss_segments));

		if (ret < 0) {
			pr_err("Unable to dump gss memory (rc = %d).\n",
			       ret);
			goto out;
		}

		ret = do_ramdump(gss_data.smem_ramdump_dev, smem_segments,
			ARRAY_SIZE(smem_segments));

		if (ret < 0) {
			pr_err("Unable to dump smem memory (rc = %d).\n", ret);
			goto out;
		}
	}

out:
	return ret;
}

static irqreturn_t gss_wdog_bite_irq(int irq, void *dev_id)
{
	pr_err("Watchdog bite received from GSS!\n");
	restart_gss();

	return IRQ_HANDLED;
}

static struct subsys_data gss_8064 = {
	.name = "gss",
	.shutdown = gss_shutdown,
	.powerup = gss_powerup,
	.ramdump = gss_ramdump,
	.crash_shutdown = gss_crash_shutdown
};

static int gss_subsystem_restart_init(void)
{
	return ssr_register_subsystem(&gss_8064);
}

static int gss_open(struct inode *inode, struct file *filep)
{
	void *ret;
	gss_data.pil_handle = ret = pil_get("gss");
	if (!ret)
		pr_debug("%s - pil_get returned NULL\n", __func__);
	return 0;
}

static int gss_release(struct inode *inode, struct file *filep)
{
	pil_put(gss_data.pil_handle);
	pr_debug("%s pil_put called on GSS\n", __func__);
	return 0;
}

const struct file_operations gss_file_ops = {
	.open = gss_open,
	.release = gss_release,
};

static int __init gss_8064_init(void)
{
	int ret;

	if (!cpu_is_apq8064())
		return -ENODEV;

	ret = smsm_state_cb_register(SMSM_MODEM_STATE, SMSM_RESET,
		smsm_state_cb, 0);

	if (ret < 0)
		pr_err("%s: Unable to register SMSM callback! (%d)\n",
				__func__, ret);

	ret = request_irq(GSS_A5_WDOG_EXPIRED, gss_wdog_bite_irq,
			IRQF_TRIGGER_RISING, "gss_a5_wdog", NULL);

	if (ret < 0) {
		pr_err("%s: Unable to request gss watchdog IRQ. (%d)\n",
				__func__, ret);
		disable_irq_nosync(GSS_A5_WDOG_EXPIRED);
		goto out;
	}

	ret = gss_subsystem_restart_init();

	if (ret < 0) {
		pr_err("%s: Unable to reg with subsystem restart. (%d)\n",
				__func__, ret);
		goto out;
	}

	gss_data.gss_dev.minor = MISC_DYNAMIC_MINOR;
	gss_data.gss_dev.name = "gss";
	gss_data.gss_dev.fops = &gss_file_ops;
	ret = misc_register(&gss_data.gss_dev);

	if (ret) {
		pr_err("%s: misc_registers failed for %s (%d)", __func__,
				gss_data.gss_dev.name, ret);
		goto out;
	}

	gss_data.gss_ramdump_dev = create_ramdump_device("gss");

	if (!gss_data.gss_ramdump_dev) {
		pr_err("%s: Unable to create gss ramdump device. (%d)\n",
				__func__, -ENOMEM);
		ret = -ENOMEM;
		goto out;
	}

	gss_data.smem_ramdump_dev = create_ramdump_device("smem-gss");

	if (!gss_data.smem_ramdump_dev) {
		pr_err("%s: Unable to create smem ramdump device. (%d)\n",
				__func__, -ENOMEM);
		ret = -ENOMEM;
		goto out;
	}

	pr_info("%s: gss fatal driver init'ed.\n", __func__);
out:
	return ret;
}

module_init(gss_8064_init);
