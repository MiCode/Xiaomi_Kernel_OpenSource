/*
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/platform_data/tegra_ahci.h>

#include "board.h"
#include "board-ardbeg.h"
#include "devices.h"
#include "iomap.h"
#include "tegra-board-id.h"

#ifdef CONFIG_SATA_AHCI_TEGRA
static struct tegra_ahci_platform_data tegra_ahci_platform_data0 = {
	.gen2_rx_eq = -1,
	.pexp_gpio = PMU_TCA6416_GPIO(9),
};
#endif

void arbdeg_sata_clk_gate(void)
{
	u32 val;
	val = readl(IO_ADDRESS(CLK_RST_CNTRL_RST_DEV_W_SET));
	if (val & SET_CEC_RST)
		writel(0x108, IO_ADDRESS(CLK_RST_CNTRL_RST_DEV_V_SET));
	val = readl(IO_ADDRESS(CLK_RST_CNTRL_RST_DEV_W_SET));
	while (val & SET_CEC_RST)
		val = readl(IO_ADDRESS(CLK_RST_CNTRL_RST_DEV_W_SET));
}

int __init ardbeg_sata_init(void)
{
#ifdef CONFIG_SATA_AHCI_TEGRA
	struct board_info board_info;
	tegra_get_board_info(&board_info);
	if ((board_info.board_id != BOARD_PM358) &&
	    (board_info.board_id != BOARD_PM359))
		tegra_ahci_platform_data0.pexp_gpio = -1;

	tegra_sata_device.dev.platform_data = &tegra_ahci_platform_data0;
	platform_device_register(&tegra_sata_device);
#endif
	return 0;
}
