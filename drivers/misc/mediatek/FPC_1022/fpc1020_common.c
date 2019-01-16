/* FPC1020 Touch sensor driver
 *
 * Copyright (c) 2013,2014 Fingerprint Cards AB <tech@fingerprints.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#define DEBUG
#define CONFIG_OF
#define FIONA

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/clk-private.h>
#include <mach/mt_spi.h>
#include <mach/mt_gpio.h>


#ifndef CONFIG_OF
#include <linux/spi/fpc1020_common.h>
#include <linux/spi/fpc1020_regs.h>
#include <linux/spi/fpc1020_capture.h>
#include <linux/spi/fpc1020_regulator.h>
#else
#include "fpc1020_common.h"
#include "fpc1020_regs.h"
#include "fpc1020_capture.h"
#include "fpc1020_regulator.h"
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	const bool target_little_endian = true;
#else
	#warning BE target not tested!
	const bool target_little_endian = false;
#endif


/* -------------------------------------------------------------------- */
/* fpc1020 data types							*/
/* -------------------------------------------------------------------- */
struct chip_struct {
	fpc1020_chip_t type;
	u16 hwid;
	u8  revision;
	u8  pixel_rows;
	u8  pixel_columns;
	u8  adc_group_size;
	u16 spi_max_khz;
	u16 spi_reg_khz;
};


/* -------------------------------------------------------------------- */
/* fpc1020 driver constants						*/
/* -------------------------------------------------------------------- */
#define FPC1022_ROWS		88u
#define FPC1022_COLUMNS		112u

#define FPC1140_ROWS		192u
#define FPC1140_COLUMNS		56u
#define FPC1150_ROWS		208u
#define FPC1150_COLUMNS		80u

#define FPC1021_ROWS		160u
#define FPC1021_COLUMNS		160u

#define FPC1020_ROWS		192u
#define FPC1020_COLUMNS		192u
#define FPC102X_ADC_GROUP_SIZE	8u

#define FPC1020_EXT_HWID_CHECK_ID1020A_ROWS 5u


static const char *chip_text[] = {
	"N/A",		/* FPC1020_CHIP_NONE */
	"fpc1020a", 	/* FPC1020_CHIP_1020A */
	"fpc1021a", 	/* FPC1020_CHIP_1021A */
	"fpc1021b", 	/* FPC1020_CHIP_1021B */
	"fpc1150a", 	/* FPC1020_CHIP_1150A */
	"fpc1150b", 	/* FPC1020_CHIP_1150B */
	"fpc1150f", 	/* FPC1020_CHIP_1150F */
	"fpc1155x", 	/* FPC1020_CHIP_1155X */
	"fpc1140a", 	/* FPC1020_CHIP_1140A */
	"fpc1145x", 	/* FPC1020_CHIP_1145X */
	"fpc1140b", 	/* FPC1020_CHIP_1140B */
	"fpc1025x", 	/* FPC1020_CHIP_1025X */
	"fpc1022a", 	/* FPC1020_CHIP_1022A */
	"fpc1035a", 	/* FPC1020_CHIP_1035A */
	"fpc1022b", 	/* FPC1020_CHIP_1022B */
	"fpc1035b", 	/* FPC1020_CHIP_1035B */
};

static const struct chip_struct chip_data[] = {
	{FPC1020_CHIP_1022A, 0x0111, 1, FPC1022_ROWS, FPC1022_COLUMNS, FPC102X_ADC_GROUP_SIZE, 5000, 1000},
	{FPC1020_CHIP_1035A, 0x8110, 1, FPC1022_ROWS, FPC1022_COLUMNS, FPC102X_ADC_GROUP_SIZE, 5000, 1000},
	{FPC1020_CHIP_1022B, 0x0121, 1, FPC1022_ROWS, FPC1022_COLUMNS, FPC102X_ADC_GROUP_SIZE, 5000, 1000},
	{FPC1020_CHIP_1035B, 0x8120, 1, FPC1022_ROWS, FPC1022_COLUMNS, FPC102X_ADC_GROUP_SIZE, 5000, 1000},
	{FPC1020_CHIP_1025X, 0x8210, 1, FPC1021_ROWS, FPC1021_COLUMNS, FPC102X_ADC_GROUP_SIZE, 5000, 1000},
	{FPC1020_CHIP_1020A, 0x020a, 0, FPC1020_ROWS, FPC1020_COLUMNS, FPC102X_ADC_GROUP_SIZE, 8000, 1000},
	{FPC1020_CHIP_1021A, 0x021a, 2, FPC1021_ROWS, FPC1021_COLUMNS, FPC102X_ADC_GROUP_SIZE, 8000, 1000},
	{FPC1020_CHIP_1021B, 0x021b, 1, FPC1021_ROWS, FPC1021_COLUMNS, FPC102X_ADC_GROUP_SIZE, 8000, 1000},
	{FPC1020_CHIP_1150A, 0x150a, 1, FPC1150_ROWS, FPC1150_COLUMNS, FPC102X_ADC_GROUP_SIZE, 8000, 1000},
	{FPC1020_CHIP_1150B, 0x150b, 1, FPC1150_ROWS, FPC1150_COLUMNS, FPC102X_ADC_GROUP_SIZE, 8000, 1000},
	{FPC1020_CHIP_1150F, 0x150f, 1, FPC1150_ROWS, FPC1150_COLUMNS, FPC102X_ADC_GROUP_SIZE, 8000, 1000},
	{FPC1020_CHIP_1155X, 0x9500, 1, FPC1150_ROWS, FPC1150_COLUMNS, FPC102X_ADC_GROUP_SIZE, 5000, 1000},
	{FPC1020_CHIP_1140A, 0x140a, 1, FPC1140_ROWS, FPC1140_COLUMNS, FPC102X_ADC_GROUP_SIZE, 8000, 1000},
	{FPC1020_CHIP_1145X, 0x9400, 1, FPC1140_ROWS, FPC1140_COLUMNS, FPC102X_ADC_GROUP_SIZE, 5000, 1000},
	{FPC1020_CHIP_1140B, 0x140b, 1, FPC1140_ROWS, FPC1140_COLUMNS, FPC102X_ADC_GROUP_SIZE, 8000, 1000},
	
	{FPC1020_CHIP_NONE,  0,      0, 0,            0,               0,                      0,    0}
};

const fpc1020_setup_t fpc1020_setup_default_1020_a1a2 = {
	.adc_gain			= {0, 0, 1, 1},
	.adc_shift			= {1, 2, 1, 1},
	.pxl_ctrl			= {0x1b, 0x1f, 0x0b, 0x0b},
	.capture_settings_mux		= 0,
	.capture_count			= 1,
	.capture_mode			= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start		= 0,
	.capture_row_count		= FPC1020_ROWS,
	.capture_col_start		= 0,
	.capture_col_groups		= FPC1020_COLUMNS / FPC102X_ADC_GROUP_SIZE,
	.capture_finger_up_threshold	= 0,
	.capture_finger_down_threshold	= 6,
	.finger_detect_threshold	= 0x50,
	.wakeup_detect_rows		= {92, 92},
	.wakeup_detect_cols		= {64, 120},
};

const fpc1020_setup_t fpc1020_setup_default_1020_a3a4 = {
	.adc_gain			= {2, 2, 2, 2},
	.adc_shift			= {10, 10, 10, 10},
	.pxl_ctrl			= {0x1e, 0x0e, 0x0a, 0x0a},
	.capture_settings_mux		= 0,
	.capture_count			= 1,
	.capture_mode			= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start		= 0,
	.capture_row_count		= FPC1020_ROWS,
	.capture_col_start		= 0,
	.capture_col_groups		= FPC1020_COLUMNS / FPC102X_ADC_GROUP_SIZE,
	.capture_finger_up_threshold	= 0,
	.capture_finger_down_threshold	= 6,
	.finger_detect_threshold	= 0x50,
	.wakeup_detect_rows		= {92, 92},
	.wakeup_detect_cols		= {64, 120},
};

const fpc1020_setup_t fpc1020_setup_default_1021_a2b1 = {
	.adc_gain			= {2, 2, 2, 2},
	.adc_shift			= {10, 10, 10, 10},
	.pxl_ctrl			= {0x1e, 0x0e, 0x0a, 0x0a},
	.capture_settings_mux		= 0,
	.capture_count			= 1,
	.capture_mode			= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start		= 0,
	.capture_row_count		= FPC1021_ROWS,
	.capture_col_start		= 0,
	.capture_col_groups		= FPC1021_COLUMNS / FPC102X_ADC_GROUP_SIZE,
	.capture_finger_up_threshold	= 0,
	.capture_finger_down_threshold	= 6,
	.finger_detect_threshold	= 0x50,
	.wakeup_detect_rows		= {76, 76},
	.wakeup_detect_cols		= {56, 88},
};

const fpc1020_setup_t fpc1020_setup_default_1025 = {
	.adc_gain			= {2, 2, 2, 2},
	.adc_shift			= {10, 10, 10, 10},
	.pxl_ctrl			= {0x1e, 0x0e, 0x0a, 0x0a},
	.capture_settings_mux		= 0,
	.capture_count			= 1,
	.capture_mode			= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start		= 0,
	.capture_row_count		= FPC1021_ROWS,
	.capture_col_start		= 0,
	.capture_col_groups		= FPC1021_COLUMNS / FPC102X_ADC_GROUP_SIZE,
	.capture_finger_up_threshold	= 0,
	.capture_finger_down_threshold	= 6,
	.finger_detect_threshold	= 0x50,
	.wakeup_detect_rows		= {76, 76},
	.wakeup_detect_cols		= {56, 88},
};
const fpc1020_setup_t fpc1020_setup_default_1150_a1b1f1 = {
	.adc_gain			= {2, 2, 2, 2},
	.adc_shift			= {10, 10, 10, 10},
	.pxl_ctrl			= {0x1e, 0x0e, 0x0a, 0x0a},
	.capture_settings_mux		= 0,
	.capture_count			= 1,
	.capture_mode			= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start		= 0,
	.capture_row_count		= FPC1150_ROWS,
	.capture_col_start		= 0,
	.capture_col_groups		= FPC1150_COLUMNS / FPC102X_ADC_GROUP_SIZE,
	.capture_finger_up_threshold	= 0,
	.capture_finger_down_threshold	= 7,
	.finger_detect_threshold	= 0x50,
	.wakeup_detect_rows		= {72, 128},
	.wakeup_detect_cols 		= {32, 32},
};

const fpc1020_setup_t fpc1020_setup_default_1155 = {
	.adc_gain			= {2, 2, 2, 2},
	.adc_shift			= {10, 10, 10, 10},
	.pxl_ctrl			= {0x1e, 0x0e, 0x0a, 0x0a},
	.capture_settings_mux		= 0,
	.capture_count			= 1,
	.capture_mode			= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start		= 0,
	.capture_row_count		= FPC1150_ROWS,
	.capture_col_start		= 0,
	.capture_col_groups		= FPC1150_COLUMNS / FPC102X_ADC_GROUP_SIZE,
	.capture_finger_up_threshold	= 0,
	.capture_finger_down_threshold	= 7,
	.finger_detect_threshold	= 0x50,
	.wakeup_detect_rows		= {72, 128},
	.wakeup_detect_cols 		= {32, 32},
};

const fpc1020_setup_t fpc1020_setup_default_1140 = {
	.adc_gain			= {2, 2, 2, 2},
	.adc_shift			= {10, 10, 10, 10},
	.pxl_ctrl			= {0x1e, 0x0e, 0x0a, 0x0a},
	.capture_settings_mux		= 0,
	.capture_count			= 1,
	.capture_mode			= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start		= 0,
	.capture_row_count		= FPC1140_ROWS,
	.capture_col_start		= 0,
	.capture_col_groups		= FPC1140_COLUMNS / FPC102X_ADC_GROUP_SIZE,
	.capture_finger_up_threshold	= 0,
	.capture_finger_down_threshold	= 7,
	.finger_detect_threshold	= 0x50,
	.wakeup_detect_rows		= {66, 118},
	.wakeup_detect_cols 		= {24, 24},
};

const fpc1020_setup_t fpc1020_setup_default_1145 = {
	.adc_gain			= {2, 2, 2, 2},
	.adc_shift			= {10, 10, 10, 10},
	.pxl_ctrl			= {0x1e, 0x0e, 0x0a, 0x0a},
	.capture_settings_mux		= 0,
	.capture_count			= 1,
	.capture_mode			= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start		= 0,
	.capture_row_count		= FPC1140_ROWS,
	.capture_col_start		= 0,
	.capture_col_groups		= FPC1140_COLUMNS / FPC102X_ADC_GROUP_SIZE,
	.capture_finger_up_threshold	= 0,
	.capture_finger_down_threshold	= 7,
	.finger_detect_threshold	= 0x50,
	.wakeup_detect_rows		= {66, 118},
	.wakeup_detect_cols 		= {24, 24},
};

const fpc1020_setup_t fpc1020_setup_default_1022 = {
	.adc_gain			= {2, 2, 2, 2},
	.adc_shift			= {10, 10, 10, 10},
	.pxl_ctrl			= {0x1e, 0x0e, 0x0a, 0x0a},
	.capture_settings_mux		= 0,
	.capture_count			= 1,
	.capture_mode			= FPC1020_MODE_WAIT_AND_CAPTURE,
	.capture_row_start		= 0,
	.capture_row_count		= FPC1022_ROWS,
	.capture_col_start		= 0,
	.capture_col_groups		= FPC1022_COLUMNS / FPC102X_ADC_GROUP_SIZE,
	.capture_finger_up_threshold	= 0,
	.capture_finger_down_threshold	= 7,
	.finger_detect_threshold	= 0x50,
	.wakeup_detect_rows		= {40, 40},
	.wakeup_detect_cols		= {48, 64},
};

const fpc1020_diag_t fpc1020_diag_default = {
	.selftest     = 0,
	.spi_register = 0,
	.spi_regsize  = 0,
	.spi_data     = 0,
};


/* -------------------------------------------------------------------- */
/* function prototypes							*/
/* -------------------------------------------------------------------- */
static int fpc1020_check_hw_id_extended(fpc1020_data_t *fpc1020);

static int fpc1020_hwid_1020a(fpc1020_data_t *fpc1020);

static int fpc1020_write_id_1020a_setup(fpc1020_data_t *fpc1020);

static int fpc1020_write_sensor_1020a_setup(fpc1020_data_t *fpc1020);
static int fpc1020_write_sensor_1020a_a1a2_setup(fpc1020_data_t *fpc1020);
static int fpc1020_write_sensor_1020a_a3a4_setup(fpc1020_data_t *fpc1020);

static int fpc1020_write_sensor_1021_setup(fpc1020_data_t *fpc1020);

static int fpc1020_write_sensor_1025_setup(fpc1020_data_t *fpc1020);
static int fpc1020_write_sensor_1150_setup(fpc1020_data_t *fpc1020);

static int fpc1020_write_sensor_1155_setup(fpc1020_data_t *fpc1020);
static int fpc1020_write_sensor_1140_setup(fpc1020_data_t *fpc1020);

static int fpc1020_write_sensor_1145_setup(fpc1020_data_t *fpc1020);

static int fpc1020_write_sensor_1022_setup(fpc1020_data_t *fpc1020);

static int fpc1020_check_irq_after_reset(fpc1020_data_t *fpc1020);

static int fpc1020_flush_adc(fpc1020_data_t *fpc1020);

/* -------------------------------------------------------------------- */
/* function definitions							*/
/* -------------------------------------------------------------------- */
size_t fpc1020_calc_huge_buffer_minsize(fpc1020_data_t *fpc1020)
{
	const size_t buff_min = FPC1020_EXT_HWID_CHECK_ID1020A_ROWS *
				FPC1020_COLUMNS;
	size_t buff_req;

	buff_req = (fpc1020->chip.type == FPC1020_CHIP_NONE) ? buff_min :
						(fpc1020->chip.pixel_columns *
						fpc1020->chip.pixel_rows *
						FPC1020_BUFFER_MAX_IMAGES);

	return (buff_req > buff_min) ? buff_req : buff_min;
}


/* -------------------------------------------------------------------- */
int fpc1020_manage_huge_buffer(fpc1020_data_t *fpc1020, size_t new_size)
{
	int error = 0;
	int buffer_order_new, buffer_order_curr;

	buffer_order_curr = get_order(fpc1020->huge_buffer_size);
	buffer_order_new  = get_order(new_size);

	if (new_size == 0) {
		if (fpc1020->huge_buffer) {
			free_pages((unsigned long)fpc1020->huge_buffer,
							buffer_order_curr);

			fpc1020->huge_buffer = NULL;
		}
		fpc1020->huge_buffer_size = 0;
		error = 0;

	} else {
		if (fpc1020->huge_buffer &&
			(buffer_order_curr != buffer_order_new)) {

			free_pages((unsigned long)fpc1020->huge_buffer,
							buffer_order_curr);

			fpc1020->huge_buffer = NULL;
		}

		if (fpc1020->huge_buffer == NULL)
		{
			fpc1020->huge_buffer =
				(u8 *)__get_free_pages(GFP_KERNEL,
							buffer_order_new);

			fpc1020->huge_buffer_size = (fpc1020->huge_buffer) ?
				(size_t)PAGE_SIZE << buffer_order_new : 0;

			error = (fpc1020->huge_buffer_size == 0) ? -ENOMEM : 0;
		}
	}


	if (error) {
		dev_err(&fpc1020->spi->dev, "%s, failed %d\n",
							__func__, error);
	} else {
		dev_err(&fpc1020->spi->dev, "%s, size=%d bytes\n",
					__func__, (int)fpc1020->huge_buffer_size);
	}

	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_setup_defaults(fpc1020_data_t *fpc1020)
{
	int error = 0;
	const fpc1020_setup_t *ptr;

	memcpy((void *)&fpc1020->diag,
	       (void *)&fpc1020_diag_default,
	       sizeof(fpc1020_diag_t));

	switch (fpc1020->chip.type) {

	case FPC1020_CHIP_1020A:

		ptr = (fpc1020->chip.revision == 1) ? &fpc1020_setup_default_1020_a1a2 :
			(fpc1020->chip.revision == 2) ? &fpc1020_setup_default_1020_a1a2 :
			(fpc1020->chip.revision == 3) ? &fpc1020_setup_default_1020_a3a4 :
			(fpc1020->chip.revision == 4) ? &fpc1020_setup_default_1020_a3a4 :
			NULL;
		break;

	case FPC1020_CHIP_1021A:
		ptr = (fpc1020->chip.revision == 2) ? &fpc1020_setup_default_1021_a2b1 :
			NULL;
		break;

	case FPC1020_CHIP_1021B:
		ptr = (fpc1020->chip.revision == 1) ? &fpc1020_setup_default_1021_a2b1 :
			NULL;
		break;

	case FPC1020_CHIP_1025X:
		ptr = (fpc1020->chip.revision == 1) ? &fpc1020_setup_default_1025 :
			NULL;
		break;
	case FPC1020_CHIP_1150A:
	case FPC1020_CHIP_1150B:
	case FPC1020_CHIP_1150F:
		ptr = (fpc1020->chip.revision == 1) ? &fpc1020_setup_default_1150_a1b1f1 :
			NULL;
		break;

	case FPC1020_CHIP_1155X:
		ptr = (fpc1020->chip.revision == 1) ? &fpc1020_setup_default_1155 : NULL;
		break;

	case FPC1020_CHIP_1140A:
	case FPC1020_CHIP_1140B:
		ptr = (fpc1020->chip.revision == 1) ? &fpc1020_setup_default_1140 : NULL;
		break;

	case FPC1020_CHIP_1145X:
		ptr = (fpc1020->chip.revision == 1) ? &fpc1020_setup_default_1145 : NULL;
		break;

	case FPC1020_CHIP_1022A:
	case FPC1020_CHIP_1035A:
	case FPC1020_CHIP_1022B:
	case FPC1020_CHIP_1035B:
		ptr = (fpc1020->chip.revision == 1) ? &fpc1020_setup_default_1022 : NULL;		
        break;

	default:
		ptr = NULL;
		break;
	}

	error = (ptr == NULL) ? -EINVAL : 0;
	if (error)
		goto out_err;

	memcpy((void *)&fpc1020->setup,	ptr, sizeof(fpc1020_setup_t));

	dev_err(&fpc1020->spi->dev, "%s OK\n", __func__);

	return 0;

out_err:
	memset((void *)&fpc1020->setup,	0, sizeof(fpc1020_setup_t));
	dev_err(&fpc1020->spi->dev, "%s FAILED %d\n", __func__, error);

	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_gpio_reset(fpc1020_data_t *fpc1020)
{
	int error = 0;
	int counter = FPC1020_RESET_RETRIES;
//        int testcwh ;

	while (counter) {
		counter--;

#ifdef MTK_PLATFORM
		mt_set_gpio_out(fpc1020->reset_gpio,GPIO_OUT_ONE);
		udelay(FPC1020_RESET_HIGH1_US);

		mt_set_gpio_out(fpc1020->reset_gpio,GPIO_OUT_ZERO);
		udelay(FPC1020_RESET_LOW_US);
                udelay(FPC1020_RESET_LOW_US);

		mt_set_gpio_out(fpc1020->reset_gpio,GPIO_OUT_ONE);
		udelay(FPC1020_RESET_HIGH2_US);

		error = mt_get_gpio_in(fpc1020->irq_gpio) ? 0 : -EIO;
#else
		gpio_set_value(fpc1020->reset_gpio, 1);
		udelay(FPC1020_RESET_HIGH1_US);

		gpio_set_value(fpc1020->reset_gpio, 0);
		udelay(FPC1020_RESET_LOW_US);
                udelay(FPC1020_RESET_LOW_US);

		gpio_set_value(fpc1020->reset_gpio, 1);
		udelay(FPC1020_RESET_HIGH2_US);

		error = gpio_get_value(fpc1020->irq_gpio) ? 0 : -EIO;
#endif

		if (!error) {
			printk("%s OK !\n", __func__);
			counter = 0;
		} else {
			printk("%s timed out,retrying ...\n",
				__func__);

			udelay(1250);
		}
	}
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_spi_reset(fpc1020_data_t *fpc1020)
{
	int error = 0;
	int counter = FPC1020_RESET_RETRIES;

	dev_err(&fpc1020->spi->dev, "%s\n", __func__);

	while (counter) {
		counter--;

		error = fpc1020_cmd(fpc1020,
				FPC1020_CMD_SOFT_RESET,
				false);

		if (error >= 0) {
			error = fpc1020_wait_for_irq(fpc1020,
					FPC1020_DEFAULT_IRQ_TIMEOUT_MS);
		}

		if (error >= 0) {
			error = mt_get_gpio_in(fpc1020->irq_gpio) ? 0 : -EIO;

			if (!error) {
				dev_err(&fpc1020->spi->dev,
					"%s OK !\n", __func__);

				counter = 0;

			} else {
				dev_err(&fpc1020->spi->dev,
					"%s timed out,retrying ...\n",
					__func__);
			}
		}
	}
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_reset(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_err(&fpc1020->spi->dev, "%s\n", __func__);
	
 	fpc1020->soft_reset_enabled = 0;
 
	error = (fpc1020->soft_reset_enabled) ?
					fpc1020_spi_reset(fpc1020) :  fpc1020_gpio_reset(fpc1020);

	disable_irq(fpc1020->irq);
	fpc1020->interrupt_done = false;
	enable_irq(fpc1020->irq);


	error = fpc1020_check_irq_after_reset(fpc1020);

	if (error < 0)
		dev_err(&fpc1020->spi->dev,
			"CHECK IRQ after result, error= 0x%x.\n",(u8)error);
//		goto out;
#ifdef FIONA
	dev_err(&fpc1020->spi->dev,
		"CHECK IRQ after result, error= 0x%x.\n",(u8)error);
#endif

	error = (mt_get_gpio_in(fpc1020->irq_gpio) != 0) ? -EIO : 0;

	if (error)
		dev_err(&fpc1020->spi->dev, "IRQ pin, not low after clear.\n");

	error = fpc1020_read_irq(fpc1020, true);

	if (error != 0) {
		dev_err(&fpc1020->spi->dev,
			"IRQ register, expected 0x%x, got 0x%x.\n",
			0,
			(u8)error);

		error = -EIO;
	}

	if (!error && (fpc1020->chip.type == FPC1020_CHIP_1020A))
		error = fpc1020_flush_adc(fpc1020);


	fpc1020->capture.available_bytes = 0;
	fpc1020->capture.read_offset = 0;
	fpc1020->capture.read_pending_eof = false;
	
out:
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_check_hw_id(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u16 hardware_id;

	fpc1020_reg_access_t reg;
	int counter = 0;
	bool match = false;

#ifdef FIONA
	printk("---%s-----start\n",__func__);
#endif

	FPC1020_MK_REG_READ(reg, FPC102X_REG_HWID, &hardware_id);
	error = fpc1020_reg_access(fpc1020, &reg);

	if (error)
		return error;

	printk("Hardware id, detected 0x%x - forced setting 0x%x\n",
					hardware_id, fpc1020->force_hwid);

	if(fpc1020->force_hwid > 0) {

		printk("Hardware id, detected 0x%x - forced setting 0x%x\n",
					hardware_id, fpc1020->force_hwid);

		hardware_id = fpc1020->force_hwid;
			}
				else if (fpc1020->use_fpc2050) {
		dev_info(&fpc1020->spi->dev,
			"Modify hwid from 0x%x\n", hardware_id);
		hardware_id |= 0x8000;  // Mark it as none bezel
		hardware_id &= 0xFFF0;  // Ignore manufacture
	}	

	while (!match && chip_data[counter].type != FPC1020_CHIP_NONE) {
		if (chip_data[counter].hwid == hardware_id)
			match = true;
		else
			counter++;
	}

	if (match) {
		fpc1020->chip.type     = chip_data[counter].type;
		fpc1020->chip.revision = chip_data[counter].revision;

		fpc1020->chip.pixel_rows     = chip_data[counter].pixel_rows;
		fpc1020->chip.pixel_columns  = chip_data[counter].pixel_columns;
		fpc1020->chip.adc_group_size = chip_data[counter].adc_group_size;
		fpc1020->chip.spi_max_khz    = chip_data[counter].spi_max_khz;
		fpc1020->chip.spi_reg_khz    = chip_data[counter].spi_reg_khz;

		if (fpc1020->chip.revision == 0) {
			error = fpc1020_check_hw_id_extended(fpc1020);

			if (error > 0) {
				fpc1020->chip.revision = error;
				error = 0;
			}
		}

		dev_err(&fpc1020->spi->dev,
				"Hardware id: 0x%x (%s, rev.%d) \n",
						hardware_id,
						chip_text[fpc1020->chip.type],
						fpc1020->chip.revision);
	} else {
		dev_err(&fpc1020->spi->dev,
			"Hardware id mismatch: got 0x%x\n", hardware_id);

		fpc1020->chip.type = FPC1020_CHIP_NONE;
		fpc1020->chip.revision = 0;

		return -EIO;
	}



	return error;
}


/* -------------------------------------------------------------------- */
const char *fpc1020_hw_id_text(fpc1020_data_t *fpc1020)
{
	return chip_text[fpc1020->chip.type];
}


/* -------------------------------------------------------------------- */
static int fpc1020_check_hw_id_extended(fpc1020_data_t *fpc1020)
{
	int error = 0;

	if (fpc1020->chip.revision != 0) {
		return fpc1020->chip.revision;
	}

	error = (fpc1020->chip.type == FPC1020_CHIP_1020A) ?
			fpc1020_hwid_1020a(fpc1020) : -EINVAL;

	if (error < 0) {
		dev_err(&fpc1020->spi->dev,
			"%s, Unable to check chip revision %d\n",
			__func__,error);
	}

	return (error < 0) ? error : fpc1020->chip.revision;
}


/* -------------------------------------------------------------------- */
static int fpc1020_hwid_1020a(fpc1020_data_t *fpc1020)
{
	int error = 0;
	int xpos, ypos, m1, m2, count;

	const int num_rows = FPC1020_EXT_HWID_CHECK_ID1020A_ROWS;
	const int num_pixels = 32;
	const size_t image_size = num_rows * fpc1020->chip.pixel_columns;

	dev_err(&fpc1020->spi->dev, "%s\n", __func__);

	if (fpc1020->chip.type != FPC1020_CHIP_1020A)
		return -EINVAL;

	error = fpc1020_write_id_1020a_setup(fpc1020);
	if (error)
		goto out_err;

	error = fpc1020_capture_set_crop(fpc1020,
					0,
					fpc1020->chip.pixel_columns / fpc1020->chip.adc_group_size,
					0,
					num_rows);
	if (error)
		goto out_err;

	error = fpc1020_capture_buffer(fpc1020,
					fpc1020->huge_buffer,
					0,
					image_size);
	if (error)
		goto out_err;

	m1 = m2 = count = 0;

	for (ypos = 1; ypos < num_rows; ypos++) {
		for (xpos = 0; xpos < num_pixels; xpos++) {
			m1 += fpc1020->huge_buffer
				[(ypos * fpc1020->chip.pixel_columns) + xpos];

			m2 += fpc1020->huge_buffer
				[(ypos * fpc1020->chip.pixel_columns) +
					(fpc1020->chip.pixel_columns - 1 - xpos)];
			count++;
		}
	}

	m1 /= count;
	m2 /= count;

	if (fpc1020_check_in_range_u64(m1, 181, 219) &&
		fpc1020_check_in_range_u64(m2, 101, 179))
	{
		fpc1020->chip.revision = 1;
	}
	else if (fpc1020_check_in_range_u64(m1, 181, 219) &&
		fpc1020_check_in_range_u64(m2, 181, 219))
	{
		fpc1020->chip.revision = 2;
	}
	else if (fpc1020_check_in_range_u64(m1, 101, 179) &&
		fpc1020_check_in_range_u64(m2, 151, 179))
	{
		fpc1020->chip.revision = 3;
	}
	else if (fpc1020_check_in_range_u64(m1, 0, 99) &&
		fpc1020_check_in_range_u64(m2, 0, 99))
	{
		fpc1020->chip.revision = 4;
	}
	else
	{
		fpc1020->chip.revision = 0;
	}

	dev_err(&fpc1020->spi->dev, "%s m1,m2 = %d,%d %s rev=%d \n", __func__,
		m1, m2,
		(fpc1020->chip.revision == 0) ? "UNKNOWN!" : "detected",
		fpc1020->chip.revision);

out_err:
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_id_1020a_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	fpc1020_reg_access_t reg;

#ifdef DEBUG_LOG
	printk("---%s-----1\n",__func__);
#endif

	temp_u16 = 15 << 8;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0xffff;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_TST_COL_PATTERN_EN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x04;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_write_sensor_setup(fpc1020_data_t *fpc1020)
{
	switch (fpc1020->chip.type)
	{
		case FPC1020_CHIP_1020A:
		return fpc1020_write_sensor_1020a_setup(fpc1020);

		case FPC1020_CHIP_1021A:
		case FPC1020_CHIP_1021B:
		return fpc1020_write_sensor_1021_setup(fpc1020);

		case FPC1020_CHIP_1025X:
		return fpc1020_write_sensor_1025_setup(fpc1020);
		case FPC1020_CHIP_1150A:
		case FPC1020_CHIP_1150B:
		case FPC1020_CHIP_1150F:
		return fpc1020_write_sensor_1150_setup(fpc1020);

		case FPC1020_CHIP_1155X:
		return fpc1020_write_sensor_1155_setup(fpc1020);
		case FPC1020_CHIP_1140A:
		case FPC1020_CHIP_1140B:
		return fpc1020_write_sensor_1140_setup(fpc1020);

		case FPC1020_CHIP_1145X:
		return fpc1020_write_sensor_1145_setup(fpc1020);

		case FPC1020_CHIP_1022A:
		case FPC1020_CHIP_1035A:
		case FPC1020_CHIP_1022B:
		case FPC1020_CHIP_1035B:
		return fpc1020_write_sensor_1022_setup(fpc1020);

		default:
		break;
	}

	return -EINVAL;
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1020a_setup(fpc1020_data_t *fpc1020)
{
	switch (fpc1020->chip.revision)
	{
		case 1:
		case 2:
		return fpc1020_write_sensor_1020a_a1a2_setup(fpc1020);

		case 3:
		case 4:
		return fpc1020_write_sensor_1020a_a3a4_setup(fpc1020);

		default:
		break;
	}

	return -EINVAL;
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1020a_a1a2_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u32 temp_u32;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = FPC1020_MAX_ADC_SETTINGS - 1;
	const int rev = fpc1020->chip.revision;

	dev_err(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if ((rev != 1) && (rev != 2))
		return -EINVAL;

	temp_u64 = (rev == 1) ?	0x363636363f3f3f3f : 0x141414141e1e1e1e;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (rev == 1) ?	0x33 : 	0x0f;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (rev == 1) ? 0x37 : 0x15;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x5540003f24;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SETUP, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u32 = 0x00080000;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ANA_TEST_MUX, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x5540003f34;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SETUP, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (fpc1020->vddtx_mv > 0) ? 0x02 :	/* external supply */
		(fpc1020->txout_boost) ? 0x22 : 0x12;	/* internal supply */
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
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
	temp_u16 |= FPC1020_PXL_BIAS_CTRL;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = fpc1020->setup.finger_detect_threshold;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_THRES, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0x00ff;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_CNTR, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1020a_a3a4_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = FPC1020_MAX_ADC_SETTINGS - 1;
	const int rev = fpc1020->chip.revision;

	dev_err(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 4)
	{
		temp_u8 = 0x09;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

		temp_u64 = 0x0808080814141414;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;
	}
	else if (rev == 3)
	{
		temp_u64 = 0x1717171723232323;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

		temp_u8 = 0x0f;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;

		temp_u8 = 0x18;
		FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			goto out;
	}
	else
	{
		error = -EINVAL;
		goto out;
	}

	temp_u8 = (fpc1020->vddtx_mv > 0) ? 0x02 :	/* external supply */
		(fpc1020->txout_boost) ? 0x22 : 0x12;	/* internal supply */
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
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
	temp_u16 |= FPC1020_PXL_BIAS_CTRL;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = fpc1020->setup.finger_detect_threshold;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_THRES, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0x00ff;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_CNTR, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1021_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = FPC1020_MAX_ADC_SETTINGS - 1;
	const int rev = fpc1020->chip.revision;

	dev_err(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 0)
		return -EINVAL;

	temp_u8 = 0x09;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x0808080814141414;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (fpc1020->vddtx_mv > 0) ? 0x02 :	/* external supply */
		(fpc1020->txout_boost) ? 0x22 : 0x12;	/* internal supply */
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
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
	temp_u16 |= FPC1020_PXL_BIAS_CTRL;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = fpc1020->setup.finger_detect_threshold;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_THRES, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0x00ff;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_CNTR, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1025_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = FPC1020_MAX_ADC_SETTINGS - 1;
	const int rev = fpc1020->chip.revision;

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 0)
		return -EINVAL;

	temp_u8 = 0x37;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x363636363D3D3D3D;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	if (fpc1020->vddtx_mv > 0)
		dev_err(&fpc1020->spi->dev, "%s Ignoring external TxOut setting\n", __func__);

	if (fpc1020->txout_boost)
		dev_err(&fpc1020->spi->dev, "%s Ignoring TxOut boost setting\n", __func__);

	temp_u16 = fpc1020->setup.adc_shift[mux];
	temp_u16 <<= 8;
	temp_u16 |= fpc1020->setup.adc_gain[mux];

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SHIFT_GAIN, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = fpc1020->setup.pxl_ctrl[mux];
	temp_u16 |= FPC1020_PXL_BIAS_CTRL;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = fpc1020->setup.finger_detect_threshold;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_THRES, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u16 = 0x00ff;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_CNTR, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x2E;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}

/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1150_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u32 temp_u32;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = FPC1020_MAX_ADC_SETTINGS - 1;
	const int rev = fpc1020->chip.revision;

	dev_err(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 0)
		return -EINVAL;

	temp_u8 = 0x09;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x0808080814141414;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (fpc1020->vddtx_mv > 0) ? 0x02 :	/* external supply */
		(fpc1020->txout_boost) ? 0x22 : 0x12;	/* internal supply */
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
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
	temp_u16 |= FPC1020_PXL_BIAS_CTRL;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u32 = 0x0001; /* fngrUpSteps */
	temp_u32 <<= 8;
	temp_u32 |= fpc1020->setup.finger_detect_threshold; /* fngrLstThr */
	temp_u32 <<= 8;
	temp_u32 |= fpc1020->setup.finger_detect_threshold; /* fngrDetThr */
	FPC1020_MK_REG_WRITE(reg, FPC1150_REG_FNGR_DET_THRES, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u32 = 0x190100ff;
	FPC1020_MK_REG_WRITE(reg, FPC1150_REG_FNGR_DET_CNTR, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1155_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u32 temp_u32;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = FPC1020_MAX_ADC_SETTINGS - 1;
	const int rev = fpc1020->chip.revision;

	dev_err(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 0)
		return -EINVAL;

	temp_u8 = 0x38;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x373737373F3F3F3F;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	if (fpc1020->vddtx_mv > 0)
		dev_err(&fpc1020->spi->dev, "%s Ignoring external TxOut setting\n", __func__);

	if (fpc1020->txout_boost)
		dev_err(&fpc1020->spi->dev, "%s Ignoring TxOut boost setting\n", __func__);

	temp_u8 = 0x12; /* internal supply, no boost */
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
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
	temp_u16 |= FPC1020_PXL_BIAS_CTRL;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u32 = 0x0001; /* fngrUpSteps */
	temp_u32 <<= 8;
	temp_u32 |= fpc1020->setup.finger_detect_threshold; /* fngrLstThr */
	temp_u32 <<= 8;
	temp_u32 |= fpc1020->setup.finger_detect_threshold; /* fngrDetThr */
	FPC1020_MK_REG_WRITE(reg, FPC1150_REG_FNGR_DET_THRES, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u32 = 0x1901ffff;
	FPC1020_MK_REG_WRITE(reg, FPC1150_REG_FNGR_DET_CNTR, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x5540002D24;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SETUP, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x2F;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1140_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u32 temp_u32;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = FPC1020_MAX_ADC_SETTINGS - 1;
	const int rev = fpc1020->chip.revision;

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 0)
		return -EINVAL;

	temp_u8 = 0x09;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x0808080814141414;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = (fpc1020->vddtx_mv > 0) ? 0x02 :	/* external supply */
		(fpc1020->txout_boost) ? 0x22 : 0x12;	/* internal supply */
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
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
	temp_u16 |= FPC1020_PXL_BIAS_CTRL;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u32 = 0x0001; /* fngrUpSteps */
	temp_u32 <<= 8;
	temp_u32 |= fpc1020->setup.finger_detect_threshold; /* fngrLstThr */
	temp_u32 <<= 8;
	temp_u32 |= fpc1020->setup.finger_detect_threshold; /* fngrDetThr */
	FPC1020_MK_REG_WRITE(reg, FPC1150_REG_FNGR_DET_THRES, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u32 = 0x190100ff;
	FPC1020_MK_REG_WRITE(reg, FPC1150_REG_FNGR_DET_CNTR, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1145_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u32 temp_u32;
	u64 temp_u64;
	fpc1020_reg_access_t reg;
	const int mux = FPC1020_MAX_ADC_SETTINGS - 1;
	const int rev = fpc1020->chip.revision;

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	if (rev == 0)
		return -EINVAL;

	temp_u8 = 0x38;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x373737373F3F3F3F;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_SAMPLE_PX_DLY, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	if (fpc1020->vddtx_mv > 0)
		dev_err(&fpc1020->spi->dev, "%s Ignoring external TxOut setting\n", __func__);

	if (fpc1020->txout_boost)
		dev_err(&fpc1020->spi->dev, "%s Ignoring TxOut boost setting\n", __func__);

	temp_u8 = 0x12; /* internal supply, no boost */
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
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
	temp_u16 |= FPC1020_PXL_BIAS_CTRL;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x03 | 0x08;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u32 = 0x0001; /* fngrUpSteps */
	temp_u32 <<= 8;
	temp_u32 |= fpc1020->setup.finger_detect_threshold; /* fngrLstThr */
	temp_u32 <<= 8;
	temp_u32 |= fpc1020->setup.finger_detect_threshold; /* fngrDetThr */
	FPC1020_MK_REG_WRITE(reg, FPC1150_REG_FNGR_DET_THRES, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u32 = 0x1901ffff;
	FPC1020_MK_REG_WRITE(reg, FPC1150_REG_FNGR_DET_CNTR, &temp_u32);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u64 = 0x5540002D24;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SETUP, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x2F;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1022_setup(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 temp_u8;
	u16 temp_u16;
	u64 temp_u64;
	uint8_t temp_buf[16];
	fpc1020_reg_access_t reg;
	const int mux = FPC1020_MAX_ADC_SETTINGS - 1;

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

	temp_u8 = (fpc1020->vddtx_mv > 0) ? 0x02 :	/* external supply */
		(fpc1020->txout_boost) ? 0x22 : 0x12;	/* internal supply */
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

 	temp_u64 = 0x0a0a0a0a00000000;
 	FPC1020_MK_REG_WRITE(reg, FPC1022_REG_FINGER_DRIVE_DLY, &temp_u64);
 	error = fpc1020_reg_access(fpc1020, &reg);
 	if (error)
 		goto out;

	temp_buf[8] = 0x01;
	temp_buf[7] = 0x08;
	temp_buf[6] = 0x08;
	temp_buf[5] = 0x09;
	temp_buf[4] = 0x09;
	temp_buf[3] = 0x12;
	temp_buf[2] = 0x12;
	temp_buf[1] = 0x13;
	temp_buf[0] = 0x13;
 	FPC1020_MK_REG_WRITE(reg, FPC1022_REG_SAMPLE_PX_DLY, &temp_buf);
 	error = fpc1020_reg_access(fpc1020, &reg);
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
 	temp_u16 |= FPC1020_PXL_BIAS_CTRL;
 	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_CTRL, &temp_u16);
 	error = fpc1020_reg_access(fpc1020, &reg);
 	if (error)
 		goto out;

 	temp_u8 = 0x03 | 0x08;
 	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_IMAGE_SETUP, &temp_u8);
 	error = fpc1020_reg_access(fpc1020, &reg);
 	if (error)
 		goto out;

	temp_buf[9] = 0x03;
	temp_buf[8] = 0xe9;
	temp_buf[7] = 0xbf;
	temp_buf[6] = 0x37;
	temp_buf[5] = 0xfe;
	temp_buf[4] = 0x05;
	temp_buf[3] = 0x19;
	temp_buf[2] = 0x32;
	temp_buf[1] = 0x9b;
	temp_buf[0] = 0x66;
 	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_WEIGHT_TABLE, &temp_buf);
 	error = fpc1020_reg_access(fpc1020, &reg);
 	if (error)
 		goto out;

	temp_buf[9] = 0x1f;
	temp_buf[8] = 0x1f;
	temp_buf[7] = 0x1f;
	temp_buf[6] = 0x1f;
	temp_buf[5] = 0x00;
	temp_buf[4] = 0xaa;
	temp_buf[3] = 0x20;
	temp_buf[2] = 0x14;
	temp_buf[1] = 0x20;
	temp_buf[0] = 0x20;
 	FPC1020_MK_REG_WRITE(reg, FPC1022_REG_FNGR_DET_THRES, &temp_buf);
 	error = fpc1020_reg_access(fpc1020, &reg);
 	if (error)
 		goto out;

 	temp_u64 = 0x190100ff;
 	FPC1020_MK_REG_WRITE(reg, FPC1022_REG_FNGR_DET_CNTR, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
 	if (error)
 		goto out;

	temp_u64 = 0x5540003f00;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_ADC_SETUP, &temp_u64);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	temp_u8 = 0x00;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

out:
	return error;
}
static int fpc1020_check_irq_after_reset(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 irq_status;


	fpc1020_reg_access_t reg_clear = {
		.reg = FPC102X_REG_READ_INTERRUPT_WITH_CLEAR,
		.write = false,
		.reg_size = FPC1020_REG_SIZE(
					FPC102X_REG_READ_INTERRUPT_WITH_CLEAR),
		.dataptr = &irq_status
	};

	error = fpc1020_reg_access(fpc1020, &reg_clear);

	if(error < 0) {
#ifdef FIONA
		dev_err(&fpc1020->spi->dev,
			"fpc1020_reg_access error= 0x%x.\n",
			error);
#endif
		return error;
}

	if (irq_status != FPC_1020_IRQ_REG_BITS_REBOOT) {
		dev_err(&fpc1020->spi->dev,
			"IRQ register, expected 0x%x, got 0x%x.\n",
			FPC_1020_IRQ_REG_BITS_REBOOT,
			irq_status);

		error = -EIO;
	}
#ifdef FIONA
		dev_err(&fpc1020->spi->dev,
			"IRQ register, expected 0x%x, got 0x%x.\n",
			FPC_1020_IRQ_REG_BITS_REBOOT,
			irq_status);
#endif

	return (error < 0) ? error : irq_status;
}


/* -------------------------------------------------------------------- */
int fpc1020_wait_for_irq(fpc1020_data_t *fpc1020, int timeout)
{
	int result = 0;

#ifdef DEBUG_LOG
	printk("----%s..timeout=%d\n",__func__,timeout);
#endif

	if (!timeout) {
		result = wait_event_interruptible(
				fpc1020->wq_irq_return,
				fpc1020->interrupt_done);
	} else {
		result = wait_event_interruptible_timeout(
				fpc1020->wq_irq_return,
				fpc1020->interrupt_done, timeout);
	}

	if (result < 0) {
		dev_err(&fpc1020->spi->dev,
			 "wait_event_interruptible interrupted by signal.\n");

		return result;
	}

	if (result || !timeout) {
		fpc1020->interrupt_done = false;
		return 0;
	}

	return -ETIMEDOUT;
}


/* -------------------------------------------------------------------- */
int fpc1020_read_irq(fpc1020_data_t *fpc1020, bool clear_irq)
{
	int error = 0;
	u8 irq_status;
	fpc1020_reg_access_t reg_read = {
		.reg = FPC102X_REG_READ_INTERRUPT,
		.write = false,
		.reg_size = FPC1020_REG_SIZE(FPC102X_REG_READ_INTERRUPT),
		.dataptr = &irq_status
	};

	fpc1020_reg_access_t reg_clear = {
		.reg = FPC102X_REG_READ_INTERRUPT_WITH_CLEAR,
		.write = false,
		.reg_size = FPC1020_REG_SIZE(
					FPC102X_REG_READ_INTERRUPT_WITH_CLEAR),
		.dataptr = &irq_status
	};


#ifdef FIONA
	printk("------------%s start\n",__func__);
#endif
	
	error = fpc1020_reg_access(fpc1020,
				(clear_irq) ? &reg_clear : &reg_read);	

	if(error < 0)
		return error;

	if (irq_status == FPC_1020_IRQ_REG_BITS_REBOOT) {

		dev_err(&fpc1020->spi->dev,
			"%s: unexpected irq_status = 0x%x\n"
			, __func__, irq_status);

		error = -EIO;
	}
#ifdef FIONA
	printk("------------%s --over ,error =%d irq_status=%d\n",__func__,error,irq_status);
#endif
	return (error < 0) ? error : irq_status;
}


/* -------------------------------------------------------------------- */
int fpc1020_read_status_reg(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 status;
	/* const */ fpc1020_reg_access_t reg_read = {
		.reg = FPC102X_REG_FPC_STATUS,
		.write = false,
		.reg_size = FPC1020_REG_SIZE(FPC102X_REG_FPC_STATUS),
		.dataptr = &status
	};

	error = fpc1020_reg_access(fpc1020, &reg_read);

	return (error < 0) ? error : status;
}

/* -------------------------------------------------------------------- */
#include <cust_gpio_usage.h>
#include <mach/mt_gpio.h>
#include <mach/mt_clkmgr.h>
extern struct mt_chip_conf spi_conf_mt65xx;

static void mtspi_set_mode(fpc1020_data_t *fpc1020,int mode)
{
	struct mt_chip_conf* spi_par;
	spi_par =&spi_conf_mt65xx;
	if(!spi_par)	{

	{
		printk("spi config fail\n");
		return;
	}

	if(1 == mode)
		if(spi_par->com_mod == DMA_TRANSFER) {
			return;
		}
		
		spi_par->com_mod = DMA_TRANSFER;
	}
	else
	{
		if(spi_par->com_mod == FIFO_TRANSFER) {
			return;
		}
		
		spi_par->com_mod = FIFO_TRANSFER;
	}
	
	if(spi_setup(fpc1020->spi))
	{
		printk("spi_setup  fail\n");
	}
	
		
	return;
}

/* -------------------------------------------------------------------- */

int fpc1020_reg_access(fpc1020_data_t *fpc1020,
		      fpc1020_reg_access_t *reg_data)
{
	int error = 0;

	u8 temp_buffer[FPC1020_REG_MAX_SIZE];
	int cmd_size = 1 + FPC1020_REG_ACCESS_DUMMY_BYTES(reg_data->reg);

	temp_buffer[0] = reg_data->reg;
	
	struct spi_message msg;

	struct spi_transfer cmd = {
		.cs_change = 0,
		.delay_usecs = 0,
		.speed_hz = FPC1020_SPI_CLOCK_SPEED,
		.tx_buf = temp_buffer,
		.rx_buf = (!reg_data->write) ? temp_buffer : NULL,
		.len    = cmd_size + reg_data->reg_size,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 0,
	};

	
	if (reg_data->reg_size > sizeof(temp_buffer)) {
		dev_err(&fpc1020->spi->dev,
			"%s : illegal register size\n",
			__func__);

		error = -ENOMEM;
		goto out;
	}

	//if (gpio_is_valid(fpc1020->cs_gpio))
	//	gpio_set_value(fpc1020->cs_gpio, 0);

	if (reg_data->write) {
		if (target_little_endian) {
			int src = 0;
			int dst = reg_data->reg_size - 1;

			while (src < reg_data->reg_size) {
				temp_buffer[dst+cmd_size] = reg_data->dataptr[src];
				src++;
				dst--;
			}
		} else {
			memcpy(temp_buffer+cmd_size,
				reg_data->dataptr,
				reg_data->reg_size);
		}
	}

	mtspi_set_mode(fpc1020,0);
	spi_message_init(&msg);
	spi_message_add_tail(&cmd,  &msg);

	error = spi_sync(fpc1020->spi, &msg);
	disable_clock(MT_CG_PERI_SPI0, "spi");
	if (error)
		dev_err(&fpc1020->spi->dev, "%s : spi_sync failed.\n", __func__);

	if (!reg_data->write) {
		if (target_little_endian) {
			int src = reg_data->reg_size - 1;
			int dst = 0;

			while (dst < reg_data->reg_size) {
				reg_data->dataptr[dst] = temp_buffer[src+cmd_size];
				src--;
				dst++;
			}
		} else {
			memcpy(reg_data->dataptr,
				temp_buffer+cmd_size,
				reg_data->reg_size);
		}
	}

#ifdef LOGXX
	printk(
		"%s %s 0x%x/%dd (%d bytes) %x %x %x %x : %x %x %x %x..rx_len=%d\n",
		 __func__,
		(reg_data->write) ? "WRITE" : "READ",
		reg_data->reg,
		reg_data->reg,
		reg_data->reg_size,
		temp_buffer[0],temp_buffer[1],temp_buffer[2],temp_buffer[3],
		temp_buffer[4],temp_buffer[5],temp_buffer[6],temp_buffer[7],
		cmd_size + reg_data->reg_size);
#endif
out:
	return error;
}

/* -------------------------------------------------------------------- */
int fpc1020_cmd(fpc1020_data_t *fpc1020,
			fpc1020_cmd_t cmd,
			u8 wait_irq_mask)
{
	int error = 0;
	struct spi_message msg;

	struct spi_transfer t = {
		.cs_change = 1,
		.delay_usecs = 0,
		.speed_hz = FPC1020_SPI_CLOCK_SPEED,//(u32)fpc1020->spi_freq_khz * 1000u,
		.tx_buf = &cmd,
		.rx_buf = NULL,
		.len    = 1,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 0,
	};
	
#ifdef DEBUG_LOG
	printk("-----%s start\n",__func__);
#endif

	spi_message_init(&msg);
	spi_message_add_tail(&t,  &msg);

	error = spi_sync(fpc1020->spi, &msg);

	if (error)
		dev_err(&fpc1020->spi->dev, "spi_sync failed.\n");

#ifdef DEBUG_LOG
	printk("-----%s 22 error=%d..wait_irq_mask=%d\n",__func__,error,wait_irq_mask);
#endif

	if ((error >= 0) && wait_irq_mask) {
		error = fpc1020_wait_for_irq(fpc1020,
					FPC1020_DEFAULT_IRQ_TIMEOUT_MS);

#ifdef FIONA
		printk("-----%s 33333..error=%d\n",__func__,error);
#endif

		if (error >= 0)
			error = fpc1020_read_irq(fpc1020, true);
		
#ifdef FIONA
		printk("-----%s 444..error=%d\n",__func__,error);
#endif
	}

#ifdef DEBUG_LOG
	printk("-----%s--error=%d end\n",__func__,error);
#endif

	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_wait_finger_present(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_err(&fpc1020->spi->dev, "%s\n", __func__);
        //dump_stack();

	error = fpc1020_read_irq(fpc1020, true);
	if (error < 0)
		return error;

	error = fpc1020_cmd(fpc1020,
			FPC1020_CMD_WAIT_FOR_FINGER_PRESENT, 0);

	if (error < 0)
		return error;

	while (1) {
		
		printk("------%s\n",__func__);
                //dump_stack();
		error = fpc1020_wait_for_irq(fpc1020,
						FPC1020_DEFAULT_IRQ_TIMEOUT_MS);

		if (error >= 0) {
			error = fpc1020_read_irq(fpc1020, true);
			if (error < 0)
				return error;

			if (error &
				(FPC_1020_IRQ_REG_BIT_FINGER_DOWN |
				FPC_1020_IRQ_REG_BIT_COMMAND_DONE)) {

				dev_err(&fpc1020->spi->dev, "Finger down\n");

				error = 0;
			} else {
				dev_err(&fpc1020->spi->dev,
					"%s Unexpected IRQ = %d\n", __func__,
					error);

				error = -EIO;
			}
			return error;
		}

		if (error < 0) {
			if (fpc1020->worker.stop_request)
				return -EINTR;
			if (error != -ETIMEDOUT)
				return error;
		}
	}
}

/* -------------------------------------------------------------------- */
int fpc1020_get_finger_present_status(fpc1020_data_t *fpc1020)
{
	int status;

	if (!down_trylock(&fpc1020->mutex)) {
		if (!down_trylock(&fpc1020->worker.sem_idle)) {
			status = fpc1020_capture_finger_detect_settings(fpc1020);
			status = fpc1020_check_finger_present_raw(fpc1020);

			up(&fpc1020->worker.sem_idle);
		} else {
			/* Return last recorded status */
			status = (int)fpc1020->diag.finger_present_status;
		}
		up(&fpc1020->mutex);
	} else {
		/* Return last recorded status */
		status = (int)fpc1020->diag.finger_present_status;
	}
	return status;
}

/* -------------------------------------------------------------------- */
int fpc1020_check_finger_present_raw(fpc1020_data_t *fpc1020)
{
	fpc1020_reg_access_t reg;
	u16 temp_u16;
	int error = 0;

	error = fpc1020_read_irq(fpc1020, true);
	if (error < 0)
		return error;

	error = fpc1020_cmd(fpc1020,
			FPC1020_CMD_FINGER_PRESENT_QUERY,
			FPC_1020_IRQ_REG_BIT_COMMAND_DONE);

	if (error < 0)
		return error;

	/* There should most likely be a unique function for 1022 since the functionality
	   has changed and with it the register size 
	*/
	if( (fpc1020->chip.type != FPC1020_CHIP_1022A) &&
	    (fpc1020->chip.type != FPC1020_CHIP_1035A) && 
	    (fpc1020->chip.type != FPC1020_CHIP_1022B) &&
	    (fpc1020->chip.type != FPC1020_CHIP_1035B) )
	{
		FPC1020_MK_REG_READ(reg, FPC102X_REG_FINGER_PRESENT_STATUS, &temp_u16);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			return error;

		fpc1020->diag.finger_present_status = temp_u16;
	}
	else
	{
		uint32_t reg_value;
		FPC1020_MK_REG_READ(reg, FPC1022_REG_FINGER_PRESENT_STATUS, &reg_value);
		error = fpc1020_reg_access(fpc1020, &reg);
		if (error)
			return error;


		temp_u16 = (u16)(reg_value & 0x0fff);
		fpc1020->diag.finger_present_status = temp_u16;
	}

	//dev_dbg(&fpc1020->spi->dev, "%s zonedata = 0x%x\n", __func__, temp_u16);
	/*
	FPC1020_MK_REG_READ(reg, FPC102X_REG_FINGER_PRESENT_STATUS, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		return error;

	fpc1020->diag.finger_present_status = temp_u16;
	*/

	return temp_u16;
}


/* -------------------------------------------------------------------- */
int fpc1020_check_finger_present_sum(fpc1020_data_t *fpc1020)
{
	int zones = 0;
	u16 mask = FPC1020_FINGER_DETECT_ZONE_MASK;
	u8 count = 0;

	zones = fpc1020_check_finger_present_raw(fpc1020);

	if (zones < 0)
		return zones;
	else {
		zones &= mask;
		while (zones && mask) {
			count += (zones & 1) ? 1 : 0;
			zones >>= 1;
			mask >>= 1;
		}
		return (int)count;
	}
}


/* -------------------------------------------------------------------- */
int fpc1020_wake_up(fpc1020_data_t *fpc1020)
{
	const fpc1020_status_reg_t status_mask = FPC1020_STATUS_REG_MODE_MASK;

	int power  = 0;//fpc1020_regulator_set(fpc1020, true);
	int reset  = fpc1020_reset(fpc1020);
	int status = fpc1020_read_status_reg(fpc1020);


	printk("---%s--start\n",__func__);

	if (power == 0 && reset == 0 && status >= 0 &&
		(fpc1020_status_reg_t)(status & status_mask) ==
		FPC1020_STATUS_REG_IN_IDLE_MODE) {

		dev_err(&fpc1020->spi->dev, "%s OK\n", __func__);

		return 0;
	} else {

           printk("reset=%d  read_status_reg=%d\n",reset,status);
		dev_err(&fpc1020->spi->dev, "%s FAILED\n", __func__);

		return -EIO;
	}
}


/* -------------------------------------------------------------------- */
int fpc1020_sleep(fpc1020_data_t *fpc1020, bool deep_sleep)
{
	const char *str_deep = "deep";
	const char *str_regular = "regular";

	const fpc1020_status_reg_t status_mask = FPC1020_STATUS_REG_MODE_MASK;

	int error = fpc1020_cmd(fpc1020,
				(deep_sleep) ? FPC1020_CMD_ACTIVATE_DEEP_SLEEP_MODE :
						FPC1020_CMD_ACTIVATE_SLEEP_MODE,
				0);
	bool sleep_ok;

	int retries = FPC1020_SLEEP_RETRIES;

	if (error) {
		dev_err(&fpc1020->spi->dev,
			"%s %s command failed %d\n", __func__,
			(deep_sleep)? str_deep : str_regular,
			error);

		return error;
	}

	error = 0;
	sleep_ok = false;

	while (!sleep_ok && retries && (error >= 0)) {

		error = fpc1020_read_status_reg(fpc1020);

		if (error < 0) {
			dev_err(&fpc1020->spi->dev,
				"%s %s read status failed %d\n", __func__,
				(deep_sleep)? str_deep : str_regular,
				error);
		} else {
			error &= status_mask;
			sleep_ok = (deep_sleep) ?
				error == FPC1020_STATUS_REG_IN_DEEP_SLEEP_MODE :
				error == FPC1020_STATUS_REG_IN_SLEEP_MODE;
		}
		if (!sleep_ok) {
			udelay(FPC1020_SLEEP_RETRY_TIME_US);
			retries--;
		}
	}

	if (deep_sleep && sleep_ok && gpio_is_valid(fpc1020->reset_gpio))
		mt_set_gpio_out(fpc1020->reset_gpio, 0);

	if (sleep_ok) {
		dev_err(&fpc1020->spi->dev,
			"%s %s OK\n", __func__,
			(deep_sleep)? str_deep : str_regular);
		return 0;
	} else {
		dev_err(&fpc1020->spi->dev,
			"%s %s FAILED\n", __func__,
			(deep_sleep)? str_deep : str_regular);

		return (deep_sleep) ? -EIO : -EAGAIN;
	}
}

/* -------------------------------------------------------------------- */
int fpc1020_fetch_image(fpc1020_data_t *fpc1020,
				u8 *buffer,
				int offset,
				size_t image_size_bytes,
				size_t buff_size)
{
	int error = 0;
	int length = (int)image_size_bytes+2;
	int rest_length = 0;
	struct spi_message msg;
	const u8 tx_data[2] = {FPC1020_CMD_READ_IMAGE , 0};
	struct mt_chip_conf *chip_config = NULL;

	dev_err(&fpc1020->spi->dev, "%s (+%d)\n", __func__, offset);

#ifdef DEBUG_LOG
	printk("--------%s------1\n",__func__);
#endif

	if ((offset + (int)image_size_bytes) > buff_size) {
		dev_err(&fpc1020->spi->dev,
			"Image buffer too small for offset +%d (max %d bytes)",
			offset,
			(int)buff_size);

		error = -ENOBUFS;
	}
#ifdef DEBUG_LOG
	printk("--------%s------2\n",__func__);
	printk("--------%s------2\n",__func__);
#endif
	if (!error) {
		
		chip_config = ( struct mt_chip_conf * ) fpc1020->spi->controller_data;
		chip_config->com_mod = DMA_TRANSFER;
		chip_config->pause = 1;
		spi_setup(fpc1020->spi);

		memcpy(buffer + offset,tx_data,2);

		if((length > 1024) && (length%1024 > 0)) {
			length = 1024*(length/1024);
			rest_length = (int)image_size_bytes+2-length;
		}
		
		struct spi_transfer cmd = {
			.cs_change = 0,
			.delay_usecs = 0,
			.speed_hz = FPC1020_SPI_CLOCK_SPEED,
			.tx_buf = buffer + offset,
			.rx_buf = buffer + offset,
			.len    = length,
			.tx_dma = 0,
			.rx_dma = 0,
			.bits_per_word = 0,
		};

		spi_message_init(&msg);
		spi_message_add_tail(&cmd,	&msg);

#ifdef DEBUG_LOG
		printk("--------%s------3\n",__func__);
		dev_err(&fpc1020->spi->dev, "%s (rest_length %d)\n", __func__, rest_length);
#endif

		if( rest_length > 0) {

			struct spi_transfer cmd2 = {
				.cs_change = 0,
				.delay_usecs = 0,
				.speed_hz = FPC1020_SPI_CLOCK_SPEED,
				.tx_buf = buffer + offset + length,
				.rx_buf = buffer + offset + length,
				.len	= rest_length,
				.tx_dma = 0,
				.rx_dma = 0,
				.bits_per_word = 0,
			};
			spi_message_add_tail(&cmd2, &msg); //Note: if this does not work, just do it for a new spi_sync()
		
		}
		
#ifdef DEBUG_LOG
		printk("--------%s------3\n",__func__);
#endif
		error = spi_sync(fpc1020->spi, &msg);

		if (error)
			dev_err(&fpc1020->spi->dev, "spi_sync failed.\n");

#ifdef DEBUG_LOG
		printk("--------%s------4\n",__func__);
#endif
		int i;
		for(i=0;i<image_size_bytes;i++)
			buffer[i+offset] = buffer[i+offset+2];

#ifdef DEBUG
		printk(" %s buffer length %d, %x %x %x %x: %x %x %x %x\n ",
			__func__, length+rest_length, buffer[0], buffer[1], buffer[2], buffer[3], buffer[image_size_bytes-4], buffer[image_size_bytes-3], buffer[image_size_bytes-2], buffer[image_size_bytes-1]);
#endif

		if (gpio_is_valid(fpc1020->cs_gpio))
			gpio_set_value(fpc1020->cs_gpio, 1);
		
		chip_config = ( struct mt_chip_conf * ) fpc1020->spi->controller_data;
		chip_config->com_mod = FIFO_TRANSFER;
		chip_config->pause = 0;
		spi_setup(fpc1020->spi);
		
#ifdef DEBUG_LOG
		printk("--------%s------5\n",__func__);
#endif
	}
	
#ifdef DEBUG_LOG
	printk("--------%s------6\n",__func__);
#endif
	error = fpc1020_read_irq(fpc1020, true);
	
#ifdef DEBUG_LOG
	printk("--------%s----err=%d--7\n",__func__,error);
#endif
	if (error > 0)
		error = (error & FPC_1020_IRQ_REG_BIT_ERROR) ? -EIO : 0;

	return error;
}

/* -------------------------------------------------------------------- */
bool fpc1020_check_in_range_u64(u64 val, u64 min, u64 max)
{
	return (val >= min) && (val <= max);
}


/* -------------------------------------------------------------------- */
u32 fpc1020_calc_pixel_sum(u8 *buffer, size_t count)
{
	size_t index = count;
	u32 sum = 0;

	while(index)
	{
		index--;
		sum += ((0xff - buffer[index]) / 8);
	}
	return sum;
}


/* -------------------------------------------------------------------- */
static int fpc1020_set_finger_drive(fpc1020_data_t *fpc1020, bool enable)
{

	int error = 0;
	u8 config;
	fpc1020_reg_access_t reg;

	dev_err(&fpc1020->spi->dev, "%s %s\n", __func__, (enable)? "ON":"OFF");

	FPC1020_MK_REG_READ(reg, FPC102X_REG_FINGER_DRIVE_CONF, &config);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;

	if (enable)
		config |= 0x02;
	else
		config &= ~0x02;

	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_FINGER_DRIVE_CONF, &config);
	error = fpc1020_reg_access(fpc1020, &reg);

out:
	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_calc_finger_detect_threshold_min(fpc1020_data_t *fpc1020)
{
	int error = 0;
	int index;
	int first_col, first_row, adc_groups, row_count;
	u32 pixelsum[FPC1020_WAKEUP_DETECT_ZONE_COUNT] = {0, 0};
	u32 temp_u32;

	size_t image_size;

	dev_err(&fpc1020->spi->dev, "%s..1..err=%d\n", __func__,error);

	error = fpc1020_write_sensor_setup(fpc1020);


	dev_err(&fpc1020->spi->dev, "%s..2..err=%d\n", __func__,error);

	if (!error)
		error = fpc1020_capture_finger_detect_settings(fpc1020);

	dev_err(&fpc1020->spi->dev, "%s..3..err=%d\n", __func__,error);

	if (!error)  
		error = fpc1020_set_finger_drive(fpc1020, false);


	dev_err(&fpc1020->spi->dev, "%s..5..err=%d\n", __func__,error);


	adc_groups = FPC1020_WAKEUP_DETECT_COLS	/ fpc1020->chip.adc_group_size;
	image_size = (FPC1020_WAKEUP_DETECT_ROWS + 1) * adc_groups * fpc1020->chip.adc_group_size;
	row_count = FPC1020_WAKEUP_DETECT_ROWS + 1;

	index = FPC1020_WAKEUP_DETECT_ZONE_COUNT;
	while (index && !error) {

		index--;

		first_col = fpc1020->setup.wakeup_detect_cols[index] / fpc1020->chip.adc_group_size;
		first_row = fpc1020->setup.wakeup_detect_rows[index] - 1;

		error = fpc1020_capture_set_crop(fpc1020,
						first_col,
						adc_groups,
						first_row,
						row_count);

	dev_err(&fpc1020->spi->dev, "%s..6..err=%d\n", __func__,error);

		if (!error) {
			error = fpc1020_capture_buffer(fpc1020,
						fpc1020->huge_buffer,
						0,
						image_size);
		}
		
	dev_err(&fpc1020->spi->dev, "%s..7..err=%d\n", __func__,error);

		if (!error) {
			pixelsum[index] = fpc1020_calc_pixel_sum(
				fpc1020->huge_buffer + fpc1020->chip.adc_group_size,
				image_size - fpc1020->chip.adc_group_size);
		}
	}

	dev_err(&fpc1020->spi->dev, "%s..8..err=%d\n", __func__,error);

	if (!error) {
		temp_u32 = 0;

		index = FPC1020_WAKEUP_DETECT_ZONE_COUNT;
		while (index) {
			index--;
			if (pixelsum[index] > temp_u32)
				temp_u32 = pixelsum[index];
		}
		error = (int)(temp_u32 / 2);

		if (error >= 0xff)
			error = -EINVAL;
	}

	dev_err(&fpc1020->spi->dev, "%s..9..err=%d\n", __func__,error);

	dev_err(&fpc1020->spi->dev, "%s : %s %d\n",
			__func__,
			(error < 0) ? "Error" : "measured min =",
			error);

	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_set_finger_detect_threshold(fpc1020_data_t *fpc1020,
					int measured_val)
{
	int error = 0;
	int new_val;
	u8 old_val = fpc1020->setup.finger_detect_threshold;

	new_val = measured_val + 40; // Todo: awaiting calculated values


	dev_err(&fpc1020->spi->dev, "%s..1..measured_val(<0)=%d,new_val(>0xff)=%x\n", __func__,measured_val,new_val);

	if ((measured_val < 0) || (new_val >= 0xff))
		error = -EINVAL;

	dev_err(&fpc1020->spi->dev, "%s..2..err=%d\n", __func__,error);
	if (!error) {
		fpc1020->setup.finger_detect_threshold = (u8)new_val;

		dev_err(&fpc1020->spi->dev, "%s %d -> %d\n",
					__func__,
					old_val,
					fpc1020->setup.finger_detect_threshold);
	} else {
		dev_err(&fpc1020->spi->dev,
			"%s unable to set finger detect threshold %d\n",
			__func__,
			error);
	}
	dev_err(&fpc1020->spi->dev, "%s..over..err=%d\n", __func__,error);
	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_flush_adc(fpc1020_data_t *fpc1020)
{
	int error = 0;
	const int adc_groups = 9;
	const int row_count  = 1;
	const int image_size = adc_groups * fpc1020->chip.adc_group_size * row_count;

	dev_err(&fpc1020->spi->dev, "%s\n", __func__);

	error = fpc1020_capture_set_crop(fpc1020, 0, adc_groups, 0, row_count);

	if (!error) {
		error = fpc1020_capture_buffer(fpc1020,
						fpc1020->huge_buffer,
						0,
						image_size);
	}

	return error;
}


/* -------------------------------------------------------------------- */

