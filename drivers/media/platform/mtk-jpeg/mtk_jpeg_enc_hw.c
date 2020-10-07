// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Xia Jiang <xia.jiang@mediatek.com>
 *
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <media/videobuf2-core.h>

#include "mtk_jpeg_enc_hw.h"

static const struct mtk_jpeg_enc_qlt mtk_jpeg_enc_quality[] = {
	{.quality_param = 34, .hardware_value = JPEG_ENC_QUALITY_Q34},
	{.quality_param = 39, .hardware_value = JPEG_ENC_QUALITY_Q39},
	{.quality_param = 48, .hardware_value = JPEG_ENC_QUALITY_Q48},
	{.quality_param = 60, .hardware_value = JPEG_ENC_QUALITY_Q60},
	{.quality_param = 64, .hardware_value = JPEG_ENC_QUALITY_Q64},
	{.quality_param = 68, .hardware_value = JPEG_ENC_QUALITY_Q68},
	{.quality_param = 74, .hardware_value = JPEG_ENC_QUALITY_Q74},
	{.quality_param = 80, .hardware_value = JPEG_ENC_QUALITY_Q80},
	{.quality_param = 82, .hardware_value = JPEG_ENC_QUALITY_Q82},
	{.quality_param = 84, .hardware_value = JPEG_ENC_QUALITY_Q84},
	{.quality_param = 87, .hardware_value = JPEG_ENC_QUALITY_Q87},
	{.quality_param = 90, .hardware_value = JPEG_ENC_QUALITY_Q90},
	{.quality_param = 92, .hardware_value = JPEG_ENC_QUALITY_Q92},
	{.quality_param = 95, .hardware_value = JPEG_ENC_QUALITY_Q95},
	{.quality_param = 97, .hardware_value = JPEG_ENC_QUALITY_Q97},
};

void mtk_jpeg_enc_reset(void __iomem *base)
{
	writel(0x00, base + JPEG_ENC_RSTB);
	writel(JPEG_ENC_RESET_BIT, base + JPEG_ENC_RSTB);
	writel(0x00, base + JPEG_ENC_CODEC_SEL);
}

u32 mtk_jpeg_enc_get_and_clear_int_status(void __iomem *base)
{
	u32 ret;

	ret = readl(base + JPEG_ENC_INT_STS) &
		    JPEG_ENC_INT_STATUS_MASK_ALLIRQ;
	if (ret)
		writel(0, base + JPEG_ENC_INT_STS);

	return ret;
}

u32 mtk_jpeg_enc_get_file_size(void __iomem *base)
{
	return readl(base + JPEG_ENC_DMA_ADDR0) -
	       readl(base + JPEG_ENC_DST_ADDR0);
}

u32 mtk_jpeg_enc_enum_result(void __iomem *base, u32 irq_status)
{
	if (irq_status & JPEG_ENC_INT_STATUS_DONE) {
		pr_info("%s status done", __func__);
		return MTK_JPEG_ENC_RESULT_DONE;
	} else if (irq_status & JPEG_ENC_INT_STATUS_STALL) {
		pr_info("%s status stall", __func__);
		return MTK_JPEG_ENC_RESULT_STALL;
	} else
		return MTK_JPEG_ENC_RESULT_VCODEC_IRQ;
}

void mtk_jpeg_enc_set_img_size(void __iomem *base, u32 width, u32 height)
{
	u32 value;

	value = width << 16 | height;
	writel(value, base + JPEG_ENC_IMG_SIZE);
}

void mtk_jpeg_enc_set_blk_num(void __iomem *base, u32 enc_format, u32 width,
			      u32 height)
{
	u32 blk_num;
	u32 is_420;
	u32 padding_width;
	u32 padding_height;
	u32 luma_blocks;
	u32 chroma_blocks;

	is_420 = (enc_format == V4L2_PIX_FMT_NV12M ||
		  enc_format == V4L2_PIX_FMT_NV21M) ? 1 : 0;
	padding_width = round_up(width, 16);
	padding_height = round_up(height, is_420 ? 16 : 8);

	luma_blocks = padding_width / 8 * padding_height / 8;
	if (is_420)
		chroma_blocks = luma_blocks / 4;
	else
		chroma_blocks = luma_blocks / 2;

	blk_num = luma_blocks + 2 * chroma_blocks - 1;

	writel(blk_num, base + JPEG_ENC_BLK_NUM);
}

void mtk_jpeg_enc_set_stride(void __iomem *base, u32 enc_format, u32 width,
			     u32 height, u32 bytesperline)
{
	u32 img_stride;
	u32 mem_stride;

	if (enc_format == V4L2_PIX_FMT_NV12M ||
	    enc_format == V4L2_PIX_FMT_NV21M) {
		img_stride = round_up(width, 16);
		mem_stride = bytesperline;
	} else {
		img_stride = round_up(width * 2, 32);
		mem_stride = img_stride;
	}

	writel(img_stride, base + JPEG_ENC_IMG_STRIDE);
	writel(mem_stride, base + JPEG_ENC_STRIDE);
}

void mtk_jpeg_enc_set_src_addr(void __iomem *base, u32 src_addr,
			       u32 plane_index)
{
	if (!plane_index)
		writel(src_addr, base + JPEG_ENC_SRC_LUMA_ADDR);
	else
		writel(src_addr, base + JPEG_ENC_SRC_CHROMA_ADDR);
}

void mtk_jpeg_enc_set_dst_addr(void __iomem *base, u32 dst_addr,
			       u32 stall_size, u32 init_offset,
			       u32 offset_mask)
{
	writel(init_offset & ~0xf, base + JPEG_ENC_OFFSET_ADDR);
	writel(offset_mask & 0xf, base + JPEG_ENC_BYTE_OFFSET_MASK);
	writel(dst_addr & ~0xf, base + JPEG_ENC_DST_ADDR0);
	writel((dst_addr + stall_size) & ~0xf, base + JPEG_ENC_STALL_ADDR0);
}

static void mtk_jpeg_enc_set_quality(void __iomem *base, u32 quality)
{
	u32 value;
	u32 i, enc_quality;

	enc_quality = mtk_jpeg_enc_quality[0].hardware_value;
	for (i = 0; i < ARRAY_SIZE(mtk_jpeg_enc_quality); i++) {
		if (quality <= mtk_jpeg_enc_quality[i].quality_param) {
			enc_quality = mtk_jpeg_enc_quality[i].hardware_value;
			break;
		}
	}

	value = readl(base + JPEG_ENC_QUALITY);
	value = (value & JPEG_ENC_QUALITY_MASK) | enc_quality;
	writel(value, base + JPEG_ENC_QUALITY);
}

static void mtk_jpeg_enc_set_ctrl(void __iomem *base, u32 enc_format,
				  bool exif_en, u32 restart_interval)
{
	u32 value;

	value = readl(base + JPEG_ENC_CTRL);
	value &= ~JPEG_ENC_CTRL_YUV_FORMAT_MASK;
	value |= (enc_format & 3) << 3;
	if (exif_en)
		value |= JPEG_ENC_CTRL_FILE_FORMAT_BIT;
	else
		value &= ~JPEG_ENC_CTRL_FILE_FORMAT_BIT;
	if (restart_interval)
		value |= JPEG_ENC_CTRL_RESTART_EN_BIT;
	else
		value &= ~JPEG_ENC_CTRL_RESTART_EN_BIT;
	writel(value, base + JPEG_ENC_CTRL);
}

void mtk_jpeg_enc_set_config(void __iomem *base, u32 enc_format, bool exif_en,
			     u32 quality, u32 restart_interval)
{
	mtk_jpeg_enc_set_quality(base, quality);

	mtk_jpeg_enc_set_ctrl(base, enc_format, exif_en, restart_interval);

	writel(restart_interval, base + JPEG_ENC_RST_MCU_NUM);
}

void mtk_jpeg_enc_start(void __iomem *base)
{
	u32 value;

	value = readl(base + JPEG_ENC_CTRL);
	value |= JPEG_ENC_CTRL_INT_EN_BIT | JPEG_ENC_CTRL_ENABLE_BIT;
	writel(value, base + JPEG_ENC_CTRL);
}
