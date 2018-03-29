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


#ifndef __hdmictrl_h__
#define __hdmictrl_h__
#ifdef CONFIG_MTK_INTERNAL_HDMI_SUPPORT

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>

#include "hdmitable.h"
#include "internal_hdmi_drv.h"
/* ////////////////////////////////////// */

#if 0
#define HDMIDRV_BASE  (0xF4015000)
#define HDMISYS_BASE  (0xF4000000)
#define HDMIPLL_BASE  (0xF0209000)	/* pll */
#define HDMICKGEN_BASE  (0xF0000000)
#endif

#define FALSE                       0
#define TRUE                        1


enum HDMI_REG_ENUM {
	HDMI_SHELL,
	HDMI_DDC,
	HDMI_CEC,
	HDMI_REG_NUM,
};

enum HDMI_REF_MODULE_ENUM {
	AP_CCIF0,		/*  PLL/TVD */
	TOPCK_GEN,
	INFRA_SYS,
	MMSYS_CONFIG,
	GPIO_REG,
	DPI0_REG,
	DISP_CONFIG2,
	GIC_REG,
	PERISYS_REG,
	HDMI_REF_REG_NUM,
};

enum HDMI_REF_CLOCK_ENUM {
	/* MMSYS_POWER, */		/* Must be first,  power of the mmsys */
	PERI_DDC,
	MMSYS_HDMI_HDCP,
	MMSYS_HDMI_HDCP24,
	INFRA_SYS_CEC,
	MMSYS_HDMI_PLL,
	MMSYS_HDMI_PIXEL,
	MMSYS_HDMI_AUDIO,
	MMSYS_HDMI_SPIDIF,
	TOP_HDMI_SEL,
	TOP_HDMISEL_DIG_CTS,
	TOP_HDMISEL_D2,
	TOP_HDMISEL_D3,
	TOP_HDCP_SEL,
	TOP_HDCPSEL_SYS4D2,
	TOP_HDCPSEL_SYS3D4,
	TOP_HDCPSEL_UNIV2D2,
	TOP_HDCP24_SEL,
	TOP_HDCP24SEL_UNIVPD26,
	TOP_HDCP24SEL_UNIVPD52,
	TOP_HDCP24SEL_UNIVP2D8,
	HDMI_SEL_CLOCK_NUM,
};

extern unsigned long hdmi_reg[HDMI_REG_NUM];
extern unsigned long hdmi_ref_reg[HDMI_REF_REG_NUM];
extern struct clk *hdmi_ref_clock[HDMI_SEL_CLOCK_NUM];
extern unsigned char hdmi_dpi_output;

#define HDMIDRV_BASE   (hdmi_reg[HDMI_SHELL])
#define HDMIDDC_BASE   (hdmi_reg[HDMI_DDC])
#define HDMICEC_BASE   (hdmi_reg[HDMI_CEC])

#define HDMISYS_BASE   (hdmi_ref_reg[MMSYS_CONFIG])
#define HDMIPLL_BASE   (hdmi_ref_reg[AP_CCIF0])	/* pll */
#define HDMICKGEN_BASE (hdmi_ref_reg[TOPCK_GEN])
#define HDMIPAD_BASE   (hdmi_ref_reg[GPIO_REG])
#define HDMI_INFRA_SYS (hdmi_ref_reg[INFRA_SYS])
#define DISP_CONFIG2_BASE (hdmi_ref_reg[DISP_CONFIG2])



/* ////////////////////////////////////// */
#define PORD_MODE     (1<<0)
#define HOTPLUG_MODE  (1<<1)
/* ////////////////////////////////////// */
#define AV_INFO_HD_ITU709           0x80
#define AV_INFO_SD_ITU601           0x40
#define AV_INFO_4_3_OUTPUT          0x10
#define AV_INFO_16_9_OUTPUT         0x20

/* AVI Info Frame */
#define AVI_TYPE            0x82
#define AVI_VERS            0x02
#define AVI_LEN             0x0d

/* Audio Info Frame */
#define AUDIO_TYPE             0x84
#define AUDIO_VERS             0x01
#define AUDIO_LEN              0x0A

#define VS_TYPE            0x81
#define VS_VERS            0x01
#define VS_LEN             0x05
#define VS_PB_LEN          0x0b	/* VS_LEN+1 include checksum */
/* GAMUT Data */
#define GAMUT_TYPE             0x0A
#define GAMUT_PROFILE          0x81
#define GAMUT_SEQ              0x31

/* ACP Info Frame */
#define ACP_TYPE            0x04
/* #define ACP_VERS            0x02 */
#define ACP_LEN             0x00

/* ISRC1 Info Frame */
#define ISRC1_TYPE            0x05
/* #define ACP_VERS            0x02 */
#define ISRC1_LEN             0x00

/* SPD Info Frame */
#define SPD_TYPE            0x83
#define SPD_VERS            0x01
#define SPD_LEN             0x19


#define SV_ON        (unsigned short)(1)
#define SV_OFF       (unsigned short)(0)

#define TMDS_CLK_X1 1
#define TMDS_CLK_X1_25  2
#define TMDS_CLK_X1_5  3
#define TMDS_CLK_X2  4

#define RJT_24BIT 0
#define RJT_16BIT 1
#define LJT_24BIT 2
#define LJT_16BIT 3
#define I2S_24BIT 4
#define I2S_16BIT 5

/* ///////////////////////////////////// */
#define GRL_INT              0x14
#define INT_MDI            (0x1 << 0)
#define INT_HDCP           (0x1 << 1)
#define INT_FIFO_O         (0x1 << 2)
#define INT_FIFO_U         (0x1 << 3)
#define INT_IFM_ERR        (0x1 << 4)
#define INT_INF_DONE       (0x1 << 5)
#define INT_NCTS_DONE      (0x1 << 6)
#define INT_CTRL_PKT_DONE  (0x1 << 7)
#define GRL_INT_MASK              0x18
#define GRL_CTRL             0x1C
#define CTRL_GEN_EN        (0x1 << 2)
#define CTRL_SPD_EN        (0x1 << 3)
#define CTRL_MPEG_EN       (0x1 << 4)
#define CTRL_AUDIO_EN      (0x1 << 5)
#define CTRL_AVI_EN        (0x1 << 6)
#define CTRL_AVMUTE        (0x1 << 7)
#define GRL_STATUS           0x20
#define STATUS_HTPLG       (0x1 << 0)
#define STATUS_PORD        (0x1 << 1)

#define GRL_CFG0             0x24
#define CFG0_I2S_MODE_RTJ  0x1
#define CFG0_I2S_MODE_LTJ  0x0
#define CFG0_I2S_MODE_I2S  0x2
#define CFG0_I2S_MODE_24Bit 0x00
#define CFG0_I2S_MODE_16Bit 0x10

#define GRL_CFG1             0x28
#define CFG1_EDG_SEL       (0x1 << 0)
#define CFG1_SPDIF         (0x1 << 1)
#define CFG1_DVI           (0x1 << 2)
#define CFG1_HDCP_DEBUG    (0x1 << 3)
#define GRL_CFG2             0x2c
#define CFG2_NOTICE_EN     (0x1 << 6)
#define MHL_DE_SEL         (0x1<<3)
#define GRL_CFG3             0x30
#define CFG3_AES_KEY_INDEX_MASK    0x3f
#define CFG3_CONTROL_PACKET_DELAY  (0x1 << 6)
#define CFG3_KSV_LOAD_START        (0x1 << 7)
#define GRL_CFG4             0x34
#define CFG4_AES_KEY_LOAD  (0x1 << 4)
#define CFG4_AV_UNMUTE_EN  (0x1 << 5)
#define CFG4_AV_UNMUTE_SET (0x1 << 6)
#define CFG_MHL_MODE (0x1<<7)
#define GRL_CFG5             0x38
#define CFG5_CD_RATIO_MASK 0x8F
#define CFG5_FS128         (0x1 << 4)
#define CFG5_FS256         (0x2 << 4)
#define CFG5_FS384         (0x3 << 4)
#define CFG5_FS512         (0x4 << 4)
#define CFG5_FS768         (0x6 << 4)
#define GRL_WR_BKSV0         0x40
#define GRL_WR_AN0           0x54
#define GRL_RD_AKSV0         0x74
#define GRL_RI_0             0x88
#define GRL_KEY_PORT         0x90
#define GRL_KSVLIST          0x94
#define GRL_HDCP_STA         0xB8
#define HDCP_STA_RI_RDY    (0x1 << 2)
#define HDCP_STA_V_MATCH   (0x1 << 3)
#define HDCP_STA_V_RDY     (0x1 << 4)


#define GRL_HDCP_CTL         0xBC
#define HDCP_CTL_ENC_EN    (0x1 << 0)
#define HDCP_CTL_AUTHEN_EN (0x1 << 1)
#define HDCP_CTL_CP_RSTB   (0x1 << 2)
#define HDCP_CTL_AN_STOP   (0x1 << 3)
#define HDCP_CTRL_RX_RPTR  (0x1 << 4)
#define HDCP_CTL_HOST_KEY  (0x1 << 6)
#define HDCP_CTL_SHA_EN    (0x1 << 7)

#define GRL_REPEATER_HASH    0xC0
#define GRL_I2S_C_STA0             0x140
#define GRL_I2S_C_STA1             0x144
#define GRL_I2S_C_STA2             0x148
#define GRL_I2S_C_STA3             0x14C	/* including sampling frequency information. */
#define GRL_I2S_C_STA4             0x150
#define GRL_I2S_UV           0x154
#define GRL_ACP_ISRC_CTRL    0x158
#define VS_EN              (0x01<<0)
#define ACP_EN             (0x01<<1)
#define ISRC1_EN           (0x01<<2)
#define ISRC2_EN           (0x01<<3)
#define GAMUT_EN           (0x01<<4)
#define GRL_CTS_CTRL         0x160
#define CTS_CTRL_SOFT      (0x1 << 0)

#define GRL_CTS0             0x164
#define GRL_CTS1             0x168
#define GRL_CTS2             0x16c

#define GRL_DIVN             0x170
#define NCTS_WRI_ANYTIME   (0x01<<6)

#define GRL_DIV_RESET        0x178
#define SWAP_YC  (0x01 << 0)
#define UNSWAP_YC  (0x00 << 0)

#define GRL_AUDIO_CFG        0x17C
#define AUDIO_ZERO          (0x01<<0)
#define HIGH_BIT_RATE      (0x01<<1)
#define SACD_DST           (0x01<<2)
#define DST_NORMAL_DOUBLE  (0x01<<3)
#define DSD_INV            (0x01<<4)
#define LR_INV             (0x01<<5)
#define LR_MIX             (0x01<<6)
#define SACD_SEL           (0x01<<7)

#define GRL_NCTS             0x184

#define GRL_IFM_PORT         0x188
#define GRL_CH_SW0           0x18C
#define GRL_CH_SW1           0x190
#define GRL_CH_SW2           0x194
#define GRL_CH_SWAP          0x198
#define LR_SWAP             (0x01<<0)
#define LFE_CC_SWAP         (0x01<<1)
#define LSRS_SWAP           (0x01<<2)
#define RLS_RRS_SWAP        (0x01<<3)
#define LR_STATUS_SWAP      (0x01<<4)

#define GRL_INFOFRM_VER      0x19C
#define GRL_INFOFRM_TYPE     0x1A0
#define GRL_INFOFRM_LNG      0x1A4
#define GRL_SHIFT_R2         0x1B0
#define AUDIO_PACKET_OFF      (0x01 << 6)
#define GRL_MIX_CTRL         0x1B4
#define MIX_CTRL_SRC_EN    (0x1 << 0)
#define BYPASS_VOLUME    (0x1 << 1)
#define MIX_CTRL_FLAT      (0x1 << 7)
#define GRL_IIR_FILTER       0x1B8
#define GRL_SHIFT_L1         0x1C0
#define GRL_AOUT_BNUM_SEL    0x1C4
#define AOUT_24BIT         0x00
#define AOUT_20BIT         0x02
#define AOUT_16BIT         0x03
#define HIGH_BIT_RATE_PACKET_ALIGN (0x3 << 6)

#define GRL_L_STATUS_0        0x200
#define GRL_L_STATUS_1        0x204
#define GRL_L_STATUS_2        0x208
#define GRL_L_STATUS_3        0x20c
#define GRL_L_STATUS_4        0x210
#define GRL_L_STATUS_5        0x214
#define GRL_L_STATUS_6        0x218
#define GRL_L_STATUS_7        0x21c
#define GRL_L_STATUS_8        0x220
#define GRL_L_STATUS_9        0x224
#define GRL_L_STATUS_10        0x228
#define GRL_L_STATUS_11        0x22c
#define GRL_L_STATUS_12        0x230
#define GRL_L_STATUS_13        0x234
#define GRL_L_STATUS_14        0x238
#define GRL_L_STATUS_15        0x23c
#define GRL_L_STATUS_16        0x240
#define GRL_L_STATUS_17        0x244
#define GRL_L_STATUS_18        0x248
#define GRL_L_STATUS_19        0x24c
#define GRL_L_STATUS_20        0x250
#define GRL_L_STATUS_21        0x254
#define GRL_L_STATUS_22        0x258
#define GRL_L_STATUS_23        0x25c
#define GRL_R_STATUS_0        0x260
#define GRL_R_STATUS_1        0x264
#define GRL_R_STATUS_2        0x268
#define GRL_R_STATUS_3        0x26c
#define GRL_R_STATUS_4        0x270
#define GRL_R_STATUS_5        0x274
#define GRL_R_STATUS_6        0x278
#define GRL_R_STATUS_7        0x27c
#define GRL_R_STATUS_8        0x280
#define GRL_R_STATUS_9        0x284
#define GRL_R_STATUS_10        0x288
#define GRL_R_STATUS_11        0x28c
#define GRL_R_STATUS_12        0x290
#define GRL_R_STATUS_13        0x294
#define GRL_R_STATUS_14        0x298
#define GRL_R_STATUS_15        0x29c
#define GRL_R_STATUS_16        0x2a0
#define GRL_R_STATUS_17        0x2a4
#define GRL_R_STATUS_18        0x2a8
#define GRL_R_STATUS_19        0x2ac
#define GRL_R_STATUS_20        0x2b0
#define GRL_R_STATUS_21        0x2b4
#define GRL_R_STATUS_22        0x2b8
#define GRL_R_STATUS_23        0x2bc

#define DUMMY_304    0x304
#define CHMO_SEL  (0x3<<2)
#define CHM1_SEL  (0x3<<4)
#define CHM2_SEL  (0x3<<6)
#define AUDIO_I2S_NCTS_SEL   (1<<1)
#define AUDIO_I2S_NCTS_SEL_64   (1<<1)
#define AUDIO_I2S_NCTS_SEL_128  (0<<1)
#define NEW_GCP_CTRL (1<<0)
#define NEW_GCP_CTRL_MERGE (1<<0)
#define NEW_GCP_CTRL_ORIG  (0<<0)

#define CRC_CTRL 0x310
#define clr_crc_result (1<<1)
#define init_crc  (1<<0)

#define CRC_RESULT_L 0x314

#define CRC_RESULT_H 0x318

#define VIDEO_CFG_0 0x380
#define VIDEO_CFG_1 0x384
#define VIDEO_CFG_2 0x388
#define VIDEO_CFG_3 0x38c
#define VIDEO_CFG_4 0x390
#define VIDEO_SOURCE_SEL (1<<7)
#define NORMAL_PATH (1<<7)
#define GEN_RGB (0<<7)
/***********************************************/
#define CLK_CFG_6   0xA0
#define CLK_DPI0_INV       (0x01 << 4)
#define CLK_DPI0_SEL       (0x07 << 0)
#define CLK_DPI0_SEL_26M       (0x0 << 0)
#define CLK_DPI0_SEL_D2       (0x01 << 0)
#define CLK_DPI0_SEL_D4       (0x02 << 0)
#define CLK_DPI0_SEL_D8       (0x05 << 0)
#define CLK_DPI0_SEL_D16       (0x06 << 0)

#define CLK_CFG_7   0xC0
#define CLK_CFG_8   0xD0
#define CLK_AUDDIV_4 0x134
#define CLK_SEL_INPUT_IIS (0x0 << 16)
#define CLK_SEL_INPUT_SPDIF (0x1 << 16)
#define CLK_SPDIF_1 (0x0 << 17)
#define CLK_SPDIF_2 (0x1 << 17)
#define CLK_SPDIF_HDMI1 (0x0 << 18)
#define CLK_SPDIF_HDMI2 (0x1 << 18)
#define CLK_SEL_APLL1 (0x0 << 19)
#define CLK_SEL_APLL2 (0x1 << 19)
#define CLK_MUX_POWERUP (0x0 << 20)
#define CLK_MUX_POWERDOWN (0x1 << 20)

#define GPIO_MODE4 0x630
#define HDMISCK (0x1 << 12)
#define HDMI2SCK (0x2 << 12)
#define SCKMASK (0x7 << 12)
#define GPIO_MODE5 0x640
#define HDMISDA (0x1 << 0)
#define HDMI2SDA (0x2 << 0)
#define SDAMASK (0x7 << 0)

/***********************************************/

/* ////////////////////////////////////////////////// */
#define HDMICLK_CFG_4	0x80
#define CLK_DPI1_SEL (0x3<<24)
#define DPI1_MUX_CK (0x0<<24)
#define DPI1_H_CK (0x1<<24)
#define DPI1_D2  (0x2<<24)
#define DPI1_D4  (0x3<<24)

#define HDMICLK_CFG_5	0x90
#define CLK_HDMIPLL_SEL (0x3<<8)
#define HDMIPLL_MUX_CK (0x0<<8)
#define HDMIPLL_CTS (0x1<<8)
#define HDMIPLL_D2  (0x2<<8)
#define HDMIPLL_D4  (0x3<<8)

#define INFRA_RST0	0x30
#define CEC_SOFT_RST (0x1<<9)
#define CEC_NORMAL (0x0<<9)

#define INFRA_PDN0	0x40
#define CEC_PDN0 (0x1<<18)

#define INFRA_PDN1	0x44
#define CEC_PDN1 (0x1<<18)

#define PREICFG_PDN_SET	0x08
#define DDC_PDN_SET (0x1<<30)

#define PREICFG_PDN_CLR	0x10
#define DDC_PDN_CLR (0x1<<30)

  /***********************************************/

#define MMSYS_CG_CON1 0x110
#define DPI_PIXEL_CLK   (0x1 << 8)
#define HDMI_PIXEL_CLK  (0x1 << 12)
#define HDMI_PLL_CLK    (0x1 << 13)
#define HDMI_AUDIO_BCLK (0x1 << 14)
#define HDMI_SPDIF_CLK  (0x1 << 15)
#define HDMI_HDCP_CLK   (0x1 << 19)
#define HDMI_HDCP24_CLK (0x1 << 20)
#define DPI_PIXEL_CLK_EN   (0x0 << 8)
#define HDMI_PIXEL_CLK_EN  (0x0 << 12)
#define HDMI_PLL_CLK_EN    (0x0 << 13)
#define HDMI_AUDIO_BCLK_EN (0x0 << 14)
#define HDMI_SPDIF_CLK_EN  (0x0 << 15)
#define HDMI_HDCP_CLK_EN   (0x0 << 19)
#define HDMI_HDCP24_CLK_EN (0x0 << 20)


  /***********************************************/

/* MMSYS Clock gating config, 0:clock enable; 1:clock gating */



#define HDMI_SYS_CFG1C   0x900
#define HDMI_ON            (0x01 << 0)
#define HDMI_RST           (0x01 << 1)
#define ANLG_ON           (0x01 << 2)
#define CFG10_DVI          (0x01 << 3)
#define HDMI_TST           (0x01 << 4)
#define SYS_KEYMASK1       (0xff<<8)
#define SYS_KEYMASK2       (0xffff<<8)
#define AUD_OUTSYNC_EN     (((unsigned int)1)<<24)
#define AUD_OUTSYNC_PRE_EN (((unsigned int)1)<<25)
#define I2CM_ON            (((unsigned int)1)<<26)
#define E2PROM_TYPE_8BIT   (((unsigned int)1)<<27)
#define MCM_E2PROM_ON      (((unsigned int)1)<<28)
#define EXT_E2PROM_ON      (((unsigned int)1)<<29)
#define HTPLG_PIN_SEL_OFF  (((unsigned int)1)<<30)
#define AES_EFUSE_ENABLE   (((unsigned int)1)<<31)

#define HDMI_SYS_CFG20   0x904
#define DEEP_COLOR_MODE_MASK (3 << 1)
#define COLOR_8BIT_MODE	 (0 << 1)
#define COLOR_10BIT_MODE	 (1 << 1)
#define COLOR_12BIT_MODE	 (2 << 1)
#define COLOR_16BIT_MODE	 (3 << 1)
#define DEEP_COLOR_EN		 (1 << 0)
#define HDMI_AUDIO_TEST_SEL	  (0x01 << 8)
#define HDMI2P0_EN			  (0x1 << 11)
#define HDMI_OUT_FIFO_EN		  (0x01 << 16)
#define HDMI_OUT_FIFO_CLK_INV	  (0x01 << 17)
#define MHL_MODE_ON			  (0x01 << 28)
#define MHL_PP_MODE			  (0x01 << 29)
#define MHL_SYNC_AUTO_EN		  (0x01 << 30)
#define HDMI_PCLK_FREE_RUN	  (0x01 << 31)

#define HDMI_SYS_CFG24   0x24
#define HDMI_SYS_CFG28   0x28
#define HDMI_SYS_FMETER   0x4c
#define TRI_CAL (1<<0)
#define CLK_EXC (1<<1)
#define CAL_OK (1<<2)
#define CALSEL (2<<3)
#define CAL_CNT (0xffff<<16)
/* ///////////////////////////////////////////////// */
#define HDMI_SYS_PWR_RST_B  0x100
#define hdmi_pwr_sys_sw_reset (0<<0)
#define hdmi_pwr_sys_sw_unreset (1<<0)

#define HDMI_PWR_CTRL  0x104
#define hdmi_iso_dis (0<<0)
#define hdmi_iso_en (1<<0)
#define hdmi_power_turnoff (0<<1)
#define hdmi_power_turnon (1<<1)
#define hdmi_clock_on (0<<2)
#define hdmi_clock_off (1<<2)

#define HDMI_SYS_AMPCTRL  0x328
#define CK_TXAMP_ENB             0x00000008
#define D0_TXAMP_ENB             0x00000080
#define D1_TXAMP_ENB             0x00000800
#define D2_TXAMP_ENB             0x00008000
#define RSET_DTXST_OFF           0x00080000
#define SET_DTXST_ON             0x00800000
#define ENTXTST_ENB              0x08000000
#define CKFIFONEG                0x80000000
#define RG_SET_DTXST           (1<<23)

#define HDMI_SYS_AMPCTRL1  0x32c

#define HDMI_SYS_PLLCTRL1  0x330
#define PRE_AMP_ENB           0x00000080	/* (0x01<<7) */
#define INV_CLCK              0x80000000
#define RG_ENCKST   (1<<2)

#define HDMI_SYS_PLLCTRL2  0x334
#define POW_HDMITX    (0x01 << 20)
#define POW_PLL_L     (0x01 << 21)


#define HDMI_SYS_PLLCTRL3  0x338
#define PLLCTRL3_MASK 0xffffffff
#define RG_N3_MASK             0x0000001f	/* 1f <<0 */
#define N3_POS              0
#define RG_N4_MASK             (3 << 5)
#define N4_POS              5

#define RG_BAND_MASK_0         (1<<7)
#define BAND_MASK_0_POS     7
#define RG_BAND_MASK_1         (1<<11)
#define BAND_MASK_1_POS     11


#define HDMI_SYS_PLLCTRL4  0x33c
#define PLLCTRL4_MASK 0xffffffff
#define RG_ENRST_CALIB  (1 << 21)

#define HDMI_SYS_PLLCTRL5  0x340
#define PLLCTRL5_MASK 0xffffffff

#define TURN_ON_RSEN_RESISTANCE     (1<<7)
#define RG_N5_MASK             0x0000001f
#define N5_POS              0
#define RG_N6_MASK             (3 << 5)
#define N6_POS              5

#define RG_N1_MASK             0x00001f00
#define N1_POS              8


#define HDMI_SYS_PLLCTRL6  0x344
#define PLLCTRL6_MASK 0xffffffff
#define RG_CPLL1_VCOCAL_EN (1 << 25)
#define ABIST_MODE_SET (0x5C<<24)
#define ABIST_MODE_SET_MSK (0xFF<<24)
#define ABIST_MODE_EN (1<<19)
#define ABIST_LV_EN (1<<16)
#define RG_DISC_TH  (0x3<<0)
#define RG_CK148M_EN (1<<4)

#define HDMI_SYS_PLLCTRL7  0x348
#define PLLCTRL7_MASK  0xffffffff
#define TX_DRV_ENABLE (0xFF<<16)
#define TX_DRV_ENABLE_MSK (0xFF<<16)

  /* ////////////////////////////////////////////////////////////// */
#define HDMI_CON0	0x100

#define RG_HDMITX_PLL_EN (1 << 31)
#define RG_HDMITX_PLL_FBKDIV (0x7f << 24)
#define PLL_FBKDIV_SHIFT (24)
#define RG_HDMITX_PLL_FBKSEL (0x3 << 22)
#define PLL_FBKSEL_SHIFT (22)
#define RG_HDMITX_PLL_PREDIV (0x3 << 20)
#define PREDIV_SHIFT (20)
#define RG_HDMITX_PLL_POSDIV (0x3 << 18)
#define POSDIV_SHIFT (18)
#define RG_HDMITX_PLL_RST_DLY (0x3 << 16)
#define RG_HDMITX_PLL_IR (0xf << 12)
#define PLL_IR_SHIFT (12)
#define RG_HDMITX_PLL_IC (0xf << 8)
#define PLL_IC_SHIFT (8)
#define RG_HDMITX_PLL_BP (0xf << 4)
#define PLL_BP_SHIFT (4)
#define RG_HDMITX_PLL_BR (0x3 << 2)
#define PLL_BR_SHIFT (2)
#define RG_HDMITX_PLL_BC (0x3 << 0)
#define PLL_BC_SHIFT (0)

#define HDMI_CON1	0x104

#define RG_HDMITX_PLL_DIVEN (0x7 << 29)
#define PLL_DIVEN_SHIFT (29)

#define RG_HDMITX_PLL_AUTOK_EN (0x1 << 28)
#define RG_HDMITX_PLL_AUTOK_KF (0x3 << 26)
#define RG_HDMITX_PLL_AUTOK_KS (0x3 << 24)
#define RG_HDMITX_PLL_AUTOK_LOAD (0x1 << 23)
#define RG_HDMITX_PLL_BAND (0x3f << 16)
#define RG_HDMITX_PLL_REF_SEL (0x1 << 15)
#define RG_HDMITX_PLL_BIAS_EN (0x1 << 14)
#define RG_HDMITX_PLL_BIAS_LPF_EN (0x1 << 13)
#define RG_HDMITX_PLL_TXDIV_EN (0x1 << 12)
#define RG_HDMITX_PLL_TXDIV (0x3 << 10)
#define PLL_TXDIV_SHIFT (10)
#define RG_HDMITX_PLL_LVROD_EN (0x1 << 9)
#define RG_HDMITX_PLL_MONVC_EN (0x1 << 8)
#define RG_HDMITX_PLL_MONCK_EN (0x1 << 7)
#define RG_HDMITX_PLL_MONREF_EN (0x1 << 6)
#define RG_HDMITX_PLL_TST_EN (0x1 << 5)
#define RG_HDMITX_PLL_TST_CK_EN (0x1 << 4)
#define RG_HDMITX_PLL_TST_SEL (0xf << 0)


#define HDMI_CON2	0x108

#define RGS_HDMITX_PLL_AUTOK_BAND (0x7f << 8)
#define RGS_HDMITX_PLL_AUTOK_FAIL (0x1 << 1)
#define RG_HDMITX_EN_TX_CKLDO (0x1 << 0)


#define HDMI_CON3	0x10c

#define RG_HDMITX_SER_EN (0xf << 28)
#define RG_HDMITX_PRD_EN (0xf << 24)
#define RG_HDMITX_PRD_IMP_EN (0xf << 20)
#define RG_HDMITX_DRV_EN (0xf << 16)
#define RG_HDMITX_DRV_IMP_EN (0xf << 12)
#define DRV_IMP_EN_SHIFT (12)

#define RG_HDMITX_MHLCK_FORCE (0x1 << 10)
#define RG_HDMITX_MHLCK_PPIX_EN (0x1 << 9)
#define RG_HDMITX_MHLCK_EN (0x1 << 8)
#define RG_HDMITX_SER_DIN_SEL (0xf << 4)
#define RG_HDMITX_SER_5T1_BIST_EN (0x1 << 3)
#define RG_HDMITX_SER_BIST_TOG (0x1 << 2)
#define RG_HDMITX_SER_DIN_TOG (0x1 << 1)
#define RG_HDMITX_SER_CLKDIG_INV (0x1 << 0)


#define HDMI_CON4	0x110

#define RG_HDMITX_PRD_IBIAS_CLK (0xf << 24)
#define RG_HDMITX_PRD_IBIAS_D2 (0xf << 16)
#define RG_HDMITX_PRD_IBIAS_D1 (0xf << 8)
#define RG_HDMITX_PRD_IBIAS_D0 (0xf << 0)
#define PRD_IBIAS_CLK_SHIFT (24)
#define PRD_IBIAS_D2_SHIFT (16)
#define PRD_IBIAS_D1_SHIFT (8)
#define PRD_IBIAS_D0_SHIFT (0)


#define HDMI_CON5	0x114

#define RG_HDMITX_DRV_IBIAS_CLK (0x3f << 24)
#define RG_HDMITX_DRV_IBIAS_D2 (0x3f << 16)
#define RG_HDMITX_DRV_IBIAS_D1 (0x3f << 8)
#define RG_HDMITX_DRV_IBIAS_D0 (0x3f << 0)
#define DRV_IBIAS_CLK_SHIFT (24)
#define DRV_IBIAS_D2_SHIFT (16)
#define DRV_IBIAS_D1_SHIFT (8)
#define DRV_IBIAS_D0_SHIFT (0)


#define HDMI_CON6	0x118

#define RG_HDMITX_DRV_IMP_CLK (0x3f << 24)
#define RG_HDMITX_DRV_IMP_D2 (0x3f << 16)
#define RG_HDMITX_DRV_IMP_D1 (0x3f << 8)
#define RG_HDMITX_DRV_IMP_D0 (0x3f << 0)
#define DRV_IMP_CLK_SHIFT (24)
#define DRV_IMP_D2_SHIFT (16)
#define DRV_IMP_D1_SHIFT (8)
#define DRV_IMP_D0_SHIFT (0)


#define HDMI_CON7	0x11c

#define RG_HDMITX_MHLCK_DRV_IBIAS (0x1f << 27)
#define RG_HDMITX_SER_DIN (0x3ff << 16)
#define RG_HDMITX_CHLDC_TST (0xf << 12)
#define RG_HDMITX_CHLCK_TST (0xf << 8)
#define RG_HDMITX_RESERVE (0xff << 0)


#define HDMI_CON8	0x120

#define RGS_HDMITX_2T1_LEV (0xf << 16)
#define RGS_HDMITX_2T1_EDG (0xf << 12)
#define RGS_HDMITX_5T1_LEV (0xf << 8)
#define RGS_HDMITX_5T1_EDG (0xf << 4)
#define RGS_HDMITX_PLUG_TST (0x1 << 0)

#define HDMI_CON9	0x124
#define TVDPLL_POSDIV_2 (0x7 << 16)


#if 0
#define IO_PAD_PD 0x58
#define IO_PAD_HOT_PLUG_PD (1<<21)
#define IO_PAD_UP 0x64
#define IO_PAD_HOT_PLUG_UP (1<<21)
#endif

#define PLL_TEST_CON0 0x40
#define TVDPLL_POSDIV_2 (0x7 << 16)

#define MHL_TVDPLL_CON0	0x270
#define RG_TVDPLL_EN			(1)
#define RG_TVDPLL_POSDIV				(4)
#define RG_TVDPLL_POSDIV_MASK			(0x07 << 4)
#define MHL_TVDPLL_CON1	0x274
#define RG_TVDPLL_SDM_PCW				(0)
#define RG_TVDPLL_SDM_PCW_MASK			(0x1FFFFF)
#define TVDPLL_SDM_PCW_CHG        (1 << 31)
#define TVDPLL_SDM_PCW_F        (1<<23)

#define MHL_TVDPLL_PWR	0x27C
#define RG_TVDPLL_PWR_ON		(1)


#define RG_HDMITX_DCLK (0x3ff << 0)


/* //////////////////////////////////////////////////////////// */
#define IO_PAD_PD 0x210
#define IO_PAD_HOT_PLUG_PD (1<<5)
#define IO_PAD_EN 0x110
#define IO_PAD_HOT_PLUG_EN (1<<5)

/* /////////////////////////////////////////////////////////////// */
/* hdmi2start */
/***********************************************/
#define TOP_CFG00 0x000

#define ABIST_ENABLE (1 << 31)
#define ABIST_DISABLE (0 << 31)
#define ABIST_SPEED_MODE_SET (1 << 27)
#define ABIST_SPEED_MODE_CLR (0 << 27)
#define ABIST_DATA_FORMAT_STAIR (0 << 24)
#define ABIST_DATA_FORMAT_0xAA (1 << 24)
#define ABIST_DATA_FORMAT_0x55 (2 << 24)
#define ABIST_DATA_FORMAT_0x11 (3 << 24)
#define ABIST_VSYNC_POL_LOW (0 << 23)
#define ABIST_VSYNC_POL_HIGH (1 << 23)
#define ABIST_HSYNC_POL_LOW (0 << 22)
#define ABIST_HSYNC_POL_HIGH (1 << 22)
#define ABIST_VIDEO_FORMAT_720x480P (0x2 << 16)
#define ABIST_VIDEO_FORMAT_720P60 (0x3 << 16)
#define ABIST_VIDEO_FORMAT_1080I60 (0x4 << 16)
#define ABIST_VIDEO_FORMAT_480I (0x5 << 16)
#define ABIST_VIDEO_FORMAT_1440x480P (0x9 << 16)
#define ABIST_VIDEO_FORMAT_1080P60 (0xA << 16)
#define ABIST_VIDEO_FORMAT_576P (0xB << 16)
#define ABIST_VIDEO_FORMAT_720P50 (0xC << 16)
#define ABIST_VIDEO_FORMAT_1080I50 (0xD << 16)
#define ABIST_VIDEO_FORMAT_576I (0xE << 16)
#define ABIST_VIDEO_FORMAT_1440x576P (0x12 << 16)
#define ABIST_VIDEO_FORMAT_1080P50 (0x13 << 16)
#define ABIST_VIDEO_FORMAT_3840x2160P25 (0x18 << 16)
#define ABIST_VIDEO_FORMAT_3840x2160P30 (0x19 << 16)
#define ABIST_VIDEO_FORMAT_4096X2160P30 (0x1A << 16)
#define HDMI2_BYP_ON (1 << 13)
#define HDMI2_BYP_OFF (0 << 13)
#define DEEPCOLOR_PAT_EN (1 << 12)
#define DEEPCOLOR_PAT_DIS (0 << 12)
#define DEEPCOLOR_MODE_8BIT (0 << 8)
#define DEEPCOLOR_MODE_10BIT (1 << 8)
#define DEEPCOLOR_MODE_12BIT (2 << 8)
#define DEEPCOLOR_MODE_16BIT (3 << 8)
#define DEEPCOLOR_MODE_MASKBIT (3 << 8)

#define HDMIMODE_VAL_HDMI (1 << 7)
#define HDMIMODE_VAL_DVI (0 << 7)
#define HDMIMODE_OVR_ON (1 << 6)
#define HDMIMODE_OVR_OFF (0 << 6)
#define SCR_MODE_CTS (1 << 5)
#define SCR_MODE_NORMAL (0 << 5)
#define SCR_ON (1 << 4)
#define SCR_OFF (0 << 4)
#define HDMI_MODE_HDMI (1 << 3)
#define HDMI_MODE_DVI (0 << 3)
#define HDMI2_ON (1 << 2)
#define HDMI2_OFF (0 << 2)


/***********************************************/
#define TOP_CFG01 0x004

#define VIDEO_MUTE_DATA (0xFFFFFF << 4)
#define NULL_PKT_VSYNC_HIGH_EN (1 << 3)
#define NULL_PKT_VSYNC_HIGH_DIS (0 << 3)
#define NULL_PKT_EN (1 << 2)
#define NULL_PKT_DIS (0 << 2)
#define CP_CLR_MUTE_EN (1 << 1)
#define CP_CLR_MUTE_DIS (0 << 1)
#define CP_SET_MUTE_EN (1 << 0)
#define CP_SET_MUTE_DIS (0 << 0)


/***********************************************/
#define TOP_CFG02 0x008

#define MHL_IEEE_NO (0xFFFFFF << 8)
#define MHL_VFMT_EXTD (0x3 << 5)
#define VSI_OVERRIDE_DIS (0x1 << 4)
#define AVI_OVERRIDE_DIS (0x1 << 3)
#define MINIVSYNC_ON (0x1 << 2)
#define MHL_3DCONV_EN (0x1 << 1)
#define INTR_ENCRYPTION_EN (0x1 << 0)
#define INTR_ENCRYPTION_DIS (0x0 << 0)

/***********************************************/
#define TOP_AUD_MAP 0x00C

#define SD7_MAP (0x7 << 28)
#define SD6_MAP (0x7 << 24)
#define SD5_MAP (0x7 << 20)
#define SD4_MAP (0x7 << 16)
#define SD3_MAP (0x7 << 12)
#define SD2_MAP (0x7 << 8)
#define SD1_MAP (0x7 << 4)
#define SD0_MAP (0x7 << 0)

#define C_SD7 (0x7 << 28)
#define C_SD6 (0x6 << 24)
#define C_SD5 (0x5 << 20)
#define C_SD4 (0x4 << 16)
#define C_SD3 (0x3 << 12)
#define C_SD2 (0x2 << 8)
#define C_SD1 (0x1 << 4)
#define C_SD0 (0x0 << 0)


/***********************************************/
#define TOP_OUT00 0x010

#define QC_SEL (0x3 << 10)
#define Q2_SEL (0x3 << 8)
#define Q1_SEL (0x3 << 6)
#define Q0_SEL (0x3 << 4)
#define USE_CH_MUX4 (0x1 << 2)
#define USE_CH_MUX0 (0x0 << 2)
#define TX_BIT_INV_EN (0x1 << 1)
#define TX_BIT_INV_DIS (0x0 << 1)
#define Q_9T0_BIT_SWAP (0x1 << 0)
#define Q_9T0_NO_SWAP (0x0 << 0)

#define SELECT_CK_CHANNEL 0x3
#define SELECT_Q2_CHANNEL 0x2
#define SELECT_Q1_CHANNEL 0x1
#define SELECT_Q0_CHANNEL 0x0

/***********************************************/
#define TOP_OUT01 0x014

#define TXC_DIV8 (0x3 << 0x30)
#define TXC_DIV4 (0x2 << 0x30)
#define TXC_DIV2 (0x1 << 0x30)
#define TXC_DIV1 (0x0 << 0x30)
#define TXC_DATA2 (0x3FF << 20)
#define TXC_DATA1 (0x3FF << 10)
#define TXC_DATA0 (0x3FF << 0)

/***********************************************/
#define TOP_OUT02 0x018

#define FIX_OUT_EN (0x1 << 31)
#define FIX_OUT_DIS (0x0 << 31)
#define QOUT_DATA (0x3FFFFFFF << 0)

/***********************************************/
#define TOP_INFO_EN 0x01C

#define VSIF_EN_WR (0x1 << 27)
#define VSIF_DIS_WR (0x0 << 27)
#define GAMUT_EN_WR (0x1 << 26)
#define GAMUT_DIS_WR (0x0 << 26)
#define GEN5_EN_WR (0x1 << 25)
#define GEN5_DIS_WR (0x0 << 25)
#define GEN4_EN_WR (0x1 << 24)
#define GEN4_DIS_WR (0x1 << 24)
#define GEN3_EN_WR (0x1 << 23)
#define GEN3_DIS_WR (0x0 << 23)
#define GEN2_EN_WR (0x1 << 22)
#define GEN2_DIS_WR (0x0 << 22)
#define CP_EN_WR (0x1 << 21)
#define CP_DIS_WR (0x0 << 21)
#define GEN_EN_WR (0x1 << 20)
#define GEN_DIS_WR (0x0 << 20)
#define MPEG_EN_WR (0x1 << 19)
#define MPEG_DIS_WR (0x0 << 19)
#define AUD_EN_WR (0x1 << 18)
#define AUD_DIS_WR (0x0 << 18)
#define SPD_EN_WR (0x1 << 17)
#define SPD_DIS_WR (0x0 << 17)
#define AVI_EN_WR (0x1 << 16)
#define AVI_DIS_WR (0x1 << 16)
#define VSIF_EN (0x1 << 11)
#define VSIF_DIS (0x0 << 11)
#define HDMI2_GAMUT_EN (0x1 << 10)
#define HDMI2_GAMUT_DIS (0x0 << 10)
#define GEN5_EN (0x1 << 9)
#define GEN5_DIS (0x0 << 9)
#define GEN4_EN (0x1 << 8)
#define GEN4_DIS (0x1 << 8)
#define GEN3_EN (0x1 << 7)
#define GEN3_DIS (0x0 << 7)
#define GEN2_EN (0x1 << 6)
#define GEN2_DIS (0x0 << 6)
#define CP_EN (0x1 << 5)
#define CP_DIS (0x0 << 5)
#define GEN_EN (0x1 << 4)
#define GEN_DIS (0x0 << 4)
#define MPEG_EN (0x1 << 3)
#define MPEG_DIS (0x0 << 3)
#define AUD_EN (0x1 << 2)
#define AUD_DIS (0x0 << 2)
#define SPD_EN (0x1 << 1)
#define SPD_DIS (0x0 << 1)
#define AVI_EN (0x1 << 0)
#define AVI_DIS (0x0 << 0)

/***********************************************/
#define TOP_INFO_RPT 0x020

#define VSIF_RPT_EN (0x1 << 11)
#define VSIF_RPT_DIS (0x0 << 11)
#define GAMUT_RPT_EN (0x1 << 10)
#define GAMUT_RPT_DIS (0x0 << 10)
#define GEN5_RPT_EN (0x1 << 9)
#define GEN5_RPT_DIS (0x0 << 9)
#define GEN4_RPT_EN (0x1 << 8)
#define GEN4_RPT_DIS (0x1 << 8)
#define GEN3_RPT_EN (0x1 << 7)
#define GEN3_RPT_DIS (0x0 << 7)
#define GEN2_RPT_EN (0x1 << 6)
#define GEN2_RPT_DIS (0x0 << 6)
#define CP_RPT_EN (0x1 << 5)
#define CP_RPT_DIS (0x0 << 5)
#define GEN_RPT_EN (0x1 << 4)
#define GEN_RPT_DIS (0x0 << 4)
#define MPEG_RPT_EN (0x1 << 3)
#define MPEG_RPT_DIS (0x0 << 3)
#define AUD_RPT_EN (0x1 << 2)
#define AUD_RPT_DIS (0x0 << 2)
#define SPD_RPT_EN (0x1 << 1)
#define SPD_RPT_DIS (0x0 << 1)
#define AVI_RPT_EN (0x1 << 0)
#define AVI_RPT_DIS (0x1 << 0)

/***********************************************/
#define TOP_AVI_HEADER 0x024

#define AVI_HEADER (0xFFFFFF << 0)

/***********************************************/
#define TOP_AVI_PKT00 0x028

#define AVI_PKT00 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_AVI_PKT01 0x02C

#define AVI_PKT01 (0xFFFFFF << 0)

/***********************************************/
#define TOP_AVI_PKT02 0x030

#define AVI_PKT02 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_AVI_PKT03 0x034

#define AVI_PKT03 (0xFFFFFF << 0)

/***********************************************/
#define TOP_AVI_PKT04 0x038

#define AVI_PKT04 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_AVI_PKT05 0x03C

#define AVI_PKT05 (0xFFFFFF << 0)

/***********************************************/
#define TOP_AIF_HEADER 0x040

#define AIF_HEADER (0x00FFFFFF << 0)

/***********************************************/
#define TOP_AIF_PKT00 0x044

#define AIF_PKT00 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_AIF_PKT01 0x048

#define AIF_PKT01 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_AIF_PKT02 0x04C

#define AIF_PKT02 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_AIF_PKT03 0x050

#define AIF_PKT03 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_SPDIF_HEADER 0x054

#define SPDIF_HEADER (0x00FFFFFF << 0)

/***********************************************/
#define TOP_SPDIF_PKT00 0x058

#define SPDIF_PKT00 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_SPDIF_PKT01 0x05C

#define SPDIF_PKT01 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_SPDIF_PKT02 0x060

#define SPDIF_PKT02 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_SPDIF_PKT03 0x064

#define SPDIF_PKT03 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_SPDIF_PKT04 0x068

#define SPDIF_PKT04 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_SPDIF_PKT05 0x06C

#define SPDIF_PKT05 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_SPDIF_PKT06 0x070

#define SPDIF_PKT06 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_SPDIF_PKT07 0x074

#define SPDIF_PKT07 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_MPEG_HEADER 0x078

#define MPEG_HEADER (0x00FFFFFF << 0)

/***********************************************/
#define TOP_MPEG_PKT00 0x07C

#define MPEG_PKT00 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_MPEG_PKT01 0x080

#define MPEG_PKT01 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_MPEG_PKT02 0x084

#define MPEG_PKT02 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_MPEG_PKT03 0x088

#define MPEG_PKT03 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_MPEG_PKT04 0x08C

#define MPEG_PKT04 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_MPEG_PKT05 0x090

#define MPEG_PKT05 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_MPEG_PKT06 0x094

#define MPEG_PKT06 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_MPEG_PKT07 0x098

#define MPEG_PKT07 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN_HEADER 0x09C

#define GEN_HEADER (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN_PKT00 0x0A0

#define GEN_PKT00 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN_PKT01 0x0A4

#define GEN_PKT01 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN_PKT02 0x0A8

#define GEN_PKT02 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN_PKT03 0x0AC

#define GEN_PKT03 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN_PKT04 0x0B0

#define GEN_PKT04 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN_PKT05 0x0B4

#define GEN_PKT05 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN_PKT06 0x0B8

#define GEN_PKT06 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN_PKT07 0x0BC

#define GEN_PKT07 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN2_HEADER 0x0C0

#define GEN2_HEADER (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN2_PKT00 0x0C4

#define GEN2_PKT00 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN2_PKT01 0x0C8

#define GEN2_PKT01 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN2_PKT02 0x0CC

#define GEN2_PKT02 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN2_PKT03 0x0D0

#define GEN2_PKT03 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN2_PKT04 0x0D4

#define GEN2_PKT04 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN2_PKT05 0x0D8

#define GEN2_PKT05 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN2_PKT06 0x0DC

#define GEN2_PKT06 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN2_PKT07 0x0E0

#define GEN2_PKT07 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN3_HEADER 0x0E4

#define GEN3_HEADER (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN3_PKT00 0x0E8

#define GEN3_PKT00 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN3_PKT01 0x0EC

#define GEN3_PKT01 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN3_PKT02 0x0F0

#define GEN3_PKT02 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN3_PKT03 0x0F4

#define GEN3_PKT03 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN3_PKT04 0x0F8

#define GEN3_PKT04 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN3_PKT05 0x0FC

#define GEN3_PKT05 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN3_PKT06 0x100

#define GEN3_PKT06 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN3_PKT07 0x104

#define GEN3_PKT07 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN4_HEADER 0x108

#define GEN4_HEADER (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN4_PKT00 0x10C

#define GEN4_PKT00 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN4_PKT01 0x110

#define GEN4_PKT01 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN4_PKT02 0x114

#define GEN4_PKT02 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN4_PKT03 0x118

#define GEN4_PKT03 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN4_PKT04 0x11C

#define GEN4_PKT04 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN4_PKT05 0x120

#define GEN4_PKT05 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN4_PKT06 0x124

#define GEN4_PKT06 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN4_PKT07 0x128

#define GEN4_PKT07 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN5_HEADER 0x12C

#define GEN5_HEADER (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN5_PKT00 0x130

#define GEN5_PKT00 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN5_PKT01 0x134

#define GEN5_PKT01 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN5_PKT02 0x138

#define GEN5_PKT02 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN5_PKT03 0x13C

#define GEN5_PKT03 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN5_PKT04 0x140

#define GEN5_PKT04 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN5_PKT05 0x144

#define GEN5_PKT05 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GEN5_PKT06 0x148

#define GEN5_PKT06 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GEN5_PKT07 0x14C

#define GEN5_PKT07 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GAMUT_HEADER 0x150

#define GAMUT_HEADER (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GAMUT_PKT00 0x154

#define GAMUT_PKT00 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GAMUT_PKT01 0x158

#define GAMUT_PKT01 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GAMUT_PKT02 0x15C

#define GAMUT_PKT02 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GAMUT_PKT03 0x160

#define GAMUT_PKT03 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GAMUT_PKT04 0x164

#define GAMUT_PKT04 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GAMUT_PKT05 0x168

#define GAMUT_PKT05 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_GAMUT_PKT06 0x16C

#define GAMUT_PKT06 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_GAMUT_PKT07 0x170

#define GAMUT_PKT07 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_VSIF_HEADER 0x174

#define VSIF_HEADER (0x00FFFFFF << 0)

/***********************************************/
#define TOP_VSIF_PKT00 0x178

#define VSIF_PKT00 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_VSIF_PKT01 0x17C

#define VSIF_PKT01 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_VSIF_PKT02 0x180

#define VSIF_PKT02 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_VSIF_PKT03 0x184

#define VSIF_PKT03 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_VSIF_PKT04 0x188

#define VSIF_PKT04 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_VSIF_PKT05 0x18C

#define VSIF_PKT05 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_VSIF_PKT06 0x190

#define VSIF_PKT06 (0xFFFFFFFF << 0)

/***********************************************/
#define TOP_VSIF_PKT07 0x194

#define VSIF_PKT07 (0x00FFFFFF << 0)

/***********************************************/
#define TOP_DROP_CFG00 0x198

/***********************************************/
#define TOP_DROP_CFG01 0x19C

/***********************************************/
#define TOP_CLK_RST_CFG 0x1A0

#define SLOW_CLK_DIV_CNT (0xFF << 16)
#define HDCP_TCLK_EN (0x1 << 8)
#define HDCP_TCLK_DIS (0x0 << 8)
#define SOFT_HDCP_CORE_RST (0x1 << 1)
#define SOFT_HDCP_CORE_NOR (0x0 << 1)
#define SOFT_HDCP_RST (0x1 << 0)
#define SOFT_HDCP_NOR (0x0 << 0)

/***********************************************/
#define TOP_MISC_CTLR 0x1A4

#define INFO_HEADER (0xFF << 16)
#define HSYNC_POL_POS (0x1 << 1)
#define HSYNC_POL_NEG (0x0 << 1)
#define VSYNC_POL_POS (0x1 << 0)
#define VSYNC_POL_NEG (0x0 << 0)

/***********************************************/
#define TOP_INT_STA00 0x1A8

#define DDC_FIFO_HALF_FULL_INT_STA (1 << 31)
#define DDC_FIFO_FULL_INT_STA (1 << 30)
#define DDC_I2C_IN_PROG_INT_STA (1 << 29)
#define HDCP_RI_128_INT_STA (1 << 28)
#define HDCP_SHA_START_INT_STA (1 << 27)
#define HDCP2X_RX_RPT_READY_DDCM_INT_STA (1 << 26)
#define HDCP2X_RX_REAUTH_REQ_DDCM_INT_STA (1 << 25)
#define HDCP2X_RPT_SMNG_XFER_DONE_INT_STA (1 << 24)
#define HDCP2X_RPT_RCVID_CHANGED_INT_STA (1 << 23)
#define HDCP2X_CERT_SEND_RCVD_INT_STA (1 << 22)
#define HDCP2X_SKE_SENT_RCVD_INT_STA (1 << 21)
#define HDCP2X_AKE_SENT_RCVD_INT_STA (1 << 20)
#define HDCP2X_HASH_FAIL_INT_STA (1 << 19)
#define HDCP2X_CCHK_DONE_INT_STA (1 << 18)
#define HDCP2X_AUTH_FAIL_INT_STA (1 << 17)
#define HDCP2X_AUTH_DONE_INT_STA (1 << 16)
#define HDCP2X_MSG_7_INT_STA (1 << 15)
#define HDCP2X_MSG_6_INT_STA (1 << 14)
#define HDCP2X_MSG_5_INT_STA (1 << 13)
#define HDCP2X_MSG_4_INT_STA (1 << 12)
#define HDCP2X_MSG_3_INT_STA (1 << 11)
#define HDCP2X_MSG_2_INT_STA (1 << 10)
#define HDCP2X_MSG_1_INT_STA (1 << 9)
#define HDCP2X_MSG_0_INT_STA (1 << 8)
#define INFO_DONE_INT_STA (1 << 7)
#define PB_FULL_INT_STA (1 << 6)
#define AUDIO_INT_STA (1 << 5)
#define VSYNC_INT_STA (1 << 4)
#define PORD_F_INT_STA (1 << 3)
#define PORD_R_INT_STA (1 << 2)
#define HTPLG_F_INT_STA (1 << 1)
#define HTPLG_R_INT_STA (1 << 0)

/***********************************************/
#define TOP_INT_STA01 0x1AC

#define HDCP2X_STATE_CHANGE_P_INT_STA (1 << 10)
#define SCDC_UP_FLAG_DONE_INT_STA (1 << 9)
#define SCDC_SLAVE_RD_REQ_P_INT_STA (1 << 8)
#define SCDC_DDC_CONFICT_INT_STA (1 << 7)
#define SCDC_DDC_DONE_INT_STA (1 << 6)
#define RI_ERR3_INT_STA (1 << 5)
#define RI_ERR2_INT_STA (1 << 4)
#define RI_ERR1_INT_STA (1 << 3)
#define RI_ERR0_INT_STA (1 << 2)
#define RI_BCAP_ON_INT_STA (1 << 1)
#define DDC_FIFO_EMPTY_INT_STA (1 << 0)

/***********************************************/
#define TOP_INT_MASK00 0x1B0

#define DDC_FIFO_HALF_FULL_INT_MASK (1 << 31)
#define DDC_FIFO_HALF_FULL_INT_UNMASK (0 << 31)
#define DDC_FIFO_FULL_INT_MASK (1 << 30)
#define DDC_FIFO_FULL_INT_UNMASK (0 << 30)
#define DDC_I2C_IN_PROG_INT_MASK (1 << 29)
#define DDC_I2C_IN_PROG_INT_UNMASK (0 << 29)
#define HDCP_RI_128_INT_MASK (1 << 28)
#define HDCP_RI_128_INT_UNMASK (0 << 28)
#define HDCP_SHA_START_INT_MASK (1 << 27)
#define HDCP_SHA_START_INT_UNMASK (0 << 27)
#define HDCP2X_RX_RPT_READY_DDCM_INT_MASK (1 << 26)
#define HDCP2X_RX_RPT_READY_DDCM_INT_UNMASK (0 << 26)
#define HDCP2X_RX_REAUTH_REQ_DDCM_INT_MASK (1 << 25)
#define HDCP2X_RX_REAUTH_REQ_DDCM_INT_UNMASK (0 << 25)
#define HDCP2X_RPT_SMNG_XFER_DONE_INT_MASK (1 << 24)
#define HDCP2X_RPT_SMNG_XFER_DONE_INT_UNMASK (0 << 24)
#define HDCP2X_RPT_RCVID_CHANGED_INT_MASK (1 << 23)
#define HDCP2X_RPT_RCVID_CHANGED_INT_UNMASK (0 << 23)
#define HDCP2X_CERT_SEND_RCVD_INT_MASK (1 << 22)
#define HDCP2X_CERT_SEND_RCVD_INT_UNMASK (0 << 22)
#define HDCP2X_SKE_SENT_RCVD_INT_MASK (1 << 21)
#define HDCP2X_SKE_SENT_RCVD_INT_UNMASK (0 << 21)
#define HDCP2X_AKE_SENT_RCVD_INT_MASK (1 << 20)
#define HDCP2X_AKE_SENT_RCVD_INT_UNMASK (0 << 20)
#define HDCP2X_HASH_FAIL_INT_MASK (1 << 19)
#define HDCP2X_HASH_FAIL_INT_UNMASK (0 << 19)
#define HDCP2X_CCHK_DONE_INT_MASK (1 << 18)
#define HDCP2X_CCHK_DONE_INT_UNMASK (0 << 18)
#define HDCP2X_AUTH_FAIL_INT_MASK (1 << 17)
#define HDCP2X_AUTH_FAIL_INT_UNMASK (0 << 17)
#define HDCP2X_AUTH_DONE_INT_MASK (1 << 16)
#define HDCP2X_AUTH_DONE_INT_UNMASK (0 << 16)
#define HDCP2X_MSG_7_INT_MASK (1 << 15)
#define HDCP2X_MSG_7_INT_UNMASK (0 << 15)
#define HDCP2X_MSG_6_INT_MASK (1 << 14)
#define HDCP2X_MSG_6_INT_UNMASK (0 << 14)
#define HDCP2X_MSG_5_INT_MASK (1 << 13)
#define HDCP2X_MSG_5_INT_UNMASK (0 << 13)
#define HDCP2X_MSG_4_INT_MASK (1 << 12)
#define HDCP2X_MSG_4_INT_UNMASK (0 << 12)
#define HDCP2X_MSG_3_INT_MASK (1 << 11)
#define HDCP2X_MSG_3_INT_UNMASK (0 << 11)
#define HDCP2X_MSG_2_INT_MASK (1 << 10)
#define HDCP2X_MSG_2_INT_UNMASK (0 << 10)
#define HDCP2X_MSG_1_INT_MASK (1 << 9)
#define HDCP2X_MSG_1_INT_UNMASK (0 << 9)
#define HDCP2X_MSG_0_INT_MASK (1 << 8)
#define HDCP2X_MSG_0_INT_UNMASK (0 << 8)
#define INFO_DONE_INT_MASK (1 << 7)
#define INFO_DONE_INT_UNMASK (0 << 7)
#define PB_FULL_INT_MASK (1 << 6)
#define PB_FULL_INT_UNMASK (0 << 6)
#define AUDIO_INT_MASK (1 << 5)
#define AUDIO_INT_UNMASK (0 << 5)
#define VSYNC_INT_MASK (1 << 4)
#define VSYNC_INT_UNMASK (0 << 4)
#define PORD_F_INT_MASK (1 << 3)
#define PORD_F_INT_UNMASK (0 << 3)
#define PORD_R_INT_MASK (1 << 2)
#define PORD_R_INT_UNMASK (0 << 2)
#define HTPLG_F_INT_MASK (1 << 1)
#define HTPLG_F_INT_UNMASK (0 << 1)
#define HTPLG_R_INT_MASK (1 << 0)
#define HTPLG_R_INT_UNMASK (0 << 0)

/***********************************************/
#define TOP_INT_MASK01 0x1B4

#define HDCP2X_STATE_CHANGE_P_INT_MASK (1 << 10)
#define SCDC_UP_FLAG_DONE_INT_MASK (1 << 9)
#define SCDC_SLAVE_RD_REQ_P_INT_MASK (1 << 8)
#define SCDC_DDC_CONFICT_INT_MASK (1 << 7)
#define SCDC_DDC_DONE_INT_MASK (1 << 6)
#define RI_ERR3_INT_MASK (1 << 5)
#define RI_ERR2_INT_MASK (1 << 4)
#define RI_ERR1_INT_MASK (1 << 3)
#define RI_ERR0_INT_MASK (1 << 2)
#define RI_BCAP_ON_INT_MASK (1 << 1)
#define DDC_FIFO_EMPTY_INT_MASK (1 << 0)

#define HDCP2X_STATE_CHANGE_P_INT_UNMASK (0 << 10)
#define SCDC_UP_FLAG_DONE_INT_UNMASK (0 << 9)
#define SCDC_SLAVE_RD_REQ_P_INT_UNMASK (0 << 8)
#define SCDC_DDC_CONFICT_INT_UNMASK (0 << 7)
#define SCDC_DDC_DONE_INT_UNMASK (0 << 6)
#define RI_ERR3_INT_UNMASK (0 << 5)
#define RI_ERR2_INT_UNMASK (0 << 4)
#define RI_ERR1_INT_UNMASK (0 << 3)
#define RI_ERR0_INT_UNMASK (0 << 2)
#define RI_BCAP_ON_INT_UNMASK (0 << 1)
#define DDC_FIFO_EMPTY_INT_UNMASK (0 << 0)
/***********************************************/
#define TOP_INT_CLR00 0x1B8

#define DDC_FIFO_HALF_FULL_INT_CLR (1 << 31)
#define DDC_FIFO_FULL_INT_CLR (1 << 30)
#define DDC_I2C_IN_PROG_INT_CLR (1 << 29)
#define HDCP_RI_128_INT_CLR (1 << 28)
#define HDCP_SHA_START_INT_CLR (1 << 27)
#define HDCP2X_RX_RPT_READY_DDCM_INT_CLR (1 << 26)
#define HDCP2X_RX_REAUTH_REQ_DDCM_INT_CLR (1 << 25)
#define HDCP2X_RPT_SMNG_XFER_DONE_INT_CLR (1 << 24)
#define HDCP2X_RPT_RCVID_CHANGED_INT_CLR (1 << 23)
#define HDCP2X_CERT_SEND_RCVD_INT_CLR (1 << 22)
#define HDCP2X_SKE_SENT_RCVD_INT_CLR (1 << 21)
#define HDCP2X_AKE_SENT_RCVD_INT_CLR (1 << 20)
#define HDCP2X_HASH_FAIL_INT_CLR (1 << 19)
#define HDCP2X_CCHK_DONE_INT_CLR (1 << 18)
#define HDCP2X_AUTH_FAIL_INT_CLR (1 << 17)
#define HDCP2X_AUTH_DONE_INT_CLR (1 << 16)
#define HDCP2X_MSG_7_INT_CLR (1 << 15)
#define HDCP2X_MSG_6_INT_CLR (1 << 14)
#define HDCP2X_MSG_5_INT_CLR (1 << 13)
#define HDCP2X_MSG_4_INT_CLR (1 << 12)
#define HDCP2X_MSG_3_INT_CLR (1 << 11)
#define HDCP2X_MSG_2_INT_CLR (1 << 10)
#define HDCP2X_MSG_1_INT_CLR (1 << 9)
#define HDCP2X_MSG_0_INT_CLR (1 << 8)
#define INFO_DONE_INT_CLR (1 << 7)
#define PB_FULL_INT_CLR (1 << 6)
#define AUDIO_INT_CLR (1 << 5)
#define VSYNC_INT_CLR (1 << 4)
#define PORD_F_INT_CLR (1 << 3)
#define PORD_R_INT_CLR (1 << 2)
#define HTPLG_F_INT_CLR (1 << 1)
#define HTPLG_R_INT_CLR (1 << 0)

#define DDC_FIFO_HALF_FULL_INT_UNCLR (0 << 31)
#define DDC_FIFO_FULL_INT_UNCLR (0 << 30)
#define DDC_I2C_IN_PROG_INT_UNCLR (0 << 29)
#define HDCP_RI_128_INT_UNCLR (0 << 28)
#define HDCP_SHA_START_INT_UNCLR (0 << 27)
#define HDCP2X_RX_RPT_READY_DDCM_INT_UNCLR (0 << 26)
#define HDCP2X_RX_REAUTH_REQ_DDCM_INT_UNCLR (0 << 25)
#define HDCP2X_RPT_SMNG_XFER_DONE_INT_UNCLR (0 << 24)
#define HDCP2X_RPT_RCVID_CHANGED_INT_UNCLR (0 << 23)
#define HDCP2X_CERT_SEND_RCVD_INT_UNCLR (0 << 22)
#define HDCP2X_SKE_SENT_RCVD_INT_UNCLR (0 << 21)
#define HDCP2X_AKE_SENT_RCVD_INT_UNCLR (0 << 20)
#define HDCP2X_HASH_FAIL_INT_UNCLR (0 << 19)
#define HDCP2X_CCHK_DONE_INT_UNCLR (0 << 18)
#define HDCP2X_AUTH_FAIL_INT_UNCLR (0 << 17)
#define HDCP2X_AUTH_DONE_INT_UNCLR (0 << 16)
#define HDCP2X_MSG_7_INT_UNCLR (0 << 15)
#define HDCP2X_MSG_6_INT_UNCLR (0 << 14)
#define HDCP2X_MSG_5_INT_UNCLR (0 << 13)
#define HDCP2X_MSG_4_INT_UNCLR (0 << 12)
#define HDCP2X_MSG_3_INT_UNCLR (0 << 11)
#define HDCP2X_MSG_2_INT_UNCLR (0 << 10)
#define HDCP2X_MSG_1_INT_UNCLR (0 << 9)
#define HDCP2X_MSG_0_INT_UNCLR (0 << 8)
#define INFO_DONE_INT_UNCLR (0 << 7)
#define PB_FULL_INT_UNCLR (0 << 6)
#define AUDIO_INT_UNCLR (0 << 5)
#define VSYNC_INT_UNCLR (0 << 4)
#define PORD_F_INT_UNCLR (0 << 3)
#define PORD_R_INT_UNCLR (0 << 2)
#define HTPLG_F_INT_UNCLR (0 << 1)
#define HTPLG_R_INT_UNCLR (0 << 0)

/***********************************************/
#define TOP_INT_CLR01 0x1BC

#define HDCP2X_STATE_CHANGE_P_INT_CLR (1 << 10)
#define SCDC_UP_FLAG_DONE_INT_CLR (1 << 9)
#define SCDC_SLAVE_RD_REQ_P_INT_CLR (1 << 8)
#define SCDC_DDC_CONFICT_INT_CLR (1 << 7)
#define SCDC_DDC_DONE_INT_CLR (1 << 6)
#define RI_ERR3_INT_CLR (1 << 5)
#define RI_ERR2_INT_CLR (1 << 4)
#define RI_ERR1_INT_CLR (1 << 3)
#define RI_ERR0_INT_CLR (1 << 2)
#define RI_BCAP_ON_INT_CLR (1 << 1)
#define DDC_FIFO_EMPTY_INT_CLR (1 << 0)

#define HDCP2X_STATE_CHANGE_P_INT_UNCLR (0 << 10)
#define SCDC_UP_FLAG_DONE_INT_UNCLR (0 << 9)
#define SCDC_SLAVE_RD_REQ_P_INT_UNCLR (0 << 8)
#define SCDC_DDC_CONFICT_INT_UNCLR (0 << 7)
#define SCDC_DDC_DONE_INT_UNCLR (0 << 6)
#define RI_ERR3_INT_UNCLR (0 << 5)
#define RI_ERR2_INT_UNCLR (0 << 4)
#define RI_ERR1_INT_UNCLR (0 << 3)
#define RI_ERR0_INT_UNCLR (0 << 2)
#define RI_BCAP_ON_INT_UNCLR (0 << 1)
#define DDC_FIFO_EMPTY_INT_UNCLR (0 << 0)

/***********************************************/
#define TOP_STA 0x1C0

#define STARTUP_FLAG (1 << 13)
#define P_STABLE (1 << 12)
#define CEA_AVI_EN (1 << 11)
#define CEA_SPD_EN (1 << 10)
#define CEA_AUD_EN (1 << 9)
#define CEA_MPEG_EN (1 << 8)
#define CEA_GEN_EN (1 << 7)
#define CEA_CP_EN (1 << 6)
#define CEA_GEN2_EN (1 << 5)
#define CEA_GEN3_EN (1 << 4)
#define CEA_GEN4_EN (1 << 3)
#define CEA_GEN5_EN (1 << 2)
#define CEA_GAMUT_EN (1 << 1)
#define CEA_VSIF_EN (1 << 0)

/***********************************************/
#define AIP_CTRL 0x400

#define I2S_EN (0xF << 16)
#define I2S_EN_SHIFT 16
#define DSD_EN (1 << 15)
#define HBRA_ON (1 << 14)
#define HBRA_OFF (0 << 14)
#define SPDIF_EN (1 << 13)
#define SPDIF_EN_SHIFT 13
#define SPDIF_DIS (0 << 13)
#define AUD_PKT_EN (1 << 12)
#define AUD_PAR_EN (1 << 10)
#define AUD_SEL_OWRT (1 << 9)
#define AUD_IN_EN (1 << 8)
#define AUD_IN_EN_SHIFT 8
#define FM_IN_VAL_SW (0x7 << 4)
#define MCLK_128FS 0x0
#define MCLK_256FS 0x1
#define MCLK_384FS 0x2
#define MCLK_512FS 0x3
#define MCLK_768FS 0x4
#define MCLK_1024FS 0x5
#define MCLK_1152FS 0x6
#define MCLK_192FS 0x7
#define NO_MCLK_CTSGEN_SEL (1 << 3)
#define MCLK_CTSGEN_SEL (0 << 3)
#define MCLK_EN (1 << 2)
#define CTS_REQ_EN (1 << 1)
#define CTS_REQ_DIS (0 << 1)
#define CTS_SW_SEL (1 << 0)
#define CTS_HW_SEL (0 << 0)

/***********************************************/
#define AIP_N_VAL 0x404

#define N_VAL_SW (0xFFFFF << 0)

/***********************************************/
#define AIP_CTS_SVAL 0x408

#define CTS_VAL_SW (0xFFFFF << 0)

/***********************************************/
#define AIP_SPDIF_CTRL 0x40C

#define I2S2DSD_EN (1 << 30)
#define AUD_ERR_THRESH (0x3F << 24)
#define AUD_ERR_THRESH_SHIFT 24

#define MAX_2UI_WRITE (0xFF << 16)
#define MAX_2UI_WRITE_SHIFT 16

#define MAX_1UI_WRITE (0xFF << 8)
#define MAX_1UI_WRITE_SHIFT 8

#define WR_2UI_LOCK (1 << 2)
#define WR_2UI_UNLOCK (0 << 2)
#define FS_OVERRIDE_WRITE (1 << 1)
#define FS_UNOVERRIDE (0 << 1)
#define WR_1UI_LOCK (1 << 0)
#define WR_1UI_UNLOCK (0 << 0)

/***********************************************/
#define AIP_I2S_CTRL 0x410

#define DSD_INTERLEAVE (0xFF << 20)
#define I2S_IN_LENGTH (0xF << 16)
#define I2S_IN_LENGTH_SHIFT 16
#define I2S_LENGTH_24BITS 0xB
#define I2S_LENGTH_23BITS 0x9
#define I2S_LENGTH_22BITS 0x5
#define I2S_LENGTH_21BITS 0xD
#define I2S_LENGTH_20BITS 0xA
#define I2S_LENGTH_19BITS 0x8
#define I2S_LENGTH_18BITS 0x4
#define I2S_LENGTH_17BITS 0xC
#define I2S_LENGTH_16BITS 0x2
#define SCK_EDGE_RISE (0x1 << 14)
#define SCK_EDGE_FALL (0x0 << 14)
#define CBIT_ORDER_SAME (0x1 << 13)
#define CBIT_ORDER_CON (0x0 << 13)
#define VBIT_PCM (0x0 << 12)
#define VBIT_COM (0x1 << 12)
#define WS_LOW (0x0 << 11)
#define WS_HIGH (0x1 << 11)
#define JUSTIFY_RIGHT (0x1 << 10)
#define JUSTIFY_LEFT (0x0 << 10)
#define DATA_DIR_MSB (0x0 << 9)
#define DATA_DIR_LSB (0x1 << 9)
#define I2S_1ST_BIT_SHIFT (0x0 << 8)
#define I2S_1ST_BIT_NOSHIFT (0x1 << 8)
#define FIFO3_MAP (0x3 << 6)
#define FIFO2_MAP (0x3 << 4)
#define FIFO1_MAP (0x3 << 2)
#define FIFO0_MAP (0x3 << 0)
#define MAP_SD0 0x0
#define MAP_SD1 0x1
#define MAP_SD2 0x2
#define MAP_SD3 0x3

/***********************************************/
#define AIP_I2S_CHST0 0x414

#define CBIT3B (0xF << 28)
#define CBIT3A (0xF << 24)
#define CBIT2B (0xF << 20)
#define CBIT2A (0xF << 16)
#define FS_2205KHZ 0x4
#define FS_441KHZ 0x0
#define FS_882KHZ 0x8
#define FS_1764KHZ 0xC
#define FS_24KHZ 0x6
#define FS_48KHZ 0x2
#define FS_96KHZ 0xA
#define FS_192KHZ 0xE
#define FS_32KHZ 0x3
#define FS_768KHZ 0x9
#define CBIT1 (0xFF << 8)
#define CBIT0 (0xFF << 0)

/***********************************************/
#define AIP_I2S_CHST1 0x418

#define CBIT_MSB (0xFFFF << 8)
#define CBIT4B (0xF << 4)
#define CBIT4A (0xF << 0)
#define NOT_INDICATE 0x0
#define BITS20_16 0x1
#define BITS22_18 0x2
#define BITS23_19 0x4
#define BITS24_20 0x5
#define BITS22_17 0x6

/***********************************************/
#define AIP_DOWNSAMPLE_CTRL 0x41C

/***********************************************/
#define AIP_PAR_CTRL 0x420

/***********************************************/
#define AIP_TXCTRL 0x424

#define AUD_MUTE_FIFO_EN (0x1 << 5)
#define AUD_MUTE_DIS (0x0 << 5)
#define LAYOUT0 (0x0 << 4)
#define LAYOUT1 (0x1 << 4)
#define RST4AUDIO_ACR (0x1 << 2)
#define RST4AUDIO_FIFO (0x1 << 1)
#define RST4AUDIO (0x1 << 0)

/***********************************************/
#define AIP_TPI_CTRL 0x428

#define TPI_AUD_SF_OVRD_VALUE (0x1 << 15)
#define TPI_AUD_SF_OVRD_STREAM (0x0 << 15)
#define TPI_AUD_SF (0x3F << 8)
#define SF2205 0x4
#define SF441 0x0
#define SF882 0x8
#define SF1764 0xC
#define SF3528 0xD
#define SF7056 0x2D
#define SF14112 0x1D
#define SF24 0x6
#define SF48 0x2
#define SF96 0xA
#define SF192 0xE
#define SF384 0x5
#define SF768 0x9
#define SF1536 0x15
#define SF32 0x3
#define SF64 0xB
#define SF128 0x2B
#define SF256 0x1B
#define SF512 0x3B
#define SF1024 0x35
#define TPI_SPDIF_SAMPLE_SIZE (0x3 << 6)
#define REFER_HEADER 0x0
#define SAMPLE_SIZE_BIT16 0x1
#define SAMPLE_SIZE_BIT20 0x2
#define SAMPLE_SIZE_BIT24 0x3
#define TPI_AUD_MUTE (0x1 << 4)
#define TPI_AUD_UNMUTE (0x0 << 4)
#define TPI_AUDIO_LOOKUP_EN (0x1 << 2)
#define TPI_AUDIO_LOOKUP_DIS (0x0 << 2)
#define TPI_AUD_HNDL (0x3 << 0)
#define BLOCK_AUDIO 0x0
#define DOWNSAMPLE 0x2
#define PASSAUDIO 0x3

/***********************************************/
#define AIP_INT_CTRL 0x42C

#define DSD_VALID_INT_MSK (0x1 << 24)
#define P_ERR_INT_MSK (0x1 << 23)
#define PREAMBLE_ERR_INT_MSK (0x1 << 22)
#define HW_CTS_CHANGED_INT_MSK (0x1 << 21)
#define PKT_OVRWRT_INT_MSK (0x1 << 20)
#define DROP_SMPL_ERR_INT_MSK (0x1 << 19)
#define BIPHASE_ERR_INT_MSK (0x1 << 18)
#define UNDERRUN_INT_MSK (0x1 << 17)
#define OVERRUN_INT_MSK (0x1 << 16)
#define DSD_VALID_INT_CLR (0x1 << 8)
#define P_ERR_INT_CLR (0x1 << 7)
#define PREAMBLE_ERR_INT_CLR (0x1 << 6)
#define HW_CTS_CHANGED_INT_CLR (0x1 << 5)
#define PKT_OVRWRT_INT_CLR (0x1 << 4)
#define DROP_SMPL_ERR_INT_CLR (0x1 << 3)
#define BIPHASE_ERR_INT_CLR (0x1 << 2)
#define UNDERRUN_INT_CLR (0x1 << 1)
#define OVERRUN_INT_CLR (0x1 << 0)
#define DSD_VALID_INT_UNMSK (0x0 << 24)
#define P_ERR_INT_UNMSK (0x0 << 23)
#define PREAMBLE_ERR_INT_UNMSK (0x0 << 22)
#define HW_CTS_CHANGED_INT_UNMSK (0x0 << 21)
#define PKT_OVRWRT_INT_UNMSK (0x0 << 20)
#define DROP_SMPL_ERR_INT_UNMSK (0x0 << 19)
#define BIPHASE_ERR_INT_UNMSK (0x0 << 18)
#define UNDERRUN_INT_UNMSK (0x0 << 17)
#define OVERRUN_INT_UNMSK (0x0 << 16)
#define DSD_VALID_INT_UNCLR (0x0 << 8)
#define P_ERR_INT_UNCLR (0x0 << 7)
#define PREAMBLE_ERR_INT_UNCLR (0x0 << 6)
#define HW_CTS_CHANGED_INT_UNCLR (0x0 << 5)
#define PKT_OVRWRT_INT_UNCLR (0x0 << 4)
#define DROP_SMPL_ERR_INT_UNCLR (0x0 << 3)
#define BIPHASE_ERR_INT_UNCLR (0x0 << 2)
#define UNDERRUN_INT_UNCLR (0x0 << 1)
#define OVERRUN_INT_UNCLR (0x0 << 0)

/***********************************************/
#define AIP_STA00 0x430

#define AUD_NO_AUDIO (0x1 << 29)
#define RO_2UI_LOCK (0x1 << 28)
#define FS_OVERRIDE_READ (0x1 << 27)
#define RO_1UI_LOCK (0x1 << 26)
#define AUD_ID (0x3F << 20)
#define N_VAL_SW (0xFFFFF << 0)

/***********************************************/
#define AIP_STA01 0x434

#define AUDIO_SPDIF_FS (0x3F << 24)
#define AUDIO_LENGTH (0xF << 20)
#define CTS_VAL_HW (0xFFFFF << 0)

/***********************************************/
#define AIP_STA02 0x438

#define FIFO_DIFF (0x3F << 24)
#define CBIT_L (0xFF << 16)
#define MAX_2UI_READ (0xFF << 8)
#define MAX_1UI_READ (0xFF << 0)

/***********************************************/
#define AIP_STA03 0x43C

#define SRC_EN (0x1 << 20)
#define SRC_CTRL (0x1 << 19)
#define LAYOUT (0x1 << 18)
#define AUD_MUTE_EN (0x1 << 17)
#define HDMI_MUTE (0x1 << 16)
#define DSD_VALID_INT_STA (0x1 << 8)
#define P_ERR_INT_STA (0x1 << 7)
#define PREAMBLE_ERR_INT_STA (0x1 << 6)
#define HW_CTS_CHANGED_INT_STA (0x1 << 5)
#define PKT_OVRWRT_INT_STA (0x1 << 4)
#define DROP_SMPL_ERR_INT_STA (0x1 << 3)
#define BIPHASE_ERR_INT_STA (0x1 << 2)
#define UNDERRUN_INT_STA (0x1 << 1)
#define OVERRUN_INT_STA (0x1 << 0)

/***********************************************/
#define HDCP_TOP_CTRL 0xC00

#define OTP2XAOVR_EN (0x1 << 13)
#define OTP2XVOVR_EN (0x1 << 12)
#define OTPAMUTEOVR_SET (0x1 << 10)
#define OTPADROPOVR_SET (0x1 << 9)
#define OTPVMUTEOVR_SET (0x1 << 8)
#define OTP14AOVR_EN (0x1 << 5)
#define OTP14VOVR_EN (0x1 << 4)
#define HDCP_DISABLE (0x1 << 0)
#define OTP2XAOVR_DIS (0x0 << 13)
#define OTP2XVOVR_DIS (0x0 << 12)
#define OTPAMUTEOVR_UNSET (0x0 << 10)
#define OTPADROPOVR_UNSET (0x0 << 9)
#define OTPVMUTEOVR_UNSET (0x0 << 8)
#define OTP14AOVR_DIS (0x0 << 5)
#define OTP14VOVR_DIS (0x0 << 4)
#define HDCP_ENABLE (0x0 << 0)

/***********************************************/
#define HPD_PORD_CTRL 0xC04

#define PORD_T2 (0xFF << 24)
#define PORD_T1 (0xFF << 16)
#define HPD_T2 (0xFF << 8)
#define HPD_T1 (0xFF << 0)

/***********************************************/
#define HPD_DDC_CTRL 0xC08

#define DDC_DELAY_CNT (0xFFFF << 16)
#define DDC_DELAY_CNT_SHIFT (16)

#define HW_DDC_MASTER (0x1 << 14)
#define DDC_DEBUG (0x1 << 13)
#define MAN_DDC_SYNC (0x1 << 12)
#define DSDA_SYNC (0x1 << 11)
#define DSCL_SYNC (0x1 << 10)
#define TPI_DDC_REQ_LEVEL (0x3 << 8)
#define DDC_GPU_REQUEST (0x1 << 7)
#define TPI_DDC_BURST_MODE (0x1 << 6)
#define DDC_SHORT_RI_RD (0x1 << 5)
#define DDC_FLT_EN_SYNC (0x1 << 4)
#define PORD_DEBOUNCE_EN (0x1 << 3)
#define HPD_DEBOUNCE_EN (0x1 << 2)
#define HPDIN_STA (0x1 << 1)
#define HPDIN_OVER_EN (0x1 << 0)

/***********************************************/
#define HDCP_RI_CTRL 0xC0C

#define RI_BCAP_EN (0x1 << 29)
#define RI_EN (0x1 << 28)
#define RI_128_COMP (0x7F << 20)
#define KSV_FORWARD (0x1 << 19)
#define INTERM_RI_CHECK_EN (0x1 << 18)
#define R0_ABSOLUTE  (0x1 << 17)
#define DOUBLE_RI_CHECK (0x1 << 16)
#define TPI_R0_CALC_TIME (0xF << 12)
#define RI_CHECK_SHIP (0x1 << 11)
#define LEGACY_TPI_RI_CHECK (0x7 << 8)
#define RI_LN_NUM (0xFF << 0)

/***********************************************/
#define DDC_CTRL 0xC10

#define DDC_CMD (0xF << 28)
#define DDC_CMD_SHIFT (28)

#define ABORT_TRANSACTION 0xF
#define CLEAR_FIFO 0x9
#define CLOCK_SCL 0xA
#define CURR_READ_NO_ACK 0x0
#define CURR_READ_ACK 0x1
#define SEQ_READ_NO_ACK 0x2
#define SEQ_READ_ACK 0x3
#define ENH_READ_NO_ACK 0x4
#define ENH_READ_ACK 0x5
#define SEQ_WRITE_IGN_ACK 0x6
#define SEQ_WRITE_REQ_ACK 0x7
#define DDC_DIN_CNT (0x3FF << 16)
#define DDC_DIN_CNT_SHIFT (16)

#define DDC_OFFSET (0xFF << 8)
#define DDC_OFFSET_SHIFT (8)

#define DDC_ADDR (0x7F << 1)

/***********************************************/
#define HDCP_TPI_CTRL 0xC14

#define EDID_MODE_EN (0x1 << 8)
#define CANCEL_PROT_EN (0x1 << 7)
#define TPI_AUTH_RETRY_CNT (0x7 << 4)
#define COPP_PROTLEVEL (0x1 << 3)
#define TPI_REAUTH_CTL (0x1 << 2)
#define TPI_HDCP_PREP_EN (0x1 << 1)
#define SW_TPI_EN (0x1 << 0)

/***********************************************/
#define SCDC_CTRL 0xC18

#define DDC_SEGMENT (0xFF << 8)
#define DDC_SEGMENT_SHIFT (8)

#define SCDC_AUTO_REPLY_STOP (0x1 << 3)
#define SCDC_AUTO_POLL (0x1 << 2)
#define SCDC_AUTO_REPLY (0x1 << 1)
#define SCDC_ACCESS (0x1 << 0)

/***********************************************/
#define TXDS_BSTATUS 0xC1C

#define DS_BSTATUS (0x7 << 13)
#define DS_HDMI_MODE (0x1 << 12)
#define DS_CASC_EXCEED (0x1 << 11)
#define DS_DEPTH (0x7 << 8)
#define DS_DEV_EXCEED (0x1 << 7)
#define DS_DEV_CNT (0x7F << 0)

/***********************************************/
#define HDCP2X_CTRL_0 0xC20

#define HDCP2X_CPVER (0xF << 20)
#define HDCP2X_CUPD_START (0x1 << 16)
#define HDCP2X_REAUTH_MSK (0xF << 12)
#define HDCP2X_REAUTH_MSK_SHIFT (12)

#define HDCP2X_HPD_SW (0x1 << 11)
#define HDCP2X_HPD_OVR (0x1 << 10)
#define HDCP2X_CTL3MSK (0x1 << 9)
#define HDCP2X_REAUTH_SW (0x1 << 8)
#define HDCP2X_ENCRYPT_EN (0x1 << 7)
#define HDCP2X_POLINT_SEL (0x1 << 6)
#define HDCP2X_POLINT_OVR (0x1 << 5)
#define HDCP2X_PRECOMPUTE (0x1 << 4)
#define HDCP2X_HDMIMODE (0x1 << 3)
#define HDCP2X_REPEATER (0x1 << 2)
#define HDCP2X_HDCPTX (0x1 << 1)
#define HDCP2X_EN (0x1 << 0)

/***********************************************/
#define HDCP2X_CTRL_1 0xC24

#define HDCP2X_CUPD_SIZE (0xFFFF << 0)

/***********************************************/
#define HDCP2X_CTRL_2 0xC28

#define HDCP2X_RINGOSC_BIST_START (0x1 << 28)
#define HDCP2X_MSG_SZ_CLR_OPTION (0x1 << 26)
#define HDCP2X_RPT_READY_CLR_OPTION (0x1 << 25)
#define HDCP2X_REAUTH_REQ_CLR_OPTION (0x1 << 24)
#define HDCP2X_RPT_SMNG_IN (0xFF << 16)
#define HDCP2X_RPT_SMNG_K (0xFF << 8)
#define HDCP2X_RPT_SMNG_XFER_START (0x1 << 4)
#define HDCP2X_RPT_SMNG_WR_START (0x1 << 3)
#define HDCP2X_RPT_SMNG_WR (0x1 << 2)
#define HDCP2X_RPT_RCVID_RD_START (0x1 << 1)
#define HDCP2X_RPT_RCVID_RD (0x1 << 0)

#define HDCP2X_RPT_SMNG_K_SHIFT (8)
#define HDCP2X_RPT_SMNG_IN_SHIFT (16)

/***********************************************/
#define HDCP2X_CTRL_STM 0xC2C

#define HDCP2X_STM_CTR (0xFFFFFFFF << 0)

/***********************************************/
#define HDCP2X_TEST_TP0 0xC30

#define HDCP2X_TP3 (0xFF << 24)
#define HDCP2X_TP2 (0xFF << 16)
#define HDCP2X_TP1 (0xFF << 8)
#define HDCP2X_TP0 (0xFF << 0)
#define HDCP2X_TP3_SHIFT (24)
#define HDCP2X_TP2_SHIFT (16)
#define HDCP2X_TP1_SHIFT (8)
#define HDCP2X_TP0_SHIFT (0)
/***********************************************/
#define HDCP2X_TEST_TP1 0xC34

#define HDCP2X_TP7 (0xFF << 24)
#define HDCP2X_TP6 (0xFF << 16)
#define HDCP2X_TP5 (0xFF << 8)
#define HDCP2X_TP4 (0xFF << 0)

/***********************************************/
#define HDCP2X_TEST_TP2 0xC38

#define HDCP2X_TP11 (0xFF << 24)
#define HDCP2X_TP10 (0xFF << 16)
#define HDCP2X_TP9 (0xFF << 8)
#define HDCP2X_TP8 (0xFF << 0)

/***********************************************/
#define HDCP2X_TEST_TP3 0xC3C

#define HDCP2X_TP15 (0xFF << 24)
#define HDCP2X_TP14 (0xFF << 16)
#define HDCP2X_TP13 (0xFF << 8)
#define HDCP2X_TP12 (0xFF << 0)

/***********************************************/
#define HDCP2X_GP_IN 0xC40

#define HDCP2X_GP_IN3 (0xFF << 24)
#define HDCP2X_GP_IN2 (0xFF << 16)
#define HDCP2X_GP_IN1 (0xFF << 8)
#define HDCP2X_GP_IN0 (0xFF << 0)
#define HDCP2X_GP_IN3_SHIFT (24)
#define HDCP2X_GP_IN2_SHIFT (16)
#define HDCP2X_GP_IN1_SHIFT (8)
#define HDCP2X_GP_IN0_SHIFT (0)
/***********************************************/
#define HDCP2X_DEBUG_CTRL 0xC44

#define HDCP2X_DB_CTRL3 (0xFF << 24)
#define HDCP2X_DB_CTRL2 (0xFF << 16)
#define HDCP2X_DB_CTRL1 (0xFF << 8)
#define HDCP2X_DB_CTRL0 (0xFF << 0)

/***********************************************/
#define HDCP2X_SPI_SI_ADDR 0xC48

#define HDCP2X_SPI_SI_S_ADDR (0xFFFF << 16)
#define HDCP2X_SPI_SI_E_ADDR (0xFFFF << 0)

/***********************************************/
#define HDCP2X_SPI_ADDR_CTRL 0xC4C

#define HDCP2X_SPI_SI_S_ADDR (0xFFFF << 16)
#define HDCP2X_SPI_SI_E_ADDR (0xFFFF << 0)

/***********************************************/
#define HDCP2X_RPT_SEQ_NUM 0xC50

#define MHL3_P0_STM_ID (0x7 << 24)
#define HDCP2X_RPT_SEQ_NUM_M (0xFFFFFF << 0)

/***********************************************/
#define HDCP2X_POL_CTRL 0xC54

#define HDCP2X_DIS_POLL_EN (0x1 << 16)
#define HDCP2X_POL_VAL1 (0xFF << 8)
#define HDCP2X_POL_VAL0 (0xFF << 0)

/***********************************************/
#define WR_PULSE 0xC58

#define HDCP2X_SPI_ADDR_RESET (0x1 << 3)
#define DDC_BUS_LOW (0x1 << 2)
#define DDC_NO_ACK (0x1 << 1)
#define DDCM_ABORT (0x1 << 0)

/***********************************************/
#define HPD_DDC_STATUS 0xC60

#define DDC_DATA_OUT (0xFF << 16)
#define DDC_DATA_OUT_SHIFT (16)

#define DDC_FIFO_FULL (0x1 << 15)
#define DDC_FIFO_EMPTY (0x1 << 14)
#define DDC_I2C_IN_PROG (0x1 << 13)
#define DDC_DATA_OUT_CNT (0x1F << 8)
#define RX_HDCP2_CAP_EN (0x1 << 7)
#define HDCP2_AUTH_RXCAP_FAIL (0x1 << 6)
#define PORD_PIN_STA (0x1 << 5)
#define HPD_PIN_STA (0x1 << 4)
#define PORD_STATE (0x3 << 2)
#define PORD_STATE_SHIFT (2)
#define PORD_STATE_CONNECTED (2)
#define PORD_STATE_DISCONNECTED (0)
#define HPD_STATE (0x3 << 0)
#define HPD_STATE_SHIFT (0)
#define HPD_STATE_CONNECTED (2)
#define HPD_STATE_DISCONNECTED (0)

/***********************************************/
#define SCDC_STATUS 0xC64

#define SCDC_STATE (0xF << 28)
#define SCDC_RREQ_STATE (0xF << 24)
#define SCDC_UP_FLAG1_STATUS (0xFF << 16)
#define SCDC_UP_FLAG0_STATUS (0xFF << 8)
#define SCDC_UP_FLAG_DONE (0x1 << 6)
#define SCDC_SLAVE_RD_REQ_P (0x1 << 5)
#define SCDC_DDC_CONFLICT (0x1 << 4)
#define SCDC_DDC_DONE (0x1 << 3)
#define SCDC_IN_PROG (0x1 << 2)
#define SCDC_RREQ_IN_PROG (0x1 << 1)
#define SCDC_ACTIVE (0x1 << 0)

/***********************************************/
#define HDCP2X_DDCM_STATUS 0xC68

#define RI_ON (0x1 << 31)
#define HDCP_I_CNT (0x7F << 24)
#define HDCP_RI_RDY (0x1 << 23)
#define KSV_FIFO_FIRST (0x1 << 22)
#define KSV_FIFO_LAST (0x1 << 21)
#define KSV_FIFO_BYTES (0x1F << 16)
#define HDCP2X_DDCM_CTL_CS (0xF << 12)
#define DDC_I2C_BUS_LOW	(0x1 << 11)
#define DDC_I2C_NO_ACK (0x1 << 10)
#define DDC_FIFO_WRITE_IN_USE (0x1 << 9)
#define DDC_FIFO_READ_IN_USE (0x1 << 8)
#define HDCP1_DDC_TPI_GRANT (0x1 << 6)
#define HDCP2X_DDCM_AUTH_POLL_ERR (0x1 << 5)
#define HDCP2X_DDCM_RCV_FAIL (0x1 << 4)
#define HDCP2X_DDCM_SND_FAIL (0x1 << 3)
#define HDCP2X_DDCM_AUTH_ERR (0x1 << 2)
#define HDCP2_DDC_TPI_GRANT (0x1 << 1)
#define HDCP2X_DIS_POLL_GNT (0x1 << 0)

/***********************************************/
#define TPI_STATUS_0 0xC6C

#define TPI_AUTH_STATE (0x3 << 30)
#define TPI_COPP_LINK_STATUS (0x3 << 28)
#define TPI_COPP_GPROT (0x1 << 27)
#define TPI_COPP_LPROT (0x1 << 26)
#define TPI_COPP_HDCP_REP (0x1 << 25)
#define TPI_COPP_PROTYPE (0x1 << 24)
#define TPI_RI_PRIME1 (0xFF << 16)
#define TPI_RI_PRIME0 (0xFF << 8)
#define DS_BKSV (0xFF << 0)

/***********************************************/
#define TPI_STATUS_1 0xC70

#define TPI_READ_V_PRIME_ERR (0x1 << 26)
#define TPI_READ_RI_PRIME_ERR (0x1 << 25)
#define TPI_READ_RI_2ND_ERR (0x1 << 24)
#define TPI_READ_R0_PRIME_ERR (0x1 << 23)
#define TPI_READ_KSV_FIFO_RDY_ERR (0x1 << 22)
#define TPI_READ_BSTATUS_ERR (0x1 << 21)
#define TPI_READ_KSV_LIST_ERR (0x1 << 20)
#define TPI_WRITE_AKSV_ERR (0x1 << 19)
#define TPI_WRITE_AN_ERR (0x1 << 18)
#define TPI_READ_RX_REPEATER_ERR (0x1 << 17)
#define TPI_READ_BKSV_ERR (0x1 << 16)
#define TPI_READ_V_PRIME_DONE (0x1 << 10)
#define TPI_READ_RI_PRIME_DONE (0x1 << 9)
#define TPI_READ_RI_2ND_DONE (0x1 << 8)
#define TPI_READ_R0_PRIME_DONE (0x1 << 7)
#define TPI_READ_KSV_FIFO_RDY_DONE (0x1 << 6)
#define TPI_READ_BSTATUS_DONE (0x1 << 5)
#define TPI_READ_KSV_LIST_DONE (0x1 << 4)
#define TPI_WRITE_AKSV_DONE (0x1 << 3)
#define TPI_WRITE_AN_DONE (0x1 << 2)
#define TPI_READ_RX_REPEATER_DONE (0x1 << 1)
#define TPI_READ_BKSV_DONE (0x1 << 0)

/***********************************************/
#define TPI_STATUS_FSM 0xC74

#define DDC_HDCP_ACC_NMB (0x3FF << 20)
#define TPI_DDCM_CTL_CS (0xF << 16)
#define TPI_DS_AUTH_CS (0xF << 12)
#define TPI_LINK_ENC_CS (0x7 << 9)
#define TPI_RX_AUTH_CS (0x1F << 4)
#define TPI_HW_CS (0xF << 0)

/***********************************************/
#define KSV_RI_STATUS 0xC78

#define RI_RX (0xFFFF << 16)
#define RI_TX (0xFFFF << 0)

/***********************************************/
#define HDCP2X_DEBUG_STATUS_0 0xC7C

#define HDCP2X_DEBUG_STAT3 (0xFF << 24)
#define HDCP2X_DEBUG_STAT2 (0xFF << 16)
#define HDCP2X_DEBUG_STAT1 (0xFF << 8)
#define HDCP2X_DEBUG_STAT0 (0xFF << 0)

/***********************************************/
#define HDCP2X_DEBUG_STATUS_1 0xC80

#define HDCP2X_DEBUG_STAT7 (0xFF << 24)
#define HDCP2X_DEBUG_STAT6 (0xFF << 16)
#define HDCP2X_DEBUG_STAT5 (0xFF << 8)
#define HDCP2X_DEBUG_STAT4 (0xFF << 0)

/***********************************************/
#define HDCP2X_DEBUG_STATUS_2 0xC84

#define HDCP2X_DEBUG_STAT11 (0xFF << 24)
#define HDCP2X_DEBUG_STAT10 (0xFF << 16)
#define HDCP2X_DEBUG_STAT9 (0xFF << 8)
#define HDCP2X_DEBUG_STAT8 (0xFF << 0)

/***********************************************/
#define HDCP2X_DEBUG_STATUS_3 0xC88

#define HDCP2X_DEBUG_STAT15 (0xFF << 24)
#define HDCP2X_DEBUG_STAT14 (0xFF << 16)
#define HDCP2X_DEBUG_STAT13 (0xFF << 8)
#define HDCP2X_DEBUG_STAT12 (0xFF << 0)

/***********************************************/
#define HDCP2X_STATUS_0 0xC8C

#define HDCP2X_AUTH_STAT (0xFF << 24)
#define HDCP2X_STATE (0xFF << 16)
#define HDCP2X_RPT_REPEATER (0x1 << 8)
#define HDCP2X_PRG_SEL (0x1 << 7)
#define HDCP2X_CUPD_DONE (0x1 << 6)
#define HDCP2X_RPT_MX_DEVS_EXC (0x1 << 5)
#define HDCP2X_RPT_MAX_CASC_EXC (0x1 << 4)
#define HDCP2X_RPT_HDCP20RPT_DSTRM (0x1 << 3)
#define HDCP2X_RPT_HDCP1DEV_DSTRM (0x1 << 2)
#define HDCP2X_RPT_RCVID_CHANGED (0x1 << 1)
#define HDCP2X_RPT_SMNG_XFER_DONE (0x1 << 0)

/***********************************************/
#define HDCP2X_STATUS_1 0xC90

#define HDCP2X_RINGOSS_BIST_FAIL (0x1 << 25)
#define HDCP2X_RINGOSC_BIST_DONE (0x1 << 24)
#define HDCP2X_RPT_RCVID_OUT (0xFF << 16)
#define HDCP2X_RPT_DEVCNT (0xFF << 8)
#define HDCP2X_RPT_DEPTH (0xFF << 0)

#define HDCP2X_RPT_DEVCNT_SHIFT (8)
#define HDCP2X_RPT_RCVID_OUT_SHIFT (16)

/***********************************************/
#define HDCP2X_RCVR_ID 0xC94

#define HDCP2X_RCVR_ID_L (0xFFFFFFFF << 0)

/***********************************************/
#define HDCP2X_RPT_SEQ 0xC98

#define HDCP2X_RCVR_ID_H (0xFF << 24)
#define HDCP2X_RPT_SEQ_NUM_V (0xFFFFFF << 0)

/***********************************************/
#define HDCP2X_GP_OUT 0xC9C

#define HDCP2X_GP_OUT3 (0xFF << 24)
#define HDCP2X_GP_OUT2 (0xFF << 16)
#define HDCP2X_GP_OUT1 (0xFF << 8)
#define HDCP2X_GP_OUT0 (0xFF << 0)

/***********************************************/
#define PROM_CTRL 0xCA0

#define PROM_ADDR (0xFFFF << 16)
#define PROM_ADDR_SHIFT (16)

#define PROM_WDATA (0xFF << 8)
#define PROM_WDATA_SHIFT (8)

#define PROM_CS (0x1 << 1)
#define PROM_WR (0x1 << 0)

/***********************************************/
#define PRAM_CTRL 0xCA4

#define PRAM_ADDR (0xFFFF << 16)
#define PRAM_ADDR_SHIFT (16)

#define PRAM_WDATA (0xFF << 8)
#define PRAM_WDATA_SHIFT (8)

#define PRAM_CTRL_SEL (0x1 << 2)
#define PRAM_CS (0x1 << 1)
#define PRAM_WR (0x1 << 0)

/***********************************************/
#define PROM_DATA 0xCA8

#define PROM_RDATA (0xFF << 8)
#define PRAM_RDATA (0xFF << 0)

/***********************************************/
#define SI2C_CTRL 0xCAC

#define SI2C_ADDR (0xFFFF << 16)
#define SI2C_ADDR_SHIFT (16)
#define SI2C_ADDR_READ (0xF4)

#define SI2C_WDATA (0xFF << 8)
#define SI2C_WDATA_SHIFT (8)

#define RX_CAP_RD_TRIG (0x1 << 5)
#define KSV_RD (0x1 << 4)
#define SI2C_STOP (0x1 << 3)
#define SI2C_CONFIRM_READ (0x1 << 2)
#define SI2C_RD (0x1 << 1)
#define SI2C_WR (0x1 << 0)

/***********************************************/
#define HDMI_HDCP_VERSION_INDEX 4
#define HDMI_CLKD2_DRVIMP_INDEX 37
#define HDMI_D0D1_DRVIMP_INDEX 30
#define HDMI_DDC_MODE  0
#define HDCP_DDC_MODE  1

extern unsigned char _bflagvideomute;
extern unsigned char _bflagaudiomute;
extern unsigned char _bsvpvideomute;
extern unsigned char _bsvpaudiomute;

extern unsigned char _bHdcpOff;
extern struct HDMI_SINK_AV_CAP_T _HdmiSinkAvCap;
extern unsigned char hdcp2_version_flag;
extern unsigned char hdmi2_force_output;

extern HDMI_AV_INFO_T _stAvdAVInfo;

extern unsigned int hdmi_drv_read(unsigned short u2Reg);
extern void hdmi_drv_write(unsigned short u2Reg, unsigned int u4Data);

extern unsigned int hdmi_sys_read(unsigned short u2Reg);
extern void hdmi_sys_write(unsigned short u2Reg, unsigned int u4Data);

extern unsigned int hdmi_pll_read(unsigned short u2Reg);
extern void hdmi_pll_write(unsigned short u2Reg, unsigned int u4Data);

extern unsigned int hdmi_pad_read(unsigned short u2Reg);
extern void hdmi_pad_write(unsigned short u2Reg, unsigned int u4Data);

extern unsigned int hdmi_hdmitopck_read(unsigned short u2Reg);
extern void hdmi_hdmitopck_write(unsigned short u2Reg, unsigned int u4Data);

extern void hdmi_infrasys_write(unsigned short u2Reg, unsigned int u4Data);
extern unsigned int hdmi_infrasys_read(unsigned short u2Reg);
extern void hdmi_perisys_write(unsigned short u2Reg, unsigned int u4Data);
extern unsigned int hdmi_perisys_read(unsigned short u2Reg);

extern void internal_hdmi_read(unsigned long u4Reg, unsigned int *p4Data);

extern void internal_hdmi_write(unsigned long u4Reg, unsigned int u4data);


#define vWriteByteHdmiGRL(dAddr, dVal)  (hdmi_drv_write(dAddr, dVal))
#define bReadByteHdmiGRL(bAddr)         (hdmi_drv_read(bAddr))
#define vWriteHdmiGRLMsk(dAddr, dVal, dMsk) \
(vWriteByteHdmiGRL((dAddr), (bReadByteHdmiGRL(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))

#define vWriteHdmiSYS(dAddr, dVal)  (hdmi_sys_write(dAddr, dVal))
#define dReadHdmiSYS(dAddr)         (hdmi_sys_read(dAddr))
#define vWriteHdmiSYSMsk(dAddr, dVal, dMsk) \
	(vWriteHdmiSYS((dAddr), (dReadHdmiSYS(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))

#define vWriteHdmiTOPCK(dAddr, dVal)  (hdmi_hdmitopck_write(dAddr, dVal))
#define dReadHdmiTOPCK(dAddr)         (hdmi_hdmitopck_read(dAddr))
#define vWriteHdmiTOPCKMsk(dAddr, dVal, dMsk) \
	(vWriteHdmiTOPCK((dAddr), (dReadHdmiTOPCK(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))
#define vWriteHdmiTOPCKUnMsk(dAddr, dMsk)(vWriteHdmiTOPCK((dAddr), (dReadHdmiTOPCK(dAddr) & (~(dMsk)))))

#define vWriteIoPll(dAddr, dVal)  (hdmi_pll_write(dAddr, dVal))
#define dReadIoPll(dAddr)         (hdmi_pll_read(dAddr))
#define vWriteIoPllMsk(dAddr, dVal, dMsk) vWriteIoPll((dAddr), (dReadIoPll(dAddr) & (~(dMsk))) | ((dVal) & (dMsk)))

#define vWriteIoPad(dAddr, dVal)  (hdmi_pad_write(dAddr, dVal))
#define dReadIoPad(dAddr)         (hdmi_pad_read(dAddr))
#define vWriteIoPadMsk(dAddr, dVal, dMsk) vWriteIoPad((dAddr), (dReadIoPad(dAddr) & (~(dMsk))) | ((dVal) & (dMsk)))

#define vWriteINFRASYS(dAddr, dVal)  (hdmi_infrasys_write(dAddr, dVal))
#define dReadINFRASYS(dAddr)         (hdmi_infrasys_read(dAddr))
#define vWriteINFRASYSMsk(dAddr, dVal, dMsk) \
	(vWriteINFRASYS((dAddr), (dReadINFRASYS(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))
#define vWriteINFRASYSUnMsk(dAddr, dMsk)(vWriteINFRASYS((dAddr), (dReadINFRASYS(dAddr) & (~(dMsk)))))

#define vWriteHdmiCEC(dAddr, dVal)  (hdmi_cec_write(dAddr, dVal))
#define dReadHdmiCEC(dAddr)         (hdmi_cec_read(dAddr))
#define vWriteHdmiCECMsk(dAddr, dVal, dMsk) \
	(vWriteHdmiCEC((dAddr), (dReadHdmiCEC(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))
#define vWriteHdmiCECUnMsk(dAddr, dMsk)(vWriteHdmiCEC((dAddr), (dReadHdmiCEC(dAddr) & (~(dMsk)))))

#define vWritePeriSYS(dAddr, dVal)  (hdmi_perisys_write(dAddr, dVal))
#define dReadPeriSYS(dAddr)         (hdmi_perisys_read(dAddr))
#define vWritePeriSYSMsk(dAddr, dVal, dMsk) \
	(vWriteINFRASYS((dAddr), (dReadINFRASYS(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))


extern void vSetCTL0BeZero(unsigned char fgBeZero);
extern void vWriteHdmiIntMask(unsigned char bMask);
extern void vHDMIAVUnMute(void);
extern void vHDMIAVMute(void);
extern void vTmdsOnOffAndResetHdcp(unsigned char fgHdmiTmdsEnable);
extern void vChangeVpll(unsigned char bRes, unsigned char bdeepmode);
extern void vChgHDMIVideoResolution(unsigned char ui1resindex, unsigned char ui1colorspace,
				    unsigned char ui1hdmifs, unsigned char bdeepmode);
extern void vChgHDMIAudioOutput(unsigned char ui1hdmifs, unsigned char ui1resindex,
				unsigned char bdeepmode);
extern void vChgtoSoftNCTS(unsigned char ui1resindex, unsigned char ui1audiosoft,
			   unsigned char ui1hdmifs, unsigned char bdeepmode);
extern void vSendAVIInfoFrame(unsigned char ui1resindex, unsigned char ui1colorspace);
extern void hdmi_hdmistatus(void);
extern void vTxSignalOnOff(unsigned char bOn);
extern unsigned char bCheckPordHotPlug(unsigned char bMode);
extern void vHotPlugPinInit(struct platform_device *pdv);
extern void vBlackHDMIOnly(void);
extern void vUnBlackHDMIOnly(void);
extern void UnMuteHDMIAudio(void);
extern void MuteHDMIAudio(void);
extern unsigned char hdmi_get_port_hpd_value(void);

extern void vTmds2OnOffAndResetHdcp(unsigned char fgHdmiTmdsEnable);
extern void vHDMI2AVMute(void);
extern void vBlackHDMI2Only(void);
extern void vUnBlackHDMI2Only(void);
extern void MuteHDMI2Audio(void);
extern void vChangeV2pll(unsigned char bRes, unsigned char bdeepmode);
extern void vChgHDMI2VideoResolution(unsigned char ui1resindex, unsigned char ui1colorspace,
				     unsigned char ui1hdmifs, unsigned char bdeepmode);
extern void vChgHDMI2AudioOutput(unsigned char ui1hdmifs, unsigned char ui1resindex,
				 unsigned char bdeepmode);
extern void UnMuteHDMI2Audio(void);
extern void vHDMI2AVUnMute(void);
extern void vChgtoSoftNCTS2(unsigned char ui1resindex, unsigned char ui1audiosoft,
			    unsigned char ui1hdmifs, unsigned char bdeepmode);
extern unsigned char bResolution_4K2K(unsigned char bResIndex);
extern unsigned int hdmi_check_status(void);
extern unsigned char bCheckPordHotPlug2(unsigned char bMode);
extern unsigned int IS_HDMI2_HTPLG(void);
extern unsigned int IS_HDMI2_PORD(void);
int vGet_DDC_Mode(struct platform_device *pdev);
int vSet_DDC_Mode(unsigned int HDCP_DDC_Mode);

#endif
#endif
