// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/completion.h>
#include <media/cam_cre.h>
#include "cam_io_util.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cre_core.h"
#include "cre_soc.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cre_hw.h"
#include "cre_dev_intf.h"
#include "cre_top.h"

static struct cre_top cre_top_info;

static int cam_cre_top_reset(struct cam_cre_hw *cre_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	struct cam_cre_top_reg *top_reg;
	struct cam_cre_top_reg_val *top_reg_val;
	uint32_t irq_mask, irq_status;
	unsigned long flags;

	if (!cre_hw_info) {
		CAM_ERR(CAM_CRE, "Invalid cre_hw_info");
		return -EINVAL;
	}

	top_reg = cre_hw_info->top_reg_offset;
	top_reg_val = cre_hw_info->top_reg_val;

	mutex_lock(&cre_top_info.cre_hw_mutex);
	reinit_completion(&cre_top_info.reset_complete);
	reinit_completion(&cre_top_info.idle_done);

	/* enable interrupt mask */
	cam_io_w_mb(top_reg_val->irq_mask,
		cre_hw_info->top_reg_offset->base + top_reg->irq_mask);

	/* CRE SW RESET */
	cam_io_w_mb(top_reg_val->sw_reset_cmd,
		cre_hw_info->top_reg_offset->base + top_reg->reset_cmd);

	rc = wait_for_completion_timeout(
			&cre_top_info.reset_complete,
			msecs_to_jiffies(60));

	if (!rc || rc < 0) {
		spin_lock_irqsave(&cre_top_info.hw_lock, flags);
		if (!completion_done(&cre_top_info.reset_complete)) {
			CAM_DBG(CAM_CRE,
				"IRQ delayed, checking the status registers");
			irq_mask = cam_io_r_mb(cre_hw_info->top_reg_offset->base +
				top_reg->irq_mask);
			irq_status = cam_io_r_mb(cre_hw_info->top_reg_offset->base +
				top_reg->irq_status);
			if (irq_status & top_reg_val->rst_done) {
				CAM_DBG(CAM_CRE, "cre reset done");
				cam_io_w_mb(irq_status,
					top_reg->base + top_reg->irq_clear);
				cam_io_w_mb(top_reg_val->irq_cmd_clear,
					top_reg->base + top_reg->irq_cmd);
			} else {
				CAM_ERR(CAM_CRE,
					"irq mask 0x%x irq status 0x%x",
					irq_mask, irq_status);
				rc = -ETIMEDOUT;
			}
		} else {
			rc = 0;
		}
		spin_unlock_irqrestore(&cre_top_info.hw_lock, flags);
	} else {
		rc = 0;
	}

	/* enable interrupt mask */
	cam_io_w_mb(top_reg_val->irq_mask,
		cre_hw_info->top_reg_offset->base + top_reg->irq_mask);

	mutex_unlock(&cre_top_info.cre_hw_mutex);
	return rc;
}

static int cam_cre_top_release(struct cam_cre_hw *cre_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;

	if (ctx_id < 0) {
		CAM_ERR(CAM_CRE, "Invalid data: %d", ctx_id);
		return -EINVAL;
	}

	cre_top_info.top_ctx[ctx_id].cre_acquire = NULL;

	return rc;
}

static int cam_cre_top_acquire(struct cam_cre_hw *cre_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	struct cam_cre_dev_acquire *cre_dev_acquire = data;

	if (ctx_id < 0 || !data) {
		CAM_ERR(CAM_CRE, "Invalid data: %d %x", ctx_id, data);
		return -EINVAL;
	}

	cre_top_info.top_ctx[ctx_id].cre_acquire = cre_dev_acquire->cre_acquire;
	cre_dev_acquire->cre_top = &cre_top_info;

	return rc;
}

static int cam_cre_top_init(struct cam_cre_hw *cre_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	struct cam_cre_top_reg *top_reg;
	struct cam_cre_top_reg_val *top_reg_val;
	struct cam_cre_dev_init *dev_init = data;
	uint32_t irq_mask, irq_status;
	unsigned long flags;

	if (!cre_hw_info) {
		CAM_ERR(CAM_CRE, "Invalid cre_hw_info");
		return -EINVAL;
	}

	top_reg = cre_hw_info->top_reg_offset;
	top_reg_val = cre_hw_info->top_reg_val;

	top_reg->base = dev_init->core_info->cre_hw_info->cre_top_base;

	mutex_init(&cre_top_info.cre_hw_mutex);
	/* CRE SW RESET */
	init_completion(&cre_top_info.reset_complete);
	init_completion(&cre_top_info.idle_done);
	init_completion(&cre_top_info.bufdone);

	/* enable interrupt mask */
	cam_io_w_mb(top_reg_val->irq_mask,
		cre_hw_info->top_reg_offset->base + top_reg->irq_mask);
	cam_io_w_mb(top_reg_val->sw_reset_cmd,
		cre_hw_info->top_reg_offset->base + top_reg->reset_cmd);

	rc = wait_for_completion_timeout(
			&cre_top_info.reset_complete,
			msecs_to_jiffies(60));

	if (!rc || rc < 0) {
		spin_lock_irqsave(&cre_top_info.hw_lock, flags);
		if (!completion_done(&cre_top_info.reset_complete)) {
			CAM_DBG(CAM_CRE,
				"IRQ delayed, checking the status registers");
			irq_mask = cam_io_r_mb(cre_hw_info->top_reg_offset->base +
				top_reg->irq_mask);
			irq_status = cam_io_r_mb(cre_hw_info->top_reg_offset->base +
				top_reg->irq_status);
			if (irq_status & top_reg_val->rst_done) {
				CAM_DBG(CAM_CRE, "cre reset done");
				cam_io_w_mb(irq_status,
					top_reg->base + top_reg->irq_clear);
				cam_io_w_mb(top_reg_val->irq_cmd_clear,
					top_reg->base + top_reg->irq_cmd);
			} else {
				CAM_ERR(CAM_CRE,
					"irq mask 0x%x irq status 0x%x",
					irq_mask, irq_status);
				rc = -ETIMEDOUT;
			}
		} else {
			CAM_DBG(CAM_CRE, "reset done");
			rc = 0;
		}
		spin_unlock_irqrestore(&cre_top_info.hw_lock, flags);
	} else {
		rc = 0;
	}
	/* enable interrupt mask */
	cam_io_w_mb(top_reg_val->irq_mask,
		cre_hw_info->top_reg_offset->base + top_reg->irq_mask);
	return rc;
}

static int cam_cre_top_probe(struct cam_cre_hw *cre_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;

	if (!cre_hw_info) {
		CAM_ERR(CAM_CRE, "Invalid cre_hw_info");
		return -EINVAL;
	}

	cre_top_info.cre_hw_info = cre_hw_info;
	spin_lock_init(&cre_top_info.hw_lock);

	return rc;
}

static int cam_cre_top_isr(struct cam_cre_hw *cre_hw_info,
	int32_t ctx_id, void *data)
{
	int rc = 0;
	uint32_t irq_status;
	struct cam_cre_top_reg *top_reg;
	struct cam_cre_top_reg_val *top_reg_val;
	struct cam_cre_irq_data *irq_data = data;

	if (!cre_hw_info) {
		CAM_ERR(CAM_CRE, "Invalid cre_hw_info");
		return -EINVAL;
	}

	top_reg = cre_hw_info->top_reg_offset;
	top_reg_val = cre_hw_info->top_reg_val;

	spin_lock(&cre_top_info.hw_lock);
	/* Read and Clear Top Interrupt status */
	irq_status = cam_io_r_mb(top_reg->base + top_reg->irq_status);
	cam_io_w_mb(irq_status,
		top_reg->base + top_reg->irq_clear);

	cam_io_w_mb(top_reg_val->irq_cmd_clear,
		top_reg->base + top_reg->irq_cmd);

	if (irq_status & top_reg_val->rst_done) {
		CAM_DBG(CAM_CRE, "cre reset done");
		complete(&cre_top_info.reset_complete);
	}

	if (irq_status & top_reg_val->idle) {
		CAM_DBG(CAM_CRE, "cre idle IRQ, can configure new settings");
		complete(&cre_top_info.idle_done);
	}

	if (irq_status & top_reg_val->we_done)
		CAM_DBG(CAM_CRE, "Received Write Engine IRQ");

	if (irq_status & top_reg_val->fe_done)
		CAM_DBG(CAM_CRE, "Received Fetch Engine IRQ");

	irq_data->top_irq_status = irq_status;
	spin_unlock(&cre_top_info.hw_lock);

	return rc;
}

int cam_cre_top_process(struct cam_cre_hw *cre_hw_info,
	int32_t ctx_id, uint32_t cmd_id, void *data)
{
	int rc = 0;

	switch (cmd_id) {
	case CRE_HW_PROBE:
		CAM_DBG(CAM_CRE, "CRE_HW_PROBE: E");
		rc = cam_cre_top_probe(cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_PROBE: X");
		break;
	case CRE_HW_INIT:
		CAM_DBG(CAM_CRE, "CRE_HW_INIT: E");
		rc = cam_cre_top_init(cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_INIT: X");
		break;
	case CRE_HW_DEINIT:
		break;
	case CRE_HW_ACQUIRE:
		CAM_DBG(CAM_CRE, "CRE_HW_ACQUIRE: E");
		rc = cam_cre_top_acquire(cre_hw_info, ctx_id, data);
		CAM_DBG(CAM_CRE, "CRE_HW_ACQUIRE: X");
		break;
	case CRE_HW_PREPARE:
		break;
	case CRE_HW_RELEASE:
		rc = cam_cre_top_release(cre_hw_info, ctx_id, data);
		break;
	case CRE_HW_REG_SET_UPDATE:
		break;
	case CRE_HW_START:
		break;
	case CRE_HW_STOP:
		break;
	case CRE_HW_FLUSH:
		break;
	case CRE_HW_ISR:
		rc = cam_cre_top_isr(cre_hw_info, ctx_id, data);
		break;
	case CRE_HW_RESET:
		rc = cam_cre_top_reset(cre_hw_info, ctx_id, 0);
		break;
	default:
		break;
	}

	return rc;
}
