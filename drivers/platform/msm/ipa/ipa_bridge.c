/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/ratelimit.h>
#include <mach/msm_smem.h>
#include "ipa_i.h"

/*
 * EP0 (teth)
 * A2_BAM(1)->(12)DMA_BAM->DMA_BAM(13)->(6)IPA_BAM->IPA_BAM(10)->USB_BAM(0)
 * A2_BAM(0)<-(15)DMA_BAM<-DMA_BAM(14)<-(7)IPA_BAM<-IPA_BAM(11)<-USB_BAM(1)
 *
 * EP2 (emb)
 * A2_BAM(5)->(16)DMA_BAM->DMA_BAM(17)->(8)IPA_BAM->
 * A2_BAM(4)<-(19)DMA_BAM<-DMA_BAM(18)<-(9)IPA_BAM<-
 */

#define A2_TETHERED_PIPE_UL      0
#define DMA_A2_TETHERED_PIPE_UL  15
#define DMA_IPA_TETHERED_PIPE_UL 14
#define A2_TETHERED_PIPE_DL      1
#define DMA_A2_TETHERED_PIPE_DL  12
#define DMA_IPA_TETHERED_PIPE_DL 13

#define A2_EMBEDDED_PIPE_UL      4
#define DMA_A2_EMBEDDED_PIPE_UL  19
#define DMA_IPA_EMBEDDED_PIPE_UL 18
#define A2_EMBEDDED_PIPE_DL      5
#define DMA_A2_EMBEDDED_PIPE_DL  16
#define DMA_IPA_EMBEDDED_PIPE_DL 17

#define IPA_SMEM_PIPE_MEM_SZ 32768

#define IPA_UL_DATA_FIFO_SZ 0xc00
#define IPA_UL_DESC_FIFO_SZ 0x530
#define IPA_DL_DATA_FIFO_SZ 0x2400
#define IPA_DL_DESC_FIFO_SZ 0x8a0
#define IPA_DL_EMB_DATA_FIFO_SZ 0x1800
#define IPA_DL_EMB_DESC_FIFO_SZ 0x4e8

#define IPA_SMEM_UL_DATA_FIFO_OFST 0
#define IPA_SMEM_UL_DESC_FIFO_OFST 0xc00
#define IPA_SMEM_DL_DATA_FIFO_OFST 0x1130
#define IPA_SMEM_DL_DESC_FIFO_OFST 0x3530
#define IPA_SMEM_UL_EMB_DATA_FIFO_OFST 0x3dd0
#define IPA_SMEM_UL_EMB_DESC_FIFO_OFST 0x49d0

#define IPA_OCIMEM_DL_A2_DATA_FIFO_OFST 0
#define IPA_OCIMEM_DL_A2_DESC_FIFO_OFST (IPA_OCIMEM_DL_A2_DATA_FIFO_OFST + \
		IPA_DL_EMB_DATA_FIFO_SZ)
#define IPA_OCIMEM_DL_IPA_DATA_FIFO_OFST (IPA_OCIMEM_DL_A2_DESC_FIFO_OFST + \
		IPA_DL_EMB_DESC_FIFO_SZ)
#define IPA_OCIMEM_DL_IPA_DESC_FIFO_OFST (IPA_OCIMEM_DL_IPA_DATA_FIFO_OFST + \
		IPA_DL_EMB_DATA_FIFO_SZ)

enum ipa_pipe_type {
	IPA_DL_FROM_A2,
	IPA_DL_TO_IPA,
	IPA_UL_FROM_IPA,
	IPA_UL_TO_A2,
	IPA_PIPE_TYPE_MAX
};

struct ipa_bridge_pipe_context {
	struct sps_pipe *pipe;
	bool ipa_facing;
	bool valid;
};

struct ipa_bridge_context {
	struct ipa_bridge_pipe_context pipe[IPA_PIPE_TYPE_MAX];
	enum ipa_bridge_type type;
};

static struct ipa_bridge_context bridge[IPA_BRIDGE_TYPE_MAX];

static void ipa_get_dma_pipe_num(enum ipa_bridge_dir dir,
		enum ipa_bridge_type type, int *a2, int *ipa)
{
	if (type == IPA_BRIDGE_TYPE_TETHERED) {
		if (dir == IPA_BRIDGE_DIR_UL) {
			*a2 = DMA_A2_TETHERED_PIPE_UL;
			*ipa = DMA_IPA_TETHERED_PIPE_UL;
		} else {
			*a2 = DMA_A2_TETHERED_PIPE_DL;
			*ipa = DMA_IPA_TETHERED_PIPE_DL;
		}
	} else {
		if (dir == IPA_BRIDGE_DIR_UL) {
			*a2 = DMA_A2_EMBEDDED_PIPE_UL;
			*ipa = DMA_IPA_EMBEDDED_PIPE_UL;
		} else {
			*a2 = DMA_A2_EMBEDDED_PIPE_DL;
			*ipa = DMA_IPA_EMBEDDED_PIPE_DL;
		}
	}
}

static int ipa_get_desc_fifo_sz(enum ipa_bridge_dir dir,
		enum ipa_bridge_type type)
{
	int sz;

	if (type == IPA_BRIDGE_TYPE_TETHERED) {
		if (dir == IPA_BRIDGE_DIR_UL)
			sz = IPA_UL_DESC_FIFO_SZ;
		else
			sz = IPA_DL_DESC_FIFO_SZ;
	} else {
		if (dir == IPA_BRIDGE_DIR_UL)
			sz = IPA_UL_DESC_FIFO_SZ;
		else
			sz = IPA_DL_EMB_DESC_FIFO_SZ;
	}

	return sz;
}

static int ipa_get_data_fifo_sz(enum ipa_bridge_dir dir,
		enum ipa_bridge_type type)
{
	int sz;

	if (type == IPA_BRIDGE_TYPE_TETHERED) {
		if (dir == IPA_BRIDGE_DIR_UL)
			sz = IPA_UL_DATA_FIFO_SZ;
		else
			sz = IPA_DL_DATA_FIFO_SZ;
	} else {
		if (dir == IPA_BRIDGE_DIR_UL)
			sz = IPA_UL_DATA_FIFO_SZ;
		else
			sz = IPA_DL_EMB_DATA_FIFO_SZ;
	}

	return sz;
}

static int ipa_get_a2_pipe_num(enum ipa_bridge_dir dir,
		enum ipa_bridge_type type)
{
	int ep;

	if (type == IPA_BRIDGE_TYPE_TETHERED) {
		if (dir == IPA_BRIDGE_DIR_UL)
			ep = A2_TETHERED_PIPE_UL;
		else
			ep = A2_TETHERED_PIPE_DL;
	} else {
		if (dir == IPA_BRIDGE_DIR_UL)
			ep = A2_EMBEDDED_PIPE_UL;
		else
			ep = A2_EMBEDDED_PIPE_DL;
	}

	return ep;
}

int ipa_setup_ipa_dma_fifos(enum ipa_bridge_dir dir,
		enum ipa_bridge_type type,
		struct sps_mem_buffer *desc,
		struct sps_mem_buffer *data)
{
	int ret;

	ret = sps_setup_bam2bam_fifo(data,
			IPA_OCIMEM_DL_IPA_DATA_FIFO_OFST,
			ipa_get_data_fifo_sz(dir, type), 1);
	if (ret) {
		IPAERR("DAFIFO setup fail %d dir %d type %d\n",
				ret, dir, type);
		return ret;
	}

	ret = sps_setup_bam2bam_fifo(desc,
			IPA_OCIMEM_DL_IPA_DESC_FIFO_OFST,
			ipa_get_desc_fifo_sz(dir, type), 1);
	if (ret) {
		IPAERR("DEFIFO setup fail %d dir %d type %d\n",
				ret, dir, type);
		return ret;
	}

	IPADBG("dir=%d type=%d Dpa=%x Dsz=%u Dva=%p dpa=%x dsz=%u dva=%p\n",
			dir, type, data->phys_base, data->size, data->base,
			desc->phys_base, desc->size, desc->base);

	return 0;
}

int ipa_setup_a2_dma_fifos(enum ipa_bridge_dir dir,
		enum ipa_bridge_type type,
		struct sps_mem_buffer *desc,
		struct sps_mem_buffer *data)
{
	int ret;

	if (type == IPA_BRIDGE_TYPE_TETHERED) {
		if (dir == IPA_BRIDGE_DIR_UL) {
			desc->base = ipa_ctx->smem_pipe_mem +
				IPA_SMEM_UL_DESC_FIFO_OFST;
			desc->phys_base = smem_virt_to_phys(desc->base);
			desc->size = ipa_get_desc_fifo_sz(dir, type);
			data->base = ipa_ctx->smem_pipe_mem +
				IPA_SMEM_UL_DATA_FIFO_OFST;
			data->phys_base = smem_virt_to_phys(data->base);
			data->size = ipa_get_data_fifo_sz(dir, type);
		} else {
			desc->base = ipa_ctx->smem_pipe_mem +
				IPA_SMEM_DL_DESC_FIFO_OFST;
			desc->phys_base = smem_virt_to_phys(desc->base);
			desc->size = ipa_get_desc_fifo_sz(dir, type);
			data->base = ipa_ctx->smem_pipe_mem +
				IPA_SMEM_DL_DATA_FIFO_OFST;
			data->phys_base = smem_virt_to_phys(data->base);
			data->size = ipa_get_data_fifo_sz(dir, type);
		}
	} else {
		if (dir == IPA_BRIDGE_DIR_UL) {
			desc->base = ipa_ctx->smem_pipe_mem +
				IPA_SMEM_UL_EMB_DESC_FIFO_OFST;
			desc->phys_base = smem_virt_to_phys(desc->base);
			desc->size = ipa_get_desc_fifo_sz(dir, type);
			data->base = ipa_ctx->smem_pipe_mem +
				IPA_SMEM_UL_EMB_DATA_FIFO_OFST;
			data->phys_base = smem_virt_to_phys(data->base);
			data->size = ipa_get_data_fifo_sz(dir, type);
		} else {
			ret = sps_setup_bam2bam_fifo(data,
					IPA_OCIMEM_DL_A2_DATA_FIFO_OFST,
					ipa_get_data_fifo_sz(dir, type), 1);
			if (ret) {
				IPAERR("DAFIFO setup fail %d dir %d type %d\n",
						ret, dir, type);
				return ret;
			}

			ret = sps_setup_bam2bam_fifo(desc,
					IPA_OCIMEM_DL_A2_DESC_FIFO_OFST,
					ipa_get_desc_fifo_sz(dir, type), 1);
			if (ret) {
				IPAERR("DEFIFO setup fail %d dir %d type %d\n",
						ret, dir, type);
				return ret;
			}
		}
	}

	IPADBG("dir=%d type=%d Dpa=%x Dsz=%u Dva=%p dpa=%x dsz=%u dva=%p\n",
			dir, type, data->phys_base, data->size, data->base,
			desc->phys_base, desc->size, desc->base);

	return 0;
}

static int setup_dma_bam_bridge(enum ipa_bridge_dir dir,
			       enum ipa_bridge_type type,
			       struct ipa_sys_connect_params *props,
			       u32 *clnt_hdl)
{
	struct ipa_connect_params ipa_in_params;
	struct ipa_sps_params sps_out_params;
	int dma_a2_pipe;
	int dma_ipa_pipe;
	struct sps_pipe *pipe;
	struct sps_pipe *pipe_a2;
	struct sps_connect _connection;
	struct sps_connect *connection = &_connection;
	struct a2_mux_pipe_connection pipe_conn = {0};
	enum a2_mux_pipe_direction pipe_dir;
	u32 dma_hdl = sps_dma_get_bam_handle();
	u32 a2_hdl;
	u32 pa;
	int ret;

	memset(&ipa_in_params, 0, sizeof(ipa_in_params));
	memset(&sps_out_params, 0, sizeof(sps_out_params));

	pipe_dir = (dir == IPA_BRIDGE_DIR_UL) ? IPA_TO_A2 : A2_TO_IPA;

	ret = ipa_get_a2_mux_pipe_info(pipe_dir, &pipe_conn);
	if (ret) {
		IPAERR("ipa_get_a2_mux_pipe_info failed dir=%d type=%d\n",
				dir, type);
		goto fail_get_a2_prop;
	}

	pa = (dir == IPA_BRIDGE_DIR_UL) ? pipe_conn.dst_phy_addr :
					  pipe_conn.src_phy_addr;

	ret = sps_phy2h(pa, &a2_hdl);
	if (ret) {
		IPAERR("sps_phy2h failed (A2 BAM) %d dir=%d type=%d\n",
				ret, dir, type);
		goto fail_get_a2_prop;
	}

	ipa_get_dma_pipe_num(dir, type, &dma_a2_pipe, &dma_ipa_pipe);

	ipa_in_params.ipa_ep_cfg = props->ipa_ep_cfg;
	ipa_in_params.client = props->client;
	ipa_in_params.client_bam_hdl = dma_hdl;
	ipa_in_params.client_ep_idx = dma_ipa_pipe;
	ipa_in_params.priv = props->priv;
	ipa_in_params.notify = props->notify;
	ipa_in_params.desc_fifo_sz = ipa_get_desc_fifo_sz(dir, type);
	ipa_in_params.data_fifo_sz = ipa_get_data_fifo_sz(dir, type);

	if (type == IPA_BRIDGE_TYPE_EMBEDDED && dir == IPA_BRIDGE_DIR_DL) {
		if (ipa_setup_ipa_dma_fifos(dir, type, &ipa_in_params.desc,
					&ipa_in_params.data)) {
			IPAERR("fail to setup IPA-DMA FIFOs dir=%d type=%d\n",
					dir, type);
			goto fail_get_a2_prop;
		}
	}

	if (ipa_connect(&ipa_in_params, &sps_out_params, clnt_hdl)) {
		IPAERR("ipa connect failed dir=%d type=%d\n", dir, type);
		goto fail_get_a2_prop;
	}

	pipe = sps_alloc_endpoint();
	if (pipe == NULL) {
		IPAERR("sps_alloc_endpoint failed dir=%d type=%d\n", dir, type);
		ret = -ENOMEM;
		goto fail_sps_alloc;
	}

	memset(&_connection, 0, sizeof(_connection));
	ret = sps_get_config(pipe, connection);
	if (ret) {
		IPAERR("sps_get_config failed %d dir=%d type=%d\n", ret, dir,
				type);
		goto fail_sps_get_config;
	}

	if (dir == IPA_BRIDGE_DIR_DL) {
		connection->mode = SPS_MODE_SRC;
		connection->source = dma_hdl;
		connection->destination = sps_out_params.ipa_bam_hdl;
		connection->src_pipe_index = dma_ipa_pipe;
		connection->dest_pipe_index = sps_out_params.ipa_ep_idx;
	} else {
		connection->mode = SPS_MODE_DEST;
		connection->source = sps_out_params.ipa_bam_hdl;
		connection->destination = dma_hdl;
		connection->src_pipe_index = sps_out_params.ipa_ep_idx;
		connection->dest_pipe_index = dma_ipa_pipe;
	}

	connection->event_thresh = IPA_EVENT_THRESHOLD;
	connection->data = sps_out_params.data;
	connection->desc = sps_out_params.desc;
	connection->options = SPS_O_AUTO_ENABLE;

	ret = sps_connect(pipe, connection);
	if (ret) {
		IPAERR("sps_connect failed %d dir=%d type=%d\n", ret, dir,
				type);
		goto fail_sps_get_config;
	}

	if (dir == IPA_BRIDGE_DIR_DL) {
		bridge[type].pipe[IPA_DL_TO_IPA].pipe = pipe;
		bridge[type].pipe[IPA_DL_TO_IPA].ipa_facing = true;
		bridge[type].pipe[IPA_DL_TO_IPA].valid = true;
	} else {
		bridge[type].pipe[IPA_UL_FROM_IPA].pipe = pipe;
		bridge[type].pipe[IPA_UL_FROM_IPA].ipa_facing = true;
		bridge[type].pipe[IPA_UL_FROM_IPA].valid = true;
	}

	IPADBG("dir=%d type=%d (ipa) src(0x%x:%u)->dst(0x%x:%u)\n", dir, type,
			connection->source, connection->src_pipe_index,
			connection->destination, connection->dest_pipe_index);

	pipe_a2 = sps_alloc_endpoint();
	if (pipe_a2 == NULL) {
		IPAERR("sps_alloc_endpoint failed2 dir=%d type=%d\n", dir,
				type);
		ret = -ENOMEM;
		goto fail_sps_alloc_a2;
	}

	memset(&_connection, 0, sizeof(_connection));
	ret = sps_get_config(pipe_a2, connection);
	if (ret) {
		IPAERR("sps_get_config failed2 %d dir=%d type=%d\n", ret, dir,
				type);
		goto fail_sps_get_config_a2;
	}

	if (dir == IPA_BRIDGE_DIR_DL) {
		connection->mode = SPS_MODE_DEST;
		connection->source = a2_hdl;
		connection->destination = dma_hdl;
		connection->src_pipe_index = ipa_get_a2_pipe_num(dir, type);
		connection->dest_pipe_index = dma_a2_pipe;
	} else {
		connection->mode = SPS_MODE_SRC;
		connection->source = dma_hdl;
		connection->destination = a2_hdl;
		connection->src_pipe_index = dma_a2_pipe;
		connection->dest_pipe_index = ipa_get_a2_pipe_num(dir, type);
	}

	connection->event_thresh = IPA_EVENT_THRESHOLD;

	if (ipa_setup_a2_dma_fifos(dir, type, &connection->desc,
				&connection->data)) {
		IPAERR("fail to setup A2-DMA FIFOs dir=%d type=%d\n",
				dir, type);
		goto fail_sps_get_config_a2;
	}

	connection->options = SPS_O_AUTO_ENABLE;

	ret = sps_connect(pipe_a2, connection);
	if (ret) {
		IPAERR("sps_connect failed2 %d dir=%d type=%d\n", ret, dir,
				type);
		goto fail_sps_get_config_a2;
	}

	if (dir == IPA_BRIDGE_DIR_DL) {
		bridge[type].pipe[IPA_DL_FROM_A2].pipe = pipe_a2;
		bridge[type].pipe[IPA_DL_FROM_A2].valid = true;
	} else {
		bridge[type].pipe[IPA_UL_TO_A2].pipe = pipe_a2;
		bridge[type].pipe[IPA_UL_TO_A2].valid = true;
	}

	IPADBG("dir=%d type=%d (a2) src(0x%x:%u)->dst(0x%x:%u)\n", dir, type,
			connection->source, connection->src_pipe_index,
			connection->destination, connection->dest_pipe_index);

	return 0;

fail_sps_get_config_a2:
	sps_free_endpoint(pipe_a2);
fail_sps_alloc_a2:
	sps_disconnect(pipe);
fail_sps_get_config:
	sps_free_endpoint(pipe);
fail_sps_alloc:
	ipa_disconnect(*clnt_hdl);
fail_get_a2_prop:
	return ret;
}

/**
 * ipa_bridge_init()
 *
 * Return codes: 0: success, -ENOMEM: failure
 */
int ipa_bridge_init(void)
{
	int i;

	ipa_ctx->smem_pipe_mem = smem_alloc2(SMEM_BAM_PIPE_MEMORY,
			IPA_SMEM_PIPE_MEM_SZ);
	if (!ipa_ctx->smem_pipe_mem) {
		IPAERR("smem alloc failed\n");
		return -ENOMEM;
	}
	IPADBG("smem_pipe_mem = %p\n", ipa_ctx->smem_pipe_mem);

	for (i = 0; i < IPA_BRIDGE_TYPE_MAX; i++)
		bridge[i].type = i;

	return 0;
}

/**
 * ipa_bridge_setup() - setup SW bridge leg
 * @dir: downlink or uplink (from air interface perspective)
 * @type: tethered or embedded bridge
 * @props: bridge leg properties (EP config, callbacks, etc)
 * @clnt_hdl: [out] handle of IPA EP belonging to bridge leg
 *
 * NOTE: IT IS CALLER'S RESPONSIBILITY TO ENSURE BAMs ARE
 * OPERATIONAL AS LONG AS BRIDGE REMAINS UP
 *
 * Return codes:
 * 0: success
 * various negative error codes on errors
 */
int ipa_bridge_setup(enum ipa_bridge_dir dir, enum ipa_bridge_type type,
		     struct ipa_sys_connect_params *props, u32 *clnt_hdl)
{
	int ret;

	if (props == NULL || clnt_hdl == NULL ||
	    type >= IPA_BRIDGE_TYPE_MAX || dir >= IPA_BRIDGE_DIR_MAX ||
	    props->client >= IPA_CLIENT_MAX) {
		IPAERR("Bad param props=%p clnt_hdl=%p type=%d dir=%d\n",
		       props, clnt_hdl, type, dir);
		return -EINVAL;
	}

	ipa_inc_client_enable_clks();

	if (setup_dma_bam_bridge(dir, type, props, clnt_hdl)) {
		IPAERR("fail to setup SYS pipe to IPA dir=%d type=%d\n",
		       dir, type);
		ret = -EINVAL;
		goto bail_ipa;
	}

	return 0;

bail_ipa:
	ipa_dec_client_disable_clks();
	return ret;
}
EXPORT_SYMBOL(ipa_bridge_setup);

/**
 * ipa_bridge_teardown() - teardown SW bridge leg
 * @dir: downlink or uplink (from air interface perspective)
 * @type: tethered or embedded bridge
 * @clnt_hdl: handle of IPA EP
 *
 * Return codes:
 * 0: success
 * various negative error codes on errors
 */
int ipa_bridge_teardown(enum ipa_bridge_dir dir, enum ipa_bridge_type type,
			u32 clnt_hdl)
{
	struct ipa_bridge_pipe_context *sys;
	int lo;
	int hi;

	if (dir >= IPA_BRIDGE_DIR_MAX || type >= IPA_BRIDGE_TYPE_MAX ||
	    clnt_hdl >= IPA_NUM_PIPES || ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad param dir=%d type=%d\n", dir, type);
		return -EINVAL;
	}

	if (dir == IPA_BRIDGE_DIR_UL) {
		lo = IPA_UL_FROM_IPA;
		hi = IPA_UL_TO_A2;
	} else {
		lo = IPA_DL_FROM_A2;
		hi = IPA_DL_TO_IPA;
	}

	for (; lo <= hi; lo++) {
		sys = &bridge[type].pipe[lo];
		if (sys->valid) {
			if (sys->ipa_facing)
				ipa_disconnect(clnt_hdl);
			sps_disconnect(sys->pipe);
			sps_free_endpoint(sys->pipe);
			sys->valid = false;
		}
	}

	memset(&ipa_ctx->ep[clnt_hdl], 0, sizeof(struct ipa_ep_context));

	ipa_dec_client_disable_clks();

	return 0;
}
EXPORT_SYMBOL(ipa_bridge_teardown);
