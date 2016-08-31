/*
 * max77660-charger-extcon.c -- MAXIM MAX77660 VBUS detection.
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Darbha Sriharsha <dsriharsha@nvidia.com>
 * Author: Syed Rafiuddin <srafiuddin@nvidia.com>
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/iio/consumer.h>
#include <linux/pm.h>
#include <linux/extcon.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/max77660/max77660-core.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/power_supply.h>
#include <linux/power/battery-charger-gauge-comm.h>

#define CHARGER_USB_EXTCON_REGISTRATION_DELAY	5000
#define CHARGER_TYPE_DETECTION_DEBOUNCE_TIME_MS	500
#define MAX77660_RESTART_CHARGING_AFTER_DONE      (15 * 60)

#define BATT_TEMP_HOT				56
#define BATT_TEMP_WARM				45
#define BATT_TEMP_COOL				10
#define BATT_TEMP_COLD				3

#define NO_LIMIT				99999

enum charging_states {
	ENABLED_HALF_IBAT = 1,
	ENABLED_FULL_IBAT,
	DISABLED,
};

static int max77660_dccrnt_current_lookup[] = {
	1000, 1000, 1000, 2750, 3000, 3250, 3500, 3750,
	4000, 4250, 4500, 4750, 5000, 5250, 5500, 5750,
	6000, 6250, 6500, 6750, 7000, 7250, 7500, 7750,
	8000, 8250, 8500, 8750, 9000, 9250, 9500, 9750,
	10000, 10250, 10500, 10750, 11000, 11250, 11500, 11750,
	12000, 12250, 12500, 12750, 13000, 13250, 13500, 13750,
	14000, 14250, 14500, 14750, 15000, 15375, 15750, 16125,
	16500, 16875, 17250, 17625, 18000, 18375, 18750, NO_LIMIT,
};

static int max77660_chrg_wdt[] = {16, 32, 64, 128};

struct max77660_charger {
	struct max77660_bcharger_platform_data *bcharger_pdata;
	struct device *dev;
	int irq;
	int status;
	int in_current_lim;
	int wdt_timeout;
};

struct max77660_chg_extcon {
	struct device			*dev;
	struct device			*parent;
	struct extcon_dev		*edev;
	struct regulator_dev		*chg_rdev;

	struct regulator_dev		*vbus_rdev;
	struct regulator_desc		vbus_reg_desc;
	struct regulator_init_data	vbus_reg_init_data;

	struct max77660_charger		*charger;
	int				chg_irq;
	int				chg_wdt_irq;
	int				cable_connected;
	struct regulator_desc		chg_reg_desc;
	struct regulator_init_data	chg_reg_init_data;
	struct battery_charger_dev	*bc_dev;
	int				charging_state;
	int				cable_connected_state;
	int				battery_present;
	int				chg_restart_time_sec;
	int				last_charging_current;
};

struct max77660_chg_extcon *max77660_ext;

const char *max77660_excon_cable[] = {
	[0] = "USB",
	NULL,
};

/* IRQ definitions */
enum {
	MAX77660_CHG_BAT_I = 0,
	MAX77660_CHG_CHG_I,
	MAX77660_CHG_DC_UVP,
	MAX77660_CHG_DC_OVP,
	MAX77660_CHG_DC_I,
	MAX77660_CHG_DC_V,
	MAX77660_CHG_NR_IRQS,
};

static inline int fchg_current(int x)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(max77660_dccrnt_current_lookup); i++) {
		if (10*x < max77660_dccrnt_current_lookup[i])
			break;
	}
	ret = i-1;

	if (ret < 3)
		return 0;
	else
		return ret;
}

static int max77660_battery_detect(struct max77660_chg_extcon *chip)
{
	int ret = 0;
	u8 status;

	ret = max77660_reg_read(chip->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_DETAILS1, &status);
	if (ret < 0) {
		dev_err(chip->dev, "CHARGER_CHGINT read failed: %d\n", ret);
		return ret;
	}
	if ((status & MAX77660_BATDET_DTLS) == MAX77660_BATDET_DTLS_NO_BAT)
		return -EPERM;
	return 0;
}

static int max77660_charger_init(struct max77660_chg_extcon *chip, int enable)
{
	struct max77660_charger *charger = chip->charger;
	int ret;
	u8 read_val;

	if (!chip->battery_present)
		return 0;

	/* Enable USB suspend if 2mA is current configuration */
	if (charger->in_current_lim == 2)
		ret = max77660_reg_set_bits(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_USBCHGCTRL,
				MAX77660_USBCHGCTRL_USB_SUSPEND);
	else
		ret = max77660_reg_clr_bits(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_USBCHGCTRL,
				MAX77660_USBCHGCTRL_USB_SUSPEND);
	if (ret < 0)
		return ret;

	charger->in_current_lim = fchg_current(charger->in_current_lim);

	/* unlock charger protection */
	ret = max77660_reg_write(chip->parent, MAX77660_CHG_SLAVE,
		MAX77660_CHARGER_CHGCTRL1,
		MAX77660_CHGCC_MASK);
	if (ret < 0)
		return ret;

	if (enable) {
		/* enable charger */
		/* Set DCILMT to 1A */
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_DCCRNT,
				charger->in_current_lim);
		if (ret < 0)
			return ret;

		/* Fast charge to 5 hours, fast charge current to 1.1A */
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE, MAX77660_CHARGER_FCHGCRNT,
				MAX77660_FCHG_CRNT);
		if (ret < 0)
			return ret;

		ret = max77660_reg_read(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_FCHGCRNT, &read_val);
		if (ret < 0)
			return ret;

		/* Set TOPOFF to 10 min */
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_TOPOFF,
				MAX77660_ITOPOFF_200MA |
				MAX77660_TOPOFFT_10MIN);
		if (ret < 0)
			return ret;
		/* MBATREG to 4.2V */
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_BATREGCTRL,
				MAX77660_MBATREG_4200MV);
		if (ret < 0)
			return ret;
		/* MBATREGMAX to 4.2V */
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_MBATREGMAX,
				MAX77660_MBATREG_4200MV);
		if (ret < 0)
			return ret;

		/* DSILIM_EN = 1; CEN= 1; QBATEN = 0; VSYSREG = 3.6V */
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_CHGCTRL2,
				MAX77660_VSYSREG_3600MV |
				MAX77660_CEN_MASK |
				MAX77660_PREQ_CURNT |
				MAX77660_DCILIM_EN_MASK);
		if (ret < 0)
			return ret;

		/* Enable register settings for charging*/
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_CHGCCMAX,
				MAX77660_CHGCCMAX_CRNT);

		if (ret < 0)
			return ret;

		/* Enable top level charging */
		ret = max77660_reg_write(chip->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_GLOBAL_CFG1,
				MAX77660_GLBLCNFG1_MASK);
		if (ret < 0)
			return ret;
		ret = max77660_reg_write(chip->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_GLOBAL_CFG4,
				MAX77660_GLBLCNFG4_WDTC_SYS_CLR);
		if (ret < 0)
			return ret;

		ret = max77660_reg_write(chip->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_GLOBAL_CFG6,
				MAX77660_GLBLCNFG6_MASK);
		if (ret < 0)
			return ret;

	} else {
		ret = max77660_reg_write(chip->parent,
				MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_DCCRNT,
				0);
		if (ret < 0)
			return ret;
		/* disable charge */
		/* Clear CEN */
		ret = max77660_reg_clr_bits(chip->parent,
			MAX77660_CHG_SLAVE, MAX77660_CHARGER_CHGCTRL2,
			MAX77660_CEN_MASK);
		if (ret < 0)
			return ret;
		/* Clear top level charge */
		ret = max77660_reg_clr_bits(chip->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_GLOBAL_CFG1, MAX77660_GLBLCNFG1_ENCHGTL);
		if (ret < 0)
			return ret;

	}
	dev_info(chip->dev, "%s\n", (enable) ? "Enable charger" :
			"Disable charger");
	return 0;
}

int max77660_full_current_enable(struct max77660_chg_extcon *chip)
{
	int ret;

	ret = max77660_charger_init(chip, true);
	if (ret < 0) {
		dev_err(chip->dev,
			"Failed to initialise full current charging\n");
		return ret;
	}

	chip->charging_state = ENABLED_FULL_IBAT;

	return 0;
}

int max77660_half_current_enable(struct max77660_chg_extcon *chip)
{
	int ret;
	int temp;

	temp = chip->charger->in_current_lim;
	chip->charger->in_current_lim = chip->charger->in_current_lim/2;
	ret = max77660_charger_init(chip, true);
	if (ret < 0) {
		dev_err(chip->dev,
			"Failed to initialise full current charging\n");
		return ret;
	}
	chip->charger->in_current_lim = temp;
	chip->charging_state = ENABLED_HALF_IBAT;

	return 0;
}

int max77660_charging_disable(struct max77660_chg_extcon *chip)
{
	int ret;

	ret = max77660_charger_init(chip, false);
	if (ret < 0) {
		dev_err(chip->dev,
			"Failed to disable charging\n");
		return ret;
	}
	chip->charging_state = DISABLED;

	return 0;
}

static int max77660_set_charging_current(struct regulator_dev *rdev,
		int min_uA, int max_uA)
{
	struct max77660_chg_extcon *chip = rdev_get_drvdata(rdev);
	struct max77660_charger *charger = chip->charger;
	int ret;
	u8 status;

	ret = max77660_battery_detect(chip);
	if (ret < 0) {
		dev_err(chip->dev,
			"Battery detection failed\n");
		goto error;
	}

	chip->last_charging_current = max_uA;
	charger->in_current_lim = max_uA/1000;
	charger->status = BATTERY_DISCHARGING;

	ret = max77660_reg_read(chip->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_CHGSTAT, &status);
	if (ret < 0) {
		dev_err(chip->dev, "CHSTAT read failed: %d\n", ret);
		return ret;
	}
	if (charger->in_current_lim == 0 &&
			!(status & MAX77660_CHG_CHGINT_DC_UVP))
		return 0;

	if (charger->in_current_lim == 0) {
		chip->cable_connected = 0;
		ret = max77660_charging_disable(chip);
		if (ret < 0)
			goto error;
		battery_charging_status_update(chip->bc_dev,
					BATTERY_DISCHARGING);
		battery_charger_thermal_stop_monitoring(chip->bc_dev);
	} else {
		chip->cable_connected = 1;
		charger->status = BATTERY_CHARGING;
		ret = max77660_full_current_enable(chip);
		if (ret < 0)
			goto error;
		battery_charging_status_update(chip->bc_dev,
					BATTERY_CHARGING);
		battery_charger_thermal_start_monitoring(chip->bc_dev);
	}

	return 0;
error:
	return ret;
}

static struct regulator_ops max77660_charger_ops = {
	.set_current_limit = max77660_set_charging_current,
};

static int max77660_init_charger_regulator(struct max77660_chg_extcon *chip,
	struct max77660_bcharger_platform_data *bcharger_pdata)
{
	struct regulator_config config = { };
	int ret = 0;

	if (!bcharger_pdata) {
		dev_err(chip->dev, "No charger platform data\n");
		return 0;
	}

	chip->chg_reg_desc.name  = "max77660-charger";
	chip->chg_reg_desc.ops   = &max77660_charger_ops;
	chip->chg_reg_desc.type  = REGULATOR_CURRENT;
	chip->chg_reg_desc.owner = THIS_MODULE;

	chip->chg_reg_init_data.supply_regulator     = NULL;
	chip->chg_reg_init_data.regulator_init	= NULL;
	chip->chg_reg_init_data.num_consumer_supplies =
				bcharger_pdata->num_consumer_supplies;
	chip->chg_reg_init_data.consumer_supplies    =
				bcharger_pdata->consumer_supplies;
	chip->chg_reg_init_data.driver_data	   = chip->charger;
	chip->chg_reg_init_data.constraints.name     = "max77660-charger";
	chip->chg_reg_init_data.constraints.min_uA   = 0;
	chip->chg_reg_init_data.constraints.max_uA   =
			bcharger_pdata->max_charge_current_mA * 1000;

	 chip->chg_reg_init_data.constraints.valid_modes_mask =
						REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_STANDBY;

	chip->chg_reg_init_data.constraints.valid_ops_mask =
						REGULATOR_CHANGE_MODE |
						REGULATOR_CHANGE_STATUS |
						REGULATOR_CHANGE_CURRENT;

	config.dev = chip->dev;
	config.init_data = &chip->chg_reg_init_data;
	config.driver_data = chip;

	chip->chg_rdev = regulator_register(&chip->chg_reg_desc, &config);
	if (IS_ERR(chip->chg_rdev)) {
		ret = PTR_ERR(chip->chg_rdev);
		dev_err(chip->dev,
			"vbus-charger regulator register failed %d\n", ret);
	}
	return ret;
}

static int max77660_chg_extcon_cable_update(
		struct max77660_chg_extcon *chg_extcon)
{
	int ret;
	u8 status;

	ret = max77660_reg_read(chg_extcon->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_CHGSTAT, &status);
	if (ret < 0) {
		dev_err(chg_extcon->dev, "CHSTAT read failed: %d\n", ret);
		return ret;
	}
	if (status & MAX77660_CHG_CHGINT_DC_UVP)
		extcon_set_cable_state(chg_extcon->edev, "USB", false);
	else
		extcon_set_cable_state(chg_extcon->edev, "USB", true);

	dev_info(chg_extcon->dev, "VBUS %s status: 0x%02x\n",
		(status & MAX77660_CHG_CHGINT_DC_UVP) ? "Invalid" : "Valid",
		status);

	return 0;
}

static int max77660_charger_detail_irq(int irq, void *data, u8 *val)
{
	struct max77660_chg_extcon *chip = (struct max77660_chg_extcon *)data;

	switch (irq) {
	case MAX77660_CHG_BAT_I:
		switch ((val[2] & MAX77660_BAT_DTLS_MASK)
					>>MAX77660_BAT_DTLS_SHIFT) {
		case MAX77660_BAT_DTLS_BATDEAD:
			dev_info(chip->dev,
				"Battery Interrupt: Battery Dead\n");
			break;
		case MAX77660_BAT_DTLS_TIMER_FAULT:
			dev_info(chip->dev,
			"Battery Interrupt: Charger Watchdog Timer fault\n");
			break;
		case MAX77660_BAT_DTLS_BATOK:
			dev_info(chip->dev,
					"Battery Interrupt: Battery Ok\n");
			break;
		case MAX77660_BAT_DTLS_GTBATOVF:
			dev_info(chip->dev,
				"Battery Interrupt: GTBATOVF\n");
			break;
		default:
			dev_info(chip->dev,
				"Battery Interrupt: details-0x%x\n",
					(val[2] & MAX77660_BAT_DTLS_MASK));
			break;
		}
		break;

	case MAX77660_CHG_CHG_I:
		switch (val[2] & MAX77660_CHG_DTLS_MASK) {
		case MAX77660_CHG_DTLS_DEAD_BAT:
			dev_info(chip->dev,
			"Fast Charge current Interrupt: Dead Battery\n");

			break;
		case MAX77660_CHG_DTLS_PREQUAL:
			dev_info(chip->dev,
				"Fast Charge current Interrupt: PREQUAL\n");

			break;
		case MAX77660_CHG_DTLS_FAST_CHARGE_CC:
			dev_info(chip->dev,
			"Fast Charge current Interrupt: FAST_CHARGE_CC\n");

			break;
		case MAX77660_CHG_DTLS_FAST_CHARGE_CV:
			dev_info(chip->dev,
			"Fast Charge current Interrupt: FAST_CHARGE_CV\n");

			break;
		case MAX77660_CHG_DTLS_TOP_OFF:
			dev_info(chip->dev,
			"Fast Charge current Interrupt: TOP_OFF\n");

			break;
		case MAX77660_CHG_DTLS_DONE:
			dev_info(chip->dev,
			"Fast Charge current Interrupt: DONE\n");
			battery_charging_status_update(chip->bc_dev,
						BATTERY_CHARGING_DONE);
			battery_charger_thermal_stop_monitoring(chip->bc_dev);
			battery_charging_restart(chip->bc_dev,
					chip->chg_restart_time_sec);

			break;
		case MAX77660_CHG_DTLS_DONE_QBAT_ON:
			dev_info(chip->dev,
			"Fast Charge current Interrupt: DONE_QBAT_ON\n");

			break;
		case MAX77660_CHG_DTLS_TIMER_FAULT:
			dev_info(chip->dev,
			"Fast Charge current Interrupt: TIMER_FAULT\n");

			break;
		case MAX77660_CHG_DTLS_DC_INVALID:
			dev_info(chip->dev,
			"Fast Charge current Interrupt: DC_INVALID\n");

			break;
		case MAX77660_CHG_DTLS_THERMAL_LOOP_ACTIVE:
			dev_info(chip->dev,
			"Fast Charge current Interrupt:"
					"THERMAL_LOOP_ACTIVE\n");

			break;
		case MAX77660_CHG_DTLS_CHG_OFF:
			dev_info(chip->dev,
			"Fast Charge current Interrupt: CHG_OFF\n");

			break;
		default:
			dev_info(chip->dev,
			"Fast Charge current Interrupt: details-0x%x\n",
					(val[2] & MAX77660_CHG_DTLS_MASK));
			break;
		}
		break;

	case MAX77660_CHG_DC_UVP:
		max77660_chg_extcon_cable_update(chip);
		break;

	case MAX77660_CHG_DC_OVP:
		if ((val[1] & MAX77660_DC_OVP_MASK) == MAX77660_DC_OVP_MASK) {
			dev_info(chip->dev,
			"DC Over voltage Interrupt: VDC > VDC_OVLO\n");

			/*  VBUS is invalid. VDC > VDC_OVLO  */
			max77660_charger_init(chip, 0);
		} else if ((val[1] & MAX77660_DC_OVP_MASK) == 0) {
			dev_info(chip->dev,
			"DC Over voltage Interrupt: VDC < VDC_OVLO\n");

			/*  VBUS is valid. VDC < VDC_OVLO  */
			max77660_charger_init(chip, 1);
		}
		break;

	case MAX77660_CHG_DC_I:
		dev_info(chip->dev,
		"DC Input Current Limit Interrupt: details-0x%x\n",
				(val[1] & MAX77660_DC_I_MASK));
		break;

	case MAX77660_CHG_DC_V:
		dev_info(chip->dev,
			"DC Input Voltage Limit Interrupt: details-0x%x\n",
					(val[1] & MAX77660_DC_V_MASK));
		break;

	}
	return 0;
}


static irqreturn_t max77660_chg_extcon_irq(int irq, void *data)
{
	struct max77660_chg_extcon *chg_extcon = data;
	u8 irq_val = 0;
	u8 irq_mask = 0;
	u8 irq_name = 0;
	u8 val[3];
	int ret;

	val[0] = 0;
	val[1] = 0;
	val[2] = 0;

	ret = max77660_reg_read(chg_extcon->parent, MAX77660_CHG_SLAVE,
					MAX77660_CHARGER_CHGINT, &irq_val);
	ret = max77660_reg_read(chg_extcon->parent, MAX77660_CHG_SLAVE,
					MAX77660_CHARGER_CHGINTM, &irq_mask);

	ret = max77660_reg_read(chg_extcon->parent, MAX77660_CHG_SLAVE,
					MAX77660_CHARGER_CHGSTAT, &val[0]);
	ret = max77660_reg_read(chg_extcon->parent, MAX77660_CHG_SLAVE,
					MAX77660_CHARGER_DETAILS1, &val[1]);
	ret = max77660_reg_read(chg_extcon->parent, MAX77660_CHG_SLAVE,
					MAX77660_CHARGER_DETAILS2, &val[2]);

	for (irq_name = MAX77660_CHG_BAT_I; irq_name < MAX77660_CHG_NR_IRQS;
								irq_name++) {
		if ((irq_val & (0x01<<(irq_name+2))) &&
				!(irq_mask & (0x01<<(irq_name+2))))
			max77660_charger_detail_irq(irq_name, data, val);
	}

	return IRQ_HANDLED;
}

static int max77660_charger_wdt(struct max77660_chg_extcon *chip)
{
	struct max77660_charger *charger = chip->charger;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(max77660_chrg_wdt); ++i) {
		if (max77660_chrg_wdt[i] >= charger->wdt_timeout)
			break;
	}

	if (i == ARRAY_SIZE(max77660_chrg_wdt)) {
		dev_err(chip->dev, "Charger WDT %d sec is not supported\n",
			charger->wdt_timeout);
		return -EINVAL;
	}

	ret = max77660_reg_update(chip->parent, MAX77660_PWR_SLAVE,
			  MAX77660_REG_GLOBAL_CFG2,
			  MAX77660_GLBLCNFG2_TWD_CHG_MASK,
			  MAX77660_GLBLCNFG2_TWD_CHG(i));
	if (ret < 0) {
		dev_err(chip->dev,
			"GLOBAL_CFG2 update failed: %d\n", ret);
		return ret;
	}

	if (charger->wdt_timeout)
		ret = max77660_reg_set_bits(chip->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_GLOBAL_CFG1, MAX77660_GLBLCNFG1_ENCHGTL);
	else
		ret = max77660_reg_clr_bits(chip->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_GLOBAL_CFG1, MAX77660_GLBLCNFG1_ENCHGTL);
	if (ret < 0) {
		dev_err(chip->dev,
			"GLBLCNFG1_ENCHGTL update failed: %d\n", ret);
		 return ret;
	}
	return ret;
}

static irqreturn_t max77660_chg_wdt_irq(int irq, void *data)
{
	 struct max77660_chg_extcon *chip = (struct max77660_chg_extcon *)data;
	 int ret;

	ret = max77660_reg_write(chip->parent, MAX77660_PWR_SLAVE,
		MAX77660_REG_GLOBAL_CFG6,
		MAX77660_GLBLCNFG4_WDTC_SYS_CLR);
	if (ret < 0)
		dev_err(chip->dev, "GLOBAL_CFG4 update failed: %d\n", ret);

	 return IRQ_HANDLED;
}

static int max77660_vbus_enable_time(struct regulator_dev *rdev)
{
	 return 500;
}

static int max77660_vbus_is_enabled(struct regulator_dev *rdev)
{
	struct max77660_chg_extcon *chg_extcon =rdev_get_drvdata(rdev);
	int ret;
	u8 val;

	ret = max77660_reg_read(chg_extcon->parent, MAX77660_CHG_SLAVE,
					MAX77660_CHARGER_RBOOST, &val);
	if (ret < 0) {
		dev_err(chg_extcon->dev, "RBOOST read failed: %d\n", ret);
		return ret;
	}
	return !!(val & MAX77660_RBOOST_RBOOSTEN);
}

static int max77660_vbus_enable(struct regulator_dev *rdev)
{
	struct max77660_chg_extcon *chg_extcon =rdev_get_drvdata(rdev);
	int ret;

	ret = max77660_reg_update(chg_extcon->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_RBOOST,
			MAX77660_RBOOST_RBOUT_VOUT(0x6),
			MAX77660_RBOOST_RBOUT_MASK);

	if (ret < 0) {
		dev_err(chg_extcon->dev, "RBOOST update failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_set_bits(chg_extcon->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_RBOOST, MAX77660_RBOOST_RBOOSTEN);
	if (ret < 0)
		dev_err(chg_extcon->dev, "RBOOST setbits failed: %d\n", ret);
	return ret;
}

static int max77660_vbus_disable(struct regulator_dev *rdev)
{
	struct max77660_chg_extcon *chg_extcon =rdev_get_drvdata(rdev);
	 int ret;

	ret = max77660_reg_clr_bits(chg_extcon->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_RBOOST, MAX77660_RBOOST_RBOOSTEN);
	if (ret < 0)
		dev_err(chg_extcon->dev, "RBOOST clrbits failed: %d\n", ret);
	 return ret;
}

static struct regulator_ops max77660_vbus_ops = {
	.enable		= max77660_vbus_enable,
	.disable	= max77660_vbus_disable,
	.is_enabled	= max77660_vbus_is_enabled,
	.enable_time	= max77660_vbus_enable_time,
};

static int max77660_init_vbus_regulator(struct max77660_chg_extcon *chg_extcon,
		struct max77660_vbus_platform_data *vbus_pdata)
{
	struct regulator_config rconfig = { };
	int ret = 0;

	if (!vbus_pdata) {
		dev_err(chg_extcon->dev, "No vbus platform data\n");
		return 0;
	}

	chg_extcon->vbus_reg_desc.name = "max77660-vbus";
	chg_extcon->vbus_reg_desc.ops = &max77660_vbus_ops;
	chg_extcon->vbus_reg_desc.type = REGULATOR_VOLTAGE;
	chg_extcon->vbus_reg_desc.owner = THIS_MODULE;

	chg_extcon->vbus_reg_init_data.supply_regulator    = NULL;
	chg_extcon->vbus_reg_init_data.regulator_init      = NULL;
	chg_extcon->vbus_reg_init_data.num_consumer_supplies       =
					vbus_pdata->num_consumer_supplies;
	chg_extcon->vbus_reg_init_data.consumer_supplies   =
					vbus_pdata->consumer_supplies;
	chg_extcon->vbus_reg_init_data.driver_data	 = chg_extcon;

	chg_extcon->vbus_reg_init_data.constraints.name    = "max77660-vbus";
	chg_extcon->vbus_reg_init_data.constraints.min_uV  = 0;
	chg_extcon->vbus_reg_init_data.constraints.max_uV  = 5000000,
	chg_extcon->vbus_reg_init_data.constraints.valid_modes_mask =
					REGULATOR_MODE_NORMAL |
					REGULATOR_MODE_STANDBY;
	chg_extcon->vbus_reg_init_data.constraints.valid_ops_mask =
					REGULATOR_CHANGE_MODE |
					REGULATOR_CHANGE_STATUS |
					REGULATOR_CHANGE_VOLTAGE;

	rconfig.dev = chg_extcon->dev;
	rconfig.of_node = NULL;
	rconfig.init_data = &chg_extcon->vbus_reg_init_data;
	rconfig.driver_data = chg_extcon;
	chg_extcon->vbus_rdev = regulator_register(&chg_extcon->vbus_reg_desc,
				&rconfig);
	if (IS_ERR(chg_extcon->vbus_rdev)) {
		ret = PTR_ERR(chg_extcon->vbus_rdev);
		dev_err(chg_extcon->dev, "Failed to register VBUS regulator: %d\n",
					ret);
		return ret;
	}
	return ret;
}

static int max77660_charger_get_status(struct battery_charger_dev *bc_dev)
{
	struct max77660_chg_extcon *chip = battery_charger_get_drvdata(bc_dev);
	struct max77660_charger *charger = chip->charger;

	return charger->status;
}

static int max77660_charger_thermal_configure(
		struct battery_charger_dev *bc_dev,
		int temp, bool enable_charger, bool enable_charg_half_current,
		int battery_voltage)
{
	struct max77660_chg_extcon *chip = battery_charger_get_drvdata(bc_dev);
	int temperature;
	int ret;

	if (!chip->cable_connected)
		return 0;

	temperature = temp;
	dev_info(chip->dev, "Battery temp %d\n", temperature);
	if (enable_charger) {
		if (!enable_charg_half_current &&
			chip->charging_state != ENABLED_FULL_IBAT) {
			max77660_full_current_enable(chip);
			battery_charging_status_update(chip->bc_dev,
				BATTERY_CHARGING);
		} else if (enable_charg_half_current &&
			chip->charging_state != ENABLED_HALF_IBAT)
			max77660_half_current_enable(chip);
			/* MBATREGMAX to 4.05V */
			ret = max77660_reg_write(chip->parent,
					MAX77660_CHG_SLAVE,
					MAX77660_CHARGER_MBATREGMAX,
					MAX77660_MBATREG_4050MV);
			if (ret < 0)
				return ret;
			battery_charging_status_update(chip->bc_dev,
							BATTERY_CHARGING);
	} else {
		if (chip->charging_state != DISABLED) {
			max77660_charging_disable(chip);
			battery_charging_status_update(chip->bc_dev,
						BATTERY_DISCHARGING);
		}
	}
	return 0;
}

static int max77660_charging_restart(struct battery_charger_dev *bc_dev)
{
	struct max77660_chg_extcon *chip = battery_charger_get_drvdata(bc_dev);
	int ret;

	if (!chip->cable_connected)
		return 0;

	dev_info(chip->dev, "Restarting the charging\n");
	ret = max77660_set_charging_current(chip->chg_rdev,
			chip->last_charging_current,
			chip->last_charging_current);
	if (ret < 0) {
		dev_err(chip->dev, "restarting of charging failed: %d\n", ret);
		battery_charging_restart(chip->bc_dev,
				chip->chg_restart_time_sec);
	}
	return ret;
}

static int max77660_init_oc_alert(struct max77660_chg_extcon *chip)
{
	int ret;
	u8 octh;

	octh = chip->charger->bcharger_pdata->oc_thresh;

	if (octh >= OC_THRESH_DIS) {
		ret = max77660_reg_clr_bits(chip->parent, MAX77660_CHG_SLAVE,
				MAX77660_CHARGER_BAT2SYS,
				MAX77660_CHARGER_BAT2SYS_OCEN);
		if (ret < 0)
			dev_err(chip->dev, "BAT2SYS update failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_set_bits(chip->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_GLOBAL_CFG1,
			MAX77660_GLBLCNFG1_ENPGOC);
	if (ret < 0) {
		dev_err(chip->dev, "GLBLCNFG1 update failed: %d\n", ret);
		return ret;
	}

	ret = max77660_reg_update(chip->parent,
			MAX77660_CHG_SLAVE, MAX77660_CHARGER_BAT2SYS,
			MAX77660_CHARGER_BAT2SYS_OCEN |
			MAX77660_CHARGER_BAT2SYS_OC_MASK,
			MAX77660_CHARGER_BAT2SYS_OCEN | octh);
	if (ret < 0)
		dev_err(chip->dev, "BAT2SYS update failed: %d\n", ret);
	return ret;
}

static struct battery_charging_ops max77660_charger_bci_ops = {
	.get_charging_status = max77660_charger_get_status,
	.thermal_configure = max77660_charger_thermal_configure,
	.restart_charging = max77660_charging_restart,
};

static struct battery_charger_info max77660_charger_bci = {
	.cell_id = 0,
	.bc_ops = &max77660_charger_bci_ops,
};

static int max77660_chg_extcon_probe(struct platform_device *pdev)
{
	struct max77660_chg_extcon *chg_extcon;
	struct max77660_platform_data *pdata;
	struct max77660_charger_platform_data *chg_pdata;
	struct max77660_bcharger_platform_data *bcharger_pdata;
	struct max77660_vbus_platform_data *vbus_pdata;
	struct extcon_dev *edev;
	struct max77660_charger *charger;
	int ret;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (!pdata || !pdata->charger_pdata) {
		dev_err(&pdev->dev, "No platform data\n");
		return -ENODEV;
	}

	chg_pdata = pdata->charger_pdata;
	bcharger_pdata = chg_pdata->bcharger_pdata;
	vbus_pdata = chg_pdata->vbus_pdata;

	chg_extcon = devm_kzalloc(&pdev->dev, sizeof(*chg_extcon), GFP_KERNEL);
	if (!chg_extcon) {
		dev_err(&pdev->dev, "Memory allocation failed for chg_extcon\n");
		return -ENOMEM;
	}

	edev = devm_kzalloc(&pdev->dev, sizeof(*edev), GFP_KERNEL);
	if (!edev) {
		dev_err(&pdev->dev, "Memory allocation failed for edev\n");
		return -ENOMEM;
	}

	chg_extcon->charger = devm_kzalloc(&pdev->dev,
				sizeof(*(chg_extcon->charger)), GFP_KERNEL);
	if (!chg_extcon->charger) {
		dev_err(&pdev->dev, "Memory allocation failed for charger\n");
		return -ENOMEM;
	}

	charger = chg_extcon->charger;
	charger->status = BATTERY_DISCHARGING;
	charger->bcharger_pdata = bcharger_pdata;

	chg_extcon->edev = edev;
	chg_extcon->edev->name = (chg_pdata->ext_conn_name) ?
					chg_pdata->ext_conn_name :
					dev_name(&pdev->dev);
	chg_extcon->edev->supported_cable = max77660_excon_cable;


	chg_extcon->dev = &pdev->dev;
	chg_extcon->parent = pdev->dev.parent;
	dev_set_drvdata(&pdev->dev, chg_extcon);

	chg_extcon->chg_irq = platform_get_irq(pdev, 0);

	chg_extcon->chg_wdt_irq = platform_get_irq(pdev, 1);
	max77660_ext = chg_extcon;

	ret = extcon_dev_register(chg_extcon->edev, NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to register extcon device\n");
		return ret;
	}

	/* Set initial state */
	ret = max77660_chg_extcon_cable_update(chg_extcon);
	if (ret < 0) {
		dev_err(&pdev->dev, "Cable init failed: %d\n", ret);
		goto extcon_free;
	}

	ret = max77660_reg_clr_bits(chg_extcon->parent, MAX77660_CHG_SLAVE,
			MAX77660_CHARGER_CHGINTM, MAX77660_CHG_CHGINT_DC_UVP);
	if (ret < 0) {
		dev_err(chg_extcon->dev, "CHGINTM update failed: %d\n", ret);
		goto extcon_free;
	}

	ret = request_threaded_irq(chg_extcon->chg_irq, NULL,
		max77660_chg_extcon_irq,
		IRQF_ONESHOT | IRQF_EARLY_RESUME, "max77660-charger",
		chg_extcon);
	if (ret < 0) {
		dev_err(chg_extcon->dev,
			"request irq %d failed: %dn", chg_extcon->chg_irq, ret);
		goto extcon_free;
	}

	ret = max77660_init_vbus_regulator(chg_extcon, vbus_pdata);
	if (ret < 0) {
		dev_err(chg_extcon->dev, "Vbus regulator init failed %d\n", ret);
		goto chg_irq_free;
	}

	if (!bcharger_pdata) {
		dev_info(chg_extcon->dev,
			"Battery not connected, charging not supported\n");
		goto skip_bcharger_init;
	}
	chg_extcon->battery_present = true;
	charger->wdt_timeout = bcharger_pdata->wdt_timeout;

	ret = request_threaded_irq(chg_extcon->chg_wdt_irq, NULL,
		max77660_chg_wdt_irq,
		IRQF_ONESHOT | IRQF_EARLY_RESUME, "max77660-charger-wdt",
		chg_extcon);
	if (ret < 0) {
		dev_err(chg_extcon->dev, "request irq %d failed: %d\n",
			chg_extcon->chg_wdt_irq, ret);
		goto vbus_reg_err;
	}

	ret = max77660_init_charger_regulator(chg_extcon, bcharger_pdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register charger regulator: %d\n",
					ret);
		goto wdt_irq_free;
	}

	ret = max77660_charger_wdt(chg_extcon);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"Charger watchdog timer init failed %d\n", ret);
		goto chg_reg_err;
	}

	ret = max77660_init_oc_alert(chg_extcon);
	if (ret < 0) {
		dev_err(&pdev->dev, "OC init failed: %d\n", ret);
		goto chg_reg_err;
	}

	/* Charging auto restart time */
	chg_extcon->chg_restart_time_sec = MAX77660_RESTART_CHARGING_AFTER_DONE;
	max77660_charger_bci.polling_time_sec =
			bcharger_pdata->temperature_poll_period_secs;
	max77660_charger_bci.tz_name = bcharger_pdata->tz_name;
	chg_extcon->bc_dev = battery_charger_register(&pdev->dev,
					&max77660_charger_bci, chg_extcon);
	if (IS_ERR(chg_extcon->bc_dev)) {
		ret = PTR_ERR(chg_extcon->bc_dev);
		dev_err(&pdev->dev, "battery charger register failed: %d\n",
			ret);
		goto chg_reg_err;
	}

skip_bcharger_init:
	device_set_wakeup_capable(&pdev->dev, 1);
	return 0;

chg_reg_err:
	regulator_unregister(chg_extcon->chg_rdev);
wdt_irq_free:
	free_irq(chg_extcon->chg_wdt_irq, chg_extcon);
vbus_reg_err:
	regulator_unregister(chg_extcon->vbus_rdev);
chg_irq_free:
	free_irq(chg_extcon->chg_irq, chg_extcon);
extcon_free:
	extcon_dev_unregister(chg_extcon->edev);
	return ret;
}

static int max77660_chg_extcon_remove(struct platform_device *pdev)
{
	struct max77660_chg_extcon *chg_extcon = dev_get_drvdata(&pdev->dev);

	free_irq(chg_extcon->chg_irq, chg_extcon);
	regulator_unregister(chg_extcon->vbus_rdev);
	if (chg_extcon->battery_present) {
		battery_charger_unregister(chg_extcon->bc_dev);
		extcon_dev_unregister(chg_extcon->edev);
		free_irq(chg_extcon->chg_wdt_irq, chg_extcon);
	}
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max77660_chg_extcon_suspend(struct device *dev)
{
	struct max77660_chg_extcon *chg_extcon = dev_get_drvdata(dev);
	int ret;

	if (device_may_wakeup(dev)) {
		enable_irq_wake(chg_extcon->chg_irq);
		if (chg_extcon->battery_present &&
				chg_extcon->charger->wdt_timeout)
			enable_irq_wake(chg_extcon->chg_wdt_irq);
	} else {
		if (chg_extcon->battery_present &&
				chg_extcon->charger->wdt_timeout) {
			ret = max77660_reg_clr_bits(chg_extcon->parent,
				MAX77660_PWR_SLAVE,
				MAX77660_REG_GLOBAL_CFG1,
				MAX77660_GLBLCNFG1_ENCHGTL);
			if (ret < 0)
				dev_err(chg_extcon->dev,
					"GLBLCNFG1_ENCHGTL update failed: %d\n",
					 ret);
		}
	}
	return 0;
}

static int max77660_chg_extcon_resume(struct device *dev)
{
	struct max77660_chg_extcon *chg_extcon = dev_get_drvdata(dev);
	int ret;

	if (device_may_wakeup(dev)) {
		disable_irq_wake(chg_extcon->chg_irq);
		if (chg_extcon->battery_present &&
			chg_extcon->charger->wdt_timeout)
			disable_irq_wake(chg_extcon->chg_wdt_irq);
	} else {
		if (chg_extcon->battery_present &&
			chg_extcon->charger->wdt_timeout) {
			ret = max77660_reg_set_bits(chg_extcon->parent,
				MAX77660_PWR_SLAVE,
				MAX77660_REG_GLOBAL_CFG1,
				MAX77660_GLBLCNFG1_ENCHGTL);
			if (ret < 0)
				dev_err(chg_extcon->dev,
					"GLBLCNFG1_ENCHGTL update failed: %d\n",
					 ret);
		}
	}
	return 0;
};
#endif

static const struct dev_pm_ops max77660_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(max77660_chg_extcon_suspend,
				max77660_chg_extcon_resume)
};

static struct platform_driver max77660_chg_extcon_driver = {
	.probe = max77660_chg_extcon_probe,
	.remove = max77660_chg_extcon_remove,
	.driver = {
		.name = "max77660-charger-extcon",
		.owner = THIS_MODULE,
		.pm = &max77660_pm_ops,
	},
};

static int __init max77660_chg_extcon_driver_init(void)
{
	return platform_driver_register(&max77660_chg_extcon_driver);
}
subsys_initcall_sync(max77660_chg_extcon_driver_init);

static void __exit max77660_chg_extcon_driver_exit(void)
{
	platform_driver_unregister(&max77660_chg_extcon_driver);
}
module_exit(max77660_chg_extcon_driver_exit);

MODULE_DESCRIPTION("max77660 charger-extcon driver");
MODULE_AUTHOR("Darbha Sriharsha<dsriharsha@nvidia.com>");
MODULE_AUTHOR("Syed Rafiuddin<srafiuddin@nvidia.com>");
MODULE_AUTHOR("Laxman Dewangan<ldewangan@nvidia.com>");
MODULE_ALIAS("platform:max77660-charger-extcon");
MODULE_LICENSE("GPL v2");
