/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/input.h>

#define QPNP_PON_RT_STS(base)		(base + 0x10)
#define QPNP_PON_PULL_CTL(base)		(base + 0x70)
#define QPNP_PON_DBC_CTL(base)		(base + 0x71)

#define QPNP_PON_CNTL_PULL_UP		BIT(1)
#define QPNP_PON_CNTL_TRIG_DELAY_MASK	(0x7)
#define QPNP_PON_KPDPWR_N_SET		BIT(0)

struct qpnp_pon {
	struct spmi_device *spmi;
	struct input_dev *pon_input;
	u32 key_status_irq;
	u16 base;
};

static irqreturn_t qpnp_pon_key_irq(int irq, void *_pon)
{
	u8 pon_rt_sts;
	int rc;
	struct qpnp_pon *pon = _pon;

	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_RT_STS(pon->base), &pon_rt_sts, 1);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to read PON RT status\n");
		return IRQ_HANDLED;
	}

	input_report_key(pon->pon_input, KEY_POWER,
				!(pon_rt_sts & QPNP_PON_KPDPWR_N_SET));
	input_sync(pon->pon_input);

	return IRQ_HANDLED;
}

static int __devinit qpnp_pon_key_init(struct qpnp_pon *pon)
{
	int rc = 0;
	u32 pullup, delay;
	u8 pon_cntl;

	pon->key_status_irq = spmi_get_irq_byname(pon->spmi,
						NULL, "power-key");
	if (pon->key_status_irq < 0) {
		dev_err(&pon->spmi->dev, "Unable to get pon key irq\n");
		return -ENXIO;
	}

	rc = of_property_read_u32(pon->spmi->dev.of_node,
					"qcom,pon-key-dbc-delay", &delay);
	if (rc) {
		delay = (delay << 6) / USEC_PER_SEC;
		delay = ilog2(delay);

		rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_DBC_CTL(pon->base), &pon_cntl, 1);
		if (rc) {
			dev_err(&pon->spmi->dev, "spmi read addr=%x failed\n",
						QPNP_PON_DBC_CTL(pon->base));
			return rc;
		}
		pon_cntl &= ~QPNP_PON_CNTL_TRIG_DELAY_MASK;
		pon_cntl |= (delay & QPNP_PON_CNTL_TRIG_DELAY_MASK);
		rc = spmi_ext_register_writel(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_DBC_CTL(pon->base), &pon_cntl, 1);
		if (rc) {
			dev_err(&pon->spmi->dev, "spmi write addre=%x failed\n",
						QPNP_PON_DBC_CTL(pon->base));
			return rc;
		}
	}

	rc = of_property_read_u32(pon->spmi->dev.of_node,
				"qcom,pon-key-pull-up", &pullup);
	if (!rc) {
		rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_PULL_CTL(pon->base), &pon_cntl, 1);
		if (rc) {
			dev_err(&pon->spmi->dev, "spmi read addr=%x failed\n",
						QPNP_PON_PULL_CTL(pon->base));
			return rc;
		}
		if (pullup)
			pon_cntl |= QPNP_PON_CNTL_PULL_UP;
		else
			pon_cntl &= ~QPNP_PON_CNTL_PULL_UP;

		rc = spmi_ext_register_writel(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_PULL_CTL(pon->base), &pon_cntl, 1);
		if (rc) {
			dev_err(&pon->spmi->dev, "spmi write addr=%x failed\n",
						QPNP_PON_PULL_CTL(pon->base));
			return rc;
		}
	}

	pon->pon_input = input_allocate_device();
	if (!pon->pon_input) {
		dev_err(&pon->spmi->dev, "Can't allocate pon button\n");
		return -ENOMEM;
	}

	input_set_capability(pon->pon_input, EV_KEY, KEY_POWER);
	pon->pon_input->name = "qpnp_pon_key";
	pon->pon_input->phys = "qpnp_pon_key/input0";

	rc = input_register_device(pon->pon_input);
	if (rc) {
		dev_err(&pon->spmi->dev, "Can't register pon key: %d\n", rc);
		goto free_input_dev;
	}

	rc = request_any_context_irq(pon->key_status_irq, qpnp_pon_key_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						"qpnp_pon_key_status", pon);
	if (rc < 0) {
		dev_err(&pon->spmi->dev, "Can't request %d IRQ for pon: %d\n",
						pon->key_status_irq, rc);
		goto unreg_input_dev;
	}

	device_init_wakeup(&pon->spmi->dev, 1);
	enable_irq_wake(pon->key_status_irq);

	return rc;

unreg_input_dev:
	input_unregister_device(pon->pon_input);
free_input_dev:
	input_free_device(pon->pon_input);
	return rc;
}

static int __devinit qpnp_pon_probe(struct spmi_device *spmi)
{
	struct qpnp_pon *pon;
	struct resource *pon_resource;
	u32 pon_key_enable = 0;
	int rc = 0;

	pon = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_pon),
							GFP_KERNEL);
	if (!pon) {
		dev_err(&spmi->dev, "Can't allocate qpnp_pon\n");
		return -ENOMEM;
	}

	pon->spmi = spmi;

	pon_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!pon_resource) {
		dev_err(&spmi->dev, "Unable to get PON base address\n");
		return -ENXIO;
	}
	pon->base = pon_resource->start;

	dev_set_drvdata(&spmi->dev, pon);

	/* pon-key-enable property must be set to register pon key */
	rc = of_property_read_u32(spmi->dev.of_node, "qcom,pon-key-enable",
							&pon_key_enable);
	if (rc && rc != -EINVAL) {
		dev_err(&spmi->dev,
			"Error reading 'pon-key-enable' property (%d)", rc);
		return rc;
	}

	if (pon_key_enable) {
		rc = qpnp_pon_key_init(pon);
		if (rc < 0) {
			dev_err(&spmi->dev, "Failed to register pon-key\n");
			return rc;
		}
	}

	return 0;
}

static int qpnp_pon_remove(struct spmi_device *spmi)
{
	struct qpnp_pon *pon = dev_get_drvdata(&spmi->dev);

	if (pon->pon_input) {
		free_irq(pon->key_status_irq, pon);
		input_unregister_device(pon->pon_input);
	}

	return 0;
}

static struct of_device_id spmi_match_table[] = {
	{	.compatible = "qcom,qpnp-power-on",
	}
};

static struct spmi_driver qpnp_pon_driver = {
	.driver		= {
		.name	= "qcom,qpnp-power-on",
		.of_match_table = spmi_match_table,
	},
	.probe		= qpnp_pon_probe,
	.remove		= __devexit_p(qpnp_pon_remove),
};

static int __init qpnp_pon_init(void)
{
	return spmi_driver_register(&qpnp_pon_driver);
}
module_init(qpnp_pon_init);

static void __exit qpnp_pon_exit(void)
{
	return spmi_driver_unregister(&qpnp_pon_driver);
}
module_exit(qpnp_pon_exit);

MODULE_DESCRIPTION("QPNP PMIC POWER-ON driver");
MODULE_LICENSE("GPL v2");
