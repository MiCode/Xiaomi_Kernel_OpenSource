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

#include "ia_css_properties.h"
#include <assert_support.h>
#include "ia_css_types.h"
#include "gdc_device.h"

void
ia_css_get_properties(struct ia_css_properties *properties)
{
	assert(properties != NULL);
#if defined(HAS_GDC_VERSION_2) || defined(HAS_GDC_VERSION_3)
/*
 * MW: We don't want to store the coordinates
 * full range in memory: Truncate
 */
	properties->gdc_coord_one = gdc_get_unity(GDC0_ID)/HRT_GDC_COORD_SCALE;
#else
#error "Unknown GDC version"
#endif

	properties->l1_base_is_index = true;

#if defined(HAS_VAMEM_VERSION_1)
	properties->vamem_type = IA_CSS_VAMEM_TYPE_1;
#elif defined(HAS_VAMEM_VERSION_2)
	properties->vamem_type = IA_CSS_VAMEM_TYPE_2;
#else
#error "Unknown VAMEM version"
#endif
}
