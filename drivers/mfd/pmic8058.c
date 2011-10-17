/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * Qualcomm PMIC8058 driver
 *
 */
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/ratelimit.h>
#include <linux/kthread.h>
#include <linux/msm_ssbi.h>
#include <linux/mfd/core.h>
#include <linux/mfd/pmic8058.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/irq.h>
#include <linux/syscore_ops.h>
#include <linux/gpio.h>

/* PMIC8058 Revision */
#define SSBI_REG_REV			0x002  /* PMIC4 revision */

/* PMIC8058 IRQ */
#define	SSBI_REG_ADDR_IRQ_BASE		0x1BB

#define	SSBI_REG_ADDR_IRQ_ROOT		(SSBI_REG_ADDR_IRQ_BASE + 0)
#define	SSBI_REG_ADDR_IRQ_M_STATUS1	(SSBI_REG_ADDR_IRQ_BASE + 1)
#define	SSBI_REG_ADDR_IRQ_M_STATUS2	(SSBI_REG_ADDR_IRQ_BASE + 2)
#define	SSBI_REG_ADDR_IRQ_M_STATUS3	(SSBI_REG_ADDR_IRQ_BASE + 3)
#define	SSBI_REG_ADDR_IRQ_M_STATUS4	(SSBI_REG_ADDR_IRQ_BASE + 4)
#define	SSBI_REG_ADDR_IRQ_BLK_SEL	(SSBI_REG_ADDR_IRQ_BASE + 5)
#define	SSBI_REG_ADDR_IRQ_IT_STATUS	(SSBI_REG_ADDR_IRQ_BASE + 6)
#define	SSBI_REG_ADDR_IRQ_CONFIG	(SSBI_REG_ADDR_IRQ_BASE + 7)
#define	SSBI_REG_ADDR_IRQ_RT_STATUS	(SSBI_REG_ADDR_IRQ_BASE + 8)

#define	PM8058_IRQF_LVL_SEL		0x01	/* level select */
#define	PM8058_IRQF_MASK_FE		0x02	/* mask falling edge */
#define	PM8058_IRQF_MASK_RE		0x04	/* mask rising edge */
#define	PM8058_IRQF_CLR			0x08	/* clear interrupt */
#define	PM8058_IRQF_BITS_MASK		0x70
#define	PM8058_IRQF_BITS_SHIFT		4
#define	PM8058_IRQF_WRITE		0x80

#define	PM8058_IRQF_MASK_ALL		(PM8058_IRQF_MASK_FE | \
					PM8058_IRQF_MASK_RE)
#define PM8058_IRQF_W_C_M		(PM8058_IRQF_WRITE |	\
					PM8058_IRQF_CLR |	\
					PM8058_IRQF_MASK_ALL)

/* MISC register */
#define	SSBI_REG_ADDR_MISC		0x1CC

/* PON CNTL 1 register */
#define SSBI_REG_ADDR_PON_CNTL_1	0x01C

#define PM8058_PON_PUP_MASK		0xF0

#define PM8058_PON_WD_EN_MASK		0x08
#define PM8058_PON_WD_EN_RESET		0x08
#define PM8058_PON_WD_EN_PWR_OFF	0x00

/* PON CNTL 4 register */
#define SSBI_REG_ADDR_PON_CNTL_4 0x98
#define PM8058_PON_RESET_EN_MASK 0x01

/* PON CNTL 5 register */
#define SSBI_REG_ADDR_PON_CNTL_5 0x7B
#define PM8058_HARD_RESET_EN_MASK 0x08

/* Regulator master enable addresses */
#define SSBI_REG_ADDR_VREG_EN_MSM	0x018
#define SSBI_REG_ADDR_VREG_EN_GRP_5_4	0x1C8

/* Regulator control registers for shutdown/reset */
#define SSBI_REG_ADDR_S0_CTRL		0x004
#define SSBI_REG_ADDR_S1_CTRL		0x005
#define SSBI_REG_ADDR_S3_CTRL		0x111
#define SSBI_REG_ADDR_L21_CTRL		0x120
#define SSBI_REG_ADDR_L22_CTRL		0x121

#define REGULATOR_ENABLE_MASK		0x80
#define REGULATOR_ENABLE		0x80
#define REGULATOR_DISABLE		0x00
#define REGULATOR_PULL_DOWN_MASK	0x40
#define REGULATOR_PULL_DOWN_EN		0x40
#define REGULATOR_PULL_DOWN_DIS		0x00

/* Buck CTRL register */
#define SMPS_LEGACY_VREF_SEL		0x20
#define SMPS_LEGACY_VPROG_MASK		0x1F
#define SMPS_ADVANCED_BAND_MASK		0xC0
#define SMPS_ADVANCED_BAND_SHIFT	6
#define SMPS_ADVANCED_VPROG_MASK	0x3F

/* Buck TEST2 registers for shutdown/reset */
#define SSBI_REG_ADDR_S0_TEST2		0x084
#define SSBI_REG_ADDR_S1_TEST2		0x085
#define SSBI_REG_ADDR_S3_TEST2		0x11A

#define REGULATOR_BANK_WRITE		0x80
#define REGULATOR_BANK_MASK		0x70
#define REGULATOR_BANK_SHIFT		4
#define REGULATOR_BANK_SEL(n)		((n) << REGULATOR_BANK_SHIFT)

/* Buck TEST2 register bank 1 */
#define SMPS_LEGACY_VLOW_SEL		0x01

/* Buck TEST2 register bank 7 */
#define SMPS_ADVANCED_MODE_MASK		0x02
#define SMPS_ADVANCED_MODE		0x02
#define SMPS_LEGACY_MODE		0x00

/* SLEEP CNTL register */
#define SSBI_REG_ADDR_SLEEP_CNTL	0x02B

#define PM8058_SLEEP_SMPL_EN_MASK	0x04
#define PM8058_SLEEP_SMPL_EN_RESET	0x04
#define PM8058_SLEEP_SMPL_EN_PWR_OFF	0x00

#define PM8058_SLEEP_SMPL_SEL_MASK	0x03
#define PM8058_SLEEP_SMPL_SEL_MIN	0
#define PM8058_SLEEP_SMPL_SEL_MAX	3

/* GP_TEST1 register */
#define SSBI_REG_ADDR_GP_TEST_1		0x07A

/* IRQ */
#define	MAX_PM_IRQ		256
#define	MAX_PM_BLOCKS		(MAX_PM_IRQ / 8 + 1)
#define	MAX_PM_MASTERS		(MAX_PM_BLOCKS / 8 + 1)

struct pm8058_chip {
	struct pm8058_platform_data	pdata;
	struct device		*dev;

	u8	irqs_allowed[MAX_PM_BLOCKS];
	u8	blocks_allowed[MAX_PM_MASTERS];
	u8	masters_allowed;
	int	pm_max_irq;
	int	pm_max_blocks;
	int	pm_max_masters;

	u8	config[MAX_PM_IRQ];
	u8	bus_unlock_config[MAX_PM_IRQ];
	u8	wake_enable[MAX_PM_IRQ];
	u16	count_wakeable;

	u8	revision;

	struct mutex	pm_lock;
};

#if defined(CONFIG_DEBUG_FS)
struct pm8058_dbg_device {
	struct mutex		dbg_mutex;
	struct pm8058_chip	*pm_chip;
	struct dentry		*dent;
	int			addr;
};

static struct pm8058_dbg_device *pmic_dbg_device;
#endif

static struct pm8058_chip *pmic_chip;

/* Helper Functions */
DEFINE_RATELIMIT_STATE(pm8058_msg_ratelimit, 60 * HZ, 10);

static inline int pm8058_can_print(void)
{
	return __ratelimit(&pm8058_msg_ratelimit);
}

static inline int
ssbi_read(struct device *dev, u16 addr, u8 *buf, size_t len)
{
	return msm_ssbi_read(dev->parent, addr, buf, len);
}

static inline int
ssbi_write(struct device *dev, u16 addr, u8 *buf, size_t len)
{
	return msm_ssbi_write(dev->parent, addr, buf, len);
}

static int pm8058_masked_write(u16 addr, u8 val, u8 mask)
{
	int rc;
	u8 reg;

	if (pmic_chip == NULL)
		return -ENODEV;

	rc = ssbi_read(pmic_chip->dev, addr, &reg, 1);
	if (rc) {
		pr_err("%s: ssbi_read(0x%03X) failed: rc=%d\n", __func__, addr,
			rc);
		goto done;
	}

	reg &= ~mask;
	reg |= val & mask;

	rc = ssbi_write(pmic_chip->dev, addr, &reg, 1);
	if (rc)
		pr_err("%s: ssbi_write(0x%03X)=0x%02X failed: rc=%d\n",
			__func__, addr, reg, rc);
done:
	return rc;
}

/* External APIs */
int pm8058_rev(struct pm8058_chip *chip)
{
	if (chip == NULL)
		return -EINVAL;

	return chip->revision;
}
EXPORT_SYMBOL(pm8058_rev);

int pm8058_irq_get_rt_status(struct pm8058_chip *chip, int irq)
{
	int     rc;
	u8      block, bits, bit;

	if (chip == NULL || irq < chip->pdata.irq_base ||
			irq >= chip->pdata.irq_base + MAX_PM_IRQ)
		return -EINVAL;

	irq -= chip->pdata.irq_base;

	block = irq / 8;
	bit = irq % 8;

	mutex_lock(&chip->pm_lock);

	rc = ssbi_write(chip->dev, SSBI_REG_ADDR_IRQ_BLK_SEL, &block, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_write(): rc=%d (Select Block)\n",
				__func__, rc);
		goto bail_out;
	}

	rc = ssbi_read(chip->dev, SSBI_REG_ADDR_IRQ_RT_STATUS, &bits, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_read(): rc=%d (Read RT Status)\n",
				__func__, rc);
		goto bail_out;
	}

	rc = (bits & (1 << bit)) ? 1 : 0;

bail_out:
	mutex_unlock(&chip->pm_lock);

	return rc;
}
EXPORT_SYMBOL(pm8058_irq_get_rt_status);

int pm8058_read(struct pm8058_chip *chip, u16 addr, u8 *values,
		unsigned int len)
{
	if (chip == NULL)
		return -EINVAL;

	return ssbi_read(chip->dev, addr, values, len);
}
EXPORT_SYMBOL(pm8058_read);

int pm8058_write(struct pm8058_chip *chip, u16 addr, u8 *values,
		 unsigned int len)
{
	if (chip == NULL)
		return -EINVAL;

	return ssbi_write(chip->dev, addr, values, len);
}
EXPORT_SYMBOL(pm8058_write);

int pm8058_misc_control(struct pm8058_chip *chip, int mask, int flag)
{
	int		rc;
	u8		misc;

	if (chip == NULL)
		chip = pmic_chip;	/* for calls from non child */
	if (chip == NULL)
		return -ENODEV;

	mutex_lock(&chip->pm_lock);

	rc = ssbi_read(chip->dev, SSBI_REG_ADDR_MISC, &misc, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_read(0x%x): rc=%d\n",
		       __func__, SSBI_REG_ADDR_MISC, rc);
		goto get_out;
	}

	misc &= ~mask;
	misc |= flag;

	rc = ssbi_write(chip->dev, SSBI_REG_ADDR_MISC, &misc, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_write(0x%x)=0x%x: rc=%d\n",
		       __func__, SSBI_REG_ADDR_MISC, misc, rc);
		goto get_out;
	}

get_out:
	mutex_unlock(&chip->pm_lock);

	return rc;
}
EXPORT_SYMBOL(pm8058_misc_control);

/**
 * pm8058_smpl_control - enables/disables SMPL detection
 * @enable: 0 = shutdown PMIC on power loss, 1 = reset PMIC on power loss
 *
 * This function enables or disables the Sudden Momentary Power Loss detection
 * module.  If SMPL detection is enabled, then when a sufficiently long power
 * loss event occurs, the PMIC will automatically reset itself.  If SMPL
 * detection is disabled, then the PMIC will shutdown when power loss occurs.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_smpl_control(int enable)
{
	return pm8058_masked_write(SSBI_REG_ADDR_SLEEP_CNTL,
				   (enable ? PM8058_SLEEP_SMPL_EN_RESET
					   : PM8058_SLEEP_SMPL_EN_PWR_OFF),
				   PM8058_SLEEP_SMPL_EN_MASK);
}
EXPORT_SYMBOL(pm8058_smpl_control);

/**
 * pm8058_smpl_set_delay - sets the SMPL detection time delay
 * @delay: enum value corresponding to delay time
 *
 * This function sets the time delay of the SMPL detection module.  If power
 * is reapplied within this interval, then the PMIC reset automatically.  The
 * SMPL detection module must be enabled for this delay time to take effect.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_smpl_set_delay(enum pm8058_smpl_delay delay)
{
	if (delay < PM8058_SLEEP_SMPL_SEL_MIN
	    || delay > PM8058_SLEEP_SMPL_SEL_MAX) {
		pr_err("%s: invalid delay specified: %d\n", __func__, delay);
		return -EINVAL;
	}

	return pm8058_masked_write(SSBI_REG_ADDR_SLEEP_CNTL, delay,
				   PM8058_SLEEP_SMPL_SEL_MASK);
}
EXPORT_SYMBOL(pm8058_smpl_set_delay);

/**
 * pm8058_watchdog_reset_control - enables/disables watchdog reset detection
 * @enable: 0 = shutdown when PS_HOLD goes low, 1 = reset when PS_HOLD goes low
 *
 * This function enables or disables the PMIC watchdog reset detection feature.
 * If watchdog reset detection is enabled, then the PMIC will reset itself
 * when PS_HOLD goes low.  If it is not enabled, then the PMIC will shutdown
 * when PS_HOLD goes low.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_watchdog_reset_control(int enable)
{
	return pm8058_masked_write(SSBI_REG_ADDR_PON_CNTL_1,
				   (enable ? PM8058_PON_WD_EN_RESET
					   : PM8058_PON_WD_EN_PWR_OFF),
				   PM8058_PON_WD_EN_MASK);
}
EXPORT_SYMBOL(pm8058_watchdog_reset_control);

/*
 * Set an SMPS regulator to be disabled in its CTRL register, but enabled
 * in the master enable register.  Also set it's pull down enable bit.
 * Take care to make sure that the output voltage doesn't change if switching
 * from advanced mode to legacy mode.
 */
static int disable_smps_locally_set_pull_down(u16 ctrl_addr, u16 test2_addr,
		u16 master_enable_addr, u8 master_enable_bit)
{
	int rc = 0;
	u8 vref_sel, vlow_sel, band, vprog, bank, reg;

	if (pmic_chip == NULL)
		return -ENODEV;

	bank = REGULATOR_BANK_SEL(7);
	rc = ssbi_write(pmic_chip->dev, test2_addr, &bank, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_write(0x%03X): rc=%d\n", __func__,
			test2_addr, rc);
		goto done;
	}

	rc = ssbi_read(pmic_chip->dev, test2_addr, &reg, 1);
	if (rc) {
		pr_err("%s: FAIL pm8058_read(0x%03X): rc=%d\n",
		       __func__, test2_addr, rc);
		goto done;
	}

	/* Check if in advanced mode. */
	if ((reg & SMPS_ADVANCED_MODE_MASK) == SMPS_ADVANCED_MODE) {
		/* Determine current output voltage. */
		rc = ssbi_read(pmic_chip->dev, ctrl_addr, &reg, 1);
		if (rc) {
			pr_err("%s: FAIL pm8058_read(0x%03X): rc=%d\n",
			       __func__, ctrl_addr, rc);
			goto done;
		}

		band = (reg & SMPS_ADVANCED_BAND_MASK)
			>> SMPS_ADVANCED_BAND_SHIFT;
		switch (band) {
		case 3:
			vref_sel = 0;
			vlow_sel = 0;
			break;
		case 2:
			vref_sel = SMPS_LEGACY_VREF_SEL;
			vlow_sel = 0;
			break;
		case 1:
			vref_sel = SMPS_LEGACY_VREF_SEL;
			vlow_sel = SMPS_LEGACY_VLOW_SEL;
			break;
		default:
			pr_err("%s: regulator already disabled\n", __func__);
			return -EPERM;
		}
		vprog = (reg & SMPS_ADVANCED_VPROG_MASK);
		/* Round up if fine step is in use. */
		vprog = (vprog + 1) >> 1;
		if (vprog > SMPS_LEGACY_VPROG_MASK)
			vprog = SMPS_LEGACY_VPROG_MASK;

		/* Set VLOW_SEL bit. */
		bank = REGULATOR_BANK_SEL(1);
		rc = ssbi_write(pmic_chip->dev, test2_addr, &bank, 1);
		if (rc) {
			pr_err("%s: FAIL ssbi_write(0x%03X): rc=%d\n",
			       __func__, test2_addr, rc);
			goto done;
		}
		rc = pm8058_masked_write(test2_addr,
			REGULATOR_BANK_WRITE | REGULATOR_BANK_SEL(1)
				| vlow_sel,
			REGULATOR_BANK_WRITE | REGULATOR_BANK_MASK
				| SMPS_LEGACY_VLOW_SEL);
		if (rc)
			goto done;

		/* Switch to legacy mode */
		bank = REGULATOR_BANK_SEL(7);
		rc = ssbi_write(pmic_chip->dev, test2_addr, &bank, 1);
		if (rc) {
			pr_err("%s: FAIL ssbi_write(0x%03X): rc=%d\n", __func__,
				test2_addr, rc);
			goto done;
		}
		rc = pm8058_masked_write(test2_addr,
				REGULATOR_BANK_WRITE | REGULATOR_BANK_SEL(7)
					| SMPS_LEGACY_MODE,
				REGULATOR_BANK_WRITE | REGULATOR_BANK_MASK
					| SMPS_ADVANCED_MODE_MASK);
		if (rc)
			goto done;

		/* Enable locally, enable pull down, keep voltage the same. */
		rc = pm8058_masked_write(ctrl_addr,
			REGULATOR_ENABLE | REGULATOR_PULL_DOWN_EN
				| vref_sel | vprog,
			REGULATOR_ENABLE_MASK | REGULATOR_PULL_DOWN_MASK
			       | SMPS_LEGACY_VREF_SEL | SMPS_LEGACY_VPROG_MASK);
		if (rc)
			goto done;
	}

	/* Enable in master control register. */
	rc = pm8058_masked_write(master_enable_addr, master_enable_bit,
				 master_enable_bit);
	if (rc)
		goto done;

	/* Disable locally and enable pull down. */
	rc = pm8058_masked_write(ctrl_addr,
		REGULATOR_DISABLE | REGULATOR_PULL_DOWN_EN,
		REGULATOR_ENABLE_MASK | REGULATOR_PULL_DOWN_MASK);

done:
	return rc;
}

static int disable_ldo_locally_set_pull_down(u16 ctrl_addr,
		u16 master_enable_addr, u8 master_enable_bit)
{
	int rc;

	/* Enable LDO in master control register. */
	rc = pm8058_masked_write(master_enable_addr, master_enable_bit,
				 master_enable_bit);
	if (rc)
		goto done;

	/* Disable LDO in CTRL register and set pull down */
	rc = pm8058_masked_write(ctrl_addr,
		REGULATOR_DISABLE | REGULATOR_PULL_DOWN_EN,
		REGULATOR_ENABLE_MASK | REGULATOR_PULL_DOWN_MASK);

done:
	return rc;
}

int pm8058_reset_pwr_off(int reset)
{
	int rc;
	u8 pon, ctrl, smpl;

	if (pmic_chip == NULL)
		return -ENODEV;

	/* When shutting down, enable active pulldowns on important rails. */
	if (!reset) {
		/* Disable SMPS's 0,1,3 locally and set pulldown enable bits. */
		disable_smps_locally_set_pull_down(SSBI_REG_ADDR_S0_CTRL,
		     SSBI_REG_ADDR_S0_TEST2, SSBI_REG_ADDR_VREG_EN_MSM, BIT(7));
		disable_smps_locally_set_pull_down(SSBI_REG_ADDR_S1_CTRL,
		     SSBI_REG_ADDR_S1_TEST2, SSBI_REG_ADDR_VREG_EN_MSM, BIT(6));
		disable_smps_locally_set_pull_down(SSBI_REG_ADDR_S3_CTRL,
		     SSBI_REG_ADDR_S3_TEST2, SSBI_REG_ADDR_VREG_EN_GRP_5_4,
		     BIT(7) | BIT(4));
		/* Disable LDO 21 locally and set pulldown enable bit. */
		disable_ldo_locally_set_pull_down(SSBI_REG_ADDR_L21_CTRL,
		     SSBI_REG_ADDR_VREG_EN_GRP_5_4, BIT(1));
	}

	/* Set regulator L22 to 1.225V in high power mode. */
	rc = ssbi_read(pmic_chip->dev, SSBI_REG_ADDR_L22_CTRL, &ctrl, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_read(0x%x): rc=%d\n", __func__,
			SSBI_REG_ADDR_L22_CTRL, rc);
		goto get_out3;
	}
	/* Leave pull-down state intact. */
	ctrl &= 0x40;
	ctrl |= 0x93;
	rc = ssbi_write(pmic_chip->dev, SSBI_REG_ADDR_L22_CTRL, &ctrl, 1);
	if (rc)
		pr_err("%s: FAIL ssbi_write(0x%x)=0x%x: rc=%d\n", __func__,
			SSBI_REG_ADDR_L22_CTRL, ctrl, rc);

get_out3:
	if (!reset) {
		/* Only modify the SLEEP_CNTL reg if shutdown is desired. */
		rc = ssbi_read(pmic_chip->dev, SSBI_REG_ADDR_SLEEP_CNTL,
			       &smpl, 1);
		if (rc) {
			pr_err("%s: FAIL ssbi_read(0x%x): rc=%d\n",
			       __func__, SSBI_REG_ADDR_SLEEP_CNTL, rc);
			goto get_out2;
		}

		smpl &= ~PM8058_SLEEP_SMPL_EN_MASK;
		smpl |= PM8058_SLEEP_SMPL_EN_PWR_OFF;

		rc = ssbi_write(pmic_chip->dev, SSBI_REG_ADDR_SLEEP_CNTL,
				&smpl, 1);
		if (rc)
			pr_err("%s: FAIL ssbi_write(0x%x)=0x%x: rc=%d\n",
			       __func__, SSBI_REG_ADDR_SLEEP_CNTL, smpl, rc);
	}

get_out2:
	rc = ssbi_read(pmic_chip->dev, SSBI_REG_ADDR_PON_CNTL_1, &pon, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_read(0x%x): rc=%d\n",
		       __func__, SSBI_REG_ADDR_PON_CNTL_1, rc);
		goto get_out;
	}

	pon &= ~PM8058_PON_WD_EN_MASK;
	pon |= reset ? PM8058_PON_WD_EN_RESET : PM8058_PON_WD_EN_PWR_OFF;

	/* Enable all pullups */
	pon |= PM8058_PON_PUP_MASK;

	rc = ssbi_write(pmic_chip->dev, SSBI_REG_ADDR_PON_CNTL_1, &pon, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_write(0x%x)=0x%x: rc=%d\n",
		       __func__, SSBI_REG_ADDR_PON_CNTL_1, pon, rc);
		goto get_out;
	}

get_out:
	return rc;
}
EXPORT_SYMBOL(pm8058_reset_pwr_off);

/**
 * pm8058_stay_on - enables stay_on feature
 *
 * PMIC stay-on feature allows PMIC to ignore MSM PS_HOLD=low
 * signal so that some special functions like debugging could be
 * performed.
 *
 * This feature should not be used in any product release.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_stay_on(void)
{
	u8	ctrl = 0x92;
	int	rc;

	rc = ssbi_write(pmic_chip->dev, SSBI_REG_ADDR_GP_TEST_1, &ctrl, 1);
	pr_info("%s: set stay-on: rc = %d\n", __func__, rc);
	return rc;
}
EXPORT_SYMBOL(pm8058_stay_on);

/*
   power on hard reset configuration
   config = DISABLE_HARD_RESET to disable hard reset
	  = SHUTDOWN_ON_HARD_RESET to turn off the system on hard reset
	  = RESTART_ON_HARD_RESET to restart the system on hard reset
 */
int pm8058_hard_reset_config(enum pon_config config)
{
	int rc, ret;
	u8 pon, pon_5;

	if (config >= MAX_PON_CONFIG)
		return -EINVAL;

	if (pmic_chip == NULL)
		return -ENODEV;

	mutex_lock(&pmic_chip->pm_lock);

	rc = ssbi_read(pmic_chip->dev, SSBI_REG_ADDR_PON_CNTL_5, &pon, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_read(0x%x): rc=%d\n",
		       __func__, SSBI_REG_ADDR_PON_CNTL_5, rc);
		mutex_unlock(&pmic_chip->pm_lock);
		return rc;
	}

	pon_5 = pon;
	(config != DISABLE_HARD_RESET) ? (pon |= PM8058_HARD_RESET_EN_MASK) :
					(pon &= ~PM8058_HARD_RESET_EN_MASK);

	rc = ssbi_write(pmic_chip->dev, SSBI_REG_ADDR_PON_CNTL_5, &pon, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_write(0x%x)=0x%x: rc=%d\n",
		       __func__, SSBI_REG_ADDR_PON_CNTL_5, pon, rc);
		mutex_unlock(&pmic_chip->pm_lock);
		return rc;
	}

	if (config == DISABLE_HARD_RESET) {
		mutex_unlock(&pmic_chip->pm_lock);
		return 0;
	}

	rc = ssbi_read(pmic_chip->dev, SSBI_REG_ADDR_PON_CNTL_4, &pon, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_read(0x%x): rc=%d\n",
		       __func__, SSBI_REG_ADDR_PON_CNTL_4, rc);
		goto err_restore_pon_5;
	}

	(config == RESTART_ON_HARD_RESET) ? (pon |= PM8058_PON_RESET_EN_MASK) :
					(pon &= ~PM8058_PON_RESET_EN_MASK);

	rc = ssbi_write(pmic_chip->dev, SSBI_REG_ADDR_PON_CNTL_4, &pon, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_write(0x%x)=0x%x: rc=%d\n",
		       __func__, SSBI_REG_ADDR_PON_CNTL_4, pon, rc);
		goto err_restore_pon_5;
	}
	mutex_unlock(&pmic_chip->pm_lock);
	return 0;

err_restore_pon_5:
	ret = ssbi_write(pmic_chip->dev, SSBI_REG_ADDR_PON_CNTL_5, &pon_5, 1);
	if (ret)
		pr_err("%s: FAIL ssbi_write(0x%x)=0x%x: rc=%d\n",
		       __func__, SSBI_REG_ADDR_PON_CNTL_5, pon, ret);
	mutex_unlock(&pmic_chip->pm_lock);
	return rc;
}
EXPORT_SYMBOL(pm8058_hard_reset_config);

/* Internal functions */
static inline int
pm8058_config_irq(struct pm8058_chip *chip, u8 *bp, u8 *cp)
{
	int	rc;

	rc = ssbi_write(chip->dev, SSBI_REG_ADDR_IRQ_BLK_SEL, bp, 1);
	if (rc) {
		pr_err("%s: ssbi_write: rc=%d (Select block)\n",
			__func__, rc);
		goto bail_out;
	}

	rc = ssbi_write(chip->dev, SSBI_REG_ADDR_IRQ_CONFIG, cp, 1);
	if (rc)
		pr_err("%s: ssbi_write: rc=%d (Configure IRQ)\n",
			__func__, rc);

bail_out:
	return rc;
}

static void pm8058_irq_mask(struct irq_data *data)
{
	int	master, irq_bit;
	struct	pm8058_chip *chip = irq_data_get_irq_chip_data(data);
	u8	block, config;
	unsigned int irq = data->irq;

	irq -= chip->pdata.irq_base;
	block = irq / 8;
	master = block / 8;
	irq_bit = irq % 8;

	chip->irqs_allowed[block] &= ~(1 << irq_bit);
	if (!chip->irqs_allowed[block]) {
		chip->blocks_allowed[master] &= ~(1 << (block % 8));

		if (!chip->blocks_allowed[master])
			chip->masters_allowed &= ~(1 << master);
	}

	config = PM8058_IRQF_WRITE | chip->config[irq] |
		PM8058_IRQF_MASK_FE | PM8058_IRQF_MASK_RE;
	chip->bus_unlock_config[irq] = config;
}

static void pm8058_irq_unmask(struct irq_data *data)
{
	int	master, irq_bit;
	struct	pm8058_chip *chip = irq_data_get_irq_chip_data(data);
	u8	block, config, old_irqs_allowed, old_blocks_allowed;
	unsigned int irq = data->irq;

	irq -= chip->pdata.irq_base;
	block = irq / 8;
	master = block / 8;
	irq_bit = irq % 8;

	old_irqs_allowed = chip->irqs_allowed[block];
	if (old_irqs_allowed & (1 << irq_bit)) {
		pr_debug("%s: no need to enable an already enabled irq=%d\n",
					__func__, irq + chip->pdata.irq_base);
		return;
	}

	chip->irqs_allowed[block] |= 1 << irq_bit;
	if (!old_irqs_allowed) {
		master = block / 8;

		old_blocks_allowed = chip->blocks_allowed[master];
		chip->blocks_allowed[master] |= 1 << (block % 8);

		if (!old_blocks_allowed)
			chip->masters_allowed |= 1 << master;
	}

	config = PM8058_IRQF_WRITE | chip->config[irq];
	chip->bus_unlock_config[irq] = config;
}

static void pm8058_irq_ack(struct irq_data *data)
{
	struct	pm8058_chip *chip = irq_data_get_irq_chip_data(data);
	u8	block, config;
	unsigned int irq = data->irq;

	irq -= chip->pdata.irq_base;
	block = irq / 8;

	config = PM8058_IRQF_WRITE | chip->config[irq] | PM8058_IRQF_CLR;
	/* Keep the mask */
	if (!(chip->irqs_allowed[block] & (1 << (irq % 8))))
		config |= PM8058_IRQF_MASK_FE | PM8058_IRQF_MASK_RE;
	chip->bus_unlock_config[irq] = config;
}

static int pm8058_irq_set_type(struct irq_data *data, unsigned int flow_type)
{
	int	master, irq_bit;
	struct	pm8058_chip *chip = irq_data_get_irq_chip_data(data);
	u8	block, config;
	unsigned int irq = data->irq;

	irq -= chip->pdata.irq_base;
	if (irq > chip->pm_max_irq) {
		chip->pm_max_irq = irq;
		chip->pm_max_blocks =
			chip->pm_max_irq / 8 + 1;
		chip->pm_max_masters =
			chip->pm_max_blocks / 8 + 1;
	}
	block = irq / 8;
	master = block / 8;
	irq_bit = irq % 8;

	chip->config[irq] = (irq_bit << PM8058_IRQF_BITS_SHIFT) |
			PM8058_IRQF_MASK_RE | PM8058_IRQF_MASK_FE;
	if (flow_type & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) {
		if (flow_type & IRQF_TRIGGER_RISING)
			chip->config[irq] &= ~PM8058_IRQF_MASK_RE;
		if (flow_type & IRQF_TRIGGER_FALLING)
			chip->config[irq] &= ~PM8058_IRQF_MASK_FE;
	} else {
		chip->config[irq] |= PM8058_IRQF_LVL_SEL;

		if (flow_type & IRQF_TRIGGER_HIGH)
			chip->config[irq] &= ~PM8058_IRQF_MASK_RE;
		else
			chip->config[irq] &= ~PM8058_IRQF_MASK_FE;
	}

	config = PM8058_IRQF_WRITE | chip->config[irq] | PM8058_IRQF_CLR;
	chip->bus_unlock_config[irq] = config;
	return 0;
}

static int pm8058_irq_set_wake(struct irq_data *data, unsigned int on)
{
	struct	pm8058_chip *chip = irq_data_get_irq_chip_data(data);
	unsigned int irq = data->irq;

	irq -= chip->pdata.irq_base;
	if (on) {
		if (!chip->wake_enable[irq]) {
			chip->wake_enable[irq] = 1;
			chip->count_wakeable++;
		}
	} else {
		if (chip->wake_enable[irq]) {
			chip->wake_enable[irq] = 0;
			chip->count_wakeable--;
		}
	}

	return 0;
}

static void pm8058_irq_bus_lock(struct irq_data *data)
{
	u8	block;
	struct	pm8058_chip *chip = irq_data_get_irq_chip_data(data);
	unsigned int irq = data->irq;

	irq -= chip->pdata.irq_base;
	block = irq / 8;
	chip->bus_unlock_config[irq] = 0;

	mutex_lock(&chip->pm_lock);
}

static void pm8058_irq_bus_sync_unlock(struct irq_data *data)
{
	u8	block, config;
	struct	pm8058_chip *chip = irq_data_get_irq_chip_data(data);
	unsigned int irq = data->irq;

	irq -= chip->pdata.irq_base;
	block = irq / 8;
	config = chip->bus_unlock_config[irq];
	/* dont waste cpu cycles if we dont have data to write */
	if (config)
		pm8058_config_irq(chip, &block, &config);
	mutex_unlock(&chip->pm_lock);
}

static inline int
pm8058_read_root(struct pm8058_chip *chip, u8 *rp)
{
	int	rc;

	rc = ssbi_read(chip->dev, SSBI_REG_ADDR_IRQ_ROOT, rp, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_read(): rc=%d (Read Root)\n",
			__func__, rc);
		*rp = 0;
	}

	return rc;
}

static inline int
pm8058_read_master(struct pm8058_chip *chip, u8 m, u8 *bp)
{
	int	rc;

	rc = ssbi_read(chip->dev, SSBI_REG_ADDR_IRQ_M_STATUS1 + m, bp, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_read(): rc=%d (Read Master)\n",
			__func__, rc);
		*bp = 0;
	}

	return rc;
}

static inline int
pm8058_read_block(struct pm8058_chip *chip, u8 *bp, u8 *ip)
{
	int	rc;

	rc = ssbi_write(chip->dev, SSBI_REG_ADDR_IRQ_BLK_SEL, bp, 1);
	if (rc) {
		pr_err("%s: FAIL ssbi_write(): rc=%d (Select Block)\n",
		       __func__, rc);
		*bp = 0;
		goto bail_out;
	}

	rc = ssbi_read(chip->dev, SSBI_REG_ADDR_IRQ_IT_STATUS, ip, 1);
	if (rc)
		pr_err("%s: FAIL ssbi_read(): rc=%d (Read Status)\n",
		       __func__, rc);

bail_out:
	return rc;
}

static irqreturn_t pm8058_isr_thread(int irq_requested, void *data)
{
	struct pm8058_chip *chip = data;
	int	i, j, k;
	u8	root, block, config, bits;
	u8	blocks[MAX_PM_MASTERS];
	int	masters = 0, irq, handled = 0, spurious = 0;
	u16     irqs_to_handle[MAX_PM_IRQ];

	mutex_lock(&chip->pm_lock);

	/* Read root for masters */
	if (pm8058_read_root(chip, &root))
		goto bail_out;

	masters = root >> 1;

	if (!(masters & chip->masters_allowed) ||
	    (masters & ~chip->masters_allowed)) {
		spurious = 1000000;
	}

	/* Read allowed masters for blocks. */
	for (i = 0; i < chip->pm_max_masters; i++) {
		if (masters & (1 << i)) {
			if (pm8058_read_master(chip, i, &blocks[i]))
				goto bail_out;

			if (!blocks[i]) {
				if (pm8058_can_print())
					pr_err("%s: Spurious master: %d "
					       "(blocks=0)", __func__, i);
				spurious += 10000;
			}
		} else
			blocks[i] = 0;
	}

	/* Select block, read status and call isr */
	for (i = 0; i < chip->pm_max_masters; i++) {
		if (!blocks[i])
			continue;

		for (j = 0; j < 8; j++) {
			if (!(blocks[i] & (1 << j)))
				continue;

			block = i * 8 + j;	/* block # */
			if (pm8058_read_block(chip, &block, &bits))
				goto bail_out;

			if (!bits) {
				if (pm8058_can_print())
					pr_err("%s: Spurious block: "
					       "[master, block]=[%d, %d] "
					       "(bits=0)\n", __func__, i, j);
				spurious += 100;
				continue;
			}

			/* Check IRQ bits */
			for (k = 0; k < 8; k++) {
				if (!(bits & (1 << k)))
					continue;

				/* Check spurious interrupts */
				if (((1 << i) & chip->masters_allowed) &&
				    (blocks[i] & chip->blocks_allowed[i]) &&
				    (bits & chip->irqs_allowed[block])) {

					/* Found one */
					irq = block * 8 + k;
					irqs_to_handle[handled] = irq +
						chip->pdata.irq_base;
					handled++;
				} else {
					/* Clear and mask wrong one */
					config = PM8058_IRQF_W_C_M |
						(k << PM8058_IRQF_BITS_SHIFT);

					pm8058_config_irq(chip,
							  &block, &config);

					if (pm8058_can_print())
						pr_err("%s: Spurious IRQ: "
						       "[master, block, bit]="
						       "[%d, %d (%d), %d]\n",
							__func__,
						       i, j, block, k);
					spurious++;
				}
			}
		}

	}

bail_out:

	mutex_unlock(&chip->pm_lock);

	for (i = 0; i < handled; i++) {
		int pmic_irq = irqs_to_handle[i] - chip->pdata.irq_base;

		/* ack the interrupt first */
		block  = pmic_irq / 8 ;
		config = PM8058_IRQF_WRITE | chip->config[pmic_irq]
				| PM8058_IRQF_CLR;
		pm8058_config_irq(chip, &block, &config);

		/* calle the action handler */
		handle_nested_irq(irqs_to_handle[i]);
	}

	if (spurious) {
		if (!pm8058_can_print())
			return IRQ_HANDLED;

		pr_err("%s: spurious = %d (handled = %d)\n",
		       __func__, spurious, handled);
		pr_err("   root = 0x%x (masters_allowed<<1 = 0x%x)\n",
		       root, chip->masters_allowed << 1);
		for (i = 0; i < chip->pm_max_masters; i++) {
			if (masters & (1 << i))
				pr_err("   blocks[%d]=0x%x, "
				       "allowed[%d]=0x%x\n",
				       i, blocks[i],
				       i, chip->blocks_allowed[i]);
		}
	}

	return IRQ_HANDLED;
}

#if defined(CONFIG_DEBUG_FS)

static int check_addr(int addr, const char *func_name)
{
	if (addr < 0 || addr > 0x3FF) {
		pr_err("%s: PMIC 8058 register address is invalid: %d\n",
			func_name, addr);
		return -EINVAL;
	}
	return 0;
}

static int data_set(void *data, u64 val)
{
	struct pm8058_dbg_device *dbgdev = data;
	u8 reg = val;
	int rc;

	mutex_lock(&dbgdev->dbg_mutex);

	rc = check_addr(dbgdev->addr, __func__);
	if (rc)
		goto done;

	rc = pm8058_write(dbgdev->pm_chip, dbgdev->addr, &reg, 1);

	if (rc)
		pr_err("%s: FAIL pm8058_write(0x%03X)=0x%02X: rc=%d\n",
			__func__, dbgdev->addr, reg, rc);
done:
	mutex_unlock(&dbgdev->dbg_mutex);
	return rc;
}

static int data_get(void *data, u64 *val)
{
	struct pm8058_dbg_device *dbgdev = data;
	int rc;
	u8 reg;

	mutex_lock(&dbgdev->dbg_mutex);

	rc = check_addr(dbgdev->addr, __func__);
	if (rc)
		goto done;

	rc = pm8058_read(dbgdev->pm_chip, dbgdev->addr, &reg, 1);

	if (rc) {
		pr_err("%s: FAIL pm8058_read(0x%03X)=0x%02X: rc=%d\n",
			__func__, dbgdev->addr, reg, rc);
		goto done;
	}

	*val = reg;
done:
	mutex_unlock(&dbgdev->dbg_mutex);
	return rc;
}

DEFINE_SIMPLE_ATTRIBUTE(dbg_data_fops, data_get, data_set, "0x%02llX\n");

static int addr_set(void *data, u64 val)
{
	struct pm8058_dbg_device *dbgdev = data;
	int rc;

	rc = check_addr(val, __func__);
	if (rc)
		return rc;

	mutex_lock(&dbgdev->dbg_mutex);
	dbgdev->addr = val;
	mutex_unlock(&dbgdev->dbg_mutex);

	return 0;
}

static int addr_get(void *data, u64 *val)
{
	struct pm8058_dbg_device *dbgdev = data;
	int rc;

	mutex_lock(&dbgdev->dbg_mutex);

	rc = check_addr(dbgdev->addr, __func__);
	if (rc) {
		mutex_unlock(&dbgdev->dbg_mutex);
		return rc;
	}
	*val = dbgdev->addr;

	mutex_unlock(&dbgdev->dbg_mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(dbg_addr_fops, addr_get, addr_set, "0x%03llX\n");

static int __devinit pmic8058_dbg_probe(struct pm8058_chip *chip)
{
	struct pm8058_dbg_device *dbgdev;
	struct dentry *dent;
	struct dentry *temp;
	int rc;

	if (chip == NULL) {
		pr_err("%s: no parent data passed in.\n", __func__);
		return -EINVAL;
	}

	dbgdev = kzalloc(sizeof *dbgdev, GFP_KERNEL);
	if (dbgdev == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	dbgdev->pm_chip = chip;
	dbgdev->addr = -1;

	dent = debugfs_create_dir("pm8058-dbg", NULL);
	if (dent == NULL || IS_ERR(dent)) {
		pr_err("%s: ERR debugfs_create_dir: dent=0x%X\n",
					__func__, (unsigned)dent);
		rc = PTR_ERR(dent);
		goto dir_error;
	}

	temp = debugfs_create_file("addr", S_IRUSR | S_IWUSR, dent,
					dbgdev, &dbg_addr_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("%s: ERR debugfs_create_file: dent=0x%X\n",
					__func__, (unsigned)temp);
		rc = PTR_ERR(temp);
		goto debug_error;
	}

	temp = debugfs_create_file("data", S_IRUSR | S_IWUSR, dent,
					dbgdev, &dbg_data_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("%s: ERR debugfs_create_file: dent=0x%X\n",
					__func__, (unsigned)temp);
		rc = PTR_ERR(temp);
		goto debug_error;
	}

	mutex_init(&dbgdev->dbg_mutex);

	dbgdev->dent = dent;

	pmic_dbg_device = dbgdev;

	return 0;

debug_error:
	debugfs_remove_recursive(dent);
dir_error:
	kfree(dbgdev);

	return rc;
}

static int __devexit pmic8058_dbg_remove(void)
{
	if (pmic_dbg_device) {
		debugfs_remove_recursive(pmic_dbg_device->dent);
		mutex_destroy(&pmic_dbg_device->dbg_mutex);
		kfree(pmic_dbg_device);
	}
	return 0;
}

#else

static int __devinit pmic8058_dbg_probe(struct pm8058_chip *chip)
{
	return 0;
}

static int __devexit pmic8058_dbg_remove(void)
{
	return 0;
}

#endif

static struct irq_chip pm8058_irq_chip = {
	.name      = "pm8058",
	.irq_ack	= pm8058_irq_ack,
	.irq_mask	= pm8058_irq_mask,
	.irq_unmask	= pm8058_irq_unmask,
	.irq_set_type	= pm8058_irq_set_type,
	.irq_set_wake	= pm8058_irq_set_wake,
	.irq_bus_lock	= pm8058_irq_bus_lock,
	.irq_bus_sync_unlock	= pm8058_irq_bus_sync_unlock,
};

static int pm8058_suspend(void)
{
	struct	pm8058_chip *chip = pmic_chip;
	struct irq_data *data;
	int	i;

	for (i = 0; i < MAX_PM_IRQ; i++) {
		if (chip->config[i] && !chip->wake_enable[i]) {
			if (!((chip->config[i] & PM8058_IRQF_MASK_ALL)
			      == PM8058_IRQF_MASK_ALL)) {
				data = irq_get_irq_data(i +
						chip->pdata.irq_base);
				pm8058_irq_bus_lock(data);
				pm8058_irq_mask(data);
				pm8058_irq_bus_sync_unlock(data);
			}
		}
	}

	if (!chip->count_wakeable)
		disable_irq(chip->pdata.irq);

	return 0;
}

extern int msm_show_resume_irq_mask;

static void pm8058_show_resume_irq(void)
{
	u8	block, bits;
	int i;
	struct pm8058_chip *chip = pmic_chip;

	if (!msm_show_resume_irq_mask)
		return;

	for (i = 0; i < MAX_PM_IRQ; i++) {
		if (chip->wake_enable[i]) {
			block = i / 8;
			if (!pm8058_read_block(chip, &block, &bits)) {
				if (bits & (1 << (i & 0x7)))
					pr_warning("%s:%d triggered\n",
					__func__, i + chip->pdata.irq_base);
			}
		}
	}
}

static void pm8058_resume(void)
{
	struct	pm8058_chip *chip = pmic_chip;
	struct irq_data *data;
	int	i;

	pm8058_show_resume_irq();

	for (i = 0; i < MAX_PM_IRQ; i++) {
		if (chip->config[i] && !chip->wake_enable[i]) {
			if (!((chip->config[i] & PM8058_IRQF_MASK_ALL)
			      == PM8058_IRQF_MASK_ALL)) {
				data = irq_get_irq_data(i +
						chip->pdata.irq_base);
				pm8058_irq_bus_lock(data);
				pm8058_irq_unmask(data);
				pm8058_irq_bus_sync_unlock(data);
			}
		}
	}

	if (!chip->count_wakeable)
		enable_irq(chip->pdata.irq);
}

static struct syscore_ops pm8058_pm = {
	.suspend = pm8058_suspend,
	.resume = pm8058_resume,
};

static int __devinit pm8058_probe(struct platform_device *pdev)
{
	int	i, rc;
	struct	pm8058_platform_data *pdata = pdev->dev.platform_data;
	struct	pm8058_chip *chip;

	if (pdata == NULL || !gpio_is_valid(pdata->irq)) {
		pr_err("%s: No platform_data or IRQ.\n", __func__);
		return -ENODEV;
	}

	if (pdata->num_subdevs == 0) {
		pr_err("%s: No sub devices to support.\n", __func__);
		return -ENODEV;
	}

	chip = kzalloc(sizeof *chip, GFP_KERNEL);
	if (chip == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	chip->dev = &pdev->dev;

	/* Read PMIC chip revision */
	rc = ssbi_read(chip->dev, SSBI_REG_REV, &chip->revision, 1);
	if (rc)
		pr_err("%s: Failed on ssbi_read for revision: rc=%d.\n",
			__func__, rc);
	pr_info("%s: PMIC revision: %X\n", __func__, chip->revision);

	(void) memcpy((void *)&chip->pdata, (const void *)pdata,
		      sizeof(chip->pdata));

	mutex_init(&chip->pm_lock);
	irq_set_handler_data(pdata->irq, (void *)chip);
	irq_set_irq_wake(pdata->irq, 1);

	chip->pm_max_irq = 0;
	chip->pm_max_blocks = 0;
	chip->pm_max_masters = 0;

	platform_set_drvdata(pdev, chip);

	pmic_chip = chip;

	/* Register for all reserved IRQs */
	for (i = pdata->irq_base; i < (pdata->irq_base + MAX_PM_IRQ); i++) {
		irq_set_chip(i, &pm8058_irq_chip);
		irq_set_chip_data(i, (void *)chip);
		irq_set_handler(i, handle_edge_irq);
		set_irq_flags(i, IRQF_VALID);
		irq_set_nested_thread(i, 1);
	}

	rc = mfd_add_devices(chip->dev, 0, pdata->sub_devices,
			     pdata->num_subdevs, NULL, 0);

	/* Add charger sub device with the chip parameter as driver data */
	if (pdata->charger_sub_device) {
		rc = mfd_add_devices(chip->dev, 0,
					pdata->charger_sub_device,
					1, NULL, 0);
	}

	if (pdata->init) {
		rc = pdata->init(chip);
		if (rc != 0) {
			pr_err("%s: board init failed\n", __func__);
			chip->dev = NULL;
			kfree(chip);
			return -ENODEV;
		}
	}

	rc = request_threaded_irq(pdata->irq, NULL, pm8058_isr_thread,
			IRQF_ONESHOT | IRQF_DISABLED | pdata->irq_trigger_flags,
			"pm8058-irq", chip);
	if (rc < 0)
		pr_err("%s: could not request irq %d: %d\n", __func__,
				pdata->irq, rc);

	rc = pmic8058_dbg_probe(chip);
	if (rc < 0)
		pr_err("%s: could not set up debugfs: %d\n", __func__, rc);

	rc = pm8058_hard_reset_config(SHUTDOWN_ON_HARD_RESET);
	if (rc < 0)
		pr_err("%s: failed to config shutdown on hard reset: %d\n",
								__func__, rc);

	register_syscore_ops(&pm8058_pm);

	return 0;
}

static int __devexit pm8058_remove(struct platform_device *pdev)
{
	struct	pm8058_chip *chip;

	chip = platform_get_drvdata(pdev);
	if (chip) {
		if (chip->pm_max_irq) {
			irq_set_irq_wake(chip->pdata.irq, 0);
			free_irq(chip->pdata.irq, chip);
		}
		mutex_destroy(&chip->pm_lock);
		chip->dev = NULL;

		kfree(chip);
	}

	pmic8058_dbg_remove();

	return 0;
}

static struct platform_driver pm8058_driver = {
	.probe		= pm8058_probe,
	.remove		= __devexit_p(pm8058_remove),
	.driver		= {
		.name	= "pm8058-core",
		.owner	= THIS_MODULE,
	},
};

static int __init pm8058_init(void)
{
	return platform_driver_register(&pm8058_driver);
}
arch_initcall(pm8058_init);

static void __exit pm8058_exit(void)
{
	platform_driver_unregister(&pm8058_driver);
}
module_exit(pm8058_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PMIC8058 core driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:pmic8058-core");
