/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __TSCPU_SETTINGS_H__
#define __TSCPU_SETTINGS_H__

#include "mach/mtk_thermal.h"

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include "tzcpu_initcfg.h"
#include "clatm_initcfg.h"
#include "tscpu_tsense_config.h"
#include "tscpu_lvts_config.h"

/*=============================================================
 * Genernal
 *=============================================================
 */
#define MIN(_a_, _b_) ((_a_) > (_b_) ? (_b_) : (_a_))
#define MAX(_a_, _b_) ((_a_) > (_b_) ? (_a_) : (_b_))
#define _BIT_(_bit_)		(unsigned int)(1 << (_bit_))
#define _BITMASK_(_bits_)	(((unsigned int) -1 >> (31 - ((1) ? _bits_))) \
						& ~((1U << ((0) ? _bits_)) - 1))

#define THERMAL_TPROFILE_INIT()	\
	long long thermal_pTime_us, thermal_cTime_us, thermal_diff_us

#define THERMAL_GET_PTIME() {thermal_pTime_us = thermal_get_current_time_us()}

#define THERMAL_GET_CTIME() {thermal_cTime_us = thermal_get_current_time_us()}

#define THERMAL_TIME_TH 3000

#define THERMAL_IS_TOO_LONG()   \
	do {                                    \
		thermal_diff_us = thermal_cTime_us - thermal_pTime_us;	\
		if (thermal_diff_us > THERMAL_TIME_TH) {                \
			pr_notice(TSCPU_LOG_TAG "%s: %llu us\n", __func__, \
							thermal_diff_us); \
		} else if (thermal_diff_us < 0) {	\
			pr_notice(TSCPU_LOG_TAG \
				"Warning: tProfiling uses incorrect %s %d\n", \
				__func__, __LINE__); \
		}	\
	} while (0)

/*=============================================================
 * CONFIG (SW related)
 *=============================================================
 */
/*Enable thermal controller CG*/
#define THERMAL_EBABLE_TC_CG

#define ENALBE_UART_LIMIT						(0)
#define TEMP_EN_UART							(80000)
#define TEMP_DIS_UART							(85000)
#define TEMP_TOLERANCE							(0)

#define ENALBE_SW_FILTER						(0)
#define ATM_USES_PPM							(1)

#define THERMAL_GET_AHB_BUS_CLOCK				(0)
#define THERMAL_PERFORMANCE_PROFILE				(0)

/* 1: turn on GPIO toggle monitor; 0: turn off */
#define THERMAL_GPIO_OUT_TOGGLE					(0)

/* 1: turn on adaptive AP cooler; 0: turn off */
#define CPT_ADAPTIVE_AP_COOLER					(1)

/* 1: turn on supports to MET logging; 0: turn off */
#define CONFIG_SUPPORT_MET_MTKTSCPU				(0)

/*
 * Thermal controller HW filtering function.
 * Only 1, 2, 4, 8, 16 are valid values,
 * they means one reading is a avg of X samples
 */
#define THERMAL_CONTROLLER_HW_FILTER			(2) /* 1, 2, 4, 8, 16 */

/* 1: turn on thermal controller HW thermal protection; 0: turn off */
#define THERMAL_CONTROLLER_HW_TP				(1)

/* 1: turn on fast polling in this sw module; 0: turn off */
#define MTKTSCPU_FAST_POLLING					(1)

#if CPT_ADAPTIVE_AP_COOLER
#define MAX_CPT_ADAPTIVE_COOLERS				(3)

#define THERMAL_HEADROOM						(0)
#define CONTINUOUS_TM							(1)
#define DYNAMIC_GET_GPU_POWER					(1)

/* 1: turn on precise power budgeting; 0: turn off */
#define PRECISE_HYBRID_POWER_BUDGET				(1)

#define PHPB_DEFAULT_ON							(1)
#endif

/* 1: thermal driver fast polling, use hrtimer; 0: turn off */
/*#define THERMAL_DRV_FAST_POLL_HRTIMER			(1)*/

/* 1: thermal driver update temp to MET directly, use hrtimer; 0: turn off */
#define THERMAL_DRV_UPDATE_TEMP_DIRECT_TO_MET	(1)

/* Define this in tscpu_settings.h enables this feature.
 * It polls CPU TS in hrtimer and
 * run ATM in RT 98 kthread. This is for MT6799 only.
 */
#define FAST_RESPONSE_ATM						(1)
#define THERMAL_INIT_VALUE						(0xDA1)
#define CLEAR_TEMP 26111

/* Thermal VPU throttling support */
#define THERMAL_VPU_SUPPORT
/* Thermal MDLA throttling support */
#define THERMAL_MDLA_SUPPORT
/* EARA_Thermal power budget allocation support */
/* #define EARA_THERMAL_SUPPORT */


#define TS_FILL(n) {#n, n}
/*#define TS_LEN_ARRAY(name) (sizeof(name)/sizeof(name[0]))*/
#define MAX_TS_NAME 20

#define CPU_COOLER_NUM 34
#define MTK_TS_CPU_RT                       (0)

#ifdef CONFIG_MTK_AEE_IPANIC
#define CONFIG_THERMAL_AEE_RR_REC (1)
#else
#define CONFIG_THERMAL_AEE_RR_REC (0)
#endif

#if CFG_THERM_LVTS
#define CONFIG_LVTS_ERROR_AEE_WARNING (1)
#else
#define CONFIG_LVTS_ERROR_AEE_WARNING (0)
#endif

#if CONFIG_LVTS_ERROR_AEE_WARNING
#define LVTS_FORCE_ERROR_TRIGGER (0)
#define LVTS_NUM_SKIP_SAMPLE (500)

#define HISTORY_SAMPLES (10)
#define FUTURE_SAMPLES (10)
#define R_BUFFER_SIZE (HISTORY_SAMPLES + FUTURE_SAMPLES + 1)
#define LVTS_ERROR_THRESHOLD (10000)

#define DUMP_LVTS_REGISTER (0)
#define DUMP_VCORE_VOLTAGE (0)
#endif
#define LVTS_VALID_DATA_TIME_PROFILING (0)
/*=============================================================
 *REG ACCESS
 *=============================================================
 */
/* double check */
#define TS_CONFIGURE		TS_CON1_TM	/* depend on CPU design*/
#define TS_CONFIGURE_P		TS_CON1_P	/* depend on CPU design*/

/* turn on TS_CON1[5:4] 2'b 00  11001111 -> 0xCF  ~(0x30)*/
#define TS_TURN_ON		0xFFFFFFCF
#define TS_TURN_OFF		0x00000030	/* turn off thermal*/

#define thermal_setl(addr, val)	mt_reg_sync_writel(readl(addr) | (val), \
								((void *)addr))

#define thermal_clrl(addr, val)	mt_reg_sync_writel(readl(addr) & ~(val), \
								((void *)addr))

#define MTKTSCPU_TEMP_CRIT 120000 /* 120.000 degree Celsius */

#define y_curr_repeat_times 1
#define THERMAL_NAME    "mtk-thermal-legacy"

#define TS_MS_TO_NS(x) (x * 1000 * 1000)

/*cpu core nums*/
#define TZCPU_NO_CPU_CORES              CONFIG_NR_CPUS

#if THERMAL_GET_AHB_BUS_CLOCK
#define THERMAL_MODULE_SW_CG_SET	(therm_clk_infracfg_ao_base + 0x80)
#define THERMAL_MODULE_SW_CG_CLR	(therm_clk_infracfg_ao_base + 0x84)
#define THERMAL_MODULE_SW_CG_STA	(therm_clk_infracfg_ao_base + 0x90)

#define THERMAL_CG	(therm_clk_infracfg_ao_base + 0x80)
#define THERMAL_DCM	(therm_clk_infracfg_ao_base + 0x70)
#endif

/*=============================================================
 * Common Structure and Enum
 *=============================================================
 */
#if (CONFIG_THERMAL_AEE_RR_REC == 1)
#define THERMAL_AEE_SELECTED_TS

#if defined(THERMAL_AEE_SELECTED_TS)
/* AEE reserved 10 slots of thermal zone temperature
 * in drivers/misc/mediatek/ram_console/mtk_ram_console.c
 *    #define THERMAL_RESERVED_TZS (10)
 *
 * So, THERMAL_AEE_MAX_SELECTED_TS should not larger than
 * THERMAL_RESERVED_TZS
 */
#define THERMAL_AEE_MAX_SELECTED_TS (10)
extern int (*get_aee_selected_tsX[THERMAL_AEE_MAX_SELECTED_TS])(void);
#endif

enum thermal_state {
	TSCPU_SUSPEND = 0,
	TSCPU_RESUME  = 1,
	TSCPU_NORMAL  = 2,
	TSCPU_INIT    = 3,
	TSCPU_PAUSE   = 4,
	TSCPU_RELEASE = 5
};
enum atm_state {
	ATM_WAKEUP = 0,
	ATM_CPULIMIT  = 1,
	ATM_GPULIMIT  = 2,
	ATM_DONE    = 3,
};
#endif

struct mtk_cpu_power_info {
	unsigned int cpufreq_khz;
	unsigned int cpufreq_ncpu;
	unsigned int cpufreq_power;
};

/*=============================================================
 * Tsense Structure and Enum
 *=============================================================
 */

/* private thermal sensor enum */
enum tsmcu_sensor_enum {
	L_TS_MCU0 = 0,	/* TSMCU0 */
	L_TS_MCU1,	/* TSMCU1 */
	L_TS_MCU2,	/* TSMCU2 */
	L_TS_MCU3,	/* TSMCU3 */
	L_TS_MCU4,	/* TSMCU4 */
	L_TS_MCU5,	/* TSMCU5 */
	L_TS_MCU6,	/* TSMCU6 */
	L_TS_MCU7,	/* TSMCU7 */
	L_TS_MCU8,	/* TSMCU8 */
	L_TS_MCU9,	/* TSMCU9 */
	L_TS_ABB,	/* ABB */
	L_TS_MCU_NUM
};

enum thermal_controller_name {
	THERMAL_CONTROLLER0 = 0,	/* TEMPMONCTL0 */
	THERMAL_CONTROLLER1,		/* TEMPMONCTL0_1 */
	THERMAL_CONTROLLER2,		/* TEMPMONCTL0_2 */
	THERMAL_CONTROLLER_NUM
};

struct thermal_controller_speed {
	unsigned int tempMonCtl1;
	unsigned int tempMonCtl2;
	unsigned int tempAhbPoll;
};

struct thermal_controller {
	enum tsmcu_sensor_enum ts[4]; /* Sensor point 0 ~ 3 */
	int ts_number;
	int dominator_ts_idx; //hw protection ref TS (index of the ts array)
	int tc_offset;
	struct thermal_controller_speed tc_speed;
};

/*=============================================================
 * LVTS Structure and Enum
 *=============================================================
 */
#if CFG_THERM_LVTS
/* private thermal sensor enum */
enum lvts_sensor_enum {
	L_TS_LVTS1_0 = 0,	/* LVTS1-0 */
	L_TS_LVTS1_1,		/* LVTS1-1 */
	L_TS_LVTS2_0,		/* LVTS2-0 */
	L_TS_LVTS2_1,		/* LVTS2-1 */
	L_TS_LVTS2_2,		/* LVTS2-2 */
	L_TS_LVTS3_0,		/* LVTS3-0 */
	L_TS_LVTS3_1,		/* LVTS3-1 */
	L_TS_LVTS4_0,		/* LVTS4-0 */
	L_TS_LVTS4_1,		/* LVTS4-1 */
	L_TS_LVTS9_0,		/* LVTS9-0 */
	L_TS_LVTS_NUM
};

enum lvts_tc_enum {
	LVTS_CONTROLLER0 = 0,	/* LVTSMONCTL0 */
	LVTS_CONTROLLER1,	/* LVTSMONCTL0_1 */
	LVTS_CONTROLLER2,	/* LVTSMONCTL0_2 */
	LVTS_CONTROLLER3,	/* LVTSMONCTL0_3 */
	LVTS_CONTROLLER_NUM
};

struct lvts_thermal_controller_speed {
	unsigned int tempMonCtl1;
	unsigned int tempMonCtl2;
	unsigned int tempAhbPoll;
};

struct lvts_thermal_controller {
	enum lvts_sensor_enum ts[4]; /* sensor point 0 ~ 3 */
	int ts_number;
	int dominator_ts_idx; //hw protection ref TS (index of the ts array)
	int tc_offset;
	struct lvts_thermal_controller_speed tc_speed;
};
#endif

/*=============================================================
 * Shared variables
 *=============================================================
 */
#ifdef CONFIG_OF
extern u32 thermal_irq_number;
extern void __iomem *thermal_base;
extern void __iomem *auxadc_ts_base;
extern void __iomem *infracfg_ao_base;
extern void __iomem *th_apmixed_base;
extern void __iomem *INFRACFG_AO_base;

extern int thermal_phy_base;
extern int auxadc_ts_phy_base;
extern int apmixed_phy_base;
extern int pericfg_phy_base;
#endif

#if PRECISE_HYBRID_POWER_BUDGET
/*	tscpu_prev_cpu_temp: previous CPUSYS temperature
 *	tscpu_curr_cpu_temp: current CPUSYS temperature
 *	tscpu_prev_gpu_temp: previous GPUSYS temperature
 *	tscpu_curr_gpu_temp: current GPUSYS temperature
 */
extern int tscpu_curr_cpu_temp;
extern int tscpu_curr_gpu_temp;
#endif

/*
 * In src/mtk_tc.c
 */
extern int temp_eUART;
extern int temp_dUART;

extern int tscpu_debug_log;
extern const struct of_device_id mt_thermal_of_match[2];
extern struct thermal_controller tscpu_g_tc[THERMAL_CONTROLLER_NUM];
extern int tscpu_polling_trip_temp1;
extern int tscpu_polling_trip_temp2;
extern int tscpu_polling_factor1;
extern int tscpu_polling_factor2;

/*
 * temperature array to store both tsmcu and lvts (if exist) and export them
 */
extern int tscpu_ts_temp[TS_ENUM_MAX];
extern int tscpu_ts_temp_r[TS_ENUM_MAX]; /* raw data */

/*
 * temperature array to store temp of tsmcu sensor
 */
extern int tscpu_ts_mcu_temp[L_TS_MCU_NUM];
extern int tscpu_ts_mcu_temp_r[L_TS_MCU_NUM]; /* raw data */

#if CFG_THERM_LVTS
/*
 * temperature array to store temp of lvts sensor
 */
extern int tscpu_ts_lvts_temp[L_TS_LVTS_NUM];
extern int tscpu_ts_lvts_temp_r[L_TS_LVTS_NUM]; /* raw data */
#endif

#if CONFIG_LVTS_ERROR_AEE_WARNING
extern int tscpu_ts_mcu_temp_v[L_TS_MCU_NUM];
extern int tscpu_ts_lvts_temp_v[L_TS_LVTS_NUM];
#if DUMP_VCORE_VOLTAGE
extern struct regulator *vcore_reg_id;
#endif
#endif
#if LVTS_VALID_DATA_TIME_PROFILING
extern unsigned long long int SODI3_count, noValid_count;
/* If isTempValid is 0, it means no valid temperature data
 * between two SODI3 entry points.
 */
extern int isTempValid;
extern long long int start_timestamp;
/* count if start_timestamp is bigger than end_timestamp */
extern int diff_error_count;
#endif
/*
 * support LVTS
 */
#if CFG_THERM_LVTS
extern int lvts_rawdata_debug_log;
extern int lvts_debug_log;
extern struct lvts_thermal_controller
lvts_tscpu_g_tc[LVTS_CONTROLLER_NUM];
#endif

#if MTKTSCPU_FAST_POLLING
/* Combined fast_polling_trip_temp and fast_polling_factor,
 * it means polling_delay will be 1/5 of original interval
 * after mtktscpu reports > 65C w/o exit point
 */
extern int fast_polling_trip_temp;
extern int fast_polling_trip_temp_high;
extern int fast_polling_factor;
extern int tscpu_cur_fp_factor;
extern int tscpu_next_fp_factor;
#endif

/*In common/thermal_zones/mtk_ts_cpu.c*/
extern long long int thermal_get_current_time_us(void);
extern void tscpu_workqueue_cancel_timer(void);
extern void tscpu_workqueue_start_timer(void);

extern void __iomem  *therm_clk_infracfg_ao_base;
extern int Num_of_GPU_OPP;
extern int gpu_max_opp;
extern struct mt_gpufreq_power_table_info *mtk_gpu_power;
extern int tscpu_read_curr_temp;
#if MTKTSCPU_FAST_POLLING
extern int tscpu_cur_fp_factor;
#endif
extern struct platform_device *tscpu_pdev;

#if !defined(CONFIG_MTK_CLKMGR)
extern struct clk *therm_main;           /* main clock for Thermal*/
#endif

#if CPT_ADAPTIVE_AP_COOLER
extern int tscpu_g_curr_temp;
extern int tscpu_g_prev_temp;
#if (THERMAL_HEADROOM == 1) || (CONTINUOUS_TM == 1)
extern int bts_cur_temp;	/* in mtk_ts_bts.c */
#endif

#endif

extern char *adaptive_cooler_name;

/* common/coolers/mtk_cooler_atm.c */
extern unsigned int adaptive_cpu_power_limit;
extern unsigned int adaptive_gpu_power_limit;
extern int TARGET_TJS[MAX_CPT_ADAPTIVE_COOLERS];
#ifdef FAST_RESPONSE_ATM
extern void atm_cancel_hrtimer(void);
extern void atm_restart_hrtimer(void);
#endif

/* common/coolers/mtk_cooler_dtm.c */
extern unsigned int static_cpu_power_limit;
extern unsigned int static_gpu_power_limit;
extern int tscpu_cpu_dmips[CPU_COOLER_NUM];
/*=============================================================
 * Shared functions
 *=============================================================
 */
/*In mtk_tc_wrapper.c */
extern int tscpu_get_curr_max_ts_temp(void);
extern int tscpu_max_temperature(void);
extern int tscpu_get_curr_temp(void);
extern int combine_lvts_tsmcu_temp(void);
extern int tscpu_read_temperature_info(struct seq_file *m, void *v);
extern int get_io_reg_base(void);

/*In common/thermal_zones/mtk_ts_cpu.c*/
extern void thermal_init_interrupt_for_UART(int temp_e, int temp_d);
extern void tscpu_update_tempinfo(void);
#if THERMAL_GPIO_OUT_TOGGLE
void tscpu_set_GPIO_toggle_for_monitor(void);
#endif
extern void tscpu_update_tempinfo(void);

/*In src/mtk_tc.c*/
extern void tscpu_config_all_tc_hw_protect(int temperature, int temperature2);
extern void tscpu_reset_thermal(void);
extern void tscpu_thermal_initial_all_tc(void);
extern void tscpu_thermal_read_tc_temp(
	int tc_num, enum tsmcu_sensor_enum type, int order);
extern void tscpu_thermal_cal_prepare(void);
extern void tscpu_thermal_cal_prepare_2(unsigned int ret);
extern irqreturn_t tscpu_thermal_all_tc_interrupt_handler(
int irq, void *dev_id);
extern int tscpu_thermal_clock_on(void);
extern int tscpu_thermal_clock_off(void);
extern int tscpu_dump_cali_info(struct seq_file *m, void *v);
extern int tscpu_thermal_fast_init(int tc_num);
extern void thermal_get_AHB_clk_info(void);
extern void print_risky_temps(char *prefix, int offset, int printLevel);
extern void thermal_pause_all_periodoc_temp_sensing(void);
extern void thermal_release_all_periodoc_temp_sensing(void);
extern int (*max_temperature_in_bank[THERMAL_BANK_NUM])(void);
extern void thermal_disable_all_periodoc_temp_sensing(void);
extern void read_all_tc_tsmcu_temperature(void);

/*
 * Support LVTS
 */
#if CFG_THERM_LVTS
extern int lvts_get_io_reg_base(void);
extern int lvts_max_temperature(void);
extern void lvts_config_all_tc_hw_protect(int temperature, int temperature2);
extern void lvts_thermal_read_tc_temp(
			int tc_num, enum lvts_sensor_enum type, int order);
extern void lvts_read_all_tc_temperature(void);
extern void lvts_reset_and_initial(int tc_num);
extern int (*lvts_max_temperature_in_bank[THERMAL_BANK_NUM])(void);
extern void lvts_thermal_lvts_device_init(void);
extern void lvts_read_temperature(void);
//extern void lvts_read_temperature(int temp0, int temp1);
extern void lvts_thermal_cal_prepare(void);
extern void lvts_device_identification(void);
extern void lvts_reset_device_and_stop_clk(void);
extern void  lvts_read_device_id_rev(void);
extern void lvts_Device_Enable_Init_all_Devices(void);
extern void lvts_device_read_count_RC_N(void);
extern void lvts_efuse_setting(void);
extern void Enable_LVTS_CTRL_for_thermal_Data_Fetch(void);
extern void lvts_tscpu_thermal_initial_all_tc(void);
extern void lvts_thermal_pause_all_periodoc_temp_sensing(void);
extern int lvts_thermal_check_all_sensing_point_idle(void);
extern void lvts_thermal_release_all_periodoc_temp_sensing(void);
extern void lvts_thermal_disable_all_periodoc_temp_sensing(void);
extern void read_all_tc_lvts_temperature(void);
extern irqreturn_t lvts_tscpu_thermal_all_tc_interrupt_handler(
int irq, void *dev_id);
extern int lvts_tscpu_dump_cali_info(struct seq_file *m, void *v);
extern void lvts_sodi3_release_thermal_controller(void);
#endif

/*
 * In drivers/misc/mediatek/gpu/hal/mtk_gpu_utility.c
 * It's not our api, ask them to provide header file
 */
extern bool mtk_get_gpu_loading(unsigned int *pLoading);
/*
 * In drivers/misc/mediatek/auxadc/mt_auxadc.c
 * It's not our api, ask them to provide header file
 */
extern int IMM_IsAdcInitReady(void);
/*aee related*/
#if (CONFIG_THERMAL_AEE_RR_REC == 1)
extern void aee_rr_init_thermal_temp(int num);
extern void aee_rr_rec_thermal_temp(int index, s8 val);
extern void aee_rr_rec_thermal_lvts_config(u8 val);
extern void aee_rr_rec_thermal_status(u8 val);
extern void aee_rr_rec_thermal_ATM_status(u8 val);
extern void aee_rr_rec_thermal_ktime(u64 val);

extern s8 aee_rr_curr_thermal_temp(int index);
extern u8 aee_rr_curr_thermal_status(void);
extern u8 aee_rr_curr_thermal_ATM_status(void);
extern u64 aee_rr_curr_thermal_ktime(void);
#endif

#if CONFIG_LVTS_ERROR_AEE_WARNING
extern void dump_efuse_data(void);
extern int check_lvts_mcu_efuse(void);
extern int check_auxadc_mcu_efuse(void);
#if DUMP_LVTS_REGISTER
extern void read_controller_reg_before_active(void);
extern void read_controller_reg_when_error(void);
extern void read_device_reg_before_active(void);
extern void read_device_reg_when_error(void);
extern void clear_lvts_register_value_array(void);
extern void dump_lvts_register_value(void);
#endif
#endif
#if LVTS_VALID_DATA_TIME_PROFILING
extern void lvts_dump_time_profiling_result(struct seq_file *m);
#endif
/*=============================================================
 *LOG
 *=============================================================
 */
#define TSCPU_LOG_TAG           "[Thermal/TZ/CPU]"

#define tscpu_dprintk(fmt, args...)	\
	do {					\
		if (tscpu_debug_log == 1) {	\
			pr_notice(TSCPU_LOG_TAG fmt, ##args);	\
		}	\
	} while (0)

#define tscpu_printk(fmt, args...)	pr_notice(TSCPU_LOG_TAG fmt, ##args)
#define tscpu_warn(fmt, args...)	pr_notice(TSCPU_LOG_TAG fmt, ##args)

/*
 * Support LVTS
 */
#if CFG_THERM_LVTS
#define LVTS_LOG_TAG            "[Thermal/TZ/LVTS]"
#define LVTS_LOG_REG_TAG        "[Thermal/TZ/LVTSREG]"

#define lvts_reg_print(fmt, args...)	pr_notice(LVTS_LOG_REG_TAG fmt, ##args)
#define lvts_printk(fmt, args...)	pr_notice(LVTS_LOG_TAG fmt, ##args)
#define lvts_warn(fmt, args...)		pr_notice(LVTS_LOG_TAG fmt, ##args)
#if 0
#define lvts_dbg_printk(fmt, args...)  pr_notice(LVTS_LOG_TAG fmt, ##args)
#else
#define lvts_dbg_printk(fmt, args...)   \
	do {                                    \
		if (lvts_debug_log == 1) {                \
			pr_notice(LVTS_LOG_TAG fmt, ##args); \
		}                                   \
	} while (0)
#endif
#endif

/*=============================================================
 * Register macro for internal use
 *=============================================================
 */

#if 1
#define THERM_CTRL_BASE_2    thermal_base
#define AUXADC_BASE_2        auxadc_ts_base
#define INFRACFG_AO_BASE_2   infracfg_ao_base
#define APMIXED_BASE_2       th_apmixed_base
#else
#include <mach/mt_reg_base.h>
#define AUXADC_BASE_2     AUXADC_BASE
#define THERM_CTRL_BASE_2 THERM_CTRL_BASE
#define PERICFG_BASE_2    PERICFG_BASE
#define APMIXED_BASE_2    APMIXED_BASE
#endif

/*******************************************************************************
 * AUXADC Register Definition
 *****************************************************************************
 */

#define AUXADC_CON0_V       (AUXADC_BASE_2 + 0x000)
#define AUXADC_CON1_V       (AUXADC_BASE_2 + 0x004)
#define AUXADC_CON1_SET_V   (AUXADC_BASE_2 + 0x008)
#define AUXADC_CON1_CLR_V   (AUXADC_BASE_2 + 0x00C)
#define AUXADC_CON2_V       (AUXADC_BASE_2 + 0x010)
/*#define AUXADC_CON3_V       (AUXADC_BASE_2 + 0x014)*/
#define AUXADC_DAT0_V       (AUXADC_BASE_2 + 0x014)
#define AUXADC_DAT1_V       (AUXADC_BASE_2 + 0x018)
#define AUXADC_DAT2_V       (AUXADC_BASE_2 + 0x01C)
#define AUXADC_DAT3_V       (AUXADC_BASE_2 + 0x020)
#define AUXADC_DAT4_V       (AUXADC_BASE_2 + 0x024)
#define AUXADC_DAT5_V       (AUXADC_BASE_2 + 0x028)
#define AUXADC_DAT6_V       (AUXADC_BASE_2 + 0x02C)
#define AUXADC_DAT7_V       (AUXADC_BASE_2 + 0x030)
#define AUXADC_DAT8_V       (AUXADC_BASE_2 + 0x034)
#define AUXADC_DAT9_V       (AUXADC_BASE_2 + 0x038)
#define AUXADC_DAT10_V       (AUXADC_BASE_2 + 0x03C)
#define AUXADC_DAT11_V       (AUXADC_BASE_2 + 0x040)
#define AUXADC_MISC_V       (AUXADC_BASE_2 + 0x094)

#define AUXADC_CON0_P       (auxadc_ts_phy_base + 0x000)
#define AUXADC_CON1_P       (auxadc_ts_phy_base + 0x004)
#define AUXADC_CON1_SET_P   (auxadc_ts_phy_base + 0x008)
#define AUXADC_CON1_CLR_P   (auxadc_ts_phy_base + 0x00C)
#define AUXADC_CON2_P       (auxadc_ts_phy_base + 0x010)
/*#define AUXADC_CON3_P       (auxadc_ts_phy_base + 0x014)*/
#define AUXADC_DAT0_P       (auxadc_ts_phy_base + 0x014)
#define AUXADC_DAT1_P       (auxadc_ts_phy_base + 0x018)
#define AUXADC_DAT2_P       (auxadc_ts_phy_base + 0x01C)
#define AUXADC_DAT3_P       (auxadc_ts_phy_base + 0x020)
#define AUXADC_DAT4_P       (auxadc_ts_phy_base + 0x024)
#define AUXADC_DAT5_P       (auxadc_ts_phy_base + 0x028)
#define AUXADC_DAT6_P       (auxadc_ts_phy_base + 0x02C)
#define AUXADC_DAT7_P       (auxadc_ts_phy_base + 0x030)
#define AUXADC_DAT8_P       (auxadc_ts_phy_base + 0x034)
#define AUXADC_DAT9_P       (auxadc_ts_phy_base + 0x038)
#define AUXADC_DAT10_P       (auxadc_ts_phy_base + 0x03C)
#define AUXADC_DAT11_P       (auxadc_ts_phy_base + 0x040)

#define AUXADC_MISC_P       (auxadc_ts_phy_base + 0x094)

/*******************************************************************************
 * Peripheral Configuration Register Definition
 *****************************************************************************
 */

/*APB Module infracfg_ao*/
#define INFRA_GLOBALCON_RST_0_SET (INFRACFG_AO_BASE_2 + 0x120)
#define INFRA_GLOBALCON_RST_0_CLR (INFRACFG_AO_BASE_2 + 0x124)
#define INFRA_GLOBALCON_RST_0_STA (INFRACFG_AO_BASE_2 + 0x128)
/*******************************************************************************
 * APMixedSys Configuration Register Definition
 *****************************************************************************
 */
/* TODO: check base addr. */
#define TS_CON0_TM             (APMIXED_BASE_2 + 0x600) /*yes 0x10212000*/
#define TS_CON1_TM             (APMIXED_BASE_2 + 0x604)
#define TS_CON0_P           (apmixed_phy_base + 0x600)
#define TS_CON1_P           (apmixed_phy_base + 0x604)

/*******************************************************************************
 * Thermal Controller Register Mask Definition
 *****************************************************************************
 */
#define THERMAL_ENABLE_SEN0     0x1
#define THERMAL_ENABLE_SEN1     0x2
#define THERMAL_ENABLE_SEN2     0x4
#define THERMAL_MONCTL0_MASK    0x00000007

#define THERMAL_PUNT_MASK       0x00000FFF
#define THERMAL_FSINTVL_MASK    0x03FF0000
#define THERMAL_SPINTVL_MASK    0x000003FF
#define THERMAL_MON_INT_MASK    0x0007FFFF

#define THERMAL_MON_CINTSTS0    0x000001
#define THERMAL_MON_HINTSTS0    0x000002
#define THERMAL_MON_LOINTSTS0   0x000004
#define THERMAL_MON_HOINTSTS0   0x000008
#define THERMAL_MON_NHINTSTS0   0x000010
#define THERMAL_MON_CINTSTS1    0x000020
#define THERMAL_MON_HINTSTS1    0x000040
#define THERMAL_MON_LOINTSTS1   0x000080
#define THERMAL_MON_HOINTSTS1   0x000100
#define THERMAL_MON_NHINTSTS1   0x000200
#define THERMAL_MON_CINTSTS2    0x000400
#define THERMAL_MON_HINTSTS2    0x000800
#define THERMAL_MON_LOINTSTS2   0x001000
#define THERMAL_MON_HOINTSTS2   0x002000
#define THERMAL_MON_NHINTSTS2   0x004000
#define THERMAL_MON_TOINTSTS    0x008000
#define THERMAL_MON_IMMDINTSTS0 0x010000
#define THERMAL_MON_IMMDINTSTS1 0x020000
#define THERMAL_MON_IMMDINTSTS2 0x040000
#define THERMAL_MON_FILTINTSTS0 0x080000
#define THERMAL_MON_FILTINTSTS1 0x100000
#define THERMAL_MON_FILTINTSTS2 0x200000


#define THERMAL_tri_SPM_State0	0x20000000
#define THERMAL_tri_SPM_State1	0x40000000
#define THERMAL_tri_SPM_State2	0x80000000


#define THERMAL_MSRCTL0_MASK    0x00000007
#define THERMAL_MSRCTL1_MASK    0x00000038
#define THERMAL_MSRCTL2_MASK    0x000001C0


#endif	/* __TSCPU_SETTINGS_H__ */
