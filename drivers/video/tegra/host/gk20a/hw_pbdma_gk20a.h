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
#ifndef _hw_pbdma_gk20a_h_
#define _hw_pbdma_gk20a_h_

static inline u32 pbdma_gp_entry1_r(void)
{
	return 0x10000004;
}
static inline u32 pbdma_gp_entry1_get_hi_v(u32 r)
{
	return (r >> 0) & 0xff;
}
static inline u32 pbdma_gp_entry1_length_f(u32 v)
{
	return (v & 0x1fffff) << 10;
}
static inline u32 pbdma_gp_entry1_length_v(u32 r)
{
	return (r >> 10) & 0x1fffff;
}
static inline u32 pbdma_gp_base_r(u32 i)
{
	return 0x00040048 + i*8192;
}
static inline u32 pbdma_gp_base__size_1_v(void)
{
	return 0x00000001;
}
static inline u32 pbdma_gp_base_offset_f(u32 v)
{
	return (v & 0x1fffffff) << 3;
}
static inline u32 pbdma_gp_base_rsvd_s(void)
{
	return 3;
}
static inline u32 pbdma_gp_base_hi_r(u32 i)
{
	return 0x0004004c + i*8192;
}
static inline u32 pbdma_gp_base_hi_offset_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 pbdma_gp_base_hi_limit2_f(u32 v)
{
	return (v & 0x1f) << 16;
}
static inline u32 pbdma_gp_fetch_r(u32 i)
{
	return 0x00040050 + i*8192;
}
static inline u32 pbdma_gp_get_r(u32 i)
{
	return 0x00040014 + i*8192;
}
static inline u32 pbdma_gp_put_r(u32 i)
{
	return 0x00040000 + i*8192;
}
static inline u32 pbdma_pb_fetch_r(u32 i)
{
	return 0x00040054 + i*8192;
}
static inline u32 pbdma_pb_fetch_hi_r(u32 i)
{
	return 0x00040058 + i*8192;
}
static inline u32 pbdma_get_r(u32 i)
{
	return 0x00040018 + i*8192;
}
static inline u32 pbdma_get_hi_r(u32 i)
{
	return 0x0004001c + i*8192;
}
static inline u32 pbdma_put_r(u32 i)
{
	return 0x0004005c + i*8192;
}
static inline u32 pbdma_put_hi_r(u32 i)
{
	return 0x00040060 + i*8192;
}
static inline u32 pbdma_formats_r(u32 i)
{
	return 0x0004009c + i*8192;
}
static inline u32 pbdma_formats_gp_fermi0_f(void)
{
	return 0x0;
}
static inline u32 pbdma_formats_pb_fermi1_f(void)
{
	return 0x100;
}
static inline u32 pbdma_formats_mp_fermi0_f(void)
{
	return 0x0;
}
static inline u32 pbdma_pb_header_r(u32 i)
{
	return 0x00040084 + i*8192;
}
static inline u32 pbdma_pb_header_priv_user_f(void)
{
	return 0x0;
}
static inline u32 pbdma_pb_header_method_zero_f(void)
{
	return 0x0;
}
static inline u32 pbdma_pb_header_subchannel_zero_f(void)
{
	return 0x0;
}
static inline u32 pbdma_pb_header_level_main_f(void)
{
	return 0x0;
}
static inline u32 pbdma_pb_header_first_true_f(void)
{
	return 0x400000;
}
static inline u32 pbdma_pb_header_type_inc_f(void)
{
	return 0x20000000;
}
static inline u32 pbdma_subdevice_r(u32 i)
{
	return 0x00040094 + i*8192;
}
static inline u32 pbdma_subdevice_id_f(u32 v)
{
	return (v & 0xfff) << 0;
}
static inline u32 pbdma_subdevice_status_active_f(void)
{
	return 0x10000000;
}
static inline u32 pbdma_subdevice_channel_dma_enable_f(void)
{
	return 0x20000000;
}
static inline u32 pbdma_method0_r(u32 i)
{
	return 0x000400c0 + i*8192;
}
static inline u32 pbdma_data0_r(u32 i)
{
	return 0x000400c4 + i*8192;
}
static inline u32 pbdma_target_r(u32 i)
{
	return 0x000400ac + i*8192;
}
static inline u32 pbdma_target_engine_sw_f(void)
{
	return 0x1f;
}
static inline u32 pbdma_acquire_r(u32 i)
{
	return 0x00040030 + i*8192;
}
static inline u32 pbdma_acquire_retry_man_2_f(void)
{
	return 0x2;
}
static inline u32 pbdma_acquire_retry_exp_2_f(void)
{
	return 0x100;
}
static inline u32 pbdma_acquire_timeout_exp_max_f(void)
{
	return 0x7800;
}
static inline u32 pbdma_acquire_timeout_man_max_f(void)
{
	return 0x7fff8000;
}
static inline u32 pbdma_acquire_timeout_en_disable_f(void)
{
	return 0x0;
}
static inline u32 pbdma_status_r(u32 i)
{
	return 0x00040100 + i*8192;
}
static inline u32 pbdma_channel_r(u32 i)
{
	return 0x00040120 + i*8192;
}
static inline u32 pbdma_signature_r(u32 i)
{
	return 0x00040010 + i*8192;
}
static inline u32 pbdma_signature_hw_valid_f(void)
{
	return 0xface;
}
static inline u32 pbdma_signature_sw_zero_f(void)
{
	return 0x0;
}
static inline u32 pbdma_userd_r(u32 i)
{
	return 0x00040008 + i*8192;
}
static inline u32 pbdma_userd_target_vid_mem_f(void)
{
	return 0x0;
}
static inline u32 pbdma_userd_addr_f(u32 v)
{
	return (v & 0x7fffff) << 9;
}
static inline u32 pbdma_userd_hi_r(u32 i)
{
	return 0x0004000c + i*8192;
}
static inline u32 pbdma_userd_hi_addr_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 pbdma_hce_ctrl_r(u32 i)
{
	return 0x000400e4 + i*8192;
}
static inline u32 pbdma_hce_ctrl_hce_priv_mode_yes_f(void)
{
	return 0x20;
}
static inline u32 pbdma_intr_0_r(u32 i)
{
	return 0x00040108 + i*8192;
}
static inline u32 pbdma_intr_0_memreq_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 pbdma_intr_0_memreq_pending_f(void)
{
	return 0x1;
}
static inline u32 pbdma_intr_0_memack_timeout_pending_f(void)
{
	return 0x2;
}
static inline u32 pbdma_intr_0_memack_extra_pending_f(void)
{
	return 0x4;
}
static inline u32 pbdma_intr_0_memdat_timeout_pending_f(void)
{
	return 0x8;
}
static inline u32 pbdma_intr_0_memdat_extra_pending_f(void)
{
	return 0x10;
}
static inline u32 pbdma_intr_0_memflush_pending_f(void)
{
	return 0x20;
}
static inline u32 pbdma_intr_0_memop_pending_f(void)
{
	return 0x40;
}
static inline u32 pbdma_intr_0_lbconnect_pending_f(void)
{
	return 0x80;
}
static inline u32 pbdma_intr_0_lbreq_pending_f(void)
{
	return 0x100;
}
static inline u32 pbdma_intr_0_lback_timeout_pending_f(void)
{
	return 0x200;
}
static inline u32 pbdma_intr_0_lback_extra_pending_f(void)
{
	return 0x400;
}
static inline u32 pbdma_intr_0_lbdat_timeout_pending_f(void)
{
	return 0x800;
}
static inline u32 pbdma_intr_0_lbdat_extra_pending_f(void)
{
	return 0x1000;
}
static inline u32 pbdma_intr_0_gpfifo_pending_f(void)
{
	return 0x2000;
}
static inline u32 pbdma_intr_0_gpptr_pending_f(void)
{
	return 0x4000;
}
static inline u32 pbdma_intr_0_gpentry_pending_f(void)
{
	return 0x8000;
}
static inline u32 pbdma_intr_0_gpcrc_pending_f(void)
{
	return 0x10000;
}
static inline u32 pbdma_intr_0_pbptr_pending_f(void)
{
	return 0x20000;
}
static inline u32 pbdma_intr_0_pbentry_pending_f(void)
{
	return 0x40000;
}
static inline u32 pbdma_intr_0_pbcrc_pending_f(void)
{
	return 0x80000;
}
static inline u32 pbdma_intr_0_xbarconnect_pending_f(void)
{
	return 0x100000;
}
static inline u32 pbdma_intr_0_method_pending_f(void)
{
	return 0x200000;
}
static inline u32 pbdma_intr_0_methodcrc_pending_f(void)
{
	return 0x400000;
}
static inline u32 pbdma_intr_0_device_pending_f(void)
{
	return 0x800000;
}
static inline u32 pbdma_intr_0_semaphore_pending_f(void)
{
	return 0x2000000;
}
static inline u32 pbdma_intr_0_acquire_pending_f(void)
{
	return 0x4000000;
}
static inline u32 pbdma_intr_0_pri_pending_f(void)
{
	return 0x8000000;
}
static inline u32 pbdma_intr_0_no_ctxsw_seg_pending_f(void)
{
	return 0x20000000;
}
static inline u32 pbdma_intr_0_pbseg_pending_f(void)
{
	return 0x40000000;
}
static inline u32 pbdma_intr_0_signature_pending_f(void)
{
	return 0x80000000;
}
static inline u32 pbdma_intr_1_r(u32 i)
{
	return 0x00040148 + i*8192;
}
static inline u32 pbdma_intr_en_0_r(u32 i)
{
	return 0x0004010c + i*8192;
}
static inline u32 pbdma_intr_en_0_lbreq_enabled_f(void)
{
	return 0x100;
}
static inline u32 pbdma_intr_en_1_r(u32 i)
{
	return 0x0004014c + i*8192;
}
static inline u32 pbdma_intr_stall_r(u32 i)
{
	return 0x0004013c + i*8192;
}
static inline u32 pbdma_intr_stall_lbreq_enabled_f(void)
{
	return 0x100;
}
static inline u32 pbdma_udma_nop_r(void)
{
	return 0x00000008;
}
#endif
