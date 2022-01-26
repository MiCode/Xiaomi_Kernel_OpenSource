/*
 * Copyright (c) 2019 MediaTek Inc.
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


#include <linux/mailbox_controller.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_fbconfig_kdebug.h"

#define CMDQ_MBOX_BUF_LIMIT 16 /* default limit count */

// for FPGA porting, pa 2 va
#define DISP_REG_ADDR_BASE   0x14000000
#define DISP_REG_ADDR_MASK   0xffffffl
#define DISP_COMP_REG_ADDR_MASK   0xfffl

struct client_priv {
	struct dma_pool *buf_pool;
	u32 pool_limit;
	atomic_t buf_cnt;
	struct workqueue_struct *flushq;
};

struct regs_addr_record {
	void __iomem *regs;
	resource_size_t regs_pa;
	struct list_head list;
};

struct list_head addr_head;
bool hasInit;

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
}
EXPORT_SYMBOL(cmdq_mbox_stop);

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
}

/* parse event from dts
 *
 * Example
 *
 * dts:
 * gce-event-names = "disp_rdma0_sof",
 *  "disp_rdma1_sof",
 *  "mdp_rdma0_sof";
 * gce-events = <&gce_mbox CMDQ_EVENT_DISP_RDMA0_SOF>,
 *  <&gce_mbox CMDQ_EVENT_DISP_RDMA1_SOF>,
 *  <&gce_mbox CMDQ_EVENT_MDP_RDMA0_SOF>;
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

void cmdq_mbox_destroy(struct cmdq_client *client)
{
	kfree(client->cl_priv);
	kfree(client);
}
EXPORT_SYMBOL(cmdq_mbox_destroy);

struct cmdq_pkt *cmdq_pkt_create(struct cmdq_client *client)
{
	struct cmdq_pkt *pkt;

	pkt = kzalloc(sizeof(*pkt), GFP_KERNEL);
	if (!IS_ERR_OR_NULL(pkt))
		return pkt;
	return NULL;
}
EXPORT_SYMBOL(cmdq_pkt_create);

void cmdq_pkt_destroy(struct cmdq_pkt *pkt)
{
	struct cmdq_client *client = pkt->cl;

	if (client)
		mutex_lock(&client->chan_mutex);

	kfree(pkt->flush_item);
	kfree(pkt);
	if (client)
		mutex_unlock(&client->chan_mutex);
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

s32 cmdq_pkt_read(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	dma_addr_t src_addr, u16 dst_reg_idx)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_read);

s32 cmdq_pkt_write_reg_addr(struct cmdq_pkt *pkt, dma_addr_t addr,
	u16 src_reg_idx, u32 mask)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_write_reg_addr);

s32 cmdq_pkt_write(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	dma_addr_t addr, u32 value, u32 mask)
{
	unsigned long pa = (unsigned long) addr;
	void __iomem *va = 0;
	struct regs_addr_record *regs_addr;
	unsigned int i = 0;

	struct mtk_drm_private *private = drm_dev->dev_private;
	struct mtk_ddp_comp *comp;

	if (DISP_REG_ADDR_BASE != (pa & (~DISP_REG_ADDR_MASK))) {
		DDPDBG("%s error pa:%x, addr:%x\n",
			 __func__, pa, addr);
		return -1;
	}
	// get va from comp
	for (i = 0; i < DDP_COMPONENT_ID_MAX; i++) {
		if (!private->ddp_comp[i])
			continue;
		comp = private->ddp_comp[i];
		if (comp->regs_pa == (pa & (~DISP_COMP_REG_ADDR_MASK))) {
			va = comp->regs + (pa & DISP_COMP_REG_ADDR_MASK);
			writel(value, va);
			return 0;
		}
	}

	//remap pa to va
	if (!hasInit) {
		INIT_LIST_HEAD(&addr_head);
		hasInit = true;
	}
	list_for_each_entry(regs_addr, &addr_head, list) {
		if (regs_addr && regs_addr->regs_pa == pa) {
			va = regs_addr->regs;
			break;
		}
	}
	if (!va) {
		va = ioremap_nocache(pa, sizeof(va));
		regs_addr = kzalloc(sizeof(struct repaint_job_t),
			GFP_KERNEL);
		if (!IS_ERR_OR_NULL(regs_addr)) {
			regs_addr->regs = va;
			regs_addr->regs_pa = pa;
			list_add_tail(&regs_addr->list, &addr_head);
		}
		DDPDBG("%s:%x, va:%u\n",
			__func__, pa, va);
	}

	writel(value, va);
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_write);

s32 cmdq_pkt_mem_move(struct cmdq_pkt *pkt, struct cmdq_base *clt_base,
	dma_addr_t src_addr, dma_addr_t dst_addr, u16 swap_reg_idx)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_mem_move);

s32 cmdq_pkt_assign_command(struct cmdq_pkt *pkt, u16 reg_idx, u32 value)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_assign_command);

s32 cmdq_pkt_logic_command(struct cmdq_pkt *pkt, enum CMDQ_LOGIC_ENUM s_op,
	u16 result_reg_idx,
	struct cmdq_operand *left_operand,
	struct cmdq_operand *right_operand)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_logic_command);

s32 cmdq_pkt_cond_jump_abs(struct cmdq_pkt *pkt,
	u16 addr_reg_idx,
	struct cmdq_operand *left_operand,
	struct cmdq_operand *right_operand,
	enum CMDQ_CONDITION_ENUM condition_operator)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_cond_jump_abs);

s32 cmdq_pkt_poll_reg(struct cmdq_pkt *pkt, u32 value, u8 subsys,
	u16 offset, u32 mask)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_poll_reg);

s32 cmdq_pkt_poll_timeout(struct cmdq_pkt *pkt, u32 value, u8 subsys,
	phys_addr_t addr, u32 mask, u16 count, u16 reg_gpr)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_poll_timeout);

int cmdq_pkt_wfe(struct cmdq_pkt *pkt, u16 event)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_wfe);

int cmdq_pkt_wait_no_clear(struct cmdq_pkt *pkt, u16 event)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_wait_no_clear);

s32 cmdq_pkt_clear_event(struct cmdq_pkt *pkt, u16 event)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_clear_event);

s32 cmdq_pkt_set_event(struct cmdq_pkt *pkt, u16 event)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_set_event);

s32 cmdq_pkt_finalize_loop(struct cmdq_pkt *pkt)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_finalize_loop);

s32 cmdq_pkt_flush_async(struct cmdq_pkt *pkt,
	cmdq_async_flush_cb cb, void *data)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_flush_async);

s32 cmdq_pkt_flush_threaded(struct cmdq_pkt *pkt,
	cmdq_async_flush_cb cb, void *data)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_flush_threaded);

int cmdq_pkt_flush(struct cmdq_pkt *pkt)
{
	return 0;
}
EXPORT_SYMBOL(cmdq_pkt_flush);
