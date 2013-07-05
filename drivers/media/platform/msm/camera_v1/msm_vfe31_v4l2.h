/* Copyright (c) 2012 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MSM_VFE31_V4L2_H__
#define __MSM_VFE31_V4L2_H__

#include <linux/bitops.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* This defines total number registers in VFE.
 * Each register is 4 bytes so to get the range,
 * multiply this number with 4. */
#define VFE31_REGISTER_TOTAL 0x0000017F

/* at start of camif,  bit 1:0 = 0x01:enable
 * image data capture at frame boundary. */
#define CAMIF_COMMAND_START  0x00000005

/* bit 2= 0x1:clear the CAMIF_STATUS register
 * value. */
#define CAMIF_COMMAND_CLEAR  0x00000004

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

/* clear axi_halt_irq */
#define MASK_AXI_HALT_IRQ	0xFF7FFFFF

/* reset the pipeline when stop command is issued.
 * (without reset the register.) bit 26-31 = 0,
 * domain reset, bit 0-9 = 1 for module reset, except
 * register module. */
#define VFE_RESET_UPON_STOP_CMD  0x000003ef

/* reset the pipeline when reset command.
 * bit 26-31 = 0, domain reset, bit 0-9 = 1 for module reset. */
#define VFE_RESET_UPON_RESET_CMD  0x000003ff

/* bit 5 is for axi status idle or busy.
 * 1 =  halted,  0 = busy */
#define AXI_STATUS_BUSY_MASK 0x00000020

/* bit 0 & bit 1 = 1, both y and cbcr irqs need to be present
 * for frame done interrupt */
#define VFE_COMP_IRQ_BOTH_Y_CBCR 3

/* bit 1 = 1, only cbcr irq triggers frame done interrupt */
#define VFE_COMP_IRQ_CBCR_ONLY 2

/* bit 0 = 1, only y irq triggers frame done interrupt */
#define VFE_COMP_IRQ_Y_ONLY 1

/* bit 0 = 1, PM go;   bit1 = 1, PM stop */
#define VFE_PERFORMANCE_MONITOR_GO   0x00000001
#define VFE_PERFORMANCE_MONITOR_STOP 0x00000002

/* bit 0 = 1, test gen go;   bit1 = 1, test gen stop */
#define VFE_TEST_GEN_GO   0x00000001
#define VFE_TEST_GEN_STOP 0x00000002

/* the chroma is assumed to be interpolated between
 * the luma samples.  JPEG 4:2:2 */
#define VFE_CHROMA_UPSAMPLE_INTERPOLATED 0

/* constants for irq registers */
#define VFE_DISABLE_ALL_IRQS 0
/* bit =1 is to clear the corresponding bit in VFE_IRQ_STATUS.  */
#define VFE_CLEAR_ALL_IRQS   0xffffffff

#define VFE_IRQ_STATUS0_CAMIF_SOF_MASK            0x00000001
#define VFE_IRQ_STATUS0_CAMIF_EOF_MASK            0x00000004
#define VFE_IRQ_STATUS0_REG_UPDATE_MASK           0x00000020
#define VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE0_MASK 0x00200000
#define VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE1_MASK 0x00400000
#define VFE_IRQ_STATUS0_IMAGE_COMPOSIT_DONE2_MASK 0x00800000
#define VFE_IRQ_STATUS1_RESET_AXI_HALT_ACK_MASK   0x00800000
#define VFE_IRQ_STATUS0_STATS_COMPOSIT_MASK       0x01000000

#define VFE_IRQ_STATUS0_STATS_AEC     0x2000  /* bit 13 */
#define VFE_IRQ_STATUS0_STATS_AF      0x4000  /* bit 14 */
#define VFE_IRQ_STATUS0_STATS_AWB     0x8000  /* bit 15 */
#define VFE_IRQ_STATUS0_STATS_RS      0x10000  /* bit 16 */
#define VFE_IRQ_STATUS0_STATS_CS      0x20000  /* bit 17 */
#define VFE_IRQ_STATUS0_STATS_IHIST   0x40000  /* bit 18 */

#define VFE_IRQ_STATUS0_SYNC_TIMER0   0x2000000  /* bit 25 */
#define VFE_IRQ_STATUS0_SYNC_TIMER1   0x4000000  /* bit 26 */
#define VFE_IRQ_STATUS0_SYNC_TIMER2   0x8000000  /* bit 27 */
#define VFE_IRQ_STATUS0_ASYNC_TIMER0  0x10000000  /* bit 28 */
#define VFE_IRQ_STATUS0_ASYNC_TIMER1  0x20000000  /* bit 29 */
#define VFE_IRQ_STATUS0_ASYNC_TIMER2  0x40000000  /* bit 30 */
#define VFE_IRQ_STATUS0_ASYNC_TIMER3  0x80000000  /* bit 31 */

/* imask for while waiting for stop ack,  driver has already
 * requested stop, waiting for reset irq, and async timer irq.
 * For irq_status_0, bit 28-31 are for async timer. For
 * irq_status_1, bit 22 for reset irq, bit 23 for axi_halt_ack
   irq */
#define VFE_IMASK_WHILE_STOPPING_0  0xF0000000
#define VFE_IMASK_WHILE_STOPPING_1  0x00C00000
#define VFE_IMASK_RESET             0x00400000
#define VFE_IMASK_AXI_HALT          0x00800000


/* no error irq in mask 0 */
#define VFE_IMASK_ERROR_ONLY_0  0x0
/* when normal case, don't want to block error status. */
/* bit 0-21 are error irq bits */
#define VFE_IMASK_ERROR_ONLY_1  0x003fffff

/* For BPC bit 0,bit 12-17 and bit 26 -20 are set to zero and other's 1 */
#define BPC_MASK 0xF80C0FFE

/* For BPC bit 1 and 2 are set to zero and other's 1 */
#define ABF_MASK 0xFFFFFFF9

/* For DBPC bit 0 is set to zero and other's 1 */
#define DBPC_MASK 0xFFFFFFFE

/* For DBCC bit 1 is set to zero and other's 1 */
#define DBCC_MASK 0xFFFFFFFD

/* For DBPC/ABF/DBCC/ABCC bits are set to 1 all others 0 */
#define DEMOSAIC_MASK 0x8FFFFFFF
/* For MCE enable bit 28 set to zero and other's 1 */
#define MCE_EN_MASK 0xEFFFFFFF

/* For MCE Q_K bit 28 to 31 set to zero and other's 1 */
#define MCE_Q_K_MASK 0x0FFFFFFF

#define AE_BG_ENABLE_MASK 0x00000020      /* bit 5 */
#define AF_BF_ENABLE_MASK 0x00000040      /* bit 6 */
#define AWB_ENABLE_MASK 0x00000080     /* bit 7 */

#define RS_ENABLE_MASK 0x00000100      /* bit 8  */
#define CS_ENABLE_MASK 0x00000200      /* bit 9  */
#define RS_CS_ENABLE_MASK 0x00000300   /* bit 8,9  */
#define IHIST_ENABLE_MASK 0x00008000   /* bit 15 */
#define STATS_ENABLE_MASK 0x000483E0   /* bit 18,15,9,8,7,6,5*/

#define VFE_REG_UPDATE_TRIGGER           1
#define VFE_PM_BUF_MAX_CNT_MASK          0xFF
#define VFE_DMI_CFG_DEFAULT              0x00000100
#define VFE_AE_PINGPONG_STATUS_BIT       0x80
#define VFE_AF_PINGPONG_STATUS_BIT       0x100
#define VFE_AWB_PINGPONG_STATUS_BIT      0x200

#define HFR_MODE_OFF 1
#define VFE_FRAME_SKIP_PERIOD_MASK 0x0000001F /*bits 0 -4*/

enum VFE31_DMI_RAM_SEL {
	 NO_MEM_SELECTED          = 0,
	 ROLLOFF_RAM              = 0x1,
	 RGBLUT_RAM_CH0_BANK0     = 0x2,
	 RGBLUT_RAM_CH0_BANK1     = 0x3,
	 RGBLUT_RAM_CH1_BANK0     = 0x4,
	 RGBLUT_RAM_CH1_BANK1     = 0x5,
	 RGBLUT_RAM_CH2_BANK0     = 0x6,
	 RGBLUT_RAM_CH2_BANK1     = 0x7,
	 STATS_HIST_RAM           = 0x8,
	 RGBLUT_CHX_BANK0         = 0x9,
	 RGBLUT_CHX_BANK1         = 0xa,
	 LUMA_ADAPT_LUT_RAM_BANK0 = 0xb,
	 LUMA_ADAPT_LUT_RAM_BANK1 = 0xc
};

enum vfe_output_state {
	VFE_STATE_IDLE,
	VFE_STATE_START_REQUESTED,
	VFE_STATE_STARTED,
	VFE_STATE_STOP_REQUESTED,
	VFE_STATE_STOPPED,
};

#define V31_CAMIF_OFF             0x000001E4
#define V31_CAMIF_LEN             32

#define V31_DEMUX_OFF             0x00000284
#define V31_DEMUX_LEN             20

#define V31_DEMOSAICV3_UP_REG_CNT 5

#define V31_OUT_CLAMP_OFF         0x00000524
#define V31_OUT_CLAMP_LEN         8

#define V31_OPERATION_CFG_LEN     32

#define V31_AXI_OUT_OFF           0x00000038
#define V31_AXI_OUT_LEN           212
#define V31_AXI_CH_INF_LEN        24
#define V31_AXI_CFG_LEN           47
#define V31_AXI_RESERVED            1

#define V31_FRAME_SKIP_OFF        0x00000504
#define V31_FRAME_SKIP_LEN        32

#define V31_CHROMA_SUBS_OFF       0x000004F8
#define V31_CHROMA_SUBS_LEN       12

#define V31_FOV_OFF           0x00000360
#define V31_FOV_LEN           8

#define V31_MAIN_SCALER_OFF 0x00000368
#define V31_MAIN_SCALER_LEN 28

#define V31_S2Y_OFF 0x000004D0
#define V31_S2Y_LEN 20

#define V31_S2CbCr_OFF 0x000004E4
#define V31_S2CbCr_LEN 20

#define V31_CHROMA_EN_OFF 0x000003C4
#define V31_CHROMA_EN_LEN 36

#define V31_SYNC_TIMER_OFF      0x0000020C
#define V31_SYNC_TIMER_POLARITY_OFF 0x00000234
#define V31_TIMER_SELECT_OFF        0x0000025C
#define V31_SYNC_TIMER_LEN 28

#define V31_ASYNC_TIMER_OFF 0x00000238
#define V31_ASYNC_TIMER_LEN 28

#define V31_BLACK_LEVEL_OFF 0x00000264
#define V31_BLACK_LEVEL_LEN 16

#define V31_MESH_ROLL_OFF_CFG_OFF             0x00000274
#define V31_MESH_ROLL_OFF_CFG_LEN             16
#define V31_MESH_ROLL_OFF_INIT_TABLE_SIZE     13
#define V31_MESH_ROLL_OFF_DELTA_TABLE_SIZE    208
#define V31_MESH_ROLL_OFF_DELTA_TABLE_OFFSET  32

#define V31_COLOR_COR_OFF 0x00000388
#define V31_COLOR_COR_LEN 52

#define V31_WB_OFF 0x00000384
#define V31_WB_LEN 4

#define V31_RGB_G_OFF 0x000003BC
#define V31_RGB_G_LEN 4

#define V31_LA_OFF 0x000003C0
#define V31_LA_LEN 4

#define V31_SCE_OFF 0x00000418
#define V31_SCE_LEN 136

#define V31_CHROMA_SUP_OFF 0x000003E8
#define V31_CHROMA_SUP_LEN 12

#define V31_MCE_OFF 0x000003F4
#define V31_MCE_LEN 36
#define V31_STATS_AF_OFF 0x0000053c
#define V31_STATS_AF_LEN 16

#define V31_STATS_AE_OFF 0x00000534
#define V31_STATS_AE_LEN 8

#define V31_STATS_AWB_OFF 0x0000054c
#define V31_STATS_AWB_LEN 32

#define V31_STATS_IHIST_OFF 0x0000057c
#define V31_STATS_IHIST_LEN 8

#define V31_STATS_RS_OFF 0x0000056c
#define V31_STATS_RS_LEN 8

#define V31_STATS_CS_OFF 0x00000574
#define V31_STATS_CS_LEN 8

#define V31_ASF_OFF 0x000004A0
#define V31_ASF_LEN 48
#define V31_ASF_UPDATE_LEN 36
#define V31_CAPTURE_LEN 4
#define V31_GET_HW_VERSION_OFF 0
#define V31_GET_HW_VERSION_LEN 4
#define V31_DEMOSAICV3_OFF 0x00000298
#define V31_DEMOSAICV3_LEN 4
/* BPC     */
#define V31_DEMOSAICV3_DBPC_CFG_OFF  0x0000029C
#define V31_DEMOSAICV3_DBPC_LEN 8
#define V31_XBAR_CFG_OFF 0x00000040
/* ABF     */
#define V31_DEMOSAICV3_ABF_OFF 0x000002A4
#define V31_DEMOSAICV3_ABF_LEN 180
#define V31_XBAR_CFG_LEN 8

#define V31_MODULE_CFG_OFF 0x00000010
#define V31_MODULE_CFG_LEN 4
#define V31_EZTUNE_CFG_OFF 0x00000010
#define V31_EZTUNE_CFG_LEN 4

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

#define VFE31_GAMMA_NUM_ENTRIES  64

#define VFE31_LA_TABLE_LENGTH    64

#define VFE31_HIST_TABLE_LENGTH  256

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

struct vfe31_irq_status {
	uint32_t vfeIrqStatus0;
	uint32_t vfeIrqStatus1;
	uint32_t camifStatus;
	uint32_t demosaicStatus;
	uint32_t asfMaxEdge;
};

#define V31_PREVIEW_AXI_FLAG  0x00000001
#define V31_SNAPSHOT_AXI_FLAG (0x00000001<<1)

struct vfe31_cmd_type {
	uint16_t id;
	uint32_t length;
	uint32_t offset;
	uint32_t flag;
};

struct vfe31_free_buf {
	struct list_head node;
	uint32_t paddr;
	uint32_t y_off;
	uint32_t cbcr_off;
};

struct vfe31_output_ch {
	struct list_head free_buf_queue;
	spinlock_t free_buf_lock;
	uint16_t output_fmt;
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
#define VFE31_IMASK_ERROR_ONLY_0  0x0
/* when normal case, don't want to block error status. */
/* bit 0-21 are error irq bits */
#define VFE31_IMASK_ERROR_ONLY_1               0x003FFFFF
#define VFE31_IMASK_CAMIF_ERROR               (0x00000001<<0)
#define VFE31_IMASK_STATS_CS_OVWR             (0x00000001<<1)
#define VFE31_IMASK_STATS_IHIST_OVWR          (0x00000001<<2)
#define VFE31_IMASK_REALIGN_BUF_Y_OVFL        (0x00000001<<3)
#define VFE31_IMASK_REALIGN_BUF_CB_OVFL       (0x00000001<<4)
#define VFE31_IMASK_REALIGN_BUF_CR_OVFL       (0x00000001<<5)
#define VFE31_IMASK_VIOLATION                 (0x00000001<<6)
#define VFE31_IMASK_IMG_MAST_0_BUS_OVFL       (0x00000001<<7)
#define VFE31_IMASK_IMG_MAST_1_BUS_OVFL       (0x00000001<<8)
#define VFE31_IMASK_IMG_MAST_2_BUS_OVFL       (0x00000001<<9)
#define VFE31_IMASK_IMG_MAST_3_BUS_OVFL       (0x00000001<<10)
#define VFE31_IMASK_IMG_MAST_4_BUS_OVFL       (0x00000001<<11)
#define VFE31_IMASK_IMG_MAST_5_BUS_OVFL       (0x00000001<<12)
#define VFE31_IMASK_IMG_MAST_6_BUS_OVFL       (0x00000001<<13)
#define VFE31_IMASK_STATS_AE_BG_BUS_OVFL         (0x00000001<<14)
#define VFE31_IMASK_STATS_AF_BF_BUS_OVFL         (0x00000001<<15)
#define VFE31_IMASK_STATS_AWB_BUS_OVFL        (0x00000001<<16)
#define VFE31_IMASK_STATS_RS_BUS_OVFL         (0x00000001<<17)
#define VFE31_IMASK_STATS_CS_BUS_OVFL         (0x00000001<<18)
#define VFE31_IMASK_STATS_IHIST_BUS_OVFL      (0x00000001<<19)
#define VFE31_IMASK_STATS_SKIN_BHIST_BUS_OVFL       (0x00000001<<20)
#define VFE31_IMASK_AXI_ERROR                 (0x00000001<<21)

#define VFE_COM_STATUS 0x000FE000

struct vfe31_output_path {
	uint16_t output_mode;     /* bitmask  */

	struct vfe31_output_ch out0; /* preview and thumbnail */
	struct vfe31_output_ch out1; /* snapshot */
	struct vfe31_output_ch out2; /* video    */
};

struct vfe31_frame_extra {
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

#define VFE_DISABLE_ALL_IRQS             0
#define VFE_CLEAR_ALL_IRQS               0xffffffff

#define VFE_HW_VERSION					 0x00000000
#define VFE_GLOBAL_RESET                 0x00000004
#define VFE_MODULE_RESET				 0x00000008
#define VFE_CGC_OVERRIDE                 0x0000000C
#define VFE_MODULE_CFG                   0x00000010
#define VFE_CFG				 0x00000014
#define VFE_IRQ_CMD                      0x00000018
#define VFE_IRQ_MASK_0                   0x0000001C
#define VFE_IRQ_MASK_1                   0x00000020
#define VFE_IRQ_CLEAR_0                  0x00000024
#define VFE_IRQ_CLEAR_1                  0x00000028
#define VFE_IRQ_STATUS_0                 0x0000002C
#define VFE_IRQ_STATUS_1                 0x00000030
#define VFE_IRQ_COMP_MASK                0x00000034
#define VFE_BUS_CMD                      0x00000038
#define VFE_BUS_PING_PONG_STATUS         0x00000180
#define VFE_BUS_OPERATION_STATUS         0x00000184

#define VFE_BUS_IMAGE_MASTER_0_WR_PM_STATS_0        0x00000190
#define VFE_BUS_IMAGE_MASTER_0_WR_PM_STATS_1        0x00000194

#define VFE_AXI_CMD                      0x000001D8
#define VFE_AXI_STATUS                   0x000001DC
#define VFE_BUS_STATS_PING_PONG_BASE     0x000000F4

#define VFE_BUS_STATS_AEC_WR_PING_ADDR   0x000000F4
#define VFE_BUS_STATS_AEC_WR_PONG_ADDR   0x000000F8
#define VFE_BUS_STATS_AEC_UB_CFG         0x000000FC
#define VFE_BUS_STATS_AF_WR_PING_ADDR    0x00000100
#define VFE_BUS_STATS_AF_WR_PONG_ADDR    0x00000104
#define VFE_BUS_STATS_AF_UB_CFG          0x00000108
#define VFE_BUS_STATS_AWB_WR_PING_ADDR   0x0000010C
#define VFE_BUS_STATS_AWB_WR_PONG_ADDR   0x00000110
#define VFE_BUS_STATS_AWB_UB_CFG         0x00000114
#define VFE_BUS_STATS_RS_WR_PING_ADDR    0x00000118
#define VFE_BUS_STATS_RS_WR_PONG_ADDR    0x0000011C
#define VFE_BUS_STATS_RS_UB_CFG          0x00000120

#define VFE_BUS_STATS_CS_WR_PING_ADDR    0x00000124
#define VFE_BUS_STATS_CS_WR_PONG_ADDR    0x00000128
#define VFE_BUS_STATS_CS_UB_CFG          0x0000012C
#define VFE_BUS_STATS_HIST_WR_PING_ADDR  0x00000130
#define VFE_BUS_STATS_HIST_WR_PONG_ADDR  0x00000134
#define VFE_BUS_STATS_HIST_UB_CFG        0x00000138
#define VFE_BUS_STATS_SKIN_WR_PING_ADDR  0x0000013C
#define VFE_BUS_STATS_SKIN_WR_PONG_ADDR  0x00000140
#define VFE_BUS_STATS_SKIN_UB_CFG        0x00000144
#define VFE_BUS_PM_CMD                   0x00000188
#define VFE_BUS_PM_CFG                   0x0000018C
#define VFE_CAMIF_COMMAND                0x000001E0
#define VFE_CAMIF_STATUS                 0x00000204
#define VFE_REG_UPDATE_CMD               0x00000260
#define VFE_DEMUX_GAIN_0                 0x00000288
#define VFE_DEMUX_GAIN_1                 0x0000028C
#define VFE_CHROMA_UP                    0x0000035C
#define VFE_FRAMEDROP_ENC_Y_CFG          0x00000504
#define VFE_FRAMEDROP_ENC_CBCR_CFG       0x00000508
#define VFE_FRAMEDROP_ENC_Y_PATTERN      0x0000050C
#define VFE_FRAMEDROP_ENC_CBCR_PATTERN   0x00000510
#define VFE_FRAMEDROP_VIEW_Y             0x00000514
#define VFE_FRAMEDROP_VIEW_CBCR          0x00000518
#define VFE_FRAMEDROP_VIEW_Y_PATTERN     0x0000051C
#define VFE_FRAMEDROP_VIEW_CBCR_PATTERN  0x00000520
#define VFE_CLAMP_MAX                    0x00000524
#define VFE_CLAMP_MIN                    0x00000528
#define VFE_REALIGN_BUF                  0x0000052C
#define VFE_STATS_CFG                    0x00000530
#define VFE_STATS_AWB_SGW_CFG            0x00000554
#define VFE_DMI_CFG                      0x00000598
#define VFE_DMI_ADDR                     0x0000059C
#define VFE_DMI_DATA_LO                  0x000005A4
#define VFE_AXI_CFG                      0x00000600

#define VFE31_OUTPUT_MODE_PT		BIT(0)
#define VFE31_OUTPUT_MODE_S			BIT(1)
#define VFE31_OUTPUT_MODE_V			BIT(2)
#define VFE31_OUTPUT_MODE_P			BIT(3)
#define VFE31_OUTPUT_MODE_T			BIT(4)
#define VFE31_OUTPUT_MODE_P_ALL_CHNLS		BIT(5)
#define VFE31_OUTPUT_MODE_PRIMARY		BIT(6)
#define VFE31_OUTPUT_MODE_PRIMARY_ALL_CHNLS	BIT(7)
#define VFE31_OUTPUT_MODE_SECONDARY		BIT(8)
#define VFE31_OUTPUT_MODE_SECONDARY_ALL_CHNLS	BIT(9)
struct vfe_stats_control {
	uint8_t  ackPending;
	uint32_t nextFrameAddrBuf;
	uint32_t droppedStatsFrameCount;
	uint32_t bufToRender;
};

struct vfe31_ctrl_type {
	uint16_t operation_mode;     /* streaming or snapshot */
	struct vfe31_output_path outpath;

	uint32_t vfeImaskCompositePacked;

	spinlock_t  stop_flag_lock;
	spinlock_t  update_ack_lock;
	spinlock_t  state_lock;
	spinlock_t  io_lock;

	spinlock_t  aec_ack_lock;
	spinlock_t  awb_ack_lock;
	spinlock_t  af_ack_lock;
	spinlock_t  ihist_ack_lock;
	spinlock_t  rs_ack_lock;
	spinlock_t  cs_ack_lock;
	spinlock_t  comp_stats_ack_lock;

	uint32_t extlen;
	void *extdata;

	int8_t start_ack_pending;
	int8_t stop_ack_pending;
	int8_t reset_ack_pending;
	int8_t update_ack_pending;
	enum vfe_output_state recording_state;
	int8_t update_linear;
	int8_t update_rolloff;
	int8_t update_la;
	int8_t update_gamma;
	enum vfe_output_state liveshot_state;

	spinlock_t  tasklet_lock;
	struct list_head tasklet_q;
	void __iomem *vfebase;
	void __iomem *camifbase;
	void *syncdata;
	uint32_t register_total;

	struct resource	*vfemem;
	struct resource	*camifmem;
	struct resource *vfeio;
	struct resource *camifio;
	struct resource *vfeirq;
	struct regulator *fs_vfe;

	uint32_t stats_comp;
	atomic_t vstate;
	uint32_t vfe_capture_count;
	uint32_t sync_timer_repeat_count;
	uint32_t sync_timer_state;
	uint32_t sync_timer_number;

	uint32_t vfeFrameId;
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
	struct clk *vfe_clk[5];
	struct clk *vfe_camif_clk[2];
	spinlock_t  sd_notify_lock;
	uint32_t hfr_mode;
	uint32_t frame_skip_cnt;
	uint32_t frame_skip_pattern;
	uint32_t snapshot_frame_cnt;
};

enum VFE31_STATS_NUM {
	STATS_AE_NUM,
	STATS_AF_NUM,
	STATS_AWB_NUM,
	STATS_RS_NUM,
	STATS_CS_NUM,
	STATS_IHIST_NUM,
	STATS_SKIN_NUM,
	STATS_MAX_NUM,
};

struct vfe_cmd_stats_ack {
	uint32_t  nextStatsBuf;
};

#define VFE_STATS_BUFFER_COUNT            3

struct vfe_cmd_stats_buf {
	uint32_t statsBuf[VFE_STATS_BUFFER_COUNT];
};
#endif /* __MSM_VFE31_H__ */
