/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MSM_VFE40_H__
#define __MSM_VFE40_H__

#include <linux/bitops.h>
#include "msm_vfe_stats_buf.h"

#define TRUE  1
#define FALSE 0

#define VFE40_HW_NUMBER 0x10000015

/* This defines total number registers in VFE.
 * Each register is 4 bytes so to get the range,
 * multiply this number with 4. */
#define VFE40_REGISTER_TOTAL 0x00000320

/* at stop of vfe pipeline, for now it is assumed
 * that camif will stop at any time. Bit 1:0 = 0x10:
 * disable image data capture immediately. */
#define CAMIF_COMMAND_STOP_IMMEDIATELY  0x00000002

/* at stop of vfe pipeline, for now it is assumed
 * that camif will stop at any time. Bit 1:0 = 0x00:
 * disable image data capture at frame boundary */
#define CAMIF_COMMAND_STOP_AT_FRAME_BOUNDARY  0x00000000

/* to halt axi bridge */
#define AXI_HALT  0x00000001

/* clear the halt bit. */
#define AXI_HALT_CLEAR  0x00000000

/* reset the pipeline when stop command is issued.
 * (without reset the register.) bit 26-32 = 0,
 * domain reset, bit 0-9 = 1 for module reset, except
 * register module. */
#define VFE_RESET_UPON_STOP_CMD  0x000003ef

/* reset the pipeline when reset command.
 * bit 26-32 = 0, domain reset, bit 0-9 = 1 for module reset. */
#define VFE_RESET_UPON_RESET_CMD  0x000001ff

/* constants for irq registers */
#define VFE_DISABLE_ALL_IRQS 0
/* bit =1 is to clear the corresponding bit in VFE_IRQ_STATUS.  */
#define VFE_CLEAR_ALL_IRQ0   0xffff7fff
#define VFE_CLEAR_ALL_IRQ1   0xffffffff

#define VFE_IRQ_STATUS0_CAMIF_SOF_MASK            (0x00000001<<0)
#define VFE_IRQ_STATUS0_REG_UPDATE_MASK           (0x00000001<<4)
#define VFE_IRQ_STATUS0_STATS_BE                  (0x00000001<<16)
#define VFE_IRQ_STATUS0_STATS_BG                  (0x00000001<<17)
#define VFE_IRQ_STATUS0_STATS_BF                  (0x00000001<<18)
#define VFE_IRQ_STATUS0_STATS_AWB                 (0x00000001<<19)
#define VFE_IRQ_STATUS0_STATS_RS                  (0x00000001<<20)
#define VFE_IRQ_STATUS0_STATS_CS                  (0x00000001<<21)
#define VFE_IRQ_STATUS0_STATS_IHIST               (0x00000001<<22)
#define VFE_IRQ_STATUS0_STATS_SKIN_BHIST          (0x00000001<<23)
#define VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE0_MASK (0x00000001<<25)
#define VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE1_MASK (0x00000001<<26)
#define VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE2_MASK (0x00000001<<27)
#define VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE3_MASK (0x00000001<<28)
#define VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK_0     (0x00000001<<29)
#define VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK_1     (0x00000001<<30)
#define VFE_IRQ_STATUS0_RESET_AXI_HALT_ACK_MASK   (0x00000001<<31)

#define VFE_IRQ_STATUS1_SYNC_TIMER0               (0x00000001<<25)
#define VFE_IRQ_STATUS1_SYNC_TIMER1               (0x00000001<<26)
#define VFE_IRQ_STATUS1_SYNC_TIMER2               (0x00000001<<27)
#define VFE_IRQ_STATUS1_ASYNC_TIMER0              (0x00000001<<28)
#define VFE_IRQ_STATUS1_ASYNC_TIMER1              (0x00000001<<29)
#define VFE_IRQ_STATUS1_ASYNC_TIMER2              (0x00000001<<30)
#define VFE_IRQ_STATUS1_ASYNC_TIMER3              (0x00000001<<31)

/* imask for while waiting for stop ack,  driver has already
 * requested stop, waiting for reset irq, and async timer irq.
 * For irq_status_0, bit 28-32 are for async timer. For
 * irq_status_1, bit 22 for reset irq, bit 23 for axi_halt_ack
   irq */
#define VFE_IMASK_WHILE_STOPPING_0  0x80000000
#define VFE_IMASK_WHILE_STOPPING_1  0x00000100

/* For ABF bit 4 is set to zero and other's 1 */
#define ABF_MASK 0xFFFFFFF7

/* For DBPC bit 0 is set to zero and other's 1 */
#define DBPC_MASK 0xFFFFFFFE

/* For DBPC bit 1 is set to zero and other's 1 */
#define DBCC_MASK 0xFFFFFFFD

/* For DBPC/ABF/DBCC/ABCC bits are set to 1 all others 0 */
#define DEMOSAIC_MASK 0xF

/* For MCE enable bit 28 set to zero and other's 1 */
#define MCE_EN_MASK 0xEFFFFFFF

/* For MCE Q_K bit 28 to 32 set to zero and other's 1 */
#define MCE_Q_K_MASK 0x0FFFFFFF

#define BE_ENABLE_MASK    (0x00000001<<5)
#define BG_ENABLE_MASK    (0x00000001<<6)
#define BF_ENABLE_MASK    (0x00000001<<7)
#define AWB_ENABLE_MASK   (0x00000001<<8)
#define RS_ENABLE_MASK    (0x00000001<<9)
#define CS_ENABLE_MASK    (0x00000001<<10)
#define CLF_ENABLE_MASK   (0x00000001<<12)
#define IHIST_ENABLE_MASK (0x00000001<<15)
#define RS_CS_ENABLE_MASK (RS_ENABLE_MASK|CS_ENABLE_MASK)
#define STATS_ENABLE_MASK 0x000487E0   /* bit 18,15,10,9,8,7,6,5*/

#define VFE_DMI_CFG_DEFAULT              0x00000100

#define HFR_MODE_OFF 1
#define VFE_FRAME_SKIP_PERIOD_MASK 0x0000001F /*bits 0 -4*/

enum VFE40_DMI_RAM_SEL {
	NO_MEM_SELECTED          = 0,
	BLACK_LUT_RAM_BANK0      = 0x1,
	BLACK_LUT_RAM_BANK1      = 0x2,
	ROLLOFF_RAM0_BANK0       = 0x3,
	ROLLOFF_RAM0_BANK1       = 0x4,
	DEMOSAIC_LUT_RAM_BANK0   = 0x5,
	DEMOSAIC_LUT_RAM_BANK1   = 0x6,
	STATS_BHIST_RAM0         = 0x7,
	STATS_BHIST_RAM1         = 0x8,
	RGBLUT_RAM_CH0_BANK0     = 0x9,
	RGBLUT_RAM_CH0_BANK1     = 0xa,
	RGBLUT_RAM_CH1_BANK0     = 0xb,
	RGBLUT_RAM_CH1_BANK1     = 0xc,
	RGBLUT_RAM_CH2_BANK0     = 0xd,
	RGBLUT_RAM_CH2_BANK1     = 0xe,
	RGBLUT_CHX_BANK0         = 0xf,
	RGBLUT_CHX_BANK1         = 0x10,
	STATS_IHIST_RAM          = 0x11,
	LUMA_ADAPT_LUT_RAM_BANK0 = 0x12,
	LUMA_ADAPT_LUT_RAM_BANK1 = 0x13,
};

enum vfe_output_state {
	VFE_STATE_IDLE,
	VFE_STATE_START_REQUESTED,
	VFE_STATE_STARTED,
	VFE_STATE_STOP_REQUESTED,
	VFE_STATE_STOPPED,
};

#define V40_CAMIF_OFF             0x000002F8
#define V40_CAMIF_LEN             36

#define V40_DEMUX_OFF             0x00000424
#define V40_DEMUX_LEN             28

#define V40_DEMOSAICV3_0_OFF      0x00000440
#define V40_DEMOSAICV3_0_LEN      4
#define V40_DEMOSAICV3_1_OFF      0x00000518
#define V40_DEMOSAICV3_1_LEN      88
#define V40_DEMOSAICV3_2_OFF      0x00000568
#define V40_DEMOSAICV3_UP_REG_CNT 5

#define V40_OUT_CLAMP_OFF         0x00000874
#define V40_OUT_CLAMP_LEN         16

#define V40_OPERATION_CFG_LEN     44

#define V40_AXI_OUT_OFF           0x0000004C
#define V40_AXI_OUT_LEN           412
#define V40_AXI_CH_INF_LEN        32
#define V40_AXI_CFG_LEN           71

#define V40_FOV_ENC_OFF           0x00000854
#define V40_FOV_ENC_LEN           16
#define V40_FOV_VIEW_OFF          0x00000864
#define V40_FOV_VIEW_LEN          16

#define V40_SCALER_ENC_OFF 0x0000075C
#define V40_SCALER_ENC_LEN 72

#define V40_SCALER_VIEW_OFF 0x000007A4
#define V40_SCALER_VIEW_LEN 72

#define V40_COLORXFORM_ENC_CFG_OFF 0x0000071C
#define V40_COLORXFORM_ENC_CFG_LEN 32

#define V40_COLORXFORM_VIEW_CFG_OFF 0x0000073C
#define V40_COLORXFORM_VIEW_CFG_LEN 32

#define V40_CHROMA_EN_OFF 0x00000640
#define V40_CHROMA_EN_LEN 36

#define V40_SYNC_TIMER_OFF      0x00000324
#define V40_SYNC_TIMER_POLARITY_OFF 0x0000034C
#define V40_TIMER_SELECT_OFF        0x00000374
#define V40_SYNC_TIMER_LEN 28

#define V40_ASYNC_TIMER_OFF 0x00000350
#define V40_ASYNC_TIMER_LEN 28

/* use 10x13 mesh table in vfe40*/
#define V40_MESH_ROLL_OFF_CFG_OFF             0x00000400
#define V40_MESH_ROLL_OFF_CFG_LEN             36
#define V40_MESH_ROLL_OFF_TABLE_SIZE          130


#define V40_COLOR_COR_OFF 0x000005D0
#define V40_COLOR_COR_LEN 52

#define V40_WB_OFF 0x00000580
#define V40_WB_LEN 4

#define V40_RGB_G_OFF 0x00000638
#define V40_RGB_G_LEN 4
#define V40_GAMMA_LUT_BANK_SEL_MASK           0x00000007

#define V40_LA_OFF 0x0000063C
#define V40_LA_LEN 4

#define V40_SCE_OFF 0x00000694
#define V40_SCE_LEN 136

#define V40_CHROMA_SUP_OFF 0x00000664
#define V40_CHROMA_SUP_LEN 12

#define V40_MCE_OFF 0x00000670
#define V40_MCE_LEN 36

#define V40_STATS_BE_OFF 0x0000088C
#define V40_STATS_BE_LEN 12

#define V40_STATS_BG_OFF 0x00000898
#define V40_STATS_BG_LEN 12

#define V40_STATS_BF_OFF 0x000008A4
#define V40_STATS_BF_LEN 24

#define V40_STATS_BHIST_OFF 0x000008BC
#define V40_STATS_BHIST_LEN 8

#define V40_STATS_AWB_OFF 0x000008C4
#define V40_STATS_AWB_LEN 32

#define V40_STATS_RS_OFF 0x000008E4
#define V40_STATS_RS_LEN 8

#define V40_STATS_CS_OFF 0x000008EC
#define V40_STATS_CS_LEN 8

#define V40_STATS_IHIST_OFF 0x000008F4
#define V40_STATS_IHIST_LEN 8

#define V40_STATS_SKIN_OFF 0x000008FC
#define V40_STATS_SKIN_LEN 20

#define V40_ASF_OFF 0x000007EC
#define V40_ASF_LEN 48
#define V40_ASF_UPDATE_LEN 36

#define V40_CAPTURE_LEN 4

#define V40_GET_HW_VERSION_OFF 0
#define V40_GET_HW_VERSION_LEN 4

#define V40_LINEARIZATION_OFF1 0x0000037C
#define V40_LINEARIZATION_LEN1 68

#define V40_DEMOSAICV3_DBPC_CFG_OFF  0x00000444
#define V40_DEMOSAICV3_DBPC_LEN 4

#define V40_DEMOSAICV3_DBPC_CFG_OFF0 0x00000448
#define V40_DEMOSAICV3_DBPC_CFG_OFF1 0x0000044C
#define V40_DEMOSAICV3_DBPC_CFG_OFF2 0x00000450

#define V40_DEMOSAICV3_DBCC_OFF 0x00000454
#define V40_DEMOSAICV3_DBCC_LEN 16

#define V40_DEMOSAICV3_ABF_OFF 0x00000464
#define V40_DEMOSAICV3_ABF_LEN 180

#define V40_MODULE_CFG_OFF 0x00000018
#define V40_MODULE_CFG_LEN 4

#define V40_ASF_SPECIAL_EFX_CFG_OFF 0x0000081C
#define V40_ASF_SPECIAL_EFX_CFG_LEN 4

#define V40_CLF_CFG_OFF 0x00000588
#define V40_CLF_CFG_LEN 72

#define V40_CLF_LUMA_UPDATE_OFF 0x0000058C
#define V40_CLF_LUMA_UPDATE_LEN 60

#define V40_CLF_CHROMA_UPDATE_OFF 0x000005C8
#define V40_CLF_CHROMA_UPDATE_LEN 8

#define VFE40_GAMMA_NUM_ENTRIES  64

#define VFE40_LA_TABLE_LENGTH    64

#define VFE40_LINEARIZATON_TABLE_LENGTH    36

#define VFE_WM_CFG_BASE 0x0070
#define VFE_WM_CFG_LEN 0x0024

#define vfe40_get_ch_ping_addr(base, chn) \
	(msm_camera_io_r((base) + VFE_WM_CFG_BASE + VFE_WM_CFG_LEN * (chn)))
#define vfe40_get_ch_pong_addr(base, chn) \
	(msm_camera_io_r((base) + VFE_WM_CFG_BASE + VFE_WM_CFG_LEN * (chn) + 4))
#define vfe40_get_ch_addr(ping_pong, base, chn) \
	((((ping_pong) & (1 << (chn))) == 0) ? \
	(vfe40_get_ch_pong_addr((base), chn)) : \
	(vfe40_get_ch_ping_addr((base), chn)))

#define vfe40_put_ch_ping_addr(base, chn, addr) \
	(msm_camera_io_w((addr), \
	(base) + VFE_WM_CFG_BASE + VFE_WM_CFG_LEN * (chn)))
#define vfe40_put_ch_pong_addr(base, chn, addr) \
	(msm_camera_io_w((addr), \
	(base) + VFE_WM_CFG_BASE + VFE_WM_CFG_LEN * (chn) + 4))
#define vfe40_put_ch_addr(ping_pong, base, chn, addr) \
	(((ping_pong) & (1 << (chn))) == 0 ?   \
	vfe40_put_ch_pong_addr((base), (chn), (addr)) : \
	vfe40_put_ch_ping_addr((base), (chn), (addr)))

struct vfe_cmd_hw_version {
	uint32_t minorVersion;
	uint32_t majorVersion;
	uint32_t coreVersion;
};

enum VFE_AXI_OUTPUT_MODE {
	VFE_AXI_OUTPUT_MODE_Output1,
	VFE_AXI_OUTPUT_MODE_Output2,
	VFE_AXI_OUTPUT_MODE_Output1AndOutput2,
	VFE_AXI_OUTPUT_MODE_CAMIFToAXIViaOutput2,
	VFE_AXI_OUTPUT_MODE_Output2AndCAMIFToAXIViaOutput1,
	VFE_AXI_OUTPUT_MODE_Output1AndCAMIFToAXIViaOutput2,
	VFE_AXI_LAST_OUTPUT_MODE_ENUM
};

enum VFE_RAW_WR_PATH_SEL {
	VFE_RAW_OUTPUT_DISABLED,
	VFE_RAW_OUTPUT_ENC_CBCR_PATH,
	VFE_RAW_OUTPUT_VIEW_CBCR_PATH,
	VFE_RAW_OUTPUT_PATH_INVALID
};


#define VFE_AXI_OUTPUT_BURST_LENGTH     4
#define VFE_MAX_NUM_FRAGMENTS_PER_FRAME 4
#define VFE_AXI_OUTPUT_CFG_FRAME_COUNT  3

struct vfe_cmds_per_write_master {
	uint16_t imageWidth;
	uint16_t imageHeight;
	uint16_t outRowCount;
	uint16_t outRowIncrement;
	uint32_t outFragments[VFE_AXI_OUTPUT_CFG_FRAME_COUNT]
		[VFE_MAX_NUM_FRAGMENTS_PER_FRAME];
};

struct vfe_cmds_axi_per_output_path {
	uint8_t fragmentCount;
	struct vfe_cmds_per_write_master firstWM;
	struct vfe_cmds_per_write_master secondWM;
};

enum VFE_AXI_BURST_LENGTH {
	VFE_AXI_BURST_LENGTH_IS_2  = 2,
	VFE_AXI_BURST_LENGTH_IS_4  = 4,
	VFE_AXI_BURST_LENGTH_IS_8  = 8,
	VFE_AXI_BURST_LENGTH_IS_16 = 16
};


struct vfe_cmd_fov_crop_config {
	uint8_t enable;
	uint16_t firstPixel;
	uint16_t lastPixel;
	uint16_t firstLine;
	uint16_t lastLine;
};

struct vfe_cmds_main_scaler_stripe_init {
	uint16_t MNCounterInit;
	uint16_t phaseInit;
};

struct vfe_cmds_scaler_one_dimension {
	uint8_t  enable;
	uint16_t inputSize;
	uint16_t outputSize;
	uint32_t phaseMultiplicationFactor;
	uint8_t  interpolationResolution;
};

struct vfe_cmd_main_scaler_config {
	uint8_t enable;
	struct vfe_cmds_scaler_one_dimension    hconfig;
	struct vfe_cmds_scaler_one_dimension    vconfig;
	struct vfe_cmds_main_scaler_stripe_init MNInitH;
	struct vfe_cmds_main_scaler_stripe_init MNInitV;
};

struct vfe_cmd_scaler2_config {
	uint8_t enable;
	struct vfe_cmds_scaler_one_dimension hconfig;
	struct vfe_cmds_scaler_one_dimension vconfig;
};


struct vfe_cmd_frame_skip_update {
	uint32_t output1Pattern;
	uint32_t output2Pattern;
};

struct vfe_cmd_output_clamp_config {
	uint8_t minCh0;
	uint8_t minCh1;
	uint8_t minCh2;
	uint8_t maxCh0;
	uint8_t maxCh1;
	uint8_t maxCh2;
};

struct vfe_cmd_chroma_subsample_config {
	uint8_t enable;
	uint8_t cropEnable;
	uint8_t vsubSampleEnable;
	uint8_t hsubSampleEnable;
	uint8_t vCosited;
	uint8_t hCosited;
	uint8_t vCositedPhase;
	uint8_t hCositedPhase;
	uint16_t cropWidthFirstPixel;
	uint16_t cropWidthLastPixel;
	uint16_t cropHeightFirstLine;
	uint16_t cropHeightLastLine;
};

enum VFE_START_PIXEL_PATTERN {
	VFE_BAYER_RGRGRG,
	VFE_BAYER_GRGRGR,
	VFE_BAYER_BGBGBG,
	VFE_BAYER_GBGBGB,
	VFE_YUV_YCbYCr,
	VFE_YUV_YCrYCb,
	VFE_YUV_CbYCrY,
	VFE_YUV_CrYCbY
};

enum VFE_BUS_RD_INPUT_PIXEL_PATTERN {
	VFE_BAYER_RAW,
	VFE_YUV_INTERLEAVED,
	VFE_YUV_PSEUDO_PLANAR_Y,
	VFE_YUV_PSEUDO_PLANAR_CBCR
};

enum VFE_YUV_INPUT_COSITING_MODE {
	VFE_YUV_COSITED,
	VFE_YUV_INTERPOLATED
};

struct vfe_cmds_demosaic_abf {
	uint8_t   enable;
	uint8_t   forceOn;
	uint8_t   shift;
	uint16_t  lpThreshold;
	uint16_t  max;
	uint16_t  min;
	uint8_t   ratio;
};

struct vfe_cmds_demosaic_bpc {
	uint8_t   enable;
	uint16_t  fmaxThreshold;
	uint16_t  fminThreshold;
	uint16_t  redDiffThreshold;
	uint16_t  blueDiffThreshold;
	uint16_t  greenDiffThreshold;
};

struct vfe_cmd_demosaic_config {
	uint8_t   enable;
	uint8_t   slopeShift;
	struct vfe_cmds_demosaic_abf abfConfig;
	struct vfe_cmds_demosaic_bpc bpcConfig;
};

struct vfe_cmd_demosaic_bpc_update {
	struct vfe_cmds_demosaic_bpc bpcUpdate;
};

struct vfe_cmd_demosaic_abf_update {
	struct vfe_cmds_demosaic_abf abfUpdate;
};

struct vfe_cmd_white_balance_config {
	uint8_t  enable;
	uint16_t ch2Gain;
	uint16_t ch1Gain;
	uint16_t ch0Gain;
};

enum VFE_COLOR_CORRECTION_COEF_QFACTOR {
	COEF_IS_Q7_SIGNED,
	COEF_IS_Q8_SIGNED,
	COEF_IS_Q9_SIGNED,
	COEF_IS_Q10_SIGNED
};

struct vfe_cmd_color_correction_config {
	uint8_t     enable;
	enum VFE_COLOR_CORRECTION_COEF_QFACTOR coefQFactor;
	int16_t  C0;
	int16_t  C1;
	int16_t  C2;
	int16_t  C3;
	int16_t  C4;
	int16_t  C5;
	int16_t  C6;
	int16_t  C7;
	int16_t  C8;
	int16_t  K0;
	int16_t  K1;
	int16_t  K2;
};

#define VFE_LA_TABLE_LENGTH 64

struct vfe_cmd_la_config {
	uint8_t enable;
	int16_t table[VFE_LA_TABLE_LENGTH];
};

#define VFE_GAMMA_TABLE_LENGTH 256
enum VFE_RGB_GAMMA_TABLE_SELECT {
	RGB_GAMMA_CH0_SELECTED,
	RGB_GAMMA_CH1_SELECTED,
	RGB_GAMMA_CH2_SELECTED,
	RGB_GAMMA_CH0_CH1_SELECTED,
	RGB_GAMMA_CH0_CH2_SELECTED,
	RGB_GAMMA_CH1_CH2_SELECTED,
	RGB_GAMMA_CH0_CH1_CH2_SELECTED
};

struct vfe_cmd_rgb_gamma_config {
	uint8_t enable;
	enum VFE_RGB_GAMMA_TABLE_SELECT channelSelect;
	int16_t table[VFE_GAMMA_TABLE_LENGTH];
};

struct vfe_cmd_chroma_enhan_config {
	uint8_t  enable;
	int16_t am;
	int16_t ap;
	int16_t bm;
	int16_t bp;
	int16_t cm;
	int16_t cp;
	int16_t dm;
	int16_t dp;
	int16_t kcr;
	int16_t kcb;
	int16_t RGBtoYConversionV0;
	int16_t RGBtoYConversionV1;
	int16_t RGBtoYConversionV2;
	uint8_t RGBtoYConversionOffset;
};

struct vfe_cmd_chroma_suppression_config {
	uint8_t enable;
	uint8_t m1;
	uint8_t m3;
	uint8_t n1;
	uint8_t n3;
	uint8_t nn1;
	uint8_t mm1;
};

struct vfe_cmd_asf_config {
	uint8_t enable;
	uint8_t smoothFilterEnabled;
	uint8_t sharpMode;
	uint8_t smoothCoefCenter;
	uint8_t smoothCoefSurr;
	uint8_t normalizeFactor;
	uint8_t sharpK1;
	uint8_t sharpK2;
	uint8_t sharpThreshE1;
	int8_t sharpThreshE2;
	int8_t sharpThreshE3;
	int8_t sharpThreshE4;
	int8_t sharpThreshE5;
	int8_t filter1Coefficients[9];
	int8_t filter2Coefficients[9];
	uint8_t  cropEnable;
	uint16_t cropFirstPixel;
	uint16_t cropLastPixel;
	uint16_t cropFirstLine;
	uint16_t cropLastLine;
};

struct vfe_cmd_asf_update {
	uint8_t enable;
	uint8_t smoothFilterEnabled;
	uint8_t sharpMode;
	uint8_t smoothCoefCenter;
	uint8_t smoothCoefSurr;
	uint8_t normalizeFactor;
	uint8_t sharpK1;
	uint8_t sharpK2;
	uint8_t sharpThreshE1;
	int8_t  sharpThreshE2;
	int8_t  sharpThreshE3;
	int8_t  sharpThreshE4;
	int8_t  sharpThreshE5;
	int8_t  filter1Coefficients[9];
	int8_t  filter2Coefficients[9];
	uint8_t cropEnable;
};

enum VFE_TEST_GEN_SYNC_EDGE {
	VFE_TEST_GEN_SYNC_EDGE_ActiveHigh,
	VFE_TEST_GEN_SYNC_EDGE_ActiveLow
};


struct vfe_cmd_bus_pm_start {
	uint8_t output2YWrPmEnable;
	uint8_t output2CbcrWrPmEnable;
	uint8_t output1YWrPmEnable;
	uint8_t output1CbcrWrPmEnable;
};

struct  vfe_frame_skip_counts {
	uint32_t  totalFrameCount;
	uint32_t  output1Count;
	uint32_t  output2Count;
};

enum VFE_AXI_RD_UNPACK_HBI_SEL {
	VFE_AXI_RD_HBI_32_CLOCK_CYCLES,
	VFE_AXI_RD_HBI_64_CLOCK_CYCLES,
	VFE_AXI_RD_HBI_128_CLOCK_CYCLES,
	VFE_AXI_RD_HBI_256_CLOCK_CYCLES,
	VFE_AXI_RD_HBI_512_CLOCK_CYCLES,
	VFE_AXI_RD_HBI_1024_CLOCK_CYCLES,
	VFE_AXI_RD_HBI_2048_CLOCK_CYCLES,
	VFE_AXI_RD_HBI_4096_CLOCK_CYCLES
};

struct vfe_frame_bpc_info {
	uint32_t greenDefectPixelCount;
	uint32_t redBlueDefectPixelCount;
};

struct vfe_frame_asf_info {
	uint32_t  asfMaxEdge;
	uint32_t  asfHbiCount;
};

struct vfe_msg_camif_status {
	uint8_t  camifState;
	uint32_t pixelCount;
	uint32_t lineCount;
};

struct vfe40_irq_status {
	uint32_t vfeIrqStatus0;
	uint32_t vfeIrqStatus1;
	uint32_t camifStatus;
	uint32_t demosaicStatus;
	uint32_t asfMaxEdge;
};

#define V40_PREVIEW_AXI_FLAG  0x00000001
#define V40_SNAPSHOT_AXI_FLAG (0x00000001<<1)

struct vfe40_cmd_type {
	uint16_t id;
	uint32_t length;
	uint32_t offset;
	uint32_t flag;
};

struct vfe40_free_buf {
	struct list_head node;
	uint32_t paddr;
	uint32_t y_off;
	uint32_t cbcr_off;
};

struct vfe40_output_ch {
	struct list_head free_buf_queue;
	spinlock_t free_buf_lock;
	uint16_t image_mode;
	int8_t ch0;
	int8_t ch1;
	int8_t ch2;
	uint32_t  capture_cnt;
	uint32_t  frame_drop_cnt;
	struct msm_free_buf ping;
	struct msm_free_buf pong;
	struct msm_free_buf free_buf;
};

/* no error irq in mask 0 */
#define VFE40_IMASK_ERROR_ONLY_0  0x0
/* when normal case, don't want to block error status. */
/* bit 0-21 are error irq bits */
#define VFE40_IMASK_ERROR_ONLY_1               0x005FFFFF
#define VFE40_IMASK_CAMIF_ERROR               (0x00000001<<0)
#define VFE40_IMASK_BHIST_OVWR                (0x00000001<<1)
#define VFE40_IMASK_STATS_CS_OVWR             (0x00000001<<2)
#define VFE40_IMASK_STATS_IHIST_OVWR          (0x00000001<<3)
#define VFE40_IMASK_REALIGN_BUF_Y_OVFL        (0x00000001<<4)
#define VFE40_IMASK_REALIGN_BUF_CB_OVFL       (0x00000001<<5)
#define VFE40_IMASK_REALIGN_BUF_CR_OVFL       (0x00000001<<6)
#define VFE40_IMASK_VIOLATION                 (0x00000001<<7)
#define VFE40_IMASK_IMG_MAST_0_BUS_OVFL       (0x00000001<<8)
#define VFE40_IMASK_IMG_MAST_1_BUS_OVFL       (0x00000001<<9)
#define VFE40_IMASK_IMG_MAST_2_BUS_OVFL       (0x00000001<<10)
#define VFE40_IMASK_IMG_MAST_3_BUS_OVFL       (0x00000001<<11)
#define VFE40_IMASK_IMG_MAST_4_BUS_OVFL       (0x00000001<<12)
#define VFE40_IMASK_IMG_MAST_5_BUS_OVFL       (0x00000001<<13)
#define VFE40_IMASK_IMG_MAST_6_BUS_OVFL       (0x00000001<<14)
#define VFE40_IMASK_STATS_AE_BG_BUS_OVFL      (0x00000001<<15)
#define VFE40_IMASK_STATS_AF_BF_BUS_OVFL      (0x00000001<<16)
#define VFE40_IMASK_STATS_AWB_BUS_OVFL        (0x00000001<<17)
#define VFE40_IMASK_STATS_RS_BUS_OVFL         (0x00000001<<18)
#define VFE40_IMASK_STATS_CS_BUS_OVFL         (0x00000001<<19)
#define VFE40_IMASK_STATS_IHIST_BUS_OVFL      (0x00000001<<20)
#define VFE40_IMASK_STATS_SKIN_BHIST_BUS_OVFL (0x00000001<<21)
#define VFE40_IMASK_AXI_ERROR                 (0x00000001<<22)

#define VFE_COM_STATUS 0x000FE000

struct vfe40_output_path {
	uint16_t output_mode;     /* bitmask  */

	struct vfe40_output_ch out0; /* preview and thumbnail */
	struct vfe40_output_ch out1; /* snapshot */
	struct vfe40_output_ch out2; /* video    */
};

struct vfe40_frame_extra {
	uint32_t greenDefectPixelCount;
	uint32_t redBlueDefectPixelCount;

	uint32_t  asfMaxEdge;
	uint32_t  asfHbiCount;

	uint32_t yWrPmStats0;
	uint32_t yWrPmStats1;
	uint32_t cbcrWrPmStats0;
	uint32_t cbcrWrPmStats1;

	uint32_t  frameCounter;
};

#define VFE_CLEAR_ALL_IRQS              0xffffffff

#define VFE_HW_VERSION			        0x00000000
#define VFE_GLOBAL_RESET                0x0000000C
#define VFE_MODULE_RESET                0x00000010
#define VFE_CGC_OVERRIDE                0x00000014
#define VFE_MODULE_CFG                  0x00000018
#define VFE_CFG				            0x0000001C
#define VFE_IRQ_CMD                     0x00000024
#define VFE_IRQ_MASK_0                  0x00000028
#define VFE_IRQ_MASK_1                  0x0000002C
#define VFE_IRQ_CLEAR_0                 0x00000030
#define VFE_IRQ_CLEAR_1                 0x00000034
#define VFE_IRQ_STATUS_0                0x00000038
#define VFE_IRQ_STATUS_1                0x0000003C
#define VFE_IRQ_COMP_MASK               0x00000040
#define VFE_BUS_CMD                     0x0000004C
#define VFE_BUS_PING_PONG_STATUS        0x00000180
#define VFE_AXI_CMD                     0x000001D8
#define VFE_AXI_STATUS        0x000002C0
#define VFE_BUS_STATS_PING_PONG_BASE    0x000000F4

#define VFE_BUS_STATS_AEC_WR_PING_ADDR    0x000000F4
#define VFE_BUS_STATS_AEC_WR_PONG_ADDR    0x000000F8
#define VFE_BUS_STATS_AEC_UB_CFG          0x000000FC
#define VFE_BUS_STATS_AF_WR_PING_ADDR     0x00000100
#define VFE_BUS_STATS_AF_WR_PONG_ADDR     0x00000104
#define VFE_BUS_STATS_AF_UB_CFG           0x00000108
#define VFE_BUS_STATS_AWB_WR_PING_ADDR    0x0000010C
#define VFE_BUS_STATS_AWB_WR_PONG_ADDR    0x00000110
#define VFE_BUS_STATS_AWB_UB_CFG          0x00000114
#define VFE_BUS_STATS_RS_WR_PING_ADDR    0x00000118
#define VFE_BUS_STATS_RS_WR_PONG_ADDR    0x0000011C
#define VFE_BUS_STATS_RS_UB_CFG          0x00000120
#define VFE_BUS_STATS_CS_WR_PING_ADDR    0x00000124
#define VFE_BUS_STATS_CS_WR_PONG_ADDR    0x00000128
#define VFE_BUS_STATS_CS_UB_CFG          0x0000012C
#define VFE_BUS_STATS_HIST_WR_PING_ADDR   0x00000130
#define VFE_BUS_STATS_HIST_WR_PONG_ADDR   0x00000134
#define VFE_BUS_STATS_HIST_UB_CFG          0x00000138
#define VFE_BUS_STATS_SKIN_WR_PING_ADDR    0x0000013C
#define VFE_BUS_STATS_SKIN_WR_PONG_ADDR    0x00000140
#define VFE_BUS_STATS_SKIN_UB_CFG          0x00000144

#define VFE_0_BUS_BDG_QOS_CFG_0     0x000002C4
#define VFE_0_BUS_BDG_QOS_CFG_1     0x000002C8
#define VFE_0_BUS_BDG_QOS_CFG_2     0x000002CC
#define VFE_0_BUS_BDG_QOS_CFG_3     0x000002D0
#define VFE_0_BUS_BDG_QOS_CFG_4     0x000002D4
#define VFE_0_BUS_BDG_QOS_CFG_5     0x000002D8
#define VFE_0_BUS_BDG_QOS_CFG_6     0x000002DC
#define VFE_0_BUS_BDG_QOS_CFG_7     0x000002E0

#define VFE_CAMIF_COMMAND               0x000002F4
#define VFE_CAMIF_STATUS                0x0000031C
#define VFE_REG_UPDATE_CMD              0x00000378
#define VFE_DEMUX_GAIN_0                0x00000428
#define VFE_DEMUX_GAIN_1                0x0000042C
#define VFE_CHROMA_UP                   0x0000057C

#define VFE_CLAMP_ENC_MAX               0x00000874
#define VFE_CLAMP_ENC_MIN               0x00000878
#define VFE_CLAMP_VIEW_MAX              0x0000087C
#define VFE_CLAMP_VIEW_MIN              0x00000880

#define VFE_REALIGN_BUF                 0x00000884
#define VFE_STATS_CFG                   0x00000888
#define VFE_STATS_AWB_SGW_CFG           0x000008CC
#define VFE_DMI_CFG                     0x00000910
#define VFE_DMI_ADDR                    0x00000914
#define VFE_DMI_DATA_LO                 0x0000091C
#define VFE_BUS_IO_FORMAT_CFG           0x00000054
#define VFE_RDI0_CFG                    0x000002E8
#define VFE_RDI1_CFG                    0x000002EC
#define VFE_RDI2_CFG                    0x000002F0

#define VFE_VIOLATION_STATUS            0x000007B4

#define VFE40_DMI_DATA_HI               0x00000918
#define VFE40_DMI_DATA_LO               0x0000091C

#define VFE40_OUTPUT_MODE_PT			BIT(0)
#define VFE40_OUTPUT_MODE_S			BIT(1)
#define VFE40_OUTPUT_MODE_V			BIT(2)
#define VFE40_OUTPUT_MODE_P			BIT(3)
#define VFE40_OUTPUT_MODE_T			BIT(4)
#define VFE40_OUTPUT_MODE_P_ALL_CHNLS		BIT(5)
#define VFE40_OUTPUT_MODE_PRIMARY		BIT(6)
#define VFE40_OUTPUT_MODE_PRIMARY_ALL_CHNLS	BIT(7)
#define VFE40_OUTPUT_MODE_SECONDARY		BIT(8)
#define VFE40_OUTPUT_MODE_SECONDARY_ALL_CHNLS	BIT(9)

struct vfe_stats_control {
	uint32_t droppedStatsFrameCount;
	uint32_t bufToRender;
};
struct axi_ctrl_t;
struct vfe40_ctrl_type;

struct vfe_share_ctrl_t {
	void __iomem *vfebase;
	uint32_t register_total;

	atomic_t vstate;
	uint32_t vfeFrameId;
	uint32_t stats_comp;
	spinlock_t  stop_flag_lock;
	int8_t stop_ack_pending;
	enum vfe_output_state liveshot_state;
	uint32_t vfe_capture_count;

	uint16_t operation_mode;     /* streaming or snapshot */
	struct vfe40_output_path outpath;

	uint32_t ref_count;
	spinlock_t  sd_notify_lock;
	uint32_t vfe_clk_rate;

	atomic_t irq_cnt;
	struct axi_ctrl_t *axi_ctrl;
	struct vfe40_ctrl_type *vfe40_ctrl;
};

struct axi_ctrl_t {
	struct v4l2_subdev subdev;
	struct platform_device *pdev;
	struct resource *vfeirq;
	spinlock_t  tasklet_lock;
	struct list_head tasklet_q;

	void *syncdata;

	struct resource	*vfemem;
	struct resource *vfeio;
	struct regulator *fs_vfe;
	struct clk *vfe_clk[3];
	struct tasklet_struct vfe40_tasklet;
	struct vfe_share_ctrl_t *share_ctrl;
};

struct vfe40_ctrl_type {
	uint32_t vfeImaskCompositePacked;

	spinlock_t  update_ack_lock;
	spinlock_t  state_lock;
	spinlock_t  io_lock;
	spinlock_t  stats_bufq_lock;
	uint32_t extlen;
	void *extdata;

	int8_t start_ack_pending;
	int8_t reset_ack_pending;
	int8_t update_ack_pending;
	enum vfe_output_state recording_state;
	int8_t update_linear;
	int8_t update_rolloff;
	int8_t update_la;
	int8_t update_gamma;

	struct vfe_share_ctrl_t *share_ctrl;

	uint32_t sync_timer_repeat_count;
	uint32_t sync_timer_state;
	uint32_t sync_timer_number;

	uint32_t output1Pattern;
	uint32_t output1Period;
	uint32_t output2Pattern;
	uint32_t output2Period;
	uint32_t vfeFrameSkipCount;
	uint32_t vfeFrameSkipPeriod;
	struct vfe_stats_control afStatsControl;
	struct vfe_stats_control awbStatsControl;
	struct vfe_stats_control aecStatsControl;
	struct vfe_stats_control ihistStatsControl;
	struct vfe_stats_control rsStatsControl;
	struct vfe_stats_control csStatsControl;

	/* v4l2 subdev */
	struct v4l2_subdev subdev;
	struct platform_device *pdev;
	uint32_t hfr_mode;
	uint32_t frame_skip_cnt;
	uint32_t frame_skip_pattern;
	uint32_t snapshot_frame_cnt;
	struct msm_stats_bufq_ctrl stats_ctrl;
	struct msm_stats_ops stats_ops;
};

#define statsAeNum      0
#define statsAfNum      1
#define statsAwbNum     2
#define statsRsNum      3
#define statsCsNum      4
#define statsIhistNum   5
#define statsSkinNum    6

struct vfe_cmd_stats_ack {
	uint32_t  nextStatsBuf;
};

#define VFE_STATS_BUFFER_COUNT            3

struct vfe_cmd_stats_buf {
	uint32_t statsBuf[VFE_STATS_BUFFER_COUNT];
};

void vfe40_subdev_notify(int id, int path, int image_mode,
	struct v4l2_subdev *sd, struct vfe_share_ctrl_t *share_ctrl);
struct vfe40_output_ch *vfe40_get_ch(
	int path, struct vfe_share_ctrl_t *share_ctrl);
void vfe40_send_isp_msg(struct v4l2_subdev *sd,
	uint32_t vfeFrameId, uint32_t isp_msg_id);
void vfe40_axi_probe(struct axi_ctrl_t *axi_ctrl);

static const uint32_t vfe40_AXI_WM_CFG[] = {
	0x0000006C,
	0x00000090,
	0x000000B4,
	0x000000D8,
	0x000000FC,
	0x00000120,
	0x00000144,
};

static struct vfe40_cmd_type vfe40_cmd[] = {
/*0*/
	{VFE_CMD_DUMMY_0},
	{VFE_CMD_SET_CLK},
	{VFE_CMD_RESET},
	{VFE_CMD_START},
	{VFE_CMD_TEST_GEN_START},
/*5*/
	{VFE_CMD_OPERATION_CFG, V40_OPERATION_CFG_LEN},
	{VFE_CMD_AXI_OUT_CFG, V40_AXI_OUT_LEN, V40_AXI_OUT_OFF, 0xFF},
	{VFE_CMD_CAMIF_CFG, V40_CAMIF_LEN, V40_CAMIF_OFF, 0xFF},
	{VFE_CMD_AXI_INPUT_CFG},
	{VFE_CMD_BLACK_LEVEL_CFG},
/*10*/
	{VFE_CMD_MESH_ROLL_OFF_CFG},
	{VFE_CMD_DEMUX_CFG, V40_DEMUX_LEN, V40_DEMUX_OFF, 0xFF},
	{VFE_CMD_FOV_CFG},
	{VFE_CMD_MAIN_SCALER_CFG},
	{VFE_CMD_WB_CFG, V40_WB_LEN, V40_WB_OFF, 0xFF},
/*15*/
	{VFE_CMD_COLOR_COR_CFG, V40_COLOR_COR_LEN, V40_COLOR_COR_OFF, 0xFF},
	{VFE_CMD_RGB_G_CFG, V40_RGB_G_LEN, V40_RGB_G_OFF, 0xFF},
	{VFE_CMD_LA_CFG, V40_LA_LEN, V40_LA_OFF, 0xFF },
	{VFE_CMD_CHROMA_EN_CFG, V40_CHROMA_EN_LEN, V40_CHROMA_EN_OFF, 0xFF},
	{VFE_CMD_CHROMA_SUP_CFG, V40_CHROMA_SUP_LEN, V40_CHROMA_SUP_OFF, 0xFF},
/*20*/
	{VFE_CMD_MCE_CFG, V40_MCE_LEN, V40_MCE_OFF, 0xFF},
	{VFE_CMD_SK_ENHAN_CFG, V40_SCE_LEN, V40_SCE_OFF, 0xFF},
	{VFE_CMD_ASF_CFG, V40_ASF_LEN, V40_ASF_OFF, 0xFF},
	{VFE_CMD_S2Y_CFG},
	{VFE_CMD_S2CbCr_CFG},
/*25*/
	{VFE_CMD_CHROMA_SUBS_CFG},
	{VFE_CMD_OUT_CLAMP_CFG, V40_OUT_CLAMP_LEN, V40_OUT_CLAMP_OFF, 0xFF},
	{VFE_CMD_FRAME_SKIP_CFG},
	{VFE_CMD_DUMMY_1},
	{VFE_CMD_DUMMY_2},
/*30*/
	{VFE_CMD_DUMMY_3},
	{VFE_CMD_UPDATE},
	{VFE_CMD_BL_LVL_UPDATE},
	{VFE_CMD_DEMUX_UPDATE, V40_DEMUX_LEN, V40_DEMUX_OFF, 0xFF},
	{VFE_CMD_FOV_UPDATE},
/*35*/
	{VFE_CMD_MAIN_SCALER_UPDATE},
	{VFE_CMD_WB_UPDATE, V40_WB_LEN, V40_WB_OFF, 0xFF},
	{VFE_CMD_COLOR_COR_UPDATE, V40_COLOR_COR_LEN, V40_COLOR_COR_OFF, 0xFF},
	{VFE_CMD_RGB_G_UPDATE, V40_RGB_G_LEN, V40_CHROMA_EN_OFF, 0xFF},
	{VFE_CMD_LA_UPDATE, V40_LA_LEN, V40_LA_OFF, 0xFF },
/*40*/
	{VFE_CMD_CHROMA_EN_UPDATE, V40_CHROMA_EN_LEN, V40_CHROMA_EN_OFF, 0xFF},
	{VFE_CMD_CHROMA_SUP_UPDATE, V40_CHROMA_SUP_LEN,
		V40_CHROMA_SUP_OFF, 0xFF},
	{VFE_CMD_MCE_UPDATE, V40_MCE_LEN, V40_MCE_OFF, 0xFF},
	{VFE_CMD_SK_ENHAN_UPDATE, V40_SCE_LEN, V40_SCE_OFF, 0xFF},
	{VFE_CMD_S2CbCr_UPDATE},
/*45*/
	{VFE_CMD_S2Y_UPDATE},
	{VFE_CMD_ASF_UPDATE, V40_ASF_UPDATE_LEN, V40_ASF_OFF, 0xFF},
	{VFE_CMD_FRAME_SKIP_UPDATE},
	{VFE_CMD_CAMIF_FRAME_UPDATE},
	{VFE_CMD_STATS_AF_UPDATE},
/*50*/
	{VFE_CMD_STATS_AE_UPDATE},
	{VFE_CMD_STATS_AWB_UPDATE, V40_STATS_AWB_LEN, V40_STATS_AWB_OFF},
	{VFE_CMD_STATS_RS_UPDATE, V40_STATS_RS_LEN, V40_STATS_RS_OFF},
	{VFE_CMD_STATS_CS_UPDATE, V40_STATS_CS_LEN, V40_STATS_CS_OFF},
	{VFE_CMD_STATS_SKIN_UPDATE},
/*55*/
	{VFE_CMD_STATS_IHIST_UPDATE, V40_STATS_IHIST_LEN, V40_STATS_IHIST_OFF},
	{VFE_CMD_DUMMY_4},
	{VFE_CMD_EPOCH1_ACK},
	{VFE_CMD_EPOCH2_ACK},
	{VFE_CMD_START_RECORDING},
/*60*/
	{VFE_CMD_STOP_RECORDING},
	{VFE_CMD_DUMMY_5},
	{VFE_CMD_DUMMY_6},
	{VFE_CMD_CAPTURE, V40_CAPTURE_LEN, 0xFF},
	{VFE_CMD_DUMMY_7},
/*65*/
	{VFE_CMD_STOP},
	{VFE_CMD_GET_HW_VERSION, V40_GET_HW_VERSION_LEN,
		V40_GET_HW_VERSION_OFF},
	{VFE_CMD_GET_FRAME_SKIP_COUNTS},
	{VFE_CMD_OUTPUT1_BUFFER_ENQ},
	{VFE_CMD_OUTPUT2_BUFFER_ENQ},
/*70*/
	{VFE_CMD_OUTPUT3_BUFFER_ENQ},
	{VFE_CMD_JPEG_OUT_BUF_ENQ},
	{VFE_CMD_RAW_OUT_BUF_ENQ},
	{VFE_CMD_RAW_IN_BUF_ENQ},
	{VFE_CMD_STATS_AF_ENQ},
/*75*/
	{VFE_CMD_STATS_AE_ENQ},
	{VFE_CMD_STATS_AWB_ENQ},
	{VFE_CMD_STATS_RS_ENQ},
	{VFE_CMD_STATS_CS_ENQ},
	{VFE_CMD_STATS_SKIN_ENQ},
/*80*/
	{VFE_CMD_STATS_IHIST_ENQ},
	{VFE_CMD_DUMMY_8},
	{VFE_CMD_JPEG_ENC_CFG},
	{VFE_CMD_DUMMY_9},
	{VFE_CMD_STATS_AF_START},
/*85*/
	{VFE_CMD_STATS_AF_STOP},
	{VFE_CMD_STATS_AE_START},
	{VFE_CMD_STATS_AE_STOP},
	{VFE_CMD_STATS_AWB_START, V40_STATS_AWB_LEN, V40_STATS_AWB_OFF},
	{VFE_CMD_STATS_AWB_STOP},
/*90*/
	{VFE_CMD_STATS_RS_START, V40_STATS_RS_LEN, V40_STATS_RS_OFF},
	{VFE_CMD_STATS_RS_STOP},
	{VFE_CMD_STATS_CS_START, V40_STATS_CS_LEN, V40_STATS_CS_OFF},
	{VFE_CMD_STATS_CS_STOP},
	{VFE_CMD_STATS_SKIN_START},
/*95*/
	{VFE_CMD_STATS_SKIN_STOP},
	{VFE_CMD_STATS_IHIST_START, V40_STATS_IHIST_LEN, V40_STATS_IHIST_OFF},
	{VFE_CMD_STATS_IHIST_STOP},
	{VFE_CMD_DUMMY_10},
	{VFE_CMD_SYNC_TIMER_SETTING, V40_SYNC_TIMER_LEN, V40_SYNC_TIMER_OFF},
/*100*/
	{VFE_CMD_ASYNC_TIMER_SETTING, V40_ASYNC_TIMER_LEN, V40_ASYNC_TIMER_OFF},
	{VFE_CMD_LIVESHOT},
	{VFE_CMD_LA_SETUP},
	{VFE_CMD_LINEARIZATION_CFG, V40_LINEARIZATION_LEN1,
		V40_LINEARIZATION_OFF1},
	{VFE_CMD_DEMOSAICV3},
/*105*/
	{VFE_CMD_DEMOSAICV3_ABCC_CFG},
	{VFE_CMD_DEMOSAICV3_DBCC_CFG, V40_DEMOSAICV3_DBCC_LEN,
		V40_DEMOSAICV3_DBCC_OFF},
	{VFE_CMD_DEMOSAICV3_DBPC_CFG},
	{VFE_CMD_DEMOSAICV3_ABF_CFG, V40_DEMOSAICV3_ABF_LEN,
		V40_DEMOSAICV3_ABF_OFF},
	{VFE_CMD_DEMOSAICV3_ABCC_UPDATE},
/*110*/
	{VFE_CMD_DEMOSAICV3_DBCC_UPDATE, V40_DEMOSAICV3_DBCC_LEN,
		V40_DEMOSAICV3_DBCC_OFF},
	{VFE_CMD_DEMOSAICV3_DBPC_UPDATE},
	{VFE_CMD_XBAR_CFG},
	{VFE_CMD_MODULE_CFG, V40_MODULE_CFG_LEN, V40_MODULE_CFG_OFF},
	{VFE_CMD_ZSL},
/*115*/
	{VFE_CMD_LINEARIZATION_UPDATE, V40_LINEARIZATION_LEN1,
		V40_LINEARIZATION_OFF1},
	{VFE_CMD_DEMOSAICV3_ABF_UPDATE, V40_DEMOSAICV3_ABF_LEN,
		V40_DEMOSAICV3_ABF_OFF},
	{VFE_CMD_CLF_CFG, V40_CLF_CFG_LEN, V40_CLF_CFG_OFF},
	{VFE_CMD_CLF_LUMA_UPDATE, V40_CLF_LUMA_UPDATE_LEN,
		V40_CLF_LUMA_UPDATE_OFF},
	{VFE_CMD_CLF_CHROMA_UPDATE, V40_CLF_CHROMA_UPDATE_LEN,
		V40_CLF_CHROMA_UPDATE_OFF},
/*120*/
	{VFE_CMD_PCA_ROLL_OFF_CFG},
	{VFE_CMD_PCA_ROLL_OFF_UPDATE},
	{VFE_CMD_GET_REG_DUMP},
	{VFE_CMD_GET_LINEARIZATON_TABLE},
	{VFE_CMD_GET_MESH_ROLLOFF_TABLE},
/*125*/
	{VFE_CMD_GET_PCA_ROLLOFF_TABLE},
	{VFE_CMD_GET_RGB_G_TABLE},
	{VFE_CMD_GET_LA_TABLE},
	{VFE_CMD_DEMOSAICV3_UPDATE},
	{VFE_CMD_ACTIVE_REGION_CFG},
/*130*/
	{VFE_CMD_COLOR_PROCESSING_CONFIG},
	{VFE_CMD_STATS_WB_AEC_CONFIG},
	{VFE_CMD_STATS_WB_AEC_UPDATE},
	{VFE_CMD_Y_GAMMA_CONFIG},
	{VFE_CMD_SCALE_OUTPUT1_CONFIG},
/*135*/
	{VFE_CMD_SCALE_OUTPUT2_CONFIG},
	{VFE_CMD_CAPTURE_RAW},
	{VFE_CMD_STOP_LIVESHOT},
	{VFE_CMD_RECONFIG_VFE},
	{VFE_CMD_STATS_REQBUF},
/*140*/
	{VFE_CMD_STATS_ENQUEUEBUF},
	{VFE_CMD_STATS_FLUSH_BUFQ},
	{VFE_CMD_FOV_ENC_CFG, V40_FOV_ENC_LEN, V40_FOV_ENC_OFF, 0xFF},
	{VFE_CMD_FOV_VIEW_CFG, V40_FOV_VIEW_LEN, V40_FOV_VIEW_OFF, 0xFF},
	{VFE_CMD_FOV_ENC_UPDATE, V40_FOV_ENC_LEN, V40_FOV_ENC_OFF, 0xFF},
/*145*/
	{VFE_CMD_FOV_VIEW_UPDATE, V40_FOV_VIEW_LEN, V40_FOV_VIEW_OFF, 0xFF},
	{VFE_CMD_SCALER_ENC_CFG, V40_SCALER_ENC_LEN, V40_SCALER_ENC_OFF, 0xFF},
	{VFE_CMD_SCALER_VIEW_CFG, V40_SCALER_VIEW_LEN,
		V40_SCALER_VIEW_OFF, 0xFF},
	{VFE_CMD_SCALER_ENC_UPDATE, V40_SCALER_ENC_LEN,
		V40_SCALER_ENC_OFF, 0xFF},
	{VFE_CMD_SCALER_VIEW_UPDATE, V40_SCALER_VIEW_LEN,
		V40_SCALER_VIEW_OFF, 0xFF},
/*150*/
	{VFE_CMD_COLORXFORM_ENC_CFG, V40_COLORXFORM_ENC_CFG_LEN,
		V40_COLORXFORM_ENC_CFG_OFF, 0xFF},
	{VFE_CMD_COLORXFORM_VIEW_CFG, V40_COLORXFORM_VIEW_CFG_LEN,
		V40_COLORXFORM_VIEW_CFG_OFF},
	{VFE_CMD_COLORXFORM_ENC_UPDATE, V40_COLORXFORM_ENC_CFG_LEN,
		V40_COLORXFORM_ENC_CFG_OFF, 0xFF},
	{VFE_CMD_COLORXFORM_VIEW_UPDATE, V40_COLORXFORM_VIEW_CFG_LEN,
		V40_COLORXFORM_VIEW_CFG_OFF, 0xFF},
};

#endif /* __MSM_VFE40_H__ */
