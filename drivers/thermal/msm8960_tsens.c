/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
/*
 * Qualcomm MSM8960 TSENS driver
 *
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/msm_tsens.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/pm.h>

#include <mach/msm_iomap.h>
#include <mach/socinfo.h>

/* Trips: from very hot to very cold */
enum tsens_trip_type {
	TSENS_TRIP_STAGE3 = 0,
	TSENS_TRIP_STAGE2,
	TSENS_TRIP_STAGE1,
	TSENS_TRIP_STAGE0,
	TSENS_TRIP_NUM,
};

/* MSM8960 TSENS register info */
#define TSENS_CAL_DEGC					30
#define TSENS_MAIN_SENSOR				0

#define TSENS_8960_QFPROM_ADDR0		(MSM_QFPROM_BASE + 0x00000404)
#define TSENS_8960_QFPROM_SPARE_ADDR0	(MSM_QFPROM_BASE + 0x00000414)
#define TSENS_8960_CONFIG				0x9b
#define TSENS_8960_CONFIG_SHIFT				0
#define TSENS_8960_CONFIG_MASK		(0xf << TSENS_8960_CONFIG_SHIFT)
#define TSENS_CNTL_ADDR			(MSM_CLK_CTL_BASE + 0x00003620)
#define TSENS_EN					BIT(0)
#define TSENS_SW_RST					BIT(1)
#define TSENS_ADC_CLK_SEL				BIT(2)
#define SENSOR0_EN					BIT(3)
#define SENSOR1_EN					BIT(4)
#define SENSOR2_EN					BIT(5)
#define SENSOR3_EN					BIT(6)
#define SENSOR4_EN					BIT(7)
#define SENSORS_EN			(SENSOR0_EN | SENSOR1_EN |	\
					SENSOR2_EN | SENSOR3_EN | SENSOR4_EN)
#define TSENS_STATUS_CNTL_OFFSET	8
#define TSENS_MIN_STATUS_MASK		BIT((tsens_status_cntl_start))
#define TSENS_LOWER_STATUS_CLR		BIT((tsens_status_cntl_start + 1))
#define TSENS_UPPER_STATUS_CLR		BIT((tsens_status_cntl_start + 2))
#define TSENS_MAX_STATUS_MASK		BIT((tsens_status_cntl_start + 3))

#define TSENS_MEASURE_PERIOD				4 /* 1 sec. default */
#define TSENS_8960_SLP_CLK_ENA				BIT(26)

#define TSENS_THRESHOLD_ADDR		(MSM_CLK_CTL_BASE + 0x00003624)
#define TSENS_THRESHOLD_MAX_CODE			0xff
#define TSENS_THRESHOLD_MIN_CODE			0
#define TSENS_THRESHOLD_MAX_LIMIT_SHIFT			24
#define TSENS_THRESHOLD_MIN_LIMIT_SHIFT			16
#define TSENS_THRESHOLD_UPPER_LIMIT_SHIFT		8
#define TSENS_THRESHOLD_LOWER_LIMIT_SHIFT		0
#define TSENS_THRESHOLD_MAX_LIMIT_MASK		(TSENS_THRESHOLD_MAX_CODE << \
					TSENS_THRESHOLD_MAX_LIMIT_SHIFT)
#define TSENS_THRESHOLD_MIN_LIMIT_MASK		(TSENS_THRESHOLD_MAX_CODE << \
					TSENS_THRESHOLD_MIN_LIMIT_SHIFT)
#define TSENS_THRESHOLD_UPPER_LIMIT_MASK	(TSENS_THRESHOLD_MAX_CODE << \
					TSENS_THRESHOLD_UPPER_LIMIT_SHIFT)
#define TSENS_THRESHOLD_LOWER_LIMIT_MASK	(TSENS_THRESHOLD_MAX_CODE << \
					TSENS_THRESHOLD_LOWER_LIMIT_SHIFT)
/* Initial temperature threshold values */
#define TSENS_LOWER_LIMIT_TH				0x50
#define TSENS_UPPER_LIMIT_TH				0xdf
#define TSENS_MIN_LIMIT_TH				0x0
#define TSENS_MAX_LIMIT_TH				0xff

#define TSENS_S0_STATUS_ADDR			(MSM_CLK_CTL_BASE + 0x00003628)
#define TSENS_STATUS_ADDR_OFFSET			2
#define TSENS_SENSOR_STATUS_SIZE			4
#define TSENS_INT_STATUS_ADDR			(MSM_CLK_CTL_BASE + 0x0000363c)

#define TSENS_LOWER_INT_MASK				BIT(1)
#define TSENS_UPPER_INT_MASK				BIT(2)
#define TSENS_MAX_INT_MASK				BIT(3)
#define TSENS_TRDY_MASK					BIT(7)

#define TSENS_8960_CONFIG_ADDR			(MSM_CLK_CTL_BASE + 0x00003640)
#define TSENS_TRDY_RDY_MIN_TIME				1000
#define TSENS_TRDY_RDY_MAX_TIME				1100
#define TSENS_SENSOR_SHIFT				16
#define TSENS_RED_SHIFT					8
#define TSENS_8960_QFPROM_SHIFT				4
#define TSENS_SENSOR_QFPROM_SHIFT			2
#define TSENS_SENSOR0_SHIFT				3
#define TSENS_MASK1					1

#define TSENS_8660_QFPROM_ADDR			(MSM_QFPROM_BASE + 0x000000bc)
#define TSENS_8660_QFPROM_RED_TEMP_SENSOR0_SHIFT	24
#define TSENS_8660_QFPROM_TEMP_SENSOR0_SHIFT		16
#define TSENS_8660_QFPROM_TEMP_SENSOR0_MASK		(255		\
					<< TSENS_8660_QFPROM_TEMP_SENSOR0_SHIFT)
#define TSENS_8660_CONFIG				01
#define TSENS_8660_CONFIG_SHIFT				28
#define TSENS_8660_CONFIG_MASK			(3 << TSENS_8660_CONFIG_SHIFT)
#define TSENS_8660_SLP_CLK_ENA				BIT(24)

#define TSENS_8064_SENSOR5_EN				BIT(8)
#define TSENS_8064_SENSOR6_EN				BIT(9)
#define TSENS_8064_SENSOR7_EN				BIT(10)
#define TSENS_8064_SENSOR8_EN				BIT(11)
#define TSENS_8064_SENSOR9_EN				BIT(12)
#define TSENS_8064_SENSOR10_EN				BIT(13)
#define TSENS_8064_SENSORS_EN			(SENSORS_EN | \
						TSENS_8064_SENSOR5_EN | \
						TSENS_8064_SENSOR6_EN | \
						TSENS_8064_SENSOR7_EN | \
						TSENS_8064_SENSOR8_EN | \
						TSENS_8064_SENSOR9_EN | \
						TSENS_8064_SENSOR10_EN)
#define TSENS_8064_STATUS_CNTL			(MSM_CLK_CTL_BASE + 0x00003660)
#define TSENS_8064_S5_STATUS_ADDR		(MSM_CLK_CTL_BASE + 0x00003664)
#define TSENS_8064_SEQ_SENSORS				5
#define TSENS_8064_S4_S5_OFFSET				40
#define TSENS_CNTL_RESUME_MASK				0xfffffff9
#define TSENS_8960_SENSOR_MASK				0xf8
#define TSENS_8064_SENSOR_MASK				0x3ff8

static int tsens_status_cntl_start;

struct tsens_tm_device_sensor {
	struct thermal_zone_device	*tz_dev;
	enum thermal_device_mode	mode;
	unsigned int			sensor_num;
	struct work_struct		work;
	int				offset;
	int				calib_data;
	int				calib_data_backup;
	uint32_t			slope_mul_tsens_factor;
};

struct tsens_tm_device {
	bool				prev_reading_avail;
	int				tsens_factor;
	uint32_t			tsens_num_sensor;
	enum platform_type		hw_type;
	int				pm_tsens_thr_data;
	int				pm_tsens_cntl;
	struct work_struct		tsens_work;
	struct tsens_tm_device_sensor	sensor[0];
};

struct tsens_tm_device *tmdev;

/* Temperature on y axis and ADC-code on x-axis */
static int tsens_tz_code_to_degC(int adc_code, int sensor_num)
{
	int degcbeforefactor, degc;
	degcbeforefactor = (adc_code *
			tmdev->sensor[sensor_num].slope_mul_tsens_factor
			+ tmdev->sensor[sensor_num].offset);

	if (degcbeforefactor == 0)
		degc = degcbeforefactor;
	else if (degcbeforefactor > 0)
		degc = (degcbeforefactor + tmdev->tsens_factor/2)
				/ tmdev->tsens_factor;
	else
		degc = (degcbeforefactor - tmdev->tsens_factor/2)
				/ tmdev->tsens_factor;
	return degc;
}

static int tsens_tz_degC_to_code(int degC, int sensor_num)
{
	int code = (degC * tmdev->tsens_factor -
		tmdev->sensor[sensor_num].offset
		+ tmdev->sensor[sensor_num].slope_mul_tsens_factor/2)
		/ tmdev->sensor[sensor_num].slope_mul_tsens_factor;

	if (code > TSENS_THRESHOLD_MAX_CODE)
		code = TSENS_THRESHOLD_MAX_CODE;
	else if (code < TSENS_THRESHOLD_MIN_CODE)
		code = TSENS_THRESHOLD_MIN_CODE;
	return code;
}

static void tsens8960_get_temp(int sensor_num, unsigned long *temp)
{
	unsigned int code, offset = 0, sensor_addr;

	if (!tmdev->prev_reading_avail) {
		while (!(readl_relaxed(TSENS_INT_STATUS_ADDR)
					& TSENS_TRDY_MASK))
			usleep_range(TSENS_TRDY_RDY_MIN_TIME,
				TSENS_TRDY_RDY_MAX_TIME);
		tmdev->prev_reading_avail = true;
	}

	sensor_addr = (unsigned int)TSENS_S0_STATUS_ADDR;
	if (tmdev->hw_type == APQ_8064 &&
			sensor_num >= TSENS_8064_SEQ_SENSORS)
		offset = TSENS_8064_S4_S5_OFFSET;
	code = readl_relaxed(sensor_addr + offset +
			(sensor_num << TSENS_STATUS_ADDR_OFFSET));
	*temp = tsens_tz_code_to_degC(code, sensor_num);
}

static int tsens_tz_get_temp(struct thermal_zone_device *thermal,
			     unsigned long *temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;

	if (!tm_sensor || tm_sensor->mode != THERMAL_DEVICE_ENABLED || !temp)
		return -EINVAL;

	tsens8960_get_temp(tm_sensor->sensor_num, temp);

	return 0;
}

int tsens_get_temp(struct tsens_device *device, unsigned long *temp)
{
	if (!tmdev)
		return -ENODEV;

	tsens8960_get_temp(device->sensor_num, temp);

	return 0;
}
EXPORT_SYMBOL(tsens_get_temp);

static int tsens_tz_get_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode *mode)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;

	if (!tm_sensor || !mode)
		return -EINVAL;

	*mode = tm_sensor->mode;

	return 0;
}

/* Function to enable the mode.
 * If the main sensor is disabled all the sensors are disable and
 * the clock is disabled.
 * If the main sensor is not enabled and sub sensor is enabled
 * returns with an error stating the main sensor is not enabled.
 */
static int tsens_tz_set_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode mode)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg, mask, i;

	if (!tm_sensor)
		return -EINVAL;

	if (mode != tm_sensor->mode) {
		reg = readl_relaxed(TSENS_CNTL_ADDR);

		mask = 1 << (tm_sensor->sensor_num + TSENS_SENSOR0_SHIFT);
		if (mode == THERMAL_DEVICE_ENABLED) {
			if ((mask != SENSOR0_EN) && !(reg & SENSOR0_EN)) {
				pr_info("Main sensor not enabled\n");
				return -EINVAL;
			}
			writel_relaxed(reg | TSENS_SW_RST, TSENS_CNTL_ADDR);
			if (tmdev->hw_type == MSM_8960 ||
				tmdev->hw_type == MDM_9615 ||
				tmdev->hw_type == APQ_8064)
				reg |= mask | TSENS_8960_SLP_CLK_ENA
							| TSENS_EN;
			else
				reg |= mask | TSENS_8660_SLP_CLK_ENA
							| TSENS_EN;
			tmdev->prev_reading_avail = false;
		} else {
			reg &= ~mask;
			if (!(reg & SENSOR0_EN)) {
				if (tmdev->hw_type == APQ_8064)
					reg &= ~(TSENS_8064_SENSORS_EN |
						TSENS_8960_SLP_CLK_ENA |
						TSENS_EN);
				else if (tmdev->hw_type == MSM_8960 ||
						tmdev->hw_type == MDM_9615)
					reg &= ~(SENSORS_EN |
						TSENS_8960_SLP_CLK_ENA |
						TSENS_EN);
				else
					reg &= ~(SENSORS_EN |
						TSENS_8660_SLP_CLK_ENA |
						TSENS_EN);

				for (i = 1; i < tmdev->tsens_num_sensor; i++)
					tmdev->sensor[i].mode = mode;

			}
		}
		writel_relaxed(reg, TSENS_CNTL_ADDR);
	}
	tm_sensor->mode = mode;

	return 0;
}

static int tsens_tz_get_trip_type(struct thermal_zone_device *thermal,
				   int trip, enum thermal_trip_type *type)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;

	if (!tm_sensor || trip < 0 || !type)
		return -EINVAL;

	switch (trip) {
	case TSENS_TRIP_STAGE3:
		*type = THERMAL_TRIP_CRITICAL;
		break;
	case TSENS_TRIP_STAGE2:
		*type = THERMAL_TRIP_CONFIGURABLE_HI;
		break;
	case TSENS_TRIP_STAGE1:
		*type = THERMAL_TRIP_CONFIGURABLE_LOW;
		break;
	case TSENS_TRIP_STAGE0:
		*type = THERMAL_TRIP_CRITICAL_LOW;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tsens_tz_activate_trip_type(struct thermal_zone_device *thermal,
			int trip, enum thermal_trip_activation_mode mode)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg_cntl, reg_th, code, hi_code, lo_code, mask;

	if (!tm_sensor || trip < 0)
		return -EINVAL;

	lo_code = TSENS_THRESHOLD_MIN_CODE;
	hi_code = TSENS_THRESHOLD_MAX_CODE;

	if (tmdev->hw_type == APQ_8064)
		reg_cntl = readl_relaxed(TSENS_8064_STATUS_CNTL);
	else
		reg_cntl = readl_relaxed(TSENS_CNTL_ADDR);

	reg_th = readl_relaxed(TSENS_THRESHOLD_ADDR);
	switch (trip) {
	case TSENS_TRIP_STAGE3:
		code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		mask = TSENS_MAX_STATUS_MASK;

		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			lo_code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MIN_STATUS_MASK))
			lo_code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE2:
		code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		mask = TSENS_UPPER_STATUS_CLR;

		if (!(reg_cntl & TSENS_MAX_STATUS_MASK))
			hi_code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MIN_STATUS_MASK))
			lo_code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE1:
		code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		mask = TSENS_LOWER_STATUS_CLR;

		if (!(reg_cntl & TSENS_MIN_STATUS_MASK))
			lo_code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MAX_STATUS_MASK))
			hi_code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE0:
		code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		mask = TSENS_MIN_STATUS_MASK;

		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			hi_code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MAX_STATUS_MASK))
			hi_code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	if (mode == THERMAL_TRIP_ACTIVATION_DISABLED) {
		if (tmdev->hw_type == APQ_8064)
			writel_relaxed(reg_cntl | mask, TSENS_8064_STATUS_CNTL);
		else
			writel_relaxed(reg_cntl | mask, TSENS_CNTL_ADDR);
	} else {
		if (code < lo_code || code > hi_code) {
			pr_info("%s with invalid code %x\n", __func__, code);
			return -EINVAL;
		}
		if (tmdev->hw_type == APQ_8064)
			writel_relaxed(reg_cntl & ~mask,
					TSENS_8064_STATUS_CNTL);
		else
			writel_relaxed(reg_cntl & ~mask, TSENS_CNTL_ADDR);
	}
	mb();
	return 0;
}

static int tsens_tz_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip, unsigned long *temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg;

	if (!tm_sensor || trip < 0 || !temp)
		return -EINVAL;

	reg = readl_relaxed(TSENS_THRESHOLD_ADDR);
	switch (trip) {
	case TSENS_TRIP_STAGE3:
		reg = (reg & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE2:
		reg = (reg & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE1:
		reg = (reg & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE0:
		reg = (reg & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	*temp = tsens_tz_code_to_degC(reg, tm_sensor->sensor_num);

	return 0;
}

static int tsens_tz_get_crit_temp(struct thermal_zone_device *thermal,
				  unsigned long *temp)
{
	return tsens_tz_get_trip_temp(thermal, TSENS_TRIP_STAGE3, temp);
}

static int tsens_tz_notify(struct thermal_zone_device *thermal,
				int count, enum thermal_trip_type type)
{
	/* TSENS driver does not shutdown the device.
	   All Thermal notification are sent to the
	   thermal daemon to take appropriate action */
	return 1;
}

static int tsens_tz_set_trip_temp(struct thermal_zone_device *thermal,
				   int trip, long temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg_th, reg_cntl;
	int code, hi_code, lo_code, code_err_chk;

	code_err_chk = code = tsens_tz_degC_to_code(temp,
					tm_sensor->sensor_num);
	if (!tm_sensor || trip < 0)
		return -EINVAL;

	lo_code = TSENS_THRESHOLD_MIN_CODE;
	hi_code = TSENS_THRESHOLD_MAX_CODE;

	if (tmdev->hw_type == APQ_8064)
		reg_cntl = readl_relaxed(TSENS_8064_STATUS_CNTL);
	else
		reg_cntl = readl_relaxed(TSENS_CNTL_ADDR);
	reg_th = readl_relaxed(TSENS_THRESHOLD_ADDR);
	switch (trip) {
	case TSENS_TRIP_STAGE3:
		code <<= TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		reg_th &= ~TSENS_THRESHOLD_MAX_LIMIT_MASK;

		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			lo_code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MIN_STATUS_MASK))
			lo_code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE2:
		code <<= TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		reg_th &= ~TSENS_THRESHOLD_UPPER_LIMIT_MASK;

		if (!(reg_cntl & TSENS_MAX_STATUS_MASK))
			hi_code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MIN_STATUS_MASK))
			lo_code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE1:
		code <<= TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		reg_th &= ~TSENS_THRESHOLD_LOWER_LIMIT_MASK;

		if (!(reg_cntl & TSENS_MIN_STATUS_MASK))
			lo_code = (reg_th & TSENS_THRESHOLD_MIN_LIMIT_MASK)
					>> TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MAX_STATUS_MASK))
			hi_code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		break;
	case TSENS_TRIP_STAGE0:
		code <<= TSENS_THRESHOLD_MIN_LIMIT_SHIFT;
		reg_th &= ~TSENS_THRESHOLD_MIN_LIMIT_MASK;

		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			hi_code = (reg_th & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_th & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
		else if (!(reg_cntl & TSENS_MAX_STATUS_MASK))
			hi_code = (reg_th & TSENS_THRESHOLD_MAX_LIMIT_MASK)
					>> TSENS_THRESHOLD_MAX_LIMIT_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	if (code_err_chk < lo_code || code_err_chk > hi_code)
		return -EINVAL;

	writel_relaxed(reg_th | code, TSENS_THRESHOLD_ADDR);

	return 0;
}

static struct thermal_zone_device_ops tsens_thermal_zone_ops = {
	.get_temp = tsens_tz_get_temp,
	.get_mode = tsens_tz_get_mode,
	.set_mode = tsens_tz_set_mode,
	.get_trip_type = tsens_tz_get_trip_type,
	.activate_trip_type = tsens_tz_activate_trip_type,
	.get_trip_temp = tsens_tz_get_trip_temp,
	.set_trip_temp = tsens_tz_set_trip_temp,
	.get_crit_temp = tsens_tz_get_crit_temp,
	.notify = tsens_tz_notify,
};

static void notify_uspace_tsens_fn(struct work_struct *work)
{
	struct tsens_tm_device_sensor *tm = container_of(work,
		struct tsens_tm_device_sensor, work);

	sysfs_notify(&tm->tz_dev->device.kobj,
					NULL, "type");
}

static void tsens_scheduler_fn(struct work_struct *work)
{
	struct tsens_tm_device *tm = container_of(work, struct tsens_tm_device,
					tsens_work);
	unsigned int threshold, threshold_low, i, code, reg, sensor, mask;
	unsigned int sensor_addr;
	bool upper_th_x, lower_th_x;
	int adc_code;

	if (tmdev->hw_type == APQ_8064) {
		reg = readl_relaxed(TSENS_8064_STATUS_CNTL);
		writel_relaxed(reg | TSENS_LOWER_STATUS_CLR |
			TSENS_UPPER_STATUS_CLR, TSENS_8064_STATUS_CNTL);
	} else {
		reg = readl_relaxed(TSENS_CNTL_ADDR);
		writel_relaxed(reg | TSENS_LOWER_STATUS_CLR |
			TSENS_UPPER_STATUS_CLR, TSENS_CNTL_ADDR);
	}

	mask = ~(TSENS_LOWER_STATUS_CLR | TSENS_UPPER_STATUS_CLR);
	threshold = readl_relaxed(TSENS_THRESHOLD_ADDR);
	threshold_low = (threshold & TSENS_THRESHOLD_LOWER_LIMIT_MASK)
					>> TSENS_THRESHOLD_LOWER_LIMIT_SHIFT;
	threshold = (threshold & TSENS_THRESHOLD_UPPER_LIMIT_MASK)
					>> TSENS_THRESHOLD_UPPER_LIMIT_SHIFT;
	sensor = readl_relaxed(TSENS_CNTL_ADDR);
	if (tmdev->hw_type == APQ_8064) {
		reg = readl_relaxed(TSENS_8064_STATUS_CNTL);
		sensor &= (uint32_t) TSENS_8064_SENSORS_EN;
	} else {
		reg = sensor;
		sensor &= (uint32_t) SENSORS_EN;
	}
	sensor >>= TSENS_SENSOR0_SHIFT;
	sensor_addr = (unsigned int)TSENS_S0_STATUS_ADDR;
	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		if (i == TSENS_8064_SEQ_SENSORS)
			sensor_addr += TSENS_8064_S4_S5_OFFSET;
		if (sensor & TSENS_MASK1) {
			code = readl_relaxed(sensor_addr);
			upper_th_x = code >= threshold;
			lower_th_x = code <= threshold_low;
			if (upper_th_x)
				mask |= TSENS_UPPER_STATUS_CLR;
			if (lower_th_x)
				mask |= TSENS_LOWER_STATUS_CLR;
			if (upper_th_x || lower_th_x) {
				/* Notify user space */
				schedule_work(&tm->sensor[i].work);
				adc_code = readl_relaxed(sensor_addr);
				pr_debug("Trigger (%d degrees) for sensor %d\n",
					tsens_tz_code_to_degC(adc_code, i), i);
			}
		}
		sensor >>= 1;
		sensor_addr += TSENS_SENSOR_STATUS_SIZE;
	}
	if (tmdev->hw_type == APQ_8064)
		writel_relaxed(reg & mask, TSENS_8064_STATUS_CNTL);
	else
	writel_relaxed(reg & mask, TSENS_CNTL_ADDR);
	mb();
}

static irqreturn_t tsens_isr(int irq, void *data)
{
	schedule_work(&tmdev->tsens_work);

	return IRQ_HANDLED;
}

#ifdef CONFIG_PM
static int tsens_suspend(struct device *dev)
{
	int i = 0;

	tmdev->pm_tsens_thr_data = readl_relaxed(TSENS_THRESHOLD_ADDR);
	tmdev->pm_tsens_cntl = readl_relaxed(TSENS_CNTL_ADDR);
	writel_relaxed(tmdev->pm_tsens_cntl &
		~(TSENS_8960_SLP_CLK_ENA | TSENS_EN), TSENS_CNTL_ADDR);
	tmdev->prev_reading_avail = 0;
	for (i = 0; i < tmdev->tsens_num_sensor; i++)
		tmdev->sensor[i].mode = THERMAL_DEVICE_DISABLED;
	disable_irq_nosync(TSENS_UPPER_LOWER_INT);
	mb();
	return 0;
}

static int tsens_resume(struct device *dev)
{
	unsigned int reg_cntl = 0, reg_cfg = 0, reg_sensor_mask = 0;
	unsigned int reg_status_cntl = 0, reg_thr_data = 0, i = 0;

	reg_cntl = readl_relaxed(TSENS_CNTL_ADDR);
	writel_relaxed(reg_cntl | TSENS_SW_RST, TSENS_CNTL_ADDR);

	if (tmdev->hw_type == MSM_8960 || tmdev->hw_type == MDM_9615) {
		reg_cntl |= TSENS_8960_SLP_CLK_ENA |
			(TSENS_MEASURE_PERIOD << 18) |
			TSENS_MIN_STATUS_MASK | TSENS_MAX_STATUS_MASK |
			SENSORS_EN;
		writel_relaxed(reg_cntl, TSENS_CNTL_ADDR);
	} else if (tmdev->hw_type == APQ_8064) {
		reg_cntl |= TSENS_8960_SLP_CLK_ENA |
			(TSENS_MEASURE_PERIOD << 18) |
			(((1 << tmdev->tsens_num_sensor) - 1)
					<< TSENS_SENSOR0_SHIFT);
		writel_relaxed(reg_cntl, TSENS_CNTL_ADDR);
		reg_status_cntl = readl_relaxed(TSENS_8064_STATUS_CNTL);
		reg_status_cntl |= TSENS_MIN_STATUS_MASK |
			TSENS_MAX_STATUS_MASK;
		writel_relaxed(reg_status_cntl, TSENS_8064_STATUS_CNTL);
	}

	reg_cfg = readl_relaxed(TSENS_8960_CONFIG_ADDR);
	reg_cfg = (reg_cfg & ~TSENS_8960_CONFIG_MASK) |
		(TSENS_8960_CONFIG << TSENS_8960_CONFIG_SHIFT);
	writel_relaxed(reg_cfg, TSENS_8960_CONFIG_ADDR);

	writel_relaxed((tmdev->pm_tsens_cntl & TSENS_CNTL_RESUME_MASK),
						TSENS_CNTL_ADDR);
	reg_cntl = readl_relaxed(TSENS_CNTL_ADDR);
	writel_relaxed(tmdev->pm_tsens_thr_data, TSENS_THRESHOLD_ADDR);
	reg_thr_data = readl_relaxed(TSENS_THRESHOLD_ADDR);
	if (tmdev->hw_type == MSM_8960 || tmdev->hw_type == MDM_9615)
		reg_sensor_mask = ((reg_cntl & TSENS_8960_SENSOR_MASK)
				>> TSENS_SENSOR0_SHIFT);
	else {
		reg_sensor_mask = ((reg_cntl & TSENS_8064_SENSOR_MASK)
				>> TSENS_SENSOR0_SHIFT);
	}

	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		if (reg_sensor_mask & TSENS_MASK1)
			tmdev->sensor[i].mode = THERMAL_DEVICE_ENABLED;
		reg_sensor_mask >>= 1;
	}

	enable_irq(TSENS_UPPER_LOWER_INT);
	mb();
	return 0;
}

static const struct dev_pm_ops tsens_pm_ops = {
	.suspend	= tsens_suspend,
	.resume		= tsens_resume,
};
#endif

static void tsens_disable_mode(void)
{
	unsigned int reg_cntl = 0;

	reg_cntl = readl_relaxed(TSENS_CNTL_ADDR);
	if (tmdev->hw_type == MSM_8960 || tmdev->hw_type == MDM_9615 ||
			tmdev->hw_type == APQ_8064)
		writel_relaxed(reg_cntl &
				~((((1 << tmdev->tsens_num_sensor) - 1) <<
				TSENS_SENSOR0_SHIFT) | TSENS_8960_SLP_CLK_ENA
				| TSENS_EN), TSENS_CNTL_ADDR);
	else if (tmdev->hw_type == MSM_8660)
		writel_relaxed(reg_cntl &
				~((((1 << tmdev->tsens_num_sensor) - 1) <<
				TSENS_SENSOR0_SHIFT) | TSENS_8660_SLP_CLK_ENA
				| TSENS_EN), TSENS_CNTL_ADDR);
}

static void tsens_hw_init(void)
{
	unsigned int reg_cntl = 0, reg_cfg = 0, reg_thr = 0;
	unsigned int reg_status_cntl = 0;

	reg_cntl = readl_relaxed(TSENS_CNTL_ADDR);
	writel_relaxed(reg_cntl | TSENS_SW_RST, TSENS_CNTL_ADDR);

	if (tmdev->hw_type == MSM_8960 || tmdev->hw_type == MDM_9615) {
		reg_cntl |= TSENS_8960_SLP_CLK_ENA |
			(TSENS_MEASURE_PERIOD << 18) |
			TSENS_LOWER_STATUS_CLR | TSENS_UPPER_STATUS_CLR |
			TSENS_MIN_STATUS_MASK | TSENS_MAX_STATUS_MASK |
			SENSORS_EN;
		writel_relaxed(reg_cntl, TSENS_CNTL_ADDR);
		reg_cntl |= TSENS_EN;
		writel_relaxed(reg_cntl, TSENS_CNTL_ADDR);

		reg_cfg = readl_relaxed(TSENS_8960_CONFIG_ADDR);
		reg_cfg = (reg_cfg & ~TSENS_8960_CONFIG_MASK) |
			(TSENS_8960_CONFIG << TSENS_8960_CONFIG_SHIFT);
		writel_relaxed(reg_cfg, TSENS_8960_CONFIG_ADDR);
	} else if (tmdev->hw_type == MSM_8660) {
		reg_cntl |= TSENS_8660_SLP_CLK_ENA | TSENS_EN |
			(TSENS_MEASURE_PERIOD << 16) |
			TSENS_LOWER_STATUS_CLR | TSENS_UPPER_STATUS_CLR |
			TSENS_MIN_STATUS_MASK | TSENS_MAX_STATUS_MASK |
			(((1 << tmdev->tsens_num_sensor) - 1) <<
			TSENS_SENSOR0_SHIFT);

		/* set TSENS_CONFIG bits (bits 29:28 of TSENS_CNTL) to '01';
			this setting found to be optimal. */
		reg_cntl = (reg_cntl & ~TSENS_8660_CONFIG_MASK) |
				(TSENS_8660_CONFIG << TSENS_8660_CONFIG_SHIFT);

		writel_relaxed(reg_cntl, TSENS_CNTL_ADDR);
	} else if (tmdev->hw_type == APQ_8064) {
		reg_cntl |= TSENS_8960_SLP_CLK_ENA |
			(TSENS_MEASURE_PERIOD << 18) |
			(((1 << tmdev->tsens_num_sensor) - 1)
					<< TSENS_SENSOR0_SHIFT);
		writel_relaxed(reg_cntl, TSENS_CNTL_ADDR);
		reg_status_cntl = readl_relaxed(TSENS_8064_STATUS_CNTL);
		reg_status_cntl |= TSENS_LOWER_STATUS_CLR |
			TSENS_UPPER_STATUS_CLR |
			TSENS_MIN_STATUS_MASK |
			TSENS_MAX_STATUS_MASK;
		writel_relaxed(reg_status_cntl, TSENS_8064_STATUS_CNTL);
		reg_cntl |= TSENS_EN;
		writel_relaxed(reg_cntl, TSENS_CNTL_ADDR);

		reg_cfg = readl_relaxed(TSENS_8960_CONFIG_ADDR);
		reg_cfg = (reg_cfg & ~TSENS_8960_CONFIG_MASK) |
			(TSENS_8960_CONFIG << TSENS_8960_CONFIG_SHIFT);
		writel_relaxed(reg_cfg, TSENS_8960_CONFIG_ADDR);
	}

	reg_thr |= (TSENS_LOWER_LIMIT_TH << TSENS_THRESHOLD_LOWER_LIMIT_SHIFT) |
		(TSENS_UPPER_LIMIT_TH << TSENS_THRESHOLD_UPPER_LIMIT_SHIFT) |
		(TSENS_MIN_LIMIT_TH << TSENS_THRESHOLD_MIN_LIMIT_SHIFT) |
		(TSENS_MAX_LIMIT_TH << TSENS_THRESHOLD_MAX_LIMIT_SHIFT);
	writel_relaxed(reg_thr, TSENS_THRESHOLD_ADDR);
}

static int tsens_calib_sensors8660(void)
{
	uint32_t *main_sensor_addr, sensor_shift, red_sensor_shift;
	uint32_t sensor_mask, red_sensor_mask;

	main_sensor_addr = TSENS_8660_QFPROM_ADDR;
	sensor_shift = TSENS_SENSOR_SHIFT;
	red_sensor_shift = sensor_shift + TSENS_RED_SHIFT;
	sensor_mask = TSENS_THRESHOLD_MAX_CODE << sensor_shift;
	red_sensor_mask = TSENS_THRESHOLD_MAX_CODE << red_sensor_shift;
	tmdev->sensor[TSENS_MAIN_SENSOR].calib_data =
		(readl_relaxed(main_sensor_addr) & sensor_mask)
						>> sensor_shift;
	tmdev->sensor[TSENS_MAIN_SENSOR].calib_data_backup =
				(readl_relaxed(main_sensor_addr)
			& red_sensor_mask) >> red_sensor_shift;
	if (tmdev->sensor[TSENS_MAIN_SENSOR].calib_data_backup)
			tmdev->sensor[TSENS_MAIN_SENSOR].calib_data =
			tmdev->sensor[TSENS_MAIN_SENSOR].calib_data_backup;
	if (!tmdev->sensor[TSENS_MAIN_SENSOR].calib_data) {
		pr_err("QFPROM TSENS calibration data not present\n");
		return -ENODEV;
	}

	tmdev->sensor[TSENS_MAIN_SENSOR].offset = tmdev->tsens_factor *
		TSENS_CAL_DEGC -
		tmdev->sensor[TSENS_MAIN_SENSOR].slope_mul_tsens_factor *
		tmdev->sensor[TSENS_MAIN_SENSOR].calib_data;

	tmdev->prev_reading_avail = false;
	INIT_WORK(&tmdev->sensor[TSENS_MAIN_SENSOR].work,
						notify_uspace_tsens_fn);

	return 0;
}

static int tsens_calib_sensors8960(void)
{
	uint32_t i;
	uint8_t *main_sensor_addr, *backup_sensor_addr;
	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		main_sensor_addr = TSENS_8960_QFPROM_ADDR0 + i;
		backup_sensor_addr = TSENS_8960_QFPROM_SPARE_ADDR0 + i;

		tmdev->sensor[i].calib_data = readb_relaxed(main_sensor_addr);
		tmdev->sensor[i].calib_data_backup =
			readb_relaxed(backup_sensor_addr);
		if (tmdev->sensor[i].calib_data_backup)
			tmdev->sensor[i].calib_data =
				tmdev->sensor[i].calib_data_backup;
		if (!tmdev->sensor[i].calib_data) {
			pr_err("QFPROM TSENS calibration data not present\n");
			return -ENODEV;
		}
		tmdev->sensor[i].offset = (TSENS_CAL_DEGC *
			tmdev->tsens_factor)
			- (tmdev->sensor[i].calib_data *
			tmdev->sensor[i].slope_mul_tsens_factor);
		tmdev->prev_reading_avail = false;
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
	}

	return 0;
}

static int tsens_calib_sensors(void)
{
	int rc = -ENODEV;

	if (tmdev->hw_type == MSM_8660)
		rc = tsens_calib_sensors8660();
	else if (tmdev->hw_type == MSM_8960 || tmdev->hw_type == MDM_9615 ||
		tmdev->hw_type == APQ_8064)
		rc = tsens_calib_sensors8960();

	return rc;
}

int msm_tsens_early_init(struct tsens_platform_data *pdata)
{
	int rc = 0, i;

	if (!pdata) {
		pr_err("No TSENS Platform data\n");
		return -EINVAL;
	}

	tmdev = kzalloc(sizeof(struct tsens_tm_device) +
			pdata->tsens_num_sensor *
			sizeof(struct tsens_tm_device_sensor),
			GFP_ATOMIC);
	if (tmdev == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < pdata->tsens_num_sensor; i++)
		tmdev->sensor[i].slope_mul_tsens_factor = pdata->slope[i];
	tmdev->tsens_factor = pdata->tsens_factor;
	tmdev->tsens_num_sensor = pdata->tsens_num_sensor;
	tmdev->hw_type = pdata->hw_type;

	rc = tsens_calib_sensors();
	if (rc < 0) {
		kfree(tmdev);
		tmdev = NULL;
		return rc;
	}

	if (tmdev->hw_type == APQ_8064)
		tsens_status_cntl_start = 0;
	else
		tsens_status_cntl_start = TSENS_STATUS_CNTL_OFFSET;

	tsens_hw_init();

	pr_debug("msm_tsens_early_init: done\n");

	return rc;
}

static int __devinit tsens_tm_probe(struct platform_device *pdev)
{
	int rc, i;

	if (!tmdev) {
		pr_info("%s : TSENS early init not done.\n", __func__);
		return -EFAULT;
	}

	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		char name[18];
		snprintf(name, sizeof(name), "tsens_tz_sensor%d", i);
		tmdev->sensor[i].mode = THERMAL_DEVICE_ENABLED;
		tmdev->sensor[i].sensor_num = i;
		tmdev->sensor[i].tz_dev = thermal_zone_device_register(name,
				TSENS_TRIP_NUM, &tmdev->sensor[i],
				&tsens_thermal_zone_ops, 0, 0, 0, 0);
		if (IS_ERR(tmdev->sensor[i].tz_dev)) {
			pr_err("%s: thermal_zone_device_register() failed.\n",
			__func__);
			rc = -ENODEV;
			goto fail;
		}
	}

	rc = request_irq(TSENS_UPPER_LOWER_INT, tsens_isr,
		IRQF_TRIGGER_RISING, "tsens_interrupt", tmdev);
	if (rc < 0) {
		pr_err("%s: request_irq FAIL: %d\n", __func__, rc);
		for (i = 0; i < tmdev->tsens_num_sensor; i++)
			thermal_zone_device_unregister(tmdev->sensor[i].tz_dev);
		goto fail;
	}
	INIT_WORK(&tmdev->tsens_work, tsens_scheduler_fn);

	pr_debug("%s: OK\n", __func__);
	mb();
	return 0;
fail:
	tsens_disable_mode();
	kfree(tmdev);
	tmdev = NULL;
	mb();
	return rc;
}

static int __devexit tsens_tm_remove(struct platform_device *pdev)
{
	int i;

	tsens_disable_mode();
	mb();
	free_irq(TSENS_UPPER_LOWER_INT, tmdev);
	for (i = 0; i < tmdev->tsens_num_sensor; i++)
		thermal_zone_device_unregister(tmdev->sensor[i].tz_dev);
	kfree(tmdev);
	tmdev = NULL;
	return 0;
}

static struct platform_driver tsens_tm_driver = {
	.probe = tsens_tm_probe,
	.remove = tsens_tm_remove,
	.driver = {
		.name = "tsens8960-tm",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &tsens_pm_ops,
#endif
	},
};

static int __init _tsens_tm_init(void)
{
	return platform_driver_register(&tsens_tm_driver);
}
module_init(_tsens_tm_init);

static void __exit _tsens_tm_remove(void)
{
	platform_driver_unregister(&tsens_tm_driver);
}
module_exit(_tsens_tm_remove);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM8960 Temperature Sensor driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:tsens8960-tm");
