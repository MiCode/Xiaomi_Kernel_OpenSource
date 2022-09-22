// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: Flora Fu, MediaTek
 */

#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/regmap.h>
#include <linux/mfd/core.h>
#include <linux/mfd/mt6323/core.h>
#include <linux/mfd/mt6357/core.h>
#include <linux/mfd/mt6358/core.h>
#include <linux/mfd/mt6359p/core.h>
#include <linux/mfd/mt6366/core.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6323/registers.h>
#include <linux/mfd/mt6357/registers.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/mfd/mt6359p/registers.h>
#include <linux/mfd/mt6397/registers.h>

#include <linux/printk.h>

#define MT6323_RTC_BASE		0x8000
#define MT6323_RTC_SIZE		0x40

#define MT6358_RTC_BASE		0x0588
#define MT6358_RTC_SIZE		0x3c

#define MT6397_RTC_BASE		0xe000
#define MT6397_RTC_SIZE		0x3e

#define MT6323_PWRC_BASE	0x8000
#define MT6323_PWRC_SIZE	0x40

static const struct resource mt6323_rtc_resources[] = {
	DEFINE_RES_MEM(MT6323_RTC_BASE, MT6323_RTC_SIZE),
	DEFINE_RES_IRQ(MT6323_IRQ_STATUS_RTC),
};

static const struct resource mt6357_rtc_resources[] = {
	{
		.start = MT6358_RTC_BASE,
		.end   = MT6358_RTC_BASE + MT6358_RTC_SIZE,
		.flags = IORESOURCE_MEM,
	},
	{
		.start = MT6357_IRQ_RTC,
		.end   = MT6357_IRQ_RTC,
		.flags = IORESOURCE_IRQ,
	},
};

static const struct resource mt6358_rtc_resources[] = {
	DEFINE_RES_MEM(MT6358_RTC_BASE, MT6358_RTC_SIZE),
	DEFINE_RES_IRQ(MT6358_IRQ_RTC),
};

static const struct resource mt6359p_rtc_resources[] = {
	DEFINE_RES_MEM(MT6358_RTC_BASE, MT6358_RTC_SIZE),
	DEFINE_RES_IRQ(MT6359P_IRQ_RTC),
};

static const struct resource mt6397_rtc_resources[] = {
	DEFINE_RES_MEM(MT6397_RTC_BASE, MT6397_RTC_SIZE),
	DEFINE_RES_IRQ(MT6397_IRQ_RTC),
};

static const struct resource mt6323_keys_resources[] = {
	DEFINE_RES_IRQ(MT6323_IRQ_STATUS_PWRKEY),
	DEFINE_RES_IRQ(MT6323_IRQ_STATUS_FCHRKEY),
};

static const struct resource mt6357_keys_resources[] = {
	DEFINE_RES_IRQ(MT6357_IRQ_PWRKEY),
	DEFINE_RES_IRQ(MT6357_IRQ_HOMEKEY),
	DEFINE_RES_IRQ(MT6357_IRQ_PWRKEY_R),
	DEFINE_RES_IRQ(MT6357_IRQ_HOMEKEY_R),
};

static const struct resource mt6358_keys_resources[] = {
	DEFINE_RES_IRQ(MT6358_IRQ_PWRKEY),
	DEFINE_RES_IRQ(MT6358_IRQ_HOMEKEY),
	DEFINE_RES_IRQ(MT6358_IRQ_PWRKEY_R),
	DEFINE_RES_IRQ(MT6358_IRQ_HOMEKEY_R),
};


static const struct resource mt6359p_keys_resources[] = {
	DEFINE_RES_IRQ(MT6359P_IRQ_PWRKEY),
	DEFINE_RES_IRQ(MT6359P_IRQ_HOMEKEY),
	DEFINE_RES_IRQ(MT6359P_IRQ_PWRKEY_R),
	DEFINE_RES_IRQ(MT6359P_IRQ_HOMEKEY_R),
};

static const struct resource mt6397_keys_resources[] = {
	DEFINE_RES_IRQ(MT6397_IRQ_PWRKEY),
	DEFINE_RES_IRQ(MT6397_IRQ_HOMEKEY),
};

static const struct resource mt6357_auxadc_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_AUXADC_IMP, "imp"),
};

static const struct resource mt6366_keys_resources[] = {
	DEFINE_RES_IRQ(MT6366_IRQ_PWRKEY),
	DEFINE_RES_IRQ(MT6366_IRQ_HOMEKEY),
	DEFINE_RES_IRQ(MT6366_IRQ_PWRKEY_R),
	DEFINE_RES_IRQ(MT6366_IRQ_HOMEKEY_R),
};

static const struct resource mt6359p_auxadc_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_AUXADC_IMP, "imp"),
};

static const struct resource mt6357_lbat_service_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_BAT_H, "bat_h"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_BAT_L, "bat_l"),
};

static const struct resource mt6357_leds_resources[] = {
};

static const struct resource mt6359p_battery_oc_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_FG_CUR_H, "fg_cur_h"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_FG_CUR_L, "fg_cur_l"),
};

static const struct resource mt6366_battery_oc_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_FG_CUR_H, "fg_cur_h"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_FG_CUR_L, "fg_cur_l"),
};

static const struct resource mt6359p_lbat_service_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_BAT_H, "bat_h"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_BAT_L, "bat_l"),
};

static const struct resource mt6366_lbat_service_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_BAT_H, "bat_h"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_BAT_L, "bat_l"),
};

static const struct resource mt6358_lbat_service_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_BAT_H, "bat_h"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_BAT_L, "bat_l"),
};

static const struct resource mt6358_battery_oc_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_FG_CUR_H, "fg_cur_h"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_FG_CUR_L, "fg_cur_l"),
};


static const struct resource mt6323_pwrc_resources[] = {
	DEFINE_RES_MEM(MT6323_PWRC_BASE, MT6323_PWRC_SIZE),
};

static const struct resource mt6357_accdet_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_ACCDET, "ACCDET_IRQ"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_ACCDET_EINT0, "ACCDET_EINT0"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_ACCDET_EINT1, "ACCDET_EINT1"),
};

static const struct resource mt6358_accdet_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_ACCDET, "ACCDET_IRQ"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_ACCDET_EINT0, "ACCDET_EINT0"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_ACCDET_EINT1, "ACCDET_EINT1"),
};

static const struct resource mt6357_regulators_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VPROC_OC, "VPROC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCORE_OC, "VCORE"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VMODEM_OC, "VMODEM"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VS1_OC, "VS1"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VPA_OC, "VPA"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCORE_PREOC, "VCORE_PR"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VFE28_OC, "VFE28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VXO22_OC, "VXO22"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VRF18_OC, "VRF18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VRF12_OC, "VRF12"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VEFUSE_OC, "VEFUSE"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCN33_OC, "VCN33"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCN28_OC, "VCN28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCN18_OC, "VCN18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCAMA_OC, "VCAMA"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCAMD_OC, "VCAMD"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VCAMIO_OC, "VCAMIO"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VLDO28_OC, "VLDO28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VUSB33_OC, "VUSB33"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VAUX18_OC, "VAUX18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VAUD28_OC, "VAUD28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VIO28_OC, "VIO28"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VIO18_OC, "VIO18"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSRAM_PROC_OC, "VSRAM_PROC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSRAM_OTHERS_OC, "VSRAM_OTHERS"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VIBR_OC, "VIBR"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VDRAM_OC, "VDRAM"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VMC_OC, "VMC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VMCH_OC, "VMCH"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VEMC_OC, "VEMC"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSIM1_OC, "VSIM1"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_VSIM2_OC, "VSIM2"),
};

static const struct resource mt6357_gauge_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_FG_BAT0_H, "COULOMB_H"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_FG_BAT0_L, "COULOMB_L"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_NAG_C_DLTV, "NAFG"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_FG_ZCV, "ZCV"),
};

static const struct resource mt6358_gauge_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_FG_BAT1_H, "COULOMB_H"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_FG_BAT1_L, "COULOMB_L"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_BAT2_H, "VBAT_H"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_BAT2_L, "VBAT_L"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_NAG_C_DLTV, "NAFG"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_BATON_BAT_OUT, "BAT_OUT"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_FG_ZCV, "ZCV"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_FG_N_CHARGE_L, "FG_N_CHARGE_L"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_FG_IAVG_H, "FG_IAVG_H"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_FG_IAVG_L, "FG_IAVG_L"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_BAT_TEMP_H, "BAT_TMP_H"),
	DEFINE_RES_IRQ_NAMED(MT6358_IRQ_BAT_TEMP_L, "BAT_TMP_L"),
};

static const struct resource mt6366_gauge_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_FG_BAT1_H, "COULOMB_H"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_FG_BAT1_L, "COULOMB_L"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_BAT2_H, "VBAT_H"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_BAT2_L, "VBAT_L"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_NAG_C_DLTV, "NAFG"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_BATON_BAT_OUT, "BAT_OUT"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_FG_ZCV, "ZCV"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_FG_N_CHARGE_L, "FG_N_CHARGE_L"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_FG_IAVG_H, "FG_IAVG_H"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_FG_IAVG_L, "FG_IAVG_L"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_BAT_TEMP_H, "BAT_TMP_H"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_BAT_TEMP_L, "BAT_TMP_L"),
};

static const struct resource mt6359p_gauge_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_FG_BAT_H, "COULOMB_H"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_FG_BAT_L, "COULOMB_L"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_BAT2_H, "VBAT_H"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_BAT2_L, "VBAT_L"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_NAG_C_DLTV, "NAFG"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_BATON_BAT_OU, "BAT_OUT"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_FG_ZCV, "ZCV"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_FG_N_CHARGE_L, "FG_N_CHARGE_L"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_FG_IAVG_H, "FG_IAVG_H"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_FG_IAVG_L, "FG_IAVG_L"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_BAT_TEMP_H, "BAT_TMP_H"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_BAT_TEMP_L, "BAT_TMP_L"),
};

static const struct resource mt6359p_accdet_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_ACCDET, "ACCDET_IRQ"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_ACCDET_EINT0, "ACCDET_EINT0"),
	DEFINE_RES_IRQ_NAMED(MT6359P_IRQ_ACCDET_EINT1, "ACCDET_EINT1"),
};

static const struct resource mt6357_battery_oc_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_FG_CUR_H, "fg_cur_h"),
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_FG_CUR_L, "fg_cur_l"),
};

static const struct resource mt6357_chrdet_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6357_IRQ_CHRDET_EDGE, "chrdet"),
};

static const struct resource mt6366_accdet_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_ACCDET, "ACCDET_IRQ"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_ACCDET_EINT0, "ACCDET_EINT0"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_ACCDET_EINT1, "ACCDET_EINT1"),
};

static const struct resource mt6366_regulators_resources[] = {
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VPROC11_OC, "VPROC11"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VPROC12_OC, "VPROC12"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VCORE_OC, "VCORE"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VGPU_OC, "VGPU"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VMODEM_OC, "VMODEM"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VDRAM1_OC, "VDRAM1"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VS1_OC, "VS1"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VS2_OC, "VS2"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VPA_OC, "VPA"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VCORE_PREOC, "VCORE_PR"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VFE28_OC, "VFE28"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VXO22_OC, "VXO22"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VRF18_OC, "VRF18"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VRF12_OC, "VRF12"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VEFUSE_OC, "VEFUSE"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VCN33_OC, "VCN33"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VCN28_OC, "VCN28"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VCN18_OC, "VCN18"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VM18_OC, "VM18"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VMDDR_OC, "VMDDR"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VSRAM_CORE_OC, "VSRAM_CORE"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VA12_OC, "VA12"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VAUX18_OC, "VAUX18"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VAUD28_OC, "VAUD28"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VIO28_OC, "VIO28"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VIO18_OC, "VIO18"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VSRAM_PROC11_OC, "VSRAM_PROC11"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VSRAM_PROC12_OC, "VSRAM_PROC12"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VSRAM_OTHERS_OC, "VSRAM_OTHERS"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VSRAM_GPU_OC, "VSRAM_GPU"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VDRAM2_OC, "VDRAM2"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VMC_OC, "VMC"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VMCH_OC, "VMCH"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VEMC_OC, "VEMC"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VSIM1_OC, "VSIM1"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VSIM2_OC, "VSIM2"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VIBR_OC, "VIBR"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VUSB_OC, "VUSB"),
	DEFINE_RES_IRQ_NAMED(MT6366_IRQ_VBIF28_OC, "VBIF28"),
};

static const struct mfd_cell mt6323_devs[] = {
	{
		.name = "mt6323-rtc",
		.num_resources = ARRAY_SIZE(mt6323_rtc_resources),
		.resources = mt6323_rtc_resources,
		.of_compatible = "mediatek,mt6323-rtc",
	}, {
		.name = "mt6323-regulator",
		.of_compatible = "mediatek,mt6323-regulator"
	}, {
		.name = "mt6323-led",
		.of_compatible = "mediatek,mt6323-led"
	}, {
		.name = "mtk-pmic-keys",
		.num_resources = ARRAY_SIZE(mt6323_keys_resources),
		.resources = mt6323_keys_resources,
		.of_compatible = "mediatek,mt6323-keys"
	}, {
		.name = "mt6323-pwrc",
		.num_resources = ARRAY_SIZE(mt6323_pwrc_resources),
		.resources = mt6323_pwrc_resources,
		.of_compatible = "mediatek,mt6323-pwrc"
	},
};

static const struct mfd_cell mt6357_devs[] = {
	{
		.name = "mt635x-accdet",
		.of_compatible = "mediatek,mt6357-accdet",
		.num_resources = ARRAY_SIZE(mt6357_accdet_resources),
		.resources = mt6357_accdet_resources,
	}, {
		.name = "mt635x-auxadc",
		.of_compatible = "mediatek,mt6357-auxadc",
		.num_resources = ARRAY_SIZE(mt6357_auxadc_resources),
		.resources = mt6357_auxadc_resources,
	}, {
		.name = "mt6357-efuse",
		.of_compatible = "mediatek,mt6357-efuse",
	}, {
		.name = "mt6357-lbat_service",
		.of_compatible = "mediatek,mt6357-lbat_service",
		.num_resources = ARRAY_SIZE(mt6357_lbat_service_resources),
		.resources = mt6357_lbat_service_resources,
	}, {
		.name = "mt6357-regulator",
		.of_compatible = "mediatek,mt6357-regulator",
		.num_resources = ARRAY_SIZE(mt6357_regulators_resources),
		.resources = mt6357_regulators_resources,
	}, {
		.name = "mtk-battery-oc-throttling",
		.of_compatible = "mediatek,mt6357-battery_oc_throttling",
		.num_resources = ARRAY_SIZE(mt6357_battery_oc_resources),
		.resources = mt6357_battery_oc_resources,
	}, {
		.name = "mtk-dynamic-loading-throttling",
		.of_compatible = "mediatek,mt6357-dynamic_loading_throttling",
	}, {
		.name = "mt6357-charger-type-detection",
		.num_resources = ARRAY_SIZE(mt6357_chrdet_resources),
		.resources = mt6357_chrdet_resources,
		.of_compatible = "mediatek,mt6357-charger-type"
	}, {
		.name = "mtk_ts_pmic",
		.of_compatible = "mediatek,mtk_ts_pmic"
	}, {
		.name = "mt6357_ts_buck1",
		.of_compatible = "mediatek,mt6357_ts_buck1"
	}, {
		.name = "mt6357_ts_buck2",
		.of_compatible = "mediatek,mt6357_ts_buck2"
	}, {
		.name = "leds-mt6357",
		.of_compatible = "mediatek,mt6357_leds",
		.num_resources = ARRAY_SIZE(mt6357_leds_resources),
		.resources = mt6357_leds_resources
	}, {
		.name = "mtk-pmic-keys",
		.num_resources = ARRAY_SIZE(mt6357_keys_resources),
		.resources = mt6357_keys_resources,
		.of_compatible = "mediatek,mt6357-keys"
	}, {
		.name = "mt6357-gauge",
		.num_resources = ARRAY_SIZE(mt6357_gauge_resources),
		.resources = mt6357_gauge_resources,
		.of_compatible = "mediatek,mt6357-gauge",
	}, {
		.name = "mt-pmic",
		.of_compatible = "mediatek,mt63xx-debug",
	}, {
		.name = "mt63xx-oc-debug",
		.of_compatible = "mediatek,mt63xx-oc-debug",
	}, {
		.name = "mt6397-rtc",
		.num_resources = ARRAY_SIZE(mt6357_rtc_resources),
		.resources = mt6357_rtc_resources,
		.of_compatible = "mediatek,mt6357-rtc",
	}, {
		.name = "mt6357-sound",
		.of_compatible = "mediatek,mt6357-sound"
	}, {
		.name = "mtk-clock-buffer",
		.of_compatible = "mediatek,clock_buffer",
	}, {
		.name = "mt6357-pulse-charger",
		.of_compatible = "mediatek,mt6357-pulse-charger"
	}, {
		.name = "mt6357-consys",
		.of_compatible = "mediatek,mt6357-consys"
	},
};

static const struct mfd_cell mt6358_devs[] = {
	{
		.name = "mtk_ts_pmic",
		.of_compatible = "mediatek,mtk_ts_pmic"
	}, {
		.name = "mt6358_ts_buck1",
		.of_compatible = "mediatek,mt6358_ts_buck1"
	}, {
		.name = "mt6358_ts_buck2",
		.of_compatible = "mediatek,mt6358_ts_buck2"
	},  {
		.name = "mt6358_ts_buck3",
		.of_compatible = "mediatek,mt6358_ts_buck3"
	}, {
		.name = "mt6358-regulator",
		.of_compatible = "mediatek,mt6358-regulator"
	}, {
		.name = "mt635x-auxadc",
		.of_compatible = "mediatek,mt6358-auxadc",
	}, {
		.name = "mt6358-rtc",
		.num_resources = ARRAY_SIZE(mt6358_rtc_resources),
		.resources = mt6358_rtc_resources,
		.of_compatible = "mediatek,mt6358-rtc",
	}, {
		.name = "mediatek,pmic-accdet",
		.of_compatible = "mediatek,mt6358-accdet",
		.num_resources = ARRAY_SIZE(mt6358_accdet_resources),
		.resources = mt6358_accdet_resources,
	}, {
		.name = "mt6358-sound",
		.of_compatible = "mediatek,mt6358-sound"
	}, {
		.name = "mt6358-gauge",
		.num_resources = ARRAY_SIZE(mt6358_gauge_resources),
		.resources = mt6358_gauge_resources,
		.of_compatible = "mediatek,mt6358-gauge",
	}, {
		.name = "mt6358-efuse",
		.of_compatible = "mediatek,mt6358-efuse",
	}, {
		.name = "mt6358-consys",
		.of_compatible = "mediatek,mt6358-consys"
	}, {
		.name = "mtk-battery-oc-throttling",
		.of_compatible = "mediatek,mt6358-battery_oc_throttling",
		.num_resources = ARRAY_SIZE(mt6358_battery_oc_resources),
		.resources = mt6358_battery_oc_resources,
	}, {
		.name = "mtk-dynamic-loading-throttling",
		.of_compatible = "mediatek,mt6358-dynamic_loading_throttling",
	}, {
		.name = "mtk-lbat_service",
		.of_compatible = "mediatek,mt6358-lbat_service",
		.num_resources = ARRAY_SIZE(mt6358_lbat_service_resources),
		.resources = mt6358_lbat_service_resources,
	}, {
		.name = "mtk-pmic-keys",
		.num_resources = ARRAY_SIZE(mt6358_keys_resources),
		.resources = mt6358_keys_resources,
		.of_compatible = "mediatek,mt6358-keys",
	}, {
		.name = "mt-pmic",
		.of_compatible = "mediatek,mt63xx-debug",
	},
};

static const struct mfd_cell mt6359p_devs[] = {
	{
		.name = "mt-pmic",
		.of_compatible = "mediatek,mt63xx-debug",
	}, {
		.name = "mt6359p-accdet",
		.of_compatible = "mediatek,mt6359p-accdet",
		.num_resources = ARRAY_SIZE(mt6359p_accdet_resources),
		.resources = mt6359p_accdet_resources,
	}, {
		.name = "mt635x-auxadc",
		.of_compatible = "mediatek,mt6359p-auxadc",
		.num_resources = ARRAY_SIZE(mt6359p_auxadc_resources),
		.resources = mt6359p_auxadc_resources,
	}, {
		.name = "mt6359p-efuse",
		.of_compatible = "mediatek,mt6359p-efuse",
	}, {
		.name = "mt6359p-regulator",
		.of_compatible = "mediatek,mt6359p-regulator"
	}, {
		.name = "mt6359p-rtc",
		.num_resources = ARRAY_SIZE(mt6359p_rtc_resources),
		.resources = mt6359p_rtc_resources,
		.of_compatible = "mediatek,mt6359p-rtc",
	}, {
		.name = "mt6359p-gauge",
		.num_resources = ARRAY_SIZE(mt6359p_gauge_resources),
		.resources = mt6359p_gauge_resources,
		.of_compatible = "mediatek,mt6359p-gauge",
	}, {
		.name = "mtk-battery-oc-throttling",
		.of_compatible = "mediatek,mt6359p-battery_oc_throttling",
		.num_resources = ARRAY_SIZE(mt6359p_battery_oc_resources),
		.resources = mt6359p_battery_oc_resources,
	}, {
		.name = "mtk-dynamic-loading-throttling",
		.of_compatible = "mediatek,mt6359p-dynamic_loading_throttling",
	}, {
		.name = "mtk-lbat_service",
		.of_compatible = "mediatek,mt6359p-lbat_service",
		.num_resources = ARRAY_SIZE(mt6359p_lbat_service_resources),
		.resources = mt6359p_lbat_service_resources,
	}, {
		.name = "mtk-pmic-keys",
		.num_resources = ARRAY_SIZE(mt6359p_keys_resources),
		.resources = mt6359p_keys_resources,
		.of_compatible = "mediatek,mt6359p-keys"
	}, {
		.name = "mt6359-sound",
		.of_compatible = "mediatek,mt6359p-sound"
	}, {
		.name = "mtk-clock-buffer",
		.of_compatible = "mediatek,clock_buffer",
	}
};

static const struct mfd_cell mt6366_devs[] = {
	{
		.name = "mt-pmic",
		.of_compatible = "mediatek,mt63xx-debug",
	}, {
		.name = "mediatek,pmic-accdet",
		.of_compatible = "mediatek,mt6358-accdet",
		.num_resources = ARRAY_SIZE(mt6366_accdet_resources),
		.resources = mt6366_accdet_resources,
	}, {
		.name = "mt635x-auxadc",
		.of_compatible = "mediatek,mt6358-auxadc",
	}, {
		.name = "mt6358-efuse",
		.of_compatible = "mediatek,mt6358-efuse",
	}, {
		.name = "mt6358-regulator",
		.of_compatible = "mediatek,mt6358-regulator",
		.num_resources = ARRAY_SIZE(mt6366_regulators_resources),
		.resources = mt6366_regulators_resources,
	}, {
		.name = "mtk-battery-oc-throttling",
		.of_compatible = "mediatek,mt6358-battery_oc_throttling",
		.num_resources = ARRAY_SIZE(mt6366_battery_oc_resources),
		.resources = mt6366_battery_oc_resources,
	}, {
		.name = "mtk-dynamic-loading-throttling",
		.of_compatible = "mediatek,mt6358-dynamic_loading_throttling",
	}, {
		.name = "mtk-lbat_service",
		.of_compatible = "mediatek,mt6358-lbat_service",
		.num_resources = ARRAY_SIZE(mt6366_lbat_service_resources),
		.resources = mt6366_lbat_service_resources,
	}, {
		.name = "mt63xx-oc-debug",
		.of_compatible = "mediatek,mt63xx-oc-debug",
	}, {
		.name = "mt6358-sound",
		.of_compatible = "mediatek,mt6366-sound"
	}, {
		.name = "mtk-pmic-keys",
		.num_resources = ARRAY_SIZE(mt6366_keys_resources),
		.resources = mt6366_keys_resources,
		.of_compatible = "mediatek,mt6366-keys"
	}, {
		.name = "mt6358-gauge",
		.num_resources = ARRAY_SIZE(mt6366_gauge_resources),
		.resources = mt6366_gauge_resources,
		.of_compatible = "mediatek,mt6358-gauge",
	}, {
		.name = "mtk-clock-buffer",
		.of_compatible = "mediatek,clock_buffer",
	}, {
		.name = "mt6358-rtc",
		.num_resources = ARRAY_SIZE(mt6358_rtc_resources),
		.resources = mt6358_rtc_resources,
		.of_compatible = "mediatek,mt6358-rtc",
	},
};

static const struct mfd_cell mt6397_devs[] = {
	{
		.name = "mt6397-rtc",
		.num_resources = ARRAY_SIZE(mt6397_rtc_resources),
		.resources = mt6397_rtc_resources,
		.of_compatible = "mediatek,mt6397-rtc",
	}, {
		.name = "mt6397-regulator",
		.of_compatible = "mediatek,mt6397-regulator",
	}, {
		.name = "mt6397-codec",
		.of_compatible = "mediatek,mt6397-codec",
	}, {
		.name = "mt6397-clk",
		.of_compatible = "mediatek,mt6397-clk",
	}, {
		.name = "mt6397-pinctrl",
		.of_compatible = "mediatek,mt6397-pinctrl",
	}, {
		.name = "mtk-pmic-keys",
		.num_resources = ARRAY_SIZE(mt6397_keys_resources),
		.resources = mt6397_keys_resources,
		.of_compatible = "mediatek,mt6397-keys"
	}
};

struct chip_data {
	u32 cid_addr;
	u32 cid_shift;
	const struct mfd_cell *cells;
	int cell_size;
	int (*irq_init)(struct mt6397_chip *chip);
};

static const struct chip_data mt6323_core = {
	.cid_addr = MT6323_CID,
	.cid_shift = 0,
	.cells = mt6323_devs,
	.cell_size = ARRAY_SIZE(mt6323_devs),
	.irq_init = mt6397_irq_init,
};

static const struct chip_data mt6357_core = {
	.cid_addr = MT6357_SWCID,
	.cid_shift = 8,
	.cells = mt6357_devs,
	.cell_size = ARRAY_SIZE(mt6357_devs),
	.irq_init = mt6358_irq_init,
};

static const struct chip_data mt6358_core = {
	.cid_addr = MT6358_SWCID,
	.cid_shift = 8,
	.cells = mt6358_devs,
	.cell_size = ARRAY_SIZE(mt6358_devs),
	.irq_init = mt6358_irq_init,
};

static const struct chip_data mt6359p_core = {
	.cid_addr = MT6359P_SWCID,
	.cid_shift = 8,
	.cells = mt6359p_devs,
	.cell_size = ARRAY_SIZE(mt6359p_devs),
	.irq_init = mt6358_irq_init,
};

static const struct chip_data mt6366_core = {
	.cid_addr = MT6358_SWCID,
	.cid_shift = 8,
	.cells = mt6366_devs,
	.cell_size = ARRAY_SIZE(mt6366_devs),
	.irq_init = mt6358_irq_init,
};

static const struct chip_data mt6397_core = {
	.cid_addr = MT6397_CID,
	.cid_shift = 0,
	.cells = mt6397_devs,
	.cell_size = ARRAY_SIZE(mt6397_devs),
	.irq_init = mt6397_irq_init,
};

static int mt6397_probe(struct platform_device *pdev)
{
	int ret;
	unsigned int id = 0;
	struct mt6397_chip *pmic;
	const struct chip_data *pmic_core;

	pmic = devm_kzalloc(&pdev->dev, sizeof(*pmic), GFP_KERNEL);
	if (!pmic)
		return -ENOMEM;

	pmic->dev = &pdev->dev;

	/*
	 * mt6397 MFD is child device of soc pmic wrapper.
	 * Regmap is set from its parent.
	 */
	pmic->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!pmic->regmap)
		return -ENODEV;

	pmic_core = of_device_get_match_data(&pdev->dev);
	if (!pmic_core)
		return -ENODEV;

	ret = regmap_read(pmic->regmap, pmic_core->cid_addr, &id);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read chip id: %d\n", ret);
		return ret;
	}

	pmic->chip_id = (id >> pmic_core->cid_shift) & 0xff;

	platform_set_drvdata(pdev, pmic);

	pmic->irq = platform_get_irq(pdev, 0);
	if (pmic->irq <= 0)
		return pmic->irq;

	ret = pmic_core->irq_init(pmic);
	if (ret)
		return ret;

	ret = devm_mfd_add_devices(&pdev->dev, PLATFORM_DEVID_NONE,
				   pmic_core->cells, pmic_core->cell_size,
				   NULL, 0, pmic->irq_domain);
	if (ret) {
		irq_domain_remove(pmic->irq_domain);
		dev_err(&pdev->dev, "failed to add child devices: %d\n", ret);
	}
	pr_info("mt6397 probe success!n");

	return ret;
}

static const struct of_device_id mt6397_of_match[] = {
	{
		.compatible = "mediatek,mt6323",
		.data = &mt6323_core,
	}, {
		.compatible = "mediatek,mt6357",
		.data = &mt6357_core,
	}, {
		.compatible = "mediatek,mt6358",
		.data = &mt6358_core,
	}, {
		.compatible = "mediatek,mt6359p",
		.data = &mt6359p_core,
	}, {
		.compatible = "mediatek,mt6366",
		.data = &mt6366_core,
	}, {
		.compatible = "mediatek,mt6397",
		.data = &mt6397_core,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, mt6397_of_match);

static const struct platform_device_id mt6397_id[] = {
	{ "mt6397", 0 },
	{ },
};
MODULE_DEVICE_TABLE(platform, mt6397_id);

static struct platform_driver mt6397_driver = {
	.probe = mt6397_probe,
	.driver = {
		.name = "mt6397",
		.of_match_table = of_match_ptr(mt6397_of_match),
	},
	.id_table = mt6397_id,
};



#if IS_BUILTIN(CONFIG_MFD_MT6397)
static int __init mt6397_driver_init(void)
{
	return platform_driver_register(&mt6397_driver);
}
arch_initcall(mt6397_driver_init);
#else
module_platform_driver(mt6397_driver);
#endif

MODULE_AUTHOR("Flora Fu, MediaTek");
MODULE_DESCRIPTION("Driver for MediaTek MT6397 PMIC");
MODULE_LICENSE("GPL");
