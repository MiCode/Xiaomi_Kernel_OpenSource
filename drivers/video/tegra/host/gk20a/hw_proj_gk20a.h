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
#ifndef _hw_proj_gk20a_h_
#define _hw_proj_gk20a_h_

static inline u32 proj_gpc_base_v(void)
{
	return 0x00500000;
}
static inline u32 proj_gpc_shared_base_v(void)
{
	return 0x00418000;
}
static inline u32 proj_gpc_stride_v(void)
{
	return 0x00008000;
}
static inline u32 proj_ltc_stride_v(void)
{
	return 0x00002000;
}
static inline u32 proj_lts_stride_v(void)
{
	return 0x00000400;
}
static inline u32 proj_ppc_in_gpc_base_v(void)
{
	return 0x00003000;
}
static inline u32 proj_ppc_in_gpc_stride_v(void)
{
	return 0x00000200;
}
static inline u32 proj_rop_base_v(void)
{
	return 0x00410000;
}
static inline u32 proj_rop_shared_base_v(void)
{
	return 0x00408800;
}
static inline u32 proj_rop_stride_v(void)
{
	return 0x00000400;
}
static inline u32 proj_tpc_in_gpc_base_v(void)
{
	return 0x00004000;
}
static inline u32 proj_tpc_in_gpc_stride_v(void)
{
	return 0x00000800;
}
static inline u32 proj_tpc_in_gpc_shared_base_v(void)
{
	return 0x00001800;
}
static inline u32 proj_host_num_pbdma_v(void)
{
	return 0x00000001;
}
static inline u32 proj_scal_litter_num_tpc_per_gpc_v(void)
{
	return 0x00000001;
}
static inline u32 proj_scal_litter_num_fbps_v(void)
{
	return 0x00000001;
}
static inline u32 proj_scal_litter_num_gpcs_v(void)
{
	return 0x00000001;
}
static inline u32 proj_scal_litter_num_pes_per_gpc_v(void)
{
	return 0x00000001;
}
static inline u32 proj_scal_litter_num_tpcs_per_pes_v(void)
{
	return 0x00000001;
}
static inline u32 proj_scal_litter_num_zcull_banks_v(void)
{
	return 0x00000004;
}
static inline u32 proj_scal_max_gpcs_v(void)
{
	return 0x00000020;
}
static inline u32 proj_scal_max_tpc_per_gpc_v(void)
{
	return 0x00000008;
}
#endif
