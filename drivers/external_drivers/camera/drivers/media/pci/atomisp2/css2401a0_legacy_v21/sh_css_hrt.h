/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
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

#ifndef _SH_CSS_HRT_H_
#define _SH_CSS_HRT_H_

#include <sp.h>
#include <isp.h>

#include <ia_css_err.h>

/* SP access */
void sh_css_hrt_sp_start_si(void);

void sh_css_hrt_sp_start_copy_frame(void);

void sh_css_hrt_sp_start_isp(void);

enum ia_css_err sh_css_hrt_sp_wait(void);

bool sh_css_hrt_system_is_idle(void);

#endif /* _SH_CSS_HRT_H_ */
