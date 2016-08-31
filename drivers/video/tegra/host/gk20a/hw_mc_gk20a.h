/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
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
#ifndef _hw_mc_gk20a_h_
#define _hw_mc_gk20a_h_

static inline u32 mc_boot_0_r(void)
{
	return 0x00000000;
}
static inline u32 mc_boot_0_architecture_v(u32 r)
{
	return (r >> 24) & 0x1f;
}
static inline u32 mc_boot_0_implementation_v(u32 r)
{
	return (r >> 20) & 0xf;
}
static inline u32 mc_boot_0_major_revision_v(u32 r)
{
	return (r >> 4) & 0xf;
}
static inline u32 mc_boot_0_minor_revision_v(u32 r)
{
	return (r >> 0) & 0xf;
}
static inline u32 mc_intr_0_r(void)
{
	return 0x00000100;
}
static inline u32 mc_intr_0_pfifo_pending_f(void)
{
	return 0x100;
}
static inline u32 mc_intr_0_pgraph_pending_f(void)
{
	return 0x1000;
}
static inline u32 mc_intr_0_pmu_pending_f(void)
{
	return 0x1000000;
}
static inline u32 mc_intr_0_ltc_pending_f(void)
{
	return 0x2000000;
}
static inline u32 mc_intr_0_priv_ring_pending_f(void)
{
	return 0x40000000;
}
static inline u32 mc_intr_0_pbus_pending_f(void)
{
	return 0x10000000;
}
static inline u32 mc_intr_1_r(void)
{
	return 0x00000104;
}
static inline u32 mc_intr_mask_0_r(void)
{
	return 0x00000640;
}
static inline u32 mc_intr_mask_0_pmu_enabled_f(void)
{
	return 0x1000000;
}
static inline u32 mc_intr_mask_1_r(void)
{
	return 0x00000644;
}
static inline u32 mc_intr_mask_1_pmu_enabled_f(void)
{
	return 0x1000000;
}
static inline u32 mc_intr_en_0_r(void)
{
	return 0x00000140;
}
static inline u32 mc_intr_en_0_inta_disabled_f(void)
{
	return 0x0;
}
static inline u32 mc_intr_en_0_inta_hardware_f(void)
{
	return 0x1;
}
static inline u32 mc_intr_en_1_r(void)
{
	return 0x00000144;
}
static inline u32 mc_intr_en_1_inta_disabled_f(void)
{
	return 0x0;
}
static inline u32 mc_intr_en_1_inta_hardware_f(void)
{
	return 0x1;
}
static inline u32 mc_enable_r(void)
{
	return 0x00000200;
}
static inline u32 mc_enable_xbar_enabled_f(void)
{
	return 0x4;
}
static inline u32 mc_enable_l2_enabled_f(void)
{
	return 0x8;
}
static inline u32 mc_enable_pmedia_s(void)
{
	return 1;
}
static inline u32 mc_enable_pmedia_f(u32 v)
{
	return (v & 0x1) << 4;
}
static inline u32 mc_enable_pmedia_m(void)
{
	return 0x1 << 4;
}
static inline u32 mc_enable_pmedia_v(u32 r)
{
	return (r >> 4) & 0x1;
}
static inline u32 mc_enable_priv_ring_enabled_f(void)
{
	return 0x20;
}
static inline u32 mc_enable_ce0_m(void)
{
	return 0x1 << 6;
}
static inline u32 mc_enable_pfifo_enabled_f(void)
{
	return 0x100;
}
static inline u32 mc_enable_pgraph_enabled_f(void)
{
	return 0x1000;
}
static inline u32 mc_enable_pwr_v(u32 r)
{
	return (r >> 13) & 0x1;
}
static inline u32 mc_enable_pwr_disabled_v(void)
{
	return 0x00000000;
}
static inline u32 mc_enable_pwr_enabled_f(void)
{
	return 0x2000;
}
static inline u32 mc_enable_pfb_enabled_f(void)
{
	return 0x100000;
}
static inline u32 mc_enable_ce2_m(void)
{
	return 0x1 << 21;
}
static inline u32 mc_enable_ce2_enabled_f(void)
{
	return 0x200000;
}
static inline u32 mc_enable_blg_enabled_f(void)
{
	return 0x8000000;
}
static inline u32 mc_enable_perfmon_enabled_f(void)
{
	return 0x10000000;
}
static inline u32 mc_enable_hub_enabled_f(void)
{
	return 0x20000000;
}
static inline u32 mc_enable_pb_r(void)
{
	return 0x00000204;
}
static inline u32 mc_enable_pb_0_s(void)
{
	return 1;
}
static inline u32 mc_enable_pb_0_f(u32 v)
{
	return (v & 0x1) << 0;
}
static inline u32 mc_enable_pb_0_m(void)
{
	return 0x1 << 0;
}
static inline u32 mc_enable_pb_0_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 mc_enable_pb_0_enabled_v(void)
{
	return 0x00000001;
}
static inline u32 mc_enable_pb_sel_f(u32 v, u32 i)
{
	return (v & 0x1) << (0 + i*1);
}
#endif
