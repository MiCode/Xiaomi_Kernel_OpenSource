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

#ifndef __FIFO_MONITOR_GLOBAL_H_INCLUDED__
#define __FIFO_MONITOR_GLOBAL_H_INCLUDED__

#define IS_FIFO_MONITOR_VERSION_2

/*
#define HIVE_ISP_CSS_STREAM_SWITCH_NONE      0
#define HIVE_ISP_CSS_STREAM_SWITCH_SP        1
#define HIVE_ISP_CSS_STREAM_SWITCH_ISP       2
 *
 * Actually, "HIVE_ISP_CSS_STREAM_SWITCH_SP = 1", "HIVE_ISP_CSS_STREAM_SWITCH_ISP = 0"
 * "hive_isp_css_stream_switch_hrt.h"
 */
#define HIVE_ISP_CSS_STREAM_SWITCH_ISP       0
#define HIVE_ISP_CSS_STREAM_SWITCH_SP        1
#define HIVE_ISP_CSS_STREAM_SWITCH_NONE      2

#endif /* __FIFO_MONITOR_GLOBAL_H_INCLUDED__ */
