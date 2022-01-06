// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#include "mt65xx_lcm_list.h"
#include <lcm_drv.h>
#ifdef BUILD_LK
#include <platform/disp_drv_platform.h>
#else
#include <linux/delay.h>
/* #include <mach/mt_gpio.h> */
#endif
enum LCM_DSI_MODE_CON lcm_dsi_mode;

/* used to identify float ID PIN status */
#define LCD_HW_ID_STATUS_LOW      0
#define LCD_HW_ID_STATUS_HIGH     1
#define LCD_HW_ID_STATUS_FLOAT 0x02
#define LCD_HW_ID_STATUS_ERROR  0x03

struct LCM_DRIVER *lcm_driver_list[] = {
#if defined(NT36672AH_HDP_DSI_VDO_TCL_CSOT)
	&nt36672ah_hdp_dsi_vdo_tcl_csot_lcm_drv,
#endif
#if defined(ES6311_ANX6585_ZIGZAG_WXGA)
	&es6311_anx6585_zigzag_wxga_lcm_drv,
#endif
#if defined(NT36672AH_HDP_DSI_VDO_TCL_CSOT_FWVPLUS)
	&nt36672ah_hdp_dsi_vdo_tcl_csot_fwvplus_lcm_drv,
#endif
#if defined(OTM1284A_HD720_DSI_VDO_TM)
	&otm1284a_hd720_dsi_vdo_tm_lcm_drv,
#endif
#if defined(OTM1285A_HD720_DSI_VDO_TM)
	&otm1285a_hd720_dsi_vdo_tm_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_8163)
	&nt35595_fhd_dsi_cmd_truly_8163_lcm_drv,
#endif

#if defined(NT35523_WXGA_DSI_VDO_8163)
	&nt35523_wxga_dsi_vdo_8163_lcm_drv,
#endif

#if defined(EK79007_WSVGALNL_DSI_VDO)
	&ek79007_wsvgalnl_dsi_vdo_lcm_drv,
#endif

#if defined(S6E3FA2_FHD1080_DSI_VDO)
	&s6e3fa2_fhd1080_dsi_vdo_lcm_drv,
#endif

#if defined(OTM1283A_HD720_DSI_VDO_TM)
	&otm1283a_hd720_dsi_vdo_tm_lcm_drv,
#endif

#if defined(IT6151_LP079QX1_EDP_DSI_VIDEO)
	&it6151_lp079qx1_edp_dsi_video_lcm_drv,
#endif

#if defined(VVX10F008B00_WUXGA_DSI_VDO)
	&vvx10f008b00_wuxga_dsi_vdo_lcm_drv,
#endif

#if defined(KD070D5450NHA6_RGB_DPI)
	&kd070d5450nha6_rgb_dpi_lcm_drv,
#endif

#if defined(KR101IA2S_DSI_VDO)
	&kr101ia2s_dsi_vdo_lcm_drv,
#endif

#if defined(KR070IA4T_DSI_VDO)
	&kr070ia4t_dsi_vdo_lcm_drv,
#endif

#if defined(HX8394A_HD720_DSI_VDO_TIANMA_V2)
	&hx8394a_hd720_dsi_vdo_tianma_v2_lcm_drv,
#endif

#if defined(OTM1283A)
	&otm1283a_6589_hd_dsi,
#endif
#if defined(OTM1282A_HD720_DSI_VDO_60HZ)
	&otm1282a_hd720_dsi_vdo_60hz_lcm_drv,
#endif
#if defined(OTM8018B_DSI_VDO_TXD_FWVGA)
	&otm8018b_dsi_vdo_txd_fwvga_lcm_drv,
#endif

#if defined(TF070MC_RGB_V18_MT6571)
	&tf070mc_rgb_v18_mt6571_lcm_drv,
#endif

#if defined(ZS070IH5015B3H6_RGB_MT6571)
	&zs070ih5015b3h6_mt6571_lcm_drv,
#endif

#if defined(OTM1282A_HD720_DSI_VDO)
	&otm1282a_hd720_dsi_vdo_lcm_drv,
#endif

#if defined(R63311_FHD_DSI_VDO)
	&r63311_fhd_dsi_vedio_lcm_drv,
#endif

#if defined(R63315_FHD_DSI_VDO_TRULY)
	&r63315_fhd_dsi_vdo_truly_lcm_drv,
#endif

#if defined(NT35517_QHD_DSI_VDO)
	&nt35517_dsi_vdo_lcm_drv,
#endif

#if defined(ILI9806E_DSI_VDO_FWVGA)
	&ili9806e_dsi_vdo_fwvga_drv,
#endif

#if defined(LP079X01)
	&lp079x01_lcm_drv,
#endif

#if defined(HX8369)
	&hx8369_lcm_drv,
#endif

#if defined(HX8369_6575)
	&hx8369_6575_lcm_drv,
#endif

#if defined(BM8578)
	&bm8578_lcm_drv,
#endif

#if defined(NT35582_MCU)
	&nt35582_mcu_lcm_drv,
#endif

#if defined(NT35582_MCU_6575)
	&nt35582_mcu_6575_lcm_drv,
#endif

#if defined(NT35590_HD720_DSI_CMD_TRULY2)
	&nt35590_hd720_dsi_cmd_truly2_lcm_drv,
#endif

#if defined(NT35590_HD720_DSI_VDO_TRULY)
	&nt35590_hd720_dsi_vdo_truly_lcm_drv,
#endif

#if defined(SSD2075_HD720_DSI_VDO_TRULY)
	&ssd2075_hd720_dsi_vdo_truly_lcm_drv,
#endif

#if defined(NT35590_HD720_DSI_CMD)
	&nt35590_hd720_dsi_cmd_drv,
#endif

#if defined(NT35590_HD720_DSI_CMD_AUO)
	&nt35590_hd720_dsi_cmd_auo_lcm_drv,
#endif

#if defined(NT35590_HD720_DSI_CMD_AUO_WVGA)
	&nt35590_hd720_dsi_cmd_auo_wvga_lcm_drv,
#endif

#if defined(NT35590_HD720_DSI_CMD_AUO_QHD)
	&nt35590_hd720_dsi_cmd_auo_qhd_lcm_drv,
#endif

#if defined(NT35590_HD720_DSI_CMD_AUO_FWVGA)
	&nt35590_hd720_dsi_cmd_auo_fwvga_lcm_drv,
#endif

#if defined(NT35590_HD720_DSI_CMD_CMI)
	&nt35590_hd720_dsi_cmd_cmi_lcm_drv,
#endif

#if defined(NT35582_RGB_6575)
	&nt35582_rgb_6575_lcm_drv,
#endif

#if  defined(NT51012_HD720_DSI_VDO)
	&nt51012_hd720_dsi_vdo_lcm_drv,
#endif

#if defined(HX8369_RGB_6585_FPGA)
	&hx8369_rgb_6585_fpga_lcm_drv,
#endif

#if defined(HX8369_RGB_6572_FPGA)
	&hx8369_rgb_6572_fpga_lcm_drv,
#endif

#if defined(HX8369_MCU_6572)
	&hx8369_mcu_6572_lcm_drv,
#endif

#if defined(HX8369A_WVGA_DSI_CMD)
	&hx8369a_wvga_dsi_cmd_drv,
#endif

#if defined(HX8369A_WVGA_DSI_VDO)
	&hx8369a_wvga_dsi_vdo_drv,
#endif

#if defined(HX8357B)
	&hx8357b_lcm_drv,
#endif

#if defined(HX8357C_HVGA_DSI_CMD)
	&hx8357c_hvga_dsi_cmd_drv,
#endif

#if defined(R61408)
	&r61408_lcm_drv,
#endif

#if defined(R61408_WVGA_DSI_CMD)
	&r61408_wvga_dsi_cmd_drv,
#endif

#if defined(HX8369_DSI_VDO)
	&hx8369_dsi_vdo_lcm_drv,
#endif

#if defined(HX8369_DSI)
	&hx8369_dsi_lcm_drv,
#endif

#if defined(HX8369_6575_DSI)
	&hx8369_dsi_6575_lcm_drv,
#endif

#if defined(HX8369_6575_DSI_NFC_ZTE)
	&hx8369_dsi_6575_lcm_drv,
#endif

#if defined(HX8369_6575_DSI_HVGA)
	&hx8369_dsi_6575_hvga_lcm_drv,
#endif

#if defined(HX8369_6575_DSI_QVGA)
	&hx8369_dsi_6575_qvga_lcm_drv,
#endif

#if defined(HX8369_HVGA)
	&hx8369_hvga_lcm_drv,
#endif

#if defined(NT35510)
	&nt35510_lcm_drv,
#endif

#if defined(NT35510_RGB_6575)
	&nt35510_dpi_lcm_drv,
#endif

#if defined(NT35510_HVGA)
	&nt35510_hvga_lcm_drv,
#endif

#if defined(NT35510_QVGA)
	&nt35510_qvga_lcm_drv,
#endif

#if defined(NT35510_WVGA_DSI_CMD)
	&nt35510_wvga_dsi_cmd_drv,
#endif

#if defined(NT35510_6517)
	&nt35510_6517_lcm_drv,
#endif

#if defined(NT35510_DSI_CMD_6572)
	&nt35510_dsi_cmd_6572_drv,
#endif

#if defined(NT35510_DSI_CMD_6572_HVGA)
	&nt35510_dsi_cmd_6572_hvga_drv,
#endif

#if defined(NT35510_DSI_CMD_6572_FWVGA)
	&nt35510_dsi_cmd_6572_fwvga_drv,
#endif

#if defined(NT35510_DSI_CMD_6572_QVGA)
	&nt35510_dsi_cmd_6572_qvga_drv,
#endif

#if defined(NT35510_DSI_VDO_6572)
	&nt35510_dsi_vdo_6572_drv,
#endif

#if defined(NT35510_DPI_6572)
	&nt35510_dpi_6572_lcm_drv,
#endif

#if defined(NT35510_MCU_6572)
	&nt35510_mcu_6572_lcm_drv,
#endif

#if defined(ILI9481)
	&ili9481_lcm_drv,
#endif

#if defined(NT35582)
	&nt35582_lcm_drv,
#endif

#if defined(S6D0170)
	&s6d0170_lcm_drv,
#endif

#if defined(SPFD5461A)
	&spfd5461a_lcm_drv,
#endif

#if defined(TA7601)
	&ta7601_lcm_drv,
#endif

#if defined(TFT1P3037)
	&tft1p3037_lcm_drv,
#endif

#if defined(HA5266)
	&ha5266_lcm_drv,
#endif

#if defined(HSD070IDW1)
	&hsd070idw1_lcm_drv,
#endif

#if defined(HX8363_6575_DSI)
	&hx8363_6575_dsi_lcm_drv,
#endif

#if defined(HX8363_6575_DSI_HVGA)
	&hx8363_6575_dsi_hvga_lcm_drv,
#endif

#if defined(HX8363B_WVGA_DSI_CMD)
	&hx8363b_wvga_dsi_cmd_drv,
#endif

#if defined(LG4571)
	&lg4571_lcm_drv,
#endif

#if defined(LG4573B_WVGA_DSI_VDO_LH430MV1)
	&lg4573b_wvga_dsi_vdo_lh430mv1_drv,
#endif

#if defined(LVDS_WSVGA)
	&lvds_wsvga_lcm_drv,
#endif

#if defined(LVDS_WSVGA_TI)
	&lvds_wsvga_ti_lcm_drv,
#endif

#if defined(LVDS_WSVGA_TI_N)
	&lvds_wsvga_ti_n_lcm_drv,
#endif

#if defined(NT35565_3D)
	&nt35565_3d_lcm_drv,
#endif

#if defined(TM070DDH03)
	&tm070ddh03_lcm_drv,
#endif
#if defined(R63303_IDISPLAY)
	&r63303_idisplay_lcm_drv,
#endif

#if defined(HX8369B_DSI_VDO)
	&hx8369b_dsi_vdo_lcm_drv,
#endif

#if defined(HX8369B_WVGA_DSI_VDO)
	&hx8369b_wvga_dsi_vdo_drv,
#endif

#if defined(HX8369B_QHD_DSI_VDO)
	&hx8389b_qhd_dsi_vdo_drv,
#endif

#if defined(HX8389B_HD720_DSI_VDO)
	&hx8389b_hd720_dsi_vdo_drv,
#endif

#if defined(GN_SSD2825_SMD_S6E8AA)
	&gn_ssd2825_smd_s6e8aa,
#endif
#if defined(HX8369_TM_DSI)
	&hx8369_dsi_tm_lcm_drv,
#endif

#if defined(HX8369_BLD_DSI)
	&hx8369_dsi_bld_lcm_drv,
#endif

#if defined(HJ080IA)
	&hj080ia_lcm_drv,
#endif

#if defined(HJ101NA02A)
	&hj101na02a_lcm_drv,
#endif

#if defined(HJ101NA02A_8135)
	&hj101na02a_8135_lcm_drv,
#endif

#if defined(HSD070PFW3)
	&hsd070pfw3_lcm_drv,
#endif

#if defined(HSD070PFW3_8135)
	&hsd070pfw3_8135_lcm_drv,
#endif

#if defined(EJ101IA)
	&ej101ia_lcm_drv,
#endif

#if defined(SCF0700M48GGU02)
	&scf0700m48ggu02_lcm_drv,
#endif

#if defined(OTM1280A_HD720_DSI_CMD)
	&otm1280a_hd720_dsi_cmd_drv,
#endif

#if defined(OTM8018B_DSI_VDO)
	&otm8018b_dsi_vdo_lcm_drv,
#endif

#if defined(NT35512_DSI_VDO)
	&nt35512_dsi_vdo_lcm_drv,
#endif

#if defined(NT35512_WVGA_DSI_VDO_BOE)
	&nt35512_wvga_dsi_vdo_boe_drv,
#endif

#if defined(HX8392A_DSI_CMD)
	&hx8392a_dsi_cmd_lcm_drv,
#endif

#if defined(HX8392A_DSI_CMD_3LANE)
	&hx8392a_dsi_cmd_3lane_lcm_drv,
#endif

#if defined(HX8392A_DSI_CMD_WVGA)
	&hx8392a_dsi_cmd_wvga_lcm_drv,
#endif

#if defined(HX8392A_DSI_CMD_FWVGA)
	&hx8392a_dsi_cmd_fwvga_lcm_drv,
#endif

#if defined(HX8392A_DSI_CMD_QHD)
	&hx8392a_dsi_cmd_qhd_lcm_drv,
#endif

#if defined(HX8392A_DSI_VDO)
	&hx8392a_dsi_vdo_lcm_drv,
#endif

#if defined(HX8392A_DSI_VDO_2LANE)
	&hx8392a_dsi_vdo_2lane_lcm_drv,
#endif

#if defined(HX8392A_DSI_VDO_3LANE)
	&hx8392a_dsi_vdo_3lane_lcm_drv,
#endif

#if defined(NT35516_QHD_DSI_CMD_IPSBOE)
	&nt35516_qhd_dsi_cmd_ipsboe_lcm_drv,
#endif

#if defined(NT35516_QHD_DSI_CMD_IPSBOE_WVGA)
	&nt35516_qhd_dsi_cmd_ipsboe_wvga_lcm_drv,
#endif

#if defined(NT35516_QHD_DSI_CMD_IPSBOE_FWVGA)
	&nt35516_qhd_dsi_cmd_ipsboe_fwvga_lcm_drv,
#endif

#if defined(NT35516_QHD_DSI_CMD_IPS9K1431)
	&nt35516_qhd_dsi_cmd_ips9k1431_drv,
#endif

#if defined(NT35516_QHD_DSI_CMD_TFT9K1342)
	&nt35516_qhd_dsi_cmd_tft9k1342_drv,
#endif

#if defined(NT35516_QHD_DSI_VEDIO)
	&nt35516_qhd_rav4_lcm_drv,
#endif

#if defined(BP070WS1)
	&bp070ws1_lcm_drv,
#endif

#if defined(BP101WX1)
	&bp101wx1_lcm_drv,
#endif

#if defined(BP101WX1_N)
	&bp101wx1_n_lcm_drv,
#endif

#if defined(CM_N070ICE_DSI_VDO)
	&cm_n070ice_dsi_vdo_lcm_drv,
#endif

#if defined(CM_N070ICE_DSI_VDO_MT8135)
	&cm_n070ice_dsi_vdo_mt8135_lcm_drv,
#endif

#if defined(CM_OTC3108BH161_DSI_VDO)
	&cm_otc3108bhv161_dsi_vdo_lcm_drv,
#endif
#if defined(NT35510_FWVGA)
	&nt35510_fwvga_lcm_drv,
#endif

#if defined(R63311_FHD_DSI_VDO_SHARP)
	&r63311_fhd_dsi_vdo_sharp_lcm_drv,
#endif

#if defined(R81592_HVGA_DSI_CMD)
	&r81592_hvga_dsi_cmd_drv,
#endif

#if defined(RM68190_QHD_DSI_VDO)
	&rm68190_dsi_vdo_lcm_drv,
#endif

#if defined(NT35596_FHD_DSI_VDO_TRULY)
	&nt35596_fhd_dsi_vdo_truly_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_VDO_TRULY)
	&nt35595_fhd_dsi_vdo_truly_lcm_drv,
#endif

#if defined(R63319_WQHD_DSI_VDO_TRULY)
	&r63319_wqhd_dsi_vdo_truly_lcm_drv,
#endif


#if defined(NT35598_WQHD_DSI_VDO_TRULY)
	&nt35598_wqhd_dsi_vdo_truly_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_TPS65132)
	&nt35595_fhd_dsi_cmd_truly_tps65132_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_VDO_TRULY_TPS65132)
	&nt35595_fhd_dsi_vdo_truly_tps65132_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_TPS65132_720P)
	&nt35595_fhd_dsi_cmd_truly_tps65132_720p_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY)
	&nt35595_fhd_dsi_cmd_truly_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_NT50358)
	&nt35595_fhd_dsi_cmd_truly_nt50358_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_VDO_TRULY_NT50358)
	&nt35595_fhd_dsi_vdo_truly_nt50358_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_NT50358_720P)
	&nt35595_fhd_dsi_cmd_truly_nt50358_720p_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_NT50358_QHD)
	&nt35595_fhd_dsi_cmd_truly_nt50358_qhd_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_NT50358_FWVGA)
	&nt35595_fhd_dsi_cmd_truly_nt50358_fwvga_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_NT50358_WVGA)
	&nt35595_fhd_dsi_cmd_truly_nt50358_wvga_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_NT50358_6735)
	&nt35595_fhd_dsi_cmd_truly_nt50358_6735_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_NT50358_6735_720P)
	&nt35595_fhd_dsi_cmd_truly_nt50358_6735_720p_lcm_drv,
#endif

#if defined(NT35596_FHD_DSI_VDO_YASSY)
	&nt35596_fhd_dsi_vdo_yassy_lcm_drv,
#endif

#if defined(NT35596_HD720_DSI_VDO_TRULY_TPS65132)
	&nt35596_hd720_dsi_vdo_truly_tps65132_lcm_drv,
#endif

#if defined(AUO_B079XAT02_DSI_VDO)
	&auo_b079xat02_dsi_vdo_lcm_drv,
#endif

#if defined(OTM9608_WVGA_DSI_CMD)
	&otm9608_wvga_dsi_cmd_drv,
#endif

#if defined(OTM9608_FWVGA_DSI_CMD)
	&otm9608_fwvga_dsi_cmd_drv,
#endif

#if defined(OTM9608_QHD_DSI_CMD)
	&otm9608_qhd_dsi_cmd_drv,
#endif

#if defined(OTM9608_QHD_DSI_VDO)
	&otm9608_qhd_dsi_vdo_drv,
#endif

#if defined(OTM8009A_FWVGA_DSI_CMD_TIANMA)
	&otm8009a_fwvga_dsi_cmd_tianma_lcm_drv,
#endif

#if defined(OTM8009A_FWVGA_DSI_VDO_TIANMA)
	&otm8009a_fwvga_dsi_vdo_tianma_lcm_drv,
#endif

#if defined(HX8389B_QHD_DSI_VDO_TIANMA)
	&hx8389b_qhd_dsi_vdo_tianma_lcm_drv,
#endif
#if defined(HX8389B_QHD_DSI_VDO_TIANMA055XDHP)
	&hx8389b_qhd_dsi_vdo_tianma055xdhp_lcm_drv,
#endif

#if defined(CPT_CLAA101FP01_DSI_VDO)
	&cpt_claa101fp01_dsi_vdo_lcm_drv,
#endif

#if defined(CPT_CLAA101FP01_DSI_VDO_8163)
	&cpt_claa101fp01_dsi_vdo_8163_lcm_drv,
#endif

#if defined(IT6151_EDP_DSI_VIDEO_SHARP)
	&it6151_edp_dsi_video_sharp_lcm_drv,
#endif

#if defined(CPT_CLAP070WP03XG_SN65DSI83)
	&cpt_clap070wp03xg_sn65dsi83_lcm_drv,
#endif
#if defined(NT35520_HD720_DSI_CMD_TM)
	&nt35520_hd720_tm_lcm_drv,
#endif
#if defined(NT35520_HD720_DSI_CMD_BOE)
	&nt35520_hd720_boe_lcm_drv,
#endif
#if defined(NT35521_HD720_DSI_VDO_BOE)
	&nt35521_hd720_dsi_vdo_boe_lcm_drv,
#endif
#if defined(NT35521_HD720_DSI_VIDEO_TM)
	&nt35521_hd720_tm_lcm_drv,
#endif
#if defined(NT35521_HD_DSI_VDO_TRULY_RT5081)
	&nt35521_hd_dsi_vdo_truly_rt5081_lcm_drv,
#endif
#if defined(R69338_HD720_DSI_VDO_JDI_DW8755A)
	&r69338_hd720_dsi_vdo_jdi_dw8755a_drv,
#endif
#if defined(H070D_18DM)
	&h070d_18dm_lcm_drv,
#endif
#if defined(R69429_WUXGA_DSI_VDO)
	&r69429_wuxga_dsi_vdo_lcm_drv,
#endif

#if defined(HX8394D_HD720_DSI_VDO_TIANMA)
	&hx8394d_hd720_dsi_vdo_tianma_lcm_drv,
#endif

#if defined(HX8394A_HD720_DSI_VDO_TIANMA)
	&hx8394a_hd720_dsi_vdo_tianma_lcm_drv,
#endif

#if defined(R69429_WUXGA_DSI_CMD)
	&r69429_wuxga_dsi_cmd_lcm_drv,
#endif

#if defined(RM68210_HD720_DSI_UFOE_CMD)
	&rm68210_hd720_dsi_ufoe_cmd_lcm_drv,
#endif

#if defined(CPT_CLAP070WP03XG_LVDS)
	&cpt_clap070wp03xg_lvds_lcm_drv,
#endif

#if defined(OTM8018B_DSI_VDO_L72)
	&otm8018b_dsi_vdo_l72_lcm_drv,
#endif

#if defined(HX8369_DSI_CMD_6571)
	&hx8369_dsi_cmd_6571_lcm_drv,
#endif

#if defined(HX8369_DSI_VDO_6571)
	&hx8369_dsi_vdo_6571_lcm_drv,
#endif

#if defined(RX_498HX_615B_82)
	&RX_498HX_615B_82_lcm_drv,
#endif

#if defined(HX8369_DBI_6571)
	&hx8369_dbi_6571_lcm_drv,
#endif

#if defined(RX_498HX_615B)
	&RX_498HX_615B_lcm_drv,
#endif

#if defined(HX8369_DPI_6571)
	&hx8369_dpi_6571_lcm_drv,
#endif

#if defined(HX8389B_QHD_DSI_VDO_LGD)
	&hx8389b_qhd_dsi_vdo_lgd_lcm_drv,
#endif

#if defined(NT35510_DSI_CMD_6571)
	&nt35510_dsi_cmd_6571_lcm_drv,
#endif

#if defined(NT35510_DSI_CMD_6571_HVGA)
	&nt35510_dsi_cmd_6571_hvga_lcm_drv,
#endif

#if defined(NT35510_DSI_CMD_6571_QVGA)
	&nt35510_dsi_cmd_6571_qvga_lcm_drv,
#endif

#if defined(NT35510_DSI_VDO_6571)
	&nt35510_dsi_vdo_6571_lcm_drv,
#endif

#if defined(NT35510_DBI_6571)
	&nt35510_dbi_6571_lcm_drv,
#endif

#if defined(NT35510_DPI_6571)
	&nt35510_dpi_6571_lcm_drv,
#endif

#if defined(NT35590_DSI_CMD_6571_FWVGA)
	&nt35590_dsi_cmd_6571_fwvga_lcm_drv,
#endif

#if defined(NT35590_DSI_CMD_6571_QHD)
	&nt35590_dsi_cmd_6571_qhd_lcm_drv,
#endif

#if defined(NT35517_QHD_DSI_VIDEO)
	&nt35517_qhd_dsi_vdo_lcm_drv,
#endif

#if defined(IT6151_FHD_EDP_DSI_VIDEO_AUO)
	&it6151_fhd_edp_dsi_video_auo_lcm_drv,
#endif

#if defined(A080EAN01_DSI_VDO)
	&a080ean01_dsi_vdo_lcm_drv,
#endif

#if defined(IT6121_G156XW01V1_LVDS_VDO)
	&it6121_g156xw01v1_lvds_vdo_lcm_drv,
#endif

#if defined(ILI9806C_DSI_VDO_DJN_FWVGA)
	&ili9806c_dsi_vdo_djn_fwvga_lcm_drv,
#endif

#if defined(R69338_HD720_DSI_VDO_JDI)
	&r69338_hd720_dsi_vdo_jdi_drv,
#endif

#if defined(R69338_HD720_5IN_DSI_VDO_JDI_DW8768)
	&r69338_hd720_5in_dsi_vdo_jdi_dw8768_drv,
#endif

#if defined(DB7436_DSI_VDO_FWVGA)
	&db7436_dsi_vdo_fwvga_drv,
#endif

#if defined(R63417_FHD_DSI_CMD_TRULY_NT50358)
	&r63417_fhd_dsi_cmd_truly_nt50358_lcm_drv,
#endif

#if defined(R63417_FHD_DSI_CMD_TRULY_NT50358_720P)
	&r63417_fhd_dsi_cmd_truly_nt50358_720p_lcm_drv,
#endif

#if defined(R63417_FHD_DSI_CMD_TRULY_NT50358_QHD)
	&r63417_fhd_dsi_cmd_truly_nt50358_qhd_lcm_drv,
#endif

#if defined(R63417_FHD_DSI_VDO_TRULY_NT50358)
	&r63417_fhd_dsi_vdo_truly_nt50358_lcm_drv,
#endif

#if defined(R63419_WQHD_TRULY_PHANTOM_2K_CMD_OK)
	&r63419_wqhd_truly_phantom_cmd_lcm_drv,
#endif

#if defined(R63419_WQHD_TRULY_PHANTOM_2K_CMD_OK_MT6797)
	&r63419_wqhd_truly_phantom_cmd_lcm_drv,
#endif

#if defined(R63419_WQHD_TRULY_PHANTOM_2K_VDO_OK)
	&r63419_wqhd_truly_phantom_vdo_lcm_drv,
#endif

#if defined(R63419_WQHD_TRULY_PHANTOM_2K_VDO_OK_MT6797)
	&r63419_wqhd_truly_phantom_vdo_lcm_drv,
#endif

#if defined(R63419_FHD_TRULY_PHANTOM_2K_CMD_OK)
	&r63419_fhd_truly_phantom_lcm_drv,
#endif

#if defined(R63419_FHD_TRULY_PHANTOM_2K_CMD_OK_MT6797)
	&r63419_fhd_truly_phantom_lcm_drv,
#endif

#if defined(R63423_WQHD_TRULY_PHANTOM_2K_CMD_OK)
	&r63423_wqhd_truly_phantom_lcm_drv,
#endif

#if defined(NT35523_WXGA_DSI_VDO_BOE)
	&nt35523_wxga_dsi_vdo_boe_lcm_drv,
#endif

#if defined(NT35523_WSVGA_DSI_VDO_BOE)
	&nt35523_wsvga_dsi_vdo_boe_lcm_drv,
#endif

#if defined(EK79023_DSI_WSVGA_VDO)
	&ek79023_dsi_wsvga_vdo_lcm_drv,
#endif

#if defined(OTM9605A_QHD_DSI_VDO)
	&otm9605a_qhd_dsi_vdo_drv,
#endif

#if defined(OTM1906A_FHD_DSI_CMD_AUTO)
	&otm1906a_fhd_dsi_cmd_auto_lcm_drv,
#endif

#if defined(NT35532_FHD_DSI_VDO_SHARP)
	&nt35532_fhd_dsi_vdo_sharp_lcm_drv,
#endif

#if defined(CLAP070WP03XG_LVDS)
	&clap070wp03xg_lvds_lcm_drv,
#endif

#if defined(S6D7AA0_WXGA_DSI_VDO)
	&s6d7aa0_wxga_dsi_vdo_lcm_drv,
#endif

#if defined(SY20810800210132_WUXGA_DSI_VDO)
	&sy20810800210132_wuxga_dsi_vdo_lcm_drv,
#endif

#if defined(OTM1906B_FHD_DSI_CMD_JDI_TPS65132)
	&otm1906b_fhd_dsi_cmd_jdi_tps65132_lcm_drv,
#endif

#if defined(OTM1906B_FHD_DSI_CMD_JDI_TPS65132_MT6797)
	&otm1906b_fhd_dsi_cmd_jdi_tps65132_mt6797_lcm_drv,
#endif

#if defined(OTM1906B_FHD_DSI_VDO_JDI_TPS65132_MT6797)
	&otm1906b_fhd_dsi_vdo_jdi_tps65132_mt6797_lcm_drv,
#endif

#if defined(HX8394C_WXGA_DSI_VDO)
	&hx8394c_wxga_dsi_vdo_lcm_drv,
#endif

#if defined(IT6151_LP079QX1_EDP_DSI_VIDEO_8163EVB)
	&it6151_lp079qx1_edp_dsi_video_8163evb_lcm_drv,
#endif

#if defined(NT35510_DSI_CMD)
	&nt35510_dsi_cmd_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_CMD_TRULY_NT50358)
	&nt35695B_fhd_dsi_cmd_truly_nt50358_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_CMD_TRULY_NT50358)
	&nt35695_fhd_dsi_cmd_truly_nt50358_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_VDO_TRULY_NT50358)
	&nt35695_fhd_dsi_vdo_truly_nt50358_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_CMD_TRULY_NT50358_720P)
	&nt35695_fhd_dsi_cmd_truly_nt50358_720p_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_CMD_TRULY_NT50358_QHD)
	&nt35695_fhd_dsi_cmd_truly_nt50358_qhd_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_CMD_TRULY_RT4801)
	&nt35695_fhd_dsi_cmd_truly_rt4801_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_CMD_TRULY_RT5081)
	&nt35695_fhd_dsi_cmd_truly_rt5081_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_VDO_TRULY_RT5081)
	&nt35695_fhd_dsi_vdo_truly_rt5081_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_VDO_TRULY_RT5081_720P)
	&nt35695_fhd_dsi_vdo_truly_rt5081_720p_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_VDO_TRULY_RT5081_HDP)
	&nt35695_fhd_dsi_vdo_truly_rt5081_hdp_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_VDO_TRULY_RT5081_HDP_19_9)
	&nt35695_fhd_dsi_vdo_truly_rt5081_hdp_19_9_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_VDO_TRULY_RT5081_HDP_1560)
	&nt35695_fhd_dsi_vdo_truly_rt5081_hdp_1560_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_VDO_TRULY_RT5081_HDP_1680)
	&nt35695_fhd_dsi_vdo_truly_rt5081_hdp_1680_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_VDO_TRULY_RT5081_HDP_20_9)
	&nt35695_fhd_dsi_vdo_truly_rt5081_hdp_20_9_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_VDO_TRULY_RT5081_720P)
	&nt35695_fhd_dsi_vdo_truly_rt5081_720p_lcm_drv,
#endif
#if defined(NT35695_FHD_DSI_VDO_TRULY_RT5081_QHD)
	&nt35695_fhd_dsi_vdo_truly_rt5081_qhd_lcm_drv,
#endif
#if defined(NT35695B_FHD_DSI_CMD_TRULY_RT5081)
	&nt35695B_fhd_dsi_cmd_truly_rt5081_lcm_drv,
#endif
#if defined(NT35695B_FHD_DSI_CMD_TRULY_RT5081_720P)
	&nt35695B_fhd_dsi_cmd_truly_rt5081_720p_lcm_drv,
#endif
#if defined(NT35695B_FHD_DSI_CMD_TRULY_RT5081_QHD)
	&nt35695B_fhd_dsi_cmd_truly_rt5081_qhd_lcm_drv,
#endif
#if defined(RM69032_DSI_CMD)
	&rm69032_dsi_cmd_lcm_drv,
#endif

#if defined(ST7789H2_DBI)
	&st7789h2_dbi_lcm_drv,
#endif

#if defined(CM_N070ICE_DSI_VDO_MT8173)
	&cm_n070ice_dsi_vdo_mt8173_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_NT50358_EXTERN)
	&nt35595_fhd_dsi_cmd_truly_nt50358_extern_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_NT50358_2TH)
	&nt35595_fhd_dsi_cmd_truly_nt50358_2th_lcm_drv,
#endif

#if defined(R69429_WQXGA_DSI_VDO)
	&r69429_wqxga_dsi_vdo_lcm_drv,
#endif

#if defined(HX8394C_WXGA_DSI_VDO)
	&hx8394c_wxga_dsi_vdo_lcm_drv,
#endif

#if defined(NT35595_TRULY_FHD_DSI_VDO)
	&nt35595_truly_fhd_dsi_vdo_lcm_drv,
#endif

#if defined(NT36850_WQHD_DSI_2K_CMD)
	&nt36850_wqhd_dsi_2k_cmd_lcm_drv,
#endif

#if defined(S6E3HA3_WQHD_2K_CMD)
	&s6e3ha3_wqhd_2k_cmd_lcm_drv,
#endif

#if defined(S6E3FA3_FHD_CMD)
	&s6e3fa3_fhd_cmd_lcm_drv,
#endif

#if defined(NT35595_FHD_DSI_CMD_TRULY_NT50358_720P_EXTERN)
	&nt35595_fhd_dsi_cmd_truly_nt50358_720p_extern_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_CMD_AUO_NT50358_LANESWAP)
	&nt35695_fhd_dsi_cmd_auo_nt50358_laneswap_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_VDO_AUO_NT50358_LANESWAP)
	&nt35695_fhd_dsi_vdo_auo_nt50358_laneswap_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_CMD_AUO_NT50358_LANESWAP_MT6799)
	&nt35695_fhd_dsi_cmd_auo_nt50358_laneswap_mt6799_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_VDO_AUO_NT50358_LANESWAP_MT6799)
	&nt35695_fhd_dsi_vdo_auo_nt50358_laneswap_mt6799_lcm_drv,
#endif
#if defined(NT35695B_FHD_DSI_CMD_AUO_RT4801)
	&nt35695B_fhd_dsi_cmd_auo_rt4801_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_CMD_AUO_NT50358_HDP)
	&nt35695B_fhd_dsi_cmd_auo_nt50358_hdp_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_CMD_AUO_RT5081)
	&nt35695B_fhd_dsi_cmd_auo_rt5081_lcm_drv,
#endif
#if defined(NT35695B_FHD_DSI_CMD_AUO_RT5081_720P)
	&nt35695B_fhd_dsi_cmd_auo_rt5081_720p_lcm_drv,
#endif
#if defined(NT35695B_FHD_DSI_CMD_AUO_RT5081_QHD)
	&nt35695B_fhd_dsi_cmd_auo_rt5081_qhd_lcm_drv,
#endif
#if defined(NT35695B_FHD_DSI_CMD_AUO_RT5081_HDP)
	&nt35695B_fhd_dsi_cmd_auo_rt5081_hdp_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_CMD_AUO_RT5081_HDP_19_9)
	&nt35695B_fhd_dsi_cmd_auo_rt5081_hdp_19_9_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_VDO_AUO_RT5081)
	&nt35695B_fhd_dsi_vdo_auo_rt5081_lcm_drv,
#endif
#if defined(NT35695B_FHD_DSI_VDO_AUO_RT5081_720P)
	&nt35695B_fhd_dsi_vdo_auo_rt5081_720p_lcm_drv,
#endif
#if defined(NT35695B_FHD_DSI_VDO_AUO_RT5081_QHD)
	&nt35695B_fhd_dsi_vdo_auo_rt5081_qhd_lcm_drv,
#endif
#if defined(NT35695B_FHD_DSI_VDO_AUO_RT5081_HDP)
	&nt35695B_fhd_dsi_vdo_auo_rt5081_hdp_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_VDO_AUO_RT5081_HDP_19_9)
	&nt35695B_fhd_dsi_vdo_auo_rt5081_hdp_19_9_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_VDO_AUO_RT5081_HDP_1560)
	&nt35695B_fhd_dsi_vdo_auo_rt5081_hdp_1560_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_VDO_AUO_RT5081_HDP_1680)
	&nt35695B_fhd_dsi_vdo_auo_rt5081_hdp_1680_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_VDO_AUO_RT5081_HDP_20_9)
	&nt35695B_fhd_dsi_vdo_auo_rt5081_hdp_20_9_lcm_drv,
#endif

#if defined(CLAA101FP01_DSI_VDO)
	&claa101fp01_dsi_vdo_lcm_drv,
#endif

#if defined(R61322_FHD_DSI_VDO_SHARP_LFR)
	&r61322_fhd_dsi_vdo_sharp_lfr_lcm_drv,
#endif

#if defined(S6E3HA3_WQHD_2K_CMD_LANESWAP)
	&s6e3ha3_wqhd_2k_cmd_laneswap_drv,
#endif

#if defined(NT36380_WQHD_VDO_OK)
	&nt36380_wqhd_vdo_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_CMD_AUO_NT50358)
	&nt35695B_fhd_dsi_cmd_auo_nt50358_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_VDO_AUO_NT50358)
	&nt35695B_fhd_dsi_vdo_auo_nt50358_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_CMD_AUO_NT50358_720P)
	&nt35695B_fhd_dsi_cmd_auo_nt50358_720p_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_CMD_AUO_NT50358_QHD)
	&nt35695B_fhd_dsi_cmd_auo_nt50358_qhd_lcm_drv,
#endif

#if defined(ILI9881C_HDP_DSI_VDO_ILITEK_RT5081)
	&ili9881c_hdp_dsi_vdo_ilitek_rt5081_lcm_drv,
#endif

#if defined(NT51021_WUXGA_DSI_VDO_MT8173)
	&nt51021_wuxga_dsi_vdo_mt8173_lcm_drv,
#endif
#if defined(ILI9881C_HDP_DSI_VDO_ILITEK_RT5081_EBBG)
	&ili9881c_hdp_dsi_vdo_ilitek_rt5081_lcm_drv_ebbg,
#endif
#if defined(HX8394F_HD720_DSI_VDO_TIANMA)
	&hx8394f_hd720_dsi_vdo_tianma_lcm_drv,
#endif

#if defined(NT36672_FHDP_DSI_VDO_AUO)
	&nt36672_fhdp_dsi_vdo_auo_lcm_drv,
#endif
#ifndef CONFIG_MACH_MT6761
#if defined(NT36672_FHDP_DSI_VDO_AUO_LANESWAP)
	&nt36672_fhdp_dsi_vdo_auo_laneswap_lcm_drv,
#endif
#if defined(NT35521_HD_DSI_VDO_TRULY_NT50358)
	&nt35521_hd_dsi_vdo_truly_nt50358_lcm_drv,
#endif
#endif

#if defined(ILI9881H_HDP_DSI_VDO_ILITEK_RT5081_19_9)
	&ili9881h_hdp_dsi_vdo_ilitek_rt5081_19_9_lcm_drv,
#endif

#if defined(ILI9881H_HDP_DSI_VDO_ILITEK_RT5081_19_9_90HZ)
	&ili9881h_hdp_dsi_vdo_ilitek_rt5081_19_9_90hz_lcm_drv,
#endif

#if defined(HX83112B_FHDP_DSI_CMD_AUO_RT4801)
	&hx83112b_fhdp_dsi_cmd_auo_rt4801_lcm_drv,
#endif

#if defined(HX83112B_FHDP_DSI_CMD_FHD_AUO_RT4801)
	&hx83112b_fhdp_dsi_cmd_fhd_auo_rt4801_lcm_drv,
#endif

#if defined(FT8201_WXGA_VDO_INCELL_BOE)
	&ft8201_wxga_vdo_incell_boe_lcm_drv,
#endif

#if defined(NT51021H_WUXGA_VDO_ONCELL_BOE)
	&nt51021h_wuxga_vdo_oncell_boe_lcm_drv,
#endif

#if defined(HX83112B_FHDP_DSI_VDO_AUO_RT4801)
	&hx83112b_fhdp_dsi_vdo_auo_rt4801_lcm_drv,
#endif

#if defined(HX83112B_FHDP_DSI_CMD_AUO_RT5081)
	&hx83112b_fhdp_dsi_cmd_auo_rt5081_lcm_drv,
#endif

#if defined(HX83112B_FHDP_DSI_VDO_AUO_RT5081)
	&hx83112b_fhdp_dsi_vdo_auo_rt5081_lcm_drv,
#endif

#if defined(HX83112B_FHDP_DSI_CMD_AUO_RT5081_HDP)
	&hx83112b_fhdp_dsi_cmd_auo_rt5081_hdp_lcm_drv,
#endif

#if defined(HX83112B_FHDP_DSI_VDO_AUO_RT5081_HDP)
	&hx83112b_fhdp_dsi_vdo_auo_rt5081_hdp_lcm_drv,
#endif

#if defined(SOFEG01_FHDPLUS_DSI_CMD_SAMSUNG)
	&sofeg01_fhdplus_dsi_cmd_samsung_lcm_drv,
#endif

#if defined(NT36672C_FHDP_DSI_VDO_AUO_CPHY_90HZ_TIANMA)
	&nt36672c_fhdp_dsi_vdo_auo_cphy_90hz_tianma_lcm_drv,
#endif

#if defined(NT36672C_FHDP_DSI_VDO_60HZ_WO_DSC_SHENCHAO)
	&nt36672c_fhdp_dsi_vdo_60hz_wo_dsc_shenchao_lcm_drv,
#endif

#if defined(OPPO_TIANMA_TD4310_FHDP_DSI_VDO_RT5081)
	&oppo_tianma_td4310_fhdp_dsi_vdo_rt5081_lcm_drv,
#endif

#if defined(NT36672C_FHDP_DSI_VDO_60HZ_SHENCHAO)
	&nt36672c_fhdp_dsi_vdo_60hz_shenchao_lcm_drv,
#endif

#if defined(NT36672C_FHDP_DSI_VDO_90HZ_SHENCHAO)
	&nt36672c_fhdp_dsi_vdo_90hz_shenchao_lcm_drv,
#endif

#if defined(NT36672C_FHDP_DSI_VDO_90HZ_SHENCHAO_6382)
	&nt36672c_fhdp_dsi_vdo_90hz_shenchao_6382_lcm_drv,
#endif

#if defined(TD4320_FHDP_DSI_VDO_AUO_RT5081)
	&td4320_fhdp_dsi_vdo_auo_rt5081_lcm_drv,
#endif

/*6768 OTM*/
#if defined(OTM1911A_FHDP_DSI_VDO_TRULY_RT5081)
	&otm1911a_fhdp_dsi_vdo_truly_rt5081_lcm_drv,
#endif

#if defined(OTM1911A_FHDP_DSI_VDO_TRULY_RT5081_HDP)
	&otm1911a_fhdp_dsi_vdo_truly_rt5081_hdp_lcm_drv,
#endif
};

unsigned char lcm_name_list[][128] = {
#if defined(HX8392A_DSI_CMD)
	"hx8392a_dsi_cmd",
#endif

#if defined(S6E3HA3_WQHD_2K_CMD)
	"s6e3ha3_wqhd_2k_cmd",
#endif

#if defined(HX8392A_DSI_VDO)
	"hx8392a_vdo_cmd",
#endif

#if defined(HX8392A_DSI_CMD_FWVGA)
	"hx8392a_dsi_cmd_fwvga",
#endif

#if defined(OTM9608_QHD_DSI_CMD)
	"otm9608a_qhd_dsi_cmd",
#endif

#if defined(OTM9608_QHD_DSI_VDO)
	"otm9608a_qhd_dsi_vdo",
#endif

#if defined(R63417_FHD_DSI_CMD_TRULY_NT50358)
	"r63417_fhd_dsi_cmd_truly_nt50358_drv",
#endif

#if defined(R63417_FHD_DSI_CMD_TRULY_NT50358_QHD)
	"r63417_fhd_dsi_cmd_truly_nt50358_qhd_drv",
#endif

#if defined(R63417_FHD_DSI_VDO_TRULY_NT50358)
	"r63417_fhd_dsi_vdo_truly_nt50358_drv",
#endif

#if defined(R63419_WQHD_TRULY_PHANTOM_2K_CMD_OK)
	"r63419_wqhd_truly_phantom_2k_cmd_ok",
#endif

#if defined(NT35695_FHD_DSI_CMD_TRULY_NT50358)
	"nt35695_fhd_dsi_cmd_truly_nt50358_drv",
#endif

#if defined(S6E3HA3_WQHD_2K_CMD_LANESWAP)
	"s6e3ha3_wqhd_2k_cmd_laneswap_drv",
#endif

#if defined(NT36380_WQHD_VDO_OK)
	"nt36380_wqhd_vdo_lcm_drv",
#endif
#if defined(NT35521_HD_DSI_VDO_TRULY_RT5081)
	"nt35521_hd_dsi_vdo_truly_rt5081_drv",
#endif

#if defined(ILI9881C_HDP_DSI_VDO_ILITEK_RT5081)
	"ili9881c_hdp_dsi_vdo_ilitek_rt5081_drv",
#endif

#if defined(NT35695B_FHD_DSI_VDO_AUO_RT5081_HDP)
	"nt35695B_fhd_dsi_vdo_auo_rt5081_hdp_drv",
#endif

#if defined(NT35695B_FHD_DSI_CMD_TRULY_RT5081_720P)
	"nt35695B_fhd_dsi_cmd_truly_rt5081_720p_lcm_drv",
#endif

#if defined(OPPO_TIANMA_TD4310_FHDP_DSI_VDO_RT5081)
	"oppo_tianma_td4310_fhdp_dsi_vdo_rt5081_drv",
#endif
};

#define LCM_COMPILE_ASSERT(condition) \
	LCM_COMPILE_ASSERT_X(condition, __LINE__)
#define LCM_COMPILE_ASSERT_X(condition, line) \
	LCM_COMPILE_ASSERT_XX(condition, line)
#define LCM_COMPILE_ASSERT_XX(condition, line) \
	char assertion_failed_at_line_##line[(condition) ? 1 : -1]

unsigned int lcm_count =
	sizeof(lcm_driver_list) / sizeof(struct LCM_DRIVER *);
LCM_COMPILE_ASSERT(sizeof(lcm_driver_list) / sizeof(struct LCM_DRIVER *) != 0);
#if defined(NT35520_HD720_DSI_CMD_TM) | \
	defined(NT35520_HD720_DSI_CMD_BOE) | \
	defined(NT35521_HD720_DSI_VDO_BOE) | \
	defined(NT35521_HD720_DSI_VIDEO_TM)
static unsigned char lcd_id_pins_value = 0xFF;

/*
 * Function:    which_lcd_module_triple
 * Description: read LCD ID PIN status,could identify three status:highlowfloat
 * Input:       none
 * Output:      none
 * Return:      LCD ID1|ID0 value
 * Others:
 */
unsigned char which_lcd_module_triple(void)
{
	unsigned char  high_read0 = 0;
	unsigned char  low_read0 = 0;
	unsigned char  high_read1 = 0;
	unsigned char  low_read1 = 0;
	unsigned char  lcd_id0 = 0;
	unsigned char  lcd_id1 = 0;
	unsigned char  lcd_id = 0;
	/*Solve Coverity scan warning : check return value*/
	unsigned int ret = 0;

	/*only recognise once*/
	if (lcd_id_pins_value != 0xFF)
		return lcd_id_pins_value;

	/*Solve Coverity scan warning : check return value*/
	ret = mt_set_gpio_mode(GPIO_DISP_ID0_PIN, GPIO_MODE_00);
	if (ret != 0)
		pr_debug("[LCM]ID0 mt_set_gpio_mode fail\n");

	ret = mt_set_gpio_dir(GPIO_DISP_ID0_PIN, GPIO_DIR_IN);
	if (ret != 0)
		pr_debug("[LCM]ID0 mt_set_gpio_dir fail\n");

	ret = mt_set_gpio_pull_enable(GPIO_DISP_ID0_PIN, GPIO_PULL_ENABLE);
	if (ret != 0)
		pr_debug("[LCM]ID0 mt_set_gpio_pull_enable fail\n");

	ret = mt_set_gpio_mode(GPIO_DISP_ID1_PIN, GPIO_MODE_00);
	if (ret != 0)
		pr_debug("[LCM]ID1 mt_set_gpio_mode fail\n");

	ret = mt_set_gpio_dir(GPIO_DISP_ID1_PIN, GPIO_DIR_IN);
	if (ret != 0)
		pr_debug("[LCM]ID1 mt_set_gpio_dir fail\n");

	ret = mt_set_gpio_pull_enable(GPIO_DISP_ID1_PIN, GPIO_PULL_ENABLE);
	if (ret != 0)
		pr_debug("[LCM]ID1 mt_set_gpio_pull_enable fail\n");

	/*pull down ID0 ID1 PIN*/
	ret = mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN, GPIO_PULL_DOWN);
	if (ret != 0)
		pr_debug("[LCM]ID0 mt_set_gpio_pull_select->Down fail\n");

	ret = mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN, GPIO_PULL_DOWN);
	if (ret != 0)
		pr_debug("[LCM]ID1 mt_set_gpio_pull_select->Down fail\n");

	/* delay 100ms , for discharging capacitance*/
	mdelay(100);
	/* get ID0 ID1 status*/
	low_read0 = mt_get_gpio_in(GPIO_DISP_ID0_PIN);
	low_read1 = mt_get_gpio_in(GPIO_DISP_ID1_PIN);
	/* pull up ID0 ID1 PIN */
	ret = mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN, GPIO_PULL_UP);
	if (ret != 0)
		pr_debug("[LCM]ID0 mt_set_gpio_pull_select->UP fail\n");

	ret = mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN, GPIO_PULL_UP);
	if (ret != 0)
		pr_debug("[LCM]ID1 mt_set_gpio_pull_select->UP fail\n");

	/* delay 100ms , for charging capacitance */
	mdelay(100);
	/* get ID0 ID1 status */
	high_read0 = mt_get_gpio_in(GPIO_DISP_ID0_PIN);
	high_read1 = mt_get_gpio_in(GPIO_DISP_ID1_PIN);

	if (low_read0 != high_read0) {
		/*float status , pull down ID0 ,to prevent electric leakage*/
		ret = mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN,
			GPIO_PULL_DOWN);
		if (ret != 0)
			pr_debug("[LCM]ID0 mt_set_gpio_pull_select->Down fail\n");

		lcd_id0 = LCD_HW_ID_STATUS_FLOAT;
	} else if ((low_read0 == LCD_HW_ID_STATUS_LOW) &&
		(high_read0 == LCD_HW_ID_STATUS_LOW)) {
		/*low status , pull down ID0 ,to prevent electric leakage*/
		ret = mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN,
			GPIO_PULL_DOWN);
		if (ret != 0)
			pr_debug("[LCM]ID0 mt_set_gpio_pull_select->Down fail\n");

		lcd_id0 = LCD_HW_ID_STATUS_LOW;
	} else if ((low_read0 == LCD_HW_ID_STATUS_HIGH) &&
		(high_read0 == LCD_HW_ID_STATUS_HIGH)) {
		/*high status , pull up ID0 ,to prevent electric leakage*/
		ret = mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN, GPIO_PULL_UP);
		if (ret != 0)
			pr_debug("[LCM]ID0 mt_set_gpio_pull_select->UP fail\n");

		lcd_id0 = LCD_HW_ID_STATUS_HIGH;
	} else {
		pr_debug("[LCM] Read LCD_id0 error\n");
		ret = mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN,
			GPIO_PULL_DISABLE);
		if (ret != 0)
			pr_debug("[KERNEL/LCM]ID0 mt_set_gpio_pull_select->Disable fail\n");

		lcd_id0 = LCD_HW_ID_STATUS_ERROR;
	}


	if (low_read1 != high_read1) {
		/*float status , pull down ID1 ,to prevent electric leakage*/
		ret = mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN,
			GPIO_PULL_DOWN);
		if (ret != 0)
			pr_debug("[LCM]ID1 mt_set_gpio_pull_select->Down fail\n");

		lcd_id1 = LCD_HW_ID_STATUS_FLOAT;
	} else if ((low_read1 == LCD_HW_ID_STATUS_LOW) &&
		(high_read1 == LCD_HW_ID_STATUS_LOW)) {
		/*low status , pull down ID1 ,to prevent electric leakage*/
		ret = mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN,
			GPIO_PULL_DOWN);
		if (ret != 0)
			pr_debug("[LCM]ID1 mt_set_gpio_pull_select->Down fail\n");

		lcd_id1 = LCD_HW_ID_STATUS_LOW;
	} else if ((low_read1 == LCD_HW_ID_STATUS_HIGH) &&
		(high_read1 == LCD_HW_ID_STATUS_HIGH)) {
		/*high status , pull up ID1 ,to prevent electric leakage*/
		ret = mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN, GPIO_PULL_UP);
		if (ret != 0)
			pr_debug("[LCM]ID1 mt_set_gpio_pull_select->UP fail\n");

		lcd_id1 = LCD_HW_ID_STATUS_HIGH;
	} else {

		pr_debug("[LCM] Read LCD_id1 error\n");
		ret = mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN,
			GPIO_PULL_DISABLE);
		if (ret != 0)
			pr_debug("[KERNEL/LCM]ID1 mt_set_gpio_pull_select->Disable fail\n");

		lcd_id1 = LCD_HW_ID_STATUS_ERROR;
	}
#ifdef BUILD_LK
	dprintf(CRITICAL, "%s,lcd_id0:%d\n", __func__, lcd_id0);
	dprintf(CRITICAL, "%s,lcd_id1:%d\n", __func__, lcd_id1);
#else
	pr_debug("[LCM]%s,lcd_id0:%d\n", __func__, lcd_id0);
	pr_debug("[LCM]%s,lcd_id1:%d\n", __func__, lcd_id1);
#endif
	lcd_id =  lcd_id0 | (lcd_id1 << 2);

#ifdef BUILD_LK
	dprintf(CRITICAL, "%s,lcd_id:%d\n", __func__, lcd_id);
#else
	pr_debug("[LCM]%s,lcd_id:%d\n", __func__, lcd_id);
#endif

	lcd_id_pins_value = lcd_id;
	return lcd_id;
}
#endif
