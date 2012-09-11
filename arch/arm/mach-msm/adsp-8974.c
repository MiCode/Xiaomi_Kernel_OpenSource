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
#include <linux/err.h>

#include <mach/irqs.h>
#include <mach/scm.h>
#include <mach/peripheral-loader.h>
#include <mach/subsystem_restart.h>
#include <mach/subsystem_notif.h>

#include "smd_private.h"
#include "ramdump.h"
#include "sysmon.h"

#define SCM_Q6_NMI_CMD			0x1
#define MODULE_NAME			"adsp_8974"
#define MAX_BUF_SIZE			0x51

/* Interrupt line for WDOG bite*/
#define ADSP_Q6SS_WDOG_EXPIRED		194

/* Subsystem restart: QDSP6 data, functions */
static void adsp_fatal_fn(struct work_struct *);
static DECLARE_WORK(adsp_fatal_work, adsp_fatal_fn);

struct adsp_ssr {
	void *adsp_ramdump_dev;
} adsp_ssr;

static struct adsp_ssr adsp_ssr_8974;
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

static int modem_notifier_cb(struct notifier_block *this, unsigned long code,
								void *ss_handle)
{
	int ret;
	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		pr_debug("%s: M-Notify: Shutdown started\n", __func__);
		ret = sysmon_send_event(SYSMON_SS_LPASS, "modem",
				SUBSYS_BEFORE_SHUTDOWN);
		if (ret < 0)
			pr_err("%s: sysmon_send_event error %d", __func__,
				ret);
		break;
	}
	return NOTIFY_DONE;
}

static void *ssr_modem_notif_hdle;
static struct notifier_block mnb = {
	.notifier_call = modem_notifier_cb,
};

static void adsp_log_failure_reason(void)
{
	char *reason;
	char buffer[MAX_BUF_SIZE];
	unsigned size;

	reason = smem_get_entry(SMEM_SSR_REASON_LPASS0, &size);

	if (!reason) {
		pr_err("%s: subsystem failure reason: (unknown, smem_get_entry failed).",
			 MODULE_NAME);
		return;
	}

	if (reason[0] == '\0') {
		pr_err("%s: subsystem failure reason: (unknown, init value found)",
			 MODULE_NAME);
		return;
	}

	size = size < MAX_BUF_SIZE ? size : (MAX_BUF_SIZE-1);
	memcpy(buffer, reason, size);
	buffer[size] = '\0';
	pr_err("%s: subsystem failure reason: %s", MODULE_NAME, buffer);
	memset((void *)reason, 0x0, size);
	wmb();
}

static void adsp_fatal_fn(struct work_struct *work)
{
	pr_err("%s %s: Watchdog bite received from Q6!\n", MODULE_NAME,
		__func__);
	adsp_log_failure_reason();
	panic(MODULE_NAME ": Resetting the SoC");
}

static void adsp_smsm_state_cb(void *data, uint32_t old_state,
				uint32_t new_state)
{
	/* Ignore if we're the one that set SMSM_RESET */
	if (q6_crash_shutdown)
		return;

	if (new_state & SMSM_RESET) {
		pr_debug("%s: ADSP SMSM state changed to SMSM_RESET, new_state= 0x%x, old_state = 0x%x\n",
			 __func__, new_state, old_state);
		adsp_log_failure_reason();
		panic(MODULE_NAME ": Resetting the SoC");
	}
}

static void send_q6_nmi(void)
{
	/* Send NMI to QDSP6 via an SCM call. */
	scm_call_atomic1(SCM_SVC_UTIL, SCM_Q6_NMI_CMD, 0x1);
	pr_debug("%s: Q6 NMI was sent.\n", __func__);
}

static int adsp_shutdown(const struct subsys_desc *subsys)
{
	send_q6_nmi();

	/* The write needs to go through before the q6 is shutdown. */
	mb();

	pil_force_shutdown("q6");
	disable_irq_nosync(ADSP_Q6SS_WDOG_EXPIRED);

	return 0;
}

static int adsp_powerup(const struct subsys_desc *subsys)
{
	int ret;

	if (get_restart_level() == RESET_SUBSYS_INDEPENDENT) {
		pr_debug("%s: Wait for ADSP power up!", __func__);
		msleep(10000);
	}

	ret = pil_force_boot("q6");
	enable_irq(ADSP_Q6SS_WDOG_EXPIRED);
	return ret;
}
/* RAM segments - address and size for 8974 */
static struct ramdump_segment q6_segment = {0xdc00000, 0x1800000};

static int adsp_ramdump(int enable, const struct subsys_desc *subsys)
{
	pr_debug("%s: enable[%d]\n", __func__, enable);
	if (enable)
		return do_ramdump(adsp_ssr_8974.adsp_ramdump_dev,
				&q6_segment, 1);
	else
		return 0;
}

static void adsp_crash_shutdown(const struct subsys_desc *subsys)
{
	q6_crash_shutdown = 1;
	send_q6_nmi();
}

static irqreturn_t adsp_wdog_bite_irq(int irq, void *dev_id)
{
	int ret;

	pr_debug("%s: rxed irq[0x%x]", __func__, irq);
	disable_irq_nosync(ADSP_Q6SS_WDOG_EXPIRED);
	ret = schedule_work(&adsp_fatal_work);

	return IRQ_HANDLED;
}

static struct subsys_device *adsp_8974_dev;

static struct subsys_desc adsp_8974 = {
	.name = "adsp",
	.shutdown = adsp_shutdown,
	.powerup = adsp_powerup,
	.ramdump = adsp_ramdump,
	.crash_shutdown = adsp_crash_shutdown
};

static int __init adsp_restart_init(void)
{
	adsp_8974_dev = subsys_register(&adsp_8974);
	if (IS_ERR(adsp_8974_dev))
		return PTR_ERR(adsp_8974_dev);
	return 0;
}

static int __init adsp_fatal_init(void)
{
	int ret;

	ret = smsm_state_cb_register(SMSM_Q6_STATE, SMSM_RESET,
		adsp_smsm_state_cb, 0);

	if (ret < 0)
		pr_err("%s: Unable to register SMSM callback! (%d)\n",
				__func__, ret);

	ret = request_irq(ADSP_Q6SS_WDOG_EXPIRED, adsp_wdog_bite_irq,
			IRQF_TRIGGER_RISING, "q6_wdog", NULL);

	if (ret < 0) {
		pr_err("%s: Unable to request ADSP_Q6SS_WDOG_EXPIRED irq.",
			__func__);
		goto out;
	}
	ret = adsp_restart_init();
	if (ret < 0) {
		pr_err("%s: Unable to reg with adsp ssr. (%d)\n",
				__func__, ret);
		goto out;
	}

	adsp_ssr_8974.adsp_ramdump_dev = create_ramdump_device("adsp");

	if (!adsp_ssr_8974.adsp_ramdump_dev) {
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
		free_irq(ADSP_Q6SS_WDOG_EXPIRED, NULL);
		goto out;
	}

	ssr_modem_notif_hdle = subsys_notif_register_notifier("modem",
							&mnb);
	if (IS_ERR(ssr_modem_notif_hdle) < 0) {
		ret = PTR_ERR(ssr_modem_notif_hdle);
		pr_err("%s: subsys_register_notifier for Modem: err = %d\n",
			__func__, ret);
		subsys_notif_unregister_notifier(ssr_notif_hdle, &rnb);
		free_irq(ADSP_Q6SS_WDOG_EXPIRED, NULL);
		goto out;
	}

	pr_info("%s: adsp ssr driver init'ed.\n", __func__);
out:
	return ret;
}

static void __exit adsp_fatal_exit(void)
{
	subsys_notif_unregister_notifier(ssr_notif_hdle, &rnb);
	subsys_notif_unregister_notifier(ssr_modem_notif_hdle, &mnb);
	subsys_unregister(adsp_8974_dev);
	free_irq(ADSP_Q6SS_WDOG_EXPIRED, NULL);
}

module_init(adsp_fatal_init);
module_exit(adsp_fatal_exit);

MODULE_LICENSE("GPL v2");
