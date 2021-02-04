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
#include <linux/mailbox_controller.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>


#define CMDQ_ARG_A_WRITE_MASK	0xffff
#define CMDQ_WRITE_ENABLE_MASK	BIT(0)
#define CMDQ_EOC_IRQ_EN		BIT(0)
#define CMDQ_EOC_CMD		((u64)((CMDQ_CODE_EOC << CMDQ_OP_CODE_SHIFT)) \
				<< 32 | CMDQ_EOC_IRQ_EN)
#define CMDQ_MBOX_BUF_LIMIT	16 /* default limit count */

#define CMDQ_GET_ADDR_H(addr)		(sizeof(addr) > 32 ? (addr >> 32) : 0)
#define CMDQ_GET_ARG_B(arg)		(((arg) & GENMASK(31, 16)) >> 16)
#define CMDQ_GET_ARG_C(arg)		((arg) & GENMASK(15, 0))
#define CMDQ_GET_32B_VALUE(argb, argc)	((u32)((argb) << 16) | (argc))
#define CMDQ_REG_IDX_PREFIX(type)	((type) ? "Reg Index " : "")
#define CMDQ_GET_REG_BASE(addr)		((addr) & GENMASK(31, 16))
#define CMDQ_GET_REG_OFFSET(addr)	((addr) & GENMASK(15, 0))
#define CMDQ_GET_ADDR_HIGH(addr)	((u32)((addr >> 16) & GENMASK(31, 0)))
#define CMDQ_ADDR_LOW_BIT		BIT(1)
#define CMDQ_GET_ADDR_LOW(addr)		((u16)(addr & GENMASK(15, 0)) | \
					CMDQ_ADDR_LOW_BIT)
#define CMDQ_IMMEDIATE_VALUE		0
#define CMDQ_REG_TYPE			1
#define CMDQ_WFE_OPTION			(CMDQ_WFE_UPDATE | CMDQ_WFE_WAIT | \
					CMDQ_WFE_WAIT_VALUE)
#define CMDQ_TPR_TIMEOUT_EN		0xDC

#define CMDQ_OPERAND_GET_IDX_VALUE(operand) \
	((operand)->reg ? (operand)->idx : (operand)->value)
#define CMDQ_OPERAND_TYPE(operand) \
	((operand)->reg ? CMDQ_REG_TYPE : CMDQ_IMMEDIATE_VALUE)
#define CMDQ_DBG_PERFBEGIN		CMDQ_CMD_BUFFER_SIZE
#define CMDQ_DBG_PERFEND		(CMDQ_DBG_PERFBEGIN + 4)

struct client_priv {
	struct dma_pool *buf_pool;
	u32 pool_limit;
	atomic_t buf_cnt;
	struct workqueue_struct *flushq;
};

struct cmdq_instruction {
	u16 arg_c:16;
	u16 arg_b:16;
	u16 arg_a:16;
	u8 s_op:5;
	u8 arg_c_type:1;
	u8 arg_b_type:1;
	u8 arg_a_type:1;
	u8 op:8;
};

static s8 cmdq_subsys_base_to_id(struct cmdq_base *clt_base, u32 base)
{
	u8 i;

	if (!clt_base)
		return -EINVAL;

	base = base & 0xFFFF0000;
	for (i = 0; i < clt_base->count; i++) {
		if (clt_base->subsys[i].base == base)
			return clt_base->subsys[i].id;
	}

	return -EINVAL;
}

u32 cmdq_subsys_id_to_base(struct cmdq_base *clt_base, int id)
{
	u32 i;

	if (!clt_base)
		return 0;

	for (i = 0; i < clt_base->count; i++) {
		if (clt_base->subsys[i].id == id)
			return clt_base->subsys[i].base;
	}

	return 0;
}
EXPORT_SYMBOL(cmdq_subsys_id_to_base);

int cmdq_pkt_realloc_cmd_buffer(struct cmdq_pkt *pkt, size_t size)
{
	while (pkt->buf_size < size)
		cmdq_pkt_add_cmd_buffer(pkt);
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_realloc_cmd_buffer);

struct cmdq_base *cmdq_register_device(struct device *dev)
{
	struct cmdq_base *clt_base;
	struct of_phandle_args spec;
	u32 vals[2] = {0}, idx;
	s32 ret;

	clt_base = devm_kzalloc(dev, sizeof(*clt_base), GFP_KERNEL);
	if (!clt_base)
		return NULL;

	/* parse subsys */
	for (idx = 0; idx < ARRAY_SIZE(clt_base->subsys); idx++) {
		if (of_parse_phandle_with_args(dev->of_node, "gce-subsys",
			"#gce-subsys-cells", idx, &spec))
			break;
		clt_base->subsys[idx].base = spec.args[0];
		clt_base->subsys[idx].id = spec.args[1];
	}
	clt_base->count = idx;

	/* parse CPR range */
	ret = of_property_read_u32_array(dev->of_node, "gce-cpr-range",
		vals, 2);
	if (!ret) {
		clt_base->cpr_base = vals[0] + CMDQ_CPR_STRAT_ID;
		clt_base->cpr_cnt = vals[1];
		cmdq_msg("support cpr:%d count:%d", vals[0], vals[1]);
	}

	return clt_base;
}
EXPORT_SYMBOL(cmdq_register_device);

struct cmdq_client *cmdq_mbox_create(struct device *dev, int index)
{
	struct cmdq_client *client;
	struct client_priv *priv;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return client;

	client->client.dev = dev;
	client->client.tx_block = false;
	client->chan = mbox_request_channel(&client->client, index);
	if (IS_ERR(client->chan)) {
		kfree(client);
		return NULL;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		cmdq_mbox_destroy(client);
		return ERR_PTR(-ENOMEM);
	}

	priv->pool_limit = CMDQ_MBOX_BUF_LIMIT;
	priv->flushq = create_singlethread_workqueue("cmdq_flushq");
	client->cl_priv = (void *)priv;
	mutex_init(&client->chan_mutex);

	return client;
}
EXPORT_SYMBOL(cmdq_mbox_create);

void cmdq_mbox_stop(struct cmdq_client *cl)
{
	cmdq_mbox_channel_stop(cl->chan);
}
EXPORT_SYMBOL(cmdq_mbox_stop);

void cmdq_mbox_pool_set_limit(struct cmdq_client *cl, u32 limit)
{
	struct client_priv *priv = (struct client_priv *)cl->cl_priv;

	priv->pool_limit = limit;
}
EXPORT_SYMBOL(cmdq_mbox_pool_set_limit);

void cmdq_mbox_pool_create(struct cmdq_client *cl)
{
	struct client_priv *priv = (struct client_priv *)cl->cl_priv;

	if (unlikely(priv->buf_pool)) {
		cmdq_msg("buffer pool already created");
		return;
	}

	priv->buf_pool = dma_pool_create("cmdq", cl->chan->mbox->dev,
		CMDQ_BUF_ALLOC_SIZE, 0, 0);
}
EXPORT_SYMBOL(cmdq_mbox_pool_create);

void cmdq_mbox_pool_clear(struct cmdq_client *cl)
{
	struct client_priv *priv = (struct client_priv *)cl->cl_priv;

	/* check pool still in use */
	if (unlikely((atomic_read(&priv->buf_cnt)))) {
		cmdq_msg("buffers still in use:%d",
			atomic_read(&priv->buf_cnt));
		return;
	}

	dma_pool_destroy(priv->buf_pool);
	priv->buf_pool = NULL;
}
EXPORT_SYMBOL(cmdq_mbox_pool_clear);

static void *cmdq_mbox_pool_alloc_impl(struct dma_pool *pool,
	dma_addr_t *pa_out, atomic_t *cnt, u32 limit)
{
	void *va;
	dma_addr_t pa;

	if (atomic_inc_return(cnt) > limit) {
		/* not use pool, decrease to value before call */
		atomic_dec(cnt);
		return NULL;
	}

	va = dma_pool_alloc(pool, GFP_KERNEL, &pa);
	if (!va) {
		atomic_dec(cnt);
		cmdq_err(
			"alloc buffer from pool fail va:0x%p pa:%pa pool:0x%p count:%d",
			va, &pa, pool,
			(s32)atomic_read(cnt));
		return NULL;
	}

	*pa_out = pa;

	return va;
}

static void cmdq_mbox_pool_free_impl(struct dma_pool *pool, void *va,
	dma_addr_t pa, atomic_t *cnt)
{
	if (unlikely(atomic_read(cnt) <= 0 || !pool)) {
		cmdq_err("free pool cnt:%d pool:0x%p",
			(s32)atomic_read(cnt), pool);
		return;
	}

	dma_pool_free(pool, va, pa);
	atomic_dec(cnt);
}

static void *cmdq_mbox_pool_alloc(struct cmdq_client *cl, dma_addr_t *pa_out)
{
	struct client_priv *priv = (struct client_priv *)cl->cl_priv;

	if (unlikely(!priv->buf_pool)) {
		cmdq_mbox_pool_create(cl);
		if (unlikely(!priv->buf_pool)) {
			cmdq_err("fail to create dma pool dev:0x%p",
				cl->chan->mbox->dev);
			return NULL;
		}
	}

	return cmdq_mbox_pool_alloc_impl(priv->buf_pool,
		pa_out, &priv->buf_cnt, priv->pool_limit);
}

static void cmdq_mbox_pool_free(struct cmdq_client *cl, void *va, dma_addr_t pa)
{
	struct client_priv *priv = (struct client_priv *)cl->cl_priv;

	cmdq_mbox_pool_free_impl(priv->buf_pool, va, pa, &priv->buf_cnt);
}

void *cmdq_mbox_buf_alloc(struct device *dev, dma_addr_t *pa_out)
{
	void *va;
	dma_addr_t pa;

	va = dma_alloc_coherent(dev, CMDQ_BUF_ALLOC_SIZE, &pa, GFP_KERNEL);
	if (!va) {
		cmdq_err("alloc dma buffer fail dev:0x%p", dev);
		return NULL;
	}

	*pa_out = pa;
	return va;
}

void cmdq_mbox_buf_free(struct device *dev, void *va, dma_addr_t pa)
{
	dma_free_coherent(dev, CMDQ_BUF_ALLOC_SIZE, va, pa);
}

/* parse event from dts
 *
 * Example
 *
 * dts:
 * gce-event-names = "disp_rdma0_sof",
 *	"disp_rdma1_sof",
 *	"mdp_rdma0_sof";
 * gce-events = <&gce_mbox CMDQ_EVENT_DISP_RDMA0_SOF>,
 *	<&gce_mbox CMDQ_EVENT_DISP_RDMA1_SOF>,
 *	<&gce_mbox CMDQ_EVENT_MDP_RDMA0_SOF>;
 *
 * call:
 * s32 rdma0_sof_event_id = cmdq_dev_get_event(dev, "disp_rdma0_sof");
 */
s32 cmdq_dev_get_event(struct device *dev, const char *name)
{
	s32 index = 0;
	struct of_phandle_args spec = {0};
	s32 result;

	if (!dev) {
		cmdq_err("no device node");
		return -EINVAL;
	}

	index = of_property_match_string(dev->of_node, "gce-event-names", name);
	if (index < 0) {
		cmdq_err("no gce-event-names property or no such event:%s",
			name);
		return index;
	}

	if (of_parse_phandle_with_args(dev->of_node, "gce-events",
		"#gce-event-cells", index, &spec)) {
		cmdq_err("can't parse gce-events property");
		return -ENODEV;
	}

	result = spec.args[0];
	of_node_put(spec.np);

	return result;
}

struct cmdq_pkt_buffer *cmdq_pkt_alloc_buf(struct cmdq_pkt *pkt)
{
	struct cmdq_client *cl = (struct cmdq_client *)pkt->priv;
	struct cmdq_pkt_buffer *buf;
	struct cmdq_buf_pool *pool = pkt->buf_pool;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	/* try dma pool if available */
	if (pkt->buf_pool && pool->pool)
		buf->va_base = cmdq_mbox_pool_alloc_impl(pool->pool,
			&buf->pa_base, &pool->cnt, pool->limit);
	else if (cl)
		buf->va_base = cmdq_mbox_pool_alloc(cl, &buf->pa_base);

	if (buf->va_base)
		buf->use_pool = true;
	else	/* allocate directly */
		buf->va_base = cmdq_mbox_buf_alloc(pkt->dev, &buf->pa_base);

	if (!buf->va_base) {
		cmdq_err("allocate cmd buffer failed");
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	list_add_tail(&buf->list_entry, &pkt->buf);
	pkt->avail_buf_size += CMDQ_CMD_BUFFER_SIZE;
	pkt->buf_size += CMDQ_CMD_BUFFER_SIZE;

	return buf;
}

void cmdq_pkt_free_buf(struct cmdq_pkt *pkt)
{
	struct cmdq_client *cl = (struct cmdq_client *)pkt->priv;
	struct cmdq_pkt_buffer *buf, *tmp;
	struct cmdq_buf_pool *pool = pkt->buf_pool;

	list_for_each_entry_safe(buf, tmp, &pkt->buf, list_entry) {
		list_del(&buf->list_entry);
		if (buf->use_pool) {
			if (buf->use_pool && pool)
				cmdq_mbox_pool_free_impl(pool->pool,
					buf->va_base, buf->pa_base, &pool->cnt);
			else
				cmdq_mbox_pool_free(cl, buf->va_base,
					buf->pa_base);
		}
		else
			cmdq_mbox_buf_free(pkt->dev, buf->va_base,
				buf->pa_base);
		kfree(buf);
	}
}

s32 cmdq_pkt_add_cmd_buffer(struct cmdq_pkt *pkt)
{
	s32 status = 0;
	struct cmdq_pkt_buffer *buf, *prev;
	u64 *prev_va;

	if (list_empty(&pkt->buf))
		prev = NULL;
	else
		prev = list_last_entry(&pkt->buf, typeof(*prev), list_entry);

	buf = cmdq_pkt_alloc_buf(pkt);
	if (unlikely(IS_ERR(buf))) {
		status = PTR_ERR(buf);
		cmdq_err("alloc singe buffer fail status:%d pkt:0x%p",
			status, pkt);
		return status;
	}

	/* if no previous buffer, success return */
	if (!prev)
		return 0;

	/* copy last instruction to head of new buffer and
	 * use jump to replace
	 */
	prev_va = (u64 *)(prev->va_base + CMDQ_CMD_BUFFER_SIZE -
		CMDQ_INST_SIZE);
	*((u64 *)buf->va_base) = *prev_va;

	/* insert jump to jump start of new buffer.
	 * jump to absolute addr
	 */
	*prev_va = ((u64)(CMDQ_CODE_JUMP << 24 | 1) << 32) |
		(CMDQ_REG_SHIFT_ADDR(buf->pa_base) & 0xFFFFFFFF);

	/* decrease available size since insert 1 jump */
	pkt->avail_buf_size -= CMDQ_INST_SIZE;
	/* +1 for jump instruction */
	pkt->cmd_buf_size += CMDQ_INST_SIZE;

	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_add_cmd_buffer);

void cmdq_mbox_destroy(struct cmdq_client *client)
{
	mbox_free_channel(client->chan);
	kfree(client->cl_priv);
	kfree(client);
}
EXPORT_SYMBOL(cmdq_mbox_destroy);

s32 cmdq_pkt_create(struct cmdq_pkt **pkt_ptr)
{
	struct cmdq_pkt *pkt;

	pkt = kzalloc(sizeof(*pkt), GFP_KERNEL);
	if (!pkt)
		return -ENOMEM;
	INIT_LIST_HEAD(&pkt->buf);
	*pkt_ptr = pkt;
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_create);

void cmdq_pkt_set_client(struct cmdq_pkt *pkt, struct cmdq_client *cl)
{
	pkt->priv = (void *)cl;
	pkt->dev = cl->chan->mbox->dev;
}
EXPORT_SYMBOL(cmdq_pkt_set_client);

s32 cmdq_pkt_cl_create(struct cmdq_pkt **pkt_ptr, struct cmdq_client *cl)
{
	struct cmdq_pkt *pkt;

	pkt = kzalloc(sizeof(*pkt), GFP_KERNEL);
	if (!pkt) {
		*pkt_ptr = NULL;
		return -ENOMEM;
	}
	INIT_LIST_HEAD(&pkt->buf);
	cmdq_pkt_set_client(pkt, cl);
	*pkt_ptr = pkt;
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_cl_create);

void cmdq_pkt_destroy(struct cmdq_pkt *pkt)
{
	cmdq_pkt_free_buf(pkt);
	kfree(pkt);
}
EXPORT_SYMBOL(cmdq_pkt_destroy);

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

static dma_addr_t cmdq_pkt_get_curr_buf_pa(struct cmdq_pkt *pkt)
{
	struct cmdq_pkt_buffer *buf;

	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);

	return buf->pa_base + CMDQ_CMD_BUFFER_SIZE - pkt->avail_buf_size;
}

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

static void cmdq_pkt_instr_encoder(void *buf, u16 arg_c, u16 arg_b,
	u16 arg_a, u8 s_op, u8 arg_c_type, u8 arg_b_type, u8 arg_a_type, u8 op)
{
	struct cmdq_instruction *cmdq_inst;

	cmdq_inst = buf;
	cmdq_inst->op = op;
	cmdq_inst->arg_a_type = arg_a_type;
	cmdq_inst->arg_b_type = arg_b_type;
	cmdq_inst->arg_c_type = arg_c_type;
	cmdq_inst->s_op = s_op;
	cmdq_inst->arg_a = arg_a;
	cmdq_inst->arg_b = arg_b;
	cmdq_inst->arg_c = arg_c;
}

s32 cmdq_pkt_append_command(struct cmdq_pkt *pkt, u16 arg_c, u16 arg_b,
	u16 arg_a, u8 s_op, u8 arg_c_type, u8 arg_b_type, u8 arg_a_type,
	enum cmdq_code code)
{
	struct cmdq_pkt_buffer *buf;
	void *va;

	if (!pkt)
		return -EINVAL;

	if (unlikely(!pkt->avail_buf_size)) {
		if (cmdq_pkt_add_cmd_buffer(pkt) < 0)
			return -ENOMEM;
	}

	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	va = buf->va_base + CMDQ_CMD_BUFFER_SIZE - pkt->avail_buf_size;

	cmdq_pkt_instr_encoder(va, arg_c, arg_b, arg_a, s_op, arg_c_type,
		arg_b_type, arg_a_type, code);
	pkt->cmd_buf_size += CMDQ_INST_SIZE;
	pkt->avail_buf_size -= CMDQ_INST_SIZE;

	return 0;
}

s32 cmdq_pkt_read(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	dma_addr_t src_addr, u16 dst_reg_idx)
{
	s8 subsys;

	if (!(CMDQ_GET_ADDR_H(src_addr))) {
		subsys = cmdq_subsys_base_to_id(clt_base, src_addr);
		if (subsys >= 0)
			return cmdq_pkt_read_reg(pkt,
				clt_base->subsys[subsys].id,
				CMDQ_GET_REG_OFFSET(src_addr), dst_reg_idx);
	}

	return cmdq_pkt_read_addr(pkt, src_addr, dst_reg_idx);
}
EXPORT_SYMBOL(cmdq_pkt_read);

s32 cmdq_pkt_read_reg(struct cmdq_pkt *pkt, u8 subsys, u16 offset,
	u16 dst_reg_idx)
{
	return cmdq_pkt_append_command(pkt, 0, offset, dst_reg_idx, subsys,
		CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE,
		CMDQ_CODE_READ_S);
}
EXPORT_SYMBOL(cmdq_pkt_read_reg);

s32 cmdq_pkt_read_addr(struct cmdq_pkt *pkt, dma_addr_t addr, u16 dst_reg_idx)
{
	s32 err;
	const u16 src_reg_idx = CMDQ_SPR_FOR_TEMP;

	err = cmdq_pkt_assign_command(pkt, src_reg_idx,
		CMDQ_GET_ADDR_HIGH(addr));
	if (err != 0)
		return err;

	return cmdq_pkt_append_command(pkt, 0, CMDQ_GET_ADDR_LOW(addr),
		dst_reg_idx, src_reg_idx,
		CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE,
		CMDQ_CODE_READ_S);
}
EXPORT_SYMBOL(cmdq_pkt_read_addr);

s32 cmdq_pkt_write_reg(struct cmdq_pkt *pkt, u8 subsys,
	u16 offset, u16 src_reg_idx, u32 mask)
{
	int err = 0;
	enum cmdq_code op = CMDQ_CODE_WRITE_S;

	if (mask != 0xffffffff) {
		err = cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(~mask),
			CMDQ_GET_ARG_B(~mask), 0, 0, 0, 0, 0, CMDQ_CODE_MASK);
		if (err != 0)
			return err;

		op = CMDQ_CODE_WRITE_S_W_MASK;
	}

	return cmdq_pkt_append_command(pkt, 0, src_reg_idx, offset, subsys,
		CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE, CMDQ_IMMEDIATE_VALUE, op);
}
EXPORT_SYMBOL(cmdq_pkt_write_reg);

s32 cmdq_pkt_write_value(struct cmdq_pkt *pkt, u8 subsys,
	u16 offset, u32 value, u32 mask)
{
	int err = 0;
	enum cmdq_code op = CMDQ_CODE_WRITE_S;

	if (mask != 0xffffffff) {
		err = cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(~mask),
			CMDQ_GET_ARG_B(~mask), 0, 0, 0, 0, 0, CMDQ_CODE_MASK);
		if (err != 0)
			return err;

		op = CMDQ_CODE_WRITE_S_W_MASK;
	}

	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(value),
		CMDQ_GET_ARG_B(value), offset, subsys,
		CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE,
		CMDQ_IMMEDIATE_VALUE, op);
}
EXPORT_SYMBOL(cmdq_pkt_write_value);

s32 cmdq_pkt_write_reg_addr(struct cmdq_pkt *pkt, dma_addr_t addr,
	u16 src_reg_idx, u32 mask)
{
	s32 err;
	const u16 dst_reg_idx = CMDQ_SPR_FOR_TEMP;

	err = cmdq_pkt_assign_command(pkt, dst_reg_idx,
		CMDQ_GET_ADDR_HIGH(addr));
	if (err != 0)
		return err;

	return cmdq_pkt_store_value_reg(pkt, dst_reg_idx,
		CMDQ_GET_ADDR_LOW(addr), src_reg_idx, mask);
}
EXPORT_SYMBOL(cmdq_pkt_write_reg_addr);

s32 cmdq_pkt_write_value_addr(struct cmdq_pkt *pkt, dma_addr_t addr,
	u32 value, u32 mask)
{
	s32 err;
	const u16 dst_reg_idx = CMDQ_SPR_FOR_TEMP;

	/* assign bit 47:16 to spr temp */
	err = cmdq_pkt_assign_command(pkt, dst_reg_idx,
		CMDQ_GET_ADDR_HIGH(addr));
	if (err != 0)
		return err;

	return cmdq_pkt_store_value(pkt, dst_reg_idx, CMDQ_GET_ADDR_LOW(addr),
		value, mask);
}
EXPORT_SYMBOL(cmdq_pkt_write_value_addr);

s32 cmdq_pkt_store_value(struct cmdq_pkt *pkt, u16 indirect_dst_reg_idx,
	u16 dst_addr_low, u32 value, u32 mask)
{
	int err = 0;
	enum cmdq_code op = CMDQ_CODE_WRITE_S;

	if (mask != 0xffffffff) {
		err = cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(~mask),
			CMDQ_GET_ARG_B(~mask), 0, 0, 0, 0, 0, CMDQ_CODE_MASK);
		if (err != 0)
			return err;

		op = CMDQ_CODE_WRITE_S_W_MASK;
	}

	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(value),
		CMDQ_GET_ARG_B(value), dst_addr_low,
		indirect_dst_reg_idx, CMDQ_IMMEDIATE_VALUE,
		CMDQ_IMMEDIATE_VALUE, CMDQ_IMMEDIATE_VALUE, op);
}
EXPORT_SYMBOL(cmdq_pkt_store_value);

s32 cmdq_pkt_store_value_reg(struct cmdq_pkt *pkt, u16 indirect_dst_reg_idx,
	u16 dst_addr_low, u16 indirect_src_reg_idx, u32 mask)
{
	int err = 0;
	enum cmdq_code op = CMDQ_CODE_WRITE_S;

	if (mask != 0xffffffff) {
		err = cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(~mask),
			CMDQ_GET_ARG_B(~mask), 0, 0, 0, 0, 0, CMDQ_CODE_MASK);
		if (err != 0)
			return err;

		op = CMDQ_CODE_WRITE_S_W_MASK;
	}

	if (dst_addr_low) {
		return cmdq_pkt_append_command(pkt, 0, indirect_src_reg_idx,
			dst_addr_low, indirect_dst_reg_idx,
			CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE,
			CMDQ_IMMEDIATE_VALUE, op);
	}

	return cmdq_pkt_append_command(pkt, 0,
		indirect_src_reg_idx, indirect_dst_reg_idx, 0,
		CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE, CMDQ_REG_TYPE, op);
}
EXPORT_SYMBOL(cmdq_pkt_store_value_reg);

s32 cmdq_pkt_write_indriect(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	dma_addr_t addr, u16 src_reg_idx, u32 mask)
{
	const u32 base = CMDQ_GET_ADDR_H(addr) ? 0 : addr & 0xFFFF0000;
	s32 subsys;

	subsys = cmdq_subsys_base_to_id(clt_base, base);
	if (subsys >= 0) {
		return cmdq_pkt_write_reg(pkt, subsys,
			base, src_reg_idx, mask);
	}

	return cmdq_pkt_write_reg_addr(pkt, addr, src_reg_idx, mask);
}
EXPORT_SYMBOL(cmdq_pkt_write_indriect);

s32 cmdq_pkt_write(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	dma_addr_t addr, u32 value, u32 mask)
{
	const u32 base = CMDQ_GET_ADDR_H(addr) ? 0 : addr & 0xFFFF0000;
	s32 subsys;

	subsys = cmdq_subsys_base_to_id(clt_base, base);
	if (subsys >= 0) {
		return cmdq_pkt_write_value(pkt, subsys,
			CMDQ_GET_REG_OFFSET(addr), value, mask);
	}

	return cmdq_pkt_write_value_addr(pkt, addr, value, mask);
}
EXPORT_SYMBOL(cmdq_pkt_write);

s32 cmdq_pkt_mem_move(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	dma_addr_t src_addr, dma_addr_t dst_addr, u16 swap_reg_idx)
{
	s32 err;

	err = cmdq_pkt_read(pkt, clt_base, src_addr, swap_reg_idx);
	if (err != 0)
		return err;

	return cmdq_pkt_write_indriect(pkt, clt_base, dst_addr,
		swap_reg_idx, ~0);
}
EXPORT_SYMBOL(cmdq_pkt_mem_move);

s32 cmdq_pkt_assign_command(struct cmdq_pkt *pkt, u16 reg_idx, u32 value)
{
	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(value),
		CMDQ_GET_ARG_B(value), reg_idx,
		CMDQ_LOGIC_ASSIGN, CMDQ_IMMEDIATE_VALUE,
		CMDQ_IMMEDIATE_VALUE, CMDQ_REG_TYPE,
		CMDQ_CODE_LOGIC);
}
EXPORT_SYMBOL(cmdq_pkt_assign_command);

s32 cmdq_pkt_logic_command(struct cmdq_pkt *pkt, enum CMDQ_LOGIC_ENUM s_op,
	u16 result_reg_idx,
	struct cmdq_operand *left_operand,
	struct cmdq_operand *right_operand)
{
	u32 left_idx_value;
	u32 right_idx_value;

	if (!left_operand || !right_operand)
		return -EINVAL;

	left_idx_value = CMDQ_OPERAND_GET_IDX_VALUE(left_operand);
	right_idx_value = CMDQ_OPERAND_GET_IDX_VALUE(right_operand);

	return cmdq_pkt_append_command(pkt, right_idx_value, left_idx_value,
		result_reg_idx, s_op, CMDQ_OPERAND_TYPE(right_operand),
		CMDQ_OPERAND_TYPE(left_operand), CMDQ_REG_TYPE,
		CMDQ_CODE_LOGIC);
}
EXPORT_SYMBOL(cmdq_pkt_logic_command);

s32 cmdq_pkt_jump(struct cmdq_pkt *pkt, s32 offset)
{
	s64 off = CMDQ_REG_SHIFT_ADDR((s64)offset);

	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(off),
		CMDQ_GET_ARG_B(off), 0, 0, 0, 0, 0, CMDQ_CODE_JUMP);
}
EXPORT_SYMBOL(cmdq_pkt_jump);

s32 cmdq_pkt_jump_addr(struct cmdq_pkt *pkt, u32 addr)
{
	dma_addr_t to_addr = CMDQ_REG_SHIFT_ADDR(addr);

	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(to_addr),
		CMDQ_GET_ARG_B(to_addr), 1, 0, 0, 0, 0, CMDQ_CODE_JUMP);
}
EXPORT_SYMBOL(cmdq_pkt_jump_addr);

s32 cmdq_pkt_cond_jump(struct cmdq_pkt *pkt,
	u16 offset_reg_idx,
	struct cmdq_operand *left_operand,
	struct cmdq_operand *right_operand,
	enum CMDQ_CONDITION_ENUM condition_operator)
{
	u32 left_idx_value;
	u32 right_idx_value;

	if (!left_operand || !right_operand)
		return -EINVAL;

	left_idx_value = CMDQ_OPERAND_GET_IDX_VALUE(left_operand);
	right_idx_value = CMDQ_OPERAND_GET_IDX_VALUE(right_operand);

	return cmdq_pkt_append_command(pkt, right_idx_value, left_idx_value,
		offset_reg_idx, condition_operator,
		CMDQ_OPERAND_TYPE(right_operand),
		CMDQ_OPERAND_TYPE(left_operand),
		CMDQ_REG_TYPE, CMDQ_CODE_JUMP_C_RELATIVE);
}
EXPORT_SYMBOL(cmdq_pkt_cond_jump);

s32 cmdq_pkt_cond_jump_abs(struct cmdq_pkt *pkt,
	u16 addr_reg_idx,
	struct cmdq_operand *left_operand,
	struct cmdq_operand *right_operand,
	enum CMDQ_CONDITION_ENUM condition_operator)
{
	u16 left_idx_value;
	u16 right_idx_value;

	if (!left_operand || !right_operand)
		return -EINVAL;

	left_idx_value = CMDQ_OPERAND_GET_IDX_VALUE(left_operand);
	right_idx_value = CMDQ_OPERAND_GET_IDX_VALUE(right_operand);

	return cmdq_pkt_append_command(pkt, right_idx_value, left_idx_value,
		addr_reg_idx, condition_operator,
		CMDQ_OPERAND_TYPE(right_operand),
		CMDQ_OPERAND_TYPE(left_operand),
		CMDQ_REG_TYPE, CMDQ_CODE_JUMP_C_ABSOLUTE);
}
EXPORT_SYMBOL(cmdq_pkt_cond_jump_abs);

s32 cmdq_pkt_poll_addr(struct cmdq_pkt *pkt, u32 value, u32 addr, u32 mask,
	u8 reg_gpr)
{
	s32 err;

	if (mask != 0xffffffff) {
		err = cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(~mask),
			CMDQ_GET_ARG_B(~mask), 0, 0, 0, 0, 0, CMDQ_CODE_MASK);
		if (err != 0)
			return err;

		addr = addr | 0x1;
	}

	/* Move extra handle APB address to GPR */
	err = cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(addr),
		CMDQ_GET_ARG_B(addr), 0, reg_gpr,
		0, 0, 1, CMDQ_CODE_MOVE);
	if (err != 0)
		cmdq_err("%s fail append command move addr to reg err:%d",
			__func__, err);

	err = cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(value),
		CMDQ_GET_ARG_B(value), 0, reg_gpr,
		0, 0, 1, CMDQ_CODE_POLL);
	if (err != 0)
		cmdq_err("%s fail append command poll err:%d",
			__func__, err);

	return err;
}
EXPORT_SYMBOL(cmdq_pkt_poll_addr);

s32 cmdq_pkt_poll_reg(struct cmdq_pkt *pkt, u32 value, u8 subsys,
	u16 offset, u32 mask)
{
	s32 err;

	if (mask != 0xffffffff) {
		err = cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(~mask),
			CMDQ_GET_ARG_B(~mask), 0, 0, 0, 0, 0, CMDQ_CODE_MASK);
		if (err != 0)
			return err;

		offset = offset | 0x1;
	}

	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(value),
		CMDQ_GET_ARG_B(value), offset, subsys, 0, 0, 0, CMDQ_CODE_POLL);
}
EXPORT_SYMBOL(cmdq_pkt_poll_reg);

s32 cmdq_pkt_poll(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	u32 value, u32 addr, u32 mask, u8 reg_gpr)
{
	const u32 base = addr & 0xFFFF0000;
	s8 subsys;

	subsys = cmdq_subsys_base_to_id(clt_base, base);
	if (subsys >= 0)
		return cmdq_pkt_poll_reg(pkt, value, subsys,
			CMDQ_GET_REG_OFFSET(addr), mask);

	return cmdq_pkt_poll_addr(pkt, value, addr, mask, reg_gpr);
}
EXPORT_SYMBOL(cmdq_pkt_poll);

s32 cmdq_pkt_sleep(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	u16 tick, u16 reg_gpr)
{
	const u32 tpr_en = 1 << reg_gpr;
	const u16 event = (u16)GCE_TOKEN_GPR_TIMER + reg_gpr;
	struct cmdq_client *cl = (struct cmdq_client *)pkt->priv;
	struct cmdq_thread *thread = (struct cmdq_thread *)cl->chan->con_priv;
	struct cmdq_operand lop = {.reg = true, .idx = CMDQ_TPR_ID};
	struct cmdq_operand rop = {.reg = false, .value = 1};
	const u32 timeout_en = thread->gce_pa + CMDQ_TPR_TIMEOUT_EN;

	/* set target gpr value to max to avoid event trigger
	 * before new value write to gpr
	 */
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_SUBTRACT,
		CMDQ_GPR_CNT_ID + reg_gpr, &lop, &rop);
	cmdq_pkt_write(pkt, clt_base, timeout_en, tpr_en, tpr_en);
	cmdq_pkt_clear_event(pkt, event);
	rop.value = tick;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, CMDQ_GPR_CNT_ID + reg_gpr,
		&lop, &rop);
	cmdq_pkt_wfe(pkt, event);

	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_sleep);

s32 cmdq_pkt_poll_timeout(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	u32 value, u32 addr, u32 mask, u16 count, u16 reg_gpr)
{
	const u16 reg_tmp = CMDQ_SPR_FOR_TEMP;
	const u16 reg_val = CMDQ_THR_SPR_IDX1;
	const u16 reg_poll = CMDQ_THR_SPR_IDX2;
	const u16 reg_counter = CMDQ_THR_SPR_IDX3;
	u32 begin_mark, end_addr_mark, shift_pa;
	dma_addr_t cmd_pa;
	struct cmdq_operand lop, rop;
	struct cmdq_instruction *inst;

	/* assign compare value as compare target later */
	cmdq_pkt_assign_command(pkt, reg_val, value);

	/* init loop counter as 0 */
	cmdq_pkt_assign_command(pkt, reg_counter, 0);

	/* mark begin offset of this operation */
	begin_mark = pkt->cmd_buf_size;

	/* read target address */
	cmdq_pkt_read(pkt, clt_base, addr, reg_poll);

	/* mask it */
	if (mask != ~0) {
		lop.reg = true;
		lop.idx = reg_poll;
		rop.reg = true;
		rop.idx = reg_tmp;

		cmdq_pkt_assign_command(pkt, reg_tmp, mask);
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, reg_poll,
			&lop, &rop);
	}

	/* assign temp spr as empty, shoudl fill in end addr later */
	end_addr_mark = pkt->cmd_buf_size;
	cmdq_pkt_assign_command(pkt, reg_tmp, 0);

	/* compare and jump to end if equal
	 * note that end address will fill in later into last instruction
	 */
	lop.reg = true;
	lop.idx = reg_poll;
	rop.reg = true;
	rop.idx = reg_val;
	cmdq_pkt_cond_jump_abs(pkt, reg_tmp, &lop, &rop, CMDQ_EQUAL);

	/* check if timeup and inc counter */
	lop.reg = true;
	lop.idx = reg_counter;
	rop.reg = false;
	rop.value = count;
	cmdq_pkt_cond_jump_abs(pkt, reg_tmp, &lop, &rop,
		CMDQ_GREATER_THAN_AND_EQUAL);
	lop.reg = true;
	lop.idx = reg_counter;
	rop.reg = false;
	rop.value = 1;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, reg_counter, &lop, &rop);

	/* sleep for 5 tick, which around 192us */
	cmdq_pkt_sleep(pkt, clt_base, 5, reg_gpr);

	/* loop to begin */
	cmd_pa = cmdq_pkt_get_pa_by_offset(pkt, begin_mark);
	cmdq_pkt_jump_addr(pkt, cmd_pa);

	/* read current buffer pa as end mark and fill preview assign */
	cmd_pa = cmdq_pkt_get_curr_buf_pa(pkt);
	inst = (struct cmdq_instruction *)cmdq_pkt_get_va_by_offset(
		pkt, end_addr_mark);
	/* instruction may hit boundary case,
	 * check if op code is jump and get next instruction if necessary
	 */
	if (inst->op == CMDQ_CODE_JUMP)
		inst = (struct cmdq_instruction *)cmdq_pkt_get_va_by_offset(
			pkt, end_addr_mark + CMDQ_INST_SIZE);
	if (!inst)
		return -EINVAL;

	shift_pa = CMDQ_REG_SHIFT_ADDR(cmd_pa);
	inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	inst->arg_c = CMDQ_GET_ARG_C(shift_pa);

	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_poll_timeout);

void cmdq_pkt_perf_begin(struct cmdq_pkt *pkt)
{
	dma_addr_t pa;
	struct cmdq_pkt_buffer *buf;

	if (!pkt->buf_size)
		cmdq_pkt_add_cmd_buffer(pkt);

	pa = cmdq_pkt_get_pa_by_offset(pkt, 0) + CMDQ_DBG_PERFBEGIN;
	cmdq_pkt_write_indriect(pkt, NULL, pa, CMDQ_TPR_ID, ~0);

	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	*(u32 *)(buf->va_base + CMDQ_DBG_PERFBEGIN) = 0xdeaddead;
}
EXPORT_SYMBOL(cmdq_pkt_perf_begin);

void cmdq_pkt_perf_end(struct cmdq_pkt *pkt)
{
	dma_addr_t pa;
	struct cmdq_pkt_buffer *buf;

	if (!pkt->buf_size)
		cmdq_pkt_add_cmd_buffer(pkt);

	pa = cmdq_pkt_get_pa_by_offset(pkt, 0) + CMDQ_DBG_PERFEND;
	cmdq_pkt_write_indriect(pkt, NULL, pa, CMDQ_TPR_ID, ~0);

	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	*(u32 *)(buf->va_base + CMDQ_DBG_PERFEND) = 0xdeaddead;
}
EXPORT_SYMBOL(cmdq_pkt_perf_end);

u32 *cmdq_pkt_get_perf_ret(struct cmdq_pkt *pkt)
{
	struct cmdq_pkt_buffer *buf = list_first_entry(&pkt->buf, typeof(*buf),
		list_entry);

	return (u32 *)(buf->va_base + CMDQ_DBG_PERFBEGIN);
}
EXPORT_SYMBOL(cmdq_pkt_get_perf_ret);

int cmdq_pkt_wfe(struct cmdq_pkt *pkt, u16 event)
{
	u32 arg_b;

	if (event >= CMDQ_EVENT_MAX)
		return -EINVAL;

	/*
	 * WFE arg_b
	 * bit 0-11: wait value
	 * bit 15: 1 - wait, 0 - no wait
	 * bit 16-27: update value
	 * bit 31: 1 - update, 0 - no update
	 */
	arg_b = CMDQ_WFE_UPDATE | CMDQ_WFE_WAIT | CMDQ_WFE_WAIT_VALUE;
	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(arg_b),
		CMDQ_GET_ARG_B(arg_b), event,
		0, 0, 0, 0, CMDQ_CODE_WFE);
}
EXPORT_SYMBOL(cmdq_pkt_wfe);

int cmdq_pkt_wait_no_clear(struct cmdq_pkt *pkt, u16 event)
{
	u32 arg_b;

	if (event >= CMDQ_EVENT_MAX)
		return -EINVAL;

	/*
	 * WFE arg_b
	 * bit 0-11: wait value
	 * bit 15: 1 - wait, 0 - no wait
	 * bit 16-27: update value
	 * bit 31: 1 - update, 0 - no update
	 */
	arg_b = CMDQ_WFE_WAIT | CMDQ_WFE_WAIT_VALUE;
	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(arg_b),
		CMDQ_GET_ARG_B(arg_b), event,
		0, 0, 0, 0, CMDQ_CODE_WFE);
}
EXPORT_SYMBOL(cmdq_pkt_wait_no_clear);

s32 cmdq_pkt_clear_event(struct cmdq_pkt *pkt, u16 event)
{
	if (event >= CMDQ_EVENT_MAX)
		return -EINVAL;

	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(CMDQ_WFE_UPDATE),
		CMDQ_GET_ARG_B(CMDQ_WFE_UPDATE), event,
		0, 0, 0, 0, CMDQ_CODE_WFE);
}
EXPORT_SYMBOL(cmdq_pkt_clear_event);

s32 cmdq_pkt_set_event(struct cmdq_pkt *pkt, u16 event)
{
	u32 arg_b;

	if (event >= CMDQ_EVENT_MAX)
		return -EINVAL;

	arg_b = CMDQ_WFE_UPDATE | CMDQ_WFE_UPDATE_VALUE;
	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(arg_b),
		CMDQ_GET_ARG_B(arg_b), event,
		0, 0, 0, 0, CMDQ_CODE_WFE);
}
EXPORT_SYMBOL(cmdq_pkt_set_event);

s32 cmdq_pkt_finalize(struct cmdq_pkt *pkt)
{
	int err;

	if (cmdq_pkt_is_finalized(pkt))
		return 0;

	/* insert EOC and generate IRQ for each command iteration */
	err = cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(CMDQ_EOC_IRQ_EN),
		CMDQ_GET_ARG_B(CMDQ_EOC_IRQ_EN), 0, 0, 0, 0, 0, CMDQ_CODE_EOC);
	if (err < 0)
		return err;

	/* JUMP to end */
	err = cmdq_pkt_jump(pkt, CMDQ_JUMP_PASS);
	if (err < 0)
		return err;

	cmdq_log("finalize: add EOC and JUMP cmd");

	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_finalize);

s32 cmdq_pkt_finalize_loop(struct cmdq_pkt *pkt)
{
	u32 start_pa;
	s32 err;

	if (cmdq_pkt_is_finalized(pkt))
		return 0;

	/* insert EOC and generate IRQ for each command iteration */
	err = cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(CMDQ_EOC_IRQ_EN),
		CMDQ_GET_ARG_B(CMDQ_EOC_IRQ_EN), 0, 0, 0, 0, 0, CMDQ_CODE_EOC);
	if (err < 0)
		return err;

	/* JUMP to start of pkt */
	start_pa = cmdq_pkt_get_pa_by_offset(pkt, 0);
	err = cmdq_pkt_jump_addr(pkt, start_pa);
	if (err < 0)
		return err;

	/* mark pkt as loop */
	pkt->loop = true;

	cmdq_log("finalize: add EOC and JUMP begin cmd");

	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_finalize_loop);

s32 cmdq_pkt_flush_async(struct cmdq_client *client, struct cmdq_pkt *pkt,
	cmdq_async_flush_cb cb, void *data)
{
	s32 err;

	err = cmdq_pkt_finalize(pkt);
	if (err < 0)
		return err;

	pkt->cb.cb = cb;
	pkt->cb.data = data;

	mutex_lock(&client->chan_mutex);
	err = mbox_send_message(client->chan, pkt);
	/* We can send next packet immediately, so just call txdone. */
	mbox_client_txdone(client->chan, 0);
	mutex_unlock(&client->chan_mutex);

	return err;
}
EXPORT_SYMBOL(cmdq_pkt_flush_async);

struct cmdq_flush_item {
	struct work_struct work;
	struct cmdq_client *cl;
	cmdq_async_flush_cb cb;
	void *data;
	s32 err;
};

static void cmdq_pkt_flush_cb_work(struct work_struct *w)
{
	struct cmdq_flush_item *item = container_of(w,
		struct cmdq_flush_item, work);
	struct cmdq_cb_data data;

	data.data = item->data;
	data.err = item->err;
	item->cb(data);
	kfree(item);
}

static void cmdq_pkt_flush_queue_cb(struct cmdq_cb_data data)
{
	struct cmdq_flush_item *item = (struct cmdq_flush_item *)data.data;
	struct client_priv *priv = item->cl->cl_priv;

	item->err = data.err;
	queue_work(priv->flushq, &item->work);
}

s32 cmdq_pkt_flush_threaded(struct cmdq_client *client, struct cmdq_pkt *pkt,
	cmdq_async_flush_cb cb, void *data)
{
	struct cmdq_flush_item *item = kzalloc(sizeof(*item), GFP_KERNEL);
	s32 err;

	if (!item)
		return -ENOMEM;

	item->cl = client;
	item->cb = cb;
	item->data = data;
	INIT_WORK(&item->work, cmdq_pkt_flush_cb_work);
	err = cmdq_pkt_flush_async(client, pkt, cmdq_pkt_flush_queue_cb, item);
	if (err < 0)
		kfree(item);

	return err;
}
EXPORT_SYMBOL(cmdq_pkt_flush_threaded);

static void cmdq_pkt_flush_cb(struct cmdq_cb_data data)
{
	struct cmdq_flush_completion *cmplt = data.data;

	cmplt->err = !data.err ? false : true;
	complete(&cmplt->cmplt);
}

int cmdq_pkt_flush(struct cmdq_client *client, struct cmdq_pkt *pkt)
{
	struct cmdq_flush_completion cmplt;
	int err;

	cmdq_log("start");

	init_completion(&cmplt.cmplt);
	cmplt.pkt = pkt;
	err = cmdq_pkt_flush_async(client, pkt, cmdq_pkt_flush_cb, &cmplt);
	if (err < 0)
		return err;
	wait_for_completion(&cmplt.cmplt);
	cmdq_log("done pkt:0x%p err:%d", cmplt.pkt, cmplt.err);
	return cmplt.err ? -EFAULT : 0;
}
EXPORT_SYMBOL(cmdq_pkt_flush);

static void cmdq_buf_print_read(u32 offset,
	struct cmdq_instruction *cmdq_inst)
{
	u32 addr;

	if (cmdq_inst->arg_b_type == CMDQ_IMMEDIATE_VALUE &&
		(cmdq_inst->arg_b & CMDQ_ADDR_LOW_BIT)) {
		/* 48bit format case */
		addr = cmdq_inst->arg_b & 0xfffc;

		cmdq_msg(
			"0x%04x 0x%016llx [Read | Load] Reg Index 0x%08x = addr(low) 0x%04x",
			offset, *((u64 *)cmdq_inst), cmdq_inst->arg_a, addr);
	} else {
		addr = ((u32)(cmdq_inst->arg_b |
			(cmdq_inst->s_op << CMDQ_SUBSYS_SHIFT)));

		cmdq_msg(
			"0x%04x 0x%016llx [Read | Load] Reg Index 0x%08x = %s0x%08x",
			offset, *((u64 *)cmdq_inst), cmdq_inst->arg_a,
			cmdq_inst->arg_b_type ? "*Reg Index " : "SubSys Reg ",
			addr);
	}
}

static void cmdq_buf_print_write(u32 offset,
	struct cmdq_instruction *cmdq_inst)
{
	u32 addr;

	if (cmdq_inst->arg_a_type == CMDQ_IMMEDIATE_VALUE &&
		(cmdq_inst->arg_a & CMDQ_ADDR_LOW_BIT)) {
		/* 48bit format case */
		addr = cmdq_inst->arg_a & 0xfffc;

		cmdq_msg(
			"0x%04x 0x%016llx [Write | Store] addr(low) 0x%04x = %s0x%08x%s",
			offset, *((u64 *)cmdq_inst),
			addr, CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
			cmdq_inst->arg_b_type ? cmdq_inst->arg_b :
			CMDQ_GET_32B_VALUE(cmdq_inst->arg_b, cmdq_inst->arg_c),
			cmdq_inst->op == CMDQ_CODE_WRITE_S_W_MASK ?
			" with mask" : "");
	} else {
		addr = ((u32)(cmdq_inst->arg_a |
			(cmdq_inst->s_op << CMDQ_SUBSYS_SHIFT)));

		cmdq_msg(
			"0x%04x 0x%016llx [Write | Store] %s0x%08x = %s0x%08x%s",
			offset, *((u64 *)cmdq_inst),
			cmdq_inst->arg_a_type ? "*Reg Index " : "SubSys Reg ",
			addr, CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
			cmdq_inst->arg_b_type ? cmdq_inst->arg_b :
			CMDQ_GET_32B_VALUE(cmdq_inst->arg_b, cmdq_inst->arg_c),
			cmdq_inst->op == CMDQ_CODE_WRITE_S_W_MASK ?
			" with mask" : "");
	}
}

static void cmdq_buf_print_wfe(u32 offset,
	struct cmdq_instruction *cmdq_inst)
{
	u32 cmd = CMDQ_GET_32B_VALUE(cmdq_inst->arg_b, cmdq_inst->arg_c);
	u32 event_op = cmd & 0x80008000;
	u16 update_to = cmdq_inst->arg_b & GENMASK(11, 0);
	u16 wait_to = cmdq_inst->arg_c & GENMASK(11, 0);

	switch (event_op) {
	case 0x80000000:
		cmdq_msg("0x%04x 0x%016llx [Sync ] %s event %u to %u",
			offset, *((u64 *)cmdq_inst),
			update_to ? "set" : "clear",
			cmdq_inst->arg_a,
			update_to);
		break;
	case 0x8000:
		cmdq_msg("0x%04x 0x%016llx [Sync ] wait for event %u become %u",
			offset, *((u64 *)cmdq_inst),
			cmdq_inst->arg_a,
			wait_to);
		break;
	case 0x80008000:
	default:
		cmdq_msg(
			"0x%04x 0x%016llx [Sync ] wait for event %u become %u and %s to %u",
			offset, *((u64 *)cmdq_inst),
			cmdq_inst->arg_a,
			wait_to,
			update_to ? "set" : "clear",
			update_to);
		break;
	}
}

static const char *cmdq_parse_logic_sop(enum CMDQ_LOGIC_ENUM s_op)
{
	switch (s_op) {
	case CMDQ_LOGIC_ASSIGN:
		return "= ";
	case CMDQ_LOGIC_ADD:
		return "+ ";
	case CMDQ_LOGIC_SUBTRACT:
		return "- ";
	case CMDQ_LOGIC_MULTIPLY:
		return "* ";
	case CMDQ_LOGIC_XOR:
		return "^";
	case CMDQ_LOGIC_NOT:
		return "= ~";
	case CMDQ_LOGIC_OR:
		return "| ";
	case CMDQ_LOGIC_AND:
		return "& ";
	case CMDQ_LOGIC_LEFT_SHIFT:
		return "<< ";
	case CMDQ_LOGIC_RIGHT_SHIFT:
		return ">> ";
	default:
		return "<error: unsupported logic sop>";
	}
}

static const char *cmdq_parse_jump_c_sop(enum CMDQ_CONDITION_ENUM s_op)
{
	switch (s_op) {
	case CMDQ_EQUAL:
		return "==";
	case CMDQ_NOT_EQUAL:
		return "!=";
	case CMDQ_GREATER_THAN_AND_EQUAL:
		return ">=";
	case CMDQ_LESS_THAN_AND_EQUAL:
		return "<=";
	case CMDQ_GREATER_THAN:
		return ">";
	case CMDQ_LESS_THAN:
		return "<";
	default:
		return "<error: unsupported jump conditional sop>";
	}
}

static void cmdq_buf_print_move(u32 offset,
				struct cmdq_instruction *cmdq_inst)
{
	u32 val = CMDQ_GET_32B_VALUE(cmdq_inst->arg_b, cmdq_inst->arg_c);
	u32 addr = (u32)(cmdq_inst->s_op << CMDQ_SUBSYS_SHIFT) |
		cmdq_inst->arg_a;

	if (cmdq_inst->arg_a_type)
		cmdq_msg("0x%04x 0x%016llx [Move ] move 0x%08x to %s0x%x",
			offset, *((u64 *)cmdq_inst), val,
			cmdq_inst->arg_a_type ? "*Reg Index " : "SubSys Reg ",
			addr);
	else
		cmdq_msg("0x%04x 0x%016llx [Move ] mask 0x%08x",
			offset, *((u64 *)cmdq_inst), ~val);
}

static void cmdq_buf_print_logic(u32 offset,
	struct cmdq_instruction *cmdq_inst)
{
	switch (cmdq_inst->s_op) {
	case CMDQ_LOGIC_ASSIGN:
		cmdq_msg(
			"0x%04x 0x%016llx [Logic] Reg Index 0x%04x %s%s0x%08x",
			offset, *((u64 *)cmdq_inst), cmdq_inst->arg_a,
			cmdq_parse_logic_sop(cmdq_inst->s_op),
			CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
			CMDQ_GET_32B_VALUE(cmdq_inst->arg_b, cmdq_inst->arg_c));
		break;
	case CMDQ_LOGIC_NOT:
		cmdq_msg(
			"0x%04x 0x%016llx [Logic] Reg Index 0x%04x %s%s0x%08x",
			offset, *((u64 *)cmdq_inst), cmdq_inst->arg_a,
			cmdq_parse_logic_sop(cmdq_inst->s_op),
			CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
			cmdq_inst->arg_b);
		break;
	default:
		cmdq_msg(
			"0x%04x 0x%016llx [Logic] %s0x%08x = %s0x%08x %s%s0x%08x",
			offset, *((u64 *)cmdq_inst),
			CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_a_type),
			cmdq_inst->arg_a,
			CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
			cmdq_inst->arg_b, cmdq_parse_logic_sop(cmdq_inst->s_op),
			CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_c_type),
			cmdq_inst->arg_c);
		break;
	}
}

static void cmdq_buf_print_write_jump_c(u32 offset,
	struct cmdq_instruction *cmdq_inst)
{
	cmdq_msg(
		"0x%04x 0x%016llx [Jumpc] %s if (%s0x%08x %s %s0x%08x) jump %s0x%08x",
		offset, *((u64 *)cmdq_inst),
		cmdq_inst->op == CMDQ_CODE_JUMP_C_ABSOLUTE ?
		"absolute" : "relative",
		CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
		cmdq_inst->arg_b, cmdq_parse_jump_c_sop(cmdq_inst->s_op),
		CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_c_type), cmdq_inst->arg_c,
		CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_a_type), cmdq_inst->arg_a);
}

static void cmdq_buf_print_poll(u32 offset,
	struct cmdq_instruction *cmdq_inst)
{
	u32 addr = ((u32)(cmdq_inst->arg_a |
		(cmdq_inst->s_op << CMDQ_SUBSYS_SHIFT)));

	cmdq_msg("0x%04x 0x%016llx [Poll ] poll %s0x%08x = %s0x%08x",
		offset, *((u64 *)cmdq_inst),
		cmdq_inst->arg_a_type ? "*Reg Index " : "SubSys Reg ",
		addr,
		CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
		CMDQ_GET_32B_VALUE(cmdq_inst->arg_b, cmdq_inst->arg_c));
}

static void cmdq_buf_print_jump(u32 offset,
	struct cmdq_instruction *cmdq_inst)
{
	u32 dst = ((u32)cmdq_inst->arg_b) << 16 | cmdq_inst->arg_c;

	cmdq_msg("0x%04x 0x%016llx [Jump ] jump %s 0x%llx",
		offset, *((u64 *)cmdq_inst),
		cmdq_inst->arg_a ? "absolute addr" : "relative offset",
		cmdq_inst->arg_a ? CMDQ_REG_REVERT_ADDR((u64)dst) :
		CMDQ_REG_REVERT_ADDR((s64)(s32)dst));
}

static void cmdq_buf_print_misc(u32 offset,
	struct cmdq_instruction *cmdq_inst)
{
	char *cmd_str;

	switch (cmdq_inst->op) {
	case CMDQ_CODE_EOC:
		cmd_str = "eoc";
		break;
	default:
		cmd_str = "unknown";
		break;
	}

	cmdq_msg("0x%04x 0x%016llx %s",
		offset, *((u64 *)cmdq_inst), cmd_str);
}

static void cmdq_buf_cmd_parse(u64 *buf, u32 cmd_nr, u32 pa_offset)
{
	struct cmdq_instruction *cmdq_inst = (struct cmdq_instruction *)buf;
	u32 i, offset = 0;

	for (i = 0; i < cmd_nr; i++) {
		if (offset == pa_offset)
			cmdq_err("==========ERROR==========");
		switch (cmdq_inst[i].op) {
		case CMDQ_CODE_WRITE_S:
		case CMDQ_CODE_WRITE_S_W_MASK:
			cmdq_buf_print_write(offset, &cmdq_inst[i]);
			break;
		case CMDQ_CODE_WFE:
			cmdq_buf_print_wfe(offset, &cmdq_inst[i]);
			break;
		case CMDQ_CODE_MOVE:
			cmdq_buf_print_move(offset, &cmdq_inst[i]);
			break;
		case CMDQ_CODE_READ_S:
			cmdq_buf_print_read(offset, &cmdq_inst[i]);
			break;
		case CMDQ_CODE_LOGIC:
			cmdq_buf_print_logic(offset, &cmdq_inst[i]);
			break;
		case CMDQ_CODE_JUMP_C_ABSOLUTE:
		case CMDQ_CODE_JUMP_C_RELATIVE:
			cmdq_buf_print_write_jump_c(offset, &cmdq_inst[i]);
			break;
		case CMDQ_CODE_POLL:
			cmdq_buf_print_poll(offset, &cmdq_inst[i]);
			break;
		case CMDQ_CODE_JUMP:
			cmdq_buf_print_jump(offset, &cmdq_inst[i]);
			break;
		default:
			cmdq_buf_print_misc(offset, &cmdq_inst[i]);
			break;
		}
		if (offset == pa_offset)
			cmdq_err("==========ERROR==========");
		offset += CMDQ_INST_SIZE;
	}
}

s32 cmdq_pkt_dump_buf(struct cmdq_pkt *pkt, u32 curr_pa)
{
	struct cmdq_pkt_buffer *buf;
	u32 pa_offset, size, cnt = 0;

	list_for_each_entry(buf, &pkt->buf, list_entry) {
		if (list_is_last(&buf->list_entry, &pkt->buf)) {
			size = CMDQ_CMD_BUFFER_SIZE - pkt->avail_buf_size;
		} else if (cnt > 2) {
			cmdq_msg(
				"buffer %u va:0x%p pa:%pa 0x%016llx (skip detail) 0x%016llx",
				cnt, buf->va_base, &buf->pa_base,
				*((u64 *)buf->va_base),
				*((u64 *)(buf->va_base +
				CMDQ_CMD_BUFFER_SIZE - CMDQ_INST_SIZE)));
			cnt++;
			continue;
		} else {
			size = CMDQ_CMD_BUFFER_SIZE;
		}
		cmdq_msg("pkt:0x%p buffer %u va:0x%p pa:%pa",
			pkt, cnt, buf->va_base, &buf->pa_base);
		if (curr_pa && curr_pa >= buf->pa_base && curr_pa <
			buf->pa_base + CMDQ_CMD_BUFFER_SIZE)
			pa_offset = curr_pa - buf->pa_base;
		else
			pa_offset = ~0;
		cmdq_buf_cmd_parse(buf->va_base, CMDQ_NUM_CMD(size),
			pa_offset);
		cnt++;
	}

	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_dump_buf);

int cmdq_dump_pkt(struct cmdq_pkt *pkt)
{
	if (!pkt)
		return -EINVAL;

	cmdq_msg(
		"dump cmdq pkt:0x%p size:%zu avail size:%zu priority:%u loop:%s dev:0x%p",
		pkt, pkt->buf_size, pkt->avail_buf_size, pkt->priority,
		pkt->loop ? "true" : "false", pkt->dev);
	cmdq_pkt_dump_buf(pkt, 0);
	cmdq_msg("dump cmdq pkt:0x%p end", pkt);

	return 0;
}
EXPORT_SYMBOL(cmdq_dump_pkt);

