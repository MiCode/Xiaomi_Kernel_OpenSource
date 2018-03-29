/*
 *drivers/mmc/card/cbp_sdio.h
 *
 *VIA CBP SDIO driver for Linux
 *
 *Copyright (C) 2009 VIA TELECOM Corporation, Inc.
 *Author: VIA TELECOM Corporation, Inc.
 *
 *This package is free software; you can redistribute it and/or modify
 *it under the terms of the GNU General Public License version 2 as
 *published by the Free Software Foundation.
 *
 *THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef CBP_SDIO_H
#define CBP_SDIO_H

#include <linux/init.h>
#include <linux/kernel.h>

#include <linux/mmc/host.h>
#include "c2k_hw.h"

#define DRIVER_NAME "cbp"

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
#ifdef BIT
#undef BIT
#endif

#define BIT(n)              (0x1<<n)

/*The Bellow Definition is for Driver Domain Control Register*/

#define SDIO_CCIR           0x0000	/*Chip ID */
#define SDIO_CHLPCR         0x0004	/*HIF Low Power Control */
#define SDIO_CSDIOCSR       0x0008	/*SDIO Status Register */
#define SDIO_CHCR           0x000C	/*HIF Control Register */
#define SDIO_CHISR          0x0010	/*HIF Interrupt Status */
#define SDIO_CHIER          0x0014	/*HIF Interrupt Enable */
#define SDIO_CTDR           0x0018	/*Tx Data Port */
#define SDIO_CRDR           0x001C	/*Rx Data Port */
#define SDIO_CTFSR          0x0020	/*Tx FIFO Status */
#define SDIO_CRPLR          0x0024	/*Rx Packet Length */
#define SDIO_CTMDR          0x00B0	/*Test Mode Data Port Register */
#define SDIO_CTMCR          0x00B4	/*Test Mode Control Register */
#define SDIO_CTMDPCR0       0x00B8	/*Test Mode Data Pattern 0 */
#define SDIO_CTMDPCR1       0x00BC	/*Test Mode Data Pattern 1 */
#define SDIO_CSR            0x00D8	/*Snapshot Register */
#define SDIO_CLKIOCR        0x0100
#define SDIO_CMDIOCR        0x0104
#define SDIO_DAT0IOCR       0x0108
#define SDIO_DAT1IOCR       0x010C
#define SDIO_DAT2IOCR       0x0110
#define SDIO_DAT3IOCR       0x0114
#define SDIO_CLKDLYCR       0x0118
#define SDIO_CMDDLYCR       0x011C
#define SDIO_ODATDLYCR      0x0120
#define SDIO_IDATDLYCR1     0x0124
#define SDIO_IDATDLYCR2     0x0128
#define SDIO_ILHCR          0x012C
#define SDIO_CCIR_G_FUNC_RDY            BIT(23)
#define SDIO_CCIR_F_FUNC_RDY            BIT(22)
#define SDIO_CCIR_B_FUNC_RDY            BIT(21)
#define SDIO_CCIR_POR_INDICATOR         BIT(20)
#define SDIO_CCIR_REVISION_ID           0x000F0000
#define SDIO_CCIR_CHIP_ID               0x0000FFFF
#define SDIO_CHLPCR_FW_OWN_REQ_CLR      BIT(9)	/*Get ownership from FW */
#define SDIO_CHLPCR_FW_OWN_REQ_SET      BIT(8)	/*Give ownership to FW */
#define SDIO_CHLPCR_INT_EN_CLR          BIT(1)	/*Clr will disable interrupt out to host */
#define SDIO_CHLPCR_INT_EN_SET          BIT(0)	/*Set will enable interrupt out to host */
/*Enable/disable response to CMD7 re-select */
#define SDIO_CSDIOCSR_PB_CMD7_RESELECT_DIS		BIT(3)
#define SDIO_CSDIOCSR_SDIO_INT_CTL      BIT(2)	/*Enable/disable Async interrupt */
#define SDIO_CSDIOCSR_SDIO_BUSY_EN      BIT(1)	/*Enable/disable write busy signal */
#define SDIO_CSDIOCSR_SDIO_RE_INIT_EN   BIT(0)	/*If set, it will let CMD5 reset SDIO IP */
#define SDIO_CHCR_INT_CLR_CTRL          BIT(1)	/*Control Read-Clear or Write-1-Clear */
#define SDIO_CHISR_RX_PKT_LEN           0xFFFF0000
#define SDIO_CHISR_FIRMWARE_INT         0x0000FE00
#define SDIO_CHISR_TX_OVERFLOW          BIT(8)
#define SDIO_CHISR_FW_INT_INDICATOR     BIT(7)
#define SDIO_CHISR_TX_CMPLT_CNT         0x00000070
#define SDIO_CHISR_TX_UNDER_THOLD       BIT(3)
#define SDIO_CHISR_TX_EMPTY             BIT(2)
#define SDIO_CHISR_RX_RDY               BIT(1)
#define SDIO_CHISR_FW_OWN_BACK          BIT(0)
#define SDIO_CHIER_FIRMWARE_INT_EN      0x0000FE00
#define SDIO_CHIER_TX_OVERFLOW_EN       BIT(8)
#define SDIO_CHIER_FW_INT_INDICATOR_EN  BIT(7)
#define SDIO_CHIER_TX_UNDER_THOLD_EN    BIT(3)
#define SDIO_CHIER_TX_EMPTY_EN          BIT(2)
#define SDIO_CHIER_RX_RDY_EN            BIT(1)
#define SDIO_CHIER_FW_OWN_BACK_EN       BIT(0)
#define SDIO_CTFSR_TX_FIFO_CNT          0x000000FF	/*in unit of 16byte */
#define SDIO_CRPLR_RX_PKT_LEN           0xFFFF0000	/*in unit of byte */
#define SDIO_CTMCR_FW_OWN               BIT(24)
#define SDIO_CTMCR_PRBS_INIT_VAL        0x00FF0000
#define SDIO_CTMCR_TEST_MODE_STATUS     BIT(8)
#define SDIO_CTMCT_TEST_MODE_SELECT     0x00000003
#endif

struct cbp_wait_event {
	wait_queue_head_t wait_q;
	atomic_t state;
	int wait_gpio;
	int wait_polar;
};

struct cbp_reset {
	struct mmc_host *host;
	const char *name;
	struct workqueue_struct *reset_wq;
	struct work_struct reset_work;
	struct timer_list timer_gpio;
	int rst_ind_gpio;
	int rst_ind_polar;
};
struct cbp_exception {
	struct mmc_host *host;
	const char *name;
	struct workqueue_struct *excp_wq;
	struct work_struct excp_work;
	struct timer_list timer_gpio;
	int excp_ind_gpio;
	int excp_ind_polar;
};

struct cbp_platform_data {
	char *bus;
	char *host_id;

	bool ipc_enable;
	bool data_ack_enable;
	bool rst_ind_enable;
	bool flow_ctrl_enable;
	bool tx_disable_irq;
	struct asc_config *tx_handle;

	int gpio_ap_wkup_cp;
	int gpio_cp_ready;
	int gpio_cp_wkup_ap;
	int gpio_ap_ready;
	int gpio_sync_polar;

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	int gpio_cp_exception;
	int c2k_wdt_irq_id;
#endif

	int gpio_data_ack;
	int gpio_data_ack_polar;

	int gpio_rst_ind;
	int gpio_rst_ind_polar;

	int gpio_flow_ctrl;
	int gpio_flow_ctrl_polar;

	int gpio_pwr_on;
	int gpio_rst;
	/*for the level transfor chip fssd06 */
	int gpio_sd_select;
	int gpio_mc3_enable;

	struct sdio_modem *modem;

	struct cbp_wait_event *cbp_data_ack;
	void (*data_ack_wait_event)(struct cbp_wait_event *pdata_ack);
	struct cbp_wait_event *cbp_flow_ctrl;
	void (*flow_ctrl_wait_event)(struct cbp_wait_event *pflow_ctrl);

	int (*detect_host)(const char *host_id);
	int (*cbp_setup)(struct cbp_platform_data *pdata);
	void (*cbp_destroy)(void);
};

enum {
	MODEM_ST_READY = 0,	/*modem ready */
	MODEM_ST_TX_RX,
	MODEM_ST_UNKNOWN
};

enum {
	FLOW_CTRL_DISABLE = 0,
	FLOW_CTRL_ENABLE
};

#if !defined(CONFIG_MTK_CLKMGR)
#include <linux/clk.h>
extern struct clk *clk_scp_sys_md2_main;
#endif

extern struct sdio_modem *c2k_modem;
extern void modem_pre_stop(void);
extern void modem_reset_handler(void);

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
extern unsigned int get_c2k_wdt_irq_id(void);
extern void via_sdio_on(int sdio_port_num);
extern void via_sdio_off(int sdio_port_num);
#endif

#ifdef WAKE_HOST_BY_SYNC	/*wake up sdio host by four wire sync mechanis */
/*extern void VIA_trigger_signal(int i_on);*/
extern void SRC_trigger_signal(int i_on);
#endif

extern void c2k_modem_reset_platform(void);
extern void c2k_reset_modem(void);

extern void c2k_platform_restore_first_init(void);
extern void enable_c2k_jtag(int mode);
void modem_notify_event(int event);

int c2k_gpio_get_ls(int gpio);

extern void set_ap_ready(int value);
extern void set_ap_wake_cp(int value);

extern int modem_on_off_ctrl_chan(unsigned char on);
extern void gpio_irq_cbp_rst_ind(void);
extern int dump_c2k_sdio_status(struct sdio_modem *modem);
extern void c2k_modem_power_on_platform(void);
extern void c2k_modem_power_off_platform(void);
extern void c2k_modem_reset_platform(void);
extern void c2k_wake_host(int wake);
extern void c2k_modem_reset_pccif(void);

extern struct sdio_modem *c2k_modem;

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
extern void set_ets_sel(int value);
extern int force_c2k_assert(struct sdio_modem *modem);
#endif

extern int modem_on_off_ctrl_chan(unsigned char on);
extern void gpio_irq_cbp_rst_ind(void);

extern void c2k_modem_power_on_platform(void);
extern void c2k_modem_power_off_platform(void);
extern void c2k_modem_reset_platform(void);
extern void c2k_wake_host(int wake);
extern void c2k_modem_reset_pccif(void);

extern int dump_c2k_sdio_status(struct sdio_modem *modem);
/*extern void gpio_irq_cbp_excp_ind(void);*/
extern void dump_c2k_iram(void);

#endif
