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
	baud_rate_115200 = 115200,
	baud_rate_3m = 3000000,
	baud_rate_4m = 4000000,
	baud_rate_12m = 12000000,
	baud_rate_24m = 24000000,
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
	irq_err_type_max,
};

enum debug_dump_tx_rx_index {
	DUMP0 = 0,
	DUMP1,
};

typedef void (*UARTHUB_IRQ_CB) (unsigned int err_type);

#define KERNEL_UARTHUB_open                       UARTHUB_open
#define KERNEL_UARTHUB_close                      UARTHUB_close

#define KERNEL_UARTHUB_dev0_is_uarthub_ready      UARTHUB_dev0_is_uarthub_ready
#define KERNEL_UARTHUB_get_host_wakeup_status     UARTHUB_get_host_wakeup_status
#define KERNEL_UARTHUB_get_host_set_fw_own_status UARTHUB_get_host_set_fw_own_status
#define KERNEL_UARTHUB_dev0_is_txrx_idle          UARTHUB_dev0_is_txrx_idle
#define KERNEL_UARTHUB_dev0_set_tx_request        UARTHUB_dev0_set_tx_request
#define KERNEL_UARTHUB_dev0_set_rx_request        UARTHUB_dev0_set_rx_request
#define KERNEL_UARTHUB_dev0_set_txrx_request      UARTHUB_dev0_set_txrx_request
#define KERNEL_UARTHUB_dev0_clear_tx_request      UARTHUB_dev0_clear_tx_request
#define KERNEL_UARTHUB_dev0_clear_rx_request      UARTHUB_dev0_clear_rx_request
#define KERNEL_UARTHUB_dev0_clear_txrx_request    UARTHUB_dev0_clear_txrx_request
#define KERNEL_UARTHUB_get_uart_cmm_rx_count      UARTHUB_get_uart_cmm_rx_count
#define KERNEL_UARTHUB_is_assert_state            UARTHUB_is_assert_state

#define KERNEL_UARTHUB_irq_register_cb            UARTHUB_irq_register_cb
#define KERNEL_UARTHUB_bypass_mode_ctrl           UARTHUB_bypass_mode_ctrl
#define KERNEL_UARTHUB_is_bypass_mode             UARTHUB_is_bypass_mode
#define KERNEL_UARTHUB_config_internal_baud_rate  UARTHUB_config_internal_baud_rate
#define KERNEL_UARTHUB_config_external_baud_rate  UARTHUB_config_external_baud_rate
#define KERNEL_UARTHUB_assert_state_ctrl          UARTHUB_assert_state_ctrl
#define KERNEL_UARTHUB_sw_reset                   UARTHUB_sw_reset
#define KERNEL_UARTHUB_md_adsp_fifo_ctrl          UARTHUB_md_adsp_fifo_ctrl
#define KERNEL_UARTHUB_dump_debug_info            UARTHUB_dump_debug_info
#define KERNEL_UARTHUB_dump_debug_info_with_tag   UARTHUB_dump_debug_info_with_tag
#define KERNEL_UARTHUB_debug_bt_tx_timeout        UARTHUB_debug_bt_tx_timeout
#define KERNEL_UARTHUB_loopback_test              UARTHUB_loopback_test
#define KERNEL_UARTHUB_dump_trx_info_loop_ctrl    UARTHUB_dump_trx_info_loop_ctrl
#define KERNEL_UARTHUB_debug_dump_tx_rx_count     UARTHUB_debug_dump_tx_rx_count
#define KERNEL_UARTHUB_reset_flow_control         UARTHUB_reset_flow_control

int UARTHUB_open(void);
int UARTHUB_close(void);

int UARTHUB_dev0_is_uarthub_ready(void);
int UARTHUB_get_host_wakeup_status(void);
int UARTHUB_get_host_set_fw_own_status(void);
int UARTHUB_dev0_is_txrx_idle(int rx);
int UARTHUB_dev0_set_tx_request(void);
int UARTHUB_dev0_set_rx_request(void);
int UARTHUB_dev0_set_txrx_request(void);
int UARTHUB_dev0_clear_tx_request(void);
int UARTHUB_dev0_clear_rx_request(void);
int UARTHUB_dev0_clear_txrx_request(void);
int UARTHUB_get_uart_cmm_rx_count(void);
int UARTHUB_is_assert_state(void);

int UARTHUB_irq_register_cb(UARTHUB_IRQ_CB irq_callback);
int UARTHUB_bypass_mode_ctrl(int enable);
int UARTHUB_is_bypass_mode(void);
int UARTHUB_config_internal_baud_rate(int dev_index, enum UARTHUB_baud_rate rate);
int UARTHUB_config_external_baud_rate(enum UARTHUB_baud_rate rate);
int UARTHUB_assert_state_ctrl(int assert_ctrl);
int UARTHUB_reset_flow_control(void);
int UARTHUB_sw_reset(void);
int UARTHUB_md_adsp_fifo_ctrl(int enable);
int UARTHUB_dump_debug_info(void);
int UARTHUB_dump_debug_info_with_tag(const char *tag);
int UARTHUB_loopback_test(int dev_index, int tx_to_rx, int enable);
int UARTHUB_debug_bt_tx_timeout(const char *tag);
int UARTHUB_dump_trx_info_loop_ctrl(int enable, int loop_dur_ms);
int UARTHUB_debug_dump_tx_rx_count(const char *tag, enum debug_dump_tx_rx_index trigger_point);

#endif /* UARTHUB_DRV_EXPORT_H */
