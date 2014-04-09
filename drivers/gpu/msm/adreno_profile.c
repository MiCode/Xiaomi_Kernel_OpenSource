/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#define SIZE_DATA(cnt) (3 + (cnt) * 5)

/*
 * Pre-IB command size (in dwords):
 *        : 2 - NOP start identifier
 *        : 3 - timestamp
 *        : 3 - count
 *        : 3 - context id
 * [loop count start] - for each counter to watch
 *        : 3 - Register offset
 *        : 3 - Register read lo
 *        : 3 - Register read high
 * [loop end]
 *        : 2 - NOP end identifier
 */
#define SIZE_PREIB(cnt) (13 + (cnt) * 9)

/*
 * Post-IB command size (in dwords):
 *        : 2 - NOP start identifier
 * [loop count start] - for each counter to watch
 *        : 3 - Register read lo
 *        : 3 - Register read high
 * [loop end]
 *        : 2 - NOP end identifier
 */
#define SIZE_POSTIB(cnt) (4 + (cnt) * 6)

/* Counter data + Pre size + post size = total size */
#define SIZE_SHARED_ENTRY(cnt) (SIZE_DATA(cnt) + SIZE_PREIB(cnt) \
		+ SIZE_POSTIB(cnt))

/*
 * Space for following string :"%u %u %u %.5s %u "
 * [count iterations]: "%.8s:%u %llu %llu%c"
 */
#define SIZE_PIPE_ENTRY(cnt) (50 + (cnt) * 62)
#define SIZE_LOG_ENTRY(cnt) (5 + (cnt) * 5)

static struct adreno_context_type ctxt_type_table[] = {ADRENO_DRAWCTXT_TYPES};

static const char *get_api_type_str(unsigned int type)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(ctxt_type_table) - 1; i++) {
		if (ctxt_type_table[i].type == type)
			break;
	}
	return ctxt_type_table[i].str;
}

static inline void _create_ib_ref(struct kgsl_memdesc *memdesc,
		unsigned int *cmd, unsigned int cnt, unsigned int off)
{
	cmd[0] = CP_HDR_INDIRECT_BUFFER_PFD;
	cmd[1] = memdesc->gpuaddr + off;
	cmd[2] = cnt;
}

#define IB_START(cmd) do { \
		*cmd++ = cp_nop_packet(1); \
		*cmd++ = KGSL_START_OF_PROFILE_IDENTIFIER; \
	} while (0);

#define IB_END(cmd) do { \
		*cmd++ = cp_nop_packet(1); \
		*cmd++ = KGSL_END_OF_PROFILE_IDENTIFIER; \
	} while (0);

#define IB_CMD(cmd, type, val1, val2, off) do { \
		*cmd++ = cp_type3_packet(type, 2); \
		*cmd++ = val1; \
		*cmd++ = val2; \
		off += sizeof(unsigned int); \
	} while (0);

static void _build_pre_ib_cmds(struct adreno_profile *profile,
		unsigned int *rbcmds, unsigned int head,
		unsigned int timestamp, unsigned int ctxt_id)
{
	struct adreno_profile_assigns_list *entry;
	unsigned int *start, *ibcmds;
	unsigned int count = profile->assignment_count;
	unsigned int gpuaddr = profile->shared_buffer.gpuaddr;
	unsigned int ib_offset = head + SIZE_DATA(count);
	unsigned int data_offset = head * sizeof(unsigned int);

	ibcmds = ib_offset + ((unsigned int *) profile->shared_buffer.hostptr);
	start = ibcmds;

	/* start of profile identifier */
	IB_START(ibcmds);

	/* timestamp */
	IB_CMD(ibcmds, CP_MEM_WRITE, gpuaddr + data_offset,
			timestamp, data_offset);

	/* count:  number of perf counters pairs GPU will write */
	IB_CMD(ibcmds, CP_MEM_WRITE, gpuaddr + data_offset,
			profile->assignment_count, data_offset);

	/* context id */
	IB_CMD(ibcmds, CP_MEM_WRITE, gpuaddr + data_offset,
			ctxt_id, data_offset);

	/* loop for each countable assigned */
	list_for_each_entry(entry, &profile->assignments_list, list) {
		IB_CMD(ibcmds, CP_MEM_WRITE, gpuaddr + data_offset,
				entry->offset, data_offset);
		IB_CMD(ibcmds, CP_REG_TO_MEM, entry->offset,
				gpuaddr + data_offset, data_offset);
		IB_CMD(ibcmds, CP_REG_TO_MEM, entry->offset + 1,
				gpuaddr + data_offset, data_offset);

		/* skip over post_ib counter data */
		data_offset += sizeof(unsigned int) * 2;
	}

	/* end of profile identifier */
	IB_END(ibcmds);

	_create_ib_ref(&profile->shared_buffer, rbcmds,
			ibcmds - start, ib_offset * sizeof(unsigned int));
}

static void _build_post_ib_cmds(struct adreno_profile *profile,
		unsigned int *rbcmds, unsigned int head)
{
	struct adreno_profile_assigns_list *entry;
	unsigned int *start, *ibcmds;
	unsigned int count = profile->assignment_count;
	unsigned int gpuaddr =  profile->shared_buffer.gpuaddr;
	unsigned int ib_offset = head + SIZE_DATA(count) + SIZE_PREIB(count);
	unsigned int data_offset = head * sizeof(unsigned int);

	ibcmds = ib_offset + ((unsigned int *) profile->shared_buffer.hostptr);
	start = ibcmds;
	/* end of profile identifier */
	IB_END(ibcmds);

	/* skip over pre_ib preamble */
	data_offset += sizeof(unsigned int) * 3;

	/* loop for each countable assigned */
	list_for_each_entry(entry, &profile->assignments_list, list) {
		/* skip over pre_ib counter data */
		data_offset += sizeof(unsigned int) * 3;

		IB_CMD(ibcmds, CP_REG_TO_MEM, entry->offset,
				gpuaddr + data_offset, data_offset);
		IB_CMD(ibcmds, CP_REG_TO_MEM, entry->offset + 1,
				gpuaddr + data_offset, data_offset);
	}

	/* end of profile identifier */
	IB_END(ibcmds);

	_create_ib_ref(&profile->shared_buffer, rbcmds,
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

static inline void log_buf_wrapcnt(unsigned int cnt, unsigned int *off)
{
	*off = (*off + cnt) % ADRENO_PROFILE_LOG_BUF_SIZE_DWORDS;
}

static inline void log_buf_wrapinc(unsigned int *profile_log_buffer,
		unsigned int **ptr)
{
	*ptr += 1;
	if (*ptr >= (profile_log_buffer +
				ADRENO_PROFILE_LOG_BUF_SIZE_DWORDS))
		*ptr -= ADRENO_PROFILE_LOG_BUF_SIZE_DWORDS;
}

static inline unsigned int log_buf_available(struct adreno_profile *profile,
		unsigned int *head_ptr)
{
	unsigned int tail, head;

	tail = (unsigned int) profile->log_tail -
		(unsigned int) profile->log_buffer;
	head = (unsigned int) head_ptr - (unsigned int) profile->log_buffer;
	if (tail > head)
		return (tail - head) / sizeof(unsigned int);
	else
		return ADRENO_PROFILE_LOG_BUF_SIZE_DWORDS - ((head - tail) /
				sizeof(unsigned int));
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
		unsigned int offset)
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

	strlcpy(entry->name, str, sizeof(entry->name));

	profile->assignment_count++;

	return true;
}

static void check_close_profile(struct adreno_profile *profile)
{
	if (profile->log_buffer == NULL)
		return;

	if (!adreno_profile_enabled(profile) && shared_buf_empty(profile)) {
		if (profile->log_head == profile->log_tail) {
			vfree(profile->log_buffer);
			profile->log_buffer = NULL;
			profile->log_head = NULL;
			profile->log_tail = NULL;
		}
	}
}

static bool results_available(struct kgsl_device *device,
		unsigned int *shared_buf_tail)
{
	unsigned int global_eop;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;
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

	global_eop = kgsl_readtimestamp(device, NULL, KGSL_TIMESTAMP_RETIRED);
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

static void transfer_results(struct kgsl_device *device,
		unsigned int shared_buf_tail)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;
	unsigned int buf_off;
	unsigned int ts, cnt, ctxt_id, pid, tid, client_type;
	unsigned int *ptr = (unsigned int *) profile->shared_buffer.hostptr;
	struct kgsl_context *k_ctxt;
	unsigned int *log_ptr, *log_base;
	struct adreno_profile_assigns_list *assigns_list;
	int i;

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
		ts = *(ptr + buf_off);
		cnt = *(ptr + buf_off + 1);
		ctxt_id = *(ptr + buf_off + 2);
		/*
		 * if entry overwrites the tail of log_buffer then adjust tail
		 * ptr to make room for the new entry, discarding old entry
		 */
		while (log_buf_available(profile, log_ptr) <=
				SIZE_LOG_ENTRY(cnt)) {
			unsigned int size_tail, boff;
			size_tail = SIZE_LOG_ENTRY(0xffff &
					*(profile->log_tail));
			boff = ((unsigned int) profile->log_tail -
				(unsigned int) log_base) / sizeof(unsigned int);
			log_buf_wrapcnt(size_tail, &boff);
			profile->log_tail = log_base + boff;
		}

		/* find Adreno ctxt struct */
		k_ctxt = idr_find(&device->context_idr, ctxt_id);
		if (k_ctxt == NULL) {
			shared_buf_inc(profile->shared_size,
					&profile->shared_tail,
					SIZE_SHARED_ENTRY(cnt));
			continue;
		} else {
			struct adreno_context *adreno_ctxt =
				ADRENO_CONTEXT(k_ctxt);
			pid = k_ctxt->pid;  /* pid */
			tid = k_ctxt->tid; /* tid creator */
			client_type =  adreno_ctxt->type << 16;
		}

		buf_off += 3;
		*log_ptr = client_type | cnt;
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
		shared_buf_inc(profile->shared_size,
				&profile->shared_tail,
				SIZE_SHARED_ENTRY(cnt));

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

	kgsl_mutex_lock(&device->mutex, &device->mutex_owner);
	*val = adreno_profile_enabled(&adreno_dev->profile);
	kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);

	return 0;
}

static int profile_enable_set(void *data, u64 val)
{
	struct kgsl_device *device = data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;

	kgsl_mutex_lock(&device->mutex, &device->mutex_owner);

	if (adreno_is_a2xx(adreno_dev)) {
		kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);
		return 0;
	}

	profile->enabled = val;

	check_close_profile(profile);

	kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);

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

	if (adreno_is_a2xx(adreno_dev))
		return -EINVAL;

	kgsl_mutex_lock(&device->mutex, &device->mutex_owner);

	buf = kmalloc(max_size, GFP_KERNEL);
	if (!buf) {
		kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);
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

	kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);
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
	unsigned int offset;
	const char *name = NULL;

	name = adreno_perfcounter_get_name(adreno_dev, groupid);
	if (!name)
		return;

	/* if already in assigned list skip it */
	if (_in_assignments_list(profile, groupid, countable))
		return;

	/* add to perf counter allocation, if fail skip it */
	if (adreno_perfcounter_get(adreno_dev, groupid,
				countable, &offset, PERFCOUNTER_FLAG_NONE))
		return;

	/* add to assignments list, put counter back if error */
	if (!_add_to_assignments_list(profile, name, groupid,
				countable, offset))
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

	if (adreno_is_a2xx(adreno_dev))
		return -ENOSPC;

	kgsl_mutex_lock(&device->mutex, &device->mutex_owner);

	if (adreno_profile_enabled(profile)) {
		size = -EINVAL;
		goto error_unlock;
	}

	ret = kgsl_active_count_get(device);
	if (ret)
		return -EINVAL;

	/*
	 * When adding/removing assignments, ensure that the GPU is done with
	 * all it's work.  This helps to syncronize the work flow to the
	 * GPU and avoid racey conditions.
	 */
	if (adreno_idle(device)) {
		size = -EINVAL;
		goto error_put;
	}

	/* clear all shared buffer results */
	adreno_profile_process_results(device);

	buf = kmalloc(len + 1, GFP_KERNEL);
	if (!buf) {
		size = -EINVAL;
		goto error_put;
	}

	pbuf = buf;

	/* clear the log buffer */
	if (profile->log_buffer != NULL) {
		profile->log_head = profile->log_buffer;
		profile->log_tail = profile->log_buffer;
	}

	if (copy_from_user(buf, user_buf, len)) {
		size = -EFAULT;
		goto error_free;
	}

	/* for sanity and parsing, ensure it is null terminated */
	buf[len] = '\0';

	/* parse file buf and add(remove) to(from) appropriate lists */
	while (1) {
		pbuf = _parse_next_assignment(adreno_dev, pbuf, &groupid,
				&countable, &remove_assignment);
		if (pbuf == NULL)
			break;

		if (remove_assignment)
			_remove_assignment(adreno_dev, groupid, countable);
		else
			_add_assignment(adreno_dev, groupid, countable);
	}

	size = len;

error_free:
	kfree(buf);
error_put:
	kgsl_active_count_put(device);
error_unlock:
	kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);
	return size;
}

static int _pipe_print_pending(char *ubuf, size_t max)
{
	loff_t unused = 0;
	char str[] = "Operation Would Block!";

	return simple_read_from_buffer(ubuf, max,
			&unused, str, strlen(str));
}

static int _pipe_print_results(struct adreno_device *adreno_dev,
		char *ubuf, size_t max)
{
	struct adreno_profile *profile = &adreno_dev->profile;
	const char *grp_name;
	char *usr_buf = ubuf;
	unsigned int *log_ptr = NULL;
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
		cnt = *log_ptr & 0xffff;
		if (SIZE_PIPE_ENTRY(cnt) > max) {
			status = 0;
			goto err;
		}
		if ((max - (usr_buf - ubuf)) < SIZE_PIPE_ENTRY(cnt))
			break;

		api_type = *log_ptr >> 16;
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
		if (size < 0) {
			status = -EINVAL;
			goto err;
		}

		unused = 0;
		usr_buf += size;
		total_size += size;

		for (i = 0; i < cnt; i++) {
			grp_name = adreno_perfcounter_get_name(
					adreno_dev, *log_ptr >> 16);
			if (grp_name == NULL) {
				status = -EFAULT;
				goto err;
			}

			if (i == cnt - 1)
				format_space = '\n';
			else
				format_space = ' ';

			cnt_reg = *log_ptr & 0xffff;
			log_buf_wrapinc(profile->log_buffer, &log_ptr);
			pc_start = *((unsigned long long *) log_ptr);
			log_buf_wrapinc(profile->log_buffer, &log_ptr);
			log_buf_wrapinc(profile->log_buffer, &log_ptr);
			pc_end = *((unsigned long long *) log_ptr);
			log_buf_wrapinc(profile->log_buffer, &log_ptr);
			log_buf_wrapinc(profile->log_buffer, &log_ptr);

			len = snprintf(pipe_cntr_buf,
					sizeof(pipe_cntr_buf) - 1,
					"%.8s:%u %llu %llu%c",
					grp_name, cnt_reg, pc_start,
					pc_end, format_space);

			size = simple_read_from_buffer(usr_buf,
					max - (usr_buf - ubuf),
					&unused, pipe_cntr_buf, len);
			if (size < 0) {
				status = size;
				goto err;
			}
			unused = 0;
			usr_buf += size;
			total_size += size;
		}
	} while (log_ptr != profile->log_head);

	status = total_size;
err:
	profile->log_tail = log_ptr;

	return status;
}

static int profile_pipe_print(struct file *filep, char __user *ubuf,
		size_t max, loff_t *ppos)
{
	struct kgsl_device *device = (struct kgsl_device *) filep->private_data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;
	char *usr_buf = ubuf;
	int status = 0;

	if (adreno_is_a2xx(adreno_dev))
		return 0;

	/*
	 * this file not seekable since it only supports streaming, ignore
	 * ppos <> 0
	 */
	/*
	 * format <pid>  <tid> <context id> <cnt<<16 | client type> <timestamp>
	 * for each perf counter <cntr_reg_off> <start hi & lo> <end hi & low>
	 */

	kgsl_mutex_lock(&device->mutex, &device->mutex_owner);

	while (1) {
		/* process any results that are available into the log_buffer */
		status = adreno_profile_process_results(device);
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

		kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ / 10);
		kgsl_mutex_lock(&device->mutex, &device->mutex_owner);

		if (signal_pending(current)) {
			status = 0;
			break;
		}
	}

	check_close_profile(profile);
	kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);

	return status;
}

static int profile_groups_print(struct seq_file *s, void *unused)
{
	struct kgsl_device *device = (struct kgsl_device *) s->private;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_perfcounters *counters = adreno_dev->gpudev->perfcounters;
	struct adreno_perfcount_group *group;
	int i, j, used;

	/* perfcounter list not allowed on a2xx */
	if (adreno_is_a2xx(adreno_dev))
		return -EINVAL;

	kgsl_mutex_lock(&device->mutex, &device->mutex_owner);

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

	kgsl_mutex_unlock(&device->mutex, &device->mutex_owner);

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

void adreno_profile_init(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;
	struct dentry *profile_dir;
	int ret;

	profile->enabled = false;

	/* allocate shared_buffer, which includes pre_ib and post_ib */
	profile->shared_size = ADRENO_PROFILE_SHARED_BUF_SIZE_DWORDS;
	ret = kgsl_allocate_contiguous(&profile->shared_buffer,
			profile->shared_size * sizeof(unsigned int));
	if (ret) {
		profile->shared_buffer.hostptr = NULL;
		profile->shared_size = 0;
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

void adreno_profile_close(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;
	struct adreno_profile_assigns_list *entry, *tmp;

	profile->enabled = false;
	vfree(profile->log_buffer);
	profile->log_buffer = NULL;
	profile->log_head = NULL;
	profile->log_tail = NULL;
	profile->shared_head = 0;
	profile->shared_tail = 0;
	kgsl_sharedmem_free(&profile->shared_buffer);
	profile->shared_buffer.hostptr = NULL;
	profile->shared_size = 0;

	profile->assignment_count = 0;

	list_for_each_entry_safe(entry, tmp, &profile->assignments_list, list) {
		list_del(&entry->list);
		kfree(entry);
	}
}

int adreno_profile_process_results(struct kgsl_device *device)
{

	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;
	unsigned int shared_buf_tail = profile->shared_tail;

	if (!results_available(device, &shared_buf_tail)) {
		check_close_profile(profile);
		return 0;
	}

	/* allocate profile_log_buffer if needed */
	if (profile->log_buffer == NULL) {
		profile->log_buffer = vmalloc(ADRENO_PROFILE_LOG_BUF_SIZE);
		if (profile->log_buffer == NULL)
			return -ENOMEM;
		profile->log_tail = profile->log_buffer;
		profile->log_head = profile->log_buffer;
	}

	/*
	 * transfer retired results to log_buffer
	 * update shared_buffer tail ptr
	 */
	transfer_results(device, shared_buf_tail);

	return 1;
}

void adreno_profile_preib_processing(struct kgsl_device *device,
		unsigned int context_id, unsigned int *cmd_flags,
		unsigned int **rbptr, unsigned int *cmds_gpu)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;
	int count = profile->assignment_count;
	unsigned int entry_head = profile->shared_head;
	unsigned int *shared_ptr;
	struct adreno_ringbuffer *rb = &adreno_dev->ringbuffer;
	unsigned int rbcmds[3] = { cp_nop_packet(2),
		KGSL_NOP_IB_IDENTIFIER, KGSL_NOP_IB_IDENTIFIER };

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
		if (profile->shared_tail == profile->shared_size)
			profile->shared_tail = 0;

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
	_build_pre_ib_cmds(profile, rbcmds, entry_head,
			rb->global_ts + 1, context_id);

	/* set flag to sync with post ib commands */
	*cmd_flags |= KGSL_CMD_FLAGS_PROFILE;

done:
	/* write the ibdesc to the ringbuffer */
	GSL_RB_WRITE(device, (*rbptr), (*cmds_gpu), rbcmds[0]);
	GSL_RB_WRITE(device, (*rbptr), (*cmds_gpu), rbcmds[1]);
	GSL_RB_WRITE(device, (*rbptr), (*cmds_gpu), rbcmds[2]);
}

void adreno_profile_postib_processing(struct kgsl_device *device,
		unsigned int *cmd_flags, unsigned int **rbptr,
		unsigned int *cmds_gpu)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct adreno_profile *profile = &adreno_dev->profile;
	int count = profile->assignment_count;
	unsigned int entry_head = profile->shared_head -
		SIZE_SHARED_ENTRY(count);
	unsigned int rbcmds[3] = { cp_nop_packet(2),
		KGSL_NOP_IB_IDENTIFIER, KGSL_NOP_IB_IDENTIFIER };

	if (!adreno_profile_assignments_ready(profile))
		goto done;

	if (!(*cmd_flags & KGSL_CMD_FLAGS_PROFILE))
		goto done;

	/* create the shared ibdesc */
	_build_post_ib_cmds(profile, rbcmds, entry_head);

done:
	/* write the ibdesc to the ringbuffer */
	GSL_RB_WRITE(device, (*rbptr), (*cmds_gpu), rbcmds[0]);
	GSL_RB_WRITE(device, (*rbptr), (*cmds_gpu), rbcmds[1]);
	GSL_RB_WRITE(device, (*rbptr), (*cmds_gpu), rbcmds[2]);

	/* reset the sync flag */
	*cmd_flags &= ~KGSL_CMD_FLAGS_PROFILE;
}

