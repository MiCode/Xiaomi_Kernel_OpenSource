// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <mt-plat/upmu_common.h>

#define VOLTAGE_FULL_RANGE 1200
#define ADC_PRECISE 1024

/* ADC resistor definition */
static int R_BAT_SENSE = 4;
static int R_I_SENSE = 4;
static int R_CHARGER_1 = 330;
static int R_CHARGER_2 = 39;
static int Enable_AUXADC_LOG;

static DEFINE_MUTEX(pmic_adc_mutex);

/* =====================================
 */
/* PMIC-AUXADC */
/* =====================================
 */

int PMIC_IMM_GetOneChannelValue(unsigned int dwChannel,
				int deCount, int trimd)
{
	signed int u4Sample_times = 0;
	signed int u4channel[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	signed int adc_result = 0;
	signed int adc_result_temp = 0;
	signed int r_val_temp = 0;
	signed int count = 0;
	signed int count_time_out = 1000;
	signed int ret_data = 0;

	Enable_AUXADC_LOG = 0;
	mutex_lock(&pmic_adc_mutex);

	if (dwChannel == 1) {
		upmu_set_reg_value(0x0020, 0x0801);
		upmu_set_rg_source_ch0_norm_sel(1);
		upmu_set_rg_source_ch0_lbat_sel(1);
		dwChannel = 0;
		mdelay(1);
	}

	/*
	 *  0 : V_BAT
	 *  1 : V_I_Sense
	 *  2 : V_Charger
	 *  3 : V_TBAT
	 *  4~7 : reserved
	 */
	upmu_set_rg_auxadc_chsel(dwChannel);

	/* upmu_set_rg_avg_num(0x3); */

	if (dwChannel == 3) {
		upmu_set_rg_buf_pwd_on(1);
		upmu_set_rg_buf_pwd_b(1);
		upmu_set_baton_tdet_en(1);
		msleep(20);
	}

	if (dwChannel == 4) {
		upmu_set_rg_vbuf_en(1);
		upmu_set_rg_vbuf_byp(0);

		if (trimd == 2) {
			upmu_set_rg_vbuf_calen(0); /* For T_PMIC */
			upmu_set_rg_spl_num(0x10);
			/* upmu_set_rg_spl_num(0x1E); */
			/* upmu_set_rg_avg_num(0x6); */
			trimd = 1;
		} else {
			upmu_set_rg_vbuf_calen(1); /* For T_BAT */
		}
	}
	if (dwChannel == 5) {
#ifdef CONFIG_MTK_ACCDET
		accdet_auxadc_switch(1);
#endif
	}
	u4Sample_times = 0;

	do {
		upmu_set_rg_auxadc_start(0);
		upmu_set_rg_auxadc_start(1);

		/* Duo to HW limitation */
		udelay(30);

		count = 0;
		ret_data = 0;

		switch (dwChannel) {
		case 0:
			while (upmu_get_rg_adc_rdy_c0() != 1) {
				if ((count++) > count_time_out) {
					pr_debug(
						"[Power/PMIC][IMM_GetOneChannelValue_PMIC] ");
					pr_debug("(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c0_trim();
			else
				ret_data = upmu_get_rg_adc_out_c0();

			break;
		case 1:
			while (upmu_get_rg_adc_rdy_c1() != 1) {
				if ((count++) > count_time_out) {
					pr_debug(
						"[Power/PMIC][IMM_GetOneChannelValue_PMIC] ");
					pr_debug("(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c1_trim();
			else
				ret_data = upmu_get_rg_adc_out_c1();

			break;
		case 2:
			while (upmu_get_rg_adc_rdy_c2() != 1) {
				if ((count++) > count_time_out) {
					pr_debug(
						"[Power/PMIC][IMM_GetOneChannelValue_PMIC] ");
					pr_debug("(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c2_trim();
			else
				ret_data = upmu_get_rg_adc_out_c2();

			break;
		case 3:
			while (upmu_get_rg_adc_rdy_c3() != 1) {
				if ((count++) > count_time_out) {
					pr_debug(
						"[Power/PMIC][IMM_GetOneChannelValue_PMIC] ");
					pr_debug("(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c3_trim();
			else
				ret_data = upmu_get_rg_adc_out_c3();

			break;
		case 4:
			while (upmu_get_rg_adc_rdy_c4() != 1) {
				if ((count++) > count_time_out) {
					pr_debug(
						"[Power/PMIC][IMM_GetOneChannelValue_PMIC] ");
					pr_debug("(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c4_trim();
			else
				ret_data = upmu_get_rg_adc_out_c4();

			break;
		case 5:
			while (upmu_get_rg_adc_rdy_c5() != 1) {
				if ((count++) > count_time_out) {
					pr_debug(
						"[Power/PMIC][IMM_GetOneChannelValue_PMIC] ");
					pr_debug("(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c5_trim();
			else
				ret_data = upmu_get_rg_adc_out_c5();

			break;
		case 6:
			while (upmu_get_rg_adc_rdy_c6() != 1) {
				if ((count++) > count_time_out) {
					pr_debug(
						"[Power/PMIC][IMM_GetOneChannelValue_PMIC] ");
					pr_debug("(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c6_trim();
			else
				ret_data = upmu_get_rg_adc_out_c6();

			break;
		case 7:
			while (upmu_get_rg_adc_rdy_c7() != 1) {
				if ((count++) > count_time_out) {
					pr_debug(
						"[Power/PMIC][IMM_GetOneChannelValue_PMIC] ");
					pr_debug("(%d) Time out!\n", dwChannel);
					break;
				}
			}
			if (trimd == 1)
				ret_data = upmu_get_rg_adc_out_c7_trim();
			else
				ret_data = upmu_get_rg_adc_out_c7();

			break;
		default:
			pr_debug("[Power/PMIC][AUXADC] Invalid channel value(%d,%d)\n",
				 dwChannel, trimd);
			mutex_unlock(&pmic_adc_mutex);
			return -1;
		}

		u4channel[dwChannel] += ret_data;

		u4Sample_times++;

		if (Enable_AUXADC_LOG == 1) {
			/* debug */
			pr_debug("[Power/PMIC][AUXADC] u4Sample_times=");
			pr_debug("%d, ret_data=%d, u4channel[%d]=%d.\n",
				 u4Sample_times, ret_data, dwChannel,
				 u4channel[dwChannel]);
		}

	} while (u4Sample_times < deCount);

	/* Value averaging  */
	u4channel[dwChannel] = u4channel[dwChannel] / deCount;
	adc_result_temp = u4channel[dwChannel];

	switch (dwChannel) {

	case 0:
		r_val_temp = R_BAT_SENSE;
		adc_result =
			(adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) /
			ADC_PRECISE;
		break;
	case 1:
		r_val_temp = R_I_SENSE;
		adc_result =
			(adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) /
			ADC_PRECISE;
		break;
	case 2:
		r_val_temp =
			(((R_CHARGER_1 + R_CHARGER_2) * 100) / R_CHARGER_2);
		adc_result =
			(adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) /
			ADC_PRECISE;
		break;
	case 3:
		r_val_temp = 1;
		adc_result =
			(adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) /
			ADC_PRECISE;
		break;
	case 4:
		r_val_temp = 1;
		adc_result =
			(adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) /
			ADC_PRECISE;
		break;
	case 5:
		r_val_temp = 1;
		adc_result =
			(adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) /
			ADC_PRECISE;
		break;
	case 6:
		r_val_temp = 1;
		adc_result =
			(adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) /
			ADC_PRECISE;
		break;
	case 7:
		r_val_temp = 1;
		adc_result =
			(adc_result_temp * r_val_temp * VOLTAGE_FULL_RANGE) /
			ADC_PRECISE;
		break;
	default:
		pr_debug("[Power/PMIC][AUXADC] Invalid channel value(%d,%d)\n",
			 dwChannel, trimd);
		mutex_unlock(&pmic_adc_mutex);
		return -1;
	}

	if (Enable_AUXADC_LOG == 1) {
		/* debug */
		pr_debug("[Power/PMIC][AUXADC] adc_result_temp=%d, adc_result=%d, r_val_temp=%d.\n",
			 adc_result_temp, adc_result, r_val_temp);
	}

	count = 0;

	if (dwChannel == 0) {
		upmu_set_rg_source_ch0_norm_sel(0);
		upmu_set_rg_source_ch0_lbat_sel(0);
	}

	if (dwChannel == 3) {
		upmu_set_baton_tdet_en(0);
		upmu_set_rg_buf_pwd_b(0);
		upmu_set_rg_buf_pwd_on(0);
	}

	if (dwChannel == 4) {
		/* upmu_set_rg_vbuf_en(0); */
		/* upmu_set_rg_vbuf_byp(0); */
		upmu_set_rg_vbuf_calen(0);
	}
	if (dwChannel == 5) {
#ifdef CONFIG_MTK_ACCDET
		accdet_auxadc_switch(0);
#endif
	}

	upmu_set_rg_spl_num(0x1);

	mutex_unlock(&pmic_adc_mutex);

	return adc_result;
}
