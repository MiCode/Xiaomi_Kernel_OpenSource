// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>

#include "ccci_config.h"
#include "ccci_core.h"
#include "ccci_hif_dpmaif.h"
#include "dpmaif_drv.h"
#include "dpmaif_bat.h"


#define MAX_ALLOC_BAT_CNT (100000)

#define TAG "bat"

struct temp_skb_info {
	struct sk_buff *skb;
	unsigned long long base_addr;
};

struct temp_page_info {
	struct page *page;
	unsigned long long base_addr;
	unsigned long offset;
};

#define MAX_INFO_COUNT 1000

static struct temp_skb_info g_skb_tbl[MAX_INFO_COUNT];
static unsigned int g_skb_tbl_cnt;
static unsigned int g_skb_tbl_idx;


static struct temp_page_info g_page_tbl[MAX_INFO_COUNT];
static unsigned int g_page_tbl_cnt;
static unsigned int g_page_tbl_idx;


static inline struct device *ccci_md_get_dev_by_id(int md_id)
{
	return &dpmaif_ctrl->plat_dev->dev;
}

static inline unsigned int ringbuf_writeable(
		unsigned int  total_cnt,
		unsigned int rel_idx, unsigned int  wrt_idx)
{
	unsigned int pkt_cnt = 0;

	if (wrt_idx < rel_idx)
		pkt_cnt = rel_idx - wrt_idx - 1;
	else
		pkt_cnt = total_cnt + rel_idx - wrt_idx - 1;

	return pkt_cnt;
}

static inline unsigned int ringbuf_get_next_idx(
		unsigned int buf_len,
		unsigned int buf_idx, unsigned int cnt)
{
	buf_idx += cnt;

	if (buf_idx >= buf_len)
		buf_idx -= buf_len;

	return buf_idx;
}

static inline int skb_alloc(
		struct sk_buff **ppskb,
		unsigned long long *p_base_addr,
		unsigned int pkt_buf_sz,
		int blocking)
{
	unsigned int rty_cnt = 0;

fast_retry:
	(*ppskb) = __dev_alloc_skb(pkt_buf_sz,
					(blocking ? GFP_KERNEL : GFP_ATOMIC));

	if (unlikely(!(*ppskb))) {
		if ((!blocking) && (rty_cnt++) < 20)
			goto fast_retry;

		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: alloc skb fail. (%u, %u)\n",
			__func__, pkt_buf_sz, rty_cnt);

		return LOW_MEMORY_SKB;
	}

	(*p_base_addr) = dma_map_single(
				ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
				(*ppskb)->data,
				skb_data_size((*ppskb)),
				DMA_FROM_DEVICE);

	if (dma_mapping_error(ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
				(*p_base_addr))) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: dma mapping fail: %ld!\n",
			__func__, skb_data_size(*ppskb));

		ccci_free_skb(*ppskb);
		(*ppskb) = NULL;

		return DMA_MAPPING_ERR;
	}

	return 0;
}

static inline void alloc_skb_to_tbl(int skb_cnt, int blocking)
{
	int alloc_cnt, i;
	unsigned int cur_tbl_idx;
	struct temp_skb_info *skb_info;
	unsigned int pkt_buf_sz = dpmaif_ctrl->bat_req->pkt_buf_sz;

	if (skb_cnt > MAX_INFO_COUNT)
		skb_cnt = MAX_INFO_COUNT;

	if (skb_cnt <= g_skb_tbl_cnt)
		return;

	alloc_cnt = skb_cnt - g_skb_tbl_cnt;
	cur_tbl_idx = ringbuf_get_next_idx(MAX_INFO_COUNT,
					g_skb_tbl_idx, g_skb_tbl_cnt);

	for (i = 0; i < alloc_cnt; i++) {
		skb_info = &g_skb_tbl[cur_tbl_idx];
		if (skb_alloc(&skb_info->skb, &skb_info->base_addr,
						pkt_buf_sz, blocking))
			break;
		/*
		 * The wmb() flushes writes to dram before read g_skb_tbl data.
		 */
		wmb();

		cur_tbl_idx = ringbuf_get_next_idx(MAX_INFO_COUNT,
						cur_tbl_idx, 1);
		g_skb_tbl_cnt++;
	}
}

static inline int get_skb_info_from_tbl(struct temp_skb_info *skb_info)
{
	if (g_skb_tbl_cnt > 0) {
		(*skb_info) = g_skb_tbl[g_skb_tbl_idx];

		g_skb_tbl_idx = ringbuf_get_next_idx(MAX_INFO_COUNT,
						g_skb_tbl_idx, 1);

		g_skb_tbl_cnt--;

		return 0;
	}

	return -1;
}

static inline int page_alloc(
		struct page **pp_page,
		unsigned long long *p_base_addr,
		unsigned long *offset,
		unsigned int pkt_buf_sz,
		int blocking)
{
	unsigned int rty_cnt = 0;
	int size = L1_CACHE_ALIGN(pkt_buf_sz);
	void *data;

fast_retry:
	data = netdev_alloc_frag(size);/* napi_alloc_frag(size) */
	if (unlikely(!data)) {
		if ((!blocking) && (rty_cnt++) < 20)
			goto fast_retry;

		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: alloc frag fail. (%u,%d)\n",
			__func__, size, blocking);

		return LOW_MEMORY_BAT; /*-ENOMEM;*/
	}

	(*pp_page) = virt_to_head_page(data);
	(*offset) = data - page_address((*pp_page));

	/* Get physical address of the RB */
	(*p_base_addr) = dma_map_page(
				ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
				(*pp_page), *offset,
				pkt_buf_sz,
				DMA_FROM_DEVICE);

	if (dma_mapping_error(ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
			(*p_base_addr))) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: dma mapping: %d\n",
			__func__, pkt_buf_sz);

		put_page((*pp_page));
		(*pp_page) = NULL;

		return DMA_MAPPING_ERR;
	}

	return 0;
}

static inline int get_page_info_from_tbl(struct temp_page_info *page_info)
{
	if (g_page_tbl_cnt > 0) {
		(*page_info) = g_page_tbl[g_page_tbl_idx];

		g_page_tbl_idx = ringbuf_get_next_idx(MAX_INFO_COUNT,
					g_page_tbl_idx, 1);

		g_page_tbl_cnt--;

		return 0;
	}

	return -1;
}

static inline void alloc_page_to_tbl(int page_cnt, int blocking)
{
	int alloc_cnt, i;
	unsigned int cur_tbl_idx;
	struct temp_page_info *page_info;
	unsigned int pkt_buf_sz = dpmaif_ctrl->bat_frag->pkt_buf_sz;

	if (page_cnt > MAX_INFO_COUNT)
		page_cnt = MAX_INFO_COUNT;

	if (page_cnt <= g_page_tbl_cnt)
		return;

	alloc_cnt = page_cnt - g_page_tbl_cnt;
	cur_tbl_idx = ringbuf_get_next_idx(MAX_INFO_COUNT,
					g_page_tbl_idx, g_page_tbl_cnt);

	for (i = 0; i < alloc_cnt; i++) {
		page_info = &g_page_tbl[cur_tbl_idx];
		if (page_alloc(&page_info->page, &page_info->base_addr,
				&page_info->offset, pkt_buf_sz, blocking))
			break;
		/*
		 * The wmb() flushes writes to dram before read g_skb_tbl data.
		 */
		wmb();

		cur_tbl_idx = ringbuf_get_next_idx(MAX_INFO_COUNT,
						cur_tbl_idx, 1);
		g_page_tbl_cnt++;
	}
}

static struct dpmaif_bat_request *ccci_dpmaif_bat_create(void)
{
	struct dpmaif_bat_request *bat_req;

	bat_req = kzalloc(sizeof(struct dpmaif_bat_request), GFP_KERNEL);

	if (!bat_req)
		CCCI_ERROR_LOG(-1, TAG, "alloc bat fail.\n");

	return bat_req;
}

static int dpmaif_bat_init(struct dpmaif_bat_request *bat_req,
		int is_frag)
{
	int sw_buf_size = is_frag ? sizeof(struct dpmaif_bat_page_t) :
		sizeof(struct dpmaif_bat_skb_t);

	bat_req->bat_size_cnt = DPMAIF_DL_BAT_ENTRY_SIZE;
	bat_req->skb_pkt_cnt = bat_req->bat_size_cnt;
	bat_req->pkt_buf_sz = is_frag ? DPMAIF_BUF_FRAG_SIZE :
							DPMAIF_BUF_PKT_SIZE;

	/* alloc buffer for HW && AP SW */
#if (DPMAIF_DL_BAT_SIZE > PAGE_SIZE)
	 bat_req->bat_base = dma_alloc_coherent(
		ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
		(bat_req->bat_size_cnt * sizeof(struct dpmaif_bat_t)),
		&bat_req->bat_phy_addr, GFP_KERNEL);
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "bat dma_alloc_coherent\n");
#endif
#else
	bat_req->bat_base = dma_pool_alloc(dpmaif_ctrl->rx_bat_dmapool,
		GFP_KERNEL, &bat_req->bat_phy_addr);
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(-1, TAG, "bat dma_pool_alloc\n");
#endif
#endif
	/* alloc buffer for AP SW to record skb information */

	bat_req->bat_skb_ptr = kzalloc((bat_req->skb_pkt_cnt *
		sw_buf_size), GFP_KERNEL);
	if (bat_req->bat_base == NULL || bat_req->bat_skb_ptr == NULL) {
		CCCI_ERROR_LOG(-1, TAG, "bat request fail\n");
		return LOW_MEMORY_BAT;
	}

	memset(bat_req->bat_base, 0,
		(bat_req->bat_size_cnt * sizeof(struct dpmaif_bat_t)));

	return 0;
}

static inline int alloc_bat_skb(
		unsigned int pkt_buf_sz,
		struct dpmaif_bat_skb_t *bat_skb,
		struct dpmaif_bat_t *cur_bat,
		int blocking)
{
	int ret = 0;
	unsigned long long data_base_addr;
	struct temp_skb_info skb_info;

	if (!get_skb_info_from_tbl(&skb_info)) {
		bat_skb->skb = skb_info.skb;
		data_base_addr = skb_info.base_addr;

	} else {
		ret = skb_alloc(&bat_skb->skb, &data_base_addr,
			pkt_buf_sz, blocking);
		if (ret)
			return ret;
	}

	bat_skb->data_phy_addr = data_base_addr;
	bat_skb->data_len = skb_data_size(bat_skb->skb);

	cur_bat->buffer_addr_ext = (data_base_addr >> 32) & 0xFF;
	cur_bat->p_buffer_addr = (unsigned int)(data_base_addr & 0xFFFFFFFF);

	return 0;
}

static int dpmaif_alloc_bat_req(int update_bat_cnt,
		int request_cnt, atomic_t *paused, int blocking)
{
	struct dpmaif_bat_request *bat_req = dpmaif_ctrl->bat_req;
	struct dpmaif_bat_skb_t *bat_skb, *next_skb;
	struct dpmaif_bat_t *cur_bat;
	unsigned int buf_space;
	int count = 0, ret = 0;
	unsigned short bat_wr_idx, next_wr_idx;

	bat_req->bat_rd_idx = drv_dpmaif_dl_get_bat_ridx(0);

	buf_space = ringbuf_writeable(bat_req->bat_size_cnt,
				bat_req->bat_rd_idx, bat_req->bat_wr_idx);

	if (request_cnt > buf_space)
		request_cnt = buf_space;

	if (request_cnt == 0)
		return 0;

	bat_wr_idx = bat_req->bat_wr_idx;

	//while ((!atomic_read(&dpmaif_ctrl->bat_paused_alloc))
	while (((!paused) || (!atomic_read(paused)))
			&& (count < request_cnt)) {
		bat_skb = (struct dpmaif_bat_skb_t *)bat_req->bat_skb_ptr
					+ bat_wr_idx;
		if (bat_skb->skb)
			break;

		next_wr_idx = ringbuf_get_next_idx(
						bat_req->bat_size_cnt, bat_wr_idx, 1);

		next_skb = (struct dpmaif_bat_skb_t *)bat_req->bat_skb_ptr
					+ next_wr_idx;
		if (next_skb->skb)
			break;

		cur_bat = (struct dpmaif_bat_t *)bat_req->bat_base
					+ bat_wr_idx;

		ret = alloc_bat_skb(bat_req->pkt_buf_sz,
					bat_skb, cur_bat, blocking);
		if (ret)
			goto alloc_end;

		bat_wr_idx = next_wr_idx;
		count++;
	}

alloc_end:
	if (count > 0) {
		/* wait write done */
		wmb();
		bat_req->bat_wr_idx = bat_wr_idx;

		if (update_bat_cnt) {
			ret = drv_dpmaif_dl_add_bat_cnt(0, count);
			if (ret < 0)
				CCCI_ERROR_LOG(0, TAG,
					"[%s] dpmaif: update req cnt fail(%d)\n",
					__func__, ret);
		}
	}

	if (update_bat_cnt && (buf_space - count > 0))
		alloc_skb_to_tbl(buf_space - count, blocking);

	return ret;
}

static inline int alloc_bat_page(
		unsigned int pkt_buf_sz,
		struct dpmaif_bat_page_t *bat_page,
		struct dpmaif_bat_t *cur_bat,
		int blocking)
{
	unsigned long long data_base_addr;
	int ret;
	struct temp_page_info page_info;
	unsigned long offset;

	if (!get_page_info_from_tbl(&page_info)) {
		bat_page->page = page_info.page;
		data_base_addr = page_info.base_addr;
		offset = page_info.offset;

	} else {
		ret = page_alloc(&bat_page->page, &data_base_addr,
			&offset, pkt_buf_sz, blocking);
		if (ret)
			return ret;
	}

	bat_page->data_phy_addr = data_base_addr;
	bat_page->data_len = pkt_buf_sz;
	bat_page->offset = offset;

	cur_bat->buffer_addr_ext = (data_base_addr >> 32) & 0xFF;
	cur_bat->p_buffer_addr = (unsigned int)(data_base_addr & 0xFFFFFFFF);

	return 0;
}

static int dpmaif_alloc_bat_frg(int update_bat_cnt,
		int request_cnt, atomic_t *paused, int blocking)
{
	struct dpmaif_bat_request *bat_req = dpmaif_ctrl->bat_frag;
	struct dpmaif_bat_page_t *bat_page, *next_page;
	struct dpmaif_bat_t *cur_bat;
	unsigned int buf_space;
	int count = 0, ret = 0;
	unsigned short bat_wr_idx, next_wr_idx;

	bat_req->bat_rd_idx = drv_dpmaif_dl_get_frg_bat_ridx(0);

	buf_space = ringbuf_writeable(bat_req->bat_size_cnt,
				bat_req->bat_rd_idx, bat_req->bat_wr_idx);

	if (request_cnt > buf_space)
		request_cnt = buf_space;

	if (request_cnt == 0)
		return 0;

	bat_wr_idx = bat_req->bat_wr_idx;

	//while ((!atomic_read(&dpmaif_ctrl->bat_paused_alloc))
	while (((!paused) || (!atomic_read(paused)))
			&& (count < request_cnt)) {
		bat_page = (struct dpmaif_bat_page_t *)bat_req->bat_skb_ptr
					+ bat_wr_idx;
		if (bat_page->page)
			break;

		next_wr_idx = ringbuf_get_next_idx(
				bat_req->bat_size_cnt, bat_wr_idx, 1);

		next_page = (struct dpmaif_bat_page_t *)bat_req->bat_skb_ptr
					+ next_wr_idx;
		if (next_page->page)
			break;

		cur_bat = (struct dpmaif_bat_t *)bat_req->bat_base
					+ bat_wr_idx;

		ret = alloc_bat_page(bat_req->pkt_buf_sz,
					bat_page, cur_bat, blocking);
		if (ret)
			goto alloc_end;

		bat_wr_idx = next_wr_idx;

		count++;
	}

alloc_end:
	if (count > 0) {
		/* wait write done */
		wmb();
		bat_req->bat_wr_idx = bat_wr_idx;

		if (update_bat_cnt) {
			ret = drv_dpmaif_dl_add_frg_bat_cnt(0, count);
			if (ret < 0)
				CCCI_ERROR_LOG(0, TAG,
					"[%s] dpmaif: update frg cnt fail(%d)\n",
					__func__, ret);
		}
	}

	if (update_bat_cnt && (buf_space - count > 0))
		alloc_page_to_tbl(buf_space - count, blocking);

	return ret;
}

static void ccci_dpmaif_bat_free_req(void)
{
	int j;
	struct dpmaif_bat_skb_t *bat_skb;
	struct dpmaif_bat_request *bat_req = dpmaif_ctrl->bat_req;

	if ((!bat_req) || (!bat_req->bat_base) || (!bat_req->bat_skb_ptr))
		return;

	for (j = 0; j < bat_req->bat_size_cnt; j++) {
		bat_skb = ((struct dpmaif_bat_skb_t *)
				bat_req->bat_skb_ptr + j);

		if (bat_skb->skb) {
			/* rx unmapping */
			dma_unmap_single(
				ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
				bat_skb->data_phy_addr, bat_skb->data_len,
				DMA_FROM_DEVICE);

			ccci_free_skb(bat_skb->skb);
			bat_skb->skb = NULL;
		}
	}

	memset(bat_req->bat_base, 0,
		(bat_req->bat_size_cnt * sizeof(struct dpmaif_bat_t)));

	bat_req->bat_rd_idx = 0;
	bat_req->bat_wr_idx = 0;
}

static void ccci_dpmaif_bat_free_frg(void)
{
	int j;
	struct dpmaif_bat_page_t *bat_page;
	struct dpmaif_bat_request *bat_frg = dpmaif_ctrl->bat_frag;

	if ((!bat_frg) || (!bat_frg->bat_base) || (!bat_frg->bat_skb_ptr))
		return;

	for (j = 0; j < bat_frg->bat_size_cnt; j++) {
		bat_page = ((struct dpmaif_bat_page_t *)
				bat_frg->bat_skb_ptr + j);

		if (bat_page->page) {
			/* rx unmapping */
			dma_unmap_page(
				ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
				bat_page->data_phy_addr, bat_page->data_len,
				DMA_FROM_DEVICE);

			put_page(bat_page->page);
			bat_page->page = NULL;
		}
	}

	memset(bat_frg->bat_base, 0,
		(bat_frg->bat_size_cnt * sizeof(struct dpmaif_bat_t)));

	bat_frg->bat_rd_idx = 0;
	bat_frg->bat_wr_idx = 0;
}

static void ccci_dpmaif_bat_free(void)
{
	ccci_dpmaif_bat_free_req();

	ccci_dpmaif_bat_free_frg();
}

static int dpmaif_rx_bat_alloc_thread(void *arg)
{
	int ret;

	dpmaif_ctrl->bat_alloc_running = 1;

	CCCI_NORMAL_LOG(-1, TAG, "[%s] run start.\n", __func__);

	while (1) {
		ret = wait_event_interruptible(dpmaif_ctrl->bat_alloc_wq,
				atomic_read(&dpmaif_ctrl->bat_need_alloc));

		if (atomic_read(&dpmaif_ctrl->bat_paused_alloc)
				!= BAT_ALLOC_NO_PAUSED) {
			if (atomic_read(&dpmaif_ctrl->bat_paused_alloc)
					== BAT_ALLOC_IS_PAUSED) {
				atomic_set(&dpmaif_ctrl->bat_paused_alloc,
						BAT_ALLOC_PAUSE_SUCC);
				atomic_set(&dpmaif_ctrl->bat_need_alloc, 0);
			}
			continue;
		}

		if (ret == -ERESTARTSYS)
			continue;

		if (kthread_should_stop()) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: kthread_should_stop.\n",
				__func__);
			break;
		}

		ret = dpmaif_alloc_bat_req(1, MAX_ALLOC_BAT_CNT,
				&dpmaif_ctrl->bat_paused_alloc, 0);

		ret = dpmaif_alloc_bat_frg(1, MAX_ALLOC_BAT_CNT,
				&dpmaif_ctrl->bat_paused_alloc, 0);

		if (atomic_read(&dpmaif_ctrl->bat_need_alloc) > 1)
			atomic_set(&dpmaif_ctrl->bat_need_alloc, 1);
		else
			atomic_set(&dpmaif_ctrl->bat_need_alloc, 0);
	}

	dpmaif_ctrl->bat_alloc_running = 0;

	CCCI_NORMAL_LOG(-1, TAG, "[%s] run end.\n", __func__);

	return 0;
}

static int ccci_dpmaif_create_bat_thread(void)
{
	init_waitqueue_head(&dpmaif_ctrl->bat_alloc_wq);

	dpmaif_ctrl->bat_alloc_running = 0;
	atomic_set(&dpmaif_ctrl->bat_paused_alloc, BAT_ALLOC_IS_PAUSED);
	atomic_set(&dpmaif_ctrl->bat_need_alloc, 0);

	dpmaif_ctrl->bat_alloc_thread = kthread_run(
				dpmaif_rx_bat_alloc_thread,
				NULL, "bat_alloc_thread");

	if (IS_ERR(dpmaif_ctrl->bat_alloc_thread)) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] kthread_run fail %ld\n",
			__func__, (long)dpmaif_ctrl->bat_alloc_thread);

		dpmaif_ctrl->bat_alloc_thread = NULL;

		return -1;
	}

	return 0;
}

inline void ccci_dpmaif_bat_wakeup_thread(void)
{
	if (!dpmaif_ctrl->bat_alloc_thread)
		return;

	atomic_inc(&dpmaif_ctrl->bat_need_alloc);
	wake_up_all(&dpmaif_ctrl->bat_alloc_wq);
}

static void dpmaif_bat_start_thread(void)
{
	atomic_set(&dpmaif_ctrl->bat_paused_alloc, BAT_ALLOC_NO_PAUSED);

	ccci_dpmaif_bat_wakeup_thread();
}

static void ccci_dpmaif_bat_paused_thread(void)
{
	unsigned int retry_cnt = 0;

	if ((!dpmaif_ctrl->bat_alloc_thread) ||
		(!dpmaif_ctrl->bat_alloc_running)) {
		CCCI_NORMAL_LOG(-1, TAG,
			"[%s] thread no running: %d\n",
			__func__, dpmaif_ctrl->bat_alloc_running);
		return;
	}

	atomic_set(&dpmaif_ctrl->bat_paused_alloc, BAT_ALLOC_IS_PAUSED);

	do {
		ccci_dpmaif_bat_wakeup_thread();
		mdelay(1);

		retry_cnt++;
		if ((retry_cnt % 1000) == 0)
		/* print error log every 1s */
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: pause bat thread fail\n",
				__func__);

	} while (atomic_read(&dpmaif_ctrl->bat_paused_alloc)
			== BAT_ALLOC_IS_PAUSED);

	atomic_set(&dpmaif_ctrl->bat_need_alloc, 0);
	CCCI_MEM_LOG_TAG(0, TAG, "[%s] succ.\n", __func__);
}

void ccci_dpmaif_bat_stop(void)
{
	CCCI_NORMAL_LOG(0, TAG, "[%s]\n", __func__);

	ccci_dpmaif_bat_paused_thread();

	ccci_dpmaif_bat_free();
}

int ccci_dpmaif_bat_start(void)
{
	int ret = 0;

	CCCI_NORMAL_LOG(0, TAG, "[%s]\n", __func__);

	if ((!dpmaif_ctrl->bat_req) ||
		(!dpmaif_ctrl->bat_req->bat_base) ||
		(!dpmaif_ctrl->bat_req->bat_skb_ptr) ||
		(!dpmaif_ctrl->bat_frag) ||
		(!dpmaif_ctrl->bat_frag->bat_base) ||
		(!dpmaif_ctrl->bat_frag->bat_skb_ptr)) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] bat_req or bat_frag is NULL.\n", __func__);
		return -1;
	}

	ret = dpmaif_alloc_bat_req(0, MAX_ALLOC_BAT_CNT, NULL, 1);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] dpmaif_alloc_bat_req fail: %d\n",
			__func__, ret);
		goto start_err;
	}

	ret = dpmaif_alloc_bat_frg(0, MAX_ALLOC_BAT_CNT, NULL, 1);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] dpmaif_alloc_bat_frg fail: %d\n",
			__func__, ret);
		goto start_err;
	}

	ret = drv_dpmaif_dl_add_frg_bat_cnt(0,
			(dpmaif_ctrl->bat_frag->bat_size_cnt - 1));
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] add frag cnt failed after dl enable: %d\n",
			__func__, ret);
		goto start_err;
	}

	ret = drv_dpmaif_dl_add_bat_cnt(0,
			(dpmaif_ctrl->bat_req->bat_size_cnt - 1));
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] add req cnt failed after dl enable: %d\n",
			__func__, ret);
		goto start_err;
	}

	ret = drv_dpmaif_dl_all_frg_queue_en(true);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] enable all frg queue failed: %d\n",
			__func__, ret);
		goto start_err;
	}

	ret = drv_dpmaif_dl_all_queue_en(true);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] enable all req queue failed: %d\n",
			__func__, ret);
		goto start_err;
	}

	atomic_set(&dpmaif_ctrl->bat_paused_alloc, BAT_ALLOC_NO_PAUSED);
	dpmaif_bat_start_thread();

	return 0;

start_err:
	atomic_set(&dpmaif_ctrl->bat_paused_alloc, BAT_ALLOC_IS_PAUSED);

	return ret;
}

void ccci_dpmaif_bat_hw_init(void)
{
	if ((!dpmaif_ctrl->bat_req) ||
		(!dpmaif_ctrl->bat_frag)) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] bat_req or bat_frag is NULL.\n", __func__);
		return;
	}

	drv_dpmaif_dl_set_bat_bufsz(0, DPMAIF_HW_BAT_PKTBUF);
	drv_dpmaif_dl_set_bat_rsv_len(0, DPMAIF_HW_BAT_RSVLEN);
	drv_dpmaif_dl_set_bat_chk_thres(0, DPMAIF_HW_CHK_BAT_NUM);

	drv_dpmaif_dl_set_bat_base_addr(0,
			dpmaif_ctrl->bat_req->bat_phy_addr);

	drv_dpmaif_dl_set_bat_size(0,
			dpmaif_ctrl->bat_req->bat_size_cnt);

	/*
	 * Disable BAT in init and Enable BAT,
	 * when buffer submit to HW first time
	 */
	drv_dpmaif_dl_bat_en(0, false);

	/* 3. notify HW init/setting done*/
	CCCI_REPEAT_LOG(-1, TAG, "%s:begin check\n", __func__);
	drv_dpmaif_dl_bat_init_done(0, false);
	CCCI_REPEAT_LOG(-1, TAG, "%s: dl check done\n", __func__);

	/* 4. init frg buffer feature*/
	drv_dpmaif_dl_set_ao_frg_bat_feature(0, true);
	drv_dpmaif_dl_set_ao_frg_bat_bufsz(0, DPMAIF_HW_FRG_PKTBUF);
	drv_dpmaif_dl_set_ao_frag_check_thres(0, DPMAIF_HW_CHK_FRG_NUM);

	drv_dpmaif_dl_set_bat_base_addr(0,
			dpmaif_ctrl->bat_frag->bat_phy_addr);
	drv_dpmaif_dl_set_bat_size(0, dpmaif_ctrl->bat_frag->bat_size_cnt);

	drv_dpmaif_dl_bat_en(0, false);
	drv_dpmaif_dl_bat_init_done(0, true);
}

int ccci_dpmaif_bat_sw_init(void)
{
	int ret;

	g_skb_tbl_cnt = 0;
	g_skb_tbl_idx = 0;
	g_page_tbl_cnt = 0;
	g_page_tbl_idx = 0;

	dpmaif_ctrl->bat_req = ccci_dpmaif_bat_create();
	if (!dpmaif_ctrl->bat_req)
		return LOW_MEMORY_BAT;

	dpmaif_ctrl->bat_frag = ccci_dpmaif_bat_create();
	if (!dpmaif_ctrl->bat_frag)
		return LOW_MEMORY_BAT;

#if !(DPMAIF_DL_BAT_SIZE > PAGE_SIZE)
	dpmaif_ctrl->rx_bat_dmapool = dma_pool_create("dpmaif_bat_req_DMA",
		ccci_md_get_dev_by_id(dpmaif_ctrl->md_id),
		(DPMAIF_DL_BAT_ENTRY_SIZE*sizeof(struct dpmaif_bat_t)), 64, 0);

	if (!dpmaif_ctrl->rx_bat_dmapool) {
		CCCI_ERROR_LOG(-1, TAG, "dma poll create fail.\n");
		return LOW_MEMORY_BAT;
	}
#ifdef DPMAIF_DEBUG_LOG
	CCCI_HISTORY_LOG(0, TAG, "bat dma pool\n");
#endif
#endif

	ret = dpmaif_bat_init(dpmaif_ctrl->bat_req, 0);
	if (ret)
		return ret;

	ret = dpmaif_bat_init(dpmaif_ctrl->bat_frag, 1);
	if (ret)
		return ret;

	ret = ccci_dpmaif_create_bat_thread();
	if (ret)
		return ret;

	return 0;
}
