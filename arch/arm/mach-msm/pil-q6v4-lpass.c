/* Copyright (c) 2012,2013 The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <mach/scm.h>
#include <mach/subsystem_restart.h>
#include <mach/subsystem_notif.h>
#include <mach/ramdump.h>
#include <mach/msm_smem.h>
#include <mach/msm_bus_board.h>

#include "smd_private.h"
#include "sysmon.h"
#include "peripheral-loader.h"
#include "pil-q6v4.h"
#include "scm-pas.h"

struct lpass_q6v4 {
	struct q6v4_data q6;
	void *riva_notif_hdle;
	void *modem_notif_hdle;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	int crash_shutdown;
	void *ramdump_dev;
	struct work_struct work;
	int loadable;
};

static int pil_q6v4_lpass_boot(struct pil_desc *pil)
{
	struct q6v4_data *drv = pil_to_q6v4_data(pil);
	int err;

	err = pil_q6v4_power_up(drv);
	if (err)
		return err;

	return pil_q6v4_boot(pil);
}

static int pil_q6v4_lpass_shutdown(struct pil_desc *pil)
{
	struct q6v4_data *drv = pil_to_q6v4_data(pil);
	int ret;

	ret = pil_q6v4_shutdown(pil);
	if (ret)
		return ret;
	pil_q6v4_power_down(drv);
	return 0;
}

static struct pil_reset_ops pil_q6v4_lpass_ops = {
	.auth_and_reset = pil_q6v4_lpass_boot,
	.shutdown = pil_q6v4_lpass_shutdown,
	.proxy_vote = pil_q6v4_make_proxy_votes,
	.proxy_unvote = pil_q6v4_remove_proxy_votes,
};

static struct pil_reset_ops pil_q6v4_lpass_ops_trusted = {
	.init_image = pil_q6v4_init_image_trusted,
	.auth_and_reset = pil_q6v4_boot_trusted,
	.shutdown = pil_q6v4_shutdown_trusted,
	.proxy_vote = pil_q6v4_make_proxy_votes,
	.proxy_unvote = pil_q6v4_remove_proxy_votes,
};

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
			pr_err("%s: sysmon_send_event error %d", __func__, ret);
		break;
	}
	return NOTIFY_DONE;
}

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
			pr_err("%s: sysmon_send_event error %d", __func__, ret);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block mnb = {
	.notifier_call = modem_notifier_cb,
};

static void lpass_log_failure_reason(void)
{
	char *reason;
	char buffer[81];
	unsigned size;

	reason = smem_get_entry(SMEM_SSR_REASON_LPASS0, &size);

	if (!reason) {
		pr_err("LPASS subsystem failure reason: (unknown, smem_get_entry failed).");
		return;
	}

	if (reason[0] == '\0') {
		pr_err("LPASS subsystem failure reason: (unknown, init value found)");
		return;
	}

	size = min(size, sizeof(buffer) - 1);
	memcpy(buffer, reason, size);
	buffer[size] = '\0';
	pr_err("LPASS subsystem failure reason: %s", buffer);
	memset((void *)reason, 0x0, size);
	wmb();
}

static void lpass_fatal_fn(struct work_struct *work)
{
	pr_err("Watchdog bite received from lpass Q6!\n");
	lpass_log_failure_reason();
	panic("Q6 Resetting the SoC");
}

static void lpass_smsm_state_cb(void *data, uint32_t old_state,
				uint32_t new_state)
{
	struct lpass_q6v4 *drv = data;

	/* Ignore if we're the one that set SMSM_RESET */
	if (drv->crash_shutdown)
		return;

	if (new_state & SMSM_RESET) {
		pr_err("%s: LPASS SMSM state changed to SMSM_RESET, new_state = %#x, old_state = %#x\n",
				__func__, new_state, old_state);
		lpass_log_failure_reason();
		panic("Q6 Resetting the SoC");
	}
}

#define SCM_Q6_NMI_CMD 0x1

static void send_q6_nmi(void)
{
	/* Send NMI to QDSP6 via an SCM call. */
	uint32_t cmd = 0x1;

	scm_call(SCM_SVC_UTIL, SCM_Q6_NMI_CMD,
	&cmd, sizeof(cmd), NULL, 0);

	/* Q6 requires worstcase 100ms to dump caches etc.*/
	mdelay(100);
	pr_debug("%s: Q6 NMI was sent.\n", __func__);
}

#define subsys_to_lpass(d) container_of(d, struct lpass_q6v4, subsys_desc)

static int lpass_shutdown(const struct subsys_desc *subsys, bool force_stop)
{
	struct lpass_q6v4 *drv = subsys_to_lpass(subsys);

	if (force_stop)
		send_q6_nmi();
	if (drv->loadable)
		pil_shutdown(&drv->q6.desc);
	disable_irq_nosync(drv->q6.wdog_irq);

	return 0;
}

static int lpass_powerup(const struct subsys_desc *subsys)
{
	struct lpass_q6v4 *drv = subsys_to_lpass(subsys);
	int ret = 0;

	if (drv->loadable)
		ret = pil_boot(&drv->q6.desc);
	enable_irq(drv->q6.wdog_irq);

	return ret;
}

static int lpass_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct lpass_q6v4 *drv = subsys_to_lpass(subsys);

	if (!enable)
		return 0;

	return pil_do_ramdump(&drv->q6.desc, drv->ramdump_dev);
}

static void lpass_crash_shutdown(const struct subsys_desc *subsys)
{
	struct lpass_q6v4 *drv = subsys_to_lpass(subsys);

	drv->crash_shutdown = 1;
	send_q6_nmi();
}

static irqreturn_t lpass_wdog_bite_irq(int irq, void *dev_id)
{
	struct lpass_q6v4 *drv = dev_id;

	schedule_work(&drv->work);
	return IRQ_HANDLED;
}

static int pil_q6v4_lpass_driver_probe(struct platform_device *pdev)
{
	const struct pil_q6v4_pdata *pdata = pdev->dev.platform_data;
	struct lpass_q6v4 *drv;
	struct q6v4_data *q6;
	struct pil_desc *desc;
	struct resource *res;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);
	q6 = &drv->q6;
	desc = &q6->desc;

	q6->wdog_irq = platform_get_irq(pdev, 0);
	if (q6->wdog_irq < 0)
		return q6->wdog_irq;

	drv->loadable = !!pdata; /* No pdata = don't use PIL */
	if (drv->loadable) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		q6->base = devm_request_and_ioremap(&pdev->dev, res);
		if (!q6->base)
			return -ENOMEM;

		q6->vreg = devm_regulator_get(&pdev->dev, "core_vdd");
		if (IS_ERR(q6->vreg))
			return PTR_ERR(q6->vreg);

		q6->xo = devm_clk_get(&pdev->dev, "xo");
		if (IS_ERR(q6->xo))
			return PTR_ERR(q6->xo);

		desc->name = pdata->name;
		desc->dev = &pdev->dev;
		desc->owner = THIS_MODULE;
		desc->proxy_timeout = 10000;
		pil_q6v4_init(q6, pdata);

		if (pas_supported(pdata->pas_id) > 0) {
			desc->ops = &pil_q6v4_lpass_ops_trusted;
			dev_info(&pdev->dev, "using secure boot\n");
		} else {
			desc->ops = &pil_q6v4_lpass_ops;
			dev_info(&pdev->dev, "using non-secure boot\n");
		}

		ret = pil_desc_init(desc);
		if (ret)
			return ret;
	}

	drv->subsys_desc.name = "adsp";
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.shutdown = lpass_shutdown;
	drv->subsys_desc.powerup = lpass_powerup;
	drv->subsys_desc.ramdump = lpass_ramdump;
	drv->subsys_desc.crash_shutdown = lpass_crash_shutdown;

	INIT_WORK(&drv->work, lpass_fatal_fn);

	drv->ramdump_dev = create_ramdump_device("lpass", &pdev->dev);
	if (!drv->ramdump_dev) {
		ret = -ENOMEM;
		goto err_ramdump;
	}

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		ret = PTR_ERR(drv->subsys);
		goto err_subsys;
	}
	if (!drv->loadable)
		subsys_default_online(drv->subsys);

	ret = devm_request_irq(&pdev->dev, q6->wdog_irq, lpass_wdog_bite_irq,
			IRQF_TRIGGER_RISING, dev_name(&pdev->dev), drv);
	if (ret)
		goto err_irq;
	disable_irq(q6->wdog_irq);

	ret = smsm_state_cb_register(SMSM_Q6_STATE, SMSM_RESET,
			lpass_smsm_state_cb, drv);
	if (ret < 0)
		goto err_smsm;

	drv->riva_notif_hdle = subsys_notif_register_notifier("riva", &rnb);
	if (IS_ERR(drv->riva_notif_hdle)) {
		ret = PTR_ERR(drv->riva_notif_hdle);
		goto err_notif_riva;
	}

	scm_pas_init(MSM_BUS_MASTER_SPS);

	drv->modem_notif_hdle = subsys_notif_register_notifier("modem", &mnb);
	if (IS_ERR(drv->modem_notif_hdle)) {
		ret = PTR_ERR(drv->modem_notif_hdle);
		goto err_notif_modem;
	}
	return 0;
err_notif_modem:
	subsys_notif_unregister_notifier(drv->riva_notif_hdle, &rnb);
err_notif_riva:
	smsm_state_cb_deregister(SMSM_Q6_STATE, SMSM_RESET,
			lpass_smsm_state_cb, drv);
err_smsm:
err_irq:
	subsys_unregister(drv->subsys);
err_subsys:
	destroy_ramdump_device(drv->ramdump_dev);
err_ramdump:
	if (drv->loadable)
		pil_desc_release(desc);
	return ret;
}

static int pil_q6v4_lpass_driver_exit(struct platform_device *pdev)
{
	struct lpass_q6v4 *drv = platform_get_drvdata(pdev);
	subsys_notif_unregister_notifier(drv->riva_notif_hdle, &rnb);
	subsys_notif_unregister_notifier(drv->modem_notif_hdle, &mnb);
	smsm_state_cb_deregister(SMSM_Q6_STATE, SMSM_RESET,
			lpass_smsm_state_cb, drv);
	subsys_unregister(drv->subsys);
	destroy_ramdump_device(drv->ramdump_dev);
	if (drv->loadable)
		pil_desc_release(&drv->q6.desc);
	return 0;
}

static struct platform_driver pil_q6v4_lpass_driver = {
	.probe = pil_q6v4_lpass_driver_probe,
	.remove = pil_q6v4_lpass_driver_exit,
	.driver = {
		.name = "pil-q6v4-lpass",
		.owner = THIS_MODULE,
	},
};

static int __init pil_q6v4_lpass_init(void)
{
	return platform_driver_register(&pil_q6v4_lpass_driver);
}
module_init(pil_q6v4_lpass_init);

static void __exit pil_q6v4_lpass_exit(void)
{
	platform_driver_unregister(&pil_q6v4_lpass_driver);
}
module_exit(pil_q6v4_lpass_exit);

MODULE_DESCRIPTION("Support for booting QDSP6v4 (Hexagon) processors");
MODULE_LICENSE("GPL v2");
