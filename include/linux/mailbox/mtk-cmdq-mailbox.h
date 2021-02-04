/*
 * Copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_CMDQ_MAILBOX_H__
#define __MTK_CMDQ_MAILBOX_H__

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>

/* see also gce platform binding header */
#define CMDQ_NO_TIMEOUT			0xffffffff
#define CMDQ_TIMEOUT_DEFAULT		1000

#if IS_ENABLED(CONFIG_MACH_MT6779) || IS_ENABLED(CONFIG_MACH_MT6763)
#define CMDQ_THR_MAX_COUNT		24
#else
#define CMDQ_THR_MAX_COUNT		16
#endif
#define CMDQ_INST_SIZE			8 /* instruction is 64-bit */
#define CMDQ_SUBSYS_SHIFT		16
#define CMDQ_OP_CODE_SHIFT		24
#define CMDQ_JUMP_PASS			(CMDQ_INST_SIZE)
#define CMDQ_CMD_BUFFER_SIZE		(PAGE_SIZE - 32 * CMDQ_INST_SIZE)
#define CMDQ_BUF_ALLOC_SIZE		(PAGE_SIZE)
#define CMDQ_NUM_CMD(cmd_size)		((cmd_size) / CMDQ_INST_SIZE)

#define CMDQ_WFE_UPDATE			BIT(31)
#define CMDQ_WFE_UPDATE_VALUE		BIT(16)
#define CMDQ_WFE_WAIT			BIT(15)
#define CMDQ_WFE_WAIT_VALUE		0x1

/*
 * CMDQ_CODE_MASK:
 *   set write mask
 *   format: op mask
 * CMDQ_CODE_WRITE:
 *   write value into target register
 *   format: op subsys address value
 * CMDQ_CODE_JUMP:
 *   jump by offset
 *   format: op offset
 * CMDQ_CODE_WFE:
 *   wait for event and clear
 *   it is just clear if no wait
 *   format: [wait]  op event update:1 to_wait:1 wait:1
 *           [clear] op event update:1 to_wait:0 wait:0
 * CMDQ_CODE_EOC:
 *   end of command
 *   format: op irq_flag
 */
enum cmdq_code {
	CMDQ_CODE_READ  = 0x01,
	CMDQ_CODE_MASK = 0x02,
	CMDQ_CODE_MOVE = 0x02,
	CMDQ_CODE_WRITE = 0x04,
	CMDQ_CODE_POLL  = 0x08,
	CMDQ_CODE_JUMP = 0x10,
	CMDQ_CODE_WFE = 0x20,
	CMDQ_CODE_EOC = 0x40,

	/* these are pseudo op code defined by SW */
	/* for instruction generation */
	CMDQ_CODE_WRITE_FROM_MEM = 0x05,
	CMDQ_CODE_WRITE_FROM_REG = 0x07,
	CMDQ_CODE_SET_TOKEN = 0x21,	/* set event */
	CMDQ_CODE_WAIT_NO_CLEAR = 0x22,	/* wait event, but don't clear it */
	CMDQ_CODE_CLEAR_TOKEN = 0x23,	/* clear event */
	CMDQ_CODE_RAW = 0x24,	/* allow entirely custom arg_a/arg_b */
	CMDQ_CODE_PREFETCH_ENABLE = 0x41,	/* enable prefetch marker */
	CMDQ_CODE_PREFETCH_DISABLE = 0x42,	/* disable prefetch marker */

	CMDQ_CODE_READ_S = 0x80,	/* read operation (v3 only) */
	CMDQ_CODE_WRITE_S = 0x90,	/* write operation (v3 only) */
	/* write with mask operation (v3 only) */
	CMDQ_CODE_WRITE_S_W_MASK = 0x91,
	CMDQ_CODE_LOGIC = 0xa0,	/* logic operation */
	CMDQ_CODE_JUMP_C_ABSOLUTE = 0xb0, /* conditional jump (absolute) */
	CMDQ_CODE_JUMP_C_RELATIVE = 0xb1, /* conditional jump (related) */
};

struct cmdq_cb_data {
	s32		err;
	void		*data;
};

typedef void (*cmdq_async_flush_cb)(struct cmdq_cb_data data);

struct cmdq_task_cb {
	cmdq_async_flush_cb	cb;
	void			*data;
};

struct cmdq_pkt_buffer {
	struct list_head	list_entry;
	void			*va_base;
	dma_addr_t		pa_base;
	bool			use_pool;
	bool			map;
};

typedef s32 (*cmdq_buf_cb)(struct cmdq_pkt_buffer *buf);

struct cmdq_pkt {
	struct list_head	buf;
	size_t			avail_buf_size; /* available buf size */
	size_t			cmd_buf_size; /* command occupied size */
	size_t			buf_size; /* real buffer size */
	u32			priority;
	struct cmdq_task_cb	cb;
	struct cmdq_task_cb	err_cb;
	void			*user_data;
	void			*priv;
	struct device		*dev;
	bool			loop;
	void			*buf_pool;
};

struct cmdq_thread {
	struct mbox_chan	*chan;
	void __iomem		*base;
	phys_addr_t		gce_pa;
	struct list_head	task_busy_list;
	struct timer_list	timeout;
	u32			timeout_ms;
	struct work_struct	timeout_work;
	u32			priority;
	u32			idx;
	bool			dirty;
	bool			occupied;
};

extern int mtk_cmdq_log;
#define cmdq_log(fmt, args...) \
do { \
	if (mtk_cmdq_log) \
		pr_notice("[cmdq] "fmt" @%s,%u\n", \
		##args, __func__, __LINE__); \
} while (0)


/* MTK only functions */

#define cmdq_msg(fmt, args...) \
	pr_notice("[cmdq] "fmt"\n", ##args)

#define cmdq_err(fmt, args...) \
	pr_notice("[cmdq][err] "fmt" @%s,%u\n", ##args, __func__, __LINE__)

#define cmdq_dump(fmt, args...) \
	pr_notice("[cmdq][err] "fmt"\n", ##args)

void cmdq_mbox_channel_stop(struct mbox_chan *chan);
void cmdq_thread_dump_err(struct mbox_chan *chan);
void cmdq_mbox_thread_remove_task(struct mbox_chan *chan,
	struct cmdq_pkt *pkt);
s32 cmdq_mbox_thread_reset(void *chan);
s32 cmdq_mbox_thread_suspend(void *chan);
void cmdq_mbox_thread_disable(void *chan);
u32 cmdq_mbox_set_thread_timeout(void *chan, u32 timeout);
s32 cmdq_mbox_chan_id(void *chan);
s32 cmdq_task_get_thread_pc(struct mbox_chan *chan, dma_addr_t *pc_out);
s32 cmdq_task_get_thread_irq(struct mbox_chan *chan, u32 *irq_out);
s32 cmdq_task_get_thread_irq_en(struct mbox_chan *chan, u32 *irq_en_out);
s32 cmdq_task_get_thread_end_addr(struct mbox_chan *chan,
	dma_addr_t *end_addr_out);
s32 cmdq_task_get_task_info_from_thread_unlock(struct mbox_chan *chan,
	struct list_head *task_list_out, u32 *task_num_out);
s32 cmdq_task_get_pkt_from_thread(struct mbox_chan *chan,
	struct cmdq_pkt **pkt_list_out, u32 pkt_list_size, u32 *pkt_count_out);
void cmdq_set_event(void *chan, u16 event_id);
void cmdq_clear_event(void *chan, u16 event_id);
u32 cmdq_get_event(void *chan, u16 event_id);

#endif /* __MTK_CMDQ_MAILBOX_H__ */
