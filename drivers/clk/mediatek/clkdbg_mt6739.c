/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/clk-provider.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/delay.h>


#include "clkdbg.h"
#include "mt6739_clkmgr.h"
#include "clk-fmeter.h"

/*
 * clkdbg dump_state
 */

static const char * const *get_mt6739_all_clk_names(void)
{
	static const char * const clks[] = {
		"mainpll",
		"univpll",
		"msdcpll",
		"mfgpll",
		"mmpll",
		"apll1",
		"axi_sel",
		"mem_sel",
		"ddrphycfg_sel",
		"mm_sel",
		"mfg_sel",
		"camtg_sel",
		"uart_sel",
		"spi_sel",
		"msdc50_0_hclk_sel",
		"msdc50_0_sel",
		"msdc30_1_sel",
		"audio_sel",
		"aud_intbus_sel",
		"dbi0_sel",
		"scam_sel",
		"aud_1_sel",
		"disp_pwm_sel",
		"nfi2x_sel",
		"nfiecc_sel",
		"usb_top_sel",
		"spm_sel",
		"i2c_sel",
		"senif_sel",
		"dxcc_sel",
		"camtg2_sel",
		"aud_engen1_sel",
		"PDN_AFE",
		"PDN_22M",
		"PDN_APLL_TUNER",
		"PDN_ADC",
		"PDN_DAC",
		"PDN_DAC_PREDIS",
		"PDN_TML",
		"I2S1_BCLK_SW_CG",
		"I2S2_BCLK_SW_CG",
		"I2S3_BCLK_SW_CG",
		"I2S4_BCLK_SW_CG",
		"LARB2_SMI_CKPDN",
		"CAM_SMI_CKPDN",
		"CAM_CAM_CKPDN",
		"SEN_TG_CKPDN",
		"SEN_CAM_CKPDN",
		"CAM_SV_CKPDN",
		"SUFOD_CKPDN",
		"FD_CKPDN",
		"PMIC_CG_TMR",
		"PMIC_CG_AP",
		"PMIC_CG_MD",
		"PMIC_CG_CONN",
		"SEJ_CG",
		"APXGPT_CG",
		"ICUSB_CG",
		"GCE_CG",
		"THERM_CG",
		"I2C0_CG",
		"I2C1_CG",
		"I2C2_CG",
		"I2C3_CG",
		"PWM_HCLK_CG",
		"PWM1_CG",
		"PWM2_CG",
		"PWM3_CG",
		"PWM4_CG",
		"PWM5_CG",
		"PWM_CG",
		"UART0_CG",
		"UART1_CG",
		"UART2_CG",
		"UART3_CG",
		"GCE_26M",
		"CQ_DMA_FPC",
		"BTIF_CG",
		"SPI0_CG",
		"MSDC0_CG",
		"MSDC1_CG",
		"NFIECC_312M_CG",
		"DVFSRC_CG",
		"GCPU_CG",
		"TRNG_CG",
		"AUXADC_CG",
		"CPUM_CG",
		"CCIF1_AP_CG",
		"CCIF1_MD_CG",
		"AUXADC_MD_CG",
		"NFI_CG",
		"NFI_1X_CG",
		"AP_DMA_CG",
		"XIU_CG",
		"DEVICE_APC_CG",
		"CCIF_AP_CG",
		"DEBUGSYS_CG",
		"AUDIO_CG",
		"CCIF_MD_CG",
		"DXCC_SEC_CORE_CG",
		"DXCC_AO_CG",
		"DRAMC_F26M_CG",
		"RG_PWM_FBCLK6_CK_CG",
		"DISP_PWM_CG",
		"CLDMA_BCLK_CK",
		"AUDIO_26M_BCLK_CK",
		"SPI1_CG",
		"I2C4_CG",
		"MODEM_TEMP_SHARE_CG",
		"SPI2_CG",
		"SPI3_CG",
		"I2C5_CG",
		"I2C5_ARBITER_CG",
		"I2C5_IMM_CG",
		"I2C1_ARBITER_CG",
		"I2C1_IMM_CG",
		"I2C2_ARBITER_CG",
		"I2C2_IMM_CG",
		"SPI4_CG",
		"SPI5_CG",
		"CQ_DMA_CG",
		"MSDC0_SELF_CG",
		"MSDC1_SELF_CG",
		"MSDC2_SELF_CG",
		"SSPM_26M_SELF_CG",
		"SSPM_32K_SELF_CG",
		"UFS_AXI_CG",
		"I2C6_CG",
		"AP_MSDC0_CG",
		"MD_MSDC0_CG",
		"MSDC0_SRC_CLK_CG",
		"MSDC1_SRC_CLK_CG",
		"MSDC2_SRC_CLK_CG",
		"SMI_COMMON",
		"SMI_LARB0",
		"GALS_COMM0",
		"GALS_COMM1",
		"ISP_DL",
		"MDP_RDMA0",
		"MDP_RSZ0",
		"MDP_RSZ1",
		"MDP_TDSHP",
		"MDP_WROT0",
		"MDP_WDMA0",
		"FAKE_ENG",
		"DISP_OVL0",
		"DISP_RDMA0",
		"DISP_WDMA0",
		"DISP_COLOR0",
		"DISP_CCORR0",
		"DISP_AAL0",
		"DISP_GAMMA0",
		"DISP_DITHER0",
		"DSI_MM_CLOCK",
		"DSI_INTERF",
		"DBI_MM_CLOCK",
		"DBI_INTERF",
		"F26M_HRT",
		"SET0_LARB",
		"SET1_VENC",
		"SET2_JPGENC",
		/* end */
		NULL
	};

	return clks;
}

static const struct fmeter_clk *get_all_fmeter_clks(void)
{
	return mt_get_fmeter_clks();
}

static u32 fmeter_freq_op(const struct fmeter_clk *fclk)
{
	return mt_get_fmeter_freq(fclk->id, fclk->type);
}
/*
 * init functions
 */
static struct clkdbg_ops clkdbg_mt6739_ops = {
	.get_all_fmeter_clks = get_all_fmeter_clks,
	.prepare_fmeter = NULL,
	.unprepare_fmeter = NULL,
	.fmeter_freq = fmeter_freq_op,
	.get_all_clk_names = get_mt6739_all_clk_names,
};

static int clk_dbg_mt6739_probe(struct platform_device *pdev)
{
	pr_notice("%s start\n", __func__);
	set_clkdbg_ops(&clkdbg_mt6739_ops);
	return 0;
}

static struct platform_driver clk_dbg_mt6739_drv = {
	.probe = clk_dbg_mt6739_probe,
	.driver = {
		.name = "clk-dbg-mt6739",
		.owner = THIS_MODULE,
	},
};

/*
 * init functions
 */
static int __init clkdbg_mt6739_init(void)
{
	pr_notice("%s start\n", __func__);
	return clk_dbg_driver_register(&clk_dbg_mt6739_drv, "clk-dbg-mt6739");
}

static void __exit clkdbg_mt6739_exit(void)
{
	platform_driver_unregister(&clk_dbg_mt6739_drv);
}

subsys_initcall(clkdbg_mt6739_init);
module_exit(clkdbg_mt6739_exit);

MODULE_LICENSE("GPL");
