/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <mach/irqs.h>
#include <mach/scm.h>
#include <mach/peripheral-loader.h>
#include <mach/subsystem_restart.h>
#include <mach/subsystem_notif.h>

#include "smd_private.h"
#include "modem_notifier.h"
#include "ramdump.h"

#define MODEM_HWIO_MSS_RESET_ADDR       0x00902C48
#define SCM_Q6_NMI_CMD                  0x1
#define MODULE_NAME			"subsystem_fatal_8x60"
#define Q6SS_SOFT_INTR_WAKEUP		0x288A001C
#define MODEM_WDOG_ENABLE		0x10020008
#define Q6SS_WDOG_ENABLE		0x28882024
#define MODEM_CLEANUP_DELAY_MS		20

#define SUBSYS_FATAL_DEBUG

#if defined(SUBSYS_FATAL_DEBUG)
static void debug_crash_modem_fn(struct work_struct *);
static int reset_modem;

static DECLARE_DELAYED_WORK(debug_crash_modem_work,
				debug_crash_modem_fn);

module_param(reset_modem, int, 0644);
#endif

static void do_soc_restart(void);

/* Subsystem restart: QDSP6 data, functions */
static void q6_fatal_fn(struct work_struct *);
static DECLARE_WORK(q6_fatal_work, q6_fatal_fn);
static void *q6_ramdump_dev, *modem_ramdump_dev;
static void __iomem *q6_wakeup_intr;

static void q6_fatal_fn(struct work_struct *work)
{
	pr_err("%s: Watchdog bite received from Q6!\n", MODULE_NAME);
	subsystem_restart("lpass");
	enable_irq(LPASS_Q6SS_WDOG_EXPIRED);
}

static void send_q6_nmi(void)
{
	/* Send NMI to QDSP6 via an SCM call. */
	uint32_t cmd = 0x1;

	scm_call(SCM_SVC_UTIL, SCM_Q6_NMI_CMD,
	&cmd, sizeof(cmd), NULL, 0);

	/* Wakeup the Q6 */
	if (q6_wakeup_intr)
		writel_relaxed(0x2000, q6_wakeup_intr);
	mb();

	/* Q6 requires atleast 100ms to dump caches etc.*/
	mdelay(100);

	pr_info("subsystem-fatal-8x60: Q6 NMI was sent.\n");
}

int subsys_q6_shutdown(const struct subsys_data *crashed_subsys)
{
	void __iomem *q6_wdog_addr =
		ioremap_nocache(Q6SS_WDOG_ENABLE, 8);

	send_q6_nmi();
	writel_relaxed(0x0, q6_wdog_addr);
	/* The write needs to go through before the q6 is shutdown. */
	mb();
	iounmap(q6_wdog_addr);

	pil_force_shutdown("q6");
	disable_irq_nosync(LPASS_Q6SS_WDOG_EXPIRED);

	if (get_restart_level() == RESET_SUBSYS_MIXED)
		smsm_reset_modem(SMSM_RESET);

	return 0;
}

int subsys_q6_powerup(const struct subsys_data *crashed_subsys)
{
	int ret = pil_force_boot("q6");
	enable_irq(LPASS_Q6SS_WDOG_EXPIRED);
	return ret;
}

/* FIXME: Get address, size from PIL */
static struct ramdump_segment q6_segments[] = { {0x46700000, 0x47F00000 -
					0x46700000}, {0x28400000, 0x12800} };
static int subsys_q6_ramdump(int enable,
				const struct subsys_data *crashed_subsys)
{
	if (enable)
		return do_ramdump(q6_ramdump_dev, q6_segments,
				ARRAY_SIZE(q6_segments));
	else
		return 0;
}

void subsys_q6_crash_shutdown(const struct subsys_data *crashed_subsys)
{
	send_q6_nmi();
}

/* Subsystem restart: Modem data, functions */
static void modem_fatal_fn(struct work_struct *);
static void modem_unlock_timeout(struct work_struct *work);
static int modem_notif_handler(struct notifier_block *this,
				unsigned long code,
				void *_cmd);
static DECLARE_WORK(modem_fatal_work, modem_fatal_fn);
static DECLARE_DELAYED_WORK(modem_unlock_timeout_work,
				modem_unlock_timeout);

static struct notifier_block modem_notif_nb = {
	.notifier_call = modem_notif_handler,
};

static void modem_unlock_timeout(struct work_struct *work)
{
	void __iomem *hwio_modem_reset_addr =
			ioremap_nocache(MODEM_HWIO_MSS_RESET_ADDR, 8);
	pr_crit("%s: Timeout waiting for modem to unlock.\n", MODULE_NAME);

	/* Set MSS_MODEM_RESET to 0x0 since the unlock didn't work */
	writel_relaxed(0x0, hwio_modem_reset_addr);
	/* Write needs to go through before the modem is restarted. */
	mb();
	iounmap(hwio_modem_reset_addr);

	subsystem_restart("modem");
	enable_irq(MARM_WDOG_EXPIRED);
}

static void modem_fatal_fn(struct work_struct *work)
{
	uint32_t modem_state;
	uint32_t panic_smsm_states = SMSM_RESET | SMSM_SYSTEM_DOWNLOAD;
	uint32_t reset_smsm_states = SMSM_SYSTEM_REBOOT_USR |
					SMSM_SYSTEM_PWRDWN_USR;

	pr_err("%s: Watchdog bite received from modem!\n", MODULE_NAME);

	modem_state = smsm_get_state(SMSM_MODEM_STATE);
	pr_err("%s: Modem SMSM state = 0x%x!", MODULE_NAME, modem_state);

	if (modem_state == 0 || modem_state & panic_smsm_states) {

		subsystem_restart("modem");
		enable_irq(MARM_WDOG_EXPIRED);

	} else if (modem_state & reset_smsm_states) {

		pr_err("%s: User-invoked system reset/powerdown.",
			MODULE_NAME);
		do_soc_restart();

	} else {

		int ret;
		void *hwio_modem_reset_addr =
				ioremap_nocache(MODEM_HWIO_MSS_RESET_ADDR, 8);

		pr_err("%s: Modem AHB locked up.\n", MODULE_NAME);
		pr_err("%s: Trying to free up modem!\n", MODULE_NAME);

		writel(0x3, hwio_modem_reset_addr);

		/* If we are still alive after 6 seconds (allowing for
		 * the 5-second-delayed-panic-reboot), modem is either
		 * still wedged or SMSM didn't come through. Force panic
		 * in that case.
		*/
		ret = schedule_delayed_work(&modem_unlock_timeout_work,
					msecs_to_jiffies(6000));

		iounmap(hwio_modem_reset_addr);
	}
}

static int modem_notif_handler(struct notifier_block *this,
				unsigned long code,
				void *_cmd)
{
	if (code == MODEM_NOTIFIER_START_RESET) {

		pr_err("%s: Modem error fatal'ed.", MODULE_NAME);
		subsystem_restart("modem");
	}
	return NOTIFY_DONE;
}

static int subsys_modem_shutdown(const struct subsys_data *crashed_subsys)
{
	void __iomem *modem_wdog_addr;
	int smsm_notif_unregistered = 0;

	/* If the modem didn't already crash, setting SMSM_RESET
	 * here will help flush caches etc. Unregister for SMSM
	 * notifications to prevent unnecessary secondary calls to
	 * subsystem_restart.
	 */
	if (!(smsm_get_state(SMSM_MODEM_STATE) & SMSM_RESET)) {
		modem_unregister_notifier(&modem_notif_nb);
		smsm_notif_unregistered = 1;
		smsm_reset_modem(SMSM_RESET);
	}

	/* Disable the modem watchdog to allow clean modem bootup */
	modem_wdog_addr = ioremap_nocache(MODEM_WDOG_ENABLE, 8);
	writel_relaxed(0x0, modem_wdog_addr);

	/*
	 * The write above needs to go through before the modem is
	 * powered up again (subsystem restart).
	 */
	mb();
	iounmap(modem_wdog_addr);

	/* Wait here to allow the modem to clean up caches etc. */
	msleep(MODEM_CLEANUP_DELAY_MS);
	pil_force_shutdown("modem");
	disable_irq_nosync(MARM_WDOG_EXPIRED);

	/* Re-register for SMSM notifications if necessary */
	if (smsm_notif_unregistered)
		modem_register_notifier(&modem_notif_nb);


	return 0;
}

static int subsys_modem_powerup(const struct subsys_data *crashed_subsys)
{
	int ret;

	ret = pil_force_boot("modem");
	enable_irq(MARM_WDOG_EXPIRED);

	return ret;
}

/* FIXME: Get address, size from PIL */
static struct ramdump_segment modem_segments[] = {
	{0x42F00000, 0x46000000 - 0x42F00000} };

static int subsys_modem_ramdump(int enable,
				const struct subsys_data *crashed_subsys)
{
	if (enable)
		return do_ramdump(modem_ramdump_dev, modem_segments,
			ARRAY_SIZE(modem_segments));
	else
		return 0;
}

static void subsys_modem_crash_shutdown(
				const struct subsys_data *crashed_subsys)
{
	/* If modem hasn't already crashed, send SMSM_RESET. */
	if (!(smsm_get_state(SMSM_MODEM_STATE) & SMSM_RESET)) {
		modem_unregister_notifier(&modem_notif_nb);
		smsm_reset_modem(SMSM_RESET);
	}

	/* Wait to allow the modem to clean up caches etc. */
	mdelay(5);
}

/* Non-subsystem-specific functions */
static void do_soc_restart(void)
{
	pr_err("%s: Rebooting SoC..\n", MODULE_NAME);
	kernel_restart(NULL);
}

static irqreturn_t subsys_wdog_bite_irq(int irq, void *dev_id)
{
	int ret;

	switch (irq) {

	case MARM_WDOG_EXPIRED:
		ret = schedule_work(&modem_fatal_work);
		disable_irq_nosync(MARM_WDOG_EXPIRED);
	break;

	case LPASS_Q6SS_WDOG_EXPIRED:
		ret = schedule_work(&q6_fatal_work);
		disable_irq_nosync(LPASS_Q6SS_WDOG_EXPIRED);
	break;

	default:
		pr_err("%s: %s: Unknown IRQ!\n", MODULE_NAME, __func__);
	}

	return IRQ_HANDLED;
}

static struct subsys_data subsys_8x60_q6 = {
	.name = "lpass",
	.shutdown = subsys_q6_shutdown,
	.powerup = subsys_q6_powerup,
	.ramdump = subsys_q6_ramdump,
	.crash_shutdown = subsys_q6_crash_shutdown
};

static struct subsys_data subsys_8x60_modem = {
	.name = "modem",
	.shutdown = subsys_modem_shutdown,
	.powerup = subsys_modem_powerup,
	.ramdump = subsys_modem_ramdump,
	.crash_shutdown = subsys_modem_crash_shutdown
};

static int __init subsystem_restart_8x60_init(void)
{
	ssr_register_subsystem(&subsys_8x60_modem);
	ssr_register_subsystem(&subsys_8x60_q6);

	return 0;
}

static int __init subsystem_fatal_init(void)
{
	int ret;

	/* Need to listen for SMSM_RESET always */
	modem_register_notifier(&modem_notif_nb);

#if defined(SUBSYS_FATAL_DEBUG)
	schedule_delayed_work(&debug_crash_modem_work, msecs_to_jiffies(5000));
#endif

	ret = request_irq(MARM_WDOG_EXPIRED, subsys_wdog_bite_irq,
			IRQF_TRIGGER_RISING, "modem_wdog", NULL);

	if (ret < 0) {
		pr_err("%s: Unable to request MARM_WDOG_EXPIRED irq.",
			__func__);
		goto out;
	}

	ret = request_irq(LPASS_Q6SS_WDOG_EXPIRED, subsys_wdog_bite_irq,
			IRQF_TRIGGER_RISING, "q6_wdog", NULL);

	if (ret < 0) {
		pr_err("%s: Unable to request LPASS_Q6SS_WDOG_EXPIRED irq.",
			__func__);
		goto out;
	}

	q6_wakeup_intr = ioremap_nocache(Q6SS_SOFT_INTR_WAKEUP, 8);

	if (!q6_wakeup_intr)
		pr_err("%s: Unable to request q6 wakeup interrupt.", __func__);

	q6_ramdump_dev = create_ramdump_device("lpass");

	if (!q6_ramdump_dev) {
		ret = -ENOMEM;
		goto out;
	}

	modem_ramdump_dev = create_ramdump_device("modem");

	if (!modem_ramdump_dev) {
		ret = -ENOMEM;
		goto out;
	}

	ret = subsystem_restart_8x60_init();
out:
	return ret;
}

static void __exit subsystem_fatal_exit(void)
{
	free_irq(MARM_WDOG_EXPIRED, NULL);
	free_irq(LPASS_Q6SS_WDOG_EXPIRED, NULL);
}

#ifdef SUBSYS_FATAL_DEBUG
static void debug_crash_modem_fn(struct work_struct *work)
{
	if (reset_modem == 1)
		smsm_reset_modem(SMSM_RESET);
	else if (reset_modem == 2)
		subsystem_restart("lpass");

	reset_modem = 0;
	schedule_delayed_work(&debug_crash_modem_work, msecs_to_jiffies(1000));
}
#endif

module_init(subsystem_fatal_init);
module_exit(subsystem_fatal_exit);
