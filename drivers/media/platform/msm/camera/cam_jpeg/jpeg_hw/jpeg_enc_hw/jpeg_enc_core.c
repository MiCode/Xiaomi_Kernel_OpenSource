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
#include "jpeg_enc_core.h"
#include "jpeg_enc_soc.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_enc_hw_intf.h"
#include "cam_jpeg_hw_intf.h"
#include "cam_jpeg_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"

#define CAM_JPEG_HW_IRQ_STATUS_FRAMEDONE_MASK 0x00000001
#define CAM_JPEG_HW_IRQ_STATUS_FRAMEDONE_SHIFT 0x00000000

#define CAM_JPEG_HW_IRQ_STATUS_RESET_ACK_MASK 0x10000000
#define CAM_JPEG_HW_IRQ_STATUS_RESET_ACK_SHIFT 0x0000000a

#define CAM_JPEG_HW_IRQ_STATUS_BUS_ERROR_MASK 0x00000800
#define CAM_JPEG_HW_IRQ_STATUS_BUS_ERROR_SHIFT 0x0000000b

#define CAM_JPEG_HW_IRQ_STATUS_DCD_UNESCAPED_FF      (0x1<<19)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_HUFFMAN_ERROR     (0x1<<20)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_COEFFICIENT_ERR   (0x1<<21)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_MISSING_BIT_STUFF (0x1<<22)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_SCAN_UNDERFLOW    (0x1<<23)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_INVALID_RSM       (0x1<<24)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_INVALID_RSM_SEQ   (0x1<<25)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_MISSING_RSM       (0x1<<26)
#define CAM_JPEG_HW_IRQ_STATUS_VIOLATION_MASK        (0x1<<29)

#define CAM_JPEG_HW_MASK_COMP_FRAMEDONE \
		CAM_JPEG_HW_IRQ_STATUS_FRAMEDONE_MASK
#define CAM_JPEG_HW_MASK_COMP_RESET_ACK \
		CAM_JPEG_HW_IRQ_STATUS_RESET_ACK_MASK
#define CAM_JPEG_HW_MASK_COMP_ERR \
		(CAM_JPEG_HW_IRQ_STATUS_DCD_UNESCAPED_FF | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_HUFFMAN_ERROR | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_COEFFICIENT_ERR | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_MISSING_BIT_STUFF | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_SCAN_UNDERFLOW | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_INVALID_RSM | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_INVALID_RSM_SEQ | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_MISSING_RSM | \
		CAM_JPEG_HW_IRQ_STATUS_VIOLATION_MASK)

#define CAM_JPEG_HW_IRQ_IS_FRAME_DONE(jpeg_irq_status) \
	(jpeg_irq_status & CAM_JPEG_HW_MASK_COMP_FRAMEDONE)
#define CAM_JPEG_HW_IRQ_IS_RESET_ACK(jpeg_irq_status) \
	(jpeg_irq_status & CAM_JPEG_HW_MASK_COMP_RESET_ACK)
#define CAM_JPEG_HW_IRQ_IS_ERR(jpeg_irq_status) \
	(jpeg_irq_status & CAM_JPEG_HW_MASK_COMP_ERR)

#define CAM_JPEG_ENC_RESET_TIMEOUT msecs_to_jiffies(500)

int cam_jpeg_enc_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_enc_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	struct cam_jpeg_cpas_vote cpas_vote;
	int rc;

	if (!device_priv) {
		CAM_ERR(CAM_JPEG, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &jpeg_enc_dev->soc_info;
	core_info =
		(struct cam_jpeg_enc_device_core_info *)jpeg_enc_dev->
		core_info;

	if (!soc_info || !core_info) {
		CAM_ERR(CAM_JPEG, "soc_info = %pK core_info = %pK",
			soc_info, core_info);
		return -EINVAL;
	}

	cpas_vote.ahb_vote.type = CAM_VOTE_ABSOLUTE;
	cpas_vote.ahb_vote.vote.level = CAM_SVS_VOTE;
	cpas_vote.axi_vote.compressed_bw = JPEG_TURBO_VOTE;
	cpas_vote.axi_vote.uncompressed_bw = JPEG_TURBO_VOTE;

	rc = cam_cpas_start(core_info->cpas_handle,
		&cpas_vote.ahb_vote, &cpas_vote.axi_vote);
	if (rc)
		CAM_ERR(CAM_JPEG, "cpass start failed: %d", rc);

	rc = cam_jpeg_enc_enable_soc_resources(soc_info);
	if (rc) {
		CAM_ERR(CAM_JPEG, "soc enable is failed %d", rc);
		cam_cpas_stop(core_info->cpas_handle);
	}

	return rc;
}

int cam_jpeg_enc_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_enc_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	int rc;

	if (!device_priv) {
		CAM_ERR(CAM_JPEG, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &jpeg_enc_dev->soc_info;
	core_info = (struct cam_jpeg_enc_device_core_info *)
		jpeg_enc_dev->core_info;
	if (!soc_info || !core_info) {
		CAM_ERR(CAM_JPEG, "soc_info = %pK core_info = %pK",
			soc_info, core_info);
		return -EINVAL;
	}

	rc = cam_jpeg_enc_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_JPEG, "soc enable failed %d", rc);

	rc = cam_cpas_stop(core_info->cpas_handle);
	if (rc)
		CAM_ERR(CAM_JPEG, "cpas stop failed: %d", rc);

	return 0;
}

irqreturn_t cam_jpeg_enc_irq(int irq_num, void *data)
{
	struct cam_hw_info *jpeg_enc_dev = data;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	uint32_t irq_status = 0;
	uint32_t encoded_size = 0;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_enc_device_hw_info *hw_info = NULL;
	void __iomem *mem_base;

	if (!jpeg_enc_dev) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return IRQ_HANDLED;
	}
	soc_info = &jpeg_enc_dev->soc_info;
	core_info =
		(struct cam_jpeg_enc_device_core_info *)jpeg_enc_dev->
		core_info;
	hw_info = core_info->jpeg_enc_hw_info;
	mem_base = soc_info->reg_map[0].mem_base;

	irq_status = cam_io_r_mb(mem_base +
		core_info->jpeg_enc_hw_info->int_status);

	cam_io_w_mb(irq_status,
		soc_info->reg_map[0].mem_base +
		core_info->jpeg_enc_hw_info->int_clr);

	CAM_DBG(CAM_JPEG, "irq_num %d  irq_status = %x , core_state %d",
		irq_num, irq_status, core_info->core_state);
	if (CAM_JPEG_HW_IRQ_IS_FRAME_DONE(irq_status)) {
		if (core_info->core_state == CAM_JPEG_ENC_CORE_READY) {
			encoded_size = cam_io_r_mb(mem_base + 0x180);
			if (core_info->irq_cb.jpeg_hw_mgr_cb) {
				core_info->irq_cb.jpeg_hw_mgr_cb(irq_status,
					encoded_size,
					core_info->irq_cb.data);
			} else {
				CAM_ERR(CAM_JPEG, "unexpected done");
			}
		}

		core_info->core_state = CAM_JPEG_ENC_CORE_NOT_READY;
	}
	if (CAM_JPEG_HW_IRQ_IS_RESET_ACK(irq_status)) {
		if (core_info->core_state == CAM_JPEG_ENC_CORE_RESETTING) {
			core_info->core_state = CAM_JPEG_ENC_CORE_READY;
			complete(&jpeg_enc_dev->hw_complete);
		} else {
			CAM_ERR(CAM_JPEG, "unexpected reset irq");
		}
	}
	/* Unexpected/unintended HW interrupt */
	if (CAM_JPEG_HW_IRQ_IS_ERR(irq_status)) {
		core_info->core_state = CAM_JPEG_ENC_CORE_NOT_READY;
		CAM_ERR_RATE_LIMIT(CAM_JPEG,
			"error irq_num %d  irq_status = %x , core_state %d",
			irq_num, irq_status, core_info->core_state);

		if (core_info->irq_cb.jpeg_hw_mgr_cb) {
			core_info->irq_cb.jpeg_hw_mgr_cb(irq_status,
				-1,
				core_info->irq_cb.data);
		}
	}

	return IRQ_HANDLED;
}

int cam_jpeg_enc_reset_hw(void *data,
	void *start_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_enc_dev = data;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_enc_device_hw_info *hw_info = NULL;
	void __iomem *mem_base;
	unsigned long rem_jiffies;

	if (!jpeg_enc_dev) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}
	/* maskdisable.clrirq.maskenable.resetcmd */
	soc_info = &jpeg_enc_dev->soc_info;
	core_info =
		(struct cam_jpeg_enc_device_core_info *)jpeg_enc_dev->
		core_info;
	hw_info = core_info->jpeg_enc_hw_info;
	mem_base = soc_info->reg_map[0].mem_base;

	if (core_info->core_state == CAM_JPEG_ENC_CORE_RESETTING) {
		CAM_ERR(CAM_JPEG, "alrady resetting");
		return 0;
	}

	reinit_completion(&jpeg_enc_dev->hw_complete);

	core_info->core_state = CAM_JPEG_ENC_CORE_RESETTING;

	cam_io_w_mb(0x00000000, mem_base + hw_info->int_mask);
	cam_io_w_mb(0xFFFFFFFF, mem_base + hw_info->int_clr);
	cam_io_w_mb(0xFFFFFFFF, mem_base + hw_info->int_mask);
	cam_io_w_mb(0x00032093, mem_base + hw_info->reset_cmd);

	rem_jiffies = wait_for_completion_timeout(&jpeg_enc_dev->hw_complete,
		CAM_JPEG_ENC_RESET_TIMEOUT);
	if (!rem_jiffies) {
		CAM_ERR(CAM_JPEG, "error Reset Timeout");
		core_info->core_state = CAM_JPEG_ENC_CORE_NOT_READY;
	}

	return 0;
}

int cam_jpeg_enc_start_hw(void *data,
	void *start_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_enc_dev = data;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_enc_device_hw_info *hw_info = NULL;
	void __iomem *mem_base;

	if (!jpeg_enc_dev) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}

	soc_info = &jpeg_enc_dev->soc_info;
	core_info = (struct cam_jpeg_enc_device_core_info *)
		jpeg_enc_dev->core_info;
	hw_info = core_info->jpeg_enc_hw_info;
	mem_base = soc_info->reg_map[0].mem_base;

	if (core_info->core_state != CAM_JPEG_ENC_CORE_READY) {
		CAM_ERR(CAM_JPEG, "Error not ready");
		return -EINVAL;
	}

	cam_io_w_mb(0x00000001, mem_base + 0x00000010);

	return 0;
}

int cam_jpeg_enc_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_enc_dev = device_priv;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	int rc;

	if (!device_priv) {
		CAM_ERR(CAM_JPEG, "Invalid arguments");
		return -EINVAL;
	}

	if (cmd_type >= CAM_JPEG_ENC_CMD_MAX) {
		CAM_ERR(CAM_JPEG, "Invalid command : %x", cmd_type);
		return -EINVAL;
	}

	core_info =
		(struct cam_jpeg_enc_device_core_info *)jpeg_enc_dev->
		core_info;

	switch (cmd_type) {
	case CAM_JPEG_ENC_CMD_SET_IRQ_CB:
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
