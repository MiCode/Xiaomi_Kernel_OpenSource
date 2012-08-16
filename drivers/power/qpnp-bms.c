/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/power_supply.h>
#include <linux/spmi.h>

/* Interrupt offsets */
#define INT_RT_STS(base)		(base + 0x10)
#define INT_SET_TYPE(base)		(base + 0x11)
#define INT_POLARITY_HIGH(base)		(base + 0x12)
#define INT_POLARITY_LOW(base)		(base + 0x13)
#define INT_LATCHED_CLR(base)		(base + 0x14)
#define INT_EN_SET(base)		(base + 0x15)
#define INT_EN_CLR(base)		(base + 0x16)
#define INT_LATCHED_STS(base)		(base + 0x18)
#define INT_PENDING_STS(base)		(base + 0x19)
#define INT_MID_SEL(base)		(base + 0x1A)
#define INT_PRIORITY(base)		(base + 0x1B)

/* BMS Register Offsets */
#define BMS1_REVISION1			0x0
#define BMS1_REVISION2			0x1
#define BMS1_STATUS1			0x8
#define BMS1_MODE_CTL			0X40
/* Columb counter clear registers */
#define BMS1_CC_DATA_CTL		0x42
#define BMS1_CC_CLEAR_CTRL		0x43
/* OCV limit registers */
#define BMS1_OCV_USE_LOW_LIMIT_THR0	0x48
#define BMS1_OCV_USE_LOW_LIMIT_THR1	0x49
#define BMS1_OCV_USE_HIGH_LIMIT_THR0	0x4A
#define BMS1_OCV_USE_HIGH_LIMIT_THR1	0x4B
#define BMS1_OCV_USE_LIMIT_CTL		0x4C
/* CC interrupt threshold */
#define BMS1_CC_THR0			0x7A
#define BMS1_CC_THR1			0x7B
#define BMS1_CC_THR2			0x7C
#define BMS1_CC_THR3			0x7D
#define BMS1_CC_THR4			0x7E
/* OCV for r registers */
#define BMS1_OCV_FOR_R_DATA0		0x80
#define BMS1_OCV_FOR_R_DATA1		0x81
#define BMS1_VSENSE_FOR_R_DATA0		0x82
#define BMS1_VSENSE_FOR_R_DATA1		0x83
/* Columb counter data */
#define BMS1_CC_DATA0			0x8A
#define BMS1_CC_DATA1			0x8B
#define BMS1_CC_DATA2			0x8C
#define BMS1_CC_DATA3			0x8D
#define BMS1_CC_DATA4			0x8E
/* OCV for soc data */
#define BMS1_OCV_FOR_SOC_DATA0		0x90
#define BMS1_OCV_FOR_SOC_DATA1		0x91
#define BMS1_VSENSE_PON_DATA0		0x94
#define BMS1_VSENSE_PON_DATA1		0x95
#define BMS1_VBAT_AVG_DATA0		0x9E
#define BMS1_VBAT_AVG_DATA1		0x9F
/* Extra bms registers */
#define BMS1_BMS_DATA_REG_0		0xB0
#define BMS1_BMS_DATA_REG_1		0xB1
#define BMS1_BMS_DATA_REG_2		0xB2
#define BMS1_BMS_DATA_REG_3		0xB3

#define QPNP_BMS_DEV_NAME "qcom,qpnp-bms"

struct qpnp_bms_chip {
	struct device			*dev;
	struct power_supply		bms_psy;
	struct spmi_device		*spmi;
	u16				base;

	u8				revision1;
	u8				revision2;
	int				charger_status;
	bool				online;
	/* platform data */
	unsigned int			r_sense_mohm;
	unsigned int			v_cutoff;
	unsigned int			max_voltage;
	unsigned int			r_conn_mohm;
	int				shutdown_soc_valid_limit;
	int				adjust_soc_low_threshold;
	int				adjust_soc_high_threshold;
	int				chg_term;
};

static struct of_device_id qpnp_bms_match_table[] = {
	{ .compatible = QPNP_BMS_DEV_NAME },
	{}
};

static char *qpnp_bms_supplicants[] = {
	"battery"
};

static enum power_supply_property msm_bms_power_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static int qpnp_read_wrapper(struct qpnp_bms_chip *chip, u8 *val,
			u16 base, int count)
{
	int rc;
	struct spmi_device *spmi = chip->spmi;

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, base, val, count);
	if (rc)
		pr_err("SPMI read failed rc=%d\n", rc);

	return 0;
}

/* Returns capacity as a SoC percentage between 0 and 100 */
static int get_prop_bms_capacity(struct qpnp_bms_chip *chip)
{
	/* return 50 until a real algorithm is implemented */
	return 50;
}

/* Returns instantaneous current in uA */
static int get_prop_bms_current_now(struct qpnp_bms_chip *chip)
{
	/* temporarily return 0 until a real algorithm is put in */
	return 0;
}

/* Returns full charge design in uAh */
static int get_prop_bms_charge_full_design(struct qpnp_bms_chip *chip)
{
	/* temporarily return 0 until a real algorithm is put in */
	return 0;
}

static void set_prop_bms_online(struct qpnp_bms_chip *chip, bool online)
{
	chip->online = online;
}

static void set_prop_bms_status(struct qpnp_bms_chip *chip, int status)
{
	chip->charger_status = status;
}

static void qpnp_bms_external_power_changed(struct power_supply *psy)
{
}

static int qpnp_bms_power_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct qpnp_bms_chip *chip = container_of(psy, struct qpnp_bms_chip,
								bms_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_prop_bms_capacity(chip);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = get_prop_bms_current_now(chip);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = get_prop_bms_charge_full_design(chip);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int qpnp_bms_power_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct qpnp_bms_chip *chip = container_of(psy, struct qpnp_bms_chip,
								bms_psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		set_prop_bms_online(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		set_prop_bms_status(chip, (bool)val->intval);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

#define SPMI_PROPERTY_READ(chip_prop, qpnp_spmi_property, retval, errlabel)\
do {									\
	retval = of_property_read_u32(spmi->dev.of_node,		\
				"qcom,bms-" qpnp_spmi_property,		\
					&chip->chip_prop);		\
	if (retval) {							\
		pr_err("Error reading " #qpnp_spmi_property		\
						" property %d\n", rc);	\
		goto errlabel;						\
	}								\
} while (0)

static int 
qpnp_bms_probe(struct spmi_device *spmi)
{
	struct qpnp_bms_chip *chip;
	struct resource *bms_resource;
	int rc;

	chip = kzalloc(sizeof *chip, GFP_KERNEL);

	if (chip == NULL) {
		pr_err("kzalloc() failed.\n");
		return -ENOMEM;
	}

	chip->dev = &(spmi->dev);
	chip->spmi = spmi;

	bms_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!bms_resource) {
		dev_err(&spmi->dev, "Unable to get BMS base address\n");
		return -ENXIO;
	}
	chip->base = bms_resource->start;

	rc = qpnp_read_wrapper(chip, &chip->revision1,
			chip->base + BMS1_REVISION1, 1);
	if (rc) {
		pr_err("error reading version register %d\n", rc);
		goto error_read;
	}

	rc = qpnp_read_wrapper(chip, &chip->revision2,
			chip->base + BMS1_REVISION2, 1);
	if (rc) {
		pr_err("Error reading version register %d\n", rc);
		goto error_read;
	}

	SPMI_PROPERTY_READ(r_sense_mohm, "r-sense-mohm", rc, error_read);
	SPMI_PROPERTY_READ(v_cutoff, "v-cutoff-uv", rc, error_read);
	SPMI_PROPERTY_READ(max_voltage, "max-voltage-uv", rc, error_read);
	SPMI_PROPERTY_READ(r_conn_mohm, "r-conn-mohm", rc, error_read);
	SPMI_PROPERTY_READ(shutdown_soc_valid_limit,
			"shutdown-soc-valid-limit", rc, error_read);
	SPMI_PROPERTY_READ(adjust_soc_low_threshold,
			"adjust-soc-low-threshold", rc, error_read);
	SPMI_PROPERTY_READ(adjust_soc_high_threshold,
			"adjust-soc-high-threshold", rc, error_read);
	SPMI_PROPERTY_READ(chg_term, "chg-term-ua", rc, error_read);

	pr_debug("dts data: r_sense_mohm:%d, v_cutoff:%d, max_v:%d, r_conn:%d, shutdown_soc: %d, adjust_soc_low:%d, adjust_soc_high:%d, chg_term:%d\n",
			chip->r_sense_mohm, chip->v_cutoff,
			chip->max_voltage, chip->r_conn_mohm,
			chip->shutdown_soc_valid_limit,
			chip->adjust_soc_low_threshold,
			chip->adjust_soc_high_threshold,
			chip->chg_term);

	dev_set_drvdata(&spmi->dev, chip);
	device_init_wakeup(&spmi->dev, 1);

	/* setup & register the battery power supply */
	chip->bms_psy.name = "bms";
	chip->bms_psy.type = POWER_SUPPLY_TYPE_BMS;
	chip->bms_psy.properties = msm_bms_power_props;
	chip->bms_psy.num_properties = ARRAY_SIZE(msm_bms_power_props);
	chip->bms_psy.get_property = qpnp_bms_power_get_property;
	chip->bms_psy.set_property = qpnp_bms_power_set_property;
	chip->bms_psy.external_power_changed =
		qpnp_bms_external_power_changed;
	chip->bms_psy.supplied_to = qpnp_bms_supplicants;
	chip->bms_psy.num_supplicants = ARRAY_SIZE(qpnp_bms_supplicants);

	rc = power_supply_register(chip->dev, &chip->bms_psy);

	if (rc < 0) {
		pr_err("power_supply_register bms failed rc = %d\n", rc);
		goto unregister_dc;
	}

	pr_info("probe success\n");
	return 0;

unregister_dc:
	power_supply_unregister(&chip->bms_psy);
	dev_set_drvdata(&spmi->dev, NULL);
error_read:
	kfree(chip);
	return rc;
}

static int 
qpnp_bms_remove(struct spmi_device *spmi)
{
	struct qpnp_bms_chip *chip = dev_get_drvdata(&spmi->dev);

	dev_set_drvdata(&spmi->dev, NULL);
	kfree(chip);
	return 0;
}

static struct spmi_driver qpnp_bms_driver = {
	.probe		= qpnp_bms_probe,
	.remove		= qpnp_bms_remove,
	.driver		= {
		.name		= QPNP_BMS_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= qpnp_bms_match_table,
	},
};

static int __init qpnp_bms_init(void)
{
	pr_info("QPNP BMS INIT\n");
	return spmi_driver_register(&qpnp_bms_driver);
}

static void __exit qpnp_bms_exit(void)
{
	pr_info("QPNP BMS EXIT\n");
	return spmi_driver_unregister(&qpnp_bms_driver);
}

module_init(qpnp_bms_init);
module_exit(qpnp_bms_exit);

MODULE_DESCRIPTION("QPNP BMS Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" QPNP_BMS_DEV_NAME);
