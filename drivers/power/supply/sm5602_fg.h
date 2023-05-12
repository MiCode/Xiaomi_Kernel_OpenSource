/*
 * include/linux/power/sm5602_fg.h
 *
 * Copyright (C) 2018 SiliconMitus
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef SM5602_FG_H
#define SM5602_FG_H

#define FG_INIT_MARK				0xA000

#define FG_PARAM_UNLOCK_CODE	  	0x3700
#define FG_PARAM_LOCK_CODE	  		0x0000
#define FG_TABLE_LEN				0x18//real table length -1
#define FG_ADD_TABLE_LEN			0x8//real table length -1
#define FG_INIT_B_LEN		    	0x7//real table length -1

#define ENABLE_EN_TEMP_IN           0x0200
#define ENABLE_EN_TEMP_EX           0x0400
#define ENABLE_EN_BATT_DET          0x0800
#define ENABLE_IOCV_MAN_MODE        0x1000
#define ENABLE_FORCED_SLEEP         0x2000
#define ENABLE_SLEEPMODE_EN         0x4000
#define ENABLE_SHUTDOWN             0x8000

/* REG */
#define FG_REG_SOC_CYCLE			0x0B
#define FG_REG_SOC_CYCLE_CFG		0x15
#define FG_REG_ALPHA             	0x20
#define FG_REG_BETA              	0x21
#define FG_REG_RS                	0x24
#define FG_REG_RS_1     			0x25
#define FG_REG_RS_2            		0x26
#define FG_REG_RS_3            		0x27
#define FG_REG_RS_0            		0x29
#define FG_REG_END_V_IDX			0x2F
#define FG_REG_START_LB_V			0x30
#define FG_REG_START_CB_V			0x38
#define FG_REG_START_LB_I			0x40
#define FG_REG_START_CB_I			0x48
#define FG_REG_VOLT_CAL				0x70
#define FG_REG_CURR_IN_OFFSET		0x75
#define FG_REG_CURR_IN_SLOPE		0x76
#define FG_REG_RMC					0x84

#define FG_REG_SRADDR				0x8C
#define FG_REG_SRDATA				0x8D
#define FG_REG_SWADDR				0x8E
#define FG_REG_SWDATA				0x8F

#define FG_REG_AGING_CTRL			0x9C

#define FG_TEMP_TABLE_CNT_MAX       0x65

#define I2C_ERROR_COUNT_MAX			0x5

#define FG_PARAM_VERION       		0x1E

#define INIT_CHECK_MASK         	0x0010
#define DISABLE_RE_INIT         	0x0010

#define	INVALID_REG_ADDR	0xFF
#define RET_ERR 			-1

//#define SOC_SMOOTH_TRACKING
#define SHUTDOWN_DELAY
//#define FG_ENABLE_IRQ
#define ENABLE_IOCV_ADJ
#define ENABLE_INSPECTION_TABLE
//#define ENABLE_VLCM_MODE
#define ENABLE_TEMBASE_ZDSCON
//#define ENABLE_INIT_DELAY_TEMP
#define ENABLE_TEMP_AVG
//#define IN_PRO_ADJ
#define ENABLE_NTC_COMPENSATION_1
//#define IOCV_M_TYPE1
//#define ENABLE_MAP_SOC
//#define ENABLE_MIX_NTC_BATTDET
//#define ENABLE_WAIT_SOC_FULL
//#define ENABLE_LTIM_ACT
#define ENABLE_HCIC_ACT
#define ENABLE_HCRSM_MODE

#define EX_TEMP_MIN			(-20)
#define EX_TEMP_MAX			80

#ifdef ENABLE_MAP_SOC
#define MAP_MAX_SOC			99
#define MAP_RATE_SOC		995
#define MAP_MIN_SOC			4
#endif

#ifdef ENABLE_INIT_DELAY_TEMP
#define DELAY_TEMP_TIME_5000MS		5000
#endif

#ifdef ENABLE_IOCV_ADJ
#define IOCV_MAX_ADJ_LEVEL 		0x1F33
#define IOCV_MIN_ADJ_LEVEL 		0x1D70
#define IOCI_MAX_ADJ_LEVEL 		0x1000
#define IOCI_MIN_ADJ_LEVEL 		0xCC
#define IOCV_I_SLOPE 	   		100
#define IOCV_I_OFFSET  	   		0
#endif

#ifdef ENABLE_TEMBASE_ZDSCON
#define ZDSCON_ACT_TEMP_GAP 	15
#define T_GAP_DENOM 			5
#define HMINMAN_T_VALUE_FACT 	125
#define I_GAP_DENOM 			1000
#define HMINMAN_I_VALUE_FACT 	150

#ifdef ENABLE_LTIM_ACT
#define LTIM_ACT_TEMP_GAP 		24
#define LTIM_I_LIMIT 			1500
#define LTIM_FACTOR 			42
#define LTIM_DENOM 				130
#define LTIM_MIN 				0xA
#endif
#endif

#ifdef ENABLE_HCIC_ACT
#define HCIC_MIN 4000
#define HCIC_FACTOR 20
#define HCIC_DENOM 0
#endif

#ifdef ENABLE_HCRSM_MODE
#define HCRSM_VOLT 4460
#define HCRSM_CURR 1200
#endif

#ifdef ENABLE_WAIT_SOC_FULL
#define WAIT_SOC_GAP 			1
#endif

#ifdef SHUTDOWN_DELAY	
#define SHUTDOWN_DELAY_H_VOL	3400
#define SHUTDOWN_DELAY_L_VOL	3300
#endif

#define SOC_DECIMAL_2_POINT

enum sm_fg_reg_idx {
	SM_FG_REG_DEVICE_ID = 0,
	SM_FG_REG_CNTL,
	SM_FG_REG_INT,
	SM_FG_REG_INT_MASK,
	SM_FG_REG_STATUS,
	SM_FG_REG_SOC,
	SM_FG_REG_OCV,
	SM_FG_REG_VOLTAGE,
	SM_FG_REG_CURRENT,
	SM_FG_REG_TEMPERATURE_IN,
	SM_FG_REG_TEMPERATURE_EX,
	SM_FG_REG_V_L_ALARM,
	SM_FG_REG_V_H_ALARM,	
	SM_FG_REG_A_H_ALARM,
	SM_FG_REG_T_IN_H_ALARM,
	SM_FG_REG_SOC_L_ALARM,
	SM_FG_REG_FG_OP_STATUS,
	SM_FG_REG_TOPOFFSOC,
	SM_FG_REG_PARAM_CTRL,
	SM_FG_REG_SHUTDOWN,	
	SM_FG_REG_VIT_PERIOD,
	SM_FG_REG_CURRENT_RATE,
	SM_FG_REG_BAT_CAP,
	SM_FG_REG_CURR_OFFSET,
	SM_FG_REG_CURR_SLOPE,	
	SM_FG_REG_MISC,
	SM_FG_REG_RESET,
	SM_FG_REG_RSNS_SEL,
	SM_FG_REG_VOL_COMP,
	NUM_REGS,
};

static u8 sm5602_regs[NUM_REGS] = {
	0x00, /* DEVICE_ID */
	0x01, /* CNTL */
	0x02, /* INT */
	0x03, /* INT_MASK */
	0x04, /* STATUS */
	0x05, /* SOC */
	0x06, /* OCV */
	0x07, /* VOLTAGE */
	0x08, /* CURRENT */
	0x09, /* TEMPERATURE_IN */
	0x0A, /* TEMPERATURE_EX */
	0x0C, /* V_L_ALARM */
	0x0D, /* V_H_ALARM */	
	0x0E, /* A_H_ALARM */
	0x0F, /* T_IN_H_ALARM */
	0x10, /* SOC_L_ALARM */
	0x11, /* FG_OP_STATUS */
	0x12, /* TOPOFFSOC */
	0x13, /* PARAM_CTRL */
	0x14, /* SHUTDOWN */
	0x1A, /* VIT_PERIOD */
	0x1B, /* CURRENT_RATE */
	0x62, /* BAT_CAP */	
	0x73, /* CURR_OFFSET */	
	0x74, /* CURR_SLOPE */
	0x90, /* MISC */
	0x91, /* RESET */
	0x95, /* RSNS_SEL */
	0x96, /* VOL_COMP */
};

enum sm_fg_device {
	SM5602,
};

enum sm_fg_temperature_type {
	TEMPERATURE_IN = 0,
	TEMPERATURE_EX,
};

enum battery_table_type {
	BATTERY_TABLE0 = 0,
	BATTERY_TABLE1,
	BATTERY_TABLE2,
	BATTERY_TABLE_MAX,
};

#ifdef SOC_SMOOTH_TRACKING
#define BATT_MA_AVG_SAMPLES	8
struct batt_params {
	bool			update_now;
	int				batt_raw_soc; //x.x% ex)500 = 50.0%
	int				batt_soc; //x.x% ex)500 = 50.0%
	int				samples_num;
	int				samples_index;
	int				batt_ma_avg_samples[BATT_MA_AVG_SAMPLES];
	int				batt_ma_avg;
	int				batt_ma_prev;
	int				batt_ma;
	int				batt_mv;
	int				batt_temp;
	struct timespec		last_soc_change_time;
};
#endif

#ifdef ENABLE_TEMP_AVG
#define BATT_TEMP_AVG_SAMPLES	8
struct batt_temp_params {
	bool		update_now;
	int			batt_raw_temp;
	int			batt_temp;
	int			samples_num;
	int			samples_index;
	int			batt_temp_avg_samples[BATT_TEMP_AVG_SAMPLES];
	int			batt_temp_avg;
	int			batt_temp_prev;
	struct timespec		last_temp_change_time;
};
#endif

struct sm_fg_chip;

struct sm_fg_chip {
	struct device		*dev;
	struct i2c_client	*client;
	struct mutex i2c_rw_lock; /* I2C Read/Write Lock */
	struct mutex data_lock; /* Data Lock */
	u8 chip;
	u8 regs[NUM_REGS];
	int	batt_id;
	int gpio_int;
	
	/* Status Tracking */
	bool batt_present;
	bool batt_fc;	/* Battery Full Condition */
	bool batt_ot;	/* Battery Over Temperature */
	bool batt_ut;	/* Battery Under Temperature */
	bool batt_soc1;	/* SOC Low */
	bool batt_socp;	/* SOC Poor */
	bool batt_dsg;	/* Discharge Condition*/
	int	batt_soc;
	int batt_ocv;
	int batt_fcc;	/* Full charge capacity */
	int	batt_volt;
	int	aver_batt_volt;
	int	batt_temp;
	int	batt_curr;
	int batt_rmc;
	int is_charging;	/* Charging informaion from charger IC */
    int batt_soc_cycle; /* Battery SOC cycle */
    int topoff_soc;
	int topoff_margin;
    int top_off;
	int iocv_error_count;
#ifdef SOC_SMOOTH_TRACKING
	bool	soc_reporting_ready;
#endif
#ifdef SHUTDOWN_DELAY
	bool	shutdown_delay_enable;
	bool	shutdown_delay;
#endif
#ifdef SOC_DECIMAL_2_POINT
	int soc_decimal;
	int *dec_rate_seq;
	int dec_rate_len;
#endif

	/* previous battery voltage current*/
    int p_batt_voltage;
    int p_batt_current;
	int p_report_soc;
	
	/* DT */
	bool en_temp_ex;
	bool en_temp_in;
	bool en_batt_det;
	bool iocv_man_mode;
	int aging_ctrl;
	int batt_rsns;	/* Sensing resistor value */
	int cycle_cfg;
	int fg_irq_set;
	int low_soc1;
	int low_soc2;
	int v_l_alarm;
	int v_h_alarm;
	int battery_table_num;
	int misc;
    int batt_v_max;
	int min_cap;
	u32 common_param_version;
	int t_l_alarm_in; 
	int t_h_alarm_in;
	u32 t_l_alarm_ex;
	u32 t_h_alarm_ex;
	
	/* Battery Data */
	int battery_table[BATTERY_TABLE_MAX][FG_TABLE_LEN];
	signed short battery_temp_table[FG_TEMP_TABLE_CNT_MAX]; /* Degree */
	int alpha;
	int beta;
	int rs;
	int rs_value[4];
	int vit_period;
	int mix_value;
	const char		*battery_type;
	int volt_cal;
	int curr_offset;
	int curr_slope;
	int cap;
    int n_tem_poff;
    int n_tem_poff_offset;
	int batt_max_voltage_uv;
	int temp_std;
	int en_high_fg_temp_offset;
    int high_fg_temp_offset_denom;
    int high_fg_temp_offset_fact;
    int en_low_fg_temp_offset;
    int low_fg_temp_offset_denom;
    int low_fg_temp_offset_fact;
	int en_high_fg_temp_cal;
    int high_fg_temp_p_cal_denom;
    int high_fg_temp_p_cal_fact;
    int high_fg_temp_n_cal_denom;
    int high_fg_temp_n_cal_fact;
    int en_low_fg_temp_cal;
    int low_fg_temp_p_cal_denom;
    int low_fg_temp_p_cal_fact;
    int low_fg_temp_n_cal_denom;
    int low_fg_temp_n_cal_fact;
	int	en_high_temp_cal;
    int high_temp_p_cal_denom;
    int high_temp_p_cal_fact;
    int high_temp_n_cal_denom;
    int high_temp_n_cal_fact;
    int en_low_temp_cal;
    int low_temp_p_cal_denom;
    int low_temp_p_cal_fact;
    int low_temp_n_cal_denom;
    int low_temp_n_cal_fact;
	u32 battery_param_version;
#ifdef ENABLE_NTC_COMPENSATION_1	
	int rtrace;
#endif
	
	struct delayed_work monitor_work;
#ifdef ENABLE_INIT_DELAY_TEMP	
	struct delayed_work init_delay_temp_work;
#endif

#ifdef SOC_SMOOTH_TRACKING	
#if 0	
	struct delayed_work soc_monitor_work;
#endif
	int charge_full;
#endif
	//unsigned long last_update;
#ifdef ENABLE_INIT_DELAY_TEMP
	int en_init_delay_temp;
#endif
	/* Debug */
	int	skip_reads;
	int	skip_writes;
	int fake_soc;
	int fake_temp;
	struct dentry *debug_root;
	struct power_supply *batt_psy;
	struct power_supply *fg_psy;
#ifdef SOC_SMOOTH_TRACKING	
	struct batt_params	param;
#endif
#ifdef ENABLE_TEMP_AVG
	struct batt_temp_params	temp_param;
#endif
	//fake cycle
	int fake_cycle_count;

	//add bq_psy
	struct power_supply		*bq_psy;
	//add verify_psy
	struct power_supply		*verify_psy;
};

#endif /* SM5602_FG_H */
