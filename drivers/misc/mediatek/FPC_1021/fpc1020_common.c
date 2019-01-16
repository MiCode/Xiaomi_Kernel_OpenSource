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

#include "fpc1020_common.h"
#include "fpc1020_regs.h"
#include "fpc1020_capture.h"
#include "fpc1020_regulator.h"

#include <mach/mt_gpio.h>
#include <mach/mt_spi.h>
#include <mach/eint.h>
#include <cust_eint.h>

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
	u8 revision;
	u8 pixel_rows;
	u8 pixel_columns;
	u8 adc_group_size;
};


/* -------------------------------------------------------------------- */
/* fpc1020 driver constants						*/
/* -------------------------------------------------------------------- */
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
	"fpc1155x" 	/* FPC1020_CHIP_1155X */
};

static const struct chip_struct chip_data[] = {
	{FPC1020_CHIP_1020A, 0x020a, 0, FPC1020_ROWS, FPC1020_COLUMNS, FPC102X_ADC_GROUP_SIZE},
	{FPC1020_CHIP_1021A, 0x021a, 2, FPC1021_ROWS, FPC1021_COLUMNS, FPC102X_ADC_GROUP_SIZE},
	{FPC1020_CHIP_1021B, 0x021b, 1, FPC1021_ROWS, FPC1021_COLUMNS, FPC102X_ADC_GROUP_SIZE},
	{FPC1020_CHIP_1150A, 0x150a, 1, FPC1150_ROWS, FPC1150_COLUMNS, FPC102X_ADC_GROUP_SIZE},
	{FPC1020_CHIP_1150B, 0x150b, 1, FPC1150_ROWS, FPC1150_COLUMNS, FPC102X_ADC_GROUP_SIZE},
	{FPC1020_CHIP_1150F, 0x150f, 1, FPC1150_ROWS, FPC1150_COLUMNS, FPC102X_ADC_GROUP_SIZE},
	{FPC1020_CHIP_NONE,  0,0,0,0,0}
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

static int fpc1020_write_sensor_1150_setup(fpc1020_data_t *fpc1020);

static int fpc1020_check_irq_after_reset(fpc1020_data_t *fpc1020);

static int fpc1020_flush_adc(fpc1020_data_t *fpc1020);
int fpc1020_spi_wr_rd(fpc1020_data_t *fpc1020, u8 *tx,
                                        u8 *rx, unsigned int length);

extern fpc1020_data_t *g_fpc1020;

 struct fpc1020_platform_data fpc1020_pdata_mt65xx = {
        .irq_gpio = FPC1020_IRQ_GPIO,
        .reset_gpio = FPC1020_RESET_GPIO,
};


struct mt_chip_conf spi_conf_mt65xx = {
        .setuptime = 15,
        .holdtime = 15,
        .high_time = 12, //for mt6589, 100000khz/(4+4) = 125000khz
        .low_time = 12,
        .cs_idletime = 20,
        .ulthgh_thrsh = 0,

        .cpol = 0,
        .cpha = 0,

        .rx_mlsb = 1,
        .tx_mlsb = 1,

        .tx_endian = 0,
        .rx_endian = 0,

        .com_mod = FIFO_TRANSFER,
        .pause = 0,
        .finish_intr = 1,
        .deassert = 0,
        .ulthigh = 0,
        .tckdly = 0,
};

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
		printk("[FPC_ERR] %s:line%d, error=%d\n",__func__, __LINE__,  error);
	} else {
		dev_info(&fpc1020->spi->dev, "%s, size=%d bytes\n",
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

	case FPC1020_CHIP_1150A:
	case FPC1020_CHIP_1150B:
	case FPC1020_CHIP_1150F:
		ptr = (fpc1020->chip.revision == 1) ? &fpc1020_setup_default_1150_a1b1f1 :
			NULL;
		break;

	default:
		ptr = NULL;
		break;
	}

	error = (ptr == NULL) ? -EINVAL : 0;
	if (error) {
	        printk("[FPC_ERR] %s:line%d, error=%d\n",__func__, __LINE__,  error);
		goto out_err;
	}

	memcpy((void *)&fpc1020->setup,	ptr, sizeof(fpc1020_setup_t));

	dev_dbg(&fpc1020->spi->dev, "%s OK\n", __func__);

	return 0;

out_err:
	memset((void *)&fpc1020->setup,	0, sizeof(fpc1020_setup_t));
	

	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_gpio_reset(fpc1020_data_t *fpc1020)
{
	int error = 0;
	int counter = FPC1020_RESET_RETRIES;
	while (counter) {
		counter--;
		mt_set_gpio_out(fpc1020->reset_gpio, GPIO_OUT_ONE);
		udelay(FPC1020_RESET_HIGH1_US);
		printk("[FPC_DBG] %s: pin1 = %d\n", __func__,mt_get_gpio_out(fpc1020->reset_gpio));

		mt_set_gpio_out(fpc1020->reset_gpio, GPIO_OUT_ZERO);
		udelay(FPC1020_RESET_LOW_US+2);
		printk("[FPC_DBG] %s pin2 = %d\n",__func__,mt_get_gpio_out(fpc1020->reset_gpio));

		mt_set_gpio_out(fpc1020->reset_gpio, GPIO_OUT_ONE);
		udelay(FPC1020_RESET_HIGH2_US);
		printk("[FPC_DBG] %s = pin3 %d\n", __func__, mt_get_gpio_out(fpc1020->reset_gpio));

		error = mt_get_gpio_in(fpc1020->irq_gpio) ? 0 : -EIO;

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

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

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
				dev_dbg(&fpc1020->spi->dev,
					"%s OK !\n", __func__);

				counter = 0;

			} else {
				dev_dbg(&fpc1020->spi->dev,
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
	printk("%s\n", __func__);

	error = (fpc1020->soft_reset_enabled) ?
			fpc1020_spi_reset(fpc1020) :
			fpc1020_gpio_reset(fpc1020);
	//mt_eint_mask(fpc1020->irq);
	fpc1020->interrupt_done = false;
	//mt_eint_unmask(fpc1020->irq);
	//mdelay(200);
	error = fpc1020_check_irq_after_reset(fpc1020);

	if (error < 0)
		goto out;

	error = (mt_get_gpio_in(fpc1020->irq_gpio) != 0) ? -EIO : 0;
	if (error) {
		printk("[FPC_ERR] %s:line%d, error=%d,  irq gpio is high after clear\n",__func__, __LINE__,  error);

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
	u8 cmd[8];
	int i;
	fpc1020_reg_access_t reg;
	int counter = 0;
	bool match = false;

	FPC1020_MK_REG_READ(reg, FPC102X_REG_HWID, &hardware_id);
	error = fpc1020_reg_access(fpc1020, &reg);

	if (error)
		return error;

	printk("[FPC_DBG] hw id=0x%x\n", hardware_id);
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

		if (fpc1020->chip.revision == 0) {
			error = fpc1020_check_hw_id_extended(fpc1020);

			if (error > 0) {
				fpc1020->chip.revision = error;
				error = 0;
			}
		}

		dev_info(&fpc1020->spi->dev,
				"hardware id: 0x%x (%s, rev.%d) \n",
						hardware_id,
						chip_text[fpc1020->chip.type],
						fpc1020->chip.revision);
	} else {
		dev_err(&fpc1020->spi->dev,
			"fpc hardware id mismatch: got 0x%x\n", hardware_id);

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

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

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

	dev_dbg(&fpc1020->spi->dev, "%s m1,m2 = %d,%d %s rev=%d \n", __func__,
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

		case FPC1020_CHIP_1150A:
		case FPC1020_CHIP_1150B:
		case FPC1020_CHIP_1150F:
		return fpc1020_write_sensor_1150_setup(fpc1020);

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

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

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
	temp_u8 = 0x12;
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

	temp_u16 = 0x00ff; /*yongqiang original is 0xff*/
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

	dev_dbg(&fpc1020->spi->dev, "%s %d\n", __func__, mux);

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


static void mtspi_set_mode(fpc1020_data_t *fpc1020, int mode)
{
        struct mt_chip_conf* spi_par;
        spi_par =&spi_conf_mt65xx;
        if(!spi_par)
        {
                printk("spi config fail\n");
                return;
        }

        if(1 == mode)
        {
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
                printk("[FPC_ERR]spi_setup  fail\n");
        }

        return;
}


/* -------------------------------------------------------------------- */
static int fpc1020_write_sensor_1021_setup(fpc1020_data_t *fpc1020) 
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
#if 0
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


	temp_u8 = 0x12;
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

	temp_u16 = 0x0005; //0x00ff;
	FPC1020_MK_REG_WRITE(reg, FPC1020_REG_FNGR_DET_CNTR, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)
		goto out;
#else
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

	temp_u8 = 0x2E;
	FPC1020_MK_REG_WRITE(reg, FPC102X_REG_PXL_RST_DLY, &temp_u8);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error)


#endif

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
static int fpc1020_check_irq_after_reset(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 irq_status;
	u8 cmd[8];
	u8 data[8];
	int i;
	memset(data, 0, 8);
	memset(cmd, 0, 8);	
	printk("[FPC] fpc1020_check_irq_after_reset +++\n");

	fpc1020_reg_access_t reg_clear = {
		.reg = FPC102X_REG_READ_INTERRUPT_WITH_CLEAR,
		.write = false,
		.reg_size = FPC1020_REG_SIZE(
					FPC102X_REG_READ_INTERRUPT_WITH_CLEAR),
		.dataptr = &irq_status
	};

	error = fpc1020_reg_access(fpc1020, &reg_clear);
	if(error < 0) {
	        printk("[FPC_ERR] %s:line%d, error=%d\n",__func__, __LINE__,  error);
		return error;
        }

	if (irq_status != FPC_1020_IRQ_REG_BITS_REBOOT) {
		printk("IRQ register, expected 0x%x, got 0x%x.\n",
			FPC_1020_IRQ_REG_BITS_REBOOT,
			irq_status);
                printk("[FPC_ERR] %s:line%d, error=%d\n",__func__, __LINE__,  error);
		error = -EIO;
	}
	printk("[FPC] fpc1020_check_irq_after_reset 0x%x---\n", irq_status);
	return (error < 0) ? error : irq_status;
}


/* -------------------------------------------------------------------- */
int fpc1020_wait_for_irq(fpc1020_data_t *fpc1020, int timeout)
{
	int result = 0;

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
	int i;

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

	error = fpc1020_reg_access(fpc1020, 
			(clear_irq) ? &reg_clear : &reg_read);
	if (irq_status == FPC_1020_IRQ_REG_BITS_REBOOT) {

		printk("%s: fpc1020_read_irq unexpected irq_status = 0x%x\n"
			, __func__, irq_status);
		printk("[FPC_ERR] %s:line%d, error=%d\n",__func__, __LINE__,  error);

		error = -EIO;
	}

	return (error < 0) ? error : irq_status;
}


/* -------------------------------------------------------------------- */
int fpc1020_read_status_reg(fpc1020_data_t *fpc1020)
{
	int error = 0;
	u8 cmd[8];
	int i;
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
int fpc1020_spi_wr_rd(fpc1020_data_t *fpc1020, u8 *tx,
                                        u8 *rx, unsigned int length)
{
        int error;
        struct spi_message m;
        struct spi_transfer t = {
                .cs_change = 0,
                .delay_usecs = 0,
                .speed_hz = FPC1020_SPI_CLOCK_SPEED,
                .tx_buf = tx,
                .rx_buf = rx,
                .len = length,
                .tx_dma = 0,
                .rx_dma = 0,
                .bits_per_word = 0,
        };

        mtspi_set_mode(fpc1020, 0); // FIFO
        spi_message_init(&m);
        spi_message_add_tail(&t, &m);

        error = spi_sync(fpc1020->spi, &m);

        if (error) {
                printk("spi_sync failed.\n");
                return error;
        }

        return 0;
}

/* ------yongqiang-------------------------------------------------------------- */
#if 1
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
		.speed_hz = (u32)fpc1020->spi_freq_khz * 1000u,
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
        mtspi_set_mode(fpc1020, 0); // FIFO
	spi_message_init(&msg);
	spi_message_add_tail(&cmd,  &msg);

	error = spi_sync(fpc1020->spi, &msg);

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

#ifdef DEBUG
	printk(
		"%s %s 0x%x/%dd (%d bytes) %x %x %x %x : %x %x %x %x\n",
		 __func__,
		(reg_data->write) ? "WRITE" : "READ",
		reg_data->reg,
		reg_data->reg,
		reg_data->reg_size,
		(reg_data->reg_size > 0) ? temp_buffer[0+cmd_size] : 0,
		(reg_data->reg_size > 1) ? temp_buffer[1+cmd_size] : 0,
		(reg_data->reg_size > 2) ? temp_buffer[2+cmd_size] : 0,
		(reg_data->reg_size > 3) ? temp_buffer[3+cmd_size] : 0,
		(reg_data->reg_size > 4) ? temp_buffer[4+cmd_size] : 0,
		(reg_data->reg_size > 5) ? temp_buffer[5+cmd_size] : 0,
		(reg_data->reg_size > 6) ? temp_buffer[6+cmd_size] : 0,
		(reg_data->reg_size > 7) ? temp_buffer[7+cmd_size] : 0);
#endif
out:
	return error;
}
#else
int fpc1020_reg_access(fpc1020_data_t *fpc1020,
		      fpc1020_reg_access_t *reg_data)
{
	int error = 0;

	//u8 temp_buffer[FPC1020_REG_MAX_SIZE];
	u8 temp_buffer[64];
	u8 temp_buffer2[64];
	struct spi_message msg;
	int i;
	int shift;
	int length;

	if (reg_data->write)
		length = 1 + FPC1020_REG_ACCESS_DUMMY_BYTES(reg_data->reg)+reg_data->reg_size;
	else
		length = reg_data->reg_size<<1;

	struct spi_transfer cmd = {
		.cs_change = 0,
		.delay_usecs = 0,
		.speed_hz = FPC1020_SPI_CLOCK_SPEED,
		.tx_buf = temp_buffer/*&(reg_data->reg)*/,
		.rx_buf = temp_buffer2,
		.len    = length,
		//.len = 1 + reg_data->reg_size,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 8,
	};

	if (!reg_data->write) {
                for(i = 0; i < (1+FPC1020_REG_ACCESS_DUMMY_BYTES(reg_data->reg)); i ++) {
                        switch(i) {
                                case 0: shift =0; break;
                                case 1: shift =8; break;
                                case 2: shift = 16; break;
                                case 3: shift = 24; break;
                                case 4: shift = 32; break;
                                case 5: shift = 40; break;
                                case 6: shift = 48; break;
                                case 7: shift = 56; break;
                                case 8: shift = 64; break;
                                case 9: shift = 72; break;
                                case 10: shift = 80; break;
                                default: shift = 88; break;

                        }

                        temp_buffer[i] = (reg_data->reg>>shift)&0xFF;
		}
	}
	if (reg_data->write) {
		for(i = 0; i < (1+FPC1020_REG_ACCESS_DUMMY_BYTES(reg_data->reg)); i ++) {
			switch(i) {
				case 0: shift =0; break;
				case 1: shift =8; break;
				case 2: shift = 16; break;
				case 3: shift = 24; break;
				case 4: shift = 32; break;
				case 5: shift = 40; break;
				case 6: shift = 48; break;
				case 7: shift = 56; break;
				case 8: shift = 64; break;
				case 9: shift = 72; break;
				case 10: shift = 80; break;
				default: shift = 88; break;

			}
				
			temp_buffer[i] = (reg_data->reg>>shift)&0xFF;

	
		}
                int src = 0;
		//= 1+FPC1020_REG_ACCESS_DUMMY_BYTES(reg_data->reg);

		int dst = reg_data->reg_size - 1;
		while (src < reg_data->reg_size) {
			temp_buffer[dst+1+FPC1020_REG_ACCESS_DUMMY_BYTES(reg_data->reg)] = reg_data->dataptr[src];
			src++;
			dst--;
		}

	}

	if (reg_data->reg_size > sizeof(temp_buffer)) {
		dev_err(&fpc1020->spi->dev,
			"%s : illegal register size\n",
			__func__);

		error = -ENOMEM;
		goto out;
	}

#ifdef CS_CRL_SW
	mt_set_gpio_out(fpc1020->cs_gpio, GPIO_OUT_ZERO);
#endif

	mtspi_set_mode(fpc1020, 0); // FIFO
	spi_message_init(&msg);
	spi_message_add_tail(&cmd,  &msg);
	error = spi_sync(fpc1020->spi, &msg);
	if (error)
                printk("%s : spi_sync failed.\n", __func__);

	if (!reg_data->write) {
		int src = reg_data->reg_size<<1-1;
		int dst = 0;
		while (dst < reg_data->reg_size) {
			reg_data->dataptr[dst] = temp_buffer2[src];
			src--;
			dst++;
		}

	}

#ifdef CS_CRL_SW
	mt_set_gpio_out(fpc1020->cs_gpio, GPIO_OUT_ONE);
#endif
out:
	return error;
}
#endif /*end of reg_access*/

/* -------------------------------------------------------------------- */
int fpc1020_cmd(fpc1020_data_t *fpc1020,
			fpc1020_cmd_t cmd,
			u8 wait_irq_mask)
{
	int error = 0;
	struct spi_message msg;

	struct spi_transfer t = {
		.cs_change = 0,
		.delay_usecs = 0,
		.speed_hz = FPC1020_SPI_CLOCK_SPEED,
		.tx_buf = &cmd,
		.rx_buf = NULL,
		.len    = 1,
		.tx_dma = 0,
		.rx_dma = 0,
		.bits_per_word = 0,
	};

#ifdef CS_CRL_SW
	mt_set_gpio_out(fpc1020->cs_gpio, GPIO_OUT_ZERO);

#endif
	mtspi_set_mode(fpc1020, 0); // FIFO
	spi_message_init(&msg);
	spi_message_add_tail(&t,  &msg);

	error = spi_sync(fpc1020->spi, &msg);

	if (error)
		printk("spi_sync failed.\n");

	if ((error >= 0) && wait_irq_mask) {
		error = fpc1020_wait_for_irq(fpc1020,
					FPC1020_DEFAULT_IRQ_TIMEOUT_MS);

		if (error >= 0)
			error = fpc1020_read_irq(fpc1020, true);
	}

#ifdef CS_CRL_SW
       mt_set_gpio_out(fpc1020->cs_gpio, GPIO_OUT_ONE);
#endif

	return error;
}


/* -------------------------------------------------------------------- */
int fpc1020_wait_finger_present(fpc1020_data_t *fpc1020)
{
	int error = 0;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = fpc1020_read_irq(fpc1020, true);
	if (error < 0) {
	        printk("[FPC_ERR] %s:line%d, error=%d\n",__func__, __LINE__,  error);
		return error;
	}

	error = fpc1020_cmd(fpc1020,
			FPC1020_CMD_WAIT_FOR_FINGER_PRESENT, 0);

	if (error < 0) {
	        printk("[FPC_ERR] %s:line%d, error=%d\n",__func__, __LINE__,  error);
		return error;
	}

	while (1) {
		error = fpc1020_wait_for_irq(fpc1020,
						FPC1020_DEFAULT_IRQ_TIMEOUT_MS);

		if (error >= 0) {
			error = fpc1020_read_irq(fpc1020, true);
			if (error < 0) {
			        printk("[FPC_ERR] %s:line%d, error=%d\n",__func__, __LINE__,  error);
				return error;
		        }

			if (error &
				(FPC_1020_IRQ_REG_BIT_FINGER_DOWN |
				FPC_1020_IRQ_REG_BIT_COMMAND_DONE)) {

				dev_dbg(&fpc1020->spi->dev, "Finger down\n");

				error = 0;
			} else {
				dev_dbg(&fpc1020->spi->dev,
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
	if (error < 0) {
	        printk("[FPC_ERR] %s:line%d, error=%d\n",__func__, __LINE__,  error);
		return error;
	 }

	error = fpc1020_cmd(fpc1020,
			FPC1020_CMD_FINGER_PRESENT_QUERY,
			FPC_1020_IRQ_REG_BIT_COMMAND_DONE);

	if (error < 0) {
	        printk("[FPC_ERR] %s:line%d, error=%d\n",__func__, __LINE__,  error);
		return error;
	}

	FPC1020_MK_REG_READ(reg, FPC102X_REG_FINGER_PRESENT_STATUS, &temp_u16);
	error = fpc1020_reg_access(fpc1020, &reg);
	if (error) {
	        printk("[FPC_ERR] %s:line%d, error=%d\n",__func__, __LINE__,  error);
		return error;
	}

	fpc1020->diag.finger_present_status = temp_u16;

	//dev_dbg(&fpc1020->spi->dev, "%s zonedata = 0x%x\n", __func__, temp_u16);

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
		// dev_dbg(&fpc1020->spi->dev, "%s %d zones\n", __func__, count);
		return (int)count;
	}
}


/* -------------------------------------------------------------------- */
int fpc1020_wake_up(fpc1020_data_t *fpc1020)
{
	u8 temp_u8;
	const fpc1020_status_reg_t status_mask = FPC1020_STATUS_REG_MODE_MASK;
	printk("[FPC_DBG] fpc1020_wake_up before reset status=0x%x\n", fpc1020_read_status_reg(fpc1020));
	int reset  = fpc1020_reset(fpc1020);
	int status = fpc1020_read_status_reg(fpc1020);
	printk("[FPC_DBG] fpc1020_wake_up after reset status = %d, status=0x%x\n", reset, status); 
	if (reset == 0 && status >= 0 &&
		(fpc1020_status_reg_t)(status & status_mask) ==
		FPC1020_STATUS_REG_IN_IDLE_MODE) {

		dev_dbg(&fpc1020->spi->dev, "%s OK\n", __func__);

		return 0;
	} else {
	#if 0
		 fpc1020_reg_access_t reg_1 = {
                        .reg = FPC102X_REG_READ_ERROR_WITH_CLEAR,
                        .write = false,
                        .reg_size = FPC1020_REG_SIZE(FPC102X_REG_READ_ERROR_WITH_CLEAR),
                        .dataptr = &temp_u8
                };
                fpc1020_reg_access(fpc1020, &reg_1);
                printk("[FPC_ERR]  fpc1020_wake_up FpcError reg=0x%x\n", temp_u8);
		printk("%s FAILED\n", __func__);
		fpc1020_write_sensor_1021_setup(fpc1020);
	#endif
		//return 0;
		printk("%s FAILED\n", __func__);
		//printk("[FPC_ERR] %s:line%d, error=%d\n",__func__, __LINE__,  error);
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
		dev_dbg(&fpc1020->spi->dev,
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
			dev_dbg(&fpc1020->spi->dev,
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

	if (deep_sleep && sleep_ok /*&& gpio_is_valid(fpc1020->reset_gpio)*/)
		 mt_set_gpio_out(fpc1020->reset_gpio, GPIO_OUT_ZERO);

	if (deep_sleep && sleep_ok /*&& gpio_is_valid(fpc1020->cs_gpio)*/)
	
#ifdef CS_CRL_SW
	mt_set_gpio_out(fpc1020->cs_gpio, GPIO_OUT_ZERO);
#endif

	/* Optional: Also disable power supplies in sleep */
/*
	if (deep_sleep && sleep_ok)
		error = fpc1020_regulator_set(fpc1020, false);
*/

	if (sleep_ok) {
		printk("[FPC_DBG]%s %s OK\n", __func__,
			(deep_sleep)? str_deep : str_regular);
		return 0;
	} else {
		printk("[FPC_DBG]%s %s FAILED\n", __func__,
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
	int i;
	int error = 0;
	struct spi_message msg;
	int length = (int)image_size_bytes+2;
	int rest_length = 0;
	const u8 tx_data[2] = {FPC1020_CMD_READ_IMAGE , 0};
	dev_dbg(&fpc1020->spi->dev, "%s (+%d)\n", __func__, offset);
	if ((offset + (int)image_size_bytes) > buff_size) {
		printk("[FPC]Image buffer too small for offset +%d (max %d bytes)",
			offset,
			(int)buff_size);

		error = -ENOBUFS;
	}

	if (!error) {
		memcpy(buffer+offset, tx_data, 2);
		if ((length > 1024) && (length%1024 >0)) {
			length = 1024*(length/1024);
			rest_length = (int)image_size_bytes+2-length;
		}
		struct spi_transfer cmd = {
			.cs_change = 0,
			.delay_usecs = 0,
			.speed_hz = FPC1020_SPI_CLOCK_SPEED,
			.tx_buf = buffer+offset,
			.rx_buf = buffer+offset,
			.len    = length,
			.tx_dma = 0,
			.rx_dma = 0,
			.bits_per_word = 8,
		};

#ifdef CS_CRL_SW

		mt_set_gpio_out(fpc1020->cs_gpio, GPIO_OUT_ZERO);
#endif
		error = fpc1020_read_irq(fpc1020, true);

		mtspi_set_mode(fpc1020, 1); // DMA
		spi_message_init(&msg);
		spi_message_add_tail(&cmd,  &msg);


		if (rest_length > 0) {
			struct spi_transfer cmd2 = {
                        	.cs_change = 0,
                        	.delay_usecs = 0,
                        	.speed_hz = FPC1020_SPI_CLOCK_SPEED,
                        	.tx_buf = buffer+offset+length,
                        	.rx_buf = buffer+offset+length,
                        	.len    = rest_length,
                        	.tx_dma = 0,
                        	.rx_dma = 0,
                        	.bits_per_word = 8,
                	};
			spi_message_add_tail(&cmd2,  &msg);

		}
		error = spi_sync(fpc1020->spi, &msg);

                if (error)
                        printk("spi_sync failed.\n");
#ifdef CS_CRL_SW
		mt_set_gpio_out(fpc1020->cs_gpio, GPIO_OUT_ONE);
#endif
		for (i = 0; i < image_size_bytes; i++) {
			buffer[i+offset] = buffer[i+offset+2];
		}
	}

	error = fpc1020_read_irq(fpc1020, true);
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

	dev_dbg(&fpc1020->spi->dev, "%s %s\n", __func__, (enable)? "ON":"OFF");

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

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

	error = fpc1020_write_sensor_setup(fpc1020);

	if (!error)
		error = fpc1020_capture_finger_detect_settings(fpc1020);

	if (!error)
		error = fpc1020_set_finger_drive(fpc1020, false);

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

		if (!error) {
			error = fpc1020_capture_buffer(fpc1020,
						fpc1020->huge_buffer,
						0,
						image_size);
			printk("[FPC_ERR] fpc1020_calc_finger_detect_threshold_min error=%d, LINE =%d\n", error, __LINE__);
		}

		if (!error) {
			pixelsum[index] = fpc1020_calc_pixel_sum(
				fpc1020->huge_buffer + fpc1020->chip.adc_group_size,
				image_size - fpc1020->chip.adc_group_size);
		}
	}

	if (!error) {
		temp_u32 = 0;

		index = FPC1020_WAKEUP_DETECT_ZONE_COUNT;
		while (index) {
			index--;
			if (pixelsum[index] > temp_u32)
				temp_u32 = pixelsum[index];
		}
		error = (int)(temp_u32 / 2);
		printk("[FPC_ERR] fpc1020_calc_finger_detect_threshold_min error=%d, LINE =%d\n", error, __LINE__);
		if (error >= 0xff)
			error = -EINVAL;
	}

	printk("[FPC_DBG]%s : %s %d\n",
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

	if ((measured_val < 0) || (new_val >= 0xff))
		error = -EINVAL;

	if (!error) {
		fpc1020->setup.finger_detect_threshold = (u8)new_val;

		dev_dbg(&fpc1020->spi->dev, "%s %d -> %d\n",
					__func__,
					old_val,
					fpc1020->setup.finger_detect_threshold);
	} else {
		dev_err(&fpc1020->spi->dev,
			"%s unable to set finger detect threshold %d\n",
			__func__,
			error);
	}

	return error;
}


/* -------------------------------------------------------------------- */
static int fpc1020_flush_adc(fpc1020_data_t *fpc1020)
{
	int error = 0;
	const int adc_groups = 9;
	const int row_count  = 1;
	const int image_size = adc_groups * fpc1020->chip.adc_group_size * row_count;

	dev_dbg(&fpc1020->spi->dev, "%s\n", __func__);

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

