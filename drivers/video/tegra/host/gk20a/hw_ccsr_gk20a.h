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
#ifndef _hw_ccsr_gk20a_h_
#define _hw_ccsr_gk20a_h_

static inline u32 ccsr_channel_inst_r(u32 i)
{
	return 0x00800000 + i*8;
}
static inline u32 ccsr_channel_inst__size_1_v(void)
{
	return 0x00000080;
}
static inline u32 ccsr_channel_inst_ptr_f(u32 v)
{
	return (v & 0xfffffff) << 0;
}
static inline u32 ccsr_channel_inst_target_vid_mem_f(void)
{
	return 0x0;
}
static inline u32 ccsr_channel_inst_bind_false_f(void)
{
	return 0x0;
}
static inline u32 ccsr_channel_inst_bind_true_f(void)
{
	return 0x80000000;
}
static inline u32 ccsr_channel_r(u32 i)
{
	return 0x00800004 + i*8;
}
static inline u32 ccsr_channel__size_1_v(void)
{
	return 0x00000080;
}
static inline u32 ccsr_channel_enable_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 ccsr_channel_enable_set_f(u32 v)
{
	return (v & 0x1) << 10;
}
static inline u32 ccsr_channel_enable_set_true_f(void)
{
	return 0x400;
}
static inline u32 ccsr_channel_enable_clr_true_f(void)
{
	return 0x800;
}
static inline u32 ccsr_channel_runlist_f(u32 v)
{
	return (v & 0xf) << 16;
}
static inline u32 ccsr_channel_status_v(u32 r)
{
	return (r >> 24) & 0xf;
}
static inline u32 ccsr_channel_busy_v(u32 r)
{
	return (r >> 28) & 0x1;
}
#endif
