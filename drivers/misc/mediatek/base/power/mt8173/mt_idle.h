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

#ifndef _MT_IDLE_H
#define _MT_IDLE_H

#include <mach/mt_ptp.h>

#define MT_IDLE_EN		1

enum idle_lock_spm_id {
	IDLE_SPM_LOCK_VCORE_DVFS = 0,
};

#if MT_IDLE_EN

extern void idle_lock_spm(enum idle_lock_spm_id id);
extern void idle_unlock_spm(enum idle_lock_spm_id id);

extern void enable_dpidle_by_bit(int id);
extern void disable_dpidle_by_bit(int id);
extern void enable_soidle_by_bit(int id);
extern void disable_soidle_by_bit(int id);

extern void mt_idle_init(void);

#else /* !MT_IDLE_EN */

static inline void idle_lock_spm(enum idle_lock_spm_id id) {}
static inline void idle_unlock_spm(enum idle_lock_spm_id id) {}

static inline void enable_dpidle_by_bit(int id) {}
static inline void disable_dpidle_by_bit(int id) {}
static inline void enable_soidle_by_bit(int id) {}
static inline void disable_soidle_by_bit(int id) {}

static inline void mt_idle_init(void) {}

#endif /* MT_IDLE_EN */

enum {
	CG_PERI0	= 0,
	CG_PERI1	= 1,
	CG_INFRA	= 2,
	CG_DISP0	= 3,
	CG_DISP1	= 4,
	CG_IMAGE	= 5,
	CG_MFG		= 6,
	CG_AUDIO	= 7,
	CG_VDEC0	= 8,
	CG_VDEC1	= 9,
	CG_VENC		= 10,
	CG_VENCLT	= 11,
	NR_GRPS		= 12,
};

enum cg_clk_id {
	MT_CG_PERI0_NFI			= 0,
	MT_CG_PERI0_THERM		= 1,
	MT_CG_PERI0_PWM1		= 2,
	MT_CG_PERI0_PWM2		= 3,
	MT_CG_PERI0_PWM3		= 4,
	MT_CG_PERI0_PWM4		= 5,
	MT_CG_PERI0_PWM5		= 6,
	MT_CG_PERI0_PWM6		= 7,
	MT_CG_PERI0_PWM7		= 8,
	MT_CG_PERI0_PWM			= 9,
	MT_CG_PERI0_USB0		= 10,
	MT_CG_PERI0_USB1		= 11,
	MT_CG_PERI0_AP_DMA		= 12,
	MT_CG_PERI0_MSDC30_0		= 13,
	MT_CG_PERI0_MSDC30_1		= 14,
	MT_CG_PERI0_MSDC30_2		= 15,
	MT_CG_PERI0_MSDC30_3		= 16,
	MT_CG_PERI0_NLI			= 17,
	MT_CG_PERI0_IRDA		= 18,
	MT_CG_PERI0_UART0		= 19,
	MT_CG_PERI0_UART1		= 20,
	MT_CG_PERI0_UART2		= 21,
	MT_CG_PERI0_UART3		= 22,
	MT_CG_PERI0_I2C0		= 23,
	MT_CG_PERI0_I2C1		= 24,
	MT_CG_PERI0_I2C2		= 25,
	MT_CG_PERI0_I2C3		= 26,
	MT_CG_PERI0_I2C4		= 27,
	MT_CG_PERI0_AUXADC		= 28,
	MT_CG_PERI0_SPI0		= 29,
	MT_CG_PERI0_I2C5		= 30,
	MT_CG_PERI0_NFIECC		= 31,

	MT_CG_PERI1_SPI			= 32,
	MT_CG_PERI1_IRRX		= 33,
	MT_CG_PERI1_I2C6		= 34,

	MT_CG_INFRA_DBGCLK		= 64,
	MT_CG_INFRA_SMI			= 65,
	MT_CG_INFRA_AUDIO		= 69,
	MT_CG_INFRA_GCE			= 70,
	MT_CG_INFRA_L2C_SRAM		= 71,
	MT_CG_INFRA_M4U			= 72,
	MT_CG_INFRA_CPUM		= 79,
	MT_CG_INFRA_KP			= 80,
	MT_CG_INFRA_CEC_PDN		= 82,
	MT_CG_INFRA_PMICSPI		= 86,
	MT_CG_INFRA_PMICWRAP		= 87,

	MT_CG_DISP0_SMI_COMMON		= 96,
	MT_CG_DISP0_SMI_LARB0		= 97,
	MT_CG_DISP0_CAM_MDP		= 98,
	MT_CG_DISP0_MDP_RDMA0		= 99,
	MT_CG_DISP0_MDP_RDMA1		= 100,
	MT_CG_DISP0_MDP_RSZ0		= 101,
	MT_CG_DISP0_MDP_RSZ1		= 102,
	MT_CG_DISP0_MDP_RSZ2		= 103,
	MT_CG_DISP0_MDP_TDSHP0		= 104,
	MT_CG_DISP0_MDP_TDSHP1		= 105,
	MT_CG_DISP0_MDP_WDMA		= 107,
	MT_CG_DISP0_MDP_WROT0		= 108,
	MT_CG_DISP0_MDP_WROT1		= 109,
	MT_CG_DISP0_FAKE_ENG		= 110,
	MT_CG_DISP0_MUTEX_32K		= 111,
	MT_CG_DISP0_DISP_OVL0		= 112,
	MT_CG_DISP0_DISP_OVL1		= 113,
	MT_CG_DISP0_DISP_RDMA0		= 114,
	MT_CG_DISP0_DISP_RDMA1		= 115,
	MT_CG_DISP0_DISP_RDMA2		= 116,
	MT_CG_DISP0_DISP_WDMA0		= 117,
	MT_CG_DISP0_DISP_WDMA1		= 118,
	MT_CG_DISP0_DISP_COLOR0		= 119,
	MT_CG_DISP0_DISP_COLOR1		= 120,
	MT_CG_DISP0_DISP_AAL		= 121,
	MT_CG_DISP0_DISP_GAMMA		= 122,
	MT_CG_DISP0_DISP_UFOE		= 123,
	MT_CG_DISP0_DISP_SPLIT0		= 124,
	MT_CG_DISP0_DISP_SPLIT1		= 125,
	MT_CG_DISP0_DISP_MERGE		= 126,
	MT_CG_DISP0_DISP_OD		= 127,

	MT_CG_DISP1_DISP_PWM0_MM	= 128,
	MT_CG_DISP1_DISP_PWM0_26M	= 129,
	MT_CG_DISP1_DISP_PWM1_MM	= 130,
	MT_CG_DISP1_DISP_PWM1_26M	= 131,
	MT_CG_DISP1_DSI0_ENGINE		= 132,
	MT_CG_DISP1_DSI0_DIGITAL	= 133,
	MT_CG_DISP1_DSI1_ENGINE		= 134,
	MT_CG_DISP1_DSI1_DIGITAL	= 135,
	MT_CG_DISP1_DPI_PIXEL		= 136,
	MT_CG_DISP1_DPI_ENGINE		= 137,
	MT_CG_DISP1_DPI1_PIXEL		= 138,
	MT_CG_DISP1_DPI1_ENGINE		= 139,
	MT_CG_DISP1_HDMI_PIXEL		= 140,
	MT_CG_DISP1_HDMI_PLLCK		= 141,
	MT_CG_DISP1_HDMI_AUDIO		= 142,
	MT_CG_DISP1_HDMI_SPDIF		= 143,
	MT_CG_DISP1_LVDS_PIXEL		= 144,
	MT_CG_DISP1_LVDS_CTS		= 145,
	MT_CG_DISP1_SMI_LARB4		= 146,

	MT_CG_IMAGE_LARB2_SMI		= 160,
	MT_CG_IMAGE_CAM_SMI		= 165,
	MT_CG_IMAGE_CAM_CAM		= 166,
	MT_CG_IMAGE_SEN_TG		= 167,
	MT_CG_IMAGE_SEN_CAM		= 168,
	MT_CG_IMAGE_CAM_SV		= 169,
	MT_CG_IMAGE_FD			= 171,

	MT_CG_MFG_AXI			= 192,
	MT_CG_MFG_MEM			= 193,
	MT_CG_MFG_G3D			= 194,
	MT_CG_MFG_26M			= 195,

	MT_CG_AUDIO_AFE			= 226,
	MT_CG_AUDIO_I2S			= 230,
	MT_CG_AUDIO_22M			= 232,
	MT_CG_AUDIO_24M			= 233,
	MT_CG_AUDIO_SPDF2		= 235,
	MT_CG_AUDIO_APLL2_TUNER		= 242,
	MT_CG_AUDIO_APLL_TUNER		= 243,
	MT_CG_AUDIO_HDMI		= 244,
	MT_CG_AUDIO_SPDF		= 245,
	MT_CG_AUDIO_ADDA3		= 246,
	MT_CG_AUDIO_ADDA2		= 247,
	MT_CG_AUDIO_ADC			= 248,
	MT_CG_AUDIO_DAC			= 249,
	MT_CG_AUDIO_DAC_PREDIS		= 250,
	MT_CG_AUDIO_TML			= 251,
	MT_CG_AUDIO_IDLE_EN_EXT		= 253,
	MT_CG_AUDIO_IDLE_EN_INT		= 254,

	MT_CG_VDEC0_VDEC		= 256,
	MT_CG_VDEC1_LARB		= 288,

	MT_CG_VENC_CKE0			= 320,
	MT_CG_VENC_CKE1			= 324,
	MT_CG_VENC_CKE2			= 328,
	MT_CG_VENC_CKE3			= 332,

	MT_CG_VENCLT_CKE0		= 352,
	MT_CG_VENCLT_CKE1		= 356,

	CG_INFRA_FROM			= MT_CG_INFRA_DBGCLK,
	CG_INFRA_TO			= MT_CG_INFRA_PMICWRAP,
	NR_INFRA_CLKS			= 9,

	CG_PERI0_FROM			= MT_CG_PERI0_NFI,
	CG_PERI0_TO			= MT_CG_PERI0_NFIECC,
	NR_PERI0_CLKS			= 32,

	CG_PERI1_FROM			= MT_CG_PERI1_SPI,
	CG_PERI1_TO			= MT_CG_PERI1_I2C6,
	NR_PERI1_CLKS			= 3,

	CG_MFG_FROM			= MT_CG_MFG_AXI,
	CG_MFG_TO			= MT_CG_MFG_26M,
	NR_MFG_CLKS			= 4,

	CG_IMAGE_FROM			= MT_CG_IMAGE_LARB2_SMI,
	CG_IMAGE_TO			= MT_CG_IMAGE_FD,
	NR_IMAGE_CLKS			= 7,

	CG_DISP0_FROM			= MT_CG_DISP0_SMI_COMMON,
	CG_DISP0_TO			= MT_CG_DISP0_DISP_OD,
	NR_DISP0_CLKS			= 31,

	CG_DISP1_FROM			= MT_CG_DISP1_DISP_PWM0_MM,
	CG_DISP1_TO			= MT_CG_DISP1_SMI_LARB4,
	NR_DISP1_CLKS			= 19,

	CG_VDEC0_FROM			= MT_CG_VDEC0_VDEC,
	CG_VDEC0_TO			= MT_CG_VDEC0_VDEC,
	NR_VDEC0_CLKS			= 1,

	CG_VDEC1_FROM			= MT_CG_VDEC1_LARB,
	CG_VDEC1_TO			= MT_CG_VDEC1_LARB,
	NR_VDEC1_CLKS			= 1,

	CG_VENC_FROM			= MT_CG_VENC_CKE0,
	CG_VENC_TO			= MT_CG_VENC_CKE3,
	NR_VENC_CLKS			= 4,

	CG_VENCLT_FROM			= MT_CG_VENCLT_CKE0,
	CG_VENCLT_TO			= MT_CG_VENCLT_CKE1,
	NR_VENCLT_CLKS			= 2,

	CG_AUDIO_FROM			= MT_CG_AUDIO_AFE,
	CG_AUDIO_TO			= MT_CG_AUDIO_IDLE_EN_INT,
	NR_AUDIO_CLKS			= 17,

	NR_CLKS				= 357,
};

#ifdef _MT_IDLE_C

/* TODO: remove it */

extern unsigned int g_SPM_MCDI_Abnormal_WakeUp;

#if defined(EN_PTP_OD) && EN_PTP_OD
extern u32 ptp_data[3];
#endif

extern struct kobject *power_kobj;

#endif /* _MT_IDLE_C */

#endif
