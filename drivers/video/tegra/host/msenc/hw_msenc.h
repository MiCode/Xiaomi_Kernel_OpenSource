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
#ifndef _hw_msenc_h_
#define _hw_msenc_h_

static inline u32 msenc_irqmset_r(void)
{
	return 0x1010;
}
static inline u32 msenc_irqmset_wdtmr_set_f(void)
{
	return 0x2;
}
static inline u32 msenc_irqmset_halt_set_f(void)
{
	return 0x10;
}
static inline u32 msenc_irqmset_exterr_set_f(void)
{
	return 0x20;
}
static inline u32 msenc_irqmset_swgen0_set_f(void)
{
	return 0x40;
}
static inline u32 msenc_irqmset_swgen1_set_f(void)
{
	return 0x80;
}
static inline u32 msenc_irqmset_ext_f(u32 v)
{
	return (v & 0xff) << 8;
}
static inline u32 msenc_irqdest_r(void)
{
	return 0x101c;
}
static inline u32 msenc_irqdest_host_halt_host_f(void)
{
	return 0x10;
}
static inline u32 msenc_irqdest_host_exterr_host_f(void)
{
	return 0x20;
}
static inline u32 msenc_irqdest_host_swgen0_host_f(void)
{
	return 0x40;
}
static inline u32 msenc_irqdest_host_swgen1_host_f(void)
{
	return 0x80;
}
static inline u32 msenc_irqdest_host_ext_f(u32 v)
{
	return (v & 0xff) << 8;
}
static inline u32 msenc_itfen_r(void)
{
	return 0x1048;
}
static inline u32 msenc_itfen_ctxen_enable_f(void)
{
	return 0x1;
}
static inline u32 msenc_itfen_mthden_enable_f(void)
{
	return 0x2;
}
static inline u32 msenc_idlestate_r(void)
{
	return 0x104c;
}
static inline u32 msenc_cpuctl_r(void)
{
	return 0x1100;
}
static inline u32 msenc_cpuctl_startcpu_true_f(void)
{
	return 0x2;
}
static inline u32 msenc_bootvec_r(void)
{
	return 0x1104;
}
static inline u32 msenc_bootvec_vec_f(u32 v)
{
	return (v & 0xffffffff) << 0;
}
static inline u32 msenc_dmactl_r(void)
{
	return 0x110c;
}
static inline u32 msenc_dmatrfbase_r(void)
{
	return 0x1110;
}
static inline u32 msenc_dmatrfmoffs_r(void)
{
	return 0x1114;
}
static inline u32 msenc_dmatrfmoffs_offs_f(u32 v)
{
	return (v & 0xffff) << 0;
}
static inline u32 msenc_dmatrfcmd_r(void)
{
	return 0x1118;
}
static inline u32 msenc_dmatrfcmd_idle_v(u32 r)
{
	return (r >> 1) & 0x1;
}
static inline u32 msenc_dmatrfcmd_idle_true_v(void)
{
	return 0x00000001;
}
static inline u32 msenc_dmatrfcmd_imem_f(u32 v)
{
	return (v & 0x1) << 4;
}
static inline u32 msenc_dmatrfcmd_imem_true_f(void)
{
	return 0x10;
}
static inline u32 msenc_dmatrfcmd_size_f(u32 v)
{
	return (v & 0x7) << 8;
}
static inline u32 msenc_dmatrfcmd_size_256b_f(void)
{
	return 0x600;
}
static inline u32 msenc_dmatrffboffs_r(void)
{
	return 0x111c;
}
static inline u32 msenc_dmatrffboffs_offs_f(u32 v)
{
	return (v & 0xffffffff) << 0;
}
#endif
