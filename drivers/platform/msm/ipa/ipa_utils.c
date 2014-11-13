/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <net/ip.h>
#include <linux/genalloc.h>	/* gen_pool_alloc() */
#include <linux/io.h>
#include <linux/ratelimit.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include "ipa_i.h"

#define IPA_V1_CLK_RATE (92.31 * 1000 * 1000UL)
#define IPA_V1_1_CLK_RATE (100 * 1000 * 1000UL)
#define IPA_V2_0_CLK_RATE_LOW (75 * 1000 * 1000UL)
#define IPA_V2_0_CLK_RATE_HIGH (150 * 1000 * 1000UL)
#define IPA_V1_MAX_HOLB_TMR_VAL (512 - 1)
#define IPA_V2_0_MAX_HOLB_TMR_VAL (65536 - 1)
#define IPA_V2_5_MAX_HOLB_TMR_VAL (4294967296 - 1)

#define IPA_V2_0_BW_THRESHOLD_MBPS (800)

/* Max pipes + ICs for TAG process */
#define IPA_TAG_MAX_DESC (IPA_NUM_PIPES + 6)

#define IPA_TAG_SLEEP_MIN_USEC (1000)
#define IPA_TAG_SLEEP_MAX_USEC (2000)
#define IPA_FORCE_CLOSE_TAG_PROCESS_TIMEOUT (10 * HZ)
#define IPA_BCR_REG_VAL (0x001FFF7F)
#define IPA_AGGR_GRAN_MIN (1)
#define IPA_AGGR_GRAN_MAX (32)
#define IPA_EOT_COAL_GRAN_MIN (1)
#define IPA_EOT_COAL_GRAN_MAX (16)

static const int ipa_ofst_meq32[] = { IPA_OFFSET_MEQ32_0,
					IPA_OFFSET_MEQ32_1, -1 };
static const int ipa_ofst_meq128[] = { IPA_OFFSET_MEQ128_0,
					IPA_OFFSET_MEQ128_1, -1 };
static const int ipa_ihl_ofst_rng16[] = { IPA_IHL_OFFSET_RANGE16_0,
					IPA_IHL_OFFSET_RANGE16_1, -1 };
static const int ipa_ihl_ofst_meq32[] = { IPA_IHL_OFFSET_MEQ32_0,
					IPA_IHL_OFFSET_MEQ32_1, -1 };
#define IPA_1_1 (0)
#define IPA_2_0 (1)
static const int ep_mapping[2][IPA_CLIENT_MAX] = {
	[IPA_1_1][IPA_CLIENT_HSIC1_PROD]         = 19,
	[IPA_1_1][IPA_CLIENT_WLAN1_PROD]         = -1,
	[IPA_1_1][IPA_CLIENT_HSIC2_PROD]         = 12,
	[IPA_1_1][IPA_CLIENT_USB2_PROD]          = 12,
	[IPA_1_1][IPA_CLIENT_HSIC3_PROD]         = 13,
	[IPA_1_1][IPA_CLIENT_USB3_PROD]          = 13,
	[IPA_1_1][IPA_CLIENT_HSIC4_PROD]         =  0,
	[IPA_1_1][IPA_CLIENT_USB4_PROD]          =  0,
	[IPA_1_1][IPA_CLIENT_HSIC5_PROD]         = -1,
	[IPA_1_1][IPA_CLIENT_USB_PROD]           = 11,
	[IPA_1_1][IPA_CLIENT_A5_WLAN_AMPDU_PROD] = 15,
	[IPA_1_1][IPA_CLIENT_A2_EMBEDDED_PROD]   =  8,
	[IPA_1_1][IPA_CLIENT_A2_TETHERED_PROD]   =  6,
	[IPA_1_1][IPA_CLIENT_APPS_LAN_WAN_PROD]  =  2,
	[IPA_1_1][IPA_CLIENT_APPS_CMD_PROD]      =  1,
	[IPA_1_1][IPA_CLIENT_ODU_PROD]           = -1,
	[IPA_1_1][IPA_CLIENT_Q6_LAN_PROD]        =  5,
	[IPA_1_1][IPA_CLIENT_Q6_CMD_PROD]        = -1,

	[IPA_1_1][IPA_CLIENT_HSIC1_CONS]         = 14,
	[IPA_1_1][IPA_CLIENT_WLAN1_CONS]         = -1,
	[IPA_1_1][IPA_CLIENT_HSIC2_CONS]         = 16,
	[IPA_1_1][IPA_CLIENT_USB2_CONS]          = 16,
	[IPA_1_1][IPA_CLIENT_WLAN2_CONS]         = -1,
	[IPA_1_1][IPA_CLIENT_HSIC3_CONS]         = 17,
	[IPA_1_1][IPA_CLIENT_USB3_CONS]          = 17,
	[IPA_1_1][IPA_CLIENT_WLAN3_CONS]         = -1,
	[IPA_1_1][IPA_CLIENT_HSIC4_CONS]         = 18,
	[IPA_1_1][IPA_CLIENT_USB4_CONS]          = 18,
	[IPA_1_1][IPA_CLIENT_WLAN4_CONS]         = -1,
	[IPA_1_1][IPA_CLIENT_HSIC5_CONS]         = -1,
	[IPA_1_1][IPA_CLIENT_USB_CONS]           = 10,
	[IPA_1_1][IPA_CLIENT_USB_DPL_CONS]       = -1,
	[IPA_1_1][IPA_CLIENT_A2_EMBEDDED_CONS]   =  9,
	[IPA_1_1][IPA_CLIENT_A2_TETHERED_CONS]   =  7,
	[IPA_1_1][IPA_CLIENT_A5_LAN_WAN_CONS]    =  3,
	[IPA_1_1][IPA_CLIENT_APPS_LAN_CONS]      = -1,
	[IPA_1_1][IPA_CLIENT_APPS_WAN_CONS]      = -1,
	[IPA_1_1][IPA_CLIENT_ODU_EMB_CONS]       = -1,
	[IPA_1_1][IPA_CLIENT_ODU_TETH_CONS]      = -1,
	[IPA_1_1][IPA_CLIENT_Q6_LAN_CONS]        =  4,
	[IPA_1_1][IPA_CLIENT_Q6_WAN_CONS]        = -1,


	[IPA_2_0][IPA_CLIENT_HSIC1_PROD]         = 12,
	[IPA_2_0][IPA_CLIENT_WLAN1_PROD]         = 18,
	[IPA_2_0][IPA_CLIENT_HSIC2_PROD]         = -1,
	[IPA_2_0][IPA_CLIENT_USB2_PROD]          = 12,
	[IPA_2_0][IPA_CLIENT_HSIC3_PROD]         = -1,
	[IPA_2_0][IPA_CLIENT_USB3_PROD]          = 13,
	[IPA_2_0][IPA_CLIENT_HSIC4_PROD]         = -1,
	[IPA_2_0][IPA_CLIENT_USB4_PROD]          =  0,
	[IPA_2_0][IPA_CLIENT_HSIC5_PROD]         = -1,
	[IPA_2_0][IPA_CLIENT_USB_PROD]           = 11,
	[IPA_2_0][IPA_CLIENT_A5_WLAN_AMPDU_PROD] = -1,
	[IPA_2_0][IPA_CLIENT_A2_EMBEDDED_PROD]   = -1,
	[IPA_2_0][IPA_CLIENT_A2_TETHERED_PROD]   = -1,
	[IPA_2_0][IPA_CLIENT_APPS_LAN_WAN_PROD]  =  4,
	[IPA_2_0][IPA_CLIENT_APPS_CMD_PROD]      =  3,
	[IPA_2_0][IPA_CLIENT_ODU_PROD]           = 12,
	[IPA_2_0][IPA_CLIENT_Q6_LAN_PROD]        =  6,
	[IPA_2_0][IPA_CLIENT_Q6_CMD_PROD]        =  7,
	/* Only for test purpose */
	[IPA_2_0][IPA_CLIENT_TEST_PROD]          = 19,
	[IPA_2_0][IPA_CLIENT_TEST1_PROD]         = 19,
	[IPA_2_0][IPA_CLIENT_TEST2_PROD]         = 12,
	[IPA_2_0][IPA_CLIENT_TEST3_PROD]         = 13,
	[IPA_2_0][IPA_CLIENT_TEST4_PROD]         =  0,

	[IPA_2_0][IPA_CLIENT_HSIC1_CONS]         = 13,
	[IPA_2_0][IPA_CLIENT_WLAN1_CONS]         = 17,
	[IPA_2_0][IPA_CLIENT_HSIC2_CONS]         = -1,
	[IPA_2_0][IPA_CLIENT_USB2_CONS]          = -1,
	[IPA_2_0][IPA_CLIENT_WLAN2_CONS]         = 16,
	[IPA_2_0][IPA_CLIENT_HSIC3_CONS]         = -1,
	[IPA_2_0][IPA_CLIENT_USB3_CONS]          = -1,
	[IPA_2_0][IPA_CLIENT_WLAN3_CONS]         = 14,
	[IPA_2_0][IPA_CLIENT_HSIC4_CONS]         = -1,
	[IPA_2_0][IPA_CLIENT_USB4_CONS]          = -1,
	[IPA_2_0][IPA_CLIENT_WLAN4_CONS]         = 19,
	[IPA_2_0][IPA_CLIENT_HSIC5_CONS]         = -1,
	[IPA_2_0][IPA_CLIENT_USB_CONS]           = 15,
	[IPA_2_0][IPA_CLIENT_USB_DPL_CONS]       =  0,
	[IPA_2_0][IPA_CLIENT_A2_EMBEDDED_CONS]   = -1,
	[IPA_2_0][IPA_CLIENT_A2_TETHERED_CONS]   = -1,
	[IPA_2_0][IPA_CLIENT_A5_LAN_WAN_CONS]    = -1,
	[IPA_2_0][IPA_CLIENT_APPS_LAN_CONS]      =  2,
	[IPA_2_0][IPA_CLIENT_APPS_WAN_CONS]      =  5,
	[IPA_2_0][IPA_CLIENT_ODU_EMB_CONS]       = 13,
	[IPA_2_0][IPA_CLIENT_ODU_TETH_CONS]      =  1,
	[IPA_2_0][IPA_CLIENT_Q6_LAN_CONS]        =  8,
	[IPA_2_0][IPA_CLIENT_Q6_WAN_CONS]        =  9,
	[IPA_2_0][IPA_CLIENT_Q6_DUN_CONS]        = 10,
	/* Only for test purpose */
	[IPA_2_0][IPA_CLIENT_TEST_CONS]          = 14,
	[IPA_2_0][IPA_CLIENT_TEST1_CONS]         = 14,
	[IPA_2_0][IPA_CLIENT_TEST2_CONS]         = 16,
	[IPA_2_0][IPA_CLIENT_TEST3_CONS]         = 17,
	[IPA_2_0][IPA_CLIENT_TEST4_CONS]         = 18,
};

static struct msm_bus_vectors ipa_init_vectors_v1_1[]  = {
	{
		.src = MSM_BUS_MASTER_IPA,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
	{
		.src = MSM_BUS_MASTER_BAM_DMA,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
	{
		.src = MSM_BUS_MASTER_BAM_DMA,
		.dst = MSM_BUS_SLAVE_OCIMEM,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors ipa_init_vectors_v2_0[]  = {
	{
		.src = MSM_BUS_MASTER_IPA,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
	{
		.src = MSM_BUS_MASTER_IPA,
		.dst = MSM_BUS_SLAVE_OCIMEM,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors ipa_max_perf_vectors_v1_1[]  = {
	{
		.src = MSM_BUS_MASTER_IPA,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 50000000,
		.ib = 960000000,
	},
	{
		.src = MSM_BUS_MASTER_BAM_DMA,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 50000000,
		.ib = 960000000,
	},
	{
		.src = MSM_BUS_MASTER_BAM_DMA,
		.dst = MSM_BUS_SLAVE_OCIMEM,
		.ab = 50000000,
		.ib = 960000000,
	},
};

static struct msm_bus_vectors ipa_nominal_perf_vectors_v2_0[]  = {
	{
		.src = MSM_BUS_MASTER_IPA,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 100000000,
		.ib = 1300000000,
	},
	{
		.src = MSM_BUS_MASTER_IPA,
		.dst = MSM_BUS_SLAVE_OCIMEM,
		.ab = 100000000,
		.ib = 1300000000,
	},
};

static struct msm_bus_paths ipa_usecases_v1_1[]  = {
	{
		ARRAY_SIZE(ipa_init_vectors_v1_1),
		ipa_init_vectors_v1_1,
	},
	{
		ARRAY_SIZE(ipa_max_perf_vectors_v1_1),
		ipa_max_perf_vectors_v1_1,
	},
};

static struct msm_bus_paths ipa_usecases_v2_0[]  = {
	{
		ARRAY_SIZE(ipa_init_vectors_v2_0),
		ipa_init_vectors_v2_0,
	},
	{
		ARRAY_SIZE(ipa_nominal_perf_vectors_v2_0),
		ipa_nominal_perf_vectors_v2_0,
	},
};

static struct msm_bus_scale_pdata ipa_bus_client_pdata_v1_1 = {
	ipa_usecases_v1_1,
	ARRAY_SIZE(ipa_usecases_v1_1),
	.name = "ipa",
};

static struct msm_bus_scale_pdata ipa_bus_client_pdata_v2_0 = {
	ipa_usecases_v2_0,
	ARRAY_SIZE(ipa_usecases_v2_0),
	.name = "ipa",
};

void ipa_active_clients_lock(void)
{
	unsigned long flags;

	mutex_lock(&ipa_ctx->ipa_active_clients.mutex);
	spin_lock_irqsave(&ipa_ctx->ipa_active_clients.spinlock, flags);
	ipa_ctx->ipa_active_clients.mutex_locked = true;
	spin_unlock_irqrestore(&ipa_ctx->ipa_active_clients.spinlock, flags);
}

int ipa_active_clients_trylock(unsigned long *flags)
{
	spin_lock_irqsave(&ipa_ctx->ipa_active_clients.spinlock, *flags);
	if (ipa_ctx->ipa_active_clients.mutex_locked) {
		spin_unlock_irqrestore(&ipa_ctx->ipa_active_clients.spinlock,
					 *flags);
		return 0;
	}

	return 1;
}

void ipa_active_clients_trylock_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&ipa_ctx->ipa_active_clients.spinlock, *flags);
}

void ipa_active_clients_unlock(void)
{
	unsigned long flags;

	spin_lock_irqsave(&ipa_ctx->ipa_active_clients.spinlock, flags);
	ipa_ctx->ipa_active_clients.mutex_locked = false;
	spin_unlock_irqrestore(&ipa_ctx->ipa_active_clients.spinlock, flags);
	mutex_unlock(&ipa_ctx->ipa_active_clients.mutex);
}

/**
 * ipa_get_clients_from_rm_resource() - get IPA clients which are related to an
 * IPA_RM resource
 *
 * @resource: [IN] IPA Resource Manager resource
 * @clients: [OUT] Empty array which will contain the list of clients. The
 *         caller must initialize this array.
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa_get_clients_from_rm_resource(
	enum ipa_rm_resource_name resource,
	struct ipa_client_names *clients)
{
	int i = 0;

	if (resource < 0 ||
	    resource >= IPA_RM_RESOURCE_MAX ||
	    !clients) {
		IPAERR("Bad parameters\n");
		return -EINVAL;
	}

	switch (resource) {
	case IPA_RM_RESOURCE_USB_CONS:
		clients->names[i++] = IPA_CLIENT_USB_CONS;
		break;
	case IPA_RM_RESOURCE_HSIC_CONS:
		clients->names[i++] = IPA_CLIENT_HSIC1_CONS;
		break;
	case IPA_RM_RESOURCE_WLAN_CONS:
		clients->names[i++] = IPA_CLIENT_WLAN1_CONS;
		clients->names[i++] = IPA_CLIENT_WLAN2_CONS;
		clients->names[i++] = IPA_CLIENT_WLAN3_CONS;
		clients->names[i++] = IPA_CLIENT_WLAN4_CONS;
		break;
	case IPA_RM_RESOURCE_USB_PROD:
		clients->names[i++] = IPA_CLIENT_USB_PROD;
		break;
	case IPA_RM_RESOURCE_HSIC_PROD:
		clients->names[i++] = IPA_CLIENT_HSIC1_PROD;
		break;
	default:
		break;
	}
	clients->length = i;

	return 0;
}

/**
 * ipa_should_pipe_be_suspended() - returns true when the client's pipe should
 * be suspended during a power save scenario. False otherwise.
 *
 * @client: [IN] IPA client
 */
bool ipa_should_pipe_be_suspended(enum ipa_client_type client)
{
	struct ipa_ep_context *ep;
	int ipa_ep_idx;

	ipa_ep_idx = ipa_get_ep_mapping(client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client.\n");
		WARN_ON(1);
		return false;
	}

	ep = &ipa_ctx->ep[ipa_ep_idx];

	if (ep->keep_ipa_awake)
		return false;

	if (client == IPA_CLIENT_USB_CONS   ||
	    client == IPA_CLIENT_HSIC1_CONS ||
	    client == IPA_CLIENT_WLAN1_CONS ||
	    client == IPA_CLIENT_WLAN2_CONS ||
	    client == IPA_CLIENT_WLAN3_CONS ||
	    client == IPA_CLIENT_WLAN4_CONS)
		return true;

	return false;
}

/**
 * ipa_suspend_resource_sync() - suspend client endpoints related to the IPA_RM
 * resource and decrement active clients counter, which may result in clock
 * gating of IPA clocks.
 *
 * @resource: [IN] IPA Resource Manager resource
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa_suspend_resource_sync(enum ipa_rm_resource_name resource)
{
	struct ipa_client_names clients;
	int res;
	int index;
	struct ipa_ep_cfg_ctrl suspend;
	enum ipa_client_type client;
	int ipa_ep_idx;
	bool pipe_suspended = false;

	memset(&clients, 0, sizeof(clients));
	res = ipa_get_clients_from_rm_resource(resource, &clients);
	if (res) {
		IPAERR("Bad params.\n");
		return res;
	}

	for (index = 0; index < clients.length; index++) {
		client = clients.names[index];
		ipa_ep_idx = ipa_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			IPAERR("Invalid client.\n");
			res = -EINVAL;
			continue;
		}
		if (ipa_ctx->ep[ipa_ep_idx].client == client &&
		    ipa_should_pipe_be_suspended(client)) {
			if (ipa_ctx->ep[ipa_ep_idx].valid) {
				/* suspend endpoint */
				memset(&suspend, 0, sizeof(suspend));
				suspend.ipa_ep_suspend = true;
				ipa_cfg_ep_ctrl(ipa_ep_idx, &suspend);
				pipe_suspended = true;
		    }
		    ipa_ctx->resume_on_connect[client] = false;
		}
	}
	/* Sleep ~1 msec */
	if (pipe_suspended)
		usleep_range(1000, 2000);

	/* before gating IPA clocks do TAG process */
	ipa_ctx->tag_process_before_gating = true;
	ipa_dec_client_disable_clks();

	return 0;
}

/**
 * ipa_suspend_resource_no_block() - suspend client endpoints related to the
 * IPA_RM resource and decrement active clients counter. This function is
 * guaranteed to avoid sleeping.
 *
 * @resource: [IN] IPA Resource Manager resource
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa_suspend_resource_no_block(enum ipa_rm_resource_name resource)
{
	int res;
	struct ipa_client_names clients;
	int index;
	enum ipa_client_type client;
	struct ipa_ep_cfg_ctrl suspend;
	int ipa_ep_idx;
	unsigned long flags;

	if (ipa_active_clients_trylock(&flags) == 0)
		return -EPERM;
	if (ipa_ctx->ipa_active_clients.cnt == 1) {
		res = -EPERM;
		goto bail;
	}

	memset(&clients, 0, sizeof(clients));
	res = ipa_get_clients_from_rm_resource(resource, &clients);
	if (res) {
		IPAERR("ipa_get_clients_from_rm_resource() failed, name = %d.\n"
		       , resource);
		goto bail;
	}

	for (index = 0; index < clients.length; index++) {
		client = clients.names[index];
		ipa_ep_idx = ipa_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			IPAERR("Invalid client.\n");
			res = -EINVAL;
			continue;
		}
		if (ipa_ctx->ep[ipa_ep_idx].client == client &&
		    ipa_should_pipe_be_suspended(client)) {
			if (ipa_ctx->ep[ipa_ep_idx].valid) {
				/* suspend endpoint */
				memset(&suspend, 0, sizeof(suspend));
				suspend.ipa_ep_suspend = true;
				ipa_cfg_ep_ctrl(ipa_ep_idx, &suspend);
		    }
		    ipa_ctx->resume_on_connect[client] = false;
		}
	}

	if (res == 0) {
		ipa_ctx->ipa_active_clients.cnt--;
		IPADBG("active clients = %d\n",
		       ipa_ctx->ipa_active_clients.cnt);
	}
bail:
	ipa_active_clients_trylock_unlock(&flags);

	return res;
}

/**
 * ipa_resume_resource() - resume client endpoints related to the IPA_RM
 * resource.
 *
 * @resource: [IN] IPA Resource Manager resource
 *
 * Return codes: 0 on success, negative on failure.
 */
int ipa_resume_resource(enum ipa_rm_resource_name resource)
{

	struct ipa_client_names clients;
	int res;
	int index;
	struct ipa_ep_cfg_ctrl suspend;
	enum ipa_client_type client;
	int ipa_ep_idx;

	memset(&clients, 0, sizeof(clients));
	res = ipa_get_clients_from_rm_resource(resource, &clients);
	if (res) {
		IPAERR("ipa_get_clients_from_rm_resource() failed.\n");
		return res;
	}

	for (index = 0; index < clients.length; index++) {
		client = clients.names[index];
		ipa_ep_idx = ipa_get_ep_mapping(client);
		if (ipa_ep_idx == -1) {
			IPAERR("Invalid client.\n");
			res = -EINVAL;
			continue;
		}
		if (ipa_ctx->ep[ipa_ep_idx].client == client &&
		    ipa_should_pipe_be_suspended(client)) {
			if (ipa_ctx->ep[ipa_ep_idx].valid) {
				memset(&suspend, 0, sizeof(suspend));
				suspend.ipa_ep_suspend = false;
				ipa_cfg_ep_ctrl(ipa_ep_idx, &suspend);
			}
			/*
			 * The related ep, will be resumed on connect
			 * while its resource is granted
			 */
			ipa_ctx->resume_on_connect[client] =
				true;
			IPADBG("%d will be resumed on connect.\n",
					client);
		}
	}

	return res;
}

/* read how much SRAM is available for SW use
 * In case of IPAv2.0 this will also supply an offset from
 * which we can start write
 */
void _ipa_sram_settings_read_v1_0(void)
{
	ipa_ctx->smem_restricted_bytes = 0;
	ipa_ctx->smem_sz = ipa_read_reg(ipa_ctx->mmio,
			IPA_SHARED_MEM_SIZE_OFST_v1_0);
	ipa_ctx->smem_reqd_sz = IPA_MEM_v1_RAM_END_OFST;
	ipa_ctx->hdr_tbl_lcl = 1;
	ipa_ctx->ip4_rt_tbl_lcl = 0;
	ipa_ctx->ip6_rt_tbl_lcl = 0;
	ipa_ctx->ip4_flt_tbl_lcl = 1;
	ipa_ctx->ip6_flt_tbl_lcl = 1;
}

void _ipa_sram_settings_read_v1_1(void)
{
	ipa_ctx->smem_restricted_bytes = 0;
	ipa_ctx->smem_sz = ipa_read_reg(ipa_ctx->mmio,
			IPA_SHARED_MEM_SIZE_OFST_v1_1);
	ipa_ctx->smem_reqd_sz = IPA_MEM_v1_RAM_END_OFST;
	ipa_ctx->hdr_tbl_lcl = 1;
	ipa_ctx->ip4_rt_tbl_lcl = 0;
	ipa_ctx->ip6_rt_tbl_lcl = 0;
	ipa_ctx->ip4_flt_tbl_lcl = 1;
	ipa_ctx->ip6_flt_tbl_lcl = 1;
}

void _ipa_sram_settings_read_v2_0(void)
{
	ipa_ctx->smem_restricted_bytes = ipa_read_reg_field(ipa_ctx->mmio,
			IPA_SHARED_MEM_SIZE_OFST_v2_0,
			IPA_SHARED_MEM_SIZE_SHARED_MEM_BADDR_BMSK_v2_0,
			IPA_SHARED_MEM_SIZE_SHARED_MEM_BADDR_SHFT_v2_0);
	ipa_ctx->smem_sz = ipa_read_reg_field(ipa_ctx->mmio,
			IPA_SHARED_MEM_SIZE_OFST_v2_0,
			IPA_SHARED_MEM_SIZE_SHARED_MEM_SIZE_BMSK_v2_0,
			IPA_SHARED_MEM_SIZE_SHARED_MEM_SIZE_SHFT_v2_0);
	ipa_ctx->smem_reqd_sz = IPA_MEM_PART(end_ofst);
	ipa_ctx->hdr_tbl_lcl = 0;
	ipa_ctx->ip4_rt_tbl_lcl = 0;
	ipa_ctx->ip6_rt_tbl_lcl = 0;
	ipa_ctx->ip4_flt_tbl_lcl = 1;
	ipa_ctx->ip6_flt_tbl_lcl = 0;
}

void _ipa_sram_settings_read_v2_5(void)
{
	ipa_ctx->smem_restricted_bytes = ipa_read_reg_field(ipa_ctx->mmio,
		IPA_SHARED_MEM_SIZE_OFST_v2_0,
		IPA_SHARED_MEM_SIZE_SHARED_MEM_BADDR_BMSK_v2_0,
		IPA_SHARED_MEM_SIZE_SHARED_MEM_BADDR_SHFT_v2_0);
	ipa_ctx->smem_sz = ipa_read_reg_field(ipa_ctx->mmio,
		IPA_SHARED_MEM_SIZE_OFST_v2_0,
		IPA_SHARED_MEM_SIZE_SHARED_MEM_SIZE_BMSK_v2_0,
		IPA_SHARED_MEM_SIZE_SHARED_MEM_SIZE_SHFT_v2_0);
	ipa_ctx->smem_reqd_sz = IPA_MEM_PART(end_ofst);
	ipa_ctx->hdr_tbl_lcl = 0;
	ipa_ctx->hdr_proc_ctx_tbl_lcl = 1;

	/*
	 * when proc ctx table is located in internal memory,
	 * modem entries resides first.
	 */
	if (ipa_ctx->hdr_proc_ctx_tbl_lcl) {
		ipa_ctx->hdr_proc_ctx_tbl.start_offset =
			IPA_MEM_PART(modem_hdr_proc_ctx_size);
	}
	ipa_ctx->ip4_rt_tbl_lcl = 0;
	ipa_ctx->ip6_rt_tbl_lcl = 0;
	ipa_ctx->ip4_flt_tbl_lcl = 0;
	ipa_ctx->ip6_flt_tbl_lcl = 0;
}

void _ipa_cfg_route_v1_0(struct ipa_route *route)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, route->route_dis,
			IPA_ROUTE_ROUTE_DIS_SHFT,
			IPA_ROUTE_ROUTE_DIS_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_pipe,
			IPA_ROUTE_ROUTE_DEF_PIPE_SHFT,
			IPA_ROUTE_ROUTE_DEF_PIPE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_hdr_table,
			IPA_ROUTE_ROUTE_DEF_HDR_TABLE_SHFT,
			IPA_ROUTE_ROUTE_DEF_HDR_TABLE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_hdr_ofst,
			IPA_ROUTE_ROUTE_DEF_HDR_OFST_SHFT,
			IPA_ROUTE_ROUTE_DEF_HDR_OFST_BMSK);

	ipa_write_reg(ipa_ctx->mmio, IPA_ROUTE_OFST_v1_0, reg_val);
}

void _ipa_cfg_route_v1_1(struct ipa_route *route)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, route->route_dis,
			IPA_ROUTE_ROUTE_DIS_SHFT,
			IPA_ROUTE_ROUTE_DIS_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_pipe,
			IPA_ROUTE_ROUTE_DEF_PIPE_SHFT,
			IPA_ROUTE_ROUTE_DEF_PIPE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_hdr_table,
			IPA_ROUTE_ROUTE_DEF_HDR_TABLE_SHFT,
			IPA_ROUTE_ROUTE_DEF_HDR_TABLE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_hdr_ofst,
			IPA_ROUTE_ROUTE_DEF_HDR_OFST_SHFT,
			IPA_ROUTE_ROUTE_DEF_HDR_OFST_BMSK);

	ipa_write_reg(ipa_ctx->mmio, IPA_ROUTE_OFST_v1_1, reg_val);
}

void _ipa_cfg_route_v2_0(struct ipa_route *route)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, route->route_dis,
			IPA_ROUTE_ROUTE_DIS_SHFT,
			IPA_ROUTE_ROUTE_DIS_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_pipe,
			IPA_ROUTE_ROUTE_DEF_PIPE_SHFT,
			IPA_ROUTE_ROUTE_DEF_PIPE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_hdr_table,
			IPA_ROUTE_ROUTE_DEF_HDR_TABLE_SHFT,
			IPA_ROUTE_ROUTE_DEF_HDR_TABLE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_def_hdr_ofst,
			IPA_ROUTE_ROUTE_DEF_HDR_OFST_SHFT,
			IPA_ROUTE_ROUTE_DEF_HDR_OFST_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, route->route_frag_def_pipe,
			IPA_ROUTE_ROUTE_FRAG_DEF_PIPE_SHFT,
			IPA_ROUTE_ROUTE_FRAG_DEF_PIPE_BMSK);

	ipa_write_reg(ipa_ctx->mmio, IPA_ROUTE_OFST_v1_1, reg_val);
}

/**
 * ipa_cfg_route() - configure IPA route
 * @route: IPA route
 *
 * Return codes:
 * 0: success
 */
int ipa_cfg_route(struct ipa_route *route)
{

	IPADBG("disable_route_block=%d, default_pipe=%d, default_hdr_tbl=%d\n",
		route->route_dis,
		route->route_def_pipe,
		route->route_def_hdr_table);
	IPADBG("default_hdr_ofst=%d, default_frag_pipe=%d\n",
		route->route_def_hdr_ofst,
		route->route_frag_def_pipe);

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_route(route);

	ipa_dec_client_disable_clks();

	return 0;
}

/**
 * ipa_cfg_filter() - configure filter
 * @disable: disable value
 *
 * Return codes:
 * 0: success
 */
int ipa_cfg_filter(u32 disable)
{
	u32 ipa_filter_ofst = IPA_FILTER_OFST_v1_0;

	if (ipa_ctx->ipa_hw_type != IPA_HW_v1_0)
		ipa_filter_ofst = IPA_FILTER_OFST_v1_1;
	ipa_inc_client_enable_clks();
	ipa_write_reg(ipa_ctx->mmio, ipa_filter_ofst,
			IPA_SETFIELD(!disable,
					IPA_FILTER_FILTER_EN_SHFT,
					IPA_FILTER_FILTER_EN_BMSK));
	ipa_dec_client_disable_clks();

	return 0;
}

/**
 * ipa_init_hw() - initialize HW
 *
 * Return codes:
 * 0: success
 */
int ipa_init_hw(void)
{
	u32 ipa_version = 0;

	/* do soft reset of IPA */
	ipa_write_reg(ipa_ctx->mmio, IPA_COMP_SW_RESET_OFST, 1);
	ipa_write_reg(ipa_ctx->mmio, IPA_COMP_SW_RESET_OFST, 0);

	/* enable IPA */
	ipa_write_reg(ipa_ctx->mmio, IPA_COMP_CFG_OFST, 1);

	/* Read IPA version and make sure we have access to the registers */
	ipa_version = ipa_read_reg(ipa_ctx->mmio, IPA_VERSION_OFST);
	if (ipa_version == 0)
		return -EFAULT;

	if (ipa_ctx->ipa_hw_type == IPA_HW_v2_5) {
		/* set ipa_bcr to 0xFFFFFFFF for using new IPA behavior */
		ipa_write_reg(ipa_ctx->mmio, IPA_BCR_OFST, IPA_BCR_REG_VAL);
	}
	return 0;
}

/**
 * ipa_get_ep_mapping() - provide endpoint mapping
 * @client: client type
 *
 * Return value: endpoint mapping
 */
int ipa_get_ep_mapping(enum ipa_client_type client)
{
	u8 hw_type_index = IPA_1_1;

	if (client >= IPA_CLIENT_MAX || client < 0) {
		IPAERR("Bad client number! client =%d\n", client);
		return -EINVAL;
	}

	if (ipa_ctx->ipa_hw_type == IPA_HW_v2_0 ||
		ipa_ctx->ipa_hw_type == IPA_HW_v2_5)
		hw_type_index = IPA_2_0;

	return ep_mapping[hw_type_index][client];
}
EXPORT_SYMBOL(ipa_get_ep_mapping);

/**
 * ipa_get_rm_resource_from_ep() - get the IPA_RM resource which is related to
 * the supplied pipe index.
 *
 * @pipe_idx:
 *
 * Return value: IPA_RM resource related to the pipe, -1 if a resource was not
 * found.
 */
enum ipa_rm_resource_name ipa_get_rm_resource_from_ep(int pipe_idx)
{
	int i;
	int j;
	enum ipa_client_type client;
	struct ipa_client_names clients;
	bool found = false;

	if (pipe_idx >= IPA_NUM_PIPES || pipe_idx < 0) {
		IPAERR("Bad pipe index!\n");
		return -EINVAL;
	}

	client = ipa_ctx->ep[pipe_idx].client;

	for (i = 0; i < IPA_RM_RESOURCE_MAX; i++) {
		memset(&clients, 0, sizeof(clients));
		ipa_get_clients_from_rm_resource(i, &clients);
		for (j = 0; j < clients.length; j++) {
			if (clients.names[j] == client) {
				found = true;
				break;
			}
		}
		if (found)
			break;
	}

	if (!found)
		return -EFAULT;

	return i;
}

/**
 * ipa_get_client_mapping() - provide client mapping
 * @pipe_idx: IPA end-point number
 *
 * Return value: client mapping
 */
enum ipa_client_type ipa_get_client_mapping(int pipe_idx)
{
	if (pipe_idx >= IPA_NUM_PIPES || pipe_idx < 0) {
		IPAERR("Bad pipe index!\n");
		return -EINVAL;
	}

	return ipa_ctx->ep[pipe_idx].client;
}

/**
 * ipa_write_32() - convert 32 bit value to byte array
 * @w: 32 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa_write_32(u32 w, u8 *dest)
{
	*dest++ = (u8)((w) & 0xFF);
	*dest++ = (u8)((w >> 8) & 0xFF);
	*dest++ = (u8)((w >> 16) & 0xFF);
	*dest++ = (u8)((w >> 24) & 0xFF);

	return dest;
}

/**
 * ipa_write_16() - convert 16 bit value to byte array
 * @hw: 16 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa_write_16(u16 hw, u8 *dest)
{
	*dest++ = (u8)((hw) & 0xFF);
	*dest++ = (u8)((hw >> 8) & 0xFF);

	return dest;
}

/**
 * ipa_write_8() - convert 8 bit value to byte array
 * @hw: 8 bit integer
 * @dest: byte array
 *
 * Return value: converted value
 */
u8 *ipa_write_8(u8 b, u8 *dest)
{
	*dest++ = (b) & 0xFF;

	return dest;
}

/**
 * ipa_pad_to_32() - pad byte array to 32 bit value
 * @dest: byte array
 *
 * Return value: padded value
 */
u8 *ipa_pad_to_32(u8 *dest)
{
	int i = (long)dest & 0x3;
	int j;

	if (i)
		for (j = 0; j < (4 - i); j++)
			*dest++ = 0;

	return dest;
}

void ipa_generate_mac_addr_hw_rule(u8 **buf, u8 hdr_mac_addr_offset,
	const uint8_t mac_addr_mask[ETH_ALEN],
	const uint8_t mac_addr[ETH_ALEN])
{
	*buf = ipa_write_8(hdr_mac_addr_offset, *buf);

	/* MAC addr mask copied as little endian each 4 bytes */
	*buf = ipa_write_8(mac_addr_mask[3], *buf);
	*buf = ipa_write_8(mac_addr_mask[2], *buf);
	*buf = ipa_write_8(mac_addr_mask[1], *buf);
	*buf = ipa_write_8(mac_addr_mask[0], *buf);
	*buf = ipa_write_16(0, *buf);
	*buf = ipa_write_8(mac_addr_mask[5], *buf);
	*buf = ipa_write_8(mac_addr_mask[4], *buf);
	*buf = ipa_write_32(0, *buf);
	*buf = ipa_write_32(0, *buf);

	/* MAC addr copied as little endian each 4 bytes */
	*buf = ipa_write_8(mac_addr[3], *buf);
	*buf = ipa_write_8(mac_addr[2], *buf);
	*buf = ipa_write_8(mac_addr[1], *buf);
	*buf = ipa_write_8(mac_addr[0], *buf);
	*buf = ipa_write_16(0, *buf);
	*buf = ipa_write_8(mac_addr[5], *buf);
	*buf = ipa_write_8(mac_addr[4], *buf);
	*buf = ipa_write_32(0, *buf);
	*buf = ipa_write_32(0, *buf);
	*buf = ipa_pad_to_32(*buf);
}

/**
 * ipa_generate_hw_rule() - generate HW rule
 * @ip: IP address type
 * @attrib: IPA rule attribute
 * @buf: output buffer
 * @en_rule: rule
 *
 * Return codes:
 * 0: success
 * -EPERM: wrong input
 */
int ipa_generate_hw_rule(enum ipa_ip_type ip,
	const struct ipa_rule_attrib *attrib, u8 **buf, u16 *en_rule)
{
	u8 ofst_meq32 = 0;
	u8 ihl_ofst_rng16 = 0;
	u8 ihl_ofst_meq32 = 0;
	u8 ofst_meq128 = 0;

	if (ip == IPA_IP_v4) {

		/* error check */
		if (attrib->attrib_mask & IPA_FLT_NEXT_HDR ||
		    attrib->attrib_mask & IPA_FLT_TC || attrib->attrib_mask &
		    IPA_FLT_FLOW_LABEL) {
			IPAERR("v6 attrib's specified for v4 rule\n");
			return -EPERM;
		}

		if (attrib->attrib_mask & IPA_FLT_TOS) {
			*en_rule |= IPA_TOS_EQ;
			*buf = ipa_write_8(attrib->u.v4.tos, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			/* 0 => offset of TOS in v4 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_32((attrib->tos_mask << 16), *buf);
			*buf = ipa_write_32((attrib->tos_value << 16), *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_PROTOCOL) {
			*en_rule |= IPA_PROTOCOL_EQ;
			*buf = ipa_write_8(attrib->u.v4.protocol, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			/* 12 => offset of src ip in v4 header */
			*buf = ipa_write_8(12, *buf);
			*buf = ipa_write_32(attrib->u.v4.src_addr_mask, *buf);
			*buf = ipa_write_32(attrib->u.v4.src_addr, *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			/* 16 => offset of dst ip in v4 header */
			*buf = ipa_write_8(16, *buf);
			*buf = ipa_write_32(attrib->u.v4.dst_addr_mask, *buf);
			*buf = ipa_write_32(attrib->u.v4.dst_addr, *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			/* -2 => offset of ether type in L2 hdr */
			*buf = ipa_write_8((u8)-2, *buf);
			*buf = ipa_write_16(0, *buf);
			*buf = ipa_write_16(htons(attrib->ether_type), *buf);
			*buf = ipa_write_16(0, *buf);
			*buf = ipa_write_16(htons(attrib->ether_type), *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			if (attrib->src_port_hi < attrib->src_port_lo) {
				IPAERR("bad src port range param\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 0  => offset of src port after v4 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_16(attrib->src_port_hi, *buf);
			*buf = ipa_write_16(attrib->src_port_lo, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			if (attrib->dst_port_hi < attrib->dst_port_lo) {
				IPAERR("bad dst port range param\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 2  => offset of dst port after v4 header */
			*buf = ipa_write_8(2, *buf);
			*buf = ipa_write_16(attrib->dst_port_hi, *buf);
			*buf = ipa_write_16(attrib->dst_port_lo, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_TYPE) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			/* 0  => offset of type after v4 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_32(0xFF, *buf);
			*buf = ipa_write_32(attrib->type, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_CODE) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			/* 1  => offset of code after v4 header */
			*buf = ipa_write_8(1, *buf);
			*buf = ipa_write_32(0xFF, *buf);
			*buf = ipa_write_32(attrib->code, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SPI) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			/* 0  => offset of SPI after v4 header FIXME */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_32(0xFFFFFFFF, *buf);
			*buf = ipa_write_32(attrib->spi, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 0  => offset of src port after v4 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_16(attrib->src_port, *buf);
			*buf = ipa_write_16(attrib->src_port, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 2  => offset of dst port after v4 header */
			*buf = ipa_write_8(2, *buf);
			*buf = ipa_write_16(attrib->dst_port, *buf);
			*buf = ipa_write_16(attrib->dst_port, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -14 => offset of dst mac addr in Ethernet II hdr */
			ipa_generate_mac_addr_hw_rule(
				buf,
				-14,
				attrib->dst_mac_addr_mask,
				attrib->dst_mac_addr);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -8 => offset of src mac addr in Ethernet II hdr */
			ipa_generate_mac_addr_hw_rule(
				buf,
				-8,
				attrib->src_mac_addr_mask,
				attrib->src_mac_addr);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -22 => offset of dst mac addr in 802.3 hdr */
			ipa_generate_mac_addr_hw_rule(
				buf,
				-22,
				attrib->dst_mac_addr_mask,
				attrib->dst_mac_addr);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -16 => offset of src mac addr in 802.3 hdr */
			ipa_generate_mac_addr_hw_rule(
				buf,
				-16,
				attrib->src_mac_addr_mask,
				attrib->src_mac_addr);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_META_DATA) {
			*en_rule |= IPA_METADATA_COMPARE;
			*buf = ipa_write_8(0, *buf);    /* offset, reserved */
			*buf = ipa_write_32(attrib->meta_data_mask, *buf);
			*buf = ipa_write_32(attrib->meta_data, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_FRAGMENT) {
			*en_rule |= IPA_IS_FRAG;
			*buf = ipa_pad_to_32(*buf);
		}
	} else if (ip == IPA_IP_v6) {

		/* v6 code below assumes no extension headers TODO: fix this */

		/* error check */
		if (attrib->attrib_mask & IPA_FLT_TOS ||
		    attrib->attrib_mask & IPA_FLT_PROTOCOL) {
			IPAERR("v4 attrib's specified for v6 rule\n");
			return -EPERM;
		}

		if (attrib->attrib_mask & IPA_FLT_NEXT_HDR) {
			*en_rule |= IPA_PROTOCOL_EQ;
			*buf = ipa_write_8(attrib->u.v6.next_hdr, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			/* -2 => offset of ether type in L2 hdr */
			*buf = ipa_write_8((u8)-2, *buf);
			*buf = ipa_write_16(0, *buf);
			*buf = ipa_write_16(htons(attrib->ether_type), *buf);
			*buf = ipa_write_16(0, *buf);
			*buf = ipa_write_16(htons(attrib->ether_type), *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_TYPE) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			/* 0  => offset of type after v6 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_32(0xFF, *buf);
			*buf = ipa_write_32(attrib->type, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_CODE) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			/* 1  => offset of code after v6 header */
			*buf = ipa_write_8(1, *buf);
			*buf = ipa_write_32(0xFF, *buf);
			*buf = ipa_write_32(attrib->code, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SPI) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			/* 0  => offset of SPI after v6 header FIXME */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_32(0xFFFFFFFF, *buf);
			*buf = ipa_write_32(attrib->spi, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 0  => offset of src port after v6 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_16(attrib->src_port, *buf);
			*buf = ipa_write_16(attrib->src_port, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 2  => offset of dst port after v6 header */
			*buf = ipa_write_8(2, *buf);
			*buf = ipa_write_16(attrib->dst_port, *buf);
			*buf = ipa_write_16(attrib->dst_port, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			if (attrib->src_port_hi < attrib->src_port_lo) {
				IPAERR("bad src port range param\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 0  => offset of src port after v6 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_16(attrib->src_port_hi, *buf);
			*buf = ipa_write_16(attrib->src_port_lo, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			if (attrib->dst_port_hi < attrib->dst_port_lo) {
				IPAERR("bad dst port range param\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			/* 2  => offset of dst port after v6 header */
			*buf = ipa_write_8(2, *buf);
			*buf = ipa_write_16(attrib->dst_port_hi, *buf);
			*buf = ipa_write_16(attrib->dst_port_lo, *buf);
			*buf = ipa_pad_to_32(*buf);
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];
			/* 8 => offset of src ip in v6 header */
			*buf = ipa_write_8(8, *buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr_mask[0],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr_mask[1],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr_mask[2],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr_mask[3],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr[0], *buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr[1], *buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr[2], *buf);
			*buf = ipa_write_32(attrib->u.v6.src_addr[3], *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];
			/* 24 => offset of dst ip in v6 header */
			*buf = ipa_write_8(24, *buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr_mask[0],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr_mask[1],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr_mask[2],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr_mask[3],
					*buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr[0], *buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr[1], *buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr[2], *buf);
			*buf = ipa_write_32(attrib->u.v6.dst_addr[3], *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_TC) {
			*en_rule |= IPA_FLT_TC;
			*buf = ipa_write_8(attrib->u.v6.tc, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];
			/* 0 => offset of TOS in v6 header */
			*buf = ipa_write_8(0, *buf);
			*buf = ipa_write_32((attrib->tos_mask << 20), *buf);
			*buf = ipa_write_32(0, *buf);
			*buf = ipa_write_32(0, *buf);
			*buf = ipa_write_32(0, *buf);

			*buf = ipa_write_32((attrib->tos_value << 20), *buf);
			*buf = ipa_write_32(0, *buf);
			*buf = ipa_write_32(0, *buf);
			*buf = ipa_write_32(0, *buf);
			*buf = ipa_pad_to_32(*buf);
			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -14 => offset of dst mac addr in Ethernet II hdr */
			ipa_generate_mac_addr_hw_rule(
				buf,
				-14,
				attrib->dst_mac_addr_mask,
				attrib->dst_mac_addr);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -8 => offset of src mac addr in Ethernet II hdr */
			ipa_generate_mac_addr_hw_rule(
				buf,
				-8,
				attrib->src_mac_addr_mask,
				attrib->src_mac_addr);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -22 => offset of dst mac addr in 802.3 hdr */
			ipa_generate_mac_addr_hw_rule(
				buf,
				-22,
				attrib->dst_mac_addr_mask,
				attrib->dst_mac_addr);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -16 => offset of src mac addr in 802.3 hdr */
			ipa_generate_mac_addr_hw_rule(
				buf,
				-16,
				attrib->src_mac_addr_mask,
				attrib->src_mac_addr);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_FLOW_LABEL) {
			*en_rule |= IPA_FLT_FLOW_LABEL;
			 /* FIXME FL is only 20 bits */
			*buf = ipa_write_32(attrib->u.v6.flow_label, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_META_DATA) {
			*en_rule |= IPA_METADATA_COMPARE;
			*buf = ipa_write_8(0, *buf);    /* offset, reserved */
			*buf = ipa_write_32(attrib->meta_data_mask, *buf);
			*buf = ipa_write_32(attrib->meta_data, *buf);
			*buf = ipa_pad_to_32(*buf);
		}

		if (attrib->attrib_mask & IPA_FLT_FRAGMENT) {
			*en_rule |= IPA_IS_FRAG;
			*buf = ipa_pad_to_32(*buf);
		}
	} else {
		IPAERR("unsupported ip %d\n", ip);
		return -EPERM;
	}

	/*
	 * default "rule" means no attributes set -> map to
	 * OFFSET_MEQ32_0 with mask of 0 and val of 0 and offset 0
	 */
	if (attrib->attrib_mask == 0) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		*buf = ipa_write_8(0, *buf);    /* offset */
		*buf = ipa_write_32(0, *buf);   /* mask */
		*buf = ipa_write_32(0, *buf);   /* val */
		*buf = ipa_pad_to_32(*buf);
		ofst_meq32++;
	}

	return 0;
}

void ipa_generate_flt_mac_addr_eq(struct ipa_ipfltri_rule_eq *eq_atrb,
	u8 hdr_mac_addr_offset,	const uint8_t mac_addr_mask[ETH_ALEN],
	const uint8_t mac_addr[ETH_ALEN], u8 ofst_meq128)
{
	eq_atrb->offset_meq_128[ofst_meq128].offset = hdr_mac_addr_offset;
	eq_atrb->offset_meq_128[ofst_meq128].mask[0] = mac_addr_mask[3];
	eq_atrb->offset_meq_128[ofst_meq128].mask[1] = mac_addr_mask[2];
	eq_atrb->offset_meq_128[ofst_meq128].mask[2] = mac_addr_mask[1];
	eq_atrb->offset_meq_128[ofst_meq128].mask[3] = mac_addr_mask[0];
	eq_atrb->offset_meq_128[ofst_meq128].mask[4] = 0;
	eq_atrb->offset_meq_128[ofst_meq128].mask[5] = 0;
	eq_atrb->offset_meq_128[ofst_meq128].mask[6] = mac_addr_mask[5];
	eq_atrb->offset_meq_128[ofst_meq128].mask[7] = mac_addr_mask[4];
	memset(eq_atrb->offset_meq_128[ofst_meq128].mask + 8, 0, 8);
	eq_atrb->offset_meq_128[ofst_meq128].value[0] =	mac_addr[3];
	eq_atrb->offset_meq_128[ofst_meq128].value[1] =	mac_addr[2];
	eq_atrb->offset_meq_128[ofst_meq128].value[2] =	mac_addr[1];
	eq_atrb->offset_meq_128[ofst_meq128].value[3] =	mac_addr[0];
	eq_atrb->offset_meq_128[ofst_meq128].value[4] = 0;
	eq_atrb->offset_meq_128[ofst_meq128].value[5] = 0;
	eq_atrb->offset_meq_128[ofst_meq128].value[6] =	mac_addr[5];
	eq_atrb->offset_meq_128[ofst_meq128].value[7] =	mac_addr[4];
	memset(eq_atrb->offset_meq_128[ofst_meq128].value + 8, 0, 8);
}

int ipa_generate_flt_eq(enum ipa_ip_type ip,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_atrb)
{
	u8 ofst_meq32 = 0;
	u8 ihl_ofst_rng16 = 0;
	u8 ihl_ofst_meq32 = 0;
	u8 ofst_meq128 = 0;
	u16 eq_bitmap = 0;
	u16 *en_rule = &eq_bitmap;

	if (ip == IPA_IP_v4) {

		/* error check */
		if (attrib->attrib_mask & IPA_FLT_NEXT_HDR ||
		    attrib->attrib_mask & IPA_FLT_TC || attrib->attrib_mask &
		    IPA_FLT_FLOW_LABEL) {
			IPAERR("v6 attrib's specified for v4 rule\n");
			return -EPERM;
		}

		if (attrib->attrib_mask & IPA_FLT_TOS) {
			*en_rule |= IPA_TOS_EQ;
			eq_atrb->tos_eq_present = 1;
			eq_atrb->tos_eq = attrib->u.v4.tos;
		}

		if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			eq_atrb->offset_meq_32[ofst_meq32].offset = 0;
			eq_atrb->offset_meq_32[ofst_meq32].mask =
				attrib->tos_mask << 16;
			eq_atrb->offset_meq_32[ofst_meq32].value =
				attrib->tos_value << 16;
			ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_PROTOCOL) {
			*en_rule |= IPA_PROTOCOL_EQ;
			eq_atrb->protocol_eq_present = 1;
			eq_atrb->protocol_eq = attrib->u.v4.protocol;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			eq_atrb->offset_meq_32[ofst_meq32].offset = 12;
			eq_atrb->offset_meq_32[ofst_meq32].mask =
				attrib->u.v4.src_addr_mask;
			eq_atrb->offset_meq_32[ofst_meq32].value =
				attrib->u.v4.src_addr;
			ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			eq_atrb->offset_meq_32[ofst_meq32].offset = 16;
			eq_atrb->offset_meq_32[ofst_meq32].mask =
				attrib->u.v4.dst_addr_mask;
			eq_atrb->offset_meq_32[ofst_meq32].value =
				attrib->u.v4.dst_addr;
			ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			if (attrib->src_port_hi < attrib->src_port_lo) {
				IPAERR("bad src port range param\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 0;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
				= attrib->src_port_lo;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
				= attrib->src_port_hi;
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			if (attrib->dst_port_hi < attrib->dst_port_lo) {
				IPAERR("bad dst port range param\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 2;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
				= attrib->dst_port_lo;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
				= attrib->dst_port_hi;
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_TYPE) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 0;
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask = 0xFF;
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
				attrib->type;
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_CODE) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 1;
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask = 0xFF;
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
				attrib->code;
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SPI) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 0;
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask =
				0xFFFFFFFF;
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
				attrib->spi;
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 0;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
				= attrib->src_port;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
				= attrib->src_port;
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 2;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
				= attrib->dst_port;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
				= attrib->dst_port;
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_META_DATA) {
			*en_rule |= IPA_METADATA_COMPARE;
			eq_atrb->metadata_meq32_present = 1;
			eq_atrb->metadata_meq32.offset = 0;
			eq_atrb->metadata_meq32.mask = attrib->meta_data_mask;
			eq_atrb->metadata_meq32.value = attrib->meta_data;
		}

		if (attrib->attrib_mask & IPA_FLT_FRAGMENT) {
			*en_rule |= IPA_IS_FRAG;
			eq_atrb->ipv4_frag_eq_present = 1;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -14 => offset of dst mac addr in Ethernet II hdr */
			ipa_generate_flt_mac_addr_eq(eq_atrb, -14,
				attrib->dst_mac_addr_mask, attrib->dst_mac_addr,
				ofst_meq128);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -8 => offset of src mac addr in Ethernet II hdr */
			ipa_generate_flt_mac_addr_eq(eq_atrb, -8,
				attrib->src_mac_addr_mask, attrib->src_mac_addr,
				ofst_meq128);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -22 => offset of dst mac addr in 802.3 hdr */
			ipa_generate_flt_mac_addr_eq(eq_atrb, -22,
				attrib->dst_mac_addr_mask, attrib->dst_mac_addr,
				ofst_meq128);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -16 => offset of src mac addr in 802.3 hdr */
			ipa_generate_flt_mac_addr_eq(eq_atrb, -16,
				attrib->src_mac_addr_mask, attrib->src_mac_addr,
				ofst_meq128);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			eq_atrb->offset_meq_32[ofst_meq32].offset = -2;
			eq_atrb->offset_meq_32[ofst_meq32].mask =
				htons(attrib->ether_type);
			eq_atrb->offset_meq_32[ofst_meq32].value =
				htons(attrib->ether_type);
			ofst_meq32++;
		}
	} else if (ip == IPA_IP_v6) {

		/* v6 code below assumes no extension headers TODO: fix this */

		/* error check */
		if (attrib->attrib_mask & IPA_FLT_TOS ||
		    attrib->attrib_mask & IPA_FLT_PROTOCOL) {
			IPAERR("v4 attrib's specified for v6 rule\n");
			return -EPERM;
		}

		if (attrib->attrib_mask & IPA_FLT_NEXT_HDR) {
			*en_rule |= IPA_PROTOCOL_EQ;
			eq_atrb->protocol_eq_present = 1;
			eq_atrb->protocol_eq = attrib->u.v6.next_hdr;
		}

		if (attrib->attrib_mask & IPA_FLT_TYPE) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 0;
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask = 0xFF;
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
				attrib->type;
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_CODE) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 1;
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask = 0xFF;
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
				attrib->code;
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SPI) {
			if (ipa_ihl_ofst_meq32[ihl_ofst_meq32] == -1) {
				IPAERR("ran out of ihl_meq32 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_meq32[ihl_ofst_meq32];
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 0;
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask =
				0xFFFFFFFF;
			eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
				attrib->spi;
			ihl_ofst_meq32++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 0;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
				= attrib->src_port;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
				= attrib->src_port;
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 2;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
				= attrib->dst_port;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
				= attrib->dst_port;
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			if (attrib->src_port_hi < attrib->src_port_lo) {
				IPAERR("bad src port range param\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 0;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
				= attrib->src_port_lo;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
				= attrib->src_port_hi;
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
			if (ipa_ihl_ofst_rng16[ihl_ofst_rng16] == -1) {
				IPAERR("ran out of ihl_rng16 eq\n");
				return -EPERM;
			}
			if (attrib->dst_port_hi < attrib->dst_port_lo) {
				IPAERR("bad dst port range param\n");
				return -EPERM;
			}
			*en_rule |= ipa_ihl_ofst_rng16[ihl_ofst_rng16];
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 2;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
				= attrib->dst_port_lo;
			eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
				= attrib->dst_port_hi;
			ihl_ofst_rng16++;
		}

		if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];
			eq_atrb->offset_meq_128[ofst_meq128].offset = 8;
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 0)
				= attrib->u.v6.src_addr_mask[0];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 4)
				= attrib->u.v6.src_addr_mask[1];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 8)
				= attrib->u.v6.src_addr_mask[2];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 12)
				= attrib->u.v6.src_addr_mask[3];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 0)
				= attrib->u.v6.src_addr[0];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 4)
				= attrib->u.v6.src_addr[1];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 8)
				= attrib->u.v6.src_addr[2];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value +
					12) = attrib->u.v6.src_addr[3];
			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];
			eq_atrb->offset_meq_128[ofst_meq128].offset = 24;
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 0)
				= attrib->u.v6.dst_addr_mask[0];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 4)
				= attrib->u.v6.dst_addr_mask[1];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 8)
				= attrib->u.v6.dst_addr_mask[2];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 12)
				= attrib->u.v6.dst_addr_mask[3];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 0)
				= attrib->u.v6.dst_addr[0];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 4)
				= attrib->u.v6.dst_addr[1];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 8)
				= attrib->u.v6.dst_addr[2];
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value +
					12) = attrib->u.v6.dst_addr[3];
			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_TC) {
			*en_rule |= IPA_FLT_TC;
			eq_atrb->tc_eq_present = 1;
			eq_atrb->tc_eq = attrib->u.v6.tc;
		}

		if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];
			eq_atrb->offset_meq_128[ofst_meq128].offset = 0;
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 0)
				= attrib->tos_mask << 20;
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 4)
				= 0;
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 8)
				= 0;
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 12)
				= 0;
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 0)
				= attrib->tos_value << 20;
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 4)
				= 0;
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 8)
				= 0;
			*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value +
					12) = 0;
			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_FLOW_LABEL) {
			*en_rule |= IPA_FLT_FLOW_LABEL;
			eq_atrb->fl_eq_present = 1;
			eq_atrb->fl_eq = attrib->u.v6.flow_label;
		}

		if (attrib->attrib_mask & IPA_FLT_META_DATA) {
			*en_rule |= IPA_METADATA_COMPARE;
			eq_atrb->metadata_meq32_present = 1;
			eq_atrb->metadata_meq32.offset = 0;
			eq_atrb->metadata_meq32.mask = attrib->meta_data_mask;
			eq_atrb->metadata_meq32.value = attrib->meta_data;
		}

		if (attrib->attrib_mask & IPA_FLT_FRAGMENT) {
			*en_rule |= IPA_IS_FRAG;
			eq_atrb->ipv4_frag_eq_present = 1;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -14 => offset of dst mac addr in Ethernet II hdr */
			ipa_generate_flt_mac_addr_eq(eq_atrb, -14,
				attrib->dst_mac_addr_mask, attrib->dst_mac_addr,
				ofst_meq128);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -8 => offset of src mac addr in Ethernet II hdr */
			ipa_generate_flt_mac_addr_eq(eq_atrb, -8,
				attrib->src_mac_addr_mask, attrib->src_mac_addr,
				ofst_meq128);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -22 => offset of dst mac addr in 802.3 hdr */
			ipa_generate_flt_mac_addr_eq(eq_atrb, -22,
				attrib->dst_mac_addr_mask, attrib->dst_mac_addr,
				ofst_meq128);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3) {
			if (ipa_ofst_meq128[ofst_meq128] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq128[ofst_meq128];

			/* -16 => offset of src mac addr in 802.3 hdr */
			ipa_generate_flt_mac_addr_eq(eq_atrb, -16,
				attrib->src_mac_addr_mask, attrib->src_mac_addr,
				ofst_meq128);

			ofst_meq128++;
		}

		if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE) {
			if (ipa_ofst_meq32[ofst_meq32] == -1) {
				IPAERR("ran out of meq128 eq\n");
				return -EPERM;
			}
			*en_rule |= ipa_ofst_meq32[ofst_meq32];
			eq_atrb->offset_meq_32[ofst_meq32].offset = -2;
			eq_atrb->offset_meq_32[ofst_meq32].mask =
				htons(attrib->ether_type);
			eq_atrb->offset_meq_32[ofst_meq32].value =
				htons(attrib->ether_type);
			ofst_meq32++;
		}

	} else {
		IPAERR("unsupported ip %d\n", ip);
		return -EPERM;
	}

	/*
	 * default "rule" means no attributes set -> map to
	 * OFFSET_MEQ32_0 with mask of 0 and val of 0 and offset 0
	 */
	if (attrib->attrib_mask == 0) {
		if (ipa_ofst_meq32[ofst_meq32] == -1) {
			IPAERR("ran out of meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= ipa_ofst_meq32[ofst_meq32];
		eq_atrb->offset_meq_32[ofst_meq32].offset = 0;
		eq_atrb->offset_meq_32[ofst_meq32].mask = 0;
		eq_atrb->offset_meq_32[ofst_meq32].value = 0;
		ofst_meq32++;
	}

	eq_atrb->rule_eq_bitmap = *en_rule;
	eq_atrb->num_offset_meq_32 = ofst_meq32;
	eq_atrb->num_ihl_offset_range_16 = ihl_ofst_rng16;
	eq_atrb->num_ihl_offset_meq_32 = ihl_ofst_meq32;
	eq_atrb->num_offset_meq_128 = ofst_meq128;

	return 0;
}

/**
 * ipa_cfg_ep - IPA end-point configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * This includes nat, header, mode, aggregation and route settings and is a one
 * shot API to configure the IPA end-point fully
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep(u32 clnt_hdl, const struct ipa_ep_cfg *ipa_ep_cfg)
{
	int result = -EINVAL;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ipa_ep_cfg == NULL) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	result = ipa_cfg_ep_hdr(clnt_hdl, &ipa_ep_cfg->hdr);
	if (result)
		return result;

	result = ipa_cfg_ep_hdr_ext(clnt_hdl, &ipa_ep_cfg->hdr_ext);
	if (result)
		return result;

	result = ipa_cfg_ep_aggr(clnt_hdl, &ipa_ep_cfg->aggr);
	if (result)
		return result;

	result = ipa_cfg_ep_cfg(clnt_hdl, &ipa_ep_cfg->cfg);
	if (result)
		return result;

	if (IPA_CLIENT_IS_PROD(ipa_ctx->ep[clnt_hdl].client)) {
		result = ipa_cfg_ep_nat(clnt_hdl, &ipa_ep_cfg->nat);
		if (result)
			return result;

		result = ipa_cfg_ep_mode(clnt_hdl, &ipa_ep_cfg->mode);
		if (result)
			return result;

		result = ipa_cfg_ep_route(clnt_hdl, &ipa_ep_cfg->route);
		if (result)
			return result;

		result = ipa_cfg_ep_deaggr(clnt_hdl, &ipa_ep_cfg->deaggr);
		if (result)
			return result;
	} else {
		result = ipa_cfg_ep_metadata_mask(clnt_hdl,
				&ipa_ep_cfg->metadata_mask);
		if (result)
			return result;
	}

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep);

const char *ipa_get_nat_en_str(enum ipa_nat_en_type nat_en)
{
	switch (nat_en) {
	case (IPA_BYPASS_NAT):
		return "NAT disabled";
	case (IPA_SRC_NAT):
		return "Source NAT";
	case (IPA_DST_NAT):
		return "Dst NAT";
	}

	return "undefined";
}

void _ipa_cfg_ep_nat_v1_0(u32 clnt_hdl, const struct ipa_ep_cfg_nat *ep_nat)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_nat->nat_en,
			IPA_ENDP_INIT_NAT_N_NAT_EN_SHFT,
			IPA_ENDP_INIT_NAT_N_NAT_EN_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_NAT_N_OFST_v1_0(clnt_hdl),
			reg_val);
}

void _ipa_cfg_ep_nat_v1_1(u32 clnt_hdl,
		const struct ipa_ep_cfg_nat *ep_nat)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_nat->nat_en,
			IPA_ENDP_INIT_NAT_N_NAT_EN_SHFT,
			IPA_ENDP_INIT_NAT_N_NAT_EN_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_NAT_N_OFST_v1_1(clnt_hdl),
			reg_val);
}

void _ipa_cfg_ep_nat_v2_0(u32 clnt_hdl,
		const struct ipa_ep_cfg_nat *ep_nat)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_nat->nat_en,
			IPA_ENDP_INIT_NAT_N_NAT_EN_SHFT,
			IPA_ENDP_INIT_NAT_N_NAT_EN_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_NAT_N_OFST_v2_0(clnt_hdl),
			reg_val);
}

/**
 * ipa_cfg_ep_nat() - IPA end-point NAT configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_nat(u32 clnt_hdl, const struct ipa_ep_cfg_nat *ep_nat)
{
	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ep_nat == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa_ctx->ep[clnt_hdl].client)) {
		IPAERR("NAT does not apply to IPA out EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	IPADBG("pipe=%d, nat_en=%d(%s)\n",
			clnt_hdl,
			ep_nat->nat_en,
			ipa_get_nat_en_str(ep_nat->nat_en));

	/* copy over EP cfg */
	ipa_ctx->ep[clnt_hdl].cfg.nat = *ep_nat;

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_ep_nat(clnt_hdl, ep_nat);

	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_nat);

static void _ipa_cfg_ep_status_v1_1(u32 clnt_hdl,
				const struct ipa_ep_cfg_status *ep_status)
{
	IPADBG("Not supported for version 1.1\n");
}

static void _ipa_cfg_ep_status_v2_0(u32 clnt_hdl,
		const struct ipa_ep_cfg_status *ep_status)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_status->status_en,
			IPA_ENDP_STATUS_n_STATUS_EN_SHFT,
			IPA_ENDP_STATUS_n_STATUS_EN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_status->status_ep,
			IPA_ENDP_STATUS_n_STATUS_ENDP_SHFT,
			IPA_ENDP_STATUS_n_STATUS_ENDP_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_STATUS_n_OFST(clnt_hdl),
			reg_val);
}

/**
 * ipa_cfg_ep_status() - IPA end-point status configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_status(u32 clnt_hdl, const struct ipa_ep_cfg_status *ep_status)
{
	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ep_status == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d, status_en=%d status_ep=%d\n",
			clnt_hdl,
			ep_status->status_en,
			ep_status->status_ep);

	/* copy over EP cfg */
	ipa_ctx->ep[clnt_hdl].status = *ep_status;

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_ep_status(clnt_hdl, ep_status);

	ipa_dec_client_disable_clks();

	return 0;
}

static void _ipa_cfg_ep_cfg_v1_1(u32 clnt_hdl,
				const struct ipa_ep_cfg_cfg *cfg)
{
	IPADBG("Not supported for version 1.1\n");
}

static void _ipa_cfg_ep_cfg_v2_0(u32 clnt_hdl,
		const struct ipa_ep_cfg_cfg *cfg)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, cfg->frag_offload_en,
			IPA_ENDP_INIT_CFG_n_FRAG_OFFLOAD_EN_SHFT,
			IPA_ENDP_INIT_CFG_n_FRAG_OFFLOAD_EN_BMSK);
	IPA_SETFIELD_IN_REG(reg_val, cfg->cs_offload_en,
			IPA_ENDP_INIT_CFG_n_CS_OFFLOAD_EN_SHFT,
			IPA_ENDP_INIT_CFG_n_CS_OFFLOAD_EN_BMSK);
	IPA_SETFIELD_IN_REG(reg_val, cfg->cs_metadata_hdr_offset,
			IPA_ENDP_INIT_CFG_n_CS_METADATA_HDR_OFFSET_SHFT,
			IPA_ENDP_INIT_CFG_n_CS_METADATA_HDR_OFFSET_BMSK);

	ipa_write_reg(ipa_ctx->mmio, IPA_ENDP_INIT_CFG_n_OFST(clnt_hdl),
			reg_val);
}

/**
 * ipa_cfg_ep_cfg() - IPA end-point cfg configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_cfg(u32 clnt_hdl, const struct ipa_ep_cfg_cfg *cfg)
{
	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			cfg == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d, frag_ofld_en=%d cs_ofld_en=%d mdata_hdr_ofst=%d\n",
			clnt_hdl,
			cfg->frag_offload_en,
			cfg->cs_offload_en,
			cfg->cs_metadata_hdr_offset);

	/* copy over EP cfg */
	ipa_ctx->ep[clnt_hdl].cfg.cfg = *cfg;

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_ep_cfg(clnt_hdl, cfg);

	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_cfg);

static void _ipa_cfg_ep_metadata_mask_v1_1(u32 clnt_hdl,
			const struct ipa_ep_cfg_metadata_mask *metadata_mask)
{
	IPADBG("Not supported for version 1.1\n");
}

static void _ipa_cfg_ep_metadata_mask_v2_0(u32 clnt_hdl,
		const struct ipa_ep_cfg_metadata_mask *metadata_mask)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, metadata_mask->metadata_mask,
			IPA_ENDP_INIT_HDR_METADATA_MASK_n_METADATA_MASK_SHFT,
			IPA_ENDP_INIT_HDR_METADATA_MASK_n_METADATA_MASK_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HDR_METADATA_MASK_n_OFST(clnt_hdl),
			reg_val);
}

/**
 * ipa_cfg_ep_metadata_mask() - IPA end-point meta-data mask configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_metadata_mask(u32 clnt_hdl, const struct ipa_ep_cfg_metadata_mask
		*metadata_mask)
{
	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			metadata_mask == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl,
					ipa_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d, metadata_mask=0x%x\n",
			clnt_hdl,
			metadata_mask->metadata_mask);

	/* copy over EP cfg */
	ipa_ctx->ep[clnt_hdl].cfg.metadata_mask = *metadata_mask;

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_ep_metadata_mask(clnt_hdl, metadata_mask);

	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_metadata_mask);

void _ipa_cfg_ep_hdr_v1_1(u32 pipe_number,
		const struct ipa_ep_cfg_hdr *ep_hdr)
{
	u32 val = 0;

	val = IPA_SETFIELD(ep_hdr->hdr_len,
		   IPA_ENDP_INIT_HDR_N_HDR_LEN_SHFT,
		   IPA_ENDP_INIT_HDR_N_HDR_LEN_BMSK) |
	      IPA_SETFIELD(ep_hdr->hdr_ofst_metadata_valid,
		   IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_VALID_SHFT,
		   IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_VALID_BMSK) |
	      IPA_SETFIELD(ep_hdr->hdr_ofst_metadata,
		   IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_SHFT,
		   IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_BMSK) |
	      IPA_SETFIELD(ep_hdr->hdr_additional_const_len,
		   IPA_ENDP_INIT_HDR_N_HDR_ADDITIONAL_CONST_LEN_SHFT,
		   IPA_ENDP_INIT_HDR_N_HDR_ADDITIONAL_CONST_LEN_BMSK) |
	      IPA_SETFIELD(ep_hdr->hdr_ofst_pkt_size_valid,
		   IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_VALID_SHFT,
		   IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_VALID_BMSK) |
	      IPA_SETFIELD(ep_hdr->hdr_ofst_pkt_size,
		   IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_SHFT,
		   IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_BMSK) |
	      IPA_SETFIELD(ep_hdr->hdr_a5_mux,
		   IPA_ENDP_INIT_HDR_N_HDR_A5_MUX_SHFT,
		   IPA_ENDP_INIT_HDR_N_HDR_A5_MUX_BMSK);
	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HDR_N_OFST_v1_1(pipe_number), val);
}

void _ipa_cfg_ep_hdr_v2_0(u32 pipe_number,
		const struct ipa_ep_cfg_hdr *ep_hdr)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_metadata_reg_valid,
			IPA_ENDP_INIT_HDR_N_HDR_METADATA_REG_VALID_SHFT_v2,
			IPA_ENDP_INIT_HDR_N_HDR_METADATA_REG_VALID_BMSK_v2);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_remove_additional,
			IPA_ENDP_INIT_HDR_N_HDR_LEN_INC_DEAGG_HDR_SHFT_v2,
			IPA_ENDP_INIT_HDR_N_HDR_LEN_INC_DEAGG_HDR_BMSK_v2);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_a5_mux,
			IPA_ENDP_INIT_HDR_N_HDR_A5_MUX_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_A5_MUX_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_ofst_pkt_size,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_ofst_pkt_size_valid,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_VALID_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_PKT_SIZE_VALID_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_additional_const_len,
			IPA_ENDP_INIT_HDR_N_HDR_ADDITIONAL_CONST_LEN_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_ADDITIONAL_CONST_LEN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_ofst_metadata,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_ofst_metadata_valid,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_VALID_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_OFST_METADATA_VALID_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr->hdr_len,
			IPA_ENDP_INIT_HDR_N_HDR_LEN_SHFT,
			IPA_ENDP_INIT_HDR_N_HDR_LEN_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HDR_N_OFST_v2_0(pipe_number), reg_val);
}

/**
 * ipa_cfg_ep_hdr() -  IPA end-point header configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_hdr(u32 clnt_hdl, const struct ipa_ep_cfg_hdr *ep_hdr)
{
	struct ipa_ep_context *ep;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ep_hdr == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
				clnt_hdl, ipa_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}
	IPADBG("pipe=%d remove_additional=%d, a5_mux=%d, ofst_pkt_size=0x%x\n",
		clnt_hdl,
		ep_hdr->hdr_remove_additional,
		ep_hdr->hdr_a5_mux,
		ep_hdr->hdr_ofst_pkt_size);

	IPADBG("ofst_pkt_size_valid=%d, additional_const_len=0x%x\n",
		ep_hdr->hdr_ofst_pkt_size_valid,
		ep_hdr->hdr_additional_const_len);

	IPADBG("ofst_metadata=0x%x, ofst_metadata_valid=%d, len=0x%x",
		ep_hdr->hdr_ofst_metadata,
		ep_hdr->hdr_ofst_metadata_valid,
		ep_hdr->hdr_len);

	ep = &ipa_ctx->ep[clnt_hdl];

	/* copy over EP cfg */
	ep->cfg.hdr = *ep_hdr;

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_ep_hdr(clnt_hdl, &ep->cfg.hdr);

	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_hdr);

static int _ipa_cfg_ep_hdr_ext_v1_1(u32 clnt_hdl,
				const struct ipa_ep_cfg_hdr_ext *ep_hdr)
{
	IPADBG("Not supported for version 1.1\n");
	return 0;
}

static int _ipa_cfg_ep_hdr_ext(u32 clnt_hdl,
		const struct ipa_ep_cfg_hdr_ext *ep_hdr_ext, u32 reg_val)
{
	u8 hdr_endianess = ep_hdr_ext->hdr_little_endian ? 0 : 1;

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr_ext->hdr_total_len_or_pad_offset,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_OFFSET_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_OFFSET_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr_ext->hdr_payload_len_inc_padding,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAYLOAD_LEN_INC_PADDING_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAYLOAD_LEN_INC_PADDING_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr_ext->hdr_total_len_or_pad,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr_ext->hdr_total_len_or_pad_valid,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_VALID_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_TOTAL_LEN_OR_PAD_VALID_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, hdr_endianess,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_ENDIANESS_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_ENDIANESS_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
		IPA_ENDP_INIT_HDR_EXT_n_OFST_v2_0(clnt_hdl), reg_val);

	return 0;
}

static int _ipa_cfg_ep_hdr_ext_v2_0(u32 clnt_hdl,
				const struct ipa_ep_cfg_hdr_ext *ep_hdr_ext)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr_ext->hdr_pad_to_alignment,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAD_TO_ALIGNMENT_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAD_TO_ALIGNMENT_BMSK_v2_0);

	return _ipa_cfg_ep_hdr_ext(clnt_hdl, ep_hdr_ext, reg_val);
}

static int _ipa_cfg_ep_hdr_ext_v2_5(u32 clnt_hdl,
				const struct ipa_ep_cfg_hdr_ext *ep_hdr_ext)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_hdr_ext->hdr_pad_to_alignment,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAD_TO_ALIGNMENT_SHFT,
		IPA_ENDP_INIT_HDR_EXT_n_HDR_PAD_TO_ALIGNMENT_BMSK_v2_5);

	return _ipa_cfg_ep_hdr_ext(clnt_hdl, ep_hdr_ext, reg_val);

}
/**
 * ipa_cfg_ep_hdr_ext() -  IPA end-point extended header configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ep_hdr_ext:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_hdr_ext(u32 clnt_hdl,
		       const struct ipa_ep_cfg_hdr_ext *ep_hdr_ext)
{
	struct ipa_ep_context *ep;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ep_hdr_ext == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
				clnt_hdl, ipa_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d hdr_pad_to_alignment=%d\n",
		clnt_hdl,
		ep_hdr_ext->hdr_pad_to_alignment);

	IPADBG("hdr_total_len_or_pad_offset=%d\n",
		ep_hdr_ext->hdr_total_len_or_pad_offset);

	IPADBG("hdr_payload_len_inc_padding=%d hdr_total_len_or_pad=%d\n",
		ep_hdr_ext->hdr_payload_len_inc_padding,
		ep_hdr_ext->hdr_total_len_or_pad);

	IPADBG("hdr_total_len_or_pad_valid=%d hdr_little_endian=%d\n",
		ep_hdr_ext->hdr_total_len_or_pad_valid,
		ep_hdr_ext->hdr_little_endian);

	ep = &ipa_ctx->ep[clnt_hdl];

	/* copy over EP cfg */
	ep->cfg.hdr_ext = *ep_hdr_ext;

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_ep_hdr_ext(clnt_hdl, &ep->cfg.hdr_ext);

	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_hdr_ext);

/**
 * ipa_cfg_ep_hdr() -  IPA end-point Control configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg_ctrl:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_cfg_ep_ctrl(u32 clnt_hdl, const struct ipa_ep_cfg_ctrl *ep_ctrl)
{
	u32 reg_val = 0;

	if (clnt_hdl >= IPA_NUM_PIPES || ep_ctrl == NULL) {
		IPAERR("bad parm, clnt_hdl = %d\n", clnt_hdl);
		return -EINVAL;
	}
	IPADBG("pipe=%d ep_suspend=%d, ep_delay=%d\n",
		clnt_hdl,
		ep_ctrl->ipa_ep_suspend,
		ep_ctrl->ipa_ep_delay);


	IPA_SETFIELD_IN_REG(reg_val, ep_ctrl->ipa_ep_suspend,
		IPA_ENDP_INIT_CTRL_N_ENDP_SUSPEND_SHFT,
		IPA_ENDP_INIT_CTRL_N_ENDP_SUSPEND_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_ctrl->ipa_ep_delay,
			IPA_ENDP_INIT_CTRL_N_ENDP_DELAY_SHFT,
			IPA_ENDP_INIT_CTRL_N_ENDP_DELAY_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
		IPA_ENDP_INIT_CTRL_N_OFST(clnt_hdl), reg_val);

	return 0;

}
EXPORT_SYMBOL(ipa_cfg_ep_ctrl);

/**
 * ipa_cfg_aggr_cntr_granularity() - granularity of the AGGR timer configuration
 * @aggr_granularity:     [in] defines the granularity of AGGR timers
 *			  number of units of 1/32msec
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_cfg_aggr_cntr_granularity(u8 aggr_granularity)
{
	u32 reg_val = 0;

	if (aggr_granularity <= IPA_AGGR_GRAN_MIN ||
			aggr_granularity > IPA_AGGR_GRAN_MAX) {
		IPAERR("bad param, aggr_granularity = %d\n",
				aggr_granularity);
		return -EINVAL;
	}
	IPADBG("aggr_granularity=%d\n", aggr_granularity);

	reg_val = ipa_read_reg(ipa_ctx->mmio, IPA_COUNTER_CFG_OFST);
	reg_val = (reg_val & ~IPA_COUNTER_CFG_AGGR_GRAN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, aggr_granularity - 1,
			IPA_COUNTER_CFG_AGGR_GRAN_SHFT,
			IPA_COUNTER_CFG_AGGR_GRAN_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_COUNTER_CFG_OFST, reg_val);

	return 0;

}
EXPORT_SYMBOL(ipa_cfg_aggr_cntr_granularity);

/**
 * ipa_cfg_eot_coal_cntr_granularity() - granularity of EOT_COAL timer
 *					 configuration
 * @eot_coal_granularity: defines the granularity of EOT_COAL timers
 *			  number of units of 1/32msec
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_cfg_eot_coal_cntr_granularity(u8 eot_coal_granularity)
{
	u32 reg_val = 0;

	if (eot_coal_granularity <= IPA_EOT_COAL_GRAN_MIN ||
			eot_coal_granularity > IPA_EOT_COAL_GRAN_MAX) {
		IPAERR("bad parm, eot_coal_granularity = %d\n",
				eot_coal_granularity);
		return -EINVAL;
	}
	IPADBG("eot_coal_granularity=%d\n", eot_coal_granularity);

	reg_val = ipa_read_reg(ipa_ctx->mmio, IPA_COUNTER_CFG_OFST);
	reg_val = (reg_val & ~IPA_COUNTER_CFG_EOT_COAL_GRAN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, eot_coal_granularity - 1,
			IPA_COUNTER_CFG_EOT_COAL_GRAN_SHFT,
			IPA_COUNTER_CFG_EOT_COAL_GRAN_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_COUNTER_CFG_OFST, reg_val);

	return 0;

}
EXPORT_SYMBOL(ipa_cfg_eot_coal_cntr_granularity);

const char *ipa_get_mode_type_str(enum ipa_mode_type mode)
{
	switch (mode) {
	case (IPA_BASIC):
		return "Basic";
	case (IPA_ENABLE_FRAMING_HDLC):
		return "HDLC framing";
	case (IPA_ENABLE_DEFRAMING_HDLC):
		return "HDLC de-framing";
	case (IPA_DMA):
		return "DMA";
	}

	return "undefined";
}

void _ipa_cfg_ep_mode_v1_0(u32 pipe_number, u32 dst_pipe_number,
		const struct ipa_ep_cfg_mode *ep_mode)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_mode->mode,
			IPA_ENDP_INIT_MODE_N_MODE_SHFT,
			IPA_ENDP_INIT_MODE_N_MODE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, dst_pipe_number,
			IPA_ENDP_INIT_MODE_N_DEST_PIPE_INDEX_SHFT_v1,
			IPA_ENDP_INIT_MODE_N_DEST_PIPE_INDEX_BMSK_v1);

		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_MODE_N_OFST_v1_0(pipe_number), reg_val);
}

void _ipa_cfg_ep_mode_v1_1(u32 pipe_number, u32 dst_pipe_number,
		const struct ipa_ep_cfg_mode *ep_mode)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_mode->mode,
			IPA_ENDP_INIT_MODE_N_MODE_SHFT,
			IPA_ENDP_INIT_MODE_N_MODE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, dst_pipe_number,
			IPA_ENDP_INIT_MODE_N_DEST_PIPE_INDEX_SHFT_v1,
			IPA_ENDP_INIT_MODE_N_DEST_PIPE_INDEX_BMSK_v1);

		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_MODE_N_OFST_v1_1(pipe_number), reg_val);
}

void _ipa_cfg_ep_mode_v2_0(u32 pipe_number, u32 dst_pipe_number,
		const struct ipa_ep_cfg_mode *ep_mode)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_mode->mode,
			IPA_ENDP_INIT_MODE_N_MODE_SHFT,
			IPA_ENDP_INIT_MODE_N_MODE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, dst_pipe_number,
			IPA_ENDP_INIT_MODE_N_DEST_PIPE_INDEX_SHFT_v2_0,
			IPA_ENDP_INIT_MODE_N_DEST_PIPE_INDEX_BMSK_v2_0);

		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_MODE_N_OFST_v2_0(pipe_number), reg_val);
}

/**
 * ipa_cfg_ep_mode() - IPA end-point mode configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_mode(u32 clnt_hdl, const struct ipa_ep_cfg_mode *ep_mode)
{
	int ep;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ep_mode == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl, ipa_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa_ctx->ep[clnt_hdl].client)) {
		IPAERR("MODE does not apply to IPA out EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	ep = ipa_get_ep_mapping(ep_mode->dst);
	if (ep == -1 && ep_mode->mode == IPA_DMA) {
		IPAERR("dst %d does not exist\n", ep_mode->dst);
		return -EINVAL;
	}

	IPADBG("pipe=%d mode=%d(%s), dst_client_number=%d",
			clnt_hdl,
			ep_mode->mode,
			ipa_get_mode_type_str(ep_mode->mode),
			ep_mode->dst);

	/* copy over EP cfg */
	ipa_ctx->ep[clnt_hdl].cfg.mode = *ep_mode;
	ipa_ctx->ep[clnt_hdl].dst_pipe_index = ep;

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_ep_mode(clnt_hdl,
			ipa_ctx->ep[clnt_hdl].dst_pipe_index,
			ep_mode);

	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_mode);

const char *get_aggr_enable_str(enum ipa_aggr_en_type aggr_en)
{
	switch (aggr_en) {
	case (IPA_BYPASS_AGGR):
			return "no aggregation";
	case (IPA_ENABLE_AGGR):
			return "aggregation enabled";
	case (IPA_ENABLE_DEAGGR):
		return "de-aggregation enabled";
	}

	return "undefined";
}

const char *get_aggr_type_str(enum ipa_aggr_type aggr_type)
{
	switch (aggr_type) {
	case (IPA_MBIM_16):
			return "MBIM_16";
	case (IPA_HDLC):
		return "HDLC";
	case (IPA_TLP):
			return "TLP";
	case (IPA_RNDIS):
			return "RNDIS";
	case (IPA_GENERIC):
			return "GENERIC";
	case (IPA_QCMAP):
			return "QCMAP";
	}
	return "undefined";
}


void _ipa_cfg_ep_aggr_v1_0(u32 pipe_number,
		const struct ipa_ep_cfg_aggr *ep_aggr)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_en,
			IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr,
			IPA_ENDP_INIT_AGGR_N_AGGR_TYPE_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_TYPE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_byte_limit,
			IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_time_limit,
			IPA_ENDP_INIT_AGGR_N_AGGR_TIME_LIMIT_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_TIME_LIMIT_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_AGGR_N_OFST_v1_0(pipe_number), reg_val);
}

void _ipa_cfg_ep_aggr_v1_1(u32 pipe_number,
		const struct ipa_ep_cfg_aggr *ep_aggr)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_en,
			IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr,
			IPA_ENDP_INIT_AGGR_N_AGGR_TYPE_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_TYPE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_byte_limit,
			IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_time_limit,
			IPA_ENDP_INIT_AGGR_N_AGGR_TIME_LIMIT_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_TIME_LIMIT_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_AGGR_N_OFST_v1_1(pipe_number), reg_val);
}

void _ipa_cfg_ep_aggr_v2_0(u32 pipe_number,
		const struct ipa_ep_cfg_aggr *ep_aggr)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_en,
			IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr,
			IPA_ENDP_INIT_AGGR_N_AGGR_TYPE_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_TYPE_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_byte_limit,
			IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_BYTE_LIMIT_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_time_limit,
			IPA_ENDP_INIT_AGGR_N_AGGR_TIME_LIMIT_SHFT,
			IPA_ENDP_INIT_AGGR_N_AGGR_TIME_LIMIT_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_aggr->aggr_pkt_limit,
			IPA_ENDP_INIT_AGGR_n_AGGR_PKT_LIMIT_SHFT,
			IPA_ENDP_INIT_AGGR_n_AGGR_PKT_LIMIT_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_AGGR_N_OFST_v2_0(pipe_number), reg_val);
}

/**
 * ipa_cfg_ep_aggr() - IPA end-point aggregation configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_aggr(u32 clnt_hdl, const struct ipa_ep_cfg_aggr *ep_aggr)
{
	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ep_aggr == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
			clnt_hdl, ipa_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d en=%d(%s), type=%d(%s), byte_limit=%d, time_limit=%d\n",
			clnt_hdl,
			ep_aggr->aggr_en,
			get_aggr_enable_str(ep_aggr->aggr_en),
			ep_aggr->aggr,
			get_aggr_type_str(ep_aggr->aggr),
			ep_aggr->aggr_byte_limit,
			ep_aggr->aggr_time_limit);

	/* copy over EP cfg */
	ipa_ctx->ep[clnt_hdl].cfg.aggr = *ep_aggr;

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_ep_aggr(clnt_hdl, ep_aggr);

	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_aggr);

void _ipa_cfg_ep_route_v1_0(u32 pipe_index, u32 rt_tbl_index)
{
	int reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, rt_tbl_index,
			IPA_ENDP_INIT_ROUTE_N_ROUTE_TABLE_INDEX_SHFT,
			IPA_ENDP_INIT_ROUTE_N_ROUTE_TABLE_INDEX_BMSK);

		ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_ROUTE_N_OFST_v1_0(pipe_index),
			reg_val);
}

void _ipa_cfg_ep_route_v1_1(u32 pipe_index, u32 rt_tbl_index)
{
	int reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, rt_tbl_index,
			IPA_ENDP_INIT_ROUTE_N_ROUTE_TABLE_INDEX_SHFT,
			IPA_ENDP_INIT_ROUTE_N_ROUTE_TABLE_INDEX_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_ROUTE_N_OFST_v1_1(pipe_index),
			reg_val);
}

void _ipa_cfg_ep_route_v2_0(u32 pipe_index, u32 rt_tbl_index)
{
	int reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, rt_tbl_index,
			IPA_ENDP_INIT_ROUTE_N_ROUTE_TABLE_INDEX_SHFT,
			IPA_ENDP_INIT_ROUTE_N_ROUTE_TABLE_INDEX_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_ROUTE_N_OFST_v2_0(pipe_index),
			reg_val);
}

/**
 * ipa_cfg_ep_route() - IPA end-point routing configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_route(u32 clnt_hdl, const struct ipa_ep_cfg_route *ep_route)
{
	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ep_route == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
			clnt_hdl, ipa_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_CONS(ipa_ctx->ep[clnt_hdl].client)) {
		IPAERR("ROUTE does not apply to IPA out EP %d\n",
				clnt_hdl);
		return -EINVAL;
	}

	/*
	 * if DMA mode was configured previously for this EP, return with
	 * success
	 */
	if (ipa_ctx->ep[clnt_hdl].cfg.mode.mode == IPA_DMA) {
		IPADBG("DMA enabled for ep %d, dst pipe is part of DMA\n",
				clnt_hdl);
		return 0;
	}

	if (ep_route->rt_tbl_hdl)
		IPAERR("client specified non-zero RT TBL hdl - ignore it\n");

	IPADBG("pipe=%d, rt_tbl_hdl=%d\n",
			clnt_hdl,
			ep_route->rt_tbl_hdl);

	/* always use "default" routing table when programming EP ROUTE reg */
	if (ipa_ctx->ipa_hw_type == IPA_HW_v2_0 ||
		ipa_ctx->ipa_hw_type == IPA_HW_v2_5)
		ipa_ctx->ep[clnt_hdl].rt_tbl_idx =
			IPA_MEM_PART(v4_apps_rt_index_lo);
	else
		ipa_ctx->ep[clnt_hdl].rt_tbl_idx = 0;

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_ep_route(clnt_hdl,
			ipa_ctx->ep[clnt_hdl].rt_tbl_idx);

	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_route);

void _ipa_cfg_ep_holb_v1_0(u32 pipe_number,
			const struct ipa_ep_cfg_holb *ep_holb)
{
	IPAERR("API not supported for this core\n");
}

void _ipa_cfg_ep_holb_v1_1(u32 pipe_number,
			const struct ipa_ep_cfg_holb *ep_holb)
{
	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_EN_N_OFST_v1_1(pipe_number),
			ep_holb->en);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_OFST_v1_1(pipe_number),
			(u16)ep_holb->tmr_val);
}

void _ipa_cfg_ep_holb_v2_0(u32 pipe_number,
			const struct ipa_ep_cfg_holb *ep_holb)
{
	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_EN_N_OFST_v2_0(pipe_number),
			ep_holb->en);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_OFST_v2_0(pipe_number),
			(u16)ep_holb->tmr_val);
}

void _ipa_cfg_ep_holb_v2_5(u32 pipe_number,
			const struct ipa_ep_cfg_holb *ep_holb)
{
	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_EN_N_OFST_v2_0(pipe_number),
			ep_holb->en);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HOL_BLOCK_TIMER_N_OFST_v2_0(pipe_number),
			ep_holb->tmr_val);
}

/**
 * ipa_cfg_ep_holb() - IPA end-point holb configuration
 *
 * If an IPA producer pipe is full, IPA HW by default will block
 * indefinitely till space opens up. During this time no packets
 * including those from unrelated pipes will be processed. Enabling
 * HOLB means IPA HW will be allowed to drop packets as/when needed
 * and indefinite blocking is avoided.
 *
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_cfg_ep_holb(u32 clnt_hdl, const struct ipa_ep_cfg_holb *ep_holb)
{
	if (clnt_hdl >= IPA_NUM_PIPES ||
	    ipa_ctx->ep[clnt_hdl].valid == 0 || ep_holb == NULL ||
	    ep_holb->tmr_val > ipa_ctx->ctrl->max_holb_tmr_val ||
	    ep_holb->en > 1) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	if (IPA_CLIENT_IS_PROD(ipa_ctx->ep[clnt_hdl].client)) {
		IPAERR("HOLB does not apply to IPA in EP %d\n", clnt_hdl);
		return -EINVAL;
	}

	if (!ipa_ctx->ctrl->ipa_cfg_ep_holb) {
		IPAERR("HOLB is not supported for this IPA core\n");
		return -EINVAL;
	}

	ipa_ctx->ep[clnt_hdl].holb = *ep_holb;

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_ep_holb(clnt_hdl, ep_holb);

	ipa_dec_client_disable_clks();

	IPADBG("cfg holb %u ep=%d tmr=%d\n", ep_holb->en, clnt_hdl,
				ep_holb->tmr_val);

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_holb);

/**
 * ipa_cfg_ep_holb_by_client() - IPA end-point holb configuration
 *
 * Wrapper function for ipa_cfg_ep_holb() with client name instead of
 * client handle. This function is used for clients that does not have
 * client handle.
 *
 * @client:	[in] client name
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_cfg_ep_holb_by_client(enum ipa_client_type client,
				const struct ipa_ep_cfg_holb *ep_holb)
{
	return ipa_cfg_ep_holb(ipa_get_ep_mapping(client), ep_holb);
}
EXPORT_SYMBOL(ipa_cfg_ep_holb_by_client);

static int _ipa_cfg_ep_deaggr_v1_1(u32 clnt_hdl,
				const struct ipa_ep_cfg_deaggr *ep_deaggr)
{
	IPADBG("Not supported for version 1.1\n");
	return 0;
}

static int _ipa_cfg_ep_deaggr_v2_0(u32 clnt_hdl,
				   const struct ipa_ep_cfg_deaggr *ep_deaggr)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, ep_deaggr->deaggr_hdr_len,
		IPA_ENDP_INIT_DEAGGR_n_DEAGGR_HDR_LEN_SHFT,
		IPA_ENDP_INIT_DEAGGR_n_DEAGGR_HDR_LEN_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_deaggr->packet_offset_valid,
		IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_VALID_SHFT,
		IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_VALID_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_deaggr->packet_offset_location,
		IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_LOCATION_SHFT,
		IPA_ENDP_INIT_DEAGGR_n_PACKET_OFFSET_LOCATION_BMSK);

	IPA_SETFIELD_IN_REG(reg_val, ep_deaggr->max_packet_len,
		IPA_ENDP_INIT_DEAGGR_n_MAX_PACKET_LEN_SHFT,
		IPA_ENDP_INIT_DEAGGR_n_MAX_PACKET_LEN_BMSK);

	ipa_write_reg(ipa_ctx->mmio,
		IPA_ENDP_INIT_DEAGGR_n_OFST_v2_0(clnt_hdl), reg_val);

	return 0;
}

/**
 * ipa_cfg_ep_deaggr() -  IPA end-point deaggregation configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ep_deaggr:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_deaggr(u32 clnt_hdl,
			const struct ipa_ep_cfg_deaggr *ep_deaggr)
{
	struct ipa_ep_context *ep;

	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ep_deaggr == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
				clnt_hdl, ipa_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d deaggr_hdr_len=%d\n",
		clnt_hdl,
		ep_deaggr->deaggr_hdr_len);

	IPADBG("packet_offset_valid=%d\n",
		ep_deaggr->packet_offset_valid);

	IPADBG("packet_offset_location=%d max_packet_len=%d\n",
		ep_deaggr->packet_offset_location,
		ep_deaggr->max_packet_len);

	ep = &ipa_ctx->ep[clnt_hdl];

	/* copy over EP cfg */
	ep->cfg.deaggr = *ep_deaggr;

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_ep_deaggr(clnt_hdl, &ep->cfg.deaggr);

	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_deaggr);

static void _ipa_cfg_ep_metadata_v1_1(u32 pipe_number,
					const struct ipa_ep_cfg_metadata *meta)
{
	IPADBG("Not supported for version 1.1\n");
}

static void _ipa_cfg_ep_metadata_v2_0(u32 pipe_number,
					const struct ipa_ep_cfg_metadata *meta)
{
	u32 reg_val = 0;

	IPA_SETFIELD_IN_REG(reg_val, meta->qmap_id,
			IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_SHFT,
			IPA_ENDP_INIT_HDR_METADATA_n_MUX_ID_BMASK);

	ipa_write_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_HDR_METADATA_n_OFST(pipe_number),
			reg_val);
}

/**
 * ipa_cfg_ep_metadata() - IPA end-point metadata configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_cfg_ep_metadata(u32 clnt_hdl, const struct ipa_ep_cfg_metadata *ep_md)
{
	if (clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0 ||
			ep_md == NULL) {
		IPAERR("bad parm, clnt_hdl = %d , ep_valid = %d\n",
					clnt_hdl, ipa_ctx->ep[clnt_hdl].valid);
		return -EINVAL;
	}

	IPADBG("pipe=%d, mux id=%d\n", clnt_hdl, ep_md->qmap_id);

	/* copy over EP cfg */
	ipa_ctx->ep[clnt_hdl].cfg.meta = *ep_md;

	ipa_inc_client_enable_clks();

	ipa_ctx->ctrl->ipa_cfg_ep_metadata(clnt_hdl, ep_md);
	ipa_ctx->ep[clnt_hdl].cfg.hdr.hdr_metadata_reg_valid = 1;
	ipa_ctx->ctrl->ipa_cfg_ep_hdr(clnt_hdl, &ipa_ctx->ep[clnt_hdl].cfg.hdr);

	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_cfg_ep_metadata);

int ipa_write_qmap_id(struct ipa_ioc_write_qmapid *param_in)
{
	struct ipa_ep_cfg_metadata meta;
	struct ipa_ep_context *ep;
	int ipa_ep_idx;
	int result = -EINVAL;

	if (param_in->client  >= IPA_CLIENT_MAX) {
		IPAERR("bad parm client:%d\n", param_in->client);
		goto fail;
	}

	ipa_ep_idx = ipa_get_ep_mapping(param_in->client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client.\n");
		goto fail;
	}

	ep = &ipa_ctx->ep[ipa_ep_idx];
	if (!ep->valid) {
		IPAERR("EP not allocated.\n");
		goto fail;
	}

	meta.qmap_id = param_in->qmap_id;
	if (param_in->client == IPA_CLIENT_USB_PROD ||
	    param_in->client == IPA_CLIENT_HSIC1_PROD ||
	    param_in->client == IPA_CLIENT_ODU_PROD) {
		result = ipa_cfg_ep_metadata(ipa_ep_idx, &meta);
	} else if (param_in->client == IPA_CLIENT_WLAN1_PROD) {
		ipa_ctx->ep[ipa_ep_idx].cfg.meta = meta;
		result = ipa_write_qmapid_wdi_pipe(ipa_ep_idx, meta.qmap_id);
		if (result)
			IPAERR("qmap_id %d write failed on ep=%d\n",
					meta.qmap_id, ipa_ep_idx);
		result = 0;
	}

fail:
	return result;
}

/**
 * ipa_dump_buff_internal() - dumps buffer for debug purposes
 * @base: buffer base address
 * @phy_base: buffer physical base address
 * @size: size of the buffer
 */
void ipa_dump_buff_internal(void *base, dma_addr_t phy_base, u32 size)
{
	int i;
	u32 *cur = (u32 *)base;
	u8 *byt;
	IPADBG("system phys addr=%pa len=%u\n", &phy_base, size);
	for (i = 0; i < size / 4; i++) {
		byt = (u8 *)(cur + i);
		IPADBG("%2d %08x   %02x %02x %02x %02x\n", i, *(cur + i),
				byt[0], byt[1], byt[2], byt[3]);
	}
	IPADBG("END\n");
}

/**
 * ipa_pipe_mem_init() - initialize the pipe memory
 * @start_ofst: start offset
 * @size: size
 *
 * Return value:
 * 0: success
 * -ENOMEM: no memory
 */
int ipa_pipe_mem_init(u32 start_ofst, u32 size)
{
	int res;
	u32 aligned_start_ofst;
	u32 aligned_size;
	struct gen_pool *pool;

	if (!size) {
		IPAERR("no IPA pipe memory allocated\n");
		goto fail;
	}

	aligned_start_ofst = IPA_HW_TABLE_ALIGNMENT(start_ofst);
	aligned_size = size - (aligned_start_ofst - start_ofst);

	IPADBG("start_ofst=%u aligned_start_ofst=%u size=%u aligned_size=%u\n",
	       start_ofst, aligned_start_ofst, size, aligned_size);

	/* allocation order of 8 i.e. 128 bytes, global pool */
	pool = gen_pool_create(8, -1);
	if (!pool) {
		IPAERR("Failed to create a new memory pool.\n");
		goto fail;
	}

	res = gen_pool_add(pool, aligned_start_ofst, aligned_size, -1);
	if (res) {
		IPAERR("Failed to add memory to IPA pipe pool\n");
		goto err_pool_add;
	}

	ipa_ctx->pipe_mem_pool = pool;
	return 0;

err_pool_add:
	gen_pool_destroy(pool);
fail:
	return -ENOMEM;
}

/**
 * ipa_pipe_mem_alloc() - allocate pipe memory
 * @ofst: offset
 * @size: size
 *
 * Return value:
 * 0: success
 */
int ipa_pipe_mem_alloc(u32 *ofst, u32 size)
{
	u32 vaddr;
	int res = -1;

	if (!ipa_ctx->pipe_mem_pool || !size) {
		IPAERR("failed size=%u pipe_mem_pool=%p\n", size,
				ipa_ctx->pipe_mem_pool);
		return res;
	}

	vaddr = gen_pool_alloc(ipa_ctx->pipe_mem_pool, size);

	if (vaddr) {
		*ofst = vaddr;
		res = 0;
		IPADBG("size=%u ofst=%u\n", size, vaddr);
	} else {
		IPAERR("size=%u failed\n", size);
	}

	return res;
}

/**
 * ipa_pipe_mem_free() - free pipe memory
 * @ofst: offset
 * @size: size
 *
 * Return value:
 * 0: success
 */
int ipa_pipe_mem_free(u32 ofst, u32 size)
{
	IPADBG("size=%u ofst=%u\n", size, ofst);
	if (ipa_ctx->pipe_mem_pool && size)
		gen_pool_free(ipa_ctx->pipe_mem_pool, ofst, size);
	return 0;
}

/**
 * ipa_set_aggr_mode() - Set the aggregation mode which is a global setting
 * @mode:	[in] the desired aggregation mode for e.g. straight MBIM, QCNCM,
 * etc
 *
 * Returns:	0 on success
 */
int ipa_set_aggr_mode(enum ipa_aggr_mode mode)
{
	u32 reg_val;

	ipa_inc_client_enable_clks();
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0) {
		reg_val = ipa_read_reg(ipa_ctx->mmio,
				IPA_AGGREGATION_SPARE_REG_2_OFST);
		ipa_write_reg(ipa_ctx->mmio,
				IPA_AGGREGATION_SPARE_REG_2_OFST,
				((mode & IPA_AGGREGATION_MODE_MSK) <<
					IPA_AGGREGATION_MODE_SHFT) |
					(reg_val & IPA_AGGREGATION_MODE_BMSK));
	} else {
		reg_val = ipa_read_reg(ipa_ctx->mmio, IPA_QCNCM_OFST);
		ipa_write_reg(ipa_ctx->mmio, IPA_QCNCM_OFST, (mode & 0x1) |
				(reg_val & 0xfffffffe));

	}
	ipa_dec_client_disable_clks();
	return 0;
}
EXPORT_SYMBOL(ipa_set_aggr_mode);

/**
 * ipa_set_qcncm_ndp_sig() - Set the NDP signature used for QCNCM aggregation
 * mode
 * @sig:	[in] the first 3 bytes of QCNCM NDP signature (expected to be
 * "QND")
 *
 * Set the NDP signature used for QCNCM aggregation mode. The fourth byte
 * (expected to be 'P') needs to be set using the header addition mechanism
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_set_qcncm_ndp_sig(char sig[3])
{
	u32 reg_val;

	if (sig == NULL) {
		IPAERR("bad argument for ipa_set_qcncm_ndp_sig/n");
		return -EINVAL;
	}
	ipa_inc_client_enable_clks();
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0) {
		reg_val = ipa_read_reg(ipa_ctx->mmio,
				IPA_AGGREGATION_SPARE_REG_2_OFST);
		ipa_write_reg(ipa_ctx->mmio,
				IPA_AGGREGATION_SPARE_REG_2_OFST, sig[0] <<
				IPA_AGGREGATION_QCNCM_SIG0_SHFT |
				(sig[1] << IPA_AGGREGATION_QCNCM_SIG1_SHFT) |
				sig[2] |
				(reg_val & IPA_AGGREGATION_QCNCM_SIG_BMSK));
	} else {
		reg_val = ipa_read_reg(ipa_ctx->mmio, IPA_QCNCM_OFST);
		ipa_write_reg(ipa_ctx->mmio, IPA_QCNCM_OFST, sig[0] << 20 |
				(sig[1] << 12) | (sig[2] << 4) |
				(reg_val & 0xf000000f));
	}
	ipa_dec_client_disable_clks();
	return 0;
}
EXPORT_SYMBOL(ipa_set_qcncm_ndp_sig);

/**
 * ipa_set_single_ndp_per_mbim() - Enable/disable single NDP per MBIM frame
 * configuration
 * @enable:	[in] true for single NDP/MBIM; false otherwise
 *
 * Returns:	0 on success
 */
int ipa_set_single_ndp_per_mbim(bool enable)
{
	u32 reg_val;

	ipa_inc_client_enable_clks();
	if (ipa_ctx->ipa_hw_type == IPA_HW_v1_0) {
		reg_val = ipa_read_reg(ipa_ctx->mmio,
				IPA_AGGREGATION_SPARE_REG_1_OFST);
		ipa_write_reg(ipa_ctx->mmio,
				IPA_AGGREGATION_SPARE_REG_1_OFST, (enable &
				IPA_AGGREGATION_SINGLE_NDP_MSK) |
				(reg_val & IPA_AGGREGATION_SINGLE_NDP_BMSK));
	} else {
		reg_val = ipa_read_reg(ipa_ctx->mmio, IPA_SINGLE_NDP_MODE_OFST);
		ipa_write_reg(ipa_ctx->mmio, IPA_SINGLE_NDP_MODE_OFST,
				(enable & 0x1) | (reg_val & 0xfffffffe));
	}
	ipa_dec_client_disable_clks();
	return 0;
}
EXPORT_SYMBOL(ipa_set_single_ndp_per_mbim);

/**
 * ipa_set_hw_timer_fix_for_mbim_aggr() - Enable/disable HW timer fix
 * for MBIM aggregation.
 * @enable:	[in] true for enable HW fix; false otherwise
 *
 * Returns:	0 on success
 */
int ipa_set_hw_timer_fix_for_mbim_aggr(bool enable)
{
	u32 reg_val;
	ipa_inc_client_enable_clks();
	reg_val = ipa_read_reg(ipa_ctx->mmio, IPA_AGGREGATION_SPARE_REG_1_OFST);
	ipa_write_reg(ipa_ctx->mmio, IPA_AGGREGATION_SPARE_REG_1_OFST,
		(enable << IPA_AGGREGATION_HW_TIMER_FIX_MBIM_AGGR_SHFT) |
		(reg_val & ~IPA_AGGREGATION_HW_TIMER_FIX_MBIM_AGGR_BMSK));
	ipa_dec_client_disable_clks();
	return 0;
}
EXPORT_SYMBOL(ipa_set_hw_timer_fix_for_mbim_aggr);

/**
 * ipa_straddle_boundary() - Checks whether a memory buffer straddles a boundary
 * @start: start address of the memory buffer
 * @end: end address of the memory buffer
 * @boundary: boundary
 *
 * Return value:
 * 1: if the interval [start, end] straddles boundary
 * 0: otherwise
 */
int ipa_straddle_boundary(u32 start, u32 end, u32 boundary)
{
	u32 next_start;
	u32 prev_end;

	IPADBG("start=%u end=%u boundary=%u\n", start, end, boundary);

	next_start = (start + (boundary - 1)) & ~(boundary - 1);
	prev_end = ((end + (boundary - 1)) & ~(boundary - 1)) - boundary;

	while (next_start < prev_end)
		next_start += boundary;

	if (next_start == prev_end)
		return 1;
	else
		return 0;
}

/**
 * ipa_bam_reg_dump() - Dump selected BAM registers for IPA and DMA-BAM
 *
 * Function is rate limited to avoid flooding kernel log buffer
 */
void ipa_bam_reg_dump(void)
{
	static DEFINE_RATELIMIT_STATE(_rs, 500*HZ, 1);
	if (__ratelimit(&_rs)) {
		ipa_inc_client_enable_clks();
		pr_err("IPA BAM START\n");
		sps_get_bam_debug_info(ipa_ctx->bam_handle, 5, 511950, 0, 0);
		sps_get_bam_debug_info(ipa_ctx->bam_handle, 93, 0, 0, 0);
		ipa_dec_client_disable_clks();
	}
}
EXPORT_SYMBOL(ipa_bam_reg_dump);

static void ipa_init_mem_partition_v2(void)
{
	IPADBG("Memory partition IPA 2\n");
	IPA_MEM_PART(nat_ofst) = IPA_RAM_NAT_OFST;
	IPA_MEM_PART(nat_size) = IPA_RAM_NAT_SIZE;
	IPADBG("NAT OFST 0x%x SIZE 0x%x\n", IPA_MEM_PART(nat_ofst),
		IPA_MEM_PART(nat_size));

	IPA_MEM_PART(ofst_start) = IPA_MEM_v2_RAM_OFST_START;
	IPADBG("RAM OFST 0x%x\n", IPA_MEM_PART(ofst_start));

	IPA_MEM_PART(v4_flt_ofst) = IPA_MEM_v2_RAM_V4_FLT_OFST;
	IPA_MEM_PART(v4_flt_size) = IPA_MEM_v2_RAM_V4_FLT_SIZE;
	IPA_MEM_PART(v4_flt_size_ddr) = IPA_MEM_RAM_V4_FLT_SIZE_DDR;
	IPADBG("V4 FLT OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_flt_ofst), IPA_MEM_PART(v4_flt_size),
		IPA_MEM_PART(v4_flt_size_ddr));

	IPA_MEM_PART(v6_flt_ofst) = IPA_MEM_v2_RAM_V6_FLT_OFST;
	IPA_MEM_PART(v6_flt_size) = IPA_MEM_v2_RAM_V6_FLT_SIZE;
	IPA_MEM_PART(v6_flt_size_ddr) = IPA_MEM_RAM_V6_FLT_SIZE_DDR;
	IPADBG("V6 FLT OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_flt_ofst), IPA_MEM_PART(v6_flt_size),
		IPA_MEM_PART(v6_flt_size_ddr));

	IPA_MEM_PART(v4_rt_ofst) = IPA_MEM_v2_RAM_V4_RT_OFST;
	IPADBG("V4 RT OFST 0x%x\n", IPA_MEM_PART(v4_rt_ofst));

	IPA_MEM_PART(v4_num_index) = IPA_MEM_v2_RAM_V4_NUM_INDEX;
	IPADBG("V4 RT NUM INDEX 0x%x\n", IPA_MEM_PART(v4_num_index));

	IPA_MEM_PART(v4_modem_rt_index_lo) = IPA_MEM_v2_V4_MODEM_RT_INDEX_LO;
	IPA_MEM_PART(v4_modem_rt_index_hi) = IPA_MEM_v2_V4_MODEM_RT_INDEX_HI;
	IPADBG("V4 RT MODEM INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v4_modem_rt_index_lo),
		IPA_MEM_PART(v4_modem_rt_index_hi));

	IPA_MEM_PART(v4_apps_rt_index_lo) = IPA_MEM_v2_V4_APPS_RT_INDEX_LO;
	IPA_MEM_PART(v4_apps_rt_index_hi) = IPA_MEM_v2_V4_APPS_RT_INDEX_HI;
	IPADBG("V4 RT APPS INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v4_apps_rt_index_lo),
		IPA_MEM_PART(v4_apps_rt_index_hi));

	IPA_MEM_PART(v4_rt_size) = IPA_MEM_v2_RAM_V4_RT_SIZE;
	IPA_MEM_PART(v4_rt_size_ddr) = IPA_MEM_RAM_V4_RT_SIZE_DDR;
	IPADBG("V4 RT SIZE 0x%x DDR SIZE 0x%x\n", IPA_MEM_PART(v4_rt_size),
		IPA_MEM_PART(v4_rt_size_ddr));

	IPA_MEM_PART(v6_rt_ofst) = IPA_MEM_v2_RAM_V6_RT_OFST;
	IPADBG("V6 RT OFST 0x%x\n", IPA_MEM_PART(v6_rt_ofst));

	IPA_MEM_PART(v6_num_index) = IPA_MEM_v2_RAM_V6_NUM_INDEX;
	IPADBG("V6 RT NUM INDEX 0x%x\n", IPA_MEM_PART(v6_num_index));

	IPA_MEM_PART(v6_modem_rt_index_lo) = IPA_MEM_v2_V6_MODEM_RT_INDEX_LO;
	IPA_MEM_PART(v6_modem_rt_index_hi) = IPA_MEM_v2_V6_MODEM_RT_INDEX_HI;
	IPADBG("V6 RT MODEM INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v6_modem_rt_index_lo),
		IPA_MEM_PART(v6_modem_rt_index_hi));

	IPA_MEM_PART(v6_apps_rt_index_lo) = IPA_MEM_v2_V6_APPS_RT_INDEX_LO;
	IPA_MEM_PART(v6_apps_rt_index_hi) = IPA_MEM_v2_V6_APPS_RT_INDEX_HI;
	IPADBG("V6 RT APPS INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v6_apps_rt_index_lo),
		IPA_MEM_PART(v6_apps_rt_index_hi));

	IPA_MEM_PART(v6_rt_size) = IPA_MEM_v2_RAM_V6_RT_SIZE;
	IPA_MEM_PART(v6_rt_size_ddr) = IPA_MEM_RAM_V6_RT_SIZE_DDR;
	IPADBG("V6 RT SIZE 0x%x DDR SIZE 0x%x\n", IPA_MEM_PART(v6_rt_size),
		IPA_MEM_PART(v6_rt_size_ddr));

	IPA_MEM_PART(modem_hdr_ofst) = IPA_MEM_v2_RAM_MODEM_HDR_OFST;
	IPA_MEM_PART(modem_hdr_size) = IPA_MEM_v2_RAM_MODEM_HDR_SIZE;
	IPADBG("MODEM HDR OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(modem_hdr_ofst), IPA_MEM_PART(modem_hdr_size));

	IPA_MEM_PART(apps_hdr_ofst) = IPA_MEM_v2_RAM_APPS_HDR_OFST;
	IPA_MEM_PART(apps_hdr_size) = IPA_MEM_v2_RAM_APPS_HDR_SIZE;
	IPA_MEM_PART(apps_hdr_size_ddr) = IPA_MEM_v2_RAM_HDR_SIZE_DDR;
	IPADBG("APPS HDR OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(apps_hdr_ofst), IPA_MEM_PART(apps_hdr_size),
		IPA_MEM_PART(apps_hdr_size_ddr));

	IPA_MEM_PART(modem_ofst) = IPA_MEM_v2_RAM_MODEM_OFST;
	IPA_MEM_PART(modem_size) = IPA_MEM_v2_RAM_MODEM_SIZE;
	IPADBG("MODEM OFST 0x%x SIZE 0x%x\n", IPA_MEM_PART(modem_ofst),
		IPA_MEM_PART(modem_size));

	IPA_MEM_PART(apps_v4_flt_ofst) = IPA_MEM_v2_RAM_APPS_V4_FLT_OFST;
	IPA_MEM_PART(apps_v4_flt_size) = IPA_MEM_v2_RAM_APPS_V4_FLT_SIZE;
	IPADBG("V4 APPS FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v4_flt_ofst), IPA_MEM_PART(apps_v4_flt_size));

	IPA_MEM_PART(apps_v6_flt_ofst) = IPA_MEM_v2_RAM_APPS_V6_FLT_OFST;
	IPA_MEM_PART(apps_v6_flt_size) = IPA_MEM_v2_RAM_APPS_V6_FLT_SIZE;
	IPADBG("V6 APPS FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v6_flt_ofst), IPA_MEM_PART(apps_v6_flt_size));

	IPA_MEM_PART(uc_info_ofst) = IPA_MEM_v2_RAM_UC_INFO_OFST;
	IPA_MEM_PART(uc_info_size) = IPA_MEM_v2_RAM_UC_INFO_SIZE;
	IPADBG("V6 UC INFO OFST 0x%x SIZE 0x%x\n", IPA_MEM_PART(uc_info_ofst),
		IPA_MEM_PART(uc_info_size));

	IPA_MEM_PART(end_ofst) = IPA_MEM_v2_RAM_END_OFST;
	IPA_MEM_PART(apps_v4_rt_ofst) = IPA_MEM_v2_RAM_APPS_V4_RT_OFST;
	IPA_MEM_PART(apps_v4_rt_size) = IPA_MEM_v2_RAM_APPS_V4_RT_SIZE;
	IPA_MEM_PART(apps_v6_rt_ofst) = IPA_MEM_v2_RAM_APPS_V6_RT_OFST;
	IPA_MEM_PART(apps_v6_rt_size) = IPA_MEM_v2_RAM_APPS_V6_RT_SIZE;
}

static void ipa_init_mem_partition_v2_5(void)
{
	IPADBG("Memory partition IPA 2.5\n");
	IPA_MEM_PART(nat_ofst) = IPA_RAM_NAT_OFST;
	IPA_MEM_PART(nat_size) = IPA_RAM_NAT_SIZE;
	IPADBG("NAT OFST 0x%x SIZE 0x%x\n", IPA_MEM_PART(nat_ofst),
		IPA_MEM_PART(nat_size));

	IPA_MEM_PART(ofst_start) = IPA_MEM_v2_5_RAM_OFST_START;
	IPADBG("RAM OFST 0x%x\n", IPA_MEM_PART(ofst_start));

	IPA_MEM_PART(v4_flt_ofst) = IPA_MEM_v2_5_RAM_V4_FLT_OFST;
	IPA_MEM_PART(v4_flt_size) = IPA_MEM_v2_5_RAM_V4_FLT_SIZE;
	IPA_MEM_PART(v4_flt_size_ddr) = IPA_MEM_RAM_V4_FLT_SIZE_DDR;
	IPADBG("V4 FLT OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v4_flt_ofst), IPA_MEM_PART(v4_flt_size),
		IPA_MEM_PART(v4_flt_size_ddr));

	IPA_MEM_PART(v6_flt_ofst) = IPA_MEM_v2_5_RAM_V6_FLT_OFST;
	IPA_MEM_PART(v6_flt_size) = IPA_MEM_v2_5_RAM_V6_FLT_SIZE;
	IPA_MEM_PART(v6_flt_size_ddr) = IPA_MEM_RAM_V6_FLT_SIZE_DDR;
	IPADBG("V6 FLT OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(v6_flt_ofst), IPA_MEM_PART(v6_flt_size),
		IPA_MEM_PART(v6_flt_size_ddr));

	IPA_MEM_PART(v4_rt_ofst) = IPA_MEM_v2_5_RAM_V4_RT_OFST;
	IPADBG("V4 RT OFST 0x%x\n", IPA_MEM_PART(v4_rt_ofst));

	IPA_MEM_PART(v4_num_index) = IPA_MEM_v2_5_RAM_V4_NUM_INDEX;
	IPADBG("V4 RT NUM INDEX 0x%x\n", IPA_MEM_PART(v4_num_index));

	IPA_MEM_PART(v4_modem_rt_index_lo) = IPA_MEM_v2_5_V4_MODEM_RT_INDEX_LO;
	IPA_MEM_PART(v4_modem_rt_index_hi) = IPA_MEM_v2_5_V4_MODEM_RT_INDEX_HI;
	IPADBG("V4 RT MODEM INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v4_modem_rt_index_lo),
		IPA_MEM_PART(v4_modem_rt_index_hi));

	IPA_MEM_PART(v4_apps_rt_index_lo) = IPA_MEM_v2_5_V4_APPS_RT_INDEX_LO;
	IPA_MEM_PART(v4_apps_rt_index_hi) = IPA_MEM_v2_5_V4_APPS_RT_INDEX_HI;
	IPADBG("V4 RT APPS INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v4_apps_rt_index_lo),
		IPA_MEM_PART(v4_apps_rt_index_hi));

	IPA_MEM_PART(v4_rt_size) = IPA_MEM_v2_5_RAM_V4_RT_SIZE;
	IPA_MEM_PART(v4_rt_size_ddr) = IPA_MEM_RAM_V4_RT_SIZE_DDR;
	IPADBG("V4 RT SIZE 0x%x DDR SIZE 0x%x\n", IPA_MEM_PART(v4_rt_size),
		IPA_MEM_PART(v4_rt_size_ddr));

	IPA_MEM_PART(v6_rt_ofst) = IPA_MEM_v2_5_RAM_V6_RT_OFST;
	IPADBG("V6 RT OFST 0x%x\n", IPA_MEM_PART(v6_rt_ofst));

	IPA_MEM_PART(v6_num_index) = IPA_MEM_v2_5_RAM_V6_NUM_INDEX;
	IPADBG("V6 RT NUM INDEX 0x%x\n", IPA_MEM_PART(v6_num_index));

	IPA_MEM_PART(v6_modem_rt_index_lo) = IPA_MEM_v2_5_V6_MODEM_RT_INDEX_LO;
	IPA_MEM_PART(v6_modem_rt_index_hi) = IPA_MEM_v2_5_V6_MODEM_RT_INDEX_HI;
	IPADBG("V6 RT MODEM INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v6_modem_rt_index_lo),
		IPA_MEM_PART(v6_modem_rt_index_hi));

	IPA_MEM_PART(v6_apps_rt_index_lo) = IPA_MEM_v2_5_V6_APPS_RT_INDEX_LO;
	IPA_MEM_PART(v6_apps_rt_index_hi) = IPA_MEM_v2_5_V6_APPS_RT_INDEX_HI;
	IPADBG("V6 RT APPS INDEXES 0x%x - 0x%x\n",
		IPA_MEM_PART(v6_apps_rt_index_lo),
		IPA_MEM_PART(v6_apps_rt_index_hi));

	IPA_MEM_PART(v6_rt_size) = IPA_MEM_v2_5_RAM_V6_RT_SIZE;
	IPA_MEM_PART(v6_rt_size_ddr) = IPA_MEM_RAM_V6_RT_SIZE_DDR;
	IPADBG("V6 RT SIZE 0x%x DDR SIZE 0x%x\n", IPA_MEM_PART(v6_rt_size),
		IPA_MEM_PART(v6_rt_size_ddr));

	IPA_MEM_PART(modem_hdr_ofst) = IPA_MEM_v2_5_RAM_MODEM_HDR_OFST;
	IPA_MEM_PART(modem_hdr_size) = IPA_MEM_v2_5_RAM_MODEM_HDR_SIZE;
	IPADBG("MODEM HDR OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(modem_hdr_ofst), IPA_MEM_PART(modem_hdr_size));

	IPA_MEM_PART(apps_hdr_ofst) = IPA_MEM_v2_5_RAM_APPS_HDR_OFST;
	IPA_MEM_PART(apps_hdr_size) = IPA_MEM_v2_5_RAM_APPS_HDR_SIZE;
	IPA_MEM_PART(apps_hdr_size_ddr) = IPA_MEM_v2_5_RAM_HDR_SIZE_DDR;
	IPADBG("APPS HDR OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(apps_hdr_ofst), IPA_MEM_PART(apps_hdr_size),
		IPA_MEM_PART(apps_hdr_size_ddr));

	IPA_MEM_PART(modem_hdr_proc_ctx_ofst) =
		IPA_MEM_v2_5_RAM_MODEM_HDR_PROC_CTX_OFST;
	IPA_MEM_PART(modem_hdr_proc_ctx_size) =
		IPA_MEM_v2_5_RAM_MODEM_HDR_PROC_CTX_SIZE;
	IPADBG("MODEM HDR PROC CTX OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(modem_hdr_proc_ctx_ofst),
		IPA_MEM_PART(modem_hdr_proc_ctx_size));

	IPA_MEM_PART(apps_hdr_proc_ctx_ofst) =
		IPA_MEM_v2_5_RAM_APPS_HDR_PROC_CTX_OFST;
	IPA_MEM_PART(apps_hdr_proc_ctx_size) =
		IPA_MEM_v2_5_RAM_APPS_HDR_PROC_CTX_SIZE;
	IPA_MEM_PART(apps_hdr_proc_ctx_size_ddr) =
		IPA_MEM_RAM_HDR_PROC_CTX_SIZE_DDR;
	IPADBG("APPS HDR PROC CTX OFST 0x%x SIZE 0x%x DDR SIZE 0x%x\n",
		IPA_MEM_PART(apps_hdr_proc_ctx_ofst),
		IPA_MEM_PART(apps_hdr_proc_ctx_size),
		IPA_MEM_PART(apps_hdr_proc_ctx_size_ddr));

	IPA_MEM_PART(modem_ofst) = IPA_MEM_v2_5_RAM_MODEM_OFST;
	IPA_MEM_PART(modem_size) = IPA_MEM_v2_5_RAM_MODEM_SIZE;
	IPADBG("MODEM OFST 0x%x SIZE 0x%x\n", IPA_MEM_PART(modem_ofst),
		IPA_MEM_PART(modem_size));

	IPA_MEM_PART(apps_v4_flt_ofst) = IPA_MEM_v2_5_RAM_APPS_V4_FLT_OFST;
	IPA_MEM_PART(apps_v4_flt_size) = IPA_MEM_v2_5_RAM_APPS_V4_FLT_SIZE;
	IPADBG("V4 APPS FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v4_flt_ofst), IPA_MEM_PART(apps_v4_flt_size));

	IPA_MEM_PART(apps_v6_flt_ofst) = IPA_MEM_v2_5_RAM_APPS_V6_FLT_OFST;
	IPA_MEM_PART(apps_v6_flt_size) = IPA_MEM_v2_5_RAM_APPS_V6_FLT_SIZE;
	IPADBG("V6 APPS FLT OFST 0x%x SIZE 0x%x\n",
		IPA_MEM_PART(apps_v6_flt_ofst), IPA_MEM_PART(apps_v6_flt_size));

	IPA_MEM_PART(uc_info_ofst) = IPA_MEM_v2_5_RAM_UC_INFO_OFST;
	IPA_MEM_PART(uc_info_size) = IPA_MEM_v2_5_RAM_UC_INFO_SIZE;
	IPADBG("V6 UC INFO OFST 0x%x SIZE 0x%x\n", IPA_MEM_PART(uc_info_ofst),
		IPA_MEM_PART(uc_info_size));

	IPA_MEM_PART(end_ofst) = IPA_MEM_v2_5_RAM_END_OFST;
	IPA_MEM_PART(apps_v4_rt_ofst) = IPA_MEM_v2_5_RAM_APPS_V4_RT_OFST;
	IPA_MEM_PART(apps_v4_rt_size) = IPA_MEM_v2_5_RAM_APPS_V4_RT_SIZE;
	IPA_MEM_PART(apps_v6_rt_ofst) = IPA_MEM_v2_5_RAM_APPS_V6_RT_OFST;
	IPA_MEM_PART(apps_v6_rt_size) = IPA_MEM_v2_5_RAM_APPS_V6_RT_SIZE;
}

/**
 * ipa_controller_shared_static_bind() - set the appropriate shared methods for
 * for IPA HW version 2.0 and 2.5
 *
 *  @ctrl: data structure which holds the function pointers
 */
void ipa_controller_shared_static_bind(struct ipa_controller *ctrl)
{
	ctrl->ipa_init_rt4 = _ipa_init_rt4_v2;
	ctrl->ipa_init_rt6 = _ipa_init_rt6_v2;
	ctrl->ipa_init_flt4 = _ipa_init_flt4_v2;
	ctrl->ipa_init_flt6 = _ipa_init_flt6_v2;
	ctrl->ipa_cfg_ep_hdr = _ipa_cfg_ep_hdr_v2_0;
	ctrl->ipa_cfg_ep_nat = _ipa_cfg_ep_nat_v2_0;
	ctrl->ipa_cfg_ep_aggr = _ipa_cfg_ep_aggr_v2_0;
	ctrl->ipa_cfg_ep_deaggr = _ipa_cfg_ep_deaggr_v2_0;
	ctrl->ipa_cfg_ep_mode = _ipa_cfg_ep_mode_v2_0;
	ctrl->ipa_cfg_ep_route = _ipa_cfg_ep_route_v2_0;
	ctrl->ipa_cfg_route = _ipa_cfg_route_v2_0;
	ctrl->ipa_cfg_ep_status = _ipa_cfg_ep_status_v2_0;
	ctrl->ipa_cfg_ep_cfg = _ipa_cfg_ep_cfg_v2_0;
	ctrl->ipa_cfg_ep_metadata_mask = _ipa_cfg_ep_metadata_mask_v2_0;
	ctrl->ipa_clk_rate_hi = IPA_V2_0_CLK_RATE_HIGH;
	ctrl->ipa_clk_rate_lo = IPA_V2_0_CLK_RATE_LOW;
	ctrl->ipa_read_gen_reg = _ipa_read_gen_reg_v2_0;
	ctrl->ipa_read_ep_reg = _ipa_read_ep_reg_v2_0;
	ctrl->ipa_write_dbg_cnt = _ipa_write_dbg_cnt_v2_0;
	ctrl->ipa_read_dbg_cnt = _ipa_read_dbg_cnt_v2_0;
	ctrl->ipa_commit_flt = __ipa_commit_flt_v2;
	ctrl->ipa_commit_rt = __ipa_commit_rt_v2;
	ctrl->ipa_commit_hdr = __ipa_commit_hdr_v2;
	ctrl->ipa_enable_clks = _ipa_enable_clks_v2_0;
	ctrl->ipa_disable_clks = _ipa_disable_clks_v2_0;
	ctrl->msm_bus_data_ptr = &ipa_bus_client_pdata_v2_0;
	ctrl->ipa_cfg_ep_metadata = _ipa_cfg_ep_metadata_v2_0;
	ctrl->clock_scaling_bw_threshold = IPA_V2_0_BW_THRESHOLD_MBPS;
}

/**
 * ipa_ctrl_static_bind() - set the appropriate methods for
 *  IPA Driver based on the HW version
 *
 *  @ctrl: data structure which holds the function pointers
 *  @hw_type: the HW type in use
 *
 *  This function can avoid the runtime assignment by using C99 special
 *  struct initialization - hard decision... time.vs.mem
 */
int ipa_controller_static_bind(struct ipa_controller *ctrl,
		enum ipa_hw_type hw_type)
{
	switch (hw_type) {
	case (IPA_HW_v1_0):
		ctrl->ipa_sram_read_settings = _ipa_sram_settings_read_v1_0;
		ctrl->ipa_cfg_ep_hdr = _ipa_cfg_ep_hdr_v1_1;
		ctrl->ipa_cfg_ep_hdr_ext = _ipa_cfg_ep_hdr_ext_v1_1;
		ctrl->ipa_cfg_ep_aggr = _ipa_cfg_ep_aggr_v1_0;
		ctrl->ipa_cfg_ep_deaggr = _ipa_cfg_ep_deaggr_v1_1;
		ctrl->ipa_cfg_ep_nat = _ipa_cfg_ep_nat_v1_0;
		ctrl->ipa_cfg_ep_mode = _ipa_cfg_ep_mode_v1_0;
		ctrl->ipa_cfg_ep_route = _ipa_cfg_ep_route_v1_0;
		ctrl->ipa_cfg_ep_holb = _ipa_cfg_ep_holb_v1_0;
		ctrl->ipa_cfg_route = _ipa_cfg_route_v1_0;
		ctrl->ipa_cfg_ep_status = _ipa_cfg_ep_status_v1_1;
		ctrl->ipa_cfg_ep_cfg = _ipa_cfg_ep_cfg_v1_1;
		ctrl->ipa_cfg_ep_metadata_mask = _ipa_cfg_ep_metadata_mask_v1_1;
		ctrl->ipa_clk_rate_hi = IPA_V1_CLK_RATE;
		ctrl->ipa_clk_rate_lo = IPA_V1_CLK_RATE;
		ctrl->ipa_read_gen_reg = _ipa_read_gen_reg_v1_0;
		ctrl->ipa_read_ep_reg = _ipa_read_ep_reg_v1_0;
		ctrl->ipa_write_dbg_cnt = _ipa_write_dbg_cnt_v1;
		ctrl->ipa_read_dbg_cnt = _ipa_read_dbg_cnt_v1;
		ctrl->ipa_commit_flt = __ipa_commit_flt_v1;
		ctrl->ipa_commit_rt = __ipa_commit_rt_v1;
		ctrl->ipa_commit_hdr = __ipa_commit_hdr_v1;
		ctrl->ipa_enable_clks = _ipa_enable_clks_v1;
		ctrl->ipa_disable_clks = _ipa_disable_clks_v1;
		ctrl->msm_bus_data_ptr = &ipa_bus_client_pdata_v1_1;
		ctrl->ipa_cfg_ep_metadata = _ipa_cfg_ep_metadata_v1_1;
		ctrl->ipa_reg_base_ofst = IPA_REG_BASE_OFST_v2_0;
		ctrl->max_holb_tmr_val = IPA_V1_MAX_HOLB_TMR_VAL;
		break;
	case (IPA_HW_v1_1):
		ipa_init_mem_partition_v2();
		ctrl->ipa_sram_read_settings = _ipa_sram_settings_read_v1_1;
		ctrl->ipa_cfg_ep_hdr = _ipa_cfg_ep_hdr_v1_1;
		ctrl->ipa_cfg_ep_hdr_ext = _ipa_cfg_ep_hdr_ext_v1_1;
		ctrl->ipa_cfg_ep_aggr = _ipa_cfg_ep_aggr_v1_1;
		ctrl->ipa_cfg_ep_deaggr = _ipa_cfg_ep_deaggr_v1_1;
		ctrl->ipa_cfg_ep_nat = _ipa_cfg_ep_nat_v1_1;
		ctrl->ipa_cfg_ep_mode = _ipa_cfg_ep_mode_v1_1;
		ctrl->ipa_cfg_ep_route = _ipa_cfg_ep_route_v1_1;
		ctrl->ipa_cfg_ep_holb = _ipa_cfg_ep_holb_v1_1;
		ctrl->ipa_cfg_route = _ipa_cfg_route_v1_1;
		ctrl->ipa_cfg_ep_status = _ipa_cfg_ep_status_v1_1;
		ctrl->ipa_cfg_ep_cfg = _ipa_cfg_ep_cfg_v1_1;
		ctrl->ipa_cfg_ep_metadata_mask = _ipa_cfg_ep_metadata_mask_v1_1;
		ctrl->ipa_clk_rate_hi = IPA_V1_1_CLK_RATE;
		ctrl->ipa_clk_rate_lo = IPA_V1_1_CLK_RATE;
		ctrl->ipa_read_gen_reg = _ipa_read_gen_reg_v1_1;
		ctrl->ipa_read_ep_reg = _ipa_read_ep_reg_v1_1;
		ctrl->ipa_write_dbg_cnt = _ipa_write_dbg_cnt_v1;
		ctrl->ipa_read_dbg_cnt = _ipa_read_dbg_cnt_v1;
		ctrl->ipa_commit_flt = __ipa_commit_flt_v1;
		ctrl->ipa_commit_rt = __ipa_commit_rt_v1;
		ctrl->ipa_commit_hdr = __ipa_commit_hdr_v1;
		ctrl->ipa_enable_clks = _ipa_enable_clks_v1;
		ctrl->ipa_disable_clks = _ipa_disable_clks_v1;
		ctrl->msm_bus_data_ptr = &ipa_bus_client_pdata_v1_1;
		ctrl->ipa_cfg_ep_metadata = _ipa_cfg_ep_metadata_v1_1;
		ctrl->ipa_reg_base_ofst = IPA_REG_BASE_OFST_v2_0;
		ctrl->max_holb_tmr_val = IPA_V1_MAX_HOLB_TMR_VAL;
		break;
	case (IPA_HW_v2_0):
		ipa_init_mem_partition_v2();
		ipa_controller_shared_static_bind(ctrl);
		ctrl->ipa_cfg_ep_holb = _ipa_cfg_ep_holb_v2_0;
		ctrl->ipa_reg_base_ofst = IPA_REG_BASE_OFST_v2_0;
		ctrl->max_holb_tmr_val = IPA_V2_0_MAX_HOLB_TMR_VAL;
		ctrl->ipa_cfg_ep_hdr_ext = _ipa_cfg_ep_hdr_ext_v2_0;
		ctrl->ipa_sram_read_settings = _ipa_sram_settings_read_v2_0;
		ctrl->ipa_init_sram = _ipa_init_sram_v2;
		ctrl->ipa_init_hdr = _ipa_init_hdr_v2;
		ctrl->ipa_commit_hdr = __ipa_commit_hdr_v2;
		ctrl->ipa_generate_rt_hw_rule = __ipa_generate_rt_hw_rule_v2;
		break;
	case (IPA_HW_v2_5):
		ipa_init_mem_partition_v2_5();
		ipa_controller_shared_static_bind(ctrl);
		ctrl->ipa_cfg_ep_holb = _ipa_cfg_ep_holb_v2_5;
		ctrl->ipa_reg_base_ofst = IPA_REG_BASE_OFST_v2_5;
		ctrl->max_holb_tmr_val = IPA_V2_5_MAX_HOLB_TMR_VAL;
		ctrl->ipa_cfg_ep_hdr_ext = _ipa_cfg_ep_hdr_ext_v2_5;
		ctrl->ipa_sram_read_settings = _ipa_sram_settings_read_v2_5;
		ctrl->ipa_init_sram = _ipa_init_sram_v2_5;
		ctrl->ipa_init_hdr = _ipa_init_hdr_v2_5;
		ctrl->ipa_commit_hdr = __ipa_commit_hdr_v2_5;
		ctrl->ipa_generate_rt_hw_rule = __ipa_generate_rt_hw_rule_v2_5;
		break;
	default:
		return -EPERM;
	}

	return 0;
}

void ipa_skb_recycle(struct sk_buff *skb)
{
	struct skb_shared_info *shinfo;

	shinfo = skb_shinfo(skb);
	memset(shinfo, 0, offsetof(struct skb_shared_info, dataref));
	atomic_set(&shinfo->dataref, 1);

	memset(skb, 0, offsetof(struct sk_buff, tail));
	skb->data = skb->head + NET_SKB_PAD;
	skb_reset_tail_pointer(skb);
}

int ipa_id_alloc(void *ptr)
{
	int id;

	idr_preload(GFP_KERNEL);
	spin_lock(&ipa_ctx->idr_lock);
	id = idr_alloc(&ipa_ctx->ipa_idr, ptr, 0, 0, GFP_NOWAIT);
	spin_unlock(&ipa_ctx->idr_lock);
	idr_preload_end();

	return id;
}

void *ipa_id_find(u32 id)
{
	void *ptr;

	spin_lock(&ipa_ctx->idr_lock);
	ptr = idr_find(&ipa_ctx->ipa_idr, id);
	spin_unlock(&ipa_ctx->idr_lock);

	return ptr;
}

void ipa_id_remove(u32 id)
{
	spin_lock(&ipa_ctx->idr_lock);
	idr_remove(&ipa_ctx->ipa_idr, id);
	spin_unlock(&ipa_ctx->idr_lock);
}

static void ipa_tag_free_buf(void *user1, int user2)
{
	kfree(user1);
}

static void ipa_tag_free_skb(void *user1, int user2)
{
	dev_kfree_skb_any((struct sk_buff *)user1);
}

#define REQUIRED_TAG_PROCESS_DESCRIPTORS 4

/* ipa_tag_process() - Initiates a tag process. Incorporates the input
 * descriptors
 *
 * @desc:	descriptors with commands for IC
 * @desc_size:	amount of descriptors in the above variable
 *
 * Note: The descriptors are copied (if there's room), the client needs to
 * free his descriptors afterwards
 *
 * Return: 0 or negative in case of failure
 */
int ipa_tag_process(struct ipa_desc desc[],
	int descs_num,
	unsigned long timeout)
{
	struct ipa_sys_context *sys;
	struct ipa_desc *tag_desc;
	int desc_idx = 0;
	struct ipa_ip_packet_init *pkt_init;
	struct ipa_register_write *reg_write_nop;
	struct ipa_ip_packet_tag_status *status;
	int i;
	struct sk_buff *dummy_skb;
	int res;
	struct ipa_tag_completion *comp;

	/* Not enough room for the required descriptors for the tag process */
	if (IPA_TAG_MAX_DESC - descs_num < REQUIRED_TAG_PROCESS_DESCRIPTORS) {
		IPAERR("up to %d descriptors are allowed (received %d)\n",
		       IPA_TAG_MAX_DESC - REQUIRED_TAG_PROCESS_DESCRIPTORS,
		       descs_num);
		return -ENOMEM;
	}

	sys = ipa_ctx->ep[ipa_get_ep_mapping(IPA_CLIENT_APPS_CMD_PROD)].sys;

	tag_desc = kzalloc(sizeof(*tag_desc) * IPA_TAG_MAX_DESC, GFP_KERNEL);
	if (!tag_desc) {
		IPAERR("failed to allocate memory\n");
		res = -ENOMEM;
		goto fail_alloc_desc;
	}

	/* IP_PACKET_INIT IC for tag status to be sent to apps */
	pkt_init = kzalloc(sizeof(*pkt_init), GFP_KERNEL);
	if (!pkt_init) {
		IPAERR("failed to allocate memory\n");
		res = -ENOMEM;
		goto fail_alloc_pkt_init;
	}

	pkt_init->destination_pipe_index =
		ipa_get_ep_mapping(IPA_CLIENT_APPS_LAN_CONS);

	tag_desc[desc_idx].opcode = IPA_IP_PACKET_INIT;
	tag_desc[desc_idx].pyld = pkt_init;
	tag_desc[desc_idx].len = sizeof(*pkt_init);
	tag_desc[desc_idx].type = IPA_IMM_CMD_DESC;
	tag_desc[desc_idx].callback = ipa_tag_free_buf;
	tag_desc[desc_idx].user1 = pkt_init;
	desc_idx++;

	/* NO-OP IC for ensuring that IPA pipeline is empty */
	reg_write_nop = kzalloc(sizeof(*reg_write_nop), GFP_KERNEL);
	if (!reg_write_nop) {
		IPAERR("no mem\n");
		res = -ENOMEM;
		goto fail_free_desc;
	}

	reg_write_nop->skip_pipeline_clear = 0;
	reg_write_nop->value_mask = 0x0;

	tag_desc[desc_idx].opcode = IPA_REGISTER_WRITE;
	tag_desc[desc_idx].pyld = reg_write_nop;
	tag_desc[desc_idx].len = sizeof(*reg_write_nop);
	tag_desc[desc_idx].type = IPA_IMM_CMD_DESC;
	tag_desc[desc_idx].callback = ipa_tag_free_buf;
	tag_desc[desc_idx].user1 = reg_write_nop;
	desc_idx++;

	/* status IC */
	status = kzalloc(sizeof(*status), GFP_KERNEL);
	if (!status) {
		IPAERR("no mem\n");
		res = -ENOMEM;
		goto fail_free_desc;
	}

	status->tag_f_2 = IPA_COOKIE;

	tag_desc[desc_idx].opcode = IPA_IP_PACKET_TAG_STATUS;
	tag_desc[desc_idx].pyld = status;
	tag_desc[desc_idx].len = sizeof(*status);
	tag_desc[desc_idx].type = IPA_IMM_CMD_DESC;
	tag_desc[desc_idx].callback = ipa_tag_free_buf;
	tag_desc[desc_idx].user1 = status;
	desc_idx++;

	/* Copy the required descriptors from the client now */
	memcpy(&(tag_desc[desc_idx]), desc, descs_num *
		sizeof(struct ipa_desc));
	desc_idx += descs_num;

	comp = kzalloc(sizeof(*comp), GFP_KERNEL);
	if (!comp) {
		IPAERR("no mem\n");
		res = -ENOMEM;
		goto fail_free_desc;
	}
	init_completion(&comp->comp);

	/* completion needs to be released from both here and rx handler */
	atomic_set(&comp->cnt, 2);

	/* dummy packet to send to IPA. packet payload is a completion object */
	dummy_skb = alloc_skb(sizeof(comp), GFP_KERNEL);
	if (!dummy_skb) {
		IPAERR("failed to allocate memory\n");
		res = -ENOMEM;
		goto fail_free_skb;
	}

	memcpy(skb_put(dummy_skb, sizeof(comp)), &comp, sizeof(comp));

	tag_desc[desc_idx].pyld = dummy_skb->data;
	tag_desc[desc_idx].len = dummy_skb->len;
	tag_desc[desc_idx].type = IPA_DATA_DESC_SKB;
	tag_desc[desc_idx].callback = ipa_tag_free_skb;
	tag_desc[desc_idx].user1 = dummy_skb;
	desc_idx++;

	/* send all descriptors to IPA with single EOT */
	res = ipa_send(sys, desc_idx, tag_desc, true);
	if (res) {
		IPAERR("failed to send TAG packets %d\n", res);
		res = -ENOMEM;
		goto fail_send;
	}
	kfree(tag_desc);
	tag_desc = NULL;

	IPADBG("waiting for TAG response\n");
	res = wait_for_completion_timeout(&comp->comp, timeout);
	if (res == 0) {
		IPAERR("timeout for waiting for TAG response\n");
		WARN_ON(1);
		if (atomic_dec_return(&comp->cnt) == 0)
			kfree(comp);
		return -ETIME;
	}

	IPADBG("TAG response arrived!\n");
	if (atomic_dec_return(&comp->cnt) == 0)
		kfree(comp);

	/* sleep for short period to ensure IPA wrote all packets to BAM */
	usleep_range(IPA_TAG_SLEEP_MIN_USEC, IPA_TAG_SLEEP_MAX_USEC);

	return 0;

fail_send:
	dev_kfree_skb_any(dummy_skb);
	desc_idx--;
fail_free_skb:
	kfree(comp);
fail_free_desc:
	/*
	 * Free only the first descriptors allocated here.
	 * [pkt_init, status, nop]
	 * The user is responsible to free his allocations
	 * in case of failure.
	 * The min is required because we may fail during
	 * of the initial allocations above
	 */
	for (i = 0; i < min(REQUIRED_TAG_PROCESS_DESCRIPTORS-1, desc_idx); i++)
		kfree(tag_desc[i].user1);

fail_alloc_pkt_init:
	kfree(tag_desc);
fail_alloc_desc:
	return res;
}

/**
 * ipa_tag_generate_force_close_desc() - generate descriptors for force close
 *					 immediate command
 *
 * @desc: descriptors for IC
 * @desc_size: desc array size
 * @start_pipe: first pipe to close aggregation
 * @end_pipe: last (non-inclusive) pipe to close aggregation
 *
 * Return: number of descriptors written or negative in case of failure
 */
static int ipa_tag_generate_force_close_desc(struct ipa_desc desc[],
	int desc_size, int start_pipe, int end_pipe)
{
	int i;
	u32 aggr_init;
	int desc_idx = 0;
	int res;
	struct ipa_register_write *reg_write_agg_close;

	for (i = start_pipe; i < end_pipe; i++) {
		aggr_init = ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_AGGR_N_OFST_v2_0(i));
		if (((aggr_init & IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK) >>
			IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT) != IPA_ENABLE_AGGR)
			continue;
		IPADBG("Force close ep: %d\n", i);
		if (desc_idx + 1 > desc_size) {
			IPAERR("Internal error - no descriptors\n");
			res = -EFAULT;
			goto fail_no_desc;
		}

		reg_write_agg_close = kzalloc(sizeof(*reg_write_agg_close),
			GFP_KERNEL);
		if (!reg_write_agg_close) {
			IPAERR("no mem\n");
			res = -ENOMEM;
			goto fail_alloc_reg_write_agg_close;
		}

		reg_write_agg_close->skip_pipeline_clear = 0;
		reg_write_agg_close->offset = IPA_ENDP_INIT_AGGR_N_OFST_v2_0(i);
		reg_write_agg_close->value =
			(1 & IPA_ENDP_INIT_AGGR_n_AGGR_FORCE_CLOSE_BMSK) <<
			IPA_ENDP_INIT_AGGR_n_AGGR_FORCE_CLOSE_SHFT;
		reg_write_agg_close->value_mask =
			IPA_ENDP_INIT_AGGR_n_AGGR_FORCE_CLOSE_BMSK <<
			IPA_ENDP_INIT_AGGR_n_AGGR_FORCE_CLOSE_SHFT;

		desc[desc_idx].opcode = IPA_REGISTER_WRITE;
		desc[desc_idx].pyld = reg_write_agg_close;
		desc[desc_idx].len = sizeof(*reg_write_agg_close);
		desc[desc_idx].type = IPA_IMM_CMD_DESC;
		desc[desc_idx].callback = ipa_tag_free_buf;
		desc[desc_idx].user1 = reg_write_agg_close;
		desc_idx++;
	}

	return desc_idx;

fail_alloc_reg_write_agg_close:
	for (i = 0; i < desc_idx; i++)
		kfree(desc[desc_idx].user1);
fail_no_desc:
	return res;
}

/**
 * ipa_tag_aggr_force_close() - Force close aggregation
 *
 * @pipe_num: pipe number or -1 for all pipes
 */
int ipa_tag_aggr_force_close(int pipe_num)
{
	struct ipa_desc *desc;
	int res = -1;
	int start_pipe;
	int end_pipe;
	int num_descs;
	int num_aggr_descs;

	if (pipe_num < -1 || pipe_num >= IPA_NUM_PIPES) {
		IPAERR("Invalid pipe number %d\n", pipe_num);
		return -EINVAL;
	}

	if (pipe_num == -1) {
		start_pipe = 0;
		end_pipe = IPA_NUM_PIPES;
	} else {
		start_pipe = pipe_num;
		end_pipe = pipe_num + 1;
	}

	num_descs = end_pipe - start_pipe;

	desc = kzalloc(sizeof(*desc) * num_descs, GFP_KERNEL);
	if (!desc) {
		IPAERR("no mem\n");
		return -ENOMEM;
	}

	/* Force close aggregation on all valid pipes with aggregation */
	num_aggr_descs = ipa_tag_generate_force_close_desc(desc, num_descs,
						start_pipe, end_pipe);
	if (num_aggr_descs < 0) {
		IPAERR("ipa_tag_generate_force_close_desc failed %d\n",
			num_aggr_descs);
		goto fail_free_desc;
	}

	res = ipa_tag_process(desc, num_aggr_descs,
			      IPA_FORCE_CLOSE_TAG_PROCESS_TIMEOUT);

fail_free_desc:
	kfree(desc);

	return res;
}

/**
 * ipa_is_ready() - check if IPA module was initialized
 * successfully
 *
 * Return value: true for yes; false for no
 */
bool ipa_is_ready(void)
{
	return (ipa_ctx != NULL) ? true : false;
}
EXPORT_SYMBOL(ipa_is_ready);

/**
 * ipa_is_client_handle_valid() - check if IPA client handle is valid handle
 *
 * Return value: true for yes; false for no
 */
bool ipa_is_client_handle_valid(u32 clnt_hdl)
{
	if (clnt_hdl >= 0 && clnt_hdl < IPA_NUM_PIPES)
		return true;
	return false;
}
EXPORT_SYMBOL(ipa_is_client_handle_valid);

/**
 * ipa_q6_init_done() - called when q6 ipa initialization is done
 *
 * Return value: none
 */
void ipa_q6_init_done(void)
{
	if (ipa_is_ready() && ipa_ctx->q6_proxy_clk_vote_valid) {
		ipa_dec_client_disable_clks();
		ipa_ctx->q6_proxy_clk_vote_valid = false;
	}
}
EXPORT_SYMBOL(ipa_q6_init_done);

/**
 * ipa_get_hw_type() - Return IPA HW version
 *
 * Return value: enum ipa_hw_type
 */
enum ipa_hw_type ipa_get_hw_type(void)
{
	if (ipa_ctx)
		return ipa_ctx->ipa_hw_type;
	else
		return IPA_HW_None;
}
EXPORT_SYMBOL(ipa_get_hw_type);

u16 ipa_get_smem_restr_bytes(void)
{
	if (ipa_ctx) {
		return ipa_ctx->smem_restricted_bytes;
	} else {
		IPAERR("IPA Driver not initialized\n");
		return 0;
	}
}
EXPORT_SYMBOL(ipa_get_smem_restr_bytes);
