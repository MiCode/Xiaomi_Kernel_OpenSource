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
*
* You should have received a copy of the GNU General Public License
* along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/******************************************************************************
*
 *
 * Filename:
 * ---------
 *   mt_soc_pcm_platform
 *
 * Project:
 * --------
 *   mt_soc_pcm_platform function
 *
 * Description:
 * ------------
 *   Common function
 *
 * Author:
 * -------
 *   Chipeng Chang (MTK02308)
 *
 *---------------------------------------------------------------------------
---
 *

*******************************************************************************/

#ifndef AUDIO_MT6797_SOUND_H
#define AUDIO_MT6797_SOUND_H

#include "mtk-auddrv-common.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-pcm-platform.h"
#include "mtk-auddrv-common-func.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/wakelock.h>
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <asm/div64.h>
#include <mt-plat/aee.h>
#if !defined(CONFIG_FPGA_EARLY_PORTING)
#include <mt-plat/upmu_common.h>
#endif
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>

/* #define EFUSE_HP_TRIM */
#define CHIP_SRAM_SIZE (36*1024)

/*
  *    PCM buffer size and period size setting
  */
#define BT_DAI_MAX_BUFFER_SIZE     (16*1024)
#define BT_DAI_MIN_PERIOD_SIZE     1
#define BT_DAI_MAX_PERIOD_SIZE     BT_DAI_MAX_BUFFER_SIZE

#define Dl1_MAX_BUFFER_SIZE     (36*1024)
#define Dl1_MIN_PERIOD_SIZE       1
#define Dl1_MAX_PERIOD_SIZE     Dl1_MAX_BUFFER_SIZE

#define Dl2_MAX_BUFFER_SIZE     (48*1024)
#define Dl2_MIN_PERIOD_SIZE       1
#define Dl2_MAX_PERIOD_SIZE     Dl2_MAX_BUFFER_SIZE

#define Dl3_MAX_BUFFER_SIZE     (32*1024)
#define Dl3_MIN_PERIOD_SIZE       1
#define Dl3_MAX_PERIOD_SIZE     Dl3_MAX_BUFFER_SIZE

#define MAX_BUFFER_SIZE     (32*1024)
#define MIN_PERIOD_SIZE       1
#define MAX_PERIOD_SIZE     MAX_BUFFER_SIZE

#define UL1_MAX_BUFFER_SIZE     (48*1024)
#define UL1_MIN_PERIOD_SIZE       1
#define UL1_MAX_PERIOD_SIZE     UL1_MAX_BUFFER_SIZE

#define UL2_MAX_BUFFER_SIZE     (64*1024)
#define UL2_MIN_PERIOD_SIZE       1
#define UL2_MAX_PERIOD_SIZE     UL2_MAX_BUFFER_SIZE

#define AWB_MAX_BUFFER_SIZE     (64*1024)
#define AWB_MIN_PERIOD_SIZE       1
#define AWB_MAX_PERIOD_SIZE     AWB_MAX_BUFFER_SIZE

#define MOD_DAI_MAX_BUFFER_SIZE     (16*1024)
#define MOD_DAI_MIN_PERIOD_SIZE       (1)
#define MOD_DAI_MAX_PERIOD_SIZE     MOD_DAI_MAX_BUFFER_SIZE

#define HDMI_MAX_BUFFER_SIZE     (384*1024)
#define HDMI_MIN_PERIOD_SIZE       1
#define HDMI_MAX_PERIODBYTE_SIZE     HDMI_MAX_BUFFER_SIZE
#define HDMI_MAX_2CH_16BIT_PERIOD_SIZE     (HDMI_MAX_PERIODBYTE_SIZE/(2*2)) /* 2 channels , 16bits */
#define HDMI_MAX_8CH_16BIT_PERIOD_SIZE     (HDMI_MAX_PERIODBYTE_SIZE/(8*2)) /* 8 channels , 16bits */
#define HDMI_MAX_2CH_24BIT_PERIOD_SIZE     (HDMI_MAX_PERIODBYTE_SIZE/(2*2*2)) /* 2 channels , 24bits */
#define HDMI_MAX_8CH_24BIT_PERIOD_SIZE     (HDMI_MAX_PERIODBYTE_SIZE/(8*2*2)) /* 8 channels , 24bits */

#define MRGRX_MAX_BUFFER_SIZE     (64*1024)
#define MRGRX_MIN_PERIOD_SIZE       1
#define MRGRX_MAX_PERIOD_SIZE     MRGRX_MAX_BUFFER_SIZE

#define FM_I2S_MAX_BUFFER_SIZE     (64*1024)
#define FM_I2S_MIN_PERIOD_SIZE       1
#define FM_I2S_MAX_PERIOD_SIZE     MRGRX_MAX_BUFFER_SIZE

#define AUDIO_SRAM_PLAYBACK_FULL_SIZE	Dl1_MAX_BUFFER_SIZE
#define AUDIO_SRAM_PLAYBACK_PARTIAL_SIZE	Dl1_MAX_BUFFER_SIZE
#define AUDIO_DRAM_PLAYBACK_SIZE	(1024 * 40)

#define AUDIO_SRAM_CAPTURE_SIZE	(1024 * 32)
#define AUDIO_DRAM_CAPTURE_SIZE	(1024 * 32)
#endif
