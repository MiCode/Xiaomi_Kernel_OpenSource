// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/ratelimit.h>
#include "cam_tasklet_util.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_vfe_soc.h"
#include "cam_vfe_core.h"
#include "cam_vfe_bus.h"
#include "cam_vfe_top.h"
#include "cam_ife_hw_mgr.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"

static const char drv_name[] = "vfe";

#define CAM_VFE_17X_CLEAR_0_REG_OFFSET              0x00000064
#define CAM_VFE_17X_CLEAR_1_REG_OFFSET              0x00000068
#define CAM_VFE_17X_IRQ_CMD_REG_OFFSET              0x00000058
#define CAM_VFE_17X_TOP_RESET_MASK                  0x80000000

#define CAM_VFE_48X_CLEAR_0_REG_OFFSET              0x00000048
#define CAM_VFE_48X_CLEAR_1_REG_OFFSET              0x0000004C
#define CAM_VFE_48X_CLEAR_2_REG_OFFSET              0x00000050
#define CAM_VFE_48X_IRQ_CMD_REG_OFFSET              0x00000038
#define CAM_VFE_48X_TOP_RESET_MASK                  0x00000001

#define CAM_VFE_LITE_48X_CLEAR_0_REG_OFFSET         0x00000034
#define CAM_VFE_LITE_48X_CLEAR_1_REG_OFFSET         0x00000038
#define CAM_VFE_LITE_48X_CLEAR_2_REG_OFFSET         0x0000003C
#define CAM_VFE_LITE_48X_IRQ_CMD_REG_OFFSET         0x00000024
#define CAM_VFE_LITE_48X_TOP_RESET_MASK             0x00020000

int cam_vfe_get_hw_caps(void *hw_priv, void *get_hw_cap_args, uint32_t arg_size)
{
	struct cam_hw_info                *vfe_dev = hw_priv;
	struct cam_vfe_hw_core_info       *core_info = NULL;
	int rc = 0;

	CAM_DBG(CAM_ISP, "Enter");
	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	core_info = (struct cam_vfe_hw_core_info *)vfe_dev->core_info;

	if (core_info->vfe_top->hw_ops.get_hw_caps)
		core_info->vfe_top->hw_ops.get_hw_caps(
			core_info->vfe_top->top_priv,
			get_hw_cap_args, arg_size);

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

int cam_vfe_reset_irq_top_half(uint32_t    evt_id,
	struct cam_irq_th_payload         *th_payload)
{
	int32_t                            rc = -EINVAL;
	struct cam_hw_info                *vfe_hw;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_vfe_soc_private        *soc_private = NULL;
	void __iomem                      *mem_base;

	vfe_hw = th_payload->handler_priv;
	soc_info = &vfe_hw->soc_info;
	soc_private = soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Invalid soc_private");
		return -ENODEV;
	}

	CAM_DBG(CAM_ISP, "Enter");

	/*
	 * Clear All IRQs to avoid spurious IRQs immediately
	 * after Reset Done.
	 */
	CAM_DBG(CAM_ISP, "TOP_IRQ_STATUS_0 = 0x%x",
		th_payload->evt_status_arr[0]);

	mem_base = soc_info->reg_map[VFE_CORE_BASE_IDX].mem_base;

	switch (soc_info->hw_version) {
	case CAM_CPAS_TITAN_480_V100:
		if (!soc_private->is_ife_lite) {
			if (th_payload->evt_status_arr[0] & 0x1) {
				cam_io_w(0xFFFFFFFF, mem_base +
					CAM_VFE_48X_CLEAR_0_REG_OFFSET);
				cam_io_w(0xFFFFFFFF, mem_base +
					CAM_VFE_48X_CLEAR_1_REG_OFFSET);
				cam_io_w(0xFFFFFFFF, mem_base +
					CAM_VFE_48X_CLEAR_2_REG_OFFSET);
				cam_io_w(0x00000001, mem_base +
					CAM_VFE_48X_IRQ_CMD_REG_OFFSET);
				CAM_DBG(CAM_ISP,
					"Calling Complete for RESET CMD");
				complete(&vfe_hw->hw_complete);
				rc = 0;
			}
		} else {
			if (th_payload->evt_status_arr[0] & (1<<17)) {
				cam_io_w(0xFFFFFFFF, mem_base +
					CAM_VFE_LITE_48X_CLEAR_0_REG_OFFSET);
				cam_io_w(0xFFFFFFFF, mem_base +
					CAM_VFE_LITE_48X_CLEAR_1_REG_OFFSET);
				cam_io_w(0xFFFFFFFF, mem_base +
					CAM_VFE_LITE_48X_CLEAR_2_REG_OFFSET);
				cam_io_w(0x00000001, mem_base +
					CAM_VFE_LITE_48X_IRQ_CMD_REG_OFFSET);
				CAM_DBG(CAM_ISP,
					"Calling Complete for RESET CMD");
				complete(&vfe_hw->hw_complete);
				rc = 0;
			}
		}
		break;
	default:
		if (th_payload->evt_status_arr[0] & (1<<31)) {
			cam_io_w(0xFFFFFFFF, mem_base +
				CAM_VFE_17X_CLEAR_0_REG_OFFSET);
			cam_io_w(0xFFFFFFFF, mem_base +
				CAM_VFE_17X_CLEAR_1_REG_OFFSET);
			cam_io_w(0x00000001, mem_base +
				CAM_VFE_17X_IRQ_CMD_REG_OFFSET);
			CAM_DBG(CAM_ISP, "Calling Complete for RESET CMD");
			complete(&vfe_hw->hw_complete);
			rc = 0;
		}
		break;
	}

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

int cam_vfe_init_hw(void *hw_priv, void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info                *vfe_hw = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_vfe_hw_core_info       *core_info = NULL;
	struct cam_isp_resource_node      *isp_res = NULL;
	int rc = 0;
	uint32_t                           reset_core_args =
					CAM_VFE_HW_RESET_HW_AND_REG;

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
	core_info = (struct cam_vfe_hw_core_info *)vfe_hw->core_info;

	/* Turn ON Regulators, Clocks and other SOC resources */
	rc = cam_vfe_enable_soc_resources(soc_info);
	if (rc) {
		CAM_ERR(CAM_ISP, "Enable SOC failed");
		rc = -EFAULT;
		goto decrement_open_cnt;
	}

	isp_res   = (struct cam_isp_resource_node *)init_hw_args;
	if (isp_res && isp_res->init) {
		rc = isp_res->init(isp_res, NULL, 0);
		if (rc) {
			CAM_ERR(CAM_ISP, "init Failed rc=%d", rc);
			goto disable_soc;
		}
	}

	CAM_DBG(CAM_ISP, "Enable soc done");

	/* Do HW Reset */
	rc = cam_vfe_reset(hw_priv, &reset_core_args, sizeof(uint32_t));
	if (rc) {
		CAM_ERR(CAM_ISP, "Reset Failed rc=%d", rc);
		goto deinint_vfe_res;
	}

	rc = core_info->vfe_bus->hw_ops.init(core_info->vfe_bus->bus_priv,
		NULL, 0);
	if (rc) {
		CAM_ERR(CAM_ISP, "Bus HW init Failed rc=%d", rc);
		goto deinint_vfe_res;
	}

	rc = core_info->vfe_top->hw_ops.init(core_info->vfe_top->top_priv,
		NULL, 0);
	if (rc) {
		CAM_ERR(CAM_ISP, "Top HW init Failed rc=%d", rc);
		goto deinint_vfe_res;
	}

	if (core_info->vfe_rd_bus) {
		rc = core_info->vfe_rd_bus->hw_ops.init(
			core_info->vfe_rd_bus->bus_priv,
			NULL, 0);
		if (rc) {
			CAM_ERR(CAM_ISP, "Bus RD HW init Failed rc=%d", rc);
			goto deinint_vfe_res;
		}
	}

	vfe_hw->hw_state = CAM_HW_STATE_POWER_UP;
	return rc;

deinint_vfe_res:
	if (isp_res && isp_res->deinit)
		isp_res->deinit(isp_res, NULL, 0);
disable_soc:
	cam_vfe_disable_soc_resources(soc_info);
decrement_open_cnt:
	mutex_lock(&vfe_hw->hw_mutex);
	vfe_hw->open_count--;
	mutex_unlock(&vfe_hw->hw_mutex);
	return rc;
}

int cam_vfe_deinit_hw(void *hw_priv, void *deinit_hw_args, uint32_t arg_size)
{
	struct cam_hw_info                *vfe_hw = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_vfe_hw_core_info       *core_info = NULL;
	struct cam_isp_resource_node      *isp_res = NULL;
	int rc = 0;
	uint32_t                           reset_core_args =
					CAM_VFE_HW_RESET_HW_AND_REG;

	CAM_DBG(CAM_ISP, "Enter");
	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	isp_res = (struct cam_isp_resource_node *)deinit_hw_args;
	if (isp_res && isp_res->deinit) {
		rc = isp_res->deinit(isp_res, NULL, 0);
		if (rc)
			CAM_ERR(CAM_ISP, "deinit failed");
	}

	mutex_lock(&vfe_hw->hw_mutex);
	if (!vfe_hw->open_count) {
		mutex_unlock(&vfe_hw->hw_mutex);
		CAM_ERR(CAM_ISP, "Error. Unbalanced deinit");
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
	core_info = (struct cam_vfe_hw_core_info *)vfe_hw->core_info;

	rc = core_info->vfe_bus->hw_ops.deinit(core_info->vfe_bus->bus_priv,
		NULL, 0);
	if (rc)
		CAM_ERR(CAM_ISP, "Bus HW deinit Failed rc=%d", rc);

	if (core_info->vfe_rd_bus) {
		rc = core_info->vfe_rd_bus->hw_ops.deinit(
			core_info->vfe_rd_bus->bus_priv,
			NULL, 0);
		if (rc)
			CAM_ERR(CAM_ISP, "Bus HW deinit Failed rc=%d", rc);
	}

	rc = cam_vfe_reset(hw_priv, &reset_core_args, sizeof(uint32_t));

	/* Turn OFF Regulators, Clocks and other SOC resources */
	CAM_DBG(CAM_ISP, "Disable SOC resource");
	rc = cam_vfe_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_ISP, "Disable SOC failed");

	vfe_hw->hw_state = CAM_HW_STATE_POWER_DOWN;

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

int cam_vfe_reset(void *hw_priv, void *reset_core_args, uint32_t arg_size)
{
	struct cam_hw_info *vfe_hw  = hw_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_vfe_soc_private *soc_private = NULL;
	struct cam_vfe_hw_core_info *core_info = NULL;
	uint32_t top_reset_irq_reg_mask[CAM_IFE_IRQ_REGISTERS_MAX];
	int rc;

	CAM_DBG(CAM_ISP, "Enter");

	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	soc_info = &vfe_hw->soc_info;
	soc_private = soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Invalid soc_private");
		return -ENODEV;
	}
	core_info = (struct cam_vfe_hw_core_info *)vfe_hw->core_info;

	memset(top_reset_irq_reg_mask, 0, sizeof(top_reset_irq_reg_mask));

	switch (soc_info->hw_version) {
	case CAM_CPAS_TITAN_480_V100:
		if (!soc_private->is_ife_lite)
			top_reset_irq_reg_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS0]
				= CAM_VFE_48X_TOP_RESET_MASK;
		else
			top_reset_irq_reg_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS0]
				= CAM_VFE_LITE_48X_TOP_RESET_MASK;
		break;
	default:
		top_reset_irq_reg_mask[CAM_IFE_IRQ_CAMIF_REG_STATUS0]
			= CAM_VFE_17X_TOP_RESET_MASK;
		break;
	}

	core_info->reset_irq_handle = cam_irq_controller_subscribe_irq(
		core_info->vfe_irq_controller, CAM_IRQ_PRIORITY_0,
		top_reset_irq_reg_mask, vfe_hw,
		cam_vfe_reset_irq_top_half, NULL, NULL, NULL);
	if (core_info->reset_irq_handle < 1) {
		CAM_ERR(CAM_ISP, "subscribe irq controller failed");
		core_info->reset_irq_handle = 0;
		return -EFAULT;
	}

	reinit_completion(&vfe_hw->hw_complete);

	CAM_DBG(CAM_ISP, "calling RESET on VFE:%d", soc_info->index);

	core_info->vfe_top->hw_ops.reset(core_info->vfe_top->top_priv,
		reset_core_args, arg_size);

	/* Wait for Completion or Timeout of 500ms */
	rc = wait_for_completion_timeout(&vfe_hw->hw_complete, 500);

	if (!rc)
		CAM_ERR(CAM_ISP, "Reset Timeout");
	else
		CAM_DBG(CAM_ISP, "reset complete done (%d)", rc);

	rc = cam_irq_controller_unsubscribe_irq(
		core_info->vfe_irq_controller, core_info->reset_irq_handle);
	if (rc)
		CAM_ERR(CAM_ISP, "Error. Unsubscribe failed");

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

void cam_isp_hw_get_timestamp(struct cam_isp_timestamp *time_stamp)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);
	time_stamp->mono_time.tv_sec    = ts.tv_sec;
	time_stamp->mono_time.tv_usec   = ts.tv_nsec/1000;
}

int cam_vfe_reserve(void *hw_priv, void *reserve_args, uint32_t arg_size)
{
	struct cam_vfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *vfe_hw  = hw_priv;
	struct cam_vfe_acquire_args       *acquire;
	int rc = -ENODEV;

	if (!hw_priv || !reserve_args || (arg_size !=
		sizeof(struct cam_vfe_acquire_args))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}
	core_info = (struct cam_vfe_hw_core_info *)vfe_hw->core_info;
	acquire = (struct cam_vfe_acquire_args   *)reserve_args;

	CAM_DBG(CAM_ISP, "acq res type: %d", acquire->rsrc_type);
	mutex_lock(&vfe_hw->hw_mutex);
	if (acquire->rsrc_type == CAM_ISP_RESOURCE_VFE_IN) {
		rc = core_info->vfe_top->hw_ops.reserve(
			core_info->vfe_top->top_priv,
			acquire, sizeof(*acquire));
	} else if (acquire->rsrc_type == CAM_ISP_RESOURCE_VFE_OUT) {
		rc = core_info->vfe_bus->hw_ops.reserve(
			core_info->vfe_bus->bus_priv, acquire,
			sizeof(*acquire));
	} else if (acquire->rsrc_type == CAM_ISP_RESOURCE_VFE_BUS_RD) {
		if (core_info->vfe_rd_bus)
			rc = core_info->vfe_rd_bus->hw_ops.reserve(
				core_info->vfe_rd_bus->bus_priv, acquire,
				sizeof(*acquire));
	} else
		CAM_ERR(CAM_ISP, "Invalid res type:%d", acquire->rsrc_type);

	mutex_unlock(&vfe_hw->hw_mutex);

	return rc;
}

int cam_vfe_release(void *hw_priv, void *release_args, uint32_t arg_size)
{
	struct cam_vfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *vfe_hw  = hw_priv;
	struct cam_isp_resource_node      *isp_res;
	int rc = -ENODEV;

	if (!hw_priv || !release_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	core_info = (struct cam_vfe_hw_core_info *)vfe_hw->core_info;
	isp_res = (struct cam_isp_resource_node      *) release_args;

	mutex_lock(&vfe_hw->hw_mutex);
	if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_IN)
		rc = core_info->vfe_top->hw_ops.release(
			core_info->vfe_top->top_priv, isp_res,
			sizeof(*isp_res));
	else if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_OUT)
		rc = core_info->vfe_bus->hw_ops.release(
			core_info->vfe_bus->bus_priv, isp_res,
			sizeof(*isp_res));
	else if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_BUS_RD) {
		if (core_info->vfe_rd_bus)
			rc = core_info->vfe_rd_bus->hw_ops.release(
				core_info->vfe_rd_bus->bus_priv, isp_res,
				sizeof(*isp_res));
	} else {
		CAM_ERR(CAM_ISP, "Invalid res type:%d", isp_res->res_type);
	}

	mutex_unlock(&vfe_hw->hw_mutex);

	return rc;
}


int cam_vfe_start(void *hw_priv, void *start_args, uint32_t arg_size)
{
	struct cam_vfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *vfe_hw  = hw_priv;
	struct cam_isp_resource_node      *isp_res;
	struct cam_hw_soc_info            *soc_info = NULL;
	int                                rc = 0;

	if (!hw_priv || !start_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	soc_info = &vfe_hw->soc_info;
	core_info = (struct cam_vfe_hw_core_info *)vfe_hw->core_info;
	isp_res = (struct cam_isp_resource_node  *)start_args;
	core_info->tasklet_info = isp_res->tasklet_info;

	mutex_lock(&vfe_hw->hw_mutex);
	if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_IN) {
		rc = core_info->vfe_top->hw_ops.start(
			core_info->vfe_top->top_priv, isp_res,
			sizeof(struct cam_isp_resource_node));

		if (rc)
			CAM_ERR(CAM_ISP, "Failed to start VFE IN");
	} else if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_OUT) {
		rc = core_info->vfe_bus->hw_ops.start(isp_res, NULL, 0);

		if (rc)
			CAM_ERR(CAM_ISP, "Failed to start VFE OUT");
	} else if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_BUS_RD) {
		if (core_info->vfe_rd_bus) {
			rc = core_info->vfe_rd_bus->hw_ops.start(isp_res,
				NULL, 0);

			if (rc)
				CAM_ERR(CAM_ISP, "Failed to start BUS RD");
		}
	} else {
		CAM_ERR(CAM_ISP, "Invalid res type:%d", isp_res->res_type);
		rc = -EFAULT;
	}

	mutex_unlock(&vfe_hw->hw_mutex);

	return rc;
}

int cam_vfe_stop(void *hw_priv, void *stop_args, uint32_t arg_size)
{
	struct cam_vfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *vfe_hw  = hw_priv;
	struct cam_isp_resource_node      *isp_res;
	int rc = -EINVAL;

	if (!hw_priv || !stop_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	core_info = (struct cam_vfe_hw_core_info *)vfe_hw->core_info;
	isp_res = (struct cam_isp_resource_node  *)stop_args;

	mutex_lock(&vfe_hw->hw_mutex);
	if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_IN) {
		rc = core_info->vfe_top->hw_ops.stop(
			core_info->vfe_top->top_priv, isp_res,
			sizeof(struct cam_isp_resource_node));
	} else if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_OUT) {
		rc = core_info->vfe_bus->hw_ops.stop(isp_res, NULL, 0);
	} else if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_BUS_RD) {
		if (core_info->vfe_rd_bus)
			rc = core_info->vfe_rd_bus->hw_ops.stop(isp_res,
				NULL, 0);
	} else {
		CAM_ERR(CAM_ISP, "Invalid res type:%d", isp_res->res_type);
	}

	if (core_info->reset_irq_handle > 0) {
		cam_irq_controller_unsubscribe_irq(
			core_info->vfe_irq_controller,
			core_info->reset_irq_handle);
		core_info->reset_irq_handle = 0;
	}

	mutex_unlock(&vfe_hw->hw_mutex);

	return rc;
}

int cam_vfe_read(void *hw_priv, void *read_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_vfe_write(void *hw_priv, void *write_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_vfe_process_cmd(void *hw_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info                *vfe_hw = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_vfe_hw_core_info       *core_info = NULL;
	struct cam_vfe_hw_info            *hw_info = NULL;
	int rc = 0;

	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid arguments");
		return -EINVAL;
	}

	soc_info = &vfe_hw->soc_info;
	core_info = (struct cam_vfe_hw_core_info *)vfe_hw->core_info;
	hw_info = core_info->vfe_hw_info;

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_CHANGE_BASE:
	case CAM_ISP_HW_CMD_GET_REG_UPDATE:
	case CAM_ISP_HW_CMD_CLOCK_UPDATE:
	case CAM_ISP_HW_CMD_BW_UPDATE:
	case CAM_ISP_HW_CMD_BW_CONTROL:
	case CAM_ISP_HW_CMD_CORE_CONFIG:
	case CAM_ISP_HW_CMD_BW_UPDATE_V2:
		rc = core_info->vfe_top->hw_ops.process_cmd(
			core_info->vfe_top->top_priv, cmd_type, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_BUF_UPDATE:
	case CAM_ISP_HW_CMD_GET_HFR_UPDATE:
	case CAM_ISP_HW_CMD_STRIPE_UPDATE:
	case CAM_ISP_HW_CMD_STOP_BUS_ERR_IRQ:
	case CAM_ISP_HW_CMD_UBWC_UPDATE:
	case CAM_ISP_HW_CMD_UBWC_UPDATE_V2:
	case CAM_ISP_HW_CMD_WM_CONFIG_UPDATE:
		rc = core_info->vfe_bus->hw_ops.process_cmd(
			core_info->vfe_bus->bus_priv, cmd_type, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_HFR_UPDATE_RM:
	case CAM_ISP_HW_CMD_GET_BUF_UPDATE_RM:
		if (core_info->vfe_rd_bus)
			rc = core_info->vfe_rd_bus->hw_ops.process_cmd(
				core_info->vfe_rd_bus->bus_priv, cmd_type,
				cmd_args, arg_size);
		break;

	case CAM_ISP_HW_CMD_FE_UPDATE_IN_RD:
		rc = core_info->vfe_top->hw_ops.process_cmd(
			core_info->vfe_top->top_priv, cmd_type, cmd_args,
			arg_size);
		break;
	case CAM_ISP_HW_CMD_FE_UPDATE_BUS_RD:
		if (core_info->vfe_rd_bus) {
			rc = core_info->vfe_rd_bus->hw_ops.process_cmd(
				core_info->vfe_rd_bus->bus_priv, cmd_type,
				cmd_args, arg_size);
		}
		break;
	default:
		CAM_ERR(CAM_ISP, "Invalid cmd type:%d", cmd_type);
		rc = -EINVAL;
		break;
	}
	return rc;
}

irqreturn_t cam_vfe_irq(int irq_num, void *data)
{
	struct cam_hw_info            *vfe_hw;
	struct cam_vfe_hw_core_info   *core_info;

	if (!data)
		return IRQ_NONE;

	vfe_hw = (struct cam_hw_info *)data;
	core_info = (struct cam_vfe_hw_core_info *)vfe_hw->core_info;

	return cam_irq_controller_handle_irq(irq_num,
		core_info->vfe_irq_controller);
}

int cam_vfe_core_init(struct cam_vfe_hw_core_info  *core_info,
	struct cam_hw_soc_info                     *soc_info,
	struct cam_hw_intf                         *hw_intf,
	struct cam_vfe_hw_info                     *vfe_hw_info)
{
	int rc = -EINVAL;
	struct cam_vfe_soc_private *soc_private = NULL;

	CAM_DBG(CAM_ISP, "Enter");

	soc_private = soc_info->soc_private;
	if (!soc_private) {
		CAM_ERR(CAM_ISP, "Invalid soc_private");
		return -ENODEV;
	}

	rc = cam_irq_controller_init(drv_name,
		CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX),
		vfe_hw_info->irq_reg_info, &core_info->vfe_irq_controller,
		true);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"Error, cam_irq_controller_init failed rc = %d", rc);
		return rc;
	}

	rc = cam_vfe_top_init(vfe_hw_info->top_version, soc_info, hw_intf,
		vfe_hw_info->top_hw_info, core_info->vfe_irq_controller,
		&core_info->vfe_top);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error, cam_vfe_top_init failed rc = %d", rc);
		goto deinit_controller;
	}

	rc = cam_vfe_bus_init(vfe_hw_info->bus_version, BUS_TYPE_WR,
		soc_info, hw_intf, vfe_hw_info->bus_hw_info,
		core_info->vfe_irq_controller, &core_info->vfe_bus);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error, cam_vfe_bus_init failed rc = %d", rc);
		goto deinit_top;
	}

	/* Read Bus is not valid for vfe-lite */
	if (!soc_private->is_ife_lite) {
		rc = cam_vfe_bus_init(vfe_hw_info->bus_rd_version, BUS_TYPE_RD,
			soc_info, hw_intf, vfe_hw_info->bus_rd_hw_info,
			core_info->vfe_irq_controller, &core_info->vfe_rd_bus);
		if (rc) {
			CAM_WARN(CAM_ISP, "Error, RD cam_vfe_bus_init failed");
			rc = 0;
		}
		CAM_DBG(CAM_ISP, "vfe_bus_rd %pK hw_idx %d",
			core_info->vfe_rd_bus, hw_intf->hw_idx);
	}

	spin_lock_init(&core_info->spin_lock);

	return rc;

deinit_top:
	cam_vfe_top_deinit(vfe_hw_info->top_version,
		&core_info->vfe_top);

deinit_controller:
	cam_irq_controller_deinit(&core_info->vfe_irq_controller);

	return rc;
}

int cam_vfe_core_deinit(struct cam_vfe_hw_core_info  *core_info,
	struct cam_vfe_hw_info                       *vfe_hw_info)
{
	int                rc = -EINVAL;
	unsigned long      flags;

	spin_lock_irqsave(&core_info->spin_lock, flags);

	rc = cam_vfe_bus_deinit(vfe_hw_info->bus_version,
		&core_info->vfe_bus);
	if (rc)
		CAM_ERR(CAM_ISP, "Error cam_vfe_bus_deinit failed rc=%d", rc);

	rc = cam_vfe_top_deinit(vfe_hw_info->top_version,
		&core_info->vfe_top);
	if (rc)
		CAM_ERR(CAM_ISP, "Error cam_vfe_top_deinit failed rc=%d", rc);

	rc = cam_irq_controller_deinit(&core_info->vfe_irq_controller);
	if (rc)
		CAM_ERR(CAM_ISP,
			"Error cam_irq_controller_deinit failed rc=%d", rc);

	spin_unlock_irqrestore(&core_info->spin_lock, flags);

	return rc;
}
