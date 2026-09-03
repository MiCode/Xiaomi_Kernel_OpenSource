// SPDX-License-Identifier: GPL-2.0-only
/*
 * wcn_glb.h - Unisoc platform header
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

#ifndef __WCN_GLB_H__
#define __WCN_GLB_H__

#include <misc/marlin_platform.h>

#include "bufring.h"
#include "loopcheck.h"
#include "mdbg_type.h"
//#include "rdc_debug.h"
#include "reset.h"
#include "sysfs.h"
#include "wcn_dbg.h"
#include "wcn_parn_parser.h"
#include "wcn_txrx.h"
#include "wcn_log.h"
#include "wcn_misc.h"
#include "sprd_wcn.h"
#include "wcn_dump.h"
#include "wcn_glb_reg.h"
#include "wcn_dump_integrate.h"
#include "wcn_integrate.h"
#include "wcn_integrate_boot.h"
#include "wcn_integrate_dev.h"

/* log buf size */
#define M3E_MDBG_RX_RING_SIZE		(64*1024)
#define M3L_MDBG_RX_RING_SIZE		(128 * 1024)
#define M3_MDBG_RX_RING_SIZE		(96 * 1024)
#define MDBG_RX_RING_SIZE	(128 * 1024)

#endif
