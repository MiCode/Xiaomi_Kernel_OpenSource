/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/elf.h>
#include <media/cam_icp.h>
#include "cam_io_util.h"
#include "cam_a5_hw_intf.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "a5_core.h"
#include "a5_soc.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "hfi_intf.h"
#include "hfi_sys_defs.h"
#include "cam_icp_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"

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

static int32_t cam_icp_validate_fw(const uint8_t *elf)
{
	struct elf32_hdr *elf_hdr;

	if (!elf) {
		CAM_ERR(CAM_ICP, "Invalid params");
		return -EINVAL;
	}

	elf_hdr = (struct elf32_hdr *)elf;

	if (memcmp(elf_hdr->e_ident, ELFMAG, SELFMAG)) {
		CAM_ERR(CAM_ICP, "ICP elf identifier is failed");
		return -EINVAL;
	}

	/* check architecture */
	if (elf_hdr->e_machine != EM_ARM) {
		CAM_ERR(CAM_ICP, "unsupported arch");
		return -EINVAL;
	}

	/* check elf bit format */
	if (elf_hdr->e_ident[EI_CLASS] != ELFCLASS32) {
		CAM_ERR(CAM_ICP, "elf doesn't support 32 bit format");
		return -EINVAL;
	}

	return 0;
}

static int32_t cam_icp_get_fw_size(const uint8_t *elf, uint32_t *fw_size)
{
	int32_t rc = 0;
	int32_t i = 0;
	uint32_t num_prg_hdrs;
	unsigned char *icp_prg_hdr_tbl;
	uint32_t seg_mem_size = 0;
	struct elf32_hdr *elf_hdr;
	struct elf32_phdr *prg_hdr;

	if (!elf || !fw_size) {
		CAM_ERR(CAM_ICP, "invalid args");
		return -EINVAL;
	}

	*fw_size = 0;

	elf_hdr = (struct elf32_hdr *)elf;
	num_prg_hdrs = elf_hdr->e_phnum;
	icp_prg_hdr_tbl = (unsigned char *)elf + elf_hdr->e_phoff;
	prg_hdr = (struct elf32_phdr *)&icp_prg_hdr_tbl[0];

	if (!prg_hdr) {
		CAM_ERR(CAM_ICP, "failed to get elf program header attr");
		return -EINVAL;
	}

	CAM_DBG(CAM_ICP, "num_prg_hdrs = %d", num_prg_hdrs);
	for (i = 0; i < num_prg_hdrs; i++, prg_hdr++) {
		if (prg_hdr->p_flags == 0)
			continue;

		seg_mem_size = (prg_hdr->p_memsz + prg_hdr->p_align - 1) &
					~(prg_hdr->p_align - 1);
		seg_mem_size += prg_hdr->p_vaddr;
		CAM_DBG(CAM_ICP, "memsz:%x align:%x addr:%x seg_mem_size:%x",
			(int)prg_hdr->p_memsz, (int)prg_hdr->p_align,
			(int)prg_hdr->p_vaddr, (int)seg_mem_size);
		if (*fw_size < seg_mem_size)
			*fw_size = seg_mem_size;

	}

	if (*fw_size == 0) {
		CAM_ERR(CAM_ICP, "invalid elf fw file");
		return -EINVAL;
	}

	return rc;
}

static int32_t cam_icp_program_fw(const uint8_t *elf,
		struct cam_a5_device_core_info *core_info)
{
	int32_t rc = 0;
	uint32_t num_prg_hdrs;
	unsigned char *icp_prg_hdr_tbl;
	int32_t i = 0;
	u8 *dest;
	u8 *src;
	struct elf32_hdr *elf_hdr;
	struct elf32_phdr *prg_hdr;

	elf_hdr = (struct elf32_hdr *)elf;
	num_prg_hdrs = elf_hdr->e_phnum;
	icp_prg_hdr_tbl = (unsigned char *)elf + elf_hdr->e_phoff;
	prg_hdr = (struct elf32_phdr *)&icp_prg_hdr_tbl[0];

	if (!prg_hdr) {
		CAM_ERR(CAM_ICP, "failed to get elf program header attr");
		return -EINVAL;
	}

	for (i = 0; i < num_prg_hdrs; i++, prg_hdr++) {
		if (prg_hdr->p_flags == 0)
			continue;

		CAM_DBG(CAM_ICP, "Loading FW header size: %u",
			prg_hdr->p_filesz);
		if (prg_hdr->p_filesz != 0) {
			src = (u8 *)((u8 *)elf + prg_hdr->p_offset);
			dest = (u8 *)(((u8 *)core_info->fw_kva_addr) +
						prg_hdr->p_vaddr);

			memcpy_toio(dest, src, prg_hdr->p_filesz);
		}
	}

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

	rc = request_firmware(&core_info->fw_elf, "CAMERA_ICP.elf", &pdev->dev);
	if (rc) {
		CAM_ERR(CAM_ICP, "Failed to locate fw: %d", rc);
		return rc;
	}

	if (!core_info->fw_elf) {
		CAM_ERR(CAM_ICP, "Invalid elf size");
		rc = -EINVAL;
		goto fw_download_failed;
	}

	fw_start = core_info->fw_elf->data;
	rc = cam_icp_validate_fw(fw_start);
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

	rc = cam_icp_program_fw(fw_start, core_info);
	if (rc) {
		CAM_ERR(CAM_ICP, "fw program is failed");
		goto fw_download_failed;
	}

fw_download_failed:
	release_firmware(core_info->fw_elf);
	return rc;
}

int cam_a5_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *a5_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_a5_device_core_info *core_info = NULL;
	struct cam_icp_cpas_vote cpas_vote;
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

	cpas_vote.ahb_vote.type = CAM_VOTE_ABSOLUTE;
	cpas_vote.ahb_vote.vote.level = CAM_SVS_VOTE;
	cpas_vote.axi_vote.compressed_bw = CAM_ICP_A5_BW_BYTES_VOTE;
	cpas_vote.axi_vote.uncompressed_bw = CAM_ICP_A5_BW_BYTES_VOTE;

	rc = cam_cpas_start(core_info->cpas_handle,
		&cpas_vote.ahb_vote, &cpas_vote.axi_vote);
	if (rc) {
		CAM_ERR(CAM_ICP, "cpass start failed: %d", rc);
		return rc;
	}
	core_info->cpas_start = true;

	rc = cam_a5_enable_soc_resources(soc_info);
	if (rc) {
		CAM_ERR(CAM_ICP, "soc enable is failed: %d", rc);
		if (cam_cpas_stop(core_info->cpas_handle))
			CAM_ERR(CAM_ICP, "cpas stop is failed");
		else
			core_info->cpas_start = false;
	}

	return rc;
}

int cam_a5_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *a5_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_a5_device_core_info *core_info = NULL;
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

	rc = cam_a5_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_ICP, "soc disable is failed: %d", rc);

	if (core_info->cpas_start) {
		if (cam_cpas_stop(core_info->cpas_handle))
			CAM_ERR(CAM_ICP, "cpas stop is failed");
		else
			core_info->cpas_start = false;
	}

	return rc;
}

irqreturn_t cam_a5_irq(int irq_num, void *data)
{
	struct cam_hw_info *a5_dev = data;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_a5_device_core_info *core_info = NULL;
	struct cam_a5_device_hw_info *hw_info = NULL;
	uint32_t irq_status = 0;

	if (!data) {
		CAM_ERR(CAM_ICP, "Invalid cam_dev_info or query_cap args");
		return IRQ_HANDLED;
	}

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
	}

	spin_lock(&a5_dev->hw_lock);
	if (core_info->irq_cb.icp_hw_mgr_cb)
		core_info->irq_cb.icp_hw_mgr_cb(irq_status,
					core_info->irq_cb.data);
	spin_unlock(&a5_dev->hw_lock);

	return IRQ_HANDLED;
}

int cam_a5_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info *a5_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_a5_device_core_info *core_info = NULL;
	struct cam_a5_device_hw_info *hw_info = NULL;
	struct a5_soc_info *a5_soc = NULL;
	unsigned long flags;
	int rc = 0;

	if (!device_priv) {
		CAM_ERR(CAM_ICP, "Invalid arguments");
		return -EINVAL;
	}

	if (cmd_type >= CAM_ICP_A5_CMD_MAX) {
		CAM_ERR(CAM_ICP, "Invalid command : %x", cmd_type);
		return -EINVAL;
	}

	soc_info = &a5_dev->soc_info;
	core_info = (struct cam_a5_device_core_info *)a5_dev->core_info;
	hw_info = core_info->a5_hw_info;

	switch (cmd_type) {
	case CAM_ICP_A5_CMD_FW_DOWNLOAD:
		rc = cam_a5_download_fw(device_priv);
		break;
	case CAM_ICP_A5_CMD_SET_FW_BUF: {
		struct cam_icp_a5_set_fw_buf_info *fw_buf_info = cmd_args;

		if (!cmd_args) {
			CAM_ERR(CAM_ICP, "cmd args NULL");
			return -EINVAL;
		}

		core_info->fw_buf = fw_buf_info->iova;
		core_info->fw_kva_addr = fw_buf_info->kva;
		core_info->fw_buf_len = fw_buf_info->len;

		CAM_DBG(CAM_ICP, "fw buf info = %x %llx %lld",
			core_info->fw_buf, core_info->fw_kva_addr,
			core_info->fw_buf_len);
		break;
	}
	case CAM_ICP_A5_SET_IRQ_CB: {
		struct cam_icp_a5_set_irq_cb *irq_cb = cmd_args;

		if (!cmd_args) {
			CAM_ERR(CAM_ICP, "cmd args NULL");
			return -EINVAL;
		}

		spin_lock_irqsave(&a5_dev->hw_lock, flags);
		core_info->irq_cb.icp_hw_mgr_cb = irq_cb->icp_hw_mgr_cb;
		core_info->irq_cb.data = irq_cb->data;
		spin_unlock_irqrestore(&a5_dev->hw_lock, flags);
		break;
	}

	case CAM_ICP_A5_SEND_INIT:
		hfi_send_system_cmd(HFI_CMD_SYS_INIT, 0, 0);
		break;

	case CAM_ICP_A5_CMD_PC_PREP:
		hfi_send_system_cmd(HFI_CMD_SYS_PC_PREP, 0, 0);
		break;

	case CAM_ICP_A5_CMD_VOTE_CPAS: {
		struct cam_icp_cpas_vote *cpas_vote = cmd_args;

		if (!cmd_args) {
			CAM_ERR(CAM_ICP, "cmd args NULL");
			return -EINVAL;
		}

		cam_a5_cpas_vote(core_info, cpas_vote);
		break;
	}

	case CAM_ICP_A5_CMD_CPAS_START: {
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

	case CAM_ICP_A5_CMD_CPAS_STOP:
		if (core_info->cpas_start) {
			cam_cpas_stop(core_info->cpas_handle);
			core_info->cpas_start = false;
		}
		break;
	case CAM_ICP_A5_CMD_UBWC_CFG:
		a5_soc = soc_info->soc_private;
		if (!a5_soc) {
			CAM_ERR(CAM_ICP, "A5 private soc info is NULL");
			return -EINVAL;
		}
		rc = hfi_cmd_ubwc_config(a5_soc->ubwc_cfg);
		break;
	default:
		break;
	}

	return rc;
}
