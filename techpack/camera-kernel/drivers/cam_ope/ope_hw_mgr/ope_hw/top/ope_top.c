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
#include <linux/completion.h>
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
#include "ope_top.h"
#include "cam_common_util.h"

static struct ope_top ope_top_info;

static int cam_ope_top_dump_debug_reg(struct ope_hw *ope_hw_info)
{
	uint32_t i, val[3];
	struct cam_ope_top_reg *top_reg;
	struct cam_ope_pp_reg *pp_reg;
	uint32_t pp_hw_status = 0;

	top_reg = ope_hw_info->top_reg;
	pp_reg = ope_hw_info->pp_reg;

	for (i = 0; i < top_reg->num_debug_registers; i = i+3) {
		val[0] = cam_io_r_mb(top_reg->base +
			top_reg->debug_regs[i].offset);
		val[1] = ((i+1) < top_reg->num_debug_registers) ?
			cam_io_r_mb(top_reg->base +
			top_reg->debug_regs[i+1].offset) : 0;
		val[2] = ((i+2) < top_reg->num_debug_registers) ?
			cam_io_r_mb(top_reg->base +
			top_reg->debug_regs[i+2].offset) : 0;

		CAM_INFO(CAM_OPE, "status[%d-%d] : 0x%x 0x%x 0x%x",
			i, i+2, val[0], val[1], val[2]);
	}

	CAM_INFO(CAM_OPE, "scrath reg: 0x%x, stripe_idx: %d",
		top_reg->offset + top_reg->scratch_reg,
		cam_io_r_mb(top_reg->base + top_reg->scratch_reg));

	for (i = 0; i < pp_reg->num_clients ; i++) {
		pp_hw_status = 0;
		pp_hw_status =
			cam_io_r_mb(pp_reg->base +
				pp_reg->pp_clients[i]
					.hw_status);

		if (pp_hw_status)
			CAM_ERR(CAM_OPE,
				"ope pp hw_status offset 0x%x val 0x%x",
				pp_reg->pp_clients[i].hw_status,
				pp_hw_status);
	}
	return 0;
}

static int cam_ope_top_reset(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	struct cam_ope_top_reg *top_reg;
	struct cam_ope_top_reg_val *top_reg_val;
	uint32_t irq_mask, irq_status;
	unsigned long flags;

	if (!ope_hw_info) {
		CAM_ERR(CAM_OPE, "Invalid ope_hw_info");
		return -EINVAL;
	}

	top_reg = ope_hw_info->top_reg;
	top_reg_val = ope_hw_info->top_reg_val;

	mutex_lock(&ope_top_info.ope_hw_mutex);
	reinit_completion(&ope_top_info.reset_complete);

	/* enable interrupt mask */
	cam_io_w_mb(top_reg_val->irq_mask,
		ope_hw_info->top_reg->base + top_reg->irq_mask);

	/* OPE SW RESET */
	cam_io_w_mb(top_reg_val->sw_reset_cmd,
		ope_hw_info->top_reg->base + top_reg->reset_cmd);

	rc = cam_common_wait_for_completion_timeout(
			&ope_top_info.reset_complete,
			msecs_to_jiffies(60));

	cam_io_w_mb(top_reg_val->debug_cfg_val,
		top_reg->base + top_reg->debug_cfg);

	if (!rc || rc < 0) {
		spin_lock_irqsave(&ope_top_info.hw_lock, flags);
		if (!completion_done(&ope_top_info.reset_complete)) {
			CAM_DBG(CAM_OPE,
				"IRQ delayed, checking the status registers");
			irq_mask = cam_io_r_mb(ope_hw_info->top_reg->base +
				top_reg->irq_mask);
			irq_status = cam_io_r_mb(ope_hw_info->top_reg->base +
				top_reg->irq_status);
			if (irq_status & top_reg_val->rst_done) {
				CAM_DBG(CAM_OPE, "ope reset done");
				cam_io_w_mb(irq_status,
					top_reg->base + top_reg->irq_clear);
				cam_io_w_mb(top_reg_val->irq_set_clear,
					top_reg->base + top_reg->irq_cmd);
			} else {
				CAM_ERR(CAM_OPE,
					"irq mask 0x%x irq status 0x%x",
					irq_mask, irq_status);
				cam_ope_top_dump_debug_reg(ope_hw_info);
				rc = -ETIMEDOUT;
			}
		} else {
			rc = 0;
		}
		spin_unlock_irqrestore(&ope_top_info.hw_lock, flags);
	} else {
		rc = 0;
	}

	/* enable interrupt mask */
	cam_io_w_mb(top_reg_val->irq_mask,
		ope_hw_info->top_reg->base + top_reg->irq_mask);

	mutex_unlock(&ope_top_info.ope_hw_mutex);
	return rc;
}

static int cam_ope_top_release(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;

	if (ctx_id < 0) {
		CAM_ERR(CAM_OPE, "Invalid data: %d", ctx_id);
		return -EINVAL;
	}

	ope_top_info.top_ctx[ctx_id].ope_acquire = NULL;

	return rc;
}

static int cam_ope_top_acquire(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;

	if (ctx_id < 0 || !data) {
		CAM_ERR(CAM_OPE, "Invalid data: %d %x", ctx_id, data);
		return -EINVAL;
	}

	ope_top_info.top_ctx[ctx_id].ope_acquire = data;

	return rc;
}

static int cam_ope_top_init(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	struct cam_ope_top_reg *top_reg;
	struct cam_ope_top_reg_val *top_reg_val;
	struct cam_ope_dev_init *dev_init = data;
	uint32_t irq_mask, irq_status;
	unsigned long flags;

	if (!ope_hw_info) {
		CAM_ERR(CAM_OPE, "Invalid ope_hw_info");
		return -EINVAL;
	}

	top_reg = ope_hw_info->top_reg;
	top_reg_val = ope_hw_info->top_reg_val;

	top_reg->base = dev_init->core_info->ope_hw_info->ope_top_base;

	mutex_init(&ope_top_info.ope_hw_mutex);
	/* OPE SW RESET */
	init_completion(&ope_top_info.reset_complete);

	/* enable interrupt mask */
	cam_io_w_mb(top_reg_val->irq_mask,
		ope_hw_info->top_reg->base + top_reg->irq_mask);
	cam_io_w_mb(top_reg_val->sw_reset_cmd,
		ope_hw_info->top_reg->base + top_reg->reset_cmd);

	rc = cam_common_wait_for_completion_timeout(
			&ope_top_info.reset_complete,
			msecs_to_jiffies(60));

	cam_io_w_mb(top_reg_val->debug_cfg_val,
		top_reg->base + top_reg->debug_cfg);

	if (!rc || rc < 0) {
		spin_lock_irqsave(&ope_top_info.hw_lock, flags);
		if (!completion_done(&ope_top_info.reset_complete)) {
			CAM_DBG(CAM_OPE,
				"IRQ delayed, checking the status registers");
			irq_mask = cam_io_r_mb(ope_hw_info->top_reg->base +
				top_reg->irq_mask);
			irq_status = cam_io_r_mb(ope_hw_info->top_reg->base +
				top_reg->irq_status);
			if (irq_status & top_reg_val->rst_done) {
				CAM_DBG(CAM_OPE, "ope reset done");
				cam_io_w_mb(irq_status,
					top_reg->base + top_reg->irq_clear);
				cam_io_w_mb(top_reg_val->irq_set_clear,
					top_reg->base + top_reg->irq_cmd);
			} else {
				CAM_ERR(CAM_OPE,
					"irq mask 0x%x irq status 0x%x",
					irq_mask, irq_status);
				cam_ope_top_dump_debug_reg(ope_hw_info);
				rc = -ETIMEDOUT;
			}
		} else {
			CAM_DBG(CAM_OPE, "reset done");
			rc = 0;
		}
		spin_unlock_irqrestore(&ope_top_info.hw_lock, flags);
	} else {
		rc = 0;
	}
	/* enable interrupt mask */
	cam_io_w_mb(top_reg_val->irq_mask,
		ope_hw_info->top_reg->base + top_reg->irq_mask);

	return rc;
}

static int cam_ope_top_probe(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;

	if (!ope_hw_info) {
		CAM_ERR(CAM_OPE, "Invalid ope_hw_info");
		return -EINVAL;
	}

	ope_top_info.ope_hw_info = ope_hw_info;
	spin_lock_init(&ope_top_info.hw_lock);

	return rc;
}

static int cam_ope_top_isr(struct ope_hw *ope_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	uint32_t irq_status;
	uint32_t violation_status;
	uint32_t pp_hw_status = 0;
	struct cam_ope_top_reg *top_reg;
	struct cam_ope_top_reg_val *top_reg_val;
	struct cam_ope_pp_reg *pp_reg;
	struct cam_ope_irq_data *irq_data = data;
	int i;

	if (!ope_hw_info) {
		CAM_ERR(CAM_OPE, "Invalid ope_hw_info");
		return -EINVAL;
	}

	top_reg = ope_hw_info->top_reg;
	top_reg_val = ope_hw_info->top_reg_val;
	pp_reg = ope_hw_info->pp_reg;

	spin_lock(&ope_top_info.hw_lock);
	/* Read and Clear Top Interrupt status */
	irq_status = cam_io_r_mb(top_reg->base + top_reg->irq_status);
	cam_io_w_mb(irq_status,
		top_reg->base + top_reg->irq_clear);

	cam_io_w_mb(top_reg_val->irq_set_clear,
		top_reg->base + top_reg->irq_cmd);

	if (irq_status & top_reg_val->rst_done) {
		CAM_DBG(CAM_OPE, "ope reset done");
		complete(&ope_top_info.reset_complete);
	}

	if (irq_status & top_reg_val->ope_violation) {
		violation_status = cam_io_r_mb(top_reg->base +
			top_reg->violation_status);
		irq_data->error = 1;
		CAM_ERR(CAM_OPE, "ope violation: %x", violation_status);

		for (i = 0; i < pp_reg->num_clients ; i++) {
			pp_hw_status = 0;
			pp_hw_status =
				cam_io_r_mb(pp_reg->base +
					pp_reg->pp_clients[i]
						.hw_status);

			if (pp_hw_status)
				CAM_ERR(CAM_OPE,
					"ope pp hw_status offset 0x%x val 0x%x",
					pp_reg->pp_clients[i].hw_status,
					pp_hw_status);
		}
	}
	spin_unlock(&ope_top_info.hw_lock);

	return rc;
}

int cam_ope_top_process(struct ope_hw *ope_hw_info,
	int32_t ctx_id, uint32_t cmd_id, void *data)
{
	int rc = 0;

	switch (cmd_id) {
	case OPE_HW_PROBE:
		CAM_DBG(CAM_OPE, "OPE_HW_PROBE: E");
		rc = cam_ope_top_probe(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_PROBE: X");
		break;
	case OPE_HW_INIT:
		CAM_DBG(CAM_OPE, "OPE_HW_INIT: E");
		rc = cam_ope_top_init(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_INIT: X");
		break;
	case OPE_HW_DEINIT:
		break;
	case OPE_HW_ACQUIRE:
		CAM_DBG(CAM_OPE, "OPE_HW_ACQUIRE: E");
		rc = cam_ope_top_acquire(ope_hw_info, ctx_id, data);
		CAM_DBG(CAM_OPE, "OPE_HW_ACQUIRE: X");
		break;
	case OPE_HW_PREPARE:
		break;
	case OPE_HW_RELEASE:
		rc = cam_ope_top_release(ope_hw_info, ctx_id, data);
		break;
	case OPE_HW_START:
		break;
	case OPE_HW_STOP:
		break;
	case OPE_HW_FLUSH:
		break;
	case OPE_HW_ISR:
		rc = cam_ope_top_isr(ope_hw_info, 0, data);
		break;
	case OPE_HW_RESET:
		rc = cam_ope_top_reset(ope_hw_info, 0, 0);
		break;
	case OPE_HW_DUMP_DEBUG:
		rc = cam_ope_top_dump_debug_reg(ope_hw_info);
		break;
	default:
		break;
	}

	return rc;
}
