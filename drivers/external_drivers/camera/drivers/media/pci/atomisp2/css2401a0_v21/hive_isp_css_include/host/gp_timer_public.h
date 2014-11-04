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

#ifndef __GP_TIMER_PUBLIC_H_INCLUDED__
#define __GP_TIMER_PUBLIC_H_INCLUDED__

#include "system_types.h"
#include <stdint.h> /*uint32_t */

/*! initialize mentioned timer
param ID		timer_id
*/
extern void
gp_timer_init(gp_timer_ID_t ID);


/*! read timer value for (platform selected)selected timer.
param ID		timer_id
 \return uint32_t	32 bit timer value
*/
extern uint32_t
gp_timer_read(gp_timer_ID_t ID);

#endif /* __GP_TIMER_PUBLIC_H_INCLUDED__ */
