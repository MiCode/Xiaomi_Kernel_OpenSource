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

#ifndef __JPEG_DRV_6589_COMMON_H__
#define __JPEG_DRV_6589_COMMON_H__

/* #include <mach/mt_typedefs.h> */
/* #include <mach/typedefs.h> */

#include "jpeg_drv.h"
#ifndef CONFIG_MTK_CLKMGR
#include <linux/clk.h>
#endif

typedef signed char     kal_int8;
typedef signed short    kal_int16;
typedef signed int      kal_int32;
typedef long long       kal_int64;
typedef unsigned char   kal_uint8;
typedef unsigned short  kal_uint16;
typedef unsigned int    kal_uint32;
typedef unsigned long long  kal_uint64;
typedef char            kal_char;

extern kal_uint32 _jpeg_enc_int_status;

typedef enum {
	YUYV,
	YVYU,
	NV12,
	NV21
} JpegDrvEncYUVFormat;

typedef enum {
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
} JpegDrvEncQuality;


typedef struct {
	kal_uint32 width;
	kal_uint32 height;
	kal_uint32 yuv_format;
	kal_uint32 luma_addr;
	kal_uint32 chroma_addr;
} JpegDrvEncSrcCfg;

typedef struct {
	kal_uint32 exif_en;	/* 0:JPG mode, 1:JFIF/EXIF mode */
	kal_uint32 dst_addr;
	kal_uint32 dst_size;
	kal_uint32 offset_addr;
	kal_uint32 byte_offset_mask;
} JpegDrvEncDstCfg;

typedef struct {
	kal_uint32 quality;
	kal_uint32 restart_interval;
	kal_uint32 gmc_disable;	/* HW not support */
} JpegDrvEncCtrlCfg;

#define JPEG_DRV_ENC_YUYV                     (0x00 << 3)
#define JPEG_DRV_ENC_YVYU                     (0x01 << 3)
#define JPEG_DRV_ENC_NV12                     (0x02 << 3)
#define JPEG_DRV_ENC_NV21                     (0x03 << 3)

#define JPEG_MSG pr_debug
#define JPEG_WRN pr_warn
#define JPEG_ERR pr_err
#define JPEG_VEB pr_err
/* /////// JPEG Driver Decoder /////// */
/*  */
/*  */
#ifdef JPEG_DEC_DRIVER
extern kal_uint32 _jpeg_dec_int_status;
extern kal_uint32 _jpeg_dec_mode;

int jpeg_isr_dec_lisr(void);
const long jpeg_dev_get_decoder_base_VA(void);

int jpeg_drv_dec_set_config_data(JPEG_DEC_DRV_IN *config);
void jpeg_drv_dec_set_dst_bank0(unsigned int addr_Y, unsigned int addr_U, unsigned int addr_V);
void jpeg_drv_dec_reset(void);
void jpeg_drv_dec_hard_reset(void);
void jpeg_drv_dec_start(void);
int jpeg_drv_dec_wait(JPEG_DEC_DRV_IN *config);
void jpeg_drv_dec_dump_key_reg(void);
void jpeg_drv_dec_dump_reg(void);
int jpeg_drv_dec_break(void);
void jpeg_drv_dec_set_pause_mcu_idx(unsigned int McuIdx);
void jpeg_drv_dec_resume(unsigned int resume);
kal_uint32 jpeg_drv_dec_get_result(void);
void jpeg_drv_dec_power_on(void);
void jpeg_drv_dec_power_off(void);
#endif

typedef struct JpegDeviceStruct {

	struct device *pDev;
	long encRegBaseVA;	/* considering 64 bit kernel, use long */
	long decRegBaseVA;
	uint32_t encIrqId;
	uint32_t decIrqId;

} JpegDeviceStruct;

typedef struct JpegClk {
	struct clk *clk_disp_mtcmos;
	struct clk *clk_venc_mtcmos;
	struct clk *clk_disp_smi;
	struct clk *clk_venc_larb;
	struct clk *clk_venc_jpgEnc;
	struct clk *clk_venc_jpgDec;
	struct clk *clk_venc_jpgDec_Smi;
} JpegClk;

const long jpeg_dev_get_encoder_base_VA(void);
/* ///// JPEG Driver Encoder /////// */

kal_uint32 jpeg_drv_enc_src_cfg(JpegDrvEncSrcCfg srcCfg);
kal_uint32 jpeg_drv_enc_dst_buff(JpegDrvEncDstCfg dstCfg);
kal_uint32 jpeg_drv_enc_ctrl_cfg(kal_uint32 exif_en, kal_uint32 quality,
				 kal_uint32 restart_interval);

void jpeg_drv_enc_reset(void);
kal_uint32 jpeg_drv_enc_warm_reset(void);
void jpeg_drv_enc_start(void);
kal_uint32 jpeg_drv_enc_set_quality(kal_uint32 quality);
kal_uint32 jpeg_drv_enc_set_img_size(kal_uint32 width, kal_uint32 height);
kal_uint32 jpeg_drv_enc_set_blk_num(kal_uint32 blk_num);
kal_uint32 jpeg_drv_enc_set_luma_addr(kal_uint32 src_luma_addr);
kal_uint32 jpeg_drv_enc_set_chroma_addr(kal_uint32 src_luma_addr);
kal_uint32 jpeg_drv_enc_set_memory_stride(kal_uint32 mem_stride);
kal_uint32 jpeg_drv_enc_set_image_stride(kal_uint32 img_stride);
void jpeg_drv_enc_set_restart_interval(kal_uint32 restart_interval);
kal_uint32 jpeg_drv_enc_set_offset_addr(kal_uint32 offset);
void jpeg_drv_enc_set_EncodeMode(kal_uint32 exif_en);	/* 0:JPG mode, 1:JFIF/EXIF mode */
void jpeg_drv_enc_set_burst_type(kal_uint32 burst_type);
kal_uint32 jpeg_drv_enc_set_dst_buff(kal_uint32 dst_addr, kal_uint32 stall_size,
				     kal_uint32 init_offset, kal_uint32 offset_mask);
kal_uint32 jpeg_drv_enc_set_sample_format_related(kal_uint32 width, kal_uint32 height,
						  kal_uint32 yuv_format);
kal_uint32 jpeg_drv_enc_get_file_size(void);
kal_uint32 jpeg_drv_enc_get_result(kal_uint32 *fileSize);
kal_uint32 jpeg_drv_enc_get_cycle_count(void);

void jpeg_drv_enc_dump_reg(void);

kal_uint32 jpeg_drv_enc_rw_reg(void);


int jpeg_isr_enc_lisr(void);


kal_uint32 jpeg_drv_enc_set_src_image(kal_uint32 width, kal_uint32 height, kal_uint32 yuv_format,
				      kal_uint32 totalEncDU);
kal_uint32 jpeg_drv_enc_set_src_buf(kal_uint32 yuv_format, kal_uint32 img_stride,
				    kal_uint32 mem_stride, kal_uint32 srcAddr,
				    kal_uint32 srcAddr_C);
kal_uint32 jpeg_drv_enc_set_encFormat(kal_uint32 encFormat);


#endif
