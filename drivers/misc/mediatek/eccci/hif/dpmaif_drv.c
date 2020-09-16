// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
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
			dpmaif_ctrl->ops->dump_status(DPMAIF_HIF_ID,
				DUMP_FLAG_REG, NULL, -1);
			count = 0;
			return;
		}
	}

	while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) &
		DPMAIF_DL_PIT_INIT_NOT_READY) == DPMAIF_DL_PIT_INIT_NOT_READY) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"DPMAIF_PD_DL_PIT_INIT not ready failed\n");
			dpmaif_ctrl->ops->dump_status(DPMAIF_HIF_ID,
				DUMP_FLAG_REG, NULL, -1);
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
			dpmaif_ctrl->ops->dump_status(DPMAIF_HIF_ID,
				DUMP_FLAG_REG, NULL, -1);
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

#if defined(_E1_SB_SW_WORKAROUND_)
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
				dpmaif_ctrl->ops->dump_status(DPMAIF_HIF_ID,
					DUMP_FLAG_REG, NULL, -1);
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
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s cost too long\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
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
	int count = 0;
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

#ifdef MT6297
	/* set mask register: bit1s */
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMSR0, (
#ifdef _E1_SB_SW_WORKAROUND_
		DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num) |
		DPMAIF_DL_INT_PITCNT_LEN_ERR(q_num) |
#endif
		DPMAIF_DL_INT_QDONE_MSK));
#else /*MT6297*/
	/* set mask register: bit1s */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0, (
#ifdef _E1_SB_SW_WORKAROUND_
		DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num) |
		DPMAIF_DL_INT_PITCNT_LEN_ERR(q_num) |
#endif
		DPMAIF_DL_INT_QDONE_MSK));
#endif /*MT6297*/

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

#ifdef MT6297
	/* set unmask/clear_mask register: bit0s */
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_APDL_L2TIMCR0, (
#ifdef _E1_SB_SW_WORKAROUND_
		DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num) |
		DPMAIF_DL_INT_PITCNT_LEN_ERR(q_num) |
#endif
		DPMAIF_DL_INT_QDONE_MSK));
#else /*MT6297*/
	/* set unmask/clear_mask register: bit0s */
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMCR0, (
#ifdef _E1_SB_SW_WORKAROUND_
		DPMAIF_DL_INT_BATCNT_LEN_ERR(q_num) |
		DPMAIF_DL_INT_PITCNT_LEN_ERR(q_num) |
#endif
		DPMAIF_DL_INT_QDONE_MSK));
#endif /*MT6297*/

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

int drv_dpmaif_dl_all_queue_en(bool enable)
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

#ifdef MT6297
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMSR0, ui_que_done_mask);
#else
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISR0, ui_que_done_mask);
#endif

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

#ifdef MT6297
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMCR0, ui_que_done_mask);
#else
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TICR0, ui_que_done_mask);
#endif

	/*check mask sts*/
	/* while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
	 *	ui_que_done_mask) == ui_que_done_mask);
	 */
}

unsigned int drv_dpmaif_ul_get_ridx(unsigned char q_num)
{
	unsigned int ridx;

#ifdef MT6297
	ridx = (DPMA_READ_AO_UL(DPMAIF_ULQ_STA0_n(q_num)) >> 16) & 0x0000ffff;
#else
	ridx = DPMA_READ_PD_UL(DPMAIF_ULQ_STA0_n(q_num)) & 0x0000ffff;
#endif
	return ridx;
}

unsigned int drv_dpmaif_ul_get_rwidx(unsigned char q_num)
{
	unsigned int ridx;

	ridx = DPMA_READ_AO_UL(DPMAIF_ULQ_STA0_n(q_num));

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

int drv_dpmaif_dl_bat_init_done(unsigned char q_num, bool frg_en)
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

void drv_dpmaif_dl_pit_init_done(unsigned char q_num)
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

#ifdef _HW_REORDER_SW_WORKAROUND_
void drv_dpmaif_dl_set_apit_idx(unsigned char q_num, unsigned int idx)
{
	unsigned int value;

	value = DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON3);

	value &= ~((DPMAIF_DL_PIT_WRIDX_MSK) << 16);
	value |= ((idx & DPMAIF_DL_PIT_WRIDX_MSK) << 16);

	DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT_CON3, value);
	/*notify MD idx*/
	DPMA_WRITE_PD_UL(NRL2_DPMAIF_UL_RESERVE_RW,
			(idx|DPMAIF_MD_DUMMYPIT_EN));
}
#endif

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

int drv_dpmaif_dl_all_frg_queue_en(bool enable)
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

#ifdef MT6297
void drv_dpmaif_dl_set_performance(void)
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

void drv_dpmaif_dl_set_wdma(void)
{
	unsigned int value;

	/*Set WDMA OSTD*/
	value = DPMA_READ_WDMA(NRL2_DPMAIF_WDMA_WR_CHNL_CMD_CON3);
	value &=
		~(DPMAIF_DL_WDMA_CTRL_OSTD_MSK<<DPMAIF_DL_WDMA_CTRL_OSTD_OFST);
	value |=
	(DPMAIF_DL_WDMA_CTRL_OSTD_VALUE<<DPMAIF_DL_WDMA_CTRL_OSTD_OFST);
	DPMA_WRITE_WDMA(NRL2_DPMAIF_WDMA_WR_CHNL_CMD_CON3, value);

	/*Set CTRL_INTVAL/INTAL_MIN*/
	value = DPMA_READ_WDMA(NRL2_DPMAIF_WDMA_WR_CMD_CON0);
	value &= (0xFFFF0000);
	DPMA_WRITE_WDMA(NRL2_DPMAIF_WDMA_WR_CMD_CON0, value);
}

void drv_dpmaif_dl_set_chk_rbnum(unsigned char q_num, unsigned int cnt)
{
	unsigned int value;

	/* bit0~7: chk rb pit number */
	value = DPMA_READ_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO);

	value &= ~(DPMAIF_CHK_RB_PITNUM_MSK);
	value |= ((cnt) & DPMAIF_CHK_RB_PITNUM_MSK);

	DPMA_WRITE_AO_DL(DPMAIF_AO_DL_PKTINFO_CONO, value);
}

void drv_dpmaif_common_hw_init(void)
{
	/*Set HW CG dis*/
	DPMA_WRITE_PD_MISC(NRL2_DPMAIF_AP_MISC_CG_EN, 0x7F);
	/*Set Wdma performance*/
	drv_dpmaif_dl_set_wdma();
	drv_dpmaif_md_hw_bus_remap();

	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L1TIMR0,
				((1<<9)|(1<<10)|(1<<15)|(1<<16)));

    /*Set Power on/off flag*/
	DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW, 0xff);
}

void drv_dpmaif_md_hw_bus_remap(void)
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
	get_md_resv_mem_info(MD_SYS1, &md_base_at_ap, NULL, NULL, NULL);
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
#ifdef MT6297
	/* 2. set interrupt enable mask*/
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMCR0, AP_UL_L2INTR_En_Msk);
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_L2TIMSR0, ~(AP_UL_L2INTR_En_Msk));
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_IP_BUSY, 0xFFFFFFFF);
	/* 2. set IP busy unmask*/
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_DL_UL_IP_BUSY_MASK, 0);
#else
	/* 2. set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TICR0, AP_UL_L2INTR_En_Msk);
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISR0, ~(AP_UL_L2INTR_En_Msk));
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_IP_BUSY, 0xFFFFFFFF);
	/* 2. set IP busy unmask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DLUL_IP_BUSY_MASK, 0);
#endif
}

#define DPMAIF_UL_DRBSIZE_ADDRH_n(q_num)   \
	((DPMAIF_PD_UL_CHNL0_CON1) + (0x10 * (q_num)))
void drv_dpmaif_ul_update_drb_size(unsigned char q_num, unsigned int size)
{
	unsigned int old_size, set_size;
#ifdef MT6297
	/* 1. bit 15~0: DRB count, in word(4 bytes) curr: 512*8/4 */
	set_size = size/4;
	old_size = DPMA_READ_AO_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_size &= ~DPMAIF_DRB_SIZE_MSK;
	old_size |= (set_size & DPMAIF_DRB_SIZE_MSK);

	DPMA_WRITE_AO_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_size);
#else
	/* 1. bit 15~0: DRB count, in word(4 bytes) curr: 512*8/4 */
	set_size = size/4;
	old_size = DPMA_READ_PD_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_size &= ~DPMAIF_DRB_SIZE_MSK;
	old_size |= (set_size & DPMAIF_DRB_SIZE_MSK);

	DPMA_WRITE_PD_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_size);
#endif
}

void drv_dpmaif_ul_update_drb_base_addr(unsigned char q_num,
	unsigned int lb_addr, unsigned int hb_addr)
{
	unsigned int old_addr;

#ifdef MT6297
	/* 2 bit 31~0: drb base addr low 32bits, curr: lb_addr */
	DPMA_WRITE_AO_UL(DPMAIF_ULQSAR_n(q_num), lb_addr);

	old_addr = DPMA_READ_AO_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_addr &= ~DPMAIF_DRB_ADDRH_MSK;
	old_addr |= ((hb_addr<<24) & DPMAIF_DRB_ADDRH_MSK);
	/* 2. bit 31~24: drb base addr high 8bits, curr: hb_addr */
	DPMA_WRITE_AO_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_addr);
#else
	/* 2 bit 31~0: drb base addr low 32bits, curr: lb_addr */
	DPMA_WRITE_PD_UL(DPMAIF_ULQSAR_n(q_num), lb_addr);

	old_addr = DPMA_READ_PD_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num));

	old_addr &= ~DPMAIF_DRB_ADDRH_MSK;
	old_addr |= ((hb_addr<<24) & DPMAIF_DRB_ADDRH_MSK);
	/* 2. bit 31~24: drb base addr high 8bits, curr: hb_addr */
	DPMA_WRITE_PD_UL(DPMAIF_UL_DRBSIZE_ADDRH_n(q_num), old_addr);
#endif
}


void drv_dpmaif_ul_rdy_en(unsigned char q_num, bool ready)
{
	unsigned int ul_rdy_en;

#ifdef MT6297
	ul_rdy_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);
#else
	ul_rdy_en = DPMA_READ_PD_UL(DPMAIF_PD_UL_CHNL_ARB0);
#endif

	if (ready == true)
		ul_rdy_en |= (1<<q_num);
	else
		ul_rdy_en &= ~(1<<q_num);

#ifdef MT6297
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_rdy_en);
#else
	DPMA_WRITE_PD_UL(DPMAIF_PD_UL_CHNL_ARB0, ul_rdy_en);
#endif
}

void drv_dpmaif_ul_arb_en(unsigned char q_num, bool enable)
{
	unsigned int ul_arb_en;

#ifdef MT6297
	ul_arb_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);
#else
	ul_arb_en = DPMA_READ_PD_UL(DPMAIF_PD_UL_CHNL_ARB0);
#endif

	if (enable == true)
		ul_arb_en |= (1<<(q_num+8));
	else
		ul_arb_en &= ~(1<<(q_num+8));

#ifdef MT6297
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_arb_en);
#else
	DPMA_WRITE_PD_UL(DPMAIF_PD_UL_CHNL_ARB0, ul_arb_en);
#endif
}

void drv_dpmaif_ul_all_queue_en(bool enable)
{
	unsigned long ul_arb_en;

#ifdef MT6297
	ul_arb_en = DPMA_READ_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0);
#else
	ul_arb_en = DPMA_READ_PD_UL(DPMAIF_PD_UL_CHNL_ARB0);
#endif

	if (enable == true)
		ul_arb_en |= DPMAIF_UL_ALL_QUE_ARB_EN;
	else
		ul_arb_en &= ~DPMAIF_UL_ALL_QUE_ARB_EN;

#ifdef MT6297
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_CHNL_ARB0, ul_arb_en);
#else
	DPMA_WRITE_PD_UL(DPMAIF_PD_UL_CHNL_ARB0, ul_arb_en);
#endif
}

unsigned int drv_dpmaif_ul_idle_check(void)
{
	unsigned long idle_sts;
	unsigned int ret;

	idle_sts = ((DPMA_READ_PD_UL(
		DPMAIF_PD_UL_DBG_STA2)>>DPMAIF_UL_STS_CUR_SHIFT) &
			DPMAIF_UL_IDLE_STS_MSK);

#ifdef MT6297
	if (idle_sts == DPMAIF_UL_IDLE_STS)
		ret = 0;
	else
		ret = 1;
#else
	if (idle_sts == DPMAIF_UL_IDLE_STS)
		ret = 1;
	else
		ret = 0;
#endif
	return ret;
}

/* =======================================================
 *
 * Descriptions: State part (1/3): Init(ISR)
 *
 * ========================================================
 */
int drv_dpmaif_intr_hw_init(void)
{
	int count = 0;
#ifdef MT6297

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
	drv_dpmaif_set_dl_interrupt_mask(~(AP_DL_L2INTR_En_Msk));
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
	DPMA_WRITE_AO_UL(NRL2_DPMAIF_AO_UL_AP_DL_UL_IP_BUSY_MASK, 0);
#else
	/* UL/TX interrupt init */
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, 0xFFFFFFFF);
	/* 2. set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TICR0, AP_UL_L2INTR_En_Msk);
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISR0, ~(AP_UL_L2INTR_En_Msk));

	/* 3. check mask sts*/
	count = 0;
	while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_UL_L2TIMR0) &
		AP_UL_L2INTR_En_Msk) == AP_UL_L2INTR_En_Msk) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s 3rd fail\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	/*Set DL/RX interrupt*/
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0, 0xFFFFFFFF);
	/* 2. clear interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMCR0, AP_DL_L2INTR_En_Msk);
	/*set interrupt enable mask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TIMSR0, ~(AP_DL_L2INTR_En_Msk));
	drv_dpmaif_set_dl_interrupt_mask(~(AP_DL_L2INTR_En_Msk));
	/* 3. check mask sts*/
	count = 0;
	while ((DPMA_READ_PD_MISC(DPMAIF_PD_AP_DL_L2TIMR0) &
		AP_DL_L2INTR_En_Msk) == AP_DL_L2INTR_En_Msk) {
		if (++count >= 1600000) {
			CCCI_ERROR_LOG(0, TAG,
				"%s 4th fail\n", __func__);
			dpmaif_ctrl->ops->dump_status(
				DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
			return HW_REG_TIME_OUT;
		}
	}

	/* Set AP IP busy */
	/* 1. clear dummy sts*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_IP_BUSY, 0xFFFFFFFF);
	/* 2. set IP busy unmask*/
	DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DLUL_IP_BUSY_MASK, 0);
#endif
	return 0;
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


#ifdef MT6297
	check_value = DPMA_READ_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW);
#else
	check_value = DPMA_READ_PD_UL(DPMAIF_ULQSAR_n(0));
#endif

	if (check_value == 0)
		ret = true;
	else
		ret = false;

#ifdef MT6297
	/*re-fill power flag*/
	if (ret == true)
		DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW, 0xff);
#endif

	return ret;
#endif
}

int drv_dpmaif_dl_restore(unsigned int mask)
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
	drv_dpmaif_set_dl_interrupt_mask((mask));
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

#ifdef _HW_REORDER_SW_WORKAROUND_
	/*Restore MD idx*/
	DPMA_WRITE_PD_UL(NRL2_DPMAIF_UL_RESERVE_RW,
		((dpmaif_ctrl->rxq[0].pit_dummy_idx)|DPMAIF_MD_DUMMYPIT_EN));
#endif
	return 0;
}

#ifdef _HW_REORDER_SW_WORKAROUND_
static int drv_dpmaif_set_dl_idle(bool set_en)
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
					DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
				count1 = 0;
				ret = HW_REG_CHK_FAIL;
				break;
			}
		}
	} else {
		drv_dpmaif_dl_pit_en(0, true);
		drv_dpmaif_dl_pit_only_update_enable_bit_done(0);
	}
	return ret;
}

int drv_dpmaif_dl_add_apit_num(unsigned short ap_entry_cnt)
{
	int count = 0, ret = 0;
	unsigned int ridx = 0, aidx = 0, size = 0, widx = 0;
	unsigned int chk_num, new_aidx;
	unsigned int dl_pit_init = 0;
	unsigned long md_cnt = 0;

	/*Diasbale FROCE EN*/
	DPMA_WRITE_MD_MISC_DL(NRL2_DPMAIF_PD_MD_DL_RB_PIT_INIT, 0);

	count = drv_dpmaif_set_dl_idle(true);
	if (count < 0)
		return count;
	count = 0;

	/*check DL all index*/
	ridx = ((DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2)>>16) &
		DPMAIF_DL_PIT_WRIDX_MSK);

	widx = (DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA2) &
		DPMAIF_DL_PIT_WRIDX_MSK);

	aidx = ((DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA3)>>16) &
		DPMAIF_DL_PIT_WRIDX_MSK);

	size = (DPMA_READ_AO_DL(DPMAIF_AO_DL_PIT_STA1) &
		DPMAIF_DL_PIT_WRIDX_MSK);

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
				((new_aidx & DPMAIF_DL_PIT_WRIDX_MSK)
				<< 16)|DPMAIF_PIT_EN_MSK);

		dl_pit_init |= DPMAIF_DL_PIT_INIT_ALLSET;
		dl_pit_init |= DPMAIF_DL_PIT_INIT_EN;
		dl_pit_init |= ((widx & DPMAIF_DL_PIT_WRIDX_MSK) << 4);

		while (1) {
			if ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) &
				DPMAIF_DL_PIT_INIT_NOT_READY) == 0) {
				DPMA_WRITE_PD_DL(DPMAIF_PD_DL_PIT_INIT,
						dl_pit_init);
				break;
			}
			if (++count >= 1600000) {
				CCCI_ERROR_LOG(0, TAG,
					"DPMAIF_PD_DL_PIT_INIT ready failed\n");
				dpmaif_ctrl->ops->dump_status(
					DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
				count = 0;
				ret = HW_REG_CHK_FAIL;
				return ret;
			}
		}

		while ((DPMA_READ_PD_DL(DPMAIF_PD_DL_PIT_INIT) &
			DPMAIF_DL_PIT_INIT_NOT_READY) ==
				DPMAIF_DL_PIT_INIT_NOT_READY) {
			if (++count >= 1600000) {
				CCCI_ERROR_LOG(0, TAG,
					"DPMAIF_PD_DL_PIT_INIT not ready failed\n");
				dpmaif_ctrl->ops->dump_status(
					DPMAIF_HIF_ID, DUMP_FLAG_REG, NULL, -1);
				count = 0;
				ret = HW_REG_CHK_FAIL;
				return ret;
			}
		}

		/*Notify SW update dummt count*/
		md_cnt = DPMA_READ_PD_UL(NRL2_DPMAIF_UL_RESERVE_RW);
		md_cnt &= ~DPMAIF_MD_DUMMYPIT_EN;
		md_cnt += ap_entry_cnt;
		if (md_cnt >= DPMAIF_DUMMY_PIT_MAX_NUM)
			md_cnt -= DPMAIF_DUMMY_PIT_MAX_NUM;
		DPMA_WRITE_PD_UL(NRL2_DPMAIF_UL_RESERVE_RW,
				(md_cnt|DPMAIF_MD_DUMMYPIT_EN));
		DPMA_WRITE_PD_UL(NRL2_DPMAIF_UL_RESERVE_AO_RW, 0xff);
		/* Notify to MD */
		DPMA_WRITE_MD_MISC_DL(NRL2_DPMAIF_PD_MD_MISC_MD_L1TIMSR0,
					(1<<0));
		ret = ap_entry_cnt;
	} else {
		drv_dpmaif_set_dl_idle(false);
		ret = 0;
	}
	/*Enable Force EN*/
	DPMA_WRITE_MD_MISC_DL(NRL2_DPMAIF_PD_MD_DL_RB_PIT_INIT, (1<<7));
	return ret;
}

#endif /*_HW_REORDER_SW_WORKAROUND_*/
