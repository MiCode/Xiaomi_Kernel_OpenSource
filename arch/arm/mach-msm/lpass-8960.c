/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/err.h>

#include <mach/irqs.h>
#include <mach/scm.h>
#include <mach/peripheral-loader.h>
#include <mach/subsystem_restart.h>
#include <mach/subsystem_notif.h>

#include "smd_private.h"
#include "ramdump.h"
#include "sysmon.h"

#define SCM_Q6_NMI_CMD                  0x1
#define MODULE_NAME			"lpass_8960"
#define Q6SS_SOFT_INTR_WAKEUP		0x28800024

/* Subsystem restart: QDSP6 data, functions */
static void lpass_fatal_fn(struct work_struct *);
static DECLARE_WORK(lpass_fatal_work, lpass_fatal_fn);
void __iomem *q6_wakeup_intr;
struct lpass_ssr {
	void *lpass_ramdump_dev;
} lpass_ssr;

static struct lpass_ssr lpass_ssr_8960;
static int q6_crash_shutdown;

static int riva_notifier_cb(struct notifier_block *this, unsigned long code,
								void *ss_handle)
{
	int ret;
	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		pr_debug("%s: R-Notify: Shutdown started\n", __func__);
		ret = sysmon_send_event(SYSMON_SS_LPASS, "wcnss",
				SUBSYS_BEFORE_SHUTDOWN);
		if (ret < 0)
			pr_err("%s: sysmon_send_event error %d", __func__,
				ret);
		break;
	}
	return NOTIFY_DONE;
}

static void *ssr_notif_hdle;
static struct notifier_block rnb = {
	.notifier_call = riva_notifier_cb,
};

static void lpass_fatal_fn(struct work_struct *work)
{
	pr_err("%s %s: Watchdog bite received from Q6!\n", MODULE_NAME,
		__func__);
	panic(MODULE_NAME ": Resetting the SoC");
}

static void lpass_smsm_state_cb(void *data, uint32_t old_state,
				uint32_t new_state)
{
	/* Ignore if we're the one that set SMSM_RESET */
	if (q6_crash_shutdown)
		return;

	if (new_state & SMSM_RESET) {
		pr_err("%s: LPASS SMSM state changed to SMSM_RESET,"
			" new_state = 0x%x, old_state = 0x%x\n", __func__,
			new_state, old_state);
		panic(MODULE_NAME ": Resetting the SoC");
	}
}

static void send_q6_nmi(void)
{
	/* Send NMI to QDSP6 via an SCM call. */
	uint32_t cmd = 0x1;

	scm_call(SCM_SVC_UTIL, SCM_Q6_NMI_CMD,
	&cmd, sizeof(cmd), NULL, 0);

	/* Wakeup the Q6 */
	if (q6_wakeup_intr)
		writel_relaxed(0x01, q6_wakeup_intr);
	mb();

	/* Q6 requires worstcase 100ms to dump caches etc.*/
	mdelay(100);
	pr_debug("%s: Q6 NMI was sent.\n", __func__);
}

static int lpass_shutdown(const struct subsys_data *subsys)
{
	send_q6_nmi();
	pil_force_shutdown("q6");
	disable_irq_nosync(LPASS_Q6SS_WDOG_EXPIRED);

	return 0;
}

static int lpass_powerup(const struct subsys_data *subsys)
{
	int ret = pil_force_boot("q6");
	enable_irq(LPASS_Q6SS_WDOG_EXPIRED);
	return ret;
}
/* RAM segments - address and size for 8960 */
static struct ramdump_segment q6_segments[] = { {0x8da00000, 0x8f200000 -
					0x8da00000}, {0x28400000, 0x20000} };
static int lpass_ramdump(int enable, const struct subsys_data *subsys)
{
	pr_debug("%s: enable[%d]\n", __func__, enable);
	if (enable)
		return do_ramdump(lpass_ssr_8960.lpass_ramdump_dev,
				q6_segments,
				ARRAY_SIZE(q6_segments));
	else
		return 0;
}

static void lpass_crash_shutdown(const struct subsys_data *subsys)
{
	q6_crash_shutdown = 1;
	send_q6_nmi();
}

static irqreturn_t lpass_wdog_bite_irq(int irq, void *dev_id)
{
	int ret;

	pr_debug("%s: rxed irq[0x%x]", __func__, irq);
	disable_irq_nosync(LPASS_Q6SS_WDOG_EXPIRED);
	ret = schedule_work(&lpass_fatal_work);

	return IRQ_HANDLED;
}

static struct subsys_data lpass_8960 = {
	.name = "lpass",
	.shutdown = lpass_shutdown,
	.powerup = lpass_powerup,
	.ramdump = lpass_ramdump,
	.crash_shutdown = lpass_crash_shutdown
};

static int __init lpass_restart_init(void)
{
	return ssr_register_subsystem(&lpass_8960);
}

static int __init lpass_fatal_init(void)
{
	int ret;

	ret = smsm_state_cb_register(SMSM_Q6_STATE, SMSM_RESET,
		lpass_smsm_state_cb, 0);

	if (ret < 0)
		pr_err("%s: Unable to register SMSM callback! (%d)\n",
				__func__, ret);

	ret = request_irq(LPASS_Q6SS_WDOG_EXPIRED, lpass_wdog_bite_irq,
			IRQF_TRIGGER_RISING, "q6_wdog", NULL);

	if (ret < 0) {
		pr_err("%s: Unable to request LPASS_Q6SS_WDOG_EXPIRED irq.",
			__func__);
		goto out;
	}
	ret = lpass_restart_init();
	if (ret < 0) {
		pr_err("%s: Unable to reg with lpass ssr. (%d)\n",
				__func__, ret);
		goto out;
	}
	q6_wakeup_intr = ioremap_nocache(Q6SS_SOFT_INTR_WAKEUP, 8);
	if (!q6_wakeup_intr)
		pr_err("%s: Unable to request q6 wakeup interrupt\n", __func__);

	lpass_ssr_8960.lpass_ramdump_dev = create_ramdump_device("lpass");

	if (!lpass_ssr_8960.lpass_ramdump_dev) {
		pr_err("%s: Unable to create ramdump device.\n",
				__func__);
		ret = -ENOMEM;
		goto out;
	}
	ssr_notif_hdle = subsys_notif_register_notifier("riva",
							&rnb);
	if (IS_ERR(ssr_notif_hdle) < 0) {
		ret = PTR_ERR(ssr_notif_hdle);
		pr_err("%s: subsys_register_notifier for Riva: err = %d\n",
			__func__, ret);
		iounmap(q6_wakeup_intr);
		free_irq(LPASS_Q6SS_WDOG_EXPIRED, NULL);
		goto out;
	}

	pr_info("%s: lpass SSR driver init'ed.\n", __func__);
out:
	return ret;
}

static void __exit lpass_fatal_exit(void)
{
	subsys_notif_unregister_notifier(ssr_notif_hdle, &rnb);
	iounmap(q6_wakeup_intr);
	free_irq(LPASS_Q6SS_WDOG_EXPIRED, NULL);
}

module_init(lpass_fatal_init);
module_exit(lpass_fatal_exit);

MODULE_LICENSE("GPL v2");
