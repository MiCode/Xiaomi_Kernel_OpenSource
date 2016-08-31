/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
 * Function naming determines intended use:
 *
 *     <x>_r(void) : Returns the offset for register <x>.
 *
 *     <x>_o(void) : Returns the offset for element <x>.
 *
 *     <x>_w(void) : Returns the word offset for word (4 byte) element <x>.
 *
 *     <x>_<y>_s(void) : Returns size of field <y> of register <x> in bits.
 *
 *     <x>_<y>_f(u32 v) : Returns a value based on 'v' which has been shifted
 *         and masked to place it at field <y> of register <x>.  This value
 *         can be |'d with others to produce a full register value for
 *         register <x>.
 *
 *     <x>_<y>_m(void) : Returns a mask for field <y> of register <x>.  This
 *         value can be ~'d and then &'d to clear the value of field <y> for
 *         register <x>.
 *
 *     <x>_<y>_<z>_f(void) : Returns the constant value <z> after being shifted
 *         to place it at field <y> of register <x>.  This value can be |'d
 *         with others to produce a full register value for <x>.
 *
 *     <x>_<y>_v(u32 r) : Returns the value of field <y> from a full register
 *         <x> value 'r' after being shifted to place its LSB at bit 0.
 *         This value is suitable for direct comparison with other unshifted
 *         values appropriate for use in field <y> of register <x>.
 *
 *     <x>_<y>_<z>_v(void) : Returns the constant value for <z> defined for
 *         field <y> of register <x>.  This value is suitable for direct
 *         comparison with unshifted values appropriate for use in field <y>
 *         of register <x>.
 */
#ifndef _hw_top_gk20a_h_
#define _hw_top_gk20a_h_

static inline u32 top_num_gpcs_r(void)
{
	return 0x00022430;
}
static inline u32 top_num_gpcs_value_v(u32 r)
{
	return (r >> 0) & 0x1f;
}
static inline u32 top_tpc_per_gpc_r(void)
{
	return 0x00022434;
}
static inline u32 top_tpc_per_gpc_value_v(u32 r)
{
	return (r >> 0) & 0x1f;
}
static inline u32 top_num_fbps_r(void)
{
	return 0x00022438;
}
static inline u32 top_num_fbps_value_v(u32 r)
{
	return (r >> 0) & 0x1f;
}
static inline u32 top_fs_status_r(void)
{
	return 0x00022500;
}
static inline u32 top_device_info_r(u32 i)
{
	return 0x00022700 + i*4;
}
static inline u32 top_device_info__size_1_v(void)
{
	return 0x00000040;
}
static inline u32 top_device_info_chain_v(u32 r)
{
	return (r >> 31) & 0x1;
}
static inline u32 top_device_info_chain_enable_v(void)
{
	return 0x00000001;
}
static inline u32 top_device_info_engine_enum_v(u32 r)
{
	return (r >> 26) & 0xf;
}
static inline u32 top_device_info_runlist_enum_v(u32 r)
{
	return (r >> 21) & 0xf;
}
static inline u32 top_device_info_type_enum_v(u32 r)
{
	return (r >> 2) & 0x1fffffff;
}
static inline u32 top_device_info_type_enum_graphics_v(void)
{
	return 0x00000000;
}
static inline u32 top_device_info_type_enum_graphics_f(void)
{
	return 0x0;
}
static inline u32 top_device_info_type_enum_copy0_v(void)
{
	return 0x00000001;
}
static inline u32 top_device_info_type_enum_copy0_f(void)
{
	return 0x4;
}
static inline u32 top_device_info_entry_v(u32 r)
{
	return (r >> 0) & 0x3;
}
static inline u32 top_device_info_entry_not_valid_v(void)
{
	return 0x00000000;
}
static inline u32 top_device_info_entry_enum_v(void)
{
	return 0x00000002;
}
#endif
