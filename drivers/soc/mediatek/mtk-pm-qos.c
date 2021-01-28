// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/pm_qos.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
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
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/module.h>

#include <linux/uaccess.h>
#include <linux/export.h>
#define CREATE_TRACE_POINTS
#include <mtk-pm-qos-trace.h>

struct mtk_pm_qos_object {
	struct pm_qos_constraints *constraints;
	struct mutex qos_lock;
	struct list_head req_list;
	char *name;
};

static DEFINE_SPINLOCK(mtk_pm_qos_lock);

static struct mtk_pm_qos_object null_mtk_pm_qos;

static BLOCKING_NOTIFIER_HEAD(memory_bandwidth_notifier);
static struct pm_qos_constraints memory_bw_constraints = {
	.list = PLIST_HEAD_INIT(memory_bw_constraints.list),
	.target_value = MTK_PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_MEMORY_BANDWIDTH_DEFAULT_VALUE,
	.type = PM_QOS_SUM,
	.notifiers = &memory_bandwidth_notifier,
};
static struct mtk_pm_qos_object memory_bandwidth_mtk_pm_qos = {
	.constraints = &memory_bw_constraints,
	.req_list = LIST_HEAD_INIT(memory_bandwidth_mtk_pm_qos.req_list),
	.qos_lock = __MUTEX_INITIALIZER(memory_bandwidth_mtk_pm_qos.qos_lock),
	.name = "memory_bandwidth",
};

static BLOCKING_NOTIFIER_HEAD(ddr_opp_notifier);
static struct pm_qos_constraints ddr_opp_constraints = {
	.list = PLIST_HEAD_INIT(ddr_opp_constraints.list),
	.target_value = MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_DDR_OPP_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.notifiers = &ddr_opp_notifier,
};
static struct mtk_pm_qos_object ddr_opp_mtk_pm_qos = {
	.constraints = &ddr_opp_constraints,
	.req_list = LIST_HEAD_INIT(ddr_opp_mtk_pm_qos.req_list),
	.qos_lock = __MUTEX_INITIALIZER(ddr_opp_mtk_pm_qos.qos_lock),
	.name = "ddr_opp",
};

static BLOCKING_NOTIFIER_HEAD(vcore_opp_notifier);
static struct pm_qos_constraints vcore_opp_constraints = {
	.list = PLIST_HEAD_INIT(vcore_opp_constraints.list),
	.target_value = MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_VCORE_OPP_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.notifiers = &vcore_opp_notifier,
};
static struct mtk_pm_qos_object vcore_opp_mtk_pm_qos = {
	.constraints = &vcore_opp_constraints,
	.req_list = LIST_HEAD_INIT(vcore_opp_mtk_pm_qos.req_list),
	.qos_lock = __MUTEX_INITIALIZER(vcore_opp_mtk_pm_qos.qos_lock),
	.name = "vcore_opp",
};

static BLOCKING_NOTIFIER_HEAD(scp_vcore_req_notifier);
static struct pm_qos_constraints scp_vcore_req_constraints = {
	.list = PLIST_HEAD_INIT(scp_vcore_req_constraints.list),
	.target_value = MTK_PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_SCP_VCORE_REQUEST_DEFAULT_VALUE,
	.type = PM_QOS_MIN,
	.notifiers = &scp_vcore_req_notifier,
};
static struct mtk_pm_qos_object scp_vcore_req_mtk_pm_qos = {
	.constraints = &scp_vcore_req_constraints,
	.req_list = LIST_HEAD_INIT(scp_vcore_req_mtk_pm_qos.req_list),
	.qos_lock = __MUTEX_INITIALIZER(scp_vcore_req_mtk_pm_qos.qos_lock),
	.name = "scp_vcore_req",
};

static BLOCKING_NOTIFIER_HEAD(hrt_bandwidth_notifier);
static struct pm_qos_constraints hrt_bw_constraints = {
	.list = PLIST_HEAD_INIT(hrt_bw_constraints.list),
	.target_value = MTK_PM_QOS_HRT_BANDWIDTH_DEFAULT_VALUE,
	.default_value = MTK_PM_QOS_HRT_BANDWIDTH_DEFAULT_VALUE,
	.no_constraint_value = MTK_PM_QOS_HRT_BANDWIDTH_DEFAULT_VALUE,
	.type = PM_QOS_SUM,
	.notifiers = &hrt_bandwidth_notifier,
};
static struct mtk_pm_qos_object hrt_bandwidth_mtk_pm_qos = {
	.constraints = &hrt_bw_constraints,
	.req_list = LIST_HEAD_INIT(hrt_bandwidth_mtk_pm_qos.req_list),
	.qos_lock = __MUTEX_INITIALIZER(hrt_bandwidth_mtk_pm_qos.qos_lock),
	.name = "hrt_bandwidth",
};

static struct mtk_pm_qos_object *mtk_pm_qos_array[] = {
	[MTK_PM_QOS_RESERVED] = &null_mtk_pm_qos,

	[MTK_PM_QOS_MEMORY_BANDWIDTH] = &memory_bandwidth_mtk_pm_qos,
	[MTK_PM_QOS_HRT_BANDWIDTH] = &hrt_bandwidth_mtk_pm_qos,
	[MTK_PM_QOS_DDR_OPP] = &ddr_opp_mtk_pm_qos,
	[MTK_PM_QOS_VCORE_OPP] = &vcore_opp_mtk_pm_qos,
	[MTK_PM_QOS_SCP_VCORE_REQUEST] = &scp_vcore_req_mtk_pm_qos,
};

/* unlocked internal variant */
static inline int  mtk_pm_qos_get_value(struct pm_qos_constraints *c)
{
	struct plist_node *node;
	int total_value = 0;

	if (plist_head_empty(&c->list))
		return c->no_constraint_value;

	switch (c->type) {
	case PM_QOS_MIN:
		return plist_first(&c->list)->prio;

	case PM_QOS_MAX:
		return plist_last(&c->list)->prio;

	case PM_QOS_SUM:
		plist_for_each(node, &c->list)
			total_value += node->prio;

		return total_value;

	default:
		/* runtime check for not using enum */
		WARN_ON(1);
		return PM_QOS_DEFAULT_VALUE;
	}
}

void mtk_pm_qos_trace_dbg_show_request(int pm_qos_class)
{
	struct pm_qos_constraints *c;
	struct mtk_pm_qos_request *req;
	unsigned long flags;

	if (pm_qos_class < MTK_PM_QOS_RESERVED
		|| pm_qos_class >= MTK_PM_QOS_NUM_CLASSES)
		return;

	c = mtk_pm_qos_array[pm_qos_class]->constraints;
	spin_lock_irqsave(&mtk_pm_qos_lock, flags);

	plist_for_each_entry(req, &c->list, node) {
		trace_mtk_pm_qos_update_request(req->pm_qos_class,
			req->node.prio, req->owner);
	}
	spin_unlock_irqrestore(&mtk_pm_qos_lock, flags);
}
EXPORT_SYMBOL_GPL(mtk_pm_qos_trace_dbg_show_request);

static int mtk_pm_qos_dbg_show_requests(struct seq_file *s, void *unused)
{
	struct mtk_pm_qos_object *qos = (struct mtk_pm_qos_object *)s->private;
	struct pm_qos_constraints *c;
	struct mtk_pm_qos_request *req;
	char *type;
	unsigned long flags;
	int tot_reqs = 0;
	int active_reqs = 0;

	if (IS_ERR_OR_NULL(qos)) {
		pr_err("%s: bad qos param!\n", __func__);
		return -EINVAL;
	}
	c = qos->constraints;
	if (IS_ERR_OR_NULL(c)) {
		pr_err("%s: Bad constraints on qos?\n", __func__);
		return -EINVAL;
	}

	/* Lock to ensure we have a snapshot */
	spin_lock_irqsave(&mtk_pm_qos_lock, flags);
	if (plist_head_empty(&c->list)) {
		seq_puts(s, "Empty!\n");
		goto out;
	}

	switch (c->type) {
	case PM_QOS_MIN:
		type = "Minimum";
		break;
	case PM_QOS_MAX:
		type = "Maximum";
		break;
	case PM_QOS_SUM:
		type = "Sum";
		break;
	default:
		type = "Unknown";
	}

	plist_for_each_entry(req, &c->list, node) {
		char *state = "Default";

		if ((req->node).prio != c->default_value) {
			active_reqs++;
			state = "Active";
		}
		tot_reqs++;
		seq_printf(s, "[%s] : %d: %s\n", req->owner,
			   (req->node).prio, state);
	}

	seq_printf(s, "Type=%s, Value=%d, Requests: active=%d / total=%d\n",
		   type, mtk_pm_qos_get_value(c), active_reqs, tot_reqs);

out:
	spin_unlock_irqrestore(&mtk_pm_qos_lock, flags);
	return 0;
}

static int mtk_pm_qos_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, mtk_pm_qos_dbg_show_requests,
			   inode->i_private);
}

static const struct file_operations mtk_pm_qos_debug_fops = {
	.open           = mtk_pm_qos_dbg_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};

/**
 * plist_add - add @node to @head
 *
 * @node:	&struct plist_node pointer
 * @head:	&struct plist_head pointer
 */
static void mtk_pm_qos_plist_add(struct plist_node *node,
	struct plist_head *head)
{
	struct plist_node *first, *iter, *prev = NULL;
	struct list_head *node_next = &head->node_list;

	WARN_ON(!plist_node_empty(node));
	WARN_ON(!list_empty(&node->prio_list));

	if (plist_head_empty(head))
		goto ins_node;

	first = iter = plist_first(head);

	do {
		if (node->prio < iter->prio) {
			node_next = &iter->node_list;
			break;
		}

		prev = iter;
		iter = list_entry(iter->prio_list.next,
				struct plist_node, prio_list);
	} while (iter != first);

	if (!prev || prev->prio != node->prio)
		list_add_tail(&node->prio_list, &iter->prio_list);
ins_node:
	list_add_tail(&node->node_list, node_next);

}

/**
 * plist_del - Remove a @node from plist.
 *
 * @node:	&struct plist_node pointer - entry to be removed
 * @head:	&struct plist_head pointer - list head
 */
static void mtk_pm_qos_plist_del(struct plist_node *node,
	struct plist_head *head)
{
	if (!list_empty(&node->prio_list)) {
		if (node->node_list.next != &head->node_list) {
			struct plist_node *next;

			next = list_entry(node->node_list.next,
					struct plist_node, node_list);

			/* add the next plist_node into prio_list */
			if (list_empty(&next->prio_list))
				list_add(&next->prio_list, &node->prio_list);
		}
		list_del_init(&node->prio_list);
	}

	list_del_init(&node->node_list);

}

static int mtk_pm_qos_request_active(struct mtk_pm_qos_request *req)
{
	return req->pm_qos_class != 0;
}

static void mtk_pm_qos_update_target_req_list(struct mtk_pm_qos_object *c,
		struct mtk_pm_qos_request *req, enum pm_qos_req_action action)
{
	unsigned long flags;

	spin_lock_irqsave(&mtk_pm_qos_lock, flags);

	switch (action) {
	case PM_QOS_REMOVE_REQ:
		list_del(&req->list_node);
		break;
	case PM_QOS_UPDATE_REQ:
		/*
		 * to change the list, we atomically remove, reinit
		 * with new value and add, then see if the extremal
		 * changed
		 */
		list_del(&req->list_node);
	case PM_QOS_ADD_REQ:
		list_add(&req->list_node, &c->req_list);
		break;
	default:
		/* no action */
		break;
	}

	spin_unlock_irqrestore(&mtk_pm_qos_lock, flags);
}

static inline void mtk_pm_qos_set_value(struct pm_qos_constraints *c, s32 value)
{
	c->target_value = value;
}

/**
 * mtk_pm_qos_request - returns current system wide qos expectation
 * @pm_qos_class: identification of which qos value is requested
 *
 * This function returns the current target value.
 */
int mtk_pm_qos_request(int mtk_pm_qos_class)
{
	return mtk_pm_qos_array[mtk_pm_qos_class]->constraints->target_value;
}
EXPORT_SYMBOL_GPL(mtk_pm_qos_request);


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
static int __mtk_pm_qos_update_target(struct pm_qos_constraints *c,
	struct plist_node *node, enum pm_qos_req_action action, int value)
{
	unsigned long flags;
	int prev_value, curr_value, new_value;
	int ret;

	spin_lock_irqsave(&mtk_pm_qos_lock, flags);
	prev_value = mtk_pm_qos_get_value(c);
	if (value == PM_QOS_DEFAULT_VALUE)
		new_value = c->default_value;
	else
		new_value = value;

	switch (action) {
	case PM_QOS_REMOVE_REQ:
		mtk_pm_qos_plist_del(node, &c->list);
		break;
	case PM_QOS_UPDATE_REQ:
		/*
		 * to change the list, we atomically remove, reinit
		 * with new value and add, then see if the extremal
		 * changed
		 */
		mtk_pm_qos_plist_del(node, &c->list);
		/* fall through */
	case PM_QOS_ADD_REQ:
		plist_node_init(node, new_value);
		mtk_pm_qos_plist_add(node, &c->list);
		break;
	default:
		/* no action */
		break;
	}

	curr_value = mtk_pm_qos_get_value(c);
	mtk_pm_qos_set_value(c, curr_value);

	spin_unlock_irqrestore(&mtk_pm_qos_lock, flags);

	if (prev_value != curr_value) {
		ret = 1;
		if (c->notifiers)
			blocking_notifier_call_chain(c->notifiers,
						     (unsigned long)curr_value,
						     NULL);
	} else {
		ret = 0;
	}
	return ret;
}

static void mtk_pm_qos_update_target(struct mtk_pm_qos_object *obj,
	struct plist_node *node, enum pm_qos_req_action action, int value)
{
	mutex_lock(&obj->qos_lock);
	__mtk_pm_qos_update_target(obj->constraints, node, action, value);
	mutex_unlock(&obj->qos_lock);
}

/**
 * mtk_pm_qos_add_request - inserts new qos request into the list
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

void mtk_pm_qos_add_request(struct mtk_pm_qos_request *req,
			int pm_qos_class, s32 value)
{
	char owner[20];

	if (!req) /*guard against callers passing in null */
		return;

	snprintf(owner, sizeof(owner) - 1, "%pS", __builtin_return_address(0));

	if (mtk_pm_qos_request_active(req)) {
		pr_err("%s: called for already added request\n", __func__);
		return;
	}

	/* name of mtk_pm_qos reqester */
	strncpy(req->owner, owner, sizeof(req->owner) - 1);

	req->pm_qos_class = pm_qos_class;
	trace_mtk_pm_qos_add_request(pm_qos_class, value, req->owner);
	mtk_pm_qos_update_target(
		mtk_pm_qos_array[pm_qos_class],
		&req->node, PM_QOS_ADD_REQ, value);

	mtk_pm_qos_update_target_req_list(
		mtk_pm_qos_array[req->pm_qos_class],
		req, PM_QOS_ADD_REQ);
}
EXPORT_SYMBOL_GPL(mtk_pm_qos_add_request);

/**
 * mtk_pm_qos_update_request - modifies an existing qos request
 * @req : handle to list element holding a mtk_pm_qos request to use
 * @value: defines the qos request
 *
 * Updates an existing qos request for the pm_qos_class of parameters along
 * with updating the target pm_qos_class value.
 *
 * Attempts are made to make this code callable on hot code paths.
 */
void mtk_pm_qos_update_request(struct mtk_pm_qos_request *req,
			   s32 new_value)
{
	if (!req) /*guard against callers passing in null */
		return;

	if (!mtk_pm_qos_request_active(req)) {
		pr_err("%s: called for unknown object\n", __func__);
		return;
	}

	trace_mtk_pm_qos_update_request(req->pm_qos_class,
		new_value, req->owner);

	if (new_value != req->node.prio) {
		mtk_pm_qos_update_target(
			mtk_pm_qos_array[req->pm_qos_class],
			&req->node, PM_QOS_UPDATE_REQ, new_value);

		mtk_pm_qos_update_target_req_list(
			mtk_pm_qos_array[req->pm_qos_class],
			req, PM_QOS_UPDATE_REQ);
	}
}
EXPORT_SYMBOL_GPL(mtk_pm_qos_update_request);

/**
 * mtk_pm_qos_remove_request - modifies an existing qos request
 * @req: handle to request list element
 *
 * Will remove pm qos request from the list of constraints and
 * recompute the current target value for the pm_qos_class.  Call this
 * on slow code paths.
 */
void mtk_pm_qos_remove_request(struct mtk_pm_qos_request *req)
{
	if (!req) /*guard against callers passing in null */
		return;
		/* silent return to keep pcm code cleaner */

	if (!mtk_pm_qos_request_active(req)) {
		pr_err("%s: called for unknown object\n", __func__);
		return;
	}

	trace_mtk_pm_qos_remove_request(req->pm_qos_class, PM_QOS_DEFAULT_VALUE,
			req->owner);

	mtk_pm_qos_update_target(
		mtk_pm_qos_array[req->pm_qos_class],
		&req->node, PM_QOS_REMOVE_REQ, PM_QOS_DEFAULT_VALUE);

	mtk_pm_qos_update_target_req_list(
		mtk_pm_qos_array[req->pm_qos_class],
		req, PM_QOS_REMOVE_REQ);
}
EXPORT_SYMBOL_GPL(mtk_pm_qos_remove_request);

/**
 * mtk_pm_qos_add_notifier - sets notification entry for changes to target value
 * @pm_qos_class: identifies which qos target changes should be notified.
 * @notifier: notifier block managed by caller.
 *
 * will register the notifier into a notification chain that gets called
 * upon changes to the pm_qos_class target value.
 */
int mtk_pm_qos_add_notifier(int pm_qos_class, struct notifier_block *notifier)
{
	int retval;

	retval = blocking_notifier_chain_register(
			mtk_pm_qos_array[pm_qos_class]->constraints->notifiers,
			notifier);

	return retval;
}
EXPORT_SYMBOL_GPL(mtk_pm_qos_add_notifier);

/**
 * mtk_pm_qos_remove_notifier - deletes notification entry from chain.
 * @pm_qos_class: identifies which qos target changes are notified.
 * @notifier: notifier block to be removed.
 *
 * will remove the notifier from the notification chain that gets called
 * upon changes to the pm_qos_class target value.
 */
int mtk_pm_qos_remove_notifier(int pm_qos_class,
	struct notifier_block *notifier)
{
	int retval;

	retval = blocking_notifier_chain_unregister(
			mtk_pm_qos_array[pm_qos_class]->constraints->notifiers,
			notifier);

	return retval;
}
EXPORT_SYMBOL_GPL(mtk_pm_qos_remove_notifier);

/* User space interface to PM QoS classes via misc devices */
static int register_mtk_pm_qos_debugfs(struct mtk_pm_qos_object *qos,
	struct dentry *d)
{
	if (d) {
		(void)debugfs_create_file(qos->name, 0444, d,
					  (void *)qos, &mtk_pm_qos_debug_fops);
	}

	return 0;
}

static int __init mtk_pm_qos_power_init(void)
{
	int ret = 0;
	int i;
	struct dentry *d;

	BUILD_BUG_ON(ARRAY_SIZE(mtk_pm_qos_array) != MTK_PM_QOS_NUM_CLASSES);

	d = debugfs_create_dir("mtk_pm_qos", NULL);
	if (IS_ERR_OR_NULL(d))
		d = NULL;

	for (i = MTK_PM_QOS_MEMORY_BANDWIDTH; i < MTK_PM_QOS_NUM_CLASSES; i++)
		register_mtk_pm_qos_debugfs(mtk_pm_qos_array[i], d);

	return ret;
}

late_initcall(mtk_pm_qos_power_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek PMQoS driver");
