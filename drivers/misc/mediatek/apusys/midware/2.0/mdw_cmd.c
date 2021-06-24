// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sync_file.h>

#include "mdw_cmn.h"
#include "mdw_mem.h"

#define mdw_cmd_set_state(c, s) \
	do { \
		mutex_lock(&c->mtx); \
		mdw_flw_debug("cmd(%p.%d) state(%d->%d)\n", \
			c->mpriv, c->id, c->state, s); \
		c->state = s; \
		mutex_unlock(&c->mtx); \
	} while (0)

#define mdw_cmd_show(c) \
	mdw_cmd_debug("cmd(%p.%d/0x%llx/0x%llx)param(%u/%u/%u/%u)" \
	"subcmds(%u/%p/%u/%u)pid(%d/%d/%d)\n", \
	c->mpriv, c->id, c->uid, c->kid, \
	c->priority, c->hardlimit, c->softlimit, c->power_save, \
	c->num_subcmds, c->cmdbufs, c->num_cmdbufs, c->size_cmdbufs, \
	c->pid, c->tgid, current->pid)

static struct dma_buf *mdw_cmd_get_mem(struct mdw_fpriv *mpriv, uint64_t handle)
{
	struct dma_buf *dbuf = NULL;
	void * vaddr = NULL;

	dbuf = dma_buf_get(handle);
	if (IS_ERR_OR_NULL(dbuf)) {
		mdw_drv_err("get mem fail(%llu)\n", handle);
		return NULL;
	}

	vaddr = dma_buf_vmap(dbuf);
	mdw_cmd_debug("u(%p) get cmdbuf(%p/%llu)(%p/%p)\n", mpriv, dbuf, handle, vaddr, dbuf->vmap_ptr);

	return dbuf;
}

static int mdw_cmd_put_mem(struct mdw_fpriv *mpriv, struct dma_buf *dbuf)
{
	dma_buf_vunmap(dbuf, dbuf->vmap_ptr);
	dma_buf_put(dbuf);
	return 0;
}

static void mdw_cmd_put_cmdbufs(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	unsigned int i = 0, j = 0;

	if (!c->cmdbufs)
		return;

	for (i = 0; i < c->num_subcmds; i++) {
		ksubcmd = &c->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			if (ksubcmd->ori_mems[j]) {
				if (mdw_cmd_put_mem(mpriv,
					ksubcmd->ori_mems[j]))
					mdw_drv_warn("cmd(%p.%d) put cmdbuf(%llu) fail\n",
					mpriv, c->id,
					ksubcmd->cmdbufs[j].handle);
				ksubcmd->ori_mems[j] = NULL;
			}
		}
	}
	mdw_mem_unmap(mpriv, c->cmdbufs);
	mdw_mem_free(mpriv, c->cmdbufs);
	c->cmdbufs = NULL;
}

static int mdw_cmd_get_cmdbufs(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	unsigned int i = 0, j = 0, ofs = 0;
	int ret = -EINVAL;
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	struct dma_buf *dbuf = NULL;

	if (!c->size_cmdbufs || c->cmdbufs)
		goto out;

	/* alloc cmdbuf by dmabuf */
	c->cmdbufs = mdw_mem_alloc(mpriv, c->size_cmdbufs, MDW_DEFAULT_ALIGN,
		(1ULL << MDW_MEM_IOCTL_ALLOC_CACHEABLE), MDW_MEM_TYPE_INTERNAL);
	if (!c->cmdbufs) {
		mdw_drv_err("cmd(%p.%d) alloc buffer for duplicate fail\n",
			mpriv, c->id);
		ret = -ENOMEM;
		goto out;
	}
	ret = mdw_mem_map(mpriv, c->cmdbufs);
	if (ret) {
		mdw_drv_err("cmd(%p.%d) map buffer for duplicate fail\n",
			mpriv, c->id);
		goto free_cmdbufs;
	}

	mdw_cmd_debug("alloc internal cmdbuf(%p)\n", c->cmdbufs);
	for (i = 0; i < c->num_subcmds; i++) {
		mdw_cmd_debug("sc(%u) #cmdbufs(%u)\n",
			i, c->ksubcmds[i].info->num_cmdbufs);

		ksubcmd = &c->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			/* calc align */
			if (ksubcmd->cmdbufs[j].align)
				ofs = MDW_ALIGN(ofs, ksubcmd->cmdbufs[j].align);
			else
				ofs = MDW_ALIGN(ofs, MDW_DEFAULT_ALIGN);

			mdw_cmd_debug("sc(%u) #cmdbufs(%u) offset(%u)\n",
				i, j, ofs);

			/* get mem from handle */
			dbuf = mdw_cmd_get_mem(mpriv, ksubcmd->cmdbufs[j].handle);
			if (IS_ERR_OR_NULL(dbuf)) {
				mdw_drv_err("get cmdbuf(%u-%u)(%llu) fail\n",
					i, j, ksubcmd->cmdbufs[j].handle);
				goto free_cmdbufs;
			}
			/* check mem boundary */
			if (dbuf->vmap_ptr == NULL ||
				ksubcmd->cmdbufs[j].size > dbuf->size) {
				mdw_drv_err("cmdbuf(%u-%u) range invalid(%p/%u/%u)\n",
					i, j, dbuf->vmap_ptr,
					ksubcmd->cmdbufs[j].size,
					dbuf->size);
				goto free_cmdbufs;
			}

			/* duplicate cmdbuf */
			memcpy(c->cmdbufs->vaddr + ofs, dbuf->vmap_ptr,
			ksubcmd->cmdbufs[j].size);

			/* record buffer info */
			ksubcmd->ori_mems[j] = dbuf;
			ksubcmd->kvaddrs[j] =
				(uint64_t)(c->cmdbufs->vaddr + ofs);
			ksubcmd->daddrs[j] =
				(uint64_t)(c->cmdbufs->device_va + ofs);
			ofs += ksubcmd->cmdbufs[j].size;
			mdw_cmd_debug("cmdbufs(%u-#%u) (0x%llx/0x%llx)\n",
				i, j, ksubcmd->kvaddrs[j], ksubcmd->daddrs[j]);
		}
	}

	ret = 0;
	goto out;

free_cmdbufs:
	mdw_cmd_put_cmdbufs(mpriv, c);
out:
	mdw_cmd_debug("ret(%d)\n", ret);
	return ret;
}

static int mdw_cmd_duplicate_cmdbuf(struct mdw_subcmd_kinfo *in,
	bool before_exec)
{
	int i = 0, ret = -ENOMEM;

	for (i = 0; i < in->info->num_cmdbufs; i++) {
		if (!in->kvaddrs[i] || in->ori_mems[i]->vmap_ptr == NULL)
			goto out;

		if (before_exec == true) {
			if (in->cmdbufs[i].direction == MDW_CB_OUT)
				continue;
			memcpy((void *)in->kvaddrs[i],
				(void *)in->ori_mems[i]->vmap_ptr,
				in->cmdbufs[i].size);
			/* TODO, flush */
			mdw_cmd_debug("copy in(0x%llx/0x%llx)(0x%x)\n",
				in->kvaddrs[i],
				(uint64_t)in->ori_mems[i]->vmap_ptr,
				in->cmdbufs[i].size);
		} else {
			if (in->cmdbufs[i].direction == MDW_CB_IN)
				continue;
			/* TODO, invalidate */
			memcpy((void *)in->ori_mems[i]->vmap_ptr,
				(void *)in->kvaddrs[i],
				in->cmdbufs[i].size);
			mdw_cmd_debug("copy out(0x%llx/0x%llx)(0x%x)\n",
				(uint64_t)in->ori_mems[i]->vmap_ptr,
				in->kvaddrs[i],
				in->cmdbufs[i].size);
		}
	}

	ret = 0;
out:
	if (ret)
		mdw_drv_err("duplicate cmdbuf(%u) fail\n", i);
	return ret;
}

static unsigned int mdw_cmd_create_infos(struct mdw_fpriv *mpriv,
	struct mdw_cmd *c)
{
	unsigned int i = 0, j = 0, total_size = 0;
	int ret = 0;

	for (i = 0; i < c->num_subcmds; i++) {
		c->ksubcmds[i].info = &c->subcmds[i];
		mdw_cmd_debug("subcmd(%u)(%u/%u/%u/%u/%u/%u/%u/%u/0x%llx)\n",
			i, c->subcmds[i].type,
			c->subcmds[i].suggest_time, c->subcmds[i].vlm_usage,
			c->subcmds[i].vlm_ctx_id, c->subcmds[i].vlm_force,
			c->subcmds[i].boost, c->subcmds[i].pack_id,
			c->subcmds[i].num_cmdbufs, c->subcmds[i].cmdbufs);

		/* kva for oroginal buffer */
		c->ksubcmds[i].ori_mems = kvzalloc(c->subcmds[i].num_cmdbufs *
			sizeof(c->ksubcmds[i].ori_mems), GFP_KERNEL);
		if (!c->ksubcmds[i].ori_mems)
			goto free_cmdbufs;

		/* record kva for duplicate */
		c->ksubcmds[i].kvaddrs = kvzalloc(c->subcmds[i].num_cmdbufs *
			sizeof(*c->ksubcmds[i].kvaddrs), GFP_KERNEL);
		if (!c->ksubcmds[i].kvaddrs)
			goto free_cmdbufs;

		c->ksubcmds[i].daddrs = kvzalloc(c->subcmds[i].num_cmdbufs *
			sizeof(*c->ksubcmds[i].daddrs), GFP_KERNEL);
		if (!c->ksubcmds[i].daddrs)
			goto free_cmdbufs;


		/* allocate for subcmd cmdbuf */
		c->ksubcmds[i].cmdbufs = kvzalloc(c->subcmds[i].num_cmdbufs *
			sizeof(*c->ksubcmds[i].cmdbufs), GFP_KERNEL);
		if (!c->ksubcmds[i].cmdbufs)
			goto free_cmdbufs;

		/* copy cmdbuf info */
		if (copy_from_user(c->ksubcmds[i].cmdbufs,
			(void __user *)c->subcmds[i].cmdbufs,
			c->subcmds[i].num_cmdbufs *
			sizeof(*c->ksubcmds[i].cmdbufs))) {
			goto free_cmdbufs;
		}

		/* accumulate cmdbuf size with alignment */
		for (j = 0; j < c->subcmds[i].num_cmdbufs; j++) {
			c->num_cmdbufs += c->subcmds[i].num_cmdbufs;
			if (c->ksubcmds[i].cmdbufs[j].align) {
				total_size = MDW_ALIGN(total_size,
					c->ksubcmds[i].cmdbufs[j].align) +
					c->ksubcmds[i].cmdbufs[j].size;
			} else {
				total_size += c->ksubcmds[i].cmdbufs[j].size;
			}
		}
	}
	c->size_cmdbufs = total_size;

	mdw_cmd_debug("cmdbuf num(%u) size(%u)\n",
		c->num_cmdbufs, c->size_cmdbufs);

	ret = mdw_cmd_get_cmdbufs(mpriv, c);
	if (ret)
		goto free_cmdbufs;

	goto out;

free_cmdbufs:
	for (i = 0; i < c->num_subcmds; i++) {
		/* free dvaddrs */
		if (c->ksubcmds[i].daddrs) {
			kvfree(c->ksubcmds[i].daddrs);
			c->ksubcmds[i].daddrs = NULL;
		}
		/* free kvaddrs */
		if (c->ksubcmds[i].kvaddrs) {
			kvfree(c->ksubcmds[i].kvaddrs);
			c->ksubcmds[i].kvaddrs = NULL;
		}
		/* free ori kvas */
		if (c->ksubcmds[i].ori_mems) {
			kvfree(c->ksubcmds[i].ori_mems);
			c->ksubcmds[i].ori_mems = NULL;
		}
		/* free cmdbufs */
		if (c->ksubcmds[i].cmdbufs) {
			kvfree(c->ksubcmds[i].cmdbufs);
			c->ksubcmds[i].cmdbufs = NULL;
		}
	}

	ret = -EINVAL;
out:
	return ret;
}

static void mdw_cmd_delete_infos(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	unsigned int i = 0;

	mdw_cmd_put_cmdbufs(mpriv, c);

	for (i = 0; i < c->num_subcmds; i++) {
		/* free dvaddrs */
		if (c->ksubcmds[i].daddrs) {
			kvfree(c->ksubcmds[i].daddrs);
			c->ksubcmds[i].daddrs = NULL;
		}
		/* free kvaddrs */
		if (c->ksubcmds[i].kvaddrs) {
			kvfree(c->ksubcmds[i].kvaddrs);
			c->ksubcmds[i].kvaddrs = NULL;
		}
		/* free ori kvas */
		if (c->ksubcmds[i].ori_mems) {
			kvfree(c->ksubcmds[i].ori_mems);
			c->ksubcmds[i].ori_mems = NULL;
		}
		/* free cmdbufs */
		if (c->ksubcmds[i].cmdbufs) {
			kvfree(c->ksubcmds[i].cmdbufs);
			c->ksubcmds[i].cmdbufs = NULL;
		}
	}
}

static int mdw_cmd_sanity_check(struct mdw_cmd *c)
{
	unsigned int i = 0;

	/* cmd info */
	if (c->priority >= MDW_PRIORITY_MAX) {
		mdw_drv_err("cmd invalid (0x%llx/%p.%d)(%u)\n",
			c->uid, c->mpriv, c->id, c->priority);
		return -EINVAL;
	}

	/* subcmd info */
	for (i = 0; i < c->num_subcmds; i++) {
		if (c->subcmds[i].type >= MDW_DEV_MAX ||
			c->subcmds[i].vlm_ctx_id >= MDW_SUBCMD_MAX ||
			c->subcmds[i].boost > MDW_BOOST_MAX ||
			c->subcmds[i].pack_id >= MDW_SUBCMD_MAX) {
			mdw_drv_err("subcmd(%u) invalid (%u/%u/%u/%u)\n",
				i, c->subcmds[i].type,
				c->subcmds[i].boost,
				c->subcmds[i].pack_id);
			return -EINVAL;
		}
	}

	return 0;
}

static int mdw_cmd_postset(struct mdw_cmd *c)
{
	int i = 0, ret = 0;

	/* cmdbuf handle */
	for (i = 0; i < c->num_subcmds; i++) {
		ret = mdw_cmd_duplicate_cmdbuf(&c->ksubcmds[i], false);
		if (ret) {
			mdw_drv_err("duplicate subcmd(%u) fail\n", i);
			goto out;
		}
	}
out:
	mdw_cmd_show(c);
	return ret;
}

static void mdw_cmd_release(struct kref *ref)
{
	struct mdw_cmd *c =
		container_of(ref, struct mdw_cmd, ref);

	mdw_cmd_show(c);
	/* free mdw_cmd */
	mdw_cmd_delete_infos(c->mpriv, c);
	vfree(c->ksubcmds);
	vfree(c->adj_matrix);
	vfree(c->subcmds);
	vfree(c);
}

//--------------------------------------------
static const char *mdw_fence_get_driver_name(struct dma_fence *fence)
{
	return "apu_mdw";
}

static const char *mdw_fence_get_timeline_name(struct dma_fence *fence)
{
	struct mdw_fence *f =
		container_of(fence, struct mdw_fence, base_fence);

	return dev_name(f->mdev->misc_dev.this_device);
}

static bool mdw_fence_enable_signaling(struct dma_fence *fence)
{
	return true;
}

static void mdw_fence_release(struct dma_fence *fence)
{
	struct mdw_fence *mf =
		container_of(fence, struct mdw_fence, base_fence);
	struct mdw_cmd *c = mf->priv;

	//mdw_cmd_set_state(c, MDW_CMD_STATE_IDLE);
	mdw_cmd_show(c);
	kfree(mf);
	kref_put(&c->ref, mdw_cmd_release);
}

static const struct dma_fence_ops mdw_fence_ops = {
	.get_driver_name =  mdw_fence_get_driver_name,
	.get_timeline_name =  mdw_fence_get_timeline_name,
	.enable_signaling =  mdw_fence_enable_signaling,
	.wait = dma_fence_default_wait,
	.release =  mdw_fence_release
};

//--------------------------------------------
static int mdw_fence_init(struct mdw_cmd *c)
{
	int ret = 0;

	c->fence = kzalloc(sizeof(*c->fence), GFP_KERNEL);
	if (!c->fence)
		return -ENOMEM;

	dma_fence_init(&c->fence->base_fence, &mdw_fence_ops,
		&c->fence_lock, 0, 0);
	c->fence->priv = c;
	mdw_cmd_show(c);

	return ret;
}

static int mdw_cmd_preset(struct mdw_cmd *c, struct mdw_cmd_in *in)
{
	int i = 0, ret = 0;

	if (c->state != MDW_CMD_STATE_IDLE) {
		mdw_drv_err("cmd(%p.%d) in wrong state(%d)\n",
			c->mpriv, c->id, c->state);
		ret = -EBUSY;
		goto out;
	}

	c->priority = in->exec.priority;
	c->hardlimit = in->exec.hardlimit;
	c->softlimit = in->exec.softlimit;
	c->power_save = in->exec.power_save;
	mdw_cmd_show(c);

	/* cmdbuf handle */
	for (i = 0; i < c->num_subcmds; i++) {
		ret = mdw_cmd_duplicate_cmdbuf(&c->ksubcmds[i], true);
		if (ret) {
			mdw_drv_err("duplicate subcmd(%u) fail\n", i);
			goto out;
		}
	}

	ret = mdw_fence_init(c);
	if (ret) {
		mdw_drv_err("cmd(%p.%d) ini fence fail\n");
		goto out;
	}

	mdw_cmd_set_state(c, MDW_CMD_STATE_WAIT);

out:
	return ret;
}

static int mdw_cmd_run(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_device *mdev = mpriv->mdev;
	int ret = 0;

	mdw_cmd_set_state(c, MDW_CMD_STATE_RUN);
	mdw_cmd_show(c);
	ret = mdev->dev_funcs->run_cmd(mpriv, c);
	if (ret) {
		mdw_drv_err("run cmd(%p.%d) fail(%d)\n",
			c->mpriv, c->id, ret);
		mdw_cmd_set_state(c, MDW_CMD_STATE_IDLE);

		dma_fence_set_error(&c->fence->base_fence, ret);
		dma_fence_signal(&c->fence->base_fence);
		dma_fence_put(&c->fence->base_fence);
	} else {
		mdw_flw_debug("cmd(%p.%d) run\n",
			c->mpriv, c->id);
	}

	return ret;
}

static int mdw_cmd_complete(struct mdw_cmd *c, int ret)
{
	struct dma_fence *f = &c->fence->base_fence;

	mdw_drv_debug("cmd(%p.%d) ret(%d) complete, pid(%d/%d)(%d)\n",
		c->mpriv, c->id, ret, c->pid, c->tgid, current->pid);
	mdw_cmd_postset(c);
	mdw_cmd_set_state(c, MDW_CMD_STATE_IDLE);

	dma_fence_signal(f);
	dma_fence_put(f);
	return 0;
}

static void mdw_cmd_trigger_func(struct work_struct *wk)
{
	struct mdw_cmd *c =
		container_of(wk, struct mdw_cmd, t_wk);

	if (c->wait_fence) {
		dma_fence_wait(c->wait_fence, false);
		dma_fence_put(c->wait_fence);
	}

	mdw_flw_debug("cmd(%p.%d) wait fence done, start run\n",
		c->mpriv, c->id);
	mdw_cmd_run(c->mpriv, c);
}

void mdw_cmd_mpriv_release(struct mdw_fpriv *mpriv)
{
	struct mdw_cmd *c = NULL;
	int id = 0;

	idr_for_each_entry(&mpriv->cmds_idr, c, id) {
		idr_remove(&mpriv->cmds_idr, id);
		mdw_drv_warn("remove residual cmd(%p.%d)\n", mpriv, c->id);
		kref_put(&c->ref, mdw_cmd_release);
	}
}

static int mdw_cmd_ioctl_create(struct mdw_fpriv *mpriv,
	union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;
	int ret = 0;
	unsigned int i = 0;

	if (in->build.num_subcmds > MDW_SUBCMD_MAX) {
		mdw_drv_err("too much subcmds(%u)\n", in->build.num_subcmds);
		return -EINVAL;
	}

	c = vzalloc(sizeof(*c));
	if (!c)
		return -ENOMEM;

	/* setup cmd info */
	c->pid = current->pid;
	c->tgid = current->tgid;
	c->kid = (uint64_t)c;
	c->uid = in->build.uid;
	c->usr_id = in->build.usr_id;
	c->priority = in->build.priority;
	c->hardlimit = in->build.hardlimit;
	c->softlimit = in->build.softlimit;
	c->power_save = in->build.power_save;
	c->num_subcmds = in->build.num_subcmds;
	c->subcmds = vzalloc(c->num_subcmds * sizeof(*c->subcmds));
	if (!c->subcmds) {
		ret = -ENOMEM;
		goto free_cmd;
	}

	/* copy adj matrix */
	c->adj_matrix = vzalloc(c->num_subcmds *
		c->num_subcmds * sizeof(uint8_t));
	if (copy_from_user(c->adj_matrix, (void __user *)in->build.adj_matrix,
		(c->num_subcmds * c->num_subcmds * sizeof(uint8_t)))) {
		mdw_drv_err("copy adj matrix fail\n");
		goto free_subcmds;
	}

	for (i = 0; i < c->num_subcmds * c->num_subcmds; i++)
		mdw_cmd_debug("adj_matrix(%u) = %u\n", i, c->adj_matrix[i]);

	/* copy subcmd headers info */
	if (copy_from_user(c->subcmds, (void __user *)in->build.subcmd_infos,
		c->num_subcmds * sizeof(*c->subcmds))) {
		mdw_drv_err("copy subcmds fail\n");
		ret = -EFAULT;
		goto free_adj;
	}

	c->ksubcmds = vzalloc(c->num_subcmds * sizeof(*c->ksubcmds));
	if (!c->ksubcmds) {
		ret = -ENOMEM;
		goto free_adj;
	}

	mutex_lock(&mpriv->mtx);
	ret = mdw_cmd_create_infos(mpriv, c);
	mutex_unlock(&mpriv->mtx);
	if (ret)
		goto free_ksubcmds;

	ret = mdw_cmd_sanity_check(c);
	if (ret)
		goto delete_infos;

	mutex_init(&c->mtx);
	spin_lock_init(&c->fence_lock);
	c->mpriv = mpriv;
	c->complete = mdw_cmd_complete;
	c->state = MDW_CMD_STATE_IDLE;
	INIT_WORK(&c->t_wk, &mdw_cmd_trigger_func);
	mdw_cmd_show(c);

	/* gen cmd id */
	mutex_lock(&mpriv->mtx);
	c->id = idr_alloc(&mpriv->cmds_idr, c, 1, MDW_CMD_MAX, GFP_KERNEL);
	mutex_unlock(&mpriv->mtx);
	if (c->id < 0)
		goto delete_infos;

	kref_init(&c->ref);

	/* setup return value */
	memset(args, 0, sizeof(*args));
	args->out.build.id = c->id;

	goto out;

delete_infos:
	mdw_cmd_delete_infos(mpriv, c);
free_ksubcmds:
	vfree(c->ksubcmds);
free_adj:
	vfree(c->adj_matrix);
free_subcmds:
	vfree(c->subcmds);
free_cmd:
	vfree(c);
out:
	return ret;
}

static int mdw_cmd_ioctl_delete(struct mdw_fpriv *mpriv,
	union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;
	int ret = 0, cmd_id = 0;

	mutex_lock(&mpriv->mtx);
	cmd_id = in->release.id;
	memset(args, 0, sizeof(*args));

	/* get mdw_cmd */
	c = idr_find(&mpriv->cmds_idr, cmd_id);
	if (!c) {
		mdw_drv_err("can't find cmd(%p.%d)\n", mpriv, cmd_id);
		ret = -EINVAL;
		goto unlock_mpriv;
	}
	mdw_cmd_show(c);

	/* remove from idr */
	idr_remove(&mpriv->cmds_idr, cmd_id);
	kref_put(&c->ref, mdw_cmd_release);
	mutex_unlock(&mpriv->mtx);

	goto out;

unlock_mpriv:
	mutex_unlock(&mpriv->mtx);
out:
	return ret;
}

static int mdw_cmd_ioctl_run(struct mdw_fpriv *mpriv, union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;
	struct sync_file *sync_file = NULL;
	int ret = 0, fd = 0, wait_fd = 0, cmd_id = 0;

	mutex_lock(&mpriv->mtx);
	cmd_id = in->exec.id;
	wait_fd = in->exec.fence;
	memset(args, 0, sizeof(*args));

	/* get mdw_cmd */
	c = idr_find(&mpriv->cmds_idr, cmd_id);
	if (!c) {
		mdw_drv_err("can't find cmd(%p.%d)\n", mpriv, cmd_id);
		ret = -EINVAL;
		goto out;
	}

	/* pre setup cmd */
	kref_get(&c->ref);
	ret = mdw_cmd_preset(c, in);
	if (ret) {
		mdw_drv_err("cmd(%p.%d) preset fail(%d)\n",
			c->mpriv, c->id, ret);
		goto put_cmd;
	}

	/* get sync_file fd */
	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		mdw_drv_err("get unused fd fail\n");
		ret = -EINVAL;
		goto put_cmd;
	}
	sync_file = sync_file_create(&c->fence->base_fence);
	if (!sync_file) {
		mdw_drv_err("create sync file fail\n");
		ret = -ENOMEM;
		goto put_file;
	}

	/* check wait fence from other module */
	mdw_flw_debug("cmd(%p.%d) wait fence(%d)\n",
		c->mpriv, c->id, wait_fd);
	c->wait_fence = sync_file_get_fence(wait_fd);
	if (!c->wait_fence) {
		mdw_flw_debug("cmd(%p.%d) no wait fence, trigger directly\n",
			c->mpriv, c->id);
		ret = mdw_cmd_run(mpriv, c);
	} else {
		schedule_work(&c->t_wk);
	}

	if (ret)
		goto put_file;

	/* assign fd */
	fd_install(fd, sync_file->file);
	args->out.exec.fence = fd;
	mdw_flw_debug("async fd(%d)\n", fd);
	goto out;

put_file:
	put_unused_fd(fd);
put_cmd:
	kref_put(&c->ref, mdw_cmd_release);
out:
	mutex_unlock(&mpriv->mtx);
	return ret;
}

int mdw_cmd_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_cmd_args *args = (union mdw_cmd_args *)data;
	int ret = 0;

	mdw_flw_debug("op:%d\n", args->in.op);

	switch (args->in.op) {
	case MDW_CMD_IOCTL_BUILD:
		ret = mdw_cmd_ioctl_create(mpriv, args);
		break;

	case MDW_CMD_IOCTL_RELEASE:
		ret = mdw_cmd_ioctl_delete(mpriv, args);
		break;

	case MDW_CMD_IOCTL_RUN:
		ret = mdw_cmd_ioctl_run(mpriv, args);
		break;

	default:
		ret = -EINVAL;
		break;
	}
	mdw_flw_debug("done\n");

	return ret;
}
