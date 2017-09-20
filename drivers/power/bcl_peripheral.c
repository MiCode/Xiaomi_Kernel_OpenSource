/*
 * Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/spmi.h>
#include <linux/mutex.h>
#include <linux/msm_bcl.h>
#include <linux/power_supply.h>

#define CREATE_TRACE_POINTS
#define _BCL_HW_TRACE
#include <trace/trace_thermal.h>

#define BCL_DRIVER_NAME         "bcl_peripheral"
#define BCL_VBAT_INT_NAME       "bcl-low-vbat-int"
#define BCL_IBAT_INT_NAME       "bcl-high-ibat-int"
#define BCL_PARAM_MAX_ATTR      3

#define BCL_MONITOR_EN          0x46
#define BCL_VBAT_VALUE          0x54
#define BCL_IBAT_VALUE          0x55
#define BCL_VBAT_MIN            0x58
#define BCL_IBAT_MAX            0x59
#define BCL_V_GAIN_BAT          0x60
#define BCL_I_GAIN_RSENSE       0x61
#define BCL_I_OFFSET_RSENSE     0x62
#define BCL_I_GAIN_BATFET       0x63
#define BCL_I_OFFSET_BATFET     0x64
#define BCL_I_SENSE_SRC         0x65
#define BCL_VBAT_MIN_CLR        0x66
#define BCL_IBAT_MAX_CLR        0x67
#define BCL_VBAT_TRIP           0x68
#define BCL_IBAT_TRIP           0x69

#define BCL_8998_VBAT_VALUE     0x58
#define BCL_8998_IBAT_VALUE     0x59
#define BCL_8998_VBAT_MIN       0x5C
#define BCL_8998_IBAT_MAX       0x5D
#define BCL_8998_MAX_MIN_CLR    0x48
#define BCL_8998_IBAT_MAX_CLR   3
#define BCL_8998_VBAT_MIN_CLR   2
#define BCL_8998_VBAT_ADC_LOW   0x72
#define BCL_8998_IBAT_HIGH      0x78
#define BCL_8998_BCL_CFG        0x6A

#define BCL_8998_VBAT_SCALING   39000
#define BCL_8998_IBAT_SCALING   80000
#define BCL_CFG_VAL             0x81

#define BCL_CONSTANT_NUM        32
#define BCL_READ_RETRY_LIMIT    3
#define VAL_CP_REG_BUF_LEN      3
#define VAL_REG_BUF_OFFSET      0
#define VAL_CP_REG_BUF_OFFSET   2
#define PON_SPARE_FULL_CURRENT		0x0
#define PON_SPARE_DERATED_CURRENT	0x1

#define READ_CONV_FACTOR(_node, _key, _val, _ret, _dest) do { \
		_ret = of_property_read_u32(_node, _key, &_val); \
		if (_ret) { \
			pr_err("Error reading key:%s. err:%d\n", _key, _ret); \
			goto bcl_dev_exit; \
		} \
		_dest = _val; \
	} while (0)

#define READ_OPTIONAL_PROP(_node, _key, _val, _ret, _dest) do { \
		_ret = of_property_read_u32(_node, _key, &_val); \
		if (_ret && _ret != -EINVAL) { \
			pr_err("Error reading key:%s. err:%d\n", _key, _ret); \
			goto bcl_dev_exit; \
		} else if (!_ret) { \
			_dest = _val; \
		} else { \
			_ret = 0; \
		} \
	} while (0)

enum bcl_monitor_state {
	BCL_PARAM_INACTIVE,
	BCL_PARAM_MONITOR,
	BCL_PARAM_POLLING,
};

enum bcl_hw_type {
	BCL_PMI8994,
	BCL_PMI8998,
	BCL_VERSION_MAX,
};

struct bcl_peripheral_data {
	struct bcl_param_data   *param_data;
	struct bcl_driver_ops   ops;
	enum bcl_monitor_state  state;
	struct delayed_work     poll_work;
	int                     irq_num;
	int                     high_trip;
	int                     low_trip;
	int                     trip_val;
	int                     scaling_factor;
	int                     offset_factor_num;
	int                     offset_factor_den;
	int                     offset;
	int                     gain_factor_num;
	int                     gain_factor_den;
	int                     gain;
	uint32_t                polling_delay_ms;
	int			inhibit_derating_ua;
	int (*read_max)         (int *adc_value);
	int (*clear_max)        (void);
	struct mutex            state_trans_lock;
};

struct bcl_device {
	bool                    enabled;
	struct device           *dev;
	struct spmi_device      *spmi;
	uint16_t                base_addr;
	uint16_t                pon_spare_addr;
	uint8_t                 slave_id;
	int                     i_src;
	struct bcl_peripheral_data   param[BCL_PARAM_MAX];
};

static struct bcl_device *bcl_perph;
static struct power_supply bcl_psy;
static const char bcl_psy_name[] = "fg_adc";
static bool calibration_done;
static DEFINE_MUTEX(bcl_access_mutex);
static DEFINE_MUTEX(bcl_enable_mutex);
static enum bcl_hw_type bcl_perph_version;

static int bcl_read_multi_register(int16_t reg_offset, uint8_t *data, int len)
{
	int  ret = 0, trace_len = 0;

	if (!bcl_perph) {
		pr_err("BCL device not initialized\n");
		return -EINVAL;
	}
	ret = spmi_ext_register_readl(bcl_perph->spmi->ctrl,
		bcl_perph->slave_id, (bcl_perph->base_addr + reg_offset),
		data, len);
	if (ret < 0) {
		pr_err("Error reading register %d. err:%d", reg_offset, ret);
		return ret;
	}
	while (trace_len < len) {
		trace_bcl_hw_reg_access("Read",
			bcl_perph->base_addr + reg_offset + trace_len,
			data[trace_len]);
		trace_len++;
	}

	return ret;
}

static int bcl_read_register(int16_t reg_offset, uint8_t *data)
{
	return bcl_read_multi_register(reg_offset, data, 1);
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
	ret = spmi_ext_register_writel(bcl_perph->spmi->ctrl,
		bcl_perph->slave_id, (base + reg_offset),
		write_buf, 1);
	if (ret < 0) {
		pr_err("Error reading register %d. err:%d", reg_offset, ret);
		return ret;
	}
	pr_debug("wrote 0x%02x to 0x%04x\n", data, base + reg_offset);
	trace_bcl_hw_reg_access("write", base + reg_offset, data);

	return ret;
}

static int bcl_write_register(int16_t reg_offset, uint8_t data)
{
	return bcl_write_general_register(reg_offset,
			bcl_perph->base_addr, data);
}

static void convert_vbat_to_adc_val(int *val)
{
	struct bcl_peripheral_data *perph_data = NULL;

	switch (bcl_perph_version) {
	case BCL_PMI8994:
		if (!bcl_perph)
			return;
		perph_data = &bcl_perph->param[BCL_PARAM_VOLTAGE];
		*val = (*val * 100
			/ (100 + (perph_data->gain_factor_num
			* perph_data->gain) * BCL_CONSTANT_NUM
			/ perph_data->gain_factor_den))
			/ perph_data->scaling_factor;
		break;
	case BCL_PMI8998:
		*val = *val / BCL_8998_VBAT_SCALING;
		break;
	default:
		break;
	}

	return;
}

static void convert_adc_to_vbat_val(int *val)
{
	struct bcl_peripheral_data *perph_data = NULL;

	switch (bcl_perph_version) {
	case BCL_PMI8994:
		if (!bcl_perph)
			return;
		perph_data = &bcl_perph->param[BCL_PARAM_VOLTAGE];
		*val = ((*val + 2) * perph_data->scaling_factor)
			* (100 + (perph_data->gain_factor_num
			* perph_data->gain)
			* BCL_CONSTANT_NUM  / perph_data->gain_factor_den)
			/ 100;
		break;
	case BCL_PMI8998:
		*val = *val * BCL_8998_VBAT_SCALING;
		break;
	default:
		break;
	}

	return;
}

static void convert_ibat_to_adc_val(int *val)
{
	struct bcl_peripheral_data *perph_data = NULL;

	switch (bcl_perph_version) {
	case BCL_PMI8994:
		if (!bcl_perph)
			return;
		perph_data = &bcl_perph->param[BCL_PARAM_CURRENT];
		*val = (*val * 100
			/ (100 + (perph_data->gain_factor_num
			* perph_data->gain)
			* BCL_CONSTANT_NUM / perph_data->gain_factor_den)
			- (perph_data->offset_factor_num * perph_data->offset)
			/ perph_data->offset_factor_den)
			/  perph_data->scaling_factor;
		break;
	case BCL_PMI8998:
		*val = *val / BCL_8998_IBAT_SCALING;
		break;
	default:
		break;
	}

	return;
}

static void convert_adc_to_ibat_val(int *val)
{
	struct bcl_peripheral_data *perph_data = NULL;

	switch (bcl_perph_version) {
	case BCL_PMI8994:
		if (!bcl_perph)
			return;
		perph_data = &bcl_perph->param[BCL_PARAM_CURRENT];
		*val = (*val * perph_data->scaling_factor
			+ (perph_data->offset_factor_num * perph_data->offset)
			/ perph_data->offset_factor_den)
			* (100 + (perph_data->gain_factor_num
			* perph_data->gain) * BCL_CONSTANT_NUM /
			perph_data->gain_factor_den) / 100;
		break;
	case BCL_PMI8998:
		*val = *val * BCL_8998_IBAT_SCALING;
		break;
	default:
		break;
	}

	return;
}

static int bcl_set_high_vbat(int thresh_value)
{
	bcl_perph->param[BCL_PARAM_VOLTAGE].high_trip = thresh_value;
	return 0;
}

static int bcl_set_low_ibat(int thresh_value)
{
	bcl_perph->param[BCL_PARAM_CURRENT].low_trip = thresh_value;
	return 0;
}

static int bcl_set_high_ibat(int thresh_value)
{
	int ret = 0, ibat_ua;
	int8_t val = 0;

	ibat_ua = thresh_value;
	convert_ibat_to_adc_val(&thresh_value);
	pr_debug("Setting Ibat high trip:%d. ADC_val:%d\n", ibat_ua,
			thresh_value);
	val = (int8_t)thresh_value;
	ret = bcl_write_register((bcl_perph_version == BCL_PMI8994) ?
		BCL_IBAT_TRIP : BCL_8998_IBAT_HIGH, val);
	if (ret) {
		pr_err("Error accessing BCL peripheral. err:%d\n", ret);
		return ret;
	}
	bcl_perph->param[BCL_PARAM_CURRENT].high_trip = thresh_value;

	if (bcl_perph->param[BCL_PARAM_CURRENT].inhibit_derating_ua == 0
			|| bcl_perph->pon_spare_addr == 0)
		return ret;

	ret = bcl_write_general_register(bcl_perph->pon_spare_addr,
			PON_SPARE_FULL_CURRENT, val);
	if (ret) {
		pr_debug("Error accessing PON register. err:%d\n", ret);
		return ret;
	}
	thresh_value = ibat_ua
		- bcl_perph->param[BCL_PARAM_CURRENT].inhibit_derating_ua;
	convert_ibat_to_adc_val(&thresh_value);
	val = (int8_t)thresh_value;
	ret = bcl_write_general_register(bcl_perph->pon_spare_addr,
			PON_SPARE_DERATED_CURRENT, val);
	if (ret) {
		pr_debug("Error accessing PON register. err:%d\n", ret);
		return ret;
	}

	return ret;
}

static int bcl_set_low_vbat(int thresh_value)
{
	int ret = 0, vbat_uv;
	int8_t val = 0;

	vbat_uv = thresh_value;
	convert_vbat_to_adc_val(&thresh_value);
	pr_debug("Setting Vbat low trip:%d. ADC_val:%d\n", vbat_uv,
			thresh_value);
	val = (int8_t)thresh_value;
	ret = bcl_write_register((bcl_perph_version == BCL_PMI8994)
		? BCL_VBAT_TRIP : BCL_8998_VBAT_ADC_LOW, val);
	if (ret) {
		pr_err("Error accessing BCL peripheral. err:%d\n", ret);
		return ret;
	}
	bcl_perph->param[BCL_PARAM_VOLTAGE].low_trip = thresh_value;

	return ret;
}

static int bcl_access_monitor_enable(bool enable)
{
	int ret = 0, i = 0;
	struct bcl_peripheral_data *perph_data = NULL;

	mutex_lock(&bcl_enable_mutex);
	if (enable == bcl_perph->enabled)
		goto access_exit;

	for (; i < BCL_PARAM_MAX; i++) {
		perph_data = &bcl_perph->param[i];
		mutex_lock(&perph_data->state_trans_lock);
		if (enable) {
			switch (perph_data->state) {
			case BCL_PARAM_INACTIVE:
				trace_bcl_hw_state_event(
					(i == BCL_PARAM_VOLTAGE)
					? "Voltage Inactive to Monitor"
					: "Current Inactive to Monitor",
					0);
				enable_irq(perph_data->irq_num);
				break;
			case BCL_PARAM_POLLING:
			case BCL_PARAM_MONITOR:
			default:
				break;
			}
			perph_data->state = BCL_PARAM_MONITOR;
		} else {
			switch (perph_data->state) {
			case BCL_PARAM_MONITOR:
				trace_bcl_hw_state_event(
					(i == BCL_PARAM_VOLTAGE)
					? "Voltage Monitor to Inactive"
					: "Current Monitor to Inactive",
					0);
				disable_irq_nosync(perph_data->irq_num);
				/* Fall through to clear the poll work */
			case BCL_PARAM_INACTIVE:
			case BCL_PARAM_POLLING:
				cancel_delayed_work(&perph_data->poll_work);
				break;
			default:
				break;
			}
			perph_data->state = BCL_PARAM_INACTIVE;
		}
		mutex_unlock(&perph_data->state_trans_lock);
	}
	bcl_perph->enabled = enable;

access_exit:
	mutex_unlock(&bcl_enable_mutex);
	return ret;
}

static int bcl_monitor_enable(void)
{
	trace_bcl_hw_event("BCL Enable");
	return bcl_access_monitor_enable(true);
}

static int bcl_monitor_disable(void)
{
	trace_bcl_hw_event("BCL Disable");
	return bcl_access_monitor_enable(false);
}

static int bcl_read_ibat_high_trip(int *thresh_value)
{
	int ret = 0;
	int8_t val = 0;

	*thresh_value = (int)val;
	ret = bcl_read_register((bcl_perph_version == BCL_PMI8994) ?
		BCL_IBAT_TRIP : BCL_8998_IBAT_HIGH, &val);
	if (ret) {
		pr_err("BCL register read error. err:%d\n", ret);
		ret = 0;
		val = bcl_perph->param[BCL_PARAM_CURRENT].high_trip;
		*thresh_value = (int)val;
	} else {
		*thresh_value = (int)val;
		convert_adc_to_ibat_val(thresh_value);
		pr_debug("Reading Ibat high trip:%d. ADC_val:%d\n",
				*thresh_value, val);
	}

	return ret;
}

static int bcl_read_ibat_low_trip(int *thresh_value)
{
	*thresh_value = bcl_perph->param[BCL_PARAM_CURRENT].low_trip;
	return 0;
}

static int bcl_read_vbat_low_trip(int *thresh_value)
{
	int ret = 0;
	int8_t val = 0;

	*thresh_value = (int)val;
	ret = bcl_read_register((bcl_perph_version == BCL_PMI8994)
			? BCL_VBAT_TRIP : BCL_8998_VBAT_ADC_LOW,
			&val);
	if (ret) {
		pr_err("BCL register read error. err:%d\n", ret);
		ret = 0;
		*thresh_value = bcl_perph->param[BCL_PARAM_VOLTAGE].low_trip;
	} else {
		*thresh_value = (int)val;
		convert_adc_to_vbat_val(thresh_value);
		pr_debug("Reading Ibat high trip:%d. ADC_val:%d\n",
				*thresh_value, val);
	}

	return ret;
}

static int bcl_read_vbat_high_trip(int *thresh_value)
{
	*thresh_value = bcl_perph->param[BCL_PARAM_VOLTAGE].high_trip;
	return 0;
}

static int bcl_clear_vbat_min(void)
{
	int ret  = 0;

	if (bcl_perph_version == BCL_PMI8994)
		ret = bcl_write_register(BCL_VBAT_MIN_CLR, BIT(7));
	else
		ret = bcl_write_register(BCL_8998_MAX_MIN_CLR,
			BIT(BCL_8998_VBAT_MIN_CLR));
	if (ret)
		pr_err("Error in clearing vbat min reg. err:%d", ret);

	return ret;
}

static int bcl_clear_ibat_max(void)
{
	int ret  = 0;

	if (bcl_perph_version == BCL_PMI8994)
		ret = bcl_write_register(BCL_IBAT_MAX_CLR, BIT(7));
	else
		ret = bcl_write_register(BCL_8998_MAX_MIN_CLR,
			BIT(BCL_8998_IBAT_MAX_CLR));
	if (ret)
		pr_err("Error in clearing ibat max reg. err:%d", ret);

	return ret;
}

static int bcl_read_ibat_max(int *adc_value)
{
	int ret = 0, timeout = 0;
	int8_t val[VAL_CP_REG_BUF_LEN] = {0};

	*adc_value = (int)val[VAL_REG_BUF_OFFSET];
	do {
		ret = bcl_read_multi_register(
			(bcl_perph_version == BCL_PMI8994) ? BCL_IBAT_MAX
			: BCL_8998_IBAT_MAX, val,
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
	convert_adc_to_ibat_val(adc_value);
	pr_debug("Ibat Max:%d. ADC_val:%d\n", *adc_value,
			val[VAL_REG_BUF_OFFSET]);
	trace_bcl_hw_sensor_reading("Ibat Max[uA]", *adc_value);

bcl_read_exit:
	return ret;
}

static int bcl_read_vbat_min(int *adc_value)
{
	int ret = 0, timeout = 0;
	int8_t val[VAL_CP_REG_BUF_LEN] = {0};

	*adc_value = (int)val[VAL_REG_BUF_OFFSET];
	do {
		ret = bcl_read_multi_register(
			(bcl_perph_version == BCL_PMI8994) ? BCL_VBAT_MIN
			: BCL_8998_VBAT_MIN, val,
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
	convert_adc_to_vbat_val(adc_value);
	pr_debug("Vbat Min:%d. ADC_val:%d\n", *adc_value,
			val[VAL_REG_BUF_OFFSET]);
	trace_bcl_hw_sensor_reading("vbat Min[uV]", *adc_value);

bcl_read_exit:
	return ret;
}

static int bcl_read_ibat(int *adc_value)
{
	int ret = 0, timeout = 0;
	int8_t val[VAL_CP_REG_BUF_LEN] = {0};

	*adc_value = (int)val[VAL_REG_BUF_OFFSET];
	do {
		ret = bcl_read_multi_register(
			(bcl_perph_version == BCL_PMI8994) ? BCL_IBAT_VALUE
			: BCL_8998_IBAT_VALUE, val,
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
	convert_adc_to_ibat_val(adc_value);
	pr_debug("Read Ibat:%d. ADC_val:%d\n", *adc_value,
			val[VAL_REG_BUF_OFFSET]);
	trace_bcl_hw_sensor_reading("ibat[uA]", *adc_value);

bcl_read_exit:
	return ret;
}

static int bcl_read_vbat(int *adc_value)
{
	int ret = 0, timeout = 0;
	int8_t val[VAL_CP_REG_BUF_LEN] = {0};

	*adc_value = (int)val[VAL_REG_BUF_OFFSET];
	do {
		ret = bcl_read_multi_register(
			(bcl_perph_version == BCL_PMI8994) ? BCL_VBAT_VALUE :
			BCL_8998_VBAT_VALUE, val,
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
	convert_adc_to_vbat_val(adc_value);
	pr_debug("Read Vbat:%d. ADC_val:%d\n", *adc_value,
			val[VAL_REG_BUF_OFFSET]);
	trace_bcl_hw_sensor_reading("vbat[uV]", *adc_value);

bcl_read_exit:
	return ret;
}

static void bcl_poll_ibat_low(struct work_struct *work)
{
	int ret = 0, val = 0;
	struct bcl_peripheral_data *perph_data =
		&bcl_perph->param[BCL_PARAM_CURRENT];

	trace_bcl_hw_event("ibat poll low. Enter");
	mutex_lock(&perph_data->state_trans_lock);
	if (perph_data->state != BCL_PARAM_POLLING) {
		pr_err("Invalid ibat state %d\n", perph_data->state);
		goto exit_ibat;
	}

	ret = perph_data->read_max(&val);
	if (ret) {
		pr_err("Error in reading ibat. err:%d", ret);
		goto reschedule_ibat;
	}
	ret = perph_data->clear_max();
	if (ret)
		pr_err("Error clearing max ibat reg. err:%d\n", ret);
	if (val <= perph_data->low_trip) {
		pr_debug("Ibat reached low clear trip. ibat:%d\n", val);
		trace_bcl_hw_state_event("Polling to Monitor. Ibat[uA]:", val);
		trace_bcl_hw_mitigation("Ibat low trip. Ibat[uA]", val);
		perph_data->ops.notify(perph_data->param_data, val,
			BCL_LOW_TRIP);
		perph_data->state = BCL_PARAM_MONITOR;
		enable_irq(perph_data->irq_num);
	} else {
		goto reschedule_ibat;
	}

exit_ibat:
	mutex_unlock(&perph_data->state_trans_lock);
	trace_bcl_hw_event("ibat poll low. Exit");
	return;

reschedule_ibat:
	mutex_unlock(&perph_data->state_trans_lock);
	schedule_delayed_work(&perph_data->poll_work,
		msecs_to_jiffies(perph_data->polling_delay_ms));
	trace_bcl_hw_event("ibat poll low. Exit");
	return;
}

static void bcl_poll_vbat_high(struct work_struct *work)
{
	int ret = 0, val = 0;
	struct bcl_peripheral_data *perph_data =
		&bcl_perph->param[BCL_PARAM_VOLTAGE];

	trace_bcl_hw_event("vbat poll high. Enter");
	mutex_lock(&perph_data->state_trans_lock);
	if (perph_data->state != BCL_PARAM_POLLING) {
		pr_err("Invalid vbat state %d\n", perph_data->state);
		goto exit_vbat;
	}

	ret = perph_data->read_max(&val);
	if (ret) {
		pr_err("Error in reading vbat. err:%d", ret);
		goto reschedule_vbat;
	}
	ret = perph_data->clear_max();
	if (ret)
		pr_err("Error clearing min vbat reg. err:%d\n", ret);
	if (val >= perph_data->high_trip) {
		pr_debug("Vbat reached high clear trip. vbat:%d\n", val);
		trace_bcl_hw_state_event("Polling to Monitor. vbat[uV]:", val);
		trace_bcl_hw_mitigation("vbat high trip. vbat[uV]", val);
		perph_data->ops.notify(perph_data->param_data, val,
			BCL_HIGH_TRIP);
		perph_data->state = BCL_PARAM_MONITOR;
		enable_irq(perph_data->irq_num);
	} else {
		goto reschedule_vbat;
	}

exit_vbat:
	mutex_unlock(&perph_data->state_trans_lock);
	trace_bcl_hw_event("vbat poll high. Exit");
	return;

reschedule_vbat:
	mutex_unlock(&perph_data->state_trans_lock);
	schedule_delayed_work(&perph_data->poll_work,
		msecs_to_jiffies(perph_data->polling_delay_ms));
	trace_bcl_hw_event("vbat poll high. Exit");
	return;
}

static irqreturn_t bcl_handle_ibat(int irq, void *data)
{
	int thresh_value = 0, ret = 0;
	struct bcl_peripheral_data *perph_data =
		(struct bcl_peripheral_data *)data;

	trace_bcl_hw_mitigation_event("Ibat interrupted");
	mutex_lock(&perph_data->state_trans_lock);
	if (perph_data->state == BCL_PARAM_MONITOR) {
		ret = perph_data->read_max(&perph_data->trip_val);
		if (ret) {
			pr_err("Error reading max/min reg. err:%d\n", ret);
			goto exit_intr;
		}
		ret = perph_data->clear_max();
		if (ret)
			pr_err("Error clearing max/min reg. err:%d\n", ret);
		thresh_value = perph_data->high_trip;
		convert_adc_to_ibat_val(&thresh_value);
		/* Account threshold trip from PBS threshold for dead time */
		thresh_value -= perph_data->inhibit_derating_ua;
		if (perph_data->trip_val < thresh_value) {
			pr_debug("False Ibat high trip. ibat:%d ibat_thresh_val:%d\n",
				perph_data->trip_val, thresh_value);
			trace_bcl_hw_event("Ibat invalid interrupt");
			goto exit_intr;
		}
		pr_debug("Ibat reached high trip. ibat:%d\n",
				perph_data->trip_val);
		trace_bcl_hw_state_event("Monitor to Polling. ibat[uA]:",
				perph_data->trip_val);
		disable_irq_nosync(perph_data->irq_num);
		perph_data->state = BCL_PARAM_POLLING;
		trace_bcl_hw_mitigation("ibat high trip. ibat[uA]",
				perph_data->trip_val);
		perph_data->ops.notify(perph_data->param_data,
			perph_data->trip_val, BCL_HIGH_TRIP);
		schedule_delayed_work(&perph_data->poll_work,
			msecs_to_jiffies(perph_data->polling_delay_ms));
	} else {
		pr_debug("Ignoring interrupt\n");
		trace_bcl_hw_event("Ibat Ignoring interrupt");
	}

exit_intr:
	mutex_unlock(&perph_data->state_trans_lock);
	return IRQ_HANDLED;
}

static irqreturn_t bcl_handle_vbat(int irq, void *data)
{
	int thresh_value = 0, ret = 0;
	struct bcl_peripheral_data *perph_data =
		(struct bcl_peripheral_data *)data;

	trace_bcl_hw_mitigation_event("Vbat Interrupted");
	mutex_lock(&perph_data->state_trans_lock);
	if (perph_data->state == BCL_PARAM_MONITOR) {
		ret = perph_data->read_max(&perph_data->trip_val);
		if (ret) {
			pr_err("Error reading max/min reg. err:%d\n", ret);
			goto exit_intr;
		}
		ret = perph_data->clear_max();
		if (ret)
			pr_err("Error clearing max/min reg. err:%d\n", ret);
		thresh_value = perph_data->low_trip;
		convert_adc_to_vbat_val(&thresh_value);
		if (perph_data->trip_val > thresh_value) {
			pr_debug("False vbat min trip. vbat:%d vbat_thresh_val:%d\n",
				perph_data->trip_val, thresh_value);
			trace_bcl_hw_event("Vbat Invalid interrupt");
			goto exit_intr;
		}
		pr_debug("Vbat reached Low trip. vbat:%d\n",
			perph_data->trip_val);
		trace_bcl_hw_state_event("Monitor to Polling. vbat[uV]:",
				perph_data->trip_val);
		disable_irq_nosync(perph_data->irq_num);
		perph_data->state = BCL_PARAM_POLLING;
		trace_bcl_hw_mitigation("vbat low trip. vbat[uV]",
				perph_data->trip_val);
		perph_data->ops.notify(perph_data->param_data,
			perph_data->trip_val, BCL_LOW_TRIP);
		schedule_delayed_work(&perph_data->poll_work,
			msecs_to_jiffies(perph_data->polling_delay_ms));
	} else {
		pr_debug("Ignoring interrupt\n");
		trace_bcl_hw_event("Vbat Ignoring interrupt");
	}

exit_intr:
	mutex_unlock(&perph_data->state_trans_lock);
	return IRQ_HANDLED;
}

static int bcl_get_devicetree_data(struct spmi_device *spmi)
{
	int ret = 0, irq_num = 0, temp_val = 0;
	struct resource *resource = NULL;
	char *key = NULL;
	const __be32 *prop = NULL;
	struct device_node *dev_node = spmi->dev.of_node;

	/* Get SPMI peripheral address */
	resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!resource) {
		pr_err("No base address defined\n");
		return -EINVAL;
	}
	bcl_perph->slave_id = spmi->sid;
	prop = of_get_address_by_name(dev_node,
			"fg_user_adc", 0, 0);
	if (prop) {
		bcl_perph->base_addr = be32_to_cpu(*prop);
		pr_debug("fg_user_adc@%04x\n", bcl_perph->base_addr);
	} else {
		dev_err(&spmi->dev, "No fg_user_adc registers found\n");
		return -EINVAL;
	}

	prop = of_get_address_by_name(dev_node,
			"pon_spare", 0, 0);
	if (prop) {
		bcl_perph->pon_spare_addr = be32_to_cpu(*prop);
		pr_debug("pon_spare@%04x\n", bcl_perph->pon_spare_addr);
	}

	/* Register SPMI peripheral interrupt */
	irq_num = spmi_get_irq_byname(spmi, NULL,
			BCL_VBAT_INT_NAME);
	if (irq_num < 0) {
		pr_err("Invalid vbat IRQ\n");
		ret = -ENXIO;
		goto bcl_dev_exit;
	}
	bcl_perph->param[BCL_PARAM_VOLTAGE].irq_num = irq_num;
	irq_num = spmi_get_irq_byname(spmi, NULL,
			BCL_IBAT_INT_NAME);
	if (irq_num < 0) {
		pr_err("Invalid ibat IRQ\n");
		ret = -ENXIO;
		goto bcl_dev_exit;
	}
	bcl_perph->param[BCL_PARAM_CURRENT].irq_num = irq_num;

	if (bcl_perph_version == BCL_PMI8994) {
		/* Get VADC and IADC scaling factor */
		key = "qcom,vbat-scaling-factor";
		READ_CONV_FACTOR(dev_node, key, temp_val, ret,
			bcl_perph->param[BCL_PARAM_VOLTAGE].scaling_factor);
		key = "qcom,vbat-gain-numerator";
		READ_CONV_FACTOR(dev_node, key, temp_val, ret,
			bcl_perph->param[BCL_PARAM_VOLTAGE].gain_factor_num);
		key = "qcom,vbat-gain-denominator";
		READ_CONV_FACTOR(dev_node, key, temp_val, ret,
			bcl_perph->param[BCL_PARAM_VOLTAGE].gain_factor_den);
		key = "qcom,ibat-scaling-factor";
		READ_CONV_FACTOR(dev_node, key, temp_val, ret,
			bcl_perph->param[BCL_PARAM_CURRENT].scaling_factor);
		key = "qcom,ibat-offset-numerator";
		READ_CONV_FACTOR(dev_node, key, temp_val, ret,
			bcl_perph->param[BCL_PARAM_CURRENT].offset_factor_num);
		key = "qcom,ibat-offset-denominator";
		READ_CONV_FACTOR(dev_node, key, temp_val, ret,
			bcl_perph->param[BCL_PARAM_CURRENT].offset_factor_den);
		key = "qcom,ibat-gain-numerator";
		READ_CONV_FACTOR(dev_node, key, temp_val, ret,
			bcl_perph->param[BCL_PARAM_CURRENT].gain_factor_num);
		key = "qcom,ibat-gain-denominator";
		READ_CONV_FACTOR(dev_node, key, temp_val, ret,
			bcl_perph->param[BCL_PARAM_CURRENT].gain_factor_den);
		key = "qcom,inhibit-derating-ua";
		READ_OPTIONAL_PROP(dev_node, key, temp_val, ret,
			bcl_perph->param[BCL_PARAM_CURRENT].
			inhibit_derating_ua);
	}
	key = "qcom,vbat-polling-delay-ms";
	READ_CONV_FACTOR(dev_node, key, temp_val, ret,
		bcl_perph->param[BCL_PARAM_VOLTAGE].polling_delay_ms);
	key = "qcom,ibat-polling-delay-ms";
	READ_CONV_FACTOR(dev_node, key, temp_val, ret,
		bcl_perph->param[BCL_PARAM_CURRENT].polling_delay_ms);

bcl_dev_exit:
	return ret;
}

static int bcl_calibrate(void)
{
	int ret = 0;
	int8_t i_src = 0, val = 0;

	ret = bcl_read_register(BCL_I_SENSE_SRC, &i_src);
	if (ret) {
		pr_err("Error reading current sense reg. err:%d\n", ret);
		goto bcl_cal_exit;
	}

	ret = bcl_read_register((i_src & 0x01) ? BCL_I_GAIN_RSENSE
		: BCL_I_GAIN_BATFET, &val);
	if (ret) {
		pr_err("Error reading %s current gain. err:%d\n",
			(i_src & 0x01) ? "rsense" : "batfet", ret);
		goto bcl_cal_exit;
	}
	bcl_perph->param[BCL_PARAM_CURRENT].gain = val;
	ret = bcl_read_register((i_src & 0x01) ? BCL_I_OFFSET_RSENSE
		: BCL_I_OFFSET_BATFET, &val);
	if (ret) {
		pr_err("Error reading %s current offset. err:%d\n",
			(i_src & 0x01) ? "rsense" : "batfet", ret);
		goto bcl_cal_exit;
	}
	bcl_perph->param[BCL_PARAM_CURRENT].offset = val;
	ret = bcl_read_register(BCL_V_GAIN_BAT, &val);
	if (ret) {
		pr_err("Error reading vbat offset. err:%d\n", ret);
		goto bcl_cal_exit;
	}
	bcl_perph->param[BCL_PARAM_VOLTAGE].gain = val;

	if (((i_src & 0x01) != bcl_perph->i_src)
		&& (bcl_perph->enabled)) {
		bcl_set_low_vbat(bcl_perph->param[BCL_PARAM_VOLTAGE]
				.low_trip);
		bcl_set_high_ibat(bcl_perph->param[BCL_PARAM_CURRENT]
				.high_trip);
		bcl_perph->i_src = i_src;
	}

bcl_cal_exit:
	return ret;
}

static void power_supply_callback(struct power_supply *psy)
{
	static struct power_supply *bms_psy;
	int ret = 0;

	if (calibration_done)
		return;

	if (!bms_psy)
		bms_psy = power_supply_get_by_name("bms");
	if (bms_psy) {
		calibration_done = true;
		trace_bcl_hw_event("Recalibrate callback");
		ret = bcl_calibrate();
		if (ret)
			pr_err("Could not read calibration values. err:%d",
				ret);
	}
}

static int bcl_psy_get_property(struct power_supply *psy,
				enum power_supply_property prop,
				union power_supply_propval *val)
{
	return 0;
}
static int bcl_psy_set_property(struct power_supply *psy,
				enum power_supply_property prop,
				const union power_supply_propval *val)
{
	return -EINVAL;
}

static int bcl_update_data(void)
{
	int ret = 0;

	bcl_perph->param[BCL_PARAM_VOLTAGE].ops.read = bcl_read_vbat;
	bcl_perph->param[BCL_PARAM_VOLTAGE].ops.get_high_trip
		= bcl_read_vbat_high_trip;
	bcl_perph->param[BCL_PARAM_VOLTAGE].ops.get_low_trip
		= bcl_read_vbat_low_trip;
	bcl_perph->param[BCL_PARAM_VOLTAGE].ops.set_high_trip
		= bcl_set_high_vbat;
	bcl_perph->param[BCL_PARAM_VOLTAGE].ops.set_low_trip
		= bcl_set_low_vbat;
	bcl_perph->param[BCL_PARAM_VOLTAGE].ops.enable
		 = bcl_monitor_enable;
	bcl_perph->param[BCL_PARAM_VOLTAGE].ops.disable
		= bcl_monitor_disable;
	bcl_perph->param[BCL_PARAM_VOLTAGE].read_max
		 = bcl_read_vbat_min;
	bcl_perph->param[BCL_PARAM_VOLTAGE].clear_max
		 = bcl_clear_vbat_min;

	bcl_perph->param[BCL_PARAM_CURRENT].ops.read = bcl_read_ibat;
	bcl_perph->param[BCL_PARAM_CURRENT].ops.get_high_trip
		= bcl_read_ibat_high_trip;
	bcl_perph->param[BCL_PARAM_CURRENT].ops.get_low_trip
		= bcl_read_ibat_low_trip;
	bcl_perph->param[BCL_PARAM_CURRENT].ops.set_high_trip
		 = bcl_set_high_ibat;
	bcl_perph->param[BCL_PARAM_CURRENT].ops.set_low_trip
		 = bcl_set_low_ibat;
	bcl_perph->param[BCL_PARAM_CURRENT].ops.enable
		= bcl_monitor_enable;
	bcl_perph->param[BCL_PARAM_CURRENT].ops.disable
		= bcl_monitor_disable;
	bcl_perph->param[BCL_PARAM_CURRENT].read_max
		= bcl_read_ibat_max;
	bcl_perph->param[BCL_PARAM_CURRENT].clear_max
		= bcl_clear_ibat_max;

	bcl_perph->param[BCL_PARAM_VOLTAGE].param_data = msm_bcl_register_param(
		BCL_PARAM_VOLTAGE, &bcl_perph->param[BCL_PARAM_VOLTAGE].ops,
		"vbat");
	if (!bcl_perph->param[BCL_PARAM_VOLTAGE].param_data) {
		pr_err("register Vbat failed.\n");
		ret = -ENODEV;
		goto update_data_exit;
	}
	bcl_perph->param[BCL_PARAM_CURRENT].param_data = msm_bcl_register_param(
		BCL_PARAM_CURRENT, &bcl_perph->param[BCL_PARAM_CURRENT].ops,
		"ibat");
	if (!bcl_perph->param[BCL_PARAM_CURRENT].param_data) {
		pr_err("register Ibat failed.\n");
		ret = -ENODEV;
		goto update_data_exit;
	}
	INIT_DELAYED_WORK(&bcl_perph->param[BCL_PARAM_VOLTAGE].poll_work,
		bcl_poll_vbat_high);
	INIT_DELAYED_WORK(&bcl_perph->param[BCL_PARAM_CURRENT].poll_work,
		bcl_poll_ibat_low);
	mutex_init(&bcl_perph->param[BCL_PARAM_CURRENT].state_trans_lock);
	mutex_init(&bcl_perph->param[BCL_PARAM_VOLTAGE].state_trans_lock);

update_data_exit:
	return ret;
}

static int bcl_probe(struct spmi_device *spmi)
{
	int ret = 0;

	bcl_perph = devm_kzalloc(&spmi->dev, sizeof(struct bcl_device),
			GFP_KERNEL);
	if (!bcl_perph) {
		pr_err("Memory alloc failed\n");
		return -ENOMEM;
	}
	bcl_perph->spmi = spmi;
	bcl_perph->dev = &(spmi->dev);

	ret = bcl_get_devicetree_data(spmi);
	if (ret) {
		pr_err("Device tree data fetch error. err:%d", ret);
		goto bcl_probe_exit;
	}
	if (bcl_perph_version == BCL_PMI8994) {
		ret = bcl_calibrate();
		if (ret) {
			pr_debug("Could not read calibration values. err:%d",
					ret);
			goto bcl_probe_exit;
		}
		bcl_psy.name = bcl_psy_name;
		bcl_psy.type = POWER_SUPPLY_TYPE_BMS;
		bcl_psy.get_property     = bcl_psy_get_property;
		bcl_psy.set_property     = bcl_psy_set_property;
		bcl_psy.num_properties   = 0;
		bcl_psy.external_power_changed = power_supply_callback;
		ret = power_supply_register(&spmi->dev, &bcl_psy);
		if (ret < 0) {
			pr_err("Unable to register bcl_psy rc = %d\n", ret);
			return ret;
		}
	} else {
		bcl_write_register(BCL_8998_BCL_CFG, BCL_CFG_VAL);
	}

	ret = bcl_update_data();
	if (ret) {
		pr_err("Update data failed. err:%d", ret);
		goto bcl_probe_exit;
	}
	mutex_lock(&bcl_perph->param[BCL_PARAM_VOLTAGE].state_trans_lock);
	ret = devm_request_threaded_irq(&spmi->dev,
			bcl_perph->param[BCL_PARAM_VOLTAGE].irq_num,
			NULL, bcl_handle_vbat,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			"bcl_vbat_interrupt",
			&bcl_perph->param[BCL_PARAM_VOLTAGE]);
	if (ret) {
		dev_err(&spmi->dev, "Error requesting VBAT irq. err:%d", ret);
		mutex_unlock(
			&bcl_perph->param[BCL_PARAM_VOLTAGE].state_trans_lock);
		goto bcl_probe_exit;
	}
	/*
	 * BCL is enabled by default in hardware.
	 * Disable BCL monitoring till a valid threshold is set by APPS
	 */
	disable_irq_nosync(bcl_perph->param[BCL_PARAM_VOLTAGE].irq_num);
	mutex_unlock(&bcl_perph->param[BCL_PARAM_VOLTAGE].state_trans_lock);

	mutex_lock(&bcl_perph->param[BCL_PARAM_CURRENT].state_trans_lock);
	ret = devm_request_threaded_irq(&spmi->dev,
			bcl_perph->param[BCL_PARAM_CURRENT].irq_num,
			NULL, bcl_handle_ibat,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			"bcl_ibat_interrupt",
			&bcl_perph->param[BCL_PARAM_CURRENT]);
	if (ret) {
		dev_err(&spmi->dev, "Error requesting IBAT irq. err:%d", ret);
		mutex_unlock(
			&bcl_perph->param[BCL_PARAM_CURRENT].state_trans_lock);
		goto bcl_probe_exit;
	}
	disable_irq_nosync(bcl_perph->param[BCL_PARAM_CURRENT].irq_num);
	mutex_unlock(&bcl_perph->param[BCL_PARAM_CURRENT].state_trans_lock);

	dev_set_drvdata(&spmi->dev, bcl_perph);
	ret = bcl_write_register(BCL_MONITOR_EN, BIT(7));
	if (ret) {
		pr_err("Error accessing BCL peripheral. err:%d\n", ret);
		goto bcl_probe_exit;
	}

	return 0;

bcl_probe_exit:
	bcl_perph = NULL;
	return ret;
}

static int bcl_remove(struct spmi_device *spmi)
{
	int ret = 0, i = 0;

	ret = bcl_monitor_disable();
	if (ret)
		pr_err("Error disabling BCL. err:%d\n", ret);

	for (; i < BCL_PARAM_MAX; i++) {
		if (!bcl_perph->param[i].param_data)
			continue;

		ret = msm_bcl_unregister_param(bcl_perph->param[i].param_data);
		if (ret)
			pr_err("Error unregistering with Framework. err:%d\n",
					ret);
	}

	return 0;
}

static struct of_device_id bcl_match[] = {
	{	.compatible	= "qcom,msm-bcl",
		.data		= (void *) BCL_PMI8994,
	},
	{	.compatible	= "qcom,msm-bcl-lmh",
		.data		= (void *) BCL_PMI8998,
	},
	{},
};

static struct spmi_driver bcl_driver = {
	.probe = bcl_probe,
	.remove = bcl_remove,
	.driver = {
		.name = BCL_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = bcl_match,
	},
};

static int __init bcl_perph_init(void)
{
	struct device_node *comp_node;

	comp_node = of_find_matching_node(NULL, bcl_match);
	bcl_perph_version = BCL_PMI8994;
	if (comp_node) {
		const struct of_device_id *match = of_match_node(bcl_match,
							comp_node);
		if (!match) {
			pr_err("Couldnt find a match\n");
			goto plt_register;
		}
		bcl_perph_version = (enum bcl_hw_type)match->data;
		of_node_put(comp_node);
	}

plt_register:
	return spmi_driver_register(&bcl_driver);
}

static void __exit bcl_perph_exit(void)
{
	spmi_driver_unregister(&bcl_driver);
}
fs_initcall(bcl_perph_init);
module_exit(bcl_perph_exit);
MODULE_ALIAS("platform:" BCL_DRIVER_NAME);

