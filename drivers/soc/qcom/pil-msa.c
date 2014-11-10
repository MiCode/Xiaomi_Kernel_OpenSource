/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/dma-mapping.h>
#include <soc/qcom/scm.h>

#include "peripheral-loader.h"
#include "pil-q6v5.h"
#include "pil-msa.h"

/* Q6 Register Offsets */
#define QDSP6SS_RST_EVB			0x010

/* AXI Halting Registers */
#define MSS_Q6_HALT_BASE		0x180
#define MSS_MODEM_HALT_BASE		0x200
#define MSS_NC_HALT_BASE		0x280

/* RMB Status Register Values */
#define STATUS_PBL_SUCCESS		0x1
#define STATUS_XPU_UNLOCKED		0x1
#define STATUS_XPU_UNLOCKED_SCRIBBLED	0x2

/* PBL/MBA interface registers */
#define RMB_MBA_IMAGE			0x00
#define RMB_PBL_STATUS			0x04
#define RMB_MBA_COMMAND			0x08
#define RMB_MBA_STATUS			0x0C
#define RMB_PMI_META_DATA		0x10
#define RMB_PMI_CODE_START		0x14
#define RMB_PMI_CODE_LENGTH		0x18

#define POLL_INTERVAL_US		50

#define CMD_META_DATA_READY		0x1
#define CMD_LOAD_READY			0x2

#define STATUS_META_DATA_AUTH_SUCCESS	0x3
#define STATUS_AUTH_COMPLETE		0x4

/* External BHS */
#define EXTERNAL_BHS_ON			BIT(0)
#define EXTERNAL_BHS_STATUS		BIT(4)
#define BHS_TIMEOUT_US			50

#define MSS_RESTART_ID			0xA

static int pbl_mba_boot_timeout_ms = 1000;
module_param(pbl_mba_boot_timeout_ms, int, S_IRUGO | S_IWUSR);

static int modem_auth_timeout_ms = 10000;
module_param(modem_auth_timeout_ms, int, S_IRUGO | S_IWUSR);

static void modem_log_rmb_regs(void __iomem *base)
{
	pr_err("RMB_MBA_IMAGE: %08x\n", readl_relaxed(base + RMB_MBA_IMAGE));
	pr_err("RMB_PBL_STATUS: %08x\n", readl_relaxed(base + RMB_PBL_STATUS));
	pr_err("RMB_MBA_COMMAND: %08x\n",
				readl_relaxed(base + RMB_MBA_COMMAND));
	pr_err("RMB_MBA_STATUS: %08x\n", readl_relaxed(base + RMB_MBA_STATUS));
	pr_err("RMB_PMI_META_DATA: %08x\n",
				readl_relaxed(base + RMB_PMI_META_DATA));
	pr_err("RMB_PMI_CODE_START: %08x\n",
				readl_relaxed(base + RMB_PMI_CODE_START));
	pr_err("RMB_PMI_CODE_LENGTH: %08x\n",
				readl_relaxed(base + RMB_PMI_CODE_LENGTH));

}

static int pil_mss_power_up(struct q6v5_data *drv)
{
	int ret = 0;
	u32 regval;

	if (drv->vreg) {
		ret = regulator_enable(drv->vreg);
		if (ret)
			dev_err(drv->desc.dev, "Failed to enable modem regulator.\n");
	}

	if (drv->cxrail_bhs) {
		regval = readl_relaxed(drv->cxrail_bhs);
		regval |= EXTERNAL_BHS_ON;
		writel_relaxed(regval, drv->cxrail_bhs);

		ret = readl_poll_timeout(drv->cxrail_bhs, regval,
			regval & EXTERNAL_BHS_STATUS, 1, BHS_TIMEOUT_US);
	}

	return ret;
}

static int pil_mss_power_down(struct q6v5_data *drv)
{
	u32 regval;

	if (drv->cxrail_bhs) {
		regval = readl_relaxed(drv->cxrail_bhs);
		regval &= ~EXTERNAL_BHS_ON;
		writel_relaxed(regval, drv->cxrail_bhs);
	}

	if (drv->vreg)
		return regulator_disable(drv->vreg);

	return 0;
}

static int pil_mss_enable_clks(struct q6v5_data *drv)
{
	int ret;

	ret = clk_prepare_enable(drv->ahb_clk);
	if (ret)
		goto err_ahb_clk;
	ret = clk_prepare_enable(drv->axi_clk);
	if (ret)
		goto err_axi_clk;
	ret = clk_prepare_enable(drv->rom_clk);
	if (ret)
		goto err_rom_clk;
	ret = clk_prepare_enable(drv->gpll0_mss_clk);
	if (ret)
		goto err_gpll0_mss_clk;

	return 0;

err_gpll0_mss_clk:
	clk_disable_unprepare(drv->rom_clk);
err_rom_clk:
	clk_disable_unprepare(drv->axi_clk);
err_axi_clk:
	clk_disable_unprepare(drv->ahb_clk);
err_ahb_clk:
	return ret;
}

static void pil_mss_disable_clks(struct q6v5_data *drv)
{
	clk_disable_unprepare(drv->gpll0_mss_clk);
	clk_disable_unprepare(drv->rom_clk);
	clk_disable_unprepare(drv->axi_clk);
	if (!drv->ahb_clk_vote)
		clk_disable_unprepare(drv->ahb_clk);
}

static int pil_mss_restart_reg(struct q6v5_data *drv, u32 mss_restart)
{
	int ret = 0;
	int scm_ret;

	if (drv->restart_reg && !drv->restart_reg_sec) {
		writel_relaxed(mss_restart, drv->restart_reg);
		mb();
		udelay(2);
	} else if (drv->restart_reg_sec) {
		ret = scm_call(SCM_SVC_PIL, MSS_RESTART_ID, &mss_restart,
			sizeof(mss_restart), &scm_ret, sizeof(scm_ret));
		if (ret)
			pr_err("Secure MSS restart failed\n");
	}

	return ret;
}

static int pil_msa_wait_for_mba_ready(struct q6v5_data *drv)
{
	struct device *dev = drv->desc.dev;
	int ret;
	u32 status;

	/* Wait for PBL completion. */
	ret = readl_poll_timeout(drv->rmb_base + RMB_PBL_STATUS, status,
		status != 0, POLL_INTERVAL_US, pbl_mba_boot_timeout_ms * 1000);
	if (ret) {
		dev_err(dev, "PBL boot timed out\n");
		return ret;
	}
	if (status != STATUS_PBL_SUCCESS) {
		dev_err(dev, "PBL returned unexpected status %d\n", status);
		return -EINVAL;
	}

	/* Wait for MBA completion. */
	ret = readl_poll_timeout(drv->rmb_base + RMB_MBA_STATUS, status,
		status != 0, POLL_INTERVAL_US, pbl_mba_boot_timeout_ms * 1000);
	if (ret) {
		dev_err(dev, "MBA boot timed out\n");
		return ret;
	}
	if (status != STATUS_XPU_UNLOCKED &&
	    status != STATUS_XPU_UNLOCKED_SCRIBBLED) {
		dev_err(dev, "MBA returned unexpected status %d\n", status);
		return -EINVAL;
	}

	return 0;
}

int pil_mss_shutdown(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	int ret = 0;

	if (drv->axi_halt_base) {
		pil_q6v5_halt_axi_port(pil,
			drv->axi_halt_base + MSS_Q6_HALT_BASE);
		pil_q6v5_halt_axi_port(pil,
			drv->axi_halt_base + MSS_MODEM_HALT_BASE);
		pil_q6v5_halt_axi_port(pil,
			drv->axi_halt_base + MSS_NC_HALT_BASE);
	}

	if (drv->axi_halt_q6)
		pil_q6v5_halt_axi_port(pil, drv->axi_halt_q6);
	if (drv->axi_halt_mss)
		pil_q6v5_halt_axi_port(pil, drv->axi_halt_mss);
	if (drv->axi_halt_nc)
		pil_q6v5_halt_axi_port(pil, drv->axi_halt_nc);

	ret = pil_mss_restart_reg(drv, 1);

	if (drv->is_booted) {
		pil_mss_disable_clks(drv);
		pil_mss_power_down(drv);
		drv->is_booted = false;
	}

	return ret;
}

int pil_mss_deinit_image(struct pil_desc *pil)
{
	struct modem_data *drv = dev_get_drvdata(pil->dev);
	struct q6v5_data *q6_drv = container_of(pil, struct q6v5_data, desc);
	int ret = 0;

	ret = pil_mss_shutdown(pil);

	if (q6_drv->ahb_clk_vote)
		clk_disable_unprepare(q6_drv->ahb_clk);

	/* In case of any failure where reclaim MBA memory
	 * could not happen, free the memory here */
	if (drv->q6->mba_virt)
		dma_free_attrs(&drv->mba_mem_dev, drv->q6->mba_size,
				drv->q6->mba_virt, drv->q6->mba_phys,
				&drv->attrs_dma);
	return ret;
}

int pil_mss_make_proxy_votes(struct pil_desc *pil)
{
	int ret;
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	int uv = 0;

	ret = of_property_read_u32(pil->dev->of_node, "vdd_mx-uV", &uv);
	if (ret) {
		dev_err(pil->dev, "missing vdd_mx-uV property\n");
		return ret;
	}

	ret = regulator_set_voltage(drv->vreg_mx, uv, INT_MAX);
	if (ret) {
		dev_err(pil->dev, "Failed to request vreg_mx voltage\n");
		return ret;
	}

	ret = regulator_enable(drv->vreg_mx);
	if (ret) {
		dev_err(pil->dev, "Failed to enable vreg_mx\n");
		regulator_set_voltage(drv->vreg_mx, 0, INT_MAX);
		return ret;
	}

	ret = pil_q6v5_make_proxy_votes(pil);
	if (ret) {
		regulator_disable(drv->vreg_mx);
		regulator_set_voltage(drv->vreg_mx, 0, INT_MAX);
	}

	return ret;
}

void pil_mss_remove_proxy_votes(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	pil_q6v5_remove_proxy_votes(pil);
	regulator_disable(drv->vreg_mx);
	regulator_set_voltage(drv->vreg_mx, 0, INT_MAX);
}

static int pil_mss_reset(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	phys_addr_t start_addr = pil_get_entry_addr(pil);
	int ret;

	if (drv->mba_phys)
		start_addr = drv->mba_phys;

	/*
	 * Bring subsystem out of reset and enable required
	 * regulators and clocks.
	 */
	ret = pil_mss_power_up(drv);
	if (ret)
		goto err_power;

	/* Deassert reset to subsystem and wait for propagation */
	ret = pil_mss_restart_reg(drv, 0);
	if (ret)
		goto err_restart;

	ret = pil_mss_enable_clks(drv);
	if (ret)
		goto err_clks;

	/* Program Image Address */
	if (drv->self_auth) {
		writel_relaxed(start_addr, drv->rmb_base + RMB_MBA_IMAGE);
		/*
		 * Ensure write to RMB base occurs before reset
		 * is released.
		 */
		mb();
	} else {
		writel_relaxed((start_addr >> 4) & 0x0FFFFFF0,
				drv->reg_base + QDSP6SS_RST_EVB);
	}

	ret = pil_q6v5_reset(pil);
	if (ret)
		goto err_q6v5_reset;

	/* Wait for MBA to start. Check for PBL and MBA errors while waiting. */
	if (drv->self_auth) {
		ret = pil_msa_wait_for_mba_ready(drv);
		if (ret)
			goto err_q6v5_reset;
	}

	dev_info(pil->dev, "MBA boot done\n");
	drv->is_booted = true;

	return 0;

err_q6v5_reset:
	modem_log_rmb_regs(drv->rmb_base);
	pil_mss_disable_clks(drv);
	if (drv->ahb_clk_vote)
		clk_disable_unprepare(drv->ahb_clk);
err_clks:
	pil_mss_restart_reg(drv, 1);
err_restart:
	pil_mss_power_down(drv);
err_power:
	return ret;
}

int pil_mss_reset_load_mba(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	struct modem_data *md = dev_get_drvdata(pil->dev);
	const struct firmware *fw;
	char fw_name_legacy[10] = "mba.b00";
	char fw_name[10] = "mba.mbn";
	char *fw_name_p;
	void *mba_virt;
	dma_addr_t mba_phys, mba_phys_end;
	int ret, count;
	const u8 *data;

	fw_name_p = drv->non_elf_image ? fw_name_legacy : fw_name;
	/* Load and authenticate mba image */
	ret = request_firmware(&fw, fw_name_p, pil->dev);
	if (ret) {
		dev_err(pil->dev, "Failed to locate %s\n",
						fw_name_p);
		return ret;
	}

	drv->mba_size = SZ_1M;
	md->mba_mem_dev.coherent_dma_mask =
		DMA_BIT_MASK(sizeof(dma_addr_t) * 8);
	init_dma_attrs(&md->attrs_dma);
	dma_set_attr(DMA_ATTR_STRONGLY_ORDERED, &md->attrs_dma);
	mba_virt = dma_alloc_attrs(&md->mba_mem_dev, drv->mba_size,
			&mba_phys, GFP_KERNEL, &md->attrs_dma);
	if (!mba_virt) {
		dev_err(pil->dev, "MBA metadata buffer allocation failed\n");
		ret = -ENOMEM;
		goto err_dma_alloc;
	}

	drv->mba_phys = mba_phys;
	drv->mba_virt = mba_virt;
	mba_phys_end = mba_phys + drv->mba_size;

	dev_info(pil->dev, "MBA: loading from %pa to %pa\n", &mba_phys,
								&mba_phys_end);
	/* Load the MBA image into memory */
	data = fw ? fw->data : NULL;
	if (!data) {
		dev_err(pil->dev, "MBA data is NULL\n");
		ret = -ENOMEM;
		goto err_mss_reset;
	}
	count = fw->size;
	memcpy(mba_virt, data, count);
	wmb();

	ret = pil_mss_reset(pil);
	if (ret) {
		dev_err(pil->dev, "MBA boot failed.\n");
		goto err_mss_reset;
	}

	release_firmware(fw);

	return 0;

err_mss_reset:
	dma_free_attrs(&md->mba_mem_dev, drv->mba_size, drv->mba_virt,
				drv->mba_phys, &md->attrs_dma);
err_dma_alloc:
	release_firmware(fw);
	return ret;
}

static int pil_msa_auth_modem_mdt(struct pil_desc *pil, const u8 *metadata,
					size_t size)
{
	struct modem_data *drv = dev_get_drvdata(pil->dev);
	void *mdata_virt;
	dma_addr_t mdata_phys;
	s32 status;
	int ret;
	DEFINE_DMA_ATTRS(attrs);

	drv->mba_mem_dev.coherent_dma_mask =
		DMA_BIT_MASK(sizeof(dma_addr_t) * 8);
	dma_set_attr(DMA_ATTR_STRONGLY_ORDERED, &attrs);
	/* Make metadata physically contiguous and 4K aligned. */
	mdata_virt = dma_alloc_attrs(&drv->mba_mem_dev, size, &mdata_phys,
					GFP_KERNEL, &attrs);
	if (!mdata_virt) {
		dev_err(pil->dev, "MBA metadata buffer allocation failed\n");
		return -ENOMEM;
	}
	memcpy(mdata_virt, metadata, size);
	/* wmb() ensures copy completes prior to starting authentication. */
	wmb();

	/* Initialize length counter to 0 */
	writel_relaxed(0, drv->rmb_base + RMB_PMI_CODE_LENGTH);

	/* Pass address of meta-data to the MBA and perform authentication */
	writel_relaxed(mdata_phys, drv->rmb_base + RMB_PMI_META_DATA);
	writel_relaxed(CMD_META_DATA_READY, drv->rmb_base + RMB_MBA_COMMAND);
	ret = readl_poll_timeout(drv->rmb_base + RMB_MBA_STATUS, status,
		status == STATUS_META_DATA_AUTH_SUCCESS || status < 0,
		POLL_INTERVAL_US, modem_auth_timeout_ms * 1000);
	if (ret) {
		dev_err(pil->dev, "MBA authentication of headers timed out\n");
	} else if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d for headers\n",
				status);
		ret = -EINVAL;
	}

	dma_free_attrs(&drv->mba_mem_dev, size, mdata_virt, mdata_phys, &attrs);

	if (ret) {
		modem_log_rmb_regs(drv->rmb_base);
		if (drv->q6)
			pil_mss_shutdown(pil);
	}
	return ret;
}

static int pil_msa_mss_reset_mba_load_auth_mdt(struct pil_desc *pil,
				  const u8 *metadata, size_t size)
{
	int ret;

	ret = pil_mss_reset_load_mba(pil);
	if (ret)
		return ret;

	return pil_msa_auth_modem_mdt(pil, metadata, size);
}

static int pil_msa_mba_verify_blob(struct pil_desc *pil, phys_addr_t phy_addr,
				   size_t size)
{
	struct modem_data *drv = dev_get_drvdata(pil->dev);
	s32 status;
	u32 img_length = readl_relaxed(drv->rmb_base + RMB_PMI_CODE_LENGTH);

	/* Begin image authentication */
	if (img_length == 0) {
		writel_relaxed(phy_addr, drv->rmb_base + RMB_PMI_CODE_START);
		writel_relaxed(CMD_LOAD_READY, drv->rmb_base + RMB_MBA_COMMAND);
	}
	/* Increment length counter */
	img_length += size;
	writel_relaxed(img_length, drv->rmb_base + RMB_PMI_CODE_LENGTH);

	status = readl_relaxed(drv->rmb_base + RMB_MBA_STATUS);
	if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d\n", status);
		modem_log_rmb_regs(drv->rmb_base);
		return -EINVAL;
	}

	return 0;
}

static int pil_msa_mba_auth(struct pil_desc *pil)
{
	struct modem_data *drv = dev_get_drvdata(pil->dev);
	struct q6v5_data *q6_drv = container_of(pil, struct q6v5_data, desc);
	int ret;
	s32 status;

	/* Wait for all segments to be authenticated or an error to occur */
	ret = readl_poll_timeout(drv->rmb_base + RMB_MBA_STATUS, status,
			status == STATUS_AUTH_COMPLETE || status < 0,
			50, modem_auth_timeout_ms * 1000);
	if (ret) {
		dev_err(pil->dev, "MBA authentication of image timed out\n");
	} else if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d for image\n", status);
		ret = -EINVAL;
	}

	if (drv->q6 && drv->q6->mba_virt) {
		/* Reclaim MBA memory. */
		dma_free_attrs(&drv->mba_mem_dev, drv->q6->mba_size,
					drv->q6->mba_virt, drv->q6->mba_phys,
					&drv->attrs_dma);
		drv->q6->mba_virt = NULL;
	}

	if (ret)
		modem_log_rmb_regs(drv->rmb_base);
	if (q6_drv->ahb_clk_vote)
		clk_disable_unprepare(q6_drv->ahb_clk);

	return ret;
}

/*
 * To be used only if self-auth is disabled, or if the
 * MBA image is loaded as segments and not in init_image.
 */
struct pil_reset_ops pil_msa_mss_ops = {
	.proxy_vote = pil_mss_make_proxy_votes,
	.proxy_unvote = pil_mss_remove_proxy_votes,
	.auth_and_reset = pil_mss_reset,
	.shutdown = pil_mss_shutdown,
};

/*
 * To be used if self-auth is enabled and the MBA is to be loaded
 * in init_image and the modem headers are also to be authenticated
 * in init_image. Modem segments authenticated in auth_and_reset.
 */
struct pil_reset_ops pil_msa_mss_ops_selfauth = {
	.init_image = pil_msa_mss_reset_mba_load_auth_mdt,
	.proxy_vote = pil_mss_make_proxy_votes,
	.proxy_unvote = pil_mss_remove_proxy_votes,
	.verify_blob = pil_msa_mba_verify_blob,
	.auth_and_reset = pil_msa_mba_auth,
	.deinit_image = pil_mss_deinit_image,
	.shutdown = pil_mss_shutdown,
};

/*
 * To be used if the modem headers are to be authenticated
 * in init_image, and the modem segments in auth_and_reset.
 */
struct pil_reset_ops pil_msa_femto_mba_ops = {
	.init_image = pil_msa_auth_modem_mdt,
	.verify_blob = pil_msa_mba_verify_blob,
	.auth_and_reset = pil_msa_mba_auth,
};
