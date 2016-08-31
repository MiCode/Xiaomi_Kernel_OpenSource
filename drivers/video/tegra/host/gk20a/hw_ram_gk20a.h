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
#ifndef _hw_ram_gk20a_h_
#define _hw_ram_gk20a_h_

static inline u32 ram_in_ramfc_s(void)
{
	return 4096;
}
static inline u32 ram_in_ramfc_w(void)
{
	return 0;
}
static inline u32 ram_in_page_dir_base_target_f(u32 v)
{
	return (v & 0x3) << 0;
}
static inline u32 ram_in_page_dir_base_target_w(void)
{
	return 128;
}
static inline u32 ram_in_page_dir_base_target_vid_mem_f(void)
{
	return 0x0;
}
static inline u32 ram_in_page_dir_base_vol_w(void)
{
	return 128;
}
static inline u32 ram_in_page_dir_base_vol_true_f(void)
{
	return 0x4;
}
static inline u32 ram_in_page_dir_base_lo_f(u32 v)
{
	return (v & 0xfffff) << 12;
}
static inline u32 ram_in_page_dir_base_lo_w(void)
{
	return 128;
}
static inline u32 ram_in_page_dir_base_hi_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 ram_in_page_dir_base_hi_w(void)
{
	return 129;
}
static inline u32 ram_in_adr_limit_lo_f(u32 v)
{
	return (v & 0xfffff) << 12;
}
static inline u32 ram_in_adr_limit_lo_w(void)
{
	return 130;
}
static inline u32 ram_in_adr_limit_hi_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 ram_in_adr_limit_hi_w(void)
{
	return 131;
}
static inline u32 ram_in_engine_cs_w(void)
{
	return 132;
}
static inline u32 ram_in_engine_cs_wfi_v(void)
{
	return 0x00000000;
}
static inline u32 ram_in_engine_cs_wfi_f(void)
{
	return 0x0;
}
static inline u32 ram_in_engine_cs_fg_v(void)
{
	return 0x00000001;
}
static inline u32 ram_in_engine_cs_fg_f(void)
{
	return 0x8;
}
static inline u32 ram_in_gr_cs_w(void)
{
	return 132;
}
static inline u32 ram_in_gr_cs_wfi_f(void)
{
	return 0x0;
}
static inline u32 ram_in_gr_wfi_target_w(void)
{
	return 132;
}
static inline u32 ram_in_gr_wfi_mode_w(void)
{
	return 132;
}
static inline u32 ram_in_gr_wfi_mode_physical_v(void)
{
	return 0x00000000;
}
static inline u32 ram_in_gr_wfi_mode_physical_f(void)
{
	return 0x0;
}
static inline u32 ram_in_gr_wfi_mode_virtual_v(void)
{
	return 0x00000001;
}
static inline u32 ram_in_gr_wfi_mode_virtual_f(void)
{
	return 0x4;
}
static inline u32 ram_in_gr_wfi_ptr_lo_f(u32 v)
{
	return (v & 0xfffff) << 12;
}
static inline u32 ram_in_gr_wfi_ptr_lo_w(void)
{
	return 132;
}
static inline u32 ram_in_gr_wfi_ptr_hi_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 ram_in_gr_wfi_ptr_hi_w(void)
{
	return 133;
}
static inline u32 ram_in_base_shift_v(void)
{
	return 0x0000000c;
}
static inline u32 ram_in_alloc_size_v(void)
{
	return 0x00001000;
}
static inline u32 ram_fc_size_val_v(void)
{
	return 0x00000200;
}
static inline u32 ram_fc_gp_put_w(void)
{
	return 0;
}
static inline u32 ram_fc_userd_w(void)
{
	return 2;
}
static inline u32 ram_fc_userd_hi_w(void)
{
	return 3;
}
static inline u32 ram_fc_signature_w(void)
{
	return 4;
}
static inline u32 ram_fc_gp_get_w(void)
{
	return 5;
}
static inline u32 ram_fc_pb_get_w(void)
{
	return 6;
}
static inline u32 ram_fc_pb_get_hi_w(void)
{
	return 7;
}
static inline u32 ram_fc_pb_top_level_get_w(void)
{
	return 8;
}
static inline u32 ram_fc_pb_top_level_get_hi_w(void)
{
	return 9;
}
static inline u32 ram_fc_acquire_w(void)
{
	return 12;
}
static inline u32 ram_fc_semaphorea_w(void)
{
	return 14;
}
static inline u32 ram_fc_semaphoreb_w(void)
{
	return 15;
}
static inline u32 ram_fc_semaphorec_w(void)
{
	return 16;
}
static inline u32 ram_fc_semaphored_w(void)
{
	return 17;
}
static inline u32 ram_fc_gp_base_w(void)
{
	return 18;
}
static inline u32 ram_fc_gp_base_hi_w(void)
{
	return 19;
}
static inline u32 ram_fc_gp_fetch_w(void)
{
	return 20;
}
static inline u32 ram_fc_pb_fetch_w(void)
{
	return 21;
}
static inline u32 ram_fc_pb_fetch_hi_w(void)
{
	return 22;
}
static inline u32 ram_fc_pb_put_w(void)
{
	return 23;
}
static inline u32 ram_fc_pb_put_hi_w(void)
{
	return 24;
}
static inline u32 ram_fc_pb_header_w(void)
{
	return 33;
}
static inline u32 ram_fc_pb_count_w(void)
{
	return 34;
}
static inline u32 ram_fc_subdevice_w(void)
{
	return 37;
}
static inline u32 ram_fc_formats_w(void)
{
	return 39;
}
static inline u32 ram_fc_syncpointa_w(void)
{
	return 41;
}
static inline u32 ram_fc_syncpointb_w(void)
{
	return 42;
}
static inline u32 ram_fc_target_w(void)
{
	return 43;
}
static inline u32 ram_fc_hce_ctrl_w(void)
{
	return 57;
}
static inline u32 ram_fc_chid_w(void)
{
	return 58;
}
static inline u32 ram_fc_chid_id_f(u32 v)
{
	return (v & 0xfff) << 0;
}
static inline u32 ram_fc_chid_id_w(void)
{
	return 0;
}
static inline u32 ram_fc_eng_timeslice_w(void)
{
	return 62;
}
static inline u32 ram_fc_pb_timeslice_w(void)
{
	return 63;
}
static inline u32 ram_userd_base_shift_v(void)
{
	return 0x00000009;
}
static inline u32 ram_userd_chan_size_v(void)
{
	return 0x00000200;
}
static inline u32 ram_userd_put_w(void)
{
	return 16;
}
static inline u32 ram_userd_get_w(void)
{
	return 17;
}
static inline u32 ram_userd_ref_w(void)
{
	return 18;
}
static inline u32 ram_userd_put_hi_w(void)
{
	return 19;
}
static inline u32 ram_userd_ref_threshold_w(void)
{
	return 20;
}
static inline u32 ram_userd_top_level_get_w(void)
{
	return 22;
}
static inline u32 ram_userd_top_level_get_hi_w(void)
{
	return 23;
}
static inline u32 ram_userd_get_hi_w(void)
{
	return 24;
}
static inline u32 ram_userd_gp_get_w(void)
{
	return 34;
}
static inline u32 ram_userd_gp_put_w(void)
{
	return 35;
}
static inline u32 ram_userd_gp_top_level_get_w(void)
{
	return 22;
}
static inline u32 ram_userd_gp_top_level_get_hi_w(void)
{
	return 23;
}
static inline u32 ram_rl_entry_size_v(void)
{
	return 0x00000008;
}
#endif
