/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/iommu.h>
#include <linux/qcom_iommu.h>
#include <linux/iopoll.h>
#include <linux/of.h>
#include <linux/msm_iommu_domains.h>
#include <linux/platform_device.h>
#include <linux/sizes.h>
#include <linux/regulator/consumer.h>

#include <asm/page.h>

#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>

/* VENUS WRAPPER registers */
#define VENUS_WRAPPER_HW_VERSION			0x0
#define VENUS_WRAPPER_CLOCK_CONFIG			0x4

#define VENUS_WRAPPER_VBIF_SS_SEC_CPA_START_ADDR_v1	0x1018
#define VENUS_WRAPPER_VBIF_SS_SEC_CPA_END_ADDR_v1	0x101C
#define VENUS_WRAPPER_VBIF_SS_SEC_FW_START_ADDR_v1	0x1020
#define VENUS_WRAPPER_VBIF_SS_SEC_FW_END_ADDR_v1	0x1024

#define VENUS_WRAPPER_VBIF_SS_SEC_CPA_START_ADDR_v2	0x1020
#define VENUS_WRAPPER_VBIF_SS_SEC_CPA_END_ADDR_v2	0x1024
#define VENUS_WRAPPER_VBIF_SS_SEC_FW_START_ADDR_v2	0x1028
#define VENUS_WRAPPER_VBIF_SS_SEC_FW_END_ADDR_v2	0x102C

#define VENUS_WRAPPER_CPU_CLOCK_CONFIG			0x2000
#define VENUS_WRAPPER_SW_RESET				0x3000

/* VENUS VBIF registers */
#define VENUS_VBIF_CLKON				0x4
#define VENUS_VBIF_CLKON_FORCE_ON			BIT(0)

#define VENUS_VBIF_AXI_HALT_CTRL0			0x208
#define VENUS_VBIF_AXI_HALT_CTRL0_HALT_REQ		BIT(0)

#define VENUS_VBIF_AXI_HALT_CTRL1			0x20C
#define VENUS_VBIF_AXI_HALT_CTRL1_HALT_ACK		BIT(0)
#define VENUS_VBIF_AXI_HALT_ACK_TIMEOUT_US		500000

/* Poll interval in uS */
#define POLL_INTERVAL_US				50

#define VENUS_WRAPPER_CLOCK_CONFIG			0x4
#define VENUS_WRAPPER_CPU_CLOCK_CONFIG			0x2000

#define VENUS_REGION_START				0x0C800000
#define VENUS_REGION_SIZE				0x00500000

static const char * const clk_names[] = {
	"core_clk",
	"iface_clk",
	"bus_clk",
	"mem_clk",
};

static struct {
	struct regulator *gdsc;
	const char *reg_name;
	struct clk *clks[ARRAY_SIZE(clk_names)];
	void __iomem *venus_wrapper_base;
	void __iomem *venus_vbif_base;
	struct device *iommu_fw_ctx;
	struct iommu_domain *iommu_fw_domain;
	int venus_domain_num;
	bool is_booted;
	bool hw_ver_checked;
	void *ramdump_dev;
	u32 fw_sz;
	u32 fw_min_paddr;
	u32 fw_max_paddr;
	u32 bus_perf_client;
	u32 hw_ver_major;
	u32 hw_ver_minor;
	void *venus_notif_hdle;
} *venus_data = NULL;

/* Get venus clocks and set rates for rate-settable clocks */
static int venus_clock_setup(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(clk_names); i++) {
		venus_data->clks[i] = devm_clk_get(dev, clk_names[i]);
		if (IS_ERR(venus_data->clks[i])) {
			dev_err(dev, "failed to get %s\n", clk_names[i]);
			return PTR_ERR(venus_data->clks[i]);
		}
		/* Make sure rate-settable clocks' rates are set */
		if (clk_get_rate(venus_data->clks[i]) == 0)
			clk_set_rate(venus_data->clks[i],
				clk_round_rate(venus_data->clks[i], 0));
	}

	return 0;
}

static int venus_clock_prepare_enable(struct device *dev)
{
	int rc, i;

	for (i = 0; i < ARRAY_SIZE(venus_data->clks); i++) {
		rc = clk_prepare_enable(venus_data->clks[i]);
		if (rc) {
			dev_err(dev, "failed to enable %s\n", clk_names[i]);
			for (i--; i >= 0; i--)
				clk_disable_unprepare(venus_data->clks[i]);
			return rc;
		}
	}

	return 0;
}

static void venus_clock_disable_unprepare(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(venus_data->clks); i++)
		clk_disable_unprepare(venus_data->clks[i]);
}

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

int pil_venus_mem_setup(struct platform_device *pdev, phys_addr_t addr,
							size_t size)
{
	int domain;

	venus_data->iommu_fw_ctx  = msm_iommu_get_ctx("venus_fw");
	if (!venus_data->iommu_fw_ctx) {
		dev_err(&pdev->dev, "No iommu fw context found\n");
		return -ENODEV;
	}

	if (!venus_data->venus_domain_num) {
		size = round_up(size, SZ_4K);
		domain = venus_register_domain(size);
		if (domain < 0) {
			dev_err(&pdev->dev,
				"Venus fw iommu domain register failed\n");
			return -ENODEV;
		}
		venus_data->iommu_fw_domain = msm_get_iommu_domain(domain);
		if (!venus_data->iommu_fw_domain) {
			dev_err(&pdev->dev, "No iommu fw domain found\n");
			return -ENODEV;
		}
		venus_data->venus_domain_num = domain;
		venus_data->fw_sz = size;
	}
	return 0;
}

int pil_venus_auth_and_reset(struct platform_device *pdev)
{
	int rc;
	void __iomem *wrapper_base = venus_data->venus_wrapper_base;
	phys_addr_t pa = VENUS_REGION_START;
	dma_addr_t iova;
	u32 ver, cpa_start_addr, cpa_end_addr, fw_start_addr, fw_end_addr;

	/* Get Venus version number */
	if (!venus_data->hw_ver_checked) {
		ver = readl_relaxed(wrapper_base + VENUS_WRAPPER_HW_VERSION);
		venus_data->hw_ver_minor = (ver & 0x0FFF0000) >> 16;
		venus_data->hw_ver_major = (ver & 0xF0000000) >> 28;
		venus_data->hw_ver_checked = 1;
	}

	/* Get the cpa and fw start/end addr based on Venus version */
	if (venus_data->hw_ver_major == 0x1 && venus_data->hw_ver_minor <= 1) {
		cpa_start_addr = VENUS_WRAPPER_VBIF_SS_SEC_CPA_START_ADDR_v1;
		cpa_end_addr = VENUS_WRAPPER_VBIF_SS_SEC_CPA_END_ADDR_v1;
		fw_start_addr = VENUS_WRAPPER_VBIF_SS_SEC_FW_START_ADDR_v1;
		fw_end_addr = VENUS_WRAPPER_VBIF_SS_SEC_FW_END_ADDR_v1;
	} else {
		cpa_start_addr = VENUS_WRAPPER_VBIF_SS_SEC_CPA_START_ADDR_v2;
		cpa_end_addr = VENUS_WRAPPER_VBIF_SS_SEC_CPA_END_ADDR_v2;
		fw_start_addr = VENUS_WRAPPER_VBIF_SS_SEC_FW_START_ADDR_v2;
		fw_end_addr = VENUS_WRAPPER_VBIF_SS_SEC_FW_END_ADDR_v2;
	}

	/* Program CPA start and end address */
	writel_relaxed(0, wrapper_base + cpa_start_addr);
	writel_relaxed(venus_data->fw_sz, wrapper_base + cpa_end_addr);

	/* Program FW start and end address */
	writel_relaxed(0, wrapper_base + fw_start_addr);
	writel_relaxed(venus_data->fw_sz, wrapper_base + fw_end_addr);

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

	rc = iommu_attach_device(venus_data->iommu_fw_domain,
						venus_data->iommu_fw_ctx);
	if (rc) {
		dev_err(&pdev->dev, "venus fw iommu attach failed\n");
		return rc;
	}

	/* Map virtual addr space 0 - fw_sz to firmware physical addr space */
	rc = msm_iommu_map_contig_buffer(pa, venus_data->venus_domain_num, 0,
					venus_data->fw_sz, SZ_4K, 0, &iova);

	if (rc || (iova != 0)) {
		dev_err(&pdev->dev, "Failed to setup IOMMU\n");
		goto err_iommu_map;
	}

	/* Bring Arm9 out of reset */
	writel_relaxed(0, wrapper_base + VENUS_WRAPPER_SW_RESET);

	venus_data->is_booted = 1;
	return 0;

err_iommu_map:
	iommu_detach_device(venus_data->iommu_fw_domain,
					venus_data->iommu_fw_ctx);

	return rc;
}

int pil_venus_shutdown(struct platform_device *pdev)
{
	void __iomem *vbif_base = venus_data->venus_vbif_base;
	void __iomem *wrapper_base = venus_data->venus_wrapper_base;
	u32 reg;
	int rc;

	if (!venus_data->is_booted)
		return 0;

	/* Assert the reset to ARM9 */
	reg = readl_relaxed(wrapper_base + VENUS_WRAPPER_SW_RESET);
	reg |= BIT(4);
	writel_relaxed(reg, wrapper_base + VENUS_WRAPPER_SW_RESET);

	/* Make sure reset is asserted before the mapping is removed */
	mb();

	msm_iommu_unmap_contig_buffer(0, venus_data->venus_domain_num,
				      0, venus_data->fw_sz);

	iommu_detach_device(venus_data->iommu_fw_domain,
						venus_data->iommu_fw_ctx);
	/*
	 * Force the VBIF clk to be on to avoid AXI bridge halt ack failure
	 * for certain Venus version.
	 */
	if (venus_data->hw_ver_major == 0x1 &&
				(venus_data->hw_ver_minor == 0x2 ||
				venus_data->hw_ver_minor == 0x3)) {
		reg = readl_relaxed(vbif_base + VENUS_VBIF_CLKON);
		reg |= VENUS_VBIF_CLKON_FORCE_ON;
		writel_relaxed(reg, vbif_base + VENUS_VBIF_CLKON);
	}

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
		dev_err(&pdev->dev, "Port halt timeout\n");

	venus_data->is_booted = 0;

	return 0;
}

static int venus_notifier_cb(struct notifier_block *this, unsigned long code,
							void *ss_handle)
{
	struct notif_data *data = (struct notif_data *)ss_handle;
	static bool venus_data_set;
	struct resource *res;
	int ret;

	if (!data->no_auth)
		return NOTIFY_DONE;

	if (!venus_data_set) {
		ret = venus_clock_setup(&data->pdev->dev);
		if (ret)
			return ret;

		ret = of_property_read_string(data->pdev->dev.of_node,
				"qcom,proxy-reg-names", &venus_data->reg_name);
		if (ret)
			return ret;

		venus_data->gdsc = devm_regulator_get(
				&data->pdev->dev, venus_data->reg_name);
		if (IS_ERR(venus_data->gdsc)) {
			dev_err(&data->pdev->dev, "Failed to get Venus GDSC\n");
			return -ENODEV;
		}

		res = platform_get_resource_byname(data->pdev, IORESOURCE_MEM,
							"wrapper_base");
		venus_data->venus_wrapper_base = devm_request_and_ioremap(
							&data->pdev->dev, res);
		if (!venus_data->venus_wrapper_base)
			return -ENOMEM;

		res = platform_get_resource_byname(data->pdev, IORESOURCE_MEM,
							"vbif_base");
		venus_data->venus_vbif_base = devm_request_and_ioremap(
							&data->pdev->dev, res);
		if (!venus_data->venus_vbif_base)
			return -ENOMEM;

		venus_data_set = true;
	}

	if (code != SUBSYS_AFTER_POWERUP && code != SUBSYS_AFTER_SHUTDOWN)
		return NOTIFY_DONE;

	ret = regulator_enable(venus_data->gdsc);
	if (ret) {
		dev_err(&data->pdev->dev, "GDSC enable failed\n");
		return ret;
	}

	ret = venus_clock_prepare_enable(&data->pdev->dev);
	if (ret) {
		dev_err(&data->pdev->dev, "Clock prepare and enable failed\n");
		goto err_clks;
	}

	if (code == SUBSYS_AFTER_POWERUP) {
		pil_venus_mem_setup(data->pdev,
				VENUS_REGION_START, VENUS_REGION_SIZE);
		pil_venus_auth_and_reset(data->pdev);
	 } else if (code == SUBSYS_AFTER_SHUTDOWN)
		pil_venus_shutdown(data->pdev);

	venus_clock_disable_unprepare(&data->pdev->dev);
	regulator_disable(venus_data->gdsc);

	return NOTIFY_DONE;
err_clks:
	regulator_disable(venus_data->gdsc);
	return ret;
}

static struct notifier_block venus_notifier = {
	.notifier_call = venus_notifier_cb,
};

static int __init venus_boot_init(void)
{
	venus_data = kzalloc(sizeof(*venus_data), GFP_KERNEL);
	if (!venus_data)
		return -ENOMEM;

	venus_data->venus_notif_hdle = subsys_notif_register_notifier("venus",
							&venus_notifier);
	if (IS_ERR(venus_data->venus_notif_hdle)) {
		pr_err("venus_boot: register event notification failed\n");
		return PTR_ERR(venus_data->venus_notif_hdle);
	}

	return 0;
}

static void __exit venus_boot_exit(void)
{
	subsys_notif_unregister_notifier(venus_data->venus_notif_hdle,
			&venus_notifier);
	kfree(venus_data);
}

module_init(venus_boot_init);
module_exit(venus_boot_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Venus self boot driver");
