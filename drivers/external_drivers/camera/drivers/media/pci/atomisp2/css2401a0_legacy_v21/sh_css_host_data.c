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

#include <stddef.h>
#include <ia_css_host_data.h>
#include <sh_css_internal.h>

struct ia_css_host_data *ia_css_host_data_allocate(size_t size)
{
	struct ia_css_host_data *me;

	me =  sh_css_malloc(sizeof(struct ia_css_host_data));
	if (!me)
		return NULL;
	me->size = (uint32_t)size;
	me->address = sh_css_malloc(size);
	if (!me->address) {
		sh_css_free(me);
		return NULL;
	}
	return me;
}

void ia_css_host_data_free(struct ia_css_host_data *me)
{
	if (me) {
		sh_css_free(me->address);
		sh_css_free(me);
	}
}
