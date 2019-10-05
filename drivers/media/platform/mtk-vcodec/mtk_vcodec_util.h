/*
* Copyright (c) 2016 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*       Tiffany Lin <tiffany.lin@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#ifndef _MTK_VCODEC_UTIL_H_
#define _MTK_VCODEC_UTIL_H_

#include <linux/types.h>
#include <linux/dma-direction.h>

/* #define FPGA_PWRCLK_API_DISABLE */
/* #define FPGA_INTERRUPT_API_DISABLE */

struct mtk_vcodec_mem {
	size_t length;
	size_t size;
	size_t data_offset;
	void *va;
	dma_addr_t dma_addr;
	struct dma_buf *dmabuf;
	__u32 flags;
	__u32 index;
};

/**
 * enum flags  - decoder different operation types
 * @NO_CAHCE_FLUSH	: no need to proceed cache flush
 * @NO_CAHCE_INVALIDATE	: no need to proceed cache invalidate
 * @CROP_CHANGED	: frame buffer crop changed
 * @REF_FREED	: frame buffer is reference freed
 */
enum mtk_vcodec_flags {
	NO_CAHCE_CLEAN = 1,
	NO_CAHCE_INVALIDATE = 1 << 1,
	CROP_CHANGED = 1 << 2,
	REF_FREED = 1 << 3
};

struct mtk_vcodec_ctx;
struct mtk_vcodec_dev;

extern int mtk_v4l2_dbg_level;
extern bool mtk_vcodec_dbg;
extern bool mtk_vcodec_perf;

#define DEBUG   1

#if defined(DEBUG)

#define mtk_v4l2_debug(level, fmt, args...)                              \
	do {                                                             \
		if ((mtk_v4l2_dbg_level & level) == level)           \
			pr_info("[MTK_V4L2] level=%d %s(),%d: " fmt "\n",\
				level, __func__, __LINE__, ##args);      \
	} while (0)

#define mtk_v4l2_err(fmt, args...)                \
	pr_err("[MTK_V4L2][ERROR] %s:%d: " fmt "\n", __func__, __LINE__, \
		   ##args)


#define mtk_v4l2_debug_enter()  mtk_v4l2_debug(8, "+")
#define mtk_v4l2_debug_leave()  mtk_v4l2_debug(8, "-")

#define mtk_vcodec_debug(h, fmt, args...)                               \
	do {                                                            \
		if (mtk_vcodec_dbg)                                  \
			pr_info("[MTK_VCODEC][%d]: %s() " fmt "\n",     \
				((struct mtk_vcodec_ctx *)h->ctx)->id,  \
				__func__, ##args);                      \
	} while (0)

#define mtk_vcodec_perf_log(fmt, args...)                               \
	do {                                                            \
		if (mtk_vcodec_perf)                          \
			pr_info("[MTK_PERF] " fmt "\n", ##args);        \
	} while (0)


#define mtk_vcodec_err(h, fmt, args...)                                 \
	pr_info("[MTK_VCODEC][ERROR][%d]: %s() " fmt "\n",               \
		   ((struct mtk_vcodec_ctx *)h->ctx)->id, __func__, ##args)

#define mtk_vcodec_debug_enter(h)  mtk_vcodec_debug(h, "+")
#define mtk_vcodec_debug_leave(h)  mtk_vcodec_debug(h, "-")

#else

#define mtk_v4l2_debug(level, fmt, args...)
#define mtk_v4l2_err(fmt, args...)
#define mtk_v4l2_debug_enter()
#define mtk_v4l2_debug_leave()

#define mtk_vcodec_debug(h, fmt, args...)
#define mtk_vcodec_err(h, fmt, args...)
#define mtk_vcodec_debug_enter(h)
#define mtk_vcodec_debug_leave(h)

#endif

void __iomem *mtk_vcodec_get_dec_reg_addr(struct mtk_vcodec_ctx *data,
	unsigned int reg_idx);
void __iomem *mtk_vcodec_get_enc_reg_addr(struct mtk_vcodec_ctx *data,
	unsigned int reg_idx);
int mtk_vcodec_mem_alloc(struct mtk_vcodec_ctx *data,
	struct mtk_vcodec_mem *mem);
void mtk_vcodec_mem_free(struct mtk_vcodec_ctx *data,
	struct mtk_vcodec_mem *mem);
void mtk_vcodec_set_curr_ctx(struct mtk_vcodec_dev *dev,
	struct mtk_vcodec_ctx *ctx);
struct mtk_vcodec_ctx *mtk_vcodec_get_curr_ctx(struct mtk_vcodec_dev *dev);
struct vdec_fb *mtk_vcodec_get_fb(struct mtk_vcodec_ctx *ctx);
int mtk_vdec_put_fb(struct mtk_vcodec_ctx *ctx, int type);

#endif /* _MTK_VCODEC_UTIL_H_ */
