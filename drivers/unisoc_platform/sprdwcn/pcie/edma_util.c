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

#include <misc/mchn.h>
#include <misc/wcn_bus.h>

#include "edma_engine.h"
#include "mchn.h"
#include "pcie.h"
#include "pcie_dbg.h"

#define TX_CHN (0)
#define RX_CHN (1)

struct cfg_e {
	int pool_size;
	int buf_size;
	int num;
	int chn[8][2];
};

struct test_link {
	struct mbuf_t  *head;
	struct mbuf_t  *tail;
	int      num;
	int      chn;
};

struct loopback {
	struct cfg_e  cfg;
	int    seq;
	int    loop;
	struct mchn_ops_t ops[16];
	struct test_link link[8][2];

};
struct loopback g_lo;

int cfg[] = {3, 1024, 2, 0, 1, 2, 3};

static struct mchn_ops_t *lo_ops(int chn)
{
	struct loopback *lo = &g_lo;

	return &lo->ops[chn];
}

static int lo_index(int chn)
{
	int i;
	struct loopback *lo = &g_lo;

	for (i = 0; i < lo->cfg.num; i++) {
		if ((chn == lo->link[i][TX_CHN].chn) ||
		   (chn == lo->link[i][RX_CHN].chn))
			return i;

	}
	WCN_INFO("%s(%d) err\n", __func__, chn);
	while (1)
		;

	return -1;
}

static int lo_buf_alloc(int chn, int size, int num)
{
	int ret, i;
	struct dma_buf dm = {0};
	struct mbuf_t *mbuf, *head, *tail;
	struct edma_info *edma = edma_info();

	ret = mbuf_link_alloc(chn, &head, &tail, &num);
	if (ret != 0)
		return -1;
	for (i = 0, mbuf = head; i < num; i++) {
		ret = dmalloc(edma->pcie_info, &dm, size);
		if (ret != 0)
			return -1;
		mbuf->buf = (unsigned char *)(dm.vir);
		mbuf->phy = (unsigned long)(dm.phy);
		mbuf->len = dm.size;
		memset(mbuf->buf, (unsigned char)(i+1), mbuf->len);
		mbuf = mbuf->next;
	}
	ret = mbuf_link_free(chn, head, tail, num);

	return ret;
}

static int lo_tx_pop(int chn, struct mbuf_t *head, struct mbuf_t *tail,
		     int num)
{
	struct loopback *lo = &g_lo;
	struct test_link *tx_link = &(lo->link[lo_index(chn)][TX_CHN]);

	if (tx_link->num == 0) {
		tx_link->head = head;
		tx_link->tail = tail;
		tx_link->num  = num;
	} else {
		tx_link->tail->next = head;
		tx_link->tail = tail;
		tx_link->num += num;
	}

	return 0;
}

static int lo_push(int chn)
{
	int num, ret, i, j;
	struct mchn_ops_t *ops = lo_ops(chn);
	struct mbuf_t *head, *tail, *mbuf;
	struct loopback *lo = &g_lo;

	num = ops->pool_size;
	ret = mbuf_link_alloc(chn, &head, &tail, &num);
	for (i = 0, mbuf = head; i < num; i++) {
		for (j = 0; j < mbuf->len/4; j++)
			*(int *)(mbuf->buf + j * 4) = lo->seq;

		lo->seq++;
		mbuf = mbuf->next;
	}
	ret = mchn_push_link(chn, head, tail, num);

	return ret;
}

static int lo_rx_push(int chn, struct mbuf_t **head, struct mbuf_t **tail,
		      int *num)
{
	int ret = mbuf_link_alloc(chn, head, tail, num);

	return ret;
}

static int lo_rx_pop(int chn, struct mbuf_t *head, struct mbuf_t *tail,
		     int num)
{
	int i;
	int pos = 0;
	unsigned char string[128];
	struct mbuf_t *tx_mbuf, *rx_mbuf;
	struct loopback *lo = &g_lo;
	struct mchn_ops_t *ops = lo_ops(chn);
	struct test_link *tx_link = &(lo->link[lo_index(chn)][TX_CHN]);
	struct test_link *rx_link = &(lo->link[lo_index(chn)][RX_CHN]);

	if (rx_link->num == 0) {
		rx_link->head = head;
		rx_link->tail = tail;
		rx_link->num  = num;
		rx_link->chn  = chn;
	} else {
		rx_link->tail->next = head;
		rx_link->tail = (void *)tail;
		rx_link->num += num;
	}
	if (rx_link->num == ops->pool_size) {
		//todo...
		if (tx_link->num != rx_link->num) {
			WCN_ERR("%s line:%d err\n", __func__, __LINE__);
			while (1)
				;

			return -1;
		}
		pos = sprintf(string+pos, "lo(%d,%d){",
			      tx_link->chn, rx_link->chn);
		for (i = 0, tx_mbuf = tx_link->head, rx_mbuf = rx_link->head;
		    i < tx_link->num; i++) {
			if (memcmp(tx_mbuf->buf, rx_mbuf->buf, 1024) != 0) {
				WCN_ERR("%s line:%d err\n", __func__,
					__LINE__);
				while (1)
					;
			}
			pos += sprintf(string+pos, "%d ",
				       *(int *)(tx_mbuf->buf));
			tx_mbuf = tx_mbuf->next;
			rx_mbuf = rx_mbuf->next;
		}
		WCN_INFO("%s}\n", string);
		mbuf_link_free(rx_link->chn, rx_link->head, rx_link->tail,
			       rx_link->num);
		mbuf_link_free(tx_link->chn, tx_link->head, tx_link->tail,
			       tx_link->num);

		rx_link->num = 0;
		rx_link->head = rx_link->tail = NULL;

		tx_link->num = 0;
		tx_link->head = tx_link->tail = NULL;

		if (lo->loop)
			lo_push(tx_link->chn);
	}

	return 0;
}

static int lo_tx_complete(int chn, int timeout)
{
	return 0;
}

int lo_init(void)
{
	int i, tx_chn, rx_chn;
	struct mchn_ops_t *ops;
	struct loopback *lo = &g_lo;

	WCN_INFO("[+]%s\n", __func__);
	memset(lo, 0x00, sizeof(struct loopback));
	for (i = 0; i < 8; i++) {
		lo->link[i][0].chn = -1;
		lo->link[i][1].chn = -1;
	}
	memcpy((unsigned char *)(&lo->cfg), (unsigned char *)(&cfg),
		sizeof(cfg));
	for (i = 0; i < lo->cfg.num; i++) {
		tx_chn = lo->cfg.chn[i][TX_CHN];
		rx_chn = lo->cfg.chn[i][RX_CHN];

		ops = lo_ops(tx_chn);
		ops->channel = tx_chn;
		ops->inout = 1;
		ops->hif_type = HW_TYPE_PCIE;
		ops->buf_size = lo->cfg.buf_size;
		ops->pool_size = lo->cfg.pool_size;
		ops->cb_in_irq = 0;
		ops->pop_link = lo_tx_pop;
		ops->tx_complete = lo_tx_complete;
		mchn_init(ops);
		lo_buf_alloc(ops->channel, ops->buf_size, ops->pool_size);

		ops = lo_ops(rx_chn);
		ops->channel = rx_chn;
		ops->inout = 0;
		ops->hif_type = HW_TYPE_PCIE;
		ops->buf_size = lo->cfg.buf_size;
		ops->pool_size = lo->cfg.pool_size;
		ops->cb_in_irq = 0;
		ops->pop_link = lo_rx_pop;
		ops->push_link = lo_rx_push;
		mchn_init(ops);
		lo_buf_alloc(ops->channel, ops->buf_size, ops->pool_size);

		lo->link[i][TX_CHN].chn = tx_chn;
		lo->link[i][RX_CHN].chn = rx_chn;
	}
	WCN_INFO("[-]%s\n", __func__);

	return 0;
}

int lo_start(int mode)
{
	static int lo_init_state;
	int i, tx_chn;
	struct loopback *lo = &g_lo;

	WCN_INFO("[+]%s\n", __func__);
	if (lo_init_state == 0) {
		lo_init();
		lo_init_state = 1;
	}
	lo->loop = mode;
	lo->seq = 0;
	for (i = 0; i < lo->cfg.num; i++) {
		tx_chn = lo->link[i][TX_CHN].chn;
		lo_push(tx_chn);
	}
	WCN_INFO("[-]%s\n", __func__);

	return 0;
}

int lo_stop(void)
{
	struct loopback *lo = &g_lo;

	lo->loop = 0;

	return 0;
}
