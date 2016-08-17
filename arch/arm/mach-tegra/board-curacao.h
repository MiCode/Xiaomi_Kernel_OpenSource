/*
 * arch/arm/mach-tegra/board-curacao.h
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _MACH_TEGRA_BOARD_CURACAO_H
#define _MACH_TEGRA_BOARD_CURACAO_H

#include <mach/gpio.h>
#include <mach/irqs.h>
#include <linux/mfd/max77663-core.h>
#include "gpio-names.h"

/* External peripheral act as gpio */
/* MAX77663 GPIO */
#define MAX77663_GPIO_BASE      TEGRA_NR_GPIOS
#define MAX77663_GPIO_END       (MAX77663_GPIO_BASE + MAX77663_GPIO_NR)

/*****************Interrupt tables ******************/
/* External peripheral act as interrupt controller */
/* MAX77663 IRQs */
#define MAX77663_IRQ_BASE       TEGRA_NR_IRQS
#define MAX77663_IRQ_END        (MAX77663_IRQ_BASE + MAX77663_IRQ_NR)
#define MAX77663_IRQ_ACOK_RISING MAX77663_IRQ_ONOFF_ACOK_RISING

int curacao_regulator_init(void);
int curacao_suspend_init(void);

int curacao_sdhci_init(void);
int curacao_pinmux_init(void);
int curacao_panel_init(void);
int curacao_sensors_init(void);
int curacao_emc_init(void);

#ifdef CONFIG_TEGRA_SIMULATION_PLATFORM
#define CURACAO_BOARD_NAME "curacao_sim"
int __init curacao_power_off_init(void);
#else
#define CURACAO_BOARD_NAME "curacao"
static inline int curacao_power_off_init(void)
{
	return 0;
}
#endif

#endif
