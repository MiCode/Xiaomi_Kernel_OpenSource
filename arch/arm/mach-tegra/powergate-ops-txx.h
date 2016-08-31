/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __POWERGATE_OPS_TXX_H__
#define __POWERGATE_OPS_TXX_H__

/* Common APIs for tegra2 and tegra3 SoCs */
int tegraxx_powergate_partition(int id,
	struct powergate_partition_info *pg_info);

int tegraxx_unpowergate_partition(int id,
	struct powergate_partition_info *pg_info);

int tegraxx_powergate_partition_with_clk_off(int id,
	struct powergate_partition_info *pg_info);

int tegraxx_unpowergate_partition_with_clk_on(int id,
	struct powergate_partition_info *pg_info);

#endif /* __POWERGATE_OPS_TXX_H__ */
