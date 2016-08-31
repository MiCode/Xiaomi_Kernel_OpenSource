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
#ifndef _hw_ctxsw_prog_gk20a_h_
#define _hw_ctxsw_prog_gk20a_h_

static inline u32 ctxsw_prog_fecs_header_v(void)
{
	return 0x00000100;
}
static inline u32 ctxsw_prog_main_image_num_gpcs_o(void)
{
	return 0x00000008;
}
static inline u32 ctxsw_prog_main_image_patch_count_o(void)
{
	return 0x00000010;
}
static inline u32 ctxsw_prog_main_image_patch_adr_lo_o(void)
{
	return 0x00000014;
}
static inline u32 ctxsw_prog_main_image_patch_adr_hi_o(void)
{
	return 0x00000018;
}
static inline u32 ctxsw_prog_main_image_zcull_o(void)
{
	return 0x0000001c;
}
static inline u32 ctxsw_prog_main_image_zcull_mode_no_ctxsw_v(void)
{
	return 0x00000001;
}
static inline u32 ctxsw_prog_main_image_zcull_mode_separate_buffer_v(void)
{
	return 0x00000002;
}
static inline u32 ctxsw_prog_main_image_zcull_ptr_o(void)
{
	return 0x00000020;
}
static inline u32 ctxsw_prog_main_image_pm_o(void)
{
	return 0x00000028;
}
static inline u32 ctxsw_prog_main_image_pm_mode_m(void)
{
	return 0x7 << 0;
}
static inline u32 ctxsw_prog_main_image_pm_mode_v(u32 r)
{
	return (r >> 0) & 0x7;
}
static inline u32 ctxsw_prog_main_image_pm_mode_no_ctxsw_f(void)
{
	return 0x0;
}
static inline u32 ctxsw_prog_main_image_pm_smpc_mode_m(void)
{
	return 0x7 << 3;
}
static inline u32 ctxsw_prog_main_image_pm_smpc_mode_v(u32 r)
{
	return (r >> 3) & 0x7;
}
static inline u32 ctxsw_prog_main_image_pm_smpc_mode_no_ctxsw_f(void)
{
	return 0x0;
}
static inline u32 ctxsw_prog_main_image_pm_smpc_mode_ctxsw_f(void)
{
	return 0x8;
}
static inline u32 ctxsw_prog_main_image_pm_ptr_o(void)
{
	return 0x0000002c;
}
static inline u32 ctxsw_prog_main_image_num_save_ops_o(void)
{
	return 0x000000f4;
}
static inline u32 ctxsw_prog_main_image_num_restore_ops_o(void)
{
	return 0x000000f8;
}
static inline u32 ctxsw_prog_main_image_magic_value_o(void)
{
	return 0x000000fc;
}
static inline u32 ctxsw_prog_main_image_magic_value_v_value_v(void)
{
	return 0x600dc0de;
}
static inline u32 ctxsw_prog_main_image_priv_access_map_config_o(void)
{
	return 0x000000a0;
}
static inline u32 ctxsw_prog_main_image_priv_access_map_config_mode_allow_all_f(void)
{
	return 0x0;
}
static inline u32 ctxsw_prog_main_image_priv_access_map_config_mode_allow_none_f(void)
{
	return 0x1;
}
static inline u32 ctxsw_prog_main_image_priv_access_map_config_mode_use_map_f(void)
{
	return 0x2;
}
static inline u32 ctxsw_prog_main_image_priv_access_map_addr_lo_o(void)
{
	return 0x000000a4;
}
static inline u32 ctxsw_prog_main_image_priv_access_map_addr_hi_o(void)
{
	return 0x000000a8;
}
static inline u32 ctxsw_prog_main_image_misc_options_o(void)
{
	return 0x0000003c;
}
static inline u32 ctxsw_prog_main_image_misc_options_verif_features_m(void)
{
	return 0x1 << 3;
}
static inline u32 ctxsw_prog_main_image_misc_options_verif_features_disabled_f(void)
{
	return 0x0;
}
static inline u32 ctxsw_prog_main_image_misc_options_verif_features_enabled_f(void)
{
	return 0x8;
}
static inline u32 ctxsw_prog_local_priv_register_ctl_o(void)
{
	return 0x0000000c;
}
static inline u32 ctxsw_prog_local_priv_register_ctl_offset_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 ctxsw_prog_local_image_ppc_info_o(void)
{
	return 0x000000f4;
}
static inline u32 ctxsw_prog_local_image_ppc_info_num_ppcs_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 ctxsw_prog_local_image_ppc_info_ppc_mask_v(u32 r)
{
	return (r >> 16) & 0xffff;
}
static inline u32 ctxsw_prog_local_image_num_tpcs_o(void)
{
	return 0x000000f8;
}
static inline u32 ctxsw_prog_local_magic_value_o(void)
{
	return 0x000000fc;
}
static inline u32 ctxsw_prog_local_magic_value_v_value_v(void)
{
	return 0xad0becab;
}
static inline u32 ctxsw_prog_main_extended_buffer_ctl_o(void)
{
	return 0x000000ec;
}
static inline u32 ctxsw_prog_main_extended_buffer_ctl_offset_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 ctxsw_prog_main_extended_buffer_ctl_size_v(u32 r)
{
	return (r >> 16) & 0xff;
}
static inline u32 ctxsw_prog_extended_buffer_segments_size_in_bytes_v(void)
{
	return 0x00000100;
}
static inline u32 ctxsw_prog_extended_marker_size_in_bytes_v(void)
{
	return 0x00000004;
}
static inline u32 ctxsw_prog_extended_sm_dsm_perf_counter_register_stride_v(void)
{
	return 0x00000005;
}
static inline u32 ctxsw_prog_extended_sm_dsm_perf_counter_control_register_stride_v(void)
{
	return 0x00000004;
}
static inline u32 ctxsw_prog_extended_num_smpc_quadrants_v(void)
{
	return 0x00000004;
}
#endif
