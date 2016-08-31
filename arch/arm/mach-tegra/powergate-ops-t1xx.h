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

#ifndef __POWERGATE_OPS_T1XX_H__
#define __POWERGATE_OPS_T1XX_H__

/* Common APIs for tegra1xx SoCs */
int tegra1xx_powergate(int id, struct powergate_partition_info *pg_info);
int tegra1xx_unpowergate(int id, struct powergate_partition_info *pg_info);

#endif /* __POWERGATE_OPS_T1XX_H__ */
