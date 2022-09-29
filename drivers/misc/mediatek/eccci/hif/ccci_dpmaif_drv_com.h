/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_DPMAIF_DRV_COM_H__
#define __CCCI_DPMAIF_DRV_COM_H__


#include "ccci_dpmaif_com.h"


struct dpmaif_tx_queue;
struct dpmaif_rx_queue;
struct dpmaif_normal_pit_v2;


struct dpmaif_plat_drv {
	unsigned int pit_size_msk;
	unsigned int dl_pit_wridx_msk;
	unsigned int ul_int_md_not_ready_msk;
	unsigned int ap_ul_l2intr_err_en_msk;
	unsigned int normal_pit_size;
	unsigned int ul_int_qdone_msk;
	unsigned int dl_idle_sts;
};

struct dpmaif_plat_ops {
	unsigned int (*drv_get_dl_interrupt_mask)(void);
	void         (*drv_unmask_ul_interrupt)(unsigned char q_num);
	unsigned int (*drv_dl_get_wridx)(unsigned char q_num);
	unsigned int (*drv_ul_get_rwidx)(unsigned char q_num);
	unsigned int (*drv_ul_get_rdidx)(unsigned char q_num);
	void         (*drv_ul_all_queue_en)(bool enable);

	int          (*drv_start)(void);
	int          (*drv_suspend_noirq)(struct device *dev);
	int          (*drv_resume_noirq)(struct device *dev);

	unsigned int (*drv_ul_idle_check)(void);
	void         (*drv_hw_reset)(void);
	bool         (*drv_check_power_down)(void);
	void         (*drv_txq_hw_init)(struct dpmaif_tx_queue *txq);
	void         (*drv_dump_register)(int buf_type);
};


extern struct dpmaif_plat_drv   g_plat_drv;
#define drv g_plat_drv

extern struct dpmaif_plat_ops          g_plat_ops;
#define ops g_plat_ops


void ccci_drv_set_dl_interrupt_mask(unsigned int mask);

int ccci_dpmaif_drv1_init(void);
int ccci_dpmaif_drv2_init(void);
int ccci_dpmaif_drv3_init(void);

void ccci_drv2_dl_set_remain_minsz(unsigned int sz);
void ccci_drv3_dl_set_remain_minsz(unsigned int sz);

void ccci_drv2_dl_set_bid_maxcnt(unsigned int cnt);
void ccci_drv3_dl_set_bid_maxcnt(unsigned int cnt);

void ccci_drv2_dl_set_pkt_align(bool enable, unsigned int mode);
void ccci_drv3_dl_set_pkt_align(bool enable, unsigned int mode);

void ccci_drv2_dl_set_mtu(unsigned int mtu_sz);
void ccci_drv3_dl_set_mtu(unsigned int mtu_sz);

void ccci_drv1_dl_set_pit_chknum(void);
void ccci_drv2_dl_set_pit_chknum(void);
void ccci_drv3_dl_set_pit_chknum(void);

void ccci_drv2_dl_set_chk_rbnum(unsigned int cnt);
void ccci_drv3_dl_set_chk_rbnum(unsigned int cnt);


void ccci_drv2_dl_set_performance(void);
void ccci_drv3_dl_set_performance(void);

void ccci_drv_dl_set_pit_base_addr(dma_addr_t base_addr);
void ccci_drv_dl_set_pit_size(unsigned int size);
void ccci_drv_dl_pit_en(bool enable);
void ccci_drv_dl_pit_init_done(void);

void ccci_drv2_dl_set_ao_chksum_en(bool enable);
void ccci_drv3_dl_set_ao_chksum_en(bool enable);

void ccci_drv2_dl_set_bat_bufsz(unsigned int buf_sz);
void ccci_drv3_dl_set_bat_bufsz(unsigned int buf_sz);

void ccci_drv2_dl_set_bat_rsv_len(unsigned int length);
void ccci_drv3_dl_set_bat_rsv_len(unsigned int length);

void ccci_drv1_dl_set_bat_chk_thres(void);
void ccci_drv2_dl_set_bat_chk_thres(void);
void ccci_drv3_dl_set_bat_chk_thres(void);

void ccci_drv_dl_set_bat_base_addr(dma_addr_t base_addr);
void ccci_drv_dl_set_bat_size(unsigned int size);
void ccci_drv_dl_bat_en(bool enable);
int ccci_drv_dl_bat_init_done(bool frg_en);

void ccci_drv2_dl_set_ao_frg_bat_feature(bool enable);
void ccci_drv3_dl_set_ao_frg_bat_feature(bool enable);

void ccci_drv2_dl_set_ao_frg_bat_bufsz(unsigned int buf_sz);
void ccci_drv3_dl_set_ao_frg_bat_bufsz(unsigned int buf_sz);

void ccci_drv1_dl_set_ao_frag_check_thres(void);
void ccci_drv2_dl_set_ao_frag_check_thres(void);
void ccci_drv3_dl_set_ao_frag_check_thres(void);

int ccci_drv_dl_add_frg_bat_cnt(unsigned short frg_entry_cnt);
int ccci_drv_dl_add_bat_cnt(unsigned short bat_entry_cnt);
int ccci_drv_dl_all_frg_queue_en(bool enable);
int ccci_drv_dl_all_queue_en(bool enable);


unsigned short ccci_drv2_dl_get_bat_ridx(void);
unsigned short ccci_drv3_dl_get_bat_ridx(void);
unsigned short ccci_drv3_dl_get_bat_widx(void);

unsigned short ccci_drv2_dl_get_frg_bat_ridx(void);
unsigned short ccci_drv3_dl_get_frg_bat_ridx(void);

void ccci_drv3_hw_init_done(void);
void ccci_drv_clear_ip_busy(void);


void ccci_irq_rx_lenerr_handler(unsigned int rx_int_isr);

int ccci_drv_dl_add_pit_remain_cnt(unsigned short pit_remain_cnt);

int ccci_drv_ul_add_wcnt(unsigned char q_num, unsigned short drb_wcnt);

unsigned int ccci_drv_dl_idle_check(void);

void ccci_drv3_dl_config_lro_hw(dma_addr_t addr, unsigned int size,
	bool enable, unsigned int pit_idx);
void ccci_drv3_dl_lro_hpc_hw_init(void);

#define ccci_drv_get_dl_isr_event() \
	DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0)
#define ccci_drv_get_ul_isr_event() \
	DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0)

int ccci_drv2_rxq_update_apit_dummy(struct dpmaif_rx_queue *rxq);
void ccci_drv2_rxq_hw_int_apit(struct dpmaif_rx_queue *rxq);
void ccci_drv2_rxq_handle_ig(struct dpmaif_rx_queue *rxq,
	struct dpmaif_normal_pit_v2 *nml_pit_v2);

#endif  /* __CCCI_DPMAIF_DRV_COM_H__ */
