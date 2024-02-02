/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>

#include "stmmac.h"
#include "dwmac-qcom-ethqos.h"

#define EMAC_GDSC_EMAC_NAME "gdsc_emac"
#define EMAC_VREG_RGMII_NAME "vreg_rgmii"
#define EMAC_VREG_EMAC_PHY_NAME "vreg_emac_phy"
#define EMAC_VREG_RGMII_IO_PADS_NAME "vreg_rgmii_io_pads"
#define EMAC_PIN_PPS0 "dev-emac_pin_pps_0"


static int setup_gpio_input_common
	(struct device *dev, const char *name, int *gpio)
{
	int ret = 0;

	if (of_find_property(dev->of_node, name, NULL)) {
		*gpio = ret = of_get_named_gpio(dev->of_node, name, 0);
		if (ret >= 0) {
			ret = gpio_request(*gpio, name);
			if (ret) {
				ETHQOSERR("%s: Can't get GPIO %s, ret = %d\n",
					  name, *gpio);
				*gpio = -1;
				return ret;
			}

			ret = gpio_direction_input(*gpio);
			if (ret) {
				ETHQOSERR(
				   "%s: Can't set GPIO %s direction, ret = %d\n",
				   name, ret);
				return ret;
			}
		} else {
			if (ret == -EPROBE_DEFER)
				ETHQOSERR("get EMAC_GPIO probe defer\n");
			else
				ETHQOSERR("can't get gpio %s ret %d\n", name,
					  ret);
			return ret;
		}
	} else {
		ETHQOSERR("can't find gpio %s\n", name);
		ret = -EINVAL;
	}

	return ret;
}

int ethqos_init_reqgulators(struct qcom_ethqos *ethqos)
{
	int ret = 0;

	if (of_property_read_bool(ethqos->pdev->dev.of_node,
				  "gdsc_emac-supply")) {
		ethqos->gdsc_emac =
		devm_regulator_get(&ethqos->pdev->dev, EMAC_GDSC_EMAC_NAME);
		if (IS_ERR(ethqos->gdsc_emac)) {
			ETHQOSERR("Can not get <%s>\n", EMAC_GDSC_EMAC_NAME);
			return PTR_ERR(ethqos->gdsc_emac);
		}

		ret = regulator_enable(ethqos->gdsc_emac);
		if (ret) {
			ETHQOSERR("Can not enable <%s>\n", EMAC_GDSC_EMAC_NAME);
			goto reg_error;
		}

		ETHQOSDBG("Enabled <%s>\n", EMAC_GDSC_EMAC_NAME);
	}

	if (of_property_read_bool(ethqos->pdev->dev.of_node,
				  "vreg_emac_phy-supply")) {
		ethqos->reg_emac_phy =
		devm_regulator_get(&ethqos->pdev->dev, EMAC_VREG_EMAC_PHY_NAME);
		if (IS_ERR(ethqos->reg_emac_phy)) {
			ETHQOSERR("Can not get <%s>\n",
				  EMAC_VREG_EMAC_PHY_NAME);
			return PTR_ERR(ethqos->reg_emac_phy);
		}

		ret = regulator_enable(ethqos->reg_emac_phy);
		if (ret) {
			ETHQOSERR("Can not enable <%s>\n",
				  EMAC_VREG_EMAC_PHY_NAME);
			goto reg_error;
		}

		ETHQOSDBG("Enabled <%s>\n", EMAC_VREG_EMAC_PHY_NAME);
	}

	if (of_property_read_bool(ethqos->pdev->dev.of_node,
				  "vreg_rgmii_io_pads-supply")) {
		ethqos->reg_rgmii_io_pads = devm_regulator_get
		(&ethqos->pdev->dev, EMAC_VREG_RGMII_IO_PADS_NAME);
		if (IS_ERR(ethqos->reg_rgmii_io_pads)) {
			ETHQOSERR("Can not get <%s>\n",
				  EMAC_VREG_RGMII_IO_PADS_NAME);
			return PTR_ERR(ethqos->reg_rgmii_io_pads);
		}

		ret = regulator_enable(ethqos->reg_rgmii_io_pads);
		if (ret) {
			ETHQOSERR("Can not enable <%s>\n",
				  EMAC_VREG_RGMII_IO_PADS_NAME);
			goto reg_error;
		}

		ETHQOSDBG("Enabled <%s>\n", EMAC_VREG_RGMII_IO_PADS_NAME);
	}

	if (of_property_read_bool(ethqos->pdev->dev.of_node,
				  "vreg_rgmii-supply") && (2500000 ==
		   regulator_get_voltage(ethqos->reg_rgmii_io_pads))) {
		ethqos->reg_rgmii =
		devm_regulator_get(&ethqos->pdev->dev, EMAC_VREG_RGMII_NAME);
		if (IS_ERR(ethqos->reg_rgmii)) {
			ETHQOSERR("Can not get <%s>\n", EMAC_VREG_RGMII_NAME);
			return PTR_ERR(ethqos->reg_rgmii);
		}

		ret = regulator_enable(ethqos->reg_rgmii);
		if (ret) {
			ETHQOSERR("Can not enable <%s>\n",
				  EMAC_VREG_RGMII_NAME);
			goto reg_error;
		}

		ETHQOSDBG("Enabled <%s>\n", EMAC_VREG_RGMII_NAME);
	}

	return ret;

reg_error:
	ETHQOSERR("%s failed\n", __func__);
	ethqos_disable_regulators(ethqos);
	return ret;
}

void ethqos_disable_regulators(struct qcom_ethqos *ethqos)
{
	if (ethqos->reg_rgmii) {
		regulator_disable(ethqos->reg_rgmii);
		devm_regulator_put(ethqos->reg_rgmii);
		ethqos->reg_rgmii = NULL;
	}

	if (ethqos->reg_emac_phy) {
		regulator_disable(ethqos->reg_emac_phy);
		devm_regulator_put(ethqos->reg_emac_phy);
		ethqos->reg_emac_phy = NULL;
	}

	if (ethqos->reg_rgmii_io_pads) {
		regulator_disable(ethqos->reg_rgmii_io_pads);
		devm_regulator_put(ethqos->reg_rgmii_io_pads);
		ethqos->reg_rgmii_io_pads = NULL;
	}

	if (ethqos->gdsc_emac) {
		regulator_disable(ethqos->gdsc_emac);
		devm_regulator_put(ethqos->gdsc_emac);
		ethqos->gdsc_emac = NULL;
	}
}

void ethqos_reset_phy_enable_interrupt(struct qcom_ethqos *ethqos)
{
	struct stmmac_priv *priv = qcom_ethqos_get_priv(ethqos);
	struct phy_device *phydev = priv->dev->phydev;

	/* reset the phy so that it's ready */
	if (priv->mii) {
		ETHQOSERR("do mdio reset\n");
		stmmac_mdio_reset(priv->mii);
	}
	/*Enable phy interrupt*/
	if (phy_intr_en && phydev) {
		ETHQOSDBG("PHY interrupt Mode enabled\n");
		phydev->irq = PHY_IGNORE_INTERRUPT;
		phydev->interrupts =  PHY_INTERRUPT_ENABLED;

		if (phydev->drv->config_intr &&
		    !phydev->drv->config_intr(phydev)) {
			ETHQOSERR("config_phy_intr successful after phy on\n");
		}
		qcom_ethqos_request_phy_wol(priv->plat);
	} else if (!phy_intr_en) {
		phydev->irq = PHY_POLL;
		ETHQOSDBG("PHY Polling Mode enabled\n");
	} else {
		ETHQOSERR("phydev is null , intr value=%d\n", phy_intr_en);
	}
}

int ethqos_phy_power_on(struct qcom_ethqos *ethqos)
{
	int ret = 0;

	if (ethqos->reg_emac_phy) {
		ret = regulator_enable(ethqos->reg_emac_phy);
		if (ret) {
			ETHQOSERR("Can not enable <%s>\n",
				  EMAC_VREG_EMAC_PHY_NAME);
			return ret;
		}
		ethqos->phy_state = PHY_IS_ON;
	} else {
		ETHQOSERR("reg_emac_phy is NULL\n");
	}
	return ret;
}

void  ethqos_phy_power_off(struct qcom_ethqos *ethqos)
{
	if (ethqos->reg_emac_phy) {
		regulator_disable(ethqos->reg_emac_phy);
		ethqos->phy_state = PHY_IS_OFF;
	} else {
		ETHQOSERR("reg_emac_phy is NULL\n");
	}
}

void ethqos_free_gpios(struct qcom_ethqos *ethqos)
{
	if (gpio_is_valid(ethqos->gpio_phy_intr_redirect))
		gpio_free(ethqos->gpio_phy_intr_redirect);
	ethqos->gpio_phy_intr_redirect = -1;
}

int ethqos_init_pinctrl(struct device *dev)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_state;
	int i = 0;
	int num_names;
	const char *name;
	int ret = 0;

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		ETHQOSERR("Failed to get pinctrl, err = %d\n", ret);
		return ret;
	}

	num_names = of_property_count_strings(dev->of_node, "pinctrl-names");
	if (num_names < 0) {
		dev_err(dev, "Cannot parse pinctrl-names: %d\n", num_names);
		return num_names;
	}

	for (i = 0; i < num_names; i++) {
		ret = of_property_read_string_index(
			dev->of_node, "pinctrl-names", i, &name);

		if (!strcmp(name, PINCTRL_STATE_DEFAULT))
			continue;

		pinctrl_state = pinctrl_lookup_state(pinctrl, name);
		if (IS_ERR_OR_NULL(pinctrl_state)) {
			ret = PTR_ERR(pinctrl_state);
			ETHQOSERR("lookup_state %s failed %d\n", name, ret);
			return ret;
		}

		ETHQOSDBG("pinctrl_lookup_state %s succeded\n", name);

		ret = pinctrl_select_state(pinctrl, pinctrl_state);
		if (ret) {
			ETHQOSERR("select_state %s failed %d\n", name, ret);
			return ret;
		}

		ETHQOSDBG("pinctrl_select_state %s succeded\n", name);
	}

	return ret;
}

int ethqos_init_gpio(struct qcom_ethqos *ethqos)
{
	int ret = 0;

	ethqos->gpio_phy_intr_redirect = -1;

	ret = ethqos_init_pinctrl(&ethqos->pdev->dev);
	if (ret) {
		ETHQOSERR("ethqos_init_pinctrl failed");
		return ret;
	}

	ret = setup_gpio_input_common(
			&ethqos->pdev->dev, "qcom,phy-intr-redirect",
			&ethqos->gpio_phy_intr_redirect);

	if (ret) {
		ETHQOSERR("Failed to setup <%s> gpio\n",
			  "qcom,phy-intr-redirect");
		goto gpio_error;
	}

	return ret;

gpio_error:
	ethqos_free_gpios(ethqos);
	return ret;
}
