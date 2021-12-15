/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#ifndef __MTK_CMDQ_H__
#define __MTK_CMDQ_H__

#include <linux/mailbox_client.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>

#define CMDQ_SPR_FOR_TEMP		0
#define CMDQ_THR_SPR_IDX0		0
#define CMDQ_THR_SPR_IDX1		1
#define CMDQ_THR_SPR_IDX2		2
#define CMDQ_THR_SPR_IDX3		3
#define CMDQ_THR_SPR_MAX		4

#define CMDQ_TPR_ID			56
#define CMDQ_HANDSHAKE_REG		59
#define CMDQ_GPR_CNT_ID			32
#define CMDQ_CPR_STRAT_ID		0x8000
#define CMDQ_CPR_TPR_MASK		0x8000
#define CMDQ_CPR_DISP_CNT		0x8001
#define CMDQ_EVENT_MAX			0x3FF
#define CMDQ_CPR_DDR_USR_CNT		0x8002
#define CMDQ_CPR_SLP_GPR_MAX		0x8003

#define CMDQ_GPR_CNT_ID			32
#define CMDQ_CPR_STRAT_ID		0x8000
#define SUBSYS_NO_SUPPORT		99

/* GCE provide 26M timer, thus each tick 1/26M second,
 * which is, 1 microsecond = 26 ticks
 */
#define CMDQ_US_TO_TICK(_t)		(_t * 26)
#define CMDQ_TICK_TO_US(_t)		(do_div(_t, 26))

extern int gce_shift_bit;


#define CMDQ_REG_SHIFT_ADDR(addr)	((addr) >> gce_shift_bit)
#define CMDQ_REG_REVERT_ADDR(addr)	((addr) << gce_shift_bit)

/* GCE provide 32/64 bit General Purpose Register (GPR)
 * use as data cache or address register
 *	 32bit: R0-R15
 *	 64bit: P0-P7
 * Note:
 *	R0-R15 and P0-P7 actullay share same memory
 *	R0 use as mask in instruction, thus be care of use R1/P0.
 */
enum cmdq_gpr {
	/* 32bit R0 to R15 */
	CMDQ_GPR_R00 = 0x00,
	CMDQ_GPR_R01 = 0x01,
	CMDQ_GPR_R02 = 0x02,
	CMDQ_GPR_R03 = 0x03,
	CMDQ_GPR_R04 = 0x04,
	CMDQ_GPR_R05 = 0x05,
	CMDQ_GPR_R06 = 0x06,
	CMDQ_GPR_R07 = 0x07,
	CMDQ_GPR_R08 = 0x08,
	CMDQ_GPR_R09 = 0x09,
	CMDQ_GPR_R10 = 0x0A,
	CMDQ_GPR_R11 = 0x0B,
	CMDQ_GPR_R12 = 0x0C,
	CMDQ_GPR_R13 = 0x0D,
	CMDQ_GPR_R14 = 0x0E,
	CMDQ_GPR_R15 = 0x0F,

	/* 64bit P0 to P7 */
	CMDQ_GPR_P0 = 0x10,
	CMDQ_GPR_P1 = 0x11,
	CMDQ_GPR_P2 = 0x12,
	CMDQ_GPR_P3 = 0x13,
	CMDQ_GPR_P4 = 0x14,
	CMDQ_GPR_P5 = 0x15,
	CMDQ_GPR_P6 = 0x16,
	CMDQ_GPR_P7 = 0x17,
};

/* Define GCE tokens which not change by platform */
enum gce_event {
	/* GCE handshake event 768~783 */
	CMDQ_EVENT_HANDSHAKE = 768,

	CMDQ_TOKEN_SECURE_THR_EOF = 647,
	CMDQ_TOKEN_TPR_LOCK = 652,

	/* GPR timer token, 994 to 1009 (for gpr r0 to r15) */
	CMDQ_EVENT_GPR_TIMER = 994,
};

struct cmdq_pkt;

struct cmdq_subsys {
	u32 base;
	u8 id;
};

struct cmdq_base {
	struct cmdq_subsys subsys[32];
	u8 count;
	u16 cpr_base;
	u8 cpr_cnt;
};

struct cmdq_client {
	struct mbox_client client;
	struct mbox_chan *chan;
	void *cl_priv;
	struct mutex chan_mutex;
};

struct cmdq_operand {
	/* register type */
	bool reg;
	union {
		/* index */
		u16 idx;
		/* value */
		u16 value;
	};
};

enum CMDQ_LOGIC_ENUM {
	CMDQ_LOGIC_ASSIGN = 0,
	CMDQ_LOGIC_ADD = 1,
	CMDQ_LOGIC_SUBTRACT = 2,
	CMDQ_LOGIC_MULTIPLY = 3,
	CMDQ_LOGIC_XOR = 8,
	CMDQ_LOGIC_NOT = 9,
	CMDQ_LOGIC_OR = 10,
	CMDQ_LOGIC_AND = 11,
	CMDQ_LOGIC_LEFT_SHIFT = 12,
	CMDQ_LOGIC_RIGHT_SHIFT = 13
};

enum CMDQ_CONDITION_ENUM {
	CMDQ_CONDITION_ERROR = -1,

	/* these are actual HW op code */
	CMDQ_EQUAL = 0,
	CMDQ_NOT_EQUAL = 1,
	CMDQ_GREATER_THAN_AND_EQUAL = 2,
	CMDQ_LESS_THAN_AND_EQUAL = 3,
	CMDQ_GREATER_THAN = 4,
	CMDQ_LESS_THAN = 5,

	CMDQ_CONDITION_MAX,
};

struct cmdq_flush_completion {
	struct cmdq_pkt *pkt;
	struct completion cmplt;
	s32 err;
};

u32 cmdq_subsys_id_to_base(struct cmdq_base *cmdq_base, int id);

/**
 * cmdq_pkt_realloc_cmd_buffer() - reallocate command buffer for CMDQ packet
 * @pkt:	the CMDQ packet
 * @size:	the request size
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_realloc_cmd_buffer(struct cmdq_pkt *pkt, size_t size);

/**
 * cmdq_register_device() - register device which needs CMDQ
 * @dev:	device for CMDQ to access its registers
 *
 * Return: cmdq_base pointer or NULL for failed
 */
struct cmdq_base *cmdq_register_device(struct device *dev);

/**
 * cmdq_mbox_create() - create CMDQ mailbox client and channel
 * @dev:	device of CMDQ mailbox client
 * @index:	index of CMDQ mailbox channel
 *
 * Return: CMDQ mailbox client pointer
 */
struct cmdq_client *cmdq_mbox_create(struct device *dev, int index);
void cmdq_mbox_stop(struct cmdq_client *cl);

void cmdq_mbox_pool_set_limit(struct cmdq_client *cl, u32 limit);
void cmdq_mbox_pool_create(struct cmdq_client *cl);
void cmdq_mbox_pool_clear(struct cmdq_client *cl);

void *cmdq_mbox_buf_alloc(struct device *dev, dma_addr_t *pa_out);
void cmdq_mbox_buf_free(struct device *dev, void *va, dma_addr_t pa);

s32 cmdq_dev_get_event(struct device *dev, const char *name);

struct cmdq_pkt_buffer *cmdq_pkt_alloc_buf(struct cmdq_pkt *pkt);

void cmdq_pkt_free_buf(struct cmdq_pkt *pkt);

s32 cmdq_pkt_add_cmd_buffer(struct cmdq_pkt *pkt);

/**
 * cmdq_mbox_destroy() - destroy CMDQ mailbox client and channel
 * @client:	the CMDQ mailbox client
 */
void cmdq_mbox_destroy(struct cmdq_client *client);

/**
 * cmdq_pkt_create() - create a CMDQ packet
 * @client:	the CMDQ mailbox client
 *
 * Return: CMDQ packet pointer
 */
struct cmdq_pkt *cmdq_pkt_create(struct cmdq_client *client);

/**
 * cmdq_pkt_destroy() - destroy the CMDQ packet
 * @pkt:	the CMDQ packet
 */
void cmdq_pkt_destroy(struct cmdq_pkt *pkt);

u64 *cmdq_pkt_get_va_by_offset(struct cmdq_pkt *pkt, size_t offset);

dma_addr_t cmdq_pkt_get_pa_by_offset(struct cmdq_pkt *pkt, u32 offset);

dma_addr_t cmdq_pkt_get_curr_buf_pa(struct cmdq_pkt *pkt);

void *cmdq_pkt_get_curr_buf_va(struct cmdq_pkt *pkt);

s32 cmdq_pkt_append_command(struct cmdq_pkt *pkt, u16 arg_c, u16 arg_b,
	u16 arg_a, u8 s_op, u8 arg_c_type, u8 arg_b_type, u8 arg_a_type,
	enum cmdq_code code);

s32 cmdq_pkt_move(struct cmdq_pkt *pkt, u16 reg_idx, u64 value);

s32 cmdq_pkt_read(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	dma_addr_t src_addr, u16 dst_reg_idx);

s32 cmdq_pkt_read_reg(struct cmdq_pkt *pkt, u8 subsys, u16 offset,
	u16 dst_reg_idx);

s32 cmdq_pkt_read_addr(struct cmdq_pkt *pkt, dma_addr_t addr, u16 dst_reg_idx);

s32 cmdq_pkt_write_reg(struct cmdq_pkt *pkt, u8 subsys,
	u16 offset, u16 src_reg_idx, u32 mask);

s32 cmdq_pkt_write_value(struct cmdq_pkt *pkt, u8 subsys,
	u16 offset, u32 value, u32 mask);

s32 cmdq_pkt_write_reg_addr(struct cmdq_pkt *pkt, dma_addr_t addr,
	u16 src_reg_idx, u32 mask);

s32 cmdq_pkt_write_value_addr(struct cmdq_pkt *pkt, dma_addr_t addr,
	u32 value, u32 mask);

s32 cmdq_pkt_store_value(struct cmdq_pkt *pkt, u16 indirect_dst_reg_idx,
	u16 dst_addr_low, u32 value, u32 mask);

s32 cmdq_pkt_store_value_reg(struct cmdq_pkt *pkt, u16 indirect_dst_reg_idx,
	u16 dst_addr_low, u16 indirect_src_reg_idx, u32 mask);

s32 cmdq_pkt_store64_value_reg(struct cmdq_pkt *pkt,
	u16 indirect_dst_reg_idx, u16 indirect_src_reg_idx);

s32 cmdq_pkt_write_indriect(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	dma_addr_t addr, u16 src_reg_idx, u32 mask);

/**
 * cmdq_pkt_write() - append write command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @value:	the specified target register value
 * @clt_base:	the CMDQ base
 * @addr:	target register address
 * @mask:	the specified target register mask
 *
 * Return: 0 for success; else the error code is returned
 */
s32 cmdq_pkt_write(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	dma_addr_t addr, u32 value, u32 mask);

s32 cmdq_pkt_mem_move(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	dma_addr_t src_addr, dma_addr_t dst_addr, u16 swap_reg_idx);

s32 cmdq_pkt_assign_command(struct cmdq_pkt *pkt, u16 reg_idx, u32 value);

s32 cmdq_pkt_logic_command(struct cmdq_pkt *pkt, enum CMDQ_LOGIC_ENUM s_op,
	u16 result_reg_idx,
	struct cmdq_operand *left_operand,
	struct cmdq_operand *right_operand);

s32 cmdq_pkt_jump(struct cmdq_pkt *pkt, s32 offset);

s32 cmdq_pkt_jump_addr(struct cmdq_pkt *pkt, dma_addr_t addr);

s32 cmdq_pkt_cond_jump_abs(struct cmdq_pkt *pkt,
	u16 addr_reg_idx,
	struct cmdq_operand *left_operand,
	struct cmdq_operand *right_operand,
	enum CMDQ_CONDITION_ENUM condition_operator);

s32 cmdq_pkt_poll_addr(struct cmdq_pkt *pkt, u32 value, u32 addr, u32 mask,
	u8 reg_gpr);

s32 cmdq_pkt_poll_reg(struct cmdq_pkt *pkt, u32 value, u8 subsys,
	u16 offset, u32 mask);

/**
 * cmdq_pkt_poll() - append polling command with mask to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @value:	the specified target register value
 * @subsys:	the CMDQ subsys id
 * @offset:	register offset from module base
 * @mask:	the specified target register mask
 *
 * Return: 0 for success; else the error code is returned
 */
s32 cmdq_pkt_poll(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	u32 value, u32 addr, u32 mask, u8 reg_gpr);

int cmdq_pkt_timer_en(struct cmdq_pkt *pkt);

/* cmdq_pkt_sleep() - append commands to wait a short time in microsecond
 * @pkt:	the CMDQ packet
 * @tick:	sleep time in tick, use CMDQ_MS_TO_TICK to translate into ms
 * @reg_gpr:	GPR use to counting
 *
 * Return 0 for success; else the error code is returned
 */
s32 cmdq_pkt_sleep(struct cmdq_pkt *pkt, u32 tick, u16 reg_gpr);

s32 cmdq_pkt_poll_timeout(struct cmdq_pkt *pkt, u32 value, u8 subsys,
	phys_addr_t addr, u32 mask, u16 count, u16 reg_gpr);

void cmdq_pkt_perf_end(struct cmdq_pkt *pkt);
void cmdq_pkt_perf_begin(struct cmdq_pkt *pkt);
u32 *cmdq_pkt_get_perf_ret(struct cmdq_pkt *pkt);

/**
 * cmdq_pkt_wfe() - append wait for event command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @event:	the desired event type to "wait and CLEAR"
 *
 * Return: 0 for success; else the error code is returned
 */
int cmdq_pkt_wfe(struct cmdq_pkt *pkt, u16 event);

int cmdq_pkt_wait_no_clear(struct cmdq_pkt *pkt, u16 event);

int cmdq_pkt_acquire_event(struct cmdq_pkt *pkt, u16 event);

/**
 * cmdq_pkt_clear_event() - append clear event command to the CMDQ packet
 * @pkt:	the CMDQ packet
 * @event:	the desired event to be cleared
 *
 * Return: 0 for success; else the error code is returned
 */
s32 cmdq_pkt_clear_event(struct cmdq_pkt *pkt, u16 event);

s32 cmdq_pkt_set_event(struct cmdq_pkt *pkt, u16 event);

s32 cmdq_pkt_handshake_event(struct cmdq_pkt *pkt, u16 event);

s32 cmdq_pkt_finalize(struct cmdq_pkt *pkt);

s32 cmdq_pkt_finalize_loop(struct cmdq_pkt *pkt);

/**
 * cmdq_pkt_flush_async() - trigger CMDQ to asynchronously execute the CMDQ
 *                          packet and call back at the end of done packet
 * @client:	the CMDQ mailbox client
 * @pkt:	the CMDQ packet
 * @cb:		called at the end of done packet
 * @data:	this data will pass back to cb
 *
 * Return: 0 for success; else the error code is returned
 *
 * Trigger CMDQ to asynchronously execute the CMDQ packet and call back
 * at the end of done packet. Note that this is an ASYNC function. When the
 * function returned, it may or may not be finished.
 */
s32 cmdq_pkt_flush_async(struct cmdq_pkt *pkt,
	cmdq_async_flush_cb cb, void *data);

int cmdq_pkt_wait_complete(struct cmdq_pkt *pkt);

s32 cmdq_pkt_flush_threaded(struct cmdq_pkt *pkt,
	cmdq_async_flush_cb cb, void *data);

/**
 * cmdq_pkt_flush() - trigger CMDQ to execute the CMDQ packet
 * @pkt:	the CMDQ packet
 *
 * Return: 0 for success; else the error code is returned
 *
 * Trigger CMDQ to execute the CMDQ packet. Note that this is a
 * synchronous flush function. When the function returned, the recorded
 * commands have been done.
 */
s32 cmdq_pkt_flush(struct cmdq_pkt *pkt);

void cmdq_buf_print_wfe(char *text, u32 txt_sz,
	u32 offset, void *inst);

void cmdq_buf_cmd_parse(u64 *buf, u32 cmd_nr, dma_addr_t buf_pa,
	dma_addr_t cur_pa, const char *info, void *chan);

s32 cmdq_pkt_dump_buf(struct cmdq_pkt *pkt, dma_addr_t curr_pa);

int cmdq_dump_pkt(struct cmdq_pkt *pkt, dma_addr_t pc, bool dump_inst);

void cmdq_pkt_set_err_cb(struct cmdq_pkt *pkt,
	cmdq_async_flush_cb cb, void *data);

struct cmdq_thread_task_info {
	dma_addr_t		pa_base;
	struct cmdq_pkt		*pkt;
	struct list_head	list_entry;
};

struct cmdq_timeout_info {
	u32 irq;
	u32 irq_en;
	dma_addr_t curr_pc;
	u32 *curr_pc_va;
	dma_addr_t end_addr;
	u32 task_num;
	struct cmdq_thread_task_info *timeout_task;
	struct list_head task_list;
};
#endif	/* __MTK_CMDQ_H__ */
