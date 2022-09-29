// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sync_file.h>
#include <linux/sched/clock.h>

#include "mdw_trace.h"
#include "mdw_cmn.h"
#include "mdw_mem.h"
#include "mdw_mem_pool.h"
#include "rv/mdw_rv_tag.h"

#define mdw_cmd_show(c, f) \
	f("cmd(0x%llx/0x%llx/0x%llx/0x%llx/%d/%u)param(%u/%u/%u/%u/"\
	"%u/%u/%u)subcmds(%u/%p/%u/%u)pid(%d/%d)(%d)\n", \
	(uint64_t) c->mpriv, c->uid, c->kid, c->rvid, c->id, kref_read(&c->ref), \
	c->priority, c->hardlimit, c->softlimit, \
	c->power_save, c->power_plcy, c->power_dtime, \
	c->app_type, c->num_subcmds, c->cmdbufs, \
	c->num_cmdbufs, c->size_cmdbufs, \
	c->pid, c->tgid, task_pid_nr(current))

static void mdw_cmd_cmdbuf_out(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	unsigned int i = 0, j = 0;

	if (!c->cmdbufs)
		return;

	/* flush cmdbufs and execinfos */
	if (mdw_mem_invalidate(mpriv, c->cmdbufs))
		mdw_drv_warn("s(0x%llx)c(0x%llx) invalidate cmdbufs(%u) fail\n",
			(uint64_t)mpriv, c->kid, c->cmdbufs->size);

	for (i = 0; i < c->num_subcmds; i++) {
		ksubcmd = &c->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			if (!ksubcmd->ori_cbs[j]) {
				mdw_drv_warn("no ori mems(%d-%d)\n", i, j);
				continue;
			}

			/* cmdbuf copy out */
			if (ksubcmd->cmdbufs[j].direction != MDW_CB_IN) {
				mdw_trace_begin("apumdw:cbs_copy_out|cb:%u-%u size:%u type:%u",
					i, j, ksubcmd->ori_cbs[j]->size, ksubcmd->info->type);
				memcpy(ksubcmd->ori_cbs[j]->vaddr,
					(void *)ksubcmd->kvaddrs[j],
					ksubcmd->ori_cbs[j]->size);
				mdw_trace_end();
			}
		}
	}
}

static void mdw_cmd_put_cmdbufs(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	unsigned int i = 0, j = 0;

	if (!c->cmdbufs)
		return;

	mdw_trace_begin("apumdw:cbs_put|c:0x%llx num_subcmds:%u num_cmdbufs:%u",
		c->kid, c->num_subcmds, c->num_cmdbufs);

	for (i = 0; i < c->num_subcmds; i++) {
		ksubcmd = &c->ksubcmds[i];
		for (j = 0; j < ksubcmd->info->num_cmdbufs; j++) {
			if (!ksubcmd->ori_cbs[j]) {
				mdw_drv_warn("no ori mems(%d-%d)\n", i, j);
				continue;
			}

			/* put mem */
			mdw_mem_put(mpriv, ksubcmd->ori_cbs[j]);
			ksubcmd->ori_cbs[j] = NULL;
		}
	}

	mdw_mem_pool_free(c->cmdbufs);
	c->cmdbufs = NULL;

	mdw_trace_end();
}

static int mdw_cmd_get_cmdbufs(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	unsigned int i = 0, j = 0, ofs = 0;
	int ret = -EINVAL;
	struct mdw_subcmd_kinfo *ksubcmd = NULL;
	struct mdw_mem *m = NULL;
	struct apusys_cmdbuf *acbs = NULL;

	mdw_trace_begin("apumdw:cbs_get|c:0x%llx num_subcmds:%u num_cmdbufs:%u",
		c->kid, c->num_subcmds, c->num_cmdbufs);

	if (!c->size_cmdbufs || c->cmdbufs)
		goto out;

	c->cmdbufs = mdw_mem_pool_alloc(&mpriv->cmd_buf_pool, c->size_cmdbufs,
		MDW_DEFAULT_ALIGN);
	if (!c->cmdbufs) {
		mdw_drv_err("s(0x%llx)c(0x%llx) alloc buffer for duplicate fail\n",
		(uint64_t) mpriv, c->kid);
		ret = -ENOMEM;
		goto out;
	}

	/* alloc mem for duplicated cmdbuf */
	for (i = 0; i < c->num_subcmds; i++) {
		mdw_cmd_debug("sc(0x%llx-%u) #cmdbufs(%u)\n",
			c->kid, i, c->ksubcmds[i].info->num_cmdbufs);

		acbs = kcalloc(c->ksubcmds[i].info->num_cmdbufs, sizeof(*acbs), GFP_KERNEL);
		if (!acbs)
			goto free_cmdbufs;

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
			m = mdw_mem_get(mpriv, ksubcmd->cmdbufs[j].handle);
			if (!m) {
				mdw_drv_err("sc(0x%llx-%u) cb#%u(%llu) get fail\n",
					c->kid, i, j,
					ksubcmd->cmdbufs[j].handle);
				ret = -EINVAL;
				goto free_cmdbufs;
			}
			/* check mem boundary */
			if (m->vaddr == NULL ||
				ksubcmd->cmdbufs[j].size != m->size) {
				mdw_drv_err("sc(0x%llx-%u) cb#%u invalid range(%p/%u/%u)\n",
					c->kid, i, j, m->vaddr,
					ksubcmd->cmdbufs[j].size,
					m->size);
				ret = -EINVAL;
				goto free_cmdbufs;
			}

			/* cmdbuf copy in */
			if (ksubcmd->cmdbufs[j].direction != MDW_CB_OUT) {
				mdw_trace_begin("apumdw:cbs_copy_in|cb:%u-%u size:%u type:%u",
					i, j,
					ksubcmd->cmdbufs[j].size,
					ksubcmd->info->type);
				memcpy(c->cmdbufs->vaddr + ofs,
					m->vaddr,
					ksubcmd->cmdbufs[j].size);
				mdw_trace_end();
			}

			/* record buffer info */
			ksubcmd->ori_cbs[j] = m;
			ksubcmd->kvaddrs[j] =
				(uint64_t)(c->cmdbufs->vaddr + ofs);
			ksubcmd->daddrs[j] =
				(uint64_t)(c->cmdbufs->device_va + ofs);
			ofs += ksubcmd->cmdbufs[j].size;

			mdw_cmd_debug("sc(0x%llx-%u) cb#%u (0x%llx/0x%llx/%u)\n",
				c->kid, i, j,
				ksubcmd->kvaddrs[j],
				ksubcmd->daddrs[j],
				ksubcmd->cmdbufs[j].size);

			acbs[j].kva = (void *)ksubcmd->kvaddrs[j];
			acbs[j].size = ksubcmd->cmdbufs[j].size;
		}

		ret = mdw_dev_validation(mpriv, ksubcmd->info->type,
			c, acbs, ksubcmd->info->num_cmdbufs);
		kfree(acbs);
		acbs = NULL;
		if (ret) {
			mdw_drv_err("sc(0x%llx-%u) dev(%u) validate cb(%u) fail(%d)\n",
				c->kid, i, ksubcmd->info->type, ksubcmd->info->num_cmdbufs, ret);
			goto free_cmdbufs;
		}
	}
	/* flush cmdbufs */
	if (mdw_mem_flush(mpriv, c->cmdbufs))
		mdw_drv_warn("s(0x%llx) c(0x%llx) flush cmdbufs(%u) fail\n",
			(uint64_t)mpriv, c->kid, c->cmdbufs->size);

	ret = 0;
	goto out;

free_cmdbufs:
	mdw_cmd_put_cmdbufs(mpriv, c);
	kfree(acbs);
out:
	mdw_cmd_debug("ret(%d)\n", ret);
	mdw_trace_end();
	return ret;
}

static unsigned int mdw_cmd_create_infos(struct mdw_fpriv *mpriv,
	struct mdw_cmd *c)
{
	unsigned int i = 0, j = 0, total_size = 0;
	struct mdw_subcmd_exec_info *sc_einfo = NULL;
	int ret = -ENOMEM;

	c->einfos = c->exec_infos->vaddr;
	if (!c->einfos) {
		mdw_drv_err("invalid exec info addr\n");
		return -EINVAL;
	} else {
		/* clear run infos for return */
		memset(c->exec_infos->vaddr, 0, c->exec_infos->size);
	}
	sc_einfo = &c->einfos->sc;

	for (i = 0; i < c->num_subcmds; i++) {
		c->ksubcmds[i].info = &c->subcmds[i];
		mdw_cmd_debug("subcmd(%u)(%u/%u/%u/%u/%u/%u/%u/%u/0x%llx)\n",
			i, c->subcmds[i].type,
			c->subcmds[i].suggest_time, c->subcmds[i].vlm_usage,
			c->subcmds[i].vlm_ctx_id, c->subcmds[i].vlm_force,
			c->subcmds[i].boost, c->subcmds[i].pack_id,
			c->subcmds[i].num_cmdbufs, c->subcmds[i].cmdbufs);

		/* kva for oroginal buffer */
		c->ksubcmds[i].ori_cbs = kcalloc(c->subcmds[i].num_cmdbufs,
			sizeof(c->ksubcmds[i].ori_cbs), GFP_KERNEL);
		if (!c->ksubcmds[i].ori_cbs)
			goto free_cmdbufs;

		/* record kva for duplicate */
		c->ksubcmds[i].kvaddrs = kcalloc(c->subcmds[i].num_cmdbufs,
			sizeof(*c->ksubcmds[i].kvaddrs), GFP_KERNEL);
		if (!c->ksubcmds[i].kvaddrs)
			goto free_cmdbufs;

		/* record dva for cmdbufs */
		c->ksubcmds[i].daddrs = kcalloc(c->subcmds[i].num_cmdbufs,
			sizeof(*c->ksubcmds[i].daddrs), GFP_KERNEL);
		if (!c->ksubcmds[i].daddrs)
			goto free_cmdbufs;

		/* allocate for subcmd cmdbuf */
		c->ksubcmds[i].cmdbufs = kcalloc(c->subcmds[i].num_cmdbufs,
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

		c->ksubcmds[i].sc_einfo = &sc_einfo[i];

		/* accumulate cmdbuf size with alignment */
		for (j = 0; j < c->subcmds[i].num_cmdbufs; j++) {
			c->num_cmdbufs++;
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
		kfree(c->ksubcmds[i].daddrs);
		c->ksubcmds[i].daddrs = NULL;

		/* free kvaddrs */
		kfree(c->ksubcmds[i].kvaddrs);
		c->ksubcmds[i].kvaddrs = NULL;

		/* free ori kvas */
		kfree(c->ksubcmds[i].ori_cbs);
		c->ksubcmds[i].ori_cbs = NULL;

		/* free cmdbufs */
		kfree(c->ksubcmds[i].cmdbufs);
		c->ksubcmds[i].cmdbufs = NULL;
	}

out:
	return ret;
}

static void mdw_cmd_delete_infos(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	unsigned int i = 0;

	mdw_cmd_put_cmdbufs(mpriv, c);

	for (i = 0; i < c->num_subcmds; i++) {
		/* free dvaddrs */
		kfree(c->ksubcmds[i].daddrs);
		c->ksubcmds[i].daddrs = NULL;

		/* free kvaddrs */
		kfree(c->ksubcmds[i].kvaddrs);
		c->ksubcmds[i].kvaddrs = NULL;

		/* free ori kvas */
		kfree(c->ksubcmds[i].ori_cbs);
		c->ksubcmds[i].ori_cbs = NULL;

		/* free cmdbufs */
		kfree(c->ksubcmds[i].cmdbufs);
		c->ksubcmds[i].cmdbufs = NULL;
	}
}

void mdw_cmd_mpriv_release(struct mdw_fpriv *mpriv)
{
	struct mdw_cmd *c = NULL;
	uint32_t id = 0;

	if (!atomic_read(&mpriv->active) && !atomic_read(&mpriv->active_cmds)) {
		mdw_flw_debug("s(0x%llx) release cmd\n", (uint64_t)mpriv);
		idr_for_each_entry(&mpriv->cmds, c, id) {
			idr_remove(&mpriv->cmds, id);
			mdw_cmd_delete(c);
		}
		mdw_flw_debug("s(0x%llx) release mem\n", (uint64_t)mpriv);
		mdw_mem_mpriv_release(mpriv);
	}
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

	return f->name;
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
	kfree(mf);
}

static const struct dma_fence_ops mdw_fence_ops = {
	.get_driver_name =  mdw_fence_get_driver_name,
	.get_timeline_name =  mdw_fence_get_timeline_name,
	.enable_signaling =  mdw_fence_enable_signaling,
	.wait = dma_fence_default_wait,
	.release =  mdw_fence_release,
};

//--------------------------------------------
static int mdw_fence_init(struct mdw_cmd *c, int fd)
{
	int ret = 0;

	c->fence = kzalloc(sizeof(*c->fence), GFP_KERNEL);
	if (!c->fence)
		return -ENOMEM;


	if (snprintf(c->fence->name, sizeof(c->fence->name), "%d:%s", fd, c->comm) <= 0)
		mdw_drv_warn("set fance name fail\n");
	c->fence->mdev = c->mpriv->mdev;
	spin_lock_init(&c->fence->lock);
	dma_fence_init(&c->fence->base_fence, &mdw_fence_ops,
		&c->fence->lock, 0, 0);

	mdw_cmd_debug("fence init, c(0x%llx) fence name(%s)\n",
		(uint64_t)c, c->fence->name);

	return ret;
}

static void mdw_cmd_unvoke_map(struct mdw_cmd *c)
{
	struct mdw_cmd_map_invoke *cm_invoke = NULL, *tmp = NULL;

	list_for_each_entry_safe(cm_invoke, tmp, &c->map_invokes, c_node) {
		list_del(&cm_invoke->c_node);
		mdw_cmd_debug("s(0x%llx)c(0x%llx) unvoke m(0x%llx/%u)\n",
			(uint64_t)c->mpriv, (uint64_t)c,
			cm_invoke->map->m->device_va,
			cm_invoke->map->m->dva_size);
		cm_invoke->map->put(cm_invoke->map);
		kfree(cm_invoke);
	}
}

int mdw_cmd_invoke_map(struct mdw_cmd *c, struct mdw_mem_map *map)
{
	struct mdw_cmd_map_invoke *cm_invoke = NULL;

	if (map == NULL)
		return -EINVAL;

	/* query */
	list_for_each_entry(cm_invoke, &c->map_invokes, c_node) {
		/* already invoked */
		if (cm_invoke->map == map)
			return 0;
	}

	cm_invoke = kzalloc(sizeof(*cm_invoke), GFP_KERNEL);
	if (cm_invoke == NULL)
		return -ENOMEM;

	map->get(map);
	cm_invoke->map = map;
	list_add_tail(&cm_invoke->c_node, &c->map_invokes);
	mdw_cmd_debug("s(0x%llx)c(0x%llx) invoke m(0x%llx/%u)\n",
		(uint64_t)c->mpriv, (uint64_t)c, map->m->device_va, map->m->dva_size);

	return 0;
}

static void mdw_cmd_release(struct kref *ref)
{
	struct mdw_cmd *c =
			container_of(ref, struct mdw_cmd, ref);
	struct mdw_fpriv *mpriv = c->mpriv;

	mdw_cmd_show(c, mdw_drv_debug);
	mdw_trace_begin("apumdw:cmd_release|c:0x%llx", c->kid);
	if (c->del_internal)
		c->del_internal(c);
	mdw_cmd_unvoke_map(c);
	mdw_cmd_delete_infos(c->mpriv, c);
	mdw_mem_put(c->mpriv, c->exec_infos);
	kfree(c->adj_matrix);
	kfree(c->ksubcmds);
	kfree(c->subcmds);
	kfree(c);

	mpriv->put(mpriv);
	mdw_trace_end();
}

void mdw_cmd_put(struct mdw_cmd *c)
{
	kref_put(&c->ref, mdw_cmd_release);
}

static void mdw_cmd_get(struct mdw_cmd *c)
{
	kref_get(&c->ref);
}

void mdw_cmd_delete(struct mdw_cmd *c)
{
	mdw_cmd_show(c, mdw_drv_debug);
	mdw_cmd_put(c);
}

static void mdw_cmd_delete_async(struct mdw_cmd *c)
{
	struct mdw_device *mdev = c->mpriv->mdev;

	/* add cmd list to delete */
	mutex_lock(&mdev->c_mtx);
	list_add_tail(&c->d_node, &mdev->d_cmds);
	mutex_unlock(&mdev->c_mtx);

	schedule_work(&mdev->c_wk);
}

static int mdw_cmd_sanity_check(struct mdw_cmd *c)
{
	if (c->priority >= MDW_PRIORITY_MAX ||
		c->num_subcmds > MDW_SUBCMD_MAX ||
		c->num_links > c->num_subcmds) {
		mdw_drv_err("s(0x%llx)cmd invalid(0x%llx/0x%llx)(%u/%u/%u)\n",
			(uint64_t)c->mpriv, c->uid, c->kid,
			c->priority, c->num_subcmds, c->num_links);
		return -EINVAL;
	}

	if (c->exec_infos->size != sizeof(struct mdw_cmd_exec_info) +
		c->num_subcmds * sizeof(struct mdw_subcmd_exec_info)) {
		mdw_drv_err("s(0x%llx)cmd invalid(0x%llx/0x%llx) einfo(%u/%u)\n",
			(uint64_t)c->mpriv, c->uid, c->kid,
			c->exec_infos->size,
			sizeof(struct mdw_cmd_exec_info) +
			c->num_subcmds * sizeof(struct mdw_subcmd_exec_info));
		return -EINVAL;
	}

	return 0;
}

static int mdw_cmd_adj_check(struct mdw_cmd *c)
{
	uint32_t i = 0, j = 0;

	for (i = 0; i < c->num_subcmds; i++) {
		for (j = 0; j < c->num_subcmds; j++) {
			if (i == j) {
				c->adj_matrix[i * c->num_subcmds + j] = 0;
				continue;
			}

			if (i < j)
				continue;

			if (!c->adj_matrix[i * c->num_subcmds + j] ||
				!c->adj_matrix[i + j * c->num_subcmds])
				continue;

			mdw_drv_err("s(0x%llx)c(0x%llx/0x%llx) adj matrix(%u/%u) fail\n",
				(uint64_t)c->mpriv, c->uid, c->kid, i, j);
			return -EINVAL;
		}
	}

	return 0;
}

static int mdw_cmd_link_check(struct mdw_cmd *c)
{
	uint32_t i = 0;

	for (i = 0; i < c->num_links; i++) {
		if (c->links[i].producer_idx > c->num_subcmds ||
			c->links[i].consumer_idx > c->num_subcmds ||
			!c->links[i].x || !c->links[i].y ||
			!c->links[i].va) {
			mdw_drv_err("link(%u) invalid(%u/%u)(%u/%u)(0x%llx)\n",
				c->links[i].producer_idx,
				c->links[i].consumer_idx,
				c->links[i].x,
				c->links[i].y,
				c->links[i].va);
			return -EINVAL;
		}
	}
	return 0;
}

static int mdw_cmd_sc_sanity_check(struct mdw_cmd *c)
{
	unsigned int i = 0;

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

static int mdw_cmd_run(struct mdw_fpriv *mpriv, struct mdw_cmd *c)
{
	struct mdw_device *mdev = mpriv->mdev;
	struct dma_fence *f = &c->fence->base_fence;
	int ret = 0;

	mdw_cmd_show(c, mdw_cmd_debug);

	c->start_ts = sched_clock();
	ret = mdev->dev_funcs->run_cmd(mpriv, c);
	if (ret) {
		mdw_drv_err("s(0x%llx) run cmd(0x%llx) fail(%d)\n",
			(uint64_t) c->mpriv, c->kid, ret);
		dma_fence_set_error(f, ret);
		if (dma_fence_signal(f)) {
			mdw_drv_warn("c(0x%llx) signal fence fail\n", (uint64_t)c);
			if (f->ops->get_timeline_name && f->ops->get_driver_name) {
				mdw_drv_warn(" fence name(%s-%s)\n",
				f->ops->get_driver_name(f), f->ops->get_timeline_name(f));
			}
		}
		dma_fence_put(f);
	} else {
		mdw_flw_debug("s(0x%llx) cmd(0x%llx) run\n",
			(uint64_t)c->mpriv, c->kid);
	}

	return ret;
}

static void mdw_cmd_check_rets(struct mdw_cmd *c, int ret)
{
	uint32_t idx = 0, is_dma = 0;
	DECLARE_BITMAP(tmp, 64);

	memcpy(&tmp, &c->einfos->c.sc_rets, sizeof(c->einfos->c.sc_rets));

	/* extract fail subcmd */
	do {
		idx = find_next_bit((unsigned long *)&tmp, c->num_subcmds, idx);
		if (idx >= c->num_subcmds)
			break;

		mdw_drv_warn("sc(0x%llx-#%u) type(%u) softlimit(%u) boost(%u) fail\n",
			c->kid, idx, c->subcmds[idx].type,
			c->softlimit, c->subcmds[idx].boost);
		if (c->subcmds[idx].type == APUSYS_DEVICE_EDMA)
			is_dma++;

		idx++;
	} while (idx < c->num_subcmds);

	/* trigger exception if dma */
	if (is_dma) {
		dma_exception("dma exec fail:%s:ret(%d/0x%llx)pid(%d/%d)c(0x%llx)\n",
			c->comm, ret, c->einfos->c.sc_rets,
			c->pid, c->tgid, c->kid);
	}
}

static int mdw_cmd_complete(struct mdw_cmd *c, int ret)
{
	struct dma_fence *f = &c->fence->base_fence;
	struct mdw_fpriv *mpriv = c->mpriv;

	mdw_trace_begin("apumdw:cmd_complete|cmd:0x%llx/0x%llx", c->uid, c->kid);
	mutex_lock(&c->mtx);

	c->end_ts = sched_clock();
	c->einfos->c.total_us = (c->end_ts - c->start_ts) / 1000;
	mdw_flw_debug("s(0x%llx) c(%s/0x%llx/0x%llx/0x%llx) ret(%d) sc_rets(0x%llx) complete, pid(%d/%d)(%d)\n",
		(uint64_t)mpriv, c->comm, c->uid, c->kid, c->rvid,
		ret, c->einfos->c.sc_rets,
		c->pid, c->tgid, task_pid_nr(current));

	/* check subcmds return value */
	if (c->einfos->c.sc_rets) {
		if (!ret)
			ret = -EIO;

		mdw_cmd_check_rets(c, ret);
	}
	c->einfos->c.ret = ret;

	if (ret) {
		mdw_drv_err("s(0x%llx) c(%s/0x%llx/0x%llx/0x%llx) ret(%d/0x%llx) time(%llu) pid(%d/%d)\n",
			(uint64_t)mpriv, c->comm, c->uid, c->kid, c->rvid,
			ret, c->einfos->c.sc_rets,
			c->einfos->c.total_us, c->pid, c->tgid);
		dma_fence_set_error(f, ret);

		if (mdw_debug_on(MDW_DBG_EXP))
			mdw_exception("exec fail:%s:ret(%d/0x%llx)pid(%d/%d)\n",
				c->comm, ret, c->einfos->c.sc_rets, c->pid, c->tgid);
	} else {
		mdw_flw_debug("s(0x%llx) c(%s/0x%llx/0x%llx/0x%llx) ret(%d/0x%llx) time(%llu) pid(%d/%d)\n",
			(uint64_t)mpriv, c->comm, c->uid, c->kid, c->rvid,
			ret, c->einfos->c.sc_rets,
			c->einfos->c.total_us, c->pid, c->tgid);
	}

	mdw_cmd_cmdbuf_out(mpriv, c);

	/* signal done */
	c->fence = NULL;
	atomic_dec(&c->is_running);
	if (dma_fence_signal(f)) {
		mdw_drv_warn("c(0x%llx) signal fence fail\n", (uint64_t)c);
		if (f->ops->get_timeline_name && f->ops->get_driver_name) {
			mdw_drv_warn(" fence name(%s-%s)\n",
			f->ops->get_driver_name(f), f->ops->get_timeline_name(f));
		}
	}
	dma_fence_put(f);
	atomic_dec(&mpriv->active_cmds);
	mutex_unlock(&c->mtx);

	/* check mpriv to clean cmd */
	mutex_lock(&mpriv->mtx);
	mdw_cmd_mpriv_release(mpriv);
	mutex_unlock(&mpriv->mtx);

	/* put cmd execution ref */
	mdw_cmd_put(c);
	mdw_trace_end();

	return 0;
}

static void mdw_cmd_trigger_func(struct work_struct *wk)
{
	struct mdw_cmd *c =
		container_of(wk, struct mdw_cmd, t_wk);
	int ret = 0;

	if (c->wait_fence) {
		dma_fence_wait(c->wait_fence, false);
		dma_fence_put(c->wait_fence);
	}

	mdw_flw_debug("s(0x%llx) c(0x%llx) wait fence done, start run\n",
		(uint64_t)c->mpriv, c->kid);
	mutex_lock(&c->mtx);
	ret = mdw_cmd_run(c->mpriv, c);
	mutex_unlock(&c->mtx);

	/* put cmd execution ref */
	if (ret) {
		atomic_dec(&c->is_running);
		mdw_cmd_put(c);
	}
}

static struct mdw_cmd *mdw_cmd_create(struct mdw_fpriv *mpriv,
	union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;

	mdw_trace_begin("apumdw:cmd_create|s:0x%llx", (uint64_t)mpriv);

	/* check num subcmds maximum */
	if (in->exec.num_subcmds > MDW_SUBCMD_MAX) {
		mdw_drv_err("too much subcmds(%u)\n", in->exec.num_subcmds);
		goto out;
	}

	/* alloc mdw cmd */
	c = kzalloc(sizeof(*c), GFP_KERNEL);
	if (!c)
		goto out;

	mutex_init(&c->mtx);
	INIT_LIST_HEAD(&c->map_invokes);
	c->mpriv = mpriv;
	atomic_set(&c->is_running, 0);

	/* setup cmd info */
	c->pid = task_pid_nr(current);
	c->tgid = task_tgid_nr(current);
	c->kid = (uint64_t)c;
	c->uid = in->exec.uid;
	get_task_comm(c->comm, current);
	c->priority = in->exec.priority;
	c->hardlimit = in->exec.hardlimit;
	c->softlimit = in->exec.softlimit;
	c->power_save = in->exec.power_save;
	c->power_plcy = in->exec.power_plcy;
	c->power_dtime = in->exec.power_dtime;
	c->fastmem_ms = in->exec.fastmem_ms;
	c->app_type = in->exec.app_type;
	c->num_subcmds = in->exec.num_subcmds;
	c->num_links = in->exec.num_links;
	c->exec_infos = mdw_mem_get(mpriv, in->exec.exec_infos);
	if (!c->exec_infos) {
		mdw_drv_err("get exec info fail\n");
		goto free_cmd;
	}

	/* check input params */
	if (mdw_cmd_sanity_check(c)) {
		mdw_drv_err("cmd sanity check fail\n");
		goto put_execinfos;
	}

	/* subcmds/ksubcmds */
	c->subcmds = kzalloc(c->num_subcmds * sizeof(*c->subcmds), GFP_KERNEL);
	if (!c->subcmds)
		goto put_execinfos;
	if (copy_from_user(c->subcmds, (void __user *)in->exec.subcmd_infos,
		c->num_subcmds * sizeof(*c->subcmds))) {
		mdw_drv_err("copy subcmds fail\n");
		goto free_subcmds;
	}
	if (mdw_cmd_sc_sanity_check(c)) {
		mdw_drv_err("sc sanity check fail\n");
		goto free_subcmds;
	}

	c->ksubcmds = kzalloc(c->num_subcmds * sizeof(*c->ksubcmds),
		GFP_KERNEL);
	if (!c->ksubcmds)
		goto free_subcmds;

	/* adj matrix */
	c->adj_matrix = kzalloc(c->num_subcmds *
		c->num_subcmds * sizeof(uint8_t), GFP_KERNEL);
	if (!c->adj_matrix)
		goto free_ksubcmds;
	if (copy_from_user(c->adj_matrix, (void __user *)in->exec.adj_matrix,
		(c->num_subcmds * c->num_subcmds * sizeof(uint8_t)))) {
		mdw_drv_err("copy adj matrix fail\n");
		goto free_adj;
	}
	if (g_mdw_klog & MDW_DBG_CMD) {
		print_hex_dump(KERN_INFO, "[apusys] adj matrix: ",
			DUMP_PREFIX_OFFSET, 16, 1, c->adj_matrix,
			c->num_subcmds * c->num_subcmds, 0);
	}
	if (mdw_cmd_adj_check(c))
		goto free_adj;

	/* link */
	if (c->num_links) {
		c->links = kcalloc(c->num_links, sizeof(*c->links), GFP_KERNEL);
		if (!c->links)
			goto free_adj;
		if (copy_from_user(c->links, (void __user *)in->exec.links,
			c->num_links * sizeof(*c->links))) {
			mdw_drv_err("copy links fail\n");
			goto free_link;
		}
	}
	if (mdw_cmd_link_check(c))
		goto free_link;

	/* create infos */
	if (mdw_cmd_create_infos(mpriv, c)) {
		mdw_drv_err("create cmd info fail\n");
		goto free_link;
	}

	c->mpriv->get(c->mpriv);
	c->complete = mdw_cmd_complete;
	INIT_WORK(&c->t_wk, &mdw_cmd_trigger_func);
	kref_init(&c->ref);
	mdw_cmd_show(c, mdw_drv_debug);

	goto out;

free_link:
	kfree(c->links);
free_adj:
	kfree(c->adj_matrix);
free_ksubcmds:
	kfree(c->ksubcmds);
free_subcmds:
	kfree(c->subcmds);
put_execinfos:
	mdw_mem_put(mpriv, c->exec_infos);
free_cmd:
	kfree(c);
	c = NULL;
out:
	mdw_trace_end();
	return c;
}

static int mdw_cmd_ioctl_del(struct mdw_fpriv *mpriv, union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL;
	int ret = 0;

	mdw_trace_begin("apumdw:user_delete");

	mutex_lock(&mpriv->mtx);
	c = (struct mdw_cmd *)idr_find(&mpriv->cmds, in->id);
	if (!c) {
		ret = -EINVAL;
		mdw_drv_warn("can't find id(%d)\n", in->id);
	} else {
		if (c != idr_remove(&mpriv->cmds, c->id))
			mdw_drv_warn("remove cmd idr conflict(0x%llx/%d)\n", c->kid, c->id);
		mdw_cmd_delete(c);
	}
	mutex_unlock(&mpriv->mtx);

	mdw_trace_end();

	return ret;
}

static int mdw_cmd_ioctl_run(struct mdw_fpriv *mpriv, union mdw_cmd_args *args)
{
	struct mdw_cmd_in *in = (struct mdw_cmd_in *)args;
	struct mdw_cmd *c = NULL, *priv_c = NULL;
	struct sync_file *sync_file = NULL;
	int ret = 0, fd = 0, wait_fd = 0, is_running = 0;

	mdw_trace_begin("apumdw:user_run");

	/* get wait fd */
	wait_fd = in->exec.fence;

	mutex_lock(&mpriv->mtx);
	/* get stale cmd */
	c = (struct mdw_cmd *)idr_find(&mpriv->cmds, in->id);
	if (!c) {
		/* no stale cmd, create cmd */
		mdw_cmd_debug("s(0x%llx) create new\n", (uint64_t)mpriv);
	} else if (in->op == MDW_CMD_IOCTL_RUN_STALE) {
		is_running = atomic_read(&c->is_running);
		if (is_running) {
			mdw_drv_err("s(0x%llx) c(0x%llx) is running(%d), can't execute again\n",
				(uint64_t)mpriv, (uint64_t)c, is_running);
			ret = -ETXTBSY;
			goto out;
		}
		/* run stale cmd */
		mdw_cmd_debug("s(0x%llx) run stale(0x%llx)\n",
			(uint64_t)mpriv, (uint64_t)c);
		goto exec;
	} else {
		/* release stale cmd and create new */
		mdw_cmd_debug("s(0x%llx) delete stale(0x%llx) and create new\n",
			(uint64_t)mpriv, (uint64_t)c);
		priv_c = c;
		c = NULL;
		if (priv_c != idr_remove(&mpriv->cmds, priv_c->id)) {
			mdw_drv_warn("remove cmd idr conflict(0x%llx/%d)\n",
				priv_c->kid, priv_c->id);
		}
	}

	/* create cmd */
	c = mdw_cmd_create(mpriv, args);
	if (!c) {
		mdw_drv_err("create cmd fail\n");
		ret = -EINVAL;
		goto out;
	}
	memset(args, 0, sizeof(*args));

	/* alloc idr */
	c->id = idr_alloc(&mpriv->cmds, c, MDW_CMD_IDR_MIN, MDW_CMD_IDR_MAX, GFP_KERNEL);
	if (c->id < MDW_CMD_IDR_MIN) {
		mdw_drv_err("alloc idr fail(%d)\n", c->id);
		goto delete_cmd;
	}

exec:
	mutex_lock(&c->mtx);
	mdw_cmd_trace(c, MDW_CMD_ENQUE);
	/* get sync_file fd */
	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		mdw_drv_err("get unused fd fail\n");
		ret = -EINVAL;
		goto delete_idr;
	}
	if (mdw_fence_init(c, fd)) {
		mdw_drv_err("cmd init fence fail\n");
		goto put_fd;
	}
	sync_file = sync_file_create(&c->fence->base_fence);
	if (!sync_file) {
		mdw_drv_err("create sync file fail\n");
		dma_fence_put(&c->fence->base_fence);
		ret = -ENOMEM;
		goto put_fd;
	}

	/* get cmd execution ref */
	atomic_inc(&c->is_running);
	mdw_cmd_get(c);

	/* check wait fence from other module */
	mdw_flw_debug("s(0x%llx)c(0x%llx) wait fence(%d)...\n",
			(uint64_t)c->mpriv, c->kid, wait_fd);
	c->wait_fence = sync_file_get_fence(wait_fd);
	if (!c->wait_fence) {
		mdw_flw_debug("s(0x%llx)c(0x%llx) no wait fence, trigger directly\n",
			(uint64_t)c->mpriv, c->kid);
		ret = mdw_cmd_run(mpriv, c);
	} else {
		/* wait fence from wq */
		schedule_work(&c->t_wk);
	}

	if (ret) {
		/* put cmd execution ref */
		atomic_dec(&c->is_running);
		mdw_cmd_put(c);
		goto put_file;
	}

	/* assign fd */
	fd_install(fd, sync_file->file);

	/* get ref for cmd exec */
	atomic_inc(&mpriv->active_cmds);

	/* return fd */
	args->out.exec.fence = fd;
	args->out.exec.id = c->id;
	mdw_flw_debug("async fd(%d) id(%d)\n", fd, c->id);
	mutex_unlock(&c->mtx);
	goto out;

put_file:
	fput(sync_file->file);
put_fd:
	put_unused_fd(fd);
delete_idr:
	if (c != idr_remove(&mpriv->cmds, c->id))
		mdw_drv_warn("remove cmd idr conflict(0x%llx/%d)\n", c->kid, c->id);
	mutex_unlock(&c->mtx);
delete_cmd:
	mdw_cmd_delete(c);
out:
	mutex_unlock(&mpriv->mtx);
	if (priv_c)
		mdw_cmd_delete_async(priv_c);

	mdw_trace_end();

	return ret;
}

int mdw_cmd_ioctl(struct mdw_fpriv *mpriv, void *data)
{
	union mdw_cmd_args *args = (union mdw_cmd_args *)data;
	int ret = 0;

	mdw_flw_debug("s(0x%llx) op::%d\n", (uint64_t)mpriv, args->in.op);

	switch (args->in.op) {
	case MDW_CMD_IOCTL_RUN:
	case MDW_CMD_IOCTL_RUN_STALE:
		ret = mdw_cmd_ioctl_run(mpriv, args);
		break;
	case MDW_CMD_IOCTL_DEL:
		ret = mdw_cmd_ioctl_del(mpriv, args);
		break;

	default:
		ret = -EINVAL;
		break;
	}
	mdw_flw_debug("done\n");

	return ret;
}
