/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/types.h>
#include <linux/spmi.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/time.h>
#include <linux/qpnp/clkdiv.h>

#define Q_MAX_DT_PROP_SIZE 32

#define Q_REG_ADDR(q_clkdiv, reg_offset)	\
		((q_clkdiv)->offset + reg_offset)

#define Q_REG_DIV_CTL1			   0x43
#define Q_REG_EN_CTL			   0x46

#define Q_SET_EN			   BIT(7)

#define Q_CXO_PERIOD_NS(_cxo_clk)	   (NSEC_PER_SEC / _cxo_clk)
#define Q_DIV_PERIOD_NS(_cxo_clk, _div)	   (NSEC_PER_SEC / (_cxo_clk / _div))
#define Q_ENABLE_DELAY_NS(_cxo_clk, _div)  (2 * Q_CXO_PERIOD_NS(_cxo_clk) + \
					    3 * Q_DIV_PERIOD_NS(_cxo_clk, _div))
#define Q_DISABLE_DELAY_NS(_cxo_clk, _div) (3 * Q_DIV_PERIOD_NS(_cxo_clk, _div))

struct q_clkdiv {
	uint32_t cxo_hz;
	enum q_clkdiv_cfg cxo_div;
	struct device_node *node;
	uint16_t offset;
	struct spmi_controller *ctrl;
	bool enabled;
	struct mutex lock;
	struct list_head list;
	uint8_t slave;
};

static LIST_HEAD(qpnp_clkdiv_devs);

/**
 * qpnp_clkdiv_get - get a clkdiv handle
 * @dev: client device pointer.
 * @name: client specific name for the clock in question.
 *
 * Return a clkdiv handle given a client specific name. This name be a prefix
 * for a property naming that takes a phandle to the actual clkdiv device.
 */
struct q_clkdiv *qpnp_clkdiv_get(struct device *dev, const char *name)
{
	struct q_clkdiv *q_clkdiv;
	struct device_node *divclk_node;
	char prop_name[Q_MAX_DT_PROP_SIZE];
	int n;

	n = snprintf(prop_name, Q_MAX_DT_PROP_SIZE, "%s-clk", name);
	if (n == Q_MAX_DT_PROP_SIZE)
		return ERR_PTR(-EINVAL);

	divclk_node = of_parse_phandle(dev->of_node, prop_name, 0);
	if (divclk_node == NULL)
		return ERR_PTR(-ENODEV);

	list_for_each_entry(q_clkdiv, &qpnp_clkdiv_devs, list)
		if (q_clkdiv->node == divclk_node)
			return q_clkdiv;
	return ERR_PTR(-EPROBE_DEFER);
}
EXPORT_SYMBOL(qpnp_clkdiv_get);

static int __clkdiv_enable(struct q_clkdiv *q_clkdiv, bool enable)
{
	int rc;
	char buf[1];

	buf[0] = enable ? Q_SET_EN : 0;

	mutex_lock(&q_clkdiv->lock);
	rc = spmi_ext_register_writel(q_clkdiv->ctrl, q_clkdiv->slave,
			      Q_REG_ADDR(q_clkdiv, Q_REG_EN_CTL),
			      &buf[0], 1);
	if (!rc)
		q_clkdiv->enabled = enable;

	mutex_unlock(&q_clkdiv->lock);

	if (enable)
		ndelay(Q_ENABLE_DELAY_NS(q_clkdiv->cxo_hz, q_clkdiv->cxo_div));
	else
		ndelay(Q_DISABLE_DELAY_NS(q_clkdiv->cxo_hz, q_clkdiv->cxo_div));

	return rc;
}

/**
 * qpnp_clkdiv_enable - enable a clkdiv
 * @q_clkdiv: pointer to clkdiv handle
 */
int qpnp_clkdiv_enable(struct q_clkdiv *q_clkdiv)
{
	return __clkdiv_enable(q_clkdiv, true);
}
EXPORT_SYMBOL(qpnp_clkdiv_enable);

/**
 * qpnp_clkdiv_disable - disable a clkdiv
 * @q_clkdiv: pointer to clkdiv handle
 */
int qpnp_clkdiv_disable(struct q_clkdiv *q_clkdiv)
{
	return __clkdiv_enable(q_clkdiv, false);
}
EXPORT_SYMBOL(qpnp_clkdiv_disable);

/**
 * @q_clkdiv: pointer to clkdiv handle
 * @cfg: setting used to configure the output frequency
 *
 * Given a q_clkdiv_cfg setting, configure the corresponding clkdiv device
 * for the desired output frequency.
 */
int qpnp_clkdiv_config(struct q_clkdiv *q_clkdiv, enum q_clkdiv_cfg cfg)
{
	int rc;
	char buf[1];

	if (cfg < 0 || cfg >= Q_CLKDIV_INVALID)
		return -EINVAL;

	buf[0] = cfg;

	mutex_lock(&q_clkdiv->lock);

	if (q_clkdiv->enabled) {
		rc = __clkdiv_enable(q_clkdiv, false);
		if (rc) {
			pr_err("unable to disable clock\n");
			goto cfg_err;
		}
	}

	rc = spmi_ext_register_writel(q_clkdiv->ctrl, q_clkdiv->slave,
			      Q_REG_ADDR(q_clkdiv, Q_REG_DIV_CTL1), &buf[0], 1);
	if (rc) {
		pr_err("enable to write config\n");
		q_clkdiv->enabled = 0;
		goto cfg_err;
	}

	q_clkdiv->cxo_div = cfg;

	if (q_clkdiv->enabled) {
		rc = __clkdiv_enable(q_clkdiv, true);
		if (rc) {
			pr_err("unable to re-enable clock\n");
			goto cfg_err;
		}
	}

cfg_err:
	mutex_unlock(&q_clkdiv->lock);
	return rc;
}
EXPORT_SYMBOL(qpnp_clkdiv_config);

static int qpnp_clkdiv_probe(struct spmi_device *spmi)
{
	struct q_clkdiv *q_clkdiv;
	struct device_node *node = spmi->dev.of_node;
	int rc;
	uint32_t en;
	struct resource *res;

	q_clkdiv = devm_kzalloc(&spmi->dev, sizeof(*q_clkdiv), GFP_ATOMIC);
	if (!q_clkdiv)
		return -ENOMEM;

	rc = of_property_read_u32(node, "qcom,cxo-freq",
					&q_clkdiv->cxo_hz);
	if (rc) {
		dev_err(&spmi->dev,
			"%s: unable to get qcom,cxo-freq property\n", __func__);
		return rc;
	}

	res = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&spmi->dev, "%s: unable to get device reg resource\n",
					__func__);
		return -EINVAL;
	}

	q_clkdiv->slave = spmi->sid;
	q_clkdiv->offset = res->start;
	q_clkdiv->ctrl = spmi->ctrl;
	q_clkdiv->node = node;
	mutex_init(&q_clkdiv->lock);

	rc = of_property_read_u32(node, "qcom,cxo-div",
					&q_clkdiv->cxo_div);
	if (rc && rc != -EINVAL) {
		dev_err(&spmi->dev,
			"%s: error getting qcom,cxo-div property\n",
								__func__);
		return rc;
	}

	if (!rc) {
		rc = qpnp_clkdiv_config(q_clkdiv, q_clkdiv->cxo_div);
		if (rc) {
			dev_err(&spmi->dev,
				"%s: unable to set default divide config\n",
								    __func__);
			return rc;
		}
	}

	rc = of_property_read_u32(node, "qcom,enable", &en);
	if (rc && rc != -EINVAL) {
		dev_err(&spmi->dev,
			"%s: error getting qcom,enable property\n", __func__);
		return rc;
	}
	if (!rc) {
		rc = __clkdiv_enable(q_clkdiv, en);
		dev_err(&spmi->dev,
				"%s: unable to set default config\n", __func__);
		return rc;
	}

	dev_set_drvdata(&spmi->dev, q_clkdiv);
	list_add(&q_clkdiv->list, &qpnp_clkdiv_devs);

	return 0;
}

static int qpnp_clkdiv_remove(struct spmi_device *spmi)
{
	struct q_clkdiv *q_clkdiv = dev_get_drvdata(&spmi->dev);
	list_del(&q_clkdiv->list);
	return 0;
}

static struct of_device_id spmi_match_table[] = {
	{	.compatible = "qcom,qpnp-clkdiv",
	},
	{}
};

static struct spmi_driver qpnp_clkdiv_driver = {
	.driver		= {
		.name	= "qcom,qpnp-clkdiv",
		.of_match_table = spmi_match_table,
	},
	.probe		= qpnp_clkdiv_probe,
	.remove		= qpnp_clkdiv_remove,
};

static int __init qpnp_clkdiv_init(void)
{
	return spmi_driver_register(&qpnp_clkdiv_driver);
}

static void __exit qpnp_clkdiv_exit(void)
{
	return spmi_driver_unregister(&qpnp_clkdiv_driver);
}

MODULE_DESCRIPTION("QPNP PMIC clkdiv driver");
MODULE_LICENSE("GPL v2");

module_init(qpnp_clkdiv_init);
module_exit(qpnp_clkdiv_exit);
