/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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
#include <linux/dma-mapping.h>

#include <asm/cacheflush.h>

#include <mach/scm.h>
#include <mach/socinfo.h>
#include <mach/msm_bus.h>
#include <mach/msm_bus_board.h>
#include "scm-pas.h"

#define PAS_INIT_IMAGE_CMD	1
#define PAS_MEM_SETUP_CMD	2
#define PAS_AUTH_AND_RESET_CMD	5
#define PAS_SHUTDOWN_CMD	6
#define PAS_IS_SUPPORTED_CMD	7

enum scm_clock_ids {
	BUS_CLK = 0,
	CORE_CLK,
	IFACE_CLK,
	CORE_CLK_SRC,
	NUM_CLKS
};

static const char * const scm_clock_names[NUM_CLKS] = {
	[BUS_CLK]      = "bus_clk",
	[CORE_CLK]     = "core_clk",
	[IFACE_CLK]    = "iface_clk",
	[CORE_CLK_SRC] = "core_clk_src",
};

static struct clk *scm_clocks[NUM_CLKS];

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

static DEFINE_MUTEX(scm_pas_bw_mutex);
static int scm_pas_bw_count;

static int scm_pas_enable_bw(void)
{
	int ret = 0, i;

	if (!scm_perf_client)
		return -EINVAL;

	mutex_lock(&scm_pas_bw_mutex);
	if (!scm_pas_bw_count) {
		ret = msm_bus_scale_client_update_request(scm_perf_client, 1);
		if (ret)
			goto err_bus;
		scm_pas_bw_count++;
	}
	for (i = 0; i < NUM_CLKS; i++)
		if (clk_prepare_enable(scm_clocks[i]))
			goto err_clk;

	mutex_unlock(&scm_pas_bw_mutex);
	return ret;

err_clk:
	pr_err("clk prepare_enable failed (%s)\n", scm_clock_names[i]);
	for (i--; i >= 0; i--)
		clk_disable_unprepare(scm_clocks[i]);

err_bus:
	pr_err("bandwidth request failed (%d)\n", ret);
	msm_bus_scale_client_update_request(scm_perf_client, 0);

	mutex_unlock(&scm_pas_bw_mutex);
	return ret;
}

static void scm_pas_disable_bw(void)
{
	int i;
	mutex_lock(&scm_pas_bw_mutex);
	if (scm_pas_bw_count-- == 1) {
		msm_bus_scale_client_update_request(scm_perf_client, 0);
	}
	for (i = NUM_CLKS - 1; i >= 0; i--)
		clk_disable_unprepare(scm_clocks[i]);

	mutex_unlock(&scm_pas_bw_mutex);
}

int pas_init_image(enum pas_id id, const u8 *metadata, size_t size)
{
	int ret;
	struct pas_init_image_req {
		u32	proc;
		u32	image_addr;
	} request;
	u32 scm_ret = 0;
	void *mdata_buf;
	dma_addr_t mdata_phys;
	DEFINE_DMA_ATTRS(attrs);

	ret = scm_pas_enable_bw();
	if (ret)
		return ret;

	dma_set_attr(DMA_ATTR_STRONGLY_ORDERED, &attrs);
	mdata_buf = dma_alloc_attrs(NULL, size, &mdata_phys, GFP_KERNEL,
					&attrs);
	if (!mdata_buf) {
		pr_err("Allocation for metadata failed.\n");
		return -ENOMEM;
	}

	memcpy(mdata_buf, metadata, size);

	request.proc = id;
	request.image_addr = mdata_phys;

	ret = scm_call(SCM_SVC_PIL, PAS_INIT_IMAGE_CMD, &request,
			sizeof(request), &scm_ret, sizeof(scm_ret));

	dma_free_attrs(NULL, size, mdata_buf, mdata_phys, &attrs);
	scm_pas_disable_bw();

	if (ret)
		return ret;
	return scm_ret;
}
EXPORT_SYMBOL(pas_init_image);

int pas_mem_setup(enum pas_id id, u32 start_addr, u32 len)
{
	int ret;
	struct pas_init_image_req {
		u32	proc;
		u32	start_addr;
		u32	len;
	} request;
	u32 scm_ret = 0;

	request.proc = id;
	request.start_addr = start_addr;
	request.len = len;

	ret = scm_call(SCM_SVC_PIL, PAS_MEM_SETUP_CMD, &request,
			sizeof(request), &scm_ret, sizeof(scm_ret));
	if (ret)
		return ret;
	return scm_ret;
}
EXPORT_SYMBOL(pas_mem_setup);

int pas_auth_and_reset(enum pas_id id)
{
	int ret;
	u32 proc = id, scm_ret = 0;

	ret = scm_pas_enable_bw();
	if (ret)
		return ret;

	ret = scm_call(SCM_SVC_PIL, PAS_AUTH_AND_RESET_CMD, &proc,
			sizeof(proc), &scm_ret, sizeof(scm_ret));
	if (ret)
		scm_ret = ret;

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

void scm_pas_init(enum msm_bus_fabric_master_type id)
{
	int i, rate;
	static int is_inited;

	if (is_inited)
		return;

	for (i = 0; i < NUM_CLKS; i++) {
		scm_clocks[i] = clk_get_sys("scm", scm_clock_names[i]);
		if (IS_ERR(scm_clocks[i]))
			scm_clocks[i] = NULL;
	}

	/* Fail silently if this clock is not supported */
	rate = clk_round_rate(scm_clocks[CORE_CLK_SRC], 1);
	clk_set_rate(scm_clocks[CORE_CLK_SRC], rate);

	scm_pas_bw_tbl[0].vectors[0].src = id;
	scm_pas_bw_tbl[1].vectors[0].src = id;

	clk_set_rate(scm_clocks[BUS_CLK], 64000000);

	scm_perf_client = msm_bus_scale_register_client(&scm_pas_bus_pdata);
	if (!scm_perf_client)
		pr_warn("unable to register bus client\n");

	is_inited = 1;
}
