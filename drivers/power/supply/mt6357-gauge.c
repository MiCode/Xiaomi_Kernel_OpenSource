// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include <linux/device.h>
#include <linux/iio/consumer.h>
#include <linux/interrupt.h>
#include <linux/mfd/mt6397/core.h>/* PMIC MFD core header */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include "mtk_battery.h"
#include "mtk_gauge.h"

/* ============================================================ */
/* pmic control start*/
/* ============================================================ */
#define MT6357_FGADC_CON1   0xd0a

#define PMIC_HWCID_ADDR		0x8
#define PMIC_HWCID_MASK		0xFFFF
#define PMIC_HWCID_SHIFT	0

#define PMIC_AUXADC_NAG_PRD_ADDR	0x1218
#define PMIC_AUXADC_NAG_PRD_MASK	0x7f
#define PMIC_AUXADC_NAG_PRD_SHIFT	3

#define PMIC_FG_LATCHDATA_ST_ADDR	0xd0a
#define PMIC_FG_LATCHDATA_ST_MASK	0x1
#define PMIC_FG_LATCHDATA_ST_SHIFT	15

#define PMIC_FG_SW_CLEAR_ADDR	0xd0a
#define PMIC_FG_SW_CLEAR_MASK	0x1
#define PMIC_FG_SW_CLEAR_SHIFT	3

#define PMIC_FG_SW_READ_PRE_ADDR	0xd0a
#define PMIC_FG_SW_READ_PRE_MASK	0x1
#define PMIC_FG_SW_READ_PRE_SHIFT	0

#define PMIC_FG_CURRENT_OUT_ADDR	0xd8a
#define PMIC_FG_CURRENT_OUT_MASK	0xFFFF
#define PMIC_FG_CURRENT_OUT_SHIFT	0

#define PMIC_FG_R_CURR_ADDR	0xd88
#define PMIC_FG_R_CURR_MASK	0xFFFF
#define PMIC_FG_R_CURR_SHIFT	0

#define PMIC_FG_CAR_15_00_ADDR	0xd12
#define PMIC_FG_CAR_15_00_MASK	0xFFFF
#define PMIC_FG_CAR_15_00_SHIFT	0

#define PMIC_FG_CAR_31_16_ADDR	0xd14
#define PMIC_FG_CAR_31_16_MASK	0xFFFF
#define PMIC_FG_CAR_31_16_SHIFT	0

#define PMIC_FG_BAT0_HTH_15_00_ADDR	0xd1c
#define PMIC_FG_BAT0_HTH_15_00_MASK	0xFFFF
#define PMIC_FG_BAT0_HTH_15_00_SHIFT	0

#define PMIC_FG_BAT0_HTH_31_16_ADDR	0xd1e
#define PMIC_FG_BAT0_HTH_31_16_MASK	0xFFFF
#define PMIC_FG_BAT0_HTH_31_16_SHIFT	0

#define PMIC_FG_BAT0_LTH_15_00_ADDR	0xd18
#define PMIC_FG_BAT0_LTH_15_00_MASK	0xFFFF
#define PMIC_FG_BAT0_LTH_15_00_SHIFT	0

#define PMIC_FG_BAT0_LTH_31_16_ADDR	0xd1a
#define PMIC_FG_BAT0_LTH_31_16_MASK	0xFFFF
#define PMIC_FG_BAT0_LTH_31_16_SHIFT	0

#define PMIC_RGS_BATON_UNDET_ADDR	0xe08
#define PMIC_RGS_BATON_UNDET_MASK	0x1
#define PMIC_RGS_BATON_UNDET_SHIFT	1

#define PMIC_AUXADC_ADC_RDY_PWRON_CLR_ADDR	0x114a
#define PMIC_AUXADC_ADC_RDY_PWRON_CLR_MASK	0x1
#define PMIC_AUXADC_ADC_RDY_PWRON_CLR_SHIFT	3

#define PMIC_AUXADC_NAG_CNT_15_0_ADDR			0x1220
#define PMIC_AUXADC_NAG_CNT_15_0_MASK			0xFFFF
#define PMIC_AUXADC_NAG_CNT_15_0_SHIFT			0

#define PMIC_AUXADC_NAG_CNT_25_16_ADDR			0x1222
#define PMIC_AUXADC_NAG_CNT_25_16_MASK			0x3FF
#define PMIC_AUXADC_NAG_CNT_25_16_SHIFT			0

#define PMIC_AUXADC_NAG_DLTV_ADDR				0x1224
#define PMIC_AUXADC_NAG_DLTV_MASK				0xFFFF
#define PMIC_AUXADC_NAG_DLTV_SHIFT				0

#define PMIC_AUXADC_NAG_C_DLTV_15_0_ADDR        0x1226
#define PMIC_AUXADC_NAG_C_DLTV_15_0_MASK		0xFFFF
#define PMIC_AUXADC_NAG_C_DLTV_15_0_SHIFT		0

#define PMIC_AUXADC_NAG_C_DLTV_26_16_ADDR		0x1228
#define PMIC_AUXADC_NAG_C_DLTV_26_16_MASK		0x7FF
#define PMIC_AUXADC_NAG_C_DLTV_26_16_SHIFT		0

#define PMIC_AUXADC_ADC_OUT_FGADC_PCHR_ADDR		0x10bc
#define PMIC_AUXADC_ADC_OUT_FGADC_PCHR_MASK		0x7FFF
#define PMIC_AUXADC_ADC_OUT_FGADC_PCHR_SHIFT	0

#define PMIC_AUXADC_NAG_IRQ_EN_ADDR				0x1218
#define PMIC_AUXADC_NAG_IRQ_EN_MASK				0x1
#define PMIC_AUXADC_NAG_IRQ_EN_SHIFT			10

#define PMIC_AUXADC_NAG_EN_ADDR					0x1218
#define PMIC_AUXADC_NAG_EN_MASK					0x1
#define PMIC_AUXADC_NAG_EN_SHIFT				0

#define PMIC_AUXADC_NAG_ZCV_ADDR				0x121a
#define PMIC_AUXADC_NAG_ZCV_MASK				0x7FFF
#define PMIC_AUXADC_NAG_ZCV_SHIFT				0

#define PMIC_AUXADC_NAG_C_DLTV_TH_15_0_ADDR		0x121c
#define PMIC_AUXADC_NAG_C_DLTV_TH_15_0_MASK		0xFFFF
#define PMIC_AUXADC_NAG_C_DLTV_TH_15_0_SHIFT	0

#define PMIC_AUXADC_NAG_C_DLTV_TH_26_16_ADDR	0x121e
#define PMIC_AUXADC_NAG_C_DLTV_TH_26_16_MASK	0x7FF
#define PMIC_AUXADC_NAG_C_DLTV_TH_26_16_SHIFT	0

#define PMIC_AUXADC_NAG_VBAT1_SEL_ADDR			0x1218
#define PMIC_AUXADC_NAG_VBAT1_SEL_MASK			0x1
#define PMIC_AUXADC_NAG_VBAT1_SEL_SHIFT			2

#define PMIC_FG_ZCV_CAR_TH_15_00_ADDR			0xd38
#define PMIC_FG_ZCV_CAR_TH_15_00_MASK			0xFFFF
#define PMIC_FG_ZCV_CAR_TH_15_00_SHIFT			0

#define PMIC_FG_ZCV_CAR_TH_31_16_ADDR			0xd3a
#define PMIC_FG_ZCV_CAR_TH_31_16_MASK			0xFFFF
#define PMIC_FG_ZCV_CAR_TH_31_16_SHIFT			0

#define PMIC_FG_ZCV_CAR_TH_33_32_ADDR			0xd3c
#define PMIC_FG_ZCV_CAR_TH_33_32_MASK			0x3
#define PMIC_FG_ZCV_CAR_TH_33_32_SHIFT			0

#define PMIC_FG_ZCV_DET_EN_ADDR					0xd08
#define PMIC_FG_ZCV_DET_EN_MASK					0x1
#define PMIC_FG_ZCV_DET_EN_SHIFT				10

#define PMIC_AUXADC_ADC_OUT_NAG_ADDR			0x10d4
#define PMIC_AUXADC_ADC_OUT_NAG_MASK			0x7FFF
#define PMIC_AUXADC_ADC_OUT_NAG_SHIFT			0

#define PMIC_AUXADC_ADC_RDY_PWRON_PCHR_ADDR		0x10ac
#define PMIC_AUXADC_ADC_RDY_PWRON_PCHR_MASK		0x1
#define PMIC_AUXADC_ADC_RDY_PWRON_PCHR_SHIFT	15

#define PMIC_AUXADC_ADC_OUT_PWRON_PCHR_ADDR		0x10ac
#define PMIC_AUXADC_ADC_OUT_PWRON_PCHR_MASK		0x7FFF
#define PMIC_AUXADC_ADC_OUT_PWRON_PCHR_SHIFT	0

#define PMIC_RG_STRUP_AUXADC_START_SEL_ADDR	0xa20
#define PMIC_RG_STRUP_AUXADC_START_SEL_MASK	0x1
#define PMIC_RG_STRUP_AUXADC_START_SEL_SHIFT	2

#define PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_PCHR_ADDR	0x10c0
#define PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_PCHR_MASK	0x1
#define PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_PCHR_SHIFT	15

#define PMIC_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_ADDR	0x10c0
#define PMIC_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_MASK	0x7FFF
#define PMIC_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_SHIFT	0

#define PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR_ADDR		0x114a
#define PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR_MASK		0x1
#define PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR_SHIFT	2

#define PMIC_FG_NTER_15_00_ADDR						0xd20
#define PMIC_FG_NTER_15_00_MASK						0xFFFF
#define PMIC_FG_NTER_15_00_SHIFT					0

#define PMIC_FG_NTER_31_16_ADDR						0xd22
#define PMIC_FG_NTER_31_16_MASK						0xFFFF
#define PMIC_FG_NTER_31_16_SHIFT					0

#define PMIC_FG_NTER_32_ADDR						0xd24
#define PMIC_FG_NTER_32_MASK						0x1
#define PMIC_FG_NTER_32_SHIFT					0

#define PMIC_FG_CIC2_ADDR					0xd90
#define PMIC_FG_CIC2_MASK					0xFFFF
#define PMIC_FG_CIC2_SHIFT					0

#define PMIC_FG_ZCV_CURR_ADDR						0xd30
#define PMIC_FG_ZCV_CURR_MASK						0xFFFF
#define PMIC_FG_ZCV_CURR_SHIFT						0
//

#define PMIC_RG_SYSTEM_INFO_CON0_ADDR 0xd9a

#define UNIT_FGCURRENT			(314331)
/* mt6357 314.331 uA */
#define UNIT_CHARGE				(85)
/* CHARGE_LSB 0.085 uAh*/

/* AUXADC */
#define R_VAL_TEMP_2			(1)
#define R_VAL_TEMP_3			(3)

#define UNIT_TIME				(50)
#define UNIT_FG_IAVG			(157166)
/* 3600 * 1000 * 1000 / 157166 , for coulomb interrupt */
#define DEFAULT_R_FG			(100)
/* 5mm ohm */
#define UNIT_FGCAR_ZCV			(85)
/* CHARGE_LSB = 0.085 uAh */

#define VOLTAGE_FULL_RANGES		1800
#define ADC_PRECISE				32768	/* 12 bits */

#define CAR_TO_REG_SHIFT		(3)
/*coulomb interrupt lsb might be different with coulomb lsb */
#define CAR_TO_REG_FACTOR		(0x5c2a)
/* 1000 * 1000 / CHARGE_LSB */
#define UNIT_FGCAR				(11176)
/* CHARGE_LSB 0.085 * 2^11 */

static signed int reg_to_mv_value(signed int _reg)
{
	long long _reg64 = _reg;
	int ret;

#if defined(__LP64__) || defined(_LP64)
	_reg64 = (_reg64 * VOLTAGE_FULL_RANGES
		* R_VAL_TEMP_3) / ADC_PRECISE;
#else
	_reg64 = div_s64(_reg64 * VOLTAGE_FULL_RANGES
		* R_VAL_TEMP_3, ADC_PRECISE);
#endif
	ret = _reg64;
	bm_debug("[%s] %lld => %d\n",
		__func__, _reg64, ret);
	return ret;
}

static signed int mv_to_reg_value(signed int _mv)
{
	int ret;
	long long _reg64 = _mv;
#if defined(__LP64__) || defined(_LP64)
	_reg64 = (_reg64 * ADC_PRECISE) / (VOLTAGE_FULL_RANGES
		* R_VAL_TEMP_3);
#else
	_reg64 = div_s64((_reg64 * ADC_PRECISE), (VOLTAGE_FULL_RANGES
		* R_VAL_TEMP_3));
#endif
	ret = _reg64;

	if (ret <= 0) {
		bm_err(
			"[fg_bat_nafg][%s] mv=%d,%lld => %d,\n",
			__func__, _mv, _reg64, ret);
		return ret;
	}

	bm_debug("[%s] mv=%d,%lld => %d,\n", __func__, _mv, _reg64, ret);
	return ret;
}

static void pre_gauge_update(struct mtk_gauge *gauge)
{
	int m = 0;
	unsigned int reg_val = 0;

	regmap_update_bits(gauge->regmap,
		PMIC_FG_SW_READ_PRE_ADDR,
		PMIC_FG_SW_READ_PRE_MASK << PMIC_FG_SW_READ_PRE_SHIFT,
		1 << PMIC_FG_SW_READ_PRE_SHIFT);
	do {
		m++;
		if (m > 1000) {
			bm_err("[%s] gauge_update_polling timeout 1!\r\n",
				__func__);
			break;
		}
		regmap_read(gauge->regmap, PMIC_FG_LATCHDATA_ST_ADDR, &reg_val);
		reg_val =
			(reg_val & (PMIC_FG_LATCHDATA_ST_MASK
			<< PMIC_FG_LATCHDATA_ST_SHIFT))
			>> PMIC_FG_LATCHDATA_ST_SHIFT;
	} while (reg_val == 0);
}

static void post_gauge_update(struct mtk_gauge *gauge)
{
	int m = 0;
	unsigned int reg_val;

	regmap_update_bits(gauge->regmap,
		PMIC_FG_SW_CLEAR_ADDR,
		PMIC_FG_SW_CLEAR_MASK << PMIC_FG_SW_CLEAR_SHIFT,
		1 << PMIC_FG_SW_CLEAR_SHIFT);
	regmap_update_bits(gauge->regmap,
		PMIC_FG_SW_READ_PRE_ADDR,
		PMIC_FG_SW_READ_PRE_MASK << PMIC_FG_SW_READ_PRE_SHIFT,
		0 << PMIC_FG_SW_READ_PRE_SHIFT);

	do {
		m++;
		if (m > 1000) {
			bm_err("[%s] gauge_update_polling timeout 2!\r\n",
				__func__);
			break;
		}
		regmap_read(gauge->regmap, PMIC_FG_LATCHDATA_ST_ADDR, &reg_val);
		reg_val =
			(reg_val & (PMIC_FG_LATCHDATA_ST_MASK
			<< PMIC_FG_LATCHDATA_ST_SHIFT))
			>> PMIC_FG_LATCHDATA_ST_SHIFT;
	} while (reg_val != 0);

	regmap_update_bits(gauge->regmap,
		PMIC_FG_SW_CLEAR_ADDR,
		PMIC_FG_SW_CLEAR_MASK << PMIC_FG_SW_CLEAR_SHIFT,
		0 << PMIC_FG_SW_CLEAR_SHIFT);
}

static int reg_to_current(struct mtk_gauge *gauge,
	unsigned int regval)
{
	unsigned short uvalue16;
	int dvalue, retval;
	long long temp_value = 0;
	bool is_charging = true;

	uvalue16 = (unsigned short) regval;

	dvalue = (unsigned int) uvalue16;
	if (dvalue == 0) {
		temp_value = (long long) dvalue;
	} else if (dvalue > 32767) {
		/* > 0x8000 */
		temp_value = (long long) (dvalue - 65535);
		temp_value = temp_value - (temp_value * 2);
		is_charging = false;
	} else {
		temp_value = (long long) dvalue;
	}

	temp_value = temp_value * UNIT_FGCURRENT;
#if defined(__LP64__) || defined(_LP64)
	do_div(temp_value, 100000);
#else
	temp_value = div_s64(temp_value, 100000);
#endif
	retval = (unsigned int) temp_value;

	bm_debug("[%s] 0x%x 0x%x 0x%x 0x%x 0x%x %d\n",
		__func__,
		regval,
		uvalue16,
		dvalue,
		(int)temp_value,
		retval,
		is_charging);

	if (is_charging == false)
		return -retval;

	return retval;
}

/* ============================================================ */
/* pmic control end*/
/* ============================================================ */
int get_rtc_spare0_fg_value(struct mtk_gauge *gauge)
{
	struct nvmem_cell *cell;
	u8 *buf;

	cell = nvmem_cell_get(&gauge->pdev->dev, "initialization");
	if (IS_ERR(cell)) {
		bm_err("[%s]get rtc cell fail\n", __func__);
		return 0;
	}

	buf = nvmem_cell_read(cell, NULL);
	nvmem_cell_put(cell);
	if (IS_ERR(buf)) {
		bm_err("[%s]read rtc cell fail\n", __func__);
		return 0;
	}
	bm_debug("[%s] val=%d\n", __func__, *buf);
	return *buf;
}

void set_rtc_spare0_fg_value(struct mtk_gauge *gauge, u8 val)
{
	struct nvmem_cell *cell;
	u32 length = 1;
	int ret;

	cell = nvmem_cell_get(&gauge->pdev->dev, "initialization");
	if (IS_ERR(cell)) {
		bm_err("[%s]get rtc cell fail\n", __func__);
		return;
	}

	ret = nvmem_cell_write(cell, &val, length);
	nvmem_cell_put(cell);
	if (ret != length)
		bm_err("[%s] write rtc cell fail\n", __func__);
}

int get_rtc_spare_fg_value(struct mtk_gauge *gauge)
{
	struct nvmem_cell *cell;
	u8 *buf;

	cell = nvmem_cell_get(&gauge->pdev->dev, "state-of-charge");
	if (IS_ERR(cell)) {
		bm_err("[%s]get rtc cell fail\n", __func__);
		return 0;
	}

	buf = nvmem_cell_read(cell, NULL);
	nvmem_cell_put(cell);

	if (IS_ERR(buf)) {
		bm_err("[%s]read rtc cell fail\n", __func__);
		return 0;
	}

	bm_debug("[%s] val=%d\n", __func__, *buf);

	return *buf;
}

void set_rtc_spare_fg_value(struct mtk_gauge *gauge, u8 val)
{
	struct nvmem_cell *cell;
	u32 length = 1;
	int ret;

	cell = nvmem_cell_get(&gauge->pdev->dev, "state-of-charge");
	if (IS_ERR(cell)) {
		bm_err("[%s]get rtc cell fail\n", __func__);
		return;
	}

	ret = nvmem_cell_write(cell, &val, length);
	nvmem_cell_put(cell);

	if (ret != length)
		bm_err("[%s] write rtc cell fail\n", __func__);

	bm_debug("[%s] val=%d\n", __func__, val);
}

static int fgauge_set_info(struct mtk_gauge *gauge,
	enum gauge_property ginfo, unsigned int value)
{

	bm_debug("[%s]info:%d v:%d\n", __func__, ginfo, value);

	if (ginfo == GAUGE_PROP_2SEC_REBOOT)
		regmap_update_bits(gauge->regmap,
		PMIC_RG_SYSTEM_INFO_CON0_ADDR,
		0x0001,
		value);
	else if (ginfo == GAUGE_PROP_PL_CHARGING_STATUS)
		regmap_update_bits(gauge->regmap,
		PMIC_RG_SYSTEM_INFO_CON0_ADDR,
		0x0001 << 0x1,
		value << 0x1);
	else if (ginfo == GAUGE_PROP_MONITER_PLCHG_STATUS)
		regmap_update_bits(gauge->regmap,
		PMIC_RG_SYSTEM_INFO_CON0_ADDR,
		0x0001 << 0x2,
		value << 0x2);
	else if (ginfo == GAUGE_PROP_BAT_PLUG_STATUS)
		regmap_update_bits(gauge->regmap,
		PMIC_RG_SYSTEM_INFO_CON0_ADDR,
		0x0001 << 0x3,
		value << 0x3);
	else if (ginfo == GAUGE_PROP_IS_NVRAM_FAIL_MODE)
		regmap_update_bits(gauge->regmap,
		PMIC_RG_SYSTEM_INFO_CON0_ADDR,
		0x0001 << 0x4,
		value << 0x4);
	else if (ginfo == GAUGE_PROP_MONITOR_SOFF_VALIDTIME)
		regmap_update_bits(gauge->regmap,
		PMIC_RG_SYSTEM_INFO_CON0_ADDR,
		0x0001 << 0x5,
		value << 0x5);
	else if (ginfo == GAUGE_PROP_CON0_SOC) {
		value = value / 100;
		regmap_update_bits(gauge->regmap,
		PMIC_RG_SYSTEM_INFO_CON0_ADDR,
		0x007f << 0x9,
		value << 0x9);
	}
	return 0;
}

static int fgauge_get_info(struct mtk_gauge *gauge,
	enum gauge_property ginfo, int *value)
{
	int reg_val = 0;

	regmap_read(gauge->regmap, PMIC_RG_SYSTEM_INFO_CON0_ADDR, &reg_val);

	if (ginfo == GAUGE_PROP_2SEC_REBOOT)
		*value = reg_val & 0x0001;
	else if (ginfo == GAUGE_PROP_PL_CHARGING_STATUS)
		*value =
		(reg_val & (0x0001 << 0x1))	>> 0x1;
	else if (ginfo == GAUGE_PROP_MONITER_PLCHG_STATUS)
		*value =
		(reg_val & (0x0001 << 0x2))	>> 0x2;
	else if (ginfo == GAUGE_PROP_BAT_PLUG_STATUS)
		*value =
		(reg_val & (0x0001 << 0x3))	>> 0x3;
	else if (ginfo == GAUGE_PROP_IS_NVRAM_FAIL_MODE)
		*value =
		(reg_val & (0x0001 << 0x4))	>> 0x4;
	else if (ginfo == GAUGE_PROP_MONITOR_SOFF_VALIDTIME)
		*value =
		(reg_val & (0x0001 << 0x5))	>> 0x5;
	else if (ginfo == GAUGE_PROP_CON0_SOC)
		*value =
		(reg_val & (0x007F << 0x9))	>> 0x9;

	bm_debug("[%s]info:%d v:%d\n", __func__, ginfo, *value);

	return 0;
}

static void fgauge_set_nafg_intr_internal(struct mtk_gauge *gauge,
	int _prd, int _zcv_mv, int _thr_mv)
{
	int NAG_C_DLTV_Threashold_26_16;
	int NAG_C_DLTV_Threashold_15_0;

	gauge->zcv_reg = mv_to_reg_value(_zcv_mv);
	gauge->thr_reg = mv_to_reg_value(_thr_mv);

	NAG_C_DLTV_Threashold_26_16 = (gauge->thr_reg & 0xffff0000) >> 16;
	NAG_C_DLTV_Threashold_15_0 = (gauge->thr_reg & 0x0000ffff);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_NAG_ZCV_ADDR,
		PMIC_AUXADC_NAG_ZCV_MASK << PMIC_AUXADC_NAG_ZCV_SHIFT,
		gauge->zcv_reg << PMIC_AUXADC_NAG_ZCV_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_NAG_C_DLTV_TH_26_16_ADDR,
		PMIC_AUXADC_NAG_C_DLTV_TH_26_16_MASK <<
		PMIC_AUXADC_NAG_C_DLTV_TH_26_16_SHIFT,
		NAG_C_DLTV_Threashold_26_16 <<
		PMIC_AUXADC_NAG_C_DLTV_TH_26_16_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_NAG_C_DLTV_TH_15_0_ADDR,
		PMIC_AUXADC_NAG_C_DLTV_TH_15_0_MASK <<
		PMIC_AUXADC_NAG_C_DLTV_TH_15_0_SHIFT,
		NAG_C_DLTV_Threashold_15_0 <<
		PMIC_AUXADC_NAG_C_DLTV_TH_15_0_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_NAG_PRD_ADDR,
		PMIC_AUXADC_NAG_PRD_MASK <<
		PMIC_AUXADC_NAG_PRD_SHIFT,
		_prd <<
		PMIC_AUXADC_NAG_PRD_SHIFT);

/* TODO is_power_path_supported()*/

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_NAG_VBAT1_SEL_ADDR,
		PMIC_AUXADC_NAG_VBAT1_SEL_MASK <<
		PMIC_AUXADC_NAG_VBAT1_SEL_SHIFT,
		1 <<
		PMIC_AUXADC_NAG_VBAT1_SEL_SHIFT);

	bm_debug("[fg_bat_nafg][fgauge_set_nafg_interrupt_internal] time[%d] zcv[%d:%d] thr[%d:%d] 26_16[0x%x] 15_00[0x%x]\n",
		_prd, _zcv_mv, gauge->zcv_reg, _thr_mv, gauge->thr_reg,
		NAG_C_DLTV_Threashold_26_16, NAG_C_DLTV_Threashold_15_0);

}

int nafg_zcv_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int zcv)
{
	gauge->nafg_zcv_mv = zcv;	/* 0.1 mv*/
	return 0;
}

int zcv_current_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *zcv_current)
{
	unsigned int uvalue16 = 0;
	signed int dvalue = 0;
	long long Temp_Value = 0;

	regmap_read(gauge->regmap, PMIC_FG_ZCV_CURR_ADDR,
		&uvalue16);
	uvalue16 =
		(uvalue16 & (PMIC_FG_ZCV_CURR_MASK
		<< PMIC_FG_ZCV_CURR_SHIFT))
		>> PMIC_FG_ZCV_CURR_SHIFT;

	dvalue = uvalue16;
		if (dvalue == 0) {
			Temp_Value = (long long) dvalue;
		} else if (dvalue > 32767) {
			/* > 0x8000 */
			Temp_Value = (long long) (dvalue - 65535);
			Temp_Value = Temp_Value - (Temp_Value * 2);
		} else {
			Temp_Value = (long long) dvalue;
		}

	Temp_Value = Temp_Value * UNIT_FGCURRENT;

#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 100000);
#else
	Temp_Value = div_s64(Temp_Value, 100000);
#endif
	dvalue = (unsigned int) Temp_Value;

	/* Auto adjust value */
	if (gauge->gm->fg_cust_data.r_fg_value != DEFAULT_R_FG) {
		bm_debug(
		"[fgauge_read_current] Auto adjust value due to the Rfg is %d\n Ori curr=%d",
		gauge->gm->fg_cust_data.r_fg_value, dvalue);

		dvalue = (dvalue * DEFAULT_R_FG) /
		gauge->gm->fg_cust_data.r_fg_value;

		bm_debug("[fgauge_read_current] new current=%d\n", dvalue);
	}

	bm_debug("[fgauge_read_current] ori current=%d\n", dvalue);
	dvalue = ((dvalue * gauge->gm->fg_cust_data.car_tune_value) / 1000);
	bm_debug("[fgauge_read_current] final current=%d (ratio=%d)\n",
		 dvalue, gauge->gm->fg_cust_data.car_tune_value);
	*zcv_current = dvalue;

	return 0;
}

int nafg_c_dltv_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int c_dltv_mv)
{
	gauge->nafg_c_dltv_mv = c_dltv_mv;	/* 0.1 mv*/
	fgauge_set_nafg_intr_internal(
	gauge,
	gauge->gm->fg_cust_data.nafg_time_setting,
	gauge->nafg_zcv_mv, gauge->nafg_c_dltv_mv);
	return 0;
}

static int get_nafg_vbat(struct mtk_gauge *gauge)
{
	unsigned int nag_vbat_reg, vbat_val;
	int nag_vbat_mv, i = 0;

	do {
		regmap_read(gauge->regmap, PMIC_AUXADC_ADC_OUT_NAG_ADDR,
			&nag_vbat_reg);

		nag_vbat_reg =
			(nag_vbat_reg & (PMIC_AUXADC_ADC_OUT_NAG_MASK <<
			PMIC_AUXADC_ADC_OUT_NAG_SHIFT))
			>> PMIC_AUXADC_ADC_OUT_NAG_SHIFT;

		if ((nag_vbat_reg & 0x8000) != 0)
			break;
		msleep(30);
		i++;
	} while (i <= 5);

	vbat_val = nag_vbat_reg & 0x7fff;
	nag_vbat_mv = reg_to_mv_value(vbat_val);
	return nag_vbat_mv;
}

int nafg_vbat_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *vbat)
{
	*vbat = get_nafg_vbat(gauge);
	return 0;
}

int bat_plugout_en_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	if (val != 0) {
		val = 1;
		enable_gauge_irq(gauge, BAT_PLUGOUT_IRQ);
	} else
		disable_gauge_irq(gauge, BAT_PLUGOUT_IRQ);

	return 0;
}

static void fgauge_set_zcv_intr_internal(
	struct mtk_gauge *gauge_dev,
	int fg_zcv_det_time,
	int fg_zcv_car_th)
{
	int fg_zcv_car_thr_h_reg, fg_zcv_car_thr_l_reg;
	long long fg_zcv_car_th_reg = fg_zcv_car_th;

	fg_zcv_car_th_reg = (fg_zcv_car_th_reg * 100 * 1000);

#if defined(__LP64__) || defined(_LP64)
	do_div(fg_zcv_car_th_reg, UNIT_FGCAR_ZCV);
#else
	fg_zcv_car_th_reg = div_s64(fg_zcv_car_th_reg, UNIT_FGCAR_ZCV);
#endif

	if (gauge_dev->hw_status.r_fg_value != DEFAULT_R_FG)
#if defined(__LP64__) || defined(_LP64)
		fg_zcv_car_th_reg = (fg_zcv_car_th_reg *
				gauge_dev->hw_status.r_fg_value) /
				DEFAULT_R_FG;
#else
		fg_zcv_car_th_reg = div_s64(fg_zcv_car_th_reg *
				gauge_dev->hw_status.r_fg_value,
				DEFAULT_R_FG);
#endif

#if defined(__LP64__) || defined(_LP64)
	fg_zcv_car_th_reg = ((fg_zcv_car_th_reg * 1000) /
			gauge_dev->hw_status.car_tune_value);
#else
	fg_zcv_car_th_reg = div_s64((fg_zcv_car_th_reg * 1000),
			gauge_dev->hw_status.car_tune_value);
#endif

	fg_zcv_car_thr_h_reg = (fg_zcv_car_th_reg & 0xffff0000) >> 16;
	fg_zcv_car_thr_l_reg = fg_zcv_car_th_reg & 0x0000ffff;


	regmap_update_bits(gauge_dev->regmap,
		PMIC_FG_ZCV_CAR_TH_15_00_ADDR,
		PMIC_FG_ZCV_CAR_TH_15_00_MASK <<
		PMIC_FG_ZCV_CAR_TH_15_00_SHIFT,
		fg_zcv_car_thr_l_reg <<
		PMIC_FG_ZCV_CAR_TH_15_00_SHIFT);

	regmap_update_bits(gauge_dev->regmap,
		PMIC_FG_ZCV_CAR_TH_31_16_ADDR,
		PMIC_FG_ZCV_CAR_TH_31_16_MASK <<
		PMIC_FG_ZCV_CAR_TH_31_16_SHIFT,
		fg_zcv_car_thr_h_reg <<
		PMIC_FG_ZCV_CAR_TH_31_16_SHIFT);

	bm_debug("[FG_ZCV_INT][%s] det_time %d mv %d reg %lld 31_16 0x%x 15_00 0x%x\n",
		__func__, fg_zcv_det_time, fg_zcv_car_th, fg_zcv_car_th_reg,
		fg_zcv_car_thr_h_reg, fg_zcv_car_thr_l_reg);
}

int zcv_intr_threshold_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int zcv_avg_current)
{
	int fg_zcv_det_time;
	int fg_zcv_car_th = 0;

	fg_zcv_det_time = gauge->gm->fg_cust_data.zcv_suspend_time;
	fg_zcv_car_th = (fg_zcv_det_time + 1) * 4 * zcv_avg_current / 60;

	bm_debug("[%s] current:%d, fg_zcv_det_time:%d, fg_zcv_car_th:%d\n",
		__func__, zcv_avg_current, fg_zcv_det_time, fg_zcv_car_th);

	fgauge_set_zcv_intr_internal(
		gauge, fg_zcv_det_time, fg_zcv_car_th);

	return 0;
}

int zcv_intr_en_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int en)
{
	static int cnt;

	bm_debug("%s %d %d\n", __func__,
		cnt, en);
	if (en != 0)
		cnt++;
	else
		cnt--;

	if (en == 0) {
		disable_gauge_irq(gauge, ZCV_IRQ);
		regmap_update_bits(gauge->regmap,
			PMIC_FG_ZCV_DET_EN_ADDR,
			PMIC_FG_ZCV_DET_EN_MASK <<
			PMIC_FG_ZCV_DET_EN_SHIFT,
			en <<
			PMIC_FG_ZCV_DET_EN_SHIFT);
		mdelay(1);
	}
	if (en == 1) {
		enable_gauge_irq(gauge, ZCV_IRQ);
		regmap_update_bits(gauge->regmap,
			PMIC_FG_ZCV_DET_EN_ADDR,
			PMIC_FG_ZCV_DET_EN_MASK <<
			PMIC_FG_ZCV_DET_EN_SHIFT,
			en <<
			PMIC_FG_ZCV_DET_EN_SHIFT);
	}

	bm_debug("[FG_ZCV_INT][fg_set_zcv_intr_en] En %d\n", en);

	return 0;
}

int soff_reset_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int en)
{
	return 0;
}

int ncar_reset_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	return 0;
}


int event_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int event)
{

	return 0;
}

int bat_tmp_ht_threshold_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	return 0;
}

int en_bat_tmp_ht_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int en)
{
	return 0;
}

int bat_tmp_lt_threshold_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	return 0;
}

int en_bat_tmp_lt_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int en)
{
	return 0;
}

int bat_cycle_intr_threshold_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	return 0;
}

int fgauge_get_time(struct mtk_gauge *gauge_dev, unsigned int *ptime)
{
	unsigned int time_31_16, time_15_00, ret_time;
	long long time = 0;

	pre_gauge_update(gauge_dev);

	regmap_read(gauge_dev->regmap, PMIC_FG_NTER_15_00_ADDR,
		&time_15_00);
	time_15_00 =
		(time_15_00 & (PMIC_FG_NTER_15_00_MASK <<
		PMIC_FG_NTER_15_00_SHIFT))
		>> PMIC_FG_NTER_15_00_SHIFT;

	regmap_read(gauge_dev->regmap, PMIC_FG_NTER_31_16_ADDR,
		&time_31_16);
	time_31_16 =
		(time_31_16 & (PMIC_FG_NTER_31_16_MASK <<
		PMIC_FG_NTER_31_16_SHIFT))
		>> PMIC_FG_NTER_31_16_SHIFT;

	time = time_15_00;
	time |= time_31_16 << 16;
#if defined(__LP64__) || defined(_LP64)
	time = time * UNIT_TIME / 100;
#else
	time = div_s64(time * UNIT_TIME, 100);
#endif
	ret_time = time;

	bm_debug(
		 "[%s] low:0x%x high:0x%x rtime:0x%llx 0x%x!\r\n",
		 __func__, time_15_00, time_31_16, time, ret_time);

	post_gauge_update(gauge_dev);

	*ptime = ret_time;

	return 0;
}

static int instant_current(struct mtk_gauge *gauge)
{
	unsigned int reg_value;
	int dvalue;
	int r_fg_value;
	int car_tune_value;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->hw_status.car_tune_value;
	pre_gauge_update(gauge);

	regmap_read(gauge->regmap, PMIC_FG_CURRENT_OUT_ADDR, &reg_value);
	reg_value = (reg_value &
		(PMIC_FG_CURRENT_OUT_MASK << PMIC_FG_CURRENT_OUT_SHIFT))
		>> PMIC_FG_CURRENT_OUT_SHIFT;

	post_gauge_update(gauge);
	dvalue = reg_to_current(gauge, reg_value);

	/* Auto adjust value */
	if (r_fg_value != DEFAULT_R_FG) {

		dvalue = (dvalue * DEFAULT_R_FG) /
			r_fg_value;
	}

	dvalue =
	((dvalue * car_tune_value) / 1000);

	return dvalue;
}

void read_fg_hw_info_current_1(struct mtk_gauge *gauge_dev)
{
	gauge_dev->fg_hw_info.current_1 =
		instant_current(gauge_dev);
}

void read_fg_hw_info_current_2(struct mtk_gauge *gauge_dev)
{
	long long fg_current_2_reg;
	int cic2_reg;
	signed int dvalue;
	long long Temp_Value;
	int sign_bit = 0;

	regmap_read(gauge_dev->regmap, PMIC_FG_CIC2_ADDR,
		&cic2_reg);
	cic2_reg =
		(cic2_reg & (PMIC_FG_CIC2_MASK <<
		PMIC_FG_CIC2_SHIFT))
		>> PMIC_FG_CIC2_SHIFT;
	fg_current_2_reg = cic2_reg;

	/*calculate the real world data    */
	dvalue = (unsigned int) fg_current_2_reg;
	if (dvalue == 0) {
		Temp_Value = (long long) dvalue;
		sign_bit = 0;
	} else if (dvalue > 32767) {
		/* > 0x8000 */
		Temp_Value = (long long) (dvalue - 65535);
		Temp_Value = Temp_Value - (Temp_Value * 2);
		sign_bit = 1;
	} else {
		Temp_Value = (long long) dvalue;
		sign_bit = 0;
	}

	Temp_Value = Temp_Value * UNIT_FGCURRENT;
#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 100000);
#else
	Temp_Value = div_s64(Temp_Value, 100000);
#endif
	dvalue = (unsigned int) Temp_Value;


	if (gauge_dev->hw_status.r_fg_value != DEFAULT_R_FG)
		dvalue = (dvalue * DEFAULT_R_FG) /
			gauge_dev->hw_status.r_fg_value;

	if (sign_bit == 1)
		dvalue = dvalue - (dvalue * 2);

	gauge_dev->fg_hw_info.current_2 =
		((dvalue * gauge_dev->hw_status.car_tune_value) / 1000);

}

static int average_current_get(struct mtk_gauge *gauge_dev,
	struct mtk_gauge_sysfs_field_info *attr, int *data)
{

	return 0;
}

static signed int fg_set_iavg_intr(struct mtk_gauge *gauge_dev, void *data)
{
	return 0;
}

void read_fg_hw_info_ncar(struct mtk_gauge *gauge_dev)
{

}

static int coulomb_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	unsigned int uvalue32_car = 0;
	unsigned int uvalue32_car_msb = 0;
	unsigned int temp_car_15_0 = 0;
	unsigned int temp_car_31_16 = 0;
	signed int dvalue_CAR = 0;
	long long temp_value = 0;
	int r_fg_value;
	int car_tune_value;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->hw_status.car_tune_value;
	pre_gauge_update(gauge);

	regmap_read(gauge->regmap, PMIC_FG_CAR_15_00_ADDR, &temp_car_15_0);
	temp_car_15_0 =	(temp_car_15_0 &
		(PMIC_FG_CAR_15_00_MASK << PMIC_FG_CAR_15_00_SHIFT))
		>> PMIC_FG_CAR_15_00_SHIFT;

	regmap_read(gauge->regmap, PMIC_FG_CAR_31_16_ADDR, &temp_car_31_16);
	temp_car_31_16 = (temp_car_31_16 &
		(PMIC_FG_CAR_31_16_MASK << PMIC_FG_CAR_31_16_SHIFT))
		>> PMIC_FG_CAR_31_16_SHIFT;

	post_gauge_update(gauge);

	uvalue32_car = temp_car_15_0 & 0xffff;
	uvalue32_car |= (temp_car_31_16 & 0x7fff) << 16;

	uvalue32_car_msb = (temp_car_31_16 & 0x8000) >> 15;

	/*calculate the real world data    */
	dvalue_CAR = (signed int) uvalue32_car;

	if (uvalue32_car == 0) {
		temp_value = 0;
	} else if (uvalue32_car_msb == 0x1) {
		/* dis-charging */
		temp_value = (long long) (dvalue_CAR - 0x7fffffff);
		/* keep negative value */
		temp_value = temp_value - (temp_value * 2);
	} else {
		/*charging */
		temp_value = (long long) dvalue_CAR;
	}

#if defined(__LP64__) || defined(_LP64)
	temp_value = temp_value * UNIT_CHARGE / 1000;
#else
	temp_value = div_s64(temp_value * UNIT_CHARGE, 1000);
#endif


#if defined(__LP64__) || defined(_LP64)
	do_div(temp_value, 10);
	temp_value = temp_value + 5;
	do_div(temp_value, 10);
#else
	temp_value = div_s64(temp_value, 10);
	temp_value = temp_value + 5;
	temp_value = div_s64(temp_value, 10);
#endif


	if (uvalue32_car_msb == 0x1)
		dvalue_CAR = (signed int) (temp_value - (temp_value * 2));
		/* keep negative value */
	else
		dvalue_CAR = (signed int) temp_value;


	bm_debug("[%s]l:0x%x h:0x%x val:%d msb:%d car:%d\n",
		__func__,
		temp_car_15_0, temp_car_31_16,
		uvalue32_car, uvalue32_car_msb,
		dvalue_CAR);

/*Auto adjust value*/
	if (r_fg_value != DEFAULT_R_FG) {
		bm_debug("[%s] Auto adjust value deu to the Rfg is %d\n Ori CAR=%d",
			 __func__,
			 r_fg_value, dvalue_CAR);

		dvalue_CAR = (dvalue_CAR * DEFAULT_R_FG) /
			r_fg_value;

		bm_debug("[%s] new CAR=%d\n",
			__func__,
			dvalue_CAR);
	}

	dvalue_CAR = ((dvalue_CAR *
		car_tune_value) / 1000);

	bm_debug("[%s] CAR=%d r_fg_value=%d car_tune_value=%d\n",
		__func__,
		dvalue_CAR, r_fg_value,
		car_tune_value);

	*val = dvalue_CAR;

	return 0;
}

int hw_info_set(struct mtk_gauge *gauge_dev,
	struct mtk_gauge_sysfs_field_info *attr, int en)
{
	int ret;
	char intr_name[32];
	int is_iavg_valid;
	int avg_current;
	int iavg_th;
	unsigned int time;
	struct gauge_hw_status *gauge_status;

	gauge_status = &gauge_dev->hw_status;
	/* Set Read Latchdata */
	post_gauge_update(gauge_dev);

	/* Current_1 */
	read_fg_hw_info_current_1(gauge_dev);

	/* Current_2 */
	read_fg_hw_info_current_2(gauge_dev);

	/* curr_out = pmic_get_register_value(PMIC_FG_CURRENT_OUT); */
	/* fg_offset = pmic_get_register_value(PMIC_FG_OFFSET); */

	/* Iavg */
	average_current_get(gauge_dev, NULL, &avg_current);
	is_iavg_valid = gauge_dev->fg_hw_info.current_avg_valid;
	if ((is_iavg_valid == 1) && (gauge_status->iavg_intr_flag == 0)) {
		bm_debug("[read_fg_hw_info]set first fg_set_iavg_intr %d %d\n",
			is_iavg_valid, gauge_status->iavg_intr_flag);
		gauge_status->iavg_intr_flag = 1;
		iavg_th = gauge_dev->gm->fg_cust_data.diff_iavg_th;
		ret = fg_set_iavg_intr(gauge_dev, &iavg_th);
	} else if (is_iavg_valid == 0) {
		gauge_status->iavg_intr_flag = 0;
		disable_gauge_irq(gauge_dev, FG_IAVG_H_IRQ);
		disable_gauge_irq(gauge_dev, FG_IAVG_L_IRQ);
		bm_debug(
			"[read_fg_hw_info] doublecheck first fg_set_iavg_intr %d %d\n",
			is_iavg_valid, gauge_status->iavg_intr_flag);
	}
	bm_debug("[read_fg_hw_info] thirdcheck first fg_set_iavg_intr %d %d\n",
		is_iavg_valid, gauge_status->iavg_intr_flag);

	/* Ncar */
	read_fg_hw_info_ncar(gauge_dev);

	/* recover read */
	post_gauge_update(gauge_dev);

	coulomb_get(gauge_dev, NULL, &gauge_dev->fg_hw_info.car);
	fgauge_get_time(gauge_dev, &time);
	gauge_dev->fg_hw_info.time = time;

	bm_debug("[FGADC_intr_end][%s][read_fg_hw_info] curr_1 %d curr_2 %d Iavg %d sign %d car %d ncar %d time %d\n",
		intr_name, gauge_dev->fg_hw_info.current_1,
		gauge_dev->fg_hw_info.current_2,
		gauge_dev->fg_hw_info.current_avg,
		gauge_dev->fg_hw_info.current_avg_sign,
		gauge_dev->fg_hw_info.car,
		gauge_dev->fg_hw_info.ncar, gauge_dev->fg_hw_info.time);

	return 0;
}

int nafg_en_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	static int cnt;

	bm_debug("%s %d %d\n", __func__,
		cnt, val);
	if (val != 0)
		cnt++;
	else
		cnt--;

	if (val != 0) {
		val = 1;
		enable_gauge_irq(gauge, NAFG_IRQ);
		bm_debug("[%s]enable:%d\n", __func__, val);
	} else {
		disable_gauge_irq(gauge, NAFG_IRQ);
		bm_debug("[%s]disable:%d\n", __func__, val);
	}
	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_NAG_IRQ_EN_ADDR,
		PMIC_AUXADC_NAG_IRQ_EN_MASK
		<< PMIC_AUXADC_NAG_IRQ_EN_SHIFT,
		val << PMIC_AUXADC_NAG_IRQ_EN_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_NAG_EN_ADDR,
		PMIC_AUXADC_NAG_EN_MASK
		<< PMIC_AUXADC_NAG_EN_SHIFT,
		val << PMIC_AUXADC_NAG_EN_SHIFT);

	return 0;
}

int info_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	int ret = 0;

	if (attr->prop == GAUGE_PROP_CAR_TUNE_VALUE &&
		(val > 500 && val < 1500))
		gauge->hw_status.car_tune_value = val;
	else if (attr->prop == GAUGE_PROP_R_FG_VALUE &&
		val != 0)
		gauge->hw_status.r_fg_value = val;
	else if (attr->prop == GAUGE_PROP_VBAT2_DETECT_TIME)
		gauge->hw_status.vbat2_det_time = val;
	else if (attr->prop == GAUGE_PROP_VBAT2_DETECT_COUNTER)
		gauge->hw_status.vbat2_det_counter = val;
	else
		ret = fgauge_set_info(gauge, attr->prop, val);

	return ret;
}

int info_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	int ret = 0;

	if (attr->prop == GAUGE_PROP_CAR_TUNE_VALUE)
		*val = gauge->hw_status.car_tune_value;
	else if (attr->prop == GAUGE_PROP_R_FG_VALUE)
		*val = gauge->hw_status.r_fg_value;
	else if (attr->prop == GAUGE_PROP_VBAT2_DETECT_TIME)
		*val = gauge->hw_status.vbat2_det_time;
	else if (attr->prop == GAUGE_PROP_VBAT2_DETECT_COUNTER)
		*val = gauge->hw_status.vbat2_det_counter;
	else
		ret = fgauge_get_info(gauge, attr->prop, val);

	return ret;
}

static int get_ptim_current(struct mtk_gauge *gauge)
{
	unsigned int reg_value;
	int dvalue;
	int r_fg_value;
	int car_tune_value;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->hw_status.car_tune_value;
	pre_gauge_update(gauge);
	regmap_read(gauge->regmap, PMIC_FG_R_CURR_ADDR, &reg_value);
	reg_value =
		(reg_value & (PMIC_FG_R_CURR_MASK << PMIC_FG_R_CURR_SHIFT))
		>> PMIC_FG_R_CURR_SHIFT;
	post_gauge_update(gauge);
	dvalue = reg_to_current(gauge, reg_value);

	/* Auto adjust value */
	if (r_fg_value != DEFAULT_R_FG)
		dvalue = (dvalue * DEFAULT_R_FG) / r_fg_value;

	dvalue =
	((dvalue * car_tune_value) / 1000);

	bm_debug("[%s]current:%d\n", __func__, dvalue);

	return dvalue;
}

static enum power_supply_property gauge_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static int psy_gauge_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mtk_gauge *gauge;

	gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_ptim_current(gauge);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void fgauge_read_RTC_boot_status(struct mtk_gauge *gauge)
{
	unsigned int hw_id;
	unsigned int spare0_reg = 0;
	unsigned int spare0_reg_b13 = 0;
	int spare3_reg = 0;
	int spare3_reg_valid = 0;

	regmap_read(gauge->regmap, PMIC_HWCID_ADDR, &hw_id);
	hw_id =	(hw_id & (PMIC_HWCID_MASK << PMIC_HWCID_SHIFT))
		>> PMIC_HWCID_SHIFT;
	spare0_reg = get_rtc_spare0_fg_value(gauge);
	spare3_reg = get_rtc_spare_fg_value(gauge);
	gauge->hw_status.gspare0_reg = spare0_reg;
	gauge->hw_status.gspare3_reg = spare3_reg;
	spare3_reg_valid = (spare3_reg & 0x80) >> 7;

	if (spare3_reg_valid == 0)
		gauge->hw_status.rtc_invalid = 1;
	else
		gauge->hw_status.rtc_invalid = 0;

	if (gauge->hw_status.rtc_invalid == 0) {
		spare0_reg_b13 = (spare0_reg & 0x20) >> 5;
		if ((hw_id & 0xff00) == 0x3500)
			gauge->hw_status.is_bat_plugout = spare0_reg_b13;
		else
			gauge->hw_status.is_bat_plugout = !spare0_reg_b13;

		gauge->hw_status.bat_plug_out_time = spare0_reg & 0x1f;
	} else {
		gauge->hw_status.is_bat_plugout = 1;
		gauge->hw_status.bat_plug_out_time = 31;
	}

	bm_err("[%s]rtc_invalid %d plugout %d plugout_time %d spare3 0x%x spare0 0x%x hw_id 0x%x\n",
			__func__,
			gauge->hw_status.rtc_invalid,
			gauge->hw_status.is_bat_plugout,
			gauge->hw_status.bat_plug_out_time,
			spare3_reg, spare0_reg, hw_id);
}

static int reset_fg_rtc_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	int hw_id;
	int temp_value;
	int spare0_reg, after_rst_spare0_reg;
	int spare3_reg, after_rst_spare3_reg;

	regmap_read(gauge->regmap, PMIC_HWCID_ADDR, &hw_id);
	hw_id =	(hw_id & (PMIC_HWCID_MASK << PMIC_HWCID_SHIFT))
		>> PMIC_HWCID_SHIFT;

	fgauge_read_RTC_boot_status(gauge);

	/* read spare0 */
	spare0_reg = get_rtc_spare0_fg_value(gauge);

	/* raise 15b to reset */
	if ((hw_id & 0xff00) == 0x3500) {
		temp_value = 0x80;
		set_rtc_spare0_fg_value(gauge, temp_value);
		mdelay(1);
		temp_value = 0x00;
		set_rtc_spare0_fg_value(gauge, temp_value);
	} else {
		temp_value = 0x80;
		set_rtc_spare0_fg_value(gauge, temp_value);
		mdelay(1);
		temp_value = 0x20;
		set_rtc_spare0_fg_value(gauge, temp_value);
	}

	/* read spare0 again */
	after_rst_spare0_reg = get_rtc_spare0_fg_value(gauge);

	/* read spare3 */
	spare3_reg = get_rtc_spare_fg_value(gauge);

	/* set spare3 0x7f */
	set_rtc_spare_fg_value(gauge, spare3_reg | 0x80);

	/* read spare3 again */
	after_rst_spare3_reg = get_rtc_spare_fg_value(gauge);

	bm_err("[fgauge_read_RTC_boot_status] spare0 0x%x 0x%x, spare3 0x%x 0x%x\n",
		spare0_reg, after_rst_spare0_reg, spare3_reg,
		after_rst_spare3_reg);

	return 0;
}

static int read_hw_ocv_6357_plug_in(struct mtk_gauge *gauge)
{
	signed int adc_rdy = 0;
	signed int adc_result_reg = 0;
	signed int adc_result = 0;
	int sel;

/* 6359 no need to switch SWCHR_POWER_PATH, only 56 57 */
	regmap_read(gauge->regmap, PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_PCHR_ADDR,
		&adc_rdy);
	adc_rdy = (adc_rdy & (PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_PCHR_MASK
		<< PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_PCHR_SHIFT))
		>> PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_PCHR_SHIFT;

	regmap_read(gauge->regmap, PMIC_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_ADDR,
		&adc_result_reg);
	adc_result_reg = (adc_result_reg &
		(PMIC_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_MASK
		<< PMIC_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_SHIFT))
		>> PMIC_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR_SHIFT;

	regmap_read(gauge->regmap, PMIC_RG_STRUP_AUXADC_START_SEL_ADDR,
		&sel);
	sel = (sel & (PMIC_RG_STRUP_AUXADC_START_SEL_MASK
		<< PMIC_RG_STRUP_AUXADC_START_SEL_SHIFT))
		>> PMIC_RG_STRUP_AUXADC_START_SEL_SHIFT;

	adc_result = reg_to_mv_value(adc_result_reg);
	bm_err("[oam] %s (pchr): adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
		__func__, adc_result_reg, adc_result,
		sel,
		adc_rdy);

	if (adc_rdy == 1) {
		regmap_update_bits(gauge->regmap,
			PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR_ADDR,
			PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR_MASK <<
			PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR_SHIFT,
			1 << PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR_SHIFT);
		mdelay(1);
		regmap_update_bits(gauge->regmap,
			PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR_ADDR,
			PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR_MASK <<
			PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR_SHIFT,
			0 << PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR_SHIFT);
	}

	return adc_result;
}

static int read_hw_ocv_6357_power_on(struct mtk_gauge *gauge)
{
	signed int adc_result_rdy = 0;
	signed int adc_result_reg = 0;
	signed int adc_result = 0;
	int sel;

	regmap_read(gauge->regmap, PMIC_AUXADC_ADC_RDY_PWRON_PCHR_ADDR,
		&adc_result_rdy);
	adc_result_rdy = (adc_result_rdy & (PMIC_AUXADC_ADC_RDY_PWRON_PCHR_MASK
		<< PMIC_AUXADC_ADC_RDY_PWRON_PCHR_SHIFT))
		>> PMIC_AUXADC_ADC_RDY_PWRON_PCHR_SHIFT;

	regmap_read(gauge->regmap, PMIC_AUXADC_ADC_OUT_PWRON_PCHR_ADDR,
		&adc_result_reg);
	adc_result_reg = (adc_result_reg & (PMIC_AUXADC_ADC_OUT_PWRON_PCHR_MASK
		<< PMIC_AUXADC_ADC_OUT_PWRON_PCHR_SHIFT))
		>> PMIC_AUXADC_ADC_OUT_PWRON_PCHR_SHIFT;

	regmap_read(gauge->regmap, PMIC_RG_STRUP_AUXADC_START_SEL_ADDR,
		&sel);
	sel = (sel & (PMIC_RG_STRUP_AUXADC_START_SEL_MASK
		<< PMIC_RG_STRUP_AUXADC_START_SEL_SHIFT))
		>> PMIC_RG_STRUP_AUXADC_START_SEL_SHIFT;

	adc_result = reg_to_mv_value(adc_result_reg);
	bm_err("[oam] %s (pchr) : adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
		__func__, adc_result_reg, adc_result,
		sel, adc_result_rdy);

	if (adc_result_rdy == 1) {
		regmap_update_bits(gauge->regmap,
			PMIC_AUXADC_ADC_RDY_PWRON_CLR_ADDR,
			PMIC_AUXADC_ADC_RDY_PWRON_CLR_MASK <<
			PMIC_AUXADC_ADC_RDY_PWRON_CLR_SHIFT,
			1 << PMIC_AUXADC_ADC_RDY_PWRON_CLR_SHIFT);
		mdelay(1);
		regmap_update_bits(gauge->regmap,
			PMIC_AUXADC_ADC_RDY_PWRON_CLR_ADDR,
			PMIC_AUXADC_ADC_RDY_PWRON_CLR_MASK <<
			PMIC_AUXADC_ADC_RDY_PWRON_CLR_SHIFT,
			0 << PMIC_AUXADC_ADC_RDY_PWRON_CLR_SHIFT);
	}
	return adc_result;
}

static int read_hw_ocv_6357_power_on_rdy(struct mtk_gauge *gauge)
{
	int pon_rdy = 0;

	regmap_read(gauge->regmap, PMIC_AUXADC_ADC_RDY_PWRON_PCHR_ADDR,
		&pon_rdy);
	pon_rdy = (pon_rdy & (PMIC_AUXADC_ADC_RDY_PWRON_PCHR_MASK
		<< PMIC_AUXADC_ADC_RDY_PWRON_PCHR_SHIFT))
		>> PMIC_AUXADC_ADC_RDY_PWRON_PCHR_SHIFT;

	bm_err("[%s] pwron_PCHR_rdy %d\n", __func__, pon_rdy);

	return pon_rdy;
}

static int nafg_cnt_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *nag_cnt)
{
	signed int NAG_C_DLTV_CNT;
	signed int NAG_C_DLTV_CNT_H;

	/*AUXADC_NAG_4*/
	regmap_read(gauge->regmap,
		PMIC_AUXADC_NAG_CNT_15_0_ADDR,
		&NAG_C_DLTV_CNT);

	/*AUXADC_NAG_5*/
	regmap_read(gauge->regmap,
		PMIC_AUXADC_NAG_CNT_25_16_ADDR,
		&NAG_C_DLTV_CNT_H);
	*nag_cnt = (NAG_C_DLTV_CNT & PMIC_AUXADC_NAG_CNT_15_0_MASK) +
		((NAG_C_DLTV_CNT_H & PMIC_AUXADC_NAG_CNT_25_16_MASK) << 16);
	bm_debug("[fg_bat_nafg][%s] %d [25_16 %d 15_0 %d]\n",
			__func__, *nag_cnt, NAG_C_DLTV_CNT_H, NAG_C_DLTV_CNT);

	return 0;
}

static int nafg_dltv_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *nag_dltv)
{
	signed int nag_dltv_reg_value;
	signed int nag_dltv_mv_value;
	short reg_value;

	/*AUXADC_NAG_4*/
	regmap_read(gauge->regmap,
		PMIC_AUXADC_NAG_DLTV_ADDR,
		&nag_dltv_reg_value);

	reg_value = nag_dltv_reg_value & 0xffff;

	nag_dltv_mv_value = reg_to_mv_value(nag_dltv_reg_value);
	*nag_dltv = nag_dltv_mv_value;

	bm_debug("[fg_bat_nafg][%s] mV:Reg [%d:%d] [%d:%d]\n",
		__func__, nag_dltv_mv_value, nag_dltv_reg_value,
		reg_to_mv_value(reg_value),
		reg_value);

	return 0;
}

static int nafg_c_dltv_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *nafg_c_dltv)
{
	signed int nag_c_dltv_value;
	signed int nag_c_dltv_value_h;
	signed int nag_c_dltv_reg_value;
	signed int nag_c_dltv_mv_value;
	bool bcheckbit10;

	/*AUXADC_NAG_7*/
	regmap_read(gauge->regmap, PMIC_AUXADC_NAG_C_DLTV_15_0_ADDR,
		&nag_c_dltv_value);

	/*AUXADC_NAG_8*/
	regmap_read(gauge->regmap, PMIC_AUXADC_NAG_C_DLTV_26_16_ADDR,
		&nag_c_dltv_value_h);
	nag_c_dltv_value_h = (nag_c_dltv_value_h &
		PMIC_AUXADC_NAG_C_DLTV_26_16_MASK);

	bcheckbit10 = nag_c_dltv_value_h & 0x0400;

	if (gauge->nafg_corner == 1) {
		nag_c_dltv_reg_value = (nag_c_dltv_value & 0x7fff);
		nag_c_dltv_mv_value = reg_to_mv_value(nag_c_dltv_reg_value);
		*nafg_c_dltv = nag_c_dltv_mv_value;

		bm_debug("[fg_bat_nafg][%s] mV:Reg[%d:%d] [b10:%d][26_16(0x%04x) 15_00(0x%04x)] corner:%d\n",
			__func__, nag_c_dltv_mv_value, nag_c_dltv_reg_value,
			bcheckbit10, nag_c_dltv_value_h, nag_c_dltv_value,
			gauge->nafg_corner);
		return 0;
	} else if (gauge->nafg_corner == 2) {
		nag_c_dltv_reg_value = (nag_c_dltv_value - 32768);
		nag_c_dltv_mv_value =
			reg_to_mv_value(nag_c_dltv_reg_value);
		*nafg_c_dltv = nag_c_dltv_mv_value;

		bm_debug("[fg_bat_nafg][%s] mV:Reg[%d:%d] [b10:%d][26_16(0x%04x) 15_00(0x%04x)] corner:%d\n",
			__func__, nag_c_dltv_mv_value, nag_c_dltv_reg_value,
			bcheckbit10, nag_c_dltv_value_h, nag_c_dltv_value,
			gauge->nafg_corner);
		return 0;
	}

	if (bcheckbit10 == 0)
		nag_c_dltv_reg_value = (nag_c_dltv_value & 0xffff) +
				((nag_c_dltv_value_h & 0x07ff) << 16);
	else
		nag_c_dltv_reg_value = (nag_c_dltv_value & 0xffff) +
			(((nag_c_dltv_value_h | 0xf800) & 0xffff) << 16);

	nag_c_dltv_mv_value = reg_to_mv_value(nag_c_dltv_reg_value);
	*nafg_c_dltv = nag_c_dltv_mv_value;

	bm_debug("[fg_bat_nafg][%s] mV:Reg[%d:%d] [b10:%d][26_16(0x%04x) 15_00(0x%04x)] corner:%d\n",
		__func__, nag_c_dltv_mv_value, nag_c_dltv_reg_value,
		bcheckbit10, nag_c_dltv_value_h, nag_c_dltv_value,
		gauge->nafg_corner);

	return 0;
}

static int zcv_get(struct mtk_gauge *gauge_dev,
	struct mtk_gauge_sysfs_field_info *attr, int *zcv)
{
	signed int adc_result_reg = 0;
	signed int adc_result = 0;

	regmap_read(gauge_dev->regmap,
		PMIC_AUXADC_ADC_OUT_FGADC_PCHR_ADDR,
		&adc_result_reg);
	adc_result_reg =
		(adc_result_reg & (PMIC_AUXADC_ADC_OUT_FGADC_PCHR_MASK
		<< PMIC_AUXADC_ADC_OUT_FGADC_PCHR_SHIFT))
		>> PMIC_AUXADC_ADC_OUT_FGADC_PCHR_SHIFT;

	adc_result = reg_to_mv_value(adc_result_reg);
	bm_err("[oam] %s BATSNS  (pchr):adc_result_reg=%d, adc_result=%d\n",
		 __func__, adc_result_reg, adc_result);

	*zcv = adc_result;
	return 0;
}

static int boot_zcv_get(struct mtk_gauge *gauge_dev,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	int _hw_ocv, _sw_ocv;
	int _hw_ocv_src;
	int _prev_hw_ocv, _prev_hw_ocv_src;
	int _hw_ocv_rdy;
	int _flag_unreliable;
	int _hw_ocv_57_pon;
	int _hw_ocv_57_plugin;
	int _hw_ocv_57_pon_rdy;
	int _hw_ocv_chgin;
	int _hw_ocv_chgin_rdy;
	int now_temp;
	int now_thr;
	bool fg_is_charger_exist;
	struct mtk_battery *gm;
	struct zcv_data *zcvinfo;
	struct gauge_hw_status *p;

	gm = gauge_dev->gm;
	p = &gauge_dev->hw_status;
	zcvinfo = &gauge_dev->zcv_info;
	_hw_ocv_57_pon_rdy = read_hw_ocv_6357_power_on_rdy(gauge_dev);
	_hw_ocv_57_pon = read_hw_ocv_6357_power_on(gauge_dev);
	_hw_ocv_57_plugin = read_hw_ocv_6357_plug_in(gauge_dev);

	/* todo:charger function is not ready to access charger zcv */
	/* _hw_ocv_chgin = battery_get_charger_zcv() / 100; */
	_hw_ocv_chgin = 0;
	now_temp = gm->bs_data.bat_batt_temp;

	if (gm == NULL)
		now_thr = 300;
	else {
		if (now_temp > gm->ext_hwocv_swocv_lt_temp)
			now_thr = gm->ext_hwocv_swocv;
		else
			now_thr = gm->ext_hwocv_swocv_lt;
	}

	if (_hw_ocv_chgin < 25000)
		_hw_ocv_chgin_rdy = 0;
	else
		_hw_ocv_chgin_rdy = 1;

	/* if preloader records charge in, need to using subpmic as hwocv */
	fgauge_get_info(
		gauge_dev, GAUGE_PROP_PL_CHARGING_STATUS,
		&zcvinfo->pl_charging_status);
	fgauge_set_info(
		gauge_dev, GAUGE_PROP_PL_CHARGING_STATUS, 0);
	fgauge_get_info(
		gauge_dev, GAUGE_PROP_MONITER_PLCHG_STATUS,
		&zcvinfo->moniter_plchg_bit);
	fgauge_set_info(
		gauge_dev, GAUGE_PROP_MONITER_PLCHG_STATUS, 0);

	if (zcvinfo->pl_charging_status == 1)
		fg_is_charger_exist = 1;
	else
		fg_is_charger_exist = 0;

	_hw_ocv = _hw_ocv_57_pon;
	_sw_ocv = gauge_dev->hw_status.sw_ocv;
	/* _sw_ocv = get_sw_ocv();*/
	_hw_ocv_src = FROM_PMIC_PON_ON;
	_prev_hw_ocv = _hw_ocv;
	_prev_hw_ocv_src = FROM_PMIC_PON_ON;
	_flag_unreliable = 0;

	if (fg_is_charger_exist) {
		_hw_ocv_rdy = _hw_ocv_57_pon_rdy;
		if (_hw_ocv_rdy == 1) {
			if (_hw_ocv_chgin_rdy == 1) {
				_hw_ocv = _hw_ocv_chgin;
				_hw_ocv_src = FROM_CHR_IN;
			} else {
				_hw_ocv = _hw_ocv_57_pon;
				_hw_ocv_src = FROM_PMIC_PON_ON;
			}

			if (abs(_hw_ocv - _sw_ocv) > now_thr) {
				_prev_hw_ocv = _hw_ocv;
				_prev_hw_ocv_src = _hw_ocv_src;
				_hw_ocv = _sw_ocv;
				_hw_ocv_src = FROM_SW_OCV;
				p->flag_hw_ocv_unreliable = true;
				_flag_unreliable = 1;
			}
		} else {
			/* fixme: swocv is workaround */
			/* plug charger poweron but charger not ready */
			/* should use swocv to workaround */
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
			if (_hw_ocv_chgin_rdy != 1) {
				if (abs(_hw_ocv - _sw_ocv) > now_thr) {
					_prev_hw_ocv = _hw_ocv;
					_prev_hw_ocv_src = _hw_ocv_src;
					_hw_ocv = _sw_ocv;
					_hw_ocv_src = FROM_SW_OCV;
					p->flag_hw_ocv_unreliable = true;
					_flag_unreliable = 1;
				}
			}
		}
	} else {
		if (_hw_ocv_57_pon_rdy == 0) {
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
		}
	}

	/* final chance to check hwocv */
	if (gm != NULL)
		if (_hw_ocv < 28000 && (gm->disableGM30 == 0)) {
			bm_err("[%s] ERROR, _hw_ocv=%d  src:%d, force use swocv\n",
			__func__, _hw_ocv, _hw_ocv_src);
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
		}

	*val = _hw_ocv;

	zcvinfo->charger_zcv = _hw_ocv_chgin;
	zcvinfo->pmic_rdy = _hw_ocv_57_pon_rdy;
	zcvinfo->pmic_zcv = _hw_ocv_57_pon;
	zcvinfo->pmic_in_zcv = _hw_ocv_57_plugin;
	zcvinfo->swocv = _sw_ocv;
	zcvinfo->zcv_from = _hw_ocv_src;
	zcvinfo->zcv_tmp = now_temp;

	if (zcvinfo->zcv_1st_read == false) {
		zcvinfo->charger_zcv_1st = zcvinfo->charger_zcv;
		zcvinfo->pmic_rdy_1st = zcvinfo->pmic_rdy;
		zcvinfo->pmic_zcv_1st = zcvinfo->pmic_zcv;
		zcvinfo->pmic_in_zcv_1st = zcvinfo->pmic_in_zcv;
		zcvinfo->swocv_1st = zcvinfo->swocv;
		zcvinfo->zcv_from_1st = zcvinfo->zcv_from;
		zcvinfo->zcv_tmp_1st = zcvinfo->zcv_tmp;
		zcvinfo->zcv_1st_read = true;
	}

	gauge_dev->fg_hw_info.pmic_zcv = _hw_ocv_57_pon;
	gauge_dev->fg_hw_info.pmic_zcv_rdy = _hw_ocv_57_pon_rdy;
	gauge_dev->fg_hw_info.charger_zcv = _hw_ocv_chgin;
	gauge_dev->fg_hw_info.hw_zcv = _hw_ocv;

	bm_err("[%s] g_fg_is_charger_exist %d _hw_ocv_chgin_rdy %d pl:%d %d\n",
		__func__, fg_is_charger_exist, _hw_ocv_chgin_rdy,
		zcvinfo->pl_charging_status, zcvinfo->moniter_plchg_bit);
	bm_err("[%s] _hw_ocv %d _sw_ocv %d now_thr %d\n",
		__func__, _prev_hw_ocv, _sw_ocv, now_thr);
	bm_err("[%s] _hw_ocv %d _hw_ocv_src %d _prev_hw_ocv %d _prev_hw_ocv_src %d _flag_unreliable %d\n",
		__func__, _hw_ocv, _hw_ocv_src, _prev_hw_ocv,
		_prev_hw_ocv_src, _flag_unreliable);
	bm_err("[%s] _hw_ocv_57_pon_rdy %d _hw_ocv_57_pon %d _hw_ocv_57_plugin %d _hw_ocv_chgin %d _sw_ocv %d now_temp %d now_thr %d\n",
		__func__, _hw_ocv_57_pon_rdy, _hw_ocv_57_pon,
		_hw_ocv_57_plugin, _hw_ocv_chgin, _sw_ocv,
		now_temp, now_thr);

	return 0;
}

static int initial_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	int bat_flag = 0;
	int is_charger_exist;
	int rev_val = 0;

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_NAG_PRD_ADDR,
		PMIC_AUXADC_NAG_PRD_MASK << PMIC_AUXADC_NAG_PRD_SHIFT,
		10 << PMIC_AUXADC_NAG_PRD_SHIFT);

	fgauge_get_info(gauge,
		GAUGE_PROP_BAT_PLUG_STATUS, &bat_flag);
	fgauge_get_info(gauge,
		GAUGE_PROP_PL_CHARGING_STATUS, &is_charger_exist);

	regmap_read(gauge->regmap, PMIC_RG_SYSTEM_INFO_CON0_ADDR, &rev_val);
	bm_err("bat_plug:%d chr:%d info:0x%x\n",
		bat_flag, is_charger_exist, rev_val);

	gauge->hw_status.pl_charger_status = is_charger_exist;

	if (is_charger_exist == 1) {
		gauge->hw_status.is_bat_plugout = 1;
		fgauge_set_info(gauge, GAUGE_PROP_2SEC_REBOOT, 0);
	} else {
		if (bat_flag == 0)
			gauge->hw_status.is_bat_plugout = 1;
		else
			gauge->hw_status.is_bat_plugout = 0;
	}

	fgauge_set_info(gauge, GAUGE_PROP_BAT_PLUG_STATUS, 1);
	/*[12:8], 5 bits*/
	gauge->hw_status.bat_plug_out_time = 31;

	fgauge_read_RTC_boot_status(gauge);

	return 1;
}

static int battery_current_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	*val = instant_current(gauge);

	return 0;
}

static int hw_version_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{

	*val = GAUGE_HW_V2000;

	return 0;
}

static int rtc_ui_soc_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	*val = get_rtc_spare_fg_value(gauge);

	return 0;
}

static int rtc_ui_soc_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	set_rtc_spare_fg_value(gauge, val);

	return 1;
}


static int gauge_initialized_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	return 0;
}

static int gauge_initialized_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	return 0;
}


static int battery_exist_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	unsigned int regval;

#if defined(CONFIG_FPGA_EARLY_PORTING)
	*val = 0;
	return 0;
#endif

	regmap_read(gauge->regmap, PMIC_RGS_BATON_UNDET_ADDR, &regval);
	regval =
		(regval & (PMIC_RGS_BATON_UNDET_MASK
		<< PMIC_RGS_BATON_UNDET_SHIFT))
		>> PMIC_RGS_BATON_UNDET_SHIFT;

	if (regval == 0)
		*val = 1;
	else {
		*val = 0;
		regmap_update_bits(gauge->regmap,
			PMIC_AUXADC_ADC_RDY_PWRON_CLR_ADDR,
			PMIC_AUXADC_ADC_RDY_PWRON_CLR_MASK
			<< PMIC_AUXADC_ADC_RDY_PWRON_CLR_SHIFT,
			1 << PMIC_AUXADC_ADC_RDY_PWRON_CLR_SHIFT);
		mdelay(1);
		regmap_update_bits(gauge->regmap,
			PMIC_AUXADC_ADC_RDY_PWRON_CLR_ADDR,
			PMIC_AUXADC_ADC_RDY_PWRON_CLR_MASK
			<< PMIC_AUXADC_ADC_RDY_PWRON_CLR_SHIFT,
			0 << PMIC_AUXADC_ADC_RDY_PWRON_CLR_SHIFT);
	}

	return 0;
}

static int bat_vol_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	int ret;

	if (!IS_ERR(gauge->chan_bat_voltage)) {
		ret = iio_read_channel_processed(gauge->chan_bat_voltage, val);
		if (ret < 0)
			bm_err("[%s]read fail,ret=%d\n", __func__, ret);
	} else {
		bm_err("[%s]chan error\n", __func__);
		ret = -ENOTSUPP;
	}

	return ret;
}

static int battery_temperature_adc_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	int ret;

	if (!IS_ERR(gauge->chan_bat_temp)) {
		ret = iio_read_channel_processed(gauge->chan_bat_temp, val);
		if (ret < 0)
			bm_err("[%s]read fail,ret=%d\n", __func__, ret);
	} else {
		bm_err("[%s]chan error\n", __func__);
		ret = -ENOTSUPP;
	}

	return ret;
}

static int bif_voltage_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	int ret;

	if (!IS_ERR(gauge->chan_bif)) {
		ret = iio_read_channel_processed(gauge->chan_bif, val);
		if (ret < 0)
			bm_err("[%s]read fail,ret=%d\n", __func__, ret);
	} else {
		bm_err("[%s]chan error\n", __func__);
		ret = -ENOTSUPP;
	}

	return ret;
}

static int ptim_battery_voltage_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	int ret;

	if (!IS_ERR(gauge->chan_ptim_bat_voltage)) {
		ret = iio_read_channel_processed(
			gauge->chan_ptim_bat_voltage, val);
		if (ret < 0)
			bm_err("[%s]read fail,ret=%d\n", __func__, ret);
	} else {
		bm_err("[%s]chan error\n", __func__);
		ret = -ENOTSUPP;
	}

	return ret;
}

static int ptim_resist_get(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int *val)
{
	int ret;

	if (!IS_ERR(gauge->chan_ptim_r)) {
		ret = iio_read_channel_processed(
			gauge->chan_ptim_r, val);
		if (ret < 0)
			bm_err("[%s]read fail,ret=%d\n", __func__, ret);
	} else {
		bm_err("[%s]chan error\n", __func__);
		ret = -ENOTSUPP;
	}

	return ret;
}

static int coulomb_interrupt_ht_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	unsigned int temp_car_15_0 = 0;
	unsigned int temp_car_31_16 = 0;
	unsigned int uvalue32_car_msb = 0;
	signed int upperbound = 0;
	signed int upperbound_31_16 = 0, upperbound_15_00 = 0;
	signed int value32_car;
	long long car = val;
	int r_fg_value;
	int car_tune_value;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->hw_status.car_tune_value;
	bm_debug("%s car=%d\n", __func__, val);
	if (car == 0) {
		disable_gauge_irq(gauge, COULOMB_H_IRQ);
		return 0;
	}

	pre_gauge_update(gauge);

	regmap_read(gauge->regmap, PMIC_FG_CAR_15_00_ADDR, &temp_car_15_0);
	temp_car_15_0 =
		(temp_car_15_0 &
		(PMIC_FG_CAR_15_00_MASK << PMIC_FG_CAR_15_00_SHIFT))
		>> PMIC_FG_CAR_15_00_SHIFT;

	regmap_read(gauge->regmap, PMIC_FG_CAR_31_16_ADDR, &temp_car_31_16);
	temp_car_31_16 =
		(temp_car_31_16 &
		(PMIC_FG_CAR_31_16_MASK << PMIC_FG_CAR_31_16_SHIFT))
		>> PMIC_FG_CAR_31_16_SHIFT;

	post_gauge_update(gauge);

	uvalue32_car_msb = (temp_car_31_16 & 0x8000) >> 15;
	value32_car = temp_car_15_0 & 0xffff;
	value32_car |= (temp_car_31_16 & 0xffff) << 16;

	bm_debug("[%s] FG_CAR = 0x%x:%d uvalue32_car_msb:0x%x 0x%x 0x%x\r\n",
		__func__, value32_car, value32_car, uvalue32_car_msb,
		temp_car_15_0,
		temp_car_31_16);

#if defined(__LP64__) || defined(_LP64)
	car = car * CAR_TO_REG_FACTOR / 10;
#else
	car = div_s64(car * CAR_TO_REG_FACTOR, 10);
#endif

	if (r_fg_value != 100)
#if defined(__LP64__) || defined(_LP64)
		car = (car * r_fg_value) /
			100;
#else
		car = div_s64(car * r_fg_value,
			100);
#endif

#if defined(__LP64__) || defined(_LP64)
	car = ((car * 1000) / car_tune_value);
#else
	car = div_s64((car * 1000), car_tune_value);
#endif

	upperbound = value32_car >> CAR_TO_REG_SHIFT;

	bm_debug("[%s] upper = 0x%x:%d diff_car=0x%llx:%lld, shift:%d\r\n",
		 __func__, upperbound, upperbound, car, car, CAR_TO_REG_SHIFT);

	upperbound = upperbound + car;

	upperbound_31_16 = (upperbound & 0xffff0000) >> 16;
	upperbound_15_00 = (upperbound & 0xffff);

	bm_debug("[%s] final upper = 0x%x:%d car=0x%llx:%lld\r\n",
		 __func__, upperbound, upperbound, car, car);

	bm_debug("[%s] final upper 0x%x 0x%x 0x%x car=0x%llx\n",
		 __func__,
		upperbound, upperbound_31_16, upperbound_15_00, car);

	disable_gauge_irq(gauge, COULOMB_H_IRQ);

	regmap_update_bits(gauge->regmap,
		PMIC_FG_BAT0_HTH_15_00_ADDR,
		PMIC_FG_BAT0_HTH_15_00_MASK << PMIC_FG_BAT0_HTH_15_00_SHIFT,
		upperbound_15_00 << PMIC_FG_BAT0_HTH_15_00_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_FG_BAT0_HTH_31_16_ADDR,
		PMIC_FG_BAT0_HTH_31_16_MASK << PMIC_FG_BAT0_HTH_31_16_SHIFT,
		upperbound_31_16 << PMIC_FG_BAT0_HTH_31_16_SHIFT);
	mdelay(1);

	enable_gauge_irq(gauge, COULOMB_H_IRQ);

	bm_debug("[%s] high:0x%x 0x%x car_value:%d car:%d irq:%d\r\n",
		__func__,
		upperbound_15_00,
		upperbound_31_16,
		val, value32_car,
		gauge->irq_no[COULOMB_H_IRQ]);

	return 0;
}

static int coulomb_interrupt_lt_set(struct mtk_gauge *gauge,
	 struct mtk_gauge_sysfs_field_info *attr, int val)
{
	unsigned int temp_car_15_0 = 0;
	unsigned int temp_car_31_16 = 0;
	unsigned int uvalue32_car_msb = 0;
	signed int lowbound = 0;
	signed int lowbound_31_16 = 0, lowbound_15_00 = 0;
	signed int value32_car;
	long long car = val;
	int r_fg_value;
	int car_tune_value;

	r_fg_value = gauge->hw_status.r_fg_value;
	car_tune_value = gauge->hw_status.car_tune_value;
	bm_debug("%s car=%d\n", __func__, val);
	if (car == 0) {
		disable_gauge_irq(gauge, COULOMB_L_IRQ);
		return 0;
	}

	pre_gauge_update(gauge);

	regmap_read(gauge->regmap, PMIC_FG_CAR_15_00_ADDR, &temp_car_15_0);
	temp_car_15_0 =
		(temp_car_15_0 &
		(PMIC_FG_CAR_15_00_MASK << PMIC_FG_CAR_15_00_SHIFT))
		>> PMIC_FG_CAR_15_00_SHIFT;

	regmap_read(gauge->regmap, PMIC_FG_CAR_31_16_ADDR, &temp_car_31_16);
	temp_car_31_16 =
		(temp_car_31_16 &
		(PMIC_FG_CAR_31_16_MASK << PMIC_FG_CAR_31_16_SHIFT))
		>> PMIC_FG_CAR_31_16_SHIFT;

	post_gauge_update(gauge);

	uvalue32_car_msb =
		(temp_car_31_16 & 0x8000) >> 15;
	value32_car = temp_car_15_0 & 0xffff;
	value32_car |= (temp_car_31_16 & 0xffff) << 16;

	bm_debug("[%s] FG_CAR = 0x%x:%d uvalue32_car_msb:0x%x 0x%x 0x%x\r\n",
		__func__,
		value32_car, value32_car, uvalue32_car_msb,
		temp_car_15_0,
		temp_car_31_16);

	/* gap to register-base */
#if defined(__LP64__) || defined(_LP64)
	car = car * CAR_TO_REG_FACTOR / 10;
#else
	car = div_s64(car * CAR_TO_REG_FACTOR, 10);
#endif

	if (r_fg_value != 100)
#if defined(__LP64__) || defined(_LP64)
		car = (car * r_fg_value) /
			100;
#else
		car = div_s64(car * r_fg_value,
			100);
#endif

#if defined(__LP64__) || defined(_LP64)
	car = ((car * 1000) / car_tune_value);
#else
	car = div_s64((car * 1000), car_tune_value);
#endif

	lowbound = value32_car >> CAR_TO_REG_SHIFT;

	bm_debug("[%s]low=0x%x:%d diff_car=0x%llx:%lld\r\n",
		 __func__, lowbound, lowbound, car, car);

	lowbound = lowbound - car;

	lowbound_31_16 = (lowbound & 0xffff0000) >> 16;
	lowbound_15_00 = (lowbound & 0xffff);

	bm_debug("[%s]final low=0x%x:%d car=0x%llx:%lld\r\n",
		 __func__, lowbound, lowbound, car, car);

	bm_debug("[%s] final low 0x%x 0x%x 0x%x car=0x%llx\n",
		 __func__, lowbound, lowbound_31_16, lowbound_15_00, car);

	disable_gauge_irq(gauge, COULOMB_L_IRQ);
	regmap_update_bits(gauge->regmap,
		PMIC_FG_BAT0_LTH_15_00_ADDR,
		PMIC_FG_BAT0_LTH_15_00_MASK << PMIC_FG_BAT0_LTH_15_00_SHIFT,
		lowbound_15_00 << PMIC_FG_BAT0_LTH_15_00_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_FG_BAT0_LTH_31_16_ADDR,
		PMIC_FG_BAT0_LTH_31_16_MASK << PMIC_FG_BAT0_LTH_31_16_SHIFT,
		lowbound_31_16 << PMIC_FG_BAT0_LTH_31_16_SHIFT);
	mdelay(1);
	enable_gauge_irq(gauge, COULOMB_L_IRQ);

	bm_debug("[%s] low:0x%x 0x%x car_value:%d car:%d irq:%d\r\n",
		__func__, lowbound_15_00,
		lowbound_31_16,
		val, value32_car,
		gauge->irq_no[COULOMB_L_IRQ]);

	return 0;
}


static int en_h_vbat_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	return 0;
}

static int en_l_vbat_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{

	return 0;
}

static int vbat_ht_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	return 0;
}

static int reset_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	unsigned int ret = 0;

	bm_err("[fgauge_hw_reset]\n");
	regmap_update_bits(gauge->regmap,
		MT6357_FGADC_CON1,
		0x0F00, 0x0630);
	bm_err("[fgauge_hw_reset] reset fgadc car ret =%d\n", ret);
	mdelay(1);
	regmap_update_bits(gauge->regmap,
		MT6357_FGADC_CON1,
		0x0F00, 0x0030);
	return 0;
}

static int vbat_lt_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	return 0;
}

void dump_nag(struct mtk_gauge *gauge)
{
	int nag[12];

	regmap_read(gauge->regmap, 0x11be, &nag[0]);
	regmap_read(gauge->regmap, 0x11c0, &nag[1]);
	regmap_read(gauge->regmap, 0x11c2, &nag[2]);
	regmap_read(gauge->regmap, 0x11c4, &nag[3]);
	regmap_read(gauge->regmap, 0x11c6, &nag[4]);
	regmap_read(gauge->regmap, 0x11c8, &nag[5]);
	regmap_read(gauge->regmap, 0x11ca, &nag[6]);
	regmap_read(gauge->regmap, 0x11cc, &nag[7]);
	regmap_read(gauge->regmap, 0x11ce, &nag[8]);
	regmap_read(gauge->regmap, 0x11d0, &nag[9]);
	regmap_read(gauge->regmap, 0x11d2, &nag[10]);
	regmap_read(gauge->regmap, 0x11d4, &nag[11]);

	bm_err("nag %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
		nag[0], nag[1], nag[2], nag[3], nag[4], nag[5],
		nag[6], nag[7], nag[8], nag[9], nag[10], nag[11],
		reg_to_mv_value(nag[1]),
		reg_to_mv_value(nag[2]),
		reg_to_mv_value(nag[6])
		);
}

static ssize_t gauge_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct mtk_gauge *gauge;
	struct mtk_gauge_sysfs_field_info *gauge_attr;
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);

	gauge_attr = container_of(attr,
		struct mtk_gauge_sysfs_field_info, attr);
	if (gauge_attr->set != NULL) {
		mutex_lock(&gauge->ops_lock);
		gauge_attr->set(gauge, gauge_attr, val);
		mutex_unlock(&gauge->ops_lock);
	}

	return count;
}

static ssize_t gauge_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct mtk_gauge *gauge;
	struct mtk_gauge_sysfs_field_info *gauge_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);

	gauge_attr = container_of(attr,
		struct mtk_gauge_sysfs_field_info, attr);
	if (gauge_attr->get != NULL) {
		mutex_lock(&gauge->ops_lock);
		gauge_attr->get(gauge, gauge_attr, &val);
		mutex_unlock(&gauge->ops_lock);
	}

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

/* Must be in the same order as GAUGE_PROP_* */
static struct mtk_gauge_sysfs_field_info mt6357_sysfs_field_tbl[] = {
	GAUGE_SYSFS_FIELD_WO(initial,
			GAUGE_PROP_INITIAL),
	GAUGE_SYSFS_FIELD_RO(battery_current,
		GAUGE_PROP_BATTERY_CURRENT),
	GAUGE_SYSFS_FIELD_RO(coulomb,
		GAUGE_PROP_COULOMB),
	GAUGE_SYSFS_FIELD_WO(coulomb_interrupt_ht,
		GAUGE_PROP_COULOMB_HT_INTERRUPT),
	GAUGE_SYSFS_FIELD_WO(coulomb_interrupt_lt,
		GAUGE_PROP_COULOMB_LT_INTERRUPT),
	GAUGE_SYSFS_FIELD_RO(battery_exist,
		GAUGE_PROP_BATTERY_EXIST),
	GAUGE_SYSFS_FIELD_RO(hw_version,
		GAUGE_PROP_HW_VERSION),
	GAUGE_SYSFS_FIELD_RO(bat_vol,
		GAUGE_PROP_BATTERY_VOLTAGE),
	GAUGE_SYSFS_FIELD_RO(battery_temperature_adc,
		GAUGE_PROP_BATTERY_TEMPERATURE_ADC),
	GAUGE_SYSFS_FIELD_RO(bif_voltage,
		GAUGE_PROP_BIF_VOLTAGE),
	GAUGE_SYSFS_FIELD_WO(en_h_vbat,
		GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT),
	GAUGE_SYSFS_FIELD_WO(en_l_vbat,
		GAUGE_PROP_EN_LOW_VBAT_INTERRUPT),
	GAUGE_SYSFS_FIELD_WO(vbat_ht,
		GAUGE_PROP_VBAT_HT_INTR_THRESHOLD),
	GAUGE_SYSFS_FIELD_WO(vbat_lt,
		GAUGE_PROP_VBAT_LT_INTR_THRESHOLD),
	GAUGE_SYSFS_FIELD_RW(rtc_ui_soc,
		GAUGE_PROP_RTC_UI_SOC),
	GAUGE_SYSFS_FIELD_RO(ptim_battery_voltage,
		GAUGE_PROP_PTIM_BATTERY_VOLTAGE),
	GAUGE_SYSFS_FIELD_RO(ptim_resist,
		GAUGE_PROP_PTIM_RESIST),
	GAUGE_SYSFS_FIELD_WO(reset,
		GAUGE_PROP_RESET),
	GAUGE_SYSFS_FIELD_RO(boot_zcv,
		GAUGE_PROP_BOOT_ZCV),
	GAUGE_SYSFS_FIELD_RO(zcv,
		GAUGE_PROP_ZCV),
	GAUGE_SYSFS_FIELD_RO(zcv_current,
		GAUGE_PROP_ZCV_CURRENT),
	GAUGE_SYSFS_FIELD_RO(nafg_cnt,
		GAUGE_PROP_NAFG_CNT),
	GAUGE_SYSFS_FIELD_RO(nafg_dltv,
		GAUGE_PROP_NAFG_DLTV),
	GAUGE_SYSFS_FIELD_RW(nafg_c_dltv,
		GAUGE_PROP_NAFG_C_DLTV),
	GAUGE_SYSFS_FIELD_WO(nafg_en,
		GAUGE_PROP_NAFG_EN),
	GAUGE_SYSFS_FIELD_WO(nafg_zcv,
		GAUGE_PROP_NAFG_ZCV),
	GAUGE_SYSFS_FIELD_RO(nafg_vbat,
		GAUGE_PROP_NAFG_VBAT),
	GAUGE_SYSFS_FIELD_WO(reset_fg_rtc,
		GAUGE_PROP_RESET_FG_RTC),
	GAUGE_SYSFS_FIELD_RW(gauge_initialized,
		GAUGE_PROP_GAUGE_INITIALIZED),
	GAUGE_SYSFS_FIELD_RO(average_current,
		GAUGE_PROP_AVERAGE_CURRENT),
	GAUGE_SYSFS_FIELD_WO(bat_plugout_en,
		GAUGE_PROP_BAT_PLUGOUT_EN),
	GAUGE_SYSFS_FIELD_WO(zcv_intr_threshold,
		GAUGE_PROP_ZCV_INTR_THRESHOLD),
	GAUGE_SYSFS_FIELD_WO(zcv_intr_en,
		GAUGE_PROP_ZCV_INTR_EN),
	GAUGE_SYSFS_FIELD_WO(soff_reset,
		GAUGE_PROP_SOFF_RESET),
	GAUGE_SYSFS_FIELD_WO(ncar_reset,
		GAUGE_PROP_NCAR_RESET),
	GAUGE_SYSFS_FIELD_WO(bat_cycle_intr_threshold,
		GAUGE_PROP_BAT_CYCLE_INTR_THRESHOLD),
	GAUGE_SYSFS_FIELD_WO(hw_info,
		GAUGE_PROP_HW_INFO),
	GAUGE_SYSFS_FIELD_WO(event,
		GAUGE_PROP_EVENT),
	GAUGE_SYSFS_FIELD_WO(en_bat_tmp_ht,
		GAUGE_PROP_EN_BAT_TMP_HT),
	GAUGE_SYSFS_FIELD_WO(en_bat_tmp_lt,
		GAUGE_PROP_EN_BAT_TMP_LT),
	GAUGE_SYSFS_FIELD_WO(bat_tmp_ht_threshold,
		GAUGE_PROP_BAT_TMP_HT_THRESHOLD),
	GAUGE_SYSFS_FIELD_WO(bat_tmp_lt_threshold,
		GAUGE_PROP_BAT_TMP_LT_THRESHOLD),
	GAUGE_SYSFS_INFO_FIELD_RW(
		info_2sec_reboot,
		GAUGE_PROP_2SEC_REBOOT),
	GAUGE_SYSFS_INFO_FIELD_RW(
		info_pl_charging_status,
		GAUGE_PROP_PL_CHARGING_STATUS),
	GAUGE_SYSFS_INFO_FIELD_RW(
		info_monitor_plchg_status,
		GAUGE_PROP_MONITER_PLCHG_STATUS),
	GAUGE_SYSFS_INFO_FIELD_RW(
		info_bat_plug_status,
		GAUGE_PROP_BAT_PLUG_STATUS),
	GAUGE_SYSFS_INFO_FIELD_RW(
		info_is_nvram_fail_mode,
		GAUGE_PROP_IS_NVRAM_FAIL_MODE),
	GAUGE_SYSFS_INFO_FIELD_RW(
		info_monitor_soff_validtime,
		GAUGE_PROP_MONITOR_SOFF_VALIDTIME),
	GAUGE_SYSFS_INFO_FIELD_RW(
		info_con0_soc, GAUGE_PROP_CON0_SOC),
	GAUGE_SYSFS_INFO_FIELD_RW(
		info_shutdown_car, GAUGE_PROP_SHUTDOWN_CAR),
	GAUGE_SYSFS_INFO_FIELD_RW(
		car_tune_value, GAUGE_PROP_CAR_TUNE_VALUE),
	GAUGE_SYSFS_INFO_FIELD_RW(
		r_fg_value, GAUGE_PROP_R_FG_VALUE),
	GAUGE_SYSFS_INFO_FIELD_RW(
		vbat2_detect_time, GAUGE_PROP_VBAT2_DETECT_TIME),
	GAUGE_SYSFS_INFO_FIELD_RW(
		vbat2_detect_counter, GAUGE_PROP_VBAT2_DETECT_COUNTER),
};

static struct attribute *
	mt6357_sysfs_attrs[ARRAY_SIZE(mt6357_sysfs_field_tbl) + 1];

static const struct attribute_group mt6357_sysfs_attr_group = {
	.attrs = mt6357_sysfs_attrs,
};

static void mt6357_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(mt6357_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		mt6357_sysfs_attrs[i] = &mt6357_sysfs_field_tbl[i].attr.attr;

	mt6357_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int mt6357_sysfs_create_group(struct mtk_gauge *gauge)
{
	mt6357_sysfs_init_attrs();

	return sysfs_create_group(&gauge->psy->dev.kobj,
			&mt6357_sysfs_attr_group);
}

static void mt6357_gauge_shutdown(struct platform_device *pdev)
{
	struct mtk_battery *gm;
	struct mtk_gauge *gauge;


	gauge = dev_get_drvdata(&pdev->dev);
	gm = gauge->gm;

	if (gm->shutdown != NULL)
		gm->shutdown(gm);
}

static int mt6357_gauge_suspend(struct platform_device *pdev,
	pm_message_t state)
{
	struct mtk_battery *gm;
	struct mtk_gauge *gauge;

	gauge = dev_get_drvdata(&pdev->dev);
	gm = gauge->gm;

	if (gm->suspend != NULL)
		gm->suspend(gm, state);

	return 0;
}

static int mt6357_gauge_resume(struct platform_device *pdev)
{
	struct mtk_battery *gm;
	struct mtk_gauge *gauge;

	gauge = dev_get_drvdata(&pdev->dev);
	gm = gauge->gm;

	if (gm->resume != NULL)
		gm->resume(gm);

	return 0;
}

static int mt6357_gauge_probe(struct platform_device *pdev)
{
	struct mtk_gauge *gauge;
	int ret;
	struct iio_channel *chan_bat_temp;

	bm_err("%s: starts\n", __func__);

	chan_bat_temp = devm_iio_channel_get(
		&pdev->dev, "pmic_battery_temp");
	if (IS_ERR(chan_bat_temp)) {
		bm_err("%s requests probe deferral\n", __func__);
		return -EPROBE_DEFER;
	}

	gauge = devm_kzalloc(&pdev->dev, sizeof(*gauge), GFP_KERNEL);
	if (!gauge)
		return -ENOMEM;

	gauge->chip = (struct mt6397_chip *)dev_get_drvdata(
		pdev->dev.parent);
	gauge->regmap = gauge->chip->regmap;
	dev_set_drvdata(&pdev->dev, gauge);
	gauge->pdev = pdev;
	mutex_init(&gauge->ops_lock);

	gauge->irq_no[COULOMB_H_IRQ] =
		platform_get_irq_byname(pdev, "COULOMB_H");
	gauge->irq_no[COULOMB_L_IRQ] =
		platform_get_irq_byname(pdev, "COULOMB_L");
	gauge->irq_no[NAFG_IRQ] = platform_get_irq_byname(pdev, "NAFG");
	gauge->irq_no[ZCV_IRQ] = platform_get_irq_byname(pdev, "ZCV");

	gauge->chan_bat_temp = devm_iio_channel_get(
		&pdev->dev, "pmic_battery_temp");
	if (IS_ERR(gauge->chan_bat_temp)) {
		ret = PTR_ERR(gauge->chan_bat_temp);
		bm_err("pmic_battery_temp auxadc get fail, ret=%d\n", ret);
	}

	gauge->chan_bat_voltage = devm_iio_channel_get(
		&pdev->dev, "pmic_battery_voltage");
	if (IS_ERR(gauge->chan_bat_voltage)) {
		ret = PTR_ERR(gauge->chan_bat_voltage);
		bm_err("chan_bat_voltage auxadc get fail, ret=%d\n", ret);
	}

	gauge->chan_bif = devm_iio_channel_get(
		&pdev->dev, "pmic_bif_voltage");
	if (IS_ERR(gauge->chan_bif)) {
		ret = PTR_ERR(gauge->chan_bif);
		bm_err("pmic_bif_voltage auxadc get fail, ret=%d\n", ret);
	}

	gauge->chan_ptim_bat_voltage = devm_iio_channel_get(
		&pdev->dev, "pmic_ptim_voltage");
	if (IS_ERR(gauge->chan_ptim_bat_voltage)) {
		ret = PTR_ERR(gauge->chan_ptim_bat_voltage);
		bm_err("chan_ptim_bat_voltage auxadc get fail, ret=%d\n",
			ret);
	}

	gauge->chan_ptim_r = devm_iio_channel_get(
		&pdev->dev, "pmic_ptim_r");
	if (IS_ERR(gauge->chan_ptim_r)) {
		ret = PTR_ERR(gauge->chan_ptim_r);
		bm_err("chan_ptim_r auxadc get fail, ret=%d\n",
			ret);
	}

	gauge->hw_status.car_tune_value = 1000;
	gauge->hw_status.r_fg_value = 50;

	if (battery_psy_init(pdev))
		return -ENOMEM;

	gauge->psy_desc.name = "mtk-gauge";
	gauge->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	gauge->psy_desc.properties = gauge_properties;
	gauge->psy_desc.num_properties = ARRAY_SIZE(gauge_properties);
	gauge->psy_desc.get_property = psy_gauge_get_property;
	gauge->psy_cfg.drv_data = gauge;
	gauge->psy = power_supply_register(&pdev->dev, &gauge->psy_desc,
			&gauge->psy_cfg);
	gauge->attr = mt6357_sysfs_field_tbl;
	mt6357_sysfs_create_group(gauge);

	battery_init(pdev);

	bm_err("%s: done\n", __func__);

	return 0;
}

static const struct of_device_id mt6357_gauge_of_match[] = {
	{.compatible = "mediatek,mt6357-gauge",},
	{},
};

static int mt6357_gauge_remove(struct platform_device *pdev)
{
	struct mtk_gauge *gauge = platform_get_drvdata(pdev);

	if (gauge)
		devm_kfree(&pdev->dev, gauge);
	return 0;
}

MODULE_DEVICE_TABLE(of, mt6357_gauge_of_match);

static struct platform_driver mt6357_gauge_driver = {
	.probe = mt6357_gauge_probe,
	.remove = mt6357_gauge_remove,
	.shutdown = mt6357_gauge_shutdown,
	.suspend = mt6357_gauge_suspend,
	.resume = mt6357_gauge_resume,
	.driver = {
		.name = "mt6357_gauge",
		.of_match_table = mt6357_gauge_of_match,
		},
};

static int __init mt6357_gauge_init(void)
{
	return platform_driver_register(&mt6357_gauge_driver);
}
module_init(mt6357_gauge_init);

static void __exit mt6357_gauge_exit(void)
{
	platform_driver_unregister(&mt6357_gauge_driver);
}
module_exit(mt6357_gauge_exit);

MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Gauge Device Driver");
MODULE_LICENSE("GPL");

