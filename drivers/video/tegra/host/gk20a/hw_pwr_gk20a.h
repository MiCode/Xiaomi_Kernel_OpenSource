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
#ifndef _hw_pwr_gk20a_h_
#define _hw_pwr_gk20a_h_

static inline u32 pwr_falcon_irqsset_r(void)
{
	return 0x0010a000;
}
static inline u32 pwr_falcon_irqsset_swgen0_set_f(void)
{
	return 0x40;
}
static inline u32 pwr_falcon_irqsclr_r(void)
{
	return 0x0010a004;
}
static inline u32 pwr_falcon_irqstat_r(void)
{
	return 0x0010a008;
}
static inline u32 pwr_falcon_irqstat_halt_true_f(void)
{
	return 0x10;
}
static inline u32 pwr_falcon_irqstat_exterr_true_f(void)
{
	return 0x20;
}
static inline u32 pwr_falcon_irqstat_swgen0_true_f(void)
{
	return 0x40;
}
static inline u32 pwr_falcon_irqmode_r(void)
{
	return 0x0010a00c;
}
static inline u32 pwr_falcon_irqmset_r(void)
{
	return 0x0010a010;
}
static inline u32 pwr_falcon_irqmset_gptmr_f(u32 v)
{
	return (v & 0x1) << 0;
}
static inline u32 pwr_falcon_irqmset_wdtmr_f(u32 v)
{
	return (v & 0x1) << 1;
}
static inline u32 pwr_falcon_irqmset_mthd_f(u32 v)
{
	return (v & 0x1) << 2;
}
static inline u32 pwr_falcon_irqmset_ctxsw_f(u32 v)
{
	return (v & 0x1) << 3;
}
static inline u32 pwr_falcon_irqmset_halt_f(u32 v)
{
	return (v & 0x1) << 4;
}
static inline u32 pwr_falcon_irqmset_exterr_f(u32 v)
{
	return (v & 0x1) << 5;
}
static inline u32 pwr_falcon_irqmset_swgen0_f(u32 v)
{
	return (v & 0x1) << 6;
}
static inline u32 pwr_falcon_irqmset_swgen1_f(u32 v)
{
	return (v & 0x1) << 7;
}
static inline u32 pwr_falcon_irqmclr_r(void)
{
	return 0x0010a014;
}
static inline u32 pwr_falcon_irqmclr_gptmr_f(u32 v)
{
	return (v & 0x1) << 0;
}
static inline u32 pwr_falcon_irqmclr_wdtmr_f(u32 v)
{
	return (v & 0x1) << 1;
}
static inline u32 pwr_falcon_irqmclr_mthd_f(u32 v)
{
	return (v & 0x1) << 2;
}
static inline u32 pwr_falcon_irqmclr_ctxsw_f(u32 v)
{
	return (v & 0x1) << 3;
}
static inline u32 pwr_falcon_irqmclr_halt_f(u32 v)
{
	return (v & 0x1) << 4;
}
static inline u32 pwr_falcon_irqmclr_exterr_f(u32 v)
{
	return (v & 0x1) << 5;
}
static inline u32 pwr_falcon_irqmclr_swgen0_f(u32 v)
{
	return (v & 0x1) << 6;
}
static inline u32 pwr_falcon_irqmclr_swgen1_f(u32 v)
{
	return (v & 0x1) << 7;
}
static inline u32 pwr_falcon_irqmclr_ext_f(u32 v)
{
	return (v & 0xff) << 8;
}
static inline u32 pwr_falcon_irqmask_r(void)
{
	return 0x0010a018;
}
static inline u32 pwr_falcon_irqdest_r(void)
{
	return 0x0010a01c;
}
static inline u32 pwr_falcon_irqdest_host_gptmr_f(u32 v)
{
	return (v & 0x1) << 0;
}
static inline u32 pwr_falcon_irqdest_host_wdtmr_f(u32 v)
{
	return (v & 0x1) << 1;
}
static inline u32 pwr_falcon_irqdest_host_mthd_f(u32 v)
{
	return (v & 0x1) << 2;
}
static inline u32 pwr_falcon_irqdest_host_ctxsw_f(u32 v)
{
	return (v & 0x1) << 3;
}
static inline u32 pwr_falcon_irqdest_host_halt_f(u32 v)
{
	return (v & 0x1) << 4;
}
static inline u32 pwr_falcon_irqdest_host_exterr_f(u32 v)
{
	return (v & 0x1) << 5;
}
static inline u32 pwr_falcon_irqdest_host_swgen0_f(u32 v)
{
	return (v & 0x1) << 6;
}
static inline u32 pwr_falcon_irqdest_host_swgen1_f(u32 v)
{
	return (v & 0x1) << 7;
}
static inline u32 pwr_falcon_irqdest_host_ext_f(u32 v)
{
	return (v & 0xff) << 8;
}
static inline u32 pwr_falcon_irqdest_target_gptmr_f(u32 v)
{
	return (v & 0x1) << 16;
}
static inline u32 pwr_falcon_irqdest_target_wdtmr_f(u32 v)
{
	return (v & 0x1) << 17;
}
static inline u32 pwr_falcon_irqdest_target_mthd_f(u32 v)
{
	return (v & 0x1) << 18;
}
static inline u32 pwr_falcon_irqdest_target_ctxsw_f(u32 v)
{
	return (v & 0x1) << 19;
}
static inline u32 pwr_falcon_irqdest_target_halt_f(u32 v)
{
	return (v & 0x1) << 20;
}
static inline u32 pwr_falcon_irqdest_target_exterr_f(u32 v)
{
	return (v & 0x1) << 21;
}
static inline u32 pwr_falcon_irqdest_target_swgen0_f(u32 v)
{
	return (v & 0x1) << 22;
}
static inline u32 pwr_falcon_irqdest_target_swgen1_f(u32 v)
{
	return (v & 0x1) << 23;
}
static inline u32 pwr_falcon_irqdest_target_ext_f(u32 v)
{
	return (v & 0xff) << 24;
}
static inline u32 pwr_falcon_curctx_r(void)
{
	return 0x0010a050;
}
static inline u32 pwr_falcon_nxtctx_r(void)
{
	return 0x0010a054;
}
static inline u32 pwr_falcon_mailbox0_r(void)
{
	return 0x0010a040;
}
static inline u32 pwr_falcon_mailbox1_r(void)
{
	return 0x0010a044;
}
static inline u32 pwr_falcon_itfen_r(void)
{
	return 0x0010a048;
}
static inline u32 pwr_falcon_itfen_ctxen_enable_f(void)
{
	return 0x1;
}
static inline u32 pwr_falcon_idlestate_r(void)
{
	return 0x0010a04c;
}
static inline u32 pwr_falcon_idlestate_falcon_busy_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 pwr_falcon_idlestate_ext_busy_v(u32 r)
{
	return (r >> 1) & 0x7fff;
}
static inline u32 pwr_falcon_os_r(void)
{
	return 0x0010a080;
}
static inline u32 pwr_falcon_engctl_r(void)
{
	return 0x0010a0a4;
}
static inline u32 pwr_falcon_cpuctl_r(void)
{
	return 0x0010a100;
}
static inline u32 pwr_falcon_cpuctl_startcpu_f(u32 v)
{
	return (v & 0x1) << 1;
}
static inline u32 pwr_falcon_bootvec_r(void)
{
	return 0x0010a104;
}
static inline u32 pwr_falcon_bootvec_vec_f(u32 v)
{
	return (v & 0xffffffff) << 0;
}
static inline u32 pwr_falcon_hwcfg_r(void)
{
	return 0x0010a108;
}
static inline u32 pwr_falcon_hwcfg_imem_size_v(u32 r)
{
	return (r >> 0) & 0x1ff;
}
static inline u32 pwr_falcon_hwcfg_dmem_size_v(u32 r)
{
	return (r >> 9) & 0x1ff;
}
static inline u32 pwr_falcon_dmatrfbase_r(void)
{
	return 0x0010a110;
}
static inline u32 pwr_falcon_dmatrfmoffs_r(void)
{
	return 0x0010a114;
}
static inline u32 pwr_falcon_dmatrfcmd_r(void)
{
	return 0x0010a118;
}
static inline u32 pwr_falcon_dmatrfcmd_imem_f(u32 v)
{
	return (v & 0x1) << 4;
}
static inline u32 pwr_falcon_dmatrfcmd_write_f(u32 v)
{
	return (v & 0x1) << 5;
}
static inline u32 pwr_falcon_dmatrfcmd_size_f(u32 v)
{
	return (v & 0x7) << 8;
}
static inline u32 pwr_falcon_dmatrfcmd_ctxdma_f(u32 v)
{
	return (v & 0x7) << 12;
}
static inline u32 pwr_falcon_dmatrffboffs_r(void)
{
	return 0x0010a11c;
}
static inline u32 pwr_falcon_exterraddr_r(void)
{
	return 0x0010a168;
}
static inline u32 pwr_falcon_exterrstat_r(void)
{
	return 0x0010a16c;
}
static inline u32 pwr_falcon_exterrstat_valid_m(void)
{
	return 0x1 << 31;
}
static inline u32 pwr_falcon_exterrstat_valid_v(u32 r)
{
	return (r >> 31) & 0x1;
}
static inline u32 pwr_falcon_exterrstat_valid_true_v(void)
{
	return 0x00000001;
}
static inline u32 pwr_pmu_falcon_icd_cmd_r(void)
{
	return 0x0010a200;
}
static inline u32 pwr_pmu_falcon_icd_cmd_opc_s(void)
{
	return 4;
}
static inline u32 pwr_pmu_falcon_icd_cmd_opc_f(u32 v)
{
	return (v & 0xf) << 0;
}
static inline u32 pwr_pmu_falcon_icd_cmd_opc_m(void)
{
	return 0xf << 0;
}
static inline u32 pwr_pmu_falcon_icd_cmd_opc_v(u32 r)
{
	return (r >> 0) & 0xf;
}
static inline u32 pwr_pmu_falcon_icd_cmd_opc_rreg_f(void)
{
	return 0x8;
}
static inline u32 pwr_pmu_falcon_icd_cmd_opc_rstat_f(void)
{
	return 0xe;
}
static inline u32 pwr_pmu_falcon_icd_cmd_idx_f(u32 v)
{
	return (v & 0x1f) << 8;
}
static inline u32 pwr_pmu_falcon_icd_rdata_r(void)
{
	return 0x0010a20c;
}
static inline u32 pwr_falcon_dmemc_r(u32 i)
{
	return 0x0010a1c0 + i*8;
}
static inline u32 pwr_falcon_dmemc_offs_f(u32 v)
{
	return (v & 0x3f) << 2;
}
static inline u32 pwr_falcon_dmemc_offs_m(void)
{
	return 0x3f << 2;
}
static inline u32 pwr_falcon_dmemc_blk_f(u32 v)
{
	return (v & 0xff) << 8;
}
static inline u32 pwr_falcon_dmemc_blk_m(void)
{
	return 0xff << 8;
}
static inline u32 pwr_falcon_dmemc_aincw_f(u32 v)
{
	return (v & 0x1) << 24;
}
static inline u32 pwr_falcon_dmemc_aincr_f(u32 v)
{
	return (v & 0x1) << 25;
}
static inline u32 pwr_falcon_dmemd_r(u32 i)
{
	return 0x0010a1c4 + i*8;
}
static inline u32 pwr_pmu_new_instblk_r(void)
{
	return 0x0010a480;
}
static inline u32 pwr_pmu_new_instblk_ptr_f(u32 v)
{
	return (v & 0xfffffff) << 0;
}
static inline u32 pwr_pmu_new_instblk_target_fb_f(void)
{
	return 0x0;
}
static inline u32 pwr_pmu_new_instblk_target_sys_coh_f(void)
{
	return 0x20000000;
}
static inline u32 pwr_pmu_new_instblk_valid_f(u32 v)
{
	return (v & 0x1) << 30;
}
static inline u32 pwr_pmu_mutex_id_r(void)
{
	return 0x0010a488;
}
static inline u32 pwr_pmu_mutex_id_value_v(u32 r)
{
	return (r >> 0) & 0xff;
}
static inline u32 pwr_pmu_mutex_id_value_init_v(void)
{
	return 0x00000000;
}
static inline u32 pwr_pmu_mutex_id_value_not_avail_v(void)
{
	return 0x000000ff;
}
static inline u32 pwr_pmu_mutex_id_release_r(void)
{
	return 0x0010a48c;
}
static inline u32 pwr_pmu_mutex_id_release_value_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 pwr_pmu_mutex_id_release_value_m(void)
{
	return 0xff << 0;
}
static inline u32 pwr_pmu_mutex_id_release_value_init_v(void)
{
	return 0x00000000;
}
static inline u32 pwr_pmu_mutex_id_release_value_init_f(void)
{
	return 0x0;
}
static inline u32 pwr_pmu_mutex_r(u32 i)
{
	return 0x0010a580 + i*4;
}
static inline u32 pwr_pmu_mutex__size_1_v(void)
{
	return 0x00000010;
}
static inline u32 pwr_pmu_mutex_value_f(u32 v)
{
	return (v & 0xff) << 0;
}
static inline u32 pwr_pmu_mutex_value_v(u32 r)
{
	return (r >> 0) & 0xff;
}
static inline u32 pwr_pmu_mutex_value_initial_lock_f(void)
{
	return 0x0;
}
static inline u32 pwr_pmu_queue_head_r(u32 i)
{
	return 0x0010a4a0 + i*4;
}
static inline u32 pwr_pmu_queue_head__size_1_v(void)
{
	return 0x00000004;
}
static inline u32 pwr_pmu_queue_head_address_f(u32 v)
{
	return (v & 0xffffffff) << 0;
}
static inline u32 pwr_pmu_queue_head_address_v(u32 r)
{
	return (r >> 0) & 0xffffffff;
}
static inline u32 pwr_pmu_queue_tail_r(u32 i)
{
	return 0x0010a4b0 + i*4;
}
static inline u32 pwr_pmu_queue_tail__size_1_v(void)
{
	return 0x00000004;
}
static inline u32 pwr_pmu_queue_tail_address_f(u32 v)
{
	return (v & 0xffffffff) << 0;
}
static inline u32 pwr_pmu_queue_tail_address_v(u32 r)
{
	return (r >> 0) & 0xffffffff;
}
static inline u32 pwr_pmu_msgq_head_r(void)
{
	return 0x0010a4c8;
}
static inline u32 pwr_pmu_msgq_head_val_f(u32 v)
{
	return (v & 0xffffffff) << 0;
}
static inline u32 pwr_pmu_msgq_head_val_v(u32 r)
{
	return (r >> 0) & 0xffffffff;
}
static inline u32 pwr_pmu_msgq_tail_r(void)
{
	return 0x0010a4cc;
}
static inline u32 pwr_pmu_msgq_tail_val_f(u32 v)
{
	return (v & 0xffffffff) << 0;
}
static inline u32 pwr_pmu_msgq_tail_val_v(u32 r)
{
	return (r >> 0) & 0xffffffff;
}
static inline u32 pwr_pmu_idle_mask_r(u32 i)
{
	return 0x0010a504 + i*16;
}
static inline u32 pwr_pmu_idle_mask_gr_enabled_f(void)
{
	return 0x1;
}
static inline u32 pwr_pmu_idle_mask_ce_2_enabled_f(void)
{
	return 0x200000;
}
static inline u32 pwr_pmu_idle_count_r(u32 i)
{
	return 0x0010a508 + i*16;
}
static inline u32 pwr_pmu_idle_count_value_f(u32 v)
{
	return (v & 0x7fffffff) << 0;
}
static inline u32 pwr_pmu_idle_count_value_v(u32 r)
{
	return (r >> 0) & 0x7fffffff;
}
static inline u32 pwr_pmu_idle_count_reset_f(u32 v)
{
	return (v & 0x1) << 31;
}
static inline u32 pwr_pmu_idle_ctrl_r(u32 i)
{
	return 0x0010a50c + i*16;
}
static inline u32 pwr_pmu_idle_ctrl_value_m(void)
{
	return 0x3 << 0;
}
static inline u32 pwr_pmu_idle_ctrl_value_busy_f(void)
{
	return 0x2;
}
static inline u32 pwr_pmu_idle_ctrl_value_always_f(void)
{
	return 0x3;
}
static inline u32 pwr_pmu_idle_ctrl_filter_m(void)
{
	return 0x1 << 2;
}
static inline u32 pwr_pmu_idle_ctrl_filter_disabled_f(void)
{
	return 0x0;
}
static inline u32 pwr_pmu_idle_mask_supp_r(u32 i)
{
	return 0x0010a9f0 + i*8;
}
static inline u32 pwr_pmu_idle_mask_1_supp_r(u32 i)
{
	return 0x0010a9f4 + i*8;
}
static inline u32 pwr_pmu_idle_ctrl_supp_r(u32 i)
{
	return 0x0010aa30 + i*8;
}
static inline u32 pwr_pmu_debug_r(u32 i)
{
	return 0x0010a5c0 + i*4;
}
static inline u32 pwr_pmu_debug__size_1_v(void)
{
	return 0x00000004;
}
static inline u32 pwr_pmu_mailbox_r(u32 i)
{
	return 0x0010a450 + i*4;
}
static inline u32 pwr_pmu_mailbox__size_1_v(void)
{
	return 0x0000000c;
}
static inline u32 pwr_pmu_bar0_addr_r(void)
{
	return 0x0010a7a0;
}
static inline u32 pwr_pmu_bar0_data_r(void)
{
	return 0x0010a7a4;
}
static inline u32 pwr_pmu_bar0_ctl_r(void)
{
	return 0x0010a7ac;
}
static inline u32 pwr_pmu_bar0_timeout_r(void)
{
	return 0x0010a7a8;
}
static inline u32 pwr_pmu_bar0_fecs_error_r(void)
{
	return 0x0010a988;
}
static inline u32 pwr_pmu_bar0_error_status_r(void)
{
	return 0x0010a7b0;
}
static inline u32 pwr_pmu_pg_idlefilth_r(u32 i)
{
	return 0x0010a6c0 + i*4;
}
static inline u32 pwr_pmu_pg_ppuidlefilth_r(u32 i)
{
	return 0x0010a6e8 + i*4;
}
static inline u32 pwr_pmu_pg_idle_cnt_r(u32 i)
{
	return 0x0010a710 + i*4;
}
static inline u32 pwr_pmu_pg_intren_r(u32 i)
{
	return 0x0010a760 + i*4;
}
static inline u32 pwr_fbif_transcfg_r(u32 i)
{
	return 0x0010a600 + i*4;
}
static inline u32 pwr_fbif_transcfg_target_local_fb_f(void)
{
	return 0x0;
}
static inline u32 pwr_fbif_transcfg_target_coherent_sysmem_f(void)
{
	return 0x1;
}
static inline u32 pwr_fbif_transcfg_target_noncoherent_sysmem_f(void)
{
	return 0x2;
}
static inline u32 pwr_fbif_transcfg_mem_type_s(void)
{
	return 1;
}
static inline u32 pwr_fbif_transcfg_mem_type_f(u32 v)
{
	return (v & 0x1) << 2;
}
static inline u32 pwr_fbif_transcfg_mem_type_m(void)
{
	return 0x1 << 2;
}
static inline u32 pwr_fbif_transcfg_mem_type_v(u32 r)
{
	return (r >> 2) & 0x1;
}
static inline u32 pwr_fbif_transcfg_mem_type_virtual_f(void)
{
	return 0x0;
}
static inline u32 pwr_fbif_transcfg_mem_type_physical_f(void)
{
	return 0x4;
}
#endif
