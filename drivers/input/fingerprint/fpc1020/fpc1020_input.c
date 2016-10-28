/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#define DEBUG

#include <linux/input.h>
#include <linux/delay.h>

#ifndef CONFIG_OF
#include <linux/spi/fpc1020_common.h>
#include <linux/spi/fpc1020_input.h>
#else
#include "fpc1020_common.h"
#include "fpc1020_input.h"
#endif


/* -------------------------------------------------------------------- */
/* function prototypes							*/
/* -------------------------------------------------------------------- */
static int fpc1020_write_lpm_setup(fpc1020_data_t *fpc1020);

static int fpc1020_wait_finger_present_lpm(fpc1020_data_t *fpc1020);


/* -------------------------------------------------------------------- */
/* driver constants							*/
/* -------------------------------------------------------------------- */
#define FPC1020_KEY_FINGER_PRESENT	KEY_WAKEUP	/* 143*/

#define FPC1020_INPUT_POLL_TIME_MS	1000u

/* -------------------------------------------------------------------- */
/* function definitions							*/
/* -------------------------------------------------------------------- */
int fpc1020_input_init(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020->input_dev = input_allocate_device();

	if (!fpc1020->input_dev) {
		dev_err(&fpc1020->spi->dev, "Input_allocate_device failed.\n");
		error  = -ENOMEM;
	}

	if (!error) {
		fpc1020->input_dev->name = FPC1020_DEV_NAME;

		set_bit(EV_KEY,		fpc1020->input_dev->evbit);

		set_bit(FPC1020_KEY_FINGER_PRESENT, fpc1020->input_dev->keybit);

		error = input_register_device(fpc1020->input_dev);
	}

	if (error) {
		dev_err(&fpc1020->spi->dev, "Input_register_device failed.\n");
		input_free_device(fpc1020->input_dev);
		fpc1020->input_dev = NULL;
	}

	return error;
}


/* -------------------------------------------------------------------- */
void fpc1020_input_destroy(fpc1020_data_t *fpc1020)
{
	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	if (fpc1020->input_dev != NULL)
		input_free_device(fpc1020->input_dev);
}


/* -------------------------------------------------------------------- */
int fpc1020_input_enable(fpc1020_data_t *fpc1020, bool enabled)
{
	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	fpc1020->input.enabled = enabled;

	return 0;
}


/* -------------------------------------------------------------------- */
int fpc1020_input_task(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	while (!fpc1020->worker.stop_request && !error) {

		error = fpc1020_wait_finger_present_lpm(fpc1020);

		if (error == 0) {
			dev_dbg(&fpc1020->spi->dev, "%s: Sending wake key event.\n", __func__);
			input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_PRESENT, 1);
			input_report_key(fpc1020->input_dev,
						FPC1020_KEY_FINGER_PRESENT, 0);

			input_sync(fpc1020->input_dev);
		}
	}

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_lpm_setup(fpc1020_data_t *fpc1020)
{
	const int mux = FPC1020_MAX_ADC_SETTINGS - 1;
	int error = 0;
	u16 temp_u16;
	fpc1020_reg_access_t reg;

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	error = fpc1020_write_sensor_setup(fpc1020);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.adc_shift[mux];
	temp_u16 <<= 8;
	temp_u16 |= fpc1020->setup.adc_gain[mux];

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.pxl_ctrl[mux];
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


/* --------------------------------------------------------------------
   This function will utilize the sensor sleep mode while waiting for
   a finger. Only the two center finger detect zones are used for
   detecting a finger. When either of the two zones trigger the sensor
   will issue an interrupt.
*/
static int fpc1020_wait_finger_present_lpm(fpc1020_data_t *fpc1020)
{
	const int zmask_5 = 1 << 5;
	const int zmask_6 = 1 << 6;
	const int zmask_ext = FPC1020_FINGER_DETECT_ZONE_MASK;

	int error = 0;
	int zone_raw = 0;

	bool wakeup_center = false;
	bool wakeup_ext    = false;
	bool wakeup        = false;

	error = fpc1020_wake_up(fpc1020);

	if (!error)
		error = fpc1020_calc_finger_detect_threshold_min(fpc1020);

	if (error >= 0)
		error = fpc1020_set_finger_detect_threshold(fpc1020, error);

	if (error >= 0)
		error = fpc1020_write_lpm_setup(fpc1020);

	if (!error) {
		error = fpc1020_sleep(fpc1020, false);

		if (error == -EAGAIN) {
			error = fpc1020_sleep(fpc1020, false);

			if (error == -EAGAIN)
				error = 0;
		}
	}

	while (!fpc1020->worker.stop_request && !error && !wakeup) {
		/* Wait for either of the finger detect zones to trigger */
		dev_dbg(&fpc1020->spi->dev, "%s, waiting for irq.\n", __func__);
		error = fpc1020_wait_for_irq(fpc1020, 0);
		if (error == -EINTR) {
			/* The wait was aborted, let the outer loop exit */
			continue;
		}
		error = fpc1020_read_irq(fpc1020, true);

		if (error & FPC_1020_IRQ_REG_BIT_FINGER_DOWN) {
			/* Wait for the finger to stabilise if a "finger" was detected */
			error = fpc1020_wait_finger_present(fpc1020);

			/* Are enough finger detect zones covered by the finger? */
			if (!error)
				error = fpc1020_check_finger_present_raw(fpc1020);

			zone_raw = (error >= 0) ? error : 0;

			if (error >= 0) {
				error = 0;

				wakeup_center = (zone_raw & zmask_5) ||
						(zone_raw & zmask_6);

			/* Todo: refined extended processing ? */
				wakeup_ext = ((zone_raw & zmask_ext) == zmask_ext);

			} else {
				wakeup_ext    = false;
				wakeup_center = false;
			}

			if (wakeup_center && wakeup_ext) {
				dev_dbg(&fpc1020->spi->dev,
					"%s Wake up !\n", __func__);
				wakeup = true;
			}
		} else {
			dev_dbg(&fpc1020->spi->dev, "%s: interrupt reg = 0x%x\n", __func__, error);
			error = 0;
		}

		if (!wakeup && !error) {
			error = fpc1020_sleep(fpc1020, false);

			if (error == -EAGAIN)
				error = 0;
		}
	}

	if (error < 0)
		dev_dbg(&fpc1020->spi->dev,
			"%s %s %d!\n", __func__,
			(error == -EINTR) ? "TERMINATED" : "FAILED", error);

	return error;
}


/* -------------------------------------------------------------------- */

