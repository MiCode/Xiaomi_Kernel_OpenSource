/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef UARTHUB_DRV_CORE_H
#define UARTHUB_DRV_CORE_H

#include "uarthub_def_id.h"
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/fs.h>

typedef void (*UARTHUB_CORE_IRQ_CB) (int err_type);

struct uarthub_reg_base_addr {
	unsigned long vir_addr;
	unsigned long phy_addr;
	unsigned long long size;
};

struct assert_ctrl {
	int err_type;
	struct workqueue_struct *uarthub_workqueue;
	struct work_struct trigger_assert_work;
};

/*******************************************************************************
 *                              internal function
 *******************************************************************************/
int uarthub_core_irq_register(struct platform_device *pdev);
int uarthub_core_read_reg_from_dts(struct platform_device *pdev);
int uarthub_core_read_max_dev_from_dts(struct platform_device *pdev);
int uarthub_core_config_gpio_from_dts(struct platform_device *pdev);
int uarthub_core_config_uart_glue_ctrl_from_dts(struct platform_device *pdev);
int uarthub_core_config_univpll_clk_remap_addr_from_dts(struct platform_device *pdev);
int uarthub_core_clk_get_from_dts(struct platform_device *pdev);
int uarthub_core_read_baud_rate_from_dts(int dev_index, struct platform_device *pdev);
int uarthub_core_check_irq_err_type(void);
int uarthub_core_irq_mask_ctrl(int mask);
int uarthub_core_irq_clear_ctrl(void);

int uarthub_core_crc_ctrl(int enable);
int uarthub_core_clk_univpll_ctrl(int clk_on);
int uarthub_core_rx_error_crc_info(int dev_index, int *p_crc_error_data, int *p_crc_result);
int uarthub_core_timeout_info(int dev_index, int rx, int *p_timeout_counter, int *p_pkt_counter);
int uarthub_core_config_baud_rate(void __iomem *uarthub_dev_base, int rate_index);
int uarthub_core_reset_to_ap_enable_only(int ap_only);
void uarthub_core_set_trigger_assert_worker(int err_type);
int uarthub_core_is_apb_bus_clk_enable(void);

/*******************************************************************************
 *                              public function
 *******************************************************************************/
int uarthub_core_open(void);
int uarthub_core_close(void);

int uarthub_core_dev0_is_uarthub_ready(void);
int uarthub_core_dev0_is_txrx_idle(int rx);
int uarthub_core_dev0_set_txrx_request(void);
int uarthub_core_dev0_clear_txrx_request(void);
int uarthub_core_is_assert_state(void);

int uarthub_core_irq_register_cb(UARTHUB_CORE_IRQ_CB irq_callback);
int uarthub_core_bypass_mode_ctrl(int enable);
int uarthub_core_md_adsp_fifo_ctrl(int enable);
int uarthub_core_is_bypass_mode(void);
int uarthub_core_config_internal_baud_rate(int dev_index, int rate_index);
int uarthub_core_config_external_baud_rate(int rate_index);
int uarthub_core_assert_state_ctrl(int assert_ctrl);
int uarthub_core_reset(void);
int uarthub_core_loopback_test(int dev_index, int tx_to_rx, int enable);
int uarthub_core_debug_info(void);

#endif /* UARTHUB_DRV_CORE_H */
