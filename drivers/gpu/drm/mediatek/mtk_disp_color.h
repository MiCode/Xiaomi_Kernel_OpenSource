/*
 * Copyright (c) 2019 MediaTek Inc.
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

struct mtk_pq_reg_table {
	char name[16];
	unsigned long reg_base;
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
#define DISP_COLOR_START(module)		((module)->data->color_offset)
#define DISP_COLOR_INTEN(reg)		(DISP_COLOR_START(reg) + 0x4UL)
#define DISP_COLOR_OUT_SEL(reg)		(DISP_COLOR_START(reg) + 0xCUL)
#define DISP_COLOR_WIDTH(reg)		(DISP_COLOR_START(reg) + 0x50UL)
#define DISP_COLOR_HEIGHT(reg)		(DISP_COLOR_START(reg) + 0x54UL)
#define DISP_COLOR_CM1_EN(reg)		(DISP_COLOR_START(reg) + 0x60UL)
#define DISP_COLOR_CM2_EN(reg)		(DISP_COLOR_START(reg) + 0xA0UL)


#define COLOR_BYPASS_ALL		BIT(7)
#define COLOR_SEQ_SEL			BIT(13)

struct DISP_PQ_PARAM *get_Color_config(int id);
struct DISPLAY_PQ_T *get_Color_index(void);
bool disp_color_reg_get(struct mtk_ddp_comp *comp,
	unsigned long addr, int *value);
void disp_color_set_window(struct mtk_ddp_comp *comp,
	unsigned int sat_upper, unsigned int sat_lower,
	unsigned int hue_upper, unsigned int hue_lower);

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

#endif


