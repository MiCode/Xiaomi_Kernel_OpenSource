// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/cdev.h>
#include <asm/page.h>
#include <linux/io.h>

#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <asm/cacheflush.h>
#include <linux/io.h>

#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/hardirq.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/debugfs.h>

#include <linux/ftrace.h>
#include <linux/trace_events.h>
#include <linux/bug.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include "mt-plat/aee.h"
#endif

#define MMPROFILE_INTERNAL
#include "mmprofile_internal.h"
#include "mmprofile_function.h"
#include "mmprofile_static_event.h"

#ifdef CONFIG_MTK_USE_RESERVED_EXT_MEM
#include <linux/exm_driver.h>
#endif

/* #pragma GCC optimize ("O0") */
#define MMP_DEVNAME "mmp"

#define MMPROFILE_DEFAULT_BUFFER_SIZE 0x18000
#ifdef CONFIG_MTK_ENG_BUILD
#define MMPROFILE_DEFAULT_META_BUFFER_SIZE 0x800000
static unsigned int mmprofile_meta_datacookie = 1;
#else
#define MMPROFILE_DEFAULT_META_BUFFER_SIZE 0x0
#endif

#define MMPROFILE_DUMP_BLOCK_SIZE (1024*4)

#define TAG_MMPROFILE "mmprofile"

#ifdef CONFIG_TRACING

#define ENABLE_MMP_TRACING
#ifdef ENABLE_MMP_TRACING
#define MMP_TRACING
#endif

#endif /* CONFIG_TRACING */

static bool mmp_log_on;
static bool mmp_trace_log_on;

#ifndef CONFIG_MTK_AEE_FEATURE
# undef aee_kernel_warning_api
# define aee_kernel_warning_api(...)
# undef aee_kernel_exception
# define aee_kernel_exception(...)
#endif

#define MMP_LOG(prio, fmt, arg...) \
	do { \
		if (mmp_log_on) \
			pr_debug("MMP:%s(): "fmt"\n", __func__, ##arg); \
	} while (0)

#define MMP_MSG(fmt, arg...) pr_info("MMP: %s(): "fmt"\n", __func__, ##arg)

#define mmp_aee(string, args...) do {	\
	char disp_name[100];						\
	snprintf(disp_name, 100, "[MMP]"string, ##args); \
	aee_kernel_warning_api(__FILE__, __LINE__, \
		DB_OPT_DEFAULT | DB_OPT_MMPROFILE_BUFFER | \
		DB_OPT_DISPLAY_HANG_DUMP | DB_OPT_DUMP_DISPLAY, \
		disp_name, "[MMP] error"string, ##args);		\
	pr_info("MMP error: "string, ##args);				\
} while (0)
struct mmprofile_regtable_t {
	struct mmprofile_eventinfo_t event_info;
	struct list_head list;
};

struct mmprofile_meta_datablock_t {
	struct list_head list;
	unsigned int block_size;
	unsigned int cookie;
	enum mmp_metadata_type data_type;
	unsigned int data_size;
	unsigned char meta_data[1];
};

static int bmmprofile_init_buffer;
static DEFINE_MUTEX(mmprofile_buffer_init_mutex);
static DEFINE_MUTEX(mmprofile_regtable_mutex);
#ifdef CONFIG_MTK_ENG_BUILD
static DEFINE_MUTEX(mmprofile_meta_buffer_mutex);
#endif
static struct mmprofile_event_t *p_mmprofile_ring_buffer;
#ifdef CONFIG_MTK_ENG_BUILD
static unsigned char *p_mmprofile_meta_buffer;
#endif

static struct mmprofile_global_t mmprofile_globals
__aligned(PAGE_SIZE) = {
	.buffer_size_record = MMPROFILE_DEFAULT_BUFFER_SIZE,
	.new_buffer_size_record = MMPROFILE_DEFAULT_BUFFER_SIZE,
	.buffer_size_bytes =
		((sizeof(struct mmprofile_event_t) *
		MMPROFILE_DEFAULT_BUFFER_SIZE +
	    (PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1))),
	.record_size = sizeof(struct mmprofile_event_t),
	.meta_buffer_size = MMPROFILE_DEFAULT_META_BUFFER_SIZE,
	.new_meta_buffer_size = MMPROFILE_DEFAULT_META_BUFFER_SIZE,
	.selected_buffer = MMPROFILE_PRIMARY_BUFFER,
	.reg_event_index =
		sizeof(mmprofile_static_events) /
			sizeof(struct mmp_static_event_t),
	.max_event_count = MMPROFILE_MAX_EVENT_COUNT,
};

static struct mmprofile_regtable_t mmprofile_regtable = {
	.list = LIST_HEAD_INIT(mmprofile_regtable.list),
};

static struct list_head mmprofile_meta_buffer_list =
	LIST_HEAD_INIT(mmprofile_meta_buffer_list);
static unsigned char mmprofile_dump_block[MMPROFILE_DUMP_BLOCK_SIZE];

/* Internal functions begin */
static int mmprofile_register_static_events(int sync);

static void mmprofile_force_start(int start);

unsigned int mmprofile_get_dump_size(void)
{
	unsigned int size;

	MMP_LOG(ANDROID_LOG_DEBUG, "+enable %u, start %u",
		mmprofile_globals.enable, mmprofile_globals.start);
	mmprofile_force_start(0);
	if (mmprofile_register_static_events(0) == 0)
		return 0;
	size = sizeof(struct mmprofile_global_t);
	size += sizeof(struct mmprofile_eventinfo_t) *
		(mmprofile_globals.reg_event_index + 1);
	size += mmprofile_globals.buffer_size_bytes;
	MMP_LOG(ANDROID_LOG_DEBUG, "-size %u", size);
	return size;
}

static unsigned int mmprofile_fill_dump_block(void *p_src, void *p_dst,
	unsigned int *p_src_pos, unsigned int *p_dst_pos,
	unsigned int src_size, unsigned int dst_size)
{
	unsigned int src_left = src_size - *p_src_pos;
	unsigned int dst_left = dst_size - *p_dst_pos;

	if ((src_left == 0) || (dst_left == 0))
		return 0;
	if (src_left < dst_left) {
		memcpy(((unsigned char *)p_dst) + *p_dst_pos,
			((unsigned char *)p_src) + *p_src_pos, src_left);
		*p_src_pos += src_left;
		*p_dst_pos += src_left;
		return src_left;
	}

	memcpy(((unsigned char *)p_dst) + *p_dst_pos,
		((unsigned char *)p_src) + *p_src_pos, dst_left);
	*p_src_pos += dst_left;
	*p_dst_pos += dst_left;
	return dst_left;
}

void mmprofile_get_dump_buffer(unsigned int start, unsigned long *p_addr,
	unsigned int *p_size)
{
	unsigned int total_pos = start;
	unsigned int region_pos;
	unsigned int block_pos = 0;
	unsigned int region_base = 0;
	unsigned int copy_size;
	*p_addr = (unsigned long)mmprofile_dump_block;
	*p_size = MMPROFILE_DUMP_BLOCK_SIZE;
	if (!bmmprofile_init_buffer) {
		MMP_LOG(ANDROID_LOG_DEBUG, "Ringbuffer is not initialized");
		*p_size = 0;
		return;
	}
	if (total_pos < (region_base + sizeof(struct mmprofile_global_t))) {
		/* Global structure */
		region_pos = total_pos;
		copy_size =
			mmprofile_fill_dump_block(&mmprofile_globals,
			mmprofile_dump_block,
			&region_pos, &block_pos,
			sizeof(struct mmprofile_global_t),
			MMPROFILE_DUMP_BLOCK_SIZE);
		if (block_pos == MMPROFILE_DUMP_BLOCK_SIZE)
			return;
		total_pos = region_base + sizeof(struct mmprofile_global_t);
	}
	region_base += sizeof(struct mmprofile_global_t);
	if (mmprofile_register_static_events(0) == 0) {
		MMP_LOG(ANDROID_LOG_DEBUG, "static event not register");
		*p_size = 0;
		return;
	}
	if (total_pos <
	    (region_base + sizeof(struct mmprofile_eventinfo_t) *
	    (mmprofile_globals.reg_event_index + 1))) {
		/* Register table */
		mmp_event index;
		struct mmprofile_regtable_t *p_regtable;
		struct mmprofile_eventinfo_t event_info_dummy = { 0, "" };
		unsigned int src_pos;
		unsigned int pos = 0;

		region_pos = total_pos - region_base;
		if (mutex_trylock(&mmprofile_regtable_mutex) == 0) {
			MMP_LOG(ANDROID_LOG_DEBUG, "fail to get reg lock");
			*p_size = 0;
			return;
		}
		if (pos + sizeof(struct mmprofile_eventinfo_t) > region_pos) {
			if (region_pos > pos)
				src_pos = region_pos - pos;
			else
				src_pos = 0;
			copy_size =
			    mmprofile_fill_dump_block(&event_info_dummy,
			    mmprofile_dump_block, &src_pos,
				&block_pos,
				sizeof(struct mmprofile_eventinfo_t),
				MMPROFILE_DUMP_BLOCK_SIZE);
			if (block_pos == MMPROFILE_DUMP_BLOCK_SIZE) {
				mutex_unlock(&mmprofile_regtable_mutex);
				return;
			}
		}
		pos += sizeof(struct mmprofile_eventinfo_t);
		index = MMP_ROOT_EVENT;
		list_for_each_entry(p_regtable,
			&(mmprofile_regtable.list), list) {
			if (pos + sizeof(struct mmprofile_eventinfo_t) >
				region_pos) {

				if (region_pos > pos)
					src_pos = region_pos - pos;
				else
					src_pos = 0;
				if (!virt_addr_valid(
					&(p_regtable->event_info))) {
					mmp_aee("pos=0x%x, src_pos=0x%x\n",
						pos, src_pos);
					pr_info("region_pos=0x%x, block_pos=0x%x\n",
						region_pos, block_pos);
					return;
				}
				copy_size =
				    mmprofile_fill_dump_block(
				    &(p_regtable->event_info),
					mmprofile_dump_block,
					&src_pos, &block_pos,
					sizeof(struct mmprofile_eventinfo_t),
					MMPROFILE_DUMP_BLOCK_SIZE);
				if (block_pos == MMPROFILE_DUMP_BLOCK_SIZE) {
					mutex_unlock(&mmprofile_regtable_mutex);
					return;
				}
			}
			pos += sizeof(struct mmprofile_eventinfo_t);
			index++;
		}
		mutex_unlock(&mmprofile_regtable_mutex);
		total_pos =
		    region_base + sizeof(struct mmprofile_eventinfo_t) *
		    (mmprofile_globals.reg_event_index + 1);
	}
	region_base += sizeof(struct mmprofile_eventinfo_t) *
		(mmprofile_globals.reg_event_index + 1);
	if (total_pos < (region_base + mmprofile_globals.buffer_size_bytes)) {
		/* Primary buffer */
		region_pos = total_pos - region_base;
		copy_size =
		    mmprofile_fill_dump_block(p_mmprofile_ring_buffer,
			    mmprofile_dump_block, &region_pos, &block_pos,
			    mmprofile_globals.buffer_size_bytes,
				MMPROFILE_DUMP_BLOCK_SIZE);
		if (block_pos == MMPROFILE_DUMP_BLOCK_SIZE)
			return;
	} else {
		*p_size = 0;
	}
	MMP_LOG(ANDROID_LOG_DEBUG, "end t=%u,r =%u,block_pos=%u",
		total_pos, region_base,	block_pos);
	*p_size = block_pos;
}

static void mmprofile_init_buffer(void)
{
	unsigned int b_reset_ring_buffer = 0;
#ifdef CONFIG_MTK_ENG_BUILD
	unsigned int b_reset_meta_buffer = 0;
#endif

	if (!mmprofile_globals.enable)
		return;
	if (in_interrupt())
		return;
	mutex_lock(&mmprofile_buffer_init_mutex);
	if (bmmprofile_init_buffer &&
		(mmprofile_globals.buffer_size_record ==
			mmprofile_globals.new_buffer_size_record) &&
	    (mmprofile_globals.meta_buffer_size ==
			mmprofile_globals.new_meta_buffer_size)) {
		mutex_unlock(&mmprofile_buffer_init_mutex);
		return;
	}
	bmmprofile_init_buffer = 0;

	/* Initialize */
	/* Allocate memory. */

	if (!p_mmprofile_ring_buffer) {
		mmprofile_globals.buffer_size_record =
		    mmprofile_globals.new_buffer_size_record;
		mmprofile_globals.buffer_size_bytes =
		    ((sizeof(struct mmprofile_event_t) *
				mmprofile_globals.buffer_size_record +
				(PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1)));
		b_reset_ring_buffer = 1;
	} else if (mmprofile_globals.buffer_size_record !=
		   mmprofile_globals.new_buffer_size_record) {
		vfree(p_mmprofile_ring_buffer);
		p_mmprofile_ring_buffer = NULL;
		mmprofile_globals.buffer_size_record =
		    mmprofile_globals.new_buffer_size_record;
		mmprofile_globals.buffer_size_bytes =
		    ((sizeof(struct mmprofile_event_t) *
				mmprofile_globals.buffer_size_record +
				(PAGE_SIZE - 1)) & (~(PAGE_SIZE - 1)));
		b_reset_ring_buffer = 1;
	}
	if (b_reset_ring_buffer) {
		p_mmprofile_ring_buffer =
#ifdef CONFIG_MTK_USE_RESERVED_EXT_MEM
		    (struct mmprofile_event_t *)
		    extmem_malloc_page_align(
				mmprofile_globals.buffer_size_bytes);
#else
		    vmalloc(mmprofile_globals.buffer_size_bytes);
#endif
	}
	MMP_LOG(ANDROID_LOG_DEBUG, "p_mmprofile_ring_buffer=0x%08lx",
		(unsigned long)p_mmprofile_ring_buffer);

#ifdef CONFIG_MTK_ENG_BUILD
	if (!p_mmprofile_meta_buffer) {
		mmprofile_globals.meta_buffer_size =
			mmprofile_globals.new_meta_buffer_size;
		b_reset_meta_buffer = 1;
	} else if (mmprofile_globals.meta_buffer_size !=
		   mmprofile_globals.new_meta_buffer_size) {
		vfree(p_mmprofile_meta_buffer);
		p_mmprofile_meta_buffer = NULL;
		mmprofile_globals.meta_buffer_size =
			mmprofile_globals.new_meta_buffer_size;
		b_reset_meta_buffer = 1;
	}
	if (b_reset_meta_buffer) {
		p_mmprofile_meta_buffer =
#ifdef CONFIG_MTK_USE_RESERVED_EXT_MEM
	    (unsigned char *)
	    extmem_malloc_page_align(
			mmprofile_globals.meta_buffer_size);
#else
	    vmalloc(mmprofile_globals.meta_buffer_size);
#endif
	}

	MMP_LOG(ANDROID_LOG_DEBUG, "p_mmprofile_meta_buffer=0x%08lx",
		(unsigned long)p_mmprofile_meta_buffer);

	if ((!p_mmprofile_ring_buffer) || (!p_mmprofile_meta_buffer)) {
		if (p_mmprofile_ring_buffer) {
			vfree(p_mmprofile_ring_buffer);
			p_mmprofile_ring_buffer = NULL;
		}
		if (p_mmprofile_meta_buffer) {
			vfree(p_mmprofile_meta_buffer);
			p_mmprofile_meta_buffer = NULL;
		}
		bmmprofile_init_buffer = 0;
		mutex_unlock(&mmprofile_buffer_init_mutex);
		MMP_LOG(ANDROID_LOG_DEBUG, "Cannot allocate buffer");
		return;
	}
#else
	if (!p_mmprofile_ring_buffer) {
		bmmprofile_init_buffer = 0;
		mutex_unlock(&mmprofile_buffer_init_mutex);
		MMP_LOG(ANDROID_LOG_DEBUG, "Cannot allocate buffer");
		return;
	}
#endif

	if (b_reset_ring_buffer)
		memset((void *)(p_mmprofile_ring_buffer), 0,
		       mmprofile_globals.buffer_size_bytes);
#ifdef CONFIG_MTK_ENG_BUILD
	if (b_reset_meta_buffer) {
		struct mmprofile_meta_datablock_t *p_block;

		memset((void *)(p_mmprofile_meta_buffer), 0,
		       mmprofile_globals.meta_buffer_size);
		/* Initialize the first block in meta buffer. */
		p_block =
			(struct mmprofile_meta_datablock_t *)
			p_mmprofile_meta_buffer;
		p_block->block_size = mmprofile_globals.meta_buffer_size;
		INIT_LIST_HEAD(&mmprofile_meta_buffer_list);
		list_add_tail(&(p_block->list), &mmprofile_meta_buffer_list);
	}
#endif
	bmmprofile_init_buffer = 1;

	mutex_unlock(&mmprofile_buffer_init_mutex);
}

static void mmprofile_reset_buffer(void)
{
#ifdef CONFIG_MTK_ENG_BUILD

	if (!mmprofile_globals.enable ||
		(mmprofile_globals.buffer_size_record !=
			mmprofile_globals.new_buffer_size_record))
		return;
	if (bmmprofile_init_buffer) {
		struct mmprofile_meta_datablock_t *p_block;

		memset((void *)(p_mmprofile_ring_buffer), 0,
			mmprofile_globals.buffer_size_bytes);
		mmprofile_globals.write_pointer = 0;

		mutex_lock(&mmprofile_meta_buffer_mutex);
		mmprofile_meta_datacookie = 1;
		memset((void *)(p_mmprofile_meta_buffer), 0,
			mmprofile_globals.meta_buffer_size);
		/* Initialize the first block in meta buffer. */
		p_block =
		    (struct mmprofile_meta_datablock_t *)
		    p_mmprofile_meta_buffer;
		p_block->block_size = mmprofile_globals.meta_buffer_size;
		INIT_LIST_HEAD(&mmprofile_meta_buffer_list);
		list_add_tail(&(p_block->list), &mmprofile_meta_buffer_list);

		mutex_unlock(&mmprofile_meta_buffer_mutex);

	}
#endif
}

static void mmprofile_force_start(int start)
{
	MMP_MSG("start: %d", start);
	if (!mmprofile_globals.enable)
		return;
	MMP_LOG(ANDROID_LOG_DEBUG, "+start %d", start);
	if (start && (!mmprofile_globals.start)) {
		mmprofile_init_buffer();
		mmprofile_reset_buffer();
	}
	mmprofile_globals.start = start;
	MMP_LOG(ANDROID_LOG_DEBUG, "-start=%d", mmprofile_globals.start);
}

/* this function only used by other kernel modules. */
void mmprofile_start(int start)
{
#ifndef FORBID_MMP_START
	mmprofile_force_start(start);
#endif
}
EXPORT_SYMBOL(mmprofile_start);

void mmprofile_enable(int enable)
{
	MMP_MSG("enable: %d", enable);
	if (enable)
		mmprofile_register_static_events(1);
	mmprofile_globals.enable = enable;
	if (enable == 0)
		mmprofile_force_start(0);
}
EXPORT_SYMBOL(mmprofile_enable);

/* if using remote tool (PC side) or adb shell command, can always start mmp */
static void mmprofile_remote_start(int start)
{
	MMP_MSG("remote start: %d", start);
	if (!mmprofile_globals.enable)
		return;
	MMP_LOG(ANDROID_LOG_DEBUG, "remote +start %d", start);
	if (start && (!mmprofile_globals.start)) {
		mmprofile_init_buffer();
		mmprofile_reset_buffer();
	}
	mmprofile_globals.start = start;
	MMP_LOG(ANDROID_LOG_DEBUG, "remote -start=%d",
		mmprofile_globals.start);
}

static mmp_event mmprofile_find_event_int(mmp_event parent, const char *name)
{
	mmp_event index;
	struct mmprofile_regtable_t *p_regtable;

	index = MMP_ROOT_EVENT;
	list_for_each_entry(p_regtable, &(mmprofile_regtable.list), list) {
		if ((parent == 0) ||
			(p_regtable->event_info.parent_id == parent)) {

			if (strncmp(p_regtable->event_info.name, name,
				MMPROFILE_EVENT_NAME_MAX_LEN) == 0) {
				return index;
			}
		}
		index++;
	}
	return 0;
}

static int mmprofile_get_event_name(mmp_event event, char *name, size_t *size)
{
	/* current event for seraching */
	mmp_event curr_event = event;
	/* event info for all level of the event */
	struct mmprofile_eventinfo_t *event_info[32];
	unsigned int info_cnt = 0;
	int found = 0;
	int ret = -1;

	if ((name == NULL) || (size == NULL)) {
		/* parameters invalid */
		return ret;
	}

	while (1) {
		struct mmprofile_regtable_t *p_regtable;
		int curr_found = 0;
		mmp_event index = MMP_ROOT_EVENT;

		/* check the event */
		if ((curr_event == MMP_INVALID_EVENT)
		    || (curr_event > mmprofile_globals.reg_event_index)) {
			/* the event invalid */
			break;
		}

		if (info_cnt >= ARRAY_SIZE(event_info)) {
			/* the level of event is out of limite */
			found = 1;
			break;
		}

		/* search the info for the event */
		list_for_each_entry(p_regtable,
			&(mmprofile_regtable.list), list) {
			if (index == curr_event) {
				/* find this event */
				curr_found = 1;
				event_info[info_cnt] = &p_regtable->event_info;
				break;
			}
			index++;
		}

		if (!curr_found) {
			/* can not find the event */
			break;
		}


		if ((event_info[info_cnt]->parent_id == MMP_ROOT_EVENT) ||
			(event_info[info_cnt]->parent_id ==
				MMP_INVALID_EVENT)) {
			/* find all path for the event */
			found = 1;
			info_cnt++;
			break;
		}

		/* search the parent of the event */
		curr_event = event_info[info_cnt]->parent_id;
		info_cnt++;
	}

	if (found) {
		size_t need_len = 0;
		size_t actual_len = 0;
		int info_cnt_used = 0;
		int i;

		WARN_ON(!(info_cnt > 0));

		for (i = 0; i < info_cnt; i++) {
			/* after each name has a ':' or '\0' */
			need_len += strlen(event_info[i]->name) + 1;
			if (need_len <= *size) {
				/* buffer size is ok */
				info_cnt_used = i + 1;
			}
		}

		for (i = info_cnt_used - 1; i >= 0; i--) {
			strncpy(&name[actual_len], event_info[i]->name,
				strlen(event_info[i]->name) + 1);
			actual_len += strlen(event_info[i]->name);
			if (i > 0) {
				/* not the last name */
				name[actual_len] = ':';
			}
			actual_len++;
		}

		ret = (int)actual_len;
		*size = need_len;
	}

	return ret;
}

static int mmprofile_config_event(mmp_event event, char *name,
	mmp_event parent, int sync)
{
	mmp_event index;
	struct mmprofile_regtable_t *p_regtable;

	if (in_interrupt())
		return 0;
	if ((event >= MMPROFILE_MAX_EVENT_COUNT) ||
		(event == MMP_INVALID_EVENT))
		return 0;
	if (sync) {
		mutex_lock(&mmprofile_regtable_mutex);
	} else {
		if (mutex_trylock(&mmprofile_regtable_mutex) == 0)
			return 0;
	}
	index = mmprofile_find_event_int(parent, name);
	if (index) {
		mutex_unlock(&mmprofile_regtable_mutex);
		return 1;
	}
	p_regtable = kmalloc(sizeof(struct mmprofile_regtable_t), GFP_KERNEL);
	if (!p_regtable) {
		mutex_unlock(&mmprofile_regtable_mutex);
		return 0;
	}
	strncpy(p_regtable->event_info.name, name,
		MMPROFILE_EVENT_NAME_MAX_LEN);
	p_regtable->event_info.name[MMPROFILE_EVENT_NAME_MAX_LEN] = 0;
	p_regtable->event_info.parent_id = parent;
	list_add_tail(&(p_regtable->list), &(mmprofile_regtable.list));

	mutex_unlock(&mmprofile_regtable_mutex);
	return 1;
}

static int mmprofile_register_static_events(int sync)
{
	static unsigned int b_static_event_registered;
	unsigned int static_event_count = 0;
	unsigned int i;
	int ret = 1;

	if (in_interrupt())
		return 0;
	if (b_static_event_registered)
		return 1;
	static_event_count =
		sizeof(mmprofile_static_events) /
		sizeof(struct mmp_static_event_t);
	for (i = 0; i < static_event_count; i++) {
		ret = ret
		    && mmprofile_config_event(mmprofile_static_events[i].event,
				mmprofile_static_events[i].name,
				mmprofile_static_events[i].parent, sync);
	}
	b_static_event_registered = 1;
	return ret;
}

/* the MMP_TRACING is defined only when CONFIG_TRACING is defined
 * and we enable mmp to trace its API.
 */
#ifdef MMP_TRACING
static unsigned long __read_mostly tracing_mark_write_addr;
static inline void __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");
}

static inline void mmp_kernel_trace_begin(char *name)
{
	if (mmp_trace_log_on) {
		__mt_update_tracing_mark_write_addr();
		event_trace_printk(tracing_mark_write_addr,
			"B|%d|%s\n", current->tgid, name);
	}
}

static inline void mmp_kernel_trace_counter(char *name, int count)
{
	if (mmp_trace_log_on) {
		__mt_update_tracing_mark_write_addr();
		event_trace_printk(tracing_mark_write_addr,
			"C|%d|%s|%d\n",
			in_interrupt() ? -1 : current->tgid, name, count);
	}
}

static inline void mmp_kernel_trace_end(void)
{
	if (mmp_trace_log_on) {
		__mt_update_tracing_mark_write_addr();
		event_trace_printk(tracing_mark_write_addr, "E\n");
	}
}
#else
static inline void mmp_kernel_trace_begin(char *name)
{
}

static inline void mmp_kernel_trace_end(void)
{
}

static inline void mmp_kernel_trace_counter(char *name, int count)
{
}
#endif

/* continue to use 32-bit value to store time value (separate into 2) */
static void system_time(unsigned int *low, unsigned int *high)
{
	unsigned long long temp;

	temp = sched_clock();
	*low = (unsigned int)(temp & 0xffffffff);
	*high = (unsigned int)((temp >> 32) & 0xffffffff);
}

static bool is_mmp_valid(mmp_event event)
{
	if (!bmmprofile_init_buffer || !mmprofile_globals.start)
		return false;

	if (!(mmprofile_globals.event_state[event] & MMP_EVENT_STATE_ENABLED))
		return false;
	if ((mmprofile_globals.buffer_size_record !=
			mmprofile_globals.new_buffer_size_record))
		return false;

	return true;
}

static void mmprofile_log_int(mmp_event event, enum mmp_log_type type,
	unsigned long data1, unsigned long data2,
	unsigned int meta_data_cookie)
{
	char name[256];
	size_t prefix_len;
	size_t size;
	struct mmprofile_event_t *p_event = NULL;
	unsigned int index;
	unsigned int lock;

	if (!mmprofile_globals.enable)
		return;
	if ((event >= MMPROFILE_MAX_EVENT_COUNT) ||
		(event == MMP_INVALID_EVENT))
		return;
	if (!is_mmp_valid(event))
		return;


	/* Event ID 0 and 1 are protected.
	 * They are not allowed for logging.
	 */
	if (unlikely(event < 2))
		return;
	index = ((unsigned int)atomic_inc_return((atomic_t *)
			&(mmprofile_globals.write_pointer)) - 1)
	    % (mmprofile_globals.buffer_size_record);
	/*check vmalloc address is valid or not*/
	if (!pfn_valid(vmalloc_to_pfn((struct mmprofile_event_t *)
		&(p_mmprofile_ring_buffer[index])))) {
		mmp_aee("write_pointer:0x%x,index:0x%x,line:%d\n",
			mmprofile_globals.write_pointer, index, __LINE__);
		pr_info("buffer_size_record:0x%x,new_buffer_size_record:0x%x\n",
			mmprofile_globals.buffer_size_record,
			mmprofile_globals.new_buffer_size_record);
		return;
	}
	lock = (unsigned int)atomic_inc_return((atomic_t *)
		&(p_mmprofile_ring_buffer[index].lock));
	/*atomic_t is INT, write_pointer is UINT, avoid convert error*/
	if (mmprofile_globals.write_pointer ==
		mmprofile_globals.buffer_size_record)
		mmprofile_globals.write_pointer = 0;
	if (unlikely(lock > 1)) {
		/* Do not reduce lock count since it need
		 * to be marked as invalid.
		 */
		while (1) {
			index =
				((unsigned int)atomic_inc_return((atomic_t *)
				&(mmprofile_globals.write_pointer)) - 1) %
				(mmprofile_globals.buffer_size_record);
			if (!pfn_valid(vmalloc_to_pfn
				((struct mmprofile_event_t *)
					&(p_mmprofile_ring_buffer[index])))) {
				mmp_aee("write_pt:0x%x,index:0x%x,line:%d\n",
					mmprofile_globals.write_pointer,
					index, __LINE__);
				pr_info("buf_size:0x%x,new_buf_size:0x%x\n",
					mmprofile_globals.buffer_size_record,
				mmprofile_globals.new_buffer_size_record);
				return;
			}
			lock =
			    (unsigned int)atomic_inc_return((atomic_t *) &
					(p_mmprofile_ring_buffer[index].lock));
			/*avoid convert error*/
			if (mmprofile_globals.write_pointer ==
				mmprofile_globals.buffer_size_record)
				mmprofile_globals.write_pointer = 0;
			/* Do not reduce lock count since it need to be
			 * marked as invalid.
			 */
			if (likely(lock == 1))
				break;
		}
	}
	p_event = (struct mmprofile_event_t *)
		&(p_mmprofile_ring_buffer[index]);
	system_time(&(p_event->time_low), &(p_event->time_high));
	p_event->id = event;
	p_event->flag = type;
	p_event->data1 = (unsigned int)data1;
	p_event->data2 = (unsigned int)data2;
	p_event->meta_data_cookie = meta_data_cookie;
	lock = atomic_dec_return((atomic_t *) &(p_event->lock));
	if (unlikely(lock > 0)) {
		/* Someone has marked this record as invalid.
		 * Kill this record.
		 */
		p_event->id = 0;
		p_event->lock = 0;
	}

	if ((mmprofile_globals.event_state[event] & MMP_EVENT_STATE_FTRACE)
	    || (type & MMPROFILE_FLAG_SYSTRACE)) {

		/* ignore interrupt */
		if (in_interrupt())
			return;

		memset((void *)name, 0, 256);
		name[0] = 'M';
		name[1] = 'M';
		name[2] = 'P';
		name[3] = ':';
		prefix_len = strlen(name);
		size = sizeof(name) - prefix_len;

		if (mmprofile_get_event_name(
			event, &name[prefix_len], &size) > 0) {

			if (type & MMPROFILE_FLAG_START) {
				mmp_kernel_trace_begin(name);
			} else if (type & MMPROFILE_FLAG_END) {
				mmp_kernel_trace_end();
			} else if (type & MMPROFILE_FLAG_PULSE) {
				mmp_kernel_trace_counter(name, 1);
				mmp_kernel_trace_counter(name, 0);
			}
		}
	}
}

static long mmprofile_log_meta_int(mmp_event event, enum mmp_log_type type,
	struct mmp_metadata_t *p_meta_data, long b_from_user)
{
#ifdef CONFIG_MTK_ENG_BUILD
	unsigned long retn;
	void __user *p_data;
	struct mmprofile_meta_datablock_t *p_node = NULL;
	unsigned long block_size;

	if (!mmprofile_globals.enable)
		return 0;
	if ((event >= MMPROFILE_MAX_EVENT_COUNT) ||
		(event == MMP_INVALID_EVENT))
		return -3;

	if (!is_mmp_valid(event))
		return 0;

	if (unlikely(!p_meta_data))
		return -1;
	block_size =
	    ((offsetof(struct mmprofile_meta_datablock_t, meta_data) +
	    p_meta_data->size) + 3) & (~3);
	if (block_size > mmprofile_globals.meta_buffer_size)
		return -2;
	mutex_lock(&mmprofile_meta_buffer_mutex);
	p_node = list_entry(mmprofile_meta_buffer_list.prev,
		struct mmprofile_meta_datablock_t, list);
	/* If the tail block has been used,
	 * move the first block to tail and use it for new meta data.
	 */
	if (p_node->data_size > 0) {
		list_move_tail(mmprofile_meta_buffer_list.next,
			&mmprofile_meta_buffer_list);
		p_node = list_entry(mmprofile_meta_buffer_list.prev,
			struct mmprofile_meta_datablock_t, list);
	}
	/* Migrate a block with enough size.
	 * The room is collected by sacrificing
	 * least recent used blocks.
	 */
	while (p_node->block_size < block_size) {
		struct mmprofile_meta_datablock_t *p_next_node =
		    list_entry(p_node->list.next,
				struct mmprofile_meta_datablock_t, list);
		if (&(p_next_node->list) == &mmprofile_meta_buffer_list)
			p_next_node =  list_entry(p_next_node->list.next,
				struct mmprofile_meta_datablock_t, list);

		list_del(&(p_next_node->list));
		p_node->block_size += p_next_node->block_size;
	}
	/* Split the block if left memory is enough for a new block. */
	if (((unsigned long)p_node + block_size) <
	    ((unsigned long)p_mmprofile_meta_buffer +
			mmprofile_globals.meta_buffer_size)
	    && ((unsigned long)p_node + block_size) >
	    ((unsigned long)p_mmprofile_meta_buffer +
			mmprofile_globals.meta_buffer_size -
	     offsetof(struct mmprofile_meta_datablock_t, meta_data))) {
		block_size =
		    (unsigned long)p_mmprofile_meta_buffer +
		    mmprofile_globals.meta_buffer_size -
		    (unsigned long)p_node;
	}
	if ((p_node->block_size - block_size) >=
	    offsetof(struct mmprofile_meta_datablock_t, meta_data)) {
		struct mmprofile_meta_datablock_t *p_new_node =
		    (struct mmprofile_meta_datablock_t *)
			((unsigned long)p_node + block_size);
		if ((unsigned long)p_new_node >=
		    ((unsigned long)p_mmprofile_meta_buffer +
		     mmprofile_globals.meta_buffer_size))
			p_new_node =
			    (struct mmprofile_meta_datablock_t *)
				((unsigned long)p_new_node -
				mmprofile_globals.meta_buffer_size);
		p_new_node->block_size = p_node->block_size - block_size;
		p_new_node->data_size = 0;
		list_add(&(p_new_node->list), &(p_node->list));
		p_node->block_size = block_size;
	}
	/* Fill data */
	p_node->data_size = p_meta_data->size;
	p_node->data_type = p_meta_data->data_type;
	p_node->cookie = mmprofile_meta_datacookie;
	mmprofile_log_int(event, type, p_meta_data->data1,
		p_meta_data->data2, mmprofile_meta_datacookie);
	mmprofile_meta_datacookie++;
	if (mmprofile_meta_datacookie == 0)
		mmprofile_meta_datacookie++;
	p_data = (void __user *)(p_meta_data->p_data);
	if (((unsigned long)(p_node->meta_data) + p_meta_data->size) >
	    ((unsigned long)p_mmprofile_meta_buffer +
	    mmprofile_globals.meta_buffer_size)) {

		unsigned long left_size =
		    (unsigned long)p_mmprofile_meta_buffer +
		    mmprofile_globals.meta_buffer_size -
		    (unsigned long)(p_node->meta_data);
		if (b_from_user) {
			retn =
				copy_from_user(p_node->meta_data,
					p_data, left_size);
			retn =
			    copy_from_user(p_mmprofile_meta_buffer,
					(void *)((unsigned long)p_data +
					    left_size),
					p_meta_data->size - left_size);
		} else {
			memcpy(p_node->meta_data, p_data, left_size);
			memcpy(p_mmprofile_meta_buffer,
			       (void *)((unsigned long)p_data + left_size),
			       p_meta_data->size - left_size);
		}
	} else {
		if (b_from_user)
			retn =
			    copy_from_user(p_node->meta_data, p_data,
					   p_meta_data->size);
		else
			memcpy(p_node->meta_data, p_data, p_meta_data->size);
	}
	mutex_unlock(&mmprofile_meta_buffer_mutex);
#endif

	return 0;
}

/* Internal functions end */

/* Exposed APIs begin */
mmp_event mmprofile_register_event(mmp_event parent, const char *name)
{
	mmp_event index;
	struct mmprofile_regtable_t *p_regtable;

	if (!mmprofile_globals.enable)
		return 0;
	if (in_interrupt())
		return 0;
	mutex_lock(&mmprofile_regtable_mutex);
	if (mmprofile_globals.reg_event_index >=
		(MMPROFILE_MAX_EVENT_COUNT - 1)) {
		mutex_unlock(&mmprofile_regtable_mutex);
		return 0;
	}
	/* Check if this event has already been registered. */
	index = mmprofile_find_event_int(parent, name);
	if (index) {
		mutex_unlock(&mmprofile_regtable_mutex);
		return index;
	}
	/* Check if the parent exists. */
	if ((parent == 0) || (parent > mmprofile_globals.reg_event_index)) {
		mutex_unlock(&mmprofile_regtable_mutex);
		return 0;
	}
	/* Now register the new event. */
	p_regtable = kmalloc(sizeof(struct mmprofile_regtable_t), GFP_KERNEL);
	if (!p_regtable) {
		mutex_unlock(&mmprofile_regtable_mutex);
		return 0;
	}
	index = ++(mmprofile_globals.reg_event_index);
	if (strlen(name) > MMPROFILE_EVENT_NAME_MAX_LEN) {
		memcpy(p_regtable->event_info.name, name,
			MMPROFILE_EVENT_NAME_MAX_LEN);
		p_regtable->event_info.name[MMPROFILE_EVENT_NAME_MAX_LEN] = 0;
	} else
		strncpy(p_regtable->event_info.name, name, strlen(name) + 1);
	p_regtable->event_info.parent_id = parent;
	list_add_tail(&(p_regtable->list), &(mmprofile_regtable.list));
	mmprofile_globals.event_state[index] = 0;
	mutex_unlock(&mmprofile_regtable_mutex);
	return index;
}
EXPORT_SYMBOL(mmprofile_register_event);

mmp_event mmprofile_find_event(mmp_event parent, const char *name)
{
	mmp_event event;

	if (!mmprofile_globals.enable)
		return 0;
	if (in_interrupt())
		return 0;
	mutex_lock(&mmprofile_regtable_mutex);
	event = mmprofile_find_event_int(parent, name);
	mutex_unlock(&mmprofile_regtable_mutex);
	return event;
}
EXPORT_SYMBOL(mmprofile_find_event);

void mmprofile_enable_ftrace_event(mmp_event event, long enable, long ftrace)
{
	unsigned int state;

	if (!mmprofile_globals.enable)
		return;
	if ((event < 2) || (event >= MMPROFILE_MAX_EVENT_COUNT))
		return;
	state = enable ? MMP_EVENT_STATE_ENABLED : 0;
	if (enable && ftrace)
		state |= MMP_EVENT_STATE_FTRACE;
	mmprofile_globals.event_state[event] = state;
}
EXPORT_SYMBOL(mmprofile_enable_ftrace_event);

void mmprofile_enable_event(mmp_event event, long enable)
{
	mmprofile_enable_ftrace_event(event, enable, 0);
}
EXPORT_SYMBOL(mmprofile_enable_event);

void mmprofile_enable_ftrace_event_recursive(mmp_event event, long enable,
	long ftrace)
{
	mmp_event index;
	struct mmprofile_regtable_t *p_regtable;

	index = MMP_ROOT_EVENT;
	mmprofile_enable_ftrace_event(event, enable, ftrace);
	list_for_each_entry(p_regtable, &(mmprofile_regtable.list), list) {
		if (p_regtable->event_info.parent_id != event)
			continue;
		mmprofile_enable_ftrace_event_recursive(index, enable, ftrace);

		index++;
	}
}
EXPORT_SYMBOL(mmprofile_enable_ftrace_event_recursive);

void mmprofile_enable_event_recursive(mmp_event event, long enable)
{
	mmp_event index;
	struct mmprofile_regtable_t *p_regtable;

	index = MMP_ROOT_EVENT;
	mmprofile_enable_event(event, enable);
	list_for_each_entry(p_regtable, &(mmprofile_regtable.list), list) {
		if (p_regtable->event_info.parent_id == event)
			mmprofile_enable_event_recursive(index, enable);

		index++;
	}
}
EXPORT_SYMBOL(mmprofile_enable_event_recursive);

long mmprofile_query_enable(mmp_event event)
{
	if (!mmprofile_globals.enable)
		return 0;
	if (event >= MMPROFILE_MAX_EVENT_COUNT)
		return 0;
	if (event == MMP_INVALID_EVENT)
		return mmprofile_globals.enable;
	return !!(mmprofile_globals.event_state[event] &
			MMP_EVENT_STATE_ENABLED);
}
EXPORT_SYMBOL(mmprofile_query_enable);

void mmprofile_log_ex(mmp_event event, enum mmp_log_type type,
	unsigned long data1, unsigned long data2)
{
	mmprofile_log_int(event, type, data1, data2, 0);
}
EXPORT_SYMBOL(mmprofile_log_ex);

void mmprofile_log(mmp_event event, enum mmp_log_type type)
{
	mmprofile_log_ex(event, type, 0, 0);
}
EXPORT_SYMBOL(mmprofile_log);

long mmprofile_log_meta(mmp_event event, enum mmp_log_type type,
	struct mmp_metadata_t *p_meta_data)
{
	if (!mmprofile_globals.enable)
		return 0;
	if (in_interrupt())
		return 0;
	return mmprofile_log_meta_int(event, type, p_meta_data, 0);
}
EXPORT_SYMBOL(mmprofile_log_meta);

long mmprofile_log_meta_structure(mmp_event event, enum mmp_log_type type,
	struct mmp_metadata_structure_t *p_meta_data)
{
	int ret = 0;
	struct mmp_metadata_t meta_data;

	if (!mmprofile_globals.enable)
		return 0;
	if (event >= MMPROFILE_MAX_EVENT_COUNT)
		return -3;
	if (in_interrupt())
		return 0;
	if (event == MMP_INVALID_EVENT)
		return 0;

	if (!is_mmp_valid(event))
		return 0;

	meta_data.data1 = p_meta_data->data1;
	meta_data.data2 = p_meta_data->data2;
	meta_data.data_type = MMPROFILE_META_STRUCTURE;
	meta_data.size = 32 + p_meta_data->struct_size;
	meta_data.p_data = vmalloc(meta_data.size);
	if (!meta_data.p_data)
		return -1;
	memcpy(meta_data.p_data, p_meta_data->struct_name, 32);
	memcpy((void *)((unsigned long)(meta_data.p_data) + 32),
		p_meta_data->p_data, p_meta_data->struct_size);
	ret = mmprofile_log_meta(event, type, &meta_data);
	vfree(meta_data.p_data);

	return ret;
}
EXPORT_SYMBOL(mmprofile_log_meta_structure);

long mmprofile_log_meta_string_ex(mmp_event event, enum mmp_log_type type,
	unsigned long data1, unsigned long data2, const char *str)
{
	long ret = 0;
	struct mmp_metadata_t meta_data;

	if (!mmprofile_globals.enable)
		return 0;
	if (event >= MMPROFILE_MAX_EVENT_COUNT)
		return -3;
	if (in_interrupt())
		return 0;
	if (event == MMP_INVALID_EVENT)
		return 0;
	if (!is_mmp_valid(event))
		return 0;

	meta_data.data1 = data1;
	meta_data.data2 = data2;
	meta_data.data_type = MMPROFILE_META_STRING_MBS;
	meta_data.size = strlen(str) + 1;
	meta_data.p_data = vmalloc(meta_data.size);
	if (!meta_data.p_data)
		return -1;
	strncpy((char *)meta_data.p_data, str, strlen(str) + 1);
	ret = mmprofile_log_meta(event, type, &meta_data);
	vfree(meta_data.p_data);

	return ret;
}
EXPORT_SYMBOL(mmprofile_log_meta_string_ex);

long mmprofile_log_meta_string(mmp_event event, enum mmp_log_type type,
	const char *str)
{
	return mmprofile_log_meta_string_ex(event, type, 0, 0, str);
}
EXPORT_SYMBOL(mmprofile_log_meta_string);

long mmprofile_log_meta_bitmap(mmp_event event, enum mmp_log_type type,
	struct mmp_metadata_bitmap_t *p_meta_data)
{
	int ret = 0;
	struct mmp_metadata_t meta_data;
	char *p_src, *p_dst;
	long pitch;

	if (!mmprofile_globals.enable)
		return 0;
	if (event >= MMPROFILE_MAX_EVENT_COUNT)
		return -3;
	if (in_interrupt())
		return 0;
	if (event == MMP_INVALID_EVENT)
		return 0;
	if (!is_mmp_valid(event))
		return 0;

	meta_data.data1 = p_meta_data->data1;
	meta_data.data2 = p_meta_data->data2;
	meta_data.data_type = MMPROFILE_META_BITMAP;
	meta_data.size = sizeof(struct mmp_metadata_bitmap_t) +
		p_meta_data->data_size;
	meta_data.p_data = vmalloc(meta_data.size);
	if (!meta_data.p_data)
		return -1;
	p_src = (char *)p_meta_data->p_data + p_meta_data->start_pos;
	p_dst = (char *)((unsigned long)(meta_data.p_data) +
		sizeof(struct mmp_metadata_bitmap_t));
	pitch = p_meta_data->pitch;
	memcpy(meta_data.p_data, p_meta_data,
		sizeof(struct mmp_metadata_bitmap_t));
	if (pitch < 0)
		((struct mmp_metadata_bitmap_t *)(meta_data.p_data))->pitch
			= -pitch;
	if ((pitch > 0) && (p_meta_data->down_sample_x == 1)
	    && (p_meta_data->down_sample_y == 1))
		memcpy(p_dst, p_src, p_meta_data->data_size);
	else {
		unsigned int x, y, x0, y0;
		unsigned int new_width, new_height;
		unsigned int bpp = p_meta_data->bpp / 8;
		unsigned int src_offset, dst_offset;

		new_width = (p_meta_data->width - 1) /
			p_meta_data->down_sample_x + 1;
		new_height = (p_meta_data->height - 1) /
			p_meta_data->down_sample_y + 1;
		MMP_LOG(ANDROID_LOG_DEBUG, "n(%u,%u),o(%u, %u,%d,%u) ",
			new_width, new_height, p_meta_data->width,
			p_meta_data->height, p_meta_data->pitch,
			p_meta_data->bpp);
		for (y = 0, y0 = 0; y < p_meta_data->height;
		     y0++, y += p_meta_data->down_sample_y) {
			if (p_meta_data->down_sample_x == 1)
				memcpy(p_dst + new_width * bpp * y0,
					p_src + p_meta_data->pitch * y,
					p_meta_data->width * bpp);
			else {
				for (x = 0, x0 = 0; x < p_meta_data->width;
				     x0++, x += p_meta_data->down_sample_x) {
					dst_offset =
						(new_width * y0 + x0) * bpp;
					src_offset = p_meta_data->pitch *
						y + x * bpp;
					memcpy(p_dst + dst_offset,
						p_src + src_offset, bpp);
				}
			}
		}
		meta_data.size = sizeof(struct mmp_metadata_bitmap_t) +
			new_width * bpp * new_height;
	}
	ret = mmprofile_log_meta(event, type, &meta_data);
	vfree(meta_data.p_data);

	return ret;
}
EXPORT_SYMBOL(mmprofile_log_meta_bitmap);

long mmprofile_log_meta_yuv_bitmap(mmp_event event, enum mmp_log_type type,
	struct mmp_metadata_bitmap_t *p_meta_data)
{
	int ret = 0;
	struct mmp_metadata_t meta_data;
	char *p_src, *p_dst;
	long pitch;

	if (!mmprofile_globals.enable)
		return 0;
	if (event >= MMPROFILE_MAX_EVENT_COUNT)
		return -3;
	if (in_interrupt())
		return 0;
	if (event == MMP_INVALID_EVENT)
		return 0;
	if (!is_mmp_valid(event))
		return 0;


	meta_data.data1 = p_meta_data->data1;
	meta_data.data2 = p_meta_data->data2;
	meta_data.data_type = MMPROFILE_META_RAW;
	meta_data.size = p_meta_data->data_size;
	meta_data.p_data = vmalloc(meta_data.size);
	if (!meta_data.p_data)
		return -1;
	p_src = (char *)p_meta_data->p_data + p_meta_data->start_pos;
	p_dst = (char *)((unsigned long)(meta_data.p_data));
	pitch = p_meta_data->pitch;

	if (pitch < 0)
		((struct mmp_metadata_bitmap_t *)(meta_data.p_data))->pitch
			= -pitch;
	if ((pitch > 0) && (p_meta_data->down_sample_x == 1)
	    && (p_meta_data->down_sample_y == 1))
		memcpy(p_dst, p_src, p_meta_data->data_size);
	else {
		unsigned int x, y, x0, y0;
		unsigned int new_width, new_height, new_pitch;
		unsigned int bpp = p_meta_data->bpp / 8;
		unsigned int x_offset = p_meta_data->down_sample_x * 2;
		unsigned int src_offset, dst_offset;

		new_width = (p_meta_data->width - 1) /
			p_meta_data->down_sample_x + 1;
		new_height = (p_meta_data->height - 1) /
			p_meta_data->down_sample_y + 1;
		new_pitch = new_width * bpp;
		MMP_LOG(ANDROID_LOG_DEBUG, "n(%u,%u,%u),o(%u, %u,%d,%u) ",
			new_width, new_height, new_pitch,
			p_meta_data->width, p_meta_data->height,
			p_meta_data->pitch, p_meta_data->bpp);
		for (y = 0, y0 = 0; y < p_meta_data->height;
		     y0++, y += p_meta_data->down_sample_y) {
			if (x_offset == 2)
				memcpy(p_dst + new_pitch * y0,
					p_src + p_meta_data->pitch * y,
					p_meta_data->pitch);
			else {
				for (x = 0, x0 = 0; x < p_meta_data->width;
				     x0 += 2, x += x_offset) {
					src_offset = p_meta_data->pitch * y +
						x * bpp;
					dst_offset = new_pitch * y0 + x0 * bpp;
					memcpy(p_dst + dst_offset,
						p_src + src_offset, bpp * 2);
				}
			}
		}
		meta_data.size = new_pitch * new_height;
	}
	ret = mmprofile_log_meta(event, type, &meta_data);
	vfree(meta_data.p_data);

	return ret;
}
EXPORT_SYMBOL(mmprofile_log_meta_yuv_bitmap);

/* Exposed APIs end */

/* Debug FS begin */
static struct dentry *g_p_debug_fs_dir;
static struct dentry *g_p_debug_fs_start;
static struct dentry *g_p_debug_fs_buffer;
static struct dentry *g_p_debug_fs_global;
static struct dentry *g_p_debug_fs_reset;
static struct dentry *g_p_debug_fs_enable;

static ssize_t mmprofile_dbgfs_reset_write(struct file *file,
	const char __user *buf, size_t size, loff_t *ppos)
{
	mmprofile_reset_buffer();
	return 1;
}

static ssize_t mmprofile_dbgfs_start_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	char str[32];
	int r;

	MMP_LOG(ANDROID_LOG_DEBUG, "start=%d", mmprofile_globals.start);
	r = sprintf(str, "start = %d\n", mmprofile_globals.start);
	if (r < 0)
		pr_debug("sprintf error\n");

	return simple_read_from_buffer(buf, size, ppos, str, r);
}

static ssize_t mmprofile_dbgfs_start_write(struct file *file,
	const char __user *buf, size_t size, loff_t *ppos)
{
	unsigned int str = 0;
	int start;
	ssize_t ret;

	ret = simple_write_to_buffer(&str, 4, ppos, buf, size);
	if ((str & 0xFF) == 0x30)
		start = 0;
	else
		start = 1;
	MMP_LOG(ANDROID_LOG_DEBUG, "start=%d", start);
	mmprofile_force_start(start);
	return ret;
}

static ssize_t mmprofile_dbgfs_enable_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	char str[32];
	int r;

	MMP_LOG(ANDROID_LOG_DEBUG, "enable=%d", mmprofile_globals.enable);
	r = sprintf(str, "enable = %d\n", mmprofile_globals.enable);
	if (r < 0)
		pr_debug("sprintf error\n");

	return simple_read_from_buffer(buf, size, ppos, str, r);
}

static ssize_t mmprofile_dbgfs_enable_write(struct file *file,
	const char __user *buf, size_t size, loff_t *ppos)
{
	unsigned int str = 0;
	int enable;
	ssize_t ret;

	ret = simple_write_to_buffer(&str, 4, ppos, buf, size);
	if ((str & 0xFF) == 0x30)
		enable = 0;
	else
		enable = 1;
	MMP_LOG(ANDROID_LOG_DEBUG, "enable=%d", enable);
	mmprofile_enable(enable);
	return ret;
}

static ssize_t mmprofile_dbgfs_buffer_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	static unsigned int backup_state;
	unsigned int copy_size = 0;
	unsigned int total_copy = 0;
	unsigned long addr;

	if (!bmmprofile_init_buffer)
		return -EFAULT;
	MMP_LOG(ANDROID_LOG_VERBOSE, "size=%ld ppos=%d",
		(unsigned long)size, (int)(*ppos));
	if (*ppos == 0) {
		backup_state = mmprofile_globals.start;
		mmprofile_force_start(0);
	}
	while (size > 0) {
		mmprofile_get_dump_buffer(*ppos, &addr, &copy_size);
		if (copy_size == 0) {
			if (backup_state)
				mmprofile_force_start(1);
			break;
		}
		if (size >= copy_size) {
			size -= copy_size;
		} else {
			copy_size = size;
			size = 0;
		}
		if (copy_to_user(buf + total_copy, (void *)addr, copy_size)) {
			MMP_LOG(ANDROID_LOG_DEBUG,
				"fail to copytouser total_copy=%d",
				total_copy);
			break;
		}
		*ppos += copy_size;
		total_copy += copy_size;
	}
	return total_copy;
}

static ssize_t mmprofile_dbgfs_global_read(struct file *file, char __user *buf,
	size_t size, loff_t *ppos)
{
	return simple_read_from_buffer(buf, size, ppos, &mmprofile_globals,
		MMPROFILE_GLOBALS_SIZE);
}

static const struct file_operations mmprofile_dbgfs_enable_fops = {
	.read = mmprofile_dbgfs_enable_read,
	.write = mmprofile_dbgfs_enable_write,
	.llseek = generic_file_llseek,
};

static const struct file_operations mmprofile_dbgfs_start_fops = {
	.read = mmprofile_dbgfs_start_read,
	.write = mmprofile_dbgfs_start_write,
	.llseek = generic_file_llseek,
};

static const struct file_operations mmprofile_dbgfs_reset_fops = {
	.write = mmprofile_dbgfs_reset_write,
	.llseek = generic_file_llseek,
};

static const struct file_operations mmprofile_dbgfs_buffer_fops = {
	.read = mmprofile_dbgfs_buffer_read,
	.llseek = generic_file_llseek,
};

static const struct file_operations mmprofile_dbgfs_global_fops = {
	.read = mmprofile_dbgfs_global_read,
	.llseek = generic_file_llseek,
};

/* Debug FS end */

static char cmd_buf[128];
static void process_dbg_cmd(char *cmd)
{
	if (strncmp(cmd, "mmp_log_on:", 11) == 0) {
		char *p = (char *)cmd + 11;
		unsigned long value;

		if (0 == kstrtoul(p, 10, &value) && 0 != value)
			mmp_log_on = 1;
		else
			mmp_log_on = 0;
		MMP_MSG("mmp_log_on=%d\n", mmp_log_on);
	} else if (strncmp(cmd, "mmp_trace_log_on:", 17) == 0) {
		char *p = (char *)cmd + 17;
		unsigned long value;

		if (0 == kstrtoul(p, 10, &value) && 0 != value)
			mmp_trace_log_on = 1;
		else
			mmp_trace_log_on = 0;
		MMP_MSG("mmp_trace_log_on=%d\n", mmp_trace_log_on);
	} else {
		MMP_MSG("invalid mmp debug command: %s\n",
			cmd != NULL ? cmd : "(empty)");
	}
}

/* Driver specific begin */
static int mmprofile_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int mmprofile_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mmprofile_read(struct file *file, char __user *data,
	size_t len, loff_t *ppos)
{
	return 0;
}

static ssize_t mmprofile_write(struct file *file, const char __user *data,
	size_t len, loff_t *ppos)
{
	ssize_t ret;
	size_t length = len;

	if (length > 127)
		length = 127;
	ret = length;

	if (copy_from_user(&cmd_buf, data, length))
		return -EFAULT;

	cmd_buf[length] = 0;
	process_dbg_cmd(cmd_buf);
	return ret;
}

static long mmprofile_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	long ret = 0;
	unsigned long retn = 0;

	switch (cmd) {
	case MMP_IOC_ENABLE:
		if ((arg == 0) || (arg == 1))
			mmprofile_enable((int)arg);
		else
			ret = -EINVAL;
		break;
	case MMP_IOC_REMOTESTART:
		/* if using remote tool (PC side) or adb shell command,
		 * can always start mmp
		 */
		if ((arg == 0) || (arg == 1))
			mmprofile_remote_start((int)arg);
		else
			ret = -EINVAL;
		break;
	case MMP_IOC_START:
		if ((arg == 0) || (arg == 1))
			mmprofile_force_start((int)arg);
		else
			ret = -EINVAL;
		break;
	case MMP_IOC_TIME:
	{
		unsigned int time_low;
		unsigned int time_high;
		unsigned long long time;
		unsigned long long *p_time_user =
			(unsigned long long __user *)arg;

		system_time(&time_low, &time_high);
		time = time_low + ((unsigned long long)time_high << 32);
		put_user(time, p_time_user);
	}
	break;
	case MMP_IOC_REGEVENT:
	{
		struct mmprofile_eventinfo_t event_info;
		struct mmprofile_eventinfo_t __user *p_event_info_user =
			(struct mmprofile_eventinfo_t __user *)arg;

		retn = copy_from_user(&event_info, p_event_info_user,
		    sizeof(struct mmprofile_eventinfo_t));
		event_info.name[MMPROFILE_EVENT_NAME_MAX_LEN] = 0;
		event_info.parent_id =
		    mmprofile_register_event(event_info.parent_id,
		    event_info.name);
		retn = copy_to_user(p_event_info_user, &event_info,
			sizeof(struct mmprofile_eventinfo_t));
	}
	break;
	case MMP_IOC_FINDEVENT:
	{
		struct mmprofile_eventinfo_t event_info;
		struct mmprofile_eventinfo_t __user *p_event_info_user =
			(struct mmprofile_eventinfo_t __user *)arg;

		retn = copy_from_user(&event_info, p_event_info_user,
		    sizeof(struct mmprofile_eventinfo_t));
		event_info.name[MMPROFILE_EVENT_NAME_MAX_LEN] = 0;
		mutex_lock(&mmprofile_regtable_mutex);
		event_info.parent_id =
		    mmprofile_find_event_int(event_info.parent_id,
		    event_info.name);
		mutex_unlock(&mmprofile_regtable_mutex);
		retn = copy_to_user(p_event_info_user, &event_info,
			sizeof(struct mmprofile_eventinfo_t));
	}
	break;
	case MMP_IOC_ENABLEEVENT:
	{
		mmp_event event;
		unsigned int enable;
		unsigned int recursive;
		unsigned int ftrace;
		struct mmprofile_eventsetting_t __user *p_event_setting_user =
			(struct mmprofile_eventsetting_t __user *)arg;

		get_user(event, &p_event_setting_user->event);
		get_user(enable, &p_event_setting_user->enable);
		get_user(recursive, &p_event_setting_user->recursive);
		get_user(ftrace, &p_event_setting_user->ftrace);
		if (recursive) {
			mutex_lock(&mmprofile_regtable_mutex);
			mmprofile_enable_ftrace_event_recursive(event,
				enable, ftrace);
			mutex_unlock(&mmprofile_regtable_mutex);
		} else
			mmprofile_enable_ftrace_event(event, enable, ftrace);
	}
	break;
	case MMP_IOC_LOG:
	{
		mmp_event event;
		enum mmp_log_type type;
		unsigned int data1;
		unsigned int data2;
		struct mmprofile_eventlog_t __user *p_event_log_user =
			(struct mmprofile_eventlog_t __user *)arg;

		get_user(event, &p_event_log_user->event);
		get_user(type, &p_event_log_user->type);
		get_user(data1, &p_event_log_user->data1);
		get_user(data2, &p_event_log_user->data2);
		mmprofile_log_ex(event, type, data1, data2);
	}
	break;
	case MMP_IOC_DUMPEVENTINFO:
	{
		mmp_event index;
		struct mmprofile_regtable_t *p_regtable;
		struct mmprofile_eventinfo_t __user *p_event_info_user =
			(struct mmprofile_eventinfo_t __user *)arg;
		struct mmprofile_eventinfo_t event_info_dummy = { 0, "" };

		mmprofile_register_static_events(1);
		mutex_lock(&mmprofile_regtable_mutex);
		retn =
		    copy_to_user(p_event_info_user, &event_info_dummy,
				 sizeof(struct mmprofile_eventinfo_t));
		index = MMP_ROOT_EVENT;
		list_for_each_entry(p_regtable,
			&(mmprofile_regtable.list), list) {
			retn = copy_to_user(&p_event_info_user[index],
			    &(p_regtable->event_info),
				sizeof(struct mmprofile_eventinfo_t));
			index++;
		}
		for (; index < MMPROFILE_MAX_EVENT_COUNT; index++) {
			retn = copy_to_user(&p_event_info_user[index],
				&event_info_dummy,
				sizeof(struct mmprofile_eventinfo_t));
		}
		mutex_unlock(&mmprofile_regtable_mutex);
	}
	break;
	case MMP_IOC_METADATALOG:
	{
		struct mmprofile_metalog_t meta_log;
		struct mmprofile_metalog_t __user *p_meta_log_user =
			(struct mmprofile_metalog_t __user *)arg;
		struct mmp_metadata_t meta_data;
		struct mmp_metadata_t __user *p_meta_data_user;

		retn = copy_from_user(&meta_log, p_meta_log_user,
			sizeof(struct mmprofile_metalog_t));
		if (retn) {
			pr_debug("[MMPROFILE]: copy_from_user failed! line:%d\n",
			 __LINE__);
			return -EFAULT;
		}
		p_meta_data_user = (struct mmp_metadata_t __user *)
			&(p_meta_log_user->meta_data);
		retn = copy_from_user(&meta_data, p_meta_data_user,
			sizeof(struct mmp_metadata_t));

		if (retn) {
			pr_debug("[MMPROFILE]: copy_from_user failed! line:%d\n",
			 __LINE__);
			return -EFAULT;
		}

		if (meta_data.size == 0 || meta_data.size > 0x3000000) {
			pr_debug("[MMPROFILE]: meta_data.size Invalid! line:%d\n",
			 __LINE__);
			return -EFAULT;
		}

		mmprofile_log_meta_int(meta_log.id, meta_log.type,
			&meta_data, 1);
	}
	break;
	case MMP_IOC_DUMPMETADATA:
	{
#ifdef CONFIG_MTK_ENG_BUILD

		unsigned int meta_data_count = 0;
		unsigned int offset = 0;
		unsigned int index;
		unsigned int buffer_size = 0;
		struct mmprofile_meta_datablock_t *p_meta_data_block;
		struct mmprofile_metadata_t __user *p_meta_data =
			(struct mmprofile_metadata_t __user *)(arg + 8);

		mutex_lock(&mmprofile_meta_buffer_mutex);
		list_for_each_entry(p_meta_data_block,
			&mmprofile_meta_buffer_list, list) {
			if (p_meta_data_block->data_size <= 0)
				continue;

			put_user(p_meta_data_block->cookie,
				 &(p_meta_data[meta_data_count].cookie));
			put_user(p_meta_data_block->data_size,
				 &(p_meta_data[meta_data_count].data_size));
			put_user(p_meta_data_block->data_type,
				 &(p_meta_data[meta_data_count].data_type));
			buffer_size += p_meta_data_block->data_size;
			meta_data_count++;
		}
		put_user(meta_data_count, (unsigned int __user *)arg);
		/* meta_data_count, buffer_size); */
		offset = 8 + sizeof(struct mmprofile_metadata_t) *
			meta_data_count;
		index = 0;
		list_for_each_entry(p_meta_data_block,
			&mmprofile_meta_buffer_list, list) {
			if (p_meta_data_block->data_size <= 0)
				continue;
			put_user(offset - 8, &(p_meta_data[index].data_offset));
			/* offset-8, p_meta_data_block->data_size); */
			if (((unsigned long)(p_meta_data_block->meta_data) +
			     p_meta_data_block->data_size) >
			    ((unsigned long)p_mmprofile_meta_buffer +
			     mmprofile_globals.meta_buffer_size)) {
				unsigned long left_size =
				    (unsigned long)p_mmprofile_meta_buffer +
				    mmprofile_globals.meta_buffer_size -
				    (unsigned long)
						(p_meta_data_block->meta_data);
				retn =
				    copy_to_user((void __user *)(arg + offset),
						 p_meta_data_block->meta_data,
						 left_size);
				retn =
				    copy_to_user((void __user *)
						(arg + offset + left_size),
						 p_mmprofile_meta_buffer,
						 p_meta_data_block->data_size -
						 left_size);
			} else
				retn =
				    copy_to_user((void __user *)(arg + offset),
						 p_meta_data_block->meta_data,
						 p_meta_data_block->data_size);
			offset = (offset + p_meta_data_block->data_size + 3)
				& (~3);
			index++;
		}
		put_user(offset - 8, (unsigned int __user *)(arg + 4));
		mutex_unlock(&mmprofile_meta_buffer_mutex);
#endif
	}

	break;
	case MMP_IOC_SELECTBUFFER:
		mmprofile_globals.selected_buffer = arg;
		break;
	case MMP_IOC_TRYLOG:
		if ((!mmprofile_globals.enable) ||
		    (!bmmprofile_init_buffer) ||
		    (!mmprofile_globals.start) ||
		    (arg >= MMPROFILE_MAX_EVENT_COUNT) ||
		    (arg == MMP_INVALID_EVENT))
			ret = -EINVAL;
		else if (!(mmprofile_globals.event_state[arg] &
				MMP_EVENT_STATE_ENABLED))
			ret = -EINVAL;
		break;
	case MMP_IOC_ISENABLE:
	{
		unsigned int is_enable;
		unsigned int __user *p_user = (unsigned int __user *)arg;
		mmp_event event;

		get_user(event, p_user);
		is_enable = (unsigned int)mmprofile_query_enable(event);
		put_user(is_enable, p_user);
	}
	break;
	case MMP_IOC_TEST:
		{
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
#define COMPAT_MMP_IOC_METADATALOG \
	_IOW(MMP_IOC_MAGIC, 9, struct compat_mmprofile_metalog_t)
#define COMPAT_MMP_IOC_DUMPMETADATA \
	_IOR(MMP_IOC_MAGIC, 10, struct compat_mmprofile_metalog_t)
static long mmprofile_ioctl_compat(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	long ret = 0;
	unsigned long retn;

	switch (cmd) {
	case MMP_IOC_ENABLE:
		ret = mmprofile_ioctl(file, MMP_IOC_ENABLE, arg);
		break;
	case MMP_IOC_REMOTESTART:
		/* if using remote tool (PC side) or adb shell command,
		 * can always start mmp
		 */
		ret = mmprofile_ioctl(file, MMP_IOC_REMOTESTART, arg);
		break;
	case MMP_IOC_START:
		ret = mmprofile_ioctl(file, MMP_IOC_START, arg);
		break;
	case MMP_IOC_TIME:
	{
		unsigned int time_low;
		unsigned int time_high;
		unsigned long long time;
		unsigned long long __user *p_time_user;

		p_time_user = compat_ptr(arg);

		system_time(&time_low, &time_high);
		time = time_low + ((unsigned long long)time_high << 32);
		put_user(time, p_time_user);
	}
	break;
	case MMP_IOC_REGEVENT:
	{
		struct mmprofile_eventinfo_t event_info;
		struct mmprofile_eventinfo_t __user *p_event_info_user;

		p_event_info_user = compat_ptr(arg);

		retn = copy_from_user(&event_info, p_event_info_user,
			sizeof(struct mmprofile_eventinfo_t));
		event_info.name[MMPROFILE_EVENT_NAME_MAX_LEN] = 0;
		event_info.parent_id =
		    mmprofile_register_event(event_info.parent_id,
		    event_info.name);
		retn = copy_to_user(p_event_info_user, &event_info,
		    sizeof(struct mmprofile_eventinfo_t));
	}
	break;
	case MMP_IOC_FINDEVENT:
	{
		struct mmprofile_eventinfo_t event_info;
		struct mmprofile_eventinfo_t __user *p_event_info_user;

		p_event_info_user = compat_ptr(arg);

		retn = copy_from_user(&event_info, p_event_info_user,
		    sizeof(struct mmprofile_eventinfo_t));
		event_info.name[MMPROFILE_EVENT_NAME_MAX_LEN] = 0;
		mutex_lock(&mmprofile_regtable_mutex);
		event_info.parent_id =
		    mmprofile_find_event_int(event_info.parent_id,
		    event_info.name);
		mutex_unlock(&mmprofile_regtable_mutex);
		retn = copy_to_user(p_event_info_user, &event_info,
		    sizeof(struct mmprofile_eventinfo_t));
	}
	break;
	case MMP_IOC_ENABLEEVENT:
	{
		mmp_event event;
		unsigned int enable;
		unsigned int recursive;
		unsigned int ftrace;
		struct mmprofile_eventsetting_t __user *p_event_setting_user;

		p_event_setting_user = compat_ptr(arg);

		get_user(event, &p_event_setting_user->event);
		get_user(enable, &p_event_setting_user->enable);
		get_user(recursive, &p_event_setting_user->recursive);
		get_user(ftrace, &p_event_setting_user->ftrace);
		if (recursive) {
			mutex_lock(&mmprofile_regtable_mutex);
			mmprofile_enable_ftrace_event_recursive(event,
				enable, ftrace);
			mutex_unlock(&mmprofile_regtable_mutex);
		} else
			mmprofile_enable_ftrace_event(event, enable, ftrace);
	}
	break;
	case MMP_IOC_LOG:
	{
		mmp_event event;
		enum mmp_log_type type;
		unsigned int data1;
		unsigned int data2;
		struct mmprofile_eventlog_t __user *p_event_log_user;

		p_event_log_user = compat_ptr(arg);

		get_user(event, &p_event_log_user->event);
		get_user(type, &p_event_log_user->type);
		get_user(data1, &p_event_log_user->data1);
		get_user(data2, &p_event_log_user->data2);
		mmprofile_log_ex(event, type, data1, data2);
	}
	break;
	case MMP_IOC_DUMPEVENTINFO:
	{
		mmp_event index;
		struct mmprofile_regtable_t *p_regtable;
		struct mmprofile_eventinfo_t __user *p_event_info_user;
		struct mmprofile_eventinfo_t event_info_dummy = { 0, "" };

		p_event_info_user = compat_ptr(arg);

		mmprofile_register_static_events(1);
		mutex_lock(&mmprofile_regtable_mutex);
		retn =
		    copy_to_user(p_event_info_user, &event_info_dummy,
				 sizeof(struct mmprofile_eventinfo_t));
		index = MMP_ROOT_EVENT;
		list_for_each_entry(p_regtable,
			&(mmprofile_regtable.list), list) {

			retn = copy_to_user(&p_event_info_user[index],
				&(p_regtable->event_info),
				sizeof(struct mmprofile_eventinfo_t));
			index++;
		}
		for (; index < MMPROFILE_MAX_EVENT_COUNT; index++) {
			retn = copy_to_user(&p_event_info_user[index],
				&event_info_dummy,
				sizeof(struct mmprofile_eventinfo_t));
		}
		mutex_unlock(&mmprofile_regtable_mutex);
	}
	break;
	case COMPAT_MMP_IOC_METADATALOG:
	{
		struct mmprofile_metalog_t meta_log;
		struct compat_mmprofile_metalog_t compat_meta_log;
		struct compat_mmprofile_metalog_t __user
			*p_compat_meta_log_user;

		p_compat_meta_log_user = compat_ptr(arg);

		if (copy_from_user(&compat_meta_log, p_compat_meta_log_user,
			sizeof(struct compat_mmprofile_metalog_t))) {
			pr_debug("[MMPROFILE]: copy_from_user failed! line:%d\n",
			 __LINE__);
			return -EFAULT;
		}
		meta_log.id = compat_meta_log.id;
		meta_log.type = compat_meta_log.type;
		meta_log.meta_data.data1 = compat_meta_log.meta_data.data1;
		meta_log.meta_data.data2 = compat_meta_log.meta_data.data2;
		meta_log.meta_data.data_type =
			compat_meta_log.meta_data.data_type;
		meta_log.meta_data.size = compat_meta_log.meta_data.size;
		meta_log.meta_data.p_data =
			compat_ptr(compat_meta_log.meta_data.p_data);

		if (meta_log.meta_data.size == 0 ||
			meta_log.meta_data.size > 0x3000000) {
			pr_debug("[MMPROFILE]: meta_log.meta_data.size Invalid! line:%d\n",
			 __LINE__);
			return -EFAULT;
		}
		mmprofile_log_meta_int(meta_log.id, meta_log.type,
			&(meta_log.meta_data), 1);
	}
	break;
	case COMPAT_MMP_IOC_DUMPMETADATA:
	{
#ifdef CONFIG_MTK_ENG_BUILD

		unsigned int meta_data_count = 0;
		unsigned int offset = 0;
		unsigned int index;
		unsigned int buffer_size = 0;
		struct mmprofile_meta_datablock_t *p_meta_data_block;
		struct mmprofile_metadata_t __user *p_meta_data;
		unsigned int __user *p_user;

		p_meta_data = compat_ptr(arg + 8);

		mutex_lock(&mmprofile_meta_buffer_mutex);
		list_for_each_entry(p_meta_data_block,
			&mmprofile_meta_buffer_list, list) {
			if (p_meta_data_block->data_size <= 0)
				continue;

			put_user(p_meta_data_block->cookie,
				 &(p_meta_data[meta_data_count].cookie));
			put_user(p_meta_data_block->data_size,
				 &(p_meta_data[meta_data_count].data_size));
			put_user(p_meta_data_block->data_type,
				 &(p_meta_data[meta_data_count].data_type));
			buffer_size += p_meta_data_block->data_size;
			meta_data_count++;
		}
		p_user = compat_ptr(arg);
		put_user(meta_data_count, p_user);
		/* meta_data_count, buffer_size); */
		offset = 8 + sizeof(struct mmprofile_metadata_t) *
			meta_data_count;
		index = 0;
		list_for_each_entry(p_meta_data_block,
			&mmprofile_meta_buffer_list, list) {
			if (p_meta_data_block->data_size <= 0)
				continue;

			put_user(offset - 8, &(p_meta_data[index].data_offset));
			/* offset-8, p_meta_data_block->data_size); */
			if (((unsigned long)(p_meta_data_block->meta_data) +
			     p_meta_data_block->data_size) >
			    ((unsigned long)p_mmprofile_meta_buffer +
			     mmprofile_globals.meta_buffer_size)) {
				unsigned long left_size =
				    (unsigned long)p_mmprofile_meta_buffer +
				    mmprofile_globals.meta_buffer_size -
				    (unsigned long)
						(p_meta_data_block->meta_data);
				p_user = compat_ptr(arg + offset);
				retn =
				    copy_to_user(p_user,
						 p_meta_data_block->meta_data,
						 left_size);
				p_user = compat_ptr(arg + offset + left_size);
				retn =
				    copy_to_user(p_user,
						 p_mmprofile_meta_buffer,
						 p_meta_data_block->data_size -
						 left_size);
			} else {
				p_user = compat_ptr(arg + offset);
				retn =
					copy_to_user(p_user,
						p_meta_data_block->meta_data,
						p_meta_data_block->data_size);
			}
			offset = (offset + p_meta_data_block->data_size
				+ 3) & (~3);
			index++;
		}
		p_user = compat_ptr(arg + 4);
		put_user(offset - 8, p_user);
		mutex_unlock(&mmprofile_meta_buffer_mutex);
#endif
	}
	break;
	case MMP_IOC_SELECTBUFFER:
		ret = mmprofile_ioctl(file, MMP_IOC_SELECTBUFFER, arg);
		break;
	case MMP_IOC_TRYLOG:
		if ((!mmprofile_globals.enable) ||
		    (!bmmprofile_init_buffer) ||
		    (!mmprofile_globals.start) ||
		    (arg >= MMPROFILE_MAX_EVENT_COUNT) ||
		    (!(mmprofile_globals.event_state[arg] &
				MMP_EVENT_STATE_ENABLED)))
			ret = -EINVAL;
		break;
	case MMP_IOC_ISENABLE:
		{
			unsigned int is_enable;
			unsigned int __user *p_user;
			mmp_event event;

			p_user = compat_ptr(arg);

			get_user(event, p_user);
			is_enable = (unsigned int)mmprofile_query_enable(event);
			put_user(is_enable, p_user);
		}
		break;
	case MMP_IOC_TEST:
		{
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
#endif

static int mmprofile_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long pos = 0;
	unsigned long i = 0;

	if (mmprofile_globals.selected_buffer == MMPROFILE_GLOBALS_BUFFER) {

		/* check user space buffer length */
		if ((vma->vm_end - vma->vm_start) != MMPROFILE_GLOBALS_SIZE)
			return -EINVAL;

		pos = vma->vm_start;
		for (i = 0; i < MMPROFILE_GLOBALS_SIZE;
			i += PAGE_SIZE, pos += PAGE_SIZE) {
			unsigned long pfn;

			pfn = __phys_to_pfn(__virt_to_phys(
				(unsigned long)(&mmprofile_globals) + i));
			if (remap_pfn_range
			    (vma, pos, pfn, PAGE_SIZE, PAGE_SHARED))
				return -EAGAIN;
			/* pr_debug("pfn: 0x%08x\n", pfn); */
		}
	} else if (mmprofile_globals.selected_buffer ==
		MMPROFILE_PRIMARY_BUFFER) {

		/* check user space buffer length */
		if ((vma->vm_end - vma->vm_start) !=
			mmprofile_globals.buffer_size_bytes)
			return -EINVAL;

		mmprofile_init_buffer();

		if (!bmmprofile_init_buffer)
			return -EAGAIN;

		pos = vma->vm_start;

		for (i = 0; i < mmprofile_globals.buffer_size_bytes;
		     i += PAGE_SIZE, pos += PAGE_SIZE) {
			if (remap_pfn_range
			    (vma, pos,
					vmalloc_to_pfn((void *)((unsigned long)
					p_mmprofile_ring_buffer + i)),
			     PAGE_SIZE, PAGE_SHARED))
				return -EAGAIN;
		}
	} else
		return -EINVAL;
	return 0;
}

const struct file_operations mmprofile_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mmprofile_ioctl,
	.open = mmprofile_open,
	.release = mmprofile_release,
	.read = mmprofile_read,
	.write = mmprofile_write,
	.mmap = mmprofile_mmap,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mmprofile_ioctl_compat,
#endif
};

struct miscdevice *mmp_dev;


static int mmprofile_probe(void)
{

	int ret;

	mmp_log_on = false;
	mmp_trace_log_on = false;
	/* Create debugfs */
	g_p_debug_fs_dir = debugfs_create_dir("mmprofile", NULL);
	if (g_p_debug_fs_dir) {
		/* Create debugfs files. */
		g_p_debug_fs_enable =
		    debugfs_create_file("enable", 0600,
				g_p_debug_fs_dir, NULL,
				&mmprofile_dbgfs_enable_fops);
		g_p_debug_fs_start =
		    debugfs_create_file("start", 0600,
				g_p_debug_fs_dir, NULL,
				&mmprofile_dbgfs_start_fops);
		g_p_debug_fs_buffer =
		    debugfs_create_file("buffer", 0400,
				g_p_debug_fs_dir, NULL,
				&mmprofile_dbgfs_buffer_fops);
		g_p_debug_fs_global =
		    debugfs_create_file("global", 0400,
				g_p_debug_fs_dir, NULL,
				&mmprofile_dbgfs_global_fops);
		g_p_debug_fs_reset =
		    debugfs_create_file("reset", 0200,
				g_p_debug_fs_dir, NULL,
				&mmprofile_dbgfs_reset_fops);
	}

	mmp_dev = kzalloc(sizeof(*mmp_dev), GFP_KERNEL);
	if (!mmp_dev)
		return -ENOMEM;

	mmp_dev->minor = MISC_DYNAMIC_MINOR;
	mmp_dev->name = "mmp";
	mmp_dev->fops = &mmprofile_fops;
	mmp_dev->parent = NULL;
	ret = misc_register(mmp_dev);
	if (ret) {
		pr_err("mmp: failed to register misc device.\n");
		kfree(mmp_dev);
		return ret;
	}

	return 0;
}

static int mmprofile_remove(void)
{
	kfree(mmp_dev);
	debugfs_remove(g_p_debug_fs_dir);
	debugfs_remove(g_p_debug_fs_enable);
	debugfs_remove(g_p_debug_fs_start);
	debugfs_remove(g_p_debug_fs_global);
	debugfs_remove(g_p_debug_fs_buffer);
	debugfs_remove(g_p_debug_fs_reset);
	return 0;
}

static int __init mmprofile_init(void)
{
	mmprofile_probe();
	return 0;
}

static void __exit mmprofile_exit(void)
{
	mmprofile_remove();
}

/* Driver specific end */

module_init(mmprofile_init);
module_exit(mmprofile_exit);
MODULE_AUTHOR("Tianshu Qiu <tianshu.qiu@mediatek.com>");
MODULE_DESCRIPTION("MMProfile Driver");
MODULE_LICENSE("GPL");
