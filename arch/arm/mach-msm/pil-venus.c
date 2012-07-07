/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/elf.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <asm/page.h>
#include <asm/sizes.h>

#include <mach/iommu.h>
#include <mach/iommu_domains.h>

#include "peripheral-loader.h"
#include "scm-pas.h"

/* VENUS WRAPPER registers */
#define VENUS_WRAPPER_CLOCK_CONFIG			0x4
#define VENUS_WRAPPER_VBIF_SS_SEC_CPA_START_ADDR	0x1018
#define VENUS_WRAPPER_VBIF_SS_SEC_CPA_END_ADDR		0x101C
#define VENUS_WRAPPER_VBIF_SS_SEC_FW_START_ADDR		0x1020
#define VENUS_WRAPPER_VBIF_SS_SEC_FW_END_ADDR		0x1024
#define VENUS_WRAPPER_CPU_CLOCK_CONFIG			0x2000
#define VENUS_WRAPPER_SW_RESET				0x3000

/* VENUS VBIF registers */
#define VENUS_VBIF_AXI_HALT_CTRL0			0x0
#define VENUS_VBIF_AXI_HALT_CTRL0_HALT_REQ		BIT(0)

#define VENUS_VBIF_AXI_HALT_CTRL1			0x4
#define VENUS_VBIF_AXI_HALT_CTRL1_HALT_ACK		BIT(0)
#define VENUS_VBIF_AXI_HALT_ACK_TIMEOUT_US		500000

/* PIL proxy vote timeout */
#define VENUS_PROXY_TIMEOUT				10000

/* Poll interval in uS */
#define POLL_INTERVAL_US				50

static const char * const clk_names[] = {
	"core_clk",
	"iface_clk",
	"bus_clk",
	"mem_clk",
};

struct venus_data {
	void __iomem *venus_wrapper_base;
	void __iomem *venus_vbif_base;
	struct pil_device *pil;
	struct regulator *gdsc;
	phys_addr_t start_addr;
	struct clk *clks[ARRAY_SIZE(clk_names)];
	struct device *iommu_fw_ctx;
	struct iommu_domain *iommu_fw_domain;
	int venus_domain_num;
	bool is_booted;
	u32 fw_sz;
	u32 fw_min_paddr;
	u32 fw_max_paddr;
};

static int venus_register_domain(u32 fw_max_sz)
{
	struct msm_iova_partition venus_fw_partition = {
		.start = 0,
		.size = fw_max_sz,
	};
	struct msm_iova_layout venus_fw_layout = {
		.partitions = &venus_fw_partition,
		.npartitions = 1,
		.client_name = "pil_venus",
		.domain_flags = 0,
	};

	return msm_register_domain(&venus_fw_layout);
}

/* Get venus clocks and set rates for rate-settable clocks */
static int venus_clock_setup(struct device *dev)
{
	struct venus_data *drv = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(drv->clks); i++) {
		drv->clks[i] = devm_clk_get(dev, clk_names[i]);
		if (IS_ERR(drv->clks[i])) {
			dev_err(dev, "failed to get %s\n",
				clk_names[i]);
			return PTR_ERR(drv->clks[i]);
		}
		/* Make sure rate-settable clocks' rates are set */
		if (clk_get_rate(drv->clks[i]) == 0)
			clk_set_rate(drv->clks[i],
				     clk_round_rate(drv->clks[i], 0));
	}

	return 0;
}

static int venus_clock_prepare_enable(struct device *dev)
{
	struct venus_data *drv = dev_get_drvdata(dev);
	int rc, i;

	for (i = 0; i < ARRAY_SIZE(drv->clks); i++) {
		rc = clk_prepare_enable(drv->clks[i]);
		if (rc) {
			dev_err(dev, "failed to enable %s\n",
				clk_names[i]);
			for (i--; i >= 0; i--)
				clk_disable_unprepare(drv->clks[i]);
			return rc;
		}
	}

	return 0;
}

static void venus_clock_disable_unprepare(struct device *dev)
{
	struct venus_data *drv = dev_get_drvdata(dev);
	int i;

	for (i = 0; i < ARRAY_SIZE(drv->clks); i++)
		clk_disable_unprepare(drv->clks[i]);
}

static int pil_venus_make_proxy_vote(struct pil_desc *pil)
{
	struct venus_data *drv = dev_get_drvdata(pil->dev);
	int rc;

	/*
	 * Clocks need to be proxy voted to be able to pass control
	 * of clocks from PIL driver to the Venus driver. But GDSC
	 * needs to be turned on before clocks can be turned on. So
	 * enable the GDSC here.
	 */
	rc = regulator_enable(drv->gdsc);
	if (rc) {
		dev_err(pil->dev, "GDSC enable failed\n");
		return rc;
	}

	rc = venus_clock_prepare_enable(pil->dev);
	if (rc)
		regulator_disable(drv->gdsc);

	return rc;
}

static void pil_venus_remove_proxy_vote(struct pil_desc *pil)
{
	struct venus_data *drv = dev_get_drvdata(pil->dev);

	venus_clock_disable_unprepare(pil->dev);

	/* Disable GDSC */
	regulator_disable(drv->gdsc);
}

static int pil_venus_init_image(struct pil_desc *pil, const u8 *metadata,
		size_t size)
{
	const struct elf32_hdr *ehdr = (struct elf32_hdr *)metadata;
	struct venus_data *drv = dev_get_drvdata(pil->dev);

	drv->start_addr = ehdr->e_entry;

	if (drv->start_addr < drv->fw_min_paddr ||
	    drv->start_addr >= drv->fw_max_paddr) {
		dev_err(pil->dev, "fw start addr is not within valid range\n");
		return -EINVAL;
	}

	drv->fw_sz = drv->fw_max_paddr - drv->start_addr;

	return 0;
}

static int pil_venus_reset(struct pil_desc *pil)
{
	int rc;
	struct venus_data *drv = dev_get_drvdata(pil->dev);
	void __iomem *wrapper_base = drv->venus_wrapper_base;
	phys_addr_t pa = drv->start_addr;
	unsigned long iova;

	/*
	 * GDSC needs to remain on till Venus is shutdown. So, enable
	 * the GDSC here again to make sure it remains on beyond the
	 * expiry of the proxy vote timer.
	 */
	rc = regulator_enable(drv->gdsc);
	if (rc) {
		dev_err(pil->dev, "GDSC enable failed\n");
		return rc;
	}

	/* Program CPA start and end address */
	writel_relaxed(0, wrapper_base +
			VENUS_WRAPPER_VBIF_SS_SEC_CPA_START_ADDR);
	writel_relaxed(drv->fw_sz, wrapper_base +
			VENUS_WRAPPER_VBIF_SS_SEC_CPA_END_ADDR);

	/* Program FW start and end address */
	writel_relaxed(0, wrapper_base +
			VENUS_WRAPPER_VBIF_SS_SEC_FW_START_ADDR);
	writel_relaxed(drv->fw_sz, wrapper_base +
			VENUS_WRAPPER_VBIF_SS_SEC_FW_END_ADDR);

	rc = iommu_attach_device(drv->iommu_fw_domain, drv->iommu_fw_ctx);
	if (rc) {
		dev_err(pil->dev, "venus fw iommu attach failed\n");
		goto err_iommu_attach;
	}

	/* Enable all Venus internal clocks */
	writel_relaxed(0, wrapper_base + VENUS_WRAPPER_CLOCK_CONFIG);
	writel_relaxed(0, wrapper_base + VENUS_WRAPPER_CPU_CLOCK_CONFIG);

	/* Make sure clocks are enabled */
	mb();

	/*
	 * Need to wait 10 cycles of internal clocks before bringing ARM9
	 * out of reset.
	 */
	udelay(1);

	/* Map virtual addr space 0 - fw_sz to firmware physical addr space */
	rc = msm_iommu_map_contig_buffer(pa, drv->venus_domain_num, 0,
					 drv->fw_sz, SZ_4K, 0, &iova);

	if (rc || (iova != 0)) {
		dev_err(pil->dev, "Failed to setup IOMMU\n");
		goto err_iommu_map;
	}

	/* Bring Arm9 out of reset */
	writel_relaxed(0, wrapper_base + VENUS_WRAPPER_SW_RESET);

	drv->is_booted = 1;

	return 0;

err_iommu_map:
	iommu_detach_device(drv->iommu_fw_domain, drv->iommu_fw_ctx);

err_iommu_attach:
	regulator_disable(drv->gdsc);

	return rc;
}

static int pil_venus_shutdown(struct pil_desc *pil)
{
	struct venus_data *drv = dev_get_drvdata(pil->dev);
	void __iomem *vbif_base = drv->venus_vbif_base;
	void __iomem *wrapper_base = drv->venus_wrapper_base;
	u32 reg;
	int rc;

	if (!drv->is_booted)
		return 0;

	venus_clock_prepare_enable(pil->dev);

	/* Halt AXI and AXI OCMEM VBIF Access */
	reg = readl_relaxed(vbif_base + VENUS_VBIF_AXI_HALT_CTRL0);
	reg |= VENUS_VBIF_AXI_HALT_CTRL0_HALT_REQ;
	writel_relaxed(reg, vbif_base + VENUS_VBIF_AXI_HALT_CTRL0);

	/* Request for AXI bus port halt */
	rc = readl_poll_timeout(vbif_base + VENUS_VBIF_AXI_HALT_CTRL1,
			reg, reg & VENUS_VBIF_AXI_HALT_CTRL1_HALT_ACK,
			POLL_INTERVAL_US,
			VENUS_VBIF_AXI_HALT_ACK_TIMEOUT_US);
	if (rc)
		dev_err(pil->dev, "Port halt timeout\n");

	/* Assert the reset to ARM9 */
	reg = readl_relaxed(wrapper_base + VENUS_WRAPPER_SW_RESET);
	reg |= BIT(4);
	writel_relaxed(reg, wrapper_base + VENUS_WRAPPER_SW_RESET);

	/* Make sure reset is asserted before the mapping is removed */
	mb();

	msm_iommu_unmap_contig_buffer(0, drv->venus_domain_num,
				      0, drv->fw_sz);

	iommu_detach_device(drv->iommu_fw_domain, drv->iommu_fw_ctx);

	venus_clock_disable_unprepare(pil->dev);

	regulator_disable(drv->gdsc);

	drv->is_booted = 0;

	return 0;
}

static struct pil_reset_ops pil_venus_ops = {
	.init_image = pil_venus_init_image,
	.auth_and_reset = pil_venus_reset,
	.shutdown = pil_venus_shutdown,
	.proxy_vote = pil_venus_make_proxy_vote,
	.proxy_unvote = pil_venus_remove_proxy_vote,
};

static int __devinit pil_venus_probe(struct platform_device *pdev)
{
	struct venus_data *drv;
	struct resource *res;
	struct pil_desc *desc;
	int rc;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -EINVAL;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	drv->venus_wrapper_base = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (!drv->venus_wrapper_base)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -EINVAL;

	drv->venus_vbif_base = devm_ioremap(&pdev->dev, res->start,
					resource_size(res));
	if (!drv->venus_vbif_base)
		return -ENOMEM;

	drv->gdsc = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(drv->gdsc)) {
		dev_err(&pdev->dev, "Failed to get Venus GDSC\n");
		return -ENODEV;
	}

	rc = venus_clock_setup(&pdev->dev);
	if (rc)
		return rc;

	drv->iommu_fw_ctx  = msm_iommu_get_ctx("venus_fw");
	if (!drv->iommu_fw_ctx) {
		dev_err(&pdev->dev, "No iommu fw context found\n");
		return -ENODEV;
	}

	/* Get fw address boundaries */
	rc = of_property_read_u32(pdev->dev.of_node,
				  "qcom,firmware-max-paddr",
				  &drv->fw_max_paddr);
	if (rc) {
		dev_err(&pdev->dev, "Failed to get fw max paddr\n");
		return rc;
	}

	rc = of_property_read_u32(pdev->dev.of_node,
				  "qcom,firmware-min-paddr",
				  &drv->fw_min_paddr);
	if (rc) {
		dev_err(&pdev->dev, "Failed to get fw min paddr\n");
		return rc;
	}

	if (drv->fw_max_paddr <= drv->fw_min_paddr) {
		dev_err(&pdev->dev, "Invalid fw max paddr or min paddr\n");
		return -EINVAL;
	}

	drv->venus_domain_num =
		venus_register_domain(drv->fw_max_paddr - drv->fw_min_paddr);
	if (drv->venus_domain_num < 0) {
		dev_err(&pdev->dev, "Venus fw iommu domain register failed\n");
		return -ENODEV;
	}

	drv->iommu_fw_domain = msm_get_iommu_domain(drv->venus_domain_num);
	if (!drv->iommu_fw_domain) {
		dev_err(&pdev->dev, "No iommu fw domain found\n");
		return -ENODEV;
	}

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	rc = of_property_read_string(pdev->dev.of_node, "qcom,firmware-name",
				      &desc->name);
	if (rc)
		return rc;

	desc->dev = &pdev->dev;
	desc->owner = THIS_MODULE;
	desc->proxy_timeout = VENUS_PROXY_TIMEOUT;

	/* TODO: need to add secure boot when the support is available */
	desc->ops = &pil_venus_ops;
	dev_info(&pdev->dev, "using non-secure boot\n");

	drv->pil = msm_pil_register(desc);
	if (IS_ERR(drv->pil))
		return PTR_ERR(drv->pil);

	return 0;
}

static int __devexit pil_venus_remove(struct platform_device *pdev)
{
	struct venus_data *drv = platform_get_drvdata(pdev);
	msm_pil_unregister(drv->pil);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id msm_pil_venus_match[] = {
	{.compatible = "qcom,pil-venus"},
	{}
};
#endif

static struct platform_driver pil_venus_driver = {
	.probe = pil_venus_probe,
	.remove = __devexit_p(pil_venus_remove),
	.driver = {
		.name = "pil_venus",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(msm_pil_venus_match),
	},
};

module_platform_driver(pil_venus_driver);

MODULE_DESCRIPTION("Support for booting VENUS processors");
MODULE_LICENSE("GPL v2");
