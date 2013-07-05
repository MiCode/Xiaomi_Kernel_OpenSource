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

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/smp.h>
#include <linux/miscdevice.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>

#include <mach/msm_xo.h>
#include <mach/socinfo.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_bus.h>
#include <mach/subsystem_restart.h>
#include <mach/ramdump.h>
#include <mach/msm_smem.h>

#include "peripheral-loader.h"
#include "scm-pas.h"
#include "smd_private.h"

#define GSS_CSR_AHB_CLK_SEL	0x0
#define GSS_CSR_RESET		0x4
#define GSS_CSR_CLK_BLK_CONFIG	0x8
#define GSS_CSR_CLK_ENABLE	0xC
#define GSS_CSR_BOOT_REMAP	0x14
#define GSS_CSR_POWER_UP_DOWN	0x18
#define GSS_CSR_CFG_HID		0x2C

#define GSS_SLP_CLK_CTL		0x2C60
#define GSS_RESET		0x2C64
#define GSS_CLAMP_ENA		0x2C68
#define GSS_CXO_SRC_CTL		0x2C74

#define PLL5_STATUS		0x30F8
#define PLL_ENA_GSS		0x3480

#define PLL5_VOTE		BIT(5)
#define PLL_STATUS		BIT(16)
#define REMAP_ENABLE		BIT(16)
#define A5_POWER_STATUS		BIT(4)
#define A5_POWER_ENA		BIT(0)
#define NAV_POWER_ENA		BIT(1)
#define XO_CLK_BRANCH_ENA	BIT(0)
#define SLP_CLK_BRANCH_ENA	BIT(4)
#define A5_RESET		BIT(0)

struct gss_data {
	void __iomem *base;
	void __iomem *qgic2_base;
	void __iomem *cbase;
	struct clk *xo;
	struct pil_desc pil_desc;
	struct miscdevice misc_dev;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	int crash_shutdown;
	int irq;
	void *subsys_handle;
	struct ramdump_device *ramdump_dev;
	struct ramdump_device *smem_ramdump_dev;
};

static int make_gss_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct gss_data *drv = dev_get_drvdata(pil->dev);

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		return ret;
	}
	return 0;
}

static void remove_gss_proxy_votes(struct pil_desc *pil)
{
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	clk_disable_unprepare(drv->xo);
}

static void gss_init(struct gss_data *drv)
{
	void __iomem *base = drv->base;
	void __iomem *cbase = drv->cbase;

	/* Supply clocks to GSS. */
	writel_relaxed(XO_CLK_BRANCH_ENA, cbase + GSS_CXO_SRC_CTL);
	writel_relaxed(SLP_CLK_BRANCH_ENA, cbase + GSS_SLP_CLK_CTL);

	/* Deassert GSS reset and clamps. */
	writel_relaxed(0x0, cbase + GSS_RESET);
	writel_relaxed(0x0, cbase + GSS_CLAMP_ENA);
	mb();

	/*
	 * Configure clock source and dividers for 288MHz core, 144MHz AXI and
	 * 72MHz AHB, all derived from the 288MHz PLL.
	 */
	writel_relaxed(0x341, base + GSS_CSR_CLK_BLK_CONFIG);
	writel_relaxed(0x1, base + GSS_CSR_AHB_CLK_SEL);

	/* Assert all GSS resets. */
	writel_relaxed(0x7F, base + GSS_CSR_RESET);

	/* Enable all bus clocks and wait for resets to propagate. */
	writel_relaxed(0x1F, base + GSS_CSR_CLK_ENABLE);
	mb();
	udelay(1);

	/* Release subsystem from reset, but leave A5 in reset. */
	writel_relaxed(A5_RESET, base + GSS_CSR_RESET);
}

static void cfg_qgic2_bus_access(void *data)
{
	struct gss_data *drv = data;
	int i;

	/*
	 * Apply a 8064 v1.0 workaround to configure QGIC bus access.
	 * This must be done from Krait 0 to configure the Master ID
	 * correctly.
	 */
	writel_relaxed(0x2, drv->base + GSS_CSR_CFG_HID);
	for (i = 0; i <= 3; i++)
		readl_relaxed(drv->qgic2_base);
}

static int pil_gss_shutdown(struct pil_desc *pil)
{
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	void __iomem *base = drv->base;
	void __iomem *cbase = drv->cbase;
	u32 regval;
	int ret;

	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		return ret;
	}

	/* Make sure bus port is halted. */
	msm_bus_axi_porthalt(MSM_BUS_MASTER_GSS_NAV);

	/*
	 * Vote PLL on in GSS's voting register and wait for it to enable.
	 * The PLL must be enable to switch the GFMUX to a low-power source.
	 */
	writel_relaxed(PLL5_VOTE, cbase + PLL_ENA_GSS);
	while ((readl_relaxed(cbase + PLL5_STATUS) & PLL_STATUS) == 0)
		cpu_relax();

	/* Perform one-time GSS initialization. */
	gss_init(drv);

	/* Assert A5 reset. */
	regval = readl_relaxed(base + GSS_CSR_RESET);
	regval |= A5_RESET;
	writel_relaxed(regval, base + GSS_CSR_RESET);

	/* Power down A5 and NAV. */
	regval = readl_relaxed(base + GSS_CSR_POWER_UP_DOWN);
	regval &= ~(A5_POWER_ENA|NAV_POWER_ENA);
	writel_relaxed(regval, base + GSS_CSR_POWER_UP_DOWN);

	/* Select XO clock source and increase dividers to save power. */
	regval = readl_relaxed(base + GSS_CSR_CLK_BLK_CONFIG);
	regval |= 0x3FF;
	writel_relaxed(regval, base + GSS_CSR_CLK_BLK_CONFIG);

	/* Disable bus clocks. */
	writel_relaxed(0x1F, base + GSS_CSR_CLK_ENABLE);

	/* Clear GSS PLL votes. */
	writel_relaxed(0, cbase + PLL_ENA_GSS);
	mb();

	clk_disable_unprepare(drv->xo);

	return 0;
}

static int pil_gss_reset(struct pil_desc *pil)
{
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	void __iomem *base = drv->base;
	phys_addr_t start_addr = pil_get_entry_addr(pil);
	void __iomem *cbase = drv->cbase;
	int ret;

	/* Unhalt bus port. */
	ret = msm_bus_axi_portunhalt(MSM_BUS_MASTER_GSS_NAV);
	if (ret) {
		dev_err(pil->dev, "Failed to unhalt bus port\n");
		return ret;
	}

	/* Vote PLL on in GSS's voting register and wait for it to enable. */
	writel_relaxed(PLL5_VOTE, cbase + PLL_ENA_GSS);
	while ((readl_relaxed(cbase + PLL5_STATUS) & PLL_STATUS) == 0)
		cpu_relax();

	/* Perform GSS initialization. */
	gss_init(drv);

	/* Configure boot address and enable remap. */
	writel_relaxed(REMAP_ENABLE | (start_addr >> 16),
			base + GSS_CSR_BOOT_REMAP);

	/* Power up A5 core. */
	writel_relaxed(A5_POWER_ENA, base + GSS_CSR_POWER_UP_DOWN);
	while (!(readl_relaxed(base + GSS_CSR_POWER_UP_DOWN) & A5_POWER_STATUS))
		cpu_relax();

	if (cpu_is_apq8064() &&
	    ((SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 1) &&
	     (SOCINFO_VERSION_MINOR(socinfo_get_version()) == 0))) {
		ret = smp_call_function_single(0, cfg_qgic2_bus_access, drv, 1);
		if (ret) {
			pr_err("Failed to configure QGIC2 bus access\n");
			pil_gss_shutdown(pil);
			return ret;
		}
	}

	/* Release A5 from reset. */
	writel_relaxed(0x0, base + GSS_CSR_RESET);

	return 0;
}

static struct pil_reset_ops pil_gss_ops = {
	.auth_and_reset = pil_gss_reset,
	.shutdown = pil_gss_shutdown,
	.proxy_vote = make_gss_proxy_votes,
	.proxy_unvote = remove_gss_proxy_votes,
};

static int pil_gss_init_image_trusted(struct pil_desc *pil,
		const u8 *metadata, size_t size)
{
	return pas_init_image(PAS_GSS, metadata, size);
}

static int pil_gss_shutdown_trusted(struct pil_desc *pil)
{
	struct gss_data *drv = dev_get_drvdata(pil->dev);
	int ret;

	/*
	 * CXO is used in the secure shutdown code to configure the processor
	 * for low power mode.
	 */
	ret = clk_prepare_enable(drv->xo);
	if (ret) {
		dev_err(pil->dev, "Failed to enable XO\n");
		return ret;
	}

	msm_bus_axi_porthalt(MSM_BUS_MASTER_GSS_NAV);
	ret = pas_shutdown(PAS_GSS);
	clk_disable_unprepare(drv->xo);

	return ret;
}

static int pil_gss_reset_trusted(struct pil_desc *pil)
{
	int err;

	err = msm_bus_axi_portunhalt(MSM_BUS_MASTER_GSS_NAV);
	if (err) {
		dev_err(pil->dev, "Failed to unhalt bus port\n");
		goto out;
	}

	err =  pas_auth_and_reset(PAS_GSS);
	if (err)
		goto halt_port;

	return 0;

halt_port:
	msm_bus_axi_porthalt(MSM_BUS_MASTER_GSS_NAV);
out:
	return err;
}

static struct pil_reset_ops pil_gss_ops_trusted = {
	.init_image = pil_gss_init_image_trusted,
	.auth_and_reset = pil_gss_reset_trusted,
	.shutdown = pil_gss_shutdown_trusted,
	.proxy_vote = make_gss_proxy_votes,
	.proxy_unvote = remove_gss_proxy_votes,
};

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

static void restart_gss(struct gss_data *drv)
{
	log_gss_sfr();
	subsystem_restart_dev(drv->subsys);
}

static void smsm_state_cb(void *data, uint32_t old_state, uint32_t new_state)
{
	struct gss_data *drv = data;

	/* Ignore if we're the one that set SMSM_RESET */
	if (drv->crash_shutdown)
		return;

	if (new_state & SMSM_RESET) {
		pr_err("GSS SMSM state changed to SMSM_RESET.\n"
			"Probable err_fatal on the GSS. "
			"Calling subsystem restart...\n");
		restart_gss(drv);
	}
}

static int gss_start(const struct subsys_desc *desc)
{
	struct gss_data *drv;

	drv = container_of(desc, struct gss_data, subsys_desc);
	return pil_boot(&drv->pil_desc);
}

static void gss_stop(const struct subsys_desc *desc)
{
	struct gss_data *drv;

	drv = container_of(desc, struct gss_data, subsys_desc);
	pil_shutdown(&drv->pil_desc);
}

static int gss_shutdown(const struct subsys_desc *desc)
{
	struct gss_data *drv = container_of(desc, struct gss_data, subsys_desc);

	pil_shutdown(&drv->pil_desc);
	disable_irq_nosync(drv->irq);

	return 0;
}

static int gss_powerup(const struct subsys_desc *desc)
{
	struct gss_data *drv = container_of(desc, struct gss_data, subsys_desc);

	pil_boot(&drv->pil_desc);
	enable_irq(drv->irq);
	return 0;
}

void gss_crash_shutdown(const struct subsys_desc *desc)
{
	struct gss_data *drv = container_of(desc, struct gss_data, subsys_desc);

	drv->crash_shutdown = 1;
	smsm_reset_modem(SMSM_RESET);
}

static struct ramdump_segment smem_segments[] = {
	{0x80000000, 0x00200000},
};

static int gss_ramdump(int enable, const struct subsys_desc *desc)
{
	int ret;
	struct gss_data *drv = container_of(desc, struct gss_data, subsys_desc);

	if (!enable)
		return 0;

	ret = pil_do_ramdump(&drv->pil_desc, drv->ramdump_dev);
	if (ret < 0) {
		pr_err("Unable to dump gss memory\n");
		return ret;
	}

	ret = do_elf_ramdump(drv->smem_ramdump_dev, smem_segments,
		ARRAY_SIZE(smem_segments));
	if (ret < 0) {
		pr_err("Unable to dump smem memory (rc = %d).\n", ret);
		return ret;
	}

	return 0;
}

static irqreturn_t gss_wdog_bite_irq(int irq, void *dev_id)
{
	struct gss_data *drv = dev_id;

	pr_err("Watchdog bite received from GSS!\n");
	restart_gss(drv);

	return IRQ_HANDLED;
}

static int gss_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *c = filp->private_data;
	struct gss_data *drv = container_of(c, struct gss_data, misc_dev);

	drv->subsys_handle = subsystem_get("gss");
	if (IS_ERR(drv->subsys_handle)) {
		pr_debug("%s - subsystem_get returned error\n", __func__);
		return PTR_ERR(drv->subsys_handle);
	}

	return 0;
}

static int gss_release(struct inode *inode, struct file *filp)
{
	struct miscdevice *c = filp->private_data;
	struct gss_data *drv = container_of(c, struct gss_data, misc_dev);

	subsystem_put(drv->subsys_handle);
	pr_debug("%s subsystem_put called on GSS\n", __func__);

	return 0;
}

const struct file_operations gss_file_ops = {
	.open = gss_open,
	.release = gss_release,
	.owner = THIS_MODULE,
};

static int __devinit pil_gss_probe(struct platform_device *pdev)
{
	struct gss_data *drv;
	struct resource *res;
	struct pil_desc *desc;
	int ret;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	drv->base = devm_request_and_ioremap(&pdev->dev, res);
	if (!drv->base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	drv->qgic2_base = devm_request_and_ioremap(&pdev->dev, res);
	if (!drv->qgic2_base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!res)
		return -EINVAL;
	drv->cbase = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!drv->cbase)
		return -ENOMEM;

	drv->xo = devm_clk_get(&pdev->dev, "xo");
	if (IS_ERR(drv->xo))
		return PTR_ERR(drv->xo);

	drv->irq = platform_get_irq(pdev, 0);
	if (drv->irq < 0)
		return drv->irq;

	desc = &drv->pil_desc;
	desc->name = "gss";
	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = 10000;

	if (pas_supported(PAS_GSS) > 0) {
		desc->ops = &pil_gss_ops_trusted;
		dev_info(&pdev->dev, "using secure boot\n");
	} else {
		desc->ops = &pil_gss_ops;
		dev_info(&pdev->dev, "using non-secure boot\n");
	}
	ret = pil_desc_init(desc);
	if (ret)
		return ret;

	/* Force into low power mode because hardware doesn't do this */
	desc->ops->shutdown(desc);

	ret = smsm_state_cb_register(SMSM_MODEM_STATE, SMSM_RESET,
			smsm_state_cb, drv);
	if (ret < 0)
		dev_warn(&pdev->dev, "Unable to register SMSM callback\n");

	drv->subsys_desc.name = "gss";
	drv->subsys_desc.dev = &pdev->dev;
	drv->subsys_desc.owner = THIS_MODULE;
	drv->subsys_desc.start = gss_start;
	drv->subsys_desc.stop = gss_stop;
	drv->subsys_desc.shutdown = gss_shutdown;
	drv->subsys_desc.powerup = gss_powerup;
	drv->subsys_desc.ramdump = gss_ramdump;
	drv->subsys_desc.crash_shutdown = gss_crash_shutdown;

	drv->subsys = subsys_register(&drv->subsys_desc);
	if (IS_ERR(drv->subsys)) {
		ret = PTR_ERR(drv->subsys);
		goto err_subsys;
	}

	drv->misc_dev.minor = MISC_DYNAMIC_MINOR;
	drv->misc_dev.name = "gss";
	drv->misc_dev.fops = &gss_file_ops;
	ret = misc_register(&drv->misc_dev);
	if (ret)
		goto err_misc;

	drv->ramdump_dev = create_ramdump_device("gss", &pdev->dev);
	if (!drv->ramdump_dev) {
		ret = -ENOMEM;
		goto err_ramdump;
	}

	drv->smem_ramdump_dev = create_ramdump_device("smem-gss", &pdev->dev);
	if (!drv->smem_ramdump_dev) {
		ret = -ENOMEM;
		goto err_smem;
	}

	scm_pas_init(MSM_BUS_MASTER_SPS);

	ret = devm_request_irq(&pdev->dev, drv->irq, gss_wdog_bite_irq,
			IRQF_TRIGGER_RISING, "gss_a5_wdog", drv);
	if (ret < 0)
		goto err;
	return 0;
err:
	destroy_ramdump_device(drv->smem_ramdump_dev);
err_smem:
	destroy_ramdump_device(drv->ramdump_dev);
err_ramdump:
	misc_deregister(&drv->misc_dev);
err_misc:
	subsys_unregister(drv->subsys);
err_subsys:
	pil_desc_release(desc);
	return ret;
}

static int __devexit pil_gss_remove(struct platform_device *pdev)
{
	struct gss_data *drv = platform_get_drvdata(pdev);

	destroy_ramdump_device(drv->smem_ramdump_dev);
	destroy_ramdump_device(drv->ramdump_dev);
	misc_deregister(&drv->misc_dev);
	subsys_unregister(drv->subsys);
	pil_desc_release(&drv->pil_desc);

	return 0;
}

static struct platform_driver pil_gss_driver = {
	.probe = pil_gss_probe,
	.remove = __devexit_p(pil_gss_remove),
	.driver = {
		.name = "pil_gss",
		.owner = THIS_MODULE,
	},
};

static int __init pil_gss_init(void)
{
	return platform_driver_register(&pil_gss_driver);
}
module_init(pil_gss_init);

static void __exit pil_gss_exit(void)
{
	platform_driver_unregister(&pil_gss_driver);
}
module_exit(pil_gss_exit);

MODULE_DESCRIPTION("Support for booting the GSS processor");
MODULE_LICENSE("GPL v2");
