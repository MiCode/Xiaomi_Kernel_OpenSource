/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <audio_ipi_dma.h>

#include <linux/io.h>
#include <linux/genalloc.h>

#include <linux/delay.h>
#include <linux/uaccess.h>      /* needed by copy_to_user */

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
#include <adsp_helper.h>
#endif

#ifdef CONFIG_MTK_HIFIXDSP_SUPPORT
#include "audio_memory.h"
#endif

#include <audio_log.h>
#include <audio_assert.h>
#include <audio_ipi_platform.h>

#include <audio_task.h>
#include <audio_controller_msg_id.h>
#include <audio_messenger_ipi.h>

#include <audio_ringbuf.h>




/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define MAX_SIZE_OF_ONE_FRAME (16) /* 32-bits * 4ch */

/* auidio dsp cache limitation: 128 byte alignment */
#define ADSP_CACHE_ALIGN_ORDER (7)
#define ADSP_CACHE_ALIGN_BYTES (1 << (ADSP_CACHE_ALIGN_ORDER))
#define ADSP_CACHE_ALIGN_MASK  ((ADSP_CACHE_ALIGN_BYTES) - 1)

#define DO_BYTE_ALIGN(value, mask) (((value) + mask) & (~mask))


/* hal read */
#define MAX_SCP_MSG_NUM_IN_QUEUE (64)
#define MAX_DSP_DMA_WRITE_SIZE   (0x10000)


/*
 * =============================================================================
 *                     log
 * =============================================================================
 */

#ifdef ipi_dbg
#undef ipi_dbg
#endif

#if 0
#define ipi_dbg(x...) pr_info(x)
#else
#define ipi_dbg(x...)
#endif

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[IPI][DMA] %s(), " fmt "\n", __func__


/*
 * =============================================================================
 *                     struct def
 * =============================================================================
 */

/* DMA */
struct audio_region_t {
	uint32_t offset;        /* ex: 0x1000 */
	uint32_t size;          /* ex: 0x100 */
	uint8_t  resv_128[120];

	uint32_t read_idx;      /* ex: 0x0 ~ 0xFF */
	uint8_t  resv_256[124];

	uint32_t write_idx;     /* ex: 0x0 ~ 0xFF */
	uint8_t  resv_384[124];
};


struct audio_ipi_dma_t {
	struct aud_ptr_t base_phy;
	struct aud_ptr_t base_vir;

	uint32_t size;
	uint8_t  resv_128[108];

	struct audio_region_t region[TASK_SCENE_SIZE][NUM_AUDIO_IPI_DMA_PATH];
	uint32_t checksum;

	/* the beginning of pool data: regions allocated from pool_offset */
	uint32_t pool_offset;
};



/* queue */
struct hal_dma_queue_t {
	struct ipi_msg_t msg[MAX_SCP_MSG_NUM_IN_QUEUE];

	uint32_t size;
	uint32_t idx_r;
	uint32_t idx_w;

	spinlock_t queue_lock;
	wait_queue_head_t queue_wq;

	struct audio_ringbuf_t dma_data;
	uint8_t *tmp_buf_d2k;
	uint8_t *tmp_buf_k2h;
};



/*
 * =============================================================================
 *                     global var
 * =============================================================================
 */

static struct audio_ipi_dma_t *g_dma;
static struct gen_pool *g_dma_pool;

static uint8_t g_region_reg_flag[TASK_SCENE_SIZE];

static struct hal_dma_queue_t g_hal_dma_queue;


/*
 * =============================================================================
 *                     utilities
 * =============================================================================
 */

inline unsigned long offset_to_phy_addr(const uint32_t offset)
{
	if (!g_dma)
		return 0;
	return g_dma->base_phy.addr_val + offset;
}

inline uint8_t *offset_to_vir_addr(const uint32_t offset)
{
	if (!g_dma)
		return 0;
	return g_dma->base_vir.addr + offset;
}

inline uint32_t phy_addr_to_offset(const unsigned long phy_addr)
{
	if (!g_dma)
		return 0;
	return phy_addr - g_dma->base_phy.addr_val;
}

inline uint32_t vir_addr_to_offset(const uint8_t *vir_addr)
{
	if (!g_dma)
		return 0;
	return vir_addr - g_dma->base_vir.addr;
}

inline uint8_t *dma_vir_base(void)
{
	if (!g_dma)
		return NULL;
	return g_dma->base_vir.addr;
}


#define DUMP_REGION(LOG_F, description, p_region, count) \
	do { \
		if (p_region && description) { \
			LOG_F("%s, offset: 0x%x, size: 0x%x" \
			      ", read_idx: 0x%x, write_idx: 0x%x" \
			      ", region_data_count: 0x%x, count: %u", \
			      description, \
			      (p_region)->offset, \
			      (p_region)->size, \
			      (p_region)->read_idx, \
			      (p_region)->write_idx, \
			      audio_region_data_count(p_region), \
			      count); \
		} else { \
			pr_notice("%uL, %p %p", \
				  __LINE__, p_region, description); \
		} \
	} while (0)



/*
 * =============================================================================
 *                     init
 * =============================================================================
 */

static int hal_dma_init_msg_queue(struct hal_dma_queue_t *msg_queue);
static int hal_dma_deinit_msg_queue(struct hal_dma_queue_t *msg_queue);


int init_audio_ipi_dma(void)
{
	int ret = 0;

	uint32_t size = 0;

#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	const uint32_t mem_id = ADSP_A_IPI_MEM_ID;
#elif defined(CONFIG_MTK_HIFIXDSP_SUPPORT)
	const uint32_t mem_id = ADSP_A_IPI_MEM_ID;
#endif

	if (g_dma != NULL) {
		pr_debug("already init");
		goto IPI_DMA_INIT_EXIT;
	}

	/* scp_reserve_mblock, AUDIO_IPI_MEM_ID */
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	size = (uint32_t)adsp_get_reserve_mem_size(mem_id);
#elif defined(CONFIG_MTK_HIFIXDSP_SUPPORT)
	size = (uint32_t)adsp_get_reserve_mem_size(mem_id);
#endif
	if (size < sizeof(struct audio_ipi_dma_t)) {
		pr_notice("size %u < %zu",
			  size, sizeof(struct audio_ipi_dma_t));
		ret = -ENOMEM;
		goto IPI_DMA_INIT_EXIT;
	}


	/* share mem for IPI DMA */
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	g_dma = (struct audio_ipi_dma_t *)adsp_get_reserve_mem_virt(mem_id);
#elif defined(CONFIG_MTK_HIFIXDSP_SUPPORT)
	g_dma = (struct audio_ipi_dma_t *)adsp_get_reserve_mem_virt(mem_id);
#endif
	if (g_dma == NULL) {
		pr_notice("scp_get_reserve_mem_virt(%d) fail!!", mem_id);
		ret = -ENOMEM;
		goto IPI_DMA_INIT_EXIT;
	}
	memset_io(g_dma, 0, size);

	/* TODO: by CM4/HiFi3/... */
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	g_dma->base_phy.addr = (uint8_t *)adsp_get_reserve_mem_phys(mem_id);
	g_dma->base_vir.addr = (uint8_t *)adsp_get_reserve_mem_virt(mem_id);
#elif defined(CONFIG_MTK_HIFIXDSP_SUPPORT)
	g_dma->base_phy.addr = (uint8_t *)adsp_get_reserve_mem_phys(mem_id);
	g_dma->base_vir.addr = (uint8_t *)adsp_get_reserve_mem_virt(mem_id);
#endif
	g_dma->size = size;

	g_dma->checksum = (uint8_t *)(&g_dma->checksum) - (uint8_t *)g_dma;

	/* 128 byte align */
	g_dma->pool_offset = (uint8_t *)&g_dma->pool_offset - (uint8_t *)g_dma;
	g_dma->pool_offset = DO_BYTE_ALIGN(g_dma->pool_offset,
					   ADSP_CACHE_ALIGN_MASK);

	if (size < g_dma->pool_offset) {
		pr_notice("size %u < pool_offset %u", size, g_dma->pool_offset);
		ret = -ENOMEM;
		g_dma = NULL;
		goto IPI_DMA_INIT_EXIT;
	}
	pr_info("dma %p, phy %p/0x%lx, vir %p/0x%lx, sz 0x%x, checksum %u, offset %u",
		g_dma,
		g_dma->base_phy.addr,
		g_dma->base_phy.addr_val,
		g_dma->base_vir.addr,
		g_dma->base_vir.addr_val,
		g_dma->size,
		g_dma->checksum,
		g_dma->pool_offset);


	/* pool */
	g_dma_pool = gen_pool_create(ADSP_CACHE_ALIGN_ORDER, -1);
	if (g_dma_pool == NULL) {
		pr_notice("gen_pool_create fail");
		ret = -ENOMEM;
		goto IPI_DMA_INIT_EXIT;
	}

	/* add DRAM to pool */
	if (gen_pool_add(
		    g_dma_pool,
		    g_dma->base_phy.addr_val + g_dma->pool_offset,
		    (size_t)(g_dma->size - g_dma->pool_offset),
		    -1) != 0) {
		pr_notice("gen_pool_add fail");
		ret = -ENOMEM;
		gen_pool_destroy(g_dma_pool);
		g_dma_pool = NULL;
		goto IPI_DMA_INIT_EXIT;
	}


	hal_dma_init_msg_queue(&g_hal_dma_queue);



IPI_DMA_INIT_EXIT:
	if (ret != 0 || g_dma_pool == NULL)
		g_dma = NULL;
	else
		audio_ipi_dma_init_dsp();

	return ret;
}


int deinit_audio_ipi_dma(void)
{
	int i = 0;

	if (g_dma == NULL)
		return 0;

	for (i = 0 ; i < TASK_SCENE_SIZE; i++)
		audio_ipi_dma_free_region(i);

	if (g_dma_pool != NULL) {
		gen_pool_destroy(g_dma_pool);
		g_dma_pool = NULL;
	}
	g_dma = NULL;

	hal_dma_deinit_msg_queue(&g_hal_dma_queue);

	return 0;
}


int audio_ipi_dma_init_dsp(void)
{
	static bool init_dsp;
#if 0
	struct ipi_msg_t ipi_msg;
#endif

	int ret = 0;

	if (init_dsp == true)
		return 0;

#if 0 // TODO: remove
	ret = audio_send_ipi_msg(
		      &ipi_msg,
		      TASK_SCENE_AUDIO_CONTROLLER,
		      AUDIO_IPI_LAYER_TO_DSP,
		      AUDIO_IPI_MSG_ONLY,
		      AUDIO_IPI_MSG_DIRECT_SEND,
		      AUD_CTL_MSG_A2D_DMA_INIT,
		      g_dma->base_phy.addr_val,
		      g_dma->size,
		      NULL);
#endif

	if (ret == 0)
		init_dsp = true;

	return ret;
}


void *get_audio_ipi_dma_vir_addr(unsigned long phy_addr_val)
{
	uint32_t offset = 0;

	if (g_dma == NULL)
		return NULL;

	if (phy_addr_val == 0)
		return NULL;


	offset = phy_addr_val - g_dma->base_phy.addr_val;
	return g_dma->base_vir.addr + offset;
}



/*
 * =============================================================================
 *                     alloc/free
 * =============================================================================
 */

int audio_ipi_dma_alloc(struct aud_ptr_t *phy_addr, const uint32_t size)
{
	if (g_dma == NULL) {
		pr_info("g_dma: %p", g_dma);
		return -ENODEV;
	}
	if (g_dma_pool == NULL) {
		pr_info("g_dma_pool: %p", g_dma_pool);
		return -ENOMEM;
	}
	if (phy_addr == NULL || size == 0) {
		pr_debug("arg err, %p, %u", phy_addr, size);
		return -EINVAL;
	}

	phy_addr->addr_val = gen_pool_alloc(g_dma_pool, size);
	if (phy_addr->addr_val == 0) {
		pr_notice("gen_pool_alloc(%u) fail, (%zu/%zu)",
			  size,
			  gen_pool_avail(g_dma_pool),
			  gen_pool_size(g_dma_pool));
		return -ENOMEM;
	}

	return 0;
}


int audio_ipi_dma_free(struct aud_ptr_t *phy_addr, const uint32_t size)
{
	if (g_dma == NULL) {
		pr_info("g_dma: %p", g_dma);
		return -ENODEV;
	}
	if (g_dma_pool == NULL) {
		pr_info("g_dma_pool: %p", g_dma_pool);
		return -ENOMEM;
	}
	if (phy_addr == NULL || size == 0) {
		pr_debug("arg err, %p, %u", phy_addr, size);
		return -EINVAL;
	}


	gen_pool_free(g_dma_pool, phy_addr->addr_val, size);

	phy_addr->addr_val = 0;

	return 0;
}



/*
 * =============================================================================
 *                     alloc/free region
 * =============================================================================
 */

int audio_ipi_dma_alloc_region(const uint8_t task,
			       const uint32_t ap_to_dsp_size,
			       const uint32_t dsp_to_ap_size)
{
	struct audio_region_t *region = NULL;
	uint32_t size[2] = {ap_to_dsp_size, dsp_to_ap_size};

	unsigned long phy_value = 0;
#if 0 // TODO: remove
	struct ipi_msg_t ipi_msg;
#endif

	int ret = 0;

	int i = 0;

	if (g_dma == NULL) {
		pr_info("g_dma: %p", g_dma);
		return -ENODEV;
	}
	if (g_dma_pool == NULL) {
		pr_info("g_dma_pool: %p", g_dma_pool);
		return -ENOMEM;
	}
	if (task >= TASK_SCENE_SIZE) {
		pr_info("task: %d", task);
		return -EOVERFLOW;
	}

	if (g_region_reg_flag[task] == true) {
		pr_notice("task: %d already register", task);
		return -EEXIST;
	}
	g_region_reg_flag[task] = true;


	for (i = 0; i < NUM_AUDIO_IPI_DMA_PATH; i++) {
		if (size[i] == 0) {
			pr_debug("task %d, size[%d]: %u", task, i, size[i]);
			continue;
		}

		region = &g_dma->region[task][i];

		phy_value = gen_pool_alloc(g_dma_pool, size[i]);
		if (phy_value == 0) {
			pr_notice("gen_pool_alloc(%u) fail, (%zu/%zu)",
				  size[i],
				  gen_pool_avail(g_dma_pool),
				  gen_pool_size(g_dma_pool));
			ret = -ENOMEM;
			break;
		}

		region->offset = phy_addr_to_offset(phy_value);
		region->size = size[i];
		region->read_idx = 0;
		region->write_idx = 0;

		pr_info("task %d, region[%d] sz 0x%x, offset 0x%x",
			task, i, size[i], region->offset);
	}


#if 0 // TODO: remove
	if (ret == 0) {
		audio_send_ipi_msg(
			&ipi_msg,
			TASK_SCENE_AUDIO_CONTROLLER,
			AUDIO_IPI_LAYER_TO_DSP,
			AUDIO_IPI_MSG_ONLY,
			AUDIO_IPI_MSG_DIRECT_SEND,
			AUD_CTL_MSG_A2D_DMA_UPDATE_REGION,
			task,
			0,
			NULL);
	}
#endif

	return ret;
}


int audio_ipi_dma_free_region(const uint8_t task)
{
	struct audio_region_t *region = NULL;

	unsigned long phy_value = 0;

#if 0 // TODO: remove
	struct ipi_msg_t ipi_msg;
	int ret = 0;
#endif

	int i = 0;


	if (g_dma == NULL) {
		pr_info("g_dma: %p", g_dma);
		return -ENODEV;
	}
	if (g_dma_pool == NULL) {
		pr_info("g_dma_pool: %p", g_dma_pool);
		return -ENOMEM;
	}
	if (task >= TASK_SCENE_SIZE) {
		pr_info("task: %d", task);
		return -EOVERFLOW;
	}

	if (g_region_reg_flag[task] == false) {
		pr_notice("task: %d already unregister", task);
		return -ENODEV;
	}
	g_region_reg_flag[task] = false;


	for (i = 0; i < NUM_AUDIO_IPI_DMA_PATH; i++) {
		region = &g_dma->region[task][i];

		if (region->read_idx != region->write_idx) {
			pr_notice("region[%d][%d]: %u != %u",
				  task, i, region->read_idx, region->write_idx);
		}

		if (region->size == 0) {
			AUD_ASSERT(region->offset == 0);
			continue;
		}

		phy_value = offset_to_phy_addr(region->offset);


		pr_info("task %d, region[%d] sz 0x%x, offset 0x%x",
			task, i, region->size, region->offset);

		gen_pool_free(g_dma_pool,
			      phy_value,
			      region->size);

		region->offset = 0;
		region->size = 0;
		region->read_idx = 0;
		region->write_idx = 0;
	}

#if 0 // TODO: remove
	if (ret == 0) {
		audio_send_ipi_msg(
			&ipi_msg,
			TASK_SCENE_AUDIO_CONTROLLER,
			AUDIO_IPI_LAYER_TO_DSP,
			AUDIO_IPI_MSG_ONLY,
			AUDIO_IPI_MSG_DIRECT_SEND,
			AUD_CTL_MSG_A2D_DMA_UPDATE_REGION,
			task,
			0,
			NULL);
	}
#endif

	return 0;
}



/*
 * =============================================================================
 *                     region
 * =============================================================================
 */

static uint32_t audio_region_data_count(struct audio_region_t *region)
{
	uint32_t count = 0;

	if (!region)
		return 0;

	if (region->size == 0) {
		DUMP_REGION(pr_notice, "size fail", region, count);
		return 0;
	}

	if (region->read_idx >= region->size) {
		DUMP_REGION(pr_notice, "read_idx fail", region, count);
		region->read_idx %= region->size;
	} else if (region->write_idx >= region->size) {
		DUMP_REGION(pr_notice, "write_idx fail", region, count);
		region->write_idx %= region->size;
	}

	if (region->write_idx >= region->read_idx)
		count = region->write_idx - region->read_idx;
	else
		count = region->size - (region->read_idx - region->write_idx);

	return count;
}


static uint32_t audio_region_free_space(struct audio_region_t *region)
{
	uint32_t count = 0;

	if (!region)
		return 0;

	count = region->size - audio_region_data_count(region);

	if (count >= MAX_SIZE_OF_ONE_FRAME)
		count -= MAX_SIZE_OF_ONE_FRAME;
	else
		count = 0;

	return count;
}


static int audio_region_write_from_linear(
	struct audio_region_t *region,
	const void *linear_buf,
	uint32_t count)
{
	uint32_t count_align = DO_BYTE_ALIGN(count, ADSP_CACHE_ALIGN_MASK);

	uint32_t free_space = 0;
	uint8_t *base = NULL;
	uint32_t w2e = 0;

	if (!region || !linear_buf || !dma_vir_base())
		return -EFAULT;

	if (region->size == 0) {
		DUMP_REGION(pr_notice, "size fail", region, count);
		return -ENODEV;
	}

	if (region->read_idx >= region->size) {
		DUMP_REGION(pr_notice, "read_idx fail", region, count);
		region->read_idx %= region->size;
	}
	if (region->write_idx >= region->size) {
		DUMP_REGION(pr_notice, "write_idx fail", region, count);
		region->write_idx %= region->size;
	}


	DUMP_REGION(ipi_dbg, "in", region, count);

	free_space = audio_region_free_space(region);
	if (free_space < count_align) {
		DUMP_REGION(pr_notice, "free_space < count_align",
			    region, count);
		return -EOVERFLOW;
	}

	base = dma_vir_base() + region->offset;

	if (region->read_idx <= region->write_idx) {
		w2e = region->size - region->write_idx;
		if (count_align <= w2e) {
			memcpy(base + region->write_idx, linear_buf, count);
			region->write_idx += count_align;
			if (region->write_idx == region->size)
				region->write_idx = 0;
		} else {
			memcpy(base + region->write_idx, linear_buf, w2e);
			memcpy(base, (uint8_t *)linear_buf + w2e, count - w2e);
			region->write_idx = count_align - w2e;
		}
	} else {
		memcpy(base + region->write_idx, linear_buf, count);
		region->write_idx += count_align;
	}


	DUMP_REGION(ipi_dbg, "out", region, count);

	return 0;
}


static int audio_region_read_to_linear(
	void *linear_buf,
	struct audio_region_t *region,
	uint32_t count)
{
	uint32_t count_align = DO_BYTE_ALIGN(count, ADSP_CACHE_ALIGN_MASK);

	uint32_t available_count = 0;
	uint8_t *base = NULL;
	uint32_t r2e = 0;

	if (!region || !linear_buf || !dma_vir_base())
		return -EFAULT;

	if (region->size == 0) {
		DUMP_REGION(pr_notice, "size fail", region, count);
		return -ENODEV;
	}

	if (region->read_idx >= region->size) {
		DUMP_REGION(pr_notice, "read_idx fail", region, count);
		region->read_idx %= region->size;
	}
	if (region->write_idx >= region->size) {
		DUMP_REGION(pr_notice, "write_idx fail", region, count);
		region->write_idx %= region->size;
	}


	DUMP_REGION(ipi_dbg, "in", region, count);

	available_count = audio_region_data_count(region);
	if (count_align > available_count) {
		DUMP_REGION(pr_notice, "count_align > available_count",
			    region, count);
		return -ENOMEM;
	}

	base = dma_vir_base() + region->offset;

	if (region->read_idx <= region->write_idx) {
		memcpy(linear_buf, base + region->read_idx, count);
		region->read_idx += count_align;
	} else {
		r2e = region->size - region->read_idx;
		if (r2e >= count_align) {
			memcpy(linear_buf, base + region->read_idx, count);
			region->read_idx += count_align;
			if (region->read_idx == region->size)
				region->read_idx = 0;
		} else {
			memcpy(linear_buf, base + region->read_idx, r2e);
			memcpy((uint8_t *)linear_buf + r2e, base, count - r2e);
			region->read_idx = count_align - r2e;
		}
	}

	DUMP_REGION(ipi_dbg, "out", region, count);

	return 0;
}


static int audio_region_drop(
	struct audio_region_t *region,
	uint32_t count)
{
	uint32_t count_align = DO_BYTE_ALIGN(count, ADSP_CACHE_ALIGN_MASK);

	uint32_t available_count = 0;
	uint8_t *base = NULL;
	uint32_t r2e = 0;

	if (!region || !dma_vir_base())
		return -EFAULT;

	if (region->size == 0) {
		DUMP_REGION(pr_notice, "size fail", region, count);
		return -ENODEV;
	}

	if (region->read_idx >= region->size) {
		DUMP_REGION(pr_notice, "read_idx fail", region, count);
		region->read_idx %= region->size;
	}
	if (region->write_idx >= region->size) {
		DUMP_REGION(pr_notice, "write_idx fail", region, count);
		region->write_idx %= region->size;
	}


	DUMP_REGION(ipi_dbg, "in", region, count);

	available_count = audio_region_data_count(region);
	if (count_align > available_count) {
		DUMP_REGION(pr_notice, "count_align > available_count",
			    region, count);
		return -ENOMEM;
	}

	base = dma_vir_base() + region->offset;

	if (region->read_idx <= region->write_idx)
		region->read_idx += count_align;
	else {
		r2e = region->size - region->read_idx;
		if (r2e >= count_align) {
			region->read_idx += count_align;
			if (region->read_idx == region->size)
				region->read_idx = 0;
		} else
			region->read_idx = count_align - r2e;
	}

	DUMP_REGION(ipi_dbg, "out", region, count);

	return 0;
}


int audio_ipi_dma_write_region(const uint8_t task,
			       const void *data_buf,
			       uint32_t data_size,
			       uint32_t *write_idx)
{
	struct audio_region_t *region = NULL;

	int ret = 0;

	if (task >= TASK_SCENE_SIZE) {
		pr_info("task: %d", task);
		return -EOVERFLOW;
	}
	if (!data_buf || !write_idx || !g_dma) {
		pr_info("buf %p, idx %p, dma %p NULL!!",
			data_buf, write_idx, g_dma);
		return -EFAULT;
	}
	if (data_size == 0) {
		pr_info("task: %d, data_size = 0", task);
		return -ENODATA;
	}

	region = &g_dma->region[task][AUDIO_IPI_DMA_AP_TO_SCP];
	DUMP_REGION(ipi_dbg, "region", region, data_size);

	/* keep the data index before write */
	*write_idx = region->write_idx;

	/* write data */
	ret = audio_region_write_from_linear(region, data_buf, data_size);

	return ret;
}


int audio_ipi_dma_read_region(const uint8_t task,
			      void *data_buf,
			      uint32_t data_size,
			      uint32_t read_idx)
{
	struct audio_region_t *region = NULL;

	int ret = 0;

	if (task >= TASK_SCENE_SIZE) {
		pr_info("task: %d", task);
		return -EOVERFLOW;
	}
	if (!data_buf || !g_dma) {
		pr_info("buf %p, dma %p NULL!!", data_buf, g_dma);
		return -EFAULT;
	}
	if (data_size == 0) {
		pr_info("task: %d, data_size = 0", task);
		return -ENODATA;
	}

	region = &g_dma->region[task][AUDIO_IPI_DMA_SCP_TO_AP];
	DUMP_REGION(ipi_dbg, "region", region, data_size);

	/* check read index */
	if (read_idx != region->read_idx) {
		pr_debug("read_idx 0x%x != region->read_idx 0x%x!!",
			 read_idx, region->read_idx);
		region->read_idx = read_idx;
	}

	/* read data */
	ret = audio_region_read_to_linear(data_buf, region, data_size);

	return ret;
}


int audio_ipi_dma_drop_region(const uint8_t task,
			      uint32_t drop_size,
			      uint32_t read_idx)

{
	struct audio_region_t *region = NULL;

	int ret = 0;

	if (task >= TASK_SCENE_SIZE) {
		pr_info("task: %d", task);
		return -EOVERFLOW;
	}
	if (!g_dma) {
		pr_info("dma %p NULL!!", g_dma);
		return -EFAULT;
	}
	if (drop_size == 0) {
		pr_info("task: %d, drop_size = 0", task);
		return -ENODATA;
	}

	region = &g_dma->region[task][AUDIO_IPI_DMA_SCP_TO_AP];
	DUMP_REGION(ipi_dbg, "region", region, drop_size);

	/* check read index */
	if (read_idx != region->read_idx) {
		pr_debug("read_idx 0x%x != region->read_idx 0x%x!!",
			 read_idx, region->read_idx);
		region->read_idx = read_idx;
	}

	/* drop data */
	ret = audio_region_drop(region, drop_size);

	return ret;
}



/*
 * =============================================================================
 *                     DSP -> DMA -> HAL
 * =============================================================================
 */

inline bool hal_dma_check_idx_msg_valid(
	const struct hal_dma_queue_t *msg_queue,
	const uint32_t idx_msg)
{
	if (msg_queue == NULL) {
		pr_info("msg_queue == NULL!! return");
		return false;
	}

	return (idx_msg < msg_queue->size) ? true : false;
}


inline bool hal_dma_check_queue_empty(const struct hal_dma_queue_t *msg_queue)
{
	if (msg_queue == NULL) {
		pr_info("msg_queue == NULL!! return");
		return false;
	}

	return (msg_queue->idx_r == msg_queue->idx_w);
}


inline bool hal_dma_check_queue_to_be_full(
	const struct hal_dma_queue_t *msg_queue)
{
	uint32_t idx_w_to_be = 0;

	if (msg_queue == NULL) {
		pr_info("msg_queue == NULL!! return");
		return false;
	}

	idx_w_to_be = msg_queue->idx_w + 1;
	if (idx_w_to_be == msg_queue->size)
		idx_w_to_be = 0;

	return (idx_w_to_be == msg_queue->idx_r) ? true : false;
}


inline uint32_t hal_dma_get_num_msg_in_queue(
	const struct hal_dma_queue_t *msg_queue)
{
	if (msg_queue == NULL) {
		pr_info("msg_queue == NULL!! return");
		return 0;
	}

	return (msg_queue->idx_w >= msg_queue->idx_r) ?
	       (msg_queue->idx_w - msg_queue->idx_r) :
	       ((msg_queue->size - msg_queue->idx_r) + msg_queue->idx_w);
}


static void hal_dma_dump_msg_in_queue(struct hal_dma_queue_t *msg_queue)
{
	struct ipi_msg_t *p_ipi_msg = NULL;
	uint32_t idx_dump = msg_queue->idx_r;

	pr_info("idx_r: %u, idx_w: %u, queue(%u/%u)",
		msg_queue->idx_r,
		msg_queue->idx_w,
		hal_dma_get_num_msg_in_queue(msg_queue),
		msg_queue->size);

	while (idx_dump != msg_queue->idx_w) {
		/* get head msg */
		p_ipi_msg = &msg_queue->msg[idx_dump];

		DUMP_IPI_MSG("dump queue list", p_ipi_msg);

		/* update dump index */
		idx_dump++;
		if (idx_dump == msg_queue->size)
			idx_dump = 0;
	}
}


static int hal_dma_push(
	struct hal_dma_queue_t *msg_queue,
	struct ipi_msg_t *p_ipi_msg,
	uint32_t *p_idx_msg)
{
	int retval = 0;
#if 0
	uint32_t i = 0;
#endif

	if (msg_queue == NULL || p_ipi_msg == NULL || p_idx_msg == NULL) {
		pr_info("NULL!! msg_queue: %p, p_ipi_msg: %p, p_idx_msg: %p",
			msg_queue, p_ipi_msg, p_idx_msg);
		return -EFAULT;
	}

	/* check queue full */
	if (hal_dma_check_queue_to_be_full(msg_queue) == true) {
		pr_info("task: %d, msg_id: 0x%x, queue overflow, idx_r: %u, idx_w: %u, drop it",
			p_ipi_msg->task_scene, p_ipi_msg->msg_id,
			msg_queue->idx_r, msg_queue->idx_w);
		hal_dma_dump_msg_in_queue(msg_queue);
		WARN_ON(1);
		return -EOVERFLOW;
	}

	if (hal_dma_check_idx_msg_valid(msg_queue, msg_queue->idx_w) == false) {
		pr_info("idx_w %u is invalid!! return", msg_queue->idx_w);
		return -1;
	}


	/* get dma data */
	if (p_ipi_msg->dma_info.data_size > MAX_DSP_DMA_WRITE_SIZE) {
		pr_notice("data_size %u > %u!!",
			  p_ipi_msg->dma_info.data_size,
			  MAX_DSP_DMA_WRITE_SIZE);
		return -EOVERFLOW;
	}

	retval = audio_ipi_dma_read_region(
			 p_ipi_msg->task_scene,
			 msg_queue->tmp_buf_d2k,
			 p_ipi_msg->dma_info.data_size,
			 p_ipi_msg->dma_info.rw_idx);

#if 0
	for (i = 0; i < p_ipi_msg->dma_info.data_size; i++)
		pr_info("%d", msg_queue->tmp_buf_d2k[i]);
#endif

	if (retval != 0)
		return retval;


	/* push */
	*p_idx_msg = msg_queue->idx_w;
	msg_queue->idx_w++;
	if (msg_queue->idx_w == msg_queue->size)
		msg_queue->idx_w = 0;

	/* copy */
	memcpy((void *)&msg_queue->msg[*p_idx_msg],
	       p_ipi_msg,
	       sizeof(struct ipi_msg_t));
	audio_ringbuf_copy_from_linear_impl(
		&msg_queue->dma_data,
		msg_queue->tmp_buf_d2k,
		p_ipi_msg->dma_info.data_size);


	ipi_dbg("task: %d, msg_id: 0x%x, idx_r: %u, idx_w: %u, queue(%u/%u), *p_idx_msg: %u",
		p_ipi_msg->task_scene,
		p_ipi_msg->msg_id,
		msg_queue->idx_r,
		msg_queue->idx_w,
		hal_dma_get_num_msg_in_queue(msg_queue),
		msg_queue->size,
		*p_idx_msg);

	return 0;
}


static int hal_dma_pop(struct hal_dma_queue_t *msg_queue)
{
	struct ipi_msg_t *p_ipi_msg = NULL;

	if (msg_queue == NULL) {
		pr_info("NULL!! msg_queue: %p", msg_queue);
		return -EFAULT;
	}

	/* check queue empty */
	if (hal_dma_check_queue_empty(msg_queue) == true) {
		pr_info("queue is empty, idx_r: %u, idx_w: %u",
			msg_queue->idx_r,
			msg_queue->idx_w);
		return -1;
	}

	/* pop */
	p_ipi_msg = &msg_queue->msg[msg_queue->idx_r];
	msg_queue->idx_r++;
	if (msg_queue->idx_r == msg_queue->size)
		msg_queue->idx_r = 0;


	ipi_dbg("task: %d, msg_id: 0x%x, idx_r: %u, idx_w: %u, queue(%u/%u)",
		p_ipi_msg->task_scene,
		p_ipi_msg->msg_id,
		msg_queue->idx_r,
		msg_queue->idx_w,
		hal_dma_get_num_msg_in_queue(msg_queue),
		msg_queue->size);

	return 0;
}


static int hal_dma_front(
	struct hal_dma_queue_t *msg_queue,
	struct ipi_msg_t **pp_ipi_msg,
	uint32_t *p_idx_msg)
{
	uint32_t data_size = 0;

	if (msg_queue == NULL || pp_ipi_msg == NULL || p_idx_msg == NULL) {
		pr_info("NULL!! msg_queue: %p, pp_ipi_msg: %p, p_idx_msg: %p",
			msg_queue, pp_ipi_msg, p_idx_msg);
		return -EFAULT;
	}

	*pp_ipi_msg = NULL;
	*p_idx_msg = 0xFFFFFFFF;

	/* check queue empty */
	if (hal_dma_check_queue_empty(msg_queue) == true) {
		pr_info("queue empty, idx_r: %u, idx_w: %u",
			msg_queue->idx_r, msg_queue->idx_w);
		return -ENOMEM;
	}

	/* front */
	if (hal_dma_check_idx_msg_valid(msg_queue, msg_queue->idx_r) == false) {
		pr_info("idx_r %u is invalid!! return",
			msg_queue->idx_r);
		return -1;
	}
	*p_idx_msg = msg_queue->idx_r;
	*pp_ipi_msg = &msg_queue->msg[*p_idx_msg];

	data_size =  + (*pp_ipi_msg)->dma_info.data_size;

	if ((data_size + sizeof(struct ipi_msg_t)) > MAX_DSP_DMA_WRITE_SIZE) {
		pr_notice("%u + %zu > %u!!",
			  data_size, sizeof(struct ipi_msg_t),
			  MAX_DSP_DMA_WRITE_SIZE);
		hal_dma_pop(msg_queue);
		return -EOVERFLOW;
	}

	memcpy(msg_queue->tmp_buf_k2h, *pp_ipi_msg, sizeof(struct ipi_msg_t));

	audio_ringbuf_copy_to_linear(
		msg_queue->tmp_buf_k2h + sizeof(struct ipi_msg_t),
		&msg_queue->dma_data,
		data_size);

	return 0;
}

static int hal_dma_init_msg_queue(struct hal_dma_queue_t *msg_queue)
{
	int i = 0;

	if (msg_queue == NULL) {
		pr_info("NULL!! msg_queue: %p", msg_queue);
		return -EFAULT;
	}


	/* init var */
	for (i = 0; i < MAX_SCP_MSG_NUM_IN_QUEUE; i++)
		memset((void *)&msg_queue->msg[i], 0, sizeof(struct ipi_msg_t));

	msg_queue->size = MAX_SCP_MSG_NUM_IN_QUEUE;
	msg_queue->idx_r = 0;
	msg_queue->idx_w = 0;

	spin_lock_init(&msg_queue->queue_lock);
	init_waitqueue_head(&msg_queue->queue_wq);

	msg_queue->dma_data.size = (g_dma)
				   ? g_dma->size : MAX_DSP_DMA_WRITE_SIZE;
	msg_queue->dma_data.base = vmalloc(msg_queue->dma_data.size);
	msg_queue->dma_data.read = msg_queue->dma_data.base;
	msg_queue->dma_data.write = msg_queue->dma_data.base;

	msg_queue->tmp_buf_d2k = vmalloc(MAX_DSP_DMA_WRITE_SIZE);
	msg_queue->tmp_buf_k2h = vmalloc(MAX_DSP_DMA_WRITE_SIZE);


	return 0;
}

static int hal_dma_deinit_msg_queue(struct hal_dma_queue_t *msg_queue)
{
	if (msg_queue == NULL) {
		pr_info("NULL!! msg_queue: %p", msg_queue);
		return -EFAULT;
	}

	if (msg_queue->dma_data.base != NULL) {
		vfree(msg_queue->dma_data.base);
		msg_queue->dma_data.base = NULL;
	}

	if (msg_queue->tmp_buf_d2k != NULL) {
		vfree(msg_queue->tmp_buf_d2k);
		msg_queue->tmp_buf_d2k = NULL;
	}
	if (msg_queue->tmp_buf_k2h != NULL) {
		vfree(msg_queue->tmp_buf_k2h);
		msg_queue->tmp_buf_k2h = NULL;
	}


	return 0;
}



static int hal_dma_get_queue_msg(
	struct hal_dma_queue_t *msg_queue,
	struct ipi_msg_t **pp_ipi_msg,
	uint32_t *p_idx_msg)
{
	bool is_empty = true;

	unsigned long flags = 0;
	int retval = 0;

	uint32_t try_cnt = 0;
	const uint32_t k_max_try_cnt = 10; /* retry 1 sec for -ERESTARTSYS */
	const uint32_t k_restart_sleep_ms = 100; /* 100 ms */

	spin_lock_irqsave(&msg_queue->queue_lock, flags);
	is_empty = hal_dma_check_queue_empty(msg_queue);
	spin_unlock_irqrestore(&msg_queue->queue_lock, flags);

	/* wait until message is pushed to queue */
	if (is_empty == true) {
		for (try_cnt = 0; try_cnt < k_max_try_cnt; try_cnt++) {
			retval = wait_event_interruptible(
					 msg_queue->queue_wq,
					 !hal_dma_check_queue_empty(msg_queue));

			if (hal_dma_check_queue_empty(msg_queue) == false) {
				retval = 0;
				break;
			}
			if (retval == 0) { /* got msg in queue */
				pr_notice("wait ret 0, empty %d",
					  hal_dma_check_queue_empty(msg_queue));
				break;
			}
			if (retval == -ERESTARTSYS) {
				pr_info("-ERESTARTSYS, #%u, sleep ms: %u",
					try_cnt, k_restart_sleep_ms);
				retval = -EINTR;
				msleep(k_restart_sleep_ms);
				continue;
			}
			pr_notice("retval: %d not handle!!", retval);
		}
	}

	if (hal_dma_check_queue_empty(msg_queue) == false) {
		spin_lock_irqsave(&msg_queue->queue_lock, flags);
		retval = hal_dma_front(msg_queue, pp_ipi_msg, p_idx_msg);
		spin_unlock_irqrestore(&msg_queue->queue_lock, flags);
	}

	return retval;
}


int audio_ipi_dma_msg_to_hal(struct ipi_msg_t *p_ipi_msg)
{
	struct hal_dma_queue_t *msg_queue = &g_hal_dma_queue;

	uint32_t idx_msg = 0;

	int retval = 0;
	unsigned long flags = 0;


	if (p_ipi_msg == NULL || msg_queue == NULL) {
		pr_info("NULL!! return");
		return -EFAULT;
	}

	if (p_ipi_msg->data_type != AUDIO_IPI_DMA ||
	    p_ipi_msg->target_layer != AUDIO_IPI_LAYER_TO_HAL ||
	    p_ipi_msg->dma_info.data_size == 0) {
		DUMP_IPI_MSG("msg err", p_ipi_msg);
		return -EFAULT;
	}

#if 0
	DUMP_IPI_MSG("dma dsp -> kernel", p_ipi_msg);
#endif

	/* push message to queue */
	spin_lock_irqsave(&msg_queue->queue_lock, flags);
	retval = hal_dma_push(msg_queue,
			      p_ipi_msg,
			      &idx_msg);
	spin_unlock_irqrestore(&msg_queue->queue_lock, flags);
	if (retval != 0) {
		pr_info("push fail!!");
		return retval;
	}

	/* notify queue thread to process it */
	dsb(SY);
	wake_up_interruptible(&msg_queue->queue_wq);

	return 0;
}


size_t audio_ipi_dma_msg_read(void __user *buf, size_t count)
{
	struct hal_dma_queue_t *msg_queue = &g_hal_dma_queue;
	struct ipi_msg_t *p_ipi_msg = NULL;
	uint32_t idx_msg = 0;

	size_t copy_size = 0;

	unsigned long flags = 0;
	int retval = 0;

	if (buf == NULL || count == 0 || msg_queue == NULL) {
		pr_info("arg!! %p %zu %p, return", buf, count, msg_queue);
		msleep(500);
		return 0;
	}


	/* wait until element pushed */
	retval = hal_dma_get_queue_msg(msg_queue, &p_ipi_msg, &idx_msg);
	if (retval != 0) {
		pr_info("hal_dma_get_queue_msg retval %d", retval);
		msleep(100);
		return 0;
	}
	p_ipi_msg = &msg_queue->msg[idx_msg];

#if 0
	DUMP_IPI_MSG("dma kernel -> hal", p_ipi_msg);
#endif

	/* copy data */
	copy_size = sizeof(struct ipi_msg_t) +
		    p_ipi_msg->dma_info.data_size;
	if (count < copy_size) {
		pr_notice("%zu < %zu + %u!! drop",
			  count,
			  sizeof(struct ipi_msg_t),
			  p_ipi_msg->dma_info.data_size);
		copy_size = 0;
	} else {
		retval = copy_to_user(
				 buf,
				 msg_queue->tmp_buf_k2h,
				 copy_size);
		if (retval != 0)
			pr_info("hal_dma_get_queue_msg retval %d", retval);
	}


	/* pop message from queue */
	spin_lock_irqsave(&msg_queue->queue_lock, flags);
	hal_dma_pop(msg_queue);
	spin_unlock_irqrestore(&msg_queue->queue_lock, flags);

	return copy_size;
}



