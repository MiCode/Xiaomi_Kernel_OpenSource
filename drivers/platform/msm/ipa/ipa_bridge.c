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

enum ipa_bridge_id {
	IPA_DL_FROM_A2,
	IPA_DL_TO_IPA,
	IPA_UL_FROM_IPA,
	IPA_UL_TO_A2,
	IPA_BRIDGE_ID_MAX
};

static int polling_min_sleep[IPA_DIR_MAX] = { 950, 950 };
static int polling_max_sleep[IPA_DIR_MAX] = { 1050, 1050 };
static int polling_inactivity[IPA_DIR_MAX] = { 4, 4 };

struct ipa_pkt_info {
	void *buffer;
	dma_addr_t dma_address;
	uint32_t len;
	struct list_head list_node;
};

struct ipa_bridge_pipe_context {
	struct list_head head_desc_list;
	struct sps_pipe *pipe;
	struct sps_connect connection;
	struct sps_mem_buffer desc_mem_buf;
	struct sps_register_event register_event;
	spinlock_t spinlock;
	u32 len;
	u32 free_len;
	struct list_head free_desc_list;
};

static struct ipa_bridge_pipe_context bridge[IPA_BRIDGE_ID_MAX];

static struct workqueue_struct *ipa_ul_workqueue;
static struct workqueue_struct *ipa_dl_workqueue;
static void ipa_do_bridge_work(enum ipa_bridge_dir dir);

static u32 alloc_cnt[IPA_DIR_MAX];

static void ul_work_func(struct work_struct *work)
{
	ipa_do_bridge_work(IPA_UL);
}

static void dl_work_func(struct work_struct *work)
{
	ipa_do_bridge_work(IPA_DL);
}

static DECLARE_WORK(ul_work, ul_work_func);
static DECLARE_WORK(dl_work, dl_work_func);

static int ipa_switch_to_intr_mode(enum ipa_bridge_dir dir)
{
	int ret;
	struct ipa_bridge_pipe_context *sys = &bridge[2 * dir];

	ret = sps_get_config(sys->pipe, &sys->connection);
	if (ret) {
		IPAERR("sps_get_config() failed %d\n", ret);
		goto fail;
	}
	sys->register_event.options = SPS_O_EOT;
	ret = sps_register_event(sys->pipe, &sys->register_event);
	if (ret) {
		IPAERR("sps_register_event() failed %d\n", ret);
		goto fail;
	}
	sys->connection.options =
	   SPS_O_AUTO_ENABLE | SPS_O_ACK_TRANSFERS | SPS_O_EOT;
	ret = sps_set_config(sys->pipe, &sys->connection);
	if (ret) {
		IPAERR("sps_set_config() failed %d\n", ret);
		goto fail;
	}
	ret = 0;
fail:
	return ret;
}

static int ipa_switch_to_poll_mode(enum ipa_bridge_dir dir)
{
	int ret;
	struct ipa_bridge_pipe_context *sys = &bridge[2 * dir];

	ret = sps_get_config(sys->pipe, &sys->connection);
	if (ret) {
		IPAERR("sps_get_config() failed %d\n", ret);
		goto fail;
	}
	sys->connection.options =
	   SPS_O_AUTO_ENABLE | SPS_O_ACK_TRANSFERS | SPS_O_POLL;
	ret = sps_set_config(sys->pipe, &sys->connection);
	if (ret) {
		IPAERR("sps_set_config() failed %d\n", ret);
		goto fail;
	}
	ret = 0;
fail:
	return ret;
}

static int queue_rx_single(enum ipa_bridge_dir dir)
{
	struct ipa_bridge_pipe_context *sys_rx = &bridge[2 * dir];
	struct ipa_pkt_info *info;
	int ret;

	info = kmalloc(sizeof(struct ipa_pkt_info), GFP_KERNEL);
	if (!info) {
		IPAERR("unable to alloc rx_pkt_info\n");
		goto fail_pkt;
	}

	info->buffer = kmalloc(IPA_RX_SKB_SIZE, GFP_KERNEL | GFP_DMA);
	if (!info->buffer) {
		IPAERR("unable to alloc rx_pkt_buffer\n");
		goto fail_buffer;
	}

	info->dma_address = dma_map_single(NULL, info->buffer, IPA_RX_SKB_SIZE,
					   DMA_BIDIRECTIONAL);
	if (info->dma_address == 0 || info->dma_address == ~0) {
		IPAERR("dma_map_single failure %p for %p\n",
				(void *)info->dma_address, info->buffer);
		goto fail_dma;
	}

	info->len = ~0;

	list_add_tail(&info->list_node, &sys_rx->head_desc_list);
	ret = sps_transfer_one(sys_rx->pipe, info->dma_address,
			       IPA_RX_SKB_SIZE, info,
			       SPS_IOVEC_FLAG_INT | SPS_IOVEC_FLAG_EOT);
	if (ret) {
		list_del(&info->list_node);
		dma_unmap_single(NULL, info->dma_address, IPA_RX_SKB_SIZE,
				 DMA_BIDIRECTIONAL);
		IPAERR("sps_transfer_one failed %d\n", ret);
		goto fail_dma;
	}
	sys_rx->len++;
	return 0;

fail_dma:
	kfree(info->buffer);
fail_buffer:
	kfree(info);
fail_pkt:
	IPAERR("failed\n");
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
						  list_node);
			list_move_tail(&tx_pkt->list_node,
					&sys_tx->free_desc_list);
			sys_tx->len--;
			sys_tx->free_len++;
			tx_pkt->len = ~0;
			cnt++;
		}
	} while (all);

	return cnt;
}

static void ipa_do_bridge_work(enum ipa_bridge_dir dir)
{
	struct ipa_bridge_pipe_context *sys_rx = &bridge[2 * dir];
	struct ipa_bridge_pipe_context *sys_tx = &bridge[2 * dir + 1];
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
						  list_node);
			list_del(&rx_pkt->list_node);
			sys_rx->len--;
			rx_pkt->len = iov.size;

retry_alloc_tx:
			if (list_empty(&sys_tx->free_desc_list)) {
				tmp_pkt = kmalloc(sizeof(struct ipa_pkt_info),
						GFP_KERNEL);
				if (!tmp_pkt) {
					pr_debug_ratelimited("%s: unable to alloc tx_pkt_info\n",
					       __func__);
					usleep_range(polling_min_sleep[dir],
							polling_max_sleep[dir]);
					goto retry_alloc_tx;
				}

				tmp_pkt->buffer = kmalloc(IPA_RX_SKB_SIZE,
						GFP_KERNEL | GFP_DMA);
				if (!tmp_pkt->buffer) {
					pr_debug_ratelimited("%s: unable to alloc tx_pkt_buffer\n",
					       __func__);
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
					pr_debug_ratelimited("%s: dma_map_single failure %p for %p\n",
					       __func__,
					       (void *)tmp_pkt->dma_address,
					       tmp_pkt->buffer);
				}

				list_add_tail(&tmp_pkt->list_node,
						&sys_tx->free_desc_list);
				sys_tx->free_len++;
				alloc_cnt[dir]++;

				tmp_pkt->len = ~0;
			}

			tx_pkt = list_first_entry(&sys_tx->free_desc_list,
						  struct ipa_pkt_info,
						  list_node);
			list_del(&tx_pkt->list_node);
			sys_tx->free_len--;

retry_add_rx:
			list_add_tail(&tx_pkt->list_node,
					&sys_rx->head_desc_list);
			ret = sps_transfer_one(sys_rx->pipe,
					tx_pkt->dma_address,
					IPA_RX_SKB_SIZE,
					tx_pkt,
					SPS_IOVEC_FLAG_INT |
					SPS_IOVEC_FLAG_EOT);
			if (ret) {
				list_del(&tx_pkt->list_node);
				pr_debug_ratelimited("%s: sps_transfer_one failed %d\n",
						__func__, ret);
				usleep_range(polling_min_sleep[dir],
						polling_max_sleep[dir]);
				goto retry_add_rx;
			}
			sys_rx->len++;

retry_add_tx:
			list_add_tail(&rx_pkt->list_node,
					&sys_tx->head_desc_list);
			ret = sps_transfer_one(sys_tx->pipe,
					       rx_pkt->dma_address,
					       iov.size,
					       rx_pkt,
					       SPS_IOVEC_FLAG_INT |
					       SPS_IOVEC_FLAG_EOT);
			if (ret) {
				pr_debug_ratelimited("%s: fail to add to TX dir=%d\n",
						__func__, dir);
				list_del(&rx_pkt->list_node);
				ipa_reclaim_tx(sys_tx, true);
				usleep_range(polling_min_sleep[dir],
						polling_max_sleep[dir]);
				goto retry_add_tx;
			}
			sys_tx->len++;
		}

		if (inactive_cycles >= polling_inactivity[dir]) {
			ipa_switch_to_intr_mode(dir);
			break;
		}
	}
}

static void ipa_sps_irq_rx_notify(struct sps_event_notify *notify)
{
	switch (notify->event_id) {
	case SPS_EVENT_EOT:
		ipa_switch_to_poll_mode(IPA_UL);
		queue_work(ipa_ul_workqueue, &ul_work);
		break;
	default:
		IPAERR("recieved unexpected event id %d\n", notify->event_id);
	}
}

static int setup_bridge_to_ipa(enum ipa_bridge_dir dir)
{
	struct ipa_bridge_pipe_context *sys;
	struct ipa_ep_cfg_mode mode;
	dma_addr_t dma_addr;
	int ipa_ep_idx;
	int ret;
	int i;

	if (dir == IPA_DL) {
		ipa_ep_idx = ipa_get_ep_mapping(ipa_ctx->mode,
				IPA_CLIENT_A2_TETHERED_PROD);
		if (ipa_ep_idx == -1) {
			IPAERR("Invalid client.\n");
			ret = -EINVAL;
			goto tx_alloc_endpoint_failed;
		}

		sys = &bridge[IPA_DL_TO_IPA];
		sys->pipe = sps_alloc_endpoint();
		if (sys->pipe == NULL) {
			IPAERR("tx alloc endpoint failed\n");
			ret = -ENOMEM;
			goto tx_alloc_endpoint_failed;
		}
		ret = sps_get_config(sys->pipe, &sys->connection);
		if (ret) {
			IPAERR("tx get config failed %d\n", ret);
			goto tx_get_config_failed;
		}

		sys->connection.source = SPS_DEV_HANDLE_MEM;
		sys->connection.src_pipe_index = ipa_ctx->a5_pipe_index++;
		sys->connection.destination = ipa_ctx->bam_handle;
		sys->connection.dest_pipe_index = ipa_ep_idx;
		sys->connection.mode = SPS_MODE_DEST;
		sys->connection.options =
		   SPS_O_AUTO_ENABLE | SPS_O_ACK_TRANSFERS | SPS_O_POLL;
		sys->desc_mem_buf.size = IPA_SYS_DESC_FIFO_SZ; /* 2k */
		sys->desc_mem_buf.base = dma_alloc_coherent(NULL,
				sys->desc_mem_buf.size,
				&dma_addr,
				0);
		if (sys->desc_mem_buf.base == NULL) {
			IPAERR("tx memory alloc failed\n");
			ret = -ENOMEM;
			goto tx_get_config_failed;
		}
		sys->desc_mem_buf.phys_base = dma_addr;
		memset(sys->desc_mem_buf.base, 0x0, sys->desc_mem_buf.size);
		sys->connection.desc = sys->desc_mem_buf;
		sys->connection.event_thresh = IPA_EVENT_THRESHOLD;

		ret = sps_connect(sys->pipe, &sys->connection);
		if (ret < 0) {
			IPAERR("tx connect error %d\n", ret);
			goto tx_connect_failed;
		}

		INIT_LIST_HEAD(&sys->head_desc_list);
		INIT_LIST_HEAD(&sys->free_desc_list);
		spin_lock_init(&sys->spinlock);

		ipa_ctx->ep[ipa_ep_idx].valid = 1;

		mode.mode = IPA_DMA;
		mode.dst = IPA_CLIENT_USB_CONS;
		ret = ipa_cfg_ep_mode(ipa_ep_idx, &mode);
		if (ret < 0) {
			IPAERR("DMA mode set error %d\n", ret);
			goto tx_mode_set_failed;
		}

		return 0;

tx_mode_set_failed:
		sps_disconnect(sys->pipe);
tx_connect_failed:
		dma_free_coherent(NULL, sys->desc_mem_buf.size,
				sys->desc_mem_buf.base,
				sys->desc_mem_buf.phys_base);
tx_get_config_failed:
		sps_free_endpoint(sys->pipe);
tx_alloc_endpoint_failed:
		return ret;
	} else {

		ipa_ep_idx = ipa_get_ep_mapping(ipa_ctx->mode,
				IPA_CLIENT_A2_TETHERED_CONS);
		if (ipa_ep_idx == -1) {
			IPAERR("Invalid client.\n");
			ret = -EINVAL;
			goto rx_alloc_endpoint_failed;
		}

		sys = &bridge[IPA_UL_FROM_IPA];
		sys->pipe = sps_alloc_endpoint();
		if (sys->pipe == NULL) {
			IPAERR("rx alloc endpoint failed\n");
			ret = -ENOMEM;
			goto rx_alloc_endpoint_failed;
		}
		ret = sps_get_config(sys->pipe, &sys->connection);
		if (ret) {
			IPAERR("rx get config failed %d\n", ret);
			goto rx_get_config_failed;
		}

		sys->connection.source = ipa_ctx->bam_handle;
		sys->connection.src_pipe_index = 7;
		sys->connection.destination = SPS_DEV_HANDLE_MEM;
		sys->connection.dest_pipe_index = ipa_ctx->a5_pipe_index++;
		sys->connection.mode = SPS_MODE_SRC;
		sys->connection.options = SPS_O_AUTO_ENABLE | SPS_O_EOT |
		      SPS_O_ACK_TRANSFERS;
		sys->desc_mem_buf.size = IPA_SYS_DESC_FIFO_SZ; /* 2k */
		sys->desc_mem_buf.base = dma_alloc_coherent(NULL,
				sys->desc_mem_buf.size,
				&dma_addr,
				0);
		if (sys->desc_mem_buf.base == NULL) {
			IPAERR("rx memory alloc failed\n");
			ret = -ENOMEM;
			goto rx_get_config_failed;
		}
		sys->desc_mem_buf.phys_base = dma_addr;
		memset(sys->desc_mem_buf.base, 0x0, sys->desc_mem_buf.size);
		sys->connection.desc = sys->desc_mem_buf;
		sys->connection.event_thresh = IPA_EVENT_THRESHOLD;

		ret = sps_connect(sys->pipe, &sys->connection);
		if (ret < 0) {
			IPAERR("rx connect error %d\n", ret);
			goto rx_connect_failed;
		}

		sys->register_event.options = SPS_O_EOT;
		sys->register_event.mode = SPS_TRIGGER_CALLBACK;
		sys->register_event.xfer_done = NULL;
		sys->register_event.callback = ipa_sps_irq_rx_notify;
		sys->register_event.user = NULL;
		ret = sps_register_event(sys->pipe, &sys->register_event);
		if (ret < 0) {
			IPAERR("tx register event error %d\n", ret);
			goto rx_event_reg_failed;
		}

		INIT_LIST_HEAD(&sys->head_desc_list);
		INIT_LIST_HEAD(&sys->free_desc_list);
		spin_lock_init(&sys->spinlock);

		for (i = 0; i < IPA_RX_POOL_CEIL; i++) {
			ret = queue_rx_single(dir);
			if (ret < 0)
				IPAERR("queue fail %d %d\n", dir, i);
		}

		return 0;

rx_event_reg_failed:
		sps_disconnect(sys->pipe);
rx_connect_failed:
		dma_free_coherent(NULL,
				sys->desc_mem_buf.size,
				sys->desc_mem_buf.base,
				sys->desc_mem_buf.phys_base);
rx_get_config_failed:
		sps_free_endpoint(sys->pipe);
rx_alloc_endpoint_failed:
		return ret;
	}
}

static void bam_mux_rx_notify(struct sps_event_notify *notify)
{
	switch (notify->event_id) {
	case SPS_EVENT_EOT:
		ipa_switch_to_poll_mode(IPA_DL);
		queue_work(ipa_dl_workqueue, &dl_work);
		break;
	default:
		IPAERR("recieved unexpected event id %d\n", notify->event_id);
	}
}

static int setup_bridge_to_a2(enum ipa_bridge_dir dir)
{
	struct ipa_bridge_pipe_context *sys;
	struct a2_mux_pipe_connection pipe_conn = { 0, };
	dma_addr_t dma_addr;
	u32 a2_handle;
	int ret;
	int i;

	if (dir == IPA_UL) {
		ret = ipa_get_a2_mux_pipe_info(IPA_TO_A2, &pipe_conn);
		if (ret) {
			IPAERR("ipa_get_a2_mux_pipe_info failed IPA_TO_A2\n");
			goto tx_alloc_endpoint_failed;
		}

		ret = sps_phy2h(pipe_conn.dst_phy_addr, &a2_handle);
		if (ret) {
			IPAERR("sps_phy2h failed (A2 BAM) %d\n", ret);
			goto tx_alloc_endpoint_failed;
		}

		sys = &bridge[IPA_UL_TO_A2];
		sys->pipe = sps_alloc_endpoint();
		if (sys->pipe == NULL) {
			IPAERR("tx alloc endpoint failed\n");
			ret = -ENOMEM;
			goto tx_alloc_endpoint_failed;
		}
		ret = sps_get_config(sys->pipe, &sys->connection);
		if (ret) {
			IPAERR("tx get config failed %d\n", ret);
			goto tx_get_config_failed;
		}

		sys->connection.source = SPS_DEV_HANDLE_MEM;
		sys->connection.src_pipe_index = ipa_ctx->a5_pipe_index++;
		sys->connection.destination = a2_handle;
		sys->connection.dest_pipe_index = pipe_conn.dst_pipe_index;
		sys->connection.mode = SPS_MODE_DEST;
		sys->connection.options =
		   SPS_O_AUTO_ENABLE | SPS_O_ACK_TRANSFERS | SPS_O_POLL;
		sys->desc_mem_buf.size = IPA_SYS_DESC_FIFO_SZ; /* 2k */
		sys->desc_mem_buf.base = dma_alloc_coherent(NULL,
				sys->desc_mem_buf.size,
				&dma_addr,
				0);
		if (sys->desc_mem_buf.base == NULL) {
			IPAERR("tx memory alloc failed\n");
			ret = -ENOMEM;
			goto tx_get_config_failed;
		}
		sys->desc_mem_buf.phys_base = dma_addr;
		memset(sys->desc_mem_buf.base, 0x0, sys->desc_mem_buf.size);
		sys->connection.desc = sys->desc_mem_buf;
		sys->connection.event_thresh = IPA_EVENT_THRESHOLD;

		ret = sps_connect(sys->pipe, &sys->connection);
		if (ret < 0) {
			IPAERR("tx connect error %d\n", ret);
			goto tx_connect_failed;
		}

		INIT_LIST_HEAD(&sys->head_desc_list);
		INIT_LIST_HEAD(&sys->free_desc_list);
		spin_lock_init(&sys->spinlock);

		return 0;

tx_connect_failed:
		dma_free_coherent(NULL,
				sys->desc_mem_buf.size,
				sys->desc_mem_buf.base,
				sys->desc_mem_buf.phys_base);
tx_get_config_failed:
		sps_free_endpoint(sys->pipe);
tx_alloc_endpoint_failed:
		return ret;
	} else { /* dir == IPA_UL */

		ret = ipa_get_a2_mux_pipe_info(A2_TO_IPA, &pipe_conn);
		if (ret) {
			IPAERR("ipa_get_a2_mux_pipe_info failed A2_TO_IPA\n");
			goto rx_alloc_endpoint_failed;
		}

		ret = sps_phy2h(pipe_conn.src_phy_addr, &a2_handle);
		if (ret) {
			IPAERR("sps_phy2h failed (A2 BAM) %d\n", ret);
			goto rx_alloc_endpoint_failed;
		}

		sys = &bridge[IPA_DL_FROM_A2];
		sys->pipe = sps_alloc_endpoint();
		if (sys->pipe == NULL) {
			IPAERR("rx alloc endpoint failed\n");
			ret = -ENOMEM;
			goto rx_alloc_endpoint_failed;
		}
		ret = sps_get_config(sys->pipe, &sys->connection);
		if (ret) {
			IPAERR("rx get config failed %d\n", ret);
			goto rx_get_config_failed;
		}

		sys->connection.source = a2_handle;
		sys->connection.src_pipe_index = pipe_conn.src_pipe_index;
		sys->connection.destination = SPS_DEV_HANDLE_MEM;
		sys->connection.dest_pipe_index = ipa_ctx->a5_pipe_index++;
		sys->connection.mode = SPS_MODE_SRC;
		sys->connection.options = SPS_O_AUTO_ENABLE | SPS_O_EOT |
		      SPS_O_ACK_TRANSFERS;
		sys->desc_mem_buf.size = IPA_SYS_DESC_FIFO_SZ; /* 2k */
		sys->desc_mem_buf.base = dma_alloc_coherent(NULL,
				sys->desc_mem_buf.size,
				&dma_addr,
				0);
		if (sys->desc_mem_buf.base == NULL) {
			IPAERR("rx memory alloc failed\n");
			ret = -ENOMEM;
			goto rx_get_config_failed;
		}
		sys->desc_mem_buf.phys_base = dma_addr;
		memset(sys->desc_mem_buf.base, 0x0, sys->desc_mem_buf.size);
		sys->connection.desc = sys->desc_mem_buf;
		sys->connection.event_thresh = IPA_EVENT_THRESHOLD;

		ret = sps_connect(sys->pipe, &sys->connection);
		if (ret < 0) {
			IPAERR("rx connect error %d\n", ret);
			goto rx_connect_failed;
		}

		sys->register_event.options = SPS_O_EOT;
		sys->register_event.mode = SPS_TRIGGER_CALLBACK;
		sys->register_event.xfer_done = NULL;
		sys->register_event.callback = bam_mux_rx_notify;
		sys->register_event.user = NULL;
		ret = sps_register_event(sys->pipe, &sys->register_event);
		if (ret < 0) {
			IPAERR("tx register event error %d\n", ret);
			goto rx_event_reg_failed;
		}

		INIT_LIST_HEAD(&sys->head_desc_list);
		INIT_LIST_HEAD(&sys->free_desc_list);
		spin_lock_init(&sys->spinlock);


		for (i = 0; i < IPA_RX_POOL_CEIL; i++) {
			ret = queue_rx_single(dir);
			if (ret < 0)
				IPAERR("queue fail %d %d\n", dir, i);
		}

		return 0;

rx_event_reg_failed:
		sps_disconnect(sys->pipe);
rx_connect_failed:
		dma_free_coherent(NULL,
				sys->desc_mem_buf.size,
				sys->desc_mem_buf.base,
				sys->desc_mem_buf.phys_base);
rx_get_config_failed:
		sps_free_endpoint(sys->pipe);
rx_alloc_endpoint_failed:
		return ret;
	}
}

/**
 * ipa_bridge_init() - initialize the tethered bridge, allocate UL and DL
 * workqueues
 *
 * Return codes: 0: success, -ENOMEM: failure
 */
int ipa_bridge_init(void)
{
	int ret;

	ipa_ul_workqueue = alloc_workqueue("ipa_ul",
			WQ_MEM_RECLAIM | WQ_CPU_INTENSIVE, 1);
	if (!ipa_ul_workqueue) {
		IPAERR("ipa ul wq alloc failed\n");
		ret = -ENOMEM;
		goto fail_ul;
	}

	ipa_dl_workqueue = alloc_workqueue("ipa_dl",
			WQ_MEM_RECLAIM | WQ_CPU_INTENSIVE, 1);
	if (!ipa_dl_workqueue) {
		IPAERR("ipa dl wq alloc failed\n");
		ret = -ENOMEM;
		goto fail_dl;
	}

	return 0;
fail_dl:
	destroy_workqueue(ipa_ul_workqueue);
fail_ul:
	return ret;
}

/**
 * ipa_bridge_setup() - setup tethered SW bridge in specified direction
 * @dir: downlink or uplink (from air interface perspective)
 *
 * Return codes:
 * 0: success
 * various negative error codes on errors
 */
int ipa_bridge_setup(enum ipa_bridge_dir dir)
{
	int ret;

	if (atomic_inc_return(&ipa_ctx->ipa_active_clients) == 1)
		ipa_enable_clks();

	if (setup_bridge_to_a2(dir)) {
		IPAERR("fail to setup SYS pipe to A2 %d\n", dir);
		ret = -EINVAL;
		goto bail_a2;
	}

	if (setup_bridge_to_ipa(dir)) {
		IPAERR("fail to setup SYS pipe to IPA %d\n", dir);
		ret = -EINVAL;
		goto bail_ipa;
	}

	return 0;

bail_ipa:
	if (dir == IPA_UL)
		sps_disconnect(bridge[IPA_UL_TO_A2].pipe);
	else
		sps_disconnect(bridge[IPA_DL_FROM_A2].pipe);
bail_a2:
	if (atomic_dec_return(&ipa_ctx->ipa_active_clients) == 0)
		ipa_disable_clks();

	return ret;
}

/**
 * ipa_bridge_teardown() - teardown the tethered bridge in the specified dir
 * @dir: downlink or uplink (from air interface perspective)
 *
 * Return codes:
 * 0: always
 */
int ipa_bridge_teardown(enum ipa_bridge_dir dir)
{
	struct ipa_bridge_pipe_context *sys;

	if (dir == IPA_UL) {
		sys = &bridge[IPA_UL_TO_A2];
		sps_disconnect(sys->pipe);
		sys = &bridge[IPA_UL_FROM_IPA];
		sps_disconnect(sys->pipe);
	} else {
		sys = &bridge[IPA_DL_FROM_A2];
		sps_disconnect(sys->pipe);
		sys = &bridge[IPA_DL_TO_IPA];
		sps_disconnect(sys->pipe);
	}

	if (atomic_dec_return(&ipa_ctx->ipa_active_clients) == 0)
		ipa_disable_clks();

	return 0;
}

/**
 * ipa_bridge_cleanup() - de-initialize the tethered bridge
 *
 * Return codes:
 * None
 */
void ipa_bridge_cleanup(void)
{
	destroy_workqueue(ipa_dl_workqueue);
	destroy_workqueue(ipa_ul_workqueue);
}
