/*
 * power_supply_extcon: Power supply detection through extcon.
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 * Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _LINUX_POWER_SUPPLY_EXTCON_H
#define _LINUX_POWER_SUPPLY_EXTCON_H

struct power_supply_extcon_plat_data {
	const char *extcon_name;
};

#endif
