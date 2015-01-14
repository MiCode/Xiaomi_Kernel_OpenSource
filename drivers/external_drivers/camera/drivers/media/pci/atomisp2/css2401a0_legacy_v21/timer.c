/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2015 Intel Corporation. All Rights Reserved.
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

#include <type_support.h>		/* for uint32_t */
#include "ia_css_timer.h" /*struct ia_css_clock_tick */
#include "sh_css_legacy.h" /* IA_CSS_PIPE_ID_NUM*/
#include "gp_timer.h" /*gp_timer_read()*/
#include "assert_support.h"

enum ia_css_err
ia_css_timer_get_current_tick(
	struct ia_css_clock_tick *curr_ts) {

	assert(curr_ts !=  NULL);
	if (curr_ts == NULL) {
		return IA_CSS_ERR_INVALID_ARGUMENTS;
	}
	curr_ts->ticks = (clock_value_t)gp_timer_read(GP_TIMER_SEL);
	return IA_CSS_SUCCESS;
}

