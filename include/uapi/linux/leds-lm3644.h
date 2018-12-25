/*
* Copyright (C) 2012 Texas Instruments
* Copyright (C) 2018 XiaoMi, Inc.
*
* License Terms: GNU General Public License v2
*
* Simple driver for Texas Instruments LM3644 LED driver chip
*
* Author: Tao, Jun <taojun@xiaomi.com>
*/

#ifndef __LINUX_LM3644_H
#define __LINUX_LM3644_H

#define LM3644_NAME "leds-lm3644"

#define LM3644_IOC_MAGIC 'M'
#define LM3644_PRIVATE_NUM 100


struct lm3644_platform_data {
	int tx_gpio;
	int torch_gpio;
	int hwen_gpio;
	int ito_detect_gpio;
	int ir_prot_time;
	unsigned int brightness;

	/* Simulative PWM settings */
	bool use_simulative_pwm;
	bool pass_mode;
	unsigned int pwm_period_us;
	unsigned int pwm_duty_us;
};

typedef struct {
	int ito_event;
} flood_report_data;

enum lm3644_event {
	GET_CHIP_ID_EVENT,
	SET_BRIGHTNESS_EVENT,
	GET_BRIGHTNESS_EVENT,
	MAX_NUM_EVENT,
};

typedef struct {
	unsigned int flood_enable;
	unsigned int flood_current;
	unsigned int flood_error;
} lm3644_info;

typedef struct {
	enum lm3644_event event;
	unsigned int data;
} lm3644_data;

#define FLOOD_IR_IOC_POWER_UP \
	_IO(LM3644_IOC_MAGIC, LM3644_PRIVATE_NUM + 1)
#define FLOOD_IR_IOC_POWER_DOWN \
	_IO(LM3644_IOC_MAGIC, LM3644_PRIVATE_NUM + 2)
#define FLOOD_IR_IOC_WRITE \
	_IOW(LM3644_IOC_MAGIC, LM3644_PRIVATE_NUM + 3, lm3644_data)
#define FLOOD_IR_IOC_READ \
	_IOWR(LM3644_IOC_MAGIC, LM3644_PRIVATE_NUM + 4, lm3644_data)
#define FLOOD_IR_IOC_READ_INFO \
	_IOWR(LM3644_IOC_MAGIC, LM3644_PRIVATE_NUM + 5, void*)


#endif /* __LINUX_LM3644_H */
