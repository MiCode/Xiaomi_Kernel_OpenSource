/* include/linux/tegra_caif.h
 *
 * Copyright (C) 2011 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _TEGRA_CAIF_H_
#define _TEGRA_CAIF_H_

/* The GPIO details needed by the rainbow caif */
struct tegra_caif_platform_data {
	int reset;
	int power;
	int awr;
	int cwr;
	int spi_int;
	int spi_ss;
};

#endif /* _TEGRA_CAIF_H_ */

