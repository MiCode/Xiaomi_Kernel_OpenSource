/* ltr553.c
 * LTR-On LTR-553 Proxmity and Light sensor driver
 *
 * Copyright (C) 2011 Lite-On Technology Corp (Singapore)
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/jiffies.h>
#include <mach/gpio.h>
#include <linux/fs.h>
#include <linux/hardware_info.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <linux/miscdevice.h>

#include <linux/mutex.h>
#include <linux/ctype.h>
#include <linux/pm_runtime.h>
#include <linux/device.h>
#include <linux/input/ltr553.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/sensors.h>
#include <linux/regulator/consumer.h>
#include <linux/wakelock.h>
#include <linux/hardware_info.h>

#define SENSOR_NAME		"proximity"
#define LTR553_DRV_NAME		"ltr553"
#define LTR553_MANUFAC_ID	0x05

#define VENDOR_NAME		"lite-on"
#define LTR553_SENSOR_NAME		"ltr553als"
#define DRIVER_VERSION		"1.0"
#define ALSPS_LS_ENABLE	_IOW('c', 5, int *)
#define ALSPS_PS_ENABLE	_IOW('c', 6, int *)
#define ALSPS_LSENSOR_LUX_DATA	_IOW('c', 7, int *)
#define ALSPS_PSENSOR_ABS_DISTANCE_DATA	_IOW('c', 8, int *)
#define ALSPS_GET_PS_RAW_DATA_FOR_CALI	_IOW('c', 9, int *)
#define ALSPS_REC_PS_DATA_FOR_CALI	_IOW('c', 10, int *)

#define PS_AUTO_CALIBRATION		0

#if PS_AUTO_CALIBRATION
#define	PS_MAX_INIT_KEPT_DATA_COUNTER		8
#define	PS_MAX_MOV_AVG_KEPT_DATA_CTR		7
#define PS_MIN_MEASURE_VAL	0
#define PS_MAX_MEASURE_VAL	2047
#define	FAR_VAL		1
#define	NEAR_VAL		0
uint16_t ps_init_kept_data[PS_MAX_INIT_KEPT_DATA_COUNTER];
uint16_t ps_ct_avg;
uint8_t ps_grabData_stage;
uint32_t ftn_init;
uint32_t ftn_final;
uint32_t ntf_final;
uint8_t ps_kept_data_counter;
uint16_t ps_movavg_data[PS_MAX_MOV_AVG_KEPT_DATA_CTR];
uint8_t ps_movavg_data_counter;
uint16_t ps_movct_avg;
#endif

#define ALS_AVR_COUNT 5
#define ALS_DO_AVR
#ifdef ALS_DO_AVR
static int als_times;
static int als_temp[ALS_AVR_COUNT] = {0};
static int first_cycle = 1;
#endif

static u32 ps_state_last = 1;

struct ltr553_data {

	struct i2c_client *client;
	struct input_dev *input_dev_als;
	struct input_dev *input_dev_ps;
	struct sensors_classdev als_cdev;
	struct sensors_classdev ps_cdev;

	struct ltr553_platform_data *platform_data;

	/* regulator data */
	bool power_state;
	struct regulator *vdd;
	struct regulator *vio;

	/* interrupt type is level-style */
	struct mutex lockw;
	struct mutex op_lock;

	struct delayed_work ps_work;
	struct delayed_work als_work;
	uint8_t ps_opened;
	u8 ps_open_state;
	u8 als_open_state;
	u16 irq;
	u32 ps_state;
	u32 last_lux;
	int distance_flag;
	int fastmmi_ls_flag;
	struct wake_lock ltr553_wake_lock;
};
struct ltr553_data *sensor_info;

struct ltr553_reg {
	const char *name;
	u8 addr;
	u16 defval;
	u16 curval;
};

enum ltr553_reg_tbl {
	REG_ALS_CONTR,
	REG_PS_CONTR,
	REG_ALS_PS_STATUS,
	REG_INTERRUPT,
	REG_PS_LED,
	REG_PS_N_PULSES,
	REG_PS_MEAS_RATE,
	REG_ALS_MEAS_RATE,
	REG_MANUFACTURER_ID,
	REG_INTERRUPT_PERSIST,
	REG_PS_THRES_LOW,
	REG_PS_THRES_UP,
	REG_ALS_THRES_LOW,
	REG_ALS_THRES_UP,
	REG_ALS_DATA_CH1,
	REG_ALS_DATA_CH0,
	REG_PS_DATA
};

static int ltr553_als_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable);
static int ltr553_ps_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable);
static int ltr553_als_set_poll_delay(struct ltr553_data *data, unsigned long delay);
static int sensor_platform_hw_power_onoff(struct ltr553_data *data, bool on);
int ltr553_device_init(struct i2c_client *client);


int ltr553_ps_reg_init(struct i2c_client *client);
static struct ltr553_reg ps_reg_tbl[] = {
		{
			.name = "PS_LED",
			.addr = 0x82,
			.defval = 0x7f,
			.curval = 0x7f,
		},
		{
			.name = "PS_N_PULSES",
			.addr = 0x83,
			.defval = 0x01,
			.curval = 0x06,
		},
		{
			.name = "PS_MEAS_RATE",
			.addr = 0x84,
			.defval = 0x02,
			.curval = 0x00,
		},
		{
			.name = "INTERRUPT_PERSIST",
			.addr = 0x9e,
			.defval = 0x00,
			.curval = 0x23,
		},
		{
			.name = "INTERRUPT",
			.addr = 0x8f,
			.defval = 0x00,
			.curval = 0x01,
		}
};

static  struct ltr553_reg reg_tbl[] = {
		{
			.name   = "ALS_CONTR",
			.addr   = 0x80,
			.defval = 0x00,
			.curval = 0x19,
		},
		{
			.name = "PS_CONTR",
			.addr = 0x81,
			.defval = 0x00,
			.curval = 0x03,
		},
		{
			.name = "ALS_PS_STATUS",
			.addr = 0x8c,
			.defval = 0x00,
			.curval = 0x00,
		},
		{
			.name = "INTERRUPT",
			.addr = 0x8f,
			.defval = 0x00,
			.curval = 0x01,
		},
		{
			.name = "PS_LED",
			.addr = 0x82,
			.defval = 0x7f,
			.curval = 0x7f,
		},
		{
			.name = "PS_N_PULSES",
			.addr = 0x83,
			.defval = 0x01,
			.curval = 0x06,
		},
		{
			.name = "PS_MEAS_RATE",
			.addr = 0x84,
			.defval = 0x02,
			.curval = 0x00,
		},
		{
			.name = "ALS_MEAS_RATE",
			.addr = 0x85,
			.defval = 0x03,
			.curval = 0x02,
		},
		{
			.name = "MANUFACTURER_ID",
			.addr = 0x87,
			.defval = 0x05,
			.curval = 0x05,
		},
		{
			.name = "INTERRUPT_PERSIST",
			.addr = 0x9e,
			.defval = 0x00,
			.curval = 0x23,
		},
		{
			.name = "PS_THRES_LOW",
			.addr = 0x92,
			.defval = 0x0000,
			.curval = 0x0000,
		},
		{
			.name = "PS_THRES_UP",
			.addr = 0x90,
			.defval = 0x07ff,
			.curval = 0x0000,
		},
		{
			.name = "ALS_THRES_LOW",
			.addr = 0x99,
			.defval = 0x0000,
			.curval = 0x0000,
		},
		{
			.name = "ALS_THRES_UP",
			.addr = 0x97,
			.defval = 0xffff,
			.curval = 0x0000,
		},
		{
			.name = "ALS_DATA_CH1",
			.addr = 0x88,
			.defval = 0x0000,
			.curval = 0x0000,
		},
		{
			.name = "ALS_DATA_CH0",
			.addr = 0x8a,
			.defval = 0x0000,
			.curval = 0x0000,
		},
		{
			.name = "PS_DATA",
			.addr = 0x8d,
			.defval = 0x0000,
			.curval = 0x0000,
		},
};

static struct sensors_classdev sensors_light_cdev = {
	.name = "light",
	.vendor = "liteon",
	.version = 1,
	.handle = SENSORS_LIGHT_HANDLE,
	.type = SENSOR_TYPE_LIGHT,
	.max_range = "60000",
	.resolution = "0.0125",
	.sensor_power = "0.20",
	.min_delay = 0, /* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static struct sensors_classdev sensors_proximity_cdev = {
	.name = "proximity",
	.vendor = "liteon",
	.version = 1,
	.handle = SENSORS_PROXIMITY_HANDLE,
	.type = SENSOR_TYPE_PROXIMITY,
	.max_range = "5",
	.resolution = "5.0",
	.sensor_power = "3",
	.min_delay = 0, /* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static int ltr553_chip_reset(struct i2c_client *client)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, LTR553_ALS_CONTR, MODE_ALS_StdBy);
	ret = i2c_smbus_write_byte_data(client, LTR553_PS_CONTR, MODE_PS_StdBy);
	ret = i2c_smbus_write_byte_data(client, LTR553_ALS_CONTR, 0x02);
	if (ret < 0)
		printk("%s reset chip fail\n", __func__);

	return ret;
}

static void ltr553_set_ps_threshold(struct i2c_client *client, u8 addr, u16 value)
{
	i2c_smbus_write_byte_data(client, addr, (value & 0xff));
	i2c_smbus_write_byte_data(client, addr+1, (value >> 8));
}

#if PS_AUTO_CALIBRATION
static void setThrDuringCall(void)
{
	ps_grabData_stage = 0;
	ps_kept_data_counter = 0;
	ps_movavg_data_counter = 0;
	ltr553_set_ps_threshold(sensor_info->client, LTR553_PS_THRES_LOW_0, 0);
	ltr553_set_ps_threshold(sensor_info->client, LTR553_PS_THRES_UP_0, 0);

}
#endif
static int ltr553_ps_enable(struct i2c_client *client, int on)
{
	struct ltr553_data *data = i2c_get_clientdata(client);
	int ret = 0;
	int contr_data;

	if (on) {
		ltr553_ps_reg_init(client);

		ltr553_set_ps_threshold(client, LTR553_PS_THRES_LOW_0, 0);
		ltr553_set_ps_threshold(client, LTR553_PS_THRES_UP_0, data->platform_data->prox_threshold);
		ret = i2c_smbus_write_byte_data(client, LTR553_PS_CONTR, reg_tbl[REG_PS_CONTR].curval);

		if (ret < 0) {
			pr_err("%s: enable=(%d) failed!\n", __func__, on);
			return ret;
		}
		contr_data = i2c_smbus_read_byte_data(client, LTR553_PS_CONTR);
		if (contr_data != reg_tbl[REG_PS_CONTR].curval) {

			pr_err("%s: enable=(%d) failed!\n", __func__, on);
			return -EFAULT;
		}
#if PS_AUTO_CALIBRATION
		setThrDuringCall();
#endif
		msleep(WAKEUP_DELAY);

		data->ps_state = 1;
		input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_state);
	} else {
		ret = i2c_smbus_write_byte_data(client, LTR553_PS_CONTR, MODE_PS_StdBy);
		if (ret < 0) {
			pr_err("%s: enable=(%d) failed!\n", __func__, on);
			return ret;
		}

		ps_state_last = 1;
		contr_data = i2c_smbus_read_byte_data(client, LTR553_PS_CONTR);
		if (contr_data != reg_tbl[REG_PS_CONTR].defval) {
			pr_err("%s:  enable=(%d) failed!\n", __func__, on);
			return -EFAULT;
		}
	}
	pr_err("%s: enable=(%d) OK\n", __func__, on);
	return ret;
}

/*
 * Absent Light Sensor Congfig
 */
static int ltr553_als_enable(struct i2c_client *client, int on)
{
	struct ltr553_data *data = i2c_get_clientdata(client);
	int ret;

#ifdef ALS_DO_AVR
	int i;
	als_times = 0;
	for (i = 0; i < ALS_AVR_COUNT; i++)
		als_temp[i] = 0;
	first_cycle = 1;
#endif

	if (on) {
		ret = i2c_smbus_write_byte_data(client, LTR553_ALS_MEAS_RATE, reg_tbl[REG_ALS_MEAS_RATE].curval);

		ret = i2c_smbus_write_byte_data(client, LTR553_ALS_CONTR, reg_tbl[REG_ALS_CONTR].curval);
		msleep(WAKEUP_DELAY);
		ret |= i2c_smbus_read_byte_data(client, LTR553_ALS_DATA_CH0_1);

		cancel_delayed_work_sync(&data->als_work);
		schedule_delayed_work(&data->als_work, msecs_to_jiffies(data->platform_data->als_poll_interval));

	} else {
		cancel_delayed_work_sync(&data->als_work);
		ret = i2c_smbus_write_byte_data(client, LTR553_ALS_CONTR, MODE_ALS_StdBy);
	}

	pr_err("%s: enable=(%d) ret=%d\n", __func__, on, ret);
	return ret;
}

#if PS_AUTO_CALIBRATION
static uint16_t discardMinMax_findCTMov_Avg(uint16_t *ps_val)
{
#define MAX_NUM_PS_DATA1		PS_MAX_MOV_AVG_KEPT_DATA_CTR
#define STARTING_PS_INDEX1		0
#define ENDING_PS_INDEX1		5
#define NUM_AVG_DATA1			5

	uint8_t i_ctr, i_ctr2, maxIndex, minIndex;
	uint16_t maxVal, minVal, _ps_val[MAX_NUM_PS_DATA1];
	uint16_t temp = 0;

	for (i_ctr = STARTING_PS_INDEX1; i_ctr < MAX_NUM_PS_DATA1; i_ctr++)
		_ps_val[i_ctr] = ps_val[i_ctr];

	maxVal = ps_val[STARTING_PS_INDEX1];
	maxIndex = STARTING_PS_INDEX1;
	minVal = ps_val[STARTING_PS_INDEX1];
	minIndex = STARTING_PS_INDEX1;

	for (i_ctr = STARTING_PS_INDEX1; i_ctr < MAX_NUM_PS_DATA1; i_ctr++) {
		if (ps_val[i_ctr] > maxVal) {
			maxVal = ps_val[i_ctr];
			maxIndex = i_ctr;
		}
	}

	for (i_ctr = STARTING_PS_INDEX1; i_ctr < MAX_NUM_PS_DATA1; i_ctr++) {
		if (ps_val[i_ctr] < minVal) {
			minVal = ps_val[i_ctr];
			minIndex = i_ctr;
		}
	}

	i_ctr2 = 0;
	if (minIndex != maxIndex) {
		for (i_ctr = STARTING_PS_INDEX1;
				i_ctr < MAX_NUM_PS_DATA1; i_ctr++) {
			if ((i_ctr != minIndex) && (i_ctr != maxIndex)) {
				ps_val[i_ctr2] = _ps_val[i_ctr];
				i_ctr2++;
			}
		}
	}
	ps_val[MAX_NUM_PS_DATA1 - 1] = 0;
	ps_val[MAX_NUM_PS_DATA1 - 2] = 0;

	for (i_ctr = STARTING_PS_INDEX1; i_ctr < ENDING_PS_INDEX1; i_ctr++)
		temp += ps_val[i_ctr];

	temp = (temp / NUM_AVG_DATA1);
	return temp;
}

static uint16_t findCT_Avg(uint16_t *ps_val)
{
#define MAX_NUM_PS_DATA2		PS_MAX_INIT_KEPT_DATA_COUNTER
#define STARTING_PS_INDEX2		3
#define NUM_AVG_DATA2			3

	uint8_t i_ctr, min_Index, max_Index;
	uint16_t max_val, min_val;
	uint16_t temp = 0;
	/*struct ltr553_data *ltr553 = sensor_info;*/

	max_val = ps_val[STARTING_PS_INDEX2];
	max_Index = STARTING_PS_INDEX2;
	min_val = ps_val[STARTING_PS_INDEX2];
	min_Index = STARTING_PS_INDEX2;

	for (i_ctr = STARTING_PS_INDEX2; i_ctr < MAX_NUM_PS_DATA2; i_ctr++) {
		if (ps_val[i_ctr] > max_val) {
			max_val = ps_val[i_ctr];
			max_Index = i_ctr;
		}
	}

	for (i_ctr = STARTING_PS_INDEX2; i_ctr < MAX_NUM_PS_DATA2; i_ctr++) {
		if (ps_val[i_ctr] < min_val) {
			min_val = ps_val[i_ctr];
			min_Index = i_ctr;
		}
	}

	if (min_val == max_val)
		/* all values are the same*/
		temp = ps_val[STARTING_PS_INDEX2];
	else {
		for (i_ctr = STARTING_PS_INDEX2;
				i_ctr < MAX_NUM_PS_DATA2; i_ctr++) {
			if ((i_ctr != min_Index) && (i_ctr != max_Index))
				temp += ps_val[i_ctr];
		}
		temp = (temp / NUM_AVG_DATA2);
	}
	return temp;
}
#endif

static int ltr553_als_read(struct i2c_client *client)
{
		int alsval_ch0_lo, alsval_ch0_hi, alsval_ch0;
		int alsval_ch1_lo, alsval_ch1_hi, alsval_ch1;
		int luxdata;
		int ch1_co, ch0_co, ratio;
#ifdef ALS_DO_AVR
		int sum = 0;
		int i;
#endif
		alsval_ch1_lo = i2c_smbus_read_byte_data(client, LTR553_ALS_DATA_CH1_0);
		alsval_ch1_hi = i2c_smbus_read_byte_data(client, LTR553_ALS_DATA_CH1_1);
		if (alsval_ch1_lo < 0 || alsval_ch1_hi < 0)
			return -EPERM;
		alsval_ch1 = (alsval_ch1_hi << 8) + alsval_ch1_lo;

		alsval_ch0_lo = i2c_smbus_read_byte_data(client, LTR553_ALS_DATA_CH0_0);
		alsval_ch0_hi = i2c_smbus_read_byte_data(client, LTR553_ALS_DATA_CH0_1);
		if (alsval_ch0_lo < 0 || alsval_ch0_hi < 0)
			return -EPERM;
		alsval_ch0 = (alsval_ch0_hi << 8) + alsval_ch0_lo;

		if ((alsval_ch0 + alsval_ch1) == 0)
			ratio = 1000;
		else
			ratio = alsval_ch1 * 1000 / (alsval_ch1 + alsval_ch0);

		if (ratio < 450) {
			ch0_co = 17743;
			ch1_co = -11059;
		} else if ((ratio >= 450) && (ratio < 640)) {
			ch0_co = 42785;
			ch1_co = 19548;
		} else if ((ratio >= 640) && (ratio < 850)) {
			ch0_co = 5926;
			ch1_co = -1185;
		} else if (ratio >= 850) {
			ch0_co = 0;
			ch1_co = 0;
		}
		luxdata = (alsval_ch0 * ch0_co - alsval_ch1 * ch1_co) / 10000;
#ifdef ALS_DO_AVR
		als_temp[als_times] = luxdata;
		als_times++;

		if (first_cycle) {
			for (i = 0; i < als_times; i++) {
				sum += als_temp[i];
			}
			luxdata = sum / als_times;
		} else {
			for (i = 0; i < ALS_AVR_COUNT; i++) {
				sum += als_temp[i];
			}
			luxdata = sum / ALS_AVR_COUNT;
		}

		if (als_times >= ALS_AVR_COUNT) {
			als_times = 0;
			first_cycle = 0;
		}
#endif
		return luxdata;
}

static void ltr553_ps_work_func(struct work_struct *work)
{
	struct ltr553_data *data = container_of(work, struct ltr553_data, ps_work.work);
	struct i2c_client *client = data->client;
	int als_ps_status;
	int psval_lo, psval_hi;

#if PS_AUTO_CALIBRATION
	int adc_value;
	als_ps_status = i2c_smbus_read_byte_data(client, LTR553_ALS_PS_STATUS);
	if (als_ps_status < 0)
			goto workout;

	if ((data->ps_open_state == 1) && (als_ps_status & 0x02)) {
		psval_lo = i2c_smbus_read_byte_data(client, LTR553_PS_DATA_0);
		if (psval_lo < 0) {
			adc_value = psval_lo;
			goto workout;
		}
		psval_hi = i2c_smbus_read_byte_data(client, LTR553_PS_DATA_1);
		if (psval_hi < 0) {
			adc_value = psval_hi;
			goto workout;
		}
		adc_value = ((psval_hi & 7) << 8) | psval_lo;

	}
	if (ps_grabData_stage == 0) {
		if (ps_kept_data_counter < PS_MAX_INIT_KEPT_DATA_COUNTER) {
			if (adc_value != 0) {
				ps_init_kept_data[ps_kept_data_counter] =
					adc_value;
				ps_kept_data_counter++;
			}
		}

		if (ps_kept_data_counter >= PS_MAX_INIT_KEPT_DATA_COUNTER) {
			ps_ct_avg = findCT_Avg(ps_init_kept_data);
			ftn_init = ps_ct_avg * 15;
			ps_grabData_stage = 1;
		}
	}

	if (ps_grabData_stage == 1) {
		if ((ftn_init - (ps_ct_avg * 10)) < 1000)
			ftn_final = (ps_ct_avg * 10) + 1000;
		else {
			if ((ftn_init - (ps_ct_avg * 10)) > 1500)
				ftn_final = (ps_ct_avg * 10) + 1500;
			else
				ftn_final = ftn_init;
		}
		ntf_final = (ftn_final - (ps_ct_avg * 10));
		ntf_final *= 4;
		ntf_final /= 100;
		ntf_final += ps_ct_avg;
		ftn_final /= 10;
		if (ntf_final >= PS_MAX_MEASURE_VAL)
			ntf_final = PS_MAX_MEASURE_VAL;
		if (ftn_final >= PS_MAX_MEASURE_VAL)
			ftn_final = PS_MAX_MEASURE_VAL;
		ps_grabData_stage = 2;
	}

	if (ps_grabData_stage == 2) {
		/* report NEAR or FAR to the user layer */
		if ((adc_value > ftn_final) || (adc_value < ntf_final)) {
			if (adc_value > ftn_final) {
				input_report_abs(data->input_dev_ps,
					ABS_DISTANCE, NEAR_VAL);
				input_sync(data->input_dev_ps);
				data->distance_flag = 0;
			}

			if (adc_value < ntf_final) {
				input_report_abs(data->input_dev_ps,
					ABS_DISTANCE, FAR_VAL);
				input_sync(data->input_dev_ps);
				data->distance_flag = 1;
			}

		}
		/* report NEAR or FAR to the user layer */
		if (ps_movavg_data_counter < PS_MAX_MOV_AVG_KEPT_DATA_CTR) {
			if (adc_value != 0) {
				ps_movavg_data[ps_movavg_data_counter] =
					adc_value;
				ps_movavg_data_counter++;
			}
		}
		if (ps_movavg_data_counter >= PS_MAX_MOV_AVG_KEPT_DATA_CTR) {
			ps_movct_avg =
				discardMinMax_findCTMov_Avg(ps_movavg_data);
			if (ps_movct_avg < ps_ct_avg) {
				ps_ct_avg = ps_movct_avg;
				ftn_init = ps_ct_avg * 17;
				ps_grabData_stage = 1;
			}
			ps_movavg_data_counter = 5;
		}
	}
#else
	static int val_temp = 1;
	int psdata;
	mutex_lock(&data->op_lock);

	als_ps_status = i2c_smbus_read_byte_data(client, LTR553_ALS_PS_STATUS);
	if (als_ps_status < 0)
			goto workout;
	if ((data->ps_open_state == 1) && (als_ps_status & 0x02)) {
		psval_lo = i2c_smbus_read_byte_data(client, LTR553_PS_DATA_0);
		if (psval_lo < 0) {
			psdata = psval_lo;
			goto workout;
		}
		psval_hi = i2c_smbus_read_byte_data(client, LTR553_PS_DATA_1);
		if (psval_hi < 0) {
			psdata = psval_hi;
			goto workout;
		}
		psdata = ((psval_hi & 7) << 8) | psval_lo;
		if (psdata > data->platform_data->prox_threshold) {
			data->ps_state = 0;
			ltr553_set_ps_threshold(client, LTR553_PS_THRES_LOW_0, data->platform_data->prox_hsyteresis_threshold);
			ltr553_set_ps_threshold(client, LTR553_PS_THRES_UP_0, 0x07ff);
			val_temp = 0;
		} else if (psdata < data->platform_data->prox_hsyteresis_threshold) {
			data->ps_state = 1;
			ltr553_set_ps_threshold(client, LTR553_PS_THRES_LOW_0, 0);
			ltr553_set_ps_threshold(client, LTR553_PS_THRES_UP_0, data->platform_data->prox_threshold);
			val_temp = 1;
		} else {
			data->ps_state = val_temp;
		}

		if (ps_state_last != data->ps_state) {
			input_report_abs(data->input_dev_ps, ABS_DISTANCE, data->ps_state);
			input_sync(data->input_dev_ps);
			ps_state_last = data->ps_state;
			data->distance_flag = data->ps_state;
		}
	}
#endif

workout:
	enable_irq(data->irq);
	mutex_unlock(&data->op_lock);
}

static void ltr553_als_work_func(struct work_struct *work)
{
	struct ltr553_data *data = container_of(work, struct ltr553_data, als_work.work);
	struct i2c_client *client = data->client;
	int als_ps_status;
	int als_data;
	static int active_count;

	mutex_lock(&data->op_lock);

	if (!data->als_open_state)
		goto workout;

	als_ps_status = i2c_smbus_read_byte_data(client, LTR553_ALS_PS_STATUS);
	if (als_ps_status < 0)
		goto workout;

	if ((data->als_open_state == 1) && (als_ps_status & 0x04)) {
		als_data = ltr553_als_read(client);
		if (als_data > 50000)
			als_data = 50000;

		if ((als_data >= 0) && (als_data != data->last_lux)) {
			data->last_lux = als_data;
			input_report_abs(data->input_dev_als, ABS_MISC, als_data);
			input_sync(data->input_dev_als);
		} else if (als_data == data->last_lux && als_data == 0 && active_count++ < 5) {
			input_report_abs(data->input_dev_als, ABS_MISC, active_count%2);
			input_sync(data->input_dev_als);
		}
	}

	schedule_delayed_work(&data->als_work, msecs_to_jiffies(data->platform_data->als_poll_interval));
workout:
	mutex_unlock(&data->op_lock);
}

static irqreturn_t ltr553_irq_handler(int irq, void *arg)
{
	struct ltr553_data *data = (struct ltr553_data *)arg;

	if (NULL == data)
		return IRQ_HANDLED;
	disable_irq_nosync(data->irq);
	schedule_delayed_work(&data->ps_work, 0);
	return IRQ_HANDLED;
}

static int ltr553_gpio_irq(struct ltr553_data *data)
{
	struct device_node *np = data->client->dev.of_node;
	int err = 0;

	data->platform_data->int_gpio = of_get_named_gpio_flags(np, "ltr,irq-gpio", 0, &data->platform_data->irq_gpio_flags);
	if (data->platform_data->int_gpio < 0)
		return -EIO;

	if (gpio_is_valid(data->platform_data->int_gpio)) {
		err = gpio_request(data->platform_data->int_gpio, "ltr553_irq_gpio");
		if (err) {
			printk("%s irq gpio request failed\n", __func__);
			return -EINTR;
		}

		err = gpio_direction_input(data->platform_data->int_gpio);
		if (err) {
			printk("%s set_direction for irq gpio failed\n", __func__);
			return -EIO;
		}
	}

	data->irq = data->client->irq = gpio_to_irq(data->platform_data->int_gpio);

	irq_to_desc(data->irq)->status_use_accessors |= IRQ_NOAUTOEN;

	if (request_irq(data->irq, ltr553_irq_handler, IRQ_TYPE_LEVEL_LOW/*IRQF_DISABLED|IRQ_TYPE_EDGE_FALLING*/,
		LTR553_DRV_NAME, data)) {
			printk("%s Could not allocate ltr553_INT !\n", __func__);
			return -EINTR;
	}
	return 0;
}


static void ltr553_gpio_irq_free(struct ltr553_data *data)
{
	free_irq(data->irq, data);
	gpio_free(data->platform_data->int_gpio);
}

static ssize_t ltr553_show_enable_ps(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ltr553_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", data->ps_open_state);
}

static ssize_t ltr553_store_enable_ps(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	/* If proximity work,then ALS must be enable */
	unsigned long val;
	char *after;
	struct ltr553_data *data = dev_get_drvdata(dev);

	val = simple_strtoul(buf, &after, 10);
	mutex_lock(&data->lockw);
#if 1
	ltr553_ps_set_enable(&data->ps_cdev, (unsigned int)val);
#else
	ltr553_ps_enable(data->client, (int)val);
#endif
	mutex_unlock(&data->lockw);
	return size;
}

static ssize_t ltr553_show_poll_delay_als(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ltr553_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", data->platform_data->als_poll_interval);
}

static ssize_t ltr553_store_poll_delay_als(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* If proximity work,then ALS must be enable */
	unsigned long val;
	char *after;
	struct ltr553_data *data = dev_get_drvdata(dev);

	val = simple_strtoul(buf, &after, 10);
	mutex_lock(&data->lockw);
	ltr553_als_set_poll_delay(data, val);
	mutex_unlock(&data->lockw);
	return size;
}

static ssize_t ltr553_show_enable_als(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ltr553_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%u\n", data->als_open_state);
}

static ssize_t ltr553_store_enable_als(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	/* If proximity work,then ALS must be enable */
	unsigned long val;
	char *after;
	struct ltr553_data *data = dev_get_drvdata(dev);

	val = simple_strtoul(buf, &after, 10);
	mutex_lock(&data->lockw);
#if 1
	ltr553_als_set_enable(&data->als_cdev, (unsigned int)val);
#else
	ltr553_als_enable(data->client, (int)val);
#endif
	mutex_unlock(&data->lockw);
	return size;
}

static ssize_t ltr553_driver_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
		return sprintf(buf, "Chip: %s %s\nVersion: %s\n",
				VENDOR_NAME, LTR553_SENSOR_NAME, DRIVER_VERSION);
}

static ssize_t ltr553_show_debug_regs(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	u8 val, high, low;
	int i;
	char *after;
	struct ltr553_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	after = buf;

	after += sprintf(after, "%-17s%5s%14s%16s\n", "Register Name", "address", "default", "current");
	for (i = 0; i < sizeof(reg_tbl)/sizeof(reg_tbl[0]); i++) {
			if (reg_tbl[i].name == NULL || reg_tbl[i].addr == 0)
					break;
			if (i < 10) {
					val = i2c_smbus_read_byte_data(client, reg_tbl[i].addr);
					after += sprintf(after, "%-20s0x%02x\t  0x%02x\t\t  0x%02x\n", reg_tbl[i].name, reg_tbl[i].addr, reg_tbl[i].defval, val);
			} else {
					low = i2c_smbus_read_byte_data(client, reg_tbl[i].addr);
					high = i2c_smbus_read_byte_data(client, reg_tbl[i].addr+1);
					after += sprintf(after, "%-20s0x%02x\t0x%04x\t\t0x%04x\n", reg_tbl[i].name, reg_tbl[i].addr, reg_tbl[i].defval, (high << 8) + low);
			}
	}
	after += sprintf(after, "\nYou can echo '0xaa=0xbb' to set the value 0xbb to the register of address 0xaa.\n ");

	return after - buf;
}

static ssize_t ltr553_store_debug_regs(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	/* If proximity work,then ALS must be enable */
	char *after, direct;
	u8 addr, val;
	struct ltr553_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	addr = simple_strtoul(buf, &after, 16);
	direct = *after;
	val = simple_strtoul((after+1), &after, 16);

	if (!((addr >= 0x80 && addr <= 0x93)
				|| (addr >= 0x97 && addr <= 0x9e)))
		return -EINVAL;

	mutex_lock(&data->lockw);
	if (direct == '=')
		i2c_smbus_write_byte_data(client, addr, val);
	else
		printk("%s: register(0x%02x) is: 0x%02x\n", __func__, addr, i2c_smbus_read_byte_data(client, addr));
	mutex_unlock(&data->lockw);

	return after - buf;
}

static ssize_t ps_adc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ltr553_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	u8 high, low;
	char *after;
	after = buf;

	low = i2c_smbus_read_byte_data(client, LTR553_PS_DATA_0);
	high = i2c_smbus_read_byte_data(client, LTR553_PS_DATA_1);
	if (low < 0 || high < 0)
		after += sprintf(after, "Failed to read PS adc data.\n");
	else
		after += sprintf(after, "%d\n", (high << 8) + low);

	return after - buf;
}

static ssize_t ltr553_show_lux_data(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int lux;
	struct ltr553_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	lux = ltr553_als_read(client);
	return sprintf(buf, "%d\n", lux);
}

static DEVICE_ATTR(debug_regs, 0444, ltr553_show_debug_regs,
				ltr553_store_debug_regs);
static DEVICE_ATTR(enable_als_sensor, 0444, ltr553_show_enable_als,
				ltr553_store_enable_als);
static DEVICE_ATTR(enable, 0444, ltr553_show_enable_ps,
				ltr553_store_enable_ps);
static DEVICE_ATTR(poll_delay, 0444, ltr553_show_poll_delay_als,
				ltr553_store_poll_delay_als);
static DEVICE_ATTR(info, 0444, ltr553_driver_info_show, NULL);
static DEVICE_ATTR(ps_adc, 0444, ps_adc_show, NULL);
static DEVICE_ATTR(lux_adc, 0444, ltr553_show_lux_data, NULL);

static struct attribute *ltr553_attributes[] = {
		&dev_attr_enable.attr,
		&dev_attr_info.attr,
		&dev_attr_enable_als_sensor.attr,
		&dev_attr_poll_delay.attr,
		&dev_attr_debug_regs.attr,
		&dev_attr_ps_adc.attr,
		&dev_attr_lux_adc.attr,
		NULL,
};

static const struct attribute_group ltr553_attr_group = {
		.attrs = ltr553_attributes,
};

static int ltr553_als_set_poll_delay(struct ltr553_data *data, unsigned long delay)
{

	if (data->fastmmi_ls_flag == 1) {
		if (delay < 1)
			delay = 1;
		if (delay > 1000)
			delay = 1000;

		if (data->platform_data->als_poll_interval != delay)
			data->platform_data->als_poll_interval = delay;

		cancel_delayed_work_sync(&data->als_work);
		schedule_delayed_work(&data->als_work, msecs_to_jiffies(data->platform_data->als_poll_interval));

	} else {
		mutex_lock(&data->op_lock);
		if (delay < 1)
			delay = 1;
		if (delay > 1000)
			delay = 1000;

		if (data->platform_data->als_poll_interval != delay)
			data->platform_data->als_poll_interval = delay;

		if (!data->als_open_state)
			return -ESRCH;

		cancel_delayed_work_sync(&data->als_work);
		schedule_delayed_work(&data->als_work, msecs_to_jiffies(data->platform_data->als_poll_interval));
		mutex_unlock(&data->op_lock);
	}
	return 0;
}

static int ltr553_als_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct ltr553_data *data = container_of(sensors_cdev, struct ltr553_data, als_cdev);
	int ret = 0;

	if ((enable != 0) && (enable != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enable);
		return -EINVAL;
	}

	ret = ltr553_als_enable(data->client, enable);
	if (ret < 0) {
		pr_err("%s: enable(%d) failed!\n", __func__, enable);
		return -EFAULT;
	}

	data->als_open_state = enable;
	pr_err("%s: enable=(%d), data->als_open_state=%d\n", __func__, enable, data->als_open_state);
	return ret;
}
static int ltr553_als_poll_delay(struct sensors_classdev *sensors_cdev,
		unsigned int delay_msec)
{
	struct ltr553_data *data = container_of(sensors_cdev, struct ltr553_data, als_cdev);
	ltr553_als_set_poll_delay(data, delay_msec);
	return 0;
}

static int ltr553_ps_set_enable_real(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	struct ltr553_data *data = container_of(sensors_cdev, struct ltr553_data, ps_cdev);
	int ret = 0;
	static int current_status = -1;

	if (current_status == enable)
		return ret;

	if ((enable != 0) && (enable != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enable);
		return -EINVAL;
	}

	if (enable == 1) {
		enable_irq(data->irq);
		wake_lock(&data->ltr553_wake_lock);
	} else {
		disable_irq_nosync(data->irq);
		wake_unlock(&data->ltr553_wake_lock);
	}
	ret = ltr553_ps_enable(data->client, enable);
	if (ret < 0) {
		pr_err("%s: enable(%d) failed!\n", __func__, enable);
		current_status = -1;
		return -EFAULT;
	}

	data->ps_open_state = enable;
	current_status = enable;
	pr_err("%s: enable=(%d), data->ps_open_state=%d\n", __func__, enable, data->ps_open_state);
	return ret;
}
static int ltr553_ps_set_enable(struct sensors_classdev *sensors_cdev, unsigned int enable);

static int ltr553_suspend(struct device *dev)
{
	struct ltr553_data *data = dev_get_drvdata(dev);
	int ret = 0;
	mutex_lock(&data->lockw);

#ifdef USE_SUSPEND_POWEROFF
	if (data->ps_open_state == 1) {

	} else {
		disable_irq_nosync(data->irq);
		ret |= sensor_platform_hw_power_onoff(data, false);
	}
#else
	ret |= ltr553_als_enable(data->client, 0);
#endif
	mutex_unlock(&data->lockw);
	return ret;
}

static int ltr553_resume(struct device *dev)
{
	struct ltr553_data *data = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&data->lockw);
#ifdef USE_SUSPEND_POWEROFF
	if (data->ps_open_state == 1) {

	} else {
		ret = sensor_platform_hw_power_onoff(data, true);
		ret |= ltr553_device_init(data->client);
		enable_irq(data->irq);
	}
#else
	if (data->als_open_state == 1)
		ret = ltr553_als_enable(data->client, 1);
#endif
	mutex_unlock(&data->lockw);
	return ret;
}

static int ltr553_check_chip_id(struct i2c_client *client)
{
	int id;

	id = i2c_smbus_read_byte_data(client, LTR553_MANUFACTURER_ID);
	if (id != LTR553_MANUFAC_ID)
		return -EINVAL;
	return 0;
}

int ltr553_device_init(struct i2c_client *client)
{
	int retval = 0;
	int i;

	retval = i2c_smbus_write_byte_data(client, LTR553_ALS_CONTR, 0x02);
	if (retval < 0)
		printk("%s   i2c_smbus_write_byte_data(LTR553_ALS_CONTR, 0x02);  ERROR !!!.\n", __func__);

	msleep(WAKEUP_DELAY);
	for (i = 2; i < sizeof(reg_tbl)/sizeof(reg_tbl[0]); i++) {
		if (reg_tbl[i].name == NULL || reg_tbl[i].addr == 0)
				break;
		if (reg_tbl[i].defval != reg_tbl[i].curval) {
			if (i < 10) {
				retval = i2c_smbus_write_byte_data(client, reg_tbl[i].addr, reg_tbl[i].curval);
			} else {
				retval = i2c_smbus_write_byte_data(client, reg_tbl[i].addr, reg_tbl[i].curval & 0xff);
				retval = i2c_smbus_write_byte_data(client, reg_tbl[i].addr + 1, reg_tbl[i].curval >> 8);
			}
		}
	}

	return retval;
}

int ltr553_ps_reg_init(struct i2c_client *client)
{
	int retval = 0;
	int i;

	for (i = 0; i < sizeof(ps_reg_tbl)/sizeof(ps_reg_tbl[0]); i++) {
		if (ps_reg_tbl[i].name == NULL || ps_reg_tbl[i].addr == 0)
				break;
		retval = i2c_smbus_write_byte_data(client, ps_reg_tbl[i].addr, ps_reg_tbl[i].curval);
	}
	msleep(WAKEUP_DELAY);
	return 0;
}

static int sensor_regulator_configure(struct ltr553_data *data, bool on)
{
	int rc;

	if (!on) {
		if (regulator_count_voltages(data->vdd) > 0)
			regulator_set_voltage(data->vdd, 0, LTR553_VDD_MAX_UV);
		regulator_put(data->vdd);

		if (regulator_count_voltages(data->vio) > 0)
			regulator_set_voltage(data->vio, 0, LTR553_VIO_MAX_UV);
		regulator_put(data->vio);
	} else {
		data->vdd = regulator_get(&data->client->dev, "vdd");
		if (IS_ERR(data->vdd)) {
			rc = PTR_ERR(data->vdd);
			dev_err(&data->client->dev, "Regulator get failed vdd rc=%d\n", rc);
			return rc;
		}

		if (regulator_count_voltages(data->vdd) > 0) {
			rc = regulator_set_voltage(data->vdd, LTR553_VDD_MIN_UV, LTR553_VDD_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev, "Regulator set failed vdd rc=%d\n", rc);
				goto reg_vdd_put;
			}
		}

		data->vio = regulator_get(&data->client->dev, "vio");
		if (IS_ERR(data->vio)) {
			rc = PTR_ERR(data->vio);
			dev_err(&data->client->dev, "Regulator get failed vio rc=%d\n", rc);
			goto reg_vdd_set;
		}

		if (regulator_count_voltages(data->vio) > 0) {
			rc = regulator_set_voltage(data->vio, LTR553_VIO_MIN_UV, LTR553_VIO_MAX_UV);
			if (rc) {
				dev_err(&data->client->dev, "Regulator set failed vio rc=%d\n", rc);
				goto reg_vio_put;
			}
		}
	}

	return 0;
reg_vio_put:
	regulator_put(data->vio);

reg_vdd_set:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, LTR553_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;
}

static int sensor_regulator_power_on(struct ltr553_data *data, bool on)
{
	int rc = 0;
	if (!on) {
		rc = regulator_disable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev, "Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(data->vio);
		if (rc) {
			dev_err(&data->client->dev, "Regulator vio disable failed rc=%d\n", rc);
			rc = regulator_enable(data->vdd);
			dev_err(&data->client->dev, "Regulator vio re-enabled rc=%d\n", rc);
			/*
			 * Successfully re-enable regulator.
			 * Enter poweron delay and returns error.
			 */
			if (!rc) {
				rc = -EBUSY;
				goto enable_delay;
			}
		}
		return rc;
	} else {
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev, "Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}
		rc = regulator_enable(data->vio);
		if (rc) {
			dev_err(&data->client->dev, "Regulator vio enable failed rc=%d\n", rc);
			regulator_disable(data->vdd);
			return rc;
		}
	}

enable_delay:
	msleep(130);
	dev_dbg(&data->client->dev, "Sensor regulator power on =%d\n", on);
	return rc;
}

static int sensor_platform_hw_power_onoff(struct ltr553_data *data, bool on)
{
	int err = 0;

	if (data->power_state != on) {
		if (on) {
			err = sensor_regulator_configure(data, true);
			if (err) {
				dev_err(&data->client->dev, "unable to configure regulator on=%d\n", on);
				goto power_out;
			}

			err = sensor_regulator_power_on(data, true);
			if (err) {
				dev_err(&data->client->dev, "Can't configure regulator on=%d\n", on);
				goto power_out;
			}

			data->power_state = true;
		} else {
			err = sensor_regulator_power_on(data, false);
			if (err) {
				dev_err(&data->client->dev, "Can't configure regulator on=%d\n", on);
				goto power_out;
			}

			err = sensor_regulator_configure(data, false);
			if (err) {
				dev_err(&data->client->dev, "unable to configure regulator on=%d\n", on);
				goto power_out;
			}
			data->power_state = false;
		}
	}
power_out:
	return err;
}

static int ltr553_parse_dt(struct device *dev, struct ltr553_data *data)
{
	struct ltr553_platform_data *pdata = data->platform_data;
	struct device_node *np = dev->of_node;
	unsigned int tmp;
	int rc = 0;

	/* ps tuning data*/
	rc = of_property_read_u32(np, "ltr,ps-threshold", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read ps threshold\n");
		return rc;
	}
	pdata->prox_threshold = tmp;

	rc = of_property_read_u32(np, "ltr,ps-hysteresis-threshold", &tmp);
	 if (rc) {
		dev_err(dev, "Unable to read ps hysteresis threshold\n");
		return rc;
	}
	pdata->prox_hsyteresis_threshold = tmp;
	rc = of_property_read_u32(np, "ltr,als-polling-time", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read ps hysteresis threshold\n");
		return rc;
	}
	pdata->als_poll_interval = tmp;
	return 0;
}

/* PS open fops */
ssize_t ps_open(struct inode *inode, struct file *file)
{
	file->private_data = sensor_info;
	return nonseekable_open(inode, file);
}

/* PS release fops */
ssize_t ps_release(struct inode *inode, struct file *file)
{
	return 0;
}

/* PS IOCTL */
static long ps_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc = 0, val = 0, lux = 0, enable = 0;
	int value;
	int psval_lo, psval_hi;
	struct ltr553_data *ltr553 = sensor_info;

	switch (cmd) {
	case ALSPS_REC_PS_DATA_FOR_CALI:
#if PS_AUTO_CALIBRATION
#else
		if (get_user(val, (unsigned long __user *)arg)) {
			rc = -EFAULT;
			break;
		}
		if (val == 0)
			val = 300;
		ltr553->platform_data->prox_hsyteresis_threshold = val + FAR_THRES_DATA;
		ltr553->platform_data->prox_threshold = val + NEAR_THRES_DATA;
		ltr553_set_ps_threshold(ltr553->client, LTR553_PS_THRES_LOW_0, ltr553->platform_data->prox_hsyteresis_threshold);
		ltr553_set_ps_threshold(ltr553->client, LTR553_PS_THRES_UP_0, ltr553->platform_data->prox_threshold);
#endif
		break;
	case ALSPS_GET_PS_RAW_DATA_FOR_CALI:
		psval_lo = i2c_smbus_read_byte_data(ltr553->client, LTR553_PS_DATA_0);
		if (psval_lo < 0)
			return psval_lo;
		psval_hi = i2c_smbus_read_byte_data(ltr553->client, LTR553_PS_DATA_1);
		if (psval_hi < 0)
			return psval_hi;
		value = ((psval_hi & 7) << 8) | psval_lo;
		put_user(value, (unsigned long __user *)arg);
		break;
	case ALSPS_LSENSOR_LUX_DATA:
		lux = ltr553_als_read(ltr553->client);
		put_user(lux, (unsigned long __user *)arg);
		break;
	case ALSPS_PSENSOR_ABS_DISTANCE_DATA:
		put_user(ltr553->distance_flag, (unsigned long __user *)arg);
		break;

	case ALSPS_PS_ENABLE:
		if (get_user(enable, (unsigned long __user *)arg)) {
			rc = -EFAULT;
			break;
		}
		ltr553_ps_set_enable(&ltr553->ps_cdev, (unsigned int)enable);
		break;

	case ALSPS_LS_ENABLE:
		if (get_user(enable, (unsigned long __user *)arg)) {
			rc = -EFAULT;
			break;
		}
		ltr553_als_set_enable(&ltr553->als_cdev, (unsigned int)enable);
		ltr553->fastmmi_ls_flag = 1;
		break;
	default:
		pr_err("%s: INVALID COMMAND %d\n",
			__func__, _IOC_NR(cmd));
		rc = -EINVAL;
	}
		return 0;
}

static const struct file_operations ps_fops = {
	.owner = THIS_MODULE,
	.open = ps_open,
	.release = ps_release,
	.unlocked_ioctl = ps_ioctl
};

struct miscdevice ps_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "psensor",
	.fops = &ps_fops
};

static int ps_enabled;
static int camera_opened;
static int ltr553_ps_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	int ret = 0;
	ps_enabled = enable;
	if (!camera_opened)
		ret = ltr553_ps_set_enable_real(sensors_cdev, enable);
	else
		pr_info("camera is opened , omit ps enable\n");
	return ret;
}

static void camera_callback_func(struct work_struct *work)
{
	if (!camera_opened) {
		if (ps_enabled)
			ltr553_ps_set_enable_real(&sensor_info->ps_cdev, 1);
	} else {
		if (ps_enabled)
			ltr553_ps_set_enable_real(&sensor_info->ps_cdev, 0);
	}
}

static DECLARE_DELAYED_WORK(camera_callback_work, camera_callback_func);

extern void (*msm_sensor_power_on)(int up_down) ;
void msm_sensor_camera_power_on(int power_up)
{
	cancel_delayed_work_sync(&camera_callback_work);
	if (power_up) {
		camera_opened = 1;
		schedule_delayed_work(&camera_callback_work, 10);
	} else {
		camera_opened = 0;
		schedule_delayed_work(&camera_callback_work, 2*HZ);
	}
}


int ltr553_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct ltr553_data *data;
	struct ltr553_platform_data *pdata;
	int ret = 0;

	/* check i2c*/
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WRITE_BYTE | I2C_FUNC_SMBUS_READ_BYTE_DATA)) {
		dev_err(&client->dev, "LTR-553ALS functionality check failed.\n");
		return -EIO;
	}

	/* platform data memory allocation*/
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(struct ltr553_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
	} else {
		pdata = client->dev.platform_data;
		if (!pdata) {
			dev_err(&client->dev, "No platform data\n");
			return -ENODEV;
		}
	}

	/* data memory allocation */
	data = kzalloc(sizeof(struct ltr553_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "kzalloc failed\n");
		ret = -ENOMEM;
		goto exit_kfree_pdata;
	}
	data->client = client;
	data->platform_data = pdata;
	data->distance_flag = 1;
	data->fastmmi_ls_flag = 0;
	sensor_info = data;
	ret = ltr553_parse_dt(&client->dev, data);
	if (ret) {
		dev_err(&client->dev, "can't parse platform data\n");
		ret = -EFAULT;
		goto exit_kfree_data;
	}

	/* power initialization */
	ret = sensor_platform_hw_power_onoff(data, true);
	if (ret) {
		ret = -ENOEXEC;
		dev_err(&client->dev, "power on fail\n");
		goto exit_kfree_data;
	}

	/* set client data as ltr553_data*/
	i2c_set_clientdata(client, data);

	ret = ltr553_check_chip_id(client);
	if (ret) {
		ret = -ENXIO;
		dev_err(&client->dev, "the manufacture id is not match\n");
		goto exit_power_off;
	}

	ret = ltr553_device_init(client);
	if (ret) {
		ret = -ENXIO;
		dev_err(&client->dev, "device init failed\n");
		goto exit_power_off;
	}

	/* request gpio and irq */
	ret = ltr553_gpio_irq(data);
	if (ret) {
		ret = -ENXIO;
		dev_err(&client->dev, "gpio_irq failed\n");
		goto exit_chip_reset;
	}

	/* Register Input Device */
	data->input_dev_als = input_allocate_device();
	if (!data->input_dev_als) {
		ret = -ENOMEM;
		dev_err(&client->dev, "Failed to allocate input device als\n");
		goto exit_free_irq;
	}

	data->input_dev_ps = input_allocate_device();
	if (!data->input_dev_ps) {
		ret = -ENOMEM;
		dev_err(&client->dev, "Failed to allocate input device ps\n");
		goto exit_free_dev_als;
	}

	set_bit(EV_ABS, data->input_dev_als->evbit);
	set_bit(EV_ABS, data->input_dev_ps->evbit);
	input_set_abs_params(data->input_dev_als, ABS_MISC, 0, 65535, 0, 0);
	input_set_abs_params(data->input_dev_ps, ABS_DISTANCE, 0, 1, 0, 0);

	data->input_dev_als->name = "light";
	data->input_dev_ps->name = "proximity";
	data->input_dev_als->id.bustype = BUS_I2C;
	data->input_dev_als->dev.parent = &data->client->dev;
	data->input_dev_ps->id.bustype = BUS_I2C;
	data->input_dev_ps->dev.parent = &data->client->dev;

	input_set_drvdata(data->input_dev_als, data);
	input_set_drvdata(data->input_dev_ps, data);

	ret = input_register_device(data->input_dev_als);
	if (ret) {
		ret = -ENOMEM;
		dev_err(&client->dev, "Unable to register input device als: %s\n", data->input_dev_als->name);
		goto exit_free_dev_ps;
	}

	ret = input_register_device(data->input_dev_ps);
	if (ret) {
		ret = -ENOMEM;
		dev_err(&client->dev, "Unable to register input device ps: %s\n", data->input_dev_ps->name);
		goto exit_unregister_dev_als;
	}
	ret = misc_register(&ps_misc);
	if (ret < 0) {
		dev_err(&data->client->dev,
		"%s: PS Register Misc Device Fail...\n", __func__);
		goto  exit_free_dev_als;
	}
	/* init delayed works */
	INIT_DELAYED_WORK(&data->ps_work, ltr553_ps_work_func);
	INIT_DELAYED_WORK(&data->als_work, ltr553_als_work_func);

	/* init mutex */
	mutex_init(&data->lockw);
	mutex_init(&data->op_lock);

	/* create sysfs group */
	ret = sysfs_create_group(&client->dev.kobj, &ltr553_attr_group);
	if (ret) {
		ret = -EROFS;
		dev_err(&client->dev, "Unable to creat sysfs group\n");
		goto exit_unregister_dev_ps;
	}

	wake_lock_init(&data->ltr553_wake_lock, WAKE_LOCK_SUSPEND, "ps_wakelock");
	/* Register sensors class */
	data->als_cdev = sensors_light_cdev;
	data->als_cdev.sensors_enable = ltr553_als_set_enable;
	data->als_cdev.sensors_poll_delay = ltr553_als_poll_delay;
	data->ps_cdev = sensors_proximity_cdev;
	data->ps_cdev.sensors_enable = ltr553_ps_set_enable;
	data->ps_cdev.sensors_poll_delay = NULL;

	ret = sensors_classdev_register(&client->dev, &data->als_cdev);
	if (ret) {
		ret = -EROFS;
		dev_err(&client->dev, "Unable to register to als sensor class\n");
		goto exit_remove_sysfs_group;
	}

	ret = sensors_classdev_register(&client->dev, &data->ps_cdev);
	if (ret) {
		ret = -EROFS;
		dev_err(&client->dev, "Unable to register to ps sensor class\n");
		goto exit_unregister_als_class;
	}

	hardwareinfo_set_prop(HARDWARE_ALSPS, "ltr553");
	dev_dbg(&client->dev, "probe succece\n");

	msm_sensor_power_on = msm_sensor_camera_power_on;
	return 0;

exit_unregister_als_class:
	sensors_classdev_unregister(&data->als_cdev);
exit_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &ltr553_attr_group);
exit_unregister_dev_ps:
	input_unregister_device(data->input_dev_ps);
exit_unregister_dev_als:
	input_unregister_device(data->input_dev_als);
exit_free_dev_ps:
	if (data->input_dev_ps)
			input_free_device(data->input_dev_ps);
exit_free_dev_als:
	if (data->input_dev_als)
			input_free_device(data->input_dev_als);
exit_free_irq:
	ltr553_gpio_irq_free(data);
exit_chip_reset:
	ltr553_chip_reset(client);
exit_power_off:
	sensor_platform_hw_power_onoff(data, false);
	hardwareinfo_set_prop(HARDWARE_ALSPS, "ltr553 fail");
exit_kfree_data:
	kfree(data);
exit_kfree_pdata:
	if (pdata && (client->dev.of_node))
		devm_kfree(&client->dev, pdata);
	data->platform_data = NULL;

	return ret;
}

static int ltr553_remove(struct i2c_client *client)
{
	struct ltr553_data *data = i2c_get_clientdata(client);
	struct ltr553_platform_data *pdata = data->platform_data;

	if (data == NULL || pdata == NULL)
		return 0;

	ltr553_ps_enable(client, 0);
	ltr553_als_enable(client, 0);
	input_unregister_device(data->input_dev_als);
	input_unregister_device(data->input_dev_ps);
	input_free_device(data->input_dev_als);
	input_free_device(data->input_dev_ps);
	ltr553_gpio_irq_free(data);
	sysfs_remove_group(&client->dev.kobj, &ltr553_attr_group);
	cancel_delayed_work_sync(&data->ps_work);
	cancel_delayed_work_sync(&data->als_work);

	if (pdata && (client->dev.of_node))
		devm_kfree(&client->dev, pdata);
	pdata = NULL;

	kfree(data);
	data = NULL;
	return 0;
}

static struct i2c_device_id ltr553_id[] = {
		{"ltr553", 0},
		{}
};

static struct of_device_id ltr_match_table[] = {
		{ .compatible = "ltr,ltr553",},
		{ },
};

MODULE_DEVICE_TABLE(i2c, ltr553_id);
static SIMPLE_DEV_PM_OPS(ltr553_pm_ops, ltr553_suspend, ltr553_resume);
static struct i2c_driver ltr553_driver = {
		.driver = {
				.name = LTR553_DRV_NAME,
				.owner = THIS_MODULE,
				.pm = &ltr553_pm_ops,
				.of_match_table = ltr_match_table,
		},
		.probe = ltr553_probe,
		.remove = ltr553_remove,
		.id_table = ltr553_id,
};

static int ltr553_driver_init(void)
{
		pr_info("Driver ltr5530 init.\n");
		return i2c_add_driver(&ltr553_driver);
};

static void ltr553_driver_exit(void)
{
		pr_info("Unload ltr553 module...\n");
		i2c_del_driver(&ltr553_driver);
}

module_init(ltr553_driver_init);
module_exit(ltr553_driver_exit);
MODULE_AUTHOR("Lite-On Technology Corp.");
MODULE_DESCRIPTION("Lite-On LTR-553 Proximity and Light Sensor Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
