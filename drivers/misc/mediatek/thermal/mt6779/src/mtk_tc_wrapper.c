// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/seq_file.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#if defined(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif

#include <tscpu_settings.h>
#if CFG_THERM_LVTS
#include "mtk_idle.h"
#endif

#if CONFIG_LVTS_ERROR_AEE_WARNING
#include <mt-plat/aee.h>
#include <linux/delay.h>
#if DUMP_VCORE_VOLTAGE
#include <linux/regulator/consumer.h>
#endif
struct lvts_error_data {
	int ts_temp[TS_ENUM_MAX][R_BUFFER_SIZE]; /* A ring buffer */
	int ts_temp_r[TS_ENUM_MAX][R_BUFFER_SIZE]; /* A ring buffer */
	int ts_temp_v[TS_ENUM_MAX][R_BUFFER_SIZE]; /* A ring buffer */
#if DUMP_VCORE_VOLTAGE
	int vcore_voltage[R_BUFFER_SIZE]; /* A ring buffer */
#endif
	int c_index; /* Current index points to the space to replace.*/
	int e_occurred; /* 1: An error occurred, 0: Nothing happened*/
	int f_count; /* Future count */
	enum thermal_sensor e_mcu;
	enum thermal_sensor e_lvts;
};
struct lvts_error_data g_lvts_e_data;
int tscpu_ts_mcu_temp_v[L_TS_MCU_NUM];
int tscpu_ts_lvts_temp_v[L_TS_LVTS_NUM];
#endif

int tscpu_ts_temp[TS_ENUM_MAX];
int tscpu_ts_temp_r[TS_ENUM_MAX];

int tscpu_ts_mcu_temp[L_TS_MCU_NUM];
int tscpu_ts_mcu_temp_r[L_TS_MCU_NUM];

#if CFG_THERM_LVTS
int tscpu_ts_lvts_temp[L_TS_LVTS_NUM];
int tscpu_ts_lvts_temp_r[L_TS_LVTS_NUM];
#endif

int tscpu_curr_cpu_temp;
int tscpu_curr_gpu_temp;

static int tscpu_curr_max_ts_temp;

/*
 * PTP#	module		TSMCU Plan
 *  0	MCU_LITTLE	TSMCU-5,6,7
 *  1	MCU_BIG		TSMCU-8,9
 *  2	MCU_CCI		TSMCU-5,6,7
 *  3	MFG(GPU)	TSMCU-4
 *  4	MDLA		TSMCU-1
 *  5	VPU		TSMCU-2
 *  6	TOP		TSMCU-2,3,4
 *  7	MD		TSMCU-0
 */
int get_immediate_none_wrap(void)
{
	return -127000;
}

/* chip dependent */
int get_immediate_cpuL_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(MAX(tscpu_ts_temp[TS_MCU5], tscpu_ts_temp[TS_MCU6]),
		tscpu_ts_temp[TS_MCU7]);

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_cpuB_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(tscpu_ts_temp[TS_MCU8], tscpu_ts_temp[TS_MCU9]);

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_mcucci_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(MAX(tscpu_ts_temp[TS_MCU5], tscpu_ts_temp[TS_MCU6]),
		tscpu_ts_temp[TS_MCU7]);

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_gpu_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU4];

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_mdla_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU1];

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_vpu_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU2];

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_top_wrap(void)
{
	int curr_temp;

	curr_temp = MAX(MAX(tscpu_ts_temp[TS_MCU2], tscpu_ts_temp[TS_MCU3]),
		tscpu_ts_temp[TS_MCU4]);

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_md_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU0];

	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int (*max_temperature_in_bank[THERMAL_BANK_NUM])(void) = {
	get_immediate_cpuL_wrap,
	get_immediate_cpuB_wrap,
	get_immediate_mcucci_wrap,
	get_immediate_gpu_wrap,
	get_immediate_mdla_wrap,
	get_immediate_vpu_wrap,
	get_immediate_top_wrap,
	get_immediate_md_wrap
};

int get_immediate_ts0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts2_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU2];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts3_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU3];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts4_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU4];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts5_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU5];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts6_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU6];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts7_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU7];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts8_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU8];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_ts9_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_MCU9];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

#if CFG_THERM_LVTS
int get_immediate_tslvts1_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_LVTS1_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts1_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_LVTS1_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts2_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_LVTS2_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts2_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_LVTS2_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts2_2_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_LVTS2_2];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts3_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_LVTS3_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts3_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_LVTS3_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts4_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_LVTS4_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts4_1_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_LVTS4_1];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int get_immediate_tslvts9_0_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_LVTS9_0];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}
#endif

int get_immediate_tsabb_wrap(void)
{
	int curr_temp;

	curr_temp = tscpu_ts_temp[TS_ABB];
	tscpu_dprintk("%s curr_temp=%d\n", __func__, curr_temp);

	return curr_temp;
}

int (*get_immediate_tsX[TS_ENUM_MAX])(void) = {
	get_immediate_ts0_wrap, /* TS_MCU0 */
	get_immediate_ts1_wrap, /* TS_MCU1 */
	get_immediate_ts2_wrap, /* TS_MCU2 */
	get_immediate_ts3_wrap, /* TS_MCU3 */
	get_immediate_ts4_wrap, /* TS_MCU4 */
	get_immediate_ts5_wrap, /* TS_MCU5 */
	get_immediate_ts6_wrap, /* TS_MCU6 */
	get_immediate_ts7_wrap, /* TS_MCU7 */
	get_immediate_ts8_wrap, /* TS_MCU8 */
	get_immediate_ts9_wrap, /* TS_MCU9 */
#if CFG_THERM_LVTS
	get_immediate_tslvts1_0_wrap,	/* LVTS1-0 */
	get_immediate_tslvts1_1_wrap,	/* LVTS1-1 */
	get_immediate_tslvts2_0_wrap,	/* LVTS2-0 */
	get_immediate_tslvts2_1_wrap,	/* LVTS2-1 */
	get_immediate_tslvts2_2_wrap,	/* LVTS2-2 */
	get_immediate_tslvts3_0_wrap,	/* LVTS3-0 */
	get_immediate_tslvts3_1_wrap,	/* LVTS3-1 */
	get_immediate_tslvts4_0_wrap,	/* LVTS4-0 */
	get_immediate_tslvts4_1_wrap,	/* LVTS4-1 */
	get_immediate_tslvts9_0_wrap,	/* LVTS9-0 */
#endif
	get_immediate_tsabb_wrap, /* TS_ABB */
};

/**
 * this only returns latest stored max ts temp but not updated from TC.
 */
int tscpu_get_curr_max_ts_temp(void)
{
	return tscpu_curr_max_ts_temp;
}

#if CFG_THERM_LVTS
int tscpu_max_temperature(void)
{
	int i, j, max = 0;

#if CFG_LVTS_DOMINATOR
#if CFG_THERM_LVTS
	tscpu_dprintk("lvts_max_temperature %s, %d\n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(lvts_tscpu_g_tc); i++) {
		for (j = 0; j < lvts_tscpu_g_tc[i].ts_number; j++) {
			if (i == 0 && j == 0) {
				max = tscpu_ts_lvts_temp[
					lvts_tscpu_g_tc[i].ts[j]];
			} else {
				if (max < tscpu_ts_lvts_temp[
						lvts_tscpu_g_tc[i].ts[j]])
					max = tscpu_ts_lvts_temp[
						lvts_tscpu_g_tc[i].ts[j]];
			}
		}
	}
#endif /* CFG_THERM_LVTS */
#else
	tscpu_dprintk("tscpu_get_temp %s, %d\n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(tscpu_g_tc); i++) {
		for (j = 0; j < tscpu_g_tc[i].ts_number; j++) {
			if (i == 0 && j == 0) {
				max = tscpu_ts_mcu_temp[tscpu_g_tc[i].ts[j]];
			} else {
				if (max < tscpu_ts_mcu_temp[
						tscpu_g_tc[i].ts[j]])
					max = tscpu_ts_mcu_temp[
						tscpu_g_tc[i].ts[j]];
			}
		}
	}
#endif /* CFG_LVTS_DOMINATOR */
	return max;
}
#endif

int tscpu_get_curr_temp(void)
{
	tscpu_update_tempinfo();

#if PRECISE_HYBRID_POWER_BUDGET
	/*      update CPU/GPU temp data whenever TZ times out...
	 *       If the update timing is aligned to TZ polling,
	 *       this segment should be moved to TZ code instead of thermal
	 *       controller driver
	 */
#if CFG_LVTS_DOMINATOR
#if CFG_THERM_LVTS
	tscpu_curr_cpu_temp = MAX(tscpu_ts_temp[TS_LVTS2_0],
					tscpu_ts_temp[TS_LVTS1_1]);

	tscpu_curr_gpu_temp = tscpu_ts_temp[TS_LVTS3_1]; /* check it */
#endif /* CFG_THERM_LVTS */
#else
	/* It is platform dependent which TS is better to present CPU/GPU
	 * temperature
	 */
	tscpu_curr_cpu_temp = MAX(tscpu_ts_temp[TS_MCU5],
					tscpu_ts_temp[TS_MCU9]);

	tscpu_curr_gpu_temp = tscpu_ts_temp[TS_MCU4];
#endif /* CFG_LVTS_DOMINATOR */
#endif /* PRECISE_HYBRID_POWER_BUDGET */

	/* though tscpu_max_temperature is common, put it in mtk_ts_cpu.c is
	 * weird.
	 */

	tscpu_curr_max_ts_temp = tscpu_max_temperature();

	return tscpu_curr_max_ts_temp;
}

#if CONFIG_LVTS_ERROR_AEE_WARNING
char mcu_s_array[TS_ENUM_MAX][11] = {
	"TS_MCU0",
	"TS_MCU1",
	"TS_MCU2",
	"TS_MCU3",
	"TS_MCU4",
	"TS_MCU5",
	"TS_MCU6",
	"TS_MCU7",
	"TS_MCU8",
	"TS_MCU9",
#if CFG_THERM_LVTS
	"TS_LVTS1_0",
	"TS_LVTS1_1",
	"TS_LVTS2_0",
	"TS_LVTS2_1",
	"TS_LVTS2_2",
	"TS_LVTS3_0",
	"TS_LVTS3_1",
	"TS_LVTS4_0",
	"TS_LVTS4_1",
	"TS_LVTS9_0",
#endif
	"TS_ABB"
};

static void dump_lvts_error_info(void)
{
	int i, j, index, e_index, offset;
#if DUMP_LVTS_REGISTER
	int cnt, temp;
#endif
	enum thermal_sensor mcu_index, lvts_index;
	char buffer[512];

	mcu_index = g_lvts_e_data.e_mcu;
	lvts_index = g_lvts_e_data.e_lvts;
	index = g_lvts_e_data.c_index;
	e_index = (index + HISTORY_SAMPLES + 1) % R_BUFFER_SIZE;

	tscpu_printk("[LVTS_ERROR][DUMP] %s:%d and %s:%d error: |%d| > %d\n",
		mcu_s_array[mcu_index],
		g_lvts_e_data.ts_temp[mcu_index][e_index],
		mcu_s_array[lvts_index],
		g_lvts_e_data.ts_temp[lvts_index][e_index],
		g_lvts_e_data.ts_temp[mcu_index][e_index] -
		g_lvts_e_data.ts_temp[lvts_index][e_index],
		LVTS_ERROR_THRESHOLD);

	for (i = TS_MCU1; i <= TS_LVTS4_1; i++) {
		offset = sprintf(buffer, "[LVTS_ERROR][%s][DUMP] ",
				mcu_s_array[i]);

		for (j = 0; j < R_BUFFER_SIZE; j++) {
			index = (g_lvts_e_data.c_index + 1 + j)
					% R_BUFFER_SIZE;
			offset += sprintf(buffer + offset, "%d ",
					g_lvts_e_data.ts_temp[i][index]);

		}
		buffer[offset] = '\0';
		tscpu_printk("%s\n", buffer);
	}

	offset = sprintf(buffer, "[LVTS_ERROR][%s_R][DUMP] ",
			mcu_s_array[lvts_index]);

	for (j = 0; j < R_BUFFER_SIZE; j++) {
		index = (g_lvts_e_data.c_index + 1 + j) % R_BUFFER_SIZE;
		offset += sprintf(buffer + offset, "%d ",
			g_lvts_e_data.ts_temp_r[lvts_index][index]);
	}

	buffer[offset] = '\0';
	tscpu_printk("%s\n", buffer);

	offset = sprintf(buffer, "[LVTS_ERROR][%s_V][DUMP] ",
			mcu_s_array[mcu_index]);

	for (j = 0; j < R_BUFFER_SIZE; j++) {
		index = (g_lvts_e_data.c_index + 1 + j) % R_BUFFER_SIZE;
		offset += sprintf(buffer + offset, "%d ",
			g_lvts_e_data.ts_temp_v[mcu_index][index]);
	}
	buffer[offset] = '\0';
	tscpu_printk("%s\n", buffer);

	offset = sprintf(buffer, "[LVTS_ERROR][%s_V][DUMP] ",
			mcu_s_array[lvts_index]);

	for (j = 0; j < R_BUFFER_SIZE; j++) {
		index = (g_lvts_e_data.c_index + 1 + j) % R_BUFFER_SIZE;
		offset += sprintf(buffer + offset, "%d ",
			g_lvts_e_data.ts_temp_v[lvts_index][index]);
	}

	buffer[offset] = '\0';
	tscpu_printk("%s\n", buffer);

	dump_efuse_data();
#if DUMP_LVTS_REGISTER
	read_controller_reg_when_error();

	lvts_thermal_disable_all_periodoc_temp_sensing();
	cnt = 0;
	/* Wait until all sensoring points idled */
	while (cnt < 50) {
		temp = lvts_thermal_check_all_sensing_point_idle();
		if (temp == 0)
			break;

		if ((cnt + 1) % 10 == 0) {
			pr_notice("Cnt = %d LVTS TC %d, LVTSMSRCTL1[10,7,0] = %d,%d,%d\n",
					cnt + 1, (temp >> 16),
					((temp & _BIT_(2)) >> 2),
					((temp & _BIT_(1)) >> 1),
					(temp & _BIT_(0)));
		}

		udelay(2);
		cnt++;
	}

	read_device_reg_when_error();
	dump_lvts_register_value();
#endif
#if DUMP_VCORE_VOLTAGE
	offset = sprintf(buffer, "[LVTS_ERROR][Vcore_V][DUMP] ",
			mcu_s_array[lvts_index]);
	for (j = 0; j < R_BUFFER_SIZE; j++) {
		index = (g_lvts_e_data.c_index + 1 + j) % R_BUFFER_SIZE;
		offset += sprintf(buffer + offset, "%d ",
			g_lvts_e_data.vcore_voltage[index]);
	}
	buffer[offset] = '\0';
	tscpu_printk("%s\n", buffer);
#endif
#if DUMP_LVTS_REGISTER
	lvts_reset_device_and_stop_clk();
#endif
#ifdef CONFIG_MTK_AEE_FEATURE
	aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, __func__,
		"LVTS_ERROR: %s, %s diff: %d\n", mcu_s_array[mcu_index],
		mcu_s_array[lvts_index],
		g_lvts_e_data.ts_temp[mcu_index][e_index] -
		g_lvts_e_data.ts_temp[lvts_index][e_index]);
#endif

	g_lvts_e_data.e_occurred = 0;
	g_lvts_e_data.f_count = -1;
#if DUMP_LVTS_REGISTER
	clear_lvts_register_value_array();

	lvts_device_identification();
	lvts_Device_Enable_Init_all_Devices();
	lvts_device_read_count_RC_N();
	lvts_efuse_setting();

	lvts_thermal_disable_all_periodoc_temp_sensing();
	Enable_LVTS_CTRL_for_thermal_Data_Fetch();
	lvts_tscpu_thermal_initial_all_tc();
#endif
}

static void check_lvts_error(enum thermal_sensor mcu_index,
	enum thermal_sensor lvts_index)
{
	int temp;

	temp = tscpu_ts_temp[mcu_index] - tscpu_ts_temp[lvts_index];

	if (temp < 0)
		temp = temp * -1;

	/*Skip if LVTS thermal controllers doens't ready */
	if (temp > 100000)
		return;

	if (temp > LVTS_ERROR_THRESHOLD) {
		tscpu_printk("[LVTS_ERROR] %s:%d and %s:%d error: |%d| > %d\n",
			mcu_s_array[mcu_index],
			tscpu_ts_temp[mcu_index],
			mcu_s_array[lvts_index],
			tscpu_ts_temp[lvts_index],
			tscpu_ts_temp[mcu_index] -
			tscpu_ts_temp[lvts_index],
			LVTS_ERROR_THRESHOLD);
		g_lvts_e_data.e_occurred = 1;
		g_lvts_e_data.e_mcu = mcu_index;
		g_lvts_e_data.e_lvts = lvts_index;
		g_lvts_e_data.f_count = -1;
	}
}
void dump_lvts_error_data_info(void)
{
	char buffer[512];
	int offset, j;

	tscpu_printk("[LVTS_ERROR] c_index %d, e_occurred %d, f_count %d\n",
			g_lvts_e_data.c_index, g_lvts_e_data.e_occurred,
			g_lvts_e_data.f_count);
	tscpu_printk("[LVTS_ERROR] e_mcu %d, e_lvts %d\n", g_lvts_e_data.e_mcu,
			g_lvts_e_data.e_lvts);

	offset = sprintf(buffer, "[LVTS_ERROR][%s][DUMP] ",
			mcu_s_array[TS_LVTS1_0]);
	for (j = 0; j < R_BUFFER_SIZE; j++) {
		offset += sprintf(buffer + offset, "%d ",
				g_lvts_e_data.ts_temp[TS_LVTS1_0][j]);

	}
	buffer[offset] = '\0';
	tscpu_printk("%s\n", buffer);

	offset = sprintf(buffer, "[LVTS_ERROR][%s_raw][DUMP] ",
			mcu_s_array[TS_LVTS1_0]);
	for (j = 0; j < R_BUFFER_SIZE; j++) {
		offset += sprintf(buffer + offset, "%d ",
				g_lvts_e_data.ts_temp_r[TS_LVTS1_0][j]);

	}
	buffer[offset] = '\0';
	tscpu_printk("%s\n", buffer);
}
#endif

int combine_lvts_tsmcu_temp(void)
{
	int i;
#if CFG_THERM_LVTS
	int j = 0;
#endif

#if CONFIG_LVTS_ERROR_AEE_WARNING
	int temp = g_lvts_e_data.c_index;
#if LVTS_FORCE_ERROR_TRIGGER
	static int f_e_count;
#endif
#if DUMP_VCORE_VOLTAGE
	int vcore_v;
#endif
#if DUMP_VCORE_VOLTAGE
	if (vcore_reg_id)
		vcore_v = regulator_get_voltage(vcore_reg_id);

	g_lvts_e_data.vcore_voltage[temp] = vcore_v;
#endif
#endif

#if CFG_THERM_LVTS
	if (TS_ENUM_MAX == (L_TS_MCU_NUM + L_TS_LVTS_NUM)) {
#else
	if (TS_ENUM_MAX == (L_TS_MCU_NUM + 0)) {
#endif
		for (i = 0 ; i < L_TS_MCU_NUM - 1 ; i++) {
			tscpu_ts_temp[i] = tscpu_ts_mcu_temp[i];
			tscpu_ts_temp_r[i] = tscpu_ts_mcu_temp_r[i];
#if CONFIG_LVTS_ERROR_AEE_WARNING
			g_lvts_e_data.ts_temp[i][temp] = tscpu_ts_mcu_temp[i];
			g_lvts_e_data.ts_temp_r[i][temp] =
				tscpu_ts_mcu_temp_r[i];
			g_lvts_e_data.ts_temp_v[i][temp] =
				tscpu_ts_mcu_temp_v[i];
#endif
			tscpu_dprintk(
				"#%d tscpu_ts_temp=%d, tscpu_ts_temp_r=%d\n",
				i, tscpu_ts_temp[i], tscpu_ts_temp_r[i]);
		}

#if CFG_THERM_LVTS
		for ( ; i < TS_ENUM_MAX - 1 ; i++) {
			tscpu_ts_temp[i] = tscpu_ts_lvts_temp[j];
			tscpu_ts_temp_r[i] = tscpu_ts_lvts_temp_r[j];

#if CONFIG_LVTS_ERROR_AEE_WARNING
			g_lvts_e_data.ts_temp[i][temp] = tscpu_ts_lvts_temp[j];
			g_lvts_e_data.ts_temp_r[i][temp] =
				tscpu_ts_lvts_temp_r[j];
			g_lvts_e_data.ts_temp_v[i][temp] =
				tscpu_ts_lvts_temp_v[j];
#endif
			tscpu_dprintk(
				"#%d tscpu_ts_temp=%d, tscpu_ts_temp_r=%d\n",
				i, tscpu_ts_temp[i], tscpu_ts_temp_r[i]);

			j++;
		}
#endif

		/*
		 * index (TS_ENUM_MAX - 1) = index (L_TS_MCU_NUM - 1) = TS_ABB
		 */
		tscpu_ts_temp[i] = tscpu_ts_mcu_temp[L_TS_MCU_NUM - 1];
		tscpu_ts_temp_r[i] = tscpu_ts_mcu_temp_r[L_TS_MCU_NUM - 1];
#if CONFIG_LVTS_ERROR_AEE_WARNING
		g_lvts_e_data.ts_temp[i][temp] =
			tscpu_ts_mcu_temp[L_TS_MCU_NUM - 1];
		g_lvts_e_data.ts_temp_r[i][temp] =
			tscpu_ts_mcu_temp_r[L_TS_MCU_NUM - 1];
		g_lvts_e_data.ts_temp_v[i][temp] =
			tscpu_ts_mcu_temp_v[L_TS_MCU_NUM - 1];
#endif
		tscpu_dprintk(
			"#%d tscpu_ts_temp=%d, tscpu_ts_temp_r=%d\n",
			i, tscpu_ts_temp[i], tscpu_ts_temp_r[i]);

#if CONFIG_LVTS_ERROR_AEE_WARNING
#if LVTS_FORCE_ERROR_TRIGGER
		f_e_count++;
		if (f_e_count % (LVTS_NUM_SKIP_SAMPLE + 1) == 0) {
			tscpu_ts_temp[TS_MCU1] = 0;
			g_lvts_e_data.ts_temp[TS_MCU1][temp] = 0;
			f_e_count = 0;
		}
#endif
		/* Skip the difference comparison
		 * if one of them doesn't have efuse
		 */
		if (!check_auxadc_mcu_efuse() || !check_lvts_mcu_efuse())
			return 0;

		check_lvts_error(TS_MCU1, TS_LVTS4_0);
		check_lvts_error(TS_MCU2, TS_LVTS4_1);
		check_lvts_error(TS_MCU3, TS_LVTS3_0);
		check_lvts_error(TS_MCU4, TS_LVTS3_1);
		check_lvts_error(TS_MCU5, TS_LVTS2_0);
		check_lvts_error(TS_MCU6, TS_LVTS2_1);
		check_lvts_error(TS_MCU7, TS_LVTS2_2);
		check_lvts_error(TS_MCU8, TS_LVTS1_0);
		check_lvts_error(TS_MCU9, TS_LVTS1_1);

		if (g_lvts_e_data.e_occurred == 1)
			g_lvts_e_data.f_count += 1;

		if (lvts_debug_log)
			dump_lvts_error_data_info();

		if (g_lvts_e_data.f_count == FUTURE_SAMPLES)
			dump_lvts_error_info();

		g_lvts_e_data.c_index = (g_lvts_e_data.c_index + 1)
			% R_BUFFER_SIZE;
#endif
		return 0;
	}

	tscpu_dprintk(
		"%s error !! please check the number of TS\n", __func__);

	return -1;
}

int tscpu_read_temperature_info(struct seq_file *m, void *v)
{
	seq_printf(m, "curr_temp = %d\n", tscpu_get_curr_max_ts_temp());
	tscpu_dump_cali_info(m, v);
#if CFG_THERM_LVTS
	seq_puts(m, "-----------------\n");
	lvts_tscpu_dump_cali_info(m, v);
#endif
	return 0;
}

#if CFG_THERM_LVTS
static int thermal_idle_notify_call(struct notifier_block *nfb,
				unsigned long id,
				void *arg)
{
	switch (id) {
	case NOTIFY_DPIDLE_ENTER:
		break;
	case NOTIFY_SOIDLE_ENTER:
		break;
	case NOTIFY_DPIDLE_LEAVE:
		break;
	case NOTIFY_SOIDLE_LEAVE:
		break;
	case NOTIFY_SOIDLE3_ENTER:
#if LVTS_VALID_DATA_TIME_PROFILING
		SODI3_count++;
		if (SODI3_count != 1 && isTempValid == 0)
			noValid_count++;

		if (isTempValid == 1 || SODI3_count == 1)
			start_timestamp = thermal_get_current_time_us();

		isTempValid = 0;
#endif
		break;
	case NOTIFY_SOIDLE3_LEAVE:
#if CFG_THERM_LVTS
		lvts_sodi3_release_thermal_controller();
#endif
		break;
	default:
		/* do nothing */
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block thermal_idle_nfb = {
	.notifier_call = thermal_idle_notify_call,
};
#endif

#ifdef CONFIG_OF
int get_io_reg_base(void)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,therm_ctrl");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		thermal_base = of_iomap(node, 0);
	}

	/*get thermal irq num */
	thermal_irq_number = irq_of_parse_and_map(node, 0);

	if (!thermal_irq_number) {
		/*TODO: need check "irq number"*/
		tscpu_printk("[THERM_CTRL] get irqnr failed=%d\n",
				thermal_irq_number);
		return 0;
	}

	if (of_property_read_u32_index(node, "reg", 1, &thermal_phy_base)) {
		tscpu_printk("[THERM_CTRL] config error thermal_phy_base\n");
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6779-auxadc");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		auxadc_ts_base = of_iomap(node, 0);
	}

	if (of_property_read_u32_index(node, "reg", 1, &auxadc_ts_phy_base)) {
		tscpu_printk("[THERM_CTRL] config error auxadc_ts_phy_base\n");
		return 0;
	}

	node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt6779-infracfg_ao");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		infracfg_ao_base = of_iomap(node, 0);
#if CFG_THERM_LVTS
		if (infracfg_ao_base)
			mtk_idle_notifier_register(&thermal_idle_nfb);
#endif
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt6779-apmixed");
	WARN_ON_ONCE(node == 0);
	if (node) {
		/* Setup IO addresses */
		th_apmixed_base = of_iomap(node, 0);
	}

	if (of_property_read_u32_index(node, "reg", 1, &apmixed_phy_base)) {
		tscpu_printk("[THERM_CTRL] config error apmixed_phy_base=\n");
		return 0;
	}
#if THERMAL_GET_AHB_BUS_CLOCK
	/* TODO: If this is required, it needs to confirm which node to read. */
	node = of_find_compatible_node(NULL, NULL, "mediatek,infrasys");
	if (!node) {
		pr_err("[CLK_INFRACFG_AO] find node failed\n");
		return 0;
	}
	therm_clk_infracfg_ao_base = of_iomap(node, 0);
	if (!therm_clk_infracfg_ao_base) {
		pr_err("[CLK_INFRACFG_AO] base failed\n");
		return 0;
	}
#endif
	return 1;
}
#endif

/* chip dependent */
int tscpu_thermal_clock_on(void)
{
	int ret = -1;

#if defined(CONFIG_MTK_CLKMGR)
	tscpu_printk("%s\n", __func__);
	/* ret = enable_clock(MT_CG_PERI_THERM, "THERMAL"); */
#else
	/* Use CCF instead */
	tscpu_printk("%s CCF\n", __func__);
	ret = clk_prepare_enable(therm_main);
	if (ret)
		tscpu_printk("Cannot enable thermal clock.\n");
#endif
	return ret;
}

/* chip dependent */
int tscpu_thermal_clock_off(void)
{
	int ret = -1;

#if defined(CONFIG_MTK_CLKMGR)
	tscpu_printk("%s\n", __func__);
	/*ret = disable_clock(MT_CG_PERI_THERM, "THERMAL"); */
#else
	/*Use CCF instead*/
	tscpu_printk("%s CCF\n", __func__);
	clk_disable_unprepare(therm_main);
#endif
	return ret;
}

#if defined(THERMAL_AEE_SELECTED_TS)
int (*get_aee_selected_tsX[THERMAL_AEE_MAX_SELECTED_TS])(void) = {
	get_immediate_ts0_wrap, /* TS_MCU0 */
	get_immediate_ts1_wrap, /* TS_MCU1 */
	get_immediate_ts2_wrap, /* TS_MCU2 */
	get_immediate_ts3_wrap, /* TS_MCU3 */
	get_immediate_ts4_wrap, /* TS_MCU4 */
	get_immediate_ts5_wrap, /* TS_MCU5 */
	get_immediate_ts6_wrap, /* TS_MCU6 */
	get_immediate_ts7_wrap, /* TS_MCU7 */
	get_immediate_ts8_wrap, /* TS_MCU8 */
	get_immediate_ts9_wrap, /* TS_MCU9 */
};
#endif
