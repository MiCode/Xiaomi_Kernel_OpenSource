/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(fmt)	"TYPEC: %s: " fmt, __func__

#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/spmi.h>

#define CREATE_MASK(NUM_BITS, POS) \
	((unsigned char) (((1 << (NUM_BITS)) - 1) << (POS)))
#define TYPEC_MASK(MSB_BIT, LSB_BIT) \
	CREATE_MASK(MSB_BIT - LSB_BIT + 1, LSB_BIT)

/* Interrupt offsets */
#define INT_RT_STS_REG(base)		(base + 0x10)
#define DFP_DETECT_BIT			BIT(3)
#define UFP_DETECT_BIT			BIT(1)

#define TYPEC_UFP_STATUS_REG(base)	(base +	0x08)
#define TYPEC_CCOUT_BIT			BIT(7)
#define TYPEC_CCOUT_OPEN_BIT		BIT(6)
#define TYPEC_CURRENT_MASK		TYPEC_MASK(2, 0)
#define TYPEC_RDSTD_BIT			BIT(2)
#define TYPEC_RD1P5_BIT			BIT(1)

#define TYPEC_DFP_STATUS_REG(base)	(base +	0x09)
#define VALID_DFP_MASK			TYPEC_MASK(6, 4)

#define TYPEC_STD_MA			900
#define TYPEC_MED_MA			1500
#define TYPEC_HIGH_MA			3000

#define QPNP_TYPEC_DEV_NAME	"qcom,qpnp-typec"
#define TYPEC_PSY_NAME		"typec"

enum cc_line_state {
	CC_1,
	CC_2,
	OPEN,
};

struct qpnp_typec_chip {
	struct device		*dev;
	struct spmi_device	*spmi;
	struct power_supply	*batt_psy;
	struct power_supply	type_c_psy;
	struct regulator	*ss_mux_vreg;
	struct mutex		typec_lock;

	u16			base;

	/* IRQs */
	int			vrd_changed;
	int			ufp_detach;
	int			ufp_detect;
	int			dfp_detach;
	int			dfp_detect;
	int			vbus_err;
	int			vconn_oc;

	/* Configurations */
	int			cc_line_state;
	int			current_ma;
	int			ssmux_gpio;
	enum of_gpio_flags	gpio_flag;
	int			typec_state;
};

/* SPMI operations */
static int qpnp_typec_read(struct qpnp_typec_chip *chip, u8 *val, u16 addr,
			int count)
{
	int rc = 0;
	struct spmi_device *spmi = chip->spmi;

	if (addr == 0) {
		pr_err("addr cannot be zero addr=0x%02x sid=0x%02x rc=%d\n",
				addr, spmi->sid, rc);
		return -EINVAL;
	}

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, addr, val, count);
	if (rc) {
		pr_err("spmi read failed addr=0x%02x sid=0x%02x rc=%d\n",
				addr, spmi->sid, rc);
		return rc;
	}

	return 0;
}

static int set_property_on_battery(struct qpnp_typec_chip *chip,
				enum power_supply_property prop)
{
	int rc = 0;
	union power_supply_propval ret = {0, };

	if (!chip->batt_psy) {
		chip->batt_psy = power_supply_get_by_name("battery");
		if (!chip->batt_psy) {
			pr_err("no batt psy found\n");
			return -ENODEV;
		}
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		ret.intval = chip->current_ma;
		rc = chip->batt_psy->set_property(chip->batt_psy,
			POWER_SUPPLY_PROP_CURRENT_CAPABILITY, &ret);
		if (rc)
			pr_err("failed to set current max rc=%d\n", rc);
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		/*
		 * Notify the typec mode to charger. This is useful in the DFP
		 * case where there is no notification of OTG insertion to the
		 * charger driver.
		 */
		ret.intval = chip->typec_state;
		rc = chip->batt_psy->set_property(chip->batt_psy,
				POWER_SUPPLY_PROP_TYPEC_MODE, &ret);
		if (rc)
			pr_err("failed to set typec mode rc=%d\n", rc);
		break;
	default:
		pr_err("invalid request\n");
		rc = -EINVAL;
	}

	return rc;
}

static int get_max_current(u8 reg)
{
	if (!reg)
		return 0;

	return (reg & TYPEC_RDSTD_BIT) ? TYPEC_STD_MA :
		(reg & TYPEC_RD1P5_BIT) ? TYPEC_MED_MA : TYPEC_HIGH_MA;
}

static int qpnp_typec_configure_ssmux(struct qpnp_typec_chip *chip,
				enum cc_line_state cc_line)
{
	int rc = 0;

	if (cc_line != chip->cc_line_state) {
		switch (cc_line) {
		case OPEN:
			if (chip->ss_mux_vreg) {
				rc = regulator_disable(chip->ss_mux_vreg);
				if (rc) {
					pr_err("failed to disable ssmux regulator rc=%d\n",
							rc);
					return rc;
				}
			}

			if (chip->ssmux_gpio) {
				rc = gpio_direction_input(chip->ssmux_gpio);
				if (rc) {
					pr_err("failed to configure ssmux gpio rc=%d\n",
							rc);
					return rc;
				}
			}
			break;
		case CC_1:
		case CC_2:
			if (chip->ss_mux_vreg) {
				rc = regulator_enable(chip->ss_mux_vreg);
				if (rc) {
					pr_err("failed to enable ssmux regulator rc=%d\n",
							rc);
					return rc;
				}
			}

			if (chip->ssmux_gpio) {
				rc = gpio_direction_output(chip->ssmux_gpio,
					(chip->gpio_flag == OF_GPIO_ACTIVE_LOW)
						? !cc_line : cc_line);
				if (rc) {
					pr_err("failed to configure ssmux gpio rc=%d\n",
							rc);
					return rc;
				}
			}
			break;
		}
	}

	return 0;
}

static int qpnp_typec_handle_usb_insertion(struct qpnp_typec_chip *chip, u8 reg)
{
	int rc;
	enum cc_line_state cc_line_state;

	cc_line_state = (reg & TYPEC_CCOUT_OPEN_BIT) ?
		OPEN : (reg & TYPEC_CCOUT_BIT) ? CC_2 : CC_1;
	rc = qpnp_typec_configure_ssmux(chip, cc_line_state);
	if (rc) {
		pr_err("failed to configure ss-mux rc=%d\n", rc);
		return rc;
	}

	chip->cc_line_state = cc_line_state;

	pr_debug("CC_line state = %d\n", cc_line_state);

	return 0;
}

static int qpnp_typec_handle_detach(struct qpnp_typec_chip *chip)
{
	int rc;

	rc = qpnp_typec_configure_ssmux(chip, OPEN);
	if (rc) {
		pr_err("failed to configure SSMUX rc=%d\n", rc);
		return rc;
	}

	chip->cc_line_state = OPEN;
	chip->current_ma = 0;
	chip->typec_state = POWER_SUPPLY_TYPE_UNKNOWN;
	chip->type_c_psy.type = POWER_SUPPLY_TYPE_UNKNOWN;
	rc = set_property_on_battery(chip, POWER_SUPPLY_PROP_TYPEC_MODE);
	if (rc)
		pr_err("failed to set TYPEC MODE on battery psy rc=%d\n", rc);

	pr_debug("CC_line state = %d current_ma = %d\n", chip->cc_line_state,
			chip->current_ma);

	return rc;
}

/* Interrupt handlers */
static irqreturn_t vrd_changed_handler(int irq, void *_chip)
{
	int rc, old_current;
	u8 reg;
	struct qpnp_typec_chip *chip = _chip;

	pr_debug("vrd changed triggered\n");

	mutex_lock(&chip->typec_lock);
	rc = qpnp_typec_read(chip, &reg, TYPEC_UFP_STATUS_REG(chip->base), 1);
	if (rc) {
		pr_err("failed to read status reg rc=%d\n", rc);
		goto out;
	}

	old_current = chip->current_ma;
	chip->current_ma = get_max_current(reg & TYPEC_CURRENT_MASK);

	/* only notify if current is valid and changed at runtime */
	if (chip->current_ma && (old_current != chip->current_ma)) {
		rc = set_property_on_battery(chip,
				POWER_SUPPLY_PROP_CURRENT_CAPABILITY);
		if (rc)
			pr_err("failed to set INPUT CURRENT MAX on battery psy rc=%d\n",
					rc);
	}

	pr_debug("UFP status reg = 0x%x old current = %dma new current = %dma\n",
			reg, old_current, chip->current_ma);

out:
	mutex_unlock(&chip->typec_lock);
	return IRQ_HANDLED;
}

static irqreturn_t vconn_oc_handler(int irq, void *_chip)
{
	pr_warn("vconn oc triggered\n");

	return IRQ_HANDLED;
}

static irqreturn_t ufp_detect_handler(int irq, void *_chip)
{
	int rc;
	u8 reg;
	struct qpnp_typec_chip *chip = _chip;

	pr_debug("ufp detect triggered\n");

	mutex_lock(&chip->typec_lock);
	rc = qpnp_typec_read(chip, &reg, TYPEC_UFP_STATUS_REG(chip->base), 1);
	if (rc) {
		pr_err("failed to read status reg rc=%d\n", rc);
		goto out;
	}

	rc = qpnp_typec_handle_usb_insertion(chip, reg);
	if (rc) {
		pr_err("failed to handle USB insertion rc=%d\n", rc);
		goto out;
	}

	chip->current_ma = get_max_current(reg & TYPEC_CURRENT_MASK);
	/* device in UFP state */
	chip->typec_state = POWER_SUPPLY_TYPE_UFP;
	chip->type_c_psy.type = POWER_SUPPLY_TYPE_UFP;
	rc = set_property_on_battery(chip, POWER_SUPPLY_PROP_TYPEC_MODE);
	if (rc)
		pr_err("failed to set TYPEC MODE on battery psy rc=%d\n", rc);

	pr_debug("UFP status reg = 0x%x current = %dma\n",
			reg, chip->current_ma);

out:
	mutex_unlock(&chip->typec_lock);
	return IRQ_HANDLED;
}

static irqreturn_t ufp_detach_handler(int irq, void *_chip)
{
	int rc;
	struct qpnp_typec_chip *chip = _chip;

	pr_debug("ufp detach triggered\n");

	mutex_lock(&chip->typec_lock);
	rc = qpnp_typec_handle_detach(chip);
	if (rc)
		pr_err("failed to handle UFP detach rc=%d\n", rc);

	mutex_unlock(&chip->typec_lock);

	return IRQ_HANDLED;
}

static irqreturn_t dfp_detect_handler(int irq, void *_chip)
{
	int rc;
	u8 reg[2];
	struct qpnp_typec_chip *chip = _chip;

	pr_debug("dfp detect trigerred\n");

	mutex_lock(&chip->typec_lock);
	rc = qpnp_typec_read(chip, reg, TYPEC_UFP_STATUS_REG(chip->base), 2);
	if (rc) {
		pr_err("failed to read status reg rc=%d\n", rc);
		goto out;
	}

	if (reg[1] & VALID_DFP_MASK) {
		rc = qpnp_typec_handle_usb_insertion(chip, reg[0]);
		if (rc) {
			pr_err("failed to handle USB insertion rc=%d\n", rc);
			goto out;
		}

		chip->typec_state = POWER_SUPPLY_TYPE_DFP;
		chip->type_c_psy.type = POWER_SUPPLY_TYPE_DFP;
		chip->current_ma = 0;
		rc = set_property_on_battery(chip,
				POWER_SUPPLY_PROP_TYPEC_MODE);
		if (rc)
			pr_err("failed to set TYPEC MODE on battery psy rc=%d\n",
					rc);
	}

	pr_debug("UFP status reg = 0x%x DFP status reg = 0x%x\n",
			reg[0], reg[1]);

out:
	mutex_unlock(&chip->typec_lock);
	return IRQ_HANDLED;
}

static irqreturn_t dfp_detach_handler(int irq, void *_chip)
{
	int rc;
	struct qpnp_typec_chip *chip = _chip;

	pr_debug("dfp detach triggered\n");

	mutex_lock(&chip->typec_lock);
	rc = qpnp_typec_handle_detach(chip);
	if (rc)
		pr_err("failed to handle DFP detach rc=%d\n", rc);

	mutex_unlock(&chip->typec_lock);

	return IRQ_HANDLED;
}

static irqreturn_t vbus_err_handler(int irq, void *_chip)
{
	int rc;
	struct qpnp_typec_chip *chip = _chip;

	pr_debug("vbus_err triggered\n");

	mutex_lock(&chip->typec_lock);
	rc = qpnp_typec_handle_detach(chip);
	if (rc)
		pr_err("failed to handle VBUS_ERR rc==%d\n", rc);

	mutex_unlock(&chip->typec_lock);

	return IRQ_HANDLED;
}

static int qpnp_typec_parse_dt(struct qpnp_typec_chip *chip)
{
	int rc;
	struct device_node *node = chip->dev->of_node;

	/* SS-Mux configuration gpio */
	if (of_find_property(node, "qcom,ssmux-gpio", NULL)) {
		chip->ssmux_gpio = of_get_named_gpio_flags(node,
				"qcom,ssmux-gpio", 0, &chip->gpio_flag);
		if (!gpio_is_valid(chip->ssmux_gpio)) {
			if (chip->ssmux_gpio != -EPROBE_DEFER)
				pr_err("failed to get ss-mux config gpio=%d\n",
						chip->ssmux_gpio);
			return chip->ssmux_gpio;
		}

		rc = devm_gpio_request(chip->dev, chip->ssmux_gpio,
				"typec_mux_config_gpio");
		if (rc) {
			pr_err("failed to request ss-mux gpio rc=%d\n", rc);
			return rc;
		}
	}

	/* SS-Mux regulator */
	if (of_find_property(node, "ss-mux-supply", NULL)) {
		chip->ss_mux_vreg = devm_regulator_get(chip->dev, "ss-mux");
		if (IS_ERR(chip->ss_mux_vreg))
			return PTR_ERR(chip->ss_mux_vreg);
	}

	return 0;
}

static int qpnp_typec_determine_initial_status(struct qpnp_typec_chip *chip)
{
	int rc;
	u8 rt_reg;

	rc = qpnp_typec_read(chip, &rt_reg, INT_RT_STS_REG(chip->base), 1);
	if (rc) {
		pr_err("failed to read RT status reg rc=%d\n", rc);
		return rc;
	}
	pr_debug("RT status reg = 0x%x\n", rt_reg);

	chip->cc_line_state = OPEN;
	chip->typec_state = POWER_SUPPLY_TYPE_UNKNOWN;
	chip->type_c_psy.type = POWER_SUPPLY_TYPE_UNKNOWN;

	if (rt_reg & DFP_DETECT_BIT) {
		/* we are in DFP state*/
		dfp_detect_handler(0, chip);
	} else if (rt_reg & UFP_DETECT_BIT) {
		/* we are in UFP state */
		ufp_detect_handler(0, chip);
	}

	return 0;
}

#define REQUEST_IRQ(chip, irq, irq_name, irq_handler, flags, wake, rc)	\
do {									\
	irq = spmi_get_irq_byname(chip->spmi, NULL, irq_name);		\
	if (irq < 0) {							\
		pr_err("Unable to get " irq_name " irq\n");		\
		rc |= -ENXIO;						\
	}								\
	rc = devm_request_threaded_irq(chip->dev,			\
			irq, NULL, irq_handler, flags, irq_name,	\
			chip);						\
	if (rc < 0) {							\
		pr_err("Unable to request " irq_name " irq: %d\n", rc);	\
		rc |= -ENXIO;						\
	}								\
									\
	if (wake)							\
		enable_irq_wake(irq);					\
} while (0)

static int qpnp_typec_request_irqs(struct qpnp_typec_chip *chip)
{
	int rc = 0;
	unsigned long flags = IRQF_TRIGGER_RISING | IRQF_ONESHOT;

	REQUEST_IRQ(chip, chip->vrd_changed, "vrd-change", vrd_changed_handler,
			flags, true, rc);
	REQUEST_IRQ(chip, chip->ufp_detach, "ufp-detach", ufp_detach_handler,
			flags, true, rc);
	REQUEST_IRQ(chip, chip->ufp_detect, "ufp-detect", ufp_detect_handler,
			flags, true, rc);
	REQUEST_IRQ(chip, chip->dfp_detach, "dfp-detach", dfp_detach_handler,
			flags, true, rc);
	REQUEST_IRQ(chip, chip->dfp_detect, "dfp-detect", dfp_detect_handler,
			flags, true, rc);
	REQUEST_IRQ(chip, chip->vbus_err, "vbus-err", vbus_err_handler,
			flags, true, rc);
	REQUEST_IRQ(chip, chip->vconn_oc, "vconn-oc", vconn_oc_handler,
			flags, true, rc);

	return rc;
}

static enum power_supply_property qpnp_typec_properties[] = {
	POWER_SUPPLY_PROP_CURRENT_CAPABILITY,
	POWER_SUPPLY_PROP_TYPE,
};

static int qpnp_typec_get_property(struct power_supply *psy,
				enum power_supply_property prop,
				union power_supply_propval *val)
{
	struct qpnp_typec_chip *chip = container_of(psy,
					struct qpnp_typec_chip, type_c_psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_TYPE:
		val->intval = chip->typec_state;
		break;
	case POWER_SUPPLY_PROP_CURRENT_CAPABILITY:
		val->intval = chip->current_ma;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int qpnp_typec_probe(struct spmi_device *spmi)
{
	int rc;
	struct resource *resource;
	struct qpnp_typec_chip *chip;

	resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!resource) {
		pr_err("Unable to get spmi resource for TYPEC\n");
		return -EINVAL;
	}

	chip = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_typec_chip),
			GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &spmi->dev;
	chip->spmi = spmi;

	/* parse DT */
	rc = qpnp_typec_parse_dt(chip);
	if (rc) {
		pr_err("failed to parse DT rc=%d\n", rc);
		return rc;
	}

	chip->base = resource->start;
	dev_set_drvdata(&spmi->dev, chip);
	device_init_wakeup(&spmi->dev, 1);
	mutex_init(&chip->typec_lock);

	/* determine initial status */
	rc = qpnp_typec_determine_initial_status(chip);
	if (rc) {
		pr_err("failed to determine initial state rc=%d\n", rc);
		return rc;
	}

	chip->type_c_psy.name		= TYPEC_PSY_NAME;
	chip->type_c_psy.get_property	= qpnp_typec_get_property;
	chip->type_c_psy.properties	= qpnp_typec_properties;
	chip->type_c_psy.num_properties	= ARRAY_SIZE(qpnp_typec_properties);

	rc = power_supply_register(chip->dev, &chip->type_c_psy);
	if (rc < 0) {
		pr_err("Unable to register  type_c_psy rc=%d\n", rc);
		return rc;
	}

	/* All irqs */
	rc = qpnp_typec_request_irqs(chip);
	if (rc) {
		pr_err("failed to request irqs rc=%d\n", rc);
		return rc;
	}

	pr_info("TypeC successfully probed state=%d CC-line-state=%d\n",
			chip->typec_state, chip->cc_line_state);

	return 0;
}

static int qpnp_typec_remove(struct spmi_device *spmi)
{
	int rc;
	struct qpnp_typec_chip *chip = dev_get_drvdata(&spmi->dev);

	rc = qpnp_typec_configure_ssmux(chip, OPEN);
	if (rc)
		pr_err("failed to configure SSMUX rc=%d\n", rc);

	mutex_destroy(&chip->typec_lock);
	dev_set_drvdata(chip->dev, NULL);

	return 0;
}

static struct of_device_id qpnp_typec_match_table[] = {
	{ .compatible = QPNP_TYPEC_DEV_NAME, },
	{}
};

static struct spmi_driver qpnp_typec_driver = {
	.probe		= qpnp_typec_probe,
	.remove		= qpnp_typec_remove,
	.driver		= {
		.name		= QPNP_TYPEC_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= qpnp_typec_match_table,
	},
};

/*
 * qpnp_typec_init() - register spmi driver for qpnp-typec
 */
static int __init qpnp_typec_init(void)
{
	return spmi_driver_register(&qpnp_typec_driver);
}
module_init(qpnp_typec_init);

static void __exit qpnp_typec_exit(void)
{
	spmi_driver_unregister(&qpnp_typec_driver);
}
module_exit(qpnp_typec_exit);

MODULE_DESCRIPTION("QPNP type-C driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_TYPEC_DEV_NAME);
