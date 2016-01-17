/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/sync.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>

#include "sde_rotator_r1_hwio.h"
#include "sde_rotator_core.h"
#include "sde_rotator_util.h"
#include "sde_rotator_r1_internal.h"
#include "sde_rotator_r1.h"
#include "sde_rotator_r1_debug.h"

struct sde_mdp_hw_resource {
	struct sde_rot_hw_resource hw;
	struct sde_mdp_ctl *ctl;
	struct sde_mdp_mixer *mixer;
	struct sde_mdp_pipe *pipe;
	struct sde_mdp_writeback *wb;
};

struct sde_rotator_r1_data {
	struct sde_rot_mgr *mgr;
	int wb_id;
	int ctl_id;
	int irq_num;
	struct sde_mdp_hw_resource *mdp_hw;
};

static struct sde_mdp_hw_resource *sde_rotator_hw_alloc(
	struct sde_rot_mgr *mgr, u32 ctl_id, u32 wb_id, int irq_num)
{
	struct sde_mdp_hw_resource *mdp_hw;
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	int pipe_ndx, offset = ctl_id;
	int ret;

	mdp_hw = devm_kzalloc(&mgr->pdev->dev,
			sizeof(struct sde_mdp_hw_resource), GFP_KERNEL);
	if (!mdp_hw)
		return ERR_PTR(-ENOMEM);

	mdp_hw->ctl = sde_mdp_ctl_alloc(mdata, offset);
	if (IS_ERR_OR_NULL(mdp_hw->ctl)) {
		SDEROT_ERR("unable to allocate ctl\n");
		ret = -ENODEV;
		goto error;
	}
	mdp_hw->ctl->irq_num = irq_num;

	mdp_hw->wb = sde_mdp_wb_assign(wb_id, mdp_hw->ctl->num);
	if (IS_ERR_OR_NULL(mdp_hw->wb)) {
		SDEROT_ERR("unable to allocate wb\n");
		ret = -ENODEV;
		goto error;
	}

	mdp_hw->ctl->wb = mdp_hw->wb;
	mdp_hw->mixer = sde_mdp_mixer_assign(mdp_hw->wb->num, true);
	if (IS_ERR_OR_NULL(mdp_hw->mixer)) {
		SDEROT_ERR("unable to allocate wb mixer\n");
		ret = -ENODEV;
		goto error;
	}

	mdp_hw->ctl->mixer_left = mdp_hw->mixer;
	mdp_hw->mixer->ctl = mdp_hw->ctl;

	mdp_hw->mixer->rotator_mode = true;

	switch (mdp_hw->mixer->num) {
	case SDE_MDP_WB_LAYERMIXER0:
		mdp_hw->ctl->opmode = SDE_MDP_CTL_OP_ROT0_MODE;
		break;
	case SDE_MDP_WB_LAYERMIXER1:
		mdp_hw->ctl->opmode =  SDE_MDP_CTL_OP_ROT1_MODE;
		break;
	default:
		SDEROT_ERR("invalid layer mixer=%d\n", mdp_hw->mixer->num);
		ret = -EINVAL;
		goto error;
	}

	mdp_hw->ctl->ops.start_fnc = sde_mdp_writeback_start;
	mdp_hw->ctl->wb_type = SDE_MDP_WB_CTL_TYPE_BLOCK;

	if (mdp_hw->ctl->ops.start_fnc)
		ret = mdp_hw->ctl->ops.start_fnc(mdp_hw->ctl);

	if (ret)
		goto error;

	/* override from dt */
	pipe_ndx = wb_id;
	mdp_hw->pipe = sde_mdp_pipe_assign(mdata, mdp_hw->mixer, pipe_ndx);
	if (IS_ERR_OR_NULL(mdp_hw->pipe)) {
		SDEROT_ERR("dma pipe allocation failed\n");
		ret = -ENODEV;
		goto error;
	}

	mdp_hw->pipe->mixer_left = mdp_hw->mixer;
	mdp_hw->hw.wb_id = mdp_hw->wb->num;
	mdp_hw->hw.pending_count = 0;
	atomic_set(&mdp_hw->hw.num_active, 0);
	mdp_hw->hw.max_active = 1;
	init_waitqueue_head(&mdp_hw->hw.wait_queue);

	return mdp_hw;
error:
	if (!IS_ERR_OR_NULL(mdp_hw->pipe))
		sde_mdp_pipe_destroy(mdp_hw->pipe);
	if (!IS_ERR_OR_NULL(mdp_hw->ctl)) {
		if (mdp_hw->ctl->ops.stop_fnc)
			mdp_hw->ctl->ops.stop_fnc(mdp_hw->ctl, 0);
		sde_mdp_ctl_free(mdp_hw->ctl);
	}
	devm_kfree(&mgr->pdev->dev, mdp_hw);

	return ERR_PTR(ret);
}

static void sde_rotator_hw_free(struct sde_rot_mgr *mgr,
	struct sde_mdp_hw_resource *mdp_hw)
{
	struct sde_mdp_mixer *mixer;
	struct sde_mdp_ctl *ctl;

	if (!mgr || !mdp_hw)
		return;

	mixer = mdp_hw->pipe->mixer_left;

	sde_mdp_pipe_destroy(mdp_hw->pipe);

	ctl = sde_mdp_ctl_mixer_switch(mixer->ctl,
		SDE_MDP_WB_CTL_TYPE_BLOCK);
	if (ctl) {
		if (ctl->ops.stop_fnc)
			ctl->ops.stop_fnc(ctl, 0);
		sde_mdp_ctl_free(ctl);
	}

	devm_kfree(&mgr->pdev->dev, mdp_hw);
}

static struct sde_rot_hw_resource *sde_rotator_hw_alloc_ext(
	struct sde_rot_mgr *mgr, u32 pipe_id, u32 wb_id)
{
	struct sde_mdp_hw_resource *mdp_hw;
	struct sde_rotator_r1_data *hw_data;

	if (!mgr || !mgr->hw_data)
		return NULL;

	hw_data = mgr->hw_data;
	mdp_hw = hw_data->mdp_hw;

	return &mdp_hw->hw;
}

static void sde_rotator_hw_free_ext(struct sde_rot_mgr *mgr,
	struct sde_rot_hw_resource *hw)
{
	/* currently nothing specific for this device */
}

static void sde_rotator_translate_rect(struct sde_rect *dst,
	struct sde_rect *src)
{
	dst->x = src->x;
	dst->y = src->y;
	dst->w = src->w;
	dst->h = src->h;
}

static u32 sde_rotator_translate_flags(u32 input)
{
	u32 output = 0;

	if (input & SDE_ROTATION_NOP)
		output |= SDE_ROT_NOP;
	if (input & SDE_ROTATION_FLIP_LR)
		output |= SDE_FLIP_LR;
	if (input & SDE_ROTATION_FLIP_UD)
		output |= SDE_FLIP_UD;
	if (input & SDE_ROTATION_90)
		output |= SDE_ROT_90;
	if (input & SDE_ROTATION_DEINTERLACE)
		output |= SDE_DEINTERLACE;
	if (input & SDE_ROTATION_SECURE)
		output |= SDE_SECURE_OVERLAY_SESSION;
	return output;
}

static int sde_rotator_config_hw(struct sde_rot_hw_resource *hw,
	struct sde_rot_entry *entry)
{
	struct sde_mdp_hw_resource *mdp_hw;
	struct sde_mdp_pipe *pipe;
	struct sde_rotation_item *item;
	int ret;

	if (!hw || !entry) {
		SDEROT_ERR("null hw resource/entry");
		return -EINVAL;
	}

	mdp_hw = container_of(hw, struct sde_mdp_hw_resource, hw);

	pipe = mdp_hw->pipe;
	item = &entry->item;

	pipe->flags = sde_rotator_translate_flags(item->flags);
	pipe->src_fmt = sde_get_format_params(item->input.format);
	pipe->img_width = item->input.width;
	pipe->img_height = item->input.height;
	sde_rotator_translate_rect(&pipe->src, &item->src_rect);
	sde_rotator_translate_rect(&pipe->dst, &item->src_rect);

	pipe->params_changed++;

	ret = sde_mdp_pipe_queue_data(pipe, &entry->src_buf);
	SDEROT_DBG("Config pipe. src{%u,%u,%u,%u}f=%u\n"
		"dst{%u,%u,%u,%u}f=%u session_id=%u\n",
		item->src_rect.x, item->src_rect.y,
		item->src_rect.w, item->src_rect.h, item->input.format,
		item->dst_rect.x, item->dst_rect.y,
		item->dst_rect.w, item->dst_rect.h, item->output.format,
		item->session_id);

	return ret;
}

static int sde_rotator_kickoff_entry(struct sde_rot_hw_resource *hw,
	struct sde_rot_entry *entry)
{
	struct sde_mdp_hw_resource *mdp_hw;
	int ret;
	struct sde_mdp_writeback_arg wb_args;

	if (!hw || !entry) {
		SDEROT_ERR("null hw resource/entry");
		return -EINVAL;
	}

	wb_args.data = &entry->dst_buf;
	wb_args.priv_data = entry;

	mdp_hw = container_of(hw, struct sde_mdp_hw_resource, hw);

	ret = sde_mdp_writeback_display_commit(mdp_hw->ctl, &wb_args);
	return ret;
}

static int sde_rotator_wait_for_entry(struct sde_rot_hw_resource *hw,
	struct sde_rot_entry *entry)
{
	struct sde_mdp_hw_resource *mdp_hw;
	int ret;
	struct sde_mdp_ctl *ctl;

	if (!hw || !entry) {
		SDEROT_ERR("null hw resource/entry");
		return -EINVAL;
	}

	mdp_hw = container_of(hw, struct sde_mdp_hw_resource, hw);

	ctl = mdp_hw->ctl;

	ret = sde_mdp_display_wait4comp(ctl);

	return ret;
}

static int sde_rotator_hw_validate_entry(struct sde_rot_mgr *mgr,
	struct sde_rot_entry *entry)
{
	int ret = 0;
	u16 src_w, src_h, dst_w, dst_h, bit;
	struct sde_rotation_item *item = &entry->item;
	struct sde_mdp_format_params *fmt;

	src_w = item->src_rect.w;
	src_h = item->src_rect.h;

	if (item->flags & SDE_ROTATION_90) {
		dst_w = item->dst_rect.h;
		dst_h = item->dst_rect.w;
	} else {
		dst_w = item->dst_rect.w;
		dst_h = item->dst_rect.h;
	}

	entry->dnsc_factor_w = 0;
	entry->dnsc_factor_h = 0;

	if ((src_w != dst_w) || (src_h != dst_h)) {
		if ((src_w % dst_w) || (src_h % dst_h)) {
			SDEROT_DBG("non integral scale not support\n");
			ret = -EINVAL;
			goto dnsc_err;
		}
		entry->dnsc_factor_w = src_w / dst_w;
		bit = fls(entry->dnsc_factor_w);
		if ((entry->dnsc_factor_w & ~BIT(bit - 1)) || (bit > 5)) {
			SDEROT_DBG("non power-of-2 scale not support\n");
			ret = -EINVAL;
			goto dnsc_err;
		}
		entry->dnsc_factor_h = src_h / dst_h;
		bit = fls(entry->dnsc_factor_h);
		if ((entry->dnsc_factor_h & ~BIT(bit - 1)) || (bit > 5)) {
			SDEROT_DBG("non power-of-2 dscale not support\n");
			ret = -EINVAL;
			goto dnsc_err;
		}
	}

	fmt =  sde_get_format_params(item->output.format);
	if (sde_mdp_is_ubwc_format(fmt) &&
		(entry->dnsc_factor_h || entry->dnsc_factor_w)) {
		SDEROT_DBG("downscale with ubwc not support\n");
		ret = -EINVAL;
	}

dnsc_err:

	/* Downscaler does not support asymmetrical dnsc */
	if (entry->dnsc_factor_w != entry->dnsc_factor_h) {
		SDEROT_DBG("asymmetric downscale not support\n");
		ret = -EINVAL;
	}

	if (ret) {
		entry->dnsc_factor_w = 0;
		entry->dnsc_factor_h = 0;
	}
	return ret;
}

static ssize_t sde_rotator_hw_show_caps(struct sde_rot_mgr *mgr,
		struct device_attribute *attr, char *buf, ssize_t len)
{
	struct sde_rotator_r1_data *hw_data;
	int cnt = 0;

	if (!mgr || !buf)
		return 0;

	hw_data = mgr->hw_data;

#define SPRINT(fmt, ...) \
		(cnt += scnprintf(buf + cnt, len - cnt, fmt, ##__VA_ARGS__))

	SPRINT("wb_id=%d\n", hw_data->wb_id);
	SPRINT("ctl_id=%d\n", hw_data->ctl_id);
	return cnt;
}

static ssize_t sde_rotator_hw_show_state(struct sde_rot_mgr *mgr,
		struct device_attribute *attr, char *buf, ssize_t len)
{
	struct sde_rotator_r1_data *hw_data;
	int cnt = 0;

	if (!mgr || !buf)
		return 0;

	hw_data = mgr->hw_data;

#define SPRINT(fmt, ...) \
		(cnt += scnprintf(buf + cnt, len - cnt, fmt, ##__VA_ARGS__))

	if (hw_data && hw_data->mdp_hw) {
		struct sde_rot_hw_resource *hw = &hw_data->mdp_hw->hw;

		SPRINT("irq_num=%d\n", hw_data->irq_num);
		SPRINT("max_active=%d\n", hw->max_active);
		SPRINT("num_active=%d\n", atomic_read(&hw->num_active));
		SPRINT("pending_cnt=%u\n", hw->pending_count);
	}

	return cnt;
}

static int sde_rotator_hw_parse_dt(struct sde_rotator_r1_data *hw_data,
		struct platform_device *dev)
{
	int ret = 0;
	u32 data;

	if (!hw_data || !dev)
		return -EINVAL;

	ret = of_property_read_u32(dev->dev.of_node,
			"qcom,mdss-wb-id", &data);
	if (ret)
		hw_data->wb_id = -1;
	else
		hw_data->wb_id = (int) data;
	ret = of_property_read_u32(dev->dev.of_node,
			"qcom,mdss-ctl-id", &data);
	if (ret)
		hw_data->ctl_id = -1;
	else
		hw_data->ctl_id = (int) data;

	return ret;
}

static int sde_rotator_hw_rev_init(struct sde_rot_data_type *mdata)
{
	if (!mdata) {
		SDEROT_ERR("null rotator data\n");
		return -EINVAL;
	}

	clear_bit(SDE_QOS_PER_PIPE_IB, mdata->sde_qos_map);
	set_bit(SDE_QOS_OVERHEAD_FACTOR, mdata->sde_qos_map);
	clear_bit(SDE_QOS_CDP, mdata->sde_qos_map);
	set_bit(SDE_QOS_OTLIM, mdata->sde_qos_map);
	set_bit(SDE_QOS_PER_PIPE_LUT, mdata->sde_qos_map);
	clear_bit(SDE_QOS_SIMPLIFIED_PREFILL, mdata->sde_qos_map);
	set_bit(SDE_CAPS_R1_WB, mdata->sde_caps_map);

	return 0;
}

enum {
	SDE_ROTATOR_INTR_WB_0,
	SDE_ROTATOR_INTR_WB_1,
	SDE_ROTATOR_INTR_MAX,
};

struct intr_callback {
	void (*func)(void *);
	void *arg;
};

struct intr_callback sde_intr_cb[SDE_ROTATOR_INTR_MAX];

int sde_mdp_set_intr_callback(u32 intr_type, u32 intf_num,
			       void (*fnc_ptr)(void *), void *arg)
{
	if (intf_num >= SDE_ROTATOR_INTR_MAX) {
		SDEROT_WARN("invalid intr type=%u intf_num=%u\n",
				intr_type, intf_num);
		return -EINVAL;
	}

	sde_intr_cb[intf_num].func = fnc_ptr;
	sde_intr_cb[intf_num].arg = arg;

	return 0;
}

static irqreturn_t sde_irq_handler(int irq, void *ptr)
{
	struct sde_rot_data_type *mdata = ptr;
	irqreturn_t ret = IRQ_NONE;
	u32 isr;

	isr = readl_relaxed(mdata->mdp_base + SDE_MDP_REG_INTR_STATUS);

	SDEROT_DBG("intr_status = %8.8x\n", isr);

	if (isr & SDE_MDP_INTR_WB_0_DONE) {
		struct intr_callback *cb = &sde_intr_cb[SDE_ROTATOR_INTR_WB_0];

		if (cb->func) {
			writel_relaxed(SDE_MDP_INTR_WB_0_DONE,
				mdata->mdp_base + SDE_MDP_REG_INTR_CLEAR);
			cb->func(cb->arg);
			ret = IRQ_HANDLED;
		}
	}

	if (isr & SDE_MDP_INTR_WB_1_DONE) {
		struct intr_callback *cb = &sde_intr_cb[SDE_ROTATOR_INTR_WB_1];

		if (cb->func) {
			writel_relaxed(SDE_MDP_INTR_WB_1_DONE,
				mdata->mdp_base + SDE_MDP_REG_INTR_CLEAR);
			cb->func(cb->arg);
			ret = IRQ_HANDLED;
		}
	}

	return ret;
}

static void sde_rotator_hw_destroy(struct sde_rot_mgr *mgr)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct sde_rotator_r1_data *hw_data;

	if (!mgr || !mgr->pdev || !mgr->hw_data)
		return;

	hw_data = mgr->hw_data;
	if (hw_data->irq_num >= 0)
		devm_free_irq(&mgr->pdev->dev, hw_data->irq_num, mdata);
	sde_rotator_hw_free(mgr, hw_data->mdp_hw);
	devm_kfree(&mgr->pdev->dev, mgr->hw_data);
	mgr->hw_data = NULL;
}

int sde_rotator_r1_init(struct sde_rot_mgr *mgr)
{
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();
	struct sde_rotator_r1_data *hw_data;
	int ret;

	if (!mgr || !mgr->pdev) {
		SDEROT_ERR("null rotator manager/platform device");
		return -EINVAL;
	}

	hw_data = devm_kzalloc(&mgr->pdev->dev,
			sizeof(struct sde_rotator_r1_data), GFP_KERNEL);
	if (hw_data == NULL)
		return -ENOMEM;

	mgr->hw_data = hw_data;
	mgr->ops_config_hw = sde_rotator_config_hw;
	mgr->ops_kickoff_entry = sde_rotator_kickoff_entry;
	mgr->ops_wait_for_entry = sde_rotator_wait_for_entry;
	mgr->ops_hw_alloc = sde_rotator_hw_alloc_ext;
	mgr->ops_hw_free = sde_rotator_hw_free_ext;
	mgr->ops_hw_destroy = sde_rotator_hw_destroy;
	mgr->ops_hw_validate_entry = sde_rotator_hw_validate_entry;
	mgr->ops_hw_show_caps = sde_rotator_hw_show_caps;
	mgr->ops_hw_show_state = sde_rotator_hw_show_state;
	mgr->ops_hw_create_debugfs = sde_rotator_r1_create_debugfs;

	ret = sde_rotator_hw_parse_dt(mgr->hw_data, mgr->pdev);
	if (ret)
		goto error_parse_dt;

	hw_data->irq_num = platform_get_irq(mgr->pdev, 0);
	if (hw_data->irq_num < 0) {
		SDEROT_ERR("fail to get rotator irq\n");
	} else {
		ret = devm_request_threaded_irq(&mgr->pdev->dev,
				hw_data->irq_num,
				sde_irq_handler, NULL,
				0, "sde_rotator_r1", mdata);
		if (ret) {
			SDEROT_ERR("fail to request irq r:%d\n", ret);
			hw_data->irq_num = -1;
		} else {
			disable_irq(hw_data->irq_num);
		}
	}

	hw_data->mdp_hw = sde_rotator_hw_alloc(mgr, hw_data->ctl_id,
			hw_data->wb_id, hw_data->irq_num);
	if (IS_ERR_OR_NULL(hw_data->mdp_hw))
		goto error_hw_alloc;

	ret = sde_rotator_hw_rev_init(sde_rot_get_mdata());
	if (ret)
		goto error_hw_rev_init;

	hw_data->mgr = mgr;

	return 0;
error_hw_rev_init:
	if (hw_data->irq_num >= 0)
		devm_free_irq(&mgr->pdev->dev, hw_data->irq_num, mdata);
	sde_rotator_hw_free(mgr, hw_data->mdp_hw);
error_hw_alloc:
	devm_kfree(&mgr->pdev->dev, mgr->hw_data);
error_parse_dt:
	return ret;
}
