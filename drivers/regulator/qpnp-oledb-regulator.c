/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"OLEDB: %s: " fmt, __func__

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/qpnp-labibb-regulator.h>
#include <linux/qpnp/qpnp-pbs.h>
#include <linux/qpnp/qpnp-revid.h>

#define QPNP_OLEDB_REGULATOR_DRIVER_NAME	"qcom,qpnp-oledb-regulator"
#define OLEDB_VOUT_STEP_MV				100
#define OLEDB_VOUT_MIN_MV				5000
#define OLEDB_VOUT_MAX_MV				8100
#define OLEDB_VOUT_HW_DEFAULT				6400

#define OLEDB_MODULE_RDY				0x45
#define OLEDB_MODULE_RDY_BIT				BIT(7)

#define OLEDB_MODULE_ENABLE				0x46
#define OLEDB_MODULE_ENABLE_BIT				BIT(7)

#define OLEDB_EXT_PIN_CTL				0x47
#define OLEDB_EXT_PIN_CTL_BIT				BIT(7)

#define OLEDB_SWIRE_CONTROL				0x48
#define OLEDB_EN_SWIRE_VOUT_UPD_BIT			BIT(6)
#define OLEDB_EN_SWIRE_PD_UPD_BIT			BIT(7)

#define OLEDB_VOUT_PGM					0x49
#define OLEDB_VOUT_PGM_MASK				GENMASK(4, 0)

#define OLEDB_VOUT_DEFAULT				0x4A
#define OLEDB_VOUT_DEFAULT_MASK				GENMASK(4, 0)

#define OLEDB_PD_CTL					0x4B

#define OLEDB_ILIM_NFET					0x4E
#define OLEDB_ILIMIT_NFET_MASK				GENMASK(2, 0)

#define OLEDB_BIAS_GEN_WARMUP_DELAY			0x52
#define OLEDB_BIAS_GEN_WARMUP_DELAY_MASK		GENMASK(1, 0)

#define OLEDB_SHORT_PROTECT				0x59
#define OLEDB_ENABLE_SC_DETECTION_BIT			BIT(7)
#define OLEDB_DBNC_SHORT_DETECTION_MASK			GENMASK(1, 0)

#define OLEDB_FAST_PRECHARGE				0x5A
#define OLEDB_FAST_PRECHG_PPULSE_EN_BIT			BIT(7)
#define OLEDB_DBNC_PRECHARGE_MASK			GENMASK(5, 4)
#define OLEDB_DBNC_PRECHARGE_SHIFT			4
#define OLEDB_PRECHARGE_PULSE_PERIOD_MASK		GENMASK(3, 2)
#define OLEDB_PRECHARGE_PULSE_PERIOD_SHIFT		2
#define OLEDB_PRECHARGE_PULSE_TON_MASK			GENMASK(1, 0)

#define OLEDB_EN_PSM					0x5B
#define OLEDB_PSM_ENABLE_BIT				BIT(7)

#define OLEDB_PSM_CTL					0x5C
#define OLEDB_PSM_HYSTERYSIS_CTL_BIT			BIT(3)
#define OLEDB_PSM_HYSTERYSIS_CTL_BIT_SHIFT		3
#define OLEDB_VREF_PSM_MASK				GENMASK(2, 0)

#define OLEDB_PFM_CTL					0x5D
#define OLEDB_PFM_ENABLE_BIT				BIT(7)
#define OLEDB_PFM_HYSTERYSIS_CTRL_BIT_MASK		BIT(4)
#define OLEDB_PFM_HYSTERYSIS_CTL_BIT_SHIFT		4
#define OLEDB_PFM_CURR_LIMIT_MASK			GENMASK(3, 2)
#define OLEDB_PFM_CURR_LIMIT_SHIFT			2
#define OLEDB_PFM_OFF_TIME_NS_MASK			GENMASK(1, 0)

#define OLEDB_NLIMIT					0x64
#define OLEDB_ENABLE_NLIMIT_BIT				BIT(7)
#define OLEDB_ENABLE_NLIMIT_BIT_SHIFT			7
#define OLEDB_NLIMIT_PGM_MASK				GENMASK(1, 0)

#define OLEDB_SPARE_CTL					0xE9
#define OLEDB_FORCE_PD_CTL_SPARE_BIT			BIT(7)

#define OLEDB_PD_PBS_TRIGGER_BIT			BIT(0)

#define OLEDB_SEC_UNLOCK_CODE				0xA5
#define OLEDB_PSM_HYS_CTRL_MIN				13
#define OLEDB_PSM_HYS_CTRL_MAX				26

#define OLEDB_PFM_HYS_CTRL_MIN				13
#define OLEDB_PFM_HYS_CTRL_MAX				26

#define OLEDB_PFM_OFF_TIME_MIN				110
#define OLEDB_PFM_OFF_TIME_MAX				480

#define OLEDB_PRECHG_TIME_MIN				1
#define OLEDB_PRECHG_TIME_MAX				8

#define OLEDB_PRECHG_PULSE_PERIOD_MIN			3
#define OLEDB_PRECHG_PULSE_PERIOD_MAX			12

#define OLEDB_MIN_SC_DBNC_TIME_FSW			2
#define OLEDB_MAX_SC_DBNC_TIME_FSW			16

#define OLEDB_PRECHG_PULSE_ON_TIME_MIN			1200
#define OLEDB_PRECHG_PULSE_ON_TIME_MAX			3000

#define PSM_HYSTERYSIS_MV_TO_VAL(val_mv)		((val_mv/13) - 1)
#define PFM_HYSTERYSIS_MV_TO_VAL(val_mv)		((val_mv/13) - 1)
#define PFM_OFF_TIME_NS_TO_VAL(val_ns)			((val_ns/110) - 1)
#define PRECHG_DEBOUNCE_TIME_MS_TO_VAL(val_ms)		((val_ms/2) - \
								(val_ms/8))
#define PRECHG_PULSE_PERIOD_US_TO_VAL(val_us)		((val_us/3) - 1)
#define PRECHG_PULSE_ON_TIME_NS_TO_VAL(val_ns)		(val_ns/600 - 2)
#define SHORT_CIRCUIT_DEBOUNCE_TIME_TO_VAL(val)		((val/4) - (val/16))

struct qpnp_oledb_psm_ctl {
	int psm_enable;
	int   psm_hys_ctl;
	int  psm_vref;
};

struct qpnp_oledb_pfm_ctl {
	int pfm_enable;
	int pfm_hys_ctl;
	int pfm_curr_limit;
	int pfm_off_time;
};

struct qpnp_oledb_fast_precharge_ctl {
	int fast_prechg_ppulse_en;
	int prechg_debounce_time;
	int prechg_pulse_period;
	int prechg_pulse_on_time;
};

struct qpnp_oledb {
	struct platform_device			*pdev;
	struct device				*dev;
	struct regmap				*regmap;
	struct regulator_desc			rdesc;
	struct regulator_dev			*rdev;
	struct qpnp_oledb_psm_ctl		psm_ctl;
	struct qpnp_oledb_pfm_ctl		pfm_ctl;
	struct qpnp_oledb_fast_precharge_ctl	fast_prechg_ctl;
	struct notifier_block			oledb_nb;
	struct mutex				bus_lock;
	struct device_node			*pbs_dev_node;
	struct pmic_revid_data			*pmic_rev_id;

	u32					base;
	u8					mod_enable;
	u8					ext_pinctl_state;
	int					current_voltage;
	int					default_voltage;
	int					vout_mv;
	int					warmup_delay;
	int					peak_curr_limit;
	int					pd_ctl;
	int					negative_curr_limit;
	int					nlimit_enable;
	int					sc_en;
	int					sc_dbnc_time;
	bool					swire_control;
	bool					ext_pin_control;
	bool					dynamic_ext_pinctl_config;
	bool					pbs_control;
	bool					force_pd_control;
	bool					handle_lab_sc_notification;
	bool					lab_sc_detected;
};

static const u16 oledb_warmup_dly_ns[] = {6700, 13300, 26700, 53400};
static const u16 oledb_peak_curr_limit_ma[] = {115, 265, 415, 570,
					      720, 870, 1020, 1170};
static const u16 oledb_psm_vref_mv[] = {440, 510, 580, 650, 715,
					 780, 850, 920};
static const u16 oledb_pfm_curr_limit_ma[] = {130, 200, 270, 340};
static const u16 oledb_nlimit_ma[] = {170, 300, 420, 550};

static int qpnp_oledb_read(struct qpnp_oledb *oledb, u32 address,
					u8 *val, int count)
{
	int rc = 0;
	struct platform_device *pdev = oledb->pdev;

	mutex_lock(&oledb->bus_lock);
	rc = regmap_bulk_read(oledb->regmap, address, val, count);
	if (rc)
		pr_err("Failed to read address=0x%02x sid=0x%02x rc=%d\n",
			address, to_spmi_device(pdev->dev.parent)->usid, rc);

	mutex_unlock(&oledb->bus_lock);
	return rc;
}

static int qpnp_oledb_masked_write(struct qpnp_oledb *oledb,
						u32 address, u8 mask, u8 val)
{
	int rc;

	mutex_lock(&oledb->bus_lock);
	rc = regmap_update_bits(oledb->regmap, address, mask, val);
	if (rc < 0)
		pr_err("Failed to write address 0x%04X, rc = %d\n",
					address, rc);
	else
		pr_debug("Wrote 0x%02X to addr 0x%04X\n",
			val, address);

	mutex_unlock(&oledb->bus_lock);
	return rc;
}

#define OLEDB_SEC_ACCESS	0xD0
static int qpnp_oledb_sec_masked_write(struct qpnp_oledb *oledb, u16 address,
							 u8 mask, u8 val)
{
	int rc = 0;
	u8 sec_val = OLEDB_SEC_UNLOCK_CODE;
	u16 sec_reg_addr = (address & 0xFF00) | OLEDB_SEC_ACCESS;

	mutex_lock(&oledb->bus_lock);
	rc = regmap_write(oledb->regmap, sec_reg_addr, sec_val);
	if (rc < 0) {
		pr_err("register %x failed rc = %d\n", sec_reg_addr, rc);
		goto error;
	}

	rc = regmap_update_bits(oledb->regmap, address, mask, val);
	if (rc < 0)
		pr_err("spmi write failed: addr=%03X, rc=%d\n", address, rc);

error:
	mutex_unlock(&oledb->bus_lock);
	return rc;
}

static int qpnp_oledb_write(struct qpnp_oledb *oledb, u16 address, u8 *val,
				int count)
{
	int rc = 0;
	struct platform_device *pdev = oledb->pdev;

	mutex_lock(&oledb->bus_lock);
	rc = regmap_bulk_write(oledb->regmap, address, val, count);
	if (rc)
		pr_err("Failed to write address=0x%02x sid=0x%02x rc=%d\n",
			address, to_spmi_device(pdev->dev.parent)->usid, rc);
	else
		pr_debug("Wrote 0x%02X to addr 0x%04X\n",
			*val, address);

	mutex_unlock(&oledb->bus_lock);
	return rc;
}

static int qpnp_oledb_regulator_enable(struct regulator_dev *rdev)
{
	int rc = 0;
	u8 val = 0;

	struct qpnp_oledb *oledb  = rdev_get_drvdata(rdev);

	if (oledb->lab_sc_detected == true) {
		pr_info("Short circuit detected: Disabled OLEDB rail\n");
		return 0;
	}

	if (oledb->ext_pin_control) {
		rc = qpnp_oledb_read(oledb, oledb->base + OLEDB_EXT_PIN_CTL,
								 &val, 1);
		if (rc < 0) {
			pr_err("Failed to read EXT_PIN_CTL rc=%d\n", rc);
			return rc;
		}

		/*
		 * Enable ext-pin-ctl after display-supply is turned on.
		 * This is to avoid glitches on the external pin.
		 */
		if (!(val & OLEDB_EXT_PIN_CTL_BIT) &&
					oledb->dynamic_ext_pinctl_config) {
			val = OLEDB_EXT_PIN_CTL_BIT;
			rc = qpnp_oledb_write(oledb, oledb->base +
					OLEDB_EXT_PIN_CTL, &val, 1);
			if (rc < 0) {
				pr_err("Failed to write EXT_PIN_CTL rc=%d\n",
									rc);
				return rc;
			}
		}
		pr_debug("ext-pin-ctrl mode enabled\n");
	} else {
		val = OLEDB_MODULE_ENABLE_BIT;
		rc = qpnp_oledb_write(oledb, oledb->base + OLEDB_MODULE_ENABLE,
					&val, 1);
		if (rc < 0) {
			pr_err("Failed to write MODULE_ENABLE rc=%d\n", rc);
			return rc;
		}

		ndelay(oledb->warmup_delay);
		pr_debug("register-control mode, module enabled\n");
	}

	oledb->mod_enable = true;
	if (oledb->pbs_control) {
		rc = qpnp_oledb_masked_write(oledb, oledb->base +
			OLEDB_SWIRE_CONTROL, OLEDB_EN_SWIRE_PD_UPD_BIT |
					OLEDB_EN_SWIRE_VOUT_UPD_BIT, 0);
		if (rc < 0)
			pr_err("Failed to write SWIRE_CTL for pbs mode rc=%d\n",
									 rc);
	}

	return rc;
}

static int qpnp_oledb_regulator_disable(struct regulator_dev *rdev)
{
	int rc = 0;
	u8 trigger_bitmap = OLEDB_PD_PBS_TRIGGER_BIT;
	u8 val;

	struct qpnp_oledb *oledb  = rdev_get_drvdata(rdev);

	/*
	 * Disable ext-pin-ctl after display-supply is turned off. This is to
	 * avoid glitches on the external pin.
	 */
	if (oledb->ext_pin_control) {
		if (oledb->dynamic_ext_pinctl_config) {
			rc = qpnp_oledb_masked_write(oledb, oledb->base +
				 OLEDB_EXT_PIN_CTL, OLEDB_EXT_PIN_CTL_BIT, 0);
			if (rc < 0) {
				pr_err("Failed to write EXT_PIN_CTL rc=%d\n",
									rc);
				return rc;
			}
		}
		pr_debug("ext-pin-ctrl mode disabled\n");
	} else {
		rc = qpnp_oledb_masked_write(oledb, oledb->base +
					OLEDB_MODULE_ENABLE,
					OLEDB_MODULE_ENABLE_BIT, 0);
		if (rc < 0) {
			pr_err("Failed to write MODULE_ENABLE rc=%d\n", rc);
			return rc;
		}
		pr_debug("Register-control mode, module disabled\n");
	}

	if (oledb->force_pd_control) {
		rc = qpnp_oledb_read(oledb, oledb->base + OLEDB_SPARE_CTL,
						&val, 1);
		if (rc < 0) {
			pr_err("Failed to read OLEDB_SPARE_CTL rc=%d\n", rc);
			return rc;
		}

		if (val & OLEDB_FORCE_PD_CTL_SPARE_BIT) {
			rc = qpnp_oledb_sec_masked_write(oledb, oledb->base +
					OLEDB_SPARE_CTL,
					OLEDB_FORCE_PD_CTL_SPARE_BIT, 0);
			if (rc < 0) {
				pr_err("Failed to write SPARE_CTL rc=%d\n", rc);
				return rc;
			}

			rc = qpnp_pbs_trigger_event(oledb->pbs_dev_node,
							trigger_bitmap);
			if (rc < 0)
				pr_err("Failed to trigger the PBS sequence\n");

			pr_debug("PBS event triggered\n");
		} else {
			pr_debug("OLEDB_SPARE_CTL register bit not set\n");
		}
	}

	oledb->mod_enable = false;

	return rc;
}

static int qpnp_oledb_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct qpnp_oledb *oledb  = rdev_get_drvdata(rdev);

	return oledb->mod_enable;
}

static int qpnp_oledb_regulator_set_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV, unsigned int *selector)
{
	u8 val;
	int rc = 0;

	struct qpnp_oledb *oledb = rdev_get_drvdata(rdev);

	if (oledb->swire_control)
		return 0;

	val = DIV_ROUND_UP(min_uV - OLEDB_VOUT_MIN_MV, OLEDB_VOUT_STEP_MV);

	rc = qpnp_oledb_write(oledb, oledb->base + OLEDB_VOUT_PGM,
					&val, 1);
	if (rc < 0) {
		pr_err("Failed to write VOUT_PGM rc=%d\n", rc);
		return rc;
	}

	oledb->current_voltage = min_uV;
	pr_debug("register-control mode, current voltage %d\n",
						oledb->current_voltage);

	return 0;
}

static int qpnp_oledb_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct qpnp_oledb *oledb  = rdev_get_drvdata(rdev);

	if (oledb->swire_control)
		return 0;

	return oledb->current_voltage;
}

static struct regulator_ops qpnp_oledb_ops = {
	.enable			= qpnp_oledb_regulator_enable,
	.disable		= qpnp_oledb_regulator_disable,
	.is_enabled		= qpnp_oledb_regulator_is_enabled,
	.set_voltage		= qpnp_oledb_regulator_set_voltage,
	.get_voltage		= qpnp_oledb_regulator_get_voltage,
};

static int qpnp_oledb_register_regulator(struct qpnp_oledb *oledb)
{
	int rc = 0;
	struct platform_device *pdev = oledb->pdev;
	struct regulator_init_data *init_data;
	struct regulator_desc *rdesc = &oledb->rdesc;
	struct regulator_config cfg = {};

	init_data = of_get_regulator_init_data(&pdev->dev,
						pdev->dev.of_node, rdesc);
	if (!init_data) {
		pr_err("Unable to get OLEDB regulator init data\n");
		return -ENOMEM;
	}

	if (init_data->constraints.name) {
		rdesc->owner		= THIS_MODULE;
		rdesc->type		= REGULATOR_VOLTAGE;
		rdesc->ops		= &qpnp_oledb_ops;
		rdesc->name		= init_data->constraints.name;

		cfg.dev = &pdev->dev;
		cfg.init_data = init_data;
		cfg.driver_data = oledb;
		cfg.of_node = pdev->dev.of_node;

		if (of_get_property(pdev->dev.of_node, "parent-supply",
				 NULL))
			init_data->supply_regulator = "parent";

		init_data->constraints.valid_ops_mask
				|= REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS;

		oledb->rdev = devm_regulator_register(oledb->dev, rdesc, &cfg);
		if (IS_ERR(oledb->rdev)) {
			rc = PTR_ERR(oledb->rdev);
			oledb->rdev = NULL;
			pr_err("Unable to register OLEDB regulator, rc = %d\n",
				rc);
			return rc;
		}
	} else {
		pr_err("OLEDB regulator name missing\n");
		return -EINVAL;
	}

	return 0;
}

static int qpnp_oledb_get_curr_voltage(struct qpnp_oledb *oledb,
					u16 *current_voltage)
{
	int rc = 0;
	u8 val;

	if (!(oledb->mod_enable || oledb->ext_pinctl_state)) {
		rc = qpnp_oledb_read(oledb, oledb->base + OLEDB_VOUT_DEFAULT,
						&val, 1);
		if (rc < 0) {
			pr_err("Failed to read VOUT_DEFAULT rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = qpnp_oledb_read(oledb, oledb->base +
					OLEDB_VOUT_PGM, &val, 1);
		if (rc < 0) {
			pr_err("Failed to read VOUT_PGM rc=%d\n", rc);
			return rc;
		}
	}

	*current_voltage = (val * OLEDB_VOUT_STEP_MV) + OLEDB_VOUT_MIN_MV;

	return rc;
}

static int qpnp_oledb_init_nlimit(struct qpnp_oledb *oledb)
{
	int rc = 0, i = 0;
	u32 val, mask = 0;

	if (oledb->nlimit_enable != -EINVAL) {
		val = oledb->nlimit_enable <<
					OLEDB_ENABLE_NLIMIT_BIT_SHIFT;
		mask = OLEDB_ENABLE_NLIMIT_BIT;
		if (oledb->negative_curr_limit != -EINVAL) {
			for (i = 0; i < ARRAY_SIZE(oledb_nlimit_ma); i++) {
				if (oledb->negative_curr_limit ==
						oledb_nlimit_ma[i])
					break;
			}
			val |= i;
			mask |= OLEDB_NLIMIT_PGM_MASK;
		}
		rc = qpnp_oledb_masked_write(oledb, oledb->base +
				OLEDB_NLIMIT, mask, val);
		if (rc < 0)
			pr_err("Failed to write NLIMT rc=%d\n", rc);
	}

	return rc;
}

static int qpnp_oledb_init_psm(struct qpnp_oledb *oledb)
{
	int rc = 0, i = 0;
	u32 val = 0, mask = 0, temp = 0;
	struct qpnp_oledb_psm_ctl *psm_ctl = &oledb->psm_ctl;

	if (psm_ctl->psm_enable == -EINVAL)
		return rc;

	if (psm_ctl->psm_enable) {
		val = OLEDB_PSM_ENABLE_BIT;
		rc = qpnp_oledb_masked_write(oledb, oledb->base +
				OLEDB_EN_PSM, OLEDB_PSM_ENABLE_BIT, val);
		if (rc < 0) {
			pr_err("Failed to write PSM_EN rc=%d\n", rc);
			return rc;
		}

		val = 0;
		if (psm_ctl->psm_vref != -EINVAL) {
			for (i = 0; i < ARRAY_SIZE(oledb_psm_vref_mv); i++) {
				if (psm_ctl->psm_vref ==
						oledb_psm_vref_mv[i])
					break;
			}
			val = i;
			mask = OLEDB_VREF_PSM_MASK;
		}

		if (psm_ctl->psm_hys_ctl != -EINVAL) {
			temp = PSM_HYSTERYSIS_MV_TO_VAL(psm_ctl->psm_hys_ctl);
			val |= (temp << OLEDB_PSM_HYSTERYSIS_CTL_BIT_SHIFT);
			mask |= OLEDB_PSM_HYSTERYSIS_CTL_BIT;
		}
		if (val) {
			rc = qpnp_oledb_masked_write(oledb, oledb->base +
					OLEDB_PSM_CTL, mask, val);
			if (rc < 0)
				pr_err("Failed to write PSM_CTL rc=%d\n", rc);
		}
	} else {
		rc = qpnp_oledb_masked_write(oledb, oledb->base +
				OLEDB_EN_PSM, OLEDB_PSM_ENABLE_BIT, 0);
		if (rc < 0)
			pr_err("Failed to write PSM_CTL rc=%d\n", rc);
	}

	return rc;
}

static int qpnp_oledb_init_pfm(struct qpnp_oledb *oledb)
{
	int rc = 0, i = 0;
	u32 val = 0, temp = 0, mask = 0;
	struct qpnp_oledb_pfm_ctl *pfm_ctl = &oledb->pfm_ctl;

	if (pfm_ctl->pfm_enable == -EINVAL)
		return rc;

	if (pfm_ctl->pfm_enable) {
		mask = val = OLEDB_PFM_ENABLE_BIT;
		if (pfm_ctl->pfm_hys_ctl != -EINVAL) {
			temp = PFM_HYSTERYSIS_MV_TO_VAL(pfm_ctl->pfm_hys_ctl);
			val |= temp <<
				OLEDB_PFM_HYSTERYSIS_CTL_BIT_SHIFT;
			mask |= OLEDB_PFM_HYSTERYSIS_CTRL_BIT_MASK;
		}

		if (pfm_ctl->pfm_curr_limit != -EINVAL) {
			for (i = 0; i < ARRAY_SIZE(oledb_pfm_curr_limit_ma);
									i++) {
				if (pfm_ctl->pfm_curr_limit ==
						oledb_pfm_curr_limit_ma[i])
					break;
			}
			val |= (i << OLEDB_PFM_CURR_LIMIT_SHIFT);
			mask |= OLEDB_PFM_CURR_LIMIT_MASK;
		}

		if (pfm_ctl->pfm_off_time != -EINVAL) {
			val |= PFM_OFF_TIME_NS_TO_VAL(pfm_ctl->pfm_off_time);
			mask |= OLEDB_PFM_OFF_TIME_NS_MASK;
		}

		rc = qpnp_oledb_masked_write(oledb, oledb->base +
				OLEDB_PFM_CTL, mask, val);
		if (rc < 0)
			pr_err("Failed to write PFM_CTL rc=%d\n", rc);
	} else {
		rc = qpnp_oledb_masked_write(oledb, oledb->base +
				OLEDB_PFM_CTL, OLEDB_PFM_ENABLE_BIT, 0);
		if (rc < 0)
			pr_err("Failed to write PFM_CTL rc=%d\n", rc);
	}

	return rc;
}

static int qpnp_oledb_init_fast_precharge(struct qpnp_oledb *oledb)
{
	int rc = 0;
	u32 val = 0, temp = 0, mask = 0;
	struct qpnp_oledb_fast_precharge_ctl *prechg_ctl =
					&oledb->fast_prechg_ctl;

	if (prechg_ctl->fast_prechg_ppulse_en == -EINVAL)
		return rc;

	if (prechg_ctl->fast_prechg_ppulse_en) {
		mask = val = OLEDB_FAST_PRECHG_PPULSE_EN_BIT;
		if (prechg_ctl->prechg_debounce_time != -EINVAL) {
			temp = PRECHG_DEBOUNCE_TIME_MS_TO_VAL(
					prechg_ctl->prechg_debounce_time);
			val |= temp << OLEDB_DBNC_PRECHARGE_SHIFT;
			mask |= OLEDB_DBNC_PRECHARGE_MASK;
		}

		if (prechg_ctl->prechg_pulse_period != -EINVAL) {
			temp = PRECHG_PULSE_PERIOD_US_TO_VAL(
					prechg_ctl->prechg_pulse_period);
			val |= temp << OLEDB_PRECHARGE_PULSE_PERIOD_SHIFT;
			mask |= OLEDB_PRECHARGE_PULSE_PERIOD_MASK;
		}

		if (prechg_ctl->prechg_pulse_on_time != -EINVAL) {
			val |= PRECHG_PULSE_ON_TIME_NS_TO_VAL(
					prechg_ctl->prechg_pulse_on_time);
			mask |= OLEDB_PRECHARGE_PULSE_TON_MASK;
		}

		rc = qpnp_oledb_masked_write(oledb, oledb->base +
				OLEDB_FAST_PRECHARGE, mask, val);
		if (rc < 0)
			pr_err("Failed to write FAST_PRECHARGE rc=%d\n", rc);
	} else {
		rc = qpnp_oledb_masked_write(oledb, oledb->base +
				OLEDB_FAST_PRECHARGE,
				OLEDB_FAST_PRECHG_PPULSE_EN_BIT, 0);
		if (rc < 0)
			pr_err("Failed to write FAST_PRECHARGE rc=%d\n", rc);
	}

	return rc;
}

static int qpnp_oledb_hw_init(struct qpnp_oledb *oledb)
{
	int rc, i = 0;
	u8 val = 0, mask = 0;
	u16 current_voltage;

	if (oledb->default_voltage != -EINVAL) {
		val = (oledb->default_voltage - OLEDB_VOUT_MIN_MV) /
					 OLEDB_VOUT_STEP_MV;
		rc = qpnp_oledb_write(oledb, oledb->base +
					OLEDB_VOUT_DEFAULT, &val, 1);
		if (rc < 0) {
			pr_err("Failed to write VOUT_DEFAULT rc=%d\n", rc);
			return rc;
		}
	}

	rc = qpnp_oledb_read(oledb, oledb->base + OLEDB_MODULE_ENABLE,
				&oledb->mod_enable, 1);
	if (rc < 0) {
		pr_err("Failed to read MODULE_ENABLE rc=%d\n", rc);
		return rc;
	}

	rc = qpnp_oledb_read(oledb, oledb->base + OLEDB_EXT_PIN_CTL,
					&oledb->ext_pinctl_state, 1);
	if (rc < 0) {
		pr_err("Failed to read EXT_PIN_CTL rc=%d\n", rc);
		return rc;
	}

	rc = qpnp_oledb_get_curr_voltage(oledb, &current_voltage);
	if (rc < 0)
		return rc;

	/*
	 * Go through if the module is not enabled either through
	 * external pin control or SPMI interface.
	 */
	if (!((oledb->ext_pinctl_state & OLEDB_EXT_PIN_CTL_BIT)
				|| oledb->mod_enable)) {
		if (oledb->warmup_delay != -EINVAL) {
			for (i = 0; i < ARRAY_SIZE(oledb_warmup_dly_ns); i++) {
				if (oledb->warmup_delay ==
							oledb_warmup_dly_ns[i])
					break;
			}
			val = i;
			rc = qpnp_oledb_masked_write(oledb,
				oledb->base + OLEDB_BIAS_GEN_WARMUP_DELAY,
				OLEDB_BIAS_GEN_WARMUP_DELAY_MASK, val);
			if (rc < 0) {
				pr_err("Failed to write WARMUP_DELAY rc=%d\n",
									rc);
				return rc;
			}
		} else {
			rc = qpnp_oledb_read(oledb, oledb->base +
					 OLEDB_BIAS_GEN_WARMUP_DELAY,
					&val, 1);
			if (rc < 0) {
				pr_err("Failed to read WARMUP_DELAY rc=%d\n",
									rc);
				return rc;
			}
			oledb->warmup_delay = oledb_warmup_dly_ns[val];
		}

		if (oledb->peak_curr_limit != -EINVAL) {
			for (i = 0; i < ARRAY_SIZE(oledb_peak_curr_limit_ma);
									i++) {
				if (oledb->peak_curr_limit ==
						oledb_peak_curr_limit_ma[i])
					break;
			}
			val = i;
			rc = qpnp_oledb_write(oledb,
					oledb->base + OLEDB_ILIM_NFET,
					&val, 1);
			if (rc < 0) {
				pr_err("Failed to write ILIM_NEFT rc=%d\n", rc);
				return rc;
			}
		}

		if (oledb->pd_ctl != -EINVAL) {
			val = oledb->pd_ctl;
			rc = qpnp_oledb_write(oledb, oledb->base +
					OLEDB_PD_CTL, &val, 1);
			if (rc < 0) {
				pr_err("Failed to write PD_CTL rc=%d\n", rc);
				return rc;
			}
		}

		if (oledb->sc_en != -EINVAL) {
			val = oledb->sc_en ? OLEDB_ENABLE_SC_DETECTION_BIT : 0;
			mask = OLEDB_ENABLE_SC_DETECTION_BIT;
			if (oledb->sc_dbnc_time != -EINVAL) {
				val |= SHORT_CIRCUIT_DEBOUNCE_TIME_TO_VAL(
							oledb->sc_dbnc_time);
				mask |= OLEDB_DBNC_PRECHARGE_MASK;
			}

			rc = qpnp_oledb_write(oledb, oledb->base +
					OLEDB_SHORT_PROTECT, &val, 1);
			if (rc < 0) {
				pr_err("Failed to write SHORT_PROTECT rc=%d\n",
									rc);
				return rc;
			}
		}

		rc = qpnp_oledb_init_nlimit(oledb);
		if (rc < 0)
			return rc;

		rc = qpnp_oledb_init_psm(oledb);
		if (rc < 0)
			return rc;

		rc = qpnp_oledb_init_pfm(oledb);
		if (rc < 0)
			return rc;

		rc = qpnp_oledb_init_fast_precharge(oledb);
		if (rc < 0)
			return rc;

		if (oledb->swire_control) {
			val = OLEDB_EN_SWIRE_PD_UPD_BIT |
				OLEDB_EN_SWIRE_VOUT_UPD_BIT;
			rc = qpnp_oledb_masked_write(oledb, oledb->base +
				OLEDB_SWIRE_CONTROL, OLEDB_EN_SWIRE_PD_UPD_BIT |
				OLEDB_EN_SWIRE_VOUT_UPD_BIT, val);
			if (rc < 0)
				return rc;
		}

		rc = qpnp_oledb_read(oledb, oledb->base + OLEDB_MODULE_RDY,
								&val, 1);
		if (rc < 0) {
			pr_err("Failed to read MODULE_RDY rc=%d\n", rc);
			return rc;
		}

		if (!(val & OLEDB_MODULE_RDY_BIT)) {
			val = OLEDB_MODULE_RDY_BIT;
			rc = qpnp_oledb_write(oledb, oledb->base +
				OLEDB_MODULE_RDY, &val, 1);
			if (rc < 0) {
				pr_err("Failed to write MODULE_RDY rc=%d\n",
									rc);
				return rc;
			}
		}

		if (!oledb->dynamic_ext_pinctl_config) {
			if (oledb->ext_pin_control) {
				val = OLEDB_EXT_PIN_CTL_BIT;
				rc = qpnp_oledb_write(oledb, oledb->base +
						OLEDB_EXT_PIN_CTL, &val, 1);
				if (rc < 0) {
					pr_err("Failed to write EXT_PIN_CTL rc=%d\n",
									rc);
					return rc;
				}
			} else {
				val = OLEDB_MODULE_ENABLE_BIT;
				rc = qpnp_oledb_write(oledb, oledb->base +
						 OLEDB_MODULE_ENABLE, &val, 1);
				if (rc < 0) {
					pr_err("Failed to write MODULE_ENABLE rc=%d\n",
									rc);
					return rc;
				}

				ndelay(oledb->warmup_delay);
			}

			oledb->mod_enable = true;
			if (oledb->pbs_control) {
				rc = qpnp_oledb_masked_write(oledb,
				oledb->base + OLEDB_SWIRE_CONTROL,
				OLEDB_EN_SWIRE_PD_UPD_BIT |
				OLEDB_EN_SWIRE_VOUT_UPD_BIT, 0);
				if (rc < 0) {
					pr_err("Failed to write SWIRE_CTL rc=%d\n",
									rc);
					return rc;
				}
			}
		}

		oledb->current_voltage = current_voltage;
	} else {
		 /* module is enabled */
		if (oledb->current_voltage == -EINVAL) {
			oledb->current_voltage = current_voltage;
		} else if (!oledb->swire_control) {
			if (oledb->current_voltage < OLEDB_VOUT_MIN_MV) {
				pr_err("current_voltage %d is less than min_volt %d\n",
				    oledb->current_voltage, OLEDB_VOUT_MIN_MV);
				return -EINVAL;
			}
			val = DIV_ROUND_UP(oledb->current_voltage -
					OLEDB_VOUT_MIN_MV, OLEDB_VOUT_STEP_MV);
			rc = qpnp_oledb_write(oledb, oledb->base +
						OLEDB_VOUT_PGM, &val, 1);
			if (rc < 0) {
				pr_err("Failed to write VOUT_PGM rc=%d\n",
									rc);
				return rc;
			}
		}

		oledb->mod_enable = true;
	}

	return rc;
}

static int qpnp_oledb_parse_nlimit(struct qpnp_oledb *oledb)
{
	int rc = 0;
	struct device_node *of_node = oledb->dev->of_node;

	oledb->nlimit_enable = -EINVAL;
	rc = of_property_read_u32(of_node, "qcom,negative-curr-limit-enable",
					&oledb->nlimit_enable);
	if (!rc) {
		oledb->negative_curr_limit = -EINVAL;
		rc = of_property_read_u32(of_node,
					 "qcom,negative-curr-limit-ma",
					 &oledb->negative_curr_limit);
		if (!rc) {
			u16 min_curr_limit = oledb_nlimit_ma[0];
			u16 max_curr_limit = oledb_nlimit_ma[ARRAY_SIZE(
						oledb_nlimit_ma) - 1];
			if (oledb->negative_curr_limit < min_curr_limit ||
				oledb->negative_curr_limit > max_curr_limit) {
				pr_err("Invalid value in qcom,negative-curr-limit-ma\n");
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int qpnp_oledb_parse_psm(struct qpnp_oledb *oledb)
{
	int rc = 0;
	struct qpnp_oledb_psm_ctl *psm_ctl = &oledb->psm_ctl;
	struct device_node *of_node = oledb->dev->of_node;

	psm_ctl->psm_enable = -EINVAL;
	rc = of_property_read_u32(of_node, "qcom,psm-enable",
					&psm_ctl->psm_enable);
	if (!rc) {
		psm_ctl->psm_hys_ctl = -EINVAL;
		rc = of_property_read_u32(of_node, "qcom,psm-hys-mv",
						&psm_ctl->psm_hys_ctl);
		if (!rc) {
			if (psm_ctl->psm_hys_ctl < OLEDB_PSM_HYS_CTRL_MIN ||
			      psm_ctl->psm_hys_ctl > OLEDB_PSM_HYS_CTRL_MAX) {
				pr_err("Invalid value in qcom,psm-hys-mv\n");
				return -EINVAL;
			}
		}

		psm_ctl->psm_vref = -EINVAL;
		rc = of_property_read_u32(of_node, "qcom,psm-vref-mv",
							&psm_ctl->psm_vref);
		if (!rc) {
			u16 min_vref = oledb_psm_vref_mv[0];
			u16 max_vref = oledb_psm_vref_mv[ARRAY_SIZE(
						oledb_psm_vref_mv) - 1];
			if (psm_ctl->psm_vref < min_vref ||
					psm_ctl->psm_vref > max_vref) {
				pr_err("Invalid value in qcom,psm-vref-mv\n");
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int qpnp_oledb_parse_pfm(struct qpnp_oledb *oledb)
{
	int rc = 0;
	struct qpnp_oledb_pfm_ctl *pfm_ctl = &oledb->pfm_ctl;
	struct device_node *of_node = oledb->dev->of_node;

	pfm_ctl->pfm_enable = -EINVAL;
	rc = of_property_read_u32(of_node, "qcom,pfm-enable",
					&pfm_ctl->pfm_enable);
	if (!rc) {
		pfm_ctl->pfm_hys_ctl = -EINVAL;
		rc = of_property_read_u32(of_node, "qcom,pfm-hys-mv",
						&pfm_ctl->pfm_hys_ctl);
		if (!rc) {
			if (pfm_ctl->pfm_hys_ctl < OLEDB_PFM_HYS_CTRL_MIN ||
			       pfm_ctl->pfm_hys_ctl > OLEDB_PFM_HYS_CTRL_MAX) {
				pr_err("Invalid value in qcom,pfm-hys-mv\n");
				return -EINVAL;
			}
		}

		pfm_ctl->pfm_curr_limit = -EINVAL;
		rc = of_property_read_u32(of_node,
		      "qcom,pfm-curr-limit-ma", &pfm_ctl->pfm_curr_limit);
		if (!rc) {
			u16 min_limit = oledb_pfm_curr_limit_ma[0];
			u16 max_limit = oledb_pfm_curr_limit_ma[ARRAY_SIZE(
						oledb_pfm_curr_limit_ma) - 1];
			if (pfm_ctl->pfm_curr_limit < min_limit ||
					pfm_ctl->pfm_curr_limit > max_limit) {
				pr_err("Invalid value in qcom,pfm-curr-limit-ma\n");
				return -EINVAL;
			}
		}

		pfm_ctl->pfm_off_time = -EINVAL;
		rc = of_property_read_u32(of_node, "qcom,pfm-off-time-ns",
							&pfm_ctl->pfm_off_time);
		if (!rc) {
			if (pfm_ctl->pfm_off_time < OLEDB_PFM_OFF_TIME_MIN ||
			      pfm_ctl->pfm_off_time > OLEDB_PFM_OFF_TIME_MAX) {
				pr_err("Invalid value in qcom,pfm-off-time-ns\n");
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int qpnp_oledb_parse_fast_precharge(struct qpnp_oledb *oledb)
{
	int rc = 0;
	struct device_node *of_node = oledb->dev->of_node;
	struct qpnp_oledb_fast_precharge_ctl *fast_prechg =
					&oledb->fast_prechg_ctl;

	fast_prechg->fast_prechg_ppulse_en = -EINVAL;
	rc = of_property_read_u32(of_node, "qcom,fast-precharge-ppulse-enable",
				&fast_prechg->fast_prechg_ppulse_en);
	if (!rc) {
		fast_prechg->prechg_debounce_time = -EINVAL;
		rc = of_property_read_u32(of_node,
					"qcom,precharge-debounce-time-ms",
					&fast_prechg->prechg_debounce_time);
		if (!rc) {
			int dbnc_time = fast_prechg->prechg_debounce_time;

			if (dbnc_time < OLEDB_PRECHG_TIME_MIN || dbnc_time >
						      OLEDB_PRECHG_TIME_MAX) {
				pr_err("Invalid value in qcom,precharge-debounce-time-ms\n");
				return -EINVAL;
			}
		}

		fast_prechg->prechg_pulse_period = -EINVAL;
		rc = of_property_read_u32(of_node,
					"qcom,precharge-pulse-period-us",
					&fast_prechg->prechg_pulse_period);
		if (!rc) {
			int pulse_period = fast_prechg->prechg_pulse_period;

			if (pulse_period < OLEDB_PRECHG_PULSE_PERIOD_MIN ||
			       pulse_period > OLEDB_PRECHG_PULSE_PERIOD_MAX) {
				pr_err("Invalid value in qcom,precharge-pulse-period-us\n");
				return -EINVAL;
			}
		}

		fast_prechg->prechg_pulse_on_time = -EINVAL;
		rc = of_property_read_u32(of_node,
					"qcom,precharge-pulse-on-time-ns",
					&fast_prechg->prechg_pulse_on_time);
		if (!rc) {
			int pulse_on_time = fast_prechg->prechg_pulse_on_time;

			if (pulse_on_time < OLEDB_PRECHG_PULSE_ON_TIME_MIN ||
			      pulse_on_time > OLEDB_PRECHG_PULSE_ON_TIME_MAX) {
				pr_err("Invalid value in qcom,precharge-pulse-on-time-ns\n");
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int qpnp_oledb_parse_dt(struct qpnp_oledb *oledb)
{
	int rc = 0;
	struct device_node *revid_dev_node;
	struct device_node *of_node = oledb->dev->of_node;

	revid_dev_node = of_parse_phandle(oledb->dev->of_node,
					"qcom,pmic-revid", 0);
	if (!revid_dev_node) {
		pr_err("Missing qcom,pmic-revid property - driver failed\n");
		return -EINVAL;
	}

	oledb->pmic_rev_id = get_revid_data(revid_dev_node);
	if (IS_ERR(oledb->pmic_rev_id)) {
		pr_debug("Unable to get revid data\n");
		return -EPROBE_DEFER;
	}

	oledb->swire_control =
			of_property_read_bool(of_node, "qcom,swire-control");

	oledb->ext_pin_control =
			of_property_read_bool(of_node, "qcom,ext-pin-control");

	if (oledb->ext_pin_control)
		oledb->dynamic_ext_pinctl_config =
				of_property_read_bool(of_node,
					"qcom,dynamic-ext-pinctl-config");
	oledb->pbs_control =
			of_property_read_bool(of_node, "qcom,pbs-control");

	/* Use the force_pd_control only for PM660A versions <= v2.0 */
	if (oledb->pmic_rev_id->pmic_subtype == PM660L_SUBTYPE &&
				oledb->pmic_rev_id->rev4 <= PM660L_V2P0_REV4) {
		if (!(oledb->pmic_rev_id->rev4 == PM660L_V2P0_REV4 &&
			oledb->pmic_rev_id->rev2 > PM660L_V2P0_REV2)) {
			oledb->force_pd_control = true;
		}
	}

	if (oledb->force_pd_control) {
		oledb->pbs_dev_node = of_parse_phandle(of_node,
						"qcom,pbs-client", 0);
		if (!oledb->pbs_dev_node) {
			pr_err("Missing qcom,pbs-client property\n");
			return -EINVAL;
		}
	}

	oledb->current_voltage = -EINVAL;
	rc = of_property_read_u32(of_node, "qcom,oledb-init-voltage-mv",
						&oledb->current_voltage);
	if (!rc && (oledb->current_voltage < OLEDB_VOUT_MIN_MV ||
				oledb->current_voltage > OLEDB_VOUT_MAX_MV)) {
		pr_err("Invalid value in qcom,oledb-init-voltage-mv\n");
		return -EINVAL;
	}

	oledb->default_voltage = -EINVAL;
	rc = of_property_read_u32(of_node, "qcom,oledb-default-voltage-mv",
					&oledb->default_voltage);
	if (!rc && (oledb->default_voltage < OLEDB_VOUT_MIN_MV ||
				oledb->default_voltage > OLEDB_VOUT_MAX_MV)) {
		pr_err("Invalid value in qcom,oledb-default-voltage-mv\n");
		return -EINVAL;
	}

	oledb->warmup_delay = -EINVAL;
	rc = of_property_read_u32(of_node, "qcom,bias-gen-warmup-delay-ns",
					&oledb->warmup_delay);
	if (!rc) {
		u16 min_delay = oledb_warmup_dly_ns[0];
		u16 max_delay = oledb_warmup_dly_ns[ARRAY_SIZE(
						oledb_warmup_dly_ns) - 1];
		if (oledb->warmup_delay < min_delay ||
					oledb->warmup_delay > max_delay) {
			pr_err("Invalid value in qcom,bias-gen-warmup-delay-ns\n");
			return -EINVAL;
		}
	}

	oledb->peak_curr_limit = -EINVAL;
	rc = of_property_read_u32(of_node, "qcom,peak-curr-limit-ma",
					&oledb->peak_curr_limit);
	if (!rc) {
		u16 min_limit = oledb_peak_curr_limit_ma[0];
		u16 max_limit = oledb_peak_curr_limit_ma[ARRAY_SIZE(
					oledb_peak_curr_limit_ma) - 1];
		if (oledb->peak_curr_limit < min_limit ||
				oledb->peak_curr_limit > max_limit) {
			pr_err("Invalid value in qcom,peak-curr-limit-ma\n");
			return -EINVAL;
		}
	}

	oledb->pd_ctl = -EINVAL;
	of_property_read_u32(of_node, "qcom,pull-down-enable", &oledb->pd_ctl);

	oledb->sc_en = -EINVAL;
	rc = of_property_read_u32(of_node, "qcom,enable-short-circuit",
					&oledb->sc_en);
	if (!rc) {
		oledb->sc_dbnc_time = -EINVAL;
		rc = of_property_read_u32(of_node,
			"qcom,short-circuit-dbnc-time", &oledb->sc_dbnc_time);
		if (!rc) {
			if (oledb->sc_dbnc_time < OLEDB_MIN_SC_DBNC_TIME_FSW ||
			    oledb->sc_dbnc_time > OLEDB_MAX_SC_DBNC_TIME_FSW) {
				pr_err("Invalid value in qcom,short-circuit-dbnc-time\n");
				return -EINVAL;
			}
		}
	}

	rc = qpnp_oledb_parse_nlimit(oledb);
	if (rc < 0)
		return rc;

	rc = qpnp_oledb_parse_psm(oledb);
	if (rc < 0)
		return rc;

	rc = qpnp_oledb_parse_pfm(oledb);
	if (rc < 0)
		return rc;

	rc = qpnp_oledb_parse_fast_precharge(oledb);

	return rc;
}

static int qpnp_oledb_force_pulldown_config(struct qpnp_oledb *oledb)
{
	int rc = 0;
	u8 val;

	val = 1;
	rc = qpnp_oledb_write(oledb, oledb->base + OLEDB_PD_CTL,
							&val, 1);
	if (rc < 0) {
		pr_err("Failed to write PD_CTL rc=%d\n", rc);
		return rc;
	}

	rc = qpnp_oledb_masked_write(oledb, oledb->base +
		OLEDB_SWIRE_CONTROL, OLEDB_EN_SWIRE_PD_UPD_BIT, 0);
	if (rc < 0)
		pr_err("Failed to write SWIRE_CTL for pbs mode rc=%d\n",
					rc);

	return rc;
}

static int qpnp_labibb_notifier_cb(struct notifier_block *nb,
					unsigned long action, void *data)
{
	int rc = 0;
	u8 val;
	struct qpnp_oledb *oledb = container_of(nb, struct qpnp_oledb,
								oledb_nb);

	if (action == LAB_VREG_NOT_OK) {
		/* short circuit detected. Disable OLEDB module */
		val = 0;
		rc = qpnp_oledb_write(oledb, oledb->base + OLEDB_MODULE_RDY,
					&val, 1);
		if (rc < 0) {
			pr_err("Failed to write MODULE_RDY rc=%d\n", rc);
			return NOTIFY_STOP;
		}
		oledb->lab_sc_detected = true;
		oledb->mod_enable = false;
		pr_crit("LAB SC detected, disabling OLEDB forever!\n");
	}

	if (action == LAB_VREG_OK) {
		/* Disable SWIRE pull down control and enable via spmi mode */
		rc = qpnp_oledb_force_pulldown_config(oledb);
		if (rc < 0) {
			pr_err("Failed to config force pull down\n");
			return NOTIFY_STOP;
		}
	}

	return NOTIFY_OK;
}

static int qpnp_oledb_regulator_probe(struct platform_device *pdev)
{
	int rc = 0;
	u32 val;
	struct qpnp_oledb *oledb;
	struct device_node *of_node = pdev->dev.of_node;

	oledb = devm_kzalloc(&pdev->dev,
			sizeof(struct qpnp_oledb), GFP_KERNEL);
	if (!oledb)
		return -ENOMEM;

	oledb->pdev = pdev;
	oledb->dev = &pdev->dev;
	oledb->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	dev_set_drvdata(&pdev->dev, oledb);
	if (!oledb->regmap) {
		pr_err("Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(of_node, "reg", &val);
	if (rc < 0) {
		pr_err("Couldn't find reg in node, rc = %d\n", rc);
		return rc;
	}

	mutex_init(&(oledb->bus_lock));
	oledb->base = val;
	rc = qpnp_oledb_parse_dt(oledb);
	if (rc < 0) {
		pr_err("Failed to parse common OLEDB device tree\n");
		return rc;
	}

	rc = qpnp_oledb_hw_init(oledb);
	if (rc < 0) {
		pr_err("Failed to initialize OLEDB, rc=%d\n", rc);
		return rc;
	}

	/* Enable LAB short circuit notification support */
	if (oledb->pmic_rev_id->pmic_subtype == PM660L_SUBTYPE)
		oledb->handle_lab_sc_notification = true;

	if (oledb->force_pd_control || oledb->handle_lab_sc_notification) {
		oledb->oledb_nb.notifier_call = qpnp_labibb_notifier_cb;
		rc = qpnp_labibb_notifier_register(&oledb->oledb_nb);
		if (rc < 0) {
			pr_err("Failed to register qpnp_labibb_notifier_cb\n");
			return rc;
		}
	}

	rc = qpnp_oledb_register_regulator(oledb);
	if (rc < 0) {
		pr_err("Failed to register regulator rc=%d\n", rc);
		goto out;
	}
	pr_info("OLEDB registered successfully, ext_pin_en=%d mod_en=%d current_voltage=%d mV\n",
			oledb->ext_pin_control, oledb->mod_enable,
						oledb->current_voltage);
	return 0;

out:
	if (oledb->force_pd_control) {
		rc  = qpnp_labibb_notifier_unregister(&oledb->oledb_nb);
		if (rc < 0)
			pr_err("Failed to unregister lab_vreg_ok notifier\n");
	}

	return rc;
}

static int qpnp_oledb_regulator_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct qpnp_oledb *oledb = platform_get_drvdata(pdev);

	if (oledb->force_pd_control) {
		rc  = qpnp_labibb_notifier_unregister(&oledb->oledb_nb);
		if (rc < 0)
			pr_err("Failed to unregister lab_vreg_ok notifier\n");
	}

	return rc;
}

const struct of_device_id qpnp_oledb_regulator_match_table[] = {
	{ .compatible = QPNP_OLEDB_REGULATOR_DRIVER_NAME,},
	{ },
};

static struct platform_driver qpnp_oledb_regulator_driver = {
	.driver		= {
		.name		= QPNP_OLEDB_REGULATOR_DRIVER_NAME,
		.of_match_table	= qpnp_oledb_regulator_match_table,
	},
	.probe		= qpnp_oledb_regulator_probe,
	.remove		= qpnp_oledb_regulator_remove,
};

static int __init qpnp_oledb_regulator_init(void)
{
	return platform_driver_register(&qpnp_oledb_regulator_driver);
}
arch_initcall(qpnp_oledb_regulator_init);

static void __exit qpnp_oledb_regulator_exit(void)
{
	platform_driver_unregister(&qpnp_oledb_regulator_driver);
}
module_exit(qpnp_oledb_regulator_exit);

MODULE_DESCRIPTION("QPNP OLEDB driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("qpnp-oledb-regulator");
