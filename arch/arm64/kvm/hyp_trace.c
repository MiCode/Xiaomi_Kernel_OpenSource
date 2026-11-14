// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Google LLC
 * Author: Vincent Donnefort <vdonnefort@google.com>
 */

#include <linux/arm-smccc.h>
#include <linux/percpu-defs.h>
#include <linux/panic_notifier.h>
#include <linux/trace_events.h>
#include <linux/tracefs.h>

#include <asm/kvm_host.h>
#include <asm/kvm_hyptrace.h>
#include <asm/kvm_hypevents_defs.h>
#include <asm/kvm_pkvm.h>

#include "hyp_constants.h"
#include "hyp_trace.h"

#define RB_POLL_MS 100

/* Same 10min used by clocksource when width is more than 32-bits */
#define CLOCK_MAX_CONVERSION_S 600
#define CLOCK_INIT_MS 100
#define CLOCK_POLL_MS 500

#define TRACEFS_DIR "hypervisor"
#define TRACEFS_MODE_WRITE 0640
#define TRACEFS_MODE_READ 0440

struct hyp_trace_clock {
	u64			cycles;
	u64			max_delta;
	u64			boot;
	u32			mult;
	u32			shift;
	struct delayed_work	work;
	struct completion	ready;
};

static struct hyp_trace_buffer {
	struct hyp_trace_desc		*desc;
	struct ring_buffer_writer	writer;
	struct trace_buffer		*trace_buffer;
	size_t				desc_size;
	bool				tracing_on;
	int				nr_readers;
	struct mutex			lock;
	struct hyp_trace_clock		clock;
	struct ht_iterator		*printk_iter;
	bool				printk_on;
} hyp_trace_buffer = {
	.lock		= __MUTEX_INITIALIZER(hyp_trace_buffer.lock),
};

static size_t hyp_trace_buffer_size = 7 << 10;

static bool hyp_trace_panic __read_mostly;

/* Number of pages the ring-buffer requires to accommodate for size */
#define NR_PAGES(size) \
	((PAGE_ALIGN(size) >> PAGE_SHIFT) + 1)

static inline bool hyp_trace_buffer_loaded(struct hyp_trace_buffer *hyp_buffer)
{
	return !!hyp_buffer->trace_buffer;
}

static inline bool hyp_trace_buffer_used(struct hyp_trace_buffer *hyp_buffer)
{
	return hyp_buffer->nr_readers || hyp_buffer->tracing_on ||
		!ring_buffer_empty(hyp_buffer->trace_buffer);
}

static int set_ht_printk_on(char *str)
{
	if ((strcmp(str, "=0") != 0 && strcmp(str, "=off") != 0))
		hyp_trace_buffer.printk_on = true;

	return 1;
}
__setup("hyp_trace_printk", set_ht_printk_on);

static void __hyp_clock_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct hyp_trace_buffer *hyp_buffer;
	struct hyp_trace_clock *hyp_clock;
	struct system_time_snapshot snap;
	u64 rate, delta_cycles;
	u64 boot, delta_boot;
	u64 err = 0;

	hyp_clock = container_of(dwork, struct hyp_trace_clock, work);
	hyp_buffer = container_of(hyp_clock, struct hyp_trace_buffer, clock);

	ktime_get_snapshot(&snap);
	boot = ktime_to_ns(snap.boot);

	delta_boot = boot - hyp_clock->boot;
	delta_cycles = snap.cycles - hyp_clock->cycles;

	/* Compare hyp clock with the kernel boot clock */
	if (hyp_clock->mult) {
		u64 cur = delta_cycles;

		cur *= hyp_clock->mult;
		cur >>= hyp_clock->shift;
		cur += hyp_clock->boot;

		err = abs_diff(cur, boot);

		/* No deviation, only update epoch if necessary */
		if (!err) {
			if (delta_cycles >= hyp_clock->max_delta)
				goto update_hyp;

			goto resched;
		}

		/* Warn if the error is above tracing precision (1us) */
		if (hyp_buffer->tracing_on && err > NSEC_PER_USEC)
			pr_warn_ratelimited("hyp trace clock off by %lluus\n",
					    err / NSEC_PER_USEC);
	}

	rate = div64_u64(delta_cycles * NSEC_PER_SEC, delta_boot);
	clocks_calc_mult_shift(&hyp_clock->mult, &hyp_clock->shift,
			       rate, NSEC_PER_SEC, CLOCK_MAX_CONVERSION_S);

update_hyp:
	hyp_clock->max_delta = (U64_MAX / hyp_clock->mult) >> 1;
	hyp_clock->cycles = snap.cycles;
	hyp_clock->boot = boot;
	kvm_call_hyp_nvhe(__pkvm_update_clock_tracing, hyp_clock->mult,
			  hyp_clock->shift, hyp_clock->boot, hyp_clock->cycles);
	complete(&hyp_clock->ready);

	pr_debug("hyp trace clock update mult=%u shift=%u max_delta=%llu err=%llu\n",
		 hyp_clock->mult, hyp_clock->shift, hyp_clock->max_delta, err);

resched:
	schedule_delayed_work(&hyp_clock->work,
			      msecs_to_jiffies(CLOCK_POLL_MS));
}

static void hyp_clock_start(struct hyp_trace_buffer *hyp_buffer)
{
	struct hyp_trace_clock *hyp_clock = &hyp_buffer->clock;
	struct system_time_snapshot snap;

	ktime_get_snapshot(&snap);

	hyp_clock->boot = ktime_to_ns(snap.boot);
	hyp_clock->cycles = snap.cycles;
	hyp_clock->mult = 0;

	init_completion(&hyp_clock->ready);
	INIT_DELAYED_WORK(&hyp_clock->work, __hyp_clock_work);
	schedule_delayed_work(&hyp_clock->work, msecs_to_jiffies(CLOCK_INIT_MS));
}

static void hyp_clock_stop(struct hyp_trace_buffer *hyp_buffer)
{
	struct hyp_trace_clock *hyp_clock = &hyp_buffer->clock;

	cancel_delayed_work_sync(&hyp_clock->work);
}

static void hyp_clock_wait(struct hyp_trace_buffer *hyp_buffer)
{
	struct hyp_trace_clock *hyp_clock = &hyp_buffer->clock;

	wait_for_completion(&hyp_clock->ready);
}

static int __get_reader_page(int cpu)
{
	/* we'd better no try to call the hyp if it has panic'ed */
	if (hyp_trace_panic)
		return 0;

	return kvm_call_hyp_nvhe(__pkvm_swap_reader_tracing, cpu);
}

static int __reset(int cpu)
{
	return kvm_call_hyp_nvhe(__pkvm_reset_tracing, cpu);
}

static void hyp_trace_free_pages(struct hyp_trace_desc *desc)
{
	struct rb_page_desc *rb_desc;
	int cpu, id;

	for_each_rb_page_desc(rb_desc, cpu, &desc->page_desc) {
		free_page(rb_desc->meta_va);
		for (id = 0; id < rb_desc->nr_page_va; id++)
			free_page(rb_desc->page_va[id]);
	}
}

static int hyp_trace_alloc_pages(struct hyp_trace_desc *desc, size_t size)
{
	int err = 0, cpu, id, nr_pages = NR_PAGES(size);
	struct trace_page_desc *trace_desc;
	struct rb_page_desc *rb_desc;

	trace_desc = &desc->page_desc;
	trace_desc->nr_cpus = 0;

	rb_desc = (struct rb_page_desc *)&trace_desc->__data[0];

	for_each_possible_cpu(cpu) {
		rb_desc->cpu = cpu;
		rb_desc->nr_page_va = 0;
		rb_desc->meta_va = (unsigned long)page_to_virt(alloc_page(GFP_KERNEL));
		if (!rb_desc->meta_va) {
			err = -ENOMEM;
			break;
		}
		for (id = 0; id < nr_pages; id++) {
			rb_desc->page_va[id] = (unsigned long)page_to_virt(alloc_page(GFP_KERNEL));
			if (!rb_desc->page_va[id]) {
				err = -ENOMEM;
				break;
			}
			rb_desc->nr_page_va++;
		}
		trace_desc->nr_cpus++;
		rb_desc = __next_rb_page_desc(rb_desc);
	}

	if (err) {
		hyp_trace_free_pages(desc);
		return err;
	}

	return 0;
}

static int __load_page(unsigned long va)
{
	return kvm_call_hyp_nvhe(__pkvm_host_share_hyp, virt_to_pfn((void *)va), 1);
}

static void __teardown_page(unsigned long va)
{
	WARN_ON(kvm_call_hyp_nvhe(__pkvm_host_unshare_hyp, virt_to_pfn((void *)va), 1));
}

static void hyp_trace_teardown_pages(struct hyp_trace_desc *desc,
				     int last_cpu)
{
	struct rb_page_desc *rb_desc;
	int cpu, id;

	for_each_rb_page_desc(rb_desc, cpu, &desc->page_desc) {
		if (cpu > last_cpu)
			break;
		__teardown_page(rb_desc->meta_va);
		for (id = 0; id < rb_desc->nr_page_va; id++)
			__teardown_page(rb_desc->page_va[id]);
	}
}

static int hyp_trace_load_pages(struct hyp_trace_desc *desc)
{
	int last_loaded_cpu = 0, cpu, id, err = -EINVAL;
	struct rb_page_desc *rb_desc;

	for_each_rb_page_desc(rb_desc, cpu, &desc->page_desc) {
		err = __load_page(rb_desc->meta_va);
		if (err)
			break;

		for (id = 0; id < rb_desc->nr_page_va; id++) {
			err = __load_page(rb_desc->page_va[id]);
			if (err)
				break;
		}

		if (!err)
			continue;

		for (id--; id >= 0; id--)
			__teardown_page(rb_desc->page_va[id]);

		last_loaded_cpu = cpu - 1;

		break;
	}

	if (!err)
		return 0;

	hyp_trace_teardown_pages(desc, last_loaded_cpu);

	return err;
}

static int hyp_trace_buffer_load(struct hyp_trace_buffer *hyp_buffer, size_t size)
{
	int ret, nr_pages = NR_PAGES(size);
	struct rb_page_desc *rbdesc;
	struct hyp_trace_desc *desc;
	size_t desc_size;

	if (hyp_trace_buffer_loaded(hyp_buffer))
		return 0;

	desc_size = size_add(offsetof(struct hyp_trace_desc, page_desc),
			     offsetof(struct trace_page_desc, __data));
	desc_size = size_add(desc_size,
			     size_mul(num_possible_cpus(),
				      struct_size(rbdesc, page_va, nr_pages)));
	if (desc_size == SIZE_MAX)
		return -E2BIG;

	/*
	 * The hypervisor will unmap the descriptor from the host to protect the
	 * reading. Page granularity for the allocation ensures no other
	 * useful data will be unmapped.
	 */
	desc_size = PAGE_ALIGN(desc_size);

	desc = (struct hyp_trace_desc *)alloc_pages_exact(desc_size, GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	ret = hyp_trace_alloc_pages(desc, size);
	if (ret)
		goto err_free_desc;

	ret = hyp_trace_load_pages(desc);
	if (ret)
		goto err_free_pages;

	ret = kvm_call_refill_hyp_nvhe(__pkvm_load_tracing, (unsigned long)desc,
				       desc_size);
	if (ret)
		goto err_teardown_pages;

	hyp_buffer->writer.pdesc = &desc->page_desc;
	hyp_buffer->writer.get_reader_page = __get_reader_page;
	hyp_buffer->writer.reset = __reset;
	hyp_buffer->trace_buffer = ring_buffer_reader(&hyp_buffer->writer);
	if (!hyp_buffer->trace_buffer) {
		ret = -ENOMEM;
		goto err_teardown_tracing;
	}

	hyp_buffer->desc = desc;
	hyp_buffer->desc_size = desc_size;

	return 0;

err_teardown_tracing:
	kvm_call_hyp_nvhe(__pkvm_teardown_tracing);
err_teardown_pages:
	hyp_trace_teardown_pages(desc, INT_MAX);
err_free_pages:
	hyp_trace_free_pages(desc);
err_free_desc:
	free_pages_exact(desc, desc_size);

	return ret;
}

static void hyp_trace_buffer_teardown(struct hyp_trace_buffer *hyp_buffer)
{
	struct hyp_trace_desc *desc = hyp_buffer->desc;
	size_t desc_size = hyp_buffer->desc_size;

	if (!hyp_trace_buffer_loaded(hyp_buffer))
		return;

	if (hyp_trace_buffer_used(hyp_buffer))
		return;

	if (kvm_call_hyp_nvhe(__pkvm_teardown_tracing))
		return;

	ring_buffer_free(hyp_buffer->trace_buffer);
	hyp_trace_teardown_pages(desc, INT_MAX);
	hyp_trace_free_pages(desc);
	free_pages_exact(desc, desc_size);
	hyp_buffer->trace_buffer = NULL;
}

static int hyp_trace_start(void)
{
	struct hyp_trace_buffer *hyp_buffer = &hyp_trace_buffer;
	int ret = 0;

	mutex_lock(&hyp_buffer->lock);

	if (hyp_buffer->tracing_on)
		goto out;

	hyp_clock_start(hyp_buffer);

	ret = hyp_trace_buffer_load(hyp_buffer, hyp_trace_buffer_size);
	if (ret)
		goto out;

	hyp_clock_wait(hyp_buffer);

	ret = kvm_call_hyp_nvhe(__pkvm_enable_tracing, true);
	if (ret) {
		hyp_trace_buffer_teardown(hyp_buffer);
		goto out;
	}

	hyp_buffer->tracing_on = true;

out:
	if (!hyp_buffer->tracing_on)
		hyp_clock_stop(hyp_buffer);

	mutex_unlock(&hyp_buffer->lock);

	return ret;
}

static void hyp_trace_stop(void)
{
	struct hyp_trace_buffer *hyp_buffer = &hyp_trace_buffer;
	int ret;

	mutex_lock(&hyp_buffer->lock);

	if (!hyp_buffer->tracing_on)
		goto end;

	ret = kvm_call_hyp_nvhe(__pkvm_enable_tracing, false);
	if (!ret) {
		hyp_clock_stop(hyp_buffer);
		ring_buffer_poll_writer(hyp_buffer->trace_buffer,
					RING_BUFFER_ALL_CPUS);
		hyp_buffer->tracing_on = false;
		hyp_trace_buffer_teardown(hyp_buffer);
	}

end:
	mutex_unlock(&hyp_buffer->lock);
}

static ssize_t hyp_tracing_on(struct file *filp, const char __user *ubuf,
			      size_t cnt, loff_t *ppos)
{
	unsigned long val;
	int ret;

	ret = kstrtoul_from_user(ubuf, cnt, 10, &val);
	if (ret)
		return ret;

	if (val)
		ret = hyp_trace_start();
	else
		hyp_trace_stop();

	return ret ? ret : cnt;
}

static ssize_t hyp_tracing_on_read(struct file *filp, char __user *ubuf,
				   size_t cnt, loff_t *ppos)
{
	char buf[3];
	int r;

	mutex_lock(&hyp_trace_buffer.lock);
	r = sprintf(buf, "%d\n", hyp_trace_buffer.tracing_on);
	mutex_unlock(&hyp_trace_buffer.lock);

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static const struct file_operations hyp_tracing_on_fops = {
	.write	= hyp_tracing_on,
	.read	= hyp_tracing_on_read,
};

static ssize_t hyp_buffer_size(struct file *filp, const char __user *ubuf,
			       size_t cnt, loff_t *ppos)
{
	unsigned long val;
	int ret;

	ret = kstrtoul_from_user(ubuf, cnt, 10, &val);
	if (ret)
		return ret;

	if (!val)
		return -EINVAL;

	mutex_lock(&hyp_trace_buffer.lock);
	hyp_trace_buffer_size = val << 10; /* KB to B */
	mutex_unlock(&hyp_trace_buffer.lock);

	return cnt;
}

static ssize_t hyp_buffer_size_read(struct file *filp, char __user *ubuf,
				    size_t cnt, loff_t *ppos)
{
	char buf[64];
	int r;

	mutex_lock(&hyp_trace_buffer.lock);
	r = sprintf(buf, "%lu (%s)\n", hyp_trace_buffer_size >> 10,
		    hyp_trace_buffer_loaded(&hyp_trace_buffer) ?
			"loaded" : "unloaded");
	mutex_unlock(&hyp_trace_buffer.lock);

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static const struct file_operations hyp_buffer_size_fops = {
	.write	= hyp_buffer_size,
	.read	= hyp_buffer_size_read,
};

static void ht_print_trace_time(struct ht_iterator *iter)
{
	unsigned long usecs_rem;
	u64 ts_ns = iter->ts;

	do_div(ts_ns, 1000);
	usecs_rem = do_div(ts_ns, USEC_PER_SEC);

	trace_seq_printf(&iter->seq, "%5lu.%06lu: ",
			 (unsigned long)ts_ns, usecs_rem);
}

static void ht_print_trace_cpu(struct ht_iterator *iter)
{
	trace_seq_printf(&iter->seq, "[%03d]\t", iter->ent_cpu);
}

static int ht_print_trace_fmt(struct ht_iterator *iter)
{
	struct hyp_event *e;

	if (iter->lost_events)
		trace_seq_printf(&iter->seq, "CPU:%d [LOST %lu EVENTS]\n",
				 iter->ent_cpu, iter->lost_events);

	ht_print_trace_cpu(iter);
	ht_print_trace_time(iter);

	e = hyp_trace_find_event(iter->ent->id);
	if (e)
		e->trace_func(iter);
	else
		trace_seq_printf(&iter->seq, "Unknown event id %d\n", iter->ent->id);

	return trace_seq_has_overflowed(&iter->seq) ? -EOVERFLOW : 0;
};

static struct ring_buffer_event *__ht_next_pipe_event(struct ht_iterator *iter)
{
	struct trace_buffer *trace_buffer = iter->hyp_buffer->trace_buffer;
	struct ring_buffer_event *evt = NULL;
	int cpu = iter->cpu;

	if (cpu != RING_BUFFER_ALL_CPUS) {
		if (ring_buffer_empty_cpu(trace_buffer, cpu))
			return NULL;

		iter->ent_cpu = cpu;

		return ring_buffer_peek(trace_buffer, cpu, &iter->ts,
					&iter->lost_events);
	}

	iter->ts = LLONG_MAX;
	for_each_possible_cpu(cpu) {
		struct ring_buffer_event *_evt;
		unsigned long lost_events;
		u64 ts;

		if (ring_buffer_empty_cpu(trace_buffer, cpu))
			continue;

		_evt = ring_buffer_peek(trace_buffer, cpu, &ts,
					&lost_events);
		if (!_evt)
			continue;

		if (ts >= iter->ts)
			continue;

		iter->ts = ts;
		iter->ent_cpu = cpu;
		iter->lost_events = lost_events;
		evt = _evt;
	}

	return evt;
}

static void *ht_next_pipe_event(struct ht_iterator *iter)
{
	struct ring_buffer_event *event;

	event = __ht_next_pipe_event(iter);
	if (!event)
		return NULL;

	iter->ent = (struct hyp_entry_hdr *)&event->array[1];
	iter->ent_size = event->array[0];

	return iter;
}

static ssize_t
hyp_trace_pipe_read(struct file *file, char __user *ubuf,
		    size_t cnt, loff_t *ppos)
{
	struct ht_iterator *iter = (struct ht_iterator *)file->private_data;
	struct trace_buffer *trace_buffer = iter->hyp_buffer->trace_buffer;
	int ret;

copy_to_user:
	ret = trace_seq_to_user(&iter->seq, ubuf, cnt);
	if (ret != -EBUSY)
		return ret;

	trace_seq_init(&iter->seq);

	ret = ring_buffer_wait(trace_buffer, iter->cpu, 0, NULL, NULL);
	if (ret < 0)
		return ret;

	while (ht_next_pipe_event(iter)) {
		int prev_len = iter->seq.seq.len;

		if (ht_print_trace_fmt(iter)) {
			iter->seq.seq.len = prev_len;
			break;
		}

		ring_buffer_consume(trace_buffer, iter->ent_cpu, NULL, NULL);
	}

	goto copy_to_user;
}

static void hyp_trace_buffer_printk(struct hyp_trace_buffer *hyp_buffer);

static void __poll_writer(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ht_iterator *iter;

	iter = container_of(dwork, struct ht_iterator, poll_work);

	ring_buffer_poll_writer(iter->hyp_buffer->trace_buffer, iter->cpu);

	hyp_trace_buffer_printk(iter->hyp_buffer);

	schedule_delayed_work((struct delayed_work *)work,
			      msecs_to_jiffies(RB_POLL_MS));
}

static struct ht_iterator *
ht_iterator_create(struct hyp_trace_buffer *hyp_buffer, int cpu)
{
	struct ht_iterator *iter = NULL;
	int ret;

	WARN_ON(!mutex_is_locked(&hyp_buffer->lock));

	if (hyp_buffer->nr_readers == INT_MAX) {
		ret = -EBUSY;
		goto unlock;
	}

	ret = hyp_trace_buffer_load(hyp_buffer, hyp_trace_buffer_size);
	if (ret)
		goto unlock;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter) {
		ret = -ENOMEM;
		goto unlock;
	}
	iter->hyp_buffer = hyp_buffer;
	iter->cpu = cpu;
	trace_seq_init(&iter->seq);

	ret = ring_buffer_poll_writer(hyp_buffer->trace_buffer, cpu);
	if (ret)
		goto unlock;

	INIT_DELAYED_WORK(&iter->poll_work, __poll_writer);
	schedule_delayed_work(&iter->poll_work, msecs_to_jiffies(RB_POLL_MS));

	hyp_buffer->nr_readers++;

unlock:
	if (ret) {
		hyp_trace_buffer_teardown(hyp_buffer);
		kfree(iter);
		iter = NULL;
	}

	return iter;
}

static int hyp_trace_pipe_open(struct inode *inode, struct file *file)
{
	struct hyp_trace_buffer *hyp_buffer = &hyp_trace_buffer;
	int cpu = (s64)inode->i_private;

	mutex_lock(&hyp_buffer->lock);

	file->private_data = ht_iterator_create(hyp_buffer, cpu);

	mutex_unlock(&hyp_buffer->lock);

	return file->private_data ? 0 : -EINVAL;
}

static int hyp_trace_pipe_release(struct inode *inode, struct file *file)
{
	struct hyp_trace_buffer *hyp_buffer = &hyp_trace_buffer;
	struct ht_iterator *iter = file->private_data;

	cancel_delayed_work_sync(&iter->poll_work);

	mutex_lock(&hyp_buffer->lock);

	WARN_ON(--hyp_buffer->nr_readers < 0);

	hyp_trace_buffer_teardown(hyp_buffer);

	mutex_unlock(&hyp_buffer->lock);

	kfree(iter);

	return 0;
}

static const struct file_operations hyp_trace_pipe_fops = {
	.open           = hyp_trace_pipe_open,
	.read           = hyp_trace_pipe_read,
	.release        = hyp_trace_pipe_release,
};

static ssize_t
hyp_trace_raw_read(struct file *file, char __user *ubuf,
		   size_t cnt, loff_t *ppos)
{
	struct ht_iterator *iter = (struct ht_iterator *)file->private_data;
	size_t size;
	int ret;

	if (iter->copy_leftover)
		goto read;

again:
	ret = ring_buffer_read_page(iter->hyp_buffer->trace_buffer,
				    (struct buffer_data_read_page *)iter->spare,
				    cnt, iter->cpu, 0);
	if (ret < 0) {
		if (!ring_buffer_empty_cpu(iter->hyp_buffer->trace_buffer,
					   iter->cpu))
			return 0;

		ret = ring_buffer_wait(iter->hyp_buffer->trace_buffer,
				       iter->cpu, 0, NULL, NULL);
		if (ret < 0)
			return ret;

		goto again;
	}

	iter->copy_leftover = 0;

read:
	size = PAGE_SIZE - iter->copy_leftover;
	if (size > cnt)
		size = cnt;

	ret = copy_to_user(ubuf, iter->spare + PAGE_SIZE - size, size);
	if (ret == size)
		return -EFAULT;

	size -= ret;
	*ppos += size;
	iter->copy_leftover = ret;

	return size;
}

static int hyp_trace_raw_open(struct inode *inode, struct file *file)
{
	int ret = hyp_trace_pipe_open(inode, file);
	struct ht_iterator *iter;

	if (ret)
		return ret;

	iter = file->private_data;
	iter->spare = ring_buffer_alloc_read_page(iter->hyp_buffer->trace_buffer,
						  iter->cpu);
	if (IS_ERR(iter->spare)) {
		ret = PTR_ERR(iter->spare);
		iter->spare = NULL;
		return ret;
	}

	return 0;
}

static int hyp_trace_raw_release(struct inode *inode, struct file *file)
{
	struct ht_iterator *iter = file->private_data;

	ring_buffer_free_read_page(iter->hyp_buffer->trace_buffer, iter->cpu,
				   iter->spare);

	return hyp_trace_pipe_release(inode, file);
}

static const struct file_operations hyp_trace_raw_fops = {
	.open           = hyp_trace_raw_open,
	.read           = hyp_trace_raw_read,
	.release        = hyp_trace_raw_release,
};

static void hyp_trace_reset(int cpu)
{
	struct hyp_trace_buffer *hyp_buffer = &hyp_trace_buffer;

	mutex_lock(&hyp_buffer->lock);

	if (!hyp_trace_buffer_loaded(hyp_buffer))
		goto out;

	if (cpu == RING_BUFFER_ALL_CPUS)
		ring_buffer_reset(hyp_buffer->trace_buffer);
	else
		ring_buffer_reset_cpu(hyp_buffer->trace_buffer, cpu);

out:
	mutex_unlock(&hyp_buffer->lock);
}

static int hyp_trace_open(struct inode *inode, struct file *file)
{
	int cpu = (s64)inode->i_private;

	if (file->f_mode & FMODE_WRITE) {
		hyp_trace_reset(cpu);

		return 0;
	}

	return -EPERM;
}

static ssize_t hyp_trace_write(struct file *filp, const char __user *ubuf,
			       size_t count, loff_t *ppos)
{
	return count;
}

static const struct file_operations hyp_trace_fops = {
	.open           = hyp_trace_open,
	.write          = hyp_trace_write,
	.release        = NULL,
};

static int hyp_trace_clock_show(struct seq_file *m, void *v)
{
	seq_puts(m, "[boot]\n");

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(hyp_trace_clock);

#ifdef CONFIG_PKVM_SELFTESTS
static int selftest_event_open(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_WRITE)
		return kvm_call_hyp_nvhe(__pkvm_selftest_event);

	return 0;
}

static ssize_t selftest_event_write(struct file *f, const char __user *buf,
				    size_t cnt, loff_t *pos)
{
	return cnt;
}

static const struct file_operations selftest_event_fops = {
	.open	= selftest_event_open,
	.write	= selftest_event_write,
};

static void hyp_trace_init_testing_tracefs(struct dentry *root)
{
	tracefs_create_file("selftest_event", TRACEFS_MODE_WRITE, root, NULL,
			    &selftest_event_fops);
}
#else
static void hyp_trace_init_testing_tracefs(struct dentry *root) { }
#endif

static int hyp_trace_buffer_printk_init(struct hyp_trace_buffer *hyp_buffer)
{
	int ret = 0;

	mutex_lock(&hyp_buffer->lock);

	if (hyp_buffer->printk_iter)
		goto unlock;

	hyp_buffer->printk_iter = ht_iterator_create(hyp_buffer,
						     RING_BUFFER_ALL_CPUS);
	if (!hyp_buffer->printk_iter)
		ret = -EINVAL;
unlock:
	mutex_unlock(&hyp_buffer->lock);

	return ret;
}

static void hyp_trace_buffer_printk(struct hyp_trace_buffer *hyp_buffer)
{
	struct ht_iterator *ht_iter = hyp_buffer->printk_iter;

	if (!hyp_trace_buffer.printk_on)
		return;

	trace_seq_init(&ht_iter->seq);
	while (ht_next_pipe_event(ht_iter)) {
		ht_print_trace_fmt(ht_iter);

		/* Nothing has been written in the seq_buf */
		if (!ht_iter->seq.seq.len)
			return;

		ht_iter->seq.buffer[ht_iter->seq.seq.len] = '\0';
		printk("%s", ht_iter->seq.buffer);

		ht_iter->seq.seq.len = 0;
		ring_buffer_consume(hyp_buffer->trace_buffer, ht_iter->ent_cpu,
				    NULL, NULL);
	}
}

static int hyp_trace_panic_handler(struct notifier_block *self,
				   unsigned long ev, void *v)
{
#ifdef CONFIG_PKVM_DUMP_TRACE_ON_PANIC
	if (!hyp_trace_buffer_loaded(&hyp_trace_buffer) ||
	    !hyp_trace_buffer.printk_iter)
		return NOTIFY_DONE;

	if (!strncmp("HYP panic:", v, 10))
		hyp_trace_panic = true;

	ring_buffer_poll_writer(hyp_trace_buffer.trace_buffer, RING_BUFFER_ALL_CPUS);
	hyp_trace_buffer_printk(&hyp_trace_buffer);
#endif
	return NOTIFY_DONE;
}

static struct notifier_block hyp_trace_panic_notifier = {
	.notifier_call = hyp_trace_panic_handler,
	.priority = INT_MAX - 1,
};

void hyp_trace_enable_event_early(void)
{
	if (hyp_event_early_probe()) {
		int err = hyp_trace_start();

		if (err)
			pr_warn("Failed to start early events tracing: %d\n", err);
	}
}

int hyp_trace_init_tracefs(void)
{
	struct dentry *root, *per_cpu_root;
	char per_cpu_name[16];
	long cpu;

	if (!is_protected_kvm_enabled())
		return 0;

	root = tracefs_create_dir(TRACEFS_DIR, NULL);
	if (!root) {
		pr_err("Failed to create tracefs "TRACEFS_DIR"/\n");
		return -ENODEV;
	}

	tracefs_create_file("tracing_on", TRACEFS_MODE_WRITE, root, NULL,
			    &hyp_tracing_on_fops);

	tracefs_create_file("buffer_size_kb", TRACEFS_MODE_WRITE, root, NULL,
			    &hyp_buffer_size_fops);

	tracefs_create_file("trace_pipe", TRACEFS_MODE_WRITE, root,
			    (void *)RING_BUFFER_ALL_CPUS, &hyp_trace_pipe_fops);

	tracefs_create_file("trace", TRACEFS_MODE_WRITE, root,
			    (void *)RING_BUFFER_ALL_CPUS, &hyp_trace_fops);

	tracefs_create_file("trace_clock", TRACEFS_MODE_READ, root, NULL,
			    &hyp_trace_clock_fops);

	per_cpu_root = tracefs_create_dir("per_cpu", root);
	if (!per_cpu_root) {
		pr_err("Failed to create tracefs folder "TRACEFS_DIR"/per_cpu/\n");
		return -ENODEV;
	}

	for_each_possible_cpu(cpu) {
		struct dentry *per_cpu_dir;

		snprintf(per_cpu_name, sizeof(per_cpu_name), "cpu%ld", cpu);
		per_cpu_dir = tracefs_create_dir(per_cpu_name, per_cpu_root);
		if (!per_cpu_dir) {
			pr_warn("Failed to create tracefs "TRACEFS_DIR"/per_cpu/cpu%ld\n",
				cpu);
			continue;
		}

		tracefs_create_file("trace_pipe", TRACEFS_MODE_READ, per_cpu_dir,
				    (void *)cpu, &hyp_trace_pipe_fops);

		tracefs_create_file("trace_pipe_raw", TRACEFS_MODE_READ, per_cpu_dir,
				    (void *)cpu, &hyp_trace_pipe_fops);

		tracefs_create_file("trace", TRACEFS_MODE_READ, per_cpu_dir,
				    (void *)cpu, &hyp_trace_fops);
	}

	hyp_trace_init_event_tracefs(root);

	hyp_trace_enable_event_early();

	hyp_trace_init_testing_tracefs(root);

	if (hyp_trace_buffer.printk_on &&
	    hyp_trace_buffer_printk_init(&hyp_trace_buffer))
		pr_warn("Failed to init ht_printk");

	atomic_notifier_chain_register(&panic_notifier_list, &hyp_trace_panic_notifier);

	return 0;
}
