/*
 * Fuelgauge battery driver
 *
 * Copyright (C) 2018 Siliconmitus
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#define pr_fmt(fmt)	"[sm5602] %s(%d): " fmt, __func__, __LINE__
#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/acpi.h>
#include <asm/unaligned.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <sm5602_fg.h>
#include <linux/pmic-voter.h>//"pmic-voter.h"
//#include "step-chg-jeita.h"

#if 1  /*only for debug*/
#undef pr_debug
#define pr_debug pr_err
#undef pr_info
#define pr_info pr_err
#undef dev_dbg
#define dev_dbg dev_err
#endif

#define	INVALID_REG_ADDR	0xFF
#define   RET_ERR -1

//#define ENABLE_MIX_COMP
#define ENABLE_TEMBASE_ZDSCON

#ifdef ENABLE_TEMBASE_ZDSCON
#define ZDSCON_ACT_TEMP_GAP 15
#define TEMP_GAP_DENOM 5
#define HMINMAN_VALUE_FACT 99
#endif

#define MONITOR_WORK_10S	10
#define MONITOR_WORK_5S		5
#define MONITOR_WORK_1S		1

#define SM_RAW_SOC_FULL		1000 //100.0%
#define SM_RECHARGE_SOC		971  //98.5%

#define BMS_FG_VERIFY		"BMS_FG_VERIFY"
#define BMS_FC_VOTER		"BMS_FC_VOTER"

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

const unsigned char *device2str[] = {
	"sm5602",
};

enum battery_table_type {
	BATTERY_TABLE0 = 0,
	BATTERY_TABLE1,
	BATTERY_TABLE2,
	BATTERY_TABLE_MAX,
};

#define BATT_MA_AVG_SAMPLES	8
struct batt_params {
	bool			update_now;
	int			batt_raw_soc;
	int			batt_soc;
	int			samples_num;
	int			samples_index;
	int			batt_ma_avg_samples[BATT_MA_AVG_SAMPLES];
	int			batt_ma_avg;
	int			batt_ma_prev;
	int			batt_ma;
	int			batt_mv;
	int			batt_temp;
	struct timespec		last_soc_change_time;
};

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
	
	struct notifier_block   nb;
	
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
	int batt_rmc;	/* Remaining capacity */	
	int	batt_volt;
	int	aver_batt_volt;
	int	batt_temp;
	int	batt_curr;	
	int is_charging;	/* Charging informaion from charger IC */
    int batt_soc_cycle; /* Battery SOC cycle */
    int topoff_soc;
    int top_off;
	int iocv_error_count;
	int charge_status;

	int health;
	int recharge_vol;
	bool	usb_present;
	bool	batt_sw_fc;
	bool	fast_mode;
	bool	shutdown_delay_enable;
	bool	shutdown_delay;
	bool	soc_reporting_ready;

	/* previous battery voltage current*/
    int p_batt_voltage;
    int p_batt_current;

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
	signed short battery_temp_table[FG_TEMP_TABLE_CNT_MAX]; /* -20~80 Degree */
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
    int fcm_offset;
	
	struct delayed_work monitor_work;
	unsigned long last_update;
	struct votable *fcc_votable;
	struct votable *fv_votable;
	struct votable	*chg_dis_votable;
	
	/* Debug */
	int	skip_reads;
	int	skip_writes;
	int fake_soc;
	int fake_temp;
	int *dec_rate_seq;
	int dec_rate_len;
	int	fake_chip_ok;	
	struct dentry *debug_root;
	struct power_supply* fg_psy;
	struct power_supply *usb_psy;
	struct power_supply *batt_psy;
	struct power_supply *bbc_psy;
	struct power_supply *cp_psy;
#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
	struct power_supply *max_verify_psy;
#endif
	struct power_supply_desc fg_psy_d;

	struct batt_params	param;
	//struct delayed_work	soc_monitor_work;
	struct delayed_work overtemp_delay_work; //20220108 : W/A for over 60degree
	bool overtemp_delay_on; //20220108 : W/A for over 60degree
	bool overtemp_allow_restart; //20220108 : W/A for over 60degree
	struct delayed_work LowBatteryCheckWork;
	bool low_battery_power;
	bool start_low_battery_check;
};


static int show_registers(struct seq_file *m, void *data);
static bool fg_init(struct i2c_client *client);
static int fg_set_fastcharge_mode(struct sm_fg_chip *sm, bool enable);

static int __fg_read_word(struct i2c_client *client, u8 reg, u16 *val)
{
	s32 ret;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0) {
		pr_err("i2c read word fail: can't read from reg 0x%02X\n", reg);
		return ret;
	}

	*val = (u16)ret;

	return 0;
}

static int __fg_write_word(struct i2c_client *client, u8 reg, u16 val)
{
	s32 ret;

	ret = i2c_smbus_write_word_data(client, reg, val);
	if (ret < 0) {
		pr_err("i2c write word fail: can't write 0x%02X to reg 0x%02X\n",
				val, reg);
		return ret;
	}

	return 0;
}

static int fg_read_word(struct sm_fg_chip *sm, u8 reg, u16 *val)
{
	int ret;

	if (sm->skip_reads) {
		*val = 0;
		return 0;
	}
	/* TODO:check little endian */
	mutex_lock(&sm->i2c_rw_lock);
	ret = __fg_read_word(sm->client, reg, val);
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}

static int fg_write_word(struct sm_fg_chip *sm, u8 reg, u16 val)
{
	int ret;

	if (sm->skip_writes)
		return 0;

	/* TODO:check little endian */
	mutex_lock(&sm->i2c_rw_lock);
	ret = __fg_write_word(sm->client, reg, val);
	mutex_unlock(&sm->i2c_rw_lock);

	return ret;
}

#define	FG_STATUS_SLEEP				BIT(10)
#define	FG_STATUS_BATT_PRESENT		BIT(9)
#define	FG_STATUS_SOC_UPDATE		BIT(8)
#define	FG_STATUS_TOPOFF			BIT(7)
#define	FG_STATUS_LOW_SOC2			BIT(6)
#define	FG_STATUS_LOW_SOC1			BIT(5)
#define	FG_STATUS_HIGH_CURRENT		BIT(4)
#define	FG_STATUS_HIGH_TEMPERATURE	BIT(3)
#define	FG_STATUS_LOW_TEMPERATURE	BIT(2)
#define	FG_STATUS_HIGH_VOLTAGE		BIT(1)
#define	FG_STATUS_LOW_VOLTAGE		BIT(0)

#define	FG_OP_STATUS_CHG_DISCHG		BIT(15) //if can use the charger information, plz use the charger information for CHG/DISCHG condition.

static int fg_read_status(struct sm_fg_chip *sm)
{
	int ret;
	u16 flags1, flags2;

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_STATUS], &flags1);
	if (ret < 0)
		return ret;

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_FG_OP_STATUS], &flags2);
		if (ret < 0)
			return ret;

	mutex_lock(&sm->data_lock);
	sm->batt_present	= !!(flags1 & FG_STATUS_BATT_PRESENT);
	sm->batt_ot			= !!(flags1 & FG_STATUS_HIGH_TEMPERATURE);
	sm->batt_ut			= !!(flags1 & FG_STATUS_LOW_TEMPERATURE);
	sm->batt_fc			= !!(flags1 & FG_STATUS_TOPOFF);
	sm->batt_soc1		= !!(flags1 & FG_STATUS_LOW_SOC2);
	sm->batt_socp		= !!(flags1 & FG_STATUS_LOW_SOC1);
	sm->batt_dsg		= !!!(flags2 & FG_OP_STATUS_CHG_DISCHG);
	mutex_unlock(&sm->data_lock);

	return 0;
}

#if (FG_REMOVE_IRQ == 0)
static int fg_status_changed(struct sm_fg_chip *sm)
{
	cancel_delayed_work(&sm->monitor_work);
	schedule_delayed_work(&sm->monitor_work, 0);
	power_supply_changed(sm->fg_psy);

	return IRQ_HANDLED;
}

static irqreturn_t fg_irq_thread(int irq, void *dev_id)
{
	struct sm_fg_chip *sm = dev_id;
	int ret;
	u16 data_int, data_int_mask;

	/* Read INT */
	ret = fg_read_word(sm, sm->regs[SM_FG_REG_INT_MASK], &data_int_mask);
	if (ret < 0){
		pr_err("Failed to read INT_MASK, ret = %d\n", ret);	
		return ret;	
	}
	
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_INT_MASK], 0x8000 | data_int_mask);
    if (ret < 0) {
		pr_err("Failed to write 0x8000 | INIT_MARK, ret = %d\n", ret);
		return ret;
	}

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_INT], &data_int);
	if (ret < 0) {
		pr_err("Failed to write REG_INT, ret = %d\n", ret);
		return ret;
	}

	ret = fg_write_word(sm, sm->regs[SM_FG_REG_INT_MASK], 0x03FF & data_int_mask);
    if (ret < 0) {
		pr_err("Failed to write INIT_MARK, ret = %d\n", ret);
		return ret;
	}

	fg_status_changed(sm);

	pr_info("fg_read_int = 0x%x\n", data_int);

	return 0;
}
#endif

static int fg_read_soc(struct sm_fg_chip *sm)
{
	int ret;
	int soc = 0;
	u16 data = 0;
	static int pre_soc = 0;

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_SOC], &data);
	if (ret < 0) {
		pr_err("could not read SOC, ret = %d\n", ret);
		return pre_soc;
	} else {
		/*integer bit;*/
		soc = ((data&0x7f00)>>8) * 10;
		/* integer + fractional bit*/
		soc = soc + (((data&0x00ff)*10)/256);
		
		if (data & 0x8000) {
			pr_err("fg_read_soc data=%d\n",data);
			soc *= -1;
		}
	}

	 //pr_info("fg_read_soc soc=%d\n",soc);
	//return soc/10;
	pre_soc = soc;
	return soc;
}

static int fg_get_soc_decimal(struct sm_fg_chip *sm)
{
	int raw_soc;
	raw_soc = fg_read_soc(sm);
	return raw_soc % 10 * 10;
}

static int fg_get_soc_decimal_rate(struct sm_fg_chip *sm)
{
	int soc,i;

	if(sm->dec_rate_len <= 0)
		return 0;

	soc = fg_read_soc(sm);
	soc /= 10;

	for(i = 0; i < sm->dec_rate_len; i += 2){
		if(soc < sm->dec_rate_seq[i]){
			return sm->dec_rate_seq[i-1];
		}
	}
	return sm->dec_rate_seq[sm->dec_rate_len-1];
}

static unsigned int fg_read_ocv(struct sm_fg_chip *sm)
{
	int ret;	
	u16 data = 0;
	unsigned int ocv;// = 3500; /*3500 means 3500mV*/

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_OCV], &data);
	if (ret<0) {
		pr_err("could not read OCV, ret = %d\n", ret);
		ocv = 4000;
	} else {
		ocv = (((data&0x0fff)*1000)/2048) + (((data&0xf000)>>11)*1000);
	}

	return ocv; //mV
}

static int _calculate_battery_temp_ex(struct sm_fg_chip *sm, u16 uval)
{
	int i = 0, temp = 0;
	signed short val = 0;

	if ((uval >= 0x8001) && (uval <= 0x823B)) {
		pr_info("sp_range uval = 0x%x\n",uval);
		uval = 0x0000;
	}

	val = uval;
	
	if (val >= sm->battery_temp_table[0]) {
		temp = -20; //Min : -20
	} else if (val <= sm->battery_temp_table[FG_TEMP_TABLE_CNT_MAX-1]) {
		temp = 80; //Max : 80
	} else {
		for (i = 0; i < FG_TEMP_TABLE_CNT_MAX; i++) {
			if  (val >= sm->battery_temp_table[i]) {
				temp = -20 + i; 									  //[ex] ~-20 : -20(skip), -19.9~-19.0 : 19, -18.9~-18 : 18, .., 0.9~0 : 0
				if ((temp >= 1) && (val != sm->battery_temp_table[i])) //+ range 0~79 degree. In same value case, no needed (temp-1)
					temp = temp -1; 								  //[ex] 0.1~0.9 : 0, 1.1~1.9 : 1, .., 79.1~79.9 : 79
				break;
			}
		}
	}

	pr_info("uval = 0x%x, val = 0x%x, temp = %d\n",uval, val, temp);

	return temp;		
}

static int fg_read_temperature(struct sm_fg_chip *sm, enum sm_fg_temperature_type temperature_type)
{
	int ret, temp = 0;
	u16 data = 0;
	static int pre_temp = 0;

	switch (temperature_type) {
	case TEMPERATURE_IN:	
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_TEMPERATURE_IN], &data);
		if (ret < 0) {
			pr_err("could not read temperature in , ret = %d\n", ret);
			return pre_temp;
		} else {
			/*integer bit*/
			temp = ((data & 0x00FF));
			if (data & 0x8000)
				temp *= -1;
		}
		pr_info("fg_read_temperature_in temp_in=%d\n", temp);
		break;
	case TEMPERATURE_EX:
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_TEMPERATURE_EX], &data);
		if (ret < 0) {
			pr_err("could not read temperature ex , ret = %d\n", ret);
			return pre_temp;
		} else {
			temp = _calculate_battery_temp_ex(sm, data);
			//20220108 : W/A for over 60degree
			pr_info("Pre : temp = %d, overtemp_delay_on = %d\n", temp, sm->overtemp_delay_on);
			// 1. Check whether temperature is over 61.
			if (temp>=61) {
                          	if(!sm->overtemp_allow_restart)
                                {
                                  temp = 60;
				// 2. Check the overtemp_delay_on flag
                                  if (sm->overtemp_delay_on == false ) {
                                          sm->overtemp_delay_on = true;
                                          schedule_delayed_work(&sm->overtemp_delay_work, msecs_to_jiffies(20000)); //During 1sec, keep 60 degree
                                  }
                                }
                          	else
                                {
                                	pr_info("Post 0 : temp = %d, overtemp_delay_on = %d,overtemp_allow_restart=%d\n", temp, sm->overtemp_delay_on,sm->overtemp_allow_restart);
                                }
			} else {
				// Touch to under 61degree. Work_delay & flags are cleared.
				cancel_delayed_work(&sm->overtemp_delay_work);
				sm->overtemp_delay_on = false;
				sm->overtemp_allow_restart = false;
			}
			pr_info("Post 1 : temp = %d, overtemp_delay_on = %d,overtemp_allow_restart=%d\n", temp, sm->overtemp_delay_on,sm->overtemp_allow_restart);
		}	
		 pr_info("fg_read_temperature_ex temp_ex=%d\n", temp);
		break;
		
	default:
		return -EINVAL;
	}
	pre_temp = temp;
	return temp;
}

/*
 *	Return : mV
 */
static int fg_read_volt(struct sm_fg_chip *sm)
{
	int ret = 0;
	int volt = 0;
	u16 data = 0;
	static int pre_volt = 0;

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_VOLTAGE], &data);
	if (ret < 0) {
		pr_err("could not read voltage, ret = %d\n", ret);
		return pre_volt;
	}  else {
		/* */
		volt = 1800 * (data & 0x7FFF) / 19622;
		if (data&0x8000)
			volt *= -1;
		
		volt += 2700; 
	}

	/*cal avgvoltage*/
	sm->aver_batt_volt = (((sm->aver_batt_volt)*4) + volt)/5;
	pre_volt = volt;
	return volt;
}

static int fg_get_cycle(struct sm_fg_chip *sm)
{
	int ret;
	int cycle;
	u16 data = 0;
	static int pre_cycle = 0;

	ret = fg_read_word(sm, FG_REG_SOC_CYCLE, &data);
	if (ret<0) {
		pr_err("read cycle reg fail ret = %d\n", ret);
		cycle = pre_cycle;
	} else {
		cycle = data&0x01FF;
	}
	pre_cycle = cycle;
	return cycle;
}


static int fg_read_current(struct sm_fg_chip *sm)
{
	int ret, rsns = 0;
	u16 data = 0;
	//float curr = 0.0;
	int64_t temp = 0;
	int curr = 0;

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_CURRENT], &data);
	if (ret < 0) {
		pr_err("could not read current, ret = %d\n", ret);
		return ret;
	} else {
		/* */
		if (sm->batt_rsns == -EINVAL) {
			pr_err("could not read sm->batt_rsns, rsns = 10mohm\n");
			rsns = 10;
		} else {
			sm->batt_rsns == 0 ? rsns = 5 : (rsns = sm->batt_rsns*10);
		}

		//curr =(((float)(data & 0x7FFF) * 1000 / 4088) / ((float)rsns/10));
		temp = div_s64((data & 0x7FFF) * 1000 , 4088)*(10/rsns);
          	curr = temp;
		if(data & 0x8000)
			curr *= -1;
			
	}
	//pr_err("curr = %d,data=%d\n",(int)curr,data);
	pr_err("curr = %d,data=%d\n",curr,data);
	//return (int)curr;
	return curr;
}

static int fg_read_fcc(struct sm_fg_chip *sm)
{
	int ret = 0;
	int fcc = 0;	
	u16 data = 0;
	int64_t temp = 0;

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_BAT_CAP], &data);
	if (ret < 0) {
		pr_err("could not read FCC, ret=%d\n", ret);
		return ret;
	} else {
		/* */
		temp = div_s64((data & 0x7FFF) * 1000, 2048);
		fcc = temp;
	}

	return fcc;
}

#define FG_SOFT_RESET	0xA6 
static int fg_reset(struct sm_fg_chip *sm)
{
    int ret;

    ret = fg_write_word(sm, sm->regs[SM_FG_REG_RESET], FG_SOFT_RESET);
	if (ret < 0) {
		pr_err("could not reset, ret=%d\n", ret);
		return ret;
	}

    msleep(600);

    return 0;
}
static int fg_read_rmc(struct sm_fg_chip *sm)
{
	int ret = 0;
	int rmc = 0;	
	u16 data = 0;
	int64_t temp = 0;

	ret = fg_read_word(sm, FG_REG_RMC, &data);
	if (ret < 0) {
		pr_err("could not read RMC, ret=%d\n", ret);
		return ret;
	} else {
		/* */
		temp = div_s64((data & 0x7FFF) * 1000, 2048);
		rmc = temp;
	}

	return rmc;
}


static int get_battery_status(struct sm_fg_chip *sm)
{
	union power_supply_propval ret = {0,};
	int rc;

	if (sm->batt_psy == NULL)
		sm->batt_psy = power_supply_get_by_name("battery");
	if (sm->batt_psy) {
		/* if battery has been registered, use the status property */
		rc = power_supply_get_property(sm->batt_psy,
					POWER_SUPPLY_PROP_STATUS, &ret);
		if (rc) {
			pr_err("Battery does not export status: %d\n", rc);
			return POWER_SUPPLY_STATUS_UNKNOWN;
		}
		return ret.intval;
	}

	/* Default to false if the battery power supply is not registered. */
	pr_err("battery power supply is not registered\n");
	return POWER_SUPPLY_STATUS_UNKNOWN;
}

static bool is_battery_charging(struct sm_fg_chip *sm)
{
	return get_battery_status(sm) == POWER_SUPPLY_STATUS_CHARGING;
}

#ifdef ENABLE_TEMBASE_ZDSCON
static void fg_tembase_zdscon(struct sm_fg_chip *sm)
{
        u16 hminman_value = 0, data = 0;
        int ret = 0;
        int fg_temp_gap = sm->batt_temp - sm->temp_std;

        if (fg_temp_gap < 0)
        {
                fg_temp_gap = abs(fg_temp_gap);
                if (fg_temp_gap > ZDSCON_ACT_TEMP_GAP)
                {
                        hminman_value = sm->rs_value[3] + (((fg_temp_gap - ZDSCON_ACT_TEMP_GAP) * HMINMAN_VALUE_FACT) / TEMP_GAP_DENOM);
                        ret = fg_read_word(sm, FG_REG_RS_3, &data);
                        if (ret < 0) {
                                pr_err("could not read , ret = %d\n", ret);
                        }
                        else
                        {
                                if (data != hminman_value)
                                {
                                        fg_write_word(sm, FG_REG_RS_3, hminman_value);
                                        fg_write_word(sm, FG_REG_RS_0, hminman_value+2);
                                       pr_info("%s: hminman value set 0x%x tem(%d)\n", __func__, hminman_value, sm->batt_temp);
                                }
                        }
                }
                else
                {
                        ret = fg_read_word(sm, FG_REG_RS_3, &data);
                        if (ret < 0) {
                                pr_err("could not read , ret = %d\n", ret);
                        }
                        else
                        {
                                if (data != sm->rs_value[3])
                                {
                                        fg_write_word(sm, FG_REG_RS_3, sm->rs_value[3]);
                                        fg_write_word(sm, FG_REG_RS_0, sm->rs_value[0]);
                                        pr_info("%s: hminman value restore 0x%x -> 0x%x tem(%d)\n", __func__, data, sm->rs_value[3], sm->batt_temp);
                                }
                        }
                }
        }
        return;
}
#endif

static void fg_vbatocv_check(struct sm_fg_chip *sm)
{
	int top_off = 0;
	int ret = 0;
        u16 data = 0;

	pr_info("%s: sm->batt_curr (%d), sm->is_charging (%d), sm->top_off (%d), sm->batt_soc (%d)\n",
			__func__, sm->batt_curr, sm->is_charging, sm->top_off, sm->batt_soc);

	if (sm->fast_mode)
		top_off = sm->top_off *3;
	else
		top_off = sm->top_off;

	pr_info("%s: fast_charge_mode (%d), top_off (%d)\n",__func__, sm->fast_mode, top_off);

	ret = fg_read_word(sm, FG_REG_RS_0, &data);
        if (ret < 0) {
                pr_err("could not read , ret = %d\n", ret);
        }

	if(((abs(sm->batt_curr)<50) && (abs(sm->batt_curr) > 10))||
			((sm->is_charging) && (sm->batt_curr<(top_off)) &&
			 (sm->batt_curr>(top_off/3)) && (sm->batt_soc>=900)))
	{
		if(abs(sm->batt_ocv-sm->batt_volt)>30)
		{
			sm->iocv_error_count ++;
		}

		pr_info("%s: sm5602 FG iocv_error_count (%d)\n", __func__, sm->iocv_error_count);

		if(sm->iocv_error_count > 5)
			sm->iocv_error_count = 6;
	}
	else
	{
		sm->iocv_error_count = 0;
	}

	if(sm->iocv_error_count > 5)
	{
		pr_info("%s: p_v - v = (%d)\n", __func__, sm->p_batt_voltage - sm->batt_volt);
		if(abs(sm->p_batt_voltage - sm->batt_volt)>15)
		{
			sm->iocv_error_count = 0;
		}
		else
		{
			fg_write_word(sm, FG_REG_RS_2, data);
			pr_info("%s: mode change to RS m mode ox%x\n", __func__,data);
		}
	}
	else
	{
		if((sm->p_batt_voltage < sm->n_tem_poff) &&
			(sm->batt_volt < sm->n_tem_poff) && (!sm->is_charging))
		{
			if((sm->p_batt_voltage <
				(sm->n_tem_poff - sm->n_tem_poff_offset)) &&
				(sm->batt_volt <
				(sm->n_tem_poff - sm->n_tem_poff_offset)))
			{
				fg_write_word(sm, FG_REG_RS_2, data>>1);
				pr_info("%s: mode change to normal tem RS m mode >>1 0x%x\n", __func__,data>>1);
			}
			else
			{
				fg_write_word(sm, FG_REG_RS_2, data);
				pr_info("%s: mode change to normal tem RS m mode 0x%x\n", __func__,data);
			}
		}
		else
		{
			pr_info("%s: mode change to RS a mode\n", __func__);

			fg_write_word(sm, FG_REG_RS_2, sm->rs_value[2]);
		}
	}
	sm->p_batt_voltage = sm->batt_volt;
	sm->p_batt_current = sm->batt_curr;
	// iocv error case cover end
}


static int fg_cal_carc (struct sm_fg_chip *sm)
{
	int curr_cal = 0, p_curr_cal=0, n_curr_cal=0, p_delta_cal=0, n_delta_cal=0, p_fg_delta_cal=0, n_fg_delta_cal=0, temp_curr_offset=0;
	int temp_gap, fg_temp_gap = 0;
	int ret = 0;
	u16 data[8] = {0,};
#ifdef ENABLE_MIX_COMP 
	u16 temp_aging_ctrl = 0;
#endif
	struct power_supply *sc8551_psy;
	union power_supply_propval pval = {0, };
        int sc8551_pump_work_flag = 0;

#ifdef ENABLE_TEMBASE_ZDSCON
        fg_tembase_zdscon(sm);
#endif
	fg_vbatocv_check(sm);

	sm->is_charging = is_battery_charging(sm); //From Charger Driver

	//fg_temp_gap = (sm->batt_temp/10) - sm->temp_std;
	fg_temp_gap = sm->batt_temp - sm->temp_std;	
	
	temp_curr_offset = sm->curr_offset;
	if(sm->en_high_fg_temp_offset && (fg_temp_gap > 0))
	{
		if(temp_curr_offset & 0x0080)
		{
			temp_curr_offset = -(temp_curr_offset & 0x007F);
		}
		temp_curr_offset = temp_curr_offset + (fg_temp_gap / sm->high_fg_temp_offset_denom)*sm->high_fg_temp_offset_fact;
		if(temp_curr_offset < 0)
		{
			temp_curr_offset = -temp_curr_offset;
			temp_curr_offset = temp_curr_offset|0x0080;
		}
	}
	else if (sm->en_low_fg_temp_offset && (fg_temp_gap < 0))
	{
		if(temp_curr_offset & 0x0080)
		{
			temp_curr_offset = -(temp_curr_offset & 0x007F);
		}
		temp_curr_offset = temp_curr_offset + ((-fg_temp_gap) / sm->low_fg_temp_offset_denom)*sm->low_fg_temp_offset_fact;
		if(temp_curr_offset < 0)
		{
			temp_curr_offset = -temp_curr_offset;
			temp_curr_offset = temp_curr_offset|0x0080;
		}
	}
    temp_curr_offset = temp_curr_offset | (temp_curr_offset<<8);
	ret = fg_write_word(sm, FG_REG_CURR_IN_OFFSET, temp_curr_offset);
	if (ret < 0) {
		pr_err("Failed to write CURR_IN_OFFSET, ret = %d\n", ret);
		return ret;
	} else {
		pr_err("CURR_IN_OFFSET [0x%x] = 0x%x\n", FG_REG_CURR_IN_OFFSET, temp_curr_offset);
	}

	n_curr_cal = (sm->curr_slope & 0xFF00)>>8;
	p_curr_cal = (sm->curr_slope & 0x00FF);
	
	if (sm->en_high_fg_temp_cal && (fg_temp_gap > 0))
	{
		p_fg_delta_cal = (fg_temp_gap / sm->high_fg_temp_p_cal_denom)*sm->high_fg_temp_p_cal_fact;
		n_fg_delta_cal = (fg_temp_gap / sm->high_fg_temp_n_cal_denom)*sm->high_fg_temp_n_cal_fact;
	}
	else if (sm->en_low_fg_temp_cal && (fg_temp_gap < 0))
	{
		fg_temp_gap = -fg_temp_gap;
		p_fg_delta_cal = (fg_temp_gap / sm->low_fg_temp_p_cal_denom)*sm->low_fg_temp_p_cal_fact;
		n_fg_delta_cal = (fg_temp_gap / sm->low_fg_temp_n_cal_denom)*sm->low_fg_temp_n_cal_fact;
	}
	p_curr_cal = p_curr_cal + (p_fg_delta_cal);
	n_curr_cal = n_curr_cal + (n_fg_delta_cal);

	//temp_gap = (sm->batt_temp/10) - sm->temp_std;
	temp_gap = sm->batt_temp - sm->temp_std;	
	if (sm->en_high_temp_cal && (temp_gap > 0))
	{
		p_delta_cal = (temp_gap / sm->high_temp_p_cal_denom)*sm->high_temp_p_cal_fact;
		n_delta_cal = (temp_gap / sm->high_temp_n_cal_denom)*sm->high_temp_n_cal_fact;
	}
	else if (sm->en_low_temp_cal && (temp_gap < 0))
	{
		temp_gap = -temp_gap;
		p_delta_cal = (temp_gap / sm->low_temp_p_cal_denom)*sm->low_temp_p_cal_fact;
		n_delta_cal = (temp_gap / sm->low_temp_n_cal_denom)*sm->low_temp_n_cal_fact;
	}
	p_curr_cal = p_curr_cal + (p_delta_cal);
	n_curr_cal = n_curr_cal + (n_delta_cal);

    curr_cal = (n_curr_cal << 8) | p_curr_cal;
	//PD Charging

	sc8551_psy = power_supply_get_by_name("sc8551-standalone");
	if (sc8551_psy != NULL) {
		ret = power_supply_get_property(sc8551_psy,
				POWER_SUPPLY_PROP_CHARGING_ENABLED, &pval);
		if (ret < 0) {
			pr_err("bq2589x_charger:get sc8551_psy charge property enable error.\n");
                } else {
                sc8551_pump_work_flag = pval.intval;
                }
	} else {
                pr_err("bq2589x_charger:sc8551_psy = power_supply_get_by_name(sc8551-standalone) error.\n");
        }

	if (sm->fast_mode) {
		curr_cal = (n_curr_cal << 8) | (p_curr_cal + sm->fcm_offset);
	}
	ret = fg_write_word(sm, FG_REG_CURR_IN_SLOPE, curr_cal);
	if (ret < 0) {
		pr_err("Failed to write CURR_IN_SLOPE, ret = %d\n", ret);
		return ret; 
	} else {
		pr_err("write CURR_IN_SLOPE [0x%x] = 0x%x\n", FG_REG_CURR_IN_SLOPE, curr_cal);
	}

#ifdef ENABLE_MIX_COMP
       ret = fg_read_word(sm, FG_REG_AGING_CTRL, &temp_aging_ctrl);
       if ((sm->batt_temp < 8) && (!sm->is_charging) 
		&& ((sm->batt_soc < 100 && sm->batt_soc > 20)
			||(sm->batt_soc < 300 && sm->batt_soc > 200)
			||(sm->batt_soc < 500 && sm->batt_soc > 400))){
                if (sm->aging_ctrl == temp_aging_ctrl) {
                        ret = fg_write_word(sm, FG_REG_AGING_CTRL, (sm->aging_ctrl & 0xFFFE));
                        if (ret < 0) {
                                pr_err("could not write FG_REG_AGING_CTRL, ret = %d\n", ret);
                                return ret;
                        }
                }
        } else {
                if (sm->aging_ctrl != temp_aging_ctrl) {
                        ret = fg_write_word(sm, FG_REG_AGING_CTRL, (sm->aging_ctrl));
                        if (ret < 0) {
                                pr_err("could not write FG_REG_AGING_CTRL, ret = %d\n", ret);
                                return ret;
                        }
                }
        }
        pr_info("0x9C=0x%x\n", temp_aging_ctrl);
#endif

        ret |= fg_read_word(sm, 0x06, &data[0]);
        ret |= fg_read_word(sm, 0x28, &data[1]);
        ret |= fg_read_word(sm, 0x83, &data[2]);
        ret |= fg_read_word(sm, 0x84, &data[3]);
        ret |= fg_read_word(sm, 0x86, &data[4]);
        ret |= fg_read_word(sm, 0x87, &data[5]);
        ret |= fg_read_word(sm, 0x93, &data[6]);
	ret |= fg_read_word(sm, 0x82, &data[7]);
	if (ret < 0) {
		pr_err("could not read , ret = %d\n", ret);
		return ret;
	} else
		pr_info("0x06=0x%x, 0x28=0x%x, 0x83=0x%x, 0x84=0x%x, 0x86=0x%x, 0x87=0x%x, 0x93=0x%x, 0x82=0x%x\n",
                        data[0],data[1], data[2],data[3], data[4],data[5], data[6], data[7]);

	ret = fg_read_word(sm, 0x82, &data[0]);
	if (ret < 0) {
		pr_err("could not read , ret = %d\n", ret);
		return ret;
	} else
		pr_err("0x82=0x%x\n", data[0]);

	return 1;
}

extern bool bq2589x_is_charge_done(void);
static int fg_get_batt_status(struct sm_fg_chip *sm)
{
	bool charge_done = 0;

	if(sm->batt_fc){
		charge_done = bq2589x_is_charge_done();
	}


	if (!sm->batt_present)
		return POWER_SUPPLY_STATUS_UNKNOWN;
	else if (sm->batt_fc && charge_done && (sm->batt_soc/10 > 95))
		return POWER_SUPPLY_STATUS_FULL;
	else if (sm->batt_dsg)
		return POWER_SUPPLY_STATUS_DISCHARGING;
	else if (sm->batt_curr > 0)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;



}


static int fg_get_batt_capacity_level(struct sm_fg_chip *sm)
{
	if (!sm->batt_present)
		return POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
	else if (sm->batt_fc)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (sm->batt_soc1)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (sm->batt_socp)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;

}


static int fg_get_batt_health(struct sm_fg_chip *sm)
{
	if (!sm->batt_present)
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	else if (sm->batt_ot)
		return POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (sm->batt_ut)
		return POWER_SUPPLY_HEALTH_COLD;
	else
		return POWER_SUPPLY_HEALTH_GOOD;
}

static int get_battery_id(void)
{
	struct power_supply *max_verify_psy;
	static int battery_id = 0;
	union power_supply_propval pval = {0, };
	int rc;

	max_verify_psy = power_supply_get_by_name("batt_verify");
	if (max_verify_psy != NULL) {
		rc = power_supply_get_property(max_verify_psy,
				POWER_SUPPLY_PROP_CHIP_OK, &pval);
		if (rc < 0)
			pr_err("fgauge_get_profile_id: get romid error.\n");
	}

	if (pval.intval == true) {
		rc = power_supply_get_property(max_verify_psy,
				POWER_SUPPLY_PROP_PAGE0_DATA, &pval);
		if (rc < 0) {
			pr_err("fgauge_get_profile_id: get page0 error.\n");
		} else {
			if (pval.arrayval[0] == 'N') {
				battery_id = BATTERY_VENDOR_NVT;
			} else if (pval.arrayval[0] == 'C') {
				battery_id = BATTERY_VENDOR_GY;
			} else if (pval.arrayval[0] == 'V') {
				battery_id = BATTERY_VENDOR_GY;
			} else if (pval.arrayval[0] == 'L') {
				battery_id = BATTERY_VENDOR_XWD;
			} else if (pval.arrayval[0] == 'S') {
				battery_id = BATTERY_VENDOR_XWD;
			} else if (pval.arrayval[0] == 'X') {
				battery_id = BATTERY_VENDOR_XWD;
			}
		}
	}

	pr_info("fgauge_get_profile_id: get_battery_id=%d.\n", battery_id);

	return battery_id;
}

static enum power_supply_property fg_props[] = {
#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
	POWER_SUPPLY_PROP_AUTHENTIC,
	POWER_SUPPLY_PROP_ROMID,
	POWER_SUPPLY_PROP_DS_STATUS,
	POWER_SUPPLY_PROP_PAGE0_DATA,
	POWER_SUPPLY_PROP_CHIP_OK,
#endif
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_SHUTDOWN_DELAY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_RESISTANCE_ID,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_SOC_DECIMAL,
	POWER_SUPPLY_PROP_SOC_DECIMAL_RATE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_FASTCHARGE_MODE,
	POWER_SUPPLY_PROP_BATTERY_TYPE,
	POWER_SUPPLY_PROP_RESISTANCE,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_SOH,
};

static void fg_monitor_workfunc(struct work_struct *work);

#define SHUTDOWN_DELAY_VOL	3300
static int fg_get_property(struct power_supply *psy, enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct sm_fg_chip *sm = power_supply_get_drvdata(psy);
	union power_supply_propval b_val = {0,};
	int ret;
	int vbat_uv;
	static bool last_shutdown_delay;
	//u16 flags;

#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
	if (sm->max_verify_psy == NULL)
		sm->max_verify_psy = power_supply_get_by_name("batt_verify");
	if ((psp == POWER_SUPPLY_PROP_AUTHENTIC)
		|| (psp == POWER_SUPPLY_PROP_ROMID)
		|| (psp == POWER_SUPPLY_PROP_DS_STATUS)
		|| (psp == POWER_SUPPLY_PROP_PAGE0_DATA)
		|| (psp == POWER_SUPPLY_PROP_CHIP_OK)) {
		if (sm->max_verify_psy == NULL) {
			pr_err("max_verify_psy is NULL\n");
			return -ENODATA;
		}
	}
#endif
	if (sm->cp_psy == NULL)
		sm->cp_psy = power_supply_get_by_name("sc8551-standalone");

	switch (psp) {
#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
	case POWER_SUPPLY_PROP_AUTHENTIC:
		ret = power_supply_get_property(sm->max_verify_psy,
					POWER_SUPPLY_PROP_AUTHEN_RESULT, &b_val);
		val->intval = b_val.intval;
		break;
	case POWER_SUPPLY_PROP_ROMID:
		ret = power_supply_get_property(sm->max_verify_psy,
					POWER_SUPPLY_PROP_ROMID, &b_val);
		memcpy(val->arrayval, b_val.arrayval, 8);
		break;
	case POWER_SUPPLY_PROP_DS_STATUS:
		ret = power_supply_get_property(sm->max_verify_psy,
					POWER_SUPPLY_PROP_DS_STATUS, &b_val);
		memcpy(val->arrayval, b_val.arrayval, 8);
		break;
	case POWER_SUPPLY_PROP_PAGE0_DATA:
		ret = power_supply_get_property(sm->max_verify_psy,
					POWER_SUPPLY_PROP_PAGE0_DATA, &b_val);
		memcpy(val->arrayval, b_val.arrayval, 16);
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		ret = power_supply_get_property(sm->max_verify_psy,
					POWER_SUPPLY_PROP_CHIP_OK, &b_val);
		val->intval = b_val.intval;
		break;
#endif
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = fg_get_batt_status(sm);
/*		pr_info("fg POWER_SUPPLY_PROP_STATUS:%d\n", val->intval);*/
		break;
	case POWER_SUPPLY_PROP_SHUTDOWN_DELAY:
		val->intval = sm->shutdown_delay;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if(sm->usb_present && sm->cp_psy) {
			ret = power_supply_get_property(sm->cp_psy,
					POWER_SUPPLY_PROP_SC_BATTERY_VOLTAGE, &b_val);
			val->intval = b_val.intval * 1000;
			break;
		}
		mutex_lock(&sm->data_lock);
		ret = fg_read_volt(sm);
		if (ret >= 0)
			sm->batt_volt = ret;
		val->intval = sm->batt_volt * 1000;
		mutex_unlock(&sm->data_lock);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = sm->batt_present;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		mutex_lock(&sm->data_lock);
		sm->batt_curr = fg_read_current(sm);
//		pr_err("zhushaoan: real batt_curr:%d\n", sm->batt_curr);
		val->intval = sm->batt_curr * 1000;
		val->intval = val->intval;
		/*
		pval.intval = fg_get_batt_status(sm);
		if ((pval.intval == POWER_SUPPLY_STATUS_DISCHARGING) ||
			(pval.intval == POWER_SUPPLY_STATUS_NOT_CHARGING)) {
			if (val->intval > 0)
				val->intval = 0;
		}*/

		mutex_unlock(&sm->data_lock);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (sm->fake_soc >= 0) {
			val->intval = sm->fake_soc;
			break;
		}
		ret = fg_read_soc(sm);
		mutex_lock(&sm->data_lock);
		if (ret >= 0)
			sm->batt_soc = ret;
		if (sm->param.batt_soc >= 0)
			val->intval = sm->param.batt_soc/10;
		else if ((ret >= 0) && (sm->param.batt_soc == -EINVAL))
			val->intval = (sm->batt_soc > 16) ? ((sm->batt_soc*10 + 96)/97) : (sm->batt_soc/10) ;
		else
			val->intval = 50;

		/* capacity should be between 0% and 100% */
		if (val->intval > 100)
			val->intval = 100;
		if (val->intval < 0)
			val->intval = 0;

		mutex_unlock(&sm->data_lock);
		if (sm->shutdown_delay_enable) {
			if (val->intval == 0) {
				sm->is_charging = is_battery_charging(sm);
				vbat_uv = fg_read_volt(sm);

				if (sm->is_charging && sm->shutdown_delay) {
					sm->shutdown_delay = false;
					val->intval = 1;
				} else {
					/* When the vbat is greater than 3400mv, SOC still reported 1 to avoid high shutdown volt */
					if (vbat_uv > 3400) {
						val->intval = 1;
					} else if (vbat_uv > 3300) {
						if (!sm->is_charging) {
							sm->shutdown_delay = true;
						}
						val->intval = 1;
					} else {
						sm->shutdown_delay = false;
						val->intval = 0;
					}
				}
			} else {
				sm->shutdown_delay = false;
			}
			if (last_shutdown_delay != sm->shutdown_delay) {
				last_shutdown_delay = sm->shutdown_delay;
				if (sm->batt_psy) {
					power_supply_changed(sm->batt_psy);
				}
				if (sm->fg_psy) {
					power_supply_changed(sm->fg_psy);
				}
			}
		}
		break;

	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = fg_get_batt_capacity_level(sm);
		break;

	case POWER_SUPPLY_PROP_TEMP:
		mutex_lock(&sm->data_lock);
		if (sm->fake_temp != -EINVAL) {
			val->intval = sm->fake_temp;
			mutex_unlock(&sm->data_lock);
			break;
		}
		if (sm->en_temp_in)
			ret = fg_read_temperature(sm, TEMPERATURE_IN);
		else if (sm->en_temp_ex)
			ret = fg_read_temperature(sm, TEMPERATURE_EX);
		else 
			ret = -ENODATA;
		if (ret > 0)
			sm->batt_temp = ret;
		val->intval = sm->batt_temp*10;
		mutex_unlock(&sm->data_lock);
		break;
	case POWER_SUPPLY_PROP_RESISTANCE_ID:
		val->intval = get_battery_id();
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		ret = fg_read_fcc(sm);
		mutex_lock(&sm->data_lock);
		if (ret > 0)
			sm->batt_fcc = ret;
		val->intval = sm->batt_fcc * 1000;
		mutex_unlock(&sm->data_lock);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = 5000000;
		break;
	case POWER_SUPPLY_PROP_SOC_DECIMAL:
		val->intval = fg_get_soc_decimal(sm);
		break;
	case POWER_SUPPLY_PROP_SOC_DECIMAL_RATE:
		val->intval = fg_get_soc_decimal_rate(sm);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = fg_get_batt_health(sm);
		break;

	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		val->intval = sm->fast_mode;
		break;
	case POWER_SUPPLY_PROP_BATTERY_TYPE:
		switch (get_battery_id()) {
			case BATTERY_VENDOR_NVT:
				val->strval = "M376-NVT-5000mAh";
				break;
			case BATTERY_VENDOR_GY:
				val->strval = "M376-GuanYu-5000mAh";
				break;
			case BATTERY_VENDOR_XWD:
				val->strval = "M376-Sunwoda-5000mAh";
				break;
			default:
				val->strval = "M376-unknown-5000mAh";
				break;
		}
		break;
	case POWER_SUPPLY_PROP_RESISTANCE:
		val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = sm->batt_soc_cycle;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = sm->param.batt_soc /10 * 49;
		break;
	case POWER_SUPPLY_PROP_SOH:
		val->intval = 100;
		break;
/*	
	case POWER_SUPPLY_PROP_CHIP_OK:
		if (sm->fake_chip_ok != -EINVAL) {
			val->intval = sm->fake_chip_ok;
			break;
		}
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_STATUS], &flags);
		if (ret < 0)
			val->intval = 0;
		else
			val->intval = 1;
		break;
*/
	default:
		pr_info(" wsy default err fg_get_property psp=%d\n",psp);
		return -EINVAL;
	}

	return 0;
}

static int fg_set_property(struct power_supply *psy,
				       enum power_supply_property prop,
				       const union power_supply_propval *val)
{
	struct sm_fg_chip *sm = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
		sm->fake_temp = val->intval;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		sm->fake_soc = val->intval;
		power_supply_changed(sm->fg_psy);
		break;
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
		fg_set_fastcharge_mode(sm, !!val->intval);
		break;
	case POWER_SUPPLY_PROP_CHIP_OK:
		sm->fake_chip_ok = !!val->intval;
		break;	
	default:
		return -EINVAL;
	}

	return 0;
}

static int fg_prop_is_writeable(struct power_supply *psy,
				       enum power_supply_property prop)
{
	int ret;

	switch (prop) {
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_FASTCHARGE_MODE:
	case POWER_SUPPLY_PROP_CHIP_OK:
		ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static void fg_external_power_changed(struct power_supply *psy)
{
	struct sm_fg_chip *sm = power_supply_get_drvdata(psy);

	cancel_delayed_work(&sm->monitor_work);
	schedule_delayed_work(&sm->monitor_work, 0);
}

static int fg_psy_register(struct sm_fg_chip *sm)
{
	struct power_supply_config fg_psy_cfg = {};

	//sm->fg_psy.name = "sm_bms";
	sm->fg_psy_d.name = "bms";	
	sm->fg_psy_d.type = POWER_SUPPLY_TYPE_BMS;
	sm->fg_psy_d.properties = fg_props;
	sm->fg_psy_d.num_properties = ARRAY_SIZE(fg_props);
	sm->fg_psy_d.get_property = fg_get_property;
	sm->fg_psy_d.set_property = fg_set_property;
	sm->fg_psy_d.external_power_changed = fg_external_power_changed;
	sm->fg_psy_d.property_is_writeable = fg_prop_is_writeable;

	fg_psy_cfg.drv_data = sm;
	fg_psy_cfg.num_supplicants = 0;

	sm->fg_psy = devm_power_supply_register(sm->dev, &sm->fg_psy_d, &fg_psy_cfg);
	if (IS_ERR(sm->fg_psy)) {
		pr_err("Failed to register fg_psy");
		return PTR_ERR(sm->fg_psy);
	}

	return 0;
}

static void fg_psy_unregister(struct sm_fg_chip *sm)
{
	power_supply_unregister(sm->fg_psy);
}

static const u8 fg_dump_regs[] = {
	0x00, 0x01, 0x03, 0x04,
	0x05, 0x06, 0x07, 0x08,
	0x09, 0x0A, 0x0C, 0x0D,
	0x0E, 0x0F, 0x10, 0x11,
	0x12, 0x13, 0x14, 0x1A, 
	0x1B, 0x1C, 0x62, 0x73, 
	0x74, 0x90, 0x91, 0x95, 
	0x96
};


#if 0
static int fg_dump_debug(struct sm_fg_chip *sm)
{
	int i;
	int ret;
	u16 val = 0;

	for (i = 0; i < ARRAY_SIZE(fg_dump_regs); i++) {
		ret = fg_read_word(sm, fg_dump_regs[i], &val);
		if (!ret)
			pr_info("Reg[0x%02X] = 0x%02X\n",
						fg_dump_regs[i], val);
	}
	return 0;
}
#endif



static int reg_debugfs_open(struct inode *inode, struct file *file)
{
	struct sm_fg_chip *sm = inode->i_private;

	return single_open(file, show_registers, sm);
}

static const struct file_operations reg_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= reg_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void create_debugfs_entry(struct sm_fg_chip *sm)
{
	sm->debug_root = debugfs_create_dir("sm_fg", NULL);
	if (!sm->debug_root)
		pr_err("Failed to create debug dir\n");

	if (sm->debug_root) {

		debugfs_create_file("registers", S_IFREG | S_IRUGO,
						sm->debug_root, sm, &reg_debugfs_ops);

		debugfs_create_x32("fake_soc",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  sm->debug_root,
					  &(sm->fake_soc));

		debugfs_create_x32("fake_temp",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  sm->debug_root,
					  &(sm->fake_temp));

		debugfs_create_x32("skip_reads",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  sm->debug_root,
					  &(sm->skip_reads));
		debugfs_create_x32("skip_writes",
					  S_IFREG | S_IWUSR | S_IRUGO,
					  sm->debug_root,
					  &(sm->skip_writes));
	}
}

static int show_registers(struct seq_file *m, void *data)
{
	struct sm_fg_chip *sm = m->private;
	int i;
	int ret;
	u16 val = 0;

	for (i = 0; i < ARRAY_SIZE(fg_dump_regs); i++) {
		ret = fg_read_word(sm, fg_dump_regs[i], &val);
		if (!ret)
			seq_printf(m, "Reg[0x%02X] = 0x%02X\n",
						fg_dump_regs[i], val);
	}
	return 0;
}

static ssize_t fg_attr_show_rm(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sm_fg_chip *sm = i2c_get_clientdata(client);
	int rm, len;

	rm = fg_read_rmc(sm);
	len = snprintf(buf, 1024, "%d\n", rm);

	return len;
}

static ssize_t fg_attr_show_fcc(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sm_fg_chip *sm = i2c_get_clientdata(client);
	int fcc, len;

	fcc = fg_read_fcc(sm);
	len = snprintf(buf, 1024, "%d\n", fcc);

	return len;
}

static ssize_t fg_attr_show_batt_volt(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sm_fg_chip *sm = i2c_get_clientdata(client);
	int fcc, len;

	fcc = fg_read_volt(sm);
	len = snprintf(buf, 1024, "%d\n", fcc);

	return len;
}

static DEVICE_ATTR(rm, S_IRUGO, fg_attr_show_rm, NULL);
static DEVICE_ATTR(fcc, S_IRUGO, fg_attr_show_fcc, NULL);
static DEVICE_ATTR(batt_volt, S_IRUGO, fg_attr_show_batt_volt, NULL);

static struct attribute *fg_attributes[] = {
	&dev_attr_rm.attr,
	&dev_attr_fcc.attr,
	&dev_attr_batt_volt.attr,
	NULL,
};

static const struct attribute_group fg_attr_group = {
	.attrs = fg_attributes,
};

static void battery_soc_smooth_tracking_new(struct sm_fg_chip *sm)
{
	static int system_soc;

	system_soc = sm->param.batt_raw_soc * 10;

	sm->param.batt_soc = system_soc * 10;
}

#define MONITOR_SOC_WAIT_MS		1000
#define MONITOR_SOC_WAIT_PER_MS		10000
static int fg_set_fastcharge_mode(struct sm_fg_chip *sm, bool enable)
{
	int ret = 0;

	sm->fast_mode = enable;

	return ret;
}

static void fg_refresh_status(struct sm_fg_chip *sm)
{
	bool last_batt_inserted;
	bool last_batt_fc;
	bool last_batt_ot;
	bool last_batt_ut;
	static int last_soc, last_temp;
	union power_supply_propval b_val = {0,};
	union power_supply_propval u_val = {0,};
	int cp_vbat = 0, ret;

	last_batt_inserted	= sm->batt_present;
	last_batt_fc		= sm->batt_fc;
	last_batt_ot		= sm->batt_ot;
	last_batt_ut		= sm->batt_ut;

	fg_read_status(sm);

	if (!last_batt_inserted && sm->batt_present) {/* battery inserted */
		pr_info("Battery inserted\n");
	} else if (last_batt_inserted && !sm->batt_present) {/* battery removed */
		pr_info("Battery removed\n");
		sm->batt_soc	= -ENODATA;
		sm->batt_fcc	= -ENODATA;
		sm->batt_volt	= -ENODATA;
		sm->batt_curr	= -ENODATA;
		sm->batt_temp	= -ENODATA;
	}

	if ((last_batt_inserted != sm->batt_present)
		|| (last_batt_fc != sm->batt_fc)
		|| (last_batt_ot != sm->batt_ot)
		|| (last_batt_ut != sm->batt_ut))
		power_supply_changed(sm->fg_psy);

	if (sm->batt_present) {
		sm->batt_soc = fg_read_soc(sm);
		sm->batt_ocv = fg_read_ocv(sm);
		sm->batt_volt = fg_read_volt(sm);
		sm->batt_curr = fg_read_current(sm);
		sm->batt_soc_cycle = fg_get_cycle(sm);
		sm->batt_rmc = fg_read_rmc(sm);
		if (sm->en_temp_in)
			sm->batt_temp = fg_read_temperature(sm, TEMPERATURE_IN);
		else if (sm->en_temp_ex)
			sm->batt_temp = fg_read_temperature(sm, TEMPERATURE_EX);
		else 
			sm->batt_temp = -ENODATA;
		fg_cal_carc(sm);

		if (sm->cp_psy == NULL)
			sm->cp_psy = power_supply_get_by_name("sc8551-standalone");
		if(sm->usb_present && sm->cp_psy) {
			ret = power_supply_get_property(sm->cp_psy,
					POWER_SUPPLY_PROP_SC_BATTERY_VOLTAGE, &b_val);
			cp_vbat = b_val.intval;
		}

		if ((last_soc != sm->batt_soc) || (last_temp != sm->batt_temp)) {
			if (sm->fg_psy)
				power_supply_changed(sm->fg_psy);
			if (sm->batt_psy)
				power_supply_changed(sm->batt_psy);
		}

		last_soc = sm->batt_soc;
		last_temp = sm->batt_temp;
		/* Update battery information */
		sm->param.batt_ma = sm->batt_curr;
		sm->param.batt_raw_soc = sm->batt_soc;


		if (!sm->usb_psy)
			sm->usb_psy = power_supply_get_by_name("usb");
		if (sm->usb_psy) {
			ret = power_supply_get_property(sm->usb_psy, POWER_SUPPLY_PROP_ONLINE, &u_val);
			if (ret < 0) {
				pr_err("sm could not get real type!\n");
			}
		}
		if(!u_val.intval){
			/*check vbat when reach power-off threshold*/
			if(!sm->low_battery_power){
				if((sm->batt_soc < 10 && sm->batt_volt < 3400) && (!sm->start_low_battery_check))
				{
					sm->start_low_battery_check =true;
					schedule_delayed_work(&sm->LowBatteryCheckWork, 0);
				}
			}else{
				sm->param.batt_raw_soc = 0;
				sm->batt_soc = 0;
				cancel_delayed_work(&sm->LowBatteryCheckWork);
			}
		}else{
			cancel_delayed_work(&sm->LowBatteryCheckWork);
			sm->start_low_battery_check =false;
			sm->low_battery_power = false;
		}

		sm->soc_reporting_ready = 1;

		if (sm->soc_reporting_ready)
			battery_soc_smooth_tracking_new(sm);
	}

	//sm->last_update = jiffies;

}

#define SM5602_FFC_TERM_WAM_TEMP		350
#define SM5602_COLD_TEMP_TERM			0
#define SM5602_FFC_FULL_FV				8940
#define SM5602_NOR_FULL_FV				8880
#define BAT_FULL_CHECK_TIME				1

static int fg_check_full_status(struct sm_fg_chip *sm)
{
	union power_supply_propval prop = {0, };
	static int last_term, full_check;
	int term_curr, full_volt, rc;
	int interval = MONITOR_WORK_10S;


	if (!sm->usb_psy)
		return interval;

	if (!sm->chg_dis_votable)
		sm->chg_dis_votable = find_votable("CHG_DISABLE");

	if (!sm->fv_votable)
		sm->fv_votable = find_votable("BBC_FV");

	rc = power_supply_get_property(sm->usb_psy,
		POWER_SUPPLY_PROP_PRESENT, &prop);
	if (!prop.intval) {
		//vote(sm->chg_dis_votable, BMS_FC_VOTER, false, 0);
		sm->batt_sw_fc = false;
		full_check = 0;
		return interval;
	}

	if (sm->fast_mode) {
		interval = MONITOR_WORK_1S;
	} else {
		if (sm->batt_temp < SM5602_COLD_TEMP_TERM) {
		} else
		if (sm->usb_present)
			interval = MONITOR_WORK_5S;
		else
			interval = MONITOR_WORK_10S;
	}
	full_volt = get_effective_result(sm->fv_votable) / 1000 - 20;

	if (sm->usb_present && sm->batt_soc == SM_RAW_SOC_FULL && sm->batt_volt > full_volt &&
			sm->batt_curr < 0 && (sm->batt_curr > term_curr * (-1)) &&
			!sm->batt_sw_fc) {
		full_check++;
		if (full_check > BAT_FULL_CHECK_TIME) {
			sm->batt_sw_fc = true;
			//vote(sm->chg_dis_votable, BMS_FC_VOTER, true, 0);
		}
		return MONITOR_WORK_1S;
	} else {
		full_check = 0;
	}

	if (term_curr == last_term)
		return interval;

	if (!sm->bbc_psy)
		sm->bbc_psy = power_supply_get_by_name("bbc");
	if (sm->bbc_psy) {
		prop.intval = term_curr;
		rc = power_supply_get_property(sm->bbc_psy,
			POWER_SUPPLY_PROP_TERMINATION_CURRENT, &prop);
		if (rc < 0) {
			pr_err("sm could not set termi current!\n");
			return interval;
		}
	}
	last_term = term_curr;

	return interval;
}

#define BAT_WARM_TEMP				48

static int fg_check_recharge_status(struct sm_fg_chip *sm)
{
	int rc;
	union power_supply_propval prop = {0, };

	if (!sm->batt_psy) {
		sm->batt_psy = power_supply_get_by_name("battery");
		if (!sm->batt_psy) {
			return 0;
		}
	}
	if (!sm->chg_dis_votable)
		sm->chg_dis_votable = find_votable("CHG_DISABLE");

	rc = power_supply_get_property(sm->batt_psy,
			POWER_SUPPLY_PROP_HEALTH, &prop);
	sm->health = prop.intval;
	rc = power_supply_get_property(sm->batt_psy,
			POWER_SUPPLY_PROP_STATUS, &prop);
	sm->charge_status = prop.intval;

	if ((sm->batt_soc <= SM_RECHARGE_SOC) && (sm->charge_status == POWER_SUPPLY_STATUS_FULL) &&
			(sm->batt_temp < BAT_WARM_TEMP)) {
		sm->batt_sw_fc = false;
		prop.intval = true;
		vote(sm->chg_dis_votable, BMS_FC_VOTER, true, 0);
		msleep(200);
		vote(sm->chg_dis_votable, BMS_FC_VOTER, false, 0);
		rc = power_supply_get_property(sm->batt_psy,
				POWER_SUPPLY_PROP_FORCE_RECHARGE, &prop);
		if (rc < 0) {
			pr_err("sm could not set force recharging!\n");
			return rc;
		}
	}

	return 0;
}

//static unsigned int poll_interval = 60;
static void fg_monitor_workfunc(struct work_struct *work)
{
	struct sm_fg_chip *sm = container_of(work, struct sm_fg_chip,
								monitor_work.work);
	int interval;

	mutex_lock(&sm->data_lock);
	fg_init(sm->client);
	mutex_unlock(&sm->data_lock);

	fg_refresh_status(sm);
	interval = fg_check_full_status(sm);
	fg_check_recharge_status(sm);

	if (interval > 0) {
		//set_timer_slack(&sm->monitor_work.timer, poll_interval * HZ / 4);
		schedule_delayed_work(&sm->monitor_work, interval * HZ);
	}

}

void start_fg_monitor_work(struct power_supply *psy)
{
	struct sm_fg_chip *sm = power_supply_get_drvdata(psy);
	pr_err("start_fg_monitor_work");
	schedule_delayed_work(&sm->monitor_work, 0);
}
EXPORT_SYMBOL(start_fg_monitor_work);

void stop_fg_monitor_work(struct power_supply *psy)
{
	struct sm_fg_chip *sm = power_supply_get_drvdata(psy);
	pr_err("stop_fg_monitor_work");
	cancel_delayed_work(&sm->monitor_work);
}
EXPORT_SYMBOL(stop_fg_monitor_work);

#define COMMON_PARAM_MASK		0xFF00
#define COMMON_PARAM_SHIFT		8
#define BATTERY_PARAM_MASK		0x00FF
static bool fg_check_reg_init_need(struct i2c_client *client)
{
	struct sm_fg_chip *sm = i2c_get_clientdata(client); 
	int ret = 0;
	u16 data = 0;
	u16 param_ver = 0;
	
	ret = fg_read_word(sm, sm->regs[SM_FG_REG_FG_OP_STATUS], &data);
	if (ret < 0) {
			pr_err("Failed to read param_ctrl unlock, ret = %d\n", ret);
			return ret;
	} else {
		pr_info("FG_OP_STATUS = 0x%x\n", data);	

		ret = fg_read_word(sm, FG_PARAM_VERION, &param_ver);
		if (ret < 0) {
				pr_err("Failed to read FG_PARAM_VERION, ret = %d\n", ret);
				return ret;
		} 

		pr_info("param_ver = 0x%x, common_param_version = 0x%x, battery_param_version = 0x%x\n", param_ver, sm->common_param_version, sm->battery_param_version);

		if(((data & INIT_CHECK_MASK) == DISABLE_RE_INIT)
			&& (((param_ver & COMMON_PARAM_MASK) >> COMMON_PARAM_SHIFT) >= sm->common_param_version)
			&& ((param_ver & BATTERY_PARAM_MASK) >= sm->battery_param_version))
		{
			pr_info("%s: SM_FG_REG_FG_OP_STATUS : 0x%x , return FALSE NO init need\n", __func__, data);
			return 0;
		}
		else
		{
			pr_info("%s: SM_FG_REG_FG_OP_STATUS : 0x%x , return TRUE init need!!!!\n", __func__, data);
			return 1;
		}
	}
}

#define MINVAL(a, b) ((a <= b) ? a : b)
#define MAXVAL(a, b) ((a > b) ? a : b)
static int fg_calculate_iocv(struct sm_fg_chip *sm)
{
	bool only_lb=false, sign_i_offset=0; //valid_cb=false, 
	int roop_start=0, roop_max=0, i=0, cb_last_index = 0, cb_pre_last_index =0;
	int lb_v_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int lb_i_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int cb_v_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int cb_i_buffer[FG_INIT_B_LEN+1] = {0, 0, 0, 0, 0, 0, 0, 0};
	int i_offset_margin = 0x14, i_vset_margin = 0x67;
	int v_max=0, v_min=0, v_sum=0, lb_v_avg=0, cb_v_avg=0, lb_v_set=0, lb_i_set=0, i_offset=0;
	int i_max=0, i_min=0, i_sum=0, lb_i_avg=0, cb_i_avg=0, cb_v_set=0, cb_i_set=0;
	int lb_i_p_v_min=0, lb_i_n_v_max=0, cb_i_p_v_min=0, cb_i_n_v_max=0;

	u16 v_ret, i_ret = 0;
	int ret=0;

	u16 data = 0;

	ret = fg_read_word(sm, FG_REG_END_V_IDX, &data);
	if (ret < 0) {
			pr_err("Failed to read FG_REG_END_V_IDX, ret = %d\n", ret);
			return ret;
	} else {
		pr_info("iocv_status_read = addr : 0x%x , data : 0x%x\n", FG_REG_END_V_IDX, data);
	}

	if((data & 0x0010) == 0x0000)
	{
		only_lb = true;
	}

    roop_max = (data & 0x000F);
    if(roop_max > FG_INIT_B_LEN)
        roop_max = FG_INIT_B_LEN;

	roop_start = FG_REG_START_LB_V;
	for (i = roop_start; i < roop_start + roop_max; i++)
	{
		ret = fg_read_word(sm, i, &v_ret);
		if (ret < 0) {
			pr_err("Failed to read 0x%x, ret = %d\n",i, ret);
			return ret;
		}
		ret = fg_read_word(sm, i+0x20, &i_ret);
		if (ret < 0) {
			pr_err("Failed to read 0x%x, ret = %d\n",i, ret);
			return ret;
		}

		if((i_ret&0x4000) == 0x4000)
		{
			i_ret = -(i_ret&0x3FFF);
		}

		lb_v_buffer[i-roop_start] = v_ret;
		lb_i_buffer[i-roop_start] = i_ret;

		if (i == roop_start)
		{
			v_max = v_ret;
			v_min = v_ret;
			v_sum = v_ret;
			i_max = i_ret;
			i_min = i_ret;
			i_sum = i_ret;
		}
		else
		{
			if(v_ret > v_max)
				v_max = v_ret;
			else if(v_ret < v_min)
				v_min = v_ret;
			v_sum = v_sum + v_ret;

			if(i_ret > i_max)
				i_max = i_ret;
			else if(i_ret < i_min)
				i_min = i_ret;
			i_sum = i_sum + i_ret;
		}

		if(abs(i_ret) > i_vset_margin)
		{
			if(i_ret > 0)
			{
				if(lb_i_p_v_min == 0)
				{
					lb_i_p_v_min = v_ret;
				}
				else
				{
					if(v_ret < lb_i_p_v_min)
						lb_i_p_v_min = v_ret;
				}
			}
			else
			{
				if(lb_i_n_v_max == 0)
				{
					lb_i_n_v_max = v_ret;
				}
				else
				{
					if(v_ret > lb_i_n_v_max)
						lb_i_n_v_max = v_ret;
				}
			}
		}
	}
	v_sum = v_sum - v_max - v_min;
	i_sum = i_sum - i_max - i_min;

	lb_v_avg = v_sum / (roop_max-2);
	lb_i_avg = i_sum / (roop_max-2);

	if(abs(lb_i_buffer[roop_max-1]) < i_vset_margin)
	{
		if(abs(lb_i_buffer[roop_max-2]) < i_vset_margin)
		{
			lb_v_set = MAXVAL(lb_v_buffer[roop_max-2], lb_v_buffer[roop_max-1]);
			if(abs(lb_i_buffer[roop_max-3]) < i_vset_margin)
			{
				lb_v_set = MAXVAL(lb_v_buffer[roop_max-3], lb_v_set);
			}
		}
		else
		{
			lb_v_set = lb_v_buffer[roop_max-1];
		}
	}
	else
	{
		lb_v_set = lb_v_avg;
	}

	if(lb_i_n_v_max > 0)
	{
		lb_v_set = MAXVAL(lb_i_n_v_max, lb_v_set);
	}

	if(roop_max > 3)
	{
		lb_i_set = (lb_i_buffer[2] + lb_i_buffer[3]) / 2;
	}

	if((abs(lb_i_buffer[roop_max-1]) < i_offset_margin) && (abs(lb_i_set) < i_offset_margin))
	{
		lb_i_set = MAXVAL(lb_i_buffer[roop_max-1], lb_i_set);
	}
	else if(abs(lb_i_buffer[roop_max-1]) < i_offset_margin)
	{
		lb_i_set = lb_i_buffer[roop_max-1];
	}
	else if(abs(lb_i_set) < i_offset_margin)
	{
		//lb_i_set = lb_i_set;
	}
	else
	{
		lb_i_set = 0;
	}

	i_offset = lb_i_set;

	i_offset = i_offset + 4;

	if(i_offset <= 0)
	{
		sign_i_offset = 1;
#ifdef IGNORE_N_I_OFFSET
		i_offset = 0;
#else
		i_offset = -i_offset;
#endif
	}

	i_offset = i_offset>>1;

	if(sign_i_offset == 0)
	{
		i_offset = i_offset|0x0080;
	}
    i_offset = i_offset | i_offset<<8;

	if(!only_lb)
	{
		roop_start = FG_REG_START_CB_V;
		roop_max = 6;
		for (i = roop_start; i < roop_start + roop_max; i++)
		{
			ret = fg_read_word(sm, i, &v_ret);
			if (ret < 0) {
				pr_err("Failed to read 0x%x, ret = %d\n",i, ret);
				return ret;
			}
			ret = fg_read_word(sm, i+0x20, &i_ret);
			if (ret < 0) {
				pr_err("Failed to read 0x%x, ret = %d\n",i, ret);
				return ret;
			}

			if((i_ret&0x4000) == 0x4000)
			{
				i_ret = -(i_ret&0x3FFF);
			}

			cb_v_buffer[i-roop_start] = v_ret;
			cb_i_buffer[i-roop_start] = i_ret;

			if (i == roop_start)
			{
				v_max = v_ret;
				v_min = v_ret;
				v_sum = v_ret;
				i_max = i_ret;
				i_min = i_ret;
				i_sum = i_ret;
			}
			else
			{
				if(v_ret > v_max)
					v_max = v_ret;
				else if(v_ret < v_min)
					v_min = v_ret;
				v_sum = v_sum + v_ret;

				if(i_ret > i_max)
					i_max = i_ret;
				else if(i_ret < i_min)
					i_min = i_ret;
				i_sum = i_sum + i_ret;
			}

			if(abs(i_ret) > i_vset_margin)
			{
				if(i_ret > 0)
				{
					if(cb_i_p_v_min == 0)
					{
						cb_i_p_v_min = v_ret;
					}
					else
					{
						if(v_ret < cb_i_p_v_min)
							cb_i_p_v_min = v_ret;
					}
				}
				else
				{
					if(cb_i_n_v_max == 0)
					{
						cb_i_n_v_max = v_ret;
					}
					else
					{
						if(v_ret > cb_i_n_v_max)
							cb_i_n_v_max = v_ret;
					}
				}
			}
		}
		v_sum = v_sum - v_max - v_min;
		i_sum = i_sum - i_max - i_min;

		cb_v_avg = v_sum / (roop_max-2);
		cb_i_avg = i_sum / (roop_max-2);

		cb_last_index = (data & 0x000F)-7; //-6-1
		if(cb_last_index < 0)
		{
			cb_last_index = 5;
		}

		for (i = roop_max; i > 0; i--)
		{
			if(abs(cb_i_buffer[cb_last_index]) < i_vset_margin)
			{
				cb_v_set = cb_v_buffer[cb_last_index];
				if(abs(cb_i_buffer[cb_last_index]) < i_offset_margin)
				{
					cb_i_set = cb_i_buffer[cb_last_index];
				}

				cb_pre_last_index = cb_last_index - 1;
				if(cb_pre_last_index < 0)
				{
					cb_pre_last_index = 5;
				}

				if(abs(cb_i_buffer[cb_pre_last_index]) < i_vset_margin)
				{
					cb_v_set = MAXVAL(cb_v_buffer[cb_pre_last_index], cb_v_set);
					if(abs(cb_i_buffer[cb_pre_last_index]) < i_offset_margin)
					{
						cb_i_set = MAXVAL(cb_i_buffer[cb_pre_last_index], cb_i_set);
					}
				}
			}
			else
			{
				cb_last_index--;
				if(cb_last_index < 0)
				{
					cb_last_index = 5;
				}
			}
		}

		if(cb_v_set == 0)
		{
			cb_v_set = cb_v_avg;
			if(cb_i_set == 0)
			{
				cb_i_set = cb_i_avg;
			}
		}

		if(cb_i_n_v_max > 0)
		{
			cb_v_set = MAXVAL(cb_i_n_v_max, cb_v_set);
		}

		if(abs(cb_i_set) < i_offset_margin)
		{
			if(cb_i_set > lb_i_set)
			{
				i_offset = cb_i_set;
				i_offset = i_offset + 4;

				if(i_offset <= 0)
				{
					sign_i_offset = 1;
#ifdef IGNORE_N_I_OFFSET
					i_offset = 0;
#else
					i_offset = -i_offset;
#endif
				}

				i_offset = i_offset>>1;

				if(sign_i_offset == 0)
				{
					i_offset = i_offset|0x0080;
				}
                i_offset = i_offset | i_offset<<8;

			}
		}
	}

	if((abs(cb_i_set) > i_vset_margin) || only_lb)
	{
		ret = MAXVAL(lb_v_set, cb_i_n_v_max);
	}
	else
	{
		ret = cb_v_set;
	}

    if(ret > sm->battery_table[BATTERY_TABLE0][FG_TABLE_LEN-1])
    {
        ret = sm->battery_table[BATTERY_TABLE0][FG_TABLE_LEN-1];
    }
    else if(ret < sm->battery_table[BATTERY_TABLE0][0])
    {
        ret = sm->battery_table[BATTERY_TABLE0][0] + 0x10;
    }

	return ret;
}

static bool fg_reg_init(struct i2c_client *client)
{
	struct sm_fg_chip *sm = i2c_get_clientdata(client);
	int i, j, value, ret, cnt = 0;
	uint8_t table_reg;
	u16 data, data_int_mask = 0;

	pr_info("sm5602_fg_reg_init START!!\n");

	/* Init mark */
	if (sm->fg_irq_set == -EINVAL) {
		pr_err("sm->fg_irq_set is invalid");
	} else {
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_INT_MASK], &data_int_mask);
		if (ret < 0){
			pr_err("Failed to read INT_MASK, ret = %d\n", ret);	
			return ret;	
		}	
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_INT_MASK], 0x4000 | (data_int_mask | sm->fg_irq_set));
	    if (ret < 0) {
			pr_err("Failed to write 0x4000 | INIT_MASK, ret = %d\n", ret);
			return ret;
		}
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_INT_MASK], 0x07FF & (data_int_mask | sm->fg_irq_set));
	    if (ret < 0) {
			pr_err("Failed to write INIT_MASK, ret = %d\n", ret);
			return ret;
		}
	}

	/* Low SOC1  */
	if (sm->low_soc1 == -EINVAL) {
		pr_err("sm->low_soc1 is invalid");
	} else {
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_SOC_L_ALARM], &data);
		if (ret < 0){
			pr_err("Failed to read SOC_L_ALARM (LOW_SOC1), ret = %d\n", ret);	
			return ret;	
		}		
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_SOC_L_ALARM], ((data & 0xFFE0) | sm->low_soc1));
	    if (ret < 0) {
			pr_err("Failed to write SOC_L_ALARM (LOW_SOC1), ret = %d\n", ret);
			return ret;
		}
	}

	/* Low SOC2  */	
	if (sm->low_soc2 == -EINVAL) {
		pr_err("sm->low_soc2 is invalid");
	} else {
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_SOC_L_ALARM], &data);
		if (ret < 0){
			pr_err("Failed to read SOC_L_ALARM (LOW_SOC2), ret = %d\n", ret);	
			return ret;	
		}	
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_SOC_L_ALARM],((data & 0xE0FF) | (sm->low_soc2 << 8)));
	    if (ret < 0) {
			pr_err("Failed to write LOW_SOC2, ret = %d\n", ret);
			return ret;
		}
	}

	/* V L ALARM  */
	if (sm->v_l_alarm == -EINVAL) {
		pr_err("sm->v_l_alarm is invalid");
	} else {
		if (sm->v_l_alarm >= 2000 && sm->v_l_alarm < 3000)
			data = (0xFEFF & (sm->v_l_alarm/10 * 256));
		else if (sm->v_l_alarm >= 3000 && sm->v_l_alarm < 4000)
			data = (0x0100 | (sm->v_l_alarm/10 * 256));
		else {
			ret = -EINVAL;
			pr_err("Failed to calculate V_L_ALARM, ret = %d\n", ret);
			return ret;
		}
		
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_V_L_ALARM], data);
	    if (ret < 0) {
			pr_err("Failed to write V_L_ALARM, ret = %d\n", ret);
			return ret;
		}
	}

	/* V H ALARM  */
	if (sm->v_h_alarm == -EINVAL) {
		pr_err("sm->v_h_alarm is invalid");
	} else {
		if (sm->v_h_alarm >= 3000 && sm->v_h_alarm < 4000)
			data = (0xFEFF & (sm->v_h_alarm/10 * 256));
		else if (sm->v_h_alarm >= 4000 && sm->v_h_alarm < 5000)
			data = (0x0100 | (sm->v_h_alarm/10 * 256));
		else {
			ret = -EINVAL;
			pr_err("Failed to calculate V_H_ALARM, ret = %d\n", ret);
			return ret;
		}
		
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_V_H_ALARM], data);
		if (ret < 0) {
			pr_err("Failed to write V_H_ALARM, ret = %d\n", ret);
			return ret;
		}
	}

	/* T IN H/L ALARM  */
	if (sm->t_h_alarm_in == -EINVAL 
		|| sm->t_l_alarm_in == -EINVAL) {
		pr_err("sm->t_h_alarm_in || sm->t_l_alarm_in is invalid");
	} else {
		data = 0; //clear value
		//T IN H ALARM
		if (sm->t_h_alarm_in < 0) {
			data |= 0x8000;
			data |= ((((-1)*sm->t_h_alarm_in) & 0x7F) << 8);
		} else {
			data |= (((sm->t_h_alarm_in) & 0x7F) << 8);
		}
		//T IN L ALARM
		if (sm->t_l_alarm_in < 0) {
			data |= 0x0080;
			data |= ((((-1)*sm->t_l_alarm_in) & 0x7F));
		} else {
			data |= (((sm->t_l_alarm_in) & 0x7F));
		}
		
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_T_IN_H_ALARM], data);
		if (ret < 0) {
			pr_err("Failed to write SM_FG_REG_T_IN_H_ALARM, ret = %d\n", ret);
			return ret;
		}
	}
	
	do {		
		ret = fg_write_word(sm, sm->regs[SM_FG_REG_PARAM_CTRL], (FG_PARAM_UNLOCK_CODE | ((sm->battery_table_num & 0x0003) << 6) | (FG_TABLE_LEN-1)));
		if (ret < 0) {
			pr_err("Failed to write param_ctrl unlock, ret = %d\n", ret);
			return ret;
		} else {
			pr_info("Param Unlock\n");
		}
		//msleep(3);
		msleep(60);
		ret = fg_read_word(sm, sm->regs[SM_FG_REG_FG_OP_STATUS], &data);
		if (ret < 0){
			pr_err("Failed to read FG_OP_STATUS, ret = %d\n", ret);	
		} else {
			pr_info(" FG_OP_STATUS = 0x%x\n", data);
		}
		cnt++;

	} while(((data & 0x03)!=0x03) && cnt <= 3);
	
	/* VIT_PERIOD write */
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_VIT_PERIOD], sm->vit_period);
	if (ret < 0) {
		pr_err("Failed to write VIT PERIOD, ret = %d\n", ret);
		return ret;
	} else {
			pr_info("Write VIT_PERIOD = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_VIT_PERIOD], sm->vit_period);
	}

	/* Aging ctrl write */
	ret = fg_write_word(sm, FG_REG_AGING_CTRL, sm->aging_ctrl);
	if (ret < 0) {
		pr_err("Failed to write FG_REG_AGING_CTRL, ret = %d\n", ret);
		return ret;
	} else {
			pr_info("Write FG_REG_AGING_CTRL = 0x%x : 0x%x\n", FG_REG_AGING_CTRL, sm->aging_ctrl);
	}

	/* SOC Cycle ctrl write */
	ret = fg_write_word(sm, FG_REG_SOC_CYCLE_CFG, sm->cycle_cfg);
	if (ret < 0) {
		pr_err("Failed to write FG_REG_SOC_CYCLE_CFG, ret = %d\n", ret);
		return ret;
	} else {
			pr_info("Write FG_REG_SOC_CYCLE_CFG = 0x%x : 0x%x\n", FG_REG_SOC_CYCLE_CFG, sm->cycle_cfg);
	}

	/*RSNS write */
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_RSNS_SEL], sm->batt_rsns);
	if (ret < 0) {
		pr_err("Failed to write SM_FG_REG_RSNS_SEL, ret = %d\n", ret);
		return ret;
	} else {
			pr_info("Write SM_FG_REG_RSNS_SEL = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_RSNS_SEL], sm->batt_rsns);
	}
	
	/* Battery_Table write */
	for (i = BATTERY_TABLE0; i < BATTERY_TABLE2; i++) {
		table_reg = 0xA0 + (i*FG_TABLE_LEN);
		for (j = 0; j < FG_TABLE_LEN; j++) {
			ret = fg_write_word(sm, (table_reg + j), sm->battery_table[i][j]);
			if (ret < 0) {
				pr_err("Failed to write Battery Table, ret = %d\n", ret);
				return ret;
			} 
			/*else {
				pr_info("TABLE write OK [%d][%d] = 0x%x : 0x%x\n",
					i, j, (table_reg + j), sm->battery_table[i][j]);
			}*/
		}
	}	

	for(j=0; j < FG_ADD_TABLE_LEN; j++)
	{
		table_reg = 0xD0 + j;
		ret = fg_write_word(sm, table_reg, sm->battery_table[i][j]);
		if (ret < 0) {
			pr_err("Failed to write Battery Table, ret = %d\n", ret);
			return ret;
		}/* else {
			pr_info("TABLE write OK [%d][%d] = 0x%x : 0x%x\n",
				i, j, table_reg, sm->battery_table[i][j]);
		}*/
	}

	/*  RS write */
	ret = fg_write_word(sm, FG_REG_RS, sm->rs);
	if (ret < 0) {
		pr_err("Failed to write RS, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("RS = 0x%x : 0x%x\n",FG_REG_RS, sm->rs);
	}

	/*  alpha write */
	ret = fg_write_word(sm, FG_REG_ALPHA, sm->alpha);
	if (ret < 0) {
		pr_err("Failed to write FG_REG_ALPHA, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("ALPHA = 0x%x : 0x%x\n",FG_REG_ALPHA, sm->alpha);
	}

	/*  beta write */
	ret = fg_write_word(sm, FG_REG_BETA, sm->beta);
	if (ret < 0) {
		pr_err("Failed to write FG_REG_BETA, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("BETA = 0x%x : 0x%x\n",FG_REG_BETA, sm->beta);
	}

	/*  RS write */
	ret = fg_write_word(sm, FG_REG_RS_0, sm->rs_value[0]);
	if (ret < 0) {
		pr_err("Failed to write RS_0, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("RS = 0x%x : 0x%x\n",FG_REG_RS_0, sm->rs_value[0]);
	}

	ret = fg_write_word(sm, FG_REG_RS_1, sm->rs_value[1]);
	if (ret < 0) {
		pr_err("Failed to write RS_1, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("RS_1 = 0x%x : 0x%x\n", FG_REG_RS_1, sm->rs_value[1]);
	}
		
	ret = fg_write_word(sm, FG_REG_RS_2, sm->rs_value[2]);
	if (ret < 0) {
		pr_err("Failed to write RS_2, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("RS_2 = 0x%x : 0x%x\n", FG_REG_RS_2, sm->rs_value[2]);
	}

	ret = fg_write_word(sm, FG_REG_RS_3, sm->rs_value[3]);
	if (ret < 0) {
		pr_err("Failed to write RS_3, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("RS_3 = 0x%x : 0x%x\n", FG_REG_RS_3, sm->rs_value[3]);
	}

	ret = fg_write_word(sm, sm->regs[SM_FG_REG_CURRENT_RATE], sm->mix_value);
	if (ret < 0) {
		pr_err("Failed to write CURRENT_RATE, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("CURRENT_RATE = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_CURRENT_RATE], sm->mix_value);
	}

	pr_info("RS_0 = 0x%x, RS_1 = 0x%x, RS_2 = 0x%x, RS_3 = 0x%x, CURRENT_RATE = 0x%x\n",
		sm->rs_value[0], sm->rs_value[1], sm->rs_value[2], sm->rs_value[3], sm->mix_value);

	/* VOLT_CAL write*/
	ret = fg_write_word(sm, FG_REG_VOLT_CAL, sm->volt_cal);
	if (ret < 0) {
		pr_err("Failed to write FG_REG_VOLT_CAL, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("FG_REG_VOLT_CAL = 0x%x : 0x%x\n", FG_REG_VOLT_CAL, sm->volt_cal);
	}		

	/* CAL write*/	
	ret = fg_write_word(sm, FG_REG_CURR_IN_OFFSET, sm->curr_offset);
	if (ret < 0) {
		pr_err("Failed to write CURR_IN_OFFSET, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("CURR_IN_OFFSET = 0x%x : 0x%x\n", FG_REG_CURR_IN_OFFSET, sm->curr_offset);
	}		
	ret = fg_write_word(sm, FG_REG_CURR_IN_SLOPE, sm->curr_slope);
	if (ret < 0) {
		pr_err("Failed to write CURR_IN_SLOPE, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("CURR_IN_SLOPE = 0x%x : 0x%x\n", FG_REG_CURR_IN_SLOPE, sm->curr_slope);
	}

	/* BAT CAP write */
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_BAT_CAP], sm->cap);
	if (ret < 0) {
		pr_err("Failed to write BAT_CAP, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("BAT_CAP = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_BAT_CAP], sm->cap);
	}

	/* MISC write */
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_MISC], sm->misc);
	if (ret < 0) {
		pr_err("Failed to write REG_MISC, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("REG_MISC 0x%x : 0x%x\n", sm->regs[SM_FG_REG_MISC], sm->misc);
	}

	/* TOPOFF SOC */
	ret = fg_write_word(sm, sm->regs[SM_FG_REG_TOPOFFSOC], sm->topoff_soc);	
	if (ret < 0) {
		pr_err("Failed to write SM_FG_REG_TOPOFFSOC, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("SM_REG_TOPOFFSOC 0x%x : 0x%x\n", sm->regs[SM_FG_REG_TOPOFFSOC], sm->topoff_soc);
	}
	
	/*INIT_last -  control register set*/
	ret = fg_read_word(sm, sm->regs[SM_FG_REG_CNTL], &data);
	if (ret < 0) {
			pr_err("Failed to read CNTL, ret = %d\n", ret);
			return ret;
	}
	
	if (sm->en_temp_in)
		data |= ENABLE_EN_TEMP_IN;
	if (sm->en_temp_ex)
		data |= ENABLE_EN_TEMP_EX;
	if (sm->en_batt_det)
		data |= ENABLE_EN_BATT_DET;
	if (sm->iocv_man_mode)
		data |= ENABLE_IOCV_MAN_MODE;

	ret = fg_write_word(sm, sm->regs[SM_FG_REG_CNTL], data);
	if (ret < 0) {
		pr_err("Failed to write CNTL, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("CNTL = 0x%x : 0x%x\n", sm->regs[SM_FG_REG_CNTL], data);
	}

	/* Parameter Version [COMMON(0~255) | BATTERY(0~255)] */
	ret = fg_write_word(sm, FG_PARAM_VERION, ((sm->common_param_version << 8) | sm->battery_param_version));	
	if (ret < 0) {
		pr_err("Failed to write FG_PARAM_VERION, ret = %d\n", ret);
		return ret;
	}

	/* T EX L ALARM  */
	if (sm->t_l_alarm_ex == -EINVAL) {
		pr_err("sm->t_l_alarm_ex is invalid");
	} else {
		data = (sm->t_l_alarm_ex) >> 1; //NTC Value/2

		ret = fg_write_word(sm, FG_REG_SWADDR, 0x6A);
		if (ret < 0) {
			pr_err("Failed to write FG_REG_SWADDR, ret = %d\n", ret);
			return ret;
		}
		ret = fg_write_word(sm, FG_REG_SWDATA, data);
		if (ret < 0) {
			pr_err("Failed to write FG_REG_SWADDR, ret = %d\n", ret);
			return ret;
		}	

		pr_info("write to T_EX_H_ALARM = 0x%x\n", data);
	}

	/* T EX H ALARM  */
	if (sm->t_h_alarm_ex == -EINVAL) {
		pr_err("sm->t_h_alarm_ex is invalid");
	} else {
		data = (sm->t_h_alarm_ex) >> 1; //NTC Value/2

		ret = fg_write_word(sm, FG_REG_SWADDR, 0x6B);
		if (ret < 0) {
			pr_err("Failed to write FG_REG_SWADDR, ret = %d\n", ret);
			return ret;
		}
		ret = fg_write_word(sm, FG_REG_SWDATA, data);
		if (ret < 0) {
			pr_err("Failed to write FG_REG_SWADDR, ret = %d\n", ret);
			return ret;
		}	

		pr_info("write to T_EX_L_ALARM = 0x%x\n", data);
	}

	if (sm->iocv_man_mode) {
		value = fg_calculate_iocv(sm);

	    msleep(10);
		ret = fg_write_word(sm, FG_REG_SWADDR, 0x75);
		if (ret < 0) {
			pr_err("Failed to write FG_REG_SWADDR, ret = %d\n", ret);
			return ret;
		}
		ret = fg_write_word(sm, FG_REG_SWDATA, value);
		if (ret < 0) {
			pr_err("Failed to write FG_REG_SWADDR, ret = %d\n", ret);
			return ret;
		}		
		pr_info("IOCV_MAN : 0x%x\n", value);
	}
	
	msleep(20);

	ret = fg_write_word(sm, sm->regs[SM_FG_REG_PARAM_CTRL], ((FG_PARAM_LOCK_CODE | (sm->battery_table_num & 0x0003) << 6) | (FG_TABLE_LEN-1)));
	if (ret < 0) {
		pr_err("Failed to write param_ctrl lock, ret = %d\n", ret);
		return ret;
	} else {
		pr_info("Param Lock\n");
	}

	msleep(160);

	return 1;
}

static unsigned int fg_get_device_id(struct i2c_client *client)
{
	struct sm_fg_chip *sm = i2c_get_clientdata(client);
	int ret;
	u16 data;

	ret = fg_read_word(sm, sm->regs[SM_FG_REG_DEVICE_ID], &data);
	if (ret < 0) {
		pr_err("Failed to read DEVICE_ID, ret = %d\n", ret);
		return ret;
	}
	
	pr_info("revision_id = 0x%x\n",(data & 0x000f));
	pr_info("device_id = 0x%x\n",(data & 0x00f0)>>4);

	return data;
}

static bool fg_check_device_id(struct i2c_client *client)
{
	bool ret = false;
	u16 vendorId;
	
	vendorId = fg_get_device_id(client);
	
	if(vendorId >= 0)
	{
		if(0x0001 == ((vendorId & 0x00f0)>>4))
		{
			ret = true;
		}
	}

	return ret;
	
}

static bool fg_init(struct i2c_client *client)
{
	int ret;
	struct sm_fg_chip *sm = i2c_get_clientdata(client);

	/*sm5602 i2c read check*/
	ret = fg_get_device_id(client);
	if (ret < 0) {
		pr_err("%s: fail to do i2c read(%d)\n", __func__, ret);
		return false;
	}

	if (fg_check_reg_init_need(client)) {
		ret = fg_reset(sm);
		if (ret < 0) {
			pr_err("%s: fail to do reset(%d)\n", __func__, ret);
			return false;
		}
		fg_reg_init(client);
	}

	//sm->is_charging = (sm->batt_current > 9) ? true : false;
	//pr_err("is_charging = %dd\n",sm->is_charging);

	return true;
}

#define PROPERTY_NAME_SIZE 128
static int fg_common_parse_dt(struct sm_fg_chip *sm)
{
	struct device *dev = &sm->client->dev;
	struct device_node *np = dev->of_node;
	int rc,len;
	const u32 *p;
    
	BUG_ON(dev == 0);
	BUG_ON(np == 0);
#if (FG_REMOVE_IRQ == 0)
	sm->gpio_int = of_get_named_gpio(np, "qcom,irq-gpio", 0);
	pr_info("gpio_int=%d\n", sm->gpio_int);

	if (!gpio_is_valid(sm->gpio_int)) {
		pr_info("gpio_int is not valid\n");
		sm->gpio_int = -EINVAL;
	}
#endif
    /* EN TEMP EX/IN */
    if (of_property_read_bool(np, "sm,en_temp_ex"))
        sm->en_temp_ex = true;
	else
		sm->en_temp_ex = 0;
    pr_info("Temperature EX enabled = %d\n", sm->en_temp_ex);

    if (of_property_read_bool(np, "sm,en_temp_in"))
        sm->en_temp_in = true;
	else
		sm->en_temp_in = 0;
    pr_info("Temperature IN enabled = %d\n", sm->en_temp_in);

	/* EN BATT DET  */
	if (of_property_read_bool(np, "sm,en_batt_det"))
		sm->en_batt_det = true;
	else
		sm->en_batt_det = 0;
	pr_info("Batt Det enabled = %d\n", sm->en_batt_det);

    /* MISC */
    rc = of_property_read_u32(np, "sm,misc",
                        &sm->misc);
    if (rc < 0)
        sm->misc = 0x0800;

	/* IOCV MAN MODE */
	if (of_property_read_bool(np, "sm,iocv_man_mode"))
		sm->iocv_man_mode = true;
	else
		sm->iocv_man_mode = 0;
	pr_info("IOCV_MAN_MODE = %d\n", sm->iocv_man_mode);

    /* Aging */
    rc = of_property_read_u32(np, "sm,aging_ctrl",
                        &sm->aging_ctrl);
    if (rc < 0)
        sm->aging_ctrl = -EINVAL;

	/*decimal rate*/
	len = 0;
	p = of_get_property(np, "sm,soc_decimal_rate", &len);
	if (p) {
		sm->dec_rate_seq = kzalloc(len,GFP_KERNEL);
		sm->dec_rate_len = len / sizeof(*sm->dec_rate_seq);

		rc = of_property_read_u32_array(np, "sm,soc_decimal_rate",sm->dec_rate_seq,sm->dec_rate_len);
		if (rc) {
			pr_err("%s:failed to read dec_rate data: %d\n", __func__,rc);
			kfree(sm->dec_rate_seq);
		}
	}else {
		pr_err("%s: there is no decimal data\n", __func__);
	}

    /* SOC Cycle cfg */
    rc = of_property_read_u32(np, "sm,cycle_cfg",
                        &sm->cycle_cfg);
    if (rc < 0)
        sm->cycle_cfg = -EINVAL;

    /* RSNS */
    rc = of_property_read_u32(np, "sm,rsns",
                        &sm->batt_rsns);
    if (rc < 0)
        sm->batt_rsns = -EINVAL;

    /* IRQ Mask */
    rc = of_property_read_u32(np, "sm,fg_irq_set",
                        &sm->fg_irq_set);
    if (rc < 0)
        sm->fg_irq_set = -EINVAL;

    /* LOW SOC1/2 */
    rc = of_property_read_u32(np, "sm,low_soc1", 
    					&sm->low_soc1);
	if (rc < 0)
        sm->low_soc1 = -EINVAL;
    pr_info("low_soc1 = %d\n", sm->low_soc1);

    rc = of_property_read_u32(np, "sm,low_soc2", 
					&sm->low_soc2);
	if (rc < 0)
        sm->low_soc2 = -EINVAL;	
    pr_info("low_soc2 = %d\n", sm->low_soc2);

    /* V_L/H_ALARM */
    rc = of_property_read_u32(np, "sm,v_l_alarm", 
    					&sm->v_l_alarm);
	if (rc < 0)
        sm->v_l_alarm = -EINVAL;		
    pr_info("v_l_alarm = %d\n", sm->v_l_alarm);

    rc = of_property_read_u32(np, "sm,v_h_alarm", 
					&sm->v_h_alarm);
	if (rc < 0)
        sm->v_h_alarm = -EINVAL;	
    pr_info("v_h_alarm = %d\n", sm->v_h_alarm);

    /* T_IN_H/L_ALARM */
    rc = of_property_read_u32(np, "sm,t_l_alarm_in", 
    					&sm->t_l_alarm_in);
	if (rc < 0)
        sm->t_l_alarm_in = -EINVAL;		
    pr_info("t_l_alarm_in = %d\n", sm->t_l_alarm_in);

    rc = of_property_read_u32(np, "sm,t_h_alarm_in", 
					&sm->t_h_alarm_in);
	if (rc < 0)
        sm->t_h_alarm_in = -EINVAL;	
    pr_info("t_h_alarm_in = %d\n", sm->t_h_alarm_in);

    /* T_EX_H/L_ALARM */
    rc = of_property_read_u32(np, "sm,t_l_alarm_ex", 
    					&sm->t_l_alarm_ex);
	if (rc < 0)
        sm->t_l_alarm_ex = -EINVAL;		
    pr_info("t_l_alarm_ex = %d\n", sm->t_l_alarm_ex);

    rc = of_property_read_u32(np, "sm,t_h_alarm_ex", 
					&sm->t_h_alarm_ex);
	if (rc < 0)
        sm->t_h_alarm_ex = -EINVAL;	
    pr_info("t_h_alarm_ex = %d\n", sm->t_h_alarm_ex);

    /* Battery Table Number */
    rc = of_property_read_u32(np, "sm,battery_table_num",
                        &sm->battery_table_num);
    if (rc < 0)
        sm->battery_table_num = -EINVAL;

    /* Paramater Number */
    rc = of_property_read_u32(np, "sm,param_version",
                        &sm->common_param_version);
    if (rc < 0)
        sm->common_param_version = -EINVAL;

	/* Shutdown feature */
	if (of_property_read_bool(np, "sm,shutdown-delay-enable"))
		sm->shutdown_delay_enable = true;
	else
		sm->shutdown_delay_enable = 0;

	return 0;
}

static int fg_battery_parse_dt(struct sm_fg_chip *sm)
{
	struct device *dev = &sm->client->dev;
	struct device_node *np = dev->of_node;
	char prop_name[PROPERTY_NAME_SIZE];
	int battery_id = -1;
	int battery_temp_table[FG_TEMP_TABLE_CNT_MAX];
	int table[FG_TABLE_LEN];
	int rs_value[4];
	int topoff_soc[3];
	int temp_offset[6];
	int temp_cal[10];
	int ext_temp_cal[10];
	int battery_type[3];
	int set_temp_poff[4];
	int ret;
	int i, j;
    
	BUG_ON(dev == 0);
	BUG_ON(np == 0);

	/* battery_params node*/
	np = of_find_node_by_name(of_node_get(np), "battery_params");
	if (np == NULL) {
		pr_info("Cannot find child node \"battery_params\"\n");
		return -EINVAL;
	}

	/* battery_id*/
	if (of_property_read_u32(np, "battery,id", &battery_id) < 0)
		pr_err("not battery,id property\n");
	if (battery_id == -1)
		battery_id = get_battery_id();
	pr_info("battery id = %d\n", battery_id);

	/*  battery_table*/
	for (i = BATTERY_TABLE0; i < BATTERY_TABLE2; i++) {
		snprintf(prop_name, PROPERTY_NAME_SIZE,
			 "battery%d,%s%d", battery_id, "battery_table", i);

		ret = of_property_read_u32_array(np, prop_name, table, FG_TABLE_LEN);
		if (ret < 0)
			pr_info("Can get prop %s (%d)\n", prop_name, ret);
		for (j = 0; j < FG_TABLE_LEN; j++) {
			sm->battery_table[i][j] = table[j];
			//pr_info("%s = <table[%d][%d] 0x%x>\n",
			//	prop_name, i, j, table[j]);
		}
	}

	i = BATTERY_TABLE2;
	snprintf(prop_name, PROPERTY_NAME_SIZE,
		 "battery%d,%s%d", battery_id, "battery_table", i);
	ret = of_property_read_u32_array(np, prop_name, table, FG_ADD_TABLE_LEN);
	if (ret < 0)
		pr_info("Can get prop %s (%d)\n", prop_name, ret);
	else {
		for(j=0; j < FG_ADD_TABLE_LEN; j++)
		{
			sm->battery_table[i][j] = table[j];
			//pr_info("%s = <table[%d][%d] 0x%x>\n",
			//	prop_name, i, j, table[j]);
		
		}
	}
	
    /* rs */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "rs");
	ret = of_property_read_u32_array(np, prop_name, &sm->rs, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->rs);

    /* alpha */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "alpha");
	ret = of_property_read_u32_array(np, prop_name, &sm->alpha, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->alpha);

    /* beta */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "beta");
	ret = of_property_read_u32_array(np, prop_name, &sm->beta, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->beta);

	/* rs_value*/
	for (i = 0; i < 4; i++) {
		snprintf(prop_name,
			PROPERTY_NAME_SIZE, "battery%d,%s",
			battery_id, "rs_value");
		ret = of_property_read_u32_array(np, prop_name, rs_value, 4);
		if (ret < 0)
			pr_err("Can get prop %s (%d)\n", prop_name, ret);
		sm->rs_value[i] = rs_value[i];
	}
	pr_info("%s = <0x%x 0x%x 0x%x 0x%x>\n",
		prop_name, rs_value[0], rs_value[1], rs_value[2], rs_value[3]);

	/* vit_period*/
	snprintf(prop_name,
		PROPERTY_NAME_SIZE, "battery%d,%s",
		battery_id, "vit_period");
	ret = of_property_read_u32_array(np,
		prop_name, &sm->vit_period, 1);
	if (ret < 0)
		pr_info("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->vit_period);

    /* battery_type*/
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "battery_type");
    ret = of_property_read_u32_array(np, prop_name, battery_type, 3);
    if (ret < 0)
        pr_err("Can get prop %s (%d)\n", prop_name, ret);
    sm->batt_v_max = battery_type[0];
    sm->min_cap = battery_type[1];
    sm->cap = battery_type[2];

    pr_info("%s = <%d %d %d>\n", prop_name,
        sm->batt_v_max, sm->min_cap, sm->cap);

	/* tem poff level */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "tem_poff");
	ret = of_property_read_u32_array(np, prop_name, set_temp_poff, 2);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	sm->n_tem_poff = set_temp_poff[0];
	sm->n_tem_poff_offset = set_temp_poff[1];

	pr_info("%s = <%d, %d>\n",
		prop_name,
		sm->n_tem_poff, sm->n_tem_poff_offset);

    /* max-voltage -mv*/
    snprintf(prop_name,
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "max_voltage_uv");    
	ret = of_property_read_u32(np, prop_name,
				        &sm->batt_max_voltage_uv);
	if (ret < 0)
	    pr_err("couldn't find battery max voltage\n");        

    // TOPOFF SOC
    snprintf(prop_name, 
    	PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "topoff_soc");
    ret = of_property_read_u32_array(np, prop_name, topoff_soc, 2);
    if (ret < 0)
        pr_err("Can get prop %s (%d)\n", prop_name, ret);
    sm->topoff_soc = topoff_soc[0];
    sm->top_off = topoff_soc[1];

    pr_info("%s = <%d %d>\n", prop_name,
        sm->topoff_soc, sm->top_off);

    // Mix
    snprintf(prop_name, 
    	PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "mix_value");
    ret = of_property_read_u32_array(np, prop_name, &sm->mix_value, 1);
    if (ret < 0)
        pr_err("Can get prop %s (%d)\n", prop_name, ret);

    pr_info("%s = <%d>\n", prop_name,
        sm->mix_value);

    /* VOLT CAL */
    snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "volt_cal");
    ret = of_property_read_u32_array(np, prop_name, &sm->volt_cal, 1);
    if (ret < 0)
        pr_err("Can get prop %s (%d)\n", prop_name, ret);
    pr_info("%s = <0x%x>\n", prop_name, sm->volt_cal);

	/* CURR OFFSET */
	snprintf(prop_name,
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "curr_offset");
	ret = of_property_read_u32_array(np,
		prop_name, &sm->curr_offset, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->curr_offset);

	/* CURR SLOPE */
	snprintf(prop_name,
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "curr_slope");
	ret = of_property_read_u32_array(np,
		prop_name, &sm->curr_slope, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->curr_slope);

	/* temp_std */
	snprintf(prop_name, 
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_std");
	ret = of_property_read_u32_array(np, prop_name, &sm->temp_std, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <%d>\n", prop_name, sm->temp_std);

	/* temp_offset */
	snprintf(prop_name, 
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_offset");
	ret = of_property_read_u32_array(np, prop_name, temp_offset, 6);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	sm->en_high_fg_temp_offset = temp_offset[0];
	sm->high_fg_temp_offset_denom = temp_offset[1];
	sm->high_fg_temp_offset_fact = temp_offset[2];
	sm->en_low_fg_temp_offset = temp_offset[3];
	sm->low_fg_temp_offset_denom = temp_offset[4];
	sm->low_fg_temp_offset_fact = temp_offset[5];
	pr_info("%s = <%d, %d, %d, %d, %d, %d>\n", prop_name,
		sm->en_high_fg_temp_offset, 
		sm->high_fg_temp_offset_denom, sm->high_fg_temp_offset_fact,
		sm->en_low_fg_temp_offset,
		sm->low_fg_temp_offset_denom, sm->low_fg_temp_offset_fact);

	/* temp_calc */
	snprintf(prop_name, 
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "temp_cal");
	ret = of_property_read_u32_array(np, prop_name, temp_cal, 10);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	sm->en_high_fg_temp_cal = temp_cal[0];
	sm->high_fg_temp_p_cal_denom = temp_cal[1];
	sm->high_fg_temp_p_cal_fact = temp_cal[2];
	sm->high_fg_temp_n_cal_denom = temp_cal[3];
	sm->high_fg_temp_n_cal_fact = temp_cal[4];
	sm->en_low_fg_temp_cal = temp_cal[5];
	sm->low_fg_temp_p_cal_denom = temp_cal[6];
	sm->low_fg_temp_p_cal_fact = temp_cal[7];
	sm->low_fg_temp_n_cal_denom = temp_cal[8];
	sm->low_fg_temp_n_cal_fact = temp_cal[9];
	pr_info("%s = <%d, %d, %d, %d, %d, %d, %d, %d, %d, %d>\n", prop_name,
		sm->en_high_fg_temp_cal, 
		sm->high_fg_temp_p_cal_denom, sm->high_fg_temp_p_cal_fact,
		sm->high_fg_temp_n_cal_denom, sm->high_fg_temp_n_cal_fact,
		sm->en_low_fg_temp_cal,
		sm->low_fg_temp_p_cal_denom, sm->low_fg_temp_p_cal_fact,
		sm->low_fg_temp_n_cal_denom, sm->low_fg_temp_n_cal_fact);

	/* ext_temp_calc */
	snprintf(prop_name, 
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "ext_temp_cal");
	ret = of_property_read_u32_array(np, prop_name, ext_temp_cal, 10);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	sm->en_high_temp_cal = ext_temp_cal[0];
	sm->high_temp_p_cal_denom = ext_temp_cal[1];
	sm->high_temp_p_cal_fact = ext_temp_cal[2];
	sm->high_temp_n_cal_denom = ext_temp_cal[3];
	sm->high_temp_n_cal_fact = ext_temp_cal[4];
	sm->en_low_temp_cal = ext_temp_cal[5];
	sm->low_temp_p_cal_denom = ext_temp_cal[6];
	sm->low_temp_p_cal_fact = ext_temp_cal[7];
	sm->low_temp_n_cal_denom = ext_temp_cal[8];
	sm->low_temp_n_cal_fact = ext_temp_cal[9];
	pr_info("%s = <%d, %d, %d, %d, %d, %d, %d, %d, %d, %d>\n", prop_name,
		sm->en_high_temp_cal, 
		sm->high_temp_p_cal_denom, sm->high_temp_p_cal_fact,
		sm->high_temp_n_cal_denom, sm->high_temp_n_cal_fact,
		sm->en_low_temp_cal,
		sm->low_temp_p_cal_denom, sm->low_temp_p_cal_fact,
		sm->low_temp_n_cal_denom, sm->low_temp_n_cal_fact);

	/* FCM Offset */
	snprintf(prop_name,
		PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "fcm_offset");
	ret = of_property_read_u32_array(np,
		prop_name, &sm->fcm_offset, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->fcm_offset);
	/* get battery_temp_table*/
	 snprintf(prop_name, PROPERTY_NAME_SIZE,
		  "battery%d,%s", battery_id, "thermal_table");
	
	 ret = of_property_read_u32_array(np, prop_name, battery_temp_table, FG_TEMP_TABLE_CNT_MAX);
	 if (ret < 0)
		 pr_err("Can get prop %s (%d)\n", prop_name, ret);
	 for (i = 0; i < FG_TEMP_TABLE_CNT_MAX; i++) {
		 sm->battery_temp_table[i] = battery_temp_table[i];
		// pr_err("%s = <battery_temp_table[%d] 0x%x>\n",
		//	 prop_name, i,	battery_temp_table[i]);
	 }

    /* Battery Paramter */
	snprintf(prop_name, PROPERTY_NAME_SIZE, "battery%d,%s", battery_id, "param_version");
	ret = of_property_read_u32_array(np, prop_name, &sm->battery_param_version, 1);
	if (ret < 0)
		pr_err("Can get prop %s (%d)\n", prop_name, ret);
	pr_info("%s = <0x%x>\n", prop_name, sm->battery_param_version);
	
	return 0;
}

bool hal_fg_init(struct i2c_client *client)
{
	struct sm_fg_chip *sm = i2c_get_clientdata(client);

	pr_info("sm5602 hal_fg_init...\n");
	mutex_lock(&sm->data_lock);
	if (client->dev.of_node) {
		/* Load common data from DTS*/
		fg_common_parse_dt(sm);
		/* Load battery data from DTS*/
		fg_battery_parse_dt(sm);
	}

	if(!fg_init(client))
        return false;
	//sm->batt_temp = 250;

	mutex_unlock(&sm->data_lock);
	pr_info("hal fg init OK\n");
	return true;
}

static int sm5602_get_psy(struct sm_fg_chip *sm)
{

	if (sm->usb_psy && sm->batt_psy)
		return 0;

	sm->usb_psy = power_supply_get_by_name("usb");
	if (!sm->usb_psy) {
		pr_err("USB supply not found, defer probe\n");
		return -EINVAL;
	}

	sm->batt_psy = power_supply_get_by_name("battery");
	if (!sm->batt_psy) {
		pr_err("bms supply not found, defer probe\n");
		return -EINVAL;
	}

	return 0;
}

//20220108 : W/A for over 60degree
static void overtemp_delay_work(struct work_struct *work)
{
	struct sm_fg_chip *sm = container_of(work,
				struct sm_fg_chip,
				overtemp_delay_work.work);

	sm->overtemp_allow_restart = true;
	//sm->overtemp_cnt++;
	//if (sm->overtemp_cnt >= 20)
	//	sm->overtemp_cnt = 20;

}

/*if rawsoc less than 1% and vbat less than 3.4V then force UI_SOC update to 0%.*/
static void LowBatteryChecKFunc(struct work_struct *work)
{
	struct sm_fg_chip *sm = container_of(work,
			struct sm_fg_chip,
			LowBatteryCheckWork.work);
	int counter = 4;
	int low_soc_counter = 0;

	while(counter--){
		if(sm->batt_soc < 10 && sm->batt_volt < 3400){
			low_soc_counter++;
		}
		sm->batt_volt = fg_read_volt(sm);
		sm->batt_soc = fg_read_soc(sm);
		msleep(1000);
	}
	if(low_soc_counter == 4){
		sm->low_battery_power = true;
		sm->param.batt_raw_soc = 0;
		sm->batt_soc = 0;
	}else{
		sm->low_battery_power = false;
	}
	sm->start_low_battery_check = false;

	return;
}
static int sm5602_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct power_supply *psy = v;
	struct sm_fg_chip *sm = container_of(nb, struct sm_fg_chip, nb);
	union power_supply_propval pval = {0, };
	int rc;

	if (ev != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;

	rc = sm5602_get_psy(sm);
	if (rc < 0) {
		return NOTIFY_OK;
	}

	//if (strcmp(psy->desc->name, "usb") != 0)
	if (strcmp(psy->desc->name, "usb") != 0)
		return NOTIFY_OK;

	if (sm->usb_psy) {
		rc = power_supply_get_property(sm->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &pval);

		if (rc < 0) {
			pr_err("failed get usb present\n");
			return -EINVAL;
		}
		if (pval.intval) {
			sm->usb_present = true;
			pm_stay_awake(sm->dev);
		} else {
			sm->batt_sw_fc = false;
			sm->usb_present = false;
			pm_relax(sm->dev);
		}
	}

	return NOTIFY_OK;
}

static int sm_fg_probe(struct i2c_client *client,
							const struct i2c_device_id *id)
{

	int ret;
	struct sm_fg_chip *sm;
	u8 *regs;
	pr_err("2012.09.04 wsy %s: start\n", __func__);

	pr_info("enter\n");

	sm = devm_kzalloc(&client->dev, sizeof(*sm), GFP_KERNEL);

	if (!sm)
		return -ENOMEM;

	sm->dev = &client->dev;
	sm->client = client;
	sm->chip = id->driver_data;

	sm->batt_soc	= -ENODATA;
	sm->batt_fcc	= -ENODATA;
	//sm->batt_dc		= -ENODATA;
	sm->batt_volt	= -ENODATA;
	sm->batt_temp	= -ENODATA;
	sm->batt_curr	= -ENODATA;
	sm->fake_soc	= -EINVAL;
	sm->fake_temp	= -EINVAL;
#ifdef CONFIG_BATT_VERIFY_BY_DS28E16
	sm->max_verify_psy = power_supply_get_by_name("batt_verify");
	if(!sm->max_verify_psy){
		pr_err("batt_verify failed!!!\n");
		return -EPROBE_DEFER;
	}
#endif

	if (sm->chip == SM5602) {
		regs = sm5602_regs;
	} else {
		pr_err("unexpected fuel gauge: %d\n", sm->chip);
		regs = sm5602_regs;
	}

	memcpy(sm->regs, regs, NUM_REGS);

	i2c_set_clientdata(client, sm);

	mutex_init(&sm->i2c_rw_lock);
	mutex_init(&sm->data_lock);
	

	if(true != fg_check_device_id(client))
	{
		ret = -ENODEV; 
		goto err_free; 	
	}

	if (!hal_fg_init(client)) {
	    pr_err("Failed to Initialize Fuelgauge\n");
		ret = -ENODEV; 
        goto err_free; 	
	}

	fg_set_fastcharge_mode(sm, false);

	INIT_DELAYED_WORK(&sm->monitor_work, fg_monitor_workfunc);

	//INIT_DELAYED_WORK(&sm->soc_monitor_work, soc_monitor_work);

	//20220108 : W/A for over 60degree
	sm->overtemp_delay_on = false;
	sm->overtemp_allow_restart = false;
	sm->low_battery_power = false;
	sm->start_low_battery_check = false;
	INIT_DELAYED_WORK(&sm->overtemp_delay_work, overtemp_delay_work);
	INIT_DELAYED_WORK(&sm->LowBatteryCheckWork, LowBatteryChecKFunc);

	pr_info("overtemp_delay_on : %d\n", sm->overtemp_delay_on);

	fg_psy_register(sm);

#if (FG_REMOVE_IRQ == 0)
	if (sm->gpio_int != -EINVAL)
		//client->irq = sm->gpio_int;
		pr_err("unuse\n");
	else {
		pr_err("Failed to registe gpio interrupt\n");
		goto err_free;
	}

	if (client->irq) {
		ret = devm_request_threaded_irq(&client->dev, client->irq, NULL,
				fg_irq_thread,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"sm fuel gauge irq", sm);
		if (ret < 0) {
			pr_err("request irq for irq=%d failed, ret = %d\n", client->irq, ret);
			//goto err_1;
		}
	}
#endif
	//fg_irq_thread(client->irq, sm); // if IRQF_TRIGGER_FALLING or IRQF_TRIGGER_RISING is needed, enable initial irq.

	sm->nb.notifier_call = sm5602_notifier_call;
	ret = power_supply_reg_notifier(&sm->nb);
	if (ret < 0) {
		pr_err("Couldn't register psy notifier rc = %d\n", ret);
		return ret;
	}

	create_debugfs_entry(sm);

	ret = sysfs_create_group(&sm->dev->kobj, &fg_attr_group);
	if (ret)
		pr_err("Failed to register sysfs:%d\n", ret);

	//fg_dump_debug(sm);

	schedule_delayed_work(&sm->monitor_work, 10 * HZ);

	sm->param.batt_soc = -EINVAL;
	//schedule_delayed_work(&sm->soc_monitor_work, msecs_to_jiffies(MONITOR_SOC_WAIT_MS));
	
	pr_info("sm fuel gauge probe successfully, %s\n",device2str[sm->chip]);
	pr_err("2012.09.04 wsy %s: end\n", __func__);

	return 0;

//err_1:
//	fg_psy_unregister(sm);
err_free:
	mutex_destroy(&sm->data_lock);
	mutex_destroy(&sm->i2c_rw_lock);
	devm_kfree(&client->dev,sm);
	return ret;

}


static int sm_fg_remove(struct i2c_client *client)
{
	struct sm_fg_chip *sm = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&sm->monitor_work);
	cancel_delayed_work_sync(&sm->LowBatteryCheckWork);
	//cancel_delayed_work_sync(&sm->soc_monitor_work);

	fg_psy_unregister(sm);

	mutex_destroy(&sm->data_lock);
	mutex_destroy(&sm->i2c_rw_lock);

	debugfs_remove_recursive(sm->debug_root);

	sysfs_remove_group(&sm->dev->kobj, &fg_attr_group);

	return 0;

}

static void sm_fg_shutdown(struct i2c_client *client)
{
	pr_info("sm fuel gauge driver shutdown!\n");
}

static const struct of_device_id sm_fg_match_table[] = {
	{.compatible = "sm,sm5602",},
	{},
};
MODULE_DEVICE_TABLE(of, sm_fg_match_table);

static const struct i2c_device_id sm_fg_id[] = {
	{ "sm5602", SM5602 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sm_fg_id);

static struct i2c_driver sm_fg_driver = {
	.driver		= {
		.name		= "sm5602",
		.owner		= THIS_MODULE,
		.of_match_table	= sm_fg_match_table,
	},
	.id_table   = sm_fg_id,
	.probe		= sm_fg_probe,
	.remove		= sm_fg_remove,
	.shutdown     = sm_fg_shutdown,
};


module_i2c_driver(sm_fg_driver);

MODULE_DESCRIPTION("SM SM5602 Gauge Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Siliconmitus");

