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
#include "gps_dl_hal.h"
#include "gps_dl_hw_api.h"
#include "gps_dsp_fsm.h"
#include "gps_each_link.h"
#include "gps_dl_isr.h"
#include "gps_dl_context.h"
#include "gps_dl_name_list.h"
#include "gps_dl_subsys_reset.h"
#if GPS_DL_HAS_CONNINFRA_DRV
#include "conninfra.h"
#endif

#include "linux/jiffies.h"


static bool s_gps_has_data_irq_masked[GPS_DATA_LINK_NUM];


void gps_dl_hal_event_send(enum gps_dl_hal_event_id evt,
	enum gps_dl_link_id_enum link_id)
{
#if (GPS_DL_HAS_CTRLD == 0)
	gps_dl_hal_event_proc(evt, link_id, gps_each_link_get_session_id(link_id));
#else
	{
		struct gps_dl_osal_lxop *pOp;
		struct gps_dl_osal_signal *pSignal;
		int iRet;

		pOp = gps_dl_get_free_op();
		if (!pOp)
			return;

		pSignal = &pOp->signal;
		pSignal->timeoutValue = 0;/* send data need to wait ?ms */
		if (link_id < GPS_DATA_LINK_NUM) {
			pOp->op.opId = GPS_DL_OPID_HAL_EVENT_PROC;
			pOp->op.au4OpData[0] = link_id;
			pOp->op.au4OpData[1] = evt;
			pOp->op.au4OpData[2] = gps_each_link_get_session_id(link_id);
			iRet = gps_dl_put_act_op(pOp);
		} else {
			gps_dl_put_op_to_free_queue(pOp);
			/*printf error msg*/
			return;
		}
	}
#endif
}

void gps_dl_hal_event_proc(enum gps_dl_hal_event_id evt,
	enum gps_dl_link_id_enum link_id, int sid_on_evt)
{
	struct gps_each_link *p_link = gps_dl_link_get(link_id);
	struct gdl_dma_buf_entry dma_buf_entry;
	enum GDL_RET_STATUS gdl_ret;
	unsigned int write_index;
	int curr_sid;
	bool last_session_msg = false;
	unsigned long j0, j1;
	bool show_log, reg_rw_log;
	bool conninfra_okay, dma_irq_en;

	j0 = jiffies;
	curr_sid = gps_each_link_get_session_id(link_id);

	if (!gps_dl_reset_level_is_none(link_id) ||
		gps_each_link_get_bool_flag(link_id, LINK_IS_RESETTING)) {
		/* ack the reset status */
		last_session_msg = true;
	} else if (sid_on_evt != curr_sid && sid_on_evt != GPS_EACH_LINK_SID_NO_CHECK) {
		GDL_LOGXW_EVT(link_id, "curr_sid = %d, evt = %s, on_sid = %d, not matching",
			curr_sid, gps_dl_hal_event_name(evt), sid_on_evt);
		last_session_msg = true;
	} else if (!gps_each_link_is_active(link_id) ||
		gps_each_link_get_bool_flag(link_id, LINK_TO_BE_CLOSED)) {
		GDL_LOGXW_EVT(link_id, "curr_sid = %d, evt = %s, on_sid = %d, not active",
			curr_sid, gps_dl_hal_event_name(evt), sid_on_evt);
		last_session_msg = true;
	}

	if (last_session_msg) {
		/* unmask irq to make it balance */
		if (evt == GPS_DL_HAL_EVT_D2A_RX_HAS_NODATA) {
			gps_dl_irq_each_link_unmask(link_id,
				GPS_DL_IRQ_TYPE_HAS_NODATA, GPS_DL_IRQ_CTRL_FROM_HAL);
		} else if (evt == GPS_DL_HAL_EVT_D2A_RX_HAS_DATA) {
			gps_dl_irq_each_link_unmask(link_id,
				GPS_DL_IRQ_TYPE_HAS_DATA, GPS_DL_IRQ_CTRL_FROM_HAL);
		} else if (evt == GPS_DL_HAL_EVT_MCUB_HAS_IRQ) {
			gps_dl_irq_each_link_unmask(link_id,
				GPS_DL_IRQ_TYPE_MCUB, GPS_DL_IRQ_CTRL_FROM_HAL);
		} else if (evt == GPS_DL_HAL_EVT_DMA_ISR_PENDING) {
			/*
			 * do nothing if last_session_msg
			 */
		}
		return;
	}

	if (sid_on_evt == GPS_EACH_LINK_SID_NO_CHECK) {
		GDL_LOGXW_EVT(link_id, "curr_sid = %d, evt = %s, on_sid = %d, no check",
			curr_sid, gps_dl_hal_event_name(evt), sid_on_evt);
	} else if (sid_on_evt <= 0 || sid_on_evt > GPS_EACH_LINK_SID_MAX) {
		GDL_LOGXW_EVT(link_id, "curr_sid = %d, evt = %s, on_sid = %d, out of range",
			curr_sid, gps_dl_hal_event_name(evt), sid_on_evt);
	} else {
		GDL_LOGXD_EVT(link_id, "curr_sid = %d, evt = %s, on_sid = %d",
			curr_sid, gps_dl_hal_event_name(evt), sid_on_evt);
	}

	GDL_LOGXD_EVT(link_id, "evt = %s", gps_dl_hal_event_name(evt));
	switch (evt) {
	case GPS_DL_HAL_EVT_D2A_RX_HAS_DATA:
		gdl_ret = gdl_dma_buf_get_free_entry(
			&p_link->rx_dma_buf, &dma_buf_entry, true);

		s_gps_has_data_irq_masked[link_id] = true;
		if (gdl_ret == GDL_OKAY) {
			gps_dl_hal_d2a_rx_dma_claim_emi_usage(link_id, true);
			gps_dl_hal_d2a_rx_dma_start(link_id, &dma_buf_entry);
		} else {

			/* TODO: has pending rx: GDL_FAIL_NOSPACE_PENDING_RX */
			GDL_LOGXI_DRW(link_id, "rx dma not start due to %s", gdl_ret_to_name(gdl_ret));
		}
		break;

	/* TODO: handle the case data_len is just equal to buf_len, */
	/* the rx_dma_done and usrt_has_nodata both happen. */
	case GPS_DL_HAL_EVT_D2A_RX_DMA_DONE:
		/* TODO: to make mock work with it */

		/* stop and clear int flag in isr */
		/* gps_dl_hal_d2a_rx_dma_stop(link_id); */
		p_link->rx_dma_buf.dma_working_entry.write_index =
			p_link->rx_dma_buf.dma_working_entry.read_index;

		/* check whether no data also happen */
		if (gps_dl_hw_usrt_has_set_nodata_flag(link_id)) {
			p_link->rx_dma_buf.dma_working_entry.is_nodata = true;
			gps_dl_hw_usrt_clear_nodata_irq(link_id);
		} else
			p_link->rx_dma_buf.dma_working_entry.is_nodata = false;

		gdl_ret = gdl_dma_buf_set_free_entry(&p_link->rx_dma_buf,
			&p_link->rx_dma_buf.dma_working_entry);

		if (gdl_ret == GDL_OKAY) {
			p_link->rx_dma_buf.dma_working_entry.is_valid = false;
			gps_dl_link_wake_up(&p_link->waitables[GPS_DL_WAIT_READ]);
		}

		gps_dl_hal_d2a_rx_dma_claim_emi_usage(link_id, false);
		/* mask data irq */
		if (s_gps_has_data_irq_masked[link_id] == true) {
			gps_dl_irq_each_link_unmask(link_id, GPS_DL_IRQ_TYPE_HAS_DATA, GPS_DL_IRQ_CTRL_FROM_HAL);
			s_gps_has_data_irq_masked[link_id] = false;
		} else
			GDL_LOGXW_DRW(link_id, "D2A_RX_DMA_DONE  while s_gps_has_data_irq_masked is false");
		break;

	case GPS_DL_HAL_EVT_D2A_RX_HAS_NODATA:
		/* get rx length */
		gdl_ret = gps_dl_hal_d2a_rx_dma_get_write_index(link_id, &write_index);

		/* 20181118 for mock, rx dma stop must after get write index */
		gps_dl_hal_d2a_rx_dma_stop(link_id);

		if (gdl_ret == GDL_OKAY) {
			/* no need to mask data irq */
			p_link->rx_dma_buf.dma_working_entry.write_index = write_index;
			p_link->rx_dma_buf.dma_working_entry.is_nodata = true;

			gdl_ret = gdl_dma_buf_set_free_entry(&p_link->rx_dma_buf,
				&p_link->rx_dma_buf.dma_working_entry);

			if (gdl_ret != GDL_OKAY)
				GDL_LOGD("gdl_dma_buf_set_free_entry ret = %s", gdl_ret_to_name(gdl_ret));

		} else
			GDL_LOGD("gps_dl_hal_d2a_rx_dma_get_write_index ret = %s", gdl_ret_to_name(gdl_ret));

		if (gdl_ret == GDL_OKAY) {
			p_link->rx_dma_buf.dma_working_entry.is_valid = false;
			gps_dl_link_wake_up(&p_link->waitables[GPS_DL_WAIT_READ]);
		}

		gps_dl_hal_d2a_rx_dma_claim_emi_usage(link_id, false);
		gps_dl_hw_usrt_clear_nodata_irq(link_id);
		gps_dl_irq_each_link_unmask(link_id, GPS_DL_IRQ_TYPE_HAS_NODATA, GPS_DL_IRQ_CTRL_FROM_HAL);
		if (gps_dl_test_mask_hasdata_irq_get(link_id)) {
			GDL_LOGXE(link_id, "test mask hasdata irq, not unmask irq and wait reset");
			gps_dl_test_mask_hasdata_irq_set(link_id, false);
			gps_dl_hal_set_irq_dis_flag(link_id, GPS_DL_IRQ_TYPE_HAS_DATA, true);
		} else {
			if (s_gps_has_data_irq_masked[link_id] == true) {
				gps_dl_irq_each_link_unmask(link_id, GPS_DL_IRQ_TYPE_HAS_DATA,
					GPS_DL_IRQ_CTRL_FROM_HAL);
				s_gps_has_data_irq_masked[link_id] = false;
			} else
				GDL_LOGXW_DRW(link_id, "D2A_RX_HAS_NODATA  while s_gps_has_data_irq_masked is false");
		}
		break;

	case GPS_DL_HAL_EVT_A2D_TX_DMA_DONE:
		/* gps_dl_hw_print_usrt_status(link_id); */

		/* data tx finished */
		gdl_ret = gdl_dma_buf_set_data_entry(&p_link->tx_dma_buf,
			&p_link->tx_dma_buf.dma_working_entry);

		p_link->tx_dma_buf.dma_working_entry.is_valid = false;

		GDL_LOGD("gdl_dma_buf_set_data_entry ret = %s", gdl_ret_to_name(gdl_ret));

		/* stop tx dma, should stop and clear int flag in isr */
		/* gps_dl_hal_a2d_tx_dma_stop(link_id); */

		/* wakeup writer if it's pending on it */
		gps_dl_link_wake_up(&p_link->waitables[GPS_DL_WAIT_WRITE]);
		gps_dl_hal_a2d_tx_dma_claim_emi_usage(link_id, false);
		gps_dl_link_start_tx_dma_if_has_data(link_id);
		break;

	case GPS_DL_HAL_EVT_DMA_ISR_PENDING:
		conninfra_okay = gps_dl_conninfra_is_okay_or_handle_it(NULL, true);
		dma_irq_en = gps_dl_hal_get_dma_irq_en_flag();

		GDL_LOGXE(link_id, "conninfra_okay = %d, dma_irq_en = %d", conninfra_okay, dma_irq_en);
		if (conninfra_okay && !dma_irq_en) {
			gps_dl_irq_unmask_dma_intr(GPS_DL_IRQ_CTRL_FROM_HAL);
			gps_dl_hal_set_dma_irq_en_flag(true);
		}
		break;

	case GPS_DL_HAL_EVT_MCUB_HAS_IRQ:
		reg_rw_log = gps_dl_log_reg_rw_is_on(GPS_DL_REG_RW_MCUB_IRQ_HANDLER);
		if (reg_rw_log)
			show_log = gps_dl_set_show_reg_rw_log(true);
		if (!gps_dl_hal_mcub_flag_handler(link_id)) {
			GDL_LOGXE(link_id, "mcub_flag_handler not okay, not unmask irq and wait reset");
			gps_dl_hal_set_mcub_irq_dis_flag(link_id, true);
		} else
			gps_dl_irq_each_link_unmask(link_id, GPS_DL_IRQ_TYPE_MCUB, GPS_DL_IRQ_CTRL_FROM_HAL);
		if (reg_rw_log)
			gps_dl_set_show_reg_rw_log(show_log);
		break;

#if 0
	case GPS_DL_HAL_EVT_DSP_ROM_START:
		gps_dsp_fsm(GPS_DSP_EVT_RESET_DONE, link_id);

		/* if has pending data, can send it now */
		gdl_ret = gdl_dma_buf_get_data_entry(&p_link->tx_dma_buf, &dma_buf_entry);
		if (gdl_ret == GDL_OKAY)
			gps_dl_hal_a2d_tx_dma_start(link_id, &dma_buf_entry);
		break;

	case GPS_DL_HAL_EVT_DSP_RAM_START:
		gps_dsp_fsm(GPS_DSP_EVT_RAM_CODE_READY, link_id);

		/* start reg polling */
		break;
#endif

	default:
		break;
	}

	j1 = jiffies;
	GDL_LOGXI_EVT(link_id, "evt = %s, on_sid = %d, dj = %lu",
		gps_dl_hal_event_name(evt), sid_on_evt, j1 - j0);
}

bool gps_dl_hal_mcub_flag_handler(enum gps_dl_link_id_enum link_id)
{
	struct gps_dl_hal_mcub_info d2a;
	bool conninfra_okay;

	/* Todo: while condition make sure DSP is on and session ID */
	while (1) {
		conninfra_okay = gps_dl_conninfra_is_okay_or_handle_it(NULL, true);
		if (!conninfra_okay) {
			GDL_LOGXE(link_id, "conninfra_okay = %d", conninfra_okay);
			return false; /* not okay */
		}

		gps_dl_hw_get_mcub_info(link_id, &d2a);
		if (d2a.flag == 0)
			break;

		if (d2a.flag == GPS_MCUB_D2AF_MASK_DSP_REG_READ_READY) {
			/* do nothing
			 *
			 * only "reg read ready" in flag bits, print the information in
			 * gps_each_dsp_reg_read_ack, rather than here.
			 */
		} else {
			GDL_LOGXI(link_id, "d2a: flag = 0x%04x, d0 = 0x%04x, d1 = 0x%04x",
				d2a.flag, d2a.dat0, d2a.dat1);
		}

		if (d2a.flag == 0xdeadfeed) {
			gps_dl_hw_dump_host_csr_gps_info(true);
			gps_dl_hw_dump_sleep_prot_status();

			GDL_LOGXE(link_id, "deadfeed, trigger connsys reset");
			gps_dl_trigger_connsys_reset();
			return false;
		}

		/* Todo: if (dsp is off) -> break */
		/* Note: clear flag before check and handle the flage event,
		 *   avoiding race condtion when dsp do "too fast twice ack".
		 *   EX: gps_each_dsp_reg_gourp_read_next
		 */
		gps_dl_hw_clear_mcub_d2a_flag(link_id, d2a.flag);

		if (d2a.flag & GPS_MCUB_D2AF_MASK_DSP_RESET_DONE) {
			/* gps_dl_hal_event_send(GPS_DL_HAL_EVT_DSP_ROM_START, link_id); */
			gps_dsp_fsm(GPS_DSP_EVT_RESET_DONE, link_id);
		}

		if (d2a.flag & GPS_MCUB_D2AF_MASK_DSP_RAMCODE_READY) {
			if (d2a.dat1 == 0xDEAD || d2a.dat1 == 0xBEEF) {
				GDL_LOGXW(link_id,
					"d2a: flag = 0x%04x, d0 = 0x%04x, d1 = 0x%04x, do dump",
					d2a.flag, d2a.dat0, d2a.dat1);
				gps_dl_hw_dump_host_csr_gps_info(true);
#if GPS_DL_HAS_CONNINFRA_DRV
				/* API to check and dump host csr */
				conninfra_is_bus_hang();
#else
				gps_dl_hw_dump_host_csr_conninfra_info(true);
#endif
				gps_dl_hw_print_hw_status(link_id, true);
				gps_dl_hw_dump_host_csr_gps_info(true);
				continue;
			}

			/* bypass gps_dsp_fsm if ariving here and the status is already working */
			if (GPS_DSP_ST_WORKING == gps_dsp_state_get(link_id))
				continue;

			/* gps_dl_hal_event_send(GPS_DL_HAL_EVT_DSP_RAM_START, link_id); */
			gps_dsp_fsm(GPS_DSP_EVT_RAM_CODE_READY, link_id);

			/* start reg polling */
			gps_each_dsp_reg_gourp_read_start(link_id, false, 1);
		}

		if (d2a.flag & GPS_MCUB_D2AF_MASK_DSP_REG_READ_READY)
			gps_each_dsp_reg_read_ack(link_id, &d2a);

	}

	return true;
}


unsigned int g_gps_dl_hal_emi_usage_bitmask;

void gps_dl_hal_emi_usage_init(void)
{
	unsigned int old_mask;

	old_mask = g_gps_dl_hal_emi_usage_bitmask;
	g_gps_dl_hal_emi_usage_bitmask = 0;

	if (old_mask)
		GDL_LOGW("mask is 0x%x, force it to 0", old_mask);
	else
		GDL_LOGD("mask is 0");

	/* Not claim/disclaim it for low power
	 * gps_dl_hal_emi_usage_claim(GPS_DL_EMI_USER_GPS_ON, true);
	 */
}

void gps_dl_hal_emi_usage_deinit(void)
{
	unsigned int old_mask;

	/* Not claim/disclaim it for low power
	 * gps_dl_hal_emi_usage_claim(GPS_DL_EMI_USER_GPS_ON, false);
	 */

	old_mask = g_gps_dl_hal_emi_usage_bitmask;

	if (old_mask)
		GDL_LOGW("mask is 0x%x, force to release emi usage", old_mask);
	else
		GDL_LOGD("mask is 0");

	/* force to release it anyway */
	gps_dl_hw_gps_sw_request_emi_usage(false);
}

void gps_dl_hal_emi_usage_claim(enum gps_dl_hal_emi_user user, bool use_emi)
{
	unsigned int old_mask, new_mask;
	bool changed = false, usage = false;

	/* TODO: protect them using spin lock to make it more safe,
	 *       currently only one thread call me, no racing.
	 */
	old_mask = g_gps_dl_hal_emi_usage_bitmask;
	if (use_emi)
		g_gps_dl_hal_emi_usage_bitmask = old_mask | (1UL << user);
	else
		g_gps_dl_hal_emi_usage_bitmask = old_mask & ~(1UL << user);
	new_mask = g_gps_dl_hal_emi_usage_bitmask;

	if (old_mask == 0 && new_mask != 0) {
		gps_dl_hw_gps_sw_request_emi_usage(true);
		changed = true;
		usage = true;
	} else if (old_mask != 0 && new_mask == 0) {
		gps_dl_hw_gps_sw_request_emi_usage(false);
		changed = true;
		usage = false;
	}

	if (changed) {
		GDL_LOGD("user = %d, use = %d, old_mask = 0x%x, new_mask = 0x%x, change = %d/%d",
			user, use_emi, old_mask, new_mask, changed, usage);
	} else {
		GDL_LOGD("user = %d, use = %d, old_mask = 0x%x, new_mask = 0x%x, change = %d",
			user, use_emi, old_mask, new_mask, changed);
	}
}

void gps_dl_hal_a2d_tx_dma_claim_emi_usage(enum gps_dl_link_id_enum link_id, bool use_emi)
{
	if (link_id == GPS_DATA_LINK_ID0)
		gps_dl_hal_emi_usage_claim(GPS_DL_EMI_USER_TX_DMA0, use_emi);
	else if (link_id == GPS_DATA_LINK_ID1)
		gps_dl_hal_emi_usage_claim(GPS_DL_EMI_USER_TX_DMA1, use_emi);
}

void gps_dl_hal_d2a_rx_dma_claim_emi_usage(enum gps_dl_link_id_enum link_id, bool use_emi)
{
	if (link_id == GPS_DATA_LINK_ID0)
		gps_dl_hal_emi_usage_claim(GPS_DL_EMI_USER_RX_DMA0, use_emi);
	else if (link_id == GPS_DATA_LINK_ID1)
		gps_dl_hal_emi_usage_claim(GPS_DL_EMI_USER_RX_DMA1, use_emi);
}

