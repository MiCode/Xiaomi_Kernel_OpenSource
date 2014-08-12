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

#ifndef _I915_SCHEDULER_H_
#define _I915_SCHEDULER_H_

enum i915_scheduler_queue_status {
	/* Limbo: */
	i915_sqs_none = 0,
	/* Not yet submitted to hardware: */
	i915_sqs_queued,
	/* Popped from queue, ready to fly: */
	i915_sqs_popped,
	/* Sent to hardware for processing: */
	i915_sqs_flying,
	/* Finished processing on the hardware: */
	i915_sqs_complete,
	/* Killed by watchdog or catastrophic submission failure: */
	i915_sqs_dead,
	/* Limit value for use with arrays/loops */
	i915_sqs_MAX
};

#define I915_SQS_IS_QUEUED(node)	(((node)->status == i915_sqs_queued))
#define I915_SQS_IS_FLYING(node)	(((node)->status == i915_sqs_flying))
#define I915_SQS_IS_COMPLETE(node)	(((node)->status == i915_sqs_complete) || \
					 ((node)->status == i915_sqs_dead))

#define I915_SQS_CASE_QUEUED		i915_sqs_queued
#define I915_SQS_CASE_FLYING		i915_sqs_flying
#define I915_SQS_CASE_COMPLETE		i915_sqs_complete:		\
					case i915_sqs_dead

struct i915_scheduler_obj_entry {
	struct drm_i915_gem_object          *obj;
};

struct i915_scheduler_queue_entry {
	struct i915_execbuffer_params       params;
	uint32_t                            priority;
	struct i915_scheduler_obj_entry     *saved_objects;
	int                                 num_objs;
	bool                                bumped;
	struct i915_scheduler_queue_entry   **dep_list;
	int                                 num_deps;
	enum i915_scheduler_queue_status    status;
	struct timespec                     stamp;
	struct list_head                    link;
	uint32_t                            scheduler_index;
};

#define I915_SCHEDULER_FLUSH_ALL(ring, locked)                              \
	i915_scheduler_flush(ring, locked)

#define I915_SCHEDULER_FLUSH_REQUEST(req, locked)                           \
	i915_scheduler_flush_request(req, locked)

struct i915_scheduler {
	struct list_head    node_queue[I915_NUM_RINGS];
	uint32_t            flags[I915_NUM_RINGS];
	spinlock_t          lock;
	uint32_t            index;

	/* Tuning parameters: */
	uint32_t            priority_level_max;
	uint32_t            priority_level_preempt;
	uint32_t            min_flying;
};

/* Flag bits for i915_scheduler::flags */
enum {
	i915_sf_interrupts_enabled  = (1 << 0),
	i915_sf_submitting          = (1 << 1),
};

bool        i915_scheduler_is_enabled(struct drm_device *dev);
int         i915_scheduler_init(struct drm_device *dev);
int         i915_scheduler_closefile(struct drm_device *dev,
				     struct drm_file *file);
int         i915_scheduler_queue_execbuffer(struct i915_scheduler_queue_entry *qe);
int         i915_scheduler_handle_irq(struct intel_engine_cs *ring);
bool        i915_scheduler_is_ring_idle(struct intel_engine_cs *ring);
void        i915_scheduler_kill_all(struct drm_device *dev);
void        i915_gem_scheduler_work_handler(struct work_struct *work);
int         i915_scheduler_flush(struct intel_engine_cs *ring, bool is_locked);
int         i915_scheduler_flush_request(struct drm_i915_gem_request *req,
					 bool is_locked);
bool        i915_scheduler_is_request_tracked(struct drm_i915_gem_request *req,
					      bool *completed, bool *busy);

#endif  /* _I915_SCHEDULER_H_ */
