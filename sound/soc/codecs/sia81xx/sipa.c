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
#define LOG_FLAG	"sipa_driver"


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
#include <sound/core.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/cpufreq.h>
#include <linux/syscalls.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/version.h>

#include "sipa_common.h"
#include "sipa_regmap.h"
#include "sipa_timer_task.h"
#include "sipa_tuning_if.h"
#include "sipa_set_vdd.h"
#include "sia91xx_common.h"

#ifdef SIPA_TUNING
#include "sipa_socket.h"
#endif

#ifdef PLATFORM_TYPE_MTK
#include "mtk-sp-spk-amp.h"
#endif

#include "sipa_parameter.h"
#include "sipa_cal_spk.h"
#include "sipa_tuning_cmd.h"

#define SIPA_NAME							("sipa")
#define SIPA_I2C_NAME						(SIPA_NAME)

//#define DISTINGUISH_CHIP_TYPE
//#define ALGO_SWITCH_EN

#define SIPA_CMD_POWER_ON					(1)
#define SIPA_CMD_POWER_OFF					(2)
#define SIPA_CMD_GET_MODE					(3)
#define SIPA_CMD_SET_MODE					(4)
#define SIPA_CMD_GET_REG					(5)
#define SIPA_CMD_SET_REG					(6)

#define SIPA_CMD_WRITE_BYTE					(100)
#define SIPA_CMD_SET_OWI_DELAY				(101)
#define SIPA_CMD_CLR_ERROR					(200)
#define SIPA_CMD_SOCKET_OPEN				(400)
#define SIPA_CMD_SOCKET_CLOSE				(401)
#define SIPA_CMD_VDD_SET_OPEN				(402)
#define SIPA_CMD_VDD_SET_CLOSE				(403)
#define SIPA_CMD_TIMER_TASK_OPEN			(404)
#define SIPA_CMD_TIMER_TASK_CLOSE			(405)
#ifdef ALGO_SWITCH_EN
#define SIPA_CMD_ALGORITHM_OPEN				(406)
#define SIPA_CMD_ALGORITHM_CLOSE			(407)
#define SIPA_CMD_ALGORITHM_STATUS			(408)
#endif

#define SIPA_CMD_TEST_CREATE_FW_FILE		(600)
#define SIPA_CMD_TEST_LOAD_FW				(601)
#define SIPA_CMD_TEST_FW_SHOW				(602)
#define SIPA_CMD_TEST_SET_SPK_CAL			(603)
#define SIPA_CMD_TEST_SHOW_MONITOR			(604)
#define SIPA_CMD_TEST_SET_AUDIO_EFFECTS_EN	(605)
#define SIPA_CMD_TEST_GET_AUDIO_EFFECTS_EN	(606)
#define SIPA_CMD_TEST_DEBUG_PRINT_TO_DSP	(607)
#define SIPA_CMD_TEST_WRITE_FILE			(610)
#define SIPA_CMD_CAL_SPK_EXECUTE			(611)
char test_write_file_buf[] = "test write file data!";


#define SIPA_CMD_GET_RST_PIN				(612)
#define SIPA_CMD_SET_RST_PIN				(613)

// #define SIPA_CMD_SHOW_ALL_REG			(614)
#define SIPA_CMD_SET_MUTE_MODE				(615)
#define SIPA_CMD_WRITE_SPK_R0           	(616)
#define SIPA_CMD_MANUAL_CONFIG_R0       	(617)
#define SIPA_CMD_GET_RDC_TEMP           	(618)
#define SIPA_CMD_WRITE_SPK_MODE_PARAM   	(619)
#define SIPA_CMD_CLOSE_TEMP_LIMITER			(620)
#define SIPA_CMD_CLOSE_F0_TRACKING			(621)

#define SIPA_CMD_TEST						(777)

#ifdef SIA91XX_TYPE
#define SIA81XX_ENABLE_LEVEL				(0)
#define SIA81XX_DISABLE_LEVEL				(1)
#else
#define SIA81XX_ENABLE_LEVEL				(1)
#define SIA81XX_DISABLE_LEVEL				(0)
#endif
/* 10us > pulse width > 0.75us */
#define MIN_OWI_PULSE_GAP_TIME_US			(1)
#define MAX_OWI_PULSE_GAP_TIME_US			(160)
#define MAX_OWI_RETRY_TIMES					(10)
#define MIN_OWI_MODE						(1)
#define MAX_OWI_MODE						(16)
#define DEFAULT_OWI_MODE					(6)
/* OWI_POLARITY 0 : pulse level == high, 1 : pulse level == low */
#define OWI_POLARITY						(SIA81XX_DISABLE_LEVEL)


//#define OWI_SUPPORT_WRITE_DATA
#ifdef OWI_SUPPORT_WRITE_DATA
#define OWI_DATA_BIG_END
#endif

/* error list */
/* pulse width time out */
#define EPTOUT								(100)
/* pulse electrical level opposite with the polarity */
#define EPOLAR								(101)

#define SIXTH_SIPA_RX_MODULE				(0x1000E900)/* module id */
#define SIXTH_SIPA_RX_ENABLE				(0x1000EA01)/* parameter id */

static ssize_t sipa_cmd_show(struct device *cd,
	struct device_attribute *attr, char *buf);
static ssize_t sipa_cmd_store(struct device *cd,
	struct device_attribute *attr, const char *buf, size_t len);
static DEVICE_ATTR_RW(sipa_cmd);

#ifdef DISTINGUISH_CHIP_TYPE
static ssize_t sipa_device_show(struct device *cd,
	struct device_attribute *attr, char *buf);
static ssize_t sipa_device_store(struct device *cd,
	struct device_attribute *attr, const char *buf, size_t len);

static int check_sipa_status(sipa_dev_t *si_pa);
static DEVICE_ATTR_RW(sipa_device);
#endif

#ifdef SIA91XX_TYPE
static ssize_t sipa_spk_cal_show(struct device *cd,
	struct device_attribute *attr, char *buf);
static ssize_t sipa_spk_cal_store(struct device *cd,
	struct device_attribute *attr, const char *buf, size_t len);
static ssize_t sipa_f0_show(struct device *cd,
	struct device_attribute *attr, char *buf);
static ssize_t sipa_r0_show(struct device *cd,
	struct device_attribute *attr, char *buf);

static DEVICE_ATTR_RW(sipa_spk_cal);
static DEVICE_ATTR_RO(sipa_f0);
static DEVICE_ATTR_RO(sipa_r0);
#endif

static DEFINE_MUTEX(sia81xx_list_mutex);
static LIST_HEAD(sia81xx_list);

static const char *support_chip_type_name_table[] = {
	[CHIP_TYPE_SIA8101]  = "sia8101",
	[CHIP_TYPE_SIA8109]  = "sia8109",
	[CHIP_TYPE_SIA8152]  = "sia8152",
	[CHIP_TYPE_SIA8152S] = "sia8152s",
	[CHIP_TYPE_SIA8100X] = "sia8100x",
	[CHIP_TYPE_SIA8159]  = "sia8159",
	[CHIP_TYPE_SIA8159A] = "sia8159a",
	[CHIP_TYPE_SIA81X9]  = "sia81x9",
	[CHIP_TYPE_SIA8152X] = "sia8152x",
	[CHIP_TYPE_SIA9195]  = "sia9195",
	[CHIP_TYPE_SIA9175]  = "sia9175"
};

static sipa_dev_t *g_default_sia_dev;

static int sipa_resume(
	struct sipa_dev_s *si_pa);
static int sipa_suspend(
	struct sipa_dev_s *si_pa);

static int sia91xx_show_all_reg(struct sipa_dev_s *si_pa)
{
	char sia91Addr[] = {
				0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x08, 0x09, 0x0A,
				0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
				0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
				0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x30, 0x31,
				0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x6E, 0x6F,
				0x70, 0x71, 0x72};
	int i, arrSize;
	int reg = 0, ret = 0;
	arrSize = sizeof(sia91Addr) / sizeof(char);

	pr_info("rst_pin = %d dynamic_updata_vdd_port = 0x%x \r\n", si_pa->rst_pin,
			si_pa->dyn_ud_vdd_port);

	for (i = 0; i < arrSize; i++) {
		ret = regmap_read(si_pa->regmap, sia91Addr[i], &reg);
		if (ret == 0) {
			pr_info("reg addr = %02x,value = 0x%x\n", sia91Addr[i], reg);
			reg = 0;
		} else {
			pr_info("read fail : reg addr = %02x\n", sia91Addr[i]);
			return -1;
		}
	}
	return 0;
}

#ifdef SIA91XX_TYPE
/* Alpha */
static ssize_t sipa_spk_cal_store(
	struct device *cd,
	struct device_attribute *attr,
	const char *buf,
	size_t len)
{
	const char *split_symb = ",";
	char *cur = (char *)buf;
	char *slice = NULL;
	char *after = NULL;
	int32_t t0 = SIPA_CAL_SPK_DEFAULT_VAL;
	int32_t wire_r0 = SIPA_CAL_SPK_DEFAULT_VAL;
	sipa_dev_t *si_pa = (sipa_dev_t *)dev_get_drvdata(cd);

	slice = strsep(&cur, split_symb);
	if (NULL != slice) {
		t0 = simple_strtoul(slice, &after, 10);
	}

	slice = strsep(&cur, split_symb);
	if (NULL != slice) {
		wire_r0 = simple_strtoul(slice, &after, 10);
	}

	sipa_cal_spk_execute(
		si_pa->dyn_ud_vdd_port,
		si_pa->channel_num,
		t0,
		wire_r0);

	return len;
}

static ssize_t sipa_spk_cal_show(
	struct device *cd,
	struct device_attribute *attr,
	char *buf)
{
	sipa_dev_t *si_pa = (sipa_dev_t *)dev_get_drvdata(cd);
	SIPA_PARAM_CAL_SPK cal_spk;

	if (0 != sipa_param_read_spk_calibration(si_pa->channel_num, &cal_spk)) {
		strcpy(buf, "Read spker calibration parameters error \r\n");
		goto end;
	}

	if (SIPA_CAL_SPK_OK == cal_spk.cal_ok) {
		strcpy(buf, "ok \r\n");
	} else {
		strcpy(buf, "uncalibrated \r\n");
	}

end:
	return strlen(buf);
}

static ssize_t sipa_f0_show(
	struct device *cd,
	struct device_attribute *attr,
	char *buf)
{
	sipa_dev_t *si_pa = (sipa_dev_t *)dev_get_drvdata(cd);
	int instant_f0, rdc, temperature;

	if (0 != sipa_tuning_cmd_get_rdc_temp(
		si_pa->dyn_ud_vdd_port,
		si_pa->channel_num,
		&instant_f0,
		&rdc,
		&temperature)) {
		strcpy(buf, "Read f0 error \r\n");
		goto end;
	}
	sprintf(buf, "%d\r\n", instant_f0);

	end:
		return strlen(buf);

}

static ssize_t sipa_r0_show(
	struct device *cd,
	struct device_attribute *attr,
	char *buf)
{
	sipa_dev_t *si_pa = (sipa_dev_t *)dev_get_drvdata(cd);
	int32_t r0, t0, wire_r0, a;

	if (0 != sipa_tuning_cmd_get_spk_cal_val(si_pa->dyn_ud_vdd_port, si_pa->channel_num,
		&r0, &t0, &wire_r0, &a)) {
		strcpy(buf, "Read r0 error \r\n");
		goto end;
	}
	sprintf(buf, "%d\r\n", r0);

end:
		return strlen(buf);
}
#endif

/********************************************************************
 * si_pa misc option
 ********************************************************************/
static void clear_sipa_err_info(
	sipa_dev_t *si_pa)
{
	if (NULL == si_pa)
		return ;

	memset(&si_pa->err_info, 0, sizeof(si_pa->err_info));
}
/********************************************************************
 * end - si_pa misc option
 ********************************************************************/

/********************************************************************
 * si_pa GPIO option
 ********************************************************************/
static __inline void gpio_flipping(
	int pin,
	s64 *intervel_ns)
{
	static struct timespec64 cur_time, last_time, temp_time;

	gpio_set_value(pin, !gpio_get_value(pin));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0))
	ktime_get_real_ts64(&cur_time);
#else
	getnstimeofday64(&cur_time);
#endif
	temp_time = timespec64_sub(cur_time, last_time);
	if (NULL != intervel_ns)
		*intervel_ns = timespec64_to_ns(&temp_time);
	last_time = cur_time;
}

void distinguish_chip_type(sipa_dev_t *si_pa)
{
#ifdef DISTINGUISH_CHIP_TYPE
	/* check sia81xx is available */
	if (CHIP_TYPE_UNKNOWN == si_pa->chip_type ||
		0 != check_sipa_status(si_pa)) {

		si_pa->chip_type = CHIP_TYPE_UNKNOWN;
		si_pa->en_dyn_ud_pvdd = 0;
		pr_info("[ info][%s] %s: there is no si_pa device \r\n",
			LOG_FLAG, __func__);
	} else {
		device_create_file(&si_pa->pdev->dev, &dev_attr_sipa_device);

		pr_info("[ info][%s] %s: sipa device is available \r\n",
			LOG_FLAG, __func__);
	}
#endif
}
static __inline int gpio_produce_one_pulse(
	int pin,
	int polarity,
	unsigned int width_us,
	s64 *real_duty_ns,
	s64 *real_idle_ns)
{
	if (polarity != gpio_get_value(pin)) {
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
	if (0 == ret) /* if pulse produce suceess */
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

	if (0 == pin) {
		pr_err("[  err][%s] %s: 0 == pin \r\n", LOG_FLAG, __func__);
		return -EINVAL;
	}

	while (cycles) {
		cycles--;

		ret = gpio_produce_one_pulse_cycle(pin, polarity,
			delay_us, &real_duty_ns, &real_idle_ns);
		if (0 != ret)
			break;

		/* monitor pulse with overtime */
		if (false == is_fst_pulse) {
			max_pulse_time =
				max_pulse_time > real_idle_ns ? max_pulse_time : real_idle_ns;
		}
		max_pulse_time =
			max_pulse_time > real_duty_ns ? max_pulse_time : real_duty_ns;
		is_fst_pulse = false;

	}

	if (NULL != max_real_delay_ns)
		*max_real_delay_ns = max_pulse_time;

	return ret;
}
/********************************************************************
 * end - si_pa GPIO option
 ********************************************************************/


/********************************************************************
 * si_pa owi option
 ********************************************************************/
#ifdef OWI_SUPPORT_WRITE_DATA
static void sia81xx_owi_calc_bit_duty_ns(
	s64 duty_ns,
	s64 idle_ns,
	s64 *duty_time_ns,
	s64 *idle_time_ns,
	const char bit)
{
	if (0 == !!(bit & (char)0x01)) {
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
	struct sipa_dev_s *si_pa,
	const char bit,
	s64 *real_duty_ns,
	s64 *real_idle_ns)
{
	int ret = 0;
	unsigned int duty_time_us, idle_time_us;

	sia81xx_owi_calc_bit_duty_us(si_pa->owi_delay_us,
		MIN_OWI_PULSE_GAP_TIME_US, &duty_time_us, &idle_time_us, bit);

	ret = __gpio_produce_one_pulse_cycle(si_pa->owi_pin,
		si_pa->owi_polarity, duty_time_us, idle_time_us,
		real_duty_ns, real_idle_ns);

	return ret;
}

static int sia81xx_owi_start_write_byte(
	struct sipa_dev_s *si_pa)
{
	/* before start write, gpio level must be opposite with OWI_POLARITY */
	if (gpio_get_value(si_pa->owi_pin) == si_pa->owi_polarity) {
		pr_err("[  err][%s] %s: EPOLAR !!! \r\n", LOG_FLAG, __func__);
		return -EPOLAR;
	}

	/* *******************************************************************
	 * for reset flipping timer,
	 * otherwise only need gpio_set_value(dev->owi_pin, OWI_POLARITY);
	 * *******************************************************************/
	gpio_flipping(si_pa->owi_pin, NULL);

	return 0;
}

static void sia81xx_owi_end_write_byte(
	struct sipa_dev_s *si_pa,
	s64 *real_idle_ns)
{
	gpio_flipping(si_pa->owi_pin, real_idle_ns);
}

static s64 sia81xx_owi_calc_deviation_ns(
	struct sipa_dev_s *dev,
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

	if (duty_time_ns < idle_time_ns)
		deviation = idle_time_ns - duty_time_ns;

	return deviation;
}

static int sia81xx_owi_write_one_byte(
	struct sipa_dev_s *si_pa,
	const char data)
{
	int ret = 0, i = 0;
	s64 real_duty_ns = 0, real_idle_ns = 0, last_real_duty_ns = 0;
	s64 max_deviation_ns = 0, cur_deviation_ns = 0;
	char last_bit = 0;
	unsigned long flags;

	spin_lock_irqsave(&si_pa->owi_lock, flags);

	ret = sia81xx_owi_start_write_byte(si_pa);
	if (0 != ret)
		goto owi_end_write_one_byte;

#ifdef OWI_DATA_BIG_END
	for (i = 7; i >= 0; i--) {
		ret = sia81xx_owi_write_one_bit(si_pa, (data >> i),
			&real_duty_ns, &real_idle_ns);
		if (0 != ret)
			goto owi_end_write_one_byte;

		/* when second bit has been sent,
		 * can be get first bit's real idle time */
		if (7 > i) {
			cur_deviation_ns = sia81xx_owi_calc_deviation_ns(si_pa,
				last_real_duty_ns, real_idle_ns, last_bit);
			max_deviation_ns = max_deviation_ns > cur_deviation_ns ?
				max_deviation_ns : cur_deviation_ns;

			/* occur write error */
			if (0 != cur_deviation_ns)
				break;
		}
		last_bit = (data >> i);
		last_real_duty_ns = real_duty_ns;
	}
#else
	for (i = 0; i < 8; i++) {
		ret = sia81xx_owi_write_one_bit(si_pa, (data >> i),
			&real_duty_ns, &real_idle_ns);
		if (0 != ret)
			goto owi_end_write_one_byte;

		/* when second bit has been sent,
		 * can be get first bit's real idle time */
		if (0 < i) {
			cur_deviation_ns = sia81xx_owi_calc_deviation_ns(si_pa,
				last_real_duty_ns, real_idle_ns, last_bit);
			max_deviation_ns = max_deviation_ns > cur_deviation_ns ?
				max_deviation_ns : cur_deviation_ns;

			/* occur write error */
			if (0 != cur_deviation_ns)
				break;
		}
		last_bit = (data >> i);
		last_real_duty_ns = real_duty_ns;
	}
#endif

	/* end write byte, and get last bit real idle time */
	sia81xx_owi_end_write_byte(si_pa, &real_idle_ns);

	/* no write error */
	if (0 == cur_deviation_ns) {
		/* then can be get the last bit's real idle time */
		cur_deviation_ns = sia81xx_owi_calc_deviation_ns(si_pa,
			last_real_duty_ns, real_idle_ns, last_bit);
		max_deviation_ns = max_deviation_ns > cur_deviation_ns ?
			max_deviation_ns : cur_deviation_ns;
	}

owi_end_write_one_byte:

	spin_unlock_irqrestore(&si_pa->owi_lock, flags);

	if (0 != cur_deviation_ns)
		ret = -EPTOUT;

	if (0 != ret) {
		if (-EPOLAR == ret)
			/* record polarity err */
			si_pa->err_info.owi_polarity_err_cnt++;
		else
			/* record wirte err */
			si_pa->err_info.owi_write_err_cnt++;
	}

	/* record history max deviation time */
	si_pa->err_info.owi_max_deviation =
		si_pa->err_info.owi_max_deviation > max_deviation_ns ?
		si_pa->err_info.owi_max_deviation : max_deviation_ns;

	return ret;
}

static int sipa_owi_write_data(
	struct sipa_dev_s *si_pa,
	unsigned int len,
	const char *buf)
{
	int ret = 0, i = 0, retry = 0;

	si_pa->err_info.owi_write_data_cnt++;

	for (i = 0; i < len; i++) {

		retry = 0;

		while (retry < MAX_OWI_RETRY_TIMES) {

			ret = sia81xx_owi_write_one_byte(si_pa, buf[i]);
			if (0 == ret)
				break;

			/* must be ust msleep, do not use mdelay */
			msleep(1);

			retry++;
		}

		/* record max retry time */
		si_pa->err_info.owi_max_retry_cnt =
			si_pa->err_info.owi_max_retry_cnt > retry ?
			si_pa->err_info.owi_max_retry_cnt : retry;

		if (retry >= MAX_OWI_RETRY_TIMES) {
			si_pa->err_info.owi_write_data_err_cnt++;
			return -EPTOUT;
		}
	}

	return ret;
}
#endif

#ifndef SIA91XX_TYPE
static void sia81xx_set_owi_polarity(
	struct sipa_dev_s *si_pa)
{
	gpio_set_value(si_pa->owi_pin, si_pa->owi_polarity);
}
static int __sipa_owi_write_mode(
	struct sipa_dev_s *si_pa,
	unsigned int cycles)
{
	int ret = 0;
	s64 max_flipping_ns = 0, last_flipping_ns = 0;
	unsigned long flags;

	spin_lock_irqsave(&si_pa->owi_lock, flags);

	sia81xx_set_owi_polarity(si_pa);
	udelay(1500);	/* wait for owi reset, longer than 1ms */

	/* last pulse only flipping once, so pulses = cycles - 1 */
	ret = gpio_produce_pulse_cycles(si_pa->owi_pin, si_pa->owi_polarity,
			si_pa->owi_delay_us, cycles - 1, &max_flipping_ns);
	/* last pulse */
	gpio_flipping(si_pa->owi_pin, &last_flipping_ns);

	spin_unlock_irqrestore(&si_pa->owi_lock, flags);

	if (0 != ret) {
		if (-EPOLAR == ret)
			/* record polarity err */
			si_pa->err_info.owi_polarity_err_cnt++;
		else
			/* record wirte err */
			si_pa->err_info.owi_write_err_cnt++;
		return ret;
	}

	/* if cycles == 1, then only flipping gpio, and do not care flipping time */
	if (0 == (cycles - 1))
		return 0;

	/* get max flipping time */
	max_flipping_ns = max_flipping_ns > last_flipping_ns ?
		max_flipping_ns : last_flipping_ns;

	/* record history max flipping time */
	si_pa->err_info.owi_max_gap =
		si_pa->err_info.owi_max_gap > max_flipping_ns ?
		si_pa->err_info.owi_max_gap : max_flipping_ns;

	if ((MAX_OWI_PULSE_GAP_TIME_US * NSEC_PER_USEC) <= max_flipping_ns) {
		/* record wirte err */
		si_pa->err_info.owi_write_err_cnt++;
		return -EPTOUT;
	}

	return 0;
}

static int sipa_owi_write_mode(
	struct sipa_dev_s *si_pa,
	unsigned int mode)
{
	unsigned int retry = 0;

	if ((MAX_OWI_MODE < mode) || (MIN_OWI_MODE > mode))
		return -EINVAL;

	si_pa->err_info.owi_set_mode_cnt++;

	while (retry < MAX_OWI_RETRY_TIMES) {

		if (0 == __sipa_owi_write_mode(si_pa, mode))
			break;

		/* must be use msleep, do not use mdelay */
		msleep(1);

		retry++;
	}

	/* record max retry time */
	si_pa->err_info.owi_max_retry_cnt =
		si_pa->err_info.owi_max_retry_cnt > retry ?
		si_pa->err_info.owi_max_retry_cnt : retry;

	if (retry >= MAX_OWI_RETRY_TIMES) {
		si_pa->err_info.owi_set_mode_err_cnt++;
		return -EPTOUT;
	}

	return 0;
}

static int sipa_owi_init(
	sipa_dev_t *si_pa,
	unsigned int owi_mode)
{
	si_pa->owi_delay_us = MIN_OWI_PULSE_GAP_TIME_US;
	si_pa->owi_polarity = OWI_POLARITY;
	if ((owi_mode >= MIN_OWI_MODE) && (owi_mode <= MAX_OWI_MODE))
		si_pa->owi_cur_mode = owi_mode;
	else
		si_pa->owi_cur_mode = DEFAULT_OWI_MODE;

	pr_debug("[debug][%s] %s: running mode = %u \r\n",
		LOG_FLAG, __func__, si_pa->owi_cur_mode);

	return 0;
}
#endif
/********************************************************************
* end - si_pa owis option
********************************************************************/


/********************************************************************
 * si_pa chip option
 ********************************************************************/
static bool is_chip_type_supported(unsigned int chip_type)
{
	if (chip_type >= ARRAY_SIZE(support_chip_type_name_table))
		return false;

	return true;
}

static bool sipa_is_chip_en(sipa_dev_t *si_pa)
{
	if (0 == si_pa->disable_pin) {
#ifdef SIA91XX_TYPE
		if ((SIA91XX_ENABLE_LEVEL == gpio_get_value(si_pa->rst_pin))
			&& sipa_regmap_get_chip_en(si_pa))
			return true;
#else
		if (SIA81XX_ENABLE_LEVEL == gpio_get_value(si_pa->rst_pin))
			return true;
#endif
	} else {
		if (sipa_regmap_get_chip_en(si_pa))
			return true;
	}

	return false;
}

int sipa_reg_init(
	struct sipa_dev_s *si_pa)
{
	if (NULL == si_pa->client)
		return 0;

	if (CHIP_TYPE_SIA8101 == si_pa->chip_type
		&& 0 != si_pa->channel_num) {
		sipa_regmap_defaults(g_default_sia_dev->regmap,
			si_pa->chip_type, si_pa->scene, si_pa->channel_num);
	} else {
		sipa_regmap_defaults(si_pa->regmap,
			si_pa->chip_type, si_pa->scene, si_pa->channel_num);
	}

	udelay(100);
	if (0 != sipa_regmap_check_chip_id(si_pa->regmap,
			si_pa->channel_num, si_pa->chip_type)) {
		pr_err("[  err][%s] %s: sipa_regmap_check_chip_id error !!! \r\n",
			LOG_FLAG, __func__);
	}

	return 0;
}

static int sipa_resume(
	struct sipa_dev_s *si_pa)
{
	unsigned long flags;
	int default_sia_is_open = 0;

	pr_debug("[debug][%s] %s: si_pa->chip_type = %d running \r\n",
		LOG_FLAG, __func__, si_pa->chip_type);

	if (NULL == si_pa)
		return -ENODEV;

	if (is_chip_type_supported(si_pa->chip_type) &&
		!sipa_is_chip_en(si_pa)) {

		if (0 == si_pa->disable_pin) {

			if (CHIP_TYPE_SIA8101 == si_pa->chip_type
				&& 0 != si_pa->channel_num) {
				if (likely(NULL != g_default_sia_dev)) {
					default_sia_is_open = gpio_get_value(g_default_sia_dev->rst_pin);
					if (1 == default_sia_is_open)
						sipa_suspend(g_default_sia_dev);
				} else {
					pr_err("[  err][%s] %s: g_default_sia_dev == NULL !!! \r\n",
						LOG_FLAG, __func__);
					goto err_sipa_resume;
				}
			}
#ifndef SIA91XX_TYPE
			/* power up chip */
			if (0 != sipa_owi_write_mode(si_pa, si_pa->owi_cur_mode))
				goto err_sipa_resume;
#endif
			spin_lock_irqsave(&si_pa->rst_lock, flags);
			gpio_set_value(si_pa->rst_pin, SIA81XX_ENABLE_LEVEL);
			mdelay(1);	/* wait chip power up, the time must be > 1ms */
			spin_unlock_irqrestore(&si_pa->rst_lock, flags);

			if (CHIP_TYPE_SIA8109 == si_pa->chip_type ||
				CHIP_TYPE_SIA81X9 == si_pa->chip_type)
				mdelay(39);	/* for sia8109 gain rising. */
		}

		sipa_reg_init(si_pa);
		sipa_regmap_set_chip_on(si_pa);
		sipa_regmap_check_trimming(si_pa);

		if (0 == si_pa->disable_pin) {
			if (CHIP_TYPE_SIA8101 == si_pa->chip_type
				&& 0 != si_pa->channel_num) {
				if (1 == default_sia_is_open)
					sipa_resume(g_default_sia_dev);
			}
		}
	}

	if (si_pa->en_dyn_ud_vdd || si_pa->en_dyn_ud_pvdd
	    || si_pa->en_spk_cal_dl)
		sipa_timer_task_start(si_pa->timer_task_hdl);

	return 0;

err_sipa_resume:
	pr_err("[  err][%s] %s: error !!! \r\n", LOG_FLAG, __func__);
	return -EINVAL;

}

static int sipa_suspend(
	struct sipa_dev_s *si_pa)
{
	unsigned long flags;

	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	if (NULL == si_pa)
		return -ENODEV;

	if (si_pa->en_dyn_ud_vdd || si_pa->en_dyn_ud_pvdd)
		sipa_timer_task_stop(si_pa->timer_task_hdl);

	if (is_chip_type_supported(si_pa->chip_type) &&
		sipa_is_chip_en(si_pa)) {

		sipa_regmap_set_chip_off(si_pa);

		if (0 == si_pa->disable_pin) {
			spin_lock_irqsave(&si_pa->rst_lock, flags);
			/* power off chip */
			gpio_set_value(si_pa->rst_pin, SIA81XX_DISABLE_LEVEL);
			mdelay(1);	/* wait chip power off, the time must be > 1ms */
			spin_unlock_irqrestore(&si_pa->rst_lock, flags);
		}
	}

	return 0;
}

static int sipa_reboot(
	struct sipa_dev_s *si_pa)
{
	int ret = 0;

	ret = sipa_suspend(si_pa);
	if (0 != ret)
		return ret;

	mdelay(5);

	return sipa_resume(si_pa);
}

static int sipa_set_mode(
	struct sipa_dev_s *si_pa,
	unsigned int mode)
{
	int ret = 0;
	if (si_pa->chip_type == CHIP_TYPE_SIA9195) {
		pr_info("[ info]SIA9195 no mode set\n");
		return ret;
	}

	if ((MAX_OWI_MODE < mode) || (MIN_OWI_MODE > mode)) {
		pr_err("[  err][%s] %s: error mode = %u !!! \r\n",
			LOG_FLAG, __func__, mode);
		return -EINVAL;
	}

	si_pa->owi_cur_mode = mode;

	ret = sipa_reboot(si_pa);

	return ret;
}


static int sipa_dev_init(
	sipa_dev_t *si_pa,
	struct device_node	*sipa_of_node)
{
	int ret = 0;
#ifndef SIA91XX_TYPE
	int owi_mode = DEFAULT_OWI_MODE;
#endif
	int channel_num = 0;

	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	ret = of_property_read_u32(sipa_of_node,
			"channel_num", &channel_num);
	if (0 != ret) {
		channel_num = 0;
	}

#ifndef SIA91XX_TYPE
	ret = of_property_read_u32(sipa_of_node, "owi_mode", &owi_mode);
	if (0 != ret || MIN_OWI_MODE > owi_mode || MAX_OWI_MODE < owi_mode) {
		pr_err("[  err][%s] %s: get owi_mode "
			"form dts fail, ret = %d, owi_mode = %d !!! \r\n",
			LOG_FLAG, __func__, ret, owi_mode);
		owi_mode = DEFAULT_OWI_MODE;
	}
#endif

	clear_sipa_err_info(si_pa);

	si_pa->channel_num = (uint32_t)channel_num;
	si_pa->scene = AUDIO_SCENE_PLAYBACK;
#ifndef SIA91XX_TYPE
	sipa_owi_init(si_pa, (unsigned int)owi_mode);
#endif

	return 0;
}

static int sipa_dev_remove(
	sipa_dev_t *si_pa)
{
	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	si_pa->timer_task_hdl = SIPA_TIMER_TASK_INVALID_HDL;

	sipa_suspend(si_pa);

	return 0;
}

#ifdef ALGO_SWITCH_EN
static int sipa_algo_en_write(
	struct sipa_dev_s *si_pa,
	int32_t enable)
{
	unsigned long cal_handle = 0;

	pr_debug("[debug][%s] %s: tuning port = %d \r\n",
		LOG_FLAG, __func__, si_pa->dyn_ud_vdd_port);

	if (NULL == tuning_if_opt.open || NULL == tuning_if_opt.write) {
		pr_err("[  err][%s] %s: NULL == tuning_if_opt.open/write \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	cal_handle = tuning_if_opt.open(si_pa->dyn_ud_vdd_port);
	if (0 == cal_handle) {
		pr_err("[  err][%s] %s: NULL == cal_handle \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	return tuning_if_opt.write(cal_handle,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_ENABLE,
		sizeof(enable), (uint8_t *)&enable);
}

static int sipa_algo_en_read(
	struct sipa_dev_s *si_pa,
	int32_t *enable)
{
	unsigned long cal_handle = 0;

	pr_debug("[debug][%s] %s: tuning port = %d \r\n",
		LOG_FLAG, __func__, si_pa->dyn_ud_vdd_port);

	if (NULL == tuning_if_opt.open || NULL == tuning_if_opt.write) {
		pr_err("[  err][%s] %s: NULL == tuning_if_opt.open/write \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	cal_handle = tuning_if_opt.open(si_pa->dyn_ud_vdd_port);
	if (0 == cal_handle) {
		pr_err("[  err][%s] %s: NULL == cal_handle \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	return tuning_if_opt.read(cal_handle,
		SIXTH_SIPA_RX_MODULE,
		SIXTH_SIPA_RX_ENABLE,
		sizeof(*enable), (uint8_t *)enable);
}
#endif
/********************************************************************
 * end - si_pa chip option
 ********************************************************************/

#ifdef DISTINGUISH_CHIP_TYPE
/********************************************************************
 * device attr option
 ********************************************************************/
static ssize_t sipa_device_show(
	struct device *cd,
	struct device_attribute *attr,
	char *buf)
{
	return 0;
}

static ssize_t sipa_device_store(
	struct device *cd,
	struct device_attribute *attr,
	const char *buf,
	size_t len)
{
	return 0;
}
#endif

static ssize_t sipa_cmd_show(
	struct device *cd,
	struct device_attribute *attr,
	char *buf)
{
	sipa_dev_t *si_pa = (sipa_dev_t *)dev_get_drvdata(cd);
	char tb[1024];
	char chip_id = 0;
	char vals[0x64];
	int owi_pin_val = 0;

	if (si_pa->chip_type == CHIP_TYPE_SIA9195 ||
		si_pa->chip_type == CHIP_TYPE_SIA9175) {
		sia91xx_show_all_reg(si_pa);
		return 0;
	}

	switch (si_pa->chip_type) {

	case CHIP_TYPE_SIA8109:
		sipa_regmap_read(
			si_pa->regmap, si_pa->chip_type, 0x41, 1, &chip_id);
		break;
	case CHIP_TYPE_SIA8100X:
		pr_debug("[debug][%s] %s: delay = %u \r\n",
				LOG_FLAG, __func__, si_pa->owi_delay_us);
		return 0;
	default:
		sipa_regmap_read(
			si_pa->regmap, si_pa->chip_type, 0x00, 1, &chip_id);
		break;
	}

	memset(vals, 0, sizeof(vals));
	sipa_regmap_read(
		si_pa->regmap, si_pa->chip_type, 0x00, 0x0D + 1, vals);

	if (0 == si_pa->disable_pin) {
		owi_pin_val = gpio_get_value(si_pa->owi_pin);
	}

	snprintf(tb, 1024, "sipa_cmd_show : rst pin status : %d, chip id = 0x%02x \r\n"
		"reg val = %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x, "
		"%02x, %02x, %02x, %02x\r\n"
		"rst_pin = %d, owi_pin = %d \r\n"
		"set_mode = %lu, set_mode_err = %lu, polarity_err = %lu \r\n "
		"max_retry = %lu, write_err = %lu, delay = %u, m_gap = %lu \r\n "
		"owi_max_deviation = %lu, owi_write_data_err_cnt = %lu, \r\n"
		"owi_write_data_cnt = %lu \r\n"
		"channel_num = %u, owi_mode = %u \r\n"
		"si_pa_disable_pin = %d,  \r\n"
		"en_dynamic_updata_vdd = %u, en_dynamic_updata_pvdd = %u, \r\n"
		"dynamic_updata_vdd_port = 0x%x \r\n",
		owi_pin_val, chip_id,
		vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6],
		vals[7], vals[8], vals[9], vals[10], vals[11], vals[12], vals[13],
		si_pa->rst_pin, si_pa->owi_pin,
		si_pa->err_info.owi_set_mode_cnt,
		si_pa->err_info.owi_set_mode_err_cnt,
		si_pa->err_info.owi_polarity_err_cnt,
		si_pa->err_info.owi_max_retry_cnt,
		si_pa->err_info.owi_write_err_cnt,
		si_pa->owi_delay_us,
		si_pa->err_info.owi_max_gap,
		si_pa->err_info.owi_max_deviation,
		si_pa->err_info.owi_write_data_err_cnt,
		si_pa->err_info.owi_write_data_cnt,
		si_pa->channel_num, si_pa->owi_cur_mode,
		si_pa->disable_pin,
		si_pa->en_dyn_ud_vdd, si_pa->en_dyn_ud_pvdd,
		si_pa->dyn_ud_vdd_port);
	strcpy(buf, tb);

	return strlen(buf);
}

static ssize_t sipa_cmd_store(
	struct device *cd,
	struct device_attribute *attr,
	const char *buf,
	size_t len)
{
	const char *split_symb = ",";
	/* in strsep will be modify "cur" value, nor cur[i] value,
	so this point shoud not be defined with "char * const" */
	char *cur = (char *)buf;
	char *after;
	sipa_dev_t *si_pa = (sipa_dev_t *)dev_get_drvdata(cd);

	switch (simple_strtoul(strsep(&cur, split_symb), &after, 10)) {
	case SIPA_CMD_POWER_ON:
		sipa_resume(si_pa);
		break;
	case SIPA_CMD_POWER_OFF:
		sipa_suspend(si_pa);
		break;
	case SIPA_CMD_GET_MODE:
	{
		pr_debug("[debug][%s] %s: mode = %u \r\n",
				LOG_FLAG, __func__, si_pa->owi_cur_mode);
		break;
	}
	case SIPA_CMD_SET_MODE:
	{
		sipa_set_mode(si_pa,
			simple_strtoul(strsep(&cur, split_symb), &after, 10));
		break;
	}
	case SIPA_CMD_GET_REG:
	{
		unsigned char addr = (unsigned char)simple_strtoul(
				strsep(&cur, split_symb), &after, 16);
		unsigned int val = 0;

		regmap_read(si_pa->regmap, addr, &val);
		pr_debug("[debug][%s] %s: addr = 0x%x, val = 0x%x \r\n",
						LOG_FLAG, __func__, addr, val);
		break;
	}
	case SIPA_CMD_SET_REG:
	{
		unsigned char addr = (unsigned int)simple_strtoul(
				strsep(&cur, split_symb), &after, 16);
		unsigned int val = (unsigned int)simple_strtoul(
				strsep(&cur, split_symb), &after, 16);
		regmap_write(si_pa->regmap, addr, val);
		break;
	}
	case SIPA_CMD_SET_OWI_DELAY:
	{
		unsigned int temp_us = (unsigned int)simple_strtoul(
									strsep(&cur, split_symb), &after, 10);
		if (temp_us < MAX_OWI_PULSE_GAP_TIME_US) /* only for test, pulse width must be < 1ms */
			si_pa->owi_delay_us = temp_us;
		else
			si_pa->owi_delay_us = MAX_OWI_PULSE_GAP_TIME_US;
		break;
	}
	case SIPA_CMD_CLR_ERROR:
	{
		memset(&si_pa->err_info, 0, sizeof(si_pa->err_info));
		break;
	}
	case SIPA_CMD_WRITE_BYTE:
	{
#ifdef OWI_SUPPORT_WRITE_DATA
		char data = simple_strtoul(strsep(&cur, split_symb), &after, 10);
		sipa_owi_write_data(si_pa, 1, &data);
#else
		pr_info("[ info][%s] %s: the option "
			"OWI_SUPPORT_WRITE_DATA unsupport!! \r\n", LOG_FLAG, __func__);
#endif
		break;
	}
	case SIPA_CMD_SOCKET_OPEN:
	{
#ifdef SIPA_TUNING
		sipa_open_sock_server();
#endif
		break;
	}
	case SIPA_CMD_SOCKET_CLOSE:
	{
#ifdef SIPA_TUNING
		sipa_close_sock_server();
#endif
		break;
	}
	case SIPA_CMD_VDD_SET_OPEN:
	{
		si_pa->en_dyn_ud_vdd = 1;
		sipa_auto_set_vdd_probe(
			si_pa->timer_task_hdl,
			si_pa->chip_type,
			si_pa->channel_num,
			si_pa->regmap,
			si_pa->dyn_ud_vdd_port,
			SIPA_AUTO_VDD_EN_SET(si_pa->en_dyn_ud_vdd) |
			SIPA_AUTO_PVDD_EN_SET(si_pa->en_dyn_ud_pvdd));
		pr_debug("[debug][%s] %s: set auto vdd state %u, port 0x%04x \r\n",
				LOG_FLAG, __func__,
				si_pa->en_dyn_ud_vdd,
				si_pa->dyn_ud_vdd_port);
		break;
	}
	case SIPA_CMD_VDD_SET_CLOSE:
	{
		//sipa_disable_auto_set_vdd(sipa->dyn_ud_vdd_port);
		si_pa->en_dyn_ud_vdd = 0;

		sipa_set_auto_set_vdd_work_state(
			si_pa->timer_task_hdl,
			si_pa->channel_num,
			si_pa->en_dyn_ud_vdd);

		sipa_auto_set_vdd_remove(
			si_pa->timer_task_hdl,
			si_pa->channel_num);
		//sia81xx_close_set_vdd_server(0x400c/* SLIMBUS_6_RX */);
		pr_debug("[debug][%s] %s: set auto vdd state %u, port 0x%04x \r\n",
				LOG_FLAG, __func__,
				si_pa->en_dyn_ud_vdd,
				si_pa->dyn_ud_vdd_port);
		break;
	}
	case SIPA_CMD_TIMER_TASK_OPEN:
	{
		si_pa->timer_task_hdl = si_pa->timer_task_hdl_backup;
		break;
	}
	case SIPA_CMD_TIMER_TASK_CLOSE:
	{
		sipa_timer_task_stop(si_pa->timer_task_hdl);
		si_pa->timer_task_hdl_backup = si_pa->timer_task_hdl;
		si_pa->timer_task_hdl = SIPA_TIMER_TASK_INVALID_HDL;
		break;
	}
#ifdef ALGO_SWITCH_EN
	case SIPA_CMD_ALGORITHM_OPEN:
	{
		sipa_algo_en_write(si_pa, 1);
		break;
	}
	case SIPA_CMD_ALGORITHM_CLOSE:
	{
		sipa_algo_en_write(si_pa, 0);
		break;
	}
	case SIPA_CMD_ALGORITHM_STATUS:
	{
		int32_t algo_en;
		if (0 != sipa_algo_en_read(si_pa, &algo_en)) {
			pr_debug("[debug][%s] %s: err sipa_algo_en_read \r\n",
				LOG_FLAG, __func__);
		} else {
			pr_debug("[debug][%s] %s: algo_en = %d \r\n",
				LOG_FLAG, __func__, algo_en);
		}
		break;
	}
#endif
	case SIPA_CMD_GET_RST_PIN:
	{
		if (0 == si_pa->disable_pin) {
			unsigned int gpio_value = 0xff;

			gpio_value = gpio_get_value(si_pa->rst_pin);
			pr_debug("[debug][%s] %s: get reset pin value, pin num:%u, value:%u \r\n",
					LOG_FLAG, __func__, si_pa->rst_pin, gpio_value);
		}
		break;
	}
	case SIPA_CMD_SET_RST_PIN:
	{
		if (0 == si_pa->disable_pin) {
			unsigned char value = (unsigned int)simple_strtoul(
				strsep(&cur, split_symb), &after, 16);

			/* power on/off chip */
			gpio_set_value(si_pa->rst_pin, value);
			mdelay(1);	/* wait chip power off, the time must be > 1ms */

			pr_debug("[debug][%s] %s: set reset pin %u to %u \r\n",
					LOG_FLAG, __func__, si_pa->rst_pin, value);
		}
		break;
	}
/*
	case SIPA_CMD_SHOW_ALL_REG:
	{
		sia91xx_show_all_reg(si_pa);
		break;
	}
*/
	case SIPA_CMD_SET_MUTE_MODE:
	{
		unsigned char mode = (unsigned char)simple_strtoul(
			strsep(&cur, split_symb), &after, 16);

		if (0 == mode) {
			si_pa->mute_mode = MUTE_MODE_EXTERNAL;
		} else if (1 == mode) {
			si_pa->mute_mode = MUTE_MODE_INTERNAL;
		} else {
			pr_info("mute mode error!\n");
		}
		break;
	}

	case SIPA_CMD_TEST_CREATE_FW_FILE:
	{
		sipa_param_create_default_param();
		break;
	}
	case SIPA_CMD_TEST_LOAD_FW:
	{
		//sipa_param_load_fw(&si_pa->pdev->dev);
		break;
	}
	case SIPA_CMD_TEST_FW_SHOW:
	{
		sipa_param_print();
		break;
	}
	case SIPA_CMD_TEST_SET_SPK_CAL:
	{
		SIPA_PARAM_CAL_SPK cal_spk;
		sipa_param_read_spk_calibration(
			si_pa->channel_num, &cal_spk);
		sipa_tuning_cmd_set_spk_cal_val(
			si_pa->dyn_ud_vdd_port, si_pa->channel_num,
			cal_spk.r0, cal_spk.t0, cal_spk.a, cal_spk.wire_r0);
		break;
	}
	case SIPA_CMD_TEST_SHOW_MONITOR:
	{
		sipa_tuning_cmd_print_monitor_data(
			si_pa->dyn_ud_vdd_port, si_pa->channel_num);
		break;
	}
	case SIPA_CMD_TEST_SET_AUDIO_EFFECTS_EN:
	{
		sipa_tuning_cmd_set_en(
			si_pa->dyn_ud_vdd_port,
			simple_strtoul(strsep(&cur, split_symb), &after, 10));
		break;
	}
	case SIPA_CMD_TEST_GET_AUDIO_EFFECTS_EN:
	{
		sipa_tuning_cmd_get_en(si_pa->dyn_ud_vdd_port);
		break;
	}
	case SIPA_CMD_TEST_DEBUG_PRINT_TO_DSP:
	{
		char *str = strsep(&cur, split_symb);
		if (NULL == str) {
			sipa_tuning_cmd_debug_show(
				si_pa->dyn_ud_vdd_port, SIPA_CAL_SPK_DEFAULT_VAL);
		} else {
			sipa_tuning_cmd_debug_show(
				si_pa->dyn_ud_vdd_port, simple_strtoul(str, &after, 10));

		}
		break;
	}
	case SIPA_CMD_TEST_WRITE_FILE:
	{

		struct file *file;
		mm_segment_t fs;
		//loff_t pos = 0;
		//int ret = 0;
		char *path = strsep(&cur, split_symb);
		char *cmd = strsep(&cur, split_symb);
		if (NULL == path) {
			pr_err("[  err][%s] %s: no path \r\n", LOG_FLAG, __func__);
		} else {
			pr_err("[  err][%s] %s: pwd: %s \r\n", LOG_FLAG, __func__, path);

			file = filp_open(path, O_WRONLY | O_CREAT, 0);
			if (IS_ERR(file)) {
				pr_err("[  err][%s] %s: open file(%s) err %d \r\n",
					LOG_FLAG, __func__, path, (int)(PTR_ERR(file)));
				break ;
			}

			if (NULL != cmd) {
				int val = simple_strtoul(cmd, &after, 10);
				pr_err("[  err][%s] %s: val: %d \r\n", LOG_FLAG, __func__, val);
				if (0 == val) {
					fs = get_fs();
					set_fs(KERNEL_DS);

					//ret = kernel_write(file, (const char __user *)test_write_file_buf,
						//sizeof(test_write_file_buf), &pos);

					set_fs(fs);
				} else if (1 == val) {
					fs = get_fs();
					set_fs(KERNEL_DS);

					//ret = kernel_write(file, (const char *)test_write_file_buf,
						//sizeof(test_write_file_buf), &pos);

					set_fs(fs);
				} else if (2 == val) {
					//ret = kernel_write(file, (const char __user *)test_write_file_buf,
						//sizeof(test_write_file_buf), &pos);
				} else if (3 == val) {
					//ret = kernel_write(file, (const char *)test_write_file_buf,
						//sizeof(test_write_file_buf), &pos);
				}
			}

			filp_close(file, NULL);
			//pr_info("[ info][%s] %s: kernel_write done ret = %d \r\n",
				//LOG_FLAG, __func__, ret);
		}
		break;

	}
	case SIPA_CMD_WRITE_SPK_R0:
	{
		uint32_t ch = (unsigned int)simple_strtoul(
			strsep(&cur, split_symb), &after, 10);
		uint32_t r0 = (unsigned int)simple_strtoul(
			strsep(&cur, split_symb), &after, 10);
		uint32_t cal_ok = (unsigned int)simple_strtoul(
			strsep(&cur, split_symb), &after, 10);
		sipa_param_write_spk_r0(ch, r0, cal_ok);
		break;
	}
	case SIPA_CMD_MANUAL_CONFIG_R0:
	{
		uint32_t r0 = (unsigned int)simple_strtoul(
			strsep(&cur, split_symb), &after, 10);
		SIPA_PARAM_CAL_SPK cal_spk;
		memset(&cal_spk, 0x00, sizeof(cal_spk));

		sipa_tuning_cmd_get_spk_cal_val(si_pa->dyn_ud_vdd_port,
			si_pa->channel_num, &cal_spk.r0, &cal_spk.t0, &cal_spk.wire_r0, &cal_spk.a);

		cal_spk.r0 = r0;
		cal_spk.t0 = 25000;
		sipa_tuning_cmd_set_spk_cal_val(
			si_pa->dyn_ud_vdd_port, si_pa->channel_num,
			cal_spk.r0, cal_spk.t0, cal_spk.a, cal_spk.wire_r0);
		break;
	}
	case SIPA_CMD_GET_RDC_TEMP:
	{
		int instant_f0, rdc, temperature;

		sipa_tuning_cmd_get_rdc_temp(
			si_pa->dyn_ud_vdd_port,
			si_pa->channel_num,
			&instant_f0,
			&rdc,
			&temperature);
		break;
	}
	case SIPA_CMD_WRITE_SPK_MODE_PARAM:
	{
		SIPA_PARAM_SPK_MODEL_PARAM spk_model_param;
		uint32_t ch = (unsigned int)simple_strtoul(
			strsep(&cur, split_symb), &after, 10);
		spk_model_param.f0 = (unsigned int)simple_strtoul(
			strsep(&cur, split_symb), &after, 10);
		spk_model_param.q = (unsigned int)simple_strtoul(
			strsep(&cur, split_symb), &after, 10);
		spk_model_param.xthresh = (unsigned int)simple_strtoul(
			strsep(&cur, split_symb), &after, 10);
		spk_model_param.xthresh_rdc = (unsigned int)simple_strtoul(
			strsep(&cur, split_symb), &after, 10);

		sipa_param_write_spk_model(ch, &spk_model_param);
		break;
	}

	case SIPA_CMD_CLOSE_TEMP_LIMITER:
	{
		sipa_tuning_cmd_close_temp_limiter(
			si_pa->dyn_ud_vdd_port,
			si_pa->channel_num,
			0);
		break;
	}

	case SIPA_CMD_CLOSE_F0_TRACKING:
	{
		sipa_tuning_cmd_close_f0_tracking(
			si_pa->dyn_ud_vdd_port,
			si_pa->channel_num,
			0);
		break;
	}

	case SIPA_CMD_TEST:
		break;

	default:
		return -ENOIOCTLCMD;
	}

	return len;
}
/********************************************************************
 * end - device attr option
 ********************************************************************/


/********************************************************************
 * si_pa codec driver
 ********************************************************************/
static int sipa_power_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	sipa_dev_t *si_pa = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	sipa_dev_t *si_pa = snd_soc_codec_get_drvdata(codec);
#endif

	ucontrol->value.integer.value[0] =
		(unsigned long)sipa_is_chip_en(si_pa);

	pr_debug("[debug][%s] %s: ucontrol = %ld, channel_num = %d \r\n",
		LOG_FLAG, __func__, ucontrol->value.integer.value[0], si_pa->channel_num);

	return 0;
}

static int sipa_power_set(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
    sipa_dev_t *si_pa = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	sipa_dev_t *si_pa = snd_soc_codec_get_drvdata(codec);
#endif

	pr_debug("[debug][%s] %s: ucontrol = %ld, rst = %d, channel_num = %d \r\n",
		LOG_FLAG, __func__, ucontrol->value.integer.value[0],
		si_pa->rst_pin, si_pa->channel_num);

	if (1 == ucontrol->value.integer.value[0]) {
		sipa_resume(si_pa);
	} else {
		sipa_suspend(si_pa);
	}

	return 0;
}

static int sipa_audio_scene_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	sipa_dev_t *si_pa = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	sipa_dev_t *si_pa = snd_soc_codec_get_drvdata(codec);
#endif

	ucontrol->value.integer.value[0] = si_pa->scene;

	pr_debug("[debug][%s] %s: ucontrol = %ld, channle = %d \r\n",
		LOG_FLAG, __func__, ucontrol->value.integer.value[0], si_pa->channel_num);

	return 0;
}

static int sipa_audio_scene_set(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	sipa_dev_t *si_pa = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	sipa_dev_t *si_pa = snd_soc_codec_get_drvdata(codec);
#endif

	pr_debug("[debug][%s] %s: ucontrol = %ld, rst = %d, channle = %d \r\n",
		LOG_FLAG, __func__, ucontrol->value.integer.value[0],
		si_pa->rst_pin, si_pa->channel_num);

	if (AUDIO_SCENE_NUM <= ucontrol->value.integer.value[0]) {
		si_pa->scene = AUDIO_SCENE_PLAYBACK;
		pr_err("[  err][%s] %s: set audio scene val = %ld !!! \r\n",
			LOG_FLAG, __func__, ucontrol->value.integer.value[0]);
	} else {
		si_pa->scene = ucontrol->value.integer.value[0];
	}

	if (sipa_is_chip_en(si_pa)) {
#ifdef SIA91XX_TYPE
		sia91xx_soft_mute(si_pa);
		sipa_reg_init(si_pa);
		sia91xx_dsp_start(si_pa, SNDRV_PCM_STREAM_PLAYBACK);
#else
		sipa_reboot(si_pa);
#endif
	}
	return 0;
}

#ifdef ALGO_SWITCH_EN
static int sipa_algo_en_get(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	sipa_dev_t *si_pa = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	sipa_dev_t *si_pa = snd_soc_codec_get_drvdata(codec);
#endif
	int32_t algo_en = 0;

	pr_debug("[debug][%s] %s: ucontrol = %ld \r\n",
		LOG_FLAG, __func__, ucontrol->value.integer.value[0]);

	if (sipa_is_chip_en(si_pa))
		sipa_algo_en_read(si_pa, &algo_en);

	ucontrol->value.integer.value[0] = algo_en;
	return 0;
}

static int sipa_algo_en_set(
	struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	sipa_dev_t *si_pa = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	sipa_dev_t *si_pa = snd_soc_codec_get_drvdata(codec);
#endif

	pr_debug("[debug][%s] %s: ucontrol = %ld\r\n",
		LOG_FLAG, __func__, ucontrol->value.integer.value[0]);

	if (sipa_is_chip_en(si_pa))
		if (0 >= sipa_algo_en_write(si_pa,
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

static const struct snd_kcontrol_new sipa_controls[] = {

	SOC_ENUM_EXT("Sipa Power", power_enum,
			sipa_power_get, sipa_power_set),
#ifdef ALGO_SWITCH_EN
	SOC_ENUM_EXT("Sipa Algo", algo_enum,
			sipa_algo_en_get, sipa_algo_en_set),
#endif

	SOC_ENUM_EXT("Sipa Audio Scene", audio_scene_enum,
			sipa_audio_scene_get, sipa_audio_scene_set)
};

#ifdef SIA91XX_TYPE
static const struct snd_soc_dai_ops sia91xx_dai_ops = {
	.startup = sia91xx_startup,
	.set_fmt = sia91xx_set_fmt,
	.hw_params = sia91xx_hw_params,
	.mute_stream = sia91xx_mute,
};

static struct snd_soc_dai_driver sia91xx_dai[] = {
	{
		.name = "sia91xx-aif",
		.id = 1,
		.playback = {
			.stream_name = "AIF Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SIA91XX_RATES,
			.formats = SIA91XX_FORMATS,
		},
		.capture = {
			 .stream_name = "AIF Capture",
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = SIA91XX_RATES,
			 .formats = SIA91XX_FORMATS,
		 },
		 .ops = &sia91xx_dai_ops,
	}
};
#endif

static int sipa_spkr_pa_event(
	struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol,
	int event)
{
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	sipa_dev_t *si_pa = snd_soc_component_get_drvdata(component);
#else
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	sipa_dev_t *si_pa = snd_soc_codec_get_drvdata(codec);
#endif

	pr_debug("[debug][%s] %s: msg = %d \r\n", LOG_FLAG, __func__, event);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		sipa_resume(si_pa);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		sipa_suspend(si_pa);
		break;
	default:
		pr_err("[  err][%s] %s: msg = %d \r\n", LOG_FLAG, __func__, event);
		break;
	}

	return 0;
}

static const struct snd_soc_dapm_widget sipa_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("IN"),

	SND_SOC_DAPM_OUT_DRV_E("SPKR DRV", 0, 0, 0, NULL, 0,
			sipa_spkr_pa_event, SND_SOC_DAPM_POST_PMU |
			SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUTPUT("SPKR")
};

/*static const struct snd_soc_dapm_route sia81xx_audio_map[] = {
	{"SPKR DRV", NULL, "IN"},
	{"SPKR", NULL, "SPKR DRV"}
};*/


#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
static int sipa_component_probe(
	struct snd_soc_component *component)
{
    pr_info("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);
#ifdef SIA91XX_TYPE
	sia91xx_component_probe(component);
#endif
	return 0;
}

static void sipa_component_remove(
	struct snd_soc_component *component)
{
	pr_info("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);
	return;
}
#else
static int sipa_codec_probe(
	struct snd_soc_codec *codec)
{
    pr_info("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);
#ifdef SIA91XX_TYPE
	sia91xx_codec_probe(codec);
#endif
	return 0;
}

static int sipa_codec_remove(
	struct snd_soc_codec *codec)
{
	pr_info("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);
	return 0;
}

static struct regmap *sipa_codec_get_regmap(struct device *dev)
{
	return NULL;
}
#endif


#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
static int sipa_component_write(struct snd_soc_component *component,
				 unsigned int reg, unsigned int val) 
{
	return 0;
}

static unsigned int sipa_component_read(struct snd_soc_component *component,
			     unsigned int reg)
{
	return 0;
}

static struct snd_soc_component_driver soc_component_dev_sipa = {
	.probe = sipa_component_probe,
	.remove = sipa_component_remove,
	.controls = sipa_controls,
	.num_controls = ARRAY_SIZE(sipa_controls),
	.dapm_widgets = sipa_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sipa_dapm_widgets),
	.dapm_routes = NULL, //sia81xx_audio_map,
	.num_dapm_routes = 0,//ARRAY_SIZE(sia81xx_audio_map),
	.write = sipa_component_write,
	.read = sipa_component_read,
};

#elif (LINUX_VERSION_CODE > KERNEL_VERSION(4, 8, 17) && LINUX_VERSION_CODE <= KERNEL_VERSION(4, 16, 28))
static struct snd_soc_codec_driver soc_codec_dev_sipa = {
	.probe = sipa_codec_probe,
	.remove = sipa_codec_remove,
	.get_regmap = sipa_codec_get_regmap,
	.component_driver = {
		.controls = sipa_controls,
		.num_controls = ARRAY_SIZE(sipa_controls),
		.dapm_widgets = sipa_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(sipa_dapm_widgets),
		.dapm_routes = NULL, //sia81xx_audio_map,
		.num_dapm_routes = 0,//ARRAY_SIZE(sia81xx_audio_map),
	},
};
#else
static struct snd_soc_codec_driver soc_codec_dev_sipa = {
	.probe = sipa_codec_probe,
	.remove = sipa_codec_remove,
	.get_regmap = sipa_codec_get_regmap,
	.controls = sipa_controls,
	.num_controls = ARRAY_SIZE(sipa_controls),
	.dapm_widgets = sipa_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(sipa_dapm_widgets),
	.dapm_routes = NULL, //sia81xx_audio_map,
	.num_dapm_routes = 0,//ARRAY_SIZE(sia81xx_audio_map),
};
#endif

/********************************************************************
 * end - sipa codec driver
 ********************************************************************/




/********************************************************************
 * sipa driver common
 ********************************************************************/
static unsigned int sipa_list_count(void)
{
	unsigned count = 0;
	sipa_dev_t *si_pa = NULL;

	mutex_lock(&sia81xx_list_mutex);

	list_for_each_entry(si_pa, &sia81xx_list, list) {
		count++;
	}

	mutex_unlock(&sia81xx_list_mutex);

	return count;
}

sipa_dev_t *find_sipa_dev(
	const char *name,
	struct device_node *of_node)
{
	sipa_dev_t *si_pa = NULL, *find = NULL;

	if ((NULL == name) && (NULL == of_node)) {
		pr_err("[  err][%s] %s: NULL == input parameter \r\n",
			LOG_FLAG, __func__);
		return NULL;
	}

	mutex_lock(&sia81xx_list_mutex);

	list_for_each_entry(si_pa, &sia81xx_list, list) {
		if (NULL != name) {
			if (0 == strcmp(si_pa->name, name)) {
				find = si_pa;
				break;
			}
		}

		/* make sure that the sipa platform dev had been created */
		if ((NULL != of_node) && (NULL != si_pa->pdev)) {
			if (of_node == si_pa->pdev->dev.of_node) {
				find = si_pa;
				break;
			}
		}
	}

	mutex_unlock(&sia81xx_list_mutex);

	return find;
}

static void add_sipa_dev(
	sipa_dev_t *si_pa)
{
	struct device_node *of_node = NULL;

	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: NULL == si_pa \r\n", LOG_FLAG, __func__);
		return ;
	}

	if (NULL != si_pa->pdev)
		of_node = si_pa->pdev->dev.of_node;

	if (NULL != find_sipa_dev(si_pa->name, of_node))
		return ;

	mutex_lock(&sia81xx_list_mutex);
	list_add(&si_pa->list, &sia81xx_list);
	mutex_unlock(&sia81xx_list_mutex);

	pr_debug("[debug][%s] %s: add si_pa dev : %s, count = %u \r\n",
		LOG_FLAG, __func__, si_pa->name, sipa_list_count());
}

static void del_sipa_dev(
	sipa_dev_t *si_pa)
{
	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: NULL == si_pa \r\n", LOG_FLAG, __func__);
		return ;
	}

	pr_debug("[debug][%s] %s: del si_pa dev : %s, count = %u \r\n",
		LOG_FLAG, __func__, si_pa->name, sipa_list_count());

	mutex_lock(&sia81xx_list_mutex);
	list_del(&si_pa->list);
	mutex_unlock(&sia81xx_list_mutex);
}

static sipa_dev_t *get_sipa_dev(
	struct device *dev,
	struct device_node	*sipa_of_node)
{
	int ret = 0;
	sipa_dev_t *si_pa = NULL;
	const char *si_pa_dev_name = NULL;

	if (NULL == dev) {
		pr_err("[  err][%s] %s: NULL == dev \r\n", LOG_FLAG, __func__);
		return NULL;
	}

	/* check dev has been created or not by "si,sipa-dev-name" or of_node */
	ret = of_property_read_string_index(dev->of_node,
						    "si,sipa-dev-name",
						    0,
						    &si_pa_dev_name);
	if (0 != ret) {
		/* should been had one of "name" or "of_node" at least */
		if (NULL == sipa_of_node)
			return NULL;

		si_pa = find_sipa_dev(NULL, sipa_of_node);
		if (NULL == si_pa) {
			/* don't use devm_kzalloc() */
			si_pa = kzalloc(sizeof(sipa_dev_t), GFP_KERNEL);
			if (NULL == si_pa) {
				pr_err("[  err][%s] %s: dev[%s] cannot create "
					"memory for si_pa\n",
					LOG_FLAG, __func__, dev->init_name);
				return NULL;
			}

			/* default name */
			snprintf(si_pa->name, strlen("sipa-dummy.%u"),
				"sipa-dummy.%u", sipa_list_count());
		}
	} else {
		si_pa = find_sipa_dev(si_pa_dev_name, sipa_of_node);
		if (NULL == si_pa) {
			/* don't use devm_kzalloc() */
			si_pa = kzalloc(sizeof(sipa_dev_t), GFP_KERNEL);
			if (NULL == si_pa) {
				pr_err("[  err][%s] %s: dev[%s] cannot create "
					"memory for si_pa\n",
					LOG_FLAG, __func__, dev->init_name);
				return NULL;
			}

			strcpy(si_pa->name, si_pa_dev_name);
		}
	}

	add_sipa_dev(si_pa);

	pr_debug("[debug][%s] %s: get dev name : %s \r\n",
		LOG_FLAG, __func__, si_pa->name);

	return si_pa;
}

static void put_sipa_dev(sipa_dev_t *si_pa)
{
	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: NULL == si_pa \r\n", LOG_FLAG, __func__);
		return ;
	}

	pr_debug("[debug][%s] %s: put dev name : %s, pdev = %p, client = %p \r\n",
				LOG_FLAG, __func__,
				si_pa->name, si_pa->pdev, si_pa->client);

	if ((NULL != si_pa->pdev) || (NULL != si_pa->client))
		return ;

	del_sipa_dev(si_pa);
	kfree(si_pa);
}

static unsigned int get_chip_type(const char *name)
{
	int i = 0, len = 0;

	if (NULL == name)
		return CHIP_TYPE_UNKNOWN;

	pr_info("[ info][%s] %s: chip : %s \r\n",
		LOG_FLAG, __func__, name);

	len = strlen(name);
	for (i = 0; i < ARRAY_SIZE(support_chip_type_name_table); i++) {
		if (strlen(support_chip_type_name_table[i]) == len &&
			0 == memcmp(support_chip_type_name_table[i], name, len)) {
			return i;
		}
	}

	return CHIP_TYPE_UNKNOWN;
}

/* CHIP_TYPE_SIA81X9 */
static const uint32_t sia81x9_list[] = {
	CHIP_TYPE_SIA8159,
	CHIP_TYPE_SIA8159A,
	CHIP_TYPE_SIA8109
};

/* CHIP_TYPE_SIA8152X */
static const uint32_t sia8152x_list[] = {
	CHIP_TYPE_SIA8152S,	// first chip reg range should cover all other chips
	CHIP_TYPE_SIA8152
};

/* compatible chips should have same i2c address */
static const struct sipa_chip_compat sipa_compat_table[] = {
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

	for (i = 0; i < ARRAY_SIZE(sipa_compat_table); i++) {
		if (chip_type == sipa_compat_table[i].sub_type) {
			if (0 < sipa_compat_table[i].num)
				//pr_info("[ info][%s] %s: chip_type = %u \r\n",
				//	LOG_FLAG, __func__, sipa_compat_table[i].chips[0]);
				return sipa_compat_table[i].chips[0];
		}
	}

	return chip_type;
}

#ifdef DISTINGUISH_CHIP_TYPE
static int check_sipa_status(sipa_dev_t *si_pa)
{
	int ret = 0;

	if (0 == si_pa->disable_pin)
		sipa_resume(si_pa);

	if (1 == si_pa->en_dyn_id)
		ret = gpio_get_value(si_pa->id_pin);
	else
		ret = sipa_regmap_check_chip_id(
			si_pa->regmap, si_pa->channel_num, si_pa->chip_type);

	if (0 == si_pa->disable_pin)
	    sipa_suspend(si_pa);

	return ret;
}
#endif

void sipa_compatible_chips_adapt(
	sipa_dev_t *si_pa)
{
	int i = 0, j = 0;
	unsigned long flags;

	for (i = 0; i < ARRAY_SIZE(sipa_compat_table); i++) {
		if (si_pa->chip_type == sipa_compat_table[i].sub_type) {

			if (0 == si_pa->disable_pin) {
				spin_lock_irqsave(&si_pa->rst_lock, flags);
				gpio_set_value(si_pa->rst_pin, SIA81XX_ENABLE_LEVEL);
				mdelay(1);	/* wait chip power up, the time must be > 1ms */
				spin_unlock_irqrestore(&si_pa->rst_lock, flags);

				if (CHIP_TYPE_SIA81X9 == si_pa->chip_type)
					mdelay(39);	/* for sia8109 gain rising. */
			}

			for (j = 0; j < sipa_compat_table[i].num; j++) {
				if (NULL != sipa_compat_table[i].chips
					&& 0 == sipa_regmap_check_chip_id(si_pa->regmap,
					si_pa->channel_num, sipa_compat_table[i].chips[j])) {
					si_pa->chip_type = sipa_compat_table[i].chips[j];
					break;
				}
			}

			if (0 == si_pa->disable_pin) {
				spin_lock_irqsave(&si_pa->rst_lock, flags);
				/* power off chip */
				gpio_set_value(si_pa->rst_pin, SIA81XX_DISABLE_LEVEL);
				mdelay(1);	/* wait chip power off, the time must be > 1ms */
				spin_unlock_irqrestore(&si_pa->rst_lock, flags);
			}

			if (j >= sipa_compat_table[i].num)
				si_pa->chip_type = CHIP_TYPE_UNKNOWN;

			pr_info("[ info][%s] %s: chip_type = %u \r\n",
				LOG_FLAG, __func__, si_pa->chip_type);
			break;
		}
	}

	distinguish_chip_type(si_pa);
}

/********************************************************************
 * end - sipa driver common
 ********************************************************************/

/********************************************************************
 * i2c bus driver
 ********************************************************************/
int sipa_pending_actions(sipa_dev_t *si_pa)
{
	SIPA_CHIP_CFG chip_cfg;
	SIPA_EXTRA_CFG extra_cfg;

	if (NULL == si_pa)
		return -EINVAL;

	/* copy settings */
	if (NULL == sipa_param_read_chip_cfg(
		si_pa->channel_num, si_pa->chip_type, &chip_cfg))
		return -EFAULT;

	si_pa->en_dyn_ud_vdd = chip_cfg.en_dyn_ud_vdd;
	si_pa->en_dyn_ud_pvdd = chip_cfg.en_dyn_ud_pvdd;

	if (0 != sipa_param_read_extra_cfg(
		si_pa->channel_num, &extra_cfg))
		return -EFAULT;

	si_pa->timer_task_hdl = extra_cfg.timer_task_hdl;
	si_pa->dyn_ud_vdd_port = extra_cfg.dyn_ud_vdd_port;
	si_pa->spk_model_flag = extra_cfg.spk_model_flag;
	si_pa->en_spk_cal_dl = extra_cfg.en_spk_cal_dl;

	/* probe other sub module */ /* update info if it's already probed */
    if (1 == si_pa->en_dyn_ud_vdd || 1 == si_pa->en_dyn_ud_pvdd) {
		sipa_auto_set_vdd_probe(
			si_pa->timer_task_hdl,
			si_pa->chip_type,
			si_pa->channel_num,
			si_pa->regmap,
			si_pa->dyn_ud_vdd_port,
			SIPA_AUTO_VDD_EN_SET(si_pa->en_dyn_ud_vdd) |
			SIPA_AUTO_PVDD_EN_SET(si_pa->en_dyn_ud_pvdd));
	}

	if (1 == si_pa->en_spk_cal_dl) {
		sipa_cal_spk_update_probe(
			si_pa->timer_task_hdl,
			si_pa->dyn_ud_vdd_port,
			si_pa->channel_num);
	}
	/* end - probe other sub module */

	/* power down chip in any case when phone start up */
	sipa_suspend(si_pa);

	pr_info("[ info][%s] %s: sipa %d driver init done \r\n",
		LOG_FLAG, __func__, si_pa->channel_num);

	return 0;
}

int sipa_i2c_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	sipa_dev_t *si_pa = NULL;
	struct regmap *regmap = NULL;
	struct device_node	*sipa_of_node = NULL;
	const char *chip_type_name = NULL;
	unsigned int chip_type = CHIP_TYPE_UNKNOWN;
	int ret = 0;
	int addr_offset = 0;

#ifdef SIA91XX_TYPE
	struct snd_soc_dai_driver *dai = NULL;
#endif

	pr_info("[ info][%s] %s: i2c addr = 0x%02x \r\n",
		LOG_FLAG, __func__, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[  err][%s] %s: i2c_check_functionality return -ENODEV \r\n",
			LOG_FLAG, __func__);
		return -ENODEV;
	}

	sipa_of_node = of_parse_phandle(client->dev.of_node,
							"si,sipa-dev", 0);

	/* get chip type name */
	ret = of_property_read_string_index(sipa_of_node,
				"si,si_pa_type",
				0,
				&chip_type_name);
	if (0 != ret) {
		pr_err("[  err][%s] %s: get si,si_pa_type return %d !!! \r\n",
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

	ret = of_property_read_s32(client->dev.of_node, "si,sipa-addr_offset", &addr_offset);
	if (0 != ret) {
			pr_info("addr_offset default value!!!\r\n");
			addr_offset = 0;
	}

	client->addr += addr_offset;

	if (client->addr > 0x7f) {
		pr_err("[ err][%s] %s: client addr error!\r\n", LOG_FLAG, __func__);
		return -ENODEV;
	}

	si_pa = get_sipa_dev(&client->dev, sipa_of_node);
	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: get_sipa_dev error !!! \r\n",
			LOG_FLAG, __func__);
		return -ENODEV;
	}

	regmap = sipa_regmap_init(client, si_pa->channel_num,
		get_one_available_chip_type(chip_type));
	if (IS_ERR(regmap)) {
		pr_err("[  err][%s] %s: regmap_init_i2c error !!! \r\n",
			LOG_FLAG, __func__);
		return -ENODEV;
	}

	si_pa->regmap = regmap;

	/* save i2c client */
	si_pa->client = client;

	/* sava driver private data to the dev's driver data */
	dev_set_drvdata(&client->dev, si_pa);

	// for sia8101 stereo
	if (CHIP_TYPE_SIA8101 == si_pa->chip_type
			&& 0 == si_pa->channel_num)
		g_default_sia_dev = si_pa;

	sipa_compatible_chips_adapt(si_pa);

#ifdef SIA91XX_TYPE
	ret = sia91xx_detect_chip(si_pa);
	if (ret < 0) {
		pr_err("[  err][%s] %s: sia91xx detect fail !!! \r\n", LOG_FLAG, __func__);
		return ret;
	}

	dai = devm_kzalloc(&client->dev, sizeof(sia91xx_dai), GFP_KERNEL);
	if (NULL == dai)
		return -ENOMEM;

	memcpy(dai, sia91xx_dai, sizeof(sia91xx_dai));

	sia91xx_append_i2c_address(&client->dev,
				client,
				NULL,
				0,
				dai,
				ARRAY_SIZE(sia91xx_dai));

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
	ret = devm_snd_soc_register_component(&client->dev,
		&soc_component_dev_sipa, dai, ARRAY_SIZE(sia91xx_dai));
#else
	ret = snd_soc_register_codec(&client->dev,
		&soc_codec_dev_sipa, dai, ARRAY_SIZE(sia91xx_dai));
#endif

	if (ret < 0) {
		dev_err(&client->dev, "Failed to register sia9195xx: %d\n", ret);
		return ret;
	}
	pr_info("[info][%s] snd_soc_register_codec ret=%d!\n", __func__, ret);
#endif

	return 0;
}

int sipa_i2c_remove(
	struct i2c_client *client)
{
	int ret = 0;
	sipa_dev_t *si_pa = NULL;

	pr_info("[ info][%s] %s: remove \r\n", LOG_FLAG, __func__);

	si_pa = (sipa_dev_t *)dev_get_drvdata(&client->dev);
	if (NULL == si_pa)
		return 0;

	cancel_delayed_work_sync(&si_pa->interrupt_work);
	// cancel_delayed_work_sync(&si_pa->monitor_work);

	sipa_regmap_remove(si_pa);
	si_pa->regmap = NULL;

	si_pa->client = NULL;

	put_sipa_dev(si_pa);

	return ret;
}

#ifndef PLATFORM_TYPE_MTK
static const struct i2c_device_id si_sipa_i2c_id[] = {
	{ SIPA_I2C_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static const struct of_device_id si_sipa_i2c_match[] = {
	{.compatible = "si,sia81xx-i2c"},
	{.compatible = "si,sia8101-i2c"},
	{.compatible = "si,sia8109-i2c"},
	{.compatible = "si,sia8152-i2c"},
	{.compatible = "si,sia8152s-i2c"},
	{.compatible = "si,sia8159-i2c"},
	{.compatible = "si,sia81x9-i2c"},
	{.compatible = "si,sia8152x-i2c"},
	{.compatible = "si,sia91xx-i2c"},
	{},
};
#endif

static struct i2c_driver si_sipa_i2c_driver = {
	.driver = {
		.name = SIPA_I2C_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = si_sipa_i2c_match,
#endif
	},
	.probe		= sipa_i2c_probe,
	.remove		= sipa_i2c_remove,
	.id_table	= si_sipa_i2c_id,
};
#endif
/********************************************************************
 * end - i2c bus driver
 ********************************************************************/

/********************************************************************
 * sipa dev driver
 ********************************************************************/

/* power manage options */
static int sipa_pm_suspend(
	struct device *dev)
{
#if 0
	sipa_dev_t *si_pa = NULL;

	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);


	si_pa = (sipa_dev_t *)dev_get_drvdata(dev);

	return sipa_suspend(si_pa);
#else
	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	return 0;
#endif
}

static int sipa_pm_resume(struct device *dev)
{
#if 0
	sipa_dev_t *si_pa = NULL;

	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	si_pa = (sipa_dev_t *)dev_get_drvdata(dev);

	return sipa_resume(si_pa);
#else
	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	return 0;
#endif
}

static const struct dev_pm_ops si_sipa_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sipa_pm_suspend, sipa_pm_resume)
};

static int sipa_probe(struct platform_device *pdev)
{
	int ret = 0;
	sipa_dev_t *si_pa = NULL;
	char work_name[20];
	const char *chip_type_name = NULL;
	unsigned int chip_type = CHIP_TYPE_UNKNOWN;
	struct pinctrl *si_pa_pinctrl = NULL;
	struct pinctrl_state *pinctrl_state = NULL;
	int disable_pin = 0, rst_pin = 0, id_pin = 0, en_dyn_id = 0;

#ifdef SIA91XX_TYPE
	int irq_pin = 0;
	int mute_mode = 0;
	int en_irq_pin = 0;
#else
	int owi_pin = 0;
#endif
	pr_info("[ info][%s] %s: probe \r\n", LOG_FLAG, __func__);

	/* get chip type name */
	ret = of_property_read_string_index(pdev->dev.of_node,
				"si,si_pa_type",
				0,
				&chip_type_name);
	if (0 != ret) {
		pr_err("[  err][%s] %s: get si,si_pa_type return %d !!! \r\n",
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

	ret = of_property_read_u32(pdev->dev.of_node,
			"si,si_pa_disable_pin", &disable_pin);
	if (0 != ret) {
		pr_err("[  err][%s] %s: get si,si_pa_disable_pin return %d !!! \r\n",
			LOG_FLAG, __func__, ret);
		return -ENODEV;
	}

	si_pa = get_sipa_dev(&pdev->dev, pdev->dev.of_node);
	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: get_si_pa_dev error !!! \r\n",
			LOG_FLAG, __func__);
		ret = -ENODEV;
		goto out;
	}

	if (0 == disable_pin) {
		/* get reset gpio pin's pinctrl info */
		si_pa_pinctrl = devm_pinctrl_get(&pdev->dev);
		if (NULL == si_pa_pinctrl) {
			pr_err("[  err][%s] %s: NULL == pinctrl !!! \r\n",
				LOG_FLAG, __func__);
			return -ENODEV;
		}

		/* get owi gpio pin's specify pinctrl state */
		pinctrl_state = pinctrl_lookup_state(si_pa_pinctrl, "si_pa_gpio");
		if (NULL == pinctrl_state) {
			pr_err("[  err][%s] %s: NULL == pinctrl_state !!! \r\n",
				LOG_FLAG, __func__);
			ret = -ENODEV;
			goto out;
		}

		/* set this pinctrl state, make this pin works in the gpio mode */
		ret = pinctrl_select_state(si_pa_pinctrl, pinctrl_state);
		if (0 != ret) {
			pr_err("[  err][%s] %s: error pinctrl_select_state return %d \r\n",
				LOG_FLAG, __func__, ret);
			ret = -ENODEV;
			goto out;
		}

		/* get reset pin's sn */
		rst_pin = of_get_named_gpio(
			pdev->dev.of_node, "si,si_pa_reset", 0);
		if (rst_pin < 0) {
			pr_err("[  err][%s] %s: rst_pin < 0 !!! \r\n", LOG_FLAG, __func__);
			ret = -ENODEV;
			goto out;
		}

#ifdef SIA91XX_TYPE
		ret = of_property_read_u32(pdev->dev.of_node,
			"si,si_pa_mute_mode", &mute_mode);
		if (0 != ret) {
			mute_mode = MUTE_MODE_EXTERNAL;
		}

		ret = of_property_read_u32(pdev->dev.of_node,
				"en_irq_func", &en_irq_pin);
		if ((0 != ret) || (1 != en_irq_pin)) {
			si_pa->irq_pin = -1;
			pr_warn("[ warn] %s: No IRQ GPIO provided.!\r\n", __func__);
		} else {
			pinctrl_state = pinctrl_lookup_state(si_pa_pinctrl, "si_pa_irq");
			if (NULL == pinctrl_state) {
				pr_err("[  err][%s] %s: NULL == pinctrl_state !!! \r\n", LOG_FLAG, __func__);
				ret = -ENODEV;
				goto out;
			}

			ret = pinctrl_select_state(si_pa_pinctrl, pinctrl_state);
			if (0 != ret) {
				pr_err("[  err][%s] %s: error pinctrl_select_state return %d \r\n", LOG_FLAG, __func__, ret);
				ret = -ENODEV;
				goto out;
			}

			irq_pin = of_get_named_gpio(pdev->dev.of_node, "si,si_pa_irq", 0);
			if (irq_pin < 0) {
				pr_err("[  err][%s] %s: irq_pin < 0 !!! \r\n", LOG_FLAG, __func__);
				ret = -ENODEV;
				goto out;
			}
			si_pa->irq_pin = irq_pin;
		}
#else
		/* get owi pin's sn */
		owi_pin = of_get_named_gpio(pdev->dev.of_node, "si,si_pa_owi", 0);
		if (owi_pin < 0) {
			pr_err("[  err][%s] %s: owi_pin < 0 !!! \r\n", LOG_FLAG, __func__);
			ret = -ENODEV;
			goto out;
		}
#endif
		ret = of_property_read_u32(pdev->dev.of_node,
				"en_dynamic_id", &en_dyn_id);
		if ((0 != ret) || (1 != en_dyn_id)) {
			en_dyn_id = 0;
		}
		si_pa->en_dyn_id = (unsigned int)en_dyn_id;

		if (1 == si_pa->en_dyn_id) {
			pinctrl_state = pinctrl_lookup_state(si_pa_pinctrl, "si_pa_id");
			if (NULL == pinctrl_state) {
				pr_err("[  err][%s] %s: NULL == pinctrl_state !!! \r\n",
					LOG_FLAG, __func__);
				ret = -ENODEV;
				goto out;
			}

			/* set this pinctrl state, make this pin works in the gpio mode */
			ret = pinctrl_select_state(si_pa_pinctrl, pinctrl_state);
			if (0 != ret) {
				pr_err("[  err][%s] %s: error pinctrl_select_state return %d \r\n",
					LOG_FLAG, __func__, ret);
				ret = -ENODEV;
				goto out;
			}

			/* get id pin's sn */
			id_pin = of_get_named_gpio(pdev->dev.of_node, "si,si_pa_id", 0);
			if (id_pin < 0) {
				pr_err("[  err][%s] %s: id_pin < 0 !!! \r\n", LOG_FLAG, __func__);
				ret = -ENODEV;
				goto out;
			}

			si_pa->id_pin = id_pin;
			gpio_direction_input(si_pa->id_pin);
		}
	}

	si_pa->chip_type = chip_type;
	si_pa->rst_pin = rst_pin;
#ifdef SIA91XX_TYPE
	si_pa->mute_mode = mute_mode;
#else
	si_pa->owi_pin = owi_pin;
#endif
	si_pa->disable_pin = disable_pin;

	spin_lock_init(&si_pa->owi_lock);
	spin_lock_init(&si_pa->rst_lock);

	if (0 == disable_pin) {
		/* set rst pin's direction */
		gpio_direction_output(si_pa->rst_pin, SIA81XX_DISABLE_LEVEL);
		/* set owi pin's direction */
#ifdef SIA91XX_TYPE
		if (gpio_is_valid(si_pa->irq_pin))
			devm_gpio_request_one(&pdev->dev, si_pa->irq_pin, GPIOF_DIR_IN, "SIA91XX_INT");
#else
		gpio_direction_output(si_pa->owi_pin, OWI_POLARITY);
#endif
	}

#ifndef SIA91XX_TYPE
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 16, 28))
	ret = devm_snd_soc_register_component(&pdev->dev, &soc_component_dev_sipa, NULL, 0);
#else
	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_sipa, NULL, 0);
#endif
	if (0 != ret) {
		pr_err("[  err][%s] %s: snd_soc_register_codec fail \r\n",
			LOG_FLAG, __func__);
		goto put_dev_out;
	}
#endif

	ret = sipa_dev_init(si_pa, pdev->dev.of_node);
	if (0 != ret) {
		pr_err("[  err][%s] %s: si_pa_dev_init fail \r\n",
			LOG_FLAG, __func__);
		goto put_dev_out;
	}

	device_create_file(&pdev->dev, &dev_attr_sipa_cmd);
#ifdef SIA91XX_TYPE
	device_create_file(&pdev->dev, &dev_attr_sipa_spk_cal);
	device_create_file(&pdev->dev, &dev_attr_sipa_f0);
	device_create_file(&pdev->dev, &dev_attr_sipa_r0);
#endif
	/* save platform dev */
	si_pa->pdev = pdev;

	/* sava driver private data to the dev's driver data */
	dev_set_drvdata(&pdev->dev, si_pa);

	if (1 == si_pa->en_dyn_id)
		distinguish_chip_type(si_pa);

	snprintf(work_name, 20, "sia91xx_%d", si_pa->channel_num);
	si_pa->sia91xx_wq = create_singlethread_workqueue(work_name);
	if (!si_pa->sia91xx_wq)
		return -ENOMEM;
	/* load firmware */
	sipa_param_load_fw(&pdev->dev);

out:
	if (0 == disable_pin) {
		devm_pinctrl_put(si_pa_pinctrl);
	}

	return ret;

put_dev_out:
	if (0 == disable_pin) {
		devm_pinctrl_put(si_pa_pinctrl);
	}
	put_sipa_dev(si_pa);

	return ret;
}

static int sipa_remove(struct platform_device *pdev)
{
	int ret = 0;
	sipa_dev_t *si_pa = NULL;

	pr_info("[ info][%s] %s: remove \r\n", LOG_FLAG, __func__);

	si_pa = (sipa_dev_t *)dev_get_drvdata(&pdev->dev);
	if (NULL == si_pa)
		return 0;

	/* remove other sub module */
	sipa_auto_set_vdd_remove(
		si_pa->timer_task_hdl,
		si_pa->channel_num);
	/* end - remove other sub module */

	cancel_delayed_work_sync(&si_pa->fw_load_work);
	//cancel_delayed_work_sync(&si_pa->monitor_work);
	destroy_workqueue(si_pa->sia91xx_wq);
	sipa_param_release();

	ret = sipa_dev_remove(si_pa);
	if (0 != ret) {
		pr_err("[  err][%s] %s: si_pa_dev_remove return : %d \r\n",
			LOG_FLAG, __func__, ret);
	}

	si_pa->pdev = NULL;

	put_sipa_dev(si_pa);

	return 0;
}

static void sipa_shutdown(struct platform_device *pdev)
{
	sipa_dev_t *si_pa = NULL;

	pr_debug("[debug][%s] %s: running \r\n", LOG_FLAG, __func__);

	if (NULL == pdev)
		return ;

	si_pa = (sipa_dev_t *)dev_get_drvdata(&pdev->dev);

	sipa_suspend(si_pa);
}

#ifdef CONFIG_OF
static const struct of_device_id si_sipa_dt_match[] = {
	{ .compatible = "si,sia81xx" },
	{ .compatible = "si,sia8100" },
	{ .compatible = "si,sia8101" },
	{ .compatible = "si,sia8109" },
	{ .compatible = "si,sia8152" },
	{ .compatible = "si,sia8152s" },
	{ .compatible = "si,sia8159" },
	{ .compatible = "si,sia81x9" },
	{ .compatible = "si,sia8152x" },
	{ .compatible = "si,sia91xx" },
	{ }
};
MODULE_DEVICE_TABLE(of, si_sipa_dt_match);
#endif

static struct platform_driver si_sipa_dev_driver = {
	.probe  = sipa_probe,
	.remove = sipa_remove,
	.shutdown = sipa_shutdown,
	.driver = {
		.name  = SIPA_NAME,
		.owner = THIS_MODULE,
		.pm = &si_sipa_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = si_sipa_dt_match,
#endif
	},
};

/********************************************************************
 * end - sia81xx dev driver
 ********************************************************************/

static int __init sipa_pa_init(void)
{
	int ret = 0;

	pr_info("[ info][%s] %s: si_pa driver version : %s \r\n",
		LOG_FLAG, __func__, SIPA_DRIVER_VERSION);

	sipa_timer_task_init();

	//init algorithm lib communication interface
	if (NULL != tuning_if_opt.init) {
		tuning_if_opt.init();
	}

	//init auto set vdd funtion
	sipa_set_vdd_init();

	//init online tuning funtion
#ifdef SIPA_TUNING
	sipa_sock_init();
#endif

	ret = platform_driver_register(&si_sipa_dev_driver);
	if (ret) {
		pr_err("[  err][%s] %s: si_sipa_dev error, ret = %d !!! \r\n",
			LOG_FLAG, __func__, ret);
		return ret;
	}
#ifndef PLATFORM_TYPE_MTK
	ret = i2c_add_driver(&si_sipa_i2c_driver);
	if (ret) {
		pr_err("[  err][%s] %s: i2c_add_driver error, ret = %d !!! \r\n",
			LOG_FLAG, __func__, ret);
		return ret;
	}
#endif
	return 0;
}

static void __exit sipa_pa_exit(void)
{
	pr_info("[ info][%s] %s: running \r\n", LOG_FLAG, __func__);

	//exit online tuning funtion
#ifdef SIPA_TUNING
	sipa_sock_exit();
#endif

	//exit auto set vdd funtion
	sipa_set_vdd_exit();

	//exit algorithm lib communication interface
	if (NULL != tuning_if_opt.exit) {
		tuning_if_opt.exit();
	}

	sipa_timer_task_exit();
#ifndef PLATFORM_TYPE_MTK
	i2c_del_driver(&si_sipa_i2c_driver);
#endif
	platform_driver_unregister(&si_sipa_dev_driver);
}

module_init(sipa_pa_init);
module_exit(sipa_pa_exit);

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_AUTHOR("yun shi <yun.shi@si-in.com>");
MODULE_DESCRIPTION("SI-IN SIA81xx ASoC driver");
MODULE_LICENSE("GPL");
