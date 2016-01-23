/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#define QPNP_COINCELL_DRIVER_NAME "qcom,qpnp-coincell"

struct qpnp_coincell {
	struct spmi_device	*spmi_dev;
	u16			base_addr;
};

#define QPNP_COINCELL_REG_TYPE		0x04
#define QPNP_COINCELL_REG_SUBTYPE	0x05
#define QPNP_COINCELL_REG_RSET		0x44
#define QPNP_COINCELL_REG_VSET		0x45
#define QPNP_COINCELL_REG_ENABLE	0x46

#define QPNP_COINCELL_TYPE		0x02
#define QPNP_COINCELL_SUBTYPE		0x20
#define QPNP_COINCELL_ENABLE		0x80
#define QPNP_COINCELL_DISABLE		0x00

static const int qpnp_rset_map[] = {2100, 1700, 1200, 800};
static const int qpnp_vset_map[] = {2500, 3200, 3100, 3000};

static int qpnp_coincell_set_resistance(struct qpnp_coincell *chip, int rset)
{
	int i, rc;
	u8 reg;

	for (i = 0; i < ARRAY_SIZE(qpnp_rset_map); i++)
		if (rset == qpnp_rset_map[i])
			break;

	if (i >= ARRAY_SIZE(qpnp_rset_map)) {
		pr_err("invalid rset=%d value\n", rset);
		return -EINVAL;
	}

	reg = i;
	rc = spmi_ext_register_writel(chip->spmi_dev->ctrl, chip->spmi_dev->sid,
		chip->base_addr + QPNP_COINCELL_REG_RSET, &reg, 1);
	if (rc)
		dev_err(&chip->spmi_dev->dev, "%s: could not write to RSET register, rc=%d\n",
			__func__, rc);

	return rc;
}

static int qpnp_coincell_set_voltage(struct qpnp_coincell *chip, int vset)
{
	int i, rc;
	u8 reg;

	for (i = 0; i < ARRAY_SIZE(qpnp_vset_map); i++)
		if (vset == qpnp_vset_map[i])
			break;

	if (i >= ARRAY_SIZE(qpnp_vset_map)) {
		pr_err("invalid vset=%d value\n", vset);
		return -EINVAL;
	}

	reg = i;
	rc = spmi_ext_register_writel(chip->spmi_dev->ctrl, chip->spmi_dev->sid,
		chip->base_addr + QPNP_COINCELL_REG_VSET, &reg, 1);
	if (rc)
		dev_err(&chip->spmi_dev->dev, "%s: could not write to VSET register, rc=%d\n",
			__func__, rc);

	return rc;
}

static int qpnp_coincell_set_charge(struct qpnp_coincell *chip, bool enabled)
{
	int rc;
	u8 reg;

	reg = enabled ? QPNP_COINCELL_ENABLE : QPNP_COINCELL_DISABLE;
	rc = spmi_ext_register_writel(chip->spmi_dev->ctrl, chip->spmi_dev->sid,
		chip->base_addr + QPNP_COINCELL_REG_ENABLE, &reg, 1);
	if (rc)
		dev_err(&chip->spmi_dev->dev, "%s: could not write to ENABLE register, rc=%d\n",
			__func__, rc);

	return rc;
}

static void qpnp_coincell_charger_show_state(struct qpnp_coincell *chip)
{
	int rc, rset, vset, temp;
	bool enabled;
	u8 reg[QPNP_COINCELL_REG_ENABLE - QPNP_COINCELL_REG_RSET + 1];

	rc = spmi_ext_register_readl(chip->spmi_dev->ctrl, chip->spmi_dev->sid,
		chip->base_addr + QPNP_COINCELL_REG_RSET, reg, ARRAY_SIZE(reg));
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: could not read RSET register, rc=%d\n",
			__func__, rc);
		return;
	}

	temp = reg[QPNP_COINCELL_REG_RSET - QPNP_COINCELL_REG_RSET];
	if (temp >= ARRAY_SIZE(qpnp_rset_map)) {
		dev_err(&chip->spmi_dev->dev, "unknown RSET=0x%02X register value\n",
			temp);
		return;
	}
	rset = qpnp_rset_map[temp];

	temp = reg[QPNP_COINCELL_REG_VSET - QPNP_COINCELL_REG_RSET];
	if (temp >= ARRAY_SIZE(qpnp_vset_map)) {
		dev_err(&chip->spmi_dev->dev, "unknown VSET=0x%02X register value\n",
			temp);
		return;
	}
	vset = qpnp_vset_map[temp];

	temp = reg[QPNP_COINCELL_REG_ENABLE - QPNP_COINCELL_REG_RSET];
	enabled = temp & QPNP_COINCELL_ENABLE;

	pr_info("enabled=%c, voltage=%d mV, resistance=%d ohm\n",
		(enabled ? 'Y' : 'N'), vset, rset);
}

static int qpnp_coincell_check_type(struct qpnp_coincell *chip)
{
	int rc;
	u8 type[2];

	rc = spmi_ext_register_readl(chip->spmi_dev->ctrl, chip->spmi_dev->sid,
		chip->base_addr + QPNP_COINCELL_REG_TYPE, type, 2);
	if (rc) {
		dev_err(&chip->spmi_dev->dev, "%s: could not read type register, rc=%d\n",
			__func__, rc);
		return rc;
	}

	if (type[0] != QPNP_COINCELL_TYPE || type[1] != QPNP_COINCELL_SUBTYPE) {
		dev_err(&chip->spmi_dev->dev, "%s: invalid type=0x%02X or subtype=0x%02X register value\n",
			__func__, type[0], type[1]);
		return -ENODEV;
	}

	return rc;
}

static int qpnp_coincell_probe(struct spmi_device *spmi)
{
	struct device_node *node = spmi->dev.of_node;
	struct qpnp_coincell *chip;
	struct resource *res;
	u32 temp;
	int rc = 0;

	if (!node) {
		dev_err(&spmi->dev, "%s: device node missing\n", __func__);
		return -ENODEV;
	}

	chip = devm_kzalloc(&spmi->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&spmi->dev, "%s: cannot allocate qpnp_coincell\n",
			__func__);
		return -ENOMEM;
	}
	chip->spmi_dev = spmi;

	res = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&spmi->dev, "%s: node is missing base address\n",
			__func__);
		return -EINVAL;
	}
	chip->base_addr = res->start;

	rc = qpnp_coincell_check_type(chip);
	if (rc)
		return rc;

	rc = of_property_read_u32(node, "qcom,rset-ohms", &temp);
	if (!rc) {
		rc = qpnp_coincell_set_resistance(chip, temp);
		if (rc)
			return rc;
	}

	rc = of_property_read_u32(node, "qcom,vset-millivolts", &temp);
	if (!rc) {
		rc = qpnp_coincell_set_voltage(chip, temp);
		if (rc)
			return rc;
	}

	rc = of_property_read_u32(node, "qcom,charge-enable", &temp);
	if (!rc) {
		rc = qpnp_coincell_set_charge(chip, temp);
		if (rc)
			return rc;
	}

	qpnp_coincell_charger_show_state(chip);

	return 0;
}

static int qpnp_coincell_remove(struct spmi_device *spmi)
{
	return 0;
}

static struct of_device_id qpnp_coincell_match_table[] = {
	{ .compatible = QPNP_COINCELL_DRIVER_NAME, },
	{}
};

static const struct spmi_device_id qpnp_coincell_id[] = {
	{ QPNP_COINCELL_DRIVER_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(spmi, qpnp_coincell_id);

static struct spmi_driver qpnp_coincell_driver = {
	.driver	= {
		.name		= QPNP_COINCELL_DRIVER_NAME,
		.of_match_table	= qpnp_coincell_match_table,
		.owner		= THIS_MODULE,
	},
	.probe		= qpnp_coincell_probe,
	.remove		= qpnp_coincell_remove,
	.id_table	= qpnp_coincell_id,
};

static int __init qpnp_coincell_init(void)
{
	return spmi_driver_register(&qpnp_coincell_driver);
}

static void __exit qpnp_coincell_exit(void)
{
	spmi_driver_unregister(&qpnp_coincell_driver);
}

MODULE_DESCRIPTION("QPNP PMIC coincell charger driver");
MODULE_LICENSE("GPL v2");

module_init(qpnp_coincell_init);
module_exit(qpnp_coincell_exit);
