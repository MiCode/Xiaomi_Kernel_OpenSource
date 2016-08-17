/*
 * arch/arm/mach-tegra/board-touch-raydium.h
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef _MACH_TEGRA_BOARD_TOUCH_RAYDIUM_H
#define _MACH_TEGRA_BOARD_TOUCH_RAYDIUM_H

int __init touch_init_raydium(int irq_gpio, int reset_gpio,
				struct rm_spi_ts_platform_data *rm31080ts_data,
				struct spi_board_info *rm31080a_spi_board,
				int size);
#endif
