// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

/*
 * Bluetooth Power Switch Module
 * controls power to external Bluetooth device
 * with interface to power management device
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/bluetooth-power.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/uaccess.h>

#if defined(CONFIG_CNSS)
#include <net/cnss.h>
#endif

#if defined CONFIG_BT_SLIM_QCA6390 || defined CONFIG_BTFM_SLIM_WCN3990
#include "btfm_slim.h"
#include "btfm_slim_slave.h"
#endif
#include <linux/fs.h>

#define BT_PWR_DBG(fmt, arg...)  pr_debug("%s: " fmt "\n", __func__, ## arg)
#define BT_PWR_INFO(fmt, arg...) pr_info("%s: " fmt "\n", __func__, ## arg)
#define BT_PWR_ERR(fmt, arg...)  pr_err("%s: " fmt "\n", __func__, ## arg)

#define PWR_SRC_NOT_AVAILABLE -2
#define DEFAULT_INVALID_VALUE -1
#define PWR_SRC_INIT_STATE_IDX 0

static const struct of_device_id bt_power_match_table[] = {
	{	.compatible = "qca,ar3002" },
	{	.compatible = "qca,qca6174" },
	{	.compatible = "qca,wcn3990" },
	{	.compatible = "qca,qca6390" },
	{	.compatible = "qca,wcn6750" },
	{}
};

enum power_src_pos {
	BT_RESET_GPIO = PWR_SRC_INIT_STATE_IDX,
	BT_SW_CTRL_GPIO,
	BT_VDD_AON_LDO,
	BT_VDD_DIG_LDO,
	BT_VDD_RFA1_LDO,
	BT_VDD_RFA2_LDO,
	BT_VDD_ASD_LDO,
	BT_VDD_XTAL_LDO,
	BT_VDD_PA_LDO,
	BT_VDD_CORE_LDO,
	BT_VDD_IO_LDO,
	BT_VDD_LDO,
	BT_VDD_RFA_0p8,
	BT_VDD_RFACMN,
	// these indexes GPIOs/regs value are fetched during crash.
	BT_RESET_GPIO_CURRENT,
	BT_SW_CTRL_GPIO_CURRENT,
	BT_VDD_AON_LDO_CURRENT,
	BT_VDD_DIG_LDO_CURRENT,
	BT_VDD_RFA1_LDO_CURRENT,
	BT_VDD_RFA2_LDO_CURRENT,
	BT_VDD_ASD_LDO_CURRENT,
	BT_VDD_XTAL_LDO_CURRENT,
	BT_VDD_PA_LDO_CURRENT,
	BT_VDD_CORE_LDO_CURRENT,
	BT_VDD_IO_LDO_CURRENT,
	BT_VDD_LDO_CURRENT,
	BT_VDD_RFA_0p8_CURRENT,
	BT_VDD_RFACMN_CURRENT
};

static int bt_power_src_status[BT_POWER_SRC_SIZE];
static struct bluetooth_power_platform_data *bt_power_pdata;
static struct platform_device *btpdev;
static bool previous;
static int pwr_state;
struct class *bt_class;
static int bt_major;
static int soc_id;

static int bt_vreg_init(struct bt_power_vreg_data *vreg)
{
	int rc = 0;
	struct device *dev = &btpdev->dev;

	BT_PWR_DBG("vreg_get for : %s", vreg->name);

	/* Get the regulator handle */
	vreg->reg = regulator_get(dev, vreg->name);
	if (IS_ERR(vreg->reg)) {
		rc = PTR_ERR(vreg->reg);
		vreg->reg = NULL;
		pr_err("%s: regulator_get(%s) failed. rc=%d\n",
			__func__, vreg->name, rc);
		goto out;
	}

	if ((regulator_count_voltages(vreg->reg) > 0)
			&& (vreg->low_vol_level) && (vreg->high_vol_level))
		vreg->set_voltage_sup = 1;

out:
	return rc;
}

static int bt_vreg_enable(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	if (!vreg->is_enabled) {
		if (vreg->set_voltage_sup) {
			rc = regulator_set_voltage(vreg->reg,
						vreg->low_vol_level,
						vreg->high_vol_level);
			if (rc < 0) {
				BT_PWR_ERR("vreg_set_vol(%s) failed rc=%d\n",
						vreg->name, rc);
				goto out;
			}
		}

		if (vreg->load_uA >= 0) {
			rc = regulator_set_load(vreg->reg,
					vreg->load_uA);
			if (rc < 0) {
				BT_PWR_ERR("vreg_set_mode(%s) failed rc=%d\n",
						vreg->name, rc);
				goto out;
			}
		}

		rc = regulator_enable(vreg->reg);
		if (rc < 0) {
			BT_PWR_ERR("regulator_enable(%s) failed. rc=%d\n",
					vreg->name, rc);
			goto out;
		}
		vreg->is_enabled = true;
	}

	BT_PWR_ERR("vreg_en successful for : %s", vreg->name);
out:
	return rc;
}

static int bt_vreg_unvote(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	if (!vreg)
		return rc;

	if (vreg->is_enabled) {
		if (vreg->set_voltage_sup) {
			/* Set the min voltage to 0 */
			rc = regulator_set_voltage(vreg->reg, 0,
					vreg->high_vol_level);
			if (rc < 0) {
				BT_PWR_ERR("vreg_set_vol(%s) failed rc=%d\n",
						vreg->name, rc);
				goto out;
			}
		}
		if (vreg->load_uA >= 0) {
			rc = regulator_set_load(vreg->reg, 0);
			if (rc < 0) {
				BT_PWR_ERR("vreg_set_mode(%s) failed rc=%d\n",
						vreg->name, rc);
				goto out;
			}
		}
	}

	BT_PWR_ERR("vreg_unvote successful for : %s", vreg->name);
out:
	return rc;
}

static int bt_vreg_disable(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	if (!vreg)
		return rc;

	if (vreg->is_enabled) {
		rc = regulator_disable(vreg->reg);
		if (rc < 0) {
			BT_PWR_ERR("regulator_disable(%s) failed. rc=%d\n",
					vreg->name, rc);
			goto out;
		}
		vreg->is_enabled = false;

		if (vreg->set_voltage_sup) {
			/* Set the min voltage to 0 */
			rc = regulator_set_voltage(vreg->reg, 0,
					vreg->high_vol_level);
			if (rc < 0) {
				BT_PWR_ERR("vreg_set_vol(%s) failed rc=%d\n",
						vreg->name, rc);
				goto out;
			}
		}
		if (vreg->load_uA >= 0) {
			rc = regulator_set_load(vreg->reg, 0);
			if (rc < 0) {
				BT_PWR_ERR("vreg_set_mode(%s) failed rc=%d\n",
						vreg->name, rc);
				goto out;
			}
		}
	}

	BT_PWR_ERR("vreg_disable successful for : %s", vreg->name);
out:
	return rc;
}

static int bt_configure_vreg(struct bt_power_vreg_data *vreg)
{
	int rc = 0;

	BT_PWR_DBG("config %s", vreg->name);

	/* Get the regulator handle for vreg */
	if (!(vreg->reg)) {
		rc = bt_vreg_init(vreg);
		if (rc < 0)
			return rc;
	}
	rc = bt_vreg_enable(vreg);

	return rc;
}

static int bt_clk_enable(struct bt_power_clk_data *clk)
{
	int rc = 0;

	BT_PWR_DBG("%s", clk->name);

	/* Get the clock handle for vreg */
	if (!clk->clk || clk->is_enabled) {
		BT_PWR_ERR("error - node: %p, clk->is_enabled:%d",
			clk->clk, clk->is_enabled);
		return -EINVAL;
	}

	rc = clk_prepare_enable(clk->clk);
	if (rc) {
		BT_PWR_ERR("failed to enable %s, rc(%d)\n", clk->name, rc);
		return rc;
	}

	clk->is_enabled = true;
	return rc;
}

static int bt_clk_disable(struct bt_power_clk_data *clk)
{
	int rc = 0;

	BT_PWR_DBG("%s", clk->name);

	/* Get the clock handle for vreg */
	if (!clk->clk || !clk->is_enabled) {
		BT_PWR_ERR("error - node: %p, clk->is_enabled:%d",
			clk->clk, clk->is_enabled);
		return -EINVAL;
	}
	clk_disable_unprepare(clk->clk);

	clk->is_enabled = false;
	return rc;
}

static int bt_enable_bt_reset_gpios_safely(void)
{
	int rc = 0;
	int bt_reset_gpio = bt_power_pdata->bt_gpio_sys_rst;
	int wl_reset_gpio = bt_power_pdata->wl_gpio_sys_rst;

	if (wl_reset_gpio >= 0) {
		BT_PWR_INFO("%s: BTON:Turn Bt On", __func__);
		BT_PWR_INFO("%s: wl-reset-gpio(%d) value(%d)",
			__func__, wl_reset_gpio,
				gpio_get_value(wl_reset_gpio));
	}

	if ((wl_reset_gpio < 0) ||
		((wl_reset_gpio >= 0) &&
			gpio_get_value(wl_reset_gpio))) {
		BT_PWR_INFO("%s: BTON: Asserting BT_EN",
			__func__);
		rc = gpio_direction_output(bt_reset_gpio, 1);
		if (rc) {
			BT_PWR_ERR("%s: Unable to set direction",
				__func__);
			return rc;
		}
		bt_power_src_status[BT_RESET_GPIO] =
			gpio_get_value(bt_reset_gpio);
	}

	if ((wl_reset_gpio >= 0) &&
		(gpio_get_value(wl_reset_gpio) == 0)) {
		if (gpio_get_value(bt_reset_gpio)) {
			BT_PWR_INFO("%s: Wlan Off and BT On too close",
				__func__);
			BT_PWR_INFO("%s: Reset BT_EN", __func__);
			BT_PWR_INFO("%s: Enable it after delay",
				__func__);
			rc = gpio_direction_output(bt_reset_gpio, 0);
			if (rc) {
				BT_PWR_ERR("%s:Unable to set direction",
					__func__);
				return rc;
			}
			bt_power_src_status[BT_RESET_GPIO] =
				gpio_get_value(bt_reset_gpio);
		}
		BT_PWR_INFO("%s: 100ms delay added", __func__);
		BT_PWR_INFO("%s: for AON output to fully discharge",
			__func__);
		msleep(100);
		rc = gpio_direction_output(bt_reset_gpio, 1);
		if (rc) {
			BT_PWR_ERR("%s: Unable to set direction",
				__func__);
			return rc;
		}
		bt_power_src_status[BT_RESET_GPIO] =
			gpio_get_value(bt_reset_gpio);
	}
	return rc;
}

static int bt_configure_gpios(int on)
{
	int rc = 0;
	int bt_reset_gpio = bt_power_pdata->bt_gpio_sys_rst;
	int bt_sw_ctrl_gpio  =  bt_power_pdata->bt_gpio_sw_ctrl;
	int bt_debug_gpio  =  bt_power_pdata->bt_gpio_debug;
	int assertDebugGpio = 0;

	if (on) {
		rc = gpio_request(bt_reset_gpio, "bt_sys_rst_n");
		if (rc) {
			BT_PWR_ERR("unable to request gpio %d (%d)\n",
					bt_reset_gpio, rc);
			return rc;
		}
		rc = gpio_direction_output(bt_reset_gpio, 0);

		if (rc) {
			BT_PWR_ERR("Unable to set direction\n");
			return rc;
		}
		bt_power_src_status[BT_RESET_GPIO] =
			gpio_get_value(bt_reset_gpio);
		msleep(50);
		BT_PWR_INFO("BTON:Turn Bt Off bt-reset-gpio(%d) value(%d)",
				bt_reset_gpio, gpio_get_value(bt_reset_gpio));
		if (bt_sw_ctrl_gpio >= 0) {
			BT_PWR_INFO("BTON:Turn Bt Off");
			bt_power_src_status[BT_SW_CTRL_GPIO] =
				gpio_get_value(bt_sw_ctrl_gpio);
			BT_PWR_INFO("bt-sw-ctrl-gpio(%d) value(%d)",
					bt_sw_ctrl_gpio,
					bt_power_src_status[BT_SW_CTRL_GPIO]);
		}

		rc = bt_enable_bt_reset_gpios_safely();
		if (rc) {
			BT_PWR_ERR("%s:bt_enable_bt_reset_gpios_safely failed",
				__func__);
			return rc;
		}

		msleep(50);
		/*  Check  if  SW_CTRL  is  asserted  */
		if  (bt_sw_ctrl_gpio  >=  0)  {
			rc  =  gpio_direction_input(bt_sw_ctrl_gpio);
			if  (rc)  {
				BT_PWR_ERR("SWCTRL Dir Set Problem:%d\n", rc);
			}  else  if  (!gpio_get_value(bt_sw_ctrl_gpio))  {
				/*  Assert  debug  GPIO, if available  as
				 * SW_CTRL  is  not  asserted
				 */
				if  (bt_debug_gpio  >=  0)
					assertDebugGpio = 1;
			}
		}
		if (assertDebugGpio) {
			rc  =  gpio_request(bt_debug_gpio, "bt_debug_n");
			if  (rc)  {
				BT_PWR_ERR("unable to request Debug Gpio\n");
			}  else  {
				rc = gpio_direction_output(bt_debug_gpio,  1);
				if (rc)
					BT_PWR_ERR("Prob: Set Debug-Gpio\n");
			}
		}
		BT_PWR_INFO("BTON:Turn Bt On bt-reset-gpio(%d) value(%d)\n",
				bt_reset_gpio, gpio_get_value(bt_reset_gpio));
		if (bt_sw_ctrl_gpio >= 0) {
			BT_PWR_INFO("BTON:Turn Bt On");
			bt_power_src_status[BT_SW_CTRL_GPIO] =
				gpio_get_value(bt_sw_ctrl_gpio);
			BT_PWR_INFO("bt-sw-ctrl-gpio(%d) value(%d)",
					bt_sw_ctrl_gpio,
					bt_power_src_status[BT_SW_CTRL_GPIO]);
		}
	} else {
		gpio_set_value(bt_reset_gpio, 0);
		if  (bt_debug_gpio  >=  0)
			gpio_set_value(bt_debug_gpio,  0);
		msleep(100);
		BT_PWR_INFO("BT-OFF:bt-reset-gpio(%d) value(%d)\n",
				bt_reset_gpio, gpio_get_value(bt_reset_gpio));

		if (bt_sw_ctrl_gpio >= 0) {
			BT_PWR_INFO("BT-OFF:bt-sw-ctrl-gpio(%d) value(%d)",
					bt_sw_ctrl_gpio,
					gpio_get_value(bt_sw_ctrl_gpio));
		}
	}

	BT_PWR_INFO("bt_gpio= %d on: %d is successful", bt_reset_gpio, on);
	return rc;
}

static void bt_free_gpios(void)
{
	if (bt_power_pdata->bt_gpio_sys_rst > 0)
		gpio_free(bt_power_pdata->bt_gpio_sys_rst);
	if  (bt_power_pdata->bt_gpio_debug  >  0)
		gpio_free(bt_power_pdata->bt_gpio_debug);
}

static int bluetooth_power(int on)
{
	int rc = 0;
	struct regulator *reg = NULL;

	BT_PWR_DBG("on: %d", on);

	if (on == 1) {
		// Power On
		if (bt_power_pdata->bt_vdd_io) {
			bt_power_src_status[BT_VDD_IO_LDO] =
				DEFAULT_INVALID_VALUE;
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_io);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddio config failed");
				goto out;
			}
			reg = bt_power_pdata->bt_vdd_io->reg;
			bt_power_src_status[BT_VDD_IO_LDO] =
				regulator_get_voltage(reg);
		}
		if (bt_power_pdata->bt_vdd_xtal) {
			bt_power_src_status[BT_VDD_XTAL_LDO] =
				DEFAULT_INVALID_VALUE;
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_xtal);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddxtal config failed");
				goto vdd_xtal_fail;
			}
			reg = bt_power_pdata->bt_vdd_xtal->reg;
			bt_power_src_status[BT_VDD_XTAL_LDO] =
				regulator_get_voltage(reg);
		}
		if (bt_power_pdata->bt_vdd_core) {
			bt_power_src_status[BT_VDD_CORE_LDO] =
				DEFAULT_INVALID_VALUE;
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_core);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddcore config failed");
				goto vdd_core_fail;
			}
			reg = bt_power_pdata->bt_vdd_core->reg;
			bt_power_src_status[BT_VDD_CORE_LDO] =
				regulator_get_voltage(reg);
		}
		if (bt_power_pdata->bt_vdd_pa) {
			bt_power_src_status[BT_VDD_PA_LDO] =
				DEFAULT_INVALID_VALUE;
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_pa);

			if (rc < 0) {
				BT_PWR_ERR("bt_power vddpa config failed");
				goto vdd_pa_fail;
			}
			reg = bt_power_pdata->bt_vdd_pa->reg;
			bt_power_src_status[BT_VDD_PA_LDO] =
				regulator_get_voltage(reg);
		}
		if (bt_power_pdata->bt_vdd_ldo) {
			bt_power_src_status[BT_VDD_LDO] =
				DEFAULT_INVALID_VALUE;
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_ldo);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddldo config failed");
				goto vdd_ldo_fail;
			}
			reg = bt_power_pdata->bt_vdd_ldo->reg;
			bt_power_src_status[BT_VDD_LDO] =
				regulator_get_voltage(reg);
		}
		if (bt_power_pdata->bt_vdd_aon) {
			bt_power_src_status[BT_VDD_AON_LDO] =
				DEFAULT_INVALID_VALUE;
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_aon);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddaon config failed");
				goto vdd_aon_fail;
			}
			reg = bt_power_pdata->bt_vdd_aon->reg;
			bt_power_src_status[BT_VDD_AON_LDO] =
				regulator_get_voltage(reg);
		}
		if (bt_power_pdata->bt_vdd_dig) {
			bt_power_src_status[BT_VDD_DIG_LDO] =
				DEFAULT_INVALID_VALUE;
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_dig);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vdddig config failed");
				goto vdd_dig_fail;
			}
			reg = bt_power_pdata->bt_vdd_dig->reg;
			bt_power_src_status[BT_VDD_DIG_LDO] =
				regulator_get_voltage(reg);
		}
		if (bt_power_pdata->bt_vdd_rfa1) {
			bt_power_src_status[BT_VDD_RFA1_LDO] =
				DEFAULT_INVALID_VALUE;
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_rfa1);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddrfa1 config failed");
				goto vdd_rfa1_fail;
			}
			reg = bt_power_pdata->bt_vdd_rfa1->reg;
			bt_power_src_status[BT_VDD_RFA1_LDO] =
				regulator_get_voltage(reg);
		}
		if (bt_power_pdata->bt_vdd_rfa2) {
			bt_power_src_status[BT_VDD_RFA2_LDO] =
				DEFAULT_INVALID_VALUE;
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_rfa2);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddrfa2 config failed");
				goto vdd_rfa2_fail;
			}
			reg = bt_power_pdata->bt_vdd_rfa2->reg;
			bt_power_src_status[BT_VDD_RFA2_LDO] =
				regulator_get_voltage(reg);
		}
		if (bt_power_pdata->bt_vdd_asd) {
			bt_power_src_status[BT_VDD_ASD_LDO] =
				DEFAULT_INVALID_VALUE;
			rc = bt_configure_vreg(bt_power_pdata->bt_vdd_asd);
			if (rc < 0) {
				BT_PWR_ERR("bt_power vddasd config failed");
				goto vdd_asd_fail;
			}
			reg = bt_power_pdata->bt_vdd_asd->reg;
			bt_power_src_status[BT_VDD_ASD_LDO] =
				regulator_get_voltage(reg);
		}
		if (bt_power_pdata->bt_chip_pwd) {
			rc = bt_configure_vreg(bt_power_pdata->bt_chip_pwd);
			if (rc < 0) {
				BT_PWR_ERR("bt_power chippwd config failed");
				goto chip_pwd_fail;
			}
		}
		/* Parse dt_info and check if a target requires clock voting.
		 * Enable BT clock when BT is on and disable it when BT is off
		 */
		if (bt_power_pdata->bt_chip_clk) {
			rc = bt_clk_enable(bt_power_pdata->bt_chip_clk);
			if (rc < 0) {
				BT_PWR_ERR("bt_power gpio config failed");
				goto clk_fail;
			}
		}
		if (bt_power_pdata->bt_gpio_sys_rst > 0) {
			bt_power_src_status[BT_RESET_GPIO] =
				DEFAULT_INVALID_VALUE;
			bt_power_src_status[BT_SW_CTRL_GPIO] =
				DEFAULT_INVALID_VALUE;
			rc = bt_configure_gpios(on);
			if (rc < 0) {
				BT_PWR_ERR("bt_power gpio config failed");
				goto gpio_fail;
			}
		}
	} else if (on == 0) {
		// Power Off
		if (bt_power_pdata->bt_gpio_sys_rst > 0)
			bt_configure_gpios(on);
gpio_fail:
		//Free Gpios
		bt_free_gpios();

		if (bt_power_pdata->bt_chip_clk)
			bt_clk_disable(bt_power_pdata->bt_chip_clk);
clk_fail:
		if (bt_power_pdata->bt_chip_pwd)
			bt_vreg_disable(bt_power_pdata->bt_chip_pwd);
chip_pwd_fail:
		if (bt_power_pdata->bt_vdd_asd)
			bt_vreg_disable(bt_power_pdata->bt_vdd_asd);
vdd_asd_fail:
		if (bt_power_pdata->bt_vdd_rfa2)
			bt_vreg_disable(bt_power_pdata->bt_vdd_rfa2);
vdd_rfa2_fail:
		if (bt_power_pdata->bt_vdd_rfa1)
			bt_vreg_disable(bt_power_pdata->bt_vdd_rfa1);
vdd_rfa1_fail:
		if (bt_power_pdata->bt_vdd_dig)
			bt_vreg_disable(bt_power_pdata->bt_vdd_dig);
vdd_dig_fail:
		if (bt_power_pdata->bt_vdd_aon)
			bt_vreg_disable(bt_power_pdata->bt_vdd_aon);
vdd_aon_fail:
		if (bt_power_pdata->bt_vdd_ldo)
			bt_vreg_disable(bt_power_pdata->bt_vdd_ldo);
vdd_ldo_fail:
		if (bt_power_pdata->bt_vdd_pa)
			bt_vreg_disable(bt_power_pdata->bt_vdd_pa);
vdd_pa_fail:
		if (bt_power_pdata->bt_vdd_core)
			bt_vreg_disable(bt_power_pdata->bt_vdd_core);
vdd_core_fail:
		if (bt_power_pdata->bt_vdd_xtal)
			bt_vreg_disable(bt_power_pdata->bt_vdd_xtal);
vdd_xtal_fail:
		if (bt_power_pdata->bt_vdd_io)
			bt_vreg_disable(bt_power_pdata->bt_vdd_io);
	} else if (on == 2) {
		/* Retention mode */
		if (bt_power_pdata->bt_vdd_rfa2)
			bt_vreg_unvote(bt_power_pdata->bt_vdd_rfa2);
		if (bt_power_pdata->bt_vdd_rfa1)
			bt_vreg_unvote(bt_power_pdata->bt_vdd_rfa1);
		if (bt_power_pdata->bt_vdd_dig)
			bt_vreg_unvote(bt_power_pdata->bt_vdd_dig);
		if (bt_power_pdata->bt_vdd_aon)
			bt_vreg_unvote(bt_power_pdata->bt_vdd_aon);
	} else {
		BT_PWR_ERR("Invalid power mode: %d", on);
		rc = -1;
	}
out:
	return rc;
}

static int bluetooth_toggle_radio(void *data, bool blocked)
{
	int ret = 0;
	int (*power_control)(int enable);

	power_control =
		((struct bluetooth_power_platform_data *)data)->bt_power_setup;

	if (previous != blocked)
		ret = (*power_control)(!blocked);
	if (!ret)
		previous = blocked;
	return ret;
}

static const struct rfkill_ops bluetooth_power_rfkill_ops = {
	.set_block = bluetooth_toggle_radio,
};

#if defined(CONFIG_CNSS) && defined(CONFIG_CLD_LL_CORE)
static ssize_t extldo_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int ret;
	bool enable = false;
	struct cnss_platform_cap cap;

	ret = cnss_get_platform_cap(&cap);
	if (ret) {
		BT_PWR_ERR("Platform capability info from CNSS not available!");
		enable = false;
	} else if (!ret && (cap.cap_flag & CNSS_HAS_EXTERNAL_SWREG)) {
		enable = true;
	}
	return snprintf(buf, 6, "%s", (enable ? "true" : "false"));
}
#else
static ssize_t extldo_show(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, 6, "%s", "false");
}
#endif

static DEVICE_ATTR_RO(extldo);

static int bluetooth_power_rfkill_probe(struct platform_device *pdev)
{
	struct rfkill *rfkill;
	int ret;

	rfkill = rfkill_alloc("bt_power", &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			      &bluetooth_power_rfkill_ops,
			      pdev->dev.platform_data);

	if (!rfkill) {
		dev_err(&pdev->dev, "rfkill allocate failed\n");
		return -ENOMEM;
	}

	/* add file into rfkill0 to handle LDO27 */
	ret = device_create_file(&pdev->dev, &dev_attr_extldo);
	if (ret < 0)
		BT_PWR_ERR("device create file error!");

	/* force Bluetooth off during init to allow for user control */
	rfkill_init_sw_state(rfkill, 1);
	previous = true;

	ret = rfkill_register(rfkill);
	if (ret) {
		dev_err(&pdev->dev, "rfkill register failed=%d\n", ret);
		rfkill_destroy(rfkill);
		return ret;
	}

	platform_set_drvdata(pdev, rfkill);

	return 0;
}

static void bluetooth_power_rfkill_remove(struct platform_device *pdev)
{
	struct rfkill *rfkill;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	rfkill = platform_get_drvdata(pdev);
	if (rfkill)
		rfkill_unregister(rfkill);
	rfkill_destroy(rfkill);
	platform_set_drvdata(pdev, NULL);
}

#define MAX_PROP_SIZE 32
static int bt_dt_parse_vreg_info(struct device *dev,
		struct bt_power_vreg_data **vreg_data, const char *vreg_name)
{
	int len, ret = 0;
	const __be32 *prop;
	char prop_name[MAX_PROP_SIZE];
	struct bt_power_vreg_data *vreg;
	struct device_node *np = dev->of_node;

	BT_PWR_DBG("vreg dev tree parse for %s", vreg_name);

	*vreg_data = NULL;
	snprintf(prop_name, MAX_PROP_SIZE, "%s-supply", vreg_name);
	if (of_parse_phandle(np, prop_name, 0)) {
		vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
		if (!vreg) {
			BT_PWR_ERR("No memory for vreg: %s", vreg_name);
			ret = -ENOMEM;
			goto err;
		}

		vreg->name = vreg_name;

		/* Parse voltage-level from each node */
		snprintf(prop_name, MAX_PROP_SIZE,
				"%s-voltage-level", vreg_name);
		prop = of_get_property(np, prop_name, &len);
		if (!prop || (len != (2 * sizeof(__be32)))) {
			dev_warn(dev, "%s %s property\n",
				prop ? "invalid format" : "no", prop_name);
		} else {
			vreg->low_vol_level = be32_to_cpup(&prop[0]);
			vreg->high_vol_level = be32_to_cpup(&prop[1]);
		}

		/* Parse current-level from each node */
		snprintf(prop_name, MAX_PROP_SIZE,
				"%s-current-level", vreg_name);
		ret = of_property_read_u32(np, prop_name, &vreg->load_uA);
		if (ret < 0) {
			BT_PWR_DBG("%s property is not valid\n", prop_name);
			vreg->load_uA = -1;
			ret = 0;
		}

		*vreg_data = vreg;
		BT_PWR_DBG("%s: vol=[%d %d]uV, current=[%d]uA\n",
			vreg->name, vreg->low_vol_level,
			vreg->high_vol_level,
			vreg->load_uA);
	} else
		BT_PWR_INFO("%s: is not provided in device tree", vreg_name);

err:
	return ret;
}

static int bt_dt_parse_clk_info(struct device *dev,
		struct bt_power_clk_data **clk_data)
{
	int ret = 0;
	struct bt_power_clk_data *clk = NULL;
	struct device_node *np = dev->of_node;

	BT_PWR_DBG("");

	*clk_data = NULL;
	if (of_parse_phandle(np, "clocks", 0)) {
		clk = devm_kzalloc(dev, sizeof(*clk), GFP_KERNEL);
		if (!clk) {
			BT_PWR_ERR("No memory for clocks");
			ret = -ENOMEM;
			goto err;
		}

		/* Allocated 20 bytes size buffer for clock name string */
		clk->name = devm_kzalloc(dev, 20, GFP_KERNEL);

		/* Parse clock name from node */
		ret = of_property_read_string_index(np, "clock-names", 0,
				&(clk->name));
		if (ret < 0) {
			BT_PWR_ERR("reading \"clock-names\" failed");
			return ret;
		}

		clk->clk = devm_clk_get(dev, clk->name);
		if (IS_ERR(clk->clk)) {
			ret = PTR_ERR(clk->clk);
			BT_PWR_ERR("failed to get %s, ret (%d)",
				clk->name, ret);
			clk->clk = NULL;
			return ret;
		}

		*clk_data = clk;
	} else {
		BT_PWR_INFO("clocks is not provided in device tree");
	}

err:
	return ret;
}

static int bt_power_populate_dt_pinfo(struct platform_device *pdev)
{
	int rc;

	BT_PWR_DBG("");

	if (!bt_power_pdata)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		bt_power_pdata->bt_gpio_sys_rst =
			of_get_named_gpio(pdev->dev.of_node,
						"qca,bt-reset-gpio", 0);
		if (bt_power_pdata->bt_gpio_sys_rst < 0)
			BT_PWR_INFO("bt-reset-gpio not provided in devicetree");

		bt_power_pdata->wl_gpio_sys_rst =
			of_get_named_gpio(pdev->dev.of_node,
						"qca,wl-reset-gpio", 0);
		if (bt_power_pdata->wl_gpio_sys_rst < 0)
			BT_PWR_INFO("wl-reset-gpio not provided in devicetree");

		bt_power_pdata->bt_gpio_sw_ctrl  =
			of_get_named_gpio(pdev->dev.of_node,
						"qca,bt-sw-ctrl-gpio",  0);

		bt_power_pdata->bt_gpio_debug  =
			of_get_named_gpio(pdev->dev.of_node,
						"qca,bt-debug-gpio",  0);

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_core,
					"qca,bt-vdd-core");
		if (rc < 0)
			goto err;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_io,
					"qca,bt-vdd-io");
		if (rc < 0)
			goto err;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_xtal,
					"qca,bt-vdd-xtal");
		if (rc < 0)
			goto err;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_pa,
					"qca,bt-vdd-pa");
		if (rc < 0)
			goto err;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_ldo,
					"qca,bt-vdd-ldo");
		if (rc < 0)
			goto err;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_chip_pwd,
					"qca,bt-chip-pwd");
		if (rc < 0)
			goto err;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_aon,
					"qca,bt-vdd-aon");
		if (rc < 0)
			goto err;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_dig,
					"qca,bt-vdd-dig");
		if (rc < 0)
			goto err;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_rfa1,
					"qca,bt-vdd-rfa1");
		if (rc < 0)
			goto err;
		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_rfa2,
					"qca,bt-vdd-rfa2");
		if (rc < 0)
			goto err;

		rc = bt_dt_parse_vreg_info(&pdev->dev,
					&bt_power_pdata->bt_vdd_asd,
					"qca,bt-vdd-asd");
		if (rc < 0)
			goto err;

		rc = bt_dt_parse_clk_info(&pdev->dev,
					&bt_power_pdata->bt_chip_clk);
		if (rc < 0)
			goto err;
	}

	bt_power_pdata->bt_power_setup = bluetooth_power;

	return 0;
err:
	BT_PWR_ERR("%s: Failed with err code: %d",
		__func__, rc);
	return rc;
}

static int bt_power_probe(struct platform_device *pdev)
{
	int ret = 0;
	int itr;

	/* Fill whole array with -2 i.e NOT_AVAILABLE state by default
	 * for any GPIO or Reg handle.
	 */
	for (itr = PWR_SRC_INIT_STATE_IDX;
		itr < BT_POWER_SRC_SIZE; ++itr)
		bt_power_src_status[itr] = PWR_SRC_NOT_AVAILABLE;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	bt_power_pdata =
		kzalloc(sizeof(struct bluetooth_power_platform_data),
			GFP_KERNEL);

	if (!bt_power_pdata) {
		BT_PWR_ERR("Failed to allocate memory");
		return -ENOMEM;
	}

	if (pdev->dev.of_node) {
		ret = bt_power_populate_dt_pinfo(pdev);
		if (ret < 0) {
			BT_PWR_ERR("Failed to populate device tree info");
			goto free_pdata;
		}
		pdev->dev.platform_data = bt_power_pdata;
	} else if (pdev->dev.platform_data) {
		/* Optional data set to default if not provided */
		if (!((struct bluetooth_power_platform_data *)
			(pdev->dev.platform_data))->bt_power_setup)
			((struct bluetooth_power_platform_data *)
				(pdev->dev.platform_data))->bt_power_setup =
						bluetooth_power;

		memcpy(bt_power_pdata, pdev->dev.platform_data,
			sizeof(struct bluetooth_power_platform_data));
		pwr_state = 0;
	} else {
		BT_PWR_ERR("Failed to get platform data");
		goto free_pdata;
	}

	if (bluetooth_power_rfkill_probe(pdev) < 0)
		goto free_pdata;

	btpdev = pdev;

	return 0;

free_pdata:
	kfree(bt_power_pdata);
	return ret;
}

static int bt_power_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s\n", __func__);

	bluetooth_power_rfkill_remove(pdev);

	if (bt_power_pdata->bt_chip_pwd->reg)
		regulator_put(bt_power_pdata->bt_chip_pwd->reg);

	kfree(bt_power_pdata);

	return 0;
}

int bt_register_slimdev(struct device *dev)
{
	BT_PWR_DBG("");
	if (!bt_power_pdata || (dev == NULL)) {
		BT_PWR_ERR("Failed to allocate memory");
		return -EINVAL;
	}
	bt_power_pdata->slim_dev = dev;
	return 0;
}

int get_chipset_version(void)
{
	BT_PWR_DBG("");
	return soc_id;
}

int bt_disable_asd(void)
{
	int rc = 0;
	if (bt_power_pdata->bt_vdd_asd) {
		BT_PWR_INFO("Disabling ASD regulator");
		rc = bt_vreg_disable(bt_power_pdata->bt_vdd_asd);
	} else {
		BT_PWR_INFO("ASD regulator is not configured");
	}
	return rc;
}

static void  set_pwr_srcs_status(int ldo_index,
				struct bt_power_vreg_data *handle)
{
	if (handle) {
		bt_power_src_status[ldo_index] = DEFAULT_INVALID_VALUE;
		if (handle->is_enabled && regulator_is_enabled(handle->reg)) {
			bt_power_src_status[ldo_index] =
				(int)regulator_get_voltage(handle->reg);
			BT_PWR_ERR("%s(%d) value(%d)", handle->name,
				handle, bt_power_src_status[ldo_index]);
		} else {
			BT_PWR_ERR("%s: %s is_enabled %d", __func__,
				handle->name, handle->is_enabled);
		}
	}
}

static void  set_gpios_srcs_status(char *gpio_name,
				int gpio_index, int handle)
{
	if (handle >= 0) {
		bt_power_src_status[gpio_index] = DEFAULT_INVALID_VALUE;
		bt_power_src_status[gpio_index] = gpio_get_value(handle);
		BT_PWR_ERR("%s(%d) value(%d)", gpio_name,
				handle, bt_power_src_status[gpio_index]);
	} else {
		BT_PWR_ERR("%s: %s not configured",
			__func__, gpio_name);
	}
}

static long bt_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0, pwr_cntrl = 0;
	int chipset_version = 0;

	switch (cmd) {
	case BT_CMD_SLIM_TEST:
#if defined CONFIG_BT_SLIM_QCA6390 || defined CONFIG_BTFM_SLIM_WCN3990
		if (!bt_power_pdata->slim_dev) {
			BT_PWR_ERR("slim_dev is null\n");
			return -EINVAL;
		}
		ret = btfm_slim_hw_init(
			bt_power_pdata->slim_dev->platform_data
		);
#endif
		break;
	case BT_CMD_PWR_CTRL:
		pwr_cntrl = (int)arg;
		BT_PWR_ERR("BT_CMD_PWR_CTRL pwr_cntrl:%d", pwr_cntrl);
		if (pwr_state != pwr_cntrl) {
			ret = bluetooth_power(pwr_cntrl);
			if (!ret)
				pwr_state = pwr_cntrl;
		} else {
			BT_PWR_ERR("BT state already:%d no change done\n"
				, pwr_state);
			ret = 0;
		}
		break;
	case BT_CMD_CHIPSET_VERS:
		chipset_version = (int)arg;
		BT_PWR_ERR("unified Current SOC Version : %x", chipset_version);
		if (chipset_version) {
			soc_id = chipset_version;
			if (soc_id == QCA_HSP_SOC_ID_0100 ||
				soc_id == QCA_HSP_SOC_ID_0110 ||
				soc_id == QCA_HSP_SOC_ID_0200 ||
				soc_id == QCA_HSP_SOC_ID_0210 ||
				soc_id == QCA_HSP_SOC_ID_1211) {
				ret = bt_disable_asd();
			}
		} else {
			BT_PWR_ERR("got invalid soc version");
			soc_id = 0;
		}
		break;
	case BT_CMD_CHECK_SW_CTRL:
		BT_PWR_INFO("BT_CMD_CHECK_SW_CTRL");
		/*  Check  if  SW_CTRL  is  asserted  */
		if  (bt_power_pdata->bt_gpio_sw_ctrl > 0)  {
			bt_power_src_status[BT_SW_CTRL_GPIO] =
				DEFAULT_INVALID_VALUE;
			ret  =  gpio_direction_input(
				bt_power_pdata->bt_gpio_sw_ctrl);
			if  (ret)  {
				BT_PWR_ERR("%s:gpio_direction_input api",
					__func__);
				BT_PWR_ERR("%s:failed for SW_CTRL:%d",
					__func__, ret);
			} else {
				bt_power_src_status[BT_SW_CTRL_GPIO] =
					gpio_get_value(
					bt_power_pdata->bt_gpio_sw_ctrl);
				BT_PWR_INFO("bt-sw-ctrl-gpio(%d) value(%d)",
					bt_power_pdata->bt_gpio_sw_ctrl,
					bt_power_src_status[BT_SW_CTRL_GPIO]);
			}
		} else {
			BT_PWR_ERR("bt_gpio_sw_ctrl not configured");
			return -EINVAL;
		}
		break;
	case BT_CMD_GETVAL_POWER_SRCS:
		BT_PWR_ERR("BT_CMD_GETVAL_POWER_SRCS");
		set_gpios_srcs_status("BT_RESET_GPIO", BT_RESET_GPIO_CURRENT,
			bt_power_pdata->bt_gpio_sys_rst);
		set_gpios_srcs_status("SW_CTRL_GPIO", BT_SW_CTRL_GPIO_CURRENT,
			bt_power_pdata->bt_gpio_sw_ctrl);
		set_pwr_srcs_status(BT_VDD_AON_LDO_CURRENT,
			bt_power_pdata->bt_vdd_aon);
		set_pwr_srcs_status(BT_VDD_DIG_LDO_CURRENT,
			bt_power_pdata->bt_vdd_dig);
		set_pwr_srcs_status(BT_VDD_RFA1_LDO_CURRENT,
			bt_power_pdata->bt_vdd_rfa1);
		set_pwr_srcs_status(BT_VDD_RFA2_LDO_CURRENT,
			bt_power_pdata->bt_vdd_rfa2);
		set_pwr_srcs_status(BT_VDD_ASD_LDO_CURRENT,
			bt_power_pdata->bt_vdd_asd);
		set_pwr_srcs_status(BT_VDD_IO_LDO_CURRENT,
			bt_power_pdata->bt_vdd_io);
		set_pwr_srcs_status(BT_VDD_XTAL_LDO_CURRENT,
			bt_power_pdata->bt_vdd_xtal);
		set_pwr_srcs_status(BT_VDD_CORE_LDO_CURRENT,
			bt_power_pdata->bt_vdd_core);
		set_pwr_srcs_status(BT_VDD_PA_LDO_CURRENT,
			bt_power_pdata->bt_vdd_pa);
		set_pwr_srcs_status(BT_VDD_LDO_CURRENT,
			bt_power_pdata->bt_vdd_ldo);
		if (copy_to_user((void __user *)arg,
			bt_power_src_status, sizeof(bt_power_src_status))) {
			BT_PWR_ERR("%s: copy to user failed\n", __func__);
			ret = -EFAULT;
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static struct platform_driver bt_power_driver = {
	.probe = bt_power_probe,
	.remove = bt_power_remove,
	.driver = {
		.name = "bt_power",
		.of_match_table = bt_power_match_table,
	},
};

static const struct file_operations bt_dev_fops = {
	.unlocked_ioctl = bt_ioctl,
	.compat_ioctl = bt_ioctl,
};

static int __init bluetooth_power_init(void)
{
	int ret;

	ret = platform_driver_register(&bt_power_driver);

	bt_major = register_chrdev(0, "bt", &bt_dev_fops);
	if (bt_major < 0) {
		BT_PWR_ERR("failed to allocate char dev\n");
		goto chrdev_unreg;
	}

	bt_class = class_create(THIS_MODULE, "bt-dev");
	if (IS_ERR(bt_class)) {
		BT_PWR_ERR("coudn't create class");
		goto chrdev_unreg;
	}


	if (device_create(bt_class, NULL, MKDEV(bt_major, 0),
		NULL, "btpower") == NULL) {
		BT_PWR_ERR("failed to allocate char dev\n");
		goto chrdev_unreg;
	}
	return 0;

chrdev_unreg:
	unregister_chrdev(bt_major, "bt");
	class_destroy(bt_class);
	return ret;
}

static void __exit bluetooth_power_exit(void)
{
	platform_driver_unregister(&bt_power_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM Bluetooth power control driver");

module_init(bluetooth_power_init);
module_exit(bluetooth_power_exit);
