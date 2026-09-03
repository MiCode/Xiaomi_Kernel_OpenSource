// SPDX-License-Identifier: GPL-2.0-only
/*
 * slp_sdio.h - Unisoc platform header
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

#ifndef __SLP_SDIO_H__
#define __SLP_SDIO_H__

#include "sdio_int.h"
#include "wcn_glb.h"

union CP_SLP_CTL_REG {
	unsigned char reg;
	struct {
		unsigned char cp_slp_ctl:1;  /* 0:wakeup, 1:sleep */
		unsigned char rsvd:7;
	} bit;
};

static inline
int ap_wakeup_cp(void)
{
	return sprdwcn_bus_aon_writeb(get_cp_slp_ctl_reg(), 0);
}
int slp_allow_sleep(void);
int slp_pub_int_regcb(void);

#endif
