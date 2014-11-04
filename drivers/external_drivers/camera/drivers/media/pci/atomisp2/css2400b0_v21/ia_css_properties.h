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

#ifndef __IA_CSS_PROPERTIES_H
#define __IA_CSS_PROPERTIES_H

/** @file
 * This file contains support for retrieving properties of some hardware the CSS system
 */

#include <type_support.h> /* bool */
#include <ia_css_types.h> /* ia_css_vamem_type */

struct ia_css_properties {
	int  gdc_coord_one;
	bool l1_base_is_index; /**< Indicate whether the L1 page base
				    is a page index or a byte address. */
	enum ia_css_vamem_type vamem_type;
};

/** @brief Get hardware properties
 * @param[in,out]	properties The hardware properties
 * @return	None
 *
 * This function returns a number of hardware properties.
 */
void
ia_css_get_properties(struct ia_css_properties *properties);

#endif /* __IA_CSS_PROPERTIES_H */
