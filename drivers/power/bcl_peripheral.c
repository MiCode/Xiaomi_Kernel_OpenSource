/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/spmi.h>
#include <linux/mutex.h>
#include <linux/msm_bcl.h>

#define BCL_DRIVER_NAME         "bcl_peripheral"
#define BCL_VBAT_INT_NAME       "bcl-low-vbat-int"
#define BCL_IBAT_INT_NAME       "bcl-high-ibat-int"
#define BCL_PARAM_MAX_ATTR      3

#define BCL_INT_EN              0x15
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

#define READ_CONV_FACTOR(_node, _key, _val, _ret, _dest) do { \
		_ret = of_property_read_u32(_node, _key, &_val); \
		if (_ret) { \
			pr_err("Error reading key:%s. err:%d\n", _key, _ret); \
			goto bcl_dev_exit; \
		} \
		_dest = _val; \
	} while (0)

enum bcl_monitor_state {
	BCL_PARAM_INACTIVE,
	BCL_PARAM_MONITOR,
	BCL_PARAM_TRIPPED,
	BCL_PARAM_POLLING,
};

struct bcl_peripheral_data {
	struct bcl_param_data   *param_data;
	struct bcl_driver_ops   ops;
	enum bcl_monitor_state  state;
	struct work_struct      isr_work;
	struct delayed_work     poll_work;
	int                     irq_num;
	int                     high_trip;
	int                     low_trip;
	int                     scaling_factor;
	int                     offset_factor_num;
	int                     offset_factor_den;
	int                     gain_factor_num;
	int                     gain_factor_den;
	uint32_t                polling_delay_ms;
	int (*read_max)         (int *adc_value);
	int (*clear_max)        (void);
	int (*disable_interrupt)(void);
	int (*enable_interrupt) (void);
};

struct bcl_device {
	bool                    enabled;
	struct device           *dev;
	struct spmi_device      *spmi;
	uint16_t                base_addr;
	uint8_t                 slave_id;
	struct workqueue_struct *bcl_isr_wq;
	struct bcl_peripheral_data   param[BCL_PARAM_MAX];
};

static struct bcl_device *bcl_perph;
static DEFINE_MUTEX(bcl_access_mutex);
static DEFINE_MUTEX(bcl_enable_mutex);

static int bcl_read_register(int16_t reg_offset, uint8_t *data)
{
	int  ret = 0;

	if (!bcl_perph) {
		pr_err("BCL device not initialized\n");
		return -EINVAL;
	}
	ret = spmi_ext_register_readl(bcl_perph->spmi->ctrl,
		bcl_perph->slave_id, (bcl_perph->base_addr + reg_offset),
		data, 1);
	if (ret < 0) {
		pr_err("Error reading register %d. err:%d", reg_offset, ret);
		return ret;
	}

	return ret;
}

static int bcl_write_register(int16_t reg_offset, uint8_t data)
{
	int  ret = 0;
	uint8_t *write_buf = &data;

	if (!bcl_perph) {
		pr_err("BCL device not initialized\n");
		return -EINVAL;
	}
	ret = spmi_ext_register_writel(bcl_perph->spmi->ctrl,
		bcl_perph->slave_id, (bcl_perph->base_addr + reg_offset),
		write_buf, 1);
	if (ret < 0) {
		pr_err("Error reading register %d. err:%d", reg_offset, ret);
		return ret;
	}

	return ret;
}

static void convert_vbat_to_adc_val(int *val)
{
	struct bcl_peripheral_data *perph_data = NULL;

	if (!bcl_perph)
		return;
	perph_data = &bcl_perph->param[BCL_PARAM_VOLTAGE];
	*val = *val / perph_data->scaling_factor;
	return;
}

static void convert_adc_to_vbat_val(int *val)
{
	struct bcl_peripheral_data *perph_data = NULL;

	if (!bcl_perph)
		return;
	perph_data = &bcl_perph->param[BCL_PARAM_VOLTAGE];
	*val = (*val * perph_data->scaling_factor)
		* (1 + perph_data->gain_factor_num
		/ perph_data->gain_factor_den);
	return;
}

static void convert_ibat_to_adc_val(int *val)
{
	struct bcl_peripheral_data *perph_data = NULL;

	if (!bcl_perph)
		return;
	perph_data = &bcl_perph->param[BCL_PARAM_CURRENT];
	*val /= perph_data->scaling_factor;
	return;
}

static void convert_adc_to_ibat_val(int *val)
{
	struct bcl_peripheral_data *perph_data = NULL;

	if (!bcl_perph)
		return;
	perph_data = &bcl_perph->param[BCL_PARAM_CURRENT];
	*val = (*val * perph_data->scaling_factor +
		perph_data->offset_factor_num / perph_data->offset_factor_den)
		* (1 + perph_data->gain_factor_num
		/ perph_data->gain_factor_den);
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
	ret = bcl_write_register(BCL_IBAT_TRIP, val);
	if (ret) {
		pr_err("Error accessing BCL peripheral. err:%d\n", ret);
		return ret;
	}
	bcl_perph->param[BCL_PARAM_CURRENT].high_trip = thresh_value;

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
	ret = bcl_write_register(BCL_VBAT_TRIP, val);
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
	int8_t val = 0;
	struct bcl_peripheral_data *perph_data = NULL;

	mutex_lock(&bcl_enable_mutex);
	if (enable == bcl_perph->enabled)
		goto access_exit;

	for (; i < BCL_PARAM_MAX; i++) {
		perph_data = &bcl_perph->param[i];
		if (enable) {
			ret = perph_data->enable_interrupt();
			if (ret) {
				pr_err("Error enabling itrpt. param:%d err%d\n"
					, i, ret);
				goto access_exit;
			}
			perph_data->state = BCL_PARAM_MONITOR;
		} else {
			ret = perph_data->disable_interrupt();
			if (ret) {
				pr_err("Error disabling itrpt. param:%d err%d\n"
					, i, ret);
				goto access_exit;
			}
			cancel_delayed_work_sync(&perph_data->poll_work);
			cancel_work_sync(&perph_data->isr_work);
			perph_data->state = BCL_PARAM_INACTIVE;
		}
	}
	val = (enable) ? BIT(7) : 0;
	ret = bcl_write_register(BCL_MONITOR_EN, val);
	if (ret) {
		pr_err("Error accessing BCL peripheral. err:%d\n", ret);
		goto access_exit;
	}
	bcl_perph->enabled = enable;

access_exit:
	mutex_unlock(&bcl_enable_mutex);
	return ret;
}

static int bcl_monitor_enable(void)
{
	return bcl_access_monitor_enable(true);
}

static int bcl_monitor_disable(void)
{
	return bcl_access_monitor_enable(false);
}

static int bcl_read_ibat_high_trip(int *thresh_value)
{
	int ret = 0;
	int8_t val = 0;

	*thresh_value = (int)val;
	ret = bcl_read_register(BCL_IBAT_TRIP, &val);
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
	ret = bcl_read_register(BCL_VBAT_TRIP, &val);
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

	ret = bcl_write_register(BCL_VBAT_MIN, BIT(7));
	if (ret)
		pr_err("Error in clearing vbat min reg. err:%d", ret);

	return ret;
}

static int bcl_clear_ibat_max(void)
{
	int ret  = 0;

	ret = bcl_write_register(BCL_IBAT_MAX, BIT(7));
	if (ret)
		pr_err("Error in clearing ibat max reg. err:%d", ret);

	return ret;
}

static int bcl_read_ibat_max(int *adc_value)
{
	int ret = 0;
	int8_t val = 0;

	*adc_value = (int)val;
	ret = bcl_read_register(BCL_IBAT_MAX, &val);
	if (ret) {
		pr_err("BCL register read error. err:%d\n", ret);
		goto bcl_read_exit;
	}
	*adc_value = (int)val;
	convert_adc_to_ibat_val(adc_value);
	pr_debug("Ibat Max:%d. ADC_val:%d\n", *adc_value, val);

bcl_read_exit:
	return ret;
}

static int bcl_read_vbat_min(int *adc_value)
{
	int ret = 0;
	int8_t val = 0;

	*adc_value = (int)val;
	ret = bcl_read_register(BCL_VBAT_MIN, &val);
	if (ret) {
		pr_err("BCL register read error. err:%d\n", ret);
		goto bcl_read_exit;
	}
	*adc_value = (int)val;
	convert_adc_to_vbat_val(adc_value);
	pr_debug("Vbat Min:%d. ADC_val:%d\n", *adc_value, val);

bcl_read_exit:
	return ret;
}

static int bcl_read_ibat(int *adc_value)
{
	int ret = 0;
	int8_t val = 0;

	*adc_value = (int)val;
	ret = bcl_read_register(BCL_IBAT_VALUE, &val);
	if (ret) {
		pr_err("BCL register read error. err:%d\n", ret);
		goto bcl_read_exit;
	}
	*adc_value = (int)val;
	convert_adc_to_ibat_val(adc_value);
	pr_debug("Read Ibat:%d. ADC_val:%d\n", *adc_value, val);

bcl_read_exit:
	return ret;
}

static int bcl_read_vbat(int *adc_value)
{
	int ret = 0;
	int8_t val = 0;

	*adc_value = (int)val;
	ret = bcl_read_register(BCL_VBAT_VALUE, &val);
	if (ret) {
		pr_err("BCL register read error. err:%d\n", ret);
		goto bcl_read_exit;
	}
	*adc_value = (int)val;
	convert_adc_to_vbat_val(adc_value);
	pr_debug("Read Vbat:%d. ADC_val:%d\n", *adc_value, val);

bcl_read_exit:
	return ret;
}

static int bcl_param_enable(enum bcl_monitor_state param, bool enable)
{
	int ret = 0;
	int8_t val = 0;

	ret = bcl_read_register(BCL_INT_EN, &val);
	if (ret) {
		pr_err("Error reading interrupt enable register. err:%d", ret);
		return ret;
	}
	if (enable) {
		if (val & (BIT(param)))
			return 0;
		val |= BIT(param);
	} else {
		if (!(val & BIT(param)))
			return 0;
		val ^= BIT(param);
	}
	ret = bcl_write_register(BCL_INT_EN, val);
	if (ret) {
		pr_err("Error writing interrupt enable register. err:%d", ret);
		return ret;
	}

	return ret;
}

static int bcl_ibat_disable(void)
{
	int ret = 0;

	mutex_lock(&bcl_access_mutex);
	ret = bcl_param_enable(BCL_PARAM_CURRENT, false);
	if (ret) {
		pr_err("Error disabling ibat. err:%d", ret);
		goto ibat_disable_exit;
	}

ibat_disable_exit:
	mutex_unlock(&bcl_access_mutex);
	return ret;
}

static int bcl_ibat_enable(void)
{
	int ret = 0;

	mutex_lock(&bcl_access_mutex);
	ret = bcl_param_enable(BCL_PARAM_CURRENT, true);
	if (ret) {
		pr_err("Error enabling ibat. err:%d", ret);
		goto ibat_enable_exit;
	}

ibat_enable_exit:
	mutex_unlock(&bcl_access_mutex);
	return ret;
}

static int bcl_vbat_disable(void)
{
	int ret = 0;

	mutex_lock(&bcl_access_mutex);
	ret = bcl_param_enable(BCL_PARAM_VOLTAGE, false);
	if (ret) {
		pr_err("Error disabling vbat. err:%d", ret);
		goto vbat_disable_exit;
	}

vbat_disable_exit:
	mutex_unlock(&bcl_access_mutex);
	return ret;
}

static int bcl_vbat_enable(void)
{
	int ret = 0;

	mutex_lock(&bcl_access_mutex);
	ret = bcl_param_enable(BCL_PARAM_VOLTAGE, true);
	if (ret) {
		pr_err("Error enabling vbat. err:%d", ret);
		goto vbat_enable_exit;
	}

vbat_enable_exit:
	mutex_unlock(&bcl_access_mutex);
	return ret;
}

static void bcl_poll_ibat_low(struct work_struct *work)
{
	int ret = 0, val = 0;
	struct bcl_peripheral_data *perph_data =
		&bcl_perph->param[BCL_PARAM_CURRENT];

	if (perph_data->state != BCL_PARAM_POLLING) {
		pr_err("Invalid ibat state %d\n", perph_data->state);
		return;
	}

	ret = perph_data->ops.read(&val);
	if (ret) {
		pr_err("Error in reading ibat. err:%d", ret);
		goto reschedule_ibat;
	}
	if (val <= perph_data->low_trip) {
		pr_debug("Ibat reached low clear trip. ibat:%d\n", val);
		perph_data->ops.notify(perph_data->param_data, val,
			BCL_LOW_TRIP);
		perph_data->state = BCL_PARAM_MONITOR;
		perph_data->enable_interrupt();
	} else {
		goto reschedule_ibat;
	}
	return;

reschedule_ibat:
	schedule_delayed_work(&perph_data->poll_work,
		msecs_to_jiffies(perph_data->polling_delay_ms));
	return;
}

static void bcl_poll_vbat_high(struct work_struct *work)
{
	int ret = 0, val = 0;
	struct bcl_peripheral_data *perph_data =
		&bcl_perph->param[BCL_PARAM_VOLTAGE];

	if (perph_data->state != BCL_PARAM_POLLING) {
		pr_err("Invalid vbat state %d\n", perph_data->state);
		return;
	}

	ret = perph_data->ops.read(&val);
	if (ret) {
		pr_err("Error in reading vbat. err:%d", ret);
		goto reschedule_vbat;
	}
	if (val >= perph_data->high_trip) {
		pr_debug("Vbat reached high clear trip. vbat:%d\n", val);
		perph_data->ops.notify(perph_data->param_data, val,
			BCL_HIGH_TRIP);
		perph_data->state = BCL_PARAM_MONITOR;
		perph_data->enable_interrupt();
	} else {
		goto reschedule_vbat;
	}
	return;

reschedule_vbat:
	schedule_delayed_work(&perph_data->poll_work,
		msecs_to_jiffies(perph_data->polling_delay_ms));
	return;
}

static void bcl_handle_ibat(struct work_struct *work)
{
	int val = 0, ret = 0;
	struct bcl_peripheral_data *perph_data = container_of(work,
		struct bcl_peripheral_data, isr_work);

	if (perph_data->state != BCL_PARAM_TRIPPED) {
		pr_err("Invalid state %d\n", perph_data->state);
		return;
	}
	perph_data->disable_interrupt();
	perph_data->state = BCL_PARAM_POLLING;
	ret = perph_data->read_max(&val);
	if (ret)
		pr_err("Error reading max ibat reg. err:%d\n", ret);
	pr_debug("Ibat reached high trip. ibat:%d\n", val);
	perph_data->ops.notify(perph_data->param_data, val, BCL_HIGH_TRIP);
	schedule_delayed_work(&perph_data->poll_work,
		msecs_to_jiffies(perph_data->polling_delay_ms));
	ret = perph_data->clear_max();
	if (ret)
		pr_err("Error clearing max ibat reg. err:%d\n", ret);

	return;
}

static void bcl_handle_vbat(struct work_struct *work)
{
	int val = 0, ret = 0;
	struct bcl_peripheral_data *perph_data = container_of(work,
		struct bcl_peripheral_data, isr_work);

	if (perph_data->state != BCL_PARAM_TRIPPED) {
		pr_err("Invalid state %d\n", perph_data->state);
		return;
	}
	perph_data->disable_interrupt();
	perph_data->state = BCL_PARAM_POLLING;
	ret = perph_data->read_max(&val);
	if (ret)
		pr_err("Error reading min vbat reg. err:%d\n", ret);
	pr_debug("Vbat reached Low trip. vbat:%d\n", val);
	perph_data->ops.notify(perph_data->param_data, val, BCL_LOW_TRIP);
	schedule_delayed_work(&perph_data->poll_work,
		msecs_to_jiffies(perph_data->polling_delay_ms));
	ret = perph_data->clear_max();
	if (ret)
		pr_err("Error clearing min vbat reg. err:%d\n", ret);

	return;
}

static irqreturn_t bcl_handle_isr(int irq, void *data)
{
	struct bcl_peripheral_data *perph_data =
		(struct bcl_peripheral_data *)data;

	if (perph_data->state == BCL_PARAM_MONITOR) {
		perph_data->state = BCL_PARAM_TRIPPED;
		queue_work(bcl_perph->bcl_isr_wq, &perph_data->isr_work);
	}

	return IRQ_HANDLED;
}

static int bcl_get_devicetree_data(struct spmi_device *spmi)
{
	int ret = 0, irq_num = 0, temp_val = 0;
	struct resource *resource = NULL;
	int8_t i_src = 0, val = 0;
	char *key = NULL;
	struct device_node *dev_node = spmi->dev.of_node;

	/* Get SPMI peripheral address */
	resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!resource) {
		pr_err("No base address defined\n");
		return -EINVAL;
	}
	bcl_perph->slave_id = spmi->sid;
	bcl_perph->base_addr = resource->start;

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
	key = "qcom,vbat-polling-delay-ms";
	READ_CONV_FACTOR(dev_node, key, temp_val, ret,
		bcl_perph->param[BCL_PARAM_VOLTAGE].polling_delay_ms);
	key = "qcom,ibat-polling-delay-ms";
	READ_CONV_FACTOR(dev_node, key, temp_val, ret,
		bcl_perph->param[BCL_PARAM_CURRENT].polling_delay_ms);

	ret = bcl_read_register(BCL_I_SENSE_SRC, &i_src);
	if (ret) {
		pr_err("Error reading current sense reg. err:%d\n", ret);
		goto bcl_dev_exit;
	}
	ret = bcl_read_register((i_src & 0x01) ? BCL_I_GAIN_RSENSE
		: BCL_I_GAIN_BATFET, &val);
	if (ret) {
		pr_err("Error reading %s current gain. err:%d\n",
			(i_src & 0x01) ? "rsense" : "batfet", ret);
		goto bcl_dev_exit;
	}
	bcl_perph->param[BCL_PARAM_CURRENT].gain_factor_num *= val;
	ret = bcl_read_register((i_src & 0x01) ? BCL_I_GAIN_RSENSE
		: BCL_I_GAIN_BATFET, &val);
	if (ret) {
		pr_err("Error reading %s current offset. err:%d\n",
			(i_src & 0x01) ? "rsense" : "batfet", ret);
		goto bcl_dev_exit;
	}
	bcl_perph->param[BCL_PARAM_CURRENT].offset_factor_num *= val;
	ret = bcl_read_register(BCL_V_GAIN_BAT, &val);
	if (ret) {
		pr_err("Error reading vbat offset. err:%d\n", ret);
		goto bcl_dev_exit;
	}
	bcl_perph->param[BCL_PARAM_VOLTAGE].gain_factor_num *= val;

bcl_dev_exit:
	return ret;
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
	bcl_perph->param[BCL_PARAM_VOLTAGE].disable_interrupt
		= bcl_vbat_disable;
	bcl_perph->param[BCL_PARAM_VOLTAGE].enable_interrupt
		 = bcl_vbat_enable;
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
	bcl_perph->param[BCL_PARAM_CURRENT].disable_interrupt
		= bcl_ibat_disable;
	bcl_perph->param[BCL_PARAM_CURRENT].enable_interrupt
		= bcl_ibat_enable;
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
	INIT_WORK(&bcl_perph->param[BCL_PARAM_VOLTAGE].isr_work,
		bcl_handle_vbat);
	INIT_DELAYED_WORK(&bcl_perph->param[BCL_PARAM_VOLTAGE].poll_work,
		bcl_poll_vbat_high);
	INIT_WORK(&bcl_perph->param[BCL_PARAM_CURRENT].isr_work,
		bcl_handle_ibat);
	INIT_DELAYED_WORK(&bcl_perph->param[BCL_PARAM_CURRENT].poll_work,
		bcl_poll_ibat_low);

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
	memset(bcl_perph, 0, sizeof(struct bcl_device));
	bcl_perph->spmi = spmi;
	bcl_perph->dev = &(spmi->dev);

	ret = bcl_get_devicetree_data(spmi);
	if (ret) {
		pr_err("Device tree data fetch error. err:%d", ret);
		goto bcl_probe_exit;
	}
	bcl_perph->bcl_isr_wq = alloc_workqueue("bcl_isr_wq", WQ_HIGHPRI, 0);
	if (!bcl_perph->bcl_isr_wq) {
		pr_err("Alloc work queue failed\n");
		ret = -ENOMEM;
		goto bcl_probe_exit;
	}
	ret = bcl_update_data();
	if (ret) {
		pr_err("Update data failed. err:%d", ret);
		goto bcl_probe_exit;
	}

	ret = devm_request_irq(&spmi->dev,
			bcl_perph->param[BCL_PARAM_VOLTAGE].irq_num,
			bcl_handle_isr, IRQF_TRIGGER_RISING,
			"bcl_vbat_interrupt",
			&bcl_perph->param[BCL_PARAM_VOLTAGE]);
	if (ret) {
		dev_err(&spmi->dev, "Error requesting VBAT irq. err:%d", ret);
		goto bcl_probe_exit;
	} else {
		enable_irq_wake(bcl_perph->param[BCL_PARAM_VOLTAGE].irq_num);
	}
	ret = devm_request_irq(&spmi->dev,
			bcl_perph->param[BCL_PARAM_CURRENT].irq_num,
			bcl_handle_isr, IRQF_TRIGGER_RISING,
			"bcl_ibat_interrupt",
			&bcl_perph->param[BCL_PARAM_CURRENT]);
	if (ret) {
		dev_err(&spmi->dev, "Error requesting IBAT irq. err:%d", ret);
		goto bcl_probe_exit;
	} else {
		enable_irq_wake(bcl_perph->param[BCL_PARAM_CURRENT].irq_num);
	}

	dev_set_drvdata(&spmi->dev, bcl_perph);
	/* BCL is enabled by default in hardware
	** Disable BCL polling till a valid threshold is set by APPS */
	bcl_perph->enabled = true;
	ret = bcl_monitor_disable();
	if (ret) {
		pr_err("Error disabling BCL. err:%d", ret);
		goto bcl_probe_exit;
	}

	return 0;

bcl_probe_exit:
	if (bcl_perph->bcl_isr_wq)
		destroy_workqueue(bcl_perph->bcl_isr_wq);
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
	if (bcl_perph->bcl_isr_wq)
		destroy_workqueue(bcl_perph->bcl_isr_wq);

	return 0;
}

static struct of_device_id bcl_match[] = {
	{	.compatible = "qcom,msm-bcl",
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
	pr_info("BCL Initialized\n");
	return spmi_driver_register(&bcl_driver);
}

static void __exit bcl_perph_exit(void)
{
	spmi_driver_unregister(&bcl_driver);
}
fs_initcall(bcl_perph_init);
module_exit(bcl_perph_exit);
MODULE_ALIAS("platform:" BCL_DRIVER_NAME);

