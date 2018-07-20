/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include "tsens.h"

/* eeprom layout data for 8937 */
#define BASE0_MASK_8937				0x000000ff
#define BASE1_MASK_8937				0xff000000
#define BASE1_SHIFT_8937			24

#define S0_P1_MASK_8937				0x000001f8
#define S1_P1_MASK_8937				0x001f8000
#define S2_P1_MASK_0_4_8937			0xf8000000
#define S2_P1_MASK_5_8937			0x00000001
#define S3_P1_MASK_8937				0x00001f80
#define S4_P1_MASK_8937				0x01f80000
#define S5_P1_MASK_8937				0x00003f00
#define S6_P1_MASK_8937				0x03f00000
#define S7_P1_MASK_8937				0x0000003f
#define S8_P1_MASK_8937				0x0003f000
#define S9_P1_MASK_8937				0x0000003f
#define S10_P1_MASK_8937			0x0003f000

#define S0_P2_MASK_8937				0x00007e00
#define S1_P2_MASK_8937				0x07e00000
#define S2_P2_MASK_8937				0x0000007e
#define S3_P2_MASK_8937				0x0007e000
#define S4_P2_MASK_8937				0x7e000000
#define S5_P2_MASK_8937				0x000fc000
#define S6_P2_MASK_8937				0xfc000000
#define S7_P2_MASK_8937				0x00000fc0
#define S8_P2_MASK_8937				0x00fc0000
#define S9_P2_MASK_8937				0x00000fc0
#define S10_P2_MASK_8937			0x00fc0000

#define S0_P1_SHIFT_8937			3
#define S1_P1_SHIFT_8937			15
#define S2_P1_SHIFT_0_4_8937			27
#define S2_P1_SHIFT_5_8937			5
#define S3_P1_SHIFT_8937			7
#define S4_P1_SHIFT_8937			19
#define S5_P1_SHIFT_8937			8
#define S6_P1_SHIFT_8937			20
#define S8_P1_SHIFT_8937			12
#define S10_P1_SHIFT_8937			12

#define S0_P2_SHIFT_8937			9
#define S1_P2_SHIFT_8937			21
#define S2_P2_SHIFT_8937			1
#define S3_P2_SHIFT_8937			13
#define S4_P2_SHIFT_8937			25
#define S5_P2_SHIFT_8937			14
#define S6_P2_SHIFT_8937			26
#define S7_P2_SHIFT_8937			6
#define S8_P2_SHIFT_8937			18
#define S9_P2_SHIFT_8937			6
#define S10_P2_SHIFT_8937			18

#define CAL_SEL_MASK_8937			0x00000007

/* eeprom layout for 8909 */
#define TSENS_EEPROM(n)				((n) + 0xa0)
#define BASE0_MASK_8909				0x000000ff
#define BASE1_MASK_8909				0x0000ff00

#define S0_P1_MASK_8909				0x0000003f
#define S1_P1_MASK_8909				0x0003f000
#define S2_P1_MASK_8909				0x3f000000
#define S3_P1_MASK_8909				0x000003f0
#define S4_P1_MASK_8909				0x003f0000

#define S0_P2_MASK_8909				0x00000fc0
#define S1_P2_MASK_8909				0x00fc0000
#define S2_P2_MASK_0_1_8909				0xc0000000
#define S2_P2_MASK_2_5_8909				0x0000000f
#define S3_P2_MASK_8909				0x0000fc00
#define S4_P2_MASK_8909				0x0fc00000

#define TSENS_CAL_SEL_8909				0x00070000
#define CAL_SEL_SHIFT_8909				16
#define BASE1_SHIFT_8909				8

#define S1_P1_SHIFT_8909				12
#define S2_P1_SHIFT_8909				24
#define S3_P1_SHIFT_8909				4
#define S4_P1_SHIFT_8909				16

#define S0_P2_SHIFT_8909				6
#define S1_P2_SHIFT_8909				18
#define S2_P2_SHIFT_0_1_8909				30
#define S2_P2_SHIFT_2_5_8909				2
#define S3_P2_SHIFT_8909				10
#define S4_P2_SHIFT_8909				22

#define CAL_DEGC_PT1				30
#define CAL_DEGC_PT2				120
/*
 * Use this function on devices where slope and offset calculations
 * depend on calibration data read from qfprom. On others the slope
 * and offset values are derived from tz->tzp->slope and tz->tzp->offset
 * resp.
 */
static void compute_intercept_slope(struct tsens_device *tmdev, u32 *p1,
	u32 *p2, u32 mode)
{
	int i;
	int num, den;

	for (i = 0; i < tmdev->ctrl_data->num_sensors; i++) {
		pr_debug(
			"sensor%d - data_point1:%#x data_point2:%#x\n",
			i, p1[i], p2[i]);

		tmdev->sensor[i].slope = SLOPE_DEFAULT;
		if (mode == TWO_PT_CALIB) {
			/*
			 * slope (m) = adc_code2 - adc_code1 (y2 - y1)/
			 *	temp_120_degc - temp_30_degc (x2 - x1)
			 */
			num = p2[i] - p1[i];
			num *= SLOPE_FACTOR;
			den = CAL_DEGC_PT2 - CAL_DEGC_PT1;
			tmdev->sensor[i].slope = num / den;
		}

		tmdev->sensor[i].offset = (p1[i] * SLOPE_FACTOR) -
			(CAL_DEGC_PT1 *
			tmdev->sensor[i].slope);
		pr_debug("offset:%d\n", tmdev->sensor[i].offset);
	}
}

int calibrate_8937(struct tsens_device *tmdev)
{
	int base0 = 0, base1 = 0, i;
	u32 p1[TSENS_NUM_SENSORS_8937], p2[TSENS_NUM_SENSORS_8937];
	int mode = 0, tmp = 0;
	u32 qfprom_cdata[5] = {0, 0, 0, 0, 0};

	qfprom_cdata[0] = readl_relaxed(tmdev->tsens_calib_addr + 0x1D8);
	qfprom_cdata[1] = readl_relaxed(tmdev->tsens_calib_addr + 0x1DC);
	qfprom_cdata[2] = readl_relaxed(tmdev->tsens_calib_addr + 0x210);
	qfprom_cdata[3] = readl_relaxed(tmdev->tsens_calib_addr + 0x214);
	qfprom_cdata[4] = readl_relaxed(tmdev->tsens_calib_addr + 0x230);

	mode = (qfprom_cdata[2] & CAL_SEL_MASK_8937);
	pr_debug("calibration mode is %d\n", mode);

	switch (mode) {
	case TWO_PT_CALIB:
		base1 = (qfprom_cdata[1] &
				BASE1_MASK_8937) >> BASE1_SHIFT_8937;
		p2[0] = (qfprom_cdata[2] &
				S0_P2_MASK_8937) >> S0_P2_SHIFT_8937;
		p2[1] = (qfprom_cdata[2] &
				S1_P2_MASK_8937) >> S1_P2_SHIFT_8937;
		p2[2] = (qfprom_cdata[3] &
				S2_P2_MASK_8937) >> S2_P2_SHIFT_8937;
		p2[3] = (qfprom_cdata[3] &
				S3_P2_MASK_8937) >> S3_P2_SHIFT_8937;
		p2[4] = (qfprom_cdata[3] &
				S4_P2_MASK_8937) >> S4_P2_SHIFT_8937;
		p2[5] = (qfprom_cdata[0] &
				S5_P2_MASK_8937) >> S5_P2_SHIFT_8937;
		p2[6] = (qfprom_cdata[0] &
				S6_P2_MASK_8937) >> S6_P2_SHIFT_8937;
		p2[7] = (qfprom_cdata[1] &
				S7_P2_MASK_8937) >> S7_P2_SHIFT_8937;
		p2[8] = (qfprom_cdata[1] &
				S8_P2_MASK_8937) >> S8_P2_SHIFT_8937;
		p2[9] = (qfprom_cdata[4] &
				S9_P2_MASK_8937) >> S9_P2_SHIFT_8937;
		p2[10] = (qfprom_cdata[4] &
				S10_P2_MASK_8937) >> S10_P2_SHIFT_8937;

		for (i = 0; i < TSENS_NUM_SENSORS_8937; i++)
			p2[i] = ((base1 + p2[i]) << 2);
		/* Fall through */
	case ONE_PT_CALIB2:
		base0 = (qfprom_cdata[0] & BASE0_MASK_8937);
		p1[0] = (qfprom_cdata[2] &
				S0_P1_MASK_8937) >> S0_P1_SHIFT_8937;
		p1[1] = (qfprom_cdata[2] &
				S1_P1_MASK_8937) >> S1_P1_SHIFT_8937;
		p1[2] = (qfprom_cdata[2] &
				S2_P1_MASK_0_4_8937) >> S2_P1_SHIFT_0_4_8937;
		tmp = (qfprom_cdata[3] &
				S2_P1_MASK_5_8937) << S2_P1_SHIFT_5_8937;
		p1[2] |= tmp;
		p1[3] = (qfprom_cdata[3] &
				S3_P1_MASK_8937) >> S3_P1_SHIFT_8937;
		p1[4] = (qfprom_cdata[3] &
				S4_P1_MASK_8937) >> S4_P1_SHIFT_8937;
		p1[5] = (qfprom_cdata[0] &
				S5_P1_MASK_8937) >> S5_P1_SHIFT_8937;
		p1[6] = (qfprom_cdata[0] &
				S6_P1_MASK_8937) >> S6_P1_SHIFT_8937;
		p1[7] = (qfprom_cdata[1] & S7_P1_MASK_8937);
		p1[8] = (qfprom_cdata[1] &
				S8_P1_MASK_8937) >> S8_P1_SHIFT_8937;
		p1[9] = (qfprom_cdata[4] & S9_P1_MASK_8937);
		p1[10] = (qfprom_cdata[4] &
				S10_P1_MASK_8937) >> S10_P1_SHIFT_8937;

		for (i = 0; i < TSENS_NUM_SENSORS_8937; i++)
			p1[i] = (((base0) + p1[i]) << 2);
		break;
	default:
		for (i = 0; i < TSENS_NUM_SENSORS_8937; i++) {
			p1[i] = 500;
			p2[i] = 780;
		}
		break;
	}

	compute_intercept_slope(tmdev, p1, p2, mode);

	return 0;
}

int calibrate_8909(struct tsens_device *tmdev)
{
	int i, base0 = 0, base1 = 0;
	u32 p1[TSENS_NUM_SENSORS_8909], p2[TSENS_NUM_SENSORS_8909];
	int mode = 0, temp = 0;
	uint32_t calib_data[3] = {0, 0, 0};

	calib_data[0] = readl_relaxed(
		TSENS_EEPROM(tmdev->tsens_calib_addr));
	calib_data[1] = readl_relaxed(
		(TSENS_EEPROM(tmdev->tsens_calib_addr) + 0x4));
	calib_data[2] = readl_relaxed(
		(TSENS_EEPROM(tmdev->tsens_calib_addr) + 0x3c));
	mode = (calib_data[2] & TSENS_CAL_SEL_8909) >> CAL_SEL_SHIFT_8909;

	pr_debug("calib mode is %d\n", mode);

	switch (mode) {
	case TWO_PT_CALIB:
		base1 = (calib_data[2] & BASE1_MASK_8909) >> BASE1_SHIFT_8909;
		p2[0] = (calib_data[0] & S0_P2_MASK_8909) >> S0_P2_SHIFT_8909;
		p2[1] = (calib_data[0] & S1_P2_MASK_8909) >> S1_P2_SHIFT_8909;
		p2[2] = (calib_data[0] &
				S2_P2_MASK_0_1_8909) >> S2_P2_SHIFT_0_1_8909;
		temp  = (calib_data[1] &
				S2_P2_MASK_2_5_8909) << S2_P2_SHIFT_2_5_8909;
		p2[2] |= temp;
		p2[3] = (calib_data[1] & S3_P2_MASK_8909) >> S3_P2_SHIFT_8909;
		p2[4] = (calib_data[1] & S4_P2_MASK_8909) >> S4_P2_SHIFT_8909;

		for (i = 0; i < TSENS_NUM_SENSORS_8909; i++)
			p2[i] = ((base1 + p2[i]) << 2);
	/* Fall through */
	case ONE_PT_CALIB2:
		base0 = (calib_data[2] & BASE0_MASK_8909);
		p1[0] = (calib_data[0] & S0_P1_MASK_8909);
		p1[1] = (calib_data[0] & S1_P1_MASK_8909) >> S1_P1_SHIFT_8909;
		p1[2] = (calib_data[0] & S2_P1_MASK_8909) >> S2_P1_SHIFT_8909;
		p1[3] = (calib_data[1] & S3_P1_MASK_8909) >> S3_P1_SHIFT_8909;
		p1[4] = (calib_data[1] & S4_P1_MASK_8909) >> S4_P1_SHIFT_8909;
		for (i = 0; i < TSENS_NUM_SENSORS_8909; i++)
			p1[i] = (((base0) + p1[i]) << 2);
		break;
	default:
		for (i = 0; i < TSENS_NUM_SENSORS_8909; i++) {
			p1[i] = 500;
			p2[i] = 780;
		}
		break;
	}

	compute_intercept_slope(tmdev, p1, p2, mode);
	return 0;
}
