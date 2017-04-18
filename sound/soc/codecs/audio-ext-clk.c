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
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <dt-bindings/clock/msm-clocks-8996.h>
#include <dt-bindings/clock/msm-clocks-9650.h>
#include <sound/q6afe-v2.h>

enum clk_mux {
	AP_CLK2,
	LPASS_MCLK,
	LPASS_MCLK2,
};

enum clk_enablement {
	CLK_DISABLE = 0,
	CLK_ENABLE,
};

struct pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *sleep;
	struct pinctrl_state *active;
};

struct audio_ext_ap_clk {
	bool enabled;
	int gpio;
	struct clk c;
};

struct audio_ext_pmi_clk {
	int gpio;
	struct clk c;
};

struct audio_ext_ap_clk2 {
	bool enabled;
	struct pinctrl_info pnctrl_info;
	struct clk c;
};

struct audio_ext_lpass_mclk {
	struct pinctrl_info pnctrl_info;
	struct clk c;
	u32 lpass_clock;
	void __iomem *lpass_csr_gpio_mux_spkrctl_vaddr;
};

static struct afe_clk_set clk2_config = {
	Q6AFE_LPASS_CLK_CONFIG_API_VERSION,
	Q6AFE_LPASS_CLK_ID_SPEAKER_I2S_OSR,
	Q6AFE_LPASS_IBIT_CLK_11_P2896_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static const struct afe_clk_cfg lpass_default = {
	AFE_API_VERSION_I2S_CONFIG,
	Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ,
	Q6AFE_LPASS_OSR_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_SRC_INTERNAL,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	Q6AFE_LPASS_MODE_BOTH_VALID,
	0,
};

static struct afe_clk_set lpass_default2 = {
	Q6AFE_LPASS_CLK_CONFIG_API_VERSION,
	Q6AFE_LPASS_CLK_ID_MCLK_3,
	Q6AFE_LPASS_IBIT_CLK_12_P288_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static struct afe_clk_set digital_cdc_core_clk = {
	Q6AFE_LPASS_CLK_CONFIG_API_VERSION,
	Q6AFE_LPASS_CLK_ID_INTERNAL_DIGITAL_CODEC_CORE,
	Q6AFE_LPASS_OSR_CLK_9_P600_MHZ,
	Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO,
	Q6AFE_LPASS_CLK_ROOT_DEFAULT,
	0,
};

static inline struct audio_ext_ap_clk *to_audio_ap_clk(struct clk *clk)
{
	return container_of(clk, struct audio_ext_ap_clk, c);
}

static int audio_ext_clk_prepare(struct clk *clk)
{
	struct audio_ext_ap_clk *audio_clk = to_audio_ap_clk(clk);

	pr_debug("%s: gpio: %d\n", __func__, audio_clk->gpio);
	if (gpio_is_valid(audio_clk->gpio))
		return gpio_direction_output(audio_clk->gpio, 1);
	return 0;
}

static void audio_ext_clk_unprepare(struct clk *clk)
{
	struct audio_ext_ap_clk *audio_clk = to_audio_ap_clk(clk);

	pr_debug("%s: gpio: %d\n", __func__, audio_clk->gpio);
	if (gpio_is_valid(audio_clk->gpio))
		gpio_direction_output(audio_clk->gpio, 0);
}

static inline struct audio_ext_ap_clk2 *to_audio_ap_clk2(struct clk *clk)
{
	return container_of(clk, struct audio_ext_ap_clk2, c);
}

static int audio_ext_clk2_prepare(struct clk *clk)
{
	struct audio_ext_ap_clk2 *audio_clk2 = to_audio_ap_clk2(clk);
	struct pinctrl_info *pnctrl_info = &audio_clk2->pnctrl_info;
	int ret;

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

static void audio_ext_clk2_unprepare(struct clk *clk)
{
	struct audio_ext_ap_clk2 *audio_clk2 = to_audio_ap_clk2(clk);
	struct pinctrl_info *pnctrl_info = &audio_clk2->pnctrl_info;
	int ret;

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

static int audio_ext_set_lpass_mclk_v1(struct clk *clk,
				       enum clk_enablement enable)
{
	struct audio_ext_lpass_mclk *audio_lpass_mclk;
	struct afe_clk_cfg lpass_clks = lpass_default;
	int val = 0;
	int ret;

	pr_debug("%s: Setting clock using v1, enable(%d)\n", __func__, enable);

	audio_lpass_mclk = container_of(clk, struct audio_ext_lpass_mclk, c);
	if (audio_lpass_mclk == NULL) {
		pr_err("%s: audio_lpass_mclk is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (!audio_lpass_mclk->lpass_clock) {
		lpass_clks.clk_val2 =
			enable ? Q6AFE_LPASS_OSR_CLK_12_P288_MHZ : 0;
		lpass_clks.clk_set_mode = Q6AFE_LPASS_MODE_CLK2_VALID;
		ret = afe_set_lpass_clock(AFE_PORT_ID_SECONDARY_MI2S_RX,
					  &lpass_clks);
		if (ret < 0) {
			pr_err("%s: afe_set_lpass_clock failed with ret = %d\n",
			       __func__, ret);
			ret = -EINVAL;
			goto done;
		}
	} else {
		if (audio_lpass_mclk->lpass_csr_gpio_mux_spkrctl_vaddr &&
		    enable) {
			val = ioread32(audio_lpass_mclk->
					lpass_csr_gpio_mux_spkrctl_vaddr);
			val = val | 0x00000002;
			iowrite32(val, audio_lpass_mclk->
					lpass_csr_gpio_mux_spkrctl_vaddr);
		}

		digital_cdc_core_clk.enable = enable;
		ret = afe_set_lpass_clock_v2(AFE_PORT_ID_PRIMARY_MI2S_RX,
					     &digital_cdc_core_clk);
		if (ret < 0) {
			pr_err("%s: afe_set_digital_codec_core_clock failed with ret %d\n",
			       __func__, ret);
			goto done;
		}
	}

	ret = 0;

done:
	return ret;
}

static int audio_ext_set_lpass_mclk_v2(enum clk_enablement enable)
{
	struct afe_clk_set m_clk = lpass_default2;
	struct afe_clk_set ibit_clk = lpass_default2;
	int ret = 0;

	pr_debug("%s: Setting clock using v2, enable(%d)\n", __func__, enable);

	/* Set both mclk and ibit clocks when using LPASS_CLK_VER_2 */
	m_clk.enable = enable;
	ret = afe_set_lpass_clock_v2(AFE_PORT_ID_PRIMARY_MI2S_RX, &m_clk);
	if (ret < 0) {
		pr_err("%s: afe_set_lpass_clock_v2 failed for mclk_3 with ret %d\n",
		       __func__, ret);
		goto done;
	}

	ibit_clk.clk_id = Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT;
	ibit_clk.clk_freq_in_hz = Q6AFE_LPASS_IBIT_CLK_1_P536_MHZ;
	ibit_clk.enable = enable;
	ret = afe_set_lpass_clock_v2(AFE_PORT_ID_PRIMARY_MI2S_RX, &ibit_clk);
	if (ret < 0) {
		pr_err("%s: afe_set_lpass_clock_v2 failed for ibit with ret %d\n",
		       __func__, ret);
		goto err_ibit_clk_set;
	}

	ret = 0;

done:
	return ret;

err_ibit_clk_set:
	m_clk.enable = CLK_DISABLE;
	if (afe_set_lpass_clock_v2(AFE_PORT_ID_PRIMARY_MI2S_RX, &m_clk)) {
		pr_err("%s: afe_set_lpass_clock_v2 failed to disable mclk_3\n",
		       __func__);
	}
	return ret;
}

static int audio_ext_lpass_mclk_prepare(struct clk *clk)
{
	struct audio_ext_lpass_mclk *audio_lpass_mclk;
	struct pinctrl_info *pnctrl_info;
	enum lpass_clk_ver lpass_clk_ver;
	int ret;

	audio_lpass_mclk = container_of(clk, struct audio_ext_lpass_mclk, c);
	if (audio_lpass_mclk == NULL) {
		pr_err("%s: audio_lpass_mclk is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	pnctrl_info = &audio_lpass_mclk->pnctrl_info;
	if (pnctrl_info && pnctrl_info->pinctrl) {
		ret = pinctrl_select_state(pnctrl_info->pinctrl,
					   pnctrl_info->active);
		if (ret) {
			pr_err("%s: pinctrl active state selection failed with %d\n",
				__func__, ret);
			ret = -EIO;
			goto done;
		}
	}

	lpass_clk_ver = afe_get_lpass_clk_ver();

	if (lpass_clk_ver >= LPASS_CLK_VER_2)
		ret = audio_ext_set_lpass_mclk_v2(CLK_ENABLE);
	else
		ret = audio_ext_set_lpass_mclk_v1(clk, CLK_ENABLE);

done:
	return ret;
}

static void audio_ext_lpass_mclk_unprepare(struct clk *clk)
{
	struct audio_ext_lpass_mclk *audio_lpass_mclk;
	struct pinctrl_info *pnctrl_info;
	enum lpass_clk_ver lpass_clk_ver;
	int ret;

	audio_lpass_mclk = container_of(clk, struct audio_ext_lpass_mclk, c);
	if (audio_lpass_mclk == NULL) {
		pr_err("%s: audio_lpass_mclk is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	pnctrl_info = &audio_lpass_mclk->pnctrl_info;
	if (pnctrl_info && pnctrl_info->pinctrl) {
		ret = pinctrl_select_state(pnctrl_info->pinctrl,
					   pnctrl_info->sleep);
		if (ret) {
			pr_err("%s: pinctrl sleep state selection failed with %d\n",
				__func__, ret);
			ret = -EIO;
			goto done;
		}
	}

	lpass_clk_ver = afe_get_lpass_clk_ver();

	if (lpass_clk_ver >= LPASS_CLK_VER_2)
		ret = audio_ext_set_lpass_mclk_v2(CLK_DISABLE);
	else
		ret = audio_ext_set_lpass_mclk_v1(clk, CLK_DISABLE);

done:
	pr_debug("%s: Unprepare of mclk exiting with %d\n", __func__, ret);
}

static int audio_ext_lpass_mclk2_prepare(struct clk *clk)
{
	struct audio_ext_lpass_mclk *audio_lpass_mclk2;
	struct pinctrl_info *pnctrl_info;
	struct afe_clk_set mclk2 = lpass_default2;
	int ret;

	audio_lpass_mclk2 = container_of(clk, struct audio_ext_lpass_mclk, c);
	pnctrl_info = &audio_lpass_mclk2->pnctrl_info;

	if (pnctrl_info->pinctrl) {
		ret = pinctrl_select_state(pnctrl_info->pinctrl,
					   pnctrl_info->active);
		if (ret) {
			pr_err("%s: active state select failed with %d\n",
				__func__, ret);
			return -EIO;
		}
	}

	mclk2.clk_id = Q6AFE_LPASS_CLK_ID_SPEAKER_I2S_OSR;
	mclk2.enable = 1;
	ret = afe_set_lpass_clk_cfg(IDX_RSVD_3, &mclk2);
	if (ret < 0) {
		pr_err("%s: failed to set clock, ret = %d\n", __func__, ret);
		return -EINVAL;
	}

	return 0;
}

static void audio_ext_lpass_mclk2_unprepare(struct clk *clk)
{
	struct audio_ext_lpass_mclk *audio_lpass_mclk2;
	struct pinctrl_info *pnctrl_info;
	struct afe_clk_set mclk2 = lpass_default2;
	int ret;

	audio_lpass_mclk2 = container_of(clk, struct audio_ext_lpass_mclk, c);
	pnctrl_info = &audio_lpass_mclk2->pnctrl_info;

	if (pnctrl_info->pinctrl) {
		ret = pinctrl_select_state(pnctrl_info->pinctrl,
					   pnctrl_info->sleep);
		if (ret)
			pr_err("%s: sleep state select failed with %d\n",
				__func__, ret);
	}

	mclk2.clk_id = Q6AFE_LPASS_CLK_ID_SPEAKER_I2S_OSR;
	mclk2.enable = 0;
	ret = afe_set_lpass_clk_cfg(IDX_RSVD_3, &mclk2);
	if (ret < 0)
		pr_err("%s: failed to reset clock, ret = %d\n", __func__, ret);
}

static struct clk_ops audio_ext_ap_clk_ops = {
	.prepare = audio_ext_clk_prepare,
	.unprepare = audio_ext_clk_unprepare,
};

static struct clk_ops audio_ext_ap_clk2_ops = {
	.prepare = audio_ext_clk2_prepare,
	.unprepare = audio_ext_clk2_unprepare,
};

static struct clk_ops audio_ext_lpass_mclk_ops = {
	.prepare = audio_ext_lpass_mclk_prepare,
	.unprepare = audio_ext_lpass_mclk_unprepare,
};

static struct clk_ops audio_ext_lpass_mclk2_ops = {
	.prepare = audio_ext_lpass_mclk2_prepare,
	.unprepare = audio_ext_lpass_mclk2_unprepare,
};

static struct audio_ext_pmi_clk audio_pmi_clk = {
	.gpio = -EINVAL,
	.c = {
		.dbg_name = "audio_ext_pmi_clk",
		.ops = &clk_ops_dummy,
		CLK_INIT(audio_pmi_clk.c),
	},
};

static struct audio_ext_ap_clk audio_ap_clk = {
	.gpio = -EINVAL,
	.c = {
		.dbg_name = "audio_ext_ap_clk",
		.ops = &audio_ext_ap_clk_ops,
		CLK_INIT(audio_ap_clk.c),
	},
};

static struct audio_ext_ap_clk2 audio_ap_clk2 = {
	.c = {
		.dbg_name = "audio_ext_ap_clk2",
		.ops = &audio_ext_ap_clk2_ops,
		CLK_INIT(audio_ap_clk2.c),
	},
};

static struct audio_ext_lpass_mclk audio_lpass_mclk = {
	.c = {
		.dbg_name = "audio_ext_lpass_mclk",
		.ops = &audio_ext_lpass_mclk_ops,
		CLK_INIT(audio_lpass_mclk.c),
	},
};

static struct audio_ext_lpass_mclk audio_lpass_mclk2 = {
	.c = {
		.dbg_name = "audio_ext_lpass_mclk2",
		.ops = &audio_ext_lpass_mclk2_ops,
		CLK_INIT(audio_lpass_mclk2.c),
	},
};

static struct clk_lookup audio_ref_clock[] = {
	CLK_LIST(audio_ap_clk),
	CLK_LIST(audio_pmi_clk),
	CLK_LIST(audio_ap_clk2),
	CLK_LIST(audio_lpass_mclk),
	CLK_LIST(audio_lpass_mclk2),
};

static int audio_get_pinctrl(struct platform_device *pdev, enum clk_mux mux)
{
	struct pinctrl_info *pnctrl_info;
	struct pinctrl *pinctrl;
	int ret;

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
		dev_err(&pdev->dev, "%s Not a valid MUX ID: %d\n",
			__func__, mux);
		return -EINVAL;
	}
	pnctrl_info->pinctrl = NULL;
	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		dev_dbg(&pdev->dev, "%s: Unable to get pinctrl handle\n",
			__func__);
		return -EINVAL;
	}
	pnctrl_info->pinctrl = pinctrl;
	/* get all state handles from Device Tree */
	pnctrl_info->sleep = pinctrl_lookup_state(pinctrl, "sleep");
	if (IS_ERR(pnctrl_info->sleep)) {
		dev_err(&pdev->dev, "%s: could not get sleep pinstate\n",
			__func__);
		goto err;
	}
	pnctrl_info->active = pinctrl_lookup_state(pinctrl, "active");
	if (IS_ERR(pnctrl_info->active)) {
		dev_err(&pdev->dev, "%s: could not get active pinstate\n",
			__func__);
		goto err;
	}
	/* Reset the TLMM pins to a default state */
	ret = pinctrl_select_state(pnctrl_info->pinctrl,
				   pnctrl_info->sleep);
	if (ret) {
		dev_err(&pdev->dev, "%s: Disable TLMM pins failed with %d\n",
			__func__, ret);
		goto err;
	}
	return 0;

err:
	devm_pinctrl_put(pnctrl_info->pinctrl);
	return -EINVAL;
}

static void audio_ref_update_afe_mclk_id(const char *ptr, enum clk_mux mux)
{
	uint32_t *clk_id;

	switch (mux) {
	case AP_CLK2:
		clk_id = &clk2_config.clk_id;
		break;
	case LPASS_MCLK:
		clk_id = &digital_cdc_core_clk.clk_id;
		break;
	case LPASS_MCLK2:
		clk_id = &lpass_default2.clk_id;
		break;
	default:
		pr_err("%s Not a valid MUX ID: %d\n", __func__, mux);
		return;
	}

	if (!strcmp(ptr, "pri_mclk")) {
		pr_debug("%s: updating the mclk id with primary mclk\n",
				__func__);
		*clk_id = Q6AFE_LPASS_CLK_ID_MCLK_1;
	} else if (!strcmp(ptr, "sec_mclk")) {
		pr_debug("%s: updating the mclk id with secondary mclk\n",
				__func__);
		*clk_id = Q6AFE_LPASS_CLK_ID_MCLK_2;
	} else {
		pr_debug("%s: updating the mclk id with default\n", __func__);
	}
	pr_debug("%s: clk_id = 0x%x\n", __func__, *clk_id);
}

static int audio_ref_clk_probe(struct platform_device *pdev)
{
	int clk_gpio;
	int ret;
	struct clk *div_clk1;
	u32 mclk_freq;
	const char *mclk_id = "qcom,lpass-mclk-id";
	const char *mclk_str = NULL;
	u32 lpass_csr_gpio_mux_spkrctl_reg = 0;

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,lpass-clock",
			&audio_lpass_mclk.lpass_clock);
	if (ret)
		dev_dbg(&pdev->dev, "%s: qcom,lpass-clock is undefined\n",
				__func__);


	ret = of_property_read_string(pdev->dev.of_node,
				mclk_id, &mclk_str);
	if (ret)
		dev_dbg(&pdev->dev, "%s:of read string %s not present %d\n",
				__func__, mclk_id, ret);

	ret = of_property_read_u32(pdev->dev.of_node,
			"qcom,codec-mclk-clk-freq",
			&mclk_freq);
	if (!ret && (mclk_freq == 12288000 || audio_lpass_mclk.lpass_clock)) {
		digital_cdc_core_clk.clk_freq_in_hz = mclk_freq;

		ret = of_property_read_u32(pdev->dev.of_node, "reg",
				&lpass_csr_gpio_mux_spkrctl_reg);
		if (!ret) {
			audio_lpass_mclk.lpass_csr_gpio_mux_spkrctl_vaddr =
				ioremap(lpass_csr_gpio_mux_spkrctl_reg, 4);
		}

		if (mclk_str) {
			audio_ref_update_afe_mclk_id(mclk_str, LPASS_MCLK);
			audio_ref_update_afe_mclk_id(mclk_str, LPASS_MCLK2);
		}

		ret = audio_get_pinctrl(pdev, LPASS_MCLK);
		if (ret)
			dev_err(&pdev->dev, "%s: Parsing pinctrl %s failed\n",
				__func__, "LPASS_MCLK");

		ret = audio_get_pinctrl(pdev, LPASS_MCLK2);
		if (ret)
			dev_dbg(&pdev->dev, "%s: Parsing pinctrl %s failed\n",
				__func__, "LPASS_MCLK2");

		ret = of_msm_clock_register(pdev->dev.of_node, audio_ref_clock,
			      ARRAY_SIZE(audio_ref_clock));
		if (ret)
			dev_err(&pdev->dev, "%s: clock register failed\n",
				__func__);
		return ret;
	}

	if (mclk_str)
		audio_ref_update_afe_mclk_id(mclk_str, AP_CLK2);

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

	ret = audio_get_pinctrl(pdev, AP_CLK2);
	if (ret)
		dev_dbg(&pdev->dev, "%s: Parsing pinctrl failed\n",
			__func__);

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
	struct pinctrl_info *pnctrl_info = &audio_ap_clk2.pnctrl_info;

	if (audio_pmi_clk.gpio > 0)
		gpio_free(audio_pmi_clk.gpio);
	else if (audio_ap_clk.gpio > 0)
		gpio_free(audio_ap_clk.gpio);

	if (pnctrl_info->pinctrl) {
		devm_pinctrl_put(pnctrl_info->pinctrl);
		pnctrl_info->pinctrl = NULL;
	}

	pnctrl_info = &audio_lpass_mclk.pnctrl_info;
	if (pnctrl_info->pinctrl) {
		devm_pinctrl_put(pnctrl_info->pinctrl);
		pnctrl_info->pinctrl = NULL;
	}

	pnctrl_info = &audio_lpass_mclk2.pnctrl_info;
	if (pnctrl_info->pinctrl) {
		devm_pinctrl_put(pnctrl_info->pinctrl);
		pnctrl_info->pinctrl = NULL;
	}

	if (audio_lpass_mclk.lpass_csr_gpio_mux_spkrctl_vaddr)
		iounmap(audio_lpass_mclk.lpass_csr_gpio_mux_spkrctl_vaddr);

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
