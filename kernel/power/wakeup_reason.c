/*
 * kernel/power/wakeup_reason.c
 *
 * Logs the reasons which caused the kernel to resume from
 * the suspend mode.
 *
 * Copyright (C) 2014 Google, Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/wakeup_reason.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/debugfs.h>

#define MAX_WAKEUP_REASON_IRQS 32
static bool suspend_abort;
static char abort_reason[MAX_SUSPEND_ABORT_LEN];

static struct wakeup_irq_node *base_irq_nodes;
static struct wakeup_irq_node *cur_irq_tree;
static int cur_irq_tree_depth;
static LIST_HEAD(wakeup_irqs);

static struct kmem_cache *wakeup_irq_nodes_cache;
static struct kobject *wakeup_reason;
static spinlock_t resume_reason_lock;
bool log_wakeups __read_mostly;
struct completion wakeups_completion;

static ktime_t last_monotime; /* monotonic time before last suspend */
static ktime_t curr_monotime; /* monotonic time after last suspend */
static ktime_t last_stime; /* monotonic boottime offset before last suspend */
static ktime_t curr_stime; /* monotonic boottime offset after last suspend */
#if IS_ENABLED(CONFIG_SUSPEND_TIME)
static unsigned int time_in_suspend_bins[32];
#endif

static void init_wakeup_irq_node(struct wakeup_irq_node *p, int irq)
{
	p->irq = irq;
	p->desc = irq_to_desc(irq);
	p->child = NULL;
	p->parent = NULL;
	p->handled = false;
	INIT_LIST_HEAD(&p->siblings);
	INIT_LIST_HEAD(&p->next);
}

static struct wakeup_irq_node* alloc_irq_node(int irq)
{
	struct wakeup_irq_node *n;

	n = kmem_cache_alloc(wakeup_irq_nodes_cache, GFP_ATOMIC);
	if (!n) {
		pr_warning("Failed to log chained wakeup IRQ %d\n",
			irq);
		return NULL;
	}

	init_wakeup_irq_node(n, irq);
	return n;
}

static struct wakeup_irq_node *
search_siblings(struct wakeup_irq_node *root, int irq)
{
	bool found = false;
	struct wakeup_irq_node *n = NULL;
	BUG_ON(!root);

	if (root->irq == irq)
		return root;

	list_for_each_entry(n, &root->siblings, siblings) {
		if (n->irq == irq) {
			found = true;
			break;
		}
	}

	return found ? n : NULL;
}

static struct wakeup_irq_node *
add_to_siblings(struct wakeup_irq_node *root, int irq)
{
	struct wakeup_irq_node *n;
	if (root) {
		n = search_siblings(root, irq);
		if (n)
			return n;
	}
	n = alloc_irq_node(irq);

	if (n && root)
		list_add(&n->siblings, &root->siblings);
	return n;
}

#ifdef CONFIG_DEDUCE_WAKEUP_REASONS
static struct wakeup_irq_node* add_child(struct wakeup_irq_node *root, int irq)
{
	if (!root->child) {
		root->child = alloc_irq_node(irq);
		if (!root->child)
			return NULL;
		root->child->parent = root;
		return root->child;
	}

	return add_to_siblings(root->child, irq);
}

static struct wakeup_irq_node *find_first_sibling(struct wakeup_irq_node *node)
{
	struct wakeup_irq_node *n;
	if (node->parent)
		return node;
	list_for_each_entry(n, &node->siblings, siblings) {
		if (n->parent)
			return n;
	}
	return NULL;
}

static struct wakeup_irq_node *
get_base_node(struct wakeup_irq_node *node, unsigned depth)
{
	if (!node)
		return NULL;

	while (depth) {
		node = find_first_sibling(node);
		BUG_ON(!node);
		node = node->parent;
		depth--;
	}

	return node;
}
#endif /* CONFIG_DEDUCE_WAKEUP_REASONS */

static const struct list_head* get_wakeup_reasons_nosync(void);

static void print_wakeup_sources(void)
{
	struct wakeup_irq_node *n;
	const struct list_head *wakeups;

	if (suspend_abort) {
		pr_info("Abort: %s", abort_reason);
		return;
	}

	wakeups = get_wakeup_reasons_nosync();
	list_for_each_entry(n, wakeups, next) {
		if (n->desc && n->desc->action && n->desc->action->name)
			pr_info("Resume caused by IRQ %d, %s\n", n->irq,
				n->desc->action->name);
		else
			pr_info("Resume caused by IRQ %d\n", n->irq);
	}
}

static bool walk_irq_node_tree(struct wakeup_irq_node *root,
		bool (*visit)(struct wakeup_irq_node *, void *),
		void *cookie)
{
	struct wakeup_irq_node *n, *t;

	if (!root)
		return true;

	list_for_each_entry_safe(n, t, &root->siblings, siblings) {
		if (!walk_irq_node_tree(n->child, visit, cookie))
			return false;
		if (!visit(n, cookie))
			return false;
	}

	if (!walk_irq_node_tree(root->child, visit, cookie))
		return false;
	return visit(root, cookie);
}

#ifdef CONFIG_DEDUCE_WAKEUP_REASONS
static bool is_node_handled(struct wakeup_irq_node *n, void *_p)
{
	return n->handled;
}

static bool base_irq_nodes_done(void)
{
	return walk_irq_node_tree(base_irq_nodes, is_node_handled, NULL);
}
#endif

struct buf_cookie {
	char *buf;
	int buf_offset;
};

static bool print_leaf_node(struct wakeup_irq_node *n, void *_p)
{
	struct buf_cookie *b = _p;
	if (!n->child) {
		if (n->desc && n->desc->action && n->desc->action->name)
			b->buf_offset +=
				snprintf(b->buf + b->buf_offset,
					PAGE_SIZE - b->buf_offset,
					"%d %s\n",
					n->irq, n->desc->action->name);
		else
			b->buf_offset +=
				snprintf(b->buf + b->buf_offset,
					PAGE_SIZE - b->buf_offset,
					"%d\n",
					n->irq);
	}
	return true;
}

static ssize_t last_resume_reason_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	unsigned long flags;

	struct buf_cookie b = {
		.buf = buf,
		.buf_offset = 0
	};

	spin_lock_irqsave(&resume_reason_lock, flags);
	if (suspend_abort)
		b.buf_offset = snprintf(buf, PAGE_SIZE, "Abort: %s", abort_reason);
	else
		walk_irq_node_tree(base_irq_nodes, print_leaf_node, &b);
	spin_unlock_irqrestore(&resume_reason_lock, flags);

	return b.buf_offset;
}

static ssize_t last_suspend_time_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	struct timespec sleep_time;
	struct timespec total_time;
	struct timespec suspend_resume_time;

	/*
	 * total_time is calculated from monotonic bootoffsets because
	 * unlike CLOCK_MONOTONIC it include the time spent in suspend state.
	 */
	total_time = ktime_to_timespec(ktime_sub(curr_stime, last_stime));

	/*
	 * suspend_resume_time is calculated as monotonic (CLOCK_MONOTONIC)
	 * time interval before entering suspend and post suspend.
	 */
	suspend_resume_time = ktime_to_timespec(ktime_sub(curr_monotime, last_monotime));

	/* sleep_time = total_time - suspend_resume_time */
	sleep_time = timespec_sub(total_time, suspend_resume_time);

	/* Export suspend_resume_time and sleep_time in pair here. */
	return sprintf(buf, "%lu.%09lu %lu.%09lu\n",
				suspend_resume_time.tv_sec, suspend_resume_time.tv_nsec,
				sleep_time.tv_sec, sleep_time.tv_nsec);
}

static struct kobj_attribute resume_reason = __ATTR_RO(last_resume_reason);
static struct kobj_attribute suspend_time = __ATTR_RO(last_suspend_time);

static struct attribute *attrs[] = {
	&resume_reason.attr,
	&suspend_time.attr,
	NULL,
};
static struct attribute_group attr_group = {
	.attrs = attrs,
};

static inline void stop_logging_wakeup_reasons(void)
{
	ACCESS_ONCE(log_wakeups) = false;
	smp_wmb();
}

/*
 * stores the immediate wakeup irqs; these often aren't the ones seen by
 * the drivers that registered them, due to chained interrupt controllers,
 * and multiple-interrupt dispatch.
 */
void log_base_wakeup_reason(int irq)
{
	/* No locking is needed, since this function is called within
	 * syscore_resume, with both nonboot CPUs and interrupts disabled.
	 */
	base_irq_nodes = add_to_siblings(base_irq_nodes, irq);
	BUG_ON(!base_irq_nodes);
#ifndef CONFIG_DEDUCE_WAKEUP_REASONS
	base_irq_nodes->handled = true;
#endif
}

#ifdef CONFIG_DEDUCE_WAKEUP_REASONS

/* This function is called by generic_handle_irq, which may call itself
 * recursively.  This happens with interrupts disabled.  Using
 * log_possible_wakeup_reason, we build a tree of interrupts, tracing the call
 * stack of generic_handle_irq, for each wakeup source containing the
 * interrupts actually handled.
 *
 * Most of these "trees" would either have a single node (in the event that the
 * wakeup source is the final interrupt), or consist of a list of two
 * interrupts, with the wakeup source at the root, and the final dispatched
 * interrupt at the leaf.
 *
 * When *all* wakeup sources have been thusly spoken for, this function will
 * clear the log_wakeups flag, and print the wakeup reasons.

   TODO: percpu

 */

static struct wakeup_irq_node *
log_possible_wakeup_reason_start(int irq, struct irq_desc *desc, unsigned depth)
{
	BUG_ON(!irqs_disabled());
	BUG_ON((signed)depth < 0);

	/* This function can race with a call to stop_logging_wakeup_reasons()
	 * from a thread context.  If this happens, just exit silently, as we are no
	 * longer interested in logging interrupts.
	 */
	if (!logging_wakeup_reasons())
		return NULL;

	/* If suspend was aborted, the base IRQ nodes are missing, and we stop
	 * logging interrupts immediately.
	 */
	if (!base_irq_nodes) {
		stop_logging_wakeup_reasons();
		return NULL;
	}

	/* We assume wakeup interrupts are handlerd only by the first core. */
	/* TODO: relax this by having percpu versions of the irq tree */
	if (smp_processor_id() != 0) {
		return NULL;
	}

	if (depth == 0) {
		cur_irq_tree_depth = 0;
		cur_irq_tree = search_siblings(base_irq_nodes, irq);
	}
	else if (cur_irq_tree) {
		if (depth > cur_irq_tree_depth) {
			BUG_ON(depth - cur_irq_tree_depth > 1);
			cur_irq_tree = add_child(cur_irq_tree, irq);
			if (cur_irq_tree)
				cur_irq_tree_depth++;
		}
		else {
			cur_irq_tree = get_base_node(cur_irq_tree,
					cur_irq_tree_depth - depth);
			cur_irq_tree_depth = depth;
			cur_irq_tree = add_to_siblings(cur_irq_tree, irq);
		}
	}

	return cur_irq_tree;
}

static void log_possible_wakeup_reason_complete(struct wakeup_irq_node *n,
					unsigned depth,
					bool handled)
{
	if (!n)
		return;
	n->handled = handled;
	if (depth == 0) {
		if (base_irq_nodes_done()) {
			stop_logging_wakeup_reasons();
			complete(&wakeups_completion);
			print_wakeup_sources();
		}
	}
}

bool log_possible_wakeup_reason(int irq,
			struct irq_desc *desc,
			bool (*handler)(unsigned int, struct irq_desc *))
{
	static DEFINE_PER_CPU(unsigned int, depth);

	struct wakeup_irq_node *n;
	bool handled;
	unsigned d;

	d = get_cpu_var(depth)++;
	put_cpu_var(depth);

	n = log_possible_wakeup_reason_start(irq, desc, d);

	handled = handler(irq, desc);

	d = --get_cpu_var(depth);
	put_cpu_var(depth);

	if (!handled && desc && desc->action)
		pr_debug("%s: irq %d action %pF not handled\n", __func__,
			irq, desc->action->handler);

	log_possible_wakeup_reason_complete(n, d, handled);

	return handled;
}

#endif /* CONFIG_DEDUCE_WAKEUP_REASONS */

void log_suspend_abort_reason(const char *fmt, ...)
{
	va_list args;

	spin_lock(&resume_reason_lock);

	//Suspend abort reason has already been logged.
	if (suspend_abort) {
		spin_unlock(&resume_reason_lock);
		return;
	}

	suspend_abort = true;
	va_start(args, fmt);
	vsnprintf(abort_reason, MAX_SUSPEND_ABORT_LEN, fmt, args);
	va_end(args);

	spin_unlock(&resume_reason_lock);
}

static bool match_node(struct wakeup_irq_node *n, void *_p)
{
	int irq = *((int *)_p);
	return n->irq != irq;
}

int check_wakeup_reason(int irq)
{
	bool found;
	spin_lock(&resume_reason_lock);
	found = !walk_irq_node_tree(base_irq_nodes, match_node, &irq);
	spin_unlock(&resume_reason_lock);
	return found;
}

static bool build_leaf_nodes(struct wakeup_irq_node *n, void *_p)
{
	struct list_head *wakeups = _p;
	if (!n->child)
		list_add(&n->next, wakeups);
	return true;
}

static const struct list_head* get_wakeup_reasons_nosync(void)
{
	BUG_ON(logging_wakeup_reasons());
	INIT_LIST_HEAD(&wakeup_irqs);
	walk_irq_node_tree(base_irq_nodes, build_leaf_nodes, &wakeup_irqs);
	return &wakeup_irqs;
}

static bool build_unfinished_nodes(struct wakeup_irq_node *n, void *_p)
{
	struct list_head *unfinished = _p;
	if (!n->handled) {
		pr_warning("%s: wakeup irq %d was not handled\n",
			   __func__, n->irq);
		list_add(&n->next, unfinished);
	}
	return true;
}

const struct list_head* get_wakeup_reasons(unsigned long timeout,
					struct list_head *unfinished)
{
	INIT_LIST_HEAD(unfinished);

	if (logging_wakeup_reasons()) {
		unsigned long signalled = 0;
		if (timeout)
			signalled = wait_for_completion_timeout(&wakeups_completion, timeout);
		if (WARN_ON(!signalled)) {
			stop_logging_wakeup_reasons();
			walk_irq_node_tree(base_irq_nodes, build_unfinished_nodes, unfinished);
			return NULL;
		}
		pr_info("%s: waited for %u ms\n",
				__func__,
				jiffies_to_msecs(timeout - signalled));
	}

	return get_wakeup_reasons_nosync();
}

static bool delete_node(struct wakeup_irq_node *n, void *unused)
{
	list_del(&n->siblings);
	kmem_cache_free(wakeup_irq_nodes_cache, n);
	return true;
}

void clear_wakeup_reasons(void)
{
	unsigned long flags;
	spin_lock_irqsave(&resume_reason_lock, flags);

	BUG_ON(logging_wakeup_reasons());
	walk_irq_node_tree(base_irq_nodes, delete_node, NULL);
	base_irq_nodes = NULL;
	cur_irq_tree = NULL;
	cur_irq_tree_depth = 0;
	INIT_LIST_HEAD(&wakeup_irqs);
	suspend_abort = false;

	spin_unlock_irqrestore(&resume_reason_lock, flags);
}

/* Detects a suspend and clears all the previous wake up reasons*/
static int wakeup_reason_pm_event(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		spin_lock(&resume_reason_lock);
		suspend_abort = false;
		spin_unlock(&resume_reason_lock);
		/* monotonic time since boot */
		last_monotime = ktime_get();
		/* monotonic time since boot including the time spent in suspend */
		last_stime = ktime_get_boottime();
		clear_wakeup_reasons();
		break;
	case PM_POST_SUSPEND:
		/* monotonic time since boot */
		curr_monotime = ktime_get();
		/* monotonic time since boot including the time spent in suspend */
		curr_stime = ktime_get_boottime();

#ifdef CONFIG_DEDUCE_WAKEUP_REASONS
		/* log_wakeups should have been cleared by now. */
		if (WARN_ON(logging_wakeup_reasons())) {
			stop_logging_wakeup_reasons();
			print_wakeup_sources();
		}
#else
		print_wakeup_sources();
#endif
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block wakeup_reason_pm_notifier_block = {
	.notifier_call = wakeup_reason_pm_event,
};

#if IS_ENABLED(CONFIG_DEBUG_FS) && IS_ENABLED(CONFIG_SUSPEND_TIME)
static int suspend_time_debug_show(struct seq_file *s, void *data)
{
	int bin;
	seq_printf(s, "time (secs)  count\n");
	seq_printf(s, "------------------\n");
	for (bin = 0; bin < 32; bin++) {
		if (time_in_suspend_bins[bin] == 0)
			continue;
		seq_printf(s, "%4d - %4d %4u\n",
			bin ? 1 << (bin - 1) : 0, 1 << bin,
				time_in_suspend_bins[bin]);
	}
	return 0;
}

static int suspend_time_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, suspend_time_debug_show, NULL);
}

static const struct file_operations suspend_time_debug_fops = {
	.open		= suspend_time_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init suspend_time_debug_init(void)
{
	struct dentry *d;

	d = debugfs_create_file("suspend_time", 0755, NULL, NULL,
		&suspend_time_debug_fops);
	if (!d) {
		pr_err("Failed to create suspend_time debug file\n");
		return -ENOMEM;
	}

	return 0;
}

late_initcall(suspend_time_debug_init);
#endif

/* Initializes the sysfs parameter
 * registers the pm_event notifier
 */
int __init wakeup_reason_init(void)
{
	spin_lock_init(&resume_reason_lock);

	if (register_pm_notifier(&wakeup_reason_pm_notifier_block)) {
		pr_warning("[%s] failed to register PM notifier\n",
			__func__);
		goto fail;
	}

	wakeup_reason = kobject_create_and_add("wakeup_reasons", kernel_kobj);
	if (!wakeup_reason) {
		pr_warning("[%s] failed to create a sysfs kobject\n",
				__func__);
		goto fail_unregister_pm_notifier;
	}

	if (sysfs_create_group(wakeup_reason, &attr_group)) {
		pr_warning("[%s] failed to create a sysfs group\n",
			__func__);
		goto fail_kobject_put;
	}

	wakeup_irq_nodes_cache =
		kmem_cache_create("wakeup_irq_node_cache",
					sizeof(struct wakeup_irq_node), 0,
					0, NULL);
	if (!wakeup_irq_nodes_cache)
		goto fail_remove_group;

	return 0;

fail_remove_group:
	sysfs_remove_group(wakeup_reason, &attr_group);
fail_kobject_put:
	kobject_put(wakeup_reason);
fail_unregister_pm_notifier:
	unregister_pm_notifier(&wakeup_reason_pm_notifier_block);
fail:
	return 1;
}

late_initcall(wakeup_reason_init);
