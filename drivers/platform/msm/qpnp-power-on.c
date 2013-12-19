/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/log2.h>
#include <linux/qpnp/power-on.h>

#define PMIC_VER_8941           0x01
#define PMIC_VERSION_REG        0x0105
#define PMIC_VERSION_REV4_REG   0x0103

#define PMIC8941_V1_REV4        0x01
#define PMIC8941_V2_REV4        0x02

/* Common PNP defines */
#define QPNP_PON_REVISION2(base)		(base + 0x01)

/* PON common register addresses */
#define QPNP_PON_RT_STS(base)			(base + 0x10)
#define QPNP_PON_PULL_CTL(base)			(base + 0x70)
#define QPNP_PON_DBC_CTL(base)			(base + 0x71)

/* PON/RESET sources register addresses */
#define QPNP_PON_REASON1(base)			(base + 0x8)
#define QPNP_PON_WARM_RESET_REASON1(base)	(base + 0xA)
#define QPNP_PON_WARM_RESET_REASON2(base)	(base + 0xB)
#define QPNP_PON_KPDPWR_S1_TIMER(base)		(base + 0x40)
#define QPNP_PON_KPDPWR_S2_TIMER(base)		(base + 0x41)
#define QPNP_PON_KPDPWR_S2_CNTL(base)		(base + 0x42)
#define QPNP_PON_KPDPWR_S2_CNTL2(base)		(base + 0x43)
#define QPNP_PON_RESIN_S1_TIMER(base)		(base + 0x44)
#define QPNP_PON_RESIN_S2_TIMER(base)		(base + 0x45)
#define QPNP_PON_RESIN_S2_CNTL(base)		(base + 0x46)
#define QPNP_PON_RESIN_S2_CNTL2(base)		(base + 0x47)
#define QPNP_PON_KPDPWR_RESIN_S1_TIMER(base)	(base + 0x48)
#define QPNP_PON_KPDPWR_RESIN_S2_TIMER(base)	(base + 0x49)
#define QPNP_PON_KPDPWR_RESIN_S2_CNTL(base)	(base + 0x4A)
#define QPNP_PON_KPDPWR_RESIN_S2_CNTL2(base)	(base + 0x4B)
#define QPNP_PON_PS_HOLD_RST_CTL(base)		(base + 0x5A)
#define QPNP_PON_PS_HOLD_RST_CTL2(base)		(base + 0x5B)
#define QPNP_PON_WD_RST_S2_CTL(base)		(base + 0x56)
#define QPNP_PON_WD_RST_S2_CTL2(base)		(base + 0x57)
#define QPNP_PON_S3_SRC(base)			(base + 0x74)
#define QPNP_PON_S3_DBC_CTL(base)		(base + 0x75)
#define QPNP_PON_TRIGGER_EN(base)		(base + 0x80)

#define QPNP_PON_S3_SRC_KPDPWR			0
#define QPNP_PON_S3_SRC_RESIN			1
#define QPNP_PON_S3_SRC_KPDPWR_OR_RESIN		2
#define QPNP_PON_S3_SRC_KPDPWR_AND_RESIN	3
#define QPNP_PON_S3_SRC_MASK			0x3

#define QPNP_PON_WARM_RESET_TFT			BIT(4)

#define QPNP_PON_RESIN_PULL_UP			BIT(0)
#define QPNP_PON_KPDPWR_PULL_UP			BIT(1)
#define QPNP_PON_CBLPWR_PULL_UP			BIT(2)
#define QPNP_PON_S2_CNTL_EN			BIT(7)
#define QPNP_PON_S2_RESET_ENABLE		BIT(7)
#define QPNP_PON_DELAY_BIT_SHIFT		6

#define QPNP_PON_S1_TIMER_MASK			(0xF)
#define QPNP_PON_S2_TIMER_MASK			(0x7)
#define QPNP_PON_S2_CNTL_TYPE_MASK		(0xF)

#define QPNP_PON_DBC_DELAY_MASK			(0x7)
#define QPNP_PON_KPDPWR_N_SET			BIT(0)
#define QPNP_PON_RESIN_N_SET			BIT(1)
#define QPNP_PON_CBLPWR_N_SET			BIT(2)
#define QPNP_PON_RESIN_BARK_N_SET		BIT(4)
#define QPNP_PON_KPDPWR_RESIN_BARK_N_SET	BIT(5)

#define QPNP_PON_WD_EN			BIT(7)
#define QPNP_PON_RESET_EN			BIT(7)
#define QPNP_PON_POWER_OFF_MASK			0xF

/* Ranges */
#define QPNP_PON_S1_TIMER_MAX			10256
#define QPNP_PON_S2_TIMER_MAX			2000
#define QPNP_PON_S3_TIMER_SECS_MAX		128
#define QPNP_PON_S3_DBC_DELAY_MASK		0x07
#define QPNP_PON_RESET_TYPE_MAX			0xF
#define PON_S1_COUNT_MAX			0xF
#define PON_REASON_MAX				8

#define QPNP_KEY_STATUS_DELAY			msecs_to_jiffies(250)
#define QPNP_PON_REV_B				0x01

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
	bool use_bark;
};

struct qpnp_pon {
	struct spmi_device *spmi;
	struct input_dev *pon_input;
	struct qpnp_pon_config *pon_cfg;
	int num_pon_config;
	u16 base;
	struct delayed_work bark_work;
};

static struct qpnp_pon *sys_reset_dev;

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

static int
qpnp_pon_masked_write(struct qpnp_pon *pon, u16 addr, u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
							addr, &reg, 1);
	if (rc) {
		dev_err(&pon->spmi->dev,
			"Unable to read from addr=%x, rc(%d)\n", addr, rc);
		return rc;
	}

	reg &= ~mask;
	reg |= val & mask;
	rc = spmi_ext_register_writel(pon->spmi->ctrl, pon->spmi->sid,
							addr, &reg, 1);
	if (rc)
		dev_err(&pon->spmi->dev,
			"Unable to write to addr=%x, rc(%d)\n", addr, rc);
	return rc;
}

/**
 * qpnp_pon_system_pwr_off - Configure system-reset PMIC for shutdown or reset
 * @type: Determines the type of power off to perform - shutdown, reset, etc
 *
 * This function will only configure a single PMIC. The other PMICs in the
 * system are slaved off of it and require no explicit configuration. Once
 * the system-reset PMIC is configured properly, the MSM can drop PS_HOLD to
 * activate the specified configuration.
 */
int qpnp_pon_system_pwr_off(enum pon_power_off_type type)
{
	int rc;
	u8 reg;
	u16 rst_en_reg;
	struct qpnp_pon *pon = sys_reset_dev;

	if (!pon)
		return -ENODEV;

	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
			QPNP_PON_REVISION2(pon->base), &reg, 1);
	if (rc) {
		dev_err(&pon->spmi->dev,
			"Unable to read addr=%x, rc(%d)\n",
			QPNP_PON_REVISION2(pon->base), rc);
		return rc;
	}

	if (reg == 0x00)
		rst_en_reg = QPNP_PON_PS_HOLD_RST_CTL(pon->base);
	else
		rst_en_reg = QPNP_PON_PS_HOLD_RST_CTL2(pon->base);

	rc = qpnp_pon_masked_write(pon, rst_en_reg, QPNP_PON_RESET_EN, 0);
	if (rc)
		dev_err(&pon->spmi->dev,
			"Unable to write to addr=%x, rc(%d)\n", rst_en_reg, rc);

	/*
	 * We need 10 sleep clock cycles here. But since the clock is
	 * internally generated, we need to add 50% tolerance to be
	 * conservative.
	 */
	udelay(500);

	rc = qpnp_pon_masked_write(pon, QPNP_PON_PS_HOLD_RST_CTL(pon->base),
				   QPNP_PON_POWER_OFF_MASK, type);
	if (rc)
		dev_err(&pon->spmi->dev,
			"Unable to write to addr=%x, rc(%d)\n",
				QPNP_PON_PS_HOLD_RST_CTL(pon->base), rc);

	rc = qpnp_pon_masked_write(pon, rst_en_reg, QPNP_PON_RESET_EN,
						    QPNP_PON_RESET_EN);
	if (rc)
		dev_err(&pon->spmi->dev,
			"Unable to write to addr=%x, rc(%d)\n", rst_en_reg, rc);

	dev_dbg(&pon->spmi->dev, "power off type = 0x%02X\n", type);

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
	int rc;
	u8 reg;

	if (!pon)
		return -EPROBE_DEFER;

	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
			QPNP_PON_WARM_RESET_REASON1(pon->base), &reg, 1);
	if (rc) {
		dev_err(&pon->spmi->dev,
			"Unable to read addr=%x, rc(%d)\n",
			QPNP_PON_WARM_RESET_REASON1(pon->base), rc);
		return rc;
	}

	if (reg)
		return 1;

	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
			QPNP_PON_WARM_RESET_REASON2(pon->base), &reg, 1);
	if (rc) {
		dev_err(&pon->spmi->dev,
			"Unable to read addr=%x, rc(%d)\n",
			QPNP_PON_WARM_RESET_REASON2(pon->base), rc);
		return rc;
	}
	if (reg & QPNP_PON_WARM_RESET_TFT)
		return 1;

	return 0;
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

	rc = qpnp_pon_masked_write(pon, QPNP_PON_WD_RST_S2_CTL2(pon->base),
			QPNP_PON_WD_EN, enable ? QPNP_PON_WD_EN : 0);
	if (rc)
		dev_err(&pon->spmi->dev,
				"Unable to write to addr=%x, rc(%d)\n",
				QPNP_PON_WD_RST_S2_CTL2(pon->base), rc);

	return rc;
}
EXPORT_SYMBOL(qpnp_pon_wd_config);


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

	rc = qpnp_pon_masked_write(pon, QPNP_PON_TRIGGER_EN(pon->base),
				BIT(pon_src), enable ? BIT(pon_src) : 0);
	if (rc)
		dev_err(&pon->spmi->dev, "Unable to write to addr=%x, rc(%d)\n",
					QPNP_PON_TRIGGER_EN(pon->base), rc);

	return rc;
}
EXPORT_SYMBOL(qpnp_pon_trigger_config);

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

static int
qpnp_pon_input_dispatch(struct qpnp_pon *pon, u32 pon_type)
{
	int rc;
	struct qpnp_pon_config *cfg = NULL;
	u8 pon_rt_sts = 0, pon_rt_bit = 0;

	cfg = qpnp_get_cfg(pon, pon_type);
	if (!cfg)
		return -EINVAL;

	/* Check if key reporting is supported */
	if (!cfg->key_code)
		return 0;

	/* check the RT status to get the current status of the line */
	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_RT_STS(pon->base), &pon_rt_sts, 1);
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

	input_report_key(pon->pon_input, cfg->key_code,
					(pon_rt_sts & pon_rt_bit));
	input_sync(pon->pon_input);

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
				QPNP_PON_RT_STS(pon->base), &pon_rt_sts, 1);
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

static int __devinit
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

	rc = qpnp_pon_masked_write(pon, QPNP_PON_PULL_CTL(pon->base),
				pull_bit, cfg->pull_up ? pull_bit : 0);
	if (rc)
		dev_err(&pon->spmi->dev, "Unable to config pull-up\n");

	return rc;
}

static int __devinit
qpnp_config_reset(struct qpnp_pon *pon, struct qpnp_pon_config *cfg)
{
	int rc;
	u8 i;
	u16 s1_timer_addr, s2_timer_addr;

	switch (cfg->pon_type) {
	case PON_KPDPWR:
		s1_timer_addr = QPNP_PON_KPDPWR_S1_TIMER(pon->base);
		s2_timer_addr = QPNP_PON_KPDPWR_S2_TIMER(pon->base);
		break;
	case PON_RESIN:
		s1_timer_addr = QPNP_PON_RESIN_S1_TIMER(pon->base);
		s2_timer_addr = QPNP_PON_RESIN_S2_TIMER(pon->base);
		break;
	case PON_KPDPWR_RESIN:
		s1_timer_addr = QPNP_PON_KPDPWR_RESIN_S1_TIMER(pon->base);
		s2_timer_addr = QPNP_PON_KPDPWR_RESIN_S2_TIMER(pon->base);
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

	usleep(100);

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

static int __devinit
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

static int __devinit
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

static int __devinit qpnp_pon_config_init(struct qpnp_pon *pon)
{
	int rc = 0, i = 0;
	struct device_node *pp = NULL;
	struct qpnp_pon_config *cfg;
	u8 pon_ver;
	u8 pmic_type;
	u8 revid_rev4;

	/* Check if it is rev B */
	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
			QPNP_PON_REVISION2(pon->base), &pon_ver, 1);
	if (rc) {
		dev_err(&pon->spmi->dev,
			"Unable to read addr=%x, rc(%d)\n",
			QPNP_PON_REVISION2(pon->base), rc);
		return rc;
	}

	/* iterate through the list of pon configs */
	while ((pp = of_get_next_child(pon->spmi->dev.of_node, pp))) {

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
			if (rc && rc != -EINVAL) {
				dev_err(&pon->spmi->dev,
					"Unable to read 'support-reset'\n");
				return rc;
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

			if (pon_ver == QPNP_PON_REV_B) {
				cfg->s2_cntl_addr =
					QPNP_PON_KPDPWR_S2_CNTL(pon->base);
				cfg->s2_cntl2_addr =
					QPNP_PON_KPDPWR_S2_CNTL2(pon->base);
			} else {
				cfg->s2_cntl_addr = cfg->s2_cntl2_addr =
					QPNP_PON_KPDPWR_S2_CNTL(pon->base);
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
			if (rc && rc != -EINVAL) {
				dev_err(&pon->spmi->dev,
					"Unable to read 'support-reset'\n");
				return rc;
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

			if (pon_ver == QPNP_PON_REV_B) {
				cfg->s2_cntl_addr =
					QPNP_PON_RESIN_S2_CNTL(pon->base);
				cfg->s2_cntl2_addr =
					QPNP_PON_RESIN_S2_CNTL2(pon->base);
			} else {
				cfg->s2_cntl_addr = cfg->s2_cntl2_addr =
					QPNP_PON_RESIN_S2_CNTL(pon->base);
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
			if (rc && rc != -EINVAL) {
				dev_err(&pon->spmi->dev,
					"Unable to read 'support-reset'\n");
				return rc;
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

			if (pon_ver == QPNP_PON_REV_B) {
				cfg->s2_cntl_addr =
				QPNP_PON_KPDPWR_RESIN_S2_CNTL(pon->base);
				cfg->s2_cntl2_addr =
				QPNP_PON_KPDPWR_RESIN_S2_CNTL2(pon->base);
			} else {
				cfg->s2_cntl_addr = cfg->s2_cntl2_addr =
				QPNP_PON_KPDPWR_RESIN_S2_CNTL(pon->base);
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
		/* Configure the reset-configuration */
		if (cfg->support_reset) {
			rc = qpnp_config_reset(pon, cfg);
			if (rc) {
				dev_err(&pon->spmi->dev,
					"Unable to config pon reset\n");
				goto unreg_input_dev;
			}
		} else {
			/* disable S2 reset */
			rc = qpnp_pon_masked_write(pon, cfg->s2_cntl2_addr,
						QPNP_PON_S2_CNTL_EN, 0);
			if (rc) {
				dev_err(&pon->spmi->dev,
					"Unable to disable S2 reset\n");
				goto unreg_input_dev;
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

static int __devinit qpnp_pon_probe(struct spmi_device *spmi)
{
	struct qpnp_pon *pon;
	struct resource *pon_resource;
	struct device_node *itr = NULL;
	u32 delay = 0, s3_debounce = 0;
	int rc, sys_reset, index;
	u8 pon_sts = 0;
	const char *s3_src;
	u8 s3_src_reg;

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

	/* get the total number of pon configurations */
	while ((itr = of_get_next_child(spmi->dev.of_node, itr)))
		pon->num_pon_config++;

	if (!pon->num_pon_config) {
		/* No PON config., do not register the driver */
		dev_err(&spmi->dev, "No PON config. specified\n");
		return -EINVAL;
	}

	pon->pon_cfg = devm_kzalloc(&spmi->dev,
			sizeof(struct qpnp_pon_config) * pon->num_pon_config,
								GFP_KERNEL);

	pon_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
	if (!pon_resource) {
		dev_err(&spmi->dev, "Unable to get PON base address\n");
		return -ENXIO;
	}
	pon->base = pon_resource->start;

	/* PON reason */
	rc = spmi_ext_register_readl(pon->spmi->ctrl, pon->spmi->sid,
				QPNP_PON_REASON1(pon->base), &pon_sts, 1);
	if (rc) {
		dev_err(&pon->spmi->dev, "Unable to read PON_RESASON1 reg\n");
		return rc;
	}

	boot_reason = ffs(pon_sts);
	index = ffs(pon_sts);
	if ((index > PON_REASON_MAX) || (index < 0))
		index = 0;

	cold_boot = !qpnp_pon_is_warm_reset();
	pr_info("PMIC@SID%d Power-on reason: %s and '%s' boot\n",
		pon->spmi->sid, index ? qpnp_pon_reason[index - 1] :
		"Unknown", cold_boot ? "cold" : "warm");

	rc = of_property_read_u32(pon->spmi->dev.of_node,
				"qcom,pon-dbc-delay", &delay);
	if (rc) {
		if (rc != -EINVAL) {
			dev_err(&spmi->dev, "Unable to read debounce delay\n");
			return rc;
		}
	} else {
		delay = (delay << QPNP_PON_DELAY_BIT_SHIFT) / USEC_PER_SEC;
		delay = ilog2(delay);
		rc = qpnp_pon_masked_write(pon, QPNP_PON_DBC_CTL(pon->base),
						QPNP_PON_DBC_DELAY_MASK, delay);
		if (rc) {
			dev_err(&spmi->dev, "Unable to set PON debounce\n");
			return rc;
		}
	}

	/* program s3 debounce */
	rc = of_property_read_u32(pon->spmi->dev.of_node,
				"qcom,s3-debounce", &s3_debounce);
	if (rc) {
		if (rc != -EINVAL) {
			dev_err(&pon->spmi->dev, "Unable to read s3 timer\n");
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
		rc = qpnp_pon_masked_write(pon, QPNP_PON_S3_DBC_CTL(pon->base),
				QPNP_PON_S3_DBC_DELAY_MASK, s3_debounce);
		if (rc) {
			dev_err(&spmi->dev, "Unable to set S3 debounce\n");
			return rc;
		}
	}

	/* program s3 source */
	s3_src = "kpdpwr-and-resin";
	rc = of_property_read_string(pon->spmi->dev.of_node,
				"qcom,s3-src", &s3_src);
	if (rc && rc != -EINVAL) {
		dev_err(&pon->spmi->dev, "Unable to read s3 timer\n");
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

	rc = qpnp_pon_masked_write(pon, QPNP_PON_S3_SRC(pon->base),
			QPNP_PON_S3_SRC_MASK, s3_src_reg);
	if (rc) {
		dev_err(&spmi->dev,
			"Unable to program s3 source\n");
		return rc;
	}

	dev_set_drvdata(&spmi->dev, pon);

	INIT_DELAYED_WORK(&pon->bark_work, bark_work_func);

	/* register the PON configurations */
	rc = qpnp_pon_config_init(pon);
	if (rc) {
		dev_err(&spmi->dev,
			"Unable to intialize PON configurations\n");
		return rc;
	}

	return rc;
}

static int qpnp_pon_remove(struct spmi_device *spmi)
{
	struct qpnp_pon *pon = dev_get_drvdata(&spmi->dev);

	cancel_delayed_work_sync(&pon->bark_work);

	if (pon->pon_input)
		input_unregister_device(pon->pon_input);

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
