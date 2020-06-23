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

#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/timer.h>

#include "cam_io_util.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "jpeg_dma_core.h"
#include "jpeg_dma_soc.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_jpeg_hw_intf.h"
#include "cam_jpeg_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"

#define CAM_JPEG_HW_IRQ_IS_FRAME_DONE(jpeg_irq_status, hi) \
	((jpeg_irq_status) & (hi)->int_status.framedone)
#define CAM_JPEG_HW_IRQ_IS_RESET_ACK(jpeg_irq_status, hi) \
	((jpeg_irq_status) & (hi)->int_status.resetdone)
#define CAM_JPEG_HW_IRQ_IS_STOP_DONE(jpeg_irq_status, hi) \
	((jpeg_irq_status) & (hi)->int_status.stopdone)

#define CAM_JPEG_DMA_RESET_TIMEOUT msecs_to_jiffies(500)

int cam_jpeg_dma_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_dma_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_dma_device_core_info *core_info = NULL;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote;
	int rc;

	if (!device_priv) {
		CAM_ERR(CAM_JPEG, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &jpeg_dma_dev->soc_info;
	core_info = (struct cam_jpeg_dma_device_core_info *)
		jpeg_dma_dev->core_info;

	if (!soc_info || !core_info) {
		CAM_ERR(CAM_JPEG, "soc_info = %pK core_info = %pK",
			soc_info, core_info);
		return -EINVAL;
	}

	mutex_lock(&core_info->core_mutex);
	if (++core_info->ref_count > 1) {
		mutex_unlock(&core_info->core_mutex);
		return 0;
	}

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_SVS_VOTE;
	axi_vote.compressed_bw = JPEG_VOTE;
	axi_vote.compressed_bw_ab = JPEG_VOTE;
	axi_vote.uncompressed_bw = JPEG_VOTE;

	rc = cam_cpas_start(core_info->cpas_handle,
		&ahb_vote, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_JPEG, "cpass start failed: %d", rc);
		goto cpas_failed;
	}

	rc = cam_jpeg_dma_enable_soc_resources(soc_info);
	if (rc) {
		CAM_ERR(CAM_JPEG, "soc enable is failed %d", rc);
		goto soc_failed;
	}

	mutex_unlock(&core_info->core_mutex);

	return 0;

soc_failed:
	cam_cpas_stop(core_info->cpas_handle);
cpas_failed:
	--core_info->ref_count;
	mutex_unlock(&core_info->core_mutex);

	return rc;
}

int cam_jpeg_dma_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_dma_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_dma_device_core_info *core_info = NULL;
	int rc;

	if (!device_priv) {
		CAM_ERR(CAM_JPEG, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &jpeg_dma_dev->soc_info;
	core_info = (struct cam_jpeg_dma_device_core_info *)
		jpeg_dma_dev->core_info;
	if (!soc_info || !core_info) {
		CAM_ERR(CAM_JPEG, "soc_info = %pK core_info = %pK",
			soc_info, core_info);
		return -EINVAL;
	}

	mutex_lock(&core_info->core_mutex);
	if (--core_info->ref_count > 0) {
		mutex_unlock(&core_info->core_mutex);
		return 0;
	}

	if (core_info->ref_count < 0) {
		CAM_ERR(CAM_JPEG, "ref cnt %d", core_info->ref_count);
		core_info->ref_count = 0;
		mutex_unlock(&core_info->core_mutex);
		return -EFAULT;
	}

	rc = cam_jpeg_dma_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_JPEG, "soc disable failed %d", rc);

	rc = cam_cpas_stop(core_info->cpas_handle);
	if (rc)
		CAM_ERR(CAM_JPEG, "cpas stop failed: %d", rc);

	mutex_unlock(&core_info->core_mutex);

	return 0;
}

irqreturn_t cam_jpeg_dma_irq(int irq_num, void *data)
{
	struct cam_hw_info *jpeg_dma_dev = data;
	struct cam_jpeg_dma_device_core_info *core_info = NULL;
	uint32_t irq_status = 0;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_dma_device_hw_info *hw_info = NULL;
	void __iomem *mem_base;

	if (!jpeg_dma_dev) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return IRQ_HANDLED;
	}
	soc_info = &jpeg_dma_dev->soc_info;
	core_info = (struct cam_jpeg_dma_device_core_info *)
		jpeg_dma_dev->core_info;
	hw_info = core_info->jpeg_dma_hw_info;
	mem_base = soc_info->reg_map[0].mem_base;

	irq_status = cam_io_r_mb(mem_base +
		core_info->jpeg_dma_hw_info->reg_offset.int_status);
	cam_io_w_mb(irq_status,
		soc_info->reg_map[0].mem_base +
		core_info->jpeg_dma_hw_info->reg_offset.int_clr);
	CAM_DBG(CAM_JPEG, "irq_num %d  irq_status = %x , core_state %d",
		irq_num, irq_status, core_info->core_state);
	if (CAM_JPEG_HW_IRQ_IS_FRAME_DONE(irq_status, hw_info)) {
		spin_lock(&jpeg_dma_dev->hw_lock);
		if (core_info->core_state == CAM_JPEG_DMA_CORE_READY) {
			core_info->result_size = 1;
			CAM_DBG(CAM_JPEG, "result_size %d",
				core_info->result_size);
			core_info->core_state =
				CAM_JPEG_DMA_CORE_RESETTING_ON_DONE;
			cam_io_w_mb(hw_info->reg_val.reset_cmd,
				mem_base + hw_info->reg_offset.reset_cmd);
		} else {
			CAM_WARN(CAM_JPEG, "unexpected frame done ");
			core_info->core_state = CAM_JPEG_DMA_CORE_NOT_READY;
		}
		spin_unlock(&jpeg_dma_dev->hw_lock);
	}

	if (CAM_JPEG_HW_IRQ_IS_RESET_ACK(irq_status, hw_info)) {
		spin_lock(&jpeg_dma_dev->hw_lock);
		if (core_info->core_state == CAM_JPEG_DMA_CORE_RESETTING) {
			core_info->core_state = CAM_JPEG_DMA_CORE_READY;
			core_info->result_size = -1;
			complete(&jpeg_dma_dev->hw_complete);
		} else if (core_info->core_state ==
			CAM_JPEG_DMA_CORE_RESETTING_ON_DONE) {
			if (core_info->irq_cb.jpeg_hw_mgr_cb) {
				core_info->irq_cb.jpeg_hw_mgr_cb(irq_status,
					core_info->result_size,
					core_info->irq_cb.data);
			} else {
				CAM_WARN(CAM_JPEG, "unexpected frame done");
			}
			core_info->result_size = -1;
			core_info->core_state = CAM_JPEG_DMA_CORE_NOT_READY;
		} else {
			CAM_ERR(CAM_JPEG, "unexpected reset irq");
		}
		spin_unlock(&jpeg_dma_dev->hw_lock);
	}
	if (CAM_JPEG_HW_IRQ_IS_STOP_DONE(irq_status, hw_info)) {
		spin_lock(&jpeg_dma_dev->hw_lock);
		if (core_info->core_state == CAM_JPEG_DMA_CORE_ABORTING) {
			core_info->core_state = CAM_JPEG_DMA_CORE_NOT_READY;
			complete(&jpeg_dma_dev->hw_complete);
			if (core_info->irq_cb.jpeg_hw_mgr_cb) {
				core_info->irq_cb.jpeg_hw_mgr_cb(irq_status,
					-1,
					core_info->irq_cb.data);
			}
		} else {
			CAM_ERR(CAM_JPEG, "unexpected abort irq");
		}
		spin_unlock(&jpeg_dma_dev->hw_lock);
	}

	return IRQ_HANDLED;
}

int cam_jpeg_dma_reset_hw(void *data,
	void *start_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_dma_dev = data;
	struct cam_jpeg_dma_device_core_info *core_info = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_dma_device_hw_info *hw_info = NULL;
	void __iomem *mem_base;
	unsigned long rem_jiffies;

	if (!jpeg_dma_dev) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}
	/* maskdisable.clrirq.maskenable.resetcmd */
	soc_info = &jpeg_dma_dev->soc_info;
	core_info = (struct cam_jpeg_dma_device_core_info *)
		jpeg_dma_dev->core_info;
	hw_info = core_info->jpeg_dma_hw_info;
	mem_base = soc_info->reg_map[0].mem_base;

	mutex_lock(&core_info->core_mutex);
	spin_lock(&jpeg_dma_dev->hw_lock);
	if (core_info->core_state == CAM_JPEG_DMA_CORE_RESETTING) {
		CAM_ERR(CAM_JPEG, "dma alrady resetting");
		spin_unlock(&jpeg_dma_dev->hw_lock);
		mutex_unlock(&core_info->core_mutex);
		return 0;
	}

	reinit_completion(&jpeg_dma_dev->hw_complete);

	core_info->core_state = CAM_JPEG_DMA_CORE_RESETTING;
	spin_unlock(&jpeg_dma_dev->hw_lock);

	cam_io_w_mb(hw_info->reg_val.int_mask_disable_all,
		mem_base + hw_info->reg_offset.int_mask);
	cam_io_w_mb(hw_info->reg_val.int_clr_clearall,
		mem_base + hw_info->reg_offset.int_clr);
	cam_io_w_mb(hw_info->reg_val.int_mask_enable_all,
		mem_base + hw_info->reg_offset.int_mask);
	cam_io_w_mb(hw_info->reg_val.reset_cmd,
		mem_base + hw_info->reg_offset.reset_cmd);

	rem_jiffies = wait_for_completion_timeout(&jpeg_dma_dev->hw_complete,
		CAM_JPEG_DMA_RESET_TIMEOUT);
	if (!rem_jiffies) {
		CAM_ERR(CAM_JPEG, "dma error Reset Timeout");
		core_info->core_state = CAM_JPEG_DMA_CORE_NOT_READY;
	}

	mutex_unlock(&core_info->core_mutex);
	return 0;
}

int cam_jpeg_dma_start_hw(void *data,
	void *start_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_dma_dev = data;
	struct cam_jpeg_dma_device_core_info *core_info = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_dma_device_hw_info *hw_info = NULL;
	void __iomem *mem_base;

	if (!jpeg_dma_dev) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}

	soc_info = &jpeg_dma_dev->soc_info;
	core_info = (struct cam_jpeg_dma_device_core_info *)
		jpeg_dma_dev->core_info;
	hw_info = core_info->jpeg_dma_hw_info;
	mem_base = soc_info->reg_map[0].mem_base;

	if (core_info->core_state != CAM_JPEG_DMA_CORE_READY) {
		CAM_ERR(CAM_JPEG, "Error not ready");
		return -EINVAL;
	}

	CAM_DBG(CAM_JPEG, "Starting DMA");
	cam_io_w_mb(0x00000601,
		mem_base + hw_info->reg_offset.int_mask);
	cam_io_w_mb(hw_info->reg_val.hw_cmd_start,
		mem_base + hw_info->reg_offset.hw_cmd);

	return 0;
}

int cam_jpeg_dma_stop_hw(void *data,
	void *stop_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_dma_dev = data;
	struct cam_jpeg_dma_device_core_info *core_info = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_dma_device_hw_info *hw_info = NULL;
	void __iomem *mem_base;
	unsigned long rem_jiffies;

	if (!jpeg_dma_dev) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}
	soc_info = &jpeg_dma_dev->soc_info;
	core_info = (struct cam_jpeg_dma_device_core_info *)
		jpeg_dma_dev->core_info;
	hw_info = core_info->jpeg_dma_hw_info;
	mem_base = soc_info->reg_map[0].mem_base;

	mutex_lock(&core_info->core_mutex);
	spin_lock(&jpeg_dma_dev->hw_lock);
	if (core_info->core_state == CAM_JPEG_DMA_CORE_ABORTING) {
		CAM_ERR(CAM_JPEG, "alrady stopping");
		spin_unlock(&jpeg_dma_dev->hw_lock);
		mutex_unlock(&core_info->core_mutex);
		return 0;
	}

	reinit_completion(&jpeg_dma_dev->hw_complete);
	core_info->core_state = CAM_JPEG_DMA_CORE_ABORTING;
	spin_unlock(&jpeg_dma_dev->hw_lock);

	cam_io_w_mb(hw_info->reg_val.hw_cmd_stop,
		mem_base + hw_info->reg_offset.hw_cmd);

	rem_jiffies = wait_for_completion_timeout(&jpeg_dma_dev->hw_complete,
		CAM_JPEG_DMA_RESET_TIMEOUT);
	if (!rem_jiffies) {
		CAM_ERR(CAM_JPEG, "error Reset Timeout");
		core_info->core_state = CAM_JPEG_DMA_CORE_NOT_READY;
	}

	mutex_unlock(&core_info->core_mutex);
	return 0;
}

int cam_jpeg_dma_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_dma_dev = device_priv;
	struct cam_jpeg_dma_device_core_info *core_info = NULL;
	int rc;

	if (!device_priv) {
		CAM_ERR(CAM_JPEG, "Invalid arguments");
		return -EINVAL;
	}

	if (cmd_type >= CAM_JPEG_CMD_MAX) {
		CAM_ERR(CAM_JPEG, "Invalid command : %x", cmd_type);
		return -EINVAL;
	}

	core_info = (struct cam_jpeg_dma_device_core_info *)
		jpeg_dma_dev->core_info;

	switch (cmd_type) {
	case CAM_JPEG_CMD_SET_IRQ_CB:
	{
		struct cam_jpeg_set_irq_cb *irq_cb = cmd_args;

		if (!cmd_args) {
			CAM_ERR(CAM_JPEG, "cmd args NULL");
			return -EINVAL;
		}
		if (irq_cb->b_set_cb) {
			core_info->irq_cb.jpeg_hw_mgr_cb =
				irq_cb->jpeg_hw_mgr_cb;
			core_info->irq_cb.data = irq_cb->data;
		} else {
			core_info->irq_cb.jpeg_hw_mgr_cb = NULL;
			core_info->irq_cb.data = NULL;
		}
		rc = 0;
		break;
	}
	default:
		rc = -EINVAL;
		break;
	}
	if (rc)
		CAM_ERR(CAM_JPEG, "error cmdtype %d rc = %d", cmd_type, rc);
	return rc;
}
