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
#ifndef _hw_fifo_gk20a_h_
#define _hw_fifo_gk20a_h_

static inline u32 fifo_bar1_base_r(void)
{
	return 0x00002254;
}
static inline u32 fifo_bar1_base_ptr_f(u32 v)
{
	return (v & 0xfffffff) << 0;
}
static inline u32 fifo_bar1_base_ptr_align_shift_v(void)
{
	return 0x0000000c;
}
static inline u32 fifo_bar1_base_valid_false_f(void)
{
	return 0x0;
}
static inline u32 fifo_bar1_base_valid_true_f(void)
{
	return 0x10000000;
}
static inline u32 fifo_runlist_base_r(void)
{
	return 0x00002270;
}
static inline u32 fifo_runlist_base_ptr_f(u32 v)
{
	return (v & 0xfffffff) << 0;
}
static inline u32 fifo_runlist_base_target_vid_mem_f(void)
{
	return 0x0;
}
static inline u32 fifo_runlist_r(void)
{
	return 0x00002274;
}
static inline u32 fifo_runlist_engine_f(u32 v)
{
	return (v & 0xf) << 20;
}
static inline u32 fifo_eng_runlist_base_r(u32 i)
{
	return 0x00002280 + i*8;
}
static inline u32 fifo_eng_runlist_base__size_1_v(void)
{
	return 0x00000001;
}
static inline u32 fifo_eng_runlist_r(u32 i)
{
	return 0x00002284 + i*8;
}
static inline u32 fifo_eng_runlist__size_1_v(void)
{
	return 0x00000001;
}
static inline u32 fifo_eng_runlist_length_f(u32 v)
{
	return (v & 0xffff) << 0;
}
static inline u32 fifo_eng_runlist_pending_true_f(void)
{
	return 0x100000;
}
static inline u32 fifo_eng_timeslice_r(u32 i)
{
	return 0x00002310 + i*4;
}
static inline u32 fifo_eng_timeslice_timeout_128_f(void)
{
	return 0x80;
}
static inline u32 fifo_eng_timeslice_timescale_3_f(void)
{
	return 0x3000;
}
static inline u32 fifo_eng_timeslice_enable_true_f(void)
{
	return 0x10000000;
}
static inline u32 fifo_pb_timeslice_r(u32 i)
{
	return 0x00002350 + i*4;
}
static inline u32 fifo_pb_timeslice_timeout_16_f(void)
{
	return 0x10;
}
static inline u32 fifo_pb_timeslice_timescale_0_f(void)
{
	return 0x0;
}
static inline u32 fifo_pb_timeslice_enable_true_f(void)
{
	return 0x10000000;
}
static inline u32 fifo_pbdma_map_r(u32 i)
{
	return 0x00002390 + i*4;
}
static inline u32 fifo_intr_0_r(void)
{
	return 0x00002100;
}
static inline u32 fifo_intr_0_bind_error_pending_f(void)
{
	return 0x1;
}
static inline u32 fifo_intr_0_bind_error_reset_f(void)
{
	return 0x1;
}
static inline u32 fifo_intr_0_pio_error_pending_f(void)
{
	return 0x10;
}
static inline u32 fifo_intr_0_pio_error_reset_f(void)
{
	return 0x10;
}
static inline u32 fifo_intr_0_sched_error_pending_f(void)
{
	return 0x100;
}
static inline u32 fifo_intr_0_sched_error_reset_f(void)
{
	return 0x100;
}
static inline u32 fifo_intr_0_chsw_error_pending_f(void)
{
	return 0x10000;
}
static inline u32 fifo_intr_0_chsw_error_reset_f(void)
{
	return 0x10000;
}
static inline u32 fifo_intr_0_fb_flush_timeout_pending_f(void)
{
	return 0x800000;
}
static inline u32 fifo_intr_0_fb_flush_timeout_reset_f(void)
{
	return 0x800000;
}
static inline u32 fifo_intr_0_lb_error_pending_f(void)
{
	return 0x1000000;
}
static inline u32 fifo_intr_0_lb_error_reset_f(void)
{
	return 0x1000000;
}
static inline u32 fifo_intr_0_dropped_mmu_fault_pending_f(void)
{
	return 0x8000000;
}
static inline u32 fifo_intr_0_dropped_mmu_fault_reset_f(void)
{
	return 0x8000000;
}
static inline u32 fifo_intr_0_mmu_fault_pending_f(void)
{
	return 0x10000000;
}
static inline u32 fifo_intr_0_pbdma_intr_pending_f(void)
{
	return 0x20000000;
}
static inline u32 fifo_intr_0_runlist_event_pending_f(void)
{
	return 0x40000000;
}
static inline u32 fifo_intr_0_channel_intr_pending_f(void)
{
	return 0x80000000;
}
static inline u32 fifo_intr_en_0_r(void)
{
	return 0x00002140;
}
static inline u32 fifo_intr_en_1_r(void)
{
	return 0x00002528;
}
static inline u32 fifo_intr_bind_error_r(void)
{
	return 0x0000252c;
}
static inline u32 fifo_intr_sched_error_r(void)
{
	return 0x0000254c;
}
static inline u32 fifo_intr_sched_error_code_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 fifo_intr_sched_error_code_ctxsw_timeout_v(void)
{
	return 0x0000000a;
}
static inline u32 fifo_intr_chsw_error_r(void)
{
	return 0x0000256c;
}
static inline u32 fifo_intr_mmu_fault_id_r(void)
{
	return 0x0000259c;
}
static inline u32 fifo_intr_mmu_fault_eng_id_graphics_v(void)
{
	return 0x00000000;
}
static inline u32 fifo_intr_mmu_fault_eng_id_graphics_f(void)
{
	return 0x0;
}
static inline u32 fifo_intr_mmu_fault_inst_r(u32 i)
{
	return 0x00002800 + i*16;
}
static inline u32 fifo_intr_mmu_fault_inst_ptr_v(u32 r)
{
	return (r >> 0) & 0xfffffff;
}
static inline u32 fifo_intr_mmu_fault_inst_ptr_align_shift_v(void)
{
	return 0x0000000c;
}
static inline u32 fifo_intr_mmu_fault_lo_r(u32 i)
{
	return 0x00002804 + i*16;
}
static inline u32 fifo_intr_mmu_fault_hi_r(u32 i)
{
	return 0x00002808 + i*16;
}
static inline u32 fifo_intr_mmu_fault_info_r(u32 i)
{
	return 0x0000280c + i*16;
}
static inline u32 fifo_intr_mmu_fault_info_type_v(u32 r)
{
	return (r >> 0) & 0xf;
}
static inline u32 fifo_intr_mmu_fault_info_engine_subid_v(u32 r)
{
	return (r >> 6) & 0x1;
}
static inline u32 fifo_intr_mmu_fault_info_engine_subid_gpc_v(void)
{
	return 0x00000000;
}
static inline u32 fifo_intr_mmu_fault_info_engine_subid_hub_v(void)
{
	return 0x00000001;
}
static inline u32 fifo_intr_mmu_fault_info_client_v(u32 r)
{
	return (r >> 8) & 0x1f;
}
static inline u32 fifo_intr_pbdma_id_r(void)
{
	return 0x000025a0;
}
static inline u32 fifo_intr_pbdma_id_status_f(u32 v, u32 i)
{
	return (v & 0x1) << (0 + i*1);
}
static inline u32 fifo_intr_pbdma_id_status__size_1_v(void)
{
	return 0x00000001;
}
static inline u32 fifo_intr_runlist_r(void)
{
	return 0x00002a00;
}
static inline u32 fifo_fb_timeout_r(void)
{
	return 0x00002a04;
}
static inline u32 fifo_fb_timeout_period_m(void)
{
	return 0x3fffffff << 0;
}
static inline u32 fifo_fb_timeout_period_max_f(void)
{
	return 0x3fffffff;
}
static inline u32 fifo_pb_timeout_r(void)
{
	return 0x00002a08;
}
static inline u32 fifo_pb_timeout_detection_enabled_f(void)
{
	return 0x80000000;
}
static inline u32 fifo_eng_timeout_r(void)
{
	return 0x00002a0c;
}
static inline u32 fifo_eng_timeout_period_m(void)
{
	return 0x7fffffff << 0;
}
static inline u32 fifo_eng_timeout_period_max_f(void)
{
	return 0x7fffffff;
}
static inline u32 fifo_eng_timeout_detection_m(void)
{
	return 0x1 << 31;
}
static inline u32 fifo_eng_timeout_detection_enabled_f(void)
{
	return 0x80000000;
}
static inline u32 fifo_eng_timeout_detection_disabled_f(void)
{
	return 0x0;
}
static inline u32 fifo_error_sched_disable_r(void)
{
	return 0x0000262c;
}
static inline u32 fifo_sched_disable_r(void)
{
	return 0x00002630;
}
static inline u32 fifo_sched_disable_runlist_f(u32 v, u32 i)
{
	return (v & 0x1) << (0 + i*1);
}
static inline u32 fifo_sched_disable_runlist_m(u32 i)
{
	return 0x1 << (0 + i*1);
}
static inline u32 fifo_sched_disable_true_v(void)
{
	return 0x00000001;
}
static inline u32 fifo_preempt_r(void)
{
	return 0x00002634;
}
static inline u32 fifo_preempt_pending_true_f(void)
{
	return 0x100000;
}
static inline u32 fifo_preempt_type_channel_f(void)
{
	return 0x0;
}
static inline u32 fifo_preempt_chid_f(u32 v)
{
	return (v & 0xfff) << 0;
}
static inline u32 fifo_trigger_mmu_fault_r(u32 i)
{
	return 0x00002a30 + i*4;
}
static inline u32 fifo_trigger_mmu_fault_id_f(u32 v)
{
	return (v & 0x1f) << 0;
}
static inline u32 fifo_trigger_mmu_fault_enable_f(u32 v)
{
	return (v & 0x1) << 8;
}
static inline u32 fifo_engine_status_r(u32 i)
{
	return 0x00002640 + i*8;
}
static inline u32 fifo_engine_status__size_1_v(void)
{
	return 0x00000002;
}
static inline u32 fifo_engine_status_id_v(u32 r)
{
	return (r >> 0) & 0xfff;
}
static inline u32 fifo_engine_status_id_type_v(u32 r)
{
	return (r >> 12) & 0x1;
}
static inline u32 fifo_engine_status_id_type_chid_v(void)
{
	return 0x00000000;
}
static inline u32 fifo_engine_status_ctx_status_v(u32 r)
{
	return (r >> 13) & 0x7;
}
static inline u32 fifo_engine_status_ctx_status_valid_v(void)
{
	return 0x00000001;
}
static inline u32 fifo_engine_status_ctx_status_ctxsw_load_v(void)
{
	return 0x00000005;
}
static inline u32 fifo_engine_status_ctx_status_ctxsw_save_v(void)
{
	return 0x00000006;
}
static inline u32 fifo_engine_status_ctx_status_ctxsw_switch_v(void)
{
	return 0x00000007;
}
static inline u32 fifo_engine_status_next_id_v(u32 r)
{
	return (r >> 16) & 0xfff;
}
static inline u32 fifo_engine_status_next_id_type_v(u32 r)
{
	return (r >> 28) & 0x1;
}
static inline u32 fifo_engine_status_next_id_type_chid_v(void)
{
	return 0x00000000;
}
static inline u32 fifo_engine_status_faulted_v(u32 r)
{
	return (r >> 30) & 0x1;
}
static inline u32 fifo_engine_status_faulted_true_v(void)
{
	return 0x00000001;
}
static inline u32 fifo_engine_status_engine_v(u32 r)
{
	return (r >> 31) & 0x1;
}
static inline u32 fifo_engine_status_engine_idle_v(void)
{
	return 0x00000000;
}
static inline u32 fifo_engine_status_engine_busy_v(void)
{
	return 0x00000001;
}
static inline u32 fifo_engine_status_ctxsw_v(u32 r)
{
	return (r >> 15) & 0x1;
}
static inline u32 fifo_engine_status_ctxsw_in_progress_v(void)
{
	return 0x00000001;
}
static inline u32 fifo_engine_status_ctxsw_in_progress_f(void)
{
	return 0x8000;
}
static inline u32 fifo_pbdma_status_r(u32 i)
{
	return 0x00003080 + i*4;
}
static inline u32 fifo_pbdma_status__size_1_v(void)
{
	return 0x00000001;
}
static inline u32 fifo_pbdma_status_id_v(u32 r)
{
	return (r >> 0) & 0xfff;
}
static inline u32 fifo_pbdma_status_id_type_v(u32 r)
{
	return (r >> 12) & 0x1;
}
static inline u32 fifo_pbdma_status_id_type_chid_v(void)
{
	return 0x00000000;
}
static inline u32 fifo_pbdma_status_chan_status_v(u32 r)
{
	return (r >> 13) & 0x7;
}
static inline u32 fifo_pbdma_status_chan_status_valid_v(void)
{
	return 0x00000001;
}
static inline u32 fifo_pbdma_status_chan_status_chsw_load_v(void)
{
	return 0x00000005;
}
static inline u32 fifo_pbdma_status_chan_status_chsw_save_v(void)
{
	return 0x00000006;
}
static inline u32 fifo_pbdma_status_chan_status_chsw_switch_v(void)
{
	return 0x00000007;
}
static inline u32 fifo_pbdma_status_next_id_v(u32 r)
{
	return (r >> 16) & 0xfff;
}
static inline u32 fifo_pbdma_status_next_id_type_v(u32 r)
{
	return (r >> 28) & 0x1;
}
static inline u32 fifo_pbdma_status_next_id_type_chid_v(void)
{
	return 0x00000000;
}
static inline u32 fifo_pbdma_status_chsw_v(u32 r)
{
	return (r >> 15) & 0x1;
}
static inline u32 fifo_pbdma_status_chsw_in_progress_v(void)
{
	return 0x00000001;
}
#endif
