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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/hwmon.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/spmi.h>
#include <linux/of_irq.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/hwmon-sysfs.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/platform_device.h>

/* QPNP IADC register definition */
#define QPNP_IADC_REVISION1				0x0
#define QPNP_IADC_REVISION2				0x1
#define QPNP_IADC_REVISION3				0x2
#define QPNP_IADC_REVISION4				0x3
#define QPNP_IADC_PERPH_TYPE				0x4
#define QPNP_IADC_PERH_SUBTYPE				0x5

#define QPNP_IADC_SUPPORTED_REVISION2			1

#define QPNP_STATUS1					0x8
#define QPNP_STATUS1_OP_MODE				4
#define QPNP_STATUS1_MULTI_MEAS_EN			BIT(3)
#define QPNP_STATUS1_MEAS_INTERVAL_EN_STS		BIT(2)
#define QPNP_STATUS1_REQ_STS				BIT(1)
#define QPNP_STATUS1_EOC				BIT(0)
#define QPNP_STATUS2					0x9
#define QPNP_STATUS2_CONV_SEQ_STATE_SHIFT		4
#define QPNP_STATUS2_FIFO_NOT_EMPTY_FLAG		BIT(1)
#define QPNP_STATUS2_CONV_SEQ_TIMEOUT_STS		BIT(0)
#define QPNP_CONV_TIMEOUT_ERR				2

#define QPNP_INT_RT_ST					0x10
#define QPNP_INT_SET_TYPE				0x11
#define QPNP_INT_SET_TYPE_LOW_THR_INT_SET		BIT(4)
#define QPNP_INT_SET_TYPE_HIGH_THR_INT_SET		BIT(3)
#define QPNP_INT_SET_TYPE_CONV_SEQ_TIMEOUT_INT_SET	BIT(2)
#define QPNP_INT_SET_TYPE_FIFO_NOT_EMPTY_INT_SET	BIT(1)
#define QPNP_INT_SET_TYPE_EOC_SET_INT_TYPE		BIT(0)
#define QPNP_INT_POLARITY_HIGH				0x12
#define QPNP_INT_POLARITY_LOW				0x13
#define QPNP_INT_EN_SET					0x15
#define QPNP_INT_EN_SET_LOW_THR_INT_EN_SET		BIT(4)
#define QPNP_INT_EN_SET_HIGH_THR_INT_EN_SET		BIT(3)
#define QPNP_INT_EN_SET_CONV_SEQ_TIMEOUT_INT_EN		BIT(2)
#define QPNP_INT_EN_SET_FIFO_NOT_EMPTY_INT_EN		BIT(1)
#define QPNP_INT_EN_SET_EOC_INT_EN_SET			BIT(0)
#define QPNP_INT_CLR					0x16
#define QPNP_INT_CLR_LOW_THR_INT_EN_CLR			BIT(4)
#define QPNP_INT_CLR_HIGH_THR_INT_EN_CLKR		BIT(3)
#define QPNP_INT_CLR_CONV_SEQ_TIMEOUT_INT_EN		BIT(2)
#define QPNP_INT_CLR_FIFO_NOT_EMPTY_INT_EN		BIT(1)
#define QPNP_INT_CLR_EOC_INT_EN_CLR			BIT(0)
#define QPNP_INT_CLR_MASK				0x1f
#define QPNP_IADC_MODE_CTL				0x40
#define QPNP_OP_MODE_SHIFT				4
#define QPNP_USE_BMS_DATA				BIT(4)
#define QPNP_VADC_SYNCH_EN				BIT(2)
#define QPNP_OFFSET_RMV_EN				BIT(1)
#define QPNP_ADC_TRIM_EN				BIT(0)
#define QPNP_IADC_EN_CTL1				0x46
#define QPNP_IADC_ADC_EN				BIT(7)
#define QPNP_ADC_CH_SEL_CTL				0x48
#define QPNP_ADC_DIG_PARAM				0x50
#define QPNP_ADC_CLK_SEL_MASK				0x3
#define QPNP_ADC_DEC_RATIO_SEL_MASK			0xc
#define QPNP_ADC_DIG_DEC_RATIO_SEL_SHIFT		2

#define QPNP_HW_SETTLE_DELAY				0x51
#define QPNP_CONV_REQ					0x52
#define QPNP_CONV_REQ_SET				BIT(7)
#define QPNP_CONV_SEQ_CTL				0x54
#define QPNP_CONV_SEQ_HOLDOFF_SHIFT			4
#define QPNP_CONV_SEQ_TRIG_CTL				0x55
#define QPNP_FAST_AVG_CTL				0x5a

#define QPNP_M0_LOW_THR_LSB				0x5c
#define QPNP_M0_LOW_THR_MSB				0x5d
#define QPNP_M0_HIGH_THR_LSB				0x5e
#define QPNP_M0_HIGH_THR_MSB				0x5f
#define QPNP_M1_LOW_THR_LSB				0x69
#define QPNP_M1_LOW_THR_MSB				0x6a
#define QPNP_M1_HIGH_THR_LSB				0x6b
#define QPNP_M1_HIGH_THR_MSB				0x6c

#define QPNP_DATA0					0x60
#define QPNP_DATA1					0x61
#define QPNP_CONV_TIMEOUT_ERR				2

#define QPNP_IADC_ADC_CH_SEL_CTL			0x48
#define QPNP_IADC_ADC_CHX_SEL_SHIFT			3

#define QPNP_IADC_ADC_DIG_PARAM				0x50
#define QPNP_IADC_CLK_SEL_SHIFT				1
#define QPNP_IADC_DEC_RATIO_SEL				3

#define QPNP_IADC_CONV_REQUEST				0x52
#define QPNP_IADC_CONV_REQ				BIT(7)

#define QPNP_IADC_DATA0					0x60
#define QPNP_IADC_DATA1					0x61

#define QPNP_ADC_CONV_TIME_MIN				8000
#define QPNP_ADC_CONV_TIME_MAX				8200

#define QPNP_ADC_GAIN_CALCULATION_UV			17857
#define QPNP_IADC_RSENSE_MILLI_FACTOR			1000

struct qpnp_iadc_drv {
	struct qpnp_adc_drv		*adc;
	int32_t				rsense;
	struct device			*iadc_hwmon;
	bool				iadc_init_calib;
	bool				iadc_initialized;
	struct sensor_device_attribute		sens_attr[0];
};

struct qpnp_iadc_drv	*qpnp_iadc;

static int32_t qpnp_iadc_read_reg(uint32_t reg, u8 *data)
{
	struct qpnp_iadc_drv *iadc = qpnp_iadc;
	int rc;

	rc = spmi_ext_register_readl(iadc->adc->spmi->ctrl, iadc->adc->slave,
		(iadc->adc->offset + reg), data, 1);
	if (rc < 0) {
		pr_err("qpnp iadc read reg %d failed with %d\n", reg, rc);
		return rc;
	}

	return 0;
}

static int32_t qpnp_iadc_write_reg(uint32_t reg, u8 data)
{
	struct qpnp_iadc_drv *iadc = qpnp_iadc;
	int rc;
	u8 *buf;

	buf = &data;
	rc = spmi_ext_register_writel(iadc->adc->spmi->ctrl, iadc->adc->slave,
		(iadc->adc->offset + reg), buf, 1);
	if (rc < 0) {
		pr_err("qpnp iadc write reg %d failed with %d\n", reg, rc);
		return rc;
	}

	return 0;
}

static int32_t qpnp_iadc_configure_interrupt(void)
{
	int rc = 0;
	u8 data = 0;

	/* Configure interrupt as an Edge trigger */
	rc = qpnp_iadc_write_reg(QPNP_INT_SET_TYPE,
					QPNP_INT_CLR_MASK);
	if (rc < 0) {
		pr_err("%s Interrupt configure failed\n", __func__);
		return rc;
	}

	/* Configure interrupt for rising edge trigger */
	rc = qpnp_iadc_write_reg(QPNP_INT_POLARITY_HIGH,
					QPNP_INT_CLR_MASK);
	if (rc < 0) {
		pr_err("%s Rising edge trigger configure failed\n", __func__);
		return rc;
	}

	/* Disable low level interrupt triggering */
	data = QPNP_INT_CLR_MASK;
	rc = qpnp_iadc_write_reg(QPNP_INT_POLARITY_LOW,
					(~data & QPNP_INT_CLR_MASK));
	if (rc < 0) {
		pr_err("%s Setting level low to disable failed\n", __func__);
		return rc;
	}

	return 0;
}

static void trigger_iadc_completion(struct work_struct *work)
{
	struct qpnp_iadc_drv *iadc = qpnp_iadc;
	int rc;

	rc = qpnp_iadc_write_reg(QPNP_INT_CLR, QPNP_INT_CLR_MASK);
	if (rc < 0)
		pr_err("qpnp iadc interrupt mask failed with %d\n", rc);

	complete(&iadc->adc->adc_rslt_completion);

	return;
}
DECLARE_WORK(trigger_iadc_completion_work, trigger_iadc_completion);

static irqreturn_t qpnp_iadc_isr(int irq, void *dev_id)
{
	schedule_work(&trigger_iadc_completion_work);

	return IRQ_HANDLED;
}

static int32_t qpnp_iadc_enable(bool state)
{
	int rc = 0;
	u8 data = 0;

	data = QPNP_IADC_ADC_EN;
	if (state) {
		rc = qpnp_iadc_write_reg(QPNP_IADC_EN_CTL1,
					data);
		if (rc < 0) {
			pr_err("IADC enable failed\n");
			return rc;
		}
	} else {
		rc = qpnp_iadc_write_reg(QPNP_IADC_EN_CTL1,
					(~data & QPNP_IADC_ADC_EN));
		if (rc < 0) {
			pr_err("IADC disable failed\n");
			return rc;
		}
	}

	return 0;
}

static int32_t qpnp_iadc_read_conversion_result(int32_t *data)
{
	uint8_t rslt_lsb, rslt_msb;
	int32_t rc;

	rc = qpnp_iadc_read_reg(QPNP_IADC_DATA0, &rslt_lsb);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_read_reg(QPNP_IADC_DATA1, &rslt_msb);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		return rc;
	}

	*data = (rslt_msb << 8) | rslt_lsb;

	rc = qpnp_iadc_enable(false);
	if (rc)
		return rc;
	return 0;
}

static int32_t qpnp_iadc_configure(enum qpnp_iadc_channels channel,
						int32_t *result)
{
	struct qpnp_iadc_drv *iadc = qpnp_iadc;
	u8 qpnp_iadc_mode_reg = 0, qpnp_iadc_ch_sel_reg = 0;
	u8 qpnp_iadc_conv_req = 0, qpnp_iadc_dig_param_reg = 0;
	int32_t rc = 0;

	qpnp_iadc_ch_sel_reg = channel;

	qpnp_iadc_dig_param_reg |= iadc->adc->amux_prop->decimation <<
					QPNP_IADC_DEC_RATIO_SEL;

	qpnp_iadc_conv_req = QPNP_IADC_CONV_REQ;

	rc = qpnp_iadc_write_reg(QPNP_INT_EN_SET,
					QPNP_INT_EN_SET_EOC_INT_EN_SET);
	if (rc < 0) {
		pr_err("qpnp adc configure error for interrupt setup\n");
		return rc;
	}

	rc = qpnp_iadc_write_reg(QPNP_IADC_MODE_CTL, qpnp_iadc_mode_reg);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_write_reg(QPNP_IADC_ADC_CH_SEL_CTL,
						qpnp_iadc_ch_sel_reg);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_write_reg(QPNP_ADC_DIG_PARAM,
						qpnp_iadc_dig_param_reg);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		return rc;
	}

	rc = qpnp_iadc_write_reg(QPNP_HW_SETTLE_DELAY,
				iadc->adc->amux_prop->hw_settle_time);
	if (rc < 0) {
		pr_err("qpnp adc configure error for hw settling time setup\n");
		return rc;
	}

	rc = qpnp_iadc_write_reg(QPNP_FAST_AVG_CTL,
					iadc->adc->amux_prop->fast_avg_setup);
	if (rc < 0) {
		pr_err("qpnp adc fast averaging configure error\n");
		return rc;
	}

	rc = qpnp_iadc_enable(true);
	if (rc)
		return rc;

	rc = qpnp_iadc_write_reg(QPNP_CONV_REQ, qpnp_iadc_conv_req);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		return rc;
	}

	wait_for_completion(&iadc->adc->adc_rslt_completion);

	rc = qpnp_iadc_read_conversion_result(result);
	if (rc) {
		pr_err("qpnp adc read adc failed with %d\n", rc);
		return rc;
	}

	return 0;
}

static int32_t qpnp_iadc_init_calib(void)
{
	struct qpnp_iadc_drv *iadc = qpnp_iadc;
	int32_t rc = 0, result;

	rc = qpnp_iadc_configure(GAIN_CALIBRATION_25MV, &result);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		goto fail;
	}

	iadc->adc->calib.gain = result;

	rc = qpnp_iadc_configure(OFFSET_CALIBRATION_SHORT_CADC_LEADS,
								&result);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		goto fail;
	}

	iadc->adc->calib.offset = result;

fail:
	return rc;
}

static int32_t qpnp_iadc_version_check(void)
{
	uint8_t revision;
	int rc;

	rc = qpnp_iadc_read_reg(QPNP_IADC_REVISION2, &revision);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		return rc;
	}

	if (revision < QPNP_IADC_SUPPORTED_REVISION2) {
		pr_err("IADC Version not supported\n");
		return -EINVAL;
	}

	return 0;
}

int32_t qpnp_iadc_is_ready(void)
{
	struct qpnp_iadc_drv *iadc = qpnp_iadc;

	if (!iadc || !iadc->iadc_initialized)
		return -EPROBE_DEFER;
	else
		return 0;
}
EXPORT_SYMBOL(qpnp_iadc_is_ready);

int32_t qpnp_iadc_read(enum qpnp_iadc_channels channel,
						int32_t *result)
{
	struct qpnp_iadc_drv *iadc = qpnp_iadc;
	int32_t vsense_mv = 0, rc;

	if (!iadc || !iadc->iadc_initialized)
		return -EPROBE_DEFER;

	mutex_lock(&iadc->adc->adc_lock);

	if (!iadc->iadc_init_calib) {
		rc = qpnp_iadc_version_check();
		if (rc)
			goto fail;
		rc = qpnp_iadc_init_calib();
		if (rc) {
			pr_err("Calibration failed\n");
			goto fail;
		} else
			iadc->iadc_init_calib = true;
	}

	rc = qpnp_iadc_configure(channel, result);
	if (rc < 0) {
		pr_err("qpnp adc result read failed with %d\n", rc);
		goto fail;
	}

	*result = ((vsense_mv - iadc->adc->calib.offset) *
				QPNP_ADC_GAIN_CALCULATION_UV)/
			(iadc->adc->calib.gain - iadc->adc->calib.offset);

	*result = (*result / (qpnp_iadc->rsense));
fail:
	mutex_unlock(&iadc->adc->adc_lock);

	return rc;
}
EXPORT_SYMBOL(qpnp_iadc_read);

int32_t qpnp_iadc_get_gain(int32_t *result)
{
	return qpnp_iadc_read(GAIN_CALIBRATION_25MV, result);
}
EXPORT_SYMBOL(qpnp_iadc_get_gain);

int32_t qpnp_iadc_get_offset(enum qpnp_iadc_channels channel,
						int32_t *result)
{
	return qpnp_iadc_read(channel, result);
}
EXPORT_SYMBOL(qpnp_iadc_get_offset);

static ssize_t qpnp_iadc_show(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	int32_t result;
	int rc = -1;

	rc = qpnp_iadc_read(attr->index, &result);

	if (rc)
		return 0;

	return snprintf(buf, QPNP_ADC_HWMON_NAME_LENGTH,
					"Result:%d\n", result);
}

static struct sensor_device_attribute qpnp_adc_attr =
	SENSOR_ATTR(NULL, S_IRUGO, qpnp_iadc_show, NULL, 0);

static int32_t qpnp_iadc_init_hwmon(struct spmi_device *spmi)
{
	struct qpnp_iadc_drv *iadc = qpnp_iadc;
	struct device_node *child;
	struct device_node *node = spmi->dev.of_node;
	int rc = 0, i = 0, channel;

	for_each_child_of_node(node, child) {
		channel = iadc->adc->adc_channels[i].channel_num;
		qpnp_adc_attr.index = iadc->adc->adc_channels[i].channel_num;
		qpnp_adc_attr.dev_attr.attr.name =
						iadc->adc->adc_channels[i].name;
		sysfs_attr_init(&iadc->sens_attr[i].dev_attr.attr);
		memcpy(&iadc->sens_attr[i], &qpnp_adc_attr,
						sizeof(qpnp_adc_attr));
		rc = device_create_file(&spmi->dev,
				&iadc->sens_attr[i].dev_attr);
		if (rc) {
			dev_err(&spmi->dev,
				"device_create_file failed for dev %s\n",
				iadc->adc->adc_channels[i].name);
			goto hwmon_err_sens;
		}
		i++;
	}

	return 0;
hwmon_err_sens:
	pr_err("Init HWMON failed for qpnp_iadc with %d\n", rc);
	return rc;
}

static int __devinit qpnp_iadc_probe(struct spmi_device *spmi)
{
	struct qpnp_iadc_drv *iadc;
	struct qpnp_adc_drv *adc_qpnp;
	struct device_node *node = spmi->dev.of_node;
	struct device_node *child;
	int rc, count_adc_channel_list = 0;

	if (!node)
		return -EINVAL;

	if (qpnp_iadc) {
		pr_err("IADC already in use\n");
		return -EBUSY;
	}

	for_each_child_of_node(node, child)
		count_adc_channel_list++;

	if (!count_adc_channel_list) {
		pr_err("No channel listing\n");
		return -EINVAL;
	}

	iadc = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_iadc_drv) +
		(sizeof(struct sensor_device_attribute) *
				count_adc_channel_list), GFP_KERNEL);
	if (!iadc) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	adc_qpnp = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_adc_drv),
			GFP_KERNEL);
	if (!adc_qpnp) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	iadc->adc = adc_qpnp;

	rc = qpnp_adc_get_devicetree_data(spmi, iadc->adc);
	if (rc) {
		dev_err(&spmi->dev, "failed to read device tree\n");
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,rsense",
			&iadc->rsense);
	if (rc) {
		pr_err("Invalid rsens reference property\n");
		return -EINVAL;
	}

	rc = devm_request_irq(&spmi->dev, iadc->adc->adc_irq,
				qpnp_iadc_isr,
	IRQF_TRIGGER_RISING, "qpnp_iadc_interrupt", iadc);
	if (rc) {
		dev_err(&spmi->dev, "failed to request adc irq\n");
		return rc;
	} else
		enable_irq_wake(iadc->adc->adc_irq);

	iadc->iadc_init_calib = false;
	dev_set_drvdata(&spmi->dev, iadc);
	qpnp_iadc = iadc;

	rc = qpnp_iadc_init_hwmon(spmi);
	if (rc) {
		dev_err(&spmi->dev, "failed to initialize qpnp hwmon adc\n");
		return rc;
	}
	iadc->iadc_hwmon = hwmon_device_register(&iadc->adc->spmi->dev);

	rc = qpnp_iadc_configure_interrupt();
	if (rc) {
		dev_err(&spmi->dev, "failed to configure interrupt");
		return rc;
	}

	return 0;
}

static int __devexit qpnp_iadc_remove(struct spmi_device *spmi)
{
	struct qpnp_iadc_drv *iadc = dev_get_drvdata(&spmi->dev);
	struct device_node *node = spmi->dev.of_node;
	struct device_node *child;
	int i = 0;

	for_each_child_of_node(node, child) {
		device_remove_file(&spmi->dev,
			&iadc->sens_attr[i].dev_attr);
		i++;
	}
	dev_set_drvdata(&spmi->dev, NULL);

	return 0;
}

static const struct of_device_id qpnp_iadc_match_table[] = {
	{	.compatible = "qcom,qpnp-iadc",
	},
	{}
};

static struct spmi_driver qpnp_iadc_driver = {
	.driver		= {
		.name	= "qcom,qpnp-iadc",
		.of_match_table = qpnp_iadc_match_table,
	},
	.probe		= qpnp_iadc_probe,
	.remove		= qpnp_iadc_remove,
};

static int __init qpnp_iadc_init(void)
{
	return spmi_driver_register(&qpnp_iadc_driver);
}
module_init(qpnp_iadc_init);

static void __exit qpnp_iadc_exit(void)
{
	spmi_driver_unregister(&qpnp_iadc_driver);
}
module_exit(qpnp_iadc_exit);

MODULE_DESCRIPTION("QPNP PMIC current ADC driver");
MODULE_LICENSE("GPL v2");
