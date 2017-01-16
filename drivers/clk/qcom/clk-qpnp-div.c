/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/log2.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/types.h>

#define Q_REG_DIV_CTL1			0x43
#define Q_DIV_CTL1_DIV_FACTOR_MASK	GENMASK(2, 0)

#define Q_REG_EN_CTL			0x46
#define Q_REG_EN_MASK			BIT(7)
#define Q_SET_EN			BIT(7)

#define Q_CXO_PERIOD_NS(cxo_clk)	(NSEC_PER_SEC / cxo_clk)
#define Q_DIV_PERIOD_NS(cxo_clk, div)	(NSEC_PER_SEC / (cxo_clk / div))
#define Q_ENABLE_DELAY_NS(cxo_clk, div)	(2 * Q_CXO_PERIOD_NS(cxo_clk) + \
					(3 * Q_DIV_PERIOD_NS(cxo_clk, div)))
#define Q_DISABLE_DELAY_NS(cxo_clk, div) \
					(3 * Q_DIV_PERIOD_NS(cxo_clk, div))

#define CLK_QPNP_DIV_OFFSET		1

enum q_clk_div_factor {
	Q_CLKDIV_XO_DIV_1_0 = 0,
	Q_CLKDIV_XO_DIV_1,
	Q_CLKDIV_XO_DIV_2,
	Q_CLKDIV_XO_DIV_4,
	Q_CLKDIV_XO_DIV_8,
	Q_CLKDIV_XO_DIV_16,
	Q_CLKDIV_XO_DIV_32,
	Q_CLKDIV_XO_DIV_64,
	Q_CLKDIV_MAX_ALLOWED,
};

enum q_clkdiv_state {
	DISABLE = false,
	ENABLE = true,
};

struct q_clkdiv {
	struct regmap		*regmap;
	struct device		*dev;

	u16			base;
	spinlock_t		lock;

	/* clock properties */
	struct clk_hw		hw;
	unsigned int		cxo_hz;
	enum q_clk_div_factor	div_factor;
	bool			enabled;
};

static inline struct q_clkdiv *to_clkdiv(struct clk_hw *_hw)
{
	return container_of(_hw, struct q_clkdiv, hw);
}

static inline unsigned int div_factor_to_div(unsigned int div_factor)
{
	if (div_factor == Q_CLKDIV_XO_DIV_1_0)
		return 1;
	return 1 << (div_factor - CLK_QPNP_DIV_OFFSET);
}

static inline unsigned int div_to_div_factor(unsigned int div)
{
	return ilog2(div) + CLK_QPNP_DIV_OFFSET;
}

static int qpnp_clkdiv_masked_write(struct q_clkdiv *q_clkdiv, u16 offset,
			u8 mask, u8 val)
{
	int rc;

	rc = regmap_update_bits(q_clkdiv->regmap, q_clkdiv->base + offset, mask,
				val);
	if (rc)
		dev_err(q_clkdiv->dev,
			"Unable to regmap_update_bits to addr=%hx, rc(%d)\n",
			q_clkdiv->base + offset, rc);
	return rc;
}

static int qpnp_clkdiv_set_enable_state(struct q_clkdiv *q_clkdiv,
			enum q_clkdiv_state enable_state)
{
	int rc;

	rc = qpnp_clkdiv_masked_write(q_clkdiv, Q_REG_EN_CTL, Q_REG_EN_MASK,
				(enable_state == ENABLE) ? Q_SET_EN : 0);
	if (rc)
		return rc;

	if (enable_state == ENABLE)
		ndelay(Q_ENABLE_DELAY_NS(q_clkdiv->cxo_hz,
				div_factor_to_div(q_clkdiv->div_factor)));
	else
		ndelay(Q_DISABLE_DELAY_NS(q_clkdiv->cxo_hz,
				div_factor_to_div(q_clkdiv->div_factor)));

	return rc;
}

static int qpnp_clkdiv_config_freq_div(struct q_clkdiv *q_clkdiv,
			unsigned int div)
{
	unsigned int div_factor;
	int rc;

	div_factor = div_to_div_factor(div);
	if (div_factor <= 0 || div_factor >= Q_CLKDIV_MAX_ALLOWED)
		return -EINVAL;

	if (q_clkdiv->enabled) {
		rc = qpnp_clkdiv_set_enable_state(q_clkdiv, DISABLE);
		if (rc) {
			dev_err(q_clkdiv->dev, "unable to disable clock, rc = %d\n",
				rc);
			return rc;
		}
	}

	rc = qpnp_clkdiv_masked_write(q_clkdiv, Q_REG_DIV_CTL1,
				Q_DIV_CTL1_DIV_FACTOR_MASK, div_factor);
	if (rc) {
		dev_err(q_clkdiv->dev, "config divider failed, rc=%d\n",
			rc);
		return rc;
	}

	q_clkdiv->div_factor = div_factor;

	if (q_clkdiv->enabled) {
		rc = qpnp_clkdiv_set_enable_state(q_clkdiv, ENABLE);
		if (rc)
			dev_err(q_clkdiv->dev, "unable to re-enable clock, rc = %d\n",
				rc);
	}

	return rc;
}

static int clk_qpnp_div_enable(struct clk_hw *hw)
{
	struct q_clkdiv *q_clkdiv = to_clkdiv(hw);
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&q_clkdiv->lock, flags);

	rc =  qpnp_clkdiv_set_enable_state(q_clkdiv, ENABLE);
	if (rc) {
		dev_err(q_clkdiv->dev, "clk enable failed, rc=%d\n", rc);
		goto fail;
	}

	q_clkdiv->enabled = true;

fail:
	spin_unlock_irqrestore(&q_clkdiv->lock, flags);
	return rc;
}

static void clk_qpnp_div_disable(struct clk_hw *hw)
{
	struct q_clkdiv *q_clkdiv = to_clkdiv(hw);
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&q_clkdiv->lock, flags);

	rc = qpnp_clkdiv_set_enable_state(q_clkdiv, DISABLE);
	if (rc) {
		dev_err(q_clkdiv->dev, "clk disable failed, rc=%d\n", rc);
		goto fail;
	}

	q_clkdiv->enabled = false;

fail:
	spin_unlock_irqrestore(&q_clkdiv->lock, flags);
}

static long clk_qpnp_div_round_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long *parent_rate)
{
	struct q_clkdiv *q_clkdiv = to_clkdiv(hw);
	unsigned long flags, new_rate;
	unsigned int div, div_factor;

	spin_lock_irqsave(&q_clkdiv->lock, flags);
	if (rate <= 0 || rate > q_clkdiv->cxo_hz) {
		dev_err(q_clkdiv->dev, "invalid rate requested, rate = %lu\n",
			rate);
		spin_unlock_irqrestore(&q_clkdiv->lock, flags);
		return -EINVAL;
	}

	div = DIV_ROUND_UP(q_clkdiv->cxo_hz, rate);
	div_factor = div_to_div_factor(div);
	if (div_factor >= Q_CLKDIV_MAX_ALLOWED)
		div_factor = Q_CLKDIV_MAX_ALLOWED - 1;
	new_rate = q_clkdiv->cxo_hz / div_factor_to_div(div_factor);

	spin_unlock_irqrestore(&q_clkdiv->lock, flags);
	return new_rate;
}

static unsigned long clk_qpnp_div_recalc_rate(struct clk_hw *hw,
			unsigned long parent_rate)
{
	struct q_clkdiv *q_clkdiv = to_clkdiv(hw);
	unsigned long rate, flags;

	spin_lock_irqsave(&q_clkdiv->lock, flags);

	rate = q_clkdiv->cxo_hz / div_factor_to_div(q_clkdiv->div_factor);

	spin_unlock_irqrestore(&q_clkdiv->lock, flags);
	return rate;
}

static int clk_qpnp_div_set_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	struct q_clkdiv *q_clkdiv = to_clkdiv(hw);
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&q_clkdiv->lock, flags);
	if (rate <= 0 || rate > q_clkdiv->cxo_hz) {
		dev_err(q_clkdiv->dev, "invalid rate requested, rate = %lu\n",
			rate);
		rc = -EINVAL;
		goto fail;
	}

	rc = qpnp_clkdiv_config_freq_div(q_clkdiv, q_clkdiv->cxo_hz / rate);
	if (rc)
		dev_err(q_clkdiv->dev, "clkdiv set rate(%lu) failed, rc = %d\n",
			rate, rc);

fail:
	spin_unlock_irqrestore(&q_clkdiv->lock, flags);
	return rc;
}

static long clk_qpnp_div_list_rate(struct clk_hw *hw, unsigned n,
			unsigned long rate_max)
{
	struct q_clkdiv *q_clkdiv = to_clkdiv(hw);
	unsigned int div_factor;
	long rate;

	if (n >= Q_CLKDIV_MAX_ALLOWED - CLK_QPNP_DIV_OFFSET)
		return 0;

	div_factor = Q_CLKDIV_MAX_ALLOWED - 1 - n;
	rate = q_clkdiv->cxo_hz / div_factor_to_div(div_factor);

	return rate <= rate_max ? rate : 0;
}

const struct clk_ops clk_qpnp_div_ops = {
	.enable = clk_qpnp_div_enable,
	.disable = clk_qpnp_div_disable,
	.set_rate = clk_qpnp_div_set_rate,
	.recalc_rate = clk_qpnp_div_recalc_rate,
	.round_rate = clk_qpnp_div_round_rate,
	.list_rate = clk_qpnp_div_list_rate,
};

#define QPNP_CLKDIV_MAX_NAME_LEN	16

static int qpnp_clkdiv_probe(struct platform_device *pdev)
{
	struct q_clkdiv *q_clkdiv;
	struct device *dev = &pdev->dev;
	struct device_node *of_node = dev->of_node;
	struct clk_init_data *init;
	struct clk_onecell_data *clk_data;
	struct clk *clk;
	unsigned int base, div, init_freq;
	int rc = 0, id;
	char *clk_name;

	q_clkdiv = devm_kzalloc(dev, sizeof(*q_clkdiv), GFP_KERNEL);
	if (!q_clkdiv)
		return -ENOMEM;

	q_clkdiv->regmap = dev_get_regmap(dev->parent, NULL);
	if (!q_clkdiv->regmap) {
		dev_err(dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}
	q_clkdiv->dev = dev;

	rc = of_property_read_u32(of_node, "reg", &base);
	if (rc < 0) {
		dev_err(dev, "Couldn't find reg in node = %s, rc = %d\n",
			of_node->full_name, rc);
		return rc;
	}
	q_clkdiv->base = base;

	/* init clock properties */
	rc = of_property_read_u32(of_node, "qcom,cxo-freq", &q_clkdiv->cxo_hz);
	if (rc) {
		dev_err(dev, "unable to get qcom,cxo-freq property, rc = %d\n",
			rc);
		return rc;
	}

	q_clkdiv->div_factor = Q_CLKDIV_XO_DIV_1_0;
	rc = of_property_read_u32(of_node, "qcom,clkdiv-init-freq", &init_freq);
	if (rc) {
		if (rc != -EINVAL) {
			dev_err(dev, "Unable to read initial frequency value, rc=%d\n",
				rc);
			return rc;
		}
	} else {
		if (init_freq <= 0 || init_freq > q_clkdiv->cxo_hz) {
			dev_err(dev, "invalid initial frequency specified, rate = %u\n",
				init_freq);
			return -EINVAL;
		}

		div = DIV_ROUND_UP(q_clkdiv->cxo_hz, init_freq);
		q_clkdiv->div_factor = div_to_div_factor(div);
		if (q_clkdiv->div_factor >= Q_CLKDIV_MAX_ALLOWED)
			q_clkdiv->div_factor = Q_CLKDIV_MAX_ALLOWED - 1;
		rc = qpnp_clkdiv_config_freq_div(q_clkdiv,
				div_factor_to_div(q_clkdiv->div_factor));
		if (rc) {
			dev_err(dev, "Config initial frequency failed, rc = %d\n",
				rc);
			return rc;
		}
	}

	init = devm_kzalloc(dev, sizeof(*init), GFP_KERNEL);
	if (!init)
		return -ENOMEM;

	rc = of_property_read_u32(of_node, "qcom,clkdiv-id", &id);
	if (rc) {
		dev_err(dev, "Unable to read clkdiv node id, rc = %d\n", rc);
		return rc;
	}

	clk_name = devm_kcalloc(dev, QPNP_CLKDIV_MAX_NAME_LEN,
				sizeof(*clk_name), GFP_KERNEL);
	if (!clk_name)
		return -ENOMEM;
	snprintf(clk_name, QPNP_CLKDIV_MAX_NAME_LEN, "qpnp_clkdiv_%d", id);

	init->name = clk_name;
	init->ops = &clk_qpnp_div_ops;
	q_clkdiv->hw.init = init;
	spin_lock_init(&q_clkdiv->lock);

	clk_data = devm_kzalloc(dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->clks = devm_kzalloc(dev, sizeof(*clk_data->clks), GFP_KERNEL);
	if (!clk_data->clks)
		return -ENOMEM;

	clk = devm_clk_register(dev, &q_clkdiv->hw);
	if (IS_ERR(clk)) {
		dev_err(dev, "Unable to register qpnp div clock\n");
		return PTR_ERR(clk);
	}

	clk_data->clk_num = 1;
	clk_data->clks[0] = clk;

	rc = of_clk_add_provider(of_node, of_clk_src_onecell_get, clk_data);
	if (rc) {
		dev_err(dev, "Unable to register qpnp div clock provider, rc = %d\n",
			rc);
		return rc;
	}

	dev_set_drvdata(dev, q_clkdiv);
	dev_info(dev, "Registered %s successfully\n", clk_name);

	return rc;
}

static const struct of_device_id qpnp_clkdiv_match_table[] = {
	{	.compatible = "qcom,qpnp-clkdiv",
	},
	{}
};

static struct platform_driver qpnp_clkdiv_driver = {
	.driver		= {
		.name	= "qcom,qpnp-clkdiv",
		.of_match_table = qpnp_clkdiv_match_table,
	},
	.probe		= qpnp_clkdiv_probe,
};

static int __init qpnp_clkdiv_init(void)
{
	return platform_driver_register(&qpnp_clkdiv_driver);
}
module_init(qpnp_clkdiv_init);

static void __exit qpnp_clkdiv_exit(void)
{
	return platform_driver_unregister(&qpnp_clkdiv_driver);
}
module_exit(qpnp_clkdiv_exit);

MODULE_DESCRIPTION("QPNP PMIC clkdiv driver");
MODULE_LICENSE("GPL v2");
