/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/of_gpio.h>
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

#define EPM_ADC_SPI_BITS_PER_WORD	8
#define GPIO_EPM_GLOBAL_ENABLE		86
#define GPIO_EPM_MARKER1		96
#define GPIO_EPM_MARKER2		85
#define EPM_ADC_CONVERSION_TIME_MIN	50000
#define EPM_ADC_CONVERSION_TIME_MAX	51000
/* PSoc Commands */

#define EPM_PSOC_GLOBAL_ENABLE				81
#define EPM_PSOC_VREF_VOLTAGE				2048
#define EPM_PSOC_MAX_ADC_CODE_15_BIT			32767
#define EPM_PSOC_MAX_ADC_CODE_12_BIT			4096
#define EPM_GLOBAL_ENABLE_MIN_DELAY			5000
#define EPM_GLOBAL_ENABLE_MAX_DELAY			5100

struct epm_adc_drv {
	struct platform_device		*pdev;
	struct device			*hwmon;
	struct spi_device		*epm_spi_client;
	struct mutex			conv_lock;
	uint32_t			bus_id;
	struct miscdevice		misc;
	uint32_t			channel_mask;
	uint32_t			epm_global_en_gpio;
	struct epm_chan_properties	epm_psoc_ch_prop[0];
};

static struct epm_adc_drv *epm_adc_drv;

static int epm_adc_psoc_gpio_init(struct epm_adc_drv *epm_adc,
							bool enable)
{
	int rc = 0;

	if (enable) {
		rc = gpio_request(epm_adc->epm_global_en_gpio,
							"EPM_PSOC_GLOBAL_EN");
		if (!rc) {
			gpio_direction_output(epm_adc->epm_global_en_gpio, 1);
		} else {
			pr_err("%s: Configure EPM_GLOBAL_EN Failed\n",
								__func__);
			return rc;
		}
	} else {
		gpio_direction_output(epm_adc->epm_global_en_gpio, 0);
		gpio_free(epm_adc->epm_global_en_gpio);
	}

	return 0;
}

static int epm_request_marker1(void)
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

	return 0;
}

static int epm_set_marker1(struct epm_marker_level *marker_init)
{
	gpio_set_value(GPIO_EPM_MARKER1, marker_init->level);

	return 0;
}

static int epm_request_marker2(void)
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

	return 0;
}

static int epm_set_marker2(struct epm_marker_level *marker_init)
{
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

static int epm_psoc_generic_request(struct epm_adc_drv *epm_adc,
			struct epm_generic_request *psoc_get_data)
{
	struct spi_message m;
	struct spi_transfer t;
	char tx_buf[64], rx_buf[64];
	int rc = 0, data_loop = 0;

	spi_setup(epm_adc->epm_spi_client);

	memset(&t, 0, sizeof(t));
	memset(tx_buf, 0, sizeof(tx_buf));
	memset(rx_buf, 0, sizeof(tx_buf));
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	for (data_loop = 0; data_loop < 64; data_loop++)
		tx_buf[data_loop] = psoc_get_data->buf[data_loop];

	t.len = sizeof(tx_buf);
	t.bits_per_word = EPM_ADC_SPI_BITS_PER_WORD;

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	memset(tx_buf, 0, sizeof(tx_buf));

	rc = spi_sync(epm_adc->epm_spi_client, &m);
	if (rc)
		return rc;

	for (data_loop = 0; data_loop < 64; data_loop++)
		psoc_get_data->buf[data_loop] = rx_buf[data_loop];

	return rc;
}

static long epm_adc_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	struct epm_adc_drv *epm_adc = epm_adc_drv;

	switch (cmd) {
	case EPM_MARKER1_REQUEST:
		{
			uint32_t result;
			result = epm_request_marker1();

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	case EPM_MARKER2_REQUEST:
		{
			uint32_t result;
			result = epm_request_marker2();

			if (copy_to_user((void __user *)arg, &result,
						sizeof(uint32_t)))
				return -EFAULT;
			break;
		}
	case EPM_MARKER1_SET_LEVEL:
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
	case EPM_MARKER2_SET_LEVEL:
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
			int rc;

			rc = epm_adc_psoc_gpio_init(epm_adc, true);
			if (rc) {
				pr_err("GPIO init failed with %d\n", rc);
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg, &rc,
						sizeof(int)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_ADC_DEINIT:
		{
			int rc;
			rc = epm_adc_psoc_gpio_init(epm_adc, false);

			if (copy_to_user((void __user *)arg, &rc,
						sizeof(int)))
				return -EFAULT;
			break;
		}
	case EPM_PSOC_GENERIC_REQUEST:
		{
			struct epm_generic_request psoc_get_data;
			int rc;

			if (copy_from_user(&psoc_get_data,
				(void __user *)arg,
				sizeof(struct
				epm_generic_request)))
				return -EFAULT;

			rc = epm_psoc_generic_request(epm_adc, &psoc_get_data);
			if (rc) {
				pr_err("Generic request failed\n");
				return -EINVAL;
			}

			if (copy_to_user((void __user *)arg, &psoc_get_data,
				sizeof(struct
				epm_generic_request)))
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

static int get_device_tree_data(struct spi_device *spi)
{
	const struct device_node *node = spi->dev.of_node;
	struct epm_adc_drv *epm_adc;
	u32 *epm_ch_gain, *epm_ch_rsense;
	u32 rc = 0, epm_num_channels, i, channel_mask, epm_gpio_num;

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

	epm_gpio_num = of_get_named_gpio(spi->dev.of_node,
						"qcom,epm-enable-gpio", 0);
	if (epm_gpio_num < 0) {
		dev_err(&spi->dev, "missing global en gpio num\n");
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
	epm_adc->epm_global_en_gpio = epm_gpio_num;
	epm_adc_drv = epm_adc;

	return 0;
}

static int epm_adc_psoc_spi_probe(struct spi_device *spi)
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
				EPM_ADC_SPI_BITS_PER_WORD;
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
				EPM_ADC_SPI_BITS_PER_WORD;

	epm_adc->hwmon = hwmon_device_register(&spi->dev);
	if (IS_ERR(epm_adc->hwmon)) {
		dev_err(&spi->dev, "hwmon_device_register failed\n");
		return rc;
	}

	mutex_init(&epm_adc->conv_lock);
	return rc;
}

static int epm_adc_psoc_spi_remove(struct spi_device *spi)
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
	.remove = epm_adc_psoc_spi_remove,
	.driver = {
		.name = EPM_ADC_DRIVER_NAME,
		.of_match_table = epm_adc_psoc_match_table,
	},
};

static int __init epm_adc_init(void)
{
	int ret = 0;

	ret = spi_register_driver(&epm_spi_driver);
	if (ret)
		pr_err("%s: spi register failed: rc=%d\n", __func__, ret);

	return ret;
}

static void __exit epm_adc_exit(void)
{
	spi_unregister_driver(&epm_spi_driver);
}

module_init(epm_adc_init);
module_exit(epm_adc_exit);

MODULE_DESCRIPTION("EPM ADC Driver");
MODULE_ALIAS("platform:epm_adc");
MODULE_LICENSE("GPL v2");
