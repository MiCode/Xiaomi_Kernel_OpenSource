/* Copyright (c) 2013-2016, 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>

#include "adreno.h"
#include "adreno_profile.h"
#include "kgsl_sharedmem.h"
#include "kgsl_cffdump.h"
#include "adreno_pm4types.h"

#define ASSIGNS_STR_FORMAT "%.8s:%u "

/*
 * Raw Data for processing later:
 *        : 3 - timestamp, count, context id
 * [per counter] - data for each counter
 *        : 1 - Register offset
 *        : 2 - Pre IB register hi/lo value
 *        : 2 - Post IB register hi/lo value
 * [per counter end]
 */
#define SIZE_DATA(cnt) (6 + (cnt) * 5)

/*
 * Pre-IB command size (in dwords):
 *        : 2 - NOP start identifier
 *        : 4 - timestamp
 *        : 4 - count
 *        : 4 - context id
 *        : 4 - pid
 *        : 4 - tid
 *        : 4 - type
 * [loop count start] - for each counter to watch
 *        : 4 - Register offset
 *        : 4 - Register read lo
 *        : 4 - Register read high
 * [loop end]
 *        : 2 - NOP end identifier
 */
#define SIZE_PREIB(cnt) (28 + (cnt) * 12)

/*
 * Post-IB command size (in dwords):
 *        : 2 - NOP start identifier
 * [loop count start] - for each counter to watch
 *        : 4 - Register read lo
 *        : 4 - Register read high
 * [loop end]
 *        : 2 - NOP end identifier
 */
#define SIZE_POSTIB(cnt) (4 + (cnt) * 8)

/* Counter data + Pre size + post size = total size */
#define SIZE_SHARED_ENTRY(cnt) (SIZE_DATA(cnt) + SIZE_PREIB(cnt) \
		+ SIZE_POSTIB(cnt))

/*
 * Space for following string :"%u %u %u %.5s %u "
 * [count iterations]: "%.8s:%u %llu %llu%c"
 */
#define SIZE_PIPE_ENTRY(cnt) (50 + (cnt) * 62)
#define SIZE_LOG_ENTRY(cnt) (6 + (cnt) * 5)

static inline uint _ib_start(struct adreno_device *adreno_dev,
			 unsigned int *cmds)
{
	unsigned int *start = cmds;

	*cmds++ = cp_packet(adreno_dev, CP_NOP, 1);
	*cmds++ = KGSL_START_OF_PROFILE_IDENTIFIER;

	return cmds - start;
}

static inline uint _ib_end(struct adreno_device *adreno_dev,
			  unsigned int *cmds)
{
	unsigned int *start = cmds;

	*cmds++ = cp_packet(adreno_dev, CP_NOP, 1);
	*cmds++ = KGSL_END_OF_PROFILE_IDENTIFIER;

	return cmds - start;
}

static inline uint _ib_cmd_mem_write(struct adreno_device *adreno_dev,
			uint *cmds, uint64_t gpuaddr, uint val, uint *off)
{
	unsigned int *start = cmds;

	*cmds++ = cp_mem_packet(adreno_dev, CP_MEM_WRITE, 2, 1);
	cmds += cp_gpuaddr(adreno_dev, cmds, gpuaddr);
	*cmds++ = val;

	*off += sizeof(unsigned int);
	return cmds - start;
}

static inline uint _ib_cmd_reg_to_mem(struct adreno_device *adreno_dev,
			uint *cmds, uint64_t gpuaddr, uint val, uint *off)
{
	unsigned int *start = cmds;

	*cmds++ = cp_mem_packet(adreno_dev, CP_REG_TO_MEM, 2, 1);
	*cmds++ = val;
	cmds += cp_gpuaddr(adreno_dev, cmds, gpuaddr);

	*off += sizeof(unsigned int);
	return cmds - start;
}

static inline int _create_ib_ref(struct adreno_device *adreno_dev,
		struct kgsl_memdesc *memdesc, unsigned int *cmd,
		unsigned int cnt, unsigned int off)
{
	unsigned int *start = cmd;

	*cmd++ = cp_mem_packet(adreno_dev, CP_INDIRECT_BUFFER_PFE, 2, 1);
	cmd += cp_gpuaddr(adreno_dev, cmd, (memdesc->gpuaddr + off));
	*cmd++ = cnt;

	return cmd - start;
}

static int _build_pre_ib_cmds(struct adreno_device *adreno_dev,
		struct adreno_profile *profile,
		unsigned int *rbcmds, unsigned int head,
		unsigned int timestamp, struct adreno_context *drawctxt)
{
	struct adreno_profile_assigns_list *entry;
	unsigned int *start, *ibcmds;
	unsigned int count = profile->assignment_count;
	uint64_t gpuaddr = profile->shared_buffer.gpuaddr;
	unsigned int ib_offset = head + SIZE_DATA(count);
	unsigned int data_offset = head * sizeof(unsigned int);

	ibcmds = ib_offset + ((unsigned int *) profile->shared_buffer.hostptr);
	start = ibcmds;

	/* start of profile identifier */
	ibcmds += _ib_start(adreno_dev, ibcmds);

	/*
	 * Write ringbuffer commands to save the following to memory:
	 * timestamp, count, context_id, pid, tid, context type
	 */
	ibcmds += _ib_cmd_mem_write(adreno_dev, ibcmds, gpuaddr + data_offset,
			timestamp, &data_offset);
	ibcmds += _ib_cmd_mem_write(adreno_dev, ibcmds, gpuaddr + data_offset,
			profile->assignment_count, &data_offset);
	ibcmds += _ib_cmd_mem_write(adreno_dev, ibcmds, gpuaddr + data_offset,
			drawctxt->base.id, &data_offset);
	ibcmds += _ib_cmd_mem_write(adreno_dev, ibcmds, gpuaddr + data_offset,
			drawctxt->base.proc_priv->pid, &data_offset);
	ibcmds += _ib_cmd_mem_write(adreno_dev, ibcmds, gpuaddr + data_offset,
			drawctxt->base.tid, &data_offset);
	ibcmds += _ib_cmd_mem_write(adreno_dev, ibcmds, gpuaddr + data_offset,
			drawctxt->type, &data_offset);

	/* loop for each countable assigned */
	list_for_each_entry(entry, &profile->assignments_list, list) {
		ibcmds += _ib_cmd_mem_write(adreno_dev, ibcmds,
				gpuaddr + data_offset, entry->offset,
				&data_offset);
		ibcmds += _ib_cmd_reg_to_mem(adreno_dev, ibcmds,
				gpuaddr + data_offset, entry->offset,
				&data_offset);
		ibcmds += _ib_cmd_reg_to_mem(adreno_dev, ibcmds,
				gpuaddr + data_offset, entry->offset_hi,
				&data_offset);

		/* skip over post_ib counter data */
		data_offset += sizeof(unsigned int) * 2;
	}

	/* end of profile identifier */
	ibcmds += _ib_end(adreno_dev, ibcmds);

	return _create_ib_ref(adreno_dev, &profile->shared_buffer, rbcmds,
			ibcmds - start, ib_offset * sizeof(unsigned int));
}

static int _build_post_ib_cmds(struct adreno_device *adreno_dev,
		struct adreno_profile *profile,
		unsigned int *rbcmds, unsigned int head)
{
	struct adreno_profile_assigns_list *entry;
	unsigned int *start, *ibcmds;
	unsigned int count = profile->assignment_count;
	uint64_t gpuaddr =  profile->shared_buffer.gpuaddr;
	unsigned int ib_offset = head + SIZE_DATA(count) + SIZE_PREIB(count);
	unsigned int data_offset = head * sizeof(unsigned int);

	ibcmds = ib_offset + ((unsigned int *) profile->shared_buffer.hostptr);
	start = ibcmds;
	/* start of profile identifier */
	ibcmds += _ib_start(adreno_dev, ibcmds);

	/* skip over pre_ib preamble */
	data_offset += sizeof(unsigned int) * 6;

	/* loop for each countable assigned */
	list_for_each_entry(entry, &profile->assignments_list, list) {
		/* skip over pre_ib counter data */
		data_offset += sizeof(unsigned int) * 3;
		ibcmds += _ib_cmd_reg_to_mem(adreno_dev, ibcmds,
				gpuaddr + data_offset, entry->offset,
				&data_offset);
		ibcmds += _ib_cmd_reg_to_mem(adreno_dev, ibcmds,
				gpuaddr + data_offset, entry->offset_hi,
				&data_offset);
	}

	/* end of profile identifier */
	ibcmds += _ib_end(adreno_dev, ibcmds);

	return _create_ib_ref(adreno_dev, &profile->shared_buffer, rbcmds,
			ibcmds - start, ib_offset * sizeof(unsigned int));
}

static bool shared_buf_empty(struct adreno_profile *profile)
{
	if (profile->shared_buffer.hostptr == NULL ||
			profile->shared_buffer.size == 0)
		return true;

	if (profile->shared_head == profile->shared_tail)
		return true;

	return false;
}

static inline void shared_buf_inc(unsigned int max_size,
		unsigned int *offset, size_t inc)
{
	*offset = (*offset + inc) % max_size;
}

static inline void log_buf_wrapcnt(unsigned int cnt, uintptr_t *off)
{
	*off = (*off + cnt) % ADRENO_PROFILE_LOG_BUF_SIZE_DWORDS;
}

static inline void log_buf_wrapinc_len(unsigned int *profile_log_buffer,
		unsigned int **ptr, unsigned int len)
{
	*ptr += len;
	if (*ptr >= (profile_log_buffer +
				ADRENO_PROFILE_LOG_BUF_SIZE_DWORDS))
		*ptr -= ADRENO_PROFILE_LOG_BUF_SIZE_DWORDS;
}

static inline void log_buf_wrapinc(unsigned int *profile_log_buffer,
		unsigned int **ptr)
{
	log_buf_wrapinc_len(profile_log_buffer, ptr, 1);
}

static inline unsigned int log_buf_available(struct adreno_profile *profile,
		unsigned int *head_ptr)
{
	uintptr_t tail, head;

	tail = (uintptr_t) profile->log_tail -
		(uintptr_t) profile->log_buffer;
	head = (uintptr_t)head_ptr - (uintptr_t) profile->log_buffer;
	if (tail > head)
		return (tail - head) / sizeof(uintptr_t);
	else
		return ADRENO_PROFILE_LOG_BUF_SIZE_DWORDS - ((head - tail) /
				sizeof(uintptr_t));
}

static inline unsigned int shared_buf_available(struct adreno_profile *profile)
{
	if (profile->shared_tail > profile->shared_head)
		return profile->shared_tail - profile->shared_head;
	else
		return profile->shared_size -
			(profile->shared_head - profile->shared_tail);
}

static struct adreno_profile_assigns_list *_find_assignment_by_offset(
		struct adreno_profile *profile, unsigned int offset)
{
	struct adreno_profile_assigns_list *entry;

	list_for_each_entry(entry, &profile->assignments_list, list) {
		if (entry->offset == offset)
			return entry;
	}

	return NULL;
}

static bool _in_assignments_list(struct adreno_profile *profile,
		unsigned int groupid, unsigned int countable)
{
	struct adreno_profile_assigns_list *entry;

	list_for_each_entry(entry, &profile->assignments_list, list) {
		if (entry->groupid == groupid && entry->countable ==
				countable)
			return true;
	}

	return false;
}

static bool _add_to_assignments_list(struct adreno_profile *profile,
		const char *str, unsigned int groupid, unsigned int countable,
		unsigned int offset, unsigned int offset_hi)
{
	struct adreno_profile_assigns_list *entry;

	/* first make sure we can alloc memory */
	entry = kmalloc(sizeof(struct adreno_profile_assigns_list), GFP_KERNEL);
	if (!entry)
		return false;

	list_add_tail(&entry->list, &profile->assignments_list);

	entry->countable = countable;
	entry->groupid = groupid;
	entry->offset = offset;
	entry->offset_hi = offset_hi;

	strlcpy(entry->name, str, sizeof(entry->name));

	profile->assignment_count++;

	return true;
}

static bool results_available(struct adreno_device *adreno_dev,
		struct adreno_profile *profile, unsigned int *shared_buf_tail)
{
	unsigned int global_eop;
	unsigned int off = profile->shared_tail;
	unsigned int *shared_ptr = (unsigned int *)
		profile->shared_buffer.hostptr;
	unsigned int ts, cnt;
	int ts_cmp;

	/*
	 * If shared_buffer empty or Memstore EOP timestamp is less than
	 * outstanding counter buffer timestamps then no results available
	 */
	if (shared_buf_empty(profile))
		return false;

	if (adreno_rb_readtimestamp(adreno_dev,
			adreno_dev->cur_rb,
			KGSL_TIMESTAMP_RETIRED, &global_eop))
		return false;
	do {
		cnt = *(shared_ptr + off + 1);
		if (cnt == 0)
			return false;

		ts = *(shared_ptr + off);
		ts_cmp = timestamp_cmp(ts, global_eop);
		if (ts_cmp >= 0) {
			*shared_buf_tail = off;
			if (off == profile->shared_tail)
				return false;
			else
				return true;
		}
		shared_buf_inc(profile->shared_size, &off,
				SIZE_SHARED_ENTRY(cnt));
	} while (off != profile->shared_head);

	*shared_buf_tail = profile->shared_head;

	return true;
}

static void transfer_results(struct adreno_profile *profile,
		unsigned int shared_buf_tail)
{
	unsigned int buf_off;
	unsigned int ts, cnt, ctxt_id, pid, tid, client_type;
	unsigned int *ptr = (unsigned int *) profile->shared_buffer.hostptr;
	unsigned int *log_ptr, *log_base;
	struct adreno_profile_assigns_list *assigns_list;
	int i, tmp_tail;

	log_ptr = profile->log_head;
	log_base = profile->log_buffer;
	if (log_ptr == NULL)
		return;

	/*
	 * go through counter buffers and format for write into log_buffer
	 * if log buffer doesn't have space just overwrite it circularly
	 * shared_buf is guaranteed to not wrap within an entry so can use
	 * ptr increment
	 */
	while (profile->shared_tail != shared_buf_tail) {
		buf_off = profile->shared_tail;
		/*
		 * format: timestamp, count, context_id
		 * count entries: pc_off, pc_start, pc_end
		 */
		ts = *(ptr + buf_off++);
		cnt = *(ptr + buf_off++);
		ctxt_id = *(ptr + buf_off++);
		pid = *(ptr + buf_off++);
		tid = *(ptr + buf_off++);
		client_type = *(ptr + buf_off++);

		/*
		 * if entry overwrites the tail of log_buffer then adjust tail
		 * ptr to make room for the new entry, discarding old entry
		 */
		while (log_buf_available(profile, log_ptr) <=
				SIZE_LOG_ENTRY(cnt)) {
			unsigned int size_tail;
			uintptr_t boff;
			size_tail = SIZE_LOG_ENTRY(0xffff &
					*(profile->log_tail));
			boff = ((uintptr_t) profile->log_tail -
				(uintptr_t) log_base) / sizeof(uintptr_t);
			log_buf_wrapcnt(size_tail, &boff);
			profile->log_tail = log_base + boff;
		}

		*log_ptr = cnt;
		log_buf_wrapinc(log_base, &log_ptr);
		*log_ptr = client_type;
		log_buf_wrapinc(log_base, &log_ptr);
		*log_ptr = pid;
		log_buf_wrapinc(log_base, &log_ptr);
		*log_ptr = tid;
		log_buf_wrapinc(log_base, &log_ptr);
		*log_ptr = ctxt_id;
		log_buf_wrapinc(log_base, &log_ptr);
		*log_ptr = ts;
		log_buf_wrapinc(log_base, &log_ptr);

		for (i = 0; i < cnt; i++) {
			assigns_list = _find_assignment_by_offset(
					profile, *(ptr + buf_off++));
			if (assigns_list == NULL) {
				*log_ptr = (unsigned int) -1;

				shared_buf_inc(profile->shared_size,
					&profile->shared_tail,
					SIZE_SHARED_ENTRY(cnt));
				goto err;
			} else {
				*log_ptr = assigns_list->groupid << 16 |
					(assigns_list->countable & 0xffff);
			}
			log_buf_wrapinc(log_base, &log_ptr);
			*log_ptr  = *(ptr + buf_off++); /* perf cntr start hi */
			log_buf_wrapinc(log_base, &log_ptr);
			*log_ptr = *(ptr + buf_off++);  /* perf cntr start lo */
			log_buf_wrapinc(log_base, &log_ptr);
			*log_ptr = *(ptr + buf_off++);  /* perf cntr end hi */
			log_buf_wrapinc(log_base, &log_ptr);
			*log_ptr = *(ptr + buf_off++);  /* perf cntr end lo */
			log_buf_wrapinc(log_base, &log_ptr);

		}

		tmp_tail = profile->shared_tail;
		shared_buf_inc(profile->shared_size,
				&profile->shared_tail,
				SIZE_SHARED_ENTRY(cnt));
		/*
		 * Possibly lost some room as we cycled around, so it's safe to
		 * reset the max size
		 */
		if (profile->shared_tail < tmp_tail)
			profile->shared_size =
				ADRENO_PROFILE_SHARED_BUF_SIZE_DWORDS;

	}
	profile->log_head = log_ptr;
	return;
err:
	/* reset head/tail to same on error in hopes we work correctly later */
	profile->log_head = profile->log_tail;
}

static int profile_enable_get(void *data, u64 *val)
{
	struct kgsl_device *device = data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	mutex_lock(&device->mutex);
	*val = adreno_profile_enabled(&adreno_dev->profile);
	mutex_unlock(&device->mutex);

	return 0;
}

static int profile_enable_set(void *data, u64 val)
{
	struct kgsl_device *device = data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;

	mutex_lock(&device->mutex);

	if (val && profile->log_buffer == NULL) {
		/* allocate profile_log_buffer the first time enabled */
		profile->log_buffer = vmalloc(ADRENO_PROFILE_LOG_BUF_SIZE);
		if (profile->log_buffer == NULL) {
			mutex_unlock(&device->mutex);
			return -ENOMEM;
		}
		profile->log_tail = profile->log_buffer;
		profile->log_head = profile->log_buffer;
	}

	profile->enabled = val;

	mutex_unlock(&device->mutex);

	return 0;
}

static ssize_t profile_assignments_read(struct file *filep,
		char __user *ubuf, size_t max, loff_t *ppos)
{
	struct kgsl_device *device = (struct kgsl_device *) filep->private_data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;
	struct adreno_profile_assigns_list *entry;
	int len = 0, max_size = PAGE_SIZE;
	char *buf, *pos;
	ssize_t size = 0;

	mutex_lock(&device->mutex);

	if (profile->assignment_count == 0) {
		mutex_unlock(&device->mutex);
		return 0;
	}

	buf = kmalloc(max_size, GFP_KERNEL);
	if (!buf) {
		mutex_unlock(&device->mutex);
		return -ENOMEM;
	}

	pos = buf;

	/* copy all assingments from list to str */
	list_for_each_entry(entry, &profile->assignments_list, list) {
		len = snprintf(pos, max_size, ASSIGNS_STR_FORMAT,
				entry->name, entry->countable);

		max_size -= len;
		pos += len;
	}

	size = simple_read_from_buffer(ubuf, max, ppos, buf,
			strlen(buf));

	kfree(buf);

	mutex_unlock(&device->mutex);
	return size;
}

static void _remove_assignment(struct adreno_device *adreno_dev,
		unsigned int groupid, unsigned int countable)
{
	struct adreno_profile *profile = &adreno_dev->profile;
	struct adreno_profile_assigns_list *entry, *tmp;

	list_for_each_entry_safe(entry, tmp, &profile->assignments_list, list) {
		if (entry->groupid == groupid &&
				entry->countable == countable) {
			list_del(&entry->list);

			profile->assignment_count--;

			kfree(entry);

			/* remove from perf counter allocation */
			adreno_perfcounter_put(adreno_dev, groupid, countable,
					PERFCOUNTER_FLAG_KERNEL);
		}
	}
}

static void _add_assignment(struct adreno_device *adreno_dev,
		unsigned int groupid, unsigned int countable)
{
	struct adreno_profile *profile = &adreno_dev->profile;
	unsigned int offset, offset_hi;
	const char *name = NULL;

	name = adreno_perfcounter_get_name(adreno_dev, groupid);
	if (!name)
		return;

	/* if already in assigned list skip it */
	if (_in_assignments_list(profile, groupid, countable))
		return;

	/* add to perf counter allocation, if fail skip it */
	if (adreno_perfcounter_get(adreno_dev, groupid, countable,
				&offset, &offset_hi, PERFCOUNTER_FLAG_NONE))
		return;

	/* add to assignments list, put counter back if error */
	if (!_add_to_assignments_list(profile, name, groupid,
				countable, offset, offset_hi))
		adreno_perfcounter_put(adreno_dev, groupid,
				countable, PERFCOUNTER_FLAG_KERNEL);
}

static char *_parse_next_assignment(struct adreno_device *adreno_dev,
		char *str, int *groupid, int *countable, bool *remove)
{
	char *groupid_str, *countable_str, *next_str = NULL;
	int ret;

	*groupid = -EINVAL;
	*countable = -EINVAL;
	*remove = false;

	/* remove spaces */
	while (*str == ' ')
		str++;

	/* check if it's a remove assignment */
	if (*str == '-') {
		*remove = true;
		str++;
	}

	/* get the groupid string */
	groupid_str = str;
	while (*str != ':') {
		if (*str == '\0')
			return NULL;
		*str = tolower(*str);
		str++;
	}
	if (groupid_str == str)
		return NULL;

	*str = '\0';
	str++;

	/* get the countable string */
	countable_str = str;
	while (*str != ' ' && *str != '\0')
		str++;
	if (countable_str == str)
		return NULL;

	/*
	 * If we have reached the end of the original string then make sure we
	 * return NULL from this function or we could accidently overrun
	 */

	if (*str != '\0') {
		*str = '\0';
		next_str = str + 1;
	}

	/* set results */
	*groupid = adreno_perfcounter_get_groupid(adreno_dev,
			groupid_str);
	if (*groupid < 0)
		return NULL;
	ret = kstrtou32(countable_str, 10, countable);
	if (ret)
		return NULL;

	return next_str;
}

static ssize_t profile_assignments_write(struct file *filep,
		const char __user *user_buf, size_t len, loff_t *off)
{
	struct kgsl_device *device = (struct kgsl_device *) filep->private_data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;
	size_t size = 0;
	char *buf, *pbuf;
	bool remove_assignment = false;
	int groupid, countable, ret;

	if (len >= PAGE_SIZE || len == 0)
		return -EINVAL;

	buf = kmalloc(len + 1, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user(buf, user_buf, len)) {
		size = -EFAULT;
		goto error_free;
	}

	mutex_lock(&device->mutex);

	if (adreno_profile_enabled(profile)) {
		size = -EINVAL;
		goto error_unlock;
	}

	ret = kgsl_active_count_get(device);
	if (ret) {
		size = ret;
		goto error_unlock;
	}

	/*
	 * When adding/removing assignments, ensure that the GPU is done with
	 * all it's work.  This helps to syncronize the work flow to the
	 * GPU and avoid racey conditions.
	 */
	if (adreno_idle(device)) {
		size = -ETIMEDOUT;
		goto error_put;
	}

	/* clear all shared buffer results */
	adreno_profile_process_results(adreno_dev);

	pbuf = buf;

	/* clear the log buffer */
	if (profile->log_buffer != NULL) {
		profile->log_head = profile->log_buffer;
		profile->log_tail = profile->log_buffer;
	}


	/* for sanity and parsing, ensure it is null terminated */
	buf[len] = '\0';

	/* parse file buf and add(remove) to(from) appropriate lists */
	while (pbuf) {
		pbuf = _parse_next_assignment(adreno_dev, pbuf, &groupid,
				&countable, &remove_assignment);
		if (groupid < 0 || countable < 0)
			break;

		if (remove_assignment)
			_remove_assignment(adreno_dev, groupid, countable);
		else
			_add_assignment(adreno_dev, groupid, countable);
	}

	size = len;

error_put:
	kgsl_active_count_put(device);
error_unlock:
	mutex_unlock(&device->mutex);
error_free:
	kfree(buf);
	return size;
}

static int _pipe_print_pending(char __user *ubuf, size_t max)
{
	loff_t unused = 0;
	char str[] = "Operation Would Block!";

	return simple_read_from_buffer(ubuf, max,
			&unused, str, strlen(str));
}

static int _pipe_print_results(struct adreno_device *adreno_dev,
		char __user *ubuf, size_t max)
{
	struct adreno_profile *profile = &adreno_dev->profile;
	const char *grp_name;
	char __user *usr_buf = ubuf;
	unsigned int *log_ptr = NULL, *tmp_log_ptr = NULL;
	int len, i;
	int status = 0;
	ssize_t size, total_size = 0;
	unsigned int cnt, api_type, ctxt_id, pid, tid, ts, cnt_reg;
	unsigned long long pc_start, pc_end;
	const char *api_str;
	char format_space;
	loff_t unused = 0;
	char pipe_hdr_buf[51];   /* 4 uint32 + 5 space + 5 API type + '\0' */
	char pipe_cntr_buf[63];  /* 2 uint64 + 1 uint32 + 4 spaces + 8 group */

	/* convert unread entries to ASCII, copy to user-space */
	log_ptr = profile->log_tail;

	do {
		/* store the tmp var for error cases so we can skip */
		tmp_log_ptr = log_ptr;

		/* Too many to output to pipe, so skip this data */
		cnt = *log_ptr;
		log_buf_wrapinc(profile->log_buffer, &log_ptr);

		if (SIZE_PIPE_ENTRY(cnt) > max) {
			log_buf_wrapinc_len(profile->log_buffer,
				&tmp_log_ptr, SIZE_PIPE_ENTRY(cnt));
			log_ptr = tmp_log_ptr;
			goto done;
		}

		/*
		 * Not enough space left in pipe, return without doing
		 * anything
		 */
		if ((max - (usr_buf - ubuf)) < SIZE_PIPE_ENTRY(cnt)) {
			log_ptr = tmp_log_ptr;
			goto done;
		}

		api_type = *log_ptr;
		api_str = get_api_type_str(api_type);
		log_buf_wrapinc(profile->log_buffer, &log_ptr);
		pid = *log_ptr;
		log_buf_wrapinc(profile->log_buffer, &log_ptr);
		tid = *log_ptr;
		log_buf_wrapinc(profile->log_buffer, &log_ptr);
		ctxt_id =  *log_ptr;
		log_buf_wrapinc(profile->log_buffer, &log_ptr);
		ts = *log_ptr;
		log_buf_wrapinc(profile->log_buffer, &log_ptr);
		len = snprintf(pipe_hdr_buf, sizeof(pipe_hdr_buf) - 1,
				"%u %u %u %.5s %u ",
				pid, tid, ctxt_id, api_str, ts);
		size = simple_read_from_buffer(usr_buf,
				max - (usr_buf - ubuf),
				&unused, pipe_hdr_buf, len);

		/* non-fatal error, so skip rest of entry and return */
		if (size < 0) {
			log_buf_wrapinc_len(profile->log_buffer,
				&tmp_log_ptr, SIZE_PIPE_ENTRY(cnt));
			log_ptr = tmp_log_ptr;
			goto done;
		}

		unused = 0;
		usr_buf += size;
		total_size += size;

		for (i = 0; i < cnt; i++) {
			unsigned int start_lo, start_hi;
			unsigned int end_lo, end_hi;

			grp_name = adreno_perfcounter_get_name(
					adreno_dev, (*log_ptr >> 16) & 0xffff);

			/* non-fatal error, so skip rest of entry and return */
			if (grp_name == NULL) {
				log_buf_wrapinc_len(profile->log_buffer,
					&tmp_log_ptr, SIZE_PIPE_ENTRY(cnt));
				log_ptr = tmp_log_ptr;
				goto done;
			}

			if (i == cnt - 1)
				format_space = '\n';
			else
				format_space = ' ';

			cnt_reg = *log_ptr & 0xffff;
			log_buf_wrapinc(profile->log_buffer, &log_ptr);
			start_lo = *log_ptr;
			log_buf_wrapinc(profile->log_buffer, &log_ptr);
			start_hi = *log_ptr;
			log_buf_wrapinc(profile->log_buffer, &log_ptr);
			end_lo = *log_ptr;
			log_buf_wrapinc(profile->log_buffer, &log_ptr);
			end_hi = *log_ptr;
			log_buf_wrapinc(profile->log_buffer, &log_ptr);

			pc_start = (((uint64_t) start_hi) << 32) | start_lo;
			pc_end = (((uint64_t) end_hi) << 32) | end_lo;

			len = snprintf(pipe_cntr_buf,
					sizeof(pipe_cntr_buf) - 1,
					"%.8s:%u %llu %llu%c",
					grp_name, cnt_reg, pc_start,
					pc_end, format_space);

			size = simple_read_from_buffer(usr_buf,
					max - (usr_buf - ubuf),
					&unused, pipe_cntr_buf, len);

			/* non-fatal error, so skip rest of entry and return */
			if (size < 0) {
				log_buf_wrapinc_len(profile->log_buffer,
					&tmp_log_ptr, SIZE_PIPE_ENTRY(cnt));
				log_ptr = tmp_log_ptr;
				goto done;
			}
			unused = 0;
			usr_buf += size;
			total_size += size;
		}
	} while (log_ptr != profile->log_head);

done:
	status = total_size;
	profile->log_tail = log_ptr;

	return status;
}

static ssize_t profile_pipe_print(struct file *filep, char __user *ubuf,
		size_t max, loff_t *ppos)
{
	struct kgsl_device *device = (struct kgsl_device *) filep->private_data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;
	char __user *usr_buf = ubuf;
	int status = 0;

	/*
	 * this file not seekable since it only supports streaming, ignore
	 * ppos <> 0
	 */
	/*
	 * format <pid>  <tid> <context id> <cnt<<16 | client type> <timestamp>
	 * for each perf counter <cntr_reg_off> <start hi & lo> <end hi & low>
	 */

	mutex_lock(&device->mutex);

	while (1) {
		/* process any results that are available into the log_buffer */
		status = adreno_profile_process_results(adreno_dev);
		if (status > 0) {
			/* if we have results, print them and exit */
			status = _pipe_print_results(adreno_dev, usr_buf, max);
			break;
		}

		/* there are no unread results, act accordingly */
		if (filep->f_flags & O_NONBLOCK) {
			if (profile->shared_tail != profile->shared_head) {
				status = _pipe_print_pending(usr_buf, max);
				break;
			} else {
				status = 0;
				break;
			}
		}

		mutex_unlock(&device->mutex);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(100));
		mutex_lock(&device->mutex);

		if (signal_pending(current)) {
			status = 0;
			break;
		}
	}

	mutex_unlock(&device->mutex);

	return status;
}

static int profile_groups_print(struct seq_file *s, void *unused)
{
	struct kgsl_device *device = (struct kgsl_device *) s->private;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	struct adreno_perfcounters *counters = gpudev->perfcounters;
	struct adreno_perfcount_group *group;
	int i, j, used;

	mutex_lock(&device->mutex);

	for (i = 0; i < counters->group_count; ++i) {
		group = &(counters->groups[i]);
		/* get number of counters used for this group */
		used = 0;
		for (j = 0; j < group->reg_count; j++) {
			if (group->regs[j].countable !=
					KGSL_PERFCOUNTER_NOT_USED)
				used++;
		}

		seq_printf(s, "%s %d %d\n", group->name,
			group->reg_count, used);
	}

	mutex_unlock(&device->mutex);

	return 0;
}

static int profile_groups_open(struct inode *inode, struct file *file)
{
	return single_open(file, profile_groups_print, inode->i_private);
}

static const struct file_operations profile_groups_fops = {
	.owner = THIS_MODULE,
	.open = profile_groups_open,
	.read = seq_read,
	.llseek = noop_llseek,
	.release = single_release,
};

static const struct file_operations profile_pipe_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = profile_pipe_print,
	.llseek = noop_llseek,
};

static const struct file_operations profile_assignments_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = profile_assignments_read,
	.write = profile_assignments_write,
	.llseek = noop_llseek,
};

DEFINE_SIMPLE_ATTRIBUTE(profile_enable_fops,
			profile_enable_get,
			profile_enable_set, "%llu\n");

void adreno_profile_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_profile *profile = &adreno_dev->profile;
	struct dentry *profile_dir;
	int ret;

	profile->enabled = false;

	/* allocate shared_buffer, which includes pre_ib and post_ib */
	profile->shared_size = ADRENO_PROFILE_SHARED_BUF_SIZE_DWORDS;
	ret = kgsl_allocate_global(device, &profile->shared_buffer,
			profile->shared_size * sizeof(unsigned int),
			0, 0, "profile");

	if (ret) {
		profile->shared_size = 0;
		return;
	}

	INIT_LIST_HEAD(&profile->assignments_list);

	/* Create perf counter debugfs */
	profile_dir = debugfs_create_dir("profiling", device->d_debugfs);
	if (IS_ERR(profile_dir))
		return;

	debugfs_create_file("enable",  0644, profile_dir, device,
			&profile_enable_fops);
	debugfs_create_file("blocks", 0444, profile_dir, device,
			&profile_groups_fops);
	debugfs_create_file("pipe", 0444, profile_dir, device,
			&profile_pipe_fops);
	debugfs_create_file("assignments", 0644, profile_dir, device,
			&profile_assignments_fops);
}

void adreno_profile_close(struct adreno_device *adreno_dev)
{
	struct adreno_profile *profile = &adreno_dev->profile;
	struct adreno_profile_assigns_list *entry, *tmp;

	profile->enabled = false;
	vfree(profile->log_buffer);
	profile->log_buffer = NULL;
	profile->log_head = NULL;
	profile->log_tail = NULL;
	profile->shared_head = 0;
	profile->shared_tail = 0;
	kgsl_free_global(KGSL_DEVICE(adreno_dev), &profile->shared_buffer);
	profile->shared_size = 0;

	profile->assignment_count = 0;

	list_for_each_entry_safe(entry, tmp, &profile->assignments_list, list) {
		list_del(&entry->list);
		kfree(entry);
	}
}

int adreno_profile_process_results(struct adreno_device *adreno_dev)
{
	struct adreno_profile *profile = &adreno_dev->profile;
	unsigned int shared_buf_tail = profile->shared_tail;

	if (!results_available(adreno_dev, profile, &shared_buf_tail))
		return 0;

	/*
	 * transfer retired results to log_buffer
	 * update shared_buffer tail ptr
	 */
	transfer_results(profile, shared_buf_tail);

	return 1;
}

void adreno_profile_preib_processing(struct adreno_device *adreno_dev,
		struct adreno_context *drawctxt, unsigned int *cmd_flags,
		unsigned int **rbptr)
{
	struct adreno_profile *profile = &adreno_dev->profile;
	int count = profile->assignment_count;
	unsigned int entry_head = profile->shared_head;
	unsigned int *shared_ptr;
	struct adreno_ringbuffer *rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);
	unsigned int rbcmds[4];
	unsigned int *ptr = *rbptr;
	unsigned int i, ret = 0;

	*cmd_flags &= ~KGSL_CMD_FLAGS_PROFILE;

	if (!adreno_profile_assignments_ready(profile))
		goto done;

	/*
	 * check if space available, include the post_ib in space available
	 * check so don't have to handle trying to undo the pre_ib insertion in
	 * ringbuffer in the case where only the post_ib fails enough space
	 */
	if (SIZE_SHARED_ENTRY(count) >= shared_buf_available(profile))
		goto done;

	if (entry_head + SIZE_SHARED_ENTRY(count) >= profile->shared_size) {
		/* entry_head would wrap, start entry_head at 0 in buffer */
		entry_head = 0;
		profile->shared_size = profile->shared_head;
		profile->shared_head = 0;

		/* recheck space available */
		if (SIZE_SHARED_ENTRY(count) >= shared_buf_available(profile))
			goto done;
	}

	/* zero out the counter area of shared_buffer entry_head */
	shared_ptr = entry_head + ((unsigned int *)
			profile->shared_buffer.hostptr);
	memset(shared_ptr, 0, SIZE_SHARED_ENTRY(count) * sizeof(unsigned int));

	/* reserve space for the pre ib shared buffer */
	shared_buf_inc(profile->shared_size, &profile->shared_head,
			SIZE_SHARED_ENTRY(count));

	/* create the shared ibdesc */
	ret = _build_pre_ib_cmds(adreno_dev, profile, rbcmds, entry_head,
			rb->timestamp + 1, drawctxt);

	/* set flag to sync with post ib commands */
	*cmd_flags |= KGSL_CMD_FLAGS_PROFILE;

done:
	/* write the ibdesc to the ringbuffer */
	for (i = 0; i < ret; i++)
		*ptr++ = rbcmds[i];

	*rbptr = ptr;
}

void adreno_profile_postib_processing(struct adreno_device *adreno_dev,
		unsigned int *cmd_flags, unsigned int **rbptr)
{
	struct adreno_profile *profile = &adreno_dev->profile;
	int count = profile->assignment_count;
	unsigned int entry_head = profile->shared_head -
		SIZE_SHARED_ENTRY(count);
	unsigned int *ptr = *rbptr;
	unsigned int rbcmds[4];
	int ret = 0, i;

	if (!adreno_profile_assignments_ready(profile))
		goto done;

	if (!(*cmd_flags & KGSL_CMD_FLAGS_PROFILE))
		goto done;

	/* create the shared ibdesc */
	ret = _build_post_ib_cmds(adreno_dev, profile, rbcmds, entry_head);

done:
	/* write the ibdesc to the ringbuffer */
	for (i = 0; i < ret; i++)
		*ptr++ = rbcmds[i];

	*rbptr = ptr;

	/* reset the sync flag */
	*cmd_flags &= ~KGSL_CMD_FLAGS_PROFILE;
}

