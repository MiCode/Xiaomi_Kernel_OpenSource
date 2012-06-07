/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/msm_adc.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pmic8058.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/delay.h>
#include <linux/wakelock.h>

#include <mach/mpp.h>
#include <mach/msm_xo.h>

#define ADC_DRIVER_NAME			"pm8058-xoadc"

#define MAX_QUEUE_LENGTH        0X15
#define MAX_CHANNEL_PROPERTIES_QUEUE    0X7
#define MAX_QUEUE_SLOT		0x1

/* User Processor */
#define ADC_ARB_USRP_CNTRL                      0x197
	#define ADC_ARB_USRP_CNTRL_EN_ARB	BIT(0)
	#define ADC_ARB_USRP_CNTRL_RSV1		BIT(1)
	#define ADC_ARB_USRP_CNTRL_RSV2		BIT(2)
	#define ADC_ARB_USRP_CNTRL_RSV3		BIT(3)
	#define ADC_ARB_USRP_CNTRL_RSV4		BIT(4)
	#define ADC_ARB_USRP_CNTRL_RSV5		BIT(5)
	#define ADC_ARB_USRP_CNTRL_EOC		BIT(6)
	#define ADC_ARB_USRP_CNTRL_REQ		BIT(7)

#define ADC_ARB_USRP_AMUX_CNTRL         0x198
#define ADC_ARB_USRP_ANA_PARAM          0x199
#define ADC_ARB_USRP_DIG_PARAM          0x19A
#define ADC_ARB_USRP_RSV                        0x19B

#define ADC_ARB_USRP_DATA0                      0x19D
#define ADC_ARB_USRP_DATA1                      0x19C

struct pmic8058_adc {
	struct device *dev;
	struct xoadc_platform_data *pdata;
	struct adc_properties *adc_prop;
	struct xoadc_conv_state	conv[2];
	int xoadc_queue_count;
	int adc_irq;
	struct linear_graph *adc_graph;
	struct xoadc_conv_state *conv_slot_request;
	struct xoadc_conv_state *conv_queue_list;
	struct adc_conv_slot conv_queue_elements[MAX_QUEUE_LENGTH];
	int xoadc_num;
	struct msm_xo_voter *adc_voter;
	struct wake_lock adc_wakelock;
	/* flag to warn/bug if wakelocks are taken after suspend_noirq */
	int msm_suspend_check;
};

static struct pmic8058_adc *pmic_adc[XOADC_PMIC_0 + 1];

static bool xoadc_initialized, xoadc_calib_first_adc;

DEFINE_RATELIMIT_STATE(pm8058_xoadc_msg_ratelimit,
		DEFAULT_RATELIMIT_INTERVAL, DEFAULT_RATELIMIT_BURST);

static inline int pm8058_xoadc_can_print(void)
{
	return __ratelimit(&pm8058_xoadc_msg_ratelimit);
}

int32_t pm8058_xoadc_registered(void)
{
	return xoadc_initialized;
}
EXPORT_SYMBOL(pm8058_xoadc_registered);

void pm8058_xoadc_restore_slot(uint32_t adc_instance,
					struct adc_conv_slot *slot)
{
	struct pmic8058_adc *adc_pmic = pmic_adc[adc_instance];
	struct xoadc_conv_state *slot_state = adc_pmic->conv_slot_request;

	mutex_lock(&slot_state->list_lock);
	list_add(&slot->list, &slot_state->slots);
	mutex_unlock(&slot_state->list_lock);
}
EXPORT_SYMBOL(pm8058_xoadc_restore_slot);

void pm8058_xoadc_slot_request(uint32_t adc_instance,
					struct adc_conv_slot **slot)
{
	struct pmic8058_adc *adc_pmic = pmic_adc[adc_instance];
	struct xoadc_conv_state *slot_state = adc_pmic->conv_slot_request;

	mutex_lock(&slot_state->list_lock);

	if (!list_empty(&slot_state->slots)) {
		*slot = list_first_entry(&slot_state->slots,
				struct adc_conv_slot, list);
		list_del(&(*slot)->list);
	} else
		*slot = NULL;

	mutex_unlock(&slot_state->list_lock);
}
EXPORT_SYMBOL(pm8058_xoadc_slot_request);

static int32_t pm8058_xoadc_arb_cntrl(uint32_t arb_cntrl,
				uint32_t adc_instance, uint32_t channel)
{
	struct pmic8058_adc *adc_pmic = pmic_adc[adc_instance];
	int i, rc;
	u8 data_arb_cntrl;

	data_arb_cntrl = ADC_ARB_USRP_CNTRL_EOC |
			ADC_ARB_USRP_CNTRL_RSV5 |
			ADC_ARB_USRP_CNTRL_RSV4;

	if (arb_cntrl) {
		if (adc_pmic->msm_suspend_check)
			pr_err("XOADC request being made after suspend irq "
				 "with channel id:%d\n", channel);
		data_arb_cntrl |= ADC_ARB_USRP_CNTRL_EN_ARB;
		msm_xo_mode_vote(adc_pmic->adc_voter, MSM_XO_MODE_ON);
		adc_pmic->pdata->xoadc_mpp_config();
		wake_lock(&adc_pmic->adc_wakelock);
	}

	/* Write twice to the CNTRL register for the arbiter settings
	   to take into effect */
	for (i = 0; i < 2; i++) {
		rc = pm8xxx_writeb(adc_pmic->dev->parent, ADC_ARB_USRP_CNTRL,
							data_arb_cntrl);
		if (rc < 0) {
			pr_debug("%s: PM8058 write failed\n", __func__);
			return rc;
		}
	}

	if (!arb_cntrl) {
		msm_xo_mode_vote(adc_pmic->adc_voter, MSM_XO_MODE_OFF);
		wake_unlock(&adc_pmic->adc_wakelock);
	}

	return 0;
}

static int32_t pm8058_xoadc_configure(uint32_t adc_instance,
					struct adc_conv_slot *slot)
{

	struct pmic8058_adc *adc_pmic = pmic_adc[adc_instance];
	u8 data_arb_cntrl = 0, data_amux_chan = 0, data_arb_rsv = 0;
	u8 data_dig_param = 0, data_ana_param2 = 0, data_ana_param = 0;
	int rc;

	rc = pm8058_xoadc_arb_cntrl(1, adc_instance, slot->chan_path);
	if (rc < 0) {
		pr_debug("%s: Configuring ADC Arbiter"
				"enable failed\n", __func__);
		return rc;
	}

	switch (slot->chan_path) {

	case CHAN_PATH_TYPE1:
		data_amux_chan = CHANNEL_VCOIN << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 2;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE2:
		data_amux_chan = CHANNEL_VBAT << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 3;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE3:
		data_amux_chan = CHANNEL_VCHG << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 10;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE4:
		data_amux_chan = CHANNEL_CHG_MONITOR << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 1;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE5:
		data_amux_chan = CHANNEL_VPH_PWR << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 3;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE6:
		data_amux_chan = CHANNEL_MPP5 << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 1;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[1];
		break;

	case CHAN_PATH_TYPE7:
		data_amux_chan = CHANNEL_MPP6 << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 1;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE8:
		data_amux_chan = CHANNEL_MPP7 << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 2;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE9:
		data_amux_chan = CHANNEL_MPP8 << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 2;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE10:
		data_amux_chan = CHANNEL_MPP9 << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 3;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE11:
		data_amux_chan = CHANNEL_USB_VBUS << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 3;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE12:
		data_amux_chan = CHANNEL_DIE_TEMP << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 1;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE13:
		data_amux_chan = CHANNEL_125V << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 1;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE14:
		data_amux_chan = CHANNEL_INTERNAL_2 << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 1;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;

	case CHAN_PATH_TYPE_NONE:
		data_amux_chan = CHANNEL_MUXOFF << 4;
		data_arb_rsv = 0x10;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 1;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[1];
		break;

	case CHAN_PATH_TYPE15:
		data_amux_chan = CHANNEL_INTERNAL << 4;
		data_arb_rsv = 0x20;
		slot->chan_properties.gain_numerator = 1;
		slot->chan_properties.gain_denominator = 1;
		slot->chan_properties.adc_graph = &adc_pmic->adc_graph[0];
		break;
	}

	rc = pm8xxx_writeb(adc_pmic->dev->parent,
			ADC_ARB_USRP_AMUX_CNTRL, data_amux_chan);
	if (rc < 0) {
		pr_debug("%s: PM8058 write failed\n", __func__);
		return rc;
	}

	rc = pm8xxx_writeb(adc_pmic->dev->parent,
			ADC_ARB_USRP_RSV, data_arb_rsv);
	if (rc < 0) {
		pr_debug("%s: PM8058 write failed\n", __func__);
		return rc;
	}

	/* Set default clock rate to 2.4 MHz XO ADC clock digital */
	switch (slot->chan_adc_config) {

	case ADC_CONFIG_TYPE1:
		data_ana_param = 0xFE;
		data_dig_param = 0x23;
		data_ana_param2 = 0xFF;
		/* AMUX register data to start the ADC conversion */
		data_arb_cntrl = 0xF1;
		break;

	case ADC_CONFIG_TYPE2:
		data_ana_param = 0xFE;
		data_dig_param = 0x03;
		data_ana_param2 = 0xFF;
		/* AMUX register data to start the ADC conversion */
		data_arb_cntrl = 0xF1;
		break;
	}

	rc = pm8xxx_writeb(adc_pmic->dev->parent,
				ADC_ARB_USRP_ANA_PARAM, data_ana_param);
	if (rc < 0) {
		pr_debug("%s: PM8058 write failed\n", __func__);
		return rc;
	}

	rc = pm8xxx_writeb(adc_pmic->dev->parent,
			ADC_ARB_USRP_DIG_PARAM, data_dig_param);
	if (rc < 0) {
		pr_debug("%s: PM8058 write failed\n", __func__);
		return rc;
	}

	rc = pm8xxx_writeb(adc_pmic->dev->parent,
			ADC_ARB_USRP_ANA_PARAM, data_ana_param2);
	if (rc < 0) {
		pr_debug("%s: PM8058 write failed\n", __func__);
		return rc;
	}

	enable_irq(adc_pmic->adc_irq);

	rc = pm8xxx_writeb(adc_pmic->dev->parent,
				ADC_ARB_USRP_CNTRL, data_arb_cntrl);
	if (rc < 0) {
		pr_debug("%s: PM8058 write failed\n", __func__);
		return rc;
	}

	return 0;
}

int32_t pm8058_xoadc_select_chan_and_start_conv(uint32_t adc_instance,
					struct adc_conv_slot *slot)
{
	struct pmic8058_adc *adc_pmic = pmic_adc[adc_instance];
	struct xoadc_conv_state *slot_state = adc_pmic->conv_queue_list;

	if (!xoadc_initialized)
		return -ENODEV;

	mutex_lock(&slot_state->list_lock);
	list_add_tail(&slot->list, &slot_state->slots);
	if (adc_pmic->xoadc_queue_count == 0) {
		if (adc_pmic->pdata->xoadc_vreg_set != NULL)
			adc_pmic->pdata->xoadc_vreg_set(1);
		pm8058_xoadc_configure(adc_instance, slot);
	}
	adc_pmic->xoadc_queue_count++;
	mutex_unlock(&slot_state->list_lock);

	return 0;
}
EXPORT_SYMBOL(pm8058_xoadc_select_chan_and_start_conv);

static int32_t pm8058_xoadc_dequeue_slot_request(uint32_t adc_instance,
				struct adc_conv_slot **slot)
{
	struct pmic8058_adc *adc_pmic = pmic_adc[adc_instance];
	struct xoadc_conv_state *slot_state = adc_pmic->conv_queue_list;
	int rc = 0;

	mutex_lock(&slot_state->list_lock);
	if (adc_pmic->xoadc_queue_count > 0 &&
			!list_empty(&slot_state->slots)) {
		*slot = list_first_entry(&slot_state->slots,
			struct adc_conv_slot, list);
		list_del(&(*slot)->list);
	} else
		rc = -EINVAL;
	mutex_unlock(&slot_state->list_lock);

	if (rc < 0) {
		if (pm8058_xoadc_can_print())
			pr_err("Pmic 8058 xoadc spurious interrupt detected\n");
		return rc;
	}

	return 0;
}

int32_t pm8058_xoadc_read_adc_code(uint32_t adc_instance, int32_t *data)
{
	struct pmic8058_adc *adc_pmic = pmic_adc[adc_instance];
	struct xoadc_conv_state *slot_state = adc_pmic->conv_queue_list;
	uint8_t rslt_lsb, rslt_msb;
	struct adc_conv_slot *slot;
	int32_t rc, max_ideal_adc_code = 1 << adc_pmic->adc_prop->bitresolution;

	if (!xoadc_initialized)
		return -ENODEV;

	rc = pm8xxx_readb(adc_pmic->dev->parent, ADC_ARB_USRP_DATA0,
							&rslt_lsb);
	if (rc < 0) {
		pr_debug("%s: PM8058 read failed\n", __func__);
		return rc;
	}

	rc = pm8xxx_readb(adc_pmic->dev->parent, ADC_ARB_USRP_DATA1,
							&rslt_msb);
	if (rc < 0) {
		pr_debug("%s: PM8058 read failed\n", __func__);
		return rc;
	}

	*data = (rslt_msb << 8) | rslt_lsb;

	/* Use the midpoint to determine underflow or overflow */
	if (*data > max_ideal_adc_code + (max_ideal_adc_code >> 1))
		*data |= ((1 << (8 * sizeof(*data) -
			adc_pmic->adc_prop->bitresolution)) - 1) <<
			adc_pmic->adc_prop->bitresolution;
	/* Return if this is a calibration run since there
	 * is no need to check requests in the waiting queue */
	if (xoadc_calib_first_adc)
		return 0;

	mutex_lock(&slot_state->list_lock);
	adc_pmic->xoadc_queue_count--;
	if (adc_pmic->xoadc_queue_count > 0) {
		slot = list_first_entry(&slot_state->slots,
				struct adc_conv_slot, list);
		pm8058_xoadc_configure(adc_instance, slot);
	}
	mutex_unlock(&slot_state->list_lock);

	mutex_lock(&slot_state->list_lock);
	/* Default value for switching off the arbiter after reading
	   the ADC value. Bit 0 set to 0. */
	if (adc_pmic->xoadc_queue_count == 0) {
		rc = pm8058_xoadc_arb_cntrl(0, adc_instance, CHANNEL_MUXOFF);
		if (rc < 0) {
			pr_debug("%s: Configuring ADC Arbiter disable"
						"failed\n", __func__);
			return rc;
		}
		if (adc_pmic->pdata->xoadc_vreg_set != NULL)
			adc_pmic->pdata->xoadc_vreg_set(0);
	}
	mutex_unlock(&slot_state->list_lock);

	return 0;
}
EXPORT_SYMBOL(pm8058_xoadc_read_adc_code);

static irqreturn_t pm8058_xoadc(int irq, void *dev_id)
{
	struct pmic8058_adc *xoadc_8058 = dev_id;
	struct adc_conv_slot *slot = NULL;
	int rc;

	disable_irq_nosync(xoadc_8058->adc_irq);

	if (xoadc_calib_first_adc)
		return IRQ_HANDLED;

	rc = pm8058_xoadc_dequeue_slot_request(xoadc_8058->xoadc_num, &slot);

	if (rc < 0)
		return IRQ_NONE;

	if (rc == 0)
		msm_adc_conv_cb(slot, 0, NULL, 0);

	return IRQ_HANDLED;
}

struct adc_properties *pm8058_xoadc_get_properties(uint32_t dev_instance)
{
	struct pmic8058_adc *xoadc_8058 = pmic_adc[dev_instance];

	return xoadc_8058->adc_prop;
}
EXPORT_SYMBOL(pm8058_xoadc_get_properties);

int32_t pm8058_xoadc_calib_device(uint32_t adc_instance)
{
	struct pmic8058_adc *adc_pmic = pmic_adc[adc_instance];
	struct adc_conv_slot *slot;
	int rc, offset_xoadc, slope_xoadc, calib_read_1, calib_read_2;

	if (adc_pmic->pdata->xoadc_vreg_set != NULL)
		adc_pmic->pdata->xoadc_vreg_set(1);

	pm8058_xoadc_slot_request(adc_instance, &slot);
	if (slot) {
		slot->chan_path = CHAN_PATH_TYPE13;
		slot->chan_adc_config = ADC_CONFIG_TYPE2;
		slot->chan_adc_calib = ADC_CONFIG_TYPE2;
		xoadc_calib_first_adc = true;
		rc = pm8058_xoadc_configure(adc_instance, slot);
		if (rc) {
			pr_err("pm8058_xoadc configure failed\n");
			goto fail;
		}
	} else {
		rc = -EINVAL;
		goto fail;
	}

	msleep(3);

	rc = pm8058_xoadc_read_adc_code(adc_instance, &calib_read_1);
	if (rc) {
		pr_err("pm8058_xoadc read adc failed\n");
		xoadc_calib_first_adc = false;
		goto fail;
	}
	xoadc_calib_first_adc = false;

	pm8058_xoadc_slot_request(adc_instance, &slot);
	if (slot) {
		slot->chan_path = CHAN_PATH_TYPE15;
		slot->chan_adc_config = ADC_CONFIG_TYPE2;
		slot->chan_adc_calib = ADC_CONFIG_TYPE2;
		xoadc_calib_first_adc = true;
		rc = pm8058_xoadc_configure(adc_instance, slot);
		if (rc) {
			pr_err("pm8058_xoadc configure failed\n");
			goto fail;
		}
	} else {
		rc = -EINVAL;
		goto fail;
	}

	msleep(3);

	rc = pm8058_xoadc_read_adc_code(adc_instance, &calib_read_2);
	if (rc) {
		pr_err("pm8058_xoadc read adc failed\n");
		xoadc_calib_first_adc = false;
		goto fail;
	}
	xoadc_calib_first_adc = false;

	pm8058_xoadc_restore_slot(adc_instance, slot);

	slope_xoadc = (((calib_read_1 - calib_read_2) << 10)/
					CHANNEL_ADC_625_MV);
	offset_xoadc = calib_read_2 -
			((slope_xoadc * CHANNEL_ADC_625_MV) >> 10);

	printk(KERN_INFO"pmic8058_xoadc:The offset for AMUX calibration"
						"was %d\n", offset_xoadc);

	adc_pmic->adc_graph[0].offset = offset_xoadc;
	adc_pmic->adc_graph[0].dy = (calib_read_1 - calib_read_2);
	adc_pmic->adc_graph[0].dx = CHANNEL_ADC_625_MV;

	/* Retain ideal calibration settings for therm readings */
	adc_pmic->adc_graph[1].offset = 0 ;
	adc_pmic->adc_graph[1].dy = (1 << 15) - 1;
	adc_pmic->adc_graph[1].dx = 2200;

	if (adc_pmic->pdata->xoadc_vreg_set != NULL)
		adc_pmic->pdata->xoadc_vreg_set(0);

	return 0;
fail:
	if (adc_pmic->pdata->xoadc_vreg_set != NULL)
		adc_pmic->pdata->xoadc_vreg_set(0);

	return rc;
}
EXPORT_SYMBOL(pm8058_xoadc_calib_device);

int32_t pm8058_xoadc_calibrate(uint32_t dev_instance,
				struct adc_conv_slot *slot, int *calib_status)
{
	*calib_status = CALIB_NOT_REQUIRED;

	return 0;
}
EXPORT_SYMBOL(pm8058_xoadc_calibrate);

#ifdef CONFIG_PM
static int pm8058_xoadc_suspend_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pmic8058_adc *adc_pmic = platform_get_drvdata(pdev);

	adc_pmic->msm_suspend_check = 1;

	return 0;
}

static int pm8058_xoadc_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct pmic8058_adc *adc_pmic = platform_get_drvdata(pdev);

	adc_pmic->msm_suspend_check = 0;

	return 0;
}

static const struct dev_pm_ops pm8058_xoadc_dev_pm_ops = {
	.suspend_noirq = pm8058_xoadc_suspend_noirq,
	.resume_noirq = pm8058_xoadc_resume_noirq,
};

#define PM8058_XOADC_DEV_PM_OPS	(&pm8058_xoadc_dev_pm_ops)
#else
#define PM8058_XOADC_DEV_PM_OPS NULL
#endif

static int __devinit pm8058_xoadc_probe(struct platform_device *pdev)
{
	struct xoadc_platform_data *pdata = pdev->dev.platform_data;
	struct pmic8058_adc *adc_pmic;
	int i, rc = 0;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data?\n");
		return -EINVAL;
	}

	adc_pmic = devm_kzalloc(&pdev->dev, sizeof(*adc_pmic), GFP_KERNEL);
	if (!adc_pmic) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	adc_pmic->dev = &pdev->dev;
	adc_pmic->adc_prop = pdata->xoadc_prop;
	adc_pmic->xoadc_num = pdata->xoadc_num;
	adc_pmic->xoadc_queue_count = 0;

	platform_set_drvdata(pdev, adc_pmic);

	if (adc_pmic->xoadc_num > XOADC_PMIC_0) {
		dev_err(&pdev->dev, "ADC device not supported\n");
		return -EINVAL;
	}

	adc_pmic->pdata = pdata;
	adc_pmic->adc_graph = devm_kzalloc(&pdev->dev,
		sizeof(struct linear_graph) * MAX_CHANNEL_PROPERTIES_QUEUE,
		GFP_KERNEL);
	if (!adc_pmic->adc_graph) {
		dev_err(&pdev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	/* Will be replaced by individual channel calibration */
	for (i = 0; i < MAX_CHANNEL_PROPERTIES_QUEUE; i++) {
		adc_pmic->adc_graph[i].offset = 0 ;
		adc_pmic->adc_graph[i].dy = (1 << 15) - 1;
		adc_pmic->adc_graph[i].dx = 2200;
	}

	if (pdata->xoadc_mpp_config != NULL)
		pdata->xoadc_mpp_config();

	adc_pmic->conv_slot_request = &adc_pmic->conv[0];
	adc_pmic->conv_slot_request->context =
		&adc_pmic->conv_queue_elements[0];

	mutex_init(&adc_pmic->conv_slot_request->list_lock);
	INIT_LIST_HEAD(&adc_pmic->conv_slot_request->slots);

	/* tie each slot and initwork them */
	for (i = 0; i < MAX_QUEUE_LENGTH; i++) {
		list_add(&adc_pmic->conv_slot_request->context[i].list,
					&adc_pmic->conv_slot_request->slots);
		INIT_WORK(&adc_pmic->conv_slot_request->context[i].work,
							msm_adc_wq_work);
		init_completion(&adc_pmic->conv_slot_request->context[i].comp);
		adc_pmic->conv_slot_request->context[i].idx = i;
	}

	adc_pmic->conv_queue_list = &adc_pmic->conv[1];

	mutex_init(&adc_pmic->conv_queue_list->list_lock);
	INIT_LIST_HEAD(&adc_pmic->conv_queue_list->slots);

	adc_pmic->adc_irq = platform_get_irq(pdev, 0);
	if (adc_pmic->adc_irq < 0)
		return -ENXIO;

	rc = request_threaded_irq(adc_pmic->adc_irq,
				NULL, pm8058_xoadc,
		IRQF_TRIGGER_RISING, "pm8058_adc_interrupt", adc_pmic);
	if (rc) {
		dev_err(&pdev->dev, "failed to request adc irq\n");
		return rc;
	}

	disable_irq(adc_pmic->adc_irq);

	if (adc_pmic->adc_voter == NULL) {
		adc_pmic->adc_voter = msm_xo_get(MSM_XO_TCXO_D1,
							"pmic8058_xoadc");
		if (IS_ERR(adc_pmic->adc_voter)) {
			dev_err(&pdev->dev, "Failed to get XO vote\n");
			return PTR_ERR(adc_pmic->adc_voter);
		}
	}

	device_init_wakeup(&pdev->dev, pdata->xoadc_wakeup);
	wake_lock_init(&adc_pmic->adc_wakelock, WAKE_LOCK_SUSPEND,
					"pmic8058_xoadc_wakelock");

	pmic_adc[adc_pmic->xoadc_num] = adc_pmic;

	if (pdata->xoadc_vreg_setup != NULL)
		pdata->xoadc_vreg_setup();

	xoadc_initialized = true;
	xoadc_calib_first_adc = false;

	return 0;
}

static int __devexit pm8058_xoadc_teardown(struct platform_device *pdev)
{
	struct pmic8058_adc *adc_pmic = platform_get_drvdata(pdev);

	if (adc_pmic->pdata->xoadc_vreg_shutdown != NULL)
		adc_pmic->pdata->xoadc_vreg_shutdown();

	wake_lock_destroy(&adc_pmic->adc_wakelock);
	msm_xo_put(adc_pmic->adc_voter);
	device_init_wakeup(&pdev->dev, 0);
	xoadc_initialized = false;

	return 0;
}

static struct platform_driver pm8058_xoadc_driver = {
	.probe = pm8058_xoadc_probe,
	.remove = __devexit_p(pm8058_xoadc_teardown),
	.driver = {
		.name = "pm8058-xoadc",
		.owner = THIS_MODULE,
		.pm = PM8058_XOADC_DEV_PM_OPS,
	},
};

static int __init pm8058_xoadc_init(void)
{
	return platform_driver_register(&pm8058_xoadc_driver);
}
module_init(pm8058_xoadc_init);

static void __exit pm8058_xoadc_exit(void)
{
	platform_driver_unregister(&pm8058_xoadc_driver);
}
module_exit(pm8058_xoadc_exit);

MODULE_ALIAS("platform:pmic8058_xoadc");
MODULE_DESCRIPTION("PMIC8058 XOADC driver");
MODULE_LICENSE("GPL v2");
