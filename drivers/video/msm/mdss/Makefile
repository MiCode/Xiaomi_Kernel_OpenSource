mdss-mdp3-objs = mdp3.o mdp3_dma.o mdp3_ctrl.o
mdss-mdp3-objs += mdp3_ppp.o mdp3_ppp_hwio.o mdp3_ppp_data.o
obj-$(CONFIG_FB_MSM_MDSS_MDP3) += mdss-mdp3.o

mdss-mdp-objs := mdss_mdp.o mdss_mdp_ctl.o mdss_mdp_pipe.o mdss_mdp_util.o
mdss-mdp-objs += mdss_mdp_pp.o
mdss-mdp-objs += mdss_mdp_intf_video.o
mdss-mdp-objs += mdss_mdp_intf_cmd.o
mdss-mdp-objs += mdss_mdp_intf_writeback.o
mdss-mdp-objs += mdss_mdp_rotator.o
mdss-mdp-objs += mdss_mdp_overlay.o
mdss-mdp-objs += mdss_mdp_wb.o
obj-$(CONFIG_FB_MSM_MDSS) += mdss-mdp.o

ifeq ($(CONFIG_FB_MSM_MDSS),y)
obj-$(CONFIG_DEBUG_FS) += mdss_debug.o
endif

dsi-v2-objs = dsi_v2.o dsi_host_v2.o dsi_io_v2.o
obj-$(CONFIG_FB_MSM_MDSS_MDP3) += dsi-v2.o

mdss-dsi-objs := mdss_dsi.o mdss_dsi_host.o mdss_dsi_cmd.o
mdss-dsi-objs += mdss_dsi_panel.o
mdss-dsi-objs += msm_mdss_io_8974.o
obj-$(CONFIG_FB_MSM_MDSS) += mdss-dsi.o
obj-$(CONFIG_FB_MSM_MDSS) += mdss_edp.o
obj-$(CONFIG_FB_MSM_MDSS) += mdss_edp_aux.o

obj-$(CONFIG_FB_MSM_MDSS) += mdss_io_util.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_PANEL) += mdss_hdmi_tx.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_PANEL) += mdss_hdmi_util.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_PANEL) += mdss_hdmi_edid.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_PANEL) += mdss_hdmi_hdcp.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_PANEL) += mdss_hdmi_cec.o
obj-$(CONFIG_FB_MSM_MDSS_HDMI_MHL_SII8334) += mhl_sii8334.o mhl_msc.o

obj-$(CONFIG_FB_MSM_MDSS_WRITEBACK) += mdss_wb.o

mdss-qpic-objs := mdss_qpic.o mdss_fb.o mdss_qpic_panel.o
obj-$(CONFIG_FB_MSM_QPIC) += mdss-qpic.o
obj-$(CONFIG_FB_MSM_QPIC_ILI_QVGA_PANEL) += qpic_panel_ili_qvga.o

obj-$(CONFIG_FB_MSM_MDSS) += mdss_fb.o

ifeq ($(CONFIG_FB_MSM_MDSS_MDP3),y)
obj-$(CONFIG_FB_MSM_MDSS_DSI_CTRL_STATUS) += dsi_status_v2.o
else
obj-$(CONFIG_FB_MSM_MDSS_DSI_CTRL_STATUS) += mdss_dsi_status.o
endif
