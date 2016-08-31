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
#ifndef _hw_host1x02_uclass_h_
#define _hw_host1x02_uclass_h_

static inline u32 host1x_uclass_incr_syncpt_r(void)
{
	return 0x0;
}
static inline u32 host1x_uclass_incr_syncpt_cond_s(void)
{
	return 8;
}
static inline u32 host1x_uclass_incr_syncpt_cond_f(u32 v)
{
	return (v & 0xff) << 8;
}
static inline u32 host1x_uclass_incr_syncpt_cond_m(void)
{
	return 0xff << 8;
}
static inline u32 host1x_uclass_incr_syncpt_cond_v(u32 r)
{
	return (r >> 8) & 0xff;
}
static inline u32 host1x_uclass_incr_syncpt_cond_op_done_v(void)
{
	return 0x00000001;
}
static inline u32 host1x_uclass_incr_syncpt_indx_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 host1x_uclass_wait_syncpt_r(void)
{
	return 0x8;
}
static inline u32 host1x_uclass_wait_syncpt_indx_f(u32 v)
{
	return (v & 0xff) << 24;
}
static inline u32 host1x_uclass_wait_syncpt_thresh_f(u32 v)
{
	return (v & 0xffffff) << 0;
}
static inline u32 host1x_uclass_wait_syncpt_base_r(void)
{
	return 0x9;
}
static inline u32 host1x_uclass_wait_syncpt_base_indx_f(u32 v)
{
	return (v & 0xff) << 24;
}
static inline u32 host1x_uclass_wait_syncpt_base_base_indx_f(u32 v)
{
	return (v & 0xff) << 16;
}
static inline u32 host1x_uclass_wait_syncpt_base_offset_f(u32 v)
{
	return (v & 0xffff) << 0;
}
static inline u32 host1x_uclass_load_syncpt_base_r(void)
{
	return 0xb;
}
static inline u32 host1x_uclass_load_syncpt_base_base_indx_f(u32 v)
{
	return (v & 0xff) << 24;
}
static inline u32 host1x_uclass_load_syncpt_base_value_f(u32 v)
{
	return (v & 0xffffff) << 0;
}
static inline u32 host1x_uclass_incr_syncpt_base_r(void)
{
	return 0xc;
}
static inline u32 host1x_uclass_incr_syncpt_base_base_indx_f(u32 v)
{
	return (v & 0xff) << 24;
}
static inline u32 host1x_uclass_incr_syncpt_base_offset_f(u32 v)
{
	return (v & 0xffffff) << 0;
}
static inline u32 host1x_uclass_indoff_r(void)
{
	return 0x2d;
}
static inline u32 host1x_uclass_indoff_indbe_f(u32 v)
{
	return (v & 0xf) << 28;
}
static inline u32 host1x_uclass_indoff_autoinc_f(u32 v)
{
	return (v & 0x1) << 27;
}
static inline u32 host1x_uclass_indoff_indmodid_f(u32 v)
{
	return (v & 0xff) << 18;
}
static inline u32 host1x_uclass_indoff_indmodid_gr3d_v(void)
{
	return 0x00000006;
}
static inline u32 host1x_uclass_indoff_indroffset_f(u32 v)
{
	return (v & 0xffff) << 2;
}
static inline u32 host1x_uclass_indoff_rwn_read_v(void)
{
	return 0x00000001;
}
static inline u32 host1x_uclass_inddata_r(void)
{
	return 0x2e;
}
#endif
