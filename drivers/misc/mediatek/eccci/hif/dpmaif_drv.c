/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "dpmaif_drv.h"
#include "ccci_hif_dpmaif.h"

#define TAG "dpmaif"
/* =======================================================
 *
 * Descriptions: RX part
 *
 * =======================================================
 */
 #if defined(_E1_SB_SW_WORKAROUND_)

static void drv_dpmaif_dl_pit_only_update_enable_bit_done(unsigned char q_num)
{
	unsigned int dl_pit_init = 0;
	int count = 0;

	dl_pit_init |= DPMAIF_DL_PIT_INIT_ONLY_ENABLE_BIT;
	dl_pit_init |= DPMAIF_DL_PIT_INIT_EN;

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) &
			DPMAIF_DL_PIT_INIT_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT, dl_pit_init);
			break;
		}
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"DPMAIF_PD_DL_PIT_INIT ready failed\n");
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, -1);
			count = 0;
			return;
		}
	}

	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) &
		DPMAIF_DL_PIT_INIT_NOT_READY) == DPMAIF_DL_PIT_INIT_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"DPMAIF_PD_DL_PIT_INIT not ready failed\n");
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, -1);
			count = 0;
			break;
		}
	}
}

static void drv_dpmaif_check_dl_fifo_idle(void)
{
	unsigned int reg, pop_idx, push_idx;
	int count = 0;

	while (1) {
		reg = DPMA_READ_PD_DL(DPMAIF_PD_DL_DBG_STA7);

		push_idx = ((reg >> DPMAIF_DL_FIFO_PUSH_SHIFT) &
			DPMAIF_DL_FIFO_PUSH_MSK);
		pop_idx = ((reg >> DPMAIF_DL_FIFO_POP_SHIFT) &
			DPMAIF_DL_FIFO_POP_MSK);

		if ((push_idx == pop_idx) && ((reg&DPMAIF_DL_FIFO_IDLE_STS) ==
			DPMAIF_DL_FIFO_IDLE_STS))
			break;
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"DPMAIF_AO_DL_PIT_STA3 failed\n");
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, -1);
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
				"DPMAIF_DMA_WRT poll failed\n");
			count = 0;
			break;
		}
	}
}

static unsigned int dpmaif_rx_chk_pit_type(unsigned int chk_aidx)
{
	struct dpmaifq_normal_pit *pkt_inf_t =
		(struct dpmaifq_normal_pit *)dpmaif_ctrl->rxq[0].pit_base +
			chk_aidx;

	return pkt_inf_t->packet_type;
}

static int drv_dpmaif_check_dl_awidx(void)
{
	unsigned int aidx, widx, pit_size, res_val;
	unsigned int re_aidx, re_widx, tmp_idx1, tmp_idx2;
	unsigned int chk_aidx;
#ifdef DPMAIF_DEBUG_LOG
	unsigned int re_aidx_1;
#endif

	tmp_idx1 = DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2);
	tmp_idx2 = DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA3);
	pit_size = (DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA1) &
		DPMAIF_PIT_SIZE_MSK);

	widx = (tmp_idx1 & DPMAIF_DL_PIT_WRIDX_MSK);
	aidx = ((tmp_idx2>>16) & DPMAIF_DL_PIT_WRIDX_MSK);
	/*if aidx == widx, not hw error occurred*/
	if (aidx == widx) {

	} else {
#ifdef DPMAIF_DEBUG_LOG
		if (aidx & 0x01) {
			re_aidx_1 = aidx + 1;
			if (re_aidx_1 >= pit_size)
				re_aidx_1 -= pit_size;
			if (re_aidx_1 != widx) {
				CCCI_MEM_LOG_TAG(0, TAG,
					"dpmaif will do adjustment: 0x1021B558 = 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, (0x%x, 0x%x)\n",
					DPMA_READ_PD_DL(0x258),
					DPMA_READ_PD_DL(DPMAIF_PD_DL_DBG_STA0),
					DPMA_READ_PD_DL(DPMAIF_PD_DL_DBG_STA1),
					DPMA_READ_PD_DL(DPMAIF_PD_DL_DBG_STA7),
					DPMA_READ_PD_DL(DPMAIF_PD_DL_DBG_STA14),
					DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2),
					DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA3));
			}
		}
#endif
		/*if PD power down before and restore PIT PD*/
		if (DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON0) == 0) {
			res_val = DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA0);
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON0, res_val);
			res_val = DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA1);
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON1, res_val);
		}

		chk_aidx = aidx;
		if (chk_aidx == 0)
			chk_aidx = pit_size - 1;
		else
			chk_aidx -= 1;

		if (dpmaif_rx_chk_pit_type(chk_aidx) == DES_PT_MSG) {
			re_aidx = aidx + 1;
			if (re_aidx >= pit_size)
				re_aidx -= pit_size;
		} else
			re_aidx = aidx;

		re_widx = re_aidx;
		re_widx = ((tmp_idx1 & 0xFFFF0000) | re_widx);

		CCCI_MEM_LOG_TAG(0, TAG,
			"dpmaif rx adjustment: 0x%x, 0x%x, (0x%x, 0x%x)\n",
			tmp_idx1, tmp_idx2,
			DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2),
			DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA3));

		DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON2, re_widx);
		DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON3, re_aidx<<16);

		drv_dpmaif_dl_pit_init_done(0);

	}
	return 0;
}

static int drv_dpmaif_dl_set_idle(bool set_en)
{
	int ret = 0;
	int count = 0, count1 = 0;

	if (set_en == true) {
		while (1) {
			drv_dpmaif_dl_pit_en(0, false);
			drv_dpmaif_dl_pit_only_update_enable_bit_done(0);
			while (drv_dpmaif_dl_idle_check() != 0) {
				if (++count >= 1600000) {
					CCCI_MEM_LOG_TAG(0, TAG,
						"drv_dpmaif_dl_idle poll\n");
					count = 0;
					ret = HW_REG_CHK_FAIL;
					break;
				}
			}
			count = 0;
			if ((DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA3)&0x01)
				== 0) {
				while (drv_dpmaif_dl_idle_check() != 0) {
					if (++count >= 1600000) {
						CCCI_MEM_LOG_TAG(0, TAG,
						"drv_dpmaif_dl_idle poll\n");
						count = 0;
						ret = HW_REG_CHK_FAIL;
						break;
					}
				}
				drv_dpmaif_check_dl_fifo_idle();
				break;
			}
			if (++count1 >= 1600000) {
				CCCI_ERROR_LOG(0, TAG,
					"DPMAIF_AO_DL_PIT_STA3 failed\n");
				dpmaif_ctrl->ops->dump_status(
					DPMAIF_HIF_ID, DUMP_FLAG_REG, -1);
				count1 = 0;
				ret = HW_REG_CHK_FAIL;
				break;
			}
		}
		ret = drv_dpmaif_check_dl_awidx();
	} else {
		drv_dpmaif_dl_pit_en(0, true);
		drv_dpmaif_dl_pit_only_update_enable_bit_done(0);
	}
	return ret;
}
#endif

#ifdef HW_FRG_FEATURE_ENABLE
unsigned short drv_dpmaif_dl_get_frg_bat_ridx(unsigned char q_num)
{
	unsigned int ridx = 0;

	ridx = DPMA_READ_AO_DL(DPMAIF_AO_DL_FRGBAT_STA2);
	ridx = ((ridx >> 16) & DPMAIF_DL_BAT_WRIDX_MSK);

	return (unsigned short)ridx;
}

int drv_dpmaif_dl_add_frg_bat_cnt(unsigned char q_num,
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
	}

	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_ADD) &
		DPMAIF_DL_ADD_NOT_READY) == DPMAIF_DL_ADD_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"frg update failed, 0x%x\n", DPMA_READ_PD_DL(
				DPMAIF_PD_DL_DBG_STA1));
			break;
		}
	}

	return 0;
}
#endif

unsigned short drv_dpmaif_dl_get_bat_ridx(unsigned char q_num)
{
	unsigned int ridx = 0;

	ridx = DPMA_READ_AO_DL(DPMAIF_AO_DL_BAT_STA2);
	ridx = ((ridx >> 16) & DPMAIF_DL_BAT_WRIDX_MSK);

	return (unsigned short)ridx;
}

int drv_dpmaif_dl_add_bat_cnt(unsigned char q_num,
	unsigned short bat_entry_cnt)
{
	unsigned int dl_bat_update;
	int count = 0;

	dl_bat_update = (bat_entry_cnt & 0xffff);
	dl_bat_update |= DPMAIF_DL_ADD_UPDATE;

#if defined(_E1_SB_SW_WORKAROUND_)
	count = drv_dpmaif_dl_set_idle(true);
	if (count < 0)
		return count;
	count = 0;
#endif
	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_ADD) &
			DPMAIF_DL_ADD_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_ADD, dl_bat_update);
			break;
		}
	}

	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_ADD) &
		DPMAIF_DL_ADD_NOT_READY) == DPMAIF_DL_ADD_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"bat update failed, 0x%x\n", DPMA_READ_PD_DL(
				DPMAIF_PD_DL_DBG_STA1));
			break;
		}
	}
#if defined(_E1_SB_SW_WORKAROUND_)
	drv_dpmaif_dl_set_idle(false);
#endif
	return 0;
}

int drv_dpmaif_dl_add_pit_remain_cnt(unsigned char q_num,
		unsigned short pit_remain_cnt)
{
	unsigned int dl_update;
#if defined(_E1_SB_SW_WORKAROUND_)
	int ret = 0;

	ret = drv_dpmaif_dl_set_idle(true);
	if (ret < 0)
		return ret;
#endif

	dl_update = (pit_remain_cnt & 0x0000ffff);
	dl_update |= DPMAIF_DL_ADD_UPDATE;

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_ADD) &
			DPMAIF_DL_ADD_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_ADD, dl_update);
			break;
		}
	}

	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_ADD) &
		DPMAIF_DL_ADD_NOT_READY) == DPMAIF_DL_ADD_NOT_READY)
		;
#if defined(_E1_SB_SW_WORKAROUND_)
	drv_dpmaif_dl_set_idle(false);
	return ret;
#else
	return 0;
#endif
}

unsigned int  drv_dpmaif_dl_get_wridx(unsigned char q_num)
{
	unsigned int widx;

#ifdef _E1_SB_SW_WORKAROUND_
	widx = DPMA_READ_PD_DL(DPMAIF_PD_DL_STA8);
	widx = (widx >> 16) & DPMAIF_DL_PIT_WRIDX_MSK;

	CCCI_REPEAT_LOG(0, TAG,
		"DPMAIF_AO_DL_PIT_STA2/3/PD8(0x%x/0x%x/0x%x)\n",
		DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2),
		DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA3),
		widx);

#else
	widx = DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2);
	widx &= DPMAIF_DL_PIT_WRIDX_MSK;
#endif

	return widx;
}

#ifdef _E1_SB_SW_WORKAROUND_
unsigned int drv_dpmaif_dl_get_pit_ridx(unsigned char q_num)
{
	unsigned int ridx;

	ridx = ((DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2)>>16) &
		DPMAIF_DL_PIT_WRIDX_MSK);

	return ridx;
}

unsigned int drv_dpmaif_dl_get_bat_wridx(unsigned char q_num)
{
	unsigned int widx;

	widx = DPMA_READ_AO_DL(DPMAIF_AO_DL_BAT_STA2);
	widx = ((widx >> 0) & DPMAIF_DL_BAT_WRIDX_MSK);

	return widx;
}
#endif

void drv_dpmaif_set_dl_interrupt_mask(unsigned int mask)
{
	unsigned int value;

	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);

	value &= ~(DPMAIF_AO_DL_ISR_MSK);
	value |= ((mask) & DPMAIF_AO_DL_ISR_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

void drv_dpmaif_mask_dl_interrupt(unsigned char q_num)
{
#ifndef DPMAIF_NOT_ACCESS_HW
	unsigned int ui_que_done_mask;

	/* set mask register: bit1s */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0, (
#ifdef _E1_SB_SW_WORKAROUND_
		DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num) |
		DPMAIF_DL_INT_PITCNT_LEN_ERR(q_num) |
#endif
		DPMAIF_DL_INT_QDONE_MSK));

	ui_que_done_mask = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	ui_que_done_mask |= DPMAIF_DL_INT_QDONE_MSK;
#ifdef _E1_SB_SW_WORKAROUND_
	/* mask pit+batcnt len err isr */
	ui_que_done_mask |= (DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num) |
		DPMAIF_DL_INT_PITCNT_LEN_ERR(q_num));
#endif
	drv_dpmaif_set_dl_interrupt_mask(ui_que_done_mask);
	/*check mask sts: should be 1s*/
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
	 * ui_que_done_mask) != ui_que_done_mask);
	 */
#endif
}

void drv_dpmaif_unmask_dl_interrupt(unsigned char q_num)
{
	unsigned int ui_que_done_mask = DPMAIF_DL_INT_QDONE_MSK;

	/* set unmask/clear_mask register: bit0s */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMCR0, (
#ifdef _E1_SB_SW_WORKAROUND_
		DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num) |
		DPMAIF_DL_INT_PITCNT_LEN_ERR(q_num) |
#endif
		DPMAIF_DL_INT_QDONE_MSK));

	ui_que_done_mask = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	ui_que_done_mask &= ~DPMAIF_DL_INT_QDONE_MSK;
#ifdef _E1_SB_SW_WORKAROUND_
	/* mask pit+batcnt len err isr */
	ui_que_done_mask &= ~(DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num) |
		DPMAIF_DL_INT_PITCNT_LEN_ERR(q_num));
#endif
	drv_dpmaif_set_dl_interrupt_mask(ui_que_done_mask);
	/*check mask sts: should be 0s */
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
	 * ui_que_done_mask) == ui_que_done_mask);
	 */
}

#ifdef _E1_SB_SW_WORKAROUND_
void drv_dpmaif_unmask_dl_full_intr(unsigned char q_num)
{
	unsigned int ui_que_done_mask = DPMAIF_DL_INT_QDONE_MSK;

	/* set unmask/clear_mask register: bit0s */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMCR0, (
		DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num) |
		DPMAIF_DL_INT_PITCNT_LEN_ERR(q_num)));

	ui_que_done_mask = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	/* mask pit+batcnt len err isr */
	ui_que_done_mask &= ~(DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num) |
		DPMAIF_DL_INT_PITCNT_LEN_ERR(q_num));
	drv_dpmaif_set_dl_interrupt_mask(ui_que_done_mask);
	/*check mask sts: should be 0s */
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
	 * ui_que_done_mask) == ui_que_done_mask);
	 */
}

void dpmaif_mask_pitcnt_len_error_intr(unsigned char q_num)
{
	unsigned int ui_que_done_mask;

	/* mask pit len err isr */
	/* set mask register: bit1s */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0,
		DPMAIF_DL_INT_PITCNT_LEN_ERR(0));

	ui_que_done_mask = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	/* mask pit+batcnt len err isr */
	ui_que_done_mask |= DPMAIF_DL_INT_PITCNT_LEN_ERR(0);
	drv_dpmaif_set_dl_interrupt_mask(ui_que_done_mask);
}

void dpmaif_mask_batcnt_len_error_intr(unsigned char q_num)
{
	unsigned int ui_que_done_mask;

	/* mask pit len err isr */
	/* set mask register: bit1s */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0,
		DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num));

	ui_que_done_mask = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	/* mask pit+batcnt len err isr */
	ui_que_done_mask |= DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num);
	drv_dpmaif_set_dl_interrupt_mask(ui_que_done_mask);
}

#endif

void drv_dpmaif_dl_all_queue_en(bool enable)
{
	unsigned long dl_bat_init = 0;
	unsigned long value;

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
	}
	/*wait HW updating*/
	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
		DPMAIF_DL_BAT_INIT_NOT_READY) == DPMAIF_DL_BAT_INIT_NOT_READY)
		;
}

unsigned int drv_dpmaif_dl_idle_check(void)
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

void drv_dpmaif_mask_ul_que_interrupt(unsigned char q_num)
{
#ifndef DPMAIF_NOT_ACCESS_HW
	unsigned int ui_que_done_mask;

	ui_que_done_mask = DPMAIF_UL_INT_DONE(q_num) & DPMAIF_UL_INT_QDONE_MSK;

	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISR0, ui_que_done_mask);

	/* check mask sts */
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
	 * ui_que_done_mask)
	 * != ui_que_done_mask);
	 */
#endif
}

void drv_dpmaif_unmask_ul_interrupt(unsigned char q_num)
{
	unsigned int ui_que_done_mask;

	ui_que_done_mask = DPMAIF_UL_INT_DONE(q_num) & DPMAIF_UL_INT_QDONE_MSK;
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TICR0, ui_que_done_mask);

	/*check mask sts*/
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
	 *	ui_que_done_mask) == ui_que_done_mask);
	 */
}

unsigned int drv_dpmaif_ul_get_ridx(unsigned char q_num)
{
	unsigned int ridx;

	ridx = DPMA_READ_PD_UL(DPMAIF_ULQ_STA0_n(q_num)) & 0x0000ffff;

	return ridx;
}

int drv_dpmaif_ul_add_wcnt(unsigned char q_num, unsigned short drb_wcnt)
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
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, -1);
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

void drv_dpmaif_clear_ip_busy(void)
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

void drv_dpmaif_dl_bat_init_done(unsigned char q_num, bool frg_en)
{
	unsigned int dl_bat_init = 0;

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
	}

	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
		DPMAIF_DL_BAT_INIT_NOT_READY) == DPMAIF_DL_BAT_INIT_NOT_READY)
		;

}

void drv_dpmaif_dl_pit_init_done(unsigned char q_num)
{
	unsigned int dl_pit_init = 0;

	dl_pit_init |= DPMAIF_DL_PIT_INIT_ALLSET;
	dl_pit_init |= DPMAIF_DL_PIT_INIT_EN;

	while (1) {
		if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) &
			DPMAIF_DL_PIT_INIT_NOT_READY) == 0) {
			DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT, dl_pit_init);
			break;
		}
	}

	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) &
		DPMAIF_DL_PIT_INIT_NOT_READY) == DPMAIF_DL_PIT_INIT_NOT_READY)
	;
}

void drv_dpmaif_dl_set_bat_base_addr(unsigned char q_num,
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

void drv_dpmaif_dl_set_bat_size(unsigned char q_num, unsigned int size)
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

void drv_dpmaif_dl_bat_en(unsigned char q_num, bool enable)
{
	unsigned int value;

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1);

	if (enable == true)
		value |= DPMAIF_BAT_EN_MSK;
	else
		value &= ~DPMAIF_BAT_EN_MSK;

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_BAT_INIT_CON1, value);
}

void drv_dpmaif_dl_set_pit_base_addr(unsigned char q_num,
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

void drv_dpmaif_dl_set_pit_size(unsigned char q_num, unsigned int size)
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

void drv_dpmaif_dl_pit_en(unsigned char q_num, bool enable)
{
	unsigned int value;

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON3);

	if (enable == true)
		value |= DPMAIF_PIT_EN_MSK;
	else
		value &= ~DPMAIF_PIT_EN_MSK;

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON3, value);
}

void drv_dpmaif_dl_set_bid_maxcnt(unsigned char q_num, unsigned int cnt)
{
	unsigned int value;

	/* 1.4 bit31~16: max pkt count in one BAT buffer, curr: 3 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO);

	value &= ~(DPMAIF_BAT_BID_MAXCNT_MSK);
	value |= ((cnt << 16) & DPMAIF_BAT_BID_MAXCNT_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO, value);
}

void drv_dpmaif_dl_set_remain_minsz(unsigned char q_num, unsigned int sz)
{
	unsigned int value;

	/* 1.1 bit 15~8: BAT remain size < sz, use next BAT, curr: 64 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO);

	value &= ~(DPMAIF_BAT_REMAIN_MINSZ_MSK);
	value |= (((sz/DPMAIF_BAT_REMAIN_SZ_BASE) << 8) &
		DPMAIF_BAT_REMAIN_MINSZ_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO, value);
}

void drv_dpmaif_dl_set_mtu(unsigned int mtu_sz)
{
	/* 1. 6 bit 31~0: MTU setting, curr: (3*1024 + 8) = 3080 */
	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON1, mtu_sz);
}


void drv_dpmaif_dl_set_pit_chknum(unsigned char q_num, unsigned int number)
{
	unsigned int value;

	/* 2.1 bit 31~24: pit threadhold, < number will pit_err_irq, curr: 2 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_PIT_CHK_NUM_MSK);
	value |= ((number << 24) & DPMAIF_PIT_CHK_NUM_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void drv_dpmaif_dl_set_bat_bufsz(unsigned char q_num, unsigned int buf_sz)
{
	unsigned int value;

	/* 1.2 bit 16~8: BAT->buffer size: 128*28 = 3584 unit:? curr: 28 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_BAT_BUF_SZ_MSK);
	value |= (((buf_sz/DPMAIF_BAT_BUFFER_SZ_BASE) << 8) &
		DPMAIF_BAT_BUF_SZ_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void drv_dpmaif_dl_set_bat_rsv_len(unsigned char q_num, unsigned int length)
{
	unsigned int value;

	/* 1.3 bit7~0: BAT buffer reserve length, curr: 88 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2);

	value &= ~(DPMAIF_BAT_RSV_LEN_MSK);
	value |= (length & DPMAIF_BAT_RSV_LEN_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CON2, value);
}

void drv_dpmaif_dl_set_pkt_align(unsigned char q_num, bool enable,
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


void drv_dpmaif_dl_set_bat_chk_thres(unsigned char q_num, unsigned int size)
{
	unsigned int value;

	/* 2.2 bit 21~16: bat threadhold, < size will len_err_irq2, curr: 1 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);

	value &= ~(DPMAIF_BAT_CHECK_THRES_MSK);
	value |= ((size << 16) & DPMAIF_BAT_CHECK_THRES_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}

#ifdef HW_FRG_FEATURE_ENABLE
void drv_dpmaif_dl_set_ao_frag_check_thres(unsigned char q_num,
	unsigned int size)
{
	unsigned int value;

	/* 2.2 bit 21~16: bat threadhold, < size will len_err_irq2, curr: 1 */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	value &= ~(DPMAIF_FRG_CHECK_THRES_MSK);

	value |= ((size) & DPMAIF_FRG_CHECK_THRES_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);
}

void drv_dpmaif_dl_set_ao_frg_bat_feature(unsigned char q_num, bool enable)
{
	unsigned int value;

	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES);
	value &= ~(DPMAIF_FRG_BAT_BUF_FEATURE_ON_MSK);

	if (enable == true)
		value |= (DPMAIF_FRG_BAT_BUF_FEATURE_EN &
			DPMAIF_FRG_BAT_BUF_FEATURE_ON_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_FRG_THRES, value);
}

void drv_dpmaif_dl_set_ao_frg_bat_bufsz(unsigned char q_num,
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

void drv_dpmaif_dl_all_frg_queue_en(bool enable)
{
	unsigned long dl_bat_init = 0;
	unsigned long value;

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
	}
	/*wait HW updating*/
	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_BAT_INIT) &
		DPMAIF_DL_BAT_INIT_NOT_READY) == DPMAIF_DL_BAT_INIT_NOT_READY)
		;
}
#endif

#ifdef HW_CHECK_SUM_ENABLE
void drv_dpmaif_dl_set_ao_chksum_en(unsigned char q_num, bool enable)
{
	unsigned int value;

	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	value &= ~(DPMAIF_CHKSUM_ON_MSK);

	if (enable == true)
		value |= (DPMAIF_CHKSUM_ON_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES, value);
}
#endif

/* =======================================================
 *
 * Descriptions: State part (1/3): Init(RX) -- tx hw init
 *
 * ========================================================
 */

void drv_dpmaif_init_ul_intr(void)
{
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, 0xFFFFFFFF);
	/* 2. set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TICR0, AP_UL_L2INTR_En_Msk);
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISR0, ~(AP_UL_L2INTR_En_Msk));
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_IP_BUSY, 0xFFFFFFFF);
	/* 2. set IP busy unmask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DLUL_IP_BUSY_MASK, 0);
}

#define DPMAIF_UL_DRBSIZE_ADDRH_n(q_num)   \
	((DPMAIF_PD_UL_CHNL0_CON1) + (0x10 * (q_num)))
void drv_dpmaif_ul_update_drb_size(unsigned char q_num, unsigned int size)
{
	unsigned int old_size, set_size;

	/* 1. bit 15~0: DRB count, in word(4 bytes) curr: 512*8/4 */
	set_size = size/4;
	old_size = DPMA_READ_PD_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_size &= ~DPMAIF_DRB_SIZE_MSK;
	old_size |= (set_size & DPMAIF_DRB_SIZE_MSK);

	DPMA_WRITE_PD_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_size);
}

void drv_dpmaif_ul_update_drb_base_addr(unsigned char q_num,
	unsigned int lb_addr, unsigned int hb_addr)
{
	unsigned int old_addr;

	/* 2 bit 31~0: drb base addr low 32bits, curr: lb_addr */
	DPMA_WRITE_PD_UL(DPMAIF_ULQSAR_n(q_num), lb_addr);

	old_addr = DPMA_READ_PD_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_addr &= ~DPMAIF_DRB_ADDRH_MSK;
	old_addr |= ((hb_addr<<24) & DPMAIF_DRB_ADDRH_MSK);
	/* 2. bit 31~24: drb base addr high 8bits, curr: hb_addr */
	DPMA_WRITE_PD_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_addr);
}


void drv_dpmaif_ul_rdy_en(unsigned char q_num, bool ready)
{
	unsigned int ul_rdy_en;

	ul_rdy_en = DPMA_READ_PD_UL(DPMAIF_PD_UL_CHNL_ARB0);

	if (ready == true)
		ul_rdy_en |= (1<<q_num);
	else
		ul_rdy_en &= ~(1<<q_num);

	DPMA_WRITE_PD_UL(DPMAIF_PD_UL_CHNL_ARB0, ul_rdy_en);
}

void drv_dpmaif_ul_arb_en(unsigned char q_num, bool enable)
{
	unsigned int ul_arb_en;

	ul_arb_en = DPMA_READ_PD_UL(DPMAIF_PD_UL_CHNL_ARB0);

	if (enable == true)
		ul_arb_en |= (1<<(q_num+8));
	else
		ul_arb_en &= ~(1<<(q_num+8));

	DPMA_WRITE_PD_UL(DPMAIF_PD_UL_CHNL_ARB0, ul_arb_en);
}

void drv_dpmaif_ul_all_queue_en(bool enable)
{
	unsigned long ul_arb_en;

	ul_arb_en = DPMA_READ_PD_UL(DPMAIF_PD_UL_CHNL_ARB0);

	if (enable == true)
		ul_arb_en |= DPMAIF_UL_ALL_QUE_ARB_EN;
	else
		ul_arb_en &= ~DPMAIF_UL_ALL_QUE_ARB_EN;
	DPMA_WRITE_PD_UL(DPMAIF_PD_UL_CHNL_ARB0, ul_arb_en);
}

unsigned int drv_dpmaif_ul_idle_check(void)
{
	unsigned long idle_sts;
	unsigned int ret;

	idle_sts = ((DPMA_READ_PD_UL(
		DPMAIF_PD_UL_DBG_STA2)>>DPMAIF_UL_STS_CUR_SHIFT) &
			DPMAIF_UL_IDLE_STS_MSK);

	if (idle_sts == DPMAIF_UL_IDLE_STS)
		ret = 1;
	else
		ret = 0;

	return ret;
}

/* =======================================================
 *
 * Descriptions: State part (1/3): Init(ISR)
 *
 * ========================================================
 */
void drv_dpmaif_intr_hw_init(void)
{
	/* UL/TX interrupt init */
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, 0xFFFFFFFF);
	/* 2. set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TICR0, AP_UL_L2INTR_En_Msk);
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISR0, ~(AP_UL_L2INTR_En_Msk));

	/* 3. check mask sts*/
	while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
		AP_UL_L2INTR_En_Msk) == AP_UL_L2INTR_En_Msk)
		;

	/*Set DL/RX interrupt*/
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0, 0xFFFFFFFF);
	/* 2. clear interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMCR0, AP_DL_L2INTR_En_Msk);
	/*set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0, ~(AP_DL_L2INTR_En_Msk));
	drv_dpmaif_set_dl_interrupt_mask(~(AP_DL_L2INTR_En_Msk));
	/* 3. check mask sts*/
	while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
		AP_DL_L2INTR_En_Msk) == AP_DL_L2INTR_En_Msk)
		;

	/* Set AP IP busy */
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_IP_BUSY, 0xFFFFFFFF);
	/* 2. set IP busy unmask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DLUL_IP_BUSY_MASK, 0);
}

/* =======================================================
 *
 * Descriptions: State part (2/3): Resume
 *
 * ========================================================
 */
/* suspend resume */
bool drv_dpmaif_check_power_down(void)
{
#ifdef DPMAIF_NOT_ACCESS_HW
	return false;
#else
	unsigned char ret;
	unsigned int check_value;

	check_value = DPMA_READ_PD_UL(DPMAIF_ULQSAR_n(0));

	if (check_value == 0)
		ret = true;
	else
		ret = false;

	return ret;
#endif
}

void drv_dpmaif_dl_restore(unsigned int mask)
{
#ifdef USE_AO_RESTORE_PD_DL
	unsigned int value;

	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_RDY_CHK_THRES);
	value &= DPMAIF_AO_DL_ISR_MSK;

	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0, (value));
	/* 3. check mask sts: not */
	while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
		value) != value)
		;
#else
	/*Set DL/RX interrupt*/
	/* 2. clear interrupt enable mask*/
	/*DPMAIF_WriteReg32(DPMAIF_PD_AP_DL_L2TICR0, AP_DL_L2INTR_En_Msk);*/
	/*set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0, (mask));
	drv_dpmaif_set_dl_interrupt_mask((mask));
	/* 3. check mask sts*/
	while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
		mask) != mask)
		;
#endif
}

