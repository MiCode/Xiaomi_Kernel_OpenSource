// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
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
#include <linux/power_supply.h>
#include <linux/thermal.h>

#include "../thermal_core.h"

#define BCL_DRIVER_NAME       "bcl_peripheral"
#define BCL_VBAT_INT          "bcl-low-vbat"
#define BCL_VLOW_VBAT_INT     "bcl-very-low-vbat"
#define BCL_CLOW_VBAT_INT     "bcl-crit-low-vbat"
#define BCL_IBAT_INT          "bcl-high-ibat"
#define BCL_VHIGH_IBAT_INT    "bcl-very-high-ibat"
#define BCL_MONITOR_EN        0x46
#define BCL_VBAT_MIN          0x5C
#define BCL_IBAT_MAX          0x5D
#define BCL_MAX_MIN_CLR       0x48
#define BCL_IBAT_MAX_CLR      3
#define BCL_VBAT_MIN_CLR      2
#define BCL_VBAT_ADC_LOW      0x72
#define BCL_VBAT_COMP_LOW     0x75
#define BCL_VBAT_COMP_TLOW    0x76
#define BCL_IBAT_HIGH         0x78
#define BCL_IBAT_TOO_HIGH     0x79
#define BCL_LMH_CFG           0xA3
#define BCL_CFG               0x6A
#define LMH_INT_POL_HIGH      0x12
#define LMH_INT_EN            0x15
#define BCL_VBAT_SCALING      39000
#define BCL_IBAT_SCALING      80
#define BCL_LMH_CFG_VAL       0x3
#define BCL_CFG_VAL           0x81
#define LMH_INT_VAL           0x7
#define BCL_READ_RETRY_LIMIT  3
#define VAL_CP_REG_BUF_LEN    3
#define VAL_REG_BUF_OFFSET    0
#define VAL_CP_REG_BUF_OFFSET 2
#define BCL_STD_VBAT_NR       9
#define BCL_VBAT_NO_READING   127

enum bcl_dev_type {
	BCL_HIGH_IBAT,
	BCL_VHIGH_IBAT,
	BCL_LOW_VBAT,
	BCL_VLOW_VBAT,
	BCL_CLOW_VBAT,
	BCL_SOC_MONITOR,
	BCL_TYPE_MAX,
};

struct bcl_peripheral_data {
	int                     irq_num;
	long                    trip_temp;
	int                     trip_val;
	int                     last_val;
	struct mutex            state_trans_lock;
	bool			irq_enabled;
	struct thermal_zone_of_device_ops ops;
	struct thermal_zone_device *tz_dev;
};

struct bcl_device {
	struct regmap			*regmap;
	uint16_t			fg_bcl_addr;
	uint16_t			fg_lmh_addr;
	struct notifier_block		psy_nb;
	struct work_struct		soc_eval_work;
	struct bcl_peripheral_data	param[BCL_TYPE_MAX];
};

static struct bcl_device *bcl_perph;
static int vbat_low[BCL_STD_VBAT_NR] = {
		2400, 2500, 2600, 2700, 2800, 2900,
		3000, 3100, 3200};

static int bcl_read_multi_register(int16_t reg_offset, uint8_t *data, int len)
{
	int  ret = 0;

	if (!bcl_perph) {
		pr_err("BCL device not initialized\n");
		return -EINVAL;
	}
	ret = regmap_bulk_read(bcl_perph->regmap,
			       (bcl_perph->fg_bcl_addr + reg_offset),
			       data, len);
	if (ret < 0) {
		pr_err("Error reading register %d. err:%d\n", reg_offset, ret);
		return ret;
	}

	return ret;
}

static int bcl_write_general_register(int16_t reg_offset,
					uint16_t base, uint8_t data)
{
	int  ret = 0;
	uint8_t *write_buf = &data;

	if (!bcl_perph) {
		pr_err("BCL device not initialized\n");
		return -EINVAL;
	}
	ret = regmap_write(bcl_perph->regmap, (base + reg_offset), *write_buf);
	if (ret < 0) {
		pr_err("Error reading register %d. err:%d\n", reg_offset, ret);
		return ret;
	}
	pr_debug("wrote 0x%02x to 0x%04x\n", data, base + reg_offset);

	return ret;
}

static int bcl_write_register(int16_t reg_offset, uint8_t data)
{
	return bcl_write_general_register(reg_offset,
			bcl_perph->fg_bcl_addr, data);
}

static void convert_vbat_to_adc_val(int *val)
{
	*val = (*val * 1000) / BCL_VBAT_SCALING;
}

static void convert_adc_to_vbat_val(int *val)
{
	*val = *val * BCL_VBAT_SCALING / 1000;
}

static void convert_ibat_to_adc_val(int *val)
{
	*val = *val / BCL_IBAT_SCALING;
}

static void convert_adc_to_ibat_val(int *val)
{
	*val = *val * BCL_IBAT_SCALING;
}

static int bcl_set_ibat(void *data, int low, int high)
{
	int ret = 0, ibat_ua, thresh_value;
	int8_t val = 0;
	int16_t addr;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)data;

	thresh_value = high;
	if (bat_data->trip_temp == thresh_value)
		return 0;

	mutex_lock(&bat_data->state_trans_lock);
	if (bat_data->irq_num && bat_data->irq_enabled) {
		disable_irq_nosync(bat_data->irq_num);
		bat_data->irq_enabled = false;
	}
	if (thresh_value == INT_MAX) {
		bat_data->trip_temp = thresh_value;
		goto set_trip_exit;
	}

	ibat_ua = thresh_value;
	convert_ibat_to_adc_val(&thresh_value);
	val = (int8_t)thresh_value;
	if (&bcl_perph->param[BCL_HIGH_IBAT] == bat_data) {
		addr = BCL_IBAT_HIGH;
		pr_debug("ibat high threshold:%d mA ADC:0x%02x\n",
				ibat_ua, val);
	} else if (&bcl_perph->param[BCL_VHIGH_IBAT] == bat_data) {
		addr = BCL_IBAT_TOO_HIGH;
		pr_debug("ibat too high threshold:%d mA ADC:0x%02x\n",
				ibat_ua, val);
	} else {
		goto set_trip_exit;
	}
	ret = bcl_write_register(addr, val);
	if (ret) {
		pr_err("Error accessing BCL peripheral. err:%d\n", ret);
		goto set_trip_exit;
	}
	bat_data->trip_temp = ibat_ua;

	if (bat_data->irq_num && !bat_data->irq_enabled) {
		enable_irq(bat_data->irq_num);
		bat_data->irq_enabled = true;
	}

set_trip_exit:
	mutex_unlock(&bat_data->state_trans_lock);

	return ret;
}

static int bcl_set_vbat(void *data, int low, int high)
{
	int ret = 0, vbat_uv, vbat_idx, thresh_value;
	int8_t val = 0;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)data;
	uint16_t addr;

	thresh_value = low;
	if (bat_data->trip_temp == thresh_value)
		return 0;

	mutex_lock(&bat_data->state_trans_lock);

	if (bat_data->irq_num && bat_data->irq_enabled) {
		disable_irq_nosync(bat_data->irq_num);
		bat_data->irq_enabled = false;
	}
	if (thresh_value == INT_MIN) {
		bat_data->trip_temp = thresh_value;
		goto set_trip_exit;
	}
	vbat_uv = thresh_value;
	convert_vbat_to_adc_val(&thresh_value);
	val = (int8_t)thresh_value;
	/*
	 * very low and critical low trip can support only standard
	 * trip thresholds
	 */
	if (&bcl_perph->param[BCL_LOW_VBAT] == bat_data) {
		addr = BCL_VBAT_ADC_LOW;
		pr_debug("vbat low threshold:%d mv ADC:0x%02x\n",
				vbat_uv, val);
	} else if (&bcl_perph->param[BCL_VLOW_VBAT] == bat_data) {
		/*
		 * Scan the standard voltage table, sorted in ascending order
		 * and find the closest threshold that is lower or equal to
		 * the requested value. Passive trip supports thresholds
		 * indexed from 1...BCL_STD_VBAT_NR in the voltage table.
		 */
		for (vbat_idx = 2; vbat_idx < BCL_STD_VBAT_NR;
			vbat_idx++) {
			if (vbat_uv >= vbat_low[vbat_idx])
				continue;
			break;
		}
		addr = BCL_VBAT_COMP_LOW;
		val = vbat_idx - 2;
		vbat_uv = vbat_low[vbat_idx - 1];
		pr_debug("vbat too low threshold:%d mv ADC:0x%02x\n",
				vbat_uv, val);
	} else if (&bcl_perph->param[BCL_CLOW_VBAT] == bat_data) {
		/* Hot trip supports thresholds indexed from
		 * 0...BCL_STD_VBAT_NR-1 in the voltage table.
		 */
		for (vbat_idx = 1; vbat_idx < (BCL_STD_VBAT_NR - 1);
			vbat_idx++) {
			if (vbat_uv >= vbat_low[vbat_idx])
				continue;
			break;
		}
		addr = BCL_VBAT_COMP_TLOW;
		val = vbat_idx - 1;
		vbat_uv = vbat_low[vbat_idx - 1];
		pr_debug("vbat critic low threshold:%d mv ADC:0x%02x\n",
				vbat_uv, val);
	} else {
		goto set_trip_exit;
	}

	ret = bcl_write_register(addr, val);
	if (ret) {
		pr_err("Error accessing BCL peripheral. err:%d\n", ret);
		goto set_trip_exit;
	}
	bat_data->trip_temp = vbat_uv;
	if (bat_data->irq_num && !bat_data->irq_enabled) {
		enable_irq(bat_data->irq_num);
		bat_data->irq_enabled = true;
	}

set_trip_exit:
	mutex_unlock(&bat_data->state_trans_lock);
	return ret;
}

static int bcl_clear_vbat_min(void)
{
	int ret  = 0;

	ret = bcl_write_register(BCL_MAX_MIN_CLR,
			BIT(BCL_VBAT_MIN_CLR));
	if (ret)
		pr_err("Error in clearing vbat min reg. err:%d\n", ret);

	return ret;
}

static int bcl_clear_ibat_max(void)
{
	int ret  = 0;

	ret = bcl_write_register(BCL_MAX_MIN_CLR,
			BIT(BCL_IBAT_MAX_CLR));
	if (ret)
		pr_err("Error in clearing ibat max reg. err:%d\n", ret);

	return ret;
}

static int bcl_read_ibat(void *data, int *adc_value)
{
	int ret = 0, timeout = 0;
	int8_t val[VAL_CP_REG_BUF_LEN] = {0};
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)data;

	*adc_value = (int)val[VAL_REG_BUF_OFFSET];
	do {
		ret = bcl_read_multi_register(BCL_IBAT_MAX, val,
					VAL_CP_REG_BUF_LEN);
		if (ret) {
			pr_err("BCL register read error. err:%d\n", ret);
			goto bcl_read_exit;
		}
	} while (val[VAL_REG_BUF_OFFSET] != val[VAL_CP_REG_BUF_OFFSET]
		&& timeout++ < BCL_READ_RETRY_LIMIT);
	if (val[VAL_REG_BUF_OFFSET] != val[VAL_CP_REG_BUF_OFFSET]) {
		ret = -ENODEV;
		*adc_value = bat_data->last_val;
		goto bcl_read_exit;
	}
	*adc_value = (int)val[VAL_REG_BUF_OFFSET];
	if (*adc_value == 0) {
		/*
		 * The sensor sometime can read a value 0 if there is
		 * consequtive reads
		 */
		*adc_value = bat_data->last_val;
	} else {
		convert_adc_to_ibat_val(adc_value);
		bat_data->last_val = *adc_value;
	}
	pr_debug("ibat:%d mA\n", bat_data->last_val);

bcl_read_exit:
	return ret;
}

static int bcl_read_ibat_and_clear(void *data, int *adc_value)
{
	int ret = 0;

	ret = bcl_read_ibat(data, adc_value);
	if (ret)
		return ret;
	return bcl_clear_ibat_max();
}

static int bcl_read_vbat(void *data, int *adc_value)
{
	int ret = 0, timeout = 0;
	int8_t val[VAL_CP_REG_BUF_LEN] = {0};
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)data;

	*adc_value = (int)val[VAL_REG_BUF_OFFSET];
	do {
		ret = bcl_read_multi_register(BCL_VBAT_MIN, val,
			VAL_CP_REG_BUF_LEN);
		if (ret) {
			pr_err("BCL register read error. err:%d\n", ret);
			goto bcl_read_exit;
		}
	} while (val[VAL_REG_BUF_OFFSET] != val[VAL_CP_REG_BUF_OFFSET]
		&& timeout++ < BCL_READ_RETRY_LIMIT);
	if (val[VAL_REG_BUF_OFFSET] != val[VAL_CP_REG_BUF_OFFSET]) {
		ret = -ENODEV;
		goto bcl_read_exit;
	}
	*adc_value = (int)val[VAL_REG_BUF_OFFSET];
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

static int bcl_read_vbat_and_clear(void *data, int *adc_value)
{
	int ret;

	ret = bcl_read_vbat(data, adc_value);
	if (ret)
		return ret;
	return bcl_clear_vbat_min();
}

static irqreturn_t bcl_handle_ibat(int irq, void *data)
{
	struct bcl_peripheral_data *perph_data =
		(struct bcl_peripheral_data *)data;

	mutex_lock(&perph_data->state_trans_lock);
	if (!perph_data->irq_enabled) {
		WARN_ON(1);
		disable_irq_nosync(irq);
		perph_data->irq_enabled = false;
		goto exit_intr;
	}
	mutex_unlock(&perph_data->state_trans_lock);
	of_thermal_handle_trip(perph_data->tz_dev);

	return IRQ_HANDLED;

exit_intr:
	mutex_unlock(&perph_data->state_trans_lock);
	return IRQ_HANDLED;
}

static irqreturn_t bcl_handle_vbat(int irq, void *data)
{
	struct bcl_peripheral_data *perph_data =
		(struct bcl_peripheral_data *)data;

	mutex_lock(&perph_data->state_trans_lock);
	if (!perph_data->irq_enabled) {
		WARN_ON(1);
		disable_irq_nosync(irq);
		perph_data->irq_enabled = false;
		goto exit_intr;
	}
	mutex_unlock(&perph_data->state_trans_lock);
	of_thermal_handle_trip(perph_data->tz_dev);

	return IRQ_HANDLED;

exit_intr:
	mutex_unlock(&perph_data->state_trans_lock);
	return IRQ_HANDLED;
}

static int bcl_get_devicetree_data(struct platform_device *pdev)
{
	int ret = 0;
	const __be32 *prop = NULL;
	struct device_node *dev_node = pdev->dev.of_node;

	prop = of_get_address(dev_node, 0, NULL, NULL);
	if (prop) {
		bcl_perph->fg_bcl_addr = be32_to_cpu(*prop);
		pr_debug("fg_user_adc@%04x\n", bcl_perph->fg_bcl_addr);
	} else {
		dev_err(&pdev->dev, "No fg_user_adc registers found\n");
		return -ENODEV;
	}

	prop = of_get_address(dev_node, 1, NULL, NULL);
	if (prop) {
		bcl_perph->fg_lmh_addr = be32_to_cpu(*prop);
		pr_debug("fg_lmh@%04x\n", bcl_perph->fg_lmh_addr);
	} else {
		dev_err(&pdev->dev, "No fg_lmh registers found\n");
		return -ENODEV;
	}

	return ret;
}

static int bcl_set_soc(void *data, int low, int high)
{
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)data;

	if (low == bat_data->trip_temp)
		return 0;

	mutex_lock(&bat_data->state_trans_lock);
	pr_debug("low soc threshold:%d\n", low);
	bat_data->trip_temp = low;
	if (low == INT_MIN) {
		bat_data->irq_enabled = false;
		goto unlock_and_exit;
	}
	bat_data->irq_enabled = true;
	schedule_work(&bcl_perph->soc_eval_work);

unlock_and_exit:
	mutex_unlock(&bat_data->state_trans_lock);
	return 0;
}

static int bcl_read_soc(void *data, int *val)
{
	static struct power_supply *batt_psy;
	union power_supply_propval ret = {0,};
	int err = 0;

	*val = 100;
	if (!batt_psy)
		batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		err = power_supply_get_property(batt_psy,
				POWER_SUPPLY_PROP_CAPACITY, &ret);
		if (err) {
			pr_err("battery percentage read error:%d\n",
				err);
			return err;
		}
		*val = ret.intval;
	}
	pr_debug("soc:%d\n", *val);

	return err;
}

static void bcl_evaluate_soc(struct work_struct *work)
{
	int battery_percentage;
	struct bcl_peripheral_data *perph_data =
		&bcl_perph->param[BCL_SOC_MONITOR];

	if (bcl_read_soc((void *)perph_data, &battery_percentage))
		return;

	mutex_lock(&perph_data->state_trans_lock);
	if (!perph_data->irq_enabled)
		goto eval_exit;
	if (battery_percentage > perph_data->trip_temp)
		goto eval_exit;

	perph_data->trip_val = battery_percentage;
	mutex_unlock(&perph_data->state_trans_lock);
	of_thermal_handle_trip(perph_data->tz_dev);

	return;
eval_exit:
	mutex_unlock(&perph_data->state_trans_lock);
}

static int battery_supply_callback(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct power_supply *psy = data;

	if (strcmp(psy->desc->name, "battery"))
		return NOTIFY_OK;
	schedule_work(&bcl_perph->soc_eval_work);

	return NOTIFY_OK;
}

static void bcl_fetch_trip(struct platform_device *pdev, const char *int_name,
		struct bcl_peripheral_data *data,
		irqreturn_t (*handle)(int, void *))
{
	int ret = 0, irq_num = 0;

	/*
	 * Allow flexibility for the HLOS to set the trip temperature for
	 * all the thresholds but handle the interrupt for only one vbat
	 * and ibat interrupt. The LMH-DCVSh will handle and mitigate for the
	 * rest of the ibat/vbat interrupts.
	 */
	if (!handle) {
		mutex_lock(&data->state_trans_lock);
		data->irq_num = 0;
		data->irq_enabled = false;
		mutex_unlock(&data->state_trans_lock);
		return;
	}

	irq_num = platform_get_irq_byname(pdev, int_name);
	if (irq_num) {
		mutex_lock(&data->state_trans_lock);
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
		data->irq_enabled = false;
		mutex_unlock(&data->state_trans_lock);
	}
}

static void bcl_probe_soc(struct platform_device *pdev)
{
	int ret = 0;
	struct bcl_peripheral_data *soc_data;

	soc_data = &bcl_perph->param[BCL_SOC_MONITOR];
	mutex_init(&soc_data->state_trans_lock);
	soc_data->ops.get_temp = bcl_read_soc;
	soc_data->ops.set_trips = bcl_set_soc;
	INIT_WORK(&bcl_perph->soc_eval_work, bcl_evaluate_soc);
	bcl_perph->psy_nb.notifier_call = battery_supply_callback;
	ret = power_supply_reg_notifier(&bcl_perph->psy_nb);
	if (ret < 0) {
		pr_err("Unable to register soc notifier. err:%d\n", ret);
		return;
	}
	soc_data->tz_dev = thermal_zone_of_sensor_register(&pdev->dev,
				BCL_SOC_MONITOR, soc_data, &soc_data->ops);
	if (IS_ERR(soc_data->tz_dev)) {
		pr_err("vbat register failed. err:%ld\n",
				PTR_ERR(soc_data->tz_dev));
		return;
	}
	thermal_zone_device_update(soc_data->tz_dev, THERMAL_DEVICE_UP);
	schedule_work(&bcl_perph->soc_eval_work);
}

static void bcl_vbat_init(struct platform_device *pdev,
		struct bcl_peripheral_data *vbat, enum bcl_dev_type type)
{
	mutex_init(&vbat->state_trans_lock);
	switch (type) {
	case BCL_LOW_VBAT:
		bcl_fetch_trip(pdev, BCL_VBAT_INT, vbat, bcl_handle_vbat);
		break;
	case BCL_VLOW_VBAT:
		bcl_fetch_trip(pdev, BCL_VLOW_VBAT_INT, vbat, NULL);
		break;
	case BCL_CLOW_VBAT:
		bcl_fetch_trip(pdev, BCL_CLOW_VBAT_INT, vbat, NULL);
		break;
	default:
		return;
	}
	vbat->ops.get_temp = bcl_read_vbat_and_clear;
	vbat->ops.set_trips = bcl_set_vbat;
	vbat->tz_dev = thermal_zone_of_sensor_register(&pdev->dev,
				type, vbat, &vbat->ops);
	if (IS_ERR(vbat->tz_dev)) {
		pr_err("vbat register failed. err:%ld\n",
				PTR_ERR(vbat->tz_dev));
		return;
	}
	thermal_zone_device_update(vbat->tz_dev, THERMAL_DEVICE_UP);
}

static void bcl_probe_vbat(struct platform_device *pdev)
{
	bcl_vbat_init(pdev, &bcl_perph->param[BCL_LOW_VBAT], BCL_LOW_VBAT);
	bcl_vbat_init(pdev, &bcl_perph->param[BCL_VLOW_VBAT], BCL_VLOW_VBAT);
	bcl_vbat_init(pdev, &bcl_perph->param[BCL_CLOW_VBAT], BCL_CLOW_VBAT);
}

static void bcl_ibat_init(struct platform_device *pdev,
		struct bcl_peripheral_data *ibat, enum bcl_dev_type type)
{
	mutex_init(&ibat->state_trans_lock);
	if (type == BCL_HIGH_IBAT)
		bcl_fetch_trip(pdev, BCL_IBAT_INT, ibat, bcl_handle_ibat);
	else
		bcl_fetch_trip(pdev, BCL_VHIGH_IBAT_INT, ibat, NULL);
	ibat->ops.get_temp = bcl_read_ibat_and_clear;
	ibat->ops.set_trips = bcl_set_ibat;
	ibat->tz_dev = thermal_zone_of_sensor_register(&pdev->dev,
				type, ibat, &ibat->ops);
	if (IS_ERR(ibat->tz_dev)) {
		pr_err("ibat register failed. err:%ld\n",
				PTR_ERR(ibat->tz_dev));
		return;
	}
	thermal_zone_device_update(ibat->tz_dev, THERMAL_DEVICE_UP);
}

static void bcl_probe_ibat(struct platform_device *pdev)
{
	bcl_ibat_init(pdev, &bcl_perph->param[BCL_HIGH_IBAT], BCL_HIGH_IBAT);
	bcl_ibat_init(pdev, &bcl_perph->param[BCL_VHIGH_IBAT], BCL_VHIGH_IBAT);
}

static void bcl_configure_lmh_peripheral(void)
{
	bcl_write_register(BCL_LMH_CFG, BCL_LMH_CFG_VAL);
	bcl_write_register(BCL_CFG, BCL_CFG_VAL);
	bcl_write_general_register(LMH_INT_POL_HIGH,
			bcl_perph->fg_lmh_addr, LMH_INT_VAL);
	bcl_write_general_register(LMH_INT_EN,
			bcl_perph->fg_lmh_addr, LMH_INT_VAL);
}

static int bcl_remove(struct platform_device *pdev)
{
	int i = 0;

	for (; i < BCL_TYPE_MAX; i++) {
		if (!bcl_perph->param[i].tz_dev)
			continue;
		if (i == BCL_SOC_MONITOR) {
			power_supply_unreg_notifier(&bcl_perph->psy_nb);
			flush_work(&bcl_perph->soc_eval_work);
		}
		thermal_zone_of_sensor_unregister(&pdev->dev,
				bcl_perph->param[i].tz_dev);
	}
	bcl_perph = NULL;

	return 0;
}

static int bcl_probe(struct platform_device *pdev)
{
	int ret = 0;

	bcl_perph = devm_kzalloc(&pdev->dev, sizeof(*bcl_perph), GFP_KERNEL);
	if (!bcl_perph)
		return -ENOMEM;

	bcl_perph->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!bcl_perph->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	bcl_get_devicetree_data(pdev);
	bcl_probe_ibat(pdev);
	bcl_probe_vbat(pdev);
	bcl_probe_soc(pdev);
	bcl_configure_lmh_peripheral();

	dev_set_drvdata(&pdev->dev, bcl_perph);
	ret = bcl_write_register(BCL_MONITOR_EN, BIT(7));
	if (ret) {
		pr_err("Error accessing BCL peripheral. err:%d\n", ret);
		goto bcl_probe_exit;
	}

	return 0;

bcl_probe_exit:
	bcl_remove(pdev);
	return ret;
}

static const struct of_device_id bcl_match[] = {
	{
		.compatible = "qcom,msm-bcl-lmh",
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
