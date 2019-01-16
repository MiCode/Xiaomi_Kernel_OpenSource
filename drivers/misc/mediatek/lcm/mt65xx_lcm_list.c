#include <lcm_drv.h>
#ifdef BUILD_LK
#include <platform/disp_drv_platform.h>
#else
#include <linux/delay.h>
#include <mach/mt_gpio.h>
#endif
#include <cust_gpio_usage.h>
//used to identify float ID PIN status
#define LCD_HW_ID_STATUS_LOW      0
#define LCD_HW_ID_STATUS_HIGH     1
#define LCD_HW_ID_STATUS_FLOAT 0x02
#define LCD_HW_ID_STATUS_ERROR  0x03

#ifdef BUILD_LK
#define LCD_DEBUG(fmt)  dprintf(CRITICAL,fmt)
#else
#define LCD_DEBUG(fmt)  printk(fmt)
#endif
extern LCM_DRIVER otm1282a_hd720_dsi_vdo_60hz_lcm_drv;
extern LCM_DRIVER otm1282a_hd720_dsi_vdo_lcm_drv;
extern LCM_DRIVER vvx10f008b00_wuxga_dsi_vdo_lcm_drv;
extern LCM_DRIVER r63319_wqhd_dsi_vdo_truly_lcm_drv;
extern LCM_DRIVER nt35598_wqhd_dsi_vdo_truly_lcm_drv;
extern LCM_DRIVER lp079x01_lcm_drv;
extern LCM_DRIVER hx8369_lcm_drv;
extern LCM_DRIVER hx8369_6575_lcm_drv;
extern LCM_DRIVER hx8363_6575_dsi_lcm_drv;
extern LCM_DRIVER hx8363_6575_dsi_hvga_lcm_drv;
extern LCM_DRIVER hx8363_6575_dsi_qvga_lcm_drv;
extern LCM_DRIVER hx8363b_wvga_dsi_cmd_drv;
extern LCM_DRIVER bm8578_lcm_drv;
extern LCM_DRIVER nt35582_mcu_lcm_drv;
extern LCM_DRIVER nt35582_mcu_6575_lcm_drv;
extern LCM_DRIVER nt35582_rgb_6575_lcm_drv;
extern LCM_DRIVER hx8357b_lcm_drv;
extern LCM_DRIVER hx8357c_hvga_dsi_cmd_drv;
extern LCM_DRIVER hx8369_dsi_lcm_drv;
extern LCM_DRIVER hx8369_dsi_6575_lcm_drv;
extern LCM_DRIVER hx8369_dsi_6575_hvga_lcm_drv;
extern LCM_DRIVER hx8369_dsi_6575_qvga_lcm_drv;
extern LCM_DRIVER hx8369_dsi_vdo_lcm_drv;
extern LCM_DRIVER hx8369b_dsi_vdo_lcm_drv;
extern LCM_DRIVER hx8369b_wvga_dsi_vdo_drv;
extern LCM_DRIVER hx8389b_qhd_dsi_vdo_drv;
extern LCM_DRIVER hx8369_hvga_lcm_drv;
extern LCM_DRIVER ili9481_lcm_drv;
extern LCM_DRIVER nt35582_lcm_drv;
extern LCM_DRIVER s6d0170_lcm_drv;
extern LCM_DRIVER spfd5461a_lcm_drv;
extern LCM_DRIVER ta7601_lcm_drv;
extern LCM_DRIVER tft1p3037_lcm_drv;
extern LCM_DRIVER ha5266_lcm_drv;
extern LCM_DRIVER hsd070idw1_lcm_drv;
extern LCM_DRIVER lg4571_lcm_drv;
extern LCM_DRIVER lg4573b_wvga_dsi_vdo_lh430mv1_drv;
extern LCM_DRIVER lvds_wsvga_lcm_drv;
extern LCM_DRIVER lvds_wsvga_ti_lcm_drv;
extern LCM_DRIVER lvds_wsvga_ti_n_lcm_drv;
extern LCM_DRIVER nt35565_3d_lcm_drv;
extern LCM_DRIVER tm070ddh03_lcm_drv;
extern LCM_DRIVER r61408_lcm_drv;
extern LCM_DRIVER r61408_wvga_dsi_cmd_drv;
extern LCM_DRIVER nt35510_lcm_drv;
extern LCM_DRIVER nt35510_dpi_lcm_drv;
extern LCM_DRIVER nt35510_hvga_lcm_drv;
extern LCM_DRIVER nt35510_qvga_lcm_drv;
extern LCM_DRIVER nt35510_wvga_dsi_cmd_drv;
extern LCM_DRIVER nt35510_6517_lcm_drv;
extern LCM_DRIVER nt35510_dsi_cmd_6572_drv;
extern LCM_DRIVER nt35510_dsi_cmd_6572_hvga_drv;
extern LCM_DRIVER nt35510_dsi_cmd_6572_fwvga_drv;
extern LCM_DRIVER nt35510_dsi_cmd_6572_qvga_drv;
extern LCM_DRIVER nt35510_dsi_vdo_6572_drv;
extern LCM_DRIVER nt35510_dpi_6572_lcm_drv;
extern LCM_DRIVER nt35510_mcu_6572_lcm_drv;
extern LCM_DRIVER nt51012_hd720_dsi_vdo_lcm_drv;
extern LCM_DRIVER r63303_idisplay_lcm_drv;
extern LCM_DRIVER hj080ia_lcm_drv;
extern LCM_DRIVER hj101na02a_lcm_drv;
extern LCM_DRIVER hj101na02a_8135_lcm_drv;
extern LCM_DRIVER hsd070pfw3_lcm_drv;
extern LCM_DRIVER hsd070pfw3_8135_lcm_drv;
extern LCM_DRIVER cm_n070ice_dsi_vdo_lcm_drv;
extern LCM_DRIVER ej101ia_lcm_drv;
extern LCM_DRIVER scf0700m48ggu02_lcm_drv;
extern LCM_DRIVER nt35510_fwvga_lcm_drv;
#if defined(GN_SSD2825_SMD_S6E8AA)
extern LCM_DRIVER gn_ssd2825_smd_s6e8aa;
#endif
extern LCM_DRIVER nt35517_dsi_vdo_lcm_drv;
extern LCM_DRIVER hx8369_dsi_bld_lcm_drv;
extern LCM_DRIVER hx8369_dsi_tm_lcm_drv;
extern LCM_DRIVER otm1280a_hd720_dsi_cmd_drv;	 
extern LCM_DRIVER otm8018b_dsi_vdo_lcm_drv;	 
extern LCM_DRIVER otm8018b_dsi_vdo_txd_fwvga_lcm_drv;
extern LCM_DRIVER nt35512_dsi_vdo_lcm_drv;
extern LCM_DRIVER nt35512_wvga_dsi_vdo_boe_drv;
extern LCM_DRIVER hx8369_rgb_6585_fpga_lcm_drv;
extern LCM_DRIVER hx8369_rgb_6572_fpga_lcm_drv;
extern LCM_DRIVER hx8369_mcu_6572_lcm_drv;
extern LCM_DRIVER hx8369a_wvga_dsi_cmd_drv;
extern LCM_DRIVER hx8369a_wvga_dsi_vdo_drv;
extern LCM_DRIVER hx8392a_dsi_cmd_lcm_drv;
extern LCM_DRIVER hx8392a_dsi_vdo_lcm_drv;
extern LCM_DRIVER nt35590_hd720_dsi_vdo_truly_lcm_drv;
extern LCM_DRIVER ssd2075_hd720_dsi_vdo_truly_lcm_drv;
extern LCM_DRIVER nt35590_hd720_dsi_cmd_drv;
extern LCM_DRIVER nt35590_hd720_dsi_cmd_auo_lcm_drv;
extern LCM_DRIVER nt35590_hd720_dsi_cmd_auo_fwvga_lcm_drv;
extern LCM_DRIVER nt35590_hd720_dsi_cmd_auo_wvga_lcm_drv;
extern LCM_DRIVER nt35590_hd720_dsi_cmd_auo_qhd_lcm_drv;
extern LCM_DRIVER nt35590_hd720_dsi_cmd_cmi_lcm_drv;
extern LCM_DRIVER nt35516_qhd_dsi_cmd_ipsboe_lcm_drv;
extern LCM_DRIVER nt35516_qhd_dsi_cmd_ipsboe_wvga_lcm_drv;
extern LCM_DRIVER nt35516_qhd_dsi_cmd_ipsboe_fwvga_lcm_drv;
extern LCM_DRIVER nt35516_qhd_dsi_cmd_ips9k1431_drv;
extern LCM_DRIVER nt35516_qhd_dsi_cmd_tft9k1342_drv;
extern LCM_DRIVER bp070ws1_lcm_drv;
extern LCM_DRIVER bp101wx1_lcm_drv;
extern LCM_DRIVER bp101wx1_n_lcm_drv;
extern LCM_DRIVER nt35516_qhd_rav4_lcm_drv;
extern LCM_DRIVER r63311_fhd_dsi_vdo_sharp_lcm_drv;
extern LCM_DRIVER r81592_hvga_dsi_cmd_drv;
extern LCM_DRIVER rm68190_dsi_vdo_lcm_drv;
extern LCM_DRIVER nt35596_fhd_dsi_vdo_truly_lcm_drv;
extern LCM_DRIVER nt35595_fhd_dsi_vdo_truly_lcm_drv;
extern LCM_DRIVER nt35595_fhd_dsi_cmd_truly_lcm_drv;
extern LCM_DRIVER nt35595_fhd_dsi_cmd_truly_tps65132_lcm_drv;
extern LCM_DRIVER nt35595_fhd_dsi_vdo_truly_tps65132_lcm_drv;
extern LCM_DRIVER nt35595_fhd_dsi_cmd_truly_nt50358_lcm_drv;
extern LCM_DRIVER nt35595_fhd_dsi_cmd_truly_nt50358_720p_lcm_drv;
extern LCM_DRIVER nt35595_fhd_dsi_cmd_truly_nt50358_qhd_lcm_drv;
extern LCM_DRIVER nt35595_fhd_dsi_cmd_truly_nt50358_fwvga_lcm_drv;
extern LCM_DRIVER nt35595_fhd_dsi_cmd_truly_nt50358_wvga_lcm_drv;
extern LCM_DRIVER nt35595_fhd_dsi_cmd_truly_tps65132_720p_lcm_drv;
extern LCM_DRIVER nt35596_fhd_dsi_vdo_yassy_lcm_drv;
extern LCM_DRIVER nt35596_hd720_dsi_vdo_truly_tps65132_lcm_drv;
extern LCM_DRIVER nt35590_hd720_dsi_cmd_truly2_lcm_drv;
extern LCM_DRIVER otm9608_wvga_dsi_cmd_drv;
extern LCM_DRIVER otm9608_qhd_dsi_cmd_drv;
extern LCM_DRIVER nt35510_dbi_18bit_gionee_lcm_drv;
extern LCM_DRIVER otm8009a_fwvga_dsi_cmd_tianma_lcm_drv;
extern LCM_DRIVER otm8009a_fwvga_dsi_vdo_tianma_lcm_drv;
extern LCM_DRIVER hx8389b_qhd_dsi_vdo_tianma_lcm_drv;
extern LCM_DRIVER cm_otc3108bhv161_dsi_vdo_lcm_drv;
extern LCM_DRIVER auo_b079xat02_dsi_vdo_lcm_drv;
extern LCM_DRIVER hx8389b_qhd_dsi_vdo_tianma055xdhp_lcm_drv;
extern LCM_DRIVER cpt_claa101fp01_dsi_vdo_lcm_drv;
extern LCM_DRIVER h070d_18dm_lcm_drv;
extern LCM_DRIVER hx8394a_hd720_dsi_vdo_tianma_lcm_drv;
extern LCM_DRIVER hx8394d_hd720_dsi_vdo_tianma_lcm_drv;
extern LCM_DRIVER cpt_clap070wp03xg_sn65dsi83_lcm_drv;
extern LCM_DRIVER nt35520_hd720_tm_lcm_drv;
extern LCM_DRIVER nt35520_hd720_boe_lcm_drv;
extern LCM_DRIVER nt35521_hd720_dsi_vdo_boe_lcm_drv;
extern LCM_DRIVER nt35521_hd720_tm_lcm_drv;
extern LCM_DRIVER r69429_wuxga_dsi_vdo_lcm_drv;
extern LCM_DRIVER r69429_wuxga_dsi_cmd_lcm_drv;
extern LCM_DRIVER rm68210_hd720_dsi_ufoe_cmd_lcm_drv;
extern LCM_DRIVER r63311_fhd_dsi_vedio_lcm_drv;
extern LCM_DRIVER otm1283a_6589_hd_dsi;
extern LCM_DRIVER hx8394a_hd720_dsi_vdo_tianma_v2_lcm_drv;
extern LCM_DRIVER cpt_clap070wp03xg_lvds_lcm_drv;
extern LCM_DRIVER otm8018b_dsi_vdo_lcsh72_lcm_drv;
extern LCM_DRIVER hx8369_dsi_cmd_6571_lcm_drv;
extern LCM_DRIVER hx8369_dsi_vdo_6571_lcm_drv;
extern LCM_DRIVER hx8369_dbi_6571_lcm_drv;
extern LCM_DRIVER hx8369_dpi_6571_lcm_drv;
extern LCM_DRIVER nt35510_dsi_cmd_6571_lcm_drv;
extern LCM_DRIVER nt35510_dsi_cmd_6571_hvga_lcm_drv;
extern LCM_DRIVER nt35510_dsi_cmd_6571_qvga_lcm_drv;
extern LCM_DRIVER nt35510_dsi_vdo_6571_lcm_drv;
extern LCM_DRIVER nt35510_dbi_6571_lcm_drv;
extern LCM_DRIVER nt35510_dpi_6571_lcm_drv;
extern LCM_DRIVER nt35590_dsi_cmd_6571_fwvga_lcm_drv;
extern LCM_DRIVER nt35590_dsi_cmd_6571_qhd_lcm_drv;
extern LCM_DRIVER it6151_edp_dsi_video_sharp_lcm_drv; 
extern LCM_DRIVER nt35517_qhd_dsi_vdo_lcm_drv;
extern LCM_DRIVER otm1283a_hd720_dsi_vdo_tm_lcm_drv;
extern LCM_DRIVER otm1284a_hd720_dsi_vdo_tm_lcm_drv;
extern LCM_DRIVER otm1285a_hd720_dsi_vdo_tm_lcm_drv;
extern LCM_DRIVER hx8389b_qhd_dsi_vdo_lgd_lcm_drv;
extern LCM_DRIVER it6151_fhd_edp_dsi_video_auo_lcm_drv;
extern LCM_DRIVER tf070mc_rgb_v18_mt6571_lcm_drv;
extern LCM_DRIVER zs070ih5015b3h6_mt6571_lcm_drv;
extern LCM_DRIVER a080ean01_dsi_vdo_lcm_drv;
extern LCM_DRIVER it6121_g156xw01v1_lvds_vdo_lcm_drv;
extern LCM_DRIVER cpt_clap070wp03xg_lvds_lcm_drv;
extern LCM_DRIVER r63315_fhd_dsi_vdo_truly_lcm_drv;
extern LCM_DRIVER it6151_lp079qx1_edp_dsi_video_lcm_drv;
extern LCM_DRIVER RX_498HX_615B_lcm_drv;
extern LCM_DRIVER RX_498HX_615B_82_lcm_drv;
extern LCM_DRIVER ili9806c_dsi_vdo_djn_fwvga_lcm_drv;
extern LCM_DRIVER hx8389b_hd720_dsi_vdo_drv;
extern LCM_DRIVER r69338_hd720_dsi_vdo_jdi_drv;
extern LCM_DRIVER db7436_dsi_vdo_fwvga_drv;
extern LCM_DRIVER r63417_fhd_dsi_cmd_truly_nt50358_lcm_drv;
extern LCM_DRIVER r63419_wqhd_truly_phantom_cmd_lcm_drv;
extern LCM_DRIVER r63419_wqhd_truly_phantom_vdo_lcm_drv;
extern LCM_DRIVER r63419_fhd_truly_phantom_lcm_drv;
extern LCM_DRIVER r63423_wqhd_truly_phantom_lcm_drv;
extern LCM_DRIVER kr101ia2s_dsi_vdo_lcm_drv;
extern LCM_DRIVER r69338_hd720_dsi_vdo_jdi_dw8755a_drv;
extern LCM_DRIVER otm9605a_qhd_dsi_vdo_drv;
extern LCM_DRIVER ili9806e_dsi_vdo_fwvga_drv;
extern LCM_DRIVER otm1906a_fhd_dsi_cmd_auto_lcm_drv;
extern LCM_DRIVER otm1902a_fhd_dsi_cmd_tianma_lcm_drv;
extern LCM_DRIVER nt35596_fhd_tianma_phantom_lcm_drv;
extern LCM_DRIVER nt35532_fhd_boe_vdo_lcm_drv;
extern LCM_DRIVER nt35596_fhd_auo_phantom_lcm_drv;
extern LCM_DRIVER r63315_fhd_sharp_phantom_lcm_drv;
extern LCM_DRIVER nt35532_fhd_boe_s11_vdo_lcm_drv;

LCM_DRIVER* lcm_driver_list[] = 
{
#if defined(NT35596_FHD_DSI_VDO_TIANMA)
	&nt35596_fhd_tianma_phantom_lcm_drv,
#endif

#if defined(NT35596_FHD_DSI_VDO_AUO)
	&nt35596_fhd_auo_phantom_lcm_drv,
#endif

#if defined(NT35532_FHD_DSI_VDO_BOE)
	&nt35532_fhd_boe_vdo_lcm_drv,
#endif

#if defined(NT35596_FHD_DSI_VDO_TIANMA_CX865)
	&nt35596_fhd_tianma_phantom_lcm_drv,
#endif

#if defined(R63315_FHD_DSI_VDO_SHARP)
	&r63315_fhd_sharp_phantom_lcm_drv,
#endif

#if defined(NT35532_FHD_DSI_VDO_BOE_CX865)
	&nt35532_fhd_boe_vdo_lcm_drv,
#endif

#if defined(NT35532_FHD_DSI_VDO_BOE_S11_CX865)
	&nt35532_fhd_boe_s11_vdo_lcm_drv,
#endif

#if defined(OTM1284A_HD720_DSI_VDO_TM)
	&otm1284a_hd720_dsi_vdo_tm_lcm_drv,
#endif
#if defined(OTM1285A_HD720_DSI_VDO_TM)
	&otm1285a_hd720_dsi_vdo_tm_lcm_drv,
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

#if defined(KR101IA2S_DSI_VDO)
	&kr101ia2s_dsi_vdo_lcm_drv,
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

#if defined(HX8392A_DSI_VDO)
  &hx8392a_dsi_vdo_lcm_drv,
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

#if defined(OTM9608_QHD_DSI_CMD)
	&otm9608_qhd_dsi_cmd_drv,
#endif

#if defined(NT35510_DBI_18BIT_GIONEE)
	&nt35510_dbi_18bit_gionee_lcm_drv,
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

#if defined(OTM8018B_DSI_VDO_LCSH72)
	&otm8018b_dsi_vdo_lcsh72_lcm_drv,
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

#if defined(DB7436_DSI_VDO_FWVGA)
	&db7436_dsi_vdo_fwvga_drv,
#endif

#if defined(R63417_FHD_DSI_CMD_TRULY_NT50358)
    &r63417_fhd_dsi_cmd_truly_nt50358_lcm_drv,
#endif

#if defined(R63419_WQHD_TRULY_PHANTOM_2K_CMD_OK)
    &r63419_wqhd_truly_phantom_cmd_lcm_drv,
#endif

#if defined(R63419_WQHD_TRULY_PHANTOM_2K_VDO_OK)
    &r63419_wqhd_truly_phantom_vdo_lcm_drv,
#endif

#if defined(R63419_FHD_TRULY_PHANTOM_2K_CMD_OK)
    &r63419_fhd_truly_phantom_lcm_drv,
#endif

#if defined(R63423_WQHD_TRULY_PHANTOM_2K_CMD_OK)
    &r63423_wqhd_truly_phantom_lcm_drv,
#endif

#if defined(OTM9605A_QHD_DSI_VDO)
	&otm9605a_qhd_dsi_vdo_drv,
#endif

#if defined(OTM1906A_FHD_DSI_CMD_AUTO)
    &otm1906a_fhd_dsi_cmd_auto_lcm_drv,
#endif

#if defined(OTM1902A_FHD_DSI_CMD_TIANMA)
	&otm1902a_fhd_dsi_cmd_tianma_lcm_drv,
#endif
};

#define LCM_COMPILE_ASSERT(condition) LCM_COMPILE_ASSERT_X(condition, __LINE__)
#define LCM_COMPILE_ASSERT_X(condition, line) LCM_COMPILE_ASSERT_XX(condition, line)
#define LCM_COMPILE_ASSERT_XX(condition, line) char assertion_failed_at_line_##line[(condition)?1:-1]

unsigned int lcm_count = sizeof(lcm_driver_list)/sizeof(LCM_DRIVER*);
LCM_COMPILE_ASSERT(0 != sizeof(lcm_driver_list)/sizeof(LCM_DRIVER*));
#if 1
#ifdef BUILD_LK
extern void mdelay(unsigned long msec);
#endif
static unsigned char lcd_id_pins_value = 0xFF;


/******************************************************************************
Function:       which_lcd_module_triple
  Description:    read LCD ID PIN status,could identify three status:highlowfloat
  Input:           none
  Output:         none
  Return:         LCD ID1|ID0 value
  Others:         
******************************************************************************/
unsigned char which_lcd_module_triple(void)
{
    unsigned char  high_read0 = 0;
    unsigned char  low_read0 = 0;
    unsigned char  high_read1 = 0;
    unsigned char  low_read1 = 0;
    unsigned char  lcd_id0 = 0;
    unsigned char  lcd_id1 = 0;
    unsigned char  lcd_id = 0;
    //Solve Coverity scan warning : check return value
    unsigned int ret = 0;
    //only recognise once
    if(0xFF != lcd_id_pins_value) 
    {
        return lcd_id_pins_value;
    }
    //Solve Coverity scan warning : check return value
    ret = mt_set_gpio_mode(GPIO_DISP_ID0_PIN, GPIO_MODE_00);
    if(0 != ret)
    {
        LCD_DEBUG("ID0 mt_set_gpio_mode fail\n");
    }
    ret = mt_set_gpio_dir(GPIO_DISP_ID0_PIN, GPIO_DIR_IN);
    if(0 != ret)
    {
        LCD_DEBUG("ID0 mt_set_gpio_dir fail\n");
    }
    ret = mt_set_gpio_pull_enable(GPIO_DISP_ID0_PIN, GPIO_PULL_ENABLE);
    if(0 != ret)
    {
        LCD_DEBUG("ID0 mt_set_gpio_pull_enable fail\n");
    }
    ret = mt_set_gpio_mode(GPIO_DISP_ID1_PIN, GPIO_MODE_00);
    if(0 != ret)
    {
        LCD_DEBUG("ID1 mt_set_gpio_mode fail\n");
    }
    ret = mt_set_gpio_dir(GPIO_DISP_ID1_PIN, GPIO_DIR_IN);
    if(0 != ret)
    {
        LCD_DEBUG("ID1 mt_set_gpio_dir fail\n");
    }
    ret = mt_set_gpio_pull_enable(GPIO_DISP_ID1_PIN, GPIO_PULL_ENABLE);
    if(0 != ret)
    {
        LCD_DEBUG("ID1 mt_set_gpio_pull_enable fail\n");
    }
    //pull down ID0 ID1 PIN    
    ret = mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN,GPIO_PULL_DOWN);
    if(0 != ret)
    {
        LCD_DEBUG("ID0 mt_set_gpio_pull_select->Down fail\n");
    }
    ret = mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN,GPIO_PULL_DOWN);
    if(0 != ret)
    {
        LCD_DEBUG("ID1 mt_set_gpio_pull_select->Down fail\n");
    }
    //delay 100ms , for discharging capacitance 
    mdelay(100);
    //get ID0 ID1 status
    low_read0 = mt_get_gpio_in(GPIO_DISP_ID0_PIN);
    low_read1 = mt_get_gpio_in(GPIO_DISP_ID1_PIN);
    //pull up ID0 ID1 PIN 
    ret = mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN,GPIO_PULL_UP);
    if(0 != ret)
    {
        LCD_DEBUG("ID0 mt_set_gpio_pull_select->UP fail\n");
    }
    ret = mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN,GPIO_PULL_UP);
    if(0 != ret)
    {
        LCD_DEBUG("ID1 mt_set_gpio_pull_select->UP fail\n");
    }
    //delay 100ms , for charging capacitance 
    mdelay(100);
    //get ID0 ID1 status
    high_read0 = mt_get_gpio_in(GPIO_DISP_ID0_PIN);
    high_read1 = mt_get_gpio_in(GPIO_DISP_ID1_PIN);

    if( low_read0 != high_read0 )
    {
        /*float status , pull down ID0 ,to prevent electric leakage*/
        ret = mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN,GPIO_PULL_DOWN);
        if(0 != ret)
        {
            LCD_DEBUG("ID0 mt_set_gpio_pull_select->Down fail\n");
        }
        lcd_id0 = LCD_HW_ID_STATUS_FLOAT;
    }
    else if((LCD_HW_ID_STATUS_LOW == low_read0) && (LCD_HW_ID_STATUS_LOW == high_read0))
    {
        /*low status , pull down ID0 ,to prevent electric leakage*/
        ret = mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN,GPIO_PULL_DOWN);
        if(0 != ret)
        {
            LCD_DEBUG("ID0 mt_set_gpio_pull_select->Down fail\n");
        }
        lcd_id0 = LCD_HW_ID_STATUS_LOW;
    }
    else if((LCD_HW_ID_STATUS_HIGH == low_read0) && (LCD_HW_ID_STATUS_HIGH == high_read0))
    {
        /*high status , pull up ID0 ,to prevent electric leakage*/
        ret = mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN,GPIO_PULL_UP);
        if(0 != ret)
        {
            LCD_DEBUG("ID0 mt_set_gpio_pull_select->UP fail\n");
        }
        lcd_id0 = LCD_HW_ID_STATUS_HIGH;
    }
    else
    {
        LCD_DEBUG(" Read LCD_id0 error\n");
        ret = mt_set_gpio_pull_select(GPIO_DISP_ID0_PIN,GPIO_PULL_DISABLE);
        if(0 != ret)
        {
            LCD_DEBUG("ID0 mt_set_gpio_pull_select->Disbale fail\n");
        }
        lcd_id0 = LCD_HW_ID_STATUS_ERROR;
    }


    if( low_read1 != high_read1 )
    {
        /*float status , pull down ID1 ,to prevent electric leakage*/
        ret = mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN,GPIO_PULL_DOWN);
        if(0 != ret)
        {
            LCD_DEBUG("ID1 mt_set_gpio_pull_select->Down fail\n");
        }
        lcd_id1 = LCD_HW_ID_STATUS_FLOAT;
    }
    else if((LCD_HW_ID_STATUS_LOW == low_read1) && (LCD_HW_ID_STATUS_LOW == high_read1))
    {
        /*low status , pull down ID1 ,to prevent electric leakage*/
        ret = mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN,GPIO_PULL_DOWN);
        if(0 != ret)
        {
            LCD_DEBUG("ID1 mt_set_gpio_pull_select->Down fail\n");
        }
        lcd_id1 = LCD_HW_ID_STATUS_LOW;
    }
    else if((LCD_HW_ID_STATUS_HIGH == low_read1) && (LCD_HW_ID_STATUS_HIGH == high_read1))
    {
        /*high status , pull up ID1 ,to prevent electric leakage*/
        ret = mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN,GPIO_PULL_UP);
        if(0 != ret)
        {
            LCD_DEBUG("ID1 mt_set_gpio_pull_select->UP fail\n");
        }
        lcd_id1 = LCD_HW_ID_STATUS_HIGH;
    }
    else
    {

        LCD_DEBUG(" Read LCD_id1 error\n");
        ret = mt_set_gpio_pull_select(GPIO_DISP_ID1_PIN,GPIO_PULL_DISABLE);
        if(0 != ret)
        {
            LCD_DEBUG("ID1 mt_set_gpio_pull_select->Disable fail\n");
        }
        lcd_id1 = LCD_HW_ID_STATUS_ERROR;
    }
#ifdef BUILD_LK
    dprintf(CRITICAL,"which_lcd_module_triple,lcd_id0:%d\n",lcd_id0);
    dprintf(CRITICAL,"which_lcd_module_triple,lcd_id1:%d\n",lcd_id1);
#else
    printk("which_lcd_module_triple,lcd_id0:%d\n",lcd_id0);
    printk("which_lcd_module_triple,lcd_id1:%d\n",lcd_id1);
#endif
    lcd_id =  lcd_id0 | (lcd_id1 << 1);

#ifdef BUILD_LK
    dprintf(CRITICAL,"which_lcd_module_triple,lcd_id:%d\n",lcd_id);
#else
    printk("which_lcd_module_triple,lcd_id:%d\n",lcd_id);
#endif

    lcd_id_pins_value = lcd_id;
    return lcd_id;
}
#endif
