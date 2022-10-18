// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <media/cam_icp.h>

#include "cam_io_util.h"
#include "cam_a5_hw_intf.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "a5_core.h"
#include "a5_reg.h"
#include "a5_soc.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "hfi_intf.h"
#include "hfi_sys_defs.h"
#include "cam_icp_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"
#include "cam_icp_utils.h"
#include "cam_common_util.h"
#include "cam_compat.h"

#define PC_POLL_DELAY_US 100
#define PC_POLL_TIMEOUT_US 10000

#define A5_GEN_PURPOSE_REG_OFFSET 0x40

static int cam_a5_cpas_vote(struct cam_a5_device_core_info *core_info,
	struct cam_icp_cpas_vote *cpas_vote)
{
	int rc = 0;

	if (cpas_vote->ahb_vote_valid)
		rc = cam_cpas_update_ahb_vote(core_info->cpas_handle,
			&cpas_vote->ahb_vote);

	if (cpas_vote->axi_vote_valid)
		rc = cam_cpas_update_axi_vote(core_info->cpas_handle,
			&cpas_vote->axi_vote);

	if (rc)
		CAM_ERR(CAM_ICP, "cpas vote is failed: %d", rc);

	return rc;
}

static int32_t cam_a5_download_fw(void *device_priv)
{
	int32_t rc = 0;
	uint32_t fw_size;
	const uint8_t *fw_start = NULL;
	struct cam_hw_info *a5_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_a5_device_core_info *core_info = NULL;
	struct cam_a5_device_hw_info *hw_info = NULL;
	struct platform_device         *pdev = NULL;
	struct a5_soc_info *cam_a5_soc_info = NULL;

	if (!device_priv) {
		CAM_ERR(CAM_ICP, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &a5_dev->soc_info;
	core_info = (struct cam_a5_device_core_info *)a5_dev->core_info;
	hw_info = core_info->a5_hw_info;
	pdev = soc_info->pdev;
	cam_a5_soc_info = soc_info->soc_private;

	if (cam_a5_soc_info->fw_name) {
		CAM_INFO(CAM_ICP, "Downloading firmware %s",
			cam_a5_soc_info->fw_name);
		rc = firmware_request_nowarn(&core_info->fw_elf,
				cam_a5_soc_info->fw_name, &pdev->dev);
		if (rc) {
			CAM_ERR(CAM_ICP, "Failed to locate fw: %d", rc);
			return rc;
		}
	} else {
		rc = firmware_request_nowarn(&core_info->fw_elf,
				"CAMERA_ICP.elf", &pdev->dev);
		if (rc) {
			CAM_ERR(CAM_ICP, "Failed to locate fw: %d", rc);
			return rc;
		}
	}

	if (!core_info->fw_elf) {
		CAM_ERR(CAM_ICP, "Invalid elf size");
		rc = -EINVAL;
		goto fw_download_failed;
	}

	fw_start = core_info->fw_elf->data;
	rc = cam_icp_validate_fw(fw_start, EM_ARM);
	if (rc) {
		CAM_ERR(CAM_ICP, "fw elf validation failed");
		goto fw_download_failed;
	}

	rc = cam_icp_get_fw_size(fw_start, &fw_size);
	if (rc) {
		CAM_ERR(CAM_ICP, "unable to get fw size");
		goto fw_download_failed;
	}

	if (core_info->fw_buf_len < fw_size) {
		CAM_ERR(CAM_ICP, "mismatch in fw size: %u %llu",
			fw_size, core_info->fw_buf_len);
		rc = -EINVAL;
		goto fw_download_failed;
	}

	rc = cam_icp_program_fw(fw_start, core_info->fw_kva_addr);
	if (rc) {
		CAM_ERR(CAM_ICP, "fw program is failed");
		goto fw_download_failed;
	}

fw_download_failed:
	release_firmware(core_info->fw_elf);
	return rc;
}

static int cam_a5_fw_dump(
	struct cam_icp_hw_dump_args    *dump_args,
	struct cam_a5_device_core_info *core_info)
{
	u8                         *dest;
	u8                         *src;
	uint64_t                    size_required;
	struct cam_icp_dump_header *hdr;

	if (!core_info || !dump_args) {
		CAM_ERR(CAM_ICP, "invalid params %pK %pK",
		    core_info, dump_args);
		return -EINVAL;
	}
	if (!core_info->fw_kva_addr || !dump_args->cpu_addr) {
		CAM_ERR(CAM_ICP, "invalid params %pK, 0x%zx",
		    core_info->fw_kva_addr, dump_args->cpu_addr);
		return -EINVAL;
	}

	size_required = core_info->fw_buf_len +
		sizeof(struct cam_icp_dump_header);

	if (dump_args->buf_len <= dump_args->offset) {
		CAM_WARN(CAM_ICP, "Dump offset overshoot len %zu offset %zu",
			dump_args->buf_len, dump_args->offset);
		return -ENOSPC;
	}

	if ((dump_args->buf_len - dump_args->offset) < size_required) {
		CAM_WARN(CAM_ICP, "Dump buffer exhaust required %llu len %llu",
			size_required, core_info->fw_buf_len);
		return -ENOSPC;
	}

	dest = (u8 *)dump_args->cpu_addr + dump_args->offset;
	hdr = (struct cam_icp_dump_header *)dest;
	scnprintf(hdr->tag, CAM_ICP_DUMP_TAG_MAX_LEN, "ICP_FW:");
	hdr->word_size = sizeof(u8);
	hdr->size = core_info->fw_buf_len;
	src = (u8 *)core_info->fw_kva_addr;
	dest = (u8 *)dest + sizeof(struct cam_icp_dump_header);
	memcpy_fromio(dest, src, core_info->fw_buf_len);
	dump_args->offset += hdr->size + sizeof(struct cam_icp_dump_header);
	return 0;
}

static int cam_a5_fw_mini_dump(
	struct cam_icp_hw_dump_args    *dump_args,
	struct cam_a5_device_core_info *core_info)
{
	u8                         *dest;
	u8                         *src;

	if (!core_info || !dump_args) {
		CAM_ERR(CAM_ICP, "invalid params %pK %pK", core_info, dump_args);
		return -EINVAL;
	}

	if (!core_info->fw_kva_addr || !dump_args->cpu_addr) {
		CAM_ERR(CAM_ICP, "invalid params %pK, 0x%zx", core_info->fw_kva_addr,
			dump_args->cpu_addr);
		return -EINVAL;
	}

	if (dump_args->buf_len < core_info->fw_buf_len) {
		CAM_WARN(CAM_ICP, "Insufficient Len %lu fw_len %llu",
			dump_args->buf_len, core_info->fw_buf_len);
		return -ENOSPC;
	}

	dest = (u8 *)dump_args->cpu_addr;
	src = (u8 *)core_info->fw_kva_addr;
	memcpy_fromio(dest, src, core_info->fw_buf_len);
	dump_args->offset = core_info->fw_buf_len;
	return 0;
}

int cam_a5_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *a5_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_a5_device_core_info *core_info = NULL;
	struct a5_soc_info *a5_soc_info;
	struct cam_icp_cpas_vote cpas_vote;
	unsigned long flags;
	int rc = 0;

	if (!device_priv) {
		CAM_ERR(CAM_ICP, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &a5_dev->soc_info;
	core_info = (struct cam_a5_device_core_info *)a5_dev->core_info;

	if ((!soc_info) || (!core_info)) {
		CAM_ERR(CAM_ICP, "soc_info: %pK core_info: %pK",
			soc_info, core_info);
		return -EINVAL;
	}

	spin_lock_irqsave(&a5_dev->hw_lock, flags);
	if (a5_dev->hw_state == CAM_HW_STATE_POWER_UP) {
		spin_unlock_irqrestore(&a5_dev->hw_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&a5_dev->hw_lock, flags);

	a5_soc_info = soc_info->soc_private;

	cpas_vote.ahb_vote.type = CAM_VOTE_ABSOLUTE;
	cpas_vote.ahb_vote.vote.level = CAM_LOWSVS_VOTE;
	cpas_vote.axi_vote.num_paths = 1;
	cpas_vote.axi_vote.axi_path[0].path_data_type =
		CAM_ICP_DEFAULT_AXI_PATH;
	cpas_vote.axi_vote.axi_path[0].transac_type =
		CAM_ICP_DEFAULT_AXI_TRANSAC;
	cpas_vote.axi_vote.axi_path[0].camnoc_bw =
		CAM_ICP_BW_BYTES_VOTE;
	cpas_vote.axi_vote.axi_path[0].mnoc_ab_bw =
		CAM_ICP_BW_BYTES_VOTE;
	cpas_vote.axi_vote.axi_path[0].mnoc_ib_bw =
		CAM_ICP_BW_BYTES_VOTE;
	cpas_vote.axi_vote.axi_path[0].ddr_ab_bw =
		CAM_ICP_BW_BYTES_VOTE;
	cpas_vote.axi_vote.axi_path[0].ddr_ib_bw =
		CAM_ICP_BW_BYTES_VOTE;

	rc = cam_cpas_start(core_info->cpas_handle,
		&cpas_vote.ahb_vote, &cpas_vote.axi_vote);
	if (rc) {
		CAM_ERR(CAM_ICP, "cpas start failed: %d", rc);
		goto error;
	}
	core_info->cpas_start = true;

	rc = cam_a5_enable_soc_resources(soc_info);
	if (rc) {
		CAM_ERR(CAM_ICP, "soc enable is failed: %d", rc);
		if (cam_cpas_stop(core_info->cpas_handle))
			CAM_ERR(CAM_ICP, "cpas stop is failed");
		else
			core_info->cpas_start = false;
	} else {
		CAM_DBG(CAM_ICP, "a5_qos %d", a5_soc_info->a5_qos_val);
		if (a5_soc_info->a5_qos_val)
			cam_io_w_mb(a5_soc_info->a5_qos_val,
				soc_info->reg_map[A5_SIERRA_BASE].mem_base +
				ICP_SIERRA_A5_CSR_ACCESS);
	}

	spin_lock_irqsave(&a5_dev->hw_lock, flags);
	a5_dev->hw_state = CAM_HW_STATE_POWER_UP;
	spin_unlock_irqrestore(&a5_dev->hw_lock, flags);

error:
	return rc;
}

int cam_a5_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *a5_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_a5_device_core_info *core_info = NULL;
	unsigned long flags;
	int rc = 0;

	if (!device_priv) {
		CAM_ERR(CAM_ICP, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &a5_dev->soc_info;
	core_info = (struct cam_a5_device_core_info *)a5_dev->core_info;
	if ((!soc_info) || (!core_info)) {
		CAM_ERR(CAM_ICP, "soc_info = %pK core_info = %pK",
			soc_info, core_info);
		return -EINVAL;
	}

	spin_lock_irqsave(&a5_dev->hw_lock, flags);
	if (a5_dev->hw_state == CAM_HW_STATE_POWER_DOWN) {
		spin_unlock_irqrestore(&a5_dev->hw_lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&a5_dev->hw_lock, flags);

	rc = cam_a5_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_ICP, "soc disable is failed: %d", rc);

	if (core_info->cpas_start) {
		if (cam_cpas_stop(core_info->cpas_handle))
			CAM_ERR(CAM_ICP, "cpas stop is failed");
		else
			core_info->cpas_start = false;
	}

	spin_lock_irqsave(&a5_dev->hw_lock, flags);
	a5_dev->hw_state = CAM_HW_STATE_POWER_DOWN;
	spin_unlock_irqrestore(&a5_dev->hw_lock, flags);

	return rc;
}

static int cam_a5_power_resume(struct cam_hw_info *a5_info, bool debug_enabled)
{
	uint32_t val = A5_CSR_FULL_CPU_EN;
	void __iomem *base;
	struct cam_hw_soc_info *soc_info = NULL;
	struct a5_soc_info *a5_soc_info;

	if (!a5_info) {
		CAM_ERR(CAM_ICP, "invalid A5 device info");
		return -EINVAL;
	}

	base = a5_info->soc_info.reg_map[A5_SIERRA_BASE].mem_base;
	soc_info = &a5_info->soc_info;
	a5_soc_info = soc_info->soc_private;

	cam_io_w_mb(A5_CSR_A5_CPU_EN, base + ICP_SIERRA_A5_CSR_A5_CONTROL);
	cam_io_w_mb(A5_CSR_FUNC_RESET, base + ICP_SIERRA_A5_CSR_NSEC_RESET);

	if (debug_enabled)
		val |= A5_CSR_FULL_DBG_EN;

	cam_io_w_mb(val, base + ICP_SIERRA_A5_CSR_A5_CONTROL);
	cam_io_w_mb(a5_soc_info->a5_qos_val,
		base + ICP_SIERRA_A5_CSR_ACCESS);

	CAM_DBG(CAM_ICP, "a5 qos-val : 0x%x",
		cam_io_r_mb(base + ICP_SIERRA_A5_CSR_ACCESS));

	return 0;
}

static int cam_a5_power_collapse(struct cam_hw_info *a5_info)
{
	uint32_t val, status = 0;
	void __iomem *base;

	if (!a5_info) {
		CAM_ERR(CAM_ICP, "invalid A5 device info");
		return -EINVAL;
	}

	base = a5_info->soc_info.reg_map[A5_SIERRA_BASE].mem_base;

	/**
	 * Need to poll here to confirm that FW has triggered WFI
	 * and Host can then proceed. No interrupt is expected
	 * from FW at this time.
	 */

	if (cam_common_read_poll_timeout(base +
		    ICP_SIERRA_A5_CSR_A5_STATUS,
		    PC_POLL_DELAY_US, PC_POLL_TIMEOUT_US,
		    A5_CSR_A5_STANDBYWFI,
		    A5_CSR_A5_STANDBYWFI, &status)) {
		CAM_ERR(CAM_ICP, "WFI poll timed out: status=0x%08x", status);
		return -ETIMEDOUT;
	}

	val = cam_io_r(base + ICP_SIERRA_A5_CSR_A5_CONTROL);
	val &= ~(A5_CSR_A5_CPU_EN | A5_CSR_WAKE_UP_EN);
	cam_io_w_mb(val, base + ICP_SIERRA_A5_CSR_A5_CONTROL);

	return 0;
}

static void prepare_boot(struct cam_hw_info *a5_dev,
			struct cam_icp_boot_args *args)
{
	struct cam_a5_device_core_info *core_info = a5_dev->core_info;
	unsigned long flags;

	core_info->fw_buf = args->firmware.iova;
	core_info->fw_kva_addr = args->firmware.kva;
	core_info->fw_buf_len = args->firmware.len;

	spin_lock_irqsave(&a5_dev->hw_lock, flags);
	core_info->irq_cb.data = args->irq_cb.data;
	core_info->irq_cb.cb = args->irq_cb.cb;
	spin_unlock_irqrestore(&a5_dev->hw_lock, flags);
}

static void prepare_shutdown(struct cam_hw_info *a5_dev)
{
	struct cam_a5_device_core_info *core_info = a5_dev->core_info;
	unsigned long flags;

	core_info->fw_buf = 0;
	core_info->fw_kva_addr = 0;
	core_info->fw_buf_len = 0;

	spin_lock_irqsave(&a5_dev->hw_lock, flags);
	core_info->irq_cb.data = NULL;
	core_info->irq_cb.cb = NULL;
	spin_unlock_irqrestore(&a5_dev->hw_lock, flags);
}

static int cam_a5_boot(struct cam_hw_info *a5_dev,
		struct cam_icp_boot_args *args, size_t arg_size)
{
	int rc;

	if (!a5_dev || !args) {
		CAM_ERR(CAM_ICP,
			"invalid args: a5_dev=%pK args=%pK",
			a5_dev, args);
		return -EINVAL;
	}

	if (arg_size != sizeof(struct cam_icp_boot_args)) {
		CAM_ERR(CAM_ICP, "invalid boot args size");
		return -EINVAL;
	}

	prepare_boot(a5_dev, args);

	rc = cam_a5_download_fw(a5_dev);
	if (rc) {
		CAM_ERR(CAM_ICP, "firmware download failed rc=%d", rc);
		goto err;
	}

	rc = cam_a5_power_resume(a5_dev, args->debug_enabled);
	if (rc) {
		CAM_ERR(CAM_ICP, "A5 boot failed rc=%d", rc);
		goto err;
	}

	return 0;
err:
	prepare_shutdown(a5_dev);
	return rc;
}

static int cam_a5_shutdown(struct cam_hw_info *a5_dev)
{
	if (!a5_dev) {
		CAM_ERR(CAM_ICP, "invalid A5 device info");
		return -EINVAL;
	}

	prepare_shutdown(a5_dev);
	cam_a5_power_collapse(a5_dev);
	return 0;
}

irqreturn_t cam_a5_irq(int irq_num, void *data)
{
	struct cam_hw_info *a5_dev = data;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_a5_device_core_info *core_info = NULL;
	struct cam_a5_device_hw_info *hw_info = NULL;
	uint32_t irq_status = 0;
	bool recover = false;

	if (!data) {
		CAM_ERR(CAM_ICP, "Invalid cam_dev_info or query_cap args");
		return IRQ_HANDLED;
	}

	spin_lock(&a5_dev->hw_lock);
	if (a5_dev->hw_state == CAM_HW_STATE_POWER_DOWN) {
		CAM_WARN(CAM_ICP, "ICP HW powered off");
		spin_unlock(&a5_dev->hw_lock);
		return IRQ_HANDLED;
	}
	spin_unlock(&a5_dev->hw_lock);

	soc_info = &a5_dev->soc_info;
	core_info = (struct cam_a5_device_core_info *)a5_dev->core_info;
	hw_info = core_info->a5_hw_info;

	irq_status = cam_io_r_mb(soc_info->reg_map[A5_SIERRA_BASE].mem_base +
				core_info->a5_hw_info->a5_host_int_status);

	cam_io_w_mb(irq_status,
			soc_info->reg_map[A5_SIERRA_BASE].mem_base +
			core_info->a5_hw_info->a5_host_int_clr);

	if ((irq_status & A5_WDT_0) ||
		(irq_status & A5_WDT_1)) {
		CAM_ERR_RATE_LIMIT(CAM_ICP, "watch dog interrupt from A5");
		recover = true;
	}

	spin_lock(&a5_dev->hw_lock);
	if (core_info->irq_cb.cb)
		core_info->irq_cb.cb(core_info->irq_cb.data,
			recover);
	spin_unlock(&a5_dev->hw_lock);

	return IRQ_HANDLED;
}

void cam_a5_irq_raise(void *priv)
{
	struct cam_hw_info *a5_info = priv;

	if (!a5_info) {
		CAM_ERR(CAM_ICP, "invalid A5 device info");
		return;
	}

	cam_io_w_mb(A5_HOSTINT,
		a5_info->soc_info.reg_map[A5_SIERRA_BASE].mem_base +
		ICP_SIERRA_A5_CSR_HOST2ICPINT);
}

void cam_a5_irq_enable(void *priv)
{
	struct cam_hw_info *a5_info = priv;

	if (!a5_info) {
		CAM_ERR(CAM_ICP, "invalid A5 device info");
		return;
	}

	cam_io_w_mb(A5_WDT_WS0EN | A5_A2HOSTINTEN,
		a5_info->soc_info.reg_map[A5_SIERRA_BASE].mem_base +
		ICP_SIERRA_A5_CSR_A2HOSTINTEN);
}

void __iomem *cam_a5_iface_addr(void *priv)
{
	struct cam_hw_info *a5_info = priv;
	void __iomem *base;

	if (!a5_info) {
		CAM_ERR(CAM_ICP, "invalid A5 device info");
		return ERR_PTR(-EINVAL);
	}

	base = a5_info->soc_info.reg_map[A5_SIERRA_BASE].mem_base;

	return base + A5_GEN_PURPOSE_REG_OFFSET;
}

int cam_a5_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info *a5_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_a5_device_core_info *core_info = NULL;
	struct cam_a5_device_hw_info *hw_info = NULL;
	struct a5_soc_info *a5_soc = NULL;
	uint32_t ubwc_ipe_cfg[ICP_UBWC_MAX] = {0};
	uint32_t ubwc_bps_cfg[ICP_UBWC_MAX] = {0};
	uint32_t index = 0;
	int rc = 0, ddr_type = 0;

	if (!device_priv) {
		CAM_ERR(CAM_ICP, "Invalid arguments");
		return -EINVAL;
	}

	if (cmd_type >= CAM_ICP_CMD_MAX) {
		CAM_ERR(CAM_ICP, "Invalid command : %x", cmd_type);
		return -EINVAL;
	}

	soc_info = &a5_dev->soc_info;
	core_info = (struct cam_a5_device_core_info *)a5_dev->core_info;
	hw_info = core_info->a5_hw_info;

	switch (cmd_type) {
	case CAM_ICP_CMD_PROC_SHUTDOWN:
		rc = cam_a5_shutdown(device_priv);
		break;
	case CAM_ICP_CMD_PROC_BOOT:
		rc = cam_a5_boot(device_priv, cmd_args, arg_size);
		break;
	case CAM_ICP_CMD_POWER_COLLAPSE:
		rc = cam_a5_power_collapse(a5_dev);
		break;
	case CAM_ICP_CMD_POWER_RESUME:
		rc = cam_a5_power_resume(a5_dev, *((bool *)cmd_args));
		break;
	case CAM_ICP_SEND_INIT:
		hfi_send_system_cmd(HFI_CMD_SYS_INIT, 0, 0);
		break;

	case CAM_ICP_CMD_PC_PREP:
		hfi_send_system_cmd(HFI_CMD_SYS_PC_PREP, 0, 0);
		break;

	case CAM_ICP_CMD_VOTE_CPAS: {
		struct cam_icp_cpas_vote *cpas_vote = cmd_args;

		if (!cmd_args) {
			CAM_ERR(CAM_ICP, "cmd args NULL");
			return -EINVAL;
		}

		cam_a5_cpas_vote(core_info, cpas_vote);
		break;
	}

	case CAM_ICP_CMD_CPAS_START: {
		struct cam_icp_cpas_vote *cpas_vote = cmd_args;

		if (!cmd_args) {
			CAM_ERR(CAM_ICP, "cmd args NULL");
			return -EINVAL;
		}

		if (!core_info->cpas_start) {
			rc = cam_cpas_start(core_info->cpas_handle,
					&cpas_vote->ahb_vote,
					&cpas_vote->axi_vote);
			core_info->cpas_start = true;
		}
		break;
	}

	case CAM_ICP_CMD_CPAS_STOP:
		if (core_info->cpas_start) {
			cam_cpas_stop(core_info->cpas_handle);
			core_info->cpas_start = false;
		}
		break;
	case CAM_ICP_CMD_UBWC_CFG: {
		struct a5_ubwc_cfg_ext *ubwc_cfg_ext = NULL;
		uint32_t *disable_ubwc_comp;

		a5_soc = soc_info->soc_private;
		if (!a5_soc) {
			CAM_ERR(CAM_ICP, "A5 private soc info is NULL");
			return -EINVAL;
		}

		if (!cmd_args) {
			CAM_ERR(CAM_ICP, "Invalid args");
			return -EINVAL;
		}

		disable_ubwc_comp = (uint32_t *)cmd_args;

		if (a5_soc->ubwc_config_ext) {
			/* Invoke kernel API to determine DDR type */
			ddr_type = cam_get_ddr_type();
			if ((ddr_type == DDR_TYPE_LPDDR5) ||
				(ddr_type == DDR_TYPE_LPDDR5X))
				index = 1;

			ubwc_cfg_ext = &a5_soc->uconfig.ubwc_cfg_ext;
			ubwc_ipe_cfg[0] =
				ubwc_cfg_ext->ubwc_ipe_fetch_cfg[index];
			ubwc_ipe_cfg[1] =
				ubwc_cfg_ext->ubwc_ipe_write_cfg[index];
			ubwc_bps_cfg[0] =
				ubwc_cfg_ext->ubwc_bps_fetch_cfg[index];
			ubwc_bps_cfg[1] =
				ubwc_cfg_ext->ubwc_bps_write_cfg[index];
			if (*disable_ubwc_comp) {
				ubwc_ipe_cfg[1] &= ~CAM_ICP_UBWC_COMP_EN;
				ubwc_bps_cfg[1] &= ~CAM_ICP_UBWC_COMP_EN;
				CAM_DBG(CAM_ICP,
					"Force disable UBWC compression, ubwc_ipe_cfg: 0x%x, ubwc_bps_cfg: 0x%x",
					ubwc_ipe_cfg[1], ubwc_bps_cfg[1]);
			}
			rc = hfi_cmd_ubwc_config_ext(&ubwc_ipe_cfg[0],
					&ubwc_bps_cfg[0]);
		} else {
			rc = hfi_cmd_ubwc_config(a5_soc->uconfig.ubwc_cfg);
		}

		break;
	}
	case CAM_ICP_CMD_CLK_UPDATE: {
		int32_t clk_level = 0;
		struct cam_ahb_vote ahb_vote;

		if (!cmd_args) {
			CAM_ERR(CAM_ICP, "Invalid args");
			return -EINVAL;
		}

		clk_level = *((int32_t *)cmd_args);
		CAM_DBG(CAM_ICP,
			"Update ICP clock to level [%d]", clk_level);
		rc = cam_a5_update_clk_rate(soc_info, clk_level);
		if (rc)
			CAM_ERR(CAM_ICP,
				"Failed to update clk to level: %d rc: %d",
				clk_level, rc);

		ahb_vote.type = CAM_VOTE_ABSOLUTE;
		ahb_vote.vote.level = clk_level;
		cam_cpas_update_ahb_vote(
			core_info->cpas_handle, &ahb_vote);
		break;
	}
	case CAM_ICP_CMD_HW_DUMP: {
		struct cam_icp_hw_dump_args *dump_args = cmd_args;

		rc = cam_a5_fw_dump(dump_args, core_info);
		break;
	}

	case CAM_ICP_CMD_HW_MINI_DUMP: {
		struct cam_icp_hw_dump_args *dump_args = cmd_args;

		rc = cam_a5_fw_mini_dump(dump_args, core_info);
		break;
	}
	default:
		break;
	}

	return rc;
}
