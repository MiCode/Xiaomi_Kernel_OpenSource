/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GBE_USEDEXT_H
#define GBE_USEDEXT_H
#include "fpsgo_base.h"

extern int gbe2xgf_get_dep_list_num(int pid, unsigned long long bufID);
extern int gbe2xgf_get_dep_list(int pid, int count,
	struct gbe_runtime *arr, unsigned long long bufID);

#endif

