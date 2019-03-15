/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define TMEM_PROFILE_FMT
#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/slab.h>

#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_device.h"
#include "private/tmem_utils.h"
#include "private/tmem_priv.h"

struct profile_entry_string {
	int idx;
	const char *str;
};

#define STR(a) #a
struct profile_entry_string profile_entry_str[] = {
	{PROFILE_ENTRY_SSMR_GET, STR(SSMR_GET)},
	{PROFILE_ENTRY_SSMR_PUT, STR(SSMR_PUT)},
	{PROFILE_ENTRY_CHUNK_ALLOC, STR(CHUNK_ALLOC)},
	{PROFILE_ENTRY_CHUNK_FREE, STR(CHUNK_FREE)},
	{PROFILE_ENTRY_MEM_ADD, STR(MEM_ADD)},
	{PROFILE_ENTRY_MEM_REMOVE, STR(MEM_REMOVE)},
	{PROFILE_ENTRY_SESSION_OPEN, STR(SESSION_OPEN)},
	{PROFILE_ENTRY_SESSION_CLOSE, STR(SESSION_CLOSE)},
	{PROFILE_ENTRY_INVOKE_COMMAND, STR(INVOKE_COMMAND)},
};

#define GET_START_TIME() do_gettimeofday(&start_time)
#define GET_END_TIME() do_gettimeofday(&end_time)

#define SEC_TO_US(s) (s*1000000)

#include <asm/div64.h>
static inline u64 u64_div(u64 n, u64 base)
{
	do_div(n, base);
	return n;
}
static inline u64 us_to_ms(u64 us)
{
	return u64_div(us, 1000);
}

void trusted_mem_core_profile_dump(struct trusted_mem_device *mem_device)
{
	int idx;
	u64 average_one_time_us;
	u64 average_one_time_ms;

	struct profile_data_context *data = &mem_device->profile_mgr->data;

	pr_info("=================================================\n");
	pr_info("[%d] Profiling Summary:\n", mem_device->mem_type);

	for (idx = 0; idx < PROFILE_ENTRY_MAX; idx++) {
		average_one_time_us = SEC_TO_US(data->item[idx].sec);
		average_one_time_us += data->item[idx].usec;
		average_one_time_us =
			u64_div(average_one_time_us, data->item[idx].count);
		average_one_time_ms = us_to_ms(average_one_time_us);

		pr_info("[%d] Entry: %s\n", mem_device->mem_type,
			profile_entry_str[idx].str);
		pr_info("[%d]   invoke count: %lld\n", mem_device->mem_type,
			data->item[idx].count);
		pr_info("[%d]   spend time: %lld.%06lld sec\n",
			mem_device->mem_type, data->item[idx].sec,
			data->item[idx].usec);
		pr_info("[%d]   average one time: %lld msec (%06lld usec)\n",
			mem_device->mem_type, average_one_time_ms,
			average_one_time_us);
	}

	pr_info("=================================================\n");
}

static void increase_enter_count(enum PROFILE_ENTRY_TYPE entry,
				 struct profile_data_context *data)
{
	mutex_lock(&data->item[entry].lock);
	data->item[entry].count++;
	mutex_unlock(&data->item[entry].lock);
}

static void add_exec_time(enum PROFILE_ENTRY_TYPE entry,
			  struct profile_data_context *data,
			  struct timeval *start, struct timeval *end)
{
	int time_diff_sec = GET_TIME_DIFF_SEC_P(start, end);
	int time_diff_usec = GET_TIME_DIFF_USEC_P(start, end);

	mutex_lock(&data->item[entry].lock);

	data->item[entry].sec += time_diff_sec;
	data->item[entry].usec += time_diff_usec;
	if (data->item[entry].usec > 1000000) {
		data->item[entry].sec += 1;
		data->item[entry].usec -= 1000000;
	}

	mutex_unlock(&data->item[entry].lock);
}

static int profile_ssmr_get(u64 *pa, u32 *size, u32 feat, void *dev_desc)
{
	int ret;
	struct timeval start_time, end_time;
	struct profile_mgr_desc *prof_mgr = (struct profile_mgr_desc *)dev_desc;

	increase_enter_count(PROFILE_ENTRY_SSMR_GET, &prof_mgr->data);
	GET_START_TIME();

	ret = prof_mgr->profiled_ssmr_ops->offline(pa, size, feat,
						   prof_mgr->profiled_dev_desc);

	GET_END_TIME();
	add_exec_time(PROFILE_ENTRY_SSMR_GET, &prof_mgr->data, &start_time,
		      &end_time);
	return ret;
}

static int profile_ssmr_put(u32 feat, void *dev_desc)
{
	int ret;
	struct timeval start_time, end_time;
	struct profile_mgr_desc *prof_mgr = (struct profile_mgr_desc *)dev_desc;

	increase_enter_count(PROFILE_ENTRY_SSMR_PUT, &prof_mgr->data);
	GET_START_TIME();

	ret = prof_mgr->profiled_ssmr_ops->online(feat,
						  prof_mgr->profiled_dev_desc);

	GET_END_TIME();
	add_exec_time(PROFILE_ENTRY_SSMR_PUT, &prof_mgr->data, &start_time,
		      &end_time);
	return ret;
}

static int profile_chunk_alloc(u32 alignment, u32 size, u32 *refcount,
			       u32 *sec_handle, u8 *owner, u32 id, u32 clean,
			       void *peer_data, void *dev_desc)
{
	int ret;
	struct timeval start_time, end_time;
	struct profile_mgr_desc *prof_mgr = (struct profile_mgr_desc *)dev_desc;

	increase_enter_count(PROFILE_ENTRY_CHUNK_ALLOC, &prof_mgr->data);
	GET_START_TIME();

	ret = prof_mgr->profiled_peer_ops->memory_alloc(
		alignment, size, refcount, sec_handle, owner, id, clean,
		peer_data, prof_mgr->profiled_dev_desc);
	GET_END_TIME();
	add_exec_time(PROFILE_ENTRY_CHUNK_ALLOC, &prof_mgr->data, &start_time,
		      &end_time);

	return ret;
}

static int profile_chunk_free(u32 sec_handle, u8 *owner, u32 id,
			      void *peer_data, void *dev_desc)
{
	int ret;
	struct timeval start_time, end_time;
	struct profile_mgr_desc *prof_mgr = (struct profile_mgr_desc *)dev_desc;

	increase_enter_count(PROFILE_ENTRY_CHUNK_FREE, &prof_mgr->data);
	GET_START_TIME();

	ret = prof_mgr->profiled_peer_ops->memory_free(
		sec_handle, owner, id, peer_data, prof_mgr->profiled_dev_desc);

	GET_END_TIME();
	add_exec_time(PROFILE_ENTRY_CHUNK_FREE, &prof_mgr->data, &start_time,
		      &end_time);
	return ret;
}

static int profile_mem_add(u64 pa, u32 size, void *peer_data, void *dev_desc)
{
	int ret;
	struct timeval start_time, end_time;
	struct profile_mgr_desc *prof_mgr = (struct profile_mgr_desc *)dev_desc;

	increase_enter_count(PROFILE_ENTRY_MEM_ADD, &prof_mgr->data);
	GET_START_TIME();

	ret = prof_mgr->profiled_peer_ops->memory_grant(
		pa, size, peer_data, prof_mgr->profiled_dev_desc);

	GET_END_TIME();
	add_exec_time(PROFILE_ENTRY_MEM_ADD, &prof_mgr->data, &start_time,
		      &end_time);
	return ret;
}

static int profile_mem_remove(void *peer_data, void *dev_desc)
{
	int ret;
	struct timeval start_time, end_time;
	struct profile_mgr_desc *prof_mgr = (struct profile_mgr_desc *)dev_desc;

	increase_enter_count(PROFILE_ENTRY_MEM_REMOVE, &prof_mgr->data);
	GET_START_TIME();

	ret = prof_mgr->profiled_peer_ops->memory_reclaim(
		peer_data, prof_mgr->profiled_dev_desc);

	GET_END_TIME();
	add_exec_time(PROFILE_ENTRY_MEM_REMOVE, &prof_mgr->data, &start_time,
		      &end_time);
	return ret;
}

static int profile_session_open(void **peer_data, void *dev_desc)
{
	int ret;
	struct timeval start_time, end_time;
	struct profile_mgr_desc *prof_mgr = (struct profile_mgr_desc *)dev_desc;

	increase_enter_count(PROFILE_ENTRY_SESSION_OPEN, &prof_mgr->data);
	GET_START_TIME();

	ret = prof_mgr->profiled_peer_ops->session_open(
		peer_data, prof_mgr->profiled_dev_desc);

	GET_END_TIME();
	add_exec_time(PROFILE_ENTRY_SESSION_OPEN, &prof_mgr->data, &start_time,
		      &end_time);
	return ret;
}

static int profile_session_close(void *peer_data, void *dev_desc)
{
	int ret;
	struct timeval start_time, end_time;
	struct profile_mgr_desc *prof_mgr = (struct profile_mgr_desc *)dev_desc;

	increase_enter_count(PROFILE_ENTRY_SESSION_CLOSE, &prof_mgr->data);
	GET_START_TIME();

	ret = prof_mgr->profiled_peer_ops->session_close(
		peer_data, prof_mgr->profiled_dev_desc);

	GET_END_TIME();
	add_exec_time(PROFILE_ENTRY_SESSION_CLOSE, &prof_mgr->data, &start_time,
		      &end_time);
	return ret;
}

static int
profile_invoke_command(struct trusted_driver_cmd_params *invoke_params,
		       void *peer_data, void *dev_desc)
{
	int ret;
	struct timeval start_time, end_time;
	struct profile_mgr_desc *prof_mgr = (struct profile_mgr_desc *)dev_desc;

	increase_enter_count(PROFILE_ENTRY_INVOKE_COMMAND, &prof_mgr->data);
	GET_START_TIME();

	ret = prof_mgr->profiled_peer_ops->invoke_cmd(
		invoke_params, peer_data, prof_mgr->profiled_dev_desc);

	GET_END_TIME();
	add_exec_time(PROFILE_ENTRY_INVOKE_COMMAND, &prof_mgr->data,
		      &start_time, &end_time);
	return ret;
}

static struct trusted_driver_operations profiler_peer_ops = {
	.session_open = profile_session_open,
	.session_close = profile_session_close,
	.memory_alloc = profile_chunk_alloc,
	.memory_free = profile_chunk_free,
	.memory_grant = profile_mem_add,
	.memory_reclaim = profile_mem_remove,
	.invoke_cmd = profile_invoke_command,
};

static struct ssmr_operations profiler_ssmr_ops = {
	.offline = profile_ssmr_get,
	.online = profile_ssmr_put,
};

struct profile_mgr_desc *create_profile_mgr_desc(void)
{
	int idx;
	struct profile_mgr_desc *t_profile_desc;

	pr_info("TMEM_PROFILE_ENABLED\n");

	t_profile_desc =
		mld_kmalloc(sizeof(struct profile_mgr_desc), GFP_KERNEL);
	if (INVALID(t_profile_desc)) {
		pr_err("%s:%d out of memory!\n", __func__, __LINE__);
		return NULL;
	}

	for (idx = 0; idx < PROFILE_ENTRY_MAX; idx++) {
		mutex_init(&t_profile_desc->data.item[idx].lock);
		t_profile_desc->data.item[idx].count = 0;
		t_profile_desc->data.item[idx].sec = 0;
		t_profile_desc->data.item[idx].usec = 0;
	}

	t_profile_desc->profiled_peer_ops = &profiler_peer_ops;
	t_profile_desc->profiled_ssmr_ops = &profiler_ssmr_ops;
	t_profile_desc->profiled_dev_desc = t_profile_desc;

	return t_profile_desc;
}
