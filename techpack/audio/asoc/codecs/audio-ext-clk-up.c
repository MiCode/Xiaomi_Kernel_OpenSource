/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/clk-provider.h>
#include "../../../drivers/clk/qcom/common.h"
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <dt-bindings/clock/qcom,audio-ext-clk.h>
#include <dsp/q6afe-v2.h>
#include "audio-ext-clk-up.h"

enum audio_clk_mux {
	AP_CLK2,
	LPASS_MCLK,
	LPASS_MCLK2,
};

struct pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *sleep;
	struct pinctrl_state *active;
	char __iomem *base;
};

struct audio_ext_ap_clk {
	bool enabled;
	int gpio;
	struct clk_fixed_factor fact;
};

struct audio_ext_pmi_clk {
	int gpio;
	struct clk_fixed_factor fact;
};

struct audio_ext_ap_clk2 {
	bool enabled;
	struct pinctrl_info pnctrl_info;
	struct clk_fixed_factor fact;
};

struct audio_ext_lpass_mclk {
	struct pinctrl_info pnctrl_info;
	struct clk_fixed_factor fact;
};

static struct afe_clk_set clk2_config = {
	Q6AFE_LPASS_CLK_CONFIG_API_VERSION,
	Q6AFE_LPASS_CLK_ID_SPEAKER_I2S_OSR,
	Q6AFE_LPASS_IBIT_CLK_11_P2896_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static struct afe_clk_set lpass_default = {
	Q6AFE_LPASS_CLK_CONFIG_API_VERSION,
	Q6AFE_LPASS_CLK_ID_SPEAKER_I2S_OSR,
	Q6AFE_LPASS_IBIT_CLK_11_P2896_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static struct afe_clk_set lpass_mclk = {
	Q6AFE_LPASS_CLK_CONFIG_API_VERSION,
	Q6AFE_LPASS_CLK_ID_MCLK_3,
	Q6AFE_LPASS_OSR_CLK_9_P600_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static inline struct audio_ext_ap_clk *to_audio_ap_clk(struct clk_hw *hw)
{
	return container_of(hw, struct audio_ext_ap_clk, fact.hw);
}

static int audio_ext_clk_prepare(struct clk_hw *hw)
{
	struct audio_ext_ap_clk *audio_clk = to_audio_ap_clk(hw);

	pr_debug("%s: gpio: %d\n", __func__, audio_clk->gpio);
	if (gpio_is_valid(audio_clk->gpio))
		return gpio_direction_output(audio_clk->gpio, 1);
	return 0;
}

static void audio_ext_clk_unprepare(struct clk_hw *hw)
{
	struct audio_ext_ap_clk *audio_clk = to_audio_ap_clk(hw);

	pr_debug("%s: gpio: %d\n", __func__, audio_clk->gpio);
	if (gpio_is_valid(audio_clk->gpio))
		gpio_direction_output(audio_clk->gpio, 0);
}

static inline struct audio_ext_ap_clk2 *to_audio_ap_clk2(struct clk_hw *hw)
{
	return container_of(hw, struct audio_ext_ap_clk2, fact.hw);
}

static int audio_ext_clk2_prepare(struct clk_hw *hw)
{
	struct audio_ext_ap_clk2 *audio_clk2 = to_audio_ap_clk2(hw);
	struct pinctrl_info *pnctrl_info = &audio_clk2->pnctrl_info;
	int ret;


	if (!pnctrl_info->pinctrl || !pnctrl_info->active)
		return 0;

	ret = pinctrl_select_state(pnctrl_info->pinctrl,
				   pnctrl_info->active);
	if (ret) {
		pr_err("%s: active state select failed with %d\n",
			__func__, ret);
		return -EIO;
	}

	clk2_config.enable = 1;
	ret = afe_set_lpass_clk_cfg(IDX_RSVD_3, &clk2_config);
	if (ret < 0) {
		pr_err("%s: failed to set clock, ret = %d\n", __func__, ret);
		return -EINVAL;
	}

	return 0;
}

static void audio_ext_clk2_unprepare(struct clk_hw *hw)
{
	struct audio_ext_ap_clk2 *audio_clk2 = to_audio_ap_clk2(hw);
	struct pinctrl_info *pnctrl_info = &audio_clk2->pnctrl_info;
	int ret;

	if (!pnctrl_info->pinctrl || !pnctrl_info->sleep)
		return;

	ret = pinctrl_select_state(pnctrl_info->pinctrl,
				   pnctrl_info->sleep);
	if (ret)
		pr_err("%s: sleep state select failed with %d\n",
			__func__, ret);

	clk2_config.enable = 0;
	ret = afe_set_lpass_clk_cfg(IDX_RSVD_3, &clk2_config);
	if (ret < 0)
		pr_err("%s: failed to reset clock, ret = %d\n", __func__, ret);
}

static inline struct audio_ext_lpass_mclk *to_audio_lpass_mclk(
						struct clk_hw *hw)
{
	return container_of(hw, struct audio_ext_lpass_mclk, fact.hw);
}

static int audio_ext_lpass_mclk_prepare(struct clk_hw *hw)
{
	struct audio_ext_lpass_mclk *audio_lpass_mclk = to_audio_lpass_mclk(hw);
	struct pinctrl_info *pnctrl_info = &audio_lpass_mclk->pnctrl_info;
	int ret;

	lpass_mclk.enable = 1;
	ret = afe_set_lpass_clock_v2(AFE_PORT_ID_PRIMARY_MI2S_RX,
				&lpass_mclk);
	if (ret < 0) {
		pr_err("%s afe_set_digital_codec_core_clock failed\n",
			__func__);
		return ret;
	}

	if (pnctrl_info->pinctrl) {
		ret = pinctrl_select_state(pnctrl_info->pinctrl,
				pnctrl_info->active);
		if (ret) {
			pr_err("%s: active state select failed with %d\n",
				__func__, ret);
			return -EIO;
		}
	}

	if (pnctrl_info->base)
		iowrite32(1, pnctrl_info->base);
	return 0;
}

static void audio_ext_lpass_mclk_unprepare(struct clk_hw *hw)
{
	struct audio_ext_lpass_mclk *audio_lpass_mclk = to_audio_lpass_mclk(hw);
	struct pinctrl_info *pnctrl_info = &audio_lpass_mclk->pnctrl_info;
	int ret;

	if (pnctrl_info->pinctrl) {
		ret = pinctrl_select_state(pnctrl_info->pinctrl,
					   pnctrl_info->sleep);
		if (ret) {
			pr_err("%s: active state select failed with %d\n",
				__func__, ret);
			return;
		}
	}

	lpass_mclk.enable = 0;
	ret = afe_set_lpass_clock_v2(AFE_PORT_ID_PRIMARY_MI2S_RX,
			&lpass_mclk);
	if (ret < 0)
		pr_err("%s: afe_set_digital_codec_core_clock failed, ret = %d\n",
			__func__, ret);
	if (pnctrl_info->base)
		iowrite32(0, pnctrl_info->base);
}

static int audio_ext_lpass_mclk2_prepare(struct clk_hw *hw)
{
	struct audio_ext_lpass_mclk *audio_lpass_mclk2 =
					to_audio_lpass_mclk(hw);
	struct pinctrl_info *pnctrl_info = &audio_lpass_mclk2->pnctrl_info;
	int ret;

	if (pnctrl_info->pinctrl) {
		ret = pinctrl_select_state(pnctrl_info->pinctrl,
					   pnctrl_info->active);
		if (ret) {
			pr_err("%s: active state select failed with %d\n",
				__func__, ret);
			return -EIO;
		}
	}

	lpass_default.enable = 1;
	ret = afe_set_lpass_clk_cfg(IDX_RSVD_3, &lpass_default);
	if (ret < 0) {
		pr_err("%s: failed to set clock, ret = %d\n", __func__, ret);
		return -EINVAL;
	}

	return 0;
}

static void audio_ext_lpass_mclk2_unprepare(struct clk_hw *hw)
{
	struct audio_ext_lpass_mclk *audio_lpass_mclk2 =
					to_audio_lpass_mclk(hw);
	struct pinctrl_info *pnctrl_info = &audio_lpass_mclk2->pnctrl_info;
	int ret;

	if (pnctrl_info->pinctrl) {
		ret = pinctrl_select_state(pnctrl_info->pinctrl,
					   pnctrl_info->sleep);
		if (ret)
			pr_err("%s: sleep state select failed with %d\n",
				__func__, ret);
	}

	lpass_default.enable = 0;
	ret = afe_set_lpass_clk_cfg(IDX_RSVD_3, &lpass_default);
	if (ret < 0)
		pr_err("%s: failed to reset clock, ret = %d\n", __func__, ret);
}

static const struct clk_ops audio_ext_ap_clk_ops = {
	.prepare = audio_ext_clk_prepare,
	.unprepare = audio_ext_clk_unprepare,
};

static const struct clk_ops audio_ext_ap_clk2_ops = {
	.prepare = audio_ext_clk2_prepare,
	.unprepare = audio_ext_clk2_unprepare,
};

static const struct clk_ops audio_ext_lpass_mclk_ops = {
	.prepare = audio_ext_lpass_mclk_prepare,
	.unprepare = audio_ext_lpass_mclk_unprepare,
};

static const struct clk_ops audio_ext_lpass_mclk2_ops = {
	.prepare = audio_ext_lpass_mclk2_prepare,
	.unprepare = audio_ext_lpass_mclk2_unprepare,
};

static struct audio_ext_pmi_clk audio_pmi_clk = {
	.gpio = -EINVAL,
	.fact = {
		.mult = 1,
		.div = 1,
		.hw.init = &(struct clk_init_data){
			.name = "audio_ext_pmi_clk",
			.parent_names = (const char *[]){ "div_clk1" },
			.num_parents = 1,
			.ops = &clk_dummy_ops,
		},
	},
};

static struct audio_ext_pmi_clk audio_pmi_lnbb_clk = {
	.gpio = -EINVAL,
	.fact = {
		.mult = 1,
		.div = 1,
		.hw.init = &(struct clk_init_data){
			.name = "audio_ext_pmi_lnbb_clk",
			.parent_names = (const char *[]){ "ln_bb_clk2" },
			.num_parents = 1,
			.ops = &clk_dummy_ops,
		},
	},
};

static struct audio_ext_ap_clk audio_ap_clk = {
	.gpio = -EINVAL,
	.fact = {
		.mult = 1,
		.div = 1,
		.hw.init = &(struct clk_init_data){
			.name = "audio_ap_clk",
			.ops = &audio_ext_ap_clk_ops,
		},
	},
};

static struct audio_ext_ap_clk2 audio_ap_clk2 = {
	.enabled = false,
	.pnctrl_info = {NULL},
	.fact = {
		.mult = 1,
		.div = 1,
		.hw.init = &(struct clk_init_data){
			.name = "audio_ap_clk2",
			.ops = &audio_ext_ap_clk2_ops,
		},
	},
};

static struct audio_ext_lpass_mclk audio_lpass_mclk = {
	.pnctrl_info = {NULL},
	.fact = {
		.mult = 1,
		.div = 1,
		.hw.init = &(struct clk_init_data){
			.name = "audio_lpass_mclk",
			.ops = &audio_ext_lpass_mclk_ops,
		},
	},
};

static struct audio_ext_lpass_mclk audio_lpass_mclk2 = {
	.pnctrl_info = {NULL},
	.fact = {
		.mult = 1,
		.div = 1,
		.hw.init = &(struct clk_init_data){
			.name = "audio_lpass_mclk2",
			.ops = &audio_ext_lpass_mclk2_ops,
		},
	},
};

static struct clk_hw *audio_msm_hws[] = {
	&audio_pmi_clk.fact.hw,
	&audio_ap_clk.fact.hw,
	&audio_ap_clk2.fact.hw,
	&audio_lpass_mclk.fact.hw,
	&audio_lpass_mclk2.fact.hw,
};

static struct clk_hw *audio_msm_hws1[] = {
	&audio_pmi_lnbb_clk.fact.hw,
};

static int audio_get_pinctrl(struct platform_device *pdev,
			     enum audio_clk_mux mux)
{
	struct device *dev =  &pdev->dev;
	struct pinctrl_info *pnctrl_info;
	struct pinctrl *pinctrl;
	int ret;
	u32 reg;

	switch (mux) {
	case AP_CLK2:
		pnctrl_info = &audio_ap_clk2.pnctrl_info;
		break;
	case LPASS_MCLK:
		pnctrl_info = &audio_lpass_mclk.pnctrl_info;
		break;
	case LPASS_MCLK2:
		pnctrl_info = &audio_lpass_mclk2.pnctrl_info;
		break;
	default:
		dev_err(dev, "%s Not a valid MUX ID: %d\n",
			__func__, mux);
		return -EINVAL;
	}

	if (pnctrl_info->pinctrl) {
		dev_dbg(dev, "%s: already requested before\n",
			__func__);
		return -EINVAL;
	}

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		dev_dbg(dev, "%s: Unable to get pinctrl handle\n",
			__func__);
		return -EINVAL;
	}
	pnctrl_info->pinctrl = pinctrl;
	/* get all state handles from Device Tree */
	pnctrl_info->sleep = pinctrl_lookup_state(pinctrl, "sleep");
	if (IS_ERR(pnctrl_info->sleep)) {
		dev_err(dev, "%s: could not get sleep pinstate\n",
			__func__);
		goto err;
	}
	pnctrl_info->active = pinctrl_lookup_state(pinctrl, "active");
	if (IS_ERR(pnctrl_info->active)) {
		dev_err(dev, "%s: could not get active pinstate\n",
			__func__);
		goto err;
	}
	/* Reset the TLMM pins to a default state */
	ret = pinctrl_select_state(pnctrl_info->pinctrl,
				   pnctrl_info->sleep);
	if (ret) {
		dev_err(dev, "%s: Disable TLMM pins failed with %d\n",
			__func__, ret);
		goto err;
	}

	ret = of_property_read_u32(dev->of_node, "qcom,mclk-clk-reg", &reg);
	if (ret < 0) {
		dev_dbg(dev, "%s: miss mclk reg\n", __func__);
	} else {
		pnctrl_info->base = ioremap(reg, sizeof(u32));
		if (pnctrl_info->base ==  NULL) {
			dev_err(dev, "%s ioremap failed\n", __func__);
			goto err;
		}
	}

	return 0;

err:
	devm_pinctrl_put(pnctrl_info->pinctrl);
	return -EINVAL;
}

static int audio_ref_clk_probe(struct platform_device *pdev)
{
	int clk_gpio;
	int ret;
	u32 mclk_freq;
	struct clk *audio_clk;
	struct device *dev = &pdev->dev;
	int i;
	struct clk_onecell_data *clk_data;

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,codec-mclk-clk-freq",
			&mclk_freq);
	if (!ret) {
		lpass_mclk.clk_freq_in_hz = mclk_freq;

		ret = audio_get_pinctrl(pdev, LPASS_MCLK);
		if (ret)
			dev_err(&pdev->dev, "%s: Parsing pinctrl %s failed\n",
				__func__, "LPASS_MCLK");
		ret = audio_get_pinctrl(pdev, LPASS_MCLK2);
		if (ret)
			dev_dbg(&pdev->dev, "%s: Parsing pinctrl %s failed\n",
				__func__, "LPASS_MCLK2");
	}

	clk_gpio = of_get_named_gpio(pdev->dev.of_node,
				     "qcom,audio-ref-clk-gpio", 0);
	if (clk_gpio > 0) {
		ret = gpio_request(clk_gpio, "EXT_CLK");
		if (ret) {
			dev_err(&pdev->dev,
				"Request ext clk gpio failed %d, err:%d\n",
				clk_gpio, ret);
			goto err;
		}
		if (of_property_read_bool(pdev->dev.of_node,
					"qcom,node_has_rpm_clock")) {
			audio_pmi_clk.gpio = clk_gpio;
		} else
			audio_ap_clk.gpio = clk_gpio;

	}

	ret = audio_get_pinctrl(pdev, AP_CLK2);
	if (ret)
		dev_dbg(&pdev->dev, "%s: Parsing pinctrl failed\n",
			__func__);

	clk_data = devm_kzalloc(&pdev->dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		goto err_gpio;


	clk_gpio = of_get_named_gpio(pdev->dev.of_node,
				     "qcom,audio-ref-clk-gpio", 0);
	if (clk_gpio > 0) {
		clk_data->clk_num = ARRAY_SIZE(audio_msm_hws);
		clk_data->clks = devm_kzalloc(&pdev->dev,
					clk_data->clk_num *
					sizeof(struct clk *),
					GFP_KERNEL);
		if (!clk_data->clks)
			goto err_clk;

		for (i = 0; i < ARRAY_SIZE(audio_msm_hws); i++) {
			audio_clk = devm_clk_register(dev, audio_msm_hws[i]);
			if (IS_ERR(audio_clk)) {
				dev_err(&pdev->dev,
					"%s: ref clock: %d register failed\n",
					__func__, i);
				return PTR_ERR(audio_clk);
			}
			clk_data->clks[i] = audio_clk;
		}
	} else {
		clk_data->clk_num = ARRAY_SIZE(audio_msm_hws1);
		clk_data->clks = devm_kzalloc(&pdev->dev,
					clk_data->clk_num *
					sizeof(struct clk *),
					GFP_KERNEL);
		if (!clk_data->clks)
			goto err_clk;

		for (i = 0; i < ARRAY_SIZE(audio_msm_hws1); i++) {
			audio_clk = devm_clk_register(dev, audio_msm_hws1[i]);
			if (IS_ERR(audio_clk)) {
				dev_err(&pdev->dev,
					"%s: ref clock: %d register failed\n",
					__func__, i);
				return PTR_ERR(audio_clk);
			}
			clk_data->clks[i] = audio_clk;
		}
	}

	ret = of_clk_add_provider(pdev->dev.of_node,
				of_clk_src_onecell_get, clk_data);
	if (ret) {
		dev_err(&pdev->dev, "%s: audio ref clock register failed\n",
			__func__);
		goto err_gpio;
	}

	return 0;

err_clk:
	if (clk_data)
		devm_kfree(&pdev->dev, clk_data->clks);
	devm_kfree(&pdev->dev, clk_data);
err_gpio:
	gpio_free(clk_gpio);

err:
	return ret;
}

static int audio_ref_clk_remove(struct platform_device *pdev)
{
	struct pinctrl_info *pnctrl_info = &audio_ap_clk2.pnctrl_info;

	if (audio_pmi_clk.gpio > 0)
		gpio_free(audio_pmi_clk.gpio);
	else if (audio_ap_clk.gpio > 0)
		gpio_free(audio_ap_clk.gpio);

	if (pnctrl_info->pinctrl) {
		devm_pinctrl_put(pnctrl_info->pinctrl);
		pnctrl_info->pinctrl = NULL;
	}

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

int audio_ref_clk_platform_init(void)
{
	return platform_driver_register(&audio_ref_clk_driver);
}

void audio_ref_clk_platform_exit(void)
{
	platform_driver_unregister(&audio_ref_clk_driver);
}

MODULE_DESCRIPTION("Audio Ref Up Clock module platform driver");
MODULE_LICENSE("GPL v2");
