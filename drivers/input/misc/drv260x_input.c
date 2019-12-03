/*
 * DRV260X haptics driver family
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * Copyright:   (C) 2014 Texas Instruments, Inc.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME "(as input_haptics): " fmt
#define DEBUG

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/firmware.h>
#include <dt-bindings/input/ti-drv260x.h>
#include <linux/platform_data/drv260x-pdata.h>

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/debugfs.h>
#include <linux/miscdevice.h>
#include <linux/syscalls.h>
#include <linux/power_supply.h>

#include <linux/err.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/pwm.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#define CUSTOM_DATA_LEN     (3)

#define DRV260X_STATUS		0x0
#define DRV260X_MODE		0x1
#define DRV260X_RT_PB_IN	0x2
#define DRV260X_LIB_SEL		0x3
#define DRV260X_WV_SEQ_1	0x4
#define DRV260X_WV_SEQ_2	0x5
#define DRV260X_WV_SEQ_3	0x6
#define DRV260X_WV_SEQ_4	0x7
#define DRV260X_WV_SEQ_5	0x8
#define DRV260X_WV_SEQ_6	0x9
#define DRV260X_WV_SEQ_7	0xa
#define DRV260X_WV_SEQ_8	0xb
#define DRV260X_GO				0xc
#define DRV260X_OVERDRIVE_OFF	0xd
#define DRV260X_SUSTAIN_P_OFF	0xe
#define DRV260X_SUSTAIN_N_OFF	0xf
#define DRV260X_BRAKE_OFF		0x10
#define DRV260X_A_TO_V_CTRL		0x11
#define DRV260X_A_TO_V_MIN_INPUT	0x12
#define DRV260X_A_TO_V_MAX_INPUT	0x13
#define DRV260X_A_TO_V_MIN_OUT	0x14
#define DRV260X_A_TO_V_MAX_OUT	0x15
#define DRV260X_RATED_VOLT		0x16
#define DRV260X_OD_CLAMP_VOLT	0x17
#define DRV260X_CAL_COMP		0x18
#define DRV260X_CAL_BACK_EMF	0x19
#define DRV260X_FEEDBACK_CTRL	0x1a
#define DRV260X_CTRL1			0x1b
#define DRV260X_CTRL2			0x1c
#define DRV260X_CTRL3			0x1d
#define DRV260X_CTRL4			0x1e
#define DRV260X_CTRL5			0x1f
#define DRV260X_LRA_LOOP_PERIOD	0x20
#define DRV260X_VBAT_MON		0x21
#define DRV260X_LRA_RES_PERIOD	0x22
#define	DRV2604_REG_RAM_ADDR_UPPER_BYTE	0xfd
#define	DRV2604_REG_RAM_ADDR_LOWER_BYTE	0xfe
#define	DRV2604_REG_RAM_DATA			0xff

#define DRV260X_MAX_REG			0xff	//0x23
#define DRV260X_GO_BIT				0x01

/* Library Selection */
#define DRV260X_LIB_SEL_MASK		0x07
#define DRV260X_LIB_SEL_RAM			0x0
#define DRV260X_LIB_SEL_OD			0x1
#define DRV260X_LIB_SEL_40_60		0x2
#define DRV260X_LIB_SEL_60_80		0x3
#define DRV260X_LIB_SEL_100_140		0x4
#define DRV260X_LIB_SEL_140_PLUS	0x5

#define DRV260X_LIB_SEL_HIZ_MASK	0x10
#define DRV260X_LIB_SEL_HIZ_EN		0x01
#define DRV260X_LIB_SEL_HIZ_DIS		0

/* Mode register */
#define DRV260X_STANDBY				(1 << 6)
#define DRV260X_STANDBY_MASK		0x40
#define DRV260X_INTERNAL_TRIGGER	0x00
#define DRV260X_EXT_TRIGGER_EDGE	0x01
#define DRV260X_EXT_TRIGGER_LEVEL	0x02
#define DRV260X_PWM_ANALOG_IN		0x03
#define DRV260X_AUDIOHAPTIC			0x04
#define DRV260X_RT_PLAYBACK			0x05
#define DRV260X_DIAGNOSTICS			0x06
#define DRV260X_AUTO_CAL			0x07

/* Audio to Haptics Control */
#define DRV260X_AUDIO_HAPTICS_PEAK_10MS		(0 << 2)
#define DRV260X_AUDIO_HAPTICS_PEAK_20MS		(1 << 2)
#define DRV260X_AUDIO_HAPTICS_PEAK_30MS		(2 << 2)
#define DRV260X_AUDIO_HAPTICS_PEAK_40MS		(3 << 2)

#define DRV260X_AUDIO_HAPTICS_FILTER_100HZ	0x00
#define DRV260X_AUDIO_HAPTICS_FILTER_125HZ	0x01
#define DRV260X_AUDIO_HAPTICS_FILTER_150HZ	0x02
#define DRV260X_AUDIO_HAPTICS_FILTER_200HZ	0x03

/* Min/Max Input/Output Voltages */
#define DRV260X_AUDIO_HAPTICS_MIN_IN_VOLT	0x19
#define DRV260X_AUDIO_HAPTICS_MAX_IN_VOLT	0x64
#define DRV260X_AUDIO_HAPTICS_MIN_OUT_VOLT	0x19
#define DRV260X_AUDIO_HAPTICS_MAX_OUT_VOLT	0xFF

/* Feedback register */
#define DRV260X_FB_REG_ERM_MODE			0x7f
#define DRV260X_FB_REG_LRA_MODE			(1 << 7)

#define DRV260X_BRAKE_FACTOR_MASK	0x1f
#define DRV260X_BRAKE_FACTOR_2X		(1 << 0)
#define DRV260X_BRAKE_FACTOR_3X		(2 << 4)
#define DRV260X_BRAKE_FACTOR_4X		(3 << 4)
#define DRV260X_BRAKE_FACTOR_6X		(4 << 4)
#define DRV260X_BRAKE_FACTOR_8X		(5 << 4)
#define DRV260X_BRAKE_FACTOR_16		(6 << 4)
#define DRV260X_BRAKE_FACTOR_DIS	(7 << 4)

#define DRV260X_LOOP_GAIN_LOW		0xf3
#define DRV260X_LOOP_GAIN_MED		(1 << 2)
#define DRV260X_LOOP_GAIN_HIGH		(2 << 2)
#define DRV260X_LOOP_GAIN_VERY_HIGH	(3 << 2)

#define DRV260X_BEMF_GAIN_0			0xfc
#define DRV260X_BEMF_GAIN_1		(1 << 0)
#define DRV260X_BEMF_GAIN_2		(2 << 0)
#define DRV260X_BEMF_GAIN_3		(3 << 0)

/* Control 1 register */
#define DRV260X_AC_CPLE_EN			(1 << 5)
#define DRV260X_STARTUP_BOOST		(1 << 7)

/* Control 2 register */

#define DRV260X_IDISS_TIME_45		0
#define DRV260X_IDISS_TIME_75		(1 << 0)
#define DRV260X_IDISS_TIME_150		(1 << 1)
#define DRV260X_IDISS_TIME_225		0x03

#define DRV260X_BLANK_TIME_45	(0 << 2)
#define DRV260X_BLANK_TIME_75	(1 << 2)
#define DRV260X_BLANK_TIME_150	(2 << 2)
#define DRV260X_BLANK_TIME_225	(3 << 2)

#define DRV260X_SAMP_TIME_150	(0 << 4)
#define DRV260X_SAMP_TIME_200	(1 << 4)
#define DRV260X_SAMP_TIME_250	(2 << 4)
#define DRV260X_SAMP_TIME_300	(3 << 4)

#define DRV260X_BRAKE_STABILIZER	(1 << 6)
#define DRV260X_UNIDIR_IN			(0 << 7)
#define DRV260X_BIDIR_IN			(1 << 7)

/* Control 3 Register */
#define DRV260X_LRA_OPEN_LOOP		(1 << 0)
#define DRV260X_ANANLOG_IN			(1 << 1)
#define DRV260X_LRA_DRV_MODE		(1 << 2)
#define DRV260X_RTP_UNSIGNED_DATA	(1 << 3)
#define DRV260X_SUPPLY_COMP_DIS		(1 << 4)
#define DRV260X_ERM_OPEN_LOOP		(1 << 5)
#define DRV260X_NG_THRESH_0			(0 << 6)
#define DRV260X_NG_THRESH_2			(1 << 6)
#define DRV260X_NG_THRESH_4			(2 << 6)
#define DRV260X_NG_THRESH_8			(3 << 6)

/* Control 4 Register */
#define DRV260X_AUTOCAL_TIME_150MS		(0 << 4)
#define DRV260X_AUTOCAL_TIME_250MS		(1 << 4)
#define DRV260X_AUTOCAL_TIME_500MS		(2 << 4)
#define DRV260X_AUTOCAL_TIME_1000MS		(3 << 4)

#define DRV2604_MAGIC	0x2604
#define	WORK_IDLE					0x00
#define WORK_RTP			      	0x06
#define WORK_CALIBRATION	      	0x07
#define WORK_VIBRATOR		      	0x08
#define	WORK_PATTERN_RTP_ON			0x09
#define WORK_PATTERN_RTP_OFF      	0x0a
#define WORK_SEQ_RTP_ON		      	0x0b
#define WORK_SEQ_RTP_OFF    	  	0x0c
#define WORK_SEQ_PLAYBACK    	  	0x0d

#define DEV_IDLE	                0	// default
#define DEV_STANDBY					1
#define DEV_READY					2
#define FF_EFFECT_COUNT_MAX			32
#define DRV260X_EFFECT_COUNT_MAX	(0x7f+1)
/*

** Mode

*/

#define MODE_REG            0x01
#define MODE_STANDBY_MASK           0x40
#define MODE_STANDBY        0x40
#define MODE_RESET                  0x80
#define DRV2604_MODE_MASK           0x07
#define MODE_INTERNAL_TRIGGER       0
#define MODE_EXTERNAL_TRIGGER_EDGE  1
#define MODE_EXTERNAL_TRIGGER_LEVEL 2
#define MODE_PWM_OR_ANALOG_INPUT    3
#define MODE_AUDIOHAPTIC            4
#define MODE_REAL_TIME_PLAYBACK     5
#define MODE_DIAGNOSTICS            6
#define AUTO_CALIBRATION            7

#define DRV260X_HAPTIC_ACTIVATE_RTP_MODE  WORK_VIBRATOR
#define	DRV260X_HAPTIC_ACTIVATE_RAM_MODE  WORK_SEQ_PLAYBACK

#define DRV260X_EFFECT_CLICK	0
#define DRV260X_EFFECT_DOUBLECLICK	1
#define DRV260X_EFFECT_TICK	2
#define DRV260X_EFFECT_HEAVY_CLICK	5

#define DRV260X_LIGHT_CLICK_NUMBER   1
#define DRV260X_MEDIUM_CLICK_NUMBER   2
#define DRV260X_STRONG_CLICK_NUMBER   3

#define DRV260X_LIGHT_DCLICK_NUMBER   4
#define DRV260X_MEDIUM_DCLICK_NUMBER 	5
#define DRV260X_STRONG_DCLICK_NUMBER 	6

#define DRV260X_LIGHT_TICK_NUMBER       7
#define DRV260X_MEDIUM_TICK_NUMBER      8
#define DRV260X_STRONG_TICK_NUMBER      9

#define DRV260X_LIGHT_HCLICK_NUMBER     10
#define DRV260X_MEDIUM_HCLICK_NUMBER    11
#define DRV260X_STRONG_HCLICK_NUMBER    12

struct drv2604_fw_header {
	int fw_magic;
	int fw_size;
	int fw_date;
	int fw_chksum;
	int fw_effCount;
};

struct android_hal_stub {
	struct workqueue_struct *wq;
	struct work_struct haptics_play_work;
	struct work_struct haptics_stop_work;
	struct mutex lock;	/* stop/play */
	bool haptics_playing;
	struct hrtimer stop_timer;
	u32 play_time_ms;
	int duration;
};

/**
 * struct drv260x_data -
 * @input_dev - Pointer to the input device
 * @client - Pointer to the I2C client
 * @regmap - Register map of the device
 * @work - Work item used to off load the enable/disable of the vibration
 * @enable_gpio - Pointer to the gpio used for enable/disabling
 * @regulator - Pointer to the regulator for the IC
 * @magnitude - Magnitude of the vibration event
 * @mode - The operating mode of the IC (LRA_NO_CAL, ERM or LRA)
 * @library - The vibration library to be used
 * @rated_voltage - The rated_voltage of the actuator
 * @overdriver_voltage - The over drive voltage of the actuator
**/
struct drv260x_data {
	struct input_dev *input_dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct work_struct work;
	struct gpio_desc *enable_gpio;
	struct regulator *regulator;
	u32 magnitude;
	u32 mode;
	u32 library;
	int rated_voltage;
	int overdrive_voltage;
	volatile char work_mode;
	char dev_mode;
	struct android_hal_stub android;
	struct drv2604_fw_header fw_header;
	u32 *pEffDuration;
	int state;
	int effect_type;
	int effect_id;
	int effects_count;
	int play_effect_speed;
	char amp;
};

int enable_pin;
int pwm_pin;

static const struct reg_default drv260x_reg_defs[] = {
	{DRV260X_STATUS, 0xe0},
	{DRV260X_MODE, 0x40},
	{DRV260X_RT_PB_IN, 0x00},
	{DRV260X_LIB_SEL, 0x00},
	{DRV260X_WV_SEQ_1, 0x01},
	{DRV260X_WV_SEQ_2, 0x00},
	{DRV260X_WV_SEQ_3, 0x00},
	{DRV260X_WV_SEQ_4, 0x00},
	{DRV260X_WV_SEQ_5, 0x00},
	{DRV260X_WV_SEQ_6, 0x00},
	{DRV260X_WV_SEQ_7, 0x00},
	{DRV260X_WV_SEQ_8, 0x00},
	{DRV260X_GO, 0x00},
	{DRV260X_OVERDRIVE_OFF, 0x00},
	{DRV260X_SUSTAIN_P_OFF, 0x00},
	{DRV260X_SUSTAIN_N_OFF, 0x00},
	{DRV260X_BRAKE_OFF, 0x00},
	{DRV260X_A_TO_V_CTRL, 0x05},
	{DRV260X_A_TO_V_MIN_INPUT, 0x19},
	{DRV260X_A_TO_V_MAX_INPUT, 0xff},
	{DRV260X_A_TO_V_MIN_OUT, 0x19},
	{DRV260X_A_TO_V_MAX_OUT, 0xff},
	{DRV260X_RATED_VOLT, 0x3e},
	{DRV260X_OD_CLAMP_VOLT, 0x8c},
	{DRV260X_CAL_COMP, 0x0c},
	{DRV260X_CAL_BACK_EMF, 0x6c},
	{DRV260X_FEEDBACK_CTRL, 0x36},
	{DRV260X_CTRL1, 0x93},
	{DRV260X_CTRL2, 0xfa},
	{DRV260X_CTRL3, 0xa0},
	{DRV260X_CTRL4, 0x20},
	{DRV260X_CTRL5, 0x80},
	{DRV260X_LRA_LOOP_PERIOD, 0x31},
	{DRV260X_VBAT_MON, 0x00},
	{DRV260X_LRA_RES_PERIOD, 0x00},

	{0xfd, 0x00},
	{0xfe, 0x00},
	{0xff, 0x00},
};

#define DRV260X_DEF_RATED_VOLT		0x90
#define DRV260X_DEF_OD_CLAMP_VOLT	0x90

static void
drv2604_change_mode(struct drv260x_data *pDrv2604data, char work_mode,
		    char dev_mode);
/**
 * Rated and Overdriver Voltages:
 * Calculated using the formula r = v * 255 / 5.6
 * where r is what will be written to the register
 * and v is the rated or overdriver voltage of the actuator
 **/
static int drv260x_calculate_voltage(unsigned int voltage)
{
	return (voltage * 255 / 5600);
}

static void drv260x_worker(struct work_struct *work)
{
	struct drv260x_data *haptics =
	    container_of(work, struct drv260x_data, work);
	int error;

	gpio_direction_output(enable_pin, 1);
	/* Data sheet says to wait 250us before trying to communicate */
	udelay(250);

	dev_err(&haptics->client->dev, "drv260x_worker, start\n");

	error =
	    regmap_write(haptics->regmap, DRV260X_MODE, DRV260X_RT_PLAYBACK);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write set mode: %d\n", error);
	} else {
		error = regmap_write(haptics->regmap,
				     DRV260X_RT_PB_IN, haptics->magnitude);
		if (error)
			dev_err(&haptics->client->dev,
				"Failed to set magnitude: %d\n", error);
	}
}

static void drv260x_close(struct input_dev *input)
{
	struct drv260x_data *haptics = input_get_drvdata(input);
	int error;

	cancel_work_sync(&haptics->work);

	error = regmap_write(haptics->regmap, DRV260X_MODE, DRV260X_STANDBY);
	if (error)
		dev_err(&haptics->client->dev,
			"Failed to enter standby mode: %d\n", error);
}

static const struct reg_sequence drv260x_lra_cal_regs[] = {
	{DRV260X_MODE, DRV260X_AUTO_CAL},
	{DRV260X_CTRL3, DRV260X_NG_THRESH_2},
	{DRV260X_FEEDBACK_CTRL, DRV260X_FB_REG_LRA_MODE |
	 DRV260X_BRAKE_FACTOR_4X | DRV260X_LOOP_GAIN_HIGH},
};

static const struct reg_sequence drv260x_lra_init_regs[] = {

	{0x01, 0x00},		//init data copy from DemoBoard.
	{0x17, 0xA4},
	{0x1D, 0x80},
	{0x00, 0x00},
	{0x02, 0x00},
	{0x19, 0xF2},
	{0x1A, 0xe7},		//16x brake

};

static const struct reg_sequence drv260x_erm_cal_regs[] = {
	{DRV260X_MODE, DRV260X_AUTO_CAL},
	{DRV260X_A_TO_V_MIN_INPUT, DRV260X_AUDIO_HAPTICS_MIN_IN_VOLT},
	{DRV260X_A_TO_V_MAX_INPUT, DRV260X_AUDIO_HAPTICS_MAX_IN_VOLT},
	{DRV260X_A_TO_V_MIN_OUT, DRV260X_AUDIO_HAPTICS_MIN_OUT_VOLT},
	{DRV260X_A_TO_V_MAX_OUT, DRV260X_AUDIO_HAPTICS_MAX_OUT_VOLT},
	{DRV260X_FEEDBACK_CTRL, DRV260X_BRAKE_FACTOR_3X |
	 DRV260X_LOOP_GAIN_MED | DRV260X_BEMF_GAIN_2},
	{DRV260X_CTRL1, DRV260X_STARTUP_BOOST},
	{DRV260X_CTRL2, DRV260X_SAMP_TIME_250 | DRV260X_BLANK_TIME_75 |
	 DRV260X_IDISS_TIME_75},
	{DRV260X_CTRL3, DRV260X_NG_THRESH_2 | DRV260X_ERM_OPEN_LOOP},
	{DRV260X_CTRL4, DRV260X_AUTOCAL_TIME_500MS},
};

static int drv260x_init(struct drv260x_data *haptics)
{
	int error;
	unsigned int cal_buf;

	pr_err("drv260x_init start...\n");

	error = regmap_read(haptics->regmap, 0x01, &cal_buf);

	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to read  register[0x01]: %d\n", error);
		return error;
	}

	pr_err("Get drv260x status reg[0x01]=%#x\n", cal_buf);
	if ((cal_buf >> 6) & 0x01)
		pr_err("drv260x Suspend...\n");
	else
		pr_err("drv260x Activate...\n");

	//error = regmap_write(haptics->regmap,
	//                   DRV260X_RATED_VOLT, haptics->rated_voltage);
	error = regmap_write(haptics->regmap, DRV260X_RATED_VOLT, 0x48);	// Max power 1.8v to motor
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write DRV260X_RATED_VOLT register: %d\n",
			error);
		return error;
	}
	//error = regmap_write(haptics->regmap,
	//                   DRV260X_OD_CLAMP_VOLT, haptics->overdrive_voltage);
	error = regmap_write(haptics->regmap, DRV260X_OD_CLAMP_VOLT, 0x96);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write DRV260X_OD_CLAMP_VOLT register: %d\n",
			error);
		return error;
	}
	pr_err("drv260x, haptics mode=%d\n", haptics->mode);

	switch (haptics->mode) {
	case DRV260X_LRA_MODE:

		error = regmap_register_patch(haptics->regmap,
					      drv260x_lra_cal_regs,
					      ARRAY_SIZE(drv260x_lra_cal_regs));

		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to write LRA calibration registers: %d\n",
				error);
			return error;
		}

		break;

	case DRV260X_ERM_MODE:
		error = regmap_register_patch(haptics->regmap,
					      drv260x_erm_cal_regs,
					      ARRAY_SIZE(drv260x_erm_cal_regs));
		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to write ERM calibration registers: %d\n",
				error);
			return error;
		}

		error = regmap_update_bits(haptics->regmap, DRV260X_LIB_SEL,
					   DRV260X_LIB_SEL_MASK,
					   haptics->library);
		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to write DRV260X_LIB_SEL register: %d\n",
				error);
			return error;
		}

		break;

	default:
		error = regmap_register_patch(haptics->regmap,
					      drv260x_lra_init_regs,
					      ARRAY_SIZE
					      (drv260x_lra_init_regs));
		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to write LRA init registers: %d\n",
				error);
			return error;
		}

		error = regmap_update_bits(haptics->regmap, DRV260X_LIB_SEL,
					   DRV260X_LIB_SEL_MASK,
					   haptics->library);
		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to write DRV260X_LIB_SEL register: %d\n",
				error);
			return error;
		}

		error = regmap_read(haptics->regmap, DRV260X_CTRL5, &cal_buf);
		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to write DRV260X_LIB_SEL register: %d\n",
				error);
			return error;
		}

		haptics->play_effect_speed = ((cal_buf & 0x10) ? 1 : 5);
		pr_err("drv260x play_effect_speed %d\n",
		       haptics->play_effect_speed);
		/* No need to set GO bit here */
		return 0;
	}

	error = regmap_write(haptics->regmap, DRV260X_GO, DRV260X_GO_BIT);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write GO register: %d\n", error);
		return error;
	}

	do {
		error = regmap_read(haptics->regmap, DRV260X_GO, &cal_buf);
		if (error) {
			dev_err(&haptics->client->dev,
				"Failed to read GO register: %d\n", error);
			return error;
		}
	} while (cal_buf == DRV260X_GO_BIT);

	error = regmap_read(haptics->regmap, 0x00, &cal_buf);
	//pr_err("drv260x_init end,  device_id=%#x\n", cal_buf);

	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to read device_id register: %d\n", error);
		return error;
	}

	return 0;
}

static const struct regmap_config drv260x_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = DRV260X_MAX_REG,
	.reg_defaults = drv260x_reg_defs,
	.num_reg_defaults = ARRAY_SIZE(drv260x_reg_defs),
	.cache_type = REGCACHE_NONE,
};

#ifdef CONFIG_OF
static int drv260x_parse_dt(struct device *dev, struct drv260x_data *haptics)
{
	struct device_node *np = dev->of_node;
	unsigned int voltage;
	int error;

	error = of_property_read_u32(np, "mode", &haptics->mode);
	if (error) {
		dev_err(dev, "%s: No entry for mode\n", __func__);
		return error;
	}

	error = of_property_read_u32(np, "library-sel", &haptics->library);
	if (error) {
		dev_err(dev, "%s: No entry for library selection\n", __func__);
		return error;
	}

	error = of_property_read_u32(np, "vib-rated-mv", &voltage);
	if (!error)
		haptics->rated_voltage = drv260x_calculate_voltage(voltage);

	error = of_property_read_u32(np, "vib-overdrive-mv", &voltage);
	if (!error)
		haptics->overdrive_voltage = drv260x_calculate_voltage(voltage);

	enable_pin = of_get_named_gpio(np, "drv260x,enable-gpio", 0);
	pwm_pin = of_get_named_gpio(np, "drv260x,pwm-gpio", 0);
	return 0;
}
#else
static inline int
drv260x_parse_dt(struct device *dev, struct drv260x_data *haptics)
{
	dev_err(dev, "no platform data defined\n");

	return -EINVAL;
}
#endif

static int drv260x_haptics_stop(struct android_hal_stub *android)
{
	struct drv260x_data *haptics =
	    container_of(android, struct drv260x_data, android);
	int error = 0;

	pr_err("drv260x_haptics_stop motor\n");
	error = regmap_write(haptics->regmap, DRV260X_RT_PB_IN, 0x00);
	error = regmap_write(haptics->regmap, DRV260X_GO, 0x00);
	error = regmap_write(haptics->regmap, MODE_REG, MODE_STANDBY);

	if (error) {
		pr_err("drv260x_haptics_stop write fail.\n");
	}

	return 0;
}

#define  DRV260X_BRAKE_WAVEFORM_NUMBER 13

static int android_haptics_stop_playing(struct android_hal_stub *android)
{
	struct drv260x_data *haptics =
	    container_of(android, struct drv260x_data, android);
	int error = 0;

	pr_err("drv260x timer stop  playing...\n");

	error = regmap_write(haptics->regmap, DRV260X_RT_PB_IN, 0x00);
	error = regmap_write(haptics->regmap, DRV260X_GO, 0x00);
	error = regmap_write(haptics->regmap, MODE_REG, MODE_STANDBY);

	if (error) {
		pr_err("android_haptics_timer write fail.\n");
	}
	//if (android->haptics_playing) {
	//      hrtimer_cancel(&android->stop_timer);
	//}
	//android->haptics_playing = false;

	return 0;
}

static void android_haptics_stop(struct work_struct *work)
{
	struct android_hal_stub *android =
	    container_of(work, struct android_hal_stub, haptics_stop_work);

	mutex_lock(&android->lock);

	android_haptics_stop_playing(android);

	mutex_unlock(&android->lock);
}

static const struct reg_sequence drv260x_lra_play3_regs[] = {
	{0xfd, 0x00},
	{0xfe, 0x00},		//address
	{0xff, 0x00},
	{0xff, 0x00},
	{0xff, 0x04},
	{0xff, 0xe2},
	{0xff, 0xa0},
	{0xff, 0x90},

};

static void android_haptics_play(struct work_struct *work)
{
	struct android_hal_stub *android =
	    container_of(work, struct android_hal_stub, haptics_play_work);
	struct drv260x_data *haptics =
	    container_of(android, struct drv260x_data, android);
	int error;
	int waveform_select = 0;
	unsigned char value = 0;

	pr_err("android_haptics_play start...\n");

	mutex_lock(&android->lock);

	//android_haptics_stop_playing(android);
	drv260x_haptics_stop(android);

	pr_err("haptics_play effect_type=%d, work_mode=%d, state=%d \n",
	       haptics->effect_type, haptics->work_mode, haptics->state);

	if (haptics->state) {

		if (haptics->effect_type == FF_PERIODIC &&
		    haptics->work_mode == DRV260X_HAPTIC_ACTIVATE_RAM_MODE) {
			//pr_err("Start to play effect vibrating.\n");

			error =
			    regmap_write(haptics->regmap, MODE_REG,
					 MODE_INTERNAL_TRIGGER);
			udelay(1000);

			switch (haptics->effect_id) {
			case DRV260X_EFFECT_CLICK:
				waveform_select = 1;
				if (haptics->magnitude == 0x3FFF) {
					waveform_select = 1;
					pr_err("drv260x light click.\n");
				} else if (haptics->magnitude == 0x5FFF) {
					waveform_select = 2;
					pr_err("drv260x medium click.\n");
				} else if (haptics->magnitude == 0x7FFF) {
					waveform_select = 3;
					pr_err("drv260x strong click.\n");
				}
				break;

			case DRV260X_EFFECT_DOUBLECLICK:
				waveform_select = 4;
				if (haptics->magnitude == 0x3FFF) {
					waveform_select = 4;
					pr_err("drv260x light dclick.\n");
				} else if (haptics->magnitude == 0x5FFF) {
					waveform_select = 5;
					pr_err("drv260x medium dclick.\n");
				} else if (haptics->magnitude == 0x7FFF) {
					waveform_select = 6;
					pr_err("drv260x strong dclick.\n");
				}
				break;
			case DRV260X_EFFECT_TICK:
				waveform_select = 7;
				if (haptics->magnitude == 0x3FFF) {
					waveform_select = 7;
					pr_err("drv260x light tick.\n");
				} else if (haptics->magnitude == 0x5FFF) {
					waveform_select = 8;
					pr_err("drv260x medium tick.\n");
				} else if (haptics->magnitude == 0x7FFF) {
					waveform_select = 9;
					pr_err("drv260x strong tick.\n");
				}

				break;
			case DRV260X_EFFECT_HEAVY_CLICK:
				waveform_select = 10;
				if (haptics->magnitude == 0x3FFF) {
					waveform_select = 10;
					pr_err("drv260x light hclick.\n");
				} else if (haptics->magnitude == 0x5FFF) {
					waveform_select = 11;
					pr_err("drv260x medium hclick.\n");
				} else if (haptics->magnitude == 0x7FFF) {
					waveform_select = 12;
					pr_err("drv260x strong hclick.\n");
				}

				break;

			default:
				waveform_select = 1;
				break;
			}

			pr_err("android_haptics_play waveform_select= %d\n",
			       waveform_select);
			/* TODO write effect ID to sequencer registers DRV260X_WV_SEQ_1 */

			if (haptics->effect_id == DRV260X_EFFECT_DOUBLECLICK) {

				/*TODO select waveform by amp */
				error = regmap_write(haptics->regmap, DRV260X_WV_SEQ_1, waveform_select);	//register[0x04] playback from index_1

				error = regmap_write(haptics->regmap, DRV260X_WV_SEQ_2, 0x9e);	//300ms

				error =
				    regmap_write(haptics->regmap,
						 DRV260X_WV_SEQ_3,
						 waveform_select);
				error =
				    regmap_write(haptics->regmap,
						 DRV260X_WV_SEQ_4, 0);

				android->duration =
				    haptics->pEffDuration[waveform_select -
							  1] * 5 + 340;
			} else {
				error = regmap_write(haptics->regmap, DRV260X_WV_SEQ_1, waveform_select);	//register[0x04] playback from index_1
				error = regmap_write(haptics->regmap, DRV260X_WV_SEQ_2, 0);	//waveform end signal
				android->duration =
				    haptics->pEffDuration[waveform_select -
							  1] * 5;
			}

			regmap_write(haptics->regmap, DRV260X_GO, 1);

			pr_err("drv260x play effect vibrating. duration= %d\n",
			       android->duration);
		} else if (haptics->effect_type == FF_CONSTANT
			   && haptics->work_mode ==
			   DRV260X_HAPTIC_ACTIVATE_RTP_MODE) {

			pr_err("drv260x play RTP vibrating. duration= %d\n",
			       android->duration);
			regmap_write(haptics->regmap, MODE_REG,
				     MODE_REAL_TIME_PLAYBACK);

			udelay(1000);

			value = 0x3F;

			error = regmap_write(haptics->regmap, 0x02, value);
			if (error) {
				pr_err("Failed to start RTP vibrating: %d\n",
				       error);
				goto error_exit;
			}

			pr_err
			    ("Start to play RTP vibrating. duration= %d, AMP= %d\n",
			     android->duration, value);

		}
	} else {
		pr_err("drv260x not support mode\n");
	}

	if (android->duration != 0) {
		hrtimer_start(&android->stop_timer,
			      ktime_set(android->duration /
					MSEC_PER_SEC,
					(android->duration %
					 MSEC_PER_SEC) * NSEC_PER_MSEC),
			      HRTIMER_MODE_REL);
	}
	//android->haptics_playing = true;

      error_exit:

	mutex_unlock(&android->lock);
}

static enum hrtimer_restart android_haptics_stop_timer(struct hrtimer *timer)
{
	struct android_hal_stub *android =
	    container_of(timer, struct android_hal_stub, stop_timer);

	queue_work(android->wq, &android->haptics_stop_work);

	return HRTIMER_NORESTART;
}

static int android_hal_stub_init(struct drv260x_data *haptics)
{
	struct android_hal_stub *android = &haptics->android;
	//int ret;

	android->haptics_playing = false;
	android->wq = alloc_workqueue("android_wq", WQ_HIGHPRI | WQ_UNBOUND, 1);
	if (!android->wq) {
		pr_err("Failed to allocate workqueue for android hal stub\n");
		return -ENOMEM;
	}

	mutex_init(&android->lock);
	hrtimer_init(&android->stop_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	android->stop_timer.function = android_haptics_stop_timer;

	INIT_WORK(&android->haptics_play_work, android_haptics_play);
	INIT_WORK(&android->haptics_stop_work, android_haptics_stop);

	return 0;
}

static void android_hal_stub_exit(struct drv260x_data *haptics)
{
	struct android_hal_stub *android = &haptics->android;

	hrtimer_cancel(&android->stop_timer);

	cancel_work_sync(&android->haptics_play_work);
	cancel_work_sync(&android->haptics_stop_work);
	destroy_workqueue(android->wq);

	mutex_destroy(&android->lock);

}

static int fw_chksum(const struct firmware *fw)
{
	int sum = 0;
	int i = 0;
	int size = fw->size;
	const unsigned char *pBuf = fw->data;
	for (i = 0; i < size; i++) {
		if ((i > 11) && (i < 16)) {
		} else {
			sum += pBuf[i];
		}
	}
	return sum;
}

static void
drv2604_change_mode(struct drv260x_data *pDrv2604data, char work_mode,
		    char dev_mode)
{
	/* please be noted : LRA open loop cannot be used with analog input mode */
	if (dev_mode == DEV_IDLE) {
		pDrv2604data->dev_mode = dev_mode;
		pDrv2604data->work_mode = work_mode;

	} else if (dev_mode == DEV_STANDBY) {

		if (pDrv2604data->dev_mode != DEV_STANDBY) {
			pDrv2604data->dev_mode = DEV_STANDBY;
			regmap_write(pDrv2604data->regmap, MODE_REG,
				     MODE_STANDBY);
		}

		pDrv2604data->work_mode = WORK_IDLE;

	} else if (dev_mode == DEV_READY) {

		if ((work_mode != pDrv2604data->work_mode)
		    || (dev_mode != pDrv2604data->dev_mode)) {

			pDrv2604data->work_mode = work_mode;

			pDrv2604data->dev_mode = dev_mode;

			if ((pDrv2604data->work_mode == WORK_VIBRATOR)
			    || (pDrv2604data->work_mode == WORK_PATTERN_RTP_ON)
			    || (pDrv2604data->work_mode == WORK_SEQ_RTP_ON)
			    || (pDrv2604data->work_mode == WORK_RTP)) {

				regmap_write(pDrv2604data->regmap, MODE_REG,
					     MODE_REAL_TIME_PLAYBACK);

			} else if (pDrv2604data->work_mode == WORK_CALIBRATION) {
				regmap_write(pDrv2604data->regmap, MODE_REG,
					     AUTO_CALIBRATION);
			} else if (pDrv2604data->work_mode == WORK_SEQ_PLAYBACK) {
				regmap_write(pDrv2604data->regmap, DRV260X_GO,
					     1);
			} else {
				regmap_write(pDrv2604data->regmap, MODE_REG,
					     MODE_INTERNAL_TRIGGER);
			}

		}

	}

}

static void drv2604_firmware_load(const struct firmware *fw, void *context)
{

	struct drv260x_data *pDrv2604data = context;
	int size = 0, fwsize = 0, i = 0;
	const unsigned char *pBuf = NULL;
	unsigned char *pTmp = NULL;

	if (fw != NULL) {
		pBuf = fw->data;
		size = fw->size;
		memcpy(&(pDrv2604data->fw_header), pBuf,
		       sizeof(struct drv2604_fw_header));

		if ((pDrv2604data->fw_header.fw_magic != DRV2604_MAGIC)
		    || (pDrv2604data->fw_header.fw_size != size)
		    || (pDrv2604data->fw_header.fw_chksum != fw_chksum(fw))
		    || (pDrv2604data->fw_header.fw_effCount < 1)) {

			printk
			    ("%s, ERROR!! firmware not right:Magic=0x%x,Size=%d,chksum=0x%x, count=%d\n",
			     __FUNCTION__, pDrv2604data->fw_header.fw_magic,
			     pDrv2604data->fw_header.fw_size,
			     pDrv2604data->fw_header.fw_chksum,
			     pDrv2604data->fw_header.fw_effCount);
		} else {
			printk("%s, firmware good, count = %d\n", __FUNCTION__,
			       pDrv2604data->fw_header.fw_effCount);

			pDrv2604data->effects_count =
			    pDrv2604data->fw_header.fw_effCount;
			pDrv2604data->pEffDuration =
			    kmalloc(pDrv2604data->effects_count * sizeof(u32),
				    GFP_KERNEL);

			if (pDrv2604data->pEffDuration == NULL) {
				pr_err("%s: can not allocate memory\n",
				       __func__);
				//return -ENOMEM;
			}
			pTmp =
			    (unsigned char *)(pBuf) +
			    sizeof(struct drv2604_fw_header);
			memcpy(pDrv2604data->pEffDuration, pTmp,
			       pDrv2604data->effects_count * sizeof(u32));

			drv2604_change_mode(pDrv2604data, WORK_IDLE, DEV_READY);

			pTmp += pDrv2604data->effects_count * sizeof(u32);

			regmap_write(pDrv2604data->regmap,
				     DRV2604_REG_RAM_ADDR_UPPER_BYTE, 0);
			regmap_write(pDrv2604data->regmap,
				     DRV2604_REG_RAM_ADDR_LOWER_BYTE, 0);

			fwsize = size - sizeof(struct drv2604_fw_header)
			    - pDrv2604data->effects_count * sizeof(u32);

			printk("%s, firmware good, fwsize = %d\n", __FUNCTION__,
			       fwsize);

			for (i = 0; i < fwsize; i++) {
				printk("firmware bytes[0x%x]\n", pTmp[i]);
				regmap_write(pDrv2604data->regmap,
					     DRV2604_REG_RAM_DATA, pTmp[i]);
			}

			drv2604_change_mode(pDrv2604data, WORK_IDLE,
					    DEV_STANDBY);
		}
	} else {
		printk("%s, ERROR!! firmware not found\n", __FUNCTION__);
	}

}

static int drv260x_haptics_erase(struct input_dev *dev, int effect_id)
{
	struct drv260x_data *pDrv2604data = input_get_drvdata(dev);
	struct android_hal_stub *android = &pDrv2604data->android;
	int rc = 0;

	printk("%s enter\n", __func__);
	pDrv2604data->effect_type = 0;
	android->duration = 0;
	return rc;
}

static void drv260x_haptics_set_gain(struct input_dev *dev, u16 gain)
{

	char value = 0;
	struct drv260x_data *haptics = input_get_drvdata(dev);

	printk("%s enter\n", __func__);

	if (gain == 0)
		return;

	if (gain > 0x7fff)
		gain = 0x7fff;

	value = (127 * gain) / 0x7fff;
	haptics->amp = value;
}

static int drv260x_haptics_upload_effect(struct input_dev *dev,
					 struct ff_effect *effect,
					 struct ff_effect *old)
{
	struct drv260x_data *pDrv2604data = input_get_drvdata(dev);
	struct android_hal_stub *android = &pDrv2604data->android;

	s16 data[CUSTOM_DATA_LEN];

	ktime_t rem;
	s64 time_us;
	uint time_ms = 0;
	int waveform_index = 0;
	//int error;

	printk("%s  %d enter\n", __func__, __LINE__);

	if (hrtimer_active(&android->stop_timer)) {
		//rem = hrtimer_get_remaining(&pDrv2604data->timer);
		rem = hrtimer_get_remaining(&android->stop_timer);
		time_us = ktime_to_us(rem);
		printk("waiting for playing clear sequence: %lld us\n",
		       time_us);
		usleep_range(time_us, time_us + 100);
	}

	pDrv2604data->effect_type = effect->type;
	printk("%s  %d  pDrv2604data->effect_type= 0x%x\n", __func__, __LINE__,
	       pDrv2604data->effect_type);

	if (pDrv2604data->effect_type == FF_CONSTANT) {
		printk("%s  effect_type is  FF_CONSTANT!, duration=%d \n",
		       __func__, effect->replay.length);
		printk("%s  effect_type is  FF_CONSTANT!, mCurrMagnitude=%d \n",
		       __func__, effect->u.constant.level);
		/*cont mode set duration */
		android->duration = effect->replay.length;
		pDrv2604data->work_mode = DRV260X_HAPTIC_ACTIVATE_RTP_MODE;
		pDrv2604data->effect_id = DRV260X_EFFECT_COUNT_MAX;

	} else if (pDrv2604data->effect_type == FF_PERIODIC) {
		pDrv2604data->work_mode = DRV260X_HAPTIC_ACTIVATE_RAM_MODE;

		if (pDrv2604data->effects_count == 0)
			return -EINVAL;

		if (effect->u.periodic.waveform != FF_CUSTOM) {
			printk("drv260x only accept custom waveforms\n");
			return -EINVAL;
		}

		printk("%s  effect_type is  FF_PERIODIC! \n", __func__);
		if (copy_from_user(data, effect->u.periodic.custom_data,
				   sizeof(s16) * CUSTOM_DATA_LEN))
			return -EFAULT;

		/* TODO use waveform index from HAL */
		pDrv2604data->effect_id = data[0];
		pDrv2604data->magnitude = effect->u.periodic.magnitude;	/*vmax level */

		printk("HAL pass to driver, %s effect_id= %d, magnitude = %d\n", __func__,
		       data[0], pDrv2604data->magnitude);

		if ((pDrv2604data->effect_id < 0) ||
		    (pDrv2604data->effect_id > (DRV260X_EFFECT_COUNT_MAX - 1)))
			return 0;

		switch (pDrv2604data->effect_id) {
		case DRV260X_EFFECT_CLICK:
			waveform_index = 0;
			break;
		case DRV260X_EFFECT_DOUBLECLICK:
			waveform_index = 1;
			break;
		case DRV260X_EFFECT_TICK:
			waveform_index = 2;
			break;
		case DRV260X_EFFECT_HEAVY_CLICK:
			waveform_index = 3;
			break;

		default:
			waveform_index = 0;
			break;
		}

		time_ms =
		    pDrv2604data->pEffDuration[waveform_index * 3] *
		    (pDrv2604data->play_effect_speed);

		if (waveform_index == 1) {
			time_ms += 340; //double click.
		}

		printk("copy to HAL time_ms[%d]\n", time_ms);

		data[1] = time_ms / 1000;	/*second data */
		data[2] = time_ms % 1000;	/*millisecond data */

		if (copy_to_user(effect->u.periodic.custom_data, data,
				 sizeof(s16) * CUSTOM_DATA_LEN))
			return -EFAULT;
	} else {
		printk("%s Unsupported effect type: %d\n", __func__,
		       effect->type);
	}

	return 0;
}

static int drv260x_haptics_playback(struct input_dev *dev, int effect_id,
				    int val)
{
	struct drv260x_data *pDrv2604data = input_get_drvdata(dev);
	struct android_hal_stub *android = &pDrv2604data->android;
	int rc = 0;

	printk("%s effect_id=%d , val = %d\n", __func__, effect_id, val);
	printk("%s pDrv2604data->effect_id=%d , pDrv2604data->work_mode = %d\n",
	       __func__, pDrv2604data->effect_id, pDrv2604data->work_mode);

	if (val > 0)
		pDrv2604data->state = 1;
	if (val <= 0)
		pDrv2604data->state = 0;

	hrtimer_cancel(&android->stop_timer);

	if (pDrv2604data->effect_type == FF_CONSTANT &&
	    pDrv2604data->work_mode == DRV260X_HAPTIC_ACTIVATE_RTP_MODE) {

		printk("%s enter cont_mode \n", __func__);
		queue_work(android->wq, &android->haptics_play_work);

	} else if (pDrv2604data->effect_type == FF_PERIODIC &&
		   pDrv2604data->work_mode ==
		   DRV260X_HAPTIC_ACTIVATE_RAM_MODE) {
		printk("%s enter  ram_mode111\n", __func__);
		queue_work(android->wq, &android->haptics_play_work);
	} else {
		printk("%s not support other mode\n", __func__);
		/*other mode */
	}

	return rc;
}

static void HapticsFirmwareLoad(const struct firmware *fw, void *context)
{
	drv2604_firmware_load(fw, context);
	release_firmware(fw);
}

static int
drv260x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	const struct drv260x_platform_data *pdata =
	    dev_get_platdata(&client->dev);
	struct drv260x_data *haptics;
	struct ff_device *ff;
	int error;
	int rc = 0, effect_count_max;

	haptics = devm_kzalloc(&client->dev, sizeof(*haptics), GFP_KERNEL);
	if (!haptics)
		return -ENOMEM;

	haptics->rated_voltage = DRV260X_DEF_OD_CLAMP_VOLT;
	haptics->rated_voltage = DRV260X_DEF_RATED_VOLT;

	if (pdata) {
		haptics->mode = pdata->mode;
		haptics->library = pdata->library_selection;
		if (pdata->vib_overdrive_voltage)
			haptics->overdrive_voltage =
			    drv260x_calculate_voltage
			    (pdata->vib_overdrive_voltage);
		if (pdata->vib_rated_voltage)
			haptics->rated_voltage =
			    drv260x_calculate_voltage(pdata->vib_rated_voltage);
	} else if (client->dev.of_node) {

		error = drv260x_parse_dt(&client->dev, haptics);
		if (error)
			goto err_free_mem;
	} else {
		dev_err(&client->dev, "Platform data not set\n");
		goto err_free_mem;
	}

	if (haptics->mode < DRV260X_LRA_MODE
	    || haptics->mode > DRV260X_ERM_MODE) {
		dev_err(&client->dev, "Vibrator mode is invalid: %i\n",
			haptics->mode);
		goto err_free_mem;
	}

	if (haptics->library < DRV260X_LIB_EMPTY ||
	    haptics->library > DRV260X_ERM_LIB_F) {
		dev_err(&client->dev,
			"Library value is invalid: %i\n", haptics->library);
		goto err_free_mem;
	}

	if (haptics->mode == DRV260X_LRA_MODE &&
	    haptics->library != DRV260X_LIB_EMPTY &&
	    haptics->library != DRV260X_LIB_LRA) {
		dev_err(&client->dev, "LRA Mode with ERM Library mismatch\n");
		goto err_free_mem;
	}

	if (haptics->mode == DRV260X_ERM_MODE &&
	    (haptics->library == DRV260X_LIB_EMPTY ||
	     haptics->library == DRV260X_LIB_LRA)) {
		dev_err(&client->dev, "ERM Mode with LRA Library mismatch\n");
		goto err_free_mem;
	}

	pr_err("drv260x probe, i2c address= %x\n", client->addr);

	error = request_firmware_nowait(THIS_MODULE,
					FW_ACTION_HOTPLUG,
					"drv2604.bin",
					&(client->dev),
					GFP_KERNEL, haptics,
					HapticsFirmwareLoad);

	if (gpio_is_valid(enable_pin)) {

		gpio_request(enable_pin, "drv260x,enable-gpio");

		gpio_direction_output(enable_pin, 1);
	}

	if (gpio_is_valid(pwm_pin)) {
		gpio_request(pwm_pin, "drv260x,pwm-gpio");
		gpio_direction_output(pwm_pin, 1);
	}

	haptics->input_dev = devm_input_allocate_device(&client->dev);
	if (!haptics->input_dev) {
		dev_err(&client->dev, "Failed to allocate input device\n");
		goto err_free_mem;
	}

	haptics->input_dev->name = "drv260x:haptics";
	haptics->input_dev->close = drv260x_close;
	input_set_drvdata(haptics->input_dev, haptics);
	//input_set_capability(haptics->input_dev, EV_FF, FF_RUMBLE);

	input_set_capability(haptics->input_dev, EV_FF, FF_CONSTANT);
	input_set_capability(haptics->input_dev, EV_FF, FF_GAIN);
	input_set_capability(haptics->input_dev, EV_FF, FF_PERIODIC);
	input_set_capability(haptics->input_dev, EV_FF, FF_CUSTOM);

	effect_count_max = FF_EFFECT_COUNT_MAX;

	rc = input_ff_create(haptics->input_dev, effect_count_max);
	if (rc < 0) {
		dev_err(&client->dev, "create FF input device failed, rc=%d\n",
			rc);
		goto err_free_mem;
	}

	ff = haptics->input_dev->ff;
	ff->upload = drv260x_haptics_upload_effect;
	ff->playback = drv260x_haptics_playback;
	ff->erase = drv260x_haptics_erase;
	ff->set_gain = drv260x_haptics_set_gain;
	rc = input_register_device(haptics->input_dev);
	if (rc < 0) {
		dev_err(&client->dev, "input_register_device: %d\n", rc);
		goto err_free_mem;
	}

	INIT_WORK(&haptics->work, drv260x_worker);

	haptics->client = client;
	i2c_set_clientdata(client, haptics);

	haptics->regmap = devm_regmap_init_i2c(client, &drv260x_regmap_config);

	if (IS_ERR(haptics->regmap)) {
		error = PTR_ERR(haptics->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			error);
		goto err_free_mem;
	}

	error = drv260x_init(haptics);
	if (error) {
		dev_err(&client->dev, "Device init failed: %d\n", error);
		goto err_free_mem;
	}

	error = android_hal_stub_init(haptics);
	if (error) {
		pr_err("Failed to init android hal stub: %d\n", error);
		goto err_free_mem;
	}

	return 0;

      err_free_mem:
	kfree(haptics);
	return error;
}

static int drv260x_remove(struct i2c_client *client)
{
	struct drv260x_data *haptics = i2c_get_clientdata(client);

	android_hal_stub_exit(haptics);

	return 0;
}

static int __maybe_unused drv260x_suspend(struct device *dev)
{
	struct drv260x_data *haptics = dev_get_drvdata(dev);
	int ret = 0;

	ret = regmap_write(haptics->regmap, DRV260X_MODE, MODE_STANDBY);
	if (ret) {
		dev_err(dev, "Failed to set standby mode\n");
		return ret;
	}

	pr_err("drv260x_suspend successfully.\n");
	return ret;
}

static int __maybe_unused drv260x_resume(struct device *dev)
{
	struct drv260x_data *haptics = dev_get_drvdata(dev);
	int ret = 0;

	ret = regmap_update_bits(haptics->regmap,
				 DRV260X_MODE, DRV260X_STANDBY_MASK, 0);
	if (ret) {
		dev_err(dev, "Failed to unset standby mode\n");
		return ret;
	}
	pr_err("drv260x_resume successfully.\n");
	return ret;
}

static SIMPLE_DEV_PM_OPS(drv260x_pm_ops, drv260x_suspend, drv260x_resume);

static const struct i2c_device_id drv260x_id[] = {
	{"drv2605l", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, drv260x_id);

#ifdef CONFIG_OF
static const struct of_device_id drv260x_of_match[] = {
	{.compatible = "ti,drv2604",},
	{.compatible = "ti,drv2604l",},
	{.compatible = "ti,drv2605",},
	{.compatible = "ti,drv2605l",},
	{}
};

MODULE_DEVICE_TABLE(of, drv260x_of_match);
#endif

static struct i2c_driver drv260x_driver = {
	.probe = drv260x_probe,
	.driver = {
		   .name = "drv260x-haptics",
		   .of_match_table = of_match_ptr(drv260x_of_match),
		   .pm = &drv260x_pm_ops,
		   },
	.remove = drv260x_remove,
	.id_table = drv260x_id,
};

module_i2c_driver(drv260x_driver);

MODULE_DESCRIPTION("TI DRV260x haptics driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dan Murphy <dmurphy@ti.com>");
