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

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L1TIMR0, ((1<<9)|(1<<10)|(1<<15)|(1<<16)));

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

static void drv3_unmask_ul_interrupt(unsigned char q_num)
{
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMCR0,
		(DPMAIF_UL_INT_DONE(q_num) & DPMAIF_UL_INT_QDONE_MSK));

	/*check mask sts*/
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
	 *	ui_que_done_mask) == ui_que_done_mask);
	 */
}

static unsigned int drv3_dl_get_wridx(void)
{
	return (DPMA_READ_AO_DL_SRAM(DPMAIF_AO_DL_PIT_STA3) & DPMAIF_DL_PIT_WRIDX_MSK);
}

static inline unsigned int drv3_get_dl_interrupt_mask(void)
{
	return DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMR0);
}

static irqreturn_t drv3_isr(int irq, void *data)
{
	unsigned int L2RISAR0 = ccci_drv_get_dl_isr_event();
	unsigned int L2RIMR0  = drv3_get_dl_interrupt_mask();
	unsigned int L2TISAR0 = ccci_drv_get_ul_isr_event();
	unsigned int L2TIMR0  = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMR0);
	unsigned int L2RISAR0_bak = L2RISAR0, L2TISAR0_bak = L2TISAR0;

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
			tasklet_hi_schedule(&dpmaif_ctl->rxq[0].rxq_task);
		}
	}

#ifdef ENABLE_DPMAIF_ISR_LOG
	if (dpmaif_ctl->enable_pit_debug > -1) {
		if (ccci_dpmaif_record_isr_cnt(local_clock(), L2TISAR0, L2RISAR0))
			CCCI_ERROR_LOG(0, TAG, "DPMAIF IRQ L2(%x/%x)(%x/%x)\n",
					L2TISAR0_bak, L2RISAR0_bak, L2TIMR0, L2RIMR0);
	}
#endif

	return IRQ_HANDLED;
}

static void drv3_infra_ao_com_set(void)
{
	unsigned int value;
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
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: 2nd fail\n", __func__);
			return HW_REG_TIME_OUT;
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

static int drv3_suspend_noirq(struct device *dev)
{
	return 0;
}

static int drv3_resume_noirq(struct device *dev)
{
	return 0;
}

static void drv3_dump_register(int buf_type)
{
	int len;

	len = DPMAIF_PD_UL_ADD_DESC_CH - DPMAIF_PD_UL_ADD_DESC + 4;
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF Tx pdn; pd_ul_base register -> (start addr: 0x%lX, len: %d):\n",
		dpmaif_ctl->pd_ul_base + NRL2_DPMAIF_UL_ADD_DESC, len);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_ul_base + NRL2_DPMAIF_UL_ADD_DESC, len);

	if (g_plat_inf == 6985) {
		len = DPMAIF_AO_UL_CHNL3_STA_6985 - DPMAIF_AO_UL_CHNL0_STA_6985 + 4;
		CCCI_BUF_LOG_TAG(0, buf_type, TAG,
			"dump AP DPMAIF Tx ao; ao_ul_base register -> (start addr: 0x%lX, len: %d):\n",
			dpmaif_ctl->ao_ul_base + DPMAIF_AO_UL_CHNL0_STA_6985, len);
		ccci_util_mem_dump(buf_type,
			dpmaif_ctl->ao_ul_base + DPMAIF_AO_UL_CHNL0_STA_6985, len);
	} else {
		len = DPMAIF_AO_UL_CHNL3_STA - DPMAIF_AO_UL_CHNL0_STA + 4;
		CCCI_BUF_LOG_TAG(0, buf_type, TAG,
			"dump AP DPMAIF Tx ao; ao_ul_base register -> (start addr: 0x%lX, len: %d):\n",
			dpmaif_ctl->ao_ul_base + DPMAIF_AO_UL_CHNL0_STA, len);
		ccci_util_mem_dump(buf_type,
			dpmaif_ctl->ao_ul_base + DPMAIF_AO_UL_CHNL0_STA, len);
	}

	len = DPMAIF_PD_DL_MISC_CON0 - DPMAIF_PD_DL_BAT_INIT + 4;
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF Rx pdn; pd_dl_base register -> (start addr: 0x%lX, len: %d):\n",
		dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_BAT_INIT, len);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_BAT_INIT, len);

	len = DPMAIF_PD_DL_DBG_STA14 - DPMAIF_PD_DL_STA0 + 4;
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF Rx pdn; pd_dl_base register -> (start addr: 0x%lX, len: %d):\n",
		dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_STA0, len);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_STA0, len);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF dma_rd; pd_dl_base register -> (start addr: 0x%lX, len: %d):\n",
		dpmaif_ctl->pd_dl_base + 0x100, 0xC8);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + 0x100, 0xC8);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF dma_wr; pd_dl_base register -> (start addr: 0x%lX, len: %d):\n",
		dpmaif_ctl->pd_dl_base + 0x200, 0x58 + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + 0x200, 0x58 + 4);

	len = DPMAIF_AO_DL_FRGBAT_STA2 - DPMAIF_AO_DL_PKTINFO_CONO + 4;
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF Rx ao; ao_dl_base register -> (start addr: 0x%lX, len: %d):\n",
		dpmaif_ctl->ao_dl_base + DPMAIF_AO_DL_PKTINFO_CONO, len);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->ao_dl_base + DPMAIF_AO_DL_PKTINFO_CONO, len);

	len = DPMAIF_PD_AP_CODA_VER - DPMAIF_PD_AP_UL_L2TISAR0 + 4;
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF MISC pdn; pd_misc_base register -> (start addr: 0x%lX, len: %d):\n",
		dpmaif_ctl->pd_misc_base + DPMAIF_PD_AP_UL_L2TISAR0, len);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_misc_base + DPMAIF_PD_AP_UL_L2TISAR0, len);

	/* open sram clock for debug sram needs sram clock. */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_CG_EN, 0x36);
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF SRAM pdn; pd_sram_base register -> (start addr: 0x%lX, len: %d):\n",
		dpmaif_ctl->pd_sram_base + 0x00, 0x184);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_sram_base + 0x00, 0x184);

#ifdef ENABLE_DPMAIF_ISR_LOG
	if (dpmaif_ctl->enable_pit_debug > -1)
		ccci_dpmaif_print_irq_log();
#endif
}

static void drv3_hw_reset(void)
{
	unsigned int value;
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
	unsigned int value;
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

	ret = of_property_read_u32(dpmaif_ctl->dev->of_node, "hw_reset_ver",
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

	if (hw_reset_ver == 1)  // for mt6855
		ops.drv_hw_reset = &drv3_hw_reset_v1;
	else  // for other 98 dpmaif chip
		ops.drv_hw_reset = &drv3_hw_reset;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] hw_reset_ver: %d; infra_reset_pd_base: %p\n",
		__func__, hw_reset_ver, dpmaif_ctl->infra_reset_pd_base);

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

	ops.drv_isr = &drv3_isr;
	ops.drv_start = &drv3_start;
	ops.drv_suspend_noirq = drv3_suspend_noirq;
	ops.drv_resume_noirq = drv3_resume_noirq;

	ops.drv_unmask_dl_interrupt = &drv3_unmask_dl_interrupt;
	ops.drv_unmask_ul_interrupt = &drv3_unmask_ul_interrupt;
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

