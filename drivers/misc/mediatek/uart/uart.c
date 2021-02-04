/*
 * Copyright (C) 2019 MediaTek Inc.
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
/******************************************************************************
 * Dependency
 *****************************************************************************/
#if defined(CONFIG_MTK_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ		/*used in serial_core.h */
#endif
/*---------------------------------------------------------------------------*/
#include <generated/autoconf.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/timer.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/io.h>
#include <asm/irq.h>
#include <linux/irq.h>
/* #include <asm/scatterlist.h> */
#include <mt-plat/dma.h>
/* #include <mach/mt_clkmgr.h> */
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/syscore_ops.h>
#include "include/mtk_uart.h"
#include "include/mtk_uart_intf.h"
#include <linux/version.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/ratelimit.h>

#include "include/mtk_uart_internal.h"

#define TTY_FLIP_ARG(a)  ((a)->port)

spinlock_t mtk_console_lock;
spinlock_t mtk_uart_bt_lock;

struct mtk_uart *console_port;
struct mtk_uart *bt_port;
/*---------------------------------------------------------------------------*/
#define HW_FLOW_CTRL_PORT(uart) (uart->setting->hw_flow)
/*---------------------------------------------------------------------------*/
#if defined(ENABLE_VFIFO)
/*---------------------------------------------------------------------------*/
static DEFINE_SPINLOCK(mtk_uart_vfifo_port_lock);
/*---------------------------------------------------------------------------*/
#define VFIFO_INIT_RX(c, i, n) \
	{.ch = (c), .size = (n), \
	.trig = VFF_RX_THRE(n), \
	.type = UART_RX_VFIFO, \
	.port = NULL, .addr = NULL,             \
	.entry = ATOMIC_INIT(0), \
	.reg_cb = ATOMIC_INIT(0), \
	.iolock = __SPIN_LOCK_UNLOCKED(mtk_uart_vfifo_port[i].lock)}
/*---------------------------------------------------------------------------*/
#define VFIFO_INIT_TX(c, i, n) \
	{.ch = (c), \
	.size = (n), \
	.trig = VFF_TX_THRE(n), \
	.type = UART_TX_VFIFO, \
	.port = NULL,         \
	.addr = NULL, \
	.entry = ATOMIC_INIT(0), \
	.reg_cb = ATOMIC_INIT(0), \
	.iolock = __SPIN_LOCK_UNLOCKED(mtk_uart_vfifo_port[i].lock)}
/*---------------------------------------------------------------------------*/
static struct mtk_uart_vfifo mtk_uart_vfifo_port[] = {
	VFIFO_INIT_TX(P_DMA_UART1_TX, 0, C_UART1_VFF_TX_SIZE),
	VFIFO_INIT_RX(P_DMA_UART1_RX, 1, C_UART1_VFF_RX_SIZE),
	VFIFO_INIT_TX(P_DMA_UART2_TX, 2, C_UART2_VFF_TX_SIZE),
	VFIFO_INIT_RX(P_DMA_UART2_RX, 3, C_UART2_VFF_RX_SIZE),
	VFIFO_INIT_TX(P_DMA_UART3_TX, 4, C_UART3_VFF_TX_SIZE),
	VFIFO_INIT_RX(P_DMA_UART3_RX, 5, C_UART3_VFF_RX_SIZE),
#if 0 /*MT6589 only 6 DMA channel for UART */
	VFIFO_INIT_TX(P_DMA_UART4_TX, 6, C_UART4_VFF_TX_SIZE),
	VFIFO_INIT_RX(P_DMA_UART4_RX, 7, C_UART4_VFF_RX_SIZE),
#endif
};

/*---------------------------------------------------------------------------*/
#endif				/*ENABLE_VFIFO */
/*---------------------------------------------------------------------------*/
/* uart control blocks */
struct mtk_uart mtk_uarts[UART_NR];
static unsigned int uart_freeze_enable[UART_NR] = { 0 };

struct uart_history_data {
	int index;
	int offset;
	u64 last_update;
	unsigned char *buffer;
};
#define UART_HISTORY_DATA_SIZE 4096
static struct uart_history_data tx_history;
static struct uart_history_data rx_history;
/*---------------------------------------------------------------------------*/
static int mtk_uart_init_ports(void);
static void mtk_uart_stop_tx(struct uart_port *port);
/******************************************************************************
 * SYSFS support
 *****************************************************************************/
#if defined(ENABLE_SYSFS)
/*---------------------------------------------------------------------------*/
const struct sysfs_ops mtk_uart_sysfs_ops = {
	.show = mtk_uart_attr_show,
	.store = mtk_uart_attr_store,
};

/*---------------------------------------------------------------------------*/
struct mtuart_entry {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj, char *page);
	ssize_t (*store)(struct kobject *kobj, const char *page, size_t size);
};

/*---------------------------------------------------------------------------*/
struct mtuart_entry debug_entry = {
	{.name = "debug", .mode = 0644},
	mtk_uart_debug_show,
	mtk_uart_debug_store,
};

/*---------------------------------------------------------------------------*/
struct mtuart_entry sysrq_entry = {
	{.name = "sysrq", .mode = 0644},
	mtk_uart_sysrq_show,
	mtk_uart_sysrq_store,
};

/*---------------------------------------------------------------------------*/
struct mtuart_entry vffsz_entry = {
	{.name = "vffsz", .mode = 0644},
	mtk_uart_vffsz_show,
	mtk_uart_vffsz_store,
};

/*---------------------------------------------------------------------------*/
struct mtuart_entry conse_entry = {
	{.name = "conse", .mode = 0644},
	mtk_uart_conse_show,
	mtk_uart_conse_store,
};

/*---------------------------------------------------------------------------*/
struct mtuart_entry vff_en_entry = {
	{.name = "vff_en", .mode = 0644},
	mtk_uart_vff_en_show,
	mtk_uart_vff_en_store,
};

/*---------------------------------------------------------------------------*/
struct mtuart_entry lsr_status_entry = {
	{.name = "lsr_status", .mode = 0644},
	mtk_uart_lsr_status_show,
	mtk_uart_lsr_status_store,
};

/*---------------------------------------------------------------------------*/
struct mtuart_entry history_entry = {
	{.name = "history", .mode = 0644},
	mtk_uart_history_show,
	mtk_uart_history_store,
};

/*---------------------------------------------------------------------------*/
struct attribute *mtk_uart_attributes[] = {
	&conse_entry.attr,	/*console setting */
#if defined(ENABLE_DEBUG)
	&debug_entry.attr,
	&sysrq_entry.attr,
	&vffsz_entry.attr,
	&vff_en_entry.attr,
	&lsr_status_entry.attr,
	&history_entry.attr,
#endif
	NULL,
};

/*---------------------------------------------------------------------------*/
struct kobj_type mtk_uart_ktype = {
	.sysfs_ops = &mtk_uart_sysfs_ops,
	.default_attrs = mtk_uart_attributes,
};

/*---------------------------------------------------------------------------*/
struct mtuart_sysobj {
	struct kobject kobj;
	atomic_t sysrq;
	atomic_t vffLen[UART_NR * UART_VFIFO_NUM];
	atomic_t console_enable;
} mtk_uart_sysobj = {
.console_enable = ATOMIC_INIT(1),};

/*---------------------------------------------------------------------------*/
int mtk_uart_sysfs(void)
{
	struct mtuart_sysobj *obj = &mtk_uart_sysobj;
#if defined(ENABLE_VFIFO)
	int idx;
#endif

	memset(&obj->kobj, 0x00, sizeof(obj->kobj));
#if defined(CONFIG_MAGIC_SYSRQ)
	atomic_set(&obj->sysrq, 1);
#else
	atomic_set(&obj->sysrq, 0);
#endif
#if defined(ENABLE_VFIFO)
	for (idx = 0; idx < ARRAY_SIZE(obj->vffLen) &&
		idx < ARRAY_SIZE(mtk_uart_vfifo_port); idx++)
		atomic_set(&obj->vffLen[idx], mtk_uart_vfifo_port[idx].size);
#endif
	atomic_set(&obj->console_enable, 1);

	obj->kobj.parent = kernel_kobj;
	if (kobject_init_and_add(&obj->kobj, &mtk_uart_ktype, NULL, "mtuart")) {
		kobject_put(&obj->kobj);
		return -ENOMEM;
	}
	kobject_uevent(&obj->kobj, KOBJ_ADD);
	return 0;
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_attr_show(struct kobject *kobj,
			 struct attribute *attr, char *buffer)
{
	struct mtuart_entry *entry =
		container_of(attr, struct mtuart_entry, attr);

	return entry->show(kobj, buffer);
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_attr_store(struct kobject *kobj,
struct attribute *attr, const char *buffer, size_t size)
{
	struct mtuart_entry *entry =
		container_of(attr, struct mtuart_entry, attr);

	return entry->store(kobj, buffer, size);
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_debug_show(struct kobject *kobj, char *buffer)
{
	int remain = PAGE_SIZE;
	int len;
	char *ptr = buffer;
	int idx;
	unsigned int evt_mask;

	for (idx = 0; idx < UART_NR; idx++) {
		evt_mask = (unsigned int)get_uart_evt_mask(idx);
		len = scnprintf(ptr, remain, "0x%2x\n", evt_mask);
		ptr += len;
		remain -= len;
	}
	return PAGE_SIZE - remain;
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_debug_store(struct kobject *kobj,
		const char *buffer, size_t size)
{
#if (UART_NR < 5)

	int a, b, c, d;
	int res = sscanf(buffer, "0x%x 0x%x 0x%x 0x%x", &a, &b, &c, &d);

	if (res != 4) {
		MSG_ERR("%s: expect 4 numbers\n", __func__);
	} else {
		set_uart_evt_mask(0, a);
		set_uart_evt_mask(1, b);
		set_uart_evt_mask(2, c);
		set_uart_evt_mask(3, d);
	}
#else
	int a, b, c, d, e;
	int res = sscanf(buffer, "0x%x 0x%x 0x%x 0x%x 0x%x",
			&a, &b, &c, &d, &e);

	if (res != 5) {
		MSG_ERR("%s: expect 5 numbers\n", __func__);
	} else {
		set_uart_evt_mask(0, a);
		set_uart_evt_mask(1, b);
		set_uart_evt_mask(2, c);
		set_uart_evt_mask(3, d);
		set_uart_evt_mask(4, e);
	}
#endif
	return size;
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_sysrq_show(struct kobject *kobj, char *buffer)
{
	struct mtuart_sysobj *obj =
		container_of(kobj, struct mtuart_sysobj, kobj);

	return scnprintf(buffer, PAGE_SIZE, "%d\n", atomic_read(&obj->sysrq));
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_sysrq_store(struct kobject *kobj,
	const char *buffer, size_t size)
{
	struct mtuart_sysobj *obj =
		container_of(kobj, struct mtuart_sysobj, kobj);
	int a;
	int res = sscanf(buffer, "%d\n", &a);

	if (res != 1)
		MSG_ERR("%s: expect 1 number\n", __func__);
	else
		atomic_set(&obj->sysrq, a);
	return size;
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_vffsz_show(struct kobject *kobj, char *buffer)
{
	ssize_t len = 0;
#if defined(ENABLE_VFIFO)
	struct mtuart_sysobj *obj =
			container_of(kobj, struct mtuart_sysobj, kobj);
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(obj->vffLen); idx++)
		len += scnprintf(buffer + len, PAGE_SIZE - len,
		"[%02d] %4d\n", idx, atomic_read(&obj->vffLen[idx]));
#endif
	return len;
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_vffsz_store(struct kobject *kobj,
	const char *buffer, size_t size)
{
#if defined(ENABLE_VFIFO)
	struct mtuart_sysobj *obj =
			container_of(kobj, struct mtuart_sysobj, kobj);
	int idx, sz;

	if (sscanf(buffer, "%d %d", &idx, &sz) != 2)
		MSG_ERR("%s: expect 2 variables\n", __func__);
	else if (idx >= ARRAY_SIZE(obj->vffLen) || (sz % 8 != 0))
		MSG_ERR("%s: invalid args %d, %d\n", __func__, idx, sz);
	else
		atomic_set(&obj->vffLen[idx], sz);
#endif
	return size;
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_conse_show(struct kobject *kobj, char *buffer)
{
	struct mtuart_sysobj *obj =
			container_of(kobj, struct mtuart_sysobj, kobj);

	return scnprintf(buffer, PAGE_SIZE, "%d\n",
			atomic_read(&obj->console_enable));
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_conse_store(struct kobject *kobj,
	const char *buffer, size_t size)
{
	struct mtuart_sysobj *obj =
		container_of(kobj, struct mtuart_sysobj, kobj);
	int enable;

	if (kstrtoint(buffer, 10, &enable))
		MSG_ERR("%s: expect 1 variables\n", __func__);
	else
		atomic_set(&obj->console_enable, enable);

	return size;
}

/*---------------------------------------------------------------------------*/
#endif				/*ENABLE_SYSFS */
/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_vff_en_show(struct kobject *kobj, char *buffer)
{
	int remain = PAGE_SIZE;
	int len;
	char *ptr = buffer;
	int idx;
	struct mtk_uart_setting *uart_setting;

	for (idx = 0; idx < UART_NR; idx++) {
		uart_setting = get_uart_default_settings(idx);
		len = scnprintf(ptr,
				remain,
				"tx%d_m:%2x rx%d_m:%2x\n",
				idx, (unsigned int)uart_setting->tx_mode,
				idx, (unsigned int)uart_setting->rx_mode);
		ptr += len;
		remain -= len;
	}
	return PAGE_SIZE - remain;
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_vff_en_store(struct kobject *kobj,
	const char *buffer, size_t size)
{
#if (UART_NR < 5)
	int u1_tx, u1_rx, u2_tx, u2_rx;
	int u3_tx, u3_rx, u4_tx, u4_rx;
	struct mtk_uart_setting *uart_setting;
	int res = sscanf(buffer, "%x %x %x %x %x %x %x %x",
			 &u1_tx, &u1_rx, &u2_tx, &u2_rx,
			 &u3_tx, &u3_rx, &u4_tx, &u4_rx);

	if (res != 8) {
		MSG_ERR("%s: expect 8 numbers\n", __func__);
	} else {
		uart_setting = get_uart_default_settings(0);
		uart_setting->tx_mode = u1_tx;
		uart_setting->rx_mode = u1_rx;
		uart_setting = get_uart_default_settings(1);
		uart_setting->tx_mode = u2_tx;
		uart_setting->rx_mode = u2_rx;
		uart_setting = get_uart_default_settings(2);
		uart_setting->tx_mode = u3_tx;
		uart_setting->rx_mode = u3_rx;
		uart_setting = get_uart_default_settings(3);
		uart_setting->tx_mode = u4_tx;
		uart_setting->rx_mode = u4_rx;
	}
	return size;
#else
	int u1_tx, u1_rx, u2_tx, u2_rx;
	int u3_tx, u3_rx, u4_tx, u4_rx, u5_tx, u5_rx;
	struct mtk_uart_setting *uart_setting;
	int res = sscanf(buffer, "%x %x %x %x %x %x %x %x %x %x",
			 &u1_tx, &u1_rx, &u2_tx, &u2_rx, &u3_tx, &u3_rx,
			 &u4_tx, &u4_rx, &u5_tx, &u5_rx);

	if (res != 8) {
		MSG_ERR("%s: expect 8 numbers\n", __func__);
	} else {
		uart_setting = get_uart_default_settings(0);
		uart_setting->tx_mode = u1_tx;
		uart_setting->rx_mode = u1_rx;

		uart_setting = get_uart_default_settings(1);
		uart_setting->tx_mode = u2_tx;
		uart_setting->rx_mode = u2_rx;

		uart_setting = get_uart_default_settings(2);
		uart_setting->tx_mode = u3_tx;
		uart_setting->rx_mode = u3_rx;

		uart_setting = get_uart_default_settings(3);
		uart_setting->tx_mode = u4_tx;
		uart_setting->rx_mode = u4_rx;

		uart_setting = get_uart_default_settings(4);
		uart_setting->tx_mode = u5_tx;
		uart_setting->rx_mode = u5_rx;
	}
	return size;
#endif
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_lsr_status_show(struct kobject *kobj, char *buffer)
{
	int remain = PAGE_SIZE;
	int len;
	char *ptr = buffer;
	int idx;
	unsigned long lsr_status;

	for (idx = 0; idx < UART_NR; idx++) {
		lsr_status = get_uart_lsr_status(idx);
		len = scnprintf(ptr, remain,
			"%04x ", (unsigned int)lsr_status);
		ptr += len;
		remain -= len;
		set_uart_lsr_status(idx, 0);
	}
	len = scnprintf(ptr, remain, "\n");
	ptr += len;
	remain -= len;
	return PAGE_SIZE - remain;
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_lsr_status_store(struct kobject *kobj,
	const char *buffer, size_t size)
{
#if (UART_NR < 5)
	int u1_lsr, u2_lsr, u3_lsr, u4_lsr;
	int res = sscanf(buffer, "%x %x %x %x",
			 &u1_lsr, &u2_lsr, &u3_lsr, &u4_lsr);

	if (res != 4) {
		MSG_ERR("%s: expect 4 numbers\n", __func__);
	} else {
		set_uart_lsr_status(0, u1_lsr);
		set_uart_lsr_status(1, u2_lsr);
		set_uart_lsr_status(2, u3_lsr);
		set_uart_lsr_status(3, u4_lsr);
	}
	return size;
#else
	int u1_lsr, u2_lsr, u3_lsr, u4_lsr, u5_lsr;
	int res = sscanf(buffer, "%x %x %x %x %x",
			 &u1_lsr, &u2_lsr, &u3_lsr,
			 &u4_lsr, &u5_lsr);

	if (res != 5) {
		MSG_ERR("%s: expect 5 numbers\n", __func__);
	} else {
		set_uart_lsr_status(0, u1_lsr);
		set_uart_lsr_status(1, u2_lsr);
		set_uart_lsr_status(2, u3_lsr);
		set_uart_lsr_status(3, u4_lsr);
		set_uart_lsr_status(4, u4_lsr);
	}
	return size;
#endif
}

/*---------------------------------------------------------------------------*/
static void uart_mem_dump(int nport, void *start_addr, int len)
{
	unsigned int *curr_p = (unsigned int *)start_addr;
	unsigned char *curr_ch_p;
	int _16_fix_num = len / 16;
	int tail_num = len % 16;
	char buf[16];
	int i, j;

	if (curr_p == NULL) {
		pr_err("[UART%d-DUMP]NULL point to dump!\n", nport);
		return;
	}
	if (len == 0) {
		pr_err("[UART%d-DUMP]Not need to dump\n", nport);
		return;
	}

	pr_debug("[UART%d-DUMP]Base: %p\n", nport, start_addr);
	/* Fix section */
	for (i = 0; i < _16_fix_num; i++) {
		pr_debug("[UART%d-DUMP]%03X: %08X %08X %08X %08X\n",
		       nport, i * 16, *curr_p,
			*(curr_p + 1), *(curr_p + 2), *(curr_p + 3));
		curr_p += 4;
	}

	/* Tail section */
	if (tail_num > 0) {
		curr_ch_p = (unsigned char *)curr_p;
		for (j = 0; j < tail_num; j++) {
			buf[j] = *curr_ch_p;
			curr_ch_p++;
		}
		for (; j < 16; j++)
			buf[j] = 0;
		curr_p = (unsigned int *)buf;
		pr_debug("[UART%d-DUMP]%03X: %08X %08X %08X %08X\n",
		       nport, i * 16, *curr_p,
			   *(curr_p + 1), *(curr_p + 2), *(curr_p + 3));
	}
}

void mtk_uart_dump_history(void)
{
	u64 ts_nsec;
	unsigned long rem_nsec;

	ts_nsec = tx_history.last_update;
	rem_nsec = do_div(ts_nsec, 1000000000);

	pr_debug("UART Tx port %d: %d/%d @[%5lu.%06lu]\n",
	       tx_history.index, tx_history.offset,
		UART_HISTORY_DATA_SIZE,
		(unsigned long)ts_nsec, rem_nsec / 1000);
	uart_mem_dump(tx_history.index, tx_history.buffer,
		UART_HISTORY_DATA_SIZE);

	ts_nsec = rx_history.last_update;
	rem_nsec = do_div(ts_nsec, 1000000000);

	pr_debug("UART Rx port %d: %d/%d @[%5lu.%06lu]\n",
	       rx_history.index, rx_history.offset,
		   UART_HISTORY_DATA_SIZE,
		(unsigned long)ts_nsec, rem_nsec / 1000);
	uart_mem_dump(rx_history.index, rx_history.buffer,
		UART_HISTORY_DATA_SIZE);
}

void update_history_byte(char is_tx, int nport, unsigned char byte)
{
	struct uart_history_data *x_history;

	x_history = is_tx ? &tx_history : &rx_history;

	if (nport == x_history->index) {
		*(x_history->buffer + x_history->offset) = byte;
		x_history->offset++;
		if (x_history->offset == UART_HISTORY_DATA_SIZE)
			x_history->offset = 0;
	}
}

void update_history_time(char is_tx, int nport)
{
	struct uart_history_data *x_history;

	x_history = is_tx ? &tx_history : &rx_history;

	if (nport == x_history->index)
		x_history->last_update = local_clock();
}

void update_history_bulk(char is_tx, int nport, unsigned char *chars, int count)
{
	int room;
	struct uart_history_data *x_history;

	x_history = is_tx ? &tx_history : &rx_history;
	room = UART_HISTORY_DATA_SIZE - x_history->offset;

	if (nport == x_history->index) {
		if (count <= room) {
			memcpy(x_history->buffer + x_history->offset,
					chars, count);
			x_history->offset += count;
			if (x_history->offset == UART_HISTORY_DATA_SIZE)
				x_history->offset = 0;
		} else {
			memcpy(x_history->buffer + x_history->offset,
					chars, room);
			memcpy(x_history->buffer, chars, count - room);
			x_history->offset = count - room;
			if (x_history->offset == UART_HISTORY_DATA_SIZE)
				x_history->offset = 0;
		}
	}
	x_history->last_update = local_clock();
}

ssize_t mtk_uart_history_show(struct kobject *kobj, char *buffer)
{
	int remain = PAGE_SIZE;
	int len;
	char *ptr = buffer;

	len = scnprintf(ptr, remain, "tx(%d):%d; rx(%d):%d\n",
			tx_history.index, tx_history.offset,
			rx_history.index, rx_history.offset);
	ptr += len;
	remain -= len;
	mtk_uart_dump_history();
	return PAGE_SIZE - remain;
}

/*---------------------------------------------------------------------------*/
ssize_t mtk_uart_history_store(struct kobject *kobj,
		const char *buffer, size_t size)
{
	int tx_index, rx_index;
	int res = sscanf(buffer, "%d %d", &tx_index, &rx_index);

	if (res != 2)
		return 0;

	tx_history.index = tx_index;
	rx_history.index = rx_index;
	tx_history.offset = 0;
	tx_history.offset = 0;
	memset(tx_history.buffer, 0, UART_HISTORY_DATA_SIZE);
	memset(rx_history.buffer, 0, UART_HISTORY_DATA_SIZE);
	return size;
}

/* ============================= FIQ ======================================= */
#if (defined(CONFIG_FIQ_DEBUGGER_CONSOLE) && defined(CONFIG_FIQ_DEBUGGER))
#define DEFAULT_FIQ_UART_PORT           (3)
int fiq_console_port = DEFAULT_FIQ_UART_PORT;
/* struct uart_port *p_mtk_uart_port =
 * &(mtk_uarts[DEFAULT_FIQ_UART_PORT].port);
 */
/* EXPORT_SYMBOL(p_mtk_uart_port); */
struct mtk_uart *mt_console_uart = &(mtk_uarts[DEFAULT_FIQ_UART_PORT]);
#endif
/* ========================================================================= */
/* ------------------------------------------------------------------------- */
/* UART Log port switch feature */
static int find_string(char str[], const char *fingerprint, int *offset)
{
	char *curr = str;
	int i = 0;
	int str_len;
	int fingerprint_len;

	if ((str == NULL) || (fingerprint == NULL))
		return 0;
	str_len = strlen(str);
	fingerprint_len = strlen(fingerprint);
	if (str_len < fingerprint_len)
		return 0;
	for (i = 0; i <= (str_len - fingerprint_len); i++) {
		if (strncmp(curr, fingerprint, fingerprint_len) == 0) {
			if (offset != NULL)
				*offset = i;
			return 1;
		}
		curr++;
	}
	return 0;
}

static int find_fingerprint(char str[], int *offset)
{
	/**
	 * This function limitation:
	 * If the ttyMT number large than 9, this function will work abnormal.
	 * For example, ttyMT12 will be recoginzed as ttyMT1
	 */
	static const char * const fingerprint[] = {
		"console=/dev/null",
		"console=ttyMT0",
		"console=ttyMT1",
		"console=ttyMT2",
		"console=ttyMT3",
#if (UART_NR > 4)
		"console=ttyMT4",
#endif
	};
	int i;

	for (i = 0; i < sizeof(fingerprint) / sizeof(char *); i++) {
		if (find_string(str, fingerprint[i], offset) != 0)
			return i;	/* Find it. */
	}
	return -1;		/* Not find */
}

static int modify_fingerprint(char str[], int offset, char new_val)
{
	if (str == NULL)
		return 0;
	/* 14 = strlen("console=ttyMTx"), we modify x to 1~3 */
	str[offset + 14 - 1] = new_val;
	return 1;
}

void adjust_kernel_cmd_line_setting_for_console(char *u_boot_cmd_line,
		char *kernel_cmd_line)
{
	int offset = 0;
	int kernel_console_port_setting = -1;
	int u_boot_console_port_setting = -1;

	//Check u-boot command line setting
	u_boot_console_port_setting =
		find_fingerprint(u_boot_cmd_line, 0);
	if (-1 == u_boot_console_port_setting) {
	//printf("U-boot does not have console setting, return\n");
		return;
	}

	/* U-boot has console setting,
	 * check kernel console setting
	 */
	kernel_console_port_setting =
		find_fingerprint(kernel_cmd_line, &offset);
	if (-1 == kernel_console_port_setting) {
	//printf("Kernel does not have console setting, return\n");
		goto _Exit;
	}

	/**
	 * Both U-boot and Kernel has console setting.
	 * 1. If the settings are same, return directly;
	 * 2. If kernel console setting is null, then use kernel setting
	 * 3. If u-boot console setting is null, then use kernel setting
	 * 4. If kernel console setting is not null, then use u-boot setting
	 */
	if (u_boot_console_port_setting == kernel_console_port_setting) {
		/* printf("Same console setting, return\n"); */
		goto _Exit;
	}
	if (kernel_console_port_setting == 0) {
		/* printf("Kernel console setting is null,
		 * use kernel setting, return\n");
		 */
		goto _Exit;
	}
	if (u_boot_console_port_setting == 0) {
		/* printf("U-boot console setting is null,
		 * use kernel setting, return\n");
		 */
		goto _Exit;
	}
	/* Enter here, it means both kernel and u-boot
	 * console setting are not null, using u-boot setting
	 */
	switch (u_boot_console_port_setting) {
	case 1:		/* Using ttyMT0 */
		modify_fingerprint(kernel_cmd_line, offset, '0');
		break;
	case 2:		/* Using ttyMT1 */
		modify_fingerprint(kernel_cmd_line, offset, '1');
		break;
	case 3:		/* Using ttyMT2 */
		modify_fingerprint(kernel_cmd_line, offset, '2');
		break;
	case 4:		/* Using ttyMT3 */
		modify_fingerprint(kernel_cmd_line, offset, '3');
		break;
	case 5:		/* Using ttyMT4 */
		modify_fingerprint(kernel_cmd_line, offset, '4');
		break;
	default:
		/* Do nothing */
		break;
	}

 _Exit:
	kernel_console_port_setting = 0;
#if (defined(CONFIG_FIQ_DEBUGGER_CONSOLE) && defined(CONFIG_FIQ_DEBUGGER))
	kernel_console_port_setting =
		find_fingerprint(kernel_cmd_line, &offset);
	if (-1 == kernel_console_port_setting) {
		/* printf("Kernel does not have console setting, return\n"); */
		return;
	}

	if (kernel_console_port_setting > 0) {
		fiq_console_port = (kernel_console_port_setting - 1);
		mt_console_uart = &(mtk_uarts[fiq_console_port]);
	}
#endif
}

/* ======================================================================== */
/* -------------------------------- PDN ----------------------------------- */
unsigned int mtk_uart_pdn_enable(char *port, int enable)
{
	int str_len;
	int port_num;

	if (port == NULL)
		return -1;

	str_len = strlen(port);
	if (str_len != 6) {
		MSG_ERR("Length mismatch! len=%d\n", str_len);
		return -1;
	}

	if (find_string(port, "ttyMT", 0) == 0) {
		MSG_ERR("Format mismatch! str=%s\n", port);
		return -1;
	}

	port_num = port[str_len - 1] - '0';
	if (port_num >= UART_NR) {
		MSG_ERR("wrong port:%d\n", port_num);
		return -1;
	}
	bt_port = &mtk_uarts[port_num];
	if (enable)
		mtk_uart_enable_dpidle(bt_port);
	else
		mtk_uart_disable_dpidle(bt_port);
	return 0;
}

unsigned int mtk_uart_freeze_enable(char *port, int enable)
{
	int str_len;
	int port_num;

	if (port == NULL)
		return -1;

	str_len = strlen(port);
	if (str_len != 6) {
		MSG_ERR("Length mismatch! len=%d\n", str_len);
		return -1;
	}

	if (find_string(port, "ttyMT", 0) == 0) {
		MSG_ERR("Format mismatch! str=%s\n", port);
		return -1;
	}

	port_num = port[str_len - 1] - '0';
	if (port_num >= UART_NR) {
		MSG_ERR("wrong port:%d\n", port_num);
		return -1;
	}

	if (enable)
		uart_freeze_enable[port_num] = 1;
	else
		uart_freeze_enable[port_num] = 0;
	return 0;
}
EXPORT_SYMBOL(mtk_uart_freeze_enable);

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_MTK_SERIAL_CONSOLE
/*---------------------------------------------------------------------------*/
static void mtk_uart_console_write(struct console *co,
		const char *s, unsigned int count)
{
	/* Notice:
	 * (1) The function is called by printk,
	 * hence, spin lock can not be used
	 * (2) don't care vfifo setting
	 */
#define CONSOLE_RETRY (5000)
	int i;
	struct mtk_uart *uart;
	u32 cnt = 0;
	unsigned long flags;

	if (co->index >= UART_NR || !(co->flags & CON_ENABLED)
			|| !atomic_read(&mtk_uart_sysobj.console_enable))
		return;

	uart = &mtk_uarts[co->index];
	for (i = 0; i < (int)count; i++) {
		cnt = 0;
		while (!mtk_uart_write_allow(uart)) {
			barrier();
			if (cnt++ >= CONSOLE_RETRY) {
				uart->timeout_count++;
				return;
			}
		}
		spin_lock_irqsave(&mtk_console_lock, flags);
		mtk_uart_write_byte(uart, s[i]);
		spin_unlock_irqrestore(&mtk_console_lock, flags);

		if (s[i] == '\n') {
			cnt = 0;
			while (!mtk_uart_write_allow(uart)) {
				barrier();
				if (cnt++ >= CONSOLE_RETRY) {
					uart->timeout_count++;
					return;
				}
			}
			spin_lock_irqsave(&mtk_console_lock, flags);
			mtk_uart_write_byte(uart, '\r');
			spin_unlock_irqrestore(&mtk_console_lock, flags);
		}
	}
}

/*---------------------------------------------------------------------------*/
static int __init mtk_uart_console_setup(struct console *co, char *options)
{
	struct mtk_uart *uart;
	struct uart_port *port;
	int baud = 115200;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';
	int ret;

	pr_debug("[UART]mtk console setup : co->index %d options:%s\n",
				co->index, options);

	if (co->index >= UART_NR)
		co->index = 0;

	uart = &mtk_uarts[co->index];
	port = (struct uart_port *)uart;

	console_port = uart;

	mtk_uart_console_setting_switch(uart);

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	ret = uart_set_options(port, co, baud, parity, bits, flow);
	pr_debug("[UART]mtk console setup: uart_set_option port(%d) ",
				co->index);
	pr_debug("baud(%d) parity(%c) bits(%d) flow(%c) - ret(%d)\n",
				baud, parity, bits, flow, ret);

	pr_debug("[UART]mtk setting: (%d, %d, %d, %lu, %lu)\n",
	       uart->tx_mode, uart->rx_mode, uart->dma_mode,
		   uart->tx_trig, uart->rx_trig);
	/* mtk_uart_power_up(uart); */
	return ret;
}

/*---------------------------------------------------------------------------*/
static struct uart_driver mtk_uart_drv;
static struct console mtk_uart_console = {
	.name = "ttyMT",
#if !defined(CONFIG_MTK_SERIAL_MODEM_TEST)
	/*don't configure UART4 as console */
	.write = mtk_uart_console_write,
	.setup = mtk_uart_console_setup,
#endif
	.device = uart_console_device,
	.flags = CON_PRINTBUFFER,
	.index = -1,
	.data = &mtk_uart_drv,
};

/*---------------------------------------------------------------------------*/
static int __init mtk_uart_console_init(void)
{
	int err = mtk_uart_init_ports();

	if (!err)
		register_console(&mtk_uart_console);
	return err;
}

/*---------------------------------------------------------------------------*/
console_initcall(mtk_uart_console_init);
/*---------------------------------------------------------------------------*/
static int __init mtk_late_console_init(void)
{
	if (!(mtk_uart_console.flags & CON_ENABLED))
		register_console(&mtk_uart_console);

	return 0;
}

/*---------------------------------------------------------------------------*/
late_initcall(mtk_late_console_init);
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_MTK_SERIAL_CONSOLE */
/******************************************************************************
 * Virtual FIFO implementation
 *****************************************************************************/
#if defined(ENABLE_VFIFO)
/*---------------------------------------------------------------------------*/
static int mtk_uart_vfifo_del_dbgbuf(struct mtk_uart_vfifo *vfifo)
{
#if defined(ENABLE_VFIFO_DEBUG)
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(vfifo->dbg); idx++) {
		if (vfifo->dbg[idx].dat != 0)
			kfree(vfifo->dbg[idx].dat);
		vfifo->dbg[idx].dat = NULL;
		vfifo->dbg[idx].idx = 0;
		vfifo->dbg[idx].len = 0;
	}
#endif
	return 0;
}

/*---------------------------------------------------------------------------*/
static int mtk_uart_vfifo_new_dbgbuf(struct mtk_uart_vfifo *vfifo)
{
#if defined(ENABLE_VFIFO_DEBUG)
	int idx;

	for (idx = 0; idx < ARRAY_SIZE(vfifo->dbg); idx++) {
		if (vfifo->dbg[idx].dat != 0)
			kfree(vfifo->dbg[idx].dat);
		vfifo->dbg[idx].idx = 0;
		vfifo->dbg[idx].len = vfifo->size;
		vfifo->dbg[idx].dat = kzalloc(vfifo->dbg[idx].len, GFP_ATOMIC);
		if (!vfifo->dbg[idx].dat) {
			mtk_uart_vfifo_del_dbgbuf(vfifo);
			return -ENOMEM;
		}
	}
#endif
	return 0;
}

/*---------------------------------------------------------------------------*/
static int mtk_uart_vfifo_create(struct mtk_uart *uart)
{/*NOTE: please save the phyiscal address in vff->dmahd */
	struct mtk_uart_vfifo *vfifo;
	int idx, err = 0;

	MSG_FUNC_ENTRY();

	if (!uart->setting->vff) {
		MSG_RAW("[UART%2d] not support VFF, Cancel alloc\n",
			uart->nport);
		return err;
	}

	MSG_RAW("[UART%2d] create\n", uart->nport);

	for (idx = uart->nport * 2; idx < uart->nport * 2 + 2; idx++) {
		vfifo = &mtk_uart_vfifo_port[idx];
		MSG_RAW("[UART%2d] idx=%2d\n", uart->nport, idx);
		if (vfifo->size) {
			vfifo->addr =
				dma_alloc_coherent(uart->port.dev,
				vfifo->size, &vfifo->dmahd, GFP_DMA);
			/* MSG_RAW("Address: virt = 0x%p, phys = 0x%llx\n",
			 * vfifo->addr, vfifo->dmahd);
			 */
		} else {
			vfifo->addr = NULL;
		}
		if (vfifo->size && !vfifo->addr) {
			err = -ENOMEM;
			break;
		}
		err = mtk_uart_vfifo_new_dbgbuf(vfifo);
		if (err)
			break;
		MSG_RAW("[%2d] %p (%04d) ;\n", idx, vfifo->addr, vfifo->size);
	}
	MSG_RAW("\n");
	return err;
}

/*---------------------------------------------------------------------------*/
static int mtk_uart_vfifo_delete(struct mtk_uart *uart)
{
	struct mtk_uart_vfifo *vfifo;
	int idx;

	MSG_FUNC_ENTRY();

	if (!uart->setting->vff) {
		MSG_RAW("[UART%2d] not support VFF, Cancel free\n",
				uart->nport);
		return 0;
	}

	MSG_RAW("[UART%2d] delete", uart->nport);
	for (idx = uart->nport * 2; idx < uart->nport * 2 + 2; idx++) {
		vfifo = &mtk_uart_vfifo_port[idx];
		if (vfifo->addr)
			dma_free_coherent(uart->port.dev, vfifo->size,
				vfifo->addr, vfifo->dmahd);
		mtk_uart_vfifo_del_dbgbuf(vfifo);
		MSG_RAW("[%2d] %p (%04d) ;", idx, vfifo->addr, vfifo->size);
		vfifo->addr = NULL;
	}
	MSG_RAW("\n");
	return 0;
}

/*---------------------------------------------------------------------------*/
int mtk_uart_vfifo_prepare(struct mtk_uart *uart)
{
	struct mtuart_sysobj *obj = &mtk_uart_sysobj;
	struct mtk_uart_vfifo *tport, *rport;
	int tx = uart->nport << 1;
	int rx = tx + 1;

	MSG_FUNC_ENTRY();

	if (uart->nport >= UART_NR) {
		MSG_ERR("wrong port:%d\n", uart->nport);
		return -EINVAL;
	} else if (uart->setting->vff == FALSE) {
		MSG_ERR("Port :%d not support vfifo\n", uart->nport);
		return -EINVAL;
	}
	tport = &mtk_uart_vfifo_port[tx];
	rport = &mtk_uart_vfifo_port[rx];
	if ((atomic_read(&obj->vffLen[tx]) == tport->size)
			&& (atomic_read(&obj->vffLen[rx]) == rport->size))
		return 0;
	MSG_RAW("re-alloc +\n");
	mtk_uart_vfifo_delete(uart);
	tport->size = atomic_read(&obj->vffLen[tx]);
	tport->trig = VFF_TX_THRE(tport->size);
	rport->size = atomic_read(&obj->vffLen[rx]);
	rport->trig = VFF_RX_THRE(rport->size);
	mtk_uart_vfifo_create(uart);
	MSG_RAW("re-alloc -\n");
	return 0;
}

/*---------------------------------------------------------------------------*/
static struct mtk_uart_vfifo *mtk_uart_vfifo_alloc(struct mtk_uart *uart,
			int type)
{
	struct mtk_uart_vfifo *vfifo = NULL;
	unsigned long flags;

	spin_lock_irqsave(&mtk_uart_vfifo_port_lock, flags);

	MSG(INFO, "(%d, %d)", uart->nport, type);

	if ((uart->nport >= (ARRAY_SIZE(mtk_uart_vfifo_port) / 2))
			|| (type >= UART_VFIFO_NUM))
		vfifo = NULL;
	else
		vfifo = &mtk_uart_vfifo_port[2 * uart->nport + type];

	if (vfifo && vfifo->addr == NULL)
		vfifo = NULL;
	if (vfifo)
		MSG(INFO, "alloc vfifo-%d[%d](%p)\n",
			uart->nport, vfifo->size, vfifo->addr);

	spin_unlock_irqrestore(&mtk_uart_vfifo_port_lock, flags);
	return vfifo;

}

/*---------------------------------------------------------------------------*/
static void mtk_uart_vfifo_free(struct mtk_uart *uart,
		struct mtk_uart_vfifo *vfifo)
{
	unsigned long flags;

	if (vfifo) {
		spin_lock_irqsave(&mtk_uart_vfifo_port_lock, flags);
		vfifo->dma = NULL;
		vfifo->cur = NULL;
		vfifo->dbgidx = 0;
		spin_unlock_irqrestore(&mtk_uart_vfifo_port_lock, flags);
	}
}

/*---------------------------------------------------------------------------*/
static unsigned int mtk_uart_vfifo_write_allow(struct mtk_uart *uart)
{
	return !mtk_uart_vfifo_is_full(uart->tx_vfifo);
}

/*---------------------------------------------------------------------------*/
static unsigned int mtk_uart_vfifo_read_allow(struct mtk_uart *uart)
{
	return !mtk_uart_vfifo_is_empty(uart->rx_vfifo);
}

/*---------------------------------------------------------------------------*/
static inline unsigned short mtk_uart_vfifo_get_trig(struct mtk_uart *uart,
					struct mtk_uart_vfifo *vfifo)
{
	return vfifo->trig;
}

/*---------------------------------------------------------------------------*/
#define get_mtk_uart(ptr, type, member) \
	(type *)((char *)ptr - offsetof(type, member))
/*---------------------------------------------------------------------------*/
#ifdef ENABE_HRTIMER_FLUSH
static enum hrtimer_restart mtk_uart_tx_vfifo_timeout(struct hrtimer *hrt)
{
	struct mtk_uart_vfifo *vfifo =
		container_of(hrt, struct mtk_uart_vfifo, flush);
	struct mtk_uart_dma *dma = (struct mtk_uart_dma *)vfifo->dma;
	struct mtk_uart *uart = dma->uart;
#if defined(ENABLE_VFIFO_DEBUG)
	ktime_t cur = ktime_get();
	struct timespec a = ktime_to_timespec(cur);

	MSG(MSC, "flush timeout [%ld %ld]\n", a.tv_sec, a.tv_nsec);
#endif
	mtk_uart_tx_vfifo_flush(uart, 1);
	return HRTIMER_NORESTART;
}
#endif

/*---------------------------------------------------------------------------*/
static void mtk_uart_dma_vfifo_callback(void *data)
{
	struct mtk_uart_dma *dma = (struct mtk_uart_dma *)data;
	struct mtk_uart *uart = dma->uart;

	MSG(DMA, "%s VFIFO CB: %4d/%4d\n",
		dma->dir == DMA_TO_DEVICE ? "TX" : "RX",
		mtk_uart_vfifo_get_counts(dma->vfifo), dma->vfifo->size);

	if (dma->dir == DMA_FROM_DEVICE) {
		/* the data must be read before return from callback,
		 * otherwise, the interrupt
		 * will be triggered again and again
		 */
		mtk_uart_dma_vfifo_rx_tasklet((unsigned long)uart);
		/* return; [ALPS00031975] */
	}
	tasklet_schedule(&dma->tasklet);
}

/*---------------------------------------------------------------------------*/
/* static __tcmfunc irqreturn_t mtk_vfifo_irq_handler(int irq, void *dev_id) */
static irqreturn_t mtk_vfifo_irq_handler(int irq, void *dev_id)
{
	struct mtk_uart_vfifo *vfifo;

	vfifo = (struct mtk_uart_vfifo *)dev_id;

	if (!vfifo) {
		pr_err("%s: vfifo is NULL\n", __func__);
		return IRQ_NONE;
	}
	if (!vfifo->dma) {
		pr_err("%s: dma is NULL\n", __func__);
		return IRQ_NONE;
	}

	/* Call call back function */
	mtk_uart_dma_vfifo_callback(vfifo->dma);

	/* Clear interrupt flag */
	if (vfifo->type == UART_RX_VFIFO)
		mtk_uart_vfifo_clear_rx_intr(vfifo);
	else
		mtk_uart_vfifo_clear_tx_intr(vfifo);

	return IRQ_HANDLED;
}

/*---------------------------------------------------------------------------*/
static int mtk_uart_dma_alloc(struct mtk_uart *uart,
	struct mtk_uart_dma *dma, int mode, struct mtk_uart_vfifo *vfifo)
{
	int ret = 0;

	MSG_FUNC_ENTRY();

	if (mode == UART_NON_DMA)
		return -1;

	switch (mode) {
	case UART_TX_VFIFO_DMA:
		if (!vfifo) {
			MSG(ERR, "fail due to NULL tx_vfifo\n");
			ret = -1;
			break;
		}

		vfifo->dma = dma;
		dma->dir = DMA_TO_DEVICE;
		dma->mode = mode;
		dma->vfifo = vfifo;
		dma->uart = uart;

		init_completion(&dma->done);
		tasklet_init(&dma->tasklet,
			mtk_uart_dma_vfifo_tx_tasklet, (unsigned long)uart);

		if (!atomic_read(&vfifo->reg_cb)) {
			/* disable interrupts */
			/* FIXME */
			mtk_uart_vfifo_disable_tx_intr(uart);

			ret = request_irq(vfifo->irq_id,
				(irq_handler_t) mtk_vfifo_irq_handler,
				IRQF_LEVEL_TRIGGER_POLARITY,
				DRV_NAME, vfifo);
			if (ret)
				return ret;
			atomic_set(&vfifo->reg_cb, 1);
		}

		atomic_set(&dma->free, 1);

		break;

	case UART_RX_VFIFO_DMA:
		if (!vfifo) {
			MSG(ERR, "fail due to NULL rx_vfifo\n");
			ret = -1;
			break;
		}

		vfifo->dma = dma;
		dma->dir = DMA_FROM_DEVICE;
		dma->mode = mode;
		dma->vfifo = vfifo;
		dma->uart = uart;

		init_completion(&dma->done);
		tasklet_init(&dma->tasklet,
			mtk_uart_dma_vfifo_rx_tasklet, (unsigned long)uart);

		if (!atomic_read(&vfifo->reg_cb)) {
			/* disable interrupts */
			mtk_uart_vfifo_disable_rx_intr(uart);

			ret = request_irq(vfifo->irq_id,
				(irq_handler_t) mtk_vfifo_irq_handler,
				IRQF_LEVEL_TRIGGER_POLARITY, DRV_NAME, vfifo);
			if (ret)
				return ret;
			atomic_set(&vfifo->reg_cb, 1);
		}

		atomic_set(&dma->free, 1);

		break;
	}
	return ret;
}

/*---------------------------------------------------------------------------*/
void mtk_uart_dma_stop(struct mtk_uart *uart, struct mtk_uart_dma *dma)
{
	MSG_FUNC_ENTRY();
	if (!dma)
		return;

	mtk_uart_stop_dma(dma);
	atomic_set(&dma->free, 1);
	complete(&dma->done);
}

/*---------------------------------------------------------------------------*/
static void mtk_uart_dma_free(struct mtk_uart *uart, struct mtk_uart_dma *dma)
{
	unsigned long flags;

	MSG_FUNC_ENTRY();

	if (!dma)
		return;

	if (dma->mode == UART_NON_DMA)
		return;

	if ((dma->mode == UART_RX_VFIFO_DMA
		|| dma->mode == UART_TX_VFIFO_DMA) && (!dma->vfifo))
		return;

	if (dma->vfifo && !mtk_uart_vfifo_is_empty(dma->vfifo)) {
		tasklet_schedule(&dma->tasklet);
		MSG(DMA, "wait for %s vfifo dma completed!!!\n",
			dma->dir == DMA_TO_DEVICE ? "TX" : "RX");
		wait_for_completion(&dma->done);
	}
	spin_lock_irqsave(&uart->port.lock, flags);
	mtk_uart_stop_dma(dma);
	if (dma->mode == UART_TX_VFIFO_DMA) {
		if (dma->vfifo && timer_pending(&dma->vfifo->timer))
			del_timer_sync(&dma->vfifo->timer);
#ifdef ENABE_HRTIMER_FLUSH
		if (dma->vfifo && hrtimer_active(&dma->vfifo->flush))
			hrtimer_cancel(&dma->vfifo->flush);
#endif
	}
	/* [ALPS00030487] tasklet_kill function may schedule,
	 * so release spin lock first,
	 *                  after release, set spin lock again.
	 */
	/* [ALPS00030487] Add this */
	spin_unlock_irqrestore(&uart->port.lock, flags);
	tasklet_kill(&dma->tasklet);
	/* [ALPS00030487] Add this */
	spin_lock_irqsave(&uart->port.lock, flags);
	mtk_uart_reset_dma(dma);
	mtk_uart_vfifo_disable(uart, dma->vfifo);
	mtk_uart_vfifo_free(uart, dma->vfifo);
	MSG(INFO, "free %s dma completed!!!\n",
			dma->dir == DMA_TO_DEVICE ? "TX" : "RX");
	memset(dma, 0, sizeof(struct mtk_uart_dma));
	spin_unlock_irqrestore(&uart->port.lock, flags);

}
#endif				/*defined(ENABLE_VFIFO) */
/*---------------------------------------------------------------------------*/
static void mtk_uart_set_baud(struct mtk_uart *uart, int baudrate)
{
	if (uart->port.flags & ASYNC_SPD_CUST) {
		/**
		 * [ALPS00137126] Begin
		 * Because the origin design of custom baudrate
		 * in linux is for low speed case, we add some
		 * modify to support high speed case.
		 * NOTE: If the highest bit of "custom_divisor" is ONE,
		 * we will use custom_divisor store baudrate
		 * directly. That means(we suppose unsigned int is 32 bits):
		 *	custom_divisor[31] == 1,
		 *	then custom_divisor[30..0] == custom baud rate
		 *	custom_divisor[31] == 0,
		 *	then custom_divisor[30..0] == sysclk/16/baudrate
		 */
		if (uart->port.custom_divisor & (1 << 31)) {
			baudrate = uart->port.custom_divisor & (~(1 << 31));
			/* Baud rate should not more than sysclk/4 */
			if (baudrate > (uart->sysclk >> 2))
				baudrate = 9600;
		} else {
			/* the baud_base gotten in user
			 * space eqauls to sysclk/16.
			 * hence, we need to restore the difference
			 * when calculating custom baudrate
			 */
			if (!uart->custom_baud) {
				baudrate = uart->sysclk / 16;
				baudrate =
					baudrate / uart->port.custom_divisor;
			} else {
				baudrate = uart->custom_baud;
			}
			/* [ALPS00137126] End */
		}
		MSG(CFG, "CUSTOM, baudrate = %d, divisor = %d\n",
				baudrate, uart->port.custom_divisor);
	}

	if (uart->auto_baud)
		mtk_uart_set_auto_baud(uart);

	mtk_uart_baud_setting(uart, baudrate);

	uart->baudrate = baudrate;
}

/*---------------------------------------------------------------------------*/
static inline bool mtk_uart_enable_sysrq(struct mtk_uart *uart)
{
	return uart->setting->sysrq;
}

/*---------------------------------------------------------------------------*/
static void mtk_uart_rx_chars(struct mtk_uart *uart)
{
	struct uart_port *port = &uart->port;
	/*struct tty_struct *tty = uart->port.state->port.tty;*/
	int max_count = UART_FIFO_SIZE;
	unsigned int data_byte, status;
	unsigned int flag;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);
	/* MSG_FUNC_ENTRY(); */
	while (max_count-- > 0) {

		/* check status */
		if (!mtk_uart_data_ready(uart))
			break;
#if 0
		if (tty->flip.count >= TTY_FLIPBUF_SIZE) {
			if (tty->low_latency) {
			/*
			 * If this failed then we will throw away the
			 * bytes but must do so to clear interrupts
			 */
				tty_flip_buffer_push(tty);
			}
		}
#endif
		/* read the byte */
		data_byte = uart->read_byte(uart);
		port->icount.rx++;
		flag = TTY_NORMAL;

		update_history_byte(0, uart->nport, data_byte);

		status = mtk_uart_filter_line_status(uart);

		/* error handling routine */
		if (status & UART_LSR_BI) {
			MSG(INFO, "Break interrupt!!\n");
			port->icount.brk++;
			if (uart_handle_break(port))
				continue;
			flag = TTY_BREAK;
		} else if (status & UART_LSR_PE) {
			MSG(INFO, "Parity Error!!\n");
			port->icount.parity++;
			flag = TTY_PARITY;
		} else if (status & UART_LSR_FE) {
			MSG(INFO, "Frame Error!!\n");
			port->icount.frame++;
			flag = TTY_FRAME;
		} else if (status & UART_LSR_OE) {
			MSG(INFO, "Overrun!!\n");
			port->icount.overrun++;
			flag = TTY_OVERRUN;
		}
#ifdef CONFIG_MAGIC_SYSRQ
		if (mtk_uart_enable_sysrq(uart)) {
			if (uart_handle_sysrq_char(port, data_byte))
				continue;

/* FIXME. Infinity, 20081002, 'BREAK' char to enable sysrq handler { */
#if defined(CONFIG_MAGIC_SYSRQ) && defined(CONFIG_SERIAL_CORE_CONSLE)
			if (data_byte == 0)
				uart->port.sysrq = 1;
#endif
/* FIXME. Infinity, 20081002, 'BREAK' char to enable sysrq handler } */
		}
#endif

		if (!tty_insert_flip_char(&(port->state->port),
					data_byte, flag))
			MSG(ERR, "tty_insert_flip_char: no space");
	}
	tty_flip_buffer_push(&(port->state->port));
	update_history_time(0, uart->nport);
	spin_unlock_irqrestore(&port->lock, flags);
	MSG(FUC, "%s (%2d)\n", __func__, UART_FIFO_SIZE - max_count - 1);
/*
 *#if defined(CONFIG_MTK_HDMI_SUPPORT)
 *#ifdef MHL_UART_SHARE_PIN
 *	if ((UART_FIFO_SIZE - max_count - 1) > 0)
 *		hdmi_force_on(UART_FIFO_SIZE - max_count - 1);
 *#endif
 *#endif
 */
}

/*---------------------------------------------------------------------------*/
static void mtk_uart_tx_chars(struct mtk_uart *uart)
{
	/* Notice:
	 * The function is called by uart_start,
	 * which is protected by spin lock,
	 * Hence, no spin-lock is required in the functions
	 */

	struct uart_port *port = &uart->port;
	struct circ_buf *xmit = &port->state->xmit;
	int count;

	/* deal with x_char first */
	if (unlikely(port->x_char)) {
		MSG(INFO, "detect x_char!!\n");
		uart->write_byte(uart, port->x_char);
		port->icount.tx++;
		port->x_char = 0;
		return;
	}

	/* stop tx if circular buffer is empty or this port is stopped */
	if (uart_circ_empty(xmit) || uart_tx_stopped(port)) {
		struct tty_struct *tty = port->state->port.tty;

		if (!uart_circ_empty(xmit))
			MSG(ERR, "\t\tstopped: empty: %d %d %d\n",
				uart_circ_empty(xmit), tty->stopped,
			    tty->hw_stopped);
		mtk_uart_stop_tx(port);
		return;
	}

	count = port->fifosize - 1;

	do {
		if (uart_circ_empty(xmit))
			break;
		if ((count == 1) || (!uart->write_allow(uart))) {
			/* to avoid the interrupt is not enable. */
			mtk_uart_enable_intrs(uart, UART_IER_ETBEI);
			break;
		}
		uart->write_byte(uart, xmit->buf[xmit->tail]);

		update_history_byte(1, uart->nport, xmit->buf[xmit->tail]);

		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		port->icount.tx++;

	} while (--count > 0);

	MSG(INFO, "TX %d chars\n", port->fifosize - 1 - count);

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	if (uart_circ_empty(xmit))
		mtk_uart_stop_tx(port);

	update_history_time(1, uart->nport);
}

/*---------------------------------------------------------------------------*/
static void mtk_uart_rx_handler(struct mtk_uart *uart, int intrs)
{
	if (uart->rx_mode == UART_NON_DMA) {
		mtk_uart_rx_chars(uart);
	} else if (uart->rx_mode == UART_RX_VFIFO_DMA) {
#if defined(ENABLE_VFIFO)
		mtk_uart_rx_pre_handler(uart, intrs);
		mtk_uart_dma_vfifo_rx_tasklet((unsigned long)uart);
#endif
	}
}

/*---------------------------------------------------------------------------*/
void mtk_uart_tx_handler(struct mtk_uart *uart)
{
	struct uart_port *port = &uart->port;
	unsigned long flags;

	if (uart->tx_mode == UART_NON_DMA) {
		spin_lock_irqsave(&port->lock, flags);
		mtk_uart_tx_chars(uart);
		spin_unlock_irqrestore(&port->lock, flags);
	} else if (uart->tx_mode == UART_TX_VFIFO_DMA) {
		tasklet_schedule(&uart->dma_tx.tasklet);
	}
}

/*---------------------------------------------------------------------------*/
#ifdef ENABLE_DEBUG
/*---------------------------------------------------------------------------*/
static const char * const fifo[] = {
	"No FIFO", "Unstable FIFO",
	"Unknown", "FIFO Enabled"
};

static const char * const interrupt[] = {
	"Modem Status Chg", "Tx Buffer Empty",
	"Rx Data Received", "BI, FE, PE, or OE",
	"0x04", "0x05", "Rx Data Timeout", "0x07",
	"SW Flow Control", "0x09", "0x10", "0x11", "0x12",
	"0x13", "0x14", "0x15", "HW Flow Control"
};

/*---------------------------------------------------------------------------*/
#endif

/*---------------------------------------------------------------------------*/
/* static __tcmfunc irqreturn_t mtk_uart_irq(int irq, void *dev_id) */
static irqreturn_t mtk_uart_irq(int irq, void *dev_id)
{
	unsigned int intrs, timeout = 0;
	struct mtk_uart *uart = (struct mtk_uart *)dev_id;

#ifndef CONFIG_FIQ_DEBUGGER
#ifdef CONFIG_MTK_ENG_BUILD
#ifdef CONFIG_MTK_PRINTK_UART_CONSOLE
	unsigned long base;

	base = uart->base;
	if ((uart == console_port) && (UART_READ32(UART_LSR) & 0x01))
		printk_disable_uart = 0;
#endif
#endif
#endif

	intrs = mtk_uart_get_interrupt(uart);

#ifdef ENABLE_DEBUG
	{
		struct uart_iir_reg *iir = (struct uart_iir_reg *) &intrs;

		if (iir->NINT)
			MSG(INT, "No interrupt (%s)\n", fifo[iir->FIFOE]);
		else if (iir->ID < ARRAY_SIZE(interrupt))
			MSG(INT, "%02x %s (%s)\n",
				iir->ID, interrupt[iir->ID], fifo[iir->FIFOE]);
		else
			MSG(INT, "%2x\n", iir->ID);
	}
#endif
	intrs &= UART_IIR_INT_MASK;

	if (intrs == UART_IIR_NO_INT_PENDING)
		return IRQ_HANDLED;

	/* pr_debug("[UART%d] intrs:0x%x\n", uart->nport, intrs); */
	if (intrs == UART_IIR_CTI)
		timeout = 1;
	else if (intrs == UART_IIR_THRE)
		mtk_uart_tx_handler(uart);
	else if (intrs == UART_IIR_MS)
		mtk_uart_get_modem_status(uart);

	mtk_uart_intr_last_check(uart, intrs);
	mtk_uart_rx_handler(uart, intrs);
	return IRQ_HANDLED;
}

/*---------------------------------------------------------------------------*/
/* test whether the transmitter fifo and shifter for the port is empty. */
static unsigned int mtk_uart_tx_empty(struct uart_port *port)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;

	MSG_FUNC_ENTRY();

#if defined(ENABLE_VFIFO)
	if (uart->tx_mode == UART_TX_VFIFO_DMA)
		return mtk_uart_vfifo_is_empty(uart->dma_tx.vfifo) ?
					 TIOCSER_TEMT : 0;
#endif
	return uart->write_allow(uart) ? TIOCSER_TEMT : 0;
}

/*---------------------------------------------------------------------------*/
/* FIXME */
/* stop transmitting characters
 * Note: this function is call with interrupt disabled
 */
static void mtk_uart_stop_tx(struct uart_port *port)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;

	MSG_FUNC_ENTRY();
#if defined(ENABLE_VFIFO)
	if (uart->tx_mode == UART_TX_VFIFO_DMA) {
	/*1. UART_IER_ETBEI can't be disabled or zero data appears in TX */
	/*2. TX_INT_EN.INTEN will be reset automatically by HW */
	} else
#endif
		/* disable tx interrupt */
		mtk_uart_disable_intrs(uart, UART_IER_ETBEI);
	uart->tx_stop = 1;
}

/*---------------------------------------------------------------------------*/
/* FIXME */
/* start transmitting characters.
 * Note: this function is call with interrupt disabled
 */
static void mtk_uart_start_tx(struct uart_port *port)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;
	struct circ_buf *xmit = &port->state->xmit;
	unsigned long size;

	size = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);

	if (!size)
		return;

	uart->tx_stop = 0;
#ifdef ENABLE_RAW_DATA_DUMP
	reset_tx_raw_data(uart);
#endif

#if defined(ENABLE_VFIFO)
	if (uart->tx_mode == UART_TX_VFIFO_DMA) {
		if (UART_DEBUG_EVT(DBG_EVT_BUF))
			pr_debug("[UART%d] %s\n", uart->nport, __func__);
		if (!uart->write_allow(uart))
			mtk_uart_vfifo_enable_tx_intr(uart);
		else
			mtk_uart_dma_vfifo_tx_tasklet((unsigned long)uart);
	} else {
#else
	{
#endif
		if (uart->write_allow(uart))
			mtk_uart_tx_chars(uart);
	}
}

/*---------------------------------------------------------------------------*/
static void mtk_uart_send_xchar(struct uart_port *port, char ch)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;
	unsigned long flags;

	MSG_FUNC_ENTRY();

	if (uart->tx_stop)
		return;

	spin_lock_irqsave(&port->lock, flags);
	while (!mtk_uart_write_allow(uart))
		;
	mtk_uart_write_byte(uart, (unsigned char)ch);
	port->icount.tx++;
	spin_unlock_irqrestore(&port->lock, flags);
}

/*---------------------------------------------------------------------------*/
/* enable the modem status interrupts */
static void mtk_uart_enable_ms(struct uart_port *port)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;

	MSG_FUNC_ENTRY();
	uart->ms_enable = 1;
}

/*---------------------------------------------------------------------------*/
/* grab any interrupt resources and initialize any low level driver state */
static int mtk_uart_startup(struct uart_port *port)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;
	int ret;
	long mask = UART_IER_HW_NORMALINTS;

	MSG_FUNC_ENTRY();

	/*the uart port is power up in power_mgnt */

	/* Reset default flag when the uart starts up,
	 * or the previous setting,
	 * Such as custom baudrate will be
	 * still applied even it is ever closed
	 */
	/* uart->port.flags = UPF_BOOT_AUTOCONF; */
	/* uart->port.custom_divisor = 1; */

	/* Check whether is ATE_Factory mode */
#ifdef ATE_FACTORY_ENABLE
	mtk_uart_is_ate_factory_mode(uart);
#endif				/*ATE_FACTORY_ENABLE */

	uart->fctl_mode = UART_FC_NONE;

	/* disable interrupts */
	mtk_uart_disable_intrs(uart, UART_IER_ALL_INTS);

	/* allocate irq line */
	/* ret = request_irq(port->irq, mtk_uart_irq,
	 * 0, DRV_NAME, uart);
	 */

	/* [ALPS00142658] Fix incompatible pointer type waning */
	ret = request_irq(port->irq, (irq_handler_t) mtk_uart_irq,
			uart->setting->irq_flags, DRV_NAME, uart);

	if (ret)
		return ret;

#if defined(ENABLE_VFIFO)
#if defined(ENABLE_VFIFO_DEBUG)
	mtk_uart_vfifo_prepare(uart);
	uart->dma_mode = uart->setting->dma_mode;
	uart->tx_mode = uart->setting->tx_mode;
	uart->rx_mode = uart->setting->rx_mode;
	uart->tx_trig = uart->setting->tx_trig;
	uart->rx_trig = uart->setting->rx_trig;
#endif
	/* allocate vfifo */
	if (uart->rx_mode == UART_RX_VFIFO_DMA) {
		uart->rx_vfifo = mtk_uart_vfifo_alloc(uart, UART_RX_VFIFO);
		ret = mtk_uart_dma_alloc(uart, &uart->dma_rx,
				uart->rx_mode, uart->rx_vfifo);
		if (!uart->rx_vfifo || ret) {
			uart->rx_mode = UART_NON_DMA;
			MSG(ERR, "RX DMA alloc fail [%d]\n", ret);
		}
	}
	if (uart->tx_mode == UART_TX_VFIFO_DMA) {
		uart->tx_vfifo = mtk_uart_vfifo_alloc(uart, UART_TX_VFIFO);
		ret = mtk_uart_dma_alloc(uart,
				&uart->dma_tx, uart->tx_mode, uart->tx_vfifo);
		if (!uart->tx_vfifo || ret) {
			uart->tx_mode = UART_NON_DMA;
			MSG(ERR, "TX DMA alloc fail [%d]\n", ret);
		}
	}

	/* start vfifo dma */
	if (uart->tx_mode == UART_TX_VFIFO_DMA) {
		uart->write_allow = mtk_uart_vfifo_write_allow;
		uart->write_byte = mtk_uart_vfifo_write_byte;
		mtk_uart_vfifo_enable(uart, uart->tx_vfifo);
		mtk_uart_dma_setup(uart, &uart->dma_tx);
		if (mtk_uart_dma_start(uart, &uart->dma_tx))
			MSG(ERR, "mtk_uart_dma_start fails\n");

#ifdef ENABE_HRTIMER_FLUSH
		hrtimer_init(&uart->tx_vfifo->flush,
			CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
		uart->tx_vfifo->flush.function = mtk_uart_tx_vfifo_timeout;
#endif
	} else if (uart->tx_mode == UART_NON_DMA) {
		uart->write_allow = mtk_uart_write_allow;
		uart->write_byte = mtk_uart_write_byte;
	}
	if (uart->rx_mode == UART_RX_VFIFO_DMA) {
		uart->read_allow = mtk_uart_vfifo_read_allow;
		uart->read_byte = mtk_uart_vfifo_read_byte;
		mtk_uart_vfifo_enable(uart, uart->rx_vfifo);
		mtk_uart_dma_setup(uart, &uart->dma_rx);
		if (mtk_uart_dma_start(uart, &uart->dma_rx))
			MSG(ERR, "mtk_uart_dma_start fails\n");
	} else if (uart->rx_mode == UART_NON_DMA) {
		uart->read_allow = mtk_uart_read_allow;
		uart->read_byte = mtk_uart_read_byte;
	}
#endif
	if (uart->tx_mode == UART_TX_VFIFO_DMA
			|| uart->rx_mode == UART_RX_VFIFO_DMA) {
		if (!bt_port || (bt_port && (uart != bt_port)))
			mtk_uart_disable_dpidle(uart);
	} else if (uart->tx_mode == UART_NON_DMA
			&& uart->rx_mode == UART_NON_DMA) {
		mtk_uart_enable_dpidle(uart);
	}

	uart->tx_stop = 0;
	uart->rx_stop = 0;

	/* After applying UART as Level-Triggered IRQ,
	 * the function must be called or
	 * the interrupt will be incorrect activated.
	 */
	mtk_uart_fifo_set_trig(uart, uart->tx_trig, uart->rx_trig);
	mtk_uart_enable_sleep(uart);

	/* enable interrupts */
	mtk_uart_enable_intrs(uart, mask);
	return 0;
}

/*---------------------------------------------------------------------------*/
/* disable the port, disable any break condition that may be in effect, and
 * free any interrupt resources
 */
static void mtk_uart_shutdown(struct uart_port *port)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;

	MSG_FUNC_ENTRY();

	/* disable interrupts */
	mtk_uart_disable_intrs(uart, UART_IER_ALL_INTS);

#if defined(ENABLE_VFIFO)
	/* free dma channels and vfifo */
	mtk_uart_dma_free(uart, &uart->dma_tx);
	mtk_uart_dma_free(uart, &uart->dma_rx);
#endif

	mdelay(1);
	mtk_uart_fifo_flush(uart);

	/* release irq line */
	free_irq(port->irq, port);

	mtk_uart_enable_dpidle(uart);
	/* the uart port will be powered off in power_mgnt */
}

/*---------------------------------------------------------------------------*/
static void mtk_uart_flush_buffer(struct uart_port *port)
{
#if defined(ENABLE_DEBUG)
	struct mtk_uart *uart = (struct mtk_uart *)port;

	MSG_FUNC_ENTRY();
#endif
	/* mtk_uart_fifo_flush(uart); */
}

/*---------------------------------------------------------------------------*/
/*
 * For stability test
 */
void mtk_uart_update_sysclk(void)
{
	int i;
	unsigned long flags;
	struct mtk_uart *uart;
	struct uart_port *port;
	unsigned int baud;

	for (i = 0; i < UART_NR; i++) {
		uart = &mtk_uarts[i];
		port = &uart->port;
		baud = uart->baudrate;
		/* mt6575_get_bus_freq()*1000/4; */
		port->uartclk = UART_SYSCLK;
		/* mt6575_get_bus_freq()*1000/4; */
		uart->sysclk = UART_SYSCLK;
		if (baud == 0)
			/* The istance is not initialized yet. */
			continue;
		spin_lock_irqsave(&port->lock, flags);
		mtk_uart_set_baud(uart, baud);
		spin_unlock_irqrestore(&port->lock, flags);
	}
}
EXPORT_SYMBOL(mtk_uart_update_sysclk);

/*---------------------------------------------------------------------------*/
/* change the port parameters, including word length, parity, stop bits.
 * update read_status_mask and ignore_status_mask to indicate the types of
 * events we are interrested in receiving
 */
static void mtk_uart_set_termios(struct uart_port *port,
		struct ktermios *termios, struct ktermios *old)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;
	unsigned long flags;
	unsigned int baud;
	int datalen, mode;
	int parity = 0;
	int stopbit = 1;
	static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ,  5);

	MSG_FUNC_ENTRY();

	/* datalen : default 8bits */
	switch (termios->c_cflag & CSIZE) {
	case CS5:
		datalen = 5;
		break;
	case CS6:
		datalen = 6;
		break;
	case CS7:
		datalen = 7;
		break;
	case CS8:
	default:
		datalen = 8;
		break;
	}

	/* stopbit : default 1 */
	if (termios->c_cflag & CSTOPB)
		stopbit = 2;

	/* parity : default none */
	if (termios->c_cflag & PARENB) {
		if (termios->c_cflag & PARODD)
			parity = 1;	/* odd */
		else
			parity = 2;	/* even */
	}

	spin_lock_irqsave(&port->lock, flags);

	/* read status mask */
	port->read_status_mask = 0;
	if (termios->c_iflag & INPCK) {
		/* frame error, parity error */
		port->read_status_mask |= UART_LSR_FE | UART_LSR_PE;
	}
	if (termios->c_iflag & (BRKINT | PARMRK)) {
		/* break error */
		port->read_status_mask |= UART_LSR_BI;
	}

	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR) {
		/* ignore parity and framing errors */
		port->ignore_status_mask |= UART_LSR_FE | UART_LSR_PE;
	}
	if (termios->c_iflag & IGNBRK) {
		/* ignore break errors. */
		port->ignore_status_mask |= UART_LSR_BI;
		if (termios->c_iflag & IGNPAR) {
			/* ignore overrun errors */
			port->ignore_status_mask |= UART_LSR_OE;
		}
	}

	/* ignore all characters if CREAD is not set */
	if ((termios->c_cflag & CREAD) == 0)
		uart->ignore_rx = 1;

	/* update per port timeout */
	/*when dividor is 1, baudrate = clock */
	baud = uart_get_baud_rate(port, termios, old, 0, uart->sysclk);
	uart_update_timeout(port, termios->c_cflag, baud);
	mtk_uart_config(uart, datalen, stopbit, parity);
	mtk_uart_set_baud(uart, baud);

	/* setup fifo trigger level */
	mtk_uart_fifo_set_trig(uart, uart->tx_trig, uart->rx_trig);

	/* setup hw flow control: only port 0 ~1 support hw rts/cts */
	MSG(CFG, "c_lflag:%X, c_iflag:%X, c_oflag:%X, c_cflag:%X\n",
		termios->c_lflag, termios->c_iflag,
	    termios->c_oflag, termios->c_cflag);
	if (HW_FLOW_CTRL_PORT(uart) && (termios->c_cflag & CRTSCTS)
				&& (!(termios->c_iflag & 0x80000000))) {
		if (__ratelimit(&ratelimit))
			pr_debug("Hardware Flow Control\n");
		mode = UART_FC_HW;
	} else if (termios->c_iflag & 0x80000000) {
		if (__ratelimit(&ratelimit))
			pr_debug("MTK Software Flow Control\n");
		mode = UART_FC_SW;
	} else if (termios->c_iflag & (IXON | IXOFF | IXANY)) {
		if (__ratelimit(&ratelimit))
			pr_debug("Linux default SW Flow Control\n");
		mode = UART_FC_NONE;
	} else {
		if (__ratelimit(&ratelimit))
			pr_debug("No Flow Control\n");
		mode = UART_FC_NONE;
	}
	mtk_uart_set_flow_ctrl(uart, mode);

	/* determine if port should enable modem status interrupt */
	if (UART_ENABLE_MS(port, termios->c_cflag))
		uart->ms_enable = 1;
	else
		uart->ms_enable = 0;

	if (console_port && (uart == console_port) && uart->port.cons)
		uart->port.cons->cflag = termios->c_cflag;

	spin_unlock_irqrestore(&port->lock, flags);
}

/*---------------------------------------------------------------------------*/
/* perform any power management related activities on the port */
static void mtk_uart_power_mgnt(struct uart_port *port,
				unsigned int state, unsigned int oldstate)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;

	MSG(FUC, "%s(%d->%d)\n", __func__, oldstate, state);

	switch (state) {
	case 0:
		mtk_uart_power_up(uart);
		break;
	case 3:
		mtk_uart_power_down(uart);
		break;
	default:
		MSG(ERR, "unknown pm: %d\n", state);
	}
}

/*---------------------------------------------------------------------------*/
/* return a pointer to a string constant describing the port */
static const char *mtk_uart_type(struct uart_port *port)
{
	return "MTK UART";
}

/*---------------------------------------------------------------------------*/
/* release any memory and io region resources currently in used by the port */
static void mtk_uart_release_port(struct uart_port *port)
{
}

/*---------------------------------------------------------------------------*/
/* request any memory and io region resources required by the port */
static int mtk_uart_request_port(struct uart_port *port)
{
	return 0;
}

/*---------------------------------------------------------------------------*/
/* perform any autoconfiguration steps required by the port.
 * it's expected to claim the resources and map the port.
 */
static void mtk_uart_config_port(struct uart_port *port, int flags)
{
	struct mtk_uart *uart = (struct mtk_uart *)port;

	if (flags & UART_CONFIG_TYPE) {
		if (mtk_uart_request_port(port))
			MSG(ERR, "mtk_uart_request_port fail\n");
		port->type = PORT_MTK;
	}
}

/*---------------------------------------------------------------------------*/
/* verify if the new serial information contained within 'ser' is suitable */
static int mtk_uart_verify_port(struct uart_port *port,
		struct serial_struct *ser)
{
/*[ALPS00142658] Fix unused variable waring */
#if (defined(ENABLE_DEBUG) || defined(SERIAL_STRUCT_EXT))
	struct mtk_uart *uart = (struct mtk_uart *)port;
#endif
	int ret = 0;

	MSG(FUC, "%s: %8x, %d, %d\n",
		__func__, ser->flags, ser->custom_divisor, uart->custom_baud);
	if (ser->type != PORT_UNKNOWN && ser->type != PORT_MTK)
		ret = -EINVAL;
	if (ser->irq != port->irq)
		ret = -EINVAL;
	if (ser->baud_base < 110)
		ret = -EINVAL;
#if defined(SERIAL_STRUCT_EXT)
	/*EXtension: the custom baudrate is stored in reserved field */
	uart->custom_baud = ser->reserved[0];
#endif
	return ret;
}

/*---------------------------------------------------------------------------*/
/* perform any port specific IOCTLs */
static int mtk_uart_ioctl(struct uart_port *port,
	unsigned int cmd, unsigned long arg)
{
#if defined(ENABLE_DEBUG)
	struct mtk_uart *uart = (struct mtk_uart *)port;

	MSG(FUC, "IOCTL: %8X\n", cmd);
#endif
	return -ENOIOCTLCMD;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_CONSOLE_POLL
/*---------------------------------------------------------------------------*/
static int mtk_uart_get_poll_char(struct uart_port *port)
{				/* don't care vfifo setting */
	struct mtk_uart *uart = (struct mtk_uart *)port;

	/* [ALPS00033048] For Linux 2.6.35 kgdb chagne,
	 * using while loop may block kgdb,
	 * return NO_POLL_CHAR directly if no data to read
	 */
#if 0
	while (!(uart->read_status(uart) & UART_LSR_DR))
		cpu_relax();
#else
	if (!mtk_uart_data_ready(uart))
		return NO_POLL_CHAR;
#endif
	/* End of [ALPS00033048] */

	return mtk_uart_read_byte(uart);
}

/*---------------------------------------------------------------------------*/
static void mtk_uart_put_poll_char(struct uart_port *port, unsigned char c)
{				/* don't care vfifo setting */
	struct mtk_uart *uart = (struct mtk_uart *)port;

	while (!mtk_uart_write_allow(uart))
		barrier();
	mtk_uart_write_byte(uart, c);
}

/*---------------------------------------------------------------------------*/
#endif
/*---------------------------------------------------------------------------*/
static const struct uart_ops mtk_uart_ops = {
	.tx_empty = mtk_uart_tx_empty,
	.set_mctrl = mtk_uart_set_mctrl,
	.get_mctrl = mtk_uart_get_mctrl,
	.stop_tx = mtk_uart_stop_tx,
	.start_tx = mtk_uart_start_tx,
	.stop_rx = mtk_uart_stop_rx,
	.send_xchar = mtk_uart_send_xchar,
	.enable_ms = mtk_uart_enable_ms,
	.break_ctl = mtk_uart_break_ctl,
	.startup = mtk_uart_startup,
	.shutdown = mtk_uart_shutdown,
	.flush_buffer = mtk_uart_flush_buffer,
	.set_termios = mtk_uart_set_termios,
	.pm = mtk_uart_power_mgnt,
	.type = mtk_uart_type,
	.release_port = mtk_uart_release_port,
	.request_port = mtk_uart_request_port,
	.config_port = mtk_uart_config_port,
	.verify_port = mtk_uart_verify_port,
	.ioctl = mtk_uart_ioctl,
#ifdef CONFIG_CONSOLE_POLL
	.poll_get_char = mtk_uart_get_poll_char,
	.poll_put_char = mtk_uart_put_poll_char,
#endif
};

/*---------------------------------------------------------------------------*/
static struct uart_driver mtk_uart_drv = {
	.owner = THIS_MODULE,
	.driver_name = DRV_NAME,
	.dev_name = "ttyMT",
	.major = UART_MAJOR,
	.minor = UART_MINOR,
	.nr = UART_NR,
#if defined(CONFIG_MTK_SERIAL_CONSOLE) && \
	!defined(CONFIG_MTK_SERIAL_MODEM_TEST)
	.cons = &mtk_uart_console,
#endif
};

/*---------------------------------------------------------------------------*/
static int mtk_uart_probe(struct platform_device *pdev)
{
	struct mtk_uart *uart;
	int err;
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	static const char * const clk_uart_name[] = {
		"uart0-main",
		"uart1-main",
		"uart2-main",
		"uart3-main",
		"uart4-main",
	};
	struct mtk_uart_setting *uart_setting = NULL;
#if !defined(CONFIG_MTK_LEGACY)
	/* for GPIO pinctrl */
	struct pinctrl *ppinctrl = NULL;
#endif
#endif /* !defined(CONFIG_FPGA_EARLY_PORTING) */

	if (pdev->dev.of_node) {
		struct device_node *node = pdev->dev.of_node;

		err = of_property_read_u32(node, "cell-index", &pdev->id);
		if (err)
			pr_err("[DTS] get uart platform_device id fail!!\n");
	}
	if (pdev->id >= UART_NR) {
		pr_err("DTS cell ID %d > UART nuber %d\n",
			pdev->id, UART_NR);
		return -ENODEV;
	}

	uart = &mtk_uarts[pdev->id];
	MSG_FUNC_ENTRY();

/* For clock setting */
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	uart_setting = get_uart_default_settings(pdev->id);

	uart_setting->clk_uart_main =
			devm_clk_get(&pdev->dev,
			clk_uart_name[pdev->id]);
	if (IS_ERR(uart_setting->clk_uart_main)) {
		pr_err("[UART%d][CCF]cannot get %s clock. ptr_err:%ld\n",
		pdev->id, clk_uart_name[pdev->id]
		       , PTR_ERR(uart_setting->clk_uart_main));
		return PTR_ERR(uart_setting->clk_uart_main);
	}
	err = clk_prepare(uart_setting->clk_uart_main);
	if (err) {
		pr_err("[UART%d] cannot prepare main clk ctrl\n",
				pdev->id);
		return err;
	}
	pr_debug("[UART%d][CCF]clk_uart_main:%p\n",
		pdev->id, uart_setting->clk_uart_main);

	if (pdev->id == 0) {
		struct clk *clk_uart0_dma =
			devm_clk_get(&pdev->dev, "uart-apdma");

		if (IS_ERR(clk_uart0_dma)) {
			pr_err("[UART][CCF]cannot get clk_uart0_dma clock");
			pr_err("ptr_err: %ld\n", PTR_ERR(clk_uart0_dma));
			return PTR_ERR(clk_uart0_dma);
		}
		err = clk_prepare(clk_uart0_dma);
		if (err) {
			pr_err("[UART%d] cannot prepare dma clk ctrl\n",
				pdev->id);
			return err;
		}

		set_uart_dma_clk(pdev->id, clk_uart0_dma);
		pr_debug("[UART][CCF]clk_uart0_dma:%p\n", clk_uart0_dma);
	}
#else
	pr_debug("[UART][CCF]%s:CONFIG_FPGA_EARLY_PORTING is defined!\n",
			__func__);
#endif

/* For GPIO setting */
#if !defined(CONFIG_MTK_LEGACY) && !defined(CONFIG_FPGA_EARLY_PORTING)
	ppinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(ppinctrl)) {
		err = PTR_ERR(ppinctrl);
		pr_err("[UART%d][PinC]cannot find pinctrl. ptr_err:%ld\n",
		pdev->id, PTR_ERR(ppinctrl));
		return err;
	}
	set_uart_pinctrl(pdev->id, ppinctrl);
	pr_debug("[UART%d][PinC]set idx:%d, ppinctrl:%p\n",
	pdev->id, pdev->id, ppinctrl);
#else /* !defined(CONFIG_MTK_LEGACY) && !defined(CONFIG_FPGA_EARLY_PORTING) */
	pr_debug("[UART][PinC]%s:CONFIG_MTK_LEGACY", __func__);
	pr_debug("or CONFIG_FPGA_EARLY_PORTING is defined!\n");
#endif /* !defined(CONFIG_MTK_LEGACY) && !defined(CONFIG_FPGA_EARLY_PORTING) */

	if (uart->setting->support_33bits) {
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(33);
		if (dma_set_mask(&pdev->dev, DMA_BIT_MASK(33))) {
			dev_err(&pdev->dev, "dma_set_mask return error.\n");
			return -EINVAL;
		}
	} else {
		pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	}
	uart->port.dev = &pdev->dev;
	err = uart_add_one_port(&mtk_uart_drv, &uart->port);
	if (!err)
		platform_set_drvdata(pdev, uart);

#if defined(ENABLE_VFIFO)
	err = mtk_uart_vfifo_create(uart);
	if (err) {
		mtk_uart_vfifo_delete(uart);
		pr_err("create vff buffer fail:%d\n", err);
	}
#endif
	return err;
}

/*---------------------------------------------------------------------------*/
static int mtk_uart_remove(struct platform_device *pdev)
{
	struct mtk_uart *uart = platform_get_drvdata(pdev);
	int err;

	platform_set_drvdata(pdev, NULL);

	if (!uart)
		return -EINVAL;

	err = uart_remove_one_port(&mtk_uart_drv, &uart->port);

#if defined(ENABLE_VFIFO)
	mtk_uart_vfifo_delete(uart);
#endif
	return err;
}

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_PM
/*---------------------------------------------------------------------------*/
static int mtk_uart_syscore_suspend(void)
{
	int ret = 0;
	unsigned long flags;

	if (bt_port) {
		struct mtk_uart *uart = bt_port;

		spin_lock_irqsave(&mtk_uart_bt_lock, flags);
		ret = uart_suspend_port(&mtk_uart_drv, &uart->port);
		/* To keeping uart idle state */
		/* tx pin:  idle->high   power down->low */
		mtk_uart_switch_tx_to_gpio(uart);
		spin_unlock_irqrestore(&mtk_uart_bt_lock, flags);
		pr_debug("[UART%d] BT Suspend(%d)!\n", uart->nport, ret);
	}
	return ret;
}

/*---------------------------------------------------------------------------*/
static void mtk_uart_syscore_resume(void)
{
	int ret = 0;
	unsigned long flags;

	if (bt_port) {
		struct mtk_uart *uart = bt_port;

		spin_lock_irqsave(&mtk_uart_bt_lock, flags);
		mtk_uart_switch_to_tx(uart);
		ret = uart_resume_port(&mtk_uart_drv, &uart->port);
		spin_unlock_irqrestore(&mtk_uart_bt_lock, flags);
		disable_irq(uart->port.irq);
		pr_debug("[UART%d] BT Resume(%d)!\n", uart->nport, ret);
	}
}

/*---------------------------------------------------------------------------*/
static int mtk_uart_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	struct mtk_uart *uart = platform_get_drvdata(pdev);
	static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ,  5);

	if (!uart)
		return -1;

	/* For console_suspend_enabled=0 */
	mtk_uart_save(uart);

	if ((uart->nport < UART_NR) && (uart != bt_port)) {
		ret = uart_suspend_port(&mtk_uart_drv, &uart->port);
		if (__ratelimit(&ratelimit))
			pr_debug("[UART%d] Suspend(%d)!\n", uart->nport, ret);
		mtk_uart_switch_rx_to_gpio(uart);
	}
	return ret;
}

/*---------------------------------------------------------------------------*/
static int mtk_uart_resume(struct platform_device *pdev)
{
	int ret = 0;
	static DEFINE_RATELIMIT_STATE(ratelimit, 5 * HZ,  5);

	struct mtk_uart *uart = platform_get_drvdata(pdev);

	if (uart && (uart->nport < UART_NR) && (uart != bt_port)) {
		mtk_uart_switch_to_rx(uart);
		ret = uart_resume_port(&mtk_uart_drv, &uart->port);
		if (__ratelimit(&ratelimit))
			pr_debug("[UART%d] Resume(%d)!\n", uart->nport, ret);
	}
	return ret;
}

/*---------------------------------------------------------------------------*/
static int mtk_uart_pm_suspend(struct device *device)
{
	struct platform_device *pdev;

	/*pr_debug("calling %s()\n", __func__);*/

	pdev = to_platform_device(device);
	WARN_ON(pdev == NULL);

	return mtk_uart_suspend(pdev, PMSG_SUSPEND);
}

static int mtk_uart_pm_resume(struct device *device)
{
	struct platform_device *pdev;

	/*pr_debug("calling %s()\n", __func__);*/

	pdev = to_platform_device(device);
	WARN_ON(pdev == NULL);

	return mtk_uart_resume(pdev);
}

static int mtk_uart_pm_freeze(struct device *device)
{
	struct platform_device *pdev;
	struct mtk_uart *uart = NULL;
	int ret = 0;
	int port_idx = 0;

	pdev = to_platform_device(device);
	WARN_ON(pdev == NULL);
	uart = platform_get_drvdata(pdev);

	port_idx = uart->nport;
	if (uart_freeze_enable[port_idx]) {
		ret = uart_suspend_port(&mtk_uart_drv, &uart->port);
		pr_warn("[%s] here.\n", __func__);
		/* To keeping uart idle state */
		/* tx pin:  idle->high   power down->low */
		mtk_uart_switch_tx_to_gpio(uart);
		pr_warn("[%s] done.\n", __func__);
	}

	return 0;	/* mtk_uart_suspend(pdev, PMSG_SUSPEND); */
}

static int mtk_uart_pm_restore(struct device *device)
{
	struct platform_device *pdev;
	struct mtk_uart *uart;
	int ret = 0;
	int port_idx = 0;

	pdev = to_platform_device(device);
	WARN_ON(pdev == NULL);
	uart = platform_get_drvdata(pdev);

	port_idx = uart->nport;
	if (uart_freeze_enable[port_idx]) {
		mtk_uart_switch_to_tx(uart);
		ret = uart_resume_port(&mtk_uart_drv, &uart->port);
		pr_warn("[%s] uart (%p) base: 0x%lx\n",
			__func__, uart, uart->base);
		pr_warn("[%s] nport %d, dma mode:%d\n",
			__func__, uart->nport, uart->dma_mode);
	}

	return mtk_uart_resume(pdev);
}

static int mtk_uart_pm_restore_noirq(struct device *device)
{
	struct mtk_uart *uart;

	uart = dev_get_drvdata(device);
	if (!uart || !uart->setting) {
		pr_warn("[%s] uart or uart->setting is null!!\n", __func__);
		return 0;
	}
	mtk_uart_fifo_set_trig(uart, uart->tx_trig, uart->rx_trig);
	irq_set_irq_type(uart->setting->irq_num, uart->setting->irq_flags);

	if (uart->tx_vfifo && uart->tx_mode == UART_TX_VFIFO_DMA)
		irq_set_irq_type(uart->tx_vfifo->irq_id,
				IRQF_LEVEL_TRIGGER_POLARITY);
	if (uart->rx_vfifo && uart->rx_mode == UART_RX_VFIFO_DMA)
		irq_set_irq_type(uart->rx_vfifo->irq_id,
				IRQF_LEVEL_TRIGGER_POLARITY);
	return 0;
}

/*---------------------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
#define mtk_uart_pm_suspend    NULL
#define mtk_uart_pm_resume NULL
#define mtk_uart_pm_freeze NULL
#define mtk_uart_pm_restore NULL
#define mtk_uart_pm_restore_noirq NULL
/*---------------------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*---------------------------------------------------------------------------*/
const struct dev_pm_ops mtk_uart_pm_ops = {
	.suspend = mtk_uart_pm_suspend,
	.resume = mtk_uart_pm_resume,
	.freeze = mtk_uart_pm_freeze,
	.thaw = mtk_uart_pm_restore,
	.poweroff = mtk_uart_pm_suspend,
	.restore = mtk_uart_pm_restore,
	.restore_noirq = mtk_uart_pm_restore_noirq,
};

/*---------------------------------------------------------------------------*/
static int mtk_uart_init_ports(void)
{
	int i;

#if defined(ENABLE_VFIFO)
	int idx;
	struct mtk_uart_vfifo *vfifo;
#endif
	void __iomem *apdma_uart0_base = 0;
	struct mtk_uart *uart;
	unsigned long base;

	spin_lock_init(&mtk_console_lock);

	apdma_uart0_base = get_apdma_uart0_base();

	for (i = 0; i < UART_NR; i++) {
		set_uart_default_settings(i);

		uart = &mtk_uarts[i];
		uart->setting = get_uart_default_settings(i);
#if defined(ENABLE_VFIFO)
		if (uart->setting->vff) {
			if (i * 2 < ARRAY_SIZE(mtk_uart_vfifo_port)) {
				for (idx = i * 2; idx < i * 2 + 2; idx++) {
					vfifo = &mtk_uart_vfifo_port[idx];
					vfifo->base =
					(apdma_uart0_base + 0x0080 * idx);
					vfifo->irq_id =
					get_uart_vfifo_irq_id(idx);
				}
			}
		}
#endif
		base = uart->setting->uart_base;
		uart->port.iotype = UPIO_MEM;
		/* for ioremap */
		uart->port.mapbase = uart->setting->uart_phys_base;

		uart->port.membase = (unsigned char __iomem *)base;
		uart->port.irq = uart->setting->irq_num;
		uart->port.fifosize = UART_FIFO_SIZE;
		uart->port.ops = &mtk_uart_ops;
		uart->port.flags = UPF_BOOT_AUTOCONF;
		uart->port.line = i;
		uart->port.uartclk = UART_SYSCLK;
		/* pll_for_uart = mt6575_get_bus_freq(); */
		/* uart->port.uartclk  = mt6575_get_bus_freq()*1000/4; */
		spin_lock_init(&uart->port.lock);
		uart->base = base;
		uart->auto_baud = CFG_UART_AUTOBAUD;
		uart->nport = i;
		uart->sysclk = UART_SYSCLK;	/* FIXME */
		/* uart->sysclk        = mt6575_get_bus_freq()*1000/4; */
		uart->dma_mode = uart->setting->dma_mode;
		uart->tx_mode = uart->setting->tx_mode;
		uart->rx_mode = uart->setting->rx_mode;
		uart->tx_trig = uart->setting->tx_trig;
		uart->rx_trig = uart->setting->rx_trig;
		uart->write_allow = mtk_uart_write_allow;
		uart->read_allow = mtk_uart_read_allow;
		uart->write_byte = mtk_uart_write_byte;
		uart->read_byte = mtk_uart_read_byte;
		uart->read_status = mtk_uart_read_status;
		uart->poweron_count = 0;
		uart->timeout_count = 0;
		uart->baudrate = 0;
		uart->custom_baud = 0;
		uart->registers.dll = 1;
		uart->registers.dlh = 0;
		uart->registers.ier = 0;
		uart->registers.lcr = 0;
		uart->registers.mcr = 0;
		uart->registers.fcr = 0;
		uart->registers.lsr = 0x60;
		uart->registers.efr = 0;
		uart->registers.highspeed = 0;
		uart->registers.sample_count = 0;
		uart->registers.sample_point = 0xff;
		uart->registers.fracdiv_l = 0;
		uart->registers.fracdiv_m = 0;
		uart->registers.escape_en = 0;
		uart->registers.guard = 0;
		uart->registers.rx_sel = 0;
		uart->evt_mask = (unsigned int)get_uart_evt_mask(i);

#if defined(CONFIG_MTK_SERIAL_MODEM_TEST)
		if (get_modem_uart(i)) {
			/* u32 dat = UART_READ32(HW_MISC);
			 * mtk does NOT has this register
			 */
			mtk_uart_power_up(uart);/* power up */
			/* reg_sync_writel(dat | mask[i], HW_MISC); */
			continue;
		}
#else
		/* mtk_uart_power_up(uart); */
		mtk_uart_disable_intrs(uart, UART_IER_ALL_INTS);

		irq_set_irq_type(uart->setting->irq_num,
			uart->setting->irq_flags);

		mtk_uart_fifo_init(uart);
		mtk_uart_set_mode(uart, uart->dma_mode);
		/* mtk_uart_power_down(uart); */
#endif
	}
#if defined(CONFIG_MTK_SERIAL_MODEM_TEST)
	/* NOTICE: for enabling modem test,
	 * UART4 needs to be disabled. Howerver,
	 * if CONFIG_MTK_SERIAL_CONSOLE
	 * is defined, resume will fail.
	 * Since the root cause is not clear,
	 * only disable the console-related
	 * function.
	 */
	/*printk("HW_MISC: 0x%08X\n", UART_READ32(HW_MISC)); */
	/* mtk does NOT has this register */
#endif
	return 0;
}

/*---------------------------------------------------------------------------*/
static const struct of_device_id apuart_of_ids[] = {
	{.compatible = "mediatek,AP_UART0",},
	{.compatible = "mediatek,AP_UART1",},
#ifndef CONFIG_FPGA_EARLY_PORTING
	{.compatible = "mediatek,AP_UART2",},
	{.compatible = "mediatek,AP_UART3",},
#if (UART_NR > 4)
	{.compatible = "mediatek,AP_UART4",},
#endif
#endif
	{.compatible = "mediatek,mt6735-uart",},
	{.compatible = "mediatek,mt6755-uart",},
	{.compatible = "mediatek,mt8173-uart",},
	{.compatible = "mediatek,mt6797-uart",},
	{.compatible = "mediatek,mt8163-uart",},
	{.compatible = "mediatek,mt8167-uart",},
	{.compatible = "mediatek,mtk-uart",},
	{.compatible = "mediatek,mt6759-uart",},
	{.compatible = "mediatek,mt6758-uart",},
	{}
};

static struct platform_driver mtk_uart_dev_drv = {
	.probe = mtk_uart_probe,
	.remove = mtk_uart_remove,
#ifdef CONFIG_PM
	.suspend = mtk_uart_suspend,
	.resume = mtk_uart_resume,
#endif
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = apuart_of_ids,
#ifdef CONFIG_PM
		   .pm = &mtk_uart_pm_ops,
#endif

		}
};

#ifdef CONFIG_PM
static struct syscore_ops mtk_uart_syscore_ops = {
	.suspend = mtk_uart_syscore_suspend,
	.resume = mtk_uart_syscore_resume,
};

static int __init mtk_uart_init_ops(void)
{
	register_syscore_ops(&mtk_uart_syscore_ops);
	return 0;
}
#endif
/*---------------------------------------------------------------------------*/
static int __init mtk_uart_init(void)
{
	int ret = 0;

	tx_history.buffer = kzalloc(UART_HISTORY_DATA_SIZE, GFP_KERNEL);
	rx_history.buffer = kzalloc(UART_HISTORY_DATA_SIZE, GFP_KERNEL);
	tx_history.index = -1;
	rx_history.index = -1;

	if (!tx_history.buffer || !rx_history.buffer)
		return -ENOMEM;

#ifndef CONFIG_MTK_SERIAL_CONSOLE
	mtk_uart_init_ports();
#endif

#if defined(ENABLE_SYSFS)
	mtk_uart_sysfs();
#endif

	ret = uart_register_driver(&mtk_uart_drv);

	if (ret)
		return ret;

	ret = platform_driver_register(&mtk_uart_dev_drv);

	if (ret) {
		uart_unregister_driver(&mtk_uart_drv);
		return ret;
	}
#ifdef CONFIG_PM
	mtk_uart_init_ops();
#endif

#ifdef ENABLE_RAW_DATA_DUMP
	mtk_uart_init_debug_spinlock();
#endif
	spin_lock_init(&mtk_uart_bt_lock);
	return ret;
}

/*---------------------------------------------------------------------------*/
static void __exit mtk_uart_exit(void)
{
	platform_driver_unregister(&mtk_uart_dev_drv);
	uart_unregister_driver(&mtk_uart_drv);
}

/*---------------------------------------------------------------------------*/
#ifdef ENABLE_UART_SLEEP
int request_uart_to_sleep(void)
{
	u32 val1;
	int i = 0;
	int uart_idx = 0;
	struct mtk_uart *uart;
	unsigned long base;

	for (uart_idx = 0; uart_idx < UART_NR; uart_idx++) {
/*
 *#if defined(CONFIG_MTK_HDMI_SUPPORT)
 *#ifdef MHL_UART_SHARE_PIN
 *		{
 *			for K2 uart2 and mhl share pin,
 *			if mhl is in low power mode,
 *			uart rx is not working, so bypass it.
 *			if ((is_hdmi_active() == 0) && (uart_idx == 1))
 *				continue;
 *		}
 *#endif
 *#endif
 */
		uart = &mtk_uarts[uart_idx];
		base = uart->base;
		if (uart->poweron_count > 0) {
			/* request UART to sleep */
			val1 = UART_READ32(UART_SLEEP_REQ);
			reg_sync_writel(val1 | UART_CLK_OFF_REQ,
				UART_SLEEP_REQ);

			/* wait for UART to ACK */
			while (!(UART_READ32(UART_SLEEP_ACK) &
					UART_CLK_OFF_ACK)) {
				if (i++ >= WAIT_UART_ACK_TIMES) {
					reg_sync_writel(val1,
							UART_SLEEP_REQ);
					pr_err_ratelimited(
						"[UART]CANNOT GET UART[%d] SLEEP ACK\n",
							uart_idx);
					/* dump_uart_reg(); */
					return -EBUSY;
				}
				udelay(10);
			}
		} else {
			/* printk("[UART%d] clock is off\n", uart->nport); */
		}
	}

	return 0;
}
EXPORT_SYMBOL(request_uart_to_sleep);

int request_uart_to_wakeup(void)
{
	/* printk(KERN_ERR "Request UART sleep\n"); */
	u32 val1;
	int i = 0;
	int uart_idx = 0;
	struct mtk_uart *uart;
	unsigned long base;

	for (uart_idx = 0; uart_idx < UART_NR; uart_idx++) {
		uart = &mtk_uarts[uart_idx];
		base = uart->base;
		if (uart->poweron_count > 0) {
			/* wakeup uart */
			val1 = UART_READ32(UART_SLEEP_REQ);
			reg_sync_writel(val1 & (~UART_CLK_OFF_REQ),
				UART_SLEEP_REQ);

			/* wait for UART to ACK */
			while ((UART_READ32(UART_SLEEP_ACK)
					& UART_CLK_OFF_ACK)) {
				if (i++ >= WAIT_UART_ACK_TIMES) {
					reg_sync_writel(val1, UART_SLEEP_REQ);
					pr_err("[UART]CANNOT GET UART[%d] WAKE ACK\n",
							uart_idx);
					/* dump_uart_reg(); */
					return -EBUSY;
				}
				udelay(10);
			}
		} else {
			/* printk("[UART%d] clock is off\n", uart->nport); */
		}
	}

	return 0;
}
EXPORT_SYMBOL(request_uart_to_wakeup);

#endif
/*---------------------------------------------------------------------------*/
module_init(mtk_uart_init);
module_exit(mtk_uart_exit);
/*---------------------------------------------------------------------------*/
MODULE_AUTHOR("MTK");
MODULE_DESCRIPTION("MTK Serial Port Driver");
MODULE_LICENSE("GPL");
