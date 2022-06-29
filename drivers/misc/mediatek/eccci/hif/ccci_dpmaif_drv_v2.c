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
#include "ccci_dpmaif_reg_v2.h"


#define TAG "drv2"


static struct dpmaif_clk_node g_clk_tbs[] = {
	{ NULL, "infra-dpmaif-clk"},
	{ NULL, "infra-dpmaif-blk-clk"},
	{ NULL, NULL},
};


static void drv_dl_set_wdma(void)
{
	unsigned int value;

	/*Set WDMA OSTD*/
	value = DPMA_READ_WDMA(NRL2_DPMAIF_WDMA_WR_CHNL_CMD_CON3);
	value &= ~(DPMAIF_DL_WDMA_CTRL_OSTD_MSK << DPMAIF_DL_WDMA_CTRL_OSTD_OFST);
	value |= (DPMAIF_DL_WDMA_CTRL_OSTD_VALUE << DPMAIF_DL_WDMA_CTRL_OSTD_OFST);

	DPMA_WRITE_WDMA(NRL2_DPMAIF_WDMA_WR_CHNL_CMD_CON3, value);

	/*Set CTRL_INTVAL/INTAL_MIN*/
	value = DPMA_READ_WDMA(NRL2_DPMAIF_WDMA_WR_CMD_CON0);
	value &= (0xFFFF0000);
	DPMA_WRITE_WDMA(NRL2_DPMAIF_WDMA_WR_CMD_CON0, value);
}

static void drv_md_hw_bus_remap(void)
{
	unsigned int value;
	phys_addr_t md_base_at_ap;
	unsigned int tmp_val;

	/*DPMAIF MD Domain setting:MD domain:1,AP domain:0*/
	value = (((DP_DOMAIN_ID&DPMAIF_AWDOMAIN_BIT_MSK)
		<<DPMAIF_AWDOMAIN_BIT_OFT)|
		((DP_DOMAIN_ID&DPMAIF_ARDOMAIN_BIT_MSK)
		<<DPMAIF_ARDOMAIN_BIT_OFT));

	DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_DOMAIN, value);
	/*DPMAIF MD bank setting*/
	value = (((DP_BANK0_ID&DPMAIF_CACHE_BANK0_BIT_MSK)
		<<DPMAIF_CACHE_BANK0_BIT_OFT)|
		((DP_BANK1_ID&DPMAIF_CACHE_BANK1_BIT_MSK)
		<<DPMAIF_CACHE_BANK1_BIT_OFT));

	DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_CACHE, value);
	get_md_resv_mem_info(&md_base_at_ap, NULL, NULL, NULL);
	tmp_val = (unsigned int)md_base_at_ap;

	/*Remap and Remap enable for address*/
	value = ((tmp_val >> 24) & 0xFF) +
				(((tmp_val + 0x2000000) >> 8) & 0xFF0000);
	value |= ((1 << 16) | 1);
	CCCI_BOOTUP_LOG(0, TAG, "[remap]-A-0x%08x\r\n", value);
	DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAPA, value);
	tmp_val += 0x2000000 * 2;
	value = ((tmp_val >> 24) & 0xFF) +
				(((tmp_val + 0x2000000) >> 8) & 0xFF0000);
	value |= ((1 << 16) | 1);
	CCCI_BOOTUP_LOG(0, TAG, "[remap]-B-0x%08x\r\n", value);
	DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAPB, value);
	tmp_val += 0x2000000 * 2;
	value = ((tmp_val >> 24) & 0xFF) +
				(((tmp_val + 0x2000000) >> 8) & 0xFF0000);
	value |= ((1 << 16) | 1);
	CCCI_BOOTUP_LOG(0, TAG, "[remap]-C-0x%08x\r\n", value);
	DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAPC, value);
	tmp_val += 0x2000000 * 2;
	value = ((tmp_val >> 24) & 0xFF) +
				(((tmp_val + 0x2000000) >> 8) & 0xFF0000);
	value |= ((1 << 16) | 1);
	CCCI_BOOTUP_LOG(0, TAG, "[remap]-D-0x%08x\r\n", value);
	DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_BANK0_MAPD, value);

	tmp_val += 0x2000000 * 2;
	value = ((tmp_val >> 24) & 0xFF) +
				(((tmp_val + 0x2000000) >> 8) & 0xFF0000);
	value |= ((1 << 16) | 1);
	CCCI_BOOTUP_LOG(0, TAG, "[remap]-1A-0x%08x\r\n", value);
	DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAPA, value);
	tmp_val += 0x2000000 * 2;
	value = ((tmp_val >> 24) & 0xFF) +
				(((tmp_val + 0x2000000) >> 8) & 0xFF0000);
	value |= ((1 << 16) | 1);
	CCCI_BOOTUP_LOG(0, TAG, "[remap]-1B-0x%08x\r\n", value);
	DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAPB, value);
	tmp_val += 0x2000000 * 2;
	value = ((tmp_val >> 24) & 0xFF) +
				(((tmp_val + 0x2000000) >> 8) & 0xFF0000);
	value |= ((1 << 16) | 1);
	CCCI_BOOTUP_LOG(0, TAG, "[remap]-1C-0x%08x\r\n", value);
	DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAPC, value);
	tmp_val += 0x2000000 * 2;
	value = ((tmp_val >> 24) & 0xFF) +
				(((tmp_val + 0x2000000) >> 8) & 0xFF0000);
	value |= ((1 << 16) | 1);
	CCCI_BOOTUP_LOG(0, TAG, "[remap]-1D-0x%08x\r\n", value);
	DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_BANK1_MAPD, value);

	/*
	 * DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAPA, 0xe300e1);
	 * DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAPB, 0xe700e5);
	 * DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAPC, 0xeB00e9);
	 * DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_REMAP_BANK4_MAPD, 0xeF00eD);
	 */

	/*Enable DPMAIF HW remap*/
	value = DPMA_READ_AO_MD_DL(NRL2_DPMAIF_MISC_AO_CFG1);
	value |= DPMAIF_MD_AO_REMAP_ENABLE;
	DPMA_WRITE_AO_MD_DL(NRL2_DPMAIF_MISC_AO_CFG1, value);
}

static void drv2_common_hw_init(void)
{
	/*Set HW CG dis*/
	DPMA_WRITE_PD_MISC(NRL2_DPMAIF_AP_MISC_CG_EN, 0x7F);

	/*Set Wdma performance*/
	drv_dl_set_wdma();
	drv_md_hw_bus_remap();

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L1TIMR0, ((1<<9)|(1<<10)|(1<<15)|(1<<16)));

/*Set Power on/off flag*/
	DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW, 0xff);
}

/* =======================================================
 *
 * Descriptions: State part (1/3): Init(ISR)
 *
 * ========================================================
 */
static int drv2_intr_hw_init(void)
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

	ccci_drv_set_dl_interrupt_mask(~(AP_DL_L2INTR_En_Msk));

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

void ccci_drv2_dl_set_bid_maxcnt(unsigned int cnt)
{
	unsigned int value;

	/* 1.4 bit31~16: max pkt count in one BAT buffer, curr: 3 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO);

	value &= ~(DPMAIF_BAT_BID_MAXCNT_MSK);
	value |= ((cnt << 16) & DPMAIF_BAT_BID_MAXCNT_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO, value);
}

void ccci_drv2_dl_set_remain_minsz(unsigned int sz)
{
	unsigned int value;

	/* 1.1 bit 15~8: BAT remain size < sz, use next BAT, curr: 64 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO);

	value &= ~(DPMAIF_BAT_REMAIN_MINSZ_MSK);
	value |= (((sz/DPMAIF_BAT_REMAIN_SZ_BASE) << 8) & DPMAIF_BAT_REMAIN_MINSZ_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO, value);
}

void ccci_drv2_dl_set_pkt_align(bool enable, unsigned int mode)
{
	unsigned int value;

	if (mode >= 2)
		return;

	/* 1.5 bit 22: pkt align, curr: 0, 64B align */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);

	value &= ~(DPMAIF_PKT_ALIGN_MSK);

	if (enable == true) {
		value |= DPMAIF_PKT_ALIGN_EN;
		value |= ((mode << 22) & DPMAIF_PKT_ALIGN_MSK);
	}

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

void ccci_drv2_dl_set_mtu(unsigned int mtu_sz)
{
	/* 1. 6 bit 31~0: MTU setting, curr: (3*1024 + 8) = 3080 */
	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON1, mtu_sz);
}

void ccci_drv2_dl_set_pit_chknum(void)
{
	unsigned int value;
	unsigned int number = DPMAIF_HW_CHK_PIT_NUM;

	/* 2.1 bit 31~24: pit threadhold, < number will pit_err_irq, curr: 2 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_PIT_CHK_NUM_MSK);
	value |= ((number << 24) & DPMAIF_PIT_CHK_NUM_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void ccci_drv2_dl_set_chk_rbnum(unsigned int cnt)
{
	unsigned int value;

	/* bit0~7: chk rb pit number */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO);

	value &= ~(DPMAIF_CHK_RB_PITNUM_MSK);
	value |= ((cnt) & DPMAIF_CHK_RB_PITNUM_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO, value);
}

void ccci_drv2_dl_set_performance(void)
{
	unsigned int value;

	/*BAT cache enable*/
	value = DPMA_READ_PD_DL(NRL2_DPMAIF_DL_BAT_INIT_CON1);
	value |= (1<<22);
	DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_BAT_INIT_CON1, value);

	/*PIT burst en*/
	value = DPMA_READ_AO_DL(NRL2_DPMAIF_AO_DL_RDY_CHK_THRES);
	value |= (1<<13);
	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

void ccci_drv2_dl_set_ao_chksum_en(bool enable)
{
	unsigned int value;

	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	value &= ~(DPMAIF_CHKSUM_ON_MSK);

	if (enable == true)
		value |= (DPMAIF_CHKSUM_ON_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

void drv2_dl_set_apit_idx(unsigned int idx)
{
	unsigned int value;

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON3);

	value &= ~((drv.dl_pit_wridx_msk) << 16);
	value |= ((idx & drv.dl_pit_wridx_msk) << 16);

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON3, value);

	/*notify MD idx*/
	DPMA_WRITE_PD_UL(NRL2_DPMAIF_UL_RESERVE_RW, (idx|DPMAIF_MD_DUMMYPIT_EN));
}

void ccci_drv2_rxq_hw_int_apit(struct dpmaif_rx_queue *rxq)
{
	drv2_dl_set_apit_idx(DPMAIF_DUMMY_PIT_AIDX);

	rxq->pit_dummy_idx = DPMAIF_DUMMY_PIT_AIDX;
	rxq->pit_dummy_cnt = 0;
}

void ccci_drv2_rxq_handle_ig(struct dpmaif_rx_queue *rxq,
	struct dpmaif_normal_pit_v2 *nml_pit_v2)
{
	/*resv PIT IG=1 bit*/
	if (nml_pit_v2->reserved2 & (1<<6))
		rxq->pit_reload_en = 1;
	else
		rxq->pit_reload_en = 0;

	rxq->pit_dummy_cnt++;
}

static void drv2_check_dl_fifo_idle(void)
{
	unsigned int reg, pop_idx, push_idx;
	int count = 0;

	while (1) {
		reg = DPMA_READ_PD_DL(DPMAIF_PD_DL_DBG_STA7);

		push_idx = ((reg >> DPMAIF_DL_FIFO_PUSH_SHIFT) & DPMAIF_DL_FIFO_PUSH_MSK);
		pop_idx = ((reg >> DPMAIF_DL_FIFO_POP_SHIFT) & DPMAIF_DL_FIFO_POP_MSK);

		if ((push_idx == pop_idx) &&
			((reg&DPMAIF_DL_FIFO_IDLE_STS) == DPMAIF_DL_FIFO_IDLE_STS))
			break;

		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: DPMAIF_AO_DL_PIT_STA3 failed\n", __func__);
			count = 0;
			return;
		}
	}
	count = 0;
	while (1) {
		if ((DPMA_READ_PD_DL(0x258) & 0x01) == 0x01)
			break;
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: DPMAIF_DMA_WRT poll failed\n", __func__);
			count = 0;
			break;
		}
	}
}

static void drv2_dl_pit_only_update_enable_bit_done(void)
{
	unsigned int dl_pit_init = 0;
	int count = 0;

	dl_pit_init |= DPMAIF_DL_PIT_INIT_ONLY_ENABLE_BIT;
	dl_pit_init |= DPMAIF_DL_PIT_INIT_EN;

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) & DPMAIF_DL_PIT_INIT_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT, dl_pit_init);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: DPMAIF_PD_DL_PIT_INIT ready failed\n", __func__);
			count = 0;
			return;
		}
	}

	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) &
		DPMAIF_DL_PIT_INIT_NOT_READY) == DPMAIF_DL_PIT_INIT_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: DPMAIF_PD_DL_PIT_INIT not ready failed\n", __func__);
			count = 0;
			break;
		}
	}
}

static int drv2_set_dl_idle(bool set_en)
{
	int ret = 0;
	int count = 0, count1 = 0;

	if (set_en == true) {
		while (1) {
			ccci_drv_dl_pit_en(false);
			drv2_dl_pit_only_update_enable_bit_done();
			while (ccci_drv_dl_idle_check() != 0) {
				if (++count >= 1600000) {
					CCCI_MEM_LOG_TAG(0, TAG,
						"[%s] error: 1 ccci_drv_dl_idle_check() fail.\n",
						__func__);
					count = 0;
					ret = HW_REG_CHK_FAIL;
					break;
				}
			}
			count = 0;
			if ((DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA3) & 0x01) == 0) {
				while (ccci_drv_dl_idle_check() != 0) {
					if (++count >= 1600000) {
						CCCI_MEM_LOG_TAG(0, TAG,
						"[%s] error: 2 ccci_drv_dl_idle_check() fail.\n",
						__func__);
						count = 0;
						ret = HW_REG_CHK_FAIL;
						break;
					}
				}
				drv2_check_dl_fifo_idle();
				break;
			}
			if (++count1 >= 1600000) {
				CCCI_ERROR_LOG(0, TAG,
					"[%s] error: DPMAIF_AO_DL_PIT_STA3 failed\n", __func__);
				count1 = 0;
				ret = HW_REG_CHK_FAIL;
				break;
			}
		}
	} else {
		ccci_drv_dl_pit_en(true);
		drv2_dl_pit_only_update_enable_bit_done();
	}

	return ret;
}

static int drv2_dl_add_apit_num(unsigned short ap_entry_cnt)
{
	int count = 0, ret = 0;
	unsigned int ridx = 0, aidx = 0, size = 0, widx = 0;
	unsigned int chk_num, new_aidx;
	unsigned int dl_pit_init = 0;
	unsigned long md_cnt = 0;

	/*Diasbale FROCE EN*/
	DPMA_WRITE_PD_MD_MISC(NRL2_DPMAIF_PD_MD_DL_RB_PIT_INIT, 0);

	count = drv2_set_dl_idle(true);
	if (count < 0)
		return count;
	count = 0;

	/*check DL all index*/
	ridx = ((DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2)>>16) & DPMAIF_DL_PIT_WRIDX_MSK);

	widx = (DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2) & DPMAIF_DL_PIT_WRIDX_MSK);

	aidx = ((DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA3)>>16) & DPMAIF_DL_PIT_WRIDX_MSK);

	size = (DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA1) & DPMAIF_DL_PIT_WRIDX_MSK);

	/*check enough pit entry to add for dummy reorder*/
	if (ridx <= aidx)
		chk_num = size - aidx + ridx;
	else
		chk_num = ridx - aidx;

	if ((ap_entry_cnt + DPMAIF_HW_CHK_PIT_NUM) < chk_num) {
		/*cal new aidx*/
		new_aidx = aidx + ap_entry_cnt;
		if (new_aidx >= size)
			new_aidx -= size;
		/*restore all r/w/a/base/size/en */
		DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_PIT_INIT_CON0,
				DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA0));
		DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_PIT_INIT_CON1,
				DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA1));
		DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_PIT_INIT_CON2,
				DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2));
		DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_PIT_INIT_CON3,
				((new_aidx & DPMAIF_DL_PIT_WRIDX_MSK) << 16)|DPMAIF_PIT_EN_MSK);

		dl_pit_init |= DPMAIF_DL_PIT_INIT_ALLSET;
		dl_pit_init |= DPMAIF_DL_PIT_INIT_EN;
		dl_pit_init |= ((widx & DPMAIF_DL_PIT_WRIDX_MSK) << 4);

		while (1) {
			if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) & DPMAIF_DL_PIT_INIT_NOT_READY)
					== 0) {
				DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT, dl_pit_init);
				break;
			}
			if (++count >= 1600000) {
				CCCI_ERROR_LOG(0, TAG,
					"[%s] error: DPMAIF_PD_DL_PIT_INIT ready failed\n",
					__func__);
				count = 0;
				return HW_REG_CHK_FAIL;
			}
		}

		while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) & DPMAIF_DL_PIT_INIT_NOT_READY)
				== DPMAIF_DL_PIT_INIT_NOT_READY) {
			if (++count >= 1600000) {
				CCCI_ERROR_LOG(0, TAG,
					"[%s] error: DPMAIF_PD_DL_PIT_INIT not ready failed\n",
					__func__);
				count = 0;
				return HW_REG_CHK_FAIL;
			}
		}

		/*Notify SW update dummt count*/
		md_cnt = DPMA_READ_PD_UL(NRL2_DPMAIF_UL_RESERVE_RW);
		md_cnt &= ~DPMAIF_MD_DUMMYPIT_EN;
		md_cnt += ap_entry_cnt;
		if (md_cnt >= DPMAIF_DUMMY_PIT_MAX_NUM)
			md_cnt -= DPMAIF_DUMMY_PIT_MAX_NUM;
		DPMA_WRITE_PD_UL(NRL2_DPMAIF_UL_RESERVE_RW, (md_cnt|DPMAIF_MD_DUMMYPIT_EN));
		DPMA_WRITE_PD_UL(NRL2_DPMAIF_UL_RESERVE_AO_RW, 0xff);
		/* Notify to MD */
		DPMA_WRITE_PD_MD_MISC(NRL2_DPMAIF_PD_MD_MISC_MD_L1TIMSR0, (1<<0));
		ret = ap_entry_cnt;

	} else
		drv2_set_dl_idle(false);

	/*Enable Force EN*/
	DPMA_WRITE_PD_MD_MISC(NRL2_DPMAIF_PD_MD_DL_RB_PIT_INIT, (1<<7));

	return ret;
}

int ccci_drv2_rxq_update_apit_dummy(struct dpmaif_rx_queue *rxq)
{
	int ret = 0;

	if (rxq->pit_reload_en && rxq->pit_dummy_cnt) {
		ret = drv2_dl_add_apit_num(rxq->pit_dummy_cnt);
		if (ret < 0) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error(%d): update dummy pit fail(128)\n",
				__func__, ret);
			return ret;
		}

		/*reset dummy cnt and update dummy idx*/
		if (ret != 0) {
			rxq->pit_dummy_idx += rxq->pit_dummy_cnt;

			if (rxq->pit_dummy_idx >= DPMAIF_DUMMY_PIT_MAX_NUM)
				rxq->pit_dummy_idx -= DPMAIF_DUMMY_PIT_MAX_NUM;

			rxq->pit_dummy_cnt = 0;
			rxq->pit_reload_en = 0;
			return 0;
		}
	}

	return 0;
}

void ccci_drv2_dl_set_bat_bufsz(unsigned int buf_sz)
{
	unsigned int value;

	/* 1.2 bit 16~8: BAT->buffer size: 128*28 = 3584 unit:? curr: 28 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_BAT_BUF_SZ_MSK);
	value |= (((buf_sz/DPMAIF_BAT_BUFFER_SZ_BASE) << 8) & DPMAIF_BAT_BUF_SZ_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void ccci_drv2_dl_set_bat_rsv_len(unsigned int length)
{
	unsigned int value;

	/* 1.3 bit7~0: BAT buffer reserve length, curr: 88 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_BAT_RSV_LEN_MSK);
	value |= (length & DPMAIF_BAT_RSV_LEN_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void ccci_drv2_dl_set_bat_chk_thres(void)
{
	unsigned int value;

	/* 2.2 bit 21~16: bat threadhold, < size will len_err_irq2, curr: 1 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);

	value &= ~(DPMAIF_BAT_CHECK_THRES_MSK);
	value |= ((DPMAIF_HW_CHK_BAT_NUM << 16) & DPMAIF_BAT_CHECK_THRES_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

void ccci_drv2_dl_set_ao_frg_bat_feature(bool enable)
{
	unsigned int value;

	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	value &= ~(DPMAIF_FRG_BAT_BUF_FEATURE_ON_MSK);

	if (enable == true)
		value |= (DPMAIF_FRG_BAT_BUF_FEATURE_EN &
			DPMAIF_FRG_BAT_BUF_FEATURE_ON_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);
}

void ccci_drv2_dl_set_ao_frg_bat_bufsz(unsigned int buf_sz)
{
	unsigned int value;

	/* 1.2 bit 16~8: BAT->buffer size: 128*28 = 3584 unit:? curr: 28 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);

	value &= ~(DPMAIF_FRG_BAT_BUF_SZ_MSK);
	value |= (((buf_sz/DPMAIF_FRG_BAT_BUFFER_SZ_BASE) << 8) & DPMAIF_FRG_BAT_BUF_SZ_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);
}

void ccci_drv2_dl_set_ao_frag_check_thres(void)
{
	unsigned int value;

	/* 2.2 bit 21~16: bat threadhold, < size will len_err_irq2, curr: 1 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	value &= ~(DPMAIF_FRG_CHECK_THRES_MSK);

	value |= ((DPMAIF_HW_CHK_FRG_NUM) & DPMAIF_FRG_CHECK_THRES_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);
}

unsigned short ccci_drv2_dl_get_bat_ridx(void)
{
	unsigned int ridx = 0;

	ridx = DPMA_READ_AO_DL(DPMAIF_AO_DL_BAT_STA2);
	ridx = (ridx & DPMAIF_DL_BAT_WRIDX_MSK);

	return (unsigned short)ridx;
}

unsigned short ccci_drv2_dl_get_frg_bat_ridx(void)
{
	unsigned int ridx = 0;

	ridx = DPMA_READ_AO_DL(DPMAIF_AO_DL_FRGBAT_STA2);
	ridx = (ridx & DPMAIF_DL_BAT_WRIDX_MSK);

	return (unsigned short)ridx;
}

static void drv2_ul_arb_en(unsigned char q_num, bool enable)
{
	unsigned int ul_arb_en;

	ul_arb_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);

	if (enable == true)
		ul_arb_en |= (1<<(q_num+8));
	else
		ul_arb_en &= ~(1<<(q_num+8));

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_arb_en);
}

static void drv2_ul_update_drb_size(unsigned char q_num, unsigned int size)
{
	unsigned int old_size, set_size;

	/* 1. bit 15~0: DRB count, in word(4 bytes) curr: 512*8/4 */
	set_size = size/4;
	old_size = DPMA_READ_AO_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_size &= ~DPMAIF_DRB_SIZE_MSK;
	old_size |= (set_size & DPMAIF_DRB_SIZE_MSK);

	DPMA_WRITE_AO_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_size);
}

static void drv2_ul_update_drb_base_addr(unsigned char q_num,
	unsigned int lb_addr, unsigned int hb_addr)
{
	unsigned int old_addr;

	/* 2 bit 31~0: drb base addr low 32bits, curr: lb_addr */
	DPMA_WRITE_AO_UL(DPMAIF_ULQSAR_n(q_num), lb_addr);

	old_addr = DPMA_READ_AO_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_addr &= ~DPMAIF_DRB_ADDRH_MSK;
	old_addr |= ((hb_addr<<24) & DPMAIF_DRB_ADDRH_MSK);

	/* 2. bit 31~24: drb base addr high 8bits, curr: hb_addr */
	DPMA_WRITE_AO_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_addr);
}

static void drv2_ul_rdy_en(unsigned char q_num, bool ready)
{
	unsigned int ul_rdy_en;

	ul_rdy_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);

	if (ready == true)
		ul_rdy_en |= (1<<q_num);
	else
		ul_rdy_en &= ~(1<<q_num);

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_rdy_en);
}

static unsigned int drv2_ul_get_rwidx(unsigned char q_num)
{
	return DPMA_READ_AO_UL(DPMAIF_ULQ_STA0_n(q_num));
}

unsigned int drv2_ul_get_rdidx(unsigned char q_num)
{
	return (DPMA_READ_AO_UL(DPMAIF_ULQ_STA0_n(q_num)) >> 16) & 0x0000FFFF;
}

void ccci_drv2_mask_ul_que_interrupt(unsigned char q_num)
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

static inline void drv2_irq_tx_done(unsigned int tx_done_isr)
{
	int i;
	unsigned int intr_ul_que_done;
	struct dpmaif_tx_queue *txq;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		intr_ul_que_done = tx_done_isr & (1 << (i + UL_INT_DONE_OFFSET));
		if (intr_ul_que_done) {
			txq = &dpmaif_ctl->txq[i];

			if (atomic_read(&txq->txq_resume_done)) {
				atomic_set(&txq->txq_resume_done, 0);

				CCCI_NOTICE_LOG(0, TAG,
					"clear txq%d_resume_done: 0x%x, 0x%x, 0x%x\n",
					i, drv2_ul_get_rwidx(0),
					drv2_ul_get_rwidx(1),
					drv2_ul_get_rwidx(3));
			}

			ccci_drv2_mask_ul_que_interrupt(i);

			hrtimer_start(&txq->txq_done_timer,
					ktime_set(0, 500000), HRTIMER_MODE_REL);

		}
	}
}

static void drv2_set_dl_interrupt_mask(unsigned int mask)
{
	unsigned int value;

	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);

	value &= ~(DPMAIF_AO_DL_ISR_MSK);
	value |= ((mask) & DPMAIF_AO_DL_ISR_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

static void drv2_mask_dl_interrupt(void)
{
	unsigned int ui_que_done_mask;

	/* set mask register: bit1s */
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0, DPMAIF_DL_INT_QDONE_MSK);

	ui_que_done_mask = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	ui_que_done_mask |= DPMAIF_DL_INT_QDONE_MSK;

	drv2_set_dl_interrupt_mask(ui_que_done_mask);
	/*check mask sts: should be 1s*/
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
	 * ui_que_done_mask) != ui_que_done_mask);
	 */
}

static unsigned int drv2_dl_get_wridx(unsigned char q_num)
{
	return (DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2) & DPMAIF_DL_PIT_WRIDX_MSK);
}

static void drv2_unmask_dl_interrupt(void)
{
	unsigned int ui_que_done_mask = DPMAIF_DL_INT_QDONE_MSK;

	/* set unmask/clear_mask register: bit0s */
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0, DPMAIF_DL_INT_QDONE_MSK);

	ui_que_done_mask = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	ui_que_done_mask &= ~DPMAIF_DL_INT_QDONE_MSK;

	drv2_set_dl_interrupt_mask(ui_que_done_mask);
	/*check mask sts: should be 0s */
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
	 * ui_que_done_mask) == ui_que_done_mask);
	 */
}

static void drv2_unmask_ul_interrupt(unsigned char q_num)
{
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMCR0,
		(DPMAIF_UL_INT_DONE(q_num) & DPMAIF_UL_INT_QDONE_MSK));

	/*check mask sts*/
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
	 *	ui_que_done_mask) == ui_que_done_mask);
	 */
}

static inline unsigned int drv2_get_dl_interrupt_mask(void)
{
	return DPMA_READ_PD_MISC(NRL2_DPMAIF_AO_UL_APDL_L2TIMR0);
}

static irqreturn_t drv2_isr(int irq, void *data)
{
	unsigned int L2RISAR0 = ccci_drv_get_dl_isr_event();
	unsigned int L2RIMR0  = drv2_get_dl_interrupt_mask();
	unsigned int L2TISAR0 = ccci_drv_get_ul_isr_event();
	unsigned int L2TIMR0  = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMR0);

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
			drv2_irq_tx_done(L2TISAR0 & DPMAIF_UL_INT_QDONE_MSK);
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
			drv2_mask_dl_interrupt();

			/*always start work due to no napi*/
			/*for (i = 0; i < DPMAIF_HW_MAX_DLQ_NUM; i++)*/
			tasklet_hi_schedule(&dpmaif_ctl->rxq[0].rxq_task);
		}
	}

	return IRQ_HANDLED;
}

static bool drv2_dpmaif_check_power_down(void)
{
	if (DPMA_READ_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW) == 0) {
		DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW, 0xFF);
		return true;
	}

	return false;
}

static int drv2_suspend_noirq(struct device *dev)
{
	return 0;
}

static inline int drv2_dl_restore(unsigned int mask)
{
	int count = 0;

	/*Set DL/RX interrupt*/
	/* 2. clear interrupt enable mask*/
	/*DPMAIF_WriteReg32(DPMAIF_PD_AP_DL_L2TICR0, AP_DL_L2INTR_En_Msk);*/
	/*set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0, (mask));

	ccci_drv_set_dl_interrupt_mask(mask);

	/* 3. check mask sts*/
	while ((DPMA_READ_PD_MISC(NRL2_DPMAIF_AO_UL_APDL_L2TIMR0) & mask) != mask) {
		count++;
		if (count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: ckeck mask sts fail\n", __func__);
			return HW_REG_TIME_OUT;
		}
	}

	/*Restore MD idx*/
	DPMA_WRITE_PD_UL(NRL2_DPMAIF_UL_RESERVE_RW,
		((dpmaif_ctl->rxq[0].pit_dummy_idx)|DPMAIF_MD_DUMMYPIT_EN));

	return 0;
}

static inline void drv2_init_ul_intr(void)
{
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, 0xFFFFFFFF);

	/* 2. set interrupt enable mask*/
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMCR0, AP_UL_L2INTR_En_Msk);
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMSR0, ~(AP_UL_L2INTR_En_Msk));
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_IP_BUSY, 0xFFFFFFFF);
	/* 2. set IP busy unmask*/
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_DL_UL_IP_BUSY_MASK, 0);
}

static void drv2_txq_hw_init(struct dpmaif_tx_queue *txq)
{
	unsigned long long base_addr;

	if (txq->started == false) {
		drv2_ul_arb_en(txq->index, false);
		return;
	}

	base_addr = (unsigned long long)txq->drb_phy_addr;

	/* 1. BAT buffer parameters setting */
	drv2_ul_update_drb_size(txq->index,
		txq->drb_cnt * sizeof(struct dpmaif_drb_pd));
	drv2_ul_update_drb_base_addr(txq->index,
		(base_addr&0xFFFFFFFF), ((base_addr>>32)&0xFFFFFFFF));

	drv2_ul_rdy_en(txq->index, true);
	drv2_ul_arb_en(txq->index, true);
}

static int drv2_resume_noirq(struct device *dev)
{
	struct dpmaif_tx_queue *txq = NULL;
	int i, ret = 0;
	unsigned int rel_cnt = 0;

	/*IP power down before and need to restore*/
	CCCI_NORMAL_LOG(0, TAG,
		"[%s] sys_resume need to restore(0x%x, 0x%x, 0x%x)\n",
		__func__, drv2_ul_get_rwidx(0), drv2_ul_get_rwidx(1), drv2_ul_get_rwidx(3));

	/*flush and release UL descriptor*/
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		txq = &dpmaif_ctl->txq[i];

		if (atomic_read(&txq->drb_rd_idx) != atomic_read(&txq->drb_wr_idx)) {
			CCCI_NORMAL_LOG(0, TAG,
				"[%s] txq(%d) warning: md not read all skb: rel/r/w(%d,%d,%d)\n",
				__func__, i, atomic_read(&txq->drb_rel_rd_idx),
				atomic_read(&txq->drb_rd_idx), atomic_read(&txq->drb_wr_idx));
		}

		if (atomic_read(&txq->drb_wr_idx) != atomic_read(&txq->drb_rel_rd_idx)) {
			rel_cnt = get_ringbuf_release_cnt(txq->drb_cnt,
					atomic_read(&txq->drb_rel_rd_idx),
					atomic_read(&txq->drb_wr_idx));
			ccci_dpmaif_txq_release_skb(txq, rel_cnt);
		}

		atomic_set(&txq->drb_rd_idx, 0);
		atomic_set(&txq->drb_wr_idx, 0);
		atomic_set(&txq->drb_rel_rd_idx, 0);
	}

	/* there are some inter for init para. check. */
	/* maybe need changed to drv_dpmaif_intr_hw_init();*/
	ret = drv2_dl_restore(dpmaif_ctl->suspend_reg_int_mask_bak);
	if (ret)
		return ret;

	drv2_init_ul_intr();

	/*flush and release UL descriptor*/
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		txq = &dpmaif_ctl->txq[i];

		drv2_txq_hw_init(txq);
		atomic_set(&txq->txq_resume_done, 1);
	}

	return 0;
}

static void drv2_write_infra_ao_mem_prot(void)
{
	unsigned int reg_value;

	if (!dpmaif_ctl->infra_ao_mem_base) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: iomap infracfg_ao_mem is NULL.\n",
			__func__);
		return;
	}

	reg_value = dpmaif_read32(dpmaif_ctl->infra_ao_mem_base, 0);

	reg_value |= INFRA_PROT_DPMAIF_BIT;

	dpmaif_write32(dpmaif_ctl->infra_ao_mem_base, 0, reg_value);
}

static void drv2_ul_all_queue_en(bool enable)
{
	unsigned long ul_arb_en;

	ul_arb_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);

	if (enable == true)
		ul_arb_en |= DPMAIF_UL_ALL_QUE_ARB_EN;
	else
		ul_arb_en &= ~DPMAIF_UL_ALL_QUE_ARB_EN;

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_arb_en);
}

static unsigned int drv2_ul_idle_check(void)
{
	unsigned long idle_sts;

	idle_sts = ((DPMA_READ_PD_UL(DPMAIF_PD_UL_DBG_STA2) >> DPMAIF_UL_STS_CUR_SHIFT)
		& DPMAIF_UL_IDLE_STS_MSK);

	if (idle_sts == DPMAIF_UL_IDLE_STS)
		return 0;
	else
		return 1;
}

static void drv2_dump_register(int buf_type)
{
	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_ul_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_ul_base + DPMAIF_PD_UL_ADD_DESC,
		DPMAIF_PD_UL_ADD_DESC_CH4 - DPMAIF_PD_UL_ADD_DESC + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_ul_base + DPMAIF_PD_UL_ADD_DESC,
		DPMAIF_PD_UL_ADD_DESC_CH4 - DPMAIF_PD_UL_ADD_DESC + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF ao_ul_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->ao_ul_base + DPMAIF_AO_UL_CHNL0_STA,
		DPMAIF_AO_UL_CH_WEIGHT1 - DPMAIF_AO_UL_CHNL0_STA + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->ao_ul_base + DPMAIF_AO_UL_CHNL0_STA,
		DPMAIF_AO_UL_CH_WEIGHT1 - DPMAIF_AO_UL_CHNL0_STA + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_dl_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_BAT_INIT,
		DPMAIF_PD_DL_DBG_STA14 - DPMAIF_PD_DL_BAT_INIT + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_dl_base + DPMAIF_PD_DL_BAT_INIT,
		DPMAIF_PD_DL_DBG_STA14 - DPMAIF_PD_DL_BAT_INIT + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF ao_dl_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->ao_dl_base + DPMAIF_AO_DL_PKTINFO_CONO,
		DPMAIF_AO_DL_REORDER_THRES - DPMAIF_AO_DL_PKTINFO_CONO + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->ao_dl_base + DPMAIF_AO_DL_PKTINFO_CONO,
		DPMAIF_AO_DL_REORDER_THRES - DPMAIF_AO_DL_PKTINFO_CONO + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_dl_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_dl_base + 0x100, 0xC4);
	ccci_util_mem_dump(buf_type,
			dpmaif_ctl->pd_dl_base + 0x100, 0xC4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_dl_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_dl_base + 0x200, 0x58 + 4);
	ccci_util_mem_dump(buf_type,
			dpmaif_ctl->pd_dl_base + 0x200, 0x58 + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_misc_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_misc_base + DPMAIF_PD_AP_UL_L2TISAR0,
		DPMAIF_AP_MISC_APB_DBG_SRAM - DPMAIF_PD_AP_UL_L2TISAR0 + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_misc_base + DPMAIF_PD_AP_UL_L2TISAR0,
		DPMAIF_AP_MISC_APB_DBG_SRAM - DPMAIF_PD_AP_UL_L2TISAR0 + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF ao_md_dl_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->ao_md_dl_base + DPMAIF_MISC_AO_CFG0,
		DPMAIF_AXI_MAS_SECURE - DPMAIF_MISC_AO_CFG0 + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->ao_md_dl_base + DPMAIF_MISC_AO_CFG0,
		DPMAIF_AXI_MAS_SECURE - DPMAIF_MISC_AO_CFG0 + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump MD DPMAIF pd_md_misc_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_md_misc_base + DPMAIF_PD_MD_IP_BUSY,
		DPMAIF_PD_MD_IP_BUSY_MASK - DPMAIF_PD_MD_IP_BUSY + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_md_misc_base + DPMAIF_PD_MD_IP_BUSY,
		DPMAIF_PD_MD_IP_BUSY_MASK - DPMAIF_PD_MD_IP_BUSY + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_ul_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_ul_base + 0x540, 0xBC);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_ul_base + 0x540, 0xBC);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump MD DPMAIF pd_md_misc_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_md_misc_base + 0x100, 0xCC);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_md_misc_base + 0x100, 0xCC);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump AP DPMAIF pd_sram_base; register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->pd_sram_base + 0x00, 0x1FF + 4);
	ccci_util_mem_dump(buf_type,
		dpmaif_ctl->pd_sram_base + 0x00, 0x1FF + 4);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump SW CG infra_ao_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->infra_ao_base + SW_CG_2_STA, 0x0F);
	ccci_util_mem_dump(buf_type,
		(void *)dpmaif_ctl->infra_ao_base + SW_CG_2_STA, 0x0F);

	CCCI_BUF_LOG_TAG(0, buf_type, TAG,
		"dump SW CG infra_ao_base register -> (start addr: 0x%llX, len: %d):\n",
		(unsigned long long)dpmaif_ctl->infra_ao_base + SW_CG_3_STA, 0x0F);
	ccci_util_mem_dump(buf_type,
		(void *)dpmaif_ctl->infra_ao_base + SW_CG_3_STA, 0x0F);

}

static void drv2_hw_reset(void)
{
	unsigned int reg_value;

	/* pre- DPMAIF HW reset: bus-protect */
	reg_value = dpmaif_read32(dpmaif_ctl->infra_ao_mem_base, 0);
	reg_value &= ~INFRA_PROT_DPMAIF_BIT;
	dpmaif_write32(dpmaif_ctl->infra_ao_mem_base, 0, reg_value);
	CCCI_BOOTUP_LOG(0, TAG, "%s:set prot:0x%x\n", __func__, reg_value);

	/* DPMAIF HW reset */
	CCCI_BOOTUP_LOG(0, TAG, "%s:rst dpmaif\n", __func__);
	/* reset dpmaif hw: AO Domain */
	reg_value = DPMAIF_AO_RST_MASK;/* so only this bit effective */
	regmap_write(dpmaif_ctl->infra_ao_base, INFRA_RST0_REG_AO, reg_value);

	CCCI_BOOTUP_LOG(0, TAG, "%s:clear reset\n", __func__);
	/* reset dpmaif clr */
	regmap_write(dpmaif_ctl->infra_ao_base, INFRA_RST1_REG_AO, reg_value);
	CCCI_BOOTUP_LOG(0, TAG, "%s:done. reg_value: %x\n", __func__, reg_value);

	/* reset dpmaif hw: PD Domain */
	reg_value = DPMAIF_PD_RST_MASK;
	regmap_write(dpmaif_ctl->infra_ao_base, INFRA_RST0_REG_PD, reg_value);
	CCCI_BOOTUP_LOG(0, TAG, "%s:clear reset\n", __func__);

	/* reset dpmaif clr */
	regmap_write(dpmaif_ctl->infra_ao_base, INFRA_RST1_REG_PD, reg_value);
	CCCI_BOOTUP_LOG(0, TAG, "[%s]:done. reg_value: %x\n", __func__, reg_value);
}

static int drv2_start(void)
{
	drv2_write_infra_ao_mem_prot();
	drv2_common_hw_init();
	return drv2_intr_hw_init();
}

int ccci_dpmaif_drv2_init(void)
{
	dpmaif_ctl->clk_tbs = g_clk_tbs;

	/* for 97 dpmaif new register */
	dpmaif_ctl->ao_md_dl_base = dpmaif_ctl->ao_ul_base + 0x800;
	dpmaif_ctl->pd_rdma_base  = dpmaif_ctl->pd_ul_base + 0x200;
	dpmaif_ctl->pd_wdma_base  = dpmaif_ctl->pd_ul_base + 0x300;

	if (dpmaif_ctl->dl_bat_entry_size == 0)
		dpmaif_ctl->dl_bat_entry_size = DPMAIF_DL_BAT_ENTRY_SIZE;
	dpmaif_ctl->dl_pit_entry_size = dpmaif_ctl->dl_bat_entry_size * 2;
	dpmaif_ctl->dl_pit_byte_size  = DPMAIF_DL_PIT_BYTE_SIZE;

	drv.pit_size_msk = DPMAIF_PIT_SIZE_MSK;
	drv.dl_pit_wridx_msk = DPMAIF_DL_PIT_WRIDX_MSK;
	drv.ul_int_md_not_ready_msk = DPMAIF_UL_INT_MD_NOTREADY_MSK |
				DPMAIF_UL_INT_MD_PWR_NOTREADY_MSK;
	drv.ap_ul_l2intr_err_en_msk = AP_UL_L2INTR_ERR_En_Msk;
	drv.normal_pit_size = sizeof(struct dpmaif_normal_pit_v2);
	drv.ul_int_qdone_msk = DPMAIF_UL_INT_QDONE_MSK;
	drv.dl_idle_sts = DPMAIF_DL_IDLE_STS;

	dpmaif_ctl->rxq[0].rxq_isr = &drv2_isr;
	ops.drv_start = &drv2_start;
	ops.drv_suspend_noirq = drv2_suspend_noirq;
	ops.drv_resume_noirq = drv2_resume_noirq;

	dpmaif_ctl->rxq[0].rxq_drv_unmask_dl_interrupt = &drv2_unmask_dl_interrupt;
	dpmaif_ctl->rxq[0].rxq_drv_dl_add_pit_remain_cnt = &ccci_drv_dl_add_pit_remain_cnt;
	ops.drv_unmask_ul_interrupt = &drv2_unmask_ul_interrupt;
	ops.drv_dl_get_wridx = &drv2_dl_get_wridx;
	ops.drv_ul_get_rwidx = &drv2_ul_get_rwidx;
	ops.drv_ul_get_rdidx = &drv2_ul_get_rdidx;
	ops.drv_ul_all_queue_en = &drv2_ul_all_queue_en;
	ops.drv_ul_idle_check = &drv2_ul_idle_check;
	ops.drv_hw_reset = &drv2_hw_reset;
	ops.drv_check_power_down = &drv2_dpmaif_check_power_down;
	ops.drv_get_dl_interrupt_mask = &drv2_get_dl_interrupt_mask;
	ops.drv_txq_hw_init = &drv2_txq_hw_init;
	ops.drv_dump_register = &drv2_dump_register;

	return 0;
}

