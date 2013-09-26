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

#define pr_fmt(fmt)	"PMIC PDN %s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/spmi.h>
#include <linux/delay.h>

#define KRAIT_REG_PMIC_DEV_NAME "qcom,krait-regulator-pmic"

#define REG_DIG_MAJOR		0x01
#define REG_DIG_MINOR		0x00

#define REG_PERPH_TYPE		0x04
#define CTRL_TYPE_VAL		0x1C
#define PS_TYPE_VAL		0x1C
#define FREQ_TYPE_VAL		0x1D

#define REG_PERPH_SUBTYPE	0x05
#define CTRL_SUBTYPE_VAL	0x08
#define PS_SUBTYPE_VAL		0x01
#define FREQ_SUBTYPE_VAL	0x19

#define REG_V_CTL1		0x40
#define V_CTL1_VAL		0x00

#define REG_MODE_CTL		0x45
#define NPM_MODE_BIT		BIT(7)
#define AUTO_MODE_BIT		BIT(6)

#define REG_EN_CTL		0x46
#define EN_BIT			BIT(7)

#define REG_PD_CTL		0x48
#define PD_CTL_VAL		0x08

#define REG_MULTIPHASE_CTL	0x51
#define MULTIPHASE_EN_BIT	BIT(7)

#define REG_PHASE_CTL		0x52
#define BALANCE_EN_BIT		BIT(7)

#define REG_VS_CTL		0x61
#define VS_CTL_VAL		0x85

#define REG_GANG_CTL2		0xC1
#define GANG_EN_BIT		BIT(7)

#define REG_PWM_CL			0x60

struct krait_vreg_pmic_chip {
	struct spmi_device	*spmi;
	u16			ctrl_base;
	u16			ps_base;
	u16			freq_base;
	u8			ctrl_dig_major;
	u8			ctrl_dig_minor;
};

static struct krait_vreg_pmic_chip *the_chip;

static struct of_device_id krait_vreg_pmic_match_table[] = {
	{ .compatible = KRAIT_REG_PMIC_DEV_NAME },
	{}
};

static int read_byte(struct spmi_device *spmi, u16 addr, u8 *val)
{
	int rc;

	rc = spmi_ext_register_readl(spmi->ctrl, spmi->sid, addr, val, 1);
	if (rc) {
		pr_err("SPMI read failed [%d,0x%04x] rc=%d\n",
							spmi->sid, addr, rc);
		return rc;
	}
	return 0;
}

static int write_byte(struct spmi_device *spmi, u16 addr, u8 *val)
{
	int rc;

	rc = spmi_ext_register_writel(spmi->ctrl, spmi->sid, addr, val, 1);
	if (rc) {
		pr_err("SPMI write failed [%d,0x%04x] val = 0x%02x rc=%d\n",
						spmi->sid, addr, *val, rc);
		return rc;
	}
	return 0;
}

#define ISTEP_MA			500
#define IOFFSET_MA			1000
#define OVERSHOOT_DIG_MAJOR		1
#define OVERSHOOT_DIG_MINOR		1
static bool v_overshoot_fixed(void)
{
	if (the_chip->ctrl_dig_major > OVERSHOOT_DIG_MAJOR
		|| (the_chip->ctrl_dig_major == OVERSHOOT_DIG_MAJOR
		&& the_chip->ctrl_dig_minor > OVERSHOOT_DIG_MINOR)) {
		pr_debug("fixed in h/w\n");
		return true;
	}
	return false;
}

/**
 * krait_pmic_is_ready - function to check if the driver is initialized
 *
 * CONTEXT: Can be called in atomic context
 *
 * RETURNS: true if this driver has initialized, false otherwise
 */
bool krait_pmic_is_ready(void)
{
	if (the_chip == NULL) {
		pr_debug("kait_regulator_pmic not ready yet\n");
		return false;
	}
	return true;
}
EXPORT_SYMBOL(krait_pmic_is_ready);

#define I_PFM_MA		2000

/**
 * krait_pmic_post_pfm_entry - workarounds after entering pfm mode
 *
 * CONTEXT: Can be called in atomic context
 *
 * RETURNS: 0 on success, error code on failure
 */
int krait_pmic_post_pfm_entry(void)
{
	u8 setpoint;
	int rc;

	if (the_chip == NULL) {
		pr_debug("kait_regulator_pmic not ready yet\n");
		return -ENXIO;
	}

	if (v_overshoot_fixed())
		return 0;

	setpoint = (I_PFM_MA - IOFFSET_MA) / ISTEP_MA;
	rc = write_byte(the_chip->spmi,
			the_chip->ps_base + REG_PWM_CL, &setpoint);
	pr_debug("wrote 0x%02x->[%d 0x%04x] rc = %d\n", setpoint,
			the_chip->spmi->sid,
			the_chip->ps_base + REG_PWM_CL, rc);
	return rc;
}
EXPORT_SYMBOL(krait_pmic_post_pfm_entry);

#define I_PWM_MA		3500
/**
 * krait_pmic_post_pwm_entry - workarounds after entering pwm mode
 *
 * CONTEXT: Can be called in atomic context
 *
 * RETURNS: 0 on success, error code on failure
 */
int krait_pmic_post_pwm_entry(void)
{
	u8 setpoint;
	int rc;

	if (the_chip == NULL) {
		pr_debug("kait_regulator_pmic not ready yet\n");
		return -ENXIO;
	}

	if (v_overshoot_fixed())
		return 0;

	udelay(50);
	setpoint = (I_PWM_MA - IOFFSET_MA) / ISTEP_MA;

	rc = write_byte(the_chip->spmi,
			the_chip->ps_base + REG_PWM_CL, &setpoint);
	pr_debug("wrote 0x%02x->[%d 0x%04x] rc = %d\n", setpoint,
			the_chip->spmi->sid,
			the_chip->ps_base + REG_PWM_CL, rc);
	return rc;
}
EXPORT_SYMBOL(krait_pmic_post_pwm_entry);

#define READ_BYTE(chip, addr, val, rc)				\
do {								\
	rc = read_byte(chip->spmi, (addr), &val);		\
	if (rc)						\
		pr_err("register read failed rc=%d\n", rc);	\
} while (0)

#define GANGED_VREG_COUNT	4
static int gang_configuration_check(struct krait_vreg_pmic_chip *chip)
{
	u8 val;
	int rc;
	int i;

	return 0;

	READ_BYTE(chip, chip->ctrl_base + REG_V_CTL1, val, rc);
	if (rc)
		return rc;
	BUG_ON(val != V_CTL1_VAL);

	READ_BYTE(chip, chip->ctrl_base + REG_MODE_CTL, val, rc);
	if (rc)
		return rc;
	/* The Auto mode should be off */
	BUG_ON(val & AUTO_MODE_BIT);
	/* The NPM mode should be on */
	BUG_ON(!(val & NPM_MODE_BIT));

	READ_BYTE(chip, chip->ctrl_base + REG_EN_CTL, val, rc);
	if (rc)
		return rc;
	/* The en bit should be set */
	BUG_ON(val & EN_BIT);

	READ_BYTE(chip, chip->ctrl_base + REG_PD_CTL, val, rc);
	if (rc)
		return rc;
	BUG_ON(val != PD_CTL_VAL);

	READ_BYTE(chip, chip->ctrl_base + REG_MULTIPHASE_CTL, val, rc);
	if (rc)
		return rc;
	BUG_ON(!(val & MULTIPHASE_EN_BIT));

	READ_BYTE(chip, chip->ctrl_base + REG_PHASE_CTL, val, rc);
	if (rc)
		return rc;
	BUG_ON(!(val & BALANCE_EN_BIT));

	READ_BYTE(chip, chip->ctrl_base + REG_VS_CTL, val, rc);
	if (rc)
		return rc;
	BUG_ON(val != VS_CTL_VAL);

	for (i = 0; i < GANGED_VREG_COUNT; i++) {
		READ_BYTE(chip,
			chip->ctrl_base + i * 0x300 + REG_GANG_CTL2, val, rc);
		if (rc)
			return rc;

		if (!(val & GANG_EN_BIT)) {
			pr_err("buck = %d, ctrl gang not enabled\n", i);
			BUG();
		}
	}

	for (i = 0; i < GANGED_VREG_COUNT; i++) {
		READ_BYTE(chip,
			chip->ps_base + i * 0x300 + REG_GANG_CTL2, val, rc);
		if (rc)
			return rc;

		if (!(val & GANG_EN_BIT)) {
			pr_err("buck = %d, ps gang not enabled\n", i);
			BUG();
		}
	}

	for (i = 0; i < GANGED_VREG_COUNT; i++) {
		READ_BYTE(chip,
			chip->freq_base + i * 0x300 + REG_GANG_CTL2, val, rc);
		if (rc)
			return rc;

		if (!(val & GANG_EN_BIT)) {
			pr_err("buck = %d, freq gang not enabled\n", i);
			BUG();
		}
	}
	return 0;
}

static int __devinit krait_vreg_pmic_probe(struct spmi_device *spmi)
{
	u8 type, subtype;
	int rc;
	struct krait_vreg_pmic_chip *chip;
	struct spmi_resource *spmi_resource;
	struct resource *resource;

	chip = devm_kzalloc(&spmi->dev, sizeof *chip, GFP_KERNEL);
	if (chip == NULL) {
		pr_err("kzalloc() failed.\n");
		return -ENOMEM;
	}

	chip->spmi = spmi;

	spmi_for_each_container_dev(spmi_resource, spmi) {
		if (!spmi_resource) {
			pr_err("spmi resource absent\n");
			return -ENXIO;
		}

		resource = spmi_get_resource(spmi, spmi_resource,
							IORESOURCE_MEM, 0);
		if (!(resource && resource->start)) {
			pr_err("node %s IO resource absent!\n",
					spmi->dev.of_node->full_name);
			return -ENXIO;
		}

		rc = read_byte(chip->spmi,
				resource->start + REG_PERPH_TYPE,
				&type);
		if (rc) {
			pr_err("Peripheral type read failed rc=%d\n", rc);
			return -ENXIO;
		}

		rc = read_byte(chip->spmi,
				resource->start + REG_PERPH_SUBTYPE,
				&subtype);
		if (rc) {
			pr_err("Peripheral subtype read failed rc=%d\n", rc);
			return -ENXIO;
		}

		if (type == CTRL_TYPE_VAL && subtype == CTRL_SUBTYPE_VAL)
			chip->ctrl_base = resource->start;
		else if (type == PS_TYPE_VAL && subtype == PS_SUBTYPE_VAL)
			chip->ps_base = resource->start;
		else if (type == FREQ_TYPE_VAL && subtype == FREQ_SUBTYPE_VAL)
			chip->freq_base = resource->start;
	}

	if (chip->ctrl_base == 0) {
		pr_err("ctrl base address missing\n");
		return -ENXIO;
	}

	if (chip->ps_base == 0) {
		pr_err("ps base address missing\n");
		return -ENXIO;
	}

	if (chip->freq_base == 0) {
		pr_err("freq base address missing\n");
		return -ENXIO;
	}

	READ_BYTE(chip, chip->ctrl_base + REG_DIG_MAJOR,
			chip->ctrl_dig_major, rc);
	if (rc)
		return rc;

	READ_BYTE(chip, chip->ctrl_base + REG_DIG_MINOR,
			chip->ctrl_dig_minor, rc);
	if (rc)
		return rc;

	gang_configuration_check(chip);

	the_chip = chip;
	return 0;
}

static struct spmi_driver qpnp_revid_driver = {
	.probe	= krait_vreg_pmic_probe,
	.driver	= {
		.name		= KRAIT_REG_PMIC_DEV_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= krait_vreg_pmic_match_table,
	},
};

static int __init qpnp_revid_init(void)
{
	return spmi_driver_register(&qpnp_revid_driver);
}

static void __exit qpnp_revid_exit(void)
{
	return spmi_driver_unregister(&qpnp_revid_driver);
}

module_init(qpnp_revid_init);
module_exit(qpnp_revid_exit);

MODULE_DESCRIPTION("KRAIT REGULATOR PMIC DRIVER");
MODULE_LICENSE("GPL v2");
