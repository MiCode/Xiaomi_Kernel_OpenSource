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

#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/of_address.h>
#include <linux/soc/mediatek/mtk-cmdq.h>


#define CMDQ_ARG_A_WRITE_MASK	0xffff
#define CMDQ_WRITE_ENABLE_MASK	BIT(0)
#define CMDQ_EOC_IRQ_EN		BIT(0)
#define CMDQ_EOC_CMD		((u64)((CMDQ_CODE_EOC << CMDQ_OP_CODE_SHIFT)) \
				<< 32 | CMDQ_EOC_IRQ_EN)

struct cmdq_subsys {
	u32	base;
	int	id;
};

static const struct cmdq_subsys gce_subsys[] = {
	{0x1400, 1},
	{0x1401, 2},
	{0x1402, 3},
};

static int cmdq_subsys_base_to_id(u32 base)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gce_subsys); i++)
		if (gce_subsys[i].base == base)
			return gce_subsys[i].id;
	return -EFAULT;
}

u32 cmdq_subsys_id_to_base(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gce_subsys); i++)
		if (gce_subsys[i].id == id)
			return gce_subsys[i].base;
	return 0;
}
EXPORT_SYMBOL(cmdq_subsys_id_to_base);

#ifdef CMDQ_MEMORY_JUMP
int cmdq_pkt_realloc_cmd_buffer(struct cmdq_pkt *pkt, size_t size)
{
	cmdq_log("not support realloc cmd buffer");
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_realloc_cmd_buffer);
#else
int cmdq_pkt_realloc_cmd_buffer(struct cmdq_pkt *pkt, size_t size)
{
	void *new_buf;

	new_buf = krealloc(pkt->va_base, size, GFP_KERNEL | __GFP_ZERO);
	if (!new_buf)
		return -ENOMEM;
	pkt->va_base = new_buf;
	pkt->buf_size = size;
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_realloc_cmd_buffer);
#endif

struct cmdq_base *cmdq_register_device(struct device *dev)
{
	struct cmdq_base *cmdq_base;
	struct resource res;
	int subsys;
	u32 base;

	if (of_address_to_resource(dev->of_node, 0, &res))
		return NULL;
	base = (u32)res.start;

	subsys = cmdq_subsys_base_to_id(base >> 16);
	if (subsys < 0)
		return NULL;

	cmdq_base = devm_kmalloc(dev, sizeof(*cmdq_base), GFP_KERNEL);
	if (!cmdq_base)
		return NULL;
	cmdq_base->subsys = subsys;
	cmdq_base->base = base;

	return cmdq_base;
}
EXPORT_SYMBOL(cmdq_register_device);

struct cmdq_client *cmdq_mbox_create(struct device *dev, int index)
{
	struct cmdq_client *client;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	client->client.dev = dev;
	client->client.tx_block = false;
	client->chan = mbox_request_channel(&client->client, index);
	return client;
}
EXPORT_SYMBOL(cmdq_mbox_create);

void cmdq_mbox_destroy(struct cmdq_client *client)
{
	mbox_free_channel(client->chan);
	kfree(client);
}
EXPORT_SYMBOL(cmdq_mbox_destroy);

#ifdef CMDQ_MEMORY_JUMP
int cmdq_pkt_create(struct cmdq_pkt **pkt_ptr)
{
	struct cmdq_pkt *pkt;

	pkt = kzalloc(sizeof(*pkt), GFP_KERNEL);
	if (!pkt)
		return -ENOMEM;
	INIT_LIST_HEAD(&pkt->buf);
	*pkt_ptr = pkt;
	return 0;
}

void cmdq_pkt_destroy(struct cmdq_pkt *pkt)
{
	struct cmdq_pkt_buffer *buf, *tmp;

	list_for_each_entry_safe(buf, tmp, &pkt->buf, list_entry) {
		list_del(&buf->list_entry);
		kfree(buf->va_base);
		kfree(buf);
	}
	kfree(pkt);
}
EXPORT_SYMBOL(cmdq_pkt_destroy);
#else
int cmdq_pkt_create(struct cmdq_pkt **pkt_ptr)
{
	struct cmdq_pkt *pkt;
	int err;

	pkt = kzalloc(sizeof(*pkt), GFP_KERNEL);
	if (!pkt)
		return -ENOMEM;
	err = cmdq_pkt_realloc_cmd_buffer(pkt, PAGE_SIZE);
	if (err < 0) {
		kfree(pkt);
		return err;
	}
	*pkt_ptr = pkt;
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_create);

void cmdq_pkt_destroy(struct cmdq_pkt *pkt)
{
	kfree(pkt->va_base);
	kfree(pkt);
}
EXPORT_SYMBOL(cmdq_pkt_destroy);
#endif

#ifdef CMDQ_MEMORY_JUMP
u64 *cmdq_pkt_get_va_by_offset(struct cmdq_pkt *pkt, size_t offset)
{
	size_t offset_remaind = offset;
	struct cmdq_pkt_buffer *buf;

	list_for_each_entry(buf, &pkt->buf, list_entry) {
		if (offset_remaind >= CMDQ_CMD_BUFFER_SIZE) {
			offset_remaind -= CMDQ_CMD_BUFFER_SIZE;
			continue;
		}
		return (u64 *)(buf->va_base + offset_remaind);
	}

	return NULL;
}
EXPORT_SYMBOL(cmdq_pkt_get_va_by_offset);

dma_addr_t cmdq_pkt_get_pa_by_offset(struct cmdq_pkt *pkt, u32 offset)
{
	u32 offset_remaind = offset;
	struct cmdq_pkt_buffer *buf;

	list_for_each_entry(buf, &pkt->buf, list_entry) {
		if (offset_remaind >= CMDQ_CMD_BUFFER_SIZE) {
			offset_remaind -= CMDQ_CMD_BUFFER_SIZE;
			continue;
		}

		return buf->pa_base + offset_remaind;
	}

	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_get_pa_by_offset);

static bool cmdq_pkt_is_finalized(struct cmdq_pkt *pkt)
{
	u64 *expect_eoc;

	if (pkt->cmd_buf_size < CMDQ_INST_SIZE * 2)
		return false;

	expect_eoc = cmdq_pkt_get_va_by_offset(pkt,
		pkt->cmd_buf_size - CMDQ_INST_SIZE * 2);
	if (expect_eoc && *expect_eoc == CMDQ_EOC_CMD)
		return true;

	return false;
}
#else
static bool cmdq_pkt_is_finalized(struct cmdq_pkt *pkt)
{
	u64 *expect_eoc;

	if (pkt->cmd_buf_size < CMDQ_INST_SIZE << 1)
		return false;

	expect_eoc = pkt->va_base + pkt->cmd_buf_size - (CMDQ_INST_SIZE << 1);
	if (*expect_eoc == CMDQ_EOC_CMD)
		return true;

	return false;
}
#endif

#ifdef CMDQ_MEMORY_JUMP
static int cmdq_pkt_append_command(struct cmdq_pkt *pkt, enum cmdq_code code,
	u32 arg_a, u32 arg_b)
{
	u64 *cmd_ptr;
	int err;
	struct cmdq_pkt_buffer *buf;

	if (WARN_ON(cmdq_pkt_is_finalized(pkt)))
		return -EBUSY;
	if (unlikely(pkt->cmd_buf_size + CMDQ_INST_SIZE > pkt->buf_size)) {
		err = cmdq_pkt_realloc_cmd_buffer(pkt, pkt->buf_size << 1);
		if (err < 0)
			return err;
	}
	buf = list_first_entry(&pkt->buf, typeof(*buf), list_entry);
	cmd_ptr = buf->va_base + pkt->cmd_buf_size;
	(*cmd_ptr) = (u64)((code << CMDQ_OP_CODE_SHIFT) | arg_a) << 32 | arg_b;
	pkt->avail_buf_size -= CMDQ_INST_SIZE;
	pkt->cmd_buf_size += CMDQ_INST_SIZE;
	return 0;
}
#else
static int cmdq_pkt_append_command(struct cmdq_pkt *pkt, enum cmdq_code code,
				   u32 arg_a, u32 arg_b)
{
	u64 *cmd_ptr;
	int err;

	if (WARN_ON(cmdq_pkt_is_finalized(pkt)))
		return -EBUSY;
	if (unlikely(pkt->cmd_buf_size + CMDQ_INST_SIZE > pkt->buf_size)) {
		err = cmdq_pkt_realloc_cmd_buffer(pkt, pkt->buf_size << 1);
		if (err < 0)
			return err;
	}
	cmd_ptr = pkt->va_base + pkt->cmd_buf_size;
	(*cmd_ptr) = (u64)((code << CMDQ_OP_CODE_SHIFT) | arg_a) << 32 | arg_b;
	pkt->cmd_buf_size += CMDQ_INST_SIZE;
	return 0;
}
#endif

int cmdq_pkt_read(struct cmdq_pkt *pkt,
			struct cmdq_base *base, u32 offset, u32 writeAddress,
			enum cmdq_gpr_reg valueRegId,
			enum cmdq_gpr_reg destRegId)
{
	int ret;
	/* physical reg address. */
	u32 arg_a = ((base->base + offset) & CMDQ_ARG_A_WRITE_MASK) |
		    (base->subsys << CMDQ_SUBSYS_SHIFT);

	/* Load into 32-bit GPR (R0-R15) */
	ret = cmdq_pkt_append_command(pkt, CMDQ_CODE_READ,
				arg_a | (2 << 21), valueRegId);


	/* CMDQ_CODE_MASK=CMDQ_CODE_MOVE argB is 48-bit */
	/* so writeAddress is split into 2 parts */
	/* and we store address in 64-bit GPR (P0-P7) */
	ret += cmdq_pkt_append_command(pkt, CMDQ_CODE_MASK,
				((destRegId & 0x1f) << 16) | (4 << 21),
				writeAddress);

	/* write to memory */
	ret += cmdq_pkt_append_command(pkt, CMDQ_CODE_WRITE,
				((destRegId & 0x1f) << 16) | (6 << 21),
				valueRegId);

	cmdq_log("COMMAND: copy reg:0x%08x to phys:%pa, GPR(%d, %d)",
		arg_a, &writeAddress, valueRegId, destRegId);
	return ret;
}
EXPORT_SYMBOL(cmdq_pkt_read);

int cmdq_pkt_write(struct cmdq_pkt *pkt, u32 value, struct cmdq_base *base,
		   u32 offset)
{
	u32 arg_a = ((base->base + offset) & CMDQ_ARG_A_WRITE_MASK) |
		    (base->subsys << CMDQ_SUBSYS_SHIFT);
	return cmdq_pkt_append_command(pkt, CMDQ_CODE_WRITE, arg_a, value);
}
EXPORT_SYMBOL(cmdq_pkt_write);

int cmdq_pkt_write_mask(struct cmdq_pkt *pkt, u32 value,
			struct cmdq_base *base, u32 offset, u32 mask)
{
	u32 offset_mask = offset;
	int err;

	if (mask != 0xffffffff) {
		err = cmdq_pkt_append_command(pkt, CMDQ_CODE_MASK, 0, ~mask);
		if (err < 0)
			return err;
		offset_mask |= CMDQ_WRITE_ENABLE_MASK;
	}
	return cmdq_pkt_write(pkt, value, base, offset_mask);
}
EXPORT_SYMBOL(cmdq_pkt_write_mask);

int cmdq_pkt_poll(struct cmdq_pkt *pkt, u32 value, struct cmdq_base *base,
		   u32 offset)
{
	u32 arg_a = ((base->base + offset) & CMDQ_ARG_A_WRITE_MASK) |
		    (base->subsys << CMDQ_SUBSYS_SHIFT);
	return cmdq_pkt_append_command(pkt, CMDQ_CODE_POLL, arg_a, value);
}
EXPORT_SYMBOL(cmdq_pkt_poll);

int cmdq_pkt_poll_mask(struct cmdq_pkt *pkt, u32 value,
			 struct cmdq_base *base, u32 offset, u32 mask)
{
	u32 offset_mask = offset;
	int err;

	if (mask != 0xffffffff) {
		err = cmdq_pkt_append_command(pkt, CMDQ_CODE_MASK, 0, ~mask);
		if (err < 0)
			return err;
		offset_mask |= CMDQ_WRITE_ENABLE_MASK;
	}
	return cmdq_pkt_poll(pkt, value, base, offset_mask);
}
EXPORT_SYMBOL(cmdq_pkt_poll_mask);


const u32 cmdq_event_value_8173[CMDQ_MAX_EVENT] = {
#if 1
};
#else
	/* MDP start of frame(SOF) events */
	[CMDQ_EVENT_MDP_RDMA0_SOF] = 0,
	[CMDQ_EVENT_MDP_RDMA1_SOF] = 1,
	[CMDQ_EVENT_MDP_DSI0_TE_SOF] = 2,	/* DISPSYS TE event */
	[CMDQ_EVENT_MDP_DSI1_TE_SOF] = 3,	/* DISPSYS TE event */
	[CMDQ_EVENT_MDP_MVW_SOF] = 4,
	[CMDQ_EVENT_MDP_TDSHP0_SOF] = 5,
	[CMDQ_EVENT_MDP_TDSHP1_SOF] = 6,
	[CMDQ_EVENT_MDP_WDMA_SOF] = 7,
	[CMDQ_EVENT_MDP_WROT0_SOF] = 8,
	[CMDQ_EVENT_MDP_WROT1_SOF] = 9,
	[CMDQ_EVENT_MDP_CROP_SOF] = 10,
	/* Display start of frame(SOF) events */
	[CMDQ_EVENT_DISP_OVL0_SOF] = 11,
	[CMDQ_EVENT_DISP_OVL1_SOF] = 12,
	[CMDQ_EVENT_DISP_RDMA0_SOF] = 13,
	[CMDQ_EVENT_DISP_RDMA1_SOF] = 14,
	[CMDQ_EVENT_DISP_RDMA2_SOF] = 15,
	[CMDQ_EVENT_DISP_WDMA0_SOF] = 16,
	[CMDQ_EVENT_DISP_WDMA1_SOF] = 17,
	/* MDP end of frame(EOF) events */
	[CMDQ_EVENT_MDP_RDMA0_EOF] = 26,
	[CMDQ_EVENT_MDP_RDMA1_EOF] = 27,
	[CMDQ_EVENT_MDP_RSZ0_EOF] = 28,
	[CMDQ_EVENT_MDP_RSZ1_EOF] = 29,
	[CMDQ_EVENT_MDP_RSZ2_EOF] = 30,
	[CMDQ_EVENT_MDP_TDSHP0_EOF] = 31,
	[CMDQ_EVENT_MDP_TDSHP1_EOF] = 32,
	[CMDQ_EVENT_MDP_WDMA_EOF] = 33,
	[CMDQ_EVENT_MDP_WROT0_W_EOF] = 34,
	[CMDQ_EVENT_MDP_WROT0_R_EOF] = 35,
	[CMDQ_EVENT_MDP_WROT1_W_EOF] = 36,
	[CMDQ_EVENT_MDP_WROT1_R_EOF] = 37,
	[CMDQ_EVENT_MDP_CROP_EOF] = 38,
	/* Display end of frame(EOF) events */
	[CMDQ_EVENT_DISP_OVL0_EOF] = 39,
	[CMDQ_EVENT_DISP_OVL1_EOF] = 40,
	[CMDQ_EVENT_DISP_RDMA0_EOF] = 41,
	[CMDQ_EVENT_DISP_RDMA1_EOF] = 42,
	[CMDQ_EVENT_DISP_RDMA2_EOF] = 43,
	[CMDQ_EVENT_DISP_WDMA0_EOF] = 44,
	[CMDQ_EVENT_DISP_WDMA1_EOF] = 45,
	/* Mutex end of frame(EOF) events */
	[CMDQ_EVENT_MUTEX0_STREAM_EOF] = 53,
	[CMDQ_EVENT_MUTEX1_STREAM_EOF] = 54,
	[CMDQ_EVENT_MUTEX2_STREAM_EOF] = 55,
	[CMDQ_EVENT_MUTEX3_STREAM_EOF] = 56,
	[CMDQ_EVENT_MUTEX4_STREAM_EOF] = 57,
	/* Display underrun events */
	[CMDQ_EVENT_DISP_RDMA0_UNDERRUN] = 63,
	[CMDQ_EVENT_DISP_RDMA1_UNDERRUN] = 64,
	[CMDQ_EVENT_DISP_RDMA2_UNDERRUN] = 65,
};
#endif

const u32 cmdq_event_value_2712[CMDQ_MAX_EVENT] = {
#if 1
};
#else
	[CMDQ_EVENT_MDP_RDMA0_SOF] = 0,
	[CMDQ_EVENT_MDP_RDMA1_SOF] = 1,
	[CMDQ_EVENT_MDP_TDSHP0_SOF] = 5,
	[CMDQ_EVENT_MDP_TDSHP1_SOF] = 6,
	[CMDQ_EVENT_MDP_WDMA_SOF] = 7,
	[CMDQ_EVENT_MDP_WROT0_SOF] = 8,
	[CMDQ_EVENT_MDP_WROT1_SOF] = 9,
	[CMDQ_EVENT_MDP_CROP_SOF] = 10,
	[CMDQ_EVENT_DISP_OVL0_SOF] = 11,
	[CMDQ_EVENT_DISP_OVL1_SOF] = 12,
	[CMDQ_EVENT_DISP_RDMA0_SOF] = 13,
	[CMDQ_EVENT_DISP_RDMA1_SOF] = 14,
	[CMDQ_EVENT_DISP_RDMA2_SOF] = 15,
	[CMDQ_EVENT_DISP_WDMA0_SOF] = 16,
	[CMDQ_EVENT_DISP_WDMA1_SOF] = 17,
	[CMDQ_EVENT_DISP_COLOR0_SOF] = 18,
	[CMDQ_EVENT_DISP_COLOR1_SOF] = 19,
	[CMDQ_EVENT_DISP_AAL0_SOF] = 20,
	[CMDQ_EVENT_DISP_GAMMA_SOF] = 21,
	[CMDQ_EVENT_DISP_UFOE_SOF] = 22,
	[CMDQ_EVENT_DISP_PWM0_SOF] = 23,
	[CMDQ_EVENT_DISP_PWM1_SOF] = 24,
	[CMDQ_EVENT_DISP_OD0_SOF] = 25,
	[CMDQ_EVENT_MDP_RDMA2_SOF] = 26,
	[CMDQ_EVENT_MDP_RDMA3_SOF] = 27,
	[CMDQ_EVENT_MDP_TDSHP2_SOF] = 28,
	[CMDQ_EVENT_MDP_WROT2_SOF] = 29,
	[CMDQ_EVENT_DISP_OVL2_SOF] = 30,
	[CMDQ_EVENT_DISP_WDMA2_SOF] = 31,
	[CMDQ_EVENT_DISP_COLOR2_SOF] = 32,
	[CMDQ_EVENT_DISP_AAL1_SOF] = 33,
	[CMDQ_EVENT_DISP_OD1_SOF] = 34,
	[CMDQ_EVENT_MDP_RDMA0_EOF] = 37,
	[CMDQ_EVENT_MDP_RDMA1_EOF] = 38,
	[CMDQ_EVENT_MDP_RSZ0_EOF] = 39,
	[CMDQ_EVENT_MDP_RSZ1_EOF] = 40,
	[CMDQ_EVENT_MDP_RSZ2_EOF] = 41,
	[CMDQ_EVENT_MDP_TDSHP0_EOF] = 42,
	[CMDQ_EVENT_MDP_TDSHP1_EOF] = 43,
	[CMDQ_EVENT_MDP_WDMA_EOF] = 44,
	[CMDQ_EVENT_MDP_WROT0_W_EOF] = 45,
	[CMDQ_EVENT_MDP_WROT0_R_EOF] = 46,
	[CMDQ_EVENT_MDP_WROT1_W_EOF] = 47,
	[CMDQ_EVENT_MDP_WROT1_R_EOF] = 48,
	[CMDQ_EVENT_MDP_CROP_EOF] = 49,
	[CMDQ_EVENT_DISP_OVL0_EOF] = 50,
	[CMDQ_EVENT_DISP_OVL1_EOF] = 51,
	[CMDQ_EVENT_DISP_RDMA0_EOF] = 52,
	[CMDQ_EVENT_DISP_RDMA1_EOF] = 53,
	[CMDQ_EVENT_DISP_RDMA2_EOF] = 54,
	[CMDQ_EVENT_DISP_WDMA0_EOF] = 55,
	[CMDQ_EVENT_DISP_WDMA1_EOF] = 56,
	[CMDQ_EVENT_DISP_COLOR0_EOF] = 57,
	[CMDQ_EVENT_DISP_COLOR1_EOF] = 58,
	[CMDQ_EVENT_DISP_AAL0_EOF] = 59,
	[CMDQ_EVENT_DISP_GAMMA_EOF] = 60,
	[CMDQ_EVENT_DISP_UFOE_EOF] = 61,
	[CMDQ_EVENT_DISP_DPI0_EOF] = 62,
	[CMDQ_EVENT_DISP_DPI1_EOF] = 63,
	[CMDQ_EVENT_MDP_RDMA2_EOF] = 64,
	[CMDQ_EVENT_MDP_RDMA3_EOF] = 65,
	[CMDQ_EVENT_MDP_WROT2_W_EOF] = 66,
	[CMDQ_EVENT_MDP_WROT2_R_EOF] = 67,
	[CMDQ_EVENT_MDP_TDSHP2_EOF] = 68,
	[CMDQ_EVENT_DISP_OVL2_EOF] = 69,
	[CMDQ_EVENT_DISP_WDMA2_EOF] = 70,
	[CMDQ_EVENT_DISP_COLOR2_EOF] = 71,
	[CMDQ_EVENT_DISP_AAL1_EOF] = 72,
	[CMDQ_EVENT_DISP_OD0_EOF] = 73,
	[CMDQ_EVENT_DISP_OD1_EOF] = 74,
	[CMDQ_EVENT_DISP_DSI0_EOF] = 75,
	[CMDQ_EVENT_DISP_DSI1_EOF] = 76,
	[CMDQ_EVENT_DISP_DSI2_EOF] = 77,
	[CMDQ_EVENT_DISP_DSI3_EOF] = 78,
	[CMDQ_EVENT_MUTEX0_STREAM_EOF] = 79,
	[CMDQ_EVENT_MUTEX1_STREAM_EOF] = 80,
	[CMDQ_EVENT_MUTEX2_STREAM_EOF] = 81,
	[CMDQ_EVENT_MUTEX3_STREAM_EOF] = 82,
	[CMDQ_EVENT_MUTEX4_STREAM_EOF] = 83,
	[CMDQ_EVENT_DISP_RDMA0_UNDERRUN] = 89,
	[CMDQ_EVENT_DISP_RDMA1_UNDERRUN] = 90,
	[CMDQ_EVENT_DISP_RDMA2_UNDERRUN] = 91,
};
#endif

const u32 cmdq_event_value_common[CMDQ_MAX_EVENT] = {};

const u32 *cmdq_event_value;

int cmdq_pkt_wfe(struct cmdq_pkt *pkt, enum cmdq_event event)
{
	u32 arg_b;

	if (event >= CMDQ_MAX_EVENT || event < 0)
		return -EINVAL;

	/*
	 * WFE arg_b
	 * bit 0-11: wait value
	 * bit 15: 1 - wait, 0 - no wait
	 * bit 16-27: update value
	 * bit 31: 1 - update, 0 - no update
	 */
	arg_b = CMDQ_WFE_UPDATE | CMDQ_WFE_WAIT | CMDQ_WFE_WAIT_VALUE;
	return cmdq_pkt_append_command(pkt, CMDQ_CODE_WFE,
			cmdq_event_value[event], arg_b);
}
EXPORT_SYMBOL(cmdq_pkt_wfe);

int cmdq_pkt_clear_event(struct cmdq_pkt *pkt, enum cmdq_event event)
{
	if (event >= CMDQ_MAX_EVENT || event < 0)
		return -EINVAL;

	return cmdq_pkt_append_command(pkt, CMDQ_CODE_WFE,
			cmdq_event_value[event], CMDQ_WFE_UPDATE);
}
EXPORT_SYMBOL(cmdq_pkt_clear_event);

static int cmdq_pkt_finalize(struct cmdq_pkt *pkt)
{
	int err;

	if (cmdq_pkt_is_finalized(pkt))
		return 0;

	/* insert EOC and generate IRQ for each command iteration */
	err = cmdq_pkt_append_command(pkt, CMDQ_CODE_EOC, 0, CMDQ_EOC_IRQ_EN);
	if (err < 0)
		return err;

	/* JUMP to end */
	err = cmdq_pkt_append_command(pkt, CMDQ_CODE_JUMP, 0, CMDQ_JUMP_PASS);
	if (err < 0)
		return err;

	cmdq_log("finalize: add EOC and JUMP cmd");

	return 0;
}

int cmdq_pkt_flush_async(struct cmdq_client *client, struct cmdq_pkt *pkt,
			 cmdq_async_flush_cb cb, void *data)
{
	int err;

	err = cmdq_pkt_finalize(pkt);
	if (err < 0)
		return err;

	pkt->cb.cb = cb;
	pkt->cb.data = data;

	err = mbox_send_message(client->chan, pkt);
	/* We can send next packet immediately, so just call txdone. */
	mbox_client_txdone(client->chan, 0);

	return err;
}
EXPORT_SYMBOL(cmdq_pkt_flush_async);

struct cmdq_flush_completion {
	struct completion cmplt;
	s32 err;
};

static void cmdq_pkt_flush_cb(struct cmdq_cb_data data)
{
	struct cmdq_flush_completion *cmplt = data.data;

	cmdq_log("err=%d", data.err);

	cmplt->err = !data.err ? false : true;
	complete(&cmplt->cmplt);
}

int cmdq_pkt_flush(struct cmdq_client *client, struct cmdq_pkt *pkt)
{
	struct cmdq_flush_completion cmplt;
	int err;

	cmdq_log("start");

	init_completion(&cmplt.cmplt);
	err = cmdq_pkt_flush_async(client, pkt, cmdq_pkt_flush_cb, &cmplt);
	if (err < 0)
		return err;
	wait_for_completion(&cmplt.cmplt);
	cmdq_log("done, err=%u", cmplt.err);
	return cmplt.err ? -EFAULT : 0;
}
EXPORT_SYMBOL(cmdq_pkt_flush);
