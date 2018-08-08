/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/spmi.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/input.h>
#include <linux/log2.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/qpnp/power-on.h>
#include <linux/qpnp/qpnp-pbs.h>
#include <linux/qpnp-misc.h>

#define CREATE_MASK(NUM_BITS, POS) \
	((unsigned char) (((1 << (NUM_BITS)) - 1) << (POS)))
#define PON_MASK(MSB_BIT, LSB_BIT) \
	CREATE_MASK(MSB_BIT - LSB_BIT + 1, LSB_BIT)

#define PMIC_VER_8941           0x01
#define PMIC_VERSION_REG        0x0105
#define PMIC_VERSION_REV4_REG   0x0103

#define PMIC8941_V1_REV4	0x01
#define PMIC8941_V2_REV4	0x02
#define PON_PRIMARY		0x01
#define PON_SECONDARY		0x02
#define PON_1REG		0x03
#define PON_GEN2_PRIMARY	0x04
#define PON_GEN2_SECONDARY	0x05

#define PON_OFFSET(subtype, offset_gen1, offset_gen2) \
	(((subtype == PON_PRIMARY) || \
	(subtype == PON_SECONDARY) || \
	(subtype == PON_1REG)) ? offset_gen1 : offset_gen2)

/* Common PNP defines */
#define QPNP_PON_REVISION2(pon)			((pon)->base + 0x01)
#define QPNP_PON_PERPH_SUBTYPE(pon)		((pon)->base + 0x05)

/* PON common register addresses */
#define QPNP_PON_RT_STS(pon)			((pon)->base + 0x10)
#define QPNP_PON_PULL_CTL(pon)			((pon)->base + 0x70)
#define QPNP_PON_DBC_CTL(pon)			((pon)->base + 0x71)

/* PON/RESET sources register addresses */
#define QPNP_PON_REASON1(pon) \
	((pon)->base + PON_OFFSET((pon)->subtype, 0x8, 0xC0))
#define QPNP_PON_WARM_RESET_REASON1(pon) \
	((pon)->base + PON_OFFSET((pon)->subtype, 0xA, 0xC2))
#define QPNP_POFF_REASON1(pon) \
	((pon)->base + PON_OFFSET((pon)->subtype, 0xC, 0xC5))
#define QPNP_PON_WARM_RESET_REASON2(pon)	((pon)->base + 0xB)
#define QPNP_PON_OFF_REASON(pon)		((pon)->base + 0xC7)
#define QPNP_FAULT_REASON1(pon)			((pon)->base + 0xC8)
#define QPNP_S3_RESET_REASON(pon)		((pon)->base + 0xCA)
#define QPNP_PON_KPDPWR_S1_TIMER(pon)		((pon)->base + 0x40)
#define QPNP_PON_KPDPWR_S2_TIMER(pon)		((pon)->base + 0x41)
#define QPNP_PON_KPDPWR_S2_CNTL(pon)		((pon)->base + 0x42)
#define QPNP_PON_KPDPWR_S2_CNTL2(pon)		((pon)->base + 0x43)
#define QPNP_PON_RESIN_S1_TIMER(pon)		((pon)->base + 0x44)
#define QPNP_PON_RESIN_S2_TIMER(pon)		((pon)->base + 0x45)
#define QPNP_PON_RESIN_S2_CNTL(pon)		((pon)->base + 0x46)
#define QPNP_PON_RESIN_S2_CNTL2(pon)		((pon)->base + 0x47)
#define QPNP_PON_KPDPWR_RESIN_S1_TIMER(pon)	((pon)->base + 0x48)
#define QPNP_PON_KPDPWR_RESIN_S2_TIMER(pon)	((pon)->base + 0x49)
#define QPNP_PON_KPDPWR_RESIN_S2_CNTL(pon)	((pon)->base + 0x4A)
#define QPNP_PON_KPDPWR_RESIN_S2_CNTL2(pon)	((pon)->base + 0x4B)
#define QPNP_PON_PS_HOLD_RST_CTL(pon)		((pon)->base + 0x5A)
#define QPNP_PON_PS_HOLD_RST_CTL2(pon)		((pon)->base + 0x5B)
#define QPNP_PON_WD_RST_S2_CTL(pon)		((pon)->base + 0x56)
#define QPNP_PON_WD_RST_S2_CTL2(pon)		((pon)->base + 0x57)
#define QPNP_PON_S3_SRC(pon)			((pon)->base + 0x74)
#define QPNP_PON_S3_DBC_CTL(pon)		((pon)->base + 0x75)
#define QPNP_PON_SMPL_CTL(pon)			((pon)->base + 0x7F)
#define QPNP_PON_TRIGGER_EN(pon)		((pon)->base + 0x80)
#define QPNP_PON_XVDD_RB_SPARE(pon)		((pon)->base + 0x8E)
#define QPNP_PON_SOFT_RB_SPARE(pon)		((pon)->base + 0x8F)
#define QPNP_PON_SEC_ACCESS(pon)		((pon)->base + 0xD0)

#define QPNP_PON_SEC_UNLOCK			0xA5

#define QPNP_PON_WARM_RESET_TFT			BIT(4)

#define QPNP_PON_RESIN_PULL_UP			BIT(0)
#define QPNP_PON_KPDPWR_PULL_UP			BIT(1)
#define QPNP_PON_CBLPWR_PULL_UP			BIT(2)
#define QPNP_PON_FAULT_PULL_UP			BIT(4)
#define QPNP_PON_S2_CNTL_EN			BIT(7)
#define QPNP_PON_S2_RESET_ENABLE		BIT(7)
#define QPNP_PON_DELAY_BIT_SHIFT		6
#define QPNP_PON_GEN2_DELAY_BIT_SHIFT		14

#define QPNP_PON_S1_TIMER_MASK			(0xF)
#define QPNP_PON_S2_TIMER_MASK			(0x7)
#define QPNP_PON_S2_CNTL_TYPE_MASK		(0xF)

#define QPNP_PON_DBC_DELAY_MASK(pon) \
		PON_OFFSET((pon)->subtype, 0x7, 0xF)

#define QPNP_PON_KPDPWR_N_SET			BIT(0)
#define QPNP_PON_RESIN_N_SET			BIT(1)
#define QPNP_PON_CBLPWR_N_SET			BIT(2)
#define QPNP_PON_RESIN_BARK_N_SET		BIT(4)
#define QPNP_PON_KPDPWR_RESIN_BARK_N_SET	BIT(5)

#define QPNP_PON_WD_EN				BIT(7)
#define QPNP_PON_RESET_EN			BIT(7)
#define QPNP_PON_POWER_OFF_MASK			0xF
#define QPNP_GEN2_POFF_SEQ			BIT(7)
#define QPNP_GEN2_FAULT_SEQ			BIT(6)
#define QPNP_GEN2_S3_RESET_SEQ			BIT(5)

#define QPNP_PON_S3_SRC_KPDPWR			0
#define QPNP_PON_S3_SRC_RESIN			1
#define QPNP_PON_S3_SRC_KPDPWR_AND_RESIN	2
#define QPNP_PON_S3_SRC_KPDPWR_OR_RESIN		3
#define QPNP_PON_S3_SRC_MASK			0x3
#define QPNP_PON_HARD_RESET_MASK		PON_MASK(7, 5)

#define QPNP_PON_UVLO_DLOAD_EN			BIT(7)
#define QPNP_PON_SMPL_EN			BIT(7)

/* Ranges */
#define QPNP_PON_S1_TIMER_MAX			10256
#define QPNP_PON_S2_TIMER_MAX			2000
#define QPNP_PON_S3_TIMER_SECS_MAX		128
#define QPNP_PON_S3_DBC_DELAY_MASK		0x07
#define QPNP_PON_RESET_TYPE_MAX			0xF
#define PON_S1_COUNT_MAX			0xF
#define QPNP_PON_MIN_DBC_US			(USEC_PER_SEC / 64)
#define QPNP_PON_MAX_DBC_US			(USEC_PER_SEC * 2)
#define QPNP_PON_GEN2_MIN_DBC_US		62
#define QPNP_PON_GEN2_MAX_DBC_US		(USEC_PER_SEC / 4)

#define QPNP_KEY_STATUS_DELAY			msecs_to_jiffies(250)

#define QPNP_PON_BUFFER_SIZE			9

#define QPNP_POFF_REASON_UVLO			13

/* Wakeup event timeout */
#define WAKEUP_TIMEOUT_MSEC			3000

enum qpnp_pon_version {
	QPNP_PON_GEN1_V1,
	QPNP_PON_GEN1_V2,
	QPNP_PON_GEN2,
};

enum pon_type {
	PON_KPDPWR,
	PON_RESIN,
	PON_CBLPWR,
	PON_KPDPWR_RESIN,
};

struct qpnp_pon_config {
	u32 pon_type;
	u32 support_reset;
	u32 key_code;
	u32 s1_timer;
	u32 s2_timer;
	u32 s2_type;
	u32 pull_up;
	u32 state_irq;
	u32 bark_irq;
	u16 s2_cntl_addr;
	u16 s2_cntl2_addr;
	bool old_state;
	bool use_bark;
	bool config_reset;
};

struct pon_regulator {
	struct qpnp_pon		*pon;
	struct regulator_dev	*rdev;
	struct regulator_desc	rdesc;
	u32			addr;
	u32			bit;
	bool			enabled;
};

struct qpnp_pon {
	struct spmi_device	*spmi;
	struct input_dev	*pon_input;
	struct qpnp_pon_config	*pon_cfg;
	struct pon_regulator	*pon_reg_cfg;
	struct list_head	list;
	struct delayed_work	bark_work;
	struct dentry		*debugfs;
	struct device_node	*pbs_dev_node;
	int			pon_trigger_reason;
	int			pon_power_off_reason;
	int			num_pon_reg;
	int			num_pon_config;
	int			reg_count;
	u32			dbc_time_us;
	u32			uvlo;
	int			warm_reset_poff_type;
	int			hard_reset_poff_type;
	int			shutdown_poff_type;
	u16			base;
	u8			subtype;
	u8			pon_ver;
	u8			warm_reset_reason1;
	u8			warm_reset_reason2;
	u8			twm_state;
	bool			is_spon;
	bool			store_hard_reset_reason;
	bool			kpdpwr_dbc_enable;
	bool			support_twm_config;
	ktime_t			kpdpwr_last_release_time;
	struct notifier_block	pon_nb;
};

static struct qpnp_pon *sys_reset_dev;
static DEFINE_SPINLOCK(spon_list_slock);
static LIST_HEAD(spon_dev_list);

static u32 s1_delay[PON_S1_COUNT_MAX + 1] = {
	0 , 32, 56, 80, 138, 184, 272, 408, 608, 904, 1352, 2048,
	3072, 4480, 6720, 10256
};

static const char * const qpnp_pon_reason[] = {
	[0] = "Triggered from Hard Reset",
	[1] = "Triggered from SMPL (sudden momentary power loss)",
	[2] = "Triggered from RTC (RTC alarm expiry)",
	[3] = "Triggered from DC (DC charger insertion)",
	[4] = "Triggered from USB (USB charger insertion)",
	[5] = "Triggered from PON1 (secondary PMIC)",
	[6] = "Triggered from CBL (external power supply)",
	[7] = "Triggered from KPD (power key press)",
};

#define POFF_REASON_FAULT_OFFSET	16
#define POFF_REASON_S3_RESET_OFFSET	32
static const char * const qpnp_poff_reason[] = {
	/* QPNP_PON_GEN1 POFF reasons */
	[0] = "Triggered from SOFT (Software)",
	[1] = "Triggered from PS_HOLD (PS_HOLD/MSM controlled shutdown)",
	[2] = "Triggered from PMIC_WD (PMIC watchdog)",
	[3] = "Triggered from GP1 (Keypad_Reset1)",
	[4] = "Triggered from GP2 (Keypad_Reset2)",
	[5] = "Triggered from KPDPWR_AND_RESIN (Simultaneous power key and reset line)",
	[6] = "Triggered from RESIN_N (Reset line/Volume Down Key)",
	[7] = "Triggered from KPDPWR_N (Long Power Key hold)",
	[8] = "N/A",
	[9] = "N/A",
	[10] = "N/A",
	[11] = "Triggered from CHARGER (Charger ENUM_TIMER, BOOT_DONE)",
	[12] = "Triggered from TFT (Thermal Fault Tolerance)",
	[13] = "Triggered from UVLO (Under Voltage Lock Out)",
	[14] = "Triggered from OTST3 (Overtemp)",
	[15] = "Triggered from STAGE3 (Stage 3 reset)",

	/* QPNP_PON_GEN2 FAULT reasons */
	[16] = "Triggered from GP_FAULT0",
	[17] = "Triggered from GP_FAULT1",
	[18] = "Triggered from GP_FAULT2",
	[19] = "Triggered from GP_FAULT3",
	[20] = "Triggered from MBG_FAULT",
	[21] = "Triggered from OVLO (Over Voltage Lock Out)",
	[22] = "Triggered from UVLO (Under Voltage Lock Out)",
	[23] = "Triggered from AVDD_RB",
	[24] = "N/A",
	[25] = "N/A",
	[26] = "N/A",
	[27] = "Triggered from FAULT_FAULT_N",
	[28] = "Triggered from FAULT_PBS_WATCHDOG_TO",
	[29] = "Triggered from FAULT_PBS_NACK",
	[30] = "Triggered from FAULT_RESTART_PON",
	[31] = "Triggered from OTST3 (Overtemp)",

	/* QPNP_PON_GEN2 S3_RESET reasons */
	[32] = "N/A",
	[33] = "N/A",
	[34] = "N/A",
	[35] = "N/A",
	[36] = "Triggered from S3_RESET_FAULT_N",
	[37] = "Triggered from S3_RESET_PBS_WATCHDOG_TO",
	[38] = "Triggered from S3_RESET_PBS_NACK",
	[39] = "Triggered from S3_RESET_KPDPWR_ANDOR_RESIN (power key and/or reset line)",
};

/*
 * On the kernel command line specify
 * qpnp-power-on.warm_boot=1 to indicate a warm
 * boot of the device.
 */
static int warm_boot;
module_param(warm_boot, int, 0);

static int
qpnp_pon_masked_write(struct qpnp_pon *pon, u16 addr, u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
							addr, &reg, 1);
	if (rc) {
		dev_err(&pon->spmi->dev,
			"Unable to read from addr=%hx, rc(%d)\n",
			addr, rc);
		return rc;
	}

	reg &= ~mask;
	reg |= val & mask;
	rc = spmi_ext_register_writel(pon->spmi->ctrl, pon->spmi->sid,
							addr, &reg, 1);
	if (rc)
		dev_err(&pon->spmi->dev,
			"Unable to write to addr=%hx, rc(%d)\n", addr, rc);
	return rc;
}

static bool is_pon_gen1(struct qpnp_pon *pon)
{
	return pon->subtype == PON_PRIMARY ||
			pon->subtype == PON_SECONDARY;
}

static bool is_pon_gen2(struct qpnp_pon *pon)
{
	return pon->subtype == PON_GEN2_PRIMARY ||
			pon->subtype == PON_GEN2_SECONDARY;
}

/**
 * qpnp_pon_set_restart_reason - Store device restart reason in PMIC register.
 *
 * Returns = 0 if PMIC feature is not available or store restart reason
 * successfully.
 * Returns > 0 for errors
 *
 * This function is used to store device restart reason in PMIC register.
 * It checks here to see if the restart reason register has been specified.
 * If it hasn't, this function should immediately return 0
 */
int qpnp_pon_set_restart_reason(enum pon_restart_reason reason)
{
	int rc = 0;
	struct qpnp_pon *pon = sys_reset_dev;

	if (!pon)
		return 0;

	if (!pon->store_hard_reset_reason)
		return 0;

	rc = qpnp_pon_masked_write(pon, QPNP_PON_SOFT_RB_SPARE(pon),
					PON_MASK(7, 2), (reason << 2));
	if (rc)
		dev_err(&pon->spmi->dev,
				"Unable to write to addr=%x, rc(%d)\n",
				QPNP_PON_SOFT_RB_SPARE(pon), rc);
	return rc;
}
EXPORT_SYMBOL(qpnp_pon_set_restart_reason);

/*
 * qpnp_pon_check_hard_reset_stored - Checks if the PMIC need to
 * store hard reset reason.
 *
 * Returns true if reset reason can be stored, false if it cannot be stored
 *
 */
bool qpnp_pon_check_hard_reset_stored(void)
{
	struct qpnp_pon *pon = sys_reset_dev;

	if (!pon)
		return false;

	return pon->store_hard_reset_reason;
}
EXPORT_SYMBOL(qpnp_pon_check_hard_reset_stored);

static int qpnp_pon_set_dbc(struct qpnp_pon *pon, u32 delay)
{
	int rc = 0;
	u32 val;

	if (delay == pon->dbc_time_us)
		goto out;

	if (pon->pon_input)
		mutex_lock(&pon->pon_input->mutex);

	if (is_pon_gen2(pon)) {
		if (delay < QPNP_PON_GEN2_MIN_DBC_US)
			delay = QPNP_PON_GEN2_MIN_DBC_US;
		else if (delay > QPNP_PON_GEN2_MAX_DBC_US)
			delay = QPNP_PON_GEN2_MAX_DBC_US;
		val = (delay << QPNP_PON_GEN2_DELAY_BIT_SHIFT) / USEC_PER_SEC;
	} else {
		if (delay < QPNP_PON_MIN_DBC_US)
			delay = QPNP_PON_MIN_DBC_US;
		else if (delay > QPNP_PON_MAX_DBC_US)
			delay = QPNP_PON_MAX_DBC_US;
		val = (delay << QPNP_PON_DELAY_BIT_SHIFT) / USEC_PER_SEC;
	}

	val = ilog2(val);
	rc = qpnp_pon_masked_write(pon, QPNP_PON_DBC_CTL(pon),
					QPNP_PON_DBC_DELAY_MASK(pon), val);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to set PON debounce\n");
		goto unlock;
	}

	pon->dbc_time_us = delay;

unlock:
	if (pon->pon_input)
		mutex_unlock(&pon->pon_input->mutex);
out:
	return rc;
}

static int qpnp_pon_get_dbc(struct qpnp_pon *pon, u32 *delay)
{
	int rc;
	u8 val;

	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
					QPNP_PON_DBC_CTL(pon), &val, 1);
	if (rc) {
		pr_err("Unable to read pon_dbc_ctl rc=%d\n", rc);
		return rc;
	}
	val &= QPNP_PON_DBC_DELAY_MASK(pon);

	if (is_pon_gen2(pon))
		*delay = USEC_PER_SEC /
			(1 << (QPNP_PON_GEN2_DELAY_BIT_SHIFT - val));
	else
		*delay = USEC_PER_SEC /
			(1 << (QPNP_PON_DELAY_BIT_SHIFT - val));

	return rc;
}

static ssize_t qpnp_pon_dbc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct qpnp_pon *pon = dev_get_drvdata(dev);

	return snprintf(buf, QPNP_PON_BUFFER_SIZE, "%d\n", pon->dbc_time_us);
}

static ssize_t qpnp_pon_dbc_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct qpnp_pon *pon = dev_get_drvdata(dev);
	u32 value;
	int rc;

	if (size > QPNP_PON_BUFFER_SIZE)
		return -EINVAL;

	rc = kstrtou32(buf, 10, &value);
	if (rc)
		return rc;

	rc = qpnp_pon_set_dbc(pon, value);
	if (rc < 0)
		return rc;

	return size;
}

static struct qpnp_pon_config *
qpnp_get_cfg(struct qpnp_pon *pon, u32 pon_type)
{
	int i;

	for (i = 0; i < pon->num_pon_config; i++) {
		if (pon_type == pon->pon_cfg[i].pon_type)
			return  &pon->pon_cfg[i];
	}

	return NULL;
}

static DEVICE_ATTR(debounce_us, 0664, qpnp_pon_dbc_show, qpnp_pon_dbc_store);

#define PON_TWM_ENTRY_PBS_BIT		BIT(0)
static int qpnp_pon_reset_config(struct qpnp_pon *pon,
		enum pon_power_off_type type)
{
	int rc;
	u16 rst_en_reg;
	struct qpnp_pon_config *cfg;

	/* Ignore the PS_HOLD reset config if TWM ENTRY is enabled */
	if (pon->support_twm_config && pon->twm_state == PMIC_TWM_ENABLE) {
		rc = qpnp_pbs_trigger_event(pon->pbs_dev_node,
					PON_TWM_ENTRY_PBS_BIT);
		if (rc < 0) {
			pr_err("Unable to trigger PBS trigger for TWM entry rc=%d\n",
							rc);
			return rc;
		}

		cfg = qpnp_get_cfg(pon, PON_KPDPWR);
		if (cfg) {
			/* configure KPDPWR_S2 to Hard reset */
			rc = qpnp_pon_masked_write(pon, cfg->s2_cntl_addr,
						QPNP_PON_S2_CNTL_TYPE_MASK,
						PON_POWER_OFF_HARD_RESET);
			if (rc < 0)
				pr_err("Unable to config KPDPWR_N S2 for hard-reset rc=%d\n",
					rc);
		}

		pr_crit("PMIC configured for TWM entry\n");
		return 0;
	}

	if (pon->pon_ver == QPNP_PON_GEN1_V1)
		rst_en_reg = QPNP_PON_PS_HOLD_RST_CTL(pon);
	else
		rst_en_reg = QPNP_PON_PS_HOLD_RST_CTL2(pon);

	/*
	 * Based on the poweroff type set for a PON device through device tree
	 * change the type being configured into PS_HOLD_RST_CTL.
	 */
	switch (type) {
	case PON_POWER_OFF_WARM_RESET:
		if (pon->warm_reset_poff_type != -EINVAL)
			type = pon->warm_reset_poff_type;
		break;
	case PON_POWER_OFF_HARD_RESET:
		if (pon->hard_reset_poff_type != -EINVAL)
			type = pon->hard_reset_poff_type;
		break;
	case PON_POWER_OFF_SHUTDOWN:
		if (pon->shutdown_poff_type != -EINVAL)
			type = pon->shutdown_poff_type;
		break;
	default:
		break;
	}

	rc = qpnp_pon_masked_write(pon, rst_en_reg, QPNP_PON_RESET_EN, 0);
	if (rc)
		dev_err(&pon->spmi->dev,
			"Unable to write to addr=%hx, rc(%d)\n",
			rst_en_reg, rc);

	/*
	 * We need 10 sleep clock cycles here. But since the clock is
	 * internally generated, we need to add 50% tolerance to be
	 * conservative.
	 */
	udelay(500);

	rc = qpnp_pon_masked_write(pon, QPNP_PON_PS_HOLD_RST_CTL(pon),
				   QPNP_PON_POWER_OFF_MASK, type);
	if (rc)
		dev_err(&pon->spmi->dev,
			"Unable to write to addr=%x, rc(%d)\n",
				QPNP_PON_PS_HOLD_RST_CTL(pon), rc);

	rc = qpnp_pon_masked_write(pon, rst_en_reg, QPNP_PON_RESET_EN,
						    QPNP_PON_RESET_EN);
	if (rc)
		dev_err(&pon->spmi->dev,
			"Unable to write to addr=%hx, rc(%d)\n",
			rst_en_reg, rc);

	dev_dbg(&pon->spmi->dev, "power off type = 0x%02X\n", type);
	return rc;
}

/**
 * qpnp_pon_system_pwr_off - Configure system-reset PMIC for shutdown or reset
 * @type: Determines the type of power off to perform - shutdown, reset, etc
 *
 * This function will support configuring for multiple PMICs. In some cases, the
 * PON of secondary PMICs also needs to be configured. So this supports that
 * requirement. Once the system-reset and secondary PMIC is configured properly,
 * the MSM can drop PS_HOLD to activate the specified configuration. Note that
 * this function may be called from atomic context as in the case of the panic
 * notifier path and thus it should not rely on function calls that may sleep.
 */
int qpnp_pon_system_pwr_off(enum pon_power_off_type type)
{
	int rc = 0;
	struct qpnp_pon *pon = sys_reset_dev;
	struct qpnp_pon *tmp;
	unsigned long flags;

	if (!pon)
		return -ENODEV;

	rc = qpnp_pon_reset_config(pon, type);
	if (rc) {
		dev_err(&pon->spmi->dev, "Error configuring main PON rc: %d\n",
			rc);
		return rc;
	}

	/*
	 * Check if a secondary PON device needs to be configured. If it
	 * is available, configure that also as per the requested power off
	 * type
	 */
	spin_lock_irqsave(&spon_list_slock, flags);
	if (list_empty(&spon_dev_list))
		goto out;

	list_for_each_entry_safe(pon, tmp, &spon_dev_list, list) {
		dev_emerg(&pon->spmi->dev,
				"PMIC@SID%d: configuring PON for reset\n",
				pon->spmi->sid);
		rc = qpnp_pon_reset_config(pon, type);
		if (rc) {
			dev_err(&pon->spmi->dev, "Error configuring secondary PON rc: %d\n",
				rc);
			goto out;
		}
	}
out:
	spin_unlock_irqrestore(&spon_list_slock, flags);
	return rc;
}
EXPORT_SYMBOL(qpnp_pon_system_pwr_off);

/**
 * qpnp_pon_is_warm_reset - Checks if the PMIC went through a warm reset.
 *
 * Returns > 0 for warm resets, 0 for not warm reset, < 0 for errors
 *
 * Note that this function will only return the warm vs not-warm reset status
 * of the PMIC that is configured as the system-reset device.
 */
int qpnp_pon_is_warm_reset(void)
{
	struct qpnp_pon *pon = sys_reset_dev;

	if (!pon)
		return -EPROBE_DEFER;

	if (is_pon_gen1(pon) || pon->subtype == PON_1REG)
		return pon->warm_reset_reason1
			|| (pon->warm_reset_reason2 & QPNP_PON_WARM_RESET_TFT);
	else
		return pon->warm_reset_reason1;
}
EXPORT_SYMBOL(qpnp_pon_is_warm_reset);

/**
 * qpnp_pon_wd_config - Disable the wd in a warm reset.
 * @enable: to enable or disable the PON watch dog
 *
 * Returns = 0 for operate successfully, < 0 for errors
 */
int qpnp_pon_wd_config(bool enable)
{
	struct qpnp_pon *pon = sys_reset_dev;
	int rc = 0;

	if (!pon)
		return -EPROBE_DEFER;

	rc = qpnp_pon_masked_write(pon, QPNP_PON_WD_RST_S2_CTL2(pon),
			QPNP_PON_WD_EN, enable ? QPNP_PON_WD_EN : 0);
	if (rc)
		dev_err(&pon->spmi->dev,
				"Unable to write to addr=%x, rc(%d)\n",
				QPNP_PON_WD_RST_S2_CTL2(pon), rc);

	return rc;
}
EXPORT_SYMBOL(qpnp_pon_wd_config);

static int qpnp_pon_get_trigger_config(enum pon_trigger_source pon_src,
							bool *enabled)
{
	struct qpnp_pon *pon = sys_reset_dev;
	int rc;
	u16 addr;
	u8 val;
	u8 mask;

	if (!pon)
		return -ENODEV;

	if (pon_src < PON_SMPL || pon_src > PON_KPDPWR_N) {
		dev_err(&pon->spmi->dev, "Invalid PON source\n");
		return -EINVAL;
	}

	addr = QPNP_PON_TRIGGER_EN(pon);
	mask = BIT(pon_src);
	if (is_pon_gen2(pon) && pon_src == PON_SMPL) {
		addr = QPNP_PON_SMPL_CTL(pon);
		mask = QPNP_PON_SMPL_EN;
	}

	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
							addr, &val, 1);
	if (rc)
		dev_err(&pon->spmi->dev,
			"Unable to read from addr=%hx, rc(%d)\n",
			addr, rc);
	else
		*enabled = !!(val & mask);

	return rc;
}

/**
 * qpnp_pon_trigger_config - Configures (enable/disable) the PON trigger source
 * @pon_src: PON source to be configured
 * @enable: to enable or disable the PON trigger
 *
 * This function configures the power-on trigger capability of a
 * PON source. If a specific PON trigger is disabled it cannot act
 * as a power-on source to the PMIC.
 */

int qpnp_pon_trigger_config(enum pon_trigger_source pon_src, bool enable)
{
	struct qpnp_pon *pon = sys_reset_dev;
	int rc;

	if (!pon)
		return -EPROBE_DEFER;

	if (pon_src < PON_SMPL || pon_src > PON_KPDPWR_N) {
		dev_err(&pon->spmi->dev, "Invalid PON source\n");
		return -EINVAL;
	}

	if (is_pon_gen2(pon) && pon_src == PON_SMPL) {
		rc = qpnp_pon_masked_write(pon, QPNP_PON_SMPL_CTL(pon),
			QPNP_PON_SMPL_EN, enable ? QPNP_PON_SMPL_EN : 0);
		if (rc)
			dev_err(&pon->spmi->dev, "Unable to write to addr=%x, rc(%d)\n",
				QPNP_PON_SMPL_CTL(pon), rc);
	} else {
		rc = qpnp_pon_masked_write(pon, QPNP_PON_TRIGGER_EN(pon),
				BIT(pon_src), enable ? BIT(pon_src) : 0);
		if (rc)
			dev_err(&pon->spmi->dev, "Unable to write to addr=%x, rc(%d)\n",
				QPNP_PON_TRIGGER_EN(pon), rc);
	}

	return rc;
}
EXPORT_SYMBOL(qpnp_pon_trigger_config);

/*
 * This function stores the PMIC warm reset reason register values. It also
 * clears these registers if the qcom,clear-warm-reset device tree property
 * is specified.
 */
static int qpnp_pon_store_and_clear_warm_reset(struct qpnp_pon *pon)
{
	int rc;
	u8 reg = 0;

	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
			QPNP_PON_WARM_RESET_REASON1(pon),
			&pon->warm_reset_reason1, 1);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to read addr=%x, rc(%d)\n",
			QPNP_PON_WARM_RESET_REASON1(pon), rc);
		return rc;
	}

	if (is_pon_gen1(pon) || pon->subtype == PON_1REG) {
		rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_WARM_RESET_REASON2(pon),
				&pon->warm_reset_reason2, 1);
		if (rc) {
			dev_err(&pon->spmi->dev,
				"Unable to read addr=%x, rc(%d)\n",
				QPNP_PON_WARM_RESET_REASON2(pon), rc);
			return rc;
		}
	}

	if (of_property_read_bool(pon->spmi->dev.of_node,
					"qcom,clear-warm-reset")) {
		rc = spmi_ext_register_writel(pon->spmi->ctrl, pon->spmi->sid,
			QPNP_PON_WARM_RESET_REASON1(pon), &reg, 1);
		if (rc)
			dev_err(&pon->spmi->dev,
				"Unable to write to addr=%hx, rc(%d)\n",
				QPNP_PON_WARM_RESET_REASON1(pon), rc);
	}

	return 0;
}

static int
qpnp_pon_input_dispatch(struct qpnp_pon *pon, u32 pon_type)
{
	int rc;
	struct qpnp_pon_config *cfg = NULL;
	u8 pon_rt_sts = 0, pon_rt_bit = 0;
	u32 key_status;
	u64 elapsed_us;

	cfg = qpnp_get_cfg(pon, pon_type);
	if (!cfg)
		return -EINVAL;

	/* Check if key reporting is supported */
	if (!cfg->key_code)
		return 0;

	if (device_may_wakeup(&pon->spmi->dev))
		pm_wakeup_event(&pon->spmi->dev, WAKEUP_TIMEOUT_MSEC);

	if (pon->kpdpwr_dbc_enable && cfg->pon_type == PON_KPDPWR) {
		elapsed_us = ktime_us_delta(ktime_get(),
				pon->kpdpwr_last_release_time);
		if (elapsed_us < pon->dbc_time_us) {
			pr_debug("Ignoring kpdpwr event - within debounce time\n");
			return 0;
		}
	}

	/* check the RT status to get the current status of the line */
	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_RT_STS(pon), &pon_rt_sts, 1);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to read PON RT status\n");
		return rc;
	}

	switch (cfg->pon_type) {
	case PON_KPDPWR:
		pon_rt_bit = QPNP_PON_KPDPWR_N_SET;
		break;
	case PON_RESIN:
		pon_rt_bit = QPNP_PON_RESIN_N_SET;
		break;
	case PON_CBLPWR:
		pon_rt_bit = QPNP_PON_CBLPWR_N_SET;
		break;
	case PON_KPDPWR_RESIN:
		pon_rt_bit = QPNP_PON_KPDPWR_RESIN_BARK_N_SET;
		break;
	default:
		return -EINVAL;
	}

	pr_debug("PMIC input: code=%d, sts=0x%hhx\n",
					cfg->key_code, pon_rt_sts);
	key_status = pon_rt_sts & pon_rt_bit;

	if (pon->kpdpwr_dbc_enable && cfg->pon_type == PON_KPDPWR) {
		if (!key_status)
			pon->kpdpwr_last_release_time = ktime_get();
	}

	/* simulate press event in case release event occured
	 * without a press event
	 */
	if (!cfg->old_state && !key_status) {
		input_report_key(pon->pon_input, cfg->key_code, 1);
		input_sync(pon->pon_input);
	}

	input_report_key(pon->pon_input, cfg->key_code, key_status);
	input_sync(pon->pon_input);

	cfg->old_state = !!key_status;

	return 0;
}

static irqreturn_t qpnp_kpdpwr_irq(int irq, void *_pon)
{
	int rc;
	struct qpnp_pon *pon = _pon;

	rc = qpnp_pon_input_dispatch(pon, PON_KPDPWR);
	if (rc)
		dev_err(&pon->spmi->dev, "Unable to send input event\n");

	return IRQ_HANDLED;
}

static irqreturn_t qpnp_kpdpwr_bark_irq(int irq, void *_pon)
{
	return IRQ_HANDLED;
}

static irqreturn_t qpnp_resin_irq(int irq, void *_pon)
{
	int rc;
	struct qpnp_pon *pon = _pon;

	rc = qpnp_pon_input_dispatch(pon, PON_RESIN);
	if (rc)
		dev_err(&pon->spmi->dev, "Unable to send input event\n");
	return IRQ_HANDLED;
}

static irqreturn_t qpnp_kpdpwr_resin_bark_irq(int irq, void *_pon)
{
	return IRQ_HANDLED;
}

static irqreturn_t qpnp_cblpwr_irq(int irq, void *_pon)
{
	int rc;
	struct qpnp_pon *pon = _pon;

	rc = qpnp_pon_input_dispatch(pon, PON_CBLPWR);
	if (rc)
		dev_err(&pon->spmi->dev, "Unable to send input event\n");

	return IRQ_HANDLED;
}

static void print_pon_reg(struct qpnp_pon *pon, u16 offset)
{
	int rc;
	u16 addr;
	u8 reg;

	addr = pon->base + offset;
	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
			addr, &reg, 1);
	if (rc)
		dev_emerg(&pon->spmi->dev,
				"Unable to read reg at 0x%04hx\n", addr);
	else
		dev_emerg(&pon->spmi->dev, "reg@0x%04hx: %02hhx\n", addr, reg);
}

#define PON_PBL_STATUS			0x7
#define PON_PON_REASON1(subtype)	PON_OFFSET(subtype, 0x8, 0xC0)
#define PON_PON_REASON2			0x9
#define PON_WARM_RESET_REASON1(subtype)	PON_OFFSET(subtype, 0xA, 0xC2)
#define PON_WARM_RESET_REASON2		0xB
#define PON_POFF_REASON1(subtype)	PON_OFFSET(subtype, 0xC, 0xC5)
#define PON_POFF_REASON2		0xD
#define PON_SOFT_RESET_REASON1(subtype)	PON_OFFSET(subtype, 0xE, 0xCB)
#define PON_SOFT_RESET_REASON2		0xF
#define PON_FAULT_REASON1		0xC8
#define PON_FAULT_REASON2		0xC9
#define PON_PMIC_WD_RESET_S1_TIMER	0x54
#define PON_PMIC_WD_RESET_S2_TIMER	0x55
static irqreturn_t qpnp_pmic_wd_bark_irq(int irq, void *_pon)
{
	struct qpnp_pon *pon = _pon;

	print_pon_reg(pon, PON_PBL_STATUS);
	print_pon_reg(pon, PON_PON_REASON1(pon->subtype));
	print_pon_reg(pon, PON_WARM_RESET_REASON1(pon->subtype));
	print_pon_reg(pon, PON_SOFT_RESET_REASON1(pon->subtype));
	print_pon_reg(pon, PON_POFF_REASON1(pon->subtype));
	if (is_pon_gen1(pon) || pon->subtype == PON_1REG) {
		print_pon_reg(pon, PON_PON_REASON2);
		print_pon_reg(pon, PON_WARM_RESET_REASON2);
		print_pon_reg(pon, PON_POFF_REASON2);
		print_pon_reg(pon, PON_SOFT_RESET_REASON2);
	} else {
		print_pon_reg(pon, PON_FAULT_REASON1);
		print_pon_reg(pon, PON_FAULT_REASON2);
	}
	print_pon_reg(pon, PON_PMIC_WD_RESET_S1_TIMER);
	print_pon_reg(pon, PON_PMIC_WD_RESET_S2_TIMER);
	panic("PMIC Watch dog triggered");

	return IRQ_HANDLED;
}

static void bark_work_func(struct work_struct *work)
{
	int rc;
	u8 pon_rt_sts = 0;
	struct qpnp_pon_config *cfg;
	struct qpnp_pon *pon =
		container_of(work, struct qpnp_pon, bark_work.work);

	cfg = qpnp_get_cfg(pon, PON_RESIN);
	if (!cfg) {
		dev_err(&pon->spmi->dev, "Invalid config pointer\n");
		goto err_return;
	}

	/* enable reset */
	rc = qpnp_pon_masked_write(pon, cfg->s2_cntl2_addr,
				QPNP_PON_S2_CNTL_EN, QPNP_PON_S2_CNTL_EN);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to configure S2 enable\n");
		goto err_return;
	}
	/* bark RT status update delay */
	msleep(100);
	/* read the bark RT status */
	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_RT_STS(pon), &pon_rt_sts, 1);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to read PON RT status\n");
		goto err_return;
	}

	if (!(pon_rt_sts & QPNP_PON_RESIN_BARK_N_SET)) {
		/* report the key event and enable the bark IRQ */
		input_report_key(pon->pon_input, cfg->key_code, 0);
		input_sync(pon->pon_input);
		enable_irq(cfg->bark_irq);
	} else {
		/* disable reset */
		rc = qpnp_pon_masked_write(pon, cfg->s2_cntl2_addr,
				QPNP_PON_S2_CNTL_EN, 0);
		if (rc) {
			dev_err(&pon->spmi->dev,
				"Unable to configure S2 enable\n");
			goto err_return;
		}
		/* re-arm the work */
		schedule_delayed_work(&pon->bark_work, QPNP_KEY_STATUS_DELAY);
	}

err_return:
	return;
}

static irqreturn_t qpnp_resin_bark_irq(int irq, void *_pon)
{
	int rc;
	struct qpnp_pon *pon = _pon;
	struct qpnp_pon_config *cfg;

	/* disable the bark interrupt */
	disable_irq_nosync(irq);

	cfg = qpnp_get_cfg(pon, PON_RESIN);
	if (!cfg) {
		dev_err(&pon->spmi->dev, "Invalid config pointer\n");
		goto err_exit;
	}

	/* disable reset */
	rc = qpnp_pon_masked_write(pon, cfg->s2_cntl2_addr,
					QPNP_PON_S2_CNTL_EN, 0);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to configure S2 enable\n");
		goto err_exit;
	}

	/* report the key event */
	input_report_key(pon->pon_input, cfg->key_code, 1);
	input_sync(pon->pon_input);
	/* schedule work to check the bark status for key-release */
	schedule_delayed_work(&pon->bark_work, QPNP_KEY_STATUS_DELAY);
err_exit:
	return IRQ_HANDLED;
}

static int
qpnp_config_pull(struct qpnp_pon *pon, struct qpnp_pon_config *cfg)
{
	int rc;
	u8 pull_bit;

	switch (cfg->pon_type) {
	case PON_KPDPWR:
		pull_bit = QPNP_PON_KPDPWR_PULL_UP;
		break;
	case PON_RESIN:
		pull_bit = QPNP_PON_RESIN_PULL_UP;
		break;
	case PON_CBLPWR:
		pull_bit = QPNP_PON_CBLPWR_PULL_UP;
		break;
	case PON_KPDPWR_RESIN:
		pull_bit = QPNP_PON_KPDPWR_PULL_UP | QPNP_PON_RESIN_PULL_UP;
		break;
	default:
		return -EINVAL;
	}

	rc = qpnp_pon_masked_write(pon, QPNP_PON_PULL_CTL(pon),
				pull_bit, cfg->pull_up ? pull_bit : 0);
	if (rc)
		dev_err(&pon->spmi->dev, "Unable to config pull-up\n");

	return rc;
}

static int
qpnp_config_reset(struct qpnp_pon *pon, struct qpnp_pon_config *cfg)
{
	int rc;
	u8 i;
	u16 s1_timer_addr, s2_timer_addr;

	switch (cfg->pon_type) {
	case PON_KPDPWR:
		s1_timer_addr = QPNP_PON_KPDPWR_S1_TIMER(pon);
		s2_timer_addr = QPNP_PON_KPDPWR_S2_TIMER(pon);
		break;
	case PON_RESIN:
		s1_timer_addr = QPNP_PON_RESIN_S1_TIMER(pon);
		s2_timer_addr = QPNP_PON_RESIN_S2_TIMER(pon);
		break;
	case PON_KPDPWR_RESIN:
		s1_timer_addr = QPNP_PON_KPDPWR_RESIN_S1_TIMER(pon);
		s2_timer_addr = QPNP_PON_KPDPWR_RESIN_S2_TIMER(pon);
		break;
	default:
		return -EINVAL;
	}
	/* disable S2 reset */
	rc = qpnp_pon_masked_write(pon, cfg->s2_cntl2_addr,
				QPNP_PON_S2_CNTL_EN, 0);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to configure S2 enable\n");
		return rc;
	}

	usleep_range(100, 120);

	/* configure s1 timer, s2 timer and reset type */
	for (i = 0; i < PON_S1_COUNT_MAX + 1; i++) {
		if (cfg->s1_timer <= s1_delay[i])
			break;
	}
	rc = qpnp_pon_masked_write(pon, s1_timer_addr,
				QPNP_PON_S1_TIMER_MASK, i);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to configure S1 timer\n");
		return rc;
	}

	i = 0;
	if (cfg->s2_timer) {
		i = cfg->s2_timer / 10;
		i = ilog2(i + 1);
	}

	rc = qpnp_pon_masked_write(pon, s2_timer_addr,
				QPNP_PON_S2_TIMER_MASK, i);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to configure S2 timer\n");
		return rc;
	}

	rc = qpnp_pon_masked_write(pon, cfg->s2_cntl_addr,
				QPNP_PON_S2_CNTL_TYPE_MASK, (u8)cfg->s2_type);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to configure S2 reset type\n");
		return rc;
	}

	/* enable S2 reset */
	rc = qpnp_pon_masked_write(pon, cfg->s2_cntl2_addr,
				QPNP_PON_S2_CNTL_EN, QPNP_PON_S2_CNTL_EN);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to configure S2 enable\n");
		return rc;
	}

	return 0;
}

static int
qpnp_pon_request_irqs(struct qpnp_pon *pon, struct qpnp_pon_config *cfg)
{
	int rc = 0;

	switch (cfg->pon_type) {
	case PON_KPDPWR:
		rc = devm_request_irq(&pon->spmi->dev, cfg->state_irq,
							qpnp_kpdpwr_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						"qpnp_kpdpwr_status", pon);
		if (rc < 0) {
			dev_err(&pon->spmi->dev, "Can't request %d IRQ\n",
							cfg->state_irq);
			return rc;
		}
		if (cfg->use_bark) {
			rc = devm_request_irq(&pon->spmi->dev, cfg->bark_irq,
						qpnp_kpdpwr_bark_irq,
						IRQF_TRIGGER_RISING,
						"qpnp_kpdpwr_bark", pon);
			if (rc < 0) {
				dev_err(&pon->spmi->dev,
					"Can't request %d IRQ\n",
						cfg->bark_irq);
				return rc;
			}
		}
		break;
	case PON_RESIN:
		rc = devm_request_irq(&pon->spmi->dev, cfg->state_irq,
							qpnp_resin_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
						"qpnp_resin_status", pon);
		if (rc < 0) {
			dev_err(&pon->spmi->dev, "Can't request %d IRQ\n",
							cfg->state_irq);
			return rc;
		}
		if (cfg->use_bark) {
			rc = devm_request_irq(&pon->spmi->dev, cfg->bark_irq,
						qpnp_resin_bark_irq,
						IRQF_TRIGGER_RISING,
						"qpnp_resin_bark", pon);
			if (rc < 0) {
				dev_err(&pon->spmi->dev,
					"Can't request %d IRQ\n",
						cfg->bark_irq);
				return rc;
			}
		}
		break;
	case PON_CBLPWR:
		rc = devm_request_irq(&pon->spmi->dev, cfg->state_irq,
							qpnp_cblpwr_irq,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
					"qpnp_cblpwr_status", pon);
		if (rc < 0) {
			dev_err(&pon->spmi->dev, "Can't request %d IRQ\n",
							cfg->state_irq);
			return rc;
		}
		break;
	case PON_KPDPWR_RESIN:
		if (cfg->use_bark) {
			rc = devm_request_irq(&pon->spmi->dev, cfg->bark_irq,
					qpnp_kpdpwr_resin_bark_irq,
					IRQF_TRIGGER_RISING,
					"qpnp_kpdpwr_resin_bark", pon);
			if (rc < 0) {
				dev_err(&pon->spmi->dev,
					"Can't request %d IRQ\n",
						cfg->bark_irq);
				return rc;
			}
		}
		break;
	default:
		return -EINVAL;
	}

	/* mark the interrupts wakeable if they support linux-key */
	if (cfg->key_code) {
		enable_irq_wake(cfg->state_irq);
		/* special handling for RESIN due to a hardware bug */
		if (cfg->pon_type == PON_RESIN && cfg->support_reset)
			enable_irq_wake(cfg->bark_irq);
	}

	return rc;
}

static int
qpnp_pon_config_input(struct qpnp_pon *pon,  struct qpnp_pon_config *cfg)
{
	if (!pon->pon_input) {
		pon->pon_input = input_allocate_device();
		if (!pon->pon_input) {
			dev_err(&pon->spmi->dev,
				"Can't allocate pon input device\n");
			return -ENOMEM;
		}
		pon->pon_input->name = "qpnp_pon";
		pon->pon_input->phys = "qpnp_pon/input0";
	}

	/* don't send dummy release event when system resumes */
	__set_bit(INPUT_PROP_NO_DUMMY_RELEASE, pon->pon_input->propbit);
	input_set_capability(pon->pon_input, EV_KEY, cfg->key_code);

	return 0;
}

static int qpnp_pon_config_init(struct qpnp_pon *pon)
{
	int rc = 0, i = 0, pmic_wd_bark_irq;
	struct device_node *pp = NULL;
	struct qpnp_pon_config *cfg;
	u8 pmic_type;
	u8 revid_rev4;

	if (!pon->num_pon_config) {
		dev_dbg(&pon->spmi->dev, "num_pon_config: %d\n",
			pon->num_pon_config);
		return 0;
	}

	/* iterate through the list of pon configs */
	for_each_available_child_of_node(pon->spmi->dev.of_node, pp) {
		if (!of_find_property(pp, "qcom,pon-type", NULL))
			continue;

		cfg = &pon->pon_cfg[i++];

		rc = of_property_read_u32(pp, "qcom,pon-type", &cfg->pon_type);
		if (rc) {
			dev_err(&pon->spmi->dev, "PON type not specified\n");
			return rc;
		}

		switch (cfg->pon_type) {
		case PON_KPDPWR:
			cfg->state_irq = spmi_get_irq_byname(pon->spmi,
							NULL, "kpdpwr");
			if (cfg->state_irq < 0) {
				dev_err(&pon->spmi->dev,
					"Unable to get kpdpwr irq\n");
				return cfg->state_irq;
			}

			rc = of_property_read_u32(pp, "qcom,support-reset",
							&cfg->support_reset);

			if (rc) {
				if (rc == -EINVAL) {
					dev_dbg(&pon->spmi->dev,
						"'qcom,support-reset' DT property doesn't exist\n");
				 } else {
					dev_err(&pon->spmi->dev,
						"Unable to read 'qcom,support-reset'\n");
					return rc;
				}
			} else {
				cfg->config_reset = true;
			}

			cfg->use_bark = of_property_read_bool(pp,
							"qcom,use-bark");
			if (cfg->use_bark) {
				cfg->bark_irq = spmi_get_irq_byname(pon->spmi,
							NULL, "kpdpwr-bark");
				if (cfg->bark_irq < 0) {
					dev_err(&pon->spmi->dev,
					"Unable to get kpdpwr-bark irq\n");
					return cfg->bark_irq;
				}
			}

			/* If the value read from REVISION2 register is 0x00,
			   then there is a single register to control s2 reset.
			   Otherwise there are separate registers for s2 reset
			   type and s2 reset enable */
			if (pon->pon_ver == QPNP_PON_GEN1_V1) {
				cfg->s2_cntl_addr = cfg->s2_cntl2_addr =
					QPNP_PON_KPDPWR_S2_CNTL(pon);
			} else {
				cfg->s2_cntl_addr =
					QPNP_PON_KPDPWR_S2_CNTL(pon);
				cfg->s2_cntl2_addr =
					QPNP_PON_KPDPWR_S2_CNTL2(pon);
			}

			break;
		case PON_RESIN:
			cfg->state_irq = spmi_get_irq_byname(pon->spmi,
							NULL, "resin");
			if (cfg->state_irq < 0) {
				dev_err(&pon->spmi->dev,
					"Unable to get resin irq\n");
				return cfg->bark_irq;
			}

			rc = of_property_read_u32(pp, "qcom,support-reset",
							&cfg->support_reset);

			if (rc) {
				if (rc == -EINVAL) {
					dev_dbg(&pon->spmi->dev,
						"'qcom,support-reset' DT property doesn't exist\n");
				} else {
					dev_err(&pon->spmi->dev,
						"Unable to read 'qcom,support-reset'\n");
					return rc;
				}
			} else {
				cfg->config_reset = true;
			}

			cfg->use_bark = of_property_read_bool(pp,
							"qcom,use-bark");

			rc = spmi_ext_register_readl(pon->spmi->ctrl,
					pon->spmi->sid, PMIC_VERSION_REG,
							&pmic_type, 1);

			if (rc) {
				dev_err(&pon->spmi->dev,
					"Unable to read PMIC type\n");
				return rc;
			}

			if (pmic_type == PMIC_VER_8941) {

				rc = spmi_ext_register_readl(pon->spmi->ctrl,
					pon->spmi->sid, PMIC_VERSION_REV4_REG,
							&revid_rev4, 1);

				if (rc) {
					dev_err(&pon->spmi->dev,
					"Unable to read PMIC revision ID\n");
					return rc;
				}

				/*PM8941 V3 does not have harware bug. Hence
				bark is not required from PMIC versions 3.0*/
				if (!(revid_rev4 == PMIC8941_V1_REV4 ||
					revid_rev4 == PMIC8941_V2_REV4)) {
					cfg->support_reset = false;
					cfg->use_bark = false;
				}
			}

			if (cfg->use_bark) {
				cfg->bark_irq = spmi_get_irq_byname(pon->spmi,
							NULL, "resin-bark");
				if (cfg->bark_irq < 0) {
					dev_err(&pon->spmi->dev,
					"Unable to get resin-bark irq\n");
					return cfg->bark_irq;
				}
			}

			if (pon->pon_ver == QPNP_PON_GEN1_V1) {
				cfg->s2_cntl_addr = cfg->s2_cntl2_addr =
					QPNP_PON_RESIN_S2_CNTL(pon);
			} else {
				cfg->s2_cntl_addr =
					QPNP_PON_RESIN_S2_CNTL(pon);
				cfg->s2_cntl2_addr =
					QPNP_PON_RESIN_S2_CNTL2(pon);
			}

			break;
		case PON_CBLPWR:
			cfg->state_irq = spmi_get_irq_byname(pon->spmi,
							NULL, "cblpwr");
			if (cfg->state_irq < 0) {
				dev_err(&pon->spmi->dev,
						"Unable to get cblpwr irq\n");
				return rc;
			}
			break;
		case PON_KPDPWR_RESIN:
			rc = of_property_read_u32(pp, "qcom,support-reset",
							&cfg->support_reset);

			if (rc) {
				if (rc == -EINVAL) {
					dev_dbg(&pon->spmi->dev,
						"'qcom,support-reset' DT property doesn't exist\n");
				} else {
					dev_err(&pon->spmi->dev,
						"Unable to read 'qcom,support-reset'\n");
					return rc;
				}
			} else {
				cfg->config_reset = true;
			}

			cfg->use_bark = of_property_read_bool(pp,
							"qcom,use-bark");
			if (cfg->use_bark) {
				cfg->bark_irq = spmi_get_irq_byname(pon->spmi,
						NULL, "kpdpwr-resin-bark");
				if (cfg->bark_irq < 0) {
					dev_err(&pon->spmi->dev,
					"Unable to get kpdpwr-resin-bark irq\n");
					return cfg->bark_irq;
				}
			}

			if (pon->pon_ver == QPNP_PON_GEN1_V1) {
				cfg->s2_cntl_addr = cfg->s2_cntl2_addr =
				QPNP_PON_KPDPWR_RESIN_S2_CNTL(pon);
			} else {
				cfg->s2_cntl_addr =
				QPNP_PON_KPDPWR_RESIN_S2_CNTL(pon);
				cfg->s2_cntl2_addr =
				QPNP_PON_KPDPWR_RESIN_S2_CNTL2(pon);
			}

			break;
		default:
			dev_err(&pon->spmi->dev, "PON RESET %d not supported",
								cfg->pon_type);
			return -EINVAL;
		}

		if (cfg->support_reset) {
			/*
			 * Get the reset parameters (bark debounce time and
			 * reset debounce time) for the reset line.
			 */
			rc = of_property_read_u32(pp, "qcom,s1-timer",
							&cfg->s1_timer);
			if (rc) {
				dev_err(&pon->spmi->dev,
					"Unable to read s1-timer\n");
				return rc;
			}
			if (cfg->s1_timer > QPNP_PON_S1_TIMER_MAX) {
				dev_err(&pon->spmi->dev,
					"Incorrect S1 debounce time\n");
				return -EINVAL;
			}
			rc = of_property_read_u32(pp, "qcom,s2-timer",
							&cfg->s2_timer);
			if (rc) {
				dev_err(&pon->spmi->dev,
					"Unable to read s2-timer\n");
				return rc;
			}
			if (cfg->s2_timer > QPNP_PON_S2_TIMER_MAX) {
				dev_err(&pon->spmi->dev,
					"Incorrect S2 debounce time\n");
				return -EINVAL;
			}
			rc = of_property_read_u32(pp, "qcom,s2-type",
							&cfg->s2_type);
			if (rc) {
				dev_err(&pon->spmi->dev,
					"Unable to read s2-type\n");
				return rc;
			}
			if (cfg->s2_type > QPNP_PON_RESET_TYPE_MAX) {
				dev_err(&pon->spmi->dev,
					"Incorrect reset type specified\n");
				return -EINVAL;
			}

		}
		/*
		 * Get the standard-key parameters. This might not be
		 * specified if there is no key mapping on the reset line.
		 */
		rc = of_property_read_u32(pp, "linux,code", &cfg->key_code);
		if (rc && rc != -EINVAL) {
			dev_err(&pon->spmi->dev,
				"Unable to read key-code\n");
			return rc;
		}
		/* Register key configuration */
		if (cfg->key_code) {
			rc = qpnp_pon_config_input(pon, cfg);
			if (rc < 0)
				return rc;
		}
		/* get the pull-up configuration */
		rc = of_property_read_u32(pp, "qcom,pull-up", &cfg->pull_up);
		if (rc && rc != -EINVAL) {
			dev_err(&pon->spmi->dev, "Unable to read pull-up\n");
			return rc;
		}
	}

	pmic_wd_bark_irq = spmi_get_irq_byname(pon->spmi, NULL, "pmic-wd-bark");
	/* request the pmic-wd-bark irq only if it is defined */
	if (pmic_wd_bark_irq >= 0) {
		rc = devm_request_irq(&pon->spmi->dev, pmic_wd_bark_irq,
					qpnp_pmic_wd_bark_irq,
					IRQF_TRIGGER_RISING,
					"qpnp_pmic_wd_bark", pon);
		if (rc < 0) {
			dev_err(&pon->spmi->dev,
				"Can't request %d IRQ\n",
					pmic_wd_bark_irq);
			goto free_input_dev;
		}
	}

	/* register the input device */
	if (pon->pon_input) {
		rc = input_register_device(pon->pon_input);
		if (rc) {
			dev_err(&pon->spmi->dev,
				"Can't register pon key: %d\n", rc);
			goto free_input_dev;
		}
	}

	for (i = 0; i < pon->num_pon_config; i++) {
		cfg = &pon->pon_cfg[i];
		/* Configure the pull-up */
		rc = qpnp_config_pull(pon, cfg);
		if (rc) {
			dev_err(&pon->spmi->dev, "Unable to config pull-up\n");
			goto unreg_input_dev;
		}

		if (cfg->config_reset) {
			/* Configure the reset-configuration */
			if (cfg->support_reset) {
				rc = qpnp_config_reset(pon, cfg);
				if (rc) {
					dev_err(&pon->spmi->dev,
						"Unable to config pon reset\n");
					goto unreg_input_dev;
				}
			} else {
				if (cfg->pon_type != PON_CBLPWR) {
					/* disable S2 reset */
					rc = qpnp_pon_masked_write(pon,
						cfg->s2_cntl2_addr,
						QPNP_PON_S2_CNTL_EN, 0);
					if (rc) {
						dev_err(&pon->spmi->dev,
							"Unable to disable S2 reset\n");
						goto unreg_input_dev;
					}
				}
			}
		}

		rc = qpnp_pon_request_irqs(pon, cfg);
		if (rc) {
			dev_err(&pon->spmi->dev, "Unable to request-irq's\n");
			goto unreg_input_dev;
		}
	}

	device_init_wakeup(&pon->spmi->dev, 1);

	return rc;

unreg_input_dev:
	if (pon->pon_input)
		input_unregister_device(pon->pon_input);
free_input_dev:
	if (pon->pon_input)
		input_free_device(pon->pon_input);
	return rc;
}

static int pon_spare_regulator_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	u8 value;
	struct pon_regulator *pon_reg = rdev_get_drvdata(rdev);

	pr_debug("reg %s enable addr: %x bit: %d\n", rdev->desc->name,
		pon_reg->addr, pon_reg->bit);

	value = BIT(pon_reg->bit) & 0xFF;
	rc = qpnp_pon_masked_write(pon_reg->pon, pon_reg->pon->base +
				pon_reg->addr, value, value);
	if (rc)
		dev_err(&pon_reg->pon->spmi->dev, "Unable to write to %x\n",
			pon_reg->pon->base + pon_reg->addr);
	else
		pon_reg->enabled = true;
	return rc;
}

static int pon_spare_regulator_disable(struct regulator_dev *rdev)
{
	int rc = 0;
	u8 mask;
	struct pon_regulator *pon_reg = rdev_get_drvdata(rdev);

	pr_debug("reg %s disable addr: %x bit: %d\n", rdev->desc->name,
		pon_reg->addr, pon_reg->bit);

	mask = BIT(pon_reg->bit) & 0xFF;
	rc = qpnp_pon_masked_write(pon_reg->pon, pon_reg->pon->base +
				pon_reg->addr, mask, 0);
	if (rc)
		dev_err(&pon_reg->pon->spmi->dev, "Unable to write to %x\n",
			pon_reg->pon->base + pon_reg->addr);
	else
		pon_reg->enabled = false;
	return rc;
}

static int pon_spare_regulator_is_enable(struct regulator_dev *rdev)
{
	struct pon_regulator *pon_reg = rdev_get_drvdata(rdev);

	return pon_reg->enabled;
}

struct regulator_ops pon_spare_reg_ops = {
	.enable		= pon_spare_regulator_enable,
	.disable	= pon_spare_regulator_disable,
	.is_enabled	= pon_spare_regulator_is_enable,
};

static int pon_regulator_init(struct qpnp_pon *pon)
{
	int rc = 0, i = 0;
	struct regulator_init_data *init_data;
	struct regulator_config reg_cfg = {};
	struct device_node *node = NULL;
	struct device *dev = &pon->spmi->dev;
	struct pon_regulator *pon_reg;

	if (!pon->num_pon_reg)
		return 0;

	pon->pon_reg_cfg = devm_kcalloc(dev, pon->num_pon_reg,
					sizeof(*(pon->pon_reg_cfg)),
					GFP_KERNEL);

	if (!pon->pon_reg_cfg)
		return -ENOMEM;

	for_each_available_child_of_node(dev->of_node, node) {
		if (!of_find_property(node, "regulator-name", NULL))
			continue;

		pon_reg = &pon->pon_reg_cfg[i++];
		pon_reg->pon = pon;

		rc = of_property_read_u32(node, "qcom,pon-spare-reg-addr",
			&pon_reg->addr);
		if (rc) {
			dev_err(dev, "Unable to read address for regulator, rc=%d\n",
				rc);
			return rc;
		}

		rc = of_property_read_u32(node, "qcom,pon-spare-reg-bit",
			&pon_reg->bit);
		if (rc) {
			dev_err(dev, "Unable to read bit for regulator, rc=%d\n",
				rc);
			return rc;
		}

		init_data = of_get_regulator_init_data(dev, node);
		if (!init_data) {
			dev_err(dev, "regulator init data is missing\n");
			return -EINVAL;
		}
		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_STATUS;

		if (!init_data->constraints.name) {
			dev_err(dev, "regulator-name is missing\n");
			return -EINVAL;
		}

		pon_reg->rdesc.owner = THIS_MODULE;
		pon_reg->rdesc.type = REGULATOR_VOLTAGE;
		pon_reg->rdesc.ops = &pon_spare_reg_ops;
		pon_reg->rdesc.name = init_data->constraints.name;

		reg_cfg.dev = dev;
		reg_cfg.init_data = init_data;
		reg_cfg.driver_data = pon_reg;
		reg_cfg.of_node = node;

		pon_reg->rdev = regulator_register(&pon_reg->rdesc, &reg_cfg);
		if (IS_ERR(pon_reg->rdev)) {
			rc = PTR_ERR(pon_reg->rdev);
			pon_reg->rdev = NULL;
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "regulator_register failed, rc=%d\n",
					rc);
			return rc;
		}
	}
	return rc;
}

static bool smpl_en;

static int qpnp_pon_smpl_en_get(char *buf, const struct kernel_param *kp)
{
	bool enabled;
	int rc;

	rc = qpnp_pon_get_trigger_config(PON_SMPL, &enabled);
	if (rc < 0)
		return rc;

	return snprintf(buf, QPNP_PON_BUFFER_SIZE, "%d", enabled);
}

static int qpnp_pon_smpl_en_set(const char *val,
					const struct kernel_param *kp)
{
	int rc;

	rc = param_set_bool(val, kp);
	if (rc < 0) {
		pr_err("Unable to set smpl_en rc=%d\n", rc);
		return rc;
	}

	rc = qpnp_pon_trigger_config(PON_SMPL, *(bool *)kp->arg);
	return rc;
}

static struct kernel_param_ops smpl_en_ops = {
	.set = qpnp_pon_smpl_en_set,
	.get = qpnp_pon_smpl_en_get,
};

module_param_cb(smpl_en, &smpl_en_ops, &smpl_en, 0644);

static bool dload_on_uvlo;

static int qpnp_pon_debugfs_uvlo_dload_get(char *buf,
		const struct kernel_param *kp)
{
	struct qpnp_pon *pon = sys_reset_dev;
	int rc = 0;
	u8 reg;

	if (!pon)
		return -ENODEV;

	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
			QPNP_PON_XVDD_RB_SPARE(pon), &reg, 1);
	if (rc) {
		dev_err(&pon->spmi->dev,
			"Unable to read addr=%x, rc(%d)\n",
			QPNP_PON_XVDD_RB_SPARE(pon), rc);
		return rc;
	}

	return snprintf(buf, PAGE_SIZE, "%d",
			!!(QPNP_PON_UVLO_DLOAD_EN & reg));
}

static int qpnp_pon_debugfs_uvlo_dload_set(const char *val,
		const struct kernel_param *kp)
{
	struct qpnp_pon *pon = sys_reset_dev;
	int rc = 0;
	u8 reg;

	if (!pon)
		return -ENODEV;

	rc = param_set_bool(val, kp);
	if (rc) {
		pr_err("Unable to set bms_reset: %d\n", rc);
		return rc;
	}

	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
			QPNP_PON_XVDD_RB_SPARE(pon), &reg, 1);
	if (rc) {
		dev_err(&pon->spmi->dev,
			"Unable to read addr=%x, rc(%d)\n",
			QPNP_PON_XVDD_RB_SPARE(pon), rc);
		return rc;
	}

	reg &= ~QPNP_PON_UVLO_DLOAD_EN;
	if (*(bool *)kp->arg)
		reg |= QPNP_PON_UVLO_DLOAD_EN;

	rc = spmi_ext_register_writel(pon->spmi->ctrl, pon->spmi->sid,
			QPNP_PON_XVDD_RB_SPARE(pon), &reg, 1);
	if (rc) {
		dev_err(&pon->spmi->dev,
			"Unable to write to addr=%hx, rc(%d)\n",
				QPNP_PON_XVDD_RB_SPARE(pon), rc);
		return rc;
	}

	return 0;
}

static struct kernel_param_ops dload_on_uvlo_ops = {
	.set = qpnp_pon_debugfs_uvlo_dload_set,
	.get = qpnp_pon_debugfs_uvlo_dload_get,
};

module_param_cb(dload_on_uvlo, &dload_on_uvlo_ops, &dload_on_uvlo, 0644);

#if defined(CONFIG_DEBUG_FS)

static int qpnp_pon_debugfs_uvlo_get(void *data, u64 *val)
{
	struct qpnp_pon *pon = (struct qpnp_pon *) data;

	*val = pon->uvlo;

	return 0;
}

static int qpnp_pon_debugfs_uvlo_set(void *data, u64 val)
{
	struct qpnp_pon *pon = (struct qpnp_pon *) data;

	if (pon->pon_trigger_reason == PON_SMPL ||
		pon->pon_power_off_reason == QPNP_POFF_REASON_UVLO)
		panic("An UVLO was occurred.\n");
	pon->uvlo = val;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(qpnp_pon_debugfs_uvlo_fops, qpnp_pon_debugfs_uvlo_get,
			qpnp_pon_debugfs_uvlo_set, "0x%02llx\n");

static void qpnp_pon_debugfs_init(struct spmi_device *spmi)
{
	struct qpnp_pon *pon = dev_get_drvdata(&spmi->dev);
	struct dentry *ent;

	pon->debugfs = debugfs_create_dir(dev_name(&spmi->dev), NULL);
	if (!pon->debugfs) {
		dev_err(&pon->spmi->dev, "Unable to create debugfs directory\n");
	} else {
		ent = debugfs_create_file("uvlo_panic",
				S_IFREG | S_IWUSR | S_IRUGO,
				pon->debugfs, pon, &qpnp_pon_debugfs_uvlo_fops);
		if (!ent)
			dev_err(&pon->spmi->dev, "Unable to create uvlo_panic debugfs file.\n");
	}
}

static void qpnp_pon_debugfs_remove(struct spmi_device *spmi)
{
	struct qpnp_pon *pon = dev_get_drvdata(&spmi->dev);

	debugfs_remove_recursive(pon->debugfs);
}

#else

static void qpnp_pon_debugfs_init(struct spmi_device *spmi)
{}

static void qpnp_pon_debugfs_remove(struct spmi_device *spmi)
{}
#endif

static int read_gen2_pon_off_reason(struct qpnp_pon *pon, u16 *reason,
					int *reason_index_offset)
{
	int rc;
	u8 buf[2], reg;

	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_OFF_REASON(pon),
				&reg, 1);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to read PON_OFF_REASON reg rc:%d\n",
			rc);
		return rc;
	}

	if (reg & QPNP_GEN2_POFF_SEQ) {
		rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
						QPNP_POFF_REASON1(pon),
						buf, 1);
		if (rc) {
			dev_err(&pon->spmi->dev, "Unable to read POFF_REASON1 reg rc:%d\n",
				rc);
			return rc;
		}
		*reason = buf[0];
		*reason_index_offset = 0;
	} else if (reg & QPNP_GEN2_FAULT_SEQ) {
		rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
						QPNP_FAULT_REASON1(pon),
						buf, 2);
		if (rc) {
			dev_err(&pon->spmi->dev, "Unable to read FAULT_REASON regs rc:%d\n",
				rc);
			return rc;
		}
		*reason = buf[0] | (buf[1] << 8);
		*reason_index_offset = POFF_REASON_FAULT_OFFSET;
	} else if (reg & QPNP_GEN2_S3_RESET_SEQ) {
		rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
						QPNP_S3_RESET_REASON(pon),
						buf, 1);
		if (rc) {
			dev_err(&pon->spmi->dev, "Unable to read S3_RESET_REASON reg rc:%d\n",
				rc);
			return rc;
		}
		*reason = buf[0];
		*reason_index_offset = POFF_REASON_S3_RESET_OFFSET;
	}

	return 0;
}

static int pon_twm_notifier_cb(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct qpnp_pon *pon = container_of(nb, struct qpnp_pon, pon_nb);

	if (action != PMIC_TWM_CLEAR &&
			action != PMIC_TWM_ENABLE) {
		pr_debug("Unsupported option %lu\n", action);
		return NOTIFY_OK;
	}

	pon->twm_state = (u8)action;
	pr_debug("TWM state = %d\n", pon->twm_state);

	return NOTIFY_OK;
}

static int pon_register_twm_notifier(struct qpnp_pon *pon)
{
	int rc;

	pon->pon_nb.notifier_call = pon_twm_notifier_cb;
	rc = qpnp_misc_twm_notifier_register(&pon->pon_nb);
	if (rc < 0)
		pr_err("Failed to register pon_twm_notifier_cb rc=%d\n", rc);

	return rc;
}

static int qpnp_pon_probe(struct spmi_device *spmi)
{
	struct qpnp_pon *pon;
	struct resource *pon_resource;
	struct device_node *node = NULL;
	u32 delay = 0, s3_debounce = 0;
	int rc, sys_reset, index;
	int reason_index_offset = 0;
	u8 pon_sts = 0, buf[2];
	u16 poff_sts = 0;
	const char *s3_src;
	u8 s3_src_reg;
	unsigned long flags;

	pon = devm_kzalloc(&spmi->dev, sizeof(struct qpnp_pon),
							GFP_KERNEL);
	if (!pon) {
		dev_err(&spmi->dev, "Can't allocate qpnp_pon\n");
		return -ENOMEM;
	}

	sys_reset = of_property_read_bool(spmi->dev.of_node,
						"qcom,system-reset");
	if (sys_reset && sys_reset_dev) {
		dev_err(&spmi->dev, "qcom,system-reset property can only be specified for one device on the system\n");
		return -EINVAL;
	} else if (sys_reset) {
		sys_reset_dev = pon;
	}

	pon->spmi = spmi;

	pon_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!pon_resource) {
		dev_err(&spmi->dev, "Unable to get PON base address\n");
		return -ENXIO;
	}
	pon->base = pon_resource->start;

	/* get the total number of pon configurations */
	for_each_available_child_of_node(spmi->dev.of_node, node) {
		if (of_find_property(node, "regulator-name", NULL)) {
			pon->num_pon_reg++;
		} else if (of_find_property(node, "qcom,pon-type", NULL)) {
			pon->num_pon_config++;
		} else {
			pr_err("Unknown sub-node\n");
			return -EINVAL;
		}
	}

	pr_debug("PON@SID %d: num_pon_config: %d num_pon_reg: %d\n",
		pon->spmi->sid, pon->num_pon_config, pon->num_pon_reg);

	rc = pon_regulator_init(pon);
	if (rc) {
		dev_err(&pon->spmi->dev, "Error in pon_regulator_init rc: %d\n",
			rc);
		return rc;
	}

	if (!pon->num_pon_config)
		/* No PON config., do not register the driver */
		dev_info(&spmi->dev, "No PON config. specified\n");
	else
		pon->pon_cfg = devm_kzalloc(&spmi->dev,
				sizeof(struct qpnp_pon_config) *
				pon->num_pon_config, GFP_KERNEL);

	/* Read PON_PERPH_SUBTYPE register to get PON type */
	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_PERPH_SUBTYPE(pon),
				&pon->subtype, 1);
	if (rc) {
		dev_err(&pon->spmi->dev,
			"Unable to read PON_PERPH_SUBTYPE register rc: %d\n",
			rc);
		return rc;
	}

	/* Check if it is rev B */
	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
			QPNP_PON_REVISION2(pon), &pon->pon_ver, 1);
	if (rc) {
		dev_err(&pon->spmi->dev,
			"Unable to read addr=%x, rc(%d)\n",
			QPNP_PON_REVISION2(pon), rc);
		return rc;
	}
	if (is_pon_gen1(pon)) {
		if (pon->pon_ver == 0)
			pon->pon_ver = QPNP_PON_GEN1_V1;
		else
			pon->pon_ver = QPNP_PON_GEN1_V2;
	} else if (is_pon_gen2(pon)) {
		pon->pon_ver = QPNP_PON_GEN2;
	} else if (pon->subtype == PON_1REG) {
		pon->pon_ver = QPNP_PON_GEN1_V2;
	} else {
		dev_err(&pon->spmi->dev,
			"Invalid PON_PERPH_SUBTYPE value %x\n",
			pon->subtype);
		return -EINVAL;
	}

	pr_debug("%s: pon_subtype=%x, pon_version=%x\n", __func__,
			pon->subtype, pon->pon_ver);

	rc = qpnp_pon_store_and_clear_warm_reset(pon);
	if (rc) {
		dev_err(&pon->spmi->dev,
			"Unable to store/clear WARM_RESET_REASONx registers rc: %d\n",
			rc);
		return rc;
	}

	/* PON reason */
	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_REASON1(pon), &pon_sts, 1);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to read PON_REASON1 reg rc: %d\n",
			rc);
		return rc;
	}

	index = ffs(pon_sts) - 1;
	cold_boot = !qpnp_pon_is_warm_reset();
	if (index >= ARRAY_SIZE(qpnp_pon_reason) || index < 0) {
		dev_info(&pon->spmi->dev,
			"PMIC@SID%d Power-on reason: Unknown and '%s' boot\n",
			pon->spmi->sid, cold_boot ? "cold" : "warm");
	} else {
		pon->pon_trigger_reason = index;
		dev_info(&pon->spmi->dev,
			"PMIC@SID%d Power-on reason: %s and '%s' boot\n",
			pon->spmi->sid, qpnp_pon_reason[index],
			cold_boot ? "cold" : "warm");
	}

	/* POFF reason */
	if (!is_pon_gen1(pon) && pon->subtype != PON_1REG) {
		rc = read_gen2_pon_off_reason(pon, &poff_sts,
						&reason_index_offset);
		if (rc)
			return rc;
	} else {
		rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_POFF_REASON1(pon),
				buf, 2);
		if (rc) {
			dev_err(&pon->spmi->dev, "Unable to read POFF_REASON regs rc:%d\n",
				rc);
			return rc;
		}
		poff_sts = buf[0] | (buf[1] << 8);
	}
	index = ffs(poff_sts) - 1 + reason_index_offset;
	if (index >= ARRAY_SIZE(qpnp_poff_reason) || index < 0) {
		dev_info(&pon->spmi->dev,
				"PMIC@SID%d: Unknown power-off reason\n",
				pon->spmi->sid);
	} else {
		pon->pon_power_off_reason = index;
		dev_info(&pon->spmi->dev,
				"PMIC@SID%d: Power-off reason: %s\n",
				pon->spmi->sid,
				qpnp_poff_reason[index]);
	}

	if (pon->pon_trigger_reason == PON_SMPL ||
		pon->pon_power_off_reason == QPNP_POFF_REASON_UVLO) {
		if (of_property_read_bool(spmi->dev.of_node,
						"qcom,uvlo-panic"))
			panic("An UVLO was occurred.");
	}

	/* program s3 debounce */
	rc = of_property_read_u32(pon->spmi->dev.of_node,
				"qcom,s3-debounce", &s3_debounce);
	if (rc) {
		if (rc != -EINVAL) {
			dev_err(&pon->spmi->dev, "Unable to read s3 timer rc:%d\n",
				rc);
			return rc;
		}
	} else {
		if (s3_debounce > QPNP_PON_S3_TIMER_SECS_MAX) {
			dev_info(&pon->spmi->dev,
				"Exceeded S3 max value, set it to max\n");
			s3_debounce = QPNP_PON_S3_TIMER_SECS_MAX;
		}

		/* 0 is a special value to indicate instant s3 reset */
		if (s3_debounce != 0)
			s3_debounce = ilog2(s3_debounce);

		/* s3 debounce is SEC_ACCESS register */
		rc = qpnp_pon_masked_write(pon, QPNP_PON_SEC_ACCESS(pon),
					0xFF, QPNP_PON_SEC_UNLOCK);
		if (rc) {
			dev_err(&spmi->dev, "Unable to do SEC_ACCESS rc:%d\n",
				rc);
			return rc;
		}

		rc = qpnp_pon_masked_write(pon, QPNP_PON_S3_DBC_CTL(pon),
				QPNP_PON_S3_DBC_DELAY_MASK, s3_debounce);
		if (rc) {
			dev_err(&spmi->dev, "Unable to set S3 debounce rc:%d\n",
				rc);
			return rc;
		}
	}

	/* program s3 source */
	s3_src = "kpdpwr-and-resin";
	rc = of_property_read_string(pon->spmi->dev.of_node,
				"qcom,s3-src", &s3_src);
	if (rc && rc != -EINVAL) {
		dev_err(&pon->spmi->dev, "Unable to read s3 timer rc: %d\n",
			rc);
		return rc;
	}

	if (!strcmp(s3_src, "kpdpwr"))
		s3_src_reg = QPNP_PON_S3_SRC_KPDPWR;
	else if (!strcmp(s3_src, "resin"))
		s3_src_reg = QPNP_PON_S3_SRC_RESIN;
	else if (!strcmp(s3_src, "kpdpwr-or-resin"))
		s3_src_reg = QPNP_PON_S3_SRC_KPDPWR_OR_RESIN;
	else /* default combination */
		s3_src_reg = QPNP_PON_S3_SRC_KPDPWR_AND_RESIN;

	/*
	 * S3 source is a write once register. If the register has
	 * been configured by bootloader then this operation will
	 * not be effective.
	 */
	rc = qpnp_pon_masked_write(pon, QPNP_PON_S3_SRC(pon),
			QPNP_PON_S3_SRC_MASK, s3_src_reg);
	if (rc) {
		dev_err(&spmi->dev, "Unable to program s3 source rc: %d\n", rc);
		return rc;
	}

	dev_set_drvdata(&spmi->dev, pon);

	INIT_DELAYED_WORK(&pon->bark_work, bark_work_func);

	/* register the PON configurations */
	rc = qpnp_pon_config_init(pon);
	if (rc) {
		dev_err(&spmi->dev,
			"Unable to initialize PON configurations rc: %d\n", rc);
		return rc;
	}

	if (of_property_read_bool(pon->spmi->dev.of_node,
					"qcom,support-twm-config")) {
		pon->support_twm_config = true;
		rc = pon_register_twm_notifier(pon);
		if (rc < 0) {
			pr_err("Failed to register TWM notifier rc=%d\n", rc);
			return rc;
		}
		pon->pbs_dev_node = of_parse_phandle(pon->spmi->dev.of_node,
						"qcom,pbs-client", 0);
		if (!pon->pbs_dev_node) {
			pr_err("Missing qcom,pbs-client property\n");
			return -EINVAL;
		}
	}

	rc = of_property_read_u32(pon->spmi->dev.of_node,
				"qcom,pon-dbc-delay", &delay);
	if (rc) {
		if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read debounce delay rc: %d\n",
				rc);
			return rc;
		}
	} else {
		rc = qpnp_pon_set_dbc(pon, delay);
		if (rc) {
			dev_err(&spmi->dev,
				"Unable to set PON debounce delay rc=%d\n", rc);
			return rc;
		}
	}
	rc = qpnp_pon_get_dbc(pon, &pon->dbc_time_us);
	if (rc) {
		dev_err(&spmi->dev,
			"Unable to get PON debounce delay rc=%d\n", rc);
		return rc;
	}

	pon->kpdpwr_dbc_enable = of_property_read_bool(pon->spmi->dev.of_node,
					"qcom,kpdpwr-sw-debounce");

	rc = of_property_read_u32(pon->spmi->dev.of_node,
				"qcom,warm-reset-poweroff-type",
				&pon->warm_reset_poff_type);
	if (rc) {
		if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read warm reset poweroff type rc: %d\n",
				rc);
			return rc;
		}
		pon->warm_reset_poff_type = -EINVAL;
	} else if (pon->warm_reset_poff_type <= PON_POWER_OFF_RESERVED ||
			pon->warm_reset_poff_type >= PON_POWER_OFF_MAX_TYPE) {
		dev_err(&spmi->dev, "Invalid warm-reset-poweroff-type\n");
		pon->warm_reset_poff_type = -EINVAL;
	}

	rc = of_property_read_u32(pon->spmi->dev.of_node,
				"qcom,hard-reset-poweroff-type",
				&pon->hard_reset_poff_type);
	if (rc) {
		if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read hard reset poweroff type rc: %d\n",
				rc);
			return rc;
		}
		pon->hard_reset_poff_type = -EINVAL;
	} else if (pon->hard_reset_poff_type <= PON_POWER_OFF_RESERVED ||
			pon->hard_reset_poff_type >= PON_POWER_OFF_MAX_TYPE) {
		dev_err(&spmi->dev, "Invalid hard-reset-poweroff-type\n");
		pon->hard_reset_poff_type = -EINVAL;
	}

	rc = of_property_read_u32(pon->spmi->dev.of_node,
				"qcom,shutdown-poweroff-type",
				&pon->shutdown_poff_type);
	if (rc) {
		if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read shutdown poweroff type rc: %d\n",
				rc);
			return rc;
		}
		pon->shutdown_poff_type = -EINVAL;
	} else if (pon->shutdown_poff_type <= PON_POWER_OFF_RESERVED ||
			pon->shutdown_poff_type >= PON_POWER_OFF_MAX_TYPE) {
		dev_err(&spmi->dev, "Invalid shutdown-poweroff-type\n");
		pon->shutdown_poff_type = -EINVAL;
	}

	rc = device_create_file(&spmi->dev, &dev_attr_debounce_us);
	if (rc) {
		dev_err(&spmi->dev, "sys file creation failed rc: %d\n",
			rc);
		return rc;
	}

	if (of_property_read_bool(spmi->dev.of_node,
					"qcom,pon-reset-off")) {
		rc = qpnp_pon_trigger_config(PON_CBLPWR_N, false);
		if (rc) {
			dev_err(&spmi->dev, "failed update the PON_CBLPWR %d\n",
				rc);
		}
	}

	if (of_property_read_bool(spmi->dev.of_node,
					"qcom,secondary-pon-reset")) {
		if (sys_reset) {
			dev_err(&spmi->dev, "qcom,system-reset property shouldn't be used along with qcom,secondary-pon-reset property\n");
			return -EINVAL;
		}
		spin_lock_irqsave(&spon_list_slock, flags);
		list_add(&pon->list, &spon_dev_list);
		spin_unlock_irqrestore(&spon_list_slock, flags);
		pon->is_spon = true;
	} else {
		boot_reason = ffs(pon_sts);
	}

	/* config whether store the hard reset reason */
	pon->store_hard_reset_reason = of_property_read_bool(
					spmi->dev.of_node,
					"qcom,store-hard-reset-reason");

	qpnp_pon_debugfs_init(spmi);
	return 0;
}

static int qpnp_pon_remove(struct spmi_device *spmi)
{
	struct qpnp_pon *pon = dev_get_drvdata(&spmi->dev);
	unsigned long flags;

	device_remove_file(&spmi->dev, &dev_attr_debounce_us);

	cancel_delayed_work_sync(&pon->bark_work);

	if (pon->pon_input)
		input_unregister_device(pon->pon_input);
	qpnp_pon_debugfs_remove(spmi);
	if (pon->is_spon) {
		spin_lock_irqsave(&spon_list_slock, flags);
		list_del(&pon->list);
		spin_unlock_irqrestore(&spon_list_slock, flags);
	}
	return 0;
}

static struct of_device_id spmi_match_table[] = {
	{ .compatible = "qcom,qpnp-power-on", },
	{}
};

static struct spmi_driver qpnp_pon_driver = {
	.driver		= {
		.name	= "qcom,qpnp-power-on",
		.of_match_table = spmi_match_table,
	},
	.probe		= qpnp_pon_probe,
	.remove		= qpnp_pon_remove,
};

static int __init qpnp_pon_init(void)
{
	return spmi_driver_register(&qpnp_pon_driver);
}
subsys_initcall(qpnp_pon_init);

static void __exit qpnp_pon_exit(void)
{
	return spmi_driver_unregister(&qpnp_pon_driver);
}
module_exit(qpnp_pon_exit);

MODULE_DESCRIPTION("QPNP PMIC POWER-ON driver");
MODULE_LICENSE("GPL v2");
