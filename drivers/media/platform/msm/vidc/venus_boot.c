/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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

#include <asm/dma-iommu.h>
#include <asm/page.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
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
	struct device *iommu_ctx_bank_dev;
	struct dma_iommu_mapping *mapping;
	dma_addr_t fw_iova;
	bool is_booted;
	bool hw_ver_checked;
	u32 fw_sz;
	u32 hw_ver_major;
	u32 hw_ver_minor;
	void *venus_notif_hdle;
} *venus_data = NULL;

/* Get venus clocks and set rates for rate-settable clocks */
static int venus_clock_setup(void)
{
	int i, rc = 0;
	unsigned long rate;
	struct msm_vidc_platform_resources *res = venus_data->resources;
	struct clock_info *cl;

	for (i = 0; i < res->clock_set.count; i++) {
		cl = &res->clock_set.clock_tbl[i];
		/* Make sure rate-settable clocks' rates are set */
		if (!clk_get_rate(cl->clk) && cl->count) {
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

static int venus_clock_prepare_enable(void)
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

static void venus_clock_disable_unprepare(void)
{
	struct msm_vidc_platform_resources *res = venus_data->resources;
	struct clock_info *cl;
	int i = res->clock_set.count;

	for (i--; i >= 0; i--) {
		cl = &res->clock_set.clock_tbl[i];
		clk_disable_unprepare(cl->clk);
	}
}

static int venus_setup_cb(struct device *dev,
				u32 size)
{
	dma_addr_t va_start = 0x0;
	size_t va_size = size;

	venus_data->mapping = arm_iommu_create_mapping(
		dev->bus, va_start, va_size);
	if (IS_ERR_OR_NULL(venus_data->mapping)) {
		dprintk(VIDC_ERR, "%s: failed to create mapping for %s\n",
		__func__, dev_name(dev));
		return -ENODEV;
	}
	dprintk(VIDC_DBG,
		"%s Attached device %pK and created mapping %pK for %s\n",
		__func__, dev, venus_data->mapping, dev_name(dev));
	return 0;
}

static int pil_venus_mem_setup(size_t size)
{
	int rc = 0;

	if (!venus_data->mapping) {
		size = round_up(size, SZ_4K);
		rc = venus_setup_cb(venus_data->iommu_ctx_bank_dev, size);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: Failed to setup context bank for venus : %s\n",
				__func__,
				dev_name(venus_data->iommu_ctx_bank_dev));
			return rc;
		}
		venus_data->fw_sz = size;
	}

	return rc;
}

static int pil_venus_auth_and_reset(void)
{
	int rc;

	phys_addr_t fw_bias = venus_data->resources->firmware_base;
	void __iomem *reg_base = venus_data->reg_base;
	u32 ver;
	bool iommu_present = is_iommu_present(venus_data->resources);
	struct device *dev = venus_data->iommu_ctx_bank_dev;

	if (!fw_bias) {
		dprintk(VIDC_ERR, "FW bias is not valid\n");
		return -EINVAL;
	}
	venus_data->fw_iova = (dma_addr_t)NULL;
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

		rc = venus_clock_prepare_enable();
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
		venus_clock_disable_unprepare();
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

		rc = arm_iommu_attach_device(dev, venus_data->mapping);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to attach iommu for %s : %d\n",
				dev_name(dev), rc);
			goto release_mapping;
		}

		dprintk(VIDC_DBG, "Attached and created mapping for %s\n",
				dev_name(dev));

		/* Map virtual addr space 0 - fw_sz to fw phys addr space */
		rc = iommu_map(venus_data->mapping->domain,
			venus_data->fw_iova, pa, venus_data->fw_sz,
			IOMMU_READ|IOMMU_WRITE|IOMMU_PRIV);
		if (!rc) {
			dprintk(VIDC_DBG,
				"%s - Successfully mapped and performed test translation!\n",
				dev_name(dev));
		}

		if (rc || (venus_data->fw_iova != 0)) {
			dprintk(VIDC_ERR, "%s - Failed to setup IOMMU\n",
					dev_name(dev));
			goto err_iommu_map;
		}
	}
	/* Bring Arm9 out of reset */
	writel_relaxed(0, reg_base + VENUS_WRAPPER_SW_RESET);

	venus_data->is_booted = 1;
	return 0;

err_iommu_map:
	if (iommu_present)
		arm_iommu_detach_device(dev);
release_mapping:
	if (iommu_present)
		arm_iommu_release_mapping(venus_data->mapping);
err:
	return rc;
}

static int pil_venus_shutdown(void)
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
		iommu_unmap(venus_data->mapping->domain, venus_data->fw_iova,
			venus_data->fw_sz);
		arm_iommu_detach_device(venus_data->iommu_ctx_bank_dev);
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
		ret = venus_clock_setup();
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

	ret = venus_clock_prepare_enable();
	if (ret) {
		dprintk(VIDC_ERR, "Clock prepare and enable failed\n");
		goto err_clks;
	}

	if (code == SUBSYS_AFTER_POWERUP) {
		if (is_iommu_present(venus_data->resources))
			pil_venus_mem_setup(VENUS_REGION_SIZE);
		pil_venus_auth_and_reset();
	} else if (code == SUBSYS_AFTER_SHUTDOWN)
		pil_venus_shutdown();

	venus_clock_disable_unprepare();
	regulator_disable(venus_data->gdsc);

	return NOTIFY_DONE;
err_clks:
	regulator_disable(venus_data->gdsc);
	return ret;
}

static struct notifier_block venus_notifier = {
	.notifier_call = venus_notifier_cb,
};

int venus_boot_init(struct msm_vidc_platform_resources *res,
			struct context_bank_info *cb)
{
	int rc = 0;

	if (!res || !cb) {
		dprintk(VIDC_ERR, "Invalid platform resource handle\n");
		return -EINVAL;
	}
	venus_data = kzalloc(sizeof(*venus_data), GFP_KERNEL);
	if (!venus_data)
		return -ENOMEM;

	venus_data->resources = res;
	venus_data->iommu_ctx_bank_dev = cb->dev;
	if (!venus_data->iommu_ctx_bank_dev) {
		dprintk(VIDC_ERR, "Invalid venus context bank device\n");
		return -ENODEV;
	}
	venus_data->reg_base = ioremap_nocache(res->register_base,
			(unsigned long)res->register_size);
	if (!venus_data->reg_base) {
		dprintk(VIDC_ERR,
				"could not map reg addr %pa of size %d\n",
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
