/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef UARTHUB_DRV_EXPORT_H
#define UARTHUB_DRV_EXPORT_H

//#include <mtk_wcn_cmb_stub.h>
#include <linux/types.h>
#include <linux/fs.h>

enum UARTHUB_baud_rate {
	baud_rate_unknown = -1,
	baud_rate_3m = 0,
	baud_rate_4m,
	baud_rate_12m,
};

enum UARTHUB_irq_err_type {
	uarthub_unknown_irq_err = -1,
	dev0_crc_err = 0,
	dev1_crc_err,
	dev2_crc_err,
	dev0_tx_timeout_err,
	dev1_tx_timeout_err,
	dev2_tx_timeout_err,
	dev0_tx_pkt_type_err,
	dev1_tx_pkt_type_err,
	dev2_tx_pkt_type_err,
	dev0_rx_timeout_err,
	dev1_rx_timeout_err,
	dev2_rx_timeout_err,
	rx_pkt_type_err,
	intfhub_restore_err,
	intfhub_dev_rx_err,
	intfhub_dev0_tx_err,
	intfhub_dev1_tx_err,
	intfhub_dev2_tx_err,
};

typedef void (*UARTHUB_IRQ_CB) (enum UARTHUB_irq_err_type err_type);

int UARTHUB_open(void);
int UARTHUB_close(void);

int UARTHUB_dev0_is_uarthub_ready(void);
int UARTHUB_dev0_is_txrx_idle(int rx);
int UARTHUB_dev0_set_txrx_request(void);
int UARTHUB_dev0_clear_txrx_request(void);
int UARTHUB_is_assert_state(void);

int UARTHUB_irq_register_cb(UARTHUB_IRQ_CB irq_callback);
int UARTHUB_bypass_mode_ctrl(int enable);
int UARTHUB_is_bypass_mode(void);
int UARTHUB_config_internal_baud_rate(int dev_index, enum UARTHUB_baud_rate rate);
int UARTHUB_config_external_baud_rate(enum UARTHUB_baud_rate rate);
int UARTHUB_assert_state_ctrl(int assert_ctrl);
int UARTHUB_sw_reset(void);
int UARTHUB_md_adsp_fifo_ctrl(int enable);
int UARTHUB_dump_debug_info(void);
int UARTHUB_loopback_test(int dev_index, int tx_to_rx, int enable);

#endif /* UARTHUB_DRV_EXPORT_H */
