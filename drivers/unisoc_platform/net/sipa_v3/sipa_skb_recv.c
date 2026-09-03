// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/percpu-defs.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <linux/sipa.h>
#include <linux/netdevice.h>
#include <uapi/linux/sched/types.h>

#include "sipa_dummy.h"
#include "sipa_hal.h"
#include "sipa_priv.h"

#define SIPA_MAX_ORDER		4
#define SIPA_PER_PAGE_CAPACITY	32
#define SIPA_MAX_PROBE_DEPTH	128
#define SIPA_RECV_BUF_LEN	2048
#define SIPA_RECV_RSVD_LEN	NET_SKB_PAD

static int sipa_init_recv_array(struct sipa_skb_receiver *receiver, u32 depth)
{
	int i;
	struct sipa_skb_array *skb_array;
	size_t size = sizeof(struct sipa_skb_dma_addr_pair);
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		skb_array = devm_kzalloc(ipa->dev,
					 sizeof(struct sipa_skb_array),
					 GFP_KERNEL);
		if (!skb_array)
			return -ENOMEM;

		skb_array->array = devm_kcalloc(ipa->dev, depth,
						size, GFP_KERNEL);
		if (!skb_array->array)
			return -ENOMEM;

		skb_array->rp = 0;
		skb_array->wp = 0;
		skb_array->depth = depth;

		receiver->fill_array[i] = skb_array;
		INIT_LIST_HEAD(&skb_array->mem_list);
	}

	return 0;
}

void sipa_reinit_recv_array(struct device *dev)
{
	int i;
	struct sipa_skb_array *skb_array;
	struct sipa_plat_drv_cfg *ipa = dev_get_drvdata(dev);

	if (!ipa->receiver) {
		dev_err(dev, "sipa receiver is null\n");
		return;
	}

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		skb_array = ipa->receiver->fill_array[i];
		if (!skb_array->array) {
			dev_err(dev, "sipa p->array is null\n");
			return;
		}

		skb_array->rp = 0;
		skb_array->wp = skb_array->depth;
	}
}
EXPORT_SYMBOL(sipa_reinit_recv_array);

static int sipa_put_recv_array_node(struct sipa_skb_array *p,
				    struct sk_buff *skb,
				    dma_addr_t dma_addr, bool need_unmap)
{
	u32 pos;

	if ((p->wp - p->rp) >= p->depth)
		return -1;

	pos = p->wp & (p->depth - 1);
	p->array[pos].skb = skb;
	p->array[pos].dma_addr = dma_addr;
	p->array[pos].need_unmap = need_unmap;
	/* Ensure that we put the item to the fifo before
	 * we update the fifo wp.
	 */
	smp_wmb();
	p->wp++;
	return 0;
}

static int sipa_get_recv_array_node(struct sipa_skb_array *p,
				    struct sk_buff **skb, dma_addr_t *dma_addr,
				    bool *need_unmap)
{
	u32 pos;

	if (p->rp == p->wp)
		return -1;

	pos = p->rp & (p->depth - 1);
	*skb = p->array[pos].skb;
	*dma_addr = p->array[pos].dma_addr;
	*need_unmap = p->array[pos].need_unmap;
	/* Ensure that we remove the item from the fifo before
	 * we update the fifo rp.
	 */
	smp_wmb();
	p->rp++;
	return 0;
}

static void sipa_alloc_mem(struct sipa_skb_receiver *receiver, u32 chan,
			   u32 order, bool head)
{
	size_t size;
	struct page *page;
	struct sipa_recv_mem_list *skb_mem;
	struct sipa_skb_array *fill_arrays = receiver->fill_array[chan];

	if (!fill_arrays) {
		dev_err(receiver->dev, "sipa alloc mem fill_arrays is null\n");
		return;
	}

	page = dev_alloc_pages(order);
	if (!page) {
		dev_err(receiver->dev, "sipa alloc mem alloc page err\n");
		return;
	}

	skb_mem = devm_kzalloc(receiver->dev, sizeof(*skb_mem), GFP_KERNEL);
	if (!skb_mem) {
		__free_pages(page, order);
		return;
	}

	size = PAGE_SIZE << order;
	skb_mem->virt = page_to_virt(page);
	skb_mem->page = page;
	skb_mem->dma_addr = dma_map_single(receiver->dev,
					   skb_mem->virt, size,
					   DMA_FROM_DEVICE);
	if (dma_mapping_error(receiver->dev, skb_mem->dma_addr)) {
		dev_err(receiver->dev, "sipa alloc mem dma map err\n");
		kfree(skb_mem);
		__free_pages(page, order);
		return;
	}

	skb_mem->size = size;

	if (head)
		list_add(&skb_mem->list, &fill_arrays->mem_list);
	else
		list_add_tail(&skb_mem->list, &fill_arrays->mem_list);
}

static void sipa_prepare_free_node(struct sipa_skb_receiver *receiver, u32 chan)
{
	int i = 0;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	void *virt, *virt_tail;
	struct sipa_node_desc_tag *item;
	struct sipa_recv_mem_list *skb_mem;
	struct sipa_skb_array *fill_arrays = receiver->fill_array[chan];
	enum sipa_cmn_fifo_index fifo_id = receiver->ep->recv_fifo.idx + chan;

	list_for_each_entry(skb_mem, &fill_arrays->mem_list, list) {
		dma_addr = skb_mem->dma_addr;
		virt_tail = skb_mem->virt + skb_mem->size;
		for (virt = skb_mem->virt; virt < virt_tail;
		     virt += SIPA_RECV_BUF_LEN) {
			item = sipa_hal_get_rx_node_wptr(receiver->dev,
							 fifo_id, i++);
			if (!item) {
				dev_err(receiver->dev, "sipa get item err\n");
				i--;
				goto out;
			}

			skb = build_skb(virt, SIPA_RECV_BUF_LEN);
			if (!skb) {
				dev_err(receiver->dev, "%s build skb fail!\n", __func__);
				i--;
				continue;
			}

			page_ref_inc(skb_mem->page);
			sipa_put_recv_array_node(fill_arrays, skb,
						 dma_addr, false);

			item->address = dma_addr;
			item->offset = NET_SKB_PAD;
			item->length = SIPA_RECV_BUF_LEN - NET_SKB_PAD;

			dma_addr += SIPA_RECV_BUF_LEN;
		}

		if (i == fill_arrays->depth)
			break;
	}

out:
	sipa_hal_sync_node_to_rx_fifo(receiver->dev, fifo_id, i);
}

static void sipa_prepare_free_node_init(struct sipa_skb_receiver *receiver,
					u32 cnt, int cpu_num)
{
	u32 i, order;
	size_t total_size, left_size;

	for (i = 0; i < cpu_num; i++) {
		total_size = (size_t)cnt * SIPA_RECV_BUF_LEN;
		order = get_order(total_size);

		for (; order > SIPA_MAX_ORDER;) {
			sipa_alloc_mem(receiver, i, SIPA_MAX_ORDER, false);
			total_size -= (PAGE_SIZE << SIPA_MAX_ORDER);
			left_size = total_size;
			order = get_order(left_size);
		}

		if (order > 0)
			sipa_alloc_mem(receiver, i, order, false);

		sipa_prepare_free_node(receiver, i);
	}
}

static void sipa_fill_free_fifo_node(struct sipa_skb_receiver *receiver,
				     u32 chan, u32 cnt)
{
	bool ready = false;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	struct sipa_recv_mem_list *skb_mem, *temp;
	struct sipa_node_desc_tag *item = NULL;
	int i, j, success_cnt = 0, probe_depth = 0;
	struct sipa_skb_array *fill_array = receiver->fill_array[chan];
	enum sipa_cmn_fifo_index id = receiver->ep->recv_fifo.idx + chan;

	for (i = 0; i < cnt; i++) {
		list_for_each_entry_safe(skb_mem, temp,
					 &fill_array->mem_list, list) {
			if (page_ref_count(skb_mem->page) == 1) {
				ready = true;
				break;
			}

			if ((++probe_depth) > SIPA_MAX_PROBE_DEPTH &&
			    atomic_read(&fill_array->need_fill_cnt) <
			    SIPA_PER_PAGE_CAPACITY)
				break;
		}

		if (!ready || !skb_mem || !skb_mem->virt)
			break;

		ready = false;
		for (j = 0; j < SIPA_PER_PAGE_CAPACITY; j++) {
			skb = build_skb((void *)(skb_mem->virt +
						 j * SIPA_RECV_BUF_LEN),
					SIPA_RECV_BUF_LEN);
			if (!skb) {
				dev_err(receiver->dev, "%s build skb fail\n", __func__);
				break;
			}

			page_ref_inc(skb_mem->page);
			dma_addr = skb_mem->dma_addr + j * SIPA_RECV_BUF_LEN;
			if (!dma_addr) {
				dev_err(receiver->dev, "dma_addr is null\n");
				dev_kfree_skb_any(skb);
				break;
			}

			dma_sync_single_range_for_device(receiver->dev,
							 dma_addr,
							 0,
							 SIPA_RECV_BUF_LEN,
							 DMA_TO_DEVICE);

			item = sipa_hal_get_rx_node_wptr(receiver->dev, id,
							 success_cnt);
			if (!item) {
				dev_err(receiver->dev, "item is null\n");
				dev_kfree_skb_any(skb);
				break;
			}

			sipa_put_recv_array_node(fill_array, skb,
						 dma_addr, false);
			item->address = dma_addr;
			item->offset = NET_SKB_PAD;
			item->length = SIPA_RECV_BUF_LEN - NET_SKB_PAD;
			success_cnt++;
		}

		list_del(&skb_mem->list);
		list_add_tail(&skb_mem->list, &fill_array->mem_list);

		sipa_hal_sync_node_to_rx_fifo(receiver->dev, id, success_cnt);
		sipa_hal_add_rx_fifo_wptr(receiver->dev, id, success_cnt);
		atomic_sub(success_cnt, &fill_array->need_fill_cnt);
		success_cnt = 0;

		if (j != SIPA_PER_PAGE_CAPACITY)
			break;
	}
}

static int sipa_fill_free_node_by_alloc_skb(struct sipa_skb_receiver *receiver,
					    u32 chan)
{
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	u32 j, success_cnt = 0;
	struct sipa_node_desc_tag *item = NULL;
	struct sipa_skb_array *fill_array = receiver->fill_array[chan];
	enum sipa_cmn_fifo_index id = receiver->ep->recv_fifo.idx + chan;
	u32 need_fill_cnt = atomic_read(&fill_array->need_fill_cnt);

	receiver->alloc_skb_cnt++;

	for (j = 0; j < need_fill_cnt; j++) {
		skb = __dev_alloc_skb(SIPA_RECV_BUF_LEN - NET_SKB_PAD,
				      GFP_KERNEL | GFP_NOWAIT);
		if (skb) {
			dma_addr = dma_map_single(receiver->dev,
						  skb->head,
						  SIPA_RECV_BUF_LEN,
						  DMA_FROM_DEVICE);
			if (dma_mapping_error(receiver->dev, dma_addr)) {
				dev_err(receiver->dev,
					"prepare free node dma map err\n");
				break;
			}
		} else {
			dev_err(receiver->dev,
				"fill node by alloc skb fail\n");
			break;
		}

		item = sipa_hal_get_rx_node_wptr(receiver->dev,
						 id, success_cnt);
		if (!item) {
			dev_err(receiver->dev, "item is null\n");
			dev_kfree_skb_any(skb);
			break;
		}

		sipa_put_recv_array_node(fill_array, skb,
					 dma_addr, true);

		memset(item, 0, sizeof(struct sipa_node_desc_tag));
		item->address = dma_addr;
		item->offset = NET_SKB_PAD;
		item->length = SIPA_RECV_BUF_LEN - NET_SKB_PAD;
		success_cnt++;

		if (success_cnt % SIPA_PER_PAGE_CAPACITY == 0) {
			sipa_hal_sync_node_to_rx_fifo(receiver->dev,
						      id, success_cnt);
			sipa_hal_add_rx_fifo_wptr(receiver->dev,
						  id, success_cnt);
			atomic_sub(success_cnt,
				   &fill_array->need_fill_cnt);

			success_cnt = 0;
		}
	}

	if (success_cnt != 0) {
		sipa_hal_sync_node_to_rx_fifo(receiver->dev,
					      id, success_cnt);
		sipa_hal_add_rx_fifo_wptr(receiver->dev,
					  id, success_cnt);
		atomic_sub(success_cnt,
			   &fill_array->need_fill_cnt);
	}

	if (atomic_read(&fill_array->need_fill_cnt))
		return -ENOMEM;
	else
		return 0;
}

static int sipa_fill_free_fifo(struct sipa_skb_receiver *receiver,
			       u32 chan, u32 cnt)
{
	struct sipa_skb_array *fill_array = receiver->fill_array[chan];

	atomic_inc(&receiver->check_flag);

	if (atomic_read(&receiver->check_suspend)) {
		dev_warn(receiver->dev,
			 "encountered ipa suspend while fill free fifo cpu = %d\n",
			 smp_processor_id());
		atomic_dec(&receiver->check_flag);
		return -EBUSY;
	}

	sipa_fill_free_fifo_node(receiver, chan, cnt);
	if (atomic_read(&fill_array->need_fill_cnt) > 1024)
		dev_err(receiver->dev,
			"sipa memory reclamation failed need_fill_cnt = %d\n",
			atomic_read(&fill_array->need_fill_cnt));

	if (atomic_read(&fill_array->need_fill_cnt) / SIPA_PER_PAGE_CAPACITY) {
		if (unlikely(sipa_fill_free_node_by_alloc_skb(receiver,
							      chan))) {
			atomic_dec(&receiver->check_flag);
			return -ENOMEM;
		}
	}

	atomic_dec(&receiver->check_flag);

	return 0;
}

static int sipa_fill_remaining_node(struct sipa_skb_receiver *receiver,
				    u32 cnt, u32 chan)
{
	bool ready = false;
	int i, success_cnt = 0;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	struct sipa_recv_mem_list *skb_mem, *temp;
	struct sipa_node_desc_tag *item = NULL;
	struct sipa_skb_array *fill_array = receiver->fill_array[chan];
	enum sipa_cmn_fifo_index id = receiver->ep->recv_fifo.idx + chan;

	list_for_each_entry_safe(skb_mem, temp, &fill_array->mem_list, list) {
		if (page_ref_count(skb_mem->page) == 1) {
			ready = true;
			break;
		}
	}

	if (!ready || !skb_mem || !skb_mem->virt)
		return -ENOMEM;

	if (cnt > SIPA_PER_PAGE_CAPACITY)
		cnt = SIPA_PER_PAGE_CAPACITY;

	for (i = 0; i < cnt; i++) {
		skb = build_skb((void *)(skb_mem->virt +
					 i * SIPA_RECV_BUF_LEN),
				SIPA_RECV_BUF_LEN);
		if (!skb) {
			dev_err(receiver->dev, "%s build skb fail\n", __func__);
			break;
		}

		page_ref_inc(skb_mem->page);
		dma_addr = skb_mem->dma_addr + i * SIPA_RECV_BUF_LEN;
		if (!dma_addr) {
			dev_err(receiver->dev, "dma_addr is null\n");
			break;
		}

		dma_sync_single_range_for_device(receiver->dev,
						 dma_addr,
						 0,
						 SIPA_RECV_BUF_LEN,
						 DMA_TO_DEVICE);

		sipa_put_recv_array_node(fill_array, skb,
					 dma_addr, false);

		item = sipa_hal_get_rx_node_wptr(receiver->dev, id, i);
		if (!item) {
			dev_err(receiver->dev, "item is null\n");
			break;
		}

		item->address = dma_addr;
		item->offset = NET_SKB_PAD;
		item->length = SIPA_RECV_BUF_LEN - NET_SKB_PAD;
		success_cnt++;
	}

	list_del(&skb_mem->list);
	list_add_tail(&skb_mem->list, &fill_array->mem_list);

	if (success_cnt != cnt)
		dev_info(receiver->dev,
			 "fill remaining node err success_cnt = %d cnt = %d\n",
			 success_cnt, cnt);

	atomic_sub(success_cnt, &fill_array->need_fill_cnt);
	sipa_hal_sync_node_to_rx_fifo(receiver->dev, id, success_cnt);
	sipa_hal_add_rx_fifo_wptr(receiver->dev, id, success_cnt);

	return atomic_read(&fill_array->need_fill_cnt);
}

static int sipa_fill_all_free_fifo(void)
{
	int i, cnt, ret = 0;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_skb_receiver *receiver = ipa->receiver;

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		cnt = atomic_read(&receiver->fill_array[i]->need_fill_cnt) /
				  SIPA_PER_PAGE_CAPACITY;
		if (cnt)
			if (sipa_fill_free_fifo(receiver, i, cnt))
				ret = -EAGAIN;
	}

	return ret;
}

static int sipa_fill_all_remaining_node(void)
{
	u32 cnt;
	int i;
	struct sipa_skb_array *fill_array;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_skb_receiver *receiver = ipa->receiver;

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		fill_array = receiver->fill_array[i];
		cnt = atomic_read(&fill_array->need_fill_cnt);
		if (atomic_read(&fill_array->need_fill_cnt)) {
			if (sipa_fill_remaining_node(receiver, cnt, i)) {
				if (sipa_fill_free_node_by_alloc_skb(receiver,
								     i))
					return -ENOMEM;
			}
		}
	}

	return 0;
}

static int sipa_check_need_fill_cnt(void)
{
	int i;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_skb_receiver *receiver = ipa->receiver;

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		if (atomic_read(&receiver->fill_array[i]->need_fill_cnt) >= 32)
			return true;
	}

	return 0;
}

static int sipa_check_need_remaining_cnt(void)
{
	int i;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_skb_receiver *receiver = ipa->receiver;

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		if (atomic_read(&receiver->fill_array[i]->need_fill_cnt))
			return true;
	}

	return 0;
}

void sipa_recv_wake_up(void)
{
	int i;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_skb_receiver *receiver = ipa->receiver;

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		if (atomic_read(&receiver->fill_array[i]->need_fill_cnt) > 64)
			wake_up(&ipa->receiver->fill_recv_waitq);
	}
}
EXPORT_SYMBOL_GPL(sipa_recv_wake_up);

void sipa_init_free_fifo(struct sipa_skb_receiver *receiver, u32 cnt,
			 enum sipa_cmn_fifo_index id)
{
	int i;
	struct sipa_skb_array *skb_array;

	i = id - SIPA_FIFO_MAP0_OUT;
	if (i >= SIPA_RECV_QUEUES_MAX)
		return;

	skb_array = receiver->fill_array[i];
	sipa_hal_add_rx_fifo_wptr(receiver->dev,
				  receiver->ep->recv_fifo.idx + i,
				  cnt);
	if (atomic_read(&skb_array->need_fill_cnt) > 0)
		dev_info(receiver->dev,
			 "a very serious problem, mem cover may appear\n");

	atomic_set(&skb_array->need_fill_cnt, 0);
}
EXPORT_SYMBOL_GPL(sipa_init_free_fifo);

static void sipa_receiver_notify_cb(void *priv, enum sipa_hal_evt_type evt,
				    unsigned long data, int irq)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_skb_receiver *receiver = (struct sipa_skb_receiver *)priv;

	if (evt == SIPA_RECV_NO_EVT) {
		if (sipa_hal_check_cmn_fifo_enter_flowctl(SIPA_FIFO_WIFI_UL)) {
			sipa_hal_clr_cfifo_flowctl_enter_inter(SIPA_FIFO_WIFI_UL);
			dev_err(receiver->dev,
				"wifi ul fifo enter flowctrl, %d times\n",
				++receiver->wifi_ul_flowctrl);
			sipa_single_middle_core();
		}

		if (sipa_hal_check_cmn_fifo_exit_flowctl(SIPA_FIFO_WIFI_UL)) {
			sipa_hal_clr_cfifo_flowctl_exit_inter(SIPA_FIFO_WIFI_UL);
			dev_err(receiver->dev,
				"wifi ul fifo exit flowctrl\n");
		}
		/* there is no interrupt for map cfifo */
		return;
	}

	if (evt & SIPA_RECV_WARN_EVT) {
		if (net_ratelimit())
			dev_err(receiver->dev,
				"sipa maybe poor resources evt = 0x%x\n", evt);
		receiver->tx_danger_cnt++;
	}

	sipa_dummy_recv_trigger(irq - ipa->multi_intr[0]);
}

static bool sipa_check_recv_node_value(struct sipa_node_desc_tag *node,
				       dma_addr_t addr,
				       enum sipa_cmn_fifo_index id,
				       struct sk_buff *recv_skb)
{
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_skb_receiver *receiver = ipa->receiver;
	struct skb_shared_info *shinfo;
	int retry = 10;

	if (node->length > SIPA_RECV_BUF_LEN - NET_SKB_PAD -
	    SKB_DATA_ALIGN(sizeof(struct skb_shared_info))) {
		dev_err(receiver->dev, "node transfer length long is %d\n",
			node->length);
		shinfo = skb_shinfo(recv_skb);
		memset(shinfo, 0, offsetof(struct skb_shared_info, dataref));

		return false;
	}

	while ((addr != node->address || !node->src) && --retry)
		sipa_hal_sync_node_from_tx_fifo(receiver->dev, id, -1);

	if (!retry) {
		dev_info(receiver->dev,
			 "recv addr:0x%llx, recv_array addr:0x%llx not equal retry = %d src = %d, fifoid=%d\n",
			 (u64)node->address, (u64)addr, retry, node->src,
			 id - receiver->ep->recv_fifo.idx);

		return false;
	}

	return true;
}

struct sk_buff *sipa_recv_skb(struct sipa_skb_receiver *receiver,
			      struct sipa_node_info *node_info,
			      u32 index, int fifoid)
{
	dma_addr_t addr;
	bool need_unmap = false;
	int ret;
	enum sipa_cmn_fifo_index id;
	struct sk_buff *recv_skb = NULL;
	struct sipa_node_desc_tag *node = NULL;
	struct sipa_plat_drv_cfg *ipa = sipa_get_ctrl_pointer();
	struct sipa_skb_array *fill_array = receiver->fill_array[fifoid];

	atomic_inc(&receiver->check_flag);

	if (ipa->sipa_sys_eb) {
		if (time_after(jiffies + msecs_to_jiffies(50),
			       ipa->dev->power.wakeup->timer.expires)) {
			pm_wakeup_dev_event(ipa->dev, 500, true);
		}
	}

	if (atomic_read(&receiver->check_suspend)) {
		dev_warn(receiver->dev,
			 "encounter ipa suspend while reading data cpu = %d\n",
			 smp_processor_id());
		goto status_err;
	}

	id = receiver->ep->recv_fifo.idx + fifoid;
	if (sipa_hal_get_tx_fifo_empty_status(receiver->dev, id))
		goto status_err;

	node = sipa_hal_get_tx_node_rptr(receiver->dev, id, index);
	if (!node) {
		dev_err(receiver->dev, "recv node is null\n");
		goto tx_node_err;
	}

	ret = sipa_get_recv_array_node(fill_array, &recv_skb,
				       &addr, &need_unmap);
	if (ret) {
		dev_err(receiver->dev, "recv addr:0x%llx, but recv_array is empty\n",
			(u64)node->address);
		goto status_err;
	}

	if (!sipa_check_recv_node_value(node, addr, id, recv_skb))
		goto recv_err;

	if (need_unmap) {
		dma_unmap_single(receiver->dev, addr,
				 SIPA_RECV_BUF_LEN,
				 DMA_FROM_DEVICE);
	} else {
		dma_sync_single_range_for_cpu(receiver->dev, addr,
					      0,
					      SIPA_RECV_BUF_LEN,
					      DMA_FROM_DEVICE);

		skb_reserve(recv_skb, NET_SKB_PAD);
	}

	node_info->netid = node->net_id;
	node_info->src = node->src;
	node_info->dst = node->dst;
	node_info->n_ip_pkt = node->n_ip_pkt;

	if (node->checksum == 0xffff)
		recv_skb->ip_summed = CHECKSUM_UNNECESSARY;
	else
		recv_skb->ip_summed = CHECKSUM_NONE;

	skb_put(recv_skb, node->length);
	atomic_dec(&receiver->check_flag);

	return recv_skb;

recv_err:
	dev_kfree_skb_any(recv_skb);
	if (need_unmap)
		dma_unmap_single(receiver->dev, addr,
				 SIPA_RECV_BUF_LEN,
				 DMA_FROM_DEVICE);
tx_node_err:
	sipa_hal_add_tx_fifo_rptr(receiver->dev, id, 1);
	atomic_inc(&fill_array->need_fill_cnt);
status_err:
	atomic_dec(&receiver->check_flag);

	return NULL;
}

static int sipa_fill_recv_thread(void *data)
{
	struct sipa_skb_receiver *receiver = (struct sipa_skb_receiver *)data;
	struct sched_param param = {.sched_priority = 90};

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		wait_event_interruptible(receiver->fill_recv_waitq,
					 sipa_check_need_fill_cnt());

		if (sipa_fill_all_free_fifo())
			usleep_range(1000, 2000);
	}

	return 0;
}

int sipa_receiver_prepare_suspend(struct sipa_skb_receiver *receiver)
{
	int i;

	atomic_set(&receiver->check_suspend, 1);

	if (atomic_read(&receiver->check_flag)) {
		dev_err(receiver->dev,
			"task recv %d is running\n", receiver->ep->id);
		atomic_set(&receiver->check_suspend, 0);
		return -EAGAIN;
	}

	for (i = 0; i < SIPA_RECV_QUEUES_MAX; i++) {
		if (!sipa_hal_get_tx_fifo_empty_status(receiver->dev,
						       receiver->ep->recv_fifo.idx + i)) {
			dev_err(receiver->dev, "sipa recv fifo %d tx fifo is not empty\n",
				receiver->ep->recv_fifo.idx);
			atomic_set(&receiver->check_suspend, 0);
			return -EAGAIN;
		}
	}

	if (sipa_check_need_fill_cnt()) {
		dev_err(receiver->dev, "task fill_recv %d\n",
			receiver->ep->id);
		atomic_set(&receiver->check_suspend, 0);
		wake_up(&receiver->fill_recv_waitq);
		return -EAGAIN;
	}

	if (sipa_check_need_remaining_cnt() && sipa_fill_all_remaining_node()) {
		atomic_set(&receiver->check_suspend, 0);
		dev_err(receiver->dev, "sipa recv fifo need_fill remaining\n");
		return -EAGAIN;
	}

	return 0;
}

int sipa_receiver_prepare_resume(struct sipa_skb_receiver *receiver)
{
	atomic_set(&receiver->check_suspend, 0);

	wake_up_process(receiver->fill_recv_thread);

	return sipa_hal_cmn_fifo_stop_recv(receiver->dev,
					   receiver->ep->recv_fifo.idx,
					   false);
}

static void sipa_receiver_init(struct sipa_skb_receiver *receiver, u32 rsvd)
{
	int i;
	u32 depth;
	struct sipa_comm_fifo_params attr;

	/* timeout = 1 / ipa_sys_clk * 1024 * value */
	attr.tx_intr_delay_us = 0x64;
	attr.tx_intr_threshold = 0x30;
	attr.flowctrl_in_tx_full = true;
	attr.flow_ctrl_cfg = flow_ctrl_rx_empty;
	attr.flow_ctrl_irq_mode = enter_exit_flow_ctrl;
	attr.rx_enter_flowctrl_watermark =
		receiver->ep->recv_fifo.rx_fifo.fifo_depth / 2;
	attr.rx_leave_flowctrl_watermark =
		receiver->ep->recv_fifo.rx_fifo.fifo_depth * 5 / 8;
	attr.tx_enter_flowctrl_watermark = 0;
	attr.tx_leave_flowctrl_watermark = 0;

	dev_info(receiver->dev,
		 "ep_id = %d fifo_id = %d rx_fifo depth = 0x%x queues = %d\n",
		 receiver->ep->id,
		 receiver->ep->recv_fifo.idx,
		 receiver->ep->recv_fifo.rx_fifo.fifo_depth,
		 SIPA_RECV_QUEUES_MAX);
	for (i = 0; i < SIPA_RECV_CMN_FIFO_NUM; i++)
		sipa_hal_open_cmn_fifo(receiver->dev,
				       receiver->ep->recv_fifo.idx + i,
				       &attr, NULL, false,
				       sipa_receiver_notify_cb,
				       receiver);

	/* reserve space for dma flushing cache issue */
	receiver->rsvd = rsvd;
	receiver->init_flag = true;

	atomic_set(&receiver->check_suspend, 0);
	atomic_set(&receiver->check_flag, 0);

	depth = receiver->ep->recv_fifo.rx_fifo.fifo_depth;
	sipa_prepare_free_node_init(receiver, depth, SIPA_RECV_QUEUES_MAX);
}

int sipa_create_skb_receiver(struct sipa_plat_drv_cfg *ipa,
			     struct sipa_endpoint *ep,
			     struct sipa_skb_receiver **receiver_pp)
{
	struct sipa_skb_receiver *receiver = NULL;

	dev_info(ipa->dev, "ep->id = %d start\n", ep->id);
	receiver = devm_kzalloc(ipa->dev,
				sizeof(struct sipa_skb_receiver), GFP_KERNEL);
	if (!receiver)
		return -ENOMEM;

	receiver->dev = ipa->dev;
	receiver->ep = ep;
	receiver->rsvd = SIPA_RECV_RSVD_LEN;

	sipa_init_recv_array(receiver,
			     receiver->ep->recv_fifo.rx_fifo.fifo_depth);

	spin_lock_init(&receiver->lock);

	init_waitqueue_head(&receiver->fill_recv_waitq);
	sipa_receiver_init(receiver, SIPA_RECV_RSVD_LEN);

	receiver->fill_recv_thread = kthread_create(sipa_fill_recv_thread,
						    receiver,
						    "sipa-fill-recv-%d",
						    ep->id);
	if (IS_ERR(receiver->fill_recv_thread)) {
		dev_err(receiver->dev,
			"Failed to create kthread: sipa-fill-recv-%d\n",
			ep->id);
		return PTR_ERR(receiver->fill_recv_thread);
	}

	*receiver_pp = receiver;
	return 0;
}
EXPORT_SYMBOL_GPL(sipa_create_skb_receiver);
