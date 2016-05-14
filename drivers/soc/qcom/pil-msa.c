/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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
#include <linux/highmem.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/secure_buffer.h>

#include "peripheral-loader.h"
#include "pil-q6v5.h"
#include "pil-msa.h"

/* Q6 Register Offsets */
#define QDSP6SS_RST_EVB			0x010
#define QDSP6SS_DBG_CFG			0x018

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
#define RMB_PROTOCOL_VERSION		0x1C
#define RMB_MBA_DEBUG_INFORMATION	0x20

#define POLL_INTERVAL_US		50

#define CMD_META_DATA_READY		0x1
#define CMD_LOAD_READY			0x2
#define CMD_PILFAIL_NFY_MBA		0xffffdead

#define STATUS_META_DATA_AUTH_SUCCESS	0x3
#define STATUS_AUTH_COMPLETE		0x4
#define STATUS_MBA_UNLOCKED		0x6

/* External BHS */
#define EXTERNAL_BHS_ON			BIT(0)
#define EXTERNAL_BHS_STATUS		BIT(4)
#define BHS_TIMEOUT_US			50

#define MSS_RESTART_PARAM_ID		0x2
#define MSS_RESTART_ID			0xA

#define MSS_MAGIC			0XAABADEAD
enum scm_cmd {
	PAS_MEM_SETUP_CMD = 2,
};

static int pbl_mba_boot_timeout_ms = 1000;
module_param(pbl_mba_boot_timeout_ms, int, S_IRUGO | S_IWUSR);

static int modem_auth_timeout_ms = 10000;
module_param(modem_auth_timeout_ms, int, S_IRUGO | S_IWUSR);

/* If set to 0xAABADEAD, MBA failures trigger a kernel panic */
static uint modem_trigger_panic;
module_param(modem_trigger_panic, uint, S_IRUGO | S_IWUSR);

/* To set the modem debug cookie in DBG_CFG register for debugging */
static uint modem_dbg_cfg;
module_param(modem_dbg_cfg, uint, S_IRUGO | S_IWUSR);

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
	pr_err("RMB_PROTOCOL_VERSION: %08x\n",
				readl_relaxed(base + RMB_PROTOCOL_VERSION));
	pr_err("RMB_MBA_DEBUG_INFORMATION: %08x\n",
			readl_relaxed(base + RMB_MBA_DEBUG_INFORMATION));

	if (modem_trigger_panic == MSS_MAGIC)
		panic("%s: System ramdump is needed!!!\n", __func__);
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
	ret = clk_prepare_enable(drv->snoc_axi_clk);
	if (ret)
		goto err_snoc_axi_clk;
	ret = clk_prepare_enable(drv->mnoc_axi_clk);
	if (ret)
		goto err_mnoc_axi_clk;
	return 0;
err_mnoc_axi_clk:
	clk_disable_unprepare(drv->snoc_axi_clk);
err_snoc_axi_clk:
	clk_disable_unprepare(drv->gpll0_mss_clk);
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
	clk_disable_unprepare(drv->mnoc_axi_clk);
	clk_disable_unprepare(drv->snoc_axi_clk);
	clk_disable_unprepare(drv->gpll0_mss_clk);
	clk_disable_unprepare(drv->rom_clk);
	clk_disable_unprepare(drv->axi_clk);
	if (!drv->ahb_clk_vote)
		clk_disable_unprepare(drv->ahb_clk);
}

static int pil_mss_restart_reg(struct q6v5_data *drv, u32 mss_restart)
{
	int ret = 0;
	int scm_ret = 0;
	struct scm_desc desc = {0};

	desc.args[0] = mss_restart;
	desc.args[1] = 0;
	desc.arginfo = SCM_ARGS(2);

	if (drv->restart_reg && !drv->restart_reg_sec) {
		writel_relaxed(mss_restart, drv->restart_reg);
		mb();
		udelay(2);
	} else if (drv->restart_reg_sec) {
		if (!is_scm_armv8()) {
			ret = scm_call(SCM_SVC_PIL, MSS_RESTART_ID,
					&mss_restart, sizeof(mss_restart),
					&scm_ret, sizeof(scm_ret));
		} else {
			ret = scm_call2(SCM_SIP_FNID(SCM_SVC_PIL,
						MSS_RESTART_ID), &desc);
			scm_ret = desc.ret[0];
		}
		if (ret || scm_ret)
			pr_err("Secure MSS restart failed\n");
	}

	return ret;
}

static int pil_msa_wait_for_mba_ready(struct q6v5_data *drv)
{
	struct device *dev = drv->desc.dev;
	int ret;
	u32 status;
	u64 val = is_timeout_disabled() ? 0 : pbl_mba_boot_timeout_ms * 1000;

	/* Wait for PBL completion. */
	ret = readl_poll_timeout(drv->rmb_base + RMB_PBL_STATUS, status,
				 status != 0, POLL_INTERVAL_US, val);
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
				status != 0, POLL_INTERVAL_US, val);
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

	/*
	 * Software workaround to avoid high MX current during LPASS/MSS
	 * restart.
	 */
	if (drv->mx_spike_wa && drv->ahb_clk_vote) {
		ret = clk_prepare_enable(drv->ahb_clk);
		if (!ret)
			assert_clamps(pil);
		else
			dev_err(pil->dev, "error turning ON AHB clock\n");
	}

	ret = pil_mss_restart_reg(drv, 1);

	if (drv->is_booted) {
		pil_mss_disable_clks(drv);
		pil_mss_power_down(drv);
		drv->is_booted = false;
	}

	return ret;
}

int __pil_mss_deinit_image(struct pil_desc *pil, bool err_path)
{
	struct modem_data *drv = dev_get_drvdata(pil->dev);
	struct q6v5_data *q6_drv = container_of(pil, struct q6v5_data, desc);
	int ret = 0;
	struct device *dma_dev = drv->mba_mem_dev_fixed ?: &drv->mba_mem_dev;
	s32 status;
	u64 val = is_timeout_disabled() ? 0 : pbl_mba_boot_timeout_ms * 1000;

	if (err_path) {
		writel_relaxed(CMD_PILFAIL_NFY_MBA,
				drv->rmb_base + RMB_MBA_COMMAND);
		ret = readl_poll_timeout(drv->rmb_base + RMB_MBA_STATUS, status,
				status == STATUS_MBA_UNLOCKED || status < 0,
				POLL_INTERVAL_US, val);
		if (ret)
			dev_err(pil->dev, "MBA region unlock timed out\n");
		else if (status < 0)
			dev_err(pil->dev, "MBA unlock returned err status: %d\n",
						status);
	}

	ret = pil_mss_shutdown(pil);

	if (q6_drv->ahb_clk_vote)
		clk_disable_unprepare(q6_drv->ahb_clk);

	/* In case of any failure where reclaiming MBA and DP memory
	 * could not happen, free the memory here */
	if (drv->q6->mba_dp_virt) {
		if (pil->subsys_vmid > 0)
			pil_assign_mem_to_linux(pil, drv->q6->mba_dp_phys,
						drv->q6->mba_dp_size);
		dma_free_attrs(dma_dev, drv->q6->mba_dp_size,
				drv->q6->mba_dp_virt, drv->q6->mba_dp_phys,
				&drv->attrs_dma);
		drv->q6->mba_dp_virt = NULL;
	}

	return ret;
}

int pil_mss_deinit_image(struct pil_desc *pil)
{
	return __pil_mss_deinit_image(pil, true);
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

static int pil_mss_mem_setup(struct pil_desc *pil,
					phys_addr_t addr, size_t size)
{
	struct modem_data *md = dev_get_drvdata(pil->dev);

	struct pas_init_image_req {
		u32	proc;
		u32	start_addr;
		u32	len;
	} request;
	u32 scm_ret = 0;
	int ret;
	struct scm_desc desc = {0};

	if (!md->subsys_desc.pil_mss_memsetup)
		return 0;

	request.proc = md->pas_id;
	request.start_addr = addr;
	request.len = size;

	if (!is_scm_armv8()) {
		ret = scm_call(SCM_SVC_PIL, PAS_MEM_SETUP_CMD, &request,
				sizeof(request), &scm_ret, sizeof(scm_ret));
	} else {
		desc.args[0] = md->pas_id;
		desc.args[1] = addr;
		desc.args[2] = size;
		desc.arginfo = SCM_ARGS(3);
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_PIL, PAS_MEM_SETUP_CMD),
				&desc);
		scm_ret = desc.ret[0];
	}
	if (ret)
		return ret;
	return scm_ret;
}

static int pil_mss_reset(struct pil_desc *pil)
{
	struct q6v5_data *drv = container_of(pil, struct q6v5_data, desc);
	phys_addr_t start_addr = pil_get_entry_addr(pil);
	int ret;

	if (drv->mba_dp_phys)
		start_addr = drv->mba_dp_phys;

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

	if (modem_dbg_cfg)
		writel_relaxed(modem_dbg_cfg, drv->reg_base + QDSP6SS_DBG_CFG);

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

	/* Program DP Address */
	if (drv->dp_size) {
		writel_relaxed(start_addr + SZ_1M, drv->rmb_base +
			       RMB_PMI_CODE_START);
		writel_relaxed(drv->dp_size, drv->rmb_base +
			       RMB_PMI_CODE_LENGTH);
	} else {
		writel_relaxed(0, drv->rmb_base + RMB_PMI_CODE_START);
		writel_relaxed(0, drv->rmb_base + RMB_PMI_CODE_LENGTH);
	}
	/* Make sure RMB regs are written before bringing modem out of reset */
	mb();

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
	const struct firmware *fw, *dp_fw;
	char fw_name_legacy[10] = "mba.b00";
	char fw_name[10] = "mba.mbn";
	char *dp_name = "msadp";
	char *fw_name_p;
	void *mba_dp_virt;
	dma_addr_t mba_dp_phys, mba_dp_phys_end;
	int ret, count;
	const u8 *data;
	struct device *dma_dev = md->mba_mem_dev_fixed ?: &md->mba_mem_dev;

	fw_name_p = drv->non_elf_image ? fw_name_legacy : fw_name;
	ret = request_firmware(&fw, fw_name_p, pil->dev);
	if (ret) {
		dev_err(pil->dev, "Failed to locate %s\n",
						fw_name_p);
		return ret;
	}

	data = fw ? fw->data : NULL;
	if (!data) {
		dev_err(pil->dev, "MBA data is NULL\n");
		ret = -ENOMEM;
		goto err_invalid_fw;
	}

	drv->mba_dp_size = SZ_1M;
	dma_dev->coherent_dma_mask = DMA_BIT_MASK(sizeof(dma_addr_t) * 8);
	init_dma_attrs(&md->attrs_dma);
	dma_set_attr(DMA_ATTR_SKIP_ZEROING, &md->attrs_dma);
	dma_set_attr(DMA_ATTR_STRONGLY_ORDERED, &md->attrs_dma);

	ret = request_firmware(&dp_fw, dp_name, pil->dev);
	if (ret) {
		dev_warn(pil->dev, "Debug policy not present - %s. Continue.\n",
						dp_name);
	} else {
		if (!dp_fw || !dp_fw->data) {
			dev_err(pil->dev, "Invalid DP firmware\n");
			ret = -ENOMEM;
			goto err_invalid_fw;
		}
		drv->dp_size = dp_fw->size;
		drv->mba_dp_size += drv->dp_size;
	}

	mba_dp_virt = dma_alloc_attrs(dma_dev, drv->mba_dp_size, &mba_dp_phys,
				   GFP_KERNEL, &md->attrs_dma);
	if (!mba_dp_virt) {
		dev_err(pil->dev, "%s MBA metadata buffer allocation %zx bytes failed\n",
				 __func__, drv->mba_dp_size);
		ret = -ENOMEM;
		goto err_invalid_fw;
	}

	/* Make sure there are no mappings in PKMAP and fixmap */
	kmap_flush_unused();
	kmap_atomic_flush_unused();

	drv->mba_dp_phys = mba_dp_phys;
	drv->mba_dp_virt = mba_dp_virt;
	mba_dp_phys_end = mba_dp_phys + drv->mba_dp_size;

	dev_info(pil->dev, "Loading MBA and DP (if present) from %pa to %pa size %zx\n",
			&mba_dp_phys, &mba_dp_phys_end, drv->mba_dp_size);

	/* Load the MBA image into memory */
	count = fw->size;
	memcpy(mba_dp_virt, data, count);
	/* Ensure memcpy of the MBA memory is done before loading the DP */
	wmb();

	/* Load the DP image into memory */
	if (drv->mba_dp_size > SZ_1M) {
		memcpy(mba_dp_virt + SZ_1M, dp_fw->data, dp_fw->size);
		/* Ensure memcpy is done before powering up modem */
		wmb();
	}

	if (pil->subsys_vmid > 0) {
		ret = pil_assign_mem_to_subsys(pil, drv->mba_dp_phys,
							drv->mba_dp_size);
		if (ret) {
			pr_err("scm_call to unprotect MBA and DP mem failed\n");
			goto err_mba_data;
		}
	}

	ret = pil_mss_reset(pil);
	if (ret) {
		dev_err(pil->dev, "MBA boot failed.\n");
		goto err_mss_reset;
	}

	if (dp_fw)
		release_firmware(dp_fw);
	release_firmware(fw);

	return 0;

err_mss_reset:
	if (pil->subsys_vmid > 0)
		pil_assign_mem_to_linux(pil, drv->mba_dp_phys,
							drv->mba_dp_size);
err_mba_data:
	dma_free_attrs(dma_dev, drv->mba_dp_size, drv->mba_dp_virt,
				drv->mba_dp_phys, &md->attrs_dma);
err_invalid_fw:
	if (dp_fw)
		release_firmware(dp_fw);
	release_firmware(fw);
	drv->mba_dp_virt = NULL;
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
	struct device *dma_dev = drv->mba_mem_dev_fixed ?: &drv->mba_mem_dev;
	u64 val = is_timeout_disabled() ? 0 : modem_auth_timeout_ms * 1000;
	DEFINE_DMA_ATTRS(attrs);


	dma_dev->coherent_dma_mask = DMA_BIT_MASK(sizeof(dma_addr_t) * 8);
	dma_set_attr(DMA_ATTR_SKIP_ZEROING, &attrs);
	dma_set_attr(DMA_ATTR_STRONGLY_ORDERED, &attrs);
	/* Make metadata physically contiguous and 4K aligned. */
	mdata_virt = dma_alloc_attrs(dma_dev, size, &mdata_phys, GFP_KERNEL,
				     &attrs);
	if (!mdata_virt) {
		dev_err(pil->dev, "%s MBA metadata buffer allocation %zx bytes failed\n",
			 __func__, size);
		ret = -ENOMEM;
		goto fail;
	}
	memcpy(mdata_virt, metadata, size);
	/* wmb() ensures copy completes prior to starting authentication. */
	wmb();

	if (pil->subsys_vmid > 0) {
		ret = pil_assign_mem_to_subsys(pil, mdata_phys,
							ALIGN(size, SZ_4K));
		if (ret) {
			pr_err("scm_call to unprotect modem metadata mem failed\n");
			dma_free_attrs(dma_dev, size, mdata_virt,
							mdata_phys, &attrs);
			goto fail;
		}
	}

	/* Initialize length counter to 0 */
	writel_relaxed(0, drv->rmb_base + RMB_PMI_CODE_LENGTH);

	/* Pass address of meta-data to the MBA and perform authentication */
	writel_relaxed(mdata_phys, drv->rmb_base + RMB_PMI_META_DATA);
	writel_relaxed(CMD_META_DATA_READY, drv->rmb_base + RMB_MBA_COMMAND);
	ret = readl_poll_timeout(drv->rmb_base + RMB_MBA_STATUS, status,
			status == STATUS_META_DATA_AUTH_SUCCESS || status < 0,
			POLL_INTERVAL_US, val);
	if (ret) {
		dev_err(pil->dev, "MBA authentication of headers timed out\n");
	} else if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d for headers\n",
				status);
		ret = -EINVAL;
	}

	if (pil->subsys_vmid > 0)
		pil_assign_mem_to_linux(pil, mdata_phys, ALIGN(size, SZ_4K));

	dma_free_attrs(dma_dev, size, mdata_virt, mdata_phys, &attrs);

	if (!ret)
		return ret;

fail:
	modem_log_rmb_regs(drv->rmb_base);
	if (drv->q6) {
		pil_mss_shutdown(pil);
		if (pil->subsys_vmid > 0)
			pil_assign_mem_to_linux(pil, drv->q6->mba_dp_phys,
						drv->q6->mba_dp_size);
		dma_free_attrs(dma_dev, drv->q6->mba_dp_size,
				drv->q6->mba_dp_virt, drv->q6->mba_dp_phys,
				&drv->attrs_dma);
		drv->q6->mba_dp_virt = NULL;

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
	struct device *dma_dev = drv->mba_mem_dev_fixed ?: &drv->mba_mem_dev;
	s32 status;
	u64 val = is_timeout_disabled() ? 0 : modem_auth_timeout_ms * 1000;

	/* Wait for all segments to be authenticated or an error to occur */
	ret = readl_poll_timeout(drv->rmb_base + RMB_MBA_STATUS, status,
		status == STATUS_AUTH_COMPLETE || status < 0, 50, val);
	if (ret) {
		dev_err(pil->dev, "MBA authentication of image timed out\n");
	} else if (status < 0) {
		dev_err(pil->dev, "MBA returned error %d for image\n", status);
		ret = -EINVAL;
	}

	if (drv->q6) {
		if (drv->q6->mba_dp_virt) {
			/* Reclaim MBA and DP (if allocated) memory. */
			if (pil->subsys_vmid > 0)
				pil_assign_mem_to_linux(pil,
					drv->q6->mba_dp_phys,
					drv->q6->mba_dp_size);
			dma_free_attrs(dma_dev, drv->q6->mba_dp_size,
				drv->q6->mba_dp_virt, drv->q6->mba_dp_phys,
				&drv->attrs_dma);

			drv->q6->mba_dp_virt = NULL;
		}
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
	.mem_setup = pil_mss_mem_setup,
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
