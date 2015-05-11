/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/regulator/consumer.h>
#include <sync.h>

#include "mdss_rotator_internal.h"
#include "mdss_mdp.h"

/* waiting for hw time out, 3 vsync for 30fps*/
#define ROT_HW_ACQUIRE_TIMEOUT_IN_MS 100

/* acquire fence time out, following other driver fence time out practice */
#define ROT_FENCE_WAIT_TIMEOUT MSEC_PER_SEC
/*
 * Max rotator hw blocks possible. Used for upper array limits instead of
 * alloc and freeing small array
 */
#define ROT_MAX_HW_BLOCKS 2


#define CLASS_NAME "rotator"
#define DRIVER_NAME "mdss_rotator"

#define MDP_REG_BUS_VECTOR_ENTRY(ab_val, ib_val)	\
	{						\
		.src = MSM_BUS_MASTER_AMPSS_M0,		\
		.dst = MSM_BUS_SLAVE_DISPLAY_CFG,	\
		.ab = (ab_val),				\
		.ib = (ib_val),				\
	}

#define BUS_VOTE_19_MHZ 153600000

static struct msm_bus_vectors rot_reg_bus_vectors[] = {
	MDP_REG_BUS_VECTOR_ENTRY(0, 0),
	MDP_REG_BUS_VECTOR_ENTRY(0, BUS_VOTE_19_MHZ),
};
static struct msm_bus_paths rot_reg_bus_usecases[ARRAY_SIZE(
		rot_reg_bus_vectors)];
static struct msm_bus_scale_pdata rot_reg_bus_scale_table = {
	.usecase = rot_reg_bus_usecases,
	.num_usecases = ARRAY_SIZE(rot_reg_bus_usecases),
	.name = "mdss_rot_reg",
};

static struct mdss_rot_mgr *rot_mgr;
static void mdss_rotator_wq_handler(struct work_struct *work);

static int mdss_rotator_bus_scale_set_quota(struct mdss_rot_bus_data_type *bus,
		u64 quota)
{
	int new_uc_idx;

	if (bus->bus_hdl < 1) {
		pr_err("invalid bus handle %d\n", bus->bus_hdl);
		return -EINVAL;
	}

	if (bus->curr_quota_val == quota) {
		pr_debug("bw request already requested\n");
		return 0;
	}

	if (!quota) {
		new_uc_idx = 0;
	} else {
		struct msm_bus_vectors *vect = NULL;
		struct msm_bus_scale_pdata *bw_table =
			bus->bus_scale_pdata;
		u64 port_quota;
		u32 total_axi_port_cnt;
		int i;

		new_uc_idx = (bus->curr_bw_uc_idx %
			(bw_table->num_usecases - 1)) + 1;

		total_axi_port_cnt = bw_table->usecase[new_uc_idx].num_paths;
		if (total_axi_port_cnt == 0) {
			pr_err("Number of bw paths is 0\n");
			return -ENODEV;
		}
		port_quota = do_div(quota, total_axi_port_cnt);

		for (i = 0; i < total_axi_port_cnt; i++) {
			vect = &bw_table->usecase[new_uc_idx].vectors[i];
			vect->ab = port_quota;
			vect->ib = 0;
		}
	}
	bus->curr_bw_uc_idx = new_uc_idx;
	bus->curr_quota_val = quota;

	pr_debug("uc_idx=%d quota=%llu\n", new_uc_idx, quota);
	return msm_bus_scale_client_update_request(bus->bus_hdl,
		new_uc_idx);
}

static int mdss_rotator_enable_reg_bus(struct mdss_rot_mgr *mgr, u64 quota)
{
	int ret = 0, changed = 0;
	u32 usecase_ndx = 0;

	if (!mgr || !mgr->reg_bus.bus_hdl)
		return 0;

	if (quota)
		usecase_ndx = 1;

	if (usecase_ndx != mgr->reg_bus.curr_bw_uc_idx) {
		mgr->reg_bus.curr_bw_uc_idx = usecase_ndx;
		changed++;
	}

	pr_debug("%s, changed=%d register bus %s\n", __func__, changed,
		quota ? "Enable":"Disable");

	if (changed)
		ret = msm_bus_scale_client_update_request(mgr->reg_bus.bus_hdl,
			usecase_ndx);

	return ret;
}

/*
 * Clock rate of all open sessions working a particular hw block
 * are added together to get the required rate for that hw block.
 * The max of each hw block becomes the final clock rate voted for
 */
static unsigned long mdss_rotator_clk_rate_calc(
	struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private)
{
	struct mdss_rot_perf *perf;
	unsigned long clk_rate[ROT_MAX_HW_BLOCKS] = {0};
	unsigned long total_clk_rate = 0;
	int i, wb_idx;

	mutex_lock(&private->perf_lock);
	list_for_each_entry(perf, &private->perf_list, list) {
		bool rate_accounted_for = false;
		/*
		 * If there is one session that has two work items across
		 * different hw blocks rate is accounted for in both blocks.
		 */
		for (i = 0; i < mgr->queue_count; i++) {
			if (atomic_read(&perf->work_distribution[i])) {
				clk_rate[i] += perf->clk_rate;
				rate_accounted_for = true;
			}
		}

		/*
		 * Sessions that are open but not distributed on any hw block
		 * Still need to be accounted for. Rate is added to last known
		 * wb idx.
		 */
		wb_idx = perf->last_wb_idx;
		if ((!rate_accounted_for) && (wb_idx >= 0) &&
				(wb_idx < mgr->queue_count))
			clk_rate[wb_idx] += perf->clk_rate;
	}
	mutex_unlock(&private->perf_lock);

	for (i = 0; i < mgr->queue_count; i++)
		total_clk_rate = max(clk_rate[i], total_clk_rate);

	pr_debug("Total clk rate calc=%lu\n", total_clk_rate);
	return total_clk_rate;
}

static struct clk *mdss_rotator_get_clk(struct mdss_rot_mgr *mgr, u32 clk_idx)
{
	if (clk_idx >= MDSS_CLK_ROTATOR_END_IDX) {
		pr_err("Invalid clk index:%u", clk_idx);
		return NULL;
	}

	return mgr->rot_clk[clk_idx];
}

static void mdss_rotator_set_clk_rate(struct mdss_rot_mgr *mgr,
		unsigned long rate, u32 clk_idx)
{
	unsigned long clk_rate;
	struct clk *clk = mdss_rotator_get_clk(mgr, clk_idx);
	int ret;

	if (clk) {
		mutex_lock(&mgr->clk_lock);
		clk_rate = clk_round_rate(clk, rate);
		if (IS_ERR_VALUE(clk_rate)) {
			pr_err("unable to round rate err=%ld\n", clk_rate);
		} else if (clk_rate != clk_get_rate(clk)) {
			ret = clk_set_rate(clk, clk_rate);
			if (IS_ERR_VALUE(ret))
				pr_err("clk_set_rate failed, err:%d\n", ret);
			else
				pr_debug("rotator clk rate=%lu\n", clk_rate);
		}
		mutex_unlock(&mgr->clk_lock);
	} else {
		pr_err("rotator clk not setup properly\n");
	}
}

static void mdss_rotator_footswitch_ctrl(struct mdss_rot_mgr *mgr, bool on)
{
	int ret;

	if (mgr->regulator_enable == on) {
		pr_err("Regulators already in selected mode on=%d\n", on);
		return;
	}

	pr_debug("%s: rotator regulators", on ? "Enable" : "Disable");
	ret = msm_dss_enable_vreg(mgr->module_power.vreg_config,
		mgr->module_power.num_vreg, on);
	if (ret) {
		pr_warn("Rotator regulator failed to %s\n",
			on ? "enable" : "disable");
		return;
	}

	mgr->regulator_enable = on;
}

static int mdss_rotator_clk_ctrl(struct mdss_rot_mgr *mgr, int enable)
{
	struct clk *clk;
	int ret = 0;
	int i;

	/* Ref counting done by clock driver */
	for (i = 0; i < MDSS_CLK_ROTATOR_END_IDX; i++) {
		clk = mgr->rot_clk[i];
		if (enable) {
			ret = clk_enable(clk);
			if (ret) {
				pr_err("enable failed clk_idx %d\n", i);
				goto error;
			}
		} else {
			clk_disable(clk);
		}
	}

	return ret;
error:
	for (i--; i >= 0; i--)
		clk_disable(mgr->rot_clk[i]);
	return ret;
}

static int __mdss_rotator_clk_prepare(struct mdss_rot_mgr *mgr, int prepare)
{
	struct clk *clk;
	int ret = 0;
	int i;

	for (i = 0; i < MDSS_CLK_ROTATOR_END_IDX; i++) {
		clk = mgr->rot_clk[i];
		if (prepare) {
			ret = clk_prepare(clk);
			if (ret) {
				pr_err("prepare_enable failed clk_idx %d\n", i);
				goto error;
			}
		} else {
			clk_unprepare(clk);
		}
	}

	return ret;
error:
	for (i--; i >= 0; i--)
		clk_unprepare(mgr->rot_clk[i]);
	return ret;
}

int mdss_rotator_resource_ctrl(struct mdss_rot_mgr *mgr, int enable)
{
	int changed = 0;
	int ret = 0;

	mutex_lock(&mgr->clk_lock);
	if (enable) {
		if (mgr->res_ref_cnt == 0)
			changed++;
		mgr->res_ref_cnt++;
	} else {
		if (mgr->res_ref_cnt) {
			mgr->res_ref_cnt--;
			if (mgr->res_ref_cnt == 0)
				changed++;
		} else {
			pr_err("Rot resource already off\n");
		}
	}

	pr_debug("%s: res_cnt=%d changed=%d enable=%d\n",
		__func__, mgr->res_ref_cnt, changed, enable);

	if (changed) {
		if (enable) {
			mdss_rotator_footswitch_ctrl(mgr, true);
			ret = __mdss_rotator_clk_prepare(mgr, enable);
		} else {
			__mdss_rotator_clk_prepare(mgr, enable);
			mdss_rotator_footswitch_ctrl(mgr, false);
		}
	}
	mutex_unlock(&mgr->clk_lock);
	return ret;
}

static bool mdss_rotator_is_work_pending(struct mdss_rot_mgr *mgr,
	struct mdss_rot_perf *perf)
{
	int i;

	for (i = 0; i < mgr->queue_count; i++) {
		atomic_t *wrk_dis = &perf->work_distribution[i];
		if (atomic_read(wrk_dis)) {
			pr_debug("Work is still scheduled to complete\n");
			return true;
		}
	}
	return false;
}

static int mdss_rotator_create_fence(struct mdss_rot_entry *entry)
{
	int ret, fd;
	u32 val;
	struct sync_pt *sync_pt;
	struct sync_fence *fence;
	struct mdss_rot_timeline *rot_timeline;

	if (!entry->queue)
		return -EINVAL;

	rot_timeline = &entry->queue->timeline;

	mutex_lock(&rot_timeline->lock);
	val = rot_timeline->next_value + 1;

	sync_pt = sw_sync_pt_create(rot_timeline->timeline, val);
	if (sync_pt == NULL) {
		pr_err("cannot create sync point\n");
		goto sync_pt_create_err;
	}

	/* create fence */
	fence = sync_fence_create(rot_timeline->fence_name, sync_pt);
	if (fence == NULL) {
		pr_err("%s: cannot create fence\n", rot_timeline->fence_name);
		sync_pt_free(sync_pt);
		ret = -ENOMEM;
		goto sync_pt_create_err;
	}

	fd = get_unused_fd_flags(0);
	if (fd < 0) {
		pr_err("get_unused_fd_flags failed error:0x%x\n", fd);
		ret = fd;
		goto get_fd_err;
	}

	sync_fence_install(fence, fd);
	rot_timeline->next_value++;
	mutex_unlock(&rot_timeline->lock);

	entry->output_fence_fd = fd;
	entry->output_fence = fence;

	return 0;

get_fd_err:
	sync_fence_put(fence);
sync_pt_create_err:
	mutex_unlock(&rot_timeline->lock);
	return ret;
}

static void mdss_rotator_clear_fence(struct mdss_rot_entry *entry)
{
	struct mdss_rot_timeline *rot_timeline;

	if (entry->input_fence) {
		sync_fence_put(entry->input_fence);
		entry->input_fence = NULL;
	}

	rot_timeline = &entry->queue->timeline;

	/* fence failed to copy to user space */
	if (entry->output_fence) {
		sync_fence_put(entry->output_fence);
		entry->output_fence = NULL;
		put_unused_fd(entry->output_fence_fd);

		mutex_lock(&rot_timeline->lock);
		rot_timeline->next_value--;
		mutex_unlock(&rot_timeline->lock);
	}
}

static int mdss_rotator_signal_output(struct mdss_rot_entry *entry)
{
	struct mdss_rot_timeline *rot_timeline;

	if (!entry->queue)
		return -EINVAL;

	rot_timeline = &entry->queue->timeline;

	if (entry->output_signaled) {
		pr_debug("output already signaled\n");
		return 0;
	}

	mutex_lock(&rot_timeline->lock);
	sw_sync_timeline_inc(rot_timeline->timeline, 1);
	mutex_unlock(&rot_timeline->lock);

	entry->output_signaled = true;

	return 0;
}

static int mdss_rotator_wait_for_input(struct mdss_rot_entry *entry)
{
	int ret;

	if (!entry->input_fence) {
		pr_debug("invalid input fence, no wait\n");
		return 0;
	}

	ret = sync_fence_wait(entry->input_fence, ROT_FENCE_WAIT_TIMEOUT);
	sync_fence_put(entry->input_fence);
	entry->input_fence = NULL;
	return ret;
}

static int mdss_rotator_import_buffer(struct mdp_layer_buffer *buffer,
	struct mdss_mdp_data *data, u32 flags, struct device *dev, bool input)
{
	int i, ret = 0;
	struct msmfb_data planes[MAX_PLANES];
	int dir = DMA_TO_DEVICE;

	if (!input)
		dir = DMA_FROM_DEVICE;

	memset(planes, 0, sizeof(planes));

	for (i = 0; i < buffer->plane_count; i++) {
		planes[i].memory_id = buffer->planes[i].fd;
		planes[i].offset = buffer->planes[i].offset;
	}

	ret =  mdss_mdp_data_get(data, planes, buffer->plane_count,
			flags, dev, true, dir);

	return ret;
}

static int mdss_rotator_map_and_check_data(struct mdss_rot_entry *entry)
{
	int ret;
	struct mdp_layer_buffer *input;
	struct mdp_layer_buffer *output;
	struct mdss_mdp_format_params *fmt;
	struct mdss_mdp_plane_sizes ps;
	bool rotation;

	input = &entry->item.input;
	output = &entry->item.output;

	rotation = (entry->item.flags &  MDP_ROTATION_90) ? true : false;

	ret = mdss_iommu_ctrl(1);
	if (IS_ERR_VALUE(ret))
		return ret;

	/* if error during map, the caller will release the data */
	ret = mdss_mdp_data_map(&entry->src_buf, true, DMA_TO_DEVICE);
	if (ret) {
		pr_err("source buffer mapping failed ret:%d\n", ret);
		goto end;
	}

	ret = mdss_mdp_data_map(&entry->dst_buf, true, DMA_FROM_DEVICE);
	if (ret) {
		pr_err("destination buffer mapping failed ret:%d\n", ret);
		goto end;
	}

	fmt = mdss_mdp_get_format_params(input->format);
	if (!fmt) {
		pr_err("invalid input format:%d\n", input->format);
		ret = -EINVAL;
		goto end;
	}

	ret = mdss_mdp_get_plane_sizes(
			fmt, input->width, input->height, &ps, 0, rotation);
	if (ret) {
		pr_err("fail to get input plane size ret=%d\n", ret);
		goto end;
	}

	ret = mdss_mdp_data_check(&entry->src_buf, &ps, fmt);
	if (ret) {
		pr_err("fail to check input data ret=%d\n", ret);
		goto end;
	}

	fmt = mdss_mdp_get_format_params(output->format);
	if (!fmt) {
		pr_err("invalid output format:%d\n", output->format);
		ret = -EINVAL;
		goto end;
	}

	ret = mdss_mdp_get_plane_sizes(
			fmt, output->width, output->height, &ps, 0, rotation);
	if (ret) {
		pr_err("fail to get output plane size ret=%d\n", ret);
		goto end;
	}

	ret = mdss_mdp_data_check(&entry->dst_buf, &ps, fmt);
	if (ret) {
		pr_err("fail to check output data ret=%d\n", ret);
		goto end;
	}

end:
	mdss_iommu_ctrl(0);

	return ret;
}

static struct mdss_rot_perf *mdss_rotator_find_session(
	struct mdss_rot_file_private *private,
	u32 session_id)
{
	struct mdss_rot_perf *perf, *perf_next;
	bool found = false;

	mutex_lock(&private->perf_lock);

	list_for_each_entry_safe(perf, perf_next, &private->perf_list, list) {
		if (perf->config.session_id == session_id) {
			found = true;
			break;
		}
	}

	mutex_unlock(&private->perf_lock);
	if (!found)
		perf = NULL;
	return perf;
}

static void mdss_rotator_release_data(struct mdss_rot_entry *entry)
{
	mdss_mdp_data_free(&entry->src_buf, true, DMA_TO_DEVICE);
	mdss_mdp_data_free(&entry->dst_buf, true, DMA_FROM_DEVICE);
}

static int mdss_rotator_import_data(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry *entry)
{
	int ret;
	struct mdp_layer_buffer *input;
	struct mdp_layer_buffer *output;
	u32 flag = 0;

	input = &entry->item.input;
	output = &entry->item.output;

	if (entry->item.flags & MDP_ROTATION_SECURE)
		flag = MDP_SECURE_OVERLAY_SESSION;

	ret = mdss_rotator_import_buffer(input, &entry->src_buf, flag,
				&mgr->pdev->dev, true);
	if (ret) {
		pr_err("fail to import input buffer\n");
		return ret;
	}

	/*
	 * driver assumes ouput buffer is ready to be written
	 * immediately
	 */
	ret = mdss_rotator_import_buffer(output, &entry->dst_buf, flag,
				&mgr->pdev->dev, false);
	if (ret) {
		pr_err("fail to import output buffer\n");
		return ret;
	}

	return ret;
}

static struct mdss_rot_hw_resource *mdss_rotator_hw_alloc(
	struct mdss_rot_mgr *mgr, u32 pipe_id, u32 wb_id)
{
	struct mdss_rot_hw_resource *hw;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 pipe_ndx, offset = mdss_mdp_get_wb_ctl_support(mdata, true);
	int ret;

	hw = devm_kzalloc(&mgr->pdev->dev, sizeof(struct mdss_rot_hw_resource),
		GFP_KERNEL);
	if (!hw)
		return ERR_PTR(-ENOMEM);

	hw->ctl = mdss_mdp_ctl_alloc(mdata, offset);
	if (IS_ERR_OR_NULL(hw->ctl)) {
		pr_err("unable to allocate ctl\n");
		ret = -ENODEV;
		goto error;
	}

	if (wb_id == MDSS_ROTATION_HW_ANY)
		hw->wb = mdss_mdp_wb_alloc(MDSS_MDP_WB_ROTATOR, hw->ctl->num);
	else
		hw->wb = mdss_mdp_wb_assign(wb_id, hw->ctl->num);

	if (IS_ERR_OR_NULL(hw->wb)) {
		pr_err("unable to allocate wb\n");
		ret = -ENODEV;
		goto error;
	}
	hw->ctl->wb = hw->wb;
	hw->mixer = mdss_mdp_mixer_assign(hw->wb->num, true);

	if (IS_ERR_OR_NULL(hw->mixer)) {
		pr_err("unable to allocate wb mixer\n");
		ret = -ENODEV;
		goto error;
	}
	hw->ctl->mixer_left = hw->mixer;
	hw->mixer->ctl = hw->ctl;

	hw->mixer->rotator_mode = true;

	switch (hw->mixer->num) {
	case MDSS_MDP_WB_LAYERMIXER0:
		hw->ctl->opmode = MDSS_MDP_CTL_OP_ROT0_MODE;
		break;
	case MDSS_MDP_WB_LAYERMIXER1:
		hw->ctl->opmode =  MDSS_MDP_CTL_OP_ROT1_MODE;
		break;
	default:
		pr_err("invalid layer mixer=%d\n", hw->mixer->num);
		ret = -EINVAL;
		goto error;
	}

	hw->ctl->ops.start_fnc = mdss_mdp_writeback_start;
	hw->ctl->power_state = MDSS_PANEL_POWER_ON;
	hw->ctl->wb_type = MDSS_MDP_WB_CTL_TYPE_BLOCK;


	if (hw->ctl->ops.start_fnc)
		ret = hw->ctl->ops.start_fnc(hw->ctl);

	if (ret)
		goto error;

	if (pipe_id >= mdata->ndma_pipes)
		goto error;

	pipe_ndx = mdata->dma_pipes[pipe_id].ndx;
	hw->pipe = mdss_mdp_pipe_assign(mdata, hw->mixer, pipe_ndx);
	if (IS_ERR_OR_NULL(hw->pipe)) {
		pr_err("dma pipe allocation failed\n");
		ret = -ENODEV;
		goto error;
	}

	hw->pipe->mixer_left = hw->mixer;
	hw->pipe_id = hw->wb->num;
	hw->wb_id = hw->wb->num;

	return hw;
error:
	if (!IS_ERR_OR_NULL(hw->pipe))
		mdss_mdp_pipe_destroy(hw->pipe);
	if (!IS_ERR_OR_NULL(hw->ctl)) {
		if (hw->ctl->ops.stop_fnc)
			hw->ctl->ops.stop_fnc(hw->ctl, MDSS_PANEL_POWER_OFF);
		mdss_mdp_ctl_free(hw->ctl);
	}
	devm_kfree(&mgr->pdev->dev, hw);

	return ERR_PTR(ret);
}

static void mdss_rotator_free_hw(struct mdss_rot_mgr *mgr,
	struct mdss_rot_hw_resource *hw)
{
	struct mdss_mdp_mixer *mixer;
	struct mdss_mdp_ctl *ctl;

	mixer = hw->pipe->mixer_left;

	mdss_mdp_pipe_destroy(hw->pipe);

	ctl = mdss_mdp_ctl_mixer_switch(mixer->ctl,
		MDSS_MDP_WB_CTL_TYPE_BLOCK);
	if (ctl) {
		if (ctl->ops.stop_fnc)
			ctl->ops.stop_fnc(ctl, MDSS_PANEL_POWER_OFF);
		mdss_mdp_ctl_free(ctl);
	}

	devm_kfree(&mgr->pdev->dev, hw);
}

struct mdss_rot_hw_resource *mdss_rotator_get_hw_resource(
	struct mdss_rot_queue *queue, struct mdss_rot_entry *entry)
{
	struct mdss_rot_hw_resource *hw = queue->hw;

	if (!hw) {
		pr_err("no hw in the queue\n");
		return NULL;
	}

	mutex_lock(&queue->hw_lock);

	if (hw->workload) {
		hw = ERR_PTR(-EBUSY);
		goto get_hw_resource_err;
	}
	hw->workload = entry;

get_hw_resource_err:
	mutex_unlock(&queue->hw_lock);
	return hw;
}

static void mdss_rotator_put_hw_resource(struct mdss_rot_queue *queue,
	struct mdss_rot_hw_resource *hw)
{
	mutex_lock(&queue->hw_lock);
	hw->workload = NULL;
	mutex_unlock(&queue->hw_lock);
}

/*
 * caller will need to call mdss_rotator_deinit_queue when
 * the function returns error
 */
static int mdss_rotator_init_queue(struct mdss_rot_mgr *mgr)
{
	int i, size, ret = 0;
	char name[32];

	size = sizeof(struct mdss_rot_queue) * mgr->queue_count;
	mgr->queues = devm_kzalloc(&mgr->pdev->dev, size, GFP_KERNEL);
	if (!mgr->queues)
		return -ENOMEM;

	for (i = 0; i < mgr->queue_count; i++) {
		snprintf(name, sizeof(name), "rot_workq_%d", i);
		pr_debug("work queue name=%s\n", name);
		mgr->queues[i].rot_work_queue =
			create_singlethread_workqueue(name);
		if (!mgr->queues[i].rot_work_queue) {
			ret = -EPERM;
			break;
		}

		snprintf(name, sizeof(name), "rot_timeline_%d", i);
		pr_debug("timeline name=%s\n", name);
		mgr->queues[i].timeline.timeline =
			sw_sync_timeline_create(name);
		if (!mgr->queues[i].timeline.timeline) {
			ret = -EPERM;
			break;
		}

		size = sizeof(mgr->queues[i].timeline.fence_name);
		snprintf(mgr->queues[i].timeline.fence_name, size,
				"rot_fence_%d", i);
		mutex_init(&mgr->queues[i].timeline.lock);

		mutex_init(&mgr->queues[i].hw_lock);
	}

	return ret;
}

static void mdss_rotator_deinit_queue(struct mdss_rot_mgr *mgr)
{
	int i;

	if (!mgr->queues)
		return;

	for (i = 0; i < mgr->queue_count; i++) {
		if (mgr->queues[i].rot_work_queue)
			destroy_workqueue(mgr->queues[i].rot_work_queue);

		if (mgr->queues[i].timeline.timeline) {
			struct sync_timeline *obj;
			obj = (struct sync_timeline *)
				mgr->queues[i].timeline.timeline;
			sync_timeline_destroy(obj);
		}
	}
	devm_kfree(&mgr->pdev->dev, mgr->queues);
	mgr->queue_count = 0;
}

/*
 * mdss_rotator_assign_queue() - Function assign rotation work onto hw
 * @mgr:	Rotator manager.
 * @entry:	Contains details on rotator work item being requested
 * @private:	Private struct used for access rot session performance struct
 *
 * This Function allocates hw required to complete rotation work item
 * requested.
 *
 * Caller is responsible for calling cleanup function if error is returned
 */
static int mdss_rotator_assign_queue(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry *entry,
	struct mdss_rot_file_private *private)
{
	struct mdss_rot_perf *perf;
	struct mdss_rot_queue *queue;
	struct mdss_rot_hw_resource *hw;
	struct mdp_rotation_item *item = &entry->item;
	u32 wb_idx = item->wb_idx;
	u32 pipe_idx = item->pipe_idx;
	int ret = 0;

	/*
	 * todo: instead of always assign writeback block 0, we can
	 * apply some load balancing logic in the future
	 */
	if (wb_idx == MDSS_ROTATION_HW_ANY) {
		wb_idx = 0;
		pipe_idx = 0;
	}

	if (wb_idx >= mgr->queue_count) {
		pr_err("Invalid wb idx = %d\n", wb_idx);
		return -EINVAL;
	}

	queue = mgr->queues + wb_idx;

	mutex_lock(&queue->hw_lock);

	if (!queue->hw) {
		hw = mdss_rotator_hw_alloc(mgr, pipe_idx, wb_idx);
		if (IS_ERR_OR_NULL(hw)) {
			pr_err("fail to allocate hw\n");
			ret = PTR_ERR(hw);
		} else {
			queue->hw = hw;
		}
	}

	if (queue->hw) {
		entry->queue = queue;
		queue->hw->pending_count++;
	}

	mutex_unlock(&queue->hw_lock);

	perf = mdss_rotator_find_session(private, item->session_id);
	if (!perf) {
		pr_err("Could not find session based on rotation work item\n");
		return -EINVAL;
	}

	entry->perf = perf;
	perf->last_wb_idx = wb_idx;

	return ret;
}

static void mdss_rotator_unassign_queue(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry *entry)
{
	struct mdss_rot_queue *queue = entry->queue;

	if (!queue)
		return;

	entry->queue = NULL;

	mutex_lock(&queue->hw_lock);

	if (!queue->hw) {
		pr_err("entry assigned a queue with no hw\n");
		mutex_unlock(&queue->hw_lock);
		return;
	}

	queue->hw->pending_count--;
	if (queue->hw->pending_count == 0) {
		mdss_rotator_free_hw(mgr, queue->hw);
		queue->hw = NULL;
	}

	mutex_unlock(&queue->hw_lock);
}

static void mdss_rotator_queue_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private,
	struct mdss_rot_entry_container *req)
{
	struct mdss_rot_entry *entry;
	struct mdss_rot_queue *queue;
	unsigned long clk_rate;
	atomic_t *wrk_dis;
	u32 wb_idx;
	int i;

	for (i = 0; i < req->count; i++) {
		entry = req->entries + i;
		queue = entry->queue;
		wb_idx = queue->hw->wb_id;
		wrk_dis = &entry->perf->work_distribution[wb_idx];
		atomic_inc(wrk_dis);
		entry->work_assigned = true;
	}

	clk_rate = mdss_rotator_clk_rate_calc(mgr, private);
	mdss_rotator_set_clk_rate(mgr, clk_rate, MDSS_CLK_ROTATOR_CORE);

	for (i = 0; i < req->count; i++) {
		entry = req->entries + i;
		queue = entry->queue;
		entry->output_fence = NULL;
		queue_work(queue->rot_work_queue, &entry->commit_work);
	}
}

static int mdss_rotator_calc_perf(struct mdss_rot_perf *perf)
{
	struct mdp_rotation_config *config = &perf->config;
	u32 read_bw, write_bw;
	struct mdss_mdp_format_params *in_fmt, *out_fmt;

	in_fmt = mdss_mdp_get_format_params(config->input.format);
	if (!in_fmt) {
		pr_err("invalid input format\n");
		return -EINVAL;
	}
	out_fmt = mdss_mdp_get_format_params(config->output.format);
	if (!out_fmt) {
		pr_err("invalid output format\n");
		return -EINVAL;
	}

	perf->clk_rate = config->input.width * config->input.height;
	perf->clk_rate *= config->frame_rate;
	/* rotator processes 4 pixels per clock */
	perf->clk_rate /= 4;

	read_bw = config->input.width * config->input.height *
		config->frame_rate;
	if (in_fmt->chroma_sample == MDSS_MDP_CHROMA_420)
		read_bw = (read_bw * 3) / 2;
	else
		read_bw *= in_fmt->bpp;

	write_bw = config->output.width * config->output.height *
		config->frame_rate;
	if (out_fmt->chroma_sample == MDSS_MDP_CHROMA_420)
		write_bw = (write_bw * 3) / 2;
	else
		write_bw *= out_fmt->bpp;

	read_bw = mdss_apply_overhead_factors(read_bw,
		true, true, in_fmt, &config->input.comp_ratio);
	write_bw = mdss_apply_overhead_factors(write_bw,
		true, false, out_fmt, &config->input.comp_ratio);

	perf->bw = read_bw + write_bw;
	return 0;
}

static int mdss_rotator_update_perf(struct mdss_rot_mgr *mgr)
{
	struct mdss_rot_file_private *priv;
	u64 total_bw = 0;

	mutex_lock(&mgr->file_lock);
	list_for_each_entry(priv, &mgr->file_list, list) {
		struct mdss_rot_perf *perf;
		mutex_lock(&priv->perf_lock);
		list_for_each_entry(perf, &priv->perf_list, list) {
			total_bw += perf->bw;
		}
		mutex_unlock(&priv->perf_lock);
	}
	mutex_unlock(&mgr->file_lock);

	mutex_lock(&mgr->bus_lock);
	mdss_rotator_enable_reg_bus(mgr, total_bw);
	mdss_rotator_bus_scale_set_quota(&mgr->data_bus, total_bw);
	mutex_unlock(&mgr->bus_lock);
	return 0;
}

static void mdss_rotator_release_from_work_distribution(
		struct mdss_rot_mgr *mgr,
		struct mdss_rot_entry *entry)
{
	if (entry->work_assigned) {
		atomic_t *wrk_dis;
		u32 wb_idx = entry->queue->hw->wb_id;

		wrk_dis = &entry->perf->work_distribution[wb_idx];
		atomic_add_unless(wrk_dis, -1, 0);
		entry->work_assigned = false;
		if (entry->perf->waiting_for_completion &&
				!mdss_rotator_is_work_pending(mgr,
				entry->perf)) {
			complete(&entry->perf->stop_comp);
			entry->perf->waiting_for_completion = false;
		}
	}
}

static void mdss_rotator_release_entry(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry *entry)
{
	mdss_rotator_release_from_work_distribution(mgr, entry);
	mdss_rotator_clear_fence(entry);
	mdss_rotator_release_data(entry);
	mdss_rotator_unassign_queue(mgr, entry);
}

static int mdss_rotator_config_dnsc_factor(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry *entry)
{
	int ret = 0;
	u16 src_w, src_h, dst_w, dst_h, bit;
	struct mdp_rotation_item *item = &entry->item;
	struct mdss_mdp_format_params *fmt;

	src_w = item->src_rect.w;
	src_h = item->src_rect.h;

	if (item->flags & MDP_ROTATION_90) {
		dst_w = item->dst_rect.h;
		dst_h = item->dst_rect.w;
	} else {
		dst_w = item->dst_rect.w;
		dst_h = item->dst_rect.h;
	}

	if (!mgr->has_downscale &&
		(src_w != dst_w || src_h != dst_h)) {
		pr_err("rotator downscale not supported\n");
		ret = -EINVAL;
		goto dnsc_err;
	}

	entry->dnsc_factor_w = 0;
	entry->dnsc_factor_h = 0;

	if ((src_w != dst_w) || (src_h != dst_h)) {
		if ((src_w % dst_w) || (src_h % dst_h)) {
			ret = -EINVAL;
			goto dnsc_err;
		}
		entry->dnsc_factor_w = src_w / dst_w;
		bit = fls(entry->dnsc_factor_w);
		if ((entry->dnsc_factor_w & ~BIT(bit - 1)) || (bit > 5)) {
			ret = -EINVAL;
			goto dnsc_err;
		}
		entry->dnsc_factor_h = src_h / dst_h;
		bit = fls(entry->dnsc_factor_h);
		if ((entry->dnsc_factor_h & ~BIT(bit - 1)) || (bit > 5)) {
			ret = -EINVAL;
			goto dnsc_err;
		}
	}

	fmt =  mdss_mdp_get_format_params(item->output.format);
	if (mdss_mdp_is_ubwc_format(fmt) &&
		(entry->dnsc_factor_h || entry->dnsc_factor_w)) {
		pr_err("ubwc not supported with downscale %d\n",
			item->output.format);
		ret = -EINVAL;
	}

dnsc_err:

	/* Downscaler does not support asymmetrical dnsc */
	if (entry->dnsc_factor_w != entry->dnsc_factor_h)
		ret = -EINVAL;

	if (ret) {
		pr_err("Invalid rotator downscale ratio %dx%d->%dx%d\n",
			src_w, src_h, dst_w, dst_h);
		entry->dnsc_factor_w = 0;
		entry->dnsc_factor_h = 0;
	}
	return ret;
}

static bool mdss_rotator_verify_format(struct mdss_rot_mgr *mgr,
	struct mdss_mdp_format_params *in_fmt,
	struct mdss_mdp_format_params *out_fmt, bool rotation)
{
	u8 in_v_subsample, in_h_subsample;
	u8 out_v_subsample, out_h_subsample;

	if (!mgr->has_ubwc && (mdss_mdp_is_ubwc_format(in_fmt) ||
			mdss_mdp_is_ubwc_format(out_fmt))) {
		pr_err("Rotator doesn't allow ubwc\n");
		return -EINVAL;
	}

	if (!(out_fmt->flag & VALID_ROT_WB_FORMAT)) {
		pr_err("Invalid output format\n");
		return false;
	}

	if (in_fmt->is_yuv != out_fmt->is_yuv) {
		pr_err("Rotator does not support CSC\n");
		return false;
	}

	/* Forcing same pixel depth */
	if (memcmp(in_fmt->bits, out_fmt->bits, sizeof(in_fmt->bits))) {
		/* Exception is that RGB can drop alpha or add X */
		if (in_fmt->is_yuv || out_fmt->alpha_enable ||
			(in_fmt->bits[C2_R_Cr] != out_fmt->bits[C2_R_Cr]) ||
			(in_fmt->bits[C0_G_Y] != out_fmt->bits[C0_G_Y]) ||
			(in_fmt->bits[C1_B_Cb] != out_fmt->bits[C1_B_Cb])) {
			pr_err("Bit format does not match\n");
			return false;
		}
	}

	/* Need to make sure that sub-sampling persists through rotation */
	if (rotation) {
		mdss_mdp_get_v_h_subsample_rate(in_fmt->chroma_sample,
			&in_v_subsample, &in_h_subsample);
		mdss_mdp_get_v_h_subsample_rate(out_fmt->chroma_sample,
			&out_v_subsample, &out_h_subsample);

		if ((in_v_subsample != out_h_subsample) ||
				(in_h_subsample != out_v_subsample)) {
			pr_err("Rotation has invalid subsampling\n");
			return false;
		}
	} else {
		if (in_fmt->chroma_sample != out_fmt->chroma_sample) {
			pr_err("Format subsampling mismatch\n");
			return false;
		}
	}

	pr_debug("in_fmt=%0d, out_fmt=%d, has_ubwc=%d\n",
		in_fmt->format, out_fmt->format, mgr->has_ubwc);
	return true;
}

static int mdss_rotator_verify_config(struct mdss_rot_mgr *mgr,
	struct mdp_rotation_config *config)
{
	struct mdss_mdp_format_params *in_fmt, *out_fmt;
	u8 in_v_subsample, in_h_subsample;
	u8 out_v_subsample, out_h_subsample;
	u32 input, output;
	bool rotation;

	input = config->input.format;
	output = config->output.format;
	rotation = (config->flags & MDP_ROTATION_90) ? true : false;

	in_fmt = mdss_mdp_get_format_params(input);
	if (!in_fmt) {
		pr_err("Unrecognized input format:%u\n", input);
		return -EINVAL;
	}

	out_fmt = mdss_mdp_get_format_params(output);
	if (!out_fmt) {
		pr_err("Unrecognized output format:%u\n", output);
		return -EINVAL;
	}

	mdss_mdp_get_v_h_subsample_rate(in_fmt->chroma_sample,
		&in_v_subsample, &in_h_subsample);
	mdss_mdp_get_v_h_subsample_rate(out_fmt->chroma_sample,
		&out_v_subsample, &out_h_subsample);

	/* Dimension of image needs to be divisible by subsample rate  */
	if ((config->input.height % in_v_subsample) ||
			(config->input.width % in_h_subsample)) {
		pr_err("In ROI, subsample mismatch, w=%d, h=%d, vss%d, hss%d\n",
			config->input.width, config->input.height,
			in_v_subsample, in_h_subsample);
		return -EINVAL;
	}

	if ((config->output.height % out_v_subsample) ||
			(config->output.width % out_h_subsample)) {
		pr_err("Out ROI, subsample mismatch, w=%d, h=%d, vss%d, hss%d\n",
			config->output.width, config->output.height,
			out_v_subsample, out_h_subsample);
		return -EINVAL;
	}

	if (!mdss_rotator_verify_format(mgr, in_fmt,
			out_fmt, rotation)) {
		pr_err("Rot format pairing invalid, in_fmt:%d, out_fmt:%d\n",
			input, output);
		return -EINVAL;
	}

	return 0;
}

static int mdss_rotator_validate_entry(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private,
	struct mdss_rot_entry *entry)
{
	int ret;
	u32 out_format, in_format;
	struct mdp_rotation_item *item;

	item = &entry->item;
	in_format = item->input.format;
	out_format = item->output.format;

	if (item->wb_idx != item->pipe_idx) {
		pr_err("invalid writeback and pipe idx\n");
		return -EINVAL;
	}

	if (item->wb_idx != MDSS_ROTATION_HW_ANY &&
		item->wb_idx > mgr->queue_count) {
		pr_err("invalid writeback idx\n");
		return -EINVAL;
	}

	if (!mdss_rotator_find_session(private, item->session_id)) {
		pr_err("Could not find session based on rotation work item\n");
		return -EINVAL;
	}

	ret = mdss_rotator_config_dnsc_factor(mgr, entry);
	if (ret) {
		pr_err("fail to configure downscale factor\n");
		return ret;
	}

	return ret;
}

/*
 * Upon failure from the function, caller needs to make sure
 * to call mdss_rotator_remove_request to clean up resources.
 */
static int mdss_rotator_add_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private,
	struct mdss_rot_entry_container *req)
{
	struct mdss_rot_entry *entry;
	struct mdp_rotation_item *item;
	u32 flag = 0;
	int i, ret;

	for (i = 0; i < req->count; i++) {
		entry = req->entries + i;
		item = &entry->item;

		if (item->flags & MDP_ROTATION_SECURE)
			flag = MDP_SECURE_OVERLAY_SESSION;

		ret = mdss_rotator_validate_entry(mgr, private, entry);
		if (ret) {
			pr_err("fail to validate the entry\n");
			return ret;
		}

		ret = mdss_rotator_import_data(mgr, entry);
		if (ret) {
			pr_err("fail to import the data\n");
			return ret;
		}

		if (item->input.fence >= 0) {
			entry->input_fence =
				sync_fence_fdget(item->input.fence);
			if (!entry->input_fence) {
				pr_err("invalid input fence fd\n");
				return -EINVAL;
			}
		}

		ret = mdss_rotator_assign_queue(mgr, entry, private);
		if (ret) {
			pr_err("fail to assign queue to entry\n");
			return ret;
		}

		entry->request = req;

		INIT_WORK(&entry->commit_work, mdss_rotator_wq_handler);

		ret = mdss_rotator_create_fence(entry);
		if (ret) {
			pr_err("fail to create fence\n");
			return ret;
		}
		item->output.fence = entry->output_fence_fd;
	}

	mutex_lock(&private->req_lock);
	list_add(&req->list, &private->req_list);
	mutex_unlock(&private->req_lock);

	return 0;
}

static void mdss_rotator_remove_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private,
	struct mdss_rot_entry_container *req)
{
	int i;

	mutex_lock(&private->req_lock);
	for (i = 0; i < req->count; i++)
		mdss_rotator_release_entry(mgr, req->entries + i);
	list_del_init(&req->list);
	mutex_unlock(&private->req_lock);
}

static void mdss_rotator_cancel_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_entry_container *req)
{
	struct mdss_rot_entry *entry;
	int i;

	/*
	 * To avoid signal the rotation entry output fence in the wrong
	 * order, all the entries in the same request needs to be cancelled
	 * first, before signaling the output fence.
	 */
	for (i = req->count - 1; i >= 0; i--) {
		entry = req->entries + i;
		cancel_work_sync(&entry->commit_work);
	}

	for (i = req->count - 1; i >= 0; i--) {
		entry = req->entries + i;
		mdss_rotator_signal_output(entry);
		mdss_rotator_release_entry(mgr, entry);
	}

	list_del_init(&req->list);
	devm_kfree(&mgr->pdev->dev, req);
}

static void mdss_rotator_release_private_resource(
	struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private)
{
	struct mdss_rot_entry_container *req, *req_next;
	struct mdss_rot_perf *perf, *perf_next;

	mutex_lock(&private->req_lock);
	list_for_each_entry_safe(req, req_next, &private->req_list, list)
		mdss_rotator_cancel_request(mgr, req);
	mutex_unlock(&private->req_lock);

	mutex_lock(&private->perf_lock);
	list_for_each_entry_safe(perf, perf_next, &private->perf_list, list) {
		list_del_init(&perf->list);
		devm_kfree(&mgr->pdev->dev, perf->work_distribution);
		devm_kfree(&mgr->pdev->dev, perf);
	}
	mutex_unlock(&private->perf_lock);
}

static void mdss_rotator_free_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private)
{
	struct mdss_rot_entry_container *req, *req_next;

	mutex_lock(&private->req_lock);
	list_for_each_entry_safe(req, req_next, &private->req_list, list) {
		if (atomic_read(&req->pending_count) == 0) {
			list_del_init(&req->list);
			devm_kfree(&mgr->pdev->dev, req);
		}
	}
	mutex_unlock(&private->req_lock);
}

static void mdss_rotator_release_all(struct mdss_rot_mgr *mgr)
{
	struct mdss_rot_file_private *priv, *priv_next;

	mutex_lock(&mgr->file_lock);
	list_for_each_entry_safe(priv, priv_next, &mgr->file_list, list) {
		mdss_rotator_release_private_resource(rot_mgr, priv);
		mdss_rotator_resource_ctrl(mgr, false);
		list_del_init(&priv->list);
		devm_kfree(&mgr->pdev->dev, priv);
	}
	mutex_unlock(&rot_mgr->file_lock);

	mdss_rotator_update_perf(mgr);
}

static int mdss_rotator_prepare_hw(struct mdss_rot_hw_resource *hw,
	struct mdss_rot_entry *entry)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_ctl *orig_ctl, *rot_ctl;
	int ret;

	pipe = hw->pipe;
	orig_ctl = pipe->mixer_left->ctl;
	if (orig_ctl->shared_lock)
		mutex_lock(orig_ctl->shared_lock);

	rot_ctl = mdss_mdp_ctl_mixer_switch(orig_ctl,
						MDSS_MDP_WB_CTL_TYPE_BLOCK);
	if (!rot_ctl) {
		ret = -EINVAL;
		goto error;
	} else {
		hw->ctl = rot_ctl;
		pipe->mixer_left = rot_ctl->mixer_left;
	}

	return 0;

error:
	if (orig_ctl->shared_lock)
		mutex_unlock(orig_ctl->shared_lock);
	return ret;
}

static void mdss_rotator_translate_rect(struct mdss_rect *dst,
	struct mdp_rect *src)
{
	dst->x = src->x;
	dst->y = src->y;
	dst->w = src->w;
	dst->h = src->h;
}

static u32 mdss_rotator_translate_flags(u32 input)
{
	u32 output;

	if (input & MDP_ROTATION_NOP)
		output |= MDP_ROT_NOP;
	if (input & MDP_ROTATION_FLIP_LR)
		output |= MDP_FLIP_LR;
	if (input & MDP_ROTATION_FLIP_UD)
		output |= MDP_FLIP_UD;
	if (input & MDP_ROTATION_90)
		output |= MDP_ROT_90;
	if (input & MDP_ROTATION_SECURE)
		output |= MDP_SECURE_OVERLAY_SESSION;
	if (input & MDP_ROTATION_BWC_EN)
		output |= MDP_BWC_EN;

	return output;
}

static int mdss_rotator_config_hw(struct mdss_rot_hw_resource *hw,
	struct mdss_rot_entry *entry)
{
	struct mdss_mdp_pipe *pipe;
	struct mdp_rotation_item *item;
	int ret;

	pipe = hw->pipe;
	item = &entry->item;

	pipe->flags = mdss_rotator_translate_flags(item->flags);
	pipe->src_fmt = mdss_mdp_get_format_params(item->input.format);
	pipe->img_width = item->input.width;
	pipe->img_height = item->input.height;
	mdss_rotator_translate_rect(&pipe->src, &item->src_rect);
	mdss_rotator_translate_rect(&pipe->dst, &item->src_rect);

	pipe->params_changed++;

	mdss_mdp_smp_release(pipe);

	ret = mdss_mdp_smp_reserve(pipe);
	if (ret) {
		pr_debug("unable to mdss_mdp_smp_reserve rot data\n");
		return ret;
	}

	ret = mdss_mdp_pipe_queue_data(pipe, &entry->src_buf);
	return ret;
}

static int mdss_rotator_kickoff_entry(struct mdss_rot_hw_resource *hw,
	struct mdss_rot_entry *entry)
{
	int ret;
	struct mdss_mdp_writeback_arg wb_args = {
		.data = &entry->dst_buf,
		.priv_data = entry,
	};

	ret = mdss_mdp_writeback_display_commit(hw->ctl, &wb_args);
	return ret;
}

static int mdss_rotator_wait_for_entry(struct mdss_rot_hw_resource *hw,
	struct mdss_rot_entry *entry)
{
	int ret;
	struct mdss_mdp_ctl *ctl = hw->ctl;

	ret = mdss_mdp_display_wait4comp(ctl);
	if (ctl->shared_lock)
		mutex_unlock(ctl->shared_lock);
	return ret;
}

static int mdss_rotator_commit_entry(struct mdss_rot_hw_resource *hw,
	struct mdss_rot_entry *entry)
{
	int ret;

	ret = mdss_rotator_prepare_hw(hw, entry);
	if (ret) {
		pr_err("fail to prepare hw resource %d\n", ret);
		return ret;
	}

	ret = mdss_rotator_config_hw(hw, entry);
	if (ret) {
		pr_err("fail to configure hw resource %d\n", ret);
		return ret;
	}

	ret = mdss_rotator_kickoff_entry(hw, entry);
	if (ret) {
		pr_err("fail to do kickoff %d\n", ret);
		return ret;
	}

	ret = mdss_rotator_wait_for_entry(hw, entry);
	if (ret) {
		pr_err("fail to wait for completion %d\n", ret);
		return ret;
	}

	return ret;
}

static int mdss_rotator_handle_entry(struct mdss_rot_hw_resource *hw,
	struct mdss_rot_entry *entry)
{
	int ret;

	ret = mdss_rotator_wait_for_input(entry);
	if (ret) {
		pr_err("wait for input buffer failed %d\n", ret);
		return ret;
	}

	ret = mdss_rotator_map_and_check_data(entry);
	if (ret) {
		pr_err("fail to prepare input/output data %d\n", ret);
		return ret;
	}

	ret = mdss_rotator_commit_entry(hw, entry);
	if (ret)
		pr_err("rotator commit failed %d\n", ret);

	return ret;
}

static void mdss_rotator_wq_handler(struct work_struct *work)
{
	struct mdss_rot_entry *entry;
	struct mdss_rot_entry_container *request;
	struct mdss_rot_hw_resource *hw;
	int ret;

	entry = container_of(work, struct mdss_rot_entry, commit_work);
	request = entry->request;

	if (!request) {
		pr_err("fatal error, no request with entry\n");
		return;
	}

	mdss_rotator_clk_ctrl(rot_mgr, true);
	hw = mdss_rotator_get_hw_resource(entry->queue, entry);
	if (!hw) {
		pr_err("no hw for the queue\n");
		goto get_hw_res_err;
	}

	ret = mdss_rotator_handle_entry(hw, entry);
	if (ret)
		pr_err("fail to complete the roation request\n");

	mdss_rotator_put_hw_resource(entry->queue, hw);

get_hw_res_err:
	mdss_rotator_signal_output(entry);
	mdss_rotator_clk_ctrl(rot_mgr, false);
	mdss_rotator_release_entry(rot_mgr, entry);
	atomic_dec(&request->pending_count);
}

static int mdss_rotator_validate_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private,
	struct mdss_rot_entry_container *req)
{
	int i, ret = 0;
	struct mdss_rot_entry *entry;

	for (i = 0; i < req->count; i++) {
		entry = req->entries + i;
		ret = mdss_rotator_validate_entry(mgr, private,
			entry);
		if (ret) {
			pr_err("fail to validate the entry\n");
			return ret;
		}
	}

	return ret;
}

static u32 mdss_rotator_generator_session_id(struct mdss_rot_mgr *mgr)
{
	u32 id;
	mutex_lock(&mgr->lock);
	id = mgr->session_id_generator++;
	mutex_unlock(&mgr->lock);
	return id;
}

static int mdss_rotator_open_session(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private, unsigned long arg)
{
	struct mdp_rotation_config config;
	struct mdss_rot_perf *perf;
	int ret, i;

	ret = copy_from_user(&config, (void __user *)arg, sizeof(config));
	if (ret) {
		pr_err("fail to copy session data\n");
		return ret;
	}

	ret = mdss_rotator_verify_config(mgr, &config);
	if (ret) {
		pr_err("Rotator verify format failed\n");
		return ret;
	}

	perf = devm_kzalloc(&mgr->pdev->dev, sizeof(*perf), GFP_KERNEL);
	if (!perf) {
		pr_err("fail to allocate session\n");
		return -ENOMEM;
	}

	perf->work_distribution = devm_kzalloc(&mgr->pdev->dev,
		sizeof(atomic_t) * mgr->queue_count, GFP_KERNEL);
	if (!perf->work_distribution) {
		pr_err("fail to allocate work_distribution\n");
		ret = -ENOMEM;
		goto alloc_err;
	}

	config.session_id = mdss_rotator_generator_session_id(mgr);
	perf->config = config;
	perf->last_wb_idx = -1;
	init_completion(&perf->stop_comp);
	for (i = 0; i < mgr->queue_count; i++)
		atomic_set(&perf->work_distribution[i], 0);

	INIT_LIST_HEAD(&perf->list);

	ret = mdss_rotator_calc_perf(perf);
	if (ret) {
		pr_err("error setting the session%d\n", ret);
		goto copy_user_err;
	}

	ret = copy_to_user((void *)arg, &config, sizeof(config));
	if (ret) {
		pr_err("fail to copy to user\n");
		goto copy_user_err;
	}

	mutex_lock(&private->perf_lock);
	list_add(&perf->list, &private->perf_list);
	mutex_unlock(&private->perf_lock);

	ret = mdss_rotator_resource_ctrl(mgr, true);
	if (ret) {
		pr_err("Failed to aqcuire rotator resources\n");
		goto resource_err;
	}

	ret = mdss_rotator_update_perf(mgr);
	if (ret) {
		pr_err("fail to open session, not enough clk/bw\n");
		goto perf_err;
	}

	return ret;
perf_err:
	mdss_rotator_resource_ctrl(mgr, false);
resource_err:
	mutex_lock(&private->perf_lock);
	list_del_init(&perf->list);
	mutex_unlock(&private->perf_lock);
copy_user_err:
	devm_kfree(&mgr->pdev->dev, perf->work_distribution);
alloc_err:
	devm_kfree(&mgr->pdev->dev, perf);
	return ret;
}

static int mdss_rotator_close_session(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private, unsigned long arg)
{
	struct mdss_rot_perf *perf;
	u32 id;

	id = (u32)arg;
	perf = mdss_rotator_find_session(private, id);
	if (!perf) {
		pr_err("Trying to close session that does not exist\n");
		return -EINVAL;
	}

	/*
	 * Client should not be calling close session when there
	 * are pending rotation work. Driver will attempt to
	 * wait for request to complete.
	 */

	if (mdss_rotator_is_work_pending(mgr, perf)) {
		pr_debug("Work is still pending, attempting to wait\n");
		reinit_completion(&perf->stop_comp);
		perf->waiting_for_completion = true;
		wait_for_completion_timeout(&perf->stop_comp, 3 * KOFF_TIMEOUT);

		if (mdss_rotator_is_work_pending(mgr, perf)) {
			pr_err("Calling session close, rot work is pending\n");
			return -EBUSY;
		}
	}

	mdss_rotator_resource_ctrl(mgr, false);
	mutex_lock(&private->perf_lock);
	list_del_init(&perf->list);
	mutex_unlock(&private->perf_lock);
	devm_kfree(&mgr->pdev->dev, perf->work_distribution);
	devm_kfree(&mgr->pdev->dev, perf);
	mdss_rotator_update_perf(mgr);
	return 0;
}

static int mdss_rotator_config_session(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private, unsigned long arg)
{
	int ret;
	struct mdss_rot_perf *perf;
	struct mdp_rotation_config config;

	ret = copy_from_user(&config, (void __user *)arg,
				sizeof(config));
	if (ret) {
		pr_err("fail to copy session data\n");
		return ret;
	}

	ret = mdss_rotator_verify_config(mgr, &config);
	if (ret) {
		pr_err("Rotator verify format failed\n");
		return ret;
	}

	perf = mdss_rotator_find_session(private, config.session_id);
	if (!perf)
		return -EINVAL;

	mutex_lock(&private->perf_lock);
	perf->config = config;
	ret = mdss_rotator_calc_perf(perf);
	mutex_unlock(&private->perf_lock);

	if (ret) {
		pr_err("error in configuring the session %d\n", ret);
		return ret;
	}

	ret = mdss_rotator_update_perf(mgr);

	return ret;
}

struct mdss_rot_entry_container *mdss_rotator_req_init(
	struct mdss_rot_mgr *mgr, struct mdp_rotation_item *items,
	u32 count, u32 flags)
{
	struct mdss_rot_entry_container *req;
	int size, i;

	size = sizeof(struct mdss_rot_entry_container);
	size += sizeof(struct mdss_rot_entry) * count;
	req = devm_kzalloc(&mgr->pdev->dev, size, GFP_KERNEL);

	if (!req) {
		pr_err("fail to allocate rotation request\n");
		return ERR_PTR(-ENOMEM);
	}

	INIT_LIST_HEAD(&req->list);
	req->count = count;
	req->entries = (struct mdss_rot_entry *)
		((void *)req + sizeof(struct mdss_rot_entry_container));
	req->flags = flags;
	atomic_set(&req->pending_count, count);

	for (i = 0; i < count; i++)
		req->entries[i].item = items[i];

	return req;
}

static int mdss_rotator_handle_request_common(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private,
	struct mdss_rot_entry_container *req,
	struct mdp_rotation_item *items)
{
	int i, ret;

	mdss_rotator_free_request(mgr, private);

	ret = mdss_rotator_add_request(mgr, private, req);
	if (ret) {
		pr_err("fail to add rotation request\n");
		mdss_rotator_remove_request(mgr, private, req);
		return ret;
	}

	for (i = 0; i < req->count; i++)
		items[i].output.fence =
			req->entries[i].item.output.fence;

	return ret;
}

static int mdss_rotator_handle_request(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private, unsigned long arg)
{
	struct mdp_rotation_request user_req;
	struct mdp_rotation_item *items = NULL;
	struct mdss_rot_entry_container *req = NULL;
	int size, ret;

	ret = copy_from_user(&user_req, (void __user *)arg,
					sizeof(user_req));
	if (ret) {
		pr_err("fail to copy rotation request\n");
		return ret;
	}

	/*
	 * here, we make a copy of the items so that we can copy
	 * all the output fences to the client in one call.   Otherwise,
	 * we will have to call multiple copy_to_user
	 */
	size = sizeof(struct mdp_rotation_item) * user_req.count;
	items = devm_kzalloc(&mgr->pdev->dev, size, GFP_KERNEL);
	if (!items) {
		pr_err("fail to allocate rotation items\n");
		return -ENOMEM;
	}
	ret = copy_from_user(items, user_req.list, size);
	if (ret) {
		pr_err("fail to copy rotation items\n");
		goto handle_request_err;
	}

	req = mdss_rotator_req_init(mgr, items, user_req.count, user_req.flags);
	if (IS_ERR_OR_NULL(req)) {
		pr_err("fail to allocate rotation request\n");
		ret = PTR_ERR(req);
		goto handle_request_err;
	}

	mutex_lock(&mgr->lock);

	if (req->flags & MDSS_ROTATION_REQUEST_VALIDATE) {
		ret = mdss_rotator_validate_request(mgr, private, req);
		goto handle_request_err1;
	}

	ret = mdss_rotator_handle_request_common(mgr, private, req, items);
	if (ret) {
		pr_err("fail to handle request\n");
		goto handle_request_err1;
	}

	ret = copy_to_user(user_req.list, items, size);
	if (ret) {
		pr_err("fail to copy output fence to user\n");
		mdss_rotator_remove_request(mgr, private, req);
		goto handle_request_err1;
	}

	mdss_rotator_queue_request(mgr, private, req);

	mutex_unlock(&mgr->lock);

	devm_kfree(&mgr->pdev->dev, items);
	return ret;

handle_request_err1:
	mutex_unlock(&mgr->lock);
handle_request_err:
	devm_kfree(&mgr->pdev->dev, items);
	devm_kfree(&mgr->pdev->dev, req);
	return ret;
}

static int mdss_rotator_open(struct inode *inode, struct file *file)
{
	struct mdss_rot_file_private *private;

	if (!rot_mgr)
		return -ENODEV;

	if (atomic_read(&rot_mgr->device_suspended))
		return -EPERM;

	private = devm_kzalloc(&rot_mgr->pdev->dev, sizeof(*private),
		GFP_KERNEL);
	if (!private) {
		pr_err("fail to allocate rotation file private data\n");
		return -ENOMEM;
	}
	mutex_init(&private->req_lock);
	mutex_init(&private->perf_lock);
	INIT_LIST_HEAD(&private->req_list);
	INIT_LIST_HEAD(&private->perf_list);
	INIT_LIST_HEAD(&private->list);

	mutex_lock(&rot_mgr->file_lock);
	list_add(&private->list, &rot_mgr->file_list);
	mutex_unlock(&rot_mgr->file_lock);

	file->private_data = private;

	return 0;
}

static int mdss_rotator_close(struct inode *inode, struct file *file)
{
	struct mdss_rot_file_private *private;

	if (!rot_mgr)
		return -ENODEV;

	if (!file->private_data)
		return -EINVAL;

	private = (struct mdss_rot_file_private *)file->private_data;

	mdss_rotator_release_private_resource(rot_mgr, private);

	mutex_lock(&rot_mgr->file_lock);
	list_del_init(&private->list);
	devm_kfree(&rot_mgr->pdev->dev, private);
	file->private_data = NULL;
	mutex_unlock(&rot_mgr->file_lock);

	mdss_rotator_update_perf(rot_mgr);
	return 0;
}

#ifdef CONFIG_COMPAT
static int mdss_rotator_handle_request32(struct mdss_rot_mgr *mgr,
	struct mdss_rot_file_private *private, unsigned long arg)
{
	struct mdp_rotation_request32 user_req32;
	struct mdp_rotation_item *items = NULL;
	struct mdss_rot_entry_container *req = NULL;
	int size, ret;

	ret = copy_from_user(&user_req32, (void __user *)arg,
					sizeof(user_req32));
	if (ret) {
		pr_err("fail to copy rotation request\n");
		return ret;
	}

	size = sizeof(struct mdp_rotation_item) * user_req32.count;
	items = devm_kzalloc(&mgr->pdev->dev, size, GFP_KERNEL);
	if (!items) {
		pr_err("fail to allocate rotation items\n");
		return -ENOMEM;
	}
	ret = copy_from_user(items, user_req32.list, size);
	if (ret) {
		pr_err("fail to copy rotation items\n");
		goto handle_request32_err;
	}

	req = mdss_rotator_req_init(mgr, items, user_req32.count,
		user_req32.flags);
	if (IS_ERR_OR_NULL(req)) {
		pr_err("fail to allocate rotation request\n");
		ret = PTR_ERR(req);
		goto handle_request32_err;
	}

	mutex_lock(&mgr->lock);

	if (req->flags & MDSS_ROTATION_REQUEST_VALIDATE) {
		ret = mdss_rotator_validate_request(mgr, private, req);
		goto handle_request32_err1;
	}

	ret = mdss_rotator_handle_request_common(mgr, private, req, items);
	if (ret) {
		pr_err("fail to handle request\n");
		goto handle_request32_err1;
	}

	ret = copy_to_user(user_req32.list, items, size);
	if (ret) {
		pr_err("fail to copy output fence to user\n");
		mdss_rotator_remove_request(mgr, private, req);
		goto handle_request32_err1;
	}

	mdss_rotator_queue_request(mgr, private, req);

	mutex_unlock(&mgr->lock);

	devm_kfree(&mgr->pdev->dev, items);
	return ret;

handle_request32_err1:
	mutex_unlock(&mgr->lock);
handle_request32_err:
	devm_kfree(&mgr->pdev->dev, items);
	devm_kfree(&mgr->pdev->dev, req);
	return ret;
}

static long mdss_rotator_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct mdss_rot_file_private *private;
	int ret = -EINVAL;

	if (!rot_mgr)
		return -ENODEV;

	if (atomic_read(&rot_mgr->device_suspended))
		return -EPERM;

	if (!file->private_data)
		return -EINVAL;

	private = (struct mdss_rot_file_private *)file->private_data;

	switch (cmd) {
	case MDSS_ROTATION_REQUEST:
		ret = mdss_rotator_handle_request32(rot_mgr, private, arg);
		break;
	case MDSS_ROTATION_OPEN:
		ret = mdss_rotator_open_session(rot_mgr, private, arg);
		break;
	case MDSS_ROTATION_CLOSE:
		ret = mdss_rotator_close_session(rot_mgr, private, arg);
		break;
	case MDSS_ROTATION_CONFIG:
		ret = mdss_rotator_config_session(rot_mgr, private, arg);
		break;
	default:
		pr_err("unexpected IOCTL %d\n", cmd);
	}

	if (ret)
		pr_err("rotator ioctl=%d failed, err=%d\n", cmd, ret);
	return ret;

}
#endif

static long mdss_rotator_ioctl(struct file *file, unsigned int cmd,
						 unsigned long arg)
{
	struct mdss_rot_file_private *private;
	int ret = -EINVAL;

	if (!rot_mgr)
		return -ENODEV;

	if (atomic_read(&rot_mgr->device_suspended))
		return -EPERM;

	if (!file->private_data)
		return -EINVAL;

	private = (struct mdss_rot_file_private *)file->private_data;

	switch (cmd) {
	case MDSS_ROTATION_REQUEST:
		ret = mdss_rotator_handle_request(rot_mgr, private, arg);
		break;
	case MDSS_ROTATION_OPEN:
		ret = mdss_rotator_open_session(rot_mgr, private, arg);
		break;
	case MDSS_ROTATION_CLOSE:
		ret = mdss_rotator_close_session(rot_mgr, private, arg);
		break;
	case MDSS_ROTATION_CONFIG:
		ret = mdss_rotator_config_session(rot_mgr, private, arg);
		break;
	default:
		pr_err("unexpected IOCTL %d\n", cmd);
	}

	if (ret)
		pr_err("rotator ioctl=%d failed, err=%d\n", cmd, ret);
	return ret;
}

static ssize_t mdss_rotator_show_capabilities(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t len = PAGE_SIZE;
	int cnt = 0;

	if (!rot_mgr)
		return cnt;

#define SPRINT(fmt, ...) \
		(cnt += scnprintf(buf + cnt, len - cnt, fmt, ##__VA_ARGS__))

	SPRINT("wb_count=%d\n", rot_mgr->queue_count);
	SPRINT("downscale=%d\n", rot_mgr->has_downscale);

	return cnt;
}

static DEVICE_ATTR(caps, S_IRUGO, mdss_rotator_show_capabilities, NULL);

static struct attribute *mdss_rotator_fs_attrs[] = {
	&dev_attr_caps.attr,
	NULL
};

static struct attribute_group mdss_rotator_fs_attr_group = {
	.attrs = mdss_rotator_fs_attrs
};

static const struct file_operations mdss_rotator_fops = {
	.owner = THIS_MODULE,
	.open = mdss_rotator_open,
	.release = mdss_rotator_close,
	.unlocked_ioctl = mdss_rotator_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mdss_rotator_compat_ioctl,
#endif
};

static int mdss_rotator_parse_dt_bus(struct mdss_rot_mgr *mgr,
	struct platform_device *dev)
{
	int ret = 0, i;
	bool register_bus_needed;
	int usecases;

	mgr->data_bus.bus_scale_pdata = msm_bus_cl_get_pdata(dev);
	if (IS_ERR_OR_NULL(mgr->data_bus.bus_scale_pdata)) {
		ret = PTR_ERR(mgr->data_bus.bus_scale_pdata);
		if (!ret) {
			ret = -EINVAL;
			pr_err("msm_bus_cl_get_pdata failed. ret=%d\n", ret);
			mgr->data_bus.bus_scale_pdata = NULL;
		}
	}

	register_bus_needed = of_property_read_bool(dev->dev.of_node,
		"qcom,mdss-has-reg-bus");
	if (register_bus_needed) {
		mgr->reg_bus.bus_scale_pdata = &rot_reg_bus_scale_table;
		usecases = mgr->reg_bus.bus_scale_pdata->num_usecases;
		for (i = 0; i < usecases; i++) {
			rot_reg_bus_usecases[i].num_paths = 1;
			rot_reg_bus_usecases[i].vectors =
				&rot_reg_bus_vectors[i];
		}
	}
	return ret;
}

static int mdss_rotator_parse_dt(struct mdss_rot_mgr *mgr,
	struct platform_device *dev)
{
	int ret = 0;
	u32 data;

	ret = of_property_read_u32(dev->dev.of_node,
		"qcom,mdss-wb-count", &data);
	if (ret) {
		pr_err("Error in device tree\n");
		return ret;
	}
	if (data > ROT_MAX_HW_BLOCKS) {
		pr_err("Err, num of wb block (%d) larger than sw max %d\n",
			data, ROT_MAX_HW_BLOCKS);
		return -EINVAL;
	}

	rot_mgr->queue_count = data;
	rot_mgr->has_downscale = of_property_read_bool(dev->dev.of_node,
					   "qcom,mdss-has-downscale");
	rot_mgr->has_ubwc = of_property_read_bool(dev->dev.of_node,
					   "qcom,mdss-has-ubwc");

	ret = mdss_rotator_parse_dt_bus(mgr, dev);
	if (ret)
		pr_err("Failed to parse bus data\n");

	return ret;
}

static void mdss_rotator_put_dt_vreg_data(struct device *dev,
	struct dss_module_power *mp)
{
	if (!mp) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	msm_dss_config_vreg(dev, mp->vreg_config, mp->num_vreg, 0);
	if (mp->vreg_config) {
		devm_kfree(dev, mp->vreg_config);
		mp->vreg_config = NULL;
	}
	mp->num_vreg = 0;
}

static int mdss_rotator_get_dt_vreg_data(struct device *dev,
	struct dss_module_power *mp)
{
	const char *st = NULL;
	struct device_node *of_node = NULL;
	int dt_vreg_total = 0;
	int i;
	int rc;

	if (!dev || !mp) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	of_node = dev->of_node;

	dt_vreg_total = of_property_count_strings(of_node, "qcom,supply-names");
	if (dt_vreg_total < 0) {
		DEV_ERR("%s: vreg not found. rc=%d\n", __func__,
			dt_vreg_total);
		return 0;
	}
	mp->num_vreg = dt_vreg_total;
	mp->vreg_config = devm_kzalloc(dev, sizeof(struct dss_vreg) *
		dt_vreg_total, GFP_KERNEL);
	if (!mp->vreg_config) {
		DEV_ERR("%s: can't alloc vreg mem\n", __func__);
		return -ENOMEM;
	}

	/* vreg-name */
	for (i = 0; i < dt_vreg_total; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,supply-names", i, &st);
		if (rc) {
			DEV_ERR("%s: error reading name. i=%d, rc=%d\n",
				__func__, i, rc);
			goto error;
		}
		snprintf(mp->vreg_config[i].vreg_name, 32, "%s", st);
	}
	msm_dss_config_vreg(dev, mp->vreg_config, mp->num_vreg, 1);

	for (i = 0; i < dt_vreg_total; i++) {
		DEV_DBG("%s: %s min=%d, max=%d, enable=%d disable=%d\n",
			__func__,
			mp->vreg_config[i].vreg_name,
			mp->vreg_config[i].min_voltage,
			mp->vreg_config[i].max_voltage,
			mp->vreg_config[i].enable_load,
			mp->vreg_config[i].disable_load);
	}
	return rc;

error:
	if (mp->vreg_config) {
		devm_kfree(dev, mp->vreg_config);
		mp->vreg_config = NULL;
	}
	mp->num_vreg = 0;
	return rc;
}

static void mdss_rotator_bus_scale_unregister(struct mdss_rot_mgr *mgr)
{
	pr_debug("unregister bus_hdl=%x, reg_bus_hdl=%x\n",
		mgr->data_bus.bus_hdl, mgr->reg_bus.bus_hdl);

	if (mgr->data_bus.bus_hdl)
		msm_bus_scale_unregister_client(mgr->data_bus.bus_hdl);

	if (mgr->reg_bus.bus_hdl)
		msm_bus_scale_unregister_client(mgr->reg_bus.bus_hdl);
}

static int mdss_rotator_bus_scale_register(struct mdss_rot_mgr *mgr)
{
	if (!mgr->data_bus.bus_scale_pdata) {
		pr_err("Scale table is NULL\n");
		return -EINVAL;
	}

	mgr->data_bus.bus_hdl =
		msm_bus_scale_register_client(
		mgr->data_bus.bus_scale_pdata);
	if (!mgr->data_bus.bus_hdl) {
		pr_err("bus_client register failed\n");
		return -EINVAL;
	}
	pr_debug("registered bus_hdl=%x\n", mgr->data_bus.bus_hdl);

	if (mgr->reg_bus.bus_scale_pdata) {
		mgr->reg_bus.bus_hdl =
			msm_bus_scale_register_client(
			mgr->reg_bus.bus_scale_pdata);
		if (!mgr->reg_bus.bus_hdl) {
			pr_err("register bus_client register failed\n");
			mdss_rotator_bus_scale_unregister(mgr);
			return -EINVAL;
		}
		pr_debug("registered register bus_hdl=%x\n",
			mgr->reg_bus.bus_hdl);
	}

	return 0;
}

static int mdss_rotator_clk_register(struct platform_device *pdev,
	struct mdss_rot_mgr *mgr, char *clk_name, u32 clk_idx)
{
	struct clk *tmp;
	pr_debug("registered clk_reg\n");

	if (clk_idx >= MDSS_CLK_ROTATOR_END_IDX) {
		pr_err("invalid clk index %d\n", clk_idx);
		return -EINVAL;
	}

	if (mgr->rot_clk[clk_idx]) {
		pr_err("Stomping on clk prev registered:%d\n", clk_idx);
		return -EINVAL;
	}

	tmp = devm_clk_get(&pdev->dev, clk_name);
	if (IS_ERR(tmp)) {
		pr_err("unable to get clk: %s\n", clk_name);
		return PTR_ERR(tmp);
	}
	mgr->rot_clk[clk_idx] = tmp;
	return 0;
}

static int mdss_rotator_res_init(struct platform_device *pdev,
	struct mdss_rot_mgr *mgr)
{
	int ret;

	ret = mdss_rotator_get_dt_vreg_data(&pdev->dev, &mgr->module_power);
	if (ret)
		return ret;

	ret = mdss_rotator_clk_register(pdev, mgr,
		"iface_clk", MDSS_CLK_ROTATOR_AHB);
	if (ret)
		goto error;

	ret = mdss_rotator_clk_register(pdev, mgr,
		"rot_core_clk", MDSS_CLK_ROTATOR_CORE);
	if (ret)
		goto error;

	ret = mdss_rotator_bus_scale_register(mgr);
	if (ret)
		goto error;

	return 0;
error:
	mdss_rotator_put_dt_vreg_data(&pdev->dev, &mgr->module_power);
	return ret;
}

static int mdss_rotator_probe(struct platform_device *pdev)
{
	int ret;

	rot_mgr = devm_kzalloc(&pdev->dev, sizeof(struct mdss_rot_mgr),
		GFP_KERNEL);
	if (!rot_mgr) {
		pr_err("fail to allocate memory\n");
		return -ENOMEM;
	}

	rot_mgr->pdev = pdev;
	ret = mdss_rotator_parse_dt(rot_mgr, pdev);
	if (ret) {
		pr_err("fail to parse the dt\n");
		goto error_parse_dt;
	}

	mutex_init(&rot_mgr->lock);
	mutex_init(&rot_mgr->clk_lock);
	mutex_init(&rot_mgr->bus_lock);
	atomic_set(&rot_mgr->device_suspended, 0);
	ret = mdss_rotator_init_queue(rot_mgr);
	if (ret) {
		pr_err("fail to init queue\n");
		goto error_get_dev_num;
	}

	mutex_init(&rot_mgr->file_lock);
	INIT_LIST_HEAD(&rot_mgr->file_list);

	platform_set_drvdata(pdev, rot_mgr);

	ret = alloc_chrdev_region(&rot_mgr->dev_num, 0, 1, DRIVER_NAME);
	if (ret  < 0) {
		pr_err("alloc_chrdev_region failed ret = %d\n", ret);
		goto error_get_dev_num;
	}

	rot_mgr->class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(rot_mgr->class)) {
		ret = PTR_ERR(rot_mgr->class);
		pr_err("couldn't create class rc = %d\n", ret);
		goto error_class_create;
	}

	rot_mgr->device = device_create(rot_mgr->class, NULL,
		rot_mgr->dev_num, NULL, DRIVER_NAME);
	if (IS_ERR(rot_mgr->device)) {
		ret = PTR_ERR(rot_mgr->device);
		pr_err("device_create failed %d\n", ret);
		goto error_class_device_create;
	}

	cdev_init(&rot_mgr->cdev, &mdss_rotator_fops);
	ret = cdev_add(&rot_mgr->cdev,
			MKDEV(MAJOR(rot_mgr->dev_num), 0), 1);
	if (ret < 0) {
		pr_err("cdev_add failed %d\n", ret);
		goto error_cdev_add;
	}

	ret = sysfs_create_group(&rot_mgr->device->kobj,
			&mdss_rotator_fs_attr_group);
	if (ret)
		pr_err("unable to register rotator sysfs nodes\n");

	ret = mdss_rotator_res_init(pdev, rot_mgr);
	if (ret < 0) {
		pr_err("res_init failed %d\n", ret);
		goto error_res_init;
	}
	return 0;

error_res_init:
	cdev_del(&rot_mgr->cdev);
error_cdev_add:
	device_destroy(rot_mgr->class, rot_mgr->dev_num);
error_class_device_create:
	class_destroy(rot_mgr->class);
error_class_create:
	unregister_chrdev_region(rot_mgr->dev_num, 1);
error_get_dev_num:
	mdss_rotator_deinit_queue(rot_mgr);
error_parse_dt:
	devm_kfree(&pdev->dev, rot_mgr);
	rot_mgr = NULL;
	return ret;
}

static int mdss_rotator_remove(struct platform_device *dev)
{
	struct mdss_rot_mgr *mgr;

	mgr = (struct mdss_rot_mgr *)platform_get_drvdata(dev);
	if (!mgr)
		return -ENODEV;

	sysfs_remove_group(&rot_mgr->device->kobj, &mdss_rotator_fs_attr_group);

	mdss_rotator_release_all(mgr);

	mdss_rotator_put_dt_vreg_data(&dev->dev, &mgr->module_power);
	mdss_rotator_bus_scale_unregister(mgr);
	cdev_del(&rot_mgr->cdev);
	device_destroy(rot_mgr->class, rot_mgr->dev_num);
	class_destroy(rot_mgr->class);
	unregister_chrdev_region(rot_mgr->dev_num, 1);

	mdss_rotator_deinit_queue(rot_mgr);
	devm_kfree(&dev->dev, rot_mgr);
	rot_mgr = NULL;
	return 0;
}

#if defined(CONFIG_PM)
static int mdss_rotator_suspend(struct platform_device *dev, pm_message_t state)
{
	struct mdss_rot_mgr *mgr;

	mgr = (struct mdss_rot_mgr *)platform_get_drvdata(dev);
	if (!mgr)
		return -ENODEV;

	atomic_inc(&mgr->device_suspended);
	mdss_rotator_release_all(mgr);
	return 0;
}

static int mdss_rotator_resume(struct platform_device *dev)
{
	struct mdss_rot_mgr *mgr;

	mgr = (struct mdss_rot_mgr *)platform_get_drvdata(dev);
	if (!mgr)
		return -ENODEV;

	atomic_dec(&mgr->device_suspended);
	return 0;
}
#endif

static const struct of_device_id mdss_rotator_dt_match[] = {
	{ .compatible = "qcom,mdss_rotator",},
	{}
};

MODULE_DEVICE_TABLE(of, mdss_rotator_dt_match);

static struct platform_driver mdss_rotator_driver = {
	.probe = mdss_rotator_probe,
	.remove = mdss_rotator_remove,
#if defined(CONFIG_PM)
	.suspend = mdss_rotator_suspend,
	.resume = mdss_rotator_resume,
#endif
	.driver = {
		.name = "mdss_rotator",
		.of_match_table = mdss_rotator_dt_match,
		.pm = NULL,
	}
};

static int __init mdss_rotator_init(void)
{
	return platform_driver_register(&mdss_rotator_driver);
}

static void __exit mdss_rotator_exit(void)
{
	return platform_driver_unregister(&mdss_rotator_driver);
}

module_init(mdss_rotator_init);
module_exit(mdss_rotator_exit);

MODULE_DESCRIPTION("MSM Rotator driver");
MODULE_LICENSE("GPL v2");
