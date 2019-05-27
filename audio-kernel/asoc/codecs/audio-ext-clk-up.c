/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
	PMI_CLK,
	LPASS_MCLK,
	LPASS_MCLK2,
	LNBB_CLK,
};

struct pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *sleep;
	struct pinctrl_state *active;
	char __iomem *base;
};

struct audio_ext_pmi_clk {
	struct pinctrl_info pnctrl_info;
	struct clk_fixed_factor fact;
};

struct audio_ext_lpass_mclk {
	struct pinctrl_info pnctrl_info;
	struct clk_fixed_factor fact;
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

static inline struct audio_ext_pmi_clk *to_audio_pmi_clk(struct clk_hw *hw)
{
	return container_of(hw, struct audio_ext_pmi_clk, fact.hw);
}

static int audio_ext_pmi_clk_prepare(struct clk_hw *hw)
{
	struct audio_ext_pmi_clk *audio_pmi_clk = to_audio_pmi_clk(hw);
	struct pinctrl_info *pnctrl_info = &audio_pmi_clk->pnctrl_info;
	int ret;

	if (!pnctrl_info->pinctrl || !pnctrl_info->active) {
		pr_err("%s: pinctrl state not defined\n", __func__);
		return -EINVAL;
	}

	ret = pinctrl_select_state(pnctrl_info->pinctrl,
				   pnctrl_info->active);
	if (ret) {
		pr_err("%s: active state select failed with %d\n",
			__func__, ret);
		return -EIO;
	}
	return 0;
}

static void audio_ext_pmi_clk_unprepare(struct clk_hw *hw)
{
	struct audio_ext_pmi_clk *audio_pmi_clk = to_audio_pmi_clk(hw);
	struct pinctrl_info *pnctrl_info = &audio_pmi_clk->pnctrl_info;
	int ret = 0;

	if (!pnctrl_info->pinctrl || !pnctrl_info->sleep) {
		pr_err("%s: pinctrl state not defined\n", __func__);
		return;
	}

	ret = pinctrl_select_state(pnctrl_info->pinctrl,
				   pnctrl_info->sleep);
	if (ret)
		pr_err("%s: sleep state select failed with %d\n",
			__func__, ret);
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

static const struct clk_ops audio_ext_lpass_mclk_ops = {
	.prepare = audio_ext_lpass_mclk_prepare,
	.unprepare = audio_ext_lpass_mclk_unprepare,
};

static const struct clk_ops audio_ext_lpass_mclk2_ops = {
	.prepare = audio_ext_lpass_mclk2_prepare,
	.unprepare = audio_ext_lpass_mclk2_unprepare,
};

static const struct clk_ops audio_ext_pmi_clk_ops = {
	.prepare = audio_ext_pmi_clk_prepare,
	.unprepare = audio_ext_pmi_clk_unprepare,
};

static struct audio_ext_pmi_clk audio_pmi_clk = {
	.pnctrl_info = {NULL},
	.fact = {
		.mult = 1,
		.div = 1,
		.hw.init = &(struct clk_init_data){
			.name = "audio_ext_pmi_clk",
			.parent_names = (const char *[]){ "qpnp_clkdiv_1" },
			.num_parents = 1,
			.ops = &audio_ext_pmi_clk_ops,
		},
	},
};

static struct audio_ext_pmi_clk audio_pmi_lnbb_clk = {
	.pnctrl_info = {NULL},
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
	case LPASS_MCLK:
		pnctrl_info = &audio_lpass_mclk.pnctrl_info;
		break;
	case LPASS_MCLK2:
		pnctrl_info = &audio_lpass_mclk2.pnctrl_info;
		break;
	case PMI_CLK:
		pnctrl_info = &audio_pmi_clk.pnctrl_info;
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

static void audio_ref_update_afe_mclk_id(uint32_t mclk_id,
					 enum audio_clk_mux mux)
{
	uint32_t *clk_id;

	switch (mux) {
	case LPASS_MCLK:
		clk_id = &lpass_mclk.clk_id;
		break;
	default:
		pr_err("%s: Not a valid MUX ID: %d\n", __func__, mux);
		return;
	}

	*clk_id = mclk_id;
	pr_debug("%s: clk_id = 0x%x\n", __func__, *clk_id);
}

static int audio_clk_register(struct device *dev,
			       enum audio_clk_mux mux)
{
	struct clk *audio_clk;
	struct clk_hw **audio_clk_hw;
	struct clk_onecell_data *clk_data;
	int i, ret = 0;

	clk_data = devm_kzalloc(dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	switch (mux) {
	case PMI_CLK:
		clk_data->clk_num = 1;
		audio_clk_hw = &audio_msm_hws[mux];
		break;
	case LPASS_MCLK:
		clk_data->clk_num = ARRAY_SIZE(audio_msm_hws) - 1;
		audio_clk_hw = &audio_msm_hws[mux];
		break;
	case LNBB_CLK:
		clk_data->clk_num = ARRAY_SIZE(audio_msm_hws1);
		audio_clk_hw = &audio_msm_hws1[0];
		break;
	default:
		dev_err(dev, "%s: Not a valid MUX ID: %d\n", __func__, mux);
		ret = -EINVAL;
		goto err_clk;
	}

	clk_data->clks = devm_kzalloc(dev, clk_data->clk_num *
				sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks) {
		ret = -ENOMEM;
		goto err_clk;
	}
	for (i = 0; i < clk_data->clk_num; i++) {
		audio_clk = devm_clk_register(dev, audio_clk_hw[i]);
		if (IS_ERR(audio_clk)) {
			dev_err(dev, "%s: ref clock: %d register failed\n",
				__func__, i);
			ret = PTR_ERR(audio_clk);
			goto err_register;
		}
		clk_data->clks[i] = audio_clk;
	}

	ret = of_clk_add_provider(dev->of_node,
				of_clk_src_onecell_get, clk_data);
	if (ret) {
		dev_err(dev, "%s: audio ref clock register failed\n",
			__func__);
		goto err_register;
	}
	return 0;

err_register:
	devm_kfree(dev, clk_data->clks);
err_clk:
	devm_kfree(dev, clk_data);
	return ret;
}

static int audio_ref_clk_probe(struct platform_device *pdev)
{
	int clk_gpio;
	int ret;
	u32 mclk_freq, mclk_id;
	struct device *dev = &pdev->dev;

	ret = of_property_read_u32(pdev->dev.of_node,
				"qcom,lpass-mclk-id", &mclk_id);
	if (ret)
		dev_dbg(&pdev->dev, "%s:lpass-mclk_id not present %d\n",
				__func__, ret);
	else
		audio_ref_update_afe_mclk_id(mclk_id, LPASS_MCLK);

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,codec-mclk-clk-freq",
			&mclk_freq);
	if (!ret)
		lpass_mclk.clk_freq_in_hz = mclk_freq;

	clk_gpio = of_get_named_gpio(pdev->dev.of_node,
				     "qcom,audio-ref-clk-gpio", 0);
	if (clk_gpio > 0) {
		if (of_property_read_bool(pdev->dev.of_node,
				"qcom,node_has_rpm_clock")) {
			ret = audio_get_pinctrl(pdev, PMI_CLK);
			if (ret) {
				dev_err(dev, "%s: Parsing pinctrl %s failed\n",
					__func__, "PMI_CLK");
				goto err_clk_register;
			}
			ret = audio_clk_register(dev, PMI_CLK);
		} else {
			ret = audio_get_pinctrl(pdev, LPASS_MCLK);
			if (ret)
				dev_dbg(dev, "%s: Parsing pinctrl %s failed\n",
					__func__, "LPASS_MCLK");
			ret = audio_get_pinctrl(pdev, LPASS_MCLK2);
			if (ret)
				dev_dbg(dev, "%s: Parsing pinctrl %s failed\n",
					__func__, "LPASS_MCLK2");
			ret = audio_clk_register(dev, LPASS_MCLK);
		}
	} else {
		ret = audio_clk_register(dev, LNBB_CLK);
	}

	if (ret) {
		dev_err(&pdev->dev, "%s: clock register failed\n", __func__);
		goto err_clk_register;
	}

err_clk_register:
	return ret;
}

static int audio_ref_clk_remove(struct platform_device *pdev)
{
	struct pinctrl_info *pmi_pnctrl_info = &audio_pmi_clk.pnctrl_info;
	struct pinctrl_info *lpass_pnctrl_info = &audio_lpass_mclk.pnctrl_info;
	struct pinctrl_info *lpass2_pnctrl_info =
						&audio_lpass_mclk2.pnctrl_info;

	if (pmi_pnctrl_info->pinctrl) {
		devm_pinctrl_put(pmi_pnctrl_info->pinctrl);
		pmi_pnctrl_info->pinctrl = NULL;
	}

	if (lpass_pnctrl_info->pinctrl) {
		devm_pinctrl_put(lpass_pnctrl_info->pinctrl);
		lpass_pnctrl_info->pinctrl = NULL;
	}

	if (lpass2_pnctrl_info->pinctrl) {
		devm_pinctrl_put(lpass2_pnctrl_info->pinctrl);
		lpass2_pnctrl_info->pinctrl = NULL;
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
