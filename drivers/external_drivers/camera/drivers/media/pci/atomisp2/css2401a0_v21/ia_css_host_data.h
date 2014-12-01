/* Release Version: irci_master_20141129_0200 */
/* Release Version: irci_master_20141129_0200 */
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

#ifndef __SH_CSS_HOST_DATA_H
#define __SH_CSS_HOST_DATA_H

#include <ia_css_types.h>	/* ia_css_pipe */

/**
 * @brief Allocate structure ia_css_host_data.
 *
 * @param[in]	size		Size of the requested host data
 *
 * @return
 *	- NULL, can't allocate requested size
 *	- pointer to structure, field address points to host data with size bytes
 */
struct ia_css_host_data *
ia_css_host_data_allocate(size_t size);

/**
 * @brief Free structure ia_css_host_data.
 *
 * @param[in]	me	Pointer to structure, if a NULL is passed functions
 *			returns without error. Otherwise a valid pointer to
 *			structure must be passed and a related memory
 *			is freed.
 *
 * @return
 */
void ia_css_host_data_free(struct ia_css_host_data *me);

#endif /* __SH_CSS_HOST_DATA_H */

