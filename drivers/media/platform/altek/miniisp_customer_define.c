/*
 * File: miniisp_customer_define.c
 * Description: Mini ISP sample codes
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 * 2017/03/14 LouisWang; Initial version
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */



/******Include File******/
/* Linux headers*/
#include <linux/delay.h>
#include  <linux/of_gpio.h>

#include "include/miniisp_customer_define.h"
#include "include/miniisp.h"
#include "include/miniisp_ctrl.h"

#define MINI_ISP_LOG_TAG "[miniisp_customer_define]"


extern void mini_isp_poweron(void)
{
	errcode ret = 0;
	void *devdata;
	struct misp_global_variable *dev_global_variable;

	devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);
	dev_global_variable = get_mini_isp_global_variable();
	misp_info("%s no FSM", __func__);
	misp_err("[miniISP]mini_isp_poweron");

#if (ISR_MECHANISM == INTERRUPT_METHOD)
	if (IRQ_GPIO != NULL) {
		ret = request_threaded_irq(
			gpio_to_irq(dev_global_variable->irq_gpio),
			NULL, mini_isp_irq,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			"mini_isp", devdata);
	}

	if (ret != 0) {
		misp_err("%s err, %x", __func__, ret);
		return;
	}

	misp_info("%s request_threaded_irq succeed, irq_gpio: %d, irg_num %d"
				, __func__, dev_global_variable->irq_gpio,
				gpio_to_irq(dev_global_variable->irq_gpio));
#endif

	if (RESET_GPIO != NULL) {
		gpio_direction_output(dev_global_variable->reset_gpio, 0);
		gpio_set_value(dev_global_variable->reset_gpio, 0);
		msleep(20);
	}
	if (VCC1_GPIO != NULL)
		gpio_set_value(dev_global_variable->vcc1_gpio, 0);
	if (VCC2_GPIO != NULL)
		gpio_set_value(dev_global_variable->vcc2_gpio, 0);
	if (VCC3_GPIO != NULL)
		gpio_set_value(dev_global_variable->vcc3_gpio, 0);

	msleep(20);

	if (VCC1_GPIO != NULL)
		gpio_set_value(dev_global_variable->vcc1_gpio, 1);
	if (VCC2_GPIO != NULL)
		gpio_set_value(dev_global_variable->vcc2_gpio, 1);
	if (VCC3_GPIO != NULL)
		gpio_set_value(dev_global_variable->vcc3_gpio, 1);

	msleep(20);
	if (ISP_CLK != NULL)
		if (clk_prepare_enable(dev_global_variable->isp_clk) < 0)
			misp_err("mini_isp_poweron clk_prepare_enable failed");

	if (RESET_GPIO != NULL) {
		gpio_direction_output(dev_global_variable->reset_gpio, 1);
		gpio_set_value(dev_global_variable->reset_gpio, 1);
		msleep(20);
	}

	if (RESET_GPIO != NULL)
		misp_err("%s -reset_gpio gpio_get_value = %d", __func__,
			gpio_get_value(dev_global_variable->reset_gpio));
	if (VCC1_GPIO != NULL)
		misp_err("%s -vcc1_gpio gpio_get_value = %d", __func__,
			gpio_get_value(dev_global_variable->vcc1_gpio));
	if (VCC2_GPIO != NULL)
		misp_err("%s -vcc2_gpio gpio_get_value = %d", __func__,
			gpio_get_value(dev_global_variable->vcc2_gpio));
	if (VCC3_GPIO != NULL)
		misp_err("%s -vcc3_gpio gpio_get_value = %d", __func__,
			gpio_get_value(dev_global_variable->vcc3_gpio));

	misp_err("%s - leave", __func__);

	if (ret != 0) {
		misp_err("%s err, %x", __func__, ret);
		return;
	}

	dev_global_variable->before_booting = 1;
	dev_global_variable->be_set_to_bypass = 0;
}
EXPORT_SYMBOL(mini_isp_poweron);

extern void mini_isp_poweroff(void)
{
	int ret = 0;
	struct misp_global_variable *dev_global_variable;
	void *devdata;

	devdata = get_mini_isp_intf(MINIISP_I2C_SLAVE);
	dev_global_variable = get_mini_isp_global_variable();

	misp_info("%s no FSM", __func__);
	misp_err("[miniISP]mini_isp_poweroff");

	if (RESET_GPIO != NULL)
		gpio_set_value(dev_global_variable->reset_gpio, 0);
	msleep(20);
	if (VCC1_GPIO != NULL)
		gpio_set_value(dev_global_variable->vcc1_gpio, 0);
	if (VCC2_GPIO != NULL)
		gpio_set_value(dev_global_variable->vcc2_gpio, 0);
	if (VCC3_GPIO != NULL)
		gpio_set_value(dev_global_variable->vcc3_gpio, 0);
	if (ISP_CLK != NULL)
		clk_disable_unprepare(dev_global_variable->isp_clk);
	msleep(20);

#if (ISR_MECHANISM == INTERRUPT_METHOD)
	if (IRQ_GPIO != NULL)
		free_irq(gpio_to_irq(dev_global_variable->irq_gpio), devdata);

#endif

	if (ret != 0) {
		misp_err("%s err, %x", __func__, ret);
		return;
	}

	dev_global_variable->be_set_to_bypass = 0;
	dev_global_variable->before_booting = 1;

	misp_info("%s - X", __func__);
}
EXPORT_SYMBOL(mini_isp_poweroff);

extern void mini_isp_eeprom_wpon(void)
{
	struct misp_global_variable *dev_global_variable;

	misp_err("[miniISP]mini_isp_eeprom_wpon");
	if (WP_GPIO != NULL) {
		dev_global_variable = get_mini_isp_global_variable();
		gpio_set_value(dev_global_variable->wp_gpio, 1);
	}
	misp_info("%s - X", __func__);
}
EXPORT_SYMBOL(mini_isp_eeprom_wpon);

extern void mini_isp_eeprom_wpoff(void)
{
	struct misp_global_variable *dev_global_variable;

	misp_err("[miniISP]mini_isp_eeprom_wpoff");
	if (WP_GPIO != NULL) {
		dev_global_variable = get_mini_isp_global_variable();
		gpio_set_value(dev_global_variable->wp_gpio, 0);
	}
	misp_info("%s - X", __func__);
}
EXPORT_SYMBOL(mini_isp_eeprom_wpoff);

extern int mini_isp_gpio_init(struct device *dev,
			struct misp_data *drv_data,
			struct misp_global_variable *drv_global_variable)
{
	int ret = 0;

	if (VCC1_GPIO != NULL) {
		drv_global_variable->vcc1_gpio =
			of_get_named_gpio(dev->of_node, VCC1_GPIO, 0);
		misp_info("%s - probe vcc1-gpios = %d", __func__,
			drv_global_variable->vcc1_gpio);

		ret = devm_gpio_request(dev,
			drv_global_variable->vcc1_gpio, VCC1_GPIO);
		if (ret) {
			misp_err("%s -step 4. request vcc1-gpio error",
				__func__);
			goto err_gpio1_config;
		}

		gpio_direction_output(drv_global_variable->vcc1_gpio, 1);
		msleep(20);
		gpio_set_value(drv_global_variable->vcc1_gpio, 1);
		msleep(20);
	}

	if (VCC2_GPIO != NULL) {
		drv_global_variable->vcc2_gpio = of_get_named_gpio(
			dev->of_node, VCC2_GPIO, 0);
		misp_info("%s - probe vcc2-gpios = %d", __func__,
			drv_global_variable->vcc2_gpio);

		ret = devm_gpio_request(dev,
			drv_global_variable->vcc2_gpio, VCC2_GPIO);
		if (ret) {
			misp_err("%s -step 4. request vcc2-gpios error",
				__func__);
			goto err_gpio2_config;
		}

		gpio_direction_output(drv_global_variable->vcc2_gpio, 1);
		msleep(20);
		gpio_set_value(drv_global_variable->vcc2_gpio, 1);
		msleep(20);
	}

	if (VCC3_GPIO != NULL) {
		drv_global_variable->vcc3_gpio = of_get_named_gpio(
			dev->of_node, VCC3_GPIO, 0);
		misp_err("%s - probe vcc3-gpios = %d", __func__,
					drv_global_variable->vcc3_gpio);

		ret = devm_gpio_request(dev,
			drv_global_variable->vcc3_gpio, VCC3_GPIO);
		if (ret) {
			misp_err("%s -step 4. request vcc3-gpio error",
				__func__);
			goto err_gpio_config;
		}

		gpio_direction_output(drv_global_variable->vcc3_gpio, 1);
		gpio_set_value(drv_global_variable->vcc3_gpio, 1);
		msleep(20);

	}

	if (WP_GPIO != NULL) {
		drv_global_variable->wp_gpio = of_get_named_gpio(
			dev->of_node, WP_GPIO, 0);
		misp_info("%s - probe wp-gpios = %d", __func__,
					drv_global_variable->wp_gpio);

		ret = devm_gpio_request(dev,
			drv_global_variable->wp_gpio, WP_GPIO);
		if (ret) {
			misp_err("%s -step 4. request wp-gpio error",
				__func__);
			goto err_gpio_config;
		}

		gpio_direction_output(drv_global_variable->wp_gpio, 1);
		gpio_set_value(drv_global_variable->wp_gpio, 1);
		msleep(20);

	}

	if (ISP_CLK != NULL) {
		drv_global_variable->isp_clk = devm_clk_get(dev,
						ISP_CLK);
		misp_err("clk_ptr = %p", drv_global_variable->isp_clk);
		ret = clk_set_rate(drv_global_variable->isp_clk, 19200000L);
		if (ret < 0)
			misp_err("clk_set_rate failed, not fatal\n");

		misp_err("clk_get_rate %ld\n", clk_get_rate(
					drv_global_variable->isp_clk));
		ret = clk_prepare_enable(drv_global_variable->isp_clk);
		if (ret < 0) {
			misp_err("clk_prepare_enable failed\n");
			goto err_clk_config;
		}
		msleep(20);
	}

	if (RESET_GPIO != NULL) {
		drv_global_variable->reset_gpio =
			of_get_named_gpio(dev->of_node, RESET_GPIO, 0);
		misp_info("%s - probe reset_gpio = %d", __func__,
			drv_global_variable->reset_gpio);

		ret = devm_gpio_request(dev,
			drv_global_variable->reset_gpio, RESET_GPIO);
		if (ret) {
			misp_err("%s -step 4. request reset gpio error",
				__func__);
			goto err_reset_config;
		}

		gpio_direction_output(drv_global_variable->reset_gpio, 0);
		gpio_set_value(drv_global_variable->reset_gpio, 0);
		msleep(20);

	}

#if (ISR_MECHANISM == INTERRUPT_METHOD)
	if (IRQ_GPIO != NULL) {

		drv_global_variable->irq_gpio =
			of_get_named_gpio(dev->of_node, IRQ_GPIO, 0);

		misp_info("%s - probe irq_gpio = %d",
			__func__, drv_global_variable->irq_gpio);

		ret = gpio_request(drv_global_variable->irq_gpio, IRQ_GPIO);
		if (ret) {
			misp_err("%s -step 4. request irq gpio error",
				__func__);
			goto err_irq_config;
		}
		gpio_direction_input(drv_global_variable->irq_gpio);

		drv_global_variable->irq_num =
			gpio_to_irq(drv_global_variable->irq_gpio);

		misp_err("%s - probe spi->irq = %d %d ",
			__func__, drv_global_variable->irq_num,
			gpio_to_irq(drv_global_variable->irq_gpio));

		ret = request_threaded_irq(
			drv_global_variable->irq_num, NULL, mini_isp_irq,
		IRQF_TRIGGER_RISING | IRQF_ONESHOT, "mini_isp", drv_data);

		if (ret) {
			misp_err("%s - step4. probe - request irq error",
				__func__);
			goto err_dev_attr;
		}
		misp_info("%s - step4 done. irq number:%d", __func__,
			drv_global_variable->irq_num);

		free_irq(drv_global_variable->irq_num, drv_data);
	}
#endif

	/*step 5:other additional config*/

	misp_info("%s - step5 done", __func__);

	if (RESET_GPIO != NULL) {
		gpio_direction_output(drv_global_variable->reset_gpio, 1);
		gpio_set_value(drv_global_variable->reset_gpio, 1);
		msleep(20);
	}

	return ret;

#if (ISR_MECHANISM == INTERRUPT_METHOD)
err_dev_attr:
	free_irq(drv_global_variable->irq_num, drv_data);

err_irq_config:
	if (IRQ_GPIO != NULL)
		gpio_free(drv_global_variable->irq_gpio);
#endif

err_reset_config:
	if (RESET_GPIO != NULL)
		gpio_free(drv_global_variable->reset_gpio);

err_clk_config:
	if (ISP_CLK != NULL)
		clk_disable_unprepare(drv_global_variable->isp_clk);

err_gpio_config:
	if (VCC2_GPIO != NULL)
		gpio_free(drv_global_variable->vcc2_gpio);
err_gpio2_config:
	if (VCC1_GPIO != NULL)
		gpio_free(drv_global_variable->vcc1_gpio);

err_gpio1_config:

	return ret;
}
