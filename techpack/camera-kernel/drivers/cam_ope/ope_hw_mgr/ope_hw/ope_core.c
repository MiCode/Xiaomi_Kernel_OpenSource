// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/iopoll.h>
#include <media/cam_ope.h>

#include "cam_io_util.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "ope_core.h"
#include "ope_soc.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"
#include "ope_hw.h"
#include "ope_dev_intf.h"
#include "cam_cdm_util.h"
#include "ope_bus_rd.h"
#include "ope_bus_wr.h"
#include "cam_compat.h"

static int cam_ope_caps_vote(struct cam_ope_device_core_info *core_info,
	struct cam_ope_dev_bw_update *cpas_vote)
{
	int rc = 0;

	if (cpas_vote->ahb_vote_valid)
		rc = cam_cpas_update_ahb_vote(core_info->cpas_handle,
			&cpas_vote->ahb_vote);
	if (cpas_vote->axi_vote_valid)
		rc = cam_cpas_update_axi_vote(core_info->cpas_handle,
			&cpas_vote->axi_vote);
	if (rc)
		CAM_ERR(CAM_OPE, "cpas vote is failed: %d", rc);

	return rc;
}

int cam_ope_get_hw_caps(void *hw_priv, void *get_hw_cap_args,
	uint32_t arg_size)
{
	struct cam_hw_info *ope_dev = hw_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_ope_device_core_info *core_info = NULL;
	struct ope_hw_ver *ope_hw_ver;
	struct cam_ope_top_reg_val *top_reg_val;

	if (!hw_priv) {
		CAM_ERR(CAM_OPE, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &ope_dev->soc_info;
	core_info = (struct cam_ope_device_core_info *)ope_dev->core_info;

	if ((!soc_info) || (!core_info)) {
		CAM_ERR(CAM_OPE, "soc_info = %x core_info = %x",
			soc_info, core_info);
		return -EINVAL;
	}

	if (!get_hw_cap_args) {
		CAM_ERR(CAM_OPE, "Invalid caps");
		return -EINVAL;
	}

	top_reg_val = core_info->ope_hw_info->ope_hw->top_reg_val;
	ope_hw_ver = get_hw_cap_args;
	ope_hw_ver->hw_type = core_info->hw_type;
	ope_hw_ver->hw_ver.major =
		(core_info->hw_version & top_reg_val->major_mask) >>
		top_reg_val->major_shift;
	ope_hw_ver->hw_ver.minor =
		(core_info->hw_version & top_reg_val->minor_mask) >>
		top_reg_val->minor_shift;
	ope_hw_ver->hw_ver.incr =
		(core_info->hw_version & top_reg_val->incr_mask) >>
		top_reg_val->incr_shift;

	return 0;
}

int cam_ope_start(void *hw_priv, void *start_args, uint32_t arg_size)
{
	return 0;
}

int cam_ope_stop(void *hw_priv, void *start_args, uint32_t arg_size)
{
	struct cam_hw_info *ope_dev = hw_priv;
	struct cam_ope_device_core_info *core_info = NULL;
	int rc = 0;

	if (!hw_priv) {
		CAM_ERR(CAM_OPE, "Invalid cam_dev_info");
		return -EINVAL;
	}

	core_info = (struct cam_ope_device_core_info *)ope_dev->core_info;
	if (!core_info) {
		CAM_ERR(CAM_OPE, "core_info = %pK", core_info);
		return -EINVAL;
	}

	if (core_info->cpas_start) {
		if (cam_cpas_stop(core_info->cpas_handle))
			CAM_ERR(CAM_OPE, "cpas stop is failed");
		else
			core_info->cpas_start = false;
	}

	return rc;
}

int cam_ope_flush(void *hw_priv, void *flush_args, uint32_t arg_size)
{
	return 0;
}

static int cam_ope_dev_process_init(struct ope_hw *ope_hw,
	void *cmd_args)
{
	int rc = 0;

	rc = cam_ope_top_process(ope_hw, 0, OPE_HW_INIT, cmd_args);
	if (rc)
		goto top_init_fail;

	rc = cam_ope_bus_rd_process(ope_hw, 0, OPE_HW_INIT, cmd_args);
		if (rc)
			goto bus_rd_init_fail;

	rc = cam_ope_bus_wr_process(ope_hw, 0, OPE_HW_INIT, cmd_args);
		if (rc)
			goto bus_wr_init_fail;

	return rc;

bus_wr_init_fail:
	rc = cam_ope_bus_rd_process(ope_hw, 0,
		OPE_HW_DEINIT, NULL);
bus_rd_init_fail:
	rc = cam_ope_top_process(ope_hw, 0,
		OPE_HW_DEINIT, NULL);
top_init_fail:
	return rc;
}

static int cam_ope_process_init(struct ope_hw *ope_hw,
	void *cmd_args, bool hfi_en)
{
	if (!hfi_en)
		return cam_ope_dev_process_init(ope_hw, cmd_args);

	CAM_ERR(CAM_OPE, "hfi_en is not supported");
	return -EINVAL;
}

int cam_ope_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *ope_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_ope_device_core_info *core_info = NULL;
	struct cam_ope_cpas_vote *cpas_vote;
	int rc = 0;
	struct cam_ope_dev_init *init;
	struct ope_hw *ope_hw;

	if (!device_priv) {
		CAM_ERR(CAM_OPE, "Invalid cam_dev_info");
		rc = -EINVAL;
		goto end;
	}

	soc_info = &ope_dev->soc_info;
	core_info = (struct cam_ope_device_core_info *)ope_dev->core_info;
	if ((!soc_info) || (!core_info)) {
		CAM_ERR(CAM_OPE, "soc_info = %pK core_info = %pK",
			soc_info, core_info);
		rc = -EINVAL;
		goto end;
	}
	ope_hw = core_info->ope_hw_info->ope_hw;

	cpas_vote = kzalloc(sizeof(struct cam_ope_cpas_vote), GFP_KERNEL);
	if (!cpas_vote) {
		CAM_ERR(CAM_ISP, "Out of memory");
		rc = -ENOMEM;
		goto end;
	}

	cpas_vote->ahb_vote.type = CAM_VOTE_ABSOLUTE;
	cpas_vote->ahb_vote.vote.level = CAM_SVS_VOTE;
	cpas_vote->axi_vote.num_paths = 1;
	cpas_vote->axi_vote.axi_path[0].path_data_type =
		CAM_AXI_PATH_DATA_ALL;
	cpas_vote->axi_vote.axi_path[0].transac_type =
		CAM_AXI_TRANSACTION_WRITE;
	cpas_vote->axi_vote.axi_path[0].camnoc_bw =
		CAM_CPAS_DEFAULT_AXI_BW;
	cpas_vote->axi_vote.axi_path[0].mnoc_ab_bw =
		CAM_CPAS_DEFAULT_AXI_BW;
	cpas_vote->axi_vote.axi_path[0].mnoc_ib_bw =
		CAM_CPAS_DEFAULT_AXI_BW;
	cpas_vote->axi_vote.axi_path[0].ddr_ab_bw =
		CAM_CPAS_DEFAULT_AXI_BW;
	cpas_vote->axi_vote.axi_path[0].ddr_ib_bw =
		CAM_CPAS_DEFAULT_AXI_BW;

	rc = cam_cpas_start(core_info->cpas_handle,
		&cpas_vote->ahb_vote, &cpas_vote->axi_vote);
	if (rc) {
		CAM_ERR(CAM_OPE, "cpass start failed: %d", rc);
		goto free_cpas_vote;
	}
	core_info->cpas_start = true;

	rc = cam_ope_enable_soc_resources(soc_info);
	if (rc)
		goto enable_soc_resource_failed;
	else
		core_info->clk_enable = true;

	init = init_hw_args;

	core_info->ope_hw_info->hfi_en = init->hfi_en;
	init->core_info = core_info;
	rc = cam_ope_process_init(ope_hw, init_hw_args, init->hfi_en);
	if (rc)
		goto process_init_failed;
	else
		goto free_cpas_vote;

process_init_failed:
	if (cam_ope_disable_soc_resources(soc_info, core_info->clk_enable))
		CAM_ERR(CAM_OPE, "disable soc resource failed");
enable_soc_resource_failed:
	if (cam_cpas_stop(core_info->cpas_handle))
		CAM_ERR(CAM_OPE, "cpas stop is failed");
	else
		core_info->cpas_start = false;
free_cpas_vote:
	cam_free_clear((void *)cpas_vote);
	cpas_vote = NULL;
end:
	return rc;
}

int cam_ope_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *ope_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_ope_device_core_info *core_info = NULL;
	int rc = 0;

	if (!device_priv) {
		CAM_ERR(CAM_OPE, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &ope_dev->soc_info;
	core_info = (struct cam_ope_device_core_info *)ope_dev->core_info;
	if ((!soc_info) || (!core_info)) {
		CAM_ERR(CAM_OPE, "soc_info = %pK core_info = %pK",
			soc_info, core_info);
		return -EINVAL;
	}

	rc = cam_ope_disable_soc_resources(soc_info, core_info->clk_enable);
	if (rc)
		CAM_ERR(CAM_OPE, "soc disable is failed : %d", rc);
	core_info->clk_enable = false;

	return rc;
}

static int cam_ope_dev_process_dump_debug_reg(struct ope_hw *ope_hw)
{
	int rc = 0;

	rc = cam_ope_top_process(ope_hw, -1,
		OPE_HW_DUMP_DEBUG, NULL);

	return rc;
}

static int cam_ope_dev_process_reset(struct ope_hw *ope_hw, void *cmd_args)
{
	int rc = 0;

	rc = cam_ope_top_process(ope_hw, -1,
		OPE_HW_RESET, NULL);

	return rc;
}

static int cam_ope_dev_process_release(struct ope_hw *ope_hw, void *cmd_args)
{
	int rc = 0;
	struct cam_ope_dev_release *ope_dev_release;

	ope_dev_release = cmd_args;
	rc = cam_ope_top_process(ope_hw, ope_dev_release->ctx_id,
		OPE_HW_RELEASE, NULL);

	rc |= cam_ope_bus_rd_process(ope_hw, ope_dev_release->ctx_id,
		OPE_HW_RELEASE, NULL);

	rc |= cam_ope_bus_wr_process(ope_hw, ope_dev_release->ctx_id,
		OPE_HW_RELEASE, NULL);

	return rc;
}

static int cam_ope_dev_process_acquire(struct ope_hw *ope_hw, void *cmd_args)
{
	int rc = 0;
	struct cam_ope_dev_acquire *ope_dev_acquire;

	if (!cmd_args || !ope_hw) {
		CAM_ERR(CAM_OPE, "Invalid arguments: %pK %pK",
		cmd_args, ope_hw);
		return -EINVAL;
	}

	ope_dev_acquire = cmd_args;
	rc = cam_ope_top_process(ope_hw, ope_dev_acquire->ctx_id,
		OPE_HW_ACQUIRE, ope_dev_acquire->ope_acquire);
	if (rc)
		goto top_acquire_fail;

	rc = cam_ope_bus_rd_process(ope_hw, ope_dev_acquire->ctx_id,
		OPE_HW_ACQUIRE, ope_dev_acquire->ope_acquire);
	if (rc)
		goto bus_rd_acquire_fail;

	rc = cam_ope_bus_wr_process(ope_hw, ope_dev_acquire->ctx_id,
		OPE_HW_ACQUIRE, ope_dev_acquire->ope_acquire);
	if (rc)
		goto bus_wr_acquire_fail;

	return 0;

bus_wr_acquire_fail:
	cam_ope_bus_rd_process(ope_hw, ope_dev_acquire->ctx_id,
		OPE_HW_RELEASE, ope_dev_acquire->ope_acquire);
bus_rd_acquire_fail:
	cam_ope_top_process(ope_hw, ope_dev_acquire->ctx_id,
		OPE_HW_RELEASE, ope_dev_acquire->ope_acquire);
top_acquire_fail:
	return rc;
}

static int cam_ope_dev_prepare_cdm_request(
	struct cam_ope_hw_mgr *hw_mgr,
	struct cam_hw_prepare_update_args *prepare_args,
	struct cam_ope_ctx *ctx_data, uint32_t req_idx,
	uint32_t kmd_buf_offset,
	struct cam_ope_dev_prepare_req *ope_dev_prepare_req,
	uint32_t len, bool arbitrate)
{
	int i;
	struct cam_ope_request *ope_request;
	struct cam_cdm_bl_request *cdm_cmd;
	uint32_t *kmd_buf;

	ope_request = ctx_data->req_list[req_idx];
	cdm_cmd = ope_request->cdm_cmd;
	kmd_buf = (uint32_t *)ope_request->ope_kmd_buf.cpu_addr +
		kmd_buf_offset;

	cdm_cmd->type = CAM_CDM_BL_CMD_TYPE_MEM_HANDLE;
	cdm_cmd->flag = true;
	cdm_cmd->userdata = ctx_data;
	cdm_cmd->cookie = req_idx;
	cdm_cmd->gen_irq_arb = true;

	i = cdm_cmd->cmd_arrary_count;
	cdm_cmd->cmd[i].bl_addr.mem_handle =
		ope_request->ope_kmd_buf.mem_handle;
	cdm_cmd->cmd[i].offset = kmd_buf_offset +
		ope_request->ope_kmd_buf.offset;
	cdm_cmd->cmd[i].len = len;
	cdm_cmd->cmd[i].arbitrate = arbitrate;
	cdm_cmd->cmd[i].enable_debug_gen_irq = false;

	cdm_cmd->cmd_arrary_count++;

	CAM_DBG(CAM_OPE, "CDM cmd:Req idx = %d req_id = %lld array cnt = %d",
		cdm_cmd->cookie, ope_request->request_id,
		cdm_cmd->cmd_arrary_count);
	CAM_DBG(CAM_OPE, "CDM cmd:mem_hdl = %d offset = %d len = %d, iova 0x%x",
		ope_request->ope_kmd_buf.mem_handle, kmd_buf_offset, len,
		cdm_cmd->cmd[i].bl_addr.hw_iova);

	return 0;
}


static int dump_dmi_cmd(uint32_t print_idx,
	uint32_t *print_ptr, struct cdm_dmi_cmd *dmi_cmd,
	uint32_t *temp)
{
	CAM_DBG(CAM_OPE, "%d:dma_ptr:%x l:%d",
		print_idx, print_ptr,
		dmi_cmd->length);
	CAM_DBG(CAM_OPE, "%d:cmd:%hhx addr:%x",
		print_ptr, dmi_cmd->cmd,
		dmi_cmd->addr);
	CAM_DBG(CAM_OPE, "%d: dmiadr:%x sel:%d",
		print_idx, dmi_cmd->DMIAddr,
		dmi_cmd->DMISel);
	CAM_DBG(CAM_OPE, "%d: %x %x %x",
		print_idx,
		temp[0], temp[1], temp[2]);

	return 0;
}

static int dump_direct_cmd(uint32_t print_idx,
	uint32_t *print_ptr,
	struct ope_frame_process *frm_proc,
	int batch_idx, int cmd_buf_idx)
{
	int len;

	if (cmd_buf_idx >= OPE_MAX_CMD_BUFS ||
		batch_idx >= OPE_MAX_BATCH_SIZE)
		return 0;

	len = frm_proc->cmd_buf[batch_idx][cmd_buf_idx].length / 4;
	CAM_DBG(CAM_OPE, "Frame DB : direct: E");
	for (print_idx = 0; print_idx < len; print_idx++)
		CAM_DBG(CAM_OPE, "%d: %x", print_idx, print_ptr[print_idx]);
	CAM_DBG(CAM_OPE, "Frame DB : direct: X");

	return 0;
}

static int dump_frame_cmd(struct ope_frame_process *frm_proc,
	int i, int j, uint64_t iova_addr, uint32_t *kmd_buf, uint32_t buf_len)
{
	if (j >= OPE_MAX_CMD_BUFS || i >= OPE_MAX_BATCH_SIZE)
		return 0;

	CAM_DBG(CAM_OPE, "Frame DB:scope:%d buffer:%d type:%d",
		frm_proc->cmd_buf[i][j].cmd_buf_scope,
		frm_proc->cmd_buf[i][j].cmd_buf_buffered,
		frm_proc->cmd_buf[i][j].type);
	CAM_DBG(CAM_OPE, "kmdbuf:%x memhdl:%x iova:%x %pK",
		kmd_buf,
		frm_proc->cmd_buf[i][j].mem_handle,
		iova_addr, iova_addr);
	CAM_DBG(CAM_OPE, "buflen:%d len:%d offset:%d",
		buf_len,
		frm_proc->cmd_buf[i][j].length,
		frm_proc->cmd_buf[i][j].offset);

	return 0;
}

static int dump_stripe_cmd(struct ope_frame_process *frm_proc,
	uint32_t stripe_idx, int i, int k, uint64_t iova_addr,
	uint32_t *kmd_buf, uint32_t buf_len)
{
	if (k >= OPE_MAX_CMD_BUFS)
		return 0;

	CAM_DBG(CAM_OPE, "Stripe:%d scope:%d buffer:%d",
		stripe_idx,
		frm_proc->cmd_buf[i][k].cmd_buf_scope,
		frm_proc->cmd_buf[i][k].cmd_buf_buffered);
	CAM_DBG(CAM_OPE, "type:%d kmdbuf:%x memhdl:%x",
		frm_proc->cmd_buf[i][k].type, kmd_buf,
		frm_proc->cmd_buf[i][k].mem_handle);
	CAM_DBG(CAM_OPE, "iova:%x %pK buflen:%d len:%d",
		iova_addr, iova_addr, buf_len,
		frm_proc->cmd_buf[i][k].length);
	CAM_DBG(CAM_OPE, "offset:%d",
		frm_proc->cmd_buf[i][k].offset);
	return 0;
}

static uint32_t *ope_create_frame_cmd_batch(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data, uint32_t req_idx,
	uint32_t *kmd_buf, uint32_t buffered, int batch_idx,
	struct cam_ope_dev_prepare_req *ope_dev_prepare_req)
{
	int rc = 0, i, j;
	uint32_t temp[3];
	struct cam_ope_request *ope_request;
	struct cdm_dmi_cmd *dmi_cmd;
	struct ope_bus_wr_io_port_cdm_info *wr_cdm_info;
	struct ope_bus_rd_io_port_cdm_info *rd_cdm_info;
	struct ope_frame_process *frm_proc;
	dma_addr_t iova_addr;
	uintptr_t cpu_addr;
	size_t buf_len;
	uint32_t print_idx;
	uint32_t *print_ptr;
	int num_dmi = 0;
	struct cam_cdm_utils_ops *cdm_ops;

	frm_proc = ope_dev_prepare_req->frame_process;
	ope_request = ctx_data->req_list[req_idx];
	cdm_ops = ctx_data->ope_cdm.cdm_ops;
	wr_cdm_info =
		&ope_dev_prepare_req->wr_cdm_batch->io_port_cdm[0];
	rd_cdm_info =
		&ope_dev_prepare_req->rd_cdm_batch->io_port_cdm[0];

	if (batch_idx >= OPE_MAX_BATCH_SIZE) {
		CAM_ERR(CAM_OPE, "Invalid input: %d", batch_idx);
		return NULL;
	}
	i = batch_idx;

	for (j = 0; j < frm_proc->num_cmd_bufs[i]; j++) {
		if (frm_proc->cmd_buf[i][j].cmd_buf_scope !=
			OPE_CMD_BUF_SCOPE_FRAME)
			continue;

		if (frm_proc->cmd_buf[i][j].cmd_buf_usage ==
			OPE_CMD_BUF_KMD ||
			frm_proc->cmd_buf[i][j].cmd_buf_usage ==
			OPE_CMD_BUF_DEBUG)
			continue;

		if (frm_proc->cmd_buf[i][j].cmd_buf_buffered !=
			buffered)
			continue;

		if (!frm_proc->cmd_buf[i][j].mem_handle)
			continue;

		rc = cam_mem_get_io_buf(
			frm_proc->cmd_buf[i][j].mem_handle,
			hw_mgr->iommu_cdm_hdl, &iova_addr, &buf_len, NULL);
		if (rc) {
			CAM_ERR(CAM_OPE, "get cmd buf failed %x",
				hw_mgr->iommu_hdl);
			return NULL;
		}
		iova_addr = iova_addr + frm_proc->cmd_buf[i][j].offset;

		rc = cam_mem_get_cpu_buf(
			frm_proc->cmd_buf[i][j].mem_handle,
			&cpu_addr, &buf_len);
		if (rc || !cpu_addr) {
			CAM_ERR(CAM_OPE, "get cmd buf failed %x",
				hw_mgr->iommu_hdl);
			return NULL;
		}

		cpu_addr = cpu_addr + frm_proc->cmd_buf[i][j].offset;
		if (frm_proc->cmd_buf[i][j].type ==
			OPE_CMD_BUF_TYPE_DIRECT) {
			kmd_buf = cdm_ops->cdm_write_indirect(kmd_buf,
				iova_addr,
				frm_proc->cmd_buf[i][j].length);
			print_ptr = (uint32_t *)cpu_addr;
			dump_direct_cmd(print_idx, print_ptr,
				frm_proc, i, j);
		} else {
			num_dmi = frm_proc->cmd_buf[i][j].length /
				sizeof(struct cdm_dmi_cmd);
			CAM_DBG(CAM_OPE, "Frame DB : In direct: E");
			print_ptr = (uint32_t *)cpu_addr;
			for (print_idx = 0;
				print_idx < num_dmi; print_idx++) {
				memcpy(temp, (const void *)print_ptr,
					sizeof(struct cdm_dmi_cmd));
				dmi_cmd = (struct cdm_dmi_cmd *)temp;
				if (!dmi_cmd->addr) {
					CAM_ERR(CAM_OPE, "Null dmi cmd addr");
					return NULL;
				}

				kmd_buf = cdm_ops->cdm_write_dmi(
					kmd_buf,
					0, dmi_cmd->DMIAddr,
					dmi_cmd->DMISel, dmi_cmd->addr,
					dmi_cmd->length);
				if (hw_mgr->frame_dump_enable)
					dump_dmi_cmd(print_idx,
						print_ptr, dmi_cmd, temp);
				print_ptr +=
					sizeof(struct cdm_dmi_cmd) /
					sizeof(uint32_t);
			}
			CAM_DBG(CAM_OPE, "Frame DB : In direct: X");
		}
		if (hw_mgr->frame_dump_enable)
			dump_frame_cmd(frm_proc, i, j,
				iova_addr, kmd_buf, buf_len);
	}
	return kmd_buf;

}

static uint32_t *ope_create_frame_wr(struct cam_ope_ctx *ctx_data,
	struct ope_bus_wr_io_port_cdm_info *wr_cdm_info,
	uint32_t *kmd_buf, struct cam_ope_request *ope_request)
{
	struct cam_cdm_utils_ops *cdm_ops;
	int i;

	cdm_ops = ctx_data->ope_cdm.cdm_ops;

	for (i = 0; i < wr_cdm_info->num_frames_cmds; i++) {
		kmd_buf = cdm_ops->cdm_write_indirect(kmd_buf,
			(uint32_t)ope_request->ope_kmd_buf.iova_cdm_addr +
			wr_cdm_info->f_cdm_info[i].offset,
			wr_cdm_info->f_cdm_info[i].len);
		CAM_DBG(CAM_OPE, "FrameWR:i:%d kmdbuf:%x len:%d iova:%x %pK",
			i, kmd_buf, wr_cdm_info->f_cdm_info[i].len,
			ope_request->ope_kmd_buf.iova_cdm_addr,
			ope_request->ope_kmd_buf.iova_cdm_addr);
	}
	return kmd_buf;
}

static uint32_t *ope_create_frame_rd(struct cam_ope_ctx *ctx_data,
	struct ope_bus_rd_io_port_cdm_info *rd_cdm_info,
	uint32_t *kmd_buf, struct cam_ope_request *ope_request)
{
	struct cam_cdm_utils_ops *cdm_ops;
	int i;

	cdm_ops = ctx_data->ope_cdm.cdm_ops;

	/* Frame 0 RD */
	for (i = 0; i < rd_cdm_info->num_frames_cmds; i++) {
		kmd_buf = cdm_ops->cdm_write_indirect(kmd_buf,
			(uint32_t)ope_request->ope_kmd_buf.iova_cdm_addr +
			rd_cdm_info->f_cdm_info[i].offset,
			rd_cdm_info->f_cdm_info[i].len);
		CAM_DBG(CAM_OPE, "FrameRD:i:%d kmdbuf:%x len:%d iova:%x %pK",
			 i, kmd_buf, rd_cdm_info->f_cdm_info[i].len,
			 ope_request->ope_kmd_buf.iova_cdm_addr,
			 ope_request->ope_kmd_buf.iova_cdm_addr);
	}
	return kmd_buf;
}

static uint32_t *ope_create_frame_cmd(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data, uint32_t req_idx,
	uint32_t *kmd_buf, uint32_t buffered,
	struct cam_ope_dev_prepare_req *ope_dev_prepare_req)
{
	int rc = 0, i, j;
	uint32_t temp[3];
	struct cam_ope_request *ope_request;
	struct cdm_dmi_cmd *dmi_cmd;
	struct ope_bus_wr_io_port_cdm_info *wr_cdm_info;
	struct ope_bus_rd_io_port_cdm_info *rd_cdm_info;
	struct ope_frame_process *frm_proc;
	dma_addr_t iova_addr;
	uintptr_t cpu_addr;
	size_t buf_len;
	uint32_t print_idx;
	uint32_t *print_ptr;
	int num_dmi = 0;
	struct cam_cdm_utils_ops *cdm_ops;

	frm_proc = ope_dev_prepare_req->frame_process;
	ope_request = ctx_data->req_list[req_idx];
	cdm_ops = ctx_data->ope_cdm.cdm_ops;
	wr_cdm_info =
		&ope_dev_prepare_req->wr_cdm_batch->io_port_cdm[0];
	rd_cdm_info =
		&ope_dev_prepare_req->rd_cdm_batch->io_port_cdm[0];

	for (i = 0; i < frm_proc->batch_size; i++) {
		for (j = 0; j < frm_proc->num_cmd_bufs[i]; j++) {
			if (frm_proc->cmd_buf[i][j].cmd_buf_scope !=
				OPE_CMD_BUF_SCOPE_FRAME)
				continue;

			if (frm_proc->cmd_buf[i][j].cmd_buf_usage ==
				OPE_CMD_BUF_KMD ||
				frm_proc->cmd_buf[i][j].cmd_buf_usage ==
				OPE_CMD_BUF_DEBUG)
				continue;

			if (frm_proc->cmd_buf[i][j].cmd_buf_buffered !=
				buffered)
				continue;

			if (!frm_proc->cmd_buf[i][j].mem_handle)
				continue;

			rc = cam_mem_get_io_buf(
				frm_proc->cmd_buf[i][j].mem_handle,
				hw_mgr->iommu_cdm_hdl, &iova_addr, &buf_len, NULL);
			if (rc) {
				CAM_ERR(CAM_OPE, "get cmd buf failed %x",
					hw_mgr->iommu_hdl);
				return NULL;
			}
			iova_addr = iova_addr + frm_proc->cmd_buf[i][j].offset;

			rc = cam_mem_get_cpu_buf(
				frm_proc->cmd_buf[i][j].mem_handle,
				&cpu_addr, &buf_len);
			if (rc || !cpu_addr) {
				CAM_ERR(CAM_OPE, "get cmd buf failed %x",
					hw_mgr->iommu_hdl);
				return NULL;
			}

			cpu_addr = cpu_addr + frm_proc->cmd_buf[i][j].offset;
			if (frm_proc->cmd_buf[i][j].type ==
				OPE_CMD_BUF_TYPE_DIRECT) {
				kmd_buf = cdm_ops->cdm_write_indirect(kmd_buf,
					iova_addr,
					frm_proc->cmd_buf[i][j].length);
				print_ptr = (uint32_t *)cpu_addr;
				if (hw_mgr->frame_dump_enable)
					dump_direct_cmd(print_idx, print_ptr,
						frm_proc, i, j);
			} else {
				num_dmi = frm_proc->cmd_buf[i][j].length /
					sizeof(struct cdm_dmi_cmd);
				CAM_DBG(CAM_OPE, "Frame DB : In direct: E");
				print_ptr = (uint32_t *)cpu_addr;
				for (print_idx = 0;
					print_idx < num_dmi; print_idx++) {
					memcpy(temp, (const void *)print_ptr,
						sizeof(struct cdm_dmi_cmd));
					dmi_cmd = (struct cdm_dmi_cmd *)temp;
					if (!dmi_cmd->addr) {
						CAM_ERR(CAM_OPE,
							"Null dmi cmd addr");
						return NULL;
					}

					kmd_buf = cdm_ops->cdm_write_dmi(
						kmd_buf,
						0, dmi_cmd->DMIAddr,
						dmi_cmd->DMISel, dmi_cmd->addr,
						dmi_cmd->length);
					if (hw_mgr->frame_dump_enable)
						dump_dmi_cmd(print_idx,
							print_ptr, dmi_cmd,
							temp);
					print_ptr +=
						sizeof(struct cdm_dmi_cmd) /
						sizeof(uint32_t);
				}
				CAM_DBG(CAM_OPE, "Frame DB : In direct: X");
			}
			if (hw_mgr->frame_dump_enable)
				dump_frame_cmd(frm_proc, i, j,
					iova_addr, kmd_buf, buf_len);
		}
	}
	return kmd_buf;
}

static uint32_t *ope_create_stripe_cmd(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data,
	uint32_t *kmd_buf,
	int batch_idx,
	int s_idx,
	uint32_t stripe_idx,
	struct ope_frame_process *frm_proc)
{
	int rc = 0, i, j, k;
	uint32_t temp[3];
	struct cdm_dmi_cmd *dmi_cmd;
	dma_addr_t iova_addr;
	uintptr_t cpu_addr;
	size_t buf_len;
	uint32_t print_idx;
	uint32_t *print_ptr;
	int num_dmi = 0;
	struct cam_cdm_utils_ops *cdm_ops;
	uint32_t reg_val_pair[2];
	struct cam_hw_info *ope_dev;
	struct cam_ope_device_core_info *core_info;
	struct ope_hw *ope_hw;
	struct cam_ope_top_reg *top_reg;

	if (s_idx >= OPE_MAX_CMD_BUFS ||
		batch_idx >= OPE_MAX_BATCH_SIZE) {
		CAM_ERR(CAM_OPE, "Invalid inputs: %d %d",
			batch_idx, s_idx);
		return NULL;
	}

	i = batch_idx;
	j = s_idx;
	cdm_ops = ctx_data->ope_cdm.cdm_ops;
	/* cmd buffer stripes */
	for (k = 0; k < frm_proc->num_cmd_bufs[i]; k++) {
		if (frm_proc->cmd_buf[i][k].cmd_buf_scope !=
			OPE_CMD_BUF_SCOPE_STRIPE)
			continue;

		if (frm_proc->cmd_buf[i][k].stripe_idx !=
			stripe_idx)
			continue;

		if (!frm_proc->cmd_buf[i][k].mem_handle)
			continue;

		CAM_DBG(CAM_OPE, "process stripe %d", stripe_idx);
		rc = cam_mem_get_io_buf(frm_proc->cmd_buf[i][k].mem_handle,
			hw_mgr->iommu_cdm_hdl, &iova_addr, &buf_len, NULL);
		if (rc) {
			CAM_DBG(CAM_OPE, "get cmd buf fail %x",
				hw_mgr->iommu_hdl);
			return NULL;
		}
		iova_addr = iova_addr + frm_proc->cmd_buf[i][k].offset;
		rc = cam_mem_get_cpu_buf(frm_proc->cmd_buf[i][k].mem_handle,
			&cpu_addr, &buf_len);
		if (rc || !cpu_addr) {
			CAM_DBG(CAM_OPE, "get cmd buf fail %x",
				hw_mgr->iommu_hdl);
			return NULL;
		}
		cpu_addr = cpu_addr + frm_proc->cmd_buf[i][k].offset;

		if (frm_proc->cmd_buf[i][k].type == OPE_CMD_BUF_TYPE_DIRECT) {
			kmd_buf = cdm_ops->cdm_write_indirect(
				kmd_buf,
				iova_addr,
				frm_proc->cmd_buf[i][k].length);
			print_ptr = (uint32_t *)cpu_addr;
			CAM_DBG(CAM_OPE, "Stripe:%d direct:E",
				stripe_idx);
			if (hw_mgr->frame_dump_enable)
				dump_direct_cmd(print_idx, print_ptr,
					frm_proc, i, k);
			CAM_DBG(CAM_OPE, "Stripe:%d direct:X", stripe_idx);
		} else if (frm_proc->cmd_buf[i][k].type ==
			OPE_CMD_BUF_TYPE_INDIRECT) {
			num_dmi = frm_proc->cmd_buf[i][j].length /
				sizeof(struct cdm_dmi_cmd);
			CAM_DBG(CAM_OPE, "Stripe:%d Indirect:E", stripe_idx);
			print_ptr = (uint32_t *)cpu_addr;
			for (print_idx = 0; print_idx < num_dmi; print_idx++) {
				memcpy(temp, (const void *)print_ptr,
					sizeof(struct cdm_dmi_cmd));
				dmi_cmd = (struct cdm_dmi_cmd *)temp;
				if (!dmi_cmd->addr) {
					CAM_ERR(CAM_OPE, "Null dmi cmd addr");
					return NULL;
				}

				kmd_buf = cdm_ops->cdm_write_dmi(kmd_buf,
					0, dmi_cmd->DMIAddr, dmi_cmd->DMISel,
					dmi_cmd->addr, dmi_cmd->length);
				if (hw_mgr->frame_dump_enable)
					dump_dmi_cmd(print_idx,
						print_ptr, dmi_cmd, temp);
				print_ptr += sizeof(struct cdm_dmi_cmd) /
					sizeof(uint32_t);
			}
			CAM_DBG(CAM_OPE, "Stripe:%d Indirect:X", stripe_idx);
		}

		if (hw_mgr->frame_dump_enable)
			dump_stripe_cmd(frm_proc, stripe_idx, i, k,
				iova_addr, kmd_buf, buf_len);
	}

	ope_dev = hw_mgr->ope_dev_intf[0]->hw_priv;
	core_info = (struct cam_ope_device_core_info *)ope_dev->core_info;
	ope_hw = core_info->ope_hw_info->ope_hw;
	top_reg = ope_hw->top_reg;

	reg_val_pair[0] = top_reg->offset + top_reg->scratch_reg;
	reg_val_pair[1] = stripe_idx;
	kmd_buf = cdm_ops->cdm_write_regrandom(kmd_buf, 1, reg_val_pair);

	return kmd_buf;
}

static uint32_t *ope_create_stripe_wr(struct cam_ope_ctx *ctx_data,
	uint32_t stripe_idx, struct ope_bus_wr_io_port_cdm_info *wr_cdm_info,
	struct cam_ope_request *ope_request, uint32_t *kmd_buf)
{
	struct cam_cdm_utils_ops *cdm_ops;
	int k;

	if (stripe_idx >= OPE_MAX_STRIPES) {
		CAM_ERR(CAM_OPE, "invalid s_idx = %d", stripe_idx);
		return NULL;
	}

	cdm_ops = ctx_data->ope_cdm.cdm_ops;
	for (k = 0; k < wr_cdm_info->num_s_cmd_bufs[stripe_idx]; k++) {
		kmd_buf = cdm_ops->cdm_write_indirect(kmd_buf,
			(uint32_t)ope_request->ope_kmd_buf.iova_cdm_addr +
			wr_cdm_info->s_cdm_info[stripe_idx][k].offset,
			wr_cdm_info->s_cdm_info[stripe_idx][k].len);
		CAM_DBG(CAM_OPE, "WR stripe:%d %d kmdbuf:%x",
			stripe_idx, k, kmd_buf);
		CAM_DBG(CAM_OPE, "offset:%d len:%d iova:%x %pK",
			wr_cdm_info->s_cdm_info[stripe_idx][k].offset,
			wr_cdm_info->s_cdm_info[stripe_idx][k].len,
			ope_request->ope_kmd_buf.iova_cdm_addr,
			ope_request->ope_kmd_buf.iova_cdm_addr);
	}
	return kmd_buf;
}

static uint32_t *ope_create_stripe_rd(struct cam_ope_ctx *ctx_data,
	uint32_t stripe_idx, struct ope_bus_rd_io_port_cdm_info *rd_cdm_info,
	struct cam_ope_request *ope_request, uint32_t *kmd_buf)
{
	struct cam_cdm_utils_ops *cdm_ops;
	int k;

	if (stripe_idx >= OPE_MAX_STRIPES) {
		CAM_ERR(CAM_OPE, "invalid s_idx = %d", stripe_idx);
		return NULL;
	}

	cdm_ops = ctx_data->ope_cdm.cdm_ops;
	for (k = 0; k < rd_cdm_info->num_s_cmd_bufs[stripe_idx]; k++) {
		kmd_buf = cdm_ops->cdm_write_indirect(kmd_buf,
			(uint32_t)ope_request->ope_kmd_buf.iova_cdm_addr +
			rd_cdm_info->s_cdm_info[stripe_idx][k].offset,
			rd_cdm_info->s_cdm_info[stripe_idx][k].len);
		CAM_DBG(CAM_OPE, "WR stripe:%d %d kmdbuf:%x",
			stripe_idx, k, kmd_buf);
		CAM_DBG(CAM_OPE, "offset:%d len:%d iova:%x %pK",
			rd_cdm_info->s_cdm_info[stripe_idx][k].offset,
			rd_cdm_info->s_cdm_info[stripe_idx][k].len,
			ope_request->ope_kmd_buf.iova_cdm_addr,
			ope_request->ope_kmd_buf.iova_cdm_addr);
	}
	return kmd_buf;
}

static uint32_t *ope_create_stripes_batch(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data, uint32_t req_idx,
	uint32_t *kmd_buf, int batch_idx,
	struct cam_ope_dev_prepare_req *ope_dev_prepare_req)
{
	int i, j;
	struct cam_ope_request *ope_request;
	struct ope_bus_wr_io_port_cdm_info *wr_cdm_info;
	struct ope_bus_rd_io_port_cdm_info *rd_cdm_info;
	struct ope_frame_process *frm_proc;
	uint32_t stripe_idx = 0;
	struct cam_cdm_utils_ops *cdm_ops;

	frm_proc = ope_dev_prepare_req->frame_process;
	ope_request = ctx_data->req_list[req_idx];
	cdm_ops = ctx_data->ope_cdm.cdm_ops;

	if (batch_idx >= OPE_MAX_BATCH_SIZE) {
		CAM_ERR(CAM_OPE, "Invalid input: %d", batch_idx);
		return NULL;
	}
	i = batch_idx;
	/* Stripes */

	wr_cdm_info =
		&ope_dev_prepare_req->wr_cdm_batch->io_port_cdm[i];
	rd_cdm_info =
		&ope_dev_prepare_req->rd_cdm_batch->io_port_cdm[i];
	for (j = 0; j < ope_request->num_stripes[i]; j++) {
		/* cmd buffer stripes */
		kmd_buf = ope_create_stripe_cmd(hw_mgr, ctx_data,
			kmd_buf, i, j, stripe_idx, frm_proc);
		if (!kmd_buf)
			goto end;

		/* WR stripes */
		kmd_buf = ope_create_stripe_wr(ctx_data, stripe_idx,
			wr_cdm_info, ope_request, kmd_buf);
		if (!kmd_buf)
			goto end;

		/* RD stripes */
		kmd_buf = ope_create_stripe_rd(ctx_data, stripe_idx,
			rd_cdm_info, ope_request, kmd_buf);
		if (!kmd_buf)
			goto end;

		/* add go command */
		kmd_buf = cdm_ops->cdm_write_indirect(kmd_buf,
		(uint32_t)ope_request->ope_kmd_buf.iova_cdm_addr +
		rd_cdm_info->go_cmd_offset,
		rd_cdm_info->go_cmd_len);

		CAM_DBG(CAM_OPE, "Go cmd for stripe:%d kmd_buf:%x",
			stripe_idx, kmd_buf);
		CAM_DBG(CAM_OPE, "iova:%x %pK",
			ope_request->ope_kmd_buf.iova_cdm_addr,
			ope_request->ope_kmd_buf.iova_cdm_addr);

		/* wait for RUP done */
		kmd_buf = cdm_ops->cdm_write_wait_comp_event(kmd_buf,
			OPE_WAIT_COMP_RUP, 0x0);
		CAM_DBG(CAM_OPE, "wait RUP cmd stripe:%d kmd_buf:%x",
			stripe_idx, kmd_buf);
		stripe_idx++;
	}

end:
	return kmd_buf;
}

static uint32_t *ope_create_stripes(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data, uint32_t req_idx,
	uint32_t *kmd_buf,
	struct cam_ope_dev_prepare_req *ope_dev_prepare_req)
{
	int i, j;
	struct cam_ope_request *ope_request;
	struct ope_bus_wr_io_port_cdm_info *wr_cdm_info;
	struct ope_bus_rd_io_port_cdm_info *rd_cdm_info;
	struct ope_frame_process *frm_proc;
	uint32_t stripe_idx = 0;
	struct cam_cdm_utils_ops *cdm_ops;

	frm_proc = ope_dev_prepare_req->frame_process;
	ope_request = ctx_data->req_list[req_idx];
	cdm_ops = ctx_data->ope_cdm.cdm_ops;

	/* Stripes */
	for (i = 0; i < frm_proc->batch_size; i++) {
		wr_cdm_info =
		&ope_dev_prepare_req->wr_cdm_batch->io_port_cdm[i];
		rd_cdm_info =
		&ope_dev_prepare_req->rd_cdm_batch->io_port_cdm[i];
		for (j = 0; j < ope_request->num_stripes[i]; j++) {
			/* cmd buffer stripes */
			kmd_buf = ope_create_stripe_cmd(hw_mgr, ctx_data,
				kmd_buf, i, j, stripe_idx, frm_proc);
			if (!kmd_buf)
				goto end;

			/* WR stripes */
			kmd_buf = ope_create_stripe_wr(ctx_data, stripe_idx,
				wr_cdm_info, ope_request, kmd_buf);
			if (!kmd_buf)
				goto end;

			/* RD stripes */
			kmd_buf = ope_create_stripe_rd(ctx_data, stripe_idx,
				rd_cdm_info, ope_request, kmd_buf);
			if (!kmd_buf)
				goto end;

			/* add go command */
			kmd_buf = cdm_ops->cdm_write_indirect(kmd_buf,
			(uint32_t)ope_request->ope_kmd_buf.iova_cdm_addr +
			rd_cdm_info->go_cmd_offset,
			rd_cdm_info->go_cmd_len);

			CAM_DBG(CAM_OPE, "Go cmd for stripe:%d kmd_buf:%x",
				stripe_idx, kmd_buf);
			CAM_DBG(CAM_OPE, "iova:%x %pK",
				ope_request->ope_kmd_buf.iova_cdm_addr,
				ope_request->ope_kmd_buf.iova_cdm_addr);

			/* wait for RUP done */
			kmd_buf = cdm_ops->cdm_write_wait_comp_event(kmd_buf,
				OPE_WAIT_COMP_RUP, 0x0);
			CAM_DBG(CAM_OPE, "wait RUP cmd stripe:%d kmd_buf:%x",
				stripe_idx, kmd_buf);
			stripe_idx++;
		}
	}
end:
	return kmd_buf;
}

static uint32_t *ope_create_stripes_nrt(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_ope_ctx *ctx_data, uint32_t req_idx,
	uint32_t *kmd_buf,
	struct cam_ope_dev_prepare_req *ope_dev_prepare_req,
	uint32_t *kmd_buf_offset, uint32_t **cdm_kmd_start_addr)
{
	int i, j;
	struct cam_ope_request *ope_request;
	struct ope_bus_wr_io_port_cdm_info *wr_cdm_info;
	struct ope_bus_rd_io_port_cdm_info *rd_cdm_info;
	struct ope_frame_process *frm_proc;
	uint32_t stripe_idx = 0;
	struct cam_cdm_utils_ops *cdm_ops;
	uint32_t len;
	int num_nrt_stripes, num_arb;

	frm_proc = ope_dev_prepare_req->frame_process;
	ope_request = ctx_data->req_list[req_idx];
	num_nrt_stripes = ctx_data->ope_acquire.nrt_stripes_for_arb;
	num_arb = ope_request->num_stripes[0] /
		ctx_data->ope_acquire.nrt_stripes_for_arb;
	if (ope_request->num_stripes[0] %
		ctx_data->ope_acquire.nrt_stripes_for_arb)
		num_arb++;
	CAM_DBG(CAM_OPE, "Number of ARB for snap: %d", num_arb);
	cdm_ops = ctx_data->ope_cdm.cdm_ops;

	/* Stripes */
	for (i = 0; i < frm_proc->batch_size; i++) {
		wr_cdm_info =
		&ope_dev_prepare_req->wr_cdm_batch->io_port_cdm[i];
		rd_cdm_info =
		&ope_dev_prepare_req->rd_cdm_batch->io_port_cdm[i];
		for (j = 0; j < ope_request->num_stripes[i]; j++) {
			CAM_DBG(CAM_OPE, "num_nrt_stripes = %d num_arb = %d",
				num_nrt_stripes, num_arb);
			if (!num_nrt_stripes) {
				kmd_buf = cdm_ops->cdm_write_wait_comp_event(
					kmd_buf,
					OPE_WAIT_COMP_IDLE, 0x0);
				len = (kmd_buf - *cdm_kmd_start_addr) *
					sizeof(uint32_t);
				cam_ope_dev_prepare_cdm_request(
					ope_dev_prepare_req->hw_mgr,
					ope_dev_prepare_req->prepare_args,
					ope_dev_prepare_req->ctx_data,
					ope_dev_prepare_req->req_idx,
					*kmd_buf_offset, ope_dev_prepare_req,
					len, true);
				*cdm_kmd_start_addr = kmd_buf;
				*kmd_buf_offset += len;
			}
			/* cmd buffer stripes */
			kmd_buf = ope_create_stripe_cmd(hw_mgr, ctx_data,
				kmd_buf, i, j, stripe_idx, frm_proc);
			if (!kmd_buf)
				goto end;

			/* WR stripes */
			kmd_buf = ope_create_stripe_wr(ctx_data, stripe_idx,
				wr_cdm_info, ope_request, kmd_buf);
			if (!kmd_buf)
				goto end;

			/* RD stripes */
			kmd_buf = ope_create_stripe_rd(ctx_data, stripe_idx,
				rd_cdm_info, ope_request, kmd_buf);
			if (!kmd_buf)
				goto end;

			if (!num_nrt_stripes) {
				/* For num_nrt_stripes create CDM BL with ARB */
				/* Add Frame level cmds in this condition */
				/* Frame 0 DB */
				kmd_buf = ope_create_frame_cmd(hw_mgr,
					ctx_data, req_idx,
					kmd_buf, OPE_CMD_BUF_DOUBLE_BUFFERED,
					ope_dev_prepare_req);
				if (!kmd_buf)
					goto end;

				/* Frame 0 SB */
				kmd_buf = ope_create_frame_cmd(hw_mgr,
					ctx_data, req_idx,
					kmd_buf, OPE_CMD_BUF_SINGLE_BUFFERED,
					ope_dev_prepare_req);
				if (!kmd_buf)
					goto end;

				/* Frame 0 WR */
				kmd_buf = ope_create_frame_wr(ctx_data,
					wr_cdm_info, kmd_buf, ope_request);
				if (!kmd_buf)
					goto end;

				/* Frame 0 RD */
				kmd_buf = ope_create_frame_rd(ctx_data,
					rd_cdm_info, kmd_buf, ope_request);
				if (!kmd_buf)
					goto end;
				num_arb--;
				num_nrt_stripes =
				ctx_data->ope_acquire.nrt_stripes_for_arb;
			}
			// add go command
			kmd_buf = cdm_ops->cdm_write_indirect(kmd_buf,
			(uint32_t)ope_request->ope_kmd_buf.iova_cdm_addr +
			rd_cdm_info->go_cmd_offset,
			rd_cdm_info->go_cmd_len);

			CAM_DBG(CAM_OPE, "Go cmd for stripe:%d kmd_buf:%x",
				stripe_idx, kmd_buf);
			CAM_DBG(CAM_OPE, "iova:%x %pK",
				ope_request->ope_kmd_buf.iova_cdm_addr,
				ope_request->ope_kmd_buf.iova_cdm_addr);

			// wait for RUP done
			kmd_buf = cdm_ops->cdm_write_wait_comp_event(kmd_buf,
				OPE_WAIT_COMP_RUP, 0x0);
			CAM_DBG(CAM_OPE, "wait RUP cmd stripe:%d kmd_buf:%x",
				stripe_idx, kmd_buf);
			stripe_idx++;
			num_nrt_stripes--;
		}
	}
end:
	return kmd_buf;
}

static int cam_ope_dev_create_kmd_buf_nrt(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_hw_prepare_update_args *prepare_args,
	struct cam_ope_ctx *ctx_data, uint32_t req_idx,
	uint32_t kmd_buf_offset,
	struct cam_ope_dev_prepare_req *ope_dev_prepare_req)
{
	int rc = 0;
	uint32_t len;
	struct cam_ope_request *ope_request;
	uint32_t *kmd_buf;
	uint32_t *cdm_kmd_start_addr;
	struct ope_bus_wr_io_port_cdm_info *wr_cdm_info;
	struct ope_bus_rd_io_port_cdm_info *rd_cdm_info;
	struct ope_frame_process *frm_proc;
	struct cam_cdm_utils_ops *cdm_ops;

	frm_proc = ope_dev_prepare_req->frame_process;
	ope_request = ctx_data->req_list[req_idx];
	kmd_buf = (uint32_t *)ope_request->ope_kmd_buf.cpu_addr +
		(kmd_buf_offset / sizeof(len));
	cdm_kmd_start_addr = kmd_buf;
	wr_cdm_info =
		&ope_dev_prepare_req->wr_cdm_batch->io_port_cdm[0];
	rd_cdm_info =
		&ope_dev_prepare_req->rd_cdm_batch->io_port_cdm[0];

	cdm_ops = ctx_data->ope_cdm.cdm_ops;

	kmd_buf = cdm_ops->cdm_write_clear_comp_event(kmd_buf,
				OPE_WAIT_COMP_IDLE|OPE_WAIT_COMP_RUP, 0x0);

	/* Frame 0 DB */
	kmd_buf = ope_create_frame_cmd(hw_mgr,
		ctx_data, req_idx,
		kmd_buf, OPE_CMD_BUF_DOUBLE_BUFFERED,
		ope_dev_prepare_req);
	if (!kmd_buf) {
		rc = -EINVAL;
		goto end;
	}

	/* Frame 0 SB */
	kmd_buf = ope_create_frame_cmd(hw_mgr,
		ctx_data, req_idx,
		kmd_buf, OPE_CMD_BUF_SINGLE_BUFFERED,
		ope_dev_prepare_req);
	if (!kmd_buf) {
		rc = -EINVAL;
		goto end;
	}

	/* Frame 0 WR */
	kmd_buf = ope_create_frame_wr(ctx_data,
		wr_cdm_info, kmd_buf, ope_request);
	if (!kmd_buf) {
		rc = -EINVAL;
		goto end;
	}

	/* Frame 0 RD */
	kmd_buf = ope_create_frame_rd(ctx_data,
		rd_cdm_info, kmd_buf, ope_request);
	if (!kmd_buf) {
		rc = -EINVAL;
		goto end;
	}

	/* Stripes */
	kmd_buf = ope_create_stripes_nrt(hw_mgr, ctx_data, req_idx, kmd_buf,
		ope_dev_prepare_req, &kmd_buf_offset, &cdm_kmd_start_addr);
	if (!kmd_buf) {
		rc = -EINVAL;
		goto end;
	}

	/* Last arbitration if there are odd number of stripes */
	/* wait_idle_irq */
	kmd_buf = cdm_ops->cdm_write_wait_comp_event(kmd_buf,
		OPE_WAIT_COMP_IDLE, 0x0);

	/* prepare CDM submit packet */
	len = (kmd_buf - cdm_kmd_start_addr) * sizeof(uint32_t);
	cam_ope_dev_prepare_cdm_request(ope_dev_prepare_req->hw_mgr,
		ope_dev_prepare_req->prepare_args,
		ope_dev_prepare_req->ctx_data, ope_dev_prepare_req->req_idx,
		kmd_buf_offset, ope_dev_prepare_req,
		len, false);
end:
	return rc;
}

static int cam_ope_dev_create_kmd_buf_batch(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_hw_prepare_update_args *prepare_args,
	struct cam_ope_ctx *ctx_data, uint32_t req_idx,
	uint32_t kmd_buf_offset,
	struct cam_ope_dev_prepare_req *ope_dev_prepare_req)
{
	int rc = 0, i;
	uint32_t len;
	struct cam_ope_request *ope_request;
	uint32_t *kmd_buf;
	uint32_t *cdm_kmd_start_addr;
	struct ope_bus_wr_io_port_cdm_info *wr_cdm_info;
	struct ope_bus_rd_io_port_cdm_info *rd_cdm_info;
	struct ope_frame_process *frm_proc;
	struct cam_cdm_utils_ops *cdm_ops;

	frm_proc = ope_dev_prepare_req->frame_process;
	ope_request = ctx_data->req_list[req_idx];
	kmd_buf = (uint32_t *)ope_request->ope_kmd_buf.cpu_addr +
		(kmd_buf_offset / sizeof(len));
	cdm_kmd_start_addr = kmd_buf;
	cdm_ops = ctx_data->ope_cdm.cdm_ops;
	kmd_buf = cdm_ops->cdm_write_clear_comp_event(kmd_buf,
				OPE_WAIT_COMP_IDLE|OPE_WAIT_COMP_RUP, 0x0);

	for (i = 0; i < frm_proc->batch_size; i++) {
		wr_cdm_info =
		&ope_dev_prepare_req->wr_cdm_batch->io_port_cdm[i];
		rd_cdm_info =
		&ope_dev_prepare_req->rd_cdm_batch->io_port_cdm[i];

		/* After second batch DB programming add prefecth dis */
		if (i) {
			kmd_buf =
				cdm_ops->cdm_write_wait_prefetch_disable(
				kmd_buf, 0x0,
				OPE_WAIT_COMP_IDLE, 0x0);
		}

		/* Frame i DB */
		kmd_buf = ope_create_frame_cmd_batch(hw_mgr,
			ctx_data, req_idx,
			kmd_buf, OPE_CMD_BUF_DOUBLE_BUFFERED, i,
			ope_dev_prepare_req);
		if (!kmd_buf) {
			rc = -EINVAL;
			goto end;
		}

		/* Frame i SB */
		kmd_buf = ope_create_frame_cmd_batch(hw_mgr,
			ctx_data, req_idx,
			kmd_buf, OPE_CMD_BUF_SINGLE_BUFFERED, i,
			ope_dev_prepare_req);
		if (!kmd_buf) {
			rc = -EINVAL;
			goto end;
		}

		/* Frame i WR */
		kmd_buf = ope_create_frame_wr(ctx_data,
			wr_cdm_info, kmd_buf, ope_request);
		if (!kmd_buf) {
			rc = -EINVAL;
			goto end;
		}

		/* Frame i RD */
		kmd_buf = ope_create_frame_rd(ctx_data,
			rd_cdm_info, kmd_buf, ope_request);
		if (!kmd_buf) {
			rc = -EINVAL;
			goto end;
		}

		/* Stripe level programming for batch i */
			/* Stripes */
		kmd_buf = ope_create_stripes_batch(hw_mgr, ctx_data, req_idx,
			kmd_buf, i, ope_dev_prepare_req);
		if (!kmd_buf) {
			rc = -EINVAL;
			goto end;
		}
	}

	/* wait_idle_irq */
	kmd_buf = cdm_ops->cdm_write_wait_comp_event(kmd_buf,
			OPE_WAIT_COMP_IDLE, 0x0);

	/* prepare CDM submit packet */
	len = (kmd_buf - cdm_kmd_start_addr) * sizeof(uint32_t);
	cam_ope_dev_prepare_cdm_request(ope_dev_prepare_req->hw_mgr,
		ope_dev_prepare_req->prepare_args,
		ope_dev_prepare_req->ctx_data, ope_dev_prepare_req->req_idx,
		kmd_buf_offset, ope_dev_prepare_req,
		len, false);

end:
	return rc;
}

static int cam_ope_dev_create_kmd_buf(struct cam_ope_hw_mgr *hw_mgr,
	struct cam_hw_prepare_update_args *prepare_args,
	struct cam_ope_ctx *ctx_data, uint32_t req_idx,
	uint32_t kmd_buf_offset,
	struct cam_ope_dev_prepare_req *ope_dev_prepare_req)
{
	int rc = 0;
	uint32_t len;
	struct cam_ope_request *ope_request;
	uint32_t *kmd_buf;
	uint32_t *cdm_kmd_start_addr;
	struct ope_bus_wr_io_port_cdm_info *wr_cdm_info;
	struct ope_bus_rd_io_port_cdm_info *rd_cdm_info;
	struct cam_cdm_utils_ops *cdm_ops;


	if (ctx_data->ope_acquire.dev_type == OPE_DEV_TYPE_OPE_NRT) {
		return cam_ope_dev_create_kmd_buf_nrt(
			ope_dev_prepare_req->hw_mgr,
			ope_dev_prepare_req->prepare_args,
			ope_dev_prepare_req->ctx_data,
			ope_dev_prepare_req->req_idx,
			ope_dev_prepare_req->kmd_buf_offset,
			ope_dev_prepare_req);
	}

	if (ctx_data->ope_acquire.batch_size > 1) {
		return cam_ope_dev_create_kmd_buf_batch(
		ope_dev_prepare_req->hw_mgr,
		ope_dev_prepare_req->prepare_args,
		ope_dev_prepare_req->ctx_data,
		ope_dev_prepare_req->req_idx,
		ope_dev_prepare_req->kmd_buf_offset,
		ope_dev_prepare_req);
	}

	ope_request = ctx_data->req_list[req_idx];
	kmd_buf = (uint32_t *)ope_request->ope_kmd_buf.cpu_addr +
		(kmd_buf_offset / sizeof(len));
	cdm_kmd_start_addr = kmd_buf;
	cdm_ops = ctx_data->ope_cdm.cdm_ops;
	wr_cdm_info =
		&ope_dev_prepare_req->wr_cdm_batch->io_port_cdm[0];
	rd_cdm_info =
		&ope_dev_prepare_req->rd_cdm_batch->io_port_cdm[0];


	CAM_DBG(CAM_OPE, "kmd_buf:%x req_idx:%d req_id:%lld offset:%d",
		kmd_buf, req_idx, ope_request->request_id, kmd_buf_offset);

	kmd_buf = cdm_ops->cdm_write_clear_comp_event(kmd_buf,
				OPE_WAIT_COMP_IDLE|OPE_WAIT_COMP_RUP, 0x0);
	/* Frame 0 DB */
	kmd_buf = ope_create_frame_cmd(hw_mgr,
		ctx_data, req_idx,
		kmd_buf, OPE_CMD_BUF_DOUBLE_BUFFERED,
		ope_dev_prepare_req);
	if (!kmd_buf) {
		rc = -EINVAL;
		goto end;
	}

	/* Frame 0 SB */
	kmd_buf = ope_create_frame_cmd(hw_mgr,
		ctx_data, req_idx,
		kmd_buf, OPE_CMD_BUF_SINGLE_BUFFERED,
		ope_dev_prepare_req);
	if (!kmd_buf) {
		rc = -EINVAL;
		goto end;
	}

	/* Frame 0 WR */
	kmd_buf = ope_create_frame_wr(ctx_data,
		wr_cdm_info, kmd_buf, ope_request);
	if (!kmd_buf) {
		rc = -EINVAL;
		goto end;
	}

	/* Frame 0 RD */
	kmd_buf = ope_create_frame_rd(ctx_data,
		rd_cdm_info, kmd_buf, ope_request);
	if (!kmd_buf) {
		rc = -EINVAL;
		goto end;
	}

	/* Stripes */
	kmd_buf = ope_create_stripes(hw_mgr, ctx_data, req_idx, kmd_buf,
		ope_dev_prepare_req);
	if (!kmd_buf) {
		rc = -EINVAL;
		goto end;
	}

	/* wait_idle_irq */
	kmd_buf = cdm_ops->cdm_write_wait_comp_event(kmd_buf,
			OPE_WAIT_COMP_IDLE, 0x0);

	CAM_DBG(CAM_OPE, "wait for idle IRQ: kmd_buf:%x", kmd_buf);

	/* prepare CDM submit packet */
	len = (kmd_buf - cdm_kmd_start_addr) * sizeof(uint32_t);
	CAM_DBG(CAM_OPE, "kmd_start_addr:%x kmdbuf_addr:%x len:%d",
		cdm_kmd_start_addr, kmd_buf, len);
	cam_ope_dev_prepare_cdm_request(
		ope_dev_prepare_req->hw_mgr,
		ope_dev_prepare_req->prepare_args,
		ope_dev_prepare_req->ctx_data,
		ope_dev_prepare_req->req_idx,
		ope_dev_prepare_req->kmd_buf_offset,
		ope_dev_prepare_req,
		len, false);
end:
	return rc;
}

static int cam_ope_dev_process_prepare(struct ope_hw *ope_hw, void *cmd_args)
{
	int rc = 0;
	struct cam_ope_dev_prepare_req *ope_dev_prepare_req;

	ope_dev_prepare_req = cmd_args;

	rc = cam_ope_top_process(ope_hw, ope_dev_prepare_req->ctx_data->ctx_id,
		OPE_HW_PREPARE, ope_dev_prepare_req);
	if (rc)
		goto end;

	rc = cam_ope_bus_rd_process(ope_hw,
		ope_dev_prepare_req->ctx_data->ctx_id,
		OPE_HW_PREPARE, ope_dev_prepare_req);
	if (rc)
		goto end;

	rc = cam_ope_bus_wr_process(ope_hw,
		ope_dev_prepare_req->ctx_data->ctx_id,
		OPE_HW_PREPARE, ope_dev_prepare_req);
	if (rc)
		goto end;

	rc = cam_ope_dev_create_kmd_buf(ope_dev_prepare_req->hw_mgr,
			ope_dev_prepare_req->prepare_args,
			ope_dev_prepare_req->ctx_data,
			ope_dev_prepare_req->req_idx,
			ope_dev_prepare_req->kmd_buf_offset,
			ope_dev_prepare_req);

end:
	return rc;
}

static int cam_ope_dev_process_probe(struct ope_hw *ope_hw,
	void *cmd_args)
{
	cam_ope_top_process(ope_hw, -1, OPE_HW_PROBE, NULL);
	cam_ope_bus_rd_process(ope_hw, -1, OPE_HW_PROBE, NULL);
	cam_ope_bus_wr_process(ope_hw, -1, OPE_HW_PROBE, NULL);

	return 0;
}

static int cam_ope_process_probe(struct ope_hw *ope_hw,
	void *cmd_args, bool hfi_en)
{
	struct cam_ope_dev_probe *ope_probe = cmd_args;

	if (!ope_probe->hfi_en)
		return cam_ope_dev_process_probe(ope_hw, cmd_args);

	return -EINVAL;
}

static int cam_ope_process_dump_debug_reg(struct ope_hw *ope_hw,
	bool hfi_en)
{
	if (!hfi_en)
		return cam_ope_dev_process_dump_debug_reg(ope_hw);

	return -EINVAL;
}

static int cam_ope_process_reset(struct ope_hw *ope_hw,
	void *cmd_args, bool hfi_en)
{
	if (!hfi_en)
		return cam_ope_dev_process_reset(ope_hw, cmd_args);

	return -EINVAL;
}

static int cam_ope_process_release(struct ope_hw *ope_hw,
	void *cmd_args, bool hfi_en)
{
	if (!hfi_en)
		return cam_ope_dev_process_release(ope_hw, cmd_args);

	return -EINVAL;
}

static int cam_ope_process_acquire(struct ope_hw *ope_hw,
	void *cmd_args, bool hfi_en)
{
	if (!hfi_en)
		return cam_ope_dev_process_acquire(ope_hw, cmd_args);

	return -EINVAL;
}

static int cam_ope_process_prepare(struct ope_hw *ope_hw,
	void *cmd_args, bool hfi_en)
{
	if (!hfi_en)
		return cam_ope_dev_process_prepare(ope_hw, cmd_args);

	return -EINVAL;
}

int cam_ope_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	int rc = 0;
	struct    cam_hw_info *ope_dev = device_priv;
	struct    cam_hw_soc_info *soc_info = NULL;
	struct    cam_ope_device_core_info *core_info = NULL;
	struct    cam_ope_match_pid_args  *match_pid_mid = NULL;
	struct    ope_hw *ope_hw;
	bool      hfi_en;
	unsigned long flags;
	int i;
	uint32_t device_idx;

	if (!device_priv) {
		CAM_ERR(CAM_OPE, "Invalid args %x for cmd %u",
			device_priv, cmd_type);
		return -EINVAL;
	}

	soc_info = &ope_dev->soc_info;
	core_info = (struct cam_ope_device_core_info *)ope_dev->core_info;
	if ((!soc_info) || (!core_info)) {
		CAM_ERR(CAM_OPE, "soc_info = %x core_info = %x",
			soc_info, core_info);
		return -EINVAL;
	}

	hfi_en = core_info->ope_hw_info->hfi_en;
	ope_hw = core_info->ope_hw_info->ope_hw;
	if (!ope_hw) {
		CAM_ERR(CAM_OPE, "Invalid ope hw info");
		return -EINVAL;
	}

	switch (cmd_type) {
	case OPE_HW_PROBE:
		rc = cam_ope_process_probe(ope_hw, cmd_args, hfi_en);
		break;
	case OPE_HW_ACQUIRE:
		rc = cam_ope_process_acquire(ope_hw, cmd_args, hfi_en);
		break;
	case OPE_HW_RELEASE:
		rc = cam_ope_process_release(ope_hw, cmd_args, hfi_en);
		break;
	case OPE_HW_PREPARE:
		rc = cam_ope_process_prepare(ope_hw, cmd_args, hfi_en);
		break;
	case OPE_HW_START:
		break;
	case OPE_HW_STOP:
		break;
	case OPE_HW_FLUSH:
		break;
	case OPE_HW_RESET:
		rc = cam_ope_process_reset(ope_hw, cmd_args, hfi_en);
		break;
	case OPE_HW_CLK_UPDATE: {
		struct cam_ope_dev_clk_update *clk_upd_cmd =
			(struct cam_ope_dev_clk_update *)cmd_args;

		if (core_info->clk_enable == false) {
			rc = cam_soc_util_clk_enable_default(soc_info,
				CAM_SVS_VOTE);
			if (rc) {
				CAM_ERR(CAM_OPE, "Clock enable is failed");
				return rc;
			}
			core_info->clk_enable = true;
		}

		rc = cam_ope_update_clk_rate(soc_info, clk_upd_cmd->clk_rate);
		if (rc)
			CAM_ERR(CAM_OPE, "Failed to update clk: %d", rc);
		}
		break;
	case OPE_HW_CLK_DISABLE: {
		if (core_info->clk_enable == true)
			cam_soc_util_clk_disable_default(soc_info);

		core_info->clk_enable = false;
		}
		break;
	case OPE_HW_BW_UPDATE: {
		struct cam_ope_dev_bw_update *cpas_vote = cmd_args;

		if (!cmd_args)
			return -EINVAL;

		rc = cam_ope_caps_vote(core_info, cpas_vote);
		if (rc)
			CAM_ERR(CAM_OPE, "failed to update bw: %d", rc);
		}
		break;
	case OPE_HW_SET_IRQ_CB: {
		struct cam_ope_set_irq_cb *irq_cb = cmd_args;

		if (!cmd_args) {
			CAM_ERR(CAM_OPE, "cmd args NULL");
			return -EINVAL;
		}

		spin_lock_irqsave(&ope_dev->hw_lock, flags);
		core_info->irq_cb.ope_hw_mgr_cb = irq_cb->ope_hw_mgr_cb;
		core_info->irq_cb.data = irq_cb->data;
		spin_unlock_irqrestore(&ope_dev->hw_lock, flags);
		}
		break;
	case OPE_HW_DUMP_DEBUG:
		rc = cam_ope_process_dump_debug_reg(ope_hw, hfi_en);
		break;
	case OPE_HW_MATCH_PID_MID:
		if (!cmd_args) {
			CAM_ERR(CAM_OPE, "cmd args NULL");
			return -EINVAL;
		}

		match_pid_mid = (struct cam_ope_match_pid_args *)cmd_args;
		match_pid_mid->mid_match_found = false;

		device_idx = match_pid_mid->device_idx;

		for (i = 0; i < MAX_RW_CLIENTS; i++) {
			if ((match_pid_mid->fault_mid ==
				ope_hw->common->ope_mid_info[device_idx][i].mid) &&
				(match_pid_mid->fault_pid ==
				ope_hw->common->ope_mid_info[device_idx][i].pid)) {
				match_pid_mid->match_res =
				ope_hw->common->ope_mid_info[device_idx][i].cam_ope_res_type;
				match_pid_mid->mid_match_found = true;
				break;
			}
		}
		if (!match_pid_mid->mid_match_found) {
			rc = -1;
			CAM_INFO(CAM_OPE, "mid match not found");
		}
		break;
	default:
		break;
	}

	return rc;
}

irqreturn_t cam_ope_irq(int irq_num, void *data)
{
	struct cam_hw_info *ope_dev = data;
	struct cam_ope_device_core_info *core_info = NULL;
	struct ope_hw *ope_hw;
	struct cam_ope_irq_data irq_data;

	if (!data) {
		CAM_ERR(CAM_OPE, "Invalid cam_dev_info or query_cap args");
		return IRQ_HANDLED;
	}

	core_info = (struct cam_ope_device_core_info *)ope_dev->core_info;
	ope_hw = core_info->ope_hw_info->ope_hw;

	irq_data.error = 0;
	cam_ope_top_process(ope_hw, 0, OPE_HW_ISR, &irq_data);
	cam_ope_bus_rd_process(ope_hw, 0, OPE_HW_ISR, &irq_data);
	cam_ope_bus_wr_process(ope_hw, 0, OPE_HW_ISR, &irq_data);


	spin_lock(&ope_dev->hw_lock);
	if (core_info->irq_cb.ope_hw_mgr_cb && core_info->irq_cb.data)
		if (irq_data.error)
			core_info->irq_cb.ope_hw_mgr_cb(irq_data.error,
				core_info->irq_cb.data);
	spin_unlock(&ope_dev->hw_lock);


	return IRQ_HANDLED;
}
