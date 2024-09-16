/*
*  Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _BT_EXP_H_
#define _BT_EXP_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/sched.h>
#include <asm/current.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/uio.h>
#include <linux/workqueue.h>

#include "wmt_exp.h"
#include "stp_exp.h"


#define TRUE   1
#define FALSE  0

/* Flags to control BT FW log flow */
#define OFF 0x00
#define ON  0xff

#define PFX                     "[MTK-BT]"
#define BT_LOG_DBG              4
#define BT_LOG_INFO             3
#define BT_LOG_WARN             2
#define BT_LOG_ERR              1
#define RAW_MAX_BYTES           30

static uint8_t raw_buf[RAW_MAX_BYTES * 5 + 10];
extern UINT32 gBtDbgLevel;

#define BT_LOG_PRT_DBG(fmt, arg...)	\
	do { if (gBtDbgLevel >= BT_LOG_DBG) pr_info(PFX "%s: " fmt, __func__, ##arg); } while (0)
#define BT_LOG_PRT_INFO(fmt, arg...)	\
	do { if (gBtDbgLevel >= BT_LOG_INFO) pr_info(PFX "%s: " fmt, __func__, ##arg); } while (0)
#define BT_LOG_PRT_WARN(fmt, arg...)	\
	do { if (gBtDbgLevel >= BT_LOG_WARN) pr_info(PFX "%s: " fmt, __func__, ##arg); } while (0)
#define BT_LOG_PRT_ERR(fmt, arg...)	\
	do { if (gBtDbgLevel >= BT_LOG_ERR) pr_info(PFX "%s: " fmt, __func__, ##arg); } while (0)
#define BT_LOG_PRT_INFO_RATELIMITED(fmt, arg...)	\
	do { if (gBtDbgLevel >= BT_LOG_ERR) pr_info_ratelimited(PFX "%s: " fmt, __func__, ##arg); } while (0)

#define BT_LOG_PRT_DBG_RAW(p, l, fmt, ...)						\
			do {	\
				if (gBtDbgLevel >= BT_LOG_DBG) { \
					int cnt_ = 0;	\
					int len_ = (l <= RAW_MAX_BYTES ? l : RAW_MAX_BYTES);	\
					const unsigned char *ptr = p;	\
					for (cnt_ = 0; cnt_ < len_; ++cnt_) {	\
						if (snprintf(raw_buf+5*cnt_, 6, "0x%02X ", ptr[cnt_]) < 0) {	\
							pr_info("snprintf error\n");	\
							break;	\
						}	\
					}	\
					raw_buf[5*cnt_] = '\0'; \
					if (l <= RAW_MAX_BYTES) {	\
						pr_info(PFX" "fmt"%s\n", ##__VA_ARGS__, raw_buf);	\
					} else {	\
						pr_info(PFX" "fmt"%s (prtail)\n", ##__VA_ARGS__, raw_buf); \
					}	\
				}	\
			} while (0)

#define BT_LOG_PRT_INFO_RAW(p, l, fmt, ...)						\
		do {	\
			if (gBtDbgLevel >= BT_LOG_INFO) {	\
				int cnt_ = 0;	\
				int len_ = (l <= RAW_MAX_BYTES ? l : RAW_MAX_BYTES);	\
				const unsigned char *ptr = p;	\
				for (cnt_ = 0; cnt_ < len_; ++cnt_) {	\
					if (snprintf(raw_buf+5*cnt_, 6, "0x%02X ", ptr[cnt_]) < 0) {	\
						pr_info("snprintf error\n");	\
						break;	\
					}	\
				}	\
				raw_buf[5*cnt_] = '\0'; \
				if (l <= RAW_MAX_BYTES) {	\
					pr_info(PFX" "fmt"%s\n", ##__VA_ARGS__, raw_buf);	\
				} else {	\
					pr_info(PFX" "fmt"%s (prtail)\n", ##__VA_ARGS__, raw_buf); \
				}	\
			}	\
		} while (0)

struct bt_dbg_st {
	bool trx_enable;
	uint16_t trx_opcode;
	struct completion trx_comp;
	void(*trx_cb) (char *buf, int len);
	int rx_len;
	char rx_buf[64];
};

struct pm_qos_ctrl {
	struct semaphore sem;
	struct workqueue_struct *task;
	struct delayed_work work;
	u_int8_t is_hold;
};

/* *****************************************************************************************
 * BT Logger Tool will send 3 levels(Low, SQC and Debug)
 * Driver will not check its range so we can provide capability of extention.
 ******************************************************************************************/
#define DEFAULT_LEVEL 0x02 /* 0x00:OFF, 0x01: LOW POWER, 0x02: SQC, 0x03: DEBUG */

extern int  fw_log_bt_init(void);
extern void fw_log_bt_exit(void);
extern void bt_state_notify(UINT32 on_off);
extern ssize_t send_hci_frame(const PUINT8 buf, size_t count);

#endif

