/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
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
 *
 */

#ifndef __INCLUDE_BOARD_INFO_H
#define __INCLUDE_BOARD_INFO_H

#define TEGRA_MAX_BOARDS 4
#define MAX_BUFFER 8

struct tegra_board_info {
	int valid;
	char bom[MAX_BUFFER] ;
	char project[MAX_BUFFER];
	char sku[MAX_BUFFER];
	char revision[MAX_BUFFER];
	char bom_version[MAX_BUFFER];
};

bool tegra_is_board(const char *bom, const char *project,
		const char *sku, const char *revision,
		const char *bom_version);
#endif
