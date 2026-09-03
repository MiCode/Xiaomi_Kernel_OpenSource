// SPDX-License-Identifier: GPL-2.0-only
/*
 * sdio_int.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __SDIO_INT_H__
#define __SDIO_INT_H__

#include <linux/device.h>
#define SLP_MGR_HEADER "[slp_mgr]"

#define SLP_MGR_ERR(fmt, args...)	\
	pr_err(SLP_MGR_HEADER fmt "\n", ## args)
#define SLP_MGR_INFO(fmt, args...)	\
	pr_info(SLP_MGR_HEADER fmt "\n", ## args)
#define SLP_MGR_DBG(fmt, args...)	\
	pr_debug(SLP_MGR_HEADER fmt "\n", ## args)


extern struct sdio_int_t sdio_int;

typedef void (*PUB_INT_ISR)(void);
enum AP_INT_CP_BIT {
	ALLOW_CP_SLP,
	WIFI_BIN_DOWNLOAD,
	BT_BIN_DOWNLOAD,
	SAVE_CP_MEM,
	TEST_DEL_THREAD,

	INT_CP_MAX = 8,
};

enum PUB_INT_BIT {
	MEM_SAVE_BIN,
	WAKEUP_ACK,
	REQ_SLP,
	WIFI_OPEN,
	BT_OPEN,
	WIFI_CLOSE,
	BT_CLOSE,
	GNSS_CALI_DONE,

	PUB_INT_MAX = 8,
};

union AP_INT_CP0_REG {
	unsigned char reg;
	struct {
		unsigned char allow_cp_slp:1;
		unsigned char wifi_bin_download:1;
		unsigned char bt_bin_download:1;
		unsigned char save_cp_mem:1;
		unsigned char test_delet_thread:1;
		unsigned char rsvd:3;
	} bit;
};

union PUB_INT_EN0_REG {
	unsigned char reg;
	struct {
		unsigned char mem_save_bin:1;
		unsigned char wakeup_ack:1;
		unsigned char req_slp:1;
		unsigned char wifi_open:1;
		unsigned char bt_open:1;
		unsigned char wifi_close:1;
		unsigned char bt_close:1;
		unsigned char gnss_cali_done:1;
	} bit;
};

union PUB_INT_CLR0_REG {
	unsigned char reg;
	struct {
		unsigned char mem_save_bin:1;
		unsigned char wakeup_ack:1;
		unsigned char req_slp:1;
		unsigned char wifi_open:1;
		unsigned char bt_open:1;
		unsigned char wifi_close:1;
		unsigned char bt_close:1;
		unsigned char gnss_cali_done:1;
	} bit;
};

union PUB_INT_STS0_REG {
	unsigned char reg;
	struct {
		unsigned char mem_save_bin:1;
		unsigned char wakeup_ack:1;
		unsigned char req_slp:1;
		unsigned char wifi_open:1;
		unsigned char bt_open:1;
		unsigned char wifi_close:1;
		unsigned char bt_close:1;
		unsigned char gnss_cali_done:1;
	} bit;
};

struct sdio_int_t {
	unsigned int cp_slp_ctl;
	unsigned int ap_int_cp0;
	unsigned int pub_int_en0;
	unsigned int pub_int_clr0;
	unsigned int pub_int_sts0;
	PUB_INT_ISR pub_int_cb[PUB_INT_MAX];
	struct wakeup_source *pub_int_wakelock;
	struct completion pub_int_completion;
	unsigned int pub_int_num;
	/* 1: power on, 0: power off */
	atomic_t chip_power_on;
	struct wait_queue_head pub_int_done;
};

/* add start, for power save handle */
bool sdio_get_power_notify(void);
void sdio_record_power_notify(bool notify_cb_sts);
void sdio_wait_pub_int_done(void);
/* add end */

int sdio_ap_int_cp0(enum AP_INT_CP_BIT bit);
/* pub int api */
int sdio_pub_int_btwf_en0(void);
int sdio_pub_int_gnss_en0(void);
int sdio_pub_int_regcb(enum PUB_INT_BIT bit,
		PUB_INT_ISR isr_handler);
void sdio_pub_int_poweron(bool state);
int sdio_pub_int_init(int irq);
int sdio_pub_int_deinit(void);
#endif
