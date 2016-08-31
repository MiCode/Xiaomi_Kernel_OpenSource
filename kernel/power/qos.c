/*
 * This module exposes the interface to kernel space for specifying
 * QoS dependencies.  It provides infrastructure for registration of:
 *
 * Dependents on a QoS value : register requests
 * Watchers of QoS value : get notified when target QoS value changes
 *
 * This QoS design is best effort based.  Dependents register their QoS needs.
 * Watchers register to keep track of the current QoS needs of the system.
 *
 * There are 3 basic classes of QoS parameter: latency, timeout, throughput
 * each have defined units:
 * latency: usec
 * timeout: usec <-- currently not used.
 * throughput: kbs (kilo byte / sec)
 *
 * There are lists of pm_qos_objects each one wrapping requests, notifiers
 *
 * User mode requests on a QOS parameter register themselves to the
 * subsystem by opening the device node /dev/... and writing there request to
 * the node.  As long as the process holds a file handle open to the node the
 * client continues to be accounted for.  Upon file release the usermode
 * request is removed and a new qos target is computed.  This way when the
 * request that the application has is cleaned up when closes the file
 * pointer or exits the pm_qos_object will get an opportunity to clean up.
 *
 * Mark Gross <mgross@linux.intel.com>
 *
 * Support added for bounded constraints by
 * Sai Gurrappadi <sgurrappadi@nvidia.com>
 * Copyright (c) 2013-14, NVIDIA CORPORATION. All rights reserved.
 */

/*#define DEBUG*/

#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/export.h>

/*
 * locking rule: all changes to constraints or notifiers lists
 * or pm_qos_object list or pm_qos_bounded objects/lists and
 * pm_qos_objects need to happen with pm_qos_lock
 * One lock to rule them all
 */
struct pm_qos_object {
	struct pm_qos_constraints *constraints;
	struct miscdevice pm_qos_power_miscdev;
	char *name;
};

struct pm_qos_bounded_object {
	struct pm_qos_bounded_constraint *bounds;
	struct miscdevice miscdev;
	char *name;
};

static DEFINE_MUTEX(pm_qos_lock);

static struct pm_qos_object null_pm_qos;
static struct pm_qos_bounded_object null_pm_qos_bounded;

static BLOCKING_NOTIFIER_HEAD(cpu_dma_lat_notifier);
static struct pm_qos_constraints cpu_dma_constraints = {
	.list = PLIST_HEAD_INIT(cpu_dma_constraints.list),
	.target_value = PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE,
	.default_value = PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.notifiers = &cpu_dma_lat_notifier,
};
static struct pm_qos_object cpu_dma_pm_qos = {
	.constraints = &cpu_dma_constraints,
	.name = "cpu_dma_latency",
};

static BLOCKING_NOTIFIER_HEAD(network_lat_notifier);
static struct pm_qos_constraints network_lat_constraints = {
	.list = PLIST_HEAD_INIT(network_lat_constraints.list),
	.target_value = PM_QOS_NETWORK_LAT_DEFAULT_VALUE,
	.default_value = PM_QOS_NETWORK_LAT_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.notifiers = &network_lat_notifier,
};
static struct pm_qos_object network_lat_pm_qos = {
	.constraints = &network_lat_constraints,
	.name = "network_latency",
};


static BLOCKING_NOTIFIER_HEAD(network_throughput_notifier);
static struct pm_qos_constraints network_tput_constraints = {
	.list = PLIST_HEAD_INIT(network_tput_constraints.list),
	.target_value = PM_QOS_NETWORK_THROUGHPUT_DEFAULT_VALUE,
	.default_value = PM_QOS_NETWORK_THROUGHPUT_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.notifiers = &network_throughput_notifier,
};
static struct pm_qos_object network_throughput_pm_qos = {
	.constraints = &network_tput_constraints,
	.name = "network_throughput",
};

static struct pm_qos_bounded_constraint online_cpus_constraint = {
	.prio_list = PLIST_HEAD_INIT(online_cpus_constraint.prio_list),
	.max_class = PM_QOS_MAX_ONLINE_CPUS,
	.min_class = PM_QOS_MIN_ONLINE_CPUS,
	.min_wins = true,
};
static struct pm_qos_bounded_object online_cpus_pm_qos = {
	.bounds = &online_cpus_constraint,
	.name = "constraint_online_cpus",
};
static BLOCKING_NOTIFIER_HEAD(min_online_cpus_notifier);
static struct pm_qos_constraints min_online_cpus_constraints = {
	.list = PLIST_HEAD_INIT(min_online_cpus_constraints.list),
	.target_value = PM_QOS_MIN_ONLINE_CPUS_DEFAULT_VALUE,
	.default_value = PM_QOS_MIN_ONLINE_CPUS_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.notifiers = &min_online_cpus_notifier,
	.parent_class = PM_QOS_ONLINE_CPUS_BOUNDS,
};
static struct pm_qos_object min_online_cpus_pm_qos = {
	.constraints = &min_online_cpus_constraints,
	.name = "min_online_cpus",
};


static BLOCKING_NOTIFIER_HEAD(max_online_cpus_notifier);
static struct pm_qos_constraints max_online_cpus_constraints = {
	.list = PLIST_HEAD_INIT(max_online_cpus_constraints.list),
	.target_value = PM_QOS_MAX_ONLINE_CPUS_DEFAULT_VALUE,
	.default_value = PM_QOS_MAX_ONLINE_CPUS_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.notifiers = &max_online_cpus_notifier,
	.parent_class = PM_QOS_ONLINE_CPUS_BOUNDS,
};
static struct pm_qos_object max_online_cpus_pm_qos = {
	.constraints = &max_online_cpus_constraints,
	.name = "max_online_cpus",

};

static struct pm_qos_bounded_constraint cpu_freq_constraint = {
	.prio_list = PLIST_HEAD_INIT(cpu_freq_constraint.prio_list),
	.max_class = PM_QOS_CPU_FREQ_MAX,
	.min_class = PM_QOS_CPU_FREQ_MIN,
	.min_wins = false,
};
static struct pm_qos_bounded_object cpu_freq_pm_qos = {
	.bounds = &cpu_freq_constraint,
	.name = "constraint_cpu_freq",
};
static BLOCKING_NOTIFIER_HEAD(cpu_freq_min_notifier);
static struct pm_qos_constraints cpu_freq_min_constraints = {
	.list = PLIST_HEAD_INIT(cpu_freq_min_constraints.list),
	.target_value = PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE,
	.default_value = PM_QOS_CPU_FREQ_MIN_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.notifiers = &cpu_freq_min_notifier,
	.parent_class = PM_QOS_CPU_FREQ_BOUNDS,
};
static struct pm_qos_object cpu_freq_min_pm_qos = {
	.constraints = &cpu_freq_min_constraints,
	.name = "cpu_freq_min",
};


static BLOCKING_NOTIFIER_HEAD(cpu_freq_max_notifier);
static struct pm_qos_constraints cpu_freq_max_constraints = {
	.list = PLIST_HEAD_INIT(cpu_freq_max_constraints.list),
	.target_value = PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.notifiers = &cpu_freq_max_notifier,
	.parent_class = PM_QOS_CPU_FREQ_BOUNDS,
};
static struct pm_qos_object cpu_freq_max_pm_qos = {
	.constraints = &cpu_freq_max_constraints,
	.name = "cpu_freq_max",
};

static struct pm_qos_bounded_constraint gpu_freq_constraint = {
	.prio_list = PLIST_HEAD_INIT(gpu_freq_constraint.prio_list),
	.max_class = PM_QOS_GPU_FREQ_MAX,
	.min_class = PM_QOS_GPU_FREQ_MIN,
	.min_wins = false,
};
static struct pm_qos_bounded_object gpu_freq_pm_qos = {
	.bounds = &gpu_freq_constraint,
	.name = "constraint_gpu_freq",
};

static BLOCKING_NOTIFIER_HEAD(gpu_freq_min_notifier);
static struct pm_qos_constraints gpu_freq_min_constraints = {
	.list = PLIST_HEAD_INIT(gpu_freq_min_constraints.list),
	.target_value = PM_QOS_GPU_FREQ_MIN_DEFAULT_VALUE,
	.default_value = PM_QOS_GPU_FREQ_MIN_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.notifiers = &gpu_freq_min_notifier,
	.parent_class = PM_QOS_GPU_FREQ_BOUNDS,
};
static struct pm_qos_object gpu_freq_min_pm_qos = {
	.constraints = &gpu_freq_min_constraints,
	.name = "gpu_freq_min",
};

static BLOCKING_NOTIFIER_HEAD(gpu_freq_max_notifier);
static struct pm_qos_constraints gpu_freq_max_constraints = {
	.list = PLIST_HEAD_INIT(gpu_freq_max_constraints.list),
	.target_value = PM_QOS_GPU_FREQ_MAX_DEFAULT_VALUE,
	.default_value = PM_QOS_GPU_FREQ_MAX_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.notifiers = &gpu_freq_max_notifier,
	.parent_class = PM_QOS_GPU_FREQ_BOUNDS,
};
static struct pm_qos_object gpu_freq_max_pm_qos = {
	.constraints = &gpu_freq_max_constraints,
	.name = "gpu_freq_max",
};

static BLOCKING_NOTIFIER_HEAD(emc_freq_min_notifier);
static struct pm_qos_constraints emc_freq_min_constraints = {
	.list = PLIST_HEAD_INIT(emc_freq_min_constraints.list),
	.target_value = PM_QOS_EMC_FREQ_MIN_DEFAULT_VALUE,
	.default_value = PM_QOS_EMC_FREQ_MIN_DEFAULT_VALUE,
	.type = PM_QOS_MAX,
	.notifiers = &emc_freq_min_notifier,
};
static struct pm_qos_object emc_freq_min_pm_qos = {
	.constraints = &emc_freq_min_constraints,
	.name = "emc_freq_min",
};

static struct pm_qos_object *pm_qos_array[] = {
	&null_pm_qos,
	&cpu_dma_pm_qos,
	&network_lat_pm_qos,
	&network_throughput_pm_qos,
	&min_online_cpus_pm_qos,
	&max_online_cpus_pm_qos,
	&cpu_freq_min_pm_qos,
	&cpu_freq_max_pm_qos,
	&gpu_freq_min_pm_qos,
	&gpu_freq_max_pm_qos,
	&emc_freq_min_pm_qos
};

static struct pm_qos_bounded_object * const pm_qos_bounded_obj_array[] = {
	&null_pm_qos_bounded,
	&cpu_freq_pm_qos,
	&gpu_freq_pm_qos,
	&online_cpus_pm_qos
};

static ssize_t pm_qos_power_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos);
static ssize_t pm_qos_power_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos);
static int pm_qos_power_open(struct inode *inode, struct file *filp);
static int pm_qos_power_release(struct inode *inode, struct file *filp);

static const struct file_operations pm_qos_power_fops = {
	.write = pm_qos_power_write,
	.read = pm_qos_power_read,
	.open = pm_qos_power_open,
	.release = pm_qos_power_release,
	.llseek = noop_llseek,
};

static ssize_t pm_qos_bounded_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos);
static ssize_t pm_qos_bounded_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos);
static int pm_qos_bounded_open(struct inode *inode, struct file *filp);
static int pm_qos_bounded_release(struct inode *inode, struct file *filp);

static const struct file_operations pm_qos_bounded_constraint_fops = {
	.write = pm_qos_bounded_write,
	.read = pm_qos_bounded_read,
	.open = pm_qos_bounded_open,
	.release = pm_qos_bounded_release,
	.llseek = generic_file_llseek,
};

static bool pm_qos_enabled __read_mostly = true;

/* unlocked internal variant */
static inline int pm_qos_get_value(struct pm_qos_constraints *c)
{
	if (plist_head_empty(&c->list))
		return c->default_value;

	switch (c->type) {
	case PM_QOS_MIN:
		return plist_first(&c->list)->prio;

	case PM_QOS_MAX:
		return plist_last(&c->list)->prio;

	default:
		/* runtime check for not using enum */
		BUG();
		return PM_QOS_DEFAULT_VALUE;
	}
}

s32 pm_qos_read_value(struct pm_qos_constraints *c)
{
	return c->target_value;
}

static inline void pm_qos_set_value(struct pm_qos_constraints *c, s32 value)
{
	c->target_value = value;
}

/*
 * Finds the current max and min targets for the given bounded constraint.
 * If buf is non NULL, the max and min targets at each priority level are
 * written to the provided buffer as long as buf_size isn't exceeded.
 * Returns the bytes read into the buffer and updates variables target_max and
 * target_min with new targets.
 */
static size_t pm_qos_find_bounded_targets(struct pm_qos_bounded_constraint *c,
					  s32 *target_max, s32 *target_min,
					  char *buf, size_t buf_size)
{
	char str[30];
	size_t size, bytes_read;
	struct pm_qos_prio *p;
	struct pm_qos_constraints *max_constraint, *min_constraint;
	s32 cur_max, cur_min, tmp_max, tmp_min;

	bool stop = false;
	char header[] = "Priority Min Max\n";
	max_constraint = pm_qos_array[c->max_class]->constraints;
	min_constraint = pm_qos_array[c->min_class]->constraints;
	cur_max = max_constraint->default_value;
	cur_min = min_constraint->default_value;
	bytes_read = 0;
	if (buf) {
		size = strlen(header);
		if (size > buf_size)
			return 0;
		memcpy(buf, header, size);
		bytes_read = size;
		buf_size -= size;
		buf += size;
	}

	/*
	 * Output the intersection of all (min, max) higher priority
	 * ranges at each priority level
	 */
	plist_for_each_entry(p, &c->prio_list, node) {
		tmp_min = min_constraint->default_value;
		tmp_max = max_constraint->default_value;
		if (!plist_head_empty(&p->max_list))
			tmp_max = plist_first(&p->max_list)->prio;
		if (!plist_head_empty(&p->min_list))
			tmp_min = plist_last(&p->min_list)->prio;
		if (tmp_min > tmp_max) {
			if (c->min_wins)
				tmp_max = tmp_min;
			else
				tmp_min = tmp_max;
		}
		if (tmp_min > cur_min) {
			if (tmp_min < cur_max) {
				cur_min = tmp_min;
			} else {
				cur_min = cur_max;
				stop = true;
			}
		}
		if (tmp_max < cur_max) {
			if (tmp_max > cur_min) {
				cur_max = tmp_max;
			} else {
				cur_max = cur_min;
				stop = true;
			}
		}
		if (buf) {
			size = scnprintf(str, sizeof(str), "%i %i %i\n",
					 p->node.prio, cur_min, cur_max);
			if (size > buf_size)
				return 0;

			memcpy(buf, str, size);
			buf += size;
			buf_size -= size;
			bytes_read += size;
		}
		if (stop)
			break;
	}

	if (target_max)
		*target_max = cur_max;
	if (target_min)
		*target_min = cur_min;

	return bytes_read;
}

/* Updates the target bounds for the given bounded constraint */
static void pm_qos_set_bounded_targets(struct pm_qos_bounded_constraint *c)
{
	struct pm_qos_constraints *max_constraint, *min_constraint;
	s32 cur_max, cur_min;

	max_constraint = pm_qos_array[c->max_class]->constraints;
	min_constraint = pm_qos_array[c->min_class]->constraints;
	cur_max = max_constraint->default_value;
	cur_min = min_constraint->default_value;

	if (pm_qos_enabled)
		pm_qos_find_bounded_targets(c, &cur_max, &cur_min, NULL, 0);

	pm_qos_set_value(max_constraint, cur_max);
	pm_qos_set_value(min_constraint, cur_min);
}

/*
 * Remove node of the given priority and type. Removes priority node
 * from the list of priorities if it is no longer used.
 */
static void pm_qos_remove_node(struct plist_node *node,
			       struct pm_qos_prio *priority,
			       struct pm_qos_bounded_constraint *c,
			       enum pm_qos_type type)
{
	if (!priority)
		return;

	/* pm_qos_max => minimum bound of constraint and vice versa */
	if (type == PM_QOS_MAX)
		plist_del(node, &priority->min_list);
	else if (type == PM_QOS_MIN)
		plist_del(node, &priority->max_list);
	else
		return;

	/* Remove priority level if no longer used */
	if (plist_head_empty(&priority->max_list) &&
	    plist_head_empty(&priority->min_list)) {
		plist_del(&priority->node, &c->prio_list);
		kfree(priority);
	}
}

/* Add new node at the specified priority for the given type */
static void pm_qos_add_node(struct plist_node *node,
			    struct pm_qos_prio *priority,
			    enum pm_qos_type type)
{
	if (!priority)
		return;

	/* pm_qos_max => minimum bound of constraint and vice versa */
	if (type == PM_QOS_MAX)
		plist_add(node, &priority->min_list);
	else if (type == PM_QOS_MIN)
		plist_add(node, &priority->max_list);
	return;
}

/* Creates, initializes and adds the priority level. Returns NULL on failure */
static struct pm_qos_prio *pm_qos_add_priority(int priority,
					       struct plist_head *list)
{
	struct pm_qos_prio *prio = kzalloc(sizeof(*prio), GFP_KERNEL);
	if (!prio)
		return NULL;

	plist_node_init(&prio->node, priority);
	plist_head_init(&prio->max_list);
	plist_head_init(&prio->min_list);
	plist_add(&prio->node, list);

	return prio;
}

/* Returns priority level from the priority list. NULL if it doesn't exist */
static struct pm_qos_prio *pm_qos_get_prio_level(int priority,
						 struct plist_head *list)
{
	struct pm_qos_prio *p;

	if (plist_head_empty(list))
		return NULL;

	plist_for_each_entry(p, list, node)
		if (p->node.prio == priority)
			return p;

	return NULL;
}

/**
 * pm_qos_update_bounded_target - Update a bounded constraints target bounds
 * @c: bound that is being updated (either min or max)
 * @value: new value to add or update for the given bound
 * @req: the request that is getting updated/added or removed
 * @priority: new priority of the bound request
 * @action: remove/add/update req
 *
 * Returns a negative value on error, 0 if the target bounds were not updated
 * and 1 if the target bounds were updated.
 */
static int pm_qos_update_bounded_target(struct pm_qos_constraints *c, s32 value,
					struct pm_qos_request *req,
					int priority,
					enum pm_qos_req_action action)
{
	struct pm_qos_constraints *max_constraint, *min_constraint;
	struct pm_qos_bounded_constraint *parent;
	struct pm_qos_prio *prio;
	s32 prev_max, prev_min, curr_max, curr_min, new_value;
	int ret = -EINVAL;

	if (!c->parent_class)
		return 0;

	mutex_lock(&pm_qos_lock);
	parent = pm_qos_bounded_obj_array[c->parent_class]->bounds;
	max_constraint = pm_qos_array[parent->max_class]->constraints;
	min_constraint = pm_qos_array[parent->min_class]->constraints;
	prev_max = pm_qos_read_value(max_constraint);
	prev_min = pm_qos_read_value(min_constraint);
	new_value = value;
	if (value == PM_QOS_DEFAULT_VALUE)
		new_value = c->default_value;

	switch (action) {
	case PM_QOS_REMOVE_REQ:
		prio = pm_qos_get_prio_level(priority, &parent->prio_list);
		pm_qos_remove_node(&req->node, prio, parent, c->type);
		break;
	case PM_QOS_UPDATE_REQ:
		/*
		 * Remove old node and update by reinitializing node and
		 * adding it
		 */
		prio = pm_qos_get_prio_level(req->priority, &parent->prio_list);
		if (!prio) {
			WARN(1, KERN_ERR "pm_qos_update_bounded_target: priority does not exist\n");
			mutex_unlock(&pm_qos_lock);
			return -EINVAL;
		}
		pm_qos_remove_node(&req->node, prio, parent, c->type);
		/* Fall through and add */
	case PM_QOS_ADD_REQ:
		prio = pm_qos_get_prio_level(priority, &parent->prio_list);
		if (!prio) {
			prio = pm_qos_add_priority(priority,
						   &parent->prio_list);
			if (!prio) {
				mutex_unlock(&pm_qos_lock);
				return -ENOMEM;
			}
		}
		plist_node_init(&req->node, new_value);
		pm_qos_add_node(&req->node, prio, c->type);
		break;
	default:
		break;
	}

	pm_qos_set_bounded_targets(parent);

	curr_max = pm_qos_read_value(max_constraint);
	curr_min = pm_qos_read_value(min_constraint);
	ret = 0;

	/* Call notifiers if necessary */
	if (prev_max != curr_max) {
		if (curr_max < prev_min) {
			blocking_notifier_call_chain(min_constraint->notifiers,
						     (unsigned long)curr_min,
						     NULL);
			prev_min = curr_min;
		}
		blocking_notifier_call_chain(max_constraint->notifiers,
					     (unsigned long)curr_max,
					     NULL);
		ret = 1;
	}
	if (prev_min != curr_min) {
		blocking_notifier_call_chain(min_constraint->notifiers,
					     (unsigned long)curr_min,
					     NULL);
		ret = 1;
	}

	mutex_unlock(&pm_qos_lock);

	return ret;
}

/**
 * pm_qos_update_target - manages the constraints list and calls the notifiers
 *  if needed
 * @c: constraints data struct
 * @node: request to add to the list, to update or to remove
 * @action: action to take on the constraints list
 * @value: value of the request to add or update
 *
 * This function returns 1 if the aggregated constraint value has changed, 0
 *  otherwise.
 */
int pm_qos_update_target(struct pm_qos_constraints *c, struct plist_node *node,
			 enum pm_qos_req_action action, int value)
{
	int prev_value, curr_value, new_value, ret;

	mutex_lock(&pm_qos_lock);
	prev_value = pm_qos_get_value(c);
	if (value == PM_QOS_DEFAULT_VALUE)
		new_value = c->default_value;
	else
		new_value = value;

	switch (action) {
	case PM_QOS_REMOVE_REQ:
		plist_del(node, &c->list);
		break;
	case PM_QOS_UPDATE_REQ:
		/*
		 * to change the list, we atomically remove, reinit
		 * with new value and add, then see if the extremal
		 * changed
		 */
		plist_del(node, &c->list);
	case PM_QOS_ADD_REQ:
		plist_node_init(node, new_value);
		plist_add(node, &c->list);
		break;
	default:
		/* no action */
		;
	}

	if (pm_qos_enabled) {
		curr_value = pm_qos_get_value(c);
		pm_qos_set_value(c, curr_value);
	} else {
		curr_value = c->default_value;
	}

	if (prev_value != curr_value) {
		blocking_notifier_call_chain(c->notifiers,
					     (unsigned long)curr_value,
					     NULL);
		ret = 1;
	} else {
		ret = 0;
	}

	mutex_unlock(&pm_qos_lock);
	return ret;
}

/**
 * pm_qos_flags_remove_req - Remove device PM QoS flags request.
 * @pqf: Device PM QoS flags set to remove the request from.
 * @req: Request to remove from the set.
 */
static void pm_qos_flags_remove_req(struct pm_qos_flags *pqf,
				    struct pm_qos_flags_request *req)
{
	s32 val = 0;

	list_del(&req->node);
	list_for_each_entry(req, &pqf->list, node)
		val |= req->flags;

	pqf->effective_flags = val;
}

/**
 * pm_qos_update_flags - Update a set of PM QoS flags.
 * @pqf: Set of flags to update.
 * @req: Request to add to the set, to modify, or to remove from the set.
 * @action: Action to take on the set.
 * @val: Value of the request to add or modify.
 *
 * Update the given set of PM QoS flags and call notifiers if the aggregate
 * value has changed.  Returns 1 if the aggregate constraint value has changed,
 * 0 otherwise.
 */
bool pm_qos_update_flags(struct pm_qos_flags *pqf,
			 struct pm_qos_flags_request *req,
			 enum pm_qos_req_action action, s32 val)
{
	s32 prev_value, curr_value;

	mutex_lock(&pm_qos_lock);

	prev_value = list_empty(&pqf->list) ? 0 : pqf->effective_flags;

	switch (action) {
	case PM_QOS_REMOVE_REQ:
		pm_qos_flags_remove_req(pqf, req);
		break;
	case PM_QOS_UPDATE_REQ:
		pm_qos_flags_remove_req(pqf, req);
	case PM_QOS_ADD_REQ:
		req->flags = val;
		INIT_LIST_HEAD(&req->node);
		list_add_tail(&req->node, &pqf->list);
		pqf->effective_flags |= val;
		break;
	default:
		/* no action */
		;
	}

	curr_value = list_empty(&pqf->list) ? 0 : pqf->effective_flags;

	mutex_unlock(&pm_qos_lock);

	if (curr_value != prev_value && pqf->notifiers)
		blocking_notifier_call_chain(pqf->notifiers,
					     (unsigned long)curr_value,
					     NULL);

	return prev_value != curr_value;
}

/**
 * pm_qos_request - returns current system wide qos expectation
 * @pm_qos_class: identification of which qos value is requested
 *
 * This function returns the current target value.
 */
int pm_qos_request(int pm_qos_class)
{
	return pm_qos_read_value(pm_qos_array[pm_qos_class]->constraints);
}
EXPORT_SYMBOL_GPL(pm_qos_request);

int pm_qos_request_active(struct pm_qos_request *req)
{
	return req->pm_qos_class != 0;
}
EXPORT_SYMBOL_GPL(pm_qos_request_active);

static void __pm_qos_update_request(struct pm_qos_request *req,
			   s32 new_value)
{
	struct pm_qos_constraints *c;

	if (new_value == req->node.prio)
		return;

	c = pm_qos_array[req->pm_qos_class]->constraints;

	if (c->parent_class)
		pm_qos_update_bounded_target(c, new_value, req, req->priority,
					     PM_QOS_UPDATE_REQ);
	else
		pm_qos_update_target(c, &req->node, PM_QOS_UPDATE_REQ,
				     new_value);
}

/**
 * pm_qos_work_fn - the timeout handler of pm_qos_update_request_timeout
 * @work: work struct for the delayed work (timeout)
 *
 * This cancels the timeout request by falling back to the default at timeout.
 */
static void pm_qos_work_fn(struct work_struct *work)
{
	struct pm_qos_request *req = container_of(to_delayed_work(work),
						  struct pm_qos_request,
						  work);

	__pm_qos_update_request(req, PM_QOS_DEFAULT_VALUE);
}

/**
 * pm_qos_add_request - inserts new qos request into the list
 * @req: pointer to a preallocated handle
 * @pm_qos_class: identifies which list of qos request to use
 * @value: defines the qos request
 *
 * This function inserts a new entry in the pm_qos_class list of requested qos
 * performance characteristics.  It recomputes the aggregate QoS expectations
 * for the pm_qos_class of parameters and initializes the pm_qos_request
 * handle.  Caller needs to save this handle for later use in updates and
 * removal.
 */

void pm_qos_add_request(struct pm_qos_request *req,
			int pm_qos_class, s32 value)
{
	struct pm_qos_constraints *c;

	if (!req) /*guard against callers passing in null */
		return;

	if (pm_qos_request_active(req)) {
		WARN(1, KERN_ERR "pm_qos_add_request() called for already added request\n");
		return;
	}
	req->pm_qos_class = pm_qos_class;
	INIT_DELAYED_WORK(&req->work, pm_qos_work_fn);
	c = pm_qos_array[pm_qos_class]->constraints;

	if (c->parent_class) {
		req->priority = PM_QOS_PRIO_TRUSTED;
		pm_qos_update_bounded_target(c, value, req, req->priority,
					     PM_QOS_ADD_REQ);
	} else {
		pm_qos_update_target(c, &req->node, PM_QOS_ADD_REQ, value);
	}
}
EXPORT_SYMBOL_GPL(pm_qos_add_request);

/**
 * pm_qos_update_request - modifies an existing qos request
 * @req : handle to list element holding a pm_qos request to use
 * @value: defines the qos request
 *
 * Updates an existing qos request for the pm_qos_class of parameters along
 * with updating the target pm_qos_class value.
 *
 * Attempts are made to make this code callable on hot code paths.
 */
void pm_qos_update_request(struct pm_qos_request *req,
			   s32 new_value)
{
	if (!req) /*guard against callers passing in null */
		return;

	if (!pm_qos_request_active(req)) {
		WARN(1, KERN_ERR "pm_qos_update_request() called for unknown object\n");
		return;
	}

	cancel_delayed_work_sync(&req->work);

	__pm_qos_update_request(req, new_value);
}
EXPORT_SYMBOL_GPL(pm_qos_update_request);

/**
 * pm_qos_update_request_timeout - modifies an existing qos request temporarily.
 * @req : handle to list element holding a pm_qos request to use
 * @new_value: defines the temporal qos request
 * @timeout_us: the effective duration of this qos request in usecs.
 *
 * After timeout_us, this qos request is cancelled automatically.
 */
void pm_qos_update_request_timeout(struct pm_qos_request *req, s32 new_value,
				   unsigned long timeout_us)
{
	struct pm_qos_constraints *c;

	if (!req)
		return;
	if (WARN(!pm_qos_request_active(req),
		 "%s called for unknown object.", __func__))
		return;

	cancel_delayed_work_sync(&req->work);

	c = pm_qos_array[req->pm_qos_class]->constraints;

	if (new_value == req->node.prio) {
		schedule_delayed_work(&req->work, usecs_to_jiffies(timeout_us));
		return;
	}

	if (c->parent_class)
		pm_qos_update_bounded_target(c, new_value, req, req->priority,
					     PM_QOS_UPDATE_REQ);
	else
		pm_qos_update_target(c, &req->node,
				     PM_QOS_UPDATE_REQ, new_value);

	schedule_delayed_work(&req->work, usecs_to_jiffies(timeout_us));
}
EXPORT_SYMBOL_GPL(pm_qos_update_request_timeout);

/**
 * pm_qos_remove_request - modifies an existing qos request
 * @req: handle to request list element
 *
 * Will remove pm qos request from the list of constraints and
 * recompute the current target value for the pm_qos_class.  Call this
 * on slow code paths.
 */
void pm_qos_remove_request(struct pm_qos_request *req)
{
	struct pm_qos_constraints *c;

	if (!req) /*guard against callers passing in null */
		return;
		/* silent return to keep pcm code cleaner */

	if (!pm_qos_request_active(req)) {
		WARN(1, KERN_ERR "pm_qos_remove_request() called for unknown object\n");
		return;
	}

	cancel_delayed_work_sync(&req->work);

	c = pm_qos_array[req->pm_qos_class]->constraints;
	if (c->parent_class)
		pm_qos_update_bounded_target(c, PM_QOS_DEFAULT_VALUE, req,
					     req->priority, PM_QOS_REMOVE_REQ);
	else
		pm_qos_update_target(c, &req->node, PM_QOS_REMOVE_REQ,
				     PM_QOS_DEFAULT_VALUE);
	memset(req, 0, sizeof(*req));
}
EXPORT_SYMBOL_GPL(pm_qos_remove_request);

/**
 * pm_qos_add_min_bound_req - adds a new minimum bound for a constraint
 * @req: handle to request being added
 * @priority: priority of the request being added. enum pm_qos_bound_priority
 * @pm_qos_bounded_class: the bounded constraint id this min bound applies to
 * @val: value of the min bound request
 */
void pm_qos_add_min_bound_req(struct pm_qos_request *req, int priority,
			      int pm_qos_bounded_class, s32 val)
{
	struct pm_qos_bounded_constraint *c;

	if (!req)
		return;

	if (pm_qos_request_active(req)) {
		WARN(1, KERN_ERR "pm_qos_add_min_bound_req() called for already added request\n");
		return;
	}

	c = pm_qos_bounded_obj_array[pm_qos_bounded_class]->bounds;
	req->pm_qos_class = c->min_class;
	req->priority = priority;
	INIT_DELAYED_WORK(&req->work, pm_qos_work_fn);

	pm_qos_update_bounded_target(pm_qos_array[c->min_class]->constraints,
				     val, req, req->priority, PM_QOS_ADD_REQ);
}
EXPORT_SYMBOL_GPL(pm_qos_add_min_bound_req);

/**
 * pm_qos_add_max_bound_req - adds a new maximum bound for a constraint
 * @req: handle to request being added
 * @priority: priority of the request being added. enum pm_qos_bound_priority
 * @pm_qos_bounded_class: the bounded constraint id this max bound applies to
 * @val: value of the max bound request
 */
void pm_qos_add_max_bound_req(struct pm_qos_request *req, int priority,
			      int pm_qos_bounded_class, s32 val)
{
	struct pm_qos_bounded_constraint *c;

	if (!req)
		return;

	if (pm_qos_request_active(req)) {
		WARN(1, KERN_ERR "pm_qos_add_max_bound_req() called for already added request\n");
		return;
	}

	c = pm_qos_bounded_obj_array[pm_qos_bounded_class]->bounds;
	req->pm_qos_class = c->max_class;
	req->priority = priority;
	INIT_DELAYED_WORK(&req->work, pm_qos_work_fn);

	pm_qos_update_bounded_target(pm_qos_array[c->max_class]->constraints,
				     val, req, req->priority, PM_QOS_ADD_REQ);
}
EXPORT_SYMBOL_GPL(pm_qos_add_max_bound_req);

/**
 * pm_qos_update_bounded_req - updates the requested bound for the constraint
 * @req: handle to the request being updated
 * @priority: priority to use for the request
 * @val: updated value to use for the request
 *
 * Updating a request also resets any previous timeouts requests for the
 * given request handle
 */
void pm_qos_update_bounded_req(struct pm_qos_request *req, int priority,
			       s32 val)
{
	struct pm_qos_constraints *c;
	if (!req)
		return;

	if (!pm_qos_request_active(req)) {
		WARN(1, KERN_ERR "pm_qos_update_bounded_req() called for unknown object\n");
		return;
	}

	cancel_delayed_work_sync(&req->work);

	c = pm_qos_array[req->pm_qos_class]->constraints;

	if (val == req->node.prio && priority == req->priority)
		return;

	pm_qos_update_bounded_target(c, val, req, priority,
				     PM_QOS_UPDATE_REQ);
	req->priority = priority;
}
EXPORT_SYMBOL_GPL(pm_qos_update_bounded_req);

/**
 * pm_qos_update_bounded_req_timeout - updates the timeout for the request
 * @req: handle to the request for which timeout is updated
 * @timeout_us: new timeout value to use in usecs
 */
void pm_qos_update_bounded_req_timeout(struct pm_qos_request *req,
				       unsigned long timeout_us)
{
	pm_qos_update_request_timeout(req, req->node.prio, timeout_us);
}
EXPORT_SYMBOL_GPL(pm_qos_update_bounded_req_timeout);

/**
 * pm_qos_remove_bounded_req - removes the requested bound
 * @req: handle to the bound to remove
 *
 * Removes the requested type of bound for the bounded constraint
 * and ensures that the target bounds are updated properly
 */
void pm_qos_remove_bounded_req(struct pm_qos_request *req)
{
	pm_qos_remove_request(req);
}
EXPORT_SYMBOL_GPL(pm_qos_remove_bounded_req);

/**
 * pm_qos_add_min_notifier - adds a notifier for the minimum bound
 * @pm_qos_bounded_class: class id of the bounded constraint
 * @notifier: notifier to add to the notifier list for min bound
 *
 * Notifier is called when there is a change to the minimum bound of
 * the bounded constraint
 */
void pm_qos_add_min_notifier(int pm_qos_bounded_class,
			     struct notifier_block *notifier)
{
	struct pm_qos_bounded_constraint *c;

	c = pm_qos_bounded_obj_array[pm_qos_bounded_class]->bounds;
	pm_qos_remove_notifier(c->min_class, notifier);
}
EXPORT_SYMBOL_GPL(pm_qos_add_min_notifier);

/**
 * pm_qos_add_max_notifier - adds a notifier for the maximum bound
 * @pm_qos_bounded_class: class id of the bounded constraint
 * @notifier: notifier to add to the notifier list for min bound
 *
 * Notifier is called when there is a change to the maximum bound of
 * the bounded constraint
 */
void pm_qos_add_max_notifier(int pm_qos_bounded_class,
			     struct notifier_block *notifier)
{
	struct pm_qos_bounded_constraint *c;

	c = pm_qos_bounded_obj_array[pm_qos_bounded_class]->bounds;
	pm_qos_add_notifier(c->max_class, notifier);
}
EXPORT_SYMBOL_GPL(pm_qos_add_max_notifier);

/**
 * pm_qos_remove_min_notifier - removes notifier for the minimum bound
 * @pm_qos_bounded_class: class id of the bounded constraint
 * @notifier: notifier to remove from the notifier list for min bound
 */
void pm_qos_remove_min_notifier(int pm_qos_bounded_class,
				struct notifier_block *notifier)
{
	struct pm_qos_bounded_constraint *c;

	c = pm_qos_bounded_obj_array[pm_qos_bounded_class]->bounds;
	pm_qos_remove_notifier(c->min_class, notifier);
}
EXPORT_SYMBOL_GPL(pm_qos_remove_min_notifier);

/**
 * pm_qos_remove_max_notifier - removes notifier for the maximum bound
 * @pm_qos_bounded_class: class id of the bounded constraint
 * @notifier: notifier to remove from the notifier list for max bound
 */
void pm_qos_remove_max_notifier(int pm_qos_bounded_class,
				struct notifier_block *notifier)
{
	struct pm_qos_bounded_constraint *c;

	c = pm_qos_bounded_obj_array[pm_qos_bounded_class]->bounds;
	pm_qos_remove_notifier(c->max_class, notifier);
}
EXPORT_SYMBOL_GPL(pm_qos_remove_max_notifier);

/**
 * pm_qos_read_min_bound - gets the current min bound set by all requests
 * @pm_qos_bounded_class: class id of the bounded constraint
 */
s32 pm_qos_read_min_bound(int pm_qos_bounded_class)
{
	struct pm_qos_bounded_constraint *c;
	struct pm_qos_constraints *bound;

	c = pm_qos_bounded_obj_array[pm_qos_bounded_class]->bounds;
	bound = pm_qos_array[c->min_class]->constraints;
	return pm_qos_read_value(bound);
}
EXPORT_SYMBOL_GPL(pm_qos_read_min_bound);

/**
 * pm_qos_read_max_bound - gets the current max bound set by all requests
 * @pm_qos_bounded_class: class id of the bounded constraint
 */
s32 pm_qos_read_max_bound(int pm_qos_bounded_class)
{
	struct pm_qos_bounded_constraint *c;
	struct pm_qos_constraints *bound;

	c = pm_qos_bounded_obj_array[pm_qos_bounded_class]->bounds;
	bound = pm_qos_array[c->max_class]->constraints;
	return pm_qos_read_value(bound);
}
EXPORT_SYMBOL_GPL(pm_qos_read_max_bound);

static int pm_qos_enabled_set(const char *arg, const struct kernel_param *kp)
{
	bool old;
	s32 prev[PM_QOS_NUM_CLASSES], curr[PM_QOS_NUM_CLASSES];
	int ret, i;
	struct pm_qos_constraints *c;
	struct pm_qos_bounded_constraint *parent;

	old = pm_qos_enabled;
	ret = param_set_bool(arg, kp);
	if (ret != 0) {
		pr_warn("%s: cannot set PM QoS enable to %s\n",
			__FUNCTION__, arg);
		return ret;
	}
	mutex_lock(&pm_qos_lock);
	for (i = 1; i < PM_QOS_NUM_CLASSES; i++) {
		c = pm_qos_array[i]->constraints;
		prev[i] = pm_qos_read_value(c);

		if (c->parent_class) {
			int class = c->parent_class;
			parent = pm_qos_bounded_obj_array[class]->bounds;
			pm_qos_set_bounded_targets(parent);
			curr[i] = pm_qos_read_value(c);
		} else if (old && !pm_qos_enabled) {
			/* got disabled */
			curr[i] = c->default_value;
			pm_qos_set_value(pm_qos_array[i]->constraints, curr[i]);
		} else if (!old && pm_qos_enabled) {
			/* got enabled */
			curr[i] = pm_qos_get_value(c);
			pm_qos_set_value(pm_qos_array[i]->constraints, curr[i]);
		}
	}
	for (i = 1; i < PM_QOS_NUM_CLASSES; i++)
		if (prev[i] != curr[i])
			blocking_notifier_call_chain(
				pm_qos_array[i]->constraints->notifiers,
				(unsigned long)curr[i],
				NULL);
	mutex_unlock(&pm_qos_lock);
	return ret;
}

static int pm_qos_enabled_get(char *buffer, const struct kernel_param *kp)
{
	return param_get_bool(buffer, kp);
}

static struct kernel_param_ops pm_qos_enabled_ops = {
	.set = pm_qos_enabled_set,
	.get = pm_qos_enabled_get,
};

module_param_cb(enable, &pm_qos_enabled_ops, &pm_qos_enabled, 0644);

/**
 * pm_qos_add_notifier - sets notification entry for changes to target value
 * @pm_qos_class: identifies which qos target changes should be notified.
 * @notifier: notifier block managed by caller.
 *
 * will register the notifier into a notification chain that gets called
 * upon changes to the pm_qos_class target value.
 */
int pm_qos_add_notifier(int pm_qos_class, struct notifier_block *notifier)
{
	int retval;

	retval = blocking_notifier_chain_register(
			pm_qos_array[pm_qos_class]->constraints->notifiers,
			notifier);

	return retval;
}
EXPORT_SYMBOL_GPL(pm_qos_add_notifier);

/**
 * pm_qos_remove_notifier - deletes notification entry from chain.
 * @pm_qos_class: identifies which qos target changes are notified.
 * @notifier: notifier block to be removed.
 *
 * will remove the notifier from the notification chain that gets called
 * upon changes to the pm_qos_class target value.
 */
int pm_qos_remove_notifier(int pm_qos_class, struct notifier_block *notifier)
{
	int retval;

	retval = blocking_notifier_chain_unregister(
			pm_qos_array[pm_qos_class]->constraints->notifiers,
			notifier);

	return retval;
}
EXPORT_SYMBOL_GPL(pm_qos_remove_notifier);

/* User space interface to PM QoS classes via misc devices */
static int register_pm_qos_misc(struct pm_qos_object *qos)
{
	qos->pm_qos_power_miscdev.minor = MISC_DYNAMIC_MINOR;
	qos->pm_qos_power_miscdev.name = qos->name;
	qos->pm_qos_power_miscdev.fops = &pm_qos_power_fops;

	return misc_register(&qos->pm_qos_power_miscdev);
}

static int find_pm_qos_object_by_minor(int minor)
{
	int pm_qos_class;

	for (pm_qos_class = 0;
		pm_qos_class < PM_QOS_NUM_CLASSES; pm_qos_class++) {
		if (minor ==
			pm_qos_array[pm_qos_class]->pm_qos_power_miscdev.minor)
			return pm_qos_class;
	}
	return -1;
}

static int pm_qos_power_open(struct inode *inode, struct file *filp)
{
	long pm_qos_class;
	struct pm_qos_constraints *c;
	struct pm_qos_request *req;

	pm_qos_class = find_pm_qos_object_by_minor(iminor(inode));
	if (pm_qos_class < 0)
		return -EPERM;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	c = pm_qos_array[pm_qos_class]->constraints;

	if (c->parent_class && c->type == PM_QOS_MAX)
		pm_qos_add_min_bound_req(req, PM_QOS_PRIO_DEFAULT_UNTRUSTED,
					 c->parent_class, PM_QOS_DEFAULT_VALUE);
	else if (c->parent_class && c->type == PM_QOS_MIN)
		pm_qos_add_max_bound_req(req, PM_QOS_PRIO_DEFAULT_UNTRUSTED,
					 c->parent_class, PM_QOS_DEFAULT_VALUE);
	else
		pm_qos_add_request(req, pm_qos_class, PM_QOS_DEFAULT_VALUE);

	filp->private_data = req;

	return 0;
}

static int pm_qos_power_release(struct inode *inode, struct file *filp)
{
	struct pm_qos_request *req;

	req = filp->private_data;
	pm_qos_remove_request(req);
	kfree(req);

	return 0;
}


static ssize_t pm_qos_power_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value;
	struct pm_qos_request *req = filp->private_data;
	struct pm_qos_constraints *c;

	if (!req)
		return -EINVAL;
	if (!pm_qos_request_active(req))
		return -EINVAL;

	mutex_lock(&pm_qos_lock);
	c = pm_qos_array[req->pm_qos_class]->constraints;
	if (c->parent_class)
		value = pm_qos_read_value(c);
	else
		value = pm_qos_get_value(c);
	mutex_unlock(&pm_qos_lock);

	return simple_read_from_buffer(buf, count, f_pos, &value, sizeof(s32));
}

static ssize_t pm_qos_power_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value;
	struct pm_qos_request *req;

	if (count == sizeof(s32)) {
		if (copy_from_user(&value, buf, sizeof(s32)))
			return -EFAULT;
	} else if (count <= 11) { /* ASCII perhaps? */
		char ascii_value[11];
		unsigned long int ulval;
		int ret;

		if (copy_from_user(ascii_value, buf, count))
			return -EFAULT;

		if (count > 10) {
			if (ascii_value[10] == '\n')
				ascii_value[10] = '\0';
			else
				return -EINVAL;
		} else {
			ascii_value[count] = '\0';
		}
		ret = kstrtoul(ascii_value, 16, &ulval);
		if (ret) {
			pr_debug("%s, 0x%lx, 0x%x\n", ascii_value, ulval, ret);
			return -EINVAL;
		}
		value = (s32)lower_32_bits(ulval);
	} else {
		return -EINVAL;
	}

	req = filp->private_data;
	pm_qos_update_request(req, value);

	return count;
}

/* Userspace interface for bounded constraints */
static int register_pm_qos_bounded_obj(struct pm_qos_bounded_object *qos)
{
	qos->miscdev.minor = MISC_DYNAMIC_MINOR;
	qos->miscdev.name = qos->name;
	qos->miscdev.fops = &pm_qos_bounded_constraint_fops;

	return misc_register(&qos->miscdev);
}

static int find_pm_qos_bounded_obj_by_minor(int minor)
{
	int class;
	for (class = 0; class < PM_QOS_NUM_BOUNDED_CLASSES; class++)
		if (minor == pm_qos_bounded_obj_array[class]->miscdev.minor)
			return class;
	return -1;
}

struct pm_qos_bounded_user_req {
	struct pm_qos_request min_req;
	struct pm_qos_request max_req;
};

/* Represents the userspace input */
struct pm_qos_bounded_input {
	s32 max;
	s32 min;
	s32 priority;
	s32 timeout_ms;
};

#define MAX_READ_BYTES	3000
#define MAX_WRITE_BYTES	50

/*
 * Assumes input is a maximum of 4 s32 ASCII base10 numbers space delimited
 * Order of input is max, min, priority and timeout
 */
static ssize_t pm_qos_bounded_write(struct file *filp, const char __user *buf,
				    size_t count, loff_t *f_pos)
{
	int i, ret;
	char *input, *val, *tmp;
	struct pm_qos_bounded_input value;
	struct pm_qos_constraints *max_constraint, *min_constraint;
	struct pm_qos_bounded_user_req *req = filp->private_data;
	s32 * const value_array[] = {
		&value.max, &value.min, &value.priority, &value.timeout_ms
	};

	if (!count || count >= MAX_WRITE_BYTES)
		return -EINVAL;

	input = kzalloc(count, GFP_KERNEL);
	tmp = input;
	if (!input)
		return -ENOMEM;

	if (copy_from_user(input, buf, count)) {
		kfree(tmp);
		return -EFAULT;
	}
	memset(&value, 0, sizeof(value));
	max_constraint = pm_qos_array[req->max_req.pm_qos_class]->constraints;
	min_constraint = pm_qos_array[req->min_req.pm_qos_class]->constraints;
	value.max = max_constraint->default_value;
	value.min = min_constraint->default_value;
	i = 0;
	while ((val = strsep(&input, " ")) != NULL) {
		if (!val || *val == '\0')
			break;
		ret = kstrtoint(val, 10, value_array[i]);
		if (ret) {
			pr_debug("%s, %d, %x\n", val, *value_array[i], ret);
			kfree(tmp);
			return -EINVAL;
		}
		i++;
		if (i >= ARRAY_SIZE(value_array))
			break;
	}

	if (value.priority <= PM_QOS_PRIO_TRUSTED ||
	    value.priority >= PM_QOS_NUM_PRIO)
		value.priority = PM_QOS_PRIO_DEFAULT_UNTRUSTED;

	pm_qos_update_bounded_req(&req->max_req, value.priority, value.max);
	pm_qos_update_bounded_req(&req->min_req, value.priority, value.min);

	if (value.timeout_ms > 0) {
		pm_qos_update_bounded_req_timeout(&req->max_req,
						  value.timeout_ms * 1000);
		pm_qos_update_bounded_req_timeout(&req->min_req,
						  value.timeout_ms * 1000);
	}

	kfree(tmp);
	return count;
}

/*
 * Read out the intersection of all ranges with priority greater than or
 * equal to at every priority level. If the intersection set is NULL the
 * higher priority bound is taken. Returns the total number of bytes read.
 * If count (max bytes to read) is not large enough to read the ranges
 * for all priorities -EFAULT is returned
 */
static ssize_t pm_qos_bounded_read(struct file *filp, char __user *buf,
				   size_t count, loff_t *f_pos)
{
	char *output;
	struct pm_qos_constraints *max_constraint;
	struct pm_qos_bounded_constraint *parent;
	size_t bytes_read;
	ssize_t ret;
	struct pm_qos_bounded_user_req *req = filp->private_data;
	max_constraint = pm_qos_array[req->max_req.pm_qos_class]->constraints;
	parent = pm_qos_bounded_obj_array[max_constraint->parent_class]->bounds;

	/* Finished reading */
	if (*f_pos)
		return 0;

	output = kzalloc(MAX_READ_BYTES, GFP_KERNEL);
	if (!output)
		return -ENOMEM;

	mutex_lock(&pm_qos_lock);
	bytes_read = pm_qos_find_bounded_targets(parent, NULL, NULL,
						 output, MAX_READ_BYTES);
	mutex_unlock(&pm_qos_lock);

	if (bytes_read > count)
		goto err;

	ret = copy_to_user(buf, output, bytes_read);
	if (bytes_read == ret)
		goto err;

	*f_pos += bytes_read - ret;

	kfree(output);
	return bytes_read - ret;
err:
	kfree(output);
	return -EFAULT;
}

static int pm_qos_bounded_open(struct inode *inode, struct file *filp)
{
	long class;
	struct pm_qos_bounded_user_req *req;

	class = find_pm_qos_bounded_obj_by_minor(iminor(inode));
	if (class < 0)
		return -EPERM;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	pm_qos_add_max_bound_req(&req->max_req, PM_QOS_PRIO_DEFAULT_UNTRUSTED,
				 class, PM_QOS_DEFAULT_VALUE);
	pm_qos_add_min_bound_req(&req->min_req, PM_QOS_PRIO_DEFAULT_UNTRUSTED,
				 class, PM_QOS_DEFAULT_VALUE);

	filp->private_data = req;

	return 0;
}

static int pm_qos_bounded_release(struct inode *inode, struct file *filp)
{
	struct pm_qos_bounded_user_req *req;

	req = filp->private_data;
	pm_qos_remove_bounded_req(&req->max_req);
	pm_qos_remove_bounded_req(&req->min_req);

	kfree(req);

	return 0;
}

static int __init pm_qos_power_init(void)
{
	int ret = 0;
	int i;

	BUILD_BUG_ON(ARRAY_SIZE(pm_qos_array) != PM_QOS_NUM_CLASSES);
	BUILD_BUG_ON(ARRAY_SIZE(pm_qos_bounded_obj_array) !=
		     PM_QOS_NUM_BOUNDED_CLASSES);

	for (i = 1; i < PM_QOS_NUM_CLASSES; i++) {
		ret = register_pm_qos_misc(pm_qos_array[i]);
		if (ret < 0) {
			printk(KERN_ERR "pm_qos_param: %s setup failed\n",
			       pm_qos_array[i]->name);
			return ret;
		}
	}
	for (i = 1; i < PM_QOS_NUM_BOUNDED_CLASSES; i++) {
		ret = register_pm_qos_bounded_obj(pm_qos_bounded_obj_array[i]);
		if (ret < 0) {
			pr_err("pm_qos_bounded_reg: %s setup failed\n",
			       pm_qos_bounded_obj_array[i]->name);
			return ret;
		}
	}

	return ret;
}

late_initcall(pm_qos_power_init);
