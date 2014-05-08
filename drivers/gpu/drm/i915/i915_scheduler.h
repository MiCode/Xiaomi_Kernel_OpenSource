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
char i915_scheduler_queue_status_chr(enum i915_scheduler_queue_status status);
const char *i915_scheduler_queue_status_str(
				enum i915_scheduler_queue_status status);

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
const char *i915_qe_state_str(struct i915_scheduler_queue_entry *node);

#define I915_SCHEDULER_FLUSH_ALL(ring, locked)                              \
	i915_scheduler_flush(ring, locked)

#define I915_SCHEDULER_FLUSH_REQUEST(req, locked)                           \
	i915_scheduler_flush_request(req, locked)

struct i915_scheduler_stats_nodes {
	uint32_t	counts[i915_sqs_MAX + 1];
};

struct i915_scheduler_stats {
	/* Batch buffer counts: */
	uint32_t            queued;
	uint32_t            submitted;
	uint32_t            completed;
	uint32_t            expired;

	/* Other stuff: */
	uint32_t            flush_obj;
	uint32_t            flush_req;
	uint32_t            flush_all;
	uint32_t            flush_bump;
	uint32_t            flush_submit;

	uint32_t            exec_early;
	uint32_t            exec_again;
	uint32_t            exec_dead;
	uint32_t            kill_flying;
	uint32_t            kill_queued;

	uint32_t            fence_wait;
};

struct i915_scheduler {
	struct list_head    node_queue[I915_NUM_RINGS];
	uint32_t            flags[I915_NUM_RINGS];
	spinlock_t          lock;
	uint32_t            index;

	/* Tuning parameters: */
	uint32_t            priority_level_max;
	uint32_t            priority_level_preempt;
	uint32_t            min_flying;
	uint32_t            file_queue_max;

	/* Statistics: */
	struct i915_scheduler_stats     stats[I915_NUM_RINGS];
};

/* Flag bits for i915_scheduler::flags */
enum {
	/* Internal state */
	i915_sf_interrupts_enabled  = (1 << 0),
	i915_sf_submitting          = (1 << 1),

	/* Dump/debug flags */
	i915_sf_dump_force          = (1 << 8),
	i915_sf_dump_details        = (1 << 9),
	i915_sf_dump_dependencies   = (1 << 10),
};
const char *i915_scheduler_flag_str(uint32_t flags);

/* Options for 'scheduler_override' module parameter: */
enum {
	i915_so_direct_submit       = (1 << 0),
	i915_so_submit_on_queue     = (1 << 1),
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
int         i915_scheduler_dump(struct intel_engine_cs *ring,
				const char *msg);
int         i915_scheduler_dump_all(struct drm_device *dev, const char *msg);
bool        i915_scheduler_is_request_tracked(struct drm_i915_gem_request *req,
					      bool *completed, bool *busy);
int         i915_scheduler_query_stats(struct intel_engine_cs *ring,
				       struct i915_scheduler_stats_nodes *stats);
bool        i915_scheduler_file_queue_is_full(struct drm_file *file);

#endif  /* _I915_SCHEDULER_H_ */
