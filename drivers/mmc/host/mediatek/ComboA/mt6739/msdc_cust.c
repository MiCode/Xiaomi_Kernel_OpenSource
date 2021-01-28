/* Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
#if !defined(FPGA_PLATFORM)
#ifdef POWER_READY
#include "include/pmic_regulator.h"
#endif
#endif


struct msdc_host *mtk_msdc_host[HOST_MAX_NUM];
EXPORT_SYMBOL(mtk_msdc_host);

int g_dma_debug[HOST_MAX_NUM];
u32 latest_int_status[HOST_MAX_NUM];

unsigned int msdc_latest_transfer_mode[HOST_MAX_NUM] = {
	/* 0 for PIO; 1 for DMA; 3 for nothing */
	MODE_NONE,
	MODE_NONE,
};

unsigned int msdc_latest_op[HOST_MAX_NUM] = {
	/* 0 for read; 1 for write; 2 for nothing */
	OPER_TYPE_NUM,
	OPER_TYPE_NUM,
};

/* for debug zone */
unsigned int sd_debug_zone[HOST_MAX_NUM] = {
	0,
	0,
};
/* for enable/disable register dump */
unsigned int sd_register_zone[HOST_MAX_NUM] = {
	1,
	1,
};
/* mode select */
u32 dma_size[HOST_MAX_NUM] = {
	512,
	512,
};

u32 drv_mode[HOST_MAX_NUM] = {
	MODE_SIZE_DEP, /* using DMA or not depend on the size */
	MODE_SIZE_DEP,
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

static void __iomem *infracfg_ao_base;
static void __iomem *apmixed_base;
static void __iomem *topckgen_base;
static void __iomem *sleep_base;
#endif

void __iomem *msdc_io_cfg_bases[HOST_MAX_NUM];

/**************************************************************/
/* Section 2: Power                                           */
/**************************************************************/
#if !defined(FPGA_PLATFORM)
int msdc_regulator_set_and_enable(struct regulator *reg, int powerVolt)
{
	regulator_set_voltage(reg, powerVolt, powerVolt);
	return regulator_enable(reg);
}

void msdc_ldo_power(u32 on, struct regulator *reg, int voltage_mv, u32 *status)
{
	int voltage_uv = voltage_mv * 1000;

	if (reg == NULL)
		return;

	if (on) { /* want to power on */
		if (*status == 0) {  /* can power on */
			/* Comment out to reduce log */
			/* pr_info("msdc power on<%d>\n", voltage_uv); */
			(void)msdc_regulator_set_and_enable(reg, voltage_uv);
			*status = voltage_uv;
		} else if (*status == voltage_uv) {
			pr_notice("msdc power on <%d> again!\n", voltage_uv);
		} else {
			regulator_disable(reg);
			(void)msdc_regulator_set_and_enable(reg, voltage_uv);
			*status = voltage_uv;
		}
	} else {  /* want to power off */
		if (*status != 0) {  /* has been powerred on */
			/* Comment out to reduce log */
			/* pr_info("msdc power off\n"); */
			(void)regulator_disable(reg);
			*status = 0;
		} else {
			pr_notice("msdc not power on\n");
		}
	}
}

void msdc_dump_ldo_sts(char **buff, unsigned long *size,
	struct seq_file *m, struct msdc_host *host)
{
#ifdef POWER_READY
	u32 ldo_en = 0, ldo_vol = 0, ldo_cal = 0;
	u32 id = host->id;

	switch (id) {
	case 0:
		pmic_read_interface_nolock(REG_VEMC_EN, &ldo_en, MASK_VEMC_EN,
			SHIFT_VEMC_EN);
		pmic_read_interface_nolock(REG_VEMC_VOSEL, &ldo_vol,
			MASK_VEMC_VOSEL, SHIFT_VEMC_VOSEL);
		pmic_read_interface_nolock(REG_VEMC_VOSEL_CAL, &ldo_cal,
			MASK_VEMC_VOSEL_CAL, SHIFT_VEMC_VOSEL_CAL);
		SPREAD_PRINTF(buff, size, m,
		" VEMC_EN=0x%x, VEMC_VOL=0x%x [0x2(2V9),0x3(3V),0x5(3V3)], VEMC_CAL=0x%x\n",
			ldo_en, ldo_vol, ldo_cal);
		break;
	case 1:
		pmic_read_interface_nolock(REG_VMC_EN, &ldo_en, MASK_VMC_EN,
			SHIFT_VMC_EN);
		pmic_read_interface_nolock(REG_VMC_VOSEL, &ldo_vol,
			MASK_VMC_VOSEL, SHIFT_VMC_VOSEL);
		pmic_read_interface_nolock(REG_VMC_VOSEL_CAL, &ldo_cal,
			MASK_VMC_VOSEL_CAL, SHIFT_VMC_VOSEL_CAL);
		SPREAD_PRINTF(buff, size, m,
		" VMC_EN=0x%x, VMC_VOL=0x%x [0x4(1V84),0xa(2V9),0xb(3V),0xd(3V3)], VMC_CAL=0x%x\n",
			ldo_en, ldo_vol, ldo_cal);

		pmic_read_interface_nolock(REG_VMCH_EN, &ldo_en, MASK_VMCH_EN,
			SHIFT_VMCH_EN);
		pmic_read_interface_nolock(REG_VMCH_VOSEL, &ldo_vol,
			MASK_VMCH_VOSEL, SHIFT_VMCH_VOSEL);
		pmic_read_interface_nolock(REG_VMCH_VOSEL_CAL, &ldo_cal,
			MASK_VMCH_VOSEL_CAL, SHIFT_VMCH_VOSEL_CAL);
		SPREAD_PRINTF(buff, size, m,
		" VMCH_EN=0x%x, VMCH_VOL=0x%x [0x2(2V9),0x3(3V),0x5(3V3)], VMCH_CAL=0x%x\n",
			ldo_en, ldo_vol, ldo_cal);
		break;
	default:
		break;
	}
#endif
}

void msdc_sd_power_switch(struct msdc_host *host, u32 on)
{
#ifdef POWER_READY
	if (host->id == 1) {
		pmic_config_interface(REG_VMC_VOSEL_CAL,
			VMC_VOSEL_CAL_mV(SD1V8_VOL_ACTUAL - VOL_1800),
			MASK_VMC_VOSEL_CAL, SHIFT_VMC_VOSEL_CAL);
		msdc_ldo_power(on, host->mmc->supply.vqmmc, VOL_1800,
			&host->power_io);
		/* For 1.8V 28mm, Set TDSEL as 0, Set RDSEL as 0 */
		msdc_set_tdsel(host, MSDC_TDRDSEL_CUST, 0);
		msdc_set_rdsel(host, MSDC_TDRDSEL_CUST, 0);
		host->hw->driving_applied = &host->hw->driving_sdr50;
		msdc_set_driving(host, host->hw->driving_applied);
	}
#endif
}

void msdc_sdio_power(struct msdc_host *host, u32 on)
{
#ifdef POWER_READY
	if (host->id == 2) {
		host->power_flash = VOL_1800 * 1000;
		host->power_io = VOL_1800 * 1000;
	}
#endif
}

void msdc_power_calibration_init(struct msdc_host *host)
{
#ifdef POWER_READY
	if (host->hw->host_function == MSDC_EMMC) {
		pmic_config_interface(REG_VEMC_VOSEL_CAL,
			VEMC_VOSEL_CAL_mV(EMMC_VOL_ACTUAL - VOL_3000),
			MASK_VEMC_VOSEL_CAL, SHIFT_VEMC_VOSEL_CAL);

	} else if (host->hw->host_function == MSDC_SD) {
		/* move to msdc_sd_power() and msdc_sd_power_switch()
		 * since 3V and 1V8 need different calibration setting
		 */
	}
#endif
}

int msdc_oc_check(struct msdc_host *host, u32 en)
{
	int ret = 0;

#ifdef POWER_READY
	u32 val = 0;

	if (host->id != 1)
		goto out;

	if (en) {
		pmic_config_interface(REG_VMCH_OC_MASK, 1,
				MASK_VMCH_OC_MASK, SHIFT_VMCH_OC_MASK);
		pmic_config_interface(REG_VMCH_OC_EN, 1,
				MASK_VMCH_OC_EN, SHIFT_VMCH_OC_EN);

		pmic_read_interface(REG_VMCH_OC_RAW_STATUS, &val,
			MASK_VMCH_OC_RAW_STATUS, SHIFT_VMCH_OC_RAW_STATUS);

		if (val) {
			pr_notice("msdc1 OC status = %x\n", val);
			host->power_control(host, 0);
			msdc_set_bad_card_and_remove(host);

			pmic_config_interface(REG_VMCH_OC_STATUS, 1,
				MASK_VMCH_OC_STATUS, SHIFT_VMCH_OC_STATUS);

			ret = 1;
		}
	} else {
		pmic_config_interface(REG_VMCH_OC_EN, 0,
				MASK_VMCH_OC_EN, SHIFT_VMCH_OC_EN);
		pmic_config_interface(REG_VMCH_OC_MASK, 0,
				MASK_VMCH_OC_MASK, SHIFT_VMCH_OC_MASK);
	}

out:

#endif

	return ret;
}

void msdc_emmc_power(struct msdc_host *host, u32 on)
{
#ifdef POWER_READY
	void __iomem *base = host->base;

	if (on == 0) {
		if ((MSDC_READ32(MSDC_PS) & 0x10000) != 0x10000)
			emmc_sleep_failed = 1;
	} else {
		msdc_set_driving(host, &host->hw->driving);
		/* For 1.8V 28mm, Set TDSEL as 0, Set RDSEL as 0 */
		msdc_set_tdsel(host, MSDC_TDRDSEL_CUST, 0);
		msdc_set_rdsel(host, MSDC_TDRDSEL_CUST, 0);
	}

	msdc_ldo_power(on, host->mmc->supply.vmmc, VOL_3000,
		&host->power_flash);

	pr_info("msdc%d power %s\n", host->id, (on ? "on" : "off"));

#ifdef MTK_MSDC_BRINGUP_DEBUG
	msdc_dump_ldo_sts(host);
#endif
#endif
}

void msdc_sd_power(struct msdc_host *host, u32 on)
{
#ifdef POWER_READY
	u32 card_on = on;

	switch (host->id) {
	case 1:
		msdc_set_driving(host, &host->hw->driving);
		/* For 3V 28mm, Set TDSEL as 0xA, Set RDSEL as 0xC */
		msdc_set_tdsel(host, MSDC_TDRDSEL_CUST, 0xA);
		msdc_set_rdsel(host, MSDC_TDRDSEL_CUST, 0xC);
		if (host->hw->flags & MSDC_SD_NEED_POWER)
			card_on = 1;
		if (on) {
			pmic_config_interface(REG_VMCH_VOSEL_CAL,
				VMCH_VOSEL_CAL_mV(SD_VOL_ACTUAL - VOL_3000),
				MASK_VMCH_VOSEL_CAL, SHIFT_VMCH_VOSEL_CAL);
			pmic_config_interface(REG_VMC_VOSEL_CAL,
				VMC_VOSEL_CAL_mV(SD_VOL_ACTUAL - VOL_3000),
				MASK_VMC_VOSEL_CAL, SHIFT_VMC_VOSEL_CAL);
		}
		/* VMCH VOLSEL */
		msdc_ldo_power(card_on, host->mmc->supply.vmmc, VOL_3000,
			&host->power_flash);
		/* VMC VOLSEL */
		msdc_ldo_power(on, host->mmc->supply.vqmmc, VOL_3000,
			&host->power_io);
		pr_info("msdc%d power %s\n", host->id, (on ? "on" : "off"));
		break;

	default:
		break;
	}

#ifdef MTK_MSDC_BRINGUP_DEBUG
	msdc_dump_ldo_sts(host);
#endif
#endif
}

void msdc_sd_power_off(void)
{
#ifdef POWER_READY
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
#if defined(VCOREFS_READY)
	/* FIX ME,
	 * comment out due to not implemented yet on MT6739 kernel-4.14
	 */
	/*
	 * SPREAD_PRINTF(buff, size, m, "%s: Vcore %d\n", __func__,
		get_cur_vcore_opp());
	 */
#endif
}

/* FIXME: check if sleep_base is correct */
void msdc_dump_dvfs_reg(char **buff, unsigned long *size, struct seq_file *m,
	struct msdc_host *host)
{
	void __iomem *base = host->base;

	pr_info("SDC_STS:0x%x", MSDC_READ32(SDC_STS));
	if (sleep_base) {
		/* bit24 high is in dvfs request status, which cause sdc busy */
		pr_info("DVFS_REQUEST@0x%p = 0x%x, bit[24] shall 0b\n",
			sleep_base + 0x478,
			MSDC_READ32(sleep_base + 0x478));
	}
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

#if defined(MSDC_HQA) && defined(SDIO_HQA)
#error shall not define both MSDC_HQA and SDIO_HQA
#endif
#if defined(MSDC_HQA) || defined(SDIO_HQA)
/* #define MSDC_HQA_HV */
#define MSDC_HQA_NV
/* #define MSDC_HQA_LV */
/* #define MSDC_HQA_HVCore_LVio */
/* #define MSDC_HQA_LVCore_HVio */

void msdc_HQA_set_voltage(struct msdc_host *host)
{
	static int vcore_orig = -1, vio_cal_orig = -1;
#if defined(MSDC_HQA_HV) || defined(MSDC_HQA_LV)
	u32 vcore, vio_cal = 0, val_delta;
#endif

	if (host->is_autok_done == 1)
		return;

	if (vcore_orig < 0)
		pmic_read_interface(REG_VCORE_VOSEL, &vcore_orig,
			MASK_VCORE_VOSEL, SHIFT_VCORE_VOSEL);
	if (vio_cal_orig < 0)
		pmic_read_interface(REG_VIO18_VOCAL, &vio_cal_orig,
			MASK_VIO18_VOCAL, SHIFT_VIO18_VOCAL);
	pr_info("[MSDC~%d HQA] orig Vcore 0x%x, Vio_cal 0x%x\n",
		host->id, vcore_orig, vio_cal_orig);

#if !defined(MSDC_HQA_NV)
	val_delta = (VCORE_MIN_UV + vcore_orig * VCORE_STEP_UV)
		/ 20 / VCORE_STEP_UV;

	#ifdef MSDC_HQA_HV
	vcore = vcore_orig + val_delta;
	vio_cal = 10; /* MT6357 support at most +10 steps */
	#endif

	#ifdef MSDC_HQA_HVCore_LVio
	vcore = vcore_orig + val_delta;
	vio_cal = 0; /* MT6357 does not support minus adjustion */
	#endif

	#ifdef MSDC_HQA_LV
	vcore = vcore_orig - val_delta;
	vio_cal = 0; /* MT6357 does not support minus adjustion */
	#endif

	#ifdef MSDC_HQA_LVCore_HVio
	vcore = vcore_orig - val_delta;
	vio_cal = 10; /* MT6357 support at most +10 steps */
	#endif

	pmic_config_interface(REG_VCORE_VOSEL, vcore,
		MASK_VCORE_VOSEL, SHIFT_VCORE_VOSEL);

	pmic_config_interface(REG_VIO18_VOCAL, vio_cal,
		MASK_VIO18_VOCAL, SHIFT_VIO18_VOCAL);

	pr_info("[MSDC%d HQA] adj Vcore 0x%x, Vio_cal 0x%x\n",
		host->id, vcore, vio_cal);
#endif
}
#endif


/**************************************************************/
/* Section 3: Clock                                           */
/**************************************************************/
#if !defined(FPGA_PLATFORM)
u32 hclks_msdc0[] = { MSDC0_SRC_0, MSDC0_SRC_1, MSDC0_SRC_2, MSDC0_SRC_3,
		      MSDC0_SRC_4, MSDC0_SRC_5, MSDC0_SRC_6, MSDC0_SRC_7};

u32 hclks_msdc1[] = { MSDC1_SRC_0, MSDC1_SRC_1, MSDC1_SRC_2, MSDC1_SRC_3,
		      MSDC1_SRC_4, MSDC1_SRC_5, MSDC1_SRC_6, MSDC1_SRC_7};

u32 *hclks_msdc_all[] = {
	hclks_msdc0,
	hclks_msdc1,
};
u32 *hclks_msdc;

int msdc_get_ccf_clk_pointer(struct platform_device *pdev,
	struct msdc_host *host)
{
#ifdef CLOCK_READY
	u32 clk_freq;
	static char const * const clk_names[] = {
		MSDC0_CLK_NAME, MSDC1_CLK_NAME
	};
	static char const * const hclk_names[] = {
		MSDC0_HCLK_NAME, MSDC1_HCLK_NAME
	};

	if  (clk_names[pdev->id]) {
		host->clk_ctl = devm_clk_get(&pdev->dev, clk_names[pdev->id]);
		if (IS_ERR(host->clk_ctl)) {
			pr_notice("[msdc%d] cannot get clk ctrl\n", pdev->id);
			return 1;
		}
		if (clk_prepare_enable(host->clk_ctl)) {
			pr_notice("[msdc%d] cannot prepare clk ctrl\n",
				pdev->id);
			return 1;
		}
	}

	if  (hclk_names[pdev->id]) {
		host->hclk_ctl = devm_clk_get(&pdev->dev, hclk_names[pdev->id]);
		if (IS_ERR(host->hclk_ctl)) {
			pr_notice("[msdc%d] cannot get hclk ctrl\n", pdev->id);
			return 1;
		}
		if (clk_prepare_enable(host->hclk_ctl)) {
			pr_notice("[msdc%d] cannot prepare hclk ctrl\n", pdev->id);
			return 1;
		}
	}

	if (host->clk_ctl) {
		clk_freq = clk_get_rate(host->clk_ctl);
		if (clk_freq > 0)
			host->hclk = clk_freq;
	}

	pr_info("[msdc%d] hclk:%dHz, clk_ctl:%p, hclk_ctl:%p\n",
		pdev->id, host->hclk, host->clk_ctl, host->hclk_ctl);

#endif

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
			"MSDC0 CLK_MUX[0x%p][18:16]= 0x%x, CLK_CG[0x%p][9:7] = %d\n",
			topckgen_base + 0x70,
			(MSDC_READ32(topckgen_base + 0x70) >> 16) & 7,
			infracfg_ao_base + 0xC8,
			(MSDC_READ32(infracfg_ao_base + 0xC8) >> 7) & 0x7);
		buf_ptr += sprintf(buf_ptr,
			"MSDC1 CLK_MUX[0x%p][26:24]= 0x%x, CLK_CG[0x%p][1] = %d\n",
			topckgen_base + 0x70,
			(MSDC_READ32(topckgen_base + 0x70) >> 24) & 7,
			infracfg_ao_base + 0xC8,
			(MSDC_READ32(infracfg_ao_base + 0xC8) >> 10) & 0x1);
		*buf_ptr = '\0';
		SPREAD_PRINTF(buff, size, m, "%s", buffer);
	}

	buf_ptr = buffer;
	if (apmixed_base) {
		/* bit0 is enables PLL, 0: disable 1: enable */
		buf_ptr += sprintf(buf_ptr, "MSDCPLL_CON0@0x%p = 0x%x, bit[0] shall 1b\n",
			apmixed_base + MSDCPLL_CON0_OFFSET,
			MSDC_READ32(apmixed_base + MSDCPLL_CON0_OFFSET));

		buf_ptr += sprintf(buf_ptr, "MSDCPLL_CON1@0x%p = 0x%x\n",
			apmixed_base + MSDCPLL_CON1_OFFSET,
			MSDC_READ32(apmixed_base + MSDCPLL_CON1_OFFSET));

		buf_ptr += sprintf(buf_ptr, "MSDCPLL_CON2@0x%p = 0x%x\n",
			apmixed_base + MSDCPLL_CON2_OFFSET,
			MSDC_READ32(apmixed_base + MSDCPLL_CON2_OFFSET));

		buf_ptr += sprintf(buf_ptr, "MSDCPLL_PWR_CON0@0x%p = 0x%x, bit[0] shall 1b\n",
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

	/* udelay(10); */

	/* MSDC_SET_FIELD(MSDC_CFG, MSDC_CFG_MODE, MSDC_SDMMC); */

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
	int i;
	void __iomem *base = host->base;
	unsigned long polling_tmo = 0;
	void __iomem *pupd_addr[3] = {
		MSDC1_PUPD_DAT0_ADDR,
		MSDC1_PUPD_DAT1_ADDR,
		MSDC1_PUPD_DAT2_ADDR
	};
	u32 pupd_mask[3] = {
		MSDC1_PUPD_DAT0_MASK,
		MSDC1_PUPD_DAT1_MASK,
		MSDC1_PUPD_DAT2_MASK
	};
	u32 check_patterns[3] = {0xE0000, 0xD0000, 0xB0000};
	u32 orig_pull;

	if (host->id != 1)
		return 0;

	if (host->block_bad_card)
		goto SET_BAD_CARD;

	for (i = 0; i < 3; i++) {
		MSDC_GET_FIELD(pupd_addr[i], pupd_mask[i], orig_pull);
		MSDC_SET_FIELD(pupd_addr[i], pupd_mask[i], 1);
		polling_tmo = jiffies + POLLING_PINS;
		while ((MSDC_READ32(MSDC_PS) & 0xF0000) != check_patterns[i]) {
			if (time_after(jiffies, polling_tmo)) {
				/* Exception handling for some good card with
				 * pull up strength greater than pull up strength
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
	}

	return 0;

SET_BAD_CARD:
	msdc_set_bad_card_and_remove(host);

	return 1;
}

void msdc_dump_padctl_by_id(char **buff, unsigned long *size,
	struct seq_file *m, u32 id)
{
	u32 val;

	if (!gpio_base || !msdc_io_cfg_bases[id]) {
		SPREAD_PRINTF(buff, size, m,
			"err: gpio_base=%p, msdc_io_cfg_bases[%d]=%p\n",
			gpio_base, id, msdc_io_cfg_bases[id]);
		return;
	}

	if (id == 0) {
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 MODE4 [0x%p] =0x%8x\tshould: 0x11??????\n",
			MSDC0_GPIO_MODE4, MSDC_READ32(MSDC0_GPIO_MODE4));
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 MODE5 [0x%p] =0x%8x\tshould: 0x11111111\n",
			MSDC0_GPIO_MODE5, MSDC_READ32(MSDC0_GPIO_MODE5));
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 MODE6 [0x%p] =0x%8x\tshould: 0x??????11\n",
			MSDC0_GPIO_MODE6, MSDC_READ32(MSDC0_GPIO_MODE6));
		MSDC_GET_FIELD(MSDC0_GPIO_IES_ADDR, MSDC0_IES_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 IES   [0x%p] =0x%8x\tshould: 0xfff\n",
			MSDC0_GPIO_IES_ADDR, val);
		MSDC_GET_FIELD(MSDC0_GPIO_SMT_ADDR, MSDC0_SMT_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 SMT   [0x%p] =0x%8x\tshould: 0x1f\n",
			MSDC0_GPIO_SMT_ADDR, val);
		MSDC_GET_FIELD(MSDC0_GPIO_TDSEL_ADDR, MSDC0_TDSEL_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 TDSEL [0x%p] =0x%8x\tshould: 0x0\n",
			MSDC0_GPIO_TDSEL_ADDR, val);
		MSDC_GET_FIELD(MSDC0_GPIO_RDSEL0_ADDR, MSDC0_RDSEL0_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 RDSEL [0x%p] =0x%8x\tshould: 0x0\n",
			MSDC0_GPIO_RDSEL0_ADDR, val);
		MSDC_GET_FIELD(MSDC0_GPIO_RDSEL1_ADDR, MSDC0_RDSEL1_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 RDSE1 [0x%p] =0x%8x\tshould: 0x0\n",
			MSDC0_GPIO_RDSEL1_ADDR, val);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_ADDR, MSDC0_DRV0_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 DRV   [0x%p] =0x%8x\tshould: 0x924\n",
			MSDC0_GPIO_DRV0_ADDR, val);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV1_ADDR, MSDC0_DRV1_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 DRV1  [0x%p] =0x%8x\tshould: 0x924924\n",
			MSDC0_GPIO_DRV1_ADDR, val);
		MSDC_GET_FIELD(MSDC0_GPIO_SR_ADDR, MSDC0_SR_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 SR   [0x%p] =0x%8x\tshould: 0xTTT\n",
			MSDC0_GPIO_SR_ADDR, val);
		SPREAD_PRINTF(buff, size, m,
			"PUPD/R1/R0: dat/cmd:0/0/1, clk/dst: 1/1/0\n");
		MSDC_GET_FIELD(MSDC0_GPIO_PUPD_ADDR, MSDC0_PUPD_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 PUPD0 [0x%p] =0x%8x\tshould: 0x401\n",
			MSDC0_GPIO_PUPD_ADDR, val);
		MSDC_GET_FIELD(MSDC0_GPIO_R0_ADDR, MSDC0_R0_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 R0    [0x%p] =0x%8x\tshould: 0x3fe\n",
			MSDC0_GPIO_R0_ADDR, val);
		MSDC_GET_FIELD(MSDC0_GPIO_R1_ADDR, MSDC0_R1_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC0 R1    [0x%p] =0x%8x\tshould: 0x401\n",
			MSDC0_GPIO_R1_ADDR, val);

	} else if (id == 1) {
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 MODE8  [0x%p] =0x%8x\tshould: 0x1???????\n",
			MSDC1_GPIO_MODE8, MSDC_READ32(MSDC1_GPIO_MODE8));
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 MODE9  [0x%p] =0x%8x\tshould: 0x???11111\n",
			MSDC1_GPIO_MODE9, MSDC_READ32(MSDC1_GPIO_MODE9));
		MSDC_GET_FIELD(MSDC1_GPIO_IES_ADDR, MSDC1_IES_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 IES    [0x%p] =0x%8x\tshould: 0x3f\n",
			MSDC1_GPIO_IES_ADDR, val);
		MSDC_GET_FIELD(MSDC1_GPIO_SMT_ADDR, MSDC1_SMT_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 SMT    [0x%p] =0x%8x\tshould: 0x3f\n",
			MSDC1_GPIO_SMT_ADDR, val);
		MSDC_GET_FIELD(MSDC1_GPIO_TDSEL_ADDR, MSDC1_TDSEL_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 TDSEL  [0x%p] =0x%8x\n",
			MSDC1_GPIO_TDSEL_ADDR, val);
		SPREAD_PRINTF(buff, size, m,
			"should 1.8v: sleep: TBD, awake: 0xaaa\n");
		MSDC_GET_FIELD(MSDC1_GPIO_RDSEL_ADDR, MSDC1_RDSEL_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 RDSEL0  [0x%p] =0x%8x\n",
			MSDC1_GPIO_RDSEL_ADDR, val);
		SPREAD_PRINTF(buff, size, m,
			"1.8V: TBD, 3.0v: 0xc30c\n");
		MSDC_GET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 DRV    [0x%p] =0x%8x\tshould: 0xdb\n",
			MSDC1_GPIO_DRV_ADDR, val);
		MSDC_GET_FIELD(MSDC1_GPIO_SR_ADDR, MSDC1_SR_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 SR     [0x%p] =0x%8x\tshould: 0x0\n",
			MSDC1_GPIO_SR_ADDR, val);
		MSDC_GET_FIELD(MSDC1_GPIO_PUPD_ADDR, MSDC1_PUPD_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 PUPD   [0x%p] =0x%8x\tshould: 0x1\n",
			MSDC1_GPIO_PUPD_ADDR, val);
		MSDC_GET_FIELD(MSDC1_GPIO_R0_ADDR, MSDC1_R0_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 R0     [0x%p] =0x%8x\tshould: 0x0\n",
			MSDC1_GPIO_R0_ADDR, val);
		MSDC_GET_FIELD(MSDC1_GPIO_R1_ADDR, MSDC1_R1_ALL_MASK, val);
		SPREAD_PRINTF(buff, size, m,
			"MSDC1 R1     [0x%p] =0x%8x\tshould: 0x3f\n",
			MSDC1_GPIO_R1_ADDR, val);
	}
}

void msdc_set_pin_mode(struct msdc_host *host)
{
	if (host->id == 0) {
		MSDC_SET_FIELD(MSDC0_GPIO_MODE4, 0xFF000000, 0x11);
		MSDC_SET_FIELD(MSDC0_GPIO_MODE5, 0xFFFFFFFF, 0x11111111);
		MSDC_SET_FIELD(MSDC0_GPIO_MODE6, 0xFF, 0x11);
	} else if (host->id == 1) {
		MSDC_SET_FIELD(MSDC1_GPIO_MODE8, 0xF0000000, 0x1);
		MSDC_SET_FIELD(MSDC1_GPIO_MODE9, 0x000FFFFF, 0x11111);
	}
}

void msdc_set_ies_by_id(u32 id, int set_ies)
{
	if (id == 0) {
		MSDC_SET_FIELD(MSDC0_GPIO_IES_ADDR, MSDC0_IES_ALL_MASK,
			(set_ies ? 0xFFF : 0));
	} else if (id == 1) {
		MSDC_SET_FIELD(MSDC1_GPIO_IES_ADDR, MSDC1_IES_ALL_MASK,
			(set_ies ? 0x3F : 0));
	}
}

void msdc_set_smt_by_id(u32 id, int set_smt)
{
	if (id == 0) {
		MSDC_SET_FIELD(MSDC0_GPIO_SMT_ADDR, MSDC0_SMT_ALL_MASK,
			(set_smt ? 0x1F : 0));
	} else if (id == 1) {
		MSDC_SET_FIELD(MSDC1_GPIO_SMT_ADDR, MSDC1_SMT_ALL_MASK,
			(set_smt ? 0x7 : 0));
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
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL_ADDR, MSDC0_TDSEL_CMD_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL_ADDR, MSDC0_TDSEL_DAT_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL_ADDR, MSDC0_TDSEL_CLK_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL_ADDR, MSDC0_TDSEL_RSTB_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_TDSEL_ADDR, MSDC0_TDSEL_DSL_MASK,
			cust_val);
	} else if (id == 1) {
		if (flag == MSDC_TDRDSEL_CUST)
			cust_val = value;
		else
			cust_val = 0;
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL_ADDR, MSDC1_TDSEL_CMD_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL_ADDR, MSDC1_TDSEL_DAT_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_TDSEL_ADDR, MSDC1_TDSEL_CLK_MASK,
			cust_val);
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
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_ADDR, MSDC0_RDSEL_CMD_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_ADDR, MSDC0_RDSEL_DAT_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_ADDR, MSDC0_RDSEL_CLK_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL0_ADDR, MSDC0_RDSEL_DSL_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC0_GPIO_RDSEL1_ADDR, MSDC0_RDSEL_RSTB_MASK,
			cust_val);
	} else if (id == 1) {
		if (flag == MSDC_TDRDSEL_CUST)
			cust_val = value;
		else
			cust_val = 0;
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL_ADDR, MSDC1_RDSEL_CMD_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL_ADDR, MSDC1_RDSEL_DAT_MASK,
			cust_val);
		MSDC_SET_FIELD(MSDC1_GPIO_RDSEL_ADDR, MSDC1_RDSEL_CLK_MASK,
			cust_val);
	}
}

void msdc_get_tdsel_by_id(u32 id, u32 *value)
{
	if (id == 0) {
		MSDC_GET_FIELD(MSDC0_GPIO_TDSEL_ADDR, MSDC0_TDSEL_CMD_MASK,
			*value);
	} else if (id == 1) {
		MSDC_GET_FIELD(MSDC1_GPIO_TDSEL_ADDR, MSDC1_TDSEL_CMD_MASK,
			*value);
	}
}

void msdc_get_rdsel_by_id(u32 id, u32 *value)
{
	if (id == 0) {
		MSDC_GET_FIELD(MSDC0_GPIO_RDSEL0_ADDR, MSDC0_RDSEL_CMD_MASK,
			*value);
	} else if (id == 1) {
		MSDC_GET_FIELD(MSDC1_GPIO_RDSEL_ADDR, MSDC1_RDSEL_CMD_MASK,
			*value);
	}
}

void msdc_set_sr_by_id(u32 id, int clk, int cmd, int dat, int rst, int ds)
{
	if (id == 0) {
		MSDC_SET_FIELD(MSDC0_GPIO_SR_ADDR, MSDC0_SR_CMD_MASK,
			(cmd != 0));
		MSDC_SET_FIELD(MSDC0_GPIO_SR_ADDR, MSDC0_SR_CLK_MASK,
			(clk != 0));
		MSDC_SET_FIELD(MSDC0_GPIO_SR_ADDR, MSDC0_SR_DAT_MASK,
			((dat != 0) ? 0xFF : 0));
		MSDC_SET_FIELD(MSDC0_GPIO_SR_ADDR, MSDC0_SR_DSL_MASK,
			(ds != 0));
		MSDC_SET_FIELD(MSDC0_GPIO_SR_ADDR, MSDC0_SR_RSTB_MASK,
			(rst != 0));
	} else if (id == 1) {
		MSDC_SET_FIELD(MSDC1_GPIO_SR_ADDR, MSDC1_SR_CMD_MASK,
			(cmd != 0));
		MSDC_SET_FIELD(MSDC1_GPIO_SR_ADDR, MSDC1_SR_CLK_MASK,
			(clk != 0));
		MSDC_SET_FIELD(MSDC1_GPIO_SR_ADDR, MSDC1_SR_DAT0_MASK,
			(dat != 0));
	}
}

void msdc_set_driving_by_id(u32 id, struct msdc_hw_driving *driving)
{
	if (id == 0) {
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_ADDR, MSDC0_DRV_CMD_MASK,
			driving->cmd_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_ADDR, MSDC0_DRV_CLK_MASK,
			driving->clk_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_ADDR, MSDC0_DRV_DAT0_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV0_ADDR, MSDC0_DRV_DAT1_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV1_ADDR, MSDC0_DRV_DAT2_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV1_ADDR, MSDC0_DRV_DAT3_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV1_ADDR, MSDC0_DRV_DAT4_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV1_ADDR, MSDC0_DRV_DAT5_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV1_ADDR, MSDC0_DRV_DAT6_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV1_ADDR, MSDC0_DRV_DAT7_MASK,
			driving->dat_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV1_ADDR, MSDC0_DRV_DSL_MASK,
			driving->ds_drv);
		MSDC_SET_FIELD(MSDC0_GPIO_DRV1_ADDR, MSDC0_DRV_RSTB_MASK,
			driving->rst_drv);
	} else if (id == 1) {
		MSDC_SET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_CMD_MASK,
			driving->cmd_drv);
		MSDC_SET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_CLK_MASK,
			driving->clk_drv);
		MSDC_SET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_DAT_MASK,
			driving->dat_drv);
	}
}

void msdc_get_driving_by_id(u32 id, struct msdc_hw_driving *driving)
{
	if (id == 0) {
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_ADDR, MSDC0_DRV_CMD_MASK,
			driving->cmd_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_ADDR, MSDC0_DRV_CLK_MASK,
			driving->clk_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV0_ADDR, MSDC0_DRV_DAT0_MASK,
			driving->dat_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV1_ADDR, MSDC0_DRV_DSL_MASK,
			driving->ds_drv);
		MSDC_GET_FIELD(MSDC0_GPIO_DRV1_ADDR, MSDC0_DRV_RSTB_MASK,
			driving->rst_drv);
	} else if (id == 1) {
		MSDC_GET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_CMD_MASK,
			driving->cmd_drv);
		MSDC_GET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_CLK_MASK,
			driving->clk_drv);
		MSDC_GET_FIELD(MSDC1_GPIO_DRV_ADDR, MSDC1_DRV_DAT_MASK,
			driving->dat_drv);
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
			MSDC_SET_FIELD(MSDC0_GPIO_PUPD_ADDR, MSDC0_PUPD_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC0_GPIO_R0_ADDR, MSDC0_R0_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC0_GPIO_R1_ADDR, MSDC0_R1_ALL_MASK, 0x0);
		} else if (mode == MSDC_PIN_PULL_DOWN) {
			/* Switch MSDC0_* to 50K ohm PD */
			MSDC_SET_FIELD(MSDC0_GPIO_PUPD_ADDR, MSDC0_PUPD_ALL_MASK, 0x7FF);
			MSDC_SET_FIELD(MSDC0_GPIO_R0_ADDR, MSDC0_R0_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC0_GPIO_R1_ADDR, MSDC0_R1_ALL_MASK, 0x7FF);
		} else if (mode == MSDC_PIN_PULL_UP) {
			/* Switch MSDC0_CLK to 50K ohm PD,
			 * MSDC0_CMD/MSDC0_DAT* to 10K ohm PU,
			 * MSDC0_DSL to 50K ohm PD
			 */
			MSDC_SET_FIELD(MSDC0_GPIO_PUPD_ADDR, MSDC0_PUPD_ALL_MASK, 0x401);
			MSDC_SET_FIELD(MSDC0_GPIO_R0_ADDR, MSDC0_R0_ALL_MASK, 0x3FE);
			MSDC_SET_FIELD(MSDC0_GPIO_R1_ADDR, MSDC0_R1_ALL_MASK, 0x401);
		}
	} else if (id == 1) {
		if (mode == MSDC_PIN_PULL_NONE) {
			/* Switch MSDC1_* to no ohm PU */
			MSDC_SET_FIELD(MSDC1_GPIO_PUPD_ADDR, MSDC1_PUPD_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC1_GPIO_R0_ADDR, MSDC1_R0_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC1_GPIO_R1_ADDR, MSDC1_R1_ALL_MASK, 0x0);
		} else if (mode == MSDC_PIN_PULL_DOWN) {
			/* Switch MSDC1_* to 50K ohm PD */
			MSDC_SET_FIELD(MSDC1_GPIO_PUPD_ADDR, MSDC1_PUPD_ALL_MASK, 0x3F);
			MSDC_SET_FIELD(MSDC1_GPIO_R0_ADDR, MSDC1_R0_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC1_GPIO_R1_ADDR, MSDC1_R1_ALL_MASK, 0x3F);
		} else if (mode == MSDC_PIN_PULL_UP) {
			/* Switch MSDC1_CLK to 50K ohm PD,
			 * MSDC1_CMD/MSDC1_DAT* to 50K ohm PU
			 */
			MSDC_SET_FIELD(MSDC1_GPIO_PUPD_ADDR, MSDC1_PUPD_ALL_MASK, 0x1);
			MSDC_SET_FIELD(MSDC1_GPIO_R0_ADDR, MSDC1_R0_ALL_MASK, 0x0);
			MSDC_SET_FIELD(MSDC1_GPIO_R1_ADDR, MSDC1_R1_ALL_MASK, 0x3F);
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
		"pinctl",
		"pinctl_hs400", "pinctl_hs200",
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
int msdc_of_parse(struct platform_device *pdev, struct mmc_host *mmc)
{
	struct device_node *np;
	struct msdc_host *host = mmc_priv(mmc);
	int ret = 0;
	int len = 0;
	u8 hw_dvfs_support, id;
	const char *dup_name;

	np = mmc->parent->of_node; /* mmcx node in project dts */

	if (of_property_read_u8(np, "index", &id)) {
		pr_notice("[%s] host index not specified in device tree\n",
			pdev->dev.of_node->name);
		return -1;
	}
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
	host->base_top = of_iomap(np, 1);
	if (host->base_top)
		pr_info("of_iomap for MSDC%d TOP base @ 0x%p\n",
			host->id, host->base_top);

	/* get irq # */
	host->irq = irq_of_parse_and_map(np, 0);
	pr_info("[msdc%d] get irq # %d\n", host->id, host->irq);
	WARN_ON(host->irq < 0);

#if !defined(FPGA_PLATFORM)
	/* get clk_src */
	if (of_property_read_u8(np, "clk_src", &host->hw->clk_src)) {
		pr_notice("[msdc%d] error: clk_src isn't found in device tree.\n",
			host->id);
		return -1;
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
			pr_notice("[msdc%d] cd_level isn't found in device tree\n",
				host->id);
	}

	ret = of_property_read_u8(np, "hw_dvfs", &hw_dvfs_support);
	if (ret || (hw_dvfs_support == 0))
		host->use_hw_dvfs = 0;
	else
		host->use_hw_dvfs = 1;

	msdc_get_register_settings(host, np);

#if !defined(FPGA_PLATFORM)
	msdc_get_pinctl_settings(host, np);

	mmc->supply.vmmc = regulator_get(mmc_dev(mmc), "vmmc");
	mmc->supply.vqmmc = regulator_get(mmc_dev(mmc), "vqmmc");
#else
	msdc_fpga_pwr_init();
#endif
	/* fix uaf(use afer free) issue:backup pdev->name,
	 * device_rename will free pdev->name
	 */
	pdev->name = kstrdup(pdev->name, GFP_KERNEL);
	/* device rename */
	if (host->id == 0)
		device_rename(mmc->parent, "bootdevice");
	else if (host->id == 1)
		device_rename(mmc->parent, "externdevice");

	dup_name = pdev->name;
	pdev->name = pdev->dev.kobj.name;
	kfree_const(dup_name);

	return host->id;
}

int msdc_dt_init(struct platform_device *pdev, struct mmc_host *mmc)
{
	int id;

#ifndef FPGA_PLATFORM
	static char const * const ioconfig_names[] = {
		MSDC0_IOCFG_NAME, MSDC1_IOCFG_NAME,
	};
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

	if (sleep_base == NULL) {
		np = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
		sleep_base = of_iomap(np, 0);
		pr_debug("of_iomap for sleep base @ 0x%p\n",
			sleep_base);
	}

	if (infracfg_ao_base == NULL) {
		np = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
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
