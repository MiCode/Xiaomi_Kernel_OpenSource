/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/ratelimit.h>
#include "cam_mem_mgr_api.h"
#include "cam_req_mgr_workq.h"
#include "ais_vfe_soc.h"
#include "ais_vfe_core.h"
#include "cam_debug_util.h"
#include "ais_isp_trace.h"

/*VFE TOP DEFINITIONS*/
#define AIS_VFE_HW_RESET_HW_AND_REG_VAL       0x00003F9F
#define AIS_VFE_HW_RESET_HW_VAL               0x00003F87

#define AIS_VFE_IRQ_CMD 0x58
#define AIS_VFE_IRQ_MASK0 0x5C
#define AIS_VFE_IRQ_MASK1 0x60
#define AIS_VFE_IRQ_CLEAR0 0x64
#define AIS_VFE_IRQ_CLEAR1 0x68
#define AIS_VFE_IRQ_STATUS0 0x6C
#define AIS_VFE_IRQ_STATUS1 0x70

#define AIS_VFE_STATUS0_RDI_SOF_IRQ_SHFT 27
#define AIS_VFE_STATUS0_RDI_SOF_IRQ_MSK  0xF
#define AIS_VFE_STATUS0_RDI_REGUP_IRQ_SHFT 5
#define AIS_VFE_STATUS0_RDI_REGUP_IRQ_MSK  0xF
#define AIS_VFE_STATUS1_RDI_OVERFLOW_IRQ_SHFT 2
#define AIS_VFE_STATUS1_RDI_OVERFLOW_IRQ_MSK  0xF

#define AIS_VFE_MASK0_RDI 0x780001E0
#define AIS_VFE_MASK1_RDI 0x000000BC

#define AIS_VFE_STATUS0_BUS_WR_IRQ  (1 << 9)
#define AIS_VFE_STATUS0_RDI_SOF_IRQ  (0xF << AIS_VFE_STATUS0_RDI_SOF_IRQ_SHFT)
#define AIS_VFE_STATUS0_RDI_OVERFLOW_IRQ  \
	(0xF << AIS_VFE_STATUS1_RDI_OVERFLOW_IRQ_SHFT)
#define AIS_VFE_STATUS0_RESET_ACK_IRQ  (1 << 31)

#define AIS_VFE_REGUP_RDI_SHIFT 1
#define AIS_VFE_REGUP_RDI_ALL 0x1E

/*Allow max of 4 HW FIFO Q + 2 delayed buffers before error*/
#define MAX_NUM_BUF_SW_FIFOQ_ERR 6

/*VFE BUS DEFINITIONS*/
#define AIS_VFE_BUS_SET_DEBUG_REG                0x82

#define AIS_VFE_RDI_BUS_DEFAULT_WIDTH               0xFF01
#define AIS_VFE_RDI_BUS_DEFAULT_STRIDE              0xFF01

#define AIS_VFE_BUS_INTRA_CLIENT_MASK               0x3
#define AIS_VFE_BUS_ADDR_SYNC_INTRA_CLIENT_SHIFT    8
#define AIS_VFE_BUS_VER2_MAX_CLIENTS 24
#define AIS_VFE_BUS_ADDR_NO_SYNC_DEFAULT_VAL \
	((1 << AIS_VFE_BUS_VER2_MAX_CLIENTS) - 1)


static void ais_clear_rdi_path(struct ais_vfe_rdi_output *rdi_path)
{
	int i;

	rdi_path->frame_cnt = 0;

	rdi_path->num_buffer_hw_q = 0;
	INIT_LIST_HEAD(&rdi_path->buffer_q);
	INIT_LIST_HEAD(&rdi_path->buffer_hw_q);
	INIT_LIST_HEAD(&rdi_path->free_buffer_list);
	for (i = 0; i < AIS_VFE_MAX_BUF; i++) {
		INIT_LIST_HEAD(&rdi_path->buffers[i].list);
		list_add_tail(&rdi_path->buffers[i].list,
				&rdi_path->free_buffer_list);
	}

	memset(&rdi_path->last_sof_info, 0, sizeof(rdi_path->last_sof_info));

	rdi_path->num_sof_info_q = 0;
	INIT_LIST_HEAD(&rdi_path->sof_info_q);
	INIT_LIST_HEAD(&rdi_path->free_sof_info_list);
	for (i = 0; i < AIS_VFE_MAX_SOF_INFO; i++) {
		INIT_LIST_HEAD(&rdi_path->sof_info[i].list);
		list_add_tail(&rdi_path->sof_info[i].list,
				&rdi_path->free_sof_info_list);
	}
}

static int ais_vfe_bus_hw_init(struct ais_vfe_hw_core_info *core_info)
{
	struct ais_vfe_bus_ver2_hw_info   *bus_hw_info = NULL;
	struct ais_irq_register_set  *bus_hw_irq_regs = NULL;

	bus_hw_info = core_info->vfe_hw_info->bus_hw_info;
	bus_hw_irq_regs = bus_hw_info->common_reg.irq_reg_info.irq_reg_set;

	/*set IRQ mask for BUS WR*/
	core_info->irq_mask0 |= AIS_VFE_STATUS0_BUS_WR_IRQ;
	cam_io_w_mb(core_info->irq_mask0,
		core_info->mem_base + AIS_VFE_IRQ_MASK0);

	cam_io_w_mb(0x7801,
		core_info->mem_base + bus_hw_irq_regs[0].mask_reg_offset);
	cam_io_w_mb(0x0,
		core_info->mem_base + bus_hw_irq_regs[1].mask_reg_offset);
	cam_io_w_mb(0x0,
		core_info->mem_base + bus_hw_irq_regs[2].mask_reg_offset);

	/*Set Debug Registers*/
	cam_io_w_mb(AIS_VFE_BUS_SET_DEBUG_REG, core_info->mem_base +
		bus_hw_info->common_reg.debug_status_cfg);

	/* BUS_WR_INPUT_IF_ADDR_SYNC_FRAME_HEADER */
	cam_io_w_mb(0x0, core_info->mem_base +
		bus_hw_info->common_reg.addr_sync_frame_hdr);

	/* no clock gating at bus input */
	cam_io_w_mb(0xFFFFF, core_info->mem_base + 0x0000200C);

	/* BUS_WR_TEST_BUS_CTRL */
	cam_io_w_mb(0x0, core_info->mem_base + 0x0000211C);

	/* if addr_no_sync has default value then config the addr no sync reg */
	cam_io_w_mb(AIS_VFE_BUS_ADDR_NO_SYNC_DEFAULT_VAL,
		core_info->mem_base +
		bus_hw_info->common_reg.addr_sync_no_sync);

	return 0;
}

static int ais_vfe_bus_hw_deinit(struct ais_vfe_hw_core_info *core_info)
{
	struct ais_vfe_bus_ver2_hw_info   *bus_hw_info = NULL;
	struct ais_irq_register_set  *bus_hw_irq_regs = NULL;

	bus_hw_info = core_info->vfe_hw_info->bus_hw_info;
	bus_hw_irq_regs = bus_hw_info->common_reg.irq_reg_info.irq_reg_set;

	/*set IRQ mask for BUS WR*/
	core_info->irq_mask0 &= ~AIS_VFE_STATUS0_BUS_WR_IRQ;

	cam_io_w_mb(0x7800,
		core_info->mem_base + bus_hw_irq_regs[0].mask_reg_offset);
	cam_io_w_mb(0x0,
		core_info->mem_base + bus_hw_irq_regs[1].mask_reg_offset);
	cam_io_w_mb(0x0,
		core_info->mem_base + bus_hw_irq_regs[2].mask_reg_offset);

	return 0;
}

int ais_vfe_get_hw_caps(void *hw_priv, void *get_hw_cap_args, uint32_t arg_size)
{
	struct cam_hw_info                *vfe_dev = hw_priv;
	struct ais_vfe_hw_core_info       *core_info = NULL;
	int rc = 0;

	CAM_DBG(CAM_ISP, "Enter");
	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	core_info = (struct ais_vfe_hw_core_info *)vfe_dev->core_info;

	CAM_WARN(CAM_ISP, "VFE%d get_hw_caps not implemented",
			core_info->vfe_idx);

	rc = -EPERM;

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

static int ais_vfe_reset(void *hw_priv,
	void *reset_core_args, uint32_t arg_size)
{
	struct cam_hw_info                *vfe_hw  = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct ais_vfe_hw_core_info       *core_info = NULL;
	struct ais_vfe_top_ver2_hw_info   *top_hw_info = NULL;
	uint32_t *reset_reg_args = reset_core_args;
	uint32_t  reset_reg_val;
	int rc = 0;
	int i;

	CAM_DBG(CAM_ISP, "Enter");

	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	switch (*reset_reg_args) {
	case AIS_VFE_HW_RESET_HW_AND_REG:
		reset_reg_val = AIS_VFE_HW_RESET_HW_AND_REG_VAL;
		break;
	default:
		reset_reg_val = AIS_VFE_HW_RESET_HW_VAL;
		break;
	}

	soc_info = &vfe_hw->soc_info;
	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;
	top_hw_info = core_info->vfe_hw_info->top_hw_info;

	cam_io_w_mb(AIS_VFE_STATUS0_RESET_ACK_IRQ,
		core_info->mem_base + AIS_VFE_IRQ_MASK0);
	cam_io_w_mb(0x0, core_info->mem_base + AIS_VFE_IRQ_MASK1);

	reinit_completion(&vfe_hw->hw_complete);

	CAM_DBG(CAM_ISP, "calling RESET on vfe %d", soc_info->index);

	/* Reset HW */
	cam_io_w_mb(reset_reg_val,
		core_info->mem_base +
		top_hw_info->common_reg->global_reset_cmd);

	CAM_DBG(CAM_ISP, "waiting for vfe reset complete");

	/* Wait for Completion or Timeout of 500ms */
	rc = wait_for_completion_timeout(&vfe_hw->hw_complete,
					msecs_to_jiffies(500));
	if (rc) {
		rc = 0;
	} else {
		CAM_ERR(CAM_ISP, "Error! Reset Timeout");
		rc = EFAULT;
	}

	CAM_DBG(CAM_ISP, "reset complete done (%d)", rc);

	core_info->irq_mask0 = 0x0;
	cam_io_w_mb(0x0, core_info->mem_base + AIS_VFE_IRQ_MASK0);

	for (i = 0; i < AIS_IFE_PATH_MAX; i++)
		ais_clear_rdi_path(&core_info->rdi_out[i]);

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

int ais_vfe_init_hw(void *hw_priv, void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info                *vfe_hw = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct ais_vfe_hw_core_info       *core_info = NULL;
	int rc = 0;
	uint32_t reset_core_args = AIS_VFE_HW_RESET_HW_AND_REG;

	CAM_DBG(CAM_ISP, "Enter");
	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	mutex_lock(&vfe_hw->hw_mutex);
	vfe_hw->open_count++;
	if (vfe_hw->open_count > 1) {
		mutex_unlock(&vfe_hw->hw_mutex);
		CAM_DBG(CAM_ISP, "VFE has already been initialized cnt %d",
			vfe_hw->open_count);
		return 0;
	}
	mutex_unlock(&vfe_hw->hw_mutex);

	soc_info = &vfe_hw->soc_info;
	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;

	/* Turn ON Regulators, Clocks and other SOC resources */
	rc = ais_vfe_enable_soc_resources(soc_info);
	if (rc) {
		CAM_ERR(CAM_ISP, "Enable SOC failed");
		rc = -EFAULT;
		goto decrement_open_cnt;
	}

	CAM_DBG(CAM_ISP, "Enable soc done");

	/* Do HW Reset */
	rc = ais_vfe_reset(hw_priv, &reset_core_args, sizeof(uint32_t));
	if (rc) {
		CAM_ERR(CAM_ISP, "Reset Failed rc=%d", rc);
		goto disable_soc;
	}

	rc = ais_vfe_bus_hw_init(core_info);
	if (rc) {
		CAM_ERR(CAM_ISP, "Reset Failed rc=%d", rc);
		goto disable_soc;
	}

	vfe_hw->hw_state = CAM_HW_STATE_POWER_UP;
	return rc;

disable_soc:
	ais_vfe_disable_soc_resources(soc_info);
decrement_open_cnt:
	mutex_lock(&vfe_hw->hw_mutex);
	vfe_hw->open_count--;
	mutex_unlock(&vfe_hw->hw_mutex);
	return rc;
}

int ais_vfe_deinit_hw(void *hw_priv, void *deinit_hw_args, uint32_t arg_size)
{
	struct cam_hw_info                *vfe_hw = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct ais_vfe_hw_core_info       *core_info = NULL;
	int rc = 0;
	uint32_t                           reset_core_args =
					AIS_VFE_HW_RESET_HW_AND_REG;

	CAM_DBG(CAM_ISP, "Enter");
	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	mutex_lock(&vfe_hw->hw_mutex);
	if (!vfe_hw->open_count) {
		mutex_unlock(&vfe_hw->hw_mutex);
		CAM_ERR(CAM_ISP, "Error! Unbalanced deinit");
		return -EFAULT;
	}
	vfe_hw->open_count--;
	if (vfe_hw->open_count) {
		mutex_unlock(&vfe_hw->hw_mutex);
		CAM_DBG(CAM_ISP, "open_cnt non-zero =%d", vfe_hw->open_count);
		return 0;
	}
	mutex_unlock(&vfe_hw->hw_mutex);

	soc_info = &vfe_hw->soc_info;
	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;

	rc = ais_vfe_bus_hw_deinit(core_info);
	if (rc)
		CAM_ERR(CAM_ISP, "Bus HW deinit Failed rc=%d", rc);

	rc = ais_vfe_reset(hw_priv, &reset_core_args, sizeof(uint32_t));

	/* Turn OFF Regulators, Clocks and other SOC resources */
	CAM_DBG(CAM_ISP, "Disable SOC resource");
	rc = ais_vfe_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_ISP, "Disable SOC failed");

	vfe_hw->hw_state = CAM_HW_STATE_POWER_DOWN;

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

int ais_vfe_force_reset(void *hw_priv, void *reset_core_args, uint32_t arg_size)
{
	struct cam_hw_info                *vfe_hw = hw_priv;
	bool require_deinit = false;
	int rc = 0;

	mutex_lock(&vfe_hw->hw_mutex);
	if (vfe_hw->open_count) {
		vfe_hw->open_count = 1;
		require_deinit = true;

	}
	mutex_unlock(&vfe_hw->hw_mutex);

	if (require_deinit) {
		CAM_INFO(CAM_ISP, "vfe deinit HW");
		rc = ais_vfe_deinit_hw(vfe_hw, NULL, 0);
	}

	CAM_DBG(CAM_ISP, "Exit (%d)", rc);

	return rc;
}

void ais_isp_hw_get_timestamp(struct ais_isp_timestamp *time_stamp)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);
	time_stamp->mono_time.tv_sec    = ts.tv_sec;
	time_stamp->mono_time.tv_usec   = ts.tv_nsec/1000;
	time_stamp->time_usecs =  ts.tv_sec * 1000000 +
				time_stamp->mono_time.tv_usec;
}


int ais_vfe_reserve(void *hw_priv, void *reserve_args, uint32_t arg_size)
{
	struct ais_vfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *vfe_hw  = hw_priv;
	struct ais_vfe_rdi_output         *rdi_path = NULL;
	struct ais_ife_rdi_init_args      *rdi_cfg;
	struct ais_vfe_bus_ver2_hw_info   *bus_hw_info = NULL;
	struct ais_vfe_bus_ver2_reg_offset_bus_client  *client_regs = NULL;
	int rc = 0;

	if (!hw_priv || !reserve_args || (arg_size !=
		sizeof(struct ais_ife_rdi_init_args))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	rdi_cfg = (struct ais_ife_rdi_init_args *)reserve_args;
	if (rdi_cfg->path >= AIS_IFE_PATH_MAX) {
		CAM_ERR(CAM_ISP, "Invalid output path %d", rdi_cfg->path);
		return -EINVAL;
	}

	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;
	rdi_path = &core_info->rdi_out[rdi_cfg->path];
	bus_hw_info = core_info->vfe_hw_info->bus_hw_info;
	client_regs = &bus_hw_info->bus_client_reg[rdi_cfg->path];

	CAM_DBG(CAM_ISP, "Config RDI%d", rdi_cfg->path);

	mutex_lock(&vfe_hw->hw_mutex);

	if (rdi_path->state >= AIS_ISP_RESOURCE_STATE_INIT_HW) {
		CAM_ERR(CAM_ISP, "RDI%d invalid state %d", rdi_cfg->path,
				rdi_path->state);
		rc = -EINVAL;
		goto EXIT;
	}

	rdi_path->secure_mode = rdi_cfg->out_cfg.secure_mode;

	cam_io_w(0xf, core_info->mem_base + client_regs->burst_limit);
	/*disable pack as it is done in CSID*/
	cam_io_w(0x0, core_info->mem_base + client_regs->packer_cfg);

	/*frame based mode*/
	if (rdi_cfg->out_cfg.mode == 1) {
		cam_io_w_mb(AIS_VFE_RDI_BUS_DEFAULT_WIDTH,
			core_info->mem_base + client_regs->buffer_width_cfg);
		cam_io_w(0x0,
			core_info->mem_base + client_regs->buffer_height_cfg);
		cam_io_w_mb(AIS_VFE_RDI_BUS_DEFAULT_STRIDE,
			core_info->mem_base + client_regs->stride);
		cam_io_w_mb(0x0,
			core_info->mem_base + client_regs->frame_inc);
		rdi_path->en_cfg = 0x3;

	} else {
		cam_io_w_mb(rdi_cfg->out_cfg.width,
			core_info->mem_base + client_regs->buffer_width_cfg);
		cam_io_w(rdi_cfg->out_cfg.height,
			core_info->mem_base + client_regs->buffer_height_cfg);
		cam_io_w_mb(rdi_cfg->out_cfg.stride,
			core_info->mem_base + client_regs->stride);
		cam_io_w_mb(rdi_cfg->out_cfg.frame_increment,
			core_info->mem_base + client_regs->frame_inc);
		rdi_path->en_cfg = 0x1;
	}

	cam_io_w_mb(rdi_cfg->out_cfg.frame_drop_period,
		core_info->mem_base + client_regs->framedrop_period);
	cam_io_w_mb(rdi_cfg->out_cfg.frame_drop_pattern,
		core_info->mem_base + client_regs->framedrop_pattern);

	rdi_path->state = AIS_ISP_RESOURCE_STATE_INIT_HW;

EXIT:
	mutex_unlock(&vfe_hw->hw_mutex);

	return rc;
}


int ais_vfe_release(void *hw_priv, void *release_args, uint32_t arg_size)
{
	struct ais_vfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *vfe_hw  = hw_priv;
	struct ais_vfe_rdi_output         *rdi_path = NULL;
	struct ais_ife_rdi_deinit_args    *deinit_cmd;

	int rc = 0;

	if (!hw_priv || !release_args ||
		(arg_size != sizeof(struct ais_ife_rdi_deinit_args))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	deinit_cmd = (struct ais_ife_rdi_deinit_args *)release_args;

	if (deinit_cmd->path >= AIS_IFE_PATH_MAX) {
		CAM_ERR(CAM_ISP, "Invalid output path %d", deinit_cmd->path);
		return -EINVAL;
	}

	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;
	rdi_path = &core_info->rdi_out[deinit_cmd->path];

	mutex_lock(&vfe_hw->hw_mutex);

	if (rdi_path->state < AIS_ISP_RESOURCE_STATE_INIT_HW) {
		CAM_ERR(CAM_ISP, "RDI%d invalid state %d", deinit_cmd->path,
				rdi_path->state);
		rc = -EINVAL;
		goto EXIT;
	}

	rdi_path->state = AIS_ISP_RESOURCE_STATE_AVAILABLE;

EXIT:
	mutex_unlock(&vfe_hw->hw_mutex);

	return rc;
}


int ais_vfe_start(void *hw_priv, void *start_args, uint32_t arg_size)
{
	struct ais_vfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *vfe_hw  = hw_priv;
	struct ais_ife_rdi_start_args     *start_cmd;
	struct ais_vfe_rdi_output         *rdi_path;
	struct ais_vfe_top_ver2_hw_info   *top_hw_info = NULL;
	struct ais_vfe_bus_ver2_hw_info   *bus_hw_info = NULL;
	struct ais_irq_register_set       *bus_hw_irq_regs = NULL;
	struct ais_vfe_bus_ver2_reg_offset_bus_client  *client_regs = NULL;
	int rc = 0;

	if (!hw_priv || !start_args ||
		(arg_size != sizeof(struct ais_ife_rdi_start_args))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	start_cmd = (struct ais_ife_rdi_start_args *)start_args;
	if (start_cmd->path >= AIS_IFE_PATH_MAX) {
		CAM_ERR(CAM_ISP, "Invalid output path %d", start_cmd->path);
		return -EINVAL;
	}

	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;
	rdi_path = &core_info->rdi_out[start_cmd->path];
	top_hw_info = core_info->vfe_hw_info->top_hw_info;
	bus_hw_info = core_info->vfe_hw_info->bus_hw_info;
	bus_hw_irq_regs = bus_hw_info->common_reg.irq_reg_info.irq_reg_set;
	client_regs = &bus_hw_info->bus_client_reg[start_cmd->path];

	mutex_lock(&vfe_hw->hw_mutex);

	if (rdi_path->state != AIS_ISP_RESOURCE_STATE_INIT_HW) {
		CAM_ERR(CAM_ISP, "RDI%d invalid state %d", start_cmd->path,
				rdi_path->state);
		rc = -EINVAL;
		goto EXIT;
	}

	/*Enable bus WR mask*/
	core_info->bus_wr_mask1 |= (1 << start_cmd->path);
	cam_io_w_mb(core_info->bus_wr_mask1,
		core_info->mem_base + bus_hw_irq_regs[1].mask_reg_offset);

	/*Update VFE mask*/
	core_info->irq_mask0 |= AIS_VFE_MASK0_RDI;
	core_info->irq_mask1 |= AIS_VFE_MASK1_RDI;
	cam_io_w_mb(core_info->irq_mask0,
		core_info->mem_base + AIS_VFE_IRQ_MASK0);
	cam_io_w_mb(core_info->irq_mask1,
		core_info->mem_base + AIS_VFE_IRQ_MASK1);

	/* Enable WM and reg-update*/
	cam_io_w_mb(rdi_path->en_cfg, core_info->mem_base + client_regs->cfg);
	cam_io_w_mb(AIS_VFE_REGUP_RDI_ALL, core_info->mem_base +
			top_hw_info->common_reg->reg_update_cmd);

	rdi_path->state = AIS_ISP_RESOURCE_STATE_STREAMING;

EXIT:
	mutex_unlock(&vfe_hw->hw_mutex);

	return rc;
}

int ais_vfe_stop(void *hw_priv, void *stop_args, uint32_t arg_size)
{
	struct ais_vfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *vfe_hw  = hw_priv;
	struct ais_ife_rdi_stop_args      *stop_cmd;
	struct ais_vfe_rdi_output         *rdi_path;
	struct ais_vfe_top_ver2_hw_info   *top_hw_info = NULL;
	struct ais_vfe_bus_ver2_hw_info   *bus_hw_info = NULL;
	struct ais_irq_register_set       *bus_hw_irq_regs = NULL;
	struct ais_vfe_bus_ver2_reg_offset_bus_client  *client_regs = NULL;
	int rc = 0;

	if (!hw_priv || !stop_args ||
		(arg_size != sizeof(struct ais_ife_rdi_stop_args))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;
	stop_cmd = (struct ais_ife_rdi_stop_args  *)stop_args;

	if (stop_cmd->path >= AIS_IFE_PATH_MAX) {
		CAM_ERR(CAM_ISP, "Invalid output path %d", stop_cmd->path);
		return -EINVAL;
	}

	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;
	rdi_path = &core_info->rdi_out[stop_cmd->path];
	top_hw_info = core_info->vfe_hw_info->top_hw_info;
	bus_hw_info = core_info->vfe_hw_info->bus_hw_info;
	bus_hw_irq_regs = bus_hw_info->common_reg.irq_reg_info.irq_reg_set;
	client_regs = &bus_hw_info->bus_client_reg[stop_cmd->path];

	mutex_lock(&vfe_hw->hw_mutex);

	if (rdi_path->state != AIS_ISP_RESOURCE_STATE_STREAMING &&
		rdi_path->state != AIS_ISP_RESOURCE_STATE_ERROR) {
		CAM_ERR(CAM_ISP, "RDI%d invalid state %d", stop_cmd->path,
				rdi_path->state);
		rc = -EINVAL;
		goto EXIT;
	}

	rdi_path->state = AIS_ISP_RESOURCE_STATE_INIT_HW;

	core_info->bus_wr_mask1 &= ~(1 << stop_cmd->path);
	cam_io_w_mb(core_info->bus_wr_mask1,
		core_info->mem_base + bus_hw_irq_regs[1].mask_reg_offset);

	/* Disable WM and reg-update */
	cam_io_w_mb(0x0, core_info->mem_base + client_regs->cfg);
	cam_io_w_mb(AIS_VFE_REGUP_RDI_ALL, core_info->mem_base +
			top_hw_info->common_reg->reg_update_cmd);

	/* issue bus wr reset and wait for reset ack */
	reinit_completion(&vfe_hw->hw_complete);

	cam_io_w_mb((1 << stop_cmd->path), core_info->mem_base +
			bus_hw_info->common_reg.sw_reset);

	/* Wait for completion or timeout of 50ms */
	rc = wait_for_completion_timeout(&vfe_hw->hw_complete,
					msecs_to_jiffies(50));
	if (rc)
		rc = 0;
	else
		CAM_WARN(CAM_ISP, "Reset Bus WR timeout");

	ais_clear_rdi_path(rdi_path);

EXIT:
	mutex_unlock(&vfe_hw->hw_mutex);

	return rc;
}

int ais_vfe_read(void *hw_priv, void *read_args, uint32_t arg_size)
{
	return -EPERM;
}

int ais_vfe_write(void *hw_priv, void *write_args, uint32_t arg_size)
{
	return -EPERM;
}

static void ais_vfe_q_bufs_to_hw(struct ais_vfe_hw_core_info *core_info,
		enum ais_ife_output_path_id path)
{
	struct ais_vfe_rdi_output *rdi_path = NULL;
	struct ais_vfe_buffer_t *vfe_buf = NULL;
	struct ais_vfe_bus_ver2_hw_info   *bus_hw_info = NULL;
	struct ais_vfe_bus_ver2_reg_offset_bus_client  *client_regs = NULL;
	uint32_t fifo_status = 0;
	bool is_full = false;
	struct ais_ife_rdi_get_timestamp_args get_ts;

	rdi_path = &core_info->rdi_out[path];
	bus_hw_info = core_info->vfe_hw_info->bus_hw_info;
	client_regs = &bus_hw_info->bus_client_reg[path];

	fifo_status = cam_io_r_mb(core_info->mem_base +
			bus_hw_info->common_reg.addr_fifo_status);

	is_full =  fifo_status & (1 << path);

	while (!is_full) {
		if (list_empty(&rdi_path->buffer_q))
			break;

		vfe_buf = list_first_entry(&rdi_path->buffer_q,
				struct ais_vfe_buffer_t, list);
		list_del_init(&vfe_buf->list);

		get_ts.path = path;
		get_ts.ts = &vfe_buf->ts_hw;
		core_info->csid_hw->hw_ops.process_cmd(
			core_info->csid_hw->hw_priv,
			AIS_IFE_CSID_CMD_GET_TIME_STAMP,
			&get_ts,
			sizeof(get_ts));


		CAM_DBG(CAM_ISP, "IFE%d|RDI%d: Q %d(0x%x) FIFO:%d ts %llu",
			core_info->vfe_idx, path,
			vfe_buf->bufIdx, vfe_buf->iova_addr,
			rdi_path->num_buffer_hw_q, vfe_buf->ts_hw.cur_sof_ts);

		cam_io_w_mb(vfe_buf->iova_addr,
			core_info->mem_base + client_regs->image_addr);

		list_add_tail(&vfe_buf->list, &rdi_path->buffer_hw_q);
		++rdi_path->num_buffer_hw_q;


		fifo_status = cam_io_r_mb(core_info->mem_base +
			bus_hw_info->common_reg.addr_fifo_status);
		is_full =  fifo_status & (1 << path);

		trace_ais_isp_vfe_enq_buf_hw(core_info->vfe_idx, path,
			vfe_buf->bufIdx, rdi_path->num_buffer_hw_q, is_full);
	}

	if (rdi_path->num_buffer_hw_q > MAX_NUM_BUF_SW_FIFOQ_ERR)
		CAM_WARN(CAM_ISP, "Excessive number of buffers in SW FIFO (%d)",
			rdi_path->num_buffer_hw_q);
}


static int ais_vfe_cmd_enq_buf(struct ais_vfe_hw_core_info *core_info,
		struct ais_ife_enqueue_buffer_args *enq_buf)
{
	int rc;
	struct ais_vfe_buffer_t *vfe_buf = NULL;
	struct ais_vfe_rdi_output *rdi_path = NULL;
	int32_t mmu_hdl;
	size_t  src_buf_size;

	if (enq_buf->path >= AIS_IFE_PATH_MAX) {
		CAM_ERR(CAM_ISP, "Invalid output path %d", enq_buf->path);
		rc = -EINVAL;
		goto EXIT;
	}

	rdi_path = &core_info->rdi_out[enq_buf->path];
	if (rdi_path->state < AIS_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "RDI%d invalid state %d", enq_buf->path,
				rdi_path->state);
		rc = -EINVAL;
		goto EXIT;
	}

	spin_lock(&rdi_path->buffer_lock);
	if (!list_empty(&rdi_path->free_buffer_list)) {
		vfe_buf = list_first_entry(&rdi_path->free_buffer_list,
				struct ais_vfe_buffer_t, list);
		list_del_init(&vfe_buf->list);
	}
	spin_unlock(&rdi_path->buffer_lock);

	if (!vfe_buf) {
		CAM_ERR(CAM_ISP, "RDI%d No more free buffers!", enq_buf->path);
		return -ENOMEM;
	}

	vfe_buf->bufIdx = enq_buf->buffer.idx;
	vfe_buf->mem_handle = enq_buf->buffer.mem_handle;

	mmu_hdl = core_info->iommu_hdl;

	if (cam_mem_is_secure_buf(vfe_buf->mem_handle) || rdi_path->secure_mode)
		mmu_hdl = core_info->iommu_hdl_secure;

	rc = cam_mem_get_io_buf(vfe_buf->mem_handle,
		mmu_hdl, &vfe_buf->iova_addr, &src_buf_size);
	if (rc < 0) {
		CAM_ERR(CAM_ISP,
			"get src buf address fail mem_handle 0x%x",
			vfe_buf->mem_handle);
	}
	if (vfe_buf->iova_addr >> 32) {
		CAM_ERR(CAM_ISP, "Invalid mapped address");
		rc = -EINVAL;
	}

	if (enq_buf->buffer.offset >= src_buf_size) {
		CAM_ERR(CAM_ISP, "Invalid buffer offset");
		rc = -EINVAL;
	}

	//if any error, return buffer list object to being free
	if (rc) {
		spin_lock(&rdi_path->buffer_lock);
		list_add_tail(&vfe_buf->list, &rdi_path->free_buffer_list);
		spin_unlock(&rdi_path->buffer_lock);
	} else {
		//add offset
		vfe_buf->iova_addr += enq_buf->buffer.offset;

		spin_lock(&rdi_path->buffer_lock);

		trace_ais_isp_vfe_enq_req(core_info->vfe_idx, enq_buf->path,
				enq_buf->buffer.idx);

		list_add_tail(&vfe_buf->list, &rdi_path->buffer_q);

		if (rdi_path->state < AIS_ISP_RESOURCE_STATE_STREAMING)
			ais_vfe_q_bufs_to_hw(core_info, enq_buf->path);

		spin_unlock(&rdi_path->buffer_lock);
	}

EXIT:
	return rc;
}

int ais_vfe_process_cmd(void *hw_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info                *vfe_hw = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct ais_vfe_hw_core_info       *core_info = NULL;
	struct ais_vfe_hw_info            *hw_info = NULL;
	int rc = 0;

	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	soc_info = &vfe_hw->soc_info;
	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;
	hw_info = core_info->vfe_hw_info;

	mutex_lock(&vfe_hw->hw_mutex);

	switch (cmd_type) {
	case AIS_VFE_CMD_ENQ_BUFFER: {
		struct ais_ife_enqueue_buffer_args *enq_buf =
			(struct ais_ife_enqueue_buffer_args *)cmd_args;
		if (arg_size != sizeof(*enq_buf))
			rc = -EINVAL;
		else
			rc = ais_vfe_cmd_enq_buf(core_info, enq_buf);
		break;
	}
	default:
		CAM_ERR(CAM_ISP, "Invalid cmd type:%d", cmd_type);
		rc = -EINVAL;
		break;
	}

	mutex_unlock(&vfe_hw->hw_mutex);

	return rc;
}

static uint8_t ais_vfe_get_num_missed_sof(
	uint64_t cur_sof,
	uint64_t prev_sof,
	uint64_t last_sof,
	uint64_t ts_delta)
{
	uint8_t miss_sof = 0;

	if (prev_sof == last_sof) {
		miss_sof = 0;
	} else if (prev_sof < last_sof) {
		//rollover case
		miss_sof = (int)(((U64_MAX - last_sof) + prev_sof + 1 +
				ts_delta/2) / ts_delta);
	} else {
		miss_sof = (int)((prev_sof - last_sof + ts_delta/2) / ts_delta);
	}

	return miss_sof;
}

static int ais_vfe_q_sof(struct ais_vfe_hw_core_info *core_info,
	enum ais_ife_output_path_id path,
	struct ais_sof_info_t *p_sof)
{
	struct ais_vfe_rdi_output *p_rdi = &core_info->rdi_out[path];
	struct ais_sof_info_t *p_sof_info = NULL;
	int rc = 0;

	if (!list_empty(&p_rdi->free_sof_info_list)) {
		p_sof_info = list_first_entry(&p_rdi->free_sof_info_list,
			struct ais_sof_info_t, list);
		list_del_init(&p_sof_info->list);
		p_sof_info->frame_cnt = p_sof->frame_cnt;
		p_sof_info->sof_ts = p_sof->sof_ts;
		p_sof_info->cur_sof_hw_ts = p_sof->cur_sof_hw_ts;
		p_sof_info->prev_sof_hw_ts = p_sof->prev_sof_hw_ts;
		list_add_tail(&p_sof_info->list, &p_rdi->sof_info_q);
		p_rdi->num_sof_info_q++;

		trace_ais_isp_vfe_q_sof(core_info->vfe_idx, path,
			p_sof->frame_cnt, p_sof->cur_sof_hw_ts);

		CAM_DBG(CAM_ISP, "I%d|R%d|F%llu: sof %llu",
			core_info->vfe_idx, path, p_sof->frame_cnt,
			p_sof_info->cur_sof_hw_ts);
	} else {
		rc = -1;

		CAM_ERR(CAM_ISP,
			"I%d|R%d|F%llu: free timestamp empty (%d) sof %llu",
			core_info->vfe_idx, path, p_sof->frame_cnt,
			p_rdi->num_buffer_hw_q, p_sof->cur_sof_hw_ts);
	}

	return rc;
}


static void ais_vfe_handle_sof_rdi(struct ais_vfe_hw_core_info *core_info,
		struct ais_vfe_hw_work_data *work_data,
		enum ais_ife_output_path_id path)
{
	struct ais_vfe_rdi_output *p_rdi = &core_info->rdi_out[path];
	uint64_t cur_sof_hw_ts = work_data->ts_hw[path].cur_sof_ts;
	uint64_t prev_sof_hw_ts = work_data->ts_hw[path].prev_sof_ts;

	p_rdi->frame_cnt++;

	if (p_rdi->num_buffer_hw_q) {
		struct ais_sof_info_t sof = {};
		uint64_t ts_delta;
		uint8_t miss_sof = 0;

		if (cur_sof_hw_ts < prev_sof_hw_ts)
			ts_delta = cur_sof_hw_ts +
				(U64_MAX - prev_sof_hw_ts);
		else
			ts_delta = cur_sof_hw_ts - prev_sof_hw_ts;


		//check any missing SOFs
		if (p_rdi->frame_cnt > 1) {
			if (ts_delta == 0) {
				CAM_ERR(CAM_ISP, "IFE%d RDI%d ts_delta is 0",
						core_info->vfe_idx, path);
			} else {
				miss_sof = ais_vfe_get_num_missed_sof(
					cur_sof_hw_ts,
					prev_sof_hw_ts,
					p_rdi->last_sof_info.cur_sof_hw_ts,
					ts_delta);

				CAM_DBG(CAM_ISP,
					"I%d R%d miss_sof %u prev %llu last %llu cur %llu",
					core_info->vfe_idx, path,
					miss_sof, prev_sof_hw_ts,
					p_rdi->last_sof_info.cur_sof_hw_ts,
					cur_sof_hw_ts);
			}
		}

		trace_ais_isp_vfe_sof(core_info->vfe_idx, path,
				&work_data->ts_hw[path],
				p_rdi->num_buffer_hw_q, miss_sof);

		if (p_rdi->frame_cnt == 1 && prev_sof_hw_ts != 0) {
			//enq missed first frame
			sof.sof_ts = work_data->ts;
			sof.cur_sof_hw_ts = prev_sof_hw_ts;
			sof.frame_cnt = p_rdi->frame_cnt++;

			ais_vfe_q_sof(core_info, path, &sof);
		} else if (miss_sof > 0) {
			if (miss_sof > 1) {
				int i = 0;
				int miss_idx = miss_sof - 1;

				for (i = 0; i < (miss_sof - 1); i++) {

					sof.sof_ts = work_data->ts;
					sof.cur_sof_hw_ts = prev_sof_hw_ts -
						(ts_delta * miss_idx);
					sof.frame_cnt = p_rdi->frame_cnt++;

					ais_vfe_q_sof(core_info, path, &sof);

					miss_idx--;
				}
			}

			//enq prev
			sof.sof_ts = work_data->ts;
			sof.cur_sof_hw_ts = prev_sof_hw_ts;
			sof.frame_cnt = p_rdi->frame_cnt++;

			ais_vfe_q_sof(core_info, path, &sof);
		}

		//enq curr
		sof.sof_ts = work_data->ts;
		sof.cur_sof_hw_ts = cur_sof_hw_ts;
		sof.frame_cnt = p_rdi->frame_cnt;

		ais_vfe_q_sof(core_info, path, &sof);

	} else {
		trace_ais_isp_vfe_sof(core_info->vfe_idx, path,
					&work_data->ts_hw[path],
					p_rdi->num_buffer_hw_q, 0);

		CAM_DBG(CAM_ISP, "I%d R%d Flush SOF (%d) HW Q empty",
				core_info->vfe_idx, path,
				p_rdi->num_sof_info_q);

		if (p_rdi->num_sof_info_q) {
			struct ais_sof_info_t *p_sof_info;

			while (!list_empty(&p_rdi->sof_info_q)) {
				p_sof_info = list_first_entry(
					&p_rdi->sof_info_q,
					struct ais_sof_info_t, list);
				list_del_init(&p_sof_info->list);
				list_add_tail(&p_sof_info->list,
						&p_rdi->free_sof_info_list);
			}
			p_rdi->num_sof_info_q = 0;
		}

		trace_ais_isp_vfe_error(core_info->vfe_idx,
						path, 1, 0);

		//send warning
		core_info->event.type = AIS_IFE_MSG_OUTPUT_WARNING;
		core_info->event.path = path;
		core_info->event.u.err_msg.reserved = 0;

		core_info->event_cb(core_info->event_cb_priv,
			&core_info->event);

	}

	p_rdi->last_sof_info.cur_sof_hw_ts = cur_sof_hw_ts;

	//send sof only for current frame
	core_info->event.type = AIS_IFE_MSG_SOF;
	core_info->event.path = path;
	core_info->event.u.sof_msg.frame_id = p_rdi->frame_cnt;
	core_info->event.u.sof_msg.hw_ts = cur_sof_hw_ts;

	core_info->event_cb(core_info->event_cb_priv,
		&core_info->event);

}

static int ais_vfe_handle_sof(
	struct ais_vfe_hw_core_info *core_info,
	struct ais_vfe_hw_work_data *work_data)
{
	struct ais_vfe_rdi_output *p_rdi;
	int path =  0;
	int rc = 0;

	CAM_DBG(CAM_ISP, "IFE%d SOF RDIs 0x%x", core_info->vfe_idx,
			work_data->path);

	for (path = 0; path < AIS_IFE_PATH_MAX; path++) {

		if (!(work_data->path & (1 << path)))
			continue;

		p_rdi = &core_info->rdi_out[path];
		if (p_rdi->state != AIS_ISP_RESOURCE_STATE_STREAMING)
			continue;

		ais_vfe_handle_sof_rdi(core_info, work_data, path);

		//enq buffers
		spin_lock_bh(&p_rdi->buffer_lock);
		ais_vfe_q_bufs_to_hw(core_info, path);
		spin_unlock_bh(&p_rdi->buffer_lock);
	}

	return rc;
}

static int ais_vfe_handle_error(
	struct ais_vfe_hw_core_info *core_info,
	struct ais_vfe_hw_work_data *work_data)
{
	struct ais_vfe_top_ver2_hw_info   *top_hw_info = NULL;
	struct ais_vfe_bus_ver2_hw_info   *bus_hw_info = NULL;
	struct ais_irq_register_set       *bus_hw_irq_regs = NULL;
	struct ais_vfe_bus_ver2_reg_offset_bus_client  *client_regs = NULL;
	struct ais_vfe_rdi_output *p_rdi;
	int path =  0;
	int rc = 0;

	CAM_ERR(CAM_ISP, "IFE%d ERROR on RDIs 0x%x", core_info->vfe_idx,
					work_data->path);

	trace_ais_isp_vfe_error(core_info->vfe_idx,
			work_data->path, 0, 0);

	top_hw_info = core_info->vfe_hw_info->top_hw_info;
	bus_hw_info = core_info->vfe_hw_info->bus_hw_info;
	bus_hw_irq_regs = bus_hw_info->common_reg.irq_reg_info.irq_reg_set;

	for (path = 0; path < AIS_IFE_PATH_MAX; path++) {

		if (!(work_data->path & (1 << path)))
			continue;

		p_rdi = &core_info->rdi_out[path];

		if (p_rdi->state != AIS_ISP_RESOURCE_STATE_STREAMING)
			continue;

		CAM_ERR(CAM_ISP, "IFE%d Turn off RDI %d",
			core_info->vfe_idx, path);

		p_rdi->state = AIS_ISP_RESOURCE_STATE_ERROR;

		client_regs = &bus_hw_info->bus_client_reg[path];

		core_info->bus_wr_mask1 &= ~(1 << path);
		cam_io_w_mb(core_info->bus_wr_mask1,
			core_info->mem_base +
			bus_hw_irq_regs[1].mask_reg_offset);

		/* Disable WM and reg-update */
		cam_io_w_mb(0x0, core_info->mem_base + client_regs->cfg);
		cam_io_w_mb(AIS_VFE_REGUP_RDI_ALL, core_info->mem_base +
				top_hw_info->common_reg->reg_update_cmd);

		cam_io_w_mb((1 << path), core_info->mem_base +
			bus_hw_info->common_reg.sw_reset);


		core_info->event.type = AIS_IFE_MSG_OUTPUT_ERROR;
		core_info->event.path = path;

		core_info->event_cb(core_info->event_cb_priv,
				&core_info->event);
	}

	return rc;
}

static void ais_vfe_bus_handle_client_frame_done(
	struct ais_vfe_hw_core_info *core_info,
	enum ais_ife_output_path_id path,
	uint32_t last_addr)
{
	struct ais_vfe_rdi_output         *rdi_path = NULL;
	struct ais_vfe_buffer_t           *vfe_buf = NULL;
	struct ais_vfe_bus_ver2_hw_info   *bus_hw_info = NULL;
	uint64_t                           frame_cnt = 0;
	uint64_t                           sof_ts;
	uint64_t                           cur_sof_hw_ts;
	bool last_addr_match = false;


	CAM_DBG(CAM_ISP, "I%d|R%d last_addr 0x%x",
			core_info->vfe_idx, path, last_addr);

	if (last_addr == 0) {
		CAM_ERR(CAM_ISP, "I%d|R%d null last_addr",
				core_info->vfe_idx, path);
		return;
	}

	rdi_path = &core_info->rdi_out[path];
	bus_hw_info = core_info->vfe_hw_info->bus_hw_info;

	core_info->event.type = AIS_IFE_MSG_FRAME_DONE;
	core_info->event.path = path;

	while (rdi_path->num_buffer_hw_q && !last_addr_match) {
		struct ais_sof_info_t *p_sof_info = NULL;
		bool is_sof_match = false;

		if (list_empty(&rdi_path->buffer_hw_q)) {
			CAM_DBG(CAM_ISP, "I%d|R%d: FD while HW Q empty",
				core_info->vfe_idx, path);
			break;
		}

		vfe_buf = list_first_entry(&rdi_path->buffer_hw_q,
				struct ais_vfe_buffer_t, list);
		list_del_init(&vfe_buf->list);
		--rdi_path->num_buffer_hw_q;

		if (last_addr == vfe_buf->iova_addr)
			last_addr_match = true;
		else
			CAM_WARN(CAM_ISP, "IFE%d buf %d did not match addr",
				core_info->vfe_idx, vfe_buf->bufIdx);

		CAM_DBG(CAM_ISP, "I%d|R%d BUF DQ %d (0x%x) FIFO:%d|0x%x",
			core_info->vfe_idx, path,
			vfe_buf->bufIdx, vfe_buf->iova_addr,
			rdi_path->num_buffer_hw_q, last_addr);

		if (!list_empty(&rdi_path->sof_info_q)) {
			while (!is_sof_match &&
				!list_empty(&rdi_path->sof_info_q)) {
				p_sof_info =
					list_first_entry(&rdi_path->sof_info_q,
						struct ais_sof_info_t, list);
				list_del_init(&p_sof_info->list);
				rdi_path->num_sof_info_q--;
				if (p_sof_info->cur_sof_hw_ts >
					vfe_buf->ts_hw.cur_sof_ts) {
					is_sof_match = true;
					break;
				}
				list_add_tail(&p_sof_info->list,
					&rdi_path->free_sof_info_list);
			}

			if (!is_sof_match) {
				p_sof_info = NULL;
				CAM_ERR(CAM_ISP,
					"I%d|R%d: can't find the match sof",
					core_info->vfe_idx, path);
			}

		} else
			CAM_ERR(CAM_ISP, "I%d|R%d: SOF info Q is empty",
				core_info->vfe_idx, path);

		if (p_sof_info) {
			frame_cnt = p_sof_info->frame_cnt;
			sof_ts = p_sof_info->sof_ts;
			cur_sof_hw_ts = p_sof_info->cur_sof_hw_ts;
			list_add_tail(&p_sof_info->list,
					&rdi_path->free_sof_info_list);
		} else {
			frame_cnt = sof_ts = cur_sof_hw_ts = 0;
		}

		CAM_DBG(CAM_ISP, "I%d|R%d|F%llu: si [%llu, %llu, %llu]",
			core_info->vfe_idx, path, frame_cnt, sof_ts,
			cur_sof_hw_ts);


		trace_ais_isp_vfe_buf_done(core_info->vfe_idx, path,
				vfe_buf->bufIdx,
				frame_cnt,
				rdi_path->num_buffer_hw_q,
				last_addr_match);

		core_info->event.u.frame_msg.frame_id = frame_cnt;
		core_info->event.u.frame_msg.buf_idx = vfe_buf->bufIdx;
		core_info->event.u.frame_msg.ts = sof_ts;
		core_info->event.u.frame_msg.hw_ts = cur_sof_hw_ts;

		core_info->event_cb(core_info->event_cb_priv,
				&core_info->event);


		list_add_tail(&vfe_buf->list, &rdi_path->free_buffer_list);
	}

	if (!last_addr_match) {
		CAM_ERR(CAM_ISP, "IFE%d BUF| RDI%d NO MATCH addr 0x%x",
			core_info->vfe_idx, path, last_addr);

		trace_ais_isp_vfe_error(core_info->vfe_idx, path, 1, 1);

		//send warning
		core_info->event.type = AIS_IFE_MSG_OUTPUT_WARNING;
		core_info->event.path = path;
		core_info->event.u.err_msg.reserved = 1;

		core_info->event_cb(core_info->event_cb_priv,
			&core_info->event);
	}

	/* Flush SOF info Q if HW Buffer Q is empty */
	if (rdi_path->num_buffer_hw_q == 0) {
		struct ais_sof_info_t *p_sof_info = NULL;

		CAM_DBG(CAM_ISP, "I%d|R%d|F%llu: Flush SOF (%d) HW Q empty",
			core_info->vfe_idx, path, frame_cnt,
			rdi_path->num_sof_info_q);

		while (!list_empty(&rdi_path->sof_info_q)) {
			p_sof_info = list_first_entry(&rdi_path->sof_info_q,
					struct ais_sof_info_t, list);
			list_del_init(&p_sof_info->list);
			list_add_tail(&p_sof_info->list,
				&rdi_path->free_sof_info_list);
		}

		rdi_path->num_sof_info_q = 0;

		trace_ais_isp_vfe_error(core_info->vfe_idx, path, 1, 0);

		//send warning
		core_info->event.type = AIS_IFE_MSG_OUTPUT_WARNING;
		core_info->event.path = path;
		core_info->event.u.err_msg.reserved = 0;

		core_info->event_cb(core_info->event_cb_priv,
			&core_info->event);
	}

	spin_lock_bh(&rdi_path->buffer_lock);

	ais_vfe_q_bufs_to_hw(core_info, path);

	spin_unlock_bh(&rdi_path->buffer_lock);
}

static int ais_vfe_bus_handle_frame_done(
	struct ais_vfe_hw_core_info *core_info,
	struct ais_vfe_hw_work_data *work_data)
{
	struct ais_vfe_rdi_output *p_rdi = &core_info->rdi_out[0];
	uint32_t client_mask = work_data->bus_wr_status[1];
	uint32_t client;
	int rc = 0;

	CAM_DBG(CAM_ISP, "VFE%d Frame Done clients 0x%x",
		core_info->vfe_idx, client_mask);

	for (client = 0 ; client < AIS_IFE_PATH_MAX; client++) {
		p_rdi = &core_info->rdi_out[client];

		if (p_rdi->state != AIS_ISP_RESOURCE_STATE_STREAMING)
			continue;

		if (client_mask & (0x1 << client)) {
			//process frame done
			ais_vfe_bus_handle_client_frame_done(core_info,
				client, work_data->last_addr[client]);
		}
	}

	return rc;
}

static void ais_vfe_irq_fill_bus_wr_status(
	struct ais_vfe_hw_core_info *core_info,
	struct ais_vfe_hw_work_data *work_data)
{
	struct ais_vfe_bus_ver2_hw_info   *bus_hw_info = NULL;
	struct ais_irq_register_set       *bus_hw_irq_regs = NULL;
	struct ais_vfe_bus_ver2_reg_offset_bus_client  *client_regs = NULL;
	uint32_t client = 0;

	bus_hw_info = core_info->vfe_hw_info->bus_hw_info;
	bus_hw_irq_regs = bus_hw_info->common_reg.irq_reg_info.irq_reg_set;
	client_regs = &bus_hw_info->bus_client_reg[client];

	work_data->bus_wr_status[0] = cam_io_r_mb(core_info->mem_base +
		bus_hw_irq_regs[0].status_reg_offset);
	work_data->bus_wr_status[1] = cam_io_r_mb(core_info->mem_base +
		bus_hw_irq_regs[1].status_reg_offset);
	work_data->bus_wr_status[2] = cam_io_r_mb(core_info->mem_base +
		bus_hw_irq_regs[2].status_reg_offset);

	if (work_data->bus_wr_status[1]) {
		struct ais_vfe_rdi_output *p_rdi;

		for (client = 0 ; client < AIS_IFE_PATH_MAX; client++) {
			if (work_data->bus_wr_status[1] & (0x1 << client)) {
				p_rdi = &core_info->rdi_out[client];
				client_regs =
					&bus_hw_info->bus_client_reg[client];
				work_data->last_addr[client] = cam_io_r(
					core_info->mem_base +
					client_regs->status0);
			}
		}
	}

	cam_io_w(work_data->bus_wr_status[0], core_info->mem_base +
		bus_hw_irq_regs[0].clear_reg_offset);
	cam_io_w(work_data->bus_wr_status[1], core_info->mem_base +
		bus_hw_irq_regs[1].clear_reg_offset);
	cam_io_w(work_data->bus_wr_status[2], core_info->mem_base +
		bus_hw_irq_regs[2].clear_reg_offset);
	cam_io_w_mb(0x1, core_info->mem_base +
		bus_hw_info->common_reg.irq_reg_info.global_clear_offset);
}

static int ais_vfe_handle_bus_wr_irq(struct cam_hw_info *vfe_hw,
	struct ais_vfe_hw_core_info *core_info,
	struct ais_vfe_hw_work_data *work_data)
{
	int rc = 0;
	struct ais_vfe_bus_ver2_hw_info   *bus_hw_info = NULL;
	struct ais_irq_register_set       *bus_hw_irq_regs = NULL;
	struct ais_vfe_bus_ver2_reg_offset_bus_client  *client_regs = NULL;
	uint32_t client = 0;

	bus_hw_info = core_info->vfe_hw_info->bus_hw_info;
	bus_hw_irq_regs = bus_hw_info->common_reg.irq_reg_info.irq_reg_set;
	client_regs = &bus_hw_info->bus_client_reg[client];


	CAM_DBG(CAM_ISP, "VFE%d BUS status 0x%x 0x%x 0x%x", core_info->vfe_idx,
		work_data->bus_wr_status[0],
		work_data->bus_wr_status[1],
		work_data->bus_wr_status[2]);

	if (work_data->bus_wr_status[1])
		ais_vfe_bus_handle_frame_done(core_info, work_data);

	if (work_data->bus_wr_status[0] & 0x7800) {
		CAM_ERR(CAM_ISP, "VFE%d: WR BUS error occurred status = 0x%x",
			core_info->vfe_idx, work_data->bus_wr_status[0]);
		work_data->path = (work_data->bus_wr_status[0] >> 11) & 0xF;
		rc = ais_vfe_handle_error(core_info, work_data);
	}

	if (work_data->bus_wr_status[0] & 0x1) {
		CAM_DBG(CAM_ISP, "VFE%d: WR BUS reset completed",
			core_info->vfe_idx);
		complete(&vfe_hw->hw_complete);
	}

	return rc;
}

static int ais_vfe_process_irq_bh(void *priv, void *data)
{
	struct ais_vfe_hw_work_data   *work_data;
	struct cam_hw_info            *vfe_hw;
	struct ais_vfe_hw_core_info   *core_info;
	int rc = 0;

	vfe_hw = (struct cam_hw_info *)priv;
	if (!vfe_hw) {
		CAM_ERR(CAM_ISP, "Invalid parameters");
		return -EINVAL;
	}

	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;
	if (!core_info->event_cb) {
		CAM_ERR(CAM_ISP, "hw_idx %d Error Cb not registered",
			core_info->vfe_idx);
		return -EINVAL;
	}

	work_data = (struct ais_vfe_hw_work_data *)data;

	trace_ais_isp_irq_process(core_info->vfe_idx, work_data->evt_type, 1);
	CAM_DBG(CAM_ISP, "VFE[%d] event %d",
		core_info->vfe_idx, work_data->evt_type);

	core_info->event.idx = core_info->vfe_idx;
	core_info->event.boot_ts = work_data->ts;

	switch (work_data->evt_type) {
	case AIS_VFE_HW_IRQ_EVENT_SOF:
		rc = ais_vfe_handle_sof(core_info, work_data);
		break;
	case AIS_VFE_HW_IRQ_EVENT_BUS_WR:
		rc = ais_vfe_handle_bus_wr_irq(vfe_hw, core_info, work_data);
		break;
	case AIS_VFE_HW_IRQ_EVENT_ERROR:
		rc = ais_vfe_handle_error(core_info, work_data);
		break;
	default:
		CAM_ERR(CAM_ISP, "VFE[%d] invalid event type %d",
			core_info->vfe_idx, work_data->evt_type);
		break;
	}

	trace_ais_isp_irq_process(core_info->vfe_idx, work_data->evt_type, 2);

	return rc;
}

static int ais_vfe_dispatch_irq(struct cam_hw_info *vfe_hw,
		struct ais_vfe_hw_work_data *p_work)
{
	struct ais_vfe_hw_core_info *core_info;
	struct ais_vfe_hw_work_data *work_data;
	struct crm_workq_task *task;
	int rc = 0;

	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;

	CAM_DBG(CAM_ISP, "VFE[%d] event %d",
		core_info->vfe_idx, p_work->evt_type);

	task = cam_req_mgr_workq_get_task(core_info->workq);
	if (!task) {
		CAM_ERR(CAM_ISP, "Can not get task for worker");
		return -ENOMEM;
	}
	work_data = (struct ais_vfe_hw_work_data *)task->payload;
	*work_data = *p_work;

	trace_ais_isp_irq_process(core_info->vfe_idx, p_work->evt_type, 0);

	task->process_cb = ais_vfe_process_irq_bh;
	rc = cam_req_mgr_workq_enqueue_task(task, vfe_hw,
		CRM_TASK_PRIORITY_0);

	return rc;
}

irqreturn_t ais_vfe_irq(int irq_num, void *data)
{
	struct cam_hw_info            *vfe_hw;
	struct ais_vfe_hw_core_info   *core_info;
	uint32_t ife_status[2] = {};

	if (!data)
		return IRQ_NONE;

	vfe_hw = (struct cam_hw_info *)data;
	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;

	/* Read and Clear all IFE status regs */
	ife_status[0] = cam_io_r_mb(core_info->mem_base + AIS_VFE_IRQ_STATUS0);
	ife_status[1] = cam_io_r_mb(core_info->mem_base + AIS_VFE_IRQ_STATUS1);

	cam_io_w_mb(ife_status[0], core_info->mem_base + AIS_VFE_IRQ_CLEAR0);
	cam_io_w_mb(ife_status[1], core_info->mem_base + AIS_VFE_IRQ_CLEAR1);
	cam_io_w_mb(0x1, core_info->mem_base + AIS_VFE_IRQ_CMD);

	trace_ais_isp_vfe_irq_activated(core_info->vfe_idx,
			ife_status[0], ife_status[1]);
	CAM_DBG(CAM_ISP, "VFE%d irq status 0x%x 0x%x", core_info->vfe_idx,
			ife_status[0], ife_status[1]);

	//process any reset inetrrupt
	if (ife_status[0] & AIS_VFE_STATUS0_RESET_ACK_IRQ) {
		/*
		 * Clear All IRQs to avoid spurious IRQs immediately
		 * after Reset Done.
		 */
		cam_io_w(0xFFFFFFFF, core_info->mem_base + AIS_VFE_IRQ_CLEAR0);
		cam_io_w(0xFFFFFFFF, core_info->mem_base + AIS_VFE_IRQ_CLEAR1);
		cam_io_w(0x1, core_info->mem_base + AIS_VFE_IRQ_CMD);
		CAM_DBG(CAM_ISP, "VFE%d Calling Complete for RESET CMD",
				core_info->vfe_idx);
		complete(&vfe_hw->hw_complete);
	} else {
		struct ais_ife_rdi_get_timestamp_args get_ts;
		struct ais_vfe_hw_work_data work_data;
		struct timespec64 ts;

		get_monotonic_boottime64(&ts);
		work_data.ts =
			(uint64_t)((ts.tv_sec * 1000000000) + ts.tv_nsec);

		if (ife_status[0] & AIS_VFE_STATUS0_RDI_SOF_IRQ) {
			//RDI SOF
			int i;

			work_data.path = (ife_status[0] >>
				AIS_VFE_STATUS0_RDI_SOF_IRQ_SHFT) &
				AIS_VFE_STATUS0_RDI_SOF_IRQ_MSK;
			CAM_DBG(CAM_ISP, "IFE%d SOF 0x%x",
				core_info->vfe_idx, work_data.path);

			//fill HW timestamp for each RDI path
			for (i = 0; i < AIS_IFE_PATH_MAX; i++) {
				if (!(work_data.path & (1 << i)))
					continue;

				get_ts.path = i;
				get_ts.ts = &work_data.ts_hw[i];
				core_info->csid_hw->hw_ops.process_cmd(
					core_info->csid_hw->hw_priv,
					AIS_IFE_CSID_CMD_GET_TIME_STAMP,
					&get_ts,
					sizeof(get_ts));
			}

			work_data.evt_type = AIS_VFE_HW_IRQ_EVENT_SOF;
			ais_vfe_dispatch_irq(vfe_hw, &work_data);
		}
		if (ife_status[0] & AIS_VFE_STATUS0_BUS_WR_IRQ) {
			//BUS_WR IRQ
			CAM_DBG(CAM_ISP, "IFE%d BUS_WR", core_info->vfe_idx);
			work_data.evt_type = AIS_VFE_HW_IRQ_EVENT_BUS_WR;
			ais_vfe_irq_fill_bus_wr_status(core_info, &work_data);

			ais_vfe_dispatch_irq(vfe_hw, &work_data);
		}
		if (ife_status[1]) {
			if (ife_status[1] & AIS_VFE_STATUS0_RDI_OVERFLOW_IRQ) {
				work_data.path = (ife_status[1] >>
				AIS_VFE_STATUS1_RDI_OVERFLOW_IRQ_SHFT) &
				AIS_VFE_STATUS1_RDI_OVERFLOW_IRQ_MSK;

				CAM_ERR_RATE_LIMIT(CAM_ISP,
					"IFE%d Overflow 0x%x",
					core_info->vfe_idx,
					work_data.path);
				work_data.evt_type = AIS_VFE_HW_IRQ_EVENT_ERROR;
				ais_vfe_dispatch_irq(vfe_hw, &work_data);
			}
		}
	}

	return IRQ_HANDLED;
}

int ais_vfe_core_init(struct ais_vfe_hw_core_info  *core_info,
	struct cam_hw_soc_info                     *soc_info,
	struct cam_hw_intf                         *hw_intf,
	struct ais_vfe_hw_info                     *vfe_hw_info)
{
	int rc = 0;
	int i;
	char worker_name[128];

	CAM_DBG(CAM_ISP, "Enter");

	core_info->vfe_idx = soc_info->index;
	core_info->mem_base =
		CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX);

	spin_lock_init(&core_info->spin_lock);

	for (i = 0; i < AIS_IFE_PATH_MAX; i++) {
		struct ais_vfe_rdi_output *p_rdi = &core_info->rdi_out[i];

		spin_lock_init(&p_rdi->buffer_lock);
		ais_clear_rdi_path(p_rdi);
		p_rdi->state = AIS_ISP_RESOURCE_STATE_AVAILABLE;
	}


	scnprintf(worker_name, sizeof(worker_name),
		"vfe%u_worker", core_info->vfe_idx);
	CAM_DBG(CAM_ISP, "Create VFE worker %s", worker_name);
	rc = cam_req_mgr_workq_create(worker_name,
		AIS_VFE_WORKQ_NUM_TASK,
		&core_info->workq, CRM_WORKQ_USAGE_IRQ, 0);
	if (rc) {
		CAM_ERR(CAM_ISP, "Unable to create a workq, rc=%d", rc);
		goto EXIT;
	}

	for (i = 0; i < AIS_VFE_WORKQ_NUM_TASK; i++)
		core_info->workq->task.pool[i].payload =
			&core_info->work_data[i];

EXIT:
	return rc;
}

int ais_vfe_core_deinit(struct ais_vfe_hw_core_info  *core_info,
	struct ais_vfe_hw_info                       *vfe_hw_info)
{
	int                rc = -EINVAL;
	int                i;
	unsigned long      flags;

	spin_lock_irqsave(&core_info->spin_lock, flags);

	cam_req_mgr_workq_destroy(&core_info->workq);

	for (i = 0; i < AIS_IFE_PATH_MAX; i++) {
		struct ais_vfe_rdi_output *p_rdi = &core_info->rdi_out[i];

		ais_clear_rdi_path(p_rdi);
		p_rdi->state = AIS_ISP_RESOURCE_STATE_AVAILABLE;
	}

	spin_unlock_irqrestore(&core_info->spin_lock, flags);

	return rc;
}
