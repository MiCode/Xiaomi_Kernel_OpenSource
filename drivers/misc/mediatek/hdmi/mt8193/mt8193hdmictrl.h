
#ifndef __mt8193hdmictrl_h__
#define __mt8193hdmictrl_h__
#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/byteorder/generic.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/rtpm_prio.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <linux/reboot.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/completion.h>

#include "mt8193table.h"
#include "hdmi_drv.h"
/* ////////////////////////////////////// */


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


#define SV_ON        (u16)(1)
#define SV_OFF       (u16)(0)

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
#define GRL_CFG3             0x30
#define CFG3_AES_KEY_INDEX_MASK    0x3f
#define CFG3_CONTROL_PACKET_DELAY  (0x1 << 6)
#define CFG3_KSV_LOAD_START        (0x1 << 7)
#define GRL_CFG4             0x34
#define CFG4_AES_KEY_LOAD  (0x1 << 4)
#define CFG4_AV_UNMUTE_EN  (0x1 << 5)
#define CFG4_AV_UNMUTE_SET (0x1 << 6)
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

/* ////////////////////////////////////////////////// */
#define HDMI_SYS_CFG1C   0x1c
#define HDMI_ON            (0x01 << 0)
#define HDMI_RST           (0x01 << 1)
#define ANLG_ON           (0x01 << 2)
#define CFG10_DVI          (0x01 << 3)
#define HDMI_TST           (0x01 << 3)
#define SYS_KEYMASK1       (0xff<<8)
#define SYS_KEYMASK2       (0xff<<16)
#define AUD_OUTSYNC_EN     (((UINT32)1)<<24)
#define AUD_OUTSYNC_PRE_EN (((UINT32)1)<<25)
#define I2CM_ON            (((UINT32)1)<<26)
#define E2PROM_TYPE_8BIT   (((UINT32)1)<<27)
#define MCM_E2PROM_ON      (((UINT32)1)<<28)
#define EXT_E2PROM_ON      (((UINT32)1)<<29)
#define HTPLG_PIN_SEL_OFF  (((UINT32)1)<<30)
#define AES_EFUSE_ENABLE   (((UINT32)1)<<31)

#define HDMI_SYS_CFG20   0x20
#define DEEP_COLOR_MODE_MASK (3 << 1)
#define COLOR_8BIT_MODE	 (0 << 1)
#define COLOR_10BIT_MODE	 (1 << 1)
#define COLOR_12BIT_MODE	 (2 << 1)
#define COLOR_16BIT_MODE	 (3 << 1)
#define DEEP_COLOR_EN		 (1 << 0)

#define HDMI_SYS_CFG24   0x24
#define HDMI_SYS_CFG28   0x28
#define HDMI_SYS_FMETER   0x4c
#define TRI_CAL (1<<0)
#define CLK_EXC (1<<1)
#define CAL_OK (1<<2)
#define CALSEL (2<<3)
#define CAL_CNT (0xffff<<16)

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


/* ////*******************DGI************************////// */
#define dgi0_anaif_ctrl2 0x448
#define dgi1_del_d2_d4_sel             (1<<17)
#define dgi1_del_d1_sel                (1<<16)
#define dgi0_anaif_ctrl1 0x450
#define dgi1_pad_clk_inv_en            (1<<31)
#define dgi1_clk_delay_sel1            (0x3f<<24)
#define dgi1_clk_delay_sel0            (0x3f<<16)
#define dgi1_clk_delay_sel0            (0x3f<<16)
#define dgi1_clk_pad_sel_tv_mode       (1<<10)
#define data_in_tv_mode                (1<<9)
#define data_bit_inv                   (1<<8)
#define anaif_dig1_clk_sel             (1<<6)
#define clk_sel_tv_mode                (1<<4)
#define clk_mode_sel                   (1<<3)
#define nweb_clk_en                    (1<<2)
#define dgi1_pad_clk_en                (1<<1)
#define tv_mode_clk_en                 (1<<0)
#define ttl_anaif_ctrl  0x454

#define dec_ctl  0x600
#define reset_counter                  (1<<27)
#define field_dec_polar                (1<<10)
#define hsync_dec_polar                (1<<9)
#define vsync_dec_polar                (1<<8)
#define data_dec_delay_fall            (0x3<<6)
#define data_dec_delay_risc            (0x3<<4)
#define dgi1_on                        (1<<0)

#define fifo_ctrl 0x604
#define timing_fifo_in_dly_risc        (1<<31)
#define timing_fifo_in_dly_fall        (1<<30)
#define data_fifo_in_dly_rise          (1<<29)
#define data_fifo_in_dly_fall          (1<<28)
#define sw_rst                         (1<<19)
#define vsync_polar_in                 (1<<18)
#define fifo_reset_on                  (1<<17)
#define fifo_reset_sel                 (1<<16)
#define rd_start_sel                   (1<<7)
#define rd_start                       (0x7f<<0)

#define data_out_ctrl 0x608
#define tv_mode                        (1<<31)
#define tim_out_bypass                 (1<<30)
#define bit_inv_y                      (1<<28)
#define bit_inv_c                      (1<<27)
#define yc_swap                        (1<<26)
#define rise_use_fall                  (1<<21)
#define fall_use_fall                  (1<<20)
#define rg_timing_sel                  (1<<19)
#define vsync_out_polar                (1<<17)
#define hsync_out_polar                (1<<16)
#define field_out_polar                (1<<15)
#define de_out_polar                   (1<<14)
#define y_out_delay                    (0x3<<4)
#define c1_out_delay                   (0x3<<2)
#define c2_out_delay                   (0x3<<0)

#define ctrl_422_444 0x60c
#define rg_cbcr_preload                (0xf<<8)
#define single_8bit                    (1<<6)
#define yc_sel_preload                 (1<<5)
#define hsync_ycrst_polar              (1<<4)
#define rpt_422_444                    (1<<1)
#define bypass_422_444                 (1<<0)

#define tg_ctrl00 0x61c
#define tve_fld                        (1<<31)
#define prgs_autofld                   (1<<30)
#define prgs_invfld	                 (1<<29)
#define fmtmas	                     (1<<28)
#define hw_option	                     (0xf<<24)
#define syn_del	                     (0x3<<22)
#define c_fmtrst_m2	                 (1<<21)
#define prgs_out	                     (1<<20)
#define tvf_nd	                     (1<<19)
#define v_total_m2	                 (0xfff<<0)

#define tg_ctrl01 0x620
#define rg_vsync_forward	             (1<<31)
#define rg_vsync_delay	             (0x1fff<<16)
#define rg_hsync_delay	             (0x1fff<<0)

#define tg_ctrl02 0x624
#define vsync_total                    (0xfff<<16)
#define hsync_total                    (0x1fff<<0)

#define tg_ctrl03 0x628
#define vysnc_width                    (0x1ff<<16)
#define hsync_width                    (0xfff<<0)

#define tg_ctrl04 0x62c
#define h_act2_en                      (1<<10)
#define v_act2_en                      (1<<9)
#define hd_on                          (1<<8)
#define vsync_polar                    (1<<6)
#define hsync_polar                    (1<<5)
#define de_polar                       (1<<4)

#define tg_ctrl05 0x630
#define x_active_start                 (0x1fff<<16)
#define x_active_end                   (0x1fff<<0)

#define tg_ctrl06 0x634
#define y_active_ostart                (0xfff<<16)
#define y_active_oend                  (0xfff<<0)

#define tg_ctrl07 0x638
#define y_active_estart                (0xfff<<16)
#define y_active_eend                  (0xfff<<0)

#define tg_ctrl08 0x63c
#define x_active_start_1               (0x1fff<<16)
#define x_active_end_1                 (0x1fff<<0)

#define tg_ctrl09 0x640
#define y_active_ostart_1              (0xfff<<16)
#define y_active_oend_1                (0xfff<<0)

#define tg_ctrl10 0x644
#define y_active_estart_1              (0xfff<<16)
#define y_active_eend_1                (0xfff<<0)

#define dgi1_clk_rst_ctrl 0x65c
#define dgi1_test_mode	             (1<<31)
#define clk_out_to_in_inv	             (1<<5)
#define clk_out_to_in	                 (1<<4)
#define clk_pat_gen_en	             (1<<3)
#define dgi1_clk_out_enable	         (1<<2)
#define dgi1_clk_in_inv_enable	     (1<<1)
#define dgi1_clk_in_enable	         (1<<0)

#define  pat_gen_ctrl0  0x700
#define rg_ptgen_v_total               (0xfff<<16)
#define rg_ptgen_h_total               (0x1fff<<0)

#define  pat_gen_ctrl1  0x704
#define rg_ptgen_v_width               (0xfff<<16)
#define rg_ptgen_h_width               (0x1fff<<0)

#define  pat_gen_ctrl2  0x708
#define rg_ptgen_v_start               (0xfff<<16)
#define rg_ptgen_h_start               (0x1fff<<0)

#define  pat_gen_ctrl3  0x70c
#define rg_ptgen_v_active              (0xfff<<16)
#define rg_ptgen_h_active              (0x1fff<<0)

#define  pat_gen_ctrl4  0x710
#define rg_ptgen_color_bar_th	         (0xfff<<16)
#define disable_edge	                 (1<<15)
#define rg_ptgen_width                 (0x3<<12)
#define rg_ptgen_type	                 (0x3<<8)
#define black_color                  (0x0<<8)
#define pure_color                   (0x1<<8)
#define pat_gen_rst	                 (1<<2)
#define pat_in	                     (1<<1)
#define rg_tst_pat_en	                 (1<<0)

#define  pat_gen_ctrl5  0x714
#define rg_ptgen_bd_in_fall	         (0xff<<8)
#define rg_ptgen_bd_in_rise	         (0xff<<0)

#define  pat_gen_ctrl6  0x718
#define rg_ptgen_in_fall	             (0xff<<8)
#define rg_ptgen_in_rise	             (0xff<<0)

#define dgi1_crc_mon_ctrl 0x71c
#define dgi1_mon_en	                 (1<<20)
#define dgi1_mon_sel	                 (0x7<<16)
#define c_crc_clr	                     (1<<1)
#define c_crc_start	                 (1<<0)

#define dgi1_crc_out 0x720
#define crc_rdy                        (1<<28)
#define crc_out                        (0xffff<<0)

#define dgi1_mon 0x724
#define dgi1_mon_value                       (0xffffffff<<0)

#define dgi1_yuv2rgb_ctr  0x728
#define fifo_write_en (1<<31)
#define vsync_pu_sel  (1<<30)
#define inbuf_del_sel  (1<<29)
#define rg_full_range_out  (1<<4)
#define rg_uv_swap    (1<<3)
#define rg_full_renge_input  (1<<2)
#define rg_yuv709_rgb  (1<<1)
#define rg_yuv2rgb_en  (1<<0)
/* //////////////////////////////////////////////////////////// */
#define IO_PAD_PD 0x58
#define IO_PAD_HOT_PLUG_PD (1<<21)
#define IO_PAD_UP 0x64
#define IO_PAD_HOT_PLUG_UP (1<<21)



/* //////////////////////////////////////////////////////// */
typedef struct _HDMI_AV_INFO_T {
	HDMI_VIDEO_RESOLUTION e_resolution;
	u8 fgHdmiOutEnable;
	u8 u2VerFreq;
	u8 b_hotplug_state;
	HDMI_OUT_COLOR_SPACE_T e_video_color_space;
	HDMI_DEEP_COLOR_T e_deep_color_bit;
	u8 ui1_aud_out_ch_number;
	HDMI_AUDIO_SAMPLING_T e_hdmi_fs;
	u8 bhdmiRChstatus[6];
	u8 bhdmiLChstatus[6];
	u8 bMuteHdmiAudio;
	u8 u1HdmiI2sMclk;
	u8 u1hdcponoff;
	u8 u1audiosoft;
	u8 fgHdmiTmdsEnable;
} HDMI_AV_INFO_T;

/* ///////////////////////////////////////////////////////// */
#define HDMIDRV_BASE  (0x0)
#define HDMISYS_BASE  (0x1000)
#define HDMIDGI_BASE  (0x0)
#define HDMIPAD_BASE  (0x1200)

extern HDMI_AV_INFO_T _stAvdAVInfo;
extern u32 mt8193_hdmidrv_read(u16 u2Reg);
extern void mt8193_hdmidrv_write(u16 u2Reg, u32 u4Data);
extern u32 mt8193_hdmisys_read(u16 u2Reg);
extern void mt8193_hdmisys_write(u16 u2Reg, u32 u4Data);
extern u32 mt8193_hdmidgi_read(u16 u2Reg);
extern void mt8193_hdmidgi_write(u16 u2Reg, u32 u4Data);
extern u32 mt8193_pad_read(u16 u2Reg);
extern void mt8193_pad_write(u16 u2Reg, u32 u4Data);


#define vWriteByteHdmiGRL(dAddr, dVal)  (mt8193_hdmidrv_write(dAddr, dVal))
#define bReadByteHdmiGRL(bAddr)         (mt8193_hdmidrv_read(bAddr))
#define vWriteHdmiGRLMsk(dAddr, dVal, dMsk) (vWriteByteHdmiGRL((dAddr), (bReadByteHdmiGRL(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))

#define vWriteHdmiSYS(dAddr, dVal)  (mt8193_hdmisys_write(dAddr, dVal))
#define dReadHdmiSYS(dAddr)         (mt8193_hdmisys_read(dAddr))
#define vWriteHdmiSYSMsk(dAddr, dVal, dMsk) (vWriteHdmiSYS((dAddr), (dReadHdmiSYS(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))

#define vWriteHdmiDGI(dAddr, dVal)  (mt8193_hdmidgi_write(dAddr, dVal))
#define dReadHdmiDGI(dAddr)         (mt8193_hdmidgi_read(dAddr))
#define vWriteHdmiDGIMsk(dAddr, dVal, dMsk) (vWriteHdmiDGI((dAddr), (dReadHdmiDGI(dAddr) & (~(dMsk))) | ((dVal) & (dMsk))))

#define vWriteIoPad(dAddr, dVal)  (mt8193_pad_write(dAddr, dVal))
#define dReadIoPad(dAddr)         (mt8193_pad_read(dAddr))
#define vWriteIoPadMsk(dAddr, dVal, dMsk) vWriteIoPad((dAddr), (dReadIoPad(dAddr) & (~(dMsk))) | ((dVal) & (dMsk)))

extern void vSetCTL0BeZero(u8 fgBeZero);
extern void vWriteHdmiIntMask(u8 bMask);
extern void vHDMIAVUnMute(void);
extern void vHDMIAVMute(void);
extern void vTmdsOnOffAndResetHdcp(u8 fgHdmiTmdsEnable);
extern void vChangeVpll(u8 bRes, u8 bdeepmode);
extern void vChgHDMIVideoResolution(u8 ui1resindex, u8 ui1colorspace, u8 ui1hdmifs, u8 bdeepmode);
extern void vChgHDMIAudioOutput(u8 ui1hdmifs, u8 ui1resindex, u8 bdeepmode);
extern void vChgtoSoftNCTS(u8 ui1resindex, u8 ui1audiosoft, u8 ui1hdmifs, u8 bdeepmode);
extern void vSendAVIInfoFrame(u8 ui1resindex, u8 ui1colorspace);
extern void mt8193_hdmistatus(void);
extern void vTxSignalOnOff(u8 bOn);
extern u8 bCheckPordHotPlug(u8 bMode);
extern void vSetHDMITxPLLTrigger(void);
extern void vResetHDMIPLL(void);
extern void vHotPlugPinInit(void);
extern void vBlackHDMIOnly(void);
extern void vUnBlackHDMIOnly(void);
extern void UnMuteHDMIAudio(void);
extern void MuteHDMIAudio(void);
#endif
#endif
