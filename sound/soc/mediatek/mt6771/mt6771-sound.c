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

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "mtk-auddrv-common.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-pcm-platform.h"
#include "mtk-soc-digital-type.h"
#include "mtk-soc-codec-63xx.h"
#include "mtk-soc-analog-type.h"

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
#include <linux/semaphore.h>
#include <linux/jiffies.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <asm/div64.h>

#if defined(CONFIG_MTK_PASR)
#include <mt-plat/mtk_lpae.h>
#else
#define enable_4G() (false)
#endif

static const uint16_t kSideToneCoefficientTable16k[] = {
	0x049C, 0x09E8, 0x09E0, 0x089C,
	0xFF54, 0xF488, 0xEAFC, 0xEBAC,
	0xfA40, 0x17AC, 0x3D1C, 0x6028,
	0x7538
};


static const uint16_t kSideToneCoefficientTable32k[] = {
	0xFE52, 0x0042, 0x00C5, 0x0194,
	0x029A, 0x03B7, 0x04BF, 0x057D,
	0x05BE, 0x0555, 0x0426, 0x0230,
	0xFF92, 0xFC89, 0xF973, 0xF6C6,
	0xF500, 0xF49D, 0xF603, 0xF970,
	0xFEF3, 0x065F, 0x0F4F, 0x1928,
	0x2329, 0x2C80, 0x345E, 0x3A0D,
	0x3D08
};

static const uint16_t kSideToneCoefficientTable48k[] = {
	0x0401, 0xFFB0, 0xFF5A, 0xFECE,
	0xFE10, 0xFD28, 0xFC21, 0xFB08,
	0xF9EF, 0xF8E8, 0xF80A, 0xF76C,
	0xF724, 0xF746, 0xF7E6, 0xF90F,
	0xFACC, 0xFD1E, 0xFFFF, 0x0364,
	0x0737, 0x0B62, 0x0FC1, 0x1431,
	0x188A, 0x1CA4, 0x2056, 0x237D,
	0x25F9, 0x27B0, 0x2890
};

static const unsigned int mMemIfSampleRate[Soc_Aud_Digital_Block_MEM_I2S + 1][3]
= { /* reg, bit position, bit mask */
	[Soc_Aud_Digital_Block_MEM_DL1] = {AFE_DAC_CON1, 0, 0xf},
	[Soc_Aud_Digital_Block_MEM_DL2] = {AFE_DAC_CON1, 4, 0xf},
	[Soc_Aud_Digital_Block_MEM_VUL] = {AFE_DAC_CON1, 16, 0xf},
	[Soc_Aud_Digital_Block_MEM_DAI] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_DL3] = {AFE_DAC_CON2, 8, 0xf},
	[Soc_Aud_Digital_Block_MEM_AWB] = {AFE_DAC_CON1, 12, 0xf},
	[Soc_Aud_Digital_Block_MEM_MOD_DAI] = {AFE_DAC_CON1, 30, 0x3},
	[Soc_Aud_Digital_Block_MEM_DL1_DATA2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_VUL_DATA2] = {AFE_DAC_CON0, 20, 0xf},
	[Soc_Aud_Digital_Block_MEM_VUL2] = {AFE_DAC_CON2, 4, 0xf},
	[Soc_Aud_Digital_Block_MEM_DAI2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_AWB2] = {AFE_DAC_CON2, 16, 0xf},
	[Soc_Aud_Digital_Block_MEM_I2S] = {AFE_DAC_CON1, 8, 0xf},
};

static const unsigned int mMemIfChannels[Soc_Aud_Digital_Block_MEM_I2S + 1][3]
= { /* reg, bit position, bit mask */
	[Soc_Aud_Digital_Block_MEM_DL1] = {AFE_DAC_CON1, 21, 0x1},
	[Soc_Aud_Digital_Block_MEM_DL2] = {AFE_DAC_CON1, 22, 0x1},
	[Soc_Aud_Digital_Block_MEM_VUL] = {AFE_DAC_CON1, 27, 0x1},
	[Soc_Aud_Digital_Block_MEM_DAI] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_DL3] = {AFE_DAC_CON1, 23, 0x1},
	[Soc_Aud_Digital_Block_MEM_AWB] = {AFE_DAC_CON1, 24, 0x1},
	[Soc_Aud_Digital_Block_MEM_MOD_DAI] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_DL1_DATA2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_VUL_DATA2] = {AFE_DAC_CON0, 10, 0x1},
	[Soc_Aud_Digital_Block_MEM_VUL2] = {AFE_DAC_CON2, 0, 0x1},
	[Soc_Aud_Digital_Block_MEM_DAI2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_AWB2] = {AFE_DAC_CON2, 20, 0x1},
	[Soc_Aud_Digital_Block_MEM_I2S] = {AFE_REG_UNDEFINED, 0, 0x0},
};

static const unsigned int mMemIfMonoChSelect[Soc_Aud_Digital_Block_MEM_I2S +
		1][3] = { /* reg, bit position, bit mask */
	[Soc_Aud_Digital_Block_MEM_DL1] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_DL2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_VUL] = {AFE_DAC_CON1, 28, 0x1},
	[Soc_Aud_Digital_Block_MEM_DAI] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_DL3] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_AWB] = {AFE_DAC_CON1, 25, 0x1},
	[Soc_Aud_Digital_Block_MEM_MOD_DAI] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_DL1_DATA2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_VUL_DATA2] = {AFE_DAC_CON0, 11, 0x1},
	[Soc_Aud_Digital_Block_MEM_VUL2] = {AFE_DAC_CON2, 1, 0x1},
	[Soc_Aud_Digital_Block_MEM_DAI2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_AWB2] = {AFE_DAC_CON2, 21, 0x1},
	[Soc_Aud_Digital_Block_MEM_I2S] = {AFE_REG_UNDEFINED, 0, 0x0},
};

static const unsigned int mMemDuplicateWrite[Soc_Aud_Digital_Block_MEM_I2S +
		1][3] = { /* reg, bit position, bit mask */
	[Soc_Aud_Digital_Block_MEM_DL1] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_DL2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_VUL] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_DAI] = {AFE_REG_UNDEFINED, 0x0, 0x0},
	[Soc_Aud_Digital_Block_MEM_DL3] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_AWB] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_MOD_DAI] = {AFE_DAC_CON0, 26, 0x1},
	[Soc_Aud_Digital_Block_MEM_DL1_DATA2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_VUL_DATA2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_VUL2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_DAI2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_AWB2] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_Digital_Block_MEM_I2S] = {AFE_REG_UNDEFINED, 0, 0x0},
};

static const unsigned int
mMemAudioBlockEnableReg[][MEM_EN_REG_IDX_NUM] = {
	{Soc_Aud_Digital_Block_MEM_DL1, AFE_DAC_CON0, 1},
	{Soc_Aud_Digital_Block_MEM_DL2, AFE_DAC_CON0, 2},
	{Soc_Aud_Digital_Block_MEM_VUL, AFE_DAC_CON0, 3},
	{Soc_Aud_Digital_Block_MEM_DAI, AFE_REG_UNDEFINED, 0},
	{Soc_Aud_Digital_Block_MEM_DL3, AFE_DAC_CON0, 5},
	{Soc_Aud_Digital_Block_MEM_AWB, AFE_DAC_CON0, 6},
	{Soc_Aud_Digital_Block_MEM_MOD_DAI, AFE_DAC_CON0, 7},
	{Soc_Aud_Digital_Block_MEM_VUL_DATA2, AFE_DAC_CON0, 9},
	{Soc_Aud_Digital_Block_MEM_VUL2, AFE_DAC_CON0, 27},
	{Soc_Aud_Digital_Block_MEM_AWB2, AFE_DAC_CON0, 29},
};

const struct Aud_IRQ_CTRL_REG mIRQCtrlRegs[Soc_Aud_IRQ_MCU_MODE_NUM] = {
	{	/*IRQ0*/
		{AFE_IRQ_MCU_CON0, 0, 0x1}, /* irq on */
		{AFE_IRQ_MCU_CON1, 0, 0xf}, /* irq mode */
		{AFE_IRQ_MCU_CNT0, 0, 0x3ffff}, /* irq count */
		{AFE_IRQ_MCU_CLR, 0, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 16, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 0, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 0, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ1*/
		{AFE_IRQ_MCU_CON0, 1, 0x1}, /* irq on */
		{AFE_IRQ_MCU_CON1, 4, 0xf}, /* irq mode */
		{AFE_IRQ_MCU_CNT1, 0, 0x3ffff}, /* irq count */
		{AFE_IRQ_MCU_CLR, 1, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 17, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 1, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 1, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ2*/
		{AFE_IRQ_MCU_CON0, 2, 0x1}, /* irq on */
		{AFE_IRQ_MCU_CON1, 8, 0xf}, /* irq mode */
		{AFE_IRQ_MCU_CNT2, 0, 0x3ffff}, /* irq count */
		{AFE_IRQ_MCU_CLR, 2, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 18, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 2, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 2, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ3*/
		{AFE_IRQ_MCU_CON0, 3, 0x1}, /* irq on */
		{AFE_IRQ_MCU_CON1, 12, 0xf}, /* irq mode */
		{AFE_IRQ_MCU_CNT3, 0, 0x3ffff}, /* irq count */
		{AFE_IRQ_MCU_CLR, 3, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 19, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 3, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 3, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ4*/
		{AFE_IRQ_MCU_CON0, 4, 0x1}, /* irq on */
		{AFE_IRQ_MCU_CON1, 16, 0xf}, /* irq mode */
		{AFE_IRQ_MCU_CNT4, 0, 0x3ffff}, /* irq count */
		{AFE_IRQ_MCU_CLR, 4, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 20, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 4, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 4, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ5*/
		{AFE_IRQ_MCU_CON0, 5, 0x1}, /* irq on */
		{AFE_IRQ_MCU_CON1, 20, 0xf}, /* irq mode */
		{AFE_IRQ_MCU_CNT5, 0, 0x3ffff}, /* irq count */
		{AFE_IRQ_MCU_CLR, 5, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 21, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 5, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 5, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ6*/
		{AFE_IRQ_MCU_CON0, 6, 0x1}, /* irq on */
		{AFE_IRQ_MCU_CON1, 24, 0xf}, /* irq mode */
		{AFE_IRQ_MCU_CNT6, 0, 0x3ffff}, /* irq count */
		{AFE_IRQ_MCU_CLR, 6, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 22, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 6, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 6, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ7*/
		{AFE_IRQ_MCU_CON0, 7, 0x1}, /* irq on */
		{AFE_IRQ_MCU_CON1, 28, 0xf}, /* irq mode */
		{AFE_IRQ_MCU_CNT7, 0, 0x3ffff}, /* irq count */
		{AFE_IRQ_MCU_CLR, 7, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 23, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 7, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 7, 0x1}, /* irq enable */
		Soc_Aud_IRQ_CM4 /* irq use for specify purpose */
	},
	{	/*IRQ8*/
		{AFE_IRQ_MCU_CON0, 8, 0x1}, /* irq on */
		{AFE_REG_UNDEFINED, 0, 0x0}, /* irq mode */
		{AFE_REG_UNDEFINED, 0, 0x0}, /* irq count */
		{AFE_IRQ_MCU_CLR, 8, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 24, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 8, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 8, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ9*/
		{AFE_IRQ_MCU_CON0, 9, 0x1}, /* irq on */
		{AFE_REG_UNDEFINED, 0, 0x0}, /* irq mode */
		{AFE_REG_UNDEFINED, 0, 0x0}, /* irq count */
		{AFE_IRQ_MCU_CLR, 9, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 25, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 9, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 9, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ10*/
		{AFE_IRQ_MCU_CON0, 10, 0x1}, /* irq on */
		{AFE_REG_UNDEFINED, 0, 0x0}, /* irq mode */
		{AFE_REG_UNDEFINED, 0, 0x0}, /* irq count */
		{AFE_IRQ_MCU_CLR, 10, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 26, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 10, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 10, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ11*/
		{AFE_IRQ_MCU_CON0, 11, 0x1}, /* irq on */
		{AFE_IRQ_MCU_CON2, 0, 0xf}, /* irq mode */
		{AFE_IRQ_MCU_CNT11, 0, 0x3ffff}, /* irq count */
		{AFE_IRQ_MCU_CLR, 11, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 27, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 11, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 11, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ12*/
		{AFE_IRQ_MCU_CON0, 12, 0x1}, /* irq on */
		{AFE_IRQ_MCU_CON2, 4, 0xf}, /* irq mode */
		{AFE_IRQ_MCU_CNT12, 0, 0x3ffff}, /* irq count */
		{AFE_IRQ_MCU_CLR, 12, 0x1}, /* irq clear */
		{AFE_IRQ_MCU_CLR, 28, 0x1}, /* irq miss clear */
		{AFE_IRQ_MCU_STATUS, 12, 0x1}, /* irq status */
		{AFE_IRQ_MCU_EN, 12, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ_ACC1*/
		{AFE_REG_UNDEFINED, 13, 0x1}, /* irq on */
		{AFE_REG_UNDEFINED, 8, 0x0}, /* irq mode */
		{AFE_REG_UNDEFINED, 0, 0x0}, /* irq count */
		{AFE_REG_UNDEFINED, 13, 0x1}, /* irq clear */
		{AFE_REG_UNDEFINED, 29, 0x1}, /* irq miss clear */
		{AFE_REG_UNDEFINED, 13, 0x1}, /* irq status */
		{AFE_REG_UNDEFINED, 13, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
	{	/*IRQ_ACC2*/
		{AFE_REG_UNDEFINED, 14, 0x1}, /* irq on */
		{AFE_REG_UNDEFINED, 12, 0x0}, /* irq mode */
		{AFE_REG_UNDEFINED, 0, 0x0}, /* irq count */
		{AFE_REG_UNDEFINED, 14, 0x1}, /* irq clear */
		{AFE_REG_UNDEFINED, 30, 0x1}, /* irq miss clear */
		{AFE_REG_UNDEFINED, 14, 0x1}, /* irq status */
		{AFE_REG_UNDEFINED, 14, 0x1}, /* irq enable */
		Soc_Aud_IRQ_MCU /* irq use for specify purpose */
	},
};

const struct Aud_RegBitsInfo mIRQPurposeRegs[Soc_Aud_IRQ_PURPOSE_NUM] = {
	[Soc_Aud_IRQ_MCU] = {AFE_IRQ_MCU_EN, 0, 0xffff},
	[Soc_Aud_IRQ_MD32] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_IRQ_MD32_H] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_IRQ_DSP] = {AFE_REG_UNDEFINED, 0, 0x0},
	[Soc_Aud_IRQ_CM4] = {AFE_IRQ_MCU_EN1, 16, 0xffff},
};

static const unsigned int
afe_buffer_regs[Soc_Aud_AFE_IO_Block_NUM_OF_IO_BLOCK][aud_buffer_ctrl_num] = {
	[Soc_Aud_AFE_IO_Block_MEM_DL1] = {
		AFE_DL1_BASE, AFE_DL1_END, AFE_DL1_CUR
	},
	[Soc_Aud_AFE_IO_Block_MEM_DL2] = {
		AFE_DL2_BASE, AFE_DL2_END, AFE_DL2_CUR
	},
	[Soc_Aud_AFE_IO_Block_MEM_DL3] = {
		AFE_DL3_BASE, AFE_DL3_END, AFE_DL3_CUR
	},
};

#define SRC_PARAM_INVALID (0)
#define ARRAYSIZE(array) (sizeof(array) / sizeof((array)[0]))
static const unsigned int
src_param[Soc_Aud_I2S_SAMPLERATE_I2S_NUM][AUDIO_SRC_PARAM_TYPE_NUM] = {
    /* FS = {freq_mode, autorst_threadhold_high, autorst_threadhold_low}*/
	[Soc_Aud_I2S_SAMPLERATE_I2S_8K] =  {0x00050000, 0x36000, 0x2FC00},
	[Soc_Aud_I2S_SAMPLERATE_I2S_11K] = {0x0006E400, 0x28800, 0x21000},
	[Soc_Aud_I2S_SAMPLERATE_I2S_12K] = {0x00078000, 0x24000, 0x20000},
	[Soc_Aud_I2S_SAMPLERATE_I2S_16K] = {0x000A0000, 0x1B000, 0x17E00},
	[Soc_Aud_I2S_SAMPLERATE_I2S_22K] = {0x000DC800, 0x14400, 0x10800},
	[Soc_Aud_I2S_SAMPLERATE_I2S_24K] = {0x000F0000, 0x12000, 0x10000},
	[Soc_Aud_I2S_SAMPLERATE_I2S_130K] = {
		SRC_PARAM_INVALID, SRC_PARAM_INVALID, SRC_PARAM_INVALID
	},
	[Soc_Aud_I2S_SAMPLERATE_I2S_32K] = {0x00140000, 0xD800, 0xBF00},
	[Soc_Aud_I2S_SAMPLERATE_I2S_44K] = {0x001B9000, 0xA200, 0x8400},
	[Soc_Aud_I2S_SAMPLERATE_I2S_48K] = {0x001E0000, 0x9000, 0x8000},
	[Soc_Aud_I2S_SAMPLERATE_I2S_88K] = {0x00372000, 0x5100, 0x4200},
	[Soc_Aud_I2S_SAMPLERATE_I2S_96K] = {0x003C0000, 0x4800, 0x4000},
	[Soc_Aud_I2S_SAMPLERATE_I2S_174K] = {0x006E4000, 0x2880, 0x2100},
	[Soc_Aud_I2S_SAMPLERATE_I2S_192K] = {0x00780000, 0x2400, 0x2000},
	[Soc_Aud_I2S_SAMPLERATE_I2S_260K] = {
		SRC_PARAM_INVALID, SRC_PARAM_INVALID, SRC_PARAM_INVALID
	},
};

static const unsigned int src_iir_coeff_32_to_16[] = {
	0x0dbae6, 0xff9b0a, 0x0dbae6, 0x05e488, 0xe072b9, 0x000002,
	0x0dbae6, 0x000f3b, 0x0dbae6, 0x06a537, 0xe17d79, 0x000002,
	0x0dbae6, 0x01246a, 0x0dbae6, 0x087261, 0xe306be, 0x000002,
	0x0dbae6, 0x03437d, 0x0dbae6, 0x0bc16f, 0xe57c87, 0x000002,
	0x0dbae6, 0x072981, 0x0dbae6, 0x111dd3, 0xe94f2a, 0x000002,
	0x0dbae6, 0x0dc4a6, 0x0dbae6, 0x188611, 0xee85a0, 0x000002,
	0x0dbae6, 0x168b9a, 0x0dbae6, 0x200e8f, 0xf3ccf1, 0x000002,
	0x000000, 0x1b75cb, 0x1b75cb, 0x2374a2, 0x000000, 0x000001
};

static const unsigned int src_iir_coeff_44_to_16[] = {
	0x09ae28, 0xf7d97d, 0x09ae28, 0x212a3d, 0xe0ac3a, 0x000002,
	0x09ae28, 0xf8525a, 0x09ae28, 0x216d72, 0xe234be, 0x000002,
	0x09ae28, 0xf980f5, 0x09ae28, 0x22a057, 0xe45a81, 0x000002,
	0x09ae28, 0xfc0a08, 0x09ae28, 0x24d3bd, 0xe7752d, 0x000002,
	0x09ae28, 0x016162, 0x09ae28, 0x27da01, 0xeb6ea8, 0x000002,
	0x09ae28, 0x0b67df, 0x09ae28, 0x2aca4a, 0xef34c4, 0x000002,
	0x000000, 0x135c50, 0x135c50, 0x2c1079, 0x000000, 0x000001
};

static const unsigned int src_iir_coeff_44_to_32[] = {
	0x096966, 0x0c4d35, 0x096966, 0xedee81, 0xf05070, 0x000003,
	0x12d2cc, 0x193910, 0x12d2cc, 0xddbf4f, 0xe21e1d, 0x000002,
	0x12d2cc, 0x1a9e60, 0x12d2cc, 0xe18916, 0xe470fd, 0x000002,
	0x12d2cc, 0x1d06e0, 0x12d2cc, 0xe8a4a6, 0xe87b24, 0x000002,
	0x12d2cc, 0x207578, 0x12d2cc, 0xf4fe62, 0xef5917, 0x000002,
	0x12d2cc, 0x24055f, 0x12d2cc, 0x05ee2b, 0xf8b502, 0x000002,
	0x000000, 0x25a599, 0x25a599, 0x0fabe2, 0x000000, 0x000001
};

static const unsigned int src_iir_coeff_48_to_16[] = {
	0x091009, 0xf68197, 0x091009, 0x26dd51, 0xe09fd1, 0x000002,
	0x091009, 0xf6e5b0, 0x091009, 0x26e8ad, 0xe20be1, 0x000002,
	0x091009, 0xf7e303, 0x091009, 0x27af51, 0xe407c2, 0x000002,
	0x091009, 0xfa0fc6, 0x091009, 0x29331a, 0xe6e0ac, 0x000002,
	0x091009, 0xfee432, 0x091009, 0x2b4be9, 0xea7ccb, 0x000002,
	0x091009, 0x08fc54, 0x091009, 0x2d52b2, 0xede0d8, 0x000002,
	0x000000, 0x122013, 0x122013, 0x2e3256, 0x000000, 0x000001
};

static const unsigned int src_iir_coeff_48_to_32[] = {
	0x10c1b8, 0x10a7df, 0x10c1b8, 0xe7514e, 0xe0b41f, 0x000002,
	0x10c1b8, 0x116257, 0x10c1b8, 0xe9402f, 0xe25aaa, 0x000002,
	0x10c1b8, 0x130c89, 0x10c1b8, 0xed3cc3, 0xe4dddb, 0x000002,
	0x10c1b8, 0x1600dd, 0x10c1b8, 0xf48000, 0xe90c55, 0x000002,
	0x10c1b8, 0x1a672e, 0x10c1b8, 0x00494c, 0xefa807, 0x000002,
	0x10c1b8, 0x1f38e6, 0x10c1b8, 0x0ee076, 0xf7c5f3, 0x000002,
	0x000000, 0x218370, 0x218370, 0x168b40, 0x000000, 0x000001
};

static const unsigned int src_iir_coeff_48_to_44[] = {
	0x0bf71c, 0x170f3f, 0x0bf71c, 0xe3a4c8, 0xf096cb, 0x000003,
	0x0bf71c, 0x17395e, 0x0bf71c, 0xe58085, 0xf210c8, 0x000003,
	0x0bf71c, 0x1782bd, 0x0bf71c, 0xe95ef6, 0xf4c899, 0x000003,
	0x0bf71c, 0x17cd97, 0x0bf71c, 0xf1608a, 0xfa3b18, 0x000003,
	0x000000, 0x2fdc6f, 0x2fdc6f, 0xf15663, 0x000000, 0x000001
};

static const unsigned int src_iir_coeff_96_to_16[] = {
	0x0805a1, 0xf21ae3, 0x0805a1, 0x3840bb, 0xe02a2e, 0x000002,
	0x0d5dd8, 0xe8f259, 0x0d5dd8, 0x1c0af6, 0xf04700, 0x000003,
	0x0bb422, 0xec08d9, 0x0bb422, 0x1bfccc, 0xf09216, 0x000003,
	0x08fde6, 0xf108be, 0x08fde6, 0x1bf096, 0xf10ae0, 0x000003,
	0x0ae311, 0xeeeda3, 0x0ae311, 0x37c646, 0xe385f5, 0x000002,
	0x044089, 0xfa7242, 0x044089, 0x37a785, 0xe56526, 0x000002,
	0x00c75c, 0xffb947, 0x00c75c, 0x378ba3, 0xe72c5f, 0x000002,
	0x000000, 0x0ef76e, 0x0ef76e, 0x377fda, 0x000000, 0x000001,
};

static const unsigned int src_iir_coeff_96_to_44[] = {
	0x08b543, 0xfd80f4, 0x08b543, 0x0e2332, 0xe06ed0, 0x000002,
	0x1b6038, 0xf90e7e, 0x1b6038, 0x0ec1ac, 0xe16f66, 0x000002,
	0x188478, 0xfbb921, 0x188478, 0x105859, 0xe2e596, 0x000002,
	0x13eff3, 0xffa707, 0x13eff3, 0x13455c, 0xe533b7, 0x000002,
	0x0dc239, 0x03d458, 0x0dc239, 0x17f120, 0xe8b617, 0x000002,
	0x0745f1, 0x05d790, 0x0745f1, 0x1e3d75, 0xed5f18, 0x000002,
	0x05641f, 0x085e2b, 0x05641f, 0x48efd0, 0xe3e9c8, 0x000001,
	0x000000, 0x28f632, 0x28f632, 0x273905, 0x000000, 0x000001,
};
/*  Above structures may vary with chips!!!! */

/* set address hardware , platform dependent*/
static int set_mem_blk_addr(int mem_blk, dma_addr_t addr, size_t size)
{
	pr_debug("%s mem_blk = %d\n", __func__, mem_blk);
	switch (mem_blk) {
	case Soc_Aud_Digital_Block_MEM_DL1:
		Afe_Set_Reg(AFE_DL1_BASE, addr, 0xffffffff);
		Afe_Set_Reg(AFE_DL1_END, addr + (size - 1), 0xffffffff);
		break;
	case Soc_Aud_Digital_Block_MEM_DL2:
		Afe_Set_Reg(AFE_DL2_BASE, addr, 0xffffffff);
		Afe_Set_Reg(AFE_DL2_END, addr + (size - 1), 0xffffffff);
		break;
	case Soc_Aud_Digital_Block_MEM_DL3:
		Afe_Set_Reg(AFE_DL3_BASE, addr, 0xffffffff);
		Afe_Set_Reg(AFE_DL3_END, addr + (size - 1), 0xffffffff);
		break;
	case Soc_Aud_Digital_Block_MEM_VUL:
		Afe_Set_Reg(AFE_VUL_BASE, addr, 0xffffffff);
		Afe_Set_Reg(AFE_VUL_END, addr + (size - 1), 0xffffffff);
		break;
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
		Afe_Set_Reg(AFE_VUL_D2_BASE, addr, 0xffffffff);
		Afe_Set_Reg(AFE_VUL_D2_END, addr + (size - 1), 0xffffffff);
		break;
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
		Afe_Set_Reg(AFE_MOD_DAI_BASE, addr, 0xffffffff);
		Afe_Set_Reg(AFE_MOD_DAI_END, addr + (size - 1), 0xffffffff);
		break;
	case Soc_Aud_Digital_Block_MEM_AWB2:
		Afe_Set_Reg(AFE_AWB2_BASE, addr, 0xffffffff);
		Afe_Set_Reg(AFE_AWB2_END, addr + (size - 1), 0xffffffff);
		break;
	case Soc_Aud_Digital_Block_MEM_AWB:
		Afe_Set_Reg(AFE_AWB_BASE, addr, 0xffffffff);
		Afe_Set_Reg(AFE_AWB_END, addr + (size - 1), 0xffffffff);
		break;
	case Soc_Aud_Digital_Block_MEM_VUL2:
		Afe_Set_Reg(AFE_VUL2_BASE, addr, 0xffffffff);
		Afe_Set_Reg(AFE_VUL2_END, addr + (size - 1), 0xffffffff);
		break;
	default:
		pr_warn("%s not suuport mem_blk = %d", __func__, mem_blk);
	}
	return 0;
}

static struct mtk_mem_blk_ops mem_blk_ops = {
	.set_chip_memif_addr = set_mem_blk_addr,
};

static const int MEM_BLOCK_ENABLE_REG_NUM = ARRAY_SIZE(mMemAudioBlockEnableReg);

void Afe_Log_Print(void)
{
	AudDrv_Clk_On();
	pr_debug("AUDIO_TOP_CON0 = 0x%x\n", Afe_Get_Reg(AUDIO_TOP_CON0));
	pr_debug("AUDIO_TOP_CON1 = 0x%x\n", Afe_Get_Reg(AUDIO_TOP_CON1));
	pr_debug("AUDIO_TOP_CON3 = 0x%x\n", Afe_Get_Reg(AUDIO_TOP_CON3));
	pr_debug("AFE_DAC_CON0 = 0x%x\n", Afe_Get_Reg(AFE_DAC_CON0));
	pr_debug("AFE_DAC_CON1 = 0x%x\n", Afe_Get_Reg(AFE_DAC_CON1));
	pr_debug("AFE_I2S_CON = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON));
	pr_debug("AFE_DAIBT_CON0 = 0x%x\n", Afe_Get_Reg(AFE_DAIBT_CON0));
	pr_debug("AFE_CONN0 = 0x%x\n", Afe_Get_Reg(AFE_CONN0));
	pr_debug("AFE_CONN1 = 0x%x\n", Afe_Get_Reg(AFE_CONN1));
	pr_debug("AFE_CONN2 = 0x%x\n", Afe_Get_Reg(AFE_CONN2));
	pr_debug("AFE_CONN3 = 0x%x\n", Afe_Get_Reg(AFE_CONN3));
	pr_debug("AFE_CONN4 = 0x%x\n", Afe_Get_Reg(AFE_CONN4));
	pr_debug("AFE_I2S_CON1 = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON1));
	pr_debug("AFE_I2S_CON2 = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON2));
	pr_debug("AFE_MRGIF_CON = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_CON));
	pr_debug("AFE_DL1_BASE = 0x%x\n", Afe_Get_Reg(AFE_DL1_BASE));
	pr_debug("AFE_DL1_CUR = 0x%x\n", Afe_Get_Reg(AFE_DL1_CUR));
	pr_debug("AFE_DL1_END = 0x%x\n", Afe_Get_Reg(AFE_DL1_END));
	pr_debug("AFE_I2S_CON3 = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON3));
	pr_debug("AFE_DL2_BASE = 0x%x\n", Afe_Get_Reg(AFE_DL2_BASE));
	pr_debug("AFE_DL2_CUR = 0x%x\n", Afe_Get_Reg(AFE_DL2_CUR));
	pr_debug("AFE_DL2_END = 0x%x\n", Afe_Get_Reg(AFE_DL2_END));
	pr_debug("AFE_CONN5 = 0x%x\n", Afe_Get_Reg(AFE_CONN5));
	pr_debug("AFE_CONN_24BIT = 0x%x\n", Afe_Get_Reg(AFE_CONN_24BIT));
	pr_debug("AFE_AWB_BASE = 0x%x\n", Afe_Get_Reg(AFE_AWB_BASE));
	pr_debug("AFE_AWB_END = 0x%x\n", Afe_Get_Reg(AFE_AWB_END));
	pr_debug("AFE_AWB_CUR = 0x%x\n", Afe_Get_Reg(AFE_AWB_CUR));
	pr_debug("AFE_VUL_BASE = 0x%x\n", Afe_Get_Reg(AFE_VUL_BASE));
	pr_debug("AFE_VUL_END = 0x%x\n", Afe_Get_Reg(AFE_VUL_END));
	pr_debug("AFE_VUL_CUR = 0x%x\n", Afe_Get_Reg(AFE_VUL_CUR));
	pr_debug("AFE_CONN6 = 0x%x\n", Afe_Get_Reg(AFE_CONN6));
	pr_debug("AFE_MEMIF_MSB = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MSB));
	pr_debug("AFE_MEMIF_MON0 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON0));
	pr_debug("AFE_MEMIF_MON1 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON1));
	pr_debug("AFE_MEMIF_MON2 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON2));
	pr_debug("AFE_MEMIF_MON3 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON3));
	pr_debug("AFE_MEMIF_MON4 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON4));
	pr_debug("AFE_MEMIF_MON5 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON5));
	pr_debug("AFE_MEMIF_MON6 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON6));
	pr_debug("AFE_MEMIF_MON7 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON7));
	pr_debug("AFE_MEMIF_MON8 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON8));
	pr_debug("AFE_MEMIF_MON9 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON9));
	pr_debug("AFE_ADDA_DL_SRC2_CON0 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON0));
	pr_debug("AFE_ADDA_DL_SRC2_CON1 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON1));
	pr_debug("AFE_ADDA_UL_SRC_CON0 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_UL_SRC_CON0));
	pr_debug("AFE_ADDA_UL_SRC_CON1 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_UL_SRC_CON1));
	pr_debug("AFE_ADDA_TOP_CON0 = 0x%x\n", Afe_Get_Reg(AFE_ADDA_TOP_CON0));
	pr_debug("AFE_ADDA_UL_DL_CON0 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_UL_DL_CON0));
	pr_debug("AFE_ADDA_SRC_DEBUG = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_SRC_DEBUG));
	pr_debug("AFE_ADDA_SRC_DEBUG_MON0 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON0));
	pr_debug("AFE_ADDA_SRC_DEBUG_MON1 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON1));
	pr_debug("AFE_ADDA_UL_SRC_MON0 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_UL_SRC_MON0));
	pr_debug("AFE_ADDA_UL_SRC_MON1 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_UL_SRC_MON1));
	pr_debug("AFE_SIDETONE_DEBUG = 0x%x\n",
		Afe_Get_Reg(AFE_SIDETONE_DEBUG));
	pr_debug("AFE_SIDETONE_MON = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_MON));
	pr_debug("AFE_SGEN_CON2 = 0x%x\n", Afe_Get_Reg(AFE_SGEN_CON2));
	pr_debug("AFE_SIDETONE_CON0 = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_CON0));
	pr_debug("AFE_SIDETONE_COEFF = 0x%x\n",
		Afe_Get_Reg(AFE_SIDETONE_COEFF));
	pr_debug("AFE_SIDETONE_CON1 = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_CON1));
	pr_debug("AFE_SIDETONE_GAIN = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_GAIN));
	pr_debug("AFE_SGEN_CON0 = 0x%x\n", Afe_Get_Reg(AFE_SGEN_CON0));
	pr_debug("AFE_TOP_CON0 = 0x%x\n", Afe_Get_Reg(AFE_TOP_CON0));
	pr_debug("AFE_BUS_CFG = 0x%x\n", Afe_Get_Reg(AFE_BUS_CFG));
	pr_debug("AFE_BUS_MON0 = 0x%x\n", Afe_Get_Reg(AFE_BUS_MON0));
	pr_debug("AFE_ADDA_PREDIS_CON0 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_PREDIS_CON0));
	pr_debug("AFE_ADDA_PREDIS_CON1 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_PREDIS_CON1));
	pr_debug("AFE_MRGIF_MON0 = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_MON0));
	pr_debug("AFE_MRGIF_MON1 = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_MON1));
	pr_debug("AFE_MRGIF_MON2 = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_MON2));
	pr_debug("AFE_I2S_MON = 0x%x\n", Afe_Get_Reg(AFE_I2S_MON));
	pr_debug("AFE_ADDA_IIR_COEF_02_01 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_IIR_COEF_02_01));
	pr_debug("AFE_ADDA_IIR_COEF_04_03 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_IIR_COEF_04_03));
	pr_debug("AFE_ADDA_IIR_COEF_06_05 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_IIR_COEF_06_05));
	pr_debug("AFE_ADDA_IIR_COEF_08_07 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_IIR_COEF_08_07));
	pr_debug("AFE_ADDA_IIR_COEF_10_09 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_IIR_COEF_10_09));
	pr_debug("AFE_DAC_CON2 = 0x%x\n", Afe_Get_Reg(AFE_DAC_CON2));
	pr_debug("AFE_IRQ_MCU_CON1 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CON1));
	pr_debug("AFE_IRQ_MCU_CON2 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CON2));
	pr_debug("AFE_DAC_MON = 0x%x\n", Afe_Get_Reg(AFE_DAC_MON));
	pr_debug("AFE_VUL2_BASE = 0x%x\n", Afe_Get_Reg(AFE_VUL2_BASE));
	pr_debug("AFE_VUL2_END = 0x%x\n", Afe_Get_Reg(AFE_VUL2_END));
	pr_debug("AFE_VUL2_CUR = 0x%x\n", Afe_Get_Reg(AFE_VUL2_CUR));
	pr_debug("AFE_IRQ_MCU_CNT0 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT0));
	pr_debug("AFE_IRQ_MCU_CNT6 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT6));
	pr_debug("AFE_IRQ_MCU_EN1 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_EN1));
	pr_debug("AFE_IRQ0_MCU_CNT_MON = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ0_MCU_CNT_MON));
	pr_debug("AFE_IRQ6_MCU_CNT_MON = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ6_MCU_CNT_MON));
	pr_debug("AFE_MOD_DAI_BASE = 0x%x\n", Afe_Get_Reg(AFE_MOD_DAI_BASE));
	pr_debug("AFE_MOD_DAI_END = 0x%x\n", Afe_Get_Reg(AFE_MOD_DAI_END));
	pr_debug("AFE_MOD_DAI_CUR = 0x%x\n", Afe_Get_Reg(AFE_MOD_DAI_CUR));
	pr_debug("AFE_VUL_D2_BASE = 0x%x\n", Afe_Get_Reg(AFE_VUL_D2_BASE));
	pr_debug("AFE_VUL_D2_END = 0x%x\n", Afe_Get_Reg(AFE_VUL_D2_END));
	pr_debug("AFE_VUL_D2_CUR = 0x%x\n", Afe_Get_Reg(AFE_VUL_D2_CUR));
	pr_debug("AFE_DL3_BASE = 0x%x\n", Afe_Get_Reg(AFE_DL3_BASE));
	pr_debug("AFE_DL3_CUR = 0x%x\n", Afe_Get_Reg(AFE_DL3_CUR));
	pr_debug("AFE_DL3_END = 0x%x\n", Afe_Get_Reg(AFE_DL3_END));
	pr_debug("AFE_HDMI_OUT_CON0 = 0x%x\n", Afe_Get_Reg(AFE_HDMI_OUT_CON0));
	pr_debug("AFE_HDMI_OUT_BASE = 0x%x\n", Afe_Get_Reg(AFE_HDMI_OUT_BASE));
	pr_debug("AFE_HDMI_OUT_CUR = 0x%x\n", Afe_Get_Reg(AFE_HDMI_OUT_CUR));
	pr_debug("AFE_HDMI_OUT_END = 0x%x\n", Afe_Get_Reg(AFE_HDMI_OUT_END));
	pr_debug("AFE_IRQ3_MCU_CNT_MON = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ3_MCU_CNT_MON));
	pr_debug("AFE_IRQ4_MCU_CNT_MON = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ4_MCU_CNT_MON));
	pr_debug("AFE_IRQ_MCU_CON0 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CON0));
	pr_debug("AFE_IRQ_MCU_STATUS = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ_MCU_STATUS));
	pr_debug("AFE_IRQ_MCU_CLR = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CLR));
	pr_debug("AFE_IRQ_MCU_CNT1 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT1));
	pr_debug("AFE_IRQ_MCU_CNT2 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT2));
	pr_debug("AFE_IRQ_MCU_EN = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_EN));
	pr_debug("AFE_IRQ_MCU_MON2 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_MON2));
	pr_debug("AFE_IRQ_MCU_CNT5 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT5));
	pr_debug("AFE_IRQ1_MCU_CNT_MON = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ1_MCU_CNT_MON));
	pr_debug("AFE_IRQ2_MCU_CNT_MON = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ2_MCU_CNT_MON));
	pr_debug("AFE_IRQ1_MCU_EN_CNT_MON = 0x%x\n",
		 Afe_Get_Reg(AFE_IRQ1_MCU_EN_CNT_MON));
	pr_debug("AFE_IRQ5_MCU_CNT_MON = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ5_MCU_CNT_MON));
	pr_debug("AFE_MEMIF_MINLEN = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MINLEN));
	pr_debug("AFE_MEMIF_MAXLEN = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MAXLEN));
	pr_debug("AFE_MEMIF_PBUF_SIZE = 0x%x\n",
		Afe_Get_Reg(AFE_MEMIF_PBUF_SIZE));
	pr_debug("AFE_IRQ_MCU_CNT7 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT7));
	pr_debug("AFE_IRQ7_MCU_CNT_MON = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ7_MCU_CNT_MON));
	pr_debug("AFE_IRQ_MCU_CNT3 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT3));
	pr_debug("AFE_IRQ_MCU_CNT4 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT4));
	pr_debug("AFE_IRQ_MCU_CNT11 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT11));
	pr_debug("AFE_APLL1_TUNER_CFG = 0x%x\n",
		Afe_Get_Reg(AFE_APLL1_TUNER_CFG));
	pr_debug("AFE_APLL2_TUNER_CFG = 0x%x\n",
		Afe_Get_Reg(AFE_APLL2_TUNER_CFG));
	pr_debug("AFE_MEMIF_HD_MODE = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_HD_MODE));
	pr_debug("AFE_MEMIF_HDALIGN = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_HDALIGN));
	pr_debug("AFE_CONN33 = 0x%x\n", Afe_Get_Reg(AFE_CONN33));
	pr_debug("AFE_IRQ_MCU_CNT12 = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT12));
	pr_debug("AFE_GAIN1_CON0 = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CON0));
	pr_debug("AFE_GAIN1_CON1 = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CON1));
	pr_debug("AFE_GAIN1_CON2 = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CON2));
	pr_debug("AFE_GAIN1_CON3 = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CON3));
	pr_debug("AFE_CONN7 = 0x%x\n", Afe_Get_Reg(AFE_CONN7));
	pr_debug("AFE_GAIN1_CUR = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CUR));
	pr_debug("AFE_GAIN2_CON0 = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CON0));
	pr_debug("AFE_GAIN2_CON1 = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CON1));
	pr_debug("AFE_GAIN2_CON2 = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CON2));
	pr_debug("AFE_GAIN2_CON3 = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CON3));
	pr_debug("AFE_CONN8 = 0x%x\n", Afe_Get_Reg(AFE_CONN8));
	pr_debug("AFE_GAIN2_CUR = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CUR));
	pr_debug("AFE_CONN9 = 0x%x\n", Afe_Get_Reg(AFE_CONN9));
	pr_debug("AFE_CONN10 = 0x%x\n", Afe_Get_Reg(AFE_CONN10));
	pr_debug("AFE_CONN11 = 0x%x\n", Afe_Get_Reg(AFE_CONN11));
	pr_debug("AFE_CONN12 = 0x%x\n", Afe_Get_Reg(AFE_CONN12));
	pr_debug("AFE_CONN13 = 0x%x\n", Afe_Get_Reg(AFE_CONN13));
	pr_debug("AFE_CONN14 = 0x%x\n", Afe_Get_Reg(AFE_CONN14));
	pr_debug("AFE_CONN15 = 0x%x\n", Afe_Get_Reg(AFE_CONN15));
	pr_debug("AFE_CONN16 = 0x%x\n", Afe_Get_Reg(AFE_CONN16));
	pr_debug("AFE_CONN17 = 0x%x\n", Afe_Get_Reg(AFE_CONN17));
	pr_debug("AFE_CONN18 = 0x%x\n", Afe_Get_Reg(AFE_CONN18));
	pr_debug("AFE_CONN19 = 0x%x\n", Afe_Get_Reg(AFE_CONN19));
	pr_debug("AFE_CONN20 = 0x%x\n", Afe_Get_Reg(AFE_CONN20));
	pr_debug("AFE_CONN21 = 0x%x\n", Afe_Get_Reg(AFE_CONN21));
	pr_debug("AFE_CONN22 = 0x%x\n", Afe_Get_Reg(AFE_CONN22));
	pr_debug("AFE_CONN23 = 0x%x\n", Afe_Get_Reg(AFE_CONN23));
	pr_debug("AFE_CONN24 = 0x%x\n", Afe_Get_Reg(AFE_CONN24));
	pr_debug("AFE_CONN_RS = 0x%x\n", Afe_Get_Reg(AFE_CONN_RS));
	pr_debug("AFE_CONN_DI = 0x%x\n", Afe_Get_Reg(AFE_CONN_DI));
	pr_debug("AFE_CONN25 = 0x%x\n", Afe_Get_Reg(AFE_CONN25));
	pr_debug("AFE_CONN26 = 0x%x\n", Afe_Get_Reg(AFE_CONN26));
	pr_debug("AFE_CONN27 = 0x%x\n", Afe_Get_Reg(AFE_CONN27));
	pr_debug("AFE_CONN28 = 0x%x\n", Afe_Get_Reg(AFE_CONN28));
	pr_debug("AFE_CONN29 = 0x%x\n", Afe_Get_Reg(AFE_CONN29));
	pr_debug("AFE_CONN30 = 0x%x\n", Afe_Get_Reg(AFE_CONN30));
	pr_debug("AFE_CONN31 = 0x%x\n", Afe_Get_Reg(AFE_CONN31));
	pr_debug("AFE_CONN32 = 0x%x\n", Afe_Get_Reg(AFE_CONN32));
	pr_debug("AFE_SRAM_DELSEL_CON0 = 0x%x\n",
		Afe_Get_Reg(AFE_SRAM_DELSEL_CON0));
	pr_debug("AFE_SRAM_DELSEL_CON2 = 0x%x\n",
		Afe_Get_Reg(AFE_SRAM_DELSEL_CON2));
	pr_debug("AFE_SRAM_DELSEL_CON3 = 0x%x\n",
		Afe_Get_Reg(AFE_SRAM_DELSEL_CON3));
	pr_debug("AFE_ASRC_2CH_CON12 = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_2CH_CON12));
	pr_debug("AFE_ASRC_2CH_CON13 = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_2CH_CON13));
	pr_debug("PCM_INTF_CON1 = 0x%x\n", Afe_Get_Reg(PCM_INTF_CON1));
	pr_debug("PCM_INTF_CON2 = 0x%x\n", Afe_Get_Reg(PCM_INTF_CON2));
	pr_debug("PCM2_INTF_CON = 0x%x\n", Afe_Get_Reg(PCM2_INTF_CON));
	pr_debug("AFE_TDM_CON1 = 0x%x\n", Afe_Get_Reg(AFE_TDM_CON1));
	pr_debug("AFE_TDM_CON2 = 0x%x\n", Afe_Get_Reg(AFE_TDM_CON2));
	pr_debug("AFE_CONN34 = 0x%x\n", Afe_Get_Reg(AFE_CONN34));
	pr_debug("FPGA_CFG0 = 0x%x\n", Afe_Get_Reg(FPGA_CFG0));
	pr_debug("FPGA_CFG1 = 0x%x\n", Afe_Get_Reg(FPGA_CFG1));
	pr_debug("FPGA_CFG2 = 0x%x\n", Afe_Get_Reg(FPGA_CFG2));
	pr_debug("FPGA_CFG3 = 0x%x\n", Afe_Get_Reg(FPGA_CFG3));
	pr_debug("AUDIO_TOP_DBG_CON = 0x%x\n", Afe_Get_Reg(AUDIO_TOP_DBG_CON));
	pr_debug("AUDIO_TOP_DBG_MON0 = 0x%x\n",
		Afe_Get_Reg(AUDIO_TOP_DBG_MON0));
	pr_debug("AUDIO_TOP_DBG_MON1 = 0x%x\n",
		Afe_Get_Reg(AUDIO_TOP_DBG_MON1));
	pr_debug("AFE_IRQ8_MCU_CNT_MON = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ8_MCU_CNT_MON));
	pr_debug("AFE_IRQ11_MCU_CNT_MON = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ11_MCU_CNT_MON));
	pr_debug("AFE_IRQ12_MCU_CNT_MON = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ12_MCU_CNT_MON));
	pr_debug("AFE_GENERAL_REG0 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG0));
	pr_debug("AFE_GENERAL_REG1 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG1));
	pr_debug("AFE_GENERAL_REG2 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG2));
	pr_debug("AFE_GENERAL_REG3 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG3));
	pr_debug("AFE_GENERAL_REG4 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG4));
	pr_debug("AFE_GENERAL_REG5 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG5));
	pr_debug("AFE_GENERAL_REG6 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG6));
	pr_debug("AFE_GENERAL_REG7 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG7));
	pr_debug("AFE_GENERAL_REG8 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG8));
	pr_debug("AFE_GENERAL_REG9 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG9));
	pr_debug("AFE_GENERAL_REG10 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG10));
	pr_debug("AFE_GENERAL_REG11 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG11));
	pr_debug("AFE_GENERAL_REG12 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG12));
	pr_debug("AFE_GENERAL_REG13 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG13));
	pr_debug("AFE_GENERAL_REG14 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG14));
	pr_debug("AFE_GENERAL_REG15 = 0x%x\n", Afe_Get_Reg(AFE_GENERAL_REG15));
	pr_debug("AFE_CBIP_CFG0 = 0x%x\n", Afe_Get_Reg(AFE_CBIP_CFG0));
	pr_debug("AFE_CBIP_MON0 = 0x%x\n", Afe_Get_Reg(AFE_CBIP_MON0));
	pr_debug("AFE_CBIP_SLV_MUX_MON0 = 0x%x\n",
		Afe_Get_Reg(AFE_CBIP_SLV_MUX_MON0));
	pr_debug("AFE_CBIP_SLV_DECODER_MON0 = 0x%x\n",
		 Afe_Get_Reg(AFE_CBIP_SLV_DECODER_MON0));
	pr_debug("AFE_CONN0_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN0_1));
	pr_debug("AFE_CONN1_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN1_1));
	pr_debug("AFE_CONN2_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN2_1));
	pr_debug("AFE_CONN3_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN3_1));
	pr_debug("AFE_CONN4_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN4_1));
	pr_debug("AFE_CONN5_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN5_1));
	pr_debug("AFE_CONN6_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN6_1));
	pr_debug("AFE_CONN7_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN7_1));
	pr_debug("AFE_CONN8_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN8_1));
	pr_debug("AFE_CONN9_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN9_1));
	pr_debug("AFE_CONN10_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN10_1));
	pr_debug("AFE_CONN11_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN11_1));
	pr_debug("AFE_CONN12_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN12_1));
	pr_debug("AFE_CONN13_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN13_1));
	pr_debug("AFE_CONN14_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN14_1));
	pr_debug("AFE_CONN15_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN15_1));
	pr_debug("AFE_CONN16_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN16_1));
	pr_debug("AFE_CONN17_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN17_1));
	pr_debug("AFE_CONN18_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN18_1));
	pr_debug("AFE_CONN19_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN19_1));
	pr_debug("AFE_CONN20_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN20_1));
	pr_debug("AFE_CONN21_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN21_1));
	pr_debug("AFE_CONN22_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN22_1));
	pr_debug("AFE_CONN23_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN23_1));
	pr_debug("AFE_CONN24_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN24_1));
	pr_debug("AFE_CONN25_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN25_1));
	pr_debug("AFE_CONN26_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN26_1));
	pr_debug("AFE_CONN27_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN27_1));
	pr_debug("AFE_CONN28_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN28_1));
	pr_debug("AFE_CONN29_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN29_1));
	pr_debug("AFE_CONN30_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN30_1));
	pr_debug("AFE_CONN31_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN31_1));
	pr_debug("AFE_CONN32_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN32_1));
	pr_debug("AFE_CONN33_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN33_1));
	pr_debug("AFE_CONN34_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN34_1));
	pr_debug("AFE_CONN_RS_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN_RS_1));
	pr_debug("AFE_CONN_DI_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN_DI_1));
	pr_debug("AFE_CONN_24BIT_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN_24BIT_1));
	pr_debug("AFE_CONN_REG = 0x%x\n", Afe_Get_Reg(AFE_CONN_REG));
	pr_debug("AFE_CONN35 = 0x%x\n", Afe_Get_Reg(AFE_CONN35));
	pr_debug("AFE_CONN36 = 0x%x\n", Afe_Get_Reg(AFE_CONN36));
	pr_debug("AFE_CONN37 = 0x%x\n", Afe_Get_Reg(AFE_CONN37));
	pr_debug("AFE_CONN38 = 0x%x\n", Afe_Get_Reg(AFE_CONN38));
	pr_debug("AFE_CONN35_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN35_1));
	pr_debug("AFE_CONN36_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN36_1));
	pr_debug("AFE_CONN37_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN37_1));
	pr_debug("AFE_CONN38_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN38_1));
	pr_debug("AFE_CONN39 = 0x%x\n", Afe_Get_Reg(AFE_CONN39));
	pr_debug("AFE_CONN40 = 0x%x\n", Afe_Get_Reg(AFE_CONN40));
	pr_debug("AFE_CONN41 = 0x%x\n", Afe_Get_Reg(AFE_CONN41));
	pr_debug("AFE_CONN42 = 0x%x\n", Afe_Get_Reg(AFE_CONN42));
	pr_debug("AFE_CONN39_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN39_1));
	pr_debug("AFE_CONN40_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN40_1));
	pr_debug("AFE_CONN41_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN41_1));
	pr_debug("AFE_CONN42_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN42_1));
	pr_debug("AFE_I2S_CON4 = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON4));
	pr_debug("AFE_ADDA6_TOP_CON0 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA6_TOP_CON0));
	pr_debug("AFE_ADDA6_UL_SRC_CON0 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA6_UL_SRC_CON0));
	pr_debug("AFE_ADD6_UL_SRC_CON1 = 0x%x\n",
		Afe_Get_Reg(AFE_ADD6_UL_SRC_CON1));
	pr_debug("AFE_ADDA6_SRC_DEBUG = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA6_SRC_DEBUG));
	pr_debug("AFE_ADDA6_SRC_DEBUG_MON0 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_SRC_DEBUG_MON0));
	pr_debug("AFE_ADDA6_ULCF_CFG_02_01 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_02_01));
	pr_debug("AFE_ADDA6_ULCF_CFG_04_03 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_04_03));
	pr_debug("AFE_ADDA6_ULCF_CFG_06_05 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_06_05));
	pr_debug("AFE_ADDA6_ULCF_CFG_08_07 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_08_07));
	pr_debug("AFE_ADDA6_ULCF_CFG_10_09 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_10_09));
	pr_debug("AFE_ADDA6_ULCF_CFG_12_11 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_12_11));
	pr_debug("AFE_ADDA6_ULCF_CFG_14_13 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_14_13));
	pr_debug("AFE_ADDA6_ULCF_CFG_16_15 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_16_15));
	pr_debug("AFE_ADDA6_ULCF_CFG_18_17 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_18_17));
	pr_debug("AFE_ADDA6_ULCF_CFG_20_19 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_20_19));
	pr_debug("AFE_ADDA6_ULCF_CFG_22_21 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_22_21));
	pr_debug("AFE_ADDA6_ULCF_CFG_24_23 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_24_23));
	pr_debug("AFE_ADDA6_ULCF_CFG_26_25 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_26_25));
	pr_debug("AFE_ADDA6_ULCF_CFG_28_27 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_28_27));
	pr_debug("AFE_ADDA6_ULCF_CFG_30_29 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_30_29));
	pr_debug("AFE_ADD6A_UL_SRC_MON0 = 0x%x\n",
		Afe_Get_Reg(AFE_ADD6A_UL_SRC_MON0));
	pr_debug("AFE_ADDA6_UL_SRC_MON1 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA6_UL_SRC_MON1));
	pr_debug("AFE_CONN43 = 0x%x\n", Afe_Get_Reg(AFE_CONN43));
	pr_debug("AFE_CONN43_1 = 0x%x\n", Afe_Get_Reg(AFE_CONN43_1));
	pr_debug("AFE_DL1_BASE_MSB = 0x%x\n", Afe_Get_Reg(AFE_DL1_BASE_MSB));
	pr_debug("AFE_DL1_CUR_MSB = 0x%x\n", Afe_Get_Reg(AFE_DL1_CUR_MSB));
	pr_debug("AFE_DL1_END_MSB = 0x%x\n", Afe_Get_Reg(AFE_DL1_END_MSB));
	pr_debug("AFE_DL2_BASE_MSB = 0x%x\n", Afe_Get_Reg(AFE_DL2_BASE_MSB));
	pr_debug("AFE_DL2_CUR_MSB = 0x%x\n", Afe_Get_Reg(AFE_DL2_CUR_MSB));
	pr_debug("AFE_DL2_END_MSB = 0x%x\n", Afe_Get_Reg(AFE_DL2_END_MSB));
	pr_debug("AFE_AWB_BASE_MSB = 0x%x\n", Afe_Get_Reg(AFE_AWB_BASE_MSB));
	pr_debug("AFE_AWB_END_MSB = 0x%x\n", Afe_Get_Reg(AFE_AWB_END_MSB));
	pr_debug("AFE_AWB_CUR_MSB = 0x%x\n", Afe_Get_Reg(AFE_AWB_CUR_MSB));
	pr_debug("AFE_VUL_BASE_MSB = 0x%x\n", Afe_Get_Reg(AFE_VUL_BASE_MSB));
	pr_debug("AFE_VUL_END_MSB = 0x%x\n", Afe_Get_Reg(AFE_VUL_END_MSB));
	pr_debug("AFE_VUL_CUR_MSB = 0x%x\n", Afe_Get_Reg(AFE_VUL_CUR_MSB));
	pr_debug("AFE_VUL2_BASE_MSB = 0x%x\n", Afe_Get_Reg(AFE_VUL2_BASE_MSB));
	pr_debug("AFE_VUL2_END_MSB = 0x%x\n", Afe_Get_Reg(AFE_VUL2_END_MSB));
	pr_debug("AFE_VUL2_CUR_MSB = 0x%x\n", Afe_Get_Reg(AFE_VUL2_CUR_MSB));
	pr_debug("AFE_MOD_DAI_BASE_MSB = 0x%x\n",
		Afe_Get_Reg(AFE_MOD_DAI_BASE_MSB));
	pr_debug("AFE_MOD_DAI_END_MSB = 0x%x\n",
		Afe_Get_Reg(AFE_MOD_DAI_END_MSB));
	pr_debug("AFE_MOD_DAI_CUR_MSB = 0x%x\n",
		Afe_Get_Reg(AFE_MOD_DAI_CUR_MSB));
	pr_debug("AFE_VUL_D2_BASE_MSB = 0x%x\n",
		Afe_Get_Reg(AFE_VUL_D2_BASE_MSB));
	pr_debug("AFE_VUL_D2_END_MSB = 0x%x\n",
		Afe_Get_Reg(AFE_VUL_D2_END_MSB));
	pr_debug("AFE_VUL_D2_CUR_MSB = 0x%x\n",
		Afe_Get_Reg(AFE_VUL_D2_CUR_MSB));
	pr_debug("AFE_DL3_BASE_MSB = 0x%x\n", Afe_Get_Reg(AFE_DL3_BASE_MSB));
	pr_debug("AFE_DL3_CUR_MSB = 0x%x\n", Afe_Get_Reg(AFE_DL3_CUR_MSB));
	pr_debug("AFE_DL3_END_MSB = 0x%x\n", Afe_Get_Reg(AFE_DL3_END_MSB));
	pr_debug("AFE_HDMI_OUT_BASE_MSB = 0x%x\n",
		Afe_Get_Reg(AFE_HDMI_OUT_BASE_MSB));
	pr_debug("AFE_HDMI_OUT_CUR_MSB = 0x%x\n",
		Afe_Get_Reg(AFE_HDMI_OUT_CUR_MSB));
	pr_debug("AFE_HDMI_OUT_END_MSB = 0x%x\n",
		Afe_Get_Reg(AFE_HDMI_OUT_END_MSB));
	pr_debug("AFE_AWB2_BASE = 0x%x\n", Afe_Get_Reg(AFE_AWB2_BASE));
	pr_debug("AFE_AWB2_END = 0x%x\n", Afe_Get_Reg(AFE_AWB2_END));
	pr_debug("AFE_AWB2_CUR = 0x%x\n", Afe_Get_Reg(AFE_AWB2_CUR));
	pr_debug("AFE_AWB2_BASE_MSB = 0x%x\n", Afe_Get_Reg(AFE_AWB2_BASE_MSB));
	pr_debug("AFE_AWB2_END_MSB = 0x%x\n", Afe_Get_Reg(AFE_AWB2_END_MSB));
	pr_debug("AFE_AWB2_CUR_MSB = 0x%x\n", Afe_Get_Reg(AFE_AWB2_CUR_MSB));
	pr_debug("AFE_ADDA_DL_SDM_DCCOMP_CON = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_DL_SDM_DCCOMP_CON));
	pr_debug("AFE_ADDA_DL_SDM_TEST = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_DL_SDM_TEST));
	pr_debug("AFE_ADDA_DL_DC_COMP_CFG0 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_DL_DC_COMP_CFG0));
	pr_debug("AFE_ADDA_DL_DC_COMP_CFG1 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_DL_DC_COMP_CFG1));
	pr_debug("AFE_ADDA_DL_SDM_FIFO_MON = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_DL_SDM_FIFO_MON));
	pr_debug("AFE_ADDA_DL_SRC_LCH_MON = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_DL_SRC_LCH_MON));
	pr_debug("AFE_ADDA_DL_SRC_RCH_MON = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_DL_SRC_RCH_MON));
	pr_debug("AFE_ADDA_DL_SDM_OUT_MON = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_DL_SDM_OUT_MON));
	pr_debug("AFE_CONNSYS_I2S_CON = 0x%x\n",
		Afe_Get_Reg(AFE_CONNSYS_I2S_CON));
	pr_debug("AFE_CONNSYS_I2S_MON = 0x%x\n",
		Afe_Get_Reg(AFE_CONNSYS_I2S_MON));
	pr_debug("AFE_ASRC_CONNSYS_CON0 = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CONNSYS_CON0));
	pr_debug("AFE_ASRC_2CH_CON1 = 0x%x\n", Afe_Get_Reg(AFE_ASRC_2CH_CON1));
	pr_debug("AFE_ASRC_CONNSYS_CON13 = 0x%x\n",
		 Afe_Get_Reg(AFE_ASRC_CONNSYS_CON13));
	pr_debug("AFE_ASRC_CONNSYS_CON14 = 0x%x\n",
		 Afe_Get_Reg(AFE_ASRC_CONNSYS_CON14));
	pr_debug("AFE_ASRC_CONNSYS_CON15 = 0x%x\n",
		 Afe_Get_Reg(AFE_ASRC_CONNSYS_CON15));
	pr_debug("AFE_ASRC_CONNSYS_CON16 = 0x%x\n",
		 Afe_Get_Reg(AFE_ASRC_CONNSYS_CON16));
	pr_debug("AFE_ASRC_CONNSYS_CON17 = 0x%x\n",
		 Afe_Get_Reg(AFE_ASRC_CONNSYS_CON17));
	pr_debug("AFE_ASRC_CONNSYS_CON18 = 0x%x\n",
		 Afe_Get_Reg(AFE_ASRC_CONNSYS_CON18));
	pr_debug("AFE_ASRC_CONNSYS_CON19 = 0x%x\n",
		 Afe_Get_Reg(AFE_ASRC_CONNSYS_CON19));
	pr_debug("AFE_ASRC_CONNSYS_CON20 = 0x%x\n",
		 Afe_Get_Reg(AFE_ASRC_CONNSYS_CON20));
	pr_debug("AFE_ASRC_CONNSYS_CON21 = 0x%x\n",
		 Afe_Get_Reg(AFE_ASRC_CONNSYS_CON21));
	pr_debug("AFE_ADDA6_IIR_COEF_02_01 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_IIR_COEF_02_01));
	pr_debug("AFE_ADDA6_IIR_COEF_04_03 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_IIR_COEF_04_03));
	pr_debug("AFE_ADDA6_IIR_COEF_06_05 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_IIR_COEF_06_05));
	pr_debug("AFE_ADDA6_IIR_COEF_08_07 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_IIR_COEF_08_07));
	pr_debug("AFE_ADDA6_IIR_COEF_10_09 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA6_IIR_COEF_10_09));
	pr_debug("AFE_ADDA_PREDIS_CON2 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_PREDIS_CON2));
	pr_debug("AFE_ADDA_PREDIS_CON3 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_PREDIS_CON3));
	pr_debug("AFE_MEMIF_MON12 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON12));
	pr_debug("AFE_MEMIF_MON13 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON13));
	pr_debug("AFE_MEMIF_MON14 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON14));
	pr_debug("AFE_MEMIF_MON15 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON15));
	pr_debug("AFE_MEMIF_MON16 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON16));
	pr_debug("AFE_MEMIF_MON17 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON17));
	pr_debug("AFE_MEMIF_MON18 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON18));
	pr_debug("AFE_MEMIF_MON19 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON19));
	pr_debug("AFE_MEMIF_MON20 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON20));
	pr_debug("AFE_MEMIF_MON21 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON21));
	pr_debug("AFE_MEMIF_MON22 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON22));
	pr_debug("AFE_MEMIF_MON23 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON23));
	pr_debug("AFE_MEMIF_MON24 = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON24));
	pr_debug("AFE_HD_ENGEN_ENABLE = 0x%x\n",
		Afe_Get_Reg(AFE_HD_ENGEN_ENABLE));
	pr_debug("AFE_ADDA_MTKAIF_CFG0 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_MTKAIF_CFG0));
	pr_debug("AFE_ADDA_MTKAIF_TX_CFG1 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_MTKAIF_TX_CFG1));
	pr_debug("AFE_ADDA_MTKAIF_RX_CFG0 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_MTKAIF_RX_CFG0));
	pr_debug("AFE_ADDA_MTKAIF_RX_CFG1 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_MTKAIF_RX_CFG1));
	pr_debug("AFE_ADDA_MTKAIF_RX_CFG2 = 0x%x\n",
		 Afe_Get_Reg(AFE_ADDA_MTKAIF_RX_CFG2));
	pr_debug("AFE_ADDA_MTKAIF_MON0 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_MTKAIF_MON0));
	pr_debug("AFE_ADDA_MTKAIF_MON1 = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_MTKAIF_MON1));
	pr_debug("AFE_AUD_PAD_TOP_CFG = 0x%x\n",
		Afe_Get_Reg(AFE_AUD_PAD_TOP_CFG));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON0 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON0));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON1 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON1));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON2 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON2));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON3 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON3));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON4 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON4));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON5 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON5));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON6 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON6));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON7 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON7));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON8 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON8));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON9 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON9));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON10 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON10));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON12 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON12));
	pr_debug("AFE_GENERAL1_ASRC_2CH_CON13 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON13));
	pr_debug("GENERAL_ASRC_MODE = 0x%x\n",
		Afe_Get_Reg(GENERAL_ASRC_MODE));
	pr_debug("GENERAL_ASRC_EN_ON = 0x%x\n",
		Afe_Get_Reg(GENERAL_ASRC_EN_ON));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON0 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON0));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON1 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON1));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON2 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON2));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON3 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON3));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON4 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON4));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON5 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON5));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON6 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON6));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON7 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON7));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON8 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON8));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON9 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON9));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON10 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON10));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON12 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON12));
	pr_debug("AFE_GENERAL2_ASRC_2CH_CON13 = 0x%x\n",
		 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON13));
	pr_debug("AP_PLL_CON5 = 0x%x\n", GetApmixedCfg(AP_PLL_CON5));
	AudDrv_Clk_Off();
}
/* export symbols for other module using */
EXPORT_SYMBOL(Afe_Log_Print);

void Enable4pin_I2S0_I2S3(unsigned int SampleRate, unsigned int wLenBit)
{
	unsigned int Audio_I2S0 = 0;
	unsigned int Audio_I2S3 = 0;

	Afe_Set_Reg(AUDIO_TOP_CON1, 0x1 << 4,  0x1 << 4); /* I2S0 clock-gated */
	Afe_Set_Reg(AUDIO_TOP_CON1, 0x1 << 7,  0x1 << 7); /* I2S3 clock-gated */

	/* Set I2S0 configuration */
	Audio_I2S0 |= (Soc_Aud_I2S_IN_PAD_SEL_I2S_IN_FROM_IO_MUX <<
		       28);/* I2S in from io_mux */
	Audio_I2S0 |= Soc_Aud_LOW_JITTER_CLOCK << 12; /* Low jitter mode */
	Audio_I2S0 |= (Soc_Aud_INV_LRCK_NO_INVERSE << 5);
	Audio_I2S0 |= (Soc_Aud_I2S_FORMAT_I2S << 3);
	Audio_I2S0 |= (wLenBit << 1);
	Afe_Set_Reg(AFE_I2S_CON, Audio_I2S0, MASK_ALL);
	pr_debug("Audio_I2S0= 0x%x\n", Audio_I2S0);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S,
		      SampleRate); /* set I2S0 sample rate */

	/* Set I2S3 configuration */
	Audio_I2S3 |= Soc_Aud_LOW_JITTER_CLOCK << 12; /* Low jitter mode */
	Audio_I2S3 |= SampleRateTransform(SampleRate,
					  Soc_Aud_Digital_Block_I2S_IN_2) << 8;
	Audio_I2S3 |= Soc_Aud_I2S_FORMAT_I2S << 3; /*  I2s format */
	Audio_I2S3 |= wLenBit << 1; /* WLEN */
	Afe_Set_Reg(AFE_I2S_CON3, Audio_I2S3, AFE_MASK_ALL);
	pr_debug("Audio_I2S3= 0x%x\n", Audio_I2S3);

	Afe_Set_Reg(AUDIO_TOP_CON1, 0 << 4,  0x1 << 4);
	Afe_Set_Reg(AUDIO_TOP_CON1, 0 << 7,  0x1 << 7);

	udelay(200);

	/*Afe_Set_Reg(AUDIO_TOP_CON1, 0,  0x2);*/
	/* Clear I2S_SOFT_Reset  4 wire i2s mode*/

	Afe_Set_Reg(AFE_I2S_CON, 0x1, 0x1); /* Enable I2S0 */

	Afe_Set_Reg(AFE_I2S_CON3, 0x1, 0x1); /* Enable I2S3 */
}

void SetChipModemPcmConfig(int modem_index,
			   struct audio_digital_pcm p_modem_pcm_attribute)
{
	unsigned int reg_pcm2_intf_con = 0;
	unsigned int reg_pcm_intf_con1 = 0;

	if (modem_index == MODEM_1) {
		reg_pcm2_intf_con |=
			(p_modem_pcm_attribute.mTxLchRepeatSel & 0x1) << 13;
		reg_pcm2_intf_con |=
			(p_modem_pcm_attribute.mVbt16kModeSel & 0x1) << 12;
		reg_pcm2_intf_con |=
			(p_modem_pcm_attribute.mSingelMicSel & 0x1) << 7;
		reg_pcm2_intf_con |=
			(p_modem_pcm_attribute.mAsyncFifoSel & 0x1) << 6;
		reg_pcm2_intf_con |=
			(p_modem_pcm_attribute.mPcmWordLength & 0x1) << 5;
		reg_pcm2_intf_con |=
			(p_modem_pcm_attribute.mPcmModeWidebandSel & 0x3) << 3;
		reg_pcm2_intf_con |=
			(p_modem_pcm_attribute.mPcmFormat & 0x3) << 1;
		pr_debug("%s(), PCM2_INTF_CON(0x%lx) = 0x%x\n",
			__func__, PCM2_INTF_CON,
			 reg_pcm2_intf_con);
		Afe_Set_Reg(PCM2_INTF_CON, reg_pcm2_intf_con, MASK_ALL);
	} else if (modem_index == MODEM_2 || modem_index == MODEM_EXTERNAL) {
		/* MODEM_2 use PCM_INTF_CON1 (0x530) !!! */
		reg_pcm_intf_con1 |=
		(p_modem_pcm_attribute.mBclkOutInv & 0x01) << 22;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mTxLchRepeatSel & 0x01) << 19;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mVbt16kModeSel & 0x01) << 18;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mExtModemSel & 0x01) << 17;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mExtendBckSyncLength & 0x1F) <<
			9;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mExtendBckSyncTypeSel & 0x01) <<
			8;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mSingelMicSel & 0x01) << 7;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mAsyncFifoSel & 0x01) << 6;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mSlaveModeSel & 0x01) << 5;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mPcmModeWidebandSel & 0x03) <<
			3;
		reg_pcm_intf_con1 |=
			(p_modem_pcm_attribute.mPcmFormat & 0x03) << 1;
		pr_debug("%s(), PCM_INTF_CON1(0x%lx) = 0x%x",
			__func__, PCM_INTF_CON1,
			 reg_pcm_intf_con1);
		Afe_Set_Reg(PCM_INTF_CON1, reg_pcm_intf_con1, MASK_ALL);
	}
}

bool SetChipModemPcmEnable(int modem_index, bool modem_pcm_on)
{
	if (modem_index == MODEM_1) {
		/* todo:: temp for use fifo */
		Afe_Set_Reg(PCM2_INTF_CON, modem_pcm_on, 0x1);
		modem_index = MODEM_1;
	} else if (modem_index == MODEM_2 || modem_index == MODEM_EXTERNAL) {
		/* MODEM_2 use PCM_INTF_CON1 (0x530) !!! */
		if (modem_pcm_on == true)
			Afe_Set_Reg(PCM_INTF_CON1, 0x1, 0x1);
		else if (modem_pcm_on == false)
			Afe_Set_Reg(PCM_INTF_CON1, 0x0, 0x1);
	} else {
		pr_err("%s(), no such modem_index: %d!!",
			__func__, modem_index);
		return false;
	}

	return true;
}

bool set_chip_sine_gen_enable(unsigned int connection, bool direction,
			      bool Enable)
{
	pr_debug("+%s(), connection = %d, direction = %d, Enable= %d\n",
		__func__,
		 connection,
		 direction, Enable);

	if (Enable && direction) {
		Afe_Set_Reg(AFE_SGEN_CON0, 0x04AC2AC1, 0xffffffff);
		switch (connection) {
		case Soc_Aud_InterConnectionInput_I00:
		case Soc_Aud_InterConnectionInput_I01:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x0, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I02:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x1, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I03:
		case Soc_Aud_InterConnectionInput_I04:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x2, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I05:
		case Soc_Aud_InterConnectionInput_I06:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x3, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I07:
		case Soc_Aud_InterConnectionInput_I08:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x4, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I09:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x5, 0x3f);
		case Soc_Aud_InterConnectionInput_I10:
		case Soc_Aud_InterConnectionInput_I11:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x6, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I12:
		case Soc_Aud_InterConnectionInput_I13:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x7, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I14:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x8, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I15:
		case Soc_Aud_InterConnectionInput_I16:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x9, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I17:
		case Soc_Aud_InterConnectionInput_I18:
			Afe_Set_Reg(AFE_SGEN_CON2, 0xa, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I19:
		case Soc_Aud_InterConnectionInput_I20:
			Afe_Set_Reg(AFE_SGEN_CON2, 0xb, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I21:
		case Soc_Aud_InterConnectionInput_I22:
			Afe_Set_Reg(AFE_SGEN_CON2, 0xc, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I23:
		case Soc_Aud_InterConnectionInput_I24:
			Afe_Set_Reg(AFE_SGEN_CON2, 0xd, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I25:
		case Soc_Aud_InterConnectionInput_I26:
			Afe_Set_Reg(AFE_SGEN_CON2, 0xe, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I27:
		case Soc_Aud_InterConnectionInput_I28:
			Afe_Set_Reg(AFE_SGEN_CON2, 0xf, 0x3f);
			break;
		case Soc_Aud_InterConnectionInput_I34:
		case Soc_Aud_InterConnectionInput_I35:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x13, 0x3f);
			break;
		default:
			break;
		}
	} else if (Enable) {
		Afe_Set_Reg(AFE_SGEN_CON0, 0x04AC2AC1, 0xffffffff);
		switch (connection) {
		case Soc_Aud_InterConnectionOutput_O00:
		case Soc_Aud_InterConnectionOutput_O01:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x20, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O02:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x21, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O03:
		case Soc_Aud_InterConnectionOutput_O04:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x22, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O05:
		case Soc_Aud_InterConnectionOutput_O06:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x23, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O07:
		case Soc_Aud_InterConnectionOutput_O08:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x24, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O09:
		case Soc_Aud_InterConnectionOutput_O10:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x25, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O11:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x26, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O12:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x27, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O13:
		case Soc_Aud_InterConnectionOutput_O14:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x28, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O15:
		case Soc_Aud_InterConnectionOutput_O16:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x29, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O17:
		case Soc_Aud_InterConnectionOutput_O18:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x2a, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O19:
		case Soc_Aud_InterConnectionOutput_O20:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x2b, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O21:
		case Soc_Aud_InterConnectionOutput_O22:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x2c, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O23:
		case Soc_Aud_InterConnectionOutput_O24:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x2d, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O25:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x2e, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O28:
		case Soc_Aud_InterConnectionOutput_O29:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x2f, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O30:
		case Soc_Aud_InterConnectionOutput_O31:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x30, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O34:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x31, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O36:
		case Soc_Aud_InterConnectionOutput_O37:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x34, 0x3f);
			break;
		case Soc_Aud_InterConnectionOutput_O38:
		case Soc_Aud_InterConnectionOutput_O39:
			Afe_Set_Reg(AFE_SGEN_CON2, 0x35, 0x3f);
			break;
		default:
			break;
		}
	} else {
		Afe_Set_Reg(AFE_SGEN_CON0, 0x0, 0xffffffff);
		Afe_Set_Reg(AFE_SGEN_CON2, 0x3f, 0x3f);
	}
	return true;
}

static bool src1_on;
bool set_chip_general_asrc1_enable(bool enable)
{
	pr_debug("%s(), enable = %d, src1_on = %d", __func__, enable, src1_on);

	if (enable) {
		if (src1_on)
			return true;

		Afe_Set_Reg(GENERAL_ASRC_EN_ON, 0x1 << 0, 0x1 << 0);
		/* ASM_ON */
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON0, 0x1 << 0, 0x1 << 0);
		/* CHSET_ON */
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON0, 0x1 << 2, 0x1 << 2);
		/* CHSET_STR_CLR */
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON0, 0x1 << 4, 0x1 << 4);

		src1_on = true;
	} else {
		if (!src1_on)
			return true;

		/* ASM_ON */
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON0, 0x0 << 0, 0x1 << 0);
		/* CHSET_ON */
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON0, 0x0 << 2, 0x1 << 2);
		/* CHSET_STR_CLR */
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON0, 0x0 << 4, 0x1 << 4);
		Afe_Set_Reg(GENERAL_ASRC_EN_ON, 0x0 << 0, 0x1 << 0);

		src1_on = false;
	}
	return true;
}

static bool src2_on;
bool set_chip_general_asrc2_enable(bool enable)
{
	pr_debug("%s(), enable = %d, src2_on = %d", __func__, enable, src2_on);

	if (enable) {
		if (src2_on)
			return true;

		Afe_Set_Reg(GENERAL_ASRC_EN_ON, 0x1 << 1, 0x1 << 1);
		/* ASM_ON */
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON0, 0x1 << 0, 0x1 << 0);
		/* CHSET_ON */
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON0, 0x1 << 2, 0x1 << 2);
		/* CHSET_STR_CLR */
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON0, 0x1 << 4, 0x1 << 4);

		src2_on = true;
	} else {
		if (!src2_on)
			return true;

		/* ASM_ON */
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON0, 0x0 << 0, 0x1 << 0);
		/* CHSET_ON */
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON0, 0x0 << 2, 0x1 << 2);
		/* CHSET_STR_CLR */
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON0, 0x0 << 4, 0x1 << 4);
		Afe_Set_Reg(GENERAL_ASRC_EN_ON, 0x0 << 1, 0x1 << 1);

		src2_on = false;
	}

	return true;
}

bool set_chip_general_asrc_enable(enum audio_general_asrc_id id, bool enable)
{
	bool ret = false;

	pr_debug("%s(), id = %d, enable = %d", __func__, id, enable);

	switch (id) {
	case AUDIO_GENERAL_ASRC_1:
		ret = set_chip_general_asrc1_enable(enable);
		break;
	case AUDIO_GENERAL_ASRC_2:
		ret = set_chip_general_asrc2_enable(enable);
		break;
	default:
		pr_debug("%s(), No such SRC id = %d\n", __func__, id);
		return ret;
	}
	/* Toggle BT DAT memif for enable general src on 6771 & 6775 */
	Afe_Set_Reg(AFE_DAC_CON0, 0x1 << 4, 0x1 << 4);
	Afe_Set_Reg(AFE_DAC_CON0, 0x0 << 4, 0x1 << 4);
	return ret;
}

const unsigned int *get_iir_coeff(unsigned int sample_rate_in,
				  unsigned int sample_rate_out,
				  unsigned int *parameter_num)
{
	if (sample_rate_in == 32000 && sample_rate_out == 16000) {
		*parameter_num = ARRAY_SIZE(src_iir_coeff_32_to_16);
		return src_iir_coeff_32_to_16;
	} else if (sample_rate_in == 44100 && sample_rate_out == 16000) {
		*parameter_num = ARRAY_SIZE(src_iir_coeff_44_to_16);
		return src_iir_coeff_44_to_16;
	} else if (sample_rate_in == 44100 && sample_rate_out == 32000) {
		*parameter_num = ARRAY_SIZE(src_iir_coeff_44_to_32);
		return src_iir_coeff_44_to_32;
	} else if ((sample_rate_in == 48000 && sample_rate_out == 16000) ||
		   (sample_rate_in == 96000 && sample_rate_out == 32000)) {
		*parameter_num = ARRAY_SIZE(src_iir_coeff_48_to_16);
		return src_iir_coeff_48_to_16;
	} else if (sample_rate_in == 48000 && sample_rate_out == 32000) {
		*parameter_num = ARRAY_SIZE(src_iir_coeff_48_to_32);
		return src_iir_coeff_48_to_32;
	} else if (sample_rate_in == 48000 && sample_rate_out == 44100) {
		*parameter_num = ARRAY_SIZE(src_iir_coeff_48_to_44);
		return src_iir_coeff_48_to_44;
	} else if (sample_rate_in == 96000 && sample_rate_out == 16000) {
		*parameter_num = ARRAY_SIZE(src_iir_coeff_96_to_16);
		return src_iir_coeff_96_to_16;
	} else if (sample_rate_in == 96000 && sample_rate_out == 44100) {
		*parameter_num = ARRAY_SIZE(src_iir_coeff_96_to_44);
		return src_iir_coeff_96_to_44;
	}
	pr_debug("%s(), No this IIR Coeff %dHz -> %dHz\n",
		 __func__, sample_rate_in, sample_rate_out);
	*parameter_num = 0;
	return NULL;
}

bool set_general_asrc_1_parameter(unsigned int sample_rate_in,
				  unsigned int sample_rate_out)
{
	unsigned int src_in_mode = SampleRateTransform(sample_rate_in, 0);
	unsigned int src_out_mode = SampleRateTransform(sample_rate_out, 0);

	unsigned int src_in_freq_mode =
			src_param[src_in_mode][AUDIO_SRC_PARAM_TYPE_FREQ_MODE];
	unsigned int src_out_freq_mode =
			src_param[src_out_mode][AUDIO_SRC_PARAM_TYPE_FREQ_MODE];

	int coeff_index = 0;
	unsigned int iir_coeff_num = 0;

	pr_debug("%s(), sample_rate_in = %d, sample_rate_out = %d, src1_on = %d\n",
		 __func__, sample_rate_in, sample_rate_out, src1_on);

	if (src1_on)
		return true;

	Afe_Set_Reg(GENERAL_ASRC_MODE, src_in_mode << 0, 0xf << 0);
	Afe_Set_Reg(GENERAL_ASRC_MODE, src_out_mode << 4, 0xf << 4);
	if (src_out_freq_mode != SRC_PARAM_INVALID) {
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON3,
			    src_out_freq_mode, 0x00ffffff);
	}
	if (src_in_freq_mode != SRC_PARAM_INVALID) {
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON4,
			    src_in_freq_mode, 0x00ffffff);
	}
	Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON5, 0x003f5986, 0xffffffff);
	Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON5, 0x003f5987, 0xffffffff);
	Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON6, 0x00001fbd, 0xffffffff);
	Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON2, 0x00000000, 0xffffffff);

	if (sample_rate_in > sample_rate_out) {
		const unsigned int *iir_coeff =
			get_iir_coeff(sample_rate_in,
				      sample_rate_out,
				      &iir_coeff_num);

		if (iir_coeff == NULL) {
			pr_debug("%s(), iir_coeff = NULL! Return false\n",
				 __func__);
			return false;
		}
		pr_debug("%s(), iir_coeff_num = %d, iir_coeff = %p\n",
			 __func__, iir_coeff_num, iir_coeff);

		/* COEFF_SRAM_CTRL */
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON0, 0x1 << 1, 0x1 << 1);
		/* Clear coeff history to read/write coef from first position */
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON13, 0x0 << 0, 0x3f << 0);
		/* Write SRC coeff */
		for (coeff_index = 0;
		     coeff_index < iir_coeff_num; coeff_index++) {
			/* Can not use Afe_Set_Reg because it leads another
			 * read and moves the coeff position next
			 */
			Afe_Set_Reg_Val(AFE_GENERAL1_ASRC_2CH_CON12,
					iir_coeff[coeff_index]);
		}
#ifdef DEBUG_COEFF
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON13, 0x0 << 0, 0x3f << 0);
		pr_debug("%s(), set AFE_GENERAL1_ASRC_2CH_CON13[0-5] = 0, AFE_GENERAL1_ASRC_2CH_CON13 = 0x%x\n",
			 __func__, Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON13));
		for (coeff_index = 0;
		     coeff_index < iir_coeff_num; coeff_index++) {
			pr_debug("%s(), coeff_index = %d, AFE_GENERAL1_ASRC_2CH_CON12 = 0x%x\n",
				 __func__, coeff_index,
				 Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON12));
		}
#endif
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON0, 0x0 << 1, 0x1 << 1);
		/* CHSET_IIR_STAGE */
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON2,
			    ((iir_coeff_num / 6) - 1) << 8, 0x7 << 8);
		/* CHSET_IIR_EN */
		Afe_Set_Reg(AFE_GENERAL1_ASRC_2CH_CON2, 0x1 << 11, 0x1 << 11);
	}
	return true;
}

bool set_general_asrc_2_parameter(unsigned int sample_rate_in,
				  unsigned int sample_rate_out)
{
	unsigned int src_in_mode = SampleRateTransform(sample_rate_in, 0);
	unsigned int src_out_mode = SampleRateTransform(sample_rate_out, 0);

	unsigned int src_in_freq_mode =
		src_param[src_in_mode][AUDIO_SRC_PARAM_TYPE_FREQ_MODE];
	unsigned int src_out_freq_mode =
		src_param[src_out_mode][AUDIO_SRC_PARAM_TYPE_FREQ_MODE];

	int coeff_index = 0;
	unsigned int iir_coeff_num = 0;

	pr_debug("%s(), sample_rate_in = %d, sample_rate_out = %d, src2_on = %d\n",
		 __func__, sample_rate_in, sample_rate_out, src2_on);

	if (src2_on)
		return true;

	Afe_Set_Reg(GENERAL_ASRC_MODE, src_in_mode << 8, 0xf << 8);
	Afe_Set_Reg(GENERAL_ASRC_MODE, src_out_mode << 12, 0xf << 12);
	if (src_out_freq_mode != SRC_PARAM_INVALID) {
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON3,
			    src_out_freq_mode, 0x00ffffff);
	}
	if (src_in_freq_mode != SRC_PARAM_INVALID) {
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON4,
			    src_in_freq_mode, 0x00ffffff);
	}
	Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON5, 0x003f5986, 0xffffffff);
	Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON5, 0x003f5987, 0xffffffff);
	Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON6, 0x00001fbd, 0xffffffff);
	Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON2, 0x00000000, 0xffffffff);

	if (sample_rate_in > sample_rate_out) {
		const unsigned int *iir_coeff =
			get_iir_coeff(sample_rate_in,
				      sample_rate_out, &iir_coeff_num);

		if (iir_coeff == NULL) {
			pr_debug("%s(), iir_coeff = NULL! Return false\n",
				 __func__);
			return false;
		}
		pr_debug("%s(), iir_coeff_num = %d, iir_coeff = %p\n",
			 __func__, iir_coeff_num, iir_coeff);

		/* COEFF_SRAM_CTRL */
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON0, 0x1 << 1, 0x1 << 1);
		/* Clear coeff history to read/write coef from first position */
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON13, 0x0 << 0, 0x3f << 0);
		/* Write SRC coeff */
		for (coeff_index = 0;
		     coeff_index < iir_coeff_num; coeff_index++) {
			/* Can not use Afe_Set_Reg because it leads another
			 * read and moves the coeff position next
			 */
			Afe_Set_Reg_Val(AFE_GENERAL2_ASRC_2CH_CON12,
					iir_coeff[coeff_index]);
		}
#ifdef DEBUG_COEFF
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON13, 0x0 << 0, 0x3f << 0);
		pr_debug("%s(), set AFE_GENERAL2_ASRC_2CH_CON13[0-5] = 0, AFE_GENERAL2_ASRC_2CH_CON13 = 0x%x\n",
			 __func__, Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON13));
		for (coeff_index = 0;
		     coeff_index < iir_coeff_num; coeff_index++) {
			pr_debug("%s(), coeff_index = %d, AFE_GENERAL2_ASRC_2CH_CON12 = 0x%x\n",
				 __func__, coeff_index,
				 Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON12));
		}
#endif
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON0, 0x0 << 1, 0x1 << 1);
		/* CHSET_IIR_STAGE */
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON2,
			    ((iir_coeff_num / 6) - 1) << 8, 0x7 << 8);
		/* CHSET_IIR_EN */
		Afe_Set_Reg(AFE_GENERAL2_ASRC_2CH_CON2, 0x1 << 11, 0x1 << 11);
	}
	return true;
}

bool set_chip_general_asrc_parameter(enum audio_general_asrc_id id,
				     unsigned int sample_rate_in,
				     unsigned int sample_rate_out)
{
	bool ret = false;

	pr_debug("%s(), id = %d, sample_rate_in = %d, sample_rate_out = %d\n",
		 __func__, id, sample_rate_in, sample_rate_out);

	switch (id) {
	case AUDIO_GENERAL_ASRC_1:
		ret = set_general_asrc_1_parameter(sample_rate_in,
						   sample_rate_out);
		break;
	case AUDIO_GENERAL_ASRC_2:
		ret = set_general_asrc_2_parameter(sample_rate_in,
						   sample_rate_out);
		break;
	default:
		pr_debug("%s(), No such SRC id = %d\n", __func__, id);
	}
	return ret;
}

bool set_chip_sine_gen_sample_rate(unsigned int sample_rate)
{
	unsigned int sine_mode_ch1 = 0;
	unsigned int sine_mode_ch2 = 0;

	pr_debug("+%s(): sample_rate = %d\n", __func__, sample_rate);
	sine_mode_ch1 = SampleRateTransform(sample_rate, 0) << 8;
	sine_mode_ch2 = SampleRateTransform(sample_rate, 0) << 20;
	Afe_Set_Reg(AFE_SGEN_CON0, sine_mode_ch1, 0xf << 8);
	Afe_Set_Reg(AFE_SGEN_CON0, sine_mode_ch2, 0xf << 20);

	return true;
}

bool set_chip_sine_gen_amplitude(unsigned int amp_divide)
{
	if (amp_divide < Soc_Aud_SGEN_AMP_DIV_128 ||
	    amp_divide > Soc_Aud_SGEN_AMP_DIV_1) {
		pr_warn("%s(): [AudioWarn] amp_divide = %d is invalid\n",
			__func__, amp_divide);
		return false;
	}

	Afe_Set_Reg(AFE_SGEN_CON0, amp_divide << 17, 0x7 << 17);
	Afe_Set_Reg(AFE_SGEN_CON0, amp_divide << 5, 0x7 << 5);
	return true;
}

bool set_chip_afe_enable(bool enable)
{
	if (enable)
		Afe_Set_Reg(AFE_DAC_CON0, 0x1, 0x1);
	else
		Afe_Set_Reg(AFE_DAC_CON0, 0x0, 0x1);
	return true;
}

bool set_chip_dai_bt_enable(bool enable, struct audio_digital_dai_bt *dai_bt,
			    struct audio_mrg_if *mrg)
{
	if (enable == true) {
		/* turn on dai bt */
		Afe_Set_Reg(AFE_DAIBT_CON0,
		dai_bt->mDAI_BT_MODE << 9, 0x1 << 9);
		if (mrg->MrgIf_En == true) {
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);
			/* use merge */
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);
			/* data ready */
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);
			/* Turn on DAIBT */
		} else {	/* turn on merge and daiBT */
			Afe_Set_Reg(AFE_MRGIF_CON,
				mrg->Mrg_I2S_SampleRate << 20,
				0xF00000);
			/* set Mrg_I2S Samping Rate */
			Afe_Set_Reg(AFE_MRGIF_CON, 1 << 16, 1 << 16);
			/* set Mrg_I2S enable */
			Afe_Set_Reg(AFE_MRGIF_CON, 1, 0x1);
			/* Turn on Merge Interface */
			udelay(100);
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 12, 0x1 << 12);
			/* use merge */
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x1 << 3, 0x1 << 3);
			/* data ready */
			Afe_Set_Reg(AFE_DAIBT_CON0, 0x3, 0x3);
			/* Turn on DAIBT */
		}
	} else {
		if (mrg->Mergeif_I2S_Enable == true) {
			Afe_Set_Reg(AFE_DAIBT_CON0, 0, 0x3);
			/* Turn off DAIBT */
		} else {
			Afe_Set_Reg(AFE_DAIBT_CON0, 0, 0x3);
			/* Turn on DAIBT */
			udelay(100);
			Afe_Set_Reg(AFE_MRGIF_CON, 0 << 16, 1 << 16);
			/* set Mrg_I2S enable */
			Afe_Set_Reg(AFE_MRGIF_CON, 0, 0x1);
			/* Turn on Merge Interface */
			mrg->MrgIf_En = false;
		}
		dai_bt->mBT_ON = false;
		dai_bt->mDAIBT_ON = false;
	}
	return true;
}

bool set_chip_hw_digital_gain_mode(enum soc_aud_digital_block aud_block,
				   unsigned int sample_rate,
				   unsigned int sample_per_step)
{
	unsigned int value = 0;

	value = (sample_per_step << 8) |
		(SampleRateTransform(sample_rate, aud_block) << 4);

	switch (aud_block) {
	case Soc_Aud_Digital_Block_HW_GAIN1:
		Afe_Set_Reg(AFE_GAIN1_CON0, value, 0xfff0);
		break;
	case Soc_Aud_Digital_Block_HW_GAIN2:
		Afe_Set_Reg(AFE_GAIN2_CON0, value, 0xfff0);
		break;
	default:
		return false;
	}
	return true;
}

bool set_chip_hw_digital_gain_enable(enum soc_aud_digital_block aud_block,
				     bool enable)
{
	switch (aud_block) {
	case Soc_Aud_Digital_Block_HW_GAIN1:
		if (enable)
			Afe_Set_Reg(AFE_GAIN1_CUR, 0, 0xFFFFFFFF);
		/* Let current gain be 0 to ramp up */
		Afe_Set_Reg(AFE_GAIN1_CON0, enable, 0x1);
		break;
	case Soc_Aud_Digital_Block_HW_GAIN2:
		if (enable)
			Afe_Set_Reg(AFE_GAIN2_CUR, 0, 0xFFFFFFFF);
		/* Let current gain be 0 to ramp up */
		Afe_Set_Reg(AFE_GAIN2_CON0, enable, 0x1);
		break;
	default:
		pr_debug("%s with no match type\n", __func__);
		return false;
	}
	return true;
}

bool set_chip_hw_digital_gain(enum soc_aud_digital_block aud_block,
			      unsigned int gain)
{
	switch (aud_block) {
	case Soc_Aud_Digital_Block_HW_GAIN1:
		Afe_Set_Reg(AFE_GAIN1_CON1, gain, 0xffffffff);
		break;
	case Soc_Aud_Digital_Block_HW_GAIN2:
		Afe_Set_Reg(AFE_GAIN2_CON1, gain, 0xffffffff);
		break;
	default:
		pr_debug("%s with no match type\n", __func__);
		return false;
	}
	return true;
}

bool set_chip_adda_enable(bool enable)
{
	if (enable)
		Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x1, 0x1);
	else
		Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x0, 0x1);
	return true;
}

bool set_chip_ul_src_enable(bool enable)
{
	if (enable)
		Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, 0x1, 0x1);
	else
		Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, 0x0, 0x1);
	return true;
}

bool set_chip_ul2_src_enable(bool enable)
{
	if (enable)
		Afe_Set_Reg(AFE_ADDA6_UL_SRC_CON0, 0x1, 0x1);
	else
		Afe_Set_Reg(AFE_ADDA6_UL_SRC_CON0, 0x0, 0x1);
	return true;
}

bool set_chip_dl_src_enable(bool enable)
{
	if (enable)
		Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, 0x1, 0x1);
	else
		Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, 0x0, 0x1);
	return true;
}

bool set_i2s_dac_out_source(unsigned int aud_block)
{
	int source_sel = 0;

	switch (aud_block) {
	case Soc_Aud_AFE_IO_Block_I2S1_DAC: {
		source_sel = 1; /* select source from o3o4 */
		break;
	}
	case Soc_Aud_AFE_IO_Block_I2S1_DAC_2: {
		source_sel = 0; /* select source from o28o29 */
		break;
	}
	default:
		pr_warn("The source can not be the aud_block = %d\n",
			aud_block);
		return false;
	}
	Afe_Set_Reg(AFE_I2S_CON1, source_sel, 1 << 16);
	return true;
}

bool EnableSideToneFilter(bool stf_on)
{
	/* MD support 16K/32K sampling rate */
	uint8_t kSideToneHalfTapNum;
	const uint16_t *kSideToneCoefficientTable;
	unsigned int eSamplingRate =
		(Afe_Get_Reg(AFE_ADDA_UL_SRC_CON0) & 0x60000) >> 17;

	if (eSamplingRate == Soc_Aud_ADDA_UL_SAMPLERATE_48K) {
		kSideToneHalfTapNum = sizeof(kSideToneCoefficientTable48k) /
			sizeof(uint16_t);
		kSideToneCoefficientTable = kSideToneCoefficientTable48k;
	} else if (eSamplingRate == Soc_Aud_ADDA_UL_SAMPLERATE_32K) {
		kSideToneHalfTapNum = sizeof(kSideToneCoefficientTable32k) /
			sizeof(uint16_t);
		kSideToneCoefficientTable = kSideToneCoefficientTable32k;
	} else {
		kSideToneHalfTapNum = sizeof(kSideToneCoefficientTable16k) /
			sizeof(uint16_t);
		kSideToneCoefficientTable = kSideToneCoefficientTable16k;
	}
	pr_debug("+%s(), stf_on = %d, rate %u, kSTFCoef[0]=0x%x\n",
		 __func__, stf_on, eSamplingRate, kSideToneCoefficientTable[0]);
	AudDrv_Clk_On();

	if (stf_on == false) {
		/* bypass STF result & disable */
		const bool bypass_stf_on = true;
		uint32_t reg_value = (bypass_stf_on << 31) |
				     (bypass_stf_on << 30) |
				     (bypass_stf_on << 29) |
				     (bypass_stf_on << 28) |
				     (stf_on << 8);

		Afe_Set_Reg(AFE_SIDETONE_CON1, reg_value, MASK_ALL);
		/* set side tone gain = 0 */
		Afe_Set_Reg(AFE_SIDETONE_GAIN, 0, MASK_ALL);
	} else {
		const bool bypass_stf_on = false;
		/* using STF result & enable & set half tap num */
		uint32_t write_reg_value = (bypass_stf_on << 31) |
					   (bypass_stf_on << 30) |
					   (bypass_stf_on << 29) |
					   (bypass_stf_on << 28) |
					   (stf_on << 8) |
					   kSideToneHalfTapNum;
		/* set side tone coefficient */
		const bool enable_read_write =
			true;	/* enable read/write side tone coefficient */
		const bool read_write_sel = true; /* for write case */
		const bool sel_ch2 = false; /* using uplink ch1 as STF input */
		uint32_t read_reg_value = Afe_Get_Reg(AFE_SIDETONE_CON0);
		size_t coef_addr = 0;

		/* set side tone gain */
		Afe_Set_Reg(AFE_SIDETONE_GAIN, 0, MASK_ALL);
		Afe_Set_Reg(AFE_SIDETONE_CON1, write_reg_value, MASK_ALL);
		pr_debug("%s(), AFE_SIDETONE_CON1[0x%lx] = 0x%x\n",
			 __func__, AFE_SIDETONE_CON1,
			 write_reg_value);

		for (coef_addr = 0;
		     coef_addr < kSideToneHalfTapNum;
		     coef_addr++) {
			bool old_write_ready = (read_reg_value >> 29) & 0x1;
			bool new_write_ready = 0;
			int try_cnt = 0;

			write_reg_value = enable_read_write << 25 |
					  read_write_sel << 24 |
					  sel_ch2 << 23 |
					  coef_addr << 16 |
					  kSideToneCoefficientTable[coef_addr];
			Afe_Set_Reg(AFE_SIDETONE_CON0,
				    write_reg_value, 0x39FFFFF);

			/* wait flag write_ready changed (means write done) */
			for (try_cnt = 0; try_cnt < 10; try_cnt++) {
				/* max try 10 times */
				/* msleep(3); */
				/* usleep_range(3 * 1000, 20 * 1000); */
				read_reg_value = Afe_Get_Reg(AFE_SIDETONE_CON0);
				new_write_ready = (read_reg_value >> 29) & 0x1;
				if (new_write_ready == old_write_ready) {
					/* flip => ok */
					udelay(3);
					if (try_cnt == 9) {
						AUDIO_AEE("stf not ready");
						AudDrv_Clk_Off();
						return false;
					}
				} else {
					break;
				}

			}
		}

	}

	AudDrv_Clk_Off();

	return true;
}

void set_stf_gain(int gain)
{
	AudDrv_Clk_On();
	Afe_Set_Reg(AFE_SIDETONE_GAIN, gain, 0xffff);
	AudDrv_Clk_Off();
}

void set_stf_positive_gain_db(int gain_db)
{
	if (gain_db >= 0 && gain_db <= 24) {
		AudDrv_Clk_On();
		Afe_Set_Reg(AFE_SIDETONE_GAIN, (gain_db / 6) << 16, 0x7 << 16);
		AudDrv_Clk_Off();
	} else {
		pr_warn("%s(), gain_db %d invalid\n", __func__, gain_db);
	}
}

bool CleanPreDistortion(void)
{
	Afe_Set_Reg(AFE_ADDA_PREDIS_CON0, 0, MASK_ALL);
	Afe_Set_Reg(AFE_ADDA_PREDIS_CON1, 0, MASK_ALL);

	return false;
}

static void set_adda_dl_src_gain(bool mute)
{
	if (mute) {
		Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON1, 0, MASK_ALL);
	} else {
		/* SA suggest apply -0.3db to audio/speech path */
		/* 2013.02.22 for voice mode degrade 0.3 db */
		Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON1, 0xf74f0000, MASK_ALL);
	}
}

bool SetDLSrc2(unsigned int rate)
{
	unsigned int dl_src2_con0 = 0;
	unsigned int rate_idx;

	rate_idx = SampleRateTransform(rate,
				       Soc_Aud_Digital_Block_ADDA_DL);

	dl_src2_con0 |= rate_idx << 28;

	/* turn off mute function */
	dl_src2_con0 |= 0x03 << 11;

	/* set output mode */
	if (rate == 96000) {
		dl_src2_con0 |= (0x2 << 24);	/* UP_SAMPLING_RATE_X4 */
		dl_src2_con0 |= 1 << 14;
	} else if (rate == 192000) {
		dl_src2_con0 |= (0x1 << 24);	/* UP_SAMPLING_RATE_X2 */
		dl_src2_con0 |= 1 << 14;
	} else {
		dl_src2_con0 |= (0x3 << 24);	/* UP_SAMPLING_RATE_X8 */
	}

	/* 8k or 16k voice mode */
	if (rate == 8000 || rate == 16000)
		dl_src2_con0 |= 0x01 << 5;

	dl_src2_con0 |= 0x01 << 1;
	Afe_Set_Reg(AFE_ADDA_DL_SRC2_CON0, dl_src2_con0, MASK_ALL);

	set_adda_dl_src_gain(false);

	SetSdmLevel(AUDIO_SDM_LEVEL_NORMAL);

	return true;
}

enum {
	UL_IIR_SW = 0,
	/* 3.3 Hz if 32kHz mode ; 5 Hz if 48kHz mode ; 10Hz if 96kHz mode */
	UL_IIR_HW_LEVEL1,
	/* 6.6 Hz if 32kHz mode ; 10Hz if 48kHz mode ; 20Hz if 96kHz mode */
	UL_IIR_HW_LEVEL2,
	/* 16.6 Hz if 32kHz mode ; 25Hz if 48kHz mode ; 50Hz if 96kHz mode */
	UL_IIR_HW_LEVEL3,
	/* 33.3 Hz if 32kHz mode ; 50Hz if 48kHz mode ; 100Hz if 96kHz mode */
	UL_IIR_HW_LEVEL4,
	/* 50 Hz if 32kHz mode ; 75Hz if 48kHz mode ; 150Hz if 96kHz mode */
	UL_IIR_HW_LEVEL5,
};

bool set_chip_adc_in(unsigned int rate)
{
	unsigned int voice_mode;
	unsigned int enable_iir = 0;
	unsigned int ul_src_con0 = 0;	/* default value */

	/* enable aud_pad_top fifo, set after GPIO enable, pmic miso clk on */
	Afe_Set_Reg(AFE_AUD_PAD_TOP_CFG, 0x31, MASK_ALL);

	/* reset MTKAIF_CFG0, no lpbk, protocol 1 */
	Afe_Set_Reg(AFE_ADDA_MTKAIF_CFG0, 0x0, MASK_ALL);

	/* Using Internal ADC */
	Afe_Set_Reg(AFE_ADDA_TOP_CON0, 0, 0x1);

	voice_mode = SampleRateTransform(rate, Soc_Aud_Digital_Block_ADDA_UL);

	ul_src_con0 |= (voice_mode << 17) & (0x7 << 17);
	ul_src_con0 |= (enable_iir << 10) & (0x1 << 10);
	ul_src_con0 |= UL_IIR_HW_LEVEL1 << 7;

	Afe_Set_Reg(AFE_ADDA_UL_SRC_CON0, ul_src_con0, MASK_ALL);

	/* mtkaif_rxif_data_mode = 0, amic */
	Afe_Set_Reg(AFE_ADDA_MTKAIF_RX_CFG0, 0x0, 0x1);

	return true;
}

bool set_chip_adc2_in(unsigned int rate)
{
	return true;
}

bool setChipDmicPath(bool _enable, unsigned int sample_rate)
{
	return true;
}

bool SetSampleRate(unsigned int Aud_block, unsigned int SampleRate)
{
	SampleRate = SampleRateTransform(SampleRate, Aud_block);

	switch (Aud_block) {
	case Soc_Aud_Digital_Block_MEM_DL1:
	case Soc_Aud_Digital_Block_MEM_DL1_DATA2:
	case Soc_Aud_Digital_Block_MEM_DL2:
	case Soc_Aud_Digital_Block_MEM_DL3:
	case Soc_Aud_Digital_Block_MEM_I2S:
	case Soc_Aud_Digital_Block_MEM_AWB:
	case Soc_Aud_Digital_Block_MEM_VUL:
	case Soc_Aud_Digital_Block_MEM_DAI:
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
	case Soc_Aud_Digital_Block_MEM_AWB2:
	case Soc_Aud_Digital_Block_MEM_VUL2:
		Afe_Set_Reg(mMemIfSampleRate[Aud_block][0],
			    SampleRate << mMemIfSampleRate[Aud_block][1],
			    mMemIfSampleRate[Aud_block][2] <<
			    mMemIfSampleRate[Aud_block][1]);
		break;
	default:
		pr_err("audio_error: %s(): given Aud_block is not valid!!!!\n",
			__func__);
		return false;
	}
	return true;
}


bool SetChannels(unsigned int Memory_Interface, unsigned int channel)
{
	const bool bMono = (channel == 1) ? true : false;

	switch (Memory_Interface) {
	case Soc_Aud_Digital_Block_MEM_DL1:
	case Soc_Aud_Digital_Block_MEM_DL2:
	case Soc_Aud_Digital_Block_MEM_DL3:
	case Soc_Aud_Digital_Block_MEM_AWB:
	case Soc_Aud_Digital_Block_MEM_VUL:
	case Soc_Aud_Digital_Block_MEM_AWB2:
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
	case Soc_Aud_Digital_Block_MEM_VUL2:
		Afe_Set_Reg(mMemIfChannels[Memory_Interface][0],
			    bMono << mMemIfChannels[Memory_Interface][1],
			    mMemIfChannels[Memory_Interface][2] <<
			    mMemIfChannels[Memory_Interface][1]);
		break;
	case Soc_Aud_Digital_Block_MEM_DAI:
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
		SetMemDuplicateWrite(Memory_Interface, channel == 2 ? 1 : 0);
		break;
	default:
		pr_warn("[AudioWarn] %s(),  Memory_Interface = %d, channel = %d, bMono = %d\n",
			__func__, Memory_Interface, channel, bMono);
		return false;
	}
	return true;
}

int SetMemifMonoSel(unsigned int Memory_Interface, bool mono_use_r_ch)
{
	switch (Memory_Interface) {
	case Soc_Aud_Digital_Block_MEM_AWB:
	case Soc_Aud_Digital_Block_MEM_VUL:
	case Soc_Aud_Digital_Block_MEM_AWB2:
	case Soc_Aud_Digital_Block_MEM_VUL2:
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
		Afe_Set_Reg(mMemIfMonoChSelect[Memory_Interface][0],
			    mono_use_r_ch <<
			    mMemIfMonoChSelect[Memory_Interface][1],
			    mMemIfMonoChSelect[Memory_Interface][2] <<
			    mMemIfMonoChSelect[Memory_Interface][1]);
		break;
	default:
		pr_warn("[AudioWarn] %s(), invalid Memory_Interface = %d\n",
			__func__, Memory_Interface);
		return -EINVAL;
	}
	return 0;
}

bool SetMemDuplicateWrite(unsigned int InterfaceType, int dupwrite)
{
	switch (InterfaceType) {
	case Soc_Aud_Digital_Block_MEM_DAI:
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
		Afe_Set_Reg(mMemDuplicateWrite[InterfaceType][0],
			    dupwrite << mMemDuplicateWrite[InterfaceType][1],
			    mMemDuplicateWrite[InterfaceType][2] <<
			    mMemDuplicateWrite[InterfaceType][1]);
		break;
	default:
		return false;
	}

	return true;
}

unsigned int GetEnableAudioBlockRegInfo(unsigned int Aud_block, int index)
{
	int i = 0;

	for (i = 0; i < MEM_BLOCK_ENABLE_REG_NUM; i++) {
		if (mMemAudioBlockEnableReg[i][MEM_EN_REG_IDX_AUDIO_BLOCK] ==
		    Aud_block)
			return mMemAudioBlockEnableReg[i][index];
	}
	return 0; /* 0: no such bit */
}

unsigned int GetEnableAudioBlockRegAddr(unsigned int Aud_block)
{
	return GetEnableAudioBlockRegInfo(Aud_block,
		MEM_EN_REG_IDX_REG);
}

unsigned int GetEnableAudioBlockRegOffset(unsigned int Aud_block)
{
	return GetEnableAudioBlockRegInfo(Aud_block, MEM_EN_REG_IDX_OFFSET);
}

bool SetMemIfFormatReg(unsigned int InterfaceType, unsigned int eFetchFormat)
{
	unsigned int isAlign =
		eFetchFormat == AFE_WLEN_32_BIT_ALIGN_24BIT_DATA_8BIT_0 ?
			       1 : 0;
	unsigned int isHD = eFetchFormat == AFE_WLEN_16_BIT ? 0 : 1;

	/* force cpu use 8_24 format when writing 32bit data */
	Afe_Set_Reg(AFE_MEMIF_MSB, 0 << 28,
		    1 << 28);	/* TODO: KC: force use 8_24 format */

	switch (InterfaceType) {
	case Soc_Aud_Digital_Block_MEM_DL1:
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, isAlign << 0, 1 << 0);
		Afe_Set_Reg(AFE_MEMIF_HD_MODE, isHD    << 0, 3 << 0);
		break;
	case Soc_Aud_Digital_Block_MEM_DL1_DATA2:
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, isAlign << 1, 1 << 1);
		Afe_Set_Reg(AFE_MEMIF_HD_MODE, isHD    << 2, 3 << 2);
		break;
	case Soc_Aud_Digital_Block_MEM_DL2:
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, isAlign << 2, 1 << 2);
		Afe_Set_Reg(AFE_MEMIF_HD_MODE, isHD    << 4, 3 << 4);
		break;
	case Soc_Aud_Digital_Block_MEM_DL3:
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, isAlign << 3, 1 << 3);
		Afe_Set_Reg(AFE_MEMIF_HD_MODE, isHD    << 6, 3 << 6);
		break;
	case Soc_Aud_Digital_Block_MEM_AWB:
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, isAlign << 4, 1 << 4);
		Afe_Set_Reg(AFE_MEMIF_HD_MODE, isHD    << 8, 3 << 8);
		break;
	case Soc_Aud_Digital_Block_MEM_VUL:
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, isAlign << 5, 1 << 5);
		Afe_Set_Reg(AFE_MEMIF_HD_MODE, isHD    << 10, 3 << 10);
		break;
	case Soc_Aud_Digital_Block_MEM_AWB2:
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, isAlign << 14, 1 << 14);
		Afe_Set_Reg(AFE_MEMIF_HD_MODE, isHD    << 28, 3 << 28);
		break;
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, isAlign << 6, 1 << 6);
		Afe_Set_Reg(AFE_MEMIF_HD_MODE, isHD    << 12, 3 << 12);
		break;
	case Soc_Aud_Digital_Block_MEM_VUL2:
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, isAlign << 7, 1 << 7);
		Afe_Set_Reg(AFE_MEMIF_HD_MODE, isHD    << 14, 3 << 14);
		break;
	case Soc_Aud_Digital_Block_MEM_DAI:
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, isAlign << 8, 1 << 8);
		Afe_Set_Reg(AFE_MEMIF_HD_MODE, isHD    << 16, 3 << 16);
		break;
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, isAlign << 9, 1 << 9);
		Afe_Set_Reg(AFE_MEMIF_HD_MODE, isHD    << 18, 3 << 18);
		break;
	case Soc_Aud_Digital_Block_MEM_HDMI:
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, isAlign << 10, 1 << 10);
		Afe_Set_Reg(AFE_MEMIF_HD_MODE, isHD    << 20, 3 << 20);
		break;
	default:
		pr_warn("%s(), InterfaceType %u not supported\n",
			__func__, InterfaceType);
		return false;
	}

	return true;
}

ssize_t AudDrv_Reg_Dump(char *buffer, int size)
{
	int n = 0;

	pr_debug("mt_soc_debug_read\n");
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON0 = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON1 = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_CON1));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON3 = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_DAC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_DAC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON = 0x%x\n",
		       Afe_Get_Reg(AFE_I2S_CON));
	n += scnprintf(buffer + n, size - n, "AFE_DAIBT_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_DAIBT_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN0 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN2 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN2));
	n += scnprintf(buffer + n, size - n, "AFE_CONN3 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN3));
	n += scnprintf(buffer + n, size - n, "AFE_CONN4 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN4));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_I2S_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_I2S_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_CON = 0x%x\n",
		       Afe_Get_Reg(AFE_MRGIF_CON));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_BASE = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_CUR = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_END = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_END));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON3 = 0x%x\n",
		       Afe_Get_Reg(AFE_I2S_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_BASE = 0x%x\n",
		       Afe_Get_Reg(AFE_DL2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_CUR = 0x%x\n",
		       Afe_Get_Reg(AFE_DL2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_END = 0x%x\n",
		       Afe_Get_Reg(AFE_DL2_END));
	n += scnprintf(buffer + n, size - n, "AFE_CONN5 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN5));
	n += scnprintf(buffer + n, size - n, "AFE_CONN_24BIT = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN_24BIT));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_BASE = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_END = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_CUR = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_BASE = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_END = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_CUR = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_CONN6 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN6));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON3 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON3));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON4 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON4));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON5 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON5));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON6 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON6));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON7 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON7));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON8 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON8));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON9 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON9));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC2_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_UL_SRC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_UL_SRC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_TOP_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_DL_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_SRC_DEBUG));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_SRC_DEBUG_MON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_UL_SRC_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_UL_SRC_MON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_UL_SRC_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_SIDETONE_DEBUG = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_DEBUG));
	n += scnprintf(buffer + n, size - n, "AFE_SIDETONE_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_MON));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_SGEN_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_SIDETONE_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_SIDETONE_COEFF = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_COEFF));
	n += scnprintf(buffer + n, size - n, "AFE_SIDETONE_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_SIDETONE_GAIN = 0x%x\n",
		       Afe_Get_Reg(AFE_SIDETONE_GAIN));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_SGEN_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_BUS_CFG = 0x%x\n",
		       Afe_Get_Reg(AFE_BUS_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_BUS_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_BUS_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_PREDIS_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_PREDIS_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_MRGIF_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_MRGIF_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_MRGIF_MON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_MRGIF_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_I2S_MON));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_IIR_COEF_02_01 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_IIR_COEF_02_01));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_IIR_COEF_04_03 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_IIR_COEF_04_03));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_IIR_COEF_06_05 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_IIR_COEF_06_05));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_IIR_COEF_08_07 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_IIR_COEF_08_07));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_IIR_COEF_10_09 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_IIR_COEF_10_09));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_CON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_DAC_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_DAC_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_DAC_MON));
	n += scnprintf(buffer + n, size - n, "AFE_VUL2_BASE = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL2_END = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL2_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL2_CUR = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT0 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT0));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT6 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT6));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_EN1 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_EN1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ0_MCU_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ0_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ6_MCU_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ6_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_MOD_DAI_BASE = 0x%x\n",
		       Afe_Get_Reg(AFE_MOD_DAI_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_MOD_DAI_END = 0x%x\n",
		       Afe_Get_Reg(AFE_MOD_DAI_END));
	n += scnprintf(buffer + n, size - n, "AFE_MOD_DAI_CUR = 0x%x\n",
		       Afe_Get_Reg(AFE_MOD_DAI_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_BASE = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_D2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_END = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_D2_END));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_CUR = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_D2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL3_BASE = 0x%x\n",
		       Afe_Get_Reg(AFE_DL3_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_DL3_CUR = 0x%x\n",
		       Afe_Get_Reg(AFE_DL3_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_DL3_END = 0x%x\n",
		       Afe_Get_Reg(AFE_DL3_END));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_OUT_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_OUT_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_OUT_BASE = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_OUT_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_OUT_CUR = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_OUT_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_OUT_END = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_OUT_END));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ3_MCU_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ3_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ4_MCU_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ4_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_STATUS = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_STATUS));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CLR = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CLR));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT1 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT2 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_EN = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_EN));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_MON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT5 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT5));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_MCU_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ1_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ2_MCU_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ2_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ1_MCU_EN_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ1_MCU_EN_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ5_MCU_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ5_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MINLEN = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MINLEN));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MAXLEN = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MAXLEN));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_PBUF_SIZE = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_PBUF_SIZE));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT7 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT7));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ7_MCU_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ7_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT3 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT3));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT4 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT4));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT11 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT11));
	n += scnprintf(buffer + n, size - n, "AFE_APLL1_TUNER_CFG = 0x%x\n",
		       Afe_Get_Reg(AFE_APLL1_TUNER_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_APLL2_TUNER_CFG = 0x%x\n",
		       Afe_Get_Reg(AFE_APLL2_TUNER_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_HD_MODE = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_HD_MODE));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_HDALIGN = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_HDALIGN));
	n += scnprintf(buffer + n, size - n, "AFE_CONN33 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN33));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ_MCU_CNT12 = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ_MCU_CNT12));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CON3 = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_CONN7 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN7));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN1_CUR = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN1_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CON3 = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_CONN8 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN8));
	n += scnprintf(buffer + n, size - n, "AFE_GAIN2_CUR = 0x%x\n",
		       Afe_Get_Reg(AFE_GAIN2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_CONN9 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN9));
	n += scnprintf(buffer + n, size - n, "AFE_CONN10 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN10));
	n += scnprintf(buffer + n, size - n, "AFE_CONN11 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN11));
	n += scnprintf(buffer + n, size - n, "AFE_CONN12 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN12));
	n += scnprintf(buffer + n, size - n, "AFE_CONN13 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN13));
	n += scnprintf(buffer + n, size - n, "AFE_CONN14 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN14));
	n += scnprintf(buffer + n, size - n, "AFE_CONN15 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN15));
	n += scnprintf(buffer + n, size - n, "AFE_CONN16 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN16));
	n += scnprintf(buffer + n, size - n, "AFE_CONN17 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN17));
	n += scnprintf(buffer + n, size - n, "AFE_CONN18 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN18));
	n += scnprintf(buffer + n, size - n, "AFE_CONN19 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN19));
	n += scnprintf(buffer + n, size - n, "AFE_CONN20 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN20));
	n += scnprintf(buffer + n, size - n, "AFE_CONN21 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN21));
	n += scnprintf(buffer + n, size - n, "AFE_CONN22 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN22));
	n += scnprintf(buffer + n, size - n, "AFE_CONN23 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN23));
	n += scnprintf(buffer + n, size - n, "AFE_CONN24 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN24));
	n += scnprintf(buffer + n, size - n, "AFE_CONN_RS = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN_RS));
	n += scnprintf(buffer + n, size - n, "AFE_CONN_DI = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN_DI));
	n += scnprintf(buffer + n, size - n, "AFE_CONN25 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN25));
	n += scnprintf(buffer + n, size - n, "AFE_CONN26 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN26));
	n += scnprintf(buffer + n, size - n, "AFE_CONN27 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN27));
	n += scnprintf(buffer + n, size - n, "AFE_CONN28 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN28));
	n += scnprintf(buffer + n, size - n, "AFE_CONN29 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN29));
	n += scnprintf(buffer + n, size - n, "AFE_CONN30 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN30));
	n += scnprintf(buffer + n, size - n, "AFE_CONN31 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN31));
	n += scnprintf(buffer + n, size - n, "AFE_CONN32 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN32));
	n += scnprintf(buffer + n, size - n, "AFE_SRAM_DELSEL_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_SRAM_DELSEL_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_SRAM_DELSEL_CON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_SRAM_DELSEL_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_SRAM_DELSEL_CON3 = 0x%x\n",
		       Afe_Get_Reg(AFE_SRAM_DELSEL_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_2CH_CON12 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_2CH_CON12));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_2CH_CON13 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_2CH_CON13));
	n += scnprintf(buffer + n, size - n, "PCM_INTF_CON1 = 0x%x\n",
		       Afe_Get_Reg(PCM_INTF_CON1));
	n += scnprintf(buffer + n, size - n, "PCM_INTF_CON2 = 0x%x\n",
		       Afe_Get_Reg(PCM_INTF_CON2));
	n += scnprintf(buffer + n, size - n, "PCM2_INTF_CON = 0x%x\n",
		       Afe_Get_Reg(PCM2_INTF_CON));
	n += scnprintf(buffer + n, size - n, "AFE_TDM_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_TDM_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_TDM_CON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_TDM_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_CONN34 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN34));
	n += scnprintf(buffer + n, size - n, "FPGA_CFG0 = 0x%x\n",
		       Afe_Get_Reg(FPGA_CFG0));
	n += scnprintf(buffer + n, size - n, "FPGA_CFG1 = 0x%x\n",
		       Afe_Get_Reg(FPGA_CFG1));
	n += scnprintf(buffer + n, size - n, "FPGA_CFG2 = 0x%x\n",
		       Afe_Get_Reg(FPGA_CFG2));
	n += scnprintf(buffer + n, size - n, "FPGA_CFG3 = 0x%x\n",
		       Afe_Get_Reg(FPGA_CFG3));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_DBG_CON = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_DBG_CON));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_DBG_MON0 = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_DBG_MON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_DBG_MON1 = 0x%x\n",
		       Afe_Get_Reg(AUDIO_TOP_DBG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ8_MCU_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ8_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ11_MCU_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ11_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_IRQ12_MCU_CNT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_IRQ12_MCU_CNT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG0 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG0));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG1 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG1));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG2 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG2));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG3 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG3));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG4 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG4));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG5 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG5));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG6 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG6));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG7 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG7));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG8 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG8));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG9 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG9));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG10 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG10));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG11 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG11));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG12 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG12));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG13 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG13));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG14 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG14));
	n += scnprintf(buffer + n, size - n, "AFE_GENERAL_REG15 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL_REG15));
	n += scnprintf(buffer + n, size - n, "AFE_CBIP_CFG0 = 0x%x\n",
		       Afe_Get_Reg(AFE_CBIP_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_CBIP_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_CBIP_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_CBIP_SLV_MUX_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_CBIP_SLV_MUX_MON0));
	n += scnprintf(buffer + n, size - n,
		"AFE_CBIP_SLV_DECODER_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_CBIP_SLV_DECODER_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_CONN0_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN0_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN1_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN1_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN2_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN2_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN3_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN3_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN4_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN4_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN5_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN5_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN6_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN6_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN7_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN7_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN8_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN8_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN9_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN9_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN10_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN10_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN11_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN11_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN12_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN12_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN13_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN13_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN14_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN14_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN15_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN15_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN16_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN16_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN17_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN17_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN18_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN18_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN19_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN19_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN20_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN20_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN21_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN21_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN22_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN22_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN23_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN23_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN24_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN24_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN25_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN25_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN26_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN26_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN27_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN27_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN28_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN28_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN29_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN29_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN30_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN30_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN31_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN31_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN32_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN32_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN33_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN33_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN34_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN34_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN_RS_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN_RS_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN_DI_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN_DI_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN_24BIT_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN_24BIT_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN_REG = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN_REG));
	n += scnprintf(buffer + n, size - n, "AFE_CONN35 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN35));
	n += scnprintf(buffer + n, size - n, "AFE_CONN36 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN36));
	n += scnprintf(buffer + n, size - n, "AFE_CONN37 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN37));
	n += scnprintf(buffer + n, size - n, "AFE_CONN38 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN38));
	n += scnprintf(buffer + n, size - n, "AFE_CONN35_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN35_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN36_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN36_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN37_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN37_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN38_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN38_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN39 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN39));
	n += scnprintf(buffer + n, size - n, "AFE_CONN40 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN40));
	n += scnprintf(buffer + n, size - n, "AFE_CONN41 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN41));
	n += scnprintf(buffer + n, size - n, "AFE_CONN42 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN42));
	n += scnprintf(buffer + n, size - n, "AFE_CONN39_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN39_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN40_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN40_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN41_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN41_1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN42_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN42_1));
	n += scnprintf(buffer + n, size - n, "AFE_I2S_CON4 = 0x%x\n",
		       Afe_Get_Reg(AFE_I2S_CON4));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA6_TOP_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA6_UL_SRC_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_UL_SRC_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADD6_UL_SRC_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADD6_UL_SRC_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA6_SRC_DEBUG = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_SRC_DEBUG));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_SRC_DEBUG_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_SRC_DEBUG_MON0));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_02_01 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_02_01));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_04_03 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_04_03));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_06_05 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_06_05));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_08_07 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_08_07));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_10_09 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_10_09));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_12_11 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_12_11));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_14_13 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_14_13));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_16_15 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_16_15));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_18_17 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_18_17));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_20_19 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_20_19));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_22_21 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_22_21));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_24_23 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_24_23));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_26_25 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_26_25));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_28_27 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_28_27));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_ULCF_CFG_30_29 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_ULCF_CFG_30_29));
	n += scnprintf(buffer + n, size - n, "AFE_ADD6A_UL_SRC_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADD6A_UL_SRC_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA6_UL_SRC_MON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_UL_SRC_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_CONN43 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN43));
	n += scnprintf(buffer + n, size - n, "AFE_CONN43_1 = 0x%x\n",
		       Afe_Get_Reg(AFE_CONN43_1));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_BASE_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_BASE_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_CUR_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_CUR_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_DL1_END_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_DL1_END_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_BASE_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_DL2_BASE_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_CUR_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_DL2_CUR_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_DL2_END_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_DL2_END_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_BASE_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB_BASE_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_END_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB_END_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_AWB_CUR_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB_CUR_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_BASE_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_BASE_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_END_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_END_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_CUR_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_CUR_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_VUL2_BASE_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL2_BASE_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_VUL2_END_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL2_END_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_VUL2_CUR_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL2_CUR_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_MOD_DAI_BASE_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_MOD_DAI_BASE_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_MOD_DAI_END_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_MOD_DAI_END_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_MOD_DAI_CUR_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_MOD_DAI_CUR_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_BASE_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_D2_BASE_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_END_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_D2_END_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_VUL_D2_CUR_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_VUL_D2_CUR_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_DL3_BASE_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_DL3_BASE_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_DL3_CUR_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_DL3_CUR_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_DL3_END_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_DL3_END_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_OUT_BASE_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_OUT_BASE_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_OUT_CUR_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_OUT_CUR_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_HDMI_OUT_END_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_HDMI_OUT_END_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_AWB2_BASE = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB2_BASE));
	n += scnprintf(buffer + n, size - n, "AFE_AWB2_END = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB2_END));
	n += scnprintf(buffer + n, size - n, "AFE_AWB2_CUR = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB2_CUR));
	n += scnprintf(buffer + n, size - n, "AFE_AWB2_BASE_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB2_BASE_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_AWB2_END_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB2_END_MSB));
	n += scnprintf(buffer + n, size - n, "AFE_AWB2_CUR_MSB = 0x%x\n",
		       Afe_Get_Reg(AFE_AWB2_CUR_MSB));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SDM_DCCOMP_CON = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_SDM_DCCOMP_CON));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SDM_TEST = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_SDM_TEST));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_DC_COMP_CFG0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_DC_COMP_CFG0));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_DC_COMP_CFG1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_DC_COMP_CFG1));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA_DL_SDM_FIFO_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_SDM_FIFO_MON));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC_LCH_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_SRC_LCH_MON));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SRC_RCH_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_SRC_RCH_MON));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_DL_SDM_OUT_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_DL_SDM_OUT_MON));
	n += scnprintf(buffer + n, size - n, "AFE_CONNSYS_I2S_CON = 0x%x\n",
		       Afe_Get_Reg(AFE_CONNSYS_I2S_CON));
	n += scnprintf(buffer + n, size - n, "AFE_CONNSYS_I2S_MON = 0x%x\n",
		       Afe_Get_Reg(AFE_CONNSYS_I2S_MON));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CONNSYS_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CONNSYS_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_2CH_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_2CH_CON1));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CONNSYS_CON13 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CONNSYS_CON13));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CONNSYS_CON14 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CONNSYS_CON14));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CONNSYS_CON15 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CONNSYS_CON15));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CONNSYS_CON16 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CONNSYS_CON16));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CONNSYS_CON17 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CONNSYS_CON17));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CONNSYS_CON18 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CONNSYS_CON18));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CONNSYS_CON19 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CONNSYS_CON19));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CONNSYS_CON20 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CONNSYS_CON20));
	n += scnprintf(buffer + n, size - n, "AFE_ASRC_CONNSYS_CON21 = 0x%x\n",
		       Afe_Get_Reg(AFE_ASRC_CONNSYS_CON21));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_IIR_COEF_02_01 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_IIR_COEF_02_01));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_IIR_COEF_04_03 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_IIR_COEF_04_03));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_IIR_COEF_06_05 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_IIR_COEF_06_05));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_IIR_COEF_08_07 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_IIR_COEF_08_07));
	n += scnprintf(buffer + n, size - n,
		"AFE_ADDA6_IIR_COEF_10_09 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA6_IIR_COEF_10_09));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_PREDIS_CON2));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_PREDIS_CON3 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_PREDIS_CON3));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON12 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON12));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON13 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON13));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON14 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON14));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON15 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON15));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON16 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON16));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON17 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON17));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON18 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON18));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON19 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON19));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON20 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON20));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON21 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON21));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON22 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON22));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON23 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON23));
	n += scnprintf(buffer + n, size - n, "AFE_MEMIF_MON24 = 0x%x\n",
		       Afe_Get_Reg(AFE_MEMIF_MON24));
	n += scnprintf(buffer + n, size - n, "AFE_HD_ENGEN_ENABLE = 0x%x\n",
		       Afe_Get_Reg(AFE_HD_ENGEN_ENABLE));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_CFG0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_MTKAIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_TX_CFG1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_MTKAIF_TX_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_RX_CFG0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_MTKAIF_RX_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_RX_CFG1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_MTKAIF_RX_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_RX_CFG2 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_MTKAIF_RX_CFG2));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_MON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_MTKAIF_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_MON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_ADDA_MTKAIF_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_AUD_PAD_TOP_CFG = 0x%x\n",
		       Afe_Get_Reg(AFE_AUD_PAD_TOP_CFG));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON0));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON1));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON2));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON3 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON3));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON4 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON4));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON5 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON5));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON6 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON6));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON7 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON7));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON8 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON8));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON9 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON9));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON10 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON10));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON12 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON12));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL1_ASRC_2CH_CON13 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL1_ASRC_2CH_CON13));
	n += scnprintf(buffer + n, size - n, "GENERAL_ASRC_MODE = 0x%x\n",
		       Afe_Get_Reg(GENERAL_ASRC_MODE));
	n += scnprintf(buffer + n, size - n, "GENERAL_ASRC_EN_ON = 0x%x\n",
		       Afe_Get_Reg(GENERAL_ASRC_EN_ON));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON0 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON0));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON1 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON1));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON2 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON2));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON3 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON3));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON4 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON4));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON5 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON5));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON6 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON6));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON7 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON7));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON8 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON8));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON9 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON9));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON10 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON10));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON12 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON12));
	n += scnprintf(buffer + n, size - n,
		"AFE_GENERAL2_ASRC_2CH_CON13 = 0x%x\n",
		       Afe_Get_Reg(AFE_GENERAL2_ASRC_2CH_CON13));

	return n;
}

bool SetFmI2sConnection(unsigned int ConnectionState)
{
	SetIntfConnection(ConnectionState,
			  Soc_Aud_AFE_IO_Block_I2S_CONNSYS,
			  Soc_Aud_AFE_IO_Block_HW_GAIN1_OUT);
	SetIntfConnection(ConnectionState,
			  Soc_Aud_AFE_IO_Block_HW_GAIN1_IN,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	SetIntfConnection(ConnectionState,
			  Soc_Aud_AFE_IO_Block_HW_GAIN1_IN,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
	SetIntfConnection(ConnectionState,
			  Soc_Aud_AFE_IO_Block_HW_GAIN1_IN,
			  Soc_Aud_AFE_IO_Block_I2S3);
	return true;
}

bool SetFmAwbConnection(unsigned int ConnectionState)
{
	SetIntfConnection(ConnectionState,
			  Soc_Aud_AFE_IO_Block_I2S_CONNSYS,
			  Soc_Aud_AFE_IO_Block_MEM_VUL2);
	return true;
}

int SetFmI2sInEnable(bool enable)
{
	return setConnsysI2SInEnable(enable);
}

int SetFmI2sIn(struct audio_digital_i2s *mDigitalI2S)
{
	return setConnsysI2SIn(mDigitalI2S);
}

bool GetFmI2sInPathEnable(void)
{
	return GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_CONNSYS);
}

bool SetFmI2sInPathEnable(bool bEnable)
{
	return SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_IN_CONNSYS,
		bEnable);
}

int SetFmI2sAsrcEnable(bool enable)
{
	return setConnsysI2SEnable(enable);
}

int SetFmI2sAsrcConfig(bool bIsUseASRC, unsigned int dToSampleRate)
{
	return setConnsysI2SAsrc(bIsUseASRC, dToSampleRate);
}

bool SetAncRecordReg(unsigned int value, unsigned int mask)
{
	return false;
}

const struct Aud_IRQ_CTRL_REG *GetIRQCtrlReg(enum Soc_Aud_IRQ_MCU_MODE irqIndex)
{
	return &mIRQCtrlRegs[irqIndex];
}

const struct Aud_RegBitsInfo *GetIRQPurposeReg(enum Soc_Aud_IRQ_PURPOSE
		irqPurpose)
{
	return &mIRQPurposeRegs[irqPurpose];
}

const unsigned int GetBufferCtrlReg(enum soc_aud_afe_io_block memif_type,
				    enum aud_buffer_ctrl_info buffer_ctrl)
{
	if (!afe_buffer_regs[memif_type][buffer_ctrl])
		pr_warn("%s, invalid afe_buffer_regs, memif: %d, buffer_ctrl: %d",
			__func__, memif_type, buffer_ctrl);

	return afe_buffer_regs[memif_type][buffer_ctrl];
}

/*Irq handler function array*/
static void Aud_IRQ1_Handler(void)
{
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) &&
	    is_irq_from_ext_module() == false)
		Auddrv_DL1_Interrupt_Handler();
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL2))
		Auddrv_DL2_Interrupt_Handler();
}

static void Aud_IRQ2_Handler(void)
{
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL))
		Auddrv_UL1_Interrupt_Handler();
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB))
		Auddrv_AWB_Interrupt_Handler();
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DAI))
		Auddrv_DAI_Interrupt_Handler();
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL_DATA2))
		Auddrv_UL2_Interrupt_Handler();
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_MOD_DAI))
		Auddrv_MOD_DAI_Interrupt_Handler();
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_VUL2))
		Auddrv_VUL2_Interrupt_Handler();
}

static void Aud_IRQ6_Handler(void)
{
	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL3))
		Auddrv_DL1_Data2_Interrupt_Handler(
		Soc_Aud_Digital_Block_MEM_DL3);
}

static void (*Aud_IRQ_Handler_Funcs[Soc_Aud_IRQ_MCU_MODE_NUM])(void) = {
	[Soc_Aud_IRQ_MCU_MODE_IRQ0_MCU_MODE] = NULL,
	[Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE] = Aud_IRQ1_Handler,
	[Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE] = Aud_IRQ2_Handler,
	[Soc_Aud_IRQ_MCU_MODE_IRQ3_MCU_MODE] = NULL,
	[Soc_Aud_IRQ_MCU_MODE_IRQ4_MCU_MODE] = NULL,
	[Soc_Aud_IRQ_MCU_MODE_IRQ5_MCU_MODE] = NULL,
	[Soc_Aud_IRQ_MCU_MODE_IRQ6_MCU_MODE] = Aud_IRQ6_Handler,
	[Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE] = NULL, /* Reserved */
	[Soc_Aud_IRQ_MCU_MODE_IRQ8_MCU_MODE] = NULL,
	[Soc_Aud_IRQ_MCU_MODE_IRQ9_MCU_MODE] = NULL,
	[Soc_Aud_IRQ_MCU_MODE_IRQ10_MCU_MODE] = NULL,
	[Soc_Aud_IRQ_MCU_MODE_IRQ11_MCU_MODE] = NULL,
	[Soc_Aud_IRQ_MCU_MODE_IRQ12_MCU_MODE] = NULL,
	[Soc_Aud_IRQ_MCU_MODE_IRQ_ACC1_MCU_MODE] = NULL,
	[Soc_Aud_IRQ_MCU_MODE_IRQ_ACC2_MCU_MODE] = NULL,
};

void RunIRQHandler(enum Soc_Aud_IRQ_MCU_MODE irqIndex)
{
	if (Aud_IRQ_Handler_Funcs[irqIndex] != NULL)
		Aud_IRQ_Handler_Funcs[irqIndex]();
	else
		pr_warn("%s(), Aud_IRQ%d_Handler is Null", __func__, irqIndex);
}

enum Soc_Aud_IRQ_MCU_MODE irq_request_number(enum soc_aud_digital_block
		mem_block)
{
	switch (mem_block) {
	case Soc_Aud_Digital_Block_MEM_DL1:
	case Soc_Aud_Digital_Block_MEM_DL2:
		return Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE;
	case Soc_Aud_Digital_Block_MEM_DL3:
		return Soc_Aud_IRQ_MCU_MODE_IRQ6_MCU_MODE;
	case Soc_Aud_Digital_Block_MEM_VUL:
	case Soc_Aud_Digital_Block_MEM_VUL2:
	case Soc_Aud_Digital_Block_MEM_AWB:
	case Soc_Aud_Digital_Block_MEM_DAI:
	case Soc_Aud_Digital_Block_MEM_MOD_DAI:
	case Soc_Aud_Digital_Block_MEM_VUL_DATA2:
	case Soc_Aud_Digital_Block_MEM_AWB2:
	case Soc_Aud_Digital_Block_MEM_DAI2:
		return Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE;
	default:
		pr_err("%s, can't request irq_num by this mem_block = %d",
			__func__, mem_block);
		AUDIO_ASSERT(0);
		return Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE;
	}
}

bool IsNeedToSetHighAddr(bool usingdram, dma_addr_t addr)
{
	return false;
}

bool SetHighAddr(enum soc_aud_digital_block MemBlock, bool usingdram,
		 dma_addr_t addr)
{
	return true;
}

int get_usage_digital_block(enum audio_usage_id id)
{
	switch (id) {
	case AUDIO_USAGE_PCM_CAPTURE:
		return Soc_Aud_Digital_Block_MEM_VUL_DATA2;
	case AUDIO_USAGE_SCP_SPK_IV_DATA:
		return Soc_Aud_Digital_Block_MEM_AWB2;
	case AUDIO_USAGE_DEEPBUFFER_PLAYBACK:
		return Soc_Aud_Digital_Block_MEM_DL3;
	case AUDIO_USAGE_FM_CAPTURE:
		return Soc_Aud_Digital_Block_MEM_VUL2;
	case AUDIO_USAGE_VOW_BARGE_IN:
		return Soc_Aud_Digital_Block_MEM_AWB;
	default:
		pr_debug("%s(), not defined id %d\n", __func__, id);
		return -EINVAL;
	};
}

int get_usage_digital_block_io(enum audio_usage_id id)
{
	switch (id) {
	case AUDIO_USAGE_PCM_CAPTURE:
		return Soc_Aud_AFE_IO_Block_MEM_VUL_DATA2;
	case AUDIO_USAGE_SCP_SPK_IV_DATA:
		return Soc_Aud_AFE_IO_Block_MEM_AWB2;
	case AUDIO_USAGE_DEEPBUFFER_PLAYBACK:
		return Soc_Aud_AFE_IO_Block_MEM_DL3;
	case AUDIO_USAGE_FM_CAPTURE:
		return Soc_Aud_AFE_IO_Block_MEM_VUL2;
	case AUDIO_USAGE_SMART_PA_IN:
		return Soc_Aud_AFE_IO_Block_I2S0;
	case AUDIO_USAGE_VOW_BARGE_IN:
		return Soc_Aud_AFE_IO_Block_MEM_AWB;
	default:
		pr_debug("%s(), not defined id %d\n", __func__, id);
		return -EINVAL;
	};
}

void SetSdmLevel(unsigned int level)
{
	Afe_Set_Reg(AFE_ADDA_DL_SDM_DCCOMP_CON, level, 0x3f);
}

enum audio_sram_mode get_prefer_sram_mode(void)
{
	return audio_sram_compact_mode;
}

int set_sram_mode(enum audio_sram_mode sram_mode)
{
	if (sram_mode == audio_sram_compact_mode) {
		/* all memif use compact mode */
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, 0x0 << 16, 0x4fff << 16);
		/* cpu use compact mode when access sram data */
		Afe_Set_Reg(AFE_MEMIF_MSB, 1 << 29, 1 << 29);
	} else {
		/* all memif use normal mode */
		Afe_Set_Reg(AFE_MEMIF_HDALIGN, 0x4fff << 16, 0x4fff << 16);
		/* cpu use normal mode when access sram data */
		Afe_Set_Reg(AFE_MEMIF_MSB, 0 << 29, 1 << 29);
	}
	return 0;
}

/* mtk_codec_ops */
static int enable_dc_compensation(bool enable)
{
	Afe_Set_Reg(AFE_ADDA_DL_SDM_DCCOMP_CON,
		    (enable ? 1 : 0) << 8,
		    0x1 << 8);
	return 0;
}

static int set_lch_dc_compensation(int value)
{
	Afe_Set_Reg(AFE_ADDA_DL_DC_COMP_CFG0, value, MASK_ALL);
	return 0;
}

static int set_rch_dc_compensation(int value)
{
	Afe_Set_Reg(AFE_ADDA_DL_DC_COMP_CFG1, value, MASK_ALL);
	return 0;
}

static int set_ap_dmic(bool enable)
{
	pr_info("%s(), enable = %d\n", __func__, enable);

	if (enable) {
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_ADDA_UL2)) {
			set_adc2_enable(false);

			/* Set sample rate(48K) */
			Afe_Set_Reg(AFE_ADDA6_UL_SRC_CON0,
				    0x3 << 17, 0x7 << 17);

			/* Turns on digital mic  for channel 1 and channel 2 */
			Afe_Set_Reg(AFE_ADDA6_UL_SRC_CON0,
				    0x3 << 21, 0x3 << 21);

			/* Select SDM 3-level mode (DMIC) */
			Afe_Set_Reg(AFE_ADDA6_UL_SRC_CON0, 0x1 << 1, 0x1 << 1);

			/* Turns on uplink SRC */
			Afe_Set_Reg(AFE_ADDA6_UL_SRC_CON0, 0x1, 0x1);

			/* Set ch1/ch2 digital mic low power mode : 3.25M */
			Afe_Set_Reg(AFE_ADDA6_UL_SRC_CON0,
				    0x0 << 14, 0x3 << 14);

			/* Set digmic input mode : 3.25M */
			Afe_Set_Reg(AFE_ADDA6_UL_SRC_CON0, 0x0 << 5, 0x1 << 5);

			/* AFE_ADDA6_FIFO_AUTO_RST */
			Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x0 << 13, 0x1 << 13);

			/* afe_adda6_ckdiv_rst */
			Afe_Set_Reg(AFE_ADDA_UL_DL_CON0, 0x0 << 14, 0x1 << 14);

			set_adc2_enable(true);
		}
	}
	return 0;
}

static int set_hp_impedance_ctl(bool enable)
{
	/* open adda dl path for dc compensation */
	if (enable) {
		AudDrv_Clk_On();
		if (GetMemoryPathEnable(
			Soc_Aud_Digital_Block_I2S_OUT_DAC) == false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
				true);
			SetI2SDacOut(48000, false,
				Soc_Aud_I2S_WLEN_WLEN_32BITS);
			SetI2SDacEnable(true);
		} else {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
				true);
		}

		/* set data mute */
		set_adda_dl_src_gain(true);

		EnableAfe(true);
	} else {
		/* unmute data */
		set_adda_dl_src_gain(false);

		/* stop DAC output */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);

		EnableAfe(false);

		AudDrv_Clk_Off();
	}

	return 0;
}

static struct mtk_codec_ops mtk_codec_platform_ops = {
	.enable_dc_compensation = enable_dc_compensation,
	.set_lch_dc_compensation = set_lch_dc_compensation,
	.set_rch_dc_compensation = set_rch_dc_compensation,
	.set_ap_dmic = set_ap_dmic,
	.set_hp_impedance_ctl = set_hp_impedance_ctl,
};

static struct mtk_afe_platform_ops afe_platform_ops = {
	.set_sinegen = set_chip_sine_gen_enable,
	.set_general_asrc_enable = set_chip_general_asrc_enable,
	.set_general_asrc_parameter = set_chip_general_asrc_parameter,
};

void init_afe_ops(void)
{
	/* init all afe ops here */
	pr_debug("%s\n", __func__);
	set_mem_blk_ops(&mem_blk_ops);
	set_afe_platform_ops(&afe_platform_ops);

	set_codec_ops(&mtk_codec_platform_ops);
}
