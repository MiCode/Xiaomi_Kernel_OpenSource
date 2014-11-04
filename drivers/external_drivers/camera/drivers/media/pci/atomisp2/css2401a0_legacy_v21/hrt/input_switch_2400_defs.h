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

#ifndef _input_switch_2400_defs_h
#define _input_switch_2400_defs_h

#define _HIVE_INPUT_SWITCH_GET_LUT_REG_ID(ch_id, fmt_type) (((ch_id)*2) + ((fmt_type)>=16))
#define _HIVE_INPUT_SWITCH_GET_LUT_REG_LSB(fmt_type)        (((fmt_type)%16) * 2)

#define HIVE_INPUT_SWITCH_SELECT_NO_OUTPUT   0
#define HIVE_INPUT_SWITCH_SELECT_IF_PRIM     1
#define HIVE_INPUT_SWITCH_SELECT_IF_SEC      2
#define HIVE_INPUT_SWITCH_SELECT_STR_TO_MEM  3
#define HIVE_INPUT_SWITCH_VSELECT_NO_OUTPUT  0
#define HIVE_INPUT_SWITCH_VSELECT_IF_PRIM    1
#define HIVE_INPUT_SWITCH_VSELECT_IF_SEC     2
#define HIVE_INPUT_SWITCH_VSELECT_STR_TO_MEM 4

#endif /* _input_switch_2400_defs_h */
