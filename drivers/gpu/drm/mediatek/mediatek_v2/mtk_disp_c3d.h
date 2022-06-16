/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_C3D_H__
#define __MTK_DISP_C3D_H__

#include <linux/uaccess.h>
#include <drm/mediatek_drm.h>

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_lowpower.h"
#include "mtk_log.h"
#include "mtk_dump.h"

#define C3D_BYPASS_SHADOW BIT(0)

#define C3D_U32_PTR(x) ((unsigned int *)(unsigned long)(x))
#define c3d_min(a, b)  (((a) < (b)) ? (a) : (b))

/*******************************/
/* field definition */
/* ------------------------------------------------------------- */
#define C3D_EN                             (0x000)
#define C3D_CFG                            (0x004)
#define C3D_INTEN                          (0x00C)
#define C3D_INTSTA                         (0x010)
#define C3D_SIZE                           (0x024)
#define C3D_SHADOW_CTL                     (0x030)
#define C3D_C1D_000_001                    (0x034)
#define C3D_C1D_002_003                    (0x038)
#define C3D_C1D_004_005                    (0x03C)
#define C3D_C1D_006_007                    (0x040)
#define C3D_C1D_008_009                    (0x044)
#define C3D_C1D_010_011                    (0x048)
#define C3D_C1D_012_013                    (0x04C)
#define C3D_C1D_014_015                    (0x050)
#define C3D_C1D_016_017                    (0x054)
#define C3D_C1D_018_019                    (0x058)
#define C3D_C1D_020_021                    (0x05C)
#define C3D_C1D_022_023                    (0x060)
#define C3D_C1D_024_025                    (0x064)
#define C3D_C1D_026_027                    (0x068)
#define C3D_C1D_028_029                    (0x06C)
#define C3D_C1D_030_031                    (0x070)
#define C3D_SRAM_CFG                       (0x074)
#define C3D_SRAM_STATUS                    (0x078)
#define C3D_SRAM_RW_IF_0                   (0x07C)
#define C3D_SRAM_RW_IF_1                   (0x080)
#define C3D_SRAM_RW_IF_2                   (0x084)
#define C3D_SRAM_RW_IF_3                   (0x088)
#define C3D_SRAM_PINGPONG                  (0x08C)


int mtk_drm_ioctl_c3d_get_bin_num(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

int mtk_drm_ioctl_c3d_get_irq(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

int mtk_drm_ioctl_c3d_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

int mtk_drm_ioctl_c3d_get_irq_status(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

int mtk_drm_ioctl_c3d_set_lut(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

int mtk_drm_ioctl_bypass_c3d(struct drm_device *dev, void *data,
			struct drm_file *file_priv);

void disp_c3d_on_start_of_frame(void);
void disp_c3d_on_end_of_frame_mutex(void);
void mtk_disp_c3d_debug(const char *opt);
void disp_c3d_set_bypass(struct drm_crtc *crtc, int bypass);


#endif

