/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/
#include <linux/gpio.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel-mid.h>
#include "../hdmi_pipe.h"

#define MSIC_PCI_DEVICE_ID 0x11A6

#define MSIC_HPD_GPIO_PIN 16
#define MSIC_LS_EN_GPIO_PIN 177

#define CLOCK_MIN 25174
#define CLOCK_MAX 148500

static void gpio_configure_edid_read(struct hdmi_platform_config *config);

int get_gpio_pin(struct hdmi_platform_config *config)
{
	int err = 0;

	config->gpio_hpd_pin = get_gpio_by_name(MSIC_HPD_GPIO_PIN_NAME);
	if (config->gpio_hpd_pin == -1) {
		config->gpio_hpd_pin = MSIC_HPD_GPIO_PIN;
		pr_debug("get_gpio_by_name failed! Use default pin %d\n",
			MSIC_HPD_GPIO_PIN);
	}

	if (INTEL_MID_BOARD(2, PHONE, MOFD, MP, PRO) ||
		INTEL_MID_BOARD(2, PHONE, MOFD, MP, ENG)) {
		/* VV board uses GPIO pin 192 for Level shifter HDMI_LS_EN */
		config->gpio_ls_en_pin =
				get_gpio_by_name(MSIC_LS_EN_GPIO_PIN_NAME);
		if (config->gpio_ls_en_pin == -1) {
			config->gpio_ls_en_pin = MSIC_LS_EN_GPIO_PIN;
			pr_debug("get_gpio_by_name failed!Use default pin %d\n",
					MSIC_LS_EN_GPIO_PIN);
		}
	} else {
		/* PRh uses GPIO pin 177 for Level shifter HDMI_LS_EN */
		config->gpio_ls_en_pin = MSIC_LS_EN_GPIO_PIN;
	}

	if (gpio_request(config->gpio_ls_en_pin, "HDMI_LS_EN")) {
		pr_err("%s: Unable to request gpio %d\n", __func__,
		       config->gpio_ls_en_pin);
		err = -EIO;
		goto out_err0;
	}

	if (!gpio_is_valid(config->gpio_ls_en_pin)) {
		pr_err("%s: Unable to validate gpio %d\n", __func__,
		       config->gpio_ls_en_pin);
		err = -EIO;
		gpio_free(config->gpio_ls_en_pin);
		goto out_err0;
	}

	if (gpio_request(config->gpio_hpd_pin, "hdmi_hpd")) {
		pr_err("%s: Unable to request gpio %d\n", __func__,
			config->gpio_hpd_pin);
		err = -EIO;
		goto out_err0;
	}

	if (gpio_direction_input(config->gpio_hpd_pin)) {
		pr_err("%s: Unable to set gpio %d as input\n", __func__,
		config->gpio_hpd_pin);
		err = -EIO;
		gpio_free(config->gpio_hpd_pin);
		goto out_err0;
	}

	config->irq_number = gpio_to_irq(config->gpio_hpd_pin);
	pr_err("%s: IRQ number assigned = %d for HDMI HPD device\n",
		__func__, config->irq_number);
out_err0:
	return err;
}

int mofd_get_platform_configs(struct hdmi_platform_config *config)
{
	int res = 0;
	res = get_gpio_pin(config);
	if (res)
		goto out_err;

	config->last_pin_value = -1;
	config->pci_device_id = MSIC_PCI_DEVICE_ID;
	config->min_clock = CLOCK_MIN;
	config->max_clock = CLOCK_MAX;
out_err:
	return res;
}

bool mofd_hdmi_get_cable_status(struct hdmi_platform_config *config)
{
	if (config == NULL)
		return false;

	/* Read HDMI cable status from GPIO */
	/* For Moorefield, it is required that SW pull up or pull down the
	 * LS_OE GPIO pin based on cable status. This is needed before
	 * performing any EDID read operation on Moorefield.
	 */
	gpio_configure_edid_read(config);

	if (gpio_get_value(config->gpio_hpd_pin) == 0)
		return  false;
	else
		return  true;
}

static void gpio_configure_edid_read(struct hdmi_platform_config *config)
{
	int current_pin_value;

	/* TODO: err handling here */
	if (config == NULL)
		return;

	current_pin_value = gpio_get_value(config->gpio_hpd_pin);
	if (current_pin_value == config->last_pin_value)
		return;

	config->last_pin_value = current_pin_value;

	if (current_pin_value == 0)
		gpio_set_value(config->gpio_ls_en_pin, 0);
	else
		gpio_set_value(config->gpio_ls_en_pin, 1);

	pr_debug("%s: MSIC_LS_OE pin = %d (%d)\n", __func__,
		 gpio_get_value(config->gpio_ls_en_pin), current_pin_value);
}


bool mofd_hdmi_enable_hpd(bool enable)
{
	u8 pin = 0;

	/* see ShadyCove PMIC spec and board schema */
	if (INTEL_MID_BOARD(2, PHONE, MOFD, MP, PRO) ||
		INTEL_MID_BOARD(2, PHONE, MOFD, MP, ENG)) {
		/* VV board uses GPIO1 for CT_CP_HPD */
		pin = 0x7f;
	} else {
		/* PRx uses GPIO0 for CT_CP_HPD */
		pin = 0x7e;
	}

	if (enable)
		intel_scu_ipc_iowrite8(pin, 0x31);
	else
		intel_scu_ipc_iowrite8(pin, 0x30);
	return true;
}
