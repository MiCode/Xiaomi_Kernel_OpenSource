/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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

#define pr_fmt(fmt) "scm-pas: " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/clk.h>

#include <mach/scm.h>
#include <mach/socinfo.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include "scm-pas.h"

#define PAS_INIT_IMAGE_CMD	1
#define PAS_AUTH_AND_RESET_CMD	5
#define PAS_SHUTDOWN_CMD	6
#define PAS_IS_SUPPORTED_CMD	7

int pas_init_image(enum pas_id id, const u8 *metadata, size_t size)
{
	int ret;
	struct pas_init_image_req {
		u32	proc;
		u32	image_addr;
	} request;
	u32 scm_ret = 0;
	/* Make memory physically contiguous */
	void *mdata_buf = kmemdup(metadata, size, GFP_KERNEL);

	if (!mdata_buf)
		return -ENOMEM;

	request.proc = id;
	request.image_addr = virt_to_phys(mdata_buf);

	ret = scm_call(SCM_SVC_PIL, PAS_INIT_IMAGE_CMD, &request,
			sizeof(request), &scm_ret, sizeof(scm_ret));
	kfree(mdata_buf);

	if (ret)
		return ret;
	return scm_ret;
}
EXPORT_SYMBOL(pas_init_image);

static struct msm_bus_paths scm_pas_bw_tbl[] = {
	{
		.vectors = (struct msm_bus_vectors[]){
			{
				.src = MSM_BUS_MASTER_SPS,
				.dst = MSM_BUS_SLAVE_EBI_CH0,
			},
		},
		.num_paths = 1,
	},
	{
		.vectors = (struct msm_bus_vectors[]){
			{
				.src = MSM_BUS_MASTER_SPS,
				.dst = MSM_BUS_SLAVE_EBI_CH0,
				.ib = 492 * 8 * 1000000UL,
				.ab = 492 * 8 *  100000UL,
			},
		},
		.num_paths = 1,
	},
};

static struct msm_bus_scale_pdata scm_pas_bus_pdata = {
	.usecase = scm_pas_bw_tbl,
	.num_usecases = ARRAY_SIZE(scm_pas_bw_tbl),
	.name = "scm_pas",
};

static uint32_t scm_perf_client;
static struct clk *scm_bus_clk;

static DEFINE_MUTEX(scm_pas_bw_mutex);
static int scm_pas_bw_count;

static int scm_pas_enable_bw(void)
{
	int ret = 0;

	if (!scm_perf_client || !scm_bus_clk)
		return -EINVAL;

	mutex_lock(&scm_pas_bw_mutex);
	if (!scm_pas_bw_count) {
		ret = msm_bus_scale_client_update_request(scm_perf_client, 1);
		if (ret) {
			pr_err("bandwidth request failed (%d)\n", ret);
		} else {
			ret = clk_enable(scm_bus_clk);
			if (ret)
				pr_err("clock enable failed\n");
		}
	}
	if (ret)
		msm_bus_scale_client_update_request(scm_perf_client, 0);
	else
		scm_pas_bw_count++;
	mutex_unlock(&scm_pas_bw_mutex);
	return ret;
}

static void scm_pas_disable_bw(void)
{
	mutex_lock(&scm_pas_bw_mutex);
	if (scm_pas_bw_count-- == 1) {
		msm_bus_scale_client_update_request(scm_perf_client, 0);
		clk_disable(scm_bus_clk);
	}
	mutex_unlock(&scm_pas_bw_mutex);
}

int pas_auth_and_reset(enum pas_id id)
{
	int ret, bus_ret;
	u32 proc = id, scm_ret = 0;

	bus_ret = scm_pas_enable_bw();
	ret = scm_call(SCM_SVC_PIL, PAS_AUTH_AND_RESET_CMD, &proc,
			sizeof(proc), &scm_ret, sizeof(scm_ret));
	if (ret)
		scm_ret = ret;
	if (!bus_ret)
		scm_pas_disable_bw();

	return scm_ret;
}
EXPORT_SYMBOL(pas_auth_and_reset);

int pas_shutdown(enum pas_id id)
{
	int ret;
	u32 proc = id, scm_ret = 0;

	ret = scm_call(SCM_SVC_PIL, PAS_SHUTDOWN_CMD, &proc, sizeof(proc),
			&scm_ret, sizeof(scm_ret));
	if (ret)
		return ret;

	return scm_ret;
}
EXPORT_SYMBOL(pas_shutdown);

static bool secure_pil = true;
module_param(secure_pil, bool, S_IRUGO);
MODULE_PARM_DESC(secure_pil, "Use secure PIL");

int pas_supported(enum pas_id id)
{
	int ret;
	u32 periph = id, ret_val = 0;

	if (!secure_pil)
		return 0;

	/*
	 * 8660 SCM doesn't support querying secure PIL support so just return
	 * true if not overridden on the command line.
	 */
	if (cpu_is_msm8x60())
		return 1;

	if (scm_is_call_available(SCM_SVC_PIL, PAS_IS_SUPPORTED_CMD) <= 0)
		return 0;

	ret = scm_call(SCM_SVC_PIL, PAS_IS_SUPPORTED_CMD, &periph,
			sizeof(periph), &ret_val, sizeof(ret_val));
	if (ret)
		return ret;

	return ret_val;
}
EXPORT_SYMBOL(pas_supported);

static int __init scm_pas_init(void)
{
	/* TODO: Remove once bus scaling driver is in place */
	if (!cpu_is_apq8064())
		scm_perf_client = msm_bus_scale_register_client(
				&scm_pas_bus_pdata);
	if (!scm_perf_client)
		pr_warn("unable to register bus client\n");
	scm_bus_clk = clk_get_sys("scm", "bus_clk");
	if (!IS_ERR(scm_bus_clk)) {
		clk_set_rate(scm_bus_clk, 64000000);
	} else {
		scm_bus_clk = NULL;
		pr_warn("unable to get bus clock\n");
	}
	return 0;
}
module_init(scm_pas_init);
