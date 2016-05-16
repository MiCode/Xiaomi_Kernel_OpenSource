/* drivers/input/misc/elan_epl2182.c - light and proxmity sensors driver
 * Copyright (C) 2011-2014 ELAN Corporation.
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
#include <linux/debugfs.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/of_gpio.h>
#include <linux/sensors.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <linux/elan_interface.h>
#include <linux/hardware_info.h>
/*********************************************************
 * configuration
*********************************************************/
/* 0 is polling mode, 1 is interrupt mode*/
#define PS_INTERRUPT_MODE		1

#define PS_POLLING_RATE			500
#define ALS_POLLING_RATE		1000	/* msec */

#define ALSPS_GET_PS_RAW_DATA_FOR_CALI	_IOW('c', 9, int *)
#define ALSPS_REC_PS_DATA_FOR_CALI	_IOW('c', 10, int *)



#define LUX_PER_COUNT			1100
#define MAX_LUX					8700

#define PS_AUTO_ENABLE	1
#define DYNAMIC_INTT 1

#if PS_AUTO_ENABLE
static int cali_flag;
#else
static int cali_flag = 1;
#endif

#if PS_AUTO_ENABLE
static int  DYN_L_OFFSET;
static int  DYN_H_OFFSET;
#define DYN_CONDITION	7500
#endif
#define  DEBUG
#ifdef DEBUG
#define epl_info(fmt, ...) \
	printk(pr_fmt(fmt), ##__VA_ARGS__)
#else
#define epl_info(fmt, ...) \
	no_printk(KERN_INFO pr_fmt(fmt), ##__VA_ARGS__)
#endif

#define LOG_FUN(f)		epl_info("%s  %d\n", __func__, __LINE__)
/**********************************************************
 * configuration
**********************************************************/
enum CMC_MODE {
	CMC_MODE_ALS = 0x00,
	CMC_MODE_PS = 0x10,
};
#define TXBYTES				2
#define RXBYTES				2

#if DYNAMIC_INTT

#define PS_DELAY 			55
#define ALS_DELAY 			100

int als_report_count = 0;

int dynamic_intt_idx;
int dynamic_intt_init_idx = 2;


int c_gain = 72000;

uint8_t dynamic_intt_intt;
uint8_t dynamic_intt_gain;
uint16_t dynamic_intt_high_thr;
uint16_t dynamic_intt_low_thr;
uint32_t dynamic_intt_max_lux = 10000;
uint32_t dynamic_intt_min_lux = 0;
uint32_t dynamic_intt_min_unit = 1000;

static int als_dynamic_intt_intt[] = {EPL_ALS_INTT_4096, EPL_ALS_INTT_256, EPL_ALS_INTT_16, EPL_ALS_INTT_16};
static int als_dynamic_intt_intt_value[] = {4096, 256, 16, 16};
static int als_dynamic_intt_gain[] = {EPL_M_GAIN, EPL_M_GAIN, EPL_M_GAIN, EPL_L_GAIN};

static int als_dynamic_intt_high_thr[] = {7200, 6400, 3200, 45000};
static int als_dynamic_intt_low_thr[] = {64, 256, 256, 256};

static int als_dynamic_intt_intt_num =  sizeof(als_dynamic_intt_intt_value)/sizeof(int);

#else

#define PS_DELAY			55
#define ALS_DELAY			60
#endif

#define PACKAGE_SIZE			2
#define I2C_RETRY_COUNT			10
#define P_INTT				1
#define PS_INTT				4
#define ALS_INTT			7

static int set_psensor_intr_threshold(uint16_t low_thd, uint16_t high_thd);

#if PS_INTERRUPT_MODE
static void epl_sensor_irq_do_work(struct work_struct *work);
static DECLARE_WORK(epl_sensor_irq_work, epl_sensor_irq_do_work);
#endif

static void polling_do_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(polling_work, polling_do_work);

/* primitive raw data from I2C */
struct epl_raw_data {
	u8 raw_bytes[PACKAGE_SIZE];
	u16 ps_state;
	u16 ps_int_state;
	u16 ps_ch1_raw;
	u16 als_ch1_raw;
	u16 ps_min_raw;
	u16 ps_condition;
	u16 ps_cal_h;
	u16 ps_cal_l;
	u16 ps_sta;
#if DYNAMIC_INTT
	u16 als_lux;
#endif
};

struct elan_epl_data {
	struct i2c_client *client;
	struct regulator *vdd;
	struct regulator *vio;

	unsigned int als_poll_delay;
	unsigned int ps_poll_delay;
	struct input_dev *als_input_dev;
	struct input_dev *ps_input_dev;

	struct sensors_classdev als_cdev;
	struct sensors_classdev ps_cdev;

	struct workqueue_struct *epl_wq;
	int intr_pin;
	int (*power) (int on);

	int ps_opened;
	int als_opened;

	unsigned int ps_th_l;	/* P_SENSOR_LTHD */
	unsigned int ps_th_h;	/* P_SENSOR_HTHD */
	int enable_pflag;
	int enable_lflag;
	int read_flag;
	int irq_gpio;
	unsigned int irq_gpio_flags;
	int cali_on;
	int cali_value;
	int power_enabled;
	struct wake_lock elan_wake_lock;
};

static struct wake_lock g_ps_wlock;
struct elan_epl_data *epl_data;
static struct epl_raw_data gRawData;

static const char ElanPsensorName[] = "proximity";
static const char ElanALsensorName[] = "light";

static int psensor_mode_suspend;

static struct sensors_classdev sensors_light_cdev = {
	.name = "light",
	.vendor = "elan",
	.version = 1,
	.handle = SENSORS_LIGHT_HANDLE,
	.type = SENSOR_TYPE_LIGHT,
	.max_range = "30000",
	.resolution = "0.0125",
	.sensor_power = "0.20",
	.min_delay = 1000, 	/* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.flags = 2,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static struct sensors_classdev sensors_proximity_cdev = {
	.name = "proximity",
	.vendor = "elan",
	.version = 1,
	.handle = SENSORS_PROXIMITY_HANDLE,
	.type = SENSOR_TYPE_PROXIMITY,
	.max_range = "5",
	.resolution = "5.0",
	.sensor_power = "3",
	.min_delay = 1000, 	/* in microseconds */
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.flags = 3,
	.enabled = 0,
	.delay_msec = 100,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static int elan_plsensor_power_set(struct elan_epl_data *epld, bool on);

static int elan_sensor_I2C_Write(struct i2c_client *client, uint8_t regaddr,
				 uint8_t bytecount, uint8_t txbyte,
				 uint8_t data)
{
	uint8_t buffer[2];
	int ret = 0;
	int retry;

	buffer[0] = (regaddr << 3) | bytecount;
	buffer[1] = data;

	for (retry = 0; retry < I2C_RETRY_COUNT; retry++) {
		ret = i2c_master_send(client, buffer, txbyte);

		if (ret == txbyte)
			break;
		msleep(25);
	}

	if (retry >= I2C_RETRY_COUNT) {
		pr_err(KERN_ERR "i2c write retry over %d\n", I2C_RETRY_COUNT);
		return -EINVAL;
	}

	return ret;
}

static int elan_sensor_I2C_Read(struct i2c_client *client)
{
	uint8_t buffer[RXBYTES];
	int ret = 0, i = 0;
	int retry;

	for (retry = 0; retry < I2C_RETRY_COUNT; retry++) {

		ret = i2c_master_recv(client, buffer, RXBYTES);
		if (ret == RXBYTES)
			break;
		msleep(25);
	}

	if (retry >= I2C_RETRY_COUNT) {
		pr_err("i2c read retry over %d\n", I2C_RETRY_COUNT);
		return -EINVAL;
	}

	for (i = 0; i < PACKAGE_SIZE; i++)
		gRawData.raw_bytes[i] = buffer[i];

	return ret;
}

static void elan_sensor_restart_work(void)
{
	struct elan_epl_data *epld = epl_data;
	cancel_delayed_work(&polling_work);
	queue_delayed_work(epld->epl_wq, &polling_work, msecs_to_jiffies(10));
}

#if PS_AUTO_ENABLE
static void dyn_ps_cal(struct elan_epl_data *epld)
{
	if ((gRawData.ps_ch1_raw < gRawData.ps_min_raw)
			&& (gRawData.ps_sta != 1)
			&& (gRawData.ps_condition <= DYN_CONDITION)) {
		if (gRawData.ps_ch1_raw < 400) {
			DYN_L_OFFSET = 150;
			DYN_H_OFFSET = 350;
		} else if (gRawData.ps_ch1_raw >= 400 && gRawData.ps_ch1_raw < 800) {
			DYN_L_OFFSET = 300;
			DYN_H_OFFSET = 500;
		} else {
			DYN_L_OFFSET = 500;
			DYN_H_OFFSET = 800;
		}
		epl_info("elan DYN_L_OFFSET = %d DYN_H_OFFSET = %d\n", DYN_L_OFFSET, DYN_H_OFFSET);
		gRawData.ps_min_raw = gRawData.ps_ch1_raw;
		epld->ps_th_l = gRawData.ps_ch1_raw + DYN_L_OFFSET;
		epld->ps_th_h = gRawData.ps_ch1_raw + DYN_H_OFFSET;
		set_psensor_intr_threshold(epld->ps_th_l, epld->ps_th_h);
		epl_info("dyn ps raw = %d, min = %d, condition = %d\n dyn h_thre = %d, l_thre = %d, ps_state = %d",
		gRawData.ps_ch1_raw, gRawData.ps_min_raw, gRawData.ps_condition, epld->ps_th_h, epld->ps_th_l, gRawData.ps_state);
	}
}
#endif

static int elan_sensor_psensor_enable(struct elan_epl_data *epld)
{
	int ret;
	uint8_t regdata = 0;
	struct i2c_client *client = epld->client;
	LOG_FUN();
	epl_info("--- Proximity sensor Enable ---\n");

	ret =
		elan_sensor_I2C_Write(client, REG_9, W_SINGLE_BYTE, 0x02,
				  EPL_INT_DISABLE);




	regdata = EPL_SENSING_1_TIME | EPL_PS_MODE | EPL_M_GAIN;
	regdata =
		regdata | (PS_INTERRUPT_MODE ? EPL_C_SENSING_MODE :
			   EPL_S_SENSING_MODE);
	ret =
		elan_sensor_I2C_Write(client, REG_0, W_SINGLE_BYTE, 0X02, regdata);




	regdata = PS_INTT << 4 | EPL_PST_1_TIME | EPL_14BIT_ADC;
	ret =
		elan_sensor_I2C_Write(client, REG_1, W_SINGLE_BYTE, 0X02, regdata);

	if (cali_flag == 0) {
		set_psensor_intr_threshold(epld->ps_th_l, epld->ps_th_h);
	} else {
		set_psensor_intr_threshold(epld->cali_value+LOW_OFFSET, epld->cali_value+HIGH_OFFSET);
	}

	ret =
		elan_sensor_I2C_Write(client, REG_7, W_SINGLE_BYTE, 0X02,
				  EPL_C_RESET);
	ret =
		elan_sensor_I2C_Write(client, REG_7, W_SINGLE_BYTE, 0x02,
				  EPL_C_START_RUN);

#if PS_INTERRUPT_MODE
	if (epld->enable_pflag) {
		msleep(PS_DELAY);
		elan_sensor_I2C_Write(client, REG_13, R_SINGLE_BYTE, 0x01, 0);
		elan_sensor_I2C_Read(client);
		gRawData.ps_state = !((gRawData.raw_bytes[0] & 0x04) >> 2);
		gRawData.ps_sta = ((gRawData.raw_bytes[0]&0x02)>>1);

#if PS_AUTO_ENABLE
		elan_sensor_I2C_Write(client, REG_16, R_TWO_BYTE, 0x01, 0x00);
		elan_sensor_I2C_Read(client);
		gRawData.ps_ch1_raw = (gRawData.raw_bytes[1] << 8) | gRawData.raw_bytes[0];

		elan_sensor_I2C_Write(client, REG_14, R_TWO_BYTE, 0x01, 0x00);
		elan_sensor_I2C_Read(client);
		gRawData.ps_condition = ((gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0]);

		dyn_ps_cal(epld);
#endif

		if (gRawData.ps_state != gRawData.ps_int_state) {
			elan_sensor_I2C_Write(client, REG_9, W_SINGLE_BYTE,
						  0x02, EPL_INT_FRAME_ENABLE);
		} else {
			elan_sensor_I2C_Write(client, REG_9, W_SINGLE_BYTE,
						  0x02, EPL_INT_ACTIVE_LOW);
		}

	} else {
		elan_sensor_I2C_Write(client, REG_9, W_SINGLE_BYTE, 0x02,
					  EPL_INT_ACTIVE_LOW);
	}
#endif

	if (ret != 0x02)
		epl_info("P-sensor i2c err\n");

	return ret;
}

static int elan_sensor_lsensor_enable(struct elan_epl_data *epld)
{
	int ret;
	uint8_t regdata = 0;
	struct i2c_client *client = epld->client;
	LOG_FUN();
	epl_info("--- ALS sensor Enable --\n");

	regdata = EPL_INT_DISABLE;
	ret =
		elan_sensor_I2C_Write(client, REG_9, W_SINGLE_BYTE, 0x02, regdata);
#if DYNAMIC_INTT /*DYNAMIC_INTT*/

	regdata = EPL_S_SENSING_MODE | EPL_SENSING_16_TIME | EPL_ALS_MODE | dynamic_intt_gain;
	ret = elan_sensor_I2C_Write(client, REG_0, W_SINGLE_BYTE, 0X02, regdata);

	regdata = dynamic_intt_intt<<4 | EPL_PST_1_TIME | EPL_8BIT_ADC;
	ret = elan_sensor_I2C_Write(client, REG_1, W_SINGLE_BYTE, 0X02, regdata);

#else
	regdata =
		EPL_S_SENSING_MODE | EPL_SENSING_8_TIME | EPL_ALS_MODE |
		EPL_AUTO_GAIN;
	ret =
		elan_sensor_I2C_Write(client, REG_0, W_SINGLE_BYTE, 0X02, regdata);

	regdata = ALS_INTT << 4 | EPL_PST_1_TIME | EPL_10BIT_ADC;
	ret =
		elan_sensor_I2C_Write(client, REG_1, W_SINGLE_BYTE, 0X02, regdata);
#endif
	ret =
		elan_sensor_I2C_Write(client, REG_10, W_SINGLE_BYTE, 0x02,
				  EPL_GO_MID);
	ret =
		elan_sensor_I2C_Write(client, REG_11, W_SINGLE_BYTE, 0x02,
				  EPL_GO_LOW);

	ret =
		elan_sensor_I2C_Write(client, REG_7, W_SINGLE_BYTE, 0X02,
				  EPL_C_RESET);
	ret =
		elan_sensor_I2C_Write(client, REG_7, W_SINGLE_BYTE, 0x02,
				  EPL_C_START_RUN);

	msleep(ALS_DELAY);
	if (ret != 0x02)
		epl_info(" ALS-sensor i2c err\n");

	return ret;

}

static void elan_epl_ps_poll_rawdata(void)
{
	struct elan_epl_data *epld = epl_data;
	struct i2c_client *client = epld->client;

	elan_sensor_I2C_Write(epld->client, REG_7, W_SINGLE_BYTE, 0x02,
				  EPL_DATA_LOCK);

	elan_sensor_I2C_Write(client, REG_13, R_SINGLE_BYTE, 0x01, 0);
	elan_sensor_I2C_Read(client);
	gRawData.ps_state = !((gRawData.raw_bytes[0] & 0x04) >> 2);
	elan_sensor_I2C_Write(client, REG_16, R_TWO_BYTE, 0x01, 0x00);
	elan_sensor_I2C_Read(client);
	gRawData.ps_ch1_raw =
		(gRawData.raw_bytes[1] << 8) | gRawData.raw_bytes[0];

	elan_sensor_I2C_Write(epld->client, REG_7, W_SINGLE_BYTE, 0x02,
				  EPL_DATA_UNLOCK);

	epl_info("### ps_ch1_raw_data  (%d), value(%d) ###\n\n",
		 gRawData.ps_ch1_raw, gRawData.ps_state);
	printk("xmm ps = %d\n", gRawData.ps_state);
	input_report_abs(epld->ps_input_dev, ABS_DISTANCE, gRawData.ps_state);
	input_sync(epld->ps_input_dev);
}


#if DYNAMIC_INTT /
long raw_convert_to_lux(u16 raw_data)
{
	long lux = 0;

	lux =  raw_data * (c_gain / als_dynamic_intt_intt_value[dynamic_intt_idx]);
	epl_info("[%s]:c_gain=%d, raw_data=%d, lux=%ld\r\n", __func__, c_gain, raw_data, lux);

	if (lux >= (dynamic_intt_max_lux*1000)) {
		epl_info("raw_convert_to_lux: change max lux\r\n");
		lux = dynamic_intt_max_lux * 1000;
   }
	else if (lux <= (dynamic_intt_min_lux*1000)) {
		epl_info("raw_convert_to_lux: change min lux\r\n");
		lux = dynamic_intt_min_lux * 1000;
   }

	return lux;
}
#endif /*DYNAMIC_INTT*/

static void elan_epl_als_rawdata(void)
{
	struct elan_epl_data *epld = epl_data;
	struct i2c_client *client = epld->client;
	static int last_lux = 0, active_count;
#if DYNAMIC_INTT /*DYNAMIC_INTT*/
	uint32_t now_lux, lux_tmp;
	u16 raw = 256;
	int als_com = 0;

	elan_sensor_I2C_Write(client, REG_16, R_TWO_BYTE, 0x01, 0x00);
	elan_sensor_I2C_Read(client);
	raw = (gRawData.raw_bytes[1]<<8) | gRawData.raw_bytes[0];

	elan_sensor_I2C_Write(client, REG_13, R_SINGLE_BYTE, 0x01, 0);
	elan_sensor_I2C_Read(client);
	als_com = (gRawData.raw_bytes[0]&0x04)>>2;
	epl_info("dynamic_intt_idx=%d, als_dynamic_intt_intt_value=%d, dynamic_intt_gain=%d, als_raw=%d, als_com=%d\r\n",
												dynamic_intt_idx, als_dynamic_intt_intt_value[dynamic_intt_idx], dynamic_intt_gain, raw, als_com);

	if (als_com == 1) {
		if (dynamic_intt_idx == (als_dynamic_intt_intt_num - 1)) {
			lux_tmp = dynamic_intt_max_lux * 1000;
		} else {
			raw  = dynamic_intt_high_thr;
			gRawData.als_ch1_raw = dynamic_intt_high_thr;
			lux_tmp = raw_convert_to_lux(raw);
			dynamic_intt_idx++;
		}
		epl_info(">>>>>>>>>>>>>>>>>>>>>>>> channel output has saturated! \r\n");

	} else {
		if (raw > dynamic_intt_high_thr) {
			if (dynamic_intt_idx == (als_dynamic_intt_intt_num - 1)) {
				lux_tmp = dynamic_intt_max_lux * 1000;
				epl_info(">>>>>>>>>>>>>>>>>>>>>>>> INTT_MAX_LUX\r\n");
			} else {
				raw  = dynamic_intt_high_thr;
				gRawData.als_ch1_raw = dynamic_intt_high_thr;
				lux_tmp = raw_convert_to_lux(raw);
				dynamic_intt_idx++;
				epl_info(">>>>>>>>>>>>>>>>>>>>>>>>change INTT high: %d, raw: %d \r\n", dynamic_intt_idx, raw);
			}
		} else if (raw < dynamic_intt_low_thr) {
			if (dynamic_intt_idx == 0) {
				lux_tmp = dynamic_intt_min_lux * 1000;
				epl_info(">>>>>>>>>>>>>>>>>>>>>>>> INTT_MIN_LUX\r\n");
			} else {
				raw  = dynamic_intt_low_thr;
				gRawData.als_ch1_raw = dynamic_intt_low_thr;
				lux_tmp = raw_convert_to_lux(raw);
				dynamic_intt_idx--;
				epl_info(">>>>>>>>>>>>>>>>>>>>>>>>change INTT low: %d, raw: %d \r\n", dynamic_intt_idx, raw);
			}
		} else {
			lux_tmp = raw_convert_to_lux(raw);
		}
	}

	now_lux = lux_tmp / dynamic_intt_min_unit;
	gRawData.als_lux = now_lux;

	epl_info("-------------------  ALS raw = %d, now_lux = %d   \r\n", raw, now_lux);

	if (als_report_count == (als_dynamic_intt_intt_num - 1)) {
			if ((now_lux == 0 && last_lux == now_lux) && active_count++ < 5) {
			input_report_abs(epld->als_input_dev, ABS_MISC, active_count%2);
			input_sync(epld->als_input_dev);
			} else {
			input_report_abs(epld->als_input_dev, ABS_MISC, now_lux);
			input_sync(epld->als_input_dev);
		}
	} else {
		als_report_count++;
		if (als_report_count >= (als_dynamic_intt_intt_num - 1)) {
			als_report_count = (als_dynamic_intt_intt_num - 1);
		}
		epl_info("[%s]: als_report_count=%d \r\n", __func__, als_report_count);
	}

	dynamic_intt_intt = als_dynamic_intt_intt[dynamic_intt_idx];
	dynamic_intt_gain = als_dynamic_intt_gain[dynamic_intt_idx];
	dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
	dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];

#else
	uint32_t lux;

	elan_sensor_I2C_Write(client, REG_16, R_TWO_BYTE, 0x01, 0x00);
	elan_sensor_I2C_Read(client);
	gRawData.als_ch1_raw =
			(gRawData.raw_bytes[1] << 8) | gRawData.raw_bytes[0];

	lux = (gRawData.als_ch1_raw * LUX_PER_COUNT) / 1000;
	if (lux > MAX_LUX)
		lux = MAX_LUX;

	epl_info("-------------------  ALS raw = %d, lux = %d\n\n",
		 gRawData.als_ch1_raw, lux);

	input_report_abs(epld->als_input_dev, ABS_MISC, lux);
	input_sync(epld->als_input_dev);
#endif
}

static int set_psensor_intr_threshold(uint16_t low_thd, uint16_t high_thd)
{
	int ret = 0;
	struct elan_epl_data *epld = epl_data;
	struct i2c_client *client = epld->client;

	uint8_t high_msb, high_lsb, low_msb, low_lsb;

	high_msb = (uint8_t) (high_thd >> 8);
	high_lsb = (uint8_t) (high_thd & 0x00ff);
	low_msb = (uint8_t) (low_thd >> 8);
	low_lsb = (uint8_t) (low_thd & 0x00ff);

	elan_sensor_I2C_Write(client, REG_2, W_SINGLE_BYTE, 0x02, high_lsb);
	elan_sensor_I2C_Write(client, REG_3, W_SINGLE_BYTE, 0x02, high_msb);
	elan_sensor_I2C_Write(client, REG_4, W_SINGLE_BYTE, 0x02, low_lsb);
	elan_sensor_I2C_Write(client, REG_5, W_SINGLE_BYTE, 0x02, low_msb);

	return ret;
}

#if PS_INTERRUPT_MODE
static void epl_sensor_irq_do_work(struct work_struct *work)
{
	struct elan_epl_data *epld = epl_data;
	struct i2c_client *client = epld->client;
	int mode = 0;
	LOG_FUN();

	elan_sensor_I2C_Write(epld->client, REG_7, W_SINGLE_BYTE, 0x02,
				  EPL_DATA_LOCK);
	elan_sensor_I2C_Write(client, REG_13, R_SINGLE_BYTE, 0x01, 0);
	elan_sensor_I2C_Read(client);
	mode = gRawData.raw_bytes[0] & (3 << 4);

	if (mode == CMC_MODE_PS && epld->enable_pflag) {
		gRawData.ps_int_state = !((gRawData.raw_bytes[0] & 0x04) >> 2);
		elan_epl_ps_poll_rawdata();
	} else {
		epl_info("error: interrupt in als\n");
	}

	elan_sensor_I2C_Write(client, REG_9, W_SINGLE_BYTE, 0x02,
				  EPL_INT_ACTIVE_LOW);

	elan_sensor_I2C_Write(client, REG_7, W_SINGLE_BYTE, 0x02,
				  EPL_DATA_UNLOCK);

	enable_irq(client->irq);
}

static irqreturn_t elan_sensor_irq_handler(int irqNo, void *handle)
{
	struct i2c_client *client = (struct i2c_client *)handle;
	struct elan_epl_data *epld = i2c_get_clientdata(client);

	disable_irq_nosync(client->irq);
	queue_work(epld->epl_wq, &epl_sensor_irq_work);

	return IRQ_HANDLED;
}
#endif

static void polling_do_work(struct work_struct *work)
{
	struct elan_epl_data *epld = epl_data;
	struct i2c_client *client = epld->client;

	bool isInterleaving = epld->enable_pflag == 1
		&& epld->enable_lflag == 1;
	bool isAlsOnly = epld->enable_pflag == 0 && epld->enable_lflag == 1;
	bool isPsOnly = epld->enable_pflag == 1 && epld->enable_lflag == 0;

	cancel_delayed_work(&polling_work);

	if (isAlsOnly || isInterleaving || (isPsOnly && PS_INTERRUPT_MODE == 0) || PS_AUTO_ENABLE) {

		queue_delayed_work(epld->epl_wq, &polling_work, msecs_to_jiffies(ALS_DELAY+2*PS_DELAY+30));
	}

	if (isAlsOnly || isInterleaving) {
		printk("xmm  als only\n");
		elan_sensor_lsensor_enable(epld);
		elan_epl_als_rawdata();
		if (isInterleaving) {
			elan_sensor_psensor_enable(epld);
			if (PS_INTERRUPT_MODE == 0) {
				elan_epl_ps_poll_rawdata();
			}
		}
	} else if (isPsOnly) {
	printk("xmm  ps only\n");
		elan_sensor_psensor_enable(epld);

		if (PS_INTERRUPT_MODE) {

		} else {
			elan_epl_ps_poll_rawdata();
		}
	} else {
		elan_sensor_I2C_Write(client, REG_9, W_SINGLE_BYTE, 0x02,
					  EPL_INT_DISABLE);
		elan_sensor_I2C_Write(client, REG_0, W_SINGLE_BYTE, 0X02,
					  EPL_S_SENSING_MODE);
		cancel_delayed_work(&polling_work);
	}
}

static ssize_t elan_ps_threshold_show(struct device *ddri,
				   struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct elan_epl_data *epld = epl_data;


	len += snprintf(buf+len, PAGE_SIZE-len, "gRawData.ps_als_factory(H/L): %d/%d \r\n", gRawData.ps_cal_h, gRawData.ps_cal_l);
	len += snprintf(buf+len, PAGE_SIZE-len, "ps_threshold(H/L): %d/%d \r\n", epld->ps_th_h, epld->ps_th_l);
	return len;
}

static ssize_t elan_ps_raw_show(struct device *ddri,
				   struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += snprintf(buf+len, PAGE_SIZE-len, "gRawData.ps_raw(H/L): %d/ \r\n", gRawData.ps_ch1_raw);
	return len;
}

static ssize_t elan_ls_operationmode_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	uint16_t mode = 0;
	/* struct elan_epl_data *epld = epl_data; */
	struct i2c_client *client = to_i2c_client(dev);
	struct elan_epl_data *epld = i2c_get_clientdata(client);

	sscanf(buf, "%hu", &mode);
	epl_info("==>[operation mode]=%d\n", mode);

	if (mode == 0) {
		epld->enable_lflag = 0;
		epld->enable_pflag = 0;
	} else if (mode == 1) {
		epld->enable_lflag = 1;
		epld->enable_pflag = 0;
	} else if (mode == 2) {
		epld->enable_lflag = 0;
		epld->enable_pflag = 1;
	} else if (mode == 3) {
		epld->enable_lflag = 1;
		epld->enable_pflag = 1;
	} else {
		epl_info("0: none\n1: als only\n2: ps only\n3: interleaving");
	}
#if DYNAMIC_INTT
	if (epld->enable_lflag == 1) {
		als_report_count = 0;
		dynamic_intt_idx = dynamic_intt_init_idx;
		dynamic_intt_intt = als_dynamic_intt_intt[dynamic_intt_idx];
		dynamic_intt_gain = als_dynamic_intt_gain[dynamic_intt_idx];
		dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
		dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];
   }
#endif
	elan_sensor_restart_work();
	return count;
}

static ssize_t elan_ls_operationmode_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct elan_epl_data *epld = epl_data;
	long *tmp = (long *)buf;
	uint16_t mode = 0;
	LOG_FUN();

	if (epld->enable_pflag == 0 && epld->enable_lflag == 0)
		mode = 0;
	else if (epld->enable_pflag == 0 && epld->enable_lflag == 1)
		mode = 1;
	else if (epld->enable_pflag == 1 && epld->enable_lflag == 0)
		mode = 2;
	else if (epld->enable_pflag == 1 && epld->enable_lflag == 1)
		mode = 3;

	tmp[0] = mode;

	/*return sprintf(buf, "%d\n", mode); */
	return snprintf(buf, PAGE_SIZE, "%d\n", mode);
}

static ssize_t elan_ls_status_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct elan_epl_data *epld = epl_data;
	u16 ch1;

	if (!epl_data) {
		epl_info("epl_data is null!!\n");
		return 0;
	}
#if DYNAMIC_INTT
	if (epld->enable_lflag == 0) {
		epld->enable_lflag = 1;
		elan_sensor_restart_work();
		msleep(ALS_DELAY);
	}

	ch1 = gRawData.als_lux;
#else
	elan_sensor_I2C_Write(epld->client, REG_7, W_SINGLE_BYTE, 0x02,
				  EPL_DATA_LOCK);

	elan_sensor_I2C_Write(epld->client, REG_16, R_TWO_BYTE, 0x01, 0x00);
	elan_sensor_I2C_Read(epld->client);
	ch1 = (gRawData.raw_bytes[1] << 8) | gRawData.raw_bytes[0];
	epl_info("ch1 raw_data = %d\n", ch1);

	elan_sensor_I2C_Write(epld->client, REG_7, W_SINGLE_BYTE, 0x02,
				  EPL_DATA_UNLOCK);
#endif
	/* return sprintf(buf, "%d\n", ch1); */
	return snprintf(buf, PAGE_SIZE, "%d\n", ch1);
}

#if DYNAMIC_INTT
static ssize_t epl_sensor_store_c_gain(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int c;
	LOG_FUN();

	sscanf(buf, "%d", &c);

	c_gain = c;

	epl_info("c_gain = %d \r\n", c_gain);

	return count;
}
#endif

static DEVICE_ATTR(elan_ls_operationmode, 0440,
		   elan_ls_operationmode_show, elan_ls_operationmode_store);
static DEVICE_ATTR(elan_ls_status, 0440, elan_ls_status_show,
		   NULL);
static DEVICE_ATTR(elan_ps_threshold, 0440, elan_ps_threshold_show,
		   NULL);
static DEVICE_ATTR(elan_ps_raw, 0440, elan_ps_raw_show,
		   NULL);
#if DYNAMIC_INTT
static DEVICE_ATTR(c_gain, 0440, NULL, epl_sensor_store_c_gain);
#endif


static struct attribute *ets_attributes[] = {
	&dev_attr_elan_ls_operationmode.attr,
	&dev_attr_elan_ls_status.attr,
	&dev_attr_elan_ps_threshold.attr,
	&dev_attr_elan_ps_raw.attr,
#if DYNAMIC_INTT
	&dev_attr_c_gain.attr,
#endif
	NULL,
};

static struct attribute_group ets_attr_group = {
	.attrs = ets_attributes,
};

static int elan_als_open(struct inode *inode, struct file *file)
{
	struct elan_epl_data *epld = epl_data;

	LOG_FUN();

	if (epld->als_opened)
		return -EBUSY;
	epld->als_opened = 1;

	return 0;
}

static ssize_t elan_als_read(struct file *file, char __user *buffer, size_t count,
			 loff_t *ppos)
{
	struct elan_epl_data *epld = epl_data;
	int buf[1];
	if (epld->read_flag == 1) {
#if DYNAMIC_INTT
		buf[0] = gRawData.als_lux;
#else
		buf[0] = gRawData.als_ch1_raw;
#endif
		if (copy_to_user(buffer, &buf, sizeof(buf)))
			return 0;
		epld->read_flag = 0;
		return 12;
	} else {
		return 0;
	}
}

static int elan_als_release(struct inode *inode, struct file *file)
{
	struct elan_epl_data *epld = epl_data;

	LOG_FUN();

	epld->als_opened = 0;

	return 0;
}

static long elan_als_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	int flag;
	unsigned long buf[1];
	struct elan_epl_data *epld = epl_data;

	void __user *argp = (void __user *)arg;

	epl_info("als io ctrl cmd %d\n", _IOC_NR(cmd));

	switch (cmd) {
	case ELAN_EPL6800_IOCTL_GET_LFLAG:

		epl_info("elan ambient-light IOCTL Sensor get lflag\n");
		flag = epld->enable_lflag;
		if (copy_to_user(argp, &flag, sizeof(flag)))
			return -EFAULT;

		epl_info("elan ambient-light Sensor get lflag %d\n", flag);
		break;

	case ELAN_EPL6800_IOCTL_ENABLE_LFLAG:

		epl_info("elan ambient-light IOCTL Sensor set lflag\n");
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;
#if DYNAMIC_INTT
		als_report_count = 0;
		dynamic_intt_idx = dynamic_intt_init_idx;
		dynamic_intt_intt = als_dynamic_intt_intt[dynamic_intt_idx];
		dynamic_intt_gain = als_dynamic_intt_gain[dynamic_intt_idx];
		dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
		dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];
#endif

		epld->enable_lflag = flag;
		elan_sensor_restart_work();

		epl_info("elan ambient-light Sensor set lflag %d\n", flag);
		break;

	case ELAN_EPL6800_IOCTL_GETDATA:
#if DYNAMIC_INTT
		buf[0] = (unsigned long)gRawData.als_lux;

#else
		buf[0] = (unsigned long)gRawData.als_ch1_raw;
#endif
		if (copy_to_user(argp, &buf, sizeof(buf)))
			return -EFAULT;

		break;

	default:
		pr_err("invalid cmd %d\n", _IOC_NR(cmd));
		return -EINVAL;
	}

	return 0;

}

static const struct file_operations elan_als_fops = {
	.owner = THIS_MODULE,
	.open = elan_als_open,
	.read = elan_als_read,
	.release = elan_als_release,
	.unlocked_ioctl = elan_als_ioctl,
};

static struct miscdevice elan_als_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "elan_als",
	.fops = &elan_als_fops,
};

static int elan_ps_open(struct inode *inode, struct file *file)
{
	struct elan_epl_data *epld = epl_data;

	LOG_FUN();

	if (epld->ps_opened)
		return -EBUSY;

	epld->ps_opened = 1;

	return 0;
}

static int elan_ps_release(struct inode *inode, struct file *file)
{
	struct elan_epl_data *epld = epl_data;

	LOG_FUN();

	epld->ps_opened = 0;

	psensor_mode_suspend = 0;

	return 0;
}

static long elan_ps_ioctl(struct file *file, unsigned int cmd,
			  unsigned long arg)
{
	int value;
	int flag;
	struct elan_epl_data *epld = epl_data;

	void __user *argp = (void __user *)arg;

	epl_info("ps io ctrl cmd %d\n", _IOC_NR(cmd));

	switch (cmd) {
	case ELAN_EPL6800_IOCTL_GET_PFLAG:

		epl_info("elan Proximity Sensor IOCTL get pflag\n");
		flag = epld->enable_pflag;
		if (copy_to_user(argp, &flag, sizeof(flag)))
			return -EFAULT;

		epl_info("elan Proximity Sensor get pflag %d\n", flag);
		break;

	case ELAN_EPL6800_IOCTL_ENABLE_PFLAG:
		epl_info("elan Proximity IOCTL Sensor set pflag\n");
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;

		epld->enable_pflag = flag;
		elan_sensor_restart_work();

		epl_info("elan Proximity Sensor set pflag %d\n", flag);
		break;

	case ELAN_EPL6800_IOCTL_GETDATA:

		value = gRawData.ps_ch1_raw;
		if (copy_to_user(argp, &value, sizeof(value)))
			return -EFAULT;

		epl_info("elan proximity Sensor get data (%d)\n", value);
		break;


	case ALSPS_REC_PS_DATA_FOR_CALI:

		if (cali_flag == 1) {
			if (copy_from_user(&value, argp, sizeof(value)))
				return -EFAULT;

			set_psensor_intr_threshold(value+LOW_OFFSET, value+HIGH_OFFSET);
			epld->cali_value = value;
			epl_info("pppppxmmmmm---th_l %d, th_h %d\n", value+LOW_OFFSET, value+HIGH_OFFSET);
		}

			break;
	case ALSPS_GET_PS_RAW_DATA_FOR_CALI:

		msleep(PS_DELAY);
		msleep(PS_DELAY);
		if (copy_to_user(argp, &gRawData.ps_ch1_raw, sizeof(gRawData.ps_ch1_raw)))
			return -EFAULT;

			break;

	default:
		pr_err("invalid cmd %d\n", _IOC_NR(cmd));
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations elan_ps_fops = {
	.owner = THIS_MODULE,
	.open = elan_ps_open,
	.release = elan_ps_release,
	.unlocked_ioctl = elan_ps_ioctl
};

static struct miscdevice elan_ps_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "psensor",
	.fops = &elan_ps_fops
};

static int initial_sensor(struct elan_epl_data *epld)
{
	struct i2c_client *client = epld->client;

	int ret = 0;

	epl_info("initial_sensor enter!\n");

	ret = elan_sensor_I2C_Read(client);

	if (ret < 0)
		return -EINVAL;

	elan_sensor_I2C_Write(client, REG_0, W_SINGLE_BYTE, 0x02,
				  EPL_S_SENSING_MODE);
	elan_sensor_I2C_Write(client, REG_9, W_SINGLE_BYTE, 0x02,
			EPL_INT_DISABLE);
	if (cali_flag == 0) {
		set_psensor_intr_threshold(epld->ps_th_l, epld->ps_th_h);
	} else {
		set_psensor_intr_threshold(epld->cali_value+LOW_OFFSET, epld->cali_value+HIGH_OFFSET);
	}

	msleep(20);

	epld->enable_lflag = 0;
	epld->enable_pflag = 0;

	return ret;
}

static int elan_plsensor_power_set(struct elan_epl_data *epld, bool on)
{
	int rc = 0;
	if (!on && epld->power_enabled) {

		rc = regulator_disable(epld->vio);
		if (rc) {
			dev_err(&epld->client->dev,
					"Regulator vio disable failed rc=%d\n", rc);
			goto err_vio_disable;
		}

		rc = regulator_disable(epld->vdd);
		if (rc) {
			dev_err(&epld->client->dev,
					"Regulator vdd disable failed rc=%d\n", rc);
			goto err_vdd_disable;
		}

		epld->power_enabled = false;
		return rc;
	} else if (on && !epld->power_enabled) {
		rc = regulator_enable(epld->vdd);
		if (rc) {
			dev_err(&epld->client->dev,
					"Regulator vdd enable failed rc=%d\n", rc);
			goto err_vdd_enable;
		}

		rc = regulator_enable(epld->vio);
		if (rc) {
			dev_err(&epld->client->dev,
					"Regulator vio enable failed rc=%d\n", rc);
			goto err_vio_enable;
		}
		epld->power_enabled = true;
		msleep(50);
		return rc;
	} else {
		dev_warn(&epld->client->dev,
				"Power on=%d. enabled=%d\n",
				on, epld->power_enabled);
	}
err_vio_enable:
	regulator_disable(epld->vio);
err_vdd_enable:
	return rc;
err_vio_disable:
	if (regulator_enable(epld->vdd))
		dev_warn(&epld->client->dev, "Regulator vdd enable failed\n");
err_vdd_disable:
	return rc;
}

static int elan_power_init(struct elan_epl_data *epld, bool on)
{
	int rc;

	LOG_FUN();

	if (!on)
		goto pwr_deinit;

	epld->vdd = regulator_get(&epld->client->dev, "vdd");
	if (IS_ERR(epld->vdd)) {
		rc = PTR_ERR(epld->vdd);
		dev_err(&epld->client->dev,
			"Regulator get failed vdd rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(epld->vdd) > 0) {
		rc = regulator_set_voltage(epld->vdd, EPL2182_VDD_MIN_UV,
					   EPL2182_VDD_MAX_UV);
		if (rc) {
			dev_err(&epld->client->dev,
				"Regulator set_vtg failed vdd rc=%d\n", rc);
			goto reg_vdd_put;
		}
	}

	epld->vio = regulator_get(&epld->client->dev, "vio");
	if (IS_ERR(epld->vio)) {
		rc = PTR_ERR(epld->vio);
		dev_err(&epld->client->dev,
			"Regulator get failed vio rc=%d\n", rc);
		goto reg_vdd_set_vtg;
	}

	if (regulator_count_voltages(epld->vio) > 0) {
		rc = regulator_set_voltage(epld->vio, EPL2182_VIO_MIN_UV,
					   EPL2182_VIO_MAX_UV);
		if (rc) {
			dev_err(&epld->client->dev,
				"Regulator set_vtg failed vio rc=%d\n", rc);
			goto reg_vio_put;
		}
	}

	return 0;

reg_vio_put:
	regulator_put(epld->vio);
reg_vdd_set_vtg:
	if (regulator_count_voltages(epld->vdd) > 0)
		regulator_set_voltage(epld->vdd, 0, EPL2182_VDD_MAX_UV);
reg_vdd_put:
	regulator_put(epld->vdd);
	return rc;

pwr_deinit:
	if (regulator_count_voltages(epld->vdd) > 0)
		regulator_set_voltage(epld->vdd, 0, EPL2182_VDD_MAX_UV);

	regulator_put(epld->vdd);

	if (regulator_count_voltages(epld->vio) > 0)
		regulator_set_voltage(epld->vio, 0, EPL2182_VIO_MAX_UV);

	regulator_put(epld->vio);
	return 0;
}

static int lightsensor_setup(struct elan_epl_data *epld)
{
	int err = 0;
	epl_info("lightsensor_setup enter.\n");

	epld->als_input_dev = input_allocate_device();
	if (!epld->als_input_dev) {
		pr_err("could not allocate ls input device\n");
		return -ENOMEM;
	}
	epld->als_input_dev->name = ElanALsensorName;
	set_bit(EV_ABS, epld->als_input_dev->evbit);
	input_set_abs_params(epld->als_input_dev, ABS_MISC, 0, 9, 0, 0);

	err = input_register_device(epld->als_input_dev);
	if (err < 0) {
		pr_err("can not register ls input device\n");
		goto err_free_ls_input_device;
	}

	err = misc_register(&elan_als_device);
	if (err < 0) {
		pr_err("can not register ls misc device\n");
		goto err_unregister_ls_input_device;
	}

	return err;

err_unregister_ls_input_device:
	input_unregister_device(epld->als_input_dev);
err_free_ls_input_device:
	input_free_device(epld->als_input_dev);
	return err;
}

static int psensor_setup(struct elan_epl_data *epld)
{
	int err = 0;
	epl_info("psensor_setup enter.\n");

	epld->ps_input_dev = input_allocate_device();
	if (!epld->ps_input_dev) {
		pr_err("could not allocate ps input device\n");
		return -ENOMEM;
	}
	epld->ps_input_dev->name = ElanPsensorName;

	set_bit(EV_ABS, epld->ps_input_dev->evbit);
	input_set_abs_params(epld->ps_input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	err = input_register_device(epld->ps_input_dev);
	if (err < 0) {
		pr_err("could not register ps input device\n");
		goto err_free_ps_input_device;
	}

	err = misc_register(&elan_ps_device);
	if (err < 0) {
		pr_err("could not register ps misc device\n");
		goto err_unregister_ps_input_device;
	}

	elan_sensor_psensor_enable(epld);
	elan_epl_ps_poll_rawdata();
	elan_sensor_lsensor_enable(epld);
	return err;

err_unregister_ps_input_device:
	input_unregister_device(epld->ps_input_dev);
err_free_ps_input_device:
	input_free_device(epld->ps_input_dev);
	return err;
}

#if PS_INTERRUPT_MODE
static int setup_interrupt(struct elan_epl_data *epld)
{
	struct i2c_client *client = epld->client;

	int err = 0;
	msleep(20);
	LOG_FUN();
	if (gpio_is_valid(epld->irq_gpio)) {
		err = gpio_request(epld->irq_gpio, "elan_irq_gpio");
		if (err)
			dev_err(&client->dev, "irq gpio request failed");

		err = gpio_direction_input(epld->irq_gpio);
		if (err) {
			dev_err(&client->dev,
				"set_direction for irq gpio failed\n");
		}
	}

	err =
		request_irq(client->irq, elan_sensor_irq_handler,
			IRQF_TRIGGER_FALLING, "EPL2182", (void *)client);
	if (err < 0) {
		pr_err("request irq pin %d fail for gpio\n", err);
		goto exit_free_irq;
	}



	return err;

exit_free_irq:
	free_irq(client->irq, client);
	return err;
}
#endif

static int elan_enable_ps_sensor(struct i2c_client *client, int val)
{
	struct elan_epl_data *epld = i2c_get_clientdata(client);
	LOG_FUN();
	pr_debug("epl2182 enable PS sensor -> %d\n", val);

	if ((val != 0) && (val != 1)) {
		pr_debug("%s:store unvalid value=%d\n", __func__, val);
		return -EINVAL;
	}

	if (val == 1) {
		if (epld->enable_pflag == 0) {
			epld->enable_pflag = 1;
			enable_irq(client->irq);
			wake_lock(&g_ps_wlock);
		}
	} else {
		if (epld->enable_pflag == 1) {
			epld->enable_pflag = 0;
			disable_irq_nosync(client->irq);
			wake_unlock(&g_ps_wlock);
		}
	}

	elan_sensor_restart_work();

	return 0;
}

static int elan_enable_als_sensor(struct i2c_client *client, int val)
{
	struct elan_epl_data *epld = i2c_get_clientdata(client);

	LOG_FUN();
	pr_debug("%s: val=%d\n", __func__, val);

	if ((val != 0) && (val != 1)) {
		pr_err("%s: invalid value (val = %d)\n", __func__, val);
		return -EINVAL;
	}
#if DYNAMIC_INTT
	als_report_count = 0;
	dynamic_intt_idx = dynamic_intt_init_idx;
	dynamic_intt_intt = als_dynamic_intt_intt[dynamic_intt_idx];
	dynamic_intt_gain = als_dynamic_intt_gain[dynamic_intt_idx];
	dynamic_intt_high_thr = als_dynamic_intt_high_thr[dynamic_intt_idx];
	dynamic_intt_low_thr = als_dynamic_intt_low_thr[dynamic_intt_idx];
#endif
	if (val == 1) {
		if (epld->enable_lflag == 0)
			epld->enable_lflag = 1;

	} else {
		if (epld->enable_lflag == 1)
			epld->enable_lflag = 0;
	}
	elan_sensor_restart_work();

	return 0;
}

static int elan_als_set_enable(struct sensors_classdev *sensors_cdev,
				   unsigned int enable)
{
	struct elan_epl_data *epld =
			container_of(sensors_cdev, struct elan_epl_data, als_cdev);

	if ((enable != 0) && (enable != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enable);
		return -EINVAL;
	}

	return elan_enable_als_sensor(epld->client, enable);
}

static int elan_ps_set_enable(struct sensors_classdev *sensors_cdev,
				  unsigned int enable)
{
	struct elan_epl_data *epld =
			container_of(sensors_cdev, struct elan_epl_data, ps_cdev);

	if ((enable != 0) && (enable != 1)) {
		pr_err("%s: invalid value(%d)\n", __func__, enable);
		return -EINVAL;
	}
	if (enable == 1) {
		gRawData.ps_min_raw = 0xffff;
	}

	return elan_enable_ps_sensor(epld->client, enable);
}

/*at least 500ms, "echo A > poll_delay" means poll A msec! */
static int elan_als_poll_delay(struct sensors_classdev *sensors_cdev,
				   unsigned int delay_msec)
{
	struct elan_epl_data *epld =
			container_of(sensors_cdev, struct elan_epl_data, als_cdev);

	if (delay_msec < ALS_POLLING_RATE / 2)	/*at least 500 ms */
		delay_msec = ALS_POLLING_RATE / 2;

	epld->als_poll_delay = delay_msec;	/* convert us => ms */

	if (epld->enable_lflag == 1) {
		cancel_delayed_work(&polling_work);
		queue_delayed_work(epld->epl_wq, &polling_work,
				   msecs_to_jiffies(10));
	}
	return 0;
}

/*at least 500ms, "echo A > poll_delay" means poll A msec! */
static int elan_ps_poll_delay(struct sensors_classdev *sensors_cdev,
				  unsigned int delay_msec)
{
	struct elan_epl_data *epld =
			container_of(sensors_cdev, struct elan_epl_data, ps_cdev);

	if (delay_msec < PS_POLLING_RATE)	/* at least 500 ms */
		delay_msec = PS_POLLING_RATE;

	epld->ps_poll_delay = delay_msec;	/* convert us => ms */

	if (epld->enable_pflag == 1) {
		cancel_delayed_work(&polling_work);
		queue_delayed_work(epld->epl_wq, &polling_work,
				   msecs_to_jiffies(10));
	}
	return 0;
}

static int elan_sensor_suspend(struct device *dev)
{
	struct elan_epl_data *epld = dev_get_drvdata(dev);
	struct i2c_client *client = epld->client;
	if (!epl_data) {
		pr_err("null pointer!!\n");
		return 0;
	}

	if (epld->enable_pflag) {
		elan_sensor_psensor_enable(epld);
	} else {
		elan_sensor_I2C_Write(client, REG_7, W_SINGLE_BYTE, 0x02, EPL_C_P_DOWN);
		disable_irq_nosync(client->irq);
		elan_plsensor_power_set(epld, false);
	}
	return 0;
}

static int elan_sensor_resume(struct device *dev)
{
	struct elan_epl_data *epld = dev_get_drvdata(dev);

	if (!epl_data) {
		pr_err("null pointer!!\n");
		return 0;
	}

	if (epld->enable_pflag) {
		elan_sensor_restart_work();
	} else {
		elan_plsensor_power_set(epld, true);
		enable_irq(epld->client->irq);
	}
	return 0;
}

static int sensor_parse_dt(struct device *dev, struct elan_epl_data *epld)
{
	struct device_node *np = dev->of_node;
	struct i2c_client *client;
	unsigned int tmp;
	int rc = 0;

	client = epld->client;

	LOG_FUN();
	/* irq gpio */
	epld->irq_gpio = of_get_named_gpio_flags(np, "epl2182, irq-gpio",
						 0, &epld->irq_gpio_flags);
	if (epld->irq_gpio < 0)
		return epld->irq_gpio;

	/* ps tuning data */
	rc = of_property_read_u32(np, "epl2182, prox_th_min", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read prox_th_min\n");
		return rc;
	}

	epld->ps_th_l = tmp;

	rc = of_property_read_u32(np, "epl2182, prox_th_max", &tmp);
	if (rc) {
		dev_err(dev, "Unable to read prox_th_max\n");
		return rc;
	}

	epld->ps_th_h = tmp;

	gRawData.ps_cal_h = epld->ps_th_h;
	gRawData.ps_cal_l = epld->ps_th_l;
	return 0;
}
static int i2c_reg_check(struct i2c_client *client)
{
	epl_info("chip id REG 0x00 value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x00));
	epl_info("chip id REG 0x01 value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x08));
	epl_info("chip id REG 0x02 value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x10));
	epl_info("chip id REG 0x03 value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x18));
	epl_info("chip id REG 0x04 value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x20));
	epl_info("chip id REG 0x05 value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x28));
	epl_info("chip id REG 0x06 value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x30));
	epl_info("chip id REG 0x07 value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x38));
	epl_info("chip id REG 0x09 value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x48));
	epl_info("chip id REG 0x0D value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x68));
	epl_info("chip id REG 0x0E value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x70));
	epl_info("chip id REG 0x0F value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x71));
	epl_info("chip id REG 0x10 value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x80));
	epl_info("chip id REG 0x11 value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x88));
	epl_info("chip id REG 0x13 value = %8x\n",
		 i2c_smbus_read_byte_data(client, 0x98));

	return 0;
}

static int elan_sensor_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	int err = 0, ret = 0;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct elan_epl_data *epld;

	epl_info("elan sensor probe enter.\n");

	epld = kzalloc(sizeof(struct elan_epl_data), GFP_KERNEL);
	if (!epld)
		return -ENOMEM;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev,
				"No supported i2c func what we need?!!\n");
		err = -ENOTSUPP;
		goto i2c_fail;
	}
	epld->als_poll_delay = ALS_POLLING_RATE;
	epld->ps_poll_delay = PS_POLLING_RATE;
	epld->client = client;
	epld->cali_on = 0;

	i2c_set_clientdata(client, epld);

	epl_data = epld;

	err = sensor_parse_dt(&client->dev, epld);
	if (err) {
		pr_err("%s: sensor_parse_dt() err\n", __func__);
		return err;
	}
	err = elan_power_init(epld, true);
	if (err)
		dev_err(&client->dev, "power init failed");

	err = elan_plsensor_power_set(epld, true);
	if (err)
		dev_err(&client->dev, "power on failed\n");
	/*check i2c func and register value*/
	i2c_reg_check(client);

	epld->epl_wq = create_singlethread_workqueue("elan_sensor_wq");
	if (!epld->epl_wq) {
		pr_err("can't create workqueue\n");
		err = -ENOMEM;
		goto err_create_singlethread_workqueue;
	}

	err = lightsensor_setup(epld);
	if (err < 0) {
		pr_err("lightsensor_setup error!!\n");
		goto err_lightsensor_setup;
	}

	err = psensor_setup(epld);
	if (err < 0) {
		pr_err("psensor_setup error!!\n");
		goto err_psensor_setup;
	}

	err = initial_sensor(epld);
	if (err < 0) {
		pr_err("fail to initial sensor (%d)\n", err);
		goto err_sensor_setup;
	}

#if PS_INTERRUPT_MODE
	err = setup_interrupt(epld);
	if (err < 0) {
		pr_err("setup error!\n");
		goto err_sensor_setup;
	}
#endif
	wake_lock_init(&g_ps_wlock, WAKE_LOCK_SUSPEND, "ps_wakelock");
	err = sysfs_create_group(&client->dev.kobj, &ets_attr_group);
	if (err != 0) {
		dev_err(&client->dev, "%s:create sysfs group error", __func__);
		goto err_fail;
	}

	/* Register to sensors class */
	epld->als_cdev = sensors_light_cdev;
	epld->als_cdev.sensors_enable = elan_als_set_enable;
	epld->als_cdev.sensors_poll_delay = elan_als_poll_delay;

	epld->ps_cdev = sensors_proximity_cdev;
	epld->ps_cdev.sensors_enable = elan_ps_set_enable;
	epld->ps_cdev.sensors_poll_delay = elan_ps_poll_delay;

	ret = sensors_classdev_register(&client->dev, &epld->als_cdev);
	if (ret) {
		pr_err("%s: Unable to register to sensors class: %d\n",
				   __func__, ret);
		goto exit_remove_sysfs_group;
	}
	ret = sensors_classdev_register(&client->dev, &epld->ps_cdev);
	if (ret) {
		pr_err("%s: Unable to register to sensors class: %d\n",
				   __func__, ret);
		goto exit_create_class_sysfs;
	}

	epl_info("sensor probe success.\n");
	return err;

exit_create_class_sysfs:
	sensors_classdev_unregister(&epld->als_cdev);
exit_remove_sysfs_group:
	sysfs_remove_group(&client->dev.kobj, &ets_attr_group);
err_fail:
	free_irq(client->irq, client);
err_sensor_setup:
	misc_deregister(&elan_ps_device);
	input_unregister_device(epld->ps_input_dev);
	input_free_device(epld->ps_input_dev);
err_psensor_setup:
	input_unregister_device(epld->als_input_dev);
	input_free_device(epld->als_input_dev);
	misc_deregister(&elan_als_device);
err_lightsensor_setup:
	destroy_workqueue(epld->epl_wq);
err_create_singlethread_workqueue:
i2c_fail:
	kfree(epld);
	return err;
}

static int elan_sensor_remove(struct i2c_client *client)
{
	struct elan_epl_data *epld = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "%s: enter.\n", __func__);

	input_unregister_device(epld->als_input_dev);
	input_unregister_device(epld->ps_input_dev);
	input_free_device(epld->als_input_dev);
	input_free_device(epld->ps_input_dev);
	misc_deregister(&elan_ps_device);
	misc_deregister(&elan_als_device);
	free_irq(epld->irq_gpio, epld);
	destroy_workqueue(epld->epl_wq);
	kfree(epld);
	return 0;
}

static const struct i2c_device_id elan_sensor_id[] = {
	{ELAN_LS_2182, 0},
	{}
};

static struct of_device_id elan_match_table[] = {
	{.compatible = "elan, epl2182",},
	{},
};

static const struct dev_pm_ops elan_ps_pm_ops = {
	.suspend	= elan_sensor_suspend,
	.resume		= elan_sensor_resume,
};
static struct i2c_driver elan_sensor_driver = {
	.probe = elan_sensor_probe,
	.remove = elan_sensor_remove,
	.driver = {
		.name = ELAN_LS_2182,
		.owner = THIS_MODULE,
		.of_match_table = elan_match_table,
		.pm = &elan_ps_pm_ops,
	},
	.id_table = elan_sensor_id,
};

static int __init elan_sensor_init(void)
{
	return i2c_add_driver(&elan_sensor_driver);
}

static void __exit elan_sensor_exit(void)
{
	i2c_del_driver(&elan_sensor_driver);
}

module_init(elan_sensor_init);
module_exit(elan_sensor_exit);

MODULE_AUTHOR("Renato Pan <renato.pan@eminent-tek.com>");
MODULE_DESCRIPTION("ELAN epl2182 driver");
MODULE_LICENSE("GPL");

/*
 * This file is part of the EPL2182 sensor driver.
 * Chip is combined proximity and ambient light sensor.
 *
 * Copyright (C) 2011-2014 ELAN Corporation.
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
#ifndef __ELAN_EPL2182__
#define __ELAN_EPL2182__

/* POWER SUPPLY VOLTAGE RANGE */
#define EPL2182_VDD_MIN_UV  2000000
#define EPL2182_VDD_MAX_UV  3300000
#define EPL2182_VIO_MIN_UV  1750000
#define EPL2182_VIO_MAX_UV  1950000

#define ELAN_LS_2182 "elan-epl2182"
#define ELAN_LS_6801 "elan-epl6801"
#define ELAN_LS_6802 "elan-epl6802"
#define ELAN_LS_6803 "elan-epl6803"
#define ELAN_LS_6804 "elan-epl6804"
#define ELAN_LS_6814 "elan-epl6814"
#define ELAN_LS_2182 "elan-epl2182"
#define ELAN_LS_8852 "elan-epl8852"

#define ELAN_IOCTL_MAGIC 'c'
#define ELAN_EPL6801_IOCTL_GET_PFLAG _IOR(ELAN_IOCTL_MAGIC, 1, int *)
#define ELAN_EPL6801_IOCTL_GET_LFLAG _IOR(ELAN_IOCTL_MAGIC, 2, int *)
#define ELAN_EPL6801_IOCTL_ENABLE_PFLAG _IOW(ELAN_IOCTL_MAGIC, 3, int *)
#define ELAN_EPL6801_IOCTL_ENABLE_LFLAG _IOW(ELAN_IOCTL_MAGIC, 4, int *)
#define ELAN_EPL6801_IOCTL_GETDATA _IOR(ELAN_IOCTL_MAGIC, 5, int *)

#define ELAN_EPL6800_IOCTL_GET_PFLAG _IOR(ELAN_IOCTL_MAGIC, 1, int *)
#define ELAN_EPL6800_IOCTL_GET_LFLAG _IOR(ELAN_IOCTL_MAGIC, 2, int *)
#define ELAN_EPL6800_IOCTL_ENABLE_PFLAG _IOW(ELAN_IOCTL_MAGIC, 3, int *)
#define ELAN_EPL6800_IOCTL_ENABLE_LFLAG _IOW(ELAN_IOCTL_MAGIC, 4, int *)
#define ELAN_EPL6800_IOCTL_GETDATA _IOR(ELAN_IOCTL_MAGIC, 5, int *)

/*for epl6801 command */
#define REG_0			0X00
#define REG_1			0X01
#define REG_2			0X02
#define REG_3			0X03
#define REG_4			0X04
#define REG_5			0X05
#define REG_6			0X06
#define REG_7			0X07
#define REG_8			0X08
#define REG_9			0X09
#define REG_10			0X0A
#define REG_11			0X0B
#define REG_12			0X0C
#define REG_13			0X0D
#define REG_14			0X0E
#define REG_15			0X0F
#define REG_16			0X10
#define REG_17			0X11
#define REG_18			0X12
#define REG_19			0X13
#define REG_20			0X14
#define REG_21			0X15

#define W_SINGLE_BYTE		0X00
#define W_TWO_BYTE		0X01
#define W_THREE_BYTE		0X02
#define W_FOUR_BYTE		0X03
#define W_FIVE_BYTE		0X04
#define W_SIX_BYTE		0X05
#define W_SEVEN_BYTE		0X06
#define W_EIGHT_BYTE		0X07

#define R_SINGLE_BYTE		0X00
#define R_TWO_BYTE		0X01
#define R_THREE_BYTE		0X02
#define R_FOUR_BYTE		0X03
#define R_FIVE_BYTE		0X04
#define R_SIX_BYTE		0X05
#define R_SEVEN_BYTE		0X06
#define R_EIGHT_BYTE		0X07

#define EPL_SENSING_1_TIME	(0 << 5)
#define EPL_SENSING_2_TIME	(1 << 5)
#define EPL_SENSING_4_TIME	(2 << 5)
#define EPL_SENSING_8_TIME	(3 << 5)
#define EPL_SENSING_16_TIME	(4 << 5)
#define EPL_SENSING_32_TIME	(5 << 5)
#define EPL_SENSING_64_TIME	(6 << 5)
#define EPL_SENSING_128_TIME	(7 << 5)
#define EPL_C_SENSING_MODE	(0 << 4)
#define EPL_S_SENSING_MODE	(1 << 4)
#define EPL_ALS_MODE		(0 << 2)
#define EPL_PS_MODE		(1 << 2)
#define EPL_TEMP_MODE		(2 << 2)
#define EPL_H_GAIN		(0)
#define EPL_M_GAIN		(1)
#define EPL_L_GAIN		(3)
#define EPL_AUTO_GAIN		(2)

#define EPL_8BIT_ADC		0
#define EPL_10BIT_ADC		1
#define EPL_12BIT_ADC		2
#define EPL_14BIT_ADC		3

#define EPL_C_RESET				0x00
#define EPL_C_START_RUN		0x04
#define EPL_C_P_UP				0x04
#define EPL_C_P_DOWN			0x06
#define EPL_DATA_LOCK_ONLY		0x01
#define EPL_DATA_LOCK			0x05
#define EPL_DATA_UNLOCK		0x04

#define EPL_GO_MID				0x1E
#define EPL_GO_LOW				0x1E

#define EPL_INT_BINARY			0
#define EPL_INT_DISABLE			2
#define EPL_INT_ACTIVE_LOW		3
#define EPL_INT_FRAME_ENABLE		4

#define EPL_PST_1_TIME		(0 << 2)
#define EPL_PST_4_TIME		(1 << 2)
#define EPL_PST_8_TIME		(2 << 2)
#define EPL_PST_16_TIME		(3 << 2)

struct elan_epl_platform_data {
	int intr_gpio;
	int (*power) (int on);
};

#endif
