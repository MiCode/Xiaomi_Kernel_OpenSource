/*
* Copyright (C) 2012 Texas Instruments
*
* License Terms: GNU General Public License v2
*
* Simple driver for Texas Instruments AW3644 LED driver chip
*
* Author: Tao, Jun <taojun@xiaomi.com>
*/

#ifndef __LINUX_AW3644_H
#define __LINUX_AW3644_H

#define AW3644_NAME "leds-aw3644"

#define AW3644_IOC_MAGIC 'M'
#define AW3644_PRIVATE_NUM 100
#define AW3644_LED_NUMS 2
const char cdev_name[AW3644_LED_NUMS][24] = {"softlight_torch_0","softlight_torch_1"};
struct aw3644_platform_data {
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

enum aw3644_event {
	GET_CHIP_ID_EVENT,
	SET_BRIGHTNESS_EVENT,
	GET_BRIGHTNESS_EVENT,
	MAX_NUM_EVENT,
};

typedef struct {
	unsigned int flood_enable;
	unsigned int flood_current;
	unsigned int flood_error;
} aw3644_info;

typedef struct {
	enum aw3644_event event;
	unsigned int data;
} aw3644_data;

#define FLOOD_IR_IOC_POWER_UP \
	_IO(AW3644_IOC_MAGIC, AW3644_PRIVATE_NUM + 1)
#define FLOOD_IR_IOC_POWER_DOWN \
	_IO(AW3644_IOC_MAGIC, AW3644_PRIVATE_NUM + 2)
#define FLOOD_IR_IOC_WRITE \
	_IOW(AW3644_IOC_MAGIC, AW3644_PRIVATE_NUM + 3, aw3644_data)
#define FLOOD_IR_IOC_READ \
	_IOWR(AW3644_IOC_MAGIC, AW3644_PRIVATE_NUM + 4, aw3644_data)
#define FLOOD_IR_IOC_READ_INFO \
	_IOWR(AW3644_IOC_MAGIC, AW3644_PRIVATE_NUM + 5, void*)


#endif /* __LINUX_AW3644_H */
