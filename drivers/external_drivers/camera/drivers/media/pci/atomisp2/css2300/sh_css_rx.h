/*
* Support for Medfield PNW Camera Imaging ISP subsystem.
*
* Copyright (c) 2010 Intel Corporation. All Rights Reserved.
*
* Copyright (c) 2010 Silicon Hive www.siliconhive.com.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version
* 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
* 02110-1301, USA.
*
*/

#ifndef _SH_CSS_RX_H_
#define _SH_CSS_RX_H_

#include "input_system.h"

/* CSS Receiver */

void sh_css_rx_configure(
	const rx_cfg_t	*config);

void sh_css_rx_disable(void);

void sh_css_rx_enable_all_interrupts(void);

unsigned int sh_css_rx_get_interrupt_reg(void);

#endif /* _SH_CSS_RX_H_ */
