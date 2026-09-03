/*
 * Copyright (C) 2016-2018 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <misc/marlin_platform.h>
#include <misc/wcn_bus.h>

#include "edma_engine.h"
#include "mchn.h"
#include "pcie_dbg.h"

#define TX 1
#define RX 0

static struct mchn_info_t g_mchn;

struct mchn_info_t *mchn_info(void)
{
	return &g_mchn;
}

struct mchn_ops_t *mchn_ops(int channel)
{
	return g_mchn.ops[channel];
}

int mbuf_link_alloc(int chn, struct mbuf_t **head, struct mbuf_t **tail,
		    int *num)
{
	int i;
	struct mbuf_t *cur, *head__, *tail__ = NULL;
	struct mchn_info_t *mchn = mchn_info();
	struct buffer_pool *pool = &(mchn->chn_public[chn].pool);

	WCN_DBG("pool=%p, chn=%d, free=%d\n", pool, chn, pool->free);
	spin_lock_irqsave(&(pool->lock), pool->irq_flags);
	if ((*num <= 0) || (pool->free <= 0)) {
		WCN_ERR("[+]%s err, num %d, free %d)\n",
			__func__, *num, pool->free);
		*num = 0;
		*head = *tail = NULL;
		spin_unlock_irqrestore(&(pool->lock), pool->irq_flags);
		return -1;
	}
	if (*num > pool->free)
		*num = pool->free;

	for (i = 0, cur = head__ = pool->head; i < *num; i++) {

		if (i == (*num - 1))
			tail__ = cur;

		cur = cur->next;
	}
	*head = head__;
	tail__->next = NULL;
	*tail = tail__;
	pool->free -= *num;
	pool->head = cur;
	spin_unlock_irqrestore(&(pool->lock), pool->irq_flags);

	return 0;
}
EXPORT_SYMBOL(mbuf_link_alloc);

int mbuf_link_free(int chn, struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	struct mchn_info_t *mchn = mchn_info();
	struct buffer_pool *pool = &(mchn->chn_public[chn].pool);

	if ((head == NULL) || (tail == NULL) || (num == 0) ||
	    (tail->next != 0)) {
		WCN_ERR("%s@%d(%d, 0x%p, 0x%p, %d)err\n",
			__func__, __LINE__, chn, head, tail, num);

		return -1;
	}
	spin_lock_irqsave(&(pool->lock), pool->irq_flags);
	tail->next = pool->head;
	pool->head = head;
	pool->free += num;
	spin_unlock_irqrestore(&(pool->lock), pool->irq_flags);

	return 0;
}
EXPORT_SYMBOL(mbuf_link_free);

int mbuf_pool_init(struct buffer_pool *pool, int size, int payload)
{
	int i;
	struct mbuf_t *mbuf, *next;

	pool->size = size;
	pool->payload = payload;
	spin_lock_init(&(pool->lock));
	pool->mem = kmalloc((sizeof(struct mbuf_t) + payload) * size,
			     GFP_KERNEL);
	WCN_DBG("mbuf_pool->mem:0x%lx\n",
		 (unsigned long)virt_to_phys(pool->mem));
	memset(pool->mem, 0x00, (sizeof(struct mbuf_t) + payload) * size);
	pool->head = (struct mbuf_t *) (pool->mem);
	for (i = 0, mbuf = (struct mbuf_t *) (pool->head);
	     i < (size - 1); i++) {
		mbuf->seq = i;
		WCN_DBG("%s mbuf[%d]:{0x%lx, 0x%lx}\n", __func__, i,
			(unsigned long)mbuf,
			(unsigned long)virt_to_phys(mbuf));
		next = (struct mbuf_t *) ((char *)mbuf +
			sizeof(struct mbuf_t) + payload);
		mbuf->buf = (char *)mbuf + sizeof(struct mbuf_t);
		mbuf->len = payload;
		mbuf->next = next;
		mbuf = next;
	}
	WCN_DBG("%s mbuf[%d]:{0x%lx, 0x%lx}\n", __func__, i,
		 (unsigned long)mbuf,
		 (unsigned long)virt_to_phys(mbuf));
	mbuf->seq = i;
	mbuf->buf = (char *)mbuf + sizeof(struct mbuf_t);
	mbuf->len = payload;
	mbuf->next = NULL;
	pool->free = size;
	WCN_DBG("%s(0x%p, %d)\n", __func__, pool, pool->free);

	return 0;
}

int mbuf_pool_deinit(struct buffer_pool *pool)
{
	memset(pool->mem, 0x00, (sizeof(struct mbuf_t) +
	       pool->payload) * pool->size);
	kfree(pool->mem);

	return 0;
}

int mchn_hw_pop_link(int chn, void *head, void *tail, int num)
{
	struct mchn_info_t *mchn = mchn_info();

	if (mchn->ops[chn] == NULL) {
		WARN_ON(1);
		return -1;
	}
	if (mchn->ops[chn]->hif_type == HW_TYPE_PCIE)
		edma_tp_count(chn, head, tail, num);

	return mchn->ops[chn]->pop_link(chn, (struct mbuf_t *)head,
					(struct mbuf_t *)tail, num);
}
EXPORT_SYMBOL(mchn_hw_pop_link);

int mchn_hw_tx_complete(int chn, int timeout)
{
	struct mchn_info_t *mchn = mchn_info();

	if (mchn->ops[chn] == NULL)
		WARN_ON(1);
	if (mchn->ops[chn]->tx_complete)
		mchn->ops[chn]->tx_complete(chn, timeout);

	return 0;
}
EXPORT_SYMBOL(mchn_hw_tx_complete);

int mchn_hw_req_push_link(int chn, int need)
{
	int ret;
	struct mbuf_t *head = NULL, *tail = NULL;
	struct mchn_info_t *mchn = mchn_info();

	if (mchn->ops[chn] == NULL)
		return -1;

	ret = mchn->ops[chn]->push_link(chn, &head, &tail, &need);
	if (ret != 0)
		return ret;
	ret = mchn_push_link(chn, (void *)head, (void *)tail, need);

	return ret;
}
EXPORT_SYMBOL(mchn_hw_req_push_link);

int mchn_hw_cb_in_irq(int chn)
{
	if (!g_mchn.ops[chn]) {
		WCN_ERR("%s: chn=%d is not register\n", __func__, chn);
		return -1;
	}

	return g_mchn.ops[chn]->cb_in_irq;
}

int mchn_hw_max_pending(int chn)
{
	if (!g_mchn.ops[chn]) {
		WCN_ERR("%s: chn=%d is not register\n", __func__, chn);
		return -1;
	}

	return g_mchn.ops[chn]->max_pending;
}

int mchn_push_link(int chn, struct mbuf_t *head, struct mbuf_t *tail, int num)
{
	int ret = -1;
	struct mchn_info_t *mchn = mchn_info();

	if ((chn >= 16) || (mchn->ops[chn] == NULL) || (head == NULL) ||
	    (tail == NULL) || (num > mchn->ops[chn]->pool_size)) {
		WCN_ERR("%s: chn=%d, num=%d,head=%p, tail=%p\n", __func__,
			chn, num, head, tail);
		dump_stack();
		return -1;
	}

	if (!wcn_get_edma_status() && (mchn->ops[chn]->inout == RX)) {
		WCN_ERR("%s:edma not ready, chn=%d\n", __func__, chn);
		return -1;
	}

	if (!marlin_get_download_status() && (mchn->ops[chn]->inout == TX)) {
		WCN_ERR("%s:boot not ready, chn=%d\n", __func__, chn);
		return -1;
	}

	if (mchn->ops[chn]->inout == TX)
		wcn_set_tx_complete_status(0);

	switch (mchn->ops[chn]->hif_type) {
	case HW_TYPE_PCIE:
		if (mchn_hw_max_pending(chn) > 0)
			ret = edma_push_link_async(chn, (void *)head,
						   (void *)tail, num);
		else
			ret = edma_push_link(chn, (void *)head,
					     (void *)tail, num);
		break;
	default:
		break;
	}

	return ret;
}
EXPORT_SYMBOL(mchn_push_link);

int mchn_push_link_wait_complete(int chn, struct mbuf_t *head,
				 struct mbuf_t *tail, int num, int timeout)
{
	int ret = -1;
	struct mchn_info_t *mchn = mchn_info();

	if ((chn >= 32) || (mchn->ops[chn] == NULL)) {
		WARN_ON(1);
		return -1;
	}
	switch (mchn->ops[chn]->hif_type) {
	case HW_TYPE_PCIE:
		ret = edma_push_link_wait_complete(chn, (void *)head,
						   (void *)tail, num, timeout);
		break;
	default:
		break;
	}

	return ret;
}
EXPORT_SYMBOL(mchn_push_link_wait_complete);

int mchn_wcn_mem_write(unsigned int addr, void *buf, unsigned int len)
{
	return sprd_pcie_mem_write(addr, buf, len);
}
EXPORT_SYMBOL(mchn_wcn_mem_write);

int mchn_wcn_mem_read(unsigned int addr, void *buf, unsigned int len)
{
	return sprd_pcie_mem_read(addr, buf, len);
}
EXPORT_SYMBOL(mchn_wcn_mem_read);

int mchn_wcn_update_bits(unsigned int reg, unsigned int mask, unsigned int val)
{
	return sprd_pcie_update_bits(reg, mask, val);
}
EXPORT_SYMBOL(mchn_wcn_update_bits);

int mchn_init(struct mchn_ops_t *ops)
{
	int ret = -1;
	struct mchn_info_t *mchn = mchn_info();

	WCN_DBG("[+]%s(chn=%d)\n", __func__, ops->channel);
	if (ops->hif_type != HW_TYPE_PCIE) {
		WCN_INFO("%s err, hif_type %d\n", __func__, ops->hif_type);
		WARN_ON(1);

		return -1;
	}
	mchn->ops[ops->channel] = ops;

	switch (ops->hif_type) {
	case HW_TYPE_PCIE:
		ret = edma_chn_init(ops->channel, 0, ops->inout,
				    ops->pool_size);
		break;
	default:
		break;
	}

	if ((ret == 0) && (ops->pool_size > 0))
		ret = mbuf_pool_init(&(mchn->chn_public[ops->channel].pool),
				     ops->pool_size, 0);
	WCN_DBG("[-]%s(%d)\n", __func__, ops->channel);

	return ret;
}
EXPORT_SYMBOL(mchn_init);

int mchn_deinit(struct mchn_ops_t *ops)
{
	int ret = 0;
	struct mchn_info_t *mchn = mchn_info();

	WCN_INFO("[+]%s(%d, %d)\n", __func__, ops->channel, ops->hif_type);

	if ((mchn->ops[ops->channel] == NULL) ||
	    (ops->hif_type != HW_TYPE_PCIE)) {
		WCN_ERR("%s err\n", __func__);
		return -1;
	}
	switch (ops->hif_type) {
	case HW_TYPE_PCIE:
		ret = edma_chn_deinit(ops->channel);
		break;
	default:
		break;
	}
	if (ops->pool_size > 0)
		ret = mbuf_pool_deinit(&(mchn->chn_public[ops->channel].pool));
	mchn->ops[ops->channel] = NULL;
	WCN_INFO("[-]%s(%d)\n", __func__, ops->channel);

	return ret;
}
EXPORT_SYMBOL(mchn_deinit);

