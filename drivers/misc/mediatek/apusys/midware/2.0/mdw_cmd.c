// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sync_file.h>

#include "mdw_trace.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"

#define mdw_cmd_show(c, f) \
	f("cmd(%p/0x%llx/0x%llx)param(%u/%u/%u/%u/"\
	"%u/%u/%u)subcmds(%u/%p/%u/%u)pid(%d/%d)(%d)\n", \
	c->mpriv, c->uid, c->kid, \
	c->priority, c->hardlimit, c->softlimit, \
	c->power_save, c->power_plcy, c->power_dtime, \
	c->app_type, c->num_subcmds, c->cmdbufs, \
	c->num_cmdbufs, c->size_cmdbufs, \
	c->pid, c->tgid, current->pid)

static struct mdw_mem *mdw_cmd_get_mem(struct mdw_fpriv *mpriv, uint64_t handle)
{
	struct mdw_mem *m = NULL;

	m = mdw_mem_get(mpriv, handle);
	if (m == NULL)
		return NULL;

	mdw_mem_dma_map(m);
	mdw_cmd_debug("u(%p) get cmdbuf(%p/%llu)(%p/0x%llx)\n",
		mpriv, m, handle, m->vaddr, m->device_va);

	return m;
}

static int mdw_cmd_put_mem(struct mdw_fpriv *mpriv, struct mdw_mem *m)
{
	mdw_mem_dma_unmap(m);
	return 0;
}

static void mdw_cmd_put_cmdbufs(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	unsigned int i = 0, j = 0;

	if (!c->cmdbufs)
		return;

	mdw_trace_begin("put cbs|c(0x%llx) num_subcmds(%u) num_cmdbufs(%u)",
		c->kid, c->num_subcmds, c->num_cmdbufs);

	/* flush cmdbufs and execinfos */
	apusys_mem_invalidate_kva(c->cmdbufs->vaddr, c->cmdbufs->size);
	apusys_mem_invalidate_kva(c->exec_infos->vaddr, c->exec_infos->size);

	for (i = 0; i < c->num_subcmds; i++) {
		ksubcmd = &c->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			if (!ksubcmd->ori_cbs[j]) {
				mdw_drv_warn("no ori mems(%d-%d)\n", i, j);
				continue;
			}

			mdw_trace_begin("cbs copy out|sc(0x%llx-%u) cb-#%u/%u",
				c->kid, i, j, ksubcmd->ori_cbs[j]->size);

			/* cmdbuf copy out */
			if (ksubcmd->cmdbufs[j].direction != MDW_CB_IN) {
				memcpy(ksubcmd->ori_cbs[j]->vaddr,
					(void *)ksubcmd->kvaddrs[j],
					ksubcmd->ori_cbs[j]->size);
			}

			mdw_trace_end("cbs copy out|sc(0x%llx-%u) cb-#%u/%u",
				c->kid, i, j,
				ksubcmd->ori_cbs[j]->size);

			/* put mem */
			if (mdw_cmd_put_mem(mpriv,
				ksubcmd->ori_cbs[j]))
				mdw_drv_warn("cmd(%p/0x%llx) put cmdbuf(%llu) fail\n",
				mpriv, c->kid,
				ksubcmd->cmdbufs[j].handle);
			ksubcmd->ori_cbs[j] = NULL;
		}
	}

	mdw_mem_free(mpriv, c->cmdbufs);
	c->cmdbufs = NULL;

	mdw_trace_end("put cbs|c(0x%llx) num_subcmds(%u) num_cmdbufs(%u)",
		c->kid, c->num_subcmds, c->num_cmdbufs);
}

static int mdw_cmd_get_cmdbufs(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	unsigned int i = 0, j = 0, ofs = 0;
	int ret = -EINVAL;
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	struct mdw_mem *m = NULL;

	mdw_trace_begin("get cbs|c(0x%llx) num_subcmds(%u) num_cmdbufs(%u)",
		c->kid, c->num_subcmds, c->num_cmdbufs);

	if (!c->size_cmdbufs || c->cmdbufs)
		goto out;

	/* alloc cmdbuf by dmabuf */
	c->cmdbufs = mdw_mem_alloc(mpriv, c->size_cmdbufs, MDW_DEFAULT_ALIGN,
		(1ULL << MDW_MEM_IOCTL_ALLOC_CACHEABLE), MDW_MEM_TYPE_INTERNAL);
	if (!c->cmdbufs) {
		mdw_drv_err("cmd(%p/0x%llx) alloc buffer for duplicate fail\n",
			mpriv, c->kid);
		ret = -ENOMEM;
		goto out;
	}

	/* alloc mem for duplicated cmdbuf */
	for (i = 0; i < c->num_subcmds; i++) {
		mdw_cmd_debug("sc(0x%llx-%u) #cmdbufs(%u)\n",
			c->kid, i, c->ksubcmds[i].info->num_cmdbufs);

		ksubcmd = &c->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			/* calc align */
			if (ksubcmd->cmdbufs[j].align)
				ofs = MDW_ALIGN(ofs, ksubcmd->cmdbufs[j].align);
			else
				ofs = MDW_ALIGN(ofs, MDW_DEFAULT_ALIGN);

			mdw_cmd_debug("sc(0x%llx-%u) cb#%u offset(%u)\n",
				c->kid, i, j, ofs);

			/* get mem from handle */
			m = mdw_cmd_get_mem(mpriv, ksubcmd->cmdbufs[j].handle);
			if (!m) {
				mdw_drv_err("sc(0x%llx-%u) cb#%u(%llu) get fail\n",
					c->kid, i, j,
					ksubcmd->cmdbufs[j].handle);
				goto free_cmdbufs;
			}
			/* check mem boundary */
			if (m->vaddr == NULL ||
				ksubcmd->cmdbufs[j].size != m->size) {
				mdw_drv_err("sc(0x%llx-%u) cb#%u invalid range(%p/%u/%u)\n",
					c->kid, i, j, m->vaddr,
					ksubcmd->cmdbufs[j].size,
					m->size);
				goto free_cmdbufs;
			}

			mdw_trace_begin("cbs copy in|sc(0x%llx-%u) cb-#%u/%u",
				c->kid, i, j,
				ksubcmd->cmdbufs[j].size);

			/* cmdbuf copy in */
			if (ksubcmd->cmdbufs[j].direction != MDW_CB_OUT) {
				memcpy(c->cmdbufs->vaddr + ofs,
					m->vaddr,
					ksubcmd->cmdbufs[j].size);
			}

			mdw_trace_end("cbs copy in|sc(0x%llx-%u) cb-#%u/%u",
				c->kid, i, j, ksubcmd->cmdbufs[j].size);

			/* record buffer info */
			ksubcmd->ori_cbs[j] = m;
			ksubcmd->kvaddrs[j] =
				(uint64_t)(c->cmdbufs->vaddr + ofs);
			ksubcmd->daddrs[j] =
				(uint64_t)(c->cmdbufs->device_va + ofs);
			ofs += ksubcmd->cmdbufs[j].size;

			mdw_cmd_debug("sc(0x%llx-%u) cb#%u (0x%llx/0x%llx)\n",
				c->kid, i, j,
				ksubcmd->kvaddrs[j],
				ksubcmd->daddrs[j]);
		}
	}
	/* flush cmdbufs */
	apusys_mem_flush_kva(c->cmdbufs->vaddr, c->cmdbufs->size);

	ret = 0;
	goto out;

free_cmdbufs:
	mdw_cmd_put_cmdbufs(mpriv, c);
out:
	mdw_cmd_debug("ret(%d)\n", ret);
	mdw_trace_end("get cbs|c(0x%llx) num_subcmds(%u) num_cmdbufs(%u)",
		c->kid, c->num_subcmds, c->num_cmdbufs);
	return ret;
}

static unsigned int mdw_cmd_create_infos(struct mdw_fpriv *mpriv,
	struct mdw_cmd *c)
{
	unsigned int i = 0, j = 0, total_size = 0;
	struct mdw_subcmd_exec_info *einfo = NULL;
	int ret = 0;

	einfo = c->exec_infos->vaddr;
	if (!einfo) {
		mdw_drv_err("invalid exec info addr\n");
		return -EINVAL;
	}

	for (i = 0; i < c->num_subcmds; i++) {
		c->ksubcmds[i].info = &c->subcmds[i];
		mdw_cmd_debug("subcmd(%u)(%u/%u/%u/%u/%u/%u/%u/%u/0x%llx)\n",
			i, c->subcmds[i].type,
			c->subcmds[i].suggest_time, c->subcmds[i].vlm_usage,
			c->subcmds[i].vlm_ctx_id, c->subcmds[i].vlm_force,
			c->subcmds[i].boost, c->subcmds[i].pack_id,
			c->subcmds[i].num_cmdbufs, c->subcmds[i].cmdbufs);

		/* kva for oroginal buffer */
		c->ksubcmds[i].ori_cbs = kvzalloc(c->subcmds[i].num_cmdbufs *
			sizeof(c->ksubcmds[i].ori_cbs), GFP_KERNEL);
		if (!c->ksubcmds[i].ori_cbs)
			goto free_cmdbufs;

		/* record kva for duplicate */
		c->ksubcmds[i].kvaddrs = kvzalloc(c->subcmds[i].num_cmdbufs *
			sizeof(*c->ksubcmds[i].kvaddrs), GFP_KERNEL);
		if (!c->ksubcmds[i].kvaddrs)
			goto free_cmdbufs;

		/* record dva for cmdbufs */
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

		c->ksubcmds[i].exec_infos = &einfo[i];

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

	mdw_cmd_debug("sc(0x%llx) cb_num(%u) total size(%u)\n",
		c->kid, c->num_cmdbufs, c->size_cmdbufs);

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
		if (c->ksubcmds[i].ori_cbs) {
			kvfree(c->ksubcmds[i].ori_cbs);
			c->ksubcmds[i].ori_cbs = NULL;
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
		if (c->ksubcmds[i].ori_cbs) {
			kvfree(c->ksubcmds[i].ori_cbs);
			c->ksubcmds[i].ori_cbs = NULL;
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
		mdw_drv_err("cmd invalid (0x%llx/%p/0x%llx)(%u)\n",
			c->uid, c->mpriv, c->kid, c->priority);
		return -EINVAL;
	}

	if (c->exec_infos->size != c->num_subcmds *
		sizeof(struct mdw_subcmd_exec_info)) {
		mdw_drv_err("execinfo size invalid(%u/%u)\n",
			c->exec_infos->size,
			c->num_subcmds * sizeof(struct mdw_subcmd_exec_info));
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

	mdw_drv_debug("fence release\n");
	kvfree(mf);
}

static const struct dma_fence_ops mdw_fence_ops = {
	.get_driver_name =  mdw_fence_get_driver_name,
	.get_timeline_name =  mdw_fence_get_timeline_name,
	.enable_signaling =  mdw_fence_enable_signaling,
	.wait = dma_fence_default_wait,
	.release =  mdw_fence_release,
};

//--------------------------------------------
static int mdw_fence_init(struct mdw_cmd *c)
{
	int ret = 0;

	c->fence = kvzalloc(sizeof(*c->fence), GFP_KERNEL);
	if (!c->fence)
		return -ENOMEM;

	dma_fence_init(&c->fence->base_fence, &mdw_fence_ops,
		&c->fence->lock, 0, 0);
	spin_lock_init(&c->fence->lock);

	return ret;
}

static int mdw_cmd_run(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_device *mdev = mpriv->mdev;
	int ret = 0;

	mdw_cmd_show(c, mdw_cmd_debug);
	ret = mdev->dev_funcs->run_cmd(mpriv, c);
	if (ret) {
		mdw_drv_err("run cmd(%p/0x%llx) fail(%d)\n",
			c->mpriv, c->kid, ret);

		dma_fence_set_error(&c->fence->base_fence, ret);
		dma_fence_signal(&c->fence->base_fence);
		dma_fence_put(&c->fence->base_fence);
	} else {
		mdw_flw_debug("cmd(%p/0x%llx) run\n",
			c->mpriv, c->kid);
	}

	return ret;
}

static void mdw_cmd_delete(struct mdw_cmd *c)
{
	mdw_cmd_show(c, mdw_drv_debug);

	/* delete */
	mdw_cmd_delete_infos(c->mpriv, c);
	mdw_cmd_put_mem(c->mpriv, c->exec_infos);
	kvfree(c->ksubcmds);
	kvfree(c->adj_matrix);
	kvfree(c->subcmds);
	mutex_lock(&c->mpriv->mtx);
	list_del(&c->u_item);
	mutex_unlock(&c->mpriv->mtx);
	kvfree(c);
}

static int mdw_cmd_complete(struct mdw_cmd *c, int ret)
{
	struct dma_fence *f = &c->fence->base_fence;

	mdw_flw_debug("cmd(%p/0x%llx) ret(%d) complete, pid(%d/%d)(%d)\n",
		c->mpriv, c->kid, ret, c->pid, c->tgid, current->pid);

	mdw_cmd_put_cmdbufs(c->mpriv, c);
	if (ret)
		dma_fence_set_error(&c->fence->base_fence, ret);
	dma_fence_signal(f);
	mdw_cmd_delete(c);
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

	mdw_flw_debug("cmd(%p/0x%llx) wait fence done, start run\n",
		c->mpriv, c->kid);
	mdw_cmd_run(c->mpriv, c);
}

void mdw_cmd_mpriv_release(struct mdw_fpriv *mpriv)
{
	mdw_drv_debug("TODO\n");
	//TODO
}

static struct mdw_cmd *mdw_cmd_create(struct mdw_fpriv *mpriv,
	union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;

	/* check num subcmds maximum */
	if (in->exec.num_subcmds > MDW_SUBCMD_MAX) {
		mdw_drv_err("too much subcmds(%u)\n", in->exec.num_subcmds);
		goto out;
	}

	/* alloc mdw cmd */
	c = kvzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		goto out;

	/* setup cmd info */
	c->pid = current->pid;
	c->tgid = current->tgid;
	c->kid = (uint64_t)c;
	c->uid = in->exec.uid;
	c->usr_id = in->exec.usr_id;
	c->priority = in->exec.priority;
	c->hardlimit = in->exec.hardlimit;
	c->softlimit = in->exec.softlimit;
	c->power_save = in->exec.power_save;
	c->power_plcy = in->exec.power_plcy;
	c->power_dtime = in->exec.power_dtime;
	c->app_type = in->exec.app_type;
	c->num_subcmds = in->exec.num_subcmds;
	c->subcmds = kvzalloc(c->num_subcmds * sizeof(*c->subcmds), GFP_KERNEL);
	if (!c->subcmds)
		goto free_cmd;

	/* copy adj matrix */
	c->adj_matrix = kvzalloc(c->num_subcmds *
		c->num_subcmds * sizeof(uint8_t), GFP_KERNEL);
	if (copy_from_user(c->adj_matrix, (void __user *)in->exec.adj_matrix,
		(c->num_subcmds * c->num_subcmds * sizeof(uint8_t)))) {
		mdw_drv_err("copy adj matrix fail\n");
		goto free_subcmds;
	}
	if (g_mdw_klog & MDW_DBG_CMD) {
		print_hex_dump(KERN_INFO, "[apusys] adj matrix: ",
			DUMP_PREFIX_OFFSET, 16, 1, c->adj_matrix,
			c->num_subcmds * c->num_subcmds, 0);
	}

	/* copy subcmd headers info */
	if (copy_from_user(c->subcmds, (void __user *)in->exec.subcmd_infos,
		c->num_subcmds * sizeof(*c->subcmds))) {
		mdw_drv_err("copy subcmds fail\n");
		goto free_adj;
	}

	/* alloc ksubcmds */
	c->ksubcmds = kvzalloc(c->num_subcmds * sizeof(*c->ksubcmds),
		GFP_KERNEL);
	if (!c->ksubcmds)
		goto free_adj;

	/* get exec info */
	c->exec_infos = mdw_cmd_get_mem(mpriv, in->exec.exec_infos);
	if (!c->exec_infos) {
		mdw_drv_err("get exec info fail\n");
		goto free_ksubcmds;
	}

	if (mdw_cmd_create_infos(mpriv, c)) {
		mdw_drv_err("create cmd info fail\n");
		goto put_execinfo;
	}

	if (mdw_cmd_sanity_check(c)) {
		mdw_drv_err("cmd sanity check fail\n");
		goto delete_infos;
	}

	/* init fence */
	if (mdw_fence_init(c)) {
		mdw_drv_err("cmd init fence fail\n");
		goto delete_infos;
	}
	mutex_init(&c->mtx);
	c->mpriv = mpriv;
	c->complete = mdw_cmd_complete;
	INIT_WORK(&c->t_wk, &mdw_cmd_trigger_func);
	mutex_lock(&mpriv->mtx);
	list_add_tail(&c->u_item, &mpriv->cmds);
	mutex_unlock(&mpriv->mtx);
	mdw_cmd_show(c, mdw_drv_debug);

	goto out;

delete_infos:
	mdw_cmd_delete_infos(mpriv, c);
put_execinfo:
	mdw_cmd_put_mem(mpriv, c->exec_infos);
free_ksubcmds:
	vfree(c->ksubcmds);
free_adj:
	vfree(c->adj_matrix);
free_subcmds:
	vfree(c->subcmds);
free_cmd:
	vfree(c);
	c = NULL;
out:
	return c;
}

static int mdw_cmd_ioctl_run(struct mdw_fpriv *mpriv, union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;
	struct sync_file *sync_file = NULL;
	int ret = 0, fd = 0, wait_fd = 0;

	/* get wait fd */
	wait_fd = in->exec.fence;

	c = mdw_cmd_create(mpriv, args);
	if (!c) {
		mdw_drv_err("create cmd fail\n");
		ret = -EINVAL;
		goto out;
	}
	memset(args, 0, sizeof(*args));

	/* get sync_file fd */
	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		mdw_drv_err("get unused fd fail\n");
		ret = -EINVAL;
		goto delete_cmd;
	}
	sync_file = sync_file_create(&c->fence->base_fence);
	if (!sync_file) {
		mdw_drv_err("create sync file fail\n");
		ret = -ENOMEM;
		goto put_file;
	}

	/* check wait fence from other module */
	mdw_flw_debug("cmd(%p/0x%llx) wait fence(%d)\n",
		c->mpriv, c->kid, wait_fd);
	c->wait_fence = sync_file_get_fence(wait_fd);
	if (!c->wait_fence) {
		mdw_flw_debug("cmd(%p/0x%llx) no wait fence, trigger directly\n",
			c->mpriv, c->kid);
		ret = mdw_cmd_run(mpriv, c);
	} else {
		/* wait fence from wq */
		schedule_work(&c->t_wk);
	}

	if (ret)
		goto put_file;

	/* assign fd */
	fd_install(fd, sync_file->file);
	args->out.exec.fence = fd;
	mdw_flw_debug("async fd(%d)\n", fd);
	goto out;

delete_cmd:
	mdw_cmd_delete(c);
put_file:
	put_unused_fd(fd);
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
