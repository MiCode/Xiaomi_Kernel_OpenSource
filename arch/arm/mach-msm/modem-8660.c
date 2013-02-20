/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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
#define MODULE_NAME			"modem_8660"
#define MODEM_WDOG_ENABLE		0x10020008
#define MODEM_CLEANUP_DELAY_MS		20

#define SUBSYS_FATAL_DEBUG

#if defined(SUBSYS_FATAL_DEBUG)
static void debug_crash_modem_fn(struct work_struct *);
static int reset_modem;
static int ignore_smsm_ack;

static DECLARE_DELAYED_WORK(debug_crash_modem_work,
				debug_crash_modem_fn);

module_param(reset_modem, int, 0644);
#endif

/* Subsystem restart: Modem data, functions */
static void *modem_ramdump_dev;
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
		kernel_restart(NULL);

	} else {

		int ret;
		void *hwio_modem_reset_addr =
				ioremap_nocache(MODEM_HWIO_MSS_RESET_ADDR, 8);

		pr_err("%s: Modem AHB locked up.\n", MODULE_NAME);
		pr_err("%s: Trying to free up modem!\n", MODULE_NAME);

		writel_relaxed(0x3, hwio_modem_reset_addr);

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
		if (ignore_smsm_ack) {
			ignore_smsm_ack = 0;
			goto out;
		}
		pr_err("%s: Modem error fatal'ed.", MODULE_NAME);
		subsystem_restart("modem");
	}
out:
	return NOTIFY_DONE;
}

static int modem_shutdown(const struct subsys_data *crashed_subsys)
{
	void __iomem *modem_wdog_addr;

	/* If the modem didn't already crash, setting SMSM_RESET
	 * here will help flush caches etc. The ignore_smsm_ack
	 * flag is set to ignore the SMSM_RESET notification
	 * that is generated due to the modem settings its own
	 * SMSM_RESET bit in response to the apps setting the
	 * apps SMSM_RESET bit.
	 */
	if (!(smsm_get_state(SMSM_MODEM_STATE) & SMSM_RESET)) {
		ignore_smsm_ack = 1;
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



	return 0;
}

static int modem_powerup(const struct subsys_data *crashed_subsys)
{
	int ret;

	ret = pil_force_boot("modem");
	enable_irq(MARM_WDOG_EXPIRED);

	return ret;
}

/* FIXME: Get address, size from PIL */
static struct ramdump_segment modem_segments[] = {
	{0x42F00000, 0x46000000 - 0x42F00000} };

static int modem_ramdump(int enable,
				const struct subsys_data *crashed_subsys)
{
	if (enable)
		return do_ramdump(modem_ramdump_dev, modem_segments,
			ARRAY_SIZE(modem_segments));
	else
		return 0;
}

static void modem_crash_shutdown(
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

static irqreturn_t modem_wdog_bite_irq(int irq, void *dev_id)
{
	int ret;

	ret = schedule_work(&modem_fatal_work);
	disable_irq_nosync(MARM_WDOG_EXPIRED);

	return IRQ_HANDLED;
}

static struct subsys_data subsys_8660_modem = {
	.name = "modem",
	.shutdown = modem_shutdown,
	.powerup = modem_powerup,
	.ramdump = modem_ramdump,
	.crash_shutdown = modem_crash_shutdown
};

static int __init modem_8660_init(void)
{
	int ret;

	/* Need to listen for SMSM_RESET always */
	modem_register_notifier(&modem_notif_nb);

#if defined(SUBSYS_FATAL_DEBUG)
	schedule_delayed_work(&debug_crash_modem_work, msecs_to_jiffies(5000));
#endif

	ret = request_irq(MARM_WDOG_EXPIRED, modem_wdog_bite_irq,
			IRQF_TRIGGER_RISING, "modem_wdog", NULL);

	if (ret < 0) {
		pr_err("%s: Unable to request MARM_WDOG_EXPIRED irq.",
			__func__);
		goto out;
	}

	modem_ramdump_dev = create_ramdump_device("modem");

	if (!modem_ramdump_dev) {
		ret = -ENOMEM;
		goto out;
	}

	ret = ssr_register_subsystem(&subsys_8660_modem);
out:
	return ret;
}

static void __exit modem_8660_exit(void)
{
	free_irq(MARM_WDOG_EXPIRED, NULL);
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

module_init(modem_8660_init);
module_exit(modem_8660_exit);

