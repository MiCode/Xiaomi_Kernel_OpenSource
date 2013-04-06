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
#include "ipa_i.h"

#define A2_EMBEDDED_PIPE_TX 4
#define A2_EMBEDDED_PIPE_RX 5

enum ipa_pipe_type {
	IPA_DL_FROM_A2,
	IPA_DL_TO_IPA,
	IPA_UL_FROM_IPA,
	IPA_UL_TO_A2,
	IPA_PIPE_TYPE_MAX
};

static int polling_min_sleep[IPA_BRIDGE_DIR_MAX] = { 950, 950 };
static int polling_max_sleep[IPA_BRIDGE_DIR_MAX] = { 1050, 1050 };
static int polling_inactivity[IPA_BRIDGE_DIR_MAX] = { 4, 4 };

struct ipa_pkt_info {
	void *buffer;
	dma_addr_t dma_address;
	uint32_t len;
	struct list_head link;
};

struct ipa_bridge_pipe_context {
	struct list_head head_desc_list;
	struct sps_pipe *pipe;
	struct sps_connect connection;
	struct sps_mem_buffer desc_mem_buf;
	struct sps_register_event register_event;
	struct list_head free_desc_list;
	bool valid;
};

struct ipa_bridge_context {
	struct ipa_bridge_pipe_context pipe[IPA_PIPE_TYPE_MAX];
	struct workqueue_struct *ul_wq;
	struct workqueue_struct *dl_wq;
	struct work_struct ul_work;
	struct work_struct dl_work;
	enum ipa_bridge_type type;
};

static struct ipa_bridge_context bridge[IPA_BRIDGE_TYPE_MAX];

static void ipa_do_bridge_work(enum ipa_bridge_dir dir,
		struct ipa_bridge_context *ctx);

static void ul_work_func(struct work_struct *work)
{
	struct ipa_bridge_context *ctx = container_of(work,
			struct ipa_bridge_context, ul_work);
	ipa_do_bridge_work(IPA_BRIDGE_DIR_UL, ctx);
}

static void dl_work_func(struct work_struct *work)
{
	struct ipa_bridge_context *ctx = container_of(work,
			struct ipa_bridge_context, dl_work);
	ipa_do_bridge_work(IPA_BRIDGE_DIR_DL, ctx);
}

static int ipa_switch_to_intr_mode(enum ipa_bridge_dir dir,
				    struct ipa_bridge_context *ctx)
{
	int ret;
	struct ipa_bridge_pipe_context *sys = &ctx->pipe[2 * dir];

	ret = sps_get_config(sys->pipe, &sys->connection);
	if (ret) {
		IPAERR("sps_get_config() failed %d type=%d dir=%d\n",
				ret, ctx->type, dir);
		goto fail;
	}
	sys->register_event.options = SPS_O_EOT;
	ret = sps_register_event(sys->pipe, &sys->register_event);
	if (ret) {
		IPAERR("sps_register_event() failed %d type=%d dir=%d\n",
				ret, ctx->type, dir);
		goto fail;
	}
	sys->connection.options =
	   SPS_O_AUTO_ENABLE | SPS_O_ACK_TRANSFERS | SPS_O_EOT;
	ret = sps_set_config(sys->pipe, &sys->connection);
	if (ret) {
		IPAERR("sps_set_config() failed %d type=%d dir=%d\n",
				ret, ctx->type, dir);
		goto fail;
	}
	ret = 0;
fail:
	return ret;
}

static int ipa_switch_to_poll_mode(enum ipa_bridge_dir dir,
				    enum ipa_bridge_type type)
{
	int ret;
	struct ipa_bridge_pipe_context *sys = &bridge[type].pipe[2 * dir];

	ret = sps_get_config(sys->pipe, &sys->connection);
	if (ret) {
		IPAERR("sps_get_config() failed %d type=%d dir=%d\n",
				ret, type, dir);
		goto fail;
	}
	sys->connection.options =
	   SPS_O_AUTO_ENABLE | SPS_O_ACK_TRANSFERS | SPS_O_POLL;
	ret = sps_set_config(sys->pipe, &sys->connection);
	if (ret) {
		IPAERR("sps_set_config() failed %d type=%d dir=%d\n",
				ret, type, dir);
		goto fail;
	}
	ret = 0;
fail:
	return ret;
}

static int queue_rx_single(enum ipa_bridge_dir dir, enum ipa_bridge_type type)
{
	struct ipa_bridge_pipe_context *sys_rx = &bridge[type].pipe[2 * dir];
	struct ipa_pkt_info *info;
	int ret;

	info = kmalloc(sizeof(struct ipa_pkt_info), GFP_KERNEL);
	if (!info) {
		IPAERR("unable to alloc rx_pkt_info type=%d dir=%d\n",
				type, dir);
		goto fail_pkt;
	}

	info->buffer = kmalloc(IPA_RX_SKB_SIZE, GFP_KERNEL | GFP_DMA);
	if (!info->buffer) {
		IPAERR("unable to alloc rx_pkt_buffer type=%d dir=%d\n",
				type, dir);
		goto fail_buffer;
	}

	info->dma_address = dma_map_single(NULL, info->buffer, IPA_RX_SKB_SIZE,
					   DMA_BIDIRECTIONAL);
	if (info->dma_address == 0 || info->dma_address == ~0) {
		IPAERR("dma_map_single failure %p for %p type=%d dir=%d\n",
				(void *)info->dma_address, info->buffer,
				type, dir);
		goto fail_dma;
	}

	list_add_tail(&info->link, &sys_rx->head_desc_list);
	ret = sps_transfer_one(sys_rx->pipe, info->dma_address,
			       IPA_RX_SKB_SIZE, info,
			       SPS_IOVEC_FLAG_INT);
	if (ret) {
		list_del(&info->link);
		dma_unmap_single(NULL, info->dma_address, IPA_RX_SKB_SIZE,
				 DMA_BIDIRECTIONAL);
		IPAERR("sps_transfer_one failed %d type=%d dir=%d\n", ret,
				type, dir);
		goto fail_dma;
	}
	return 0;

fail_dma:
	kfree(info->buffer);
fail_buffer:
	kfree(info);
fail_pkt:
	IPAERR("failed type=%d dir=%d\n", type, dir);
	return -ENOMEM;
}

static int ipa_reclaim_tx(struct ipa_bridge_pipe_context *sys_tx, bool all)
{
	struct sps_iovec iov;
	struct ipa_pkt_info *tx_pkt;
	int cnt = 0;
	int ret;

	do {
		iov.addr = 0;
		ret = sps_get_iovec(sys_tx->pipe, &iov);
		if (ret || iov.addr == 0) {
			break;
		} else {
			tx_pkt = list_first_entry(&sys_tx->head_desc_list,
						  struct ipa_pkt_info,
						  link);
			list_move_tail(&tx_pkt->link,
					&sys_tx->free_desc_list);
			cnt++;
		}
	} while (all);

	return cnt;
}

static void ipa_do_bridge_work(enum ipa_bridge_dir dir,
			       struct ipa_bridge_context *ctx)
{
	struct ipa_bridge_pipe_context *sys_rx = &ctx->pipe[2 * dir];
	struct ipa_bridge_pipe_context *sys_tx = &ctx->pipe[2 * dir + 1];
	struct ipa_pkt_info *tx_pkt;
	struct ipa_pkt_info *rx_pkt;
	struct ipa_pkt_info *tmp_pkt;
	struct sps_iovec iov;
	int ret;
	int inactive_cycles = 0;

	while (1) {
		++inactive_cycles;

		if (ipa_reclaim_tx(sys_tx, false))
			inactive_cycles = 0;

		iov.addr = 0;
		ret = sps_get_iovec(sys_rx->pipe, &iov);
		if (ret || iov.addr == 0) {
			/* no-op */
		} else {
			inactive_cycles = 0;

			rx_pkt = list_first_entry(&sys_rx->head_desc_list,
						  struct ipa_pkt_info,
						  link);
			list_del(&rx_pkt->link);
			rx_pkt->len = iov.size;

retry_alloc_tx:
			if (list_empty(&sys_tx->free_desc_list)) {
				tmp_pkt = kmalloc(sizeof(struct ipa_pkt_info),
						GFP_KERNEL);
				if (!tmp_pkt) {
					pr_debug_ratelimited("%s: unable to alloc tx_pkt_info type=%d dir=%d\n",
					       __func__, ctx->type, dir);
					usleep_range(polling_min_sleep[dir],
							polling_max_sleep[dir]);
					goto retry_alloc_tx;
				}

				tmp_pkt->buffer = kmalloc(IPA_RX_SKB_SIZE,
						GFP_KERNEL | GFP_DMA);
				if (!tmp_pkt->buffer) {
					pr_debug_ratelimited("%s: unable to alloc tx_pkt_buffer type=%d dir=%d\n",
					       __func__, ctx->type, dir);
					kfree(tmp_pkt);
					usleep_range(polling_min_sleep[dir],
							polling_max_sleep[dir]);
					goto retry_alloc_tx;
				}

				tmp_pkt->dma_address = dma_map_single(NULL,
						tmp_pkt->buffer,
						IPA_RX_SKB_SIZE,
						DMA_BIDIRECTIONAL);
				if (tmp_pkt->dma_address == 0 ||
						tmp_pkt->dma_address == ~0) {
					pr_debug_ratelimited("%s: dma_map_single failure %p for %p type=%d dir=%d\n",
					       __func__,
					       (void *)tmp_pkt->dma_address,
					       tmp_pkt->buffer, ctx->type, dir);
				}

				list_add_tail(&tmp_pkt->link,
						&sys_tx->free_desc_list);
			}

			tx_pkt = list_first_entry(&sys_tx->free_desc_list,
						  struct ipa_pkt_info,
						  link);
			list_del(&tx_pkt->link);

retry_add_rx:
			list_add_tail(&tx_pkt->link,
					&sys_rx->head_desc_list);
			ret = sps_transfer_one(sys_rx->pipe,
					tx_pkt->dma_address,
					IPA_RX_SKB_SIZE,
					tx_pkt,
					SPS_IOVEC_FLAG_INT);
			if (ret) {
				list_del(&tx_pkt->link);
				pr_debug_ratelimited("%s: sps_transfer_one failed %d type=%d dir=%d\n",
						__func__, ret, ctx->type, dir);
				usleep_range(polling_min_sleep[dir],
						polling_max_sleep[dir]);
				goto retry_add_rx;
			}

retry_add_tx:
			list_add_tail(&rx_pkt->link,
					&sys_tx->head_desc_list);
			ret = sps_transfer_one(sys_tx->pipe,
					       rx_pkt->dma_address,
					       iov.size,
					       rx_pkt,
					       SPS_IOVEC_FLAG_INT |
					       SPS_IOVEC_FLAG_EOT);
			if (ret) {
				pr_debug_ratelimited("%s: fail to add to TX type=%d dir=%d\n",
						__func__, ctx->type, dir);
				list_del(&rx_pkt->link);
				ipa_reclaim_tx(sys_tx, true);
				usleep_range(polling_min_sleep[dir],
						polling_max_sleep[dir]);
				goto retry_add_tx;
			}
			IPA_STATS_INC_BRIDGE_CNT(ctx->type, dir,
					ipa_ctx->stats.bridged_pkts);
		}

		if (inactive_cycles >= polling_inactivity[dir]) {
			ipa_switch_to_intr_mode(dir, ctx);
			break;
		}
	}
}

static void ipa_sps_irq_rx_notify(struct sps_event_notify *notify)
{
	enum ipa_bridge_type type = (enum ipa_bridge_type) notify->user;

	switch (notify->event_id) {
	case SPS_EVENT_EOT:
		ipa_switch_to_poll_mode(IPA_BRIDGE_DIR_UL, type);
		queue_work(bridge[type].ul_wq, &bridge[type].ul_work);
		break;
	default:
		IPAERR("recieved unexpected event id %d type %d\n",
				notify->event_id, type);
	}
}

static int setup_bridge_to_ipa(enum ipa_bridge_dir dir,
			       enum ipa_bridge_type type,
			       struct ipa_sys_connect_params *props,
			       u32 *clnt_hdl)
{
	struct ipa_bridge_pipe_context *sys;
	dma_addr_t dma_addr;
	enum ipa_pipe_type pipe_type;
	int ipa_ep_idx;
	int ret;
	int i;

	ipa_ep_idx = ipa_get_ep_mapping(ipa_ctx->mode, props->client);
	if (ipa_ep_idx == -1) {
		IPAERR("Invalid client=%d mode=%d type=%d dir=%d\n",
				props->client, ipa_ctx->mode, type, dir);
		ret = -EINVAL;
		goto alloc_endpoint_failed;
	}

	if (ipa_ctx->ep[ipa_ep_idx].valid) {
		IPAERR("EP %d already allocated type=%d dir=%d\n", ipa_ep_idx,
				type, dir);
		ret = -EINVAL;
		goto alloc_endpoint_failed;
	}

	pipe_type = (dir == IPA_BRIDGE_DIR_DL) ? IPA_DL_TO_IPA :
						 IPA_UL_FROM_IPA;

	sys = &bridge[type].pipe[pipe_type];
	sys->pipe = sps_alloc_endpoint();
	if (sys->pipe == NULL) {
		IPAERR("alloc endpoint failed type=%d dir=%d\n", type, dir);
		ret = -ENOMEM;
		goto alloc_endpoint_failed;
	}
	ret = sps_get_config(sys->pipe, &sys->connection);
	if (ret) {
		IPAERR("get config failed %d type=%d dir=%d\n", ret, type, dir);
		ret = -EINVAL;
		goto get_config_failed;
	}

	if (dir == IPA_BRIDGE_DIR_DL) {
		sys->connection.source = SPS_DEV_HANDLE_MEM;
		sys->connection.src_pipe_index = ipa_ctx->a5_pipe_index++;
		sys->connection.destination = ipa_ctx->bam_handle;
		sys->connection.dest_pipe_index = ipa_ep_idx;
		sys->connection.mode = SPS_MODE_DEST;
		sys->connection.options =
		   SPS_O_AUTO_ENABLE | SPS_O_ACK_TRANSFERS | SPS_O_POLL;
	} else {
		sys->connection.source = ipa_ctx->bam_handle;
		sys->connection.src_pipe_index = ipa_ep_idx;
		sys->connection.destination = SPS_DEV_HANDLE_MEM;
		sys->connection.dest_pipe_index = ipa_ctx->a5_pipe_index++;
		sys->connection.mode = SPS_MODE_SRC;
		sys->connection.options = SPS_O_AUTO_ENABLE | SPS_O_EOT |
		      SPS_O_ACK_TRANSFERS | SPS_O_NO_DISABLE;
	}

	sys->desc_mem_buf.size = props->desc_fifo_sz;
	sys->desc_mem_buf.base = dma_alloc_coherent(NULL,
						    sys->desc_mem_buf.size,
						    &dma_addr,
						    0);
	if (sys->desc_mem_buf.base == NULL) {
		IPAERR("memory alloc failed type=%d dir=%d\n", type, dir);
		ret = -ENOMEM;
		goto get_config_failed;
	}
	sys->desc_mem_buf.phys_base = dma_addr;
	memset(sys->desc_mem_buf.base, 0x0, sys->desc_mem_buf.size);
	sys->connection.desc = sys->desc_mem_buf;
	sys->connection.event_thresh = IPA_EVENT_THRESHOLD;

	ret = sps_connect(sys->pipe, &sys->connection);
	if (ret < 0) {
		IPAERR("connect error %d type=%d dir=%d\n", ret, type, dir);
		goto connect_failed;
	}

	INIT_LIST_HEAD(&sys->head_desc_list);
	INIT_LIST_HEAD(&sys->free_desc_list);

	memset(&ipa_ctx->ep[ipa_ep_idx], 0,
	       sizeof(struct ipa_ep_context));

	ipa_ctx->ep[ipa_ep_idx].valid = 1;
	ipa_ctx->ep[ipa_ep_idx].client_notify = props->notify;
	ipa_ctx->ep[ipa_ep_idx].priv = props->priv;

	ret = ipa_cfg_ep(ipa_ep_idx, &props->ipa_ep_cfg);
	if (ret < 0) {
		IPAERR("ep cfg set error %d type=%d dir=%d\n", ret, type, dir);
		ipa_ctx->ep[ipa_ep_idx].valid = 0;
		goto event_reg_failed;
	}

	if (dir == IPA_BRIDGE_DIR_UL) {
		sys->register_event.options = SPS_O_EOT;
		sys->register_event.mode = SPS_TRIGGER_CALLBACK;
		sys->register_event.xfer_done = NULL;
		sys->register_event.callback = ipa_sps_irq_rx_notify;
		sys->register_event.user = (void *)type;
		ret = sps_register_event(sys->pipe, &sys->register_event);
		if (ret < 0) {
			IPAERR("register event error %d type=%d dir=%d\n", ret,
					type, dir);
			goto event_reg_failed;
		}

		for (i = 0; i < IPA_RX_POOL_CEIL; i++) {
			ret = queue_rx_single(dir, type);
			if (ret < 0)
				IPAERR("queue fail dir=%d type=%d iter=%d\n",
				       dir, type, i);
		}
	}

	*clnt_hdl = ipa_ep_idx;
	sys->valid = true;

	return 0;

event_reg_failed:
	sps_disconnect(sys->pipe);
connect_failed:
	dma_free_coherent(NULL,
			  sys->desc_mem_buf.size,
			  sys->desc_mem_buf.base,
			  sys->desc_mem_buf.phys_base);
get_config_failed:
	sps_free_endpoint(sys->pipe);
alloc_endpoint_failed:
	return ret;
}

static void bam_mux_rx_notify(struct sps_event_notify *notify)
{
	enum ipa_bridge_type type = (enum ipa_bridge_type) notify->user;

	switch (notify->event_id) {
	case SPS_EVENT_EOT:
		ipa_switch_to_poll_mode(IPA_BRIDGE_DIR_DL, type);
		queue_work(bridge[type].dl_wq, &bridge[type].dl_work);
		break;
	default:
		IPAERR("recieved unexpected event id %d type %d\n",
				notify->event_id, type);
	}
}

static int setup_bridge_to_a2(enum ipa_bridge_dir dir,
			      enum ipa_bridge_type type,
			      u32 desc_fifo_sz)
{
	struct ipa_bridge_pipe_context *sys;
	struct a2_mux_pipe_connection pipe_conn = { 0 };
	dma_addr_t dma_addr;
	u32 a2_handle;
	enum a2_mux_pipe_direction pipe_dir;
	enum ipa_pipe_type pipe_type;
	u32 pa;
	int ret;
	int i;

	pipe_dir = (dir == IPA_BRIDGE_DIR_UL) ? IPA_TO_A2 : A2_TO_IPA;

	ret = ipa_get_a2_mux_pipe_info(pipe_dir, &pipe_conn);
	if (ret) {
		IPAERR("ipa_get_a2_mux_pipe_info failed type=%d dir=%d\n",
				type, dir);
		ret = -EINVAL;
		goto alloc_endpoint_failed;
	}

	pa = (dir == IPA_BRIDGE_DIR_UL) ? pipe_conn.dst_phy_addr :
					  pipe_conn.src_phy_addr;

	ret = sps_phy2h(pa, &a2_handle);
	if (ret) {
		IPAERR("sps_phy2h failed (A2 BAM) %d type=%d dir=%d\n",
				ret, type, dir);
		ret = -EINVAL;
		goto alloc_endpoint_failed;
	}

	pipe_type = (dir == IPA_BRIDGE_DIR_UL) ? IPA_UL_TO_A2 : IPA_DL_FROM_A2;

	sys = &bridge[type].pipe[pipe_type];
	sys->pipe = sps_alloc_endpoint();
	if (sys->pipe == NULL) {
		IPAERR("alloc endpoint failed type=%d dir=%d\n", type, dir);
		ret = -ENOMEM;
		goto alloc_endpoint_failed;
	}
	ret = sps_get_config(sys->pipe, &sys->connection);
	if (ret) {
		IPAERR("get config failed %d type=%d dir=%d\n", ret, type, dir);
		ret = -EINVAL;
		goto get_config_failed;
	}

	if (dir == IPA_BRIDGE_DIR_UL) {
		sys->connection.source = SPS_DEV_HANDLE_MEM;
		sys->connection.src_pipe_index = ipa_ctx->a5_pipe_index++;
		sys->connection.destination = a2_handle;
		if (type == IPA_BRIDGE_TYPE_TETHERED)
			sys->connection.dest_pipe_index =
			   pipe_conn.dst_pipe_index;
		else
			sys->connection.dest_pipe_index = A2_EMBEDDED_PIPE_TX;
		sys->connection.mode = SPS_MODE_DEST;
		sys->connection.options =
		   SPS_O_AUTO_ENABLE | SPS_O_ACK_TRANSFERS | SPS_O_POLL;
	} else {
		sys->connection.source = a2_handle;
		if (type == IPA_BRIDGE_TYPE_TETHERED)
			sys->connection.src_pipe_index =
			   pipe_conn.src_pipe_index;
		else
			sys->connection.src_pipe_index = A2_EMBEDDED_PIPE_RX;
		sys->connection.destination = SPS_DEV_HANDLE_MEM;
		sys->connection.dest_pipe_index = ipa_ctx->a5_pipe_index++;
		sys->connection.mode = SPS_MODE_SRC;
		sys->connection.options = SPS_O_AUTO_ENABLE | SPS_O_EOT |
		      SPS_O_ACK_TRANSFERS;
	}

	sys->desc_mem_buf.size = desc_fifo_sz;
	sys->desc_mem_buf.base = dma_alloc_coherent(NULL,
						    sys->desc_mem_buf.size,
						    &dma_addr,
						    0);
	if (sys->desc_mem_buf.base == NULL) {
		IPAERR("memory alloc failed type=%d dir=%d\n", type, dir);
		ret = -ENOMEM;
		goto get_config_failed;
	}
	sys->desc_mem_buf.phys_base = dma_addr;
	memset(sys->desc_mem_buf.base, 0x0, sys->desc_mem_buf.size);
	sys->connection.desc = sys->desc_mem_buf;
	sys->connection.event_thresh = IPA_EVENT_THRESHOLD;

	ret = sps_connect(sys->pipe, &sys->connection);
	if (ret < 0) {
		IPAERR("connect error %d type=%d dir=%d\n", ret, type, dir);
		ret = -EINVAL;
		goto connect_failed;
	}

	INIT_LIST_HEAD(&sys->head_desc_list);
	INIT_LIST_HEAD(&sys->free_desc_list);

	if (dir == IPA_BRIDGE_DIR_DL) {
		sys->register_event.options = SPS_O_EOT;
		sys->register_event.mode = SPS_TRIGGER_CALLBACK;
		sys->register_event.xfer_done = NULL;
		sys->register_event.callback = bam_mux_rx_notify;
		sys->register_event.user = (void *)type;
		ret = sps_register_event(sys->pipe, &sys->register_event);
		if (ret < 0) {
			IPAERR("register event error %d type=%d dir=%d\n",
					ret, type, dir);
			ret = -EINVAL;
			goto event_reg_failed;
		}

		for (i = 0; i < IPA_RX_POOL_CEIL; i++) {
			ret = queue_rx_single(dir, type);
			if (ret < 0)
				IPAERR("queue fail dir=%d type=%d iter=%d\n",
				       dir, type, i);
		}
	}

	sys->valid = true;

	return 0;

event_reg_failed:
	sps_disconnect(sys->pipe);
connect_failed:
	dma_free_coherent(NULL,
			  sys->desc_mem_buf.size,
			  sys->desc_mem_buf.base,
			  sys->desc_mem_buf.phys_base);
get_config_failed:
	sps_free_endpoint(sys->pipe);
alloc_endpoint_failed:
	return ret;
}

/**
 * ipa_bridge_init() - create workqueues and work items serving SW bridges
 *
 * Return codes: 0: success, -ENOMEM: failure
 */
int ipa_bridge_init(void)
{
	int ret;
	int i;

	bridge[IPA_BRIDGE_TYPE_TETHERED].ul_wq =
		create_singlethread_workqueue("ipa_ul_teth");
	if (!bridge[IPA_BRIDGE_TYPE_TETHERED].ul_wq) {
		IPAERR("ipa ul teth wq alloc failed\n");
		ret = -ENOMEM;
		goto fail_ul_teth;
	}

	bridge[IPA_BRIDGE_TYPE_TETHERED].dl_wq =
		create_singlethread_workqueue("ipa_dl_teth");
	if (!bridge[IPA_BRIDGE_TYPE_TETHERED].dl_wq) {
		IPAERR("ipa dl teth wq alloc failed\n");
		ret = -ENOMEM;
		goto fail_dl_teth;
	}

	bridge[IPA_BRIDGE_TYPE_EMBEDDED].ul_wq =
		create_singlethread_workqueue("ipa_ul_emb");
	if (!bridge[IPA_BRIDGE_TYPE_EMBEDDED].ul_wq) {
		IPAERR("ipa ul emb wq alloc failed\n");
		ret = -ENOMEM;
		goto fail_ul_emb;
	}

	bridge[IPA_BRIDGE_TYPE_EMBEDDED].dl_wq =
		create_singlethread_workqueue("ipa_dl_emb");
	if (!bridge[IPA_BRIDGE_TYPE_EMBEDDED].dl_wq) {
		IPAERR("ipa dl emb wq alloc failed\n");
		ret = -ENOMEM;
		goto fail_dl_emb;
	}

	for (i = 0; i < IPA_BRIDGE_TYPE_MAX; i++) {
		INIT_WORK(&bridge[i].ul_work, ul_work_func);
		INIT_WORK(&bridge[i].dl_work, dl_work_func);
		bridge[i].type = i;
	}

	return 0;

fail_dl_emb:
	destroy_workqueue(bridge[IPA_BRIDGE_TYPE_EMBEDDED].ul_wq);
fail_ul_emb:
	destroy_workqueue(bridge[IPA_BRIDGE_TYPE_TETHERED].dl_wq);
fail_dl_teth:
	destroy_workqueue(bridge[IPA_BRIDGE_TYPE_TETHERED].ul_wq);
fail_ul_teth:
	return ret;
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
	    props->client >= IPA_CLIENT_MAX || props->desc_fifo_sz == 0) {
		IPAERR("Bad param props=%p clnt_hdl=%p type=%d dir=%d\n",
		       props, clnt_hdl, type, dir);
		return -EINVAL;
	}

	if (atomic_inc_return(&ipa_ctx->ipa_active_clients) == 1) {
		if (ipa_ctx->ipa_hw_mode == IPA_HW_MODE_NORMAL)
			ipa_enable_clks();
	}

	if (setup_bridge_to_ipa(dir, type, props, clnt_hdl)) {
		IPAERR("fail to setup SYS pipe to IPA dir=%d type=%d\n",
		       dir, type);
		ret = -EINVAL;
		goto bail_ipa;
	}

	if (setup_bridge_to_a2(dir, type, props->desc_fifo_sz)) {
		IPAERR("fail to setup SYS pipe to A2 dir=%d type=%d\n",
		       dir, type);
		ret = -EINVAL;
		goto bail_a2;
	}


	return 0;

bail_a2:
	ipa_bridge_teardown(dir, type, *clnt_hdl);
bail_ipa:
	if (atomic_dec_return(&ipa_ctx->ipa_active_clients) == 0) {
		if (ipa_ctx->ipa_hw_mode == IPA_HW_MODE_NORMAL)
			ipa_disable_clks();
	}
	return ret;
}
EXPORT_SYMBOL(ipa_bridge_setup);

static void ipa_bridge_free_pkt(struct ipa_pkt_info *pkt)
{
	list_del(&pkt->link);
	dma_unmap_single(NULL, pkt->dma_address, IPA_RX_SKB_SIZE,
			 DMA_BIDIRECTIONAL);
	kfree(pkt->buffer);
	kfree(pkt);
}

static void ipa_bridge_free_resources(struct ipa_bridge_pipe_context *pipe)
{
	struct ipa_pkt_info *pkt;
	struct ipa_pkt_info *n;

	list_for_each_entry_safe(pkt, n, &pipe->head_desc_list, link)
		ipa_bridge_free_pkt(pkt);

	list_for_each_entry_safe(pkt, n, &pipe->free_desc_list, link)
		ipa_bridge_free_pkt(pkt);
}

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
			sps_disconnect(sys->pipe);
			dma_free_coherent(NULL, sys->desc_mem_buf.size,
					  sys->desc_mem_buf.base,
					  sys->desc_mem_buf.phys_base);
			sps_free_endpoint(sys->pipe);
			ipa_bridge_free_resources(sys);
			sys->valid = false;
		}
	}

	memset(&ipa_ctx->ep[clnt_hdl], 0, sizeof(struct ipa_ep_context));

	if (atomic_dec_return(&ipa_ctx->ipa_active_clients) == 0) {
		if (ipa_ctx->ipa_hw_mode == IPA_HW_MODE_NORMAL)
			ipa_disable_clks();
	}

	return 0;
}
EXPORT_SYMBOL(ipa_bridge_teardown);

/**
 * ipa_bridge_cleanup() - destroy workqueues serving the SW bridges
 *
 * Return codes:
 * None
 */
void ipa_bridge_cleanup(void)
{
	int i;

	for (i = 0; i < IPA_BRIDGE_TYPE_MAX; i++) {
		destroy_workqueue(bridge[i].dl_wq);
		destroy_workqueue(bridge[i].ul_wq);
	}
}
