/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/thermal.h>

#include "../thermal_core.h"

#define BCL_DRIVER_NAME       "bcl_pmic5"
#define BCL_MONITOR_EN        0x46
#define BCL_IRQ_STATUS        0x08

#define BCL_IBAT_HIGH         0x4B
#define BCL_IBAT_TOO_HIGH     0x4C
#define BCL_IBAT_READ         0x86
#define BCL_IBAT_SCALING_UA   78127

#define BCL_VBAT_READ         0x76
#define BCL_VBAT_ADC_LOW      0x48
#define BCL_VBAT_COMP_LOW     0x49
#define BCL_VBAT_COMP_TLOW    0x4A

#define BCL_IRQ_L0       0x1
#define BCL_IRQ_L1       0x2
#define BCL_IRQ_L2       0x4

#define BCL_VBAT_SCALING_UV   49827
#define BCL_VBAT_NO_READING   127
#define BCL_VBAT_BASE_MV      2000
#define BCL_VBAT_INC_MV       25
#define BCL_VBAT_MAX_MV       3600

#define MAX_PERPH_COUNT       2

enum bcl_dev_type {
	BCL_IBAT_LVL0,
	BCL_IBAT_LVL1,
	BCL_VBAT_LVL0,
	BCL_VBAT_LVL1,
	BCL_VBAT_LVL2,
	BCL_LVL0,
	BCL_LVL1,
	BCL_LVL2,
	BCL_TYPE_MAX,
};

static char bcl_int_names[BCL_TYPE_MAX][25] = {
	"bcl-ibat-lvl0",
	"bcl-ibat-lvl1",
	"bcl-vbat-lvl0",
	"bcl-vbat-lvl1",
	"bcl-vbat-lvl2",
	"bcl-lvl0",
	"bcl-lvl1",
	"bcl-lvl2",
};

struct bcl_device;

struct bcl_peripheral_data {
	int                     irq_num;
	int                     status_bit_idx;
	long int		trip_thresh;
	int                     last_val;
	struct mutex            state_trans_lock;
	bool			irq_enabled;
	enum bcl_dev_type	type;
	struct thermal_zone_of_device_ops ops;
	struct thermal_zone_device *tz_dev;
	struct bcl_device	*dev;
};

struct bcl_device {
	struct regmap			*regmap;
	uint16_t			fg_bcl_addr;
	struct bcl_peripheral_data	param[BCL_TYPE_MAX];
};

static struct bcl_device *bcl_devices[MAX_PERPH_COUNT];
static int bcl_device_ct;
static bool ibat_use_qg_adc;

static int bcl_read_register(struct bcl_device *bcl_perph, int16_t reg_offset,
				unsigned int *data)
{
	int ret = 0;

	if (!bcl_perph) {
		pr_err("BCL device not initialized\n");
		return -EINVAL;
	}
	ret = regmap_read(bcl_perph->regmap,
			       (bcl_perph->fg_bcl_addr + reg_offset),
			       data);
	if (ret < 0)
		pr_err("Error reading register 0x%04x err:%d\n",
				bcl_perph->fg_bcl_addr + reg_offset, ret);
	else
		pr_debug("Read register:0x%04x value:0x%02x\n",
				bcl_perph->fg_bcl_addr + reg_offset,
				*data);

	return ret;
}

static int bcl_write_register(struct bcl_device *bcl_perph,
				int16_t reg_offset, uint8_t data)
{
	int  ret = 0;
	uint8_t *write_buf = &data;
	uint16_t base;

	if (!bcl_perph) {
		pr_err("BCL device not initialized\n");
		return -EINVAL;
	}
	base = bcl_perph->fg_bcl_addr;
	ret = regmap_write(bcl_perph->regmap, (base + reg_offset), *write_buf);
	if (ret < 0) {
		pr_err("Error reading register:0x%04x val:0x%02x err:%d\n",
				base + reg_offset, data, ret);
		return ret;
	}
	pr_debug("wrote 0x%02x to 0x%04x\n", data, base + reg_offset);

	return ret;
}

static void convert_adc_to_vbat_thresh_val(int *val)
{
	/*
	 * Threshold register is bit shifted from ADC MSB.
	 * So the scaling factor is half.
	 */
	*val = (*val * BCL_VBAT_SCALING_UV) / 2000;
}

static void convert_adc_to_vbat_val(int *val)
{
	*val = (*val * BCL_VBAT_SCALING_UV) / 1000;
}

static void convert_ibat_to_adc_val(int *val)
{
	/*
	 * Threshold register is bit shifted from ADC MSB.
	 * So the scaling factor is half.
	 */
	if (ibat_use_qg_adc)
		*val = (int)div_s64(*val * 2000 * 2, BCL_IBAT_SCALING_UA);
	else
		*val = (int)div_s64(*val * 2000, BCL_IBAT_SCALING_UA);

}

static void convert_adc_to_ibat_val(int *val)
{
	/* Scaling factor will be half if ibat_use_qg_adc is true */
	if (ibat_use_qg_adc)
		*val = (int)div_s64(*val * BCL_IBAT_SCALING_UA, 2 * 1000);
	else
		*val = (int)div_s64(*val * BCL_IBAT_SCALING_UA, 1000);
}

static int bcl_set_ibat(void *data, int low, int high)
{
	int ret = 0, ibat_ua, thresh_value;
	int8_t val = 0;
	int16_t addr;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)data;

	mutex_lock(&bat_data->state_trans_lock);
	thresh_value = high;
	if (bat_data->trip_thresh == thresh_value)
		goto set_trip_exit;

	if (bat_data->irq_num && bat_data->irq_enabled) {
		disable_irq_nosync(bat_data->irq_num);
		bat_data->irq_enabled = false;
	}
	if (thresh_value == INT_MAX) {
		bat_data->trip_thresh = thresh_value;
		goto set_trip_exit;
	}

	ibat_ua = thresh_value;
	convert_ibat_to_adc_val(&thresh_value);
	val = (int8_t)thresh_value;
	switch (bat_data->type) {
	case BCL_IBAT_LVL0:
		addr = BCL_IBAT_HIGH;
		pr_debug("ibat high threshold:%d mA ADC:0x%02x\n",
				ibat_ua, val);
		break;
	case BCL_IBAT_LVL1:
		addr = BCL_IBAT_TOO_HIGH;
		pr_debug("ibat too high threshold:%d mA ADC:0x%02x\n",
				ibat_ua, val);
		break;
	default:
		goto set_trip_exit;
	}
	ret = bcl_write_register(bat_data->dev, addr, val);
	if (ret)
		goto set_trip_exit;
	bat_data->trip_thresh = ibat_ua;

	if (bat_data->irq_num && !bat_data->irq_enabled) {
		enable_irq(bat_data->irq_num);
		bat_data->irq_enabled = true;
	}

set_trip_exit:
	mutex_unlock(&bat_data->state_trans_lock);

	return ret;
}

static int bcl_read_ibat(void *data, int *adc_value)
{
	int ret = 0;
	unsigned int val = 0;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)data;

	*adc_value = val;
	ret = bcl_read_register(bat_data->dev, BCL_IBAT_READ, &val);
	if (ret)
		goto bcl_read_exit;
	/* IBat ADC reading is in 2's compliment form */
	*adc_value = sign_extend32(val, 7);
	if (val == 0) {
		/*
		 * The sensor sometime can read a value 0 if there is
		 * consequtive reads
		 */
		*adc_value = bat_data->last_val;
	} else {
		convert_adc_to_ibat_val(adc_value);
		bat_data->last_val = *adc_value;
	}
	pr_debug("ibat:%d mA ADC:0x%02x\n", bat_data->last_val, val);

bcl_read_exit:
	return ret;
}

static int bcl_get_vbat_trip(void *data, int type, int *trip)
{
	int ret = 0;
	unsigned int val = 0;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)data;
	int16_t addr;

	*trip = 0;
	switch (bat_data->type) {
	case BCL_VBAT_LVL0:
		addr = BCL_VBAT_ADC_LOW;
		break;
	case BCL_VBAT_LVL1:
		addr = BCL_VBAT_COMP_LOW;
		break;
	case BCL_VBAT_LVL2:
		addr = BCL_VBAT_COMP_TLOW;
		break;
	default:
		return -ENODEV;
	}

	ret = bcl_read_register(bat_data->dev, addr, &val);
	if (ret)
		return ret;

	if (addr == BCL_VBAT_ADC_LOW) {
		*trip = val;
		convert_adc_to_vbat_thresh_val(trip);
		pr_debug("vbat trip: %d mV ADC:0x%02x\n", *trip, val);
	} else {
		*trip = 2250 + val * 25;
		if (*trip > BCL_VBAT_MAX_MV)
			*trip = BCL_VBAT_MAX_MV;
		pr_debug("vbat-%s-low trip: %d mV ADC:0x%02x\n",
				(addr == BCL_VBAT_COMP_LOW) ?
				"too" : "critical",
				*trip, val);
	}

	return 0;
}

static int bcl_set_vbat(void *data, int low, int high)
{
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)data;

	mutex_lock(&bat_data->state_trans_lock);

	if (low == INT_MIN &&
		bat_data->irq_num && bat_data->irq_enabled) {
		disable_irq_nosync(bat_data->irq_num);
		bat_data->irq_enabled = false;
		pr_debug("vbat: disable irq:%d\n", bat_data->irq_num);
	} else if (low != INT_MIN &&
		 bat_data->irq_num && !bat_data->irq_enabled) {
		enable_irq(bat_data->irq_num);
		bat_data->irq_enabled = true;
		pr_debug("vbat: enable irq:%d\n", bat_data->irq_num);
	}

	/*
	 * Vbat threshold's are pre-configured and cant be
	 * programmed.
	 */
	mutex_unlock(&bat_data->state_trans_lock);

	return 0;
}

static int bcl_read_vbat(void *data, int *adc_value)
{
	int ret = 0;
	unsigned int val = 0;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)data;

	*adc_value = val;
	ret = bcl_read_register(bat_data->dev, BCL_VBAT_READ, &val);
	if (ret)
		goto bcl_read_exit;
	*adc_value = val;
	if (*adc_value == BCL_VBAT_NO_READING) {
		*adc_value = bat_data->last_val;
	} else {
		convert_adc_to_vbat_val(adc_value);
		bat_data->last_val = *adc_value;
	}
	pr_debug("vbat:%d mv\n", bat_data->last_val);

bcl_read_exit:
	return ret;
}

static int bcl_set_lbat(void *data, int low, int high)
{
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)data;

	mutex_lock(&bat_data->state_trans_lock);

	if (high == INT_MAX &&
		bat_data->irq_num && bat_data->irq_enabled) {
		disable_irq_nosync(bat_data->irq_num);
		bat_data->irq_enabled = false;
		pr_debug("lbat[%d]: disable irq:%d\n",
				bat_data->type,
				bat_data->irq_num);
	} else if (high != INT_MAX &&
		bat_data->irq_num && !bat_data->irq_enabled) {
		enable_irq(bat_data->irq_num);
		bat_data->irq_enabled = true;
		pr_debug("lbat[%d]: enable irq:%d\n",
				bat_data->type,
				bat_data->irq_num);
	}

	mutex_unlock(&bat_data->state_trans_lock);

	return 0;
}

static int bcl_read_lbat(void *data, int *adc_value)
{
	int ret = 0;
	unsigned int val = 0;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)data;
	struct bcl_device *bcl_perph = bat_data->dev;

	*adc_value = val;
	ret = bcl_read_register(bcl_perph, BCL_IRQ_STATUS, &val);
	if (ret)
		goto bcl_read_exit;
	switch (bat_data->type) {
	case BCL_LVL0:
		*adc_value = val & BCL_IRQ_L0;
		break;
	case BCL_LVL1:
		*adc_value = val & BCL_IRQ_L1;
		break;
	case BCL_LVL2:
		*adc_value = val & BCL_IRQ_L2;
		break;
	default:
		pr_err("Invalid sensor type:%d\n", bat_data->type);
		ret = -ENODEV;
		goto bcl_read_exit;
	}
	bat_data->last_val = *adc_value;
	pr_debug("lbat:%d val:%d\n", bat_data->type,
			bat_data->last_val);

bcl_read_exit:
	return ret;
}

static irqreturn_t bcl_handle_irq(int irq, void *data)
{
	struct bcl_peripheral_data *perph_data =
		(struct bcl_peripheral_data *)data;
	unsigned int irq_status = 0;
	struct bcl_device *bcl_perph;

	bcl_perph = perph_data->dev;
	bcl_read_register(bcl_perph, BCL_IRQ_STATUS, &irq_status);

	if (irq_status & perph_data->status_bit_idx) {
		pr_debug("Irq:%d triggered for bcl type:%s. status:%u\n",
			irq, bcl_int_names[perph_data->type],
			irq_status);
		of_thermal_handle_trip_temp(perph_data->tz_dev,
				perph_data->status_bit_idx);
	}

	return IRQ_HANDLED;
}

static int bcl_get_devicetree_data(struct platform_device *pdev,
					struct bcl_device *bcl_perph)
{
	int ret = 0;
	const __be32 *prop = NULL;
	struct device_node *dev_node = pdev->dev.of_node;

	prop = of_get_address(dev_node, 0, NULL, NULL);
	if (prop) {
		bcl_perph->fg_bcl_addr = be32_to_cpu(*prop);
		pr_debug("fg_bcl@%04x\n", bcl_perph->fg_bcl_addr);
	} else {
		dev_err(&pdev->dev, "No fg_bcl registers found\n");
		return -ENODEV;
	}

	ibat_use_qg_adc =  of_property_read_bool(dev_node,
				"qcom,ibat-use-qg-adc-5a");

	return ret;
}

static void bcl_fetch_trip(struct platform_device *pdev, enum bcl_dev_type type,
		struct bcl_peripheral_data *data,
		irqreturn_t (*handle)(int, void *))
{
	int ret = 0, irq_num = 0;
	char *int_name = bcl_int_names[type];

	mutex_lock(&data->state_trans_lock);
	data->irq_num = 0;
	data->irq_enabled = false;
	irq_num = platform_get_irq_byname(pdev, int_name);
	if (irq_num > 0 && handle) {
		ret = devm_request_threaded_irq(&pdev->dev,
				irq_num, NULL, handle,
				IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				int_name, data);
		if (ret) {
			dev_err(&pdev->dev,
				"Error requesting trip irq. err:%d\n",
				ret);
			mutex_unlock(&data->state_trans_lock);
			return;
		}
		disable_irq_nosync(irq_num);
		data->irq_num = irq_num;
	} else if (irq_num > 0 && !handle) {
		disable_irq_nosync(irq_num);
		data->irq_num = irq_num;
	}
	mutex_unlock(&data->state_trans_lock);
}

static void bcl_vbat_init(struct platform_device *pdev,
		enum bcl_dev_type type, struct bcl_device *bcl_perph)
{
	struct bcl_peripheral_data *vbat = &bcl_perph->param[type];

	mutex_init(&vbat->state_trans_lock);
	vbat->type = type;
	vbat->dev = bcl_perph;
	vbat->ops.get_temp = bcl_read_vbat;
	vbat->ops.set_trips = bcl_set_vbat;
	vbat->ops.get_trip_temp = bcl_get_vbat_trip;
	vbat->irq_num = 0;
	vbat->irq_enabled = false;
	vbat->tz_dev = thermal_zone_of_sensor_register(&pdev->dev,
				type, vbat, &vbat->ops);
	if (IS_ERR(vbat->tz_dev)) {
		pr_debug("vbat[%s] register failed. err:%ld\n",
				bcl_int_names[type],
				PTR_ERR(vbat->tz_dev));
		vbat->tz_dev = NULL;
		return;
	}
	thermal_zone_device_update(vbat->tz_dev, THERMAL_DEVICE_UP);
}

static void bcl_probe_vbat(struct platform_device *pdev,
					struct bcl_device *bcl_perph)
{
	bcl_vbat_init(pdev, BCL_VBAT_LVL0, bcl_perph);
	bcl_vbat_init(pdev, BCL_VBAT_LVL1, bcl_perph);
	bcl_vbat_init(pdev, BCL_VBAT_LVL2, bcl_perph);
}

static void bcl_ibat_init(struct platform_device *pdev,
			enum bcl_dev_type type, struct bcl_device *bcl_perph)
{
	struct bcl_peripheral_data *ibat = &bcl_perph->param[type];

	mutex_init(&ibat->state_trans_lock);
	ibat->type = type;
	ibat->dev = bcl_perph;
	ibat->irq_num = 0;
	ibat->irq_enabled = false;
	ibat->ops.get_temp = bcl_read_ibat;
	ibat->ops.set_trips = bcl_set_ibat;
	ibat->tz_dev = thermal_zone_of_sensor_register(&pdev->dev,
				type, ibat, &ibat->ops);
	if (IS_ERR(ibat->tz_dev)) {
		pr_debug("ibat:[%s] register failed. err:%ld\n",
				bcl_int_names[type],
				PTR_ERR(ibat->tz_dev));
		ibat->tz_dev = NULL;
		return;
	}
	thermal_zone_device_update(ibat->tz_dev, THERMAL_DEVICE_UP);
}

static void bcl_probe_ibat(struct platform_device *pdev,
					struct bcl_device *bcl_perph)
{
	bcl_ibat_init(pdev, BCL_IBAT_LVL0, bcl_perph);
	bcl_ibat_init(pdev, BCL_IBAT_LVL1, bcl_perph);
}

static void bcl_lvl_init(struct platform_device *pdev,
	enum bcl_dev_type type, int sts_bit_idx, struct bcl_device *bcl_perph)
{
	struct bcl_peripheral_data *lbat = &bcl_perph->param[type];

	mutex_init(&lbat->state_trans_lock);
	lbat->type = type;
	lbat->dev = bcl_perph;
	lbat->status_bit_idx = sts_bit_idx;
	bcl_fetch_trip(pdev, type, lbat, bcl_handle_irq);
	if (lbat->irq_num <= 0)
		return;

	lbat->ops.get_temp = bcl_read_lbat;
	lbat->ops.set_trips = bcl_set_lbat;

	lbat->tz_dev = thermal_zone_of_sensor_register(&pdev->dev,
				type, lbat, &lbat->ops);
	if (IS_ERR(lbat->tz_dev)) {
		pr_debug("lbat:[%s] register failed. err:%ld\n",
				bcl_int_names[type],
				PTR_ERR(lbat->tz_dev));
		lbat->tz_dev = NULL;
		return;
	}
	thermal_zone_device_update(lbat->tz_dev, THERMAL_DEVICE_UP);
}

static void bcl_probe_lvls(struct platform_device *pdev,
					struct bcl_device *bcl_perph)
{
	bcl_lvl_init(pdev, BCL_LVL0, BCL_IRQ_L0, bcl_perph);
	bcl_lvl_init(pdev, BCL_LVL1, BCL_IRQ_L1, bcl_perph);
	bcl_lvl_init(pdev, BCL_LVL2, BCL_IRQ_L2, bcl_perph);
}

static void bcl_configure_bcl_peripheral(struct bcl_device *bcl_perph)
{
	bcl_write_register(bcl_perph, BCL_MONITOR_EN, BIT(7));
}

static int bcl_remove(struct platform_device *pdev)
{
	int i = 0;
	struct bcl_device *bcl_perph =
		(struct bcl_device *)dev_get_drvdata(&pdev->dev);

	for (; i < BCL_TYPE_MAX; i++) {
		if (!bcl_perph->param[i].tz_dev)
			continue;
		thermal_zone_of_sensor_unregister(&pdev->dev,
				bcl_perph->param[i].tz_dev);
	}

	return 0;
}

static int bcl_probe(struct platform_device *pdev)
{
	struct bcl_device *bcl_perph = NULL;

	if (bcl_device_ct >= MAX_PERPH_COUNT) {
		dev_err(&pdev->dev, "Max bcl peripheral supported already.\n");
		return -EINVAL;
	}
	bcl_devices[bcl_device_ct] = devm_kzalloc(&pdev->dev,
					sizeof(*bcl_devices[0]), GFP_KERNEL);
	if (!bcl_devices[bcl_device_ct])
		return -ENOMEM;
	bcl_perph = bcl_devices[bcl_device_ct];

	bcl_perph->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!bcl_perph->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	bcl_device_ct++;
	bcl_get_devicetree_data(pdev, bcl_perph);
	bcl_probe_vbat(pdev, bcl_perph);
	bcl_probe_ibat(pdev, bcl_perph);
	bcl_probe_lvls(pdev, bcl_perph);
	bcl_configure_bcl_peripheral(bcl_perph);

	dev_set_drvdata(&pdev->dev, bcl_perph);

	return 0;
}

static const struct of_device_id bcl_match[] = {
	{
		.compatible = "qcom,bcl-v5",
	},
	{},
};

static struct platform_driver bcl_driver = {
	.probe  = bcl_probe,
	.remove = bcl_remove,
	.driver = {
		.name           = BCL_DRIVER_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = bcl_match,
	},
};

builtin_platform_driver(bcl_driver);
