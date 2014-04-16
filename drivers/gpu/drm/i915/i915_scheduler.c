/*
 * Copyright (c) 2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "i915_drv.h"
#include "intel_drv.h"
#include "intel_sync.h"
#include "i915_scheduler.h"

int         i915_scheduler_fly_node(struct i915_scheduler_queue_entry *node);
int         i915_scheduler_remove_dependent(struct i915_scheduler *scheduler,
				struct i915_scheduler_queue_entry *remove);
int         i915_scheduler_submit(struct intel_engine_cs *ring,
				  bool is_locked);
int         i915_scheduler_submit_max_priority(struct intel_engine_cs *ring,
					       bool is_locked);
uint32_t    i915_scheduler_count_flying(struct i915_scheduler *scheduler,
					struct intel_engine_cs *ring);
void        i915_scheduler_priority_bump_clear(struct i915_scheduler *scheduler);
int         i915_scheduler_priority_bump(struct i915_scheduler *scheduler,
				struct i915_scheduler_queue_entry *target,
				uint32_t bump);
void        i915_scheduler_file_queue_inc(struct drm_file *file);
void        i915_scheduler_file_queue_dec(struct drm_file *file);

bool i915_scheduler_is_enabled(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;

	return dev_priv->scheduler != NULL;
}

int i915_scheduler_init(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_scheduler   *scheduler = dev_priv->scheduler;
	int                     r;

	if (scheduler)
		return 0;

	scheduler = kzalloc(sizeof(*scheduler), GFP_KERNEL);
	if (!scheduler)
		return -ENOMEM;

	spin_lock_init(&scheduler->lock);

	for (r = 0; r < I915_NUM_RINGS; r++)
		INIT_LIST_HEAD(&scheduler->node_queue[r]);

	scheduler->index = 1;

	/* Default tuning values: */
	scheduler->priority_level_max     = ~0U;
	scheduler->priority_level_preempt = 900;
	scheduler->min_flying             = 2;
	scheduler->file_queue_max         = 64;

	dev_priv->scheduler = scheduler;

	return 0;
}

int i915_scheduler_queue_execbuffer(struct i915_scheduler_queue_entry *qe)
{
	struct drm_i915_private *dev_priv = qe->params.dev->dev_private;
	struct i915_scheduler   *scheduler = dev_priv->scheduler;
	struct intel_engine_cs  *ring = qe->params.ring;
	struct i915_scheduler_queue_entry  *node;
	struct i915_scheduler_queue_entry  *test;
	struct timespec     stamp;
	unsigned long       flags;
	bool                not_flying, found;
	int                 i, j, r, got_batch = 0;
	int                 incomplete = 0;

	BUG_ON(!scheduler);

	if (i915.scheduler_override & i915_so_direct_submit) {
		int ret;

		qe->scheduler_index = scheduler->index++;

		trace_i915_scheduler_queue(qe->params.ring, qe);

		scheduler->flags[qe->params.ring->id] |= i915_sf_submitting;
		ret = dev_priv->gt.do_execfinal(&qe->params);
		scheduler->flags[qe->params.ring->id] &= ~i915_sf_submitting;

		/* Need to release the objects: */
		for (i = 0; i < qe->num_objs; i++) {
			if (!qe->saved_objects[i].obj)
				continue;

			drm_gem_object_unreference(&qe->saved_objects[i].obj->base);
		}

		kfree(qe->saved_objects);
		qe->saved_objects = NULL;
		qe->num_objs = 0;

		/* Free the context object too: */
		if (qe->params.ctx)
			i915_gem_context_unreference(qe->params.ctx);

		/* And anything else owned by the QE structure: */
		kfree(qe->params.cliprects);
		if (qe->params.dispatch_flags & I915_DISPATCH_SECURE)
			i915_gem_execbuff_release_batch_obj(qe->params.batch_obj);

#ifdef CONFIG_SYNC
		if (qe->params.fence_wait)
			sync_fence_put(qe->params.fence_wait);
#endif

		return ret;
	}

	getrawmonotonic(&stamp);

	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	*node = *qe;
	INIT_LIST_HEAD(&node->link);
	node->status = i915_sqs_queued;
	node->stamp  = stamp;
	i915_gem_request_reference(node->params.request);

	BUG_ON(node->params.request->scheduler_qe);
	node->params.request->scheduler_qe = node;

	/*
	 * Verify that the batch buffer itself is included in the object list.
	 */
	for (i = 0; i < node->num_objs; i++) {
		if (node->saved_objects[i].obj == node->params.batch_obj)
			got_batch++;
	}

	BUG_ON(got_batch != 1);

	/* Need to determine the number of incomplete entries in the list as
	 * that will be the maximum size of the dependency list.
	 *
	 * Note that the allocation must not be made with the spinlock acquired
	 * as kmalloc can sleep. However, the unlock/relock is safe because no
	 * new entries can be queued up during the unlock as the i915 driver
	 * mutex is still held. Entries could be removed from the list but that
	 * just means the dep_list will be over-allocated which is fine.
	 */
	spin_lock_irqsave(&scheduler->lock, flags);
	for (r = 0; r < I915_NUM_RINGS; r++) {
		list_for_each_entry(test, &scheduler->node_queue[r], link) {
			if (I915_SQS_IS_COMPLETE(test))
				continue;

			incomplete++;
		}
	}

	/* Temporarily unlock to allocate memory: */
	spin_unlock_irqrestore(&scheduler->lock, flags);
	if (incomplete) {
		node->dep_list = kmalloc(sizeof(node->dep_list[0]) * incomplete,
					 GFP_KERNEL);
		if (!node->dep_list) {
			kfree(node);
			return -ENOMEM;
		}
	} else
		node->dep_list = NULL;

	spin_lock_irqsave(&scheduler->lock, flags);
	node->num_deps = 0;

	if (node->dep_list) {
		for (r = 0; r < I915_NUM_RINGS; r++) {
			list_for_each_entry(test, &scheduler->node_queue[r], link) {
				if (I915_SQS_IS_COMPLETE(test))
					continue;

				found = (node->params.ctx == test->params.ctx);

				for (i = 0; (i < node->num_objs) && !found; i++) {
					for (j = 0; j < test->num_objs; j++) {
						if (node->saved_objects[i].obj !=
							    test->saved_objects[j].obj)
							continue;

						found = true;
						break;
					}
				}

				if (found) {
					node->dep_list[node->num_deps] = test;
					node->num_deps++;
				}
			}
		}

		BUG_ON(node->num_deps > incomplete);
	}

	if (node->priority && node->num_deps) {
		i915_scheduler_priority_bump_clear(scheduler);

		for (i = 0; i < node->num_deps; i++)
			i915_scheduler_priority_bump(scheduler,
					node->dep_list[i], node->priority);
	}

	node->scheduler_index = scheduler->index++;

	list_add_tail(&node->link, &scheduler->node_queue[ring->id]);

	i915_scheduler_file_queue_inc(node->params.file);

	if (i915.scheduler_override & i915_so_submit_on_queue)
		not_flying = true;
	else
		not_flying = i915_scheduler_count_flying(scheduler, ring) <
							 scheduler->min_flying;

	trace_i915_scheduler_queue(ring, node);
	trace_i915_scheduler_node_state_change(ring, node);

	spin_unlock_irqrestore(&scheduler->lock, flags);

	if (not_flying)
		i915_scheduler_submit(ring, true);

	return 0;
}

int i915_scheduler_fly_node(struct i915_scheduler_queue_entry *node)
{
	struct drm_i915_private *dev_priv = node->params.dev->dev_private;
	struct i915_scheduler   *scheduler = dev_priv->scheduler;
	struct intel_engine_cs  *ring;

	BUG_ON(!scheduler);
	BUG_ON(!node);
	BUG_ON(node->status != i915_sqs_popped);

	ring = node->params.ring;

	/* Add the node (which should currently be in state none) to the front
	 * of the queue. This ensure that flying nodes are always held in
	 * hardware submission order. */
	list_add(&node->link, &scheduler->node_queue[ring->id]);

	node->status = i915_sqs_flying;

	trace_i915_scheduler_fly(ring, node);
	trace_i915_scheduler_node_state_change(ring, node);

	if (!(scheduler->flags[ring->id] & i915_sf_interrupts_enabled)) {
		bool    success = true;

		success = ring->irq_get(ring);
		if (success)
			scheduler->flags[ring->id] |= i915_sf_interrupts_enabled;
		else
			return -EINVAL;
	}

	return 0;
}

/*
 * Nodes are considered valid dependencies if they are queued on any ring or
 * if they are in flight on a different ring. In flight on the same ring is no
 * longer interesting for non-premptive nodes as the ring serialises execution.
 * For pre-empting nodes, all in flight dependencies are valid as they must not
 * be jumped by the act of pre-empting.
 *
 * Anything that is neither queued nor flying is uninteresting.
 */
static inline bool i915_scheduler_is_dependency_valid(
			struct i915_scheduler_queue_entry *node, uint32_t idx)
{
	struct i915_scheduler_queue_entry *dep;

	dep = node->dep_list[idx];
	if (!dep)
		return false;

	if (I915_SQS_IS_QUEUED(dep))
		return true;

	if (I915_SQS_IS_FLYING(dep)) {
		if (node->params.ring != dep->params.ring)
			return true;
	}

	return false;
}

uint32_t i915_scheduler_count_flying(struct i915_scheduler *scheduler,
				     struct intel_engine_cs *ring)
{
	struct i915_scheduler_queue_entry *node;
	uint32_t                          flying = 0;

	list_for_each_entry(node, &scheduler->node_queue[ring->id], link)
		if (I915_SQS_IS_FLYING(node))
			flying++;

	return flying;
}

/* Add a popped node back in to the queue. For example, because the ring was
 * hung when execfinal() was called and thus the ring submission needs to be
 * retried later. */
static void i915_scheduler_node_requeue(struct i915_scheduler_queue_entry *node)
{
	BUG_ON(!node);
	BUG_ON(!I915_SQS_IS_FLYING(node));

	node->status = i915_sqs_queued;
	trace_i915_scheduler_unfly(node->params.ring, node);
	trace_i915_scheduler_node_state_change(node->params.ring, node);
}

/* Give up on a popped node completely. For example, because it is causing the
 * ring to hang or is using some resource that no longer exists. */
static void i915_scheduler_node_kill(struct i915_scheduler_queue_entry *node)
{
	BUG_ON(!node);
	BUG_ON(!I915_SQS_IS_FLYING(node));

	node->status = i915_sqs_dead;
	trace_i915_scheduler_unfly(node->params.ring, node);
	trace_i915_scheduler_node_state_change(node->params.ring, node);
}

/* Abandon a queued node completely. For example because the driver is being
 * reset and it is not valid to preserve absolutely any state at all across the
 * reinitialisation sequence. */
static void i915_scheduler_node_kill_queued(struct i915_scheduler_queue_entry *node)
{
	BUG_ON(!node);
	BUG_ON(!I915_SQS_IS_QUEUED(node));

	node->status = i915_sqs_dead;
	trace_i915_scheduler_node_state_change(node->params.ring, node);
}

/* The system is toast. Terminate all nodes with extreme prejudice. */
void i915_scheduler_kill_all(struct drm_device *dev)
{
	struct i915_scheduler_queue_entry   *node;
	struct drm_i915_private             *dev_priv = dev->dev_private;
	struct i915_scheduler               *scheduler = dev_priv->scheduler;
	unsigned long   flags;
	int             r;

	spin_lock_irqsave(&scheduler->lock, flags);

	for (r = 0; r < I915_NUM_RINGS; r++) {
		list_for_each_entry(node, &scheduler->node_queue[r], link) {
			switch (node->status) {
			case I915_SQS_CASE_COMPLETE:
			break;

			case I915_SQS_CASE_FLYING:
				i915_scheduler_node_kill(node);
			break;

			case I915_SQS_CASE_QUEUED:
				i915_scheduler_node_kill_queued(node);
			break;

			default:
				/* Wot no state?! */
				BUG();
			}
		}
	}

	spin_unlock_irqrestore(&scheduler->lock, flags);

	queue_work(dev_priv->wq, &dev_priv->mm.scheduler_work);
}

/*
 * The batch tagged with the indicated seqence number has completed.
 * Search the queue for it, update its status and those of any batches
 * submitted earlier, which must also have completed or been preeempted
 * as appropriate.
 *
 * Called with spinlock already held.
 */
static void i915_scheduler_seqno_complete(struct intel_engine_cs *ring, uint32_t seqno)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	struct i915_scheduler   *scheduler = dev_priv->scheduler;
	struct i915_scheduler_queue_entry *node;
	bool got_changes = false;

	/*
	 * Batch buffers are added to the head of the list in execution order,
	 * thus seqno values, although not necessarily incrementing, will be
	 * met in completion order when scanning the list. So when a match is
	 * found, all subsequent entries must have also popped out. Conversely,
	 * if a completed entry is found then there is no need to scan further.
	 */
	list_for_each_entry(node, &scheduler->node_queue[ring->id], link) {
		if (I915_SQS_IS_COMPLETE(node)) {
			trace_i915_scheduler_landing(ring, seqno, node);
			return;
		}

		if (seqno == node->params.request->seqno)
			break;
	}

	trace_i915_scheduler_landing(ring, seqno, node);

	/*
	 * NB: Lots of extra seqnos get added to the ring to track things
	 * like cache flushes and page flips. So don't complain about if
	 * no node was found.
	 */
	if (&node->link == &scheduler->node_queue[ring->id])
		return;

	BUG_ON(!I915_SQS_IS_FLYING(node));

	/* Everything from here can be marked as done: */
	list_for_each_entry_from(node, &scheduler->node_queue[ring->id], link) {
		/* Check if the marking has already been done: */
		if (I915_SQS_IS_COMPLETE(node))
			break;

		if (!I915_SQS_IS_FLYING(node))
			continue;

		/* Node was in flight so mark it as complete. */
		node->status = i915_sqs_complete;
		trace_i915_scheduler_node_state_change(ring, node);
		got_changes = true;
	}

	/* Should submit new work here if flight list is empty but the DRM
	 * mutex lock might not be available if a '__wait_request()' call is
	 * blocking the system. */

	 /* Avoid race conditions with the request tracking code: */
	if (got_changes)
		ring->last_read_seqno = 0;
}

int i915_scheduler_handle_irq(struct intel_engine_cs *ring)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	struct i915_scheduler   *scheduler = dev_priv->scheduler;
	unsigned long       flags;
	static uint32_t     last_seqno;
	uint32_t            seqno;

	seqno = ring->get_seqno(ring, false);

	trace_i915_scheduler_irq(ring, seqno);

	if (i915.scheduler_override & i915_so_direct_submit)
		return 0;

	if (seqno == last_seqno) {
		/* Why are there sometimes multiple interrupts per seqno? */
		return 0;
	}
	last_seqno = seqno;

	spin_lock_irqsave(&scheduler->lock, flags);
	i915_scheduler_seqno_complete(ring, seqno);
	spin_unlock_irqrestore(&scheduler->lock, flags);

	queue_work(dev_priv->wq, &dev_priv->mm.scheduler_work);

	return 0;
}

static int i915_scheduler_remove(struct intel_engine_cs *ring)
{
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	struct i915_scheduler   *scheduler = dev_priv->scheduler;
	struct i915_scheduler_queue_entry  *node, *node_next;
	unsigned long       flags;
	int                 flying = 0, queued = 0;
	int                 ret = 0;
	bool                do_submit;
	uint32_t            i, min_seqno;
	struct list_head    remove;

	if (list_empty(&scheduler->node_queue[ring->id]))
		return 0;

	spin_lock_irqsave(&scheduler->lock, flags);

	/* /i915_scheduler_dump_locked(ring, "remove/pre");/ */

	/*
	 * In the case where the system is idle, starting 'min_seqno' from a big
	 * number will cause all nodes to be removed as they are now back to
	 * being in-order. However, this will be a problem if the last one to
	 * complete was actually out-of-order as the ring seqno value will be
	 * lower than one or more completed buffers. Thus code looking for the
	 * completion of said buffers will wait forever.
	 * Instead, use the hardware seqno as the starting point. This means
	 * that some buffers might be kept around even in a completely idle
	 * system but it should guarantee that no-one ever gets confused when
	 * waiting for buffer completion.
	 */
	min_seqno = ring->get_seqno(ring, true);

	list_for_each_entry(node, &scheduler->node_queue[ring->id], link) {
		if (I915_SQS_IS_QUEUED(node))
			queued++;
		else if (I915_SQS_IS_FLYING(node))
			flying++;
		else if (I915_SQS_IS_COMPLETE(node))
			continue;

		if (node->params.request->seqno == 0)
			continue;

		if (!i915_seqno_passed(node->params.request->seqno, min_seqno))
			min_seqno = node->params.request->seqno;
	}

	INIT_LIST_HEAD(&remove);
	list_for_each_entry_safe(node, node_next, &scheduler->node_queue[ring->id], link) {
		/*
		 * Only remove completed nodes which have a lower seqno than
		 * all pending nodes. While there is the possibility of the
		 * ring's seqno counting backwards, all higher buffers must
		 * be remembered so that the 'i915_seqno_passed()' test can
		 * report that they have in fact passed.
		 *
		 * NB: This is not true for 'dead' nodes. The GPU reset causes
		 * the software seqno to restart from its initial value. Thus
		 * the dead nodes must be removed even though their seqno values
		 * are potentially vastly greater than the current ring seqno.
		 */
		if (!I915_SQS_IS_COMPLETE(node))
			continue;

		if (node->status != i915_sqs_dead) {
			if (i915_seqno_passed(node->params.request->seqno, min_seqno) &&
			    (node->params.request->seqno != min_seqno))
				continue;
		}

		list_del(&node->link);
		list_add(&node->link, &remove);

		/* Strip the dependency info while the mutex is still locked */
		i915_scheduler_remove_dependent(scheduler, node);

		/* Likewise clean up the file descriptor before it might disappear. */
		if (node->params.file) {
			i915_scheduler_file_queue_dec(node->params.file);
			node->params.file = NULL;
		}

		continue;
	}

	/*
	 * No idea why but this seems to cause problems occasionally.
	 * Note that the 'irq_put' code is internally reference counted
	 * and spin_locked so it should be safe to call.
	 */
	/*if ((scheduler->flags[ring->id] & i915_sf_interrupts_enabled) &&
	    (first_flight[ring->id] == NULL)) {
		ring->irq_put(ring);
		scheduler->flags[ring->id] &= ~i915_sf_interrupts_enabled;
	}*/

	/* Launch more packets now? */
	do_submit = (queued > 0) && (flying < scheduler->min_flying);

	trace_i915_scheduler_remove(ring, min_seqno, do_submit);

	spin_unlock_irqrestore(&scheduler->lock, flags);

	if (do_submit)
		ret = i915_scheduler_submit(ring, true);

	while (!list_empty(&remove)) {
		node = list_first_entry(&remove, typeof(*node), link);
		list_del(&node->link);

		trace_i915_scheduler_destroy(ring, node);

#ifdef CONFIG_SYNC
		if (node->params.fence_wait)
			sync_fence_put(node->params.fence_wait);
#endif

		/* The batch buffer must be unpinned before it is unreferenced
		 * otherwise the unpin fails with a missing vma!? */
		if (node->params.dispatch_flags & I915_DISPATCH_SECURE)
			i915_gem_execbuff_release_batch_obj(node->params.batch_obj);

		/* Release the locked buffers: */
		for (i = 0; i < node->num_objs; i++) {
			drm_gem_object_unreference(
					    &node->saved_objects[i].obj->base);
		}
		kfree(node->saved_objects);

		/* Context too: */
		if (node->params.ctx)
			i915_gem_context_unreference(node->params.ctx);

		/* And anything else owned by the node: */
		node->params.request->scheduler_qe = NULL;
		i915_gem_request_unreference(node->params.request);
		kfree(node->params.cliprects);
		kfree(node->dep_list);
		kfree(node);
	}

	return ret;
}

void i915_gem_scheduler_work_handler(struct work_struct *work)
{
	struct intel_engine_cs  *ring;
	struct drm_i915_private *dev_priv;
	struct drm_device       *dev;
	int                     i;

	dev_priv = container_of(work, struct drm_i915_private, mm.scheduler_work);
	dev = dev_priv->dev;

	mutex_lock(&dev->struct_mutex);

	for_each_ring(ring, dev_priv, i) {
		i915_scheduler_remove(ring);
	}

	mutex_unlock(&dev->struct_mutex);
}

int i915_scheduler_flush_request(struct drm_i915_gem_request *req,
				 bool is_locked)
{
	struct drm_i915_private            *dev_priv;
	struct i915_scheduler              *scheduler;
	unsigned long       flags;
	int                 flush_count;
	uint32_t            ring_id;

	if (!req)
		return -EINVAL;

	dev_priv  = req->ring->dev->dev_private;
	scheduler = dev_priv->scheduler;

	if (!scheduler)
		return 0;

	if (!req->scheduler_qe)
		return 0;

	if (!I915_SQS_IS_QUEUED(req->scheduler_qe))
		return 0;

	ring_id = req->ring->id;
	if (is_locked && (scheduler->flags[ring_id] & i915_sf_submitting)) {
		/* Scheduler is busy already submitting another batch,
		 * come back later rather than going recursive... */
		return -EAGAIN;
	}

	if (list_empty(&scheduler->node_queue[ring_id]))
		return 0;

	spin_lock_irqsave(&scheduler->lock, flags);

	i915_scheduler_priority_bump_clear(scheduler);

	flush_count = i915_scheduler_priority_bump(scheduler,
			    req->scheduler_qe, scheduler->priority_level_max);

	spin_unlock_irqrestore(&scheduler->lock, flags);

	if (flush_count) {
		DRM_DEBUG_DRIVER("<%s> Bumped %d entries\n", req->ring->name, flush_count);
		flush_count = i915_scheduler_submit_max_priority(req->ring, is_locked);
	}

	return flush_count;
}

int i915_scheduler_flush(struct intel_engine_cs *ring, bool is_locked)
{
	struct i915_scheduler_queue_entry *node;
	struct drm_i915_private           *dev_priv;
	struct i915_scheduler             *scheduler;
	unsigned long       flags;
	bool        found;
	int         ret;
	uint32_t    count = 0;

	if (!ring)
		return -EINVAL;

	dev_priv  = ring->dev->dev_private;
	scheduler = dev_priv->scheduler;

	if (!scheduler)
		return 0;

	BUG_ON(is_locked && (scheduler->flags[ring->id] & i915_sf_submitting));

	do {
		found = false;
		spin_lock_irqsave(&scheduler->lock, flags);
		list_for_each_entry(node, &scheduler->node_queue[ring->id], link) {
			if (!I915_SQS_IS_QUEUED(node))
				continue;

			found = true;
			break;
		}
		spin_unlock_irqrestore(&scheduler->lock, flags);

		if (found) {
			ret = i915_scheduler_submit(ring, is_locked);
			if (ret < 0)
				return ret;

			count += ret;
		}
	} while (found);

	return count;
}

void i915_scheduler_priority_bump_clear(struct i915_scheduler *scheduler)
{
	struct i915_scheduler_queue_entry *node;
	int i;

	/*
	 * Ensure circular dependencies don't cause problems and that a bump
	 * by object usage only bumps each using buffer once:
	 */
	for (i = 0; i < I915_NUM_RINGS; i++) {
		list_for_each_entry(node, &scheduler->node_queue[i], link)
			node->bumped = false;
	}
}

int i915_scheduler_priority_bump(struct i915_scheduler *scheduler,
				 struct i915_scheduler_queue_entry *target,
				 uint32_t bump)
{
	uint32_t new_priority;
	int      i, count;

	if (target->priority >= scheduler->priority_level_max)
		return 1;

	if (target->bumped)
		return 0;

	new_priority = target->priority + bump;
	if ((new_priority <= target->priority) ||
	    (new_priority > scheduler->priority_level_max))
		target->priority = scheduler->priority_level_max;
	else
		target->priority = new_priority;

	count = 1;
	target->bumped = true;

	for (i = 0; i < target->num_deps; i++) {
		if (!target->dep_list[i])
			continue;

		if (target->dep_list[i]->bumped)
			continue;

		count += i915_scheduler_priority_bump(scheduler,
						      target->dep_list[i],
						      bump);
	}

	return count;
}

int i915_scheduler_submit_max_priority(struct intel_engine_cs *ring,
				       bool is_locked)
{
	struct i915_scheduler_queue_entry  *node;
	struct drm_i915_private            *dev_priv = ring->dev->dev_private;
	struct i915_scheduler              *scheduler = dev_priv->scheduler;
	unsigned long	flags;
	int             ret, count = 0;
	bool            found;

	do {
		found = false;
		spin_lock_irqsave(&scheduler->lock, flags);
		list_for_each_entry(node, &scheduler->node_queue[ring->id], link) {
			if (!I915_SQS_IS_QUEUED(node))
				continue;

			if (node->priority < scheduler->priority_level_max)
				continue;

			found = true;
			break;
		}
		spin_unlock_irqrestore(&scheduler->lock, flags);

		if (!found)
			break;

		ret = i915_scheduler_submit(ring, is_locked);
		if (ret < 0)
			return ret;

		count += ret;
	} while (found);

	return count;
}

#ifdef CONFIG_DRM_I915_SYNC
/* Use a private structure in order to pass the 'dev' pointer through */
struct i915_sync_fence_waiter {
	struct sync_fence_waiter sfw;
	struct drm_device	 *dev;
};

static void i915_scheduler_wait_fence_signaled(struct sync_fence *fence,
				       struct sync_fence_waiter *waiter)
{
	struct i915_sync_fence_waiter *i915_waiter;
	struct drm_i915_private *dev_priv = NULL;

	i915_waiter = container_of(waiter, struct i915_sync_fence_waiter, sfw);
	dev_priv    = (i915_waiter && i915_waiter->dev) ?
					    i915_waiter->dev->dev_private : NULL;

	/*
	 * NB: The callback is executed at interrupt time, thus it can not
	 * call _submit() directly. It must go via the delayed work handler.
	 */
	if (dev_priv)
		queue_work(dev_priv->wq, &dev_priv->mm.scheduler_work);

	kfree(waiter);
}

static bool i915_scheduler_async_fence_wait(struct drm_device *dev,
					    struct sync_fence *fence)
{
	struct i915_sync_fence_waiter	*fence_waiter;
	int				signaled;
	bool				success = true;

	if (fence == NULL)
		return false;

	signaled = fence->status;
	if (!signaled) {
		fence_waiter = kmalloc(sizeof(*fence_waiter), GFP_KERNEL);
		if (!fence_waiter) {
			success = false;
			goto end;
		}

		INIT_LIST_HEAD(&fence_waiter->sfw.waiter_list);
		fence_waiter->sfw.callback = i915_scheduler_wait_fence_signaled;
		fence_waiter->dev = dev;

		if (sync_fence_wait_async(fence, &fence_waiter->sfw)) {
			/* an error occurred, usually this is because the
			 * fence was signaled already */
			signaled = fence->status;
			if (!signaled) {
				success = false;
				goto end;
			}
		}
	}
end:
	return success;
}
#endif

static int i915_scheduler_pop_from_queue_locked(struct intel_engine_cs *ring,
				    struct i915_scheduler_queue_entry **pop_node,
				    unsigned long *flags)
{
	struct drm_i915_private            *dev_priv = ring->dev->dev_private;
	struct i915_scheduler              *scheduler = dev_priv->scheduler;
	struct i915_scheduler_queue_entry  *best_wait;
	struct i915_scheduler_queue_entry  *best;
	struct i915_scheduler_queue_entry  *node;
	struct sync_fence  *fence_wait = NULL;
	int     ret;
	int     i;
	bool	signalled, any_queued;
	bool	has_local, has_remote, only_remote;

	*pop_node = NULL;
	ret = -ENODATA;

	any_queued = false;
	only_remote = false;
	best_wait = NULL;
	best = NULL;

#ifndef CONFIG_SYNC
	signalled = true;
#endif

	list_for_each_entry(node, &scheduler->node_queue[ring->id], link) {
		if (!I915_SQS_IS_QUEUED(node))
			continue;
		any_queued = true;

#ifdef CONFIG_SYNC
		if (node->params.fence_wait)
			signalled = node->params.fence_wait->status;
		else
			signalled = true;
#endif

		has_local  = false;
		has_remote = false;
		for (i = 0; i < node->num_deps; i++) {
			if (!i915_scheduler_is_dependency_valid(node, i))
				continue;

			if (node->dep_list[i]->params.ring == node->params.ring)
				has_local = true;
			else
				has_remote = true;
		}

		if (has_remote && !has_local)
			only_remote = true;

		if (!has_local && !has_remote) {
			if (signalled) {
				if (!best ||
				    (node->priority > best->priority))
					best = node;
			} else {
				if (!best_wait ||
				    (node->priority > best_wait->priority))
					best_wait = node;
			}
		}
	}

	if (best) {
		list_del(&best->link);

		INIT_LIST_HEAD(&best->link);
		best->status  = i915_sqs_popped;

		trace_i915_scheduler_node_state_change(ring, best);

		ret = 0;
	} else {
		/* Can only get here if:
		 * (a) there are no buffers in the queue
		 * (b) all queued buffers are dependent on other buffers
		 *     e.g. on a buffer that is in flight on a different ring
		 * (c) all independent buffers are waiting on fences
		 */
		if (best_wait) {
			/* Need to wait for something to be signalled.
			 *
			 * NB: do not really want to wait on one specific fd
			 * because there is no guarantee in the order that
			 * blocked buffers will be signalled. Need to wait on
			 * 'anything' and then rescan for best available, if
			 * still nothing then wait again...
			 *
			 * NB 2: The wait must also wake up if someone attempts
			 * to submit a new buffer. The new buffer might be
			 * independent of all others and thus could jump the
			 * queue and start running immediately.
			 *
			 * NB 3: Lastly, must not wait with the spinlock held!
			 *
			 * So rather than wait here, need to queue a deferred
			 * wait thread and just return 'nothing to do'.
			 *
			 * NB 4: Can't actually do the wait here because the
			 * spinlock is still held and the wait requires doing
			 * a memory allocation.
			 */
			fence_wait = best_wait->params.fence_wait;
			ret = -EAGAIN;
		} else if (only_remote) {
			/* The only dependent buffers are on another ring. */
			ret = -EAGAIN;
		} else if (any_queued) {
			/* It seems that something has gone horribly wrong! */
			DRM_ERROR("Broken dependency tracking on ring %d!\n",
				  (int) ring->id);
		}
	}

	/* i915_scheduler_dump_queue_pop(ring, best); */

	if (fence_wait) {
#ifdef CONFIG_DRM_I915_SYNC
		/* It should be safe to sleep now... */
		/* NB: Need to release and reacquire the spinlock though */
		spin_unlock_irqrestore(&scheduler->lock, *flags);
		i915_scheduler_async_fence_wait(ring->dev, fence_wait);
		spin_lock_irqsave(&scheduler->lock, *flags);
#else
		BUG_ON(true);
#endif
	}

	trace_i915_scheduler_pop_from_queue(ring, best);

	*pop_node = best;
	return ret;
}

int i915_scheduler_submit(struct intel_engine_cs *ring, bool was_locked)
{
	struct drm_device   *dev = ring->dev;
	struct drm_i915_private *dev_priv = dev->dev_private;
	struct i915_scheduler   *scheduler = dev_priv->scheduler;
	struct i915_scheduler_queue_entry  *node;
	unsigned long       flags;
	int                 ret = 0, count = 0;

	if (!was_locked) {
		ret = i915_mutex_lock_interruptible(dev);
		if (ret)
			return ret;
	}

	BUG_ON(!mutex_is_locked(&dev->struct_mutex));

	spin_lock_irqsave(&scheduler->lock, flags);

	/* First time around, complain if anything unexpected occurs: */
	ret = i915_scheduler_pop_from_queue_locked(ring, &node, &flags);
	if (ret) {
		spin_unlock_irqrestore(&scheduler->lock, flags);

		if (!was_locked)
			mutex_unlock(&dev->struct_mutex);

		return ret;
	}

	do {
		BUG_ON(!node);
		BUG_ON(node->params.ring != ring);
		BUG_ON(node->status != i915_sqs_popped);
		count++;

		/* The call to pop above will have removed the node from the
		 * list. So add it back in and mark it as in flight. */
		i915_scheduler_fly_node(node);

		scheduler->flags[ring->id] |= i915_sf_submitting;
		spin_unlock_irqrestore(&scheduler->lock, flags);
		ret = dev_priv->gt.do_execfinal(&node->params);
		spin_lock_irqsave(&scheduler->lock, flags);
		scheduler->flags[ring->id] &= ~i915_sf_submitting;

		if (ret) {
			int requeue = 1;

			/* Oh dear! Either the node is broken or the ring is
			 * busy. So need to kill the node or requeue it and try
			 * again later as appropriate. */

			switch (-ret) {
			case ENODEV:
			case ENOENT:
				/* Fatal errors. Kill the node. */
				requeue = -1;
			break;

			case EAGAIN:
			case EBUSY:
			case EIO:
			case ENOMEM:
			case ERESTARTSYS:
			case EINTR:
				/* Supposedly recoverable errors. */
			break;

			default:
				DRM_DEBUG_DRIVER("<%s> Got unexpected error from execfinal(): %d!\n",
						 ring->name, ret);
				/* Assume it is recoverable and hope for the best. */
			break;
			}

			/* Check that the watchdog/reset code has not nuked
			 * the node while we weren't looking: */
			if (node->status == i915_sqs_dead)
				requeue = 0;

			if (requeue == 1) {
				i915_scheduler_node_requeue(node);
				/* No point spinning if the ring is currently
				 * unavailable so just give up and come back
				 * later. */
				break;
			} else if (requeue == -1)
				i915_scheduler_node_kill(node);
		}

		/* Keep launching until the sky is sufficiently full. */
		if (i915_scheduler_count_flying(scheduler, ring) >=
						scheduler->min_flying)
			break;

		ret = i915_scheduler_pop_from_queue_locked(ring, &node, &flags);
	} while (ret == 0);

	spin_unlock_irqrestore(&scheduler->lock, flags);

	if (!was_locked)
		mutex_unlock(&dev->struct_mutex);

	/* Don't complain about not being able to submit extra entries */
	if (ret == -ENODATA)
		ret = 0;

	return (ret < 0) ? ret : count;
}

int i915_scheduler_remove_dependent(struct i915_scheduler *scheduler,
				    struct i915_scheduler_queue_entry *remove)
{
	struct i915_scheduler_queue_entry  *node;
	int     i, r;
	int     count = 0;

	for (i = 0; i < remove->num_deps; i++)
		if ((remove->dep_list[i]) &&
		    (!I915_SQS_IS_COMPLETE(remove->dep_list[i])))
			count++;
	BUG_ON(count);

	for (r = 0; r < I915_NUM_RINGS; r++) {
		list_for_each_entry(node, &scheduler->node_queue[r], link) {
			for (i = 0; i < node->num_deps; i++) {
				if (node->dep_list[i] != remove)
					continue;

				node->dep_list[i] = NULL;
			}
		}
	}

	return 0;
}

bool i915_scheduler_is_request_tracked(struct drm_i915_gem_request *req,
				       bool *completed, bool *busy)
{
	struct drm_i915_private *dev_priv = req->ring->dev->dev_private;
	struct i915_scheduler   *scheduler = dev_priv->scheduler;

	if (!scheduler)
		return false;

	if (req->scheduler_qe == NULL)
		return false;

	if (completed)
		*completed = I915_SQS_IS_COMPLETE(req->scheduler_qe);
	if (busy)
		*busy      = I915_SQS_IS_QUEUED(req->scheduler_qe);

	return true;
}

int i915_scheduler_closefile(struct drm_device *dev, struct drm_file *file)
{
	struct i915_scheduler_queue_entry  *node;
	struct drm_i915_private            *dev_priv = dev->dev_private;
	struct i915_scheduler              *scheduler = dev_priv->scheduler;
	struct drm_i915_gem_request        *req;
	struct intel_engine_cs  *ring;
	int                     i, ret;
	unsigned long           flags;
	bool                    found;

	if (!scheduler)
		return 0;

	for_each_ring(ring, dev_priv, i) {
		do {
			spin_lock_irqsave(&scheduler->lock, flags);

			found = false;
			list_for_each_entry(node, &scheduler->node_queue[ring->id], link) {
				if (I915_SQS_IS_COMPLETE(node))
					continue;

				if (node->params.file != file)
					continue;

				found = true;
				req = node->params.request;
				i915_gem_request_reference(req);
				break;
			}

			spin_unlock_irqrestore(&scheduler->lock, flags);

			if (found) {
				do {
					mutex_lock(&dev->struct_mutex);
					ret = i915_wait_request(req);
					mutex_unlock(&dev->struct_mutex);
					if (ret == -EAGAIN)
						msleep(20);
				} while (ret == -EAGAIN);

				mutex_lock(&dev->struct_mutex);
				i915_gem_request_unreference(req);
				mutex_unlock(&dev->struct_mutex);
			}
		} while (found);
	}

	spin_lock_irqsave(&scheduler->lock, flags);
	for_each_ring(ring, dev_priv, i) {
		list_for_each_entry(node, &scheduler->node_queue[ring->id], link) {
			if (node->params.file != file)
				continue;

			WARN_ON(!I915_SQS_IS_COMPLETE(node));

			node->params.file = NULL;
		}
	}
	spin_unlock_irqrestore(&scheduler->lock, flags);

	return 0;
}

bool i915_scheduler_is_ring_idle(struct intel_engine_cs *ring)
{
	struct i915_scheduler_queue_entry *node;
	struct drm_i915_private *dev_priv = ring->dev->dev_private;
	struct i915_scheduler   *scheduler = dev_priv->scheduler;
	unsigned long   flags;
	bool            idle = true;

	if (!scheduler)
		return true;

	spin_lock_irqsave(&scheduler->lock, flags);

	list_for_each_entry(node, &scheduler->node_queue[ring->id], link) {
		if (!I915_SQS_IS_COMPLETE(node)) {
			idle = false;
			break;
		}
	}

	spin_unlock_irqrestore(&scheduler->lock, flags);

	return idle;
}

bool i915_scheduler_file_queue_is_full(struct drm_file *file)
{
	struct drm_i915_file_private *file_priv = file->driver_priv;
	struct drm_i915_private      *dev_priv  = file_priv->dev_priv;
	struct i915_scheduler        *scheduler = dev_priv->scheduler;

	return file_priv->scheduler_queue_length >= scheduler->file_queue_max;
}

void i915_scheduler_file_queue_inc(struct drm_file *file)
{
	struct drm_i915_file_private *file_priv = file->driver_priv;

	file_priv->scheduler_queue_length++;
}

void i915_scheduler_file_queue_dec(struct drm_file *file)
{
	struct drm_i915_file_private *file_priv = file->driver_priv;

	file_priv->scheduler_queue_length--;
}
