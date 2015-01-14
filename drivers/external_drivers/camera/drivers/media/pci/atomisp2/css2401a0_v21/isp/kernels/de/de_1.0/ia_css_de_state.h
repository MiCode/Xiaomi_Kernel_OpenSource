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

#ifndef __IA_CSS_DE_STATE_H
#define __IA_CSS_DE_STATE_H

#include "type_support.h"
#include "vmem.h"

/* DE (Demosaic) */
struct sh_css_isp_de_vmem_state {
	VMEM_ARRAY(de_buf[4], MAX_VECTORS_PER_BUF_LINE*ISP_NWAY);
};

#endif /* __IA_CSS_DE_STATE_H */
