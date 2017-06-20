/*
 * include/linux/wakeup_reason.h
 *
 * Logs the reason which caused the kernel to resume
 * from the suspend mode.
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

#ifndef _LINUX_WAKEUP_REASON_H
#define _LINUX_WAKEUP_REASON_H

#include <linux/types.h>
#include <linux/completion.h>

#define MAX_SUSPEND_ABORT_LEN 256

struct wakeup_irq_node {
	/* @leaf is a linked list of all leaf nodes in the interrupts trees.
	 */
	struct list_head next;
	/* @irq: IRQ number of this node.
	 */
	int irq;
	struct irq_desc *desc;

	/* @siblings contains the list of irq nodes at the same depth; at a
	 * depth of zero, this is the list of base wakeup interrupts.
	 */
	struct list_head siblings;
	/* @parent: only one node in a siblings list has a pointer to the
	 * parent; that node is the head of the list of siblings.
	 */
	struct wakeup_irq_node *parent;
	/* @child: any node can have one child
	 */
	struct wakeup_irq_node *child;
	/* @handled: this flag is set to true when the interrupt handler (one of
	 * handle_.*_irq in kernel/irq/handle.c) for this node gets called; it is set
	 * to false otherwise.  We use this flag to determine whether a subtree rooted
	 * at a node has been handled.  When all trees rooted at
	 * base-wakeup-interrupt nodes have been handled, we stop logging
	 * potential wakeup interrupts, and construct the list of actual
	 * wakeups from the leaves of these trees.
	 */
	bool handled;
};

#ifdef CONFIG_DEDUCE_WAKEUP_REASONS

/* Called in the resume path, with interrupts and nonboot cpus disabled; on
 * need for a spinlock.
 */
static inline void start_logging_wakeup_reasons(void)
{
	extern bool log_wakeups;
	extern struct completion wakeups_completion;
	ACCESS_ONCE(log_wakeups) = true;
	init_completion(&wakeups_completion);
}

static inline bool logging_wakeup_reasons_nosync(void)
{
	extern bool log_wakeups;
	return ACCESS_ONCE(log_wakeups);
}

static inline bool logging_wakeup_reasons(void)
{
	smp_rmb();
	return logging_wakeup_reasons_nosync();
}

bool log_possible_wakeup_reason(int irq,
			struct irq_desc *desc,
			bool (*handler)(unsigned int, struct irq_desc *));

#else

static inline void start_logging_wakeup_reasons(void) {}
static inline bool logging_wakeup_reasons_nosync(void) { return false; }
static inline bool logging_wakeup_reasons(void) { return false; }
static inline bool log_possible_wakeup_reason(int irq,
			struct irq_desc *desc,
			bool (*handler)(unsigned int, struct irq_desc *)) { return true; }

#endif

const struct list_head*
get_wakeup_reasons(unsigned long timeout, struct list_head *unfinished);
void log_base_wakeup_reason(int irq);
void clear_wakeup_reasons(void);
void log_wakeup_reason(int irq);
int check_wakeup_reason(int irq);

#ifdef CONFIG_SUSPEND
void log_suspend_abort_reason(const char *fmt, ...);
#else
static inline void log_suspend_abort_reason(const char *fmt, ...) { }
#endif

#endif /* _LINUX_WAKEUP_REASON_H */
