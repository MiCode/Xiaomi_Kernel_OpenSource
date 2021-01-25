// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kthread.h>
#include <linux/notifier.h>
#include <linux/shmem_fs.h>

#include "kgsl_reclaim.h"
#include "kgsl_sharedmem.h"

static struct notifier_block kgsl_reclaim_nb;
static bool kgsl_reclaim;

/*
 * Reclaiming excessive number of pages from a process will impact launch
 * latency for the subsequent launch of the process. After measuring the
 * launch latencies by having various maximum limits, it has been decided
 * that allowing 30MB (7680 pages) of relcaim per process will have little
 * impact and the latency will be within acceptable limit.
 */
static u32 kgsl_reclaim_max_page_limit = 7680;

static int kgsl_memdesc_get_reclaimed_pages(struct kgsl_mem_entry *entry)
{
	struct kgsl_memdesc *memdesc = &entry->memdesc;
	int i, ret;
	struct page *page;

	for (i = 0; i < memdesc->page_count; i++) {
		if (memdesc->pages[i])
			continue;

		page = shmem_read_mapping_page_gfp(
			memdesc->shmem_filp->f_mapping, i, kgsl_gfp_mask(0));

		if (IS_ERR(page))
			return PTR_ERR(page);

		kgsl_flush_page(page);

		/*
		 * Update the pages array only if vmfault has not
		 * updated it meanwhile
		 */
		spin_lock(&memdesc->lock);
		if (!memdesc->pages[i]) {
			memdesc->pages[i] = page;
			memdesc->reclaimed_page_count--;
			atomic_dec(&entry->priv->reclaimed_page_count);
		} else
			put_page(page);
		spin_unlock(&memdesc->lock);
	}

	ret = kgsl_mmu_map(memdesc->pagetable, memdesc);
	if (ret)
		return ret;

	memdesc->priv &= ~KGSL_MEMDESC_RECLAIMED;

	return 0;
}

int kgsl_reclaim_to_pinned_state(
		struct kgsl_process_private *process)
{
	struct kgsl_mem_entry *entry;
	int next = 0, valid_entry, ret = 0;

	if (!kgsl_reclaim)
		return 0;

	mutex_lock(&process->reclaim_lock);

	if (test_bit(KGSL_PROC_PINNED_STATE, &process->state))
		goto done;

	for ( ; ; ) {
		valid_entry = 0;
		spin_lock(&process->mem_lock);
		entry = idr_get_next(&process->mem_idr, &next);
		if (entry == NULL) {
			spin_unlock(&process->mem_lock);
			break;
		}

		if (!entry->pending_free &&
				(entry->memdesc.priv & KGSL_MEMDESC_RECLAIMED))
			valid_entry = kgsl_mem_entry_get(entry);
		spin_unlock(&process->mem_lock);

		if (valid_entry) {
			ret = kgsl_memdesc_get_reclaimed_pages(entry);
			kgsl_mem_entry_put(entry);
			if (ret)
				goto done;
		}

		next++;
	}

	set_bit(KGSL_PROC_PINNED_STATE, &process->state);
done:
	mutex_unlock(&process->reclaim_lock);
	return ret;
}

static void kgsl_reclaim_foreground_work(struct work_struct *work)
{
	struct kgsl_process_private *process =
		container_of(work, struct kgsl_process_private, fg_work);

	if (test_bit(KGSL_PROC_STATE, &process->state))
		kgsl_reclaim_to_pinned_state(process);
	kgsl_process_private_put(process);
}

static ssize_t kgsl_proc_state_show(struct kobject *kobj,
		struct kgsl_process_attribute *attr, char *buf)
{
	struct kgsl_process_private *process =
		container_of(kobj, struct kgsl_process_private, kobj);

	if (test_bit(KGSL_PROC_STATE, &process->state))
		return scnprintf(buf, PAGE_SIZE, "foreground\n");
	else
		return scnprintf(buf, PAGE_SIZE, "background\n");
}

static ssize_t kgsl_proc_state_store(struct kobject *kobj,
	struct kgsl_process_attribute *attr, const char *buf, ssize_t count)
{
	struct kgsl_process_private *process =
		container_of(kobj, struct kgsl_process_private, kobj);

	if (sysfs_streq(buf, "foreground")) {
		set_bit(KGSL_PROC_STATE, &process->state);
		if (kgsl_process_private_get(process))
			kgsl_schedule_work(&process->fg_work);
	} else if (sysfs_streq(buf, "background"))
		clear_bit(KGSL_PROC_STATE, &process->state);
	else
		return -EINVAL;

	return count;
}

static ssize_t gpumem_reclaimed_show(struct kobject *kobj,
		struct kgsl_process_attribute *attr, char *buf)
{
	struct kgsl_process_private *process =
		container_of(kobj, struct kgsl_process_private, kobj);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		atomic_read(&process->reclaimed_page_count) << PAGE_SHIFT);
}

PROCESS_ATTR(state, 0644, kgsl_proc_state_show, kgsl_proc_state_store);
PROCESS_ATTR(gpumem_reclaimed, 0444, gpumem_reclaimed_show, NULL);

static const struct attribute *proc_reclaim_attrs[] = {
	&attr_state.attr,
	&attr_gpumem_reclaimed.attr,
	NULL,
};

void kgsl_reclaim_proc_sysfs_init(struct kgsl_process_private *process)
{
	if (kgsl_reclaim)
		WARN_ON(sysfs_create_files(&process->kobj, proc_reclaim_attrs));
}

ssize_t kgsl_proc_max_reclaim_limit_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;

	if (!kgsl_reclaim)
		return -EINVAL;

	ret = kstrtou32(buf, 0, &kgsl_reclaim_max_page_limit);
	return ret ? ret : count;
}

ssize_t kgsl_proc_max_reclaim_limit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if (!kgsl_reclaim)
		return 0;

	return scnprintf(buf, PAGE_SIZE, "%d\n", kgsl_reclaim_max_page_limit);
}

static int kgsl_reclaim_callback(struct notifier_block *nb,
		unsigned long pid, void *data)
{
	struct kgsl_process_private *p, *process = NULL;
	struct kgsl_mem_entry *entry;
	struct kgsl_memdesc *memdesc;
	int valid_entry, next = 0, ret;

	spin_lock(&kgsl_driver.proclist_lock);
	list_for_each_entry(p, &kgsl_driver.process_list, list) {
		if ((unsigned long)p->pid == pid) {
			if (kgsl_process_private_get(p))
				process = p;
			break;
		}
	}
	spin_unlock(&kgsl_driver.proclist_lock);

	if (!process)
		return NOTIFY_OK;

	/*
	 * If we do not get the lock here, it means that the buffers are
	 * being pinned back. So do not keep waiting here as we would anyway
	 * return empty handed once the lock is acquired.
	 */
	if (!mutex_trylock(&process->reclaim_lock))
		goto done;

	for ( ; ; ) {

		if (atomic_read(&process->reclaimed_page_count) >=
				kgsl_reclaim_max_page_limit)
			break;

		/* Abort reclaim if process submitted work. */
		if (atomic_read(&process->cmd_count))
			break;

		/* Abort reclaim if process foreground hint is received. */
		if (test_bit(KGSL_PROC_STATE, &process->state))
			break;

		valid_entry = 0;
		spin_lock(&process->mem_lock);
		entry = idr_get_next(&process->mem_idr, &next);
		if (entry == NULL) {
			spin_unlock(&process->mem_lock);
			break;
		}

		memdesc = &entry->memdesc;
		if (!entry->pending_free &&
				(memdesc->priv & KGSL_MEMDESC_CAN_RECLAIM) &&
				!(memdesc->priv & KGSL_MEMDESC_RECLAIMED) &&
				!(memdesc->priv & KGSL_MEMDESC_SKIP_RECLAIM))
			valid_entry = kgsl_mem_entry_get(entry);
		spin_unlock(&process->mem_lock);

		if (!valid_entry) {
			next++;
			continue;
		}

		if ((atomic_read(&process->reclaimed_page_count) +
			memdesc->page_count) > kgsl_reclaim_max_page_limit) {
			kgsl_mem_entry_put(entry);
			next++;
			continue;
		}

		if (!kgsl_mmu_unmap(memdesc->pagetable, memdesc)) {
			int i;

			for (i = 0; i < memdesc->page_count; i++) {
				set_page_dirty_lock(memdesc->pages[i]);
				spin_lock(&memdesc->lock);
				put_page(memdesc->pages[i]);
				memdesc->pages[i] = NULL;
				spin_unlock(&memdesc->lock);
			}

			memdesc->priv |= KGSL_MEMDESC_RECLAIMED;

			ret = reclaim_address_space
				(memdesc->shmem_filp->f_mapping, data);

			memdesc->reclaimed_page_count += memdesc->page_count;
			atomic_add(memdesc->page_count,
					&process->reclaimed_page_count);
		}

		kgsl_mem_entry_put(entry);

		if (ret == NOTIFY_DONE)
			break;

		next++;
	}
	if (next)
		clear_bit(KGSL_PROC_PINNED_STATE, &process->state);
	mutex_unlock(&process->reclaim_lock);
done:
	kgsl_process_private_put(process);
	return NOTIFY_OK;
}

void kgsl_reclaim_proc_private_init(struct kgsl_process_private *process)
{
	if (!kgsl_reclaim)
		return;

	mutex_init(&process->reclaim_lock);
	INIT_WORK(&process->fg_work, kgsl_reclaim_foreground_work);
	set_bit(KGSL_PROC_PINNED_STATE, &process->state);
	set_bit(KGSL_PROC_STATE, &process->state);
}

int kgsl_reclaim_init(struct kgsl_device *device)
{
	if (!(device->flags & KGSL_FLAG_PROCESS_RECLAIM))
		return 0;

	kgsl_reclaim = true;

	kgsl_reclaim_nb.notifier_call = kgsl_reclaim_callback;
	return proc_reclaim_notifier_register(&kgsl_reclaim_nb);
}

void kgsl_reclaim_close(void)
{
	proc_reclaim_notifier_unregister(&kgsl_reclaim_nb);
}
