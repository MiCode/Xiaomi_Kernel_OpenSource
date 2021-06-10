/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifdef JPEG_ENC_DRIVER
#include <linux/kernel.h>
/* #include <linux/xlog.h> */

#include "jpeg_drv_reg.h"
#include "jpeg_drv_common.h"

#define JPEG_ENC_RST_BIT                        0x1

#define JPEG_ENC_CTRL_ENABLE_BIT                0x01
#define JPEG_ENC_CTRL_DIS_GMC_BIT               0x02
#define JPEG_ENC_CTRL_INT_EN_BIT                0x04
#define JPEG_ENC_CTRL_YUV_BIT                   0x18
#define JPEG_ENC_CTRL_FILE_FORMAT_BIT           0x20
#define JPEG_ENC_CTRL_GRAY_EN_BIT               0x80
#define JPEG_ENC_CTRL_ULTRA_HIGH_EN_BIT         0x200
#define JPEG_ENC_CTRL_RESTART_EN_BIT            0x400
#define JPEG_ENC_CTRL_BURST_TYPE_MASK           0x00007000
#define JPEG_ENC_CTRL_BURST_TYPE_SHIFT_COUNT    12

#define JPEG_ENC_EN_DIS_GMC                     (1 << 2)
#define JPEG_ENC_EN_JFIF_EXIF                   (1 << 5)
#define JPEG_ENC_EN_SELF_INIT                   (1 << 16)

#define JPEG_ENC_DEBUG_INFO0_GMC_IDLE_MASK      (1 << 13)



unsigned int _jpeg_enc_int_status;


int jpeg_isr_enc_lisr(void)
{
	unsigned int tmp, tmp1;
	/* _jpeg_enc_int_status = REG_JPEG_ENC_INTERRUPT_STATUS; */
	tmp1 = IMG_REG_READ(REG_ADDR_JPEG_ENC_INTERRUPT_STATUS);
	tmp = tmp1 & (JPEG_DRV_ENC_INT_STATUS_MASK_ALLIRQ);
	if (tmp) {
		_jpeg_enc_int_status = tmp;

		/* / clear the interrupt status register */
		/* if(_jpeg_enc_int_status) */
		/* { */
		IMG_REG_WRITE(0, REG_ADDR_JPEG_ENC_INTERRUPT_STATUS);
		return 0;
		/* } */
	} else if (_jpeg_enc_int_status) {
		IMG_REG_WRITE(0, REG_ADDR_JPEG_ENC_INTERRUPT_STATUS);
		return 0;
	}

	return -1;
}





unsigned int jpeg_drv_enc_set_src_image(
	unsigned int width,
	 unsigned int height,
	 unsigned int yuv_format,
	 unsigned int totalEncDU)
{
	unsigned int ret = 1;

	ret &= jpeg_drv_enc_set_img_size(width, height);

	ret &= jpeg_drv_enc_set_encFormat(yuv_format);

	ret &= jpeg_drv_enc_set_blk_num(totalEncDU);

	return ret;
}



unsigned int jpeg_drv_enc_set_src_buf(
	unsigned int yuv_format, unsigned int img_stride,
	 unsigned int mem_stride, unsigned int srcAddr, unsigned int srcAddr_C)
{
	unsigned int ret = 1;

	if (yuv_format == 0x00 || yuv_format == 0x01) {
		if ((mem_stride & 0x1f) || (img_stride & 0x1f)) {
			JPEG_MSG
			("JPGENC:imag/mem stride not align fmt %x(%x/%x)\n",
			 yuv_format, mem_stride, img_stride);
			ret = 0;
		}
	}

	ret &= jpeg_drv_enc_set_image_stride(img_stride);

	ret &= jpeg_drv_enc_set_memory_stride(mem_stride);

	ret &= jpeg_drv_enc_set_luma_addr(srcAddr);

	ret &= jpeg_drv_enc_set_chroma_addr(srcAddr_C);

	return ret;
}


unsigned int jpeg_drv_enc_ctrl_cfg(unsigned int exif_en, unsigned int quality,
				 unsigned int restart_interval)
{
	jpeg_drv_enc_set_quality(quality);

	jpeg_drv_enc_set_restart_interval(restart_interval);

	jpeg_drv_enc_set_EncodeMode(exif_en);

	return 1;
}


void jpeg_drv_enc_dump_reg(void)
{
	unsigned int reg_value = 0;
	unsigned int index = 0;

	JPEG_MSG("===== JPEG ENC DUMP =====\n");
	for (index = 0x100; index < JPEG_ENC_REG_COUNT; index += 4) {
		/* reg_value = ioread32(JPEG_ENC_BASE + index); */
		reg_value = IMG_REG_READ(JPEG_ENC_BASE + index);
		JPEG_MSG("+0x%x 0x%08x\n", index, reg_value);
	}
}


void jpeg_drv_enc_start(void)
{
	unsigned int u4Value;

	u4Value = IMG_REG_READ(REG_ADDR_JPEG_ENC_CTRL);
	u4Value |= (JPEG_ENC_CTRL_INT_EN_BIT | JPEG_ENC_CTRL_ENABLE_BIT);
	/* REG_JPEG_ENC_CTRL |= */
	IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_CTRL);
	/*(JPEG_ENC_CTRL_INT_EN_BIT | JPEG_ENC_CTRL_ENABLE_BIT); */
}


/* workaround for jpeg odd read operation at cg gating state */
void jpeg_drv_enc_verify_state_and_reset(void)
{
	unsigned int temp, value;

	IMG_REG_WRITE((0), REG_ADDR_JPEG_ENC_RSTB);
	IMG_REG_WRITE((0), REG_ADDR_JPEG_ENC_RSTB);
	IMG_REG_WRITE((1), REG_ADDR_JPEG_ENC_RSTB);
	IMG_REG_WRITE((1), REG_ADDR_JPEG_ENC_RSTB);

	temp = IMG_REG_READ(REG_ADDR_JPEG_ENC_ULTRA_THRES);
	value = IMG_REG_READ(REG_ADDR_JPEG_ENC_DMA_ADDR0);

	/* issue happen, need to do 1 read at cg gating state */
	if (value == 0xFFFFFFFF) {
		JPEG_MSG("JPGENC APB R/W issue found, start to do recovery!\n");
		jpeg_drv_enc_power_off();
		value = IMG_REG_READ(REG_ADDR_JPEG_ENC_ULTRA_THRES);
		jpeg_drv_enc_power_on();
	}

	IMG_REG_WRITE((0), REG_ADDR_JPEG_ENC_CODEC_SEL);

	_jpeg_enc_int_status = 0;
}


void jpeg_drv_enc_reset(void)
{
	IMG_REG_WRITE((0), REG_ADDR_JPEG_ENC_RSTB);
	IMG_REG_WRITE((1), REG_ADDR_JPEG_ENC_RSTB);

	IMG_REG_WRITE((0), REG_ADDR_JPEG_ENC_CODEC_SEL);

	_jpeg_enc_int_status = 0;
}


unsigned int jpeg_drv_enc_warm_reset(void)
{
	unsigned int timeout = 0xFFFFF;
	unsigned int u4Value;

	u4Value = IMG_REG_READ(REG_ADDR_JPEG_ENC_CTRL);
	u4Value &= ~JPEG_ENC_CTRL_ENABLE_BIT;
	IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_CTRL);

	u4Value = IMG_REG_READ(REG_ADDR_JPEG_ENC_CTRL);
	u4Value |= JPEG_ENC_CTRL_ENABLE_BIT;
	IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_CTRL);


	while (0 ==
		 (IMG_REG_READ(REG_ADDR_JPEG_ENC_DEBUG_INFO0) &
		 JPEG_ENC_DEBUG_INFO0_GMC_IDLE_MASK)) {
		timeout--;
		if (timeout == 0) {
			JPEG_MSG("Wait for GMC IDLE timeout\n");
			return 0;
		}
	}

	u4Value = IMG_REG_READ(REG_ADDR_JPEG_ENC_RSTB);
	u4Value &= ~(JPEG_ENC_RST_BIT);
	IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_RSTB);

	u4Value = IMG_REG_READ(REG_ADDR_JPEG_ENC_RSTB);
	u4Value |= JPEG_ENC_RST_BIT;
	IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_RSTB);

	IMG_REG_WRITE((0), REG_ADDR_JPEG_ENC_CODEC_SEL);

	_jpeg_enc_int_status = 0;

	return 1;
}


unsigned int jpeg_drv_enc_set_encFormat(unsigned int encFormat)
{
	unsigned int val;
	unsigned int u4Value;

	if (encFormat & (~3)) {
		JPEG_ERR("JPEG_DRV_ENC: set encFormat Err %d!!\n", encFormat);
		return 0;
	}

	val = (encFormat & 3) << 3;
#if 0
/* REG_JPEG_ENC_CTRL &= ~JPEG_ENC_CTRL_YUV_BIT; */
/*  */
/* REG_JPEG_ENC_CTRL |= val; */
#else
	u4Value = IMG_REG_READ(REG_ADDR_JPEG_ENC_CTRL);

	u4Value &= ~JPEG_ENC_CTRL_YUV_BIT;

	u4Value |= val;

	IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_CTRL);
#endif

	return 1;
}


unsigned int jpeg_drv_enc_set_quality(unsigned int quality)
{
	unsigned int u4Value;

	if (quality == 0x8 || quality == 0xC) {
		JPEG_MSG("JPEGENC: set quality failed\n");
		return 0;
	}
	u4Value = IMG_REG_READ(REG_ADDR_JPEG_ENC_QUALITY);

	u4Value = (u4Value & 0xFFFF0000) | quality;
	IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_QUALITY);
	return 1;
}


unsigned int jpeg_drv_enc_set_img_size(unsigned int width, unsigned int height)
{
	unsigned int u4Value;

	if ((width & 0xffff0000) || (height & 0xffff0000)) {
		JPEG_MSG("JPGENC: img size exceed 65535, (%x, %x)\n",
			 width,
			 height);
		return 0;
	}
	u4Value = (width << 16) | height;

	IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_IMG_SIZE);

	return 1;
}


unsigned int jpeg_drv_enc_set_blk_num(unsigned int blk_num)	/* NO_USE */
{
	if (blk_num < 4)
		return 0;

	IMG_REG_WRITE((blk_num), REG_ADDR_JPEG_ENC_BLK_NUM);

	return 1;
}


unsigned int jpeg_drv_enc_set_luma_addr(unsigned int src_luma_addr)
{
	if (src_luma_addr & 0x0F)
		JPEG_MSG("JPGENC: set LUMA addr not align (%x)\n",
			 src_luma_addr);

	IMG_REG_WRITE((src_luma_addr), REG_ADDR_JPEG_ENC_SRC_LUMA_ADDR);

	return 1;
}


unsigned int jpeg_drv_enc_set_chroma_addr(unsigned int src_chroma_addr)
{
	if (src_chroma_addr & 0x0F)
		JPEG_MSG("JPGENC: set CHROMA addr not align (%x)\n",
			 src_chroma_addr);

	IMG_REG_WRITE((src_chroma_addr), REG_ADDR_JPEG_ENC_SRC_CHROMA_ADDR);

	return 1;
}


unsigned int jpeg_drv_enc_set_memory_stride(unsigned int mem_stride)
{
	if (mem_stride & 0x0F) {
		JPEG_MSG("JPGENC:memory stride not align to 0x1f (%x)\n",
			 mem_stride);
		return 0;
	}

	IMG_REG_WRITE((mem_stride), REG_ADDR_JPEG_ENC_STRIDE);

	return 1;
}


unsigned int jpeg_drv_enc_set_image_stride(unsigned int img_stride)
{
	if (img_stride & 0x0F) {
		JPEG_MSG("JPGENC: image stride not align to 0x0f (%x)\n",
			 img_stride);
		return 0;
	}

	IMG_REG_WRITE((img_stride), REG_ADDR_JPEG_ENC_IMG_STRIDE);

	return 1;
}


void jpeg_drv_enc_set_restart_interval(unsigned int restart_interval)
{
	unsigned int u4Value;

	u4Value = IMG_REG_READ(REG_ADDR_JPEG_ENC_CTRL);

	if (restart_interval != 0) {
		u4Value |= JPEG_ENC_CTRL_RESTART_EN_BIT;
		IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_CTRL);
	} else {
		u4Value &= ~JPEG_ENC_CTRL_RESTART_EN_BIT;
		IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_CTRL);
	}
	IMG_REG_WRITE((restart_interval), REG_ADDR_JPEG_ENC_RST_MCU_NUM);
}


unsigned int jpeg_drv_enc_set_offset_addr(unsigned int offset)
{
	if (offset & 0x0F) {
		JPEG_MSG("JPEGENC:WARN set offset addr %x\n", offset);
		/* return 0; */
	}

	IMG_REG_WRITE((offset), REG_ADDR_JPEG_ENC_OFFSET_ADDR);

	return 1;
}

unsigned int jpeg_drv_enc_set_dst_buff(
		unsigned int dst_addr, unsigned int stall_size,
		 unsigned int init_offset, unsigned int offset_mask)
{
	if (stall_size < 624) {
		JPEG_MSG("JPGENC:stall size less than 624 to write %d\n",
		 stall_size);
		return 0;
	}

	if (offset_mask & 0x0F) {
		JPEG_MSG("JPEGENC: set offset addr %x\n", offset_mask);
		/* return 0; */
	}

	IMG_REG_WRITE((init_offset & (~0xF)), REG_ADDR_JPEG_ENC_OFFSET_ADDR);

	IMG_REG_WRITE((offset_mask & 0xF), REG_ADDR_JPEG_ENC_BYTE_OFFSET_MASK);

	IMG_REG_WRITE((dst_addr & (~0xF)), REG_ADDR_JPEG_ENC_DST_ADDR0);

	IMG_REG_WRITE(((dst_addr + stall_size) & (~0xF)),
		 REG_ADDR_JPEG_ENC_STALL_ADDR0);


	return 1;
}

/* 0:JPG mode, 1:JFIF/EXIF mode */
void jpeg_drv_enc_set_EncodeMode(unsigned int exif_en)
{
	unsigned int u4Value;

	u4Value = IMG_REG_READ(REG_ADDR_JPEG_ENC_CTRL);
	u4Value &= ~(JPEG_ENC_CTRL_FILE_FORMAT_BIT);
	IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_CTRL);

	if (exif_en) {
		u4Value = IMG_REG_READ(REG_ADDR_JPEG_ENC_CTRL);
		u4Value |= JPEG_ENC_EN_JFIF_EXIF;
		IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_CTRL);
	}
}


void jpeg_drv_enc_set_gmc_disable_bit(void)
{
	unsigned int u4Value;

	u4Value = IMG_REG_READ(REG_ADDR_JPEG_ENC_CTRL);
	u4Value |= JPEG_ENC_EN_DIS_GMC;
	IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_CTRL);
}


void jpeg_drv_enc_set_burst_type(unsigned int burst_type)
{
	unsigned int u4Value;

	u4Value = IMG_REG_READ(REG_ADDR_JPEG_ENC_CTRL);
	u4Value &= ~JPEG_ENC_CTRL_BURST_TYPE_MASK;
	u4Value |= (burst_type << JPEG_ENC_CTRL_BURST_TYPE_SHIFT_COUNT);

	IMG_REG_WRITE((u4Value), REG_ADDR_JPEG_ENC_CTRL);
}

unsigned int jpeg_drv_enc_get_cycle_count(void)
{
	return IMG_REG_READ(REG_ADDR_JPEG_ENC_TOTAL_CYCLE);
}


unsigned int jpeg_drv_enc_get_file_size(void)
{
	unsigned int u4DMA_Addr0;
	unsigned int u4DST_Addr0;

	u4DMA_Addr0 = IMG_REG_READ(REG_ADDR_JPEG_ENC_DMA_ADDR0);
	u4DST_Addr0 = IMG_REG_READ(REG_ADDR_JPEG_ENC_DST_ADDR0);

	return (u4DMA_Addr0 - u4DST_Addr0);
	/* return REG_JPEG_ENC_CURR_DMA_ADDR - REG_JPEG_ENC_DST_ADDR0; */
}


#ifdef FPGA_VERSION

unsigned int jpeg_drv_enc_get_result(void)
{
	unsigned int file_size;

	file_size = jpeg_drv_enc_get_file_size();
	return file_size;
}

#else

unsigned int jpeg_drv_enc_get_result(unsigned int *fileSize)
{
	*fileSize = jpeg_drv_enc_get_file_size();

	if (_jpeg_enc_int_status & JPEG_DRV_ENC_INT_STATUS_DONE)
		return 0;
	else if (_jpeg_enc_int_status & JPEG_DRV_ENC_INT_STATUS_STALL)
		return 1;
	else if (_jpeg_enc_int_status & JPEG_DRV_ENC_INT_STATUS_VCODEC_IRQ)
		return 2;

	JPEG_MSG("JPEGENC: int_st %x!!\n", _jpeg_enc_int_status);
	return 3;
}

#endif
#endif
