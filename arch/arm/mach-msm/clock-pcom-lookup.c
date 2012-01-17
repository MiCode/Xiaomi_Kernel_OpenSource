/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include "clock.h"
#include "clock-pcom.h"
#include "clock-voter.h"

static DEFINE_CLK_PCOM(adm_clk,		ADM_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(adsp_clk,	ADSP_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(ahb_m_clk,	AHB_M_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(ahb_s_clk,	AHB_S_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(cam_m_clk,	CAM_M_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(axi_rotator_clk,	AXI_ROTATOR_CLK, 0);
static DEFINE_CLK_PCOM(ce_clk,		CE_CLK,		CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(csi0_clk,	CSI0_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(csi0_p_clk,	CSI0_P_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(csi0_vfe_clk,	CSI0_VFE_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(csi1_clk,	CSI1_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(csi1_p_clk,	CSI1_P_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(csi1_vfe_clk,	CSI1_VFE_CLK,	CLKFLAG_SKIP_AUTO_OFF);

static struct pcom_clk dsi_byte_clk = {
	.id = P_DSI_BYTE_CLK,
	.c = {
		.ops = &clk_ops_pcom_ext_config,
		.dbg_name = "dsi_byte_clk",
		CLK_INIT(dsi_byte_clk.c),
	},
};

static struct pcom_clk dsi_clk = {
	.id = P_DSI_CLK,
	.c = {
		.ops = &clk_ops_pcom_ext_config,
		.dbg_name = "dsi_clk",
		CLK_INIT(dsi_clk.c),
	},
};

static struct pcom_clk dsi_esc_clk = {
	.id = P_DSI_ESC_CLK,
	.c = {
		.ops = &clk_ops_pcom_ext_config,
		.dbg_name = "dsi_esc_clk",
		CLK_INIT(dsi_esc_clk.c),
	},
};

static struct pcom_clk dsi_pixel_clk = {
	.id = P_DSI_PIXEL_CLK,
	.c = {
		.ops = &clk_ops_pcom_ext_config,
		.dbg_name = "dsi_pixel_clk",
		CLK_INIT(dsi_pixel_clk.c),
	},
};

static DEFINE_CLK_PCOM(dsi_ref_clk,	DSI_REF_CLK,	0);
static DEFINE_CLK_PCOM(ebi1_clk,	EBI1_CLK,
		CLKFLAG_SKIP_AUTO_OFF | CLKFLAG_MIN);
static DEFINE_CLK_PCOM(ebi2_clk,	EBI2_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(ecodec_clk,	ECODEC_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(emdh_clk,	EMDH_CLK,   CLKFLAG_MIN | CLKFLAG_MAX);
static DEFINE_CLK_PCOM(gp_clk,		GP_CLK,		CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(grp_2d_clk,	GRP_2D_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(grp_2d_p_clk,	GRP_2D_P_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(grp_3d_clk,	GRP_3D_CLK,	0);
static DEFINE_CLK_PCOM(grp_3d_p_clk,	GRP_3D_P_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(gsbi1_qup_clk,	GSBI1_QUP_CLK,	0);
static DEFINE_CLK_PCOM(gsbi1_qup_p_clk,	GSBI1_QUP_P_CLK, 0);
static DEFINE_CLK_PCOM(gsbi2_qup_clk,	GSBI2_QUP_CLK,	0);
static DEFINE_CLK_PCOM(gsbi2_qup_p_clk,	GSBI2_QUP_P_CLK, 0);
static DEFINE_CLK_PCOM(gsbi_clk,	GSBI_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(gsbi_p_clk,	GSBI_P_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(hdmi_clk,	HDMI_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(i2c_clk,		I2C_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(icodec_rx_clk,	ICODEC_RX_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(icodec_tx_clk,	ICODEC_TX_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(imem_clk,	IMEM_CLK,	0);
static DEFINE_CLK_PCOM(mdc_clk,		MDC_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(mdp_clk,		MDP_CLK,	CLKFLAG_MIN);
static DEFINE_CLK_PCOM(mdp_lcdc_pad_pclk_clk, MDP_LCDC_PAD_PCLK_CLK,
		CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(mdp_lcdc_pclk_clk, MDP_LCDC_PCLK_CLK,
		CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(mdp_vsync_clk,	MDP_VSYNC_CLK,	0);
static DEFINE_CLK_PCOM(mdp_dsi_p_clk,	MDP_DSI_P_CLK,	0);
static DEFINE_CLK_PCOM(pbus_clk,	PBUS_CLK,
		CLKFLAG_SKIP_AUTO_OFF | CLKFLAG_MIN);
static DEFINE_CLK_PCOM(pcm_clk,		PCM_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(pmdh_clk,	PMDH_CLK,   CLKFLAG_MIN | CLKFLAG_MAX);
static DEFINE_CLK_PCOM(sdac_clk,	SDAC_CLK,	0);
static DEFINE_CLK_PCOM(sdc1_clk,	SDC1_CLK,	0);
static DEFINE_CLK_PCOM(sdc1_p_clk,	SDC1_P_CLK,	0);
static DEFINE_CLK_PCOM(sdc2_clk,	SDC2_CLK,	0);
static DEFINE_CLK_PCOM(sdc2_p_clk,	SDC2_P_CLK,	0);
static DEFINE_CLK_PCOM(sdc3_clk,	SDC3_CLK,	0);
static DEFINE_CLK_PCOM(sdc3_p_clk,	SDC3_P_CLK,	0);
static DEFINE_CLK_PCOM(sdc4_clk,	SDC4_CLK,	0);
static DEFINE_CLK_PCOM(sdc4_p_clk,	SDC4_P_CLK,	0);
static DEFINE_CLK_PCOM(spi_clk,		SPI_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(tsif_clk,	TSIF_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(tsif_p_clk,	TSIF_P_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(tsif_ref_clk,	TSIF_REF_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(tv_dac_clk,	TV_DAC_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(tv_enc_clk,	TV_ENC_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(uart1_clk,	UART1_CLK,	0);
static DEFINE_CLK_PCOM(uart1dm_clk,	UART1DM_CLK,	0);
static DEFINE_CLK_PCOM(uart2_clk,	UART2_CLK,	0);
static DEFINE_CLK_PCOM(uart2dm_clk,	UART2DM_CLK,	0);
static DEFINE_CLK_PCOM(uart3_clk,	UART3_CLK,	0);
static DEFINE_CLK_PCOM(usb_hs2_clk,	USB_HS2_CLK,	0);
static DEFINE_CLK_PCOM(usb_hs2_p_clk,	USB_HS2_P_CLK,	0);
static DEFINE_CLK_PCOM(usb_hs3_clk,	USB_HS3_CLK,	0);
static DEFINE_CLK_PCOM(usb_hs3_p_clk,	USB_HS3_P_CLK,	0);
static DEFINE_CLK_PCOM(usb_hs_clk,	USB_HS_CLK,	0);
static DEFINE_CLK_PCOM(usb_hs_core_clk,	USB_HS_CORE_CLK, 0);
static DEFINE_CLK_PCOM(usb_hs_p_clk,	USB_HS_P_CLK,	0);
static DEFINE_CLK_PCOM(usb_otg_clk,	USB_OTG_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(usb_phy_clk,	USB_PHY_CLK,	CLKFLAG_SKIP_AUTO_OFF);
static DEFINE_CLK_PCOM(vdc_clk,		VDC_CLK,	CLKFLAG_MIN);
static DEFINE_CLK_PCOM(vfe_axi_clk,	VFE_AXI_CLK,	0);
static DEFINE_CLK_PCOM(vfe_clk,		VFE_CLK,	0);
static DEFINE_CLK_PCOM(vfe_mdc_clk,	VFE_MDC_CLK,	0);

static DEFINE_CLK_VOTER(ebi_acpu_clk,	&ebi1_clk.c);
static DEFINE_CLK_VOTER(ebi_grp_3d_clk,	&ebi1_clk.c);
static DEFINE_CLK_VOTER(ebi_grp_2d_clk,	&ebi1_clk.c);
static DEFINE_CLK_VOTER(ebi_lcdc_clk,	&ebi1_clk.c);
static DEFINE_CLK_VOTER(ebi_mddi_clk,	&ebi1_clk.c);
static DEFINE_CLK_VOTER(ebi_tv_clk,	&ebi1_clk.c);
static DEFINE_CLK_VOTER(ebi_usb_clk,	&ebi1_clk.c);
static DEFINE_CLK_VOTER(ebi_vfe_clk,	&ebi1_clk.c);
static DEFINE_CLK_VOTER(ebi_adm_clk,	&ebi1_clk.c);

static struct clk_lookup msm_clocks_7x01a[] = {
	CLK_LOOKUP("core_clk",		adm_clk.c,	"msm_dmov"),
	CLK_LOOKUP("adsp_clk",		adsp_clk.c,	NULL),
	CLK_LOOKUP("ebi1_clk",		ebi1_clk.c,	NULL),
	CLK_LOOKUP("ebi2_clk",		ebi2_clk.c,	NULL),
	CLK_LOOKUP("ecodec_clk",	ecodec_clk.c,	NULL),
	CLK_LOOKUP("emdh_clk",		emdh_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		gp_clk.c,		NULL),
	CLK_LOOKUP("core_clk",		grp_3d_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("core_clk",		i2c_clk.c,	"msm_i2c.0"),
	CLK_LOOKUP("icodec_rx_clk",	icodec_rx_clk.c,	NULL),
	CLK_LOOKUP("icodec_tx_clk",	icodec_tx_clk.c,	NULL),
	CLK_LOOKUP("mem_clk",		imem_clk.c,	NULL),
	CLK_LOOKUP("mdc_clk",		mdc_clk.c,	NULL),
	CLK_LOOKUP("mddi_clk",		pmdh_clk.c,	NULL),
	CLK_LOOKUP("mdp_clk",		mdp_clk.c,	NULL),
	CLK_LOOKUP("pbus_clk",		pbus_clk.c,	NULL),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,	NULL),
	CLK_LOOKUP("sdac_clk",		sdac_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		sdc1_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("iface_clk",		sdc1_p_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("core_clk",		sdc2_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("iface_clk",		sdc2_p_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("core_clk",		sdc3_clk.c,	"msm_sdcc.3"),
	CLK_LOOKUP("iface_clk",		sdc3_p_clk.c,	"msm_sdcc.3"),
	CLK_LOOKUP("core_clk",		sdc4_clk.c,	"msm_sdcc.4"),
	CLK_LOOKUP("iface_clk",		sdc4_p_clk.c,	"msm_sdcc.4"),
	CLK_LOOKUP("core_clk",		tsif_clk.c,	"msm_tsif.0"),
	CLK_LOOKUP("ref_clk",		tsif_ref_clk.c,	"msm_tsif.0"),
	CLK_LOOKUP("tv_dac_clk",	tv_dac_clk.c,	NULL),
	CLK_LOOKUP("tv_enc_clk",	tv_enc_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		uart1_clk.c,	"msm_serial.0"),
	CLK_LOOKUP("core_clk",		uart2_clk.c,	"msm_serial.1"),
	CLK_LOOKUP("core_clk",		uart3_clk.c,	"msm_serial.2"),
	CLK_LOOKUP("core_clk",		uart1dm_clk.c,	"msm_serial_hs.0"),
	CLK_LOOKUP("core_clk",		uart2dm_clk.c,	"msm_serial_hs.1"),
	CLK_LOOKUP("alt_core_clk",	usb_hs_clk.c,	"msm_otg"),
	CLK_LOOKUP("iface_clk",		usb_hs_p_clk.c,	"msm_otg"),
	CLK_LOOKUP("alt_core_clk",	usb_hs_clk.c,	"msm_hsusb_otg"),
	CLK_LOOKUP("iface_clk",		usb_hs_p_clk.c,	"msm_hsusb_otg"),
	CLK_LOOKUP("alt_core_clk",	usb_hs_clk.c,	"msm_hsusb_peripheral"),
	CLK_LOOKUP("iface_clk",		usb_hs_p_clk.c,	"msm_hsusb_peripheral"),
	CLK_LOOKUP("alt_core_clk",	usb_otg_clk.c,	NULL),
	CLK_LOOKUP("vdc_clk",		vdc_clk.c,	NULL),
	CLK_LOOKUP("vfe_clk",		vfe_clk.c,	NULL),
	CLK_LOOKUP("vfe_mdc_clk",	vfe_mdc_clk.c,	NULL),
};

struct clock_init_data msm7x01a_clock_init_data __initdata = {
	.table = msm_clocks_7x01a,
	.size = ARRAY_SIZE(msm_clocks_7x01a),
};

static struct clk_lookup msm_clocks_7x27[] = {
	CLK_LOOKUP("core_clk",		adm_clk.c,	"msm_dmov"),
	CLK_LOOKUP("adsp_clk",		adsp_clk.c,	NULL),
	CLK_LOOKUP("ebi1_clk",		ebi1_clk.c,	NULL),
	CLK_LOOKUP("ebi2_clk",		ebi2_clk.c,	NULL),
	CLK_LOOKUP("ecodec_clk",	ecodec_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		gp_clk.c,		NULL),
	CLK_LOOKUP("core_clk",		grp_3d_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("core_clk",		grp_3d_clk.c,	"footswitch-pcom.2"),
	CLK_LOOKUP("iface_clk",		grp_3d_p_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("core_clk",		i2c_clk.c,	"msm_i2c.0"),
	CLK_LOOKUP("iface_clk",		grp_3d_p_clk.c,	"footswitch-pcom.2"),
	CLK_LOOKUP("icodec_rx_clk",	icodec_rx_clk.c,	NULL),
	CLK_LOOKUP("icodec_tx_clk",	icodec_tx_clk.c,	NULL),
	CLK_LOOKUP("mem_clk",		imem_clk.c,	NULL),
	CLK_LOOKUP("mdc_clk",		mdc_clk.c,	NULL),
	CLK_LOOKUP("mddi_clk",		pmdh_clk.c,	NULL),
	CLK_LOOKUP("mdp_clk",		mdp_clk.c,	NULL),
	CLK_LOOKUP("mdp_lcdc_pclk_clk", mdp_lcdc_pclk_clk.c, NULL),
	CLK_LOOKUP("mdp_lcdc_pad_pclk_clk", mdp_lcdc_pad_pclk_clk.c, NULL),
	CLK_LOOKUP("mdp_vsync_clk",	mdp_vsync_clk.c,  NULL),
	CLK_LOOKUP("pbus_clk",		pbus_clk.c,	NULL),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,	NULL),
	CLK_LOOKUP("sdac_clk",		sdac_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		sdc1_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("iface_clk",		sdc1_p_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("core_clk",		sdc2_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("iface_clk",		sdc2_p_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("core_clk",		sdc3_clk.c,	"msm_sdcc.3"),
	CLK_LOOKUP("iface_clk",		sdc3_p_clk.c,	"msm_sdcc.3"),
	CLK_LOOKUP("core_clk",		sdc4_clk.c,	"msm_sdcc.4"),
	CLK_LOOKUP("iface_clk",		sdc4_p_clk.c,	"msm_sdcc.4"),
	CLK_LOOKUP("core_clk",		tsif_clk.c,	"msm_tsif.0"),
	CLK_LOOKUP("ref_clk",		tsif_ref_clk.c,	"msm_tsif.0"),
	CLK_LOOKUP("iface_clk",		tsif_p_clk.c,	"msm_tsif.0"),
	CLK_LOOKUP("core_clk",		uart1_clk.c,	"msm_serial.0"),
	CLK_LOOKUP("core_clk",		uart2_clk.c,	"msm_serial.1"),
	CLK_LOOKUP("core_clk",		uart1dm_clk.c,	"msm_serial_hs.0"),
	CLK_LOOKUP("core_clk",		uart2dm_clk.c,	"msm_serial_hs.1"),
	CLK_LOOKUP("alt_core_clk",	usb_hs_clk.c,	"msm_otg"),
	CLK_LOOKUP("iface_clk",		usb_hs_p_clk.c,	"msm_otg"),
	CLK_LOOKUP("alt_core_clk",	usb_otg_clk.c,	NULL),
	CLK_LOOKUP("phy_clk",		usb_phy_clk.c,	"msm_otg"),
	CLK_LOOKUP("vdc_clk",		vdc_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		vdc_clk.c,	"footswitch-pcom.7"),
	CLK_LOOKUP("vfe_clk",		vfe_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		vfe_clk.c,	"footswitch-pcom.8"),
	CLK_LOOKUP("vfe_mdc_clk",	vfe_mdc_clk.c,	NULL),

	CLK_LOOKUP("ebi1_acpu_clk",	ebi_acpu_clk.c,	NULL),
	CLK_LOOKUP("bus_clk",		ebi_grp_3d_clk.c, "kgsl-3d0.0"),
	CLK_LOOKUP("ebi1_lcdc_clk",	ebi_lcdc_clk.c,	NULL),
	CLK_LOOKUP("ebi1_mddi_clk",	ebi_mddi_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		ebi_usb_clk.c,	"msm_otg"),
	CLK_LOOKUP("ebi1_vfe_clk",	ebi_vfe_clk.c,	NULL),
	CLK_LOOKUP("mem_clk",		ebi_adm_clk.c,	"msm_dmov"),
};

struct clock_init_data msm7x27_clock_init_data __initdata = {
	.table = msm_clocks_7x27,
	.size = ARRAY_SIZE(msm_clocks_7x27),
};

static struct clk_lookup msm_clocks_7x27a[] = {
	CLK_LOOKUP("core_clk",		adm_clk.c,	"msm_dmov"),
	CLK_LOOKUP("adsp_clk",		adsp_clk.c,	NULL),
	CLK_LOOKUP("ahb_m_clk",		ahb_m_clk.c,	NULL),
	CLK_LOOKUP("ahb_s_clk",		ahb_s_clk.c,	NULL),
	CLK_LOOKUP("cam_m_clk",		cam_m_clk.c,	NULL),
	CLK_LOOKUP("csi_clk",		csi0_clk.c,	"msm_camera_ov9726.0"),
	CLK_LOOKUP("csi_pclk",		csi0_p_clk.c,	"msm_camera_ov9726.0"),
	CLK_LOOKUP("csi_vfe_clk",	csi0_vfe_clk.c,	"msm_camera_ov9726.0"),
	CLK_LOOKUP("csi_clk",		csi0_clk.c,	"msm_camera_ov7692.0"),
	CLK_LOOKUP("csi_pclk",		csi0_p_clk.c,	"msm_camera_ov7692.0"),
	CLK_LOOKUP("csi_vfe_clk",	csi0_vfe_clk.c,	"msm_camera_ov7692.0"),
	CLK_LOOKUP("csi_clk",		csi1_clk.c,	NULL),
	CLK_LOOKUP("csi_pclk",		csi1_p_clk.c,	NULL),
	CLK_LOOKUP("csi_vfe_clk",	csi1_vfe_clk.c,	NULL),
	CLK_LOOKUP("csi_clk",		csi0_clk.c,	"msm_csic.0"),
	CLK_LOOKUP("csi_pclk",		csi0_p_clk.c,   "msm_csic.0"),
	CLK_LOOKUP("csi_vfe_clk",	csi0_vfe_clk.c,	"msm_csic.0"),
	CLK_LOOKUP("csi_clk",		csi1_clk.c,	"msm_csic.1"),
	CLK_LOOKUP("csi_pclk",		csi1_p_clk.c,	"msm_csic.1"),
	CLK_LOOKUP("csi_vfe_clk",	csi1_vfe_clk.c,	"msm_csic.1"),
	CLK_LOOKUP("dsi_byte_clk",	dsi_byte_clk.c,	NULL),
	CLK_LOOKUP("dsi_clk",		dsi_clk.c,	NULL),
	CLK_LOOKUP("dsi_esc_clk",	dsi_esc_clk.c,	NULL),
	CLK_LOOKUP("dsi_pixel_clk",	dsi_pixel_clk.c, NULL),
	CLK_LOOKUP("dsi_ref_clk",	dsi_ref_clk.c,	NULL),
	CLK_LOOKUP("ebi1_clk",		ebi1_clk.c,	NULL),
	CLK_LOOKUP("ebi2_clk",		ebi2_clk.c,	NULL),
	CLK_LOOKUP("ecodec_clk",	ecodec_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		gp_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		grp_3d_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("core_clk",		grp_3d_clk.c,	"footswitch-pcom.2"),
	CLK_LOOKUP("iface_clk",		grp_3d_p_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("iface_clk",		grp_3d_p_clk.c,	"footswitch-pcom.2"),
	CLK_LOOKUP("core_clk",		gsbi1_qup_clk.c, "qup_i2c.0"),
	CLK_LOOKUP("core_clk",		gsbi2_qup_clk.c, "qup_i2c.1"),
	CLK_LOOKUP("iface_clk",		gsbi1_qup_p_clk.c, "qup_i2c.0"),
	CLK_LOOKUP("iface_clk",		gsbi2_qup_p_clk.c, "qup_i2c.1"),
	CLK_LOOKUP("icodec_rx_clk",	icodec_rx_clk.c, NULL),
	CLK_LOOKUP("icodec_tx_clk",	icodec_tx_clk.c, NULL),
	CLK_LOOKUP("mem_clk",		imem_clk.c,	NULL),
	CLK_LOOKUP("mddi_clk",		pmdh_clk.c,	NULL),
	CLK_LOOKUP("mdp_clk",		mdp_clk.c,	NULL),
	CLK_LOOKUP("mdp_lcdc_pclk_clk",	mdp_lcdc_pclk_clk.c, NULL),
	CLK_LOOKUP("mdp_lcdc_pad_pclk_clk", mdp_lcdc_pad_pclk_clk.c, NULL),
	CLK_LOOKUP("mdp_vsync_clk",	mdp_vsync_clk.c,	NULL),
	CLK_LOOKUP("mdp_dsi_pclk",	mdp_dsi_p_clk.c,	NULL),
	CLK_LOOKUP("pbus_clk",		pbus_clk.c,	NULL),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,	NULL),
	CLK_LOOKUP("sdac_clk",		sdac_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		sdc1_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("iface_clk",		sdc1_p_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("core_clk",		sdc2_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("iface_clk",		sdc2_p_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("core_clk",		sdc3_clk.c,	"msm_sdcc.3"),
	CLK_LOOKUP("iface_clk",		sdc3_p_clk.c,	"msm_sdcc.3"),
	CLK_LOOKUP("core_clk",		sdc4_clk.c,	"msm_sdcc.4"),
	CLK_LOOKUP("iface_clk",		sdc4_p_clk.c,	"msm_sdcc.4"),
	CLK_LOOKUP("ref_clk",		tsif_ref_clk.c,	"msm_tsif.0"),
	CLK_LOOKUP("iface_clk",		tsif_p_clk.c,	"msm_tsif.0"),
	CLK_LOOKUP("core_clk",		uart1_clk.c,	"msm_serial.0"),
	CLK_LOOKUP("core_clk",		uart2_clk.c,	"msm_serial.1"),
	CLK_LOOKUP("core_clk",		uart1dm_clk.c,	"msm_serial_hs.0"),
	CLK_LOOKUP("core_clk",		uart2dm_clk.c,	"msm_serial_hsl.0"),
	CLK_LOOKUP("core_clk",		usb_hs_core_clk.c, "msm_otg"),
	CLK_LOOKUP("alt_core_clk",	usb_hs_clk.c,	"msm_otg"),
	CLK_LOOKUP("iface_clk",		usb_hs_p_clk.c,	"msm_otg"),
	CLK_LOOKUP("phy_clk",		usb_phy_clk.c,	"msm_otg"),
	CLK_LOOKUP("alt_core_clk",	usb_hs2_clk.c,	"msm_hsusb_host.0"),
	CLK_LOOKUP("vdc_clk",		vdc_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		vdc_clk.c,	"footswitch-pcom.7"),
	CLK_LOOKUP("vfe_clk",		vfe_clk.c,	NULL),
	CLK_LOOKUP("vfe_clk",		vfe_clk.c,	"msm_vfe.0"),
	CLK_LOOKUP("core_clk",		vfe_clk.c,	"footswitch-pcom.8"),
	CLK_LOOKUP("vfe_mdc_clk",	vfe_mdc_clk.c,	NULL),

	CLK_LOOKUP("ebi1_acpu_clk",	ebi_acpu_clk.c,	NULL),
	CLK_LOOKUP("bus_clk",		ebi_grp_3d_clk.c, "kgsl-3d0.0"),
	CLK_LOOKUP("ebi1_lcdc_clk",	ebi_lcdc_clk.c,	NULL),
	CLK_LOOKUP("ebi1_mddi_clk",	ebi_mddi_clk.c,	NULL),
	CLK_LOOKUP("ebi1_vfe_clk",	ebi_vfe_clk.c,	NULL),
	CLK_LOOKUP("mem_clk",		ebi_adm_clk.c,	"msm_dmov"),
};

struct clock_init_data msm7x27a_clock_init_data __initdata = {
	.table = msm_clocks_7x27a,
	.size = ARRAY_SIZE(msm_clocks_7x27a),
};

static struct clk_lookup msm_clocks_8x50[] = {
	CLK_LOOKUP("core_clk",		adm_clk.c,	"msm_dmov"),
	CLK_LOOKUP("core_clk",		ce_clk.c,	"qce.0"),
	CLK_LOOKUP("ebi1_clk",		ebi1_clk.c,	NULL),
	CLK_LOOKUP("ebi2_clk",		ebi2_clk.c,	NULL),
	CLK_LOOKUP("ecodec_clk",	ecodec_clk.c,	NULL),
	CLK_LOOKUP("emdh_clk",		emdh_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		gp_clk.c,		NULL),
	CLK_LOOKUP("core_clk",		grp_3d_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("core_clk",		i2c_clk.c,	"msm_i2c.0"),
	CLK_LOOKUP("icodec_rx_clk",	icodec_rx_clk.c,	NULL),
	CLK_LOOKUP("icodec_tx_clk",	icodec_tx_clk.c,	NULL),
	CLK_LOOKUP("mem_clk",		imem_clk.c,	NULL),
	CLK_LOOKUP("mdc_clk",		mdc_clk.c,	NULL),
	CLK_LOOKUP("mddi_clk",		pmdh_clk.c,	NULL),
	CLK_LOOKUP("mdp_clk",		mdp_clk.c,	NULL),
	CLK_LOOKUP("mdp_lcdc_pclk_clk", mdp_lcdc_pclk_clk.c, NULL),
	CLK_LOOKUP("mdp_lcdc_pad_pclk_clk", mdp_lcdc_pad_pclk_clk.c, NULL),
	CLK_LOOKUP("mdp_vsync_clk",	mdp_vsync_clk.c,	NULL),
	CLK_LOOKUP("pbus_clk",		pbus_clk.c,	NULL),
	CLK_LOOKUP("pcm_clk",		pcm_clk.c,	NULL),
	CLK_LOOKUP("sdac_clk",		sdac_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		sdc1_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("iface_clk",		sdc1_p_clk.c,	"msm_sdcc.1"),
	CLK_LOOKUP("core_clk",		sdc2_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("iface_clk",		sdc2_p_clk.c,	"msm_sdcc.2"),
	CLK_LOOKUP("core_clk",		sdc3_clk.c,	"msm_sdcc.3"),
	CLK_LOOKUP("iface_clk",		sdc3_p_clk.c,	"msm_sdcc.3"),
	CLK_LOOKUP("core_clk",		sdc4_clk.c,	"msm_sdcc.4"),
	CLK_LOOKUP("iface_clk",		sdc4_p_clk.c,	"msm_sdcc.4"),
	CLK_LOOKUP("core_clk",		spi_clk.c,	"spi_qsd.0"),
	CLK_DUMMY("iface_clk",		SPI_P_CLK,	"spi_qsd.0", 0),
	CLK_LOOKUP("core_clk",		tsif_clk.c,	"msm_tsif.0"),
	CLK_LOOKUP("ref_clk",		tsif_ref_clk.c,	"msm_tsif.0"),
	CLK_LOOKUP("tv_dac_clk",	tv_dac_clk.c,	NULL),
	CLK_LOOKUP("tv_enc_clk",	tv_enc_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		uart1_clk.c,	"msm_serial.0"),
	CLK_LOOKUP("core_clk",		uart2_clk.c,	"msm_serial.1"),
	CLK_LOOKUP("core_clk",		uart3_clk.c,	"msm_serial.2"),
	CLK_LOOKUP("core_clk",		uart1dm_clk.c,	"msm_serial_hs.0"),
	CLK_LOOKUP("core_clk",		uart2dm_clk.c,	"msm_serial_hs.1"),
	CLK_LOOKUP("alt_core_clk",	usb_hs_clk.c,	"msm_otg"),
	CLK_LOOKUP("iface_clk",		usb_hs_p_clk.c,	"msm_otg"),
	CLK_LOOKUP("alt_core_clk",	usb_otg_clk.c,	NULL),
	CLK_LOOKUP("vdc_clk",		vdc_clk.c,	NULL),
	CLK_LOOKUP("vfe_clk",		vfe_clk.c,	NULL),
	CLK_LOOKUP("vfe_mdc_clk",	vfe_mdc_clk.c,	NULL),
	CLK_LOOKUP("vfe_axi_clk",	vfe_axi_clk.c,	NULL),
	CLK_LOOKUP("alt_core_clk",	usb_hs2_clk.c,	 "msm_hsusb_host.0"),
	CLK_LOOKUP("iface_clk",		usb_hs2_p_clk.c, "msm_hsusb_host.0"),
	CLK_LOOKUP("alt_core_clk",	usb_hs3_clk.c,	 NULL),
	CLK_LOOKUP("iface_clk",		usb_hs3_p_clk.c, NULL),
	CLK_LOOKUP("phy_clk",		usb_phy_clk.c,	 "msm_otg"),

	CLK_LOOKUP("ebi1_acpu_clk",	ebi_acpu_clk.c,	NULL),
	CLK_LOOKUP("bus_clk",		ebi_grp_3d_clk.c, "kgsl-3d0.0"),
	CLK_LOOKUP("bus_clk",		ebi_grp_2d_clk.c, "kgsl-2d0.0"),
	CLK_LOOKUP("ebi1_lcdc_clk",	ebi_lcdc_clk.c,	NULL),
	CLK_LOOKUP("ebi1_mddi_clk",	ebi_mddi_clk.c,	NULL),
	CLK_LOOKUP("ebi1_tv_clk",	ebi_tv_clk.c,	NULL),
	CLK_LOOKUP("core_clk",		ebi_usb_clk.c,	"msm_otg"),
	CLK_LOOKUP("core_clk",		ebi_usb_clk.c,	"msm_hsusb_host.0"),
	CLK_LOOKUP("ebi1_vfe_clk",	ebi_vfe_clk.c,	NULL),
	CLK_LOOKUP("mem_clk",		ebi_adm_clk.c,	"msm_dmov"),

	CLK_LOOKUP("iface_clk",		grp_3d_p_clk.c,	"kgsl-3d0.0"),
	CLK_LOOKUP("core_clk",		grp_2d_clk.c,	"kgsl-2d0.0"),
	CLK_LOOKUP("iface_clk",		grp_2d_p_clk.c,	"kgsl-2d0.0"),
	CLK_LOOKUP("core_clk",		gsbi_clk.c,	"qup_i2c.4"),
	CLK_LOOKUP("iface_clk",		gsbi_p_clk.c,	"qup_i2c.4"),
};

struct clock_init_data qds8x50_clock_init_data __initdata = {
	.table = msm_clocks_8x50,
	.size = ARRAY_SIZE(msm_clocks_8x50),
};
