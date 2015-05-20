/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <dt-bindings/clock/msm-clocks-8952.h>

struct audio_ext_ap_clk {
	bool enabled;
	int gpio;
	struct clk c;
};

struct audio_ext_pmi_clk {
	int gpio;
	struct clk c;
};

static inline struct audio_ext_ap_clk *to_audio_ap_clk(struct clk *clk)
{
	return container_of(clk, struct audio_ext_ap_clk, c);
}

static int audio_ext_clk_prepare(struct clk *clk)
{
	struct audio_ext_ap_clk *audio_clk = to_audio_ap_clk(clk);

	pr_debug("%s: gpio: %d\n", __func__, audio_clk->gpio);
	return gpio_direction_output(audio_clk->gpio, 1);
}

static void audio_ext_clk_unprepare(struct clk *clk)
{
	struct audio_ext_ap_clk *audio_clk = to_audio_ap_clk(clk);

	pr_debug("%s: gpio: %d\n", __func__, audio_clk->gpio);
	gpio_direction_output(audio_clk->gpio, 0);

}

static struct clk_ops audio_ext_ap_clk_ops = {
	.prepare = audio_ext_clk_prepare,
	.unprepare = audio_ext_clk_unprepare,
};

static struct audio_ext_pmi_clk audio_pmi_clk = {
	.c = {
		.dbg_name = "audio_ext_pmi_clk",
		.ops = &clk_ops_dummy,
		CLK_INIT(audio_pmi_clk.c),
	},
};

static struct audio_ext_ap_clk audio_ap_clk = {
	.c = {
		.dbg_name = "audio_ext_ap_clk",
		.ops = &audio_ext_ap_clk_ops,
		CLK_INIT(audio_ap_clk.c),
	},
};

static struct clk_lookup audio_ref_clock[] = {
	CLK_LIST(audio_ap_clk),
	CLK_LIST(audio_pmi_clk),
};

static int audio_ref_clk_probe(struct platform_device *pdev)
{
	int clk_gpio;
	int ret;
	struct clk *div_clk1;

	clk_gpio = of_get_named_gpio(pdev->dev.of_node,
				     "qcom,audio-ref-clk-gpio", 0);
	if (clk_gpio < 0) {
		dev_err(&pdev->dev,
			"Looking up %s property in node %s failed %d\n",
			"qcom,audio-ref-clk-gpio",
			pdev->dev.of_node->full_name,
			clk_gpio);
		ret = -EINVAL;
		goto err;
	}
	ret = gpio_request(clk_gpio, "EXT_CLK");
	if (ret) {
		dev_err(&pdev->dev,
			"Request ext clk gpio failed %d, err:%d\n",
			clk_gpio, ret);
		goto err;
	}
	if (of_property_read_bool(pdev->dev.of_node,
				  "qcom,node_has_rpm_clock")) {
		div_clk1 = clk_get(&pdev->dev, "osr_clk");
		if (IS_ERR(div_clk1)) {
			dev_err(&pdev->dev, "Failed to get RPM div clk\n");
			ret = PTR_ERR(div_clk1);
			goto err_gpio;
		}
		audio_pmi_clk.c.parent = div_clk1;
		audio_pmi_clk.gpio = clk_gpio;
	} else
		audio_ap_clk.gpio = clk_gpio;

	ret = of_msm_clock_register(pdev->dev.of_node, audio_ref_clock,
			      ARRAY_SIZE(audio_ref_clock));
	if (ret) {
		dev_err(&pdev->dev, "%s: audio ref clock register failed\n",
			__func__);
		goto err_gpio;
	}

	return 0;

err_gpio:
	gpio_free(clk_gpio);

err:
	return ret;
}

static int audio_ref_clk_remove(struct platform_device *pdev)
{
	if (audio_pmi_clk.gpio > 0)
		gpio_free(audio_pmi_clk.gpio);
	else if (audio_ap_clk.gpio > 0)
		gpio_free(audio_ap_clk.gpio);

	return 0;
}

static const struct of_device_id audio_ref_clk_match[] = {
	{.compatible = "qcom,audio-ref-clk"},
	{}
};
MODULE_DEVICE_TABLE(of, audio_ref_clk_match);

static struct platform_driver audio_ref_clk_driver = {
	.driver = {
		.name = "audio-ref-clk",
		.owner = THIS_MODULE,
		.of_match_table = audio_ref_clk_match,
	},
	.probe = audio_ref_clk_probe,
	.remove = audio_ref_clk_remove,
};

static int __init audio_ref_clk_platform_init(void)
{
	return platform_driver_register(&audio_ref_clk_driver);
}
module_init(audio_ref_clk_platform_init);

static void __exit audio_ref_clk_platform_exit(void)
{
	platform_driver_unregister(&audio_ref_clk_driver);
}
module_exit(audio_ref_clk_platform_exit);

MODULE_DESCRIPTION("Audio Ref Clock module platform driver");
MODULE_LICENSE("GPL v2");
