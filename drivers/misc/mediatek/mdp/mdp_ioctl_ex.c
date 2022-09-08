// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/dma-buf.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
#include "mdp_def.h"
#include "mdp_common.h"
#include "mdp_driver.h"
#include "mdp_pmqos.h"
#else
#include "cmdq_def.h"
#include "cmdq_mdp_common.h"
#include "cmdq_driver.h"
#endif
#include "mdp_def_ex.h"
#include "mdp_ioctl_ex.h"
#include "cmdq_struct.h"
#include "cmdq_helper_ext.h"
#include "cmdq_record.h"
#include "cmdq_device.h"

#include "mdp_rdma_ex.h"

#ifdef CMDQ_SECURE_PATH_SUPPORT
#include "mtk_heap.h"
#endif

#define MDP_TASK_PAENDING_TIME_MAX	100000000

#define RDMA_CPR_PREBUILT(mod, pipe, index) \
	((index) < CMDQ_CPR_PREBUILT_REG_CNT ? \
	CMDQ_CPR_PREBUILT(mod, pipe, index) : \
	CMDQ_CPR_PREBUILT_EXT(mod, pipe, (index) - CMDQ_CPR_PREBUILT_REG_CNT))

struct mdpsys_con_context {
	struct device *dev;
};
struct mdpsys_con_context mdpsys_con_ctx;

#define MAX_HANDLE_NUM 10

static u64 job_mapping_idx = 1;
static struct list_head job_mapping_list;
struct mdp_job_mapping {
	u64 id;
	struct cmdqRecStruct *job;
	struct list_head list_entry;

	struct dma_buf *dma_bufs[MAX_HANDLE_NUM];
	struct dma_buf_attachment *attaches[MAX_HANDLE_NUM];
	struct sg_table *sgts[MAX_HANDLE_NUM];

	int fds[MAX_HANDLE_NUM];
	unsigned long mvas[MAX_HANDLE_NUM];
	u32 handle_count;
	void *node;
};
static DEFINE_MUTEX(mdp_job_mapping_list_mutex);

#define SLOT_GROUP_NUM 64
#define MAX_RB_SLOT_NUM (SLOT_GROUP_NUM*64)
#define MAX_COUNT_IN_RB_SLOT 0x1000 /* 4KB */
#define SLOT_ID_SHIFT 16
#define SLOT_OFFSET_MASK 0xFFFF

struct mdp_readback_slot {
	u32 count;
	dma_addr_t pa_start;
	void *fp;
};

static struct mdp_readback_slot rb_slot[MAX_RB_SLOT_NUM];
static u64 alloc_slot[SLOT_GROUP_NUM];
static u64 alloc_slot_group;
static DEFINE_MUTEX(rb_slot_list_mutex);

/* These are for pq vcp readback */
dma_addr_t vcp_paStart;
static u64 rb_slot_vcp[SLOT_GROUP_NUM];

static dma_addr_t translate_read_id_ex(u32 read_id, u32 *slot_offset)
{
	u32 slot_id;

	slot_id = read_id >> SLOT_ID_SHIFT;
	*slot_offset = read_id & SLOT_OFFSET_MASK;
	CMDQ_MSG("translate read id:%#x\n", read_id);
	if (unlikely(slot_id >= MAX_RB_SLOT_NUM ||
		*slot_offset >= rb_slot[slot_id].count)) {
		CMDQ_ERR("invalid read id:%#x\n", read_id);
		*slot_offset = 0;
		return 0;
	}
	if (!rb_slot[slot_id].pa_start)
		*slot_offset = 0;
	return rb_slot[slot_id].pa_start;
}

static dma_addr_t translate_read_id(u32 read_id)
{
	u32 slot_offset;

	return translate_read_id_ex(read_id, &slot_offset)
		+ slot_offset * sizeof(u32);
}

static u32 translate_engine_rdma(u32 engine)
{
	s32 rdma_idx = cmdq_mdp_get_rdma_idx(engine);

	if (rdma_idx < 0) {
		CMDQ_ERR("invalia rdma idx, set rdma0 as default\n");
		rdma_idx = 0;
	}
	return rdma_idx;
}

static s32 mdp_process_read_request(struct mdp_read_readback *req_user)
{
	/* create kernel-space buffer for working */
	u32 *ids = NULL;
	dma_addr_t *addrs = NULL;
	u32 *values = NULL;
	void *ids_user = NULL;
	void *values_user = NULL;
	s32 status = -EINVAL;
	u32 count, i;

	CMDQ_SYSTRACE_BEGIN("%s\n", __func__);

	do {
		if (!req_user || !req_user->count ||
			req_user->count > CMDQ_MAX_DUMP_REG_COUNT) {
			CMDQ_MSG("[READ_PA] no need to readback\n");
			status = 0;
			break;
		}

		count = req_user->count;
		CMDQ_MSG("[READ_PA] %s - count:%d\n", __func__, count);
		ids_user = (void *)CMDQ_U32_PTR(req_user->ids);
		values_user = (void *)CMDQ_U32_PTR(req_user->ret_values);
		if (!ids_user || !values_user) {
			CMDQ_ERR("[READ_PA] invalid in/out addr\n");
			break;
		}

		ids = kcalloc(count, sizeof(u32), GFP_KERNEL);
		if (!ids) {
			CMDQ_ERR("[READ_PA] fail to alloc id buf\n");
			status = -ENOMEM;
			break;
		}

		addrs = kcalloc(count, sizeof(dma_addr_t), GFP_KERNEL);
		if (!addrs) {
			CMDQ_ERR("[READ_PA] fail to alloc addr buf\n");
			status = -ENOMEM;
			break;
		}

		values = kcalloc(count, sizeof(u32), GFP_KERNEL);
		if (!values) {
			CMDQ_ERR("[READ_PA] fail to alloc value buf\n");
			status = -ENOMEM;
			break;
		}

		if (copy_from_user(ids, ids_user, count * sizeof(u32))) {
			CMDQ_ERR("[READ_PA] copy user:0x%p fail\n", ids_user);
			break;
		}

		status = 0;
		/* TODO: Refine read PA write buffers efficiency */
		for (i = 0; i < count; i++) {
			addrs[i] = translate_read_id(ids[i]);
			CMDQ_MSG("[READ_PA] %s: [%d]-%x=%pa\n",
				__func__, i, ids[i], &addrs[i]);
			if (unlikely(!addrs[i])) {
				status = -EINVAL;
				break;
			}
		}
		if (status < 0)
			break;

		CMDQ_SYSTRACE_BEGIN("%s_copy_to_user_%u\n", __func__, count);

		cmdqCoreReadWriteAddressBatch(addrs, count, values);
		cmdq_driver_dump_readback(addrs, count, values);

		/* copy value to user */
		if (copy_to_user(values_user, values, count * sizeof(u32))) {
			CMDQ_ERR("[READ_PA] fail to copy to user\n");
			status = -EINVAL;
		}

		CMDQ_SYSTRACE_END();
	} while (0);

	kfree(ids);
	kfree(addrs);
	kfree(values);

	CMDQ_SYSTRACE_END();
	return status;
}

static bool mdp_ion_get_dma_buf(struct device *dev, int fd,
	struct dma_buf **buf_out, struct dma_buf_attachment **attach_out,
	struct sg_table **sgt_out)
{
	struct dma_buf *buf = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *sgt = NULL;

	if (fd <= 0) {
		CMDQ_ERR("ion error fd %d\n", fd);
		goto err;
	}

	buf = dma_buf_get(fd);
	if (IS_ERR(buf)) {
		CMDQ_ERR("ion buf get fail %ld\n", PTR_ERR(buf));
		goto err;
	}

	attach = dma_buf_attach(buf, dev);
	if (IS_ERR(attach)) {
		CMDQ_ERR("ion buf attach fail %ld", PTR_ERR(attach));
		goto err_attach;
	}

	sgt =  dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		CMDQ_ERR("ion buf map fail %ld", PTR_ERR(sgt));
		goto err_map;
	}

	*buf_out = buf;
	*attach_out = attach;
	*sgt_out = sgt;

	return true;

err_map:
	dma_buf_detach(buf, attach);

err_attach:
	dma_buf_put(buf);
err:
	return false;
}

static void mdp_ion_free_dma_buf(struct dma_buf *buf,
	struct dma_buf_attachment *attach, struct sg_table *sgt)
{
	dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(buf, attach);
	dma_buf_put(buf);
}

static unsigned long translate_fd(struct op_meta *meta,
				struct mdp_job_mapping *mapping_job)
{
	struct dma_buf *buf = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *sgt = NULL;
	dma_addr_t ion_addr;
	u32 i;

	if (!mdpsys_con_ctx.dev) {
		CMDQ_ERR("%s mdpsys_config not ready\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < mapping_job->handle_count; i++)
		if (mapping_job->fds[i] == meta->fd)
			break;

	if (i == mapping_job->handle_count) {
		if (i >= MAX_HANDLE_NUM) {
			CMDQ_ERR("%s no more handle room\n", __func__);
			return 0;
		}
		/* need to map ion fd to iova */
		if (!mdp_ion_get_dma_buf(mdpsys_con_ctx.dev, meta->fd, &buf,
			&attach, &sgt))
			return 0;

		ion_addr = sg_dma_address(sgt->sgl);
		if (ion_addr) {
			mapping_job->fds[i] = meta->fd;
			mapping_job->attaches[i] = attach;
			mapping_job->dma_bufs[i] = buf;
			mapping_job->sgts[i] = sgt;
			mapping_job->mvas[i] = ion_addr;
			mapping_job->handle_count++;

			CMDQ_MSG("%s fd:%d -> iova:%#llx\n",
				__func__, meta->fd, (u64)ion_addr);
		} else {
			CMDQ_ERR("%s fail to get iova for fd:%d\n",
				__func__, meta->fd);
			mdp_ion_free_dma_buf(buf, attach, sgt);
			return 0;
		}
	} else {
		ion_addr = mapping_job->mvas[i];
	}
	ion_addr += meta->fd_offset;

	return ion_addr;
}

static s32 translate_meta(struct op_meta *meta,
			  struct mdp_job_mapping *mapping_job,
			  struct cmdqRecStruct *handle,
			  struct cmdq_command_buffer *cmd_buf)
{
	u32 reg_addr;
	s32 status = 0;

	switch (meta->op) {
	case CMDQ_MOP_WRITE_FD:
	{
		u32 reg_addr_msb = 0;
		unsigned long mva = translate_fd(meta, mapping_job);

		if (!mva) {
			CMDQ_ERR("%s: op:%u, get mva fail, engine %d, fd 0x%x, fd_offset 0x%x\n",
				 __func__, meta->op, meta->engine, meta->fd, meta->fd_offset);
			return -EINVAL;
		}

		/* check platform support LSB/MSB or not */
		if (gMdpRegMSBSupport) {

			reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
			if (reg_addr) {
				status = cmdq_op_write_reg_ex(
					handle, cmd_buf, reg_addr, mva & U32_MAX, ~0);
			} else {
				CMDQ_ERR("%s: op:%u, get reg_addr fail, eng:%d, offset 0x%x\n",
					__func__, meta->op, meta->engine, meta->offset);
				return -EINVAL;
			}

			reg_addr_msb = cmdq_mdp_get_hw_reg_msb(meta->engine, meta->offset);
			if (reg_addr_msb) {
				status = cmdq_op_write_reg_ex(
					handle, cmd_buf, reg_addr_msb, DO_SHIFT_RIGHT(mva, 32), ~0);
			} else {
				CMDQ_ERR("%s: op:%u, get reg_addr_msb fail, eng:%d, offset 0x%x\n",
					__func__, meta->op, meta->engine, meta->offset);
				return -EINVAL;
			}
		} else {
			reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
			if (reg_addr) {
				status = cmdq_op_write_reg_ex(
					handle, cmd_buf, reg_addr, mva, ~0);
			} else {
				CMDQ_ERR("%s: op:%u, get reg_addr fail, eng:%d, offset 0x%x\n",
					__func__, meta->op, meta->engine, meta->offset);
				return -EINVAL;
			}
		}

		break;
	}
	case CMDQ_MOP_WRITE:
	{
		reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);

		if (!reg_addr)
			return -EINVAL;

		status = cmdq_op_write_reg_ex(handle, cmd_buf, reg_addr, meta->value, meta->mask);
		break;
	}
	case CMDQ_MOP_WRITE_FD_RDMA:
	{
		u32 rdma_idx, src_base_lsb, src_base_msb;
		unsigned long mva = translate_fd(meta, mapping_job);

		rdma_idx = translate_engine_rdma(meta->engine);

		if (!mva) {
			CMDQ_ERR("%s: op:%u, get mva fail, engine %d, fd 0x%x, fd_offset 0x%x\n",
				 __func__, meta->op, meta->engine, meta->fd, meta->fd_offset);
			return -EINVAL;
		}

		if ((rdma_idx != 0) && (rdma_idx != 1)) {
			CMDQ_ERR("%s: op:%u, engine %d, rdma_idx %d invalid\n",
				 __func__, meta->op, meta->engine, rdma_idx);
			return -EINVAL;
		}

		if ((meta->cpr_idx >= CPR_MDP_RDMA_SRC_BASE_0) &&
		    (meta->cpr_idx <= CPR_MDP_RDMA_UFO_DEC_LENGTH_BASE_C)) {

			/* check platform support LSB/MSB or not */
			if (gMdpRegMSBSupport) {
				src_base_msb = DO_SHIFT_RIGHT(mva, 32);
				src_base_lsb = mva & U32_MAX;
			} else {
				src_base_msb = 0;
				src_base_lsb = mva;
			}

			status = cmdq_op_assign_reg_idx_ex(handle, cmd_buf,
					RDMA_CPR_PREBUILT(CMDQ_PREBUILT_MDP,
							  meta->pipe_idx, meta->cpr_idx),
					src_base_lsb);

			status = cmdq_op_assign_reg_idx_ex(handle, cmd_buf,
					RDMA_CPR_PREBUILT(CMDQ_PREBUILT_MDP,
							  meta->pipe_idx, meta->cpr_idx + 5),
					src_base_msb);
		} else {
			CMDQ_ERR("%s: op:%u, engine %d, rdma_idx %d, cpr_idx %d invalid\n",
				 __func__, meta->op, meta->engine, rdma_idx, meta->cpr_idx);
			return -EINVAL;
		}
		break;
	}
	case CMDQ_MOP_WRITE_RDMA:
	{
		u32 src_base_lsb, src_base_msb;
		u32 rdma_idx = translate_engine_rdma(meta->engine);

		if ((rdma_idx != 0) && (rdma_idx != 1)) {
			CMDQ_ERR("%s: op:%u, engine %d, rdma_idx %d invalid\n",
				 __func__, meta->op, meta->engine, rdma_idx);
			return -EINVAL;
		}

		if (meta->cpr_idx > CPR_MDP_RDMA_PIPE_IDX) {
			CMDQ_ERR("%s: op:%u, engine %d, rdma_idx %d, cpr_idx %d invalid\n",
				 __func__, meta->op, meta->engine, rdma_idx, meta->cpr_idx);
			return -EINVAL;
		}

		if (meta->cpr_idx == CPR_MDP_RDMA_PIPE_IDX) {
			status = cmdq_op_assign_reg_idx_ex(handle, cmd_buf,
					CMDQ_CPR_PREBUILT_PIPE(CMDQ_PREBUILT_MDP), meta->pipe_idx);
		} else if ((meta->cpr_idx >= CPR_MDP_RDMA_SRC_BASE_0) &&
			   (meta->cpr_idx <= CPR_MDP_RDMA_UFO_DEC_LENGTH_BASE_C)) {

			src_base_msb = 0x0;
			src_base_lsb = meta->value;

			status = cmdq_op_assign_reg_idx_ex(handle, cmd_buf,
					RDMA_CPR_PREBUILT(CMDQ_PREBUILT_MDP,
							  meta->pipe_idx, meta->cpr_idx),
					src_base_lsb);

			status = cmdq_op_assign_reg_idx_ex(handle, cmd_buf,
					RDMA_CPR_PREBUILT(CMDQ_PREBUILT_MDP,
							  meta->pipe_idx, meta->cpr_idx + 5),
					src_base_msb);
		} else {
			status = cmdq_op_assign_reg_idx_ex(handle, cmd_buf,
					RDMA_CPR_PREBUILT(CMDQ_PREBUILT_MDP,
							  meta->pipe_idx, meta->cpr_idx),
					meta->value);
		}
		break;
	}
	case CMDQ_MOP_READ:
	{
		u32 offset;
		dma_addr_t dram_addr;

		reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
		dram_addr = translate_read_id_ex(meta->readback_id, &offset);
		if (!reg_addr || !dram_addr)
			return -EINVAL;
		status = cmdq_op_read_reg_to_mem_ex(handle, cmd_buf,
						dram_addr + gce_mminfra, offset, reg_addr);
		break;
	}
	case CMDQ_MOP_READBACK:
	{
		dma_addr_t dram_addr;
		u32 offset;
		u32 vcp_offset = 0;

		if (!cmdq_mdp_vcp_pq_readback_support()) {
			dram_addr = translate_read_id_ex(meta->readback_id, &offset);
			if (!dram_addr)
				return -EINVAL;

#if defined(CMDQ_SECURE_PATH_SUPPORT)
			if (handle->secData.is_secure) {
				cmdq_op_set_event(handle, mdp_get_rb_event_lock());
				cmdq_op_wait(handle, mdp_get_rb_event_unlock());
			}
#endif
			/* flush first since readback add commands to pkt */
			cmdq_handle_flush_cmd_buf(handle, cmd_buf);

			cmdq_mdp_op_readback(handle, meta->engine,
				dram_addr + offset * sizeof(u32) + gce_mminfra, meta->mask);
		} else {
			dram_addr = translate_read_id_ex(meta->readback_id, &offset);
			if (!dram_addr)
				return -EINVAL;

			/* flush first since readback add commands to pkt */
			cmdq_handle_flush_cmd_buf(handle, cmd_buf);

			vcp_offset = (dram_addr - vcp_paStart) + offset * sizeof(u32);

			CMDQ_LOG_PQ("%s: engine:%d, rb_id:0x%x, dram_addr:%pa, offset:0x%x\n",
				__func__, meta->engine, meta->readback_id,
				&dram_addr, offset);
			CMDQ_LOG_PQ("%s: engine:%d, rb_id:0x%x, vcp_offset:0x%x, vcp_pa:%#llx\n",
				__func__, meta->engine, meta->readback_id,
				vcp_offset, vcp_paStart + vcp_offset);

			cmdq_mdp_vcp_pq_readback(handle, meta->engine,
				vcp_offset, MAX_COUNT_IN_RB_SLOT);
		}

		break;
	}
	case CMDQ_MOP_POLL:
		reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
		if (!reg_addr)
			return -EINVAL;

		status = cmdq_op_poll_ex(handle, cmd_buf, reg_addr,
					meta->value, meta->mask);
		break;
	case CMDQ_MOP_WAIT:
		status = cmdq_op_wait_ex(handle, cmd_buf, meta->event);
		break;
	case CMDQ_MOP_WAIT_NO_CLEAR:
		status = cmdq_op_wait_no_clear_ex(handle, cmd_buf, meta->event);
		break;
	case CMDQ_MOP_CLEAR:
		status = cmdq_op_clear_event_ex(handle, cmd_buf, meta->event);
		break;
	case CMDQ_MOP_SET:
		status = cmdq_op_set_event_ex(handle, cmd_buf, meta->event);
		break;
	case CMDQ_MOP_ACQUIRE:
		status = cmdq_op_acquire_ex(handle, cmd_buf, meta->event);
		break;
	case CMDQ_MOP_WRITE_FROM_REG:
	{
		u32 from_reg = cmdq_mdp_get_hw_reg(meta->from_engine,
			meta->from_offset);

		reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
		if (!reg_addr || !from_reg)
			return -EINVAL;
		status = cmdq_op_write_from_reg_ex(handle, cmd_buf,
					reg_addr, from_reg);
		break;
	}
	case CMDQ_MOP_WRITE_SEC:
	{
		reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
		if (!reg_addr)
			return -EINVAL;
		status = cmdq_op_write_reg_ex(handle, cmd_buf, reg_addr,
					meta->value, ~0);
		/* use total buffer size count in translation */
		if (!status) {
			/* flush to make sure count is correct */
			cmdq_handle_flush_cmd_buf(handle, cmd_buf);
			status = cmdq_mdp_update_sec_addr_index(handle,
				meta->sec_handle, meta->sec_index,
				cmdq_mdp_handle_get_instr_count(handle) - 1);
		}
		break;
	}
	case CMDQ_MOP_NOP:
		break;
#ifdef CMDQ_SECURE_PATH_SUPPORT
	case CMDQ_MOP_WRITE_SEC_FD:
	{
		struct dma_buf *buf;

		/* secure fd -> secure handle */
		buf = dma_buf_get(meta->fd);
		meta->sec_handle = dmabuf_to_secure_handle(buf);
		CMDQ_MSG("CMDQ_MOP_WRITE_SEC_FD: translate fd %d to sec_handle %d\n",
			meta->fd, meta->sec_handle);
		dma_buf_put(buf);

		reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
		if (!reg_addr)
			return -EINVAL;
		status = cmdq_op_write_reg_ex(handle, cmd_buf, reg_addr,
					meta->sec_handle, ~0);

		/* use total buffer size count in translation */
		if (!status) {
			/* flush to make sure count is correct */
			cmdq_handle_flush_cmd_buf(handle, cmd_buf);
			status = cmdq_mdp_update_sec_addr_index(handle,
				meta->sec_handle, meta->sec_index,
				cmdq_mdp_handle_get_instr_count(handle) - 1);
		}
		break;
	}
#endif
	default:
		CMDQ_ERR("invalid meta op:%u\n", meta->op);
		status = -EINVAL;
		break;
	}

	return status;
}

static s32 translate_user_job(struct mdp_submit *user_job,
			struct mdp_job_mapping *mapping_job,
			struct cmdqRecStruct *handle,
			struct cmdq_command_buffer *cmd_buf)
{
	struct op_meta *metas;
	s32 status = 0;
	u32 i, copy_size, copy_count, remain_count;
	void *cur_src = CMDQ_U32_PTR(user_job->metas);
	const u32 meta_count_in_page = PAGE_SIZE / sizeof(struct op_meta);

	metas = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!metas) {
		CMDQ_ERR("allocate metas fail\n");
		return -ENOMEM;
	}

	remain_count = user_job->meta_count;
	while (remain_count > 0) {
		copy_count = min_t(u32, remain_count, meta_count_in_page);
		copy_size = copy_count * sizeof(struct op_meta);
		if (copy_from_user(metas, cur_src, copy_size)) {
			CMDQ_ERR("copy metas from user fail:%u\n", copy_size);
			kfree(metas);
			return -EINVAL;
		}

		for (i = 0; i < copy_count; i++) {
#ifdef META_DEBUG
			CMDQ_MSG("translate meta[%u] (%u,%u,%#x,%#x,%#x)\n", i,
				metas[i].op, metas[i].engine, metas[i].offset,
				metas[i].value, metas[i].mask);
#endif
			status = translate_meta(&metas[i], mapping_job,
						handle, cmd_buf);
			if (unlikely(status < 0)) {
				CMDQ_ERR(
					"translate[%u] fail: %d meta: (%u,%u,%#x,%#x,%#x)\n",
					i, status, metas[i].op,
					metas[i].engine, metas[i].offset,
					metas[i].value, metas[i].mask);
				break;
			}
		}
		remain_count -= copy_count;
		cur_src += copy_size;
	}

	kfree(metas);
	return status;
}

static s32 cmdq_mdp_handle_setup(struct mdp_submit *user_job,
				struct task_private *desc_private,
				struct cmdqRecStruct *handle)
{
	u32 iprop_size = sizeof(struct mdp_pmqos);

	handle->engineFlag = user_job->engine_flag;
	handle->pkt->priority = user_job->priority;
	handle->user_debug_str = NULL;

	if (desc_private)
		handle->node_private = desc_private->node_private_data;

	if (user_job->prop_size && user_job->prop_addr &&
		user_job->prop_size < CMDQ_MAX_USER_PROP_SIZE) {
		handle->prop_addr = kzalloc(iprop_size, GFP_KERNEL);
		handle->prop_size = iprop_size;
		if (copy_from_user(handle->prop_addr,
				CMDQ_U32_PTR(user_job->prop_addr),
				iprop_size)) {
			CMDQ_ERR("copy prop_addr from user fail\n");
			return -EINVAL;
		}
	} else {
		handle->prop_addr = NULL;
		handle->prop_size = 0;
	}

	return 0;
}

static int mdp_implement_read_v1(struct mdp_submit *user_job,
				struct cmdqRecStruct *handle,
				struct cmdq_command_buffer *cmd_buf)
{
	int status = 0;
	struct hw_meta *hw_metas = NULL;
	const u32 count = user_job->read_count_v1;
	u32 reg_addr, i;

	if (!count || !user_job->hw_metas_read_v1)
		return 0;

	CMDQ_MSG("%s: readback count: %u\n", __func__, count);
	hw_metas = kcalloc(count, sizeof(struct hw_meta), GFP_KERNEL);
	if (!hw_metas) {
		CMDQ_ERR("allocate hw_metas fail\n");
		return -ENOMEM;
	}

	/* collect user space dump request */
	if (copy_from_user(hw_metas, CMDQ_U32_PTR(user_job->hw_metas_read_v1),
			count * sizeof(struct hw_meta))) {
		CMDQ_ERR("copy hw_metas from user fail\n");
		kfree(hw_metas);
		return -EFAULT;
	}

	handle->user_reg_count = count;
	handle->user_token = 0;
	handle->reg_count = count;
	handle->reg_values = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
		count * sizeof(handle->reg_values[0]),
		&handle->reg_values_pa, GFP_KERNEL);
	if (!handle->reg_values) {
		CMDQ_ERR("allocate hw buffer fail\n");
		kfree(hw_metas);
		return -ENOMEM;
	}

	/* TODO v2: use reg_to_mem does not efficient due to event */
	/* insert commands to read back regs into slot */
	for (i = 0; i < count; i++) {
		reg_addr = cmdq_mdp_get_hw_reg(hw_metas[i].engine,
			hw_metas[i].offset);
		if (unlikely(!reg_addr)) {
			CMDQ_ERR("%s read:%d engine:%d offset:%#x addr:%#x\n",
				__func__, i, hw_metas[i].engine,
				hw_metas[i].offset, reg_addr);
			continue;
		}
		CMDQ_MSG("%s read:%d engine:%d offset:%#x addr:%#x\n",
			__func__, i, hw_metas[i].engine,
			hw_metas[i].offset, reg_addr);
		cmdq_op_read_reg_to_mem_ex(handle, cmd_buf,
			handle->reg_values_pa, i, reg_addr);
	}

	kfree(hw_metas);
	return status;
}

#define CMDQ_MAX_META_COUNT 0x100000

s32 mdp_ioctl_async_exec(struct file *pf, unsigned long param)
{
	struct mdp_submit user_job = {0};
	struct task_private desc_private = {0};
	struct cmdqRecStruct *handle = NULL;
	s32 status;
	u64 trans_cost = 0, exec_cost = sched_clock();
	struct cmdq_command_buffer cmd_buf;
	struct mdp_job_mapping *mapping_job = NULL;

	CMDQ_TRACE_FORCE_BEGIN("%s\n", __func__);

	mapping_job = kzalloc(sizeof(*mapping_job), GFP_KERNEL);
	if (!mapping_job) {
		status = -ENOMEM;
		goto done;
	}

	if (copy_from_user(&user_job, (void *)param, sizeof(user_job))) {
		CMDQ_ERR("copy mdp_submit from user fail\n");
		kfree(mapping_job);
		status = -EFAULT;
		goto done;
	}

	if (user_job.read_count_v1 > CMDQ_MAX_DUMP_REG_COUNT ||
		!user_job.meta_count ||
		user_job.meta_count > CMDQ_MAX_META_COUNT ||
		user_job.prop_size > CMDQ_MAX_USER_PROP_SIZE) {
		CMDQ_ERR("invalid read count:%u meta count:%u prop size:%u\n",
			user_job.read_count_v1,
			user_job.meta_count, user_job.prop_size);
		kfree(mapping_job);
		status = -EINVAL;
		goto done;
	}

	cmd_buf.va_base = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!cmd_buf.va_base) {
		CMDQ_ERR("%s allocate cmd_buf fail!\n", __func__);
		kfree(mapping_job);
		status = -ENOMEM;
		goto done;
	}
	cmd_buf.avail_buf_size = PAGE_SIZE;

	status = cmdq_mdp_handle_create(&handle);
	if (status < 0) {
		kfree(mapping_job);
		kfree(cmd_buf.va_base);
		goto done;
	}

	desc_private.node_private_data = pf->private_data;
	status = cmdq_mdp_handle_setup(&user_job, &desc_private, handle);
	if (status < 0) {
		CMDQ_ERR("%s setup fail:%d\n", __func__, status);
		cmdq_task_destroy(handle);
		kfree(mapping_job);
		kfree(cmd_buf.va_base);
		goto done;
	}

	/* setup secure data */
	status = cmdq_mdp_handle_sec_setup(&user_job.secData, handle);
	if (status < 0) {
		CMDQ_ERR("%s config sec fail:%d\n", __func__, status);
		cmdq_task_destroy(handle);
		kfree(mapping_job);
		kfree(cmd_buf.va_base);
		goto done;
	}

#if IS_ENABLED(CONFIG_MTK_CMDQ_MBOX_EXT)
	/* add perf begin for this handle->pkt while non-secure case */
	if (!handle->secData.is_secure)
		cmdq_pkt_perf_begin(handle->pkt);
#endif

	/* Make command from user job */
	CMDQ_TRACE_FORCE_BEGIN("mdp_translate_user_job\n");
	trans_cost = sched_clock();
	status = translate_user_job(&user_job, mapping_job, handle, &cmd_buf);
	trans_cost = div_s64(sched_clock() - trans_cost, 1000);
	CMDQ_TRACE_FORCE_END();

	if (status < 0) {
		CMDQ_ERR("%s translate fail:%d\n", __func__, status);
		cmdq_task_destroy(handle);
		kfree(mapping_job);
		kfree(cmd_buf.va_base);
		goto done;
	}

	status = mdp_implement_read_v1(&user_job, handle, &cmd_buf);
	if (status < 0) {
		CMDQ_ERR("%s read_v1 fail:%d\n", __func__, status);
		cmdq_task_destroy(handle);
		kfree(mapping_job);
		kfree(cmd_buf.va_base);
		goto done;
	}

	if (cmdq_handle_flush_cmd_buf(handle, &cmd_buf)) {
		cmdq_task_destroy(handle);
		kfree(mapping_job);
		kfree(cmd_buf.va_base);
		status = -EFAULT;
		goto done;
	}

	/* cmdq_pkt_dump_command(handle); */
	kfree(cmd_buf.va_base);
	/* flush */
	status = cmdq_mdp_handle_flush(handle);

	if (status < 0) {
		CMDQ_ERR("%s flush fail:%d\n", __func__, status);
		cmdq_task_destroy(handle);
		kfree(mapping_job);
		goto done;
	}

	INIT_LIST_HEAD(&mapping_job->list_entry);
	mutex_lock(&mdp_job_mapping_list_mutex);
	if (job_mapping_idx == 0)
		job_mapping_idx = 1;
	mapping_job->id = job_mapping_idx;
	user_job.job_id = job_mapping_idx;
	job_mapping_idx++;
	mapping_job->job = handle;
	mapping_job->node = pf->private_data;
	list_add_tail(&mapping_job->list_entry, &job_mapping_list);
	mutex_unlock(&mdp_job_mapping_list_mutex);

	if (copy_to_user((void *)param, &user_job, sizeof(user_job))) {
		CMDQ_ERR("CMDQ_IOCTL_ASYNC_EXEC copy_to_user failed\n");
		status = -EFAULT;
		goto done;
	}

done:
	CMDQ_TRACE_FORCE_END();

	exec_cost = div_u64(sched_clock() - exec_cost, 1000);
	if (exec_cost > 3000)
		CMDQ_LOG("[warn]%s job:%u cost translate:%lluus exec:%lluus\n",
			__func__, user_job.meta_count, trans_cost, exec_cost);
	else
		CMDQ_MSG("%s job:%u cost translate:%lluus exec:%lluus\n",
			__func__, user_job.meta_count, trans_cost, exec_cost);

	return status;
}

void mdp_check_pending_task(struct mdp_job_mapping *mapping_job)
{
	struct cmdqRecStruct *handle = mapping_job->job;
	u64 cost = div_u64(sched_clock() - handle->submit, 1000);
	u32 i;

	if (cost <= MDP_TASK_PAENDING_TIME_MAX)
		return;

	CMDQ_ERR(
		"%s waiting task cost time:%lluus submit:%llu enging:%#llx caller:%llu-%s\n",
		__func__,
		cost, handle->submit, handle->engineFlag,
		(u64)handle->caller_pid, handle->caller_name);

	/* call core to wait and release task in work queue */
	cmdq_pkt_auto_release_task(handle, true);

	list_del(&mapping_job->list_entry);
	for (i = 0; i < mapping_job->handle_count; i++)
		mdp_ion_free_dma_buf(mapping_job->dma_bufs[i],
			mapping_job->attaches[i], mapping_job->sgts[i]);
	kfree(mapping_job);
}

s32 mdp_ioctl_async_wait(unsigned long param)
{
	struct mdp_wait job_result;
	struct cmdqRecStruct *handle = NULL;
	/* backup value after task release */
	s32 status, i;
	u64 exec_cost = sched_clock();
	struct mdp_job_mapping *mapping_job = NULL, *tmp = NULL;

	CMDQ_TRACE_FORCE_BEGIN("%s\n", __func__);

	if (copy_from_user(&job_result, (void *)param, sizeof(job_result))) {
		CMDQ_ERR("copy_from_user job_result fail\n");
		status = -EFAULT;
		goto done;
	}

	/* verify job handle */
	mutex_lock(&mdp_job_mapping_list_mutex);
	list_for_each_entry_safe(mapping_job, tmp, &job_mapping_list,
		list_entry) {
		if (mapping_job->id == job_result.job_id) {
			handle = mapping_job->job;
			CMDQ_MSG("find handle:%p with id:%llx\n",
				handle, job_result.job_id);
			list_del(&mapping_job->list_entry);
			break;
		}

		mdp_check_pending_task(mapping_job);
	}
	mutex_unlock(&mdp_job_mapping_list_mutex);

	if (!handle) {
		CMDQ_ERR("job not exists:0x%016llx\n", job_result.job_id);
		status = -EFAULT;
		goto done;
	}

	do {
		/* wait for task done */
		status = cmdq_mdp_wait(handle, NULL);
		if (status < 0) {
			CMDQ_ERR("wait task result failed:%d handle:0x%p\n",
				status, handle);
			break;
		}

		/* check read_v1 */
		if (job_result.read_v1_result.count != handle->user_reg_count) {
			CMDQ_ERR("handle:0x%p wrong register buffer %u < %u\n",
				handle, job_result.read_v1_result.count,
				handle->user_reg_count);
			status = -ENOMEM;
			break;
		}

		/* copy read result v1 to user space */
		if (job_result.read_v1_result.ret_values && copy_to_user(
			CMDQ_U32_PTR(job_result.read_v1_result.ret_values),
			handle->reg_values,
			handle->user_reg_count * sizeof(u32))) {
			CMDQ_ERR("Copy REGVALUE to user space failed\n");
			status = -EFAULT;
			break;
		}

		/* copy read result to user space */
		status = mdp_process_read_request(&job_result.read_result);
	} while (0);
	exec_cost = div_u64(sched_clock() - exec_cost, 1000);
	if (exec_cost > 150000)
		CMDQ_LOG("[warn]job wait and close cost:%lluus handle:0x%p\n",
			exec_cost, handle);

	for (i = 0; i < mapping_job->handle_count; i++)
		mdp_ion_free_dma_buf(mapping_job->dma_bufs[i],
			mapping_job->attaches[i], mapping_job->sgts[i]);

	kfree(mapping_job);
	CMDQ_SYSTRACE_BEGIN("%s destroy\n", __func__);
	/* task now can release */
	cmdq_task_destroy(handle);
	CMDQ_SYSTRACE_END();

done:
	CMDQ_TRACE_FORCE_END();

	return status;
}


static s32 mdp_get_free_slots(u32 *rb_slot_index)
{
	u32 free_slot, free_slot_group;

	mutex_lock(&rb_slot_list_mutex);
	CMDQ_MSG("%s: start, rb_slot_vcp[0]:0x%llx, rb_slot_vcp[1]:0x%llx\n",
		__func__, rb_slot_vcp[0], rb_slot_vcp[1]);

	if (rb_slot_vcp[0] != ULLONG_MAX) {
		CMDQ_MSG("%s: group 0 is not full, use group 0\n", __func__);
		free_slot_group = 0;
	} else if (rb_slot_vcp[1] != ULLONG_MAX) {
		CMDQ_MSG("%s: group 1 is not full, use group 1\n", __func__);
		free_slot_group = 1;
	} else {
		CMDQ_ERR("%s: group 0 and group 1 are both full\n", __func__);
		mutex_unlock(&rb_slot_list_mutex);
		return -EFAULT;
	}

	/* find slot id */
	free_slot = ffz(rb_slot_vcp[free_slot_group]);
	*rb_slot_index = free_slot + free_slot_group * SLOT_GROUP_NUM;

	/* set rb_slot_vcp[free_slot_group] is used */
	rb_slot_vcp[free_slot_group] |= 1LL << free_slot;
	if (rb_slot_vcp[free_slot_group] == ULLONG_MAX)
		CMDQ_MSG("%s: group %d is full\n", __func__, free_slot_group);

	CMDQ_MSG("%s: return alloc_slot_index:%x, free_slot:%u, free_slot_group:%u\n",
		 __func__, *rb_slot_index, free_slot, free_slot_group);
	mutex_unlock(&rb_slot_list_mutex);

	return 0;

}

s32 mdp_ioctl_alloc_readback_slots(void *fp, unsigned long param)
{
	struct mdp_readback rb_req;
	dma_addr_t paStart = 0;
	s32 status;
	u32 free_slot, free_slot_group, alloc_slot_index;
	u64 exec_cost = sched_clock(), alloc;
	dma_addr_t vcp_iova_base;
	void *vcp_va_base;

	if (copy_from_user(&rb_req, (void *)param, sizeof(rb_req))) {
		CMDQ_ERR("%s copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	if (rb_req.count > MAX_COUNT_IN_RB_SLOT) {
		CMDQ_ERR("%s invalid count:%u\n", __func__, rb_req.count);
		return -EINVAL;
	}

	if (!cmdq_mdp_vcp_pq_readback_support()) {
		status = cmdq_alloc_write_addr(rb_req.count, &paStart,
			CMDQ_CLT_MDP, fp);
		if (status != 0) {
			CMDQ_ERR("%s alloc write address failed\n", __func__);
			return status;
		}
		alloc = div_u64(sched_clock() - exec_cost, 1000);

		mutex_lock(&rb_slot_list_mutex);
		free_slot_group = ffz(alloc_slot_group);
		if (unlikely(alloc_slot_group == ~0UL)) {
			CMDQ_ERR("%s no free slot:%#llx\n", __func__, alloc_slot_group);
			cmdq_free_write_addr(paStart, CMDQ_CLT_MDP);
			mutex_unlock(&rb_slot_list_mutex);
			return -ENOMEM;
		}
		/* find slot id */
		free_slot = ffz(alloc_slot[free_slot_group]);
		if (unlikely(alloc_slot[free_slot_group] == ~0UL)) {
			CMDQ_ERR("%s not found free slot in %u: %#llx\n", __func__,
				free_slot_group, alloc_slot[free_slot_group]);
			cmdq_free_write_addr(paStart, CMDQ_CLT_MDP);
			mutex_unlock(&rb_slot_list_mutex);
			return -EFAULT;
		}
		alloc_slot[free_slot_group] |= 1LL << free_slot;
		if (alloc_slot[free_slot_group] == ~0UL)
			alloc_slot_group |= 1LL << free_slot_group;

		alloc_slot_index = free_slot + free_slot_group * 64;

		rb_slot[alloc_slot_index].count = rb_req.count;
		rb_slot[alloc_slot_index].pa_start = paStart;
		rb_slot[alloc_slot_index].fp = fp;
		CMDQ_MSG("%s get 0x%pa in %d, fp:%p\n", __func__,
			&paStart, alloc_slot_index, fp);
		CMDQ_MSG("%s alloc slot[%d] %#llx, %#llx\n", __func__, free_slot_group,
			alloc_slot[free_slot_group], alloc_slot_group);
		mutex_unlock(&rb_slot_list_mutex);

		rb_req.start_id = alloc_slot_index << SLOT_ID_SHIFT;
		CMDQ_MSG("%s get 0x%08x\n", __func__, rb_req.start_id);

	} else {

		free_slot = 0;
		free_slot_group = 0;

		/* find vcp base addr (va/iova) */
		cmdq_vcp_enable(true);
		vcp_va_base = cmdq_get_vcp_buf(CMDQ_VCP_ENG_MDP_HDR0, &vcp_iova_base);
		vcp_paStart = vcp_iova_base;
		cmdq_vcp_enable(false);

		/* find slot id */
		status = mdp_get_free_slots(&alloc_slot_index);
		if (status != 0) {
			CMDQ_ERR("%s get free rb_slot failed\n", __func__);
			return status;
		}

		/* update paStart, and store to cmdq_ctx.writeAddrList */
		status = cmdqCoreWriteAddressVcpAlloc(rb_req.count, &paStart,
			CMDQ_CLT_MDP, fp, vcp_iova_base, vcp_va_base, alloc_slot_index);

		alloc = div_u64(sched_clock() - exec_cost, 1000);

		/* store to rb_slot[] */
		rb_slot[alloc_slot_index].count = rb_req.count;
		rb_slot[alloc_slot_index].pa_start = paStart;
		rb_slot[alloc_slot_index].fp = fp;

		rb_req.start_id = alloc_slot_index << SLOT_ID_SHIFT;

		CMDQ_LOG_PQ("%s: rb_slot[%d], fp:%p, start_id:0x%x => pa:%pa, vcp_paStart:%pa\n",
			__func__, alloc_slot_index, fp, rb_req.start_id, &paStart, &vcp_paStart);

	}

	if (copy_to_user((void *)param, &rb_req, sizeof(rb_req))) {
		CMDQ_ERR("%s copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	exec_cost = div_u64(sched_clock() - exec_cost, 1000);
	if (exec_cost > 10000)
		CMDQ_LOG("[warn]%s cost:%lluus (%lluus)\n",
			__func__, exec_cost, alloc);

	return 0;
}

s32 mdp_ioctl_free_readback_slots(void *fp, unsigned long param)
{
	struct mdp_readback free_req;
	u32 free_slot_index, free_slot_group, free_slot;
	dma_addr_t paStart = 0;

	CMDQ_MSG("%s\n", __func__);

	if (copy_from_user(&free_req, (void *)param, sizeof(free_req))) {
		CMDQ_ERR("%s copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	free_slot_index = free_req.start_id >> SLOT_ID_SHIFT;
	if (unlikely(free_slot_index >= MAX_RB_SLOT_NUM)) {
		CMDQ_ERR("%s wrong:%x, start:%x\n", __func__,
			free_slot_index, free_req.start_id);
		return -EINVAL;
	}

	if (!cmdq_mdp_vcp_pq_readback_support()) {
		mutex_lock(&rb_slot_list_mutex);
		free_slot_group = free_slot_index >> 6;
		free_slot = free_slot_index & 0x3f;
		if (free_slot_group >= SLOT_GROUP_NUM) {
			mutex_unlock(&rb_slot_list_mutex);
			CMDQ_ERR("%s invalid group:%x\n", __func__, free_slot_group);
			return -EINVAL;
		}
		if (!(alloc_slot[free_slot_group] & (1LL << free_slot))) {
			mutex_unlock(&rb_slot_list_mutex);
			CMDQ_ERR("%s %d not in group[%d]:%llx\n", __func__,
				free_req.start_id, free_slot_group,
				alloc_slot[free_slot_group]);
			return -EINVAL;
		}
		if (rb_slot[free_slot_index].fp != fp) {
			mutex_unlock(&rb_slot_list_mutex);
			CMDQ_ERR("%s fp %p different:%p\n", __func__,
				fp, rb_slot[free_slot_index].fp);
			return -EINVAL;
		}
		alloc_slot[free_slot_group] &= ~(1LL << free_slot);
		if (alloc_slot[free_slot_group] != ~0UL)
			alloc_slot_group &= ~(1LL << free_slot_group);

		paStart = rb_slot[free_slot_index].pa_start;

		rb_slot[free_slot_index].count = 0;
		rb_slot[free_slot_index].pa_start = 0;
		CMDQ_MSG("%s free 0x%pa in %d, fp:%p\n", __func__,
			&paStart, free_slot_index, rb_slot[free_slot_index].fp);
		rb_slot[free_slot_index].fp = NULL;
		CMDQ_MSG("%s alloc slot[%d] %#llx, %#llx\n", __func__, free_slot_group,
			alloc_slot[free_slot_group], alloc_slot_group);
		mutex_unlock(&rb_slot_list_mutex);

		return cmdq_free_write_addr(paStart, CMDQ_CLT_MDP);

	} else {

		mutex_lock(&rb_slot_list_mutex);

		if (free_slot_index < 64) {
			free_slot_group = 0;
			free_slot = free_slot_index;
		} else if (free_slot_index < 128) {
			free_slot_group = 1;
			free_slot = free_slot_index - 64;
		} else {
			mutex_unlock(&rb_slot_list_mutex);
			CMDQ_ERR("%s: wrong:%x, start:%x\n", __func__,
				free_slot_index, free_req.start_id);
			return -EINVAL;
		}

		if (!(rb_slot_vcp[free_slot_group] & (1LL << free_slot))) {
			mutex_unlock(&rb_slot_list_mutex);
			CMDQ_ERR("%s: %d not in group[%d]:%llx\n", __func__,
				free_slot_index, free_slot_group,
				rb_slot_vcp[free_slot_group]);
			return -EINVAL;
		}

		if (rb_slot[free_slot_index].fp != fp) {
			mutex_unlock(&rb_slot_list_mutex);
			CMDQ_ERR("%s: free slot %d, fp:%p different:%p\n", __func__,
				free_slot_index, fp, rb_slot[free_slot_index].fp);
			return -EINVAL;
		}

		/* set rb_slot_vcp[free_slot_group] is un-used */
		rb_slot_vcp[free_slot_group] &= ~(1LL << free_slot);

		paStart = rb_slot[free_slot_index].pa_start;

		CMDQ_LOG_PQ("%s: rb_slot[%d], fp:%p, start_id:0x%x => pa:%pa\n",
			__func__, free_slot_index, rb_slot[free_slot_index].fp,
			free_req.start_id, &paStart);

		rb_slot[free_slot_index].count = 0;
		rb_slot[free_slot_index].pa_start = 0;
		rb_slot[free_slot_index].fp = NULL;
		mutex_unlock(&rb_slot_list_mutex);

		return cmdqCoreWriteAddressVcpFree(paStart, CMDQ_CLT_MDP);
	}

}

s32 mdp_ioctl_read_readback_slots(unsigned long param)
{
	struct mdp_read_readback read_req;

	CMDQ_MSG("%s\n", __func__);
	if (copy_from_user(&read_req, (void *)param, sizeof(read_req))) {
		CMDQ_ERR("%s copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	return mdp_process_read_request(&read_req);
}

#ifdef MDP_COMMAND_SIMULATE
s32 mdp_ioctl_simulate(unsigned long param)
{
#ifdef MDP_META_IN_LEGACY_V2
	CMDQ_LOG("%s not support\n", __func__);
	return -EFAULT;
#else
	struct mdp_simulate user_job;
	struct mdp_submit submit = {0};
	struct mdp_job_mapping *mapping_job = NULL;
	struct cmdq_command_buffer cmd_buf = {0};
	struct cmdqRecStruct *handle = NULL;
	struct cmdq_pkt_buffer *buf;
	u8 *result_buffer = NULL;
	s32 status = 0;
	u32 size, result_size = 0;
	u64 exec_cost;

	if (copy_from_user(&user_job, (void *)param, sizeof(user_job))) {
		CMDQ_ERR("%s copy mdp_simulate from user fail\n", __func__);
		status = -EFAULT;
		goto done;
	}

	if (user_job.command_size > CMDQ_MAX_SIMULATE_COMMAND_SIZE) {
		CMDQ_ERR("%s simulate command is too much\n", __func__);
		status = -EFAULT;
		goto done;
	}

	submit.metas = user_job.metas;
	submit.meta_count = user_job.meta_count;

	mapping_job = kzalloc(sizeof(*mapping_job), GFP_KERNEL);
	if (!mapping_job) {
		status = -ENOMEM;
		goto done;
	}

	result_buffer = vzalloc(user_job.command_size);
	if (!result_buffer) {
		CMDQ_ERR("%s unable to alloc necessary cmd buffer\n", __func__);
		status = -ENOMEM;
		goto done;
	}

	cmd_buf.va_base = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!cmd_buf.va_base) {
		CMDQ_ERR("%s allocate cmd_buf fail!\n", __func__);
		status = -ENOMEM;
		goto done;
	}
	cmd_buf.avail_buf_size = PAGE_SIZE;

	status = cmdq_mdp_handle_create(&handle);
	if (status < 0)
		goto done;

	/* Make command from user job */
	exec_cost = sched_clock();
	status = translate_user_job(&submit, mapping_job, handle, &cmd_buf);
	if (cmdq_handle_flush_cmd_buf(handle, &cmd_buf)) {
		CMDQ_ERR("%s do flush final cmd fail\n", __func__);
		status = -EFAULT;
		goto done;
	}
	exec_cost = div_u64(sched_clock() - exec_cost, 1000);
	CMDQ_LOG("simulate translate job[%d] cost:%lluus\n",
		user_job.meta_count, exec_cost);

	list_for_each_entry(buf, &handle->pkt->buf, list_entry) {
		if (list_is_last(&buf->list_entry, &handle->pkt->buf))
			size = CMDQ_CMD_BUFFER_SIZE -
				handle->pkt->avail_buf_size;
		else
			/* CMDQ_INST_SIZE for skip jump */
			size = CMDQ_CMD_BUFFER_SIZE - CMDQ_INST_SIZE;

		if (result_size + size > user_job.command_size)
			size = user_job.command_size - result_size;

		memcpy(result_buffer + result_size, buf->va_base, size);
		result_size += size;
		if (result_size >= user_job.command_size) {
			CMDQ_ERR("instruction buf size not enough %u < %lu\n",
				result_size, handle->pkt->cmd_buf_size);
			break;
		}
	}
	CMDQ_LOG("simulate instruction size:%u\n", result_size);

	if (!user_job.commands ||
		copy_to_user((void *)(unsigned long)user_job.commands,
		result_buffer, result_size)) {
		CMDQ_ERR("%s fail to copy instructions to user\n", __func__);
		status = -EINVAL;
		goto done;
	}

	if (user_job.result_size &&
		copy_to_user((void *)(unsigned long)user_job.result_size,
		&result_size, sizeof(u32))) {
		CMDQ_ERR("%s fail to copy result size to user\n", __func__);
		status = -EINVAL;
		goto done;
	}

	CMDQ_LOG("%s done\n", __func__);

done:
	kfree(mapping_job);
	kfree(cmd_buf.va_base);
	vfree(result_buffer);
	if (handle)
		cmdq_task_destroy(handle);
	return status;
#endif
}
#endif

void mdp_ioctl_free_job_by_node(void *node)
{
	uint32_t i;
	struct mdp_job_mapping *mapping_job = NULL, *tmp = NULL;

	/* verify job handle */
	mutex_lock(&mdp_job_mapping_list_mutex);
	list_for_each_entry_safe(mapping_job, tmp, &job_mapping_list,
		list_entry) {
		if (mapping_job->node != node)
			continue;

		CMDQ_LOG("[warn] %s job task handle %p\n",
			__func__, mapping_job->job);

		list_del(&mapping_job->list_entry);
		for (i = 0; i < mapping_job->handle_count; i++)
			mdp_ion_free_dma_buf(mapping_job->dma_bufs[i],
				mapping_job->attaches[i], mapping_job->sgts[i]);
		kfree(mapping_job);
	}
	mutex_unlock(&mdp_job_mapping_list_mutex);
}

void mdp_ioctl_free_readback_slots_by_node(void *fp)
{
	u32 i, free_slot_group = 0, free_slot = 0;
	dma_addr_t paStart = 0;
	u32 count = 0;

	CMDQ_MSG("%s, node:%p\n", __func__, fp);

	mutex_lock(&rb_slot_list_mutex);

	if (!cmdq_mdp_vcp_pq_readback_support()) {
		for (i = 0; i < ARRAY_SIZE(rb_slot); i++) {
			if (rb_slot[i].fp != fp)
				continue;

			free_slot_group = i >> 6;
			free_slot = i & 0x3f;
			alloc_slot[free_slot_group] &= ~(1LL << free_slot);
			if (alloc_slot[free_slot_group] != ~0UL)
				alloc_slot_group &= ~(1LL << free_slot_group);
			paStart = rb_slot[i].pa_start;
			rb_slot[i].count = 0;
			rb_slot[i].pa_start = 0;
			rb_slot[i].fp = NULL;
			CMDQ_MSG("%s free %pa in %u alloc slot[%d] %#llx, %#llx\n",
				__func__, &paStart, i, free_slot_group,
				alloc_slot[free_slot_group], alloc_slot_group);
			cmdq_free_write_addr(paStart, CMDQ_CLT_MDP);
			count++;
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(rb_slot); i++) {
			if (rb_slot[i].fp != fp)
				continue;

			if (i < 64) {
				free_slot_group = 0;
				free_slot = i;
			} else if (i < 128) {
				free_slot_group = 1;
				free_slot = i - 64;
			}

			/* set rb_slot_vcp[free_slot_group] is un-used */
			rb_slot_vcp[free_slot_group] &= ~(1LL << free_slot);

			paStart = rb_slot[i].pa_start;
			rb_slot[i].count = 0;
			rb_slot[i].pa_start = 0;
			rb_slot[i].fp = NULL;
			CMDQ_LOG_PQ("%s free %pa in %u alloc slot[%d] %#llx\n",
				__func__, &paStart, i, free_slot_group,
				rb_slot_vcp[free_slot_group]);

			cmdqCoreWriteAddressVcpFree(paStart, CMDQ_CLT_MDP);
			count++;
		}
	}
	mutex_unlock(&rb_slot_list_mutex);

	CMDQ_LOG("%s free %u slot group by node %p\n", __func__, count, fp);
}

int mdp_limit_dev_create(struct platform_device *device)
{
	INIT_LIST_HEAD(&job_mapping_list);

	return 0;
}

static int mdpsys_con_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	u32 dma_mask_bit = 0;
	s32 ret;

	CMDQ_LOG("%s\n", __func__);

	ret = of_property_read_u32(dev->of_node, "dma_mask_bit",
		&dma_mask_bit);
	/* if not assign from dts, give default */
	if (ret != 0 || !dma_mask_bit)
		dma_mask_bit = MDP_DEFAULT_MASK_BITS;
	ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(dma_mask_bit));
	CMDQ_LOG("%s set dma mask bit:%u result:%d\n",
		__func__, dma_mask_bit, ret);

	mdpsys_con_ctx.dev = dev;

	CMDQ_LOG("%s done\n", __func__);

	return 0;
}

static int mdpsys_con_remove(struct platform_device *pdev)
{
	CMDQ_LOG("%s\n", __func__);

	mdpsys_con_ctx.dev = NULL;

	CMDQ_LOG("%s done\n", __func__);

	return 0;
}

static const struct of_device_id mdpsyscon_of_ids[] = {
	{.compatible = "mediatek,mdpsys_config",},
	{}
};

static struct platform_driver mdpsyscon = {
	.probe = mdpsys_con_probe,
	.remove = mdpsys_con_remove,
	.driver = {
		.name = "mtk_mdpsys_con",
		.owner = THIS_MODULE,
		.pm = NULL,
		.of_match_table = mdpsyscon_of_ids,
	}
};

void mdpsyscon_init(void)
{
	int status;

	status = platform_driver_register(&mdpsyscon);
	if (status != 0) {
		CMDQ_ERR("Failed to register the CMDQ driver(%d)\n", status);
		return;
	}
}

void mdpsyscon_deinit(void)
{
	platform_driver_unregister(&mdpsyscon);
}

MODULE_LICENSE("GPL");
