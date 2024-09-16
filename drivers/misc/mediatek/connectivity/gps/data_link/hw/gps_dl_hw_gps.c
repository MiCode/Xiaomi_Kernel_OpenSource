/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include "gps_dl_config.h"

#include "gps_dl_context.h"
#include "gps_dl_time_tick.h"
#include "gps_dsp_fsm.h"
#include "gps_dl_hw_api.h"
#include "gps_dl_hw_priv_util.h"
#include "gps/bgf_gps_dma.h"
#include "gps/gps_aon_top.h"
#include "gps/gps_usrt_apb.h"
#include "gps/gps_l5_usrt_apb.h"

#define GPS_ADDR_ENTRY_NUM (1)
static const struct gps_dl_addr_map_entry g_gps_addr_table[GPS_ADDR_ENTRY_NUM] = {
	/* Put base list here: */
	/* BGF_GPS_CFG_BASE */
	{0x18C00000, 0x80000000, 0x90000},
};

unsigned int gps_bus_to_host(unsigned int gps_addr)
{
	unsigned int i;
	const struct gps_dl_addr_map_entry *p;

	for (i = 0; i < GPS_ADDR_ENTRY_NUM; i++) {
		p = &g_gps_addr_table[i];
		if (gps_addr >= p->bus_addr &&
			gps_addr < (p->bus_addr + p->length))
			return ((gps_addr - p->bus_addr) + p->host_addr);
	}

	return 0;
}


void gps_dl_hw_set_dma_start(enum gps_dl_hal_dma_ch_index channel,
	struct gdl_hw_dma_transfer *p_transfer)
{
	unsigned int bus_addr_of_data_start;
	unsigned int bus_addr_of_buf_start;
	unsigned int gdl_ret;

	gdl_ret = gps_dl_emi_remap_phy_to_bus_addr(p_transfer->transfer_start_addr, &bus_addr_of_data_start);
	gdl_ret = gps_dl_emi_remap_phy_to_bus_addr(p_transfer->buf_start_addr, &bus_addr_of_buf_start);

	switch (channel) {
	case GPS_DL_DMA_LINK0_A2D:
		if (gps_dl_is_1byte_mode())
			GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA1_CON_ADDR, 0x00128014);
		else
			GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA1_CON_ADDR, 0x00128016);

		GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA1_PGMADDR_PGMADDR_ADDR,
			bus_addr_of_data_start);
		GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA1_WPTO_WPTO_ADDR,
			bus_addr_of_buf_start);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA1_WPPT_WPPT, p_transfer->len_to_wrap);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA1_COUNT_LEN, p_transfer->transfer_max_len);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA1_START_STR, 1);
		break;
	case GPS_DL_DMA_LINK0_D2A:
		if (gps_dl_is_1byte_mode())
			GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA2_CON_ADDR, 0x00078018);
		else
			GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA2_CON_ADDR, 0x0007801A);

		GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA2_PGMADDR_PGMADDR_ADDR,
			bus_addr_of_data_start);
		GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA2_WPTO_WPTO_ADDR,
			bus_addr_of_buf_start);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA2_WPPT_WPPT, p_transfer->len_to_wrap);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA2_COUNT_LEN, p_transfer->transfer_max_len);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA2_START_STR, 1);
		break;
	case GPS_DL_DMA_LINK1_A2D:
		if (gps_dl_is_1byte_mode())
			GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA3_CON_ADDR, 0x00328014);
		else
			GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA3_CON_ADDR, 0x00328016);

		GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA3_PGMADDR_PGMADDR_ADDR,
			bus_addr_of_data_start);
		GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA3_WPTO_WPTO_ADDR,
			bus_addr_of_buf_start);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA3_WPPT_WPPT, p_transfer->len_to_wrap);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA3_COUNT_LEN, p_transfer->transfer_max_len);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA3_START_STR, 1);
		break;
	case GPS_DL_DMA_LINK1_D2A:
		if (gps_dl_is_1byte_mode())
			GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA4_CON_ADDR, 0x00278018);
		else
			GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA4_CON_ADDR, 0x0027801A);

		GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA4_PGMADDR_PGMADDR_ADDR,
			bus_addr_of_data_start);
		GDL_HW_WR_GPS_REG(BGF_GPS_DMA_DMA4_WPTO_WPTO_ADDR,
			bus_addr_of_buf_start);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA4_WPPT_WPPT, p_transfer->len_to_wrap);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA4_COUNT_LEN, p_transfer->transfer_max_len);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA4_START_STR, 1);
		break;
	default:
		return;
	}
}

void gps_dl_hw_set_dma_stop(enum gps_dl_hal_dma_ch_index channel)
{
	/* Poll until DMA IDLE */
	switch (channel) {
	case GPS_DL_DMA_LINK0_A2D:
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA1_START_STR, 0);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA1_ACKINT_ACK, 1);
		break;
	case GPS_DL_DMA_LINK0_D2A:
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA2_START_STR, 0);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA2_ACKINT_ACK, 1);
		break;
	case GPS_DL_DMA_LINK1_A2D:
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA3_START_STR, 0);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA3_ACKINT_ACK, 1);
		break;
	case GPS_DL_DMA_LINK1_D2A:
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA4_START_STR, 0);
		GDL_HW_SET_GPS_ENTRY(BGF_GPS_DMA_DMA4_ACKINT_ACK, 1);
		break;
	default:
		return;
	}
}

bool gps_dl_hw_get_dma_int_status(enum gps_dl_hal_dma_ch_index channel)
{
	/* ASSERT(channel >= 0 && channel <= GPS_DL_DMA_CH_NUM); */
	switch (channel) {
	case GPS_DL_DMA_LINK0_A2D:
		return (bool)GDL_HW_GET_GPS_ENTRY(BGF_GPS_DMA_DMA1_INTSTA_INT);
	case GPS_DL_DMA_LINK0_D2A:
		return (bool)GDL_HW_GET_GPS_ENTRY(BGF_GPS_DMA_DMA2_INTSTA_INT);
	case GPS_DL_DMA_LINK1_A2D:
		return (bool)GDL_HW_GET_GPS_ENTRY(BGF_GPS_DMA_DMA3_INTSTA_INT);
	case GPS_DL_DMA_LINK1_D2A:
		return (bool)GDL_HW_GET_GPS_ENTRY(BGF_GPS_DMA_DMA4_INTSTA_INT);
	default:
		return false;
	}
}

void gps_dl_hw_save_dma_status_struct(
	enum gps_dl_hal_dma_ch_index ch, struct gps_dl_hw_dma_status_struct *p)
{
	unsigned int offset =
		(BGF_GPS_DMA_DMA2_WPPT_ADDR - BGF_GPS_DMA_DMA1_WPPT_ADDR) * ch;

	p->wrap_count       = GDL_HW_RD_GPS_REG(BGF_GPS_DMA_DMA1_WPPT_ADDR + offset);
	p->wrap_to_addr     = GDL_HW_RD_GPS_REG(BGF_GPS_DMA_DMA1_WPTO_ADDR + offset);
	p->total_count      = GDL_HW_RD_GPS_REG(BGF_GPS_DMA_DMA1_COUNT_ADDR + offset);
	p->config           = GDL_HW_RD_GPS_REG(BGF_GPS_DMA_DMA1_CON_ADDR + offset);
	p->start_flag       = GDL_HW_RD_GPS_REG(BGF_GPS_DMA_DMA1_START_ADDR + offset);
	p->intr_flag        = GDL_HW_RD_GPS_REG(BGF_GPS_DMA_DMA1_INTSTA_ADDR + offset);
	p->left_count       = GDL_HW_RD_GPS_REG(BGF_GPS_DMA_DMA1_RLCT_ADDR + offset);
	p->curr_addr        = GDL_HW_RD_GPS_REG(BGF_GPS_DMA_DMA1_PGMADDR_ADDR + offset);
	p->state            = GDL_HW_RD_GPS_REG(BGF_GPS_DMA_DMA1_STATE_ADDR + offset);
}

void gps_dl_hw_print_dma_status_struct(
	enum gps_dl_hal_dma_ch_index ch, struct gps_dl_hw_dma_status_struct *p)
{
	if (!gps_dl_show_reg_wait_log())
		return;

	GDL_LOGW("dma ch %d, wrap = 0x%08x; tra = 0x%08x, count l/w/t = %d/%d/%d, str/int/sta = %d/%d/%d",
		ch, p->wrap_to_addr,
		p->curr_addr, p->left_count, p->wrap_count, p->total_count,
		GDL_HW_EXTRACT_ENTRY(BGF_GPS_DMA_DMA1_START_STR, p->start_flag),
		GDL_HW_EXTRACT_ENTRY(BGF_GPS_DMA_DMA1_INTSTA_INT, p->intr_flag),
		GDL_HW_EXTRACT_ENTRY(BGF_GPS_DMA_DMA1_STATE_STATE, p->state));

	GDL_LOGW("dma ch %d, conf = 0x%08x, master = %d, b2w = %d, w2b = %d, size = %d",
		ch, p->config,
		GDL_HW_EXTRACT_ENTRY(BGF_GPS_DMA_DMA1_CON_MAS, p->config),
		GDL_HW_EXTRACT_ENTRY(BGF_GPS_DMA_DMA1_CON_B2W, p->config),
		GDL_HW_EXTRACT_ENTRY(BGF_GPS_DMA_DMA1_CON_W2B, p->config),
		GDL_HW_EXTRACT_ENTRY(BGF_GPS_DMA_DMA1_CON_SIZE, p->config));
}

enum GDL_RET_STATUS gps_dl_hw_wait_until_dma_complete_and_stop_it(
	enum gps_dl_hal_dma_ch_index ch, int timeout_usec, bool return_if_not_start)
{
	struct gps_dl_hw_dma_status_struct dma_status;
	struct gps_dl_hw_usrt_status_struct usrt_status;
	enum gps_dl_link_id_enum link_id = DMA_CH_TO_LINK_ID(ch);
	bool last_rw_log_on;
	unsigned long tick0, tick1;
	bool conninfra_okay;
	bool do_stop = true;
	enum GDL_RET_STATUS ret = GDL_OKAY;
	int loop_cnt;

	tick0 = gps_dl_tick_get();
	loop_cnt = 0;
	while (1) {
		gps_dl_hw_save_dma_status_struct(ch, &dma_status);
		if (gps_dl_only_show_wait_done_log())
			last_rw_log_on = gps_dl_set_show_reg_rw_log(false);
		else
			gps_dl_hw_print_dma_status_struct(ch, &dma_status);

		if (GDL_HW_EXTRACT_ENTRY(BGF_GPS_DMA_DMA1_START_STR, dma_status.start_flag)) {
			if (gps_dl_only_show_wait_done_log()) {
				gps_dl_set_show_reg_rw_log(last_rw_log_on);
				gps_dl_hw_print_dma_status_struct(ch, &dma_status);
				gps_dl_hw_save_dma_status_struct(ch, &dma_status);
			}
			break; /* to next while-loop */
		} else if (return_if_not_start) {
			ret = GDL_OKAY;
			do_stop = false;
			goto _end;
		}

		tick1 = gps_dl_tick_get();
		if (timeout_usec > GPS_DL_RW_NO_TIMEOUT && (
			gps_dl_tick_delta_to_usec(tick0, tick1) >= timeout_usec ||
			loop_cnt * GDL_HW_STATUS_POLL_INTERVAL_USEC >= timeout_usec)) {
			ret = GDL_FAIL_TIMEOUT;
			do_stop = false;
			goto _end;
		}

		gps_dl_wait_us(GDL_HW_STATUS_POLL_INTERVAL_USEC);
		loop_cnt++;
	}

	while (1) {
		conninfra_okay = gps_dl_conninfra_is_okay_or_handle_it(NULL, true);
		if (!conninfra_okay) {
			ret = GDL_FAIL_CONN_NOT_OKAY;
			do_stop = false;
			break;
		}

		gps_dl_hw_save_dma_status_struct(ch, &dma_status);
		gps_dl_hw_save_usrt_status_struct(link_id, &usrt_status);
		if (gps_dl_only_show_wait_done_log())
			last_rw_log_on = gps_dl_set_show_reg_rw_log(false);
		else {
			gps_dl_hw_print_dma_status_struct(ch, &dma_status);
			gps_dl_hw_print_usrt_status_struct(link_id, &usrt_status);
		}

		if (GDL_HW_EXTRACT_ENTRY(BGF_GPS_DMA_DMA1_INTSTA_INT, dma_status.intr_flag) &&
			GDL_HW_EXTRACT_ENTRY(BGF_GPS_DMA_DMA1_STATE_STATE, dma_status.state) == 0x01) {
			if (gps_dl_only_show_wait_done_log()) {
				gps_dl_set_show_reg_rw_log(last_rw_log_on);
				gps_dl_hw_print_dma_status_struct(ch, &dma_status);
				gps_dl_hw_save_dma_status_struct(ch, &dma_status);
			}

			/* DMA ready to stop */
			gps_dl_hw_set_dma_stop(ch);
			gps_dl_hw_save_dma_status_struct(ch, &dma_status);
			gps_dl_hw_print_dma_status_struct(ch, &dma_status);
			ret = GDL_OKAY;
			do_stop = true;
			break;
		}

		tick1 = gps_dl_tick_get();
		if (timeout_usec > GPS_DL_RW_NO_TIMEOUT && (
			gps_dl_tick_delta_to_usec(tick0, tick1) >= timeout_usec ||
			loop_cnt * GDL_HW_STATUS_POLL_INTERVAL_USEC >= timeout_usec)) {
			ret = GDL_FAIL_TIMEOUT;
			do_stop = false;
			break;
		}

		gps_dl_wait_us(GDL_HW_STATUS_POLL_INTERVAL_USEC);
		loop_cnt++;
	}

_end:
	tick1 = gps_dl_tick_get();
	GDL_LOGW("ch = %d, d_us = %d, do_stop = %d, ret = %s",
		ch, gps_dl_tick_delta_to_usec(tick0, tick1), do_stop, gdl_ret_to_name(ret));
	return ret;
}

unsigned int gps_dl_hw_get_dma_left_len(enum gps_dl_hal_dma_ch_index channel)
{
	/* ASSERT(channel >= 0 && channel <= GPS_DL_DMA_CH_NUM); */
	switch (channel) {
	case GPS_DL_DMA_LINK0_A2D:
		return GDL_HW_GET_GPS_ENTRY(BGF_GPS_DMA_DMA1_RLCT_RLCT);
	case GPS_DL_DMA_LINK0_D2A:
		return GDL_HW_GET_GPS_ENTRY(BGF_GPS_DMA_DMA2_RLCT_RLCT);
	case GPS_DL_DMA_LINK1_A2D:
		return GDL_HW_GET_GPS_ENTRY(BGF_GPS_DMA_DMA3_RLCT_RLCT);
	case GPS_DL_DMA_LINK1_D2A:
		return GDL_HW_GET_GPS_ENTRY(BGF_GPS_DMA_DMA4_RLCT_RLCT);
	default:
		return 0;
	}
}

void gps_dl_hw_get_link_status(
	enum gps_dl_link_id_enum link_id, struct gps_dl_hw_link_status_struct *p)
{
	unsigned int reg_val;
	unsigned int offset =
		(GPS_L5_USRT_APB_APB_CTRL_ADDR - GPS_USRT_APB_APB_CTRL_ADDR) * link_id;

	/* todo: link_id error handling */

	if (link_id == GPS_DATA_LINK_ID0) {
		p->tx_dma_done = gps_dl_hw_get_dma_int_status(GPS_DL_DMA_LINK0_A2D);
		p->rx_dma_done = gps_dl_hw_get_dma_int_status(GPS_DL_DMA_LINK0_D2A);
	} else if (link_id == GPS_DATA_LINK_ID1) {
		p->tx_dma_done = gps_dl_hw_get_dma_int_status(GPS_DL_DMA_LINK1_A2D);
		p->rx_dma_done = gps_dl_hw_get_dma_int_status(GPS_DL_DMA_LINK1_D2A);
	} else {
		p->tx_dma_done = false;
		p->rx_dma_done = false;
	}

	reg_val = GDL_HW_RD_GPS_REG(GPS_USRT_APB_APB_STA_ADDR + offset);
	p->usrt_has_data = GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_TX_IND, reg_val);
	p->usrt_has_nodata = GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_NODAINTB, reg_val);
}

void gps_dl_hw_save_usrt_status_struct(
	enum gps_dl_link_id_enum link_id, struct gps_dl_hw_usrt_status_struct *p)
{
	unsigned int offset =
		(GPS_L5_USRT_APB_APB_CTRL_ADDR - GPS_USRT_APB_APB_CTRL_ADDR) * link_id;

	p->ctrl_setting = GDL_HW_RD_GPS_REG(GPS_USRT_APB_APB_CTRL_ADDR + offset);
	p->intr_enable = GDL_HW_RD_GPS_REG(GPS_USRT_APB_APB_INTEN_ADDR + offset);
	p->state = GDL_HW_RD_GPS_REG(GPS_USRT_APB_APB_STA_ADDR + offset);

	p->mcub_a2d_flag = GDL_HW_RD_GPS_REG(GPS_USRT_APB_MCUB_A2DF_ADDR + offset);
	p->mcub_d2a_flag = GDL_HW_RD_GPS_REG(GPS_USRT_APB_MCUB_D2AF_ADDR + offset);

	p->mcub_a2d_d0 = GDL_HW_RD_GPS_REG(GPS_USRT_APB_MCU_A2D0_ADDR + offset);
	p->mcub_a2d_d1 = GDL_HW_RD_GPS_REG(GPS_USRT_APB_MCU_A2D1_ADDR + offset);
	p->mcub_d2a_d0 = GDL_HW_RD_GPS_REG(GPS_USRT_APB_MCU_D2A0_ADDR + offset);
	p->mcub_d2a_d1 = GDL_HW_RD_GPS_REG(GPS_USRT_APB_MCU_D2A1_ADDR + offset);
}

void gps_dl_hw_print_usrt_status_struct(
	enum gps_dl_link_id_enum link_id, struct gps_dl_hw_usrt_status_struct *p)
{
	if (!gps_dl_show_reg_wait_log())
		return;

	GDL_LOGXW(link_id, "usrt ctrl = 0x%08x[DMA_EN RX=%d,TX=%d; 1BYTE=%d], intr_en = 0x%08x",
		p->ctrl_setting,
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_CTRL_RX_EN, p->ctrl_setting),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_CTRL_TX_EN, p->ctrl_setting),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_CTRL_BYTEN, p->ctrl_setting),
		p->intr_enable);

	GDL_LOGXW(link_id, "usrt state = 0x%08x, [UOEFS]RX=%d%d%d%d(%d),TX=%d%d%d%d(%d)",
		p->state,
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_RX_UNDR, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_RX_OVF, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_RX_EMP, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_RX_FULL, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_RX_ST, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_TX_UNDR, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_TX_OVF, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_TX_EMP, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_TX_FULL, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_TX_ST, p->state));

	GDL_LOGXW(link_id, "usrt TxReg_em=%d, TX_IND=%d, TX_IND=%d, U_em=%d, NOD_INT=%d",
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_REGE, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_TX_IND, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_TXINTB, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_URAME, p->state),
		GDL_HW_EXTRACT_ENTRY(GPS_USRT_APB_APB_STA_NODAINTB, p->state));

	GDL_LOGXW(link_id, "mcub d2a flag=0x%08x, d0=0x%08x, d1=0x%08x",
		p->mcub_d2a_flag, p->mcub_d2a_d0, p->mcub_d2a_d1);

	GDL_LOGXW(link_id, "mcub a2d flag=0x%08x, d0=0x%08x, d1=0x%08x",
		p->mcub_a2d_flag, p->mcub_a2d_d0, p->mcub_a2d_d1);
}

void gps_dl_hw_switch_dsp_jtag(void)
{
	unsigned int value, value_new;

	value = GDL_HW_RD_GPS_REG(0x80073160);
	value_new = value & 0xFFFFFFFE;
	value_new = value_new | ((~(value & 0x00000001)) & 0x00000001);
	GDL_HW_WR_GPS_REG(0x80073160, value_new);
}

enum GDL_HW_RET gps_dl_hw_get_mcub_info(enum gps_dl_link_id_enum link_id, struct gps_dl_hal_mcub_info *p)
{
	if (p == NULL)
		return E_INV_PARAMS;

	if (link_id == GPS_DATA_LINK_ID0) {
		p->flag = GDL_HW_RD_GPS_REG(GPS_USRT_APB_MCUB_D2AF_ADDR);
		p->dat0 = GDL_HW_RD_GPS_REG(GPS_USRT_APB_MCU_D2A0_ADDR);
		p->dat1 = GDL_HW_RD_GPS_REG(GPS_USRT_APB_MCU_D2A1_ADDR);
		return HW_OKAY;
	} else if (link_id == GPS_DATA_LINK_ID1) {
		p->flag = GDL_HW_RD_GPS_REG(GPS_L5_USRT_APB_MCUB_D2AF_ADDR);
		p->dat0 = GDL_HW_RD_GPS_REG(GPS_L5_USRT_APB_MCU_D2A0_ADDR);
		p->dat1 = GDL_HW_RD_GPS_REG(GPS_L5_USRT_APB_MCU_D2A1_ADDR);
		return HW_OKAY;
	} else
		return E_INV_PARAMS;
}

void gps_dl_hw_clear_mcub_d2a_flag(enum gps_dl_link_id_enum link_id, unsigned int d2a_flag)
{
	if (link_id == GPS_DATA_LINK_ID0)
		GDL_HW_WR_GPS_REG(GPS_USRT_APB_MCUB_D2AF_ADDR, d2a_flag);
	else if (link_id == GPS_DATA_LINK_ID1)
		GDL_HW_WR_GPS_REG(GPS_L5_USRT_APB_MCUB_D2AF_ADDR, d2a_flag);
}

unsigned int gps_dl_hw_get_mcub_a2d_flag(enum gps_dl_link_id_enum link_id)
{
	unsigned int val = 0;

	if (link_id == GPS_DATA_LINK_ID0)
		val = GDL_HW_RD_GPS_REG(GPS_USRT_APB_MCUB_A2DF_ADDR);
	else if (link_id == GPS_DATA_LINK_ID1)
		val = GDL_HW_RD_GPS_REG(GPS_L5_USRT_APB_MCUB_A2DF_ADDR);

	return val;
}

enum GDL_RET_STATUS gps_dl_hw_mcub_dsp_read_request(enum gps_dl_link_id_enum link_id, u16 dsp_addr)
{
	unsigned int a2d_flag = 0;

	/* Fill addr to A2D0 and trigger A2DF bit2
	 *   (0x04, GPS_MCUB_A2DF_MASK_DSP_REG_READ_REQ),
	 * the result will be put into D2A0 after D2AF bit1
	 *   (0x02, GPS_MCUB_D2AF_MASK_DSP_REG_READ_READY) set.
	 */

	if (link_id == GPS_DATA_LINK_ID0)
		a2d_flag = GDL_HW_RD_GPS_REG(GPS_USRT_APB_MCUB_A2DF_ADDR);
	else if (link_id == GPS_DATA_LINK_ID1)
		a2d_flag = GDL_HW_RD_GPS_REG(GPS_L5_USRT_APB_MCUB_A2DF_ADDR);
	else
		return GDL_FAIL_INVAL;

	/* A2DF bit2 must be cleared, otherwise dsp is busy */
	if ((a2d_flag & GPS_MCUB_A2DF_MASK_DSP_REG_READ_REQ) != 0) {
		GDL_LOGXD(link_id, "a2d_flag = 0x%x, mask = 0x%x, busy",
			a2d_flag, GPS_MCUB_A2DF_MASK_DSP_REG_READ_REQ);
		return GDL_FAIL_BUSY;
	}

	if (link_id == GPS_DATA_LINK_ID0) {
		GDL_HW_WR_GPS_REG(GPS_USRT_APB_MCU_A2D0_ADDR, (dsp_addr & 0xFFFF));
		GDL_HW_WR_GPS_REG(GPS_USRT_APB_MCUB_A2DF_ADDR,
			GPS_MCUB_A2DF_MASK_DSP_REG_READ_REQ);
	} else if (link_id == GPS_DATA_LINK_ID1) {
		GDL_HW_WR_GPS_REG(GPS_L5_USRT_APB_MCU_A2D0_ADDR, (dsp_addr & 0xFFFF));
		GDL_HW_WR_GPS_REG(GPS_L5_USRT_APB_MCUB_A2DF_ADDR,
			GPS_MCUB_A2DF_MASK_DSP_REG_READ_REQ);
	}

	return GDL_OKAY;
}

void gps_dl_hw_print_ms_counter_status(void)
{
	bool show_log;

	show_log = gps_dl_set_show_reg_rw_log(true);
	gps_dl_bus_rd_opt(GPS_DL_GPS_BUS, GPS_AON_TOP_DSLEEP_CTL_ADDR, BMASK_RW_FORCE_PRINT);
	gps_dl_bus_rd_opt(GPS_DL_GPS_BUS, GPS_AON_TOP_WAKEUP_CTL_ADDR, BMASK_RW_FORCE_PRINT);
	gps_dl_bus_rd_opt(GPS_DL_GPS_BUS, GPS_AON_TOP_TCXO_MS_H_ADDR, BMASK_RW_FORCE_PRINT);
	gps_dl_bus_rd_opt(GPS_DL_GPS_BUS, GPS_AON_TOP_TCXO_MS_L_ADDR, BMASK_RW_FORCE_PRINT);
	gps_dl_set_show_reg_rw_log(show_log);
}

