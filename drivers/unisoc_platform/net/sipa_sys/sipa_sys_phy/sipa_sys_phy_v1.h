/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2019 Spreadtrum Communications Inc.
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

#ifndef _IPA_SYS_PHY_V1_H_
#define _IPA_SYS_PHY_V1_H_

int sipa_sys_parse_dts_cb_v1(void *priv);
void sipa_sys_init_cb_v1(void *priv);
int sipa_sys_do_power_on_cb_v1(void *priv);
int sipa_sys_do_power_off_cb_v1(void *priv);
int sipa_sys_clk_enable_cb_v1(void *priv);

#endif
