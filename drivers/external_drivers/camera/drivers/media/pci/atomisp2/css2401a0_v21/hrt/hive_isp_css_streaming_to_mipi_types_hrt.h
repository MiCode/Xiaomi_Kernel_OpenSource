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

#ifndef _hive_isp_css_streaming_to_mipi_types_hrt_h_
#define _hive_isp_css_streaming_to_mipi_types_hrt_h_

#include <streaming_to_mipi_defs.h>

#define _HIVE_ISP_CH_ID_MASK    ((1U << HIVE_ISP_CH_ID_BITS)-1)
#define _HIVE_ISP_FMT_TYPE_MASK ((1U << HIVE_ISP_FMT_TYPE_BITS)-1)

#define _HIVE_STR_TO_MIPI_FMT_TYPE_LSB (HIVE_STR_TO_MIPI_CH_ID_LSB + HIVE_ISP_CH_ID_BITS)
#define _HIVE_STR_TO_MIPI_DATA_B_LSB   (HIVE_STR_TO_MIPI_DATA_A_LSB + HIVE_IF_PIXEL_WIDTH)
 
#endif /* _hive_isp_css_streaming_to_mipi_types_hrt_h_ */
