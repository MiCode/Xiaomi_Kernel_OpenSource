/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#define EPM_ADC_CONVERSION_TIME_MIN	50000
#define EPM_ADC_CONVERSION_TIME_MAX	51000

struct epm_adc_drv {
	struct platform_device		*pdev;
	struct device			*hwmon;
	struct sensor_device_attribute	*sens_attr;
	char				**fnames;
	struct spi_device		*epm_spi_client;
	struct mutex			conv_lock;
	uint32_t			bus_id;
	struct miscdevice		misc;
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
			pr_err("%s: Set GPIO_EPM_SPI_ADC2_CS_N "
					"failed\n", __func__);
			return rc;
		}
	} else {
		pr_err("%s: gpio_request GPIO_EPM_SPI_ADC2_CS_N "
				"failed\n", __func__);
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
		pr_err("epm gpio configure expander disable failed,"
			" rc = %d\n", rc);
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
	int32_t *adc_scaled_data = &conv->physical;

	/* Get the channel number */
	channel_num = (adc_raw_data[0] & EPM_ADC_ADS_CHANNEL_DATA_CHID);
	sign_bit    = 1;
	/* This is the 16-bit raw data */
	*adc_scaled_data = ((adc_raw_data[1] << 8) | adc_raw_data[2]);
	/* Obtain the internal system reading */
	if (channel_num == EPM_ADC_ADS_CHANNEL_VCC) {
		*adc_scaled_data *= EPM_ADC_SCALE_MILLI;
		*adc_scaled_data /= EPM_ADC_SCALE_CODE_VOLTS;
	} else if (channel_num == EPM_ADC_ADS_CHANNEL_GAIN) {
		*adc_scaled_data /= EPM_ADC_SCALE_CODE_GAIN;
	} else if (channel_num == EPM_ADC_ADS_CHANNEL_REF) {
		*adc_scaled_data *= EPM_ADC_SCALE_MILLI;
		*adc_scaled_data /= EPM_ADC_SCALE_CODE_VOLTS;
	} else if (channel_num == EPM_ADC_ADS_CHANNEL_TEMP) {
		/* Convert Code to micro-volts */
		/* Use this formula to get the temperature reading */
		*adc_scaled_data -= EPM_ADC_TEMP_TO_DEGC_COEFF;
		*adc_scaled_data /= EPM_ADC_TEMP_SENSOR_COEFF;
	} else if (channel_num == EPM_ADC_ADS_CHANNEL_OFFSET) {
		/* The offset should be zero */
		pr_debug("%s: ADC Channel Offset\n", __func__);
		return -EFAULT;
	} else {
		channel_num -= EPM_ADC_CHANNEL_AIN_OFFSET;
		/*
		 * Conversion for the adc channels.
		 * mvVRef is in milli-volts and resistorValue is in micro-ohms.
		 * Hence, I = V/R gives us current in kilo-amps.
		 */
		if (*adc_scaled_data & EPM_ADC_MAX_NEGATIVE_SCALE_CODE) {
			sign_bit = -1;
			*adc_scaled_data = (~*adc_scaled_data
				& EPM_ADC_NEG_LSB_CODE);
		}
		if (*adc_scaled_data != 0) {
			*adc_scaled_data *= EPM_ADC_SCALE_FACTOR;
			 /* Device is calibrated for 1LSB = VREF/7800h.*/
			*adc_scaled_data *= EPM_ADC_MILLI_VOLTS_SOURCE;
			*adc_scaled_data /= EPM_ADC_VREF_CODE;
			 /* Data will now be in micro-volts.*/
			*adc_scaled_data *= EPM_ADC_SCALE_MILLI;
			 /* Divide by amplifier gain value.*/
			*adc_scaled_data /= pdata->channel[chan_idx].gain;
			 /* Data will now be in nano-volts.*/
			*adc_scaled_data /= EPM_ADC_SCALE_FACTOR;
			*adc_scaled_data *= EPM_ADC_SCALE_MILLI;
			 /* Data is now in micro-amps.*/
			*adc_scaled_data /=
				pdata->channel[chan_idx].resistorValue;
			 /* Set the sign bit for lekage current. */
			*adc_scaled_data *= sign_bit;
		}
	}
	return 0;
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

			result = epm_adc_hw_init(epm_adc_drv);

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	case EPM_ADC_DEINIT:
		{
			uint32_t result;
			result = epm_adc_hw_deinit(epm_adc_drv);

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
	pr_debug("%s: device_idx=%d channel_idx=%d", __func__, conv.device_idx,
			conv.channel_idx);
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

static struct sensor_device_attribute epm_adc_in_attr =
	SENSOR_ATTR(NULL, S_IRUGO, epm_adc_show_in, NULL, 0);

static int __devinit epm_adc_init_hwmon(struct platform_device *pdev,
					       struct epm_adc_drv *epm_adc)
{
	struct epm_adc_platform_data *pdata = pdev->dev.platform_data;
	int num_chans = pdata->num_channels, dev_idx = 0, chan_idx = 0;
	int i = 0, rc = 0;
	const char prefix[] = "ads", postfix[] = "_chan";
	char tmpbuf[3];

	epm_adc->fnames = devm_kzalloc(&pdev->dev,
				num_chans * EPM_ADC_MAX_FNAME +
				num_chans * sizeof(char *), GFP_KERNEL);
	if (!epm_adc->fnames) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	epm_adc->sens_attr = devm_kzalloc(&pdev->dev, num_chans *
			    sizeof(struct sensor_device_attribute), GFP_KERNEL);
	if (!epm_adc->sens_attr) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		rc = -ENOMEM;
	}

	for (i = 0; i < num_chans; i++, chan_idx++) {
		epm_adc->fnames[i] = (char *)epm_adc->fnames +
			(i * EPM_ADC_MAX_FNAME) + (num_chans *
			sizeof(char *));
		if (chan_idx == pdata->chan_per_adc) {
			chan_idx = 0;
			dev_idx++;
		}
		strlcpy(epm_adc->fnames[i], prefix, EPM_ADC_MAX_FNAME);
		snprintf(tmpbuf, sizeof(tmpbuf), "%d", dev_idx);
		strlcat(epm_adc->fnames[i], tmpbuf, EPM_ADC_MAX_FNAME);
		strlcat(epm_adc->fnames[i], postfix, EPM_ADC_MAX_FNAME);
		snprintf(tmpbuf, sizeof(tmpbuf), "%d", chan_idx);
		strlcat(epm_adc->fnames[i], tmpbuf, EPM_ADC_MAX_FNAME);
		epm_adc_in_attr.index = i;
		epm_adc_in_attr.dev_attr.attr.name = epm_adc->fnames[i];
		memcpy(&epm_adc->sens_attr[i], &epm_adc_in_attr,
						sizeof(epm_adc_in_attr));
		rc = device_create_file(&pdev->dev,
				&epm_adc->sens_attr[i].dev_attr);
		if (rc) {
			dev_err(&pdev->dev, "device_create_file failed\n");
			return rc;
		}
	}

	return rc;
}

static int __devinit epm_adc_spi_probe(struct spi_device *spi)

{
	if (!epm_adc_drv)
		return -ENODEV;
	epm_adc_drv->epm_spi_client = spi;
	epm_adc_drv->epm_spi_client->bits_per_word =
				EPM_ADC_ADS_SPI_BITS_PER_WORD;

	return 0;
}

static int __devexit epm_adc_spi_remove(struct spi_device *spi)
{
	epm_adc_drv->epm_spi_client = NULL;
	return 0;
}

static struct spi_driver epm_spi_driver = {
	.probe = epm_adc_spi_probe,
	.remove = __devexit_p(epm_adc_spi_remove),
	.driver = {
		.name = EPM_ADC_DRIVER_NAME,
		.owner = THIS_MODULE,
	},
};

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

	if (epm_adc->sens_attr)
		for (i = 0; i < num_chans; i++)
			device_remove_file(&pdev->dev,
					&epm_adc->sens_attr[i].dev_attr);
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
