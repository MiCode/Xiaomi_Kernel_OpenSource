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
#include "mtk_gauge.h"

/* ============================================================ */
/* pmic control start*/
/* ============================================================ */
#define PMIC_HWCID_ADDR		0x8
#define PMIC_HWCID_MASK		0xFFFF
#define PMIC_HWCID_SHIFT	0

#define PMIC_AUXADC_NAG_PRD_SEL_ADDR	0x11be
#define PMIC_AUXADC_NAG_PRD_SEL_MASK	0x3
#define PMIC_AUXADC_NAG_PRD_SEL_SHIFT	3

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

#define PMIC_FG_CAR_15_00_ADDR	0xd16
#define PMIC_FG_CAR_15_00_MASK	0xFFFF
#define PMIC_FG_CAR_15_00_SHIFT	0

#define PMIC_FG_CAR_31_16_ADDR	0xd18
#define PMIC_FG_CAR_31_16_MASK	0xFFFF
#define PMIC_FG_CAR_31_16_SHIFT	0

#define PMIC_FG_BAT_HTH_15_00_ADDR	0xd20
#define PMIC_FG_BAT_HTH_15_00_MASK	0xFFFF
#define PMIC_FG_BAT_HTH_15_00_SHIFT	0

#define PMIC_FG_BAT_HTH_31_16_ADDR	0xd22
#define PMIC_FG_BAT_HTH_31_16_MASK	0xFFFF
#define PMIC_FG_BAT_HTH_31_16_SHIFT	0

#define PMIC_FG_BAT_LTH_15_00_ADDR	0xd1c
#define PMIC_FG_BAT_LTH_15_00_MASK	0xFFFF
#define PMIC_FG_BAT_LTH_15_00_SHIFT	0

#define PMIC_FG_BAT_LTH_31_16_ADDR	0xd1e
#define PMIC_FG_BAT_LTH_31_16_MASK	0xFFFF
#define PMIC_FG_BAT_LTH_31_16_SHIFT	0

#define PMIC_AD_BATON_UNDET_ADDR	0xe0a
#define PMIC_AD_BATON_UNDET_MASK	0x1
#define PMIC_AD_BATON_UNDET_SHIFT	1

#define PMIC_AUXADC_ADC_RDY_PWRON_CLR_ADDR	0x11b2
#define PMIC_AUXADC_ADC_RDY_PWRON_CLR_MASK	0x1
#define PMIC_AUXADC_ADC_RDY_PWRON_CLR_SHIFT	3

#define PMIC_AUXADC_LBAT2_IRQ_EN_MIN_ADDR	0x1240
#define PMIC_AUXADC_LBAT2_IRQ_EN_MIN_MASK	0x1
#define PMIC_AUXADC_LBAT2_IRQ_EN_MIN_SHIFT	12

#define PMIC_AUXADC_LBAT2_DET_MIN_ADDR	0x1240
#define PMIC_AUXADC_LBAT2_DET_MIN_MASK	0x1
#define PMIC_AUXADC_LBAT2_DET_MIN_SHIFT	13

#define PMIC_AUXADC_LBAT2_IRQ_EN_MAX_ADDR	0x123e
#define PMIC_AUXADC_LBAT2_IRQ_EN_MAX_MASK	0x1
#define PMIC_AUXADC_LBAT2_IRQ_EN_MAX_SHIFT	12

#define PMIC_AUXADC_LBAT2_DET_MAX_ADDR	0x123e
#define PMIC_AUXADC_LBAT2_DET_MAX_MASK	0x1
#define PMIC_AUXADC_LBAT2_DET_MAX_SHIFT	13

#define PMIC_AUXADC_LBAT2_EN_ADDR	0x123a
#define PMIC_AUXADC_LBAT2_EN_MASK	0x1
#define PMIC_AUXADC_LBAT2_EN_SHIFT	0

#define PMIC_AUXADC_LBAT2_VOLT_MIN_ADDR	0x1240
#define PMIC_AUXADC_LBAT2_VOLT_MIN_MASK	0xFFF
#define PMIC_AUXADC_LBAT2_VOLT_MIN_SHIFT	0

#define PMIC_AUXADC_LBAT2_DET_PRD_SEL_ADDR	0x123c
#define PMIC_AUXADC_LBAT2_DET_PRD_SEL_MASK	0x3
#define PMIC_AUXADC_LBAT2_DET_PRD_SEL_SHIFT	0

#define PMIC_AUXADC_LBAT2_DEBT_MIN_SEL_ADDR	0x123c
#define PMIC_AUXADC_LBAT2_DEBT_MIN_SEL_MASK	0x3
#define PMIC_AUXADC_LBAT2_DEBT_MIN_SEL_SHIFT	4

#define PMIC_AUXADC_LBAT2_VOLT_MAX_ADDR	0x123e
#define PMIC_AUXADC_LBAT2_VOLT_MAX_MASK	0xFFF
#define PMIC_AUXADC_LBAT2_VOLT_MAX_SHIFT	0

#define PMIC_AUXADC_LBAT2_DET_PRD_SEL_ADDR	0x123c
#define PMIC_AUXADC_LBAT2_DET_PRD_SEL_MASK	0x3
#define PMIC_AUXADC_LBAT2_DET_PRD_SEL_SHIFT	0

#define PMIC_AUXADC_LBAT2_DEBT_MAX_SEL_ADDR		0x123c
#define PMIC_AUXADC_LBAT2_DEBT_MAX_SEL_MASK		0x3
#define PMIC_AUXADC_LBAT2_DEBT_MAX_SEL_SHIFT	2

#define PMIC_RG_SYSTEM_INFO_CON0_ADDR 0xd9a

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
			dev_notice(&gauge->pdev->dev,
			"[%s] gauge_update_polling timeout 1!\r\n",
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
			dev_notice(&gauge->pdev->dev, "[%s] gauge_update_polling timeout 2!\r\n",
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

#define UNIT_FGCURRENT			(610352)
/* mt6359 610.352 uA */
#define UNIT_CHARGE				(85)
/* CHARGE_LSB 0.085 uAh*/

/* AUXADC */
#define R_VAL_TEMP_2			(25)
#define R_VAL_TEMP_3			(35)


#define UNIT_TIME				(50)
#define UNIT_FG_IAVG			(305176)
/* IAVG LSB: 305.176 uA */
#define DEFAULT_R_FG			(50)
/* 5mm ohm */
#define UNIT_FGCAR_ZCV			(85)
/* CHARGE_LSB = 0.085 uAh */

#define VOLTAGE_FULL_RANGES		1800
#define ADC_PRECISE				32768	/* 15 bits */


#define CAR_TO_REG_SHIFT		(5)
/*coulomb interrupt lsb might be different with coulomb lsb */
#define CAR_TO_REG_FACTOR		(0x2E14)
/* 1000 * 1000 / CHARGE_LSB */
#define UNIT_FGCAR				(174080)
/* CHARGE_LSB 0.085 * 2^11 */

static int mv_to_reg_12_value(struct mtk_gauge *gauge,
	signed int _reg)
{
	int ret = (_reg * 4096) / (VOLTAGE_FULL_RANGES * R_VAL_TEMP_3);

	dev_info(&gauge->pdev->dev,
		"[%s] %d => %d\n", __func__, _reg, ret);
	return ret;
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

	dev_info(&gauge->pdev->dev,
		"[%s] 0x%x 0x%x 0x%x 0x%x 0x%x %d\n",
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
	u32 *buf;

	cell = nvmem_cell_get(&gauge->pdev->dev, "initialization");
	if (IS_ERR(cell)) {
		dev_notice(&gauge->pdev->dev,
			"[%s]get rtc cell fail\n", __func__);
		return 0;
	}

	buf = nvmem_cell_read(cell, NULL);
	nvmem_cell_put(cell);

	if (IS_ERR(buf)) {
		dev_notice(&gauge->pdev->dev,
			"[%s]read rtc cell fail\n", __func__);
		return 0;
	}

	dev_notice(&gauge->pdev->dev,
		"[%s] val=%d\n", __func__, *buf);

	return *buf;
}

void set_rtc_spare0_fg_value(struct mtk_gauge *gauge, u8 val)
{
	struct nvmem_cell *cell;
	u32 length = 1;
	int ret;

	cell = nvmem_cell_get(&gauge->pdev->dev, "initialization");
	if (IS_ERR(cell)) {
		dev_notice(&gauge->pdev->dev,
			"[%s]get rtc cell fail\n", __func__);
		return;
	}

	ret = nvmem_cell_write(cell, &val, length);
	nvmem_cell_put(cell);

	if (ret != length)
		dev_notice(&gauge->pdev->dev,
		"[%s] write rtc cell fail\n", __func__);

}

int get_rtc_spare_fg_value(struct mtk_gauge *gauge)
{
	struct nvmem_cell *cell;
	u32 *buf;

	cell = nvmem_cell_get(&gauge->pdev->dev, "state-of-charge");
	if (IS_ERR(cell)) {
		dev_notice(&gauge->pdev->dev,
			"[%s]get rtc cell fail\n", __func__);
		return 0;
	}

	buf = nvmem_cell_read(cell, NULL);
	nvmem_cell_put(cell);

	if (IS_ERR(buf)) {
		dev_notice(&gauge->pdev->dev,
			"[%s]read rtc cell fail\n", __func__);
		return 0;
	}

	dev_info(&gauge->pdev->dev,
		"[%s] val=%d\n", __func__, *buf);

	return *buf;
}

void set_rtc_spare_fg_value(struct mtk_gauge *gauge, u8 val)
{
	struct nvmem_cell *cell;
	u32 length = 1;
	int ret;

	cell = nvmem_cell_get(&gauge->pdev->dev, "state-of-charge");
	if (IS_ERR(cell)) {
		dev_notice(&gauge->pdev->dev,
			"[%s]get rtc cell fail\n", __func__);
		return;
	}

	ret = nvmem_cell_write(cell, &val, length);
	nvmem_cell_put(cell);

	if (ret != length)
		dev_notice(&gauge->pdev->dev,
			"[%s] write rtc cell fail\n", __func__);

	dev_info(&gauge->pdev->dev,
		"[%s] val=%d\n", __func__, val);

}

static int fgauge_set_info(struct mtk_gauge *gauge,
	enum gauge_property ginfo, unsigned int value)
{

	dev_info(&gauge->pdev->dev,
		"[%s]info:%d v:%d\n", __func__, ginfo, value);

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

	dev_info(&gauge->pdev->dev,
		"[%s]info:%d v:%d\n", __func__, ginfo, *value);

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

	dev_info(&gauge->pdev->dev,
		"[%s]current:%d\n", __func__, dvalue);

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

	dev_notice(&gauge->pdev->dev,
	"[%s] rtc_invalid %d plugout %d plugout_time %d spare3 0x%x spare0 0x%x hw_id 0x%x\n",
			__func__,
			gauge->hw_status.rtc_invalid,
			gauge->hw_status.is_bat_plugout,
			gauge->hw_status.bat_plug_out_time,
			spare3_reg, spare0_reg, hw_id);
}

static int initial_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	int bat_flag = 0;
	int is_charger_exist;
	int rev_val = 0;

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_NAG_PRD_SEL_ADDR,
		PMIC_AUXADC_NAG_PRD_SEL_MASK << PMIC_AUXADC_NAG_PRD_SEL_SHIFT,
		2 << PMIC_AUXADC_NAG_PRD_SEL_SHIFT);

	fgauge_get_info(gauge,
		GAUGE_PROP_BAT_PLUG_STATUS, &bat_flag);
	fgauge_get_info(gauge,
		GAUGE_PROP_PL_CHARGING_STATUS, &is_charger_exist);

	regmap_read(gauge->regmap, PMIC_RG_SYSTEM_INFO_CON0_ADDR, &rev_val);
	dev_notice(&gauge->pdev->dev,
		"bat_plug:%d chr:%d info:0x%x\n",
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

	*val = dvalue;

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


	dev_notice(&gauge->pdev->dev,
		"[%s]l:0x%x h:0x%x val:%d msb:%d car:%d\n",
		__func__,
		temp_car_15_0, temp_car_31_16,
		uvalue32_car, uvalue32_car_msb,
		dvalue_CAR);

/*Auto adjust value*/
	if (r_fg_value != DEFAULT_R_FG) {
		dev_info(&gauge->pdev->dev,
			 "[%s] Auto adjust value deu to the Rfg is %d\n Ori CAR=%d",
			 __func__,
			 r_fg_value, dvalue_CAR);

		dvalue_CAR = (dvalue_CAR * DEFAULT_R_FG) /
			r_fg_value;

		dev_info(&gauge->pdev->dev,
			"[%s] new CAR=%d\n",
			__func__,
			dvalue_CAR);
	}

	dvalue_CAR = ((dvalue_CAR *
		car_tune_value) / 1000);

	dev_notice(&gauge->pdev->dev,
		"[%s] CAR=%d r_fg_value=%d car_tune_value=%d\n",
		__func__,
		dvalue_CAR, r_fg_value,
		car_tune_value);

	*val = dvalue_CAR;

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

	regmap_read(gauge->regmap, PMIC_AD_BATON_UNDET_ADDR, &regval);
	regval =
		(regval & (PMIC_AD_BATON_UNDET_MASK
		<< PMIC_AD_BATON_UNDET_SHIFT))
		>> PMIC_AD_BATON_UNDET_SHIFT;


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
			dev_notice(&gauge->pdev->dev,
				"[%s]read fail,ret=%d\n", __func__, ret);
	} else {
		dev_notice(&gauge->pdev->dev,
			"[%s]chan error\n", __func__);
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
			dev_notice(&gauge->pdev->dev,
				"[%s]read fail,ret=%d\n", __func__, ret);
	} else {
		dev_notice(&gauge->pdev->dev,
			"[%s]chan error\n", __func__);
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
			dev_notice(&gauge->pdev->dev,
				"[%s]read fail,ret=%d\n", __func__, ret);
	} else {
		dev_notice(&gauge->pdev->dev,
			"[%s]chan error\n", __func__);
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
			dev_notice(&gauge->pdev->dev,
				"[%s]read fail,ret=%d\n", __func__, ret);
	} else {
		dev_notice(&gauge->pdev->dev,
			"[%s]chan error\n", __func__);
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
	dev_notice(&gauge->pdev->dev,
		"%s car=%d\n", __func__, val);
	if (car == 0) {
		disable_irq_nosync(gauge->coulomb_h_irq);
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

	dev_notice(&gauge->pdev->dev,
		"[%s] FG_CAR = 0x%x:%d uvalue32_car_msb:0x%x 0x%x 0x%x\r\n",
		__func__, value32_car, value32_car, uvalue32_car_msb,
		temp_car_15_0,
		temp_car_31_16);

#if defined(__LP64__) || defined(_LP64)
	car = car * 100000 / UNIT_CHARGE;
	/* 1000 * 100 */
#else
	car = div_s64(car * 100000, UNIT_CHARGE);
#endif

	if (r_fg_value != DEFAULT_R_FG)
#if defined(__LP64__) || defined(_LP64)
		car = (car * r_fg_value) /
			DEFAULT_R_FG;
#else
		car = div_s64(car * r_fg_value,
			DEFAULT_R_FG);
#endif

#if defined(__LP64__) || defined(_LP64)
	car = ((car * 1000) / car_tune_value);
#else
	car = div_s64((car * 1000), car_tune_value);
#endif

	upperbound = value32_car;

	dev_info(&gauge->pdev->dev,
		"[%s] upper = 0x%x:%d diff_car=0x%llx:%lld\r\n",
		 __func__, upperbound, upperbound, car, car);

	upperbound = upperbound + car;

	upperbound_31_16 = (upperbound & 0xffff0000) >> 16;
	upperbound_15_00 = (upperbound & 0xffff);


	dev_info(&gauge->pdev->dev,
		"[%s] final upper = 0x%x:%d car=0x%llx:%lld\r\n",
		 __func__, upperbound, upperbound, car, car);

	dev_info(&gauge->pdev->dev,
		"[%s] final upper 0x%x 0x%x 0x%x car=0x%llx\n",
		 __func__,
		upperbound, upperbound_31_16, upperbound_15_00, car);

	disable_irq_nosync(gauge->coulomb_h_irq);

	regmap_update_bits(gauge->regmap,
		PMIC_FG_BAT_HTH_15_00_ADDR,
		PMIC_FG_BAT_HTH_15_00_MASK << PMIC_FG_BAT_HTH_15_00_SHIFT,
		upperbound_15_00 << PMIC_FG_BAT_HTH_15_00_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_FG_BAT_HTH_31_16_ADDR,
		PMIC_FG_BAT_HTH_31_16_MASK << PMIC_FG_BAT_HTH_31_16_SHIFT,
		upperbound_31_16 << PMIC_FG_BAT_HTH_31_16_SHIFT);
	mdelay(1);

	enable_irq(gauge->coulomb_h_irq);

	dev_info(&gauge->pdev->dev,
		"[%s] high:0x%x 0x%x car_value:%d car:%d irq:%d\r\n",
		__func__,
		upperbound_15_00,
		upperbound_31_16,
		val, value32_car,
		gauge->coulomb_h_irq);

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
	dev_notice(&gauge->pdev->dev,
		"%s car=%d\n", __func__, val);
	if (car == 0) {
		disable_irq_nosync(gauge->coulomb_l_irq);
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



	dev_info(&gauge->pdev->dev,
		"[%s] FG_CAR = 0x%x:%d uvalue32_car_msb:0x%x 0x%x 0x%x\r\n",
		__func__,
		value32_car, value32_car, uvalue32_car_msb,
		temp_car_15_0,
		temp_car_31_16);

	/* gap to register-base */
#if defined(__LP64__) || defined(_LP64)
	car = car * 100000 / UNIT_CHARGE;
	/* car * 1000 * 100 */
#else
	car = div_s64(car * 100000, UNIT_CHARGE);
#endif

	if (r_fg_value != DEFAULT_R_FG)
#if defined(__LP64__) || defined(_LP64)
		car = (car * r_fg_value) /
			DEFAULT_R_FG;
#else
		car = div_s64(car * r_fg_value,
			DEFAULT_R_FG);
#endif

#if defined(__LP64__) || defined(_LP64)
	car = ((car * 1000) / car_tune_value);
#else
	car = div_s64((car * 1000), car_tune_value);
#endif

	lowbound = value32_car;

	dev_info(&gauge->pdev->dev,
		"[%s]low=0x%x:%d diff_car=0x%llx:%lld\r\n",
		 __func__, lowbound, lowbound, car, car);

	lowbound = lowbound - car;

	lowbound_31_16 = (lowbound & 0xffff0000) >> 16;
	lowbound_15_00 = (lowbound & 0xffff);

	dev_info(&gauge->pdev->dev,
		"[%s]final low=0x%x:%d car=0x%llx:%lld\r\n",
		 __func__, lowbound, lowbound, car, car);

	dev_info(&gauge->pdev->dev,
		"[%s] final low 0x%x 0x%x 0x%x car=0x%llx\n",
		 __func__, lowbound, lowbound_31_16, lowbound_15_00, car);

	disable_irq_nosync(gauge->coulomb_l_irq);
	regmap_update_bits(gauge->regmap,
		PMIC_FG_BAT_LTH_15_00_ADDR,
		PMIC_FG_BAT_LTH_15_00_MASK << PMIC_FG_BAT_LTH_15_00_SHIFT,
		lowbound_15_00 << PMIC_FG_BAT_LTH_15_00_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_FG_BAT_LTH_31_16_ADDR,
		PMIC_FG_BAT_LTH_31_16_MASK << PMIC_FG_BAT_LTH_31_16_SHIFT,
		lowbound_31_16 << PMIC_FG_BAT_LTH_31_16_SHIFT);
	mdelay(1);
	enable_irq(gauge->coulomb_l_irq);

	dev_info(&gauge->pdev->dev,
		"[%s] low:0x%x 0x%x car_value:%d car:%d irq:%d\r\n",
		__func__, lowbound_15_00,
		lowbound_31_16,
		val, value32_car,
		gauge->coulomb_l_irq);

	return 0;
}


static void enable_lbat2_en(struct mtk_gauge *gauge)
{
	if (gauge->vbat_l_en == true || gauge->vbat_h_en == true)
		regmap_update_bits(gauge->regmap,
			PMIC_AUXADC_LBAT2_EN_ADDR,
			PMIC_AUXADC_LBAT2_EN_MASK << PMIC_AUXADC_LBAT2_EN_SHIFT,
			1 << PMIC_AUXADC_LBAT2_EN_SHIFT);

	if (gauge->vbat_l_en == false && gauge->vbat_h_en == false)
		regmap_update_bits(gauge->regmap,
			PMIC_AUXADC_LBAT2_EN_ADDR,
			PMIC_AUXADC_LBAT2_EN_MASK << PMIC_AUXADC_LBAT2_EN_SHIFT,
			0 << PMIC_AUXADC_LBAT2_EN_SHIFT);
}

static int en_h_vbat_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	if (val != 0) {
		val = 1;
		enable_irq(gauge->vbat_h_irq);
	} else
		disable_irq_nosync(gauge->vbat_h_irq);


	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_LBAT2_IRQ_EN_MAX_ADDR,
		PMIC_AUXADC_LBAT2_IRQ_EN_MAX_MASK
		<< PMIC_AUXADC_LBAT2_IRQ_EN_MAX_SHIFT,
		val << PMIC_AUXADC_LBAT2_IRQ_EN_MAX_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_LBAT2_DET_MAX_ADDR,
		PMIC_AUXADC_LBAT2_DET_MAX_MASK
		<< PMIC_AUXADC_LBAT2_DET_MAX_SHIFT,
		val << PMIC_AUXADC_LBAT2_DET_MAX_SHIFT);

	gauge->vbat_h_en = val;
	enable_lbat2_en(gauge);

	return 0;
}

static int en_l_vbat_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int val)
{
	if (val != 0) {
		val = 1;
		enable_irq(gauge->vbat_l_irq);
	} else
		disable_irq_nosync(gauge->vbat_l_irq);


	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_LBAT2_IRQ_EN_MIN_ADDR,
		PMIC_AUXADC_LBAT2_IRQ_EN_MIN_MASK
		<< PMIC_AUXADC_LBAT2_IRQ_EN_MIN_SHIFT,
		val << PMIC_AUXADC_LBAT2_IRQ_EN_MIN_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_LBAT2_DET_MIN_ADDR,
		PMIC_AUXADC_LBAT2_DET_MIN_MASK
		<< PMIC_AUXADC_LBAT2_IRQ_EN_MIN_SHIFT,
		val << PMIC_AUXADC_LBAT2_IRQ_EN_MIN_SHIFT);

	gauge->vbat_l_en = val;
	enable_lbat2_en(gauge);

	return 0;
}

static void switch_vbat2_det_time(int _prd, int *value)
{
	if (_prd >= 1 && _prd < 3)
		*value = 0;
	else if (_prd >= 3 && _prd < 5)
		*value = 1;
	else if (_prd >= 5 && _prd < 10)
		*value = 2;
	else if (_prd >= 10)
		*value = 3;
}

static void switch_vbat2_debt_counter(int _prd, int *value)
{
	if (_prd >= 1 && _prd < 2)
		*value = 0;
	else if (_prd >= 2 && _prd < 4)
		*value = 1;
	else if (_prd >= 4 && _prd < 8)
		*value = 2;
	else if (_prd >= 8)
		*value = 3;
}

static int vbat_ht_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	int vbat2_h_th_mv =  threshold;
	int vbat2_h_th_reg = mv_to_reg_12_value(gauge, vbat2_h_th_mv);
	int vbat2_det_counter = 0;
	int vbat2_det_time = 0;

	switch_vbat2_det_time(
		gauge->hw_status.vbat2_det_time,
		&vbat2_det_time);

	switch_vbat2_debt_counter(
		gauge->hw_status.vbat2_det_counter,
		&vbat2_det_counter);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_LBAT2_VOLT_MAX_ADDR,
		PMIC_AUXADC_LBAT2_VOLT_MAX_MASK
		<< PMIC_AUXADC_LBAT2_VOLT_MAX_SHIFT,
		vbat2_h_th_reg << PMIC_AUXADC_LBAT2_VOLT_MAX_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_LBAT2_DET_PRD_SEL_ADDR,
		PMIC_AUXADC_LBAT2_DET_PRD_SEL_MASK
		<< PMIC_AUXADC_LBAT2_DET_PRD_SEL_SHIFT,
		vbat2_det_time << PMIC_AUXADC_LBAT2_DET_PRD_SEL_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_LBAT2_DEBT_MAX_SEL_ADDR,
		PMIC_AUXADC_LBAT2_DEBT_MAX_SEL_MASK
		<< PMIC_AUXADC_LBAT2_DEBT_MAX_SEL_SHIFT,
		vbat2_det_counter << PMIC_AUXADC_LBAT2_DEBT_MAX_SEL_SHIFT);

	dev_info(&gauge->pdev->dev,
		"[fg_set_vbat2_h_th] thr:%d [0x%x %d 0x%x %d 0x%x]\n",
		threshold, vbat2_h_th_reg,
		gauge->hw_status.vbat2_det_time, vbat2_det_time,
		gauge->hw_status.vbat2_det_counter, vbat2_det_counter);

	return 0;
}


static int vbat_lt_set(struct mtk_gauge *gauge,
	struct mtk_gauge_sysfs_field_info *attr, int threshold)
{
	int vbat2_l_th_mv =  threshold;
	int vbat2_l_th_reg = mv_to_reg_12_value(gauge, vbat2_l_th_mv);
	int vbat2_det_counter = 0;
	int vbat2_det_time = 0;

	switch_vbat2_det_time(
		gauge->hw_status.vbat2_det_time,
		&vbat2_det_time);

	switch_vbat2_debt_counter(
		gauge->hw_status.vbat2_det_counter,
		&vbat2_det_counter);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_LBAT2_VOLT_MIN_ADDR,
		PMIC_AUXADC_LBAT2_VOLT_MIN_MASK
		<< PMIC_AUXADC_LBAT2_VOLT_MIN_SHIFT,
		vbat2_l_th_reg << PMIC_AUXADC_LBAT2_VOLT_MIN_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_LBAT2_DET_PRD_SEL_ADDR,
		PMIC_AUXADC_LBAT2_DET_PRD_SEL_MASK
		<< PMIC_AUXADC_LBAT2_DET_PRD_SEL_SHIFT,
		vbat2_det_time << PMIC_AUXADC_LBAT2_DET_PRD_SEL_SHIFT);

	regmap_update_bits(gauge->regmap,
		PMIC_AUXADC_LBAT2_DEBT_MIN_SEL_ADDR,
		PMIC_AUXADC_LBAT2_DEBT_MIN_SEL_MASK
		<< PMIC_AUXADC_LBAT2_DEBT_MIN_SEL_SHIFT,
		vbat2_det_counter << PMIC_AUXADC_LBAT2_DEBT_MIN_SEL_SHIFT);

	dev_info(&gauge->pdev->dev,
		"[fg_set_vbat2_l_th] thr:%d [0x%x %d 0x%x %d 0x%x]\n",
		threshold,
		vbat2_l_th_reg,
		gauge->hw_status.vbat2_det_time, vbat2_det_time,
		gauge->hw_status.vbat2_det_counter, vbat2_det_counter);

	return 0;
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
static struct mtk_gauge_sysfs_field_info mt6359_sysfs_field_tbl[] = {
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
		GAUGE_PROP_VBAT_HT_INTERRUPT),
	GAUGE_SYSFS_FIELD_WO(vbat_lt,
		GAUGE_PROP_VBAT_LT_INTERRUPT),
	GAUGE_SYSFS_FIELD_RW(rtc_ui_soc,
		GAUGE_PROP_RTC_UI_SOC),
	GAUGE_SYSFS_FIELD_RO(ptim_battery_voltage,
		GAUGE_PROP_PTIM_BATTERY_VOLTAGE),

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
	mt6359_sysfs_attrs[ARRAY_SIZE(mt6359_sysfs_field_tbl) + 1];

static const struct attribute_group mt6359_sysfs_attr_group = {
	.attrs = mt6359_sysfs_attrs,
};

static void mt6359_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(mt6359_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		mt6359_sysfs_attrs[i] = &mt6359_sysfs_field_tbl[i].attr.attr;

	mt6359_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int mt6359_sysfs_create_group(struct mtk_gauge *gauge)
{
	mt6359_sysfs_init_attrs();

	return sysfs_create_group(&gauge->psy->dev.kobj,
			&mt6359_sysfs_attr_group);
}

static int mt6359_gauge_probe(struct platform_device *pdev)
{
	struct mtk_gauge *gauge;
	int ret;
	struct iio_channel *chan_bat_temp;

	pr_notice("%s: starts\n", __func__);

	chan_bat_temp = devm_iio_channel_get(
		&pdev->dev, "pmic_battery_temp");
	if (IS_ERR(chan_bat_temp)) {
		pr_notice("mt6359 requests probe deferral\n");
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

	gauge->psy_desc.name = "mtk-gauge";
	gauge->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	gauge->psy_desc.properties = gauge_properties;
	gauge->psy_desc.num_properties = ARRAY_SIZE(gauge_properties);
	gauge->psy_desc.get_property = psy_gauge_get_property;
	gauge->psy_cfg.drv_data = gauge;

	gauge->psy = power_supply_register(&pdev->dev, &gauge->psy_desc,
			&gauge->psy_cfg);

	gauge->attr = mt6359_sysfs_field_tbl;

	gauge->coulomb_h_irq = platform_get_irq_byname(pdev, "COULOMB_H");
	gauge->coulomb_l_irq = platform_get_irq_byname(pdev, "COULOMB_L");
	gauge->vbat_h_irq = platform_get_irq_byname(pdev, "VBAT_H");
	gauge->vbat_l_irq = platform_get_irq_byname(pdev, "VBAT_L");

	gauge->chan_bat_temp = devm_iio_channel_get(
		&pdev->dev, "pmic_battery_temp");
	if (IS_ERR(gauge->chan_bat_temp)) {
		ret = PTR_ERR(gauge->chan_bat_temp);
		pr_notice("pmic_battery_temp auxadc get fail, ret=%d\n", ret);
	}

	gauge->chan_bat_voltage = devm_iio_channel_get(
		&pdev->dev, "pmic_battery_voltage");
	if (IS_ERR(gauge->chan_bat_voltage)) {
		ret = PTR_ERR(gauge->chan_bat_voltage);
		pr_notice("chan_bat_voltage auxadc get fail, ret=%d\n", ret);
	}

	gauge->chan_bif = devm_iio_channel_get(
		&pdev->dev, "pmic_bif_voltage");
	if (IS_ERR(gauge->chan_bif)) {
		ret = PTR_ERR(gauge->chan_bif);
		pr_notice("pmic_bif_voltage auxadc get fail, ret=%d\n", ret);
	}

	gauge->chan_ptim_bat_voltage = devm_iio_channel_get(
		&pdev->dev, "pmic_ptim_voltage");
	if (IS_ERR(gauge->chan_ptim_bat_voltage)) {
		ret = PTR_ERR(gauge->chan_ptim_bat_voltage);
		pr_notice("chan_ptim_bat_voltage auxadc get fail, ret=%d\n",
			ret);
	}

	gauge->hw_status.car_tune_value = 1000;
	gauge->hw_status.r_fg_value = 50;

	mt6359_sysfs_create_group(gauge);
	pr_notice("%s: done\n", __func__);

	return 0;
}

static const struct of_device_id mt6359_gauge_of_match[] = {
	{.compatible = "mediatek,mt6359-gauge",},
	{},
};

static int mt6359_gauge_remove(struct platform_device *pdev)
{
	struct mtk_gauge *gauge = platform_get_drvdata(pdev);

	if (gauge)
		devm_kfree(&pdev->dev, gauge);
	return 0;
}

MODULE_DEVICE_TABLE(of, mt6359_gauge_of_match);

static struct platform_driver mt6359_gauge_driver = {
	.probe = mt6359_gauge_probe,
	.remove = mt6359_gauge_remove,
	.driver = {
		.name = "mt6359_gauge",
		.of_match_table = mt6359_gauge_of_match,
		},
};

static int __init mt6359_gauge_init(void)
{
	return platform_driver_register(&mt6359_gauge_driver);
}
module_init(mt6359_gauge_init);

static void __exit mt6359_gauge_exit(void)
{
	platform_driver_unregister(&mt6359_gauge_driver);
}
module_exit(mt6359_gauge_exit);

MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Gauge Device Driver");
MODULE_LICENSE("GPL");

