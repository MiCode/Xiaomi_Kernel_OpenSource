/*

SiI8348 Linux Driver

Copyright (C) 2013 Silicon Image, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation version 2.
This program is distributed AS-IS WITHOUT ANY WARRANTY of any
kind, whether express or implied; INCLUDING without the implied warranty
of MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE or NON-INFRINGEMENT.  See 
the GNU General Public License for more details at http://www.gnu.org/licenses/gpl-2.0.html.             

*/ 

/*
 *   @file mhl_supp.h
 *
 */
#if !defined(MHL_SUPP_H)
#define MHL_SUPP_H

/* APIs exported from mhl_supp.c */
int si_mhl_tx_initialize(struct mhl_dev_context *dev_context, bool bootup);

void process_cbus_abort(struct mhl_dev_context *dev_context);

void si_mhl_tx_drive_states(struct mhl_dev_context *dev_context);

void si_mhl_tx_process_events(struct mhl_dev_context *dev_context);

uint8_t si_mhl_tx_set_preferred_pixel_format(
							struct mhl_dev_context *dev_context,
							uint8_t clkMode);

void si_mhl_tx_process_write_burst_data(struct mhl_dev_context *dev_context);

void si_mhl_tx_msc_command_done(struct mhl_dev_context *dev_context,
								uint8_t data1);

void si_mhl_tx_notify_downstream_hpd_change(
				struct mhl_dev_context *dev_context,
				uint8_t downstream_hpd);

void si_mhl_tx_got_mhl_status(struct mhl_dev_context *dev_context,
							  uint8_t status_0, uint8_t status_1);

void si_mhl_tx_got_mhl_intr(struct mhl_dev_context *dev_context,
							uint8_t intr_0, uint8_t intr_1);

enum scratch_pad_status si_mhl_tx_request_write_burst(
						struct mhl_dev_context *dev_context, uint8_t reg_offset,
						uint8_t length, uint8_t *data);

bool si_mhl_tx_rcp_send(struct mhl_dev_context *dev_context,
						uint8_t rcpKeyCode);

bool si_mhl_tx_rcpk_send(struct mhl_dev_context *dev_context,
						 uint8_t rcp_key_code);

bool si_mhl_tx_rcpe_send(struct mhl_dev_context *dev_context,
						 uint8_t rcpe_error_code);

bool si_mhl_tx_ucp_send(struct mhl_dev_context *dev_context,
						uint8_t ucp_key_code);

bool si_mhl_tx_ucpk_send(struct mhl_dev_context *dev_context,
						 uint8_t ucp_key_code);

bool si_mhl_tx_ucpe_send(struct mhl_dev_context *dev_context,
						 uint8_t ucpe_error_code);

bool si_mhl_tx_rap_send(struct mhl_dev_context *dev_context,
						uint8_t rap_action_code);

enum hdcp_state {
	AVAILABLE	= 0,
	ON,
	OFF
};

void si_mhl_tx_enable_hdcp(struct mhl_dev_context *dev_context,
						   enum hdcp_state);

enum hdcp_state si_mhl_tx_get_hdcp_state(struct mhl_dev_context *dev_context);

int si_mhl_tx_shutdown(struct mhl_dev_context *dev_context);

#endif /* #if !defined(MHL_SUPP_H) */
