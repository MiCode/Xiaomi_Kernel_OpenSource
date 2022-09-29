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

#include "ccci_dpmaif_bat.h"
#include "ccci_dpmaif_resv_mem.h"

#define BAT_ALLOC_NO_PAUSED  0
#define BAT_ALLOC_IS_PAUSED  1
#define BAT_ALLOC_PAUSE_SUCC 2


#define TAG "bat"


#ifdef NET_SKBUFF_DATA_USES_OFFSET
#define skb_data_size(x) ((x)->head + (x)->end - (x)->data)
#else
#define skb_data_size(x) ((x)->end - (x)->data)
#endif


struct temp_skb_info {
	struct sk_buff *skb;
	unsigned long long base_addr;
};

struct temp_page_info {
	struct page *page;
	unsigned long long base_addr;
	unsigned long offset;
};


#define MAX_SKB_TBL_CNT 10000
#define MAX_FRG_TBL_CNT 10000

static unsigned int g_skb_tbl_cnt;
static struct temp_skb_info *g_skb_tbl;
static atomic_t              g_skb_tbl_rdx;
static atomic_t              g_skb_tbl_wdx;


static unsigned int g_frg_tbl_cnt;
static struct temp_page_info *g_page_tbl;
static atomic_t               g_page_tbl_rdx;
static atomic_t               g_page_tbl_wdx;


unsigned int g_alloc_skb_threshold     = MIN_ALLOC_SKB_CNT;
unsigned int g_alloc_frg_threshold     = MIN_ALLOC_FRG_CNT;
unsigned int g_alloc_skb_tbl_threshold = MIN_ALLOC_SKB_TBL_CNT;
unsigned int g_alloc_frg_tbl_threshold = MIN_ALLOC_FRG_TBL_CNT;
unsigned int g_max_bat_skb_cnt_for_md  = MAX_ALLOC_BAT_CNT;

static unsigned int g_use_page_tbl;

static unsigned int g_alloc_bat_skb_flag;
static unsigned int g_alloc_bat_frg_flag;

static atomic_t g_bat_alloc_thread_wakeup_cnt;

static inline void ccci_dpmaif_skb_wakeup_thread(void)
{
	if (dpmaif_ctl->skb_alloc_thread &&
			dpmaif_ctl->skb_start_alloc == 0) {
		dpmaif_ctl->skb_start_alloc = 1;
		wake_up(&dpmaif_ctl->skb_alloc_wq);
	}
}

static inline int skb_alloc(
		struct sk_buff **ppskb,
		unsigned long long *p_base_addr,
		unsigned int pkt_buf_sz)
{
	(*ppskb) = __dev_alloc_skb(pkt_buf_sz, GFP_KERNEL);

	if (unlikely(!(*ppskb))) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: alloc skb fail. (%u)\n",
			__func__, pkt_buf_sz);

		return LOW_MEMORY_SKB;
	}

	(*p_base_addr) = dma_map_single(
			dpmaif_ctl->dev, (*ppskb)->data,
			skb_data_size((*ppskb)), DMA_FROM_DEVICE);

	if (dma_mapping_error(dpmaif_ctl->dev, (*p_base_addr))) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: dma mapping fail: %ld!\n",
			__func__, skb_data_size(*ppskb));

		dev_kfree_skb_any(*ppskb);
		(*ppskb) = NULL;

		return DMA_MAPPING_ERR;
	}

	return 0;
}

static inline void alloc_skb_to_tbl(int skb_cnt)
{
	int alloc_cnt, i;
	unsigned int used_cnt, skb_tbl_wdx = atomic_read(&g_skb_tbl_wdx);
	struct temp_skb_info *skb_info;
	unsigned int pkt_buf_sz = dpmaif_ctl->bat_skb->pkt_buf_sz;

	if (!g_skb_tbl)
		return;

	if (skb_cnt >= g_skb_tbl_cnt)
		skb_cnt = g_skb_tbl_cnt - 1;

	used_cnt = get_ringbuf_used_cnt(g_skb_tbl_cnt,
				atomic_read(&g_skb_tbl_rdx), skb_tbl_wdx);

	if (skb_cnt <= used_cnt)
		return;

	alloc_cnt = skb_cnt - used_cnt;

	for (i = 0; i < alloc_cnt; i++) {
		skb_info = &g_skb_tbl[skb_tbl_wdx];

		if (skb_alloc(&skb_info->skb, &skb_info->base_addr, pkt_buf_sz))
			break;
		/*
		 * The wmb() flushes writes to dram before read g_skb_tbl data.
		 */
		wmb();

		skb_tbl_wdx = get_ringbuf_next_idx(g_skb_tbl_cnt, skb_tbl_wdx, 1);
		atomic_set(&g_skb_tbl_wdx, skb_tbl_wdx);
	}
}

static inline int get_skb_from_tbl(struct temp_skb_info *skb_info)
{
	unsigned int skb_tbl_rdx = atomic_read(&g_skb_tbl_rdx);

	if ((!g_skb_tbl) ||
		(!get_ringbuf_used_cnt(g_skb_tbl_cnt, skb_tbl_rdx, atomic_read(&g_skb_tbl_wdx))))
		return -1;

	(*skb_info) = g_skb_tbl[skb_tbl_rdx];

	skb_tbl_rdx = get_ringbuf_next_idx(g_skb_tbl_cnt, skb_tbl_rdx, 1);
	atomic_set(&g_skb_tbl_rdx, skb_tbl_rdx);

	return 0;
}

static inline int page_alloc(
		struct page **pp_page,
		unsigned long long *p_base_addr,
		unsigned long *offset,
		unsigned int pkt_buf_sz)
{
	int size = L1_CACHE_ALIGN(pkt_buf_sz);
	void *data;

	data = netdev_alloc_frag(size);/* napi_alloc_frag(size) */
	if (unlikely(!data)) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: alloc frag fail. (%u)\n",
			__func__, size);

		return LOW_MEMORY_BAT; /*-ENOMEM;*/
	}

	(*pp_page) = virt_to_head_page(data);
	*offset = data - page_address((*pp_page));

	/* Get physical address of the RB */
	(*p_base_addr) = dma_map_page(
			dpmaif_ctl->dev, (*pp_page), *offset,
			pkt_buf_sz, DMA_FROM_DEVICE);

	if (dma_mapping_error(dpmaif_ctl->dev, (*p_base_addr))) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: dma mapping: %d\n",
			__func__, pkt_buf_sz);

		put_page((*pp_page));
		(*pp_page) = NULL;

		return DMA_MAPPING_ERR;
	}

	return 0;
}

static inline int get_page_from_tbl(struct temp_page_info *page_info)
{
	unsigned int page_tbl_rdx = atomic_read(&g_page_tbl_rdx);

	if ((!g_page_tbl) ||
		(!get_ringbuf_used_cnt(g_frg_tbl_cnt, page_tbl_rdx, atomic_read(&g_page_tbl_wdx))))
		return -1;

	(*page_info) = g_page_tbl[page_tbl_rdx];

	page_tbl_rdx = get_ringbuf_next_idx(g_frg_tbl_cnt, page_tbl_rdx, 1);

	atomic_set(&g_page_tbl_rdx, page_tbl_rdx);
	return 0;
}

static inline void alloc_page_to_tbl(int page_cnt)
{
	int alloc_cnt, i;
	unsigned int used_cnt, page_tbl_wdx = atomic_read(&g_page_tbl_wdx);
	struct temp_page_info *page_info;
	unsigned int pkt_buf_sz = dpmaif_ctl->bat_frg->pkt_buf_sz;

	if (!g_page_tbl)
		return;

	if (page_cnt >= g_frg_tbl_cnt)
		page_cnt = g_frg_tbl_cnt - 1;

	used_cnt = get_ringbuf_used_cnt(g_frg_tbl_cnt, atomic_read(&g_page_tbl_rdx), page_tbl_wdx);

	if (page_cnt <= used_cnt)
		return;

	alloc_cnt = page_cnt - used_cnt;

	for (i = 0; i < alloc_cnt; i++) {
		page_info = &g_page_tbl[page_tbl_wdx];

		if (page_alloc(&page_info->page, &page_info->base_addr,
				&page_info->offset, pkt_buf_sz))
			break;
		/*
		 * The wmb() flushes writes to dram before read g_skb_tbl data.
		 */
		wmb();

		page_tbl_wdx = get_ringbuf_next_idx(g_frg_tbl_cnt, page_tbl_wdx, 1);
		atomic_set(&g_page_tbl_wdx, page_tbl_wdx);
	}
}

static struct dpmaif_bat_request *ccci_dpmaif_bat_create(void)
{
	struct dpmaif_bat_request *bat_req = NULL;

	bat_req = kzalloc(sizeof(struct dpmaif_bat_request), GFP_KERNEL|__GFP_RETRY_MAYFAIL);

	if (!bat_req)
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: alloc bat fail.\n", __func__);
	else
		memset(bat_req, 0, sizeof(struct dpmaif_bat_request));

	return bat_req;
}

static int dpmaif_bat_init(struct dpmaif_bat_request *bat_req, int is_frag)
{
	int sw_buf_size;
	int cache_mem_from_dts = 0, nocache_mem_from_dts = 0;

	bat_req->bat_cnt = dpmaif_ctl->dl_bat_entry_size;
	bat_req->bat_pkt_cnt = bat_req->bat_cnt;

	if (is_frag) {
		bat_req->pkt_buf_sz = DPMAIF_BUF_FRAG_SIZE;
		sw_buf_size = sizeof(struct dpmaif_bat_page);

	} else {
		bat_req->pkt_buf_sz = DPMAIF_BUF_PKT_SIZE;
		sw_buf_size = sizeof(struct dpmaif_bat_skb);
	}

	/* alloc buffer for HW && AP SW */
	if (!ccci_dpmaif_get_resv_nocache_mem(&bat_req->bat_base, &bat_req->bat_phy_addr,
			(bat_req->bat_cnt * sizeof(struct dpmaif_bat_base))))
		nocache_mem_from_dts = 1;

	if (!nocache_mem_from_dts)
		bat_req->bat_base = dma_alloc_coherent(
			dpmaif_ctl->dev,
			bat_req->bat_cnt * sizeof(struct dpmaif_bat_base),
			&bat_req->bat_phy_addr, GFP_KERNEL);

	/* alloc buffer for AP SW to record skb information */
	if (!ccci_dpmaif_get_resv_cache_mem(&bat_req->bat_pkt_addr, NULL,
			(bat_req->bat_pkt_cnt * sw_buf_size)))
		cache_mem_from_dts = 1;

	if (!cache_mem_from_dts)
		bat_req->bat_pkt_addr = kzalloc((bat_req->bat_pkt_cnt * sw_buf_size), GFP_KERNEL);

	if (bat_req->bat_base == NULL || bat_req->bat_pkt_addr == NULL) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: alloc bat_base(%p) / bat_pkt_addr(%p) fail.\n",
			__func__, bat_req->bat_base, bat_req->bat_pkt_addr);
		return LOW_MEMORY_BAT;
	}

	if (!nocache_mem_from_dts)
		memset(bat_req->bat_base, 0, (bat_req->bat_cnt * sizeof(struct dpmaif_bat_base)));

	return 0;
}

static inline int alloc_bat_skb(
		unsigned int pkt_buf_sz,
		struct dpmaif_bat_skb *bat_skb,
		struct dpmaif_bat_base *cur_bat)
{
	int ret = 0;
	struct temp_skb_info skb_info;

	if (!get_skb_from_tbl(&skb_info)) {
		bat_skb->skb = skb_info.skb;
		bat_skb->data_phy_addr = skb_info.base_addr;

		if ((g_debug_flags & DEBUG_SKB_ALC_FLG) && (g_alloc_bat_skb_flag == 1)) {
			struct debug_skb_alc_flg_hdr hdr;

			hdr.type = TYPE_SKB_ALC_FLG_ID;
			hdr.flag = 0;
			hdr.time = (unsigned int)(local_clock() >> 16);
			ccci_dpmaif_debug_add(&hdr, sizeof(hdr));

			g_alloc_bat_skb_flag = 0;
		}

	} else {
		if ((g_debug_flags & DEBUG_SKB_ALC_FLG) && (g_alloc_bat_skb_flag == 0)) {
			struct debug_skb_alc_flg_hdr hdr;

			hdr.type = TYPE_SKB_ALC_FLG_ID;
			hdr.flag = 1;
			hdr.time = (unsigned int)(local_clock() >> 16);
			ccci_dpmaif_debug_add(&hdr, sizeof(hdr));

			g_alloc_bat_skb_flag = 1;
		}

		ret = skb_alloc(&bat_skb->skb, &(bat_skb->data_phy_addr), pkt_buf_sz);
		if (ret)
			return ret;
	}

	bat_skb->data_len = skb_data_size(bat_skb->skb);

	cur_bat->buffer_addr_ext = (bat_skb->data_phy_addr >> 32) & 0xFF;
	cur_bat->p_buffer_addr = (unsigned int)(bat_skb->data_phy_addr & 0xFFFFFFFF);

	return 0;
}

static int dpmaif_alloc_bat_req(int update_bat_cnt, atomic_t *paused)
{
	struct dpmaif_bat_request *bat_req = dpmaif_ctl->bat_skb;
	struct dpmaif_bat_skb *bat_skb, *next_skb;
	struct dpmaif_bat_base *cur_bat;
	unsigned int buf_space, buf_used, alloc_skb_threshold = g_alloc_skb_threshold;
	int count = 0, ret = 0, request_cnt;
	unsigned short bat_wr_idx, next_wr_idx, pre_hw_wr_idx = 0;

	if (g_dpmf_ver >= 3) {
		atomic_set(&bat_req->bat_rd_idx, ccci_drv3_dl_get_bat_ridx());
		if (g_debug_flags & DEBUG_BAT_ALC_SKB)
			pre_hw_wr_idx = ccci_drv3_dl_get_bat_widx();
	} else  //version 1, 2
		atomic_set(&bat_req->bat_rd_idx, ccci_drv2_dl_get_bat_ridx());

	//if (alloc_skb_threshold > g_max_bat_skb_cnt_for_md)
	//	alloc_skb_threshold = g_max_bat_skb_cnt_for_md;

	buf_used = get_ringbuf_used_cnt(bat_req->bat_cnt,
					atomic_read(&bat_req->bat_rd_idx),
					atomic_read(&bat_req->bat_wr_idx));
	if (buf_used >= alloc_skb_threshold)
		return 0;

	request_cnt = (alloc_skb_threshold - buf_used);
	buf_space = bat_req->bat_cnt - buf_used - 1;

	if (request_cnt > buf_space)
		request_cnt = buf_space;

	if (request_cnt == 0)
		return 0;

	bat_wr_idx = atomic_read(&bat_req->bat_wr_idx);

	while (((!paused) || (!atomic_read(paused))) && (count < request_cnt)) {
		bat_skb = (struct dpmaif_bat_skb *)bat_req->bat_pkt_addr
					+ bat_wr_idx;
		if (bat_skb->skb)
			break;

		next_wr_idx = get_ringbuf_next_idx(
						bat_req->bat_cnt, bat_wr_idx, 1);

		next_skb = (struct dpmaif_bat_skb *)bat_req->bat_pkt_addr
					+ next_wr_idx;
		if (next_skb->skb)
			break;

		cur_bat = (struct dpmaif_bat_base *)bat_req->bat_base
					+ bat_wr_idx;

		ret = alloc_bat_skb(bat_req->pkt_buf_sz, bat_skb, cur_bat);
		if (ret)
			goto alloc_end;

		bat_wr_idx = next_wr_idx;
		count++;

		if (update_bat_cnt && (count & 0x7F) == 0)
			ccci_dpmaif_skb_wakeup_thread();
	}

alloc_end:
	if (count > 0) {
		/* wait write done */
		wmb();

		atomic_set(&bat_req->bat_wr_idx, bat_wr_idx);
		if (update_bat_cnt) {
			if (ccci_drv_dl_add_bat_cnt(count))
				ops.drv_dump_register(CCCI_DUMP_REGISTER);
			ccci_dpmaif_skb_wakeup_thread();
		}

		if (g_debug_flags & DEBUG_BAT_ALC_SKB) {
			struct debug_bat_alc_skb_hdr hdr = {0};

			hdr.type = TYPE_BAT_ALC_SKB_ID;
			hdr.time = (unsigned int)(local_clock() >> 16);
			hdr.spc  = buf_used;
			hdr.cnt  = count;
			hdr.crd  = atomic_read(&bat_req->bat_rd_idx);
			hdr.cwr  = bat_wr_idx;
			hdr.hwr  = pre_hw_wr_idx;
			hdr.thrd = alloc_skb_threshold;
			ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
		}
	}

	return ((ret < 0) ? ret : count);
}

static inline int alloc_bat_page(
		unsigned int pkt_buf_sz,
		struct dpmaif_bat_page *bat_page,
		struct dpmaif_bat_base *cur_bat)
{
	int ret;
	struct temp_page_info page_info;

	if (!get_page_from_tbl(&page_info)) {
		bat_page->page = page_info.page;
		bat_page->data_phy_addr = page_info.base_addr;
		bat_page->offset = page_info.offset;

		if ((g_debug_flags & DEBUG_FRG_ALC_FLG) && (g_alloc_bat_frg_flag == 1)) {
			struct debug_skb_alc_flg_hdr hdr;

			hdr.type = TYPE_FRG_ALC_FLG_ID;
			hdr.flag = 0;
			hdr.time = (unsigned int)(local_clock() >> 16);
			ccci_dpmaif_debug_add(&hdr, sizeof(hdr));

			g_alloc_bat_frg_flag = 0;
		}

	} else {
		if ((g_debug_flags & DEBUG_FRG_ALC_FLG) && (g_alloc_bat_frg_flag == 0)) {
			struct debug_skb_alc_flg_hdr hdr;

			hdr.type = TYPE_FRG_ALC_FLG_ID;
			hdr.flag = 1;
			hdr.time = (unsigned int)(local_clock() >> 16);
			ccci_dpmaif_debug_add(&hdr, sizeof(hdr));

			g_alloc_bat_frg_flag = 1;
		}

		ret = page_alloc(&bat_page->page, &bat_page->data_phy_addr,
			&bat_page->offset, pkt_buf_sz);
		if (ret)
			return ret;
	}

	bat_page->data_len = pkt_buf_sz;

	cur_bat->buffer_addr_ext = (bat_page->data_phy_addr >> 32) & 0xFF;
	cur_bat->p_buffer_addr = (unsigned int)(bat_page->data_phy_addr & 0xFFFFFFFF);

	return 0;
}

static int dpmaif_alloc_bat_frg(int update_bat_cnt, atomic_t *paused)
{
	struct dpmaif_bat_request *bat_req = dpmaif_ctl->bat_frg;
	struct dpmaif_bat_page *bat_page, *next_page;
	struct dpmaif_bat_base *cur_bat;
	unsigned int buf_space, buf_used, alloc_frg_threshold = g_alloc_frg_threshold;
	int count = 0, ret = 0, request_cnt;
	unsigned short bat_wr_idx, next_wr_idx;

	if (g_dpmf_ver >= 3)
		atomic_set(&bat_req->bat_rd_idx, ccci_drv3_dl_get_frg_bat_ridx());
	else  //version 1, 2
		atomic_set(&bat_req->bat_rd_idx, ccci_drv2_dl_get_frg_bat_ridx());

	if (alloc_frg_threshold > g_max_bat_skb_cnt_for_md)
		alloc_frg_threshold = g_max_bat_skb_cnt_for_md;

	buf_used = get_ringbuf_used_cnt(bat_req->bat_cnt,
				atomic_read(&bat_req->bat_rd_idx),
				atomic_read(&bat_req->bat_wr_idx));
	if (buf_used >= alloc_frg_threshold)
		return 0;

	request_cnt = (alloc_frg_threshold - buf_used);
	buf_space = bat_req->bat_cnt - buf_used - 1;

	if (request_cnt > buf_space)
		request_cnt = buf_space;

	if (request_cnt == 0)
		return 0;

	bat_wr_idx = atomic_read(&bat_req->bat_wr_idx);

	while (((!paused) || (!atomic_read(paused)))
			&& (count < request_cnt)) {
		bat_page = (struct dpmaif_bat_page *)bat_req->bat_pkt_addr
					+ bat_wr_idx;
		if (bat_page->page)
			break;

		next_wr_idx = get_ringbuf_next_idx(
				bat_req->bat_cnt, bat_wr_idx, 1);

		next_page = (struct dpmaif_bat_page *)bat_req->bat_pkt_addr
					+ next_wr_idx;
		if (next_page->page)
			break;

		cur_bat = (struct dpmaif_bat_base *)bat_req->bat_base
					+ bat_wr_idx;

		ret = alloc_bat_page(bat_req->pkt_buf_sz, bat_page, cur_bat);
		if (ret)
			goto alloc_end;

		bat_wr_idx = next_wr_idx;
		count++;

		if (update_bat_cnt && (count & 0x7F) == 0) {
			g_use_page_tbl = 1;
			ccci_dpmaif_skb_wakeup_thread();
		}
	}

alloc_end:
	if (count > 0) {
		/* wait write done */
		wmb();

		atomic_set(&bat_req->bat_wr_idx, bat_wr_idx);

		if (update_bat_cnt) {
			ccci_drv_dl_add_frg_bat_cnt(count);
			g_use_page_tbl = 1;
			ccci_dpmaif_skb_wakeup_thread();
		}

		if (g_debug_flags & DEBUG_BAT_ALC_FRG) {
			struct debug_bat_alc_skb_hdr hdr = {0};

			hdr.type = TYPE_BAT_ALC_FRG_ID;
			hdr.time = (unsigned int)(local_clock() >> 16);
			hdr.spc  = buf_used;
			hdr.cnt  = count;
			hdr.crd  = atomic_read(&bat_req->bat_rd_idx);
			hdr.cwr  = bat_wr_idx;
			hdr.thrd = alloc_frg_threshold;
			ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
		}
	}

	return ((ret < 0) ? ret : count);
}

static void ccci_dpmaif_bat_free_req(void)
{
	int j;
	struct dpmaif_bat_skb *bat_skb;
	struct dpmaif_bat_request *bat_req = dpmaif_ctl->bat_skb;

	if ((!bat_req) || (!bat_req->bat_base) || (!bat_req->bat_pkt_addr))
		return;

	for (j = 0; j < bat_req->bat_cnt; j++) {
		bat_skb = ((struct dpmaif_bat_skb *)(bat_req->bat_pkt_addr) + j);

		if (bat_skb->skb) {
			/* rx unmapping */
			dma_unmap_single(
				dpmaif_ctl->dev, bat_skb->data_phy_addr,
				bat_skb->data_len, DMA_FROM_DEVICE);

			dev_kfree_skb_any(bat_skb->skb);
			bat_skb->skb = NULL;
		}
	}

	memset(bat_req->bat_base, 0, (bat_req->bat_cnt * sizeof(struct dpmaif_bat_base)));

	atomic_set(&bat_req->bat_rd_idx, 0);
	atomic_set(&bat_req->bat_wr_idx, 0);
}

static void ccci_dpmaif_bat_free_frg(void)
{
	int j;
	struct dpmaif_bat_page *bat_page;
	struct dpmaif_bat_request *bat_frg = dpmaif_ctl->bat_frg;

	if ((!bat_frg) || (!bat_frg->bat_base) || (!bat_frg->bat_pkt_addr))
		return;

	for (j = 0; j < bat_frg->bat_cnt; j++) {
		bat_page = ((struct dpmaif_bat_page *)(bat_frg->bat_pkt_addr) + j);

		if (bat_page->page) {
			/* rx unmapping */
			dma_unmap_page(
				dpmaif_ctl->dev, bat_page->data_phy_addr,
				bat_page->data_len, DMA_FROM_DEVICE);

			put_page(bat_page->page);
			bat_page->page = NULL;
		}
	}

	memset(bat_frg->bat_base, 0, (bat_frg->bat_cnt * sizeof(struct dpmaif_bat_base)));

	atomic_set(&bat_frg->bat_rd_idx, 0);
	atomic_set(&bat_frg->bat_wr_idx, 0);
}

static void ccci_dpmaif_bat_free(void)
{
	ccci_dpmaif_bat_free_req();

	ccci_dpmaif_bat_free_frg();
}

static int dpmaif_rx_bat_alloc_thread(void *arg)
{
	int ret, ret_req, ret_frg;
	u8 need1 = 0, est_cnt = 0;
	struct debug_bat_th_wake_hdr hdr = {0};

	dpmaif_ctl->bat_alloc_running = 1;

	CCCI_NORMAL_LOG(-1, TAG, "[%s] run start.\n", __func__);

	while (1) {
		ret = wait_event_interruptible(dpmaif_ctl->bat_alloc_wq,
				atomic_read(&dpmaif_ctl->bat_need_alloc));

		if (g_debug_flags & DEBUG_BAT_TH_WAKE) {
			need1 = (u8)atomic_read(&dpmaif_ctl->bat_need_alloc);
		}

		if (atomic_read(&dpmaif_ctl->bat_paused_alloc) != BAT_ALLOC_NO_PAUSED) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] bat_paused_alloc: %d; bat_need_alloc: %d\n", __func__,
				atomic_read(&dpmaif_ctl->bat_paused_alloc),
				atomic_read(&dpmaif_ctl->bat_need_alloc));

			if (atomic_read(&dpmaif_ctl->bat_paused_alloc) == BAT_ALLOC_IS_PAUSED) {
				atomic_set(&dpmaif_ctl->bat_paused_alloc, BAT_ALLOC_PAUSE_SUCC);
				atomic_set(&dpmaif_ctl->bat_need_alloc, 0);
			}
			continue;
		}

		if (ret == -ERESTARTSYS) {
			est_cnt++;
			continue;
		}

		if (kthread_should_stop()) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: kthread_should_stop.\n",
				__func__);
			break;
		}

		ret_req = dpmaif_alloc_bat_req(1, &dpmaif_ctl->bat_paused_alloc);

		ret_frg = dpmaif_alloc_bat_frg(1, &dpmaif_ctl->bat_paused_alloc);

		if (g_debug_flags & DEBUG_BAT_TH_WAKE) {
			hdr.type  = TYPE_BAT_TH_WAKE_ID;
			hdr.time  = (unsigned int)(local_clock() >> 16);
			hdr.need1 = need1;
			hdr.need2 = atomic_read(&dpmaif_ctl->bat_need_alloc);
			hdr.req   = ((ret_req < 0) ? 0 : ret_req);
			hdr.frg   = ((ret_frg < 0) ? 0 : ret_frg);
			hdr.est   = est_cnt;
			ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
			est_cnt = 0;
		}

		if (atomic_read(&dpmaif_ctl->bat_need_alloc) > 1)
			atomic_set(&dpmaif_ctl->bat_need_alloc, 1);
		else
			atomic_set(&dpmaif_ctl->bat_need_alloc, 0);
	}

	dpmaif_ctl->bat_alloc_running = 0;

	CCCI_NORMAL_LOG(-1, TAG, "[%s] run end.\n", __func__);

	return 0;
}

static int ccci_dpmaif_create_bat_thread(void)
{
	init_waitqueue_head(&dpmaif_ctl->bat_alloc_wq);

	dpmaif_ctl->bat_alloc_running = 0;
	atomic_set(&dpmaif_ctl->bat_paused_alloc, BAT_ALLOC_IS_PAUSED);
	atomic_set(&dpmaif_ctl->bat_need_alloc, 0);

	dpmaif_ctl->bat_alloc_thread = kthread_run(
				dpmaif_rx_bat_alloc_thread,
				NULL, "bat_alloc_thread");

	if (IS_ERR(dpmaif_ctl->bat_alloc_thread)) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] kthread_run fail %ld\n",
			__func__, (long)dpmaif_ctl->bat_alloc_thread);

		dpmaif_ctl->bat_alloc_thread = NULL;

		return -1;
	}

	return 0;
}

static int dpmaif_rx_skb_alloc_thread(void *arg)
{
	int ret;

	CCCI_NORMAL_LOG(-1, TAG, "[%s] run start.\n", __func__);

	while (1) {
		ret = wait_event_interruptible(dpmaif_ctl->skb_alloc_wq,
				dpmaif_ctl->skb_start_alloc);

		if (ret == -ERESTARTSYS)
			continue;

		if (kthread_should_stop()) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: kthread_should_stop.\n",
				__func__);
			break;
		}

		alloc_skb_to_tbl(g_alloc_skb_tbl_threshold);

		if (g_use_page_tbl)
			alloc_page_to_tbl(g_alloc_frg_tbl_threshold);

		dpmaif_ctl->skb_start_alloc = 0;
	}

	CCCI_NORMAL_LOG(-1, TAG, "[%s] run end.\n", __func__);

	return 0;
}

static int ccci_dpmaif_create_skb_thread(void)
{
	atomic_set(&g_skb_tbl_rdx, 0);
	atomic_set(&g_skb_tbl_wdx, 0);
	atomic_set(&g_page_tbl_rdx, 0);
	atomic_set(&g_page_tbl_wdx, 0);

	if (g_skb_tbl_cnt == 0 && g_frg_tbl_cnt == 0) {
		dpmaif_ctl->skb_alloc_thread = NULL;
		return 0;
	}

	init_waitqueue_head(&dpmaif_ctl->skb_alloc_wq);
	dpmaif_ctl->skb_start_alloc = 0;

	dpmaif_ctl->skb_alloc_thread = kthread_run(
				dpmaif_rx_skb_alloc_thread,
				NULL, "skb_alloc_thread");

	if (IS_ERR(dpmaif_ctl->skb_alloc_thread)) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] kthread_run fail %ld\n",
			__func__, (long)dpmaif_ctl->skb_alloc_thread);

		dpmaif_ctl->skb_alloc_thread = NULL;

		return -1;
	}

	return 0;
}

inline void ccci_dpmaif_bat_wakeup_thread(int wakeup_cnt)
{
	if (!dpmaif_ctl->bat_alloc_thread)
		return;

	if (wakeup_cnt) {
		if (atomic_add_return(wakeup_cnt, &g_bat_alloc_thread_wakeup_cnt) < 0x80)
			return;
		atomic_sub(0x80, &g_bat_alloc_thread_wakeup_cnt);

	} else
		atomic_set(&g_bat_alloc_thread_wakeup_cnt, 0);

	atomic_inc(&dpmaif_ctl->bat_need_alloc);
	wake_up_all(&dpmaif_ctl->bat_alloc_wq);
}

static void dpmaif_bat_start_thread(void)
{
	atomic_set(&dpmaif_ctl->bat_paused_alloc, BAT_ALLOC_NO_PAUSED);

	ccci_dpmaif_bat_wakeup_thread(0);
}

static void ccci_dpmaif_bat_paused_thread(void)
{
	unsigned int retry_cnt = 0;

	if ((!dpmaif_ctl->bat_alloc_thread) ||
		(!dpmaif_ctl->bat_alloc_running)) {
		CCCI_NORMAL_LOG(-1, TAG,
			"[%s] thread no running: %d\n",
			__func__, dpmaif_ctl->bat_alloc_running);
		return;
	}

	atomic_set(&dpmaif_ctl->bat_paused_alloc, BAT_ALLOC_IS_PAUSED);

	do {
		ccci_dpmaif_bat_wakeup_thread(0);
		mdelay(1);

		retry_cnt++;
		if ((retry_cnt % 1000) == 0)
			/* print error log every 1s */
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: pause bat thread fail\n",
				__func__);

	} while (atomic_read(&dpmaif_ctl->bat_paused_alloc)
			== BAT_ALLOC_IS_PAUSED);

	atomic_set(&dpmaif_ctl->bat_need_alloc, 0);
	CCCI_MEM_LOG_TAG(0, TAG, "[%s] succ.\n", __func__);
}

void ccci_dpmaif_bat_stop(void)
{
	CCCI_NORMAL_LOG(0, TAG, "[%s] stop.\n", __func__);

	ccci_dpmaif_bat_paused_thread();

	ccci_dpmaif_bat_free();

	g_use_page_tbl = 0;
	g_max_bat_skb_cnt_for_md = MAX_ALLOC_BAT_CNT;

	atomic_set(&g_bat_alloc_thread_wakeup_cnt, 0);
}

static void dpmaif_bat_hw_init(void)
{
	//if ((!dpmaif_ctl->bat_skb) || (!dpmaif_ctl->bat_frg)) {
	//	CCCI_ERROR_LOG(0, TAG, "[%s] bat_req or bat_frag is NULL.\n", __func__);
	//	return;
	//}

	if (g_dpmf_ver >= 3) {
		ccci_drv3_dl_set_bat_bufsz(DPMAIF_HW_BAT_PKTBUF);
		ccci_drv3_dl_set_bat_rsv_len(DPMAIF_HW_BAT_RSVLEN);
		ccci_drv3_dl_set_bat_chk_thres();

	} else {  //version 1, 2
		ccci_drv2_dl_set_bat_bufsz(DPMAIF_HW_BAT_PKTBUF);
		ccci_drv2_dl_set_bat_rsv_len(DPMAIF_HW_BAT_RSVLEN);

		if (g_dpmf_ver == 2)
			ccci_drv2_dl_set_bat_chk_thres();
		else  //version 1
			ccci_drv1_dl_set_bat_chk_thres();
	}

	ccci_drv_dl_set_bat_base_addr(dpmaif_ctl->bat_skb->bat_phy_addr);
	ccci_drv_dl_set_bat_size(dpmaif_ctl->bat_skb->bat_cnt);
	ccci_drv_dl_bat_en(false);
	ccci_drv_dl_bat_init_done(false);

	if (g_dpmf_ver >= 3) {
		ccci_drv3_dl_set_ao_frg_bat_feature(true);
		ccci_drv3_dl_set_ao_frg_bat_bufsz(DPMAIF_HW_FRG_PKTBUF);
		ccci_drv3_dl_set_ao_frag_check_thres();

	} else {  //version 1, 2
		ccci_drv2_dl_set_ao_frg_bat_feature(true);
		ccci_drv2_dl_set_ao_frg_bat_bufsz(DPMAIF_HW_FRG_PKTBUF);

		if (g_dpmf_ver == 2)
			ccci_drv2_dl_set_ao_frag_check_thres();
		else  //version 1
			ccci_drv1_dl_set_ao_frag_check_thres();
	}

	ccci_drv_dl_set_bat_base_addr(dpmaif_ctl->bat_frg->bat_phy_addr);
	ccci_drv_dl_set_bat_size(dpmaif_ctl->bat_frg->bat_cnt);
	ccci_drv_dl_bat_en(false);
	ccci_drv_dl_bat_init_done(true);

	if (g_dpmf_ver >= 3)
		ccci_drv3_dl_set_ao_chksum_en(true);
	else  //version 1, 2
		ccci_drv2_dl_set_ao_chksum_en(true);
}

int ccci_dpmaif_bat_start(void)
{
	int ret = 0, skb_cnt, frg_cnt;

	CCCI_NORMAL_LOG(0, TAG, "[%s] start.\n", __func__);

	if ((!dpmaif_ctl->bat_skb) ||
		(!dpmaif_ctl->bat_skb->bat_base) ||
		(!dpmaif_ctl->bat_skb->bat_pkt_addr) ||
		(!dpmaif_ctl->bat_frg) ||
		(!dpmaif_ctl->bat_frg->bat_base) ||
		(!dpmaif_ctl->bat_frg->bat_pkt_addr)) {
		CCCI_ERROR_LOG(0, TAG, "[%s] bat_req or bat_frag is NULL.\n", __func__);
		return -1;
	}

	dpmaif_bat_hw_init();

	skb_cnt = dpmaif_alloc_bat_req(0, NULL);
	if (skb_cnt <= 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] dpmaif_alloc_bat_req fail: %d\n",
			__func__, skb_cnt);
		ret = skb_cnt;
		goto start_err;
	}

	frg_cnt = dpmaif_alloc_bat_frg(0, NULL);
	if (frg_cnt <= 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] dpmaif_alloc_bat_frg fail: %d\n",
			__func__, frg_cnt);
		ret = frg_cnt;
		goto start_err;
	}

	if (ccci_drv_dl_add_frg_bat_cnt(frg_cnt))
		goto start_err;

	if (ccci_drv_dl_add_bat_cnt(skb_cnt))
		goto start_err;

	if (ccci_drv_dl_all_frg_queue_en(true))
		goto start_err;

	if (ccci_drv_dl_all_queue_en(true))
		goto start_err;

	atomic_set(&dpmaif_ctl->bat_paused_alloc, BAT_ALLOC_NO_PAUSED);
	dpmaif_bat_start_thread();

	return 0;

start_err:
	atomic_set(&dpmaif_ctl->bat_paused_alloc, BAT_ALLOC_IS_PAUSED);

	return ret;
}

int ccci_dpmaif_bat_late_init(void)
{
	int ret, len;

	dpmaif_ctl->bat_skb = ccci_dpmaif_bat_create();
	if (!dpmaif_ctl->bat_skb)
		return LOW_MEMORY_BAT;

	dpmaif_ctl->bat_frg = ccci_dpmaif_bat_create();
	if (!dpmaif_ctl->bat_frg)
		return LOW_MEMORY_BAT;

	if (g_skb_tbl_cnt) {
		len = sizeof(struct temp_skb_info) * g_skb_tbl_cnt;
		g_skb_tbl = kzalloc(len, GFP_KERNEL|__GFP_RETRY_MAYFAIL);
		if (!g_skb_tbl)
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: alloc g_skb_tbl fail.\n", __func__);
	}

	if (g_frg_tbl_cnt) {
		len = sizeof(struct temp_page_info) * g_frg_tbl_cnt;
		g_page_tbl = kzalloc(len, GFP_KERNEL|__GFP_RETRY_MAYFAIL);
		if (!g_page_tbl)
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: alloc g_page_tbl fail.\n", __func__);
	}

	ret = dpmaif_bat_init(dpmaif_ctl->bat_skb, 0);
	if (ret)
		return ret;

	ret = dpmaif_bat_init(dpmaif_ctl->bat_frg, 1);
	if (ret)
		return ret;

	ret = ccci_dpmaif_create_bat_thread();
	if (ret)
		return ret;

	ret = ccci_dpmaif_create_skb_thread();
	if (ret)
		return ret;

	return 0;
}

int ccci_dpmaif_bat_init(struct device *dev)
{
	atomic_set(&g_bat_alloc_thread_wakeup_cnt, 0);

	if (of_property_read_u32(dev->of_node, "alloc_skb_tbl_cnt", &g_skb_tbl_cnt))
		g_skb_tbl_cnt = MAX_SKB_TBL_CNT;

	if (of_property_read_u32(dev->of_node, "alloc_frg_tbl_cnt", &g_frg_tbl_cnt))
		g_frg_tbl_cnt = MAX_FRG_TBL_CNT;

	CCCI_NORMAL_LOG(0, TAG,
		"[%s] g_skb_tbl_cnt: %u; g_frg_tbl_cnt: %u\n",
		__func__, g_skb_tbl_cnt, g_frg_tbl_cnt);

	return 0;
}
