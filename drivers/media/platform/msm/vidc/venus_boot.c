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

#define VIDC_DBG_LABEL "venus_boot"

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

#include "msm_vidc_debug.h"
#include "vidc_hfi_io.h"
#include "venus_boot.h"

/* VENUS WRAPPER registers */
#define VENUS_WRAPPER_VBIF_SS_SEC_CPA_START_ADDR_v1 \
				(VIDC_WRAPPER_BASE_OFFS + 0x1018)
#define VENUS_WRAPPER_VBIF_SS_SEC_CPA_END_ADDR_v1 \
				(VIDC_WRAPPER_BASE_OFFS + 0x101C)
#define VENUS_WRAPPER_VBIF_SS_SEC_FW_START_ADDR_v1 \
				(VIDC_WRAPPER_BASE_OFFS + 0x1020)
#define VENUS_WRAPPER_VBIF_SS_SEC_FW_END_ADDR_v1 \
				(VIDC_WRAPPER_BASE_OFFS + 0x1024)

#define VENUS_WRAPPER_VBIF_SS_SEC_CPA_START_ADDR_v2 \
				(VIDC_WRAPPER_BASE_OFFS + 0x1020)
#define VENUS_WRAPPER_VBIF_SS_SEC_CPA_END_ADDR_v2 \
				(VIDC_WRAPPER_BASE_OFFS + 0x1024)
#define VENUS_WRAPPER_VBIF_SS_SEC_FW_START_ADDR_v2 \
				(VIDC_WRAPPER_BASE_OFFS + 0x1028)
#define VENUS_WRAPPER_VBIF_SS_SEC_FW_END_ADDR_v2 \
				(VIDC_WRAPPER_BASE_OFFS + 0x102C)

#define VENUS_WRAPPER_SW_RESET	(VIDC_WRAPPER_BASE_OFFS + 0x3000)

/* VENUS VBIF registers */
#define VENUS_VBIF_CLKON_FORCE_ON			BIT(0)

#define VENUS_VBIF_ADDR_TRANS_EN  (VIDC_VBIF_BASE_OFFS + 0x1000)
#define VENUS_VBIF_AT_OLD_BASE    (VIDC_VBIF_BASE_OFFS + 0x1004)
#define VENUS_VBIF_AT_OLD_HIGH    (VIDC_VBIF_BASE_OFFS + 0x1008)
#define VENUS_VBIF_AT_NEW_BASE    (VIDC_VBIF_BASE_OFFS + 0x1010)
#define VENUS_VBIF_AT_NEW_HIGH    (VIDC_VBIF_BASE_OFFS + 0x1018)


/* Poll interval in uS */
#define POLL_INTERVAL_US				50

#define VENUS_REGION_SIZE				0x00500000

static struct {
	struct msm_vidc_platform_resources *resources;
	struct regulator *gdsc;
	const char *reg_name;
	void __iomem *reg_base;
	struct device *iommu_fw_ctx;
	struct iommu_domain *iommu_fw_domain;
	int venus_domain_num;
	bool is_booted;
	bool hw_ver_checked;
	u32 fw_sz;
	u32 hw_ver_major;
	u32 hw_ver_minor;
	void *venus_notif_hdle;
} *venus_data = NULL;

/* Get venus clocks and set rates for rate-settable clocks */
static int venus_clock_setup(struct device *dev)
{
	int i, rc = 0;
	unsigned long rate;
	struct msm_vidc_platform_resources *res = venus_data->resources;
	struct clock_info *cl;

	for (i = 0; i < res->clock_set.count; i++) {
		cl = &res->clock_set.clock_tbl[i];
		/* Make sure rate-settable clocks' rates are set */
		if (clk_get_rate(cl->clk) == 0 && cl->count) {
			rate = clk_round_rate(cl->clk, 0);
			rc = clk_set_rate(cl->clk, rate);
			if (rc) {
				dprintk(VIDC_ERR,
						"Failed to set clock rate %lu %s: %d\n",
						rate, cl->name, rc);
				break;
			}
		}
	}

	return rc;
}

static int venus_clock_prepare_enable(struct device *dev)
{
	int i, rc = 0;
	struct msm_vidc_platform_resources *res = venus_data->resources;
	struct clock_info *cl;

	for (i = 0; i < res->clock_set.count; i++) {
		cl = &res->clock_set.clock_tbl[i];
		rc = clk_prepare_enable(cl->clk);
		if (rc) {
			dprintk(VIDC_ERR, "failed to enable %s\n", cl->name);
			for (i--; i >= 0; i--) {
				cl = &res->clock_set.clock_tbl[i];
				clk_disable_unprepare(cl->clk);
			}
			return rc;
		}
	}

	return rc;
}

static void venus_clock_disable_unprepare(struct device *dev)
{
	int i;
	struct msm_vidc_platform_resources *res = venus_data->resources;
	struct clock_info *cl;

	for (i = 0; i < res->clock_set.count; i++) {
		cl = &res->clock_set.clock_tbl[i];
		clk_disable_unprepare(cl->clk);
	}
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

static int pil_venus_mem_setup(struct platform_device *pdev, size_t size)
{
	int domain;

	venus_data->iommu_fw_ctx  = msm_iommu_get_ctx("venus_fw");
	if (!venus_data->iommu_fw_ctx) {
		dprintk(VIDC_ERR, "No iommu fw context found\n");
		return -ENODEV;
	}

	if (!venus_data->venus_domain_num) {
		size = round_up(size, SZ_4K);
		domain = venus_register_domain(size);
		if (domain < 0) {
			dprintk(VIDC_ERR,
				"Venus fw iommu domain register failed\n");
			return -ENODEV;
		}
		venus_data->iommu_fw_domain = msm_get_iommu_domain(domain);
		if (!venus_data->iommu_fw_domain) {
			dprintk(VIDC_ERR, "No iommu fw domain found\n");
			return -ENODEV;
		}
		venus_data->venus_domain_num = domain;
		venus_data->fw_sz = size;
	}
	return 0;
}

static int pil_venus_auth_and_reset(struct platform_device *pdev)
{
	int rc;
	phys_addr_t fw_bias = venus_data->resources->firmware_base;
	void __iomem *reg_base = venus_data->reg_base;
	u32 ver;
	bool iommu_present = is_iommu_present(venus_data->resources);

	if (!fw_bias) {
		dprintk(VIDC_ERR, "FW bias is not valid\n");
		return -EINVAL;
	}
	/* Get Venus version number */
	if (!venus_data->hw_ver_checked) {
		ver = readl_relaxed(reg_base + VIDC_WRAPPER_HW_VERSION);
		venus_data->hw_ver_minor = (ver & 0x0FFF0000) >> 16;
		venus_data->hw_ver_major = (ver & 0xF0000000) >> 28;
		venus_data->hw_ver_checked = 1;
	}

	if (iommu_present) {
		u32 cpa_start_addr, cpa_end_addr, fw_start_addr, fw_end_addr;
		/* Get the cpa and fw start/end addr based on Venus version */
		if (venus_data->hw_ver_major == 0x1 &&
				venus_data->hw_ver_minor <= 1) {
			cpa_start_addr =
				VENUS_WRAPPER_VBIF_SS_SEC_CPA_START_ADDR_v1;
			cpa_end_addr =
				VENUS_WRAPPER_VBIF_SS_SEC_CPA_END_ADDR_v1;
			fw_start_addr =
				VENUS_WRAPPER_VBIF_SS_SEC_FW_START_ADDR_v1;
			fw_end_addr =
				VENUS_WRAPPER_VBIF_SS_SEC_FW_END_ADDR_v1;
		} else {
			cpa_start_addr =
				VENUS_WRAPPER_VBIF_SS_SEC_CPA_START_ADDR_v2;
			cpa_end_addr =
				VENUS_WRAPPER_VBIF_SS_SEC_CPA_END_ADDR_v2;
			fw_start_addr =
				VENUS_WRAPPER_VBIF_SS_SEC_FW_START_ADDR_v2;
			fw_end_addr =
				VENUS_WRAPPER_VBIF_SS_SEC_FW_END_ADDR_v2;
		}

		/* Program CPA start and end address */
		writel_relaxed(0, reg_base + cpa_start_addr);
		writel_relaxed(venus_data->fw_sz, reg_base + cpa_end_addr);

		/* Program FW start and end address */
		writel_relaxed(0, reg_base + fw_start_addr);
		writel_relaxed(venus_data->fw_sz, reg_base + fw_end_addr);
	} else {
		rc = regulator_enable(venus_data->gdsc);
		if (rc) {
			dprintk(VIDC_ERR, "GDSC enable failed\n");
			goto err;
		}

		rc = venus_clock_prepare_enable(&pdev->dev);
		if (rc) {
			dprintk(VIDC_ERR, "Clock prepare and enable failed\n");
			regulator_disable(venus_data->gdsc);
			goto err;
		}

		writel_relaxed(0, reg_base + VENUS_VBIF_AT_OLD_BASE);
		writel_relaxed(VENUS_REGION_SIZE,
				reg_base + VENUS_VBIF_AT_OLD_HIGH);
		writel_relaxed(fw_bias, reg_base + VENUS_VBIF_AT_NEW_BASE);
		writel_relaxed(fw_bias + VENUS_REGION_SIZE,
				reg_base + VENUS_VBIF_AT_NEW_HIGH);
		writel_relaxed(0x7F007F, reg_base + VENUS_VBIF_ADDR_TRANS_EN);
		venus_clock_disable_unprepare(&pdev->dev);
		regulator_disable(venus_data->gdsc);
	}
	/* Make sure all register writes are committed. */
	mb();

	/*
	 * Need to wait 10 cycles of internal clocks before bringing ARM9
	 * out of reset.
	 */
	udelay(1);

	if (iommu_present) {
		phys_addr_t pa = fw_bias;
		dma_addr_t iova;

		rc = iommu_attach_device(venus_data->iommu_fw_domain,
				venus_data->iommu_fw_ctx);
		if (rc) {
			dprintk(VIDC_ERR,
				"venus fw iommu attach failed %d\n", rc);
			goto err;
		}

		/*
		 * Map virtual addr space 0 - fw_sz to firmware physical
		 * addr space
		 */
		rc = msm_iommu_map_contig_buffer(pa,
				venus_data->venus_domain_num, 0,
				venus_data->fw_sz, SZ_4K, 0, &iova);

		if (rc || (iova != 0)) {
			dprintk(VIDC_ERR, "Failed to setup IOMMU\n");
			iommu_detach_device(venus_data->iommu_fw_domain,
					venus_data->iommu_fw_ctx);
			goto err;
		}
	}
	/* Bring Arm9 out of reset */
	writel_relaxed(0, reg_base + VENUS_WRAPPER_SW_RESET);

	venus_data->is_booted = 1;
	return 0;

err:
	return rc;
}

static int pil_venus_shutdown(struct platform_device *pdev)
{
	void __iomem *reg_base = venus_data->reg_base;
	u32 reg;
	int rc;

	if (!venus_data->is_booted)
		return 0;

	/* Assert the reset to ARM9 */
	reg = readl_relaxed(reg_base + VENUS_WRAPPER_SW_RESET);
	reg |= BIT(4);
	writel_relaxed(reg, reg_base + VENUS_WRAPPER_SW_RESET);

	/* Make sure reset is asserted before the mapping is removed */
	mb();

	if (is_iommu_present(venus_data->resources)) {
		msm_iommu_unmap_contig_buffer(0, venus_data->venus_domain_num,
				0, venus_data->fw_sz);

		iommu_detach_device(venus_data->iommu_fw_domain,
				venus_data->iommu_fw_ctx);
	}
	/*
	 * Force the VBIF clk to be on to avoid AXI bridge halt ack failure
	 * for certain Venus version.
	 */
	if (venus_data->hw_ver_major == 0x1 &&
				(venus_data->hw_ver_minor == 0x2 ||
				venus_data->hw_ver_minor == 0x3)) {
		reg = readl_relaxed(reg_base + VIDC_VENUS_VBIF_CLK_ON);
		reg |= VENUS_VBIF_CLKON_FORCE_ON;
		writel_relaxed(reg, reg_base + VIDC_VENUS_VBIF_CLK_ON);
	}

	/* Halt AXI and AXI OCMEM VBIF Access */
	reg = readl_relaxed(reg_base + VENUS_VBIF_AXI_HALT_CTRL0);
	reg |= VENUS_VBIF_AXI_HALT_CTRL0_HALT_REQ;
	writel_relaxed(reg, reg_base + VENUS_VBIF_AXI_HALT_CTRL0);

	/* Request for AXI bus port halt */
	rc = readl_poll_timeout(reg_base + VENUS_VBIF_AXI_HALT_CTRL1,
			reg, reg & VENUS_VBIF_AXI_HALT_CTRL1_HALT_ACK,
			POLL_INTERVAL_US,
			VENUS_VBIF_AXI_HALT_ACK_TIMEOUT_US);
	if (rc)
		dprintk(VIDC_ERR, "Port halt timeout\n");

	venus_data->is_booted = 0;

	return 0;
}

static int venus_notifier_cb(struct notifier_block *this, unsigned long code,
							void *ss_handle)
{
	struct notif_data *data = (struct notif_data *)ss_handle;
	static bool venus_data_set;
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
			dprintk(VIDC_ERR, "Failed to get Venus GDSC\n");
			return -ENODEV;
		}

		venus_data_set = true;
	}

	if (code != SUBSYS_AFTER_POWERUP && code != SUBSYS_AFTER_SHUTDOWN)
		return NOTIFY_DONE;

	ret = regulator_enable(venus_data->gdsc);
	if (ret) {
		dprintk(VIDC_ERR, "GDSC enable failed\n");
		return ret;
	}

	ret = venus_clock_prepare_enable(&data->pdev->dev);
	if (ret) {
		dprintk(VIDC_ERR, "Clock prepare and enable failed\n");
		goto err_clks;
	}

	if (code == SUBSYS_AFTER_POWERUP) {
		if (is_iommu_present(venus_data->resources))
			pil_venus_mem_setup(data->pdev, VENUS_REGION_SIZE);
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

int venus_boot_init(struct msm_vidc_platform_resources *res)
{
	int rc = 0;

	if (!res) {
		dprintk(VIDC_ERR, "Invalid platform resource handle\n");
		return -EINVAL;
	}
	venus_data = kzalloc(sizeof(*venus_data), GFP_KERNEL);
	if (!venus_data)
		return -ENOMEM;

	venus_data->resources = res;
	venus_data->reg_base = ioremap_nocache(res->register_base,
			(unsigned long)res->register_size);
	if (!venus_data->reg_base) {
		dprintk(VIDC_ERR,
				"could not map reg addr 0x%pa of size %d\n",
				&res->register_base, res->register_size);
		rc = -ENOMEM;
		goto err_ioremap_fail;
	}
	venus_data->venus_notif_hdle = subsys_notif_register_notifier("venus",
							&venus_notifier);
	if (IS_ERR(venus_data->venus_notif_hdle)) {
		dprintk(VIDC_ERR, "register event notification failed\n");
		rc = PTR_ERR(venus_data->venus_notif_hdle);
		goto err_subsys_notif;
	}

	return rc;

err_subsys_notif:
err_ioremap_fail:
	kfree(venus_data);
	return rc;
}

void venus_boot_deinit(void)
{
	venus_data->resources = NULL;
	subsys_notif_unregister_notifier(venus_data->venus_notif_hdle,
			&venus_notifier);
	kfree(venus_data);
}
