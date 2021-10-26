/*
 * Copyright (c) 2021 MediaTek Inc.
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

#ifndef _CLK_MT6877_FMETER_H
#define _CLK_MT6877_FMETER_H

/* generate from clock_table.xlsx from TOPCKGEN DE */

/* CKGEN Part */
#define FM_AXI_CK				1
#define FM_SPM_CK				2
#define FM_SCP_CK				3
#define FM_BUS_CK				4
#define FM_DISP0_CK				5
#define FM_MDP0_CK				6
#define FM_IMG1_CK				7
#define FM_IPE_CK				8
#define FM_DPE_CK				9
#define FM_CAM_CK				10
#define FM_CCU_CK				11
#define FM_DSP_CK				12
#define FM_DSP1_CK				13
#define FM_DSP2_CK				14
#define FM_DSP4_CK				16
#define FM_DSP7_CK				19
#define FM_FCAMTG_CK				20
#define FM_FCAMTG2_CK				21
#define FM_FCAMTG3_CK				22
#define FM_FCAMTG4_CK				23
#define FM_FCAMTG5_CK				24
#define FM_FUART_CK				25
#define FM_SPI_CK				26
#define FM_MSDC5HCLK_CK				27
#define FM_MSDC50_0_CK				28
#define FM_MSDC30_1_CK				29
#define FM_AUDIO_CK				30
#define FM_AUD_INTBUS_CK			31
#define FM_FPWRAP_ULPOSC_CK			32
#define FM_ATB_CK				33
#define FM_SSPM_CK				34
#define FM_FDISP_PWM_CK				36
#define FM_FUSB_CK				37
#define FM_FSSUSB_XHCI_CK			38
#define FM_I2C_CK				41
#define FM_FSENINF_CK				42
#define FM_FSENINF1_CK				43
#define FM_FSENINF2_CK				44
#define FM_FSENINF3_CK				45
#define FM_DXCC_CK				46
#define FM_AUD_ENGEN1_CK			47
#define FM_AUD_ENGEN2_CK			48
#define FM_AES_UFSFDE_CK			49
#define FM_UFS_CK				50
#define FM_AUD_1_CK				51
#define FM_AUD_2_CK				52
#define FM_ADSP_CK				53
#define FM_DPMAIF_MAIN_CK			54
#define FM_VENC_CK				55
#define FM_VDEC_CK				56
#define FM_CAMTM_CK				57
#define FM_PWM_CK				58
#define FM_AUDIO_H_CK				59
#define FM_MCUPM_CK				60
#define FM_SPMI_M_MST_CK			62
#define FM_DVFSRC_CK				63
/* ABIST Part */
#define FM_ADSPPLL_CKDIV_CK			1
#define FM_APLL1_CKDIV_CK			2
#define FM_APLL2_CKDIV_CK			3
#define FM_APPLLGP_MON_FM_CK			4
#define FM_ARMPLL_BL_CKDIV_CK			7
#define FM_ARMPLL_LL_CKDIV_CK			9
#define FM_USBPLL_CKDIV_CK			10
#define FM_CCIPLL_CKDIV_CK			11
#define FM_CSI0A_CDPHY_DELAYCAL_CK		12
#define FM_CSI0B_CDPHY_DELAYCAL_CK		13
#define FM_CSI1A_DPHY_DELAYCAL_CK		14
#define FM_CSI1B_DPHY_DELAYCAL_CK		15
#define FM_CSI2A_DPHY_DELAYCAL_CK		16
#define FM_CSI2B_DPHY_DELAYCAL_CK		17
#define FM_CSI3A_DPHY_DELAYCAL_CK		18
#define FM_CSI3B_DPHY_DELAYCAL_CK		19
#define FM_DSI0_LNTC_DSICLK			20
#define FM_DSI0_MPPLL_TST_CK			21
#define FM_MAINPLL_CKDIV_CK			24
#define FM_MDPLL_FS26M_GUIDE			25
#define FM_MMPLL_CKDIV_CK			27
#define FM_MMPLL_D3_CK				28
#define FM_MPLL_CKDIV_CK			29
#define FM_MSDCPLL_CKDIV_CK			30
#define FM_RCLRPLL_DIV4_CK			31
#define FM_RPHYPLL_DIV4_CK			33
#define FM_EMI_CKDIV_CK				34
#define FM_TVDPLL_CKDIV_CK			35
#define FM_ULPOSC2_CK				36
#define FM_ULPOSC_CK				37
#define FM_UNIVPLL_CKDIV_CK			38
#define FM_USB20_192M_OPP_CK			39
#define FM_UFS_MP_CLK2FREQ			41
#define FM_WBG_DIG_BPLL_CK			42
#define FM_WBG_DIG_WPLL_CK960			43
#define FMEM_AFT_CH0				44
#define FMEM_AFT_CH1				45
#define FMEM_BFE_CH0				48
#define FMEM_BFE_CH1				49
#define FM_466M_FMEM_INFRASYS			50
#define FM_MCUSYS_ARM_OUT_ALL			51
#define FM_MSDC01_IN_CK				52
#define FM_MSDC02_IN_CK				53
#define FM_MSDC11_IN_CK				54
#define FM_MSDC12_IN_CK				55
#define FM_RTC32K_I				58
#define FM_CKMON1_CK				60
#define FM_CKMON2_CK				61
#define FM_CKMON3_CK				62
#define FM_CKMON4_CK				63
/* ABIST2 Part */
#define FM_APLL_I2S0_M_CK			1
#define FM_APLL_I2S1_M_CK			2
#define FM_APLL_I2S2_M_CK			3
#define FM_APLL_I2S3_M_CK			4
#define FM_APLL_I2S4_M_CK			5
#define FM_APLL_I2S4_B_CK			6
#define FM_APLL_I2S5_M_CK			7
#define FM_APLL_I2S6_M_CK			8
#define FM_APLL_I2S7_M_CK			9
#define FM_APLL_I2S8_M_CK			10
#define FM_APLL_I2S9_M_CK			11
#define FM_MEM_SUB_CK				12
#define FM_AES_MSDCFDE_CK			13
#define FM_DSI_OCC_CK				15
#define FM_UFS_MBIST_CK				16
#define FM_AP2CONN_HOST_CK			17
#define FM_MSDC_NEW_RX_CK			18
#define FM_UNIPLL_SES_CK			19
#define FM_F_ULPOSC_CK				20
#define FM_F_ULPOSC_CORE_CK			21
#define FM_SRCK_CK				22
#define FM_SPMI_MST_32K_CK			23
#define FM_MAIN_H728M_CK			24
#define FM_MAIN_H546M_CK			25
#define FM_MAIN_H436P8M_CK			26
#define FM_MAIN_H364M_CK			27
#define FM_MAIN_H312M_CK			28
#define FM_UNIV_1248M_CK			29
#define FM_UNIV_832M_CK				30
#define FM_UNIV_624M_CK				31
#define FM_UNIV_499M_CK				32
#define FM_UNIV_416M_CK				33
#define FM_UNIV_356P6M_CK			34
#define FM_MMPLL_D3_CK_2			35
#define FM_MMPLL_D4_CK				36
#define FM_MMPLL_D5_CK				37
#define FM_MMPLL_D6_CK				38
#define FM_MMPLL_D7_CK				39
#define FM_MMPLL_D9_CK				40
#define FM_ROSC_OUT_FREQ			41
#define FM_ALVTS_TO_PLLGP_MON_L1		42
#define FM_ALVTS_TO_PLLGP_MON_L2		43
#define FM_ALVTS_TO_PLLGP_MON_L3		44
#define FM_ALVTS_TO_PLLGP_MON_L4		45
#define FM_ALVTS_TO_PLLGP_MON_L5		46
#define FM_ALVTS_TO_PLLGP_MON_L6		47
#define FM_ALVTS_TO_PLLGP_MON_L7		48
#define FM_ALVTS_TO_PLLGP_MON_L8		49

enum fm_sys_id {
	FM_GPU_PLL_CTRL = 0,
	FM_APU_PLL_CTRL = 1,
	FM_SYS_NUM = 2,
};

struct fm_pwr_sta {
	unsigned int ofs;
	unsigned int msk;
};

struct fm_subsys {
	unsigned int id;
	const char *name;
	void __iomem *base;
	unsigned int con0;
	unsigned int con1;
	struct fm_pwr_sta pwr_sta;
};

#define FM_MFGPLL1				((FM_GPU_PLL_CTRL << 8) | 0)
#define FM_MFGPLL2				((FM_GPU_PLL_CTRL << 8) | 1)
#define FM_MFGPLL3				((FM_GPU_PLL_CTRL << 8) | 2)
#define FM_MFGPLL4				((FM_GPU_PLL_CTRL << 8) | 3)

#define FM_APUPLL1				((FM_APU_PLL_CTRL << 8) | 0)
#define FM_APUPLL2				((FM_APU_PLL_CTRL << 8) | 1)
#define FM_APUPLL3				((FM_APU_PLL_CTRL << 8) | 2)
#define FM_APUPLL4				((FM_APU_PLL_CTRL << 8) | 3)

extern unsigned int mt_get_ckgen_freq(unsigned int ID);
extern unsigned int mt_get_abist_freq(unsigned int ID);
extern unsigned int mt_get_abist2_freq(unsigned int ID);
extern unsigned int mt_get_subsys_freq(unsigned int ID);
extern const struct fmeter_clk *get_fmeter_clks(void);
extern int mt_subsys_freq_register(struct fm_subsys *fm, unsigned int size);

#endif /* _CLK_MT6877_FMETER_H */
