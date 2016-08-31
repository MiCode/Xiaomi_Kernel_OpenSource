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
#ifndef _hw_trim_gk20a_h_
#define _hw_trim_gk20a_h_

static inline u32 trim_sys_gpcpll_cfg_r(void)
{
	return 0x00137000;
}
static inline u32 trim_sys_gpcpll_cfg_enable_m(void)
{
	return 0x1 << 0;
}
static inline u32 trim_sys_gpcpll_cfg_enable_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 trim_sys_gpcpll_cfg_enable_no_f(void)
{
	return 0x0;
}
static inline u32 trim_sys_gpcpll_cfg_enable_yes_f(void)
{
	return 0x1;
}
static inline u32 trim_sys_gpcpll_cfg_iddq_m(void)
{
	return 0x1 << 1;
}
static inline u32 trim_sys_gpcpll_cfg_iddq_v(u32 r)
{
	return (r >> 1) & 0x1;
}
static inline u32 trim_sys_gpcpll_cfg_iddq_power_on_v(void)
{
	return 0x00000000;
}
static inline u32 trim_sys_gpcpll_cfg_enb_lckdet_m(void)
{
	return 0x1 << 4;
}
static inline u32 trim_sys_gpcpll_cfg_enb_lckdet_power_on_f(void)
{
	return 0x0;
}
static inline u32 trim_sys_gpcpll_cfg_enb_lckdet_power_off_f(void)
{
	return 0x10;
}
static inline u32 trim_sys_gpcpll_cfg_pll_lock_v(u32 r)
{
	return (r >> 17) & 0x1;
}
static inline u32 trim_sys_gpcpll_cfg_pll_lock_true_f(void)
{
	return 0x20000;
}
static inline u32 trim_sys_gpcpll_coeff_r(void)
{
	return 0x00137004;
}
static inline u32 trim_sys_gpcpll_coeff_mdiv_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 trim_sys_gpcpll_coeff_mdiv_v(u32 r)
{
	return (r >> 0) & 0xff;
}
static inline u32 trim_sys_gpcpll_coeff_ndiv_f(u32 v)
{
	return (v & 0xff) << 8;
}
static inline u32 trim_sys_gpcpll_coeff_ndiv_m(void)
{
	return 0xff << 8;
}
static inline u32 trim_sys_gpcpll_coeff_ndiv_v(u32 r)
{
	return (r >> 8) & 0xff;
}
static inline u32 trim_sys_gpcpll_coeff_pldiv_f(u32 v)
{
	return (v & 0x3f) << 16;
}
static inline u32 trim_sys_gpcpll_coeff_pldiv_v(u32 r)
{
	return (r >> 16) & 0x3f;
}
static inline u32 trim_sys_sel_vco_r(void)
{
	return 0x00137100;
}
static inline u32 trim_sys_sel_vco_gpc2clk_out_m(void)
{
	return 0x1 << 0;
}
static inline u32 trim_sys_sel_vco_gpc2clk_out_init_v(void)
{
	return 0x00000000;
}
static inline u32 trim_sys_sel_vco_gpc2clk_out_init_f(void)
{
	return 0x0;
}
static inline u32 trim_sys_sel_vco_gpc2clk_out_bypass_f(void)
{
	return 0x0;
}
static inline u32 trim_sys_sel_vco_gpc2clk_out_vco_f(void)
{
	return 0x1;
}
static inline u32 trim_sys_gpc2clk_out_r(void)
{
	return 0x00137250;
}
static inline u32 trim_sys_gpc2clk_out_bypdiv_s(void)
{
	return 6;
}
static inline u32 trim_sys_gpc2clk_out_bypdiv_f(u32 v)
{
	return (v & 0x3f) << 0;
}
static inline u32 trim_sys_gpc2clk_out_bypdiv_m(void)
{
	return 0x3f << 0;
}
static inline u32 trim_sys_gpc2clk_out_bypdiv_v(u32 r)
{
	return (r >> 0) & 0x3f;
}
static inline u32 trim_sys_gpc2clk_out_bypdiv_by31_f(void)
{
	return 0x3c;
}
static inline u32 trim_sys_gpc2clk_out_vcodiv_s(void)
{
	return 6;
}
static inline u32 trim_sys_gpc2clk_out_vcodiv_f(u32 v)
{
	return (v & 0x3f) << 8;
}
static inline u32 trim_sys_gpc2clk_out_vcodiv_m(void)
{
	return 0x3f << 8;
}
static inline u32 trim_sys_gpc2clk_out_vcodiv_v(u32 r)
{
	return (r >> 8) & 0x3f;
}
static inline u32 trim_sys_gpc2clk_out_vcodiv_by1_f(void)
{
	return 0x0;
}
static inline u32 trim_sys_gpc2clk_out_sdiv14_m(void)
{
	return 0x1 << 31;
}
static inline u32 trim_sys_gpc2clk_out_sdiv14_indiv4_mode_f(void)
{
	return 0x80000000;
}
static inline u32 trim_gpc_clk_cntr_ncgpcclk_cfg_r(u32 i)
{
	return 0x00134124 + i*512;
}
static inline u32 trim_gpc_clk_cntr_ncgpcclk_cfg_noofipclks_f(u32 v)
{
	return (v & 0x3fff) << 0;
}
static inline u32 trim_gpc_clk_cntr_ncgpcclk_cfg_write_en_asserted_f(void)
{
	return 0x10000;
}
static inline u32 trim_gpc_clk_cntr_ncgpcclk_cfg_enable_asserted_f(void)
{
	return 0x100000;
}
static inline u32 trim_gpc_clk_cntr_ncgpcclk_cfg_reset_asserted_f(void)
{
	return 0x1000000;
}
static inline u32 trim_gpc_clk_cntr_ncgpcclk_cnt_r(u32 i)
{
	return 0x00134128 + i*512;
}
static inline u32 trim_gpc_clk_cntr_ncgpcclk_cnt_value_v(u32 r)
{
	return (r >> 0) & 0xfffff;
}
static inline u32 trim_sys_gpcpll_cfg2_r(void)
{
	return 0x0013700c;
}
static inline u32 trim_sys_gpcpll_cfg2_pll_stepa_f(u32 v)
{
	return (v & 0xff) << 24;
}
static inline u32 trim_sys_gpcpll_cfg2_pll_stepa_m(void)
{
	return 0xff << 24;
}
static inline u32 trim_sys_gpcpll_cfg3_r(void)
{
	return 0x00137018;
}
static inline u32 trim_sys_gpcpll_cfg3_pll_stepb_f(u32 v)
{
	return (v & 0xff) << 16;
}
static inline u32 trim_sys_gpcpll_cfg3_pll_stepb_m(void)
{
	return 0xff << 16;
}
static inline u32 trim_sys_gpcpll_ndiv_slowdown_r(void)
{
	return 0x0013701c;
}
static inline u32 trim_sys_gpcpll_ndiv_slowdown_slowdown_using_pll_m(void)
{
	return 0x1 << 22;
}
static inline u32 trim_sys_gpcpll_ndiv_slowdown_slowdown_using_pll_yes_f(void)
{
	return 0x400000;
}
static inline u32 trim_sys_gpcpll_ndiv_slowdown_slowdown_using_pll_no_f(void)
{
	return 0x0;
}
static inline u32 trim_sys_gpcpll_ndiv_slowdown_en_dynramp_m(void)
{
	return 0x1 << 31;
}
static inline u32 trim_sys_gpcpll_ndiv_slowdown_en_dynramp_yes_f(void)
{
	return 0x80000000;
}
static inline u32 trim_sys_gpcpll_ndiv_slowdown_en_dynramp_no_f(void)
{
	return 0x0;
}
static inline u32 trim_gpc_bcast_gpcpll_ndiv_slowdown_debug_r(void)
{
	return 0x001328a0;
}
static inline u32 trim_gpc_bcast_gpcpll_ndiv_slowdown_debug_pll_dynramp_done_synced_v(u32 r)
{
	return (r >> 24) & 0x1;
}
#endif
