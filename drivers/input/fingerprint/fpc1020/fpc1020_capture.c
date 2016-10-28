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
#include <linux/time.h>

#ifndef CONFIG_OF
#include <linux/spi/fpc1020_common.h>
#include <linux/spi/fpc1020_capture.h>
#else
#include "fpc1020_common.h"
#include "fpc1020_capture.h"
#endif


/* -------------------------------------------------------------------- */
/* function prototypes							*/
/* -------------------------------------------------------------------- */
static size_t fpc1020_calc_image_size(fpc1020_data_t *fpc1020);


/* -------------------------------------------------------------------- */
/* function definitions							*/
/* -------------------------------------------------------------------- */
int fpc1020_init_capture(fpc1020_data_t *fpc1020)
{
	fpc1020->capture.state = FPC1020_CAPTURE_STATE_IDLE;
	fpc1020->capture.current_mode = FPC1020_MODE_IDLE;
	fpc1020->capture.available_bytes = 0;
	fpc1020->capture.deferred_finger_up = false;

	init_waitqueue_head(&fpc1020->capture.wq_data_avail);

	return 0;
}


/* -------------------------------------------------------------------- */
int fpc1020_write_capture_setup(fpc1020_data_t *fpc1020)
{
	return fpc1020_write_sensor_setup(fpc1020);
}


/* -------------------------------------------------------------------- */
int fpc1020_write_test_setup(fpc1020_data_t *fpc1020, u16 pattern)
{
	int error = 0;
	u8 config = 0x04;
	fpc1020_reg_access_t reg;

	dev_dbg(&fpc1020->spi->dev, "%s, pattern 0x%x\n", __func__, pattern);

	error = fpc1020_write_sensor_setup(fpc1020);
	if (error)
		goto out;

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_TST_COL_PATTERN_EN, &pattern);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &config);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	error = fpc1020_capture_settings(fpc1020, 0);
	if (error)
		goto out;

out:
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_write_cb_test_setup_102x(fpc1020_data_t *fpc1020, bool invert)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u64 temp_u64;
	fpc1020_reg_access_t reg;

	temp_u16 = (invert) ? 0x55aa : 0xaa55;
	dev_dbg(&fpc1020->spi->dev, "%s, pattern 0x%x\n", __func__, temp_u16);

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_TST_COL_PATTERN_EN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x04;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = (invert) ? 0x0f1b : 0x0f0f;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = (invert) ? 0x0800 : 0x00;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x14;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x20;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x1e1e1e1e2d2d2d2d;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_write_cb_test_setup_1155(fpc1020_data_t *fpc1020, bool invert)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u64 temp_u64;
	fpc1020_reg_access_t reg;

	temp_u16 = (invert) ? 0x55aa : 0xaa55;
	dev_dbg(&fpc1020->spi->dev, "%s, pattern 0x%x\n", __func__, temp_u16);

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_TST_COL_PATTERN_EN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x14;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = (invert) ? 0x0f1a : 0x0f0e;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = (invert) ? 0x0800 : 0x0000;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x2f;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x38;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x373737373f3f3f3f;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x5540002d24;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SETUP, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;
out:
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_write_cb_test_setup_1022(fpc1020_data_t *fpc1020, bool invert)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;

	fpc1020_reg_access_t reg;

	error = fpc1020_write_sensor_setup(fpc1020);
	if (error)
		goto out;

	temp_u16 = (invert) ? 0x55aa : 0xaa55;
	dev_dbg(&fpc1020->spi->dev, "%s, pattern 0x%x\n", __func__, temp_u16);

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_TST_COL_PATTERN_EN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x04;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = (invert) ? 0x0f1b : 0x0f0f;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = (invert) ? 0x0800 : 0x00;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_write_cb_test_setup_1145(fpc1020_data_t *fpc1020, bool invert)
{
	return fpc1020_write_cb_test_setup_1155(fpc1020, invert);
}


/* -------------------------------------------------------------------- */
int fpc1020_write_cb_test_setup(fpc1020_data_t *fpc1020, bool invert)
{
	int error = 0;
	switch (fpc1020->chip.type) {
	case FPC1020_CHIP_1035X:
	case FPC1020_CHIP_1022X:
		error = fpc1020_write_cb_test_setup_1022(fpc1020, invert);
		break;
	case FPC1020_CHIP_1155X:
		error = fpc1020_write_cb_test_setup_1155(fpc1020, invert);
		break;
	case FPC1020_CHIP_1145X:
		error = fpc1020_write_cb_test_setup_1145(fpc1020, invert);
		break;
	default:
		error = fpc1020_write_cb_test_setup_102x(fpc1020, invert);
		break;
	}
	return error;
}


/* -------------------------------------------------------------------- */
bool fpc1020_capture_check_ready(fpc1020_data_t *fpc1020)
{
	fpc1020_capture_state_t state = fpc1020->capture.state;

	return (state == FPC1020_CAPTURE_STATE_IDLE) ||
		(state == FPC1020_CAPTURE_STATE_COMPLETED) ||
		(state == FPC1020_CAPTURE_STATE_FAILED);
}


/* -------------------------------------------------------------------- */
int fpc1020_capture_task(fpc1020_data_t *fpc1020)
{
	struct timespec ts_t1, ts_t2, ts_t3, ts_delta;
	int time_settings_us[FPC1020_BUFFER_MAX_IMAGES];
	int time_capture_us[FPC1020_BUFFER_MAX_IMAGES];
	int time_capture_sum_us;
	int error = 0;
	bool wait_finger_down = false;
	bool wait_finger_up = false;
	bool adjust_settings;
	fpc1020_capture_mode_t mode = fpc1020->capture.current_mode;
	int current_capture, capture_count;
	int image_offset;
	size_t image_byte_size;

	fpc1020->capture.state = FPC1020_CAPTURE_STATE_WRITE_SETTINGS;

	error = fpc1020_wake_up(fpc1020);
	if (error < 0)
		goto out_error;

	switch (mode) {
	case FPC1020_MODE_CAPTURE_AND_WAIT_FINGER_UP:
		wait_finger_up   = true;
		capture_count = fpc1020->setup.capture_count;
		adjust_settings = true;
		error = fpc1020_write_capture_setup(fpc1020);
		break;

	case FPC1020_MODE_WAIT_AND_CAPTURE:
		wait_finger_down =
		wait_finger_up   = true;

	case FPC1020_MODE_SINGLE_CAPTURE:
	case FPC1020_MODE_SINGLE_CAPTURE_CAL:
		capture_count = fpc1020->setup.capture_count;
		adjust_settings = true;
		error = fpc1020_write_capture_setup(fpc1020);
		break;

	case FPC1020_MODE_CHECKERBOARD_TEST_NORM:
		capture_count = 1;
		adjust_settings = false;
		error = fpc1020_write_cb_test_setup(fpc1020, false);
		break;

	case FPC1020_MODE_CHECKERBOARD_TEST_INV:
		capture_count = 1;
		adjust_settings = false;
		error = fpc1020_write_cb_test_setup(fpc1020, true);
		break;

	case FPC1020_MODE_BOARD_TEST_ONE:
		capture_count = 1;
		adjust_settings = false;
		error = fpc1020_write_test_setup(fpc1020, 0xffff);
		break;

	case FPC1020_MODE_BOARD_TEST_ZERO:
		capture_count = 1;
		adjust_settings = false;
		error = fpc1020_write_test_setup(fpc1020, 0x0000);
		break;

	case FPC1020_MODE_WAIT_FINGER_DOWN:
		wait_finger_down = true;
		capture_count = 0;
		adjust_settings = false;
		error = fpc1020_write_capture_setup(fpc1020);
		break;

	case FPC1020_MODE_WAIT_FINGER_UP:
		wait_finger_up = true;
		capture_count = 0;
		adjust_settings = false;
		error = fpc1020_write_capture_setup(fpc1020);
		break;

	case FPC1020_MODE_IDLE:
	default:
		capture_count = 0;
		adjust_settings = false;
		error = -EINVAL;
		break;
	}

	if (error < 0)
		goto out_error;

	error = fpc1020_capture_set_crop(fpc1020,
					fpc1020->setup.capture_col_start,
					fpc1020->setup.capture_col_groups,
					fpc1020->setup.capture_row_start,
					fpc1020->setup.capture_row_count);
	if (error < 0)
		goto out_error;

	image_byte_size = fpc1020_calc_image_size(fpc1020);

	dev_dbg(&fpc1020->spi->dev,
		"Start capture, mode %d, (%d frames)\n",
		mode,
		capture_count);

	if (!wait_finger_down)
		fpc1020->capture.deferred_finger_up = false;

	if (wait_finger_down) {
		error = fpc1020_capture_finger_detect_settings(fpc1020);
		if (error < 0)
			goto out_error;
	}

	if (wait_finger_down && fpc1020->capture.deferred_finger_up) {
		fpc1020->capture.state =
				FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_UP;

		dev_dbg(&fpc1020->spi->dev, "Waiting for (deferred) finger up\n");

		error = fpc1020_capture_wait_finger_up(fpc1020);

		if (error < 0)
			goto out_error;

		dev_dbg(&fpc1020->spi->dev, "Finger up\n");

		fpc1020->capture.deferred_finger_up = false;
	}

	if (wait_finger_down) {
		fpc1020->capture.state =
				FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_DOWN;

		error = fpc1020_capture_wait_finger_down(fpc1020);

		if (error < 0)
			goto out_error;

		dev_dbg(&fpc1020->spi->dev, "Finger down\n");

		if (mode == FPC1020_MODE_WAIT_FINGER_DOWN) {

			fpc1020->capture.available_bytes = 4;
			fpc1020->huge_buffer[0] = 'F';
			fpc1020->huge_buffer[1] = ':';
			fpc1020->huge_buffer[2] = 'D';
			fpc1020->huge_buffer[3] = 'N';
		}
	}

	current_capture = 0;
	image_offset = 0;

	fpc1020->diag.last_capture_time = 0;

	if (mode == FPC1020_MODE_SINGLE_CAPTURE_CAL) {
		error = fpc1020_capture_set_sample_mode(fpc1020, 1);
		if (error)
			goto out_error;
	}

	while (capture_count && (error >= 0)) {
		getnstimeofday(&ts_t1);

		fpc1020->capture.state = FPC1020_CAPTURE_STATE_ACQUIRE;

		dev_dbg(&fpc1020->spi->dev,
			"Capture, frame #%d \n",
			current_capture + 1);

		error =	(!adjust_settings) ? 0 :
			fpc1020_capture_settings(fpc1020, current_capture);

		if (error < 0)
			goto out_error;

		getnstimeofday(&ts_t2);

		error = fpc1020_cmd(fpc1020,
				FPC1020_CMD_CAPTURE_IMAGE,
				FPC_1020_IRQ_REG_BIT_FIFO_NEW_DATA);

		if (error < 0)
			goto out_error;

		fpc1020->capture.state = FPC1020_CAPTURE_STATE_FETCH;

		error = fpc1020_fetch_image(fpc1020,
					    fpc1020->huge_buffer,
					    image_offset,
					    image_byte_size,
					    (size_t)fpc1020->huge_buffer_size);
		if (error < 0)
			goto out_error;

		fpc1020->capture.available_bytes += (error >= 0) ?
							(int)image_byte_size : 0;
		fpc1020->capture.last_error = error;

		getnstimeofday(&ts_t3);

		ts_delta = timespec_sub(ts_t2, ts_t1);
		time_settings_us[current_capture] =
			ts_delta.tv_sec * USEC_PER_SEC +
			(ts_delta.tv_nsec / NSEC_PER_USEC);

		ts_delta = timespec_sub(ts_t3, ts_t2);
		time_capture_us[current_capture] =
			ts_delta.tv_sec * USEC_PER_SEC +
			(ts_delta.tv_nsec / NSEC_PER_USEC);

		capture_count--;
		current_capture++;
		image_offset += (int)image_byte_size;
	}

	error = fpc1020_capture_set_sample_mode(fpc1020, 2);
	if (error)
		goto out_error;

	/* Update finger_present_status after image capture */
	fpc1020_check_finger_present_raw(fpc1020);

	if (mode != FPC1020_MODE_WAIT_FINGER_UP)
		wake_up_interruptible(&fpc1020->capture.wq_data_avail);

	if (wait_finger_up) {
		fpc1020->capture.state =
				FPC1020_CAPTURE_STATE_WAIT_FOR_FINGER_UP;

		error = fpc1020_capture_finger_detect_settings(fpc1020);
		if (error < 0)
			goto out_error;

		error = fpc1020_capture_wait_finger_up(fpc1020);

		if (error == -EINTR) {
			dev_dbg(&fpc1020->spi->dev, "Finger up check interrupted\n");
			fpc1020->capture.deferred_finger_up = true;
			goto out_interrupted;

		} else if (error < 0)
			goto out_error;

		if (mode == FPC1020_MODE_WAIT_FINGER_UP) {
			fpc1020->capture.available_bytes = 4;
			fpc1020->huge_buffer[0] = 'F';
			fpc1020->huge_buffer[1] = ':';
			fpc1020->huge_buffer[2] = 'U';
			fpc1020->huge_buffer[3] = 'P';

			wake_up_interruptible(&fpc1020->capture.wq_data_avail);
		}

		dev_dbg(&fpc1020->spi->dev, "Finger up\n");
	}

out_interrupted:

	capture_count = 0;
	time_capture_sum_us = 0;

	while (current_capture) {

		current_capture--;

		dev_dbg(&fpc1020->spi->dev,
			"Frame #%d acq. time %d+%d=%d (us)\n",
			capture_count + 1,
			time_settings_us[capture_count],
			time_capture_us[capture_count],
			time_settings_us[capture_count] +
				time_capture_us[capture_count]);

		time_capture_sum_us += time_settings_us[capture_count];
		time_capture_sum_us += time_capture_us[capture_count];

		capture_count++;
	}
	fpc1020->diag.last_capture_time = time_capture_sum_us / 1000;

	dev_dbg(&fpc1020->spi->dev,
		"Total acq. time %d (us)\n", time_capture_sum_us);

out_error:
	fpc1020->capture.last_error = error;

	if (error) {
		fpc1020->capture.state = FPC1020_CAPTURE_STATE_FAILED;
		dev_err(&fpc1020->spi->dev, "%s %s %d\n", __func__,
			(error == -EINTR) ? "TERMINATED" : "FAILED", error);
	} else {
		fpc1020->capture.state = FPC1020_CAPTURE_STATE_COMPLETED;
		dev_err(&fpc1020->spi->dev, "%s OK\n", __func__);
	}
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_capture_wait_finger_down(fpc1020_data_t *fpc1020)
{
	int error;
	bool finger_down = false;

	error = fpc1020_wait_finger_present(fpc1020);

	while (!finger_down && (error >= 0)) {

		if (fpc1020->worker.stop_request)
			error = -EINTR;
		else
			error = fpc1020_check_finger_present_sum(fpc1020);

		if (error > fpc1020->setup.capture_finger_down_threshold)
			finger_down = true;
		else
			msleep(FPC1020_CAPTURE_WAIT_FINGER_DELAY_MS);
	}

	fpc1020_read_irq(fpc1020, true);

	return (finger_down) ? 0 : error;
}


/* -------------------------------------------------------------------- */
int fpc1020_capture_wait_finger_up(fpc1020_data_t *fpc1020)
{
	int error = 0;
	bool finger_up = false;

	while (!finger_up && (error >= 0)) {

		if (fpc1020->worker.stop_request)
			error = -EINTR;
		else
			error = fpc1020_check_finger_present_sum(fpc1020);

		if ((error >= 0) && (error < fpc1020->setup.capture_finger_up_threshold + 1))
			finger_up = true;
		else
			msleep(FPC1020_CAPTURE_WAIT_FINGER_DELAY_MS);
	}

	fpc1020_read_irq(fpc1020, true);

	return (finger_up) ? 0 : error;
}


/* -------------------------------------------------------------------- */
int fpc1020_capture_settings(fpc1020_data_t *fpc1020, int select)
{
	int error = 0;
	fpc1020_reg_access_t reg;

	u16 pxlCtrl;
	u16 adc_shift_gain;

	dev_dbg(&fpc1020->spi->dev, "%s #%d\n", __func__, select);

	if (select >= FPC1020_MAX_ADC_SETTINGS) {
		error = -EINVAL;
		goto out_err;
	}

	pxlCtrl = fpc1020->setup.pxl_ctrl[select];
	pxlCtrl |= FPC1020_PXL_BIAS_CTRL;

	adc_shift_gain = fpc1020->setup.adc_shift[select];
	adc_shift_gain <<= 8;
	adc_shift_gain |= fpc1020->setup.adc_gain[select];

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &pxlCtrl);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out_err;

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &adc_shift_gain);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out_err;

out_err:
	if (error)
		dev_err(&fpc1020->spi->dev, "%s Error %d\n", __func__, error);

	return error;
}

int fpc1020_capture_set_sample_mode(fpc1020_data_t *fpc1020,
						unsigned int samples)
{
	int error = 0;
	fpc1020_reg_access_t reg;

	u8 image_setup;
	u32 sample_setup;
	u8 image_rd;

	dev_dbg(&fpc1020->spi->dev, "%s samples %u\n", __func__, samples);

	switch (samples) {
	case 1:
		image_setup = 0x02 | 0x08;
		image_rd = 0x0C;
		sample_setup = 0x00FF11;
	break;
	case 2:
		image_setup = 0x03 | 0x08;
		image_rd = 0x0E;
		sample_setup = 0x00FF11;
	break;
	case 4:
		image_setup = 0x04 | 0x08;
		image_rd = 0x0E;
		sample_setup = 0x00FF33;
	break;
	case 8:
		image_setup = 0x05 | 0x08;
		image_rd = 0x0E;
		sample_setup = 0x00FF77;
	break;
	default:
		error = -EINVAL;
		goto out_err;
	break;
	}

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &image_setup);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out_err;

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMG_RD, &image_rd);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out_err;

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMG_SMPL_SETUP, &sample_setup);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out_err;

out_err:
	if (error)
		dev_err(&fpc1020->spi->dev, "%s Error %d\n", __func__, error);

	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_capture_finger_detect_settings(fpc1020_data_t *fpc1020)
{
	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	return fpc1020_capture_settings(fpc1020, FPC1020_MAX_ADC_SETTINGS - 1);
}


/* -------------------------------------------------------------------- */
static size_t fpc1020_calc_image_size(fpc1020_data_t *fpc1020)
{
	int image_byte_size = fpc1020->setup.capture_row_count *
				fpc1020->setup.capture_col_groups *
				fpc1020->chip.adc_group_size;

	dev_dbg(&fpc1020->spi->dev, "%s Rows %d->%d,Cols %d->%d (%d bytes)\n",
				__func__,
				fpc1020->setup.capture_row_start,
				fpc1020->setup.capture_row_start
				+ fpc1020->setup.capture_row_count - 1,
				fpc1020->setup.capture_col_start
					* fpc1020->chip.adc_group_size,
				(fpc1020->setup.capture_col_start
					* fpc1020->chip.adc_group_size)
				+ (fpc1020->setup.capture_col_groups *
					fpc1020->chip.adc_group_size) - 1,
				image_byte_size
				);

	return image_byte_size;
}


/* -------------------------------------------------------------------- */
int fpc1020_capture_set_crop(fpc1020_data_t *fpc1020,
					int first_column,
					int num_columns,
					int first_row,
					int num_rows)
{
	fpc1020_reg_access_t reg;
	u32 temp_u32;

	temp_u32 = first_row;
	temp_u32 <<= 8;
	temp_u32 |= num_rows;
	temp_u32 <<= 8;
	temp_u32 |= (first_column * fpc1020->chip.adc_group_size);
	temp_u32 <<= 8;
	temp_u32 |= (num_columns * fpc1020->chip.adc_group_size);

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMG_CAPT_SIZE, &temp_u32);
	return fpc1020_reg_access(fpc1020, &reg);
}


/* -------------------------------------------------------------------- */
int fpc1020_capture_buffer(fpc1020_data_t *fpc1020,
				u8 *data,
				size_t offset,
				size_t image_size_bytes)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = fpc1020_cmd(fpc1020,
			FPC1020_CMD_CAPTURE_IMAGE,
			FPC_1020_IRQ_REG_BIT_FIFO_NEW_DATA);

	if (error < 0)
		goto out_error;

	error = fpc1020_fetch_image(fpc1020,
				    data,
				    offset,
				    image_size_bytes,
				    (size_t)fpc1020->huge_buffer_size);
	if (error < 0)
		goto out_error;

	return 0;

out_error:
	dev_dbg(&fpc1020->spi->dev, "%s FAILED %d\n", __func__, error);

	return error;
}


/* -------------------------------------------------------------------- */
extern int fpc1020_capture_deferred_task(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = (fpc1020->capture.deferred_finger_up) ?
		fpc1020_capture_wait_finger_up(fpc1020) : 0;

	if (error >= 0)
		fpc1020->capture.deferred_finger_up = false;

	return error;
}


/* -------------------------------------------------------------------- */

