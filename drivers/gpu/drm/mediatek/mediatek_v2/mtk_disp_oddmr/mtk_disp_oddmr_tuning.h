/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef __MTK_DISP_ODDMR_TUNING_H__
#define __MTK_DISP_ODDMR_TUNING_H__

/* od current status */
#define	OD_CURRENT_TABLE	0x0000
#define	OD_CURRENT_BL	0x0001
#define	OD_CURRENT_FPS	0x0002
#define	OD_CURRENT_HDISPLAY	0x0003
#define	OD_CURRENT_VDISPLAY	0x0004

/* od_basic_info */
#define	OD_BASIC_PANEL_ID_0	0x0010
#define	OD_BASIC_PANEL_ID_1	0x0011
#define	OD_BASIC_PANEL_ID_2	0x0012
#define	OD_BASIC_PANEL_ID_3	0x0013
#define	OD_BASIC_RESOLUTION_SWITCH_MODE	0x0014
#define	OD_BASIC_PANEL_WIDTH	0x0015
#define	OD_BASIC_PANEL_HEIGHT	0x0016
#define	OD_BASIC_TABLE_CNT	0x0017
#define	OD_BASIC_OD_MODE	0x0018
#define	OD_BASIC_DITHER_SEL	0x0019
#define	OD_BASIC_DITHER_CTL	0x001A
#define	OD_BASIC_SCALING_MODE	0x001B
#define	OD_BASIC_OD_HSK2	0x001C
#define	OD_BASIC_OD_HSK3	0x001D
#define	OD_BASIC_OD_HSK4	0x001E
#define	OD_BASIC_RESERVED	0x001F

/* od_table_basic_info */
#define	OD_TABLE_WIDTH	0x0030
#define	OD_TABLE_HEIGHT	0x0031
#define	OD_TABLE_FPS	0x0032
#define	OD_TABLE_BL	0x0033
#define	OD_TABLE_MIN_FPS	0x0034
#define	OD_TABLE_MAX_FPS	0x0035
#define	OD_TABLE_MIN_BL	0x0036
#define	OD_TABLE_MAX_BL	0x0037
#define	OD_TABLE_RESERVED	0x0038

/* od_fps_table */
#define	OD_TABLE_FPS_CNT	0x0040
#define	OD_TABLE_FPS0	0x0041
#define	OD_TABLE_FPS1	0x0042
#define	OD_TABLE_FPS2	0x0043
#define	OD_TABLE_FPS3	0x0044
#define	OD_TABLE_FPS4	0x0045
#define	OD_TABLE_FPS5	0x0046
#define	OD_TABLE_FPS6	0x0047
#define	OD_TABLE_FPS7	0x0048
#define	OD_TABLE_FPS8	0x0049
#define	OD_TABLE_FPS9	0x004A
#define	OD_TABLE_FPS10	0x004B
#define	OD_TABLE_FPS11	0x004C
#define	OD_TABLE_FPS12	0x004D
#define	OD_TABLE_FPS13	0x004E
#define	OD_TABLE_FPS14	0x004F
#define	OD_TABLE_FPS_WEIGHT0	0x0050
#define	OD_TABLE_FPS_WEIGHT1	0x0051
#define	OD_TABLE_FPS_WEIGHT2	0x0052
#define	OD_TABLE_FPS_WEIGHT3	0x0053
#define	OD_TABLE_FPS_WEIGHT4	0x0054
#define	OD_TABLE_FPS_WEIGHT5	0x0055
#define	OD_TABLE_FPS_WEIGHT6	0x0056
#define	OD_TABLE_FPS_WEIGHT7	0x0057
#define	OD_TABLE_FPS_WEIGHT8	0x0058
#define	OD_TABLE_FPS_WEIGHT9	0x0059
#define	OD_TABLE_FPS_WEIGHT10	0x005A
#define	OD_TABLE_FPS_WEIGHT11	0x005B
#define	OD_TABLE_FPS_WEIGHT12	0x005C
#define	OD_TABLE_FPS_WEIGHT13	0x005D
#define	OD_TABLE_FPS_WEIGHT14	0x005E

/* od_bl_table */
#define	OD_TABLE_BL_CNT	0x005F
#define	OD_TABLE_BL0	0x0060
#define	OD_TABLE_BL1	0x0061
#define	OD_TABLE_BL2	0x0062
#define	OD_TABLE_BL3	0x0063
#define	OD_TABLE_BL4	0x0064
#define	OD_TABLE_BL5	0x0065
#define	OD_TABLE_BL6	0x0066
#define	OD_TABLE_BL7	0x0067
#define	OD_TABLE_BL8	0x0068
#define	OD_TABLE_BL9	0x0069
#define	OD_TABLE_BL10	0x006A
#define	OD_TABLE_BL11	0x006B
#define	OD_TABLE_BL12	0x006C
#define	OD_TABLE_BL13	0x006D
#define	OD_TABLE_BL14	0x006E
#define	OD_TABLE_BL_WEIGHT0	0x006F
#define	OD_TABLE_BL_WEIGHT1	0x0070
#define	OD_TABLE_BL_WEIGHT2	0x0071
#define	OD_TABLE_BL_WEIGHT3	0x0072
#define	OD_TABLE_BL_WEIGHT4	0x0073
#define	OD_TABLE_BL_WEIGHT5	0x0074
#define	OD_TABLE_BL_WEIGHT6	0x0075
#define	OD_TABLE_BL_WEIGHT7	0x0076
#define	OD_TABLE_BL_WEIGHT8	0x0077
#define	OD_TABLE_BL_WEIGHT9	0x0078
#define	OD_TABLE_BL_WEIGHT10	0x0079
#define	OD_TABLE_BL_WEIGHT11	0x007A
#define	OD_TABLE_BL_WEIGHT12	0x007B
#define	OD_TABLE_BL_WEIGHT13	0x007C
#define	OD_TABLE_BL_WEIGHT14	0x007D

/* od sram */
#define	OD_TABLE_B_SRAM1_START	0x0100
#define	OD_TABLE_B_SRAM1_END	0x0220
#define	OD_TABLE_B_SRAM2_START	0x0221
#define	OD_TABLE_B_SRAM2_END	0x0330
#define	OD_TABLE_B_SRAM3_START	0x0331
#define	OD_TABLE_B_SRAM3_END	0x0440
#define	OD_TABLE_B_SRAM4_START	0x0441
#define	OD_TABLE_B_SRAM4_END	0x0540
#define	OD_TABLE_G_SRAM1_START	0x0541
#define	OD_TABLE_G_SRAM1_END	0x661
#define	OD_TABLE_G_SRAM2_START	0x0662
#define	OD_TABLE_G_SRAM2_END	0x0771
#define	OD_TABLE_G_SRAM3_START	0x0772
#define	OD_TABLE_G_SRAM3_END	0x0881
#define	OD_TABLE_G_SRAM4_START	0x0882
#define	OD_TABLE_G_SRAM4_END	0x0981
#define	OD_TABLE_R_SRAM1_START	0x0982
#define	OD_TABLE_R_SRAM1_END	0x0AA2
#define	OD_TABLE_R_SRAM2_START	0x0AA3
#define	OD_TABLE_R_SRAM2_END	0x0BB2
#define	OD_TABLE_R_SRAM3_START	0x0BB3
#define	OD_TABLE_R_SRAM3_END	0x0CC2
#define	OD_TABLE_R_SRAM4_START	0x0CC3
#define	OD_TABLE_R_SRAM4_END	0x0DC2

struct mtk_oddmr_sw_reg {
	unsigned int reg;
	unsigned int val;
	unsigned int mask;
};
struct mtk_oddmr_od_tuning_sram {
	unsigned int channel;
	unsigned int sram;
	unsigned int idx;
	unsigned int value;
};

int mtk_oddmr_od_tuning_read(struct mtk_ddp_comp *comp, unsigned int table_idx,
			struct mtk_oddmr_sw_reg *sw_reg, struct mtk_oddmr_od_param *pparam);
int mtk_oddmr_od_tuning_write(struct mtk_ddp_comp *comp, unsigned int table_idx,
			struct mtk_oddmr_sw_reg *sw_reg, struct mtk_oddmr_od_param *pparam);

#endif
