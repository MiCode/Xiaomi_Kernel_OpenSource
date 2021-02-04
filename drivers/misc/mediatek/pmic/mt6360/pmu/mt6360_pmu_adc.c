/*
 *  drivers/misc/mediatek/pmic/mt6360/mt6360_pmu_adc.c
 *  Driver for MT6360 PMU ADC part
 *
 *  Copyright (C) 2018 Mediatek Technology Inc.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/ktime.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>

#include "../inc/mt6360_pmu.h"
#include "../inc/mt6360_pmu_adc.h"

struct mt6360_pmu_adc_info {
	struct device *dev;
	struct mt6360_pmu_info *mpi;
	struct task_struct *scan_task;
	struct completion adc_complete;
	struct mutex adc_lock;
	ktime_t last_off_timestamps[MAX_CHANNEL];
};

static const struct mt6360_adc_platform_data def_platform_data = {
	.adc_wait_t = 0x1,
	.adc_idle_t = 0xa,
	.zcv_en = 0,
};

static int mt6360_adc_get_process_val(struct mt6360_pmu_adc_info *info,
				      int chan_idx, int *val)
{
	int ret = 0;

	switch (chan_idx) {
	case USBID_CHANNEL:
	case VREF_TS_CHANNEL:
	case TS_CHANNEL:
		*val *= 1250;
		break;
	case TEMP_JC_CHANNEL:
		*val = (*val * 105 - 8000) / 100;
		break;
	case VBAT_CHANNEL:
	case VSYS_CHANNEL:
	case CHG_VDDP_CHANNEL:
		*val *= 1250;
		break;
	case VBUSDIV5_CHANNEL:
		*val *= 6250;
		break;
	case VBUSDIV2_CHANNEL:
	case IBAT_CHANNEL:
		*val *= 2500;
		break;
	case IBUS_CHANNEL:
		ret = mt6360_pmu_reg_read(info->mpi, MT6360_PMU_CHG_CTRL3);
		if (ret < 0)
			return ret;
		if (((ret & 0xfc) >> 2) < 0x6)
			*val *= 1900;
		else
			*val *= 2500;
		break;
	default:
		break;
	}
	return ret;
}

#define ADC_RETRY_CNT	(10)
static void mt6360_pmu_adc_irq_enable(const char *name, int en);

static int mt6360_adc_read_raw(struct iio_dev *iio_dev,
			       const struct iio_chan_spec *chan,
			       int *val, int *val2, long mask)
{
	struct mt6360_pmu_adc_info *mpai = iio_priv(iio_dev);
	long timeout;
	u8 tmp[2], rpt[3];
	ktime_t predict_end_t;
	int retry_cnt = 0, ret;

	mt_dbg(&iio_dev->dev, "%s: channel [%d] s\n", __func__, chan->channel);
	mutex_lock(&mpai->adc_lock);
	/* select preferred channel that we want */
	ret = mt6360_pmu_reg_update_bits(mpai->mpi, MT6360_PMU_ADC_RPT_1,
					 0xf0, chan->channel << 4);
	if (ret < 0)
		goto err_adc_init;
	/* enable adc channel we want and adc_en */
	memset(tmp, 0, sizeof(tmp));
	tmp[0] |= (1 << 7);
	tmp[(chan->channel / 8) ? 0 : 1] |= (1 << (chan->channel % 8));
	ret = mt6360_pmu_reg_block_write(mpai->mpi,
					 MT6360_PMU_ADC_CONFIG, 2, tmp);
	if (ret < 0)
		goto err_adc_init;
	predict_end_t = ktime_add_ms(mpai->last_off_timestamps[chan->channel],
				     50);
	mt6360_pmu_adc_irq_enable("adc_donei", 1);
retry:
	if (retry_cnt++ > ADC_RETRY_CNT) {
		dev_err(mpai->dev, "reach adc retry cnt\n");
		goto err_adc_conv;
	}
	reinit_completion(&mpai->adc_complete);
	/* wait for conversion to complete */
	timeout = wait_for_completion_interruptible_timeout(
				    &mpai->adc_complete, msecs_to_jiffies(600));
	if (timeout == 0) {
		ret = -ETIMEDOUT;
		goto err_adc_conv;
	} else if (timeout < 0) {
		ret = -EINTR;
		goto err_adc_conv;
	}
	memset(rpt, 0, sizeof(rpt));
	ret = mt6360_pmu_reg_block_read(mpai->mpi,
					MT6360_PMU_ADC_RPT_1, 3, rpt);
	if (ret < 0)
		goto err_adc_conv;
	/* get report channel */
	if ((rpt[0] & 0x0f) != chan->channel) {
		mt_dbg(&iio_dev->dev,
			"not wanted channel report [%02x]\n", rpt[0]);
		goto retry;
	}
	if (!ktime_after(ktime_get(), predict_end_t)) {
		dev_dbg(&iio_dev->dev, "time is not after 50ms chan_time\n");
		goto retry;
	}
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*val = (rpt[1] << 8) | rpt[2];
		break;
	case IIO_CHAN_INFO_PROCESSED:
		*val = (rpt[1] << 8) | rpt[2];
		ret = mt6360_adc_get_process_val(mpai, chan->channel, val);
		if (ret < 0)
			goto err_adc_conv;
		break;
	default:
		break;
	}
	ret = IIO_VAL_INT;
err_adc_conv:
	mt6360_pmu_adc_irq_enable("adc_donei", 0);
	/* whatever disable all channels, except adc_en */
	memset(tmp, 0, sizeof(tmp));
	tmp[0] |= (1 << 7);
	mt6360_pmu_reg_block_write(mpai->mpi, MT6360_PMU_ADC_CONFIG, 2, tmp);
	mpai->last_off_timestamps[chan->channel] = ktime_get();
err_adc_init:
	mutex_unlock(&mpai->adc_lock);
	mt_dbg(&iio_dev->dev, "%s: channel [%d] e\n", __func__, chan->channel);
	return ret;
}

static const struct iio_info mt6360_adc_iio_info = {
	.read_raw = mt6360_adc_read_raw,
	.driver_module = THIS_MODULE,
};

#define MT6360_ADC_CHAN(idx, _type) {				\
	.type = _type,						\
	.channel = idx##_CHANNEL,				\
	.scan_index = idx##_CHANNEL,				\
	.scan_type =  {						\
		.sign = 's',					\
		.realbits = 32,					\
		.storagebits = 32,				\
		.shift = 0,					\
		.endianness = IIO_CPU,				\
	},							\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
				BIT(IIO_CHAN_INFO_PROCESSED),	\
	.datasheet_name = #idx,					\
	.indexed = 1,						\
}

static const struct iio_chan_spec mt6360_adc_channels[] = {
	MT6360_ADC_CHAN(USBID, IIO_VOLTAGE),
	MT6360_ADC_CHAN(VBUSDIV5, IIO_VOLTAGE),
	MT6360_ADC_CHAN(VBUSDIV2, IIO_VOLTAGE),
	MT6360_ADC_CHAN(VSYS, IIO_VOLTAGE),
	MT6360_ADC_CHAN(VBAT, IIO_VOLTAGE),
	MT6360_ADC_CHAN(IBUS, IIO_CURRENT),
	MT6360_ADC_CHAN(IBAT, IIO_CURRENT),
	MT6360_ADC_CHAN(CHG_VDDP, IIO_VOLTAGE),
	MT6360_ADC_CHAN(TEMP_JC, IIO_TEMP),
	MT6360_ADC_CHAN(VREF_TS, IIO_VOLTAGE),
	MT6360_ADC_CHAN(TS, IIO_VOLTAGE),
	IIO_CHAN_SOFT_TIMESTAMP(MAX_CHANNEL),
};

static irqreturn_t mt6360_pmu_bat_ovp_adc_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_adc_info *mpai = iio_priv(data);

	dev_warn(mpai->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_adc_wakeup_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_adc_info *mpai = iio_priv(data);

	dev_dbg(mpai->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_adc_donei_handler(int irq, void *data)
{
	struct mt6360_pmu_adc_info *mpai = iio_priv(data);

	mt_dbg(mpai->dev, "%s\n", __func__);
	complete(&mpai->adc_complete);
	return IRQ_HANDLED;
}

static struct mt6360_pmu_irq_desc mt6360_pmu_adc_irq_desc[] = {
	MT6360_PMU_IRQDESC(bat_ovp_adc_evt),
	MT6360_PMU_IRQDESC(adc_wakeup_evt),
	MT6360_PMU_IRQDESC(adc_donei),
};

static void mt6360_pmu_adc_irq_enable(const char *name, int en)
{
	struct mt6360_pmu_irq_desc *irq_desc;
	int i = 0;

	if (unlikely(!name))
		return;
	for (i = 0; i < ARRAY_SIZE(mt6360_pmu_adc_irq_desc); i++) {
		irq_desc = mt6360_pmu_adc_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		if (!strcmp(irq_desc->name, name)) {
			if (en)
				enable_irq(irq_desc->irq);
			else
				disable_irq_nosync(irq_desc->irq);
			break;
		}
	}
}

static void mt6360_pmu_adc_irq_register(struct platform_device *pdev)
{
	struct mt6360_pmu_irq_desc *irq_desc;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(mt6360_pmu_adc_irq_desc); i++) {
		irq_desc = mt6360_pmu_adc_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		ret = platform_get_irq_byname(pdev, irq_desc->name);
		if (ret < 0)
			continue;
		irq_desc->irq = ret;
		ret = devm_request_threaded_irq(&pdev->dev, irq_desc->irq, NULL,
						irq_desc->irq_handler,
						IRQF_TRIGGER_FALLING,
						irq_desc->name,
						platform_get_drvdata(pdev));
		if (ret < 0)
			dev_err(&pdev->dev,
				"request %s irq fail\n", irq_desc->name);
	}
}

static int mt6360_adc_scan_task_threadfn(void *data)
{
	struct mt6360_pmu_adc_info *mpai = data;
	struct iio_dev *indio_dev = iio_priv_to_dev(mpai);
	int channel_vals[MAX_CHANNEL];
	int i, bit, var = 0;
	int ret;

	dev_dbg(mpai->dev, "%s ++\n", __func__);
	while (!kthread_should_stop()) {
		memset(channel_vals, 0, sizeof(channel_vals));
		i = 0;
		for_each_set_bit(bit, indio_dev->active_scan_mask,
				 indio_dev->masklength) {
			ret = mt6360_adc_read_raw(indio_dev,
						  mt6360_adc_channels + bit,
						  &var, NULL,
						  IIO_CHAN_INFO_PROCESSED);
			if (ret < 0)
				dev_err(mpai->dev, "get adc[%d] fail\n", bit);
			if (kthread_should_stop())
				break;
			channel_vals[i++] = var;
		}
		if (kthread_should_stop())
			break;
#if 1 /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)) */
		iio_push_to_buffers_with_timestamp(indio_dev, channel_vals,
						   iio_get_time_ns(indio_dev));
#else
		iio_push_to_buffers_with_timestamp(indio_dev, channel_vals,
						   iio_get_time_ns());
#endif
	}
	dev_dbg(mpai->dev, "%s --\n", __func__);
	return 0;
}

static int mt6360_adc_iio_post_enable(struct iio_dev *iio_dev)
{
	struct mt6360_pmu_adc_info *mpai = iio_priv(iio_dev);

	dev_dbg(&iio_dev->dev, "%s ++\n", __func__);
	mpai->scan_task = kthread_run(mt6360_adc_scan_task_threadfn, mpai,
				      devm_kasprintf(mpai->dev, GFP_KERNEL,
				      "scan_thread.%s", dev_name(mpai->dev)));
	dev_dbg(&iio_dev->dev, "%s --\n", __func__);
	return PTR_ERR_OR_ZERO(mpai->scan_task);
}

static int mt6360_adc_iio_pre_disable(struct iio_dev *iio_dev)
{
	struct mt6360_pmu_adc_info *mpai = iio_priv(iio_dev);

	dev_dbg(&iio_dev->dev, "%s ++\n", __func__);
	if (mpai->scan_task) {
		kthread_stop(mpai->scan_task);
		mpai->scan_task = NULL;
	}
	dev_dbg(&iio_dev->dev, "%s --\n", __func__);
	return 0;
}

static const struct iio_buffer_setup_ops mt6360_adc_iio_setup_ops = {
	.postenable = mt6360_adc_iio_post_enable,
	.predisable = mt6360_adc_iio_pre_disable,
};

static int mt6360_adc_iio_device_register(struct iio_dev *indio_dev)
{
	struct mt6360_pmu_adc_info *mpai = iio_priv(indio_dev);
	struct iio_buffer *buffer;
	int ret;

	dev_dbg(mpai->dev, "%s ++\n", __func__);
	indio_dev->name = dev_name(mpai->dev);
	indio_dev->dev.parent = mpai->dev;
	indio_dev->dev.of_node = mpai->dev->of_node;
	indio_dev->info = &mt6360_adc_iio_info;
	indio_dev->channels = mt6360_adc_channels;
	indio_dev->num_channels = ARRAY_SIZE(mt6360_adc_channels);
	indio_dev->modes = INDIO_DIRECT_MODE | INDIO_BUFFER_SOFTWARE;
	indio_dev->setup_ops = &mt6360_adc_iio_setup_ops;
	buffer = devm_iio_kfifo_allocate(mpai->dev);
	if (!buffer)
		return -ENOMEM;
	iio_device_attach_buffer(indio_dev, buffer);
	ret = devm_iio_device_register(mpai->dev, indio_dev);
	if (ret < 0) {
		dev_err(mpai->dev, "iio device  register fail\n");
		return ret;
	}
	dev_dbg(mpai->dev, "%s --\n", __func__);
	return 0;
}

static inline int mt6360_pmu_adc_reset(struct mt6360_pmu_adc_info *info)
{
	u8 tmp[3] = {0x80, 0, 0};
	ktime_t all_off_time;
	int i;

	all_off_time = ktime_get();
	for (i = 0; i < MAX_CHANNEL; i++)
		info->last_off_timestamps[i] = all_off_time;
	/* enable adc_en, clear adc_chn_en/zcv/en/adc_wait_t/adc_idle_t */
	return mt6360_pmu_reg_block_write(info->mpi,
					  MT6360_PMU_ADC_CONFIG, 3, tmp);
}

static const struct mt6360_pdata_prop mt6360_pdata_props[] = {
};

static int mt6360_adc_apply_pdata(struct mt6360_pmu_adc_info *mpai,
				  struct mt6360_adc_platform_data *pdata)
{
	int ret;

	dev_dbg(mpai->dev, "%s ++\n", __func__);
	ret = mt6360_pdata_apply_helper(mpai->mpi, pdata, mt6360_pdata_props,
					ARRAY_SIZE(mt6360_pdata_props));
	if (ret < 0)
		return ret;
	dev_dbg(mpai->dev, "%s --\n", __func__);
	return 0;
}

static const struct mt6360_val_prop mt6360_val_props[] = {
	MT6360_DT_VALPROP(adc_wait_t, struct mt6360_adc_platform_data),
	MT6360_DT_VALPROP(adc_idle_t, struct mt6360_adc_platform_data),
	MT6360_DT_VALPROP(zcv_en, struct mt6360_adc_platform_data),
};

static int mt6360_adc_parse_dt_data(struct device *dev,
				    struct mt6360_adc_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	dev_dbg(dev, "%s ++\n", __func__);
	memcpy(pdata, &def_platform_data, sizeof(*pdata));
	mt6360_dt_parser_helper(np, (void *)pdata,
				mt6360_val_props, ARRAY_SIZE(mt6360_val_props));
	dev_dbg(dev, "%s --\n", __func__);
	return 0;
}

static int mt6360_pmu_adc_probe(struct platform_device *pdev)
{
	struct mt6360_adc_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct mt6360_pmu_adc_info *mpai;
	struct iio_dev *indio_dev;
	bool use_dt = pdev->dev.of_node;
	int ret;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	if (use_dt) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = mt6360_adc_parse_dt_data(&pdev->dev, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "parse dt fail\n");
			return ret;
		}
		pdev->dev.platform_data = pdata;
	}
	if (!pdata) {
		dev_err(&pdev->dev, "no platform data specified\n");
		return -EINVAL;
	}
	indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(*mpai));
	if (!indio_dev)
		return -ENOMEM;
	mpai = iio_priv(indio_dev);
	mpai->dev = &pdev->dev;
	mpai->mpi = dev_get_drvdata(pdev->dev.parent);
	init_completion(&mpai->adc_complete);
	mutex_init(&mpai->adc_lock);
	platform_set_drvdata(pdev, indio_dev);

	/* first reset all channels before use */
	ret = mt6360_pmu_adc_reset(mpai);
	if (ret < 0) {
		dev_err(&pdev->dev, "adc reset fail\n");
		return ret;
	}
	/* apply platform data */
	ret = mt6360_adc_apply_pdata(mpai, pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "apply pdata fail\n");
		return ret;
	}
	/* adc iio device register */
	ret = mt6360_adc_iio_device_register(indio_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "iio dev register fail\n");
		return ret;
	}
	/* irq register */
	mt6360_pmu_adc_irq_register(pdev);
	/* default disable adc_donei irq by default */
	mt6360_pmu_adc_irq_enable("adc_donei", 0);
	dev_info(&pdev->dev, "%s: successfully probed\n", __func__);
	return 0;
}

static int mt6360_pmu_adc_remove(struct platform_device *pdev)
{
	struct mt6360_pmu_adc_info *mpai = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s\n", __func__);
	if (mpai->scan_task)
		kthread_stop(mpai->scan_task);
	return 0;
}

static int __maybe_unused mt6360_pmu_adc_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused mt6360_pmu_adc_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6360_pmu_adc_pm_ops,
			 mt6360_pmu_adc_suspend, mt6360_pmu_adc_resume);

static const struct of_device_id __maybe_unused mt6360_pmu_adc_of_id[] = {
	{ .compatible = "mediatek,mt6360_pmu_adc", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_pmu_adc_of_id);

static const struct platform_device_id mt6360_pmu_adc_id[] = {
	{ "mt6360_pmu_adc", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_pmu_adc_id);

static struct platform_driver mt6360_pmu_adc_driver = {
	.driver = {
		.name = "mt6360_pmu_adc",
		.owner = THIS_MODULE,
		.pm = &mt6360_pmu_adc_pm_ops,
		.of_match_table = of_match_ptr(mt6360_pmu_adc_of_id),
	},
	.probe = mt6360_pmu_adc_probe,
	.remove = mt6360_pmu_adc_remove,
	.id_table = mt6360_pmu_adc_id,
};
module_platform_driver(mt6360_pmu_adc_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 PMU ADC Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
