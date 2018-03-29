/*
 * Rawbulk Driver from VIA Telecom
 *
 * Copyright (C) 2011 VIA Telecom, Inc.
 * Author: Karfield Chen (kfchen@via-telecom.com)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __RAWBULK_H__
#define __RAWBULK_H__

#include <linux/usb.h>
#include <linux/usb/ch9.h>
#include <linux/usb/composite.h>
#include <linux/usb/gadget.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/sched.h>

#define INTF_DESC       0
#define BULKIN_DESC     1
#define BULKOUT_DESC    2
#define MAX_DESC_ARRAY  4

#define NAME_BUFFSIZE   64
#define MAX_ATTRIBUTES    10

#define MAX_TTY_RX          8
#define MAX_TTY_RX_PACKAGE  512
#define MAX_TTY_TX          8
#define MAX_TTY_TX_PACKAGE  512

/* #define CONFIG_EVDO_DT_VIA_SUPPORT */
#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
enum transfer_id {
	RAWBULK_TID_MODEM,
	RAWBULK_TID_ETS,
	RAWBULK_TID_AT,
	RAWBULK_TID_PCV,
	RAWBULK_TID_GPS,
	_MAX_TID
};
#else
enum transfer_id {
	RAWBULK_TID_PCV,
	RAWBULK_TID_MODEM,
	RAWBULK_TID_DUMMY0,
	RAWBULK_TID_AT,
	RAWBULK_TID_GPS,
	RAWBULK_TID_DUMMY1,
	RAWBULK_TID_DUMMY2,
	RAWBULK_TID_ETS,
	_MAX_TID
};
#endif

struct rawbulk_function {
	int transfer_id;
	const char *longname;
	const char *shortname;
	struct device *dev;

	/* Controls */
	spinlock_t lock;
	int enable:1;
	int activated:1;	/* set when usb enabled */
	int tty_opened:1;

	int initialized:1;	/* init-flag for activator worker */
	struct work_struct activator;	/* asynic transaction starter */

	struct wake_lock keep_awake;

	/* USB Gadget related */
	struct usb_function function;
	struct usb_composite_dev *cdev;
	struct usb_ep *bulk_out, *bulk_in;

	int rts_state;		/* Handshaking pins (outputs) */
	int dtr_state;
	int cts_state;		/* Handshaking pins (inputs) */
	int dsr_state;
	int dcd_state;
	int ri_state;

	/* TTY related */
	struct tty_struct *tty;
	int tty_minor;
	struct tty_port port;
	spinlock_t tx_lock;
	struct list_head tx_free;
	struct list_head tx_inproc;
	spinlock_t rx_lock;
	struct list_head rx_free;
	struct list_head rx_inproc;
	struct list_head rx_throttled;
	unsigned int last_pushed;
	struct workqueue_struct *tty_push_wq;
	struct work_struct tty_push_work;
	int tty_throttled;
	/* Transfer Controls */
	int nups;
	int ndowns;
	int upsz;
	int downsz;
	/* int splitsz; */
	int autoreconn;
	int pushable;		/* Set to use push-way for upstream */
	int cbp_reset;
	/* Descriptors and Strings */
	struct usb_descriptor_header *fs_descs[MAX_DESC_ARRAY];
	struct usb_descriptor_header *hs_descs[MAX_DESC_ARRAY];
	struct usb_string string_defs[2];
	struct usb_gadget_strings string_table;
	struct usb_gadget_strings *strings[2];
	struct usb_interface_descriptor interface;
	struct usb_endpoint_descriptor fs_bulkin_endpoint;
	struct usb_endpoint_descriptor hs_bulkin_endpoint;
	struct usb_endpoint_descriptor fs_bulkout_endpoint;
	struct usb_endpoint_descriptor hs_bulkout_endpoint;

	/* Sysfs Accesses */
	int max_attrs;
	struct device_attribute attr[MAX_ATTRIBUTES];
};

typedef void (*rawbulk_autoreconn_callback_t) (int transfer_id);

/* bind/unbind host interfaces */
int rawbulk_bind_sdio_channel(int transfer_id);
void rawbulk_unbind_sdio_channel(int transfer_id);

struct rawbulk_function *rawbulk_lookup_function(int transfer_id);

/* rawbulk tty io */
int rawbulk_register_tty(struct rawbulk_function *fn);
void rawbulk_unregister_tty(struct rawbulk_function *fn);

int rawbulk_tty_stop_io(struct rawbulk_function *fn);
int rawbulk_tty_start_io(struct rawbulk_function *fn);
int rawbulk_tty_alloc_request(struct rawbulk_function *fn);
void rawbulk_tty_free_request(struct rawbulk_function *fn);

/* bind/unbind for gadget */
int rawbulk_bind_function(int transfer_id, struct usb_function *function,
			  struct usb_ep *bulk_out, struct usb_ep *bulk_in,
			  rawbulk_autoreconn_callback_t autoreconn_callback);
void rawbulk_unbind_function(int trasfer_id);
int rawbulk_check_enable(struct rawbulk_function *fn);
void rawbulk_disable_function(struct rawbulk_function *fn);

/* operations for transactions */
int rawbulk_start_transactions(int transfer_id, int nups, int ndowns, int upsz, int downsz);
void rawbulk_stop_transactions(int transfer_id);

int rawbulk_push_upstream_buffer(int transfer_id, const void *buffer, unsigned int length);
int rawbulk_transfer_statistics(int transfer_id, char *buf);
int rawbulk_transfer_state(int transfer_id);

extern int modem_dtr_set(int on, int low_latency);
extern int modem_dcd_state(void);
extern int sdio_buffer_push(int port_num, const unsigned char *buf, int count);
extern int sdio_rawbulk_intercept(int port_num, unsigned int inception);

/* debug mechanism */
extern unsigned int c2k_usb_dbg_level;	/* refer to rawbulk_transfer.c */
static inline int c2k_dbg_level(unsigned level)
{
	return c2k_usb_dbg_level >= level;
}

#define C2K_LOG_EMERG			0
#define C2K_LOG_ALERT			1
#define C2K_LOG_CRIT			2
#define C2K_LOG_ERR				3
#define C2K_LOG_WARN			4
#define C2K_LOG_NOTICE		5
#define C2K_LOG_INFO			6
#define C2K_LOG_DBG				7

#define C2K_USB_DBG_ON
#ifdef C2K_USB_DBG_ON
#define C2K_ERR(format, args...) do {if (c2k_dbg_level(C2K_LOG_ERR)) \
	pr_warn("C2K_USB_ERR,<%s %d>, " format , __func__, __LINE__ , ## args);  } \
while (0)
#define C2K_WARN(format, args...) do {if (c2k_dbg_level(C2K_LOG_WARN)) \
	pr_warn("C2K_USB_WARN,<%s %d>, " format , __func__, __LINE__ , ## args);  } \
while (0)
#define C2K_NOTE(format, args...) do {if (c2k_dbg_level(C2K_LOG_NOTICE)) \
	pr_warn("C2K_USB_NOTE,<%s %d>, " format , __func__, __LINE__ , ## args);  } \
while (0)
#define C2K_INFO(format, args...) do {if (c2k_dbg_level(C2K_LOG_INFO)) \
	pr_warn("C2K_USB_INFO,<%s %d>, " format , __func__, __LINE__ , ## args);  } \
while (0)
#define C2K_DBG(format, args...) do {if (c2k_dbg_level(C2K_LOG_DBG)) \
	pr_warn("C2K_USB_DBG,<%s %d>, " format , __func__, __LINE__ , ## args);  } \
while (0)
#else
#define C2K_ERR(format, args...) do {} while (0)
#define C2K_WARN(format, args...) do {} while (0)
#define C2K_NOTE(format, args...) do {} while (0)
#define C2K_INFO(format, args...) do {} while (0)
#define C2K_DBG(format, args...) do {} while (0)
#endif

extern unsigned int upstream_data[_MAX_TID];
extern unsigned int upstream_cnt[_MAX_TID];
extern unsigned int total_drop[_MAX_TID];
extern unsigned int alloc_fail[_MAX_TID];
extern unsigned int total_tran[_MAX_TID];
extern unsigned long volatile jiffies;


#ifdef C2K_USB_UT
extern int delay_set;
extern int ut_err;
#endif

#endif				/* __RAWBULK_HEADER_FILE__ */
