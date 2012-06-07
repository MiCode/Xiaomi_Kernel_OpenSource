/*
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/misc.h>

/* PON CTRL 1 register */
#define REG_PM8XXX_PON_CTRL_1			0x01C

#define PON_CTRL_1_PULL_UP_MASK			0xE0
#define PON_CTRL_1_USB_PWR_EN			0x10

#define PON_CTRL_1_WD_EN_MASK			0x08
#define PON_CTRL_1_WD_EN_RESET			0x08
#define PON_CTRL_1_WD_EN_PWR_OFF		0x00

/* PON CNTL registers */
#define REG_PM8058_PON_CNTL_4			0x098
#define REG_PM8901_PON_CNTL_4			0x099
#define REG_PM8018_PON_CNTL_4			0x01E
#define REG_PM8921_PON_CNTL_4			0x01E
#define REG_PM8058_PON_CNTL_5			0x07B
#define REG_PM8901_PON_CNTL_5			0x09A
#define REG_PM8018_PON_CNTL_5			0x01F
#define REG_PM8921_PON_CNTL_5			0x01F

#define PON_CTRL_4_RESET_EN_MASK		0x01
#define PON_CTRL_4_SHUTDOWN_ON_RESET		0x0
#define PON_CTRL_4_RESTART_ON_RESET		0x1
#define PON_CTRL_5_HARD_RESET_EN_MASK		0x08
#define PON_CTRL_5_HARD_RESET_EN		0x08
#define PON_CTRL_5_HARD_RESET_DIS		0x00

/* Regulator master enable addresses */
#define REG_PM8058_VREG_EN_MSM			0x018
#define REG_PM8058_VREG_EN_GRP_5_4		0x1C8

/* Regulator control registers for shutdown/reset */
#define REG_PM8058_S0_CTRL			0x004
#define REG_PM8058_S1_CTRL			0x005
#define REG_PM8058_S3_CTRL			0x111
#define REG_PM8058_L21_CTRL			0x120
#define REG_PM8058_L22_CTRL			0x121

#define PM8058_REGULATOR_ENABLE_MASK		0x80
#define PM8058_REGULATOR_ENABLE			0x80
#define PM8058_REGULATOR_DISABLE		0x00
#define PM8058_REGULATOR_PULL_DOWN_MASK		0x40
#define PM8058_REGULATOR_PULL_DOWN_EN		0x40

/* Buck CTRL register */
#define PM8058_SMPS_LEGACY_VREF_SEL		0x20
#define PM8058_SMPS_LEGACY_VPROG_MASK		0x1F
#define PM8058_SMPS_ADVANCED_BAND_MASK		0xC0
#define PM8058_SMPS_ADVANCED_BAND_SHIFT		6
#define PM8058_SMPS_ADVANCED_VPROG_MASK		0x3F

/* Buck TEST2 registers for shutdown/reset */
#define REG_PM8058_S0_TEST2			0x084
#define REG_PM8058_S1_TEST2			0x085
#define REG_PM8058_S3_TEST2			0x11A

#define PM8058_REGULATOR_BANK_WRITE		0x80
#define PM8058_REGULATOR_BANK_MASK		0x70
#define PM8058_REGULATOR_BANK_SHIFT		4
#define PM8058_REGULATOR_BANK_SEL(n)	((n) << PM8058_REGULATOR_BANK_SHIFT)

/* Buck TEST2 register bank 1 */
#define PM8058_SMPS_LEGACY_VLOW_SEL		0x01

/* Buck TEST2 register bank 7 */
#define PM8058_SMPS_ADVANCED_MODE_MASK		0x02
#define PM8058_SMPS_ADVANCED_MODE		0x02
#define PM8058_SMPS_LEGACY_MODE			0x00

/* SLEEP CTRL register */
#define REG_PM8058_SLEEP_CTRL			0x02B
#define REG_PM8921_SLEEP_CTRL			0x10A
#define REG_PM8018_SLEEP_CTRL			0x10A

#define SLEEP_CTRL_SMPL_EN_MASK			0x04
#define SLEEP_CTRL_SMPL_EN_RESET		0x04
#define SLEEP_CTRL_SMPL_EN_PWR_OFF		0x00

#define SLEEP_CTRL_SMPL_SEL_MASK		0x03
#define SLEEP_CTRL_SMPL_SEL_MIN			0
#define SLEEP_CTRL_SMPL_SEL_MAX			3

/* FTS regulator PMR registers */
#define REG_PM8901_REGULATOR_S1_PMR		0xA7
#define REG_PM8901_REGULATOR_S2_PMR		0xA8
#define REG_PM8901_REGULATOR_S3_PMR		0xA9
#define REG_PM8901_REGULATOR_S4_PMR		0xAA

#define PM8901_REGULATOR_PMR_STATE_MASK		0x60
#define PM8901_REGULATOR_PMR_STATE_OFF		0x20

/* COINCELL CHG registers */
#define REG_PM8058_COIN_CHG			0x02F
#define REG_PM8921_COIN_CHG			0x09C
#define REG_PM8018_COIN_CHG			0x09C

#define COINCELL_RESISTOR_SHIFT			0x2

/* GP TEST register */
#define REG_PM8XXX_GP_TEST_1			0x07A

/* Stay on configuration */
#define PM8XXX_STAY_ON_CFG			0x92

/* GPIO UART MUX CTRL registers */
#define REG_PM8XXX_GPIO_MUX_CTRL		0x1CC

#define UART_PATH_SEL_MASK			0x60
#define UART_PATH_SEL_SHIFT			0x5

#define USB_ID_PU_EN_MASK			0x10	/* PM8921 family only */
#define USB_ID_PU_EN_SHIFT			4

/* Shutdown/restart delays to allow for LDO 7/dVdd regulator load settling. */
#define PM8901_DELAY_AFTER_REG_DISABLE_MS	4
#define PM8901_DELAY_BEFORE_SHUTDOWN_MS		8

#define REG_PM8XXX_XO_CNTRL_2	0x114
#define MP3_1_MASK	0xE0
#define MP3_2_MASK	0x1C
#define MP3_1_SHIFT	5
#define MP3_2_SHIFT	2

#define REG_HSED_BIAS0_CNTL2		0xA1
#define REG_HSED_BIAS1_CNTL2		0x135
#define REG_HSED_BIAS2_CNTL2		0x138
#define HSED_EN_MASK			0xC0

struct pm8xxx_misc_chip {
	struct list_head			link;
	struct pm8xxx_misc_platform_data	pdata;
	struct device				*dev;
	enum pm8xxx_version			version;
	u64					osc_halt_count;
};

static LIST_HEAD(pm8xxx_misc_chips);
static DEFINE_SPINLOCK(pm8xxx_misc_chips_lock);

static int pm8xxx_misc_masked_write(struct pm8xxx_misc_chip *chip, u16 addr,
				    u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = pm8xxx_readb(chip->dev->parent, addr, &reg);
	if (rc) {
		pr_err("pm8xxx_readb(0x%03X) failed, rc=%d\n", addr, rc);
		return rc;
	}
	reg &= ~mask;
	reg |= val & mask;
	rc = pm8xxx_writeb(chip->dev->parent, addr, reg);
	if (rc)
		pr_err("pm8xxx_writeb(0x%03X)=0x%02X failed, rc=%d\n", addr,
			reg, rc);
	return rc;
}

/*
 * Set an SMPS regulator to be disabled in its CTRL register, but enabled
 * in the master enable register.  Also set it's pull down enable bit.
 * Take care to make sure that the output voltage doesn't change if switching
 * from advanced mode to legacy mode.
 */
static int
__pm8058_disable_smps_locally_set_pull_down(struct pm8xxx_misc_chip *chip,
	u16 ctrl_addr, u16 test2_addr, u16 master_enable_addr,
	u8 master_enable_bit)
{
	int rc = 0;
	u8 vref_sel, vlow_sel, band, vprog, bank, reg;

	bank = PM8058_REGULATOR_BANK_SEL(7);
	rc = pm8xxx_writeb(chip->dev->parent, test2_addr, bank);
	if (rc) {
		pr_err("%s: pm8xxx_writeb(0x%03X) failed: rc=%d\n", __func__,
			test2_addr, rc);
		goto done;
	}

	rc = pm8xxx_readb(chip->dev->parent, test2_addr, &reg);
	if (rc) {
		pr_err("%s: FAIL pm8xxx_readb(0x%03X): rc=%d\n",
		       __func__, test2_addr, rc);
		goto done;
	}

	/* Check if in advanced mode. */
	if ((reg & PM8058_SMPS_ADVANCED_MODE_MASK) ==
					PM8058_SMPS_ADVANCED_MODE) {
		/* Determine current output voltage. */
		rc = pm8xxx_readb(chip->dev->parent, ctrl_addr, &reg);
		if (rc) {
			pr_err("%s: FAIL pm8xxx_readb(0x%03X): rc=%d\n",
			       __func__, ctrl_addr, rc);
			goto done;
		}

		band = (reg & PM8058_SMPS_ADVANCED_BAND_MASK)
			>> PM8058_SMPS_ADVANCED_BAND_SHIFT;
		switch (band) {
		case 3:
			vref_sel = 0;
			vlow_sel = 0;
			break;
		case 2:
			vref_sel = PM8058_SMPS_LEGACY_VREF_SEL;
			vlow_sel = 0;
			break;
		case 1:
			vref_sel = PM8058_SMPS_LEGACY_VREF_SEL;
			vlow_sel = PM8058_SMPS_LEGACY_VLOW_SEL;
			break;
		default:
			pr_err("%s: regulator already disabled\n", __func__);
			return -EPERM;
		}
		vprog = (reg & PM8058_SMPS_ADVANCED_VPROG_MASK);
		/* Round up if fine step is in use. */
		vprog = (vprog + 1) >> 1;
		if (vprog > PM8058_SMPS_LEGACY_VPROG_MASK)
			vprog = PM8058_SMPS_LEGACY_VPROG_MASK;

		/* Set VLOW_SEL bit. */
		bank = PM8058_REGULATOR_BANK_SEL(1);
		rc = pm8xxx_writeb(chip->dev->parent, test2_addr, bank);
		if (rc) {
			pr_err("%s: FAIL pm8xxx_writeb(0x%03X): rc=%d\n",
			       __func__, test2_addr, rc);
			goto done;
		}

		rc = pm8xxx_misc_masked_write(chip, test2_addr,
			PM8058_REGULATOR_BANK_WRITE | PM8058_REGULATOR_BANK_MASK
				| PM8058_SMPS_LEGACY_VLOW_SEL,
			PM8058_REGULATOR_BANK_WRITE |
			PM8058_REGULATOR_BANK_SEL(1) | vlow_sel);
		if (rc)
			goto done;

		/* Switch to legacy mode */
		bank = PM8058_REGULATOR_BANK_SEL(7);
		rc = pm8xxx_writeb(chip->dev->parent, test2_addr, bank);
		if (rc) {
			pr_err("%s: FAIL pm8xxx_writeb(0x%03X): rc=%d\n",
					__func__, test2_addr, rc);
			goto done;
		}
		rc = pm8xxx_misc_masked_write(chip, test2_addr,
				PM8058_REGULATOR_BANK_WRITE |
				PM8058_REGULATOR_BANK_MASK |
				PM8058_SMPS_ADVANCED_MODE_MASK,
				PM8058_REGULATOR_BANK_WRITE |
				PM8058_REGULATOR_BANK_SEL(7) |
				PM8058_SMPS_LEGACY_MODE);
		if (rc)
			goto done;

		/* Enable locally, enable pull down, keep voltage the same. */
		rc = pm8xxx_misc_masked_write(chip, ctrl_addr,
			PM8058_REGULATOR_ENABLE_MASK |
			PM8058_REGULATOR_PULL_DOWN_MASK |
			PM8058_SMPS_LEGACY_VREF_SEL |
			PM8058_SMPS_LEGACY_VPROG_MASK,
			PM8058_REGULATOR_ENABLE | PM8058_REGULATOR_PULL_DOWN_EN
				| vref_sel | vprog);
		if (rc)
			goto done;
	}

	/* Enable in master control register. */
	rc = pm8xxx_misc_masked_write(chip, master_enable_addr,
			master_enable_bit, master_enable_bit);
	if (rc)
		goto done;

	/* Disable locally and enable pull down. */
	rc = pm8xxx_misc_masked_write(chip, ctrl_addr,
		PM8058_REGULATOR_ENABLE_MASK | PM8058_REGULATOR_PULL_DOWN_MASK,
		PM8058_REGULATOR_DISABLE | PM8058_REGULATOR_PULL_DOWN_EN);

done:
	return rc;
}

static int
__pm8058_disable_ldo_locally_set_pull_down(struct pm8xxx_misc_chip *chip,
		u16 ctrl_addr, u16 master_enable_addr, u8 master_enable_bit)
{
	int rc;

	/* Enable LDO in master control register. */
	rc = pm8xxx_misc_masked_write(chip, master_enable_addr,
			master_enable_bit, master_enable_bit);
	if (rc)
		goto done;

	/* Disable LDO in CTRL register and set pull down */
	rc = pm8xxx_misc_masked_write(chip, ctrl_addr,
		PM8058_REGULATOR_ENABLE_MASK | PM8058_REGULATOR_PULL_DOWN_MASK,
		PM8058_REGULATOR_DISABLE | PM8058_REGULATOR_PULL_DOWN_EN);

done:
	return rc;
}

static int __pm8018_reset_pwr_off(struct pm8xxx_misc_chip *chip, int reset)
{
	int rc;

	/* Enable SMPL if resetting is desired. */
	rc = pm8xxx_misc_masked_write(chip, REG_PM8018_SLEEP_CTRL,
	       SLEEP_CTRL_SMPL_EN_MASK,
	       (reset ? SLEEP_CTRL_SMPL_EN_RESET : SLEEP_CTRL_SMPL_EN_PWR_OFF));
	if (rc) {
		pr_err("pm8xxx_misc_masked_write failed, rc=%d\n", rc);
		return rc;
	}

	/*
	 * Select action to perform (reset or shutdown) when PS_HOLD goes low.
	 * Also ensure that KPD, CBL0, and CBL1 pull ups are enabled and that
	 * USB charging is enabled.
	 */
	rc = pm8xxx_misc_masked_write(chip, REG_PM8XXX_PON_CTRL_1,
		PON_CTRL_1_PULL_UP_MASK | PON_CTRL_1_USB_PWR_EN
		| PON_CTRL_1_WD_EN_MASK,
		PON_CTRL_1_PULL_UP_MASK | PON_CTRL_1_USB_PWR_EN
		| (reset ? PON_CTRL_1_WD_EN_RESET : PON_CTRL_1_WD_EN_PWR_OFF));
	if (rc)
		pr_err("pm8xxx_misc_masked_write failed, rc=%d\n", rc);

	return rc;
}

static int __pm8058_reset_pwr_off(struct pm8xxx_misc_chip *chip, int reset)
{
	int rc;

	/* When shutting down, enable active pulldowns on important rails. */
	if (!reset) {
		/* Disable SMPS's 0,1,3 locally and set pulldown enable bits. */
		__pm8058_disable_smps_locally_set_pull_down(chip,
			REG_PM8058_S0_CTRL, REG_PM8058_S0_TEST2,
			REG_PM8058_VREG_EN_MSM, BIT(7));
		__pm8058_disable_smps_locally_set_pull_down(chip,
			REG_PM8058_S1_CTRL, REG_PM8058_S1_TEST2,
			REG_PM8058_VREG_EN_MSM, BIT(6));
		__pm8058_disable_smps_locally_set_pull_down(chip,
			REG_PM8058_S3_CTRL, REG_PM8058_S3_TEST2,
			REG_PM8058_VREG_EN_GRP_5_4, BIT(7) | BIT(4));
		/* Disable LDO 21 locally and set pulldown enable bit. */
		__pm8058_disable_ldo_locally_set_pull_down(chip,
			REG_PM8058_L21_CTRL, REG_PM8058_VREG_EN_GRP_5_4,
			BIT(1));
	}

	/*
	 * Fix-up: Set regulator LDO22 to 1.225 V in high power mode. Leave its
	 * pull-down state intact. This ensures a safe shutdown.
	 */
	rc = pm8xxx_misc_masked_write(chip, REG_PM8058_L22_CTRL, 0xBF, 0x93);
	if (rc) {
		pr_err("pm8xxx_misc_masked_write failed, rc=%d\n", rc);
		goto read_write_err;
	}

	/* Enable SMPL if resetting is desired. */
	rc = pm8xxx_misc_masked_write(chip, REG_PM8058_SLEEP_CTRL,
	       SLEEP_CTRL_SMPL_EN_MASK,
	       (reset ? SLEEP_CTRL_SMPL_EN_RESET : SLEEP_CTRL_SMPL_EN_PWR_OFF));
	if (rc) {
		pr_err("pm8xxx_misc_masked_write failed, rc=%d\n", rc);
		goto read_write_err;
	}

	/*
	 * Select action to perform (reset or shutdown) when PS_HOLD goes low.
	 * Also ensure that KPD, CBL0, and CBL1 pull ups are enabled and that
	 * USB charging is enabled.
	 */
	rc = pm8xxx_misc_masked_write(chip, REG_PM8XXX_PON_CTRL_1,
		PON_CTRL_1_PULL_UP_MASK | PON_CTRL_1_USB_PWR_EN
		| PON_CTRL_1_WD_EN_MASK,
		PON_CTRL_1_PULL_UP_MASK | PON_CTRL_1_USB_PWR_EN
		| (reset ? PON_CTRL_1_WD_EN_RESET : PON_CTRL_1_WD_EN_PWR_OFF));
	if (rc) {
		pr_err("pm8xxx_misc_masked_write failed, rc=%d\n", rc);
		goto read_write_err;
	}

read_write_err:
	return rc;
}

static int __pm8901_reset_pwr_off(struct pm8xxx_misc_chip *chip, int reset)
{
	int rc = 0, i;
	u8 pmr_addr[4] = {
		REG_PM8901_REGULATOR_S2_PMR,
		REG_PM8901_REGULATOR_S3_PMR,
		REG_PM8901_REGULATOR_S4_PMR,
		REG_PM8901_REGULATOR_S1_PMR,
	};

	/* Fix-up: Turn off regulators S1, S2, S3, S4 when shutting down. */
	if (!reset) {
		for (i = 0; i < 4; i++) {
			rc = pm8xxx_misc_masked_write(chip, pmr_addr[i],
				PM8901_REGULATOR_PMR_STATE_MASK,
				PM8901_REGULATOR_PMR_STATE_OFF);
			if (rc) {
				pr_err("pm8xxx_misc_masked_write failed, "
					"rc=%d\n", rc);
				goto read_write_err;
			}
			mdelay(PM8901_DELAY_AFTER_REG_DISABLE_MS);
		}
	}

read_write_err:
	mdelay(PM8901_DELAY_BEFORE_SHUTDOWN_MS);
	return rc;
}

static int __pm8921_reset_pwr_off(struct pm8xxx_misc_chip *chip, int reset)
{
	int rc;

	/* Enable SMPL if resetting is desired. */
	rc = pm8xxx_misc_masked_write(chip, REG_PM8921_SLEEP_CTRL,
	       SLEEP_CTRL_SMPL_EN_MASK,
	       (reset ? SLEEP_CTRL_SMPL_EN_RESET : SLEEP_CTRL_SMPL_EN_PWR_OFF));
	if (rc) {
		pr_err("pm8xxx_misc_masked_write failed, rc=%d\n", rc);
		goto read_write_err;
	}

	/*
	 * Select action to perform (reset or shutdown) when PS_HOLD goes low.
	 * Also ensure that KPD, CBL0, and CBL1 pull ups are enabled and that
	 * USB charging is enabled.
	 */
	rc = pm8xxx_misc_masked_write(chip, REG_PM8XXX_PON_CTRL_1,
		PON_CTRL_1_PULL_UP_MASK | PON_CTRL_1_USB_PWR_EN
		| PON_CTRL_1_WD_EN_MASK,
		PON_CTRL_1_PULL_UP_MASK | PON_CTRL_1_USB_PWR_EN
		| (reset ? PON_CTRL_1_WD_EN_RESET : PON_CTRL_1_WD_EN_PWR_OFF));
	if (rc) {
		pr_err("pm8xxx_misc_masked_write failed, rc=%d\n", rc);
		goto read_write_err;
	}

read_write_err:
	return rc;
}

/**
 * pm8xxx_reset_pwr_off - switch all PM8XXX PMIC chips attached to the system to
 *			  either reset or shutdown when they are turned off
 * @reset: 0 = shudown the PMICs, 1 = shutdown and then restart the PMICs
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_reset_pwr_off(int reset)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8018:
			rc = __pm8018_reset_pwr_off(chip, reset);
			break;
		case PM8XXX_VERSION_8058:
			rc = __pm8058_reset_pwr_off(chip, reset);
			break;
		case PM8XXX_VERSION_8901:
			rc = __pm8901_reset_pwr_off(chip, reset);
			break;
		case PM8XXX_VERSION_8038:
		case PM8XXX_VERSION_8917:
		case PM8XXX_VERSION_8921:
			rc = __pm8921_reset_pwr_off(chip, reset);
			break;
		default:
			/* PMIC doesn't have reset_pwr_off; do nothing. */
			break;
		}
		if (rc) {
			pr_err("reset_pwr_off failed, rc=%d\n", rc);
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return rc;
}
EXPORT_SYMBOL_GPL(pm8xxx_reset_pwr_off);

/**
 * pm8xxx_smpl_control - enables/disables SMPL detection
 * @enable: 0 = shutdown PMIC on power loss, 1 = reset PMIC on power loss
 *
 * This function enables or disables the Sudden Momentary Power Loss detection
 * module.  If SMPL detection is enabled, then when a sufficiently long power
 * loss event occurs, the PMIC will automatically reset itself.  If SMPL
 * detection is disabled, then the PMIC will shutdown when power loss occurs.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_smpl_control(int enable)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8018:
			rc = pm8xxx_misc_masked_write(chip,
				REG_PM8018_SLEEP_CTRL, SLEEP_CTRL_SMPL_EN_MASK,
				(enable ? SLEEP_CTRL_SMPL_EN_RESET
					   : SLEEP_CTRL_SMPL_EN_PWR_OFF));
			break;
		case PM8XXX_VERSION_8058:
			rc = pm8xxx_misc_masked_write(chip,
				REG_PM8058_SLEEP_CTRL, SLEEP_CTRL_SMPL_EN_MASK,
				(enable ? SLEEP_CTRL_SMPL_EN_RESET
					   : SLEEP_CTRL_SMPL_EN_PWR_OFF));
			break;
		case PM8XXX_VERSION_8921:
			rc = pm8xxx_misc_masked_write(chip,
				REG_PM8921_SLEEP_CTRL, SLEEP_CTRL_SMPL_EN_MASK,
				(enable ? SLEEP_CTRL_SMPL_EN_RESET
					   : SLEEP_CTRL_SMPL_EN_PWR_OFF));
			break;
		default:
			/* PMIC doesn't have reset_pwr_off; do nothing. */
			break;
		}
		if (rc) {
			pr_err("setting smpl control failed, rc=%d\n", rc);
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_smpl_control);


/**
 * pm8xxx_smpl_set_delay - sets the SMPL detection time delay
 * @delay: enum value corresponding to delay time
 *
 * This function sets the time delay of the SMPL detection module.  If power
 * is reapplied within this interval, then the PMIC reset automatically.  The
 * SMPL detection module must be enabled for this delay time to take effect.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_smpl_set_delay(enum pm8xxx_smpl_delay delay)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	int rc = 0;

	if (delay < SLEEP_CTRL_SMPL_SEL_MIN
	    || delay > SLEEP_CTRL_SMPL_SEL_MAX) {
		pr_err("%s: invalid delay specified: %d\n", __func__, delay);
		return -EINVAL;
	}

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8018:
			rc = pm8xxx_misc_masked_write(chip,
				REG_PM8018_SLEEP_CTRL, SLEEP_CTRL_SMPL_SEL_MASK,
				delay);
			break;
		case PM8XXX_VERSION_8058:
			rc = pm8xxx_misc_masked_write(chip,
				REG_PM8058_SLEEP_CTRL, SLEEP_CTRL_SMPL_SEL_MASK,
				delay);
			break;
		case PM8XXX_VERSION_8921:
			rc = pm8xxx_misc_masked_write(chip,
				REG_PM8921_SLEEP_CTRL, SLEEP_CTRL_SMPL_SEL_MASK,
				delay);
			break;
		default:
			/* PMIC doesn't have reset_pwr_off; do nothing. */
			break;
		}
		if (rc) {
			pr_err("setting smpl delay failed, rc=%d\n", rc);
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_smpl_set_delay);

/**
 * pm8xxx_coincell_chg_config - Disables or enables the coincell charger, and
 *				configures its voltage and resistor settings.
 * @chg_config:			Holds both voltage and resistor values, and a
 *				switch to change the state of charger.
 *				If state is to disable the charger then
 *				both voltage and resistor are disregarded.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_coincell_chg_config(struct pm8xxx_coincell_chg *chg_config)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	u8 reg = 0, voltage, resistor;
	int rc = 0;

	if (chg_config == NULL) {
		pr_err("chg_config is NULL\n");
		return -EINVAL;
	}

	voltage = chg_config->voltage;
	resistor = chg_config->resistor;

	if (resistor < PM8XXX_COINCELL_RESISTOR_2100_OHMS ||
			resistor > PM8XXX_COINCELL_RESISTOR_800_OHMS) {
		pr_err("Invalid resistor value provided\n");
		return -EINVAL;
	}

	if (voltage < PM8XXX_COINCELL_VOLTAGE_3p2V ||
		(voltage > PM8XXX_COINCELL_VOLTAGE_3p0V &&
			voltage != PM8XXX_COINCELL_VOLTAGE_2p5V)) {
		pr_err("Invalid voltage value provided\n");
		return -EINVAL;
	}

	if (chg_config->state == PM8XXX_COINCELL_CHG_DISABLE) {
		reg = 0;
	} else {
		reg |= voltage;
		reg |= (resistor << COINCELL_RESISTOR_SHIFT);
	}

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8018:
			rc = pm8xxx_writeb(chip->dev->parent,
					REG_PM8018_COIN_CHG, reg);
			break;
		case PM8XXX_VERSION_8058:
			rc = pm8xxx_writeb(chip->dev->parent,
					REG_PM8058_COIN_CHG, reg);
			break;
		case PM8XXX_VERSION_8921:
			rc = pm8xxx_writeb(chip->dev->parent,
					REG_PM8921_COIN_CHG, reg);
			break;
		default:
			/* PMIC doesn't have reset_pwr_off; do nothing. */
			break;
		}
		if (rc) {
			pr_err("coincell chg. config failed, rc=%d\n", rc);
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_coincell_chg_config);

/**
 * pm8xxx_watchdog_reset_control - enables/disables watchdog reset detection
 * @enable: 0 = shutdown when PS_HOLD goes low, 1 = reset when PS_HOLD goes low
 *
 * This function enables or disables the PMIC watchdog reset detection feature.
 * If watchdog reset detection is enabled, then the PMIC will reset itself
 * when PS_HOLD goes low.  If it is not enabled, then the PMIC will shutdown
 * when PS_HOLD goes low.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_watchdog_reset_control(int enable)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8018:
		case PM8XXX_VERSION_8058:
		case PM8XXX_VERSION_8921:
			rc = pm8xxx_misc_masked_write(chip,
				REG_PM8XXX_PON_CTRL_1, PON_CTRL_1_WD_EN_MASK,
				(enable ? PON_CTRL_1_WD_EN_RESET
					   : PON_CTRL_1_WD_EN_PWR_OFF));
			break;
		default:
			/* WD reset control not supported */
			break;
		}
		if (rc) {
			pr_err("setting WD reset control failed, rc=%d\n", rc);
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_watchdog_reset_control);

/**
 * pm8xxx_stay_on - enables stay_on feature
 *
 * PMIC stay-on feature allows PMIC to ignore MSM PS_HOLD=low
 * signal so that some special functions like debugging could be
 * performed.
 *
 * This feature should not be used in any product release.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_stay_on(void)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8018:
		case PM8XXX_VERSION_8058:
		case PM8XXX_VERSION_8921:
			rc = pm8xxx_writeb(chip->dev->parent,
				REG_PM8XXX_GP_TEST_1, PM8XXX_STAY_ON_CFG);
			break;
		default:
			/* stay on not supported */
			break;
		}
		if (rc) {
			pr_err("stay_on failed failed, rc=%d\n", rc);
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_stay_on);

static int
__pm8xxx_hard_reset_config(struct pm8xxx_misc_chip *chip,
		enum pm8xxx_pon_config config, u16 pon4_addr, u16 pon5_addr)
{
	int rc = 0;

	switch (config) {
	case PM8XXX_DISABLE_HARD_RESET:
		rc = pm8xxx_misc_masked_write(chip, pon5_addr,
				PON_CTRL_5_HARD_RESET_EN_MASK,
				PON_CTRL_5_HARD_RESET_DIS);
		break;
	case PM8XXX_SHUTDOWN_ON_HARD_RESET:
		rc = pm8xxx_misc_masked_write(chip, pon5_addr,
				PON_CTRL_5_HARD_RESET_EN_MASK,
				PON_CTRL_5_HARD_RESET_EN);
		if (!rc) {
			rc = pm8xxx_misc_masked_write(chip, pon4_addr,
					PON_CTRL_4_RESET_EN_MASK,
					PON_CTRL_4_SHUTDOWN_ON_RESET);
		}
		break;
	case PM8XXX_RESTART_ON_HARD_RESET:
		rc = pm8xxx_misc_masked_write(chip, pon5_addr,
				PON_CTRL_5_HARD_RESET_EN_MASK,
				PON_CTRL_5_HARD_RESET_EN);
		if (!rc) {
			rc = pm8xxx_misc_masked_write(chip, pon4_addr,
					PON_CTRL_4_RESET_EN_MASK,
					PON_CTRL_4_RESTART_ON_RESET);
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}

/**
 * pm8xxx_hard_reset_config - Allows different reset configurations
 *
 * config = PM8XXX_DISABLE_HARD_RESET to disable hard reset
 *	  = PM8XXX_SHUTDOWN_ON_HARD_RESET to turn off the system on hard reset
 *	  = PM8XXX_RESTART_ON_HARD_RESET to restart the system on hard reset
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_hard_reset_config(enum pm8xxx_pon_config config)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8018:
			__pm8xxx_hard_reset_config(chip, config,
				REG_PM8018_PON_CNTL_4, REG_PM8018_PON_CNTL_5);
			break;
		case PM8XXX_VERSION_8058:
			__pm8xxx_hard_reset_config(chip, config,
				REG_PM8058_PON_CNTL_4, REG_PM8058_PON_CNTL_5);
			break;
		case PM8XXX_VERSION_8901:
			__pm8xxx_hard_reset_config(chip, config,
				REG_PM8901_PON_CNTL_4, REG_PM8901_PON_CNTL_5);
			break;
		case PM8XXX_VERSION_8921:
			__pm8xxx_hard_reset_config(chip, config,
				REG_PM8921_PON_CNTL_4, REG_PM8921_PON_CNTL_5);
			break;
		default:
			/* hard reset config. no supported */
			break;
		}
		if (rc) {
			pr_err("hard reset config. failed, rc=%d\n", rc);
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_hard_reset_config);

/* Handle the OSC_HALT interrupt: 32 kHz XTAL oscillator has stopped. */
static irqreturn_t pm8xxx_osc_halt_isr(int irq, void *data)
{
	struct pm8xxx_misc_chip *chip = data;
	u64 count = 0;

	if (chip) {
		chip->osc_halt_count++;
		count = chip->osc_halt_count;
	}

	pr_crit("%s: OSC_HALT interrupt has triggered, 32 kHz XTAL oscillator"
				" has halted (%llu)!\n", __func__, count);

	return IRQ_HANDLED;
}

/**
 * pm8xxx_uart_gpio_mux_ctrl - Mux configuration to select the UART
 *
 * @uart_path_sel: Input argument to select either UART1/2/3
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_uart_gpio_mux_ctrl(enum pm8xxx_uart_path_sel uart_path_sel)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8018:
		case PM8XXX_VERSION_8058:
		case PM8XXX_VERSION_8921:
			rc = pm8xxx_misc_masked_write(chip,
				REG_PM8XXX_GPIO_MUX_CTRL, UART_PATH_SEL_MASK,
				uart_path_sel << UART_PATH_SEL_SHIFT);
			break;
		default:
			/* Functionality not supported */
			break;
		}
		if (rc) {
			pr_err("uart_gpio_mux_ctrl failed, rc=%d\n", rc);
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_uart_gpio_mux_ctrl);

/**
 * pm8xxx_usb_id_pullup - Control a pullup for USB ID
 *
 * @enable: enable (1) or disable (0) the pullup
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_usb_id_pullup(int enable)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	int rc = -ENXIO;

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8921:
		case PM8XXX_VERSION_8922:
		case PM8XXX_VERSION_8917:
		case PM8XXX_VERSION_8038:
			rc = pm8xxx_misc_masked_write(chip,
				REG_PM8XXX_GPIO_MUX_CTRL, USB_ID_PU_EN_MASK,
				enable << USB_ID_PU_EN_SHIFT);

			if (rc)
				pr_err("Fail: reg=%x, rc=%d\n",
				       REG_PM8XXX_GPIO_MUX_CTRL, rc);
			break;
		default:
			/* Functionality not supported */
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_usb_id_pullup);

static int __pm8901_preload_dVdd(struct pm8xxx_misc_chip *chip)
{
	int rc;

	/* dVdd preloading is not needed for PMIC PM8901 rev 2.3 and beyond. */
	if (pm8xxx_get_revision(chip->dev->parent) >= PM8XXX_REVISION_8901_2p3)
		return 0;

	rc = pm8xxx_writeb(chip->dev->parent, 0x0BD, 0x0F);
	if (rc)
		pr_err("pm8xxx_writeb failed for 0x0BD, rc=%d\n", rc);

	rc = pm8xxx_writeb(chip->dev->parent, 0x001, 0xB4);
	if (rc)
		pr_err("pm8xxx_writeb failed for 0x001, rc=%d\n", rc);

	pr_info("dVdd preloaded\n");

	return rc;
}

/**
 * pm8xxx_preload_dVdd - preload the dVdd regulator during off state.
 *
 * This can help to reduce fluctuations in the dVdd voltage during startup
 * at the cost of additional off state current draw.
 *
 * This API should only be called if dVdd startup issues are suspected.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8xxx_preload_dVdd(void)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	int rc = 0;

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8901:
			rc = __pm8901_preload_dVdd(chip);
			break;
		default:
			/* PMIC doesn't have preload_dVdd; do nothing. */
			break;
		}
		if (rc) {
			pr_err("preload_dVdd failed, rc=%d\n", rc);
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return rc;
}
EXPORT_SYMBOL_GPL(pm8xxx_preload_dVdd);

int pm8xxx_aux_clk_control(enum pm8xxx_aux_clk_id clk_id,
				enum pm8xxx_aux_clk_div divider, bool enable)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	u8 clk_mask = 0, value = 0;

	if (clk_id == CLK_MP3_1) {
		clk_mask = MP3_1_MASK;
		value = divider << MP3_1_SHIFT;
	} else if (clk_id == CLK_MP3_2) {
		clk_mask = MP3_2_MASK;
		value = divider << MP3_2_SHIFT;
	} else {
		pr_err("Invalid clock id of %d\n", clk_id);
		return -EINVAL;
	}
	if (!enable)
		value = 0;

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8038:
		case PM8XXX_VERSION_8921:
			pm8xxx_misc_masked_write(chip,
					REG_PM8XXX_XO_CNTRL_2, clk_mask, value);
			break;
		default:
			/* Functionality not supported */
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(pm8xxx_aux_clk_control);

int pm8xxx_hsed_bias_control(enum pm8xxx_hsed_bias bias, bool enable)
{
	struct pm8xxx_misc_chip *chip;
	unsigned long flags;
	int rc = 0;
	u16 addr;

	switch (bias) {
	case PM8XXX_HSED_BIAS0:
		addr = REG_HSED_BIAS0_CNTL2;
		break;
	case PM8XXX_HSED_BIAS1:
		addr = REG_HSED_BIAS1_CNTL2;
		break;
	case PM8XXX_HSED_BIAS2:
		addr = REG_HSED_BIAS2_CNTL2;
		break;
	default:
		pr_err("Invalid BIAS line\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);

	/* Loop over all attached PMICs and call specific functions for them. */
	list_for_each_entry(chip, &pm8xxx_misc_chips, link) {
		switch (chip->version) {
		case PM8XXX_VERSION_8058:
		case PM8XXX_VERSION_8921:
			rc = pm8xxx_misc_masked_write(chip, addr,
				HSED_EN_MASK, enable ? HSED_EN_MASK : 0);
			if (rc < 0)
				pr_err("Enable HSED BIAS failed rc=%d\n", rc);
			break;
		default:
			/* Functionality not supported */
			break;
		}
	}

	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	return rc;
}
EXPORT_SYMBOL(pm8xxx_hsed_bias_control);

static int __devinit pm8xxx_misc_probe(struct platform_device *pdev)
{
	const struct pm8xxx_misc_platform_data *pdata = pdev->dev.platform_data;
	struct pm8xxx_misc_chip *chip;
	struct pm8xxx_misc_chip *sibling;
	struct list_head *prev;
	unsigned long flags;
	int rc = 0, irq;

	if (!pdata) {
		pr_err("missing platform data\n");
		return -EINVAL;
	}

	chip = kzalloc(sizeof(struct pm8xxx_misc_chip), GFP_KERNEL);
	if (!chip) {
		pr_err("Cannot allocate %d bytes\n",
			sizeof(struct pm8xxx_misc_chip));
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;
	chip->version = pm8xxx_get_version(chip->dev->parent);
	memcpy(&(chip->pdata), pdata, sizeof(struct pm8xxx_misc_platform_data));

	irq = platform_get_irq_byname(pdev, "pm8xxx_osc_halt_irq");
	if (irq > 0) {
		rc = request_any_context_irq(irq, pm8xxx_osc_halt_isr,
				 IRQF_TRIGGER_RISING | IRQF_DISABLED,
				 "pm8xxx_osc_halt_irq", chip);
		if (rc < 0) {
			pr_err("%s: request_any_context_irq(%d) FAIL: %d\n",
							 __func__, irq, rc);
			goto fail_irq;
		}
	}

	/* Insert PMICs in priority order (lowest value first). */
	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);
	prev = &pm8xxx_misc_chips;
	list_for_each_entry(sibling, &pm8xxx_misc_chips, link) {
		if (chip->pdata.priority < sibling->pdata.priority)
			break;
		else
			prev = &sibling->link;
	}
	list_add(&chip->link, prev);
	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	platform_set_drvdata(pdev, chip);

	return rc;

fail_irq:
	platform_set_drvdata(pdev, NULL);
	kfree(chip);
	return rc;
}

static int __devexit pm8xxx_misc_remove(struct platform_device *pdev)
{
	struct pm8xxx_misc_chip *chip = platform_get_drvdata(pdev);
	unsigned long flags;
	int irq = platform_get_irq_byname(pdev, "pm8xxx_osc_halt_irq");
	if (irq > 0)
		free_irq(irq, chip);

	spin_lock_irqsave(&pm8xxx_misc_chips_lock, flags);
	list_del(&chip->link);
	spin_unlock_irqrestore(&pm8xxx_misc_chips_lock, flags);

	platform_set_drvdata(pdev, NULL);
	kfree(chip);

	return 0;
}

static struct platform_driver pm8xxx_misc_driver = {
	.probe	= pm8xxx_misc_probe,
	.remove	= __devexit_p(pm8xxx_misc_remove),
	.driver	= {
		.name	= PM8XXX_MISC_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init pm8xxx_misc_init(void)
{
	return platform_driver_register(&pm8xxx_misc_driver);
}
postcore_initcall(pm8xxx_misc_init);

static void __exit pm8xxx_misc_exit(void)
{
	platform_driver_unregister(&pm8xxx_misc_driver);
}
module_exit(pm8xxx_misc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC 8XXX misc driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" PM8XXX_MISC_DEV_NAME);
