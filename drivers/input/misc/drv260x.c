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

#define pr_fmt(fmt) KBUILD_MODNAME "(as led_cdev): " fmt
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

struct drv2604_fw_header {
	int fw_magic;
	int fw_size;
	int fw_date;
	int fw_chksum;
	int fw_effCount;

};

struct android_hal_stub {
	struct led_classdev led_dev;
	struct workqueue_struct *wq;
	struct work_struct haptics_play_work;
	struct work_struct haptics_stop_work;
	struct mutex lock;	/* stop/play */
	bool haptics_playing;
	struct hrtimer stop_timer;
	u32 play_time_ms;
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
	//{ DRV260X_LRA_LOOP_PERIOD, 0x33 },
	{DRV260X_LRA_LOOP_PERIOD, 0x31},
	{DRV260X_VBAT_MON, 0x00},
	{DRV260X_LRA_RES_PERIOD, 0x00},

	{0xfd, 0x00},
	{0xfe, 0x00},
	{0xff, 0x00},
};

#define DRV260X_DEF_RATED_VOLT		0x90
#define DRV260X_DEF_OD_CLAMP_VOLT	0x90

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

	//gpiod_set_value(haptics->enable_gpio, 1);
	//gpio_set_value(4, 1);
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

static int
drv260x_haptics_play(struct input_dev *input, void *data,
		     struct ff_effect *effect)
{
	struct drv260x_data *haptics = input_get_drvdata(input);

	haptics->mode = DRV260X_LRA_NO_CAL_MODE;

	if (effect->u.rumble.strong_magnitude > 0)
		haptics->magnitude = effect->u.rumble.strong_magnitude;
	else if (effect->u.rumble.weak_magnitude > 0)
		haptics->magnitude = effect->u.rumble.weak_magnitude;
	else
		haptics->magnitude = 0;

	schedule_work(&haptics->work);

	return 0;
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

static const struct reg_sequence drv260x_lra_cal_regs_yzl[] = {
	//{ DRV260X_MODE, DRV260X_AUTO_CAL },
	//{ DRV260X_CTRL3, DRV260X_NG_THRESH_2 },
	//{ DRV260X_FEEDBACK_CTRL, DRV260X_FB_REG_LRA_MODE |
	//      DRV260X_BRAKE_FACTOR_4X | DRV260X_LOOP_GAIN_HIGH },

	{0x1A, 0XB6},
	{0x1B, 0X93},
	{0x1C, 0XF5},
	{0x1D, 0X80},
	{0x03, 0X06},
	{0x01, 0X07},

	{0x1E, 0X20},

};

static const struct reg_sequence drv260x_lra_init_regs[] = {

#if 0
	{DRV260X_MODE, DRV260X_RT_PLAYBACK},
	{DRV260X_A_TO_V_CTRL, DRV260X_AUDIO_HAPTICS_PEAK_20MS |
	 DRV260X_AUDIO_HAPTICS_FILTER_125HZ},
	{DRV260X_A_TO_V_MIN_INPUT, DRV260X_AUDIO_HAPTICS_MIN_IN_VOLT},
	{DRV260X_A_TO_V_MAX_INPUT, DRV260X_AUDIO_HAPTICS_MAX_IN_VOLT},
	{DRV260X_A_TO_V_MIN_OUT, DRV260X_AUDIO_HAPTICS_MIN_OUT_VOLT},
	{DRV260X_A_TO_V_MAX_OUT, DRV260X_AUDIO_HAPTICS_MAX_OUT_VOLT},
	{DRV260X_FEEDBACK_CTRL, DRV260X_FB_REG_LRA_MODE |
	 DRV260X_BRAKE_FACTOR_2X | DRV260X_LOOP_GAIN_MED | DRV260X_BEMF_GAIN_3},
	{DRV260X_CTRL1, DRV260X_STARTUP_BOOST},
	{DRV260X_CTRL2, DRV260X_SAMP_TIME_250},
	{DRV260X_CTRL3, DRV260X_NG_THRESH_2 | DRV260X_ANANLOG_IN},
	{DRV260X_CTRL4, DRV260X_AUTOCAL_TIME_500MS},

#else
	{0x01, 0x00},
	{0x17, 0xA4},
	{0x1D, 0x80},
	{0x00, 0x00},
	{0x02, 0x00},
	{0x19, 0xF2},
	{0x1A, 0xA7},
#endif

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

	error = regmap_write(haptics->regmap,
			     DRV260X_RATED_VOLT, haptics->rated_voltage);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write DRV260X_RATED_VOLT register: %d\n",
			error);
		return error;
	}

	error = regmap_write(haptics->regmap,
			     DRV260X_OD_CLAMP_VOLT, haptics->overdrive_voltage);
	if (error) {
		dev_err(&haptics->client->dev,
			"Failed to write DRV260X_OD_CLAMP_VOLT register: %d\n",
			error);
		return error;
	}

	pr_err("drv260x, haptics mode=%d\n", haptics->mode);

	switch (haptics->mode) {
	case DRV260X_LRA_MODE:

#if 1
		error = regmap_register_patch(haptics->regmap,
					      drv260x_lra_cal_regs,
					      ARRAY_SIZE(drv260x_lra_cal_regs));
#else
		error = regmap_register_patch(haptics->regmap,
					      drv260x_lra_cal_regs_yzl,
					      ARRAY_SIZE
					      (drv260x_lra_cal_regs_yzl));

#endif

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
	pr_err("drv260x_init end,  device_id=%#x\n", cal_buf);

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

static inline struct drv260x_data *led_to_haptics(struct led_classdev *led_cdev)
{
	struct android_hal_stub *android =
	    container_of(led_cdev, struct android_hal_stub, led_dev);
	return container_of(android, struct drv260x_data, android);
}

static int android_haptics_stop_playing(struct android_hal_stub *android)
{
	struct drv260x_data *haptics =
	    container_of(android, struct drv260x_data, android);
	int error;
	unsigned int cal_buf = 8;

	/* Lock needed. Only came here from android_haptics_stop/android_haptics_play */
	if (android->haptics_playing) {
		error = regmap_write(haptics->regmap, DRV260X_GO, 0x00);
		if (error) {
			pr_err("Failed to stop vibrating: %d\n", error);
			return error;
		}

		regmap_read(haptics->regmap, DRV260X_GO, &cal_buf);
		pr_err("android_haptics_stop_playing stop fire cal_buf=%#x\n",
		       cal_buf);
		android->haptics_playing = false;
		android->led_dev.brightness = LED_OFF;
		hrtimer_cancel(&android->stop_timer);
	}

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

#if 0
	{0xfd, 0x00},
	{0xfe, 0x00},		//address

	{0xff, 0x00},
	{0xff, 0x00},
	{0xff, 0x04},

	{0xff, 0x02},
	{0xff, 0xa0},
	{0xff, 0x90},
#endif
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
	//int i = 0;
	unsigned int cal_buf;
	mutex_lock(&android->lock);
	error = android_haptics_stop_playing(android);
	if (error)
		goto error_exit;

	error = regmap_write(haptics->regmap, 0x01, 0x00);
	if (error) {
		pr_err("Failed to start vibrating: %d\n", error);
		goto error_exit;
	}
//      error = regmap_register_patch(haptics->regmap,
//                                      drv260x_lra_play3_regs,
//                                      ARRAY_SIZE(drv260x_lra_play3_regs));

	error = regmap_write(haptics->regmap, 0x04, 0x01);	//register[0x04] playback from index_1
	if (error) {
		pr_err("Failed to start vibrating: %d\n", error);
		goto error_exit;
	}

	error = regmap_write(haptics->regmap, 0x05, 0x00);
	if (error) {
		pr_err("Failed to start vibrating: %d\n", error);
		goto error_exit;
	}

	error = regmap_write(haptics->regmap, DRV260X_GO, DRV260X_GO_BIT);
	if (error) {
		pr_err("Failed to start vibrating: %d\n", error);
		goto error_exit;
	}

	regmap_read(haptics->regmap, 0x20, &cal_buf);
	pr_err("android_haptics_play no firmware reg[20] = %#X\n", cal_buf);

	if (android->play_time_ms != 0) {
		hrtimer_start(&android->stop_timer,
			      ktime_set(android->play_time_ms / MSEC_PER_SEC,
					(android->play_time_ms % MSEC_PER_SEC) *
					NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}

	android->haptics_playing = true;
	android->led_dev.brightness = LED_FULL;

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

static void
android_vibrator_enable(struct led_classdev *led_cdev,
			enum led_brightness value)
{
	struct android_hal_stub *android =
	    container_of(led_cdev, struct android_hal_stub, led_dev);

	if (value == LED_OFF)
		queue_work(android->wq, &android->haptics_stop_work);
	else
		queue_work(android->wq, &android->haptics_play_work);
}

static ssize_t
state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct drv260x_data *haptics = led_to_haptics(led_cdev);
	int error;
	int value;

	error = regmap_read(haptics->regmap, DRV260X_MODE, &value);
	if (error) {
		pr_err("Failed to read Mode register: %d\n", error);
		value = DRV260X_STANDBY;
	}

	pr_debug("%s >> %d\n", __func__,
		 (value & DRV260X_STANDBY_MASK) == DRV260X_STANDBY ? 0 : 1);

	return snprintf(buf, PAGE_SIZE, "%d\n",
			(value & DRV260X_STANDBY_MASK) ==
			DRV260X_STANDBY ? 0 : 1);
}

static ssize_t
state_store(struct device *dev,
	    struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct drv260x_data *haptics = led_to_haptics(led_cdev);
	int ret;
	u32 state;
	int standby;

	ret = kstrtou32(buf, 10, &state);
	if (ret) {
		pr_err("Invalid input for state: %d\n", ret);
		return ret;
	}

	if (state == 0)
		standby = 1;
	else if (state == 1)
		standby = 0;
	else {
		pr_err("Invalid value (%d) for state\n", state);
		return -EINVAL;
	}

	ret = regmap_update_bits(haptics->regmap,
				 DRV260X_MODE, DRV260X_STANDBY_MASK, standby);
	if (ret)
		pr_err("Failed to write Mode register: %d\n", ret);

	pr_debug("%s << %d\n", __func__, state);

	return count;
}

static ssize_t
activate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct android_hal_stub *android =
	    container_of(led_cdev, struct android_hal_stub, led_dev);
	pr_debug("%s >> %d\n", __func__, android->haptics_playing ? 1 : 0);
	return snprintf(buf, PAGE_SIZE, "%d\n",
			android->haptics_playing ? 1 : 0);
}

static ssize_t
activate_store(struct device *dev,
	       struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct drv260x_data *haptics = led_to_haptics(led_cdev);
	int ret;
	unsigned int cal_buf;
	u32 activate;

	regmap_read(haptics->regmap, 0x0, &cal_buf);

	pr_err("drv260x, activate_store device_id=%#x\n", cal_buf);

	ret = kstrtou32(buf, 10, &activate);
	if (ret) {
		pr_err("Invalid input for activate: ret = %d\n", ret);
		return ret;
	}

	if (activate == 0) {
		pr_err("drv260x, activate: %d\n", activate);
		android_vibrator_enable(led_cdev, LED_OFF);
	} else if (activate == 1) {
		pr_err("drv260x, activate: %d\n", activate);
		android_vibrator_enable(led_cdev, LED_FULL);
	} else {
		pr_err("Invalid value (%d) for activate\n", activate);
		return -EINVAL;
	}

	pr_debug("%s << %d\n", __func__, activate);

	return count;
}

static ssize_t
duration_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct android_hal_stub *android =
	    container_of(led_cdev, struct android_hal_stub, led_dev);
	ktime_t time_rem;
	s64 time_us = 0;

	if (hrtimer_active(&android->stop_timer)) {
		time_rem = hrtimer_get_remaining(&android->stop_timer);
		time_us = ktime_to_us(time_rem);
		pr_debug("%s >> %lldms remaining\n", __func__, time_us / 1000);
	} else {
		pr_debug("%s >> %dms is set\n", __func__,
			 android->play_time_ms);
	}

	return snprintf(buf, PAGE_SIZE, "%lld\n", time_us);
}

static ssize_t
duration_store(struct device *dev,
	       struct device_attribute *attr, const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct android_hal_stub *android =
	    container_of(led_cdev, struct android_hal_stub, led_dev);
	u32 value;
	int ret;

	ret = kstrtouint(buf, 0, &value);
	if (ret) {
		pr_err("Invalid input for store: %d\n", ret);
		return ret;
	}

	android->play_time_ms = value;
	pr_debug("%s << %dms\n", __func__, value);

	return count;
}

static DEVICE_ATTR(state, 0644, state_show, state_store);
static DEVICE_ATTR(activate, 0644, activate_show, activate_store);
static DEVICE_ATTR(duration, 0644, duration_show, duration_store);

static struct attribute *android_led_dev_fs_attrs[] = {
	&dev_attr_state.attr,
	&dev_attr_activate.attr,
	&dev_attr_duration.attr,
	NULL,
};

static struct attribute_group android_led_dev_fs_attr_group = {
	.attrs = android_led_dev_fs_attrs,
};

static const struct attribute_group *android_led_dev_fs_attr_groups[] = {
	&android_led_dev_fs_attr_group,
	NULL,
};

static int android_hal_stub_init(struct drv260x_data *haptics)
{
	struct android_hal_stub *android = &haptics->android;
	int ret;

	android->haptics_playing = false;
	android->led_dev.name = "vibrator";
	android->led_dev.max_brightness = LED_FULL;
	android->led_dev.brightness_set = android_vibrator_enable;
	android->led_dev.flags = LED_BRIGHTNESS_FAST;
	android->led_dev.groups = android_led_dev_fs_attr_groups;

	ret = led_classdev_register(&haptics->client->dev, &android->led_dev);
	if (ret) {
		pr_err("Failed to create led classdev: %d\n", ret);
		return ret;
	}
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

	led_classdev_unregister(&android->led_dev);
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
			//schedule_timeout_interruptible(msecs_to_jiffies(WAKE_STANDBY_DELAY));
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
			} else {
				regmap_write(pDrv2604data->regmap, MODE_REG,
					     MODE_INTERNAL_TRIGGER);
				//schedule_timeout_interruptible(msecs_to_jiffies(STANDBY_WAKE_DELAY));
			}

		}

	}

}

static void drv2604_firmware_load(const struct firmware *fw, void *context)
{

	struct drv260x_data *pDrv2604data = context;
	int size = 0, fwsize = 0, i = 0;
	const unsigned char *pBuf = NULL;

	if (fw != NULL) {
		pBuf = fw->data;
		size = fw->size;
		memcpy(&(pDrv2604data->fw_header), pBuf,
		       sizeof(struct drv2604_fw_header));

		if ((pDrv2604data->fw_header.fw_magic != DRV2604_MAGIC)
		    || (pDrv2604data->fw_header.fw_size != size)
		    || (pDrv2604data->fw_header.fw_chksum != fw_chksum(fw))) {

			printk
			    ("%s, ERROR!! firmware not right:Magic=0x%x,Size=%d,chksum=0x%x\n",
			     __FUNCTION__, pDrv2604data->fw_header.fw_magic,
			     pDrv2604data->fw_header.fw_size,
			     pDrv2604data->fw_header.fw_chksum);
		} else {
			printk("%s, firmware good\n", __FUNCTION__);
			drv2604_change_mode(pDrv2604data, WORK_IDLE, DEV_READY);
			pBuf += sizeof(struct drv2604_fw_header);
			regmap_write(pDrv2604data->regmap,
				     DRV2604_REG_RAM_ADDR_UPPER_BYTE, 0);
			regmap_write(pDrv2604data->regmap,
				     DRV2604_REG_RAM_ADDR_LOWER_BYTE, 0);
			fwsize = size - sizeof(struct drv2604_fw_header);
			for (i = 0; i < fwsize; i++) {
				regmap_write(pDrv2604data->regmap,
					     DRV2604_REG_RAM_DATA, pBuf[i]);
			}
			drv2604_change_mode(pDrv2604data, WORK_IDLE,
					    DEV_STANDBY);
		}
	} else {
		printk("%s, ERROR!! firmware not found\n", __FUNCTION__);
	}

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
	int error;

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
			return error;
	} else {
		dev_err(&client->dev, "Platform data not set\n");
		return -ENODEV;
	}

	if (haptics->mode < DRV260X_LRA_MODE
	    || haptics->mode > DRV260X_ERM_MODE) {
		dev_err(&client->dev, "Vibrator mode is invalid: %i\n",
			haptics->mode);
		return -EINVAL;
	}

	if (haptics->library < DRV260X_LIB_EMPTY ||
	    haptics->library > DRV260X_ERM_LIB_F) {
		dev_err(&client->dev,
			"Library value is invalid: %i\n", haptics->library);
		return -EINVAL;
	}

	if (haptics->mode == DRV260X_LRA_MODE &&
	    haptics->library != DRV260X_LIB_EMPTY &&
	    haptics->library != DRV260X_LIB_LRA) {
		dev_err(&client->dev, "LRA Mode with ERM Library mismatch\n");
		return -EINVAL;
	}

	if (haptics->mode == DRV260X_ERM_MODE &&
	    (haptics->library == DRV260X_LIB_EMPTY ||
	     haptics->library == DRV260X_LIB_LRA)) {
		dev_err(&client->dev, "ERM Mode with LRA Library mismatch\n");
		return -EINVAL;
	}

	pr_err("drv260x probe, i2c address= %x\n", client->addr);

	error = request_firmware_nowait(THIS_MODULE,
					FW_ACTION_HOTPLUG,
					"drv2604.bin",
					&(client->dev),
					GFP_KERNEL, haptics,
					HapticsFirmwareLoad);

#if 0
	haptics->regulator = devm_regulator_get(&client->dev, "vbat");
	if (IS_ERR(haptics->regulator)) {
		error = PTR_ERR(haptics->regulator);
		dev_err(&client->dev, "unable to get regulator, error: %d\n",
			error);
		return error;
	}
#endif
	//haptics->enable_gpio = 4; //devm_gpiod_get_optional(&client->dev, "enable",
	//  GPIO_OUT_HIGH);
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
		return -ENOMEM;
	}

	haptics->input_dev->name = "drv260x:haptics";
	haptics->input_dev->close = drv260x_close;
	input_set_drvdata(haptics->input_dev, haptics);
	input_set_capability(haptics->input_dev, EV_FF, FF_RUMBLE);

	error = input_ff_create_memless(haptics->input_dev, NULL,
					drv260x_haptics_play);
	if (error) {
		dev_err(&client->dev, "input_ff_create() failed: %d\n", error);
		return error;
	}

	INIT_WORK(&haptics->work, drv260x_worker);

	haptics->client = client;
	i2c_set_clientdata(client, haptics);

	haptics->regmap = devm_regmap_init_i2c(client, &drv260x_regmap_config);

	if (IS_ERR(haptics->regmap)) {
		error = PTR_ERR(haptics->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n",
			error);
		return error;
	}

	error = drv260x_init(haptics);
	if (error) {
		dev_err(&client->dev, "Device init failed: %d\n", error);
		return error;
	}
	error = input_register_device(haptics->input_dev);
	if (error) {
		dev_err(&client->dev, "couldn't register input device: %d\n",
			error);
		return error;
	}
	error = android_hal_stub_init(haptics);
	if (error) {
		pr_err("Failed to init android hal stub: %d\n", error);
		return error;
	}

	return 0;
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

	ret = regmap_update_bits(haptics->regmap,
				 DRV260X_MODE,
				 DRV260X_STANDBY_MASK, DRV260X_STANDBY);
	if (ret) {
		dev_err(dev, "Failed to set standby mode\n");
		return ret;
	}
	pr_err("drv260x_suspend successfully.\n");
	return ret;
#if 0
	mutex_lock(&haptics->input_dev->mutex);

	if (haptics->input_dev->users) {
		ret = regmap_update_bits(haptics->regmap,
					 DRV260X_MODE,
					 DRV260X_STANDBY_MASK, DRV260X_STANDBY);
		if (ret) {
			dev_err(dev, "Failed to set standby mode\n");
			goto out;
		}

		ret = regulator_disable(haptics->regulator);
		if (ret) {
			dev_err(dev, "Failed to disable regulator\n");
			regmap_update_bits(haptics->regmap,
					   DRV260X_MODE, DRV260X_STANDBY_MASK,
					   0);
		}
	}
      out:
	mutex_unlock(&haptics->input_dev->mutex);
	return ret;
#endif
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
#if 0
	mutex_lock(&haptics->input_dev->mutex);

	if (haptics->input_dev->users) {
		ret = regulator_enable(haptics->regulator);
		if (ret) {
			dev_err(dev, "Failed to enable regulator\n");
			goto out;
		}

		ret = regmap_update_bits(haptics->regmap,
					 DRV260X_MODE, DRV260X_STANDBY_MASK, 0);
		if (ret) {
			dev_err(dev, "Failed to unset standby mode\n");
			regulator_disable(haptics->regulator);
			goto out;
		}
		//gpiod_set_value(haptics->enable_gpio, 1);
		//gpio_set_value(4, 1);

	}

      out:
	mutex_unlock(&haptics->input_dev->mutex);
	return ret;
#endif
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
