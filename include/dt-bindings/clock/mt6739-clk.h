/*
  * Copyright (C) 2016 MediaTek Inc.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License version 2 as
  * published by the Free Software Foundation.
  *
  * This program is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
  */


#ifndef _DT_BINDINGS_CLK_MT6739_H
#define _DT_BINDINGS_CLK_MT6739_H

#define	CLK_TOP_CLK26M		0

/* APMIXEDSYS */
#define CLK_APMIXED_ARMPLL_LL		0	/* armpll_ll		ARMPLL_LL */
#define CLK_APMIXED_MAINPLL		1	/* mainpll		MAINPLL */
#define CLK_APMIXED_MFGPLL		2	/* mfgpll		MFGPLL */
#define CLK_APMIXED_MMPLL		3	/* mmpll		MMPLL */
#define CLK_APMIXED_UNIVPLL		4	/* univpll		UNIVPLL */
#define CLK_APMIXED_MSDCPLL		5	/* msdcpll		MSDCPLL */
#define CLK_APMIXED_APLL1		6	/* apll1		APLL1 */
#define CLK_APMIXED_NR_CLK		7

/* TOPCK */

#define CLK_TOP_SYSPLL			0	/* syspll_ck		syspll_ck */
#define CLK_TOP_SYSPLL_D2		1	/* syspll_d2		syspll_d2 */
#define CLK_TOP_SYSPLL1_D2		2	/* syspll1_d2		syspll1_d2 */
#define CLK_TOP_SYSPLL1_D4		3	/* syspll1_d4		syspll1_d4 */
#define CLK_TOP_SYSPLL1_D8		4	/* syspll1_d8		syspll1_d8 */
#define CLK_TOP_SYSPLL1_D16		5	/* syspll1_d16		syspll1_d16 */
#define CLK_TOP_SYSPLL_D3		6	/* syspll_d3		syspll_d3 */
#define CLK_TOP_SYSPLL2_D2		7	/* syspll2_d2		syspll2_d2 */
#define CLK_TOP_SYSPLL2_D4		8	/* syspll2_d4		syspll2_d4 */
#define CLK_TOP_SYSPLL_D5		9	/* syspll_d5		syspll_d5 */
#define CLK_TOP_SYSPLL3_D2		10	/* syspll3_d2		syspll3_d2 */
#define CLK_TOP_SYSPLL3_D4		11	/* syspll3_d4		syspll3_d4 */
#define CLK_TOP_SYSPLL_D7		12	/* syspll_d7		syspll_d7 */
#define CLK_TOP_SYSPLL4_D2		13	/* syspll4_d2		syspll4_d2 */
#define CLK_TOP_SYSPLL4_D4		14	/* syspll4_d4		syspll4_d4 */
#define CLK_TOP_UNIVPLL			15	/* univpll_ck		univpll_ck */
#define CLK_TOP_UNIVPLL_D26		16	/* univpll_d26		univpll_d26 */
#define CLK_TOP_UNIVPLL_48M_D2		17	/* univpll_48m_d2	univpll_48m_d2 */
#define CLK_TOP_UNIVPLL_48M_D4		18	/* univpll_48m_d4	univpll_48m_d4 */
#define CLK_TOP_UNIVPLL_48M_D8		19	/* univpll_48m_d8	univpll_48m_d8 */
#define CLK_TOP_UNIVPLL_D2		20	/* univpll_d2		univpll_d2 */
#define CLK_TOP_UNIVPLL1_D2		21	/* univpll1_d2		univpll1_d2 */
#define CLK_TOP_UNIVPLL1_D4		22	/* univpll1_d4		univpll1_d4 */
#define CLK_TOP_UNIVPLL_D3		23	/* univpll_d3		univpll_d3 */
#define CLK_TOP_UNIVPLL2_D2		24	/* univpll2_d2		univpll2_d2 */
#define CLK_TOP_UNIVPLL2_D4		25	/* univpll2_d4		univpll2_d4 */
#define CLK_TOP_UNIVPLL2_D8		26	/* univpll2_d8		univpll2_d8 */
#define CLK_TOP_UNIVPLL2_D32		27	/* univpll2_d32		univpll2_d32 */
#define CLK_TOP_UNIVPLL_D5		28	/* univpll_d5		univpll_d5 */
#define CLK_TOP_UNIVPLL3_D2		29	/* univpll3_d2		univpll3_d2 */
#define CLK_TOP_UNIVPLL3_D4		30	/* univpll3_d4		univpll3_d4 */
#define CLK_TOP_UNIVPLL3_D8		31	/* univpll3_d8		univpll3_d8 */
#define CLK_TOP_MMPLL			32	/* mmpll_ck		mmpll_ck */
#define CLK_TOP_VENCPLL			33	/* vencpll_ck		vencpll_ck */
#define CLK_TOP_MSDCPLL			34	/* msdcpll_ck		msdcpll_ck */
#define CLK_TOP_MSDCPLL_D2		35	/* msdcpll_d2		msdcpll_d2 */
#define CLK_TOP_APLL1			36	/* apll1_ck		apll1_ck */
#define CLK_TOP_APLL1_D2		37	/* apll1_d2		apll1_d2 */
#define CLK_TOP_APLL1_D4		38	/* apll1_d4		apll1_d4 */
#define CLK_TOP_APLL1_D8		39	/* apll1_d8		apll1_d8 */
#define CLK_TOP_DMPLL			40	/* dmpll_ck		dmpll_ck */
#define CLK_TOP_AXI_SEL			41	/* axi_sel		axi_sel */
#define CLK_TOP_MEM_SEL			42	/* mem_sel		mem_sel */
#define CLK_TOP_DDRPHYCFG_SEL		43	/* ddrphycfg_sel	ddrphycfg_sel */
#define CLK_TOP_MM_SEL			44	/* mm_sel		mm_sel */
#define CLK_TOP_MFG_SEL			45	/* mfg_sel		mfg_sel */
#define CLK_TOP_CAMTG_SEL		46	/* camtg_sel		camtg_sel */
#define CLK_TOP_UART_SEL		47	/* uart_sel		uart_sel */
#define CLK_TOP_SPI_SEL			48	/* spi_sel		spi_sel */
#define CLK_TOP_MSDC50_0_HCLK_SEL	49	/* msdc5hclk		msdc50_0_hclk_sel */
#define CLK_TOP_MSDC50_0_SEL		50	/* msdc50_0_sel		msdc50_0_sel */
#define CLK_TOP_MSDC30_1_SEL		51	/* msdc30_1_sel		msdc30_1_sel */
#define CLK_TOP_AUDIO_SEL		52	/* audio_sel		audio_sel */
#define CLK_TOP_AUD_INTBUS_SEL		53	/* aud_intbus_sel	aud_intbus_sel */
#define CLK_TOP_DBI0_SEL		54	/* dbi0_sel		dbi0_sel */
#define CLK_TOP_SCAM_SEL		55	/* scam_sel		scam_sel */
#define CLK_TOP_AUD_1_SEL		56	/* aud_1_sel		aud_1_sel */
#define CLK_TOP_DISP_PWM_SEL		57	/* disp_pwm_sel		disp_pwm_sel */
#define CLK_TOP_NFI2X_SEL		58	/* nfi2x_sel		nfi2x_sel */
#define CLK_TOP_NFIECC_SEL		59	/* nfiecc_sel		nfiecc_sel */
#define CLK_TOP_USB_TOP_SEL		60	/* usb_top_sel		usb_top_sel */
#define CLK_TOP_SPM_SEL			61	/* spm_sel		spm_sel */
#define CLK_TOP_I2C_SEL			62	/* i2c_sel		i2c_sel */
#define CLK_TOP_SENIF_SEL		63	/* senif_sel		senif_sel */
#define CLK_TOP_DXCC_SEL		64	/* dxcc_sel		dxcc_sel */
#define CLK_TOP_CAMTG2_SEL		65	/* camtg2_sel		camtg2_sel */
#define CLK_TOP_AUD_ENGEN1_SEL		66	/* aud_engen1_sel	aud_engen1_sel */
#define CLK_TOP_NR_CLK			67

/* AUDIO */

#define CLK_AUDIO_AFE			0	/* aud_afe		PDN_AFE */
#define CLK_AUDIO_22M			1	/* aud_22m		PDN_22M */
#define CLK_AUDIO_APLL_TUNER		2	/* aud_apll_tuner	PDN_APLL_TUNER */
#define CLK_AUDIO_ADC			3	/* aud_adc		PDN_ADC */
#define CLK_AUDIO_DAC			4	/* aud_dac		PDN_DAC */
#define CLK_AUDIO_DAC_PREDIS		5	/* aud_dac_predis	PDN_DAC_PREDIS */
#define CLK_AUDIO_TML			6	/* aud_tml		PDN_TML */
#define CLK_AUDIO_I2S1_BCLK		7	/* aud_i2s1_bclk	I2S1_BCLK_SW_CG */
#define CLK_AUDIO_I2S2_BCLK		8	/* aud_i2s2_bclk	I2S2_BCLK_SW_CG */
#define CLK_AUDIO_I2S3_BCLK		9	/* aud_i2s3_bclk	I2S3_BCLK_SW_CG */
#define CLK_AUDIO_I2S4_BCLK		10	/* aud_i2s4_bclk	I2S4_BCLK_SW_CG */
#define CLK_AUDIO_NR_CLK		11

/* IMGSYS */

#define CLK_IMG_LARB2_SMI		0	/* img_larb2_smi	LARB2_SMI_CKPDN */
#define CLK_IMG_CAM_SMI			1	/* img_cam_smi		CAM_SMI_CKPDN */
#define CLK_IMG_CAM_CAM			2	/* img_cam_cam		CAM_CAM_CKPDN */
#define CLK_IMG_SEN_TG			3	/* img_sen_tg		SEN_TG_CKPDN */
#define CLK_IMG_SEN_CAM			4	/* img_sen_cam		SEN_CAM_CKPDN */
#define CLK_IMG_CAM_SV			5	/* img_cam_sv		CAM_SV_CKPDN */
#define CLK_IMG_SUFOD			6	/* img_sufod		SUFOD_CKPDN */
#define CLK_IMG_FD			7	/* img_fd		FD_CKPDN */
#define CLK_IMG_NR_CLK			8

/* INFRACFG_AO */

#define CLK_INFRA_PMIC_TMR		0	/* infra_pmic_tmr	PMIC_CG_TMR */
#define CLK_INFRA_PMIC_AP		1	/* infra_pmic_ap	PMIC_CG_AP */
#define CLK_INFRA_PMIC_MD		2	/* infra_pmic_md	PMIC_CG_MD */
#define CLK_INFRA_PMIC_CONN		3	/* infra_pmic_conn	PMIC_CG_CONN */
#define CLK_INFRA_SEJ			4	/* infra_sej		SEJ_CG */
#define CLK_INFRA_APXGPT		5	/* infra_apxgpt		APXGPT_CG */
#define CLK_INFRA_ICUSB			6	/* infra_icusb		ICUSB_CG */
#define CLK_INFRA_GCE			7	/* infra_gce		GCE_CG */
#define CLK_INFRA_THERM			8	/* infra_therm		THERM_CG */
#define CLK_INFRA_I2C0			9	/* infra_i2c0		I2C0_CG */
#define CLK_INFRA_I2C1			10	/* infra_i2c1		I2C1_CG */
#define CLK_INFRA_I2C2			11	/* infra_i2c2		I2C2_CG */
#define CLK_INFRA_I2C3			12	/* infra_i2c3		I2C3_CG */
#define CLK_INFRA_PWM_HCLK		13	/* infra_pwm_hclk	PWM_HCLK_CG */
#define CLK_INFRA_PWM1			14	/* infra_pwm1		PWM1_CG */
#define CLK_INFRA_PWM2			15	/* infra_pwm2		PWM2_CG */
#define CLK_INFRA_PWM3			16	/* infra_pwm3		PWM3_CG */
#define CLK_INFRA_PWM4			17	/* infra_pwm4		PWM4_CG */
#define CLK_INFRA_PWM5			18	/* infra_pwm5		PWM5_CG */
#define CLK_INFRA_PWM			19	/* infra_pwm		PWM_CG */
#define CLK_INFRA_UART0			20	/* infra_uart0		UART0_CG */
#define CLK_INFRA_UART1			21	/* infra_uart1		UART1_CG */
#define CLK_INFRA_UART2			22	/* infra_uart2		UART2_CG */
#define CLK_INFRA_UART3			23	/* infra_uart3		UART3_CG */
#define CLK_INFRA_GCE_26M		24	/* infra_gce_26m	GCE_26M */
#define CLK_INFRA_CQ_DMA_FPC		25	/* infra_dma		CQ_DMA_FPC */
#define CLK_INFRA_BTIF			26	/* infra_btif		BTIF_CG */
#define CLK_INFRA_SPI0			27	/* infra_spi0		SPI0_CG */
#define CLK_INFRA_MSDC0			28	/* infra_msdc0		MSDC0_CG */
#define CLK_INFRA_MSDC1			29	/* infra_msdc1		MSDC1_CG */
#define CLK_INFRA_NFIECC_312M		30	/* infra_nfiecc		NFIECC_312M_CG */
#define CLK_INFRA_DVFSRC		31	/* infra_dvfsrc		DVFSRC_CG */
#define CLK_INFRA_GCPU			32	/* infra_gcpu		GCPU_CG */
#define CLK_INFRA_TRNG			33	/* infra_trng		TRNG_CG */
#define CLK_INFRA_AUXADC		34	/* infra_auxadc		AUXADC_CG */
#define CLK_INFRA_CPUM			35	/* infra_cpum		CPUM_CG */
#define CLK_INFRA_CCIF1_AP		36	/* infra_ccif1_ap	CCIF1_AP_CG */
#define CLK_INFRA_CCIF1_MD		37	/* infra_ccif1_md	CCIF1_MD_CG */
#define CLK_INFRA_AUXADC_MD		38	/* infra_auxadc_md	AUXADC_MD_CG */
#define CLK_INFRA_NFI			39	/* infra_nfi		NFI_CG */
#define CLK_INFRA_AP_DMA		40	/* infra_ap_dma		AP_DMA_CG */
#define CLK_INFRA_XIU			41	/* infra_xiu		XIU_CG */
#define CLK_INFRA_DEVICE_APC		42	/* infra_dapc		DEVICE_APC_CG */
#define CLK_INFRA_CCIF_AP		43	/* infra_ccif_ap	CCIF_AP_CG */
#define CLK_INFRA_DEBUGSYS		44	/* infra_debugsys	DEBUGSYS_CG */
#define CLK_INFRA_AUDIO			45	/* infra_audio		AUDIO_CG */
#define CLK_INFRA_CCIF_MD		46	/* infra_ccif_md	CCIF_MD_CG */
#define CLK_INFRA_DXCC_SEC_CORE		47	/* infra_secore		DXCC_SEC_CORE_CG */
#define CLK_INFRA_DXCC_AO		48	/* infra_dxcc_ao	DXCC_AO_CG */
#define CLK_INFRA_DRAMC_F26M		49	/* infra_dramc26	DRAMC_F26M_CG */
#define CLK_INFRA_RG_PWM_FBCLK6		50	/* infra_pwmfb		RG_PWM_FBCLK6_CK_CG */
#define CLK_INFRA_DISP_PWM		51	/* infra_disp_pwm	DISP_PWM_CG */
#define CLK_INFRA_CLDMA_BCLK		52	/* infra_cldmabclk	CLDMA_BCLK_CK */
#define CLK_INFRA_AUDIO_26M_BCLK	53	/* infra_audio26m	AUDIO_26M_BCLK_CK */
#define CLK_INFRA_SPI1			54	/* infra_spi1		SPI1_CG */
#define CLK_INFRA_I2C4			55	/* infra_i2c4		I2C4_CG */
#define CLK_INFRA_MODEM_TEMP_SHARE	56	/* infra_mdtemp		MODEM_TEMP_SHARE_CG */
#define CLK_INFRA_SPI2			57	/* infra_spi2		SPI2_CG */
#define CLK_INFRA_SPI3			58	/* infra_spi3		SPI3_CG */
#define CLK_INFRA_I2C5			59	/* infra_i2c5		I2C5_CG */
#define CLK_INFRA_I2C5_ARBITER		60	/* infra_i2c5a		I2C5_ARBITER_CG */
#define CLK_INFRA_I2C5_IMM		61	/* infra_i2c5_imm	I2C5_IMM_CG */
#define CLK_INFRA_I2C1_ARBITER		62	/* infra_i2c1a		I2C1_ARBITER_CG */
#define CLK_INFRA_I2C1_IMM		63	/* infra_i2c1_imm	I2C1_IMM_CG */
#define CLK_INFRA_I2C2_ARBITER		64	/* infra_i2c2a		I2C2_ARBITER_CG */
#define CLK_INFRA_I2C2_IMM		65	/* infra_i2c2_imm	I2C2_IMM_CG */
#define CLK_INFRA_SPI4			66	/* infra_spi4		SPI4_CG */
#define CLK_INFRA_SPI5			67	/* infra_spi5		SPI5_CG */
#define CLK_INFRA_CQ_DMA		68	/* infra_cq_dma		CQ_DMA_CG */
#define CLK_INFRA_MSDC0_SELF		69	/* infra_msdc0sf	MSDC0_SELF_CG */
#define CLK_INFRA_MSDC1_SELF		70	/* infra_msdc1sf	MSDC1_SELF_CG */
#define CLK_INFRA_MSDC2_SELF		71	/* infra_msdc2sf	MSDC2_SELF_CG */
#define CLK_INFRA_I2C6			72	/* infra_i2c6		I2C6_CG */
#define CLK_INFRA_AP_MSDC0		73	/* infra_ap_msdc0	AP_MSDC0_CG */
#define CLK_INFRA_MD_MSDC0		74	/* infra_md_msdc0	MD_MSDC0_CG */
#define CLK_INFRA_MSDC0_SRC		75	/* infra_msdc0_clk	MSDC0_SRC_CLK_CG */
#define CLK_INFRA_MSDC1_SRC		76	/* infra_msdc1_clk	MSDC1_SRC_CLK_CG */
#define CLK_INFRA_MSDC2_SRC		77	/* infra_msdc2_clk	MSDC2_SRC_CLK_CG */
#define CLK_INFRA_PERI_DCM_RG_FORCE_CLKOFF 78	/* infra_dcmforce	PERI_DCM_RG_FORCE_CLKOFF */
#define CLK_INFRA_NFI_1X                79      /* infra_nfi_1x         NFI_1X_CG */
#define CLK_INFRA_NR_CLK		80

/* MMSYS_CONFIG */

#define CLK_MM_SMI_COMMON		0	/* mm_smi_common	SMI_COMMON */
#define CLK_MM_SMI_LARB0		1	/* mm_smi_larb0		SMI_LARB0 */
#define CLK_MM_GALS_COMM0		2	/* mm_gals_comm0	GALS_COMM0 */
#define CLK_MM_GALS_COMM1		3	/* mm_gals_comm1	GALS_COMM1 */
#define CLK_MM_ISP_DL			4	/* mm_isp_dl		ISP_DL */
#define CLK_MM_MDP_RDMA0		5	/* mm_mdp_rdma0		MDP_RDMA0 */
#define CLK_MM_MDP_RSZ0			6	/* mm_mdp_rsz0		MDP_RSZ0 */
#define CLK_MM_MDP_RSZ1			7	/* mm_mdp_rsz1		MDP_RSZ1 */
#define CLK_MM_MDP_TDSHP		8	/* mm_mdp_tdshp		MDP_TDSHP */
#define CLK_MM_MDP_WROT0		9	/* mm_mdp_wrot0		MDP_WROT0 */
#define CLK_MM_MDP_WDMA0		10	/* mm_mdp_wdma0		MDP_WDMA0 */
#define CLK_MM_FAKE_ENG			11	/* mm_fake_eng		FAKE_ENG */
#define CLK_MM_DISP_OVL0		12	/* mm_disp_ovl0		DISP_OVL0 */
#define CLK_MM_DISP_RDMA0		13	/* mm_disp_rdma0	DISP_RDMA0 */
#define CLK_MM_DISP_WDMA0		14	/* mm_disp_wdma0	DISP_WDMA0 */
#define CLK_MM_DISP_COLOR0		15	/* mm_disp_color0	DISP_COLOR0 */
#define CLK_MM_DISP_CCORR0		16	/* mm_disp_ccorr0	DISP_CCORR0 */
#define CLK_MM_DISP_AAL0		17	/* mm_disp_aal0		DISP_AAL0 */
#define CLK_MM_DISP_GAMMA0		18	/* mm_disp_gamma0	DISP_GAMMA0 */
#define CLK_MM_DISP_DITHER0		19	/* mm_disp_dither0	DISP_DITHER0 */
#define CLK_MM_DSI_MM_CLOCK		20	/* mm_dsi_mm_clock	DSI_MM_CLOCK */
#define CLK_MM_DSI_INTERF		21	/* mm_dsi_interf	DSI_INTERF */
#define CLK_MM_DBI_MM_CLOCK		22	/* mm_dbi_mm_clock	DBI_MM_CLOCK */
#define CLK_MM_DBI_INTERF		23	/* mm_dbi_interf	DBI_INTERF */
#define CLK_MM_F26M_HRT			24	/* mm_f26m_hrt		F26M_HRT */
#define CLK_MM_CG0_B25			25	/* mm_cg0_b25		CG0_B25 */
#define CLK_MM_CG0_B26			26	/* mm_cg0_b26		CG0_B26 */
#define CLK_MM_CG0_B27			27	/* mm_cg0_b27		CG0_B27 */
#define CLK_MM_CG0_B28			28	/* mm_cg0_b28		CG0_B28 */
#define CLK_MM_CG0_B29			29	/* mm_cg0_b29		CG0_B29 */
#define CLK_MM_CG0_B30			30	/* mm_cg0_b30		CG0_B30 */
#define CLK_MM_CG0_B31			31	/* mm_cg0_b31		CG0_B31 */
#define CLK_MM_NR_CLK			32

/* VENC_GCON */

#define CLK_VENC_SET0_LARB		0	/* venc_set0_larb	SET0_LARB */
#define CLK_VENC_SET1_VENC		1	/* venc_set1_venc	SET1_VENC */
#define CLK_VENC_SET2_JPGENC		2	/* jpgenc		SET2_JPGENC */
#define CLK_VENC_SET3_VDEC		3	/* venc_set3_vdec	SET3_VDEC */
#define CLK_VENC_NR_CLK			4

/* PERICFG */

#define CLK_PERIAXI_DISABLE		0	/* periaxi_disable	PERIAXI_CG_DISABLE */
#define CLK_PER_NR_CLK			1

/* SCP_SYS */
#define SCP_SYS_MFG0  0
#define SCP_SYS_MFG1  1
#define SCP_SYS_MD1  2
#define SCP_SYS_CONN  3
#define SCP_SYS_MM0  4
#define SCP_SYS_ISP  5
#define SCP_SYS_VEN  6
#define SCP_NR_SYSS  7
#endif
