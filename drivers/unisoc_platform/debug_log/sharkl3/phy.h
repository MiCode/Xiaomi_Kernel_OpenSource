/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MIPI_LOG_PHY_R4PX_H__
#define __MIPI_LOG_PHY_R4PX_H__

int dbg_phy_init(struct phy_ctx *phy);
int dbg_phy_exit(struct phy_ctx *phy);
int dbg_phy_enable(struct phy_ctx *phy, int enable,
		   void (*ext_ctrl)(void *), void *ext_para);

#endif
