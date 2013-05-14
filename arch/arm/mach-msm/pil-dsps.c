/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/atomic.h>
#include <linux/interrupt.h>

#include <mach/subsystem_restart.h>
#include <mach/msm_smsm.h>
#include <mach/ramdump.h>
#include <mach/msm_smem.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

#define PPSS_RESET			0x2594
#define PPSS_RESET_PROC_RESET		0x2
#define PPSS_RESET_RESET		0x1
#define PPSS_PROC_CLK_CTL		0x2588
#define CLK_BRANCH_ENA			0x10
#define PPSS_HCLK_CTL			0x2580
#define CLK_HALT_DFAB_STATE		0x2FC8

#define PPSS_WDOG_UNMASKED_INT_EN	0x1808

struct dsps_data {
	void __iomem *base;
	struct pil_desc desc;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	int crash;
	int wdog_irq;
	atomic_t wd_crash;
	atomic_t crash_in_progress;
	void __iomem *ppss_base;

	void *ramdump_dev;

	void *smem_ramdump_dev;
	struct ramdump_segment smem_ramdump_segments[1];
};

#define desc_to_drv(d) container_of(d, struct dsps_data, subsys_desc)
#define pil_to_drv(d) container_of(d, struct dsps_data, desc)

static int init_image_dsps(struct pil_desc *pil, const u8 *metadata,
				     size_t size)
{
	struct dsps_data *drv = pil_to_drv(pil);

	/* Bring memory and bus interface out of reset */
	writel_relaxed(PPSS_RESET_PROC_RESET, drv->base + PPSS_RESET);
	writel_relaxed(CLK_BRANCH_ENA, drv->base + PPSS_HCLK_CTL);
	mb();
	return 0;
}

static int reset_dsps(struct pil_desc *pil)
{
	struct dsps_data *drv = pil_to_drv(pil);

	writel_relaxed(CLK_BRANCH_ENA, drv->base + PPSS_PROC_CLK_CTL);
	while (readl_relaxed(drv->base + CLK_HALT_DFAB_STATE) & BIT(18))
		cpu_relax();
	/* Bring DSPS out of reset */
	writel_relaxed(0x0, drv->base + PPSS_RESET);
	return 0;
}

static int shutdown_dsps(struct pil_desc *pil)
{
	struct dsps_data *drv = pil_to_drv(pil);

	writel_relaxed(PPSS_RESET_PROC_RESET | PPSS_RESET_RESET,
			drv->base + PPSS_RESET);
	usleep_range(1000, 2000);
	writel_relaxed(PPSS_RESET_PROC_RESET, drv->base + PPSS_RESET);
	writel_relaxed(0x0, drv->base + PPSS_PROC_CLK_CTL);
	return 0;
}

struct pil_reset_ops pil_dsps_ops = {
	.init_image = init_image_dsps,
	.auth_and_reset = reset_dsps,
	.shutdown = shutdown_dsps,
};

static int init_image_dsps_trusted(struct pil_desc *pil, const u8 *metadata,
				   size_t size)
{
	return pas_init_image(PAS_DSPS, metadata, size);
}

static int reset_dsps_trusted(struct pil_desc *pil)
{
	return pas_auth_and_reset(PAS_DSPS);
}

static int shutdown_dsps_trusted(struct pil_desc *pil)
{
	return pas_shutdown(PAS_DSPS);
}

struct pil_reset_ops pil_dsps_ops_trusted = {
	.init_image = init_image_dsps_trusted,
	.auth_and_reset = reset_dsps_trusted,
	.shutdown = shutdown_dsps_trusted,
};

static void dsps_log_sfr(void)
{
	const char dflt_reason[] = "Died too early due to unknown reason";
	char *smem_reset_reason;
	unsigned smem_reset_size;

	smem_reset_reason = smem_get_entry(SMEM_SSR_REASON_DSPS0,
		&smem_reset_size);
	if (smem_reset_reason != NULL && smem_reset_reason[0] != 0) {
		smem_reset_reason[smem_reset_size-1] = 0;
		pr_err("%s: DSPS failure: %s\nResetting DSPS\n",
			__func__, smem_reset_reason);
		memset(smem_reset_reason, 0, smem_reset_size);
		wmb();
	} else
		pr_err("%s: DSPS failure: %s\nResetting DSPS\n",
			__func__, dflt_reason);
}


static void dsps_restart_handler(struct dsps_data *drv)
{
	if (atomic_add_return(1, &drv->crash_in_progress) > 1) {
		pr_err("%s: DSPS already resetting. Count %d\n", __func__,
		       atomic_read(&drv->crash_in_progress));
	} else {
		subsystem_restart_dev(drv->subsys);
	}
}

static void dsps_smsm_state_cb(void *data, uint32_t old_state,
			       uint32_t new_state)
{
	struct dsps_data *drv = data;

	if (drv->crash == 1) {
		pr_debug("SMSM_RESET state change ignored\n");
		drv->crash = 0;
	} else if (new_state & SMSM_RESET) {
		dsps_log_sfr();
		dsps_restart_handler(drv);
	}
}

static int dsps_start(const struct subsys_desc *desc)
{
	struct dsps_data *drv = desc_to_drv(desc);

	return pil_boot(&drv->desc);
}

static void dsps_stop(const struct subsys_desc *desc)
{
	struct dsps_data *drv = desc_to_drv(desc);
	pil_shutdown(&drv->desc);
}

static int dsps_shutdown(const struct subsys_desc *desc)
{
	struct dsps_data *drv = desc_to_drv(desc);
	disable_irq_nosync(drv->wdog_irq);
	if (drv->ppss_base) {
		writel_relaxed(0, drv->ppss_base + PPSS_WDOG_UNMASKED_INT_EN);
		mb(); /* Make sure wdog is disabled before shutting down */
	}
	pil_shutdown(&drv->desc);
	return 0;
}

static int dsps_powerup(const struct subsys_desc *desc)
{
	struct dsps_data *drv = desc_to_drv(desc);

	pil_boot(&drv->desc);
	atomic_set(&drv->crash_in_progress, 0);
	enable_irq(drv->wdog_irq);

	return 0;
}

static int dsps_ramdump(int enable, const struct subsys_desc *desc)
{
	int ret;
	struct dsps_data *drv = desc_to_drv(desc);

	if (!enable)
		return 0;

	ret = pil_do_ramdump(&drv->desc, drv->ramdump_dev);
	if (ret < 0) {
		pr_err("%s: Unable to dump DSPS memory (rc = %d).\n",
		       __func__, ret);
		return ret;
	}
	ret = do_elf_ramdump(drv->smem_ramdump_dev, drv->smem_ramdump_segments,
		ARRAY_SIZE(drv->smem_ramdump_segments));
	if (ret < 0) {
		pr_err("%s: Unable to dump smem memory (rc = %d).\n",
		       __func__, ret);
		return ret;
	}
	return 0;
}

static void dsps_crash_shutdown(const struct subsys_desc *desc)
{
	struct dsps_data *drv = desc_to_drv(desc);

	disable_irq_nosync(drv->wdog_irq);
	drv->crash = 1;
	smsm_change_state(SMSM_DSPS_STATE, SMSM_RESET, SMSM_RESET);
}

static irqreturn_t dsps_wdog_bite_irq(int irq, void *dev_id)
{
	struct dsps_data *drv = dev_id;

	atomic_set(&drv->wd_crash, 1);
	dsps_log_sfr();
	dsps_restart_handler(drv);
	return IRQ_HANDLED;
}

static int __devinit pil_dsps_driver_probe(struct platform_device *pdev)
{
	struct dsps_data *drv;
	struct pil_desc *desc;
	struct resource *res;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;
	drv->base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!drv->base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (res) {
		drv->ppss_base = devm_ioremap(&pdev->dev, res->start,
					      resource_size(res));
		if (!drv->ppss_base)
			return -ENOMEM;
	}

	desc = &drv->desc;
	desc->name = pdev->dev.platform_data;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->flags = PIL_SKIP_ENTRY_CHECK;
	if (pas_supported(PAS_DSPS) > 0) {
		desc->ops = &pil_dsps_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_dsps_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}
	ret = pil_desc_init(desc);
	if (ret)
		return ret;

	drv->ramdump_dev = create_ramdump_device("dsps", &pdev->dev);
	if (!drv->ramdump_dev) {
		ret = -ENOMEM;
		goto err_ramdump;
	}

	drv->smem_ramdump_segments[0].address = PHYS_OFFSET - SZ_2M;
	drv->smem_ramdump_segments[0].size =  SZ_2M;
	drv->smem_ramdump_dev = create_ramdump_device("smem-dsps", &pdev->dev);
	if (!drv->smem_ramdump_dev) {
		ret = -ENOMEM;
		goto err_smem_ramdump;
	}

	drv->subsys_desc.name = "dsps";
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.start = dsps_start;
	drv->subsys_desc.stop = dsps_stop;
	drv->subsys_desc.shutdown = dsps_shutdown;
	drv->subsys_desc.powerup = dsps_powerup;
	drv->subsys_desc.ramdump = dsps_ramdump,
	drv->subsys_desc.crash_shutdown = dsps_crash_shutdown;

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		ret = PTR_ERR(drv->subsys);
		goto err_subsys;
	}

	ret = smsm_state_cb_register(SMSM_DSPS_STATE, SMSM_RESET,
				     dsps_smsm_state_cb, drv);
	if (ret)
		goto err_smsm;

	drv->wdog_irq = platform_get_irq(pdev, 0);
	if (drv->wdog_irq >= 0) {
		ret = devm_request_irq(&pdev->dev, drv->wdog_irq,
				dsps_wdog_bite_irq, IRQF_TRIGGER_RISING,
				"dsps_wdog", drv);
		if (ret) {
			dev_err(&pdev->dev, "request_irq failed\n");
			goto err_smsm;
		}
	} else {
		drv->wdog_irq = -1;
		dev_dbg(&pdev->dev, "ppss_wdog not supported\n");
	}

	return 0;

err_smsm:
	subsys_unregister(drv->subsys);
err_subsys:
	destroy_ramdump_device(drv->smem_ramdump_dev);
err_smem_ramdump:
	destroy_ramdump_device(drv->ramdump_dev);
err_ramdump:
	pil_desc_release(desc);
	return ret;
}

static int __devexit pil_dsps_driver_exit(struct platform_device *pdev)
{
	struct dsps_data *drv = platform_get_drvdata(pdev);
	smsm_state_cb_deregister(SMSM_DSPS_STATE, SMSM_RESET,
				 dsps_smsm_state_cb, drv);
	subsys_unregister(drv->subsys);
	destroy_ramdump_device(drv->smem_ramdump_dev);
	destroy_ramdump_device(drv->ramdump_dev);
	pil_desc_release(&drv->desc);
	return 0;
}

static struct platform_driver pil_dsps_driver = {
	.probe = pil_dsps_driver_probe,
	.remove = __devexit_p(pil_dsps_driver_exit),
	.driver = {
		.name = "pil_dsps",
		.owner = THIS_MODULE,
	},
};

static int __init pil_dsps_init(void)
{
	return platform_driver_register(&pil_dsps_driver);
}
module_init(pil_dsps_init);

static void __exit pil_dsps_exit(void)
{
	platform_driver_unregister(&pil_dsps_driver);
}
module_exit(pil_dsps_exit);

MODULE_DESCRIPTION("Support for booting sensors (DSPS) images");
MODULE_LICENSE("GPL v2");
