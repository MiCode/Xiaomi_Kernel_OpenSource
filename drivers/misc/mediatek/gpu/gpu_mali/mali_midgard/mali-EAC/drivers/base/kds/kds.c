/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */





#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/kds.h>

#include <asm/atomic.h>

#define KDS_LINK_TRIGGERED (1u << 0)
#define KDS_LINK_EXCLUSIVE (1u << 1)

#define KDS_INVALID (void *)-2
#define KDS_RESOURCE (void *)-1

struct kds_resource_set
{
	unsigned long         num_resources;
	unsigned long         pending;
	struct kds_callback  *cb;
	void                 *callback_parameter;
	void                 *callback_extra_parameter;
	struct list_head      callback_link;
	struct work_struct    callback_work;
	atomic_t              cb_queued;

	/* This is only initted when kds_waitall() is called. */
	wait_queue_head_t     wake;

	struct kds_link       resources[0];
};

static DEFINE_SPINLOCK(kds_lock);

int kds_callback_init(struct kds_callback *cb, int direct, kds_callback_fn user_cb)
{
	int ret = 0;

	cb->direct = direct;
	cb->user_cb = user_cb;

	if (!direct)
	{
		cb->wq = alloc_workqueue("kds", WQ_UNBOUND | WQ_HIGHPRI, WQ_UNBOUND_MAX_ACTIVE);
		if (!cb->wq)
			ret = -ENOMEM;
	}
	else
	{
		cb->wq = NULL;
	}

	return ret;
}
EXPORT_SYMBOL(kds_callback_init);

void kds_callback_term(struct kds_callback *cb)
{
	if (!cb->direct)
	{
		BUG_ON(!cb->wq);
		destroy_workqueue(cb->wq);
	}
	else
	{
		BUG_ON(cb->wq);
	}
}

EXPORT_SYMBOL(kds_callback_term);

static void kds_do_user_callback(struct kds_resource_set *rset)
{
	rset->cb->user_cb(rset->callback_parameter, rset->callback_extra_parameter);
}

static void kds_queued_callback(struct work_struct *work)
{
	struct kds_resource_set *rset;
	rset = container_of(work, struct kds_resource_set, callback_work);

	atomic_dec(&rset->cb_queued);

	kds_do_user_callback(rset);
}

static void kds_callback_perform(struct kds_resource_set *rset)
{
	if (rset->cb->direct)
		kds_do_user_callback(rset);
	else
	{
		int result;

		atomic_inc(&rset->cb_queued);

		result = queue_work(rset->cb->wq, &rset->callback_work);
		/* if we got a 0 return it means we've triggered the same rset twice! */
		WARN_ON(!result);
	}
}

void kds_resource_init(struct kds_resource * const res)
{
	BUG_ON(!res);
	INIT_LIST_HEAD(&res->waiters.link);
	res->waiters.parent = KDS_RESOURCE;
}
EXPORT_SYMBOL(kds_resource_init);

int kds_resource_term(struct kds_resource *res)
{
	unsigned long lflags;
	BUG_ON(!res);
	spin_lock_irqsave(&kds_lock, lflags);
	if (!list_empty(&res->waiters.link))
	{
		spin_unlock_irqrestore(&kds_lock, lflags);
		pr_err("ERROR: KDS resource is still in use\n");
		return -EBUSY;
	}
	res->waiters.parent = KDS_INVALID;
	spin_unlock_irqrestore(&kds_lock, lflags);
	return 0;
}
EXPORT_SYMBOL(kds_resource_term);

int kds_async_waitall(
		struct kds_resource_set ** const pprset,
		struct kds_callback      *cb,
		void                     *callback_parameter,
		void                     *callback_extra_parameter,
		int                       number_resources,
		unsigned long            *exclusive_access_bitmap,
		struct kds_resource     **resource_list)
{
	struct kds_resource_set *rset = NULL;
	unsigned long lflags;
	int i;
	int triggered;
	int err = -EFAULT;

	BUG_ON(!pprset);
	BUG_ON(!resource_list);
	BUG_ON(!cb);

	WARN_ONCE(number_resources > 10, "Waiting on a high numbers of resources may increase latency, see documentation.");

	rset = kmalloc(sizeof(*rset) + number_resources * sizeof(struct kds_link), GFP_KERNEL);
	if (!rset)
	{
		return -ENOMEM;
	}

	rset->num_resources = number_resources;
	rset->pending = number_resources;
	rset->cb = cb;
	rset->callback_parameter = callback_parameter;
	rset->callback_extra_parameter = callback_extra_parameter;
	INIT_LIST_HEAD(&rset->callback_link);
	INIT_WORK(&rset->callback_work, kds_queued_callback);
	atomic_set(&rset->cb_queued, 0);

	for (i = 0; i < number_resources; i++)
	{
		INIT_LIST_HEAD(&rset->resources[i].link);
		rset->resources[i].parent = rset;
	}

	spin_lock_irqsave(&kds_lock, lflags);

	for (i = 0; i < number_resources; i++)
	{
		unsigned long link_state = 0;

		if (test_bit(i, exclusive_access_bitmap))
		{
			link_state |= KDS_LINK_EXCLUSIVE;
		}

		/* no-one else waiting? */
		if (list_empty(&resource_list[i]->waiters.link))
		{
			link_state |= KDS_LINK_TRIGGERED;
			rset->pending--;
		}
		/* Adding a non-exclusive and the current tail is a triggered non-exclusive? */
		else if (((link_state & KDS_LINK_EXCLUSIVE) == 0) &&
				(((list_entry(resource_list[i]->waiters.link.prev, struct kds_link, link)->state & (KDS_LINK_EXCLUSIVE | KDS_LINK_TRIGGERED)) == KDS_LINK_TRIGGERED)))
		{
			link_state |= KDS_LINK_TRIGGERED;
			rset->pending--;
		}
		rset->resources[i].state = link_state;

		/* avoid double wait (hang) */
		if (!list_empty(&resource_list[i]->waiters.link))
		{
			/* adding same rset again? */
			if (list_entry(resource_list[i]->waiters.link.prev, struct kds_link, link)->parent == rset)
			{
				goto roll_back;
			}
		}
		list_add_tail(&rset->resources[i].link, &resource_list[i]->waiters.link);
	}

	triggered = (rset->pending == 0);

	/* set the pointer before the callback is called so it sees it */
	*pprset = rset;

	spin_unlock_irqrestore(&kds_lock, lflags);

	if (triggered)
	{
		/* all resources obtained, trigger callback */
		kds_callback_perform(rset);
	}

	return 0;

roll_back:
	/* roll back */
	while (i-- > 0)
	{
		list_del(&rset->resources[i].link);
	}
	err = -EINVAL;

	spin_unlock_irqrestore(&kds_lock, lflags);
	kfree(rset);
	return err;
}
EXPORT_SYMBOL(kds_async_waitall);

static void wake_up_sync_call(void *callback_parameter, void *callback_extra_parameter)
{
	wait_queue_head_t *wait = (wait_queue_head_t *)callback_parameter;
	wake_up(wait);
}

static struct kds_callback sync_cb =
{
	wake_up_sync_call,
	1,
	NULL,
};

struct kds_resource_set *kds_waitall(
		int                   number_resources,
		unsigned long        *exclusive_access_bitmap,
		struct kds_resource **resource_list,
		unsigned long         jiffies_timeout)
{
	struct kds_resource_set *rset;
	unsigned long lflags;
	int i;
	int triggered = 0;
	DECLARE_WAIT_QUEUE_HEAD_ONSTACK(wake);

	rset = kmalloc(sizeof(*rset) + number_resources * sizeof(struct kds_link), GFP_KERNEL);
	if (!rset)
		return rset;

	rset->num_resources = number_resources;
	rset->pending = number_resources;
	init_waitqueue_head(&rset->wake);
	INIT_LIST_HEAD(&rset->callback_link);
	INIT_WORK(&rset->callback_work, kds_queued_callback);
	atomic_set(&rset->cb_queued, 0);

	spin_lock_irqsave(&kds_lock, lflags);

	for (i = 0; i < number_resources; i++)
	{
		unsigned long link_state = 0;

		if (test_bit(i, exclusive_access_bitmap))
		{
			link_state |= KDS_LINK_EXCLUSIVE;
		}

		if (list_empty(&resource_list[i]->waiters.link))
		{
			link_state |= KDS_LINK_TRIGGERED;
			rset->pending--;
		}
		/* Adding a non-exclusive and the current tail is a triggered non-exclusive? */
		else if (((link_state & KDS_LINK_EXCLUSIVE) == 0) &&
				(((list_entry(resource_list[i]->waiters.link.prev, struct kds_link, link)->state & (KDS_LINK_EXCLUSIVE | KDS_LINK_TRIGGERED)) == KDS_LINK_TRIGGERED)))
		{
			link_state |= KDS_LINK_TRIGGERED;
			rset->pending--;
		}

		INIT_LIST_HEAD(&rset->resources[i].link);
		rset->resources[i].parent = rset;
		rset->resources[i].state = link_state;

		/* avoid double wait (hang) */
		if (!list_empty(&resource_list[i]->waiters.link))
		{
			/* adding same rset again? */
			if (list_entry(resource_list[i]->waiters.link.prev, struct kds_link, link)->parent == rset)
			{
				goto roll_back;
			}
		}

		list_add_tail(&rset->resources[i].link, &resource_list[i]->waiters.link);
	}

	if (rset->pending == 0)
		triggered = 1;
	else
	{
		rset->cb = &sync_cb;
		rset->callback_parameter = &rset->wake;
		rset->callback_extra_parameter = NULL;
	}

	spin_unlock_irqrestore(&kds_lock, lflags);

	if (!triggered)
	{
		long wait_res = 0;
		long timeout = (jiffies_timeout == KDS_WAIT_BLOCKING) ?
				MAX_SCHEDULE_TIMEOUT : jiffies_timeout;

		if (timeout)
		{
			wait_res = wait_event_interruptible_timeout(rset->wake,
					rset->pending == 0, timeout);
		}

		if ((wait_res == -ERESTARTSYS) || (wait_res == 0))
		{
			/* use \a kds_resource_set_release to roll back */
			kds_resource_set_release(&rset);
			return ERR_PTR(wait_res);
		}
	}
	return rset;

roll_back:
	/* roll back */
	while (i-- > 0)
	{
		list_del(&rset->resources[i].link);
	}

	spin_unlock_irqrestore(&kds_lock, lflags);
	kfree(rset);
	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL(kds_waitall);

static void __kds_resource_set_release_common(struct kds_resource_set *rset)
{
	struct list_head triggered = LIST_HEAD_INIT(triggered);
	struct kds_resource_set *it;
	unsigned long lflags;
	int i;

	spin_lock_irqsave(&kds_lock, lflags);

	for (i = 0; i < rset->num_resources; i++)
	{
		struct kds_resource *resource;
		struct kds_link *it = NULL;

		/* fetch the previous entry on the linked list */
		it = list_entry(rset->resources[i].link.prev, struct kds_link, link);
		/* unlink ourself */
		list_del(&rset->resources[i].link);

		/* any waiters? */
		if (list_empty(&it->link))
			continue;

		/* were we the head of the list? (head if prev is a resource) */
		if (it->parent != KDS_RESOURCE)
		{
			if ((it->state & KDS_LINK_TRIGGERED) && !(it->state & KDS_LINK_EXCLUSIVE))
			{
				/*
				 * previous was triggered and not exclusive, so we
				 * trigger non-exclusive until end-of-list or first
				 * exclusive
				 */

				struct kds_link *it_waiting = it;

				list_for_each_entry(it, &it_waiting->link, link)
				{
					/* exclusive found, stop triggering */
					if (it->state & KDS_LINK_EXCLUSIVE)
						break;

					it->state |= KDS_LINK_TRIGGERED;
					/* a parent to update? */
					if (it->parent != KDS_RESOURCE)
					{
						if (0 == --it->parent->pending)
						{
							/* new owner now triggered, track for callback later */
							list_add(&it->parent->callback_link, &triggered);
						}
					}
				}
			}
			continue;
		}

		/* we were the head, find the kds_resource */
		resource = container_of(it, struct kds_resource, waiters);

		/* we know there is someone waiting from the any-waiters test above */

		/* find the head of the waiting list */
		it = list_first_entry(&resource->waiters.link, struct kds_link, link);

		/* new exclusive owner? */
		if (it->state & KDS_LINK_EXCLUSIVE)
		{
			/* link now triggered */
			it->state |= KDS_LINK_TRIGGERED;
			/* a parent to update? */
			if (0 == --it->parent->pending)
			{
				/* new owner now triggered, track for callback later */
				list_add(&it->parent->callback_link, &triggered);
			}
		}
		/* exclusive releasing ? */
		else if (rset->resources[i].state & KDS_LINK_EXCLUSIVE)
		{
			/* trigger non-exclusive until end-of-list or first exclusive */
			list_for_each_entry(it, &resource->waiters.link, link)
			{
				/* exclusive found, stop triggering */
				if (it->state & KDS_LINK_EXCLUSIVE)
					break;

				it->state |= KDS_LINK_TRIGGERED;
				/* a parent to update? */
				if (0 == --it->parent->pending)
				{
					/* new owner now triggered, track for callback later */
					list_add(&it->parent->callback_link, &triggered);
				}
			}
		}
	}

	spin_unlock_irqrestore(&kds_lock, lflags);

	while (!list_empty(&triggered))
	{
		it = list_first_entry(&triggered, struct kds_resource_set, callback_link);
		list_del(&it->callback_link);
		kds_callback_perform(it);
	}
}

void kds_resource_set_release(struct kds_resource_set **pprset)
{
	struct kds_resource_set *rset;
	int queued;

	rset = cmpxchg(pprset, *pprset, NULL);

	if (!rset)
	{
		/* caught a race between a cancelation
		 * and a completion, nothing to do */
		return;
	}

	__kds_resource_set_release_common(rset);

	/*
	 * Caller is responsible for guaranteeing that callback work is not
	 * pending (i.e. its running or completed) prior to calling release.
	 */
	queued = atomic_read(&rset->cb_queued);
	BUG_ON(queued);

	/* free the resource set */
	kfree(rset);
}
EXPORT_SYMBOL(kds_resource_set_release);

void kds_resource_set_release_sync(struct kds_resource_set **pprset)
{
	struct kds_resource_set *rset;

	rset = cmpxchg(pprset, *pprset, NULL);
	if (!rset)
	{
		/* caught a race between a cancelation
		 * and a completion, nothing to do */
		return;
	}

	__kds_resource_set_release_common(rset);

	/*
	 * In the case of a kds async wait cancellation ensure the deferred
	 * call back does not get scheduled if a trigger fired at the same time
	 * to release the wait.
	 */
	cancel_work_sync(&rset->callback_work);

	/* free the resource set */
	kfree(rset);
}
EXPORT_SYMBOL(kds_resource_set_release_sync);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ARM Ltd.");
MODULE_VERSION("1.0");
