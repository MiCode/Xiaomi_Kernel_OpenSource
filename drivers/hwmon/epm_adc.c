/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/hwmon.h>
#include <linux/delay.h>
#include <linux/epm_adc.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>
#include <linux/hwmon-sysfs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>

#define EPM_ADC_DRIVER_NAME		"epm_adc"
#define EPM_ADC_MAX_FNAME		20
#define EPM_ADC_CONVERSION_DELAY	100 /* milliseconds */
/* Command Bits */
#define EPM_ADC_ADS_SPI_BITS_PER_WORD	8
#define EPM_ADC_ADS_DATA_READ_CMD	(0x1 << 5)
#define EPM_ADC_ADS_REG_READ_CMD	(0x2 << 5)
#define EPM_ADC_ADS_REG_WRITE_CMD	(0x3 << 5)
#define EPM_ADC_ADS_PULSE_CONVERT_CMD	(0x4 << 5)
#define EPM_ADC_ADS_MULTIPLE_REG_ACCESS	(0x1 << 4)
/* Register map */
#define EPM_ADC_ADS_CONFIG0_REG_ADDR	0x0
#define EPM_ADC_ADS_CONFIG1_REG_ADDR	0x1
#define EPM_ADC_ADS_MUXSG0_REG_ADDR	0x4
#define EPM_ADC_ADS_MUXSG1_REG_ADDR	0x5
/* Register map default data */
#define EPM_ADC_ADS_REG0_DEFAULT	0x2
#define EPM_ADC_ADS_REG1_DEFAULT	0x52
#define EPM_ADC_ADS_CHANNEL_DATA_CHID	0x1f
/* Channel ID */
#define EPM_ADC_ADS_CHANNEL_OFFSET	0x18
#define EPM_ADC_ADS_CHANNEL_VCC		0x1a
#define EPM_ADC_ADS_CHANNEL_TEMP	0x1b
#define EPM_ADC_ADS_CHANNEL_GAIN	0x1c
#define EPM_ADC_ADS_CHANNEL_REF		0x1d
/* Scaling data co-efficients */
#define EPM_ADC_SCALE_MILLI		1000
#define EPM_ADC_SCALE_CODE_VOLTS	3072
#define EPM_ADC_SCALE_CODE_GAIN		30720
#define EPM_ADC_TEMP_SENSOR_COEFF	394
#define EPM_ADC_TEMP_TO_DEGC_COEFF	168000
#define EPM_ADC_CHANNEL_AIN_OFFSET	8
#define EPM_ADC_MAX_NEGATIVE_SCALE_CODE	0x8000
#define EPM_ADC_NEG_LSB_CODE		0xffff
#define EPM_ADC_VREF_CODE		0x7800
#define EPM_ADC_MILLI_VOLTS_SOURCE	4750
#define EPM_ADC_SCALE_FACTOR		64
#define GPIO_EPM_GLOBAL_ENABLE		86
#define GPIO_EPM_MARKER1		85
#define GPIO_EPM_MARKER2		96
#define EPM_ADC_CONVERSION_TIME_MIN	50000
#define EPM_ADC_CONVERSION_TIME_MAX	51000
/* PSoc Commands */
#define EPM_PSOC_INIT_CMD				0x1
#define EPM_PSOC_INIT_RESPONSE_CMD			0x2
#define EPM_PSOC_CHANNEL_ENABLE_DISABLE_CMD		0x5
#define EPM_PSOC_CHANNEL_ENABLE_DISABLE_RESPONSE_CMD	0x6
#define EPM_PSOC_SET_AVERAGING_CMD			0x7
#define EPM_PSOC_SET_AVERAGING_RESPONSE_CMD		0x8
#define EPM_PSOC_GET_LAST_MEASUREMENT_CMD		0x9
#define EPM_PSOC_GET_LAST_MEASUREMENT_RESPONSE_CMD	0xa
#define EPM_PSOC_GET_BUFFERED_DATA_CMD			0xb
#define EPM_PSOC_GET_BUFFERED_RESPONSE_CMD		0xc
#define EPM_PSOC_GET_SYSTEM_TIMESTAMP_CMD		0x11
#define EPM_PSOC_GET_SYSTEM_TIMESTAMP_RESPONSE_CMD	0x12
#define EPM_PSOC_SET_SYSTEM_TIMESTAMP_CMD		0x13
#define EPM_PSOC_SET_SYSTEM_TIMESTAMP_RESPONSE_CMD	0x14
#define EPM_PSOC_SET_CHANNEL_TYPE_CMD			0x15
#define EPM_PSOC_SET_CHANNEL_TYPE_RESPONSE_CMD		0x16
#define EPM_PSOC_GET_AVERAGED_DATA_CMD			0x19
#define EPM_PSOC_GET_AVERAGED_DATA_RESPONSE_CMD		0x1a
#define EPM_PSOC_SET_CHANNEL_SWITCH_DELAY_CMD		0x1b
#define EPM_PSOC_SET_CHANNEL_SWITCH_DELAY_RESPONSE_CMD	0x1c
#define EPM_PSOC_CLEAR_BUFFER_CMD			0x1d
#define EPM_PSOC_CLEAR_BUFFER_RESPONSE_CMD		0x1e
#define EPM_PSOC_SET_VADC_REFERENCE_CMD			0x1f
#define EPM_PSOC_SET_VADC_REFERENCE_RESPONSE_CMD	0x20
#define EPM_PSOC_PAUSE_CONVERSION			0x35
#define EPM_PSOC_PAUSE_CONVERSION_RSP_CMD		0x36
#define EPM_PSOC_UNPAUSE_CONVERSION			0x37
#define EPM_PSOC_UNPAUSE_CONVERSION_RSP_CMD		0x38
#define EPM_PSOC_GPIO_BUFFER_REQUEST_CMD		0x4f
#define EPM_PSOC_GPIO_BUFFER_REQUEST_RESPONSE_CMD	0x50
#define EPM_PSOC_GET_GPIO_BUFFER_CMD			0x51
#define EPM_PSOC_GET_GPIO_BUFFER_RESPONSE_CMD		0x52

#define EPM_PSOC_GLOBAL_ENABLE				81
#define EPM_PSOC_VREF_VOLTAGE				2048
#define EPM_PSOC_MAX_ADC_CODE_15_BIT			32767
#define EPM_PSOC_MAX_ADC_CODE_12_BIT			4096
#define EPM_GLOBAL_ENABLE_MIN_DELAY			5000
#define EPM_GLOBAL_ENABLE_MAX_DELAY			5100

#define EPM_AVG_BUF_MASK1				0xfff00000
#define EPM_AVG_BUF_MASK2				0xfff00
#define EPM_AVG_BUF_MASK3				0xff
#define EPM_AVG_BUF_MASK4				0xf0000000
#define EPM_AVG_BUF_MASK5				0xfff0000
#define EPM_AVG_BUF_MASK6				0xfff0
#define EPM_AVG_BUF_MASK7				0xf
#define EPM_AVG_BUF_MASK8				0xff000000
#define EPM_AVG_BUF_MASK9				0xfff000
#define EPM_AVG_BUF_MASK10				0xfff

#define EPM_PSOC_BUFFERED_DATA_LENGTH			48
#define EPM_PSOC_BUFFERED_DATA_LENGTH2			54

struct epm_adc_drv {
	struct platform_device		*pdev;
	struct device			*hwmon;
	struct spi_device		*epm_spi_client;
	struct mutex			conv_lock;
	uint32_t			bus_id;
	struct miscdevice		misc;
	uint32_t			channel_mask;
	struct epm_chan_properties	epm_psoc_ch_prop[0];
};

static struct epm_adc_drv *epm_adc_drv;
static struct i2c_board_info *epm_i2c_info;
static bool epm_adc_first_request;
static int epm_gpio_expander_base_addr;
static bool epm_adc_expander_register;

#define GPIO_EPM_EXPANDER_IO0	epm_gpio_expander_base_addr
#define GPIO_PWR_MON_ENABLE	(GPIO_EPM_EXPANDER_IO0 + 1)
#define GPIO_ADC1_PWDN_N	(GPIO_PWR_MON_ENABLE + 1)
#define GPIO_PWR_MON_RESET_N	(GPIO_ADC1_PWDN_N + 1)
#define GPIO_EPM_SPI_ADC1_CS_N	(GPIO_PWR_MON_RESET_N + 1)
#define GPIO_PWR_MON_START	(GPIO_EPM_SPI_ADC1_CS_N + 1)
#define GPIO_ADC1_DRDY_N	(GPIO_PWR_MON_START + 1)
#define GPIO_ADC2_PWDN_N	(GPIO_ADC1_DRDY_N + 1)
#define GPIO_EPM_SPI_ADC2_CS_N	(GPIO_ADC2_PWDN_N + 1)
#define GPIO_ADC2_DRDY_N	(GPIO_EPM_SPI_ADC2_CS_N + 1)

static int epm_adc_i2c_expander_register(void)
{
	int rc = 0;
	static struct i2c_adapter *i2c_adap;
	static struct i2c_client *epm_i2c_client;

	rc = gpio_request(GPIO_EPM_GLOBAL_ENABLE, "EPM_GLOBAL_EN");
	if (!rc) {
		gpio_direction_output(GPIO_EPM_GLOBAL_ENABLE, 1);
	} else {
		pr_err("%s: Configure EPM_GLOBAL_EN Failed\n", __func__);
		return rc;
	}

	usleep_range(EPM_ADC_CONVERSION_TIME_MIN,
			EPM_ADC_CONVERSION_TIME_MAX);

	i2c_adap = i2c_get_adapter(epm_adc_drv->bus_id);
	if (i2c_adap == NULL) {
		pr_err("%s: i2c_get_adapter() failed\n", __func__);
		return -EINVAL;
	}

	usleep_range(EPM_ADC_CONVERSION_TIME_MIN,
			EPM_ADC_CONVERSION_TIME_MAX);

	epm_i2c_client = i2c_new_device(i2c_adap, epm_i2c_info);
	if (IS_ERR(epm_i2c_client)) {
		pr_err("Error with i2c epm device register\n");
		return -ENODEV;
	}

	epm_adc_first_request = false;

	return 0;
}

static int epm_adc_gpio_configure_expander_enable(void)
{
	int rc = 0;

	if (epm_adc_first_request) {
		rc = gpio_request(GPIO_EPM_GLOBAL_ENABLE, "EPM_GLOBAL_EN");
		if (!rc) {
			gpio_direction_output(GPIO_EPM_GLOBAL_ENABLE, 1);
		} else {
			pr_err("%s: Configure EPM_GLOBAL_EN Failed\n",
								__func__);
			return rc;
		}
	} else {
		epm_adc_first_request = true;
	}

	usleep_range(EPM_ADC_CONVERSION_TIME_MIN,
			EPM_ADC_CONVERSION_TIME_MAX);

	rc = gpio_request(GPIO_PWR_MON_ENABLE, "GPIO_PWR_MON_ENABLE");
	if (!rc) {
		rc = gpio_direction_output(GPIO_PWR_MON_ENABLE, 1);
		if (rc) {
			pr_err("%s: Set GPIO_PWR_MON_ENABLE failed\n",
					__func__);
			return rc;
		}
	} else {
		pr_err("%s: gpio_request GPIO_PWR_MON_ENABLE failed\n",
				__func__);
		return rc;
	}

	rc = gpio_request(GPIO_ADC1_PWDN_N, "GPIO_ADC1_PWDN_N");
	if (!rc) {
		rc = gpio_direction_output(GPIO_ADC1_PWDN_N, 1);
		if (rc) {
			pr_err("%s: Set GPIO_ADC1_PWDN_N failed\n", __func__);
			return rc;
		}
	} else {
		pr_err("%s: gpio_request GPIO_ADC1_PWDN_N failed\n", __func__);
		return rc;
	}

	rc = gpio_request(GPIO_ADC2_PWDN_N, "GPIO_ADC2_PWDN_N");
	if (!rc) {
		rc = gpio_direction_output(GPIO_ADC2_PWDN_N, 1);
		if (rc) {
			pr_err("%s: Set GPIO_ADC2_PWDN_N failed\n",
					__func__);
			return rc;
		}
	} else {
		pr_err("%s: gpio_request GPIO_ADC2_PWDN_N failed\n",
				__func__);
		return rc;
	}

	rc = gpio_request(GPIO_EPM_SPI_ADC1_CS_N, "GPIO_EPM_SPI_ADC1_CS_N");
	if (!rc) {
		rc = gpio_direction_output(GPIO_EPM_SPI_ADC1_CS_N, 1);
		if (rc) {
			pr_err("%s:Set GPIO_EPM_SPI_ADC1_CS_N failed\n",
					__func__);
			return rc;
		}
	} else {
		pr_err("%s: gpio_request GPIO_EPM_SPI_ADC1_CS_N failed\n",
				__func__);
		return rc;
	}

	rc = gpio_request(GPIO_EPM_SPI_ADC2_CS_N,
			"GPIO_EPM_SPI_ADC2_CS_N");
	if (!rc) {
		rc = gpio_direction_output(GPIO_EPM_SPI_ADC2_CS_N, 1);
		if (rc) {
			pr_err("Set GPIO_EPM_SPI_ADC2_CS_N failed\n");
			return rc;
		}
	} else {
		pr_err("gpio_request GPIO_EPM_SPI_ADC2_CS_N failed\n");
		return rc;
	}

	rc = gpio_direction_output(GPIO_EPM_SPI_ADC1_CS_N, 0);
	if (rc) {
		pr_err("%s:Reset GPIO_EPM_SPI_ADC1_CS_N failed\n", __func__);
		return rc;
	}

	rc = gpio_direction_output(GPIO_EPM_SPI_ADC1_CS_N, 1);
	if (rc) {
		pr_err("%s: Set GPIO_EPM_SPI_ADC1_CS_N failed\n", __func__);
		return rc;
	}

	rc = gpio_request(GPIO_PWR_MON_START, "GPIO_PWR_MON_START");
	if (!rc) {
		rc = gpio_direction_output(GPIO_PWR_MON_START, 0);
		if (rc) {
			pr_err("%s: Reset GPIO_PWR_MON_START failed\n",
					__func__);
			return rc;
		}
	} else {
		pr_err("%s: gpio_request GPIO_PWR_MON_START failed\n",
				__func__);
		return rc;
	}

	rc = gpio_request(GPIO_PWR_MON_RESET_N, "GPIO_PWR_MON_RESET_N");
	if (!rc) {
		rc = gpio_direction_output(GPIO_PWR_MON_RESET_N, 0);
		if (rc) {
			pr_err("%s: Reset GPIO_PWR_MON_RESET_N failed\n",
					__func__);
			return rc;
		}
	} else {
		pr_err("%s: gpio_request GPIO_PWR_MON_RESET_N failed\n",
				__func__);
		return rc;
	}

	rc = gpio_direction_output(GPIO_PWR_MON_RESET_N, 1);
	if (rc) {
		pr_err("%s: Set GPIO_PWR_MON_RESET_N failed\n", __func__);
		return rc;
	}

	rc = gpio_direction_output(GPIO_EPM_SPI_ADC1_CS_N, 0);
	if (rc) {
		pr_err("%s:Reset GPIO_EPM_SPI_ADC1_CS_N failed\n", __func__);
		return rc;
	}
	return rc;
}

static int epm_adc_gpio_configure_expander_disable(void)
{
	int rc = 0;
	gpio_free(GPIO_PWR_MON_ENABLE);
	gpio_free(GPIO_ADC1_PWDN_N);
	gpio_free(GPIO_ADC2_PWDN_N);
	gpio_free(GPIO_EPM_SPI_ADC1_CS_N);
	gpio_free(GPIO_EPM_SPI_ADC2_CS_N);
	gpio_free(GPIO_PWR_MON_START);
	gpio_free(GPIO_PWR_MON_RESET_N);
	rc = gpio_direction_output(GPIO_EPM_GLOBAL_ENABLE, 0);
	if (rc)
		pr_debug("%s: Disable EPM_GLOBAL_EN Failed\n", __func__);
	gpio_free(GPIO_EPM_GLOBAL_ENABLE);
	return rc;
}

static int epm_adc_spi_chip_select(int32_t id)
{
	int rc = 0;
	if (id == 0) {
		rc = gpio_direction_output(GPIO_EPM_SPI_ADC2_CS_N, 1);
		if (rc) {
			pr_err("%s:Disable SPI_ADC2_CS failed",
					__func__);
			return rc;
		}

		rc = gpio_direction_output(GPIO_EPM_SPI_ADC1_CS_N, 0);
		if (rc) {
			pr_err("%s:Enable SPI_ADC1_CS failed", __func__);
			return rc;
		}
	} else if (id == 1) {
		rc = gpio_direction_output(GPIO_EPM_SPI_ADC1_CS_N, 1);
		if (rc) {
			pr_err("%s:Disable SPI_ADC1_CS failed", __func__);
			return rc;
		}
		rc = gpio_direction_output(GPIO_EPM_SPI_ADC2_CS_N, 0);
		if (rc) {
			pr_err("%s:Enable SPI_ADC2_CS failed", __func__);
			return rc;
		}
	} else {
		rc = -EFAULT;
	}
	return rc;
}

static int epm_adc_ads_spi_write(struct epm_adc_drv *epm_adc,
		uint8_t addr, uint8_t val)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[2];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = EPM_ADC_ADS_REG_WRITE_CMD | addr;
	tx_buf[1] = val;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);

	return rc;
}

static int epm_adc_init_ads(struct epm_adc_drv *epm_adc)
{
	int rc = 0;

	rc = epm_adc_ads_spi_write(epm_adc, EPM_ADC_ADS_CONFIG0_REG_ADDR,
						EPM_ADC_ADS_REG0_DEFAULT);
	if (rc)
		return rc;

	rc = epm_adc_ads_spi_write(epm_adc, EPM_ADC_ADS_CONFIG1_REG_ADDR,
						EPM_ADC_ADS_REG1_DEFAULT);
	if (rc)
		return rc;
	return rc;
}

static int epm_adc_ads_pulse_convert(struct epm_adc_drv *epm_adc)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[1];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = EPM_ADC_ADS_PULSE_CONVERT_CMD;
	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);

	return rc;
}

static int epm_adc_ads_read_data(struct epm_adc_drv *epm_adc, char *adc_data)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[4], rx_buf[4];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = EPM_ADC_ADS_DATA_READ_CMD |
			EPM_ADC_ADS_MULTIPLE_REG_ACCESS;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	adc_data[0] = rx_buf[1];
	adc_data[1] = rx_buf[2];
	adc_data[2] = rx_buf[3];

	return rc;
}

static int epm_adc_hw_init(struct epm_adc_drv *epm_adc)
{
	int rc = 0;

	mutex_lock(&epm_adc->conv_lock);
	rc = epm_adc_gpio_configure_expander_enable();
	if (rc != 0) {
		pr_err("epm gpio configure expander failed, rc = %d\n", rc);
		goto epm_adc_hw_init_err;
	}
	rc = epm_adc_init_ads(epm_adc);
	if (rc) {
		pr_err("epm_adc_init_ads failed, rc=%d\n", rc);
		goto epm_adc_hw_init_err;
	}

epm_adc_hw_init_err:
	mutex_unlock(&epm_adc->conv_lock);
	return rc;
}

static int epm_adc_hw_deinit(struct epm_adc_drv *epm_adc)
{
	int rc = 0;

	mutex_lock(&epm_adc->conv_lock);
	rc = epm_adc_gpio_configure_expander_disable();
	if (rc != 0) {
		pr_err("gpio expander disable failed with %d\n", rc);
		goto epm_adc_hw_deinit_err;
	}

epm_adc_hw_deinit_err:
	mutex_unlock(&epm_adc->conv_lock);
	return rc;
}

static int epm_adc_ads_scale_result(struct epm_adc_drv *epm_adc,
		uint8_t *adc_raw_data, struct epm_chan_request *conv)
{
	uint32_t channel_num;
	int16_t  sign_bit;
	struct epm_adc_platform_data *pdata = epm_adc->pdev->dev.platform_data;
	uint32_t chan_idx = (conv->device_idx * pdata->chan_per_adc) +
					conv->channel_idx;
	int64_t adc_scaled_data = 0;

	/* Get the channel number */
	channel_num = (adc_raw_data[0] & EPM_ADC_ADS_CHANNEL_DATA_CHID);
	sign_bit    = 1;
	/* This is the 16-bit raw data */
	adc_scaled_data = ((adc_raw_data[1] << 8) | adc_raw_data[2]);
	/* Obtain the internal system reading */
	if (channel_num == EPM_ADC_ADS_CHANNEL_VCC) {
		adc_scaled_data *= EPM_ADC_SCALE_MILLI;
		do_div(adc_scaled_data, EPM_ADC_SCALE_CODE_VOLTS);
	} else if (channel_num == EPM_ADC_ADS_CHANNEL_GAIN) {
		do_div(adc_scaled_data, EPM_ADC_SCALE_CODE_GAIN);
	} else if (channel_num == EPM_ADC_ADS_CHANNEL_REF) {
		adc_scaled_data *= EPM_ADC_SCALE_MILLI;
		do_div(adc_scaled_data, EPM_ADC_SCALE_CODE_VOLTS);
	} else if (channel_num == EPM_ADC_ADS_CHANNEL_TEMP) {
		/* Convert Code to micro-volts */
		/* Use this formula to get the temperature reading */
		adc_scaled_data -= EPM_ADC_TEMP_TO_DEGC_COEFF;
		do_div(adc_scaled_data, EPM_ADC_TEMP_SENSOR_COEFF);
	} else if (channel_num == EPM_ADC_ADS_CHANNEL_OFFSET) {
		/* The offset should be zero */
		pr_debug("%s: ADC Channel Offset\n", __func__);
		return -EFAULT;
	} else {
		channel_num -= EPM_ADC_CHANNEL_AIN_OFFSET;
		/*
		 * Conversion for the adc channels.
		 * mvVRef is in milli-volts and resistorvalue is in micro-ohms.
		 * Hence, I = V/R gives us current in kilo-amps.
		 */
		if (adc_scaled_data & EPM_ADC_MAX_NEGATIVE_SCALE_CODE) {
			sign_bit = -1;
			adc_scaled_data = (~adc_scaled_data
				& EPM_ADC_NEG_LSB_CODE);
		}
		if (adc_scaled_data != 0) {
			adc_scaled_data *= EPM_ADC_SCALE_FACTOR;
			 /* Device is calibrated for 1LSB = VREF/7800h.*/
			adc_scaled_data *= EPM_ADC_MILLI_VOLTS_SOURCE;
			do_div(adc_scaled_data, EPM_ADC_VREF_CODE);
			 /* Data will now be in micro-volts.*/
			adc_scaled_data *= EPM_ADC_SCALE_MILLI;
			 /* Divide by amplifier gain value.*/
			do_div(adc_scaled_data, pdata->channel[chan_idx].gain);
			 /* Data will now be in nano-volts.*/
			do_div(adc_scaled_data, EPM_ADC_SCALE_FACTOR);
			adc_scaled_data *= EPM_ADC_SCALE_MILLI;
			 /* Data is now in micro-amps.*/
			do_div(adc_scaled_data,
				pdata->channel[chan_idx].resistorvalue);
			 /* Set the sign bit for lekage current. */
			adc_scaled_data *= sign_bit;
		}
	}

	conv->physical = (int32_t) adc_scaled_data;

	return 0;
}

static int epm_psoc_scale_result(int16_t result, uint32_t index)
{
	struct epm_adc_drv *epm_adc = epm_adc_drv;
	int32_t result_cur, neg = 0;

	if ((1 << index) & epm_adc->channel_mask) {
		if (result & 0x800) {
			neg = 1;
			result = result & 0x7ff;
		}
		/* result = (2.048V * code)/(4096 * gain * rsense) */
		result_cur = ((EPM_PSOC_VREF_VOLTAGE * result)/
				EPM_PSOC_MAX_ADC_CODE_12_BIT);

		result_cur = (result_cur/
			(epm_adc->epm_psoc_ch_prop[index].gain *
			epm_adc->epm_psoc_ch_prop[index].resistorvalue));
		if (neg)
			result_cur -= result_cur;
	} else {
		if (result & 0x8000) {
			neg = 1;
			result = result & 0x7fff;
		}
		/* result = (2.048V * code)/(32767 * gain * rsense) */
		result_cur = (((EPM_PSOC_VREF_VOLTAGE * (int) result)/
				EPM_PSOC_MAX_ADC_CODE_15_BIT) * 1000);

		result_cur = (result_cur/
		(epm_adc->epm_psoc_ch_prop[index].gain *
			epm_adc->epm_psoc_ch_prop[index].resistorvalue));
		if (neg)
			result_cur -= result_cur;
	}

	return result_cur;
}

static int epm_adc_blocking_conversion(struct epm_adc_drv *epm_adc,
					struct epm_chan_request *conv)
{
	struct epm_adc_platform_data *pdata = epm_adc->pdev->dev.platform_data;
	int32_t channel_num = 0, mux_chan_idx = 0;
	char adc_data[3];
	int rc = 0;

	mutex_lock(&epm_adc->conv_lock);

	rc = epm_adc_spi_chip_select(conv->device_idx);
	if (rc) {
		pr_err("epm_adc_chip_select failed, rc=%d\n", rc);
		goto conv_err;
	}

	if (conv->channel_idx < pdata->chan_per_mux) {
		/* Reset MUXSG1_REGISTER */
		rc = epm_adc_ads_spi_write(epm_adc, EPM_ADC_ADS_MUXSG1_REG_ADDR,
							0x0);
		if (rc)
			goto conv_err;

		mux_chan_idx = 1 << conv->channel_idx;
		/* Select Channel index in MUXSG0_REGISTER */
		rc = epm_adc_ads_spi_write(epm_adc, EPM_ADC_ADS_MUXSG0_REG_ADDR,
				mux_chan_idx);
		if (rc)
			goto conv_err;
	} else {
		/* Reset MUXSG0_REGISTER */
		rc = epm_adc_ads_spi_write(epm_adc, EPM_ADC_ADS_MUXSG0_REG_ADDR,
							0x0);
		if (rc)
			goto conv_err;

		mux_chan_idx = 1 << (conv->channel_idx - pdata->chan_per_mux);
		/* Select Channel index in MUXSG1_REGISTER */
		rc = epm_adc_ads_spi_write(epm_adc, EPM_ADC_ADS_MUXSG1_REG_ADDR,
				mux_chan_idx);
		if (rc)
			goto conv_err;
	}

	rc = epm_adc_ads_pulse_convert(epm_adc);
	if (rc) {
		pr_err("epm_adc_ads_pulse_convert failed, rc=%d\n", rc);
		goto conv_err;
	}

	rc = epm_adc_ads_read_data(epm_adc, adc_data);
	if (rc) {
		pr_err("epm_adc_ads_read_data failed, rc=%d\n", rc);
		goto conv_err;
	}

	channel_num = (adc_data[0] & EPM_ADC_ADS_CHANNEL_DATA_CHID);
	pr_debug("ADC data Read: adc_data =%d, %d, %d\n",
			adc_data[0], adc_data[1], adc_data[2]);

	epm_adc_ads_scale_result(epm_adc, (uint8_t *)adc_data, conv);

	pr_debug("channel_num(0x) = %x, scaled_data = %d\n",
		 (channel_num - EPM_ADC_ADS_SPI_BITS_PER_WORD),
						conv->physical);
conv_err:
	mutex_unlock(&epm_adc->conv_lock);
	return rc;
}

static int epm_adc_psoc_gpio_init(bool enable)
{
	int rc = 0;

	if (enable) {
		rc = gpio_request(EPM_PSOC_GLOBAL_ENABLE, "EPM_PSOC_GLOBAL_EN");
		if (!rc) {
			gpio_direction_output(EPM_PSOC_GLOBAL_ENABLE, 1);
		} else {
			pr_err("%s: Configure EPM_GLOBAL_EN Failed\n",
								__func__);
			return rc;
		}
	} else {
		gpio_direction_output(EPM_PSOC_GLOBAL_ENABLE, 0);
		gpio_free(EPM_PSOC_GLOBAL_ENABLE);
	}

	return 0;
}

static int epm_set_marker1(struct epm_marker_level *marker_init)
{
	int rc = 0;

	rc = gpio_request(GPIO_EPM_MARKER1, "EPM_MARKER1");
	if (!rc) {
		gpio_direction_output(GPIO_EPM_MARKER1, 1);
	} else {
		pr_err("%s: Configure MARKER1 GPIO Failed\n",
							__func__);
		return rc;
	}

	gpio_set_value(GPIO_EPM_MARKER1, marker_init->level);

	return 0;
}

static int epm_set_marker2(struct epm_marker_level *marker_init)
{
	int rc = 0;

	rc = gpio_request(GPIO_EPM_MARKER2, "EPM_MARKER2");
	if (!rc) {
		gpio_direction_output(GPIO_EPM_MARKER2, 1);
	} else {
		pr_err("%s: Configure MARKER2 GPIO Failed\n",
							__func__);
		return rc;
	}

	gpio_set_value(GPIO_EPM_MARKER2, marker_init->level);

	return 0;
}

static int epm_marker1_release(void)
{
	gpio_free(GPIO_EPM_MARKER1);

	return 0;
}

static int epm_marker2_release(void)
{
	gpio_free(GPIO_EPM_MARKER2);

	return 0;
}

static int epm_psoc_pause_conversion(struct epm_adc_drv *epm_adc)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[2], rx_buf[2];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = EPM_PSOC_PAUSE_CONVERSION;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc) {
		pr_err("spi sync err with %d\n", rc);
		return rc;
	}

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc) {
		pr_err("spi sync err with %d\n", rc);
		return rc;
	}

	return rx_buf[0];
}

static int epm_psoc_unpause_conversion(struct epm_adc_drv *epm_adc)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[2], rx_buf[2];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = EPM_PSOC_UNPAUSE_CONVERSION;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc) {
		pr_err("spi sync err with %d\n", rc);
		return rc;
	}

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc) {
		pr_err("spi sync err with %d\n", rc);
		return rc;
	}

	return rx_buf[0];
}

static int epm_psoc_init(struct epm_adc_drv *epm_adc,
					struct epm_psoc_init_resp *init_resp)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[17], rx_buf[17];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = init_resp->cmd;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;
	init_resp->cmd			= rx_buf[0];
	init_resp->version		= rx_buf[1];
	init_resp->compatible_ver	= rx_buf[2];
	init_resp->firm_ver[0]		= rx_buf[3];
	init_resp->firm_ver[1]		= rx_buf[4];
	init_resp->firm_ver[2]		= rx_buf[5];
	init_resp->num_dev		= rx_buf[6];
	init_resp->num_channel		= rx_buf[7];

	pr_debug("EPM PSOC response for hello command: resp_cmd:0x%x\n",
							rx_buf[0]);
	pr_debug("EPM PSOC version:0x%x\n", rx_buf[1]);
	pr_debug("EPM PSOC firmware version:0x%x\n",
			rx_buf[6] | rx_buf[5] | rx_buf[4] | rx_buf[3]);

	return rc;
}

static int epm_psoc_channel_configure(struct epm_adc_drv *epm_adc,
		struct epm_psoc_channel_configure *psoc_chan_configure)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[9], rx_buf[9];
	int32_t rc = 0, chan_num;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	chan_num = psoc_chan_configure->channel_num;

	tx_buf[0] = psoc_chan_configure->cmd;
	tx_buf[1] = 0;
	tx_buf[2] = (chan_num & 0xff000000) >> 24;
	tx_buf[3] = (chan_num & 0xff0000) >> 16;
	tx_buf[4] = (chan_num & 0xff00) >> 8;
	tx_buf[5] = (chan_num & 0xff);

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	psoc_chan_configure->cmd		= rx_buf[0];
	psoc_chan_configure->device_num		= rx_buf[1];
	chan_num = rx_buf[2] << 24 | (rx_buf[3] << 16) | (rx_buf[4] << 8) |
						rx_buf[5];
	psoc_chan_configure->channel_num	= chan_num;

	return rc;
}

static int epm_psoc_set_averaging(struct epm_adc_drv *epm_adc,
		struct epm_psoc_set_avg *psoc_set_avg)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[4], rx_buf[4];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = psoc_set_avg->cmd;
	tx_buf[1] = psoc_set_avg->avg_period;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	psoc_set_avg->cmd		= rx_buf[0];
	psoc_set_avg->return_code	= rx_buf[1];

	return rc;
}

static int epm_psoc_get_data(struct epm_adc_drv *epm_adc,
		struct epm_psoc_get_data *psoc_get_meas)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[10], rx_buf[10];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = psoc_get_meas->cmd;
	tx_buf[1] = psoc_get_meas->dev_num;
	tx_buf[2] = psoc_get_meas->chan_num;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	psoc_get_meas->cmd		= rx_buf[0];
	psoc_get_meas->dev_num		= rx_buf[1];
	psoc_get_meas->chan_num		= rx_buf[2];
	psoc_get_meas->timestamp_resp_value = (rx_buf[3] << 24) |
			(rx_buf[4] << 16) | (rx_buf[5] << 8) |
			rx_buf[6];
	psoc_get_meas->reading_raw = (rx_buf[7] << 8) | rx_buf[8];

	return rc;
}

static int epm_psoc_get_buffered_data(struct epm_adc_drv *epm_adc,
		struct epm_psoc_get_buffered_data *psoc_get_meas)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[64], rx_buf[64];
	int rc = 0, i;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = psoc_get_meas->cmd;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	psoc_get_meas->cmd		= rx_buf[0];
	psoc_get_meas->dev_num		= rx_buf[1];
	psoc_get_meas->status_mask	= rx_buf[2];
	psoc_get_meas->chan_idx		= rx_buf[3];
	psoc_get_meas->chan_mask	= (rx_buf[4] << 24 |
		rx_buf[5] << 16 | rx_buf[6] << 8
			| rx_buf[7]);
	psoc_get_meas->timestamp_start	= (rx_buf[8] << 24 |
			rx_buf[9] << 16 | rx_buf[10] << 8
			| rx_buf[11]);
	psoc_get_meas->timestamp_end	= (rx_buf[12] << 24 |
			rx_buf[13] << 16 | rx_buf[14] << 8
			| rx_buf[15]);

	for (i = 0; i < EPM_PSOC_BUFFERED_DATA_LENGTH; i++)
		psoc_get_meas->buff_data[i] = rx_buf[16 + i];

	return rc;
}

static int epm_psoc_get_timestamp(struct epm_adc_drv *epm_adc,
		struct epm_psoc_system_time_stamp *psoc_timestamp)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[10], rx_buf[10];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	psoc_timestamp->cmd = EPM_PSOC_GET_SYSTEM_TIMESTAMP_CMD;
	tx_buf[0] = psoc_timestamp->cmd;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	psoc_timestamp->cmd		= rx_buf[0];
	psoc_timestamp->timestamp = rx_buf[1] << 24 | rx_buf[2] << 16 |
					rx_buf[3] << 8 | rx_buf[4];

	return rc;
}

static int epm_psoc_set_timestamp(struct epm_adc_drv *epm_adc,
		struct epm_psoc_system_time_stamp *psoc_timestamp)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[10], rx_buf[10];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	psoc_timestamp->cmd = EPM_PSOC_SET_SYSTEM_TIMESTAMP_CMD;
	tx_buf[0] = psoc_timestamp->cmd;
	tx_buf[1] = (psoc_timestamp->timestamp >> 24) & 0xff;
	tx_buf[2] = (psoc_timestamp->timestamp >> 16) & 0xff;
	tx_buf[3] = (psoc_timestamp->timestamp >> 8) & 0xff;
	tx_buf[4] = (psoc_timestamp->timestamp & 0xff);

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	psoc_timestamp->cmd		= rx_buf[0];
	psoc_timestamp->timestamp = rx_buf[1] << 24 | rx_buf[2] << 16 |
					rx_buf[3] << 8 | rx_buf[4];

	return rc;
}

static int epm_psoc_get_avg_buffered_switch_data(struct epm_adc_drv *epm_adc,
		struct epm_psoc_get_avg_buffered_switch_data *psoc_get_meas)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[64], rx_buf[64];
	int rc = 0, i = 0, j = 0, z = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = psoc_get_meas->cmd;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	psoc_get_meas->cmd		= rx_buf[0];
	psoc_get_meas->status		= rx_buf[1];
	psoc_get_meas->timestamp_start	= (rx_buf[2] << 24 |
			rx_buf[3] << 16 | rx_buf[4] << 8
			| rx_buf[5]);
	psoc_get_meas->channel_mask	= (rx_buf[6] << 24 |
		rx_buf[7] << 16 | rx_buf[8] << 8
			| rx_buf[9]);

	for (i = 0; i < EPM_PSOC_BUFFERED_DATA_LENGTH2; i++)
		psoc_get_meas->avg_data[i] = rx_buf[10 + i];

	i = j = 0;
	for (z = 0; z < 4; z++) {
		psoc_get_meas->data[i].channel = i;
		psoc_get_meas->data[i].avg_buffer_sample =
				rx_buf[10 + j] & EPM_AVG_BUF_MASK1;
		i++;
		j++;
		psoc_get_meas->data[i].avg_buffer_sample =
			rx_buf[10 + j] & EPM_AVG_BUF_MASK2;
		i++;
		j++;
		psoc_get_meas->data[i].avg_buffer_sample =
				rx_buf[10 + j] & EPM_AVG_BUF_MASK3;
		psoc_get_meas->data[i].avg_buffer_sample <<= 8;
		j++;
		psoc_get_meas->data[i].avg_buffer_sample =
		psoc_get_meas->data[i].avg_buffer_sample |
				(rx_buf[10 + j] & EPM_AVG_BUF_MASK4);
		i++;
		j++;
		psoc_get_meas->data[i].avg_buffer_sample =
				rx_buf[10 + j] & EPM_AVG_BUF_MASK5;
		i++;
		j++;
		psoc_get_meas->data[i].avg_buffer_sample =
				rx_buf[10 + j] & EPM_AVG_BUF_MASK6;
		i++;
		j++;
		psoc_get_meas->data[i].avg_buffer_sample =
				rx_buf[10 + j] & EPM_AVG_BUF_MASK7;
		psoc_get_meas->data[i].avg_buffer_sample <<= 4;
		j++;
		psoc_get_meas->data[i].avg_buffer_sample =
		psoc_get_meas->data[i].avg_buffer_sample |
				(rx_buf[10 + j] & EPM_AVG_BUF_MASK8);
		i++;
		j++;
		psoc_get_meas->data[i].avg_buffer_sample =
				rx_buf[10 + j] & EPM_AVG_BUF_MASK9;
		i++;
		j++;
		psoc_get_meas->data[i].avg_buffer_sample =
				rx_buf[10 + j] & EPM_AVG_BUF_MASK10;
	}

	for (z = 0; z < 32; z++) {
		if (psoc_get_meas->data[z].avg_buffer_sample != 0)
			psoc_get_meas->data[z].result = epm_psoc_scale_result(
				psoc_get_meas->data[z].avg_buffer_sample, z);
	}

	return rc;
}

static int epm_psoc_set_vadc(struct epm_adc_drv *epm_adc,
		struct epm_psoc_set_vadc *psoc_set_vadc)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[10], rx_buf[10];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = psoc_set_vadc->cmd;
	tx_buf[1] = psoc_set_vadc->vadc_dev;
	tx_buf[2] = (psoc_set_vadc->vadc_voltage & 0xff000000) >> 24;
	tx_buf[3] = (psoc_set_vadc->vadc_voltage & 0xff0000) >> 16;
	tx_buf[4] = (psoc_set_vadc->vadc_voltage & 0xff00) >> 8;
	tx_buf[5] = psoc_set_vadc->vadc_voltage & 0xff;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	psoc_set_vadc->cmd		= rx_buf[0];
	psoc_set_vadc->vadc_dev		= rx_buf[1];
	psoc_set_vadc->vadc_voltage = (rx_buf[2] << 24) | (rx_buf[3] << 16) |
					(rx_buf[4] << 8) | (rx_buf[5]);

	return rc;
}

static int epm_psoc_set_channel_switch(struct epm_adc_drv *epm_adc,
		struct epm_psoc_set_channel_switch *psoc_channel_switch)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[10], rx_buf[10];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = psoc_channel_switch->cmd;
	tx_buf[1] = psoc_channel_switch->dev;
	tx_buf[2] = (psoc_channel_switch->delay & 0xff000000) >> 24;
	tx_buf[3] = (psoc_channel_switch->delay & 0xff0000) >> 16;
	tx_buf[4] = (psoc_channel_switch->delay & 0xff00) >> 8;
	tx_buf[5] = psoc_channel_switch->delay & 0xff;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	psoc_channel_switch->cmd		= rx_buf[0];
	psoc_channel_switch->dev		= rx_buf[1];
	psoc_channel_switch->delay		= rx_buf[2] << 24 |
					rx_buf[3] << 16 |
					rx_buf[4] << 8 | rx_buf[5];

	return rc;
}

static int epm_psoc_clear_buffer(struct epm_adc_drv *epm_adc)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[3], rx_buf[3];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = EPM_PSOC_CLEAR_BUFFER_CMD;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = rx_buf[1];

	return rc;
}

static int epm_psoc_get_gpio_buffer_data(struct epm_adc_drv *epm_adc,
			struct epm_get_gpio_buffer_resp *gpio_resp_pkt)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[7], rx_buf[7];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = EPM_PSOC_GET_GPIO_BUFFER_CMD;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	gpio_resp_pkt->cmd = rx_buf[0];
	gpio_resp_pkt->status = rx_buf[1];
	gpio_resp_pkt->bitmask_monitor_pin = rx_buf[2];
	gpio_resp_pkt->timestamp = rx_buf[3] << 24 | rx_buf[4] << 16 |
					rx_buf[5] << 8 | tx_buf[6];

	return rc;
}

static int epm_psoc_gpio_buffer_request_configure(struct epm_adc_drv *epm_adc,
			struct epm_gpio_buffer_request *gpio_request)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[2], rx_buf[2];
	int rc = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof t);
	memset(tx_buf, 0, sizeof tx_buf);
	memset(rx_buf, 0, sizeof tx_buf);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = EPM_PSOC_GPIO_BUFFER_REQUEST_CMD;
	tx_buf[1] = gpio_request->bitmask_monitor_pin;

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_ADS_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	gpio_request->cmd = rx_buf[0];
	gpio_request->status = rx_buf[1];

	return rc;
}

static long epm_adc_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	struct epm_adc_drv *epm_adc = epm_adc_drv;

	switch (cmd) {
	case EPM_ADC_REQUEST:
		{
			struct epm_chan_request conv;
			int rc;

			if (copy_from_user(&conv, (void __user *)arg,
					sizeof(struct epm_chan_request)))
				return -EFAULT;

			rc = epm_adc_blocking_conversion(epm_adc, &conv);
			if (rc) {
				pr_err("Failed EPM conversion:%d\n", rc);
				return rc;
			}

			if (copy_to_user((void __user *)arg, &conv,
				sizeof(struct epm_chan_request)))
				return -EFAULT;
			break;
		}
	case EPM_ADC_INIT:
		{
			uint32_t result;
			if (!epm_adc_expander_register) {
				result = epm_adc_i2c_expander_register();
				if (result) {
					pr_err("Failed i2c register:%d\n",
								result);
					return result;
				}
				epm_adc_expander_register = true;
			}

			result = epm_adc_hw_init(epm_adc);

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	case EPM_ADC_DEINIT:
		{
			uint32_t result;
			result = epm_adc_hw_deinit(epm_adc);

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	case EPM_MARKER1_REQUEST:
		{
			struct epm_marker_level marker_init;
			uint32_t result;

			if (copy_from_user(&marker_init, (void __user *)arg,
					sizeof(struct epm_marker_level)))
				return -EFAULT;

			result = epm_set_marker1(&marker_init);

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	case EPM_MARKER2_REQUEST:
		{
			struct epm_marker_level marker_init;
			uint32_t result;

			if (copy_from_user(&marker_init, (void __user *)arg,
					sizeof(struct epm_marker_level)))
				return -EFAULT;

			result = epm_set_marker2(&marker_init);

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	case EPM_MARKER1_RELEASE:
		{
			uint32_t result;
			result = epm_marker1_release();

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	case EPM_MARKER2_RELEASE:
		{
			uint32_t result;
			result = epm_marker2_release();

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_ADC_INIT:
		{
			struct epm_psoc_init_resp psoc_init;
			int rc;

			if (copy_from_user(&psoc_init, (void __user *)arg,
					sizeof(struct epm_psoc_init_resp)))
				return -EFAULT;

			psoc_init.cmd = EPM_PSOC_INIT_CMD;
			rc = epm_psoc_init(epm_adc, &psoc_init);
			if (rc) {
				pr_err("PSOC initialization failed\n");
				return -EINVAL;
			}

			if (!rc) {
				rc = epm_adc_psoc_gpio_init(true);
				if (rc) {
					pr_err("GPIO init failed\n");
					return -EINVAL;
				}
			}

			if (copy_to_user((void __user *)arg, &psoc_init,
				sizeof(struct epm_psoc_init_resp)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_ADC_DEINIT:
		{
			uint32_t result;
			result = epm_adc_psoc_gpio_init(false);

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_ADC_CHANNEL_ENABLE:
	case EPM_PSOC_ADC_CHANNEL_DISABLE:
		{
			struct epm_psoc_channel_configure psoc_chan_configure;
			int rc;

			if (copy_from_user(&psoc_chan_configure,
				(void __user *)arg,
				sizeof(struct epm_psoc_channel_configure)))
				return -EFAULT;

			psoc_chan_configure.cmd =
					EPM_PSOC_CHANNEL_ENABLE_DISABLE_CMD;
			rc = epm_psoc_channel_configure(epm_adc,
							&psoc_chan_configure);
			if (rc) {
				pr_err("PSOC channel configure failed\n");
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg,
				&psoc_chan_configure,
				sizeof(struct epm_psoc_channel_configure)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_ADC_SET_AVERAGING:
		{
			struct epm_psoc_set_avg psoc_set_avg;
			int rc;

			if (copy_from_user(&psoc_set_avg, (void __user *)arg,
					sizeof(struct epm_psoc_set_avg)))
				return -EFAULT;

			psoc_set_avg.cmd = EPM_PSOC_SET_AVERAGING_CMD;
			rc = epm_psoc_set_averaging(epm_adc, &psoc_set_avg);
			if (rc) {
				pr_err("PSOC averaging failed\n");
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg, &psoc_set_avg,
					sizeof(struct epm_psoc_set_avg)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_ADC_GET_LAST_MEASUREMENT:
		{
			struct epm_psoc_get_data psoc_get_data;
			int rc;

			if (copy_from_user(&psoc_get_data,
					(void __user *)arg,
					sizeof(struct epm_psoc_get_data)))
				return -EFAULT;

			psoc_get_data.cmd = EPM_PSOC_GET_LAST_MEASUREMENT_CMD;
			rc = epm_psoc_get_data(epm_adc, &psoc_get_data);
			if (rc) {
				pr_err("PSOC last measured data failed\n");
				return -EINVAL;
			}

			psoc_get_data.reading_value = epm_psoc_scale_result(
				psoc_get_data.reading_raw,
				psoc_get_data.chan_num);

			if (copy_to_user((void __user *)arg, &psoc_get_data,
				sizeof(struct epm_psoc_get_data)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_ADC_GET_BUFFERED_DATA:
		{
			struct epm_psoc_get_buffered_data psoc_get_data;
			int rc;

			if (copy_from_user(&psoc_get_data,
				(void __user *)arg,
				sizeof(struct epm_psoc_get_buffered_data)))
				return -EFAULT;

			psoc_get_data.cmd = EPM_PSOC_GET_BUFFERED_DATA_CMD;
			rc = epm_psoc_get_buffered_data(epm_adc,
								&psoc_get_data);
			if (rc) {
				pr_err("PSOC buffered measurement failed\n");
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg, &psoc_get_data,
				sizeof(struct epm_psoc_get_buffered_data)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_ADC_GET_SYSTEM_TIMESTAMP:
		{
			struct epm_psoc_system_time_stamp psoc_timestamp;
			int rc;

			if (copy_from_user(&psoc_timestamp,
				(void __user *)arg,
				sizeof(struct epm_psoc_system_time_stamp)))
				return -EFAULT;

			rc = epm_psoc_get_timestamp(epm_adc, &psoc_timestamp);
			if (rc) {
				pr_err("PSOC get timestamp failed\n");
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg, &psoc_timestamp,
				sizeof(struct epm_psoc_system_time_stamp)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_ADC_SET_SYSTEM_TIMESTAMP:
		{
			struct epm_psoc_system_time_stamp psoc_timestamp;
			int rc;

			if (copy_from_user(&psoc_timestamp,
				(void __user *)arg,
				sizeof(struct epm_psoc_system_time_stamp)))
				return -EFAULT;

			rc = epm_psoc_set_timestamp(epm_adc, &psoc_timestamp);
			if (rc) {
				pr_err("PSOC set timestamp failed\n");
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg, &psoc_timestamp,
				sizeof(struct epm_psoc_system_time_stamp)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_ADC_GET_AVERAGE_DATA:
		{
			struct epm_psoc_get_avg_buffered_switch_data
								psoc_get_data;
			int rc;

			if (copy_from_user(&psoc_get_data,
				(void __user *)arg,
				sizeof(struct
				epm_psoc_get_avg_buffered_switch_data)))
				return -EFAULT;

			psoc_get_data.cmd = EPM_PSOC_GET_AVERAGED_DATA_CMD;
			rc = epm_psoc_get_avg_buffered_switch_data(epm_adc,
								&psoc_get_data);
			if (rc) {
				pr_err("Get averaged buffered data failed\n");
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg, &psoc_get_data,
				sizeof(struct
				epm_psoc_get_avg_buffered_switch_data)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_SET_CHANNEL_SWITCH:
		{
			struct epm_psoc_set_channel_switch psoc_channel_switch;
			int rc;

			if (copy_from_user(&psoc_channel_switch,
				(void __user *)arg,
				sizeof(struct epm_psoc_set_channel_switch)))
				return -EFAULT;

			rc = epm_psoc_set_channel_switch(epm_adc,
						&psoc_channel_switch);
			if (rc) {
				pr_err("PSOC channel switch failed\n");
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg,
				&psoc_channel_switch,
				sizeof(struct epm_psoc_set_channel_switch)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_CLEAR_BUFFER:
		{
			int rc;
			rc = epm_psoc_clear_buffer(epm_adc);
			if (rc) {
				pr_err("PSOC clear buffer failed\n");
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg, &rc,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_ADC_SET_VADC_REFERENCE:
		{
			struct epm_psoc_set_vadc psoc_set_vadc;
			int rc;

			if (copy_from_user(&psoc_set_vadc,
					(void __user *)arg,
					sizeof(struct epm_psoc_set_vadc)))
				return -EFAULT;

			rc = epm_psoc_set_vadc(epm_adc, &psoc_set_vadc);
			if (rc) {
				pr_err("PSOC set VADC failed\n");
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg, &psoc_set_vadc,
				sizeof(struct epm_psoc_set_vadc)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_GPIO_BUFFER_REQUEST:
		{
			struct epm_gpio_buffer_request gpio_request;
			int rc;

			if (copy_from_user(&gpio_request,
					(void __user *)arg,
					sizeof(struct epm_gpio_buffer_request)))
				return -EFAULT;

			rc = epm_psoc_gpio_buffer_request_configure(epm_adc,
							&gpio_request);
			if (rc) {
				pr_err("PSOC buffer request failed\n");
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg, &gpio_request,
				sizeof(struct epm_gpio_buffer_request)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_GET_GPIO_BUFFER_DATA:
		{
			struct epm_get_gpio_buffer_resp gpio_resp_pkt;
			int rc;

			if (copy_from_user(&gpio_resp_pkt,
				(void __user *)arg,
				sizeof(struct epm_get_gpio_buffer_resp)))
				return -EFAULT;

			rc = epm_psoc_get_gpio_buffer_data(epm_adc,
							&gpio_resp_pkt);
			if (rc) {
				pr_err("PSOC get buffer data failed\n");
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg, &gpio_resp_pkt,
				sizeof(struct epm_get_gpio_buffer_resp)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_PAUSE_CONVERSION_REQUEST:
		{
			uint32_t result;
			result = epm_psoc_pause_conversion(epm_adc);

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_UNPAUSE_CONVERSION_REQUEST:
		{
			uint32_t result;
			result = epm_psoc_unpause_conversion(epm_adc);

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	default:
		return -EINVAL;
	}

	return 0;
}

const struct file_operations epm_adc_fops = {
	.unlocked_ioctl = epm_adc_ioctl,
};

static ssize_t epm_adc_psoc_show_in(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct epm_adc_drv *epm_adc = epm_adc_drv;
	struct epm_psoc_init_resp init_resp;
	struct epm_psoc_channel_configure psoc_chan_configure;
	struct epm_psoc_get_data psoc_get_meas;
	int rc = 0;

	rc = epm_adc_psoc_gpio_init(true);
	if (rc) {
		pr_err("GPIO init failed\n");
		return 0;
	}
	usleep_range(EPM_GLOBAL_ENABLE_MIN_DELAY,
				EPM_GLOBAL_ENABLE_MAX_DELAY);

	init_resp.cmd = EPM_PSOC_INIT_CMD;
	rc = epm_psoc_init(epm_adc, &init_resp);
	if (rc) {
		pr_err("PSOC init failed %d\n", rc);
		return 0;
	}


	psoc_chan_configure.channel_num = (1 << attr->index);
	psoc_chan_configure.cmd = EPM_PSOC_CHANNEL_ENABLE_DISABLE_CMD;
	rc = epm_psoc_channel_configure(epm_adc, &psoc_chan_configure);
	if (rc) {
		pr_err("PSOC channel configure failed\n");
		return 0;
	}

	usleep_range(EPM_GLOBAL_ENABLE_MIN_DELAY,
				EPM_GLOBAL_ENABLE_MAX_DELAY);

	psoc_get_meas.cmd = EPM_PSOC_GET_LAST_MEASUREMENT_CMD;
	psoc_get_meas.dev_num = 0;
	psoc_get_meas.chan_num = attr->index;
	rc = epm_psoc_get_data(epm_adc, &psoc_get_meas);
	if (rc) {
		pr_err("PSOC get data failed\n");
		return 0;
	}

	psoc_get_meas.reading_value = epm_psoc_scale_result(
			psoc_get_meas.reading_value,
			attr->index);

	rc = epm_adc_psoc_gpio_init(false);
	if (rc) {
		pr_err("GPIO de-init failed\n");
		return 0;
	}

	return snprintf(buf, 16, "Result: %d\n", psoc_get_meas.reading_value);
}

static struct sensor_device_attribute epm_adc_psoc_in_attrs[] = {
	SENSOR_ATTR(psoc0_chan0,  S_IRUGO, epm_adc_psoc_show_in, NULL, 0),
	SENSOR_ATTR(psoc0_chan1,  S_IRUGO, epm_adc_psoc_show_in, NULL, 1),
	SENSOR_ATTR(psoc0_chan2,  S_IRUGO, epm_adc_psoc_show_in, NULL, 2),
	SENSOR_ATTR(psoc0_chan3,  S_IRUGO, epm_adc_psoc_show_in, NULL, 3),
	SENSOR_ATTR(psoc0_chan4,  S_IRUGO, epm_adc_psoc_show_in, NULL, 4),
	SENSOR_ATTR(psoc0_chan5,  S_IRUGO, epm_adc_psoc_show_in, NULL, 5),
	SENSOR_ATTR(psoc0_chan6,  S_IRUGO, epm_adc_psoc_show_in, NULL, 6),
	SENSOR_ATTR(psoc0_chan7,  S_IRUGO, epm_adc_psoc_show_in, NULL, 7),
	SENSOR_ATTR(psoc0_chan8,  S_IRUGO, epm_adc_psoc_show_in, NULL, 8),
	SENSOR_ATTR(psoc0_chan9,  S_IRUGO, epm_adc_psoc_show_in, NULL, 9),
	SENSOR_ATTR(psoc0_chan10, S_IRUGO, epm_adc_psoc_show_in, NULL, 10),
	SENSOR_ATTR(psoc0_chan11, S_IRUGO, epm_adc_psoc_show_in, NULL, 11),
	SENSOR_ATTR(psoc0_chan12, S_IRUGO, epm_adc_psoc_show_in, NULL, 12),
	SENSOR_ATTR(psoc0_chan13, S_IRUGO, epm_adc_psoc_show_in, NULL, 13),
	SENSOR_ATTR(psoc0_chan14, S_IRUGO, epm_adc_psoc_show_in, NULL, 14),
	SENSOR_ATTR(psoc0_chan15, S_IRUGO, epm_adc_psoc_show_in, NULL, 15),
	SENSOR_ATTR(psoc0_chan16,  S_IRUGO, epm_adc_psoc_show_in, NULL, 16),
	SENSOR_ATTR(psoc0_chan17,  S_IRUGO, epm_adc_psoc_show_in, NULL, 17),
	SENSOR_ATTR(psoc0_chan18,  S_IRUGO, epm_adc_psoc_show_in, NULL, 18),
	SENSOR_ATTR(psoc0_chan19,  S_IRUGO, epm_adc_psoc_show_in, NULL, 19),
	SENSOR_ATTR(psoc0_chan20,  S_IRUGO, epm_adc_psoc_show_in, NULL, 20),
	SENSOR_ATTR(psoc0_chan21,  S_IRUGO, epm_adc_psoc_show_in, NULL, 21),
	SENSOR_ATTR(psoc0_chan22,  S_IRUGO, epm_adc_psoc_show_in, NULL, 22),
	SENSOR_ATTR(psoc0_chan23,  S_IRUGO, epm_adc_psoc_show_in, NULL, 23),
	SENSOR_ATTR(psoc0_chan24,  S_IRUGO, epm_adc_psoc_show_in, NULL, 24),
	SENSOR_ATTR(psoc0_chan25,  S_IRUGO, epm_adc_psoc_show_in, NULL, 25),
	SENSOR_ATTR(psoc0_chan26, S_IRUGO, epm_adc_psoc_show_in, NULL, 26),
	SENSOR_ATTR(psoc0_chan27, S_IRUGO, epm_adc_psoc_show_in, NULL, 27),
	SENSOR_ATTR(psoc0_chan28, S_IRUGO, epm_adc_psoc_show_in, NULL, 28),
	SENSOR_ATTR(psoc0_chan29, S_IRUGO, epm_adc_psoc_show_in, NULL, 29),
	SENSOR_ATTR(psoc0_chan30, S_IRUGO, epm_adc_psoc_show_in, NULL, 30),
	SENSOR_ATTR(psoc0_chan31, S_IRUGO, epm_adc_psoc_show_in, NULL, 31),
};

static int __devinit epm_adc_psoc_init_hwmon(struct spi_device *spi,
						struct epm_adc_drv *epm_adc)
{
	int i, rc, num_chans = 31;

	for (i = 0; i < num_chans; i++) {
		rc = device_create_file(&spi->dev,
				&epm_adc_psoc_in_attrs[i].dev_attr);
		if (rc) {
			dev_err(&spi->dev, "device_create_file failed\n");
			return rc;
		}
	}

	return 0;
}

static int get_device_tree_data(struct spi_device *spi)
{
	const struct device_node *node = spi->dev.of_node;
	struct epm_adc_drv *epm_adc;
	u32 *epm_ch_gain, *epm_ch_rsense;
	u32 rc = 0, epm_num_channels, i, channel_mask;

	if (!node)
		return -EINVAL;

	rc = of_property_read_u32(node,
			"qcom,channels", &epm_num_channels);
	if (rc) {
		dev_err(&spi->dev, "missing channel numbers\n");
		return -ENODEV;
	}

	epm_ch_gain = devm_kzalloc(&spi->dev,
			epm_num_channels * sizeof(u32), GFP_KERNEL);
	if (!epm_ch_gain) {
		dev_err(&spi->dev, "cannot allocate gain\n");
		return -ENOMEM;
	}

	epm_ch_rsense = devm_kzalloc(&spi->dev,
			epm_num_channels * sizeof(u32), GFP_KERNEL);
	if (!epm_ch_rsense) {
		dev_err(&spi->dev, "cannot allocate rsense\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(node,
			"qcom,gain", epm_ch_gain, epm_num_channels);
	if (rc) {
		dev_err(&spi->dev, "invalid gain property:%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32_array(node,
			"qcom,rsense", epm_ch_rsense, epm_num_channels);
	if (rc) {
		dev_err(&spi->dev, "invalid rsense property:%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32(node,
			"qcom,channel-type", &channel_mask);
	if (rc) {
		dev_err(&spi->dev, "missing channel mask\n");
		return -ENODEV;
	}

	epm_adc = devm_kzalloc(&spi->dev,
			sizeof(struct epm_adc_drv) +
			(epm_num_channels *
			sizeof(struct epm_chan_properties)),
			GFP_KERNEL);
	if (!epm_adc) {
		dev_err(&spi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < epm_num_channels; i++) {
		epm_adc->epm_psoc_ch_prop[i].resistorvalue =
							epm_ch_rsense[i];
		epm_adc->epm_psoc_ch_prop[i].gain =
							epm_ch_gain[i];
	}

	epm_adc->channel_mask = channel_mask;
	epm_adc_drv = epm_adc;

	return 0;
}

static int __devinit epm_adc_psoc_spi_probe(struct spi_device *spi)
{

	struct epm_adc_drv *epm_adc;
	struct device_node *node = spi->dev.of_node;
	int rc = 0;

	if (node) {
		rc = get_device_tree_data(spi);
		if (rc)
			return rc;
	} else {
		epm_adc = epm_adc_drv;
		epm_adc_drv->epm_spi_client = spi;
		epm_adc_drv->epm_spi_client->bits_per_word =
				EPM_ADC_ADS_SPI_BITS_PER_WORD;
		return rc;
	}

	epm_adc = epm_adc_drv;
	epm_adc->misc.name = EPM_ADC_DRIVER_NAME;
	epm_adc->misc.minor = MISC_DYNAMIC_MINOR;

	if (node) {
		epm_adc->misc.fops = &epm_adc_fops;
		if (misc_register(&epm_adc->misc)) {
			pr_err("Unable to register misc device!\n");
			return -EFAULT;
		}
	}

	epm_adc_drv->epm_spi_client = spi;
	epm_adc_drv->epm_spi_client->bits_per_word =
				EPM_ADC_ADS_SPI_BITS_PER_WORD;
	rc = epm_adc_psoc_init_hwmon(spi, epm_adc);
	if (rc) {
		dev_err(&spi->dev, "msm_adc_dev_init failed\n");
		return rc;
	}

	epm_adc->hwmon = hwmon_device_register(&spi->dev);
	if (IS_ERR(epm_adc->hwmon)) {
		dev_err(&spi->dev, "hwmon_device_register failed\n");
		return rc;
	}

	mutex_init(&epm_adc->conv_lock);
	return rc;
}

static int __devexit epm_adc_psoc_spi_remove(struct spi_device *spi)
{
	epm_adc_drv->epm_spi_client = NULL;
	return 0;
}

static const struct of_device_id epm_adc_psoc_match_table[] = {
	{	.compatible = "cy,epm-adc-cy8c5568lti-114",
	},
	{}
};

static struct spi_driver epm_spi_driver = {
	.probe = epm_adc_psoc_spi_probe,
	.remove = __devexit_p(epm_adc_psoc_spi_remove),
	.driver = {
		.name = EPM_ADC_DRIVER_NAME,
		.of_match_table = epm_adc_psoc_match_table,
	},
};

static ssize_t epm_adc_show_in(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	struct epm_adc_drv *epm_adc = dev_get_drvdata(dev);
	struct epm_adc_platform_data *pdata = epm_adc->pdev->dev.platform_data;
	struct epm_chan_request conv;
	int rc = 0;

	conv.device_idx = attr->index / pdata->chan_per_adc;
	conv.channel_idx = attr->index % pdata->chan_per_adc;
	conv.physical = 0;

	if (!epm_adc_expander_register) {
		rc = epm_adc_i2c_expander_register();
		if (rc) {
			pr_err("I2C expander register failed:%d\n", rc);
			return rc;
		}
		epm_adc_expander_register = true;
	}

	rc = epm_adc_hw_init(epm_adc);
	if (rc) {
		pr_err("%s: epm_adc_hw_init() failed, rc = %d",
			__func__, rc);
		return 0;
	}

	rc = epm_adc_blocking_conversion(epm_adc, &conv);
	if (rc) {
		pr_err("%s: epm_adc_blocking_conversion() failed, rc = %d\n",
			__func__, rc);
		return 0;
	}

	rc = epm_adc_hw_deinit(epm_adc);
	if (rc) {
		pr_err("%s: epm_adc_hw_deinit() failed, rc = %d",
			__func__, rc);
		return 0;
	}

	return snprintf(buf, 16, "Result: %d\n", conv.physical);
}

static struct sensor_device_attribute epm_adc_in_attrs[] = {
	SENSOR_ATTR(ads0_chan0,  S_IRUGO, epm_adc_show_in, NULL, 0),
	SENSOR_ATTR(ads0_chan1,  S_IRUGO, epm_adc_show_in, NULL, 1),
	SENSOR_ATTR(ads0_chan2,  S_IRUGO, epm_adc_show_in, NULL, 2),
	SENSOR_ATTR(ads0_chan3,  S_IRUGO, epm_adc_show_in, NULL, 3),
	SENSOR_ATTR(ads0_chan4,  S_IRUGO, epm_adc_show_in, NULL, 4),
	SENSOR_ATTR(ads0_chan5,  S_IRUGO, epm_adc_show_in, NULL, 5),
	SENSOR_ATTR(ads0_chan6,  S_IRUGO, epm_adc_show_in, NULL, 6),
	SENSOR_ATTR(ads0_chan7,  S_IRUGO, epm_adc_show_in, NULL, 7),
	SENSOR_ATTR(ads0_chan8,  S_IRUGO, epm_adc_show_in, NULL, 8),
	SENSOR_ATTR(ads0_chan9,  S_IRUGO, epm_adc_show_in, NULL, 9),
	SENSOR_ATTR(ads0_chan10, S_IRUGO, epm_adc_show_in, NULL, 10),
	SENSOR_ATTR(ads0_chan11, S_IRUGO, epm_adc_show_in, NULL, 11),
	SENSOR_ATTR(ads0_chan12, S_IRUGO, epm_adc_show_in, NULL, 12),
	SENSOR_ATTR(ads0_chan13, S_IRUGO, epm_adc_show_in, NULL, 13),
	SENSOR_ATTR(ads0_chan14, S_IRUGO, epm_adc_show_in, NULL, 14),
	SENSOR_ATTR(ads0_chan15, S_IRUGO, epm_adc_show_in, NULL, 15),
	SENSOR_ATTR(ads1_chan0,  S_IRUGO, epm_adc_show_in, NULL, 16),
	SENSOR_ATTR(ads1_chan1,  S_IRUGO, epm_adc_show_in, NULL, 17),
	SENSOR_ATTR(ads1_chan2,  S_IRUGO, epm_adc_show_in, NULL, 18),
	SENSOR_ATTR(ads1_chan3,  S_IRUGO, epm_adc_show_in, NULL, 19),
	SENSOR_ATTR(ads1_chan4,  S_IRUGO, epm_adc_show_in, NULL, 20),
	SENSOR_ATTR(ads1_chan5,  S_IRUGO, epm_adc_show_in, NULL, 21),
	SENSOR_ATTR(ads1_chan6,  S_IRUGO, epm_adc_show_in, NULL, 22),
	SENSOR_ATTR(ads1_chan7,  S_IRUGO, epm_adc_show_in, NULL, 23),
	SENSOR_ATTR(ads1_chan8,  S_IRUGO, epm_adc_show_in, NULL, 24),
	SENSOR_ATTR(ads1_chan9,  S_IRUGO, epm_adc_show_in, NULL, 25),
	SENSOR_ATTR(ads1_chan10, S_IRUGO, epm_adc_show_in, NULL, 26),
	SENSOR_ATTR(ads1_chan11, S_IRUGO, epm_adc_show_in, NULL, 27),
	SENSOR_ATTR(ads1_chan12, S_IRUGO, epm_adc_show_in, NULL, 28),
	SENSOR_ATTR(ads1_chan13, S_IRUGO, epm_adc_show_in, NULL, 29),
	SENSOR_ATTR(ads1_chan14, S_IRUGO, epm_adc_show_in, NULL, 30),
	SENSOR_ATTR(ads1_chan15, S_IRUGO, epm_adc_show_in, NULL, 31),
};

static int __devinit epm_adc_init_hwmon(struct platform_device *pdev,
					       struct epm_adc_drv *epm_adc)
{
	struct epm_adc_platform_data *pdata = pdev->dev.platform_data;
	int i, rc, num_chans = pdata->num_channels;

	for (i = 0; i < num_chans; i++) {
		rc = device_create_file(&pdev->dev,
				&epm_adc_in_attrs[i].dev_attr);
		if (rc) {
			dev_err(&pdev->dev, "device_create_file failed\n");
			return rc;
		}
	}

	return 0;
}

static int __devinit epm_adc_probe(struct platform_device *pdev)
{
	struct epm_adc_drv *epm_adc;
	struct epm_adc_platform_data *pdata = pdev->dev.platform_data;
	int rc = 0;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data?\n");
		return -EINVAL;
	}

	epm_adc = kzalloc(sizeof(struct epm_adc_drv), GFP_KERNEL);
	if (!epm_adc) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, epm_adc);
	epm_adc_drv = epm_adc;
	epm_adc->pdev = pdev;

	epm_adc->misc.name = EPM_ADC_DRIVER_NAME;
	epm_adc->misc.minor = MISC_DYNAMIC_MINOR;
	epm_adc->misc.fops = &epm_adc_fops;

	if (misc_register(&epm_adc->misc)) {
		dev_err(&pdev->dev, "Unable to register misc device!\n");
		return -EFAULT;
	}

	rc = epm_adc_init_hwmon(pdev, epm_adc);
	if (rc) {
		dev_err(&pdev->dev, "msm_adc_dev_init failed\n");
		misc_deregister(&epm_adc->misc);
		return rc;
	}

	epm_adc->hwmon = hwmon_device_register(&pdev->dev);
	if (IS_ERR(epm_adc->hwmon)) {
		dev_err(&pdev->dev, "hwmon_device_register failed\n");
		misc_deregister(&epm_adc->misc);
		rc = PTR_ERR(epm_adc->hwmon);
		return rc;
	}

	mutex_init(&epm_adc->conv_lock);
	epm_i2c_info = &pdata->epm_i2c_board_info;
	epm_adc->bus_id = pdata->bus_id;
	epm_gpio_expander_base_addr = pdata->gpio_expander_base_addr;
	epm_adc_expander_register = false;

	return rc;
}

static int __devexit epm_adc_remove(struct platform_device *pdev)
{
	struct epm_adc_drv *epm_adc = platform_get_drvdata(pdev);
	struct epm_adc_platform_data *pdata = pdev->dev.platform_data;
	int num_chans = pdata->num_channels;
	int i = 0;

	for (i = 0; i < num_chans; i++)
		device_remove_file(&pdev->dev, &epm_adc_in_attrs[i].dev_attr);
	hwmon_device_unregister(epm_adc->hwmon);
	misc_deregister(&epm_adc->misc);
	epm_adc = NULL;

	return 0;
}

static struct platform_driver epm_adc_driver = {
	.probe = epm_adc_probe,
	.remove = __devexit_p(epm_adc_remove),
	.driver = {
		.name = EPM_ADC_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init epm_adc_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&epm_adc_driver);
	if (ret) {
		pr_err("%s: driver register failed, rc=%d\n", __func__, ret);
		return ret;
	}

	ret = spi_register_driver(&epm_spi_driver);
	if (ret)
		pr_err("%s: spi register failed: rc=%d\n", __func__, ret);

	return ret;
}

static void __exit epm_adc_exit(void)
{
	spi_unregister_driver(&epm_spi_driver);
	platform_driver_unregister(&epm_adc_driver);
}

module_init(epm_adc_init);
module_exit(epm_adc_exit);

MODULE_DESCRIPTION("EPM ADC Driver");
MODULE_ALIAS("platform:epm_adc");
MODULE_LICENSE("GPL v2");
