/*
 * opa1622.c  --  PA driver for OPA1622
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Author: Nannan Wang <wangnannan@xiaomi.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#define DEBUG
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <sound/soc.h>
#include <asm/bootinfo.h>

struct opa1622_priv {
	struct snd_soc_codec *codec;
	struct regulator *power;
	struct regulator *aux_power;
	int enable_gpio;
	int mute_gpio;
	int switch_gpio;
	int port1_enabled;
	int port2_enabled;
};


static void opa1622_power(struct opa1622_priv *opa1622, bool on)
{
	struct snd_soc_codec *codec = opa1622->codec;
	int ret;

	if (opa1622->power) {
		if (on) {
			ret = regulator_enable(opa1622->power);
			if (ret < 0) {
				dev_err(codec->dev, "%s: Failed to enable power(%d)\n",
					__func__, ret);
			}
		} else {
			ret = regulator_disable(opa1622->power);
			if (ret < 0) {
				dev_err(codec->dev, "%s: Failed to disable power(%d)\n",
					__func__, ret);
			}
		}
	}

	if (opa1622->aux_power) {
		if (on) {
			ret = regulator_enable(opa1622->aux_power);
			if (ret < 0) {
				dev_err(codec->dev, "%s: Failed to enable aux power(%d)\n",
					__func__, ret);
			}
		} else {
			ret = regulator_disable(opa1622->aux_power);
			if (ret < 0) {
				dev_err(codec->dev, "%s: Failed to disable aux power(%d)\n",
					__func__, ret);
			}
		}
	}
}

static void opa1622_enable(struct opa1622_priv *opa1622, int enable)
{
	if (gpio_is_valid(opa1622->enable_gpio))
		gpio_direction_output(opa1622->enable_gpio, enable);
}

static void opa1622_mute(struct opa1622_priv *opa1622, int mute)
{
	if (gpio_is_valid(opa1622->mute_gpio))
		gpio_direction_output(opa1622->mute_gpio, mute);
}

static void opa1622_switch(struct opa1622_priv *opa1622, int enable)
{
	if (gpio_is_valid(opa1622->switch_gpio))
		gpio_direction_output(opa1622->switch_gpio, enable);
}

static void opa1622_startup(struct opa1622_priv *opa1622, int enable)
{
	if (enable) {
		opa1622_mute(opa1622, 1);
		opa1622_power(opa1622, true);
		opa1622_enable(opa1622, 1);
		opa1622_switch(opa1622, 1);
		opa1622_mute(opa1622, 0);
	} else {
		opa1622_mute(opa1622, 1);
		opa1622_switch(opa1622, 0);
		opa1622_enable(opa1622, 0);
		opa1622_power(opa1622, false);
		opa1622_mute(opa1622, 0);
	}
}

static int opa1622_probe(struct snd_soc_codec *codec)
{
	struct opa1622_priv *opa1622;
	int ret = 0;

	dev_dbg(codec->dev, "%s: enter\n", __func__);
	opa1622 = kzalloc(sizeof(struct opa1622_priv), GFP_KERNEL);
	if (opa1622 == NULL) {
		dev_err(codec->dev, "%s: Failed to alloc opa1622_priv\n", __func__);
		return -ENOMEM;
	}
	memset(opa1622, 0, sizeof(struct opa1622_priv));

	opa1622->enable_gpio = of_get_named_gpio(codec->dev->of_node,
				"opa-en-gpio", 0);
	dev_dbg(codec->dev, "%s: opa enable gpio %d\n", __func__, opa1622->enable_gpio);
	if (gpio_is_valid(opa1622->enable_gpio)) {
		ret = gpio_request(opa1622->enable_gpio, "opa1622 enable");
		if (ret < 0) {
			dev_err(codec->dev, "%s: Failed to request enable gpio %d\n",
				__func__, ret);
			goto opa1622_free;
		}
	} else {
		ret = opa1622->enable_gpio;
		dev_err(codec->dev, "%s: Failed to parse enable-gpio(%d)\n",
			__func__, ret);
		goto opa1622_free;
	}
	opa1622_enable(opa1622, 0);

	opa1622->mute_gpio = of_get_named_gpio(codec->dev->of_node,
				"mute-gpio", 0);
	dev_dbg(codec->dev, "%s: mute gpio %d\n", __func__, opa1622->mute_gpio);
	if (gpio_is_valid(opa1622->mute_gpio)) {
		ret = gpio_request(opa1622->mute_gpio, "opa1622 mute");
		if (ret < 0) {
			dev_err(codec->dev, "%s: Failed to request mute gpio %d\n",
				__func__, ret);
			goto enable_free;
		}
	} else {
		ret = opa1622->mute_gpio;
		dev_err(codec->dev, "%s: Failed to parse mute-gpio(%d)\n",
			__func__, ret);
		goto enable_free;
	}
	opa1622_mute(opa1622, 0);

	opa1622->switch_gpio = of_get_named_gpio(codec->dev->of_node,
				"switch-gpio", 0);
	dev_dbg(codec->dev, "%s: switch gpio %d\n", __func__, opa1622->switch_gpio);
	if (gpio_is_valid(opa1622->switch_gpio)) {
		ret = gpio_request(opa1622->switch_gpio, "opa1622 switch");
		if (ret < 0) {
			dev_err(codec->dev, "%s: Failed to request switch gpio %d\n",
				__func__, ret);
			goto mute_free;
		}
	} else {
		ret = opa1622->switch_gpio;
		dev_err(codec->dev, "%s: Failed to parse switch-gpio(%d)\n",
			__func__, ret);
		goto mute_free;
	}
	opa1622_switch(opa1622, 0);

	/* initialize power supply */
	if ((get_hw_version_devid() == 4) &&
	    (get_hw_version() &
	     (HW_MAJOR_VERSION_MASK | HW_MINOR_VERSION_MASK)) <= 0x20)
		opa1622->power = regulator_get(codec->dev, "opa-p2-power");
	else
		opa1622->power = regulator_get(codec->dev, "opa-power");

	if (IS_ERR(opa1622->power)) {
		dev_info(codec->dev, "opa power can't be found\n");
		opa1622->power = NULL;
	}

	if ((get_hw_version_devid() == 8) &&
	    (get_hw_version() &
	     (HW_MAJOR_VERSION_MASK | HW_MINOR_VERSION_MASK)) >= 0x30) {
		opa1622->aux_power = NULL;
	} else {
		opa1622->aux_power = regulator_get(codec->dev, "opa-power-aux");
		if (IS_ERR(opa1622->aux_power)) {
			dev_info(codec->dev, "opa aux power can't be found\n");
			opa1622->aux_power = NULL;
		}
	}

	snd_soc_dapm_ignore_suspend(&codec->dapm, "OPA IN1");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "OPA IN2");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "OPA OUT1");
	snd_soc_dapm_ignore_suspend(&codec->dapm, "OPA OUT2");

	opa1622->codec = codec;
	snd_soc_codec_set_drvdata(codec, opa1622);
	return 0;

mute_free:
	gpio_free(opa1622->mute_gpio);
enable_free:
	gpio_free(opa1622->enable_gpio);
opa1622_free:
	kfree(opa1622);
	opa1622 = NULL;
	return ret;
}

static int opa1622_remove(struct snd_soc_codec *codec)
{
	struct opa1622_priv *opa1622 = snd_soc_codec_get_drvdata(codec);

	if (opa1622->power)
		regulator_put(opa1622->power);
	if (opa1622->aux_power)
		regulator_put(opa1622->aux_power);

	if (gpio_is_valid(opa1622->enable_gpio))
		gpio_free(opa1622->enable_gpio);
	if (gpio_is_valid(opa1622->mute_gpio))
		gpio_free(opa1622->mute_gpio);
	if (gpio_is_valid(opa1622->switch_gpio))
		gpio_free(opa1622->switch_gpio);

	kfree(opa1622);
	return 0;
}

static int opa1622_pa_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct opa1622_priv *opa1622 = snd_soc_codec_get_drvdata(codec);
	int port;

	dev_dbg(codec->dev, "%s: %s %d\n", __func__, w->name, event);
	if (!(strcmp(w->name, "OPA PGA1")))
		port = 1;
	else
		port = 2;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		if (!opa1622->port1_enabled && !opa1622->port2_enabled)
			opa1622_startup(opa1622, 1);
		if (port == 1)
			opa1622->port1_enabled = 1;
		else
			opa1622->port2_enabled = 1;
		break;
	case SND_SOC_DAPM_POST_PMD:
		if ((opa1622->port1_enabled | opa1622->port2_enabled) == 1 &&
			(opa1622->port1_enabled & opa1622->port2_enabled) == 0)
			opa1622_startup(opa1622, 0);
		if (port == 1)
			opa1622->port1_enabled = 0;
		else
			opa1622->port2_enabled = 0;
		break;
	default:
		break;
	}

	return 0;
}

static const struct snd_kcontrol_new opa1622_pa_port1[] = {
	SOC_DAPM_SINGLE("Switch1", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_kcontrol_new opa1622_pa_port2[] = {
	SOC_DAPM_SINGLE("Switch2", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_soc_dapm_widget opa1622_widgets[] = {
	SND_SOC_DAPM_INPUT("IN1"),
	SND_SOC_DAPM_INPUT("IN2"),
	SND_SOC_DAPM_MIXER("Port1", SND_SOC_NOPM, 0, 0,
			opa1622_pa_port1, ARRAY_SIZE(opa1622_pa_port1)),
	SND_SOC_DAPM_MIXER("Port2", SND_SOC_NOPM, 0, 0,
			opa1622_pa_port2, ARRAY_SIZE(opa1622_pa_port2)),
	SND_SOC_DAPM_PGA_E("PGA1", SND_SOC_NOPM, 0, 0, NULL, 0,
			opa1622_pa_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_PGA_E("PGA2", SND_SOC_NOPM, 0, 0, NULL, 0,
			opa1622_pa_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_OUTPUT("OUT1"),
	SND_SOC_DAPM_OUTPUT("OUT2"),
};

static const struct snd_soc_dapm_route opa1622_routes[] = {
	{"Port1", "Switch1", "IN1"},
	{"Port2", "Switch2", "IN2"},
	{"PGA1", NULL, "Port1"},
	{"PGA2", NULL, "Port2"},
	{"OUT1", NULL, "PGA1"},
	{"OUT2", NULL, "PGA2"},
};

static struct snd_soc_codec_driver opa1622_drv = {
	.probe = opa1622_probe,
	.remove = opa1622_remove,
	.dapm_widgets = opa1622_widgets,
	.num_dapm_widgets = ARRAY_SIZE(opa1622_widgets),
	.dapm_routes = opa1622_routes,
	.num_dapm_routes = ARRAY_SIZE(opa1622_routes),
};

static int opa1622_pa_probe(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s: enter\n", __func__);
	return snd_soc_register_codec(&pdev->dev, &opa1622_drv,
					 NULL, 0);
}

static int opa1622_pa_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static int opa1622_pa_suspend(struct device *dev)
{
	dev_dbg(dev, "%s: system suspend\n", __func__);
	return 0;
}

static int opa1622_pa_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct opa1622_priv *opa1622 = platform_get_drvdata(pdev);

	if (!opa1622) {
		dev_err(dev, "%s: opa1622 private data is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(dev, "%s: system resume\n", __func__);
	return 0;
}

static const struct dev_pm_ops opa1622_pa_pm_ops = {
	.suspend = opa1622_pa_suspend,
	.resume = opa1622_pa_resume,
};


static const struct of_device_id of_opa1622_pa_match[] = {
	{.compatible = "ti,opa1622",},
	{},
};

static struct platform_driver opa1622_pa_driver = {
	.probe  = opa1622_pa_probe,
	.remove = opa1622_pa_remove,
	.driver = {
		.name  = "opa1622",
		.owner = THIS_MODULE,
		.pm = &opa1622_pa_pm_ops,
		.of_match_table = of_opa1622_pa_match,
	},
};

static int __init opa1622_pa_init(void)
{
	return platform_driver_register(&opa1622_pa_driver);
}

static void __exit opa1622_pa_exit(void)
{
	platform_driver_unregister(&opa1622_pa_driver);
}

module_init(opa1622_pa_init);
module_exit(opa1622_pa_exit);

MODULE_AUTHOR("Nannan Wang <wangnannan@xiaomi.com>");
MODULE_DESCRIPTION("OPA1622 Audio Operational Amplifier Driver");
MODULE_LICENSE("GPL");
