/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include "vpu_pool.h"
#include "vpu_cmn.h"

void vpu_pool_init(struct vpu_pool *p, const char *name, int type)
{
	if (name)
		strlcpy(p->name, name, sizeof(p->name));
	else
		p->name[0] = 0;

	LOG_DBG("%s: \"%s\"\n", __func__, p->name);

	INIT_LIST_HEAD(&p->pool);
	mutex_init(&p->lock);
	p->size = 0;
	p->type = type;
	p->priv = NULL;
}

int vpu_pool_size(struct vpu_pool *p)
{
	int ret;

	mutex_lock(&p->lock);
	ret = p->size;
	mutex_unlock(&p->lock);

	return ret;
}

int vpu_pool_is_empty(struct vpu_pool *p)
{
	int ret;

	mutex_lock(&p->lock);
	ret = list_empty(&p->pool);
	mutex_unlock(&p->lock);

	return ret;
}

static int vpu_pool_enqueue_dep(struct vpu_pool *p, struct vpu_request *req)
{
	int ret = 0;
	struct vpu_pool_dep *dep, *n;

	WARN_ON(!(req->requested_core & VPU_CORE_MULTIPROC));

	mutex_lock(&p->lock);

	/* Match & insert to sub-pool */
	list_for_each_entry_safe(dep, n, &p->pool, link) {
		if (dep->user != req->user_id)
			continue;
		if (dep->tail_id == req->request_id)
			goto insert_out;
	}

	/* Allocate & init new sub-pool */
	dep = kzalloc(sizeof(struct vpu_pool_dep), GFP_KERNEL);

	if (!dep) {
		ret = -ENOMEM;
		goto out;
	}

	dep->head_id = req->request_id;
	dep->user = req->user_id;
	INIT_LIST_HEAD(&dep->sub_pool);
	list_add_tail(&dep->link, &p->pool);

	LOG_DBG("%s: %s: NEW dep: %p, user: %p, head: %p\n",
			__func__,
			p->name,
			dep,
			dep->user,
			dep->head_id);

insert_out:
	list_add_tail(vlist_link(req, struct vpu_request), &dep->sub_pool);
	p->size++;
	/**
	 * The sub-pool is ready for processing, if the last
	 * dependency request is enqueued, Eq. tail_id = 0
	 */
	dep->tail_id = req->next_req_id;

	LOG_DBG("%s: %s: user: %p, head: %p, tail: %p, req: %p\n",
		__func__,
		p->name,
		dep->user,
		dep->head_id,
		dep->tail_id,
		req->request_id);

out:
	mutex_unlock(&p->lock);
	return ret;
}

static struct vpu_request *vpu_pool_dequeue_dep(struct vpu_pool *p)
{
	struct vpu_request *req = NULL;
	struct vpu_pool_dep *dep, *n;

	mutex_lock(&p->lock);
	if (list_empty(&p->pool))
		goto out;

	/* Get next request from current active sub-pool */
	dep = (struct vpu_pool_dep *)p->priv;

	if (dep) {
		if (!list_empty(&dep->sub_pool)) {
			req = vlist_node_of(dep->sub_pool.next,
				struct vpu_request);
			goto got_req;
		}
	}

	/* Get head request from the first ready sub-pool */
	list_for_each_entry_safe(dep, n, &p->pool, link) {
		if (!dep)
			break;
		if (dep->tail_id)  /* Skip non-ready sub-pools */
			continue;
		if (!list_empty(&dep->sub_pool)) {
			p->priv = dep; /* mark active */
			req = vlist_node_of(dep->sub_pool.next,
				struct vpu_request);
			break;
		}
	}

got_req:
	if (req) {
		list_del_init(vlist_link(req, struct vpu_request));

		LOG_DBG("%s: %s: user: %p, head: %p, tail: %p, req: %p\n",
			__func__,
			p->name,
			dep->user,
			dep->head_id,
			dep->tail_id,
			req->request_id);

		/* free & remove the sub-pool, if it's empty */
		if (list_empty(&dep->sub_pool)) {
			p->priv = NULL; /* remove active */
			WARN_ON(dep->tail_id != 0);
			list_del_init(&dep->link);
			LOG_DBG("%s: %s: FREE dep: user: %p, head: %p\n",
				__func__,
				p->name,
				dep->user,
				dep->head_id);
			kfree(dep);
		}
		p->size--;
	}

	LOG_DBG("flag - 3: get %s pool, size(%d)\n", p->name, p->size);
out:
	mutex_unlock(&p->lock);

	return req;
}

static int vpu_pool_enqueue_pri(struct vpu_pool *p, struct vpu_request *req,
	unsigned int *priority)
{

	LOG_DBG("%s: request_id: %p, core: %x => %s Pool\n",
		__func__,
		req->request_id,
		req->requested_core,
		p->name);

	WARN_ON(req->requested_core & VPU_CORE_MULTIPROC);

	mutex_lock(&p->lock);
	list_add_tail(vlist_link(req, struct vpu_request), &p->pool);
	p->size++;
	if (priority)
		*priority += 1;
	mutex_unlock(&p->lock);

	return 0;
}

static struct vpu_request *vpu_pool_dequeue_pri(struct vpu_pool *p,
	unsigned int *priority)
{
	struct vpu_request *req = NULL;

	mutex_lock(&p->lock);
	if (list_empty(&p->pool))
		goto out;

	req = vlist_node_of(p->pool.next, struct vpu_request);
	list_del_init(vlist_link(req, struct vpu_request));
	p->size--;
	if (priority)
		*priority -= 1;

	LOG_DBG("flag - 3: get %s pool, size(%d)\n", p->name, p->size);
out:
	mutex_unlock(&p->lock);

	if (req)
		LOG_DBG("%s: request_id: %p, core: %x => %s Pool\n",
			__func__,
			req->request_id,
			req->requested_core,
			p->name);

	return req;
}

int vpu_pool_enqueue(struct vpu_pool *p, struct vpu_request *req,
	unsigned int *priority)
{
	if (!req)
		return -ENOENT;

	if (p->type == VPU_POOL_DEP)
		return vpu_pool_enqueue_dep(p, req);

	return vpu_pool_enqueue_pri(p, req, priority);
}

struct vpu_request *vpu_pool_dequeue(struct vpu_pool *p,
	unsigned int *priority)
{
	if (p->type == VPU_POOL_DEP)
		return vpu_pool_dequeue_dep(p);

	return vpu_pool_dequeue_pri(p, priority);
}

