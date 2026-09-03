// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 */

#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/leds.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/device.h>

#define VIB_DEVICE			"sprd_vibrator"

#define CUR_DRV_CAL_SEL			GENMASK(13, 12)
#define SLP_LDOVIBR_PD_EN		BIT(9)
#define LDO_VIBR_PD			BIT(8)
#define SC2730_CUR_DRV_CAL_SEL		0
#define SC2730_SLP_LDOVIBR_PD_EN	BIT(14)
#define SC2730_LDO_VIBR_PD		BIT(13)
#define UMP9620_CUR_DRV_CAL_SEL		0
#define UMP9620_SLP_LDOVIBR_PD_EN	BIT(14)
#define UMP9620_LDO_VIBR_PD		BIT(13)

struct sc27xx_vibra_data {
	u32 cur_drv_cal_sel;
	u32 slp_pd_en;
	u32 ldo_pd;
};

struct vibra_info {
	struct		workqueue_struct *vibr_queue;
	struct		work_struct vibr_work;
	struct		hrtimer vibr_timer;
	struct regmap	*regmap;
	const struct sc27xx_vibra_data *data;
	int		ldo_state;
	int		shutdown_flag;
	atomic_t	vibr_dur;
	spinlock_t	vibr_lock;
	atomic_t	vibr_state;
	u32		base;
	bool		enabled;
};

static struct vibra_info *g_vibra_info;

struct sc27xx_vibra_data sc2731_data = {
	.cur_drv_cal_sel = CUR_DRV_CAL_SEL,
	.slp_pd_en = SLP_LDOVIBR_PD_EN,
	.ldo_pd = LDO_VIBR_PD,
};

struct sc27xx_vibra_data sc2730_data = {
	.cur_drv_cal_sel = SC2730_CUR_DRV_CAL_SEL,
	.slp_pd_en = SC2730_SLP_LDOVIBR_PD_EN,
	.ldo_pd = SC2730_LDO_VIBR_PD,
};

struct sc27xx_vibra_data sc2721_data = {
	.cur_drv_cal_sel = CUR_DRV_CAL_SEL,
	.slp_pd_en = SLP_LDOVIBR_PD_EN,
	.ldo_pd = LDO_VIBR_PD,
};

struct sc27xx_vibra_data ump9620_data = {
	.cur_drv_cal_sel = UMP9620_CUR_DRV_CAL_SEL,
	.slp_pd_en = UMP9620_SLP_LDOVIBR_PD_EN,
	.ldo_pd = UMP9620_LDO_VIBR_PD,
};

static void sc27xx_vibra_set(struct vibra_info *info, bool on)
{
	const struct sc27xx_vibra_data *data = info->data;
	if (on) {
		regmap_update_bits(info->regmap, info->base, data->ldo_pd, 0);
		regmap_update_bits(info->regmap, info->base,
				   data->slp_pd_en, 0);
		info->enabled = true;
	} else {
		regmap_update_bits(info->regmap, info->base, data->ldo_pd,
				   data->ldo_pd);
		regmap_update_bits(info->regmap, info->base,
				   data->slp_pd_en, data->slp_pd_en);
		info->enabled = false;
	}
}

static int vibr_Enable(void)
{
	if (!g_vibra_info->ldo_state) {
		sc27xx_vibra_set(g_vibra_info, true);
		g_vibra_info->ldo_state = 1;
	}
	return 0;
}

static int vibr_Disable(void)
{

	if (g_vibra_info->ldo_state) {
		sc27xx_vibra_set(g_vibra_info, false);
		g_vibra_info->ldo_state = 0;
	}
	return 0;
}

static void update_vibrator(struct work_struct *work)
{
	struct vibra_info *info = container_of(work, struct vibra_info, vibr_work);

	if (atomic_read(&info->vibr_state) == 0)
		vibr_Disable();
	else
		vibr_Enable();
}

static void vibrator_enable(unsigned int dur, unsigned int activate)
{
	unsigned long flags;

	spin_lock_irqsave(&g_vibra_info->vibr_lock, flags);
	hrtimer_cancel(&g_vibra_info->vibr_timer);

	if (activate == 0 || g_vibra_info->shutdown_flag == 1) {
		atomic_set(&g_vibra_info->vibr_state, 0);
	} else {
		dur = (dur > 15000 ? 15000 : dur);
		atomic_set(&g_vibra_info->vibr_state, 1);
		hrtimer_start(&g_vibra_info->vibr_timer,
			      ktime_set(dur / 1000, (dur % 1000) * 1000000),
			      HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&g_vibra_info->vibr_lock, flags);
	queue_work(g_vibra_info->vibr_queue, &g_vibra_info->vibr_work);
}

static int sc27xx_vibra_hw_init(struct vibra_info *info)
{
	const struct sc27xx_vibra_data *data = info->data;

	if (!data->cur_drv_cal_sel)
		return 0;
	return regmap_update_bits(info->regmap, info->base, data->cur_drv_cal_sel, 0);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	struct vibra_info *info = container_of(timer, struct vibra_info, vibr_timer);

	atomic_set(&info->vibr_state, 0);
	queue_work(info->vibr_queue, &info->vibr_work);
	return HRTIMER_NORESTART;
}

static atomic_t vib_state;

static ssize_t vibr_activate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&g_vibra_info->vibr_state));
}

static ssize_t vibr_activate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int activate, dur;
	ssize_t ret;

	ret = kstrtouint(buf, 10, &activate);
	if (ret) {
		dev_err(dev, "set activate fail\n");
		return ret;
	}

	dur = atomic_read(&g_vibra_info->vibr_dur);
	vibrator_enable(dur, activate);
	ret = size;
	return ret;
}

static ssize_t vibr_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", atomic_read(&vib_state));
}

static ssize_t vibr_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int state;
	ssize_t ret;

	ret = kstrtouint(buf, 10, &state);
	if (ret) {
		dev_err(dev, "set state fail\n");
		return ret;
	}
	atomic_set(&vib_state, state);

	ret = size;
	return ret;
}
static ssize_t vibr_duration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int duration;
	ssize_t ret;

	ret = kstrtouint(buf, 10, &duration);
	if (ret) {
		dev_err(dev, "set duration fail\n");
		return ret;
	}

	atomic_set(&g_vibra_info->vibr_dur, duration);
	ret = size;
	return ret;
}

static DEVICE_ATTR(activate, 0644, vibr_activate_show, vibr_activate_store);
static DEVICE_ATTR(state, 0644, vibr_state_show, vibr_state_store);
static DEVICE_ATTR(duration, 0644, NULL, vibr_duration_store);

static struct attribute *activate_attrs[] = {
	&dev_attr_activate.attr,
	NULL,
};

static struct attribute *state_attrs[] = {
	&dev_attr_state.attr,
	NULL,
};

static struct attribute *duration_attrs[] = {
	&dev_attr_duration.attr,
	NULL,
};

static struct attribute_group activate_group = {
	.attrs = activate_attrs,
};

static struct attribute_group state_group = {
	.attrs = state_attrs,
};

static struct attribute_group duration_group = {
	.attrs = duration_attrs,
};

static const struct attribute_group *vibr_group[] = {
	&activate_group,
	&state_group,
	&duration_group,
	NULL
};

static struct led_classdev led_vibr = {
	.name		= "vibrator",
	.groups		= vibr_group,
};

static int sc27xx_vibra_probe(struct platform_device *pdev)
{
	struct vibra_info *info;
	const struct sc27xx_vibra_data *data;
	int error;

	data = of_device_get_match_data(&pdev->dev);
	if (!data) {
		dev_err(&pdev->dev, "no matching driver data found\n");
		return -EINVAL;
	}

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!info->regmap) {
		dev_err(&pdev->dev, "failed to get vibrator regmap.\n");
		return -ENODEV;
	}

	error = device_property_read_u32(&pdev->dev, "reg", &info->base);
	if (error) {
		dev_err(&pdev->dev, "failed to get vibrator base address.\n");
		return error;
	}

	info->data = data;
	info->enabled = false;

	error = sc27xx_vibra_hw_init(info);
	if (error) {
		dev_err(&pdev->dev, "failed to initialize the vibrator.\n");
		return error;
	}

	error = devm_led_classdev_register(&pdev->dev, &led_vibr);
	if (error < 0) {
		dev_err(&pdev->dev, "led class register fail\n");
		return error;
	}

	info->vibr_queue = create_singlethread_workqueue(VIB_DEVICE);
	if (!info->vibr_queue) {
		dev_err(&pdev->dev, "unable to create workqueue\n");
		return -ENODATA;
	}

	INIT_WORK(&info->vibr_work, update_vibrator);
	spin_lock_init(&info->vibr_lock);
	info->shutdown_flag = 0;
	atomic_set(&info->vibr_state, 0);
	hrtimer_init(&info->vibr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	info->vibr_timer.function = vibrator_timer_func;

	dev_set_drvdata(&pdev->dev, info);
	g_vibra_info = info;

	dev_info(&pdev->dev, "probe done\n");

	return 0;
}

static int vib_remove(struct platform_device *pdev)
{
	struct vibra_info *info = dev_get_drvdata(&pdev->dev);

	cancel_work_sync(&info->vibr_work);
	hrtimer_cancel(&info->vibr_timer);

	return 0;
}

static void vib_shutdown(struct platform_device *pdev)
{
	unsigned long flags;
	struct vibra_info *info = dev_get_drvdata(&pdev->dev);

	dev_err(&pdev->dev, "shutdown: enter!\n");
	spin_lock_irqsave(&info->vibr_lock, flags);
	info->shutdown_flag = 1;
	if (atomic_read(&info->vibr_state)) {
		dev_err(&pdev->dev, "vib_shutdown: vibrator will disable\n");
		atomic_set(&info->vibr_state, 0);
		spin_unlock_irqrestore(&info->vibr_lock, flags);
		vibr_Disable();
		return;
	}
	spin_unlock_irqrestore(&info->vibr_lock, flags);
}

static const struct of_device_id sc27xx_vibra_of_match[] = {
	{ .compatible = "sprd,sc2731-vibrator", .data = &sc2731_data },
	{ .compatible = "sprd,sc2730-vibrator", .data = &sc2730_data },
	{ .compatible = "sprd,sc2721-vibrator", .data = &sc2721_data },
	{ .compatible = "sprd,ump9620-vibrator", .data = &ump9620_data },
	{}
};
MODULE_DEVICE_TABLE(of, sc27xx_vibra_of_match);

static struct platform_driver sc27xx_vibra_driver = {
	.driver = {
		.name = "sc27xx-vibrator",
		.of_match_table = sc27xx_vibra_of_match,
	},
	.probe = sc27xx_vibra_probe,
	.remove = vib_remove,
	.shutdown = vib_shutdown,
};

module_platform_driver(sc27xx_vibra_driver);

MODULE_DESCRIPTION("Spreadtrum SC27xx Vibrator Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("zhiyun zhu<zhiyun.zhu@unisoc.com>");
