// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "dpmaif_drv_v3.h"
#include "ccci_hif_dpmaif_v3.h"

#define TAG "dpmaif"
/* =======================================================
 *
 * Descriptions: RX part
 *
 * =======================================================
 */

extern struct hif_dpmaif_ctrl *dpmaif_ctrl_v3;
#define dpmaif_ctrl dpmaif_ctrl_v3


#ifdef HW_FRG_FEATURE_ENABLE
unsigned short drv3_dpmaif_dl_get_frg_bat_ridx(unsigned char q_num)
{
	unsigned int ridx = 0;

	ridx = DPMA_READ_AO_DL(DPMAIF_AO_DL_FRGBAT_STA2);
	ridx = (ridx & DPMAIF_DL_BAT_WRIDX_MSK);

	return (unsigned short)ridx;
}

int drv3_dpmaif_dl_add_frg_bat_cnt(unsigned char q_num,
	unsigned short frg_entry_cnt)
{
	unsigned int dl_bat_update;
	int count = 0;

	dl_bat_update = (frg_entry_cnt & 0xffff);
	dl_bat_update |= (DPMAIF_DL_ADD_UPDATE|DPMAIF_DL_BAT_FRG_ADD);

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_ADD) &
			DPMAIF_DL_ADD_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_ADD, dl_bat_update);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s cost too long\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	return 0;
}
#endif

unsigned short drv3_dpmaif_dl_get_bat_ridx(unsigned char q_num)
{
	unsigned int ridx = 0;

	ridx = DPMA_READ_AO_DL(DPMAIF_AO_DL_BAT_STA2);
	ridx = (ridx & DPMAIF_DL_BAT_WRIDX_MSK);

	return (unsigned short)ridx;
}

int drv3_dpmaif_dl_add_bat_cnt(unsigned char q_num,
	unsigned short bat_entry_cnt)
{
	unsigned int dl_bat_update;
	int count = 0;

	dl_bat_update = (bat_entry_cnt & 0xffff);
	dl_bat_update |= DPMAIF_DL_ADD_UPDATE;

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_ADD) &
			DPMAIF_DL_ADD_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_ADD, dl_bat_update);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s cost too long\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	return 0;
}

int drv3_dpmaif_dl_add_pit_remain_cnt(unsigned char q_num,
		unsigned short pit_remain_cnt)
{
	unsigned int dl_update;
	int count = 0;

	dl_update = (pit_remain_cnt & DPMAIF_DL_PIT_REMAIN_CNT_MSK);

	dl_update |= DPMAIF_DL_ADD_UPDATE;

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_ADD) &
			DPMAIF_DL_ADD_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_ADD, dl_update);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"1st DPMAIF_PD_DL_PIT_ADD read fail\n");
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			count = 0;
			return HW_REG_TIME_OUT;
		}
	}
	count = 0;
	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_ADD) &
		DPMAIF_DL_ADD_NOT_READY) == DPMAIF_DL_ADD_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				 "2nd DPMAIF_PD_DL_PIT_ADD read fail\n");
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			count = 0;
			return HW_REG_TIME_OUT;
		}
	}

	return 0;
}

unsigned int  drv3_dpmaif_dl_get_wridx(unsigned char q_num)
{
	unsigned int widx;

	widx = DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA3);
	widx &= DPMAIF_DL_PIT_WRIDX_MSK;

	return widx;
}

void drv3_dpmaif_set_dl_interrupt_mask(unsigned int mask)
{

}

void drv3_dpmaif_mask_dl_interrupt(unsigned char q_num)
{
#ifndef DPMAIF_NOT_ACCESS_HW
	unsigned int ui_que_done_mask;

	/* set mask register: bit1s */
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0, (
		DPMAIF_DL_INT_QDONE_MSK));

	ui_que_done_mask = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	ui_que_done_mask |= DPMAIF_DL_INT_QDONE_MSK;

	drv3_dpmaif_set_dl_interrupt_mask(ui_que_done_mask);
	/*check mask sts: should be 1s*/
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
	 * ui_que_done_mask) != ui_que_done_mask);
	 */
#endif
}

void drv3_dpmaif_unmask_dl_interrupt(unsigned char q_num)
{
	unsigned int ui_que_done_mask = DPMAIF_DL_INT_QDONE_MSK;

	/* set unmask/clear_mask register: bit0s */
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0, (
		DPMAIF_DL_INT_QDONE_MSK));

	ui_que_done_mask = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	ui_que_done_mask &= ~DPMAIF_DL_INT_QDONE_MSK;

	drv3_dpmaif_set_dl_interrupt_mask(ui_que_done_mask);
	/*check mask sts: should be 0s */
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
	 * ui_que_done_mask) == ui_que_done_mask);
	 */
}

int drv3_dpmaif_dl_all_queue_en(bool enable)
{
	unsigned long dl_bat_init = 0;
	unsigned long value;
	int count = 0;

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1);

	if (enable == true)
		value |= DPMAIF_BAT_EN_MSK;
	else
		value &= ~DPMAIF_BAT_EN_MSK;
	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1, value);
	/* only update bat_en bit. */
	dl_bat_init |= DPMAIF_DL_BAT_INIT_ONLY_ENABLE_BIT;
	dl_bat_init |= DPMAIF_DL_BAT_INIT_EN;

	/*update DL bat setting to HW*/
	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
				DPMAIF_DL_BAT_INIT_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT, dl_bat_init);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"1st DPMAIF_PD_DL_BAT_INIT read/write fail\n");
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}
	count = 0;
	/*wait HW updating*/
	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
		DPMAIF_DL_BAT_INIT_NOT_READY) == DPMAIF_DL_BAT_INIT_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
			"2nd DPMAIF_PD_DL_BAT_INIT read fail\n");
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}
	return 0;
}

unsigned int drv3_dpmaif_dl_idle_check(void)
{
	unsigned int ret;

	ret = (DPMA_READ_PD_DL(DPMAIF_PD_DL_DBG_STA1) & DPMAIF_DL_IDLE_STS);

	if (ret == DPMAIF_DL_IDLE_STS)
		ret = 0; /* idle */
	else
		ret = 1;

	return ret;
}
/* =======================================================
 *
 * Descriptions:  TX part
 *
 * ========================================================
 */

void drv3_dpmaif_mask_ul_que_interrupt(unsigned char q_num)
{
#ifndef DPMAIF_NOT_ACCESS_HW
	unsigned int ui_que_done_mask;

	ui_que_done_mask = DPMAIF_UL_INT_DONE(q_num) & DPMAIF_UL_INT_QDONE_MSK;

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMSR0, ui_que_done_mask);

	/* check mask sts */
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
	 * ui_que_done_mask)
	 * != ui_que_done_mask);
	 */
#endif
}

void drv3_dpmaif_unmask_ul_interrupt(unsigned char q_num)
{
	unsigned int ui_que_done_mask;

	ui_que_done_mask = DPMAIF_UL_INT_DONE(q_num) & DPMAIF_UL_INT_QDONE_MSK;

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMCR0, ui_que_done_mask);

	/*check mask sts*/
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
	 *	ui_que_done_mask) == ui_que_done_mask);
	 */
}

unsigned int drv3_dpmaif_ul_get_ridx(unsigned char q_num)
{
	unsigned int ridx;

	ridx = (DPMA_READ_AO_UL_SRAM(DPMAIF_ULQ_STA0_n(q_num)) >> 16) & 0x0000ffff;

	return ridx;
}

unsigned int drv3_dpmaif_ul_get_rwidx(unsigned char q_num)
{
	unsigned int ridx;

	ridx = DPMA_READ_AO_UL_SRAM(DPMAIF_ULQ_STA0_n(q_num));

	return ridx;
}

int drv3_dpmaif_ul_add_wcnt(unsigned char q_num, unsigned short drb_wcnt)
{
	unsigned int ul_update;
	int count = 0;

	ul_update = (drb_wcnt & 0x0000ffff);
	ul_update |= DPMAIF_UL_ADD_UPDATE;

	while (1) {
		if ((DPMA_READ_PD_UL(DPMAIF_ULQ_ADD_DESC_CH_n(q_num)) &
			DPMAIF_UL_ADD_NOT_READY) == 0) {
			DPMA_WRITE_PD_UL(DPMAIF_ULQ_ADD_DESC_CH_n(q_num),
				ul_update);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "drb_add rdy poll fail: 0x%x\n",
				DPMA_READ_PD_UL(DPMAIF_PD_UL_DBG_STA2));
			dpmaif_ctrl->ops->dump_status(DPMAIF_HIF_ID,
				DUMP_FLAG_REG, NULL, -1);
			return HW_REG_CHK_FAIL;
		}
	}
	count = 0;
	while ((DPMA_READ_PD_UL(DPMAIF_ULQ_ADD_DESC_CH_n(q_num)) &
		DPMAIF_UL_ADD_NOT_READY) == DPMAIF_UL_ADD_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG, "drb_add fail: 0x%x\n",
				DPMA_READ_PD_UL(DPMAIF_PD_UL_DBG_STA2));
			break;
		}
	}
	return 0;
}

/* =======================================================
 *
 * Descriptions: ISR part
 *
 * ========================================================
 */
/* definition in dpmaif_drv.h directly. */

void drv3_dpmaif_clear_ip_busy(void)
{
	if (DPMA_READ_PD_MISC(DPMAIF_PD_AP_IP_BUSY))
		DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_IP_BUSY,
			DPMA_READ_PD_MISC(DPMAIF_PD_AP_IP_BUSY));
}

/* =======================================================
 *
 * Descriptions: State part (1/3): Init(RX) -- rx hw init
 *
 * ========================================================
 */

int drv3_dpmaif_dl_bat_init_done(unsigned char q_num, bool frg_en)
{
	unsigned int dl_bat_init = 0;
	int count = 0;

	if (frg_en == true)
		dl_bat_init |= DPMAIF_DL_BAT_FRG_INIT;
	/* update  all bat settings. */
	dl_bat_init |= DPMAIF_DL_BAT_INIT_ALLSET;
	dl_bat_init |= DPMAIF_DL_BAT_INIT_EN;

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
			DPMAIF_DL_BAT_INIT_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT, dl_bat_init);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s 1s fail\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	count = 0;
	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
		DPMAIF_DL_BAT_INIT_NOT_READY) == DPMAIF_DL_BAT_INIT_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s 2nd fail\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}
	return 0;
}

void drv3_dpmaif_dl_pit_init_done(unsigned char q_num)
{
	unsigned int dl_pit_init = 0;
	int count = 0;

	dl_pit_init |= DPMAIF_DL_PIT_INIT_ALLSET;
	dl_pit_init |= DPMAIF_DL_PIT_INIT_EN;

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) &
			DPMAIF_DL_PIT_INIT_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT, dl_pit_init);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s 1st fail\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return;
		}
	}

	count = 0;
	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) &
		DPMAIF_DL_PIT_INIT_NOT_READY) == DPMAIF_DL_PIT_INIT_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s 2nd fail\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return;
		}
	}
}

void drv3_dpmaif_dl_set_bat_base_addr(unsigned char q_num,
	dma_addr_t base_addr)
{
	unsigned int value;

	/* 2.3 bit 31~0: bat base addr low 32bits, curr: lb_addr */
	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON0, (unsigned int)base_addr);

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1);

	value &= ~(DPMAIF_BAT_ADDRH_MSK);
	/* ((base_addr >> 32) << 24) = (base_addr >> 8) */
	value |= ((base_addr >> 8) & DPMAIF_BAT_ADDRH_MSK);
	/* 2.3 bit 31~24: bat base addr high 8bits, curr: hb_addr */
	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1, value);
}

void drv3_dpmaif_dl_set_bat_size(unsigned char q_num, unsigned int size)
{
	unsigned int value;

	/* 2.5 bit 15~0: bat count, unit:8byte one BAT,
	 * curr: DPMAIF_DL_BAT_ENTRY_SIZE 128
	 */
	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1);

	value &= ~(DPMAIF_BAT_SIZE_MSK);
	value |= (size & DPMAIF_BAT_SIZE_MSK);

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1, value);
}

void drv3_dpmaif_dl_bat_en(unsigned char q_num, bool enable)
{
	unsigned int value;

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1);

	if (enable == true)
		value |= DPMAIF_BAT_EN_MSK;
	else
		value &= ~DPMAIF_BAT_EN_MSK;

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1, value);
}

void drv3_dpmaif_dl_set_pit_base_addr(unsigned char q_num,
	dma_addr_t base_addr)
{
	unsigned int value;

	/* 2.4 bit 31~0: pit base addr low 32bits, curr: lb_addr */
	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON0, (unsigned int)base_addr);

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON1);

	value &= ~(DPMAIF_PIT_ADDRH_MSK);
	/* ((base_addr >> 32) << 24) = (base_addr >> 8) */
	value |= ((base_addr >> 8) & DPMAIF_PIT_ADDRH_MSK);
	/* 2.4 bit 31~24: pit base addr high 8bits, curr: hb_addr */
	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON1, value);
}

void drv3_dpmaif_dl_set_pit_size(unsigned char q_num, unsigned int size)
{
	unsigned int value;

	/* 2.6 bit 15~0: pit count, unit: 12byte one pit,
	 * curr: DPMAIF_DL_PIT_ENTRY_SIZE 256
	 */
	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON1);

	value &= ~(DPMAIF_PIT_SIZE_MSK);
	value |= (size & DPMAIF_PIT_SIZE_MSK);

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON1, value);
}

void drv3_dpmaif_dl_pit_en(unsigned char q_num, bool enable)
{
	unsigned int value;

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON3);

	if (enable == true)
		value |= DPMAIF_PIT_EN_MSK;
	else
		value &= ~DPMAIF_PIT_EN_MSK;

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON3, value);
}

void drv3_dpmaif_dl_set_bid_maxcnt(unsigned char q_num, unsigned int cnt)
{
	unsigned int value;

	/* 1.4 bit31~16: max pkt count in one BAT buffer, curr: 3 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO);

	value &= ~(DPMAIF_BAT_BID_MAXCNT_MSK);
	value |= ((cnt << 16) & DPMAIF_BAT_BID_MAXCNT_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO, value);
}

void drv3_dpmaif_dl_set_remain_minsz(unsigned char q_num, unsigned int sz)
{
	unsigned int value;

	/* 1.1 bit 15~8: BAT remain size < sz, use next BAT, curr: 64 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO);

	value &= ~(DPMAIF_BAT_REMAIN_MINSZ_MSK);
	value |= (((sz/DPMAIF_BAT_REMAIN_SZ_BASE) << 8) &
		DPMAIF_BAT_REMAIN_MINSZ_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO, value);
}

void drv3_dpmaif_dl_set_mtu(unsigned int mtu_sz)
{
	/* 1. 6 bit 31~0: MTU setting, curr: (3*1024 + 8) = 3080 */
	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON1, mtu_sz);
}


void drv3_dpmaif_dl_set_pit_chknum(unsigned char q_num, unsigned int number)
{
	unsigned int value;

	/* 2.1 bit 31~24: pit threadhold, < number will pit_err_irq, curr: 2 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_PIT_CHK_NUM_MSK);
	value |= ((number << 24) & DPMAIF_PIT_CHK_NUM_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void drv3_dpmaif_dl_set_bat_bufsz(unsigned char q_num, unsigned int buf_sz)
{
	unsigned int value;

	/* 1.2 bit 16~8: BAT->buffer size: 128*28 = 3584 unit:? curr: 28 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_BAT_BUF_SZ_MSK);
	value |= (((buf_sz/DPMAIF_BAT_BUFFER_SZ_BASE) << 8) &
		DPMAIF_BAT_BUF_SZ_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void drv3_dpmaif_dl_set_bat_rsv_len(unsigned char q_num, unsigned int length)
{
	unsigned int value;

	/* 1.3 bit7~0: BAT buffer reserve length, curr: 88 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_BAT_RSV_LEN_MSK);
	value |= (length & DPMAIF_BAT_RSV_LEN_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void drv3_dpmaif_dl_set_pkt_align(unsigned char q_num, bool enable,
	unsigned int mode)
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


void drv3_dpmaif_dl_set_bat_chk_thres(unsigned char q_num, unsigned int size)
{
	unsigned int value;

	/* 2.2 bit 21~16: bat threadhold, < size will len_err_irq2, curr: 1 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);

	value &= ~(DPMAIF_BAT_CHECK_THRES_MSK);
	value |= ((size << 16) & DPMAIF_BAT_CHECK_THRES_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

#ifdef HW_FRG_FEATURE_ENABLE
void drv3_dpmaif_dl_set_ao_frag_check_thres(unsigned char q_num,
	unsigned int size)
{
	unsigned int value;

	/* 2.2 bit 21~16: bat threadhold, < size will len_err_irq2, curr: 1 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	value &= ~(DPMAIF_FRG_CHECK_THRES_MSK);

	value |= ((size) & DPMAIF_FRG_CHECK_THRES_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);
}

void drv3_dpmaif_dl_set_ao_frg_bat_feature(unsigned char q_num, bool enable)
{
	unsigned int value;

	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	value &= ~(DPMAIF_FRG_BAT_BUF_FEATURE_ON_MSK);

	if (enable == true)
		value |= (DPMAIF_FRG_BAT_BUF_FEATURE_EN &
			DPMAIF_FRG_BAT_BUF_FEATURE_ON_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);
}

void drv3_dpmaif_dl_set_ao_frg_bat_bufsz(unsigned char q_num,
	unsigned int buf_sz)
{
	unsigned int value;

	/* 1.2 bit 16~8: BAT->buffer size: 128*28 = 3584 unit:? curr: 28 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);

	value &= ~(DPMAIF_FRG_BAT_BUF_SZ_MSK);
	value |= (((buf_sz/DPMAIF_FRG_BAT_BUFFER_SZ_BASE) << 8) &
		DPMAIF_FRG_BAT_BUF_SZ_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);

}

int drv3_dpmaif_dl_all_frg_queue_en(bool enable)
{
	unsigned long dl_bat_init = 0;
	unsigned long value;
	int count = 0;

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1);

	if (enable == true)
		value |= DPMAIF_BAT_EN_MSK;
	else
		value &= ~DPMAIF_BAT_EN_MSK;
	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1, value);
	/* only update bat_en bit. */
	dl_bat_init |= DPMAIF_DL_BAT_INIT_ONLY_ENABLE_BIT;
	dl_bat_init |= (DPMAIF_DL_BAT_INIT_EN|DPMAIF_DL_BAT_FRG_INIT);

	/*update DL bat setting to HW*/
	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
				DPMAIF_DL_BAT_INIT_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT, dl_bat_init);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s 1st fail\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}
	count = 0;
	/*wait HW updating*/
	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
		DPMAIF_DL_BAT_INIT_NOT_READY) == DPMAIF_DL_BAT_INIT_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s 2nd fail\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}
	return 0;
}
#endif

#ifdef HW_CHECK_SUM_ENABLE
void drv3_dpmaif_dl_set_ao_chksum_en(unsigned char q_num, bool enable)
{
	unsigned int value;

	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	value &= ~(DPMAIF_CHKSUM_ON_MSK);

	if (enable == true)
		value |= (DPMAIF_CHKSUM_ON_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}
#endif

void drv3_dpmaif_dl_set_performance(void)
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

void drv3_dpmaif_dl_set_chk_rbnum(unsigned char q_num, unsigned int cnt)
{
	unsigned int value;

	/* bit0~7: chk rb pit number */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO);

	value &= ~(DPMAIF_CHK_RB_PITNUM_MSK);
	value |= ((cnt) & DPMAIF_CHK_RB_PITNUM_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO, value);
}

void drv3_dpmaif_sram_init(void)
{
	unsigned int value;

	value = DPMA_READ_PD_MISC(NRL2_DPMAIF_AP_MISC_MEM_CLR);
	value |= DPMAIF_MEM_CLR_MASK;
	DPMA_WRITE_PD_MISC(NRL2_DPMAIF_AP_MISC_MEM_CLR, value);

	while((DPMA_READ_PD_MISC(NRL2_DPMAIF_AP_MISC_MEM_CLR)
			& DPMAIF_MEM_CLR_MASK));
}

void drv3_dpmaif_hw_init_done(void)
{
	unsigned int reg_value = 0;

	/*sync default value to SRAM*/
	reg_value = DPMA_READ_PD_MISC(NRL2_DPMAIF_AP_MISC_OVERWRITE_CFG);
	reg_value |= (DPMAIF_SRAM_SYNC_MASK);
	DPMA_WRITE_PD_MISC(NRL2_DPMAIF_AP_MISC_OVERWRITE_CFG, reg_value);

	/*polling status*/
	while((DPMA_READ_PD_MISC(NRL2_DPMAIF_AP_MISC_OVERWRITE_CFG)&DPMAIF_SRAM_SYNC_MASK));

	/*UL cfg done*/
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_INIT_SET, DPMAIF_UL_INIT_DONE_MASK);
	/*DL cfg done*/
	DPMA_WRITE_AO_DL_NOSRAM(NRL2_DPMAIF_AO_DL_INIT_SET, DPMAIF_DL_INIT_DONE_MASK);
}

void drv3_dpmaif_md_hw_bus_remap(void)
{
	unsigned int value;
	phys_addr_t md_base_at_ap;
	unsigned long long tmp_val;

	get_md_resv_mem_info(MD_SYS1, &md_base_at_ap, NULL, NULL, NULL);
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

void drv3_dpmaif_common_hw_init(void)
{
	drv3_dpmaif_sram_init();

	/*Set HW CG dis*/
	DPMA_WRITE_PD_MISC(NRL2_DPMAIF_AP_MISC_CG_EN, 0x7F);

	drv3_dpmaif_md_hw_bus_remap();

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L1TIMR0,
				((1<<9)|(1<<10)|(1<<15)|(1<<16)));

    /*Set Power on/off flag*/
	DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW, 0xff);
}

void drv3_dpmaif_set_axi_out_gated(void)
{
	unsigned int value;

	value = DPMA_READ_AO_MD_DL(DPMAIF_MISC_AO_MSIC_CFG);
	value |= (1<<1);

	DPMA_WRITE_AO_MD_DL(DPMAIF_MISC_AO_MSIC_CFG, value);
}

void drv3_dpmaif_clr_axi_out_gated(void)
{
	unsigned int value;

	value = DPMA_READ_AO_MD_DL(DPMAIF_MISC_AO_MSIC_CFG);
	value &= ~(1<<1);

	DPMA_WRITE_AO_MD_DL(DPMAIF_MISC_AO_MSIC_CFG, value);
}

/* =======================================================
 *
 * Descriptions: State part (1/3): Init(RX) -- tx hw init
 *
 * ========================================================
 */

void drv3_dpmaif_init_ul_intr(void)
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

#define DPMAIF_UL_DRBSIZE_ADDRH_n(q_num)   \
	((DPMAIF_PD_UL_CHNL0_CON1) + (0x10 * (q_num)))
void drv3_dpmaif_ul_update_drb_size(unsigned char q_num, unsigned int size)
{
	unsigned int old_size, set_size;

	/* 1. bit 15~0: DRB count, in word(4 bytes) curr: 512*8/4 */
	set_size = size/4;
	old_size = DPMA_READ_AO_UL_SRAM(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_size &= ~DPMAIF_DRB_SIZE_MSK;
	old_size |= (set_size & DPMAIF_DRB_SIZE_MSK);

	DPMA_WRITE_AO_UL_SRAM(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_size);

}

void drv3_dpmaif_ul_update_drb_base_addr(unsigned char q_num,
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


void drv3_dpmaif_ul_rdy_en(unsigned char q_num, bool ready)
{
	unsigned int ul_rdy_en;

	ul_rdy_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);

	if (ready == true)
		ul_rdy_en |= (1<<q_num);
	else
		ul_rdy_en &= ~(1<<q_num);

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_rdy_en);

}

void drv3_dpmaif_ul_arb_en(unsigned char q_num, bool enable)
{
	unsigned int ul_arb_en;

	ul_arb_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);

	if (enable == true)
		ul_arb_en |= (1<<(q_num+8));
	else
		ul_arb_en &= ~(1<<(q_num+8));

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_arb_en);

}

void drv3_dpmaif_ul_all_queue_en(bool enable)
{
	unsigned long ul_arb_en;

	ul_arb_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);

	if (enable == true)
		ul_arb_en |= DPMAIF_UL_ALL_QUE_ARB_EN;
	else
		ul_arb_en &= ~DPMAIF_UL_ALL_QUE_ARB_EN;

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_arb_en);
}

unsigned int drv3_dpmaif_ul_idle_check(void)
{
	unsigned long idle_sts;
	unsigned int ret;

	idle_sts = ((DPMA_READ_PD_UL(
		DPMAIF_PD_UL_DBG_STA2)>>DPMAIF_UL_STS_CUR_SHIFT) &
			DPMAIF_UL_IDLE_STS_MSK);

	if (idle_sts == DPMAIF_UL_IDLE_STS)
		ret = 0;
	else
		ret = 1;

	return ret;
}

/* =======================================================
 *
 * Descriptions: State part (1/3): Init(ISR)
 *
 * ========================================================
 */
int drv3_dpmaif_intr_hw_init(void)
{
	int count = 0;

	/* UL/TX interrupt init */
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, 0xFFFFFFFF);
	/* 2. set interrupt enable mask*/
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMCR0, AP_UL_L2INTR_En_Msk);
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMSR0,
				 ~(AP_UL_L2INTR_En_Msk));

	/* 3. check mask sts*/
	while ((DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMR0) &
		AP_UL_L2INTR_En_Msk) == AP_UL_L2INTR_En_Msk) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s 1st fail\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	/*Set DL/RX interrupt*/
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0, 0xFFFFFFFF);
	/* 2. clear interrupt enable mask*/
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0, AP_DL_L2INTR_En_Msk);
	/*set interrupt enable mask*/
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0,
			~(AP_DL_L2INTR_En_Msk));
	drv3_dpmaif_set_dl_interrupt_mask(~(AP_DL_L2INTR_En_Msk));
	/* 3. check mask sts*/
	count = 0;
	while ((DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMR0) &
		AP_DL_L2INTR_En_Msk) == AP_DL_L2INTR_En_Msk) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s 2nd fail\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	/* Set AP IP busy */
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_IP_BUSY, 0xFFFFFFFF);
	/* 2. set IP busy unmask*/
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_DL_UL_IP_BUSY_MASK, 1);

	return 0;
}

/* =======================================================
 *
 * Descriptions: State part (2/3): Resume
 *
 * ========================================================
 */
/* suspend resume */
bool drv3_dpmaif_check_power_down(void)
{
#ifdef DPMAIF_NOT_ACCESS_HW
	return false;
#else
	unsigned char ret;
	unsigned int check_value;

	check_value = DPMA_READ_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW);
	if (check_value == 0)
		ret = true;
	else
		ret = false;

	/*re-fill power flag*/
	if (ret == true)
		DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW, 0xff);

	return ret;
#endif
}

int drv3_dpmaif_dl_restore(unsigned int mask)
{
	int count = 0;
#ifdef USE_AO_RESTORE_PD_DL
	unsigned int value;

	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	value &= DPMAIF_AO_DL_ISR_MSK;

	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0, (value));
	/* 3. check mask sts: not */
	while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
		value) != value) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s fail\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}
#else
	/*Set DL/RX interrupt*/
	/* 2. clear interrupt enable mask*/
	/*DPMAIF_WriteReg32(DPMAIF_PD_AP_DL_L2TICR0, AP_DL_L2INTR_En_Msk);*/
	/*set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0, (mask));
	drv3_dpmaif_set_dl_interrupt_mask((mask));
	/* 3. check mask sts*/
	while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
		mask) != mask) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s fail\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}
#endif

	return 0;
}


