/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef C2K_HW_H
#define C2K_HW_H

#include <linux/interrupt.h>
#include "core.h"

#define GPIO_C2K_UNKNOWN (-1)
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
#define AP_USING_REGISTER (0xFFFFFF)
#endif
#define GPIO_C2K_VALID(gpio) ((gpio == GPIO_C2K_UNKNOWN) ? 0 : 1)

#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
#define MDM_MMC_ID "mmc2"
#else
#define MDM_MMC_ID "mmc3"
#endif

/******************Gpio Config *************************/

#include <linux/gpio.h>
/*#include <mt-plat/eint.h>*/

#if defined(CONFIG_MTK_LEGACY)
#include <cust_gpio_usage.h>
#endif

#include "modem_sdio.h"
#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
#define GPIO_C2K_MDM_PWR_EN				GPIO_VIA_MDM_PWR_EN
#define GPIO_C2K_MDM_PWR_IND			GPIO_C2K_UNKNOWN
#define GPIO_C2K_MDM_RST				GPIO_VIA_MDM_RST
#define GPIO_C2K_MDM_RST_IND			GPIO_VIA_MDM_RST_IND
#define GPIO_C2K_MDM_RST_IND_POLAR		0
#define GPIO_C2K_MDM_BOOT_SEL			GPIO_C2K_UNKNOWN
#define GPIO_C2K_MDM_ETS_SEL			GPIO_VIA_ETS_SEL
#define GPIO_C2K_SDIO_AP_RDY			GPIO_VIA_AP_RDY
#define GPIO_C2K_SDIO_MDM_RDY			GPIO_VIA_MDM_RDY
#define GPIO_C2K_SDIO_AP_WAKE_MDM		GPIO_VIA_AP_WAKE_MDM
#define GPIO_C2K_SDIO_MDM_WAKE_AP		GPIO_VIA_MDM_WAKE_AP
#define GPIO_C2K_SDIO_SYNC_POLAR		0
#ifdef GPIO_VIA_CBP_BCKUP_1
#define GPIO_C2K_CRASH_CBP				GPIO_VIA_CBP_BCKUP_1
#else
#define GPIO_C2K_CRASH_CBP				GPIO_C2K_UNKNOWN
#endif
#define GPIO_C2K_SDIO_DATA_ACK			GPIO_VIA_SDIO_ACK
#define GPIO_C2K_SDIO_DATA_ACK_POLAR	0
#define GPIO_C2K_SDIO_FLOW_CTRL			GPIO_VIA_FLOW_CTRL
#define GPIO_C2K_SDIO_FLOW_CTRL_POLAR	1
/*level shift maybe not used*/
#define GPIO_C2K_MC3_EN_N				GPIO_C2K_UNKNOWN
#define GPIO_C2K_SD_SEL_N				GPIO_C2K_UNKNOWN
#else				/*CONFIG_EVDO_DT_VIA_SUPPORT */
/*We actually only use EINT, no real GPIO. So no GPIO dws setting.*/
#define GPIO_C2K_MDM_RST_IND			208	/*EINT NUM, not GPIO num */
#define GPIO_C2K_SDIO_MDM_RDY			211	/*EINT NUM, not GPIO num */
#define GPIO_C2K_SDIO_MDM_WAKE_AP		212	/*EINT NUM, not GPIO num */
#define GPIO_C2K_EXCEPTION				210	/*EINT NUM, not GPIO num */
#define GPIO_C2K_MDM_RST_IND_POLAR		0
#define GPIO_C2K_SDIO_SYNC_POLAR		0
#define GPIO_C2K_MDM_PWR_EN				GPIO_C2K_UNKNOWN
#define GPIO_C2K_MDM_PWR_IND			GPIO_C2K_UNKNOWN
#define GPIO_C2K_MDM_RST				GPIO_C2K_UNKNOWN
#define GPIO_C2K_MDM_BOOT_SEL			GPIO_C2K_UNKNOWN
#define GPIO_C2K_MDM_ETS_SEL			AP_USING_REGISTER
#define GPIO_C2K_SDIO_AP_RDY			AP_USING_REGISTER
#define GPIO_C2K_SDIO_AP_WAKE_MDM		AP_USING_REGISTER
#define GPIO_C2K_CRASH_CBP				GPIO_C2K_UNKNOWN
#define GPIO_C2K_SDIO_DATA_ACK			GPIO_C2K_UNKNOWN
#define GPIO_C2K_SDIO_DATA_ACK_POLAR	0
#define GPIO_C2K_SDIO_FLOW_CTRL		GPIO_C2K_UNKNOWN
#define GPIO_C2K_SDIO_FLOW_CTRL_POLAR	1
/*level shift maybe not used*/
#define GPIO_C2K_MC3_EN_N				GPIO_C2K_UNKNOWN
#define GPIO_C2K_SD_SEL_N				GPIO_C2K_UNKNOWN
#endif

/******************Gpio Function   ***********************/

/*config the gpio to be input for irq if the SOC need*/
int c2k_gpio_direction_input_for_irq(int gpio);
int c2k_gpio_direction_output(int gpio, int value);
int c2k_gpio_get_value(int gpio);
int c2k_gpio_to_irq(int gpio);
int c2k_irq_to_gpio(int irq);
int c2k_gpio_set_irq_type(int gpio, unsigned int type);
int c2k_gpio_request_irq(int gpio, irq_handler_t handler, unsigned long flags,
			 const char *name, void *dev);
void c2k_gpio_irq_mask(int gpio);
void c2k_gpio_irq_unmask(int gpio);
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
int c2k_gpio_to_ls(int gpio);
int c2k_ap_wake_cp(int value);
int c2k_ap_ready_indicate(int value);
#endif

/*********************Sync Control ************************/

/*notifer events */
#define ASC_NTF_TX_READY      0x0001	/*notifie CBP is ready to work */
#define ASC_NTF_TX_UNREADY    0x0002	/*notifie CBP is not ready to work */
#define ASC_NTF_RX_PREPARE    0x1001	/*notifier the device active to receive data from CBP */
#define ASC_NTF_RX_POST       0x1002	/*notifer the device CBP stop tx data */
#define ASC_NAME_LEN   (64)
/*used to register handle*/
struct asc_config {
	int gpio_ready;
	int gpio_wake;
	/*the level which indicate ap is ready */
	int polar;
	char name[ASC_NAME_LEN];
};

/*Used to registe user according to handle*/
struct asc_infor {
	void *data;
	int (*notifier)(int, void *);
	char name[ASC_NAME_LEN];
};

#define CBP_TX_HD_NAME "TxHdCbp"
#define CBP_TX_USER_NAME "cbp"

#define USB_RX_HD_NAME "RxHdUsb"
#define USB_RX_USER_NAME "usb"

#define UART_RX_HD_NAME "RxHdUart"
#define UART_RX_USER_NAME "uart"

#define SDIO_RX_HD_NAME "RxHdSdio"
#define SDIO_RX_USER_NAME "sdio"

#define RAWBULK_RX_USER_NAME "rawbulk"

#define ASC_PATH(hd, user) hd"."user

int asc_tx_register_handle(struct asc_config *cfg);
int asc_tx_add_user(const char *name, struct asc_infor *infor);
void asc_tx_del_user(const char *path);
int asc_tx_get_ready(const char *path, int sync);
int asc_tx_put_ready(const char *path, int sync);
int asc_tx_auto_ready(const char *name, int sync);
int asc_tx_ready_count(const char *name, int inc);

int asc_tx_check_ready(const char *name);
int asc_tx_set_auto_delay(const char *name, int delay);
int asc_tx_user_count(const char *path);
void asc_tx_reset(const char *name);

int asc_rx_register_handle(struct asc_config *cfg);
int asc_rx_add_user(const char *name, struct asc_infor *infor);
void asc_rx_del_user(const char *path);
int asc_rx_confirm_ready(const char *name, int ready);
void asc_rx_reset(const char *name);
int asc_rx_check_on_start(const char *name);

/*******************************Power Control *********************************/

/*modem event notification values */
enum clock_event_nofitiers {
	MDM_EVT_NOTIFY_POWER_ON = 0,
	MDM_EVT_NOTIFY_POWER_OFF,
	MDM_EVT_NOTIFY_RESET_ON,
	MDM_EVT_NOTIFY_RESET_OFF,
	MDM_EVT_NOTIFY_HD_ERR,
	MDM_EVT_NOTIFY_HD_ENHANCE,
	MDM_EVT_NOTIFY_IPOH,
	MDM_EVT_NOTIFY_WDT,
	MDM_EVT_NOTIFY_EXCP,
	MDM_EVT_NOTIFY_NUM
};

void modem_notify_event(int event);
int modem_register_notifier(struct notifier_block *nb);
int modem_unregister_notifier(struct notifier_block *nb);
void c2k_reset_tx_gpio_ready(int gpio);

extern unsigned int mt_gpio_to_irq(unsigned int gpio);

#endif
