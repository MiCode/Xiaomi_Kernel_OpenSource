// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */
#include <linux/kernel.h>

#include "uarthub_drv_export.h"
#include "uarthub_drv_core.h"

#include <linux/string.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/cdev.h>

#include <linux/interrupt.h>
#include <linux/ratelimit.h>


UARTHUB_IRQ_CB g_irq_callback;

static void UARTHUB_irq_error_register_cb(unsigned int err_type)
{
	if (g_irq_callback)
		(*g_irq_callback)(err_type);
}

int UARTHUB_open(void)
{
	return uarthub_core_open();
}
EXPORT_SYMBOL(UARTHUB_open);

int UARTHUB_close(void)
{
	return uarthub_core_close();
}
EXPORT_SYMBOL(UARTHUB_close);

int UARTHUB_dev0_is_uarthub_ready(void)
{
	return uarthub_core_dev0_is_uarthub_ready();
}
EXPORT_SYMBOL(UARTHUB_dev0_is_uarthub_ready);

int UARTHUB_get_host_wakeup_status(void)
{
	return uarthub_core_get_host_wakeup_status();
}
EXPORT_SYMBOL(UARTHUB_get_host_wakeup_status);

int UARTHUB_get_host_set_fw_own_status(void)
{
	return uarthub_core_get_host_set_fw_own_status();
}
EXPORT_SYMBOL(UARTHUB_get_host_set_fw_own_status);

int UARTHUB_dev0_is_txrx_idle(int rx)
{
	return uarthub_core_dev0_is_txrx_idle(rx);
}
EXPORT_SYMBOL(UARTHUB_dev0_is_txrx_idle);

int UARTHUB_dev0_set_tx_request(void)
{
	return uarthub_core_dev0_set_tx_request();
}
EXPORT_SYMBOL(UARTHUB_dev0_set_tx_request);

int UARTHUB_dev0_set_rx_request(void)
{
	return uarthub_core_dev0_set_rx_request();
}
EXPORT_SYMBOL(UARTHUB_dev0_set_rx_request);

int UARTHUB_dev0_set_txrx_request(void)
{
	return uarthub_core_dev0_set_txrx_request();
}
EXPORT_SYMBOL(UARTHUB_dev0_set_txrx_request);

int UARTHUB_dev0_clear_tx_request(void)
{
	return uarthub_core_dev0_clear_tx_request();
}
EXPORT_SYMBOL(UARTHUB_dev0_clear_tx_request);

int UARTHUB_dev0_clear_rx_request(void)
{
	return uarthub_core_dev0_clear_rx_request();
}
EXPORT_SYMBOL(UARTHUB_dev0_clear_rx_request);

int UARTHUB_dev0_clear_txrx_request(void)
{
	return uarthub_core_dev0_clear_txrx_request();
}
EXPORT_SYMBOL(UARTHUB_dev0_clear_txrx_request);

int UARTHUB_get_uart_cmm_rx_count(void)
{
	return uarthub_core_get_uart_cmm_rx_count();
}
EXPORT_SYMBOL(UARTHUB_get_uart_cmm_rx_count);

int UARTHUB_is_assert_state(void)
{
	return uarthub_core_is_assert_state();
}
EXPORT_SYMBOL(UARTHUB_is_assert_state);

int UARTHUB_irq_register_cb(UARTHUB_IRQ_CB irq_callback)
{
	g_irq_callback = irq_callback;
	uarthub_core_irq_register_cb(UARTHUB_irq_error_register_cb);
	return 0;
}
EXPORT_SYMBOL(UARTHUB_irq_register_cb);

int UARTHUB_bypass_mode_ctrl(int enable)
{
	return uarthub_core_bypass_mode_ctrl(enable);
}
EXPORT_SYMBOL(UARTHUB_bypass_mode_ctrl);

int UARTHUB_is_bypass_mode(void)
{
	return uarthub_core_is_bypass_mode();
}
EXPORT_SYMBOL(UARTHUB_is_bypass_mode);

int UARTHUB_config_internal_baud_rate(int dev_index, enum UARTHUB_baud_rate rate)
{
	return uarthub_core_config_internal_baud_rate(dev_index, (int)rate);
}
EXPORT_SYMBOL(UARTHUB_config_internal_baud_rate);

int UARTHUB_config_external_baud_rate(enum UARTHUB_baud_rate rate)
{
	return uarthub_core_config_external_baud_rate((int)rate);
}
EXPORT_SYMBOL(UARTHUB_config_external_baud_rate);

int UARTHUB_assert_state_ctrl(int assert_ctrl)
{
	return uarthub_core_assert_state_ctrl(assert_ctrl);
}
EXPORT_SYMBOL(UARTHUB_assert_state_ctrl);

int UARTHUB_reset_flow_control(void)
{
	return uarthub_core_reset_flow_control();
}
EXPORT_SYMBOL(UARTHUB_reset_flow_control);

int UARTHUB_sw_reset(void)
{
	uarthub_core_reset();
	return 0;
}
EXPORT_SYMBOL(UARTHUB_sw_reset);

int UARTHUB_md_adsp_fifo_ctrl(int enable)
{
	return uarthub_core_md_adsp_fifo_ctrl(enable);
}
EXPORT_SYMBOL(UARTHUB_md_adsp_fifo_ctrl);

int UARTHUB_dump_debug_info(void)
{
	return uarthub_core_debug_info();
}
EXPORT_SYMBOL(UARTHUB_dump_debug_info);

int UARTHUB_dump_debug_info_with_tag(const char *tag)
{
	return uarthub_core_debug_info_with_tag(tag);
}
EXPORT_SYMBOL(UARTHUB_dump_debug_info_with_tag);

int UARTHUB_loopback_test(int dev_index, int tx_to_rx, int enable)
{
	return uarthub_core_loopback_test(dev_index, tx_to_rx, enable);
}
EXPORT_SYMBOL(UARTHUB_loopback_test);

int UARTHUB_debug_bt_tx_timeout(const char *tag)
{
	return uarthub_core_debug_bt_tx_timeout(tag);
}
EXPORT_SYMBOL(UARTHUB_debug_bt_tx_timeout);

int UARTHUB_dump_trx_info_loop_ctrl(int enable, int loop_dur_ms)
{
	return uarthub_core_dump_trx_info_loop_ctrl(enable, loop_dur_ms);
}
EXPORT_SYMBOL(UARTHUB_dump_trx_info_loop_ctrl);

int UARTHUB_debug_dump_tx_rx_count(const char *tag, enum debug_dump_tx_rx_index trigger_point)
{
	return uarthub_core_debug_dump_tx_rx_count(tag, (int)trigger_point);
}
EXPORT_SYMBOL(UARTHUB_debug_dump_tx_rx_count);

MODULE_LICENSE("GPL");
