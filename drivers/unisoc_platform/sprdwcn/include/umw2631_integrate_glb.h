// SPDX-License-Identifier: GPL-2.0-only
/*
 * umw2631_integrate_glb.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _UMW2631_INTEG_GLB_H_
#define _UMW2631_INTEG_GLB_H_

#include "wcn_integrate_base_glb.h"

/* ap cp sync flag */
#define UMW2631_MARLIN_CP_INIT_READY_MAGIC	(0xf0f0f0ff)


/* AP regs start and end */
#define UMW2631_WCN_DUMP_AP_REGS_END 7

/* CP2 regs start and end */
#define UMW2631_WCN_DUMP_CP2_REGS_START (UMW2631_WCN_DUMP_AP_REGS_END + 1)

#endif
