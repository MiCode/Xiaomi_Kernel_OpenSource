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

#ifndef __IA_CSS_CNR_STATE_H
#define __IA_CSS_CNR_STATE_H

#include "type_support.h"

#include "vmem.h"

typedef struct
{
  VMEM_ARRAY(u, ISP_NWAY);
  VMEM_ARRAY(v, ISP_NWAY);
} s_cnr_buf;

/* CNR (color noise reduction) */
struct sh_css_isp_cnr_vmem_state {
	s_cnr_buf cnr_buf[2][MAX_VECTORS_PER_BUF_LINE/2];
};

#endif /* __IA_CSS_CNR_STATE_H */
