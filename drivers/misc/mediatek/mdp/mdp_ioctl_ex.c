// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/sched/clock.h>

#ifdef CONFIG_MTK_CMDQ_MBOX_EXT
#include "mdp_def.h"
#include "mdp_common.h"
#include "mdp_driver.h"
#include "mdp_cmdq_helper_ext.h"
#include "mdp_cmdq_record.h"
#include "mdp_cmdq_device.h"
#else
#include "cmdq_def.h"
#include "cmdq_mdp_common.h"
#include "cmdq_driver.h"
#include "cmdq_record.h"
#ifndef MDP_META_IN_LEGACY_V2
#include "cmdq_helper_ext.h"
#else
#include "cmdq_core.h"
#endif
#include "cmdq_device.h"
#endif
#include "mdp_def_ex.h"
#include "mdp_ioctl_ex.h"
#include "cmdq_struct.h"
#include "mdp_m4u.h"

#ifdef MDP_M4U_TEE_SUPPORT
static atomic_t m4u_init = ATOMIC_INIT(0);
#endif
#ifdef MDP_M4U_MTEE_SUPPORT
static atomic_t m4u_gz_init = ATOMIC_INIT(0);
#endif

static int mdp_limit_open(struct inode *pInode, struct file *pFile)
{
	struct cmdqFileNodeStruct *pNode;

	CMDQ_VERBOSE("mdp limit driver open fd=%p begin\n", pFile);

	pFile->private_data = kzalloc(sizeof(struct cmdqFileNodeStruct),
		GFP_KERNEL);
	if (!pFile->private_data) {
		CMDQ_ERR("Can't allocate memory for mdp limit file node\n");
		return -ENOMEM;
	}

	pNode = (struct cmdqFileNodeStruct *)pFile->private_data;
	pNode->userPID = current->pid;
	pNode->userTGID = current->tgid;

	INIT_LIST_HEAD(&(pNode->taskList));
	spin_lock_init(&pNode->nodeLock);

	CMDQ_VERBOSE("mdp limit driver open end\n");

	return 0;
}


static int mdp_limit_release(struct inode *pInode, struct file *pFile)
{
	struct cmdqFileNodeStruct *pNode;
	unsigned long flags;

	CMDQ_VERBOSE("mdp limit driver release fd=%p begin\n", pFile);

	pNode = (struct cmdqFileNodeStruct *)pFile->private_data;

	if (!pNode) {
		CMDQ_ERR("mdp limit file node NULL\n");
		return -EFAULT;
	}

	spin_lock_irqsave(&pNode->nodeLock, flags);

	/* note that we did not release CMDQ tasks
	 * issued by this file node,
	 * since their HW operation may be pending.
	 */

	spin_unlock_irqrestore(&pNode->nodeLock, flags);

	/* scan through tasks that created by
	 * this file node and release them
	 */
	cmdq_mdp_release_task_by_file_node((void *)pNode);

	kfree(pFile->private_data);
	pFile->private_data = NULL;

	cmdq_free_write_addr_by_node(CMDQ_CLT_MDP, pFile);

	CMDQ_VERBOSE("CMDQ driver release end\n");

	return 0;
}

#define MAX_HANDLE_NUM 10

static u64 job_mapping_idx = 1;
static struct list_head job_mapping_list;
struct mdp_job_mapping {
	u64 id;
	struct cmdqRecStruct *job;
	struct list_head list_entry;
	struct ion_handle *handles[MAX_HANDLE_NUM];
	int fds[MAX_HANDLE_NUM];
	u32 mvas[MAX_HANDLE_NUM];
	u32 handle_count;
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

static s32 mdp_process_read_request(struct mdp_read_readback *req_user)
{
	/* create kernel-space buffer for working */
	u32 *ids = NULL;
	u32 *addrs = NULL;
	u32 *values = NULL;
	void *ids_user;
	void *values_user;
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

		addrs = kcalloc(count, sizeof(u32), GFP_KERNEL);
		if (!ids) {
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

static unsigned long translate_fd(struct op_meta *meta,
				struct mdp_job_mapping *mapping_job)
{
	struct ion_handle *ion_h;
	unsigned long mva = 0;
	u32 i;
	u32 port = cmdq_mdp_get_hw_port(meta->engine);

	if (!port)
		return 0;

	for (i = 0; i < mapping_job->handle_count; i++)
		if (mapping_job->fds[i] == meta->fd)
			break;

	if (i == mapping_job->handle_count) {
		if (i >= MAX_HANDLE_NUM) {
			CMDQ_ERR("%s no more handle room\n", __func__);
			return 0;
		}
		/* need to map ion handle and mva */
		ion_h = mdp_ion_import_handle(meta->fd);
		if (!ion_h)
			return 0;

		mdp_ion_get_mva(ion_h, &mva, 0, port);
		if (mva) {
			mapping_job->fds[i] = meta->fd;
			mapping_job->handles[i] = ion_h;
			mapping_job->mvas[i] = mva;
			mapping_job->handle_count++;
		} else {
			mdp_ion_free_handle(ion_h);
			return 0;
		}
	} else {
		mva = mapping_job->mvas[i];
	}
	mva += meta->fd_offset;

	return mva;
}

#ifdef TRACK_PERF_MON
static u64 trans_perf_mon_cost[CMDQ_MOP_NOP] = {0LL};
static u32 trans_perf_mon_count[CMDQ_MOP_NOP] = {0};
#endif

static s32 translate_meta(struct op_meta *meta,
			struct mdp_job_mapping *mapping_job,
			struct cmdqRecStruct *handle)
{
	u32 reg_addr;
	s32 status = 0;
	u64 exec_cost = sched_clock();

	switch (meta->op) {
	case CMDQ_MOP_WRITE_FD:
	{
		unsigned long mva = translate_fd(meta, mapping_job);

		reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
		if (!reg_addr || !mva)
			return -EINVAL;
		status = cmdq_op_write_reg_ex(handle, reg_addr, mva, ~0);
		break;
	}
	case CMDQ_MOP_WRITE:
		reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
		if (!reg_addr)
			return -EINVAL;
		status = cmdq_op_write_reg_ex(handle, reg_addr,
					meta->value, meta->mask);
		break;
	case CMDQ_MOP_READ:
	{
		u32 offset;
		dma_addr_t dram_addr;

		reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
		dram_addr = translate_read_id_ex(meta->readback_id, &offset);
		if (!reg_addr || !dram_addr)
			return -EINVAL;
		status = cmdq_op_read_reg_to_mem(handle,
						dram_addr, offset, reg_addr);
		break;
	}
	case CMDQ_MOP_POLL:
		reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
		if (!reg_addr)
			return -EINVAL;
		status = cmdq_op_poll(handle, reg_addr,
					meta->value, meta->mask);
		break;
	case CMDQ_MOP_WAIT:
		status = cmdq_op_wait(handle, meta->event);
		break;
	case CMDQ_MOP_WAIT_NO_CLEAR:
		status = cmdq_op_wait_no_clear(handle, meta->event);
		break;
	case CMDQ_MOP_CLEAR:
		status = cmdq_op_clear_event(handle, meta->event);
		break;
	case CMDQ_MOP_SET:
		status = cmdq_op_set_event(handle, meta->event);
		break;
	case CMDQ_MOP_ACQUIRE:
		status = cmdq_op_acquire(handle, meta->event);
		break;
	case CMDQ_MOP_WRITE_FROM_REG:
	{
		u32 from_reg = cmdq_mdp_get_hw_reg(meta->from_engine,
						meta->from_offset);

		reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
		if (!reg_addr || !from_reg)
			return -EINVAL;
		status = cmdq_op_write_from_reg(handle, reg_addr, from_reg);
		break;
	}
	case CMDQ_MOP_WRITE_SEC:
	{
		reg_addr = cmdq_mdp_get_hw_reg(meta->engine, meta->offset);
		if (!reg_addr)
			return -EINVAL;
		status = cmdq_op_write_reg_ex(handle, reg_addr,
					meta->value, ~0);
		if (!status)
			status = cmdq_mdp_update_sec_addr_index(handle,
				meta->sec_handle, meta->sec_index,
				cmdq_mdp_handle_get_instr_count(handle) - 1);
		break;
	}
	case CMDQ_MOP_NOP:
		break;
	default:
		CMDQ_ERR("invalid meta op:%u\n", meta->op);
		status = -EINVAL;
		break;
	}

	exec_cost = sched_clock() - exec_cost;
#ifdef TRACK_PERF_MON
	trans_perf_mon_count[meta->op]++;
	trans_perf_mon_cost[meta->op] += exec_cost;
#endif
	return status;
}

static s32 translate_user_job(struct mdp_submit *user_job,
			struct mdp_job_mapping *mapping_job,
			struct cmdqRecStruct *handle)
{
	struct op_meta *metas;
	s32 status = 0;
	u32 i, copy_size, copy_count, remain_count;
	u64 exec_cost;
	void *cur_src = CMDQ_U32_PTR(user_job->metas);
	const u32 meta_count_in_page = PAGE_SIZE / sizeof(struct op_meta);

	exec_cost = sched_clock();
	metas = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!metas) {
		CMDQ_ERR("allocate metas fail\n");
		return -ENOMEM;
	}
	exec_cost = div_s64(sched_clock() - exec_cost, 1000);
	CMDQ_MSG("[log]%s kmalloc cost:%lluus\n",
		__func__, exec_cost);

	cmdq_mdp_meta_replace_sec_addr(metas, user_job, handle);
#ifdef TRACK_PERF_MON
	memset(trans_perf_mon_cost, 0, sizeof(u64)*CMDQ_MOP_NOP);
	memset(trans_perf_mon_count, 0, sizeof(u32)*CMDQ_MOP_NOP);
#endif

	remain_count = user_job->meta_count;
	while (remain_count > 0) {
		copy_count = min_t(u32, remain_count, meta_count_in_page);
		copy_size = copy_count * sizeof(struct op_meta);
		exec_cost = sched_clock();
		if (copy_from_user(metas, cur_src, copy_size)) {
			CMDQ_ERR("copy metas from user fail:%u\n", copy_size);
			kfree(metas);
			return -EINVAL;
		}
		exec_cost = div_s64(sched_clock() - exec_cost, 1000);
		CMDQ_MSG("[log]%s copy from user cost:%lluus\n",
			__func__, exec_cost);

		cmdq_mdp_meta_replace_sec_addr(metas, user_job, handle);
		for (i = 0; i < copy_count; i++) {
			CMDQ_MSG("translate meta[%u] (%u,%u,%#x,%#x,%#x)\n", i,
				metas[i].op, metas[i].engine, metas[i].offset,
				metas[i].value, metas[i].mask);
			status = translate_meta(&metas[i], mapping_job, handle);
			if (unlikely(status < 0)) {
				CMDQ_ERR("translate[%u] fail: %d\n", i, status);
				break;
			}
		}
		remain_count -= copy_count;
		cur_src += copy_size;
	}

#ifdef TRACK_PERF_MON
	for (i = 0; i < ARRAY_SIZE(trans_perf_mon_count); i++) {
		if (trans_perf_mon_count[i] > 0)
			CMDQ_LOG("[TM] id:%d: total:%u, Sum:%lluus\n",
				i, trans_perf_mon_count[i],
				div_s64(trans_perf_mon_cost[i], 1000));
	}
#endif
	kfree(metas);
	return status;
}

static s32 cmdq_mdp_handle_setup(struct mdp_submit *user_job,
				struct task_private *desc_private,
				struct cmdqRecStruct *handle)
{
#ifndef MDP_META_IN_LEGACY_V2
	const u64 inorder_mask = 1ll << CMDQ_ENG_INORDER;

	handle->engineFlag = user_job->engine_flag & ~inorder_mask;
	handle->pkt->priority = user_job->priority;
	handle->user_debug_str = NULL;

	if (user_job->engine_flag & inorder_mask)
		handle->force_inorder = true;

	if (desc_private)
		handle->node_private = desc_private->node_private_data;

	if (user_job->prop_size && user_job->prop_addr &&
		user_job->prop_size < CMDQ_MAX_USER_PROP_SIZE) {
		handle->prop_addr = kzalloc(user_job->prop_size, GFP_KERNEL);
		handle->prop_size = user_job->prop_size;
		if (copy_from_user(handle->prop_addr,
				CMDQ_U32_PTR(user_job->prop_addr),
				user_job->prop_size)) {
			CMDQ_ERR("copy prop_addr from user fail\n");
			return -EINVAL;
		}
	} else {
		handle->prop_addr = NULL;
		handle->prop_size = 0;
	}
#else
	handle->engineFlag = user_job->engine_flag;
	handle->priority = user_job->priority;
#endif
	return 0;
}

static int mdp_implement_read_v1(struct mdp_submit *user_job,
				struct cmdqRecStruct *handle)
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
		CMDQ_MSG("%s: read[%d] engine[%d], offset[%x], addr[%x]\n",
			__func__, i, hw_metas[i].engine,
			hw_metas[i].offset, reg_addr);
		cmdq_op_read_reg_to_mem(handle, handle->reg_values_pa,
					i, reg_addr);
	}

	kfree(hw_metas);
	return status;
}

#define CMDQ_MAX_META_COUNT 0x100000

s32 mdp_ioctl_async_exec(struct file *pf, unsigned long param)
{
	struct mdp_submit user_job;
	struct task_private desc_private = {0};
	struct cmdqRecStruct *handle = NULL;
	s32 status;
	u64 exec_cost;

	struct mdp_job_mapping *mapping_job = NULL;

	mapping_job = kzalloc(sizeof(*mapping_job), GFP_KERNEL);
	if (!mapping_job)
		return -ENOMEM;

	if (copy_from_user(&user_job, (void *)param, sizeof(user_job))) {
		CMDQ_ERR("copy mdp_submit from user fail\n");
		kfree(mapping_job);
		return -EFAULT;
	}

	if (user_job.read_count_v1 > CMDQ_MAX_DUMP_REG_COUNT ||
		!user_job.meta_count ||
		user_job.meta_count > CMDQ_MAX_META_COUNT ||
		user_job.prop_size > CMDQ_MAX_USER_PROP_SIZE) {
		CMDQ_ERR("invalid read count:%u meta count:%u prop size:%u\n",
			user_job.read_count_v1,
			user_job.meta_count, user_job.prop_size);
		kfree(mapping_job);
		return -EINVAL;
	}

	status = cmdq_mdp_handle_create(&handle);
	if (status < 0) {
		kfree(mapping_job);
		return status;
	}

	status = cmdq_mdp_handle_setup(&user_job, &desc_private, handle);
	if (status < 0) {
		CMDQ_ERR("%s setup fail:%d\n", __func__, status);
		cmdq_task_destroy(handle);
		kfree(mapping_job);
		return status;
	}

#ifdef MDP_M4U_TEE_SUPPORT
	if (atomic_cmpxchg(&m4u_init, 0, 1) == 0) {
		m4u_sec_init();
		CMDQ_LOG("[SEC] m4u_sec_init is called\n");
	}
#endif
#ifdef MDP_M4U_MTEE_SUPPORT
	if (atomic_cmpxchg(&m4u_gz_init, 0, 1) == 0) {
		// 0: SEC_ID_SEC_CAM
		m4u_gz_sec_init(0);
		CMDQ_LOG("[SEC] m4u_gz_sec_init is called\n");
	}
#endif

	/* setup secure data */
	status = cmdq_mdp_handle_sec_setup(&user_job.secData, handle);
	if (status < 0) {
		CMDQ_ERR("%s config sec fail:%d\n", __func__, status);
		cmdq_task_destroy(handle);
		kfree(mapping_job);
		return status;
	}

	/* Make command from user job */
	exec_cost = sched_clock();
	status = translate_user_job(&user_job, mapping_job, handle);
	exec_cost = div_s64(sched_clock() - exec_cost, 1000);
	if (exec_cost > 3000) {
		CMDQ_ERR("[warn]translate job[%d] cost:%lluus\n",
			user_job.meta_count, exec_cost);
	} else {
		CMDQ_MSG("[log]translate job[%d] cost:%lluus\n",
			user_job.meta_count, exec_cost);
	}
	if (status < 0) {
		CMDQ_ERR("%s translate fail:%d\n", __func__, status);
		cmdq_task_destroy(handle);
		kfree(mapping_job);
		return status;
	}

	status = mdp_implement_read_v1(&user_job, handle);
	if (status < 0) {
		CMDQ_ERR("%s read_v1 fail:%d\n", __func__, status);
		cmdq_task_destroy(handle);
		kfree(mapping_job);
		return status;
	}

	/* cmdq_pkt_dump_command(handle); */
	/* flush */
	status = cmdq_mdp_handle_flush(handle);

	if (status < 0) {
		CMDQ_ERR("%s flush fail:%d\n", __func__, status);
		if (handle) {
#ifndef MDP_META_IN_LEGACY_V2
			if (handle->thread != CMDQ_INVALID_THREAD)
				cmdq_mdp_unlock_thread(handle);
#endif
			cmdq_task_destroy(handle);
		}

		kfree(mapping_job);
		return status;
	}

	INIT_LIST_HEAD(&mapping_job->list_entry);
	mutex_lock(&mdp_job_mapping_list_mutex);
	if (job_mapping_idx == 0)
		job_mapping_idx = 1;
	mapping_job->id = job_mapping_idx;
	user_job.job_id = job_mapping_idx;
	job_mapping_idx++;
	mapping_job->job = handle;
	list_add_tail(&mapping_job->list_entry, &job_mapping_list);
	mutex_unlock(&mdp_job_mapping_list_mutex);

	if (copy_to_user((void *)param, &user_job, sizeof(user_job))) {
		CMDQ_ERR("CMDQ_IOCTL_ASYNC_EXEC copy_to_user failed\n");
		return -EFAULT;
	}

	return 0;
}

s32 mdp_ioctl_async_wait(unsigned long param)
{
	struct mdp_wait job_result;
	struct cmdqRecStruct *handle = NULL;
	/* backup value after task release */
	s32 status, i;
	u64 exec_cost = sched_clock();
	struct mdp_job_mapping *mapping_job = NULL, *tmp = NULL;

	if (copy_from_user(&job_result, (void *)param, sizeof(job_result))) {
		CMDQ_ERR("copy_from_user job_result fail\n");
		return -EFAULT;
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
	}
	mutex_unlock(&mdp_job_mapping_list_mutex);

	if (!handle) {
		CMDQ_ERR("job not exists:0x%016llx\n", job_result.job_id);
		return -EFAULT;
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
		if (copy_to_user(
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
	exec_cost = div_s64(sched_clock() - exec_cost, 1000);
	if (exec_cost > 150000)
		CMDQ_LOG("[warn]job wait and close cost:%lluus handle:0x%p\n",
			exec_cost, handle);

	for (i = 0; i < mapping_job->handle_count; i++)
		mdp_ion_free_handle(mapping_job->handles[i]);

	kfree(mapping_job);
	CMDQ_SYSTRACE_BEGIN("%s destroy\n", __func__);
	/* task now can release */
	cmdq_task_destroy(handle);
	CMDQ_SYSTRACE_END();

	return status;
}

s32 mdp_ioctl_alloc_readback_slots(void *fp, unsigned long param)
{
	struct mdp_readback rb_req;
	dma_addr_t paStart = 0;
	s32 status;
	u32 free_slot, free_slot_group, alloc_slot_index;

	if (copy_from_user(&rb_req, (void *)param, sizeof(rb_req))) {
		CMDQ_ERR("%s copy_from_user failed\n", __func__);
		return -EFAULT;
	}

	if (rb_req.count > MAX_COUNT_IN_RB_SLOT) {
		CMDQ_ERR("%s invalid count:%u\n", __func__, rb_req.count);
		return -EINVAL;
	}

	status = cmdq_alloc_write_addr(rb_req.count, &paStart,
		CMDQ_CLT_MDP, fp);
	if (status != 0) {
		CMDQ_ERR("%s alloc write address failed\n", __func__);
		return status;
	}

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

	if (copy_to_user((void *)param, &rb_req, sizeof(rb_req))) {
		CMDQ_ERR("%s copy_to_user failed\n", __func__);
		return -EFAULT;
	}

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

void mdp_ioctl_free_readback_slots_by_node(void *fp)
{
	u32 i, free_slot_group, free_slot;
	dma_addr_t paStart = 0;

	CMDQ_MSG("%s, node:%p\n", __func__, fp);

	mutex_lock(&rb_slot_list_mutex);
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
		CMDQ_MSG("%s free 0x%pa in %d\n", __func__, &paStart, i);
		CMDQ_MSG("%s alloc slot[%d] %#llx, %#llx\n", __func__,
			free_slot_group,
			alloc_slot[free_slot_group], alloc_slot_group);
		cmdq_free_write_addr(paStart, CMDQ_CLT_MDP);
	}
	mutex_unlock(&rb_slot_list_mutex);
}

static long mdp_limit_ioctl(struct file *pf, unsigned int code,
	unsigned long param)
{
	s32 status = 0;

	CMDQ_VERBOSE("%s code:0x%08x f:0x%p\n", __func__, code, pf);

	switch (code) {
	case CMDQ_IOCTL_QUERY_USAGE:
		CMDQ_MSG("ioctl CMDQ_IOCTL_QUERY_USAGE\n");
		status = cmdq_driver_ioctl_query_usage(pf, param);
		break;
	case CMDQ_IOCTL_QUERY_CAP_BITS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_QUERY_CAP_BITS\n");
		status = cmdq_driver_ioctl_query_cap_bits(param);
		break;
	case CMDQ_IOCTL_QUERY_DTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_QUERY_DTS\n");
		status = cmdq_driver_ioctl_query_dts(param);
		break;
	case CMDQ_IOCTL_NOTIFY_ENGINE:
		CMDQ_MSG("ioctl CMDQ_IOCTL_NOTIFY_ENGINE\n");
		status = cmdq_driver_ioctl_notify_engine(param);
		break;
	case CMDQ_IOCTL_ASYNC_EXEC:
		CMDQ_MSG("ioctl CMDQ_IOCTL_ASYNC_EXEC\n");
		CMDQ_SYSTRACE_BEGIN("%s_async_exec\n", __func__);
		status = mdp_ioctl_async_exec(pf, param);
		CMDQ_SYSTRACE_END();
		break;
	case CMDQ_IOCTL_ASYNC_WAIT:
		CMDQ_MSG("ioctl CMDQ_IOCTL_ASYNC_WAIT\n");
		CMDQ_SYSTRACE_BEGIN("%s_async_wait\n", __func__);
		status = mdp_ioctl_async_wait(param);
		CMDQ_SYSTRACE_END();
		break;
	case CMDQ_IOCTL_ALLOC_READBACK_SLOTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_ALLOC_READBACK_SLOTS\n");
		status = mdp_ioctl_alloc_readback_slots(pf, param);
		break;
	case CMDQ_IOCTL_FREE_READBACK_SLOTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_FREE_READBACK_SLOTS\n");
		status = mdp_ioctl_free_readback_slots(pf, param);
		break;
	case CMDQ_IOCTL_READ_READBACK_SLOTS:
		CMDQ_MSG("ioctl CMDQ_IOCTL_READ_READBACK_SLOTS\n");
		status = mdp_ioctl_read_readback_slots(param);
		break;
	default:
		CMDQ_ERR("unrecognized ioctl 0x%08x\n", code);
		return -ENOIOCTLCMD;
	}

	if (status < 0)
		CMDQ_ERR("ioctl return fail:%d\n", status);

	return status;
}

#ifdef CONFIG_COMPAT
static long mdp_limit_ioctl_compat(struct file *pFile, unsigned int code,
	unsigned long param)
{
	switch (code) {
	case CMDQ_IOCTL_QUERY_USAGE:
	case CMDQ_IOCTL_QUERY_CAP_BITS:
	case CMDQ_IOCTL_QUERY_DTS:
	case CMDQ_IOCTL_NOTIFY_ENGINE:
	case CMDQ_IOCTL_ASYNC_EXEC:
	case CMDQ_IOCTL_ASYNC_WAIT:
	case CMDQ_IOCTL_ALLOC_READBACK_SLOTS:
	case CMDQ_IOCTL_FREE_READBACK_SLOTS:
	case CMDQ_IOCTL_READ_READBACK_SLOTS:
		/* All ioctl structures should be the same size in
		 * 32-bit and 64-bit linux.
		 */
		return mdp_limit_ioctl(pFile, code, param);
	case CMDQ_IOCTL_LOCK_MUTEX:
	case CMDQ_IOCTL_UNLOCK_MUTEX:
		CMDQ_ERR("[COMPAT]deprecated ioctl 0x%08x\n", code);
		return -ENOIOCTLCMD;
	default:
		CMDQ_ERR("[COMPAT]unrecognized ioctl 0x%08x\n", code);
		return -ENOIOCTLCMD;
	}

	CMDQ_ERR("[COMPAT]unrecognized ioctl 0x%08x\n", code);
	return -ENOIOCTLCMD;
}
#endif

static const struct file_operations mdp_op_ex = {
	.owner = THIS_MODULE,
	.open = mdp_limit_open,
	.release = mdp_limit_release,
	.unlocked_ioctl = mdp_limit_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mdp_limit_ioctl_compat,
#endif
};

static struct cdev *mdp_limit_cdev;
#ifdef MDP_LIMIT_NODE
static dev_t mdp_limit_dev_no;
static struct class *mdp_limit_class;
#endif

int mdp_limit_late_init(void)
{
	mdp_ion_create("MDP");

	return 0;
}

int mdp_limit_dev_create(struct platform_device *device)
{
	int status = 0;

	mdp_limit_cdev = cdev_alloc();
	mdp_limit_cdev->owner = THIS_MODULE;
	mdp_limit_cdev->ops = &mdp_op_ex;

#ifdef MDP_LIMIT_NODE
	status = alloc_chrdev_region(&mdp_limit_dev_no, 0, 1,
		MDP_LIMIT_DRIVER_DEVICE_NAME);
	if (status != 0)
		CMDQ_ERR("Get MDP device major number(%d) failed(%d)\n",
			mdp_limit_dev_no, status);
	else
		CMDQ_MSG("Get MDP device major number(%d) success(%d)\n",
			mdp_limit_dev_no, status);

	status = cdev_add(mdp_limit_cdev, mdp_limit_dev_no, 1);

	mdp_limit_class = class_create(THIS_MODULE,
		MDP_LIMIT_DRIVER_DEVICE_NAME);
	device_create(mdp_limit_class, NULL, mdp_limit_dev_no, NULL,
		MDP_LIMIT_DRIVER_DEVICE_NAME);
#endif
	INIT_LIST_HEAD(&job_mapping_list);

	CMDQ_LOG("MDP limit dev create end\n");

	return status;
}

void mdp_limit_dev_destroy(void)
{
#ifdef MDP_LIMIT_NODE
	device_destroy(mdp_limit_class, mdp_limit_dev_no);
	class_destroy(mdp_limit_class);
	unregister_chrdev_region(mdp_limit_dev_no, 1);
#endif
	cdev_del(mdp_limit_cdev);
	mdp_limit_cdev = NULL;
	mdp_ion_destroy();
}
