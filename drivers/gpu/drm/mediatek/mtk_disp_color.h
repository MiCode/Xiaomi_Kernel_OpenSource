/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_DISP_COLOR_H__
#define __MTK_DISP_COLOR_H__

enum disp_color_id_t {
	COLOR_ID_0 = 0,
	COLOR_ID_1 = 1,
	DISP_COLOR_TOTAL
};

enum WINDOW_SETTING {
	WIN1 = 0,
	WIN2,
	WIN3,
	WIN_TOTAL
};

enum LUT_YHS {
	LUT_H = 0,
	LUT_Y,
	LUT_S,
	LUT_TOTAL
};

enum LUT_REG {
	REG_SLOPE0 = 0,
	REG_SLOPE1,
	REG_SLOPE2,
	REG_SLOPE3,
	REG_SLOPE4,
	REG_SLOPE5,
	REG_WGT_LSLOPE,
	REG_WGT_USLOPE,
	REG_L,
	REG_POINT0,
	REG_POINT1,
	REG_POINT2,
	REG_POINT3,
	REG_POINT4,
	REG_U,
	LUT_REG_TOTAL
};

#define SG1 0
#define SG2 1
#define SG3 2
#define SP1 3
#define SP2 4

#define PURP_TONE_START    0
#define PURP_TONE_END      2
#define SKIN_TONE_START    3
#define SKIN_TONE_END     10
#define GRASS_TONE_START  11
#define GRASS_TONE_END    16
#define SKY_TONE_START    17
#define SKY_TONE_END      19

/* Register */
#define DISP_COLOR_CFG_MAIN			0x0400
#define DISP_COLOR_WIN_X_MAIN		0x40c
#define DISP_COLOR_WIN_Y_MAIN		0x410
#define DISP_COLOR_DBG_CFG_MAIN		0x420
#define DISP_COLOR_C_BOOST_MAIN		0x428
#define DISP_COLOR_C_BOOST_MAIN_2	0x42C
#define DISP_COLOR_LUMA_ADJ			0x430
#define DISP_COLOR_G_PIC_ADJ_MAIN_1	0x434
#define DISP_COLOR_G_PIC_ADJ_MAIN_2	0x438
#define DISP_COLOR_CAP_IN_DATA_MAIN 0x490
#define DISP_COLOR_CAP_IN_DATA_MAIN_CR 0x494
#define DISP_COLOR_CAP_OUT_DATA_MAIN 0x498
#define DISP_COLOR_CAP_OUT_DATA_MAIN_CR 0x49C
#define DISP_COLOR_Y_SLOPE_1_0_MAIN 0x4A0
#define DISP_COLOR_LOCAL_HUE_CD_0	0x620
#define DISP_COLOR_TWO_D_WINDOW_1	0x740
#define DISP_COLOR_TWO_D_W1_RESULT	0x74C
#define DISP_COLOR_PART_SAT_GAIN1_0 0x7FC
#define DISP_COLOR_PART_SAT_GAIN1_1 0x800
#define DISP_COLOR_PART_SAT_GAIN1_2 0x804
#define DISP_COLOR_PART_SAT_GAIN1_3 0x808
#define DISP_COLOR_PART_SAT_GAIN1_4 0x80C
#define DISP_COLOR_PART_SAT_GAIN2_0 0x810
#define DISP_COLOR_PART_SAT_GAIN2_1 0x814
#define DISP_COLOR_PART_SAT_GAIN2_2 0x818
#define DISP_COLOR_PART_SAT_GAIN2_3	0x81C
#define DISP_COLOR_PART_SAT_GAIN2_4 0x820
#define DISP_COLOR_PART_SAT_GAIN3_0 0x824
#define DISP_COLOR_PART_SAT_GAIN3_1 0x828
#define DISP_COLOR_PART_SAT_GAIN3_2 0x82C
#define DISP_COLOR_PART_SAT_GAIN3_3 0x830
#define DISP_COLOR_PART_SAT_GAIN3_4 0x834
#define DISP_COLOR_PART_SAT_POINT1_0 0x838
#define DISP_COLOR_PART_SAT_POINT1_1 0x83C
#define DISP_COLOR_PART_SAT_POINT1_2 0x840
#define DISP_COLOR_PART_SAT_POINT1_3 0x844
#define DISP_COLOR_PART_SAT_POINT1_4 0x848
#define DISP_COLOR_PART_SAT_POINT2_0 0x84C
#define DISP_COLOR_PART_SAT_POINT2_1 0x850
#define DISP_COLOR_PART_SAT_POINT2_2 0x854
#define DISP_COLOR_PART_SAT_POINT2_3 0x858
#define DISP_COLOR_PART_SAT_POINT2_4 0x85C
#define DISP_COLOR_CM_CONTROL		0x860
#define DISP_COLOR_CM_W1_HUE_0		0x864
#define DISP_COLOR_CM_W1_HUE_1      0x868
#define DISP_COLOR_CM_W1_HUE_2      0x86C
#define DISP_COLOR_CM_W1_HUE_3      0x870
#define DISP_COLOR_CM_W1_HUE_4      0x874
/*
 * #define DISP_COLOR_CM_W1_LUMA_0     0x878
 * #define DISP_COLOR_CM_W1_LUMA_1     0x87C
 * #define DISP_COLOR_CM_W1_LUMA_2     0x880
 * #define DISP_COLOR_CM_W1_LUMA_3     0x884
 * #define DISP_COLOR_CM_W1_LUMA_4     0x888
 * #define DISP_COLOR_CM_W1_SAT_0      0x88C
 * #define DISP_COLOR_CM_W1_SAT_1      0x890
 * #define DISP_COLOR_CM_W1_SAT_2      0x894
 * #define DISP_COLOR_CM_W1_SAT_3      0x898
 * #define DISP_COLOR_CM_W1_SAT_4      0x89C
 * #define DISP_COLOR_CM_W2_HUE_0      0x8A0
 * #define DISP_COLOR_CM_W2_HUE_1      0x8A4
 * #define DISP_COLOR_CM_W2_HUE_2      0x8A8
 * #define DISP_COLOR_CM_W2_HUE_3      0x8AC
 * #define DISP_COLOR_CM_W2_HUE_4      0x8B0
 * #define DISP_COLOR_CM_W2_LUMA_0     0x8B4
 * #define DISP_COLOR_CM_W2_LUMA_1     0x8B8
 * #define DISP_COLOR_CM_W2_LUMA_2     0x8BC
 * #define DISP_COLOR_CM_W2_LUMA_3     0x8C0
 * #define DISP_COLOR_CM_W2_LUMA_4     0x8C4
 * #define DISP_COLOR_CM_W2_SAT_0      0x8C8
 * #define DISP_COLOR_CM_W2_SAT_1      0x8CC
 * #define DISP_COLOR_CM_W2_SAT_2      0x8D0
 * #define DISP_COLOR_CM_W2_SAT_3      0x8D4
 * #define DISP_COLOR_CM_W2_SAT_4      0x8D8
 * #define DISP_COLOR_CM_W3_HUE_0      0x8DC
 * #define DISP_COLOR_CM_W3_HUE_1      0x8E0
 * #define DISP_COLOR_CM_W3_HUE_2      0x8E4
 * #define DISP_COLOR_CM_W3_HUE_3      0x8E8
 * #define DISP_COLOR_CM_W3_HUE_4      0x8EC
 * #define DISP_COLOR_CM_W3_LUMA_0     0x8F0
 * #define DISP_COLOR_CM_W3_LUMA_1     0x8F4
 * #define DISP_COLOR_CM_W3_LUMA_2     0x8F8
 * #define DISP_COLOR_CM_W3_LUMA_3     0x8FC
 * #define DISP_COLOR_CM_W3_LUMA_4     0x900
 * #define DISP_COLOR_CM_W3_SAT_0      0x904
 * #define DISP_COLOR_CM_W3_SAT_1      0x908
 * #define DISP_COLOR_CM_W3_SAT_2      0x90C
 * #define DISP_COLOR_CM_W3_SAT_3      0x910
 * #define DISP_COLOR_CM_W3_SAT_4      0x914
 */

#define DISP_COLOR_S_GAIN_BY_Y0_0	0xCF4
#define DISP_COLOR_LSP_1			0xD58
#define DISP_COLOR_LSP_2			0xD5C

#define DISP_COLOR_START_MT2701		0x0f00
#define DISP_COLOR_START_MT6779		0x0c00
#define DISP_COLOR_START_MT6885		0x0c00
#define DISP_COLOR_START_MT8173		0x0c00
#define DISP_COLOR_START_MT6873		0x0c00
#define DISP_COLOR_START_MT6781		0x0c00
#define DISP_COLOR_START(module)		((module)->data->color_offset)
#define DISP_COLOR_INTEN(reg)		(DISP_COLOR_START(reg) + 0x4UL)
#define DISP_COLOR_OUT_SEL(reg)		(DISP_COLOR_START(reg) + 0xCUL)
#define DISP_COLOR_WIDTH(reg)		(DISP_COLOR_START(reg) + 0x50UL)
#define DISP_COLOR_HEIGHT(reg)		(DISP_COLOR_START(reg) + 0x54UL)
#define DISP_COLOR_CM1_EN(reg)		(DISP_COLOR_START(reg) + 0x60UL)
#define DISP_COLOR_CM2_EN(reg)		(DISP_COLOR_START(reg) + 0xA0UL)

#define DISP_COLOR_SHADOW_CTRL		0x0cb0
#define COLOR_BYPASS_SHADOW		BIT(0)
#define COLOR_READ_WRK_REG		BIT(2)

#define COLOR_BYPASS_ALL		BIT(7)
#define COLOR_SEQ_SEL			BIT(13)

struct DISP_PQ_PARAM *get_Color_config(int id);
struct DISPLAY_PQ_T *get_Color_index(void);
bool disp_color_reg_get(struct mtk_ddp_comp *comp,
	unsigned long addr, int *value);
void disp_color_set_window(struct mtk_ddp_comp *comp,
	unsigned int sat_upper, unsigned int sat_lower,
	unsigned int hue_upper, unsigned int hue_lower);
void mtk_color_setbypass(struct mtk_ddp_comp *comp, struct cmdq_pkt *handle,
		bool bypass);
void ddp_color_bypass_color(struct mtk_ddp_comp *comp, int bypass,
		struct cmdq_pkt *handle);

int mtk_drm_ioctl_set_pqparam(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
int mtk_drm_ioctl_set_pqindex(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
int mtk_drm_ioctl_set_color_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
int mtk_drm_ioctl_mutex_control(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
int mtk_drm_ioctl_read_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
int mtk_drm_ioctl_write_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
int mtk_drm_ioctl_bypass_color(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
int mtk_drm_ioctl_pq_set_window(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
int mtk_drm_ioctl_read_sw_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv);
int mtk_drm_ioctl_write_sw_reg(struct drm_device *dev, void *data,
		struct drm_file *file_priv);


// SW Reg
/* ------------------------------------------------------------------------- */

#define MIRAVISION_HW_VERSION_MASK  (0xFF000000)
#define MIRAVISION_SW_VERSION_MASK  (0x00FF0000)
#define MIRAVISION_SW_FEATURE_MASK  (0x0000FFFF)
#define MIRAVISION_HW_VERSION_SHIFT (24)
#define MIRAVISION_SW_VERSION_SHIFT (16)
#define MIRAVISION_SW_FEATURE_SHIFT (0)

#if defined(CONFIG_MACH_MT6595)
#define MIRAVISION_HW_VERSION       (1)
#elif defined(CONFIG_MACH_MT6752)
#define MIRAVISION_HW_VERSION       (2)
#elif defined(CONFIG_MACH_MT6795)
#define MIRAVISION_HW_VERSION       (3)
#elif defined(CONFIG_MACH_MT6735) || defined(CONFIG_MACH_MT8167)
#define MIRAVISION_HW_VERSION       (4)
#elif defined(CONFIG_MACH_MT6735M)
#define MIRAVISION_HW_VERSION       (5)
#elif defined(CONFIG_MACH_MT6753)
#define MIRAVISION_HW_VERSION       (6)
#elif defined(CONFIG_MACH_MT6580)
#define MIRAVISION_HW_VERSION       (7)
#elif defined(CONFIG_MACH_MT6755)
#define MIRAVISION_HW_VERSION       (8)
#elif defined(CONFIG_MACH_MT6797)
#define MIRAVISION_HW_VERSION       (9)
#elif defined(CONFIG_MACH_MT6750)
#define MIRAVISION_HW_VERSION       (10)
#elif defined(CONFIG_MACH_MT6757)
#define MIRAVISION_HW_VERSION       (11)
#define MIRAVISION_HW_P_VERSION     (13)
#elif defined(CONFIG_MACH_MT6799)
#define MIRAVISION_HW_VERSION       (12)
#elif defined(CONFIG_MACH_MT6763)
#define MIRAVISION_HW_VERSION       (14)
#elif defined(CONFIG_MACH_MT6758)
#define MIRAVISION_HW_VERSION       (15)
#elif defined(CONFIG_MACH_MT6739)
#define MIRAVISION_HW_VERSION       (16)
#elif defined(CONFIG_MACH_MT6775)
#define MIRAVISION_HW_VERSION       (17)
#elif defined(CONFIG_MACH_MT6771)
#define MIRAVISION_HW_VERSION       (18)
#elif defined(CONFIG_MACH_MT6765)
#define MIRAVISION_HW_VERSION       (19)
#else
#define MIRAVISION_HW_VERSION       (0)
#endif

#define MIRAVISION_SW_VERSION       (3)	/* 3:Android N*/
#define MIRAVISION_SW_FEATURE_VIDEO_DC  (0x1)
#define MIRAVISION_SW_FEATURE_AAL       (0x2)
#define MIRAVISION_SW_FEATURE_PQDS       (0x4)

#if defined(CONFIG_MACH_MT6757)
#define MIRAVISION_VERSION \
	((color_get_chip_ver() << MIRAVISION_HW_VERSION_SHIFT) | \
	(MIRAVISION_SW_VERSION << MIRAVISION_SW_VERSION_SHIFT) | \
	MIRAVISION_SW_FEATURE_VIDEO_DC | \
	MIRAVISION_SW_FEATURE_AAL | \
	MIRAVISION_SW_FEATURE_PQDS)
#else
#define MIRAVISION_VERSION \
	((MIRAVISION_HW_VERSION << MIRAVISION_HW_VERSION_SHIFT) | \
	(MIRAVISION_SW_VERSION << MIRAVISION_SW_VERSION_SHIFT) | \
	MIRAVISION_SW_FEATURE_VIDEO_DC | \
	MIRAVISION_SW_FEATURE_AAL | \
	MIRAVISION_SW_FEATURE_PQDS)
#endif

#define SW_VERSION_VIDEO_DC         (1)
#define SW_VERSION_AAL              (1)
#if defined(CONFIG_MACH_MT6755)
#define SW_VERSION_PQDS             (2)
#else
#define SW_VERSION_PQDS             (1)
#endif

#define DISP_COLOR_SWREG_START          (0xFFFF0000)
#define DISP_COLOR_SWREG_COLOR_BASE     (DISP_COLOR_SWREG_START)
#define DISP_COLOR_SWREG_TDSHP_BASE     (DISP_COLOR_SWREG_COLOR_BASE + 0x1000)
#define DISP_COLOR_SWREG_PQDC_BASE      (DISP_COLOR_SWREG_TDSHP_BASE + 0x1000)


#define DISP_COLOR_SWREG_PQDS_BASE       (DISP_COLOR_SWREG_PQDC_BASE + 0x1000)
#define DISP_COLOR_SWREG_MDP_COLOR_BASE  (DISP_COLOR_SWREG_PQDS_BASE + 0x1000)
#define DISP_COLOR_SWREG_END        (DISP_COLOR_SWREG_MDP_COLOR_BASE + 0x1000)

#define SWREG_COLOR_BASE_ADDRESS        (DISP_COLOR_SWREG_COLOR_BASE + 0x0000)
#define SWREG_GAMMA_BASE_ADDRESS        (DISP_COLOR_SWREG_COLOR_BASE + 0x0001)
#define SWREG_TDSHP_BASE_ADDRESS        (DISP_COLOR_SWREG_COLOR_BASE + 0x0002)
#define SWREG_AAL_BASE_ADDRESS          (DISP_COLOR_SWREG_COLOR_BASE + 0x0003)
#define SWREG_MIRAVISION_VERSION        (DISP_COLOR_SWREG_COLOR_BASE + 0x0004)
#define SWREG_SW_VERSION_VIDEO_DC       (DISP_COLOR_SWREG_COLOR_BASE + 0x0005)
#define SWREG_SW_VERSION_AAL            (DISP_COLOR_SWREG_COLOR_BASE + 0x0006)
#define SWREG_CCORR_BASE_ADDRESS        (DISP_COLOR_SWREG_COLOR_BASE + 0x0007)
#define SWREG_MDP_COLOR_BASE_ADDRESS    (DISP_COLOR_SWREG_COLOR_BASE + 0x0008)
#define SWREG_COLOR_MODE                (DISP_COLOR_SWREG_COLOR_BASE + 0x0009)
#define SWREG_RSZ_BASE_ADDRESS          (DISP_COLOR_SWREG_COLOR_BASE + 0x000A)
#define SWREG_MDP_RDMA_BASE_ADDRESS     (DISP_COLOR_SWREG_COLOR_BASE + 0x000B)
#define SWREG_MDP_AAL_BASE_ADDRESS      (DISP_COLOR_SWREG_COLOR_BASE + 0x000C)
#define SWREG_MDP_HDR_BASE_ADDRESS      (DISP_COLOR_SWREG_COLOR_BASE + 0x000D)

#define SWREG_TDSHP_TUNING_MODE         (DISP_COLOR_SWREG_TDSHP_BASE + 0x0000)
#define SWREG_TDSHP_GAIN_MID	        (DISP_COLOR_SWREG_TDSHP_BASE + 0x0001)
#define SWREG_TDSHP_GAIN_HIGH	        (DISP_COLOR_SWREG_TDSHP_BASE + 0x0002)
#define SWREG_TDSHP_COR_GAIN	        (DISP_COLOR_SWREG_TDSHP_BASE + 0x0003)
#define SWREG_TDSHP_COR_THR             (DISP_COLOR_SWREG_TDSHP_BASE + 0x0004)
#define SWREG_TDSHP_COR_ZERO	        (DISP_COLOR_SWREG_TDSHP_BASE + 0x0005)
#define SWREG_TDSHP_GAIN                (DISP_COLOR_SWREG_TDSHP_BASE + 0x0006)
#define SWREG_TDSHP_COR_VALUE	        (DISP_COLOR_SWREG_TDSHP_BASE + 0x0007)

#define SWREG_PQDC_BLACK_EFFECT_ENABLE \
	(DISP_COLOR_SWREG_PQDC_BASE + BlackEffectEnable)
#define SWREG_PQDC_WHITE_EFFECT_ENABLE \
	(DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectEnable)
#define SWREG_PQDC_STRONG_BLACK_EFFECT \
	(DISP_COLOR_SWREG_PQDC_BASE + StrongBlackEffect)
#define SWREG_PQDC_STRONG_WHITE_EFFECT \
	(DISP_COLOR_SWREG_PQDC_BASE + StrongWhiteEffect)
#define SWREG_PQDC_ADAPTIVE_BLACK_EFFECT \
	(DISP_COLOR_SWREG_PQDC_BASE + AdaptiveBlackEffect)
#define SWREG_PQDC_ADAPTIVE_WHITE_EFFECT \
	(DISP_COLOR_SWREG_PQDC_BASE + AdaptiveWhiteEffect)
#define SWREG_PQDC_SCENCE_CHANGE_ONCE_EN  \
	(DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeOnceEn)
#define SWREG_PQDC_SCENCE_CHANGE_CONTROL_EN \
	(DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeControlEn)
#define SWREG_PQDC_SCENCE_CHANGE_CONTROL \
	(DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeControl)
#define SWREG_PQDC_SCENCE_CHANGE_TH1 \
	(DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeTh1)
#define SWREG_PQDC_SCENCE_CHANGE_TH2 \
	(DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeTh2)
#define SWREG_PQDC_SCENCE_CHANGE_TH3 \
	(DISP_COLOR_SWREG_PQDC_BASE + ScenceChangeTh3)
#define SWREG_PQDC_CONTENT_SMOOTH1 \
	(DISP_COLOR_SWREG_PQDC_BASE + ContentSmooth1)
#define SWREG_PQDC_CONTENT_SMOOTH2 \
	(DISP_COLOR_SWREG_PQDC_BASE + ContentSmooth2)
#define SWREG_PQDC_CONTENT_SMOOTH3 \
	(DISP_COLOR_SWREG_PQDC_BASE + ContentSmooth3)
#define SWREG_PQDC_MIDDLE_REGION_GAIN1 \
	(DISP_COLOR_SWREG_PQDC_BASE + MiddleRegionGain1)
#define SWREG_PQDC_MIDDLE_REGION_GAIN2 \
	(DISP_COLOR_SWREG_PQDC_BASE + MiddleRegionGain2)
#define SWREG_PQDC_BLACK_REGION_GAIN1 \
	(DISP_COLOR_SWREG_PQDC_BASE + BlackRegionGain1)
#define SWREG_PQDC_BLACK_REGION_GAIN2 \
	(DISP_COLOR_SWREG_PQDC_BASE + BlackRegionGain2)
#define SWREG_PQDC_BLACK_REGION_RANGE \
	(DISP_COLOR_SWREG_PQDC_BASE + BlackRegionRange)
#define SWREG_PQDC_BLACK_EFFECT_LEVEL \
	(DISP_COLOR_SWREG_PQDC_BASE + BlackEffectLevel)
#define SWREG_PQDC_BLACK_EFFECT_PARAM1 \
	(DISP_COLOR_SWREG_PQDC_BASE + BlackEffectParam1)
#define SWREG_PQDC_BLACK_EFFECT_PARAM2 \
	(DISP_COLOR_SWREG_PQDC_BASE + BlackEffectParam2)
#define SWREG_PQDC_BLACK_EFFECT_PARAM3 \
	(DISP_COLOR_SWREG_PQDC_BASE + BlackEffectParam3)
#define SWREG_PQDC_BLACK_EFFECT_PARAM4 \
	(DISP_COLOR_SWREG_PQDC_BASE + BlackEffectParam4)
#define SWREG_PQDC_WHITE_REGION_GAIN1 \
	(DISP_COLOR_SWREG_PQDC_BASE + WhiteRegionGain1)
#define SWREG_PQDC_WHITE_REGION_GAIN2 \
	(DISP_COLOR_SWREG_PQDC_BASE + WhiteRegionGain2)
#define SWREG_PQDC_WHITE_REGION_RANGE \
	(DISP_COLOR_SWREG_PQDC_BASE + WhiteRegionRange)
#define SWREG_PQDC_WHITE_EFFECT_LEVEL \
	(DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectLevel)
#define SWREG_PQDC_WHITE_EFFECT_PARAM1 \
	(DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectParam1)
#define SWREG_PQDC_WHITE_EFFECT_PARAM2 \
	(DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectParam2)
#define SWREG_PQDC_WHITE_EFFECT_PARAM3 \
	(DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectParam3)
#define SWREG_PQDC_WHITE_EFFECT_PARAM4 \
	(DISP_COLOR_SWREG_PQDC_BASE + WhiteEffectParam4)
#define SWREG_PQDC_CONTRAST_ADJUST1 \
	(DISP_COLOR_SWREG_PQDC_BASE + ContrastAdjust1)
#define SWREG_PQDC_CONTRAST_ADJUST2 \
	(DISP_COLOR_SWREG_PQDC_BASE + ContrastAdjust2)
#define SWREG_PQDC_DC_CHANGE_SPEED_LEVEL \
	(DISP_COLOR_SWREG_PQDC_BASE + DCChangeSpeedLevel)
#define SWREG_PQDC_PROTECT_REGION_EFFECT \
	(DISP_COLOR_SWREG_PQDC_BASE + ProtectRegionEffect)
#define SWREG_PQDC_DC_CHANGE_SPEED_LEVEL2 \
	(DISP_COLOR_SWREG_PQDC_BASE + DCChangeSpeedLevel2)
#define SWREG_PQDC_PROTECT_REGION_WEIGHT \
	(DISP_COLOR_SWREG_PQDC_BASE + ProtectRegionWeight)
#define SWREG_PQDC_DC_ENABLE \
	(DISP_COLOR_SWREG_PQDC_BASE + DCEnable)

#define SWREG_PQDS_DS_EN         (DISP_COLOR_SWREG_PQDS_BASE + DS_en)
#define SWREG_PQDS_UP_SLOPE      (DISP_COLOR_SWREG_PQDS_BASE + iUpSlope)
#define SWREG_PQDS_UP_THR        (DISP_COLOR_SWREG_PQDS_BASE + iUpThreshold)
#define SWREG_PQDS_DOWN_SLOPE    (DISP_COLOR_SWREG_PQDS_BASE + iDownSlope)
#define SWREG_PQDS_DOWN_THR      (DISP_COLOR_SWREG_PQDS_BASE + iDownThreshold)
#define SWREG_PQDS_ISO_EN        (DISP_COLOR_SWREG_PQDS_BASE + iISO_en)
#define SWREG_PQDS_ISO_THR1      (DISP_COLOR_SWREG_PQDS_BASE + iISO_thr1)
#define SWREG_PQDS_ISO_THR0      (DISP_COLOR_SWREG_PQDS_BASE + iISO_thr0)
#define SWREG_PQDS_ISO_THR3      (DISP_COLOR_SWREG_PQDS_BASE + iISO_thr3)
#define SWREG_PQDS_ISO_THR2      (DISP_COLOR_SWREG_PQDS_BASE + iISO_thr2)
#define SWREG_PQDS_ISO_IIR       (DISP_COLOR_SWREG_PQDS_BASE + iISO_IIR_alpha)
#define SWREG_PQDS_COR_ZERO_2    (DISP_COLOR_SWREG_PQDS_BASE + iCorZero_clip2)
#define SWREG_PQDS_COR_ZERO_1    (DISP_COLOR_SWREG_PQDS_BASE + iCorZero_clip1)
#define SWREG_PQDS_COR_ZERO_0    (DISP_COLOR_SWREG_PQDS_BASE + iCorZero_clip0)
#define SWREG_PQDS_COR_THR_2     (DISP_COLOR_SWREG_PQDS_BASE + iCorThr_clip2)
#define SWREG_PQDS_COR_THR_1     (DISP_COLOR_SWREG_PQDS_BASE + iCorThr_clip1)
#define SWREG_PQDS_COR_THR_0     (DISP_COLOR_SWREG_PQDS_BASE + iCorThr_clip0)
#define SWREG_PQDS_COR_GAIN_2    (DISP_COLOR_SWREG_PQDS_BASE + iCorGain_clip2)
#define SWREG_PQDS_COR_GAIN_1    (DISP_COLOR_SWREG_PQDS_BASE + iCorGain_clip1)
#define SWREG_PQDS_COR_GAIN_0    (DISP_COLOR_SWREG_PQDS_BASE + iCorGain_clip0)
#define SWREG_PQDS_GAIN_2        (DISP_COLOR_SWREG_PQDS_BASE + iGain_clip2)
#define SWREG_PQDS_GAIN_1        (DISP_COLOR_SWREG_PQDS_BASE + iGain_clip1)
#define SWREG_PQDS_GAIN_0        (DISP_COLOR_SWREG_PQDS_BASE + iGain_clip0)
#define SWREG_PQDS_END           (DISP_COLOR_SWREG_PQDS_BASE + PQ_DS_INDEX_MAX)

#define SWREG_MDP_COLOR_CAPTURE_EN \
	(DISP_COLOR_SWREG_MDP_COLOR_BASE + 0x0000)
#define SWREG_MDP_COLOR_CAPTURE_POS_X \
	(DISP_COLOR_SWREG_MDP_COLOR_BASE + 0x0001)
#define SWREG_MDP_COLOR_CAPTURE_POS_Y \
	(DISP_COLOR_SWREG_MDP_COLOR_BASE + 0x0002)


/* ------------------------------------------------------------------------- */




#endif


