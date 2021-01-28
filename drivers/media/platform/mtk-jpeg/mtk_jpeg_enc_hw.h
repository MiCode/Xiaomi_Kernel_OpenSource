/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_JPEG_ENC_HW_H
#define _MTK_JPEG_ENC_HW_H

#include <media/videobuf2-core.h>
#include "mtk_jpeg_core.h"
#include "mtk_jpeg_enc_reg.h"

#define JPEG_ENC_DST_ADDR_OFFSET_MASK 0x0f
#define JPEG_ENC_CTRL_YUV_BIT 0x18
#define JPEG_ENC_CTRL_RESTART_EN_BIT            0x400
#define JPEG_ENC_CTRL_FILE_FORMAT_BIT           0x20
#define JPEG_ENC_EN_JFIF_EXIF                   (1 << 5)
#define JPEG_ENC_CTRL_INT_EN_BIT                0x04
#define JPEG_ENC_CTRL_ENABLE_BIT                0x01
#define JPEG_ENC_CTRL_RDMA_PADDING_EN           (1 << 20)
#define JPEG_ENC_CTRL_RDMA_RIGHT_PADDING_EN     (1 << 29)
#define JPEG_ENC_CTRL_RDMA_PADDING_0_EN         (1 << 30)

enum {
	MTK_JPEG_ENC_RESULT_DONE		= 0,
	MTK_JPEG_ENC_RESULT_STALL		= 1,
	MTK_JPEG_ENC_RESULT_VCODEC_IRQ		= 2,
	MTK_JPEG_ENC_RESULT_ERROR_UNKNOWN	= 3
};

struct mtk_jpeg_enc_bs {
	dma_addr_t	dma_addr;
	size_t		size;
	u32			dma_addr_offset;
	u32			dma_addr_offsetmask;
};
struct mtk_jpeg_mem {
	dma_addr_t	dma_addr;
	size_t		size;
};
struct mtk_jpeg_enc_fb {
	struct mtk_jpeg_mem	fb_addr[MTK_JPEG_COMP_MAX];
	u32			num_planes;
};
void mtk_jpeg_enc_reset(void __iomem *base);
u32 mtk_jpeg_enc_get_int_status(void __iomem *base);
u32 mtk_jpeg_enc_get_file_size(const void __iomem *base);
u32 mtk_jpeg_enc_enum_result(void __iomem *base, u32 irq_status, u32 *fileSize);
void mtk_jpeg_enc_start(void __iomem *enc_reg_base);
void mtk_jpeg_enc_set_config(void __iomem *base,
				  struct mtk_jpeg_enc_param *config,
				  struct mtk_jpeg_enc_bs *bs,
				  struct mtk_jpeg_enc_fb *fb);
#endif /* _MTK_JPEG_HW_H */
