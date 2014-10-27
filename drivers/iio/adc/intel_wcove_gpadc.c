/*
 * iio_whiskeycove_gpadc.c - Intel  Whiskey Cove GPADC Driver
 *
 * Copyright (C) 2012 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Bin Yang <bin.yang@intel.com>
 * Author: Jenny TC <jenny.tc@intel.com>
 * Author: Pavan Kumar S <pavan.kumar.s@intel.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/iio/iio.h>
#include <linux/iio/machine.h>
#include <linux/iio/buffer.h>
#include <linux/iio/driver.h>
#include <linux/iio/types.h>
#include <linux/iio/consumer.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/iio/intel_wcove_gpadc.h>
#include <linux/platform_device.h>

#define OHM_MULTIPLIER 10

struct gpadc_info {
	u8 irq_status;
	u8 pmic_id;
	u8 intr_mask;
	int irq;
	int initialized;
	int sample_done;
	int channel_num;
	/* This mutex protects gpadc sample/config from concurrent conflict.
	   Any function, which does the sample or config, needs to
	   hold this lock.
	   If it is locked, it also means the gpadc is in active mode.
	*/
	struct mutex lock;
	struct device *dev;
	struct gpadc_regmap_t *gpadc_regmaps;
	struct gpadc_regs_t *gpadc_regs;
	bool is_pmic_provisioned;
	wait_queue_head_t wait;
};

static const char * const iio_ev_type_text[] = {
	[IIO_EV_TYPE_THRESH] = "thresh",
	[IIO_EV_TYPE_MAG] = "mag",
	[IIO_EV_TYPE_ROC] = "roc",
	[IIO_EV_TYPE_THRESH_ADAPTIVE] = "thresh_adaptive",
	[IIO_EV_TYPE_MAG_ADAPTIVE] = "mag_adaptive",
};

static const char * const iio_ev_dir_text[] = {
	[IIO_EV_DIR_EITHER] = "either",
	[IIO_EV_DIR_RISING] = "rising",
	[IIO_EV_DIR_FALLING] = "falling"
};

static const char * const iio_ev_info_text[] = {
	[IIO_EV_INFO_ENABLE] = "en",
	[IIO_EV_INFO_VALUE] = "value",
	[IIO_EV_INFO_HYSTERESIS] = "hysteresis",
};

static int gpadc_busy_wait(struct gpadc_regs_t *regs)
{
	u8 tmp;
	int timeout = 0;

	tmp = intel_soc_pmic_readb(regs->gpadcreq);
	while (tmp & regs->gpadcreq_busy && timeout < 500) {
		tmp = intel_soc_pmic_readb(regs->gpadcreq);
		usleep_range(1800, 2000);
		timeout++;
	}

	if (tmp & regs->gpadcreq_busy)
		return -EBUSY;
	else
		return 0;
}

static void gpadc_dump(struct gpadc_info *info)
{
	u8 tmp;
	struct gpadc_regs_t *regs = info->gpadc_regs;

	dev_err(info->dev, "GPADC registers dump:\n");
	tmp = intel_soc_pmic_readb(regs->adcirq);
	dev_err(info->dev, "ADCIRQ: 0x%x\n", tmp);
	tmp = intel_soc_pmic_readb(regs->madcirq);
	dev_err(info->dev, "MADCIRQ: 0x%x\n", tmp);
	tmp = intel_soc_pmic_readb(regs->gpadcreq);
	dev_err(info->dev, "GPADCREQ: 0x%x\n", tmp);
	tmp = intel_soc_pmic_readb(regs->adc1cntl);
	dev_err(info->dev, "ADC1CNTL: 0x%x\n", tmp);
}

static irqreturn_t gpadc_isr(int irq, void *data)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t gpadc_threaded_isr(int irq, void *data)
{
	struct gpadc_info *info = iio_priv(data);
	struct gpadc_regs_t *regs = info->gpadc_regs;

	info->sample_done = 1;
	wake_up(&info->wait);

	return IRQ_HANDLED;
}


/**
 * iio_whiskeycove_gpadc_sample - do gpadc sample.
 * @indio_dev: industrial IO GPADC device handle
 * @ch: gpadc bit set of channels to sample, for example, set ch = (1<<0)|(1<<2)
 *	means you are going to sample both channel 0 and 2 at the same time.
 * @res:gpadc sampling result
 *
 * Returns 0 on success or an error code.
 *
 * This function may sleep.
 */

int iio_whiskeycove_gpadc_sample(struct iio_dev *indio_dev,
				int ch, struct gpadc_result *res)
{
	struct gpadc_info *info = iio_priv(indio_dev);
	int i, ret, reg_val;
	u8 tmp, th, tl;
	u8 mask, cursrc;
	unsigned long rlsb;
	static const unsigned long rlsb_array[] = {
		0, 260420, 130210, 65100, 32550, 16280,
		8140, 4070, 2030, 0, 260420, 130210};

	struct gpadc_regs_t *regs = info->gpadc_regs;

	if (!info->initialized)
		return -ENODEV;

	mutex_lock(&info->lock);

	mask = info->intr_mask;
	intel_soc_pmic_clearb(regs->madcirq, mask);
	intel_soc_pmic_clearb(regs->mirqlvl1, regs->mirqlvl1_adc);

	tmp = regs->gpadcreq_irqen;

	for (i = 0; i < info->channel_num; i++) {
		if (ch & (1 << i))
			tmp |= (1 << info->gpadc_regmaps[i].cntl);
	}

	info->sample_done = 0;

	ret = gpadc_busy_wait(regs);
	if (ret) {
		dev_err(info->dev, "GPADC is busy\n");
		goto done;
	}

	intel_soc_pmic_writeb(regs->gpadcreq, tmp);

	ret = wait_event_timeout(info->wait, info->sample_done, HZ);
	if (ret == 0) {
		gpadc_dump(info);
		ret = -ETIMEDOUT;
		dev_err(info->dev, "sample timeout, return %d\n", ret);
		goto done;
	} else {
		ret = 0;
	}

	for (i = 0; i < info->channel_num; i++) {
		if (ch & (1 << i)) {
			tl = intel_soc_pmic_readb(info->gpadc_regmaps[i].rsltl);
			th = intel_soc_pmic_readb(info->gpadc_regmaps[i].rslth);

			reg_val = ((th & 0xF) << 8) + tl;
			switch (i) {
			case PMIC_GPADC_CHANNEL_VBUS:
			case PMIC_GPADC_CHANNEL_PMICTEMP:
			case PMIC_GPADC_CHANNEL_PEAK:
			case PMIC_GPADC_CHANNEL_AGND:
			case PMIC_GPADC_CHANNEL_VREF:
				/* Auto mode not applicable */
				res->data[i] = reg_val;
				break;
			case PMIC_GPADC_CHANNEL_BATID:
			case PMIC_GPADC_CHANNEL_BATTEMP0:
			case PMIC_GPADC_CHANNEL_BATTEMP1:
			case PMIC_GPADC_CHANNEL_SYSTEMP0:
			case PMIC_GPADC_CHANNEL_SYSTEMP1:
			case PMIC_GPADC_CHANNEL_SYSTEMP2:
			if (!info->is_pmic_provisioned) {
					/* Auto mode with Scaling 4
					 * for non-provisioned A0 */
					rlsb = 32550;
					res->data[i] = (reg_val * rlsb)/10000;
					break;
			}
			/* Case fall-through for PMIC-A1 onwards.
			 * For USBID, Auto-mode-without-scaling always
			 */
			case PMIC_GPADC_CHANNEL_USBID:
				/* Auto mode without Scaling */
				cursrc = (th & 0xF0) >> 4;
				rlsb = rlsb_array[cursrc];
				res->data[i] = (reg_val * rlsb)/10000;
				break;
			}
		}
	}

done:
	intel_soc_pmic_setb(regs->mirqlvl1, regs->mirqlvl1_adc);
	intel_soc_pmic_setb(regs->madcirq, mask);
	mutex_unlock(&info->lock);
	return ret;
}
EXPORT_SYMBOL(iio_whiskeycove_gpadc_sample);

static struct gpadc_result sample_result;
static int chs;

static ssize_t intel_whiskeycove_gpadc_store_channel(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct gpadc_info *info = iio_priv(indio_dev);

	if (sscanf(buf, "%x", &chs) != 1) {
		dev_err(dev, "one channel argument is needed\n");
		return -EINVAL;
	}

	if (chs < (1 << 0) || chs >= (1 << info->channel_num)) {
		dev_err(dev, "invalid channel, should be in [0x1 - 0x1FF]\n");
		return -EINVAL;
	}

	return size;
}

static ssize_t intel_whiskeycove_gpadc_show_channel(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", chs);
}

static ssize_t intel_whiskeycove_gpadc_store_sample(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int value, ret;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);

	memset(sample_result.data, 0, sizeof(sample_result.data));

	if (sscanf(buf, "%d", &value) != 1) {
		dev_err(dev, "one argument is needed\n");
		return -EINVAL;
	}

	if (value == 1) {
		ret = iio_whiskeycove_gpadc_sample(indio_dev, chs,
						&sample_result);
		if (ret) {
			dev_err(dev, "sample failed\n");
			return ret;
		}
	} else {
		dev_err(dev, "input '1' to sample\n");
		return -EINVAL;
	}

	return size;
}

static ssize_t intel_whiskeycove_gpadc_show_result(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i;
	int used = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct gpadc_info *info = iio_priv(indio_dev);

	for (i = 0; i < info->channel_num; i++) {
		used += snprintf(buf + used, PAGE_SIZE - used,
				"sample_result[%s] = %x\n",
				info->gpadc_regmaps[i].name,
				sample_result.data[i]);
	}

	return used;
}


static DEVICE_ATTR(channel, S_IWUSR | S_IRUGO,
		intel_whiskeycove_gpadc_show_channel,
		intel_whiskeycove_gpadc_store_channel);
static DEVICE_ATTR(sample, S_IWUSR, NULL, intel_whiskeycove_gpadc_store_sample);
static DEVICE_ATTR(result, S_IRUGO, intel_whiskeycove_gpadc_show_result, NULL);

static struct attribute *intel_whiskeycove_gpadc_attrs[] = {
	&dev_attr_channel.attr,
	&dev_attr_sample.attr,
	&dev_attr_result.attr,
	NULL,
};
static struct attribute_group intel_whiskeycove_gpadc_attr_group = {
	.name = "whiskeycove_gpadc",
	.attrs = intel_whiskeycove_gpadc_attrs,
};

static u16 get_tempzone_val(u16 resi_val)
{
	u8 cursel = 0, hys = 0;
	u16 trsh = 0, count = 0, bsr_num = 0;
	u16 tempzone_val = 0;

	/* multiply to convert into Ohm*/
	resi_val *= OHM_MULTIPLIER;

	/* CUR = max(floor(log2(round(ADCNORM/2^5)))-7,0)
	 * TRSH = round(ADCNORM/(2^(4+CUR)))
	 * HYS = if(∂ADCNORM>0 then max(round(∂ADCNORM/(2^(7+CUR))),1)
	 * else 0
	 */

	/*
	 * while calculating the CUR[2:0], instead of log2
	 * do a BSR (bit scan reverse) since we are dealing with integer values
	 */
	bsr_num = resi_val;
	bsr_num /= (1 << 5);

	while (bsr_num >>= 1)
		count++;

	cursel = max((count - 7), 0);

	/* calculate the TRSH[8:0] to be programmed */
	trsh = ((resi_val) / (1 << (4 + cursel)));

	tempzone_val = (hys << 12) | (cursel << 9) | trsh;

	return tempzone_val;
}

static int whiskeycove_adc_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long m)
{
	int ret;
	int ch = chan->channel;
	struct gpadc_info *info = iio_priv(indio_dev);
	struct gpadc_result res;

	ret = iio_whiskeycove_gpadc_sample(indio_dev, (1 << ch), &res);
	if (ret) {
		dev_err(info->dev, "sample failed\n");
		return -EINVAL;
	}

	*val = res.data[ch];

	return ret;
}

static int whiskeycove_adc_read_event_value(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			enum iio_event_type type, enum iio_event_direction dir,
			enum iio_event_info info, int *val, int *val2)
{
	int ch = chan->channel;
	u16 reg_h, reg_l;
	u8 val_h, val_l;
	struct gpadc_info *gp_info = iio_priv(indio_dev);

	dev_dbg(gp_info->dev, "ch:%d, adc_read_event: %s-%s-%s\n", ch,
			iio_ev_type_text[type], iio_ev_dir_text[dir],
			iio_ev_info_text[info]);

	if (type == IIO_EV_TYPE_THRESH) {
		switch (dir) {
		case IIO_EV_DIR_RISING:
			reg_h = gp_info->gpadc_regmaps[ch].alrt_max_h;
			reg_l = gp_info->gpadc_regmaps[ch].alrt_max_l;
			break;
		case IIO_EV_DIR_FALLING:
			reg_h = gp_info->gpadc_regmaps[ch].alrt_min_h;
			reg_l = gp_info->gpadc_regmaps[ch].alrt_min_l;
			break;
		default:
			dev_err(gp_info->dev,
				"iio_event_direction %d not supported\n",
				dir);
			return -EINVAL;
		}
	} else {
		dev_err(gp_info->dev,
				"iio_event_type %d not supported\n",
				type);
		return -EINVAL;
	}

	dev_dbg(gp_info->dev, "reg_h:%x, reg_l:%x\n", reg_h, reg_l);

	val_l = intel_soc_pmic_readb(reg_l);
	val_h = intel_soc_pmic_readb(reg_h);

	switch (info) {
	case IIO_EV_INFO_VALUE:
		*val = ((val_h & 0xF) << 8) + val_l;
		if (ch != PMIC_GPADC_CHANNEL_PMICTEMP) {
				int thrsh = *val & 0x01FF;
				int cur = (*val >> 9) & 0x07;

				*val = thrsh * (1 << (4 + cur))
					/ OHM_MULTIPLIER;
		}
		break;
	case IIO_EV_INFO_HYSTERESIS:
		*val = (val_h >> 4) & 0x0F;
		break;
	default:
		dev_err(gp_info->dev,
				"iio_event_info %d not supported\n",
				info);
		return -EINVAL;
	}

	dev_dbg(gp_info->dev, "read_event_sent: %x\n", *val);
	return IIO_VAL_INT;
}

static int whiskeycove_adc_write_event_value(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			enum iio_event_type type, enum iio_event_direction dir,
			enum iio_event_info info, int val, int val2)
{
	int err;
	int ch = chan->channel;
	u16 reg_h, reg_l;
	u8 val_h, val_l, mask;
	struct gpadc_info *gp_info = iio_priv(indio_dev);

	dev_dbg(gp_info->dev, "ch:%d, adc_write_event: %s-%s-%s\n", ch,
			iio_ev_type_text[type], iio_ev_dir_text[dir],
			iio_ev_info_text[info]);

	dev_dbg(gp_info->dev, "write_event_value: %x\n", val);

	if (type == IIO_EV_TYPE_THRESH) {
		switch (dir) {
		case IIO_EV_DIR_RISING:
			reg_h = gp_info->gpadc_regmaps[ch].alrt_max_h;
			reg_l = gp_info->gpadc_regmaps[ch].alrt_max_l;
			break;
		case IIO_EV_DIR_FALLING:
			reg_h = gp_info->gpadc_regmaps[ch].alrt_min_h;
			reg_l = gp_info->gpadc_regmaps[ch].alrt_min_l;
			break;
		default:
			dev_err(gp_info->dev,
				"iio_event_direction %d not supported\n",
				dir);
			return -EINVAL;
		}
	} else {
		dev_err(gp_info->dev,
				"iio_event_type %d not supported\n",
				type);
		return -EINVAL;
	}

	dev_dbg(gp_info->dev, "reg_h:%x, reg_l:%x\n", reg_h, reg_l);

	switch (info) {
	case IIO_EV_INFO_VALUE:
		if (ch != PMIC_GPADC_CHANNEL_PMICTEMP)
			val = get_tempzone_val(val);

		val_h = (val >> 8) & 0xF;
		val_l = val & 0xFF;
		mask = 0x0F;

		err = intel_soc_pmic_update(reg_l, val_l, 0xFF);
		if (err) {
			dev_err(gp_info->dev, "Error updating register:%X\n",
				reg_l);
			return -EINVAL;
		}
		break;
	case IIO_EV_INFO_HYSTERESIS:
		val_h = (val << 4) & 0xF0;
		mask = 0xF0;
		break;
	default:
		dev_err(gp_info->dev,
				"iio_event_info %d not supported\n",
				info);
		return -EINVAL;
	}

	err = intel_soc_pmic_update(reg_h, val_h, mask);
	if (err) {
		dev_err(gp_info->dev, "Error updating register:%X\n", reg_h);
		return -EINVAL;
	}

	return IIO_VAL_INT;
}

static const struct iio_info whiskeycove_adc_info = {
	.read_raw = &whiskeycove_adc_read_raw,
	.read_event_value = &whiskeycove_adc_read_event_value,
	.write_event_value = &whiskeycove_adc_write_event_value,
	.driver_module = THIS_MODULE,
};

static int wcove_gpadc_probe(struct platform_device *pdev)
{
	int err;
	u8 pmic_prov;
	struct gpadc_info *info;
	struct iio_dev *indio_dev;
	struct intel_wcove_gpadc_platform_data *pdata =
			pdev->dev.platform_data;

	if (!pdata) {
		dev_err(&pdev->dev, "no platform data supplied\n");
		return -EINVAL;
	}

	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*info));
	if (indio_dev == NULL) {
		dev_err(&pdev->dev, "allocating iio device failed\n");
		return -ENOMEM;
	}

	info = iio_priv(indio_dev);

	mutex_init(&info->lock);
	init_waitqueue_head(&info->wait);
	info->dev = &pdev->dev;
	info->irq = platform_get_irq(pdev, 0);
	info->intr_mask = pdata->intr_mask;
	info->channel_num = pdata->channel_num;
	info->gpadc_regmaps = pdata->gpadc_regmaps;
	info->gpadc_regs = pdata->gpadc_regs;

	err = request_threaded_irq(info->irq, gpadc_isr, gpadc_threaded_isr,
			IRQF_ONESHOT, "wcove_gpadc", indio_dev);
	if (err) {
		dev_err(&pdev->dev, "unable to register irq %d\n", info->irq);
		return err;
	}

	platform_set_drvdata(pdev, indio_dev);

	indio_dev->dev.parent = &pdev->dev;
	indio_dev->name = pdev->name;

	indio_dev->channels = pdata->gpadc_channels;
	indio_dev->num_channels = pdata->channel_num;
	indio_dev->info = &whiskeycove_adc_info;
	indio_dev->modes = INDIO_DIRECT_MODE;

	err = iio_map_array_register(indio_dev, pdata->gpadc_iio_maps);
	if (err)
		goto err_release_irq;

	err = iio_device_register(indio_dev);
	if (err < 0)
		goto err_array_unregister;

	info->pmic_id = intel_soc_pmic_readb(PMIC_ID_ADDR);
	if (info->pmic_id < 0) {
		dev_err(&pdev->dev, "Error reading PMIC ID register\n");
		goto err_iio_device_unregister;
	}

	dev_info(&pdev->dev, "PMIC-ID: %x\n", info->pmic_id);
	/* Check if PMIC is provisioned */
	pmic_prov = intel_soc_pmic_readb(PMIC_SPARE03_ADDR);
	if (pmic_prov < 0) {
		dev_err(&pdev->dev,
				"Error reading PMIC SPARE03 REG\n");
		goto err_iio_device_unregister;
	}

	if ((pmic_prov & PMIC_PROV_MASK) == PMIC_PROVISIONED) {
		dev_info(&pdev->dev, "PMIC provisioned\n");
		info->is_pmic_provisioned = true;
	} else {
		dev_info(info->dev, "PMIC not provisioned\n");
	}

	err = sysfs_create_group(&pdev->dev.kobj,
			&intel_whiskeycove_gpadc_attr_group);
	if (err) {
		dev_err(&pdev->dev, "Unable to export sysfs interface, error: %d\n",
			err);
		goto err_iio_device_unregister;
	}

	info->initialized = 1;

	dev_dbg(&pdev->dev, "wcove adc probed\n");

	return 0;

err_iio_device_unregister:
	iio_device_unregister(indio_dev);
err_array_unregister:
	iio_map_array_unregister(indio_dev);
err_release_irq:
	free_irq(info->irq, info);
	return err;
}

static int wcove_gpadc_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct gpadc_info *info = iio_priv(indio_dev);

	sysfs_remove_group(&pdev->dev.kobj,
			&intel_whiskeycove_gpadc_attr_group);

	iio_device_unregister(indio_dev);
	iio_map_array_unregister(indio_dev);
	free_irq(info->irq, info);

	return 0;
}

#ifdef CONFIG_PM
static int wcove_gpadc_suspend(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct gpadc_info *info = iio_priv(indio_dev);

	if (!mutex_trylock(&info->lock))
		return -EBUSY;

	return 0;
}

static int wcove_gpadc_resume(struct device *dev)
{
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct gpadc_info *info = iio_priv(indio_dev);

	mutex_unlock(&info->lock);
	return 0;
}
#endif

static const struct dev_pm_ops wcove_gpadc_driver_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(wcove_gpadc_suspend,
				wcove_gpadc_resume)
};

static struct platform_driver wcove_gpadc_driver = {
	.driver = {
		   .name = "wcove_gpadc",
		   .owner = THIS_MODULE,
		   .pm = &wcove_gpadc_driver_pm_ops,
		   },
	.probe = wcove_gpadc_probe,
	.remove = wcove_gpadc_remove,
};
module_platform_driver(wcove_gpadc_driver);

MODULE_AUTHOR("Yang Bin<bin.yang@intel.com>");
MODULE_DESCRIPTION("Intel Whiskey Cove GPADC Driver");
MODULE_LICENSE("GPL");
