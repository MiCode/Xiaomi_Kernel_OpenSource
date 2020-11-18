/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __MTK_CMDQ_MAILBOX_H__
#define __MTK_CMDQ_MAILBOX_H__

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/trace_events.h>

#if !IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
#define cmdq_util_msg(f, args...) cmdq_msg(f, ##args)
#define cmdq_util_err(f, args...) cmdq_dump(f, ##args)
#endif

/* see also gce platform binding header */
#define CMDQ_NO_TIMEOUT			0xffffffff
#define CMDQ_TIMEOUT_DEFAULT		1000

#if IS_ENABLED(CONFIG_MACH_MT6768)
#define CMDQ_THR_MAX_COUNT		16
#else
#define CMDQ_THR_MAX_COUNT		24
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
#define CMDQ_EVENT_MAX			0x3FF

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

struct cmdq_buf_pool {
	struct dma_pool *pool;
	atomic_t *cnt;
	u32 *limit;
};

struct cmdq_pkt {
	struct list_head	buf;
	size_t			avail_buf_size; /* available buf size */
	size_t			cmd_buf_size; /* command occupied size */
	size_t			buf_size; /* real buffer size */
	u32			priority;
	struct cmdq_task_cb	cb;
	struct cmdq_task_cb	err_cb;
	void			*user_data;
	void			*cl;
	struct device		*dev;	/* client assigned dev */
	bool			loop;
	void			*flush_item;
	struct completion	cmplt;
	struct cmdq_buf_pool	cur_pool;
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	bool			mdp;
	u64			rec_submit;
	u64			rec_trigger;
	u64			rec_wait;
	u64			rec_irq;

#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
	void			*sec_data;
#endif
#endif	/* end of CONFIG_MTK_CMDQ_MBOX_EXT */
	bool			task_alloc;
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
	bool			occupied;
	bool			dirty;
	u64			timer_mod;
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

/* CMDQ FTRACE */
#define cmdq_trace_begin(fmt, args...) do { \
	char buf[MAX_INPUT]; \
	s32 len = snprintf( \
		buf, sizeof(buf), "B|%d|"fmt"\n", current->tgid, ##args); \
	if (len >= MAX_INPUT) \
		buf[MAX_INPUT - 1] = '\n'; \
	tracing_mark_write(buf); \
} while (0)

#define cmdq_trace_end() do { \
	tracing_mark_write("E\n"); \
} while (0)

#define cmdq_trace_c(fmt, args...) do { \
	char buf[MAX_INPUT]; \
	s32 len = snprintf( \
		buf, sizeof(buf), "C|"fmt, ##args); \
	if (len >= MAX_INPUT) \
		buf[MAX_INPUT - 1] = '\n'; \
	tracing_mark_write(buf); \
} while (0)

extern int cmdq_trace;
#define cmdq_trace_ex_begin(fmt, args...) do { \
	if (cmdq_trace) \
		cmdq_trace_begin(fmt, ##args); \
} while (0)

#define cmdq_trace_ex_end(fmt, args...) do { \
	if (cmdq_trace) \
		cmdq_trace_end(fmt, ##args); \
} while (0)

dma_addr_t cmdq_thread_get_pc(struct cmdq_thread *thread);
dma_addr_t cmdq_thread_get_end(struct cmdq_thread *thread);
void cmdq_thread_set_spr(struct mbox_chan *chan, u8 id, u32 val);
void cmdq_init_cmds(void *dev_cmdq);
void cmdq_mbox_channel_stop(struct mbox_chan *chan);
void cmdq_dump_core(struct mbox_chan *chan);
void cmdq_thread_dump_spr(struct cmdq_thread *thread);
void cmdq_thread_dump(struct mbox_chan *chan, struct cmdq_pkt *cl_pkt,
	u64 **inst_out, dma_addr_t *pc_out);
void cmdq_thread_dump_all(void *mbox_cmdq);
void cmdq_thread_dump_all_seq(void *mbox_cmdq, struct seq_file *seq);
void cmdq_mbox_thread_remove_task(struct mbox_chan *chan,
	struct cmdq_pkt *pkt);
void cmdq_mbox_enable(void *chan);
void cmdq_mbox_disable(void *chan);
s32 cmdq_mbox_get_usage(void *chan);
void *cmdq_mbox_get_base(void *chan);
phys_addr_t cmdq_mbox_get_base_pa(void *chan);
s32 cmdq_mbox_thread_reset(void *chan);
s32 cmdq_mbox_thread_suspend(void *chan);
void cmdq_mbox_thread_disable(void *chan);
u32 cmdq_mbox_get_thread_timeout(void *chan);
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
void cmdq_event_verify(void *chan, u16 event_id);
void tracing_mark_write(const char *buf);
unsigned long cmdq_get_tracing_mark(void);
u32 cmdq_thread_timeout_backup(struct cmdq_thread *thread, const u32 ms);
void cmdq_thread_timeout_restore(struct cmdq_thread *thread, const u32 ms);

#if IS_ENABLED(CONFIG_MMPROFILE)
void cmdq_mmp_wait(struct mbox_chan *chan, void *pkt);
#endif

#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
s32 cmdq_sec_insert_backup_cookie(struct cmdq_pkt *pkt);
#endif

#endif /* __MTK_CMDQ_MAILBOX_H__ */
