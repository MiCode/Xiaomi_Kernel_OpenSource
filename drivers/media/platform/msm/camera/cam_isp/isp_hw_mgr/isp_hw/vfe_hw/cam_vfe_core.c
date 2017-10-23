/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include "cam_tasklet_util.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_vfe_soc.h"
#include "cam_vfe_core.h"
#include "cam_vfe_bus.h"
#include "cam_vfe_top.h"
#include "cam_ife_hw_mgr.h"
#include "cam_debug_util.h"

static const char drv_name[] = "vfe";

static uint32_t irq_reg_offset[CAM_IFE_IRQ_REGISTERS_MAX] = {
	0x0000006C,
	0x00000070,
	0x0000007C,
};

static uint32_t camif_irq_reg_mask[CAM_IFE_IRQ_REGISTERS_MAX] = {
	0x0003FD1F,
	0x0FFF7EBC,
};

static uint32_t rdi_irq_reg_mask[CAM_IFE_IRQ_REGISTERS_MAX] = {
	0x780000e0,
	0x00000000,
};

static uint32_t top_reset_irq_reg_mask[CAM_IFE_IRQ_REGISTERS_MAX] = {
	0x80000000,
	0x00000000,
};

static int cam_vfe_get_evt_payload(struct cam_vfe_hw_core_info *core_info,
	struct cam_vfe_top_irq_evt_payload    **evt_payload)
{
	spin_lock(&core_info->spin_lock);
	if (list_empty(&core_info->free_payload_list)) {
		*evt_payload = NULL;
		spin_unlock(&core_info->spin_lock);
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free payload, core info 0x%x\n",
			core_info->cpas_handle);
		return -ENODEV;
	}

	*evt_payload = list_first_entry(&core_info->free_payload_list,
		struct cam_vfe_top_irq_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
	spin_unlock(&core_info->spin_lock);

	return 0;
}

int cam_vfe_put_evt_payload(void             *core_info,
	struct cam_vfe_top_irq_evt_payload  **evt_payload)
{
	struct cam_vfe_hw_core_info        *vfe_core_info = core_info;
	unsigned long                       flags;

	if (!core_info) {
		CAM_ERR(CAM_ISP, "Invalid param core_info NULL");
		return -EINVAL;
	}
	if (*evt_payload == NULL) {
		CAM_ERR(CAM_ISP, "No payload to put");
		return -EINVAL;
	}

	spin_lock_irqsave(&vfe_core_info->spin_lock, flags);
	list_add_tail(&(*evt_payload)->list, &vfe_core_info->free_payload_list);
	spin_unlock_irqrestore(&vfe_core_info->spin_lock, flags);

	*evt_payload = NULL;
	return 0;
}

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
	struct cam_vfe_irq_handler_priv   *handler_priv;

	handler_priv = th_payload->handler_priv;

	CAM_DBG(CAM_ISP, "Enter");
	CAM_DBG(CAM_ISP, "IRQ status_0 = 0x%x", th_payload->evt_status_arr[0]);

	if (th_payload->evt_status_arr[0] & (1<<31)) {
		/*
		 * Clear All IRQs to avoid spurious IRQs immediately
		 * after Reset Done.
		 */
		cam_io_w(0xFFFFFFFF, handler_priv->mem_base + 0x64);
		cam_io_w(0xFFFFFFFF, handler_priv->mem_base + 0x68);
		cam_io_w(0x1, handler_priv->mem_base + 0x58);
		CAM_DBG(CAM_ISP, "Calling Complete for RESET CMD");
		complete(handler_priv->reset_complete);


		rc = 0;
	}

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

int cam_vfe_init_hw(void *hw_priv, void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info                *vfe_hw = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_vfe_hw_core_info       *core_info = NULL;
	int rc = 0;

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

	CAM_DBG(CAM_ISP, "Enable soc done");

	/* Do HW Reset */
	rc = cam_vfe_reset(hw_priv, NULL, 0);
	if (rc) {
		CAM_ERR(CAM_ISP, "Reset Failed rc=%d", rc);
		goto disable_soc;
	}

	rc = core_info->vfe_bus->hw_ops.init(core_info->vfe_bus->bus_priv,
		NULL, 0);
	if (rc) {
		CAM_ERR(CAM_ISP, "Bus HW init Failed rc=%d", rc);
		goto disable_soc;
	}

	return 0;
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
	int rc = 0;

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
	struct cam_hw_info                *vfe_hw  = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_vfe_hw_core_info       *core_info = NULL;
	int rc;

	CAM_DBG(CAM_ISP, "Enter");

	if (!hw_priv) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	soc_info = &vfe_hw->soc_info;
	core_info = (struct cam_vfe_hw_core_info *)vfe_hw->core_info;

	core_info->irq_payload.core_index = soc_info->index;
	core_info->irq_payload.mem_base =
		vfe_hw->soc_info.reg_map[VFE_CORE_BASE_IDX].mem_base;
	core_info->irq_payload.core_info = core_info;
	core_info->irq_payload.reset_complete = &vfe_hw->hw_complete;

	core_info->irq_handle = cam_irq_controller_subscribe_irq(
		core_info->vfe_irq_controller, CAM_IRQ_PRIORITY_0,
		top_reset_irq_reg_mask, &core_info->irq_payload,
		cam_vfe_reset_irq_top_half, NULL, NULL, NULL);
	if (core_info->irq_handle < 0) {
		CAM_ERR(CAM_ISP, "subscribe irq controller failed");
		return -EFAULT;
	}

	reinit_completion(&vfe_hw->hw_complete);

	CAM_DBG(CAM_ISP, "calling RESET");
	core_info->vfe_top->hw_ops.reset(core_info->vfe_top->top_priv, NULL, 0);
	CAM_DBG(CAM_ISP, "waiting for vfe reset complete");
	/* Wait for Completion or Timeout of 500ms */
	rc = wait_for_completion_timeout(&vfe_hw->hw_complete, 500);
	if (!rc)
		CAM_ERR(CAM_ISP, "Error! Reset Timeout");

	CAM_DBG(CAM_ISP, "reset complete done (%d)", rc);

	rc = cam_irq_controller_unsubscribe_irq(
		core_info->vfe_irq_controller, core_info->irq_handle);
	if (rc)
		CAM_ERR(CAM_ISP, "Error! Unsubscribe failed");

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


int cam_vfe_irq_top_half(uint32_t    evt_id,
	struct cam_irq_th_payload   *th_payload)
{
	int32_t                              rc;
	int                                  i;
	struct cam_vfe_irq_handler_priv     *handler_priv;
	struct cam_vfe_top_irq_evt_payload  *evt_payload;

	handler_priv = th_payload->handler_priv;

	CAM_DBG(CAM_ISP, "IRQ status_0 = %x", th_payload->evt_status_arr[0]);
	CAM_DBG(CAM_ISP, "IRQ status_1 = %x", th_payload->evt_status_arr[1]);

	rc  = cam_vfe_get_evt_payload(handler_priv->core_info, &evt_payload);
	if (rc) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"No tasklet_cmd is free in queue\n");
		return rc;
	}

	cam_isp_hw_get_timestamp(&evt_payload->ts);

	evt_payload->core_index = handler_priv->core_index;
	evt_payload->core_info  = handler_priv->core_info;
	evt_payload->evt_id  = evt_id;

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	for (; i < CAM_IFE_IRQ_REGISTERS_MAX; i++) {
		evt_payload->irq_reg_val[i] = cam_io_r(handler_priv->mem_base +
			irq_reg_offset[i]);
	}
	CAM_DBG(CAM_ISP, "Violation status = %x", evt_payload->irq_reg_val[2]);

	/*
	 *  need to handle overflow condition here, otherwise irq storm
	 *  will block everything.
	 */
	if (evt_payload->irq_reg_val[1]) {
		CAM_ERR(CAM_ISP,
			"Encountered Error Irq_status1=0x%x. Stopping further IRQ processing from this HW",
			evt_payload->irq_reg_val[1]);
		CAM_ERR(CAM_ISP, "Violation status = %x",
			evt_payload->irq_reg_val[2]);
		cam_io_w(0, handler_priv->mem_base + 0x60);
		cam_io_w(0, handler_priv->mem_base + 0x5C);

		evt_payload->error_type = CAM_ISP_HW_ERROR_OVERFLOW;
	}

	th_payload->evt_payload_priv = evt_payload;

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
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

	mutex_lock(&vfe_hw->hw_mutex);
	if (acquire->rsrc_type == CAM_ISP_RESOURCE_VFE_IN)
		rc = core_info->vfe_top->hw_ops.reserve(
			core_info->vfe_top->top_priv,
			acquire,
			sizeof(*acquire));
	else if (acquire->rsrc_type == CAM_ISP_RESOURCE_VFE_OUT)
		rc = core_info->vfe_bus->hw_ops.reserve(
			core_info->vfe_bus->bus_priv, acquire,
			sizeof(*acquire));
	else
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
	else
		CAM_ERR(CAM_ISP, "Invalid res type:%d", isp_res->res_type);

	mutex_unlock(&vfe_hw->hw_mutex);

	return rc;
}


int cam_vfe_start(void *hw_priv, void *start_args, uint32_t arg_size)
{
	struct cam_vfe_hw_core_info       *core_info = NULL;
	struct cam_hw_info                *vfe_hw  = hw_priv;
	struct cam_isp_resource_node      *isp_res;
	int rc = -ENODEV;

	if (!hw_priv || !start_args ||
		(arg_size != sizeof(struct cam_isp_resource_node))) {
		CAM_ERR(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	core_info = (struct cam_vfe_hw_core_info *)vfe_hw->core_info;
	isp_res = (struct cam_isp_resource_node  *)start_args;

	mutex_lock(&vfe_hw->hw_mutex);
	if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_IN) {
		if (isp_res->res_id == CAM_ISP_HW_VFE_IN_CAMIF)
			isp_res->irq_handle = cam_irq_controller_subscribe_irq(
				core_info->vfe_irq_controller,
				CAM_IRQ_PRIORITY_1,
				camif_irq_reg_mask, &core_info->irq_payload,
				cam_vfe_irq_top_half, cam_ife_mgr_do_tasklet,
				isp_res->tasklet_info, cam_tasklet_enqueue_cmd);
		else
			isp_res->irq_handle = cam_irq_controller_subscribe_irq(
				core_info->vfe_irq_controller,
				CAM_IRQ_PRIORITY_1,
				rdi_irq_reg_mask, &core_info->irq_payload,
				cam_vfe_irq_top_half, cam_ife_mgr_do_tasklet,
				isp_res->tasklet_info, cam_tasklet_enqueue_cmd);

		if (isp_res->irq_handle > 0)
			rc = core_info->vfe_top->hw_ops.start(
				core_info->vfe_top->top_priv, isp_res,
				sizeof(struct cam_isp_resource_node));
		else
			CAM_ERR(CAM_ISP,
				"Error! subscribe irq controller failed");
	} else if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_OUT) {
		rc = core_info->vfe_bus->hw_ops.start(isp_res, NULL, 0);
	} else {
		CAM_ERR(CAM_ISP, "Invalid res type:%d", isp_res->res_type);
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
		cam_irq_controller_unsubscribe_irq(
			core_info->vfe_irq_controller, isp_res->irq_handle);
		rc = core_info->vfe_top->hw_ops.stop(
			core_info->vfe_top->top_priv, isp_res,
			sizeof(struct cam_isp_resource_node));
	} else if (isp_res->res_type == CAM_ISP_RESOURCE_VFE_OUT) {
		rc = core_info->vfe_bus->hw_ops.stop(isp_res, NULL, 0);
	} else {
		CAM_ERR(CAM_ISP, "Invalid res type:%d", isp_res->res_type);
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
	case CAM_VFE_HW_CMD_GET_CHANGE_BASE:
	case CAM_VFE_HW_CMD_GET_REG_UPDATE:
		rc = core_info->vfe_top->hw_ops.process_cmd(
			core_info->vfe_top->top_priv, cmd_type, cmd_args,
			arg_size);

		break;
	case CAM_VFE_HW_CMD_GET_BUF_UPDATE:
	case CAM_VFE_HW_CMD_GET_HFR_UPDATE:
	case CAM_VFE_HW_CMD_STRIPE_UPDATE:
		rc = core_info->vfe_bus->hw_ops.process_cmd(
			core_info->vfe_bus->bus_priv, cmd_type, cmd_args,
			arg_size);
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
	int i;

	CAM_DBG(CAM_ISP, "Enter");

	rc = cam_irq_controller_init(drv_name,
		CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX),
		vfe_hw_info->irq_reg_info, &core_info->vfe_irq_controller);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error! cam_irq_controller_init failed");
		return rc;
	}

	rc = cam_vfe_top_init(vfe_hw_info->top_version,
		soc_info, hw_intf, vfe_hw_info->top_hw_info,
		&core_info->vfe_top);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error! cam_vfe_top_init failed");
		goto deinit_controller;
	}

	rc = cam_vfe_bus_init(vfe_hw_info->bus_version, soc_info, hw_intf,
		vfe_hw_info->bus_hw_info, core_info->vfe_irq_controller,
		&core_info->vfe_bus);
	if (rc) {
		CAM_ERR(CAM_ISP, "Error! cam_vfe_bus_init failed");
		goto deinit_top;
	}

	INIT_LIST_HEAD(&core_info->free_payload_list);
	for (i = 0; i < CAM_VFE_EVT_MAX; i++) {
		INIT_LIST_HEAD(&core_info->evt_payload[i].list);
		list_add_tail(&core_info->evt_payload[i].list,
			&core_info->free_payload_list);
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
	int                i;
	unsigned long      flags;

	spin_lock_irqsave(&core_info->spin_lock, flags);

	INIT_LIST_HEAD(&core_info->free_payload_list);
	for (i = 0; i < CAM_VFE_EVT_MAX; i++)
		INIT_LIST_HEAD(&core_info->evt_payload[i].list);

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

