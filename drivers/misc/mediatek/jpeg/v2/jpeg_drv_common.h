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

#ifndef __JPEG_DRV_COMMON_H__
#define __JPEG_DRV_COMMON_H__

/* #include <mach/mt_typedefs.h> */
/* #include <mach/typedefs.h> */

#include "jpeg_drv.h"
#include <ion_drv.h>




extern unsigned int _jpeg_enc_int_status;
extern unsigned int _jpeg_dec_int_status;
extern unsigned int _jpeg_hybrid_dec_int_status[HW_CORE_NUMBER];
extern unsigned int _jpeg_dec_mode;

enum JpegDrvEncYUVFormat {
	YUYV,
	YVYU,
	NV12,
	NV21
};

enum JpegDrvEncQuality {
	Q60 = 0x0,
	Q80 = 0x1,
	Q90 = 0x2,
	Q95 = 0x3,

	Q39 = 0x4,
	Q68 = 0x5,
	Q84 = 0x6,
	Q92 = 0x7,

	Q48 = 0x9,
	Q74 = 0xA,
	Q87 = 0xB,

	Q34 = 0xD,
	Q64 = 0xE,
	Q82 = 0xF,

	Q_ALL = 0x10
};


struct JpegDrvEncSrcCfg {
	unsigned int width;
	unsigned int height;
	unsigned int yuv_format;
	unsigned int luma_addr;
	unsigned int chroma_addr;
};

struct JpegDrvEncDstCfg {
	unsigned int exif_en;	/* 0:JPG mode, 1:JFIF/EXIF mode */
	unsigned int dst_addr;
	unsigned int dst_size;
	unsigned int offset_addr;
	unsigned int byte_offset_mask;
};

struct JpegDrvEncCtrlCfg {
	unsigned int quality;
	unsigned int restart_interval;
	unsigned int gmc_disable;	/* HW not support */
};

#define JPEG_DRV_ENC_YUYV                     (0x00 << 3)
#define JPEG_DRV_ENC_YVYU                     (0x01 << 3)
#define JPEG_DRV_ENC_NV12                     (0x02 << 3)
#define JPEG_DRV_ENC_NV21                     (0x03 << 3)

#define JPEG_MSG pr_debug
#define JPEG_WRN pr_debug
#define JPEG_ERR pr_debug
#define JPEG_VEB pr_debug
#define JPEG_LOG(level, format, args...)                       \
	do {                                                        \
		if ((jpg_dbg_level & level) == level)              \
			pr_info("[JPEG] level=%d %s(),%d: " format "\n",\
				level, __func__, __LINE__, ##args);      \
	} while (0)

/* /////// JPEG Driver Decoder /////// */
/*  */
/*  */
void jpeg_drv_dec_power_on(void);
void jpeg_drv_dec_power_off(void);

unsigned int jpeg_drv_dec_set_config_data(struct JPEG_DEC_DRV_IN *config);
unsigned int jpeg_drv_dec_set_dst_bank0(
	unsigned int addr_Y,
	 unsigned int addr_U,
	 unsigned int addr_V);
void jpeg_drv_dec_verify_state_and_reset(void);
void jpeg_drv_dec_reset(void);
void jpeg_drv_dec_warm_reset(void);
void jpeg_drv_dec_start(void);
int jpeg_drv_dec_wait(struct JPEG_DEC_DRV_IN *config);
void jpeg_drv_dec_dump_key_reg(void);
void jpeg_drv_dec_dump_reg(void);
int jpeg_drv_dec_break(void);

unsigned int jpeg_drv_dec_set_pause_mcu_idx(unsigned int McuIdx);
void jpeg_drv_dec_resume(unsigned int resume);

unsigned int jpeg_drv_dec_get_result(void);


/* ///// JPEG Driver Encoder /////// */

void jpeg_drv_enc_power_on(void);
void jpeg_drv_enc_power_off(void);

unsigned int jpeg_drv_enc_src_cfg(struct JpegDrvEncSrcCfg srcCfg);
unsigned int jpeg_drv_enc_dst_buff(struct JpegDrvEncDstCfg dstCfg);
unsigned int jpeg_drv_enc_ctrl_cfg(unsigned int exif_en, unsigned int quality,
				 unsigned int restart_interval);

void jpeg_drv_enc_verify_state_and_reset(void);
void jpeg_drv_enc_reset(void);
unsigned int jpeg_drv_enc_warm_reset(void);
void jpeg_drv_enc_start(void);
unsigned int jpeg_drv_enc_set_quality(unsigned int quality);
unsigned int jpeg_drv_enc_set_img_size(unsigned int width, unsigned int height);
unsigned int jpeg_drv_enc_set_blk_num(unsigned int blk_num);
unsigned int jpeg_drv_enc_set_luma_addr(dma_addr_t src_luma_addr);
unsigned int jpeg_drv_enc_set_chroma_addr(dma_addr_t src_luma_addr);
unsigned int jpeg_drv_enc_set_memory_stride(unsigned int mem_stride);
unsigned int jpeg_drv_enc_set_image_stride(unsigned int img_stride);
void jpeg_drv_enc_set_restart_interval(unsigned int restart_interval);
unsigned int jpeg_drv_enc_set_offset_addr(unsigned int offset);
/* 0:JPG mode, 1:JFIF/EXIF mode */
void jpeg_drv_enc_set_EncodeMode(unsigned int exif_en);
void jpeg_drv_enc_set_burst_type(unsigned int burst_type);
unsigned int jpeg_drv_enc_set_dst_buff(
	struct ion_client *pIonClient,
	int dstFd,
	 unsigned int stall_size,
	 unsigned int init_offset,
	 unsigned int offset_mask);
unsigned int jpeg_drv_enc_set_sample_format_related(
	unsigned int width,
	 unsigned int height,
	 unsigned int yuv_format);
unsigned int jpeg_drv_enc_get_file_size(void);
unsigned int jpeg_drv_enc_get_result(unsigned int *fileSize);
unsigned int jpeg_drv_enc_get_cycle_count(void);

void jpeg_drv_enc_dump_reg(void);

unsigned int jpeg_drv_enc_rw_reg(void);
void jpegenc_drv_enc_remove_bw_request(void);
void jpeg_drv_enc_prepare_bw_request(void);
void jpegenc_drv_enc_update_bw_request(struct JPEG_ENC_DRV_IN cfgEnc);


int jpeg_isr_enc_lisr(void);
int jpeg_isr_dec_lisr(void);
int jpeg_isr_hybrid_dec_lisr(int id);
int jpeg_drv_hybrid_dec_start(unsigned int data[],
				unsigned int id,
				int *index_buf_fd);

void jpeg_drv_hybrid_dec_get_p_n_s(unsigned int id,
				int *progress_n_status);



unsigned int jpeg_drv_enc_set_src_image(
	unsigned int width,
	 unsigned int height,
	 unsigned int yuv_format,
	 unsigned int totalEncDU);

unsigned int jpeg_drv_enc_set_src_buf(
		struct ion_client *pIonClient,
		unsigned int yuv_format,
		 unsigned int img_stride,
		 unsigned int mem_stride,
		 unsigned int mem_height,
		 int srcFd,
		 int srcFd2);
unsigned int jpeg_drv_enc_set_encFormat(unsigned int encFormat);

#endif
