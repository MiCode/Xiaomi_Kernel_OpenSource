/* drivers/i2c/chips/epl259x.c - light and proxmity sensors driver
 * Copyright (C) 2014 ELAN Corporation.
 * Copyright (C) 2016 XiaoMi, Inc.
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


#include <linux/hrtimer.h>
#include <linux/timer.h>
#include <linux/delay.h>
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include <asm/setup.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include "epl259x.h"
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/sensors.h>

#include <linux/hardware_info.h>
/******************************************************************************
* configuration
*******************************************************************************/
#define ALS_POLLING_MODE		 	1
#define PS_POLLING_MODE		 	0

#define ALS_LOW_THRESHOLD		1000
#define ALS_HIGH_THRESHOLD		3000

#define PS_LOW_THRESHOLD		500
#define PS_HIGH_THRESHOLD		800

#define LUX_PER_COUNT			700


#define S5PV210	 0
#define SPREAD	  0
#define QCOM		1
#define LEADCORE	0
#define MARVELL	 0

#define ELAN_INT_PIN 129	/*Interrupt pin setting*/


#define COMMON_DEBUG 0
#define ALS_DEBUG   0
#define PS_DEBUG	0
#define SHOW_DBG	1

#define ALS_DYN_INTT	0
#define PS_DYN_K		1
#define PS_DYN_K_STR	0
#define HS_ENABLE	   0
#define PS_GES		  0

#define SENSOR_CLASS	1

#define ALS_LEVEL	16
static int polling_time = 200;

static int als_level[] = {20, 45, 70, 90, 150, 300, 500, 700, 1150, 2250, 4500, 8000, 15000, 30000, 50000};
static int als_value[] = {10, 30, 60, 80, 100, 200, 400, 600, 800, 1500, 3000, 6000, 10000, 20000, 40000, 60000};

#if ALS_DYN_INTT

int dynamic_intt_idx;
int dynamic_intt_init_idx = 1;
int c_gain;
int dynamic_intt_lux = 0;

uint16_t dynamic_intt_high_thr;
uint16_t dynamic_intt_low_thr;
uint32_t dynamic_intt_max_lux = 12000;
uint32_t dynamic_intt_min_lux = 0;
uint32_t dynamic_intt_min_unit = 1000;

static int als_dynamic_intt_intt[] = {EPL_ALS_INTT_8192, EPL_ALS_INTT_64};
static int als_dynamic_intt_value[] = {8192, 64};
static int als_dynamic_intt_gain[] = {EPL_GAIN_MID, EPL_GAIN_MID};
static int als_dynamic_intt_high_thr[] = {60000, 53000};
static int als_dynamic_intt_low_thr[] = {200, 200};
static int als_dynamic_intt_intt_num =  sizeof(als_dynamic_intt_value)/sizeof(int);
#endif

#if ALS_DYN_INTT
typedef enum {
	CMC_BIT_LSRC_NON		= 0x0,
	CMC_BIT_LSRC_SCALE	 	= 0x1,
	CMC_BIT_LSRC_SLOPE		= 0x2,
	CMC_BIT_LSRC_BOTH		= 0x3,
} CMC_LSRC_REPORT_TYPE;
#endif

#if PS_DYN_K
static int dynk_polling_delay = 200;
int dynk_min_ps_raw_data;
int dynk_max_ir_data;

u32 dynk_thd_low = 0;
u32 dynk_thd_high = 0;

int dynk_low_offset;
int dynk_high_offset;

bool dynk_change_flag = false;
u16 dynk_change_thd_max;
u16 dynk_thd_offset;

u8 dynk_last_status = 0;

#if PS_DYN_K_STR
bool dynk_enhance_flag = true;
u8 dynk_enhance_integration_time;
u8 dynk_enhance_gain;
u8 dynk_enhance_adc;
u16 dynk_enhance_ch0;
u16 dynk_enhance_ch1;
u16 dynk_enhance_max_ch0;
#endif

#endif

#if HS_ENABLE
static struct mutex hs_sensor_mutex;
static bool hs_enable_flag;
#endif

#if PS_GES
static bool ps_ges_enable_flag;
u16 ges_threshold_low = 1000;
u16 ges_threshold_high = 1500;
#define KEYCODE_LEFT			KEY_LEFTALT
bool ps_ges_suspend_flag = false;
#endif

bool polling_flag = true;
bool eint_flag = true;


static const char ps_cal_file[] = "/data/data/com.eminent.ps.calibration/ps.dat";
static const char als_cal_file[] = "/data/data/com.eminent.ps.calibration/als.dat";

static int PS_h_offset = 510;
static int PS_l_offset = 160;
static int PS_MAX_XTALK = 1000;

int als_frame_time = 0;
int ps_frame_time = 0;
/******************************************************************************
*******************************************************************************/

#define TXBYTES							 2
#define RXBYTES							 2

#define PACKAGE_SIZE 			8
#define I2C_RETRY_COUNT 		10
#define EPL_DEV_NAME   			"EPL259x"
#define DRIVER_VERSION		  "1.0.8"

typedef enum {
	CMC_BIT_RAW   			= 0x0,
	CMC_BIT_PRE_COUNT	 	= 0x1,
	CMC_BIT_DYN_INT			= 0x2,
	CMC_BIT_DEF_LIGHT		= 0x4,
	CMC_BIT_TABLE			= 0x8,
} CMC_ALS_REPORT_TYPE;

typedef struct _epl_raw_data {
	u8 raw_bytes[PACKAGE_SIZE];
	u16 renvo;
} epl_raw_data;

struct epl_sensor_priv {
	struct i2c_client *client;
	struct input_dev *als_input_dev;
	struct input_dev *ps_input_dev;
	struct delayed_work  eint_work;
	struct delayed_work  polling_work;
#if PS_DYN_K
	struct delayed_work  dynk_thd_polling_work;
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	int intr_pin;
	int (*power)(int on);

	int ps_opened;
	int als_opened;

	int als_suspend;
	int ps_suspend;

	int lux_per_count;

	int enable_pflag;
	int enable_lflag;
#if HS_ENABLE
	int enable_hflag;
	int hs_suspend;
#endif
#if PS_GES
	struct input_dev *gs_input_dev;
	int enable_gflag;
	int ges_suspend;
#endif
	int read_flag;
	int irq;
	spinlock_t lock;

#if ALS_DYN_INTT
	uint32_t ratio;
	uint32_t last_ratio;
	int c_gain_h;
	int c_gain_l;
	uint32_t lsource_thd_high;
	uint32_t lsource_thd_low;
#endif
#if SENSOR_CLASS
	struct sensors_classdev	als_cdev;
	struct sensors_classdev	ps_cdev;
	int			flush_count;
#endif
	/*data*/
	u16		 	als_level_num;
	u16		 	als_value_num;
	u32		 	als_level[ALS_LEVEL-1];
	u32		 	als_value[ALS_LEVEL];
} ;

static struct platform_device *sensor_dev;
struct epl_sensor_priv *epl_sensor_obj;
static epl_optical_sensor epl_sensor;
int i2c_max_count = 8;

static epl_raw_data	gRawData;

static struct wake_lock ps_lock;
static struct mutex sensor_mutex;

#if S5PV210
static const char ElanPsensorName[] = "proximity";
static const char ElanALsensorName[] = "lightsensor-level";
#elif SPREAD
static const char ElanPsensorName[] = "light sensor";
#elif QCOM || LEADCORE
static const char ElanPsensorName[] = "proximity";
static const char ElanALsensorName[] = "light";
#elif MARVELL
static const char ElanPsensorName[] = "alps_pxy";
#endif

#define LOG_TAG					  "[EPL259x] "
#define LOG_FUN(f)			   	 printk(KERN_INFO LOG_TAG"%s\n", __FUNCTION__)
#define LOG_INFO(fmt, args...)		 printk(KERN_INFO LOG_TAG fmt, ##args)
#define LOG_ERR(fmt, args...)   	 printk(KERN_ERR  LOG_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)

#if SENSOR_CLASS
#define PS_MIN_POLLING_RATE	200
#define ALS_MIN_POLLING_RATE	200

static struct sensors_classdev als_cdev = {
	.name = "epl259x-light",
	.vendor = "Eminent Technology Corp",
	.version = 1,
	.handle = SENSORS_LIGHT_HANDLE,
	.type = SENSOR_TYPE_LIGHT,
	.max_range = "65536",
	.resolution = "1.0",
	.sensor_power = "0.25",
	.min_delay = 50000,
	.max_delay = 2000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.flags = 2,
	.enabled = 0,
	.delay_msec = 50,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static struct sensors_classdev ps_cdev = {
	.name = "epl259x-proximity",
	.vendor = "Eminent Technology Corp",
	.version = 1,
	.handle = SENSORS_PROXIMITY_HANDLE,
	.type = SENSOR_TYPE_PROXIMITY,
	.max_range = "7",
	.resolution = "5.0",
	.sensor_power = "0.25",
	.min_delay = 10000,
	.max_delay = 2000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.flags = 3,
	.enabled = 0,
	.delay_msec = 50,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};
#endif

void epl_sensor_update_mode(struct i2c_client *client);
int epl_sensor_read_als_status(struct i2c_client *client);
static int epl_sensor_setup_interrupt(struct epl_sensor_priv *epld);
static int ps_sensing_time(int intt, int adc, int cycle);
static int als_sensing_time(int intt, int adc, int cycle);

static void epl_sensor_eint_work(struct work_struct *work);


static void epl_sensor_polling_work(struct work_struct *work);


#if PS_DYN_K
void epl_sensor_dynk_thd_polling_work(struct work_struct *work);
void epl_sensor_restart_dynk_polling(void);

#endif


bool low_trigger_flag;

/*

*/
static int epl_sensor_I2C_Write_Cmd(struct i2c_client *client, uint8_t regaddr, uint8_t data, uint8_t txbyte)
{
	uint8_t buffer[2];
	int ret = 0;
	int retry;

	buffer[0] = regaddr ;
	buffer[1] = data;

	for (retry = 0; retry < I2C_RETRY_COUNT; retry++) {
		ret = i2c_master_send(client, buffer, txbyte);

		if (ret == txbyte) {
			break;
		}

		LOG_ERR("i2c write error, TXBYTES %d\n", ret);
		mdelay(10);
	}


	if (retry >= I2C_RETRY_COUNT) {
		LOG_ERR("i2c write retry over %d\n", I2C_RETRY_COUNT);
		return -EINVAL;
	}

	return ret;
}

static int epl_sensor_I2C_Write(struct i2c_client *client, uint8_t regaddr, uint8_t data)
{
	int ret = 0;
	ret = epl_sensor_I2C_Write_Cmd(client, regaddr, data, 0x02);
	return ret;
	return 0;
}
/*----------------------------------------------------------------------------*/
static int epl_sensor_I2C_Read(struct i2c_client *client, uint8_t regaddr, uint8_t bytecount)
{

	int ret = 0;


	int retry;
	int read_count = 0, rx_count = 0;

	while (bytecount > 0) {
		epl_sensor_I2C_Write_Cmd(client, regaddr+read_count, 0x00, 0x01);

		for (retry = 0; retry < I2C_RETRY_COUNT; retry++) {
			rx_count = bytecount > i2c_max_count?i2c_max_count:bytecount;
			ret = i2c_master_recv(client, &gRawData.raw_bytes[read_count], rx_count);

			if (ret == rx_count)
				break;

			LOG_ERR("i2c read error, RXBYTES %d\r\n", ret);
			mdelay(10);
		}

		if (retry >= I2C_RETRY_COUNT) {
			LOG_ERR("i2c read retry over %d\n", I2C_RETRY_COUNT);
			return -EINVAL;
		}
		bytecount -= rx_count;
		read_count += rx_count;
	}

	return ret;
}

/*----------------------------------------------------------------------------*/
static void epl_sensor_restart_polling(void)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	cancel_delayed_work(&epld->polling_work);
	schedule_delayed_work(&epld->polling_work, msecs_to_jiffies(50));

}
/*----------------------------------------------------------------------------*/

#if PS_GES
static void epl_sensor_notify_event(void)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	struct input_dev *idev = epld->gs_input_dev;

	LOG_INFO("  --> LEFT\n\n");
	input_report_key(idev, KEYCODE_LEFT, 1);
	input_report_key(idev, KEYCODE_LEFT, 0);
	input_sync(idev);
}
#endif

/*----------------------------------------------------------------------------*/
static void epl_sensor_report_lux(int report_lux)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
#if ALS_DEBUG
	LOG_INFO("-------------------  ALS raw = %d, lux = %d\n\n", epl_sensor.als.data.channels[1], report_lux);
#endif

#if SPREAD || MARVELL
	input_report_abs(epld->ps_input_dev, ABS_MISC, report_lux);
	input_sync(epld->ps_input_dev);
#else
	input_report_abs(epld->als_input_dev, ABS_MISC, report_lux);
	input_sync(epld->als_input_dev);
#endif
}
/*----------------------------------------------------------------------------*/
#if ALS_DYN_INTT
long raw_convert_to_lux(u16 raw_data)
{
	long lux = 0;
	long dyn_intt_raw = 0;

	dyn_intt_raw = (raw_data * 10) / (10*als_dynamic_intt_value[dynamic_intt_idx] / als_dynamic_intt_value[1]);

	LOG_INFO("[%s]: dyn_intt_raw=%ld \r\n", __func__, dyn_intt_raw);

	if (dyn_intt_raw > 0xffff)
		epl_sensor.als.dyn_intt_raw = 0xffff;
	else
		epl_sensor.als.dyn_intt_raw = dyn_intt_raw;

	lux = c_gain * epl_sensor.als.dyn_intt_raw;

#if ALS_DEBUG
	LOG_INFO("[%s]:raw_data=%d, epl_sensor.als.dyn_intt_raw=%d, lux=%ld\r\n", __func__, raw_data, epl_sensor.als.dyn_intt_raw, lux);
#endif

	if (lux >= (dynamic_intt_max_lux * dynamic_intt_min_unit)) {
#if ALS_DEBUG
		LOG_INFO("[%s]:raw_convert_to_lux: change max lux\r\n", __func__);
#endif
		lux = dynamic_intt_max_lux * dynamic_intt_min_unit;
	} else if (lux <= (dynamic_intt_min_lux*dynamic_intt_min_unit)) {
#if ALS_DEBUG
		LOG_INFO("[%s]:raw_convert_to_lux: change min lux\r\n", __func__);
#endif
		lux = dynamic_intt_min_lux * dynamic_intt_min_unit;
	}

	return lux;
}
#endif
/*----------------------------------------------------------------------------*/
static int epl_sensor_get_als_value(struct epl_sensor_priv *obj, u16 als)
{
	int idx;
	int invalid = 0;
#if ALS_DYN_INTT
	long now_lux = 0, lux_tmp = 0;
	bool change_flag = false;
#endif
	switch (epl_sensor.als.report_type) {
	case CMC_BIT_RAW:
		return als;
		break;

	case CMC_BIT_PRE_COUNT:
		return (als * epl_sensor.als.factory.lux_per_count)/1000;
		break;

	case CMC_BIT_TABLE:
		for (idx = 0; idx < obj->als_level_num; idx++) {
			if (als < als_level[idx]) {
				break;
			}
		}

		if (idx >= obj->als_value_num) {
			LOG_ERR("exceed range\n");
			idx = obj->als_value_num - 1;
		}

		if (!invalid) {
			LOG_INFO("ALS: %05d => %05d\n", als, als_value[idx]);
			return als_value[idx];
		} else {
			LOG_ERR("ALS: %05d => %05d (-1)\n", als, als_value[idx]);
			return als;
		}
		break;
#if ALS_DYN_INTT
	case CMC_BIT_DYN_INT:

		if (epl_sensor.als.lsrc_type != CMC_BIT_LSRC_NON) {
			long luxratio = 0;
			epl_sensor_read_als_status(obj->client);

			if (epl_sensor.als.data.channels[0] == 0) {
			   epl_sensor.als.data.channels[0] = 1;
			   LOG_ERR("[%s]:read ch0 data is 0 \r\n", __func__);
			}

			luxratio = (long)((als*dynamic_intt_min_unit) / epl_sensor.als.data.channels[0]);

			obj->ratio = luxratio;
			if ((epl_sensor.als.saturation >> 5) == 0) {
				if (epl_sensor.als.lsrc_type == CMC_BIT_LSRC_SCALE || epl_sensor.als.lsrc_type == CMC_BIT_LSRC_BOTH) {
					if (obj->ratio == 0) {
						obj->last_ratio = luxratio;
					} else {
						obj->last_ratio = (luxratio + obj->last_ratio*9)  / 10;
					}

					if (obj->last_ratio >= obj->lsource_thd_high) {
						c_gain = obj->c_gain_h;
					} else if (obj->last_ratio <= obj->lsource_thd_low) {
						c_gain = obj->c_gain_l;
					} else if (epl_sensor.als.lsrc_type == CMC_BIT_LSRC_BOTH) {
						int a = 0, b = 0, c = 0;
						a = (obj->c_gain_h - obj->c_gain_l) * dynamic_intt_min_unit / (obj->lsource_thd_high - obj->lsource_thd_low);
						b = (obj->c_gain_h) - ((a * obj->lsource_thd_high)/dynamic_intt_min_unit);
						c  = ((a * obj->last_ratio)/dynamic_intt_min_unit) + b;
						if (c > obj->c_gain_h)
							c_gain = obj->c_gain_h;
						else if (c < obj->c_gain_l)
							c_gain = obj->c_gain_l;
						else
							c_gain = c;
					}
				} else if (epl_sensor.als.lsrc_type == CMC_BIT_LSRC_SLOPE) {
					if (luxratio >= obj->lsource_thd_high) {
						c_gain = obj->c_gain_h;
						} else if (luxratio <= obj->lsource_thd_low) {
							c_gain = obj->c_gain_l;
						} else {/*mix*/
							int a = 0, b = 0, c = 0;
							a = (obj->c_gain_h - obj->c_gain_l) * dynamic_intt_min_unit / (obj->lsource_thd_high - obj->lsource_thd_low);
							b = (obj->c_gain_h) - ((a * obj->lsource_thd_high)/dynamic_intt_min_unit);
							c = ((a * luxratio)/dynamic_intt_min_unit) + b;
							if (c > obj->c_gain_h)
								c_gain = obj->c_gain_h;
							else if (c < obj->c_gain_l)
								c_gain = obj->c_gain_l;
							else
								c_gain = c;
						}
					}
					LOG_INFO("[%s]:ch0=%d, ch1=%d, c_gain=%d, obj->ratio=%d, obj->last_ratio=%d \r\n\n",
									  __func__, epl_sensor.als.data.channels[0], als, c_gain, obj->ratio, obj->last_ratio);
				} else {
					LOG_INFO("[%s]: ALS saturation(%d) \r\n", __func__, (epl_sensor.als.saturation >> 5));
				}
			}

#if ALS_DEBUG
			LOG_INFO("[%s]: dynamic_intt_idx=%d, als_dynamic_intt_value=%d, dynamic_intt_gain=%d, als=%d \r\n",
									__func__, dynamic_intt_idx, als_dynamic_intt_value[dynamic_intt_idx], als_dynamic_intt_gain[dynamic_intt_idx], als);
#endif

			if (als > dynamic_intt_high_thr) {
				if (dynamic_intt_idx == (als_dynamic_intt_intt_num - 1)) {
					als = dynamic_intt_high_thr;
					lux_tmp = raw_convert_to_lux(als);
#if ALS_DEBUG
					LOG_INFO(">>>>>>>>>>>>>>>>>>>>>>>> INTT_MAX_LUX\r\n");
#endif
				} else {
					change_flag = true;
					als  = dynamic_intt_high_thr;
					lux_tmp = raw_convert_to_lux(als);
					dynamic_intt_idx++;
#if ALS_DEBUG
					LOG_INFO(">>>>>>>>>>>>>>>>>>>>>>>>change INTT high: %d, raw: %d \r\n", dynamic_intt_idx, als);
#endif
				}
			} else if (als < dynamic_intt_low_thr) {
				if (dynamic_intt_idx == 0) {

					lux_tmp = raw_convert_to_lux(als);
#if ALS_DEBUG
					LOG_INFO(">>>>>>>>>>>>>>>>>>>>>>>> INTT_MIN_LUX\r\n");
#endif
				} else {
					change_flag = true;
					als  = dynamic_intt_low_thr;
					lux_tmp = raw_convert_to_lux(als);
					dynamic_intt_idx--;
#if ALS_DEBUG
					LOG_INFO(">>>>>>>>>>>>>>>>>>>>>>>>change INTT low: %d, raw: %d \r\n", dynamic_intt_idx, als);
#endif
				}
			} else {
				lux_tmp = raw_convert_to_lux(als);
			}

			now_lux = lux_tmp;
			dynamic_intt_lux = now_lux/dynamic_intt_min_unit;

			epl_sensor_report_lux(dynamic_intt_lux);

			if (change_flag == true) {
				epl_sensor.als.integration_time = als_dynamic_intt_intt[dynamic_intt_idx];
				epl_sensor.als.gain = als_dynamic_intt_gain[dynamic_intt_idx];
				dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
				dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];
				epl_sensor_update_mode(obj->client);
				change_flag = false;
			}
			return dynamic_intt_lux;

		break;
#endif

	}
	return 0;
}
/*----------------------------------------------------------------------------*/

int epl_sensor_read_als(struct i2c_client *client)
{
	struct epl_sensor_priv *epld = i2c_get_clientdata(client);

	if (client == NULL) {
		LOG_ERR("CLIENT CANN'T EQUL NULL\n");
		return -EPERM;
	}

	epl_sensor_I2C_Read(epld->client, 0x13, 4);
#if PS_DYN_K && PS_DYN_K_STR
	if (dynk_enhance_flag == false) {
		dynk_enhance_ch0 = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];
		dynk_enhance_ch1 = (gRawData.raw_bytes[3]<<8) | gRawData.raw_bytes[2];

		LOG_INFO("read dynk_enhance_ch0 = %d\n", dynk_enhance_ch0);
		LOG_INFO("read dynk_enhance_ch1 = %d\n", dynk_enhance_ch1);
	} else {
#endif
		epl_sensor.als.data.channels[0] = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];
		epl_sensor.als.data.channels[1] = (gRawData.raw_bytes[3]<<8) | gRawData.raw_bytes[2];

#if ALS_DEBUG
		LOG_INFO("read als channel 0 = %d\n", epl_sensor.als.data.channels[0]);
		LOG_INFO("read als channel 1 = %d\n", epl_sensor.als.data.channels[1]);
#endif
	}


	if (epl_sensor.wait == EPL_WAIT_SINGLE)
		epl_sensor_I2C_Write(epld->client, 0x11, epl_sensor.power | epl_sensor.reset);
	return 0;
}

/*----------------------------------------------------------------------------*/

static void epl_sensor_report_ps_status(void)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;


	LOG_INFO("------------------- epl_sensor.ps.data.data=%d, value=%d \n\n", epl_sensor.ps.data.data, epl_sensor.ps.compare_low >> 3);

	input_report_abs(epld->ps_input_dev, ABS_DISTANCE, epl_sensor.ps.compare_low >> 3);
	input_sync(epld->ps_input_dev);
}

int epl_sensor_read_ps(struct i2c_client *client)
{
	struct epl_sensor_priv *epld = i2c_get_clientdata(client);
	if (client == NULL) {
		LOG_ERR("CLIENT CANN'T EQUL NULL\n");
		return -EPERM;
	}

	epl_sensor_I2C_Read(epld->client, 0x1c, 4);

	epl_sensor.ps.data.ir_data = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];
	epl_sensor.ps.data.data = (gRawData.raw_bytes[3]<<8) | gRawData.raw_bytes[2];

#if PS_DEBUG
	LOG_INFO("[%s] data = %d\n", __FUNCTION__, epl_sensor.ps.data.data);
	LOG_INFO("[%s] ir data = %d\n", __FUNCTION__, epl_sensor.ps.data.ir_data);
#endif

	if (epl_sensor.wait == EPL_WAIT_SINGLE)
		epl_sensor_I2C_Write(epld->client, 0x11, epl_sensor.power | epl_sensor.reset);

	return 0;
}

int epl_sensor_read_ps_status(struct i2c_client *client)
{
	u8 buf;
#if PS_GES
	struct epl_sensor_priv *obj = epl_sensor_obj;
	u8 new_ps_state;
	u8 ges_saturation;
	u8 ges_interrupt_flag;
	bool enable_ges = obj->enable_gflag == 1 && obj->ges_suspend == 0;
#endif
	if (client == NULL) {
		LOG_ERR("CLIENT CANN'T EQUL NULL\n");
		return -EPERM;
	}

	epl_sensor_I2C_Read(client, 0x1b, 1);

	buf = gRawData.raw_bytes[0];

#if PS_GES
	ges_saturation = (buf & 0x20);
	new_ps_state = (buf & 0x08) >> 3;
	ges_interrupt_flag = (buf & 0x04);
	LOG_INFO("[%s]:new_ps_state=%d, ps_ges_suspend_flag=%d \r\n", __func__, new_ps_state, ps_ges_suspend_flag);
	if (enable_ges == 1 && epl_sensor.ges.polling_mode == 1) {
		if (new_ps_state == 0 && (epl_sensor.ps.compare_low >> 3 == 1) && ps_ges_suspend_flag == false && ges_saturation == 0 && ges_interrupt_flag == 0x04)
			epl_sensor_notify_event();
	} else if (enable_ges == 1 && epl_sensor.ges.polling_mode == 0 && ps_ges_suspend_flag == false && ges_saturation == 0 && ges_interrupt_flag == 0x04) {
		if (new_ps_state == 0)
		   epl_sensor_notify_event();
	}
#endif

	epl_sensor.ps.saturation = (buf & 0x20);
	epl_sensor.ps.compare_high = (buf & 0x10);
	epl_sensor.ps.compare_low = (buf & 0x08);
	epl_sensor.ps.interrupt_flag = (buf & 0x04);
	epl_sensor.ps.compare_reset = (buf & 0x02);
	epl_sensor.ps.lock = (buf & 0x01);
#if PS_DEBUG
	LOG_INFO("ps: ~~~~ PS ~~~~~ \n");
	LOG_INFO("ps: buf = 0x%x\n", buf);
	LOG_INFO("ps: sat = 0x%x\n", epl_sensor.ps.saturation);
	LOG_INFO("ps: cmp h = 0x%x, l = 0x%x\n", epl_sensor.ps.compare_high, epl_sensor.ps.compare_low);
	LOG_INFO("ps: int_flag = 0x%x\n", epl_sensor.ps.interrupt_flag);
	LOG_INFO("ps: cmp_rstn = 0x%x, lock = %x\n", epl_sensor.ps.compare_reset, epl_sensor.ps.lock);
#endif
	return 0;
}

/*----------------------------------------------------------------------------*/

static int set_psensor_intr_threshold(uint16_t low_thd, uint16_t high_thd)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	struct i2c_client *client = epld->client;
	uint8_t high_msb , high_lsb, low_msb, low_lsb;


	high_msb = (uint8_t) (high_thd >> 8);
	high_lsb   = (uint8_t) (high_thd & 0x00ff);
	low_msb  = (uint8_t) (low_thd >> 8);
	low_lsb	= (uint8_t) (low_thd & 0x00ff);

	LOG_INFO("%s: low_thd = %d, high_thd = %d \n", __FUNCTION__, low_thd, high_thd);

	epl_sensor_I2C_Write(client, 0x0c, low_lsb);
	epl_sensor_I2C_Write(client, 0x0d, low_msb);
	epl_sensor_I2C_Write(client, 0x0e, high_lsb);
	epl_sensor_I2C_Write(client, 0x0f, high_msb);

	return 0;
}

static int set_lsensor_intr_threshold(uint16_t low_thd, uint16_t high_thd)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	struct i2c_client *client = epld->client;
	uint8_t high_msb , high_lsb, low_msb, low_lsb;

	high_msb = (uint8_t) (high_thd >> 8);
	high_lsb = (uint8_t) (high_thd & 0x00ff);
	low_msb = (uint8_t) (low_thd >> 8);
	low_lsb	= (uint8_t) (low_thd & 0x00ff);

	epl_sensor_I2C_Write(client, 0x08, low_lsb);
	epl_sensor_I2C_Write(client, 0x09, low_msb);
	epl_sensor_I2C_Write(client, 0x0a, high_lsb);
	epl_sensor_I2C_Write(client, 0x0b, high_msb);

	LOG_INFO("%s: low_thd = %d, high_thd = %d \n", __FUNCTION__, low_thd, high_thd);

	return 0;
}

int epl_sensor_read_als_status(struct i2c_client *client)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	u8 buf;

	if (client == NULL) {
		LOG_ERR("CLIENT CANN'T EQUL NULL\n");
		return -EPERM;
	}

	epl_sensor_I2C_Read(epld->client, 0x12, 1);
	buf = gRawData.raw_bytes[0];

	epl_sensor.als.saturation = (buf & 0x20);
	epl_sensor.als.compare_high = (buf & 0x10);
	epl_sensor.als.compare_low = (buf & 0x08);
	epl_sensor.als.interrupt_flag = (buf & 0x04);
	epl_sensor.als.compare_reset = (buf & 0x02);
	epl_sensor.als.lock = (buf & 0x01);
#if ALS_DEBUG
	LOG_INFO("als: ~~~~ ALS ~~~~~ \n");
	LOG_INFO("als: buf = 0x%x\n", buf);
	LOG_INFO("als: sat = 0x%x\n", epl_sensor.als.saturation);
	LOG_INFO("als: cmp h = 0x%x, l = 0x%x\n", epl_sensor.als.compare_high, epl_sensor.als.compare_low);
	LOG_INFO("als: int_flag = 0x%x\n", epl_sensor.als.interrupt_flag);
	LOG_INFO("als: cmp_rstn = 0x%x, lock = 0x%x\n", epl_sensor.als.compare_reset, epl_sensor.als.lock);
#endif
	return 0;
}

static int write_factory_calibration(struct epl_sensor_priv *epl_data, char *ps_data, int ps_cal_len)
{
	struct file *fp_cal;

	mm_segment_t fs;
	loff_t pos;

	LOG_FUN();
	pos = 0;

	fp_cal = filp_open(ps_cal_file, O_CREAT|O_RDWR|O_TRUNC, 0755/*S_IRWXU*/);
	if (IS_ERR(fp_cal)) {
		LOG_ERR("[ELAN]create file_h error\n");
		return -EPERM;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);
	vfs_write(fp_cal, ps_data, ps_cal_len, &pos);
	filp_close(fp_cal, NULL);
	set_fs(fs);

	return 0;
}

static bool read_factory_calibration(void)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	char buffer[100] = {0};
	if (epl_sensor.ps.factory.calibration_enable && !epl_sensor.ps.factory.calibrated) {
		fp = filp_open(ps_cal_file, O_RDWR, S_IRUSR);

		if (IS_ERR(fp)) {
			LOG_ERR("NO PS calibration file(%d)\n", (int)IS_ERR(fp));
			epl_sensor.ps.factory.calibration_enable =  false;
		} else {
			int ps_cancelation = 0, ps_hthr = 0, ps_lthr = 0;
			pos = 0;
			fs = get_fs();
			set_fs(KERNEL_DS);
			vfs_read(fp, buffer, sizeof(buffer), &pos);
			filp_close(fp, NULL);

			sscanf(buffer, "%d, %d, %d", &ps_cancelation, &ps_hthr, &ps_lthr);
			epl_sensor.ps.factory.cancelation = ps_cancelation;
			epl_sensor.ps.factory.high_threshold = ps_hthr;
			epl_sensor.ps.factory.low_threshold = ps_lthr;
			set_fs(fs);

			epl_sensor.ps.high_threshold = epl_sensor.ps.factory.high_threshold;
			epl_sensor.ps.low_threshold = epl_sensor.ps.factory.low_threshold;
			epl_sensor.ps.cancelation = epl_sensor.ps.factory.cancelation;
		}

		epl_sensor_I2C_Write(epld->client, 0x22, (u8)(epl_sensor.ps.cancelation & 0xff));
		epl_sensor_I2C_Write(epld->client, 0x23, (u8)((epl_sensor.ps.cancelation & 0xff00) >> 8));
		set_psensor_intr_threshold(epl_sensor.ps.low_threshold, epl_sensor.ps.high_threshold);

		epl_sensor.ps.factory.calibrated = true;
	}

	if (epl_sensor.als.factory.calibration_enable && !epl_sensor.als.factory.calibrated) {
		fp = filp_open(als_cal_file, O_RDONLY, S_IRUSR);
		if (IS_ERR(fp)) {
			LOG_ERR("NO ALS calibration file(%d)\n", (int)IS_ERR(fp));
			epl_sensor.als.factory.calibration_enable =  false;
		} else {
			int als_lux_per_count = 0;
			pos = 0;
			fs = get_fs();
			set_fs(KERNEL_DS);
			vfs_read(fp, buffer, sizeof(buffer), &pos);
			filp_close(fp, NULL);

			sscanf(buffer, "%d", &als_lux_per_count);
			epl_sensor.als.factory.lux_per_count = als_lux_per_count;
			set_fs(fs);
		}
		epl_sensor.als.factory.calibrated = true;
	}
	return true;
}

static int epl_run_ps_calibration(struct epl_sensor_priv *epl_data)
{
	struct epl_sensor_priv *epld = epl_data;
	bool enable_ps = epld->enable_pflag == 1 && epld->ps_suspend == 0;
	u16 ch1 = 0;
	u32 ch1_all = 0;
	int count = 5, i;
	int ps_hthr = 0, ps_lthr = 0, ps_cancelation = 0, ps_cal_len = 0;
	char ps_calibration[20];


	if (PS_MAX_XTALK < 0) {
		LOG_ERR("[%s]:Failed: PS_MAX_XTALK < 0 \r\n", __func__);
		return -EINVAL;
	}

	if (enable_ps == 0) {
		epld->enable_pflag = 1;
		epl_sensor_update_mode(epld->client);
	}

	polling_flag = false;

	for (i = 0; i < count; i++) {
		msleep(50);
		switch (epl_sensor.mode) {
		case EPL_MODE_PS:
		case EPL_MODE_ALS_PS:
			if (enable_ps == true && polling_flag == true && eint_flag == true && epl_sensor.ps.polling_mode == 0)
				epl_sensor_read_ps(epld->client);
			ch1 = epl_sensor.ps.data.data;
		break;
		}

		ch1_all = ch1_all + ch1;
		if (epl_sensor.wait == EPL_WAIT_SINGLE)
			epl_sensor_I2C_Write(epld->client, 0x11, epl_sensor.power | epl_sensor.reset);
	}

	ch1 = (u16)(ch1_all/count);

	if (ch1 > PS_MAX_XTALK) {
		LOG_ERR("[%s]:Failed: ch1 > max_xtalk(%d) \r\n", __func__, ch1);
		return -EINVAL;
	} else if (ch1 <= 0) {
		LOG_ERR("[%s]:Failed: ch1 = 0\r\n", __func__);
		return -EINVAL;
	}

	ps_hthr = ch1 + PS_h_offset;
	ps_lthr = ch1 + PS_l_offset;

	ps_cal_len = sprintf(ps_calibration, "%d, %d, %d", ps_cancelation, ps_hthr, ps_lthr);

	if (write_factory_calibration(epld, ps_calibration, ps_cal_len) < 0) {
		LOG_ERR("[%s] create file error \n", __func__);
		return -EINVAL;
	}

	epl_sensor.ps.low_threshold = ps_lthr;
	epl_sensor.ps.high_threshold = ps_hthr;
	set_psensor_intr_threshold(epl_sensor.ps.low_threshold, epl_sensor.ps.high_threshold);

	LOG_INFO("[%s]: ch1 = %d\n", __func__, ch1);

	polling_flag = true;
	epl_sensor_restart_polling();
	return ch1;
}

/*

*/
static void write_global_variable(struct i2c_client *client)
{
	u8 buf;
#if HS_ENABLE || PS_GES
	struct epl_sensor_priv *obj = epl_sensor_obj;
#endif
#if HS_ENABLE
	bool enable_hs = obj->enable_hflag == 1 && obj->hs_suspend == 0;
#endif
#if PS_GES
	bool enable_ges = obj->enable_gflag == 1 && obj->ges_suspend == 0;
#endif

	buf = epl_sensor.reset | epl_sensor.power;
	epl_sensor_I2C_Write(client, 0x11, buf);

	   /* read revno*/
	epl_sensor_I2C_Read(client, 0x20, 2);
	epl_sensor.revno = gRawData.raw_bytes[0] | gRawData.raw_bytes[1] << 8;

	/*chip refrash*/
	epl_sensor_I2C_Write(client, 0xfd, 0x8e);
	epl_sensor_I2C_Write(client, 0xfe, 0x22);
	epl_sensor_I2C_Write(client, 0xfe, 0x02);
	epl_sensor_I2C_Write(client, 0xfd, 0x00);

	epl_sensor_I2C_Write(client, 0xfc, EPL_A_D | EPL_NORMAL | EPL_GFIN_ENABLE | EPL_VOS_ENABLE | EPL_DOC_ON);

#if HS_ENABLE
	if (enable_hs) {
		epl_sensor.mode = EPL_MODE_PS;
		epl_sensor_I2C_Write(obj->client, 0x00, epl_sensor.wait | EPL_MODE_IDLE);

		/*hs setting*/
		buf = epl_sensor.hs.integration_time | epl_sensor.hs.gain;
		epl_sensor_I2C_Write(client, 0x03, buf);

		buf = epl_sensor.hs.adc | epl_sensor.hs.cycle;
		epl_sensor_I2C_Write(client, 0x04, buf);

		buf = epl_sensor.hs.ir_on_control | epl_sensor.hs.ir_mode | epl_sensor.hs.ir_driver;
		epl_sensor_I2C_Write(client, 0x05, buf);

		buf = epl_sensor.hs.compare_reset | epl_sensor.hs.lock;

		epl_sensor_I2C_Write(client, 0x1b, buf);
	}
#if !PS_GES	/*PS_GES*/
	else
#elif PS_GES
	else if (enable_ges)
#endif  /*PS_GES*/
#endif

#if PS_GES

#if !HS_ENABLE /*HS_ENABLE*/
	if (enable_ges) {
#endif  /*HS_ENABLE*/
		/*ges setting*/
		buf = epl_sensor.ges.integration_time | epl_sensor.ges.gain;
		epl_sensor_I2C_Write(client, 0x03, buf);

		buf = epl_sensor.ges.adc | epl_sensor.ges.cycle;
		epl_sensor_I2C_Write(client, 0x04, buf);

		buf = epl_sensor.ges.ir_on_control | epl_sensor.ges.ir_mode | epl_sensor.ges.ir_drive;
		epl_sensor_I2C_Write(client, 0x05, buf);

		buf = epl_sensor.interrupt_control | epl_sensor.ges.persist | epl_sensor.ges.interrupt_type;
		epl_sensor_I2C_Write(client, 0x06, buf);

		buf = epl_sensor.ges.compare_reset | epl_sensor.ges.lock;
		epl_sensor_I2C_Write(client, 0x1b, buf);

		epl_sensor_I2C_Write(client, 0x22, (u8)(epl_sensor.ges.cancelation & 0xff));
		epl_sensor_I2C_Write(client, 0x23, (u8)((epl_sensor.ges.cancelation & 0xff00) >> 8));
		set_psensor_intr_threshold(epl_sensor.ges.low_threshold, epl_sensor.ges.high_threshold);
	} else {
#endif
		/*ps setting*/
		buf = epl_sensor.ps.integration_time | epl_sensor.ps.gain;
		epl_sensor_I2C_Write(client, 0x03, buf);

		buf = epl_sensor.ps.adc | epl_sensor.ps.cycle;
		epl_sensor_I2C_Write(client, 0x04, buf);

		buf = epl_sensor.ps.ir_on_control | epl_sensor.ps.ir_mode | epl_sensor.ps.ir_drive;
		epl_sensor_I2C_Write(client, 0x05, buf);

		buf = epl_sensor.interrupt_control | epl_sensor.ps.persist | epl_sensor.ps.interrupt_type;
		epl_sensor_I2C_Write(client, 0x06, buf);

		buf = epl_sensor.ps.compare_reset | epl_sensor.ps.lock;
		epl_sensor_I2C_Write(client, 0x1b, buf);

		epl_sensor_I2C_Write(client, 0x22, (u8)(epl_sensor.ps.cancelation & 0xff));
		epl_sensor_I2C_Write(client, 0x23, (u8)((epl_sensor.ps.cancelation & 0xff0) >> 8));
		set_psensor_intr_threshold(epl_sensor.ps.low_threshold, epl_sensor.ps.high_threshold);

		/*als setting*/
		buf = epl_sensor.als.integration_time | epl_sensor.als.gain;
		epl_sensor_I2C_Write(client, 0x01, buf);

		buf = epl_sensor.als.adc | epl_sensor.als.cycle;
		epl_sensor_I2C_Write(client, 0x02, buf);

		buf = epl_sensor.als.interrupt_channel_select | epl_sensor.als.persist | epl_sensor.als.interrupt_type;
		epl_sensor_I2C_Write(client, 0x07, buf);

		buf = epl_sensor.als.compare_reset | epl_sensor.als.lock;
		epl_sensor_I2C_Write(client, 0x12, buf);

		set_lsensor_intr_threshold(epl_sensor.als.low_threshold, epl_sensor.als.high_threshold);
	}

	buf = epl_sensor.wait | epl_sensor.mode;
	epl_sensor_I2C_Write(client, 0x00, buf);
}

static void set_als_ps_intr_type(struct i2c_client *client, bool ps_polling, bool als_polling)
{


	switch ((ps_polling << 1) | als_polling) {
	case 0:
		epl_sensor.interrupt_control = 	EPL_INT_CTRL_ALS_OR_PS;
		epl_sensor.als.interrupt_type = EPL_INTTY_ACTIVE;
		epl_sensor.ps.interrupt_type = EPL_INTTY_ACTIVE;
		break;

	case 1:
		epl_sensor.interrupt_control = 	EPL_INT_CTRL_PS;
		epl_sensor.als.interrupt_type = EPL_INTTY_DISABLE;
		epl_sensor.ps.interrupt_type = EPL_INTTY_ACTIVE;
		break;

	case 2:
		epl_sensor.interrupt_control = 	EPL_INT_CTRL_ALS;
		epl_sensor.als.interrupt_type = EPL_INTTY_ACTIVE;
		epl_sensor.ps.interrupt_type = EPL_INTTY_DISABLE;
		break;

	case 3:
		epl_sensor.interrupt_control = 	EPL_INT_CTRL_ALS_OR_PS;
		epl_sensor.als.interrupt_type = EPL_INTTY_DISABLE;
		epl_sensor.ps.interrupt_type = EPL_INTTY_DISABLE;
		break;
	}
}


static void initial_global_variable(struct i2c_client *client, struct epl_sensor_priv *obj)
{


	epl_sensor.power = EPL_POWER_ON;
	epl_sensor.reset = EPL_RESETN_RUN;
	epl_sensor.mode = EPL_MODE_IDLE;
	epl_sensor.wait = EPL_WAIT_20_MS;
	epl_sensor.osc_sel = EPL_OSC_SEL_1MHZ;


	epl_sensor.als.polling_mode = ALS_POLLING_MODE;
	epl_sensor.als.integration_time = EPL_ALS_INTT_64;
	epl_sensor.als.gain = EPL_GAIN_MID;
	epl_sensor.als.adc = EPL_PSALS_ADC_13;
	epl_sensor.als.cycle = EPL_CYCLE_16;
	epl_sensor.als.interrupt_channel_select = EPL_ALS_INT_CHSEL_1;
	epl_sensor.als.persist = EPL_PERIST_1;
	epl_sensor.als.compare_reset = EPL_CMP_RESET;
	epl_sensor.als.lock = EPL_UN_LOCK;
	epl_sensor.als.report_type = CMC_BIT_RAW;
	epl_sensor.als.high_threshold = ALS_HIGH_THRESHOLD;
	epl_sensor.als.low_threshold = ALS_LOW_THRESHOLD;

	epl_sensor.als.factory.calibration_enable =  false;
	epl_sensor.als.factory.calibrated = false;
	epl_sensor.als.factory.lux_per_count = LUX_PER_COUNT;

#if ALS_DYN_INTT

	epl_sensor.als.lsrc_type = CMC_BIT_LSRC_NON;

	if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
		dynamic_intt_idx = dynamic_intt_init_idx;
		epl_sensor.als.integration_time = als_dynamic_intt_intt[dynamic_intt_idx];
		epl_sensor.als.gain = als_dynamic_intt_gain[dynamic_intt_idx];
		dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
		dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];
	}

	c_gain = 300;

	obj->lsource_thd_high = 1900;
	obj->lsource_thd_low = 1500;
	obj->c_gain_h = 300;
	obj->c_gain_l = 214;

#endif

	epl_sensor.ps.polling_mode = PS_POLLING_MODE;
	epl_sensor.ps.integration_time = EPL_PS_INTT_144;
	epl_sensor.ps.gain = EPL_GAIN_LOW;
#if PS_DYN_K
	dynk_min_ps_raw_data = 0xffff;
	dynk_max_ir_data = 50000;
	dynk_low_offset = 160;
	dynk_high_offset = 510;
	dynk_change_thd_max = 30000;
	dynk_thd_offset = 0;
#if PS_DYN_K_STR
	dynk_enhance_max_ch0 = 50000;
	dynk_enhance_integration_time = EPL_ALS_INTT_128;
	dynk_enhance_gain = epl_sensor.ps.gain;
	dynk_enhance_adc = EPL_PSALS_ADC_12;
#endif
#endif
	epl_sensor.ps.adc = EPL_PSALS_ADC_12;
	epl_sensor.ps.cycle = EPL_CYCLE_16;
	epl_sensor.ps.persist = EPL_PERIST_1;
	epl_sensor.ps.ir_on_control = EPL_IR_ON_CTRL_ON;
	epl_sensor.ps.ir_mode = EPL_IR_MODE_CURRENT;
	epl_sensor.ps.ir_drive = EPL_IR_DRIVE_100;
	epl_sensor.ps.compare_reset = EPL_CMP_RESET;
	epl_sensor.ps.lock = EPL_UN_LOCK;
	epl_sensor.ps.high_threshold = PS_HIGH_THRESHOLD;
	epl_sensor.ps.low_threshold = PS_LOW_THRESHOLD;

	epl_sensor.ps.factory.calibration_enable = false;
	epl_sensor.ps.factory.calibrated = false;
	epl_sensor.ps.factory.cancelation = 0;

#if HS_ENABLE

	epl_sensor.hs.integration_time = EPL_PS_INTT_80;
	epl_sensor.hs.integration_time_max = EPL_PS_INTT_272;
	epl_sensor.hs.integration_time_min = EPL_PS_INTT_32;
	epl_sensor.hs.gain = EPL_GAIN_LOW;
	epl_sensor.hs.adc = EPL_PSALS_ADC_11;
	epl_sensor.hs.cycle = EPL_CYCLE_4;
	epl_sensor.hs.ir_on_control = EPL_IR_ON_CTRL_ON;
	epl_sensor.hs.ir_mode = EPL_IR_MODE_CURRENT;
	epl_sensor.hs.ir_driver = EPL_IR_DRIVE_200;
	epl_sensor.hs.compare_reset = EPL_CMP_RESET;
	epl_sensor.hs.lock = EPL_UN_LOCK;
	epl_sensor.hs.low_threshold = 6400;
	epl_sensor.hs.mid_threshold = 25600;
	epl_sensor.hs.high_threshold = 60800;
#endif

#if PS_GES

	epl_sensor.ges.polling_mode = PS_POLLING_MODE;
	epl_sensor.ges.integration_time = EPL_PS_INTT_80;
	epl_sensor.ges.gain = EPL_GAIN_LOW;
	epl_sensor.ges.adc = EPL_PSALS_ADC_12;
	epl_sensor.ges.cycle = EPL_CYCLE_2;
	epl_sensor.ges.persist = EPL_PERIST_1;
	epl_sensor.ges.ir_on_control = EPL_IR_ON_CTRL_ON;
	epl_sensor.ges.ir_mode = EPL_IR_MODE_CURRENT;
	epl_sensor.ges.ir_drive = EPL_IR_DRIVE_200;
	epl_sensor.ges.compare_reset = EPL_CMP_RESET;
	epl_sensor.ges.lock = EPL_UN_LOCK;
	epl_sensor.ges.high_threshold = ges_threshold_high;
	epl_sensor.ges.low_threshold = ges_threshold_low;
#endif

	set_als_ps_intr_type(client, epl_sensor.ps.polling_mode, epl_sensor.als.polling_mode);

	write_global_variable(client);
}

#if HS_ENABLE
int epl_sensor_read_hs(struct i2c_client *client)
{
	u8 buf;

	if (client == NULL) {
		LOG_ERR("CLIENT CANN'T EQUL NULL\n");
		return -EPERM;
	}

	mutex_lock(&hs_sensor_mutex);
	epl_sensor_I2C_Read(client, 0x1e, 2);
	epl_sensor.hs.raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];
	LOG_INFO("epl_sensor.hs.raw=%d \r\n", epl_sensor.hs.raw);
	if (epl_sensor.hs.dynamic_intt == true && epl_sensor.hs.raw > epl_sensor.hs.high_threshold && epl_sensor.hs.integration_time > epl_sensor.hs.integration_time_min) {
		epl_sensor.hs.integration_time -= 4;
		buf = epl_sensor.hs.integration_time | epl_sensor.hs.gain;
		epl_sensor_I2C_Write(client, 0x03, buf);
	} else if (epl_sensor.hs.dynamic_intt == true && epl_sensor.hs.raw > epl_sensor.hs.low_threshold && epl_sensor.hs.raw < epl_sensor.hs.mid_threshold && epl_sensor.hs.integration_time < epl_sensor.hs.integration_time_max) {
		epl_sensor.hs.integration_time += 4;
		buf = epl_sensor.hs.integration_time | epl_sensor.hs.gain;
		epl_sensor_I2C_Write(client, 0x03, buf);
	}

	mutex_unlock(&hs_sensor_mutex);

	if (epl_sensor.hs.raws_count < 200) {
		epl_sensor.hs.raws[epl_sensor.hs.raws_count] = epl_sensor.hs.raw;
		epl_sensor.hs.raws_count++;
	}

	return 0;
}
#endif

#if PS_DYN_K
void epl_sensor_reset_dynk_thd(u8 last_status, u8 now_status)
{
	if (last_status == 0 && now_status == 1) {
#if PS_DYN_K_STR
		if ((epl_sensor.ps.saturation == 0) && (epl_sensor.ps.data.ir_data < dynk_max_ir_data) && (dynk_enhance_ch0 < dynk_enhance_max_ch0))
#else
		if ((epl_sensor.ps.saturation == 0) && (epl_sensor.ps.data.ir_data < dynk_max_ir_data)) {
#endif
			dynk_min_ps_raw_data = epl_sensor.ps.data.data;
		}

		dynk_thd_low = dynk_min_ps_raw_data + dynk_low_offset;
		dynk_thd_high = dynk_min_ps_raw_data + dynk_high_offset;

		if (dynk_thd_low > 65534)
			dynk_thd_low = 65534;
		if (dynk_thd_high > 65535)
			dynk_thd_high = 65535;
#if PS_DEBUG
		LOG_INFO("[%s]:restart dynk ps raw = %d, min = %d, ir_data = %d\n", __func__, epl_sensor.ps.data.data, dynk_min_ps_raw_data, epl_sensor.ps.data.ir_data);
		LOG_INFO("[%s]:restart dynk thre_l = %ld, thre_h = %ld\n", __func__, (long)dynk_thd_low, (long)dynk_thd_high);
#endif
		eint_flag = false;
		set_psensor_intr_threshold((u16)dynk_thd_low, (u16)dynk_thd_high);
		eint_flag = true;
	} else if (last_status == 1 && now_status == 0) {
		dynk_change_flag = true;
	}
}
#if PS_DYN_K_STR
void epl_sensor_enhance_enable(struct i2c_client *client, bool enable)
{
	bool cmp_flag = false;
	u8 buf, now_cmp_l;
	int enh_time = 0, ps_time = 0, total_time = 0;

	ps_time = ps_sensing_time(epl_sensor.ps.integration_time, epl_sensor.ps.adc, epl_sensor.ps.cycle);
	mutex_lock(&sensor_mutex);
	polling_flag = false;

	epl_sensor_I2C_Read(client, 0x1b, 1);
	buf = gRawData.raw_bytes[0];
	epl_sensor.ps.compare_high = (buf & 0x10);
	now_cmp_l = (buf & 0x08);

	if (now_cmp_l != epl_sensor.ps.compare_low) {
		LOG_INFO("[%s]: buf= 0x%x, now_cmp_l= 0x%x, epl_sensor.ps.compare_low= 0x%x \r\n", __func__, buf, now_cmp_l, epl_sensor.ps.compare_low);

		epl_sensor.ps.compare_reset = EPL_CMP_RESET;
		epl_sensor.ps.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(client, 0x1b, epl_sensor.ps.compare_reset | epl_sensor.ps.lock);

		epl_sensor.ps.interrupt_flag = EPL_INT_CLEAR;
		epl_sensor.als.interrupt_flag = EPL_INT_CLEAR;
		cmp_flag = true;
		low_trigger_flag = false;
	}

	epl_sensor_I2C_Write(client, 0x00, epl_sensor.wait | EPL_MODE_IDLE);

	if (enable == true) {
		enh_time = als_sensing_time(dynk_enhance_integration_time, dynk_enhance_adc, epl_sensor.als.cycle) / 2;
		epl_sensor_I2C_Write(client, 0x01, dynk_enhance_integration_time | dynk_enhance_gain);
		epl_sensor_I2C_Write(client, 0x02, dynk_enhance_adc | epl_sensor.als.cycle);
		epl_sensor_I2C_Write(client, 0xfc, EPL_A_D | EPL_NORMAL | EPL_GFIN_ENABLE | EPL_VOS_ENABLE | EPL_DOC_OFF);
	} else {
		enh_time = als_sensing_time(epl_sensor.als.integration_time, epl_sensor.als.adc, epl_sensor.als.cycle);
		epl_sensor_I2C_Write(client, 0x01, epl_sensor.als.integration_time | epl_sensor.als.gain);
		epl_sensor_I2C_Write(client, 0x02, epl_sensor.als.adc | epl_sensor.als.cycle);
		epl_sensor_I2C_Write(client, 0xfc, EPL_A_D | EPL_NORMAL | EPL_GFIN_ENABLE | EPL_VOS_ENABLE | EPL_DOC_ON);
	}

	als_frame_time = enh_time;
	ps_frame_time = ps_time;

	epl_sensor_I2C_Write(client, 0x00, epl_sensor.wait | epl_sensor.mode);
	if (cmp_flag == true) {

		epl_sensor.ps.compare_reset = EPL_CMP_RUN;
		epl_sensor.ps.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(client, 0x1b, epl_sensor.ps.compare_reset | epl_sensor.ps.lock);

		cmp_flag = false;
	}
	mutex_unlock(&sensor_mutex);

	total_time = ps_time+enh_time;
	if ((2*total_time) >= dynk_polling_delay) {
		dynk_polling_delay = 2*total_time+50;
		LOG_INFO("[%s]: dynk_polling_delay=%d \r\n", __func__, dynk_polling_delay);
	}

	if (epl_sensor.als.polling_mode == 1) {
		msleep(total_time);
		LOG_INFO("[%s] PS+ALS(%dms)\r\n", __func__, total_time);
	}


	if (enable == true) {
		if (epl_sensor.ps.polling_mode == 1) {
			epl_sensor_read_ps(client);
		}
		epl_sensor_read_als(client);
	}

	if (epl_sensor.ps.interrupt_flag == EPL_INT_TRIGGER && epl_sensor.ps.polling_mode == 0) {

		epl_sensor.ps.compare_reset = EPL_CMP_RUN;
		epl_sensor.ps.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(client, 0x1b, epl_sensor.ps.compare_reset | epl_sensor.ps.lock);
	}

	if (epl_sensor.als.interrupt_flag == EPL_INT_TRIGGER && epl_sensor.als.polling_mode == 0) {

		epl_sensor.als.compare_reset = EPL_CMP_RUN;
		epl_sensor.als.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(client, 0x12, epl_sensor.als.compare_reset | epl_sensor.als.lock);

	}
	low_trigger_flag = false;
	polling_flag = true;

}
#endif

void epl_sensor_restart_dynk_polling(void)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	cancel_delayed_work(&epld->dynk_thd_polling_work);
	schedule_delayed_work(&epld->dynk_thd_polling_work, msecs_to_jiffies(2*dynk_polling_delay));
}

void epl_sensor_dynk_thd_polling_work(struct work_struct *work)
{
	struct epl_sensor_priv *obj = epl_sensor_obj;

	bool enable_ps = obj->enable_pflag == 1 && obj->ps_suspend == 0;
#if PS_DEBUG
	bool enable_als = obj->enable_lflag == 1 && obj->als_suspend == 0;
	LOG_INFO("[%s]:als / ps enable: %d / %d\n", __func__, enable_als, enable_ps);
#endif

	if (enable_ps == true) {
#if PS_DYN_K_STR
		dynk_enhance_flag = false;
		if (polling_flag == true && eint_flag == true) {
			 epl_sensor_enhance_enable(obj->client, true);
		}
#endif
		if (polling_flag == true && eint_flag == true && epl_sensor.ps.polling_mode == 0) {
			mutex_lock(&sensor_mutex);
			epl_sensor_read_ps_status(obj->client);
			epl_sensor_read_ps(obj->client);
			mutex_unlock(&sensor_mutex);
		}
#if PS_DYN_K_STR
		if ((dynk_min_ps_raw_data > epl_sensor.ps.data.data)
			&& (epl_sensor.ps.saturation == 0)
			&& (epl_sensor.ps.data.ir_data < dynk_max_ir_data)
			&& (dynk_enhance_ch0 < dynk_enhance_max_ch0))
#else
		if ((dynk_min_ps_raw_data > epl_sensor.ps.data.data)
			&& (epl_sensor.ps.saturation == 0)
			&& (epl_sensor.ps.data.ir_data < dynk_max_ir_data)) {
#endif
			dynk_min_ps_raw_data = epl_sensor.ps.data.data;
			dynk_thd_low = dynk_min_ps_raw_data + dynk_low_offset;
			dynk_thd_high = dynk_min_ps_raw_data + dynk_high_offset;

			if (dynk_thd_low > 65534)
				dynk_thd_low = 65534;
			if (dynk_thd_high > 65535)
				dynk_thd_high = 65535;
#if PS_DEBUG
			LOG_INFO("[%s]:dyn ps raw = %d, min = %d, ir_data = %d\n", __func__, epl_sensor.ps.data.data, dynk_min_ps_raw_data, epl_sensor.ps.data.ir_data);
#endif
			eint_flag = false;
			mutex_lock(&sensor_mutex);
			set_psensor_intr_threshold((u16)dynk_thd_low, (u16)dynk_thd_high);
			mutex_unlock(&sensor_mutex);
			eint_flag = true;
#if PS_DEBUG
			LOG_INFO("[%s]:dyn k thre_l = %ld, thre_h = %ld\n", __func__, (long)dynk_thd_low, (long)dynk_thd_high);
#endif
		} else if (dynk_change_flag == true && (epl_sensor.ps.data.data > dynk_change_thd_max) && ((epl_sensor.ps.compare_low >> 3) == 0)) {
			dynk_change_flag = false;
			dynk_thd_low += dynk_thd_offset;
			dynk_thd_high += dynk_thd_offset;

			if (dynk_thd_low > 65534)
				dynk_thd_low = 65534;
			if (dynk_thd_high > 65535)
				dynk_thd_high = 65535;

			eint_flag = false;
			mutex_lock(&sensor_mutex);
			set_psensor_intr_threshold((u16)dynk_thd_low, (u16)dynk_thd_high);
			mutex_unlock(&sensor_mutex);
			eint_flag = true;
#if PS_DEBUG
			LOG_INFO("[%s]: epl_sensor.ps.data.data=%d, L/H=%ld/%ld \r\n", __func__, epl_sensor.ps.data.data, (long)dynk_thd_low, (long)dynk_thd_high);
#endif
		}
#if PS_DYN_K_STR
		if (polling_flag == true && eint_flag == true) {
			epl_sensor_enhance_enable(obj->client, false);
		}
		dynk_enhance_flag = true;
#endif
		schedule_delayed_work(&obj->dynk_thd_polling_work, msecs_to_jiffies(dynk_polling_delay));
	}
}
#endif

/************************************************************************/

static int als_sensing_time(int intt, int adc, int cycle)
{
	long sensing_us_time;
	int sensing_ms_time;
	int als_intt, als_adc, als_cycle;

	als_intt = als_intt_value[intt>>2];
	als_adc = adc_value[adc>>3];
	als_cycle = cycle_value[cycle];
#if COMMON_DEBUG
	LOG_INFO("ALS: INTT=%d, ADC=%d, Cycle=%d \r\n", als_intt, als_adc, als_cycle);
#endif

	sensing_us_time = (als_intt + als_adc*2*2) * 2 * als_cycle;
	sensing_ms_time = sensing_us_time / 1000;

#if COMMON_DEBUG
	LOG_INFO("[%s]: sensing=%d ms \r\n", __func__, sensing_ms_time);
#endif
	return sensing_ms_time + 5;
}

static int ps_sensing_time(int intt, int adc, int cycle)
{
	long sensing_us_time;
	int sensing_ms_time;
	int ps_intt, ps_adc, ps_cycle;

	ps_intt = ps_intt_value[intt>>2];
	ps_adc = adc_value[adc>>3];
	ps_cycle = cycle_value[cycle];
#if COMMON_DEBUG
	LOG_INFO("PS: INTT=%d, ADC=%d, Cycle=%d \r\n", ps_intt, ps_adc, ps_cycle);
#endif

	sensing_us_time = (ps_intt*3 + ps_adc*2*3) * ps_cycle;
	sensing_ms_time = sensing_us_time / 1000;
#if COMMON_DEBUG
	LOG_INFO("[%s]: sensing=%d ms\r\n", __func__, sensing_ms_time);
#endif

	return sensing_ms_time + 5;
}

static int epl_sensor_get_wait_time(int ps_time, int als_time)
{
	int wait_idx = 0;
	int wait_time = 0;

	wait_time = als_time - ps_time;
	if (wait_time < 0) {
		wait_time = 0;
	}
#if COMMON_DEBUG
	LOG_INFO("[%s]: wait_len = %d \r\n", __func__, wait_len);
#endif
	for (wait_idx = 0; wait_idx < wait_len; wait_idx++) {
		if (wait_time < wait_value[wait_idx]) {
			break;
		}
	}
	if (wait_idx >= wait_len) {
		wait_idx = wait_len - 1;
	}

#if COMMON_DEBUG
	LOG_INFO("[%s]: wait_idx = %d, wait = %dms \r\n", __func__, wait_idx, wait_value[wait_idx]);
#endif
	return wait_idx << 4;
}
/************************************************************************/

void epl_sensor_update_mode(struct i2c_client *client)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int als_time = 0, ps_time = 0;

	bool enable_ps = epld->enable_pflag == 1 && epld->ps_suspend == 0;
	bool enable_als = epld->enable_lflag == 1 && epld->als_suspend == 0;
#if HS_ENABLE
	bool enable_hs = epld->enable_hflag == 1 && epld->hs_suspend == 0;
#endif
#if PS_GES
	bool enable_ges = epld->enable_gflag == 1 && epld->ges_suspend == 0;
#endif
	polling_flag = false;
	low_trigger_flag = false;
	als_time = als_sensing_time(epl_sensor.als.integration_time, epl_sensor.als.adc, epl_sensor.als.cycle);
	ps_time = ps_sensing_time(epl_sensor.ps.integration_time, epl_sensor.ps.adc, epl_sensor.ps.cycle);

	als_frame_time = als_time;
	ps_frame_time = ps_time;

#if HS_ENABLE
	if (enable_hs) {
		LOG_INFO("[%s]: HS mode \r\n", __func__);
		epl_sensor_restart_polling();
	} else {
#endif

		epl_sensor.ps.compare_reset = EPL_CMP_RESET;
		epl_sensor.ps.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x1b, epl_sensor.ps.compare_reset | epl_sensor.ps.lock);


		epl_sensor.als.compare_reset = EPL_CMP_RESET;
		epl_sensor.als.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x12, epl_sensor.als.compare_reset | epl_sensor.als.lock);



		epl_sensor.ps.interrupt_flag = EPL_INT_CLEAR;
		epl_sensor.als.interrupt_flag = EPL_INT_CLEAR;

#if PS_GES
		LOG_INFO("mode selection = 0x%x\n", (enable_ps|enable_ges) | (enable_als << 1));
#else
		LOG_INFO("mode selection = 0x%x\n", enable_ps | (enable_als << 1));
#endif

#if PS_GES

		switch ((enable_als << 1) | (enable_ps|enable_ges))
#else

		switch ((enable_als << 1) | enable_ps) {
#endif

		case 0:
			epl_sensor.mode = EPL_MODE_IDLE;
			break;

		case 1:
#if PS_DYN_K && PS_DYN_K_STR
			epl_sensor.mode = EPL_MODE_ALS_PS;
#else
			epl_sensor.mode = EPL_MODE_PS;
#endif
			break;

		case 2:
			epl_sensor.mode = EPL_MODE_ALS;
			break;

		case 3:
			epl_sensor.mode = EPL_MODE_ALS_PS;
			break;
		}

		epl_sensor_I2C_Write(epld->client, 0x00, epl_sensor.wait | EPL_MODE_IDLE);
		read_factory_calibration();
		epl_sensor_I2C_Write(epld->client, 0x02, epl_sensor.als.adc | epl_sensor.als.cycle);
		set_als_ps_intr_type(epld->client, epl_sensor.ps.polling_mode, epl_sensor.als.polling_mode);
		epl_sensor_I2C_Write(epld->client, 0x06, epl_sensor.interrupt_control | epl_sensor.ps.persist | epl_sensor.ps.interrupt_type);

		epl_sensor_I2C_Write(epld->client, 0x07, epl_sensor.als.interrupt_channel_select | epl_sensor.als.persist | epl_sensor.als.interrupt_type);

#if ALS_DYN_INTT
		if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
			epl_sensor_I2C_Write(client, 0x01, epl_sensor.als.integration_time | epl_sensor.als.gain);
		}
#endif

		if (enable_als == true && enable_ps == false) {
			epl_sensor_I2C_Write(client, 0xfc, EPL_A_D | EPL_NORMAL | EPL_GFIN_ENABLE | EPL_VOS_ENABLE | EPL_DOC_ON);
		}

		if (epl_sensor.mode == EPL_MODE_ALS_PS && epl_sensor.als.polling_mode == 0 && epl_sensor.ps.polling_mode == 0) {
			int wait = 0;
			wait = epl_sensor_get_wait_time(ps_time, als_time);
			epl_sensor_I2C_Write(epld->client, 0x00, wait | epl_sensor.mode);
			epl_sensor.wait = wait;
			LOG_INFO("[%s]: epl_sensor.als.polling_mode=%d \r\n", __func__, epl_sensor.als.polling_mode);
		} else {
#if PS_GES
			if (enable_ges == 1 && enable_ps == 0) {
				set_psensor_intr_threshold(epl_sensor.ges.low_threshold, epl_sensor.ges.high_threshold);
				epl_sensor_I2C_Write(client, 0x00, EPL_WAIT_2_MS | epl_sensor.mode);
			} else
#endif
				epl_sensor_I2C_Write(epld->client, 0x00, epl_sensor.wait | epl_sensor.mode);
		}

		epl_sensor.als.compare_reset = EPL_CMP_RUN;
		epl_sensor.als.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x12, epl_sensor.als.compare_reset | epl_sensor.als.lock);

		epl_sensor.ps.compare_reset = EPL_CMP_RUN;
		epl_sensor.ps.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x1b, epl_sensor.ps.compare_reset | epl_sensor.ps.lock);

#if COMMON_DEBUG

		if (enable_ps == 1) {
			LOG_INFO("[%s] PS:low_thd = %d, high_thd = %d \n", __func__, epl_sensor.ps.low_threshold, epl_sensor.ps.high_threshold);
		}

		if (enable_als == 1 && epl_sensor.als.polling_mode == 0) {
			LOG_INFO("[%s] ALS:low_thd = %d, high_thd = %d \n", __func__, epl_sensor.als.low_threshold, epl_sensor.als.high_threshold);
		}
#if PS_GES
		if (enable_ges) {
			LOG_INFO("[%s] GES:low_thd = %d, high_thd = %d \n", __func__, epl_sensor.ges.low_threshold, epl_sensor.ges.high_threshold);

		}
#endif

		LOG_INFO("[%s] reg0x00= 0x%x \n", __func__, epl_sensor.wait | epl_sensor.mode);
		LOG_INFO("[%s] reg0x07= 0x%x \n", __func__, epl_sensor.als.interrupt_channel_select | epl_sensor.als.persist | epl_sensor.als.interrupt_type);
		LOG_INFO("[%s] reg0x06= 0x%x \n", __func__, epl_sensor.interrupt_control | epl_sensor.ps.persist | epl_sensor.ps.interrupt_type);
		LOG_INFO("[%s] reg0x11= 0x%x \n", __func__, epl_sensor.power | epl_sensor.reset);
		LOG_INFO("[%s] reg0x12= 0x%x \n", __func__, epl_sensor.als.compare_reset | epl_sensor.als.lock);
		LOG_INFO("[%s] reg0x1b= 0x%x \n", __func__, epl_sensor.ps.compare_reset | epl_sensor.ps.lock);
#endif

		if (epl_sensor.mode == EPL_MODE_PS) {
			msleep(ps_time);
			LOG_INFO("[%s] PS only(%dms)\r\n", __func__, ps_time);
		} else if (epl_sensor.mode == EPL_MODE_ALS) {
			msleep(als_time);
			LOG_INFO("[%s] ALS only(%dms)\r\n", __func__, als_time);
		} else if (epl_sensor.mode == EPL_MODE_ALS_PS && epl_sensor.als.polling_mode == 1) {
			msleep(ps_time+als_time);
			LOG_INFO("[%s] PS+ALS(%dms)\r\n", __func__, ps_time+als_time);
		}

		if (epl_sensor.ps.interrupt_flag == EPL_INT_TRIGGER) {

			epl_sensor.ps.compare_reset = EPL_CMP_RUN;
			epl_sensor.ps.lock = EPL_UN_LOCK;
			epl_sensor_I2C_Write(epld->client, 0x1b, epl_sensor.ps.compare_reset | epl_sensor.ps.lock);
		}

		if (epl_sensor.als.interrupt_flag == EPL_INT_TRIGGER) {

			epl_sensor.als.compare_reset = EPL_CMP_RUN;
			epl_sensor.als.lock = EPL_UN_LOCK;
			epl_sensor_I2C_Write(epld->client, 0x12, epl_sensor.als.compare_reset | epl_sensor.als.lock);
		}
#if PS_GES
		if ((enable_als == 1 && epl_sensor.als.polling_mode == 1) || (enable_ps == 1 && epl_sensor.ps.polling_mode == 1) || (enable_ges == 1 && epl_sensor.ges.polling_mode == 1))
#else
		if ((enable_als == 1 && epl_sensor.als.polling_mode == 1) || (enable_ps == 1 && epl_sensor.ps.polling_mode == 1)) {
#endif
			epl_sensor_restart_polling();
		}

#if PS_DYN_K
		if (enable_ps == 1) {
			epl_sensor_restart_dynk_polling();
		}
#endif
	}
	low_trigger_flag = false;
	polling_flag = true;
#if PS_DYN_K_STR
	if (enable_als) {
		LOG_INFO("[%s]: ALS mode is enabled ! \r\n", __func__);
		dynk_enhance_flag = true;
	}
#endif
}

/*----------------------------------------------------------------------------*/
static void epl_sensor_polling_work(struct work_struct *work)
{

	struct epl_sensor_priv *epld = epl_sensor_obj;
	struct i2c_client *client = epld->client;

	bool enable_ps = epld->enable_pflag == 1 && epld->ps_suspend == 0;
	bool enable_als = epld->enable_lflag == 1 && epld->als_suspend == 0;
#if HS_ENABLE
	bool enable_hs = epld->enable_hflag == 1 && epld->hs_suspend == 0;

#endif
#if PS_GES
	 bool enable_ges = epld->enable_gflag == 1 && epld->ges_suspend == 0;
#endif

#if COMMON_DEBUG
#if HS_ENABLE && PS_GES
	LOG_INFO("enable_pflag=%d, enable_lflag=%d, enable_hs=%d, enable_ges=%d \n", enable_ps, enable_als, enable_hs, enable_ges);
#elif HS_ENABLE
	LOG_INFO("enable_pflag=%d, enable_lflag=%d, enable_hs=%d\n", enable_ps, enable_als, enable_hs);
#elif PS_GES
	LOG_INFO("enable_pflag=%d, enable_lflag=%d, enable_ges=%d\n", enable_ps, enable_als, enable_ges);
#else
	LOG_INFO("enable_pflag = %d, enable_lflag = %d \n", enable_ps, enable_als);
#endif
#endif

	cancel_delayed_work(&epld->polling_work);

	if ((enable_als &&  epl_sensor.als.polling_mode == 1) || (enable_ps &&  epl_sensor.ps.polling_mode == 1)) {
		schedule_delayed_work(&epld->polling_work, msecs_to_jiffies(polling_time));
	}

#if HS_ENABLE
	if (enable_hs) {
		schedule_delayed_work(&epld->polling_work, msecs_to_jiffies(20));
		epl_sensor_read_hs(client);
	}
#if PS_GES  /*PS_GES*/
	else if (enable_ges && epl_sensor.ges.polling_mode == 1)
#endif /*PS_GES*/
#endif

#if PS_GES
#if !HS_ENABLE /*HS_ENABLE*/
	if (enable_ges && epl_sensor.ges.polling_mode == 1) {
#endif  /*HS_ENABLE*/
		schedule_delayed_work(&epld->polling_work, msecs_to_jiffies(polling_time));
		epl_sensor_read_ps(epld->client);
		epl_sensor_read_ps_status(epld->client);
	}
#endif

	if (enable_als &&  epl_sensor.als.polling_mode == 1) {
		int report_lux = 0;
#if PS_DYN_K && PS_DYN_K_STR
		if (polling_flag == true && eint_flag == true && dynk_enhance_flag == true)

		if (polling_flag == true && eint_flag == true) {
			mutex_lock(&sensor_mutex);

			epl_sensor_read_als(client);
			mutex_unlock(&sensor_mutex);
			report_lux = epl_sensor_get_als_value(epld, epl_sensor.als.data.channels[1]);
			if (epl_sensor.als.report_type != CMC_BIT_DYN_INT) {
				epl_sensor_report_lux(report_lux);
			}
		}
	}

	if (enable_ps && epl_sensor.ps.polling_mode == 1) {
#if PS_DYN_K && PS_DYN_K_STR
		if (polling_flag == true && eint_flag == true && dynk_enhance_flag == true)
#else
		if (polling_flag == true && eint_flag == true) {
			mutex_lock(&sensor_mutex);
			epl_sensor_read_ps_status(client);
			epl_sensor_read_ps(client);
			mutex_unlock(&sensor_mutex);
#if PS_DYN_K
			epl_sensor_reset_dynk_thd(dynk_last_status, (epl_sensor.ps.compare_low >> 3));
			dynk_last_status = (epl_sensor.ps.compare_low >> 3);
#endif
			epl_sensor_report_ps_status();
		}
	}
#if HS_ENABLE && PS_GES
	if (enable_als == false && enable_ps == false && enable_hs == false && enable_ges == false)
#elif HS_ENABLE
	if (enable_als == false && enable_ps == false && enable_hs == false)
#elif PS_GES
	if (enable_als == false && enable_ps == false && enable_ges == false)
#else
	if (enable_als == false && enable_ps == false) {
		cancel_delayed_work(&epld->polling_work);
		LOG_INFO("disable sensor\n");
	}

}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static irqreturn_t epl_sensor_eint_func(int irqNo, void *handle)
{
	struct epl_sensor_priv *epld = (struct epl_sensor_priv *)handle;
	disable_irq_nosync(epld->irq);
	schedule_delayed_work(&epld->eint_work, 0);

	return IRQ_HANDLED;
}
/*----------------------------------------------------------------------------*/
static void epl_sensor_intr_als_report_lux(void)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int report_lux = 0;
	LOG_INFO("[%s]: IDEL MODE \r\n", __func__);
	epl_sensor_I2C_Write(epld->client, 0x00, epl_sensor.wait | EPL_MODE_IDLE);

	epl_sensor_read_als(epld->client);

	report_lux = epl_sensor_get_als_value(epld, epl_sensor.als.data.channels[1]);
	epl_sensor_report_lux(report_lux);

	epl_sensor.als.compare_reset = EPL_CMP_RESET;
	epl_sensor.als.lock = EPL_UN_LOCK;
	epl_sensor_I2C_Write(epld->client, 0x12, epl_sensor.als.compare_reset | epl_sensor.als.lock);


	if (epl_sensor.als.compare_high >> 4) {
		epl_sensor.als.high_threshold = epl_sensor.als.high_threshold + 250;
		epl_sensor.als.low_threshold = epl_sensor.als.low_threshold + 250;

		if (epl_sensor.als.high_threshold > 60000) {
			epl_sensor.als.high_threshold = epl_sensor.als.high_threshold - 250;
			epl_sensor.als.low_threshold = epl_sensor.als.low_threshold - 250;
		}
	}
	if (epl_sensor.als.compare_low >> 3) {
		epl_sensor.als.high_threshold = epl_sensor.als.high_threshold - 250;
		epl_sensor.als.low_threshold = epl_sensor.als.low_threshold - 250;

		if (epl_sensor.als.high_threshold < 250) {
			epl_sensor.als.high_threshold = epl_sensor.als.high_threshold + 250;
			epl_sensor.als.low_threshold = epl_sensor.als.low_threshold + 250;
		}
	}

	if (epl_sensor.als.high_threshold < epl_sensor.als.low_threshold) {
		LOG_INFO("[%s]:recover default setting \r\n", __FUNCTION__);
		epl_sensor.als.high_threshold = ALS_HIGH_THRESHOLD;
		epl_sensor.als.low_threshold = ALS_LOW_THRESHOLD;
	}


	set_lsensor_intr_threshold(epl_sensor.als.low_threshold, epl_sensor.als.high_threshold);

	epl_sensor_I2C_Write(epld->client, 0x00, epl_sensor.wait | epl_sensor.mode);
	LOG_INFO("[%s]: MODE= 0x%x \r\n", __func__, epl_sensor.mode);
}

/*----------------------------------------------------------------------------*/
static void epl_sensor_eint_work(struct work_struct *work)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	bool enable_ps = epld->enable_pflag == 1 && epld->ps_suspend == 0;
	bool enable_als = epld->enable_lflag == 1 && epld->als_suspend == 0;
#if PS_GES
	bool enable_ges = epld->enable_gflag == 1 && epld->ges_suspend == 0;
#endif
	eint_flag = false;
	mutex_lock(&sensor_mutex);

	LOG_INFO("xxxxxxxxxxx\n\n");

	if (low_trigger_flag == true) {
		LOG_INFO("[%s]: low trigger is ignore........\r\n", __func__);
		goto exit;
	}
#if PS_GES
	if (enable_ges && epl_sensor.ges.polling_mode == 0) {
		epl_sensor_read_ps_status(epld->client);
		if (polling_flag == true) {

			epl_sensor.ges.compare_reset = EPL_CMP_RUN;
			epl_sensor.ges.lock = EPL_UN_LOCK;
			epl_sensor_I2C_Write(epld->client, 0x1b, epl_sensor.ges.compare_reset | epl_sensor.ges.lock);
		} else {
			low_trigger_flag = true;
		}
	}
#endif
	if (enable_ps && epl_sensor.ps.polling_mode == 0) {
		epl_sensor_read_ps_status(epld->client);
		if (epl_sensor.ps.interrupt_flag == EPL_INT_TRIGGER) {
#if PS_DEBUG || PS_DYN_K
			epl_sensor_read_ps(epld->client);
#endif

#if PS_DYN_K
			epl_sensor_reset_dynk_thd(dynk_last_status, (epl_sensor.ps.compare_low >> 3));
			dynk_last_status = (epl_sensor.ps.compare_low >> 3);
#endif
			epl_sensor_report_ps_status();

			if (polling_flag == true) {


				epl_sensor.ps.compare_reset = EPL_CMP_RUN;
				epl_sensor.ps.lock = EPL_UN_LOCK;
				epl_sensor_I2C_Write(epld->client, 0x1b, epl_sensor.ps.compare_reset | epl_sensor.ps.lock);
			} else {
				low_trigger_flag = true;
			}

		}
#if PS_DYN_K_STR
		if (enable_als == 0 && epl_sensor.als.polling_mode == 0) {

			epl_sensor.als.compare_reset = EPL_CMP_RUN;
			epl_sensor.als.lock = EPL_UN_LOCK;
			epl_sensor_I2C_Write(epld->client, 0x12, epl_sensor.als.compare_reset | epl_sensor.als.lock);
		}
#endif
	}

	if (enable_als && epl_sensor.als.polling_mode == 0) {
		epl_sensor_read_als_status(epld->client);
		if (epl_sensor.als.interrupt_flag == EPL_INT_TRIGGER) {
			epl_sensor_intr_als_report_lux();
			if (polling_flag == true) {
				epl_sensor.als.compare_reset = EPL_CMP_RUN;
				epl_sensor.als.lock = EPL_UN_LOCK;
				epl_sensor_I2C_Write(epld->client, 0x12, epl_sensor.als.compare_reset | epl_sensor.als.lock);
			} else {
				low_trigger_flag = true;
			}
		}
	}

exit:
	mutex_unlock(&sensor_mutex);
	eint_flag = true;
	enable_irq(epld->irq);
}
/*----------------------------------------------------------------------------*/
static int epl_sensor_setup_interrupt(struct epl_sensor_priv *epld)
{
	struct i2c_client *client = epld->client;
	int err = 0;
#if QCOM
	unsigned int irq_gpio;
	unsigned int irq_gpio_flags;
	struct device_node *np = client->dev.of_node;
#endif
	msleep(5);
#if S5PV210
	err = gpio_request(S5PV210_GPH0(1), "Elan EPL IRQ");
	if (err) {
		LOG_ERR("gpio pin request fail (%d)\n", err);
		goto initial_fail;
	} else {
		LOG_INFO("----- Samsung gpio config success -----\n");
		s3c_gpio_cfgpin(S5PV210_GPH0(1), S3C_GPIO_SFN(0x0F)/*(S5PV210_GPH0_1_EXT_INT30_1) */);
		s3c_gpio_setpull(S5PV210_GPH0(1), S3C_GPIO_PULL_UP);

	}
#elif SPREAD
	epld->intr_pin = ELAN_INT_PIN; /*need setting*/
	err = gpio_request(epld->intr_pin, "Elan EPL IRQ");
	if (err) {
		LOG_ERR("gpio pin request fail (%d)\n", err);
		goto initial_fail;
	} else {
		gpio_direction_input(epld->intr_pin);

		/*get irq*/
		client->irq = gpio_to_irq(epld->intr_pin);
		epld->irq = client->irq;

		LOG_INFO("IRQ number is %d\n", client->irq);

	}
#elif QCOM

	irq_gpio = of_get_named_gpio_flags(np, "epl, irq-gpio", 0, &irq_gpio_flags);

	epld->intr_pin = irq_gpio;
	if (epld->intr_pin < 0) {
		goto initial_fail;
	}

	if (gpio_is_valid(epld->intr_pin)) {
			err = gpio_request(epld->intr_pin, "epl_irq_gpio");
			if (err) {
				LOG_ERR("irq gpio request failed");
				goto initial_fail;
			}

			err = gpio_direction_input(epld->intr_pin);
			if (err) {
					LOG_ERR("set_direction for irq gpio failed\n");
					goto initial_fail;
			}
	}
#elif LEADCORE

		epld->intr_pin = irq_to_gpio(client->irq); /*need confirm*/
		err = gpio_request(epld->intr_pin, "epl irq");

		if (err < 0) {
			LOG_ERR("%s:Gpio request failed! \r\n", __func__);
			goto initial_fail;
		}
		gpio_direction_input(epld->intr_pin);
		client->irq = gpio_to_irq(epld->intr_pin);
#elif MARVELL
   epld->intr_pin = ELAN_INT_PIN; /*need setting*/

   if (client->irq <= 0) {
		LOG_ERR("client->irq(%d) Failed \r\n", client->irq);
		goto initial_fail;
  }
   gpio_request(epld->intr_pin, "epl irq");
   gpio_direction_input(epld->intr_pin);

#endif
	err = request_irq(epld->irq, epl_sensor_eint_func, IRQF_TRIGGER_FALLING,
					  client->dev.driver->name, epld);
	if (err < 0) {
		LOG_ERR("request irq pin %d fail for gpio\n", err);
		goto fail_free_intr_pin;
	}

	return err;

initial_fail:
fail_free_intr_pin:
	gpio_free(epld->intr_pin);

	return err;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/

static ssize_t epl_sensor_show_reg(struct device *dev, struct device_attribute *attr, char *buf)
{

	ssize_t len = 0;
	struct i2c_client *client = epl_sensor_obj->client;

	if (!epl_sensor_obj) {
		LOG_ERR("epl_obj is null!!\n");
		return 0;
	}

	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x00 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x00));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x01 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x01));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x02 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x02));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x03 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x03));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x04 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x04));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x05 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x05));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x06 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x06));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x07 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x07));
	if (epl_sensor.als.polling_mode == 0) {
		len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x08 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x08));
		len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x09 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x09));
		len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0A value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x0A));
		len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0B value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x0B));
	}
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0C value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x0C));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0D value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x0D));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0E value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x0E));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x0F value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x0F));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x11 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x11));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x12 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x12));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x1B value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x1B));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x22 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x22));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x23 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x23));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x24 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x24));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0x25 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x25));
	len += snprintf(buf+len, PAGE_SIZE-len, "chip id REG 0xFC value = 0x%x\n", i2c_smbus_read_byte_data(client, 0xFC));

	return len;

}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_show_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct epl_sensor_priv *epld = epl_sensor_obj;
	bool enable_ps = epld->enable_pflag == 1 && epld->ps_suspend == 0;
	bool enable_als = epld->enable_lflag == 1 && epld->als_suspend == 0;
#if HS_ENABLE
	bool enable_hs = epld->enable_hflag == 1 && epld->hs_suspend == 0;
#endif

	if (!epl_sensor_obj) {
		LOG_ERR("epl_sensor_obj is null!!\n");
		return 0;
	}

	len += snprintf(buf+len, PAGE_SIZE-len, "chip is %s, ver is %s \n", EPL_DEV_NAME, DRIVER_VERSION);
	len += snprintf(buf+len, PAGE_SIZE-len, "als/ps polling is %d-%d\n", epl_sensor.als.polling_mode, epl_sensor.ps.polling_mode);
	len += snprintf(buf+len, PAGE_SIZE-len, "wait = %d, mode = %d\n", epl_sensor.wait >> 4, epl_sensor.mode);
	len += snprintf(buf+len, PAGE_SIZE-len, "interrupt control = %d\n", epl_sensor.interrupt_control >> 4);
	len += snprintf(buf+len, PAGE_SIZE-len, "frame time ps=%dms, als=%dms\n", ps_frame_time, als_frame_time);
#if HS_ENABLE
	if (enable_hs) {
		len += snprintf(buf+len, PAGE_SIZE-len, "hs adc= %d\n", epl_sensor.hs.adc>>3);
		len += snprintf(buf+len, PAGE_SIZE-len, "hs int_time= %d\n", epl_sensor.hs.integration_time>>2);
		len += snprintf(buf+len, PAGE_SIZE-len, "hs cycle= %d\n", epl_sensor.hs.cycle);
		len += snprintf(buf+len, PAGE_SIZE-len, "hs gain= %d\n", epl_sensor.hs.gain);
		len += snprintf(buf+len, PAGE_SIZE-len, "hs ch1 raw= %d\n", epl_sensor.hs.raw);
	}
#endif
	if (enable_ps) {
		len += snprintf(buf+len, PAGE_SIZE-len, "PS: \n");
		len += snprintf(buf+len, PAGE_SIZE-len, "INTEG = %d, gain = %d\n", epl_sensor.ps.integration_time >> 2, epl_sensor.ps.gain);
		len += snprintf(buf+len, PAGE_SIZE-len, "ADC = %d, cycle = %d, ir drive = %d\n", epl_sensor.ps.adc >> 3, epl_sensor.ps.cycle, epl_sensor.ps.ir_drive);
		len += snprintf(buf+len, PAGE_SIZE-len, "saturation = %d, int flag = %d\n", epl_sensor.ps.saturation >> 5, epl_sensor.ps.interrupt_flag >> 2);
		len += snprintf(buf+len, PAGE_SIZE-len, "Thr(L/H) = (%d/%d)\n", epl_sensor.ps.low_threshold, epl_sensor.ps.high_threshold);
#if PS_DYN_K
#if PS_DYN_K_STR
		len += snprintf(buf+len, PAGE_SIZE-len, "Dyn enhance ch0 = %d \n", dynk_enhance_ch0);
#endif
		len += snprintf(buf+len, PAGE_SIZE-len, "Dyn thr(L/H) = (%ld/%ld)\n", (long)dynk_thd_low, (long)dynk_thd_high);
#endif
		len += snprintf(buf+len, PAGE_SIZE-len, "pals data = %d, data = %d\n", epl_sensor.ps.data.ir_data, epl_sensor.ps.data.data);
	}
	if (enable_als) {
		len += snprintf(buf+len, PAGE_SIZE-len, "ALS: \n");
		len += snprintf(buf+len, PAGE_SIZE-len, "INTEG = %d, gain = %d\n", epl_sensor.als.integration_time >> 2, epl_sensor.als.gain);
		len += snprintf(buf+len, PAGE_SIZE-len, "ADC = %d, cycle = %d\n", epl_sensor.als.adc >> 3, epl_sensor.als.cycle);
#if ALS_DYN_INTT
		if (epl_sensor.als.lsrc_type != CMC_BIT_LSRC_NON) {
			len += snprintf(buf+len, PAGE_SIZE-len, "lsource_thd_low=%d, lsource_thd_high=%d \n", epld->lsource_thd_low, epld->lsource_thd_high);
			len += snprintf(buf+len, PAGE_SIZE-len, "saturation = %d\n", epl_sensor.als.saturation >> 5);

			if (epl_sensor.als.lsrc_type == CMC_BIT_LSRC_SCALE || epl_sensor.als.lsrc_type == CMC_BIT_LSRC_BOTH) {
				len += snprintf(buf+len, PAGE_SIZE-len, "real_ratio = %d\n", epld->ratio);
				len += snprintf(buf+len, PAGE_SIZE-len, "use_ratio = %d\n", epld->last_ratio);
			} else if (epl_sensor.als.lsrc_type == CMC_BIT_LSRC_SLOPE) {
				len += snprintf(buf+len, PAGE_SIZE-len, "ratio = %d\n", epld->ratio);
			}
		}
		if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
			len += snprintf(buf+len, PAGE_SIZE-len, "c_gain = %d\n", c_gain);
			len += snprintf(buf+len, PAGE_SIZE-len, "epl_sensor.als.dyn_intt_raw = %d, dynamic_intt_lux = %d\n", epl_sensor.als.dyn_intt_raw, dynamic_intt_lux);
		}
#endif
		if (epl_sensor.als.polling_mode == 0)
			len += snprintf(buf+len, PAGE_SIZE-len, "Thr(L/H) = (%d/%d)\n", epl_sensor.als.low_threshold, epl_sensor.als.high_threshold);
		len += snprintf(buf+len, PAGE_SIZE-len, "ch0 = %d, ch1 = %d\n", epl_sensor.als.data.channels[0], epl_sensor.als.data.channels[1]);
	}

	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_als_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	uint16_t mode = 0;
	struct epl_sensor_priv *epld = epl_sensor_obj;
	LOG_FUN();
	sscanf(buf, "%hu", &mode);
	if (epld->enable_lflag != mode) {
#if ALS_DYN_INTT
		if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
			dynamic_intt_idx = dynamic_intt_init_idx;
			epl_sensor.als.integration_time = als_dynamic_intt_intt[dynamic_intt_idx];
			epl_sensor.als.gain = als_dynamic_intt_gain[dynamic_intt_idx];
			dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
			dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];
		}
#endif
		epld->enable_lflag = mode;

		epl_sensor_update_mode(epld->client);
	}

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_ps_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	uint16_t mode = 0;
	struct epl_sensor_priv *epld = epl_sensor_obj;
#if HS_ENABLE
	bool enable_hs = epld->enable_hflag == 1 && epld->hs_suspend == 0;
#endif
#if PS_GES
	bool enable_ges = epld->enable_gflag == 1 && epld->ges_suspend == 0;
#endif
	LOG_FUN();

	sscanf(buf, "%hu", &mode);
#if HS_ENABLE
	if (enable_hs == 1 && mode == 1) {
		epld->enable_hflag = 0;
		if (hs_enable_flag == true) {
			epld->enable_lflag = 1;
			hs_enable_flag = false;
		}
		write_global_variable(epld->client);
		LOG_INFO("[%s] Disable HS and recover ps setting \r\n", __func__);
	}
#endif
#if PS_GES
	if (enable_ges == 1 && mode == 1) {
		epld->enable_gflag = 0;
		write_global_variable(epld->client);
		ps_ges_enable_flag = true;
		LOG_INFO("[%s] Disable GES and recover ps setting \r\n", __func__);
	} else if (ps_ges_enable_flag == true && mode == 0) {
		epld->enable_gflag = 1;
		write_global_variable(epld->client);
		ps_ges_enable_flag = false;
		LOG_INFO("[%s] enable GES and recover ges setting \r\n", __func__);
	}
#endif
	if (epld->enable_pflag != mode) {
		epld->enable_pflag = mode;
		if (mode) {
			wake_lock(&ps_lock);
#if PS_DYN_K
			dynk_min_ps_raw_data = 0xffff;
			dynk_change_flag = false;
#if PS_DYN_K_STR
			dynk_enhance_flag = false;
#endif
#endif
		} else {
#if PS_DYN_K
			cancel_delayed_work(&epld->dynk_thd_polling_work);
#endif
			wake_unlock(&ps_lock);
		}

		epl_sensor_update_mode(epld->client);
	}

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_show_cal_raw(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	u16 ch1 = 0;
	u32 ch1_all = 0;
	int count = 5;
	int i;
	ssize_t len = 0;
#if !PS_DYN_K
	bool enable_ps = epld->enable_pflag == 1 && epld->ps_suspend == 0;
#endif

	if (!epl_sensor_obj) {
		LOG_ERR("epl_sensor_obj is null!!\n");
		return 0;
	}

	for (i = 0; i < count; i++) {
		switch (epl_sensor.mode) {
		msleep(50);
#if PS_DYN_K_STR
		case EPL_MODE_ALS_PS:
#else
		case EPL_MODE_PS:
#endif
#if !PS_DYN_K
			if (enable_ps == true && polling_flag == true && eint_flag == true && epl_sensor.ps.polling_mode == 0)
				epl_sensor_read_ps(epld->client);
#endif
			ch1 = epl_sensor.ps.data.data;
			break;

		case EPL_MODE_ALS:
			if (epl_sensor.als.polling_mode == 0)
				epl_sensor_read_als(epld->client);
			ch1 = epl_sensor.als.data.channels[1];
			break;
	}

	ch1_all = ch1_all + ch1;
	if (epl_sensor.wait == EPL_WAIT_SINGLE)
		epl_sensor_I2C_Write(epld->client, 0x11, epl_sensor.power | epl_sensor.reset);
	}
	ch1 = (u16)(ch1_all/count);
	LOG_INFO("cal_raw = %d \r\n" , ch1);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d \r\n", ch1);

	return  len;
}


/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_threshold(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int hthr = 0, lthr = 0;
	if (!epld) {
		LOG_ERR("epl_sensor_obj is null!!\n");
		return 0;
	}

	switch (epl_sensor.mode) {
#if PS_DYN_K_STR
	case EPL_MODE_ALS_PS:
#else
	case EPL_MODE_PS:
#endif
		sscanf(buf, "%d, %d", &lthr, &hthr);
		epl_sensor.ps.low_threshold = lthr;
		epl_sensor.ps.high_threshold = hthr;
		set_psensor_intr_threshold(epl_sensor.ps.low_threshold, epl_sensor.ps.high_threshold);
		break;

	case EPL_MODE_ALS:
		sscanf(buf, "%d, %d", &lthr, &hthr);
		epl_sensor.als.low_threshold = lthr;
		epl_sensor.als.high_threshold = hthr;
		set_lsensor_intr_threshold(epl_sensor.als.low_threshold, epl_sensor.als.high_threshold);
		break;

	}
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_wait_time(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	sscanf(buf, "%d", &val);

	epl_sensor.wait = (val & 0xf) << 4;

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_gain(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int value = 0;
	LOG_FUN();

	sscanf(buf, "%d", &value);

	value = value & 0x03;

	switch (epl_sensor.mode) {
#if PS_DYN_K_STR
	case EPL_MODE_ALS_PS:
#else
	case EPL_MODE_PS:
#endif
		epl_sensor.ps.gain = value;
		epl_sensor_I2C_Write(epld->client, 0x03, epl_sensor.ps.integration_time | epl_sensor.ps.gain);
		break;

	case EPL_MODE_ALS:
		epl_sensor.als.gain = value;
		epl_sensor_I2C_Write(epld->client, 0x01, epl_sensor.als.integration_time | epl_sensor.als.gain);
		break;
	}

	epl_sensor_update_mode(epld->client);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int value = 0;

	LOG_FUN();
	epld->enable_pflag = 0;
	epld->enable_lflag = 0;
	sscanf(buf, "%d", &value);

	switch (value) {
	case 0:
		epl_sensor.mode = EPL_MODE_IDLE;
		break;

	case 1:
		epld->enable_lflag = 1;
		epl_sensor.mode = EPL_MODE_ALS;
		break;

	case 2:
		epld->enable_pflag = 1;
		epl_sensor.mode = EPL_MODE_PS;
		break;

	case 3:
		epld->enable_lflag = 1;
		epld->enable_pflag = 1;
		epl_sensor.mode = EPL_MODE_ALS_PS;
		break;
	}

	epl_sensor_update_mode(epld->client);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_ir_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();
	sscanf(buf, "%d", &value);

	switch (epl_sensor.mode) {
#if PS_DYN_K_STR
	case EPL_MODE_ALS_PS:
#else
	case EPL_MODE_PS:
#endif
		switch (value) {
		case 0:
			epl_sensor.ps.ir_mode = EPL_IR_MODE_CURRENT;
			break;

		case 1:
			epl_sensor.ps.ir_mode = EPL_IR_MODE_VOLTAGE;
			break;
		}

			epl_sensor_I2C_Write(epld->client, 0x05, epl_sensor.ps.ir_on_control | epl_sensor.ps.ir_mode | epl_sensor.ps.ir_drive);
		break;
	}

	epl_sensor_I2C_Write(epld->client, 0x00, epl_sensor.wait | epl_sensor.mode);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_ir_contrl(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	uint8_t  data;
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();
	sscanf(buf, "%d", &value);

	switch (epl_sensor.mode) {
#if PS_DYN_K_STR
	case EPL_MODE_ALS_PS:
#else
	case EPL_MODE_PS:
#endif
		switch (value) {
		case 0:
			epl_sensor.ps.ir_on_control = EPL_IR_ON_CTRL_OFF;
			break;
		case 1:
			epl_sensor.ps.ir_on_control = EPL_IR_ON_CTRL_ON;
			break;
		}

		data = epl_sensor.ps.ir_on_control | epl_sensor.ps.ir_mode | epl_sensor.ps.ir_drive;
		LOG_INFO("[%s]: 0x05 = 0x%x\n", __FUNCTION__, data);

		epl_sensor_I2C_Write(epld->client, 0x05, epl_sensor.ps.ir_on_control | epl_sensor.ps.ir_mode | epl_sensor.ps.ir_drive);
		break;
	}

	epl_sensor_I2C_Write(epld->client, 0x00, epl_sensor.wait | epl_sensor.mode);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_ir_drive(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();
	sscanf(buf, "%d", &value);

	switch (epl_sensor.mode) {
#if PS_DYN_K_STR
	case EPL_MODE_ALS_PS:
#else
	case EPL_MODE_PS:
#endif
		epl_sensor.ps.ir_drive = (value & 0x03);
		epl_sensor_I2C_Write(epld->client, 0x05, epl_sensor.ps.ir_on_control | epl_sensor.ps.ir_mode | epl_sensor.ps.ir_drive);
		break;
	}

	epl_sensor_I2C_Write(epld->client, 0x00, epl_sensor.wait | epl_sensor.mode);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_interrupt_type(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	struct epl_sensor_priv *epld = epl_sensor_obj;
	LOG_FUN();
	sscanf(buf, "%d", &value);

	switch (epl_sensor.mode) {
#if PS_DYN_K_STR
	case EPL_MODE_ALS_PS:
#else
	case EPL_MODE_PS:
#endif
		if (!epl_sensor.ps.polling_mode) {
			epl_sensor.ps.interrupt_type = value & 0x03;
			epl_sensor_I2C_Write(epld->client, 0x06, epl_sensor.interrupt_control | epl_sensor.ps.persist | epl_sensor.ps.interrupt_type);
			LOG_INFO("[%s]: 0x06 = 0x%x\n", __FUNCTION__, epl_sensor.interrupt_control | epl_sensor.ps.persist | epl_sensor.ps.interrupt_type);
		}
		break;

	case EPL_MODE_ALS:
		if (!epl_sensor.als.polling_mode) {
			epl_sensor.als.interrupt_type = value & 0x03;
			epl_sensor_I2C_Write(epld->client, 0x07, epl_sensor.als.interrupt_channel_select | epl_sensor.als.persist | epl_sensor.als.interrupt_type);
			LOG_INFO("[%s]: 0x07 = 0x%x\n", __FUNCTION__, epl_sensor.als.interrupt_channel_select | epl_sensor.als.persist | epl_sensor.als.interrupt_type);
		}
		break;
	}

	return count;
}

/*----------------------------------------------------------------------------*/

static ssize_t epl_sensor_store_ps_polling_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int polling_mode = 0;


	sscanf(buf, "%d", &polling_mode);
	epl_sensor.ps.polling_mode = polling_mode;
#if PS_GES
	epl_sensor.ges.polling_mode = polling_mode;
#endif

	epl_sensor_update_mode(epld->client);

	return count;
}

static ssize_t epl_sensor_store_als_polling_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int polling_mode = 0;

	sscanf(buf, "%d", &polling_mode);
	epl_sensor.als.polling_mode = polling_mode;

	epl_sensor_update_mode(epld->client);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_integration(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
#if ALS_DYN_INTT
	int value1 = 0;
#endif
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();



	switch (epl_sensor.mode) {
#if PS_DYN_K_STR
	case EPL_MODE_ALS_PS:
#else
	case EPL_MODE_PS:
#endif
		sscanf(buf, "%d", &value);

		epl_sensor.ps.integration_time = (value & 0xf) << 2;
		epl_sensor_I2C_Write(epld->client, 0x03, epl_sensor.ps.integration_time | epl_sensor.ps.gain);
		epl_sensor_I2C_Read(epld->client, 0x03, 1);
		LOG_INFO("[%s]: 0x03 = 0x%x (0x%x)\n", __FUNCTION__, epl_sensor.ps.integration_time | epl_sensor.ps.gain, gRawData.raw_bytes[0]);
		break;

	case EPL_MODE_ALS:
#if ALS_DYN_INTT

		if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
			sscanf(buf, "%d, %d", &value, &value1);

			als_dynamic_intt_intt[0] = (value & 0xf) << 2;
			als_dynamic_intt_value[0] = als_intt_value[value];

			als_dynamic_intt_intt[1] = (value1 & 0xf) << 2;
			als_dynamic_intt_value[1] = als_intt_value[value1];
			LOG_INFO("[%s]: als_dynamic_intt_value=%d, %d \r\n", __func__, als_dynamic_intt_value[0], als_dynamic_intt_value[1]);

			dynamic_intt_idx = dynamic_intt_init_idx;
			epl_sensor.als.integration_time = als_dynamic_intt_intt[dynamic_intt_idx];
			epl_sensor.als.gain = als_dynamic_intt_gain[dynamic_intt_idx];
			dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
			dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];
		} else {
			sscanf(buf, "%d", &value);
			epl_sensor.als.integration_time = (value & 0xf) << 2;
			epl_sensor_I2C_Write(epld->client, 0x01, epl_sensor.als.integration_time | epl_sensor.als.gain);
			epl_sensor_I2C_Read(epld->client, 0x01, 1);
			LOG_INFO("[%s]: 0x01 = 0x%x (0x%x)\n", __FUNCTION__, epl_sensor.als.integration_time | epl_sensor.als.gain, gRawData.raw_bytes[0]);
		}
		break;
	}

	epl_sensor_update_mode(epld->client);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_adc(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();
	sscanf(buf, "%d", &value);

	switch (epl_sensor.mode) {
#if PS_DYN_K_STR
	case EPL_MODE_ALS_PS:
#else
	case EPL_MODE_PS:
#endif
		epl_sensor.ps.adc = (value & 0x3) << 3;
		epl_sensor_I2C_Write(epld->client, 0x04, epl_sensor.ps.adc | epl_sensor.ps.cycle);
		epl_sensor_I2C_Read(epld->client, 0x04, 1);
		LOG_INFO("[%s]:0x04 = 0x%x (0x%x)\n", __FUNCTION__, epl_sensor.ps.adc | epl_sensor.ps.cycle, gRawData.raw_bytes[0]);
		break;

	case EPL_MODE_ALS:
		epl_sensor.als.adc = (value & 0x3) << 3;
		epl_sensor_I2C_Write(epld->client, 0x02, epl_sensor.als.adc | epl_sensor.als.cycle);
		epl_sensor_I2C_Read(epld->client, 0x02, 1);
		LOG_INFO("[%s]:0x02 = 0x%x (0x%x)\n", __FUNCTION__, epl_sensor.als.adc | epl_sensor.als.cycle, gRawData.raw_bytes[0]);
		break;
	}

	epl_sensor_update_mode(epld->client);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_cycle(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();
	sscanf(buf, "%d", &value);

	switch (epl_sensor.mode) {
#if PS_DYN_K_STR
	case EPL_MODE_ALS_PS:
#else
	case EPL_MODE_PS:
#endif
		epl_sensor.ps.cycle = (value & 0x7);
		epl_sensor_I2C_Write(epld->client, 0x04, epl_sensor.ps.adc | epl_sensor.ps.cycle);
		LOG_INFO("[%s]:0x04 = 0x%x (0x%x)\n", __FUNCTION__, epl_sensor.ps.adc | epl_sensor.ps.cycle, gRawData.raw_bytes[0]);
		break;

	case EPL_MODE_ALS:
		epl_sensor.als.cycle = (value & 0x7);
		epl_sensor_I2C_Write(epld->client, 0x02, epl_sensor.als.adc | epl_sensor.als.cycle);
		LOG_INFO("[%s]:0x02 = 0x%x (0x%x)\n", __FUNCTION__, epl_sensor.als.adc | epl_sensor.als.cycle, gRawData.raw_bytes[0]);
		break;
	}

	epl_sensor_update_mode(epld->client);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_als_report_type(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;

	LOG_FUN();

	sscanf(buf, "%d", &value);
	epl_sensor.als.report_type = value & 0xf;

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_ps_w_calfile(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int ps_hthr = 0, ps_lthr = 0, ps_cancelation = 0;
	int ps_cal_len = 0;
	char ps_calibration[20];
	LOG_FUN();

	if (!epl_sensor_obj) {
		LOG_ERR("epl_obj is null!!\n");
		return 0;
	}
	sscanf(buf, "%d, %d, %d", &ps_cancelation, &ps_hthr, &ps_lthr);

	ps_cal_len = sprintf(ps_calibration, "%d, %d, %d", ps_cancelation, ps_hthr, ps_lthr);

	write_factory_calibration(epld, ps_calibration, ps_cal_len);
	return count;
}
/*----------------------------------------------------------------------------*/

static ssize_t epl_sensor_store_reg_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int reg;
	int data;
	LOG_FUN();

	sscanf(buf, "%x, %x", &reg, &data);
	LOG_INFO("[%s]: reg= 0x%x, data= 0x%x", __func__, reg, data);
	epl_sensor_I2C_Write(epld->client, reg, data);

	return count;
}

static ssize_t epl_sensor_store_unlock(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int mode;
	LOG_FUN();

	sscanf(buf, "%d", &mode);

	LOG_INFO("mode = %d \r\n", mode);
	switch (mode) {
	case 0:
		epl_sensor.ps.compare_reset = EPL_CMP_RUN;
		epl_sensor.ps.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x1b, epl_sensor.ps.compare_reset | epl_sensor.ps.lock);
		break;

	case 1:
		epl_sensor.als.compare_reset = EPL_CMP_RUN;
		epl_sensor.als.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x12, epl_sensor.als.compare_reset | epl_sensor.als.lock);
		break;

	case 2:
		epl_sensor.als.compare_reset = EPL_CMP_RESET;
		epl_sensor.als.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x12, epl_sensor.als.compare_reset | epl_sensor.als.lock);
		break;

	case 3:
		epl_sensor.ps.compare_reset = EPL_CMP_RUN;
		epl_sensor.ps.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x1b, epl_sensor.ps.compare_reset | epl_sensor.ps.lock);

		epl_sensor.als.compare_reset = EPL_CMP_RUN;
		epl_sensor.als.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x12, epl_sensor.als.compare_reset | epl_sensor.als.lock);
		break;
	}
	/*double check PS or ALS lock*/


	return count;
}

static ssize_t epl_sensor_store_als_ch_sel(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int ch_sel;
	LOG_FUN();

	sscanf(buf, "%d", &ch_sel);

	LOG_INFO("channel selection = %d \r\n", ch_sel);
	switch (ch_sel) {
	case 0:
		epl_sensor.als.interrupt_channel_select = EPL_ALS_INT_CHSEL_0;
	break;
	case 1:
		epl_sensor.als.interrupt_channel_select = EPL_ALS_INT_CHSEL_1;
	break;
	}
	epl_sensor_I2C_Write(epld->client, 0x07, epl_sensor.als.interrupt_channel_select | epl_sensor.als.persist | epl_sensor.als.interrupt_type);

	epl_sensor_update_mode(epld->client);

	return count;
}

static ssize_t epl_sensor_store_ps_cancelation(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int cancelation;
	LOG_FUN();

	sscanf(buf, "%d", &cancelation);

	epl_sensor.ps.cancelation = cancelation;

	LOG_INFO("epl_sensor.ps.cancelation = %d \r\n", epl_sensor.ps.cancelation);

	epl_sensor_I2C_Write(epld->client, 0x22, (u8)(epl_sensor.ps.cancelation & 0xff));
	epl_sensor_I2C_Write(epld->client, 0x23, (u8)((epl_sensor.ps.cancelation & 0xff00) >> 8));

	return count;
}

static ssize_t epl_sensor_show_ps_polling(struct device *dev, struct device_attribute *attr, char *buf)
{
	u16 *tmp = (u16 *)buf;
	tmp[0] = epl_sensor.ps.polling_mode;
	return 2;
}

static ssize_t epl_sensor_show_als_polling(struct device *dev, struct device_attribute *attr, char *buf)
{
	u16 *tmp = (u16 *)buf;
	tmp[0] = epl_sensor.als.polling_mode;
	return 2;
}

static ssize_t epl_sensor_show_ps_run_cali(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	ssize_t len = 0;
	int ret;

	LOG_FUN();

	ret = epl_run_ps_calibration(epld);

	len += snprintf(buf+len, PAGE_SIZE-len, "ret = %d\r\n", ret);

	return len;
}

static ssize_t epl_sensor_show_pdata(struct device *dev, struct device_attribute *attr, char *buf)
{
	  struct epl_sensor_priv *epld = epl_sensor_obj;
	  ssize_t len = 0;
	  bool enable_ps = epld->enable_pflag == 1 && epld->ps_suspend == 0;

	  LOG_FUN();

	  if (enable_ps == true && polling_flag == true && eint_flag == true && epl_sensor.ps.polling_mode == 0) {
		mutex_lock(&sensor_mutex);
		epl_sensor_read_ps(epld->client);
		mutex_unlock(&sensor_mutex);
	}

	  LOG_INFO("[%s]: epl_sensor.ps.data.data = %d \r\n", __func__, epl_sensor.ps.data.data);
	  len += snprintf(buf + len, PAGE_SIZE - len, "%d", epl_sensor.ps.data.data);
	  return len;

}

static ssize_t epl_sensor_show_als_data(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	ssize_t len = 0;
	bool enable_als = epld->enable_lflag == 1 && epld->als_suspend == 0;
	LOG_FUN();

#if ALS_DYN_INTT
	if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
		LOG_INFO("[%s]: epl_sensor.als.dyn_intt_raw = %d \r\n", __func__, epl_sensor.als.dyn_intt_raw);
		len += snprintf(buf + len, PAGE_SIZE - len, "%d", epl_sensor.als.dyn_intt_raw);
	} else {
		if (enable_als == true && polling_flag == true && eint_flag == true && epl_sensor.als.polling_mode == 0) {
			mutex_lock(&sensor_mutex);
			epl_sensor_read_als(epld->client);
			mutex_unlock(&sensor_mutex);
		}
		len += snprintf(buf + len, PAGE_SIZE - len, "%d", epl_sensor.als.data.channels[1]);
	}

	return len;
}

#if ALS_DYN_INTT
static ssize_t epl_sensor_store_c_gain(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int c_h, c_l;
	LOG_FUN();

	if (epl_sensor.als.lsrc_type == CMC_BIT_LSRC_NON) {
		sscanf(buf, "%d", &c_h);
		c_gain = c_h;
		LOG_INFO("c_gain = %d \r\n", c_gain);
	} else {
		sscanf(buf, "%d, %d", &c_l, &c_h);
		epld->c_gain_h = c_h;
		epld->c_gain_l = c_l;
	}

	return count;
}

static ssize_t epl_sensor_store_lsrc_type(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int type;
	LOG_FUN();

	sscanf(buf, "%d", &type);

	epl_sensor.als.lsrc_type = type;

	LOG_INFO("epl_sensor.als.lsrc_type = %d \r\n", epl_sensor.als.lsrc_type);

	return count;
}

static ssize_t epl_sensor_store_lsrc_thd(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int lsrc_thrl, lsrc_thrh;
	struct epl_sensor_priv *epld = epl_sensor_obj;
	LOG_FUN();

	sscanf(buf, "%d, %d", &lsrc_thrl, &lsrc_thrh);

	epld->lsource_thd_low = lsrc_thrl;
	epld->lsource_thd_high = lsrc_thrh;

	LOG_INFO("lsource_thd=(%d, %d) \r\n", epld->lsource_thd_low, epld->lsource_thd_high);

	return count;
}

#endif

#if PS_DYN_K
static ssize_t epl_sensor_store_dyn_offset(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int dyn_h, dyn_l;
	LOG_FUN();

	sscanf(buf, "%d, %d", &dyn_l, &dyn_h);

	dynk_low_offset = dyn_l;
	dynk_high_offset = dyn_h;

	return count;
}

static ssize_t epl_sensor_store_dyn_thd_offset(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int thd_offset;
	LOG_FUN();

	sscanf(buf, "%d", &thd_offset);
	dynk_thd_offset = thd_offset;

	return count;
}

static ssize_t epl_sensor_store_dyn_change_thd_max(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int thd_max;
	LOG_FUN();

	sscanf(buf, "%d", &thd_max);
	dynk_change_thd_max = thd_max;

	return count;
}

static ssize_t epl_sensor_store_dyn_max_ir_data(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int max_ir_data;
	LOG_FUN();

	sscanf(buf, "%d", &max_ir_data);
	dynk_max_ir_data = max_ir_data;

	return count;
}

#if PS_DYN_K_STR
static ssize_t epl_sensor_store_dyn_enhance_max_ch0(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	int max_ch0;
	LOG_FUN();

	sscanf(buf, "%d", &max_ch0);
	dynk_enhance_max_ch0 = max_ch0;

	return count;
}

static ssize_t epl_sensor_store_dyn_enhance_gain_intt(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{

	int gain, intt;
	LOG_FUN();

	sscanf(buf, "%d, %d", &gain, &intt);

	dynk_enhance_integration_time = intt;
	dynk_enhance_gain = gain;

	return count;
}
#endif

#endif

#if HS_ENABLE
static ssize_t epl_sensor_show_renvo(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	LOG_FUN();
	LOG_INFO("gRawData.renvo= 0x%x \r\n", epl_sensor.revno);

	len += snprintf(buf+len, PAGE_SIZE-len, "%x", epl_sensor.revno);

	return len;
}

static ssize_t epl_sensor_store_hs_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	uint16_t mode = 0;
	struct epl_sensor_priv *obj = epl_sensor_obj;
	bool enable_ps = obj->enable_pflag == 1 && obj->ps_suspend == 0;
	bool enable_als = obj->enable_lflag == 1 && obj->als_suspend == 0;
	LOG_FUN();

	sscanf(buf, "%hu", &mode);

	if (enable_ps == 0) {
		if (mode > 0) {
			if (enable_als == 1) {
				obj->enable_lflag = 0;
				hs_enable_flag = true;
			}
			epl_sensor.hs.integration_time = epl_sensor.hs.integration_time_max;
			epl_sensor.hs.raws_count = 0;
			obj->enable_hflag = 1;

			if (mode == 2) {
				epl_sensor.hs.dynamic_intt = false;
			} else {
				epl_sensor.hs.dynamic_intt = true;
			}
		} else {
			obj->enable_hflag = 0;
			if (hs_enable_flag == true) {
				obj->enable_lflag = 1;
				hs_enable_flag = false;
			}

		}
		write_global_variable(obj->client);
		epl_sensor_update_mode(obj->client);
	}

	return count;
}

static ssize_t epl_sensor_store_hs_int_time(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *obj = epl_sensor_obj;
	int value;
	u8 intt_buf;

	sscanf(buf, "%d", &value);

	mutex_lock(&hs_sensor_mutex);
	epl_sensor.hs.integration_time = value<<2;
	intt_buf = epl_sensor.hs.integration_time | epl_sensor.hs.gain;
	epl_sensor_I2C_Write(obj->client, 0x03, intt_buf);
	mutex_unlock(&hs_sensor_mutex);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_show_hs_raws(struct device *dev, struct device_attribute *attr, char *buf)
{
	u16 *tmp = (u16 *)buf;
	int byte_count = 2+epl_sensor.hs.raws_count*2;
	int i = 0;
	LOG_FUN();
	mutex_lock(&hs_sensor_mutex);
	tmp[0] = epl_sensor.hs.raws_count;

	for (i = 0; i < epl_sensor.hs.raws_count; i++) {
		tmp[i+1] = epl_sensor.hs.raws[i];
	}

	epl_sensor.hs.raws_count = 0;
	mutex_unlock(&hs_sensor_mutex);

	return byte_count;
}
#endif

#if PS_GES
static ssize_t epl_sensor_store_ges_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ges_enable = 0;
	struct epl_sensor_priv *obj = epl_sensor_obj;
	struct i2c_client *client = obj->client;
	bool enable_ps = obj->enable_pflag == 1 && obj->ps_suspend == 0;
	LOG_FUN();

	sscanf(buf, "%d", &ges_enable);
	LOG_INFO("[%s]: enable_ps=%d, ges_enable=%d \r\n", __func__, enable_ps, ges_enable);

	if (enable_ps == 0) {
		if (ges_enable == 1) {
			obj->enable_gflag = 1;
		} else {
			obj->enable_gflag = 0;
		}
	}

	write_global_variable(obj->client);
	epl_sensor_update_mode(client);
	return count;
}

static ssize_t epl_sensor_store_ges_polling_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ges_mode = 0;
	struct epl_sensor_priv *obj = epl_sensor_obj;
	struct i2c_client *client = obj->client;

	LOG_FUN();
	sscanf(buf, "%d", &ges_mode);
	LOG_INFO("[%s]: ges_mode=%d \r\n", __func__, ges_mode);

	epl_sensor.ges.polling_mode = ges_mode;
	epl_sensor.ps.polling_mode = ges_mode;

	epl_sensor_update_mode(client);
	return count;
}

static ssize_t epl_sensor_store_ges_thd(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ges_l = 0, ges_h = 0;
	struct epl_sensor_priv *obj = epl_sensor_obj;
	struct i2c_client *client = obj->client;

	LOG_FUN();
	sscanf(buf, "%d, %d", &ges_l, &ges_h);
	epl_sensor.ges.low_threshold = ges_l;
	epl_sensor.ges.high_threshold = ges_h;
	LOG_INFO("[%s]: ges_thd=%d, %d \r\n", __func__, epl_sensor.ges.low_threshold, epl_sensor.ges.high_threshold);

	write_global_variable(obj->client);
	epl_sensor_update_mode(client);
	return count;
}

#endif

/*----------------------------------------------------------------------------*/
/*CTS --> S_IWUSR | S_IRUGO*/
static DEVICE_ATTR(elan_status, S_IROTH | S_IWOTH, epl_sensor_show_status, NULL);
static DEVICE_ATTR(elan_reg, S_IROTH | S_IWOTH, epl_sensor_show_reg, NULL);
static DEVICE_ATTR(mode, S_IROTH | S_IWOTH, NULL, epl_sensor_store_mode);
static DEVICE_ATTR(wait_time, S_IROTH | S_IWOTH, NULL, epl_sensor_store_wait_time);
static DEVICE_ATTR(set_threshold, S_IROTH | S_IWOTH, NULL, epl_sensor_store_threshold);
static DEVICE_ATTR(cal_raw, S_IROTH | S_IWOTH, epl_sensor_show_cal_raw, NULL);
static DEVICE_ATTR(als_enable, S_IROTH | S_IWOTH, NULL, epl_sensor_store_als_enable);
static DEVICE_ATTR(als_report_type, S_IROTH | S_IWOTH, NULL,  elp_sensor_store_als_report_type);
static DEVICE_ATTR(ps_enable, S_IROTH | S_IWOTH, NULL, epl_sensor_store_ps_enable);
static DEVICE_ATTR(ps_polling_mode, S_IROTH | S_IWOTH, epl_sensor_show_ps_polling, epl_sensor_store_ps_polling_mode);
static DEVICE_ATTR(als_polling_mode, S_IROTH | S_IWOTH, epl_sensor_show_als_polling, epl_sensor_store_als_polling_mode);
static DEVICE_ATTR(gain, S_IROTH | S_IWOTH, NULL, epl_sensor_store_gain);
static DEVICE_ATTR(ir_mode, S_IROTH | S_IWOTH, NULL, epl_sensor_store_ir_mode);
static DEVICE_ATTR(ir_drive, S_IROTH | S_IWOTH, NULL, epl_sensor_store_ir_drive);
static DEVICE_ATTR(ir_on, S_IROTH | S_IWOTH, NULL, elp_sensor_store_ir_contrl);
static DEVICE_ATTR(interrupt_type, S_IROTH | S_IWOTH, NULL, elp_sensor_store_interrupt_type);
static DEVICE_ATTR(integration, S_IROTH | S_IWOTH, NULL, elp_sensor_store_integration);
static DEVICE_ATTR(adc, S_IROTH | S_IWOTH, NULL, elp_sensor_store_adc);
static DEVICE_ATTR(cycle, S_IROTH | S_IWOTH, NULL, elp_sensor_store_cycle);
static DEVICE_ATTR(ps_w_calfile, S_IROTH | S_IWOTH, NULL, elp_sensor_store_ps_w_calfile);
static DEVICE_ATTR(i2c_w, S_IROTH | S_IWOTH, NULL, epl_sensor_store_reg_write);
static DEVICE_ATTR(unlock, S_IROTH | S_IWOTH, NULL, elp_sensor_store_unlock);
static DEVICE_ATTR(als_ch, S_IROTH | S_IWOTH, NULL, elp_sensor_store_als_ch_sel);
static DEVICE_ATTR(ps_cancel, S_IROTH | S_IWOTH, NULL, elp_sensor_store_ps_cancelation);
static DEVICE_ATTR(run_ps_cali, S_IROTH | S_IWOTH, epl_sensor_show_ps_run_cali, NULL);
static DEVICE_ATTR(pdata, S_IROTH | S_IWOTH, epl_sensor_show_pdata, NULL);
static DEVICE_ATTR(als_data, S_IROTH | S_IWOTH, epl_sensor_show_als_data, NULL);
#if ALS_DYN_INTT
static DEVICE_ATTR(als_dyn_c_gain, S_IROTH | S_IWOTH, NULL, epl_sensor_store_c_gain);
static DEVICE_ATTR(als_dyn_lsrc_type, S_IROTH | S_IWOTH, NULL, epl_sensor_store_lsrc_type);
static DEVICE_ATTR(als_dyn_lsrc_thd, S_IROTH | S_IWOTH, NULL, epl_sensor_store_lsrc_thd);
#endif
#if PS_DYN_K
static DEVICE_ATTR(dyn_offset, S_IROTH | S_IWOTH, NULL,	epl_sensor_store_dyn_offset);
static DEVICE_ATTR(dyn_thd_offset, S_IROTH | S_IWOTH, NULL,	epl_sensor_store_dyn_thd_offset);
static DEVICE_ATTR(dyn_change_max, S_IROTH | S_IWOTH, NULL,	epl_sensor_store_dyn_change_thd_max);
static DEVICE_ATTR(dyn_max_ir_data, S_IROTH | S_IWOTH, NULL, epl_sensor_store_dyn_max_ir_data);
#if PS_DYN_K_STR
static DEVICE_ATTR(dyn_enh_max_ch0, S_IROTH | S_IWOTH, NULL, epl_sensor_store_dyn_enhance_max_ch0);
static DEVICE_ATTR(dyn_enh_gain_intt, S_IROTH | S_IWOTH, NULL, epl_sensor_store_dyn_enhance_gain_intt);
#endif
#endif
#if HS_ENABLE
static DEVICE_ATTR(elan_renvo, S_IROTH | S_IWOTH, epl_sensor_show_renvo, NULL);
static DEVICE_ATTR(hs_enable, S_IROTH | S_IWOTH, NULL, epl_sensor_store_hs_enable);
static DEVICE_ATTR(hs_int_time, S_IROTH | S_IWOTH, NULL, epl_sensor_store_hs_int_time);
static DEVICE_ATTR(hs_raws, S_IROTH | S_IWOTH, epl_sensor_show_hs_raws, NULL);
#endif
#if PS_GES
static DEVICE_ATTR(ges_enable, S_IROTH | S_IWOTH, NULL, epl_sensor_store_ges_enable);
static DEVICE_ATTR(ges_polling_mode, S_IROTH | S_IWOTH, NULL, epl_sensor_store_ges_polling_mode);
static DEVICE_ATTR(ges_thd, S_IROTH | S_IWOTH, NULL, epl_sensor_store_ges_thd);
#endif
/*----------------------------------------------------------------------------*/
static struct attribute *epl_sensor_attr_list[] = {
	&dev_attr_elan_status.attr,
	&dev_attr_elan_reg.attr,
	&dev_attr_als_enable.attr,
	&dev_attr_ps_enable.attr,
	&dev_attr_cal_raw.attr,
	&dev_attr_set_threshold.attr,
	&dev_attr_wait_time.attr,
	&dev_attr_gain.attr,
	&dev_attr_mode.attr,
	&dev_attr_ir_mode.attr,
	&dev_attr_ir_drive.attr,
	&dev_attr_ir_on.attr,
	&dev_attr_interrupt_type.attr,
	&dev_attr_integration.attr,
	&dev_attr_adc.attr,
	&dev_attr_cycle.attr,
	&dev_attr_als_report_type.attr,
	&dev_attr_ps_polling_mode.attr,
	&dev_attr_als_polling_mode.attr,
	&dev_attr_ps_w_calfile.attr,
	&dev_attr_i2c_w.attr,
	&dev_attr_unlock.attr,
	&dev_attr_als_ch.attr,
	&dev_attr_ps_cancel.attr,
	&dev_attr_run_ps_cali.attr,
	&dev_attr_pdata.attr,
	&dev_attr_als_data.attr,
#if ALS_DYN_INTT
	&dev_attr_als_dyn_c_gain.attr,
	&dev_attr_als_dyn_lsrc_type.attr,
	&dev_attr_als_dyn_lsrc_thd.attr,
#endif
#if PS_DYN_K
	&dev_attr_dyn_offset.attr,
	&dev_attr_dyn_thd_offset.attr,
	&dev_attr_dyn_change_max.attr,
	&dev_attr_dyn_max_ir_data.attr,
#if PS_DYN_K_STR
	&dev_attr_dyn_enh_max_ch0.attr,
	&dev_attr_dyn_enh_gain_intt.attr,
#endif
#endif
#if HS_ENABLE
	&dev_attr_elan_renvo.attr,
	&dev_attr_hs_enable.attr,
	&dev_attr_hs_int_time.attr,
	&dev_attr_hs_raws.attr,
#endif
#if PS_GES
	&dev_attr_ges_enable.attr,
	&dev_attr_ges_polling_mode.attr,
	&dev_attr_ges_thd.attr,
#endif
	NULL,
};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct attribute_group epl_sensor_attr_group = {
	.attrs = epl_sensor_attr_list,
};
/*----------------------------------------------------------------------------*/
#if !(SPREAD || MARVELL)/*SPREAD MARVELL start.....*/
/*----------------------------------------------------------------------------*/
static int epl_sensor_als_open(struct inode *inode, struct file *file)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	if (epld->als_opened) {
		return -EBUSY;
	}
	epld->als_opened = 1;

	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_als_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int buf[1];
	if (epld->read_flag == 1) {
		buf[0] = epl_sensor.als.data.channels[1];
		if (copy_to_user(buffer, &buf , sizeof(buf)))
			return 0;
		epld->read_flag = 0;
		return 12;
	} else {
		return 0;
	}
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int epl_sensor_als_release(struct inode *inode, struct file *file)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	epld->als_opened = 0;

	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static long epl_sensor_als_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int flag;
	unsigned long buf[1];
	struct epl_sensor_priv *epld = epl_sensor_obj;
	bool enable_als = epld->enable_lflag == 1 && epld->als_suspend == 0;
	void __user *argp = (void __user *)arg;

	LOG_INFO("als io ctrl cmd %d\n", _IOC_NR(cmd));

	switch (cmd) {
	case ELAN_EPL8800_IOCTL_GET_LFLAG:
			LOG_INFO("elan ambient-light IOCTL Sensor get lflag \n");
		flag = epld->enable_lflag;
		if (copy_to_user(argp, &flag, sizeof(flag)))
			return -EFAULT;
		LOG_INFO("elan ambient-light Sensor get lflag %d\n", flag);
		break;

	case ELAN_EPL8800_IOCTL_ENABLE_LFLAG:
#if LEADCORE
	case LIGHT_SET_ENALBE:
#endif
		LOG_INFO("elan ambient-light IOCTL Sensor set lflag \n");
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;

		if (epld->enable_lflag != flag) {
#if ALS_DYN_INTT
			if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
				dynamic_intt_idx = dynamic_intt_init_idx;
				epl_sensor.als.integration_time = als_dynamic_intt_intt[dynamic_intt_idx];
				epl_sensor.als.gain = als_dynamic_intt_gain[dynamic_intt_idx];
				dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
				dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];
			}
#endif
				epld->enable_lflag = flag;

				epl_sensor_update_mode(epld->client);
			}

		LOG_INFO("elan ambient-light Sensor set lflag %d\n", flag);
		break;

	case ELAN_EPL8800_IOCTL_GETDATA:
		if (enable_als == 0) {
			epld->enable_lflag = 1;
			epl_sensor_update_mode(epld->client);
			msleep(30);
		}

		if (enable_als == true && polling_flag == true
				&& eint_flag == true && epl_sensor.als.polling_mode == 0) {
			mutex_lock(&sensor_mutex);
			epl_sensor_read_als(epld->client);
			mutex_unlock(&sensor_mutex);
		}
#if ALS_DYN_INTT
		if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
			buf[0] = epl_sensor.als.dyn_intt_raw;
			LOG_INFO("[%s]: als epl_sensor.als.dyn_intt_raw = %d \r\n", __func__, epl_sensor.als.dyn_intt_raw);
		} else {
			buf[0] = epl_sensor.als.data.channels[1];
			LOG_INFO("[%s]: epl_sensor.als.data.channels[1] = %d \r\n", __func__, epl_sensor.als.data.channels[1]);
		}
#endif

		if (copy_to_user(argp, &buf , sizeof(buf)))
			return -EFAULT;

		break;
#if LEADCORE
	case LIGHT_SET_DELAY:
		if (arg > LIGHT_MAX_DELAY)
			arg = LIGHT_MAX_DELAY;
		else if (arg < LIGHT_MIN_DELAY)
			arg = LIGHT_MIN_DELAY;
		LOG_INFO("LIGHT_SET_DELAY--%d\r\n", (int)arg);
		polling_time = arg;
		break;
#endif
	default:
		LOG_ERR("invalid cmd %d\n", _IOC_NR(cmd));
		return -EINVAL;
	}

	return 0;

}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct file_operations epl_sensor_als_fops = {
	.owner = THIS_MODULE,
	.open = epl_sensor_als_open,
	.read = epl_sensor_als_read,
	.release = epl_sensor_als_release,
	.unlocked_ioctl = epl_sensor_als_ioctl
};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct miscdevice epl_sensor_als_device = {
	.minor = MISC_DYNAMIC_MINOR,
#if LEADCORE
	.name = "light",
#else
	.name = "elan_als",
#endif
	.fops = &epl_sensor_als_fops
};
/*----------------------------------------------------------------------------*/
#endif /*SPREAD MARVELL end.........*/

/*----------------------------------------------------------------------------*/
static int epl_sensor_ps_open(struct inode *inode, struct file *file)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();
	if (epld->ps_opened)
		return -EBUSY;

	epld->ps_opened = 1;
	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int epl_sensor_ps_release(struct inode *inode, struct file *file)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	LOG_FUN();
	epld->ps_opened = 0;

	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static long epl_sensor_ps_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int value;
	int flag;
	u16 buf[2];
	struct epl_sensor_priv *epld = epl_sensor_obj;
	bool enable_ps = epld->enable_pflag == 1 && epld->ps_suspend == 0;
#if HS_ENABLE
	bool enable_hs = epld->enable_hflag == 1 && epld->hs_suspend == 0;
#endif
#if PS_GES
	bool enable_ges = epld->enable_gflag == 1 && epld->ges_suspend == 0;
#endif
	void __user *argp = (void __user *)arg;

	LOG_INFO("ps io ctrl cmd %d\n", _IOC_NR(cmd));


	switch (cmd) {

	case ELAN_EPL8800_IOCTL_GET_PFLAG:
#if MARVELL
	case LTR_IOCTL_GET_PFLAG:
#endif
		LOG_INFO("elan Proximity Sensor IOCTL get pflag \n");
		flag = epld->enable_pflag;
		if (copy_to_user(argp, &flag, sizeof(flag)))
			return -EFAULT;

		LOG_INFO("elan Proximity Sensor get pflag %d\n", flag);
		break;

	case ELAN_EPL8800_IOCTL_ENABLE_PFLAG:
#if LEADCORE
	case PROXIMITY_SET_ENALBE:
#elif MARVELL
	case LTR_IOCTL_SET_PFLAG:
#endif
		LOG_INFO("elan Proximity IOCTL Sensor set pflag \n");
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;
#if HS_ENABLE
		if (enable_hs == 1 && flag == 1) {
			epld->enable_hflag = 0;
			if (hs_enable_flag == true) {
				epld->enable_lflag = 1;
				hs_enable_flag = false;
			}
			write_global_variable(epld->client);
			LOG_INFO("[%s] Disable HS and recover ps setting \r\n", __func__);
		}
#endif
#if PS_GES
		if (enable_ges == 1 && flag == 1) {
			epld->enable_gflag = 0;
			write_global_variable(epld->client);
			ps_ges_enable_flag = true;
			LOG_INFO("[%s] Disable GES and recover ps setting \r\n", __func__);
		} else if (ps_ges_enable_flag == true && flag == 0) {
			epld->enable_gflag = 1;
			write_global_variable(epld->client);
			ps_ges_enable_flag = false;
			LOG_INFO("[%s] enable GES and recover ges setting \r\n", __func__);
		}
#endif
		if (epld->enable_pflag != flag) {
			epld->enable_pflag = flag;
			if (flag) {
				wake_lock(&ps_lock);
#if PS_DYN_K
				dynk_min_ps_raw_data = 0xffff;
				dynk_change_flag = false;
#if PS_DYN_K_STR
				dynk_enhance_flag = false;
#endif

#endif
			} else {

#if PS_DYN_K
				cancel_delayed_work(&epld->dynk_thd_polling_work);
#endif
				wake_unlock(&ps_lock);
			}

			epl_sensor_update_mode(epld->client);
		}

		LOG_INFO("elan Proximity Sensor set pflag %d\n", flag);
		break;

	case ELAN_EPL8800_IOCTL_GETDATA:
		if (enable_ps == 0) {
			epld->enable_pflag = 1;
			epl_sensor_update_mode(epld->client);
			msleep(30);
		}

		if (enable_ps == true && polling_flag == true
					&& eint_flag == true && epl_sensor.ps.polling_mode == 0) {
			mutex_lock(&sensor_mutex);
			epl_sensor_read_ps(epld->client);
			mutex_unlock(&sensor_mutex);
		}

		LOG_INFO("[%s]: epl_sensor.ps.data.data = %d \r\n", __func__, epl_sensor.ps.data.data);

		value = epl_sensor.ps.data.data;
		if (copy_to_user(argp, &value , sizeof(value)))
			return -EFAULT;

		LOG_INFO("elan proximity Sensor get data (%d) \n", value);
		break;
#if LEADCORE
	case PROXIMITY_SET_DELAY:
		if (arg > PROXIMITY_MAX_DELAY)
			arg = PROXIMITY_MAX_DELAY;
		else if (arg < PROXIMITY_MIN_DELAY)
			arg = PROXIMITY_MIN_DELAY;
		LOG_INFO("PROXIMITY_SET_DELAY--%d\r\n", (int)arg);
		polling_time = arg;
		break;
#endif

#if SPREAD || MARVELL  /*SPREAD MARVELL start.....*/
	case ELAN_EPL8800_IOCTL_GET_LFLAG:
#if MARVELL
	case LTR_IOCTL_GET_LFLAG:
#endif
		LOG_INFO("elan ambient-light IOCTL Sensor get lflag \n");
		flag = epld->enable_lflag;
		if (copy_to_user(argp, &flag, sizeof(flag)))
			return -EFAULT;

		LOG_INFO("elan ambient-light Sensor get lflag %d\n", flag);
		break;

	case ELAN_EPL8800_IOCTL_ENABLE_LFLAG:
#if MARVELL
	case LTR_IOCTL_SET_LFLAG:
#endif
		LOG_INFO("elan ambient-light IOCTL Sensor set lflag \n");
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;

		if (epld->enable_lflag != flag) {
#if ALS_DYN_INTT
			if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
				dynamic_intt_idx = dynamic_intt_init_idx;
				epl_sensor.als.integration_time = als_dynamic_intt_intt[dynamic_intt_idx];
				epl_sensor.als.gain = als_dynamic_intt_gain[dynamic_intt_idx];
				dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
				dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];
			}
#endif
			epld->enable_lflag = flag;

			epl_sensor_update_mode(epld->client);
		}

		LOG_INFO("elan ambient-light Sensor set lflag %d\n", flag);
		break;
#endif  /*SPREAD MARVELL end......*/

	case ALSPS_REC_PS_DATA_FOR_CALI:
#if PS_DYN_K
	break;
#else
		if (copy_from_user(&value, argp, sizeof(value)))
			return -EFAULT;

		set_psensor_intr_threshold(value+dynk_low_offset, value+dynk_high_offset);
		LOG_INFO("%s: ---th_l %d, th_h %d\n", __func__, value+LOW_OFFSET, value+HIGH_OFFSET);
		break;
#endif
	case ALSPS_GET_PS_RAW_DATA_FOR_CALI:
		msleep(110);
		mutex_lock(&sensor_mutex);
		epl_sensor_read_ps(epld->client);
		mutex_unlock(&sensor_mutex);
		buf[0] = epl_sensor.ps.data.data;
		buf[1] = epl_sensor.ps.compare_low >> 3;
		if (copy_to_user(argp, buf, sizeof(buf)))
			return -EFAULT;
		LOG_INFO("%s: ---ps.data %d\n", __func__, epl_sensor.ps.data.data);
		break;
		default:
			LOG_ERR("invalid cmd %d\n", _IOC_NR(cmd));
			return -EINVAL;
	}

	return 0;

}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct file_operations epl_sensor_ps_fops = {
	.owner = THIS_MODULE,
	.open = epl_sensor_ps_open,
	.release = epl_sensor_ps_release,
	.unlocked_ioctl = epl_sensor_ps_ioctl
};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct miscdevice epl_sensor_ps_device = {
	.minor = MISC_DYNAMIC_MINOR,
#if LEADCORE
	.name = "proximity",
#elif MARVELL
	.name = "alps_pxy",
#else
	.name = "psensor",
#endif
	.fops = &epl_sensor_ps_fops
};

#if !(SPREAD || MARVELL) /*SPREAD MARVELL start.....*/
/*----------------------------------------------------------------------------*/
static ssize_t light_enable_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct epl_sensor_priv *epld  = epl_sensor_obj;
	LOG_INFO("%s: ALS_status=%d\n", __func__, epld->enable_lflag);
	return sprintf(buf, "%d\n", epld->enable_lflag);
}

static ssize_t light_enable_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	uint16_t als_enable = 0;
	LOG_INFO("light_enable_store: enable=%s \n", buf);

	sscanf(buf, "%hu", &als_enable);

	if (epld->enable_lflag != als_enable) {
		epld->enable_lflag = als_enable;
		epl_sensor_update_mode(epld->client);
	}

	return size;
}
/*----------------------------------------------------------------------------*/
#if MARVELL
static struct device_attribute dev_attr_light_enable =
__ATTR(active, S_IRWXUGO,
	   light_enable_show, light_enable_store);
#else
static struct device_attribute dev_attr_light_enable =
__ATTR(enable, S_IRWXUGO,
	   light_enable_show, light_enable_store);
#endif
static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};
/*----------------------------------------------------------------------------*/
static int epl_sensor_setup_lsensor(struct epl_sensor_priv *epld)
{
	int err = 0;
	LOG_INFO("epl_sensor_setup_lsensor enter.\n");

	epld->als_input_dev = input_allocate_device();
	if (!epld->als_input_dev) {
		LOG_ERR("could not allocate ls input device\n");
		return -ENOMEM;
	}
	epld->als_input_dev->name = ElanALsensorName;
	set_bit(EV_ABS, epld->als_input_dev->evbit);
	input_set_abs_params(epld->als_input_dev, ABS_MISC, 0, 9, 0, 0);

	err = input_register_device(epld->als_input_dev);
	if (err < 0) {
		LOG_ERR("can not register ls input device\n");
		goto err_free_ls_input_device;
	}

	err = misc_register(&epl_sensor_als_device);
	if (err < 0) {
		LOG_ERR("can not register ls misc device\n");
		goto err_unregister_ls_input_device;
	}

	err = sysfs_create_group(&epld->als_input_dev->dev.kobj, &light_attribute_group);

	if (err) {
		pr_err("%s: could not create sysfs group\n", __func__);
		goto err_free_ls_input_device;
	}
	return err;


err_unregister_ls_input_device:
	input_unregister_device(epld->als_input_dev);
err_free_ls_input_device:
	input_free_device(epld->als_input_dev);
	return err;
}
#endif /*SPREAD MARVELL end.....*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static ssize_t proximity_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct epl_sensor_priv *epld  = epl_sensor_obj;
	LOG_INFO("%s: PS status=%d\n", __func__, epld->enable_pflag);
	return sprintf(buf, "%d\n", epld->enable_pflag);
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static ssize_t proximity_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	uint16_t ps_enable = 0;
#if HS_ENABLE
	bool enable_hs = epld->enable_hflag == 1 && epld->hs_suspend == 0;
#endif
#if PS_GES
	bool enable_ges = epld->enable_gflag == 1 && epld->ges_suspend == 0;
#endif
	LOG_INFO("proximity_enable_store: enable=%s \n", buf);

	sscanf(buf, "%hu", &ps_enable);

#if HS_ENABLE
	if (enable_hs == 1 && ps_enable == 1) {
		epld->enable_hflag = 0;
		if (hs_enable_flag == true) {
			epld->enable_lflag = 1;
			hs_enable_flag = false;
		}
		write_global_variable(epld->client);
		LOG_INFO("[%s] Disable HS and recover ps setting \r\n", __func__);
	}
#endif
#if PS_GES
	if (enable_ges == 1 && ps_enable == 1) {
		epld->enable_gflag = 0;
		write_global_variable(epld->client);
		ps_ges_enable_flag = true;
		LOG_INFO("[%s] Disable GES and recover ps setting \r\n", __func__);
	} else if (ps_ges_enable_flag == true && ps_enable == 0) {
		epld->enable_gflag = 1;
		write_global_variable(epld->client);
		ps_ges_enable_flag = false;
		LOG_INFO("[%s] enable GES and recover ges setting \r\n", __func__);
	}
#endif
	if (epld->enable_pflag != ps_enable) {
		epld->enable_pflag = ps_enable;
		if (ps_enable) {
			wake_lock(&ps_lock);
#if PS_DYN_K
			dynk_min_ps_raw_data = 0xffff;
			dynk_change_flag = false;
#if PS_DYN_K_STR
			dynk_enhance_flag = false;
#endif
#endif
		} else {
#if PS_DYN_K
			cancel_delayed_work(&epld->dynk_thd_polling_work);
#endif
			wake_unlock(&ps_lock);
		}

		epl_sensor_update_mode(epld->client);
	}

	return size;
}
/*----------------------------------------------------------------------------*/
#if MARVELL
static struct device_attribute dev_attr_psensor_enable =
__ATTR(active, S_IRWXUGO,
	   proximity_enable_show, proximity_enable_store);
#else
static struct device_attribute dev_attr_psensor_enable =
__ATTR(enable, S_IRWXUGO,
	   proximity_enable_show, proximity_enable_store);
#endif
static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_psensor_enable.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int epl_sensor_setup_psensor(struct epl_sensor_priv *epld)
{
	int err = 0;
	LOG_INFO("epl_sensor_setup_psensor enter.\n");


	epld->ps_input_dev = input_allocate_device();
	if (!epld->ps_input_dev) {
		LOG_ERR("could not allocate ps input device\n");
		return -ENOMEM;
	}
	epld->ps_input_dev->name = ElanPsensorName;

	set_bit(EV_ABS, epld->ps_input_dev->evbit);
	input_set_abs_params(epld->ps_input_dev, ABS_DISTANCE, 0, 1, 0, 0);
#if SPREAD || MARVELL
	set_bit(EV_ABS, epld->ps_input_dev->evbit);
	input_set_abs_params(epld->ps_input_dev, ABS_MISC, 0, 9, 0, 0);
#endif
	err = input_register_device(epld->ps_input_dev);
	if (err < 0) {
		LOG_ERR("could not register ps input device\n");
		goto err_free_ps_input_device;
	}

	err = misc_register(&epl_sensor_ps_device);
	if (err < 0) {
		LOG_ERR("could not register ps misc device\n");
		goto err_unregister_ps_input_device;
	}

	err = sysfs_create_group(&epld->ps_input_dev->dev.kobj, &proximity_attribute_group);

	if (err) {
		pr_err("%s: PS could not create sysfs group\n", __func__);
		goto err_free_ps_input_device;
	}

	return err;


err_unregister_ps_input_device:
	input_unregister_device(epld->ps_input_dev);
err_free_ps_input_device:
	input_free_device(epld->ps_input_dev);
	return err;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#ifdef CONFIG_SUSPEND
static int epl_sensor_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	LOG_FUN();

	epld->als_suspend = 1;
#if HS_ENABLE
	epld->hs_suspend = 1;
#endif
#if PS_GES
	epld->ges_suspend = 1;
	ps_ges_suspend_flag = true;
#endif
	if (epld->enable_pflag == 1) {

		LOG_INFO("[%s]: ps enable \r\n", __func__);
	} else {

		LOG_INFO("[%s]: ps disable \r\n", __func__);
		epl_sensor_update_mode(epld->client);
	}

	return 0;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void epl_sensor_early_suspend(struct early_suspend *h)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	LOG_FUN();

	epld->als_suspend = 1;
#if HS_ENABLE
	epld->hs_suspend = 1;
#endif
#if PS_GES
	epld->ges_suspend = 1;
	ps_ges_suspend_flag = true;
#endif
	if (epld->enable_pflag == 1) {

		LOG_INFO("[%s]: ps enable \r\n", __func__);
	} else {

		LOG_INFO("[%s]: ps disable \r\n", __func__);
		epl_sensor_update_mode(epld->client);
	}

}
#endif

static int epl_sensor_resume(struct i2c_client *client)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	LOG_FUN();

	epld->als_suspend = 0;
	epld->ps_suspend = 0;
#if HS_ENABLE
	epld->hs_suspend = 0;
#endif
#if PS_GES
	epld->ges_suspend = 0;
#endif
	if (epld->enable_pflag == 1) {

		LOG_INFO("[%s]: ps enable \r\n", __func__);
		epl_sensor_restart_polling();
	} else {

		LOG_INFO("[%s]: ps disable \r\n", __func__);
		epl_sensor_update_mode(epld->client);
	}

#if !defined(CONFIG_HAS_EARLYSUSPEND) && PS_GES
	ps_ges_suspend_flag = false;
#endif
	return 0;
}
/*----------------------------------------------------------------------------*/

#if defined(CONFIG_HAS_EARLYSUSPEND)
/*----------------------------------------------------------------------------*/
static void epl_sensor_late_resume(struct early_suspend *h)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	LOG_FUN();

	epld->als_suspend = 0;
	epld->ps_suspend = 0;
#if HS_ENABLE
	epld->hs_suspend = 0;
#endif
#if PS_GES
	epld->ges_suspend = 0;
#endif
	if (epld->enable_pflag == 1) {

		LOG_INFO("[%s]: ps enable \r\n", __func__);
		epl_sensor_restart_polling();
	} else {

		LOG_INFO("[%s]: ps disable \r\n", __func__);
		epl_sensor_update_mode(epld->client);
	}
#if PS_GES
	ps_ges_suspend_flag = false;
#endif
}
#endif

#endif
/*----------------------------------------------------------------------------*/
#if SENSOR_CLASS
static int epld_sensor_cdev_enable_als(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	LOG_INFO("[%s]: enable=%d \r\n", __func__, enable);

	if (epld->enable_lflag != enable) {
#if ALS_DYN_INTT
		if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
			dynamic_intt_idx = dynamic_intt_init_idx;
			epl_sensor.als.integration_time = als_dynamic_intt_intt[dynamic_intt_idx];
			epl_sensor.als.gain = als_dynamic_intt_gain[dynamic_intt_idx];
			dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
			dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];
		}
#endif
		epld->enable_lflag = enable;

		epl_sensor_update_mode(epld->client);
	}

	return 0;
}

static int epld_sensor_cdev_enable_ps(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;


#if HS_ENABLE
	bool enable_hs = epld->enable_hflag == 1 && epld->hs_suspend == 0;
#endif
#if PS_GES
	bool enable_ges = epld->enable_gflag == 1 && epld->ges_suspend == 0;
#endif
	LOG_INFO("[%s]: enable=%d \r\n", __func__, enable);
#if HS_ENABLE
	if (enable_hs == 1 && enable == 1) {
		epld->enable_hflag = 0;
		if (hs_enable_flag == true) {
			epld->enable_lflag = 1;
			hs_enable_flag = false;
		}
		write_global_variable(epld->client);
		LOG_INFO("[%s] Disable HS and recover ps setting \r\n", __func__);
	}
#endif
#if PS_GES
	if (enable_ges == 1 && enable == 1) {
		epld->enable_gflag = 0;
		write_global_variable(epld->client);
		ps_ges_enable_flag = true;
		LOG_INFO("[%s] Disable GES and recover ps setting \r\n", __func__);
	} else if (ps_ges_enable_flag == true && enable == 0) {
		epld->enable_gflag = 1;
		write_global_variable(epld->client);
		ps_ges_enable_flag = false;
		LOG_INFO("[%s] enable GES and recover ges setting \r\n", __func__);
	}
#endif
	if (epld->enable_pflag != enable) {
		epld->enable_pflag = enable;
		if (enable) {
			wake_lock(&ps_lock);
#if PS_DYN_K
			dynk_min_ps_raw_data = 0xffff;
			dynk_change_flag = false;
#if PS_DYN_K_STR
			dynk_enhance_flag = false;
#endif
#endif
		} else {
#if PS_DYN_K
			cancel_delayed_work(&epld->dynk_thd_polling_work);
#endif
			wake_unlock(&ps_lock);
		}

		epl_sensor_update_mode(epld->client);
	}


	return 0;
}
static int epl_snesor_cdev_set_ps_delay(struct sensors_classdev *sensors_cdev, unsigned int delay_msec)
{


	if (delay_msec < PS_MIN_POLLING_RATE)	/*at least 200 ms */
		delay_msec = PS_MIN_POLLING_RATE;

	polling_time = delay_msec;	/* convert us => ms */

	return 0;
}

static int epl_sensor_cdev_set_als_delay(struct sensors_classdev *sensors_cdev, unsigned int delay_msec)
{


	if (delay_msec < ALS_MIN_POLLING_RATE)	/*at least 200 ms */
		delay_msec = ALS_MIN_POLLING_RATE;

	polling_time = delay_msec;	/* convert us => ms */

	return 0;
}

#endif


/*----------------------------------------------------------------------------*/
static int epl_sensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;
	struct epl_sensor_priv *epld ;

	LOG_INFO("elan sensor probe enter.\n");

	epld = kzalloc(sizeof(struct epl_sensor_priv), GFP_KERNEL);
	if (!epld)
		return -ENOMEM;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "No supported i2c func what we need?!!\n");
		err = -ENOTSUPP;
		goto i2c_fail;
	}
	LOG_INFO("chip id REG 0x00 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x00));
	LOG_INFO("chip id REG 0x01 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x01));
	LOG_INFO("chip id REG 0x02 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x02));
	LOG_INFO("chip id REG 0x03 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x03));
	LOG_INFO("chip id REG 0x04 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x04));
	LOG_INFO("chip id REG 0x05 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x05));
	LOG_INFO("chip id REG 0x06 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x06));
	LOG_INFO("chip id REG 0x07 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x07));
	LOG_INFO("chip id REG 0x11 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x11));
	LOG_INFO("chip id REG 0x12 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x12));
	LOG_INFO("chip id REG 0x1B value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x1B));
	LOG_INFO("chip id REG 0x20 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x20));
	LOG_INFO("chip id REG 0x21 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x21));
	LOG_INFO("chip id REG 0x24 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x24));
	LOG_INFO("chip id REG 0x25 value = 0x%x\n", i2c_smbus_read_byte_data(client, 0x25));

	if ((i2c_smbus_read_byte_data(client, 0x21)) != 0x81) {
		LOG_INFO("elan ALS/PS sensor is failed. \n");
		err = -ENOTSUPP;
		goto i2c_fail;
	}

	epld->als_level_num = sizeof(epld->als_level)/sizeof(epld->als_level[0]);
	epld->als_value_num = sizeof(epld->als_value)/sizeof(epld->als_value[0]);
	BUG_ON(sizeof(epld->als_level) != sizeof(als_level));
	memcpy(epld->als_level, als_level, sizeof(epld->als_level));
	BUG_ON(sizeof(epld->als_value) != sizeof(als_value));
	memcpy(epld->als_value, als_value, sizeof(epld->als_value));


	epld->client = client;
	epld->irq = client->irq;

#if HS_ENABLE
	epld->hs_suspend = 0;
	mutex_init(&hs_sensor_mutex);
#endif
#if PS_GES
	epld->ges_suspend = 0;
	epld->gs_input_dev = input_allocate_device();
	set_bit(EV_KEY, epld->gs_input_dev->evbit);
	set_bit(EV_REL, epld->gs_input_dev->evbit);
	set_bit(EV_ABS, epld->gs_input_dev->evbit);
	epld->gs_input_dev->evbit[0] |= BIT_MASK(EV_REP);
	epld->gs_input_dev->keycodemax = 500;
	epld->gs_input_dev->name = "elan_gesture";
	epld->gs_input_dev->keybit[BIT_WORD(KEYCODE_LEFT)] |= BIT_MASK(KEYCODE_LEFT);
	if (input_register_device(epld->gs_input_dev))
		LOG_ERR("register input error\n");
#endif
	i2c_set_clientdata(client, epld);

	epl_sensor_obj = epld;

	INIT_DELAYED_WORK(&epld->eint_work, epl_sensor_eint_work);
	INIT_DELAYED_WORK(&epld->polling_work, epl_sensor_polling_work);
#if PS_DYN_K
	INIT_DELAYED_WORK(&epld->dynk_thd_polling_work, epl_sensor_dynk_thd_polling_work);
#endif
	mutex_init(&sensor_mutex);

	initial_global_variable(client, epld);
#if !(SPREAD || MARVELL)
	err = epl_sensor_setup_lsensor(epld);
	if (err < 0) {
		LOG_ERR("epl_sensor_setup_lsensor error!!\n");
		goto err_lightsensor_setup;
	}
#endif

	err = epl_sensor_setup_psensor(epld);
	if (err < 0) {
		LOG_ERR("epl_sensor_setup_psensor error!!\n");
		goto err_psensor_setup;
	}


	if (epl_sensor.als.polling_mode == 0 || epl_sensor.ps.polling_mode == 0) {
		err = epl_sensor_setup_interrupt(epld);
		if (err < 0) {
			LOG_ERR("setup error!\n");
			goto err_sensor_setup;
		}
	}

#if SENSOR_CLASS
	epld->als_cdev = als_cdev;
	epld->als_cdev.sensors_enable = epld_sensor_cdev_enable_als;
	epld->als_cdev.sensors_poll_delay = epl_sensor_cdev_set_als_delay;

	err = sensors_classdev_register(&client->dev, &epld->als_cdev);
	if (err) {
		LOG_ERR("sensors class register failed.\n");
		goto err_register_als_cdev;
	}

	epld->ps_cdev = ps_cdev;
	epld->ps_cdev.sensors_enable = epld_sensor_cdev_enable_ps;
	epld->ps_cdev.sensors_poll_delay = epl_snesor_cdev_set_ps_delay;




	err = sensors_classdev_register(&client->dev, &epld->ps_cdev);
	if (err) {
		LOG_ERR("sensors class register failed.\n");
		goto err_register_ps_cdev;
	}
#endif

#ifdef CONFIG_SUSPEND

#if defined(CONFIG_HAS_EARLYSUSPEND)
	epld->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	epld->early_suspend.suspend = epl_sensor_early_suspend;
	epld->early_suspend.resume = epl_sensor_late_resume;
	register_early_suspend(&epld->early_suspend);
#endif

#endif
	wake_lock_init(&ps_lock, WAKE_LOCK_SUSPEND, "ps wakelock");

	sensor_dev = platform_device_register_simple("elan_alsps", -1, NULL, 0);
	if (IS_ERR(sensor_dev)) {
		printk ("sensor_dev_init: error\n");
		goto err_fail;
	}


	err = sysfs_create_group(&sensor_dev->dev.kobj, &epl_sensor_attr_group);
	if (err != 0) {
		dev_err(&client->dev, "%s:create sysfs group error", __func__);
		goto err_fail;
	}

	LOG_INFO("sensor probe success.\n");

	return err;

#if SENSOR_CLASS
err_register_ps_cdev:
	sensors_classdev_unregister(&epld->ps_cdev);
err_register_als_cdev:
	sensors_classdev_unregister(&epld->als_cdev);
#endif
err_fail:
	input_unregister_device(epld->als_input_dev);
	input_unregister_device(epld->ps_input_dev);
	input_free_device(epld->als_input_dev);
	input_free_device(epld->ps_input_dev);
#if !(SPREAD || MARVELL)
err_lightsensor_setup:
#endif
err_psensor_setup:
err_sensor_setup:
	misc_deregister(&epl_sensor_ps_device);
#if !(SPREAD || MARVELL)
	misc_deregister(&epl_sensor_als_device);
#endif
i2c_fail:
	kfree(epld);
	return err;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int epl_sensor_remove(struct i2c_client *client)
{
	struct epl_sensor_priv *epld = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "%s: enter.\n", __func__);
#if defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&epld->early_suspend);
#endif
	sysfs_remove_group(&sensor_dev->dev.kobj, &epl_sensor_attr_group);
	platform_device_unregister(sensor_dev);
	input_unregister_device(epld->als_input_dev);
	input_unregister_device(epld->ps_input_dev);
#if !(SPREAD || MARVELL)
	input_free_device(epld->als_input_dev);
#endif
	input_free_device(epld->ps_input_dev);
	misc_deregister(&epl_sensor_ps_device);
#if !(SPREAD || MARVELL)
	misc_deregister(&epl_sensor_als_device);
#endif
	free_irq(epld->irq, epld);
	kfree(epld);
	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id epl_sensor_id[] = {
	{EPL_DEV_NAME, 0},
	{}
};

#if QCOM
static struct of_device_id epl_match_table[] = {
				{.compatible = "epl, epl259x",},
				{},
		};
#endif

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct i2c_driver epl_sensor_driver = {
	.probe	= epl_sensor_probe,
	.remove	= epl_sensor_remove,
	.id_table	= epl_sensor_id,
	.driver	= {
		.name = EPL_DEV_NAME,
		.owner = THIS_MODULE,
#if QCOM
		.of_match_table = epl_match_table,
#endif
	},
#ifdef CONFIG_SUSPEND
	.suspend = epl_sensor_suspend,
	.resume = epl_sensor_resume,
#endif
};

static int __init epl_sensor_init(void)
{
	return i2c_add_driver(&epl_sensor_driver);
}

static void __exit epl_sensor_exit(void)
{
	i2c_del_driver(&epl_sensor_driver);
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
module_init(epl_sensor_init);
module_exit(epl_sensor_exit);
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Renato Pan <renato.pan@eminent-tek.com>");
MODULE_DESCRIPTION("ELAN epl259x driver");
MODULE_LICENSE("GPL");






