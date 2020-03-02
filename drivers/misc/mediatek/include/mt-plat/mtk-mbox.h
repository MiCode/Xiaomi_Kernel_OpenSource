/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __MTK_MBOX_H__
#define __MTK_MBOX_H__

#include <linux/platform_device.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>

/*
 * mbox slot size defination
 * 1 slot for 4 bytes
 *
 */
#define MBOX_SLOT_SIZE 4

typedef int (*mbox_rx_cb_t)(void *);
typedef int (*mbox_pin_cb_t)(unsigned int ipi_id, void *prdata, void *data,
	unsigned int len);

/*
 * mbox pin structure, this is for send defination,
 * ipi=endpoint=pin
 * mbox     : (mbox number)mbox number of the pin, up to 16(plt)
 * offset   : (slot)msg offset in share memory, up to 1024*4 KB(plt)
 * send_opt : (opt)send opt, 0:send ,1: send for response(plt)
 * lock     : (lock)polling lock 0:unuse,1:used
 * msg_size : (slot)message size in words, 4 bytes alignment(plt)
 * pin_index  : (bit offset)pin index in the mbox(plt)
 * chan_id    : (u32) ipc channel id(plt)
 * mutex      : (mutex)mutex for remote response
 * completion : (completion)completion for remote response
 * pin_lock   : (spinlock_t)lock of the pin
 */
struct mtk_mbox_pin_send {
	unsigned int mbox     : 4,
				 offset   : 20,
				 send_opt : 2,
				 lock    : 2;
	unsigned int msg_size;
	unsigned int pin_index;
	unsigned int chan_id;
	struct mutex mutex_send;
	struct completion comp_ack;  // to remove
	spinlock_t pin_lock;
};

/*
 * mbox pin structure, this is for receive defination,
 * ipi=endpoint=pin
 * mbox     : (mbox number)mbox number of the pin, up to 16(plt)
 * offset   : (slot)msg offset in share memory, up to 1024*4 KB(plt)
 * recv_opt : (opt)recv option,  0:receive ,1: response(plt)
 * lock     : (lock)polling lock 0:unuse,1:used
 * buf_full_opt : (opt)buffer option 0:drop, 1:assert, 2:overwrite(plt)
 * cb_ctx_opt : (opt)callback option 0:isr context, 1:process context(plt)
 * msg_size   : (slot)msg used slots in the mbox, 4 bytes alignment(plt)
 * pin_index  : (bit offset)pin index in the mbox(plt)
 * chan_id : (u32) ipc channel id(plt)
 * notify     : (completion)notify process
 * mbox_pin_cb: (cb)cb function
 * pin_buf : (void*)buffer point
 * prdata  : (void*)private data
 * pin_lock: (spinlock_t)lock of the pin
 */
struct mtk_mbox_pin_recv {
	unsigned int mbox     : 4,
				 offset   : 20,
				 recv_opt     : 2,
				 lock         : 2,
				 buf_full_opt : 2,
				 cb_ctx_opt   : 2;
	unsigned int msg_size;
	unsigned int pin_index;
	unsigned int chan_id;
	struct completion notify;
	mbox_pin_cb_t mbox_pin_cb;
	void *pin_buf;
	void *prdata;
	spinlock_t pin_lock;
};


/*
 * mtk mbox device,Mbox is a dedicate hardware of a tinysys consists of:
 * 1) a share memory tightly coupled to the tinysys
 * 2) several IRQs
 *
 * @pre_cb: the callback handler in the begin of mbox receiving ipi
 * @post_cb: the callback handler in the end of mbox receiving ipi
 * @prdata: private data for the callback use
 */
struct mtk_mbox_device {
	/* Identity of the device */
	const char *name;
	unsigned int id;
	/* Following are platform specific interface*/
	struct mtk_mbox_pin_recv *pin_recv_table;
	struct mtk_mbox_pin_send *pin_send_table;
	struct mtk_mbox_info *info_table;
	unsigned int count;
	unsigned int recv_count;
	unsigned int send_count;
	void (*ipi_cb)(struct mtk_mbox_pin_recv *pin_recv);
	mbox_rx_cb_t pre_cb;
	mbox_rx_cb_t post_cb;
	void (*memcpy_to_tiny)(void __iomem *dest, const void *src, int size);
	void (*memcpy_from_tiny)(void *dest, const void __iomem *src, int size);
	void *prdata;
};

/*
 * mbox callback function context defination
 * 0: isr context, 1:process context
 */
enum MBOX_PIN_CTX {
	MBOX_CB_IN_ISR   = 0,
	MBOX_CB_IN_PROCESS  = 1,
};

/*
 * mbox buffer full action defination
 * 0:drop, 1:assert, 2:overwrite
 */
enum MBOX_BUF_OPT {
	MBOX_BUF_FULL_DROP       = 0,
	MBOX_BUF_FULL_ASSERT     = 1,
	MBOX_BUF_FULL_OVERWRITE  = 2,
	MBOX_BUF_COPY_DONE       = 3,
	MBOX_BUF_FULL_RET        = 4,
};

/*
 * mbox recv action defination
 * 0:for recv, 1:for response
 */
enum MBOX_RECV_OPT {
	MBOX_RECV         = 0,
	MBOX_RESPONSE     = 1,
};

/*
 * mbox information
 *
 * mbdev  :mbox device
 * irq_num:identity of mbox irq
 * id     :mbox id
 * slot   :how many slots that mbox used
 * opt    :option for tx mode, 0:mbox, 1:share memory 2:queue
 * enable :mbox status, 0:disable, 1: enable
 * is64d  :mbox is64d status, 0:32d, 1: 64d
 * base   :mbox base address
 * set_irq_reg  : mbox set irq register
 * clr_irq_reg  : mbox clear irq register
 * init_base_reg: mbox initialize register
 * mbox lock    : lock of mbox
 */
struct mtk_mbox_info {
	struct mtk_mbox_device *mbdev;
	int irq_num;
	unsigned int id;
	unsigned int slot;
	unsigned int opt;
	bool enable;
	bool is64d;
	void __iomem *base;
	void __iomem *set_irq_reg;
	void __iomem *clr_irq_reg;
	void __iomem *init_base_reg;
	void __iomem *send_status_reg;
	void __iomem *recv_status_reg;
	spinlock_t mbox_lock;
};

/*
 * mbox return value defination
 */
enum MBOX_RETURN {
	MBOX_CONFIG_ERROR = -3,
	MBOX_IRQ_ERROR    = -2,
	MBOX_PARA_ERROR   = -1,
	MBOX_DONE         = 0,
	MBOX_PIN_BUSY     = 1,
};

/*
 * mbox message options
 * 0: mbox
 * 1: share memory
 * 2: mbox with queue
 * 3: share memory with queue
 */
enum {
	MBOX_OPT_DIRECT = 0,
	MBOX_OPT_SMEM   = 1,
	MBOX_OPT_QUEUE_DIR  = 2,
	MBOX_OPT_QUEUE_SMEM  = 3,
};

/*
 * mtk ipi message header
 *
 * id       :message id
 * len      :data length in byte
 * options  :options
 * reserved :reserved
 */
struct mtk_ipi_msg_hd {
	uint32_t id;
	uint32_t len;
	uint32_t options;
	uint32_t reserved;
};

struct mtk_ipi_msg {
	struct mtk_ipi_msg_hd ipihd;
	void *data;
};

int mtk_mbox_write_hd(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int slot, void *msg);
int mtk_mbox_read_hd(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int slot, void *dest);
int mtk_mbox_write(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int slot, void *data, unsigned int len);
int mtk_mbox_read(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int slot, void *data, unsigned int len);
int mtk_mbox_clr_irq(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int irq);
int mtk_mbox_trigger_irq(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int irq);
unsigned int mtk_mbox_read_irq_status(struct mtk_mbox_device *mbdev,
		unsigned int mbox);
int mtk_mbox_set_base_addr(struct mtk_mbox_device *mbdev, unsigned int mbox,
		unsigned int addr);
unsigned int mtk_mbox_check_send_irq(struct mtk_mbox_device *mbdev,
		unsigned int mbox, unsigned int pin_index);
int mtk_mbox_probe(struct platform_device *pdev, struct mtk_mbox_device *mbdev,
		unsigned int mbox);
int mtk_mbox_cb_register(struct mtk_mbox_device *mbdev, unsigned int pin_offset,
		mbox_pin_cb_t mbox_pin_cb, void *prdata);
int mtk_mbox_polling(struct mtk_mbox_device *mbdev, unsigned int mbox,
		void *data, struct mtk_mbox_pin_recv *pin_recv);
int mtk_smem_init(struct platform_device *pdev, struct mtk_mbox_device *mbdev,
		unsigned int mbox, void __iomem *base,
		void __iomem *set_irq_reg, void __iomem *clr_irq_reg,
		void __iomem *send_status_reg, void __iomem *recv_status_reg);
void mtk_mbox_info_dump(struct mtk_mbox_device *mbdev);

#endif

