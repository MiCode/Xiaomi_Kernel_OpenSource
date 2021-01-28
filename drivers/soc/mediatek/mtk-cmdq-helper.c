// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/completion.h>
#include <linux/errno.h>
#include <linux/of_address.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <linux/mailbox_controller.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/sched/clock.h>

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
#include "cmdq-util.h"

#if IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	IS_ENABLED(CONFIG_MTK_CAM_SECURITY_SUPPORT)
#include "cmdq-sec.h"
#endif

#endif

#define CMDQ_ARG_A_WRITE_MASK	0xffff
#define CMDQ_WRITE_ENABLE_MASK	BIT(0)
#define CMDQ_EOC_IRQ_EN		BIT(0)
#define CMDQ_EOC_CMD		((u64)((CMDQ_CODE_EOC << CMDQ_OP_CODE_SHIFT)) \
				<< 32 | CMDQ_EOC_IRQ_EN)
#define CMDQ_MBOX_BUF_LIMIT	16 /* default limit count */

#define CMDQ_PREDUMP_TIMEOUT_MS		200

/* sleep for 312 tick, which around 12us */
#define CMDQ_POLL_TICK			312

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
#define CMDQ_TPR_MASK			0xD0
#define CMDQ_TPR_TIMEOUT_EN		0xDC
#define CMDQ_GPR_R0_OFF			0x80

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

struct cmdq_flush_item {
	struct work_struct work;
	struct cmdq_pkt *pkt;
	cmdq_async_flush_cb cb;
	void *data;
	cmdq_async_flush_cb err_cb;
	void *err_data;
	s32 err;
	bool done;
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
		return ERR_PTR(-ENOMEM);

	client->client.dev = dev;
	client->client.tx_block = false;
	client->chan = mbox_request_channel(&client->client, index);
	if (IS_ERR(client->chan)) {
		cmdq_err("channel request fail:%d, idx:%d",
			PTR_ERR(client->chan), index);
		dump_stack();
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
	dma_addr_t pa = 0;

	va = dma_alloc_coherent(dev, CMDQ_BUF_ALLOC_SIZE, &pa, GFP_KERNEL);
	if (!va) {
		cmdq_err("alloc dma buffer fail dev:0x%p", dev);
		dump_stack();
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
	struct cmdq_client *cl = (struct cmdq_client *)pkt->cl;
	struct cmdq_pkt_buffer *buf;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	/* try dma pool if available */
	if (pkt->cur_pool.pool)
		buf->va_base = cmdq_mbox_pool_alloc_impl(pkt->cur_pool.pool,
			&buf->pa_base, pkt->cur_pool.cnt, *pkt->cur_pool.limit);
	else if (cl) {
		struct client_priv *priv = (struct client_priv *)cl->cl_priv;

		buf->va_base = cmdq_mbox_pool_alloc(cl, &buf->pa_base);
		if (buf->va_base) {
			pkt->cur_pool.pool = priv->buf_pool;
			pkt->cur_pool.cnt = &priv->buf_cnt;
			pkt->cur_pool.limit = &priv->pool_limit;
		}
	}

	if (buf->va_base)
		buf->use_pool = true;
	else	/* allocate directly */
		buf->va_base = cmdq_mbox_buf_alloc(pkt->dev,
			&buf->pa_base);

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
	struct cmdq_client *cl = (struct cmdq_client *)pkt->cl;
	struct cmdq_pkt_buffer *buf, *tmp;

	list_for_each_entry_safe(buf, tmp, &pkt->buf, list_entry) {
		list_del(&buf->list_entry);
		if (buf->use_pool) {
			if (pkt->cur_pool.pool)
				cmdq_mbox_pool_free_impl(pkt->cur_pool.pool,
					buf->va_base, buf->pa_base,
					pkt->cur_pool.cnt);
			else {
				cmdq_err("free pool:%s dev:%#lx pa:%pa cl:%#lx",
					buf->use_pool ? "true" : "false",
					(unsigned long)pkt->dev,
					&buf->pa_base,
					cl);
				cmdq_mbox_pool_free(cl, buf->va_base,
					buf->pa_base);
			}
		} else
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

struct cmdq_pkt *cmdq_pkt_create(struct cmdq_client *client)
{
	struct cmdq_pkt *pkt;

	pkt = kzalloc(sizeof(*pkt), GFP_KERNEL);
	if (!pkt)
		return ERR_PTR(-ENOMEM);
	INIT_LIST_HEAD(&pkt->buf);
	init_completion(&pkt->cmplt);
	pkt->cl = (void *)client;
	if (client)
		pkt->dev = client->chan->mbox->dev;

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	if (client && cmdq_util_is_feature_en(CMDQ_LOG_FEAT_PERF))
		cmdq_pkt_perf_begin(pkt);
#endif

	return pkt;
}
EXPORT_SYMBOL(cmdq_pkt_create);

void cmdq_pkt_destroy(struct cmdq_pkt *pkt)
{
	cmdq_pkt_free_buf(pkt);
	kfree(pkt->flush_item);
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

int cmdq_pkt_timer_en(struct cmdq_pkt *pkt)
{
	struct cmdq_client *cl = pkt->cl;
	const u32 en = 0x8000000;
	phys_addr_t gce_pa;

	if (!cl)
		return -EINVAL;

	gce_pa = cmdq_mbox_get_base_pa(cl->chan);

	return cmdq_pkt_write(pkt, NULL, gce_pa + CMDQ_TPR_MASK, en, en);
}
EXPORT_SYMBOL(cmdq_pkt_timer_en);

s32 cmdq_pkt_sleep(struct cmdq_pkt *pkt, u32 tick, u16 reg_gpr)
{
	const u32 tpr_en = 1 << reg_gpr;
	const u16 event = (u16)CMDQ_EVENT_GPR_TIMER + reg_gpr;
	struct cmdq_client *cl = (struct cmdq_client *)pkt->cl;
	struct cmdq_operand lop, rop;
	const u32 timeout_en = cmdq_mbox_get_base_pa(cl->chan) +
		CMDQ_TPR_TIMEOUT_EN;

	/* set target gpr value to max to avoid event trigger
	 * before new value write to gpr
	 */
	lop.reg = true;
	lop.idx = CMDQ_TPR_ID;
	rop.reg = false;
	rop.value = 1;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_SUBTRACT,
		CMDQ_GPR_CNT_ID + reg_gpr, &lop, &rop);

	lop.reg = true;
	lop.idx = CMDQ_CPR_TPR_MASK;
	rop.reg = false;
	rop.value = tpr_en;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_OR, CMDQ_CPR_TPR_MASK,
		&lop, &rop);
	cmdq_pkt_write_indriect(pkt, NULL, timeout_en, CMDQ_CPR_TPR_MASK, ~0);
	cmdq_pkt_read(pkt, NULL, timeout_en, CMDQ_SPR_FOR_TEMP);
	cmdq_pkt_clear_event(pkt, event);

	if (tick < U16_MAX) {
		lop.reg = true;
		lop.idx = CMDQ_TPR_ID;
		rop.reg = false;
		rop.value = tick;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD,
			CMDQ_GPR_CNT_ID + reg_gpr, &lop, &rop);
	} else {
		cmdq_pkt_assign_command(pkt, CMDQ_SPR_FOR_TEMP, tick);
		lop.reg = true;
		lop.idx = CMDQ_TPR_ID;
		rop.reg = true;
		rop.value = CMDQ_SPR_FOR_TEMP;
		cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD,
			CMDQ_GPR_CNT_ID + reg_gpr, &lop, &rop);
	}
	cmdq_pkt_wfe(pkt, event);

	lop.reg = true;
	lop.idx = CMDQ_CPR_TPR_MASK;
	rop.reg = false;
	rop.value = ~tpr_en;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_AND, CMDQ_CPR_TPR_MASK,
		&lop, &rop);

	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_sleep);

s32 cmdq_pkt_poll_timeout(struct cmdq_pkt *pkt, u32 value, u8 subsys,
	phys_addr_t addr, u32 mask, u16 count, u16 reg_gpr)
{
	const u16 reg_tmp = CMDQ_SPR_FOR_TEMP;
	const u16 reg_val = CMDQ_THR_SPR_IDX1;
	const u16 reg_poll = CMDQ_THR_SPR_IDX2;
	const u16 reg_counter = CMDQ_THR_SPR_IDX3;
	u32 begin_mark, end_addr_mark, cnt_end_addr_mark = 0, shift_pa;
	dma_addr_t cmd_pa;
	struct cmdq_operand lop, rop;
	struct cmdq_instruction *inst;
	bool absolute = true;

	if (pkt->avail_buf_size > PAGE_SIZE)
		absolute = false;

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
#if IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	IS_ENABLED(CONFIG_MTK_CAM_SECURITY_SUPPORT)
	if (pkt->sec_data)
		absolute = false;
#endif
#endif

	/* assign compare value as compare target later */
	cmdq_pkt_assign_command(pkt, reg_val, value);

	/* init loop counter as 0, counter can be count poll limit or debug */
	cmdq_pkt_assign_command(pkt, reg_counter, 0);

	/* mark begin offset of this operation */
	begin_mark = pkt->cmd_buf_size;

	/* read target address */
	if (subsys != SUBSYS_NO_SUPPORT)
		cmdq_pkt_read_reg(pkt, subsys, CMDQ_GET_REG_OFFSET(addr),
			reg_poll);
	else
		cmdq_pkt_read_addr(pkt, addr, reg_poll);

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
	if (absolute)
		cmdq_pkt_cond_jump_abs(pkt, reg_tmp, &lop, &rop, CMDQ_EQUAL);
	else
		cmdq_pkt_cond_jump(pkt, reg_tmp, &lop, &rop, CMDQ_EQUAL);

	/* check if timeup and inc counter */
	if (count != U16_MAX) {
		if (!absolute) {
			cnt_end_addr_mark = pkt->cmd_buf_size;
			cmdq_pkt_assign_command(pkt, reg_tmp, 0);
		}
		lop.reg = true;
		lop.idx = reg_counter;
		rop.reg = false;
		rop.value = count;
		if (absolute)
			cmdq_pkt_cond_jump_abs(pkt, reg_tmp, &lop, &rop,
				CMDQ_GREATER_THAN_AND_EQUAL);
		else
			cmdq_pkt_cond_jump(pkt, reg_tmp, &lop, &rop,
				CMDQ_GREATER_THAN_AND_EQUAL);
	}

	/* always inc counter */
	lop.reg = true;
	lop.idx = reg_counter;
	rop.reg = false;
	rop.value = 1;
	cmdq_pkt_logic_command(pkt, CMDQ_LOGIC_ADD, reg_counter, &lop,
		&rop);

	cmdq_pkt_sleep(pkt, CMDQ_POLL_TICK, reg_gpr);

	/* loop to begin */
	if (absolute) {
		cmd_pa = cmdq_pkt_get_pa_by_offset(pkt, begin_mark);
		cmdq_pkt_jump_addr(pkt, cmd_pa);
	} else {
		/* jump relative back to begin mark */
		cmdq_pkt_jump(pkt, -(s32)(pkt->cmd_buf_size - begin_mark));
	}

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
	if (absolute)
		shift_pa = CMDQ_REG_SHIFT_ADDR(cmd_pa);
	else
		shift_pa = CMDQ_REG_SHIFT_ADDR(
			pkt->cmd_buf_size - end_addr_mark - CMDQ_INST_SIZE);
	inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
	inst->arg_c = CMDQ_GET_ARG_C(shift_pa);

	/* relative case the counter have different offset */
	if (cnt_end_addr_mark) {
		inst = (struct cmdq_instruction *)cmdq_pkt_get_va_by_offset(
			pkt, cnt_end_addr_mark);
		if (inst->op == CMDQ_CODE_JUMP)
			inst = (struct cmdq_instruction *)
				cmdq_pkt_get_va_by_offset(
				pkt, end_addr_mark + CMDQ_INST_SIZE);
		shift_pa = CMDQ_REG_SHIFT_ADDR(
			pkt->cmd_buf_size - cnt_end_addr_mark - CMDQ_INST_SIZE);
		inst->arg_b = CMDQ_GET_ARG_B(shift_pa);
		inst->arg_c = CMDQ_GET_ARG_C(shift_pa);
	}

	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_poll_timeout);

void cmdq_pkt_perf_begin(struct cmdq_pkt *pkt)
{
	dma_addr_t pa;
	struct cmdq_pkt_buffer *buf;

	if (!pkt->buf_size)
		if (cmdq_pkt_add_cmd_buffer(pkt) < 0)
			return;

	pa = cmdq_pkt_get_pa_by_offset(pkt, 0) + CMDQ_DBG_PERFBEGIN;
	cmdq_pkt_write_indriect(pkt, NULL, pa, CMDQ_TPR_ID, ~0);

	buf = list_first_entry(&pkt->buf, typeof(*buf), list_entry);
	*(u32 *)(buf->va_base + CMDQ_DBG_PERFBEGIN) = 0xdeaddead;
}
EXPORT_SYMBOL(cmdq_pkt_perf_begin);

void cmdq_pkt_perf_end(struct cmdq_pkt *pkt)
{
	dma_addr_t pa;
	struct cmdq_pkt_buffer *buf;

	if (!pkt->buf_size)
		if (cmdq_pkt_add_cmd_buffer(pkt) < 0)
			return;

	pa = cmdq_pkt_get_pa_by_offset(pkt, 0) + CMDQ_DBG_PERFEND;
	cmdq_pkt_write_indriect(pkt, NULL, pa, CMDQ_TPR_ID, ~0);

	buf = list_first_entry(&pkt->buf, typeof(*buf), list_entry);
	*(u32 *)(buf->va_base + CMDQ_DBG_PERFEND) = 0xdeaddead;
}
EXPORT_SYMBOL(cmdq_pkt_perf_end);

u32 *cmdq_pkt_get_perf_ret(struct cmdq_pkt *pkt)
{
	struct cmdq_pkt_buffer *buf;

	if (!pkt->cmd_buf_size)
		return NULL;

	buf = list_first_entry(&pkt->buf, typeof(*buf),
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

int cmdq_pkt_acquire_event(struct cmdq_pkt *pkt, u16 event)
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
	arg_b = CMDQ_WFE_UPDATE | CMDQ_WFE_UPDATE_VALUE | CMDQ_WFE_WAIT;
	return cmdq_pkt_append_command(pkt, CMDQ_GET_ARG_C(arg_b),
		CMDQ_GET_ARG_B(arg_b), event,
		0, 0, 0, 0, CMDQ_CODE_WFE);
}
EXPORT_SYMBOL(cmdq_pkt_acquire_event);

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

s32 cmdq_pkt_handshake_event(struct cmdq_pkt *pkt, u16 event)
{
	u16 shake_bit = 1 << (event - CMDQ_EVENT_HANDSHAKE);

	return cmdq_pkt_assign_command(pkt, CMDQ_HANDSHAKE_REG, shake_bit);
}
EXPORT_SYMBOL(cmdq_pkt_handshake_event);

s32 cmdq_pkt_finalize(struct cmdq_pkt *pkt)
{
	int err;

	if (cmdq_pkt_is_finalized(pkt))
		return 0;

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	if (cmdq_util_is_feature_en(CMDQ_LOG_FEAT_PERF))
		cmdq_pkt_perf_end(pkt);

#if IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	IS_ENABLED(CONFIG_MTK_CAM_SECURITY_SUPPORT)
	if (pkt->sec_data) {
		err = cmdq_sec_insert_backup_cookie(pkt);
		if (err)
			return err;
	}
#endif
#endif	/* end of CONFIG_MTK_CMDQ_MBOX_EXT */

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

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
static struct cmdq_flush_item *cmdq_prepare_flush_tiem(struct cmdq_pkt *pkt)
{
	struct cmdq_flush_item *item;

	kfree(pkt->flush_item);
	pkt->flush_item = NULL;

	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item)
		return ERR_PTR(-ENOMEM);

	pkt->flush_item = item;

	return item;
}
#endif

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
static void cmdq_pkt_err_irq_dump(struct cmdq_pkt *pkt)
{
	struct cmdq_client *client = pkt->cl;
	dma_addr_t pc = 0;
	struct cmdq_instruction *inst = NULL;
	const char *mod = "CMDQ";
	struct cmdq_pkt_buffer *buf;
	u32 size = pkt->cmd_buf_size, cnt = 0;
	s32 thread_id = cmdq_mbox_chan_id(client->chan);
	static u8 err_num;

	cmdq_msg("%s pkt:%p", __func__, pkt);

	cmdq_util_dump_lock();
	cmdq_util_error_enable();

	cmdq_util_err("begin of error irq %u", err_num++);

	cmdq_task_get_thread_pc(client->chan, &pc);
	cmdq_util_err("pkt:%lx thread:%d pc:%lx",
		(unsigned long)pkt, thread_id, (unsigned long)pc);

	if (pc) {
		list_for_each_entry(buf, &pkt->buf, list_entry) {
			if (pc < buf->pa_base ||
				pc > buf->pa_base + CMDQ_CMD_BUFFER_SIZE) {
				size -= CMDQ_CMD_BUFFER_SIZE;
				cmdq_util_msg("buffer %u va:0x%p pa:%pa",
					cnt, buf->va_base, &buf->pa_base);
				cnt++;
				continue;
			}
			inst  = (struct cmdq_instruction *)(
				buf->va_base + (pc - buf->pa_base));

			if (size > CMDQ_CMD_BUFFER_SIZE)
				size = CMDQ_CMD_BUFFER_SIZE;

			cmdq_util_msg("error irq buffer %u va:0x%p pa:%pa",
				cnt, buf->va_base, &buf->pa_base);
			cmdq_buf_cmd_parse(buf->va_base, CMDQ_NUM_CMD(size),
				buf->pa_base, pc, NULL);

			break;
		}
	}

	if (inst) {
		/* not sync case, print raw */
		cmdq_util_aee(mod,
			"%s(%s) inst:%#018llx thread:%d",
			mod, cmdq_util_hw_name(client->chan),
			*(u64 *)inst, thread_id);
	} else {
		/* no inst available */
		cmdq_util_aee(mod,
			"%s(%s) instruction not available pc:%#llx thread:%d",
			mod, cmdq_util_hw_name(client->chan), pc, thread_id);
	}

	cmdq_util_error_disable();
	cmdq_util_dump_unlock();
}

static void cmdq_flush_async_cb(struct cmdq_cb_data data)
{
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data.data;
	struct cmdq_flush_item *item = pkt->flush_item;
	struct cmdq_cb_data user_data = {
		.data = item->data, .err = data.err };

	cmdq_log("%s pkt:%p", __func__, pkt);

	if (data.err == -EINVAL)
		cmdq_pkt_err_irq_dump(pkt);

	if (item->cb)
		item->cb(user_data);
	complete(&pkt->cmplt);
	item->done = true;
}
#endif

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
static void cmdq_print_wait_summary(void *chan, dma_addr_t pc,
	struct cmdq_instruction *inst)
{
#define txt_len 128
	char text[txt_len];
	char text_gpr[30] = {0};
	void *base;
	u32 gprid, val, len;

	cmdq_buf_print_wfe(text, txt_len, (u32)(pc & 0xFFFF), (void *)inst);

	if (inst->arg_a >= CMDQ_EVENT_GPR_TIMER &&
		inst->arg_a <= CMDQ_EVENT_GPR_TIMER + CMDQ_GPR_R15) {
		base = cmdq_mbox_get_base(chan);
		gprid = inst->arg_a - CMDQ_EVENT_GPR_TIMER;
		val = readl(base + CMDQ_GPR_R0_OFF + gprid * 4);

		len = snprintf(text_gpr, ARRAY_SIZE(text_gpr),
			" GPR R%u:%#x", gprid, val);
		if (len >= ARRAY_SIZE(text_gpr))
			cmdq_log("len:%d over text_gpr size:%d",
				len, ARRAY_SIZE(text_gpr));
	}

	cmdq_util_msg("curr inst: %s value:%u%s",
		text, cmdq_get_event(chan, inst->arg_a), text_gpr);
}
#endif

void cmdq_pkt_err_dump_cb(struct cmdq_cb_data data)
{
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)

	static u32 err_num;
	struct cmdq_pkt *pkt = (struct cmdq_pkt *)data.data;
	struct cmdq_client *client = pkt->cl;
	struct cmdq_flush_item *item =
		(struct cmdq_flush_item *)pkt->flush_item;
	struct cmdq_instruction *inst = NULL;
	dma_addr_t pc = 0;
	phys_addr_t gce_pa = cmdq_mbox_get_base_pa(client->chan);
	const char *mod = NULL;
	s32 thread_id = cmdq_mbox_chan_id(client->chan);

	cmdq_util_dump_lock();

	/* assign error during dump cb */
	item->err = data.err;

	if (err_num == 0)
		cmdq_util_error_enable();

	cmdq_util_err("Begin of Error %u", err_num);

	cmdq_dump_core(client->chan);

#if IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	IS_ENABLED(CONFIG_MTK_CAM_SECURITY_SUPPORT)
	/* for secure path dump more detail */
	if (pkt->sec_data) {
		cmdq_util_msg("thd:%d Hidden thread info since it's secure",
			thread_id);
		cmdq_sec_err_dump(pkt, client, (u64 **)&inst, &mod);
	} else {
		cmdq_thread_dump(client->chan, pkt, (u64 **)&inst, &pc);
	}

	if (data.err == -ECONNABORTED) {
		cmdq_util_msg("skip since abort");
		goto done;
	}

#else
	cmdq_thread_dump(client->chan, pkt, (u64 **)&inst, &pc);
#endif

	if (inst && inst->op == CMDQ_CODE_WFE)
		cmdq_print_wait_summary(client->chan, pc, inst);
	else if (inst)
		cmdq_buf_cmd_parse((u64 *)inst, 1, pc, pc, "curr inst:");
	else
		cmdq_util_msg("curr inst: Not Available");

	if (item->err_cb) {
		struct cmdq_cb_data cb_data = {
			.data = item->err_data,
			.err = data.err
		};

		item->err_cb(cb_data);
	}

	cmdq_dump_pkt(pkt, pc, true);

#if IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	IS_ENABLED(CONFIG_MTK_CAM_SECURITY_SUPPORT)
	if (!pkt->sec_data)
		cmdq_util_dump_smi();
#else
	cmdq_util_dump_smi();
#endif

	if (inst && inst->op == CMDQ_CODE_WFE) {
		mod = cmdq_event_module_dispatch(gce_pa, inst->arg_a,
			thread_id);
		cmdq_util_aee(mod,
			"DISPATCH:%s(%s) inst:%#018llx OP:WAIT EVENT:%hu thread:%d",
			mod, cmdq_util_hw_name(client->chan),
			*(u64 *)inst, inst->arg_a, thread_id);
	} else if (inst) {
		if (!mod)
			mod = cmdq_thread_module_dispatch(gce_pa, thread_id);

		/* not sync case, print raw */
		cmdq_util_aee(mod,
			"DISPATCH:%s(%s) inst:%#018llx OP:%#04hhx thread:%d",
			mod, cmdq_util_hw_name(client->chan),
			*(u64 *)inst, inst->op, thread_id);
	} else {
		if (!mod)
			mod = "CMDQ";

		/* no inst available */
		cmdq_util_aee(mod,
			"DISPATCH:%s(%s) unknown instruction thread:%d",
			mod, cmdq_util_hw_name(client->chan), thread_id);
	}

done:
	cmdq_util_err("End of Error %u", err_num);
	if (err_num == 0) {
		cmdq_util_error_disable();
		cmdq_util_set_first_err_mod(client->chan, mod);
	}
	err_num++;

	cmdq_util_dump_unlock();

#else
	cmdq_err("cmdq error:%d", data.err);
#endif
}

s32 cmdq_pkt_flush_async(struct cmdq_pkt *pkt,
	cmdq_async_flush_cb cb, void *data)
{
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	struct cmdq_flush_item *item = cmdq_prepare_flush_tiem(pkt);
#endif
	struct cmdq_client *client = pkt->cl;
	s32 err;

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	if (IS_ERR(item))
		return -ENOMEM;
#endif

	err = cmdq_pkt_finalize(pkt);
	if (err < 0)
		return err;

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	item->cb = cb;
	item->data = data;
	pkt->cb.cb = cmdq_flush_async_cb;
	pkt->cb.data = pkt;

	item->err_cb = pkt->err_cb.cb;
	item->err_data = pkt->err_cb.data;
	pkt->err_cb.cb = cmdq_pkt_err_dump_cb;
	pkt->err_cb.data = pkt;

	pkt->rec_submit = sched_clock();
#else
	pkt->cb.cb = cb;
	pkt->cb.data = data;
#endif

	mutex_lock(&client->chan_mutex);
	err = mbox_send_message(client->chan, pkt);
	if (!pkt->task_alloc)
		err = -ENOMEM;
	/* We can send next packet immediately, so just call txdone. */
	mbox_client_txdone(client->chan, 0);
	mutex_unlock(&client->chan_mutex);

	return err;
}
EXPORT_SYMBOL(cmdq_pkt_flush_async);

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
void cmdq_dump_summary(struct cmdq_client *client, struct cmdq_pkt *pkt)
{
	struct cmdq_instruction *inst = NULL;
	dma_addr_t pc;

	cmdq_dump_core(client->chan);
	cmdq_thread_dump(client->chan, pkt, (u64 **)&inst, &pc);
	if (inst && inst->op == CMDQ_CODE_WFE)
		cmdq_print_wait_summary(client->chan, pc, inst);
	else if (inst)
		cmdq_buf_cmd_parse((u64 *)inst, 1, pc, pc,
			"curr inst:");
	else
		cmdq_msg("curr inst: Not Available");
	cmdq_dump_pkt(pkt, pc, false);
}

static int cmdq_pkt_wait_complete_loop(struct cmdq_pkt *pkt)
{
	struct cmdq_client *client = pkt->cl;
	struct cmdq_flush_item *item = pkt->flush_item;
	unsigned long ret;
	int cnt = 0;
	u32 timeout_ms = cmdq_mbox_get_thread_timeout((void *)client->chan);
	bool skip = false;

#if IS_ENABLED(CONFIG_MMPROFILE)
	cmdq_mmp_wait(client->chan, pkt);
#endif

	/* make sure gce won't turn off during dump */
	cmdq_mbox_enable(client->chan);

	while (!pkt->task_alloc) {
		ret = wait_for_completion_timeout(&pkt->cmplt,
			msecs_to_jiffies(CMDQ_PREDUMP_TIMEOUT_MS));
		if (ret) {
			/* task alloc failed then skip predump */
			skip = true;
			break;
		}
		cmdq_msg("wait before submit pkt:%p, task_alloc: %d",
			pkt, pkt->task_alloc);
	}

	while (!skip) {
		if (timeout_ms == CMDQ_NO_TIMEOUT) {
			wait_for_completion(&pkt->cmplt);
			break;
		}

		ret = wait_for_completion_timeout(&pkt->cmplt,
			msecs_to_jiffies(CMDQ_PREDUMP_TIMEOUT_MS));
		if (ret)
			break;

		cmdq_util_dump_lock();
		cmdq_msg("===== SW timeout Pre-dump %u =====", cnt++);
		cmdq_dump_summary(client, pkt);
		cmdq_util_dump_unlock();
	}

	pkt->task_alloc = false;
	cmdq_mbox_disable(client->chan);

	return item->err;
}

int cmdq_pkt_wait_complete(struct cmdq_pkt *pkt)
{
	struct cmdq_flush_item *item = pkt->flush_item;

	if (!item) {
		cmdq_err("pkt need flush from flush async ex:0x%p", pkt);
		return -EINVAL;
	}

	pkt->rec_wait = sched_clock();
	cmdq_trace_begin("%s", __func__);

#if IS_ENABLED(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	IS_ENABLED(CONFIG_MTK_CAM_SECURITY_SUPPORT)
	if (pkt->sec_data)
		cmdq_sec_pkt_wait_complete(pkt);
	else
		cmdq_pkt_wait_complete_loop(pkt);
#else
	cmdq_pkt_wait_complete_loop(pkt);
#endif

	cmdq_trace_end();
	cmdq_util_track(pkt);

	return item->err;
}
EXPORT_SYMBOL(cmdq_pkt_wait_complete);
#endif

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
static void cmdq_pkt_flush_q_wait_work(struct work_struct *w)
{
	struct cmdq_flush_item *item_q = container_of(w,
		struct cmdq_flush_item, work);
	int ret;

	ret = cmdq_pkt_wait_complete(item_q->pkt);
	if (item_q->cb) {
		struct cmdq_cb_data data = {.data = item_q->data, .err = ret};

		item_q->cb(data);
	}
	kfree(item_q);
}
#else
static void cmdq_pkt_flush_q_cb_work(struct work_struct *w)
{
	struct cmdq_flush_item *item_q = container_of(w,
		struct cmdq_flush_item, work);
	struct cmdq_cb_data data;

	data.data = item_q->data;
	data.err = item_q->err;
	item_q->cb(data);
	kfree(item_q);
}
#endif

static void cmdq_pkt_flush_q_cb(struct cmdq_cb_data data)
{
	struct cmdq_flush_item *item_q = (struct cmdq_flush_item *)data.data;
	struct cmdq_client *cl = item_q->pkt->cl;
	struct client_priv *priv = cl->cl_priv;

	item_q->err = data.err;
	queue_work(priv->flushq, &item_q->work);
}

s32 cmdq_pkt_flush_threaded(struct cmdq_pkt *pkt,
	cmdq_async_flush_cb cb, void *data)
{
	struct cmdq_flush_item *item_q = kzalloc(sizeof(*item_q), GFP_KERNEL);
	s32 err;

	if (!item_q)
		return -ENOMEM;

	item_q->cb = cb;
	item_q->data = data;
	item_q->pkt = pkt;

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)

	INIT_WORK(&item_q->work, cmdq_pkt_flush_q_wait_work);
	err = cmdq_pkt_flush_async(pkt, NULL, NULL);
	if (err >= 0) {
		struct cmdq_cb_data data = {.data = item_q, .err = 0};

		cmdq_pkt_flush_q_cb(data);
	}
#else
	INIT_WORK(&item_q->work, cmdq_pkt_flush_q_cb_work);
	err = cmdq_pkt_flush_async(pkt, cmdq_pkt_flush_q_cb, item_q);
#endif
	return err;
}
EXPORT_SYMBOL(cmdq_pkt_flush_threaded);

#if !IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
static void cmdq_pkt_flush_cb(struct cmdq_cb_data data)
{
	struct cmdq_flush_completion *cmplt = data.data;

	cmplt->err = !data.err ? false : true;
	complete(&cmplt->cmplt);
}
#endif

int cmdq_pkt_flush(struct cmdq_pkt *pkt)
{
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	int err;

	err = cmdq_pkt_flush_async(pkt, NULL, NULL);
	if (err < 0)
		return err;
	return cmdq_pkt_wait_complete(pkt);
#else
	struct cmdq_flush_completion cmplt;
	int err;

	cmdq_log("start");

	init_completion(&cmplt.cmplt);
	cmplt.pkt = pkt;
	err = cmdq_pkt_flush_async(pkt, cmdq_pkt_flush_cb, &cmplt);
	if (err < 0)
		return err;

	wait_for_completion(&cmplt.cmplt);

	cmdq_log("done pkt:0x%p err:%d", cmplt.pkt, cmplt.err);
	return cmplt.err ? -EFAULT : 0;
#endif
}
EXPORT_SYMBOL(cmdq_pkt_flush);

static void cmdq_buf_print_read(char *text, u32 txt_sz,
	u32 offset, struct cmdq_instruction *cmdq_inst)
{
	u32 addr, len;

	if (cmdq_inst->arg_b_type == CMDQ_IMMEDIATE_VALUE &&
		(cmdq_inst->arg_b & CMDQ_ADDR_LOW_BIT)) {
		/* 48bit format case */
		addr = cmdq_inst->arg_b & 0xfffc;

		len = snprintf(text, txt_sz,
			"%#06x %#018llx [Read ] Reg Index %#010x = addr(low) %#06x",
			offset, *((u64 *)cmdq_inst), cmdq_inst->arg_a, addr);
	} else {
		addr = ((u32)(cmdq_inst->arg_b |
			(cmdq_inst->s_op << CMDQ_SUBSYS_SHIFT)));

		len = snprintf(text, txt_sz,
			"%#06x %#018llx [Read ] Reg Index %#010x = %s%#010x",
			offset, *((u64 *)cmdq_inst), cmdq_inst->arg_a,
			cmdq_inst->arg_b_type ? "*Reg Index " : "SubSys Reg ",
			addr);
	}
	if (len >= txt_sz)
		cmdq_log("len:%d over txt_sz:%d", len, txt_sz);
}

static void cmdq_buf_print_write(char *text, u32 txt_sz,
	u32 offset, struct cmdq_instruction *cmdq_inst)
{
	u32 addr, len;

	if (cmdq_inst->arg_a_type == CMDQ_IMMEDIATE_VALUE &&
		(cmdq_inst->arg_a & CMDQ_ADDR_LOW_BIT)) {
		/* 48bit format case */
		addr = cmdq_inst->arg_a & 0xfffc;

		len = snprintf(text, txt_sz,
			"%#06x %#018llx [Write] addr(low) %#06x = %s%#010x%s",
			offset, *((u64 *)cmdq_inst),
			addr, CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
			cmdq_inst->arg_b_type ? cmdq_inst->arg_b :
			CMDQ_GET_32B_VALUE(cmdq_inst->arg_b, cmdq_inst->arg_c),
			cmdq_inst->op == CMDQ_CODE_WRITE_S_W_MASK ?
			" with mask" : "");
	} else {
		addr = ((u32)(cmdq_inst->arg_a |
			(cmdq_inst->s_op << CMDQ_SUBSYS_SHIFT)));

		len = snprintf(text, txt_sz,
			"%#06x %#018llx [Write] %s%#010x = %s%#010x%s",
			offset, *((u64 *)cmdq_inst),
			cmdq_inst->arg_a_type ? "*Reg Index " : "SubSys Reg ",
			addr, CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
			cmdq_inst->arg_b_type ? cmdq_inst->arg_b :
			CMDQ_GET_32B_VALUE(cmdq_inst->arg_b, cmdq_inst->arg_c),
			cmdq_inst->op == CMDQ_CODE_WRITE_S_W_MASK ?
			" with mask" : "");
	}
	if (len >= txt_sz)
		cmdq_log("len:%d over txt_sz:%d", len, txt_sz);
}

void cmdq_buf_print_wfe(char *text, u32 txt_sz,
	u32 offset, void *inst)
{
	struct cmdq_instruction *cmdq_inst = inst;
	u32 len, cmd = CMDQ_GET_32B_VALUE(cmdq_inst->arg_b, cmdq_inst->arg_c);
	u32 event_op = cmd & 0x80008000;
	u16 update_to = cmdq_inst->arg_b & GENMASK(11, 0);
	u16 wait_to = cmdq_inst->arg_c & GENMASK(11, 0);

	switch (event_op) {
	case 0x80000000:
		len = snprintf(text, txt_sz,
			"%#06x %#018llx [Sync ] %s event %u to %u",
			offset, *((u64 *)cmdq_inst),
			update_to ? "set" : "clear",
			cmdq_inst->arg_a,
			update_to);
		break;
	case 0x8000:
		len = snprintf(text, txt_sz,
			"%#06x %#018llx [Sync ] wait for event %u become %u",
			offset, *((u64 *)cmdq_inst),
			cmdq_inst->arg_a,
			wait_to);
		break;
	case 0x80008000:
	default:
		len = snprintf(text, txt_sz,
			"%#06x %#018llx [Sync ] wait for event %u become %u and %s to %u",
			offset, *((u64 *)cmdq_inst),
			cmdq_inst->arg_a,
			wait_to,
			update_to ? "set" : "clear",
			update_to);
		break;
	}
	if (len >= txt_sz)
		cmdq_log("len:%d over txt_sz:%d", len, txt_sz);
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

static void cmdq_buf_print_move(char *text, u32 txt_sz,
	u32 offset, struct cmdq_instruction *cmdq_inst)
{
	u64 len, val = (u64)cmdq_inst->arg_a |
		CMDQ_GET_32B_VALUE(cmdq_inst->arg_b, cmdq_inst->arg_c);

	if (cmdq_inst->arg_a)
		len = snprintf(text, txt_sz,
			"%#06x %#018llx [Move ] move %#llx to %s%hhu",
			offset, *((u64 *)cmdq_inst), val,
			"Reg Index GPR R", cmdq_inst->s_op);
	else
		len = snprintf(text, txt_sz,
			"%#06x %#010x [Move ] mask %#018llx",
			offset, *((u32 *)cmdq_inst), ~val);
	if (len >= txt_sz)
		cmdq_log("len:%d over txt_sz:%d", len, txt_sz);
}

static void cmdq_buf_print_logic(char *text, u32 txt_sz,
	u32 offset, struct cmdq_instruction *cmdq_inst)
{
	u32 len;

	switch (cmdq_inst->s_op) {
	case CMDQ_LOGIC_ASSIGN:
		len = snprintf(text, txt_sz,
			"%#06x %#018llx [Logic] Reg Index %#06x %s%s%#010x",
			offset, *((u64 *)cmdq_inst), cmdq_inst->arg_a,
			cmdq_parse_logic_sop(cmdq_inst->s_op),
			CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
			CMDQ_GET_32B_VALUE(cmdq_inst->arg_b, cmdq_inst->arg_c));
		break;
	case CMDQ_LOGIC_NOT:
		len = snprintf(text, txt_sz,
			"%#06x %#018llx [Logic] Reg Index %#06x %s%s%#010x",
			offset, *((u64 *)cmdq_inst), cmdq_inst->arg_a,
			cmdq_parse_logic_sop(cmdq_inst->s_op),
			CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
			cmdq_inst->arg_b);
		break;
	default:
		len = snprintf(text, txt_sz,
			"%#06x %#018llx [Logic] %s%#010x = %s%#010x %s%s%#010x",
			offset, *((u64 *)cmdq_inst),
			CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_a_type),
			cmdq_inst->arg_a,
			CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
			cmdq_inst->arg_b, cmdq_parse_logic_sop(cmdq_inst->s_op),
			CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_c_type),
			cmdq_inst->arg_c);
		break;
	}
	if (len >= txt_sz)
		cmdq_log("len:%d over txt_sz:%d", len, txt_sz);
}

static void cmdq_buf_print_write_jump_c(char *text, u32 txt_sz,
	u32 offset, struct cmdq_instruction *cmdq_inst)
{
	u32 len;

	len = snprintf(text, txt_sz,
		"%#06x %#018llx [Jumpc] %s if (%s%#010x %s %s%#010x) jump %s%#010x",
		offset, *((u64 *)cmdq_inst),
		cmdq_inst->op == CMDQ_CODE_JUMP_C_ABSOLUTE ?
		"absolute" : "relative",
		CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
		cmdq_inst->arg_b, cmdq_parse_jump_c_sop(cmdq_inst->s_op),
		CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_c_type), cmdq_inst->arg_c,
		CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_a_type), cmdq_inst->arg_a);
	if (len >= txt_sz)
		cmdq_log("len:%d over txt_sz:%d", len, txt_sz);
}

static void cmdq_buf_print_poll(char *text, u32 txt_sz,
	u32 offset, struct cmdq_instruction *cmdq_inst)
{
	u32 len, addr = ((u32)(cmdq_inst->arg_a |
		(cmdq_inst->s_op << CMDQ_SUBSYS_SHIFT)));

	len = snprintf(text, txt_sz,
		"%#06x %#018llx [Poll ] poll %s%#010x = %s%#010x",
		offset, *((u64 *)cmdq_inst),
		cmdq_inst->arg_a_type ? "*Reg Index " : "SubSys Reg ",
		addr,
		CMDQ_REG_IDX_PREFIX(cmdq_inst->arg_b_type),
		CMDQ_GET_32B_VALUE(cmdq_inst->arg_b, cmdq_inst->arg_c));
	if (len >= txt_sz)
		cmdq_log("len:%d over txt_sz:%d", len, txt_sz);
}

static void cmdq_buf_print_jump(char *text, u32 txt_sz,
	u32 offset, struct cmdq_instruction *cmdq_inst)
{
	u32 len, dst = ((u32)cmdq_inst->arg_b) << 16 | cmdq_inst->arg_c;

	len = snprintf(text, txt_sz,
		"%#06x %#018llx [Jump ] jump %s %#llx",
		offset, *((u64 *)cmdq_inst),
		cmdq_inst->arg_a ? "absolute addr" : "relative offset",
		cmdq_inst->arg_a ? CMDQ_REG_REVERT_ADDR((u64)dst) :
		CMDQ_REG_REVERT_ADDR((s64)(s32)dst));
	if (len >= txt_sz)
		cmdq_log("len:%d over txt_sz:%d", len, txt_sz);
}

static void cmdq_buf_print_misc(char *text, u32 txt_sz,
	u32 offset, struct cmdq_instruction *cmdq_inst)
{
	char *cmd_str;
	u32 len;

	switch (cmdq_inst->op) {
	case CMDQ_CODE_EOC:
		cmd_str = "eoc";
		break;
	default:
		cmd_str = "unknown";
		break;
	}

	len = snprintf(text, txt_sz, "%#06x %#018llx %s",
		offset, *((u64 *)cmdq_inst), cmd_str);
	if (len >= txt_sz)
		cmdq_log("len:%d over txt_sz:%d", len, txt_sz);
}

void cmdq_buf_cmd_parse(u64 *buf, u32 cmd_nr, dma_addr_t buf_pa,
	dma_addr_t cur_pa, const char *info)
{
#define txt_sz 128
	static char text[txt_sz];
	struct cmdq_instruction *cmdq_inst = (struct cmdq_instruction *)buf;
	u32 i;

	for (i = 0; i < cmd_nr; i++) {
		switch (cmdq_inst[i].op) {
		case CMDQ_CODE_WRITE_S:
		case CMDQ_CODE_WRITE_S_W_MASK:
			cmdq_buf_print_write(text, txt_sz, (u32)buf_pa,
				&cmdq_inst[i]);
			break;
		case CMDQ_CODE_WFE:
			cmdq_buf_print_wfe(text, txt_sz, (u32)buf_pa,
				(void *)&cmdq_inst[i]);
			break;
		case CMDQ_CODE_MOVE:
			cmdq_buf_print_move(text, txt_sz, (u32)buf_pa,
				&cmdq_inst[i]);
			break;
		case CMDQ_CODE_READ_S:
			cmdq_buf_print_read(text, txt_sz, (u32)buf_pa,
				&cmdq_inst[i]);
			break;
		case CMDQ_CODE_LOGIC:
			cmdq_buf_print_logic(text, txt_sz, (u32)buf_pa,
				&cmdq_inst[i]);
			break;
		case CMDQ_CODE_JUMP_C_ABSOLUTE:
		case CMDQ_CODE_JUMP_C_RELATIVE:
			cmdq_buf_print_write_jump_c(text, txt_sz, (u32)buf_pa,
				&cmdq_inst[i]);
			break;
		case CMDQ_CODE_POLL:
			cmdq_buf_print_poll(text, txt_sz, (u32)buf_pa,
				&cmdq_inst[i]);
			break;
		case CMDQ_CODE_JUMP:
			cmdq_buf_print_jump(text, txt_sz, (u32)buf_pa,
				&cmdq_inst[i]);
			break;
		default:
			cmdq_buf_print_misc(text, txt_sz, (u32)buf_pa,
				&cmdq_inst[i]);
			break;
		}
		cmdq_util_msg("%s%s",
			info ? info : (buf_pa == cur_pa ? ">>" : "  "),
			text);
		buf_pa += CMDQ_INST_SIZE;
	}
}

s32 cmdq_pkt_dump_buf(struct cmdq_pkt *pkt, dma_addr_t curr_pa)
{
	struct cmdq_pkt_buffer *buf;
	u32 size, cnt = 0;

	list_for_each_entry(buf, &pkt->buf, list_entry) {
		if (list_is_last(&buf->list_entry, &pkt->buf)) {
			size = CMDQ_CMD_BUFFER_SIZE - pkt->avail_buf_size;
		} else if (cnt > 2 && !(curr_pa >= buf->pa_base &&
			curr_pa < buf->pa_base + CMDQ_BUF_ALLOC_SIZE)) {
			cmdq_util_msg(
				"buffer %u va:0x%p pa:%pa %#018llx (skip detail) %#018llx",
				cnt, buf->va_base, &buf->pa_base,
				*((u64 *)buf->va_base),
				*((u64 *)(buf->va_base +
				CMDQ_CMD_BUFFER_SIZE - CMDQ_INST_SIZE)));
			cnt++;
			continue;
		} else {
			size = CMDQ_CMD_BUFFER_SIZE;
		}
		cmdq_util_msg("buffer %u va:0x%p pa:%pa",
			cnt, buf->va_base, &buf->pa_base);
		cmdq_buf_cmd_parse(buf->va_base, CMDQ_NUM_CMD(size),
			buf->pa_base, curr_pa, NULL);
		cnt++;
	}

	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_dump_buf);

int cmdq_dump_pkt(struct cmdq_pkt *pkt, dma_addr_t pc, bool dump_ist)
{
	if (!pkt)
		return -EINVAL;

	cmdq_util_msg(
		"pkt:0x%p(%#x) size:%zu/%zu avail size:%zu priority:%u%s",
		pkt, (u32)(unsigned long)pkt, pkt->cmd_buf_size,
		pkt->buf_size, pkt->avail_buf_size,
		pkt->priority, pkt->loop ? " loop" : "");
#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	cmdq_util_msg(
		"submit:%llu trigger:%llu wait:%llu irq:%llu",
		pkt->rec_submit, pkt->rec_trigger,
		pkt->rec_wait, pkt->rec_irq);
#endif
	if (dump_ist)
		cmdq_pkt_dump_buf(pkt, pc);

	return 0;
}
EXPORT_SYMBOL(cmdq_dump_pkt);

void cmdq_pkt_set_err_cb(struct cmdq_pkt *pkt,
	cmdq_async_flush_cb cb, void *data)
{
	pkt->err_cb.cb = cb;
	pkt->err_cb.data = (void *)data;
}
EXPORT_SYMBOL(cmdq_pkt_set_err_cb);

