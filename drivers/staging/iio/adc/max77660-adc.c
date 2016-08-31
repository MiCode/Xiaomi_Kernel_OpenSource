/*
 * max77660-adc.c -- MAXIM MAX77660 ADC.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/module.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/mfd/max77660/max77660-core.h>
#include <linux/completion.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/driver.h>

#define MOD_NAME "max77660-adc"
#define ADC_CONVERTION_TIMEOUT	(msecs_to_jiffies(1000))

struct max77660_adc_info {
	int acquisition_time_us;
	int mult_volt_uV;
	int div_val;
};

#define MAX77660_ADC_INFO(_chan, _adcacq, _mult, _div)	\
[MAX77660_ADC_CH_##_chan] = {				\
		.acquisition_time_us = _adcacq,		\
		.mult_volt_uV =	_mult,			\
		.div_val = _div,			\
	}

static struct max77660_adc_info max77660_adc_info[] = {
	MAX77660_ADC_INFO(VBYP, 12, 10240, 4096),
	MAX77660_ADC_INFO(TDIE, 16, 1, 1),
	MAX77660_ADC_INFO(VBBATT, 16, 4300, 4096),
	MAX77660_ADC_INFO(VSYS, 12, 5120, 4096),
	MAX77660_ADC_INFO(VDCIN, 12, 10240, 4096),
	MAX77660_ADC_INFO(VWCSNS, 12, 10240, 4096),
	MAX77660_ADC_INFO(VTHM, 64, 2500, 4096),
	MAX77660_ADC_INFO(VICHG, 8, 2500, 4096),
	MAX77660_ADC_INFO(VMBATDET, 96, 2500, 4096),
	MAX77660_ADC_INFO(VMBAT, 12, 5120, 4096),
	MAX77660_ADC_INFO(ADC0, 16, 2500, 4096),
	MAX77660_ADC_INFO(ADC1, 16, 2500, 4096),
	MAX77660_ADC_INFO(ADC2, 16, 2500, 4096),
	MAX77660_ADC_INFO(ADC3, 16, 2500, 4096),
};

struct max77660_adc {
	struct device			*dev;
	struct device			*parent;
	u8				adc_control;
	u8				iadc_val;
	int				irq;
	struct max77660_adc_info	*adc_info;
	struct completion		conv_completion;
	struct max77660_adc_wakeup_property adc_wake_props;
	bool				wakeup_props_available;
};

static inline int max77660_is_valid_channel(struct  max77660_adc *adc, int chan)
{
	/*
	 * ES1.0: Do not convert ADC0 channel otherwise it will shutdown system.
	 */
	if (max77660_is_es_1_0(adc->parent) &&
			(chan == MAX77660_ADC_CH_ADC0))  {
		dev_err(adc->dev, "ES1.0 verion errata: do not convert ADC0\n");
		return false;
	}
	return true;
}

static irqreturn_t max77660_adc_irq(int irq, void *data)
{
	struct max77660_adc *adc = data;
	u8 status;
	int ret;

	ret = max77660_reg_read(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_ADCINT, &status);
	if (ret < 0) {
		dev_err(adc->dev, "ADCINT read failed: %d\n", ret);
		goto out;
	}

	if (status & MAX77660_ADCINT_ADCCONVINT)
		complete(&adc->conv_completion);
	else if (status & MAX77660_ADCINT_DTRINT)
		dev_info(adc->dev, "DTR int occured\n");
	else if (status & MAX77660_ADCINT_DTFINT)
		dev_info(adc->dev, "DTF int occured\n");
	else
		dev_err(adc->dev, "ADC-IRQ for unknown reason, 0x%02x\n",
			status);
out:
	return IRQ_HANDLED;
}

static int max77660_adc_start_mask_interrupt(struct max77660_adc *adc, int irq,
		int mask)
{
	int ret;

	if (mask)
		ret = max77660_reg_set_bits(adc->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_ADCINTM, irq);
	else
		ret = max77660_reg_clr_bits(adc->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_ADCINTM, irq);
	if (ret < 0)
		dev_err(adc->dev, "ADCINTM update failed: %d\n", ret);
	return ret;
}

static int max77660_adc_enable(struct max77660_adc *adc, int enable)
{
	int ret;

	if (enable)
		ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_ADCCTRL,
				adc->adc_control | MAX77660_ADCCTRL_ADCEN);
	else
		ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_ADCCTRL, adc->adc_control);
	if (ret < 0)
		dev_err(adc->dev, "ADCCTRL write failed: %d\n", ret);
	return ret;
}

static int max77660_adc_start_convertion(struct max77660_adc *adc, int adc_chan)
{
	u8 adc_l;
	u8 adc_h;
	int ret;
	u8 chan0 = 0;
	u8 chan1 = 0;

	ret = max77660_adc_enable(adc, true);
	if (ret < 0)
		return ret;

	ret = max77660_adc_start_mask_interrupt(adc,
				MAX77660_ADCINT_ADCCONVINT, 0);
	if (ret < 0)
		goto out;

	if (adc_chan < 8)
		chan0 = BIT(adc_chan);
	else
		chan1 = BIT(adc_chan - 8);

	ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
		MAX77660_REG_ADCSEL0, chan0);
	if (ret < 0) {
		dev_err(adc->dev, "ADCSEL0 write failed: %d\n", ret);
		goto out;
	}

	ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
		MAX77660_REG_ADCSEL1, chan1);
	if (ret < 0) {
		dev_err(adc->dev, "ADCSEL1 write failed: %d\n", ret);
		goto out;
	}

	ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
		MAX77660_REG_ADCCHSEL, adc_chan);
	if (ret < 0) {
		dev_err(adc->dev, "ADCCHSEL write failed: %d\n", ret);
		goto out;
	}

	if (adc_chan >= MAX77660_ADC_CH_ADC0) {
		int chan_num = adc_chan - MAX77660_ADC_CH_ADC0;
		u8 iadc = adc->iadc_val;
		iadc |= MAX77660_IADC_IADCMUX(chan_num);
		ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_IADC, iadc);
		if (ret < 0) {
			dev_err(adc->dev, "IADC write failed: %d\n", ret);
			goto out;
		}
	}

	if (adc->adc_info[adc_chan].acquisition_time_us)
		udelay(adc->adc_info[adc_chan].acquisition_time_us);

	INIT_COMPLETION(adc->conv_completion);
	ret = max77660_reg_update(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_ADCCTRL, MAX77660_ADCCTRL_ADCCONV,
			MAX77660_ADCCTRL_ADCCONV);
	if (ret < 0) {
		dev_err(adc->dev, "ADCCTR write failed: %d\n", ret);
		goto out;
	}

	ret = wait_for_completion_timeout(&adc->conv_completion,
				ADC_CONVERTION_TIMEOUT);
	if (ret == 0) {
		dev_err(adc->dev, "ADC conversion not completed\n");
		ret = -ETIMEDOUT;
		goto out;
	}

	ret = max77660_reg_read(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_ADCDATAL, &adc_l);
	if (ret < 0) {
		dev_err(adc->dev, "ADCDATAL read failed: %d\n", ret);
		goto out;
	}

	ret = max77660_reg_read(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_ADCDATAH, &adc_h);
	if (ret < 0) {
		dev_err(adc->dev, "ADCDATAH read failed: %d\n", ret);
		goto out;
	}

	ret = ((adc_h & 0xF) << 8) | adc_l;

out:
	max77660_adc_start_mask_interrupt(adc, MAX77660_ADCINT_ADCCONVINT, 1);
	max77660_adc_enable(adc, false);
	return ret;
}

static int max77660_adc_read_channel(struct max77660_adc *adc, int adc_chan)
{
	int ret;
	int val;

	ret = max77660_adc_start_convertion(adc, adc_chan);
	if (ret < 0) {
		dev_err(adc->dev, "ADC start coversion failed\n");
		return ret;
	}

	if (adc_chan != MAX77660_ADC_CH_TDIE)
		val = (adc->adc_info[adc_chan].mult_volt_uV * ret) /
					adc->adc_info[adc_chan].div_val;
	else
		val = (298 * ret * 250)/(163 * 4095) - 273;
	return val;
}

static int max77660_adc_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct  max77660_adc *adc = iio_priv(indio_dev);
	int ret;

	if (!max77660_is_valid_channel(adc, chan->channel))
		return -EINVAL;

	switch (mask) {
	case 0:
		mutex_lock(&indio_dev->mlock);
		ret = max77660_adc_start_convertion(adc, chan->channel);
		if (ret < 0) {
			dev_err(adc->dev, "ADC start coversion failed\n");
			mutex_unlock(&indio_dev->mlock);
			return ret;
		}
		*val = ret;
		mutex_unlock(&indio_dev->mlock);
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		mutex_lock(&indio_dev->mlock);
		ret = max77660_adc_read_channel(adc, chan->channel);
		if (ret < 0) {
			dev_err(adc->dev, "ADC read failed: %d\n", ret);
			mutex_unlock(&indio_dev->mlock);
			return ret;
		}
		*val =  ret;
		mutex_unlock(&indio_dev->mlock);
		return IIO_VAL_INT;
	}
	return -EINVAL;
}

static const struct iio_info max77660_adc_iio_info = {
	.read_raw = max77660_adc_read_raw,
	.driver_module = THIS_MODULE,
};

#define MAX77660_ADC_CHAN_IIO(chan)				\
{								\
	.datasheet_name = MAX77660_DATASHEET_NAME(chan),	\
	.type = IIO_VOLTAGE,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_SCALE),		\
	.indexed = 1,						\
	.channel = MAX77660_ADC_CH_##chan,			\
}

static const struct iio_chan_spec max77660_adc_iio_channel[] = {
	MAX77660_ADC_CHAN_IIO(VBYP),
	MAX77660_ADC_CHAN_IIO(TDIE),
	MAX77660_ADC_CHAN_IIO(VBBATT),
	MAX77660_ADC_CHAN_IIO(VSYS),
	MAX77660_ADC_CHAN_IIO(VDCIN),
	MAX77660_ADC_CHAN_IIO(VWCSNS),
	MAX77660_ADC_CHAN_IIO(VTHM),
	MAX77660_ADC_CHAN_IIO(VICHG),
	MAX77660_ADC_CHAN_IIO(VMBATDET),
	MAX77660_ADC_CHAN_IIO(VMBAT),
	MAX77660_ADC_CHAN_IIO(ADC0),
	MAX77660_ADC_CHAN_IIO(ADC1),
	MAX77660_ADC_CHAN_IIO(ADC2),
	MAX77660_ADC_CHAN_IIO(ADC3),
};

static int max77660_adc_probe(struct platform_device *pdev)
{
	struct max77660_adc *adc;
	struct max77660_platform_data *pdata;
	struct max77660_adc_platform_data *adc_pdata;
	struct iio_dev *iodev;
	int ret;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (!pdata || !pdata->adc_pdata) {
		dev_err(&pdev->dev, "No platform data\n");
		return -ENODEV;
	}
	adc_pdata = pdata->adc_pdata;

	iodev = iio_device_alloc(sizeof(*adc));
	if (!iodev) {
		dev_err(&pdev->dev, "iio_device_alloc failed\n");
		return -ENOMEM;
	}
	adc = iio_priv(iodev);
	adc->dev = &pdev->dev;
	adc->parent = pdev->dev.parent;
	adc->adc_info = max77660_adc_info;
	init_completion(&adc->conv_completion);
	dev_set_drvdata(&pdev->dev, iodev);

	adc->irq = platform_get_irq(pdev, 0);
	ret = request_threaded_irq(adc->irq, NULL,
		max77660_adc_irq,
		IRQF_ONESHOT | IRQF_EARLY_RESUME, dev_name(adc->dev),
		adc);
	if (ret < 0) {
		dev_err(adc->dev,
			"request irq %d failed: %dn", adc->irq, ret);
		goto out;
	}

	if (adc_pdata->adc_ref_enabled)
		adc->adc_control |= MAX77660_ADCCTRL_ADCREFEN;

	if (adc_pdata->adc_avg_sample <= 1)
		adc->adc_control |= MAX77660_ADCCTRL_ADCAVG(0);
	else if (adc_pdata->adc_avg_sample <= 2)
		adc->adc_control |= MAX77660_ADCCTRL_ADCAVG(1);
	else if (adc_pdata->adc_avg_sample <= 16)
		adc->adc_control |= MAX77660_ADCCTRL_ADCAVG(2);
	else
		adc->adc_control |= MAX77660_ADCCTRL_ADCAVG(3);

	if (adc_pdata->adc_current_uA == 0)
		adc->iadc_val = 0;
	else if (adc_pdata->adc_current_uA <= 10)
		adc->iadc_val = 1;
	else if (adc_pdata->adc_current_uA <= 50)
		adc->iadc_val = 2;
	else
		adc->iadc_val = 3;

	iodev->name = MOD_NAME;
	iodev->dev.parent = &pdev->dev;
	iodev->info = &max77660_adc_iio_info;
	iodev->modes = INDIO_DIRECT_MODE;
	iodev->channels = max77660_adc_iio_channel;
	iodev->num_channels = ARRAY_SIZE(max77660_adc_iio_channel);
	ret = iio_device_register(iodev);
	if (ret < 0) {
		dev_err(adc->dev, "iio_device_register() failed: %d\n", ret);
		goto out_irq_free;
	}

	if (adc_pdata->channel_mapping) {
		ret = iio_map_array_register(iodev, adc_pdata->channel_mapping);
		if (ret < 0) {
			dev_err(adc->dev,
				"iio_map_array_register() failed: %d\n", ret);
			goto out_irq_free;
			}
	}

	device_set_wakeup_capable(&pdev->dev, 1);
	if (adc_pdata->adc_wakeup_data) {
		memcpy(&adc->adc_wake_props, adc_pdata->adc_wakeup_data,
			sizeof(struct max77660_adc_wakeup_property));
		adc->wakeup_props_available = true;
		device_wakeup_enable(&pdev->dev);
	}
	return 0;

out_irq_free:
	free_irq(adc->irq, adc);
out:
	iio_device_free(iodev);
	return ret;
}

static int max77660_adc_remove(struct platform_device *pdev)
{
	struct iio_dev *iodev = dev_to_iio_dev(&pdev->dev);
	struct max77660_adc *adc = iio_priv(iodev);

	iio_device_unregister(iodev);
	free_irq(adc->irq, adc);
	iio_device_free(iodev);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max77660_adc_wakeup_configure(struct max77660_adc *adc)
{
	int thres_h, thres_l, ch;
	u8 int_mask, ch1, ch0;
	u8 adc_avg;
	int ret;

	thres_h = 0;
	thres_l = 0;
	int_mask = 0xFF;
	ch = adc->adc_wake_props.adc_channel_number;
	if (adc->adc_wake_props.adc_high_threshold > 0) {
		thres_h = adc->adc_wake_props.adc_high_threshold & 0xFFF;
		int_mask &= ~MAX77660_ADCINT_DTRINT;
	}
	if (adc->adc_wake_props.adc_low_threshold > 0) {
		thres_l = adc->adc_wake_props.adc_low_threshold & 0xFFF;
		int_mask &= ~MAX77660_ADCINT_DTFINT;
	}

	ch0 = (ch > 7) ? 0 : BIT(ch);
	ch1 = (ch > 7) ? BIT(ch - 8) : 0;

	if (adc->adc_wake_props.adc_avg_sample <= 1)
		adc_avg = MAX77660_ADCCTRL_ADCAVG(0);
	else if (adc->adc_wake_props.adc_avg_sample <= 2)
		adc_avg = MAX77660_ADCCTRL_ADCAVG(1);
	else if (adc->adc_wake_props.adc_avg_sample <= 16)
		adc_avg = MAX77660_ADCCTRL_ADCAVG(2);
	else
		adc_avg = MAX77660_ADCCTRL_ADCAVG(3);

	ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_DTRL, thres_h & 0xFF);
	if (ret < 0) {
		dev_err(adc->dev, "DTRL write failed: %d\n", ret);
		return ret;
	}
	ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_DTRH, (thres_h >> 8) & 0xF);
	if (ret < 0) {
		dev_err(adc->dev, "DTRH write failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_DTFL, thres_l & 0xFF);
	if (ret < 0) {
		dev_err(adc->dev, "DTFL write failed: %d\n", ret);
		return ret;
	}
	ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_DTFH, (thres_l >> 8) & 0xF);
	if (ret < 0) {
		dev_err(adc->dev, "DTFH write failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_ADCSEL0, ch0);
	if (ret < 0) {
		dev_err(adc->dev, "ADCSEL0 write failed: %d\n", ret);
		return ret;
	}
	ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_ADCSEL1, ch1);
	if (ret < 0) {
		dev_err(adc->dev, "ADCSEL1 write failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_update(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_ADCCTRL, adc_avg,
			MAX77660_ADCCTRL_ADCAVG_MASK);
	if (ret < 0) {
		dev_err(adc->dev, "ADCCTRL update failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_ADCINTM, int_mask);
	if (ret < 0) {
		dev_err(adc->dev, "ADCINTM write failed\n");
		return ret;
	}

	ret = max77660_reg_update(adc->parent, MAX77660_PWR_SLAVE,
		MAX77660_REG_ADCCTRL, MAX77660_ADCCTRL_ADCCONT,
		MAX77660_ADCCTRL_ADCCONT);
	if (ret < 0) {
		dev_err(adc->dev, "ADCCTR update failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static int max77660_adc_wakeup_reset(struct max77660_adc *adc)
{
	int ret;

	ret = max77660_reg_write(adc->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_ADCINTM, 0xFF);
	if (ret < 0) {
		dev_err(adc->dev, "ADCINTM write failed\n");
		return ret;
	}

	ret = max77660_reg_update(adc->parent, MAX77660_PWR_SLAVE,
		MAX77660_REG_ADCCTRL, 0, MAX77660_ADCCTRL_ADCCONT);
	if (ret < 0) {
		dev_err(adc->dev, "ADCCTR update failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static int max77660_adc_suspend(struct device *dev)
{
	struct iio_dev *iodev = dev_to_iio_dev(dev);
	struct max77660_adc *adc = iio_priv(iodev);
	int ret;

	if (!device_may_wakeup(dev) || !adc->wakeup_props_available)
		goto skip_wakeup;

	ret = max77660_adc_wakeup_configure(adc);
	if (ret < 0)
		goto skip_wakeup;

	enable_irq_wake(adc->irq);
skip_wakeup:
	return 0;
}

static int max77660_adc_resume(struct device *dev)
{
	struct iio_dev *iodev = dev_to_iio_dev(dev);
	struct max77660_adc *adc = iio_priv(iodev);
	int ret;

	if (!device_may_wakeup(dev) || !adc->wakeup_props_available)
		goto skip_wakeup;

	ret = max77660_adc_wakeup_reset(adc);
	if (ret < 0)
		goto skip_wakeup;

	disable_irq_wake(adc->irq);
skip_wakeup:
	return 0;
};
#endif

static const struct dev_pm_ops max77660_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(max77660_adc_suspend,
				max77660_adc_resume)
};

static struct platform_driver max77660_adc_driver = {
	.probe = max77660_adc_probe,
	.remove = max77660_adc_remove,
	.driver = {
		.name = "max77660-adc",
		.owner = THIS_MODULE,
		.pm = &max77660_pm_ops,
	},
};

static int __init max77660_adc_driver_init(void)
{
	return platform_driver_register(&max77660_adc_driver);
}
subsys_initcall_sync(max77660_adc_driver_init);

static void __exit max77660_adc_driver_exit(void)
{
	platform_driver_unregister(&max77660_adc_driver);
}
module_exit(max77660_adc_driver_exit);


MODULE_DESCRIPTION("max77660 ADC driver");
MODULE_AUTHOR("Laxman Dewangan<ldewangan@nvidia.com>");
MODULE_ALIAS("platform:max77660-adc");
MODULE_LICENSE("GPL v2");
