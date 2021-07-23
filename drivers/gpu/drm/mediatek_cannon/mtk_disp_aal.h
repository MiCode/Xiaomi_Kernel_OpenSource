/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef __MTK_DISP_AAL_H__
#define __MTK_DISP_AAL_H__

#include <linux/uaccess.h>
#include <drm/mediatek_drm.h>

#define AAL_SERVICE_FORCE_UPDATE 0x1
/*******************************/
/* field definition */
/* ------------------------------------------------------------- */
/* AAL */
#define DISP_AAL_EN                             (0x000)
#define DISP_AAL_RESET                          (0x004)
#define DISP_AAL_INTEN                          (0x008)
#define DISP_AAL_INTSTA                         (0x00c)
#define DISP_AAL_STATUS                         (0x010)
#define DISP_AAL_CFG                            (0x020)
#define DISP_AAL_IN_CNT                         (0x024)
#define DISP_AAL_OUT_CNT                        (0x028)
#define DISP_AAL_CHKSUM                         (0x02c)
#define DISP_AAL_SIZE                           (0x030)
#define DISP_AAL_SHADOW_CTL                     (0x0B0)
#define DISP_AAL_DUMMY_REG                      (0x0c0)
#define DISP_AAL_SHADOW_CTRL                    (0x0f0)
#define AAL_BYPASS_SHADOW	BIT(0)
#define AAL_READ_WRK_REG	BIT(2)
#define DISP_AAL_MAX_HIST_CONFIG_00             (0x204)
#define DISP_AAL_CABC_00                        (0x20c)
#define DISP_AAL_CABC_02                        (0x214)
#define DISP_AAL_CABC_04                        (0x21c)
#define DISP_AAL_STATUS_00                      (0x224)
/* 00 ~ 32: max histogram */
#define DISP_AAL_STATUS_32                      (0x2a4)
/* bit 8: dre_gain_force_en */
#define DISP_AAL_DRE_GAIN_FILTER_00             (0x354)
#define DISP_AAL_DRE_FLT_FORCE(idx) \
	(0x358 + (idx) * 4)
#define DISP_AAL_DRE_CRV_CAL_00                 (0x344)
#define DISP_AAL_DRE_MAPPING_00                 (0x3b4)
#define DISP_AAL_CABC_GAINLMT_TBL(idx) \
	(0x410 + (idx) * 4)

#define DISP_AAL_DBG_CFG_MAIN                   (0x45c)

/* DRE 3.0 */
#define DISP_AAL_CFG_MAIN                       (0x200)
#define DISP_AAL_SRAM_CFG                       (0x0c4)
#define DISP_AAL_SRAM_STATUS                    (0x0c8)
#define DISP_AAL_SRAM_RW_IF_0                   (0x0cc)
#define DISP_AAL_SRAM_RW_IF_1                   (0x0d0)
#define DISP_AAL_SRAM_RW_IF_2                   (0x0d4)
#define DISP_AAL_SRAM_RW_IF_3                   (0x0d8)
#define DISP_AAL_WIN_X_MAIN                     (0x460)
#define DISP_AAL_WIN_Y_MAIN                     (0x464)
#define DISP_AAL_DRE_BLOCK_INFO_00              (0x468)
#define DISP_AAL_DRE_BLOCK_INFO_01              (0x46c)
#define DISP_AAL_DRE_BLOCK_INFO_02              (0x470)
#define DISP_AAL_DRE_BLOCK_INFO_03              (0x474)
#define DISP_AAL_DRE_BLOCK_INFO_04              (0x478)
#define DISP_AAL_DRE_CHROMA_HIST_00             (0x480)
#define DISP_AAL_DRE_CHROMA_HIST_01             (0x484)
#define DISP_AAL_DRE_ALPHA_BLEND_00             (0x488)
#define DISP_AAL_DRE_BITPLUS_00                 (0x48c)
#define DISP_AAL_DRE_BITPLUS_01                 (0x490)
#define DISP_AAL_DRE_BITPLUS_02                 (0x494)
#define DISP_AAL_DRE_BITPLUS_03                 (0x498)
#define DISP_AAL_DRE_BITPLUS_04                 (0x49c)
#define DISP_AAL_DRE_BLOCK_INFO_05              (0x4b4)
#define DISP_AAL_DRE_BLOCK_INFO_06              (0x4b8)
#define DISP_AAL_DUAL_PIPE_INFO_00              (0x4d0)
#define DISP_AAL_DUAL_PIPE_INFO_01              (0x4d4)
#define DISP_AAL_OUTPUT_SIZE                    (0x4d8)
#define DISP_AAL_OUTPUT_OFFSET                  (0x4dc)

#define DISP_Y_HISTOGRAM_00                     (0x504)

#define DISP_AAL_DRE_BLOCK_INFO_07              (0x0f8)
#define MDP_AAL_TILE_00				(0x4EC)
#define MDP_AAL_TILE_01				(0x4F0)
#define MDP_AAL_TILE_02				(0x0F4)

#define AAL_DRE30_GAIN_REGISTER_NUM		(544)
#define AAL_DRE30_HIST_REGISTER_NUM		(768)

#define AAL_U32_PTR(x) ((unsigned int *)(unsigned long)x)
#define aal_u32_handle_t unsigned long long

enum AAL_ESS_UD_MODE {
	CONFIG_BY_CUSTOM_LIB = 0,
	CONFIG_TO_LCD = 1,
	CONFIG_TO_AMOLED = 2
};

enum AAL_DRE_MODE {
	DRE_EN_BY_CUSTOM_LIB = 0xFFFF,
	DRE_OFF = 0,
	DRE_ON = 1
};

enum AAL_ESS_MODE {
	ESS_EN_BY_CUSTOM_LIB = 0xFFFF,
	ESS_OFF = 0,
	ESS_ON = 1
};

enum AAL_ESS_LEVEL {
	ESS_LEVEL_BY_CUSTOM_LIB = 0xFFFF
};

enum DISP_AAL_REFRESH_LATENCY {
	AAL_REFRESH_17MS = 17,
	AAL_REFRESH_33MS = 33
};

struct DISP_DRE30_HIST {
	unsigned int dre_hist[AAL_DRE30_HIST_REGISTER_NUM];
	int dre_blk_x_num;
	int dre_blk_y_num;
};

struct DISP_DRE30_PARAM {
	unsigned int dre30_gain[AAL_DRE30_GAIN_REGISTER_NUM];
};

void disp_aal_debug(const char *opt);

/* Provide for LED */
void disp_aal_notify_backlight_changed(int bl_1024);

void disp_aal_on_start_of_frame(void);

/* AAL Control API in Kernel */
void disp_aal_set_lcm_type(unsigned int panel_type);
void disp_aal_set_ess_level(int level);
void disp_aal_set_ess_en(int enable);
void disp_aal_set_dre_en(int enable);

int mtk_drm_ioctl_aal_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aal_get_hist(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aal_init_reg(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aal_init_dre30(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aal_get_size(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_aal_set_param(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

#endif

