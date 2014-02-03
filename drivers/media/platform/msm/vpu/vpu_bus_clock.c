/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"VPU, %s: " fmt, __func__

#include <linux/types.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/msm-bus.h>

#include "vpu_bus_clock.h"
#include "vpu_resources.h"


struct vpu_bus_ctrl {
	u32 bus_client;
	struct bus_load_tbl *btabl;
};

static struct vpu_bus_ctrl g_vpu_bus_ctrl;

int vpu_bus_init(struct vpu_platform_resources *res)
{
	struct vpu_bus_ctrl *ctrl;
	int rc = 0;

	if (!res || res->bus_table.count == 0)
		return -EINVAL;

	ctrl = &g_vpu_bus_ctrl;
	ctrl->btabl = &res->bus_table;
	if (ctrl->bus_client)
		return 0;

	ctrl->bus_client = msm_bus_scale_register_client(&res->bus_pdata);
	if (!ctrl->bus_client) {
		pr_err("Failed to register bus scale client\n");
		goto err_init_bus;
	}

	return rc;

err_init_bus:
	vpu_bus_deinit();
	return -EINVAL;
}

void vpu_bus_deinit(void)
{
	struct vpu_bus_ctrl *ctrl = &g_vpu_bus_ctrl;

	if (ctrl->bus_client) {
		msm_bus_scale_unregister_client(ctrl->bus_client);
		ctrl->bus_client = 0;
	}
}

static int __get_bus_vector(struct vpu_bus_ctrl *ctrl, u32 load)
{
	int i, j;
	int num_rows = ctrl->btabl ? ctrl->btabl->count : 0;

	if (num_rows <= 1)
		return 0;

	for (i = 0; i < num_rows; i++) {
		if (load <= ctrl->btabl->loads[i])
			break;
	}

	j = (i < num_rows) ? i : num_rows - 1;

	pr_debug("Required bus = %d\n", j);
	return j;
}

int vpu_bus_vote(void)
{
	int rc = 0;
	u32 handle = 0;
	struct vpu_bus_ctrl *ctrl = &g_vpu_bus_ctrl;

	handle = ctrl->bus_client;
	if (handle) {
		/* vote for first non-zero bandwidth */
		rc = msm_bus_scale_client_update_request(handle, 1);
		if (rc)
			pr_err("Failed to vote bus: %d\n", rc);
	}

	return rc;
}

int vpu_bus_unvote(void)
{
	int rc = 0;
	u32 handle = 0;
	struct vpu_bus_ctrl *ctrl = &g_vpu_bus_ctrl;

	handle = ctrl->bus_client;
	if (handle) {
		/* vote for zero bandwidth */
		rc = msm_bus_scale_client_update_request(handle, 0);
		if (rc)
			pr_err("Failed to unvote bus: %d\n", rc);
	}

	return rc;
}

int vpu_bus_scale(u32 load)
{
	int rc = 0;
	u32 handle = 0;
	struct vpu_bus_ctrl *ctrl = &g_vpu_bus_ctrl;

	handle = ctrl->bus_client;
	if (handle) {
		rc = msm_bus_scale_client_update_request(
				handle, __get_bus_vector(ctrl, load));
		if (rc)
			pr_err("Failed to scale bus: %d\n", rc);
	}

	return rc;
}

/*
 * Here's the list of clks going into VPU:
 * clock name:			svs/nominal/turbo (MHz)
 * vpu_ahb_clk			40 / 80/ 80
 * vpu_axi_clk			150/333/466
 * vpu_bus_clk			40 / 80/ 80
 * vpu_maple_clk		200/400/400
 * vpu_vdp_clk			200/200/400
 * vpu_qdss_apb_clk
 * vpu_qdss_at_clk
 * vpu_qdss_tsctr_div8_clk
 * vpu_sleep_clk	qtimer when xo is disabled, watchdog
 * vpu_cxo_clk		qtimer in active mode

 * The vpu_ahb_clk, vpu_maple_axi_clk, and vpu_axi_clk will be
 * subject to DCD frequency changes.
 * There is a case where for power consumption we may wish to switch the
 * vpu_vdp_clk between 200MHz and 400MHz during runtime to optimize for
 * power consumption
 */

#define NOT_USED(clk_ctrl, i) (!( \
			((clk_ctrl)->clock[i]->flag & CLOCK_PRESENT) && \
			((clk_ctrl)->clock[i]->flag & (clk_ctrl)->mask)))
#define NOT_IN_GROUP(clk_ctrl, i, clk_group) (!( \
			((clk_ctrl)->clock[i]->flag & (clk_group))))

#define CLOCK_IS_SCALABLE(clk_ctrl, i) \
			((clk_ctrl)->clock[i]->flag & CLOCK_SCALABLE)

/*
 * Note: there is no lock in this block
 * It is caller's responsibility to serialize the calls
 */
struct vpu_clk_control {

	/* svs, nominal, turbo, dynamic(default) */
	u32 mode;

	struct vpu_clock *clock[VPU_MAX_CLKS];

	u32 mask;
};

void *vpu_clock_init(struct vpu_platform_resources *res)
{
	int i;
	struct vpu_clock *cl;
	struct vpu_clk_control *clk_ctrl;

	if (!res)
		return NULL;

	clk_ctrl = (struct vpu_clk_control *)
			kzalloc(sizeof(struct vpu_clk_control), GFP_KERNEL);
	if (!clk_ctrl) {
		pr_err("failed to allocate clock ctrl block\n");
		return NULL;
	}

	for (i = 0; i < VPU_MAX_CLKS; i++)
		clk_ctrl->clock[i] = &res->clock[i];

	/* mask allowing to only enable clocks from certain groups of clocks */
	clk_ctrl->mask = CLOCK_CORE;

	/* setup the clock handles */
	for (i = 0; i < VPU_MAX_CLKS; i++) {
		if (NOT_USED(clk_ctrl, i))
			continue;

		cl = clk_ctrl->clock[i];

		if (CLOCK_IS_SCALABLE(clk_ctrl, i) && !cl->pwr_frequencies) {
			pr_err("%s pwr frequencies unknown\n", cl->name);
			goto fail_init_clocks;
		}

		cl->clk = devm_clk_get(&res->pdev->dev, cl->name);
		if (IS_ERR_OR_NULL(cl->clk)) {
			pr_err("Failed to get clock: %s\n", cl->name);
			cl->clk = NULL;
			goto fail_init_clocks;
		}
		cl->status = 0;
	}

	return clk_ctrl;

fail_init_clocks:
	vpu_clock_deinit((void *)clk_ctrl);
	kfree(clk_ctrl);
	return NULL;
}

void vpu_clock_deinit(void *clkh)
{
	int i;
	struct vpu_clock *cl;
	struct vpu_clk_control *clk_ctr = (struct vpu_clk_control *)clkh;

	if (!clk_ctr) {
		pr_err("Invalid param\n");
		return;
	}

	for (i = 0; i < VPU_MAX_CLKS; i++) {
		if (NOT_USED(clk_ctr, i))
			continue;
		cl = clk_ctr->clock[i];
		if (cl->status) {
			clk_disable_unprepare(cl->clk);
			cl->status = 0;
		}
		if (cl->clk) {
			clk_put(cl->clk);
			cl->clk = NULL;
		}
	}

	kfree(clk_ctr);
}

/*
 * vpu_clock_enable() - enable a group of clocks
 *
 * @clkh:		clock handler
 * @clk_group:	see vpu_clock_flag (group section)
 *
 */
int vpu_clock_enable(void *clkh, u32 clk_group)
{
	struct vpu_clock *cl;
	struct vpu_clk_control *clk_ctr = (struct vpu_clk_control *)clkh;
	int i = 0;
	int rc = 0;

	if (!clk_ctr) {
		pr_err("Invalid param: %p\n", clk_ctr);
		return -EINVAL;
	}

	clk_ctr->mode = VPU_POWER_DYNAMIC;

	for (i = 0; i < VPU_MAX_CLKS; i++) {
		if (NOT_USED(clk_ctr, i) ||
				NOT_IN_GROUP(clk_ctr, i, clk_group))
			continue;

		cl = clk_ctr->clock[i];

		if (cl->status == 0) {
			if (CLOCK_IS_SCALABLE(clk_ctr, i)) {
				cl->dynamic_freq =
					cl->pwr_frequencies[VPU_POWER_SVS];

				pr_debug("clock %s set at %dHz\n", cl->name,
							   cl->dynamic_freq);
				rc = clk_set_rate(cl->clk, cl->dynamic_freq);
				if (rc) {
					pr_err("Failed to set rate for %s\n",
						cl->name);
					goto fail_clk_enable;
				}
			}

			rc = clk_prepare_enable(cl->clk);
			if (rc) {
				pr_err("Failed to enable clock %s (err %d)\n",
						cl->name, rc);
				goto fail_clk_enable;
			} else {
				pr_debug("%s prepare_enabled\n", cl->name);
				cl->status = 1;
			}
		}
	}

	return rc;

fail_clk_enable:
	vpu_clock_disable(clkh, clk_group);
	return rc;
}

/*
 * vpu_clock_disable() - disable a group of clocks
 *
 * @clkh:		clock handler
 * @clk_group:	see vpu_clock_flag (group section)
 *
 */
void vpu_clock_disable(void *clkh, u32 clk_group)
{
	int i;
	struct vpu_clock *cl;
	struct vpu_clk_control *clk_ctr = (struct vpu_clk_control *)clkh;

	if (!clk_ctr) {
		pr_err("Invalid param: %p\n", clk_ctr);
		return;
	}

	for (i = 0; i < VPU_MAX_CLKS; i++) {
		if (NOT_USED(clk_ctr, i) ||
				NOT_IN_GROUP(clk_ctr, i, clk_group))
			continue;
		cl = clk_ctr->clock[i];
		if (cl->status) {
			clk_disable_unprepare(cl->clk);
			cl->status = 0;
		}
	}
}

int vpu_clock_scale(void *clkh, enum vpu_power_mode mode)
{
	struct vpu_clk_control *clk_ctr = (struct vpu_clk_control *)clkh;
	int i, rc = 0;

	if (!clk_ctr) {
		pr_err("Invalid param: %p\n", clk_ctr);
		return -EINVAL;
	}

	for (i = 0; i < VPU_MAX_CLKS; i++) {
		struct vpu_clock *cl = clk_ctr->clock[i];
		unsigned long freq;

		if (NOT_USED(clk_ctr, i) || !CLOCK_IS_SCALABLE(clk_ctr, i))
			continue;

		freq = cl->pwr_frequencies[mode];
		if (clk_ctr->mode == VPU_POWER_DYNAMIC) {
			pr_debug("clock %s set at %luHz\n", cl->name, freq);
			rc = clk_set_rate(cl->clk, freq);
			if (rc) {
				pr_err("clk_set_rate failed %s rate: %lu\n",
						cl->name, freq);
				break;
			}
		}
		cl->dynamic_freq = freq;
	}

	return rc;
}

int vpu_clock_gating_off(void *clkh)
{
	int i;
	struct vpu_clock *cl;
	struct vpu_clk_control *clk_ctr = (struct vpu_clk_control *)clkh;
	int rc = 0;

	if (!clk_ctr) {
		pr_err("Invalid param: %p\n", clk_ctr);
		return -EINVAL;
	}

	/* no change if in manual mode */
	if (clk_ctr->mode != VPU_POWER_DYNAMIC)
		return 0;

	for (i = 0; i < VPU_MAX_CLKS; i++) {
		if (NOT_USED(clk_ctr, i) || !CLOCK_IS_SCALABLE(clk_ctr, i))
			continue;

		cl = clk_ctr->clock[i];
		if (cl->status == 0) {
			rc = clk_enable(cl->clk);
			if (rc) {
				pr_err("Failed to enable %s\n", cl->name);
				break;
			} else {
				cl->status = 1;
				pr_debug("%s enabled\n", cl->name);
			}
		}
	}

	return rc;
}

void vpu_clock_gating_on(void *clkh)
{
	int i;
	struct vpu_clock *cl;
	struct vpu_clk_control *clk_ctr = (struct vpu_clk_control *)clkh;

	if (!clk_ctr) {
		pr_err("Invalid param: %p\n", clk_ctr);
		return;
	}

	/* no change if in manual mode */
	if (clk_ctr->mode != VPU_POWER_DYNAMIC)
		return;

	for (i = 0; i < VPU_MAX_CLKS; i++) {
		if (NOT_USED(clk_ctr, i) || !CLOCK_IS_SCALABLE(clk_ctr, i))
			continue;

		cl = clk_ctr->clock[i];
		if (cl->status) {
			clk_disable(cl->clk);
			cl->status = 0;
		}
	}

	return;
}

void vpu_clock_mode_set(void *clkh, enum vpu_power_mode mode)
{
	struct vpu_clk_control *clk_ctr = (struct vpu_clk_control *)clkh;
	int i, rc = 0;

	if (!clk_ctr || (mode > VPU_POWER_MAX))
		return;

	/* no need to do anything if no change */
	if (mode == clk_ctr->mode)
		return;

	if (mode <= VPU_POWER_DYNAMIC) {
		clk_ctr->mode = mode;
		for (i = 0; i < VPU_MAX_CLKS; i++) {
			struct vpu_clock *cl = clk_ctr->clock[i];
			unsigned long freq;

			if (NOT_USED(clk_ctr, i) ||
					!CLOCK_IS_SCALABLE(clk_ctr, i))
				continue;

			if (mode < VPU_POWER_DYNAMIC)
				freq = cl->pwr_frequencies[mode];
			else
				freq = cl->dynamic_freq;

			pr_debug("clock %s set at %luHz\n", cl->name, freq);
			rc = clk_set_rate(cl->clk, freq);

			if (rc)
				pr_err("clk_set_rate failed %s rate: %lu\n",
						cl->name, freq);
		}
	}
}

enum vpu_power_mode vpu_clock_mode_get(void *clkh)
{
	struct vpu_clk_control *clk_ctr = (struct vpu_clk_control *)clkh;

	if (!clk_ctr)
		return 0;
	else
		return clk_ctr->mode;
}

