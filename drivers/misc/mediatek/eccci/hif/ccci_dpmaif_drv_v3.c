// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>


#include "ccci_dpmaif_com.h"
#include "ccci_dpmaif_reg_v3.h"


#define TAG "drv3"

static unsigned int g_backup_dl_isr, g_backup_ul_isr;

static struct dpmaif_clk_node g_clk_tbs[] = {
	{ NULL, "infra-dpmaif-clk"},
	{ NULL, "infra-dpmaif-blk-clk"},
	{ NULL, "infra-dpmaif-rg-mmw-clk"},
	{ NULL, NULL},
};

static void drv_sram_init(void)
{
	unsigned int value;

	value = DPMA_READ_PD_MISC(NRL2_DPMAIF_AP_MISC_MEM_CLR);
	value |= DPMAIF_MEM_CLR_MASK;
	DPMA_WRITE_PD_MISC(NRL2_DPMAIF_AP_MISC_MEM_CLR, value);

	while ((DPMA_READ_PD_MISC(NRL2_DPMAIF_AP_MISC_MEM_CLR) & DPMAIF_MEM_CLR_MASK))
		;
}


static void drv_md_hw_bus_remap(void)
{
	unsigned int value;
	phys_addr_t md_base_at_ap;
	unsigned long long tmp_val;

	get_md_resv_mem_info(&md_base_at_ap, NULL, NULL, NULL);
	tmp_val = (unsigned long long)md_base_at_ap;

	/*Remap and Remap enable for address*/
	/*1~9, 11~19, 21~29 bit is map address*/
	/*0, 10, 20 bit is enable/disable map address bit*/

	/*Bank0 0-2*/
	value = ((tmp_val >> 24) & (0x3FE << 0)) +
		(((tmp_val + 0x2000000) >> 14) & (0x3FE << 10)) +
		(((tmp_val + (0x2000000*2)) >> 4) & (0x3FE << 20));
	value |= ((1 << 20) | (1 << 10) | (1 << 0));
	DPMA_WRITE_AO_MISC_SRAM(NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAP0_2, value);

	/*Bank0 3-5*/
	tmp_val += 0x2000000 * 3;
	value = ((tmp_val >> 24) & (0x3FE << 0)) +
		(((tmp_val + 0x2000000) >> 14) & (0x3FE << 10)) +
		(((tmp_val + (0x2000000*2)) >> 4) & (0x3FE << 20));
	value |= ((1 << 20) | (1 << 10) | (1 << 0));
	DPMA_WRITE_AO_MISC_SRAM(NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAP3_5, value);

	/*Bank0 6-7 + Bank1 0*/
	tmp_val += 0x2000000 * 3;
	value = ((tmp_val >> 24) & (0x3FE << 0)) +
		(((tmp_val + 0x2000000) >> 14) & (0x3FE << 10)) +
		(((tmp_val + (0x2000000*2)) >> 4) & (0x3FE << 20));
	value |= ((1 << 20) | (1 << 10) | (1 << 0));
	DPMA_WRITE_AO_MISC_SRAM(
		NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAP6_7_BANK1_MAP0, value);

	/*Bank1 1-3*/
	tmp_val += 0x2000000 * 3;
	value = ((tmp_val >> 24) & (0x3FE << 0)) +
		(((tmp_val + 0x2000000) >> 14) & (0x3FE << 10)) +
		(((tmp_val + (0x2000000*2)) >> 4) & (0x3FE << 20));
	value |= ((1 << 20) | (1 << 10) | (1 << 0));

	DPMA_WRITE_AO_MISC_SRAM(NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAP1_3, value);

	/*Bank1 4-6*/
	tmp_val += 0x2000000 * 3;
	value = ((tmp_val >> 24) & (0x3FE << 0)) +
		(((tmp_val + 0x2000000) >> 14) & (0x3FE << 10)) +
		(((tmp_val + (0x2000000*2)) >> 4) & (0x3FE << 20));
	value |= ((1 << 20) | (1 << 10) | (1 << 0));
	DPMA_WRITE_AO_MISC_SRAM(NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAP4_6, value);

	/*Bank1 7 + Bank4 0-1*/
	tmp_val += 0x2000000 * 3;
	value = ((tmp_val >> 24) & (0x3FE << 0)) +
		(((tmp_val + 0x2000000) >> 14) & (0x3FE << 10)) +
		(((tmp_val + (0x2000000*2)) >> 4) & (0x3FE << 20));
	value |= ((0 << 20) | (0 << 10) | (1 << 0));
	DPMA_WRITE_AO_MISC_SRAM(
		NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAP7_BANK4_MAP0_1, value);

	/*Bank4 2-4*/
	tmp_val += 0x2000000 * 3;
	value = ((tmp_val >> 24) & (0x3FE << 0)) +
		(((tmp_val + 0x2000000) >> 14) & (0x3FE << 10)) +
		(((tmp_val + (0x2000000*2)) >> 4) & (0x3FE << 20));
	value |= ((0 << 20) | (0 << 10) | (0 << 0));
	DPMA_WRITE_AO_MISC_SRAM(NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAP2_4, value);

	/*Bank4 5-7*/
	tmp_val += 0x2000000 * 3;
	value = ((tmp_val >> 24) & (0x3FE << 0)) +
		(((tmp_val + 0x2000000) >> 14) & (0x3FE << 10)) +
		(((tmp_val + (0x2000000*2)) >> 4) & (0x3FE << 20));
	value |= ((0 << 20) | (0 << 10) | (0 << 0));
	DPMA_WRITE_AO_MISC_SRAM(NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAP5_7, value);
}

static void drv3_common_hw_init(void)
{
	drv_sram_init();

	/*Set HW CG dis*/
	DPMA_WRITE_PD_MISC(NRL2_DPMAIF_AP_MISC_CG_EN, 0x7F);

	drv_md_hw_bus_remap();

	if (g_plat_inf == 6985)
		/* this bit24 is used to mask irq2's isr to irq1 */
		DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L1TIMR0,
				((1<<9)|(1<<10)|(1<<15)|(1<<16)|(1<<24)));
	else
		DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L1TIMR0,
				((1<<9)|(1<<10)|(1<<15)|(1<<16)));

	/*Set Power on/off flag*/
	DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW, 0xFF);
}

void ccci_drv3_dl_set_bid_maxcnt(unsigned int cnt)
{
	unsigned int value;

	/* 1.4 bit31~16: max pkt count in one BAT buffer, curr: 3 */
	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CONO);

	value &= ~(DPMAIF_BAT_BID_MAXCNT_MSK);
	value |= ((cnt << 16) & DPMAIF_BAT_BID_MAXCNT_MSK);

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CONO, value);
}

void ccci_drv3_dl_set_remain_minsz(unsigned int sz)
{
	unsigned int value;

	/* 1.1 bit 15~8: BAT remain size < sz, use next BAT, curr: 64 */
	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CONO);

	value &= ~(DPMAIF_BAT_REMAIN_MINSZ_MSK);
	value |= (((sz/DPMAIF_BAT_REMAIN_SZ_BASE) << 8) & DPMAIF_BAT_REMAIN_MINSZ_MSK);

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CONO, value);
}

void ccci_drv3_dl_set_pkt_align(bool enable, unsigned int mode)
{
	unsigned int value;

	if (mode >= 2)
		return;

	/* 1.5 bit 22: pkt align, curr: 0, 64B align */
	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_THRES);

	value &= ~(DPMAIF_PKT_ALIGN_MSK);

	if (enable == true) {
		value |= DPMAIF_PKT_ALIGN_EN;
		value |= ((mode << 22) & DPMAIF_PKT_ALIGN_MSK);
	}

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

void ccci_drv3_dl_set_mtu(unsigned int mtu_sz)
{
	/* 1. 6 bit 31~0: MTU setting, curr: (3*1024 + 8) = 3080 */
	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CON1, mtu_sz);
}

void ccci_drv3_dl_set_pit_chknum(void)
{
	unsigned int value;
	unsigned int number = DPMAIF_HW_CHK_PIT_NUM;

	/* 2.1 bit 31~24: pit threadhold, < number will pit_err_irq, curr: 2 */
	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_PIT_CHK_NUM_MSK);
	value |= ((number << 24) & DPMAIF_PIT_CHK_NUM_MSK);

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void ccci_drv3_dl_set_chk_rbnum(unsigned int cnt)
{
	unsigned int value;

	/* bit0~7: chk rb pit number */
	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CONO);

	value &= ~(DPMAIF_CHK_RB_PITNUM_MSK);
	value |= ((cnt) & DPMAIF_CHK_RB_PITNUM_MSK);

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CONO, value);
}

void ccci_drv3_dl_set_performance(void)
{
	unsigned int value;

	/*BAT cache enable*/
	value = DPMA_READ_PD_DL(NRL2_DPMAIF_DL_BAT_INIT_CON1);
	value |= (1<<22);
	DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_BAT_INIT_CON1, value);

	/*PIT burst en*/
	value = DPMA_READ_AO_DL_SRAM(NRL2_DPMAIF_AO_DL_RDY_CHK_THRES);
	value |= (1<<13);
	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

void ccci_drv3_dl_set_ao_chksum_en(bool enable)
{
	unsigned int value;

	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_THRES);
	value &= ~(DPMAIF_CHKSUM_ON_MSK);

	if (enable == true)
		value |= (DPMAIF_CHKSUM_ON_MSK);

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

void ccci_drv3_dl_set_bat_bufsz(unsigned int buf_sz)
{
	unsigned int value;

	/* 1.2 bit 16~8: BAT->buffer size: 128*28 = 3584 unit:? curr: 28 */
	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_BAT_BUF_SZ_MSK);
	value |= (((buf_sz/DPMAIF_BAT_BUFFER_SZ_BASE) << 8) & DPMAIF_BAT_BUF_SZ_MSK);

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void ccci_drv3_dl_set_bat_rsv_len(unsigned int length)
{
	unsigned int value;

	/* 1.3 bit7~0: BAT buffer reserve length, curr: 88 */
	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_BAT_RSV_LEN_MSK);
	value |= (length & DPMAIF_BAT_RSV_LEN_MSK);

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void ccci_drv3_dl_set_bat_chk_thres(void)
{
	unsigned int value;

	/* 2.2 bit 21~16: bat threadhold, < size will len_err_irq2, curr: 1 */
	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_THRES);

	value &= ~(DPMAIF_BAT_CHECK_THRES_MSK);
	value |= ((DPMAIF_HW_CHK_BAT_NUM << 16) & DPMAIF_BAT_CHECK_THRES_MSK);

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

void ccci_drv3_dl_set_ao_frg_bat_feature(bool enable)
{
	unsigned int value;

	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	value &= ~(DPMAIF_FRG_BAT_BUF_FEATURE_ON_MSK);

	if (enable == true)
		value |= (DPMAIF_FRG_BAT_BUF_FEATURE_EN &
			DPMAIF_FRG_BAT_BUF_FEATURE_ON_MSK);

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);
}

void ccci_drv3_dl_set_ao_frg_bat_bufsz(unsigned int buf_sz)
{
	unsigned int value;

	/* 1.2 bit 16~8: BAT->buffer size: 128*28 = 3584 unit:? curr: 28 */
	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);

	value &= ~(DPMAIF_FRG_BAT_BUF_SZ_MSK);
	value |= (((buf_sz/DPMAIF_FRG_BAT_BUFFER_SZ_BASE) << 8) & DPMAIF_FRG_BAT_BUF_SZ_MSK);

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);
}

void ccci_drv3_dl_set_ao_frag_check_thres(void)
{
	unsigned int value;

	/* 2.2 bit 21~16: bat threadhold, < size will len_err_irq2, curr: 1 */
	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	value &= ~(DPMAIF_FRG_CHECK_THRES_MSK);

	value |= ((DPMAIF_HW_CHK_FRG_NUM) & DPMAIF_FRG_CHECK_THRES_MSK);

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);
}

unsigned short ccci_drv3_dl_get_bat_ridx(void)
{
	unsigned int ridx = 0;

	ridx = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_BAT_STA2);
	ridx = (ridx & DPMAIF_DL_BAT_WRIDX_MSK);

	return (unsigned short)ridx;
}

unsigned short ccci_drv3_dl_get_bat_widx(void)
{
	unsigned int widx = 0;

	widx = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_BAT_STA3);
	widx = (widx & DPMAIF_DL_BAT_WRIDX_MSK);

	return (unsigned short)widx;
}

unsigned short ccci_drv3_dl_get_frg_bat_ridx(void)
{
	unsigned int ridx = 0;

	ridx = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_FRGBAT_STA2);
	ridx = (ridx & DPMAIF_DL_BAT_WRIDX_MSK);

	return (unsigned short)ridx;
}

static void drv3_ul_update_drb_size(unsigned char q_num, unsigned int size)
{
	unsigned int old_size, set_size;

	/* 1. bit 15~0: DRB count, in word(4 bytes) curr: 512*8/4 */
	set_size = size/4;
	old_size = DPMA_READ_AO_UL_SRAM(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_size &= ~DPMAIF_DRB_SIZE_MSK;
	old_size |= (set_size & DPMAIF_DRB_SIZE_MSK);

	DPMA_WRITE_AO_UL_SRAM(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_size);

}

static void drv3_ul_update_drb_base_addr(unsigned char q_num,
	unsigned int lb_addr, unsigned int hb_addr)
{
	unsigned int old_addr;

	/* 2 bit 31~0: drb base addr low 32bits, curr: lb_addr */
	DPMA_WRITE_AO_UL_SRAM(DPMAIF_ULQSAR_n(q_num), lb_addr);

	old_addr = DPMA_READ_AO_UL_SRAM(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_addr &= ~DPMAIF_DRB_ADDRH_MSK;
	old_addr |= ((hb_addr<<24) & DPMAIF_DRB_ADDRH_MSK);

	/* 2. bit 31~24: drb base addr high 8bits, curr: hb_addr */
	DPMA_WRITE_AO_UL_SRAM(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_addr);
}

void ccci_drv3_hw_init_done(void)
{
	unsigned int reg_value = 0;

	/*sync default value to SRAM*/
	reg_value = DPMA_READ_PD_MISC(NRL2_DPMAIF_AP_MISC_OVERWRITE_CFG);
	reg_value |= (DPMAIF_SRAM_SYNC_MASK);

	DPMA_WRITE_PD_MISC(NRL2_DPMAIF_AP_MISC_OVERWRITE_CFG, reg_value);

	/*polling status*/
	while ((DPMA_READ_PD_MISC(NRL2_DPMAIF_AP_MISC_OVERWRITE_CFG) & DPMAIF_SRAM_SYNC_MASK))
		;

	/*UL cfg done*/
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_INIT_SET, DPMAIF_UL_INIT_DONE_MASK);

	/*DL cfg done*/
	DPMA_WRITE_AO_DL(NRL2_DPMAIF_AO_DL_INIT_SET, DPMAIF_DL_INIT_DONE_MASK);
}

static unsigned int drv3_ul_get_rwidx(unsigned char q_num)
{
	return DPMA_READ_AO_UL_SRAM(DPMAIF_ULQ_STA0_n(q_num));
}

static unsigned int drv3_ul_get_rdidx(unsigned char q_num)
{
	return (DPMA_READ_AO_UL_SRAM(DPMAIF_ULQ_STA0_n(q_num)) >> 16) & 0x0000FFFF;
}

static unsigned int drv3_ul_get_rwidx_6985(unsigned char q_num)
{
	return DPMA_READ_AO_UL_SRAM(DPMAIF_ULQ_STA0_6985_n(q_num));
}

static unsigned int drv3_ul_get_rdidx_6985(unsigned char q_num)
{
	return (DPMA_READ_AO_UL_SRAM(DPMAIF_ULQ_STA0_6985_n(q_num)) >> 16) & 0x0000FFFF;
}

static void drv3_mask_ul_que_interrupt(unsigned char q_num)
{
	unsigned int ui_que_done_mask;

	ui_que_done_mask = DPMAIF_UL_INT_DONE(q_num) & DPMAIF_UL_INT_QDONE_MSK;

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMSR0, ui_que_done_mask);

	/* check mask sts */
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
	 * ui_que_done_mask)
	 * != ui_que_done_mask);
	 */
}

static inline void drv3_irq_tx_done(unsigned int tx_done_isr)
{
	int i;
	unsigned int intr_ul_que_done;
	struct dpmaif_tx_queue *txq;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		intr_ul_que_done = tx_done_isr & (1 << (i + UL_INT_DONE_OFFSET));
		if (intr_ul_que_done) {
			txq = &dpmaif_ctl->txq[i];

			drv3_mask_ul_que_interrupt(i);

			hrtimer_start(&txq->txq_done_timer,
				ktime_set(0, 500000), HRTIMER_MODE_REL);

		}
	}
}

static void drv3_mask_dl_interrupt(void)
{
	/* set mask register: bit1s */
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0, DPMAIF_DL_INT_QDONE_MSK);
}

static void drv3_unmask_dl_interrupt(void)
{
	/* set unmask/clear_mask register: bit0s */
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0, DPMAIF_DL_INT_QDONE_MSK);
}

void drv3_unmask_dl_lro0_interrupt(void)
{
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0, DPMAIF_DL_INT_LRO0_QDONE_MSK);
}

void drv3_unmask_dl_lro1_interrupt(void)
{
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0, DPMAIF_DL_INT_LRO1_QDONE_MSK);
}

static void drv3_unmask_ul_interrupt(unsigned char q_num)
{
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMCR0,
		(DPMAIF_UL_INT_DONE(q_num) & DPMAIF_UL_INT_QDONE_MSK));

	/*check mask sts*/
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
	 *	ui_que_done_mask) == ui_que_done_mask);
	 */
}

static unsigned int drv3_dl_get_wridx(unsigned char q_num)
{
	return (DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_PIT_STA3) & DPMAIF_DL_PIT_WRIDX_MSK);
}

static inline unsigned int drv3_dl_get_lro0_wridx(void)
{
	unsigned int widx;

	widx = DPMA_READ_AO_DL_SRAM(NRL2_DPMAIF_AO_DL_LRO_STA5);
	widx &= DPMAIF_DL_PIT_WRIDX_MSK;

	return widx;
}

static inline drv3_dl_get_lro1_wridx(void)
{
	unsigned int widx;

	widx = DPMA_READ_AO_DL_SRAM(NRL2_DPMAIF_AO_DL_LRO_STA13);
	widx &= DPMAIF_DL_PIT_WRIDX_MSK;

	return widx;
}

static unsigned int drv3_dl_get_lro_wridx(unsigned char q_num)
{
	if (q_num == 0)
		return drv3_dl_get_lro0_wridx();
	else
		return drv3_dl_get_lro1_wridx();
}

static inline unsigned int drv3_get_dl_interrupt_mask(void)
{
	return DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMR0);
}

static void drv3_mask_dl_lro0_interrupt(void)
{
	int count = 0;

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0, DPMAIF_DL_INT_LRO0_QDONE_MSK);

	/*check mask sts*/
	while ((DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMR0) &
		DPMAIF_DL_INT_LRO0_QDONE_MSK) != DPMAIF_DL_INT_LRO0_QDONE_MSK) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: mask dl lro0 irq fail\n", __func__);
			break;
		}
	}
}

static void drv3_mask_dl_lro1_interrupt(void)
{
	int count = 0;

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0, DPMAIF_DL_INT_LRO1_QDONE_MSK);

	/*check mask sts*/
	while ((DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMR0) &
		DPMAIF_DL_INT_LRO1_QDONE_MSK) != DPMAIF_DL_INT_LRO1_QDONE_MSK) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: mask dl lro1 irq fail\n", __func__);
			break;
		}
	}
}

static irqreturn_t drv3_isr0(int irq, void *data)
{
	struct dpmaif_rx_queue *rxq = (struct dpmaif_rx_queue *)data;
	unsigned int L2RISAR0 = ccci_drv_get_dl_isr_event();
	unsigned int L2RIMR0  = drv3_get_dl_interrupt_mask();
	unsigned int L2TISAR0 = ccci_drv_get_ul_isr_event();
	unsigned int L2TIMR0  = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMR0);
#ifdef ENABLE_DPMAIF_ISR_LOG
	unsigned int L2RISAR0_bak = L2RISAR0, L2TISAR0_bak = L2TISAR0;
#endif

	/* clear IP busy register wake up cpu case */
	ccci_drv_clear_ip_busy();

	if (atomic_read(&dpmaif_ctl->wakeup_src) == 1)
		CCCI_NOTICE_LOG(0, TAG, "[%s] wake up by MD0 HIF L2(%x/%x)(%x/%x)!\n",
			__func__, L2TISAR0, L2TIMR0, L2RISAR0, L2RIMR0);

	/* TX interrupt */
	if (L2TISAR0) {
		L2TISAR0 &= ~(L2TIMR0);
		DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, L2TISAR0);

		/* this log may be printed frequently, so first cancel it*/
		if (L2TISAR0 & DPMAIF_UL_INT_ERR_MSK)
			CCCI_ERROR_LOG(0, TAG, "[%s] dpmaif: ul error L2(%x)\n",
				__func__, L2TISAR0);

		/* tx done */
		if (L2TISAR0 & DPMAIF_UL_INT_QDONE_MSK)
			drv3_irq_tx_done(L2TISAR0 & DPMAIF_UL_INT_QDONE_MSK);
	}

	/* RX interrupt */
	if (L2RISAR0) {
		L2RISAR0 &= ~(L2RIMR0|DP_DL_INT_LRO1_QDONE_SET);
		if (L2RISAR0 & AP_DL_L2INTR_ERR_En_Msk)
			ccci_irq_rx_lenerr_handler(L2RISAR0);

		/* ACK interrupt after lenerr_handler*/
		/* ACK RX interrupt */
		DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0, L2RISAR0);

		if (L2RISAR0 & DP_DL_INT_LRO0_QDONE_SET) {
			/* disable RX_DONE  interrupt */
			drv3_mask_dl_lro0_interrupt();

			/*always start work due to no napi*/
			tasklet_hi_schedule(&rxq->rxq_task);
		}
	}

	if (g_debug_flags & DEBUG_RXTX_ISR) {
		struct debug_rxtx_isr_hdr hdr = {0};

		hdr.type = TYPE_RXTX_ISR_ID;
		hdr.qidx = 0;
		hdr.time = (unsigned int)(local_clock() >> 16);
		hdr.rxsr = L2RISAR0;
		hdr.rxmr = L2RIMR0;
		hdr.txsr = L2TISAR0;
		hdr.txmr = L2TIMR0;
		ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
	}
#ifdef ENABLE_DPMAIF_ISR_LOG
	if (ccci_dpmaif_record_isr_cnt(local_clock(), rxq, L2TISAR0, L2RISAR0))
		CCCI_ERROR_LOG(0, TAG, "DPMAIF IRQ L2(%x/%x)(%x/%x)\n",
			L2TISAR0_bak, L2RISAR0_bak, L2TIMR0, L2RIMR0);
#endif

	return IRQ_HANDLED;
}

static irqreturn_t drv3_isr1(int irq, void *data)
{
	struct dpmaif_rx_queue *rxq = (struct dpmaif_rx_queue *)data;
	unsigned int L2RISAR0 = ccci_drv_get_dl_isr_event();
	unsigned int L2RIMR0  = drv3_get_dl_interrupt_mask();
#ifdef ENABLE_DPMAIF_ISR_LOG
		unsigned int L2RISAR0_bak = L2RISAR0;
#endif

	/* clear IP busy register wake up cpu case */
	ccci_drv_clear_ip_busy();

	if (atomic_read(&dpmaif_ctl->wakeup_src) == 1)
		CCCI_NOTICE_LOG(0, TAG, "[%s] wake up by MD0 HIF L2(%x/%x)!\n",
			__func__, L2RISAR0, L2RIMR0);

	/* RX interrupt */
	if (L2RISAR0) {
		if (L2RISAR0 & DP_DL_INT_LRO1_QDONE_SET) {
			/* ACK RX interrupt */
			DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0, DP_DL_INT_LRO1_QDONE_SET);
			/* disable RX_DONE  interrupt */
			drv3_mask_dl_lro1_interrupt();
			/*always start work due to no napi*/
			tasklet_hi_schedule(&rxq->rxq_task);
		}
	}

	if (g_debug_flags & DEBUG_RXTX_ISR) {
		struct debug_rxtx_isr_hdr hdr = {0};

		hdr.type = TYPE_RXTX_ISR_ID;
		hdr.qidx = 1;
		hdr.time = (unsigned int)(local_clock() >> 16);
		hdr.rxsr = L2RISAR0;
		hdr.rxmr = L2RIMR0;
		ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
	}
#ifdef ENABLE_DPMAIF_ISR_LOG
	if (ccci_dpmaif_record_isr_cnt(local_clock(), rxq, 0, L2RISAR0))
		CCCI_ERROR_LOG(0, TAG, "%s:DPMAIF IRQ L2(%x/%x)\n",
			__func__, L2RISAR0_bak, L2RISAR0);
#endif

	return IRQ_HANDLED;
}

static irqreturn_t drv3_isr(int irq, void *data)
{
	struct dpmaif_rx_queue *rxq = (struct dpmaif_rx_queue *)data;
	unsigned int L2RISAR0 = ccci_drv_get_dl_isr_event();
	unsigned int L2RIMR0  = drv3_get_dl_interrupt_mask();
	unsigned int L2TISAR0 = ccci_drv_get_ul_isr_event();
	unsigned int L2TIMR0  = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMR0);
#ifdef ENABLE_DPMAIF_ISR_LOG
	unsigned int L2RISAR0_bak = L2RISAR0, L2TISAR0_bak = L2TISAR0;
#endif

	/* clear IP busy register wake up cpu case */
	ccci_drv_clear_ip_busy();

	if (atomic_read(&dpmaif_ctl->wakeup_src) == 1)
		CCCI_NOTICE_LOG(0, TAG, "[%s] wake up by MD0 HIF L2(%x/%x)(%x/%x)!\n",
			__func__, L2TISAR0, L2TIMR0, L2RISAR0, L2RIMR0);

	/* TX interrupt */
	if (L2TISAR0) {
		L2TISAR0 &= ~(L2TIMR0);

		DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, L2TISAR0);

		/* this log may be printed frequently, so first cancel it*/
		if (L2TISAR0 & DPMAIF_UL_INT_ERR_MSK)
			CCCI_ERROR_LOG(0, TAG, "[%s] dpmaif: ul error L2(%x)\n",
				__func__, L2TISAR0);

		/* tx done */
		if (L2TISAR0 & DPMAIF_UL_INT_QDONE_MSK)
			drv3_irq_tx_done(L2TISAR0 & DPMAIF_UL_INT_QDONE_MSK);
	}

	/* RX interrupt */
	if (L2RISAR0) {
		L2RISAR0 &= ~(L2RIMR0);

		if (L2RISAR0 & AP_DL_L2INTR_ERR_En_Msk)
			ccci_irq_rx_lenerr_handler(L2RISAR0 & AP_DL_L2INTR_ERR_En_Msk);

		/* ACK interrupt after lenerr_handler*/
		/* ACK RX interrupt */
		DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0, L2RISAR0);

		if (L2RISAR0 & DPMAIF_DL_INT_QDONE_MSK) {
			/* disable RX_DONE  interrupt */
			drv3_mask_dl_interrupt();

			/*always start work due to no napi*/
			/*for (i = 0; i < DPMAIF_HW_MAX_DLQ_NUM; i++)*/
			tasklet_hi_schedule(&rxq->rxq_task);
		}
	}

	if (g_debug_flags & DEBUG_RXTX_ISR) {
		struct debug_rxtx_isr_hdr hdr = {0};

		hdr.type = TYPE_RXTX_ISR_ID;
		hdr.qidx = 0;
		hdr.time = (unsigned int)(local_clock() >> 16);
		hdr.rxsr = L2RISAR0;
		hdr.rxmr = L2RIMR0;
		hdr.txsr = L2TISAR0;
		hdr.txmr = L2TIMR0;
		ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
	}
#ifdef ENABLE_DPMAIF_ISR_LOG
	if (ccci_dpmaif_record_isr_cnt(local_clock(), rxq, L2TISAR0, L2RISAR0))
		CCCI_ERROR_LOG(0, TAG, "DPMAIF IRQ L2(%x/%x)(%x/%x)\n",
			L2TISAR0_bak, L2RISAR0_bak, L2TIMR0, L2RIMR0);
#endif

	return IRQ_HANDLED;
}

static void drv3_infra_ao_com_set(void)
{
	unsigned int value = 0;
	int ret;

	ret = regmap_read(dpmaif_ctl->infra_ao_base, 0x0208, &value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d error: read infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);

	value |= (1<<15);

	ret = regmap_write(dpmaif_ctl->infra_ao_base, 0x0208, value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d error: write infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);

	ret = regmap_read(dpmaif_ctl->infra_ao_base, 0x0208, &value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d error: read infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);
}

/* =======================================================
 *
 * Descriptions: State part (1/3): Init(ISR)
 *
 * ========================================================
 */
static int drv3_intr_hw_init(void)
{
	int count = 0;

	/* UL/TX interrupt init */
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, 0xFFFFFFFF);
	/* 2. set interrupt enable mask*/
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMCR0, AP_UL_L2INTR_En_Msk);
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMSR0, ~(AP_UL_L2INTR_En_Msk));

	/* 3. check mask sts*/
	while ((DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMR0) & AP_UL_L2INTR_En_Msk)
			== AP_UL_L2INTR_En_Msk) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] error: 1st fail\n", __func__);
			return HW_REG_TIME_OUT;
		}
	}

	if (!dpmaif_ctl->support_2rxq) {
		/*Set DL/RX interrupt*/
		/* 1. clear dummy sts*/
		DPMA_WRITE_PD_MISC(NRL2_DPMAIF_AP_MISC_APDL_L2TISAR0, 0xFFFFFFFF);
		/* 2. clear interrupt enable mask*/
		DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0, AP_DL_L2INTR_En_Msk);
		/*set interrupt enable mask*/
		DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0, ~(AP_DL_L2INTR_En_Msk));

		/* 3. check mask sts*/
		count = 0;
		while ((DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMR0) &
			AP_DL_L2INTR_En_Msk) == AP_DL_L2INTR_En_Msk) {
			if (++count >= 1600000) {
				CCCI_ERROR_LOG(0, TAG, "[%s] error: 2nd fail\n", __func__);
				return HW_REG_TIME_OUT;
			}
		}
	} else {
		/*Set DL/RX interrupt*/
		/* 1. clear dummy sts*/
		DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0, 0xFFFFFFFF);
		/* 2. clear interrupt enable mask*/
		DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0, AP_DL_L2INTR_LRO_En_Msk);
		/*set interrupt enable mask*/
		DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0, ~(AP_DL_L2INTR_LRO_En_Msk));

		/* 3. check mask sts*/
		count = 0;
		while ((DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMR0) &
			AP_DL_L2INTR_LRO_En_Msk) == AP_DL_L2INTR_LRO_En_Msk) {
			if (++count >= 1600000) {
				CCCI_ERROR_LOG(0, TAG, "[%s] error: 2nd fail\n", __func__);
				return HW_REG_TIME_OUT;
			}
		}
	}

	/* Set AP IP busy */
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(NRL2_DPMAIF_AP_MISC_AP_IP_BUSY, 0xFFFFFFFF);
	/* 2. set IP busy unmask*/
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_DL_UL_IP_BUSY_MASK, 0);

	return 0;
}

static void drv3_ul_all_queue_en(bool enable)
{
	unsigned long ul_arb_en;

	ul_arb_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);

	if (enable == true)
		ul_arb_en |= DPMAIF_UL_ALL_QUE_ARB_EN;
	else
		ul_arb_en &= ~DPMAIF_UL_ALL_QUE_ARB_EN;

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_arb_en);
}

static unsigned int drv3_ul_idle_check(void)
{
	unsigned long idle_sts;

	idle_sts = ((DPMA_READ_PD_UL(DPMAIF_PD_UL_DBG_STA2) >> DPMAIF_UL_STS_CUR_SHIFT)
		& DPMAIF_UL_IDLE_STS_MSK);

	if (idle_sts == DPMAIF_UL_IDLE_STS)
		return 0;
	else
		return 1;
}

static bool drv3_dpmaif_check_power_down(void)
{
	if (DPMA_READ_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW) == 0) {
		DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW, 0xFF);
		return true;
	}

	return false;
}

static int drv3_start(void)
{
	drv3_infra_ao_com_set();
	drv3_common_hw_init();
	return drv3_intr_hw_init();
}

static void drv3_ul_arb_en(unsigned char q_num, bool enable)
{
	unsigned int ul_arb_en;

	ul_arb_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);

	if (enable == true)
		ul_arb_en |= (1<<(q_num+8));
	else
		ul_arb_en &= ~(1<<(q_num+8));

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_arb_en);
}

static void drv3_ul_rdy_en(unsigned char q_num, bool ready)
{
	unsigned int ul_rdy_en;

	ul_rdy_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);

	if (ready == true)
		ul_rdy_en |= (1<<q_num);
	else
		ul_rdy_en &= ~(1<<q_num);

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_rdy_en);
}

static void drv3_txq_hw_init(struct dpmaif_tx_queue *txq)
{
	unsigned long long base_addr;

	if (txq->started == false) {
		drv3_ul_arb_en(txq->index, false);
		return;
	}

	base_addr = (unsigned long long)txq->drb_phy_addr;

	/* 1. BAT buffer parameters setting */
	drv3_ul_update_drb_size(txq->index,
		txq->drb_cnt * sizeof(struct dpmaif_drb_pd));
	drv3_ul_update_drb_base_addr(txq->index,
		(base_addr&0xFFFFFFFF), ((base_addr>>32)&0xFFFFFFFF));

	drv3_ul_rdy_en(txq->index, true);
	drv3_ul_arb_en(txq->index, true);

}

static void drv3_dl_set_lro_pit_base_addr(dma_addr_t addr)
{
	unsigned int lb_addr = (unsigned int)(addr & 0xFFFFFFFF);
	unsigned int value;

	DPMA_WRITE_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_INIT_CON0, lb_addr);

	value = DPMA_READ_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_INIT_CON1);
	value |= ((addr >> 8) & DPMAIF_PIT_ADDRH_MSK);
	DPMA_WRITE_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_INIT_CON1, value);
}

static void drv3_dl_set_lro_pit_size(unsigned int size)
{
	unsigned int value;

	value = DPMA_READ_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_INIT_CON1);

	value &= ~(DPMAIF_PIT_SIZE_MSK);
	value |= (size & DPMAIF_PIT_SIZE_MSK);

	DPMA_WRITE_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_INIT_CON1, value);
}

static void drv3_dl_lro_pit_en(bool enable)
{
	unsigned int value;

	value = DPMA_READ_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_INIT_CON3);

	if (enable == true)
		value |= DPMAIF_LROPIT_EN_MSK;
	else
		value &= ~DPMAIF_LROPIT_EN_MSK;

	DPMA_WRITE_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_INIT_CON3, value);
}

void drv3_dl_set_ao_lro_pit_chknum(unsigned int pit_idx)
{
	unsigned int value;

	value = DPMA_READ_AO_DL(NRL2_DPMAIF_AO_DL_PIT_SEQ_END);

	if (pit_idx == 0) {
		value &= ~(DPMAIF_LRO_PIT0_CHK_NUM_MASK);
		value |= (((0xFF)<<DPMAIF_LRO_PIT0_CHK_NUM_OFS)&DPMAIF_LRO_PIT0_CHK_NUM_MASK);
	} else {
		value &= ~(DPMAIF_LRO_PIT1_CHK_NUM_MASK);
		value |= (((0xFF)<<DPMAIF_LRO_PIT1_CHK_NUM_OFS)&DPMAIF_LRO_PIT1_CHK_NUM_MASK);
	}
	//DPMA_WRITE_AO_DL(NRL2_DPMAIF_AO_DL_PIT_SEQ_END,value);
	DPMA_WRITE_AO_DL_SRAM(NRL2_DPMAIF_AO_DL_PIT_SEQ_END, value);
}

static void drv3_dl_lro_pit_init_done(unsigned int pit_idx)
{
	unsigned int dl_pit_init = 0;
	int count;

	dl_pit_init |= DPMAIF_DL_PIT_INIT_ALLSET;
	dl_pit_init |= (pit_idx << DPMAIF_LROPIT_CHAN_OFS);
	dl_pit_init |= DPMAIF_DL_PIT_INIT_EN;

	count = 0;
	while (1) {
		if ((DPMA_READ_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_INIT) &
			DPMAIF_DL_PIT_INIT_NOT_READY) == 0) {

			DPMA_WRITE_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_INIT, dl_pit_init);
			break;
		}

		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] 1st error: pit init fail\n", __func__);
			break;
		}
	}

	count = 0;
	while ((DPMA_READ_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_INIT) &
		DPMAIF_DL_PIT_INIT_NOT_READY) == DPMAIF_DL_PIT_INIT_NOT_READY) {

		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "[%s] 2nd error: pit init fail\n", __func__);
			break;
		}
	}
}

void ccci_drv3_dl_config_lro_hw(dma_addr_t addr, unsigned int size,
	bool enable, unsigned int pit_idx)
{
	drv3_dl_set_lro_pit_base_addr(addr);
	drv3_dl_set_lro_pit_size(size);
	drv3_dl_lro_pit_en(enable);
	drv3_dl_set_ao_lro_pit_chknum(pit_idx);
	drv3_dl_lro_pit_init_done(pit_idx);
}

static void drv3_hw_hpc_cntl_set(void)
{
	unsigned int cfg = 0;

	cfg =  ((DPMAIF_HPC_LRO_PATH_DF & 0x3)  << 0);
	cfg |= ((DPMAIF_HPC_ADD_MODE_DF & 0x3)  << 2);
	cfg |= ((DPMAIF_HASH_PRIME_DF   & 0xf)  << 4);
	cfg |= ((DPMAIF_HPC_TOTAL_NUM   & 0xff) << 8);

	//cfg include hpclro path, hpc add mode, hash prime, hpc total num
	//DPMA_WRITE_AO_DL(NRL2_DPMAIF_AO_DL_HPC_CNTL, cfg);
	DPMA_WRITE_AO_DL_SRAM(NRL2_DPMAIF_AO_DL_HPC_CNTL, cfg);
}

static void drv3_hw_agg_cfg_set(void)
{
	unsigned int cfg = 0;

	cfg = ((DPMAIF_AGG_MAX_LEN_DF & 0xffff) << 0);
	cfg |= ((DPMAIF_AGG_TBL_ENT_NUM_DF & 0xffff) << 16);

	//cfg include agg max length, agg table num
	//DPMA_WRITE_AO_DL(NRL2_DPMAIF_AO_DL_LRO_AGG_CFG, cfg);
	DPMA_WRITE_AO_DL_SRAM(NRL2_DPMAIF_AO_DL_LRO_AGG_CFG, cfg);
}

static void drv3_hw_hash_bit_choose_set(void)
{
	unsigned int cfg = 0;

	cfg = ((DPMAIF_LRO_HASH_BIT_CHOOSE_DF & 0x7) << 0);

	DPMA_WRITE_AO_DL(NRL2_DPMAIF_AO_DL_LROPIT_INIT_CON5, cfg);
}

static void drv3_hw_mid_pit_timeout_thres_set(void)
{
	//DPMA_WRITE_AO_DL(NRL2_DPMAIF_AO_DL_LROPIT_TIMEOUT0, DPMAIF_MID_TIMEOUT_THRES_DF);
	DPMA_WRITE_AO_DL_SRAM(NRL2_DPMAIF_AO_DL_LROPIT_TIMEOUT0, DPMAIF_MID_TIMEOUT_THRES_DF);
}

static void drv3_hw_lro_timeout_thres_set(void)
{
	unsigned int tmp, idx;

	for (idx = 0; idx < DPMAIF_HPC_MAX_TOTAL_NUM; idx++) {
		//tmp = DPMA_READ_AO_DL(NRL2_DPMAIF_AO_DL_LROPIT_TIMEOUT1 + 4*(idx/2));
		tmp = DPMA_READ_AO_DL_SRAM(NRL2_DPMAIF_AO_DL_LROPIT_TIMEOUT1 + 4*(idx/2));
		if (idx % 2)  //odd idx
			tmp = ((tmp & 0xFFFF) | (DPMAIF_LRO_TIMEOUT_THRES_DF << 16));
		else  //even idx
			tmp = ((tmp & 0xFFFF0000) | (DPMAIF_LRO_TIMEOUT_THRES_DF));

		//DPMA_WRITE_AO_DL(NRL2_DPMAIF_AO_DL_LROPIT_TIMEOUT1 + (4*(idx/2)), tmp);
		DPMA_WRITE_AO_DL_SRAM(NRL2_DPMAIF_AO_DL_LROPIT_TIMEOUT1 + (4*(idx/2)), tmp);
	}
}

static void drv3_hw_lro_start_prs_thres_set(void)
{
	unsigned int cfg = 0;

	cfg = (DPMAIF_LRO_PRS_THRES_DF & 0x3FFFF);

	DPMA_WRITE_AO_DL(NRL2_DPMAIF_AO_DL_LROPIT_TRIG_THRES, cfg);
}

static void drv3_hw_lro_set_agg_en_df(bool enable)
{
	unsigned int value;

	value = DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	value &= ~(0xFF<<20);

	if (enable == true)
		value |= (0xFF<<20);
	else
		value &= ~(0xFF<<20);

	DPMA_WRITE_AO_DL_SRAM(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);
}

void ccci_drv3_dl_lro_hpc_hw_init(void)
{
	drv3_hw_hpc_cntl_set();
	drv3_hw_agg_cfg_set();
	drv3_hw_hash_bit_choose_set();
	drv3_hw_mid_pit_timeout_thres_set();
	drv3_hw_lro_timeout_thres_set();
	drv3_hw_lro_start_prs_thres_set();
	drv3_hw_lro_set_agg_en_df(true);
}

static int drv3_suspend_noirq(struct device *dev)
{
	g_backup_dl_isr = drv3_get_dl_interrupt_mask();
	g_backup_ul_isr = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMR0);
	return 0;
}

static int drv3_resume_noirq(struct device *dev)
{
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMSR0, g_backup_ul_isr);
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0, g_backup_dl_isr);

	/* use msk to clear dummy interrupt */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0, g_backup_dl_isr);
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, g_backup_ul_isr);
	return 0;
}

static void drv3_dump_register(int buf_type)
{
	int len;

	len = DPMAIF_PD_UL_ADD_DESC_CH - DPMAIF_PD_UL_ADD_DESC + 4;
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF Tx pdn; pd_ul_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_ul_base + NRL2_DPMAIF_UL_ADD_DESC, len);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_ul_base + NRL2_DPMAIF_UL_ADD_DESC, len);

	if (g_plat_inf == 6985) {
		len = DPMAIF_AO_UL_CHNL3_STA_6985 - DPMAIF_AO_UL_CHNL0_STA_6985 + 4;
		CCCI_BUF_LOG_TAG(0, buf_type, TAG,
			"dump AP DPMAIF Tx ao; ao_ul_base register -> (start addr: 0x%llX, len: %d):\n",
			(unsigned long long)dpmaif_ctl->ao_ul_base + DPMAIF_AO_UL_CHNL0_STA_6985,
				len);
		ccci_util_mem_dump(buf_type,
			dpmaif_ctl->ao_ul_base + DPMAIF_AO_UL_CHNL0_STA_6985, len);
	} else {
		len = DPMAIF_AO_UL_CHNL3_STA - DPMAIF_AO_UL_CHNL0_STA + 4;
		CCCI_BUF_LOG_TAG(0, buf_type, TAG,
			"dump AP DPMAIF Tx ao; ao_ul_base register -> (start addr: 0x%llX, len: %d):\n",
			(unsigned long long)dpmaif_ctl->ao_ul_base + DPMAIF_AO_UL_CHNL0_STA, len);
		ccci_util_mem_dump(buf_type,
			dpmaif_ctl->ao_ul_base + DPMAIF_AO_UL_CHNL0_STA, len);
	}

	len = DPMAIF_PD_DL_MISC_CON0 - DPMAIF_PD_DL_BAT_INIT + 4;
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF Rx pdn; pd_dl_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_BAT_INIT, len);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_BAT_INIT, len);

	len = DPMAIF_PD_DL_DBG_STA14 - DPMAIF_PD_DL_STA0 + 4;
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF Rx pdn; pd_dl_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_STA0, len);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_STA0, len);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF dma_rd; pd_dl_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_dl_base + 0x100, 0xC8);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + 0x100, 0xC8);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF dma_wr; pd_dl_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_dl_base + 0x200, 0x58 + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + 0x200, 0x58 + 4);

	len = DPMAIF_AO_DL_FRGBAT_STA2 - DPMAIF_AO_DL_PKTINFO_CONO + 4;
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF Rx ao; ao_dl_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->ao_dl_base + DPMAIF_AO_DL_PKTINFO_CONO, len);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->ao_dl_base + DPMAIF_AO_DL_PKTINFO_CONO, len);

	len = DPMAIF_PD_AP_CODA_VER - DPMAIF_PD_AP_UL_L2TISAR0 + 4;
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF MISC pdn; pd_misc_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_misc_base + DPMAIF_PD_AP_UL_L2TISAR0, len);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_misc_base + DPMAIF_PD_AP_UL_L2TISAR0, len);

	/* open sram clock for debug sram needs sram clock. */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_CG_EN, 0x36);
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF SRAM pdn; pd_sram_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_sram_base + 0x00, 0x184);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_sram_base + 0x00, 0x184);

	/* open sram clock for sram dump */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_CG_EN, 0x7F);
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF AO MISC SRAM; ao_msic_sram_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->ao_msic_sram_base + 0x00, 0xFF);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->ao_msic_sram_base + 0x00, 0xFF);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF AO UL SRAM; ao_ul_sram_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->ao_ul_sram_base + 0x00, 0xFF);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->ao_ul_sram_base + 0x00, 0xFF);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF AO DL SRAM; ao_dl_sram_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->ao_dl_sram_base + 0x00, 0xFF);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->ao_dl_sram_base + 0x00, 0xFF);

#ifdef ENABLE_DPMAIF_ISR_LOG
	ccci_dpmaif_show_irq_log();
#endif
}

static void drv3_hw_reset(void)
{
	unsigned int value = 0;
	int ret;

	ret = regmap_read(dpmaif_ctl->infra_ao_base, 0x0208, &value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d read infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);
	value &= ~(1<<15);

	ret = regmap_write(dpmaif_ctl->infra_ao_base, 0x0208, value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d write infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);
	ret = regmap_read(dpmaif_ctl->infra_ao_base, 0x0208, &value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d read infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);
	udelay(500);

	/* DPMAIF HW reset */
	CCCI_BOOTUP_LOG(0, TAG, "%s:rst dpmaif\n", __func__);
	/* reset dpmaif hw: PD Domain */
	ret = regmap_write(dpmaif_ctl->infra_ao_base, INFRA_RST0_REG_PD, DPMAIF_PD_RST_MASK);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d write infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);
	ret = regmap_read(dpmaif_ctl->infra_ao_base, INFRA_RST0_REG_PD, &value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d read infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);
	udelay(500);

	/* reset dpmaif hw: AO Domain */
	CCCI_BOOTUP_LOG(0, TAG, "%s:clear reset\n", __func__);
	ret = regmap_write(dpmaif_ctl->infra_ao_base, INFRA_RST0_REG_AO, DPMAIF_AO_RST_MASK);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d write infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);
	ret = regmap_read(dpmaif_ctl->infra_ao_base, INFRA_RST0_REG_AO, &value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d read infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);
	udelay(500);

	/* reset dpmaif clr */
	CCCI_BOOTUP_LOG(0, TAG, "%s:clear reset\n", __func__);
	ret = regmap_write(dpmaif_ctl->infra_ao_base, INFRA_RST1_REG_AO, DPMAIF_AO_RST_MASK);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d write infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);
	ret = regmap_read(dpmaif_ctl->infra_ao_base, INFRA_RST1_REG_AO, &value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d read infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);
	udelay(500);

	/* reset dpmaif clr */
	CCCI_BOOTUP_LOG(0, TAG, "[%s]:done ret: %d\n", __func__, ret);
	ret = regmap_write(dpmaif_ctl->infra_ao_base, INFRA_RST1_REG_PD, DPMAIF_PD_RST_MASK);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d write infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);
	ret = regmap_read(dpmaif_ctl->infra_ao_base, INFRA_RST1_REG_PD, &value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
		"[%s]-%d read infra_ao_base ret=%d\n",
		__func__, __LINE__, ret);
}

static void drv3_hw_reset_v1(void)
{
	unsigned int value = 0;
	int ret;

	ret = regmap_read(dpmaif_ctl->infra_ao_base, 0x0208, &value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG, "[%s]-%d read infra_ao_base ret=%d\n",
			__func__, __LINE__, ret);
	value &= ~(1<<15);

	ret = regmap_write(dpmaif_ctl->infra_ao_base, 0x0208, value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG, "[%s]-%d write infra_ao_base ret=%d\n",
			__func__, __LINE__, ret);
	ret = regmap_read(dpmaif_ctl->infra_ao_base, 0x0208, &value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG, "[%s]-%d read infra_ao_base ret=%d\n",
			__func__, __LINE__, ret);
	udelay(500);

	/* DPMAIF HW reset */
	CCCI_DEBUG_LOG(0, TAG, "%s:rst dpmaif\n", __func__);
	/* reset dpmaif hw: PD Domain */
	if (g_plat_inf == 6985)
		dpmaif_write32(dpmaif_ctl->infra_reset_pd_base, 0xF50, 1<<14);
	else
		dpmaif_write32(dpmaif_ctl->infra_reset_pd_base, 0xF50, 1<<22);

	value = dpmaif_read32(dpmaif_ctl->infra_reset_pd_base, 0xF50);
	CCCI_NORMAL_LOG(0, TAG, "[%s]-%d read 0xF50 value=%d\n",
			__func__, __LINE__, value);

	udelay(500);

	/* reset dpmaif hw: AO Domain */
	ret = regmap_write(dpmaif_ctl->infra_ao_base, 0x130, 1<<0);
	if (ret)
		CCCI_ERROR_LOG(0, TAG, "[%s]-%d write 0x130 ret=%d\n",
			__func__, __LINE__, ret);
	ret = regmap_read(dpmaif_ctl->infra_ao_base, 0x130, &value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG, "[%s]-%d read 0x130 ret=%d\n",
			__func__, __LINE__, ret);

	udelay(500);

	/* reset dpmaif clr */
	ret = regmap_write(dpmaif_ctl->infra_ao_base, 0x134, 1<<0);
	if (ret)
		CCCI_ERROR_LOG(0, TAG, "[%s]-%d write 0x134 ret=%d\n",
			__func__, __LINE__, ret);
	ret = regmap_read(dpmaif_ctl->infra_ao_base, 0x134, &value);
	if (ret)
		CCCI_ERROR_LOG(0, TAG, "[%s]-%d read 0x134 ret=%d\n",
			__func__, __LINE__, ret);
	CCCI_BOOTUP_LOG(0, TAG, "[%s]: done. %d\n", __func__, ret);

	udelay(500);

	/* reset dpmaif clr */
	if (g_plat_inf == 6985)
		dpmaif_write32(dpmaif_ctl->infra_reset_pd_base, 0xF54, 1<<14);
	else
		dpmaif_write32(dpmaif_ctl->infra_reset_pd_base, 0xF54, 1<<22);

	value = dpmaif_read32(dpmaif_ctl->infra_reset_pd_base, 0xF54);
	CCCI_NORMAL_LOG(0, TAG, "[%s]-%d read 0xF54 value=%d\n",
			__func__, __LINE__, value);
}

static int drv3_setting_hw_reset_func(void)
{
	int ret;
	int hw_reset_ver = 0;
	struct device_node *node;

	ret = of_property_read_u32(dpmaif_ctl->dev->of_node, "hw-reset-ver",
			&hw_reset_ver);

	if (hw_reset_ver == 1) {
		node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg");
		if (!node) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: infracfg node is not exist.\n", __func__);
			return -1;
		}
		dpmaif_ctl->infra_reset_pd_base = of_iomap(node, 0);
		if (dpmaif_ctl->infra_reset_pd_base == NULL) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: the reg value of infracfg is not exist.\n",
				__func__);
			return -1;
		}
	}

	if (hw_reset_ver == 1)  // for mt6855&mt6985
		ops.drv_hw_reset = &drv3_hw_reset_v1;
	else  // for other 98 dpmaif chip
		ops.drv_hw_reset = &drv3_hw_reset;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] hw_reset_ver: %d; infra_reset_pd_base: %p\n",
		__func__, hw_reset_ver, dpmaif_ctl->infra_reset_pd_base);

	return 0;
}

static spinlock_t g_add_pit_cnt_lro_lock;
static int drv3_dl_add_lro0_pit_remain_cnt(unsigned short pit_remain_cnt)
{
	unsigned int dl_update;
	int count = 0, ret = 0;

	dl_update = ((pit_remain_cnt & 0x0003FFFF) | DPMAIF_DL_ADD_UPDATE);

	if (dpmaif_ctl->support_2rxq)
		spin_lock(&g_add_pit_cnt_lro_lock);

	while (1) {
		if ((DPMA_READ_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_ADD)
				& DPMAIF_DL_ADD_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_ADD, dl_update);
			break;
		}

		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: 1st DPMAIF_PD_DL_PIT_ADD read fail\n", __func__);
			ret = HW_REG_TIME_OUT;
			goto fun_exit;
		}
	}

	count = 0;
	while ((DPMA_READ_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_ADD) &
			DPMAIF_DL_ADD_NOT_READY) == DPMAIF_DL_ADD_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				 "[%s] error: 2nd DPMAIF_PD_DL_PIT_ADD read fail\n", __func__);
			ret = HW_REG_TIME_OUT;
			goto fun_exit;
		}
	}

fun_exit:
	if (dpmaif_ctl->support_2rxq)
		spin_unlock(&g_add_pit_cnt_lro_lock);

	return ret;
}

static int drv3_dl_add_lro1_pit_remain_cnt(unsigned short pit_remain_cnt)
{
	unsigned int dl_update;
	int count = 0, ret = 0;

	dl_update = (pit_remain_cnt & 0x0003FFFF);
	dl_update |= (DPMAIF_DL_ADD_UPDATE | DPMAIF_ADD_LRO_PIT_CHAN_OFS);

	if (dpmaif_ctl->support_2rxq)
		spin_lock(&g_add_pit_cnt_lro_lock);

	while (1) {
		if ((DPMA_READ_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_ADD) &
				DPMAIF_DL_ADD_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_ADD, dl_update);
			break;
		}

		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: 1st DPMAIF_PD_DL_PIT_ADD read fail\n", __func__);
			ret = HW_REG_TIME_OUT;
			goto fun_exit;
		}
	}

	count = 0;
	while ((DPMA_READ_PD_DL_LRO(NRL2_DPMAIF_DL_LROPIT_ADD) &
			DPMAIF_DL_ADD_NOT_READY) == DPMAIF_DL_ADD_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				 "[%s] error: 2nd DPMAIF_PD_DL_PIT_ADD read fail\n", __func__);
			ret = HW_REG_TIME_OUT;
			goto fun_exit;
		}
	}

fun_exit:
	if (dpmaif_ctl->support_2rxq)
		spin_unlock(&g_add_pit_cnt_lro_lock);

	return 0;
}

static int drv3_init_rxq_cb(void)
{
	int i;
	struct dpmaif_rx_queue *rxq;

	if (dpmaif_ctl->support_2rxq)
		spin_lock_init(&g_add_pit_cnt_lro_lock);

	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++) {
		rxq = &dpmaif_ctl->rxq[i];

		if (dpmaif_ctl->support_2rxq) {
			if (i == 0) {
				rxq->rxq_isr = drv3_isr0;
				rxq->rxq_drv_unmask_dl_interrupt = &drv3_unmask_dl_lro0_interrupt;
				rxq->rxq_drv_dl_add_pit_remain_cnt =
					&drv3_dl_add_lro0_pit_remain_cnt;
			} else if (i == 1) {
				rxq->rxq_isr = drv3_isr1;
				rxq->rxq_drv_unmask_dl_interrupt = &drv3_unmask_dl_lro1_interrupt;
				rxq->rxq_drv_dl_add_pit_remain_cnt =
					&drv3_dl_add_lro1_pit_remain_cnt;
			} else {
				CCCI_ERROR_LOG(0, TAG,
					"[%s] error: no isr func for rxq%d\n",
					__func__, i);
				return -1;
			}
		} else {
			rxq->rxq_isr = &drv3_isr;
			rxq->rxq_drv_unmask_dl_interrupt = &drv3_unmask_dl_interrupt;
			rxq->rxq_drv_dl_add_pit_remain_cnt = &ccci_drv_dl_add_pit_remain_cnt;
		}
	}

	return 0;
}

int ccci_dpmaif_drv3_init(void)
{
	int ret;

	dpmaif_ctl->clk_tbs = g_clk_tbs;

	/* for 97 dpmaif new register */
	dpmaif_ctl->ao_md_dl_base = dpmaif_ctl->ao_ul_base + 0x800;
	dpmaif_ctl->pd_rdma_base  = dpmaif_ctl->pd_ul_base + 0x200;
	dpmaif_ctl->pd_wdma_base  = dpmaif_ctl->pd_ul_base + 0x300;

	if (dpmaif_ctl->support_2rxq) {
		dpmaif_ctl->pd_mmw_hpc_base = dpmaif_ctl->pd_ul_base + 0x600;
		dpmaif_ctl->pd_dl_lro_base  = dpmaif_ctl->pd_ul_base + 0x900;
	}

	/* for 98 dpmaif new register */
	dpmaif_ctl->ao_dl_sram_base   = dpmaif_ctl->pd_ul_base + 0xC00;
	dpmaif_ctl->ao_ul_sram_base   = dpmaif_ctl->pd_ul_base + 0xD00;
	dpmaif_ctl->ao_msic_sram_base = dpmaif_ctl->pd_ul_base + 0xE00;

	if (dpmaif_ctl->dl_bat_entry_size == 0)
		dpmaif_ctl->dl_bat_entry_size = DPMAIF_DL_BAT_ENTRY_SIZE;
	dpmaif_ctl->dl_pit_entry_size = dpmaif_ctl->dl_bat_entry_size * 2;
	dpmaif_ctl->dl_pit_byte_size  = DPMAIF_DL_PIT_BYTE_SIZE;

	drv.pit_size_msk = DPMAIF_PIT_SIZE_MSK;
	drv.dl_pit_wridx_msk = DPMAIF_DL_PIT_WRIDX_MSK;
	drv.ul_int_md_not_ready_msk = DPMAIF_UL_INT_MD_NOTREADY_MSK |
				DPMAIF_UL_INT_MD_PWR_NOTREADY_MSK;
	drv.ap_ul_l2intr_err_en_msk = AP_UL_L2INTR_ERR_En_Msk;
	drv.normal_pit_size = sizeof(struct dpmaif_normal_pit_v3);
	drv.ul_int_qdone_msk = DPMAIF_UL_INT_QDONE_MSK;
	drv.dl_idle_sts = DPMAIF_DL_IDLE_STS;

	if (drv3_init_rxq_cb())
		return -1;

	ops.drv_start = &drv3_start;
	ops.drv_suspend_noirq = drv3_suspend_noirq;
	ops.drv_resume_noirq = drv3_resume_noirq;

	ops.drv_unmask_ul_interrupt = &drv3_unmask_ul_interrupt;

	if (dpmaif_ctl->support_2rxq)
		ops.drv_dl_get_wridx = &drv3_dl_get_lro_wridx;
	else
		ops.drv_dl_get_wridx = &drv3_dl_get_wridx;

	if (g_plat_inf == 6985) {
		ops.drv_ul_get_rwidx = &drv3_ul_get_rwidx_6985;
		ops.drv_ul_get_rdidx = &drv3_ul_get_rdidx_6985;
	} else {
		ops.drv_ul_get_rwidx = &drv3_ul_get_rwidx;
		ops.drv_ul_get_rdidx = &drv3_ul_get_rdidx;
	}

	ops.drv_ul_all_queue_en = &drv3_ul_all_queue_en;
	ops.drv_ul_idle_check = &drv3_ul_idle_check;

	ret = drv3_setting_hw_reset_func();
	if (ret)
		return ret;

	ops.drv_check_power_down = &drv3_dpmaif_check_power_down;
	ops.drv_get_dl_interrupt_mask = &drv3_get_dl_interrupt_mask;
	ops.drv_txq_hw_init = &drv3_txq_hw_init;
	ops.drv_dump_register = &drv3_dump_register;

	return 0;
}

