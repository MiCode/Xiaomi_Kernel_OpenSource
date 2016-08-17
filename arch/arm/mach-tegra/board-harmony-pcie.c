/*
 * arch/arm/mach-tegra/board-harmony-pcie.c
 *
 * Copyright (C) 2010 CompuLab, Ltd.
 * Mike Rapoport <mike@compulab.co.il>
 *
 * Copyright (C) 2011-2012 NVIDIA Corporation.
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

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>

#include <asm/mach-types.h>

#include <mach/pci.h>
#include "devices.h"
#include "board.h"
#include "board-harmony.h"

#ifdef CONFIG_TEGRA_PCI

/* GPIO 3 of the PMIC */
#define EN_VDD_1V05_GPIO	(TEGRA_NR_GPIOS + 2)

static struct tegra_pci_platform_data harmony_pci_platform_data = {
	.port_status[0]	= 1,
	.port_status[1]	= 1,
	.use_dock_detect	= 0,
	.gpio		= 0,
};

int __init harmony_pcie_init(void)
{
	struct regulator *regulator = NULL;
	int err;

	if (!machine_is_harmony())
		return 0;

	err = gpio_request(TEGRA_GPIO_EN_VDD_1V05_GPIO, "EN_VDD_1V05");
	if (err)
		return err;

	gpio_direction_output(TEGRA_GPIO_EN_VDD_1V05_GPIO, 1);

	regulator = regulator_get(NULL, "pex_clk");
	if (IS_ERR_OR_NULL(regulator))
		goto err_reg;

	regulator_enable(regulator);

	tegra_pci_device.dev.platform_data = &harmony_pci_platform_data;
	platform_device_register(&tegra_pci_device);

	return 0;

	regulator_disable(regulator);
	regulator_put(regulator);
err_reg:
	gpio_free(TEGRA_GPIO_EN_VDD_1V05_GPIO);

	return err;
}

#endif
