/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifdef pr_fmt
#undef pr_fmt
#endif

#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>

#include "mtk_sd.h"
#include "dbg.h"

#include "include/pmic_regulator.h"
#include <mt-plat/mtk_boot.h>


struct msdc_host *mtk_msdc_host[HOST_MAX_NUM];
EXPORT_SYMBOL(mtk_msdc_host);

int g_dma_debug[HOST_MAX_NUM];
u32 latest_int_status[HOST_MAX_NUM];

unsigned int msdc_latest_transfer_mode[HOST_MAX_NUM] = {
	/* 0 for PIO; 1 for DMA; 3 for nothing */
	MODE_NONE,
	MODE_NONE
};

unsigned int msdc_latest_op[HOST_MAX_NUM] = {
	/* 0 for read; 1 for write; 2 for nothing */
	OPER_TYPE_NUM,
	OPER_TYPE_NUM
};

/* for debug zone */
unsigned int sd_debug_zone[HOST_MAX_NUM] = {
	0,
	0
};
/* for enable/disable register dump */
unsigned int sd_register_zone[HOST_MAX_NUM] = {
	1,
	1
};
/* mode select */
u32 dma_size[HOST_MAX_NUM] = {
	512,
	512
};

u32 drv_mode[HOST_MAX_NUM] = {
	MODE_SIZE_DEP, /* using DMA or not depend on the size */
	MODE_SIZE_DEP
};

int dma_force[HOST_MAX_NUM]; /* used for sd ioctrol */

/**************************************************************/
/* Section 1: Device Tree Global Variables                    */
/**************************************************************/
const struct of_device_id msdc_of_ids[] = {
	{   .compatible = DT_COMPATIBLE_NAME, },
	{ },
};

#if !defined(FPGA_PLATFORM)
static void __iomem *gpio_base;

static void __iomem *topckgen_base;
static void __iomem *infracfg_ao_base;
static void __iomem *apmixed_base;
#endif

void __iomem *msdc_io_cfg_bases[HOST_MAX_NUM];

/**************************************************************/
/* Section 2: Power                                           */
/**************************************************************/
#if !defined(FPGA_PLATFORM)
int msdc_regulator_set_and_enable(struct regulator *reg, int powerVolt)
{
#ifndef CONFIG_MTK_MSDC_BRING_UP_BYPASS
	regulator_set_voltage(reg, powerVolt, powerVolt);
	return regulator_enable(reg);
#else
	return 0;
#endif
}

void msdc_ldo_power(u32 on, struct regulator *reg, int voltage_mv, u32 *status)
{
#if !defined(CONFIG_MTK_MSDC_BRING_UP_BYPASS)
	int voltage_uv = voltage_mv * 1000;

	if (reg == NULL)
		return;

	if (on) { /* want to power on */
		if (*status == 0) {  /* can power on */
			/* Comment out to reduce log */
			/* pr_notice("msdc power on<%d>\n", voltage_uv); */
			(void)msdc_regulator_set_and_enable(reg, voltage_uv);
			*status = voltage_uv;
		} else if (*status == voltage_uv) {
			pr_notice("msdc power on <%d> again!\n", voltage_uv);
		} else {
			pr_notice("msdc change<%d> to <%d>\n",
				*status, voltage_uv);
			regulator_disable(reg);
			(void)msdc_regulator_set_and_enable(reg, voltage_uv);
			*status = voltage_uv;
		}
	} else {  /* want to power off */
		if (*status != 0) {  /* has been powerred on */
			pr_notice("msdc power off\n");
			(void)regulator_disable(reg);
			*status = 0;
		} else {
			pr_notice("msdc not power on\n");
		}
	}
#endif
}

void msdc_dump_ldo_sts(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
#if !defined(CONFIG_MTK_MSDC_BRING_UP_BYPASS) \
	|| defined(MTK_MSDC_BRINGUP_DEBUG)
	u32 ldo_en = 0, vm_mode = 0;
	u32 ldo_vol[2] = {0}, ldo_cal[2] = {0};
	u32 id = host->id;

	switch (id) {
	case 0:
		/*
		 * 6359p provide two groups of VOSEL and VOCAL
		 * when ufs3.0 used,you need to check VOSEL_1,
		 * or you need to check VOSEL_0
		 * you can identify the right VOSEL by checking VM_MODE
		 * VM_MODE=1 <-->VOSEL_1
		 * VM_MODE=0 <-->VOSEL_0
		 */
		pmic_read_interface_nolock(REG_VEMC_EN, &ldo_en, MASK_VEMC_EN,
			SHIFT_VEMC_EN);
		pmic_read_interface_nolock(PMIC_RG_VEMC_VOSEL_0_ADDR,
			&ldo_vol[0],
			PMIC_RG_VEMC_VOSEL_0_MASK, PMIC_RG_VEMC_VOSEL_0_SHIFT);
		pmic_read_interface_nolock(PMIC_RG_VEMC_VOCAL_0_ADDR,
			&ldo_cal[0],
			PMIC_RG_VEMC_VOCAL_0_MASK, PMIC_RG_VEMC_VOCAL_0_SHIFT);
		pmic_read_interface_nolock(PMIC_RG_VEMC_VOSEL_1_ADDR,
			&ldo_vol[1],
			PMIC_RG_VEMC_VOSEL_1_MASK, PMIC_RG_VEMC_VOSEL_1_SHIFT);
		pmic_read_interface_nolock(PMIC_RG_VEMC_VOCAL_1_ADDR,
			&ldo_cal[1],
			PMIC_RG_VEMC_VOCAL_1_MASK, PMIC_RG_VEMC_VOCAL_1_SHIFT);
		pmic_read_interface_nolock(PMIC_VM_MODE_ADDR,
			&vm_mode,
			PMIC_VM_MODE_MASK, PMIC_VM_MODE_SHIFT);
		SPREAD_PRINTF(buff, size, m,
			" VEMC_EN=0x%x, VM_MODE=%d, VEMC_VOL=0:0x%x 1:0x%x [4b'1011(3V)], VEMC_CAL=0:0x%x 1:0x%x\n",
			ldo_en, vm_mode, ldo_vol[0],
			ldo_vol[1], ldo_cal[0], ldo_cal[1]);
		break;
	case 1:
		/*
		 * 6360 only provide regulator APIs with mutex protection.
		 * Therefore, can not dump msdc1 ldo status in IRQ context.
		 * Enable dump msdc1 if you make sure the dump context
		 * is correct.
		 */
		#if 0
		SPREAD_PRINTF(buff, size, m,
		" VMCH_EN=0x%x, VMCH_VOL=0x%x\n",
			regulator_is_enabled(host->mmc->supply.vmmc),
			regulator_get_voltage(host->mmc->supply.vmmc));
		SPREAD_PRINTF(buff, size, m,
		" VMC_EN=0x%x, VMC_VOL=0x%x\n",
			regulator_is_enabled(host->mmc->supply.vqmmc),
			regulator_get_voltage(host->mmc->supply.vqmmc));
		break;
		#endif
	default:
		break;
	}
#endif
}

void msdc_sd_power_switch(struct msdc_host *host, u32 on)
{
#if !defined(CONFIG_MTK_MSDC_BRING_UP_BYPASS)
	if (host->id == 1) {
		msdc_ldo_power(on, host->mmc->supply.vqmmc, VOL_1860,
			&host->power_io);
		msdc_set_tdsel(host, MSDC_TDRDSEL_CUST, 0);
		msdc_set_rdsel(host, MSDC_TDRDSEL_CUST, 0);
		host->hw->driving_applied = &host->hw->driving_sdr50;
		msdc_set_driving(host, host->hw->driving_applied);
	}
#endif
}

void msdc_sdio_power(struct msdc_host *host, u32 on)
{
#if !defined(CONFIG_MTK_MSDC_BRING_UP_BYPASS)
	if (host->id == 2) {
		host->power_flash = VOL_1800 * 1000;
		host->power_io = VOL_1800 * 1000;
	}
#endif
}

void msdc_power_calibration_init(struct msdc_host *host)
{
#if 0
	if (host->hw->host_function == MSDC_EMMC) {
		pmic_config_interface(REG_VEMC_VOSEL_CAL,
			VEMC_VOSEL_CAL_mV(EMMC_VOL_ACTUAL - VOL_3000),
			MASK_VEMC_VOSEL_CAL, SHIFT_VEMC_VOSEL_CAL);

	} else if (host->hw->host_function == MSDC_SD) {
		#if 0
		pmic_config_interface(REG_VMCH_VOSEL_CAL,
			VMCH_VOSEL_CAL_mV(SD_VOL_ACTUAL - VOL_3000),
			MASK_VMCH_VOSEL_CAL, SHIFT_VMCH_VOSEL_CAL);
		pmic_config_interface(REG_VMC_VOSEL_CAL,
			VMC_VOSEL_CAL_mV(SD_VOL_ACTUAL - VOL_3000),
			MASK_VMC_VOSEL_CAL, SHIFT_VMC_VOSEL_CAL);
		#endif
	}
#endif
}

int msdc_oc_check(struct msdc_host *host, u32 en)
{
	/* oc is detected by PMIc interrupt */

	return 0;
}

void msdc_emmc_power(struct msdc_host *host, u32 on)
{
#if !defined(CONFIG_MTK_MSDC_BRING_UP_BYPASS)
	if (on) {
		msdc_set_driving(host, &host->hw->driving);
		msdc_set_tdsel(host, MSDC_TDRDSEL_CUST, 0);
		msdc_set_rdsel(host, MSDC_TDRDSEL_CUST, 0);
	}

	msdc_ldo_power(on, host->mmc->supply.vmmc,
		VOL_3000, &host->power_flash);
	pr_info("msdc%d power %s\n", host->id, (on ? "on" : "off"));
#endif
#ifdef MTK_MSDC_BRINGUP_DEBUG
	msdc_dump_ldo_sts(NULL, 0, NULL, host);
#endif
}

void msdc_sd_power(struct msdc_host *host, u32 on)
{
#if !defined(CONFIG_MTK_MSDC_BRING_UP_BYPASS)
	u32 card_on = on;

	switch (host->id) {
	case 1:
		msdc_set_driving(host, &host->hw->driving);
		msdc_set_tdsel(host, MSDC_TDRDSEL_CUST, 0);
		msdc_set_rdsel(host, MSDC_TDRDSEL_CUST, 0);
		if (host->hw->flags & MSDC_SD_NEED_POWER)
			card_on = 1;

		/* soft start, when power on */
		if (card_on) {
#if 0
			/*
			 * 2'b00: 60us
			 * 2'b01: 120 us
			 * 2'b10: 240 us
			 * 2'b11: 360 us
			 */
			pmic_set_register_value(PMIC_RG_LDO_VMCH_STBTD, 0x2);
			/*
			 * 2'b00: 60us
			 * 2'b01: 120 us
			 * 2'b10: 180 us
			 * 2'b11: 240 us
			 */
			pmic_set_register_value(PMIC_RG_LDO_VMC_STBTD, 0x3);
#endif
		}

		/* VMCH VOLSEL */
		msdc_ldo_power(card_on, host->mmc->supply.vmmc, VOL_3000,
			&host->power_flash);

		/* VMC VOLSEL */
#ifdef NMCARD_SUPPORT
		msdc_ldo_power(on, host->mmc->supply.vqmmc, VOL_1800,
			&host->power_io);
#else
		msdc_ldo_power(on, host->mmc->supply.vqmmc, VOL_3000,
			&host->power_io);
#endif

		pr_info("msdc%d power %s\n", host->id, (on ? "on" : "off"));
		break;

	default:
		break;
	}

#ifdef MTK_MSDC_BRINGUP_DEBUG
	msdc_dump_ldo_sts(NULL, 0, NULL, host);
#endif
#endif
}

static int msdc_sd_event(struct notifier_block *nb,
		unsigned long event, void *data)
{
#if !defined(CONFIG_MTK_MSDC_BRING_UP_BYPASS)
	switch (event) {
	case REGULATOR_EVENT_OVER_CURRENT:
	case REGULATOR_EVENT_FAIL:
		msdc_sd_power_off();
		break;
	default:
		break;
	};
#endif
	return NOTIFY_OK;
}
void msdc_sd_power_off(void)
{
#if !defined(CONFIG_MTK_MSDC_BRING_UP_BYPASS)
	struct msdc_host *host = mtk_msdc_host[1];

	if (host) {
		pr_notice("Power Off, SD card\n");

		/* power must be on */
		host->power_io = VOL_3000 * 1000;
		host->power_flash = VOL_3000 * 1000;

		host->power_control(host, 0);

		msdc_set_bad_card_and_remove(host);
	}
#endif
}
EXPORT_SYMBOL(msdc_sd_power_off);

void msdc_dump_vcore(char **buff, unsigned long *size, struct seq_file *m)
{
#if !defined(CONFIG_MTK_MSDC_BRING_UP_BYPASS) && defined(VCOREFS_READY)
	SPREAD_PRINTF(buff, size, m,
	"%s: Vcore %d\n", __func__, get_cur_vcore_opp());
#endif
}

void msdc_dump_dvfs_reg(char **buff, unsigned long *size, struct seq_file *m,
	struct msdc_host *host)
{
}

void msdc_pmic_force_vcore_pwm(bool enable)
{
	/* Temporarily disable force pwm */

	/* buck_set_mode(VCORE, enable); */
}
#endif /*if !defined(FPGA_PLATFORM)*/

void msdc_set_host_power_control(struct msdc_host *host)
{
	if (host->hw->host_function == MSDC_EMMC) {
		host->power_control = msdc_emmc_power;
	} else if (host->hw->host_function == MSDC_SD) {
		host->power_control = msdc_sd_power;
		host->power_switch = msdc_sd_power_switch;

		#if SD_POWER_DEFAULT_ON
		/* If SD card power is default on, turn it off so that
		 * removable card slot won't keep power when no card plugged
		 */
		if (!(host->mmc->caps & MMC_CAP_NONREMOVABLE)) {
			/* turn on first to match HW/SW state*/
			msdc_sd_power(host, 1);
			mdelay(10);
			msdc_sd_power(host, 0);
		}
		#endif
	} else if (host->hw->host_function == MSDC_SDIO) {
		host->power_control = msdc_sdio_power;
	}

	if (host->power_control != NULL) {
		msdc_power_calibration_init(host);
	} else {
		ERR_MSG("Host function defination error for msdc%d", host->id);
		WARN_ON(1);
	}
}

#if defined(MSDC_HQA)
//#define MSDC_HQA_HV
/*#define MSDC_HQA_NV*/
//#define MSDC_HQA_LV

void msdc_HQA_set_voltage(struct msdc_host *host)
{
#if defined(MSDC_HQA_HV) || defined(MSDC_HQA_LV)
	u32 vcore;
	/*u32 val_delta;*/
#endif
	u32 vio_cal = 0, vio_trim = 0, vio_sel = 0;
	static int vcore_orig = -1;

	if (host->is_autok_done == 1)
		return;

	if (vcore_orig < 0)
		pmic_read_interface(0x1534, &vcore_orig, 0x7F, 0);

	pmic_read_interface(PMIC_RG_VIO18_VOCAL_ADDR, &vio_cal,
		PMIC_RG_VIO18_VOCAL_MASK, PMIC_RG_VIO18_VOCAL_SHIFT);
	pmic_read_interface(PMIC_RG_VIO18_VOTRIM_ADDR, &vio_trim,
		PMIC_RG_VIO18_VOTRIM_MASK, PMIC_RG_VIO18_VOTRIM_SHIFT);
	pmic_read_interface(PMIC_RG_VIO18_VOSEL_ADDR, &vio_sel,
		PMIC_RG_VIO18_VOSEL_MASK, PMIC_RG_VIO18_VOSEL_SHIFT);
	pr_info("[MSDC%d HQA] orig Vcore 0x%x, Vio_Sel=0x%x, Vio_trim 0x%x, Vio_cal 0x%x\n",
		host->id, vcore_orig, vio_sel, vio_trim, vio_cal);

#if defined(MSDC_HQA_HV) || defined(MSDC_HQA_LV)
	/*val_delta = (500000 + vcore_orig * 6250) / 20 / 6250;*/
	vcore = vcore_orig;
	vio_cal = 0;
	vio_trim = 0;

	#ifdef MSDC_HQA_HV
	/*vcore = vcore_orig + val_delta;*/
	/* set to 1.89V */
	vio_sel = 0xC;
	vio_cal = 0x9; /*+90mV*/
	vio_trim = 0;
	#endif

	#ifdef MSDC_HQA_LV
	/*vcore = vcore_orig - val_delta;*/
	/* set to 1.71V */
	vio_sel = 0xB;
	vio_cal = 0x1; /*+10mv*/
	vio_trim = 0;
	#endif

	/*let DRAM do the setting for Vcore*/
	/*pmic_config_interface(0x14aa, vcore, 0x7F, 0);*/

	pr_info("[MSDC%d HQA] adj Vcore 0x%x, Vio_sel=%x, Vio_trim 0x%x, Vio_cal= 0x%x(0x9 = +90mv)\n",
	host->id, vcore, vio_sel, vio_trim, vio_cal);
	pmic_config_interface(PMIC_RG_VIO18_VOSEL_ADDR,
		vio_sel, PMIC_RG_VIO18_VOSEL_MASK, PMIC_RG_VIO18_VOSEL_SHIFT);
	pmic_config_interface(PMIC_RG_VIO18_VOCAL_ADDR,
		vio_cal, PMIC_RG_VIO18_VOCAL_MASK, PMIC_RG_VIO18_VOCAL_SHIFT);

	/* trim need ask pmic owner if trim efuse reg */
	pmic_config_interface(PMIC_TMA_KEY_ADDR,
		0x9CA6, PMIC_TMA_KEY_MASK, PMIC_TMA_KEY_SHIFT);
	pmic_config_interface(PMIC_RG_VIO18_VOTRIM_ADDR, vio_trim,
		PMIC_RG_VIO18_VOTRIM_MASK, PMIC_RG_VIO18_VOTRIM_SHIFT);
	pmic_config_interface(PMIC_TMA_KEY_ADDR,
		0x0, PMIC_TMA_KEY_MASK, PMIC_TMA_KEY_SHIFT);

	/*read twice to check*/
	pmic_read_interface(PMIC_RG_VIO18_VOCAL_ADDR, &vio_cal,
		PMIC_RG_VIO18_VOCAL_MASK, PMIC_RG_VIO18_VOCAL_SHIFT);
	pmic_read_interface(PMIC_RG_VIO18_VOTRIM_ADDR, &vio_trim,
		PMIC_RG_VIO18_VOTRIM_MASK, PMIC_RG_VIO18_VOTRIM_SHIFT);
	pmic_read_interface(PMIC_RG_VIO18_VOSEL_ADDR, &vio_sel,
		PMIC_RG_VIO18_VOSEL_MASK, PMIC_RG_VIO18_VOSEL_SHIFT);
	pr_info("[MSDC%d HQA] after set: Vcore 0x%x,Vio_sel=0x%x,Vio_trim 0x%x, Vio_cal 0x%x\n",
		host->id, vcore_orig, vio_sel, vio_trim, vio_cal);
#endif
}
#endif

/**************************************************************/
/* Section 3: Clock                                           */
/**************************************************************/
#if !defined(FPGA_PLATFORM)
u32 hclks_msdc0[] = { MSDC0_SRC_0, MSDC0_SRC_1, MSDC0_SRC_2, MSDC0_SRC_3,
		      MSDC0_SRC_4, MSDC0_SRC_5};

/* msdc1/2 clock source reference value is 200M */
u32 hclks_msdc1[] = { MSDC1_SRC_0, MSDC1_SRC_1, MSDC1_SRC_2, MSDC1_SRC_3,
		      MSDC1_SRC_4};
u32 *hclks_msdc_all[] = {
	hclks_msdc0,
	hclks_msdc1,
};
u32 *hclks_msdc;

int msdc_get_ccf_clk_pointer(struct platform_device *pdev,
	struct msdc_host *host)
{
	u32 clk_freq;
	static char const * const clk_names[] = {
		MSDC0_CLK_NAME, MSDC1_CLK_NAME
	};
	static char const * const hclk_names[] = {
		MSDC0_HCLK_NAME, MSDC1_HCLK_NAME
	};

	/* clk enable flow
	 * First turn on the clock source of the MSDC register
	 * msdc src hclk -> msdc hclk cg
	 */

#if 0
	if  (pdev->id == 0) {
		host->src_hclk_ctl = devm_clk_get(&pdev->dev,
				MSDC0_SRC_HCLK_NAME);
		if (IS_ERR(host->src_hclk_ctl)) {
			pr_notice("[msdc%d] cannot get src hclk ctl\n",
				pdev->id);
			WARN_ON(1);
			return 1;
		}
		if (clk_prepare_enable(host->src_hclk_ctl)) {
			pr_notice("[msdc%d] cannot prepare src hclk ctrl\n",
				pdev->id);
			return 1;
		}
	}
#endif

	if  (clk_names[pdev->id]) {
		host->clk_ctl = devm_clk_get(&pdev->dev,
			clk_names[pdev->id]);
		if (IS_ERR(host->clk_ctl)) {
			pr_notice("[msdc%d] cannot get clk ctrl\n",
				pdev->id);
			return 1;
		}
		if (clk_prepare_enable(host->clk_ctl)) {
			pr_notice("[msdc%d] cannot prepare clk ctrl\n",
				pdev->id);
			return 1;
		}
	}

	if  (hclk_names[pdev->id]) {
		host->hclk_ctl = devm_clk_get(&pdev->dev,
			hclk_names[pdev->id]);
		if (IS_ERR(host->hclk_ctl)) {
			pr_notice("[msdc%d] cannot get hclk ctrl\n",
				pdev->id);
			return 1;
		}
		if (clk_prepare_enable(host->hclk_ctl)) {
			pr_notice("[msdc%d] cannot prepare hclk ctrl\n",
				pdev->id);
			return 1;
		}
	}
	if (host->clk_ctl) {
		clk_freq = clk_get_rate(host->clk_ctl);
		if (clk_freq > 0)
			host->hclk = clk_freq;
	}

#if defined(CONFIG_MTK_HW_FDE) || defined(CONFIG_MMC_CRYPTO)
	if (pdev->id == 0) {
		host->aes_clk_ctl = devm_clk_get(&pdev->dev,
			MSDC0_AES_CLK_NAME);
		if (IS_ERR(host->aes_clk_ctl)) {
			pr_notice("[msdc%d] can not get aes clock control\n",
				pdev->id);
			WARN_ON(1);
			return 1;
		}
		if (clk_prepare_enable(host->aes_clk_ctl)) {
			pr_notice(
				"[msdc%d] can not prepare aes clock control\n",
				pdev->id);
			WARN_ON(1);
			return 1;
		}
	}
#endif

	pr_info("[msdc%d] src_hclk_ctl:%p, hclk:%p, clk_ctl:%p, hclk_ctl:%p\n",
		pdev->id, host->src_hclk_ctl, host->hclk,
		host->clk_ctl, host->hclk_ctl);

	return 0;
}

#include <linux/seq_file.h>
static void msdc_dump_clock_sts_core(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	char buffer[512];
	char *buf_ptr = buffer;

	if (topckgen_base && infracfg_ao_base) {
		buf_ptr += sprintf(buf_ptr,
			"MSDC0 HCLK_MUX[0x%p][25:24]=%d, pdn=%d\n",
			topckgen_base + 0x070,
			/* mux at bits 25~24 */
			(MSDC_READ32(topckgen_base + 0x070) >> 24) & 3,
			/* pdn at bit 31 */
			(MSDC_READ32(topckgen_base + 0x070) >> 31) & 1);
		buf_ptr += sprintf(buf_ptr,
			"MSDC0 CLK_MUX[%p][2:0]=%d, pdn=%d, CLK_CG[%p]bit 2,6=%d,%d\n",
			topckgen_base + 0x080,
			/* mux at bits 2~0 */
			(MSDC_READ32(topckgen_base + 0x080) >> 0) & 7,
			/* pdn at bit 7 */
			(MSDC_READ32(topckgen_base + 0x080) >> 7) & 1,
			infracfg_ao_base + 0x094,
			/* cg at bit 2,6 */
			(MSDC_READ32(infracfg_ao_base + 0x094) >> 2) & 1,
			(MSDC_READ32(infracfg_ao_base + 0x094) >> 6) & 1);
		buf_ptr += sprintf(buf_ptr,
			"MSDC1 CLK_MUX[%p][10:8]=%d, pdn=%d, CLK_CG[%p]bit 4,16=%d,%d\n",
			topckgen_base + 0x080,
			/* mux at bits 10~8 */
			(MSDC_READ32(topckgen_base + 0x080) >> 8) & 7,
			/* pdn at bit 15 */
			(MSDC_READ32(topckgen_base + 0x080) >> 15) & 1,
			infracfg_ao_base + 0x094,
			/* cg at bit 4,16 */
			(MSDC_READ32(infracfg_ao_base + 0x094) >> 4) & 1,
			(MSDC_READ32(infracfg_ao_base + 0x094) >> 16) & 1);

		*buf_ptr = '\0';
		SPREAD_PRINTF(buff, size, m, "%s", buffer);
	}

	buf_ptr = buffer;
	if (apmixed_base) {
		/* bit0 is enables PLL, 0: disable 1: enable */
		buf_ptr += sprintf(buf_ptr,
			"MSDCPLL_CON0@0x%p = 0x%x, bit[0] shall 1b\n",
			apmixed_base + MSDCPLL_CON0_OFFSET,
			MSDC_READ32(apmixed_base + MSDCPLL_CON0_OFFSET));

		buf_ptr += sprintf(buf_ptr,
			"MSDCPLL_CON1@0x%p = 0x%x\n",
			apmixed_base + MSDCPLL_CON1_OFFSET,
			MSDC_READ32(apmixed_base + MSDCPLL_CON1_OFFSET));

		buf_ptr += sprintf(buf_ptr,
			"MSDCPLL_CON2@0x%p = 0x%x\n",
			apmixed_base + MSDCPLL_CON2_OFFSET,
			MSDC_READ32(apmixed_base + MSDCPLL_CON2_OFFSET));

		buf_ptr += sprintf(buf_ptr,
			"MSDCPLL_PWR_CON0@0x%p = 0x%x, bit[0] shall 1b\n",
			apmixed_base + MSDCPLL_PWR_CON0_OFFSET,
			MSDC_READ32(apmixed_base + MSDCPLL_PWR_CON0_OFFSET));
		*buf_ptr = '\0';
		SPREAD_PRINTF(buff, size, m, "%s", buffer);
	}
}

void msdc_dump_clock_sts(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
	msdc_dump_clock_sts_core(buff, size, m, host);
}

void msdc_clk_enable_and_stable(struct msdc_host *host)
{
	void __iomem *base = host->base;
	u32 div, mode, hs400_div_dis;
	u32 val;

	msdc_clk_enable(host);

	val = MSDC_READ32(MSDC_CFG);
	GET_FIELD(val, CFG_CKDIV_SHIFT, CFG_CKDIV_MASK, div);
	GET_FIELD(val, CFG_CKMOD_SHIFT, CFG_CKMOD_MASK, mode);
	GET_FIELD(val, CFG_CKMOD_HS400_SHIFT, CFG_CKMOD_HS400_MASK,
			hs400_div_dis);
	msdc_clk_stable(host, mode, div, hs400_div_dis);
}
#endif /*if !defined(FPGA_PLATFORM)*/

/**************************************************************/
/* Section 4: GPIO and Pad                                    */
/**************************************************************/
#if !defined(FPGA_PLATFORM)
/*
 * Power off card on the 2 bad card conditions:
 * 1. if dat pins keep high when pulled low or
 * 2. dat pins alway keeps high
 */
int msdc_io_check(struct msdc_host *host)
{
#if 0
	int i;

	void __iomem *base = host->base;
	unsigned long polling_tmo = 0;
#ifndef SD_GPIO_PAD_A_EN
	void __iomem *pupd_addr[3] = {
		MSDC1_PUPD_DAT0_ADDR,
		MSDC1_PUPD_DAT1_ADDR,
		MSDC1_PUPD_DAT2_ADDR
	};
	void __iomem *r0_addr[3] = {
		MSDC1_R0_DAT0_ADDR,
		MSDC1_R0_DAT0_ADDR,
		MSDC1_R0_DAT0_ADDR
	};
	void __iomem *r1_addr[3] = {
		MSDC1_R1_DAT0_ADDR,
		MSDC1_R1_DAT0_ADDR,
		MSDC1_R1_DAT0_ADDR
	};
	u32 pupd_mask[3] = {
		MSDC1_PUPD_DAT0_MASK,
		MSDC1_PUPD_DAT1_MASK,
		MSDC1_PUPD_DAT2_MASK
	};
	u32 r0_mask[3] = {
		MSDC1_R0_DAT0_MASK,
		MSDC1_R0_DAT1_MASK,
		MSDC1_R0_DAT2_MASK
	};
	u32 r1_mask[3] = {
		MSDC1_R1_DAT0_MASK,
		MSDC1_R1_DAT1_MASK,
		MSDC1_R1_DAT2_MASK
	};
#else
	void __iomem *pupd_addr[3] = {
		MSDC1_PUPD_DAT0_ADDR_A,
		MSDC1_PUPD_DAT1_ADDR_A,
		MSDC1_PUPD_DAT2_ADDR_A
	};
	void __iomem *r0_addr[3] = {
		MSDC1_R0_DAT0_ADDR_A,
		MSDC1_R0_DAT0_ADDR_A,
		MSDC1_R0_DAT0_ADDR_A
	};
	void __iomem *r1_addr[3] = {
		MSDC1_R1_DAT0_ADDR_A,
		MSDC1_R1_DAT0_ADDR_A,
		MSDC1_R1_DAT0_ADDR_A
	};
	u32 pupd_mask[3] = {
		MSDC1_PUPD_DAT0_MASK_A,
		MSDC1_PUPD_DAT1_MASK_A,
		MSDC1_PUPD_DAT2_MASK_A
	};
	u32 r0_mask[3] = {
		MSDC1_R0_DAT0_MASK_A,
		MSDC1_R0_DAT1_MASK_A,
		MSDC1_R0_DAT2_MASK_A
	};
	u32 r1_mask[3] = {
		MSDC1_R1_DAT0_MASK_A,
		MSDC1_R1_DAT1_MASK_A,
		MSDC1_R1_DAT2_MASK_A
	};
#endif
	u32 check_patterns[3] = {0xE0000, 0xD0000, 0xB0000};
	u32 orig_pull;
	u32 orig_r0;
	u32 orig_r1;

	if (host->id != 1)
		return 0;

	if (host->block_bad_card)
		goto SET_BAD_CARD;

	for (i = 0; i < 3; i++) {

		MSDC_GET_FIELD(pupd_addr[i], pupd_mask[i], orig_pull);
		MSDC_GET_FIELD(r0_addr[i], r0_mask[i], orig_r0);
		MSDC_GET_FIELD(r1_addr[i], r1_mask[i], orig_r1);
		/*R1R0=00:High-Z, R1R0=01:10kohm,R1R0=10:50kohm,R1R0=11:8kohm*/


		MSDC_SET_FIELD(pupd_addr[i], pupd_mask[i], MSDC1_PD);
		MSDC_SET_FIELD(r0_addr[i], r0_mask[i], 0x1);
		MSDC_SET_FIELD(r1_addr[i], r1_mask[i], 0x1);

		polling_tmo = jiffies + POLLING_PINS;
		while ((MSDC_READ32(MSDC_PS) & 0xF0000) != check_patterns[i]) {
			if (time_after(jiffies, polling_tmo)) {
				/* Exception handling for
				 * some good card with
				 * pull up strength greater
				 * than pull up strength
				 * of gpio.
				 */

				if ((MSDC_READ32(MSDC_PS) & 0xF0000) == 0xF0000)
					break;
				pr_notice("msdc%d DAT%d pin get wrong, ps = 0x%x!\n",
					host->id, i, MSDC_READ32(MSDC_PS));

				goto SET_BAD_CARD;
			}
		}
		MSDC_SET_FIELD(pupd_addr[i], pupd_mask[i], orig_pull);
		MSDC_SET_FIELD(r0_addr[i], r0_mask[i], orig_r0);
		MSDC_SET_FIELD(r1_addr[i], r1_mask[i], orig_r1);
	}

	return 0;

SET_BAD_CARD:
	msdc_set_bad_card_and_remove(host);
	return 1;
#endif
return 0;
}

void msdc_dump_padctl_by_id(char **buff, unsigned long *size,
	struct seq_file *m, u32 id)
{
	if (!gpio_base || !msdc_io_cfg_bases[id]) {
		SPREAD_PRINTF(buff, size, m,
			"err: gpio_base=%p, msdc_io_cfg_bases[%d]=%p\n",
			gpio_base, id, msdc_io_cfg_bases[id]);
		return;
	}

	if (id == 0) {
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 MODE8 [0x%p] =0x%8x\tshould: 0x1111111?\n",
			MSDC0_GPIO_MODE8, MSDC_READ32(MSDC0_GPIO_MODE8));
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 MODE9 [0x%p] =0x%8x\tshould: 0x???11111\n",
			MSDC0_GPIO_MODE9, MSDC_READ32(MSDC0_GPIO_MODE9));
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 IES    [0x%p] =0x%8x\tshould: 0x???1FFE?\n",
			MSDC0_GPIO_IES, MSDC_READ32(MSDC0_GPIO_IES));
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 SMT    [0x%p] =0x%8x\tshould: 0x????3FFC\n",
			MSDC0_GPIO_SMT, MSDC_READ32(MSDC0_GPIO_SMT));
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 TDSEL0 [0x%p] =0x%8x, [0x%p] =0x%8x\n",
			MSDC0_GPIO_TDSEL0_0,
			MSDC_READ32(MSDC0_GPIO_TDSEL0_0),
			MSDC0_GPIO_TDSEL0_1,
			MSDC_READ32(MSDC0_GPIO_TDSEL0_1));
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 RDSEL0 [0x%p] =0x%8x, [0x%p] = 0x%8x, [0x%p] = 0x%8x\n",
			MSDC0_GPIO_RDSEL0_0,
			MSDC_READ32(MSDC0_GPIO_RDSEL0_0),
			MSDC0_GPIO_RDSEL0_1,
			MSDC_READ32(MSDC0_GPIO_RDSEL0_1),
			MSDC0_GPIO_RDSEL0_2,
			MSDC_READ32(MSDC0_GPIO_RDSEL0_2));
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 DRV0   [0x%p] =0x%8x, [0x%p] =0x%8x\n",
			MSDC0_GPIO_DRV0_0,
			MSDC_READ32(MSDC0_GPIO_DRV0_0),
			MSDC0_GPIO_DRV0_1,
			MSDC_READ32(MSDC0_GPIO_DRV0_1));
		SPREAD_PRINTF(buff, size, m,
			"PUPD/R1/R0: dat/cmd:0/0/1, clk/dst: 1/1/0\n");
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 PUPD0  [0x%p] =0x%8x\tshould: 0x?????401\n",
			MSDC0_GPIO_PUPD0,
			MSDC_READ32(MSDC0_GPIO_PUPD0));
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 R0  [0x%p] =0x%8x\tshould: 0x?????BFE\n",
			MSDC0_GPIO_R0,
			MSDC_READ32(MSDC0_GPIO_R0));
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 R1  [0x%p] =0x%8x\tshould: 0x?????401\n",
			MSDC0_GPIO_R1,
			MSDC_READ32(MSDC0_GPIO_R1));
	} else if (id == 1) {
#ifndef SD_GPIO_PAD_A_EN
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 MODE15  [0x%p] =0x%8x\tshould: 0x111?????\n",
			MSDC1_GPIO_MODE15, MSDC_READ32(MSDC1_GPIO_MODE15));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 MODE16  [0x%p] =0x%8x\tshould: 0x?????111\n",
			MSDC1_GPIO_MODE16, MSDC_READ32(MSDC1_GPIO_MODE16));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 IES    [0x%p] =0x%8x\t  5-0bits should: 2b111111\n",
			MSDC1_GPIO_IES, MSDC_READ32(MSDC1_GPIO_IES));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 SMT    [0x%p] =0x%8x\t  5-0bits should: 2b111111\n",
			MSDC1_GPIO_SMT, MSDC_READ32(MSDC1_GPIO_SMT));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 TDSEL0 [0x%p] =0x%8x\n",
			MSDC1_GPIO_TDSEL0,
			MSDC_READ32(MSDC1_GPIO_TDSEL0));
		SPREAD_PRINTF(buff, size, m,
			"should 1.8v: sleep: TBD, awake: TBD\n");
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 RDSEL0 [0x%p] =0x%8x, [0x%p] =0x%8x\n",
			MSDC1_GPIO_RDSEL0_0,
			MSDC_READ32(MSDC1_GPIO_RDSEL0_0),
			MSDC1_GPIO_RDSEL0_1,
			MSDC_READ32(MSDC1_GPIO_RDSEL0_1));
		SPREAD_PRINTF(buff, size, m,
			"1.8V: TBD, 2.9v: TBD\n");
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 DRV0   [0x%p] =0x%8x\n",
			MSDC1_GPIO_DRV0,
			MSDC_READ32(MSDC1_GPIO_DRV0));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 PUPD0  [0x%p] =0x%8x\tshould: 0x??????01\n",
			MSDC1_GPIO_PUPD0,
			MSDC_READ32(MSDC1_GPIO_PUPD0));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 R0  [0x%p] =0x%8x\tshould: 0x??????00\n",
			MSDC1_GPIO_R0,
			MSDC_READ32(MSDC1_GPIO_R0));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 R1  [0x%p] =0x%8x\tshould: 0x??????3F\n",
			MSDC1_GPIO_R1,
			MSDC_READ32(MSDC1_GPIO_R1));
#else
		SPREAD_PRINTF(buff, size, m,
			"MSDC1_A_GPIO_MISC  [0x%p] =0x%8x\tshould: bit[9]=1\n",
			MSDC1_GPIO_MISC, MSDC_READ32(MSDC1_GPIO_MISC));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1_A MODE1  [0x%p] =0x%8x\tshould: 0x111111??\n",
			MSDC1_GPIO_MODE1, MSDC_READ32(MSDC1_GPIO_MODE1));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1_A IES	  [0x%p] =0x%8x\tshould: 0x????7E??\n",
		MSDC1_GPIO_IES_A, MSDC_READ32(MSDC1_GPIO_IES_A));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1_A SMT	  [0x%p] =0x%8x\tshould: 0x??????1?\n",
		MSDC1_GPIO_SMT_A, MSDC_READ32(MSDC1_GPIO_SMT_A));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1_A TDSEL0 [0x%p] =0x%8x\n",
			MSDC1_GPIO_TDSEL0_A,
			MSDC_READ32(MSDC1_GPIO_TDSEL0_A));
		SPREAD_PRINTF(buff, size, m,
			"should 1.8v: sleep: TBD, awake: TBD\n");
		SPREAD_PRINTF(buff, size, m,
			"MSDC1_A RDSEL0 [0x%p] =0x%8x\n",
			MSDC1_GPIO_RDSEL0_A,
			MSDC_READ32(MSDC1_GPIO_RDSEL0_A));
		SPREAD_PRINTF(buff, size, m,
			"1.8V: TBD, 2.9v: TBD\n");
		SPREAD_PRINTF(buff, size, m,
			"MSDC1_A DRV0   [0x%p] =0x%8x\n",
			MSDC1_GPIO_DRV0_A,
			MSDC_READ32(MSDC1_GPIO_DRV0_A));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1_A PUPD0  [0x%p] =0x%8x\tshould: 0x??????01\n",
			MSDC1_GPIO_PUPD0_A,
			MSDC_READ32(MSDC1_GPIO_PUPD0_A));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1_A R0  [0x%p] =0x%8x\tshould: 0x??????00\n",
			MSDC1_GPIO_R0_A,
			MSDC_READ32(MSDC1_GPIO_R0_A));

		SPREAD_PRINTF(buff, size, m,
			"MSDC1_A R1  [0x%p] =0x%8x\tshould: 0x??????3F\n",
			MSDC1_GPIO_R1_A,
			MSDC_READ32(MSDC1_GPIO_R1_A));
#endif

	}
}

void msdc_set_pin_mode(struct msdc_host *host)
{
	if (host->id == 0) {
		MSDC_SET_FIELD(MSDC0_GPIO_MODE8, 0xFFFFFFF0, 0x1111111);
		MSDC_SET_FIELD(MSDC0_GPIO_MODE9, 0x000FFFFF, 0x11111);
	} else if (host->id == 1) {
#ifndef SD_GPIO_PAD_A_EN
		MSDC_SET_FIELD(MSDC1_GPIO_MODE15, 0xFFF00000, 0x111);
		MSDC_SET_FIELD(MSDC1_GPIO_MODE16, 0x00000FFF, 0x111);
#else
		MSDC_SET_FIELD(MSDC1_GPIO_MISC, MSDC1_PIN_MUX_SEL_MASK_A, 0x1);
		MSDC_SET_FIELD(MSDC1_GPIO_MODE1, 0xFFFFFF00, 0x111111);
#endif
	}
}

void msdc_set_ies_by_id(u32 id, int set_ies)
{
	if (id == 0) {
		MSDC_SET_FIELD(MSDC0_GPIO_IES, MSDC0_IES_ALL_MASK,
			(set_ies ? 0x7FF : 0));
	} else if (id == 1) {
#ifndef SD_GPIO_PAD_A_EN
		MSDC_SET_FIELD(MSDC1_GPIO_IES, MSDC1_IES_ALL_MASK,
			(set_ies ? 0x3F : 0));
#else
		MSDC_SET_FIELD(MSDC1_GPIO_IES_A, MSDC1_IES_ALL_MASK_A,
			(set_ies ? 0x3F : 0));
#endif
	}
}

void msdc_set_smt_by_id(u32 id, int set_smt)
{
	if (id == 0) {
		MSDC_SET_FIELD(MSDC0_GPIO_SMT, MSDC0_SMT_ALL_MASK,
			(set_smt ? 0x7FF : 0));
	} else if (id == 1) {
#ifndef SD_GPIO_PAD_A_EN
		MSDC_SET_FIELD(MSDC1_GPIO_SMT, MSDC1_SMT_ALL_MASK,
			(set_smt ? 0x3F : 0));
#else
		MSDC_SET_FIELD(MSDC1_GPIO_SMT_A, MSDC1_SMT_ALL_MASK_A,
			(set_smt ? 0x3F : 0));
#endif
	}
}

void msdc_set_tdsel_by_id(u32 id, u32 flag, u32 value)
{
	u32 cust_val;

	if (id == 0) {
		if (flag == MSDC_TDRDSEL_CUST)
			cust_val = value;
		else
			cust_val = 0;
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL0_0, MSDC0_TDSEL0_CMD_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL0_0, MSDC0_TDSEL0_DAT0_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL0_0, MSDC0_TDSEL0_DAT1_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL0_0, MSDC0_TDSEL0_DAT2_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL0_0, MSDC0_TDSEL0_DAT3_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL0_1, MSDC0_TDSEL0_DAT4_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL0_1, MSDC0_TDSEL0_DAT5_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL0_1, MSDC0_TDSEL0_DAT6_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL0_1, MSDC0_TDSEL0_DAT7_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL0_0, MSDC0_TDSEL0_CLK_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL0_1, MSDC0_TDSEL0_DSL_MASK,
			cust_val);
	} else if (id == 1) {
		if (flag == MSDC_TDRDSEL_CUST)
			cust_val = value;
		else
			cust_val = 0;
#ifndef SD_GPIO_PAD_A_EN
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL0, MSDC1_TDSEL0_DAT0_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL0, MSDC1_TDSEL0_DAT1_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL0, MSDC1_TDSEL0_DAT2_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL0, MSDC1_TDSEL0_DAT3_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL0, MSDC1_TDSEL0_CMD_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL0, MSDC1_TDSEL0_CLK_MASK,
			cust_val);
#else
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL0_A,
			MSDC1_TDSEL0_DAT_MASK_A, cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL0_A,
			MSDC1_TDSEL0_CMD_MASK_A, cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL0_A,
			MSDC1_TDSEL0_CLK_MASK_A, cust_val);
#endif
	}
}

void msdc_set_rdsel_by_id(u32 id, u32 flag, u32 value)
{
	u32 cust_val;

	if (id == 0) {
		if (flag == MSDC_TDRDSEL_CUST)
			cust_val = value;
		else
			cust_val = 0;
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_0, MSDC0_RDSEL0_CMD_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_0, MSDC0_RDSEL0_DAT0_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_0, MSDC0_RDSEL0_DAT1_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_1, MSDC0_RDSEL0_DAT2_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_1, MSDC0_RDSEL0_DAT3_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_1, MSDC0_RDSEL0_DAT4_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_1, MSDC0_RDSEL0_DAT5_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_1, MSDC0_RDSEL0_DAT6_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_2, MSDC0_RDSEL0_DAT7_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_0, MSDC0_RDSEL0_CLK_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_2, MSDC0_RDSEL0_DSL_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_2, MSDC0_RDSEL0_RSTB_MASK,
			cust_val);
	} else if (id == 1) {
		if (flag == MSDC_TDRDSEL_CUST)
			cust_val = value;
		else
			cust_val = 0;
#ifndef SD_GPIO_PAD_A_EN
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL0_0, MSDC1_RDSEL0_CMD_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL0_0, MSDC1_RDSEL0_DAT0_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL0_0, MSDC1_RDSEL0_DAT1_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL0_0, MSDC1_RDSEL0_DAT2_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL0_1, MSDC1_RDSEL0_DAT3_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL0_0, MSDC1_RDSEL0_CLK_MASK,
			cust_val);
#else
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL0_A,
			MSDC1_RDSEL0_CMD_MASK_A, cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL0_A,
			MSDC1_RDSEL0_DAT_MASK_A, cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL0_A,
			MSDC1_RDSEL0_CLK_MASK_A, cust_val);
#endif
	}
}

void msdc_get_tdsel_by_id(u32 id, u32 *value)
{
	if (id == 0) {
		MSDC_GET_FIELD(MSDC0_GPIO_TDSEL0_0, MSDC0_TDSEL0_CMD_MASK,
			*value);
	} else if (id == 1) {
#ifndef SD_GPIO_PAD_A_EN
		MSDC_GET_FIELD(MSDC1_GPIO_TDSEL0, MSDC1_TDSEL0_CMD_MASK,
			*value);
#else
		MSDC_GET_FIELD(MSDC1_GPIO_TDSEL0_A,
			MSDC1_TDSEL0_CMD_MASK_A, *value);
#endif
	}

}

void msdc_get_rdsel_by_id(u32 id, u32 *value)
{
	if (id == 0) {
		MSDC_GET_FIELD(MSDC0_GPIO_RDSEL0_0, MSDC0_RDSEL0_CMD_MASK,
			*value);
	} else if (id == 1) {
#ifndef SD_GPIO_PAD_A_EN
		MSDC_GET_FIELD(MSDC1_GPIO_RDSEL0_0, MSDC1_RDSEL0_CMD_MASK,
			*value);
#else
		MSDC_GET_FIELD(MSDC1_GPIO_RDSEL0_A,
			MSDC1_RDSEL0_CMD_MASK_A, *value);
#endif
	}
}

void msdc_set_sr_by_id(u32 id, int clk, int cmd, int dat, int rst, int ds)
{

}
void msdc_set_driving_by_id(u32 id, struct msdc_hw_driving *driving)
{
	if (id == 0) {
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_1,
			MSDC0_DRV0_DSL_MASK,
			driving->ds_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_DAT0_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_DAT1_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_DAT2_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_DAT3_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_DAT4_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_DAT5_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_1,
			MSDC0_DRV0_DAT6_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_1,
			MSDC0_DRV0_DAT7_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_CMD_MASK,
			driving->cmd_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_CLK_MASK,
			driving->clk_drv);
	} else if (id == 1) {
#ifndef SD_GPIO_PAD_A_EN
		MSDC_SET_FIELD(MSDC1_GPIO_DRV0,
			MSDC1_DRV0_CMD_MASK,
			driving->cmd_drv);
		MSDC_SET_FIELD(MSDC1_GPIO_DRV0,
			MSDC1_DRV0_CLK_MASK,
			driving->clk_drv);
		MSDC_SET_FIELD(MSDC1_GPIO_DRV0,
			MSDC1_DRV0_DAT0_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC1_GPIO_DRV0,
			MSDC1_DRV0_DAT1_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC1_GPIO_DRV0,
			MSDC1_DRV0_DAT2_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC1_GPIO_DRV0,
			MSDC1_DRV0_DAT3_MASK,
			driving->dat_drv);
#else
		MSDC_SET_FIELD(MSDC1_GPIO_DRV0_A,
			MSDC1_DRV0_CMD_MASK_A,
			driving->cmd_drv);
		MSDC_SET_FIELD(MSDC1_GPIO_DRV0_A,
			MSDC1_DRV0_CLK_MASK_A,
			driving->clk_drv);
		MSDC_SET_FIELD(MSDC1_GPIO_DRV0_A,
			MSDC1_DRV0_DAT0_MASK_A,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC1_GPIO_DRV0_A,
			MSDC1_DRV0_DAT1_MASK_A,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC1_GPIO_DRV0_A,
			MSDC1_DRV0_DAT2_MASK_A,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC1_GPIO_DRV0_A,
			MSDC1_DRV0_DAT3_MASK_A,
			driving->dat_drv);
#endif
	}
}

void msdc_get_driving_by_id(u32 id, struct msdc_hw_driving *driving)
{
	if (id == 0) {
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_1,
			MSDC0_DRV0_DSL_MASK,
			driving->ds_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_DAT0_MASK,
			driving->dat_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_DAT1_MASK,
			driving->dat_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_DAT2_MASK,
			driving->dat_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_DAT3_MASK,
			driving->dat_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_DAT4_MASK,
			driving->dat_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_DAT5_MASK,
			driving->dat_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_1,
			MSDC0_DRV0_DAT6_MASK,
			driving->dat_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_1,
			MSDC0_DRV0_DAT7_MASK,
			driving->dat_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_CMD_MASK,
			driving->cmd_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_0,
			MSDC0_DRV0_CLK_MASK,
			driving->clk_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_1,
			MSDC0_DRV0_RSTB_MASK,
			driving->rst_drv);
	} else if (id == 1) {
#ifndef SD_GPIO_PAD_A_EN
		MSDC_GET_FIELD(MSDC1_GPIO_DRV0,
			MSDC1_DRV0_CMD_MASK,
			driving->cmd_drv);
		MSDC_GET_FIELD(MSDC1_GPIO_DRV0,
			MSDC1_DRV0_CLK_MASK,
			driving->clk_drv);
		MSDC_GET_FIELD(MSDC1_GPIO_DRV0,
			MSDC1_DRV0_DAT0_MASK,
			driving->dat_drv);
		MSDC_GET_FIELD(MSDC1_GPIO_DRV0,
			MSDC1_DRV0_DAT1_MASK,
			driving->dat_drv);
		MSDC_GET_FIELD(MSDC1_GPIO_DRV0,
			MSDC1_DRV0_DAT2_MASK,
			driving->dat_drv);
		MSDC_GET_FIELD(MSDC1_GPIO_DRV0,
			MSDC1_DRV0_DAT3_MASK,
			driving->dat_drv);
#else
		MSDC_GET_FIELD(MSDC1_GPIO_DRV0_A,
			MSDC1_DRV0_CMD_MASK_A,
			driving->cmd_drv);
		MSDC_GET_FIELD(MSDC1_GPIO_DRV0_A,
			MSDC1_DRV0_CLK_MASK_A,
			driving->clk_drv);
		MSDC_GET_FIELD(MSDC1_GPIO_DRV0_A,
			MSDC1_DRV0_DAT0_MASK_A,
			driving->dat_drv);
#endif
	}
}

/* msdc pin config
 * MSDC0
 * PUPD/R1/R0
 * 0/0/0: High-Z
 * 0/1/0: Pull-up with 50Kohm
 * 0/0/1: Pull-up with 10Kohm
 * 0/1/1: Pull-up with 50Kohm//10Kohm
 * 1/0/0: High-Z
 * 1/1/0: Pull-down with 50Kohm
 * 1/0/1: Pull-down with 10Kohm
 * 1/1/1: Pull-down with 50Kohm//10Kohm
 */
void msdc_pin_config_by_id(u32 id, u32 mode)
{
	if (id == 0) {
		/* 1. don't pull CLK high;
		 * 2. Don't toggle RST to prevent from entering boot mode
		 */
		if (mode == MSDC_PIN_PULL_NONE) {
			/* Switch MSDC0_* to no ohm PU */
			MSDC_SET_FIELD(MSDC0_GPIO_PUPD0,
				MSDC0_PUPD_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC0_GPIO_R0,
				MSDC0_R0_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC0_GPIO_R1,
				MSDC0_R1_ALL_MASK, 0x0);
		} else if (mode == MSDC_PIN_PULL_DOWN) {
			/* Switch MSDC0_* to 50K ohm PD */
			MSDC_SET_FIELD(MSDC0_GPIO_PUPD0,
				MSDC0_PUPD_ALL_MASK, 0x7FF);
			MSDC_SET_FIELD(MSDC0_GPIO_R0,
				MSDC0_R0_ALL_MASK, 0);
			MSDC_SET_FIELD(MSDC0_GPIO_R1,
				MSDC0_R1_ALL_MASK, 0xFFF);
		} else if (mode == MSDC_PIN_PULL_UP) {
			/* Switch MSDC0_CLK to 50K ohm PD,
			 * MSDC0_CMD/MSDC0_DAT* to 10K ohm PU,
			 * MSDC0_DSL to 50K ohm PD
			 */
			MSDC_SET_FIELD(MSDC0_GPIO_PUPD0,
				MSDC0_PUPD_ALL_MASK, 0x401);
			MSDC_SET_FIELD(MSDC0_GPIO_R0,
				MSDC0_R0_ALL_MASK, 0xBFE);
			MSDC_SET_FIELD(MSDC0_GPIO_R1,
				MSDC0_R1_ALL_MASK, 0x401);
		}
	} else if (id == 1) {
		if (mode == MSDC_PIN_PULL_NONE) {
			/* Switch MSDC1_* to no ohm PU */
#ifndef SD_GPIO_PAD_A_EN
			MSDC_SET_FIELD(MSDC1_GPIO_PUPD0,
				MSDC1_PUPD_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC1_GPIO_R0,
				MSDC1_R0_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC1_GPIO_R1,
				MSDC1_R1_ALL_MASK, 0x0);
#else
			MSDC_SET_FIELD(MSDC1_GPIO_PUPD0_A,
				MSDC1_PUPD_ALL_MASK_A, 0x0);
			MSDC_SET_FIELD(MSDC1_GPIO_R0_A,
				MSDC1_R0_ALL_MASK_A, 0x0);
			MSDC_SET_FIELD(MSDC1_GPIO_R1_A,
				MSDC1_R1_ALL_MASK_A, 0x0);
#endif
		} else if (mode == MSDC_PIN_PULL_DOWN) {
			/* Switch MSDC1_* to 50K ohm PD */
#ifndef SD_GPIO_PAD_A_EN
			MSDC_SET_FIELD(MSDC1_GPIO_PUPD0,
				MSDC1_PUPD_ALL_MASK, 0x3F);
			MSDC_SET_FIELD(MSDC1_GPIO_R0,
				MSDC1_R0_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC1_GPIO_R1,
				MSDC1_R1_ALL_MASK, 0x3F);
#else
			MSDC_SET_FIELD(MSDC1_GPIO_PUPD0_A,
				MSDC1_PUPD_ALL_MASK_A, 0x3F);
			MSDC_SET_FIELD(MSDC1_GPIO_R0_A,
				MSDC1_R0_ALL_MASK_A, 0x0);
			MSDC_SET_FIELD(MSDC1_GPIO_R1_A,
				MSDC1_R1_ALL_MASK_A, 0x3F);
#endif
		} else if (mode == MSDC_PIN_PULL_UP) {
			/* Switch MSDC1_CLK to 50K ohm PD,
			 * MSDC1_CMD/MSDC1_DAT* to 50K ohm PU
			 */
#ifndef SD_GPIO_PAD_A_EN
			MSDC_SET_FIELD(MSDC1_GPIO_PUPD0,
				MSDC1_PUPD_ALL_MASK, 0x1);
			MSDC_SET_FIELD(MSDC1_GPIO_R0,
				MSDC1_R0_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC1_GPIO_R1,
				MSDC1_R1_ALL_MASK, 0x3F);
#else
			MSDC_SET_FIELD(MSDC1_GPIO_PUPD0_A,
				MSDC1_PUPD_ALL_MASK_A, 0x1);
			MSDC_SET_FIELD(MSDC1_GPIO_R0_A,
				MSDC1_R0_ALL_MASK_A, 0x0);
			MSDC_SET_FIELD(MSDC1_GPIO_R1_A,
				MSDC1_R1_ALL_MASK_A, 0x3F);
#endif
		}
	}
}
#endif /*if !defined(FPGA_PLATFORM)*/


/**************************************************************/
/* Section 5: Device Tree Init function                       */
/*            This function is placed here so that all	      */
/*            functions and variables used by it has already  */
/*            been declared                                   */
/**************************************************************/
/*
 * parse pinctl settings
 * Driver strength
 */
#if !defined(FPGA_PLATFORM)
static int msdc_get_pinctl_settings(struct msdc_host *host,
	struct device_node *np)
{
	struct device_node *pinctl_node, *pins_node;
	static char const * const pinctl_names[] = {
		"pinctl", "pinctl_hs400", "pinctl_hs200",
		"pinctl_sdr104", "pinctl_sdr50", "pinctl_ddr50"
	};

	/* sequence shall be the same as sequence in msdc_hw_driving */
	static char const * const pins_names[] = {
		"pins_cmd", "pins_dat", "pins_clk", "pins_rst", "pins_ds"
	};
	unsigned char *pin_drv;
	int i, j;

	host->hw->driving_applied = &host->hw->driving;
	for (i = 0; i < ARRAY_SIZE(pinctl_names); i++) {
		pinctl_node = of_parse_phandle(np, pinctl_names[i], 0);

		if (strcmp(pinctl_names[i], "pinctl") == 0)
			pin_drv = (unsigned char *)&host->hw->driving;
		else if (strcmp(pinctl_names[i], "pinctl_hs400") == 0)
			pin_drv = (unsigned char *)&host->hw->driving_hs400;
		else if (strcmp(pinctl_names[i], "pinctl_hs200") == 0)
			pin_drv = (unsigned char *)&host->hw->driving_hs200;
		else if (strcmp(pinctl_names[i], "pinctl_sdr104") == 0)
			pin_drv = (unsigned char *)&host->hw->driving_sdr104;
		else if (strcmp(pinctl_names[i], "pinctl_sdr50") == 0)
			pin_drv = (unsigned char *)&host->hw->driving_sdr50;
		else if (strcmp(pinctl_names[i], "pinctl_ddr50") == 0)
			pin_drv = (unsigned char *)&host->hw->driving_ddr50;
		else
			continue;

		for (j = 0; j < ARRAY_SIZE(pins_names); j++) {
			pins_node = of_get_child_by_name(pinctl_node,
				pins_names[j]);

			if (pins_node)
				of_property_read_u8(pins_node,
					"drive-strength", pin_drv);
			pin_drv++;
		}
	}

	return 0;
}
#endif

/* Get msdc register settings
 * 1. internal data delay for tuning, FIXME: can be removed when use data tune?
 * 2. sample edge
 */
static int msdc_get_register_settings(struct msdc_host *host,
	struct device_node *np)
{
	struct device_node *register_setting_node = NULL;

	/* parse hw property settings */
	register_setting_node = of_parse_phandle(np, "register_setting", 0);
	if (register_setting_node) {
		of_property_read_u8(register_setting_node, "cmd_edge",
				&host->hw->cmd_edge);
		of_property_read_u8(register_setting_node, "rdata_edge",
				&host->hw->rdata_edge);
		of_property_read_u8(register_setting_node, "wdata_edge",
				&host->hw->wdata_edge);
	} else {
		pr_notice("[msdc%d] register_setting is not found in DT\n",
			host->id);
	}

	return 0;
}

/*
 *	msdc_of_parse() - parse host's device-tree node
 *	@host: host whose node should be parsed.
 *
 */
static struct notifier_block sd_oc_nb;  //register call back for 6360 PMIC OC
int msdc_of_parse(struct platform_device *pdev, struct mmc_host *mmc)
{
	struct device_node *np;
	struct msdc_host *host = mmc_priv(mmc);
	int ret = 0;
	int len = 0;
	u8 id = 0;
	const char *dup_name; /*use to solve UAF issue :ALPS04094268*/
	int boot_type;

	np = mmc->parent->of_node; /* mmcx node in project dts */

	if (of_property_read_u8(np, "index", &id)) {
		pr_notice("[%s] host index not specified in device tree\n",
			pdev->dev.of_node->name);
		return -1;
	}

	/* Add get_boot_type check and return ENODEV if not eMMC boot */
	boot_type = get_boot_type();

	if ((boot_type != BOOTDEV_SDMMC) && (id == 0))
		return -ENODEV;

	host->id = id;
	pdev->id = id;

	pr_notice("DT probe %s%d!\n", pdev->dev.of_node->name, id);

	ret = mmc_of_parse(mmc);
	if (ret) {
		pr_notice("%s: mmc of parse error!!: %d\n", __func__, ret);
		return ret;
	}

	host->mmc = mmc;
	host->hw = kzalloc(sizeof(struct msdc_hw), GFP_KERNEL);

	/* iomap register */
	host->base = of_iomap(np, 0);
	if (!host->base) {
		pr_notice("[msdc%d] of_iomap failed\n", mmc->index);
		return -ENOMEM;
	}

	/* get irq # */
	host->irq = irq_of_parse_and_map(np, 0);
	pr_notice("[msdc%d] get irq # %d\n", host->id, host->irq);
	WARN_ON(host->irq < 0);

#if !defined(FPGA_PLATFORM)
	/* get clk_src */
	if (of_property_read_u8(np, "clk_src", &host->hw->clk_src)) {
		pr_notice("[msdc%d] error: clk_src isn't found in device tree.\n",
			host->id);
		WARN_ON(1);
	}
	host->hclk = msdc_get_hclk(host->id, host->hw->clk_src);
#endif

	if (of_find_property(np, "sd-uhs-ddr208", &len))
		host->hw->flags |= MSDC_SDIO_DDR208;

	/* Returns 0 on success, -EINVAL if the property does not exist,
	 * -ENODATA if property does not have a value, and -EOVERFLOW if the
	 * property data isn't large enough.
	 */
	if (of_property_read_u8(np, "host_function", &host->hw->host_function))
		pr_notice("[msdc%d] host_function isn't found in device tree\n",
			host->id);

	/* get cd_gpio and cd_level */
	ret = of_get_named_gpio(np, "cd-gpios", 0);
	if (ret >= 0) {
		cd_gpio = ret;
		if (of_property_read_u8(np, "cd_level", &host->hw->cd_level))
			pr_info("[msdc%d] cd_level isn't found in device tree\n",
				host->id);
	}

	msdc_get_register_settings(host, np);
#if !defined(FPGA_PLATFORM)
	msdc_get_pinctl_settings(host, np);
	mmc->supply.vmmc = regulator_get(mmc_dev(mmc), "vmmc");
	if (IS_ERR(mmc->supply.vmmc)) {
		pr_info("err=%ld,failed to get vmmc\n",
			PTR_ERR(mmc->supply.vmmc));
		goto vmmc_fail;
	}

	mmc->supply.vqmmc = regulator_get(mmc_dev(mmc), "vqmmc");
	if (IS_ERR(mmc->supply.vqmmc)) {
		pr_info("err=%ld ,failed to get vqmmc\n",
			PTR_ERR(mmc->supply.vqmmc));
		goto vqmmc_fail;
	}
#else
	msdc_fpga_pwr_init();
#endif

#if !defined(FPGA_PLATFORM)
	if (host->hw->host_function == MSDC_EMMC) {
		np = of_find_compatible_node(NULL, NULL, "mediatek,msdc0_top");
		host->base_top = of_iomap(np, 0);
	} else if (host->hw->host_function == MSDC_SD)  {
		np = of_find_compatible_node(NULL, NULL, "mediatek,msdc1_top");
		host->base_top = of_iomap(np, 0);
	}

	if (host->base_top)
		pr_debug("of_iomap for MSDC%d TOP base @ 0x%p\n",
			host->id, host->base_top);
#endif

#if defined(CFG_DEV_MSDC3)
	if (host->hw->host_function == MSDC_SDIO) {
		host->hw->flags |= MSDC_EXT_SDIO_IRQ;
		host->hw->request_sdio_eirq = mt_sdio_ops[3].sdio_request_eirq;
		host->hw->enable_sdio_eirq = mt_sdio_ops[3].sdio_enable_eirq;
		host->hw->disable_sdio_eirq = mt_sdio_ops[3].sdio_disable_eirq;
		host->hw->register_pm = mt_sdio_ops[3].sdio_register_pm;
	}
#endif
	/* fix uaf(use afer free) issue:backup pdev->name,
	 * device_rename will free pdev->name
	 */
	pdev->name = kstrdup(pdev->name, GFP_KERNEL);
	/* device rename */
	if ((host->id == 0) && !device_rename(mmc->parent, "bootdevice"))
		pr_notice("[msdc%d] device renamed to bootdevice.\n", host->id);
	else if ((host->id == 1) && !device_rename(mmc->parent, "externdevice"))
		pr_notice("[msdc%d] device renamed to externdevice.\n",
			host->id);
	else if ((host->id == 0) || (host->id == 1))
		pr_notice("[msdc%d] error: device renamed failed.\n", host->id);

	dup_name = pdev->name;
	pdev->name = pdev->dev.kobj.name;
	kfree_const(dup_name);

#if !defined(FPGA_PLATFORM) && !defined(CONFIG_MTK_MSDC_BRING_UP_BYPASS)
	if (host->id == 1) {
		sd_oc_nb.notifier_call = msdc_sd_event;
		devm_regulator_register_notifier(mmc->supply.vmmc
			, &sd_oc_nb);
		devm_regulator_register_notifier(mmc->supply.vqmmc

			, &sd_oc_nb);
	}
#endif

	return host->id;
vqmmc_fail:
	regulator_put(mmc->supply.vmmc);
vmmc_fail:
	kfree(host->hw);
	return -1;
}

int msdc_dt_init(struct platform_device *pdev, struct mmc_host *mmc)
{
	int id;

#ifndef FPGA_PLATFORM
#ifndef SD_GPIO_PAD_A_EN
	static char const * const ioconfig_names[] = {
		MSDC0_IOCFG_NAME, MSDC1_IOCFG_NAME
	};
#else
	static char const * const ioconfig_names[] = {
		MSDC0_IOCFG_NAME, MSDC1_A_IOCFG_NAME
	};
#endif
	struct device_node *np;
#endif

	id = msdc_of_parse(pdev, mmc);
	if (id < 0) {
		pr_notice("%s: msdc_of_parse error!!: %d\n", __func__, id);
		return id;
	}

#ifndef FPGA_PLATFORM
	if (gpio_base == NULL) {
		np = of_find_compatible_node(NULL, NULL, "mediatek,gpio");
		gpio_base = of_iomap(np, 0);
		pr_debug("of_iomap for gpio base @ 0x%p\n", gpio_base);
	}

	if (msdc_io_cfg_bases[id] == NULL) {
		np = of_find_compatible_node(NULL, NULL, ioconfig_names[id]);
		msdc_io_cfg_bases[id] = of_iomap(np, 0);
		pr_debug("of_iomap for MSDC%d IOCFG base @ 0x%p\n",
			id, msdc_io_cfg_bases[id]);
	}

	if (apmixed_base == NULL) {
		np = of_find_compatible_node(NULL, NULL, "mediatek,apmixed");
		apmixed_base = of_iomap(np, 0);
		pr_debug("of_iomap for apmixed base @ 0x%p\n",
			apmixed_base);
	}

	if (topckgen_base == NULL) {
		np = of_find_compatible_node(NULL, NULL, "mediatek,topckgen");
		topckgen_base = of_iomap(np, 0);
		pr_debug("of_iomap for topckgen base @ 0x%p\n",
			topckgen_base);
	}

	if (infracfg_ao_base == NULL) {
		np = of_find_compatible_node(NULL, NULL,
			"mediatek,infracfg_ao");
		infracfg_ao_base = of_iomap(np, 0);
		pr_debug("of_iomap for infracfg_ao base @ 0x%p\n",
			infracfg_ao_base);
	}

#endif

	return 0;
}

/**************************************************************/
/* Section 7: For msdc register dump                          */
/**************************************************************/
u16 msdc_offsets[] = {
	OFFSET_MSDC_CFG,
	OFFSET_MSDC_IOCON,
	OFFSET_MSDC_PS,
	OFFSET_MSDC_INT,
	OFFSET_MSDC_INTEN,
	OFFSET_MSDC_FIFOCS,
	OFFSET_SDC_CFG,
	OFFSET_SDC_CMD,
	OFFSET_SDC_ARG,
	OFFSET_SDC_STS,
	OFFSET_SDC_RESP0,
	OFFSET_SDC_RESP1,
	OFFSET_SDC_RESP2,
	OFFSET_SDC_RESP3,
	OFFSET_SDC_BLK_NUM,
	OFFSET_SDC_VOL_CHG,
	OFFSET_SDC_CSTS,
	OFFSET_SDC_CSTS_EN,
	OFFSET_SDC_DCRC_STS,
	OFFSET_SDC_ADV_CFG0,
	OFFSET_EMMC_CFG0,
	OFFSET_EMMC_CFG1,
	OFFSET_EMMC_STS,
	OFFSET_EMMC_IOCON,
	OFFSET_SDC_ACMD_RESP,
	OFFSET_MSDC_DMA_SA_HIGH,
	OFFSET_MSDC_DMA_SA,
	OFFSET_MSDC_DMA_CA,
	OFFSET_MSDC_DMA_CTRL,
	OFFSET_MSDC_DMA_CFG,
	OFFSET_MSDC_DMA_LEN,
	OFFSET_MSDC_DBG_SEL,
	OFFSET_MSDC_DBG_OUT,
	OFFSET_MSDC_PATCH_BIT0,
	OFFSET_MSDC_PATCH_BIT1,
	OFFSET_MSDC_PATCH_BIT2,
	OFFSET_MSDC_PAD_TUNE0,
	OFFSET_MSDC_PAD_TUNE1,
	OFFSET_MSDC_HW_DBG,
	OFFSET_MSDC_VERSION,

	OFFSET_EMMC50_PAD_DS_TUNE,
	OFFSET_EMMC50_PAD_CMD_TUNE,
	OFFSET_EMMC50_PAD_DAT01_TUNE,
	OFFSET_EMMC50_PAD_DAT23_TUNE,
	OFFSET_EMMC50_PAD_DAT45_TUNE,
	OFFSET_EMMC50_PAD_DAT67_TUNE,
	OFFSET_EMMC51_CFG0,
	OFFSET_EMMC50_CFG0,
	OFFSET_EMMC50_CFG1,
	OFFSET_EMMC50_CFG2,
	OFFSET_EMMC50_CFG3,
	OFFSET_EMMC50_CFG4,
	OFFSET_SDC_FIFO_CFG,
	OFFSET_MSDC_AES_SEL,

#ifdef CONFIG_MTK_HW_FDE
	OFFSET_EMMC52_AES_EN,
	OFFSET_EMMC52_AES_CFG_GP0,
	OFFSET_EMMC52_AES_IV0_GP0,
	OFFSET_EMMC52_AES_IV1_GP0,
	OFFSET_EMMC52_AES_IV2_GP0,
	OFFSET_EMMC52_AES_IV3_GP0,
	OFFSET_EMMC52_AES_CTR0_GP0,
	OFFSET_EMMC52_AES_CTR1_GP0,
	OFFSET_EMMC52_AES_CTR2_GP0,
	OFFSET_EMMC52_AES_CTR3_GP0,
	OFFSET_EMMC52_AES_KEY0_GP0,
	OFFSET_EMMC52_AES_KEY1_GP0,
	OFFSET_EMMC52_AES_KEY2_GP0,
	OFFSET_EMMC52_AES_KEY3_GP0,
	OFFSET_EMMC52_AES_KEY4_GP0,
	OFFSET_EMMC52_AES_KEY5_GP0,
	OFFSET_EMMC52_AES_KEY6_GP0,
	OFFSET_EMMC52_AES_KEY7_GP0,
	OFFSET_EMMC52_AES_TKEY0_GP0,
	OFFSET_EMMC52_AES_TKEY1_GP0,
	OFFSET_EMMC52_AES_TKEY2_GP0,
	OFFSET_EMMC52_AES_TKEY3_GP0,
	OFFSET_EMMC52_AES_TKEY4_GP0,
	OFFSET_EMMC52_AES_TKEY5_GP0,
	OFFSET_EMMC52_AES_TKEY6_GP0,
	OFFSET_EMMC52_AES_TKEY7_GP0,
	OFFSET_EMMC52_AES_SWST,
	OFFSET_EMMC52_AES_CFG_GP1,
	OFFSET_EMMC52_AES_IV0_GP1,
	OFFSET_EMMC52_AES_IV1_GP1,
	OFFSET_EMMC52_AES_IV2_GP1,
	OFFSET_EMMC52_AES_IV3_GP1,
	OFFSET_EMMC52_AES_CTR0_GP1,
	OFFSET_EMMC52_AES_CTR1_GP1,
	OFFSET_EMMC52_AES_CTR2_GP1,
	OFFSET_EMMC52_AES_CTR3_GP1,
	OFFSET_EMMC52_AES_KEY0_GP1,
	OFFSET_EMMC52_AES_KEY1_GP1,
	OFFSET_EMMC52_AES_KEY2_GP1,
	OFFSET_EMMC52_AES_KEY3_GP1,
	OFFSET_EMMC52_AES_KEY4_GP1,
	OFFSET_EMMC52_AES_KEY5_GP1,
	OFFSET_EMMC52_AES_KEY6_GP1,
	OFFSET_EMMC52_AES_KEY7_GP1,
	OFFSET_EMMC52_AES_TKEY0_GP1,
	OFFSET_EMMC52_AES_TKEY1_GP1,
	OFFSET_EMMC52_AES_TKEY2_GP1,
	OFFSET_EMMC52_AES_TKEY3_GP1,
	OFFSET_EMMC52_AES_TKEY4_GP1,
	OFFSET_EMMC52_AES_TKEY5_GP1,
	OFFSET_EMMC52_AES_TKEY6_GP1,
	OFFSET_EMMC52_AES_TKEY7_GP1,
#endif

	0xFFFF /*as mark of end */
};

u16 msdc_offsets_top[] = {
	OFFSET_EMMC_TOP_CONTROL,
	OFFSET_EMMC_TOP_CMD,
	OFFSET_TOP_EMMC50_PAD_CTL0,
	OFFSET_TOP_EMMC50_PAD_DS_TUNE,
	OFFSET_TOP_EMMC50_PAD_DAT0_TUNE,
	OFFSET_TOP_EMMC50_PAD_DAT1_TUNE,
	OFFSET_TOP_EMMC50_PAD_DAT2_TUNE,
	OFFSET_TOP_EMMC50_PAD_DAT3_TUNE,
	OFFSET_TOP_EMMC50_PAD_DAT4_TUNE,
	OFFSET_TOP_EMMC50_PAD_DAT5_TUNE,
	OFFSET_TOP_EMMC50_PAD_DAT6_TUNE,
	OFFSET_TOP_EMMC50_PAD_DAT7_TUNE,

	0xFFFF /*as mark of end */
};
