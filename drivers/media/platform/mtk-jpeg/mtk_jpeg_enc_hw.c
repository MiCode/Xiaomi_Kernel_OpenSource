// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <media/videobuf2-core.h>

#include "mtk_jpeg_enc_hw.h"

void mtk_jpeg_enc_reset(void __iomem *base)
{
	writel(0x00, base + JPGENC_RSTB);
	writel(0x01, base + JPGENC_RSTB);
	writel(0x00, base + JPGENC_CODEC_SEL);
}

u32 mtk_jpeg_enc_get_int_status(void __iomem *base)
{
	u32 ret;

	ret = readl(base + JPGENC_INT_STS) &
			JPEG_DRV_ENC_INT_STATUS_MASK_ALLIRQ;
	if (ret)
		writel(0, base + JPGENC_INT_STS);

	return ret;
}

u32 mtk_jpeg_enc_get_file_size(void __iomem *base) //for dst size
{
	return readl(base + JPGENC_DMA_ADDR0) - readl(base + JPGENC_DST_ADDR0);
}

u32 mtk_jpeg_enc_enum_result(void __iomem *base, u32 irq_status, u32 *fileSize)
{
	*fileSize = mtk_jpeg_enc_get_file_size(base);
	if (irq_status & JPEG_DRV_ENC_INT_STATUS_DONE)
		return 0;
	else if (irq_status & JPEG_DRV_ENC_INT_STATUS_STALL)
		return 1;
	else if (irq_status & JPEG_DRV_ENC_INT_STATUS_VCODEC_IRQ)
		return 2;
	return 3;
}

static void mtk_jpeg_enc_set_blk_num(void __iomem *base, u32 blk_num)
{
	writel((blk_num), base + JPGENC_BLK_NUM);
}

static void mtk_jpeg_enc_set_encFormat(void __iomem *base, u32 encFormat)
{
	u32 val;
	u32 u4Value;

	val = (encFormat & 3) << 3;

	u4Value = readl(base + JPGENC_CTRL);
	u4Value &= ~JPEG_ENC_CTRL_YUV_BIT;
	u4Value |= val;
	writel((u4Value), base + JPGENC_CTRL);
}

static void mtk_jpeg_enc_set_img_size(void __iomem *base, u32 width, u32 height)
{
	u32 u4Value;

	u4Value = (width << 16) | height;
	writel((u4Value), base + JPGENC_IMG_SIZE);
}

static void mtk_jpeg_enc_set_src_img(void __iomem *base, u32 width,
					     u32 height, u32 yuv_format,
					     u32 totalEncDU)
{

	mtk_jpeg_enc_set_img_size(base, width, height);
	mtk_jpeg_enc_set_encFormat(base, yuv_format);
	mtk_jpeg_enc_set_blk_num(base, totalEncDU);
}

static void mtk_jpeg_enc_set_image_stride(void __iomem *base, u32 img_stride)
{
	writel((img_stride), base + JPGENC_IMG_STRIDE);
}

static void mtk_jpeg_enc_set_memory_stride(void __iomem *base, u32 mem_stride)
{
	writel((mem_stride), base + JPGENC_STRIDE);
}

static void mtk_jpeg_enc_set_luma_addr(void __iomem *base, u32 src_luma_addr)
{
	writel((src_luma_addr), base + JPGENC_SRC_LUMA_ADDR);
}

static void mtk_jpeg_enc_set_chroma_addr(void __iomem *base,
				u32 src_chroma_addr)
{
	writel((src_chroma_addr), base + JPGENC_SRC_CHROMA_ADDR);
}

static void mtk_jpeg_enc_set_src_buf(void __iomem *base, u32 img_stride,
				    u32 mem_stride, u32 srcAddr, u32 srcAddr_C)
{

	mtk_jpeg_enc_set_image_stride(base, img_stride);
	mtk_jpeg_enc_set_memory_stride(base, mem_stride);
	mtk_jpeg_enc_set_luma_addr(base, srcAddr);
	mtk_jpeg_enc_set_chroma_addr(base, srcAddr_C);
}

static void mtk_jpeg_enc_set_dst_buf(void __iomem *base, u32 dst_addr,
			u32 stall_size, u32 init_offset, u32 offset_mask)
{

	writel((init_offset & (~0xF)), base + JPGENC_OFFSET_ADDR);
	writel((offset_mask & 0xF), base + JPGENC_BYTE_OFFSET_MASK);
	writel((dst_addr & (~0xF)), base + JPGENC_DST_ADDR0);
	writel(((dst_addr + stall_size) & (~0xF)), base + JPGENC_STALL_ADDR0);
}

static void mtk_jpeg_enc_set_quality(void __iomem *base, u32 quality)
{
	u32 u4Value;

	u4Value = readl(base + JPGENC_QUALITY);
	u4Value = (u4Value & 0xFFFF0000) | quality;
	writel((u4Value), base + JPGENC_QUALITY);
}

static void mtk_jpeg_enc_set_restart_interval(void __iomem *base,
					u32 restart_interval)
{
	u32 Value;

	Value = readl(base + JPGENC_CTRL);
	if (restart_interval != 0) {
		Value |= JPEG_ENC_CTRL_RESTART_EN_BIT;
		writel((Value), base + JPGENC_CTRL);
	} else {
		Value &= ~JPEG_ENC_CTRL_RESTART_EN_BIT;
		writel((Value), base + JPGENC_CTRL);
	}
	writel((restart_interval), base + JPGENC_RST_MCU_NUM);
}

static void mtk_jpeg_enc_set_EncodeMode(void __iomem *base, u32 exif_en)
{
	u32 u4Value;

	u4Value = readl(base + JPGENC_CTRL);
	u4Value &= ~(JPEG_ENC_CTRL_FILE_FORMAT_BIT);
	writel((u4Value), base + JPGENC_CTRL);

	if (exif_en) {
		u4Value = readl(base + JPGENC_CTRL);
		u4Value |= JPEG_ENC_EN_JFIF_EXIF;
		writel((u4Value), base + JPGENC_CTRL);
	}
}

static void mtk_jpeg_enc_set_ctrl_cfg(void __iomem *base, u32 exif_en,
				u32 quality, u32 restart_interval)
{
	mtk_jpeg_enc_set_quality(base, quality);

	mtk_jpeg_enc_set_restart_interval(base, restart_interval);

	mtk_jpeg_enc_set_EncodeMode(base, exif_en);
}

void mtk_jpeg_enc_start(void __iomem *base)
{
	u32 u4Value;

	u4Value = readl(base + JPGENC_CTRL);
	u4Value |= (JPEG_ENC_CTRL_INT_EN_BIT | JPEG_ENC_CTRL_ENABLE_BIT);
	writel((u4Value), base + JPGENC_CTRL);
}

void mtk_jpeg_enc_set_config(void __iomem *base,
				  struct mtk_jpeg_enc_param *config,
				  struct mtk_jpeg_enc_bs *bs,
				  struct mtk_jpeg_enc_fb *fb)
{
	mtk_jpeg_enc_set_src_img(base, config->enc_w, config->enc_h,
				config->enc_format, config->total_encdu);
	mtk_jpeg_enc_set_src_buf(base, config->img_stride, config->mem_stride,
	fb->fb_addr[0].dma_addr, fb->fb_addr[1].dma_addr);
	mtk_jpeg_enc_set_dst_buf(base, bs->dma_addr, bs->size,
				bs->dma_addr_offset, bs->dma_addr_offsetmask);
	mtk_jpeg_enc_set_ctrl_cfg(base, config->enable_exif,
				config->enc_quality, config->restart_interval);
}
