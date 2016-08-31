/*
 * drivers/power/tps80031_charger.c
 *
 * Battery charger driver for TI's tps80031
 *
 * Copyright (c) 2011-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/tps80031.h>
#include <linux/tps80031-charger.h>
#include <linux/module.h>

#define CONTROLLER_CTRL1	0xe1
#define CONTROLLER_STAT1	0xe3
#define CHARGERUSB_CTRL2	0xe9
#define CHARGERUSB_CTRL3	0xea
#define CHARGERUSB_VOREG	0xec
#define CHARGERUSB_VICHRG	0xed
#define CHARGERUSB_CINLIMIT	0xee
#define CHARGERUSB_CTRLLIMIT2	0xf0
#define CHARGERUSB_CTRLLIMIT1	0xef
#define CHARGERUSB_VICHRG_PC	0xdd
#define CONTROLLER_WDG		0xe2
#define LINEAR_CHRG_STS		0xde

#define TPS80031_VBUS_DET	BIT(2)
#define TPS80031_VAC_DET	BIT(3)

struct tps80031_charger {
	int			max_charge_current_mA;
	int			max_charge_volt_mV;
	struct device		*dev;
	struct regulator_dev	*rdev;
	struct regulator_desc	reg_desc;
	struct regulator_init_data		reg_init_data;
	struct tps80031_charger_platform_data	*pdata;
	int (*board_init)(void *board_data);
	void			*board_data;
	int			irq_base;
	int			watch_time_sec;
	enum charging_states	state;
	int			charging_term_current_mA;
	charging_callback_t	charger_cb;
	void			*charger_cb_data;
};

static struct tps80031_charger *charger_data;

static uint8_t tps80031_get_vbus_input_current_limit_code(int max_uA)
{
	const uint8_t current_to_code[] = {
		0x0,				    /* 0 - 50 mA */
		0x0,  0x1,  0x2,  0x3,  0x4,  0x5,  /* 50,  100,  ..., 300mA */
		0x6,  0x7,  0x8,  0x9,  0xA,  0xB,  /* 350, 400,  ..., 600mA */
		0xC,  0xD,  0xE,  0x27, 0x37, 0x28, /* 650, 700,  ..., 900mA */
		0x38, 0x29, 0x39, 0x2A, 0x3A, 0x2B, /* 950, 700,  ..., 1200mA */
		0x3B, 0x2C, 0x3C, 0x2D, 0x3D, 0x2E, /* 1200,1250, ..., 1500mA */
	};
	int charge_mA;
	uint8_t code;

	charge_mA = max_uA / 1000;
	if (charge_mA < 0)
		BUG();
	else if (charge_mA < 1800)
		code = current_to_code[charge_mA / 50];
	else if (charge_mA < 2100)
		code = 0x20; /* use 1800mA code */
	else if (charge_mA < 2250)
		code = 0x21; /* use 2100mA code */
	else
		code = 0x22; /* use 2250mA code */

	return code;
};

static int set_charge_current_limit(struct regulator_dev *rdev,
		int min_uA, int max_uA)
{
	struct tps80031_charger *charger = rdev_get_drvdata(rdev);
	int max_charge_current = 1500;
	uint8_t code;
	int ret;

	dev_info(charger->dev, "%s(): Min curr %dmA and max current %dmA\n",
		__func__, min_uA/1000, max_uA/1000);

	if (!max_uA) {
		ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2,
						CONTROLLER_CTRL1, 0x0);
		if (ret < 0)
			dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_CTRL1);

		ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2,
						CONTROLLER_WDG, 0x0);
		if (ret < 0)
			dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_WDG);
		charger->state = charging_state_charging_stopped;
		if (charger->charger_cb)
			charger->charger_cb(charger->state,
					charger->charger_cb_data);
		return ret;
	}

	code = tps80031_get_vbus_input_current_limit_code(max_uA);
	ret = tps80031_update(charger->dev->parent, TPS80031_SLAVE_ID2,
			CHARGERUSB_CINLIMIT, code, 0x3F);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_CINLIMIT);
		return ret;
	}

	max_charge_current = min(max_uA/1000, max_charge_current);
	if (max_charge_current <= 300)
		max_charge_current = 0;
	else if ((max_charge_current > 300) && (max_charge_current <= 500))
		max_charge_current = (max_charge_current - 300)/50;
	else
		max_charge_current = (max_charge_current - 500) / 100 + 4;
	ret = tps80031_update(charger->dev->parent, TPS80031_SLAVE_ID2,
			CHARGERUSB_VICHRG, (uint8_t)max_charge_current, 0xF);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_VICHRG);
		return ret;
	}

	/* Enable watchdog timer */
	ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2,
				CONTROLLER_WDG, charger->watch_time_sec);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_WDG);
		return ret;
	}

	/* Enable the charging */
	ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2,
				CONTROLLER_CTRL1, 0x30);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_CTRL1);
		return ret;
	}
	charger->state = charging_state_charging_in_progress;
	if (charger->charger_cb)
		charger->charger_cb(charger->state,
				charger->charger_cb_data);
	return 0;
}

static struct regulator_ops tegra_regulator_ops = {
	.set_current_limit = set_charge_current_limit,
};

int register_charging_state_callback(charging_callback_t cb, void *args)
{
	struct tps80031_charger *charger = charger_data;
	if (!charger_data)
		return -ENODEV;

	charger->charger_cb = cb;
	charger->charger_cb_data = args;
	return 0;
}
EXPORT_SYMBOL_GPL(register_charging_state_callback);

static int configure_charging_parameter(struct tps80031_charger *charger)
{
	int ret;
	int max_charge_current;
	int max_charge_volt;
	int term_current;

	/* Disable watchdog timer */
	ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2,
				CONTROLLER_WDG, 0x0);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_WDG);
		return ret;
	}

	/* Disable the charging if any */
	ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2,
				CONTROLLER_CTRL1, 0x0);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_CTRL1);
		return ret;
	}

	if (charger->board_init) {
		ret = charger->board_init(charger->board_data);
		if (ret < 0) {
			dev_err(charger->dev, "%s(): Failed in board init\n",
				__func__);
			return ret;
		}
	}

	/* Unlock value */
	ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2,
			CHARGERUSB_CTRLLIMIT2, 0);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_CTRLLIMIT2);
		return ret;
	}

	/* Set max current limit */
	max_charge_current = min(1500, charger->max_charge_current_mA);
	if (max_charge_current < 100)
		max_charge_current = 0;
	else
		max_charge_current = (max_charge_current - 100)/100;
	max_charge_current &= 0xF;
	ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2,
		CHARGERUSB_CTRLLIMIT2, (uint8_t)max_charge_current);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register "
				"0x%02x\n", __func__, CHARGERUSB_CTRLLIMIT2);
		return ret;
	}

	/* Set max voltage limit */
	max_charge_volt = min(4760, charger->max_charge_volt_mV);
	max_charge_volt = max(3500, max_charge_volt);
	max_charge_volt -= 3500;
	max_charge_volt = max_charge_volt/20;
	ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2,
		CHARGERUSB_CTRLLIMIT1, (uint8_t)max_charge_volt);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_CTRLLIMIT1);
		return ret;
	}

	/* Lock value */
	ret = tps80031_set_bits(charger->dev->parent, TPS80031_SLAVE_ID2,
			CHARGERUSB_CTRLLIMIT2, (1 << 4));
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_CTRLLIMIT2);
		return ret;
	}

	/* set Pre Charge current to 400mA */
	ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2,
			CHARGERUSB_VICHRG_PC, 0x3);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, 0xDD);
		return ret;
	}

	/* set charging termination current*/
	if (charger->charging_term_current_mA > 400)
		term_current =  7;
	else
		term_current = (charger->charging_term_current_mA - 50)/50;
	term_current = term_current << 5;
	ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2,
			CHARGERUSB_CTRL2, term_current);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CHARGERUSB_CTRL2);
		return ret;
	}

	return 0;
}

static bool tps80031_check_charging_completed(struct tps80031_charger *charger)
{
	int ret;
	uint8_t linch_status;

	ret = tps80031_read(charger->dev->parent, TPS80031_SLAVE_ID2,
			LINEAR_CHRG_STS, &linch_status);
	if (ret < 0) {
		dev_err(charger->dev, "%s(): Failed in reading register 0x%02x\n",
				__func__, LINEAR_CHRG_STS);
		return false;
	}

	if (linch_status & 0x20) {
		charger->state = charging_state_charging_completed;
		ret = true;
	} else {
		charger->state = charging_state_charging_in_progress;
		ret = false;
	}

	return ret;
}

static irqreturn_t linch_status_isr(int irq, void *dev_id)
{
	struct tps80031_charger *charger = dev_id;

	dev_info(charger->dev, "%s() got called\n", __func__);

	if (tps80031_check_charging_completed(charger)) {
		charger->state = charging_state_charging_completed;
		if (charger->charger_cb)
			charger->charger_cb(charger->state,
					charger->charger_cb_data);
	}

	return IRQ_HANDLED;
}

static irqreturn_t watchdog_expire_isr(int irq, void *dev_id)
{
	struct tps80031_charger *charger = dev_id;
	int ret;

	dev_info(charger->dev, "%s()\n", __func__);
	if (charger->state != charging_state_charging_in_progress) {
		/*
		 * After the charge completed, the chip can enable the
		 * charging again if battery voltage is 120mV below the
		 * charging voltage (defined by VOREG register).
		 */
		if (tps80031_check_charging_completed(charger)) {
			return IRQ_HANDLED;
		} else {
			/* "recharging" after charging completed happened */
			charger->state = charging_state_charging_in_progress;
			if (charger->charger_cb)
				charger->charger_cb(charger->state,
						charger->charger_cb_data);
		}
	}

	/* Enable watchdog timer again*/
	ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2, CONTROLLER_WDG,
			charger->watch_time_sec);
	if (ret < 0)
		dev_err(charger->dev, "%s(): Failed in writing register 0x%02x\n",
				__func__, CONTROLLER_WDG);

	/* Rewrite to enable the charging */
	if (!ret) {
		ret = tps80031_write(charger->dev->parent, TPS80031_SLAVE_ID2,
			CONTROLLER_CTRL1, 0x30);
		if (ret < 0)
			dev_err(charger->dev, "%s(): Failed in writing "
				"register 0x%02x\n",
				__func__, CONTROLLER_CTRL1);
	}
	return IRQ_HANDLED;
}

static int tps80031_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct tps80031_charger *charger;
	struct tps80031_platform_data *tps80031_pdata;
	struct tps80031_charger_platform_data *pdata;
	struct regulator_config config = { };

	dev_info(dev, "%s()\n", __func__);

	tps80031_pdata = dev_get_platdata(pdev->dev.parent);
	if (!tps80031_pdata) {
		dev_err(&pdev->dev, "no tps80031 platform_data specified\n");
		return -EINVAL;
	}

	pdata = tps80031_pdata->battery_charger_pdata;
	if (!pdata) {
		dev_err(dev, "%s() No platform data, exiting..\n", __func__);
		return -ENODEV;
	}

	if (!pdata->num_consumer_supplies) {
		dev_err(dev, "%s() No consumer supply list, exiting..\n",
				__func__);
		return -ENODEV;
	}

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger) {
		dev_err(dev, "failed to allocate memory status\n");
		return -ENOMEM;
	}

	charger->dev =  &pdev->dev;

	charger->max_charge_current_mA = (pdata->max_charge_current_mA) ?
					pdata->max_charge_current_mA : 1000;
	charger->max_charge_volt_mV = (pdata->max_charge_volt_mV) ?
					pdata->max_charge_volt_mV : 4200;
	charger->irq_base = pdata->irq_base;
	charger->watch_time_sec = min(pdata->watch_time_sec, 127);
	if (!charger->watch_time_sec)
		charger->watch_time_sec = 127;
	charger->charging_term_current_mA =
			min(50, pdata->charging_term_current_mA);
	if (charger->charging_term_current_mA < 50)
		charger->charging_term_current_mA = 50;

	charger->reg_desc.name = "vbus_charger";
	charger->reg_desc.id = pdata->regulator_id;
	charger->reg_desc.ops = &tegra_regulator_ops;
	charger->reg_desc.type = REGULATOR_CURRENT;
	charger->reg_desc.owner = THIS_MODULE;

	charger->reg_init_data.supply_regulator = NULL;
	charger->reg_init_data.num_consumer_supplies =
					pdata->num_consumer_supplies;
	charger->reg_init_data.consumer_supplies = pdata->consumer_supplies;
	charger->reg_init_data.regulator_init = NULL;
	charger->reg_init_data.driver_data = charger;
	charger->reg_init_data.constraints.name = "vbus_charger";
	charger->reg_init_data.constraints.min_uA = 0;
	charger->reg_init_data.constraints.max_uA =
					pdata->max_charge_current_mA * 1000;
	charger->reg_init_data.constraints.valid_modes_mask =
					REGULATOR_MODE_NORMAL |
					REGULATOR_MODE_STANDBY;
	charger->reg_init_data.constraints.valid_ops_mask =
					REGULATOR_CHANGE_MODE |
					REGULATOR_CHANGE_STATUS |
					REGULATOR_CHANGE_CURRENT;

	charger->board_init = pdata->board_init;
	charger->board_data = pdata->board_data;
	charger->state = charging_state_idle;

	config.dev = &pdev->dev;
	config.init_data = &charger->reg_init_data;
	config.driver_data = charger;
	config.of_node = NULL;

	charger->rdev = regulator_register(&charger->reg_desc, &config);
	if (IS_ERR(charger->rdev)) {
		dev_err(&pdev->dev, "failed to register %s\n",
						charger->reg_desc.name);
		ret = PTR_ERR(charger->rdev);
		goto regulator_fail;
	}

	ret = request_threaded_irq(charger->irq_base + TPS80031_INT_LINCH_GATED,
			NULL, linch_status_isr,	0, "tps80031-linch", charger);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register irq %d; error %d\n",
			charger->irq_base + TPS80031_INT_LINCH_GATED, ret);
		goto irq_linch_fail;
	}

	ret = request_threaded_irq(charger->irq_base + TPS80031_INT_FAULT_WDG,
			NULL, watchdog_expire_isr, 0, "tps80031-wdg", charger);
	if (ret) {
		dev_err(&pdev->dev, "Unable to register irq %d; error %d\n",
			charger->irq_base + TPS80031_INT_FAULT_WDG, ret);
		goto irq_wdg_fail;
	}

	ret = configure_charging_parameter(charger);
	if (ret)
		goto config_fail;

	dev_set_drvdata(&pdev->dev, charger);
	charger_data = charger;
	return ret;

config_fail:
	free_irq(charger->irq_base + TPS80031_INT_FAULT_WDG, charger);
irq_wdg_fail:
	free_irq(charger->irq_base + TPS80031_INT_LINCH_GATED, charger);
irq_linch_fail:
	regulator_unregister(charger->rdev);
regulator_fail:
	kfree(charger);
	return ret;
}

static int tps80031_charger_remove(struct platform_device *pdev)
{
	struct tps80031_charger *charger = dev_get_drvdata(&pdev->dev);

	regulator_unregister(charger->rdev);
	kfree(charger);
	return 0;
}

static struct platform_driver tps80031_charger_driver = {
	.driver	= {
		.name	= "tps80031-charger",
		.owner	= THIS_MODULE,
	},
	.probe	= tps80031_charger_probe,
	.remove = tps80031_charger_remove,
};

static int __init tps80031_charger_init(void)
{
	return platform_driver_register(&tps80031_charger_driver);
}

static void __exit tps80031_charger_exit(void)
{
	platform_driver_unregister(&tps80031_charger_driver);
}

subsys_initcall(tps80031_charger_init);
module_exit(tps80031_charger_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("tps80031 battery charger driver");
