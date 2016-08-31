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
#ifndef _hw_host1x04_sync_h_
#define _hw_host1x04_sync_h_

static inline u32 host1x_sync_intstatus_r(void)
{
	return 0x0;
}
static inline u32 host1x_sync_intmask_r(void)
{
	return 0x4;
}
static inline u32 host1x_sync_intc0mask_r(void)
{
	return 0x8;
}
static inline u32 host1x_sync_hintstatus_r(void)
{
	return 0x20;
}
static inline u32 host1x_sync_hintmask_r(void)
{
	return 0x24;
}
static inline u32 host1x_sync_hintstatus_ext_r(void)
{
	return 0x28;
}
static inline u32 host1x_sync_hintstatus_ext_ip_read_int_v(u32 r)
{
	return (r >> 30) & 0x1;
}
static inline u32 host1x_sync_hintstatus_ext_ip_write_int_v(u32 r)
{
	return (r >> 31) & 0x1;
}
static inline u32 host1x_sync_hintmask_ext_r(void)
{
	return 0x2c;
}
static inline u32 host1x_sync_cmdproc_stat_r(void)
{
	return 0xa8;
}
static inline u32 host1x_sync_cmdproc_stop_r(void)
{
	return 0xac;
}
static inline u32 host1x_sync_ch_teardown_r(void)
{
	return 0xb0;
}
static inline u32 host1x_sync_mod_teardown_r(void)
{
	return 0xb4;
}
static inline u32 host1x_sync_usec_clk_r(void)
{
	return 0x1a4;
}
static inline u32 host1x_sync_ctxsw_timeout_cfg_r(void)
{
	return 0x1a8;
}
static inline u32 host1x_sync_ip_busy_timeout_r(void)
{
	return 0x1bc;
}
static inline u32 host1x_sync_ip_read_timeout_addr_r(void)
{
	return 0x1c0;
}
static inline u32 host1x_sync_ip_write_timeout_addr_r(void)
{
	return 0x1c4;
}
static inline u32 host1x_sync_mlock_0_r(void)
{
	return 0x2c0;
}
static inline u32 host1x_sync_mlock_owner_0_r(void)
{
	return 0x340;
}
static inline u32 host1x_sync_mlock_owner_0_mlock_owner_chid_0_v(u32 r)
{
	return (r >> 8) & 0xf;
}
static inline u32 host1x_sync_mlock_owner_0_mlock_cpu_owns_0_v(u32 r)
{
	return (r >> 1) & 0x1;
}
static inline u32 host1x_sync_mlock_owner_0_mlock_ch_owns_0_v(u32 r)
{
	return (r >> 0) & 0x1;
}
static inline u32 host1x_sync_syncpt_base_0_r(void)
{
	return 0x600;
}
static inline u32 host1x_sync_cfpeek_ctrl_r(void)
{
	return 0x74c;
}
static inline u32 host1x_sync_cfpeek_ctrl_cfpeek_addr_f(u32 v)
{
	return (v & 0x3ff) << 0;
}
static inline u32 host1x_sync_cfpeek_ctrl_cfpeek_addr_v(u32 r)
{
	return (r >> 0) & 0x3ff;
}
static inline u32 host1x_sync_cfpeek_ctrl_cfpeek_channr_f(u32 v)
{
	return (v & 0xf) << 16;
}
static inline u32 host1x_sync_cfpeek_ctrl_cfpeek_channr_v(u32 r)
{
	return (r >> 16) & 0xf;
}
static inline u32 host1x_sync_cfpeek_ctrl_cfpeek_ena_f(u32 v)
{
	return (v & 0x1) << 31;
}
static inline u32 host1x_sync_cfpeek_read_r(void)
{
	return 0x750;
}
static inline u32 host1x_sync_cfpeek_ptrs_r(void)
{
	return 0x754;
}
static inline u32 host1x_sync_cfpeek_ptrs_cf_rd_ptr_v(u32 r)
{
	return (r >> 0) & 0x3ff;
}
static inline u32 host1x_sync_cfpeek_ptrs_cf_wr_ptr_v(u32 r)
{
	return (r >> 16) & 0x3ff;
}
static inline u32 host1x_sync_cf0_setup_r(void)
{
	return 0xc00;
}
static inline u32 host1x_sync_cf0_setup_cf0_base_v(u32 r)
{
	return (r >> 0) & 0x3ff;
}
static inline u32 host1x_sync_cf0_setup_cf0_limit_v(u32 r)
{
	return (r >> 16) & 0x3ff;
}
static inline u32 host1x_sync_cbread0_r(void)
{
	return 0xc80;
}
static inline u32 host1x_sync_cbstat_0_r(void)
{
	return 0xcc0;
}
static inline u32 host1x_sync_cbstat_0_cbclass0_v(u32 r)
{
	return (r >> 16) & 0x3ff;
}
static inline u32 host1x_sync_cbstat_0_cboffset0_v(u32 r)
{
	return (r >> 0) & 0xffff;
}
static inline u32 host1x_sync_syncpt_thresh_cpu0_int_status_r(void)
{
	return 0xe80;
}
static inline u32 host1x_sync_syncpt_thresh_cpu1_int_status_r(void)
{
	return 0xea0;
}
static inline u32 host1x_sync_syncpt_thresh_int_disable_r(void)
{
	return 0xf00;
}
static inline u32 host1x_sync_syncpt_thresh_int_enable_cpu0_r(void)
{
	return 0xf20;
}
static inline u32 host1x_sync_syncpt_cpu_incr_r(void)
{
	return 0xf60;
}
static inline u32 host1x_sync_syncpt_0_r(void)
{
	return 0xf80;
}
static inline u32 host1x_sync_syncpt_int_thresh_0_r(void)
{
	return 0x1380;
}
#endif
