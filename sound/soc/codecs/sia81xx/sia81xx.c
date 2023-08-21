/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#define DEBUG
#define LOG_FLAG	"sia81xx_driver"


#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/gameport.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
//#include <linux/wakelock.h>
#include <linux/cdev.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/cpufreq.h>
#include <linux/syscalls.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/version.h>


#include "sia81xx_common.h"
#include "sia81xx_regmap.h"
#include "sia81xx_timer_task.h"
#include "sia81xx_tuning_if.h"
#include "sia81xx_set_vdd.h"

#ifdef SIA81XX_TUNING
#include "sia81xx_socket.h"
#endif


#define SIA81XX_NAME					"sia81xx"
#define SIA81XX_I2C_NAME				SIA81XX_NAME

#define DISTINGUISH_CHIP_TYPE
//#define ALGO_SWITCH_EN

#define SIA81XX_CMD_POWER_ON			(1)
#define SIA81XX_CMD_POWER_OFF			(2)
#define SIA81XX_CMD_GET_MODE			(3)
#define SIA81XX_CMD_SET_MODE			(4)
#define SIA81XX_CMD_GET_REG				(5)
#define SIA81XX_CMD_SET_REG				(6)

#define SIA81XX_CMD_WRITE_BYTE			(100)
#define SIA81XX_CMD_SET_OWI_DELAY		(101)
#define SIA81XX_CMD_CLR_ERROR			(200)
#define SIA81XX_CMD_SOCKET_OPEN			(400)
#define SIA81XX_CMD_SOCKET_CLOSE		(401)
#define SIA81XX_CMD_VDD_SET_OPEN		(402)
#define SIA81XX_CMD_VDD_SET_CLOSE		(403)
#define SIA81XX_CMD_TIMER_TASK_OPEN		(404)
#define SIA81XX_CMD_TIMER_TASK_CLOSE	(405)
#ifdef ALGO_SWITCH_EN
#define SIA81XX_CMD_ALGORITHM_OPEN		(406)
#define SIA81XX_CMD_ALGORITHM_CLOSE		(407)
#define SIA81XX_CMD_ALGORITHM_STATUS	(408)
#endif

#define SIA81XX_CMD_GET_RST_PIN			(600)
#define SIA81XX_CMD_SET_RST_PIN			(601)

#define SIA81XX_CMD_TEST				(777)


#define SIA81XX_ENABLE_LEVEL			(1)
#define SIA81XX_DISABLE_LEVEL			(0)

/* 10us > pulse width > 0.75us */
#define MIN_OWI_PULSE_GAP_TIME_US		(1)
#define MAX_OWI_PULSE_GAP_TIME_US		(160)
#define MAX_OWI_RETRY_TIMES				(10)
#define MIN_OWI_MODE					(1)
#define MAX_OWI_MODE					(16)
#define DEFAULT_OWI_MODE				(6)
/* OWI_POLARITY 0 : pulse level == high, 1 : pulse level == low */
#define OWI_POLARITY					SIA81XX_DISABLE_LEVEL


//#define OWI_SUPPORT_WRITE_DATA
#ifdef OWI_SUPPORT_WRITE_DATA
#define OWI_DATA_BIG_END
#endif

/* error list */
#define EPTOUT	(100) /* pulse width time out */
#define EPOLAR	(101) /* pulse electrical level opposite with the polarity */

struct sia81xx_err {
	unsigned long owi_set_mode_cnt;
	unsigned long owi_set_mode_err_cnt;
	unsigned long owi_write_err_cnt;
	unsigned long owi_polarity_err_cnt;
	unsigned long owi_max_retry_cnt;
	unsigned long owi_max_gap;
	unsigned long owi_max_deviation;
	unsigned long owi_write_data_err_cnt;
	unsigned long owi_write_data_cnt;
};

typedef struct sia81xx_dev_s {
	char name[32];
	unsigned int chip_type;
	struct platform_device *pdev;
	struct i2c_client *client;

	int disable_pin;
	int rst_pin;
	int owi_pin;

	unsigned int owi_delay_us;
	unsigned int owi_cur_mode;
	unsigned int owi_polarity;

	spinlock_t rst_lock;
	spinlock_t owi_lock;

	struct regmap *regmap;
	unsigned int scene;

	uint32_t channel_num;
	unsigned int en_xfilter;
	unsigned int en_dyn_ud_vdd;
	unsigned int en_dyn_ud_pvdd;
	unsigned int dyn_ud_vdd_port;
	uint32_t timer_task_hdl;
	uint32_t timer_task_hdl_backup;

	struct list_head list;

	struct sia81xx_err err_info;
}sia81xx_dev_t;

struct sia81xx_chip_compat {
	const uint32_t sub_type;
	struct {
		const uint32_t *chips;
		const uint32_t num;
	};
};

static ssize_t sia81xx_cmd_show(struct device* cd,
	struct device_attribute *attr, char* buf);
static ssize_t sia81xx_cmd_store(struct device* cd, 
	struct device_attribute *attr,const char* buf, size_t len);

#ifdef DISTINGUISH_CHIP_TYPE
static ssize_t sia81xx_device_show(struct device* cd,
	struct device_attribute *attr, char* buf);
static ssize_t sia81xx_device_store(struct device* cd, 
	struct device_attribute *attr,const char* buf, size_t len);
#endif

static DEVICE_ATTR_RW(sia81xx_cmd);
#ifdef DISTINGUISH_CHIP_TYPE
static DEVICE_ATTR_RW(sia81xx_device);
#endif

static DEFINE_MUTEX(sia81xx_list_mutex);
static LIST_HEAD(sia81xx_list);


static const char *support_chip_type_name_table[] = {
	[CHIP_TYPE_SIA8101] = "sia8101",
	[CHIP_TYPE_SIA8108] = "sia8108",
	[CHIP_TYPE_SIA8109] = "sia8109",
	[CHIP_TYPE_SIA8152] = "sia8152",
	[CHIP_TYPE_SIA8152S] = "sia8152s",
	[CHIP_TYPE_SIA8100] = "sia8100",
	[CHIP_TYPE_SIA8159] = "sia8159",
	[CHIP_TYPE_SIA81X9] = "sia81x9",
	[CHIP_TYPE_SIA8152X] = "sia8152x"
};

static sia81xx_dev_t *g_default_sia_dev = NULL;

static int sia81xx_resume(
	struct sia81xx_dev_s *sia81xx);
static int sia81xx_suspend(
	struct sia81xx_dev_s *sia81xx);
/********************************************************************
 * sia81xx misc option
 ********************************************************************/
static void clear_sia81xx_err_info(
	sia81xx_dev_t *sia81xx)
{
	if(NULL == sia81xx)
		return ;

	memset(&sia81xx->err_info, 0, sizeof(sia81xx->err_info));
}
/********************************************************************
 * end - sia81xx misc option
 ********************************************************************/

/********************************************************************
 * sia81xx GPIO option
 ********************************************************************/
static __inline void gpio_flipping(
	int pin, 
	s64 *intervel_ns)
{
	static struct timespec64 cur_time, last_time, temp_time;
	
	gpio_set_value(pin, !gpio_get_value(pin));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
	ktime_get_real_ts64(&cur_time);
#else
	getnstimeofday64(&cur_time);
#endif
	temp_time = timespec64_sub(cur_time, last_time);
	if(NULL != intervel_ns)
		*intervel_ns = timespec64_to_ns(&temp_time);
	last_time = cur_time;
}

static __inline int gpio_produce_one_pulse(
	int pin, 
	int polarity, 
	unsigned int width_us, 
	s64 *real_duty_ns, 
	s64 *real_idle_ns)
{
	if(polarity != gpio_get_value(pin)) {
		pr_err("[  err][%s] %s: EPOLAR \r\n", LOG_FLAG, __func__);
		return -EPOLAR;
	}
	
	gpio_flipping(pin, real_idle_ns);
	udelay(width_us);	
	gpio_flipping(pin, real_duty_ns);

	return 0;
}

static __inline int __gpio_produce_one_pulse_cycle(
	int pin, 
	int polarity, 
	unsigned int duty_time_us, 
	unsigned int idle_time_us, 
	s64 *real_duty_ns, 
	s64 *real_idle_ns)
{
	int ret = gpio_produce_one_pulse(pin, polarity, 
		duty_time_us, real_duty_ns, real_idle_ns);
	if(0 == ret) /* if pulse produce suceess */
		udelay(idle_time_us);

	return ret;
}

static __inline int gpio_produce_one_pulse_cycle(
	int pin, 
	int polarity, 
	unsigned int delay_us, 
	s64 *real_duty_ns, 
	s64 *real_idle_ns)
{
	return __gpio_produce_one_pulse_cycle(pin, polarity, 
		delay_us, delay_us, real_duty_ns, real_idle_ns);
}

static __inline int gpio_produce_pulse_cycles(
	int pin, 
	int polarity, 
	unsigned int delay_us, 
	unsigned int cycles, 
	s64 *max_real_delay_ns) {

	s64 real_duty_ns = 0, real_idle_ns = 0, max_pulse_time = 0;
	bool is_fst_pulse = true;
	int ret = 0;

	if(0 == pin) {
		pr_err("[  err][%s] %s: 0 == pin \r\n", LOG_FLAG, __func__);
		return -EINVAL;
	}

	while(cycles) {
		cycles --;

		ret = gpio_produce_one_pulse_cycle(pin, polarity, 
			delay_us, &real_duty_ns, &real_idle_ns);
		if(0 != ret)
			break;

		/* monitor pulse with overtime */
		if(false == is_fst_pulse) {
			max_pulse_time = 
				max_pulse_time > real_idle_ns ? max_pulse_time : real_idle_ns;
		}
		max_pulse_time = 
			max_pulse_time > real_duty_ns ? max_pulse_time : real_duty_ns;
		is_fst_pulse = false;
		
	}

	if(NULL != max_real_delay_ns)
		*max_real_delay_ns = max_pulse_time;

	return ret;
}
/********************************************************************
 * end - sia81xx GPIO option
 ********************************************************************/


/********************************************************************
 * sia81xx owi option
 ********************************************************************/
#ifdef OWI_SUPPORT_WRITE_DATA
static void sia81xx_owi_calc_bit_duty_ns(
	s64 duty_ns, 
	s64 idle_ns, 
	s64 *duty_time_ns, 
	s64 *idle_time_ns, 
	const char bit)
{
	if(0 == !!(bit & (char)0x01)) {
		*duty_time_ns = idle_ns;
		*idle_time_ns = duty_ns;
	} else {
		*duty_time_ns = duty_ns;
		*idle_time_ns = idle_ns;
	}
}

static void sia81xx_owi_calc_bit_duty_us(
	u32 duty_us, 
	u32 idle_us, 
	u32 *duty_time_us, 
	u32 *idle_time_us, 
	const char bit)
{
	s64 duty_time_ns, idle_time_ns;
	
	sia81xx_owi_calc_bit_duty_ns((s64)duty_us * NSEC_PER_USEC, 
		(s64)idle_us * NSEC_PER_USEC, &duty_time_ns, &idle_time_ns, bit);

	*duty_time_us = duty_time_ns / NSEC_PER_USEC;
	*idle_time_us = idle_time_ns / NSEC_PER_USEC;
}

static int sia81xx_owi_write_one_bit(
	struct sia81xx_dev_s *sia81xx, 
	const char bit, 
	s64 *real_duty_ns, 
	s64 *real_idle_ns)
{
	int ret = 0;
	unsigned int duty_time_us, idle_time_us;

	sia81xx_owi_calc_bit_duty_us(sia81xx->owi_delay_us, 
		MIN_OWI_PULSE_GAP_TIME_US, &duty_time_us, &idle_time_us, bit);

	ret = __gpio_produce_one_pulse_cycle(sia81xx->owi_pin, 
		sia81xx->owi_polarity, duty_time_us, idle_time_us, 
		real_duty_ns, real_idle_ns);
	
	return ret;
}

static int sia81xx_owi_start_write_byte(
	struct sia81xx_dev_s *sia81xx)
{
	/* before start write, gpio level must be opposite with OWI_POLARITY */
	if(gpio_get_value(sia81xx->owi_pin) == sia81xx->owi_polarity) {
		pr_err("[  err][%s] %s: EPOLAR !!! \r\n", LOG_FLAG, __func__);
		return -EPOLAR;
	}

	/* *******************************************************************
	 * for reset flipping timer, 
	 * otherwise only need gpio_set_value(dev->owi_pin, OWI_POLARITY); 
	 * *******************************************************************/
	gpio_flipping(sia81xx->owi_pin, NULL);

	return 0;
}

static void sia81xx_owi_end_write_byte(
	struct sia81xx_dev_s *sia81xx, 
	s64 *real_idle_ns)
{
	gpio_flipping(sia81xx->owi_pin, real_idle_ns);
}

static s64 sia81xx_owi_calc_deviation_ns(
	struct sia81xx_dev_s *dev, 
	s64 duty_time_ns, 
	s64 idle_time_ns, 
	const char bit)
{
	s64 deviation = 0;

	dev->err_info.owi_max_gap = 
		dev->err_info.owi_max_gap > duty_time_ns ?
		dev->err_info.owi_max_gap : duty_time_ns;

	dev->err_info.owi_max_gap = 
		dev->err_info.owi_max_gap > idle_time_ns ?
		dev->err_info.owi_max_gap : idle_time_ns;
	
	sia81xx_owi_calc_bit_duty_ns(duty_time_ns, idle_time_ns, 
		&duty_time_ns, &idle_time_ns, bit);

	if(duty_time_ns < idle_time_ns)
		deviation = idle_time_ns - duty_time_ns;

	return deviation;
}

static int sia81xx_owi_write_one_byte(
	struct sia81xx_dev_s *sia81xx, 
	const char data)
{
	int ret = 0, i = 0;
	s64 real_duty_ns = 0, real_idle_ns = 0, last_real_duty_ns = 0;
	s64 max_deviation_ns = 0, cur_deviation_ns = 0;
	char last_bit = 0;
	unsigned long flags;

	spin_lock_irqsave(&sia81xx->owi_lock, flags);

	ret = sia81xx_owi_start_write_byte(sia81xx);
	if(0 != ret)
		goto owi_end_write_one_byte;

#ifdef OWI_DATA_BIG_END
	for(i = 7; i >= 0; i--) {
		ret = sia81xx_owi_write_one_bit(sia81xx, (data >> i), 
			&real_duty_ns, &real_idle_ns);
		if(0 != ret)
			goto owi_end_write_one_byte;

		/* when second bit has been sent, 
		 * can be get first bit's real idle time */
		if(7 > i) {
			cur_deviation_ns = sia81xx_owi_calc_deviation_ns(sia81xx, 
				last_real_duty_ns, real_idle_ns, last_bit);
			max_deviation_ns = max_deviation_ns > cur_deviation_ns ? 
				max_deviation_ns : cur_deviation_ns;

			/* occur write error */
			if(0 != cur_deviation_ns)
				break;
		}
		last_bit = (data >> i);
		last_real_duty_ns = real_duty_ns;
	}
#else
	for(i = 0; i < 8; i++) {
		ret = sia81xx_owi_write_one_bit(sia81xx, (data >> i), 
			&real_duty_ns, &real_idle_ns);
		if(0 != ret)
			goto owi_end_write_one_byte;

		/* when second bit has been sent, 
		 * can be get first bit's real idle time */
		if(0 < i) {
			cur_deviation_ns = sia81xx_owi_calc_deviation_ns(sia81xx, 
				last_real_duty_ns, real_idle_ns, last_bit);
			max_deviation_ns = max_deviation_ns > cur_deviation_ns ? 
				max_deviation_ns : cur_deviation_ns;

			/* occur write error */
			if(0 != cur_deviation_ns)
				break;
		}
		last_bit = (data >> i);
		last_real_duty_ns = real_duty_ns;
	}
#endif

	/* end write byte, and get last bit real idle time */
	sia81xx_owi_end_write_byte(sia81xx, &real_idle_ns);

	/* no write error */
	if(0 == cur_deviation_ns) {
		/* then can be get the last bit's real idle time */
		cur_deviation_ns = sia81xx_owi_calc_deviation_ns(sia81xx, 
			last_real_duty_ns, real_idle_ns, last_bit);
		max_deviation_ns = max_deviation_ns > cur_deviation_ns ? 
			max_deviation_ns : cur_deviation_ns;
	}

owi_end_write_one_byte:	

	spin_unlock_irqrestore(&sia81xx->owi_lock, flags);

	if(0 != cur_deviation_ns)
		ret = -EPTOUT;

	if(0 != ret) {
		if(-EPOLAR == ret)
			/* record polarity err */
			sia81xx->err_info.owi_polarity_err_cnt++;
		else
			/* record wirte err */
			sia81xx->err_info.owi_write_err_cnt ++;
	}

	/* record history max deviation time */
	sia81xx->err_info.owi_max_deviation = 
		sia81xx->err_info.owi_max_deviation > max_deviation_ns ? 
		sia81xx->err_info.owi_max_deviation : max_deviation_ns;
	
	return ret;
}

static int sia81xx_owi_write_data(
	struct sia81xx_dev_s *sia81xx, 
	unsigned int len, 
	const char *buf)
{
	int ret = 0, i = 0, retry = 0;

	sia81xx->err_info.owi_write_data_cnt ++;

	for(i = 0; i < len; i++) {
		
		retry = 0;
		
		while (retry < MAX_OWI_RETRY_TIMES) {
			
			if(0 == (ret = sia81xx_owi_write_one_byte(sia81xx, buf[i])))
				break;

			/* must be ust msleep, do not use mdelay */
			msleep(1);

			retry ++;
		}

		/* record max retry time */
		sia81xx->err_info.owi_max_retry_cnt = 
			sia81xx->err_info.owi_max_retry_cnt > retry ? 
			sia81xx->err_info.owi_max_retry_cnt : retry;

		if(retry >= MAX_OWI_RETRY_TIMES) {
			sia81xx->err_info.owi_write_data_err_cnt ++;
			return -EPTOUT;
		}
	}
	
	return ret;
}
#endif

static void sia81xx_set_owi_polarity(
	struct sia81xx_dev_s *sia81xx)
{
	gpio_set_value(sia81xx->owi_pin, sia81xx->owi_polarity);
}

static int __sia81xx_owi_write_mode(
	struct sia81xx_dev_s *sia81xx, 
	unsigned int cycles)
{	
	int ret = 0;
	s64 max_flipping_ns = 0, last_flipping_ns = 0;
	unsigned long flags;

	spin_lock_irqsave(&sia81xx->owi_lock, flags);
	
	sia81xx_set_owi_polarity(sia81xx);
	udelay(1500);	/* wait for owi reset, longer than 1ms */
	
	/* last pulse only flipping once, so pulses = cycles - 1 */
	ret = gpio_produce_pulse_cycles(sia81xx->owi_pin, sia81xx->owi_polarity, 
			sia81xx->owi_delay_us, cycles - 1, &max_flipping_ns);
	/* last pulse */
	gpio_flipping(sia81xx->owi_pin, &last_flipping_ns);

	spin_unlock_irqrestore(&sia81xx->owi_lock, flags);

	if(0 != ret) {
		if(-EPOLAR == ret)
			/* record polarity err */
			sia81xx->err_info.owi_polarity_err_cnt++;
		else
			/* record wirte err */
			sia81xx->err_info.owi_write_err_cnt ++;
		return ret;
	}
	
	/* if cycles == 1, then only flipping gpio, and do not care flipping time */
	if(0 == (cycles - 1))
		return 0;

	/* get max flipping time */
	max_flipping_ns = max_flipping_ns > last_flipping_ns ? 
		max_flipping_ns : last_flipping_ns;

	/* record history max flipping time */
	sia81xx->err_info.owi_max_gap = 
		sia81xx->err_info.owi_max_gap > max_flipping_ns ? 
		sia81xx->err_info.owi_max_gap : max_flipping_ns;

	if((MAX_OWI_PULSE_GAP_TIME_US * NSEC_PER_USEC) <= max_flipping_ns ) {
		/* record wirte err */
		sia81xx->err_info.owi_write_err_cnt ++;
		return -EPTOUT;
	}

	return 0;
}

static int sia81xx_owi_write_mode(
	struct sia81xx_dev_s *sia81xx, 
	unsigned int mode)
{
	unsigned int retry = 0;
	
	if((MAX_OWI_MODE < mode) || (MIN_OWI_MODE > mode))
		return -EINVAL;

	sia81xx->err_info.owi_set_mode_cnt ++;
	
	while(retry < MAX_OWI_RETRY_TIMES) {

		if(0 == __sia81xx_owi_write_mode(sia81xx, mode))
			break;

		/* must be use msleep, do not use mdelay */
		msleep(1);

		retry ++;
	}

	/* record max retry time */
	sia81xx->err_info.owi_max_retry_cnt = 
		sia81xx->err_info.owi_max_retry_cnt > retry ? 
		sia81xx->err_info.owi_max_retry_cnt : retry;

	if(retry >= MAX_OWI_RETRY_TIMES) {
		sia81xx->err_info.owi_set_mode_err_cnt ++;
		return -EPTOUT;
	}
	
	return 0;
}

static int sia81xx_owi_init(
	sia81xx_dev_t *sia81xx, 
	unsigned int owi_mode)
{
	sia81xx->owi_delay_us = MIN_OWI_PULSE_GAP_TIME_US;
	sia81xx->owi_polarity = OWI_POLARITY;
	if((owi_mode >= MIN_OWI_MODE) && (owi_mode <= MAX_OWI_MODE))
		sia81xx->owi_cur_mode = owi_mode;
	else
		sia81xx->owi_cur_mode = DEFAULT_OWI_MODE;

	pr_debug("[debug][%s] %s: running mode = %u \r\n", 
		LOG_FLAG, __func__, sia81xx->owi_cur_mode);
	
	return 0;	
}
/********************************************************************
* end - sia81xx owis option
********************************************************************/
 

/********************************************************************
 * sia81xx chip option
 ********************************************************************/
static bool is_chip_type_supported(unsigned int chip_type)
{
	if (chip_type >= ARRAY_SIZE(support_chip_type_name_table))
		return false;

	return true;
}

static bool sia81xx_is_chip_en(sia81xx_dev_t *sia81xx)
{
	if (0 == sia81xx->disable_pin) {
		if (SIA81XX_ENABLE_LEVEL == gpio_get_value(sia81xx->rst_pin))
			return true;
	} else {
		if (sia81xx_regmap_get_chip_en(sia81xx->regmap, sia81xx->chip_type))
			return true;
	}

	return false;
}

static int sia81xx_reg_init(
	struct sia81xx_dev_s *sia81xx) 
{
	if (NULL == sia81xx->client)
		return 0;

	if (CHIP_TYPE_SIA8101 == sia81xx->chip_type 
		&& 0 != sia81xx->channel_num) {
		sia81xx_regmap_defaults(g_default_sia_dev->regmap, 
			sia81xx->chip_type, sia81xx->scene, sia81xx->channel_num);
	} else {
		sia81xx_regmap_defaults(sia81xx->regmap, 
			sia81xx->chip_type, sia81xx->scene, sia81xx->channel_num);
	}

	udelay(100);
	if (0 != sia81xx_regmap_check_chip_id(sia81xx->regmap, sia81xx->chip_type)) {
		pr_warn("[ warn][%s] %s: sia81xx_regmap_check_chip_id failed !!! \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	/* useless in any case */
	//sia81xx_regmap_set_xfilter(sia81xx->regmap, 
	//	sia81xx->chip_type, sia81xx->en_xfilter);

	return 0;
}

static int sia81xx_resume(
	struct sia81xx_dev_s *sia81xx)
{
	unsigned long flags;
	int default_sia_is_open = 0;
	char chip_id = 0;

	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);


    sia81xx_regmap_read(sia81xx->regmap, 0x00, 1, &chip_id);
    pr_debug("[debug][%s] %s: chip_id is  %d \r\n", LOG_FLAG, __func__,chip_id);

	if (NULL == sia81xx)
		return -ENODEV;

	if (is_chip_type_supported(sia81xx->chip_type) &&
		!sia81xx_is_chip_en(sia81xx)) {

		if (0 == sia81xx->disable_pin) {
			if (CHIP_TYPE_SIA8101 == sia81xx->chip_type
				&& 0 != sia81xx->channel_num) {
				if (likely(NULL != g_default_sia_dev)) {
					default_sia_is_open = gpio_get_value(g_default_sia_dev->rst_pin);
					if (1 == default_sia_is_open)
						sia81xx_suspend(g_default_sia_dev);
				} else {
					pr_err("[  err][%s] %s: g_default_sia_dev == NULL !!! \r\n", 
						LOG_FLAG, __func__);
					goto err_sia81xx_resume;
				}
			}

			/* power up chip */
			if (0 != sia81xx_owi_write_mode(sia81xx, sia81xx->owi_cur_mode))
				goto err_sia81xx_resume;

			spin_lock_irqsave(&sia81xx->rst_lock, flags);
			gpio_set_value(sia81xx->rst_pin, SIA81XX_ENABLE_LEVEL);
			mdelay(1);	/* wait chip power up, the time must be > 1ms */
			spin_unlock_irqrestore(&sia81xx->rst_lock, flags);

			if (CHIP_TYPE_SIA8109 == sia81xx->chip_type ||
				CHIP_TYPE_SIA81X9 == sia81xx->chip_type)
				mdelay(39);	/* for sia8109 gain rising. */
		}

		sia81xx_reg_init(sia81xx);
		sia81xx_regmap_set_chip_on(sia81xx->regmap, 
			sia81xx->chip_type, sia81xx->scene, sia81xx->channel_num);
		sia81xx_regmap_check_trimming(sia81xx->regmap, sia81xx->chip_type);

		if (0 == sia81xx->disable_pin) {
			if (CHIP_TYPE_SIA8101 == sia81xx->chip_type
				&& 0 != sia81xx->channel_num) {
				if (1 == default_sia_is_open)
					sia81xx_resume(g_default_sia_dev);
			}
		}
	}

	if (sia81xx->en_dyn_ud_vdd || sia81xx->en_dyn_ud_pvdd)
		sia81xx_timer_task_start(sia81xx->timer_task_hdl);

	return 0;

err_sia81xx_resume:
	pr_err("[  err][%s] %s: error !!! \r\n", LOG_FLAG, __func__);
	return -EINVAL;
}

static int sia81xx_suspend(
	struct sia81xx_dev_s *sia81xx)
{
	unsigned long flags;

	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	if (NULL == sia81xx)
		return -ENODEV;

	if (sia81xx->en_dyn_ud_vdd || sia81xx->en_dyn_ud_pvdd)
		sia81xx_timer_task_stop(sia81xx->timer_task_hdl);

	if (is_chip_type_supported(sia81xx->chip_type) &&
		sia81xx_is_chip_en(sia81xx)) {

		sia81xx_regmap_set_chip_off(sia81xx->regmap, sia81xx->chip_type);

		if (0 == sia81xx->disable_pin) {
			spin_lock_irqsave(&sia81xx->rst_lock, flags);

			/* power off chip */
			gpio_set_value(sia81xx->rst_pin, SIA81XX_DISABLE_LEVEL);
			mdelay(1);	/* wait chip power off, the time must be > 1ms */

			spin_unlock_irqrestore(&sia81xx->rst_lock, flags);
		}
	}

	return 0;
}

static int sia81xx_reboot(
	struct sia81xx_dev_s *sia81xx)
{
	int ret = 0;

	ret = sia81xx_suspend(sia81xx);
	if(0 != ret)
		return ret;
        mdelay(5);
	return sia81xx_resume(sia81xx);
}

static int sia81xx_set_mode(
	struct sia81xx_dev_s *sia81xx,
	unsigned int mode)
{
	int ret = 0;

	if((MAX_OWI_MODE < mode) || (MIN_OWI_MODE > mode)) {
		pr_err("[  err][%s] %s: error mode = %u !!! \r\n",
			LOG_FLAG, __func__, mode);
		return -EINVAL;
	}

	sia81xx->owi_cur_mode = mode;

	ret = sia81xx_reboot(sia81xx);

	return ret;
}

static int sia81xx_dev_init(
	sia81xx_dev_t *sia81xx, 
	struct device_node	*sia81xx_of_node) 
{
	int ret = 0;
	int owi_mode = DEFAULT_OWI_MODE;
	int en_xfilter = 0;
	int en_dyn_ud_vdd = 0;
	int en_dyn_ud_pvdd = 0;
	int dyn_ud_vdd_port = 0;
	int timer_task_hdl = 0;
	int channel_num = 0;
	
	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	ret = of_property_read_u32(sia81xx_of_node, 
			"channel_num", &channel_num);
	if(0 != ret) {
		channel_num = 0;
	}

	ret = of_property_read_u32(sia81xx_of_node, "en_x_filter", &en_xfilter);
	if((0 != ret) || (1 != en_xfilter)) {
		en_xfilter = 0;
	}

	ret = of_property_read_u32(sia81xx_of_node, 
			"timer_task_hdl", &timer_task_hdl);
	if(0 != ret) {
		timer_task_hdl = SIA81XX_TIMER_TASK_INVALID_HDL;
	}

	ret = of_property_read_u32(sia81xx_of_node, 
			"en_dynamic_updata_vdd", &en_dyn_ud_vdd);
	if((0 != ret) || (1 != en_dyn_ud_vdd)) {
		en_dyn_ud_vdd = 0;
	}

	ret = of_property_read_u32(sia81xx_of_node, 
			"en_dynamic_updata_pvdd", &en_dyn_ud_pvdd);
	if((0 != ret) || (1 != en_dyn_ud_pvdd)) {
		en_dyn_ud_pvdd = 0;
	}

	ret = of_property_read_u32(sia81xx_of_node, 
			"dynamic_updata_vdd_port", &dyn_ud_vdd_port);
	if(0 != ret) {
		pr_err("[  err][%s] %s: get dynamic_updata_vdd_port "
			"form dts fail, ret = %d !!! \r\n", 
			LOG_FLAG, __func__, dyn_ud_vdd_port);
		en_dyn_ud_vdd = 0;
	}

	ret = of_property_read_u32(sia81xx_of_node, "owi_mode", &owi_mode);
	if(0 != ret || MIN_OWI_MODE > owi_mode || MAX_OWI_MODE < owi_mode) {
		pr_err("[  err][%s] %s: get owi_mode "
			"form dts fail, ret = %d, owi_mode = %d !!! \r\n", 
			LOG_FLAG, __func__, ret, owi_mode);
		owi_mode = DEFAULT_OWI_MODE;
	}

	clear_sia81xx_err_info(sia81xx);

	sia81xx->channel_num = (uint32_t)channel_num;
	sia81xx->en_xfilter = (unsigned int)en_xfilter;
	sia81xx->timer_task_hdl = (uint32_t)timer_task_hdl;
	sia81xx->en_dyn_ud_vdd = (unsigned int)en_dyn_ud_vdd;
	sia81xx->en_dyn_ud_pvdd = (unsigned int)en_dyn_ud_pvdd;
	sia81xx->dyn_ud_vdd_port = (unsigned int)dyn_ud_vdd_port;
	sia81xx->scene = AUDIO_SCENE_PLAYBACK;
	sia81xx_owi_init(sia81xx, (unsigned int)owi_mode);

	sia81xx_suspend(sia81xx);
	
	return 0;
}

static int sia81xx_dev_remove(
	sia81xx_dev_t *sia81xx)
{
	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	sia81xx->timer_task_hdl = SIA81XX_TIMER_TASK_INVALID_HDL;
	
	sia81xx_suspend(sia81xx);
	
	return 0;
}

#ifdef ALGO_SWITCH_EN
static int sia81xx_algo_en_write(
	struct sia81xx_dev_s *sia81xx, 
	int32_t enable)
{
	unsigned long cal_handle = 0;

	pr_debug("[debug][%s] %s: tuning port = %d \r\n", 
		LOG_FLAG, __func__, sia81xx->dyn_ud_vdd_port);

	if (NULL == tuning_if_opt.open || NULL == tuning_if_opt.write) {
		pr_err("[  err][%s] %s: NULL == tuning_if_opt.open/write \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	cal_handle = tuning_if_opt.open(sia81xx->dyn_ud_vdd_port);
	if (0 == cal_handle) {
		pr_err("[  err][%s] %s: NULL == cal_handle \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	return tuning_if_opt.write(cal_handle, 
		SIXTH_SIA81XX_RX_MODULE,
		SIXTH_SIA81XX_RX_ENABLE,
		sizeof(enable), (uint8_t *)&enable);
}

static int sia81xx_algo_en_read(
	struct sia81xx_dev_s *sia81xx, 
	int32_t* enable)
{
	unsigned long cal_handle = 0;

	pr_debug("[debug][%s] %s: tuning port = %d \r\n", 
		LOG_FLAG, __func__, sia81xx->dyn_ud_vdd_port);

	if (NULL == tuning_if_opt.open || NULL == tuning_if_opt.write) {
		pr_err("[  err][%s] %s: NULL == tuning_if_opt.open/write \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	cal_handle = tuning_if_opt.open(sia81xx->dyn_ud_vdd_port);
	if (0 == cal_handle) {
		pr_err("[  err][%s] %s: NULL == cal_handle \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	return tuning_if_opt.read(cal_handle, 
		SIXTH_SIA81XX_RX_MODULE,
		SIXTH_SIA81XX_RX_ENABLE,
		sizeof(*enable), (uint8_t *)enable);
}
#endif
/********************************************************************
 * end - sia81xx chip option
 ********************************************************************/

#ifdef DISTINGUISH_CHIP_TYPE
/********************************************************************
 * device attr option
 ********************************************************************/
static ssize_t sia81xx_device_show(
	struct device* cd,
	struct device_attribute *attr, 
	char* buf)
{
	return 0;
}

static ssize_t sia81xx_device_store(
	struct device* cd, 
	struct device_attribute *attr, 
	const char* buf, 
	size_t len)
{
	return 0;
}
#endif

static ssize_t sia81xx_cmd_show(
	struct device* cd,
	struct device_attribute *attr, 
	char* buf)
{
	sia81xx_dev_t *sia81xx = (sia81xx_dev_t *)dev_get_drvdata(cd);
	char tb[1024];
	char chip_id = 0;
	char vals[0x64];
	int owi_pin_val = 0;

	switch (sia81xx->chip_type) {
		case CHIP_TYPE_SIA8108 :
		case CHIP_TYPE_SIA8109 :
			sia81xx_regmap_read(
				sia81xx->regmap, 0x41, 1, &chip_id);
			break;
		case CHIP_TYPE_SIA8100 :
			pr_debug("[debug][%s] %s: delay = %u \r\n", 
					LOG_FLAG, __func__, sia81xx->owi_delay_us);
			return 0;
		default :
			sia81xx_regmap_read(
				sia81xx->regmap, 0x00, 1, &chip_id);
			break;
	}

	memset(vals, 0, sizeof(vals));
	sia81xx_regmap_read(
		sia81xx->regmap, 0x00, 0x0D + 1, vals);

	if(0 == sia81xx->disable_pin) {
		owi_pin_val = gpio_get_value(sia81xx->owi_pin);
	}

	snprintf(tb, 1024, "sia81xx_cmd_show : rst pin status : %d, chip id = 0x%02x \r\n"
		"reg val = %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, "
		"%02x, %02x, %02x, %02x\r\n"
		"rst_pin = %d, owi_pin = %d \r\n"
		"set_mode = %lu, set_mode_err = %lu, polarity_err = %lu \r\n "
		"max_retry = %lu, write_err = %lu, delay = %u, m_gap = %lu \r\n "
		"owi_max_deviation = %lu, owi_write_data_err_cnt = %lu, \r\n"
		"owi_write_data_cnt = %lu \r\n"
		"channel_num = %u, owi_mode = %u, en_x_filter = %u \r\n"
		"sia81xx_disable_pin = %d, \r\n"
		"en_dynamic_updata_vdd = %u, en_dynamic_updata_pvdd = %u, \r\n"
		"dynamic_updata_vdd_port = 0x%x \r\n", 
		owi_pin_val, chip_id,
		vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], 
		vals[7], vals[8], vals[9], vals[10], vals[11], vals[12], vals[13], 
		sia81xx->rst_pin, sia81xx->owi_pin,
		sia81xx->err_info.owi_set_mode_cnt, 
		sia81xx->err_info.owi_set_mode_err_cnt, 
		sia81xx->err_info.owi_polarity_err_cnt, 
		sia81xx->err_info.owi_max_retry_cnt, 
		sia81xx->err_info.owi_write_err_cnt, 
		sia81xx->owi_delay_us,
		sia81xx->err_info.owi_max_gap, 
		sia81xx->err_info.owi_max_deviation, 
		sia81xx->err_info.owi_write_data_err_cnt, 
		sia81xx->err_info.owi_write_data_cnt,
		sia81xx->channel_num, sia81xx->owi_cur_mode, sia81xx->en_xfilter,
		sia81xx->disable_pin, 
		sia81xx->en_dyn_ud_vdd, sia81xx->en_dyn_ud_pvdd, 
		sia81xx->dyn_ud_vdd_port);
	strcpy(buf, tb);

	return strlen(buf);
}

static ssize_t sia81xx_cmd_store(
	struct device* cd, 
	struct device_attribute *attr, 
	const char* buf, 
	size_t len)
{
	const char *split_symb = ",";
	/* in strsep will be modify "cur" value, nor cur[i] value, 
	so this point shoud not be defined with "char * const" */
	char *cur = (char *)buf;
	char *after;
	sia81xx_dev_t *sia81xx = (sia81xx_dev_t *)dev_get_drvdata(cd);
	
	switch(simple_strtoul(strsep(&cur, split_symb), &after, 10)) {
		case SIA81XX_CMD_POWER_ON : 
			sia81xx_resume(sia81xx);
			break;
		case SIA81XX_CMD_POWER_OFF :
			sia81xx_suspend(sia81xx);
			break;
		case SIA81XX_CMD_GET_MODE :
		{
			pr_debug("[debug][%s] %s: mode = %u \r\n", 
					LOG_FLAG, __func__, sia81xx->owi_cur_mode);
			break;
		}
		case SIA81XX_CMD_SET_MODE :
		{
			sia81xx_set_mode(sia81xx, 
				simple_strtoul(strsep(&cur, split_symb), &after, 10));
			break;
		}
		case SIA81XX_CMD_GET_REG :
		{
			unsigned char addr = (unsigned char)simple_strtoul(
				strsep(&cur, split_symb), &after, 16);
			char val;

			if(0 != sia81xx_regmap_read(sia81xx->regmap, addr, 1, &val)) {
				pr_debug("[debug][%s] %s: err regmap_read \r\n", 
					LOG_FLAG, __func__);
			} else {
				pr_debug("[debug][%s] %s: addr = 0x%02x, val = 0x%02x \r\n", 
					LOG_FLAG, __func__, addr, val);
			}
			break;
		}
		case SIA81XX_CMD_SET_REG :
		{
			unsigned char addr = (unsigned int)simple_strtoul(
				strsep(&cur, split_symb), &after, 16);
			char val = (char)simple_strtoul(
				strsep(&cur, split_symb), &after, 16);

			if(0 != sia81xx_regmap_write(sia81xx->regmap, addr, 1, &val)) {
				pr_err("[  err][%s] %s: regmap_write \r\n", 
					LOG_FLAG, __func__);
			}
			break;
		}
		case SIA81XX_CMD_SET_OWI_DELAY :
 		{
			unsigned int temp_us = (unsigned int)simple_strtoul(
										strsep(&cur, split_symb), &after, 10);
			if(temp_us < MAX_OWI_PULSE_GAP_TIME_US) /* only for test, pulse width must be < 1ms */
				sia81xx->owi_delay_us = temp_us;
			else
				sia81xx->owi_delay_us = MAX_OWI_PULSE_GAP_TIME_US;
			
			break;
 		}
		case SIA81XX_CMD_CLR_ERROR :
		{
			memset(&sia81xx->err_info, 0, sizeof(sia81xx->err_info));
			break;
		}
		case SIA81XX_CMD_WRITE_BYTE :
		{
#ifdef OWI_SUPPORT_WRITE_DATA
			char data = simple_strtoul(strsep(&cur, split_symb), &after, 10);
			sia81xx_owi_write_data(sia81xx, 1, &data);
#else
			pr_info("[ info][%s] %s: the option "
				"OWI_SUPPORT_WRITE_DATA unsupport!! \r\n", LOG_FLAG, __func__);
#endif
			break;
		}
		case SIA81XX_CMD_SOCKET_OPEN : 
		{
#ifdef SIA81XX_TUNING
			sia81xx_open_sock_server();
#endif
			break;
		}
		case SIA81XX_CMD_SOCKET_CLOSE: 
		{
#ifdef SIA81XX_TUNING
			sia81xx_close_sock_server();
#endif
			break;
		}
		case SIA81XX_CMD_VDD_SET_OPEN :
		{
			sia81xx->en_dyn_ud_vdd = 1;
			sia81xx_auto_set_vdd_probe(
				sia81xx->timer_task_hdl, 
				sia81xx->chip_type, 
				sia81xx->channel_num, 
				sia81xx->regmap, 
				sia81xx->dyn_ud_vdd_port, 
				SIA81XX_AUTO_VDD_EN_SET(sia81xx->en_dyn_ud_vdd) | 
	 			SIA81XX_AUTO_PVDD_EN_SET(sia81xx->en_dyn_ud_pvdd));
			pr_debug("[debug][%s] %s: set auto vdd state %u, port 0x%04x \r\n", 
					LOG_FLAG, __func__, 
					sia81xx->en_dyn_ud_vdd, 
					sia81xx->dyn_ud_vdd_port);
			break;
		}
		case SIA81XX_CMD_VDD_SET_CLOSE :
		{
			//sia81xx_disable_auto_set_vdd(sia81xx->dyn_ud_vdd_port);
			sia81xx->en_dyn_ud_vdd = 0;
			
			sia81xx_set_auto_set_vdd_work_state(
				sia81xx->timer_task_hdl, 
				sia81xx->channel_num, 
				sia81xx->en_dyn_ud_vdd);
			
			sia81xx_auto_set_vdd_remove(
				sia81xx->timer_task_hdl, 
				sia81xx->channel_num);
			//sia81xx_close_set_vdd_server(0x400c/* SLIMBUS_6_RX */);
			pr_debug("[debug][%s] %s: set auto vdd state %u, port 0x%04x \r\n", 
					LOG_FLAG, __func__, 
					sia81xx->en_dyn_ud_vdd, 
					sia81xx->dyn_ud_vdd_port);
			break;
		}
		case SIA81XX_CMD_TIMER_TASK_OPEN :
		{
			sia81xx->timer_task_hdl = sia81xx->timer_task_hdl_backup;
			break;
		}
		case SIA81XX_CMD_TIMER_TASK_CLOSE : 
		{
			sia81xx_timer_task_stop(sia81xx->timer_task_hdl);
			sia81xx->timer_task_hdl_backup = sia81xx->timer_task_hdl;
			sia81xx->timer_task_hdl = SIA81XX_TIMER_TASK_INVALID_HDL;
			break;
		}
#ifdef ALGO_SWITCH_EN
		case SIA81XX_CMD_ALGORITHM_OPEN :
		{
			sia81xx_algo_en_write(sia81xx, 1);
			break;
		}
		case SIA81XX_CMD_ALGORITHM_CLOSE : 
		{
			sia81xx_algo_en_write(sia81xx, 0);
			break;
		}
		case SIA81XX_CMD_ALGORITHM_STATUS : 
		{
			int32_t algo_en;
			if (0 != sia81xx_algo_en_read(sia81xx, &algo_en)) {
				pr_debug("[debug][%s] %s: err sia81xx_algo_en_read \r\n", 
					LOG_FLAG, __func__);
			} else {
				pr_debug("[debug][%s] %s: algo_en = %d \r\n", 
					LOG_FLAG, __func__, algo_en);
			}
			break;
		}
#endif
		case SIA81XX_CMD_GET_RST_PIN: 
		{
			if (0 == sia81xx->disable_pin) {
				unsigned int gpio_value = 0xff;
				
				gpio_value = gpio_get_value(sia81xx->rst_pin);
				pr_debug("[debug][%s] %s: get reset pin value, pin num:%u, value:%u \r\n", 
						LOG_FLAG, __func__, sia81xx->rst_pin, gpio_value);
			}
			break;
		}
		case SIA81XX_CMD_SET_RST_PIN: 
		{
			if (0 == sia81xx->disable_pin) {
				unsigned char value = (unsigned int)simple_strtoul(
					strsep(&cur, split_symb), &after, 16);
				
				/* power on/off chip */
				gpio_set_value(sia81xx->rst_pin, value);
				mdelay(1);	/* wait chip power off, the time must be > 1ms */	
				
				pr_debug("[debug][%s] %s: set reset pin %u to %u \r\n", 
						LOG_FLAG, __func__, sia81xx->rst_pin, value);	
			}
			break;
		}
		case SIA81XX_CMD_TEST :
		{
			break;
		}
		default :
			return -ENOIOCTLCMD;
	}
	
	return len;
}
/********************************************************************
 * end - device attr option
 ********************************************************************/


/********************************************************************
 * sia81xx codec driver
 ********************************************************************/
static int sia81xx_power_get(
	struct snd_kcontrol *kcontrol, 
	struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,16,28))
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	sia81xx_dev_t *sia81xx = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	sia81xx_dev_t *sia81xx = snd_soc_codec_get_drvdata(codec);
#endif	

	ucontrol->value.integer.value[0] = 
		(unsigned long)sia81xx_is_chip_en(sia81xx);

	pr_debug("[debug][%s] %s: ucontrol = %ld, channel_num = %d \r\n", 
		LOG_FLAG, __func__, ucontrol->value.integer.value[0], sia81xx->channel_num);
	
	return 0;
}

static int sia81xx_power_set(
	struct snd_kcontrol *kcontrol, 
	struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,16,28))
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
    sia81xx_dev_t *sia81xx = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	sia81xx_dev_t *sia81xx = snd_soc_codec_get_drvdata(codec);
#endif	

	pr_debug("[debug][%s] %s: ucontrol = %ld, rst = %d, channel_num = %d \r\n", 
		LOG_FLAG, __func__, ucontrol->value.integer.value[0], 
		sia81xx->rst_pin, sia81xx->channel_num);

	if(1 == ucontrol->value.integer.value[0]) {
		sia81xx_resume(sia81xx);
	} else {
		sia81xx_suspend(sia81xx);
	}

	return 0;
}

static int sia81xx_audio_scene_get(
	struct snd_kcontrol *kcontrol, 
	struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,16,28))
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	sia81xx_dev_t *sia81xx = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	sia81xx_dev_t *sia81xx = snd_soc_codec_get_drvdata(codec);
#endif	

	ucontrol->value.integer.value[0] = sia81xx->scene;

	pr_debug("[debug][%s] %s: ucontrol = %ld, channle = %d \r\n", 
		LOG_FLAG, __func__, ucontrol->value.integer.value[0], sia81xx->channel_num);
	
	return 0;
}

static int sia81xx_audio_scene_set(
	struct snd_kcontrol *kcontrol, 
	struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,16,28))
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	sia81xx_dev_t *sia81xx = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	sia81xx_dev_t *sia81xx = snd_soc_codec_get_drvdata(codec);
#endif	

	pr_debug("[debug][%s] %s: ucontrol = %ld, rst = %d, channle = %d \r\n", 
		LOG_FLAG, __func__, ucontrol->value.integer.value[0], 
		sia81xx->rst_pin, sia81xx->channel_num);

	if(AUDIO_SCENE_NUM <= ucontrol->value.integer.value[0]) {
		sia81xx->scene = AUDIO_SCENE_PLAYBACK;
		pr_err("[  err][%s] %s: set audio scene val = %ld !!! \r\n",
			LOG_FLAG, __func__, ucontrol->value.integer.value[0]);
	} else {
		sia81xx->scene = ucontrol->value.integer.value[0];
	}

	if (sia81xx_is_chip_en(sia81xx))
		sia81xx_reboot(sia81xx);
	
	return 0;
}

#ifdef ALGO_SWITCH_EN
static int sia81xx_algo_en_get(
	struct snd_kcontrol *kcontrol, 
	struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,16,28))
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	sia81xx_dev_t *sia81xx = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	sia81xx_dev_t *sia81xx = snd_soc_codec_get_drvdata(codec);
#endif
	int32_t algo_en = 0;

	pr_debug("[debug][%s] %s: ucontrol = %ld \r\n", 
		LOG_FLAG, __func__, ucontrol->value.integer.value[0]);

	if (sia81xx_is_chip_en(sia81xx))
		sia81xx_algo_en_read(sia81xx, &algo_en);

	ucontrol->value.integer.value[0] = algo_en;
	return 0;
}

static int sia81xx_algo_en_set(
	struct snd_kcontrol *kcontrol, 
	struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,16,28))
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	sia81xx_dev_t *sia81xx = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	sia81xx_dev_t *sia81xx = snd_soc_codec_get_drvdata(codec);
#endif

	pr_debug("[debug][%s] %s: ucontrol = %ld\r\n", 
		LOG_FLAG, __func__, ucontrol->value.integer.value[0]);

	if (sia81xx_is_chip_en(sia81xx))
		if (0 >= sia81xx_algo_en_write(sia81xx, 
				ucontrol->value.integer.value[0]))
			return -EINVAL;
	
	return 0;
}
#endif

static const char *const power_function[] = { "Off", "On" };
#ifdef ALGO_SWITCH_EN
static const char *const algo_enable[] = { "Off", "On" };
#endif
static const char *const audio_scene[] = { "Playback", "Voice", "Receiver", "Factory" };

static const struct soc_enum power_enum = 
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(power_function), power_function);
#ifdef ALGO_SWITCH_EN
static const struct soc_enum algo_enum = 
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(algo_enable), algo_enable);
#endif
static const struct soc_enum audio_scene_enum = 
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(audio_scene), audio_scene);

static const struct snd_kcontrol_new sia81xx_controls[] = {
	SOC_ENUM_EXT("Sia81xx Power", power_enum, 
			sia81xx_power_get, sia81xx_power_set), 
#ifdef ALGO_SWITCH_EN
	SOC_ENUM_EXT("Sia81xx Algo", algo_enum, 
			sia81xx_algo_en_get, sia81xx_algo_en_set), 
#endif
	SOC_ENUM_EXT("Sia81xx Audio Scene", audio_scene_enum, 
			sia81xx_audio_scene_get, sia81xx_audio_scene_set)
};

static int sia81xx_spkr_pa_event(
 	struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, 
	int event)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,16,28))
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	sia81xx_dev_t *sia81xx = snd_soc_component_get_drvdata(component);
#else	
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	sia81xx_dev_t *sia81xx = snd_soc_codec_get_drvdata(codec);
#endif

	pr_debug("[debug][%s] %s: msg = %d \r\n", LOG_FLAG, __func__, event);

	switch (event) {
		case SND_SOC_DAPM_POST_PMU : 
			sia81xx_resume(sia81xx);
			break;
		case SND_SOC_DAPM_PRE_PMD :
			sia81xx_suspend(sia81xx);
			break;
		default :
			pr_err("[  err][%s] %s: msg = %d \r\n", LOG_FLAG, __func__, event);
			break;
	}
	
	return 0;
}

static const struct snd_soc_dapm_widget sia81xx_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("IN"),
		
	SND_SOC_DAPM_OUT_DRV_E("SPKR DRV", 0, 0, 0, NULL, 0,
			sia81xx_spkr_pa_event, SND_SOC_DAPM_POST_PMU | 
			SND_SOC_DAPM_PRE_PMD),
			
	SND_SOC_DAPM_OUTPUT("SPKR")
};

/*static const struct snd_soc_dapm_route sia81xx_audio_map[] = {
	{"SPKR DRV", NULL, "IN"},
	{"SPKR", NULL, "SPKR DRV"}
};*/

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,16,28))
static int sia81xx_component_probe(
	struct snd_soc_component *component)
{
 	pr_info("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);
	return 0;
}

static void sia81xx_component_remove(
	struct snd_soc_component *component)
{
 	pr_info("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);
	return;
}
#else
static int sia81xx_codec_probe(
	struct snd_soc_codec *codec)
{
 	pr_info("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);
	return 0;
}
 
static int sia81xx_codec_remove(
	struct snd_soc_codec *codec)
{
 	pr_info("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);
	return 0;
}

static struct regmap *sia81xx_codec_get_regmap(struct device *dev)
{
	return NULL;
}
#endif

/*
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,19,0))
static struct snd_soc_component_driver soc_component_dev_sia81xx = {
	.probe = sia81xx_component_probe,
	.remove = sia81xx_component_remove,
	.controls = sia81xx_controls,
	.num_controls = ARRAY_SIZE(sia81xx_controls),
	.dapm_widgets = sia81xx_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sia81xx_dapm_widgets),
	.dapm_routes = NULL, //sia81xx_audio_map,
	.num_dapm_routes = 0,//ARRAY_SIZE(sia81xx_audio_map),
};
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0) && LINUX_VERSION_CODE < KERNEL_VERSION(4,19,0))
static struct snd_soc_codec_driver soc_codec_dev_sia81xx = {
	.probe = sia81xx_codec_probe,
	.remove = sia81xx_codec_remove,
	.get_regmap = sia81xx_codec_get_regmap,
	.component_driver = {
		.controls = sia81xx_controls,
		.num_controls = ARRAY_SIZE(sia81xx_controls),
		.dapm_widgets = sia81xx_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(sia81xx_dapm_widgets),
		.dapm_routes = NULL, //sia81xx_audio_map,
		.num_dapm_routes = 0,//ARRAY_SIZE(sia81xx_audio_map),
	},
};
#else
static struct snd_soc_codec_driver soc_codec_dev_sia81xx = {
	.probe = sia81xx_codec_probe,
	.remove = sia81xx_codec_remove,
	.get_regmap = sia81xx_codec_get_regmap,
	.controls = sia81xx_controls,
	.num_controls = ARRAY_SIZE(sia81xx_controls),
	.dapm_widgets = sia81xx_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sia81xx_dapm_widgets),
	.dapm_routes = NULL, //sia81xx_audio_map,
	.num_dapm_routes = 0,//ARRAY_SIZE(sia81xx_audio_map),
};
#endif
*/
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,16,28))
static struct snd_soc_component_driver soc_component_dev_sia81xx = {
	.probe = sia81xx_component_probe,
	.remove = sia81xx_component_remove,
	.controls = sia81xx_controls,
	.num_controls = ARRAY_SIZE(sia81xx_controls),
	.dapm_widgets = sia81xx_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sia81xx_dapm_widgets),
	.dapm_routes = NULL, //sia81xx_audio_map,
	.num_dapm_routes = 0,//ARRAY_SIZE(sia81xx_audio_map),
};
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(4,8,17) && LINUX_VERSION_CODE <= KERNEL_VERSION(4,16,28))
static struct snd_soc_codec_driver soc_codec_dev_sia81xx = {
	.probe = sia81xx_codec_probe,
	.remove = sia81xx_codec_remove,
	.get_regmap = sia81xx_codec_get_regmap,
	.component_driver = {
		.controls = sia81xx_controls,
		.num_controls = ARRAY_SIZE(sia81xx_controls),
		.dapm_widgets = sia81xx_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(sia81xx_dapm_widgets),
		.dapm_routes = NULL, //sia81xx_audio_map,
		.num_dapm_routes = 0,//ARRAY_SIZE(sia81xx_audio_map),
	},
};
#else
static struct snd_soc_codec_driver soc_codec_dev_sia81xx = {
	.probe = sia81xx_codec_probe,
	.remove = sia81xx_codec_remove,
	.get_regmap = sia81xx_codec_get_regmap,
	.controls = sia81xx_controls,
	.num_controls = ARRAY_SIZE(sia81xx_controls),
	.dapm_widgets = sia81xx_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sia81xx_dapm_widgets),
	.dapm_routes = NULL, //sia81xx_audio_map,
	.num_dapm_routes = 0,//ARRAY_SIZE(sia81xx_audio_map),
};
#endif

/********************************************************************
 * end - sia81xx codec driver
 ********************************************************************/




/********************************************************************
 * sia81xx driver common
 ********************************************************************/
static unsigned int sia81xx_list_count(void)
{
	unsigned count = 0;
	sia81xx_dev_t *sia81xx = NULL;
	
	mutex_lock(&sia81xx_list_mutex);

	list_for_each_entry(sia81xx, &sia81xx_list, list) {
		count ++;
	}

	mutex_unlock(&sia81xx_list_mutex);

	return count;
}

static sia81xx_dev_t *find_sia81xx_dev(
	const char *name, 
	struct device_node *of_node)
{
	sia81xx_dev_t *sia81xx = NULL, *find = NULL;

	if((NULL == name) && (NULL == of_node)) {
		pr_err("[  err][%s] %s: NULL == input parameter \r\n", 
			LOG_FLAG, __func__);
		return NULL;
	}

	mutex_lock(&sia81xx_list_mutex);

	list_for_each_entry(sia81xx, &sia81xx_list, list) {
		if(NULL != name) {
			if(0 == strcmp(sia81xx->name, name)) {
				find = sia81xx;
				break;
			}
		} 

		/* make sure that the sia81xx platform dev had been created */
		if((NULL != of_node) && (NULL != sia81xx->pdev)) {
			if(of_node == sia81xx->pdev->dev.of_node) {
				find = sia81xx;
				break;
			}
		}
	}

	mutex_unlock(&sia81xx_list_mutex);

	return find;
}

static void add_sia81xx_dev(
	sia81xx_dev_t *sia81xx)
{
	struct device_node *of_node = NULL;

	if(NULL == sia81xx) {
		pr_err("[  err][%s] %s: NULL == sia81xx \r\n", LOG_FLAG, __func__);
		return ;
	}

	if(NULL != sia81xx->pdev)
		of_node = sia81xx->pdev->dev.of_node;

	if(NULL != find_sia81xx_dev(sia81xx->name, of_node))
		return ;

	mutex_lock(&sia81xx_list_mutex);
	list_add(&sia81xx->list, &sia81xx_list);
	mutex_unlock(&sia81xx_list_mutex);

	pr_debug("[debug][%s] %s: add sia81xx dev : %s, count = %u \r\n", 
		LOG_FLAG, __func__, sia81xx->name, sia81xx_list_count());
}

static void del_sia81xx_dev(
	sia81xx_dev_t *sia81xx)
{
	if(NULL == sia81xx) {
		pr_err("[  err][%s] %s: NULL == sia81xx \r\n", LOG_FLAG, __func__);
		return ;
	}

	pr_debug("[debug][%s] %s: del sia81xx dev : %s, count = %u \r\n", 
		LOG_FLAG, __func__, sia81xx->name, sia81xx_list_count());

	mutex_lock(&sia81xx_list_mutex);
	list_del(&sia81xx->list);
	mutex_unlock(&sia81xx_list_mutex);
}

static sia81xx_dev_t *get_sia81xx_dev(
	struct device *dev, 
	struct device_node	*sia81xx_of_node)
{
	int ret = 0;
	sia81xx_dev_t *sia81xx = NULL;
	const char *sia81xx_dev_name = NULL;
	
	if(NULL == dev) {
		pr_err("[  err][%s] %s: NULL == dev \r\n", LOG_FLAG, __func__);
		return NULL;
	}

	/* check dev has been created or not by "si,sia81xx-dev-name" or of_node */
	ret = of_property_read_string_index(dev->of_node,
						    "si,sia81xx-dev-name",
						    0,
						    &sia81xx_dev_name);
	if(0 != ret) {
		/* should been had one of "name" or "of_node" at least */
		if(NULL == sia81xx_of_node)
			return NULL;
		
		sia81xx = find_sia81xx_dev(NULL, sia81xx_of_node);
		if(NULL == sia81xx) {
			/* don't use devm_kzalloc() */
			sia81xx = kzalloc(sizeof(sia81xx_dev_t), GFP_KERNEL);
			if(NULL == sia81xx) {
				pr_err("[  err][%s] %s: dev[%s] cannot create "
					"memory for sia81xx\n",
					LOG_FLAG, __func__, dev->init_name);
				return NULL;
			}

			/* default name */
			snprintf(sia81xx->name, strlen("sia81xx-dummy.%u"), 
				"sia81xx-dummy.%u", sia81xx_list_count());
		}
	} else {
		sia81xx = find_sia81xx_dev(sia81xx_dev_name, sia81xx_of_node);
		if(NULL == sia81xx) {
			/* don't use devm_kzalloc() */
			sia81xx = kzalloc(sizeof(sia81xx_dev_t), GFP_KERNEL);
			if(NULL == sia81xx) {
				pr_err("[  err][%s] %s: dev[%s] cannot create "
					"memory for sia81xx\n",
					LOG_FLAG, __func__, dev->init_name);
				return NULL;
			}

			strcpy(sia81xx->name, sia81xx_dev_name);
		}
	}

	add_sia81xx_dev(sia81xx);

	pr_debug("[debug][%s] %s: get dev name : %s \r\n", 
		LOG_FLAG, __func__, sia81xx->name);

	return sia81xx;
}

static void put_sia81xx_dev(sia81xx_dev_t *sia81xx)
{
	if(NULL == sia81xx) {
		pr_err("[  err][%s] %s: NULL == sia81xx \r\n", LOG_FLAG, __func__);
		return ;
	}

	pr_debug("[debug][%s] %s: put dev name : %s, pdev = %p, client = %p \r\n", 
				LOG_FLAG, __func__, 
				sia81xx->name, sia81xx->pdev, sia81xx->client);

	if((NULL != sia81xx->pdev) || (NULL != sia81xx->client))
		return ;

	del_sia81xx_dev(sia81xx);
	kfree(sia81xx);
}

static unsigned int get_chip_type(const char *name)
{
	int i = 0, len = 0;
	
	if(NULL == name)
		return CHIP_TYPE_UNKNOWN;

	pr_info("[ info][%s] %s: chip : %s \r\n", 
		LOG_FLAG, __func__, name);

	len = strlen(name);
	for (i = 0; i < ARRAY_SIZE(support_chip_type_name_table); i ++) {
		if (strlen(support_chip_type_name_table[i]) == len && 
			0 == memcmp(support_chip_type_name_table[i], name, len)) {
			return i;
		}
	}

	return CHIP_TYPE_UNKNOWN;
}

/* CHIP_TYPE_SIA81X9 */
static const uint32_t sia81x9_list[] = {
	CHIP_TYPE_SIA8109,	// first chip reg range should cover all other chips
	CHIP_TYPE_SIA8159
};
	
/* CHIP_TYPE_SIA8152X */
static const uint32_t sia8152x_list[] = {
	CHIP_TYPE_SIA8152S,	// first chip reg range should cover all other chips
	CHIP_TYPE_SIA8152
};

/* compatible chips should have same i2c address */
static const struct sia81xx_chip_compat sia81xx_compat_table[] = {
	{ 
		CHIP_TYPE_SIA81X9, 
		{sia81x9_list, ARRAY_SIZE(sia81x9_list)} 
	},
	{ 
		CHIP_TYPE_SIA8152X, 
		{sia8152x_list, ARRAY_SIZE(sia8152x_list)} 
	}
};

unsigned int get_one_available_chip_type(unsigned int chip_type)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(sia81xx_compat_table); i++) {
		if (chip_type == sia81xx_compat_table[i].sub_type) {
			if (0 < sia81xx_compat_table[i].num)
				//pr_info("[ info][%s] %s: chip_type = %u \r\n", 
				//	LOG_FLAG, __func__, sia81xx_compat_table[i].chips[0]);
				return sia81xx_compat_table[i].chips[0];
		}
	}

	return chip_type;
}

#ifdef DISTINGUISH_CHIP_TYPE
static int check_sia81xx_status(sia81xx_dev_t *sia81xx)
{
	int ret = 0;

	if (0 == sia81xx->disable_pin)
		sia81xx_resume(sia81xx);

	ret = sia81xx_regmap_check_chip_id(
			sia81xx->regmap,  sia81xx->chip_type);

	if (0 == sia81xx->disable_pin)
		sia81xx_suspend(sia81xx);

	return ret;
}
#endif

void sia81xx_compatible_chips_adapt(
	sia81xx_dev_t *sia81xx)
{
	int i = 0, j = 0;

	for (i = 0; i < ARRAY_SIZE(sia81xx_compat_table); i++) {
		if (sia81xx->chip_type == sia81xx_compat_table[i].sub_type) {

			if (0 == sia81xx->disable_pin)
				sia81xx_resume(sia81xx);

			for (j = 0; j < sia81xx_compat_table[i].num; j++) {
				if (NULL != sia81xx_compat_table[i].chips
					&& 0 == sia81xx_regmap_check_chip_id(
						sia81xx->regmap, sia81xx_compat_table[i].chips[j])) {
					sia81xx->chip_type = sia81xx_compat_table[i].chips[j];
					break;
				}
			}

			if (0 == sia81xx->disable_pin)
				sia81xx_suspend(sia81xx);

			if (j >= sia81xx_compat_table[i].num)
				sia81xx->chip_type = CHIP_TYPE_UNKNOWN;

			pr_info("[ info][%s] %s: chip_type = %u \r\n", 
				LOG_FLAG, __func__, sia81xx->chip_type);
			break;
		}
	}

#ifdef DISTINGUISH_CHIP_TYPE
	/* check sia81xx is available */
	if (CHIP_TYPE_UNKNOWN == sia81xx->chip_type || 
		0 != check_sia81xx_status(sia81xx)) {
		
		sia81xx->chip_type = CHIP_TYPE_UNKNOWN;
		sia81xx->en_dyn_ud_pvdd = 0;
		pr_info("[ info][%s] %s: there is no sia81xx device \r\n", 
			LOG_FLAG, __func__);
	} else {
		device_create_file(&sia81xx->pdev->dev, &dev_attr_sia81xx_device);
		
		pr_info("[ info][%s] %s: sia81xx device is available \r\n", 
			LOG_FLAG, __func__);
	}
#endif
}

/********************************************************************
 * end - sia81xx driver common
 ********************************************************************/


/********************************************************************
 * i2c bus driver
 ********************************************************************/
static int sia81xx_i2c_probe(
	struct i2c_client *client, 
	const struct i2c_device_id *id)
{
	sia81xx_dev_t *sia81xx = NULL;
	struct regmap *regmap = NULL;
	struct device_node	*sia81xx_of_node = NULL;
	const char *chip_type_name = NULL;
	unsigned int chip_type = CHIP_TYPE_UNKNOWN;
	int ret = 0;

	pr_info("[ info][%s] %s: i2c addr = 0x%02x \r\n", 
		LOG_FLAG, __func__, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[  err][%s] %s: i2c_check_functionality return -ENODEV \r\n", 
			LOG_FLAG, __func__);
		return -ENODEV;
	}

	sia81xx_of_node = of_parse_phandle(client->dev.of_node,
							"si,sia81xx-dev", 0);

	/* get chip type name */
	ret = of_property_read_string_index(sia81xx_of_node,
				"si,sia81xx_type",
				0,
				&chip_type_name);
	if (0 != ret) {
		pr_err("[  err][%s] %s: get si,sia81xx_type return %d !!! \r\n", 
			LOG_FLAG, __func__, ret);
		return -ENODEV;
	}

	/* get chip type value, 
	 * and check the chip type Whether or not to be supported */
	chip_type = get_chip_type(chip_type_name);
	if (CHIP_TYPE_UNKNOWN == chip_type) {
		pr_err("[  err][%s] %s: CHIP_TYPE_UNKNOWN == chip_type !!! \r\n", 
			LOG_FLAG, __func__);
		return -ENODEV;
	}

	regmap = sia81xx_regmap_init(client, 
		get_one_available_chip_type(chip_type));
	if (IS_ERR(regmap)) {
		pr_err("[  err][%s] %s: regmap_init_i2c error !!! \r\n", 
			LOG_FLAG, __func__);
		return -ENODEV;
	}

	sia81xx = get_sia81xx_dev(&client->dev, sia81xx_of_node);
	if (NULL == sia81xx) {
		pr_err("[  err][%s] %s: get_sia81xx_dev error !!! \r\n", 
			LOG_FLAG, __func__);
		return -ENODEV;
	}

	sia81xx->regmap = regmap;

	/* save i2c client */
	sia81xx->client = client;

	/* sava driver private data to the dev's driver data */
	dev_set_drvdata(&client->dev, sia81xx);

	// for sia8101 stereo
	if (CHIP_TYPE_SIA8101 == sia81xx->chip_type 
			&& 0 == sia81xx->channel_num)
		g_default_sia_dev = sia81xx;

	/* A temporary solution for customer, it should be ensure that,
	 * the sia81xx_probe() is executed before sia81xx_i2c_probe() execute */
	sia81xx_compatible_chips_adapt(sia81xx);

	/* probe other sub module */ /* update info if it's already probed */
	if (1 == sia81xx->en_dyn_ud_vdd || 1 == sia81xx->en_dyn_ud_pvdd) {
		sia81xx_auto_set_vdd_probe(
			sia81xx->timer_task_hdl, 
			sia81xx->chip_type, 
			sia81xx->channel_num, 
			sia81xx->regmap, 
			sia81xx->dyn_ud_vdd_port, 
 			SIA81XX_AUTO_VDD_EN_SET(sia81xx->en_dyn_ud_vdd) | 
 			SIA81XX_AUTO_PVDD_EN_SET(sia81xx->en_dyn_ud_pvdd));
	}
	/* end - probe other sub module */

	/* power down chip in any case when phone start up */
	sia81xx_suspend(sia81xx);

	return 0;
}

static int sia81xx_i2c_remove(
	struct i2c_client *client)
{
	int ret = 0;
	sia81xx_dev_t *sia81xx = NULL;

	pr_info("[ info][%s] %s: remove \r\n", LOG_FLAG, __func__);

	sia81xx = (sia81xx_dev_t *)dev_get_drvdata(&client->dev);
	if(NULL == sia81xx)
		return 0;

	sia81xx_regmap_remove(sia81xx->regmap);
	sia81xx->regmap = NULL;

	sia81xx->client= NULL;

	put_sia81xx_dev(sia81xx);
	
	return ret;
}

static const struct i2c_device_id si_sia81xx_i2c_id[] = {
	{ SIA81XX_I2C_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static const struct of_device_id si_sia81xx_i2c_match[] = {
	{.compatible = "si,sia81xx-i2c"},
	{.compatible = "si,sia8101-i2c"},
	{.compatible = "si,sia8108-i2c"},
	{.compatible = "si,sia8109-i2c"},
	{.compatible = "si,sia8152-i2c"},
	{.compatible = "si,sia8152s-i2c"},
	{.compatible = "si,sia8159-i2c"},
	{.compatible = "si,sia81x9-i2c"},
	{.compatible = "si,sia8152x-i2c"},
	{},
};
#endif

static struct i2c_driver si_sia81xx_i2c_driver = {
	.driver = {
		.name = SIA81XX_I2C_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = si_sia81xx_i2c_match,
#endif
	},
	.probe		= sia81xx_i2c_probe,
	.remove		= sia81xx_i2c_remove,
	.id_table	= si_sia81xx_i2c_id,
};

/********************************************************************
 * end - i2c bus driver
 ********************************************************************/

/********************************************************************
 * sia81xx dev driver
 ********************************************************************/

/* power manage options */
static int sia81xx_pm_suspend(
	struct device *dev)
{
#if 0
	sia81xx_dev_t *sia81xx = NULL;

	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);


	sia81xx = (sia81xx_dev_t *)dev_get_drvdata(dev);

	return sia81xx_suspend(sia81xx);
#else
	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	return 0;
#endif
}

static int sia81xx_pm_resume(struct device *dev)
{
#if 0
	sia81xx_dev_t *sia81xx = NULL;

	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	sia81xx = (sia81xx_dev_t *)dev_get_drvdata(dev);

	return sia81xx_resume(sia81xx);
#else
	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	return 0;
#endif
}

static const struct dev_pm_ops si_sia81xx_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sia81xx_pm_suspend, sia81xx_pm_resume)
};

static int sia81xx_probe(struct platform_device *pdev)
{
	int ret = 0;
	sia81xx_dev_t *sia81xx = NULL;
	const char *chip_type_name = NULL;
	unsigned int chip_type = CHIP_TYPE_UNKNOWN;
	struct pinctrl *sia81xx_pinctrl = NULL;
	struct pinctrl_state *pinctrl_state = NULL;
	int disable_pin = 0, owi_pin = 0, rst_pin = 0;

	pr_info("[ info][%s] %s: probe \r\n", LOG_FLAG, __func__);

	/* get chip type name */
	ret = of_property_read_string_index(pdev->dev.of_node,
				"si,sia81xx_type",
				0,
				&chip_type_name);
	if(0 != ret) {
		pr_err("[  err][%s] %s: get si,sia81xx_type return %d !!! \r\n", 
			LOG_FLAG, __func__, ret);
		return -ENODEV;
	}

	/* get chip type value, 
	 * and check the chip type Whether or not to be supported */
	chip_type = get_chip_type(chip_type_name);
	if(CHIP_TYPE_UNKNOWN == chip_type) {
		pr_err("[  err][%s] %s: CHIP_TYPE_UNKNOWN == chip_type !!! \r\n", 
			LOG_FLAG, __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32(pdev->dev.of_node, 
			"si,sia81xx_disable_pin", &disable_pin);
	if(0 != ret) {
		pr_err("[  err][%s] %s: get si,sia81xx_disable_pin return %d !!! \r\n", 
			LOG_FLAG, __func__, ret);
		return -ENODEV;
	}

	if(0 == disable_pin) {
		/* get reset gpio pin's pinctrl info */
		sia81xx_pinctrl = devm_pinctrl_get(&pdev->dev);
		if(NULL == sia81xx_pinctrl) {
			pr_err("[  err][%s] %s: NULL == pinctrl !!! \r\n", 
				LOG_FLAG, __func__);
			return -ENODEV;
		}

		/* get owi gpio pin's specify pinctrl state */
		pinctrl_state = pinctrl_lookup_state(sia81xx_pinctrl, "sia81xx_gpio");
		if(NULL == pinctrl_state) {
			pr_err("[  err][%s] %s: NULL == pinctrl_state !!! \r\n", 
				LOG_FLAG, __func__);
			ret = -ENODEV;
			goto out;
		}

		/* set this pinctrl state, make this pin works in the gpio mode */
		if(0 != (ret = pinctrl_select_state(sia81xx_pinctrl, pinctrl_state))) {
			pr_err("[  err][%s] %s: error pinctrl_select_state return %d \r\n", 
				LOG_FLAG, __func__, ret);
			ret = -ENODEV;
			goto out;
		}

		/* get reset pin's sn */
		rst_pin = of_get_named_gpio(
			pdev->dev.of_node, "si,sia81xx_reset", 0);
		if(rst_pin < 0) {
			pr_err("[  err][%s] %s: rst_pin < 0 !!! \r\n", LOG_FLAG, __func__);
			ret = -ENODEV;
			goto out;
		}

		/* get owi pin's sn */
		owi_pin = of_get_named_gpio(
			pdev->dev.of_node, "si,sia81xx_owi", 0);
		if(owi_pin < 0)	{
			pr_err("[  err][%s] %s: owi_pin < 0 !!! \r\n", LOG_FLAG, __func__);
			ret = -ENODEV;
			goto out;
		}
	}

	sia81xx = get_sia81xx_dev(&pdev->dev, pdev->dev.of_node);
	if(NULL == sia81xx) {
		pr_err("[  err][%s] %s: get_sia81xx_dev error !!! \r\n", 
			LOG_FLAG, __func__);
		ret = -ENODEV;
		goto out;
	}

	sia81xx->chip_type = chip_type;
	sia81xx->rst_pin = rst_pin;
	sia81xx->owi_pin = owi_pin;	
	sia81xx->disable_pin = disable_pin;
	
	spin_lock_init(&sia81xx->owi_lock);
	spin_lock_init(&sia81xx->rst_lock);

	if(0 == disable_pin) {
		/* set rst pin's direction */
		gpio_direction_output(sia81xx->rst_pin, SIA81XX_DISABLE_LEVEL);
		/* set owi pin's direction */
		gpio_direction_output(sia81xx->owi_pin, OWI_POLARITY);
	}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,16,28))
	ret = devm_snd_soc_register_component(&pdev->dev, &soc_component_dev_sia81xx, NULL, 0);
#else
	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_sia81xx, NULL, 0);
#endif

	if(0 != ret) {
		pr_err("[  err][%s] %s: snd_soc_register_codec fail \r\n", 
			LOG_FLAG, __func__);
		goto put_dev_out;
	}

	ret = sia81xx_dev_init(sia81xx, pdev->dev.of_node);
	if(0 != ret) {
		pr_err("[  err][%s] %s: sia81xx_dev_init fail \r\n", 
			LOG_FLAG, __func__);
		goto put_dev_out;
	}

	device_create_file(&pdev->dev, &dev_attr_sia81xx_cmd);

	/* save platform dev */
	sia81xx->pdev = pdev;
	
	/* sava driver private data to the dev's driver data */
	dev_set_drvdata(&pdev->dev, sia81xx);

	/* probe other sub module */
	if(1 == sia81xx->en_dyn_ud_vdd || 1 == sia81xx->en_dyn_ud_pvdd) {
		sia81xx_auto_set_vdd_probe(
			sia81xx->timer_task_hdl, 
			sia81xx->chip_type, 
			sia81xx->channel_num, 
			sia81xx->regmap, 
			sia81xx->dyn_ud_vdd_port, 
			SIA81XX_AUTO_VDD_EN_SET(sia81xx->en_dyn_ud_vdd) | 
			SIA81XX_AUTO_PVDD_EN_SET(sia81xx->en_dyn_ud_pvdd));
	}
	/* end - probe other sub module */

out:
	if(0 == disable_pin) {
		devm_pinctrl_put(sia81xx_pinctrl);
	}
	
	return ret;

put_dev_out:
	if(0 == disable_pin) {
		devm_pinctrl_put(sia81xx_pinctrl);
	}
	put_sia81xx_dev(sia81xx);
	
	return ret;
}

static int sia81xx_remove(struct platform_device *pdev)
{
	int ret = 0;
	sia81xx_dev_t *sia81xx = NULL;

	pr_info("[ info][%s] %s: remove \r\n", LOG_FLAG, __func__);

	sia81xx = (sia81xx_dev_t *)dev_get_drvdata(&pdev->dev);
	if(NULL == sia81xx)
		return 0;

	/* remove other sub module */
	sia81xx_auto_set_vdd_remove(
		sia81xx->timer_task_hdl, 
		sia81xx->channel_num);
	/* end - remove other sub module */

	ret = sia81xx_dev_remove(sia81xx);
	if(0 != ret) {
		pr_err("[  err][%s] %s: sia81xx_dev_remove return : %d \r\n", 
			LOG_FLAG, __func__, ret);
	}

	sia81xx->pdev = NULL;

	put_sia81xx_dev(sia81xx);

	return 0;
}

static void sia81xx_shutdown(struct platform_device *pdev)
{
	sia81xx_dev_t *sia81xx = NULL;

	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	if (NULL == pdev)
		return ;

	sia81xx = (sia81xx_dev_t *)dev_get_drvdata(&pdev->dev);

	sia81xx_suspend(sia81xx);
}

#ifdef CONFIG_OF
static const struct of_device_id si_sia81xx_dt_match[] = {
	{ .compatible = "si,sia81xx" },
	{ .compatible = "si,sia8100" },
	{ .compatible = "si,sia8101" },
	{ .compatible = "si,sia8108" },
	{ .compatible = "si,sia8109" },
	{ .compatible = "si,sia8152" },
	{ .compatible = "si,sia8152s" },
	{ .compatible = "si,sia8159" },
	{ .compatible = "si,sia81x9" },
	{ .compatible = "si,sia8152x" },
	{ }
};
MODULE_DEVICE_TABLE(of, si_sia81xx_dt_match);
#endif

static struct platform_driver si_sia81xx_dev_driver = {
	.probe  = sia81xx_probe,
	.remove = sia81xx_remove,
	.shutdown = sia81xx_shutdown,
	.driver = {
		.name = SIA81XX_NAME,
		.owner = THIS_MODULE,
		.pm = &si_sia81xx_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = si_sia81xx_dt_match,
#endif
	},
};

/********************************************************************
 * end - sia81xx dev driver
 ********************************************************************/

static int __init sia81xx_pa_init(void) 
{	
	int ret = 0;
	
	pr_info("[ info][%s] %s: sia81xx driver version : %s \r\n", 
		LOG_FLAG, __func__, SIA81XX_DRIVER_VERSION);

	sia81xx_timer_task_init();

	//init algorithm lib communication interface
	if(NULL != tuning_if_opt.init) {
		tuning_if_opt.init();
	}

	//init auto set vdd funtion
	sia81xx_set_vdd_init();

	//init online tuning funtion
#ifdef SIA81XX_TUNING
	sia81xx_sock_init();
#endif

	ret = platform_driver_register(&si_sia81xx_dev_driver);
	if (ret) {
		pr_err("[  err][%s] %s: si_sia81xx_dev error, ret = %d !!! \r\n",
			LOG_FLAG, __func__, ret);
		return ret;
	}

	ret = i2c_add_driver(&si_sia81xx_i2c_driver);
	if (ret) {
		pr_err("[  err][%s] %s: i2c_add_driver error, ret = %d !!! \r\n",
			LOG_FLAG, __func__, ret);
		return ret;
	}
	
	return 0;
}

static void __exit sia81xx_pa_exit(void) 
{
	pr_info("[ info][%s] %s: running \r\n", LOG_FLAG, __func__);

	//exit online tuning funtion
#ifdef SIA81XX_TUNING
	sia81xx_sock_exit();
#endif

	//exit auto set vdd funtion
	sia81xx_set_vdd_exit();

	//exit algorithm lib communication interface
	if(NULL != tuning_if_opt.exit) {
		tuning_if_opt.exit();
	}

	sia81xx_timer_task_exit();
	
	i2c_del_driver(&si_sia81xx_i2c_driver);

	platform_driver_unregister(&si_sia81xx_dev_driver);
}

module_init(sia81xx_pa_init);
module_exit(sia81xx_pa_exit);

MODULE_AUTHOR("yun shi <yun.shi@si-in.com>");
MODULE_DESCRIPTION("SI-IN SIA81xx ASoC driver");
MODULE_LICENSE("GPL");

