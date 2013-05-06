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
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/interrupt.h>

#include <mach/subsystem_restart.h>
#include <mach/msm_smsm.h>
#include <mach/ramdump.h>
#include <mach/msm_smem.h>

#include "smd_private.h"
#include "peripheral-loader.h"
#include "pil-q6v4.h"
#include "scm-pas.h"

#define MSS_S_HCLK_CTL		0x2C70
#define MSS_SLP_CLK_CTL		0x2C60
#define SFAB_MSS_M_ACLK_CTL	0x2340
#define SFAB_MSS_S_HCLK_CTL	0x2C00
#define MSS_RESET		0x2C64

struct q6v4_modem {
	struct q6v4_data q6_fw;
	struct q6v4_data q6_sw;
	void __iomem *modem_base;
	void __iomem *cbase;
	void *fw_ramdump_dev;
	void *sw_ramdump_dev;
	void *smem_ramdump_dev;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	int crash_shutdown;
	int loadable;
	void *pil;
};

static DEFINE_MUTEX(pil_q6v4_modem_lock);
static unsigned pil_q6v4_modem_count;

/* Bring modem subsystem out of reset */
static void pil_q6v4_init_modem(void __iomem *base, void __iomem *cbase,
				void __iomem *jtag_clk)
{
	mutex_lock(&pil_q6v4_modem_lock);
	if (!pil_q6v4_modem_count) {
		/* Enable MSS clocks */
		writel_relaxed(0x10, cbase + SFAB_MSS_M_ACLK_CTL);
		writel_relaxed(0x10, cbase + SFAB_MSS_S_HCLK_CTL);
		writel_relaxed(0x10, cbase + MSS_S_HCLK_CTL);
		writel_relaxed(0x10, cbase + MSS_SLP_CLK_CTL);
		/* Wait for clocks to enable */
		mb();
		udelay(10);

		/* De-assert MSS reset */
		writel_relaxed(0x0, cbase + MSS_RESET);
		mb();
		udelay(10);
		/* Enable MSS */
		writel_relaxed(0x7, base);
	}

	/* Enable JTAG clocks */
	/* TODO: Remove if/when Q6 software enables them? */
	writel_relaxed(0x10, jtag_clk);

	pil_q6v4_modem_count++;
	mutex_unlock(&pil_q6v4_modem_lock);
}

/* Put modem subsystem back into reset */
static void pil_q6v4_shutdown_modem(struct q6v4_modem *mdm)
{
	mutex_lock(&pil_q6v4_modem_lock);
	if (pil_q6v4_modem_count)
		pil_q6v4_modem_count--;
	if (pil_q6v4_modem_count == 0)
		writel_relaxed(0x1, mdm->cbase + MSS_RESET);
	mutex_unlock(&pil_q6v4_modem_lock);
}

static int pil_q6v4_modem_boot(struct pil_desc *pil)
{
	struct q6v4_data *drv = pil_to_q6v4_data(pil);
	struct q6v4_modem *mdm = dev_get_drvdata(pil->dev);
	int err;

	err = pil_q6v4_power_up(drv);
	if (err)
		return err;

	pil_q6v4_init_modem(mdm->modem_base, mdm->cbase, drv->jtag_clk_reg);
	return pil_q6v4_boot(pil);
}

static int pil_q6v4_modem_shutdown(struct pil_desc *pil)
{
	struct q6v4_data *drv = pil_to_q6v4_data(pil);
	struct q6v4_modem *mdm = dev_get_drvdata(pil->dev);
	int ret;

	ret = pil_q6v4_shutdown(pil);
	if (ret)
		return ret;
	pil_q6v4_shutdown_modem(mdm);
	pil_q6v4_power_down(drv);
	return 0;
}

static struct pil_reset_ops pil_q6v4_modem_ops = {
	.auth_and_reset = pil_q6v4_modem_boot,
	.shutdown = pil_q6v4_modem_shutdown,
	.proxy_vote = pil_q6v4_make_proxy_votes,
	.proxy_unvote = pil_q6v4_remove_proxy_votes,
};

static struct pil_reset_ops pil_q6v4_modem_ops_trusted = {
	.init_image = pil_q6v4_init_image_trusted,
	.auth_and_reset = pil_q6v4_boot_trusted,
	.shutdown = pil_q6v4_shutdown_trusted,
	.proxy_vote = pil_q6v4_make_proxy_votes,
	.proxy_unvote = pil_q6v4_remove_proxy_votes,
};

static void log_modem_sfr(void)
{
	u32 size;
	char *smem_reason, reason[81];

	smem_reason = smem_get_entry(SMEM_SSR_REASON_MSS0, &size);
	if (!smem_reason || !size) {
		pr_err("modem subsystem failure reason: (unknown, smem_get_entry failed).\n");
		return;
	}
	if (!smem_reason[0]) {
		pr_err("modem subsystem failure reason: (unknown, init string found).\n");
		return;
	}

	size = min(size, sizeof(reason)-1);
	memcpy(reason, smem_reason, size);
	reason[size] = '\0';
	pr_err("modem subsystem failure reason: %s.\n", reason);

	smem_reason[0] = '\0';
	wmb();
}

static void restart_modem(struct q6v4_modem *drv)
{
	log_modem_sfr();
	subsystem_restart_dev(drv->subsys);
}

#define desc_to_modem(d) container_of(d, struct q6v4_modem, subsys_desc)

static int modem_start(const struct subsys_desc *desc)
{
	struct q6v4_modem *drv = desc_to_modem(desc);
	int ret = 0;

	if (drv->loadable) {
		ret = pil_boot(&drv->q6_fw.desc);
		if (ret)
			return ret;
		ret = pil_boot(&drv->q6_sw.desc);
		if (ret)
			pil_shutdown(&drv->q6_fw.desc);
	}
	return ret;
}

static void modem_stop(const struct subsys_desc *desc)
{
	struct q6v4_modem *drv = desc_to_modem(desc);
	if (drv->loadable) {
		pil_shutdown(&drv->q6_sw.desc);
		pil_shutdown(&drv->q6_fw.desc);
	}
}

static int modem_shutdown(const struct subsys_desc *subsys)
{
	struct q6v4_modem *drv = desc_to_modem(subsys);

	/* The watchdogs keep running even after the modem is shutdown */
	writel_relaxed(0x0, drv->q6_fw.wdog_base + 0x24);
	writel_relaxed(0x0, drv->q6_sw.wdog_base + 0x24);
	mb();

	if (drv->loadable) {
		pil_shutdown(&drv->q6_sw.desc);
		pil_shutdown(&drv->q6_fw.desc);
	}

	disable_irq_nosync(drv->q6_fw.wdog_irq);
	disable_irq_nosync(drv->q6_sw.wdog_irq);

	return 0;
}

static int modem_powerup(const struct subsys_desc *subsys)
{
	struct q6v4_modem *drv = desc_to_modem(subsys);
	int ret;

	if (drv->loadable) {
		ret = pil_boot(&drv->q6_fw.desc);
		if (ret)
			return ret;
		ret = pil_boot(&drv->q6_sw.desc);
		if (ret) {
			pil_shutdown(&drv->q6_fw.desc);
			return ret;
		}
	}
	enable_irq(drv->q6_fw.wdog_irq);
	enable_irq(drv->q6_sw.wdog_irq);
	return 0;
}

void modem_crash_shutdown(const struct subsys_desc *subsys)
{
	struct q6v4_modem *drv = desc_to_modem(subsys);

	drv->crash_shutdown = 1;
	smsm_reset_modem(SMSM_RESET);
}

static struct ramdump_segment smem_segments[] = {
	{0x80000000, 0x00200000},
};

static int modem_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct q6v4_modem *drv = desc_to_modem(subsys);
	int ret;

	if (!enable)
		return 0;

	ret = pil_do_ramdump(&drv->q6_sw.desc, drv->sw_ramdump_dev);
	if (ret < 0)
		return ret;

	ret = pil_do_ramdump(&drv->q6_fw.desc, drv->fw_ramdump_dev);
	if (ret < 0)
		return ret;

	ret = do_elf_ramdump(drv->smem_ramdump_dev, smem_segments,
		ARRAY_SIZE(smem_segments));
	if (ret < 0)
		return ret;

	return 0;
}

static void smsm_state_cb(void *data, uint32_t old_state, uint32_t new_state)
{
	struct q6v4_modem *drv = data;

	/* Ignore if we're the one that set SMSM_RESET */
	if (drv->crash_shutdown)
		return;

	if (new_state & SMSM_RESET) {
		pr_err("Probable fatal error on the modem.\n");
		restart_modem(drv);
	}
}

static irqreturn_t modem_wdog_bite_irq(int irq, void *dev_id)
{
	struct q6v4_modem *drv = dev_id;
	restart_modem(drv);
	return IRQ_HANDLED;
}

static int __devinit
pil_q6v4_proc_init(struct q6v4_data *drv, struct platform_device *pdev, int i)
{
	static const char *name[2] = { "fw", "sw" };
	const struct pil_q6v4_pdata *pdata_p = pdev->dev.platform_data;
	const struct pil_q6v4_pdata *pdata = pdata_p + i;
	char reg_name[12];
	struct pil_desc *desc;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2 + (i * 2));
	drv->base = devm_request_and_ioremap(&pdev->dev, res);
	if (!drv->base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 3 + (i * 2));
	drv->wdog_base = devm_request_and_ioremap(&pdev->dev, res);
	if (!drv->wdog_base)
		return -ENOMEM;

	snprintf(reg_name, sizeof(reg_name), "%s_core_vdd", name[i]);
	drv->vreg = devm_regulator_get(&pdev->dev, reg_name);
	if (IS_ERR(drv->vreg))
		return PTR_ERR(drv->vreg);

	drv->xo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return PTR_ERR(drv->xo);

	desc = &drv->desc;
	desc->name = pdata->name;
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = 10000;
	pil_q6v4_init(drv, pdata);

	if (pas_supported(pdata->pas_id) > 0) {
		desc->ops = &pil_q6v4_modem_ops_trusted;
		dev_info(&pdev->dev, "using secure boot for %s\n", name[i]);
	} else {
		desc->ops = &pil_q6v4_modem_ops;
		dev_info(&pdev->dev, "using non-secure boot for %s\n", name[i]);
	}
	return 0;
}

static int __devinit pil_q6v4_modem_driver_probe(struct platform_device *pdev)
{
	struct q6v4_data *drv_fw, *drv_sw;
	struct q6v4_modem *drv;
	struct resource *res;
	struct regulator *pll_supply;
	int ret;
	const struct pil_q6v4_pdata *pdata = pdev->dev.platform_data;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	drv_fw = &drv->q6_fw;
	drv_sw = &drv->q6_sw;

	drv_fw->wdog_irq = platform_get_irq(pdev, 0);
	if (drv_fw->wdog_irq < 0)
		return drv_fw->wdog_irq;

	drv_sw->wdog_irq = platform_get_irq(pdev, 1);
	if (drv_sw->wdog_irq < 0)
		return drv_sw->wdog_irq;

	drv->loadable = !!pdata; /* No pdata = don't use PIL */
	if (drv->loadable) {
		ret = pil_q6v4_proc_init(drv_fw, pdev, 0);
		if (ret)
			return ret;

		ret = pil_q6v4_proc_init(drv_sw, pdev, 1);
		if (ret)
			return ret;

		pll_supply = devm_regulator_get(&pdev->dev, "pll_vdd");
		drv_fw->pll_supply = drv_sw->pll_supply = pll_supply;
		if (IS_ERR(pll_supply))
			return PTR_ERR(pll_supply);

		ret = regulator_set_voltage(pll_supply, 1800000, 1800000);
		if (ret) {
			dev_err(&pdev->dev, "failed to set pll voltage\n");
			return ret;
		}

		ret = regulator_set_optimum_mode(pll_supply, 100000);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to set pll optimum mode\n");
			return ret;
		}

		res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		drv->modem_base = devm_request_and_ioremap(&pdev->dev, res);
		if (!drv->modem_base)
			return -ENOMEM;

		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (!res)
			return -EINVAL;
		drv->cbase = devm_ioremap(&pdev->dev, res->start,
					  resource_size(res));
		if (!drv->cbase)
			return -ENOMEM;

		ret = pil_desc_init(&drv_fw->desc);
		if (ret)
			return ret;

		ret = pil_desc_init(&drv_sw->desc);
		if (ret)
			goto err_pil_sw;
	}

	drv->subsys_desc.name = "modem";
	drv->subsys_desc.depends_on = "adsp";
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.start = modem_start;
	drv->subsys_desc.stop = modem_stop;
	drv->subsys_desc.shutdown = modem_shutdown;
	drv->subsys_desc.powerup = modem_powerup;
	drv->subsys_desc.ramdump = modem_ramdump;
	drv->subsys_desc.crash_shutdown = modem_crash_shutdown;

	drv->fw_ramdump_dev = create_ramdump_device("modem_fw", &pdev->dev);
	if (!drv->fw_ramdump_dev) {
		ret = -ENOMEM;
		goto err_fw_ramdump;
	}

	drv->sw_ramdump_dev = create_ramdump_device("modem_sw", &pdev->dev);
	if (!drv->sw_ramdump_dev) {
		ret = -ENOMEM;
		goto err_sw_ramdump;
	}

	drv->smem_ramdump_dev = create_ramdump_device("smem-modem", &pdev->dev);
	if (!drv->smem_ramdump_dev) {
		ret = -ENOMEM;
		goto err_smem_ramdump;
	}

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		ret = PTR_ERR(drv->subsys);
		goto err_subsys;
	}
	if (!drv->loadable)
		subsys_default_online(drv->subsys);

	ret = devm_request_irq(&pdev->dev, drv_fw->wdog_irq,
			modem_wdog_bite_irq, IRQF_TRIGGER_RISING,
			dev_name(&pdev->dev), drv);
	if (ret)
		goto err_irq;

	ret = devm_request_irq(&pdev->dev, drv_sw->wdog_irq,
			modem_wdog_bite_irq, IRQF_TRIGGER_RISING,
			dev_name(&pdev->dev), drv);
	if (ret)
		goto err_irq;

	ret = smsm_state_cb_register(SMSM_MODEM_STATE, SMSM_RESET,
			smsm_state_cb, drv);
	if (ret)
		goto err_irq;
	return 0;

err_irq:
	subsys_unregister(drv->subsys);
err_subsys:
	destroy_ramdump_device(drv->smem_ramdump_dev);
err_smem_ramdump:
	destroy_ramdump_device(drv->sw_ramdump_dev);
err_sw_ramdump:
	destroy_ramdump_device(drv->fw_ramdump_dev);
err_fw_ramdump:
	if (drv->loadable)
		pil_desc_release(&drv_sw->desc);
err_pil_sw:
	pil_desc_release(&drv_fw->desc);
	return ret;
}

static int __devexit pil_q6v4_modem_driver_exit(struct platform_device *pdev)
{
	struct q6v4_modem *drv = platform_get_drvdata(pdev);

	smsm_state_cb_deregister(SMSM_MODEM_STATE, SMSM_RESET,
			smsm_state_cb, drv);
	subsys_unregister(drv->subsys);
	destroy_ramdump_device(drv->smem_ramdump_dev);
	destroy_ramdump_device(drv->sw_ramdump_dev);
	destroy_ramdump_device(drv->fw_ramdump_dev);
	if (drv->loadable) {
		pil_desc_release(&drv->q6_sw.desc);
		pil_desc_release(&drv->q6_fw.desc);
	}
	return 0;
}

static struct platform_driver pil_q6v4_modem_driver = {
	.probe = pil_q6v4_modem_driver_probe,
	.remove = __devexit_p(pil_q6v4_modem_driver_exit),
	.driver = {
		.name = "pil-q6v4-modem",
		.owner = THIS_MODULE,
	},
};

static int __init pil_q6v4_modem_init(void)
{
	return platform_driver_register(&pil_q6v4_modem_driver);
}
module_init(pil_q6v4_modem_init);

static void __exit pil_q6v4_modem_exit(void)
{
	platform_driver_unregister(&pil_q6v4_modem_driver);
}
module_exit(pil_q6v4_modem_exit);

MODULE_DESCRIPTION("Support for booting QDSP6v4 (Hexagon) processors");
MODULE_LICENSE("GPL v2");
