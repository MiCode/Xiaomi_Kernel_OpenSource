// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/ratelimit.h>
#include "cam_custom_sub_mod_core.h"

int cam_custom_hw_sub_mod_get_hw_caps(void *hw_priv,
	void *get_hw_cap_args, uint32_t arg_size)
{
	int rc = 0;

	if (!hw_priv) {
		CAM_ERR(CAM_CUSTOM, "Invalid arguments");
		return -EINVAL;
	}
	/* Add HW Capabilities to be published */
	return rc;
}

int cam_custom_hw_sub_mod_init_hw(void *hw_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info                *custom_hw = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_custom_resource_node   *custom_res = NULL;
	int rc = 0;

	if (!hw_priv) {
		CAM_ERR(CAM_CUSTOM, "Invalid arguments");
		return -EINVAL;
	}

	mutex_lock(&custom_hw->hw_mutex);
	custom_hw->open_count++;
	if (custom_hw->open_count > 1) {
		mutex_unlock(&custom_hw->hw_mutex);
		CAM_DBG(CAM_CUSTOM,
			"Cam Custom has already been initialized cnt %d",
			custom_hw->open_count);
		return 0;
	}
	mutex_unlock(&custom_hw->hw_mutex);

	soc_info = &custom_hw->soc_info;

	/* Turn ON Regulators, Clocks and other SOC resources */
	rc = cam_custom_hw_sub_mod_enable_soc_resources(soc_info);
	if (rc) {
		CAM_ERR(CAM_CUSTOM, "Enable SOC failed");
		rc = -EFAULT;
		goto decrement_open_cnt;
	}

	custom_res   = (struct cam_custom_resource_node *)init_hw_args;
	if (custom_res && custom_res->init) {
		rc = custom_res->init(custom_res, NULL, 0);
		if (rc) {
			CAM_ERR(CAM_CUSTOM, "init Failed rc=%d", rc);
			goto decrement_open_cnt;
		}
	}

	rc = cam_custom_hw_sub_mod_reset(hw_priv, NULL, 0);
	if (rc < 0) {
		CAM_ERR(CAM_CUSTOM, "Custom HW reset failed : %d", rc);
		goto decrement_open_cnt;
	}
	/* Initialize all resources here */
	custom_hw->hw_state = CAM_HW_STATE_POWER_UP;
	return rc;

decrement_open_cnt:
	mutex_lock(&custom_hw->hw_mutex);
	custom_hw->open_count--;
	mutex_unlock(&custom_hw->hw_mutex);
	return rc;
}

int cam_custom_hw_sub_mod_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size)
{
	struct cam_hw_info                *custom_hw = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	struct cam_custom_resource_node   *custom_res = NULL;
	int rc = 0;

	if (!hw_priv) {
		CAM_ERR(CAM_CUSTOM, "Invalid arguments");
		return -EINVAL;
	}

	mutex_lock(&custom_hw->hw_mutex);
	if (!custom_hw->open_count) {
		mutex_unlock(&custom_hw->hw_mutex);
		CAM_ERR(CAM_CUSTOM, "Error! Unbalanced deinit");
		return -EFAULT;
	}
	custom_hw->open_count--;
	if (custom_hw->open_count) {
		mutex_unlock(&custom_hw->hw_mutex);
		CAM_DBG(CAM_CUSTOM,
			"open_cnt non-zero =%d", custom_hw->open_count);
		return 0;
	}
	mutex_unlock(&custom_hw->hw_mutex);

	soc_info = &custom_hw->soc_info;

	custom_res   = (struct cam_custom_resource_node *)deinit_hw_args;
	if (custom_res && custom_res->deinit) {
		rc = custom_res->deinit(custom_res, NULL, 0);
		if (rc)
			CAM_ERR(CAM_CUSTOM, "deinit failed");
	}

	rc = cam_custom_hw_sub_mod_reset(hw_priv, NULL, 0);

	/* Turn OFF Regulators, Clocks and other SOC resources */
	CAM_DBG(CAM_CUSTOM, "Disable SOC resource");
	rc = cam_custom_hw_sub_mod_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "Disable SOC failed");

	custom_hw->hw_state = CAM_HW_STATE_POWER_DOWN;

	return rc;
}

int cam_custom_hw_sub_mod_reset(void *hw_priv,
	void *reserve_args, uint32_t arg_size)
{
	struct cam_hw_info                *custom_hw  = hw_priv;
	struct cam_hw_soc_info            *soc_info = NULL;
	int rc = 0;

	if (!hw_priv) {
		CAM_ERR(CAM_CUSTOM, "Invalid input arguments");
		return -EINVAL;
	}

	soc_info = &custom_hw->soc_info;
	/* Do Reset of HW */
	return rc;
}

int cam_custom_hw_sub_mod_reserve(void *hw_priv,
	void *reserve_args, uint32_t arg_size)
{
	int rc = 0;

	if (!hw_priv || !reserve_args) {
		CAM_ERR(CAM_CUSTOM, "Invalid input arguments");
		return -EINVAL;
	}

	/*Reserve Args */
	return rc;
}


int cam_custom_hw_sub_mod_release(void *hw_priv,
	void *release_args, uint32_t arg_size)
{
	struct cam_hw_info                *custom_hw  = hw_priv;
	int rc = 0;

	if (!hw_priv || !release_args) {
		CAM_ERR(CAM_CUSTOM, "Invalid input arguments");
		return -EINVAL;
	}

	mutex_lock(&custom_hw->hw_mutex);
	/* Release Resources */
	mutex_unlock(&custom_hw->hw_mutex);

	return rc;
}


int cam_custom_hw_sub_mod_start(void *hw_priv,
	void *start_args, uint32_t arg_size)
{
	struct cam_hw_info                *custom_hw  = hw_priv;
	int rc = 0;

	if (!hw_priv || !start_args) {
		CAM_ERR(CAM_CUSTOM, "Invalid input arguments");
		return -EINVAL;
	}

	mutex_lock(&custom_hw->hw_mutex);
	/* Start HW -- Stream On*/
	mutex_unlock(&custom_hw->hw_mutex);

	return rc;
}

int cam_custom_hw_sub_mod_stop(void *hw_priv,
	void *stop_args, uint32_t arg_size)
{
	struct cam_hw_info                *custom_hw  = hw_priv;
	int rc = 0;

	if (!hw_priv || !stop_args) {
		CAM_ERR(CAM_CUSTOM, "Invalid input arguments");
		return -EINVAL;
	}

	mutex_lock(&custom_hw->hw_mutex);
	/* Stop HW */
	mutex_unlock(&custom_hw->hw_mutex);

	return rc;
}

int cam_custom_hw_sub_mod_read(void *hw_priv,
	void *read_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_custom_hw_sub_mod_write(void *hw_priv,
	void *write_args, uint32_t arg_size)
{
	return -EPERM;
}

int cam_custom_hw_submit_req(void *hw_priv, void *hw_submit_args,
	uint32_t arg_size)
{
	struct cam_hw_info			         *custom_dev = hw_priv;
	struct cam_custom_sub_mod_req_to_dev *submit_req =
		(struct cam_custom_sub_mod_req_to_dev *)hw_submit_args;
	struct cam_custom_sub_mod_core_info  *core_info = NULL;

	core_info =
		(struct cam_custom_sub_mod_core_info *)custom_dev->core_info;

	spin_lock(&custom_dev->hw_lock);
	if (core_info->curr_req) {
		CAM_WARN(CAM_CUSTOM, "Req %lld still processed by %s",
			core_info->curr_req->req_id,
			custom_dev->soc_info.dev_name);
		spin_unlock(&custom_dev->hw_lock);
		return -EAGAIN;
	}

	core_info->curr_req = submit_req;
	spin_unlock(&custom_dev->hw_lock);

	/* Do other submit procedures */
	return 0;
}

irqreturn_t cam_custom_hw_sub_mod_irq(int irq_num, void *data)
{
	struct cam_hw_info *custom_dev = data;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_custom_hw_cb_args cb_args;
	struct cam_custom_sub_mod_core_info *core_info = NULL;
	uint32_t irq_status = 0;

	if (!data) {
		CAM_ERR(CAM_CUSTOM, "Invalid custom_dev_info");
		return IRQ_HANDLED;
	}

	soc_info = &custom_dev->soc_info;
	core_info =
		(struct cam_custom_sub_mod_core_info *)custom_dev->core_info;

	irq_status = cam_io_r_mb(soc_info->reg_map[0].mem_base +
				core_info->device_hw_info->irq_status);

	cam_io_w_mb(irq_status,
			soc_info->reg_map[0].mem_base +
			core_info->device_hw_info->irq_clear);

	spin_lock(&custom_dev->hw_lock);
	core_info->curr_req = NULL;
	if (core_info->irq_cb.custom_hw_mgr_cb)
		core_info->irq_cb.custom_hw_mgr_cb(
			core_info->irq_cb.data, &cb_args);
	spin_unlock(&custom_dev->hw_lock);

	return IRQ_HANDLED;
}

int cam_custom_hw_sub_mod_process_cmd(void *hw_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info                  *hw = hw_priv;
	struct cam_hw_soc_info              *soc_info = NULL;
	struct cam_custom_sub_mod_core_info *core_info = NULL;
	unsigned long flag = 0;
	int rc = 0;

	if (!hw_priv || !cmd_args) {
		CAM_ERR(CAM_CUSTOM, "Invalid arguments");
		return -EINVAL;
	}

	soc_info = &hw->soc_info;
	core_info = hw->core_info;
	/* Handle any custom process cmds */

	switch (cmd_type) {
	case CAM_CUSTOM_SET_IRQ_CB: {
		struct cam_custom_sub_mod_set_irq_cb *irq_cb = cmd_args;
		/* This can be deprecated */
		CAM_DBG(CAM_CUSTOM, "Setting irq cb");
		spin_lock_irqsave(&hw->hw_lock, flag);
		core_info->irq_cb.custom_hw_mgr_cb = irq_cb->custom_hw_mgr_cb;
		core_info->irq_cb.data = irq_cb->data;
		spin_unlock_irqrestore(&hw->hw_lock, flag);
		break;
	}
	case CAM_CUSTOM_SUBMIT_REQ: {
		rc = cam_custom_hw_submit_req(hw_priv, cmd_args, arg_size);
		break;
	}
	default:
		break;
	}

	return rc;
}


