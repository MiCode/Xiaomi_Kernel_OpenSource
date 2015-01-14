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

#ifndef __IA_CSS_CTC_PARAM_H
#define __IA_CSS_CTC_PARAM_H

#include "type_support.h"
#include <system_global.h>

#include "ia_css_ctc_types.h"

#ifndef PIPE_GENERATION
#if defined(HAS_VAMEM_VERSION_2)
#define SH_CSS_ISP_CTC_TABLE_SIZE_LOG2       IA_CSS_VAMEM_2_CTC_TABLE_SIZE_LOG2
#define SH_CSS_ISP_CTC_TABLE_SIZE            IA_CSS_VAMEM_2_CTC_TABLE_SIZE
#elif defined(HAS_VAMEM_VERSION_1)
#define SH_CSS_ISP_CTC_TABLE_SIZE_LOG2       IA_CSS_VAMEM_1_CTC_TABLE_SIZE_LOG2
#define SH_CSS_ISP_CTC_TABLE_SIZE            IA_CSS_VAMEM_1_CTC_TABLE_SIZE
#else
#error "VAMEM should be {VERSION1, VERSION2}" 
#endif

#else
/* For pipe generation, the size is not relevant */
#define SH_CSS_ISP_CTC_TABLE_SIZE 0
#endif

/* This should be vamem_data_t, but that breaks the pipe generator */
struct sh_css_isp_ctc_vamem_params {
	uint16_t ctc[SH_CSS_ISP_CTC_TABLE_SIZE];
};

#endif /* __IA_CSS_CTC_PARAM_H */
