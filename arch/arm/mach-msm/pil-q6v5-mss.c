/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/of_gpio.h>

#include <mach/subsystem_restart.h>
#include <mach/clk.h>
#include <mach/msm_smsm.h>
#include <mach/ramdump.h>
#include <mach/msm_smem.h>

#include "peripheral-loader.h"
#include "pil-q6v5.h"
#include "pil-msa.h"
#include "sysmon.h"

#define MAX_VDD_MSS_UV		1150000
#define PROXY_TIMEOUT_MS	10000
#define MAX_SSR_REASON_LEN	81U
#define STOP_ACK_TIMEOUT_MS	1000

struct modem_data {
	struct mba_data *mba;
	struct q6v5_data *q6;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	void *adsp_state_notifier;
	void *ramdump_dev;
	bool crash_shutdown;
	bool ignore_errors;
	struct completion stop_ack;
};

#define subsys_to_drv(d) container_of(d, struct modem_data, subsys_desc)

static void log_modem_sfr(void)
{
	u32 size;
	char *smem_reason, reason[MAX_SSR_REASON_LEN];

	smem_reason = smem_get_entry_no_rlock(SMEM_SSR_REASON_MSS0, &size);
	if (!smem_reason || !size) {
		pr_err("modem subsystem failure reason: (unknown, smem_get_entry_no_rlock failed).\n");
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

static void restart_modem(struct modem_data *drv)
{
	log_modem_sfr();
	drv->ignore_errors = true;
	subsystem_restart_dev(drv->subsys);
}

static irqreturn_t modem_err_fatal_intr_handler(int irq, void *dev_id)
{
	struct modem_data *drv = subsys_to_drv(dev_id);

	/* Ignore if we're the one that set the force stop GPIO */
	if (drv->crash_shutdown)
		return IRQ_HANDLED;

	pr_err("Fatal error on the modem.\n");
	subsys_set_crash_status(drv->subsys, true);
	restart_modem(drv);
	return IRQ_HANDLED;
}

static irqreturn_t modem_stop_ack_intr_handler(int irq, void *dev_id)
{
	struct modem_data *drv = subsys_to_drv(dev_id);
	pr_info("Received stop ack interrupt from modem\n");
	complete(&drv->stop_ack);
	return IRQ_HANDLED;
}

static int modem_shutdown(const struct subsys_desc *subsys)
{
	struct modem_data *drv = subsys_to_drv(subsys);
	unsigned long ret;

	if (subsys->is_not_loadable)
		return 0;

	if (!subsys_get_crash_status(drv->subsys)) {
		gpio_set_value(subsys->force_stop_gpio, 1);
		ret = wait_for_completion_timeout(&drv->stop_ack,
				msecs_to_jiffies(STOP_ACK_TIMEOUT_MS));
		if (!ret)
			pr_warn("Timed out on stop ack from modem.\n");
		gpio_set_value(subsys->force_stop_gpio, 0);
	}

	pil_shutdown(&drv->mba->desc);
	pil_shutdown(&drv->q6->desc);
	return 0;
}

static int modem_powerup(const struct subsys_desc *subsys)
{
	struct modem_data *drv = subsys_to_drv(subsys);
	int ret;

	if (subsys->is_not_loadable)
		return 0;
	/*
	 * At this time, the modem is shutdown. Therefore this function cannot
	 * run concurrently with either the watchdog bite error handler or the
	 * SMSM callback, making it safe to unset the flag below.
	 */
	INIT_COMPLETION(drv->stop_ack);
	drv->ignore_errors = false;
	ret = pil_boot(&drv->q6->desc);
	if (ret)
		return ret;
	ret = pil_boot(&drv->mba->desc);
	if (ret)
		pil_shutdown(&drv->q6->desc);
	return ret;
}

static void modem_crash_shutdown(const struct subsys_desc *subsys)
{
	struct modem_data *drv = subsys_to_drv(subsys);
	drv->crash_shutdown = true;
	if (!subsys_get_crash_status(drv->subsys)) {
		gpio_set_value(subsys->force_stop_gpio, 1);
		mdelay(STOP_ACK_TIMEOUT_MS);
	}
}

static int modem_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct modem_data *drv = subsys_to_drv(subsys);
	int ret;

	if (!enable)
		return 0;

	ret = pil_boot(&drv->q6->desc);
	if (ret)
		return ret;

	ret = pil_do_ramdump(&drv->mba->desc, drv->ramdump_dev);
	if (ret < 0)
		pr_err("Unable to dump modem fw memory (rc = %d).\n", ret);

	pil_shutdown(&drv->q6->desc);
	return ret;
}

static int adsp_state_notifier_fn(struct notifier_block *this,
				unsigned long code, void *ss_handle)
{
	int ret;
	ret = sysmon_send_event(SYSMON_SS_MODEM, "adsp", code);
	if (ret < 0)
		pr_err("%s: sysmon_send_event failed (%d).", __func__, ret);
	return NOTIFY_DONE;
}

static struct notifier_block adsp_state_notifier_block = {
	.notifier_call = adsp_state_notifier_fn,
};

static irqreturn_t modem_wdog_bite_intr_handler(int irq, void *dev_id)
{
	struct modem_data *drv = subsys_to_drv(dev_id);
	if (drv->ignore_errors)
		return IRQ_HANDLED;
	pr_err("Watchdog bite received from modem software!\n");
	subsys_set_crash_status(drv->subsys, true);
	restart_modem(drv);
	return IRQ_HANDLED;
}

static int mss_start(const struct subsys_desc *desc)
{
	int ret;
	struct modem_data *drv = subsys_to_drv(desc);

	if (desc->is_not_loadable)
		return 0;

	INIT_COMPLETION(drv->stop_ack);
	ret = pil_boot(&drv->q6->desc);
	if (ret)
		return ret;
	ret = pil_boot(&drv->mba->desc);
	if (ret)
		pil_shutdown(&drv->q6->desc);
	return ret;
}

static void mss_stop(const struct subsys_desc *desc)
{
	struct modem_data *drv = subsys_to_drv(desc);

	if (desc->is_not_loadable)
		return;

	pil_shutdown(&drv->mba->desc);
	pil_shutdown(&drv->q6->desc);
}

static int __devinit pil_subsys_init(struct modem_data *drv,
					struct platform_device *pdev)
{
	int ret;

	drv->subsys_desc.name = "modem";
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.shutdown = modem_shutdown;
	drv->subsys_desc.powerup = modem_powerup;
	drv->subsys_desc.ramdump = modem_ramdump;
	drv->subsys_desc.crash_shutdown = modem_crash_shutdown;
	drv->subsys_desc.start = mss_start;
	drv->subsys_desc.stop = mss_stop;
	drv->subsys_desc.err_fatal_handler = modem_err_fatal_intr_handler;
	drv->subsys_desc.stop_ack_handler = modem_stop_ack_intr_handler;
	drv->subsys_desc.wdog_bite_handler = modem_wdog_bite_intr_handler;

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		ret = PTR_ERR(drv->subsys);
		goto err_subsys;
	}

	drv->ramdump_dev = create_ramdump_device("modem", &pdev->dev);
	if (!drv->ramdump_dev) {
		pr_err("%s: Unable to create a modem ramdump device.\n",
			__func__);
		ret = -ENOMEM;
		goto err_ramdump;
	}

	drv->adsp_state_notifier = subsys_notif_register_notifier("adsp",
						&adsp_state_notifier_block);
	if (IS_ERR(drv->adsp_state_notifier)) {
		ret = PTR_ERR(drv->adsp_state_notifier);
		dev_err(&pdev->dev, "%s: Registration with the SSR notification driver failed (%d)",
			__func__, ret);
		goto err_irq;
	}

	return 0;

err_irq:
	destroy_ramdump_device(drv->ramdump_dev);
err_ramdump:
	subsys_unregister(drv->subsys);
err_subsys:
	return ret;
}

static int __devinit pil_mss_loadable_init(struct modem_data *drv,
					struct platform_device *pdev)
{
	struct q6v5_data *q6;
	struct mba_data *mba;
	struct pil_desc *q6_desc, *mba_desc;
	struct resource *res;
	struct property *prop;
	int ret;

	mba = devm_kzalloc(&pdev->dev, sizeof(*mba), GFP_KERNEL);
	if (!mba)
		return -ENOMEM;
	drv->mba = mba;

	q6 = pil_q6v5_init(pdev);
	if (IS_ERR(q6))
		return PTR_ERR(q6);
	drv->q6 = q6;
	drv->mba->xo = q6->xo;

	q6_desc = &q6->desc;
	q6_desc->ops = &pil_msa_pbl_ops;
	q6_desc->owner = THIS_MODULE;
	q6_desc->proxy_timeout = PROXY_TIMEOUT_MS;

	q6->self_auth = of_property_read_bool(pdev->dev.of_node,
							"qcom,pil-self-auth");
	if (q6->self_auth) {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						    "rmb_base");
		q6->rmb_base = devm_request_and_ioremap(&pdev->dev, res);
		if (!q6->rmb_base)
			return -ENOMEM;
		mba->rmb_base = q6->rmb_base;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "restart_reg");
	q6->restart_reg = devm_request_and_ioremap(&pdev->dev, res);
	if (!q6->restart_reg)
		return -ENOMEM;

	q6->vreg = NULL;

	prop = of_find_property(pdev->dev.of_node, "vdd_mss-supply", NULL);
	if (prop) {
		q6->vreg = devm_regulator_get(&pdev->dev, "vdd_mss");
		if (IS_ERR(q6->vreg))
			return PTR_ERR(q6->vreg);

		ret = regulator_set_voltage(q6->vreg, VDD_MSS_UV,
						MAX_VDD_MSS_UV);
		if (ret)
			dev_err(&pdev->dev, "Failed to set vreg voltage.\n");

		ret = regulator_set_optimum_mode(q6->vreg, 100000);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to set vreg mode.\n");
			return ret;
		}
	}

	q6->vreg_mx = devm_regulator_get(&pdev->dev, "vdd_mx");
	if (IS_ERR(q6->vreg_mx))
		return PTR_ERR(q6->vreg_mx);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
		"cxrail_bhs_reg");
	if (res)
		q6->cxrail_bhs = devm_ioremap(&pdev->dev, res->start,
					  resource_size(res));

	q6->ahb_clk = devm_clk_get(&pdev->dev, "iface_clk");
	if (IS_ERR(q6->ahb_clk))
		return PTR_ERR(q6->ahb_clk);

	q6->axi_clk = devm_clk_get(&pdev->dev, "bus_clk");
	if (IS_ERR(q6->axi_clk))
		return PTR_ERR(q6->axi_clk);

	q6->rom_clk = devm_clk_get(&pdev->dev, "mem_clk");
	if (IS_ERR(q6->rom_clk))
		return PTR_ERR(q6->rom_clk);

	ret = pil_desc_init(q6_desc);
	if (ret)
		return ret;

	mba_desc = &mba->desc;
	mba_desc->name = "modem";
	mba_desc->dev = &pdev->dev;
	mba_desc->ops = &pil_msa_mba_ops;
	mba_desc->owner = THIS_MODULE;

	ret = pil_desc_init(mba_desc);
	if (ret)
		goto err_mba_desc;

	return 0;

err_mba_desc:
	pil_desc_release(q6_desc);
	return ret;

}

static int __devinit pil_mss_driver_probe(struct platform_device *pdev)
{
	struct modem_data *drv;
	int ret, is_not_loadable;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	is_not_loadable = of_property_read_bool(pdev->dev.of_node,
							"qcom,is-not-loadable");
	if (is_not_loadable) {
		drv->subsys_desc.is_not_loadable = 1;
	} else {
		ret = pil_mss_loadable_init(drv, pdev);
		if (ret)
			return ret;
	}
	init_completion(&drv->stop_ack);

	return pil_subsys_init(drv, pdev);
}

static int __devexit pil_mss_driver_exit(struct platform_device *pdev)
{
	struct modem_data *drv = platform_get_drvdata(pdev);

	subsys_notif_unregister_notifier(drv->adsp_state_notifier,
						&adsp_state_notifier_block);
	subsys_unregister(drv->subsys);
	destroy_ramdump_device(drv->ramdump_dev);
	pil_desc_release(&drv->mba->desc);
	pil_desc_release(&drv->q6->desc);
	return 0;
}

static struct of_device_id mss_match_table[] = {
	{ .compatible = "qcom,pil-q6v5-mss" },
	{}
};

static struct platform_driver pil_mss_driver = {
	.probe = pil_mss_driver_probe,
	.remove = __devexit_p(pil_mss_driver_exit),
	.driver = {
		.name = "pil-q6v5-mss",
		.of_match_table = mss_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init pil_mss_init(void)
{
	return platform_driver_register(&pil_mss_driver);
}
module_init(pil_mss_init);

static void __exit pil_mss_exit(void)
{
	platform_driver_unregister(&pil_mss_driver);
}
module_exit(pil_mss_exit);

MODULE_DESCRIPTION("Support for booting modem subsystems with QDSP6v5 Hexagon processors");
MODULE_LICENSE("GPL v2");
