/*
 * arch/arm/mach-tegra/include/mach/tegra-bb-power.h
 *
 * Copyright (C) 2011 NVIDIA Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define GPIO_INVALID UINT_MAX

union tegra_bb_gpio_id {
	struct {
		int mdm_reset;
		int mdm_on;
		int ap2mdm_ack;
		int mdm2ap_ack;
		int ap2mdm_ack2;
		int mdm2ap_ack2;
		int rsvd1;
		int rsvd2;
	} generic;
	struct {
		int reset;
		int pwron;
		int awr;
		int cwr;
		int spare;
		int wdi;
		int rsvd1;
		int rsvd2;
	} oem1;
};

typedef struct platform_device* (*ehci_register_cb)(struct platform_device *);
typedef void (*ehci_unregister_cb)(struct platform_device **);

struct tegra_bb_pdata {
	/* List of platform gpios for modem */
	union tegra_bb_gpio_id *id;
	/* Ehci device pointer */
	struct platform_device *device;
	/* Ehci register callback */
	ehci_register_cb ehci_register;
	/* Ehci unregister callback */
	ehci_unregister_cb ehci_unregister;
	/* Baseband ID */
	int bb_id;
	/* HSIC rail regulator name. Provide a name if --
	rail is shared and the co-owner will turn it off when done */
	char *regulator;
};
