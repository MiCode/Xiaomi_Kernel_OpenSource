// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sync_file.h>

#include "mdw_cmn.h"

#define mdw_cmd_set_state(c, s) \
	do { \
		mutex_lock(&c->mtx); \
		mdw_flw_debug("cmd(%p.%d) state(%d->%d)\n", \
			c->mpriv, c->id, c->state, s); \
		c->state = s; \
		mutex_unlock(&c->mtx); \
	} while (0)

static void mdw_cmd_put_cmdbufs(struct mdw_fpriv *mpriv, struct mdw_cmd *cmd)
{
	if (!cmd->cmdbufs)
		return;

	/* ref for map */
	mdw_mem_free(mpriv, cmd->cmdbufs);
	/* ref for alloc */
	mdw_mem_free(mpriv, cmd->cmdbufs);
	cmd->cmdbufs = NULL;
}

static int mdw_cmd_get_cmdbufs(struct mdw_fpriv *mpriv, struct mdw_cmd *cmd)
{
	unsigned int i = 0, j = 0, ofs = 0;
	int ret = 0;
	struct mdw_subcmd_kinfo *ksubcmd = NULL;

	if (!cmd->size_cmdbufs || cmd->cmdbufs)
		return -EINVAL;

	/* alloc cmdbuf by dmabuf */
	cmd->cmdbufs = mdw_mem_alloc(mpriv, cmd->size_cmdbufs,
		MDW_DEFAULT_ALIGN, true);
	if (!cmd->cmdbufs)
		return -ENOMEM;
	if (mdw_mem_map(mpriv, cmd->cmdbufs))
		goto free_cmdbufs;

	/* TODO, should ref++ for output */
	for (i = 0; i < cmd->num_subcmds; i++) {
		mdw_cmd_debug("sc(%u) #cmdbufs(%u)\n",
			i, cmd->ksubcmds[i].info->num_cmdbufs);

		ksubcmd = &cmd->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			/* calc align */
			if (ksubcmd->cmdbufs[j].align)
				ofs = MDW_ALIGN(ofs, ksubcmd->cmdbufs[j].align);
			else
				ofs = MDW_ALIGN(ofs, MDW_DEFAULT_ALIGN);

			mdw_cmd_debug("sc(%u) #cmdbufs(%u) offset(%u)\n",
				i, j, ofs);

			/* copy cmdbuf */
			if (copy_from_user(cmd->cmdbufs->vaddr + ofs,
				(void __user *)ksubcmd->cmdbufs[j].handle,
				ksubcmd->cmdbufs[j].size)) {
				ret = -EFAULT;
				goto free_cmdbufs;
			}
			/* write */
			ksubcmd->kvaddrs[j] =
				(uint64_t)(cmd->cmdbufs->vaddr + ofs);
			ksubcmd->daddrs[j] =
				(uint64_t)(cmd->cmdbufs->device_va + ofs);
			ofs += ksubcmd->cmdbufs[j].size;
			mdw_cmd_debug("cmdbufs(%u-#%u) (0x%llx/0x%llx)\n",
				i, j, ksubcmd->kvaddrs[j], ksubcmd->daddrs[j]);
		}
	}

	goto out;

free_cmdbufs:
	mdw_mem_free(mpriv, cmd->cmdbufs);
	cmd->cmdbufs = NULL;
out:
	mdw_cmd_debug("ret(%d)\n", ret);
	return ret;
}

static int mdw_cmd_duplicate_cmdbuf(struct mdw_subcmd_kinfo *in,
	bool before_exec)
{
	int i = 0, ret = 0;

	for (i = 0; i < in->info->num_cmdbufs; i++) {
		if (before_exec == true) {
			if (in->cmdbufs[i].direction == MDW_CB_OUT)
				continue;

			if (copy_from_user((void *)in->kvaddrs[i],
				(void __user *)in->cmdbufs[i].handle,
				in->cmdbufs[i].size)) {
				ret = -EFAULT;
				goto out;
			}
		} else {
			if (in->cmdbufs[i].direction == MDW_CB_IN)
				continue;

			if (copy_to_user((void __user *)in->cmdbufs[i].handle,
				(void *)in->kvaddrs[i],
				in->cmdbufs[i].size)) {
				ret = -EFAULT;
				goto out;
			}
		}
	}

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

		/* record kva for duplicate */
		c->ksubcmds[i].kvaddrs = vzalloc(c->subcmds[i].num_cmdbufs *
			sizeof(*c->ksubcmds[i].kvaddrs));
		if (!c->ksubcmds[i].kvaddrs)
			goto free_cmdbufs;

		c->ksubcmds[i].daddrs = vzalloc(c->subcmds[i].num_cmdbufs *
			sizeof(*c->ksubcmds[i].daddrs));
		if (!c->ksubcmds[i].daddrs)
			goto free_cmdbufs;


		/* allocate for subcmd cmdbuf */
		c->ksubcmds[i].cmdbufs = vzalloc(c->subcmds[i].num_cmdbufs *
			sizeof(*c->ksubcmds[i].cmdbufs));
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
			vfree(c->ksubcmds[i].daddrs);
			c->ksubcmds[i].daddrs = NULL;
		}
		/* free kvaddrs */
		if (c->ksubcmds[i].kvaddrs) {
			vfree(c->ksubcmds[i].kvaddrs);
			c->ksubcmds[i].kvaddrs = NULL;
		}
		/* free cmdbufs */
		if (c->ksubcmds[i].cmdbufs) {
			vfree(c->ksubcmds[i].cmdbufs);
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
		if (!c->ksubcmds[i].cmdbufs)
			continue;
		vfree(c->ksubcmds[i].cmdbufs);
		c->ksubcmds[i].cmdbufs = NULL;
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
	return ret;
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

	return dev_name(f->mdev->dev);
}

static bool mdw_fence_enable_signaling(struct dma_fence *fence)
{
	mdw_flw_debug("current pid(%d)\n", current->pid);
	return true;
}

static void mdw_fence_release(struct dma_fence *fence)
{
	struct mdw_fence *f =
		container_of(fence, struct mdw_fence, base_fence);
	struct mdw_cmd *c = f->priv;

	mdw_cmd_set_state(c, MDW_CMD_STATE_IDLE);
	mdw_cmd_postset(c);
	mdw_flw_debug("current pid(%d)\n", current->pid);
	kfree(f);
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

	spin_lock_init(&c->fence->lock);
	dma_fence_init(&c->fence->base_fence, &mdw_fence_ops, &c->fence->lock, 0, 0);
	c->fence->priv = c;

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
	mdw_flw_debug("cmd(%p.%d) exec(%u/%u/%u/%u)\n",
		c->priority, c->hardlimit,
		c->softlimit, c->power_save);

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

out:
	return ret;
}

static int mdw_cmd_run(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_device *mdev = mpriv->mdev;
	int ret = 0;

	mdw_cmd_set_state(c, MDW_CMD_STATE_RUN);
	ret = mdev->dev_funcs->run_cmd(mpriv, c);
	if (ret) {
		mdw_drv_err("run cmd(%p.%d) fail(%d)\n",
			c->mpriv, c->id, ret);
		mdw_cmd_set_state(c, MDW_CMD_STATE_IDLE);
	}

	return ret;
}

static int mdw_cmd_complete(struct mdw_cmd *c, int ret)
{
	mdw_drv_debug("cmd(%p.%d) ret(%d) complete\n",
		c->mpriv, c->id, ret);
	mdw_flw_debug("current pid(%d)\n", current->pid);
	mdw_cmd_set_state(c, MDW_CMD_STATE_DONE);

	return dma_fence_signal(&c->fence->base_fence);
}

static void mdw_cmd_trigger_func(struct work_struct *wk)
{
	struct mdw_cmd *c =
		container_of(wk, struct mdw_cmd, t_wk);
	int ret = 0;

	if (c->wait_fence) {
		mdw_flw_debug("wait sync fence(%d)\n", c->wait_fd);
		dma_fence_wait(c->wait_fence, false);
		dma_fence_put(c->wait_fence);
	}

	c->wait_fd = 0;
	mdw_flw_debug("pid(%d)\n", current->pid);
	ret = mdw_cmd_run(c->mpriv, c);
	if (ret) {
		mdw_drv_err("cmd(%p.%d) run fail(%d)\n",
			c->mpriv, c->id, ret);
		dma_fence_set_error(&c->fence->base_fence, ret);
		dma_fence_signal(&c->fence->base_fence);
	} else {
		mdw_flw_debug("cmd(%p.%d) run\n",
			c->mpriv, c->id);
	}
}

static int mdw_cmd_create(struct mdw_fpriv *mpriv, union mdw_cmd_args *args)
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

	/* copy cmd header info */
	c->kid = (uint64_t)c;
	c->uid = in->build.cmd_uid;
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
	mdw_cmd_debug("cmd(0x%llx/%u/%u/%u/%u)\n", c->uid, c->priority,
		c->hardlimit, c->softlimit, c->power_save);

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

	ret = mdw_cmd_create_infos(mpriv, c);
	if (ret)
		goto free_ksubcmds;

	ret = mdw_cmd_sanity_check(c);
	if (ret)
		goto delete_infos;

	mutex_init(&c->mtx);
	c->mpriv = mpriv;
	c->complete = mdw_cmd_complete;
	c->state = MDW_CMD_STATE_IDLE;
	INIT_WORK(&c->t_wk, &mdw_cmd_trigger_func);

	/* gen cmd id */
	mutex_lock(&mpriv->mtx);
	c->id = idr_alloc(&mpriv->cmds_idr, c, 1, MDW_CMD_MAX, GFP_KERNEL);
	mutex_unlock(&mpriv->mtx);
	if (c->id < 0)
		goto delete_infos;
	mdw_drv_debug("cmd(0x%llx/%p.%d) create\n",
		c->uid, c->mpriv, c->id);

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

static int mdw_cmd_delete(struct mdw_fpriv *mpriv, union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;
	int ret = 0;

	mutex_lock(&mpriv->mtx);
	/* get mdw_cmd */
	c = idr_find(&mpriv->cmds_idr, args->in.release.id);
	if (!c) {
		mdw_drv_err("can't find cmd(%p.%d)\n", mpriv, in->release.id);
		ret = -EINVAL;
		goto unlock_mpriv;
	}

	/* check state */
	if (c->state != MDW_CMD_STATE_IDLE) {
		mdw_drv_err("cmd(%p.%d) in wrong state(%d)\n",
			c->mpriv, c->id, c->state);
		ret = -EBUSY;
		goto unlock_mpriv;
	}

	/* remove from idr */
	idr_remove(&mpriv->cmds_idr, args->in.release.id);
	mutex_unlock(&mpriv->mtx);

	/* free mdw_cmd */
	mdw_cmd_delete_infos(mpriv, c);
	vfree(c->ksubcmds);
	vfree(c->adj_matrix);
	vfree(c->subcmds);
	vfree(c);

	mdw_drv_debug("cmd(%p.%d) delete\n", mpriv, in->release.id);
	memset(args, 0, sizeof(*args));
	goto out;

unlock_mpriv:
	mutex_unlock(&mpriv->mtx);
out:
	return ret;
}

static int mdw_cmd_run_sync(struct mdw_fpriv *mpriv, union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;
	int ret = 0, timeout = 0;

	mutex_lock(&mpriv->mtx);
	/* get mdw_cmd */
	c = idr_find(&mpriv->cmds_idr, in->exec.id);
	if (!c) {
		mdw_drv_err("can't find cmd(%p.%d)\n", mpriv, in->exec.id);
		ret = -EINVAL;
		goto unlock_mpriv;
	}

	ret = mdw_cmd_preset(c, in);
	if (ret) {
		mdw_drv_err("cmd(%p.%d) preset fail(%d)\n",
			c->mpriv, c->id, ret);
		goto unlock_mpriv;
	}

	ret = mdw_cmd_run(mpriv, c);
	if (ret) {
		mdw_drv_err("cmd(%p.%d) run fail(%d)\n",
			c->mpriv, c->id, ret);
		goto unlock_mpriv;
	}

	mutex_unlock(&mpriv->mtx);

	timeout = msecs_to_jiffies(MDW_DEFAULT_TIMEOUT_MS);
	ret = dma_fence_wait_timeout(&c->fence->base_fence, true, timeout);
	if (ret > 0 &&  c->fence->base_fence.error) {
		mdw_drv_warn("cmd(%p.%d) wait fail(%d/%d)\n",
			c->mpriv, c->id, ret, c->fence->base_fence.error);
		ret = c->fence->base_fence.error;
	} else {
		mdw_flw_debug("cmd(%p.%d) wait ret(%d/%d)\n",
			c->mpriv, c->id, ret, c->fence->base_fence.error);
		ret = 0;
	}
	dma_fence_put(&c->fence->base_fence);

	goto out;

unlock_mpriv:
	mutex_unlock(&mpriv->mtx);
out:
	memset(args, 0, sizeof(*args));
	return ret;
}

static int mdw_cmd_run_async(struct mdw_fpriv *mpriv, union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;
	struct sync_file *sync_file = NULL;
	int ret = 0, fd = 0, wait_fd = 0;

	mutex_lock(&mpriv->mtx);
	wait_fd = args->in.exec.fence;

	/* get mdw_cmd */
	c = idr_find(&mpriv->cmds_idr, in->exec.id);
	if (!c) {
		mdw_drv_err("can't find cmd(%p.%d)\n", mpriv, in->exec.id);
		ret = -EINVAL;
		goto out;
	}

	ret = mdw_cmd_preset(c, in);
	if (ret) {
		mdw_drv_err("cmd(%p.%d) preset fail(%d)\n",
			c->mpriv, c->id, ret);
		goto out;
	}

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		mdw_drv_err("get unused fd fail\n");
		ret = -EINVAL;
		goto out;
	}
	sync_file = sync_file_create(&c->fence->base_fence);
	if (!sync_file) {
		mdw_drv_err("create sync file fail\n");
		put_unused_fd(fd);
		ret = -ENOMEM;
		goto out;
	}
	dma_fence_put(&c->fence->base_fence);
	fd_install(fd, sync_file->file);

#if 0
	if (wait_fd <= 0) {
		ret = mdw_cmd_run(mpriv, c);
		if (ret) {
			mdw_drv_err("cmd(%p.%d) run fail(%d)\n",
				c->mpriv, c->id, ret);
			put_unused_fd(fd);
			goto out;
		}
	} else {
		c->wait_fd = wait_fd;
		schedule_work(&c->t_wk);
	}
#else
	mdw_flw_debug("cmd(%p.%d) wait fence(%d)\n",
		c->mpriv, c->id, wait_fd);
	c->wait_fence = sync_file_get_fence(wait_fd);
	c->wait_fd = wait_fd;
	if (!c->wait_fence)
		mdw_flw_debug("no wait fence\n");
	schedule_work(&c->t_wk);
#endif
	memset(args, 0, sizeof(*args));
	args->out.exec.fence = fd;

	mdw_flw_debug("async fd(%d)\n", fd);
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
		ret = mdw_cmd_create(mpriv, args);
		break;

	case MDW_CMD_IOCTL_RELEASE:
		ret = mdw_cmd_delete(mpriv, args);
		break;

	case MDW_CMD_IOCTL_RUN:
		ret = mdw_cmd_run_sync(mpriv, args);
		break;

	case MDW_CMD_IOCTL_RUNASYNC:
		ret = mdw_cmd_run_async(mpriv, args);
		break;

	default:
		ret = -EINVAL;
		break;
	}
	mdw_flw_debug("done\n");

	return ret;
}
