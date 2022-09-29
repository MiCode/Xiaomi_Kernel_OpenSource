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

struct uarthub_reg_base_addr {
	unsigned long vir_addr;
	unsigned long phy_addr;
};

struct assert_ctrl {
	int err_type;
	struct work_struct trigger_assert_work;
};

struct debug_info_ctrl {
	char tag[256];
	struct work_struct debug_info_work;
};

struct uarthub_gpio_base_addr {
	unsigned long addr;
	unsigned long mask;
	unsigned long value;
	unsigned long gpio_value;
};

struct uarthub_gpio_trx_info {
	struct uarthub_gpio_base_addr tx_mode;
	struct uarthub_gpio_base_addr rx_mode;
	struct uarthub_gpio_base_addr tx_dir;
	struct uarthub_gpio_base_addr rx_dir;
	struct uarthub_gpio_base_addr tx_ies;
	struct uarthub_gpio_base_addr rx_ies;
	struct uarthub_gpio_base_addr tx_pu;
	struct uarthub_gpio_base_addr rx_pu;
	struct uarthub_gpio_base_addr tx_pd;
	struct uarthub_gpio_base_addr rx_pd;
	struct uarthub_gpio_base_addr tx_drv;
	struct uarthub_gpio_base_addr rx_drv;
	struct uarthub_gpio_base_addr tx_smt;
	struct uarthub_gpio_base_addr rx_smt;
	struct uarthub_gpio_base_addr tx_tdsel;
	struct uarthub_gpio_base_addr rx_tdsel;
	struct uarthub_gpio_base_addr tx_rdsel;
	struct uarthub_gpio_base_addr rx_rdsel;
	struct uarthub_gpio_base_addr tx_sec_en;
	struct uarthub_gpio_base_addr rx_sec_en;
	struct uarthub_gpio_base_addr rx_din;
};

struct uarthub_uart_ip_debug_info {
	unsigned long dev0;
	unsigned long dev1;
	unsigned long dev2;
	unsigned long cmm;
	unsigned long ap;
};

typedef void (*UARTHUB_CORE_IRQ_CB) (unsigned int err_type);

typedef int(*UARTHUB_PLAT_INIT_REMAP_REG) (void);
typedef int(*UARTHUB_PLAT_DEINIT_UNMAP_REG) (void);
typedef int(*UARTHUB_PLAT_GET_MAX_NUM_DEV_HOST) (void);
typedef int(*UARTHUB_PLAT_GET_DEFAULT_BAUD_RATE) (int dev_index);
typedef int(*UARTHUB_PLAT_CONFIG_GPIO_TRX) (void);
typedef int(*UARTHUB_PLAT_GET_GPIO_TRX_INFO) (struct uarthub_gpio_trx_info *info);
typedef int(*UARTHUB_PLAT_GET_UARTHUB_CG_INFO) (void);
typedef int(*UARTHUB_PLAT_GET_UARTHUB_CLK_CG_INFO) (void);
typedef int(*UARTHUB_PLAT_GET_HWCCF_UNIVPLL_VOTE_INFO) (void);
typedef int(*UARTHUB_PLAT_GET_HWCCF_UNIVPLL_ON_INFO) (void);
typedef int(*UARTHUB_PLAT_GET_UART_MUX_INFO) (void);
typedef int(*UARTHUB_PLAT_GET_UARTHUB_ADDR_INFO) (struct uarthub_reg_base_addr *info);
typedef void __iomem *(*UARTHUB_PLAT_GET_AP_UART_BASE_ADDR) (void);
typedef void __iomem *(*UARTHUB_PLAT_GET_AP_DMA_TX_INT_ADDR) (void);
typedef void __iomem *(*UARTHUB_PLAT_GET_SYS_SRAM_ADDR) (void);
typedef int(*UARTHUB_PLAT_GET_SPM_RES_INFO) (void);
typedef int(*UARTHUB_PLAT_GET_SPM_SYS_TIMER) (uint32_t *hi, uint32_t *lo);
typedef int(*UARTHUB_PLAT_GET_PERI_CLK_INFO) (void);
typedef int(*UARTHUB_PLAT_GET_PERI_UART_PAD_MODE) (void);

struct uarthub_ops_struct {
	/* load from dts */
	UARTHUB_PLAT_INIT_REMAP_REG uarthub_plat_init_remap_reg;
	UARTHUB_PLAT_DEINIT_UNMAP_REG uarthub_plat_deinit_unmap_reg;
	UARTHUB_PLAT_GET_MAX_NUM_DEV_HOST uarthub_plat_get_max_num_dev_host;
	UARTHUB_PLAT_GET_DEFAULT_BAUD_RATE uarthub_plat_get_default_baud_rate;
	UARTHUB_PLAT_CONFIG_GPIO_TRX uarthub_plat_config_gpio_trx;
	UARTHUB_PLAT_GET_GPIO_TRX_INFO uarthub_plat_get_gpio_trx_info;
	UARTHUB_PLAT_GET_UARTHUB_CG_INFO uarthub_plat_get_uarthub_cg_info;
	UARTHUB_PLAT_GET_UARTHUB_CLK_CG_INFO uarthub_plat_get_uarthub_clk_cg_info;
	UARTHUB_PLAT_GET_HWCCF_UNIVPLL_VOTE_INFO uarthub_plat_get_hwccf_univpll_vote_info;
	UARTHUB_PLAT_GET_HWCCF_UNIVPLL_ON_INFO uarthub_plat_get_hwccf_univpll_on_info;
	UARTHUB_PLAT_GET_UART_MUX_INFO uarthub_plat_get_uart_mux_info;
	UARTHUB_PLAT_GET_UARTHUB_ADDR_INFO uarthub_plat_get_uarthub_addr_info;
	UARTHUB_PLAT_GET_AP_UART_BASE_ADDR uarthub_plat_get_ap_uart_base_addr;
	UARTHUB_PLAT_GET_SYS_SRAM_ADDR uarthub_plat_get_sys_sram_base_addr;
	UARTHUB_PLAT_GET_AP_DMA_TX_INT_ADDR uarthub_plat_get_ap_dma_tx_int_addr;
	UARTHUB_PLAT_GET_SPM_RES_INFO uarthub_plat_get_spm_res_info;
	UARTHUB_PLAT_GET_SPM_SYS_TIMER uarthub_plat_get_spm_sys_timer;
	UARTHUB_PLAT_GET_PERI_CLK_INFO uarthub_plat_get_peri_clk_info;
	UARTHUB_PLAT_GET_PERI_UART_PAD_MODE uarthub_plat_get_peri_uart_pad_mode;
};

static char * const UARTHUB_irq_err_type_str[] = {
	"dev0_crc_err",
	"dev1_crc_err",
	"dev2_crc_err",
	"dev0_tx_timeout_err",
	"dev1_tx_timeout_err",
	"dev2_tx_timeout_err",
	"dev0_tx_pkt_type_err",
	"dev1_tx_pkt_type_err",
	"dev2_tx_pkt_type_err",
	"dev0_rx_timeout_err",
	"dev1_rx_timeout_err",
	"dev2_rx_timeout_err",
	"rx_pkt_type_err",
	"intfhub_restore_err",
	"intfhub_dev_rx_err",
	"intfhub_dev0_tx_err",
	"intfhub_dev1_tx_err",
	"intfhub_dev2_tx_err"
};

/*******************************************************************************
 *                              internal function
 *******************************************************************************/
struct uarthub_ops_struct *uarthub_core_get_platform_ic_ops(struct platform_device *pdev);
int uarthub_core_irq_register(struct platform_device *pdev);
int uarthub_core_get_uarthub_reg(void);
int uarthub_core_check_disable_from_dts(struct platform_device *pdev);
int uarthub_core_get_max_dev(void);
int uarthub_core_config_hub_mode_gpio(void);
int uarthub_core_clk_get_from_dts(struct platform_device *pdev);
int uarthub_core_get_default_baud_rate(int dev_index);
int uarthub_core_check_irq_err_type(void);
int uarthub_core_irq_mask_ctrl(int mask);
int uarthub_core_irq_clear_ctrl(void);

int uarthub_core_crc_ctrl(int enable);
int uarthub_core_clk_univpll_ctrl(int clk_on);
int uarthub_core_is_univpll_on(void);
int uarthub_core_rx_error_crc_info(int dev_index, int *p_crc_error_data, int *p_crc_result);
int uarthub_core_timeout_info(int dev_index, int rx, int *p_timeout_counter, int *p_pkt_counter);
int uarthub_core_config_baud_rate(void __iomem *uarthub_dev_base, int rate_index);
int uarthub_core_reset_to_ap_enable_only(int ap_only);
void uarthub_core_set_trigger_uarthub_error_worker(int err_type);
void uarthub_core_set_trigger_uarthub_frame_error_worker(void);
int uarthub_core_is_apb_bus_clk_enable(void);
int uarthub_core_is_uarthub_clk_enable(void);
int uarthub_core_debug_uart_ip_info_with_tag_ex(const char *tag, int boundary);
int uarthub_core_debug_uart_ip_info_loop(void);
int uarthub_core_debug_uart_ip_info_loop_compare_diff(void);
int uarthub_core_debug_apdma_uart_info_with_tag_ex(const char *tag, int boundary);
int uarthub_core_debug_info_with_tag_worker(const char *tag);
int uarthub_core_debug_clk_info_worker(const char *tag);
int uarthub_core_dump_trx_info_loop_trigger(void);
int uarthub_core_debug_byte_cnt_info(const char *tag);
int uarthub_core_debug_clk_info(const char *tag);

/*******************************************************************************
 *                              public function
 *******************************************************************************/
int uarthub_core_open(void);
int uarthub_core_close(void);

int uarthub_core_dev0_is_uarthub_ready(void);
int uarthub_core_get_host_wakeup_status(void);
int uarthub_core_get_host_set_fw_own_status(void);
int uarthub_core_dev0_is_txrx_idle(int rx);
int uarthub_core_dev0_set_tx_request(void);
int uarthub_core_dev0_set_rx_request(void);
int uarthub_core_dev0_set_txrx_request(void);
int uarthub_core_dev0_clear_tx_request(void);
int uarthub_core_dev0_clear_rx_request(void);
int uarthub_core_dev0_clear_txrx_request(void);
int uarthub_core_get_uart_cmm_rx_count(void);
int uarthub_core_is_assert_state(void);

int uarthub_core_irq_register_cb(UARTHUB_CORE_IRQ_CB irq_callback);
int uarthub_core_bypass_mode_ctrl(int enable);
int uarthub_core_md_adsp_fifo_ctrl(int enable);
int uarthub_core_is_bypass_mode(void);
int uarthub_core_config_internal_baud_rate(int dev_index, int rate_index);
int uarthub_core_config_external_baud_rate(int rate_index);
int uarthub_core_assert_state_ctrl(int assert_ctrl);
int uarthub_core_reset_flow_control(void);
int uarthub_core_reset(void);
int uarthub_core_loopback_test(int dev_index, int tx_to_rx, int enable);
int uarthub_core_debug_info(void);
int uarthub_core_debug_info_with_tag(const char *tag);
int uarthub_core_debug_bt_tx_timeout(const char *tag);
int uarthub_core_dump_trx_info_loop_ctrl(int enable, int loop_dur_ms);
int uarthub_core_debug_dump_tx_rx_count(const char *tag, int trigger_point);

#endif /* UARTHUB_DRV_CORE_H */
