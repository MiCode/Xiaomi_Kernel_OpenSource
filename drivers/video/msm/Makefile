ifeq ($(CONFIG_FB_MSM_MDSS_COMMON),y)
obj-y += mdss/
else
obj-y := msm_fb.o

obj-$(CONFIG_FB_MSM_LOGO) += logo.o
obj-$(CONFIG_FB_BACKLIGHT) += msm_fb_bl.o

ifeq ($(CONFIG_FB_MSM_MDP_HW),y)
# MDP
obj-y += mdp.o

obj-$(CONFIG_DEBUG_FS) += mdp_debugfs.o

ifeq ($(CONFIG_FB_MSM_MDP40),y)
obj-y += mdp4_util.o
else
obj-y += mdp_hw_init.o
obj-y += mdp_ppp.o
ifeq ($(CONFIG_FB_MSM_MDP31),y)
obj-y += mdp_ppp_v31.o
else
obj-y += mdp_ppp_v20.o
endif
endif

ifeq ($(CONFIG_FB_MSM_OVERLAY),y)
obj-y += mdp4_overlay.o
obj-y += mdp4_overlay_lcdc.o
ifeq ($(CONFIG_FB_MSM_MIPI_DSI),y)
obj-y += mdp4_overlay_dsi_video.o
obj-y += mdp4_overlay_dsi_cmd.o
else
obj-y += mdp4_overlay_mddi.o
endif
else
obj-y += mdp_dma_lcdc.o
endif

obj-$(CONFIG_FB_MSM_MDP303) += mdp_dma_dsi_video.o

ifeq ($(CONFIG_FB_MSM_DTV),y)
obj-y += mdp4_dtv.o
obj-y += mdp4_overlay_dtv.o
endif

obj-y += mdp_dma.o
obj-y += mdp_dma_s.o
obj-y += mdp_vsync.o
obj-y += mdp_cursor.o
obj-y += mdp_dma_tv.o
obj-$(CONFIG_ARCH_MSM7X27A) += msm_dss_io_7x27a.o
obj-$(CONFIG_ARCH_MSM8X60) += msm_dss_io_8x60.o
obj-$(CONFIG_ARCH_MSM8960) += msm_dss_io_8960.o

# EBI2
obj-$(CONFIG_FB_MSM_EBI2) += ebi2_lcd.o

# LCDC
obj-$(CONFIG_FB_MSM_LCDC) += lcdc.o

# LVDS
obj-$(CONFIG_FB_MSM_LVDS) += lvds.o

# MDDI
msm_mddi-objs := mddi.o mddihost.o mddihosti.o
obj-$(CONFIG_FB_MSM_MDDI) += msm_mddi.o

# External MDDI
msm_mddi_ext-objs := mddihost_e.o mddi_ext.o
obj-$(CONFIG_FB_MSM_EXTMDDI) += msm_mddi_ext.o

# MIPI gereric
msm_mipi-objs := mipi_dsi.o mipi_dsi_host.o
obj-$(CONFIG_FB_MSM_MIPI_DSI) += msm_mipi.o

# MIPI manufacture
obj-$(CONFIG_FB_MSM_MIPI_DSI_TOSHIBA) += mipi_toshiba.o
obj-$(CONFIG_FB_MSM_MIPI_DSI_NOVATEK) += mipi_novatek.o
obj-$(CONFIG_FB_MSM_MIPI_DSI_ORISE) += mipi_orise.o
obj-$(CONFIG_FB_MSM_MIPI_DSI_RENESAS) += mipi_renesas.o
obj-$(CONFIG_FB_MSM_MIPI_DSI_TRULY) += mipi_truly.o
obj-$(CONFIG_FB_MSM_MIPI_DSI_NT35510) += mipi_NT35510.o
obj-$(CONFIG_FB_MSM_MIPI_DSI_NT35516) += mipi_truly_tft540960_1_e.o
obj-$(CONFIG_FB_MSM_MIPI_DSI_SIMULATOR) += mipi_simulator.o

# MIPI Bridge
obj-$(CONFIG_FB_MSM_MIPI_DSI_TC358764_DSI2LVDS) += mipi_tc358764_dsi2lvds.o

# TVEnc
obj-$(CONFIG_FB_MSM_TVOUT) += tvenc.o
ifeq ($(CONFIG_FB_MSM_OVERLAY),y)
obj-$(CONFIG_FB_MSM_TVOUT) += mdp4_overlay_atv.o
endif

# MSM FB Panel
obj-y += msm_fb_panel.o
obj-$(CONFIG_FB_MSM_EBI2_TMD_QVGA_EPSON_QCIF) += ebi2_tmd20.o
obj-$(CONFIG_FB_MSM_EBI2_TMD_QVGA_EPSON_QCIF) += ebi2_l2f.o

ifeq ($(CONFIG_FB_MSM_MDDI_AUTO_DETECT),y)
obj-y += mddi_prism.o
obj-y += mddi_toshiba.o
obj-y += mddi_toshiba_vga.o
obj-y += mddi_toshiba_wvga_pt.o
obj-y += mddi_toshiba_wvga.o
obj-y += mddi_sharp.o
obj-y += mddi_orise.o
obj-y += mddi_quickvx.o
else
obj-$(CONFIG_FB_MSM_MDDI_PRISM_WVGA) += mddi_prism.o
obj-$(CONFIG_FB_MSM_MDDI_TOSHIBA_COMMON) += mddi_toshiba.o
obj-$(CONFIG_FB_MSM_MDDI_TOSHIBA_COMMON_VGA) += mddi_toshiba_vga.o
obj-$(CONFIG_FB_MSM_MDDI_TOSHIBA_WVGA_PORTRAIT) += mddi_toshiba_wvga_pt.o
obj-$(CONFIG_FB_MSM_MDDI_TOSHIBA_WVGA) += mddi_toshiba_wvga.o
obj-$(CONFIG_FB_MSM_MDDI_SHARP_QVGA_128x128) += mddi_sharp.o
obj-$(CONFIG_FB_MSM_MDDI_ORISE) += mddi_orise.o
obj-$(CONFIG_FB_MSM_MDDI_QUICKVX) += mddi_quickvx.o
endif

ifeq ($(CONFIG_FB_MSM_MIPI_PANEL_DETECT),y)
obj-y += mipi_toshiba_video_wvga_pt.o mipi_toshiba_video_wsvga_pt.o mipi_toshiba_video_wuxga.o
obj-y += mipi_novatek_video_qhd_pt.o mipi_novatek_cmd_qhd_pt.o
obj-y += mipi_orise_video_720p_pt.o mipi_orise_cmd_720p_pt.o
obj-y += mipi_renesas_video_fwvga_pt.o mipi_renesas_cmd_fwvga_pt.o
obj-y += mipi_NT35510_video_wvga_pt.o mipi_NT35510_cmd_wvga_pt.o
obj-y += mipi_truly_tft540960_1_e_video_qhd_pt.o mipi_truly_tft540960_1_e_cmd_qhd_pt.o
obj-y += mipi_chimei_wxga_pt.o
obj-y += mipi_chimei_wuxga.o
obj-y += mipi_truly_video_wvga_pt.o
else
obj-$(CONFIG_FB_MSM_MIPI_TOSHIBA_VIDEO_WVGA_PT) += mipi_toshiba_video_wvga_pt.o
obj-$(CONFIG_FB_MSM_MIPI_TOSHIBA_VIDEO_WSVGA_PT) += mipi_toshiba_video_wsvga_pt.o
obj-$(CONFIG_FB_MSM_MIPI_TOSHIBA_VIDEO_WUXGA) += mipi_toshiba_video_wuxga.o
obj-$(CONFIG_FB_MSM_MIPI_NOVATEK_VIDEO_QHD_PT) += mipi_novatek_video_qhd_pt.o
obj-$(CONFIG_FB_MSM_MIPI_ORISE_VIDEO_720P_PT) += mipi_orise_video_720p_pt.o
obj-$(CONFIG_FB_MSM_MIPI_ORISE_CMD_720P_PT) += mipi_orise_cmd_720p_pt.o
obj-$(CONFIG_FB_MSM_MIPI_NOVATEK_CMD_QHD_PT) += mipi_novatek_cmd_qhd_pt.o
obj-$(CONFIG_FB_MSM_MIPI_RENESAS_VIDEO_FWVGA_PT) += mipi_renesas_video_fwvga_pt.o
obj-$(CONFIG_FB_MSM_MIPI_RENESAS_CMD_FWVGA_PT) += mipi_renesas_cmd_fwvga_pt.o
obj-$(CONFIG_FB_MSM_MIPI_RENESAS_VIDEO_FWVGA_PT) += mipi_renesas_video_fwvga_pt.o
obj-$(CONFIG_FB_MSM_MIPI_TRULY_VIDEO_WVGA_PT) += mipi_truly_video_wvga_pt.o
obj-$(CONFIG_FB_MSM_MIPI_NT35510_CMD_WVGA_PT) += mipi_NT35510_cmd_wvga_pt.o
obj-$(CONFIG_FB_MSM_MIPI_NT35510_VIDEO_WVGA_PT) += mipi_NT35510_video_wvga_pt.o
obj-$(CONFIG_FB_MSM_MIPI_NT35516_CMD_QHD_PT) += mipi_truly_tft540960_1_e_cmd_qhd_pt.o
obj-$(CONFIG_FB_MSM_MIPI_NT35516_VIDEO_QHD_PT) += mipi_truly_tft540960_1_e_video_qhd_pt.o
obj-$(CONFIG_FB_MSM_MIPI_SIMULATOR_VIDEO) += mipi_simulator_video.o
obj-$(CONFIG_FB_MSM_MIPI_CHIMEI_WXGA) += mipi_chimei_wxga_pt.o
obj-$(CONFIG_FB_MSM_MIPI_CHIMEI_WUXGA) += mipi_chimei_wuxga.o
endif

obj-$(CONFIG_FB_MSM_LCDC_PANEL) += lcdc_panel.o
obj-$(CONFIG_FB_MSM_LCDC_PRISM_WVGA) += lcdc_prism.o
obj-$(CONFIG_FB_MSM_LCDC_SAMSUNG_WSVGA) += lcdc_samsung_wsvga.o
obj-$(CONFIG_FB_MSM_LCDC_CHIMEI_WXGA) += lcdc_chimei_wxga.o
obj-$(CONFIG_FB_MSM_LCDC_NT35582_WVGA) += lcdc_nt35582_wvga.o
obj-$(CONFIG_FB_MSM_LCDC_EXTERNAL_WXGA) += lcdc_external.o
obj-$(CONFIG_FB_MSM_HDMI_SII_EXTERNAL_720P) += hdmi_sii9022.o
obj-$(CONFIG_FB_MSM_LCDC_GORDON_VGA) += lcdc_gordon.o
obj-$(CONFIG_FB_MSM_LCDC_WXGA) += lcdc_wxga.o
obj-$(CONFIG_FB_MSM_LCDC_TOSHIBA_WVGA_PT) += lcdc_toshiba_wvga_pt.o
obj-$(CONFIG_FB_MSM_LCDC_TOSHIBA_FWVGA_PT) += lcdc_toshiba_fwvga_pt.o
obj-$(CONFIG_FB_MSM_LCDC_SHARP_WVGA_PT) += lcdc_sharp_wvga_pt.o
obj-$(CONFIG_FB_MSM_LCDC_AUO_WVGA) += lcdc_auo_wvga.o
obj-$(CONFIG_FB_MSM_LCDC_SAMSUNG_OLED_PT) += lcdc_samsung_oled_pt.o
obj-$(CONFIG_FB_MSM_HDMI_ADV7520_PANEL) += adv7520.o
obj-$(CONFIG_FB_MSM_LCDC_ST15_WXGA) += lcdc_st15.o
obj-$(CONFIG_FB_MSM_LVDS_CHIMEI_WXGA) += lvds_chimei_wxga.o
obj-$(CONFIG_FB_MSM_LVDS_FRC_FHD) += lvds_frc_fhd.o
obj-$(CONFIG_FB_MSM_HDMI_MSM_PANEL) += hdmi_msm.o
obj-$(CONFIG_FB_MSM_EXT_INTERFACE_COMMON) += external_common.o
obj-$(CONFIG_FB_MSM_LCDC_TRULY_HVGA_IPS3P2335) += lcdc_truly_ips3p2335.o

obj-$(CONFIG_FB_MSM_TVOUT) += tvout_msm.o

ccflags-y := -I$(src)/mhl
obj-$(CONFIG_FB_MSM_HDMI_MHL_8334) += mhl-8334.o
mhl-8334-objs  += mhl/mhl_8334.o
mhl-8334-objs  += mhl/mhl_i2c_utils.o

obj-$(CONFIG_FB_MSM_EXTMDDI_SVGA) += mddi_ext_lcd.o

obj-$(CONFIG_FB_MSM_WRITEBACK_MSM_PANEL) += mdp4_wfd_writeback_panel.o
obj-$(CONFIG_FB_MSM_WRITEBACK_MSM_PANEL) += mdp4_wfd_writeback.o
obj-$(CONFIG_FB_MSM_WRITEBACK_MSM_PANEL) += mdp4_overlay_writeback.o
else
obj-$(CONFIG_FB_MSM_EBI2) += ebi2_host.o
obj-$(CONFIG_FB_MSM_EBI2) += ebi2_lcd.o
obj-y += msm_fb_panel.o
obj-$(CONFIG_FB_MSM_EBI2_EPSON_S1D_QVGA_PANEL) += ebi2_epson_s1d_qvga.o
endif
endif

obj-$(CONFIG_MSM_VIDC_1080P) += vidc/
obj-$(CONFIG_MSM_VIDC_720P) += vidc/
clean:
	rm *.o .*cmd
