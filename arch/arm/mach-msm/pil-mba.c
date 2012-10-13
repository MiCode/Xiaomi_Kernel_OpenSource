/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/elf.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/interrupt.h>

#include <mach/subsystem_restart.h>
#include <mach/msm_smsm.h>
#include <mach/peripheral-loader.h>

#include "peripheral-loader.h"
#include "ramdump.h"

#define RMB_MBA_COMMAND			0x08
#define RMB_MBA_STATUS			0x0C
#define RMB_PMI_META_DATA		0x10
#define RMB_PMI_CODE_START		0x14
#define RMB_PMI_CODE_LENGTH		0x18

#define CMD_META_DATA_READY		0x1
#define CMD_LOAD_READY			0x2

#define STATUS_META_DATA_AUTH_SUCCESS	0x3
#define STATUS_AUTH_COMPLETE		0x4

#define PROXY_TIMEOUT_MS		10000
#define POLL_INTERVAL_US		50

#define MAX_SSR_REASON_LEN 81U

static int modem_auth_timeout_ms = 10000;
module_param(modem_auth_timeout_ms, int, S_IRUGO | S_IWUSR);

struct mba_data {
	void __iomem *reg_base;
	void __iomem *metadata_base;
	unsigned long metadata_phys;
	struct pil_device *pil;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	struct clk *xo;
	void *ramdump_dev;
	void *smem_ramdump_dev;
	bool crash_shutdown;
	bool ignore_errors;
	u32 img_length;
};

static int pil_mba_make_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct mba_data *drv = dev_get_drvdata(pil->dev);

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		return ret;
	}
	return 0;
}

static void pil_mba_remove_proxy_votes(struct pil_desc *pil)
{
	struct mba_data *drv = dev_get_drvdata(pil->dev);
	clk_disable_unprepare(drv->xo);
}

static int pil_mba_init_image(struct pil_desc *pil,
			      const u8 *metadata, size_t size)
{
	struct mba_data *drv = dev_get_drvdata(pil->dev);
	s32 status;
	int ret;

	/* Copy metadata to assigned shared buffer location */
	memcpy(drv->metadata_base, metadata, size);

	/* Initialize length counter to 0 */
	writel_relaxed(0, drv->reg_base + RMB_PMI_CODE_LENGTH);
	drv->img_length = 0;

	/* Pass address of meta-data to the MBA and perform authentication */
	writel_relaxed(drv->metadata_phys, drv->reg_base + RMB_PMI_META_DATA);
	writel_relaxed(CMD_META_DATA_READY, drv->reg_base + RMB_MBA_COMMAND);
	ret = readl_poll_timeout(drv->reg_base + RMB_MBA_STATUS, status,
		status == STATUS_META_DATA_AUTH_SUCCESS || status < 0,
		POLL_INTERVAL_US, modem_auth_timeout_ms * 1000);
	if (ret) {
		dev_err(pil->dev, "MBA authentication timed out\n");
	} else if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d\n", status);
		ret = -EINVAL;
	}

	return ret;
}

static int pil_mba_verify_blob(struct pil_desc *pil, u32 phy_addr,
			       size_t size)
{
	struct mba_data *drv = dev_get_drvdata(pil->dev);
	s32 status;

	/* Begin image authentication */
	if (drv->img_length == 0) {
		writel_relaxed(phy_addr, drv->reg_base + RMB_PMI_CODE_START);
		writel_relaxed(CMD_LOAD_READY, drv->reg_base + RMB_MBA_COMMAND);
	}
	/* Increment length counter */
	drv->img_length += size;
	writel_relaxed(drv->img_length, drv->reg_base + RMB_PMI_CODE_LENGTH);

	status = readl_relaxed(drv->reg_base + RMB_MBA_STATUS);
	if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d\n", status);
		return -EINVAL;
	}

	return 0;
}

static int pil_mba_auth(struct pil_desc *pil)
{
	struct mba_data *drv = dev_get_drvdata(pil->dev);
	int ret;
	s32 status;

	/* Wait for all segments to be authenticated or an error to occur */
	ret = readl_poll_timeout(drv->reg_base + RMB_MBA_STATUS, status,
			status == STATUS_AUTH_COMPLETE || status < 0,
			50, modem_auth_timeout_ms * 1000);
	if (ret) {
		dev_err(pil->dev, "MBA authentication timed out\n");
	} else if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d\n", status);
		ret = -EINVAL;
	}

	return ret;
}

static int pil_mba_shutdown(struct pil_desc *pil)
{
	return 0;
}

static struct pil_reset_ops pil_mba_ops = {
	.init_image = pil_mba_init_image,
	.proxy_vote = pil_mba_make_proxy_votes,
	.proxy_unvote = pil_mba_remove_proxy_votes,
	.verify_blob = pil_mba_verify_blob,
	.auth_and_reset = pil_mba_auth,
	.shutdown = pil_mba_shutdown,
};

#define subsys_to_drv(d) container_of(d, struct mba_data, subsys_desc)

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

static void restart_modem(struct mba_data *drv)
{
	log_modem_sfr();
	drv->ignore_errors = true;
	subsystem_restart_dev(drv->subsys);
}

static void smsm_state_cb(void *data, uint32_t old_state, uint32_t new_state)
{
	struct mba_data *drv = data;

	/* Ignore if we're the one that set SMSM_RESET */
	if (drv->crash_shutdown)
		return;

	if (new_state & SMSM_RESET) {
		pr_err("Probable fatal error on the modem.\n");
		restart_modem(drv);
	}
}

static int modem_shutdown(const struct subsys_desc *subsys)
{
	pil_force_shutdown("modem");
	pil_force_shutdown("mba");
	return 0;
}

static int modem_powerup(const struct subsys_desc *subsys)
{
	struct mba_data *drv = subsys_to_drv(subsys);
	/*
	 * At this time, the modem is shutdown. Therefore this function cannot
	 * run concurrently with either the watchdog bite error handler or the
	 * SMSM callback, making it safe to unset the flag below.
	 */
	drv->ignore_errors = 0;
	pil_force_boot("mba");
	pil_force_boot("modem");
	return 0;
}

static void modem_crash_shutdown(const struct subsys_desc *subsys)
{
	struct mba_data *drv = subsys_to_drv(subsys);
	drv->crash_shutdown = true;
	smsm_reset_modem(SMSM_RESET);
}

static struct ramdump_segment modem_segments[] = {
	{0x08400000, 0x0D100000 - 0x08400000},
};

static struct ramdump_segment smem_segments[] = {
	{0x0FA00000, 0x0FC00000 - 0x0FA00000},
};

static int modem_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct mba_data *drv = subsys_to_drv(subsys);
	int ret;

	if (!enable)
		return 0;

	pil_force_boot("mba");

	ret = do_ramdump(drv->ramdump_dev, modem_segments,
				ARRAY_SIZE(modem_segments));
	if (ret < 0) {
		pr_err("Unable to dump modem fw memory (rc = %d).\n", ret);
		goto out;
	}

	ret = do_ramdump(drv->smem_ramdump_dev, smem_segments,
		ARRAY_SIZE(smem_segments));
	if (ret < 0) {
		pr_err("Unable to dump smem memory (rc = %d).\n", ret);
		goto out;
	}

out:
	pil_force_shutdown("mba");
	return ret;
}

static irqreturn_t modem_wdog_bite_irq(int irq, void *dev_id)
{
	struct mba_data *drv = dev_id;
	if (drv->ignore_errors)
		return IRQ_HANDLED;
	pr_err("Watchdog bite received from modem software!\n");
	restart_modem(drv);
	return IRQ_HANDLED;
}

static int __devinit pil_mba_driver_probe(struct platform_device *pdev)
{
	struct mba_data *drv;
	struct resource *res;
	struct pil_desc *desc;
	int ret, irq;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rmb_base");
	if (!res)
		return -EINVAL;
	drv->reg_base = devm_ioremap(&pdev->dev, res->start,
				     resource_size(res));
	if (!drv->reg_base)
		return -ENOMEM;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					    "metadata_base");
	if (res) {
		drv->metadata_base = devm_ioremap(&pdev->dev, res->start,
						  resource_size(res));
		if (!drv->metadata_base)
			return -ENOMEM;
		drv->metadata_phys = res->start;
	}

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;

	ret = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
				      &desc->name);
	if (ret)
		return ret;

	of_property_read_string(pdev->dev.of_node, "qcom,depends-on",
				      &desc->depends_on);

	drv->xo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return PTR_ERR(drv->xo);

	desc->dev = &pdev->dev;
	desc->ops = &pil_mba_ops;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = PROXY_TIMEOUT_MS;

	drv->pil = msm_pil_register(desc);
	if (IS_ERR(drv->pil))
		return PTR_ERR(drv->pil);

	drv->subsys_desc.name = desc->name;
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.shutdown = modem_shutdown;
	drv->subsys_desc.powerup = modem_powerup;
	drv->subsys_desc.ramdump = modem_ramdump;
	drv->subsys_desc.crash_shutdown = modem_crash_shutdown;

	drv->ramdump_dev = create_ramdump_device("modem");
	if (!drv->ramdump_dev) {
		pr_err("%s: Unable to create a modem ramdump device.\n",
			__func__);
		ret = -ENOMEM;
		goto err_ramdump;
	}

	drv->smem_ramdump_dev = create_ramdump_device("smem-modem");
	if (!drv->smem_ramdump_dev) {
		pr_err("%s: Unable to create an smem ramdump device.\n",
			__func__);
		ret = -ENOMEM;
		goto err_ramdump_smem;
	}

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		goto err_subsys;
		ret = PTR_ERR(drv->subsys);
	}

	ret = devm_request_irq(&pdev->dev, irq, modem_wdog_bite_irq,
				IRQF_TRIGGER_RISING, "modem_wdog", drv);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to request watchdog IRQ.\n");
		goto err_irq;
	}

	ret = smsm_state_cb_register(SMSM_MODEM_STATE, SMSM_RESET,
		smsm_state_cb, drv);
	if (ret < 0) {
		dev_err(&pdev->dev, "Unable to register SMSM callback!\n");
		goto err_irq;
	}

	return 0;

err_irq:
	subsys_unregister(drv->subsys);
err_subsys:
	destroy_ramdump_device(drv->smem_ramdump_dev);
err_ramdump_smem:
	destroy_ramdump_device(drv->ramdump_dev);
err_ramdump:
	msm_pil_unregister(drv->pil);
	return ret;
}

static int __devexit pil_mba_driver_exit(struct platform_device *pdev)
{
	struct mba_data *drv = platform_get_drvdata(pdev);
	smsm_state_cb_deregister(SMSM_MODEM_STATE, SMSM_RESET,
			smsm_state_cb, drv);
	subsys_unregister(drv->subsys);
	destroy_ramdump_device(drv->smem_ramdump_dev);
	destroy_ramdump_device(drv->ramdump_dev);
	msm_pil_unregister(drv->pil);
	return 0;
}

static struct of_device_id mba_match_table[] = {
	{ .compatible = "qcom,pil-mba" },
	{}
};

struct platform_driver pil_mba_driver = {
	.probe = pil_mba_driver_probe,
	.remove = __devexit_p(pil_mba_driver_exit),
	.driver = {
		.name = "pil-mba",
		.of_match_table = mba_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init pil_mba_init(void)
{
	return platform_driver_register(&pil_mba_driver);
}
module_init(pil_mba_init);

static void __exit pil_mba_exit(void)
{
	platform_driver_unregister(&pil_mba_driver);
}
module_exit(pil_mba_exit);

MODULE_DESCRIPTION("Support for modem boot using the Modem Boot Authenticator");
MODULE_LICENSE("GPL v2");
