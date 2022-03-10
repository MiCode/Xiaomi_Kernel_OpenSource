/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __MODEM_DPMAIF_DRV1_H__
#define __MODEM_DPMAIF_DRV1_H__
#include "ccci_config.h"
#include "dpmaif_reg_v1.h"

#ifdef MT6297
#undef MT6297
#endif

#ifdef _HW_REORDER_SW_WORKAROUND_
#undef _HW_REORDER_SW_WORKAROUND_
#endif

#ifdef USING_TX_DONE_KERNEL_THREAD
#undef USING_TX_DONE_KERNEL_THREAD
#endif

/* == RX part == */
#ifdef HW_FRG_FEATURE_ENABLE
unsigned short drv1_dpmaif_dl_get_frg_bat_ridx(unsigned char q_num);
int drv1_dpmaif_dl_add_frg_bat_cnt(unsigned char q_num,
	unsigned short frg_entry_cnt);
#endif
unsigned short drv1_dpmaif_dl_get_bat_ridx(unsigned char q_num);
int drv1_dpmaif_dl_add_bat_cnt(unsigned char q_num,
		unsigned short bat_entry_cnt);
int drv1_dpmaif_dl_add_pit_remain_cnt(unsigned char q_num,
		unsigned short pit_remain_cnt);
unsigned int  drv1_dpmaif_dl_get_wridx(unsigned char q_num);
void drv1_dpmaif_mask_dl_interrupt(unsigned char q_num);
void drv1_dpmaif_unmask_dl_interrupt(unsigned char q_num);
int drv1_dpmaif_dl_all_queue_en(bool enable);
unsigned int drv1_dpmaif_dl_idle_check(void);

/* == TX part == */
void drv1_dpmaif_mask_ul_que_interrupt(unsigned char q_num);
void drv1_dpmaif_unmask_ul_interrupt(unsigned char q_num);
unsigned int drv1_dpmaif_ul_get_ridx(unsigned char q_num);
unsigned int drv1_dpmaif_ul_get_rwidx(unsigned char q_num);
unsigned int drv1_dpmaif_ul_get_hw_widx_01(void);
int drv1_dpmaif_ul_add_wcnt(unsigned char q_num, unsigned short drb_wcnt);
/* isr part */
#define drv1_dpmaif_get_dl_isr_event() \
	DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0)
#define drv1_dpmaif_get_ul_isr_event() \
	DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0)
#define  drv1_dpmaif_get_dl_interrupt_mask() \
	DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0)

/* use ao domain:
 * (DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES)&DPMAIF_AO_DL_ISR_MSK)
 */
#ifdef MT6297
#define  drv1_dpmaif_ul_get_ul_interrupt_mask() \
	DPMA_READ_AO_UL(DPMAIF_PD_AP_UL_L2TIMR0)
#else
#define  drv1_dpmaif_ul_get_ul_interrupt_mask() \
	DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0)
#endif
void drv1_dpmaif_clear_ip_busy(void);

/* == state part == */
/*void drv1_dpmaif_set_dl_interrupt_mask(unsigned int mask);*/
int drv1_dpmaif_intr_hw_init(void);
/* init: rx init */
int drv1_dpmaif_dl_bat_init_done(unsigned char q_num, bool frg_en);
void drv1_dpmaif_dl_pit_init_done(unsigned char q_num);
void drv1_dpmaif_dl_set_bat_base_addr(unsigned char q_num,
	dma_addr_t base_addr);
void drv1_dpmaif_dl_set_bat_size(unsigned char q_num, unsigned int size);
void drv1_dpmaif_dl_bat_en(unsigned char q_num, bool enable);
void drv1_dpmaif_dl_set_pit_base_addr(unsigned char q_num,
	dma_addr_t base_addr);
void drv1_dpmaif_dl_set_pit_size(unsigned char q_num, unsigned int size);
void drv1_dpmaif_dl_pit_en(unsigned char q_num, bool enable);
void drv1_dpmaif_dl_set_bid_maxcnt(unsigned char q_num, unsigned int cnt);
void drv1_dpmaif_dl_set_remain_minsz(unsigned char q_num, unsigned int sz);
#ifdef _HW_REORDER_SW_WORKAROUND_
void drv1_dpmaif_dl_set_apit_idx(unsigned char q_num, unsigned int idx);
int drv1_dpmaif_dl_add_apit_num(unsigned short ap_entry_cnt);
#endif
void drv1_dpmaif_dl_set_mtu(unsigned int mtu_sz);
void drv1_dpmaif_dl_set_pit_chknum(unsigned char q_num, unsigned int number);
void drv1_dpmaif_dl_set_bat_bufsz(unsigned char q_num, unsigned int buf_sz);
void drv1_dpmaif_dl_set_bat_rsv_len(unsigned char q_num, unsigned int length);
void drv1_dpmaif_dl_set_pkt_align(unsigned char q_num, bool enable,
	unsigned int mode);
void drv1_dpmaif_dl_set_bat_chk_thres(unsigned char q_num, unsigned int size);
#ifdef HW_FRG_FEATURE_ENABLE
void drv1_dpmaif_dl_set_ao_frag_check_thres(unsigned char q_num,
	unsigned int size);
void drv1_dpmaif_dl_set_ao_frg_bat_feature(unsigned char q_num, bool enable);
void drv1_dpmaif_dl_set_ao_frg_bat_bufsz(unsigned char q_num,
	unsigned int buf_sz);
int drv1_dpmaif_dl_all_frg_queue_en(bool enable);
#endif
#ifdef HW_CHECK_SUM_ENABLE
void drv1_dpmaif_dl_set_ao_chksum_en(unsigned char q_num, bool enable);
#endif
/* init: tx init */

void drv1_dpmaif_init_ul_intr(void);
void drv1_dpmaif_ul_update_drb_size(unsigned char q_num, unsigned int size);
void drv1_dpmaif_ul_update_drb_base_addr(unsigned char q_num,
	unsigned int lb_addr, unsigned int hb_addr);
void drv1_dpmaif_ul_rdy_en(unsigned char q_num, bool ready);
void drv1_dpmaif_ul_arb_en(unsigned char q_num, bool enable);
void drv1_dpmaif_ul_all_queue_en(bool enable);
unsigned int drv1_dpmaif_ul_idle_check(void);

/* suspend resume */
bool drv1_dpmaif_check_power_down(void);
int drv1_dpmaif_dl_restore(unsigned int mask);

#ifdef _E1_SB_SW_WORKAROUND_
unsigned int drv1_dpmaif_dl_get_pit_ridx(unsigned char q_num);
unsigned int drv1_dpmaif_dl_get_bat_wridx(unsigned char q_num);
void drv1_dpmaif_unmask_dl_full_intr(unsigned char q_num);
void dpmaif_mask_pitcnt_len_error_intr(unsigned char q_num);
void dpmaif_mask_batcnt_len_error_intr(unsigned char q_num);
#endif

#ifdef MT6297
void drv1_dpmaif_dl_set_performance(void);
void drv1_dpmaif_dl_set_wdma(void);
void drv1_dpmaif_dl_set_chk_rbnum(unsigned char q_num, unsigned int cnt);
void drv1_dpmaif_common_hw_init(void);
void drv1_dpmaif_md_hw_bus_remap(void);
#endif

#endif
