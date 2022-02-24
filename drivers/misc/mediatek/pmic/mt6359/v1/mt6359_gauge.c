/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/


#include <linux/delay.h>
#include <linux/time.h>

#include <asm/div64.h>

#include <mt-plat/upmu_common.h>

#include <mach/mtk_battery_property.h>
#include <mach/mtk_pmic.h>
#include <mt-plat/v1/mtk_battery.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_rtc_hal_common.h>
#include <mt-plat/mtk_rtc.h>
#include "include/pmic_throttling_dlpt.h"
#include <linux/proc_fs.h>
#include <linux/math64.h>
#include <linux/of.h>

#include <mtk_gauge_class.h>
#include <mtk_battery_internal.h>
#include <mt-plat/mtk_auxadc_intf.h>
#include "aee.h"

/*********************** MT6359 setting *********************/
#define UNIT_FGCURRENT     (610352)
/* mt6359 610.352 uA */
#define UNIT_CHARGE	(85)
/* CHARGE_LSB 0.085 uAh*/

/* AUXADC */
#define R_VAL_TEMP_2         (25)
#define R_VAL_TEMP_3         (35)


#define UNIT_TIME          (50)
#define UNIT_FG_IAVG		(305176)
/* IAVG LSB: 305.176 uA */
#define DEFAULT_R_FG (50)
/* 5mm ohm */
#define UNIT_FGCAR_ZCV     (85)
/* CHARGE_LSB = 0.085 uAh */

#define VOLTAGE_FULL_RANGES    1800
#define ADC_PRECISE           32768	/* 15 bits */


#define CAR_TO_REG_SHIFT (5)
/*coulomb interrupt lsb might be different with coulomb lsb */
#define CAR_TO_REG_FACTOR  (0x2E14)
/* 1000 * 1000 / CHARGE_LSB */
#define UNIT_FGCAR         (174080)
/* CHARGE_LSB 0.085 * 2^11 */

static signed int g_hw_ocv_tune_value;
static bool g_fg_is_charger_exist;
static bool gvbat2_low_en;
static bool gvbat2_high_en;
static int g_nag_corner;
static int g_fg_zcv_det_iv;

struct mt6359_gauge {
	const char *gauge_dev_name;
	struct gauge_device *gauge_dev;
	struct gauge_properties gauge_prop;
	struct alarm zcv_timer;
};


enum {
	FROM_SW_OCV = 1,
	FROM_6359_PLUG_IN,
	FROM_6359_PON_ON,
	FROM_6360_CHR_IN
};


int MV_to_REG_12_value(signed int _reg)
{
	int ret = (_reg * 4096) / (VOLTAGE_FULL_RANGES * R_VAL_TEMP_3);

	bm_trace("[%s] %d => %d\n", __func__, _reg, ret);
	return ret;
}

static int MV_to_REG_12_temp_value(signed int _reg)
{
	int ret = (_reg * 4096) / (VOLTAGE_FULL_RANGES * R_VAL_TEMP_2);

	bm_trace("[%s] %d => %d\n", __func__, _reg, ret);
	return ret;
}

static signed int REG_to_MV_value(signed int _reg)
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

	bm_trace("[%s] %lld => %d\n",
		__func__, _reg64, ret);
	return ret;
}

static signed int MV_to_REG_value(signed int _mv)
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

	bm_trace("[%s] mv=%d,%lld => %d,\n", __func__, _mv, _reg64, ret);
	return ret;
}

static int fgauge_set_info(
	struct gauge_device *gauge_dev,
	enum gauge_info ginfo, int value)
{
	int ret = 0;

	if (ginfo == GAUGE_2SEC_REBOOT)
		pmic_config_interface(
		PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x0);
	else if (ginfo == GAUGE_PL_CHARGING_STATUS)
		pmic_config_interface(
		PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x1);
	else if (ginfo == GAUGE_MONITER_PLCHG_STATUS)
		pmic_config_interface(
		PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x2);
	else if (ginfo == GAUGE_BAT_PLUG_STATUS)
		pmic_config_interface(
		PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x3);
	else if (ginfo == GAUGE_IS_NVRAM_FAIL_MODE)
		pmic_config_interface(
		PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x4);
	else if (ginfo == GAUGE_MONITOR_SOFF_VALIDTIME)
		pmic_config_interface(
		PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x5);
	else if (ginfo == GAUGE_CON0_SOC) {
		value = value / 100;
		pmic_config_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x007F, 0x9);
	} else
		ret = -1;

	return 0;
}

static int fgauge_get_info(
	struct gauge_device *gauge_dev,
	enum gauge_info ginfo,
	int *value)
{
	int ret = 0;

	if (ginfo == GAUGE_2SEC_REBOOT)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x0);
	else if (ginfo == GAUGE_PL_CHARGING_STATUS)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x1);
	else if (ginfo == GAUGE_MONITER_PLCHG_STATUS)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x2);
	else if (ginfo == GAUGE_BAT_PLUG_STATUS)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x3);
	else if (ginfo == GAUGE_IS_NVRAM_FAIL_MODE)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x4);
	else if (ginfo == GAUGE_MONITOR_SOFF_VALIDTIME)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x0001, 0x5);
	else if (ginfo == GAUGE_CON0_SOC)
		pmic_read_interface(
			PMIC_RG_SYSTEM_INFO_CON0_ADDR, value, 0x007F, 0x9);
	else
		ret = -1;

	return 0;
}

static unsigned int fg_get_data_ready_status(void)
{
	unsigned int ret = 0;
	unsigned int temp_val = 0;

	ret = pmic_read_interface(
			PMIC_FG_LATCHDATA_ST_ADDR, &temp_val, 0xFFFF, 0x0);

	temp_val =
	(temp_val & (PMIC_FG_LATCHDATA_ST_MASK << PMIC_FG_LATCHDATA_ST_SHIFT))
	>> PMIC_FG_LATCHDATA_ST_SHIFT;

	return temp_val;
}

void read_fg_hw_info_current_1(struct gauge_device *gauge_dev)
{
	long long fg_current_1_reg;
	signed int dvalue;
	long long Temp_Value;
	int sign_bit = 0;

	fg_current_1_reg = pmic_get_register_value(PMIC_FG_CURRENT_OUT);

	/*calculate the real world data    */
	dvalue = (unsigned int) fg_current_1_reg;
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


	if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG)
		dvalue = (dvalue * DEFAULT_R_FG /
			gauge_dev->fg_cust_data->r_fg_value);

	if (sign_bit == 1)
		dvalue = dvalue - (dvalue * 2);

	gauge_dev->fg_hw_info.current_1 =
		((dvalue * gauge_dev->fg_cust_data->car_tune_value) / 1000);

}

void read_fg_hw_info_current_2(struct gauge_device *gauge_dev)
{
	long long fg_current_2_reg;
	signed int dvalue;
	long long Temp_Value;
	int sign_bit = 0;

	fg_current_2_reg = pmic_get_register_value(PMIC_FG_CIC2);

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


	if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG)
		dvalue = (dvalue * DEFAULT_R_FG) /
			gauge_dev->fg_cust_data->r_fg_value;

	if (sign_bit == 1)
		dvalue = dvalue - (dvalue * 2);

	gauge_dev->fg_hw_info.current_2 =
		((dvalue * gauge_dev->fg_cust_data->car_tune_value) / 1000);

}


static void read_fg_hw_info_Iavg(
	struct gauge_device *gauge_dev,
	int *is_iavg_valid)
{
	long long fg_iavg_reg = 0;
	long long fg_iavg_reg_tmp = 0;
	long long fg_iavg_ma = 0;
	int fg_iavg_reg_27_16 = 0;
	int fg_iavg_reg_15_00 = 0;
	int sign_bit = 0;
	int is_bat_charging;
	int valid_bit;

	valid_bit = pmic_get_register_value(PMIC_FG_IAVG_VLD);
	*is_iavg_valid = valid_bit;

	if (valid_bit == 1) {
		fg_iavg_reg_27_16 =
		pmic_get_register_value(PMIC_FG_IAVG_27_16);

		fg_iavg_reg_15_00 =
		pmic_get_register_value(PMIC_FG_IAVG_15_00);

		fg_iavg_reg = fg_iavg_reg_27_16;

		fg_iavg_reg =
		((long long)fg_iavg_reg << 16) + fg_iavg_reg_15_00;

		sign_bit = (fg_iavg_reg_27_16 & 0x800) >> 11;

		if (sign_bit) {
			fg_iavg_reg_tmp = fg_iavg_reg;
			fg_iavg_reg = 0xfffffff - fg_iavg_reg_tmp + 1;
		}

		if (sign_bit)
			is_bat_charging = 0;	/* discharge */
		else
			is_bat_charging = 1;	/* charge */

		fg_iavg_ma = fg_iavg_reg * UNIT_FG_IAVG *
			gauge_dev->fg_cust_data->car_tune_value;

#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 1000000);
		/* LSB UNIT_FG_IAVG and cartune */
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 1000000);
#endif


		if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG) {
#if defined(__LP64__) || defined(_LP64)
			fg_iavg_ma = (fg_iavg_ma * DEFAULT_R_FG /
				gauge_dev->fg_cust_data->r_fg_value);
#else
			fg_iavg_ma = div_s64(fg_iavg_ma * DEFAULT_R_FG,
				gauge_dev->fg_cust_data->r_fg_value);
#endif
		}


#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 100);	/* change to 0.1mA */
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 100);
#endif

		if (sign_bit == 1)
			fg_iavg_ma = 0 - fg_iavg_ma;

		gauge_dev->fg_hw_info.current_avg = fg_iavg_ma;
		gauge_dev->fg_hw_info.current_avg_sign = sign_bit;
	} else {
		read_fg_hw_info_current_1(gauge_dev);

		gauge_dev->fg_hw_info.current_avg =
			gauge_dev->fg_hw_info.current_1;

		is_bat_charging = 0;	/* discharge */
	}

	bm_debug(
		"[%s] fg_iavg_reg 0x%llx fg_iavg_reg_tmp 0x%llx 27_16 0x%x 15_00 0x%x\n",
			__func__, fg_iavg_reg, fg_iavg_reg_tmp,
			fg_iavg_reg_27_16, fg_iavg_reg_15_00);
	bm_debug(
		"[%s] is_bat_charging %d fg_iavg_ma 0x%llx\n",
			__func__, is_bat_charging, fg_iavg_ma);


}

static signed int fg_get_current_iavg(
	struct gauge_device *gauge_dev,
	int *data)
{
	long long fg_iavg_reg = 0;
	long long fg_iavg_reg_tmp = 0;
	long long fg_iavg_ma = 0;
	int fg_iavg_reg_27_16 = 0;
	int fg_iavg_reg_15_00 = 0;
	int sign_bit = 0;
	int is_bat_charging;
	int m;

	/* Set Read Latchdata */
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 1);

	m = 0;
		while (fg_get_data_ready_status() == 0) {
			m++;
			if (m > 1000) {
				bm_err(
				"[%s] fg_get_data_ready_status timeout1!\r\n",
					__func__);
				break;
			}
		}

	if (pmic_get_register_value(PMIC_FG_IAVG_VLD) == 1) {
		fg_iavg_reg_27_16 =
		pmic_get_register_value(PMIC_FG_IAVG_27_16);

		fg_iavg_reg_15_00 =
		pmic_get_register_value(PMIC_FG_IAVG_15_00);

		fg_iavg_reg = fg_iavg_reg_27_16;

		fg_iavg_reg =
		((long long)fg_iavg_reg << 16) + fg_iavg_reg_15_00;

		sign_bit = (fg_iavg_reg_27_16 & 0x800) >> 11;

		if (sign_bit) {
			fg_iavg_reg_tmp = fg_iavg_reg;
			/*fg_iavg_reg = fg_iavg_reg_tmp - 0xfffffff - 1;*/
			fg_iavg_reg = 0xfffffff - fg_iavg_reg_tmp + 1;
		}

		if (sign_bit == 1)
			is_bat_charging = 0;	/* discharge */
		else
			is_bat_charging = 1;	/* charge */

		fg_iavg_ma = fg_iavg_reg * UNIT_FG_IAVG *
			gauge_dev->fg_cust_data->car_tune_value;

		bm_trace(
			"[%s] fg_iavg_ma %lld fg_iavg_reg %lld fg_iavg_reg_tmp %lld\n",
			__func__, fg_iavg_ma, fg_iavg_reg, fg_iavg_reg_tmp);

#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 1000000);
		/* LSB UNIT_FG_IAVG , cartune */
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 1000000);
#endif

		bm_trace("[%s] fg_iavg_ma3 %lld\n",
			__func__, fg_iavg_ma);

		if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG) {

#if defined(__LP64__) || defined(_LP64)
			fg_iavg_ma = (fg_iavg_ma * DEFAULT_R_FG /
				gauge_dev->fg_cust_data->r_fg_value);
#else
			fg_iavg_ma = div_s64(fg_iavg_ma * DEFAULT_R_FG,
				gauge_dev->fg_cust_data->r_fg_value);
#endif

		}


#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 100);	/* change to 0.1mA */
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 100);
#endif

		bm_trace("[%s] fg_iavg_ma4 %lld\n",
			__func__, fg_iavg_ma);

		if (sign_bit == 1)
			fg_iavg_ma = 0 - fg_iavg_ma;

		bm_trace(
			"[%s] fg_iavg_ma %lld fg_iavg_reg %lld r_fg_value %d 27_16 0x%x 15_00 0x%x\n",
			__func__, fg_iavg_ma, fg_iavg_reg,
			gauge_dev->fg_cust_data->r_fg_value,
			fg_iavg_reg_27_16, fg_iavg_reg_15_00);
		gauge_dev->fg_hw_info.current_avg = fg_iavg_ma;
		gauge_dev->fg_hw_info.current_avg_sign = sign_bit;
		bm_trace("[%s] PMIC_FG_IAVG_VLD == 1\n", __func__);
	} else {
		read_fg_hw_info_current_1(gauge_dev);
		gauge_dev->fg_hw_info.current_avg =
			gauge_dev->fg_hw_info.current_1;

		if (gauge_dev->fg_hw_info.current_1 < 0)
			gauge_dev->fg_hw_info.current_avg_sign = 1;
		bm_debug("[%s] PMIC_FG_IAVG_VLD != 1, avg %d, current_1 %d\n",
			__func__, gauge_dev->fg_hw_info.current_avg,
			gauge_dev->fg_hw_info.current_1);
	}

	/* recover read */
	pmic_set_register_value(PMIC_FG_SW_CLEAR, 1);
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 0);

	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
				"[%s]data_ready_status timeout 2!\r\n",
					__func__);
			break;
		}
	}

	pmic_set_register_value(PMIC_FG_SW_CLEAR, 0);

	*data = gauge_dev->fg_hw_info.current_avg;

	bm_debug("[%s] %d\n", __func__, *data);

	return 0;
}


static signed int fg_set_iavg_intr(struct gauge_device *gauge_dev, void *data)
{
	int iavg_gap = *(unsigned int *) (data);
	int iavg;
	long long iavg_ht, iavg_lt;
	int ret;
	long long fg_iavg_reg_ht, fg_iavg_reg_lt;
	int fg_iavg_lth_28_16, fg_iavg_lth_15_00;
	int fg_iavg_hth_28_16, fg_iavg_hth_15_00;


	ret = fg_get_current_iavg(gauge_dev, &iavg);

	iavg_ht = abs(iavg) + iavg_gap;
	iavg_lt = abs(iavg) - iavg_gap;

	if (iavg_lt <= 0)
		iavg_lt = 0;

	get_mtk_battery()->hw_status.iavg_ht = iavg_ht;
	get_mtk_battery()->hw_status.iavg_lt = iavg_lt;

/* reverse for IAVG */
/* fg_iavg_ma * 100 * fg_cust_data.r_fg_value / DEFAULT_RFG * 1000 * 1000 */
/* / fg_cust_data.car_tune_value / UNIT_FG_IAVG  = fg_iavg_reg  */

	fg_iavg_reg_ht = iavg_ht * 100;
	if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG) {
		fg_iavg_reg_ht = fg_iavg_reg_ht *
			gauge_dev->fg_cust_data->r_fg_value;
#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_reg_ht, DEFAULT_R_FG);
#else
		fg_iavg_reg_ht = div_s64(fg_iavg_reg_ht, DEFAULT_R_FG);
#endif
	}

	fg_iavg_reg_ht = fg_iavg_reg_ht * 1000000;

#if defined(__LP64__) || defined(_LP64)
	do_div(fg_iavg_reg_ht, UNIT_FG_IAVG);
	do_div(fg_iavg_reg_ht, gauge_dev->fg_cust_data->car_tune_value);
#else
	fg_iavg_reg_ht = div_s64(fg_iavg_reg_ht, UNIT_FG_IAVG);
	fg_iavg_reg_ht = div_s64(fg_iavg_reg_ht,
				gauge_dev->fg_cust_data->car_tune_value);
#endif


	fg_iavg_reg_lt = iavg_lt * 100;

	if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG) {
		fg_iavg_reg_lt = fg_iavg_reg_lt *
			gauge_dev->fg_cust_data->r_fg_value;
#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_reg_lt, DEFAULT_R_FG);
#else
		fg_iavg_reg_lt = div_s64(fg_iavg_reg_lt, DEFAULT_R_FG);
#endif
	}

	fg_iavg_reg_lt = fg_iavg_reg_lt * 1000000;

#if defined(__LP64__) || defined(_LP64)
	do_div(fg_iavg_reg_lt, UNIT_FG_IAVG);
	do_div(fg_iavg_reg_lt, gauge_dev->fg_cust_data->car_tune_value);
#else
	fg_iavg_reg_lt = div_s64(fg_iavg_reg_lt, UNIT_FG_IAVG);
	fg_iavg_reg_lt = div_s64(fg_iavg_reg_lt,
				gauge_dev->fg_cust_data->car_tune_value);
#endif

	fg_iavg_lth_28_16 = (fg_iavg_reg_lt & 0x1fff0000) >> 16;
	fg_iavg_lth_15_00 = fg_iavg_reg_lt & 0xffff;
	fg_iavg_hth_28_16 = (fg_iavg_reg_ht & 0x1fff0000) >> 16;
	fg_iavg_hth_15_00 = fg_iavg_reg_ht & 0xffff;

	gauge_enable_interrupt(FG_IAVG_H_NO, 0);
	gauge_enable_interrupt(FG_IAVG_L_NO, 0);

	pmic_set_register_value(PMIC_FG_IAVG_LTH_28_16, fg_iavg_lth_28_16);
	pmic_set_register_value(PMIC_FG_IAVG_LTH_15_00, fg_iavg_lth_15_00);
	pmic_set_register_value(PMIC_FG_IAVG_HTH_28_16, fg_iavg_hth_28_16);
	pmic_set_register_value(PMIC_FG_IAVG_HTH_15_00, fg_iavg_hth_15_00);

	gauge_enable_interrupt(FG_IAVG_H_NO, 1);
	if (iavg_lt > 0)
		gauge_enable_interrupt(FG_IAVG_L_NO, 1);
	else
		gauge_enable_interrupt(FG_IAVG_L_NO, 0);

	bm_debug("[FG_IAVG_INT][%s] iavg %d iavg_gap %d iavg_ht %lld iavg_lt %lld fg_iavg_reg_ht %lld fg_iavg_reg_lt %lld\n",
			__func__, iavg, iavg_gap, iavg_ht, iavg_lt,
			fg_iavg_reg_ht, fg_iavg_reg_lt);

	bm_debug("[FG_IAVG_INT][%s] lt_28_16 0x%x lt_15_00 0x%x ht_28_16 0x%x ht_15_00 0x%x\n",
			__func__, fg_iavg_lth_28_16, fg_iavg_lth_15_00,
			fg_iavg_hth_28_16, fg_iavg_hth_15_00);

	gauge_enable_interrupt(FG_IAVG_H_NO, 1);
	if (iavg_lt > 0)
		gauge_enable_interrupt(FG_IAVG_L_NO, 1);
	else
		gauge_enable_interrupt(FG_IAVG_L_NO, 0);

	return 0;
}

void read_fg_hw_info_ncar(struct gauge_device *gauge_dev)
{
	unsigned int uvalue32_NCAR = 0;
	unsigned int uvalue32_NCAR_MSB = 0;
	unsigned int temp_NCAR_15_0 = 0;
	unsigned int temp_NCAR_31_16 = 0;
	signed int dvalue_NCAR = 0;
	long long Temp_Value = 0;

	temp_NCAR_15_0 = pmic_get_register_value(PMIC_FG_NCAR_15_00);
	temp_NCAR_31_16 = pmic_get_register_value(PMIC_FG_NCAR_31_16);

	uvalue32_NCAR = temp_NCAR_15_0 & 0xffff;
	uvalue32_NCAR |= (temp_NCAR_31_16 & 0x7fff) << 16;

	uvalue32_NCAR_MSB = (temp_NCAR_31_16 & 0x8000) >> 15;

	/*calculate the real world data    */
	dvalue_NCAR = (signed int) uvalue32_NCAR;

	if (uvalue32_NCAR == 0) {
		Temp_Value = 0;
	} else if (uvalue32_NCAR_MSB == 0x1) {
		/* dis-charging */
		Temp_Value = (long long) (dvalue_NCAR - 0x7fffffff);
		/* keep negative value */
		Temp_Value = Temp_Value - (Temp_Value * 2);
	} else {
		/*charging */
		Temp_Value = (long long) dvalue_NCAR;
	}


	/* 0.1 mAh */
#if defined(__LP64__) || defined(_LP64)
	Temp_Value = Temp_Value * UNIT_CHARGE / 1000;
#else
	Temp_Value = div_s64(Temp_Value * UNIT_CHARGE, 1000);
#endif

#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	do_div(Temp_Value, 10);
#else
	Temp_Value = div_s64(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	Temp_Value = div_s64(Temp_Value, 10);
#endif

	if (uvalue32_NCAR_MSB == 0x1)
		dvalue_NCAR = (signed int) (Temp_Value - (Temp_Value * 2));
	else
		dvalue_NCAR = (signed int) Temp_Value;

	/*Auto adjust value*/
	if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG)
		dvalue_NCAR = (dvalue_NCAR * DEFAULT_R_FG) /
			gauge_dev->fg_cust_data->r_fg_value;

	gauge_dev->fg_hw_info.ncar =
		((dvalue_NCAR * gauge_dev->fg_cust_data->car_tune_value)
		/ 1000);

}


static int gspare0_reg, gspare3_reg;
static int rtc_invalid;
static int is_bat_plugout;
static int bat_plug_out_time;

static void fgauge_read_RTC_boot_status(void)
{
	int hw_id = pmic_get_register_value(PMIC_HWCID);
	unsigned int spare0_reg = 0;
	unsigned int spare0_reg_b13 = 0;
	int spare3_reg = 0;
	int spare3_reg_valid = 0;

	spare0_reg = get_rtc_spare0_fg_value();
	spare3_reg = get_rtc_spare_fg_value();
	gspare0_reg = spare0_reg;
	gspare3_reg = spare3_reg;
	spare3_reg_valid = (spare3_reg & 0x80) >> 7;

	if (spare3_reg_valid == 0)
		rtc_invalid = 1;
	else
		rtc_invalid = 0;

	if (rtc_invalid == 0) {
		spare0_reg_b13 = (spare0_reg & 0x20) >> 5;
		if ((hw_id & 0xff00) == 0x3500)
			is_bat_plugout = spare0_reg_b13;
		else
			is_bat_plugout = !spare0_reg_b13;

		bat_plug_out_time = spare0_reg & 0x1f;	/*[12:8], 5 bits*/
	} else {
		is_bat_plugout = 1;
		bat_plug_out_time = 31;	/*[12:8], 5 bits*/
	}

	bm_err(
	"[%s] rtc_invalid %d plugout %d plugout_time %d spare3 0x%x spare0 0x%x hw_id 0x%x\n",
			__func__,
			rtc_invalid, is_bat_plugout, bat_plug_out_time,
			spare3_reg, spare0_reg, hw_id);
}


static int fgauge_initial(struct gauge_device *gauge_dev)
{
	int bat_flag = 0;
	int is_charger_exist;

	/* for bat plugout */
	/* set BATON_DEBOUNCE_THD to 0x0, set BATON_DEBOUNCE_WND to 0x10 */
/*	pmic_set_register_value(PMIC_RG_BATON_DEBOUNCE_THD, 0); */
/*	pmic_set_register_value(PMIC_RG_BATON_DEBOUNCE_WND, 2); */

	/* AUXADC_NAG_PRD_SEL  change to 0x10 means 10s detect*/
	pmic_set_register_value(PMIC_AUXADC_NAG_PRD_SEL, 2);

	fgauge_get_info(gauge_dev, GAUGE_BAT_PLUG_STATUS, &bat_flag);
	fgauge_get_info(gauge_dev, GAUGE_PL_CHARGING_STATUS, &is_charger_exist);

	bm_err("bat_plug:%d chr:%d info:0x%x\n",
		bat_flag, is_charger_exist,
		upmu_get_reg_value(PMIC_RG_SYSTEM_INFO_CON0_ADDR));

	get_mtk_battery()->hw_status.pl_charger_status = is_charger_exist;

	if (is_charger_exist == 1) {
		is_bat_plugout = 1;
		fgauge_set_info(gauge_dev, GAUGE_2SEC_REBOOT, 0);
	} else {
		if (bat_flag == 0)
			is_bat_plugout = 1;
		else
			is_bat_plugout = 0;
	}

	fgauge_set_info(gauge_dev, GAUGE_BAT_PLUG_STATUS, 1);
	bat_plug_out_time = 31;	/*[12:8], 5 bits*/

	fgauge_read_RTC_boot_status();


	return 0;
}

static int fgauge_read_current(
	struct gauge_device *gauge_dev,
	bool *fg_is_charging,
	int *data)
{
	unsigned short uvalue16 = 0;
	signed int dvalue = 0;
	int m = 0;
	long long Temp_Value = 0;

	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 1);
	m = 0;
		while (fg_get_data_ready_status() == 0) {
			m++;
			if (m > 1000) {
				bm_err(
				"[%s] fg_get_data_ready_status timeout 1!\r\n",
					__func__);
				break;
			}
		}

	uvalue16 = pmic_get_register_value(PMIC_FG_CURRENT_OUT);
	bm_trace("[%s] : FG_CURRENT = %x\r\n", __func__, uvalue16);

	pmic_set_register_value(PMIC_FG_SW_CLEAR, 1);
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 0);

	m = 0;
		while (fg_get_data_ready_status() != 0) {
			m++;
			if (m > 1000) {
				bm_err(
					"[%s] get_ready_status timeout2!\r\n",
						__func__);
				break;
			}
		}

	pmic_set_register_value(PMIC_FG_SW_CLEAR, 0);

	/*calculate the real world data    */
	dvalue = (unsigned int) uvalue16;
		if (dvalue == 0) {
			Temp_Value = (long long) dvalue;
			*fg_is_charging = false;
		} else if (dvalue > 32767) {
			/* > 0x8000 */
			Temp_Value = (long long) (dvalue - 65535);
			Temp_Value = Temp_Value - (Temp_Value * 2);
			*fg_is_charging = false;
		} else {
			Temp_Value = (long long) dvalue;
			*fg_is_charging = true;
		}

	Temp_Value = Temp_Value * UNIT_FGCURRENT;

#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 100000);
#else
	Temp_Value = div_s64(Temp_Value, 100000);
#endif
	dvalue = (unsigned int) Temp_Value;

	if (*fg_is_charging == true)
		bm_trace("[%s]curr(charging) = %d mA\r\n",
			 __func__, dvalue);
	else
		bm_trace("[%s]curr(discharging) = %d mA\r\n",
			 __func__, dvalue);

		/* Auto adjust value */
		if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG) {
			bm_trace(
			"[%s] Auto adjust value due to the Rfg is %d Ori curr=%d\n",
			__func__, gauge_dev->fg_cust_data->r_fg_value, dvalue);

			dvalue = (dvalue * DEFAULT_R_FG) /
				gauge_dev->fg_cust_data->r_fg_value;

			bm_trace("[%s] new current=%d\n",
				__func__, dvalue);
		}

		bm_trace("[%s] ori current=%d\n", __func__, dvalue);

		dvalue =
		((dvalue * gauge_dev->fg_cust_data->car_tune_value) / 1000);

		bm_debug("[%s] final current=%d (ratio=%d)\n",
			 __func__,
			dvalue, gauge_dev->fg_cust_data->car_tune_value);

		*data = dvalue;

	return 0;
}

static int fgauge_get_average_current(
	struct gauge_device *gauge_dev,
	int *data,
	bool *valid)
{
	long long fg_iavg_reg = 0;
	long long fg_iavg_reg_tmp = 0;
	long long fg_iavg_ma = 0;
	int fg_iavg_reg_27_16 = 0;
	int fg_iavg_reg_15_00 = 0;
	int sign_bit = 0;
	int is_bat_charging;
	int m;

	/* Set Read Latchdata */
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 1);

	m = 0;
		while (fg_get_data_ready_status() == 0) {
			m++;
			if (m > 1000) {
				bm_err(
				"[fg_get_current_iavg] fg_get_data_ready_status timeout 1 !\r\n");
				break;
			}
		}

	if (pmic_get_register_value(PMIC_FG_IAVG_VLD) == 1) {
		fg_iavg_reg_27_16 =
		pmic_get_register_value(PMIC_FG_IAVG_27_16);

		fg_iavg_reg_15_00 =
		pmic_get_register_value(PMIC_FG_IAVG_15_00);

		fg_iavg_reg = fg_iavg_reg_27_16;
		fg_iavg_reg =
		((long long)fg_iavg_reg << 16) + fg_iavg_reg_15_00;

		sign_bit = (fg_iavg_reg_27_16 & 0x800) >> 11;

		if (sign_bit) {
			fg_iavg_reg_tmp = fg_iavg_reg;
			/*fg_iavg_reg = fg_iavg_reg_tmp - 0xfffffff - 1;*/
			fg_iavg_reg = 0xfffffff - fg_iavg_reg_tmp + 1;
		}

		if (sign_bit == 1)
			is_bat_charging = 0;	/* discharge */
		else
			is_bat_charging = 1;	/* charge */

		fg_iavg_ma = fg_iavg_reg * UNIT_FG_IAVG *
			gauge_dev->fg_cust_data->car_tune_value;

		bm_trace(
			"[fg_get_current_iavg] fg_iavg_ma %lld fg_iavg_reg %lld fg_iavg_reg_tmp %lld\n",
			fg_iavg_ma, fg_iavg_reg, fg_iavg_reg_tmp);

#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 1000000);
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 1000000);
#endif


		if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG) {
#if defined(__LP64__) || defined(_LP64)
			fg_iavg_ma = (fg_iavg_ma * DEFAULT_R_FG /
				gauge_dev->fg_cust_data->r_fg_value);
#else
			fg_iavg_ma = div_s64(fg_iavg_ma * DEFAULT_R_FG,
				gauge_dev->fg_cust_data->r_fg_value);
#endif
		}

#if defined(__LP64__) || defined(_LP64)
		do_div(fg_iavg_ma, 100);
#else
		fg_iavg_ma = div_s64(fg_iavg_ma, 100);
#endif

		bm_trace("[fg_get_current_iavg] fg_iavg_ma %lld\n",
			fg_iavg_ma);


		if (sign_bit == 1)
			fg_iavg_ma = 0 - fg_iavg_ma;

		bm_trace(
			"[fg_get_current_iavg] fg_iavg_ma %lld fg_iavg_reg %lld r_fg_value %d 27_16 0x%x 15_00 0x%x\n",
			fg_iavg_ma, fg_iavg_reg,
			gauge_dev->fg_cust_data->r_fg_value,
			fg_iavg_reg_27_16, fg_iavg_reg_15_00);

		gauge_dev->fg_hw_info.current_avg = fg_iavg_ma;
		gauge_dev->fg_hw_info.current_avg_sign = sign_bit;
		bm_trace("[fg_get_current_iavg] PMIC_FG_IAVG_VLD == 1\n");
	} else {
		read_fg_hw_info_current_1(gauge_dev);
		gauge_dev->fg_hw_info.current_avg =
			gauge_dev->fg_hw_info.current_1;

		if (gauge_dev->fg_hw_info.current_1 < 0)
			gauge_dev->fg_hw_info.current_avg_sign = 1;

		bm_debug("[fg_get_current_iavg] PMIC_FG_IAVG_VLD != 1, avg %d, current_1 %d\n",
			gauge_dev->fg_hw_info.current_avg,
			gauge_dev->fg_hw_info.current_1);
	}

	/* recover read */
	pmic_set_register_value(PMIC_FG_SW_CLEAR, 1);
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 0);

	m = 0;
		while (fg_get_data_ready_status() != 0) {
			m++;
			if (m > 1000) {
				bm_err(
					"[fg_get_current_iavg] data_ready_status timeout 2 !\r\n");
				break;
			}
		}

	pmic_set_register_value(PMIC_FG_SW_CLEAR, 0);

	*data = gauge_dev->fg_hw_info.current_avg;

	*valid = pmic_get_register_value(PMIC_FG_IAVG_VLD);
	bm_debug("[fg_get_current_iavg] %d %d\n", *data, *valid);

	return 0;
}

static int fgauge_get_coulomb(struct gauge_device *gauge_dev, int *data)
{
#if defined(SOC_BY_3RD_FG)
		*data = bq27531_get_remaincap();
		return 0;
#else
	unsigned int uvalue32_CAR = 0;
	unsigned int uvalue32_CAR_MSB = 0;
	unsigned int temp_CAR_15_0 = 0;
	unsigned int temp_CAR_31_16 = 0;
	signed int dvalue_CAR = 0;
	int m = 0;
	long long Temp_Value = 0;

	/*fg_dump_register();*/
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 1);

	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err("[fgauge_read_columb_internal] data_ready_status timeout 1 !\r\n");
			break;
		}
	}

	temp_CAR_15_0 = pmic_get_register_value(PMIC_FG_CAR_15_00);
	temp_CAR_31_16 = pmic_get_register_value(PMIC_FG_CAR_31_16);

	uvalue32_CAR = temp_CAR_15_0 & 0xffff;
	uvalue32_CAR |= (temp_CAR_31_16 & 0x7fff) << 16;

	uvalue32_CAR_MSB = (temp_CAR_31_16 & 0x8000) >> 15;

	bm_trace(
		"[fgauge_read_columb_internal] temp_CAR_15_0 = 0x%x temp_CAR_31_16 = 0x%x\n",
		 temp_CAR_15_0, temp_CAR_31_16);

	bm_trace("[fgauge_read_columb_internal] FG_CAR = 0x%x\r\n",
		 uvalue32_CAR);
	bm_trace(
		 "[fgauge_read_columb_internal] uvalue32_CAR_MSB = 0x%x\r\n",
		 uvalue32_CAR_MSB);

	/*calculate the real world data    */
	dvalue_CAR = (signed int) uvalue32_CAR;


	pmic_set_register_value(PMIC_FG_SW_CLEAR, 1);
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 0);

	m = 0;

	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[fgauge_read_columb_internal] data_ready_status timeout 2 !\r\n");
			break;
		}
	}

	pmic_set_register_value(PMIC_FG_SW_CLEAR, 0);

	if (uvalue32_CAR == 0) {
		Temp_Value = 0;
	} else if (uvalue32_CAR_MSB == 0x1) {
		/* dis-charging */
		Temp_Value = (long long) (dvalue_CAR - 0x7fffffff);
		/* keep negative value */
		Temp_Value = Temp_Value - (Temp_Value * 2);
	} else {
		/*charging */
		Temp_Value = (long long) dvalue_CAR;
	}


#if defined(__LP64__) || defined(_LP64)
	Temp_Value = Temp_Value * UNIT_CHARGE / 1000;
#else
	Temp_Value = div_s64(Temp_Value * UNIT_CHARGE, 1000);
#endif


#if defined(__LP64__) || defined(_LP64)
	do_div(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	do_div(Temp_Value, 10);
#else
	Temp_Value = div_s64(Temp_Value, 10);
	Temp_Value = Temp_Value + 5;
	Temp_Value = div_s64(Temp_Value, 10);
#endif


	if (uvalue32_CAR_MSB == 0x1)
		dvalue_CAR = (signed int) (Temp_Value - (Temp_Value * 2));
		/* keep negative value */
	else
		dvalue_CAR = (signed int) Temp_Value;

	bm_trace("[fgauge_read_columb_internal] dvalue_CAR = %d\r\n",
		 dvalue_CAR);

/*Auto adjust value*/
	if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG) {
		bm_trace(
			 "[fgauge_read_columb_internal] Auto adjust value deu to the Rfg is %d\n Ori CAR=%d",
			 gauge_dev->fg_cust_data->r_fg_value, dvalue_CAR);

		dvalue_CAR = (dvalue_CAR * DEFAULT_R_FG) /
			gauge_dev->fg_cust_data->r_fg_value;

		bm_trace("[fgauge_read_columb_internal] new CAR=%d\n",
			 dvalue_CAR);
	}

	dvalue_CAR = ((dvalue_CAR *
		gauge_dev->fg_cust_data->car_tune_value) / 1000);

	bm_debug("[fgauge_read_columb_internal] CAR=%d r_fg_value=%d car_tune_value=%d\n",
		dvalue_CAR, gauge_dev->fg_cust_data->r_fg_value,
		gauge_dev->fg_cust_data->car_tune_value);

	*data = dvalue_CAR;

	return 0;
#endif
}

static int read_hw_ocv_6359_plug_in(void)
{
	signed int adc_rdy = 0;
	signed int adc_result_reg = 0;
	signed int adc_result = 0;
/* 6359 no need to switch SWCHR_POWER_PATH, only 56 57 */
	adc_rdy = pmic_get_register_value(
		PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_PCHR);
	adc_result_reg = pmic_get_register_value(
		PMIC_AUXADC_ADC_OUT_BAT_PLUGIN_PCHR);
	adc_result = REG_to_MV_value(adc_result_reg);
	bm_debug("[oam] %s (pchr): adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
		__func__, adc_result_reg, adc_result,
		pmic_get_register_value(PMIC_RG_HK_STRUP_AUXADC_START_SEL),
		adc_rdy);

	if (adc_rdy == 1) {
		pmic_set_register_value(PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR, 1);
		mdelay(1);
		pmic_set_register_value(PMIC_AUXADC_ADC_RDY_BAT_PLUGIN_CLR, 0);
	}

	adc_result += g_hw_ocv_tune_value;
	return adc_result;
}


static int read_hw_ocv_6359_power_on(void)
{
	signed int adc_result_rdy = 0;
	signed int adc_result_reg = 0;
	signed int adc_result = 0;

	adc_result_rdy = pmic_get_register_value(
			PMIC_AUXADC_ADC_RDY_PWRON_PCHR);
	adc_result_reg = pmic_get_register_value(
			PMIC_AUXADC_ADC_OUT_PWRON_PCHR);
	adc_result = REG_to_MV_value(adc_result_reg);
	bm_debug("[oam] %s (pchr) : adc_result_reg=%d, adc_result=%d, start_sel=%d, rdy=%d\n",
		__func__, adc_result_reg, adc_result,
		pmic_get_register_value(PMIC_RG_HK_STRUP_AUXADC_START_SEL),
		adc_result_rdy);

	if (adc_result_rdy == 1) {
		pmic_set_register_value(PMIC_AUXADC_ADC_RDY_PWRON_CLR, 1);
		mdelay(1);
		pmic_set_register_value(PMIC_AUXADC_ADC_RDY_PWRON_CLR, 0);
	}
	adc_result += g_hw_ocv_tune_value;
	return adc_result;
}


static int read_hw_ocv_6359_power_on_rdy(void)
{
	int pon_rdy = 0;

	pon_rdy = pmic_get_register_value(PMIC_AUXADC_ADC_RDY_PWRON_PCHR);
	bm_err("[%s] pwron_PCHR_rdy %d\n", __func__, pon_rdy);

	return pon_rdy;
}

static int charger_zcv;
static int pmic_in_zcv;
static int pmic_zcv;
static int pmic_rdy;
static int swocv;
static int zcv_from;
static int zcv_tmp;

static bool zcv_1st_read;
static int charger_zcv_1st;
static int pmic_in_zcv_1st;
static int pmic_zcv_1st;
static int pmic_rdy_1st;
static int swocv_1st;
static int zcv_from_1st;
static int zcv_tmp_1st;
static int moniter_plchg_bit;
static int pl_charging_status;

int read_hw_ocv(struct gauge_device *gauge_dev, int *data)
{
	int _hw_ocv, _sw_ocv;
	int _hw_ocv_src;
	int _prev_hw_ocv, _prev_hw_ocv_src;
	int _hw_ocv_rdy;
	int _flag_unreliable;
	int _hw_ocv_59_pon;
	int _hw_ocv_59_plugin;
	int _hw_ocv_59_pon_rdy;
	int _hw_ocv_chgin;
	int _hw_ocv_chgin_rdy;
	int now_temp;
	int now_thr;

	_hw_ocv_59_pon_rdy = read_hw_ocv_6359_power_on_rdy();
	_hw_ocv_59_pon = read_hw_ocv_6359_power_on();
	_hw_ocv_59_plugin = read_hw_ocv_6359_plug_in();
	_hw_ocv_chgin = battery_get_charger_zcv() / 100;
	now_temp = fg_get_battery_temperature_for_zcv();

	if (now_temp > get_mtk_battery()->ext_hwocv_swocv_lt_temp)
		now_thr = get_mtk_battery()->ext_hwocv_swocv;
	else
		now_thr = get_mtk_battery()->ext_hwocv_swocv_lt;

	if (_hw_ocv_chgin < 25000)
		_hw_ocv_chgin_rdy = 0;
	else
		_hw_ocv_chgin_rdy = 1;

	/* if preloader records charge in, need to using subpmic as hwocv */
	fgauge_get_info(
		gauge_dev, GAUGE_PL_CHARGING_STATUS, &pl_charging_status);
	fgauge_set_info(
		gauge_dev, GAUGE_PL_CHARGING_STATUS, 0);
	fgauge_get_info(
		gauge_dev, GAUGE_MONITER_PLCHG_STATUS, &moniter_plchg_bit);
	fgauge_set_info(
		gauge_dev, GAUGE_MONITER_PLCHG_STATUS, 0);

	if (pl_charging_status == 1)
		g_fg_is_charger_exist = 1;
	else
		g_fg_is_charger_exist = 0;

	_hw_ocv = _hw_ocv_59_pon;
	_sw_ocv = get_mtk_battery()->hw_status.sw_ocv;
	/* _sw_ocv = get_sw_ocv();*/
	_hw_ocv_src = FROM_6359_PON_ON;
	_prev_hw_ocv = _hw_ocv;
	_prev_hw_ocv_src = FROM_6359_PON_ON;
	_flag_unreliable = 0;

	if (g_fg_is_charger_exist) {
		_hw_ocv_rdy = _hw_ocv_59_pon_rdy;
		if (_hw_ocv_rdy == 1) {
			if (_hw_ocv_chgin_rdy == 1) {
				_hw_ocv = _hw_ocv_chgin;
				_hw_ocv_src = FROM_6360_CHR_IN;
			} else {
				_hw_ocv = _hw_ocv_59_pon;
				_hw_ocv_src = FROM_6359_PON_ON;
			}

			if (abs(_hw_ocv - _sw_ocv) > now_thr) {
				_prev_hw_ocv = _hw_ocv;
				_prev_hw_ocv_src = _hw_ocv_src;
				_hw_ocv = _sw_ocv;
				_hw_ocv_src = FROM_SW_OCV;
				set_hw_ocv_unreliable(true);
				_flag_unreliable = 1;
			}
		} else {
			/* fixme: swocv is workaround */
			/* plug charger poweron but 6359_pon not ready */
			/* should use swocv to workaround */
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
			if (_hw_ocv_chgin_rdy != 1) {
				if (abs(_hw_ocv - _sw_ocv) > now_thr) {
					_prev_hw_ocv = _hw_ocv;
					_prev_hw_ocv_src = _hw_ocv_src;
					_hw_ocv = _sw_ocv;
					_hw_ocv_src = FROM_SW_OCV;
					set_hw_ocv_unreliable(true);
					_flag_unreliable = 1;
				}
			}
		}
	} else {
		if (_hw_ocv_59_pon_rdy == 0) {
			_hw_ocv = _sw_ocv;
			_hw_ocv_src = FROM_SW_OCV;
		}
	}

	/* final chance to check hwocv */
	if (_hw_ocv < 28000 && (is_fg_disabled() == 0)) {
		bm_err("[%s] ERROR, _hw_ocv=%d  src:%d, force use swocv\n",
		__func__, _hw_ocv, _hw_ocv_src);
		_hw_ocv = _sw_ocv;
		_hw_ocv_src = FROM_SW_OCV;
	}

	*data = _hw_ocv;

	charger_zcv = _hw_ocv_chgin;
	pmic_rdy = _hw_ocv_59_pon_rdy;
	pmic_zcv = _hw_ocv_59_pon;
	pmic_in_zcv = _hw_ocv_59_plugin;
	swocv = _sw_ocv;
	zcv_from = _hw_ocv_src;
	zcv_tmp = now_temp;

	if (zcv_1st_read == false) {
		charger_zcv_1st = charger_zcv;
		pmic_rdy_1st = pmic_rdy;
		pmic_zcv_1st = pmic_zcv;
		pmic_in_zcv_1st = pmic_in_zcv;
		swocv_1st = swocv;
		zcv_from_1st = zcv_from;
		zcv_tmp_1st = zcv_tmp;
		zcv_1st_read = true;
	}

	gauge_dev->fg_hw_info.pmic_zcv = _hw_ocv_59_pon;
	gauge_dev->fg_hw_info.pmic_zcv_rdy = _hw_ocv_59_pon_rdy;
	gauge_dev->fg_hw_info.charger_zcv = _hw_ocv_chgin;
	gauge_dev->fg_hw_info.hw_zcv = _hw_ocv;

	bm_err("[%s] g_fg_is_charger_exist %d _hw_ocv_chgin_rdy %d pl:%d %d\n",
		__func__, g_fg_is_charger_exist, _hw_ocv_chgin_rdy,
		pl_charging_status, moniter_plchg_bit);
	bm_err("[%s] _hw_ocv %d _sw_ocv %d now_thr %d\n",
		__func__, _prev_hw_ocv, _sw_ocv, now_thr);
	bm_err("[%s] _hw_ocv %d _hw_ocv_src %d _prev_hw_ocv %d _prev_hw_ocv_src %d _flag_unreliable %d\n",
		__func__, _hw_ocv, _hw_ocv_src, _prev_hw_ocv,
		_prev_hw_ocv_src, _flag_unreliable);
	bm_debug("[%s] _hw_ocv_59_pon_rdy %d _hw_ocv_59_pon %d _hw_ocv_59_plugin %d _hw_ocv_chgin %d _sw_ocv %d now_temp %d now_thr %d\n",
		__func__, _hw_ocv_59_pon_rdy, _hw_ocv_59_pon,
		_hw_ocv_59_plugin, _hw_ocv_chgin, _sw_ocv,
		now_temp, now_thr);

	return 0;
}


int fgauge_set_coulomb_interrupt1_ht(
	struct gauge_device *gauge_dev,
	int car_value)
{

	unsigned int temp_CAR_15_0 = 0;
	unsigned int temp_CAR_31_16 = 0;
	unsigned int uvalue32_CAR_MSB = 0;
	signed int upperbound = 0;
	signed int upperbound_31_16 = 0, upperbound_15_00 = 0;
	signed short m;
	signed int value32_CAR;
	long long car = car_value;

	bm_trace("%s car=%d\n", __func__, car_value);
	if (car == 0) {
		gauge_enable_interrupt(FG_BAT1_INT_H_NO, 0);
		return 0;
	}

	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 1);

	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[%s] data_ready_status timeout1!", __func__);
			break;
		}
	}

	temp_CAR_15_0 = pmic_get_register_value(PMIC_FG_CAR_15_00);
	temp_CAR_31_16 = pmic_get_register_value(PMIC_FG_CAR_31_16);

	value32_CAR = temp_CAR_15_0 & 0xffff;
	value32_CAR |= (temp_CAR_31_16 & 0xffff) << 16;

	uvalue32_CAR_MSB =
		(pmic_get_register_value(PMIC_FG_CAR_31_16) & 0x8000) >> 15;

	bm_trace(
		"[%s] FG_CAR = 0x%x:%d uvalue32_CAR_MSB:0x%x 0x%x 0x%x\r\n",
		__func__, value32_CAR, value32_CAR, uvalue32_CAR_MSB,
		temp_CAR_15_0,
		temp_CAR_31_16);

	pmic_set_register_value(PMIC_FG_SW_CLEAR, 1);
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 0);

	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[%s] data_ready_status time2\r\n", __func__);
			break;
		}
	}
	pmic_set_register_value(PMIC_FG_SW_CLEAR, 0);

#if defined(__LP64__) || defined(_LP64)
	car = car * 100000 / UNIT_CHARGE;
	/* 1000 * 100 */
#else
	car = div_s64(car * 100000, UNIT_CHARGE);
#endif

	if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG)
#if defined(__LP64__) || defined(_LP64)
		car = (car * gauge_dev->fg_cust_data->r_fg_value) /
			DEFAULT_R_FG;
#else
		car = div_s64(car * gauge_dev->fg_cust_data->r_fg_value,
			DEFAULT_R_FG);
#endif

#if defined(__LP64__) || defined(_LP64)
	car = ((car * 1000) / gauge_dev->fg_cust_data->car_tune_value);
#else
	car = div_s64((car * 1000), gauge_dev->fg_cust_data->car_tune_value);
#endif

	upperbound = value32_CAR;

	bm_trace(
		"[%s] upper = 0x%x:%d diff_car=0x%llx:%lld\r\n",
		 __func__, upperbound, upperbound, car, car);

	upperbound = upperbound + car;

	upperbound_31_16 = (upperbound & 0xffff0000) >> 16;
	upperbound_15_00 = (upperbound & 0xffff);


	bm_trace(
		"[%s] final upper = 0x%x:%d car=0x%llx:%lld\r\n",
		 __func__, upperbound, upperbound, car, car);

	bm_trace(
		"[%s] final upper 0x%x 0x%x 0x%x car=0x%llx\n",
		 __func__,
		upperbound, upperbound_31_16, upperbound_15_00, car);

	gauge_enable_interrupt(FG_BAT1_INT_H_NO, 0);

	pmic_set_register_value(PMIC_FG_BAT_HTH_15_00, upperbound_15_00);
	pmic_set_register_value(PMIC_FG_BAT_HTH_31_16, upperbound_31_16);
	mdelay(1);

	gauge_enable_interrupt(FG_BAT1_INT_H_NO, 1);

	bm_debug(
		"[%s] high:0x%x 0x%x car_value:%d car:%d\r\n",
		__func__,
		pmic_get_register_value(PMIC_FG_BAT_HTH_15_00),
		pmic_get_register_value(PMIC_FG_BAT_HTH_31_16),
		car_value, value32_CAR);

	return 0;

}

int fgauge_set_coulomb_interrupt1_lt(
	struct gauge_device *gauge_dev,
	int car_value)
{
	unsigned int temp_CAR_15_0 = 0;
	unsigned int temp_CAR_31_16 = 0;
	unsigned int uvalue32_CAR_MSB = 0;
	signed int lowbound = 0;
	signed int lowbound_31_16 = 0, lowbound_15_00 = 0;
	signed short m;
	signed int value32_CAR;
	long long car = car_value;

	bm_trace("%s car=%d\n", __func__, car_value);
	if (car == 0) {
		pmic_enable_interrupt(FG_BAT1_INT_L_NO, 0, "GM30");
		return 0;
	}

	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 1);

	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[%s] data_ready_status timeout1!", __func__);
			break;
		}
	}

	temp_CAR_15_0 = pmic_get_register_value(PMIC_FG_CAR_15_00);
	temp_CAR_31_16 = pmic_get_register_value(PMIC_FG_CAR_31_16);

	value32_CAR = temp_CAR_15_0 & 0xffff;
	value32_CAR |= (temp_CAR_31_16 & 0xffff) << 16;

	uvalue32_CAR_MSB =
		(pmic_get_register_value(PMIC_FG_CAR_31_16) & 0x8000) >> 15;

	bm_trace(
		"[%s] FG_CAR = 0x%x:%d uvalue32_CAR_MSB:0x%x 0x%x 0x%x\r\n",
		__func__,
		value32_CAR, value32_CAR, uvalue32_CAR_MSB,
		(pmic_get_register_value(PMIC_FG_CAR_15_00)),
		(pmic_get_register_value(PMIC_FG_CAR_31_16)));

	pmic_set_register_value(PMIC_FG_SW_CLEAR, 1);
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 0);

	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[%s]data_ready_status timeout 2!\r\n",
				__func__);
			break;
		}
	}
	pmic_set_register_value(PMIC_FG_SW_CLEAR, 0);


	/* gap to register-base */
#if defined(__LP64__) || defined(_LP64)
	car = car * 100000 / UNIT_CHARGE;
	/* car * 1000 * 100 */
#else
	car = div_s64(car * 100000, UNIT_CHARGE);
#endif

	if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG)
#if defined(__LP64__) || defined(_LP64)
		car = (car * gauge_dev->fg_cust_data->r_fg_value) /
			DEFAULT_R_FG;
#else
		car = div_s64(car * gauge_dev->fg_cust_data->r_fg_value,
			DEFAULT_R_FG);
#endif

#if defined(__LP64__) || defined(_LP64)
	car = ((car * 1000) / gauge_dev->fg_cust_data->car_tune_value);
#else
	car = div_s64((car * 1000), gauge_dev->fg_cust_data->car_tune_value);
#endif

	lowbound = value32_CAR;

	bm_trace(
		"[%s]low=0x%x:%d diff_car=0x%llx:%lld\r\n",
		 __func__, lowbound, lowbound, car, car);

	lowbound = lowbound - car;

	lowbound_31_16 = (lowbound & 0xffff0000) >> 16;
	lowbound_15_00 = (lowbound & 0xffff);

	bm_trace(
		"[%s]final low=0x%x:%d car=0x%llx:%lld\r\n",
		 __func__, lowbound, lowbound, car, car);

	bm_trace(
		"[%s] final low 0x%x 0x%x 0x%x car=0x%llx\n",
		 __func__, lowbound, lowbound_31_16, lowbound_15_00, car);

	gauge_enable_interrupt(FG_BAT1_INT_L_NO, 0);
	pmic_set_register_value(PMIC_FG_BAT_LTH_15_00, lowbound_15_00);
	pmic_set_register_value(PMIC_FG_BAT_LTH_31_16, lowbound_31_16);
	mdelay(1);

	gauge_enable_interrupt(FG_BAT1_INT_L_NO, 1);

	bm_debug(
		"[%s] low:0x%x 0x%x car_value:%d car:%d\r\n",
		__func__, pmic_get_register_value(PMIC_FG_BAT_LTH_15_00),
		pmic_get_register_value(PMIC_FG_BAT_LTH_31_16),
		car_value, value32_CAR);

	return 0;
}

static int fgauge_read_boot_battery_plug_out_status(
	struct gauge_device *gauge_dev,
	int *is_plugout,
	int *plutout_time)
{
	*is_plugout = is_bat_plugout;
	*plutout_time = bat_plug_out_time;
	bm_err(
		"[%s] rtc_invalid %d plugout %d bat_plug_out_time %d sp3:0x%x pl:%d %d\n",
		__func__, rtc_invalid, is_bat_plugout, bat_plug_out_time,
		gspare3_reg, moniter_plchg_bit, pl_charging_status);

	return 0;
}

static int fgauge_get_ptim_current(
	struct gauge_device *gauge_dev,
	int *ptim_current,
	bool *is_charging)
{
		unsigned short uvalue16 = 0;
		signed int dvalue = 0;
		/*int m = 0;*/
		long long Temp_Value = 0;
		/*unsigned int ret = 0;*/

		uvalue16 = pmic_get_register_value(PMIC_FG_R_CURR);
		bm_trace("[%s] : FG_CURRENT = %x\r\n",
			 __func__, uvalue16);

		/*calculate the real world data    */
		dvalue = (unsigned int) uvalue16;
		if (dvalue == 0) {
			Temp_Value = (long long) dvalue;
			*is_charging = false;
		} else if (dvalue > 32767) {
			/* > 0x8000 */
			Temp_Value = (long long) (dvalue - 65535);
			Temp_Value = Temp_Value - (Temp_Value * 2);
			*is_charging = false;
		} else {
			Temp_Value = (long long) dvalue;
			*is_charging = true;
		}

		Temp_Value = Temp_Value * UNIT_FGCURRENT;
#if defined(__LP64__) || defined(_LP64)
		do_div(Temp_Value, 100000);
#else
		Temp_Value = div_s64(Temp_Value, 100000);
#endif

		dvalue = (unsigned int) Temp_Value;

		if (*is_charging == true)
			bm_trace(
			"[fgauge_read_IM_current]curr(charging)=%dmA\r\n",
			dvalue);
		else
			bm_trace(
			"[fgauge_read_IM_current]curr(discharging)=%dmA\r\n",
			dvalue);

		/* Auto adjust value */
		if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG) {
			bm_trace(
			"[fgauge_read_IM_current] Auto adjust value due to the Rfg is %d\n Ori curr=%d, ",
			gauge_dev->fg_cust_data->r_fg_value, dvalue);

			dvalue = (dvalue * DEFAULT_R_FG) /
				gauge_dev->fg_cust_data->r_fg_value;

			bm_trace("[fgauge_read_IM_current] new current=%d\n",
			dvalue);
		}

		bm_trace("[fgauge_read_IM_current] ori current=%d\n", dvalue);

		dvalue = ((dvalue *
			gauge_dev->fg_cust_data->car_tune_value) / 1000);

		bm_debug("[fgauge_read_IM_current] final current=%d (ratio=%d)\n",
			 dvalue, gauge_dev->fg_cust_data->car_tune_value);

		*ptim_current = dvalue;

		return 0;
}

static int fgauge_get_zcv_current(
	struct gauge_device *gauge_dev,
	int *zcv_current)
{
	unsigned short uvalue16 = 0;
	signed int dvalue = 0;
	long long Temp_Value = 0;

	uvalue16 = pmic_get_register_value(PMIC_FG_ZCV_CURR);
	dvalue = (unsigned int) uvalue16;
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
	if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG) {
		bm_trace(
		"[fgauge_read_current] Auto adjust value due to the Rfg is %d\n Ori curr=%d",
		gauge_dev->fg_cust_data->r_fg_value, dvalue);

		dvalue = (dvalue * DEFAULT_R_FG) /
		gauge_dev->fg_cust_data->r_fg_value;

		bm_trace("[fgauge_read_current] new current=%d\n", dvalue);
	}

	bm_trace("[fgauge_read_current] ori current=%d\n", dvalue);

	dvalue = ((dvalue * gauge_dev->fg_cust_data->car_tune_value) / 1000);

	bm_debug("[fgauge_read_current] final current=%d (ratio=%d)\n",
		 dvalue, gauge_dev->fg_cust_data->car_tune_value);

	*zcv_current = dvalue;

	return 0;

}


static int fgauge_get_zcv(struct gauge_device *gauge_dev, int *zcv)
{
	signed int adc_result_reg = 0;
	signed int adc_result = 0;

	adc_result_reg =
		pmic_get_register_value(PMIC_AUXADC_ADC_OUT_FGADC_PCHR);
	adc_result = REG_to_MV_value(adc_result_reg);
	bm_debug("[oam] %s BATSNS  (pchr):adc_result_reg=%d, adc_result=%d\n",
		 __func__, adc_result_reg, adc_result);

	adc_result += g_hw_ocv_tune_value;
	*zcv = adc_result;
	return 0;
}

static int fgauge_is_gauge_initialized(
	struct gauge_device *gauge_dev,
	int *init)
{
	int fg_reset_status = pmic_get_register_value(PMIC_FG_RSTB_STATUS);

	*init = fg_reset_status;

	return 0;
}

static int fgauge_set_gauge_initialized(
	struct gauge_device *gauge_dev,
	int init)
{
	int fg_reset_status = init;

	pmic_set_register_value(PMIC_FG_RSTB_STATUS, fg_reset_status);

	return 0;
}

static int fgauge_reset_shutdown_time(struct gauge_device *gauge_dev)
{
	return 0;
}

static int fgauge_reset_ncar(struct gauge_device *gauge_dev)
{
	pmic_set_register_value(PMIC_FG_N_CHARGE_RST, 1);
	udelay(200);
	pmic_set_register_value(PMIC_FG_N_CHARGE_RST, 0);

	return 0;
}

static int nag_zcv_mv;
static int nag_c_dltv_mv;
static int _zcv_reg;
static int _thr_reg;

static void switch_nafg_period(int _prd, int *value)
{
	if (_prd >= 1 && _prd < 5) {
		/* NAG_PRD = 1s detect 1 time*/
		*value = 0;
	} else if (_prd >= 5 && _prd < 10) {
		/* NAG_PRD = 5s detect 1 time*/
		*value = 1;
	} else if (_prd >= 10 && _prd < 20) {
		/* NAG_PRD = 10s detect 1 time*/
		*value = 2;
	} else if (_prd >= 20) {
		/* NAG_PRD = 20s detect 1 time*/
		*value = 3;
	}
}

static void fgauge_set_nafg_intr_internal(int _prd, int _zcv_mv, int _thr_mv)
{
	int NAG_C_DLTV_Threashold_26_16;
	int NAG_C_DLTV_Threashold_15_0;
	int period = 0;

	_zcv_reg = MV_to_REG_value(_zcv_mv);
	_thr_reg = MV_to_REG_value(_thr_mv);

	NAG_C_DLTV_Threashold_26_16 = (_thr_reg & 0xffff0000) >> 16;
	NAG_C_DLTV_Threashold_15_0 = (_thr_reg & 0x0000ffff);

	pmic_set_register_value(PMIC_AUXADC_NAG_ZCV, _zcv_reg);

	pmic_set_register_value(PMIC_AUXADC_NAG_C_DLTV_TH_26_16,
				NAG_C_DLTV_Threashold_26_16);
	pmic_set_register_value(PMIC_AUXADC_NAG_C_DLTV_TH_15_0,
				NAG_C_DLTV_Threashold_15_0);

	/* AUXADC_NAG_PRD_SEL  change to 0x10 means 10s detect*/
	switch_nafg_period(_prd, &period);
	pmic_set_register_value(PMIC_AUXADC_NAG_PRD_SEL, period);

	pmic_set_register_value(PMIC_AUXADC_NAG_VBAT1_SEL, 0);/* use Batsns */

	bm_debug("[fg_bat_nafg][fgauge_set_nafg_interrupt_internal] time[%d] zcv[%d:%d] thr[%d:%d] 26_16[0x%x] 15_00[0x%x] %d\n",
		_prd, _zcv_mv, _zcv_reg, _thr_mv, _thr_reg,
		NAG_C_DLTV_Threashold_26_16, NAG_C_DLTV_Threashold_15_0,
		pmic_get_register_value(PMIC_AUXADC_NAG_VBAT1_SEL));

}


static int fgauge_set_nag_zcv(struct gauge_device *gauge_dev, int zcv)
{
	nag_zcv_mv = zcv;	/* 0.1 mv*/
	return 0;
}

static int fgauge_set_nag_c_dltv(struct gauge_device *gauge_dev, int c_dltv_mv)
{
	nag_c_dltv_mv = c_dltv_mv;	/* 0.1 mv*/

	fgauge_set_nafg_intr_internal(
	gauge_dev->fg_cust_data->nafg_time_setting, nag_zcv_mv, nag_c_dltv_mv);

	return 0;
}

static int fgauge_enable_nag_interrupt(struct gauge_device *gauge_dev, int en)
{
	if (en != 0)
		en = 1;
	gauge_enable_interrupt(FG_RG_INT_EN_NAG_C_DLTV, en);
	pmic_set_register_value(PMIC_AUXADC_NAG_IRQ_EN, en);
	pmic_set_register_value(PMIC_AUXADC_NAG_EN, en);

	return 0;
}

static int fgauge_get_nag_cnt(struct gauge_device *gauge_dev, int *nag_cnt)
{
	signed int NAG_C_DLTV_CNT;
	signed int NAG_C_DLTV_CNT_H;

	/*AUXADC_NAG_4*/
	NAG_C_DLTV_CNT = pmic_get_register_value(PMIC_AUXADC_NAG_CNT_15_0);

	/*AUXADC_NAG_5*/
	NAG_C_DLTV_CNT_H = pmic_get_register_value(PMIC_AUXADC_NAG_CNT_25_16);
	*nag_cnt = (NAG_C_DLTV_CNT & 0xffff) +
		((NAG_C_DLTV_CNT_H & 0x3ff) << 16);
	bm_debug("[fg_bat_nafg][%s] %d [25_16 %d 15_0 %d]\n",
			__func__, *nag_cnt, NAG_C_DLTV_CNT_H, NAG_C_DLTV_CNT);

	return 0;
}

static int fgauge_get_nag_dltv(struct gauge_device *gauge_dev, int *nag_dltv)
{
	signed int NAG_DLTV_reg_value;
	signed int NAG_DLTV_mV_value;

	NAG_DLTV_reg_value = pmic_get_register_value(PMIC_AUXADC_NAG_DLTV);

	NAG_DLTV_mV_value = REG_to_MV_value(NAG_DLTV_reg_value);
	*nag_dltv = NAG_DLTV_mV_value;

	bm_debug("[fg_bat_nafg][%s] mV:Reg [%d:%d]\n",
		__func__, NAG_DLTV_mV_value, NAG_DLTV_reg_value);

	return 0;
}

static int fgauge_get_nag_c_dltv(
	struct gauge_device *gauge_dev,
	int *nag_c_dltv)
{
	signed int NAG_C_DLTV_value;
	signed int NAG_C_DLTV_value_H;
	signed int NAG_C_DLTV_reg_value;
	signed int NAG_C_DLTV_mV_value;
	bool bcheckbit10;

	/*AUXADC_NAG_7*/
	NAG_C_DLTV_value = pmic_get_register_value(
			PMIC_AUXADC_NAG_C_DLTV_15_0);

	/*AUXADC_NAG_8*/
	NAG_C_DLTV_value_H = pmic_get_register_value(
			PMIC_AUXADC_NAG_C_DLTV_26_16);

	bcheckbit10 = NAG_C_DLTV_value_H & 0x0400;

	if (g_nag_corner == 1) {
		NAG_C_DLTV_reg_value = (NAG_C_DLTV_value & 0x7fff);
		NAG_C_DLTV_mV_value = REG_to_MV_value(NAG_C_DLTV_reg_value);
		*nag_c_dltv = NAG_C_DLTV_mV_value;

		bm_err("[fg_bat_nafg][%s] mV:Reg[%d:%d] [b10:%d][26_16(0x%04x) 15_00(0x%04x)] corner:%d\n",
			__func__, NAG_C_DLTV_mV_value, NAG_C_DLTV_reg_value,
			bcheckbit10, NAG_C_DLTV_value_H, NAG_C_DLTV_value,
			g_nag_corner);
		return 0;
	} else if (g_nag_corner == 2) {
		NAG_C_DLTV_reg_value = (NAG_C_DLTV_value - 32768);
		NAG_C_DLTV_mV_value =
			REG_to_MV_value(NAG_C_DLTV_reg_value);
		*nag_c_dltv = NAG_C_DLTV_mV_value;

		bm_err("[fg_bat_nafg][%s] mV:Reg[%d:%d] [b10:%d][26_16(0x%04x) 15_00(0x%04x)] corner:%d\n",
			__func__, NAG_C_DLTV_mV_value, NAG_C_DLTV_reg_value,
			bcheckbit10, NAG_C_DLTV_value_H, NAG_C_DLTV_value,
			g_nag_corner);
		return 0;
	}

	if (bcheckbit10 == 0)
		NAG_C_DLTV_reg_value = (NAG_C_DLTV_value & 0xffff) +
				((NAG_C_DLTV_value_H & 0x07ff) << 16);
	else
		NAG_C_DLTV_reg_value = (NAG_C_DLTV_value & 0xffff) +
			(((NAG_C_DLTV_value_H | 0xf800) & 0xffff) << 16);

	NAG_C_DLTV_mV_value = REG_to_MV_value(NAG_C_DLTV_reg_value);
	*nag_c_dltv = NAG_C_DLTV_mV_value;

	bm_debug("[fg_bat_nafg][%s] mV:Reg[%d:%d] [b10:%d][26_16(0x%04x) 15_00(0x%04x)] corner:%d\n",
		__func__, NAG_C_DLTV_mV_value, NAG_C_DLTV_reg_value,
		bcheckbit10, NAG_C_DLTV_value_H, NAG_C_DLTV_value,
		g_nag_corner);

	return 0;

}

static void fgauge_set_zcv_intr_internal(
	struct gauge_device *gauge_dev,
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

	if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG)
#if defined(__LP64__) || defined(_LP64)
		fg_zcv_car_th_reg = (fg_zcv_car_th_reg *
				gauge_dev->fg_cust_data->r_fg_value) /
				DEFAULT_R_FG;
#else
		fg_zcv_car_th_reg = div_s64(fg_zcv_car_th_reg *
				gauge_dev->fg_cust_data->r_fg_value,
				DEFAULT_R_FG);
#endif

#if defined(__LP64__) || defined(_LP64)
	fg_zcv_car_th_reg = ((fg_zcv_car_th_reg * 1000) /
			gauge_dev->fg_cust_data->car_tune_value);
#else
	fg_zcv_car_th_reg = div_s64((fg_zcv_car_th_reg * 1000),
			gauge_dev->fg_cust_data->car_tune_value);
#endif

	fg_zcv_car_thr_h_reg = (fg_zcv_car_th_reg & 0xffff0000) >> 16;
	fg_zcv_car_thr_l_reg = fg_zcv_car_th_reg & 0x0000ffff;

	pmic_set_register_value(PMIC_FG_ZCV_DET_IV, fg_zcv_det_time);
	g_fg_zcv_det_iv = fg_zcv_det_time;
	pmic_set_register_value(PMIC_FG_ZCV_CAR_TH_15_00,
				fg_zcv_car_thr_l_reg);
	pmic_set_register_value(PMIC_FG_ZCV_CAR_TH_30_16,
				fg_zcv_car_thr_h_reg);

	bm_err("[FG_ZCV_INT][%s] det_time %d mv %d reg %lld 30_16 0x%x 15_00 0x%x\n",
		__func__, fg_zcv_det_time, fg_zcv_car_th, fg_zcv_car_th_reg,
		fg_zcv_car_thr_h_reg, fg_zcv_car_thr_l_reg);
}

void reset_zcv_int(struct gauge_device *gauge_dev)
{
	struct timespec time, time_now, end_time;
	ktime_t ktime;
	struct mt6359_gauge *gauge;

	pmic_set_register_value(PMIC_RG_INT_EN_FG_ZCV, 0);
	pmic_set_register_value(PMIC_FG_ZCV_DET_EN, 0);
	msleep(30);
	pmic_set_register_value(PMIC_FG_ZCV_DET_EN, 1);
	msleep(30);

	get_monotonic_boottime(&time_now);
	time.tv_sec = (g_fg_zcv_det_iv + 1) * 3 * 60 + 60;
	time.tv_nsec = 0;

	end_time = timespec_add(time_now, time);
	ktime = ktime_set(end_time.tv_sec, end_time.tv_nsec);

	gauge = (struct mt6359_gauge *)gauge_dev->driver_data;
	alarm_start(&gauge->zcv_timer, ktime);

}


static int fgauge_reset_hw(struct gauge_device *gauge_dev)
{
	unsigned int ret = 0, check_car = 0;

	bm_trace("[fgauge_hw_reset] : Start, only reset time and car\n");

	ret = pmic_config_interface(
		MT6359_FGADC_CON1, 0x0630, 0x0F00, 0x0);
	bm_err("[fgauge_hw_reset] reset fgadc car ret =%d\n", ret);

	mdelay(1);

	ret = pmic_config_interface(
		MT6359_FGADC_CON1, 0x0030, 0x0F00, 0x0);

	fgauge_get_coulomb(gauge_dev, &check_car);
	reset_zcv_int(gauge_dev);

	bm_trace("[fgauge_hw_reset]:End car=%d,ret=%d\n", check_car, ret);

	return 0;
}

static int fgauge_enable_zcv_interrupt(struct gauge_device *gauge_dev, int en)
{
	if (en == 0) {
		gauge_enable_interrupt(FG_ZCV_NO, en);
		pmic_set_register_value(PMIC_FG_ZCV_DET_EN, en);
		mdelay(1);
	}
	if (en == 1) {
		gauge_enable_interrupt(FG_ZCV_NO, en);
		pmic_set_register_value(PMIC_FG_ZCV_DET_EN, en);
	}

	bm_debug("[FG_ZCV_INT][fg_set_zcv_intr_en] En %d\n", en);

	return 0;
}

static int fgauge_set_zcv_interrupt_threshold(
	struct gauge_device *gauge_dev,
	int zcv_avg_current)
{
	int fg_zcv_det_time = gauge_dev->fg_cust_data->zcv_suspend_time;
	int fg_zcv_car_th = 0;

	fg_zcv_car_th = (fg_zcv_det_time + 1) * 4 * zcv_avg_current / 60;

	bm_err("[%s] current:%d, fg_zcv_det_time:%d, fg_zcv_car_th:%d\n",
		__func__, zcv_avg_current, fg_zcv_det_time, fg_zcv_car_th);

	fgauge_set_zcv_intr_internal(
		gauge_dev, fg_zcv_det_time, fg_zcv_car_th);

	return 0;
}

void battery_dump_nag(void)
{
	unsigned int nag_vbat_reg, vbat_val;
	int nag_vbat_mv, i = 0;

	do {
		nag_vbat_reg = upmu_get_reg_value(
				PMIC_AUXADC_ADC_OUT_NAG_ADDR);
		if ((nag_vbat_reg & 0x8000) != 0)
			break;
		msleep(30);
		i++;
	} while (i <= 5);

	vbat_val = nag_vbat_reg & 0x7fff;
	nag_vbat_mv = REG_to_MV_value(vbat_val);

	bm_err("[read_nafg_vbat] i:%d nag_vbat_reg 0x%x nag_vbat_mv %d:%d %d, nag_zcv:%d,_zcv_reg:0x%x,thr:%d,_thr_reg:0x%x\n",
		i, nag_vbat_reg, nag_vbat_mv, vbat_val,
		pmic_get_battery_voltage(),
		nag_zcv_mv, _zcv_reg, nag_c_dltv_mv, _thr_reg
		);


	bm_err("[read_nafg_vbat1] %d %d %d %d %d %d %d %d %d %d\n",
		pmic_get_register_value(PMIC_AUXADC_NAG_C_DLTV_IRQ),
		pmic_get_register_value(PMIC_AUXADC_NAG_IRQ_EN),
		pmic_get_register_value(PMIC_AUXADC_NAG_PRD_SEL),
		pmic_get_register_value(PMIC_AUXADC_NAG_VBAT1_SEL),
		pmic_get_register_value(PMIC_AUXADC_NAG_CLR),
		pmic_get_register_value(PMIC_AUXADC_NAG_EN),
		pmic_get_register_value(PMIC_AUXADC_NAG_ZCV),
		pmic_get_register_value(PMIC_AUXADC_NAG_C_DLTV_TH_15_0),
		pmic_get_register_value(PMIC_AUXADC_NAG_C_DLTV_TH_26_16),
		pmic_get_register_value(PMIC_AUXADC_NAG_CNT_15_0)
		);

	bm_err("[read_nafg_vbat2] %d %d %d %d %d %d %d %d %d %d\n",
		pmic_get_register_value(PMIC_RG_AUXADC_CK_PDN_HWEN),
		pmic_get_register_value(PMIC_RG_AUXADC_CK_PDN),
		pmic_get_register_value(PMIC_RG_AUXADC_RNG_CK_PDN_HWEN),
		pmic_get_register_value(PMIC_RG_AUXADC_32K_CK_PDN),
		pmic_get_register_value(PMIC_RG_AUXADC_1M_CK_TSTSEL),
		pmic_get_register_value(PMIC_RG_AUXADC_1M_CK_PDN),
		pmic_get_register_value(PMIC_RG_AUXADC_RST),
		pmic_get_register_value(PMIC_RG_INT_EN_NAG_C_DLTV),
		pmic_get_register_value(PMIC_RG_INT_MASK_NAG_C_DLTV),
		pmic_get_register_value(PMIC_RG_INT_STATUS_NAG_C_DLTV)
	);
}

static int fgauge_get_nag_vbat(struct gauge_device *gauge_dev, int *vbat)
{
	unsigned int nag_vbat_reg, vbat_val;
	int nag_vbat_mv, i = 0;

	do {
		nag_vbat_reg = upmu_get_reg_value(
				PMIC_AUXADC_ADC_OUT_NAG_ADDR);
		if ((nag_vbat_reg & 0x8000) != 0)
			break;
		msleep(30);
		i++;
	} while (i <= 5);

	vbat_val = nag_vbat_reg & 0x7fff;
	nag_vbat_mv = REG_to_MV_value(vbat_val);
	*vbat = nag_vbat_mv;

	return 0;
}

static int fgauge_enable_battery_tmp_lt_interrupt(
	struct gauge_device *gauge_dev,
	bool en,
	int threshold)
{
	int tmp_int_lt = 0;

	if (en == 0) {
		gauge_enable_interrupt(FG_RG_INT_EN_BAT_TEMP_L, 0);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_IRQ_EN_MAX, 0);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_EN, 0);
	} else {
		tmp_int_lt = MV_to_REG_12_temp_value(threshold);

		pmic_set_register_value(
			PMIC_AUXADC_BAT_TEMP_DET_PRD_SEL, 2);
		/* unit: 0x10 = 2, means 5 second */

		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_DEBT_MAX_SEL, 2);
		/* debounce 0x10 = 2 , means 4 times*/
		/* 5s * 4 times = 20s to issue bat_temp interrupt */

		pmic_set_register_value(
			PMIC_AUXADC_BAT_TEMP_VOLT_MAX, tmp_int_lt);
		/* MAX is high temp */
		pmic_set_register_value(PMIC_RG_INT_MASK_BAT_TEMP_L, 0);
		gauge_enable_interrupt(FG_RG_INT_EN_BAT_TEMP_L, 1);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_IRQ_EN_MAX, 1);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_DET_MAX, 1);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_EN, 1);
	}

	bm_debug("[%s]en:%d mv:%d reg:%d\n",
			__func__, en, threshold, tmp_int_lt);

	return 0;
}


static int fgauge_enable_battery_tmp_ht_interrupt(
	struct gauge_device *gauge_dev,
	bool en,
	int threshold)
{
	int tmp_int_ht = 0;

	if (en == 0) {
		gauge_enable_interrupt(FG_RG_INT_EN_BAT_TEMP_H, 0);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_IRQ_EN_MIN, 0);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_EN, 0);
	} else {
		tmp_int_ht = MV_to_REG_12_temp_value(threshold);

		pmic_set_register_value(
			PMIC_AUXADC_BAT_TEMP_DET_PRD_SEL, 2);
		/* unit: 0x10 = 2, means 5 second */

		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_DEBT_MIN_SEL, 2);
		/* debounce 0x10 = 2 , means 4 times*/
		/* 5s * 4 times = 20s to issue bat_temp interrupt */

		pmic_set_register_value(
			PMIC_AUXADC_BAT_TEMP_VOLT_MIN, tmp_int_ht);
		/* MAX is low temp */

		pmic_set_register_value(PMIC_RG_INT_MASK_BAT_TEMP_H, 0);
		gauge_enable_interrupt(FG_RG_INT_EN_BAT_TEMP_H, 1);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_IRQ_EN_MIN, 1);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_DET_MIN, 1);
		pmic_set_register_value(PMIC_AUXADC_BAT_TEMP_EN, 1);
	}

	bm_debug("[%s]en:%d mv:%d reg:%d\n",
			__func__, en, threshold, tmp_int_ht);

	return 0;
}

int fgauge_get_time(struct gauge_device *gauge_dev, unsigned int *ptime)
{
	unsigned int time_29_16, time_15_00, ret_time;
	int m = 0;
	long long time = 0;

	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 1);
	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[%s] data_ready_sta tim out1!\n", __func__);
			break;
		}
	}

	time_15_00 = pmic_get_register_value(PMIC_FG_NTER_15_00);
	time_29_16 = pmic_get_register_value(PMIC_FG_NTER_29_16);

	time = time_15_00;
	time |= time_29_16 << 16;
#if defined(__LP64__) || defined(_LP64)
	time = time * UNIT_TIME / 100;
#else
	time = div_s64(time * UNIT_TIME, 100);
#endif
	ret_time = time;

	bm_trace(
		 "[%s] low:0x%x high:0x%x rtime:0x%llx 0x%x!\r\n",
		 __func__, time_15_00, time_29_16, time, ret_time);

	pmic_set_register_value(PMIC_FG_SW_CLEAR, 1);
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 0);

	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
				 "[%s] fg_get_data_ready_status timeout 2\r\n",
					__func__);
			break;
		}
	}
	pmic_set_register_value(PMIC_FG_SW_CLEAR, 0);

	*ptime = ret_time;

	return 0;
}

int fgauge_set_time_interrupt(struct gauge_device *gauge_dev, int threshold)
{
	return 0;

}

static void Intr_Number_to_Name(char *intr_name, int intr_no)
{
	switch (intr_no) {
	case FG_INTR_0:
		sprintf(intr_name, "FG_INTR_INIT");
		break;

	case FG_INTR_TIMER_UPDATE:
		sprintf(intr_name, "FG_INTR_TIMER_UPDATE");
		break;

	case FG_INTR_BAT_CYCLE:
		sprintf(intr_name, "FG_INTR_BAT_CYCLE");
		break;

	case FG_INTR_CHARGER_OUT:
		sprintf(intr_name, "FG_INTR_CHARGER_OUT");
		break;

	case FG_INTR_CHARGER_IN:
		sprintf(intr_name, "FG_INTR_CHARGER_IN");
		break;

	case FG_INTR_FG_TIME:
		sprintf(intr_name, "FG_INTR_FG_TIME");
		break;

	case FG_INTR_BAT_INT1_HT:
		sprintf(intr_name, "FG_INTR_COULOMB_HT");
		break;

	case FG_INTR_BAT_INT1_LT:
		sprintf(intr_name, "FG_INTR_COULOMB_LT");
		break;

	case FG_INTR_BAT_INT2_HT:
		sprintf(intr_name, "FG_INTR_UISOC_HT");
		break;

	case FG_INTR_BAT_INT2_LT:
		sprintf(intr_name, "FG_INTR_UISOC_LT");
		break;

	case FG_INTR_BAT_TMP_HT:
		sprintf(intr_name, "FG_INTR_BAT_TEMP_HT");
		break;

	case FG_INTR_BAT_TMP_LT:
		sprintf(intr_name, "FG_INTR_BAT_TEMP_LT");
		break;

	case FG_INTR_BAT_TIME_INT:
		sprintf(intr_name, "FG_INTR_BAT_TIME_INT");
		break;

	case FG_INTR_NAG_C_DLTV:
		sprintf(intr_name, "FG_INTR_NAFG_VOLTAGE");
		break;

	case FG_INTR_FG_ZCV:
		sprintf(intr_name, "FG_INTR_FG_ZCV");
		break;

	case FG_INTR_SHUTDOWN:
		sprintf(intr_name, "FG_INTR_SHUTDOWN");
		break;

	case FG_INTR_RESET_NVRAM:
		sprintf(intr_name, "FG_INTR_RESET_NVRAM");
		break;

	case FG_INTR_BAT_PLUGOUT:
		sprintf(intr_name, "FG_INTR_BAT_PLUGOUT");
		break;

	case FG_INTR_IAVG:
		sprintf(intr_name, "FG_INTR_IAVG");
		break;

	case FG_INTR_VBAT2_L:
		sprintf(intr_name, "FG_INTR_VBAT2_L");
		break;

	case FG_INTR_VBAT2_H:
		sprintf(intr_name, "FG_INTR_VBAT2_H");
		break;

	case FG_INTR_CHR_FULL:
		sprintf(intr_name, "FG_INTR_CHR_FULL");
		break;

	case FG_INTR_DLPT_SD:
		sprintf(intr_name, "FG_INTR_DLPT_SD");
		break;

	case FG_INTR_BAT_TMP_C_HT:
		sprintf(intr_name, "FG_INTR_BAT_TMP_C_HT");
		break;

	case FG_INTR_BAT_TMP_C_LT:
		sprintf(intr_name, "FG_INTR_BAT_TMP_C_LT");
		break;

	case FG_INTR_BAT_INT1_CHECK:
		sprintf(intr_name, "FG_INTR_COULOMB_C");
		break;

	default:
		sprintf(intr_name, "FG_INTR_UNKNOWN");
		bm_err("[%s] unknown intr %d\n", __func__, intr_no);
		break;
	}
}

int gauge_IP_debug(void)
{

	bm_debug("[fgadc] %d %d %d %d %d %d %d\n",
		pmic_get_register_value(PMIC_RG_AUXADC_CK_PDN_HWEN),
		pmic_get_register_value(PMIC_RG_AUXADC_CK_PDN),
		pmic_get_register_value(PMIC_RG_AUXADC_RNG_CK_PDN_HWEN),
		pmic_get_register_value(PMIC_RG_AUXADC_32K_CK_PDN),
		pmic_get_register_value(PMIC_RG_AUXADC_1M_CK_TSTSEL),
		pmic_get_register_value(PMIC_RG_AUXADC_1M_CK_PDN),
		pmic_get_register_value(PMIC_RG_AUXADC_RST));


	bm_debug("[fgadc] %d %d %d %d %d, osr1:%d %d %d %d,ON:%d\n",
		pmic_get_register_value(PMIC_RG_FGADC_ANA_CK_PDN),
		pmic_get_register_value(PMIC_RG_FGADC_DIG_CK_PDN),
		pmic_get_register_value(PMIC_RG_FG_CK_TST_DIS),
		pmic_get_register_value(PMIC_RG_FGADC_ANA_CK_TSTSEL),
		pmic_get_register_value(PMIC_RG_FG_CK_TSTSEL),

		pmic_get_register_value(PMIC_FG_OSR1),
		pmic_get_register_value(PMIC_FG_OSR2),
		pmic_get_register_value(PMIC_FG_IAVG_MODE),
		pmic_get_register_value(PMIC_FG_GAIN),
		pmic_get_register_value(PMIC_FG_ON));

	bm_debug("[fgadc]%d,DA:%d %d,con0:0x%x,con1:0x%x,BMTOP:0x%x,set:0x%x,clear:0x%x\n",
		pmic_get_register_value(PMIC_RG_FGADC_RST_SRC_SEL),
		pmic_get_register_value(PMIC_DA_VAUX18_B_EN),
		pmic_get_register_value(PMIC_DA_VAUX18_B_STB),
		upmu_get_reg_value(MT6359_FGADC_CON0),
		upmu_get_reg_value(MT6359_FGADC_CON1),
		upmu_get_reg_value(MT6359_BM_TOP_RST_CON0),
		pmic_get_register_value(PMIC_BM_TOP_RST_CON0_SET),
		pmic_get_register_value(PMIC_BM_TOP_RST_CON0_CLR));

	return 0;
}

int fgauge_get_hw_status(
	struct gauge_device *gauge_dev,
	struct gauge_hw_status *gauge_status,
	int intr_no)
{
	int ret, m;
	char intr_name[32];
	int is_iavg_valid;
	int iavg_th;
	unsigned int time;
	/* unsigned int fg_offset, curr_out; */

	Intr_Number_to_Name(intr_name, intr_no);

	/* Set Read Latchdata */
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 1);
	m = 0;
		while (fg_get_data_ready_status() == 0) {
			m++;
			if (m > 1000) {
				bm_err(
				"[read_fg_hw_info] fg_get_data_ready_status timeout 1 !\r\n");
				break;
			}
		}

	/* Current_1 */
	read_fg_hw_info_current_1(gauge_dev);

	/* Current_2 */
	read_fg_hw_info_current_2(gauge_dev);

	/* curr_out = pmic_get_register_value(PMIC_FG_CURRENT_OUT); */
	/* fg_offset = pmic_get_register_value(PMIC_FG_OFFSET); */

	/* Iavg */
	read_fg_hw_info_Iavg(gauge_dev, &is_iavg_valid);
	if ((is_iavg_valid == 1) && (gauge_status->iavg_intr_flag == 0)) {
		bm_trace("[read_fg_hw_info]set first fg_set_iavg_intr %d %d\n",
			is_iavg_valid, gauge_status->iavg_intr_flag);
		gauge_status->iavg_intr_flag = 1;
		iavg_th = gauge_dev->fg_cust_data->diff_iavg_th;
		ret = fg_set_iavg_intr(gauge_dev, &iavg_th);
	} else if (is_iavg_valid == 0) {
		gauge_status->iavg_intr_flag = 0;
		gauge_enable_interrupt(FG_IAVG_H_NO, 0);
		gauge_enable_interrupt(FG_IAVG_L_NO, 0);
		bm_trace(
			"[read_fg_hw_info] doublecheck first fg_set_iavg_intr %d %d\n",
			is_iavg_valid, gauge_status->iavg_intr_flag);
	}
	bm_trace("[read_fg_hw_info] thirdcheck first fg_set_iavg_intr %d %d\n",
		is_iavg_valid, gauge_status->iavg_intr_flag);

	/* Ncar */
	read_fg_hw_info_ncar(gauge_dev);

	/* recover read */
	pmic_set_register_value(PMIC_FG_SW_CLEAR, 1);
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 0);
	m = 0;
		while (fg_get_data_ready_status() != 0) {
			m++;
			if (m > 1000) {
				bm_err(
					"[read_fg_hw_info] data_ready_status timeout 2 !\r\n");
				break;
			}
		}
	pmic_set_register_value(PMIC_FG_SW_CLEAR, 0);


	fgauge_get_coulomb(gauge_dev, &gauge_dev->fg_hw_info.car);
	fgauge_get_time(gauge_dev, &time);
	gauge_dev->fg_hw_info.time = time;


	bm_err("[FGADC_intr_end][%s][read_fg_hw_info] curr_1 %d curr_2 %d Iavg %d sign %d car %d ncar %d time %d\n",
		intr_name, gauge_dev->fg_hw_info.current_1,
		gauge_dev->fg_hw_info.current_2,
		gauge_dev->fg_hw_info.current_avg,
		gauge_dev->fg_hw_info.current_avg_sign,
		gauge_dev->fg_hw_info.car,
		gauge_dev->fg_hw_info.ncar, gauge_dev->fg_hw_info.time);

	/* gauge_IP_debug(); */
	return 0;

}

int fgauge_enable_bat_plugout_interrupt(
	struct gauge_device *gauge_dev,
	int en)
{
	if (en == 0)
		gauge_enable_interrupt(FG_BAT_PLUGOUT_NO, 0);
	else
		gauge_enable_interrupt(FG_BAT_PLUGOUT_NO, 1);
	return 0;
}

int fgauge_enable_iavg_interrupt(
	struct gauge_device *gauge_dev,
	bool ht_en, int ht_th,
	bool lt_en, int lt_th)
{
	gauge_enable_interrupt(FG_IAVG_H_NO, ht_en);
	gauge_enable_interrupt(FG_IAVG_L_NO, lt_en);

	return 0;
}

int vbat2_debug_dump(void)
{
	bm_err("[%s]thd:[%d %d]prd:%d debt:[%d %d],L[%d %d %d]H[%d %d %d]en[l:%d h:%d]en:%d\n",
		__func__,
		pmic_get_register_value(PMIC_AUXADC_LBAT2_VOLT_MIN),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_VOLT_MAX),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DET_PRD_SEL),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DEBT_MIN_SEL),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DEBT_MAX_SEL),
		pmic_get_register_value(PMIC_RG_INT_EN_BAT2_L),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MIN),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DET_MIN),
		pmic_get_register_value(PMIC_RG_INT_EN_BAT2_H),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MAX),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DET_MAX),
		gvbat2_low_en, gvbat2_high_en,
		pmic_get_register_value(PMIC_AUXADC_LBAT2_EN));


	return 0;
}

int enable_lbat2_en(void)
{
	if (gvbat2_low_en == true || gvbat2_high_en == true)
		pmic_set_register_value(PMIC_AUXADC_LBAT2_EN, 1);

	if (gvbat2_low_en == false && gvbat2_high_en == false)
		pmic_set_register_value(PMIC_AUXADC_LBAT2_EN, 0);

	return 0;
}

int fgauge_enable_vbat_low_interrupt(struct gauge_device *gauge_dev, int en)
{
	gauge_enable_interrupt(FG_RG_INT_EN_BAT2_L, en);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MIN, en);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_MIN, en);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_EN, en);

	gvbat2_low_en = en;
	enable_lbat2_en();

	vbat2_debug_dump();
	return 0;
}

int fgauge_enable_vbat_high_interrupt(struct gauge_device *gauge_dev, int en)
{
	gauge_enable_interrupt(FG_RG_INT_EN_BAT2_H, en);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_IRQ_EN_MAX, en);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_MAX, en);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_EN, en);

	gvbat2_high_en = en;
	enable_lbat2_en();

	vbat2_debug_dump();
	return 0;
}


static void switch_vbat2_det_time(int _prd, int *value)
{
	if (_prd >= 1 && _prd < 3) {
		/* 1s detect 1 time */
		*value = 0;
	} else if (_prd >= 3 && _prd < 5) {
		/* 3s detect 1 time */
		*value = 1;
	} else if (_prd >= 5 && _prd < 10) {
		/* 5s detect 1 time */
		*value = 2;
	} else if (_prd >= 10) {
		/* 10s detect 1 time */
		*value = 3;
	}
}

static void switch_vbat2_debt_counter(int _prd, int *value)
{
	if (_prd >= 1 && _prd < 2) {
		/* debounce 1 time */
		*value = 0;
	} else if (_prd >= 2 && _prd < 4) {
		/* debounce 2 times */
		*value = 1;
	} else if (_prd >= 4 && _prd < 8) {
		/* debounce 4 times */
		*value = 2;
	} else if (_prd >= 8) {
		/* debounce 8 times */
		*value = 3;
	}
}

int (*gauge_enable_vbat_low_threshold)
	(struct gauge_device *gauge_dev, int threshold);
int (*gauge_enable_vbat_high_threshold)
	(struct gauge_device *gauge_dev, int threshold);

int fgauge_set_vbat_low_threshold(
	struct gauge_device *gauge_dev,
	int threshold)
{
	int vbat2_l_th_mv =  threshold;
	int vbat2_l_th_reg = MV_to_REG_12_value(vbat2_l_th_mv);
	int vbat2_det_counter = gauge_dev->fg_cust_data->vbat2_det_counter;
	int vbat2_det_time = gauge_dev->fg_cust_data->vbat2_det_time;

	switch_vbat2_det_time(
		gauge_dev->fg_cust_data->vbat2_det_time,
		&vbat2_det_time);

	switch_vbat2_debt_counter(
		gauge_dev->fg_cust_data->vbat2_det_counter,
		&vbat2_det_counter);

	pmic_set_register_value(PMIC_AUXADC_LBAT2_VOLT_MIN, vbat2_l_th_reg);

	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_PRD_SEL,
				vbat2_det_time);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DEBT_MIN_SEL,
				vbat2_det_counter);

	bm_err("[fg_set_vbat2_l_th] thr:%d [0x%x %d 0x%x %d 0x%x] get [0x%x 0x%x 0x%x]\n",
		threshold,
		vbat2_l_th_reg,
		gauge_dev->fg_cust_data->vbat2_det_time, vbat2_det_time,
		gauge_dev->fg_cust_data->vbat2_det_counter, vbat2_det_counter,
		pmic_get_register_value(PMIC_AUXADC_LBAT2_VOLT_MIN),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DET_PRD_SEL),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DEBT_MIN_SEL));

	return 0;
}


int fgauge_set_vbat_high_threshold(
	struct gauge_device *gauge_dev,
	int threshold)
{
	int vbat2_h_th_mv =  threshold;
	int vbat2_h_th_reg = MV_to_REG_12_value(vbat2_h_th_mv);
	int vbat2_det_counter = gauge_dev->fg_cust_data->vbat2_det_counter;
	int vbat2_det_time = gauge_dev->fg_cust_data->vbat2_det_time;


	switch_vbat2_det_time(
		gauge_dev->fg_cust_data->vbat2_det_time,
		&vbat2_det_time);

	switch_vbat2_debt_counter(
		gauge_dev->fg_cust_data->vbat2_det_counter,
		&vbat2_det_counter);

	pmic_set_register_value(PMIC_AUXADC_LBAT2_VOLT_MAX, vbat2_h_th_reg);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DET_PRD_SEL, vbat2_det_time);
	pmic_set_register_value(PMIC_AUXADC_LBAT2_DEBT_MAX_SEL,
				vbat2_det_counter);

	bm_debug("[fg_set_vbat2_h_th] thr:%d [0x%x %d 0x%x %d 0x%x] get [0x%x 0x%x 0x%x]\n",
		threshold, vbat2_h_th_reg,
		gauge_dev->fg_cust_data->vbat2_det_time, vbat2_det_time,
		gauge_dev->fg_cust_data->vbat2_det_counter, vbat2_det_counter,
		pmic_get_register_value(PMIC_AUXADC_LBAT2_VOLT_MAX),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DET_PRD_SEL),
		pmic_get_register_value(PMIC_AUXADC_LBAT2_DEBT_MAX_SEL));

	return 0;
}

static signed int fgauge_get_AUXADC_current_rawdata(unsigned short *uvalue16)
{
	int m;

	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 1);

	m = 0;
	while (fg_get_data_ready_status() == 0) {
		m++;
		if (m > 1000) {
			bm_err(
			"[%s] fg_get_data_ready_sta timeout 1\r\n", __func__);
			break;
		}
	}

	*uvalue16 = pmic_get_register_value(PMIC_FG_CURRENT_OUT);

	pmic_set_register_value(PMIC_FG_SW_CLEAR, 1);
	pmic_set_register_value(PMIC_FG_SW_READ_PRE, 0);

	m = 0;
	while (fg_get_data_ready_status() != 0) {
		m++;
		if (m > 1000) {
			bm_err(
			 "[%s] fg_get_data_ready_sta timeout 2\r\n", __func__);
			break;
		}
	}

	pmic_set_register_value(PMIC_FG_SW_CLEAR, 0);

	return 0;
}

static int fgauge_enable_car_tune_value_calibration(
	struct gauge_device *gauge_dev,
	int meta_input_cali_current,
	int *car_tune_value)
{
	int cali_car_tune;
	long long sum_all = 0;
	long long temp_sum = 0;
	int	avg_cnt = 0;
	int i;
	unsigned short uvalue16;
	unsigned int uvalue32;
	signed int dvalue = 0;
	long long Temp_Value1 = 0;
	long long Temp_Value2 = 0;
	long long current_from_ADC = 0;

	if (meta_input_cali_current != 0) {
		for (i = 0; i < CALI_CAR_TUNE_AVG_NUM; i++) {
			if (!fgauge_get_AUXADC_current_rawdata(&uvalue16)) {
				uvalue32 = (unsigned int) uvalue16;
				if (uvalue32 <= 0x8000) {
					Temp_Value1 = (long long)uvalue32;
					bm_err("[111]uvalue16 %d uvalue32 %d Temp_Value1 %lld\n",
						uvalue16, uvalue32,
						Temp_Value1);
				} else if (uvalue32 > 0x8000) {
					Temp_Value1 =
						(long long) (65535 - uvalue32);
					bm_err("[222]uvalue16 %d uvalue32 %d Temp_Value1 %lld\n",
						uvalue16, uvalue32,
						Temp_Value1);
				}
				sum_all += Temp_Value1;
				avg_cnt++;
				/*****************/
				bm_err("[333]uvalue16 %d uvalue32 %d Temp_Value1 %lld sum_all %lld\n",
					uvalue16, uvalue32,
					Temp_Value1, sum_all);
				/*****************/
			}
			mdelay(30);
		}
		/*calculate the real world data    */
		/*current_from_ADC = sum_all / avg_cnt;*/
		temp_sum = sum_all;
		bm_err("[444]sum_all %lld temp_sum %lld avg_cnt %d current_from_ADC %lld\n",
			sum_all, temp_sum, avg_cnt, current_from_ADC);

		if (avg_cnt != 0) {
#if defined(__LP64__) || defined(_LP64)
			do_div(temp_sum, avg_cnt);
#else
			temp_sum = div_s64(temp_sum, avg_cnt);
#endif
		}

		current_from_ADC = temp_sum;

		bm_err("[555]sum_all %lld temp_sum %lld avg_cnt %d current_from_ADC %lld\n",
			sum_all, temp_sum, avg_cnt, current_from_ADC);

		Temp_Value2 = current_from_ADC * UNIT_FGCURRENT;

		bm_err("[555]Temp_Value2 %lld current_from_ADC %lld UNIT_FGCURRENT %d\n",
			Temp_Value2, current_from_ADC, UNIT_FGCURRENT);

		/* Move 100 from denominator to cali_car_tune's numerator */
		/*do_div(Temp_Value2, 1000000);*/
#if defined(__LP64__) || defined(_LP64)
		do_div(Temp_Value2, 10000);
#else
		Temp_Value2 = div_s64(Temp_Value2, 10000);
#endif

		bm_err("[666]Temp_Value2 %lld current_from_ADC %lld UNIT_FGCURRENT %d\n",
			Temp_Value2, current_from_ADC, UNIT_FGCURRENT);

		dvalue = (unsigned int) Temp_Value2;

		/* Auto adjust value */
		if (gauge_dev->fg_cust_data->r_fg_value != DEFAULT_R_FG)
			dvalue = (dvalue * DEFAULT_R_FG) /
				gauge_dev->fg_cust_data->r_fg_value;

		bm_err("[666]dvalue %d fg_cust_data.r_fg_value %d\n",
			dvalue, gauge_dev->fg_cust_data->r_fg_value);

		/* Move 100 from denominator to cali_car_tune's numerator */
		/*cali_car_tune = meta_input_cali_current * 1000 / dvalue;*/

		if (dvalue != 0) {
			cali_car_tune =
				meta_input_cali_current * 1000 * 100 / dvalue;

		bm_err("[777]dvalue %d fg_cust_data.r_fg_value %d cali_car_tune %d\n",
			dvalue, gauge_dev->fg_cust_data->r_fg_value,
			cali_car_tune);

		*car_tune_value = cali_car_tune;

		bm_err(
			"[fgauge_meta_cali_car_tune_value][%d] meta:%d, adc:%lld, UNI_FGCUR:%d, r_fg_value:%d\n",
			cali_car_tune, meta_input_cali_current,
			current_from_ADC, UNIT_FGCURRENT,
			gauge_dev->fg_cust_data->r_fg_value);
		}

		return 0;
	}

	return 0;
}

static int fgauge_set_rtc_ui_soc(
	struct gauge_device *gauge_dev,
	int rtc_ui_soc)
{
	int spare3_reg = get_rtc_spare_fg_value();
	int spare3_reg_valid;
	int new_spare3_reg;

	spare3_reg_valid = (spare3_reg & 0x80);
	new_spare3_reg = spare3_reg_valid + rtc_ui_soc;

	/* set spare3 0x7f */
	set_rtc_spare_fg_value(new_spare3_reg);

	bm_notice("[fg_set_rtc_ui_soc] rtc_ui_soc %d spare3_reg 0x%x new_spare3_reg 0x%x\n",
		rtc_ui_soc, spare3_reg, new_spare3_reg);

	return 0;
}

static int fgauge_get_rtc_ui_soc(struct gauge_device *gauge_dev, int *ui_soc)
{
	int spare3_reg = get_rtc_spare_fg_value();
	int rtc_ui_soc;

	rtc_ui_soc = (spare3_reg & 0x7f);

	*ui_soc = rtc_ui_soc;
	bm_notice("[%s] rtc_ui_soc %d spare3_reg 0x%x\n",
		__func__, rtc_ui_soc, spare3_reg);

	return 0;
}

int fgauge_is_rtc_invalid(struct gauge_device *gauge_dev, int *invalid)
{
	/* DON'T get spare3_reg_valid here */
	/* because it has been reset by fg_set_fg_reset_rtc_status() */

	*invalid = rtc_invalid;
	bm_notice("[fg_get_rtc_invalid] rtc_invalid %d\n", rtc_invalid);

	return 0;
}

int fgauge_set_reset_status(struct gauge_device *gauge_dev, int reset)
{
	int hw_id = pmic_get_register_value(PMIC_HWCID);
	int temp_value;
	int spare0_reg, after_rst_spare0_reg;
	int spare3_reg, after_rst_spare3_reg;

	fgauge_read_RTC_boot_status();

	/* read spare0 */
	spare0_reg = get_rtc_spare0_fg_value();

	/* raise 15b to reset */
	if ((hw_id & 0xff00) == 0x3500) {
		temp_value = 0x80;
		set_rtc_spare0_fg_value(temp_value);
		mdelay(1);
		temp_value = 0x00;
		set_rtc_spare0_fg_value(temp_value);
	} else {
		temp_value = 0x80;
		set_rtc_spare0_fg_value(temp_value);
		mdelay(1);
		temp_value = 0x20;
		set_rtc_spare0_fg_value(temp_value);
	}

	/* read spare0 again */
	after_rst_spare0_reg = get_rtc_spare0_fg_value();


	/* read spare3 */
	spare3_reg = get_rtc_spare_fg_value();

	/* set spare3 0x7f */
	set_rtc_spare_fg_value(spare3_reg | 0x80);

	/* read spare3 again */
	after_rst_spare3_reg = get_rtc_spare_fg_value();

	bm_err("[fgauge_read_RTC_boot_status] spare0 0x%x 0x%x, spare3 0x%x 0x%x\n",
		spare0_reg, after_rst_spare0_reg, spare3_reg,
		after_rst_spare3_reg);

	return 0;

}

static void fgauge_dump_type0(struct seq_file *m)
{
	int vbif28;

	if (m != NULL) {
		seq_puts(m, "fgauge dump\n");
		seq_printf(m, "AUXADC_ADC_RDY_LBAT2 :%x\n",
			pmic_get_register_value(PMIC_AUXADC_ADC_RDY_LBAT2));
		seq_printf(m, "AUXADC_ADC_OUT_LBAT2  :%x\n",
			pmic_get_register_value(PMIC_AUXADC_ADC_OUT_LBAT2));

		seq_printf(m, "PMIC_AUXADC_LBAT2_VOLT_MIN  :%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT2_VOLT_MIN));
		seq_printf(m, "PMIC_AUXADC_LBAT2_VOLT_MAX:%x\n",
			pmic_get_register_value(PMIC_AUXADC_LBAT2_VOLT_MAX));

		seq_printf(m,
			"1st chr_zcv:%d pmic_zcv:%d %d pmic_in_zcv:%d swocv:%d zcv_from:%d tmp:%d\n",
			charger_zcv_1st, pmic_rdy_1st, pmic_zcv_1st,
			pmic_in_zcv_1st, swocv_1st, zcv_from_1st, zcv_tmp_1st);

		seq_printf(m,
			"chr_zcv:%d pmic_zcv:%d %d pmic_in_zcv:%d swocv:%d zcv_from:%d tmp:%d\n",
			charger_zcv, pmic_rdy, pmic_zcv,
			pmic_in_zcv, swocv, zcv_from, zcv_tmp);
	}

	vbif28 = pmic_get_auxadc_value(AUXADC_LIST_VBIF);

	bm_debug(
		"1st chr_zcv:%d pmic_zcv:%d %d pmic_in_zcv:%d swocv:%d zcv_from:%d tmp:%d\n",
		charger_zcv_1st, pmic_rdy_1st, pmic_zcv_1st, pmic_in_zcv_1st,
		swocv_1st, zcv_from_1st, zcv_tmp_1st);

	bm_debug(
		"chr_zcv:%d pmic_zcv:%d %d pmic_in_zcv:%d swocv:%d zcv_from:%d tmp:%d\n",
		charger_zcv, pmic_rdy, pmic_zcv, pmic_in_zcv,
		swocv, zcv_from, zcv_tmp);

}

static int fgauge_dump(
	struct gauge_device *gauge_dev, struct seq_file *m, int type)
{
	if (type == 0)
		fgauge_dump_type0(m);
	else if (type == 1)
		battery_dump_nag();

	return 0;
}

static int fgauge_get_hw_version(struct gauge_device *gauge_dev)
{
	int hw_id = pmic_get_register_value(PMIC_HWCID);

	bm_debug("[%s]hw_id=%d", __func__, hw_id);

	return GAUGE_HW_V2000;
}

int fgauge_set_battery_cycle_interrupt(
	struct gauge_device *gauge_dev,
	int threshold)
{
	long long car = threshold;
	long long carReg;

	gauge_enable_interrupt(FG_N_CHARGE_L_NO, 0);

#if defined(__LP64__) || defined(_LP64)
	car = car * 100000 / UNIT_CHARGE;
	/* 1000 * 100 */
#else
	car = div_s64(car * 100000, UNIT_CHARGE);
#endif

	if (fg_cust_data.r_fg_value != DEFAULT_R_FG) {
		car = (car * fg_cust_data.r_fg_value);
#if defined(__LP64__) || defined(_LP64)
		do_div(car, DEFAULT_R_FG);
#else
		car = div_s64(car, DEFAULT_R_FG);
#endif
	}

	car = car * 1000;
#if defined(__LP64__) || defined(_LP64)
	do_div(car, fg_cust_data.car_tune_value);
#else
	car = div_s64(car, fg_cust_data.car_tune_value);
#endif

	carReg = car;
	carReg = 0 - carReg;

	pmic_set_register_value(
		PMIC_FG_N_CHARGE_LTH_15_00, (carReg & 0xffff));
	pmic_set_register_value(
		PMIC_FG_N_CHARGE_LTH_31_16, (carReg & 0xffff0000) >> 16);

	bm_err("car:%d carR:%lld r:%lld current:low:0x%x high:0x%x target:low:0x%x high:0x%x",
		threshold, car, carReg,
		pmic_get_register_value(PMIC_FG_NCAR_15_00),
		pmic_get_register_value(PMIC_FG_NCAR_31_16),
		pmic_get_register_value(PMIC_FG_N_CHARGE_LTH_15_00),
		pmic_get_register_value(PMIC_FG_N_CHARGE_LTH_31_16));

	gauge_enable_interrupt(FG_N_CHARGE_L_NO, 1);

	return 0;

}

int nafg_check_corner(struct gauge_device *gauge_dev)
{
	int nag_vbat = 0;
	int setto_cdltv_thr_mv = 0;
	int get_c_dltv_mv = 0;
	int diff = 0;
	signed int NAG_C_DLTV_value;
	signed int NAG_C_DLTV_value_H;
	signed int NAG_C_DLTV_reg_value;
	bool bcheckbit10;
	int nag_zcv = nag_zcv_mv;

	setto_cdltv_thr_mv = nag_c_dltv_mv;

	/*AUXADC_NAG_7*/
	NAG_C_DLTV_value = pmic_get_register_value(
			PMIC_AUXADC_NAG_C_DLTV_15_0);

	/*AUXADC_NAG_8*/
	NAG_C_DLTV_value_H = pmic_get_register_value(
			PMIC_AUXADC_NAG_C_DLTV_26_16);

	bcheckbit10 = NAG_C_DLTV_value_H & 0x0400;

	if (bcheckbit10 == 0)
		NAG_C_DLTV_reg_value = (NAG_C_DLTV_value & 0xffff) +
				((NAG_C_DLTV_value_H & 0x07ff) << 16);
	else
		NAG_C_DLTV_reg_value = (NAG_C_DLTV_value & 0xffff) +
			(((NAG_C_DLTV_value_H | 0xf800) & 0xffff) << 16);

	get_c_dltv_mv = REG_to_MV_value(NAG_C_DLTV_reg_value);
	fgauge_get_nag_vbat(gauge_dev, &nag_vbat);

	g_nag_corner = 0;

	if (nag_vbat < 31500 && nag_zcv > 31500)
		g_nag_corner = 1;
	else if (nag_zcv < 31500 && nag_vbat > 31500)
		g_nag_corner = 2;
	else
		g_nag_corner = 0;


	bm_err("%s:corner:%d nag_vbat:%d nag_zcv:%d get_c_dltv_mv:%d setto_cdltv_thr_mv:%d, diff:%d, RG[0x%x,0x%x]\n",
		__func__, g_nag_corner, nag_vbat, nag_zcv, get_c_dltv_mv,
		setto_cdltv_thr_mv, diff,
		NAG_C_DLTV_value_H, NAG_C_DLTV_value);

	return 0;
}



int fgauge_notify_event(
	struct gauge_device *gauge_dev,
	enum gauge_event evt, int value)
{

	if (evt == EVT_INT_NAFG_CHECK)
		nafg_check_corner(gauge_dev);

	return 0;
}

static struct gauge_ops mt6359_gauge_ops = {
	.gauge_initial = fgauge_initial,
	.gauge_read_current = fgauge_read_current,
	.gauge_get_average_current = fgauge_get_average_current,
	.gauge_get_coulomb = fgauge_get_coulomb,
	.gauge_reset_hw = fgauge_reset_hw,
	.gauge_get_hwocv = read_hw_ocv,
	.gauge_set_coulomb_interrupt1_ht = fgauge_set_coulomb_interrupt1_ht,
	.gauge_set_coulomb_interrupt1_lt = fgauge_set_coulomb_interrupt1_lt,
	.gauge_get_boot_battery_plug_out_status =
		fgauge_read_boot_battery_plug_out_status,
	.gauge_get_ptim_current = fgauge_get_ptim_current,
	.gauge_get_zcv_current = fgauge_get_zcv_current,
	.gauge_get_zcv = fgauge_get_zcv,
	.gauge_is_gauge_initialized = fgauge_is_gauge_initialized,
	.gauge_set_gauge_initialized = fgauge_set_gauge_initialized,
	.gauge_set_battery_cycle_interrupt =
		fgauge_set_battery_cycle_interrupt,
	.gauge_reset_shutdown_time = fgauge_reset_shutdown_time,
	.gauge_reset_ncar = fgauge_reset_ncar,
	.gauge_set_nag_zcv = fgauge_set_nag_zcv,
	.gauge_set_nag_c_dltv = fgauge_set_nag_c_dltv,
	.gauge_enable_nag_interrupt = fgauge_enable_nag_interrupt,
	.gauge_get_nag_cnt = fgauge_get_nag_cnt,
	.gauge_get_nag_dltv = fgauge_get_nag_dltv,
	.gauge_get_nag_c_dltv = fgauge_get_nag_c_dltv,
	.gauge_get_nag_vbat = fgauge_get_nag_vbat,
	.gauge_enable_zcv_interrupt = fgauge_enable_zcv_interrupt,
	.gauge_set_zcv_interrupt_threshold = fgauge_set_zcv_interrupt_threshold,
	.gauge_enable_battery_tmp_lt_interrupt =
		fgauge_enable_battery_tmp_lt_interrupt,
	.gauge_enable_battery_tmp_ht_interrupt =
		fgauge_enable_battery_tmp_ht_interrupt,
	.gauge_get_time = fgauge_get_time,
	.gauge_set_time_interrupt = fgauge_set_time_interrupt,
	.gauge_get_hw_status = fgauge_get_hw_status,
	.gauge_enable_bat_plugout_interrupt =
		fgauge_enable_bat_plugout_interrupt,
	.gauge_enable_iavg_interrupt = fgauge_enable_iavg_interrupt,
	.gauge_enable_vbat_low_interrupt = fgauge_enable_vbat_low_interrupt,
	.gauge_enable_vbat_high_interrupt = fgauge_enable_vbat_high_interrupt,
	.gauge_set_vbat_low_threshold = fgauge_set_vbat_low_threshold,
	.gauge_set_vbat_high_threshold = fgauge_set_vbat_high_threshold,
	.gauge_enable_car_tune_value_calibration =
		fgauge_enable_car_tune_value_calibration,
	.gauge_set_rtc_ui_soc = fgauge_set_rtc_ui_soc,
	.gauge_get_rtc_ui_soc = fgauge_get_rtc_ui_soc,
	.gauge_is_rtc_invalid = fgauge_is_rtc_invalid,
	.gauge_set_reset_status = fgauge_set_reset_status,
	.gauge_dump = fgauge_dump,
	.gauge_get_hw_version = fgauge_get_hw_version,
	.gauge_set_info = fgauge_set_info,
	.gauge_get_info = fgauge_get_info,
	.gauge_notify_event = fgauge_notify_event,
};

static int mt6359_parse_dt(struct mt6359_gauge *info, struct device *dev)
{
	struct device_node *np = dev->of_node;

	bm_err("%s: starts\n", __func__);

	if (!np) {
		bm_err("%s: no device node\n", __func__);
		return -EINVAL;
	}

	if (of_property_read_string(np, "gauge_name",
		&info->gauge_dev_name) < 0) {
		bm_err("%s: no charger name\n", __func__);
		info->gauge_dev_name = "gauge";
	}

	return 0;
}

/* ============================================================ */
/* alarm timer handler */
/* ============================================================ */
static enum alarmtimer_restart zcv_timer_callback(
	struct alarm *alarm, ktime_t now)
{
	bm_err("%s: enable PMIC_RG_INT_EN_FG_ZCV\n", __func__);
	pmic_set_register_value(PMIC_RG_INT_EN_FG_ZCV, 1);
	return ALARMTIMER_NORESTART;
}

static int mt6359_gauge_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mt6359_gauge *info;

	bm_err("%s: starts\n", __func__);

	info = devm_kzalloc(
		&pdev->dev, sizeof(struct mt6359_gauge), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mt6359_parse_dt(info, &pdev->dev);
	platform_set_drvdata(pdev, info);

	/* Register charger device */
	info->gauge_dev = gauge_device_register(info->gauge_dev_name,
		&pdev->dev, info, &mt6359_gauge_ops, &info->gauge_prop);
	if (IS_ERR_OR_NULL(info->gauge_dev)) {
		ret = PTR_ERR(info->gauge_dev);
		goto err_register_gauge_dev;
	}

	info->gauge_dev->driver_data = info;

	alarm_init(&info->zcv_timer, ALARM_BOOTTIME,
		zcv_timer_callback);


	return 0;
err_register_gauge_dev:
	devm_kfree(&pdev->dev, info);
	return ret;

}

static int mt6359_gauge_remove(struct platform_device *pdev)
{
	struct mt6359_gauge *mt = platform_get_drvdata(pdev);

	if (mt)
		devm_kfree(&pdev->dev, mt);
	return 0;
}

static void mt6359_gauge_shutdown(struct platform_device *dev)
{
}


static const struct of_device_id mt6359_gauge_of_match[] = {
	{.compatible = "mediatek,mt6359_gauge",},
	{},
};

MODULE_DEVICE_TABLE(of, mt6359_gauge_of_match);

static struct platform_driver mt6359_gauge_driver = {
	.probe = mt6359_gauge_probe,
	.remove = mt6359_gauge_remove,
	.shutdown = mt6359_gauge_shutdown,
	.driver = {
		   .name = "mt6359_gauge",
		   .of_match_table = mt6359_gauge_of_match,
		   },
};

static int __init mt6359_gauge_init(void)
{
	return platform_driver_register(&mt6359_gauge_driver);
}
device_initcall(mt6359_gauge_init);

static void __exit mt6359_gauge_exit(void)
{
	platform_driver_unregister(&mt6359_gauge_driver);
}
module_exit(mt6359_gauge_exit);

MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Gauge Device Driver");
MODULE_LICENSE("GPL");
