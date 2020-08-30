/*
 * Copyright (C) 2015 MediaTek Inc.
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

#include "disp_drv_log.h"
#include "ion_drv.h"
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

#include <linux/wait.h>
#include <linux/file.h>

#include "mtk_sync.h"
#include "debug.h"
#include "ddp_ovl.h"
#include "mtkfb_fence.h"
#include "ddp_path.h"
#include "disp_drv_platform.h"
#include "display_recorder.h"
#include "ddp_mmp.h"
#include "primary_display.h"
#include "mtk_disp_mgr.h"


static bool mtkfb_fence_on;

#define MTKFB_FENCE_LOG(fmt, arg...)				\
	do {							\
		if (mtkfb_fence_on)				\
			pr_debug("DISP/fence " fmt, ##arg);	\
	} while (0)
#define MTKFB_FENCE_PR_ERR(fmt, arg...) \
		pr_err("DISP/fence error(%d):"fmt, __LINE__, ##arg)
#define MTKFB_FENCE_LOG_D(fmt, arg...)	pr_debug("DISP/fence " fmt, ##arg)
#define MTKFB_FENCE_LOG_D_IF(con, fmt, arg...)			\
	do {							\
		if (con)					\
			pr_debug("DISP/fence " fmt, ##arg);	\
	} while (0)

void mtkfb_fence_log_enable(bool enable)
{
	mtkfb_fence_on = enable;
	MTKFB_FENCE_LOG_D("mtkfb_fence log %s\n",
			  enable ? "enabled" : "disabled");
}

#ifdef CONFIG_MTK_AEE_FEATURE
#  ifndef ASSERT
#  define ASSERT(expr)						\
	do {							\
		if (expr)					\
			break;					\
		pr_debug("FENCE ASSERT FAILED %s, %d\n",	\
			 __FILE__, __LINE__);			\
		aee_kernel_exception("fence", "[FENCE]error:",	\
				     __FILE__, __LINE__);	\
	} while (0)
#  endif
#else /* !CONFIG_MTK_AEE_FEATURE */
#  ifndef ASSERT
#  define ASSERT(expr)						\
	do {							\
		if (expr)					\
			break;					\
		pr_debug("FENCE ASSERT FAILED %s, %d\n",	\
			 __FILE__, __LINE__);			\
	} while (0)
#  endif
#endif /* CONFIG_MTK_AEE_FEATURE */

int fence_clean_up_task_wakeup;

#if defined(MTK_FB_ION_SUPPORT)
static struct ion_client *ion_client;
#endif
/* prepare present fence index */
unsigned int prepare_present_fence_idx[MAX_SESSION_COUNT];

/* how many counters prior to current timeline real-time counter */
#define FENCE_STEP_COUNTER	(1)
#define MTK_FB_NO_ION_FD	((int)(~0U>>1))
#define DISP_SESSION_TYPE(id)	(((id)>>16)&0xff)


static LIST_HEAD(info_pool_head);
static DEFINE_MUTEX(_disp_fence_mutex);
static DEFINE_MUTEX(fence_buffer_mutex);

static struct disp_session_sync_info session_ctx[MAX_SESSION_COUNT];

static struct disp_session_sync_info
*_get_session_sync_info(unsigned int session)
{
	int i = 0, j = 0;
	struct disp_session_sync_info *s_info = NULL;
	struct disp_sync_info *l_info = NULL;
	char name[32];
	const char *prefix = "timeline";

	if (DISP_SESSION_TYPE(session) != DISP_SESSION_PRIMARY &&
	    DISP_SESSION_TYPE(session) != DISP_SESSION_MEMORY &&
	    DISP_SESSION_TYPE(session) != DISP_SESSION_EXTERNAL) {
		DISPPR_ERROR("invalid session id:0x%08x\n", session);
		return NULL;
	}

	mutex_lock(&_disp_fence_mutex);
	for (i = 0; i < ARRAY_SIZE(session_ctx); i++) {
		if (session == session_ctx[i].session_id) {
			/* found */
			s_info = &(session_ctx[i]);
			goto done;
		}
	}

	for (i = 0; i < ARRAY_SIZE(session_ctx); i++) {
		if (session_ctx[i].session_id != 0xffffffff)
			continue;

		DISPMSG("not found session info for session_id:0x%08x,insert %p to array index:%d\n",
			session, &(session_ctx[i]), i);
		session_ctx[i].session_id = session;
		s_info = &(session_ctx[i]);

		scnprintf(name, sizeof(name),
			"%s%d_prepare", disp_session_type_str(session),
			DISP_SESSION_DEV(session));
		dprec_logger_event_init(&s_info->event_prepare, name,
					DPREC_LOGGER_LEVEL_DEFAULT |
					DPREC_LOGGER_LEVEL_SYSTRACE,
					&ddp_mmp_get_events()->session_Parent);
		scnprintf(name, sizeof(name),
			"%s%d_frame_cfg", disp_session_type_str(session),
			DISP_SESSION_DEV(session));
		dprec_logger_event_init(&s_info->event_frame_cfg, name,
					DPREC_LOGGER_LEVEL_DEFAULT,
					&s_info->event_prepare.mmp);

		scnprintf(name, sizeof(name),
			"%s%d_wait_fence", disp_session_type_str(session),
			DISP_SESSION_DEV(session));
		dprec_logger_event_init(&s_info->event_wait_fence, name,
					DPREC_LOGGER_LEVEL_DEFAULT,
					&s_info->event_prepare.mmp);
		scnprintf(name, sizeof(name),
			"%s%d_setinput", disp_session_type_str(session),
			DISP_SESSION_DEV(session));
		dprec_logger_event_init(&s_info->event_setinput, name,
					DPREC_LOGGER_LEVEL_DEFAULT |
					DPREC_LOGGER_LEVEL_SYSTRACE,
					&s_info->event_prepare.mmp);

		scnprintf(name, sizeof(name),
			"%s%d_setoutput", disp_session_type_str(session),
			DISP_SESSION_DEV(session));
		dprec_logger_event_init(&s_info->event_setoutput, name,
					DPREC_LOGGER_LEVEL_DEFAULT |
					DPREC_LOGGER_LEVEL_SYSTRACE,
					&s_info->event_prepare.mmp);

		scnprintf(name, sizeof(name),
			"%s%d_trigger", disp_session_type_str(session),
			DISP_SESSION_DEV(session));
		dprec_logger_event_init(&s_info->event_trigger, name,
					DPREC_LOGGER_LEVEL_DEFAULT |
					DPREC_LOGGER_LEVEL_SYSTRACE,
					&s_info->event_prepare.mmp);

		scnprintf(name, sizeof(name),
			"%s%d_findidx", disp_session_type_str(session),
			DISP_SESSION_DEV(session));
		dprec_logger_event_init(&s_info->event_findidx, name,
					DPREC_LOGGER_LEVEL_DEFAULT,
					&s_info->event_prepare.mmp);

		scnprintf(name, sizeof(name),
			"%s%d_release", disp_session_type_str(session),
			DISP_SESSION_DEV(session));
		dprec_logger_event_init(&s_info->event_release, name,
					DPREC_LOGGER_LEVEL_DEFAULT |
					DPREC_LOGGER_LEVEL_SYSTRACE,
					&s_info->event_prepare.mmp);

		scnprintf(name, sizeof(name),
			"%s%d_waitvsync", disp_session_type_str(session),
			DISP_SESSION_DEV(session));
		dprec_logger_event_init(&s_info->event_waitvsync, name,
					DPREC_LOGGER_LEVEL_DEFAULT |
					DPREC_LOGGER_LEVEL_SYSTRACE,
					&s_info->event_prepare.mmp);

		scnprintf(name, sizeof(name),
			"%s%d_err", disp_session_type_str(session),
			DISP_SESSION_DEV(session));
		dprec_logger_event_init(&s_info->event_err, name,
					DPREC_LOGGER_LEVEL_DEFAULT |
					DPREC_LOGGER_LEVEL_SYSTRACE,
					&s_info->event_prepare.mmp);

		for (j = 0; j < (sizeof(s_info->session_layer_info) /
				 sizeof(s_info->session_layer_info[0])); j++) {

			if (DISP_SESSION_TYPE(session) == DISP_SESSION_PRIMARY)
				scnprintf(name, sizeof(name),
					"%s-primary-%d-%d", prefix,
					DISP_SESSION_DEV(session), j);
			else if (DISP_SESSION_TYPE(session) ==
						DISP_SESSION_EXTERNAL)
				scnprintf(name, sizeof(name),
					"%s-external-%d-%d", prefix,
					DISP_SESSION_DEV(session), j);
			else if (DISP_SESSION_TYPE(session) ==
						DISP_SESSION_MEMORY)
				scnprintf(name, sizeof(name),
					"%s-memory-%d-%d", prefix,
					DISP_SESSION_DEV(session), j);
			else
				scnprintf(name, sizeof(name),
					"%s-unknown-%d-%d", prefix,
					DISP_SESSION_DEV(session), j);

			l_info = &(s_info->session_layer_info[j]);
			mutex_init(&(l_info->sync_lock));
			l_info->layer_id = j;
			l_info->fence_idx = 0;
			l_info->timeline_idx = 0;
			l_info->inc = 0;
			l_info->cur_idx = 0;
			l_info->inited = 1;
			l_info->timeline = timeline_create(name);
			if (l_info->timeline)
				DISPDBG("create timeline success: %s=%p, layer_info=%p\n",
					name, l_info->timeline, l_info);

			INIT_LIST_HEAD(&l_info->buf_list);
		}

		goto done;
	}

done:

	if (!s_info)
		DISPPR_ERROR("wrong session_id:%d, 0x%08x\n", session, session);

	mutex_unlock(&_disp_fence_mutex);
	return s_info;
}

struct disp_session_sync_info
*disp_get_session_sync_info_for_debug(unsigned int session)
{
	return _get_session_sync_info(session);
}

struct disp_sync_info *_get_sync_info(unsigned int session,
					     unsigned int timeline_id)
{
	struct disp_sync_info *l_info = NULL;
	struct disp_session_sync_info *s_info;

	s_info = _get_session_sync_info(session);

	mutex_lock(&_disp_fence_mutex);
	if (s_info) {
		if (timeline_id >= sizeof(s_info->session_layer_info) /
		    sizeof(s_info->session_layer_info[0])) {

			DISPPR_ERROR("invalid timeline_id:%d\n", timeline_id);
			goto done;
		} else {
			l_info = &(s_info->session_layer_info[timeline_id]);
		}
	}

	if (l_info == NULL || s_info == NULL) {
		DISPPR_ERROR("cant get sync info for session_id:0x%08x, timeline_id:%d\n",
			session, timeline_id);
		goto done;
	}

	if (l_info->inited == 0) {
		DISPPR_ERROR("layer_info[%d] not inited\n", timeline_id);
		goto done;
	}

done:
	mutex_unlock(&_disp_fence_mutex);
	return l_info;
}



/* ------------------------------------------------------------------------- */
/* local function declarations */
/* ------------------------------------------------------------------------- */

/******************* ION *************************/
#if defined(MTK_FB_ION_SUPPORT)
static void mtkfb_ion_init(void)
{
	if (!ion_client && g_ion_device)
		ion_client = ion_client_create(g_ion_device, "display");

	if (!ion_client) {
		MTKFB_FENCE_PR_ERR("create ion client failed!\n");
		return;
	}

	MTKFB_FENCE_LOG("create ion client 0x%p\n", ion_client);
}

/**
 * Import ion handle and configure this buffer
 * @client
 * @fd ion shared fd
 * @return ion handle
 */
static struct ion_handle *mtkfb_ion_import_handle(struct ion_client *client,
						  int fd)
{
	struct ion_handle *handle = NULL;
	struct ion_mm_data mm_data;

	/* If no need ION support, do nothing! */
	if (fd == MTK_FB_NO_ION_FD) {
		MTKFB_FENCE_LOG("NO NEED ion support\n");
		return handle;
	}

	if (!ion_client) {
		MTKFB_FENCE_PR_ERR("invalid ion client!\n");
		return handle;
	}
	if (fd == MTK_FB_INVALID_ION_FD) {
		MTKFB_FENCE_PR_ERR("invalid ion fd!\n");
		return handle;
	}
	handle = ion_import_dma_buf_fd(client, fd);
	if (IS_ERR(handle)) {
		MTKFB_FENCE_PR_ERR("import ion handle failed!\n");
		return NULL;
	}
	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.module_id = 0;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 0;

	if (ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA,
			     (unsigned long)&mm_data))
		MTKFB_FENCE_PR_ERR("configure ion buffer failed!\n");

	MTKFB_FENCE_LOG("import ion handle fd=%d,hnd=0x%p\n", fd, handle);

	return handle;
}

static void mtkfb_ion_free_handle(struct ion_client *client,
				  struct ion_handle *handle)
{
	if (!ion_client) {
		MTKFB_FENCE_PR_ERR("invalid ion client!\n");
		return;
	}
	if (!handle)
		return;

	ion_free(client, handle);

	MTKFB_FENCE_LOG("free ion handle 0x%p\n", handle);
}

static size_t mtkfb_ion_phys_mmu_addr(struct ion_client *client,
				      struct ion_handle *handle,
				      unsigned int *mva)
{
	size_t size;
	ion_phys_addr_t phy_addr = 0;

	if (!ion_client) {
		MTKFB_FENCE_PR_ERR("invalid ion client!\n");
		return 0;
	}
	if (!handle)
		return 0;

	ion_phys(client, handle, &phy_addr, &size);
	*mva = (unsigned int)phy_addr;
	MTKFB_FENCE_LOG("alloc mmu addr hnd=0x%p,mva=0x%08x\n",
			handle, (unsigned int)*mva);
	return size;
}

static void mtkfb_ion_cache_flush(struct ion_client *client,
				  struct ion_handle *handle,
				  unsigned long mva, unsigned int size)
{
	struct ion_sys_data sys_data;
	void *va = NULL;

	if (!ion_client || !handle)
		return;

	va = ion_map_kernel(client, handle);
	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle = handle;
	sys_data.cache_sync_param.va = va;
	sys_data.cache_sync_param.size = size;
	sys_data.cache_sync_param.sync_type = ION_CACHE_FLUSH_BY_RANGE;

	if (ion_kernel_ioctl(client, ION_CMD_SYSTEM, (unsigned long)&sys_data))
		MTKFB_FENCE_PR_ERR("ion cache flush failed!\n");

	ion_unmap_kernel(client, handle);
}
#endif /* MTK_FB_ION_SUPPORT */

unsigned int mtkfb_query_buf_mva(unsigned int session_id, unsigned int layer_id,
				 unsigned int idx)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	unsigned int mva = 0x0;
	struct disp_sync_info *l_info = NULL;

	l_info = _get_sync_info(session_id, layer_id);
	if (!l_info) {
		DISPPR_ERROR("layer_info is null\n");
		return 0;
	}

	mutex_lock(&l_info->sync_lock);
	list_for_each_entry(buf, &l_info->buf_list, list) {
		if (buf->idx == idx) {
			mva = buf->mva;
			buf->buf_state = reg_configed;
			break;
		}
	}
	mutex_unlock(&l_info->sync_lock);

	if (!mva) {
		/* FIXME: non-ion buffer need cache sync here? */
		DISPPR_ERROR("can't find session(0x%x) buf, layer=%d, idx=%d, fence_idx=%d, timeline_idx=%d, cur_idx=%d\n",
			session_id, layer_id, idx, l_info->fence_idx,
			l_info->timeline_idx, l_info->cur_idx);
		return mva;
	}

	buf->ts_create = sched_clock();
	if (buf->cache_sync) {
		mmprofile_log_ex(ddp_mmp_get_events()->primary_cache_sync,
				 MMPROFILE_FLAG_START, current->pid, 0);
#if defined(MTK_FB_ION_SUPPORT)
		mtkfb_ion_cache_flush(ion_client, buf->hnd,
				      buf->mva, buf->size);
#endif
		mmprofile_log_ex(ddp_mmp_get_events()->primary_cache_sync,
				 MMPROFILE_FLAG_END, current->pid, 0);
	}
	MTKFB_FENCE_LOG("query buf mva: layer=%d, idx=%d, mva=0x%08x\n",
			layer_id, idx, (unsigned int)(buf->mva));

	return mva;
}

unsigned int mtkfb_query_buf_va(unsigned int session_id, unsigned int layer_id,
				unsigned int idx)
{
	struct mtkfb_fence_buf_info *buf;
	unsigned int va = 0x0;
	struct disp_session_sync_info *s_info;
	struct disp_sync_info *l_info;

	ASSERT(layer_id < DISP_SESSION_TIMELINE_COUNT);

	s_info = _get_session_sync_info(session_id);
	l_info = &(s_info->session_layer_info[layer_id]);
	if (layer_id != l_info->layer_id) {
		MTKFB_FENCE_PR_ERR("wrong layer id %d(rt), %d(in)!\n",
		       l_info->layer_id, layer_id);
		return 0;
	}

	mutex_lock(&l_info->sync_lock);
	list_for_each_entry(buf, &l_info->buf_list, list) {
		if (buf->idx == idx) {
			va = buf->va;
			break;
		}
	}
	mutex_unlock(&l_info->sync_lock);

	if (!va) {
		/* FIXME: non-ion buffer need cache sync here? */
		MTKFB_FENCE_PR_ERR
		    ("cannot find this buf, layer=%d, idx=%d, fence_idx=%d, timeline_idx=%d, cur_idx=%d!\n",
		     layer_id, idx, l_info->fence_idx, l_info->timeline_idx,
		     l_info->cur_idx);
	}

	return va;
}

unsigned int
mtkfb_query_release_idx(unsigned int session_id, unsigned int layer_id,
			unsigned long phy_addr)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	struct mtkfb_fence_buf_info *pre_buf = NULL;
	unsigned int idx = 0x0;
	struct disp_session_sync_info *s_info;
	struct disp_sync_info *l_info;

	s_info = _get_session_sync_info(session_id);
	l_info = &(s_info->session_layer_info[layer_id]);

	if (layer_id != l_info->layer_id) {
		MTKFB_FENCE_PR_ERR("wrong layer id %d(rt), %d(in)!\n",
			l_info->layer_id, layer_id);
		return 0;
	}

	mutex_lock(&l_info->sync_lock);
	list_for_each_entry(buf, &l_info->buf_list, list) {
		if (((buf->mva + buf->mva_offset) == phy_addr) &&
		    (buf->buf_state < reg_updated && buf->buf_state > create)) {
			/* idx = buf->idx; */
			buf->buf_state = reg_updated;
			DISPDBG("mva query1:idx=0x%x, mva=0x%lx, off=%d st %x\n",
				buf->idx, buf->mva, buf->mva_offset,
				buf->buf_state);
		} else if (((buf->mva + buf->mva_offset) != phy_addr) &&
			   (buf->buf_state == reg_updated)) {
			buf->buf_state = read_done;
			DISPDBG("mva query2:idx=0x%x, mva=0x%lx, off=%d st %x\n",
				buf->idx, buf->mva, buf->mva_offset,
				buf->buf_state);
		} else if ((phy_addr == 0) && (buf->buf_state > create)) {
			buf->buf_state = read_done;
		}
		/*
		 * temp solution:
		 * hwc will post same buffer with different idx sometimes.
		 */
		if (pre_buf && ((pre_buf->mva + pre_buf->mva_offset) ==
				(buf->mva + buf->mva_offset)) &&
		    (pre_buf->buf_state == reg_updated)) {
			pre_buf->buf_state = read_done;
			idx = pre_buf->idx;
		}

		if (buf->buf_state == read_done)
			idx = buf->idx;

		pre_buf = buf;

	}
	mutex_unlock(&l_info->sync_lock);
	return idx;
}

unsigned int
mtkfb_update_buf_ticket(unsigned int session_id, unsigned int layer_id,
			unsigned int idx, unsigned int ticket)
{
	struct mtkfb_fence_buf_info *buf;
	unsigned int mva = 0x0;
	struct disp_session_sync_info *s_info;
	struct disp_sync_info *l_info;

	/* ASSERT(layer_id < HW_OVERLAY_COUNT); */
	if (layer_id >= DISP_SESSION_TIMELINE_COUNT) {
		DISPERR("mtkfb_update_buf_state return MVA=0x0 mtkfb_query_buf_mva layer_id %d !(Warning)\n",
			layer_id);
		return mva;
	}

	s_info = _get_session_sync_info(session_id);
	l_info = &(s_info->session_layer_info[layer_id]);

	if (layer_id != l_info->layer_id) {
		DISPERR("wrong layer id %d(rt), %d(in)!\n",
			l_info->layer_id, layer_id);
		return 0;
	}

	mutex_lock(&l_info->sync_lock);
	list_for_each_entry(buf, &l_info->buf_list, list) {
		if (buf->idx == idx) {
			buf->trigger_ticket = ticket;
			break;
		}
	}
	mutex_unlock(&l_info->sync_lock);

	return mva;
}

unsigned int mtkfb_query_idx_by_ticket(unsigned int session_id,
				unsigned int layer_id, unsigned int ticket)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	int idx = -1;
	struct disp_session_sync_info *s_info;
	struct disp_sync_info *l_info;

	s_info = _get_session_sync_info(session_id);
	l_info = &(s_info->session_layer_info[layer_id]);

	if (layer_id != l_info->layer_id) {
		DISPERR("wrong layer id %d(rt), %d(in)!\n",
			l_info->layer_id, layer_id);
		return 0;
	}

	mutex_lock(&l_info->sync_lock);
	list_for_each_entry(buf, &l_info->buf_list, list) {
		if (buf->trigger_ticket == ticket)
			idx = buf->idx;
	}
	mutex_unlock(&l_info->sync_lock);

	return idx;
}

bool mtkfb_update_buf_info_new(unsigned int session_id, unsigned int mva_offset,
			       struct disp_input_config *buf_info)
{
	struct mtkfb_fence_buf_info *buf;
	struct disp_session_sync_info *s_info;
	struct disp_sync_info *l_info;
	unsigned int mva = 0x0;

	if (buf_info->layer_id >= DISP_SESSION_TIMELINE_COUNT) {
		DISPWARN("layer_id %d !(Warning)\n",
			 buf_info->layer_id);
		return mva;
	}

	s_info = _get_session_sync_info(session_id);
	l_info = &(s_info->session_layer_info[buf_info->layer_id]);
	if (buf_info->layer_id != l_info->layer_id) {
		DISPERR("wrong layer id %d(rt), %d(in)!\n",
			l_info->layer_id, buf_info->layer_id);
		return 0;
	}

	mutex_lock(&l_info->sync_lock);
	list_for_each_entry(buf, &l_info->buf_list, list) {
		if (buf->idx == buf_info->next_buff_idx) {
			buf->layer_type = buf_info->layer_type;
			break;
		}
	}
	mutex_unlock(&l_info->sync_lock);

	DISPMSG("mva updaten:session_id=0x%08x, layer_id=%d, %x, mva=0x%lx-%x\n",
		session_id, buf_info->layer_id,	buf_info->next_buff_idx,
		buf->mva, buf_info->layer_type);

	return mva;
}

unsigned int mtkfb_query_buf_info(unsigned int session_id,
				  unsigned int layer_id, unsigned long phy_addr,
				  int query_type)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	struct disp_session_sync_info *s_info;
	struct disp_sync_info *l_info;
	int query_info = 0;

	s_info = _get_session_sync_info(session_id);
	l_info = &(s_info->session_layer_info[layer_id]);
	if (layer_id != l_info->layer_id) {
		DISPERR("wrong layer id %d(rt), %d(in)!\n",
			l_info->layer_id, layer_id);
		return 0;
	}

	mutex_lock(&l_info->sync_lock);
	list_for_each_entry(buf, &l_info->buf_list, list) {
		DISPMSG("mva updatenn layer%d, idx=0x%x, mva=0x%08lx-%x py %lx\n",
			layer_id, buf->idx, buf->mva,
			buf->layer_type, phy_addr);
		if ((buf->mva + buf->mva_offset) == phy_addr) {
			query_info = buf->layer_type;
			mutex_unlock(&l_info->sync_lock);
			return query_info;
		}
	}
	mutex_unlock(&l_info->sync_lock);

	return query_info;
}

bool mtkfb_update_buf_info(unsigned int session_id, unsigned int layer_id,
			   unsigned int idx, unsigned int mva_offset,
			   unsigned int seq)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	bool ret = false;
	struct disp_sync_info *l_info = NULL;

	l_info = _get_sync_info(session_id, layer_id);
	if (!l_info) {
		DISPPR_ERROR("layer_info is null\n");
		return ret;
	}

	mutex_lock(&l_info->sync_lock);
	list_for_each_entry(buf, &l_info->buf_list, list) {
		if (buf->idx != idx)
			continue;

		buf->mva_offset = mva_offset;
		buf->seq = seq;
		ret = true;
		mmprofile_log_ex(ddp_mmp_get_events()->primary_seq_insert,
				 MMPROFILE_FLAG_PULSE,
				 buf->mva + buf->mva_offset, buf->seq);
		break;
	}
	mutex_unlock(&l_info->sync_lock);

	return ret;
}

unsigned int
mtkfb_query_frm_seq_by_addr(unsigned int session_id, unsigned int layer_id,
			    unsigned long phy_addr)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	unsigned int frm_seq = 0x0;
	struct disp_session_sync_info *s_info;
	struct disp_sync_info *l_info;

	if (session_id <= 0)
		return 0;

	s_info = _get_session_sync_info(session_id);
	if (s_info == 0)
		return 0;
	l_info = &(s_info->session_layer_info[layer_id]);

	if (layer_id != l_info->layer_id) {
		MTKFB_FENCE_PR_ERR("wrong layer id %d(rt), %d(in)!\n",
			l_info->layer_id, layer_id);
		return 0;
	}

	mutex_lock(&l_info->sync_lock);
	list_for_each_entry(buf, &l_info->buf_list, list) {
		if (phy_addr > 0) {
			if ((buf->mva + buf->mva_offset) == phy_addr) {
				frm_seq = buf->seq;
				break;
			}
		} else { /* get last buffer's seq */
			if (buf->seq < frm_seq)
				break;

			frm_seq = buf->seq;
		}
	}
	mutex_unlock(&l_info->sync_lock);

	return frm_seq;
}

int disp_sync_init(void)
{
	int i = 0;
	struct disp_session_sync_info *s_info = NULL;

	memset((void *)&session_ctx, 0, sizeof(session_ctx));

	for (i = 0; i < ARRAY_SIZE(session_ctx) ; i++) {
		s_info = &session_ctx[i];
		s_info->session_id = 0xffffffff;
	}

#ifdef MTK_FB_ION_SUPPORT
	mtkfb_ion_init();
#endif
	return 0;
}

struct mtkfb_fence_buf_info
*mtkfb_init_buf_info(struct mtkfb_fence_buf_info *buf)
{
	INIT_LIST_HEAD(&buf->list);
	buf->fence = MTK_FB_INVALID_FENCE_FD;
	buf->hnd = NULL;
	buf->idx = 0;
	buf->mva = 0;
	buf->cache_sync = 0;
	buf->layer_type = 0;
	return buf;
}

/**
 * Query a @mtkfb_fence_buf_info node from @info_pool_head,
 * if empty, create a new one
 */
static struct mtkfb_fence_buf_info *mtkfb_get_buf_info(void)
{
	struct mtkfb_fence_buf_info *buf;

	/*
	 * we must use another mutex for buffer list
	 * because it will be operated by ALL layer info.
	 */
	mutex_lock(&fence_buffer_mutex);
	if (!list_empty(&info_pool_head)) {
		buf = list_first_entry(&info_pool_head,
				       struct mtkfb_fence_buf_info, list);
		list_del_init(&buf->list);
		mtkfb_init_buf_info(buf);
	} else {
		buf = kzalloc(sizeof(struct mtkfb_fence_buf_info), GFP_KERNEL);
		mtkfb_init_buf_info(buf);
		MTKFB_FENCE_LOG("create new mtkfb_fence_buf_info node %p\n",
				buf);
	}
	mutex_unlock(&fence_buffer_mutex);

	return buf;
}

/**
 * signal fence and release buffer
 * layer: set layer
 * fence: signal fence which value is not bigger than this param
 */
void mtkfb_release_fence(unsigned int session, unsigned int layer_id,
			 int fence_idx)
{
	struct mtkfb_fence_buf_info *buf;
	struct mtkfb_fence_buf_info *n;
	int num_fence = 0;
	int current_timeline_idx = 0;
	int ion_release_count = 0;
	struct disp_sync_info *l_info = NULL;
	struct disp_session_sync_info *s_info = NULL;

	s_info = _get_session_sync_info(session);
	l_info = _get_sync_info(session, layer_id);

	if (!l_info) {
		DISPPR_ERROR("layer_info is null\n");
		return;
	}

	if (!l_info->timeline)
		return;

	mutex_lock(&l_info->sync_lock);
	current_timeline_idx = l_info->timeline_idx;
	num_fence = fence_idx - l_info->timeline_idx;

	if (num_fence <= 0) {
		mutex_unlock(&l_info->sync_lock);
		return;
	}

#ifdef DISP_SYSTRACE_BEGIN
	DISP_SYSTRACE_BEGIN("releae_fence:idx%d,layer%d,%s%d\n",
			    fence_idx, layer_id,
			    disp_session_type_str(session),
			    DISP_SESSION_DEV(session));
#endif
	timeline_inc(l_info->timeline, num_fence);
	l_info->timeline_idx = fence_idx;

#ifdef DISP_SYSTRACE_END
	DISP_SYSTRACE_END();
#endif
	if (num_fence >= 2)
		DISPPR_FENCE("Warning, R+/%s%d/L%d/timeline_idx:%d/fence_idx:%d\n",
			  disp_session_type_str(session),
			  DISP_SESSION_DEV(session), layer_id,
			     current_timeline_idx, fence_idx);

	list_for_each_entry_safe(buf, n, &l_info->buf_list, list) {
		if (buf->idx > fence_idx)
			continue;

		l_info->fence_fd = buf->fence;

		list_del_init(&buf->list);
#ifdef MTK_FB_ION_SUPPORT
		if (buf->va && ((DISP_SESSION_TYPE(session) >
				 DISP_SESSION_PRIMARY)))
			ion_unmap_kernel(ion_client, buf->hnd);

		if (buf->hnd)
			mtkfb_ion_free_handle(ion_client, buf->hnd);
#endif
		ion_release_count++;

		/*
		 * we must use another mutex for buffer list
		 * because it will be operated by ALL layer info.
		 */
		mutex_lock(&fence_buffer_mutex);
		list_add_tail(&buf->list, &info_pool_head);
		mutex_unlock(&fence_buffer_mutex);

		buf->ts_period_keep = sched_clock() - buf->ts_create;
		dprec_trigger(&s_info->event_release, buf->idx, layer_id);

		DISPPR_FENCE("R+/%s%d/L%d/idx%d/last%d/new%d/free/idx%d\n",
			  disp_session_type_str(session),
			  DISP_SESSION_DEV(session), layer_id, fence_idx,
			  current_timeline_idx, l_info->fence_idx, buf->idx);
	}

	mutex_unlock(&l_info->sync_lock);

	if (ion_release_count != num_fence)
		DISPPR_ERROR("released %d fence but %d ion handle freed\n",
			num_fence, ion_release_count);
}

void mtkfb_release_layer_fence(unsigned int session_id, unsigned int layer_id)
{
	struct disp_sync_info *l_info = NULL;
	int fence_idx = 0;

	l_info = _get_sync_info(session_id, layer_id);
	if (!l_info) {
		DISPPR_ERROR("layer_info is null\n");
		return;
	}

	mutex_lock(&l_info->sync_lock);
	fence_idx = l_info->fence_idx;
	mutex_unlock(&l_info->sync_lock);

	DISPPR_FENCE("RL+/%s%d/L%d/id%d\n", disp_session_type_str(session_id),
		     DISP_SESSION_DEV(session_id), layer_id, fence_idx);
	/* DISPMSG("layer%d release all fence %d\n", layer_id, fence_idx); */
	mtkfb_release_fence(session_id, layer_id, fence_idx);
}

int mtkfb_release_present_fence(unsigned int session, unsigned int fence_idx)
{
	struct disp_sync_info *l_info = NULL;
	unsigned int timeline_id = 0;
	int fence_increment = 0;

	timeline_id = disp_sync_get_present_timeline_id(session);
	l_info = _get_sync_info(session, timeline_id);
	if (l_info == NULL) {
		DISPERR("layer_info is null\n");
		if (DISP_SESSION_TYPE(session) == DISP_SESSION_PRIMARY)
			mmprofile_log_ex(
			    ddp_mmp_get_events()->primary_present_fence_release,
				       MMPROFILE_FLAG_PULSE, -1, 0x5a5a5a5a);
		return -1;
	}

	mutex_lock(&l_info->sync_lock);
	fence_increment = fence_idx - l_info->timeline->value;

	if (fence_increment >= 2)
		DISPPR_FENCE("Warning, R/%s%d/L%d/timeline idx:%d/fence:%d\n",
			disp_session_type_str(session),
			DISP_SESSION_DEV(session), timeline_id,
			l_info->timeline->value, fence_idx);

	if (fence_increment > 0) {
		timeline_inc(l_info->timeline, fence_increment);
		DISPPR_FENCE("RL+/%s%d/L%d/id%d\n",
		     disp_session_type_str(session),
		     DISP_SESSION_DEV(session), timeline_id, fence_idx);
	}

	if (DISP_SESSION_TYPE(session) == DISP_SESSION_PRIMARY)
		mmprofile_log_ex(
			ddp_mmp_get_events()->primary_present_fence_release,
			MMPROFILE_FLAG_PULSE, fence_idx, fence_increment);

	mutex_unlock(&l_info->sync_lock);

	return 0;
}

void mtkfb_release_present_timeline_fence(unsigned int session_id)
{
	mtkfb_release_present_fence(session_id,
	    prepare_present_fence_idx[DISP_SESSION_TYPE(session_id) - 1]);
}

void mtkfb_release_session_fence(unsigned int session_id)
{
	struct disp_session_sync_info *session_sync_info = NULL;
	int i;

	session_sync_info = _get_session_sync_info(session_id);
	if (session_sync_info == NULL) {
		DISPPR_ERROR("layer_info is null\n");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(session_sync_info->session_layer_info); i++)
		mtkfb_release_layer_fence(session_id, i);
}
int disp_sync_get_ovl_timeline_id(int layer_id)
{
	return DISP_SESSION_OVL_TIMELINE_ID(layer_id);
}

int disp_sync_get_output_timeline_id(void)
{
	return DISP_SESSION_OUTPUT_TIMELINE_ID;
}

int disp_sync_get_output_interface_timeline_id(void)
{
	return DISP_SESSION_OUTPUT_INTERFACE_TIMELINE_ID;
}

int disp_sync_get_present_timeline_id(unsigned int session_id)
{
	if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_PRIMARY)
		return DISP_SESSION_PRIMARY_PRESENT_TIMELINE_ID;
	else if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_EXTERNAL)
		return DISP_SESSION_EXTERNAL_PRESENT_TIMELINE_ID;

	DISPPR_ERROR("session id is wrong, session=0x%x!!\n", session_id);

	return -1;
}

int disp_sync_get_cached_layer_info(unsigned int session_id,
				    unsigned int timeline_idx,
				    unsigned int *layer_en, unsigned long *addr,
				    unsigned int *fence_idx)
{
	int ret = -1;
	struct disp_sync_info *l_info = NULL;

	l_info = _get_sync_info(session_id, timeline_idx);
	if (!l_info) {
		DISPPR_ERROR("layer_info is null\n");
		return 0;
	}

	mutex_lock(&l_info->sync_lock);

	if (layer_en && addr && fence_idx) {
		*layer_en = l_info->cached_config.layer_en;
		*addr = l_info->cached_config.addr;
		*fence_idx = l_info->cached_config.buff_idx;
		ret = 0;
	} else {
		ret = -1;
	}

	mutex_unlock(&l_info->sync_lock);

	return ret;
}

int disp_sync_put_cached_layer_info(unsigned int session_id,
		unsigned int timeline_idx, struct disp_input_config *cfg,
		unsigned long mva)
{
	int ret = -1;
	struct disp_sync_info *l_info = NULL;

	l_info = _get_sync_info(session_id, timeline_idx);
	if (!l_info) {
		DISPPR_ERROR("layer_info is null\n");
		return -1;
	}

	mutex_lock(&l_info->sync_lock);

	if (cfg) {
		disp_sync_convert_input_to_fence_layer_info(cfg,
					&l_info->cached_config, mva);
		ret = 0;
	} else {
		ret = -1;
	}

	mutex_unlock(&l_info->sync_lock);

	return ret;
}

int disp_sync_convert_input_to_fence_layer_info(struct disp_input_config *src,
			struct FENCE_LAYER_INFO *dst, unsigned long dst_mva)
{
	if (!(src && dst))
		return -1;

	dst->layer = src->layer_id;
	dst->addr = dst_mva;
	/* no fence */
	if (src->next_buff_idx == 0) {
		dst->layer_en = 0;
	} else {
		dst->layer_en = src->layer_enable;
		dst->buff_idx = src->next_buff_idx;
	}

	return 0;
}

static int
disp_sync_convert_input_to_fence_layer_info_v2(struct FENCE_LAYER_INFO *dst,
			unsigned int timeline_idx, unsigned int fence_id,
			int layer_en, unsigned long mva)
{
	if (!dst) {
		pr_err("%s error!\n", __func__);
		return -1;
	}

	dst->layer = timeline_idx;
	dst->addr = mva;
	if (fence_id == 0) {
		dst->layer_en = 0;
	} else {
		dst->layer_en = layer_en;
		dst->buff_idx = fence_id;
	}

	return 0;
}

int disp_sync_put_cached_layer_info_v2(unsigned int session_id,
			unsigned int timeline_idx, unsigned int fence_id,
			int layer_en, unsigned long mva)
{
	struct disp_sync_info *l_info = NULL;

	l_info = _get_sync_info(session_id, timeline_idx);

	if (!l_info) {
		DISPPR_ERROR("layer_info is null\n");
		return -1;
	}

	mutex_lock(&(l_info->sync_lock));

	disp_sync_convert_input_to_fence_layer_info_v2(
				&(l_info->cached_config), timeline_idx,
				fence_id, layer_en, mva);

	mutex_unlock(&l_info->sync_lock);

	return 0;
}


static int prepare_ion_buf(struct disp_buffer_info *buf,
			   struct mtkfb_fence_buf_info *buf_info)
{
	unsigned int mva = 0x0;
	struct ion_handle *handle = NULL;

#if defined(MTK_FB_ION_SUPPORT)
	handle = mtkfb_ion_import_handle(ion_client, buf->ion_fd);
	if (handle)
		buf_info->size = mtkfb_ion_phys_mmu_addr(ion_client, handle,
							  &mva);
	else
		DISPPR_ERROR("can't import ion handle for fd:%d\n",
			buf->ion_fd);
#endif

	buf_info->hnd = handle;
	buf_info->mva = mva;
	buf_info->va = 0;
	return 0;
}

/**
 * 1. query a @mtkfb_fence_buf_info list node
 * 2. create fence object
 * 3. create ion mva
 * 4. save fence fd, mva to @mtkfb_fence_buf_info node
 * 5. add @mtkfb_fence_buf_info node to @mtkfb_fence_sync_info.buf_list
 * @buf struct @fb_overlay_buffer
 * @return struct @mtkfb_fence_buf_info
 */
struct mtkfb_fence_buf_info
*disp_sync_prepare_buf(struct disp_buffer_info *buf)
{
	int ret = 0;
	unsigned int session = 0;
	unsigned int timeline_id = 0;
	struct mtkfb_fence_buf_info *buf_info = NULL;
	struct fence_data data;
	struct disp_sync_info *l_info = NULL;
	struct disp_session_sync_info *s_info = NULL;

	if (!buf) {
		DISPPR_ERROR("Prepare Buffer, buf is NULL!!\n");
		return NULL;
	}

	session = buf->session_id;
	timeline_id = buf->layer_id;
	s_info = _get_session_sync_info(session);
	l_info = _get_sync_info(session, timeline_id);
	if (!l_info) {
		DISPPR_ERROR("layer_info is null\n");
		return NULL;
	}

	if (!l_info->inited) {
		DISPPR_ERROR("FATAL ERROR, sync info not inited, session_id=0x%08x|layer_id=%d\n",
			session, timeline_id);
		return NULL;
	}

	dprec_start(&s_info->event_prepare, buf->layer_id,
			buf->ion_fd);

	buf_info = mtkfb_get_buf_info();
	mutex_lock(&l_info->sync_lock);
	data.fence = MTK_FB_INVALID_FENCE_FD;
	data.value = ++(l_info->fence_idx);
	mutex_unlock(&(l_info->sync_lock));

	scnprintf(data.name, sizeof(data.name), "disp-S%x-L%d-%d",
		       session, timeline_id, data.value);

	ret = fence_create(l_info->timeline, &data);
	if (ret) {
		/* Does this really happen? */
		DISPPR_ERROR("%s%d,layer%d create Fence Object failed ret=%d!\n",
			disp_session_type_str(session),
			DISP_SESSION_DEV(session), timeline_id, ret);
	}
	buf_info->fence = data.fence;
	buf_info->idx = data.value;

	if (buf->ion_fd >= 0)
		prepare_ion_buf(buf, buf_info);

	buf_info->mva_offset = 0;
	buf_info->trigger_ticket = 0;
	buf_info->buf_state = create;
	buf_info->cache_sync = buf->cache_sync;
	mutex_lock(&l_info->sync_lock);
	list_add_tail(&buf_info->list, &l_info->buf_list);
	mutex_unlock(&l_info->sync_lock);

	DISPPR_FENCE("P+/%s%d/L%d/id%d/fd%d/mva0x%08lx/size0x%08x/ion_fd:%d\n",
		     disp_session_type_str(session), DISP_SESSION_DEV(session),
		     timeline_id, buf_info->idx, buf_info->fence, buf_info->mva,
		     buf_info->size, buf->ion_fd);

	dprec_submit(&s_info->event_prepare, buf_info->idx, buf_info->mva);
	dprec_submit(&s_info->event_prepare, buf_info->idx, buf_info->size);
	dprec_done(&s_info->event_prepare, buf_info->idx, buf_info->fence);

	return buf_info;
}

int disp_sync_find_fence_idx_by_addr(unsigned int session_id,
			unsigned int timeline_id, unsigned long phy_addr)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	int idx = -1;
	unsigned int layer_en = 0;
	unsigned long addr = 0;
	unsigned int fence_idx = -1;
	struct disp_sync_info *l_info = NULL;
	struct disp_session_sync_info *s_info = NULL;

	s_info = _get_session_sync_info(session_id);
	l_info = _get_sync_info(session_id, timeline_id);

	if (!l_info) {
		DISPPR_ERROR("layer_info is null\n");
		return -1;
	}

	if (l_info->fence_idx == 0)
		return -2;

	disp_sync_get_cached_layer_info(session_id, timeline_id, &layer_en,
					&addr, &fence_idx);

	/* DISPFENCE("F+/%d/0x%08x/%d\n", layer_en, addr, fence_idx); */

	dprec_start(&s_info->event_findidx, layer_en | (timeline_id << 16),
		    fence_idx);
	if (phy_addr) {
		mutex_lock(&l_info->sync_lock);
		list_for_each_entry(buf, &l_info->buf_list, list) {
			/*
			 * Because buf_list stores the lates fence info
			 * from prepare, so we should gurantee only release
			 * the fences that has set inputed.
			 */
			if (buf->idx > fence_idx)
				continue;

			/*
			 * Because we use cached idx as boundary,
			 * so we will continue traverse even the idx
			 * has been found.
			 */
			if ((buf->mva + buf->mva_offset) == phy_addr) {
				if (phy_addr >= buf->mva &&
				    buf->mva + buf->size > phy_addr)
					;
				else
					DISPERR("wrong addr:pa0x%08lx,mva0x%08lx,sz0x%08x\n",
						phy_addr, buf->mva, buf->size);

				idx = buf->idx - 1;
			}
		}
		mutex_unlock(&l_info->sync_lock);
	} else {
		if (!layer_en)
			idx = fence_idx;
		else
			idx = fence_idx - 1;
	}

	dprec_done(&s_info->event_findidx, phy_addr, idx);

	/* DISPFENCE("F/%d\n", idx); */

	return idx;
}

unsigned int disp_sync_buf_cache_sync(unsigned int session_id,
				      unsigned int timeline_id,
				      unsigned int idx)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	struct disp_sync_info *l_info = NULL;
	int found = 0;

	l_info = _get_sync_info(session_id, timeline_id);
	if (!l_info) {
		DISPPR_ERROR("layer_info is null\n");
		return -1;
	}

	mutex_lock(&l_info->sync_lock);

	list_for_each_entry(buf, &l_info->buf_list, list) {
		if (buf->idx != idx)
			continue;

#if defined(MTK_FB_ION_SUPPORT)
		if (buf->cache_sync) {
			dprec_logger_start(DPREC_LOGGER_DISPMGR_CACHE_SYNC,
					   (unsigned long)buf->hnd, buf->mva);
			mtkfb_ion_cache_flush(ion_client, buf->hnd,
					      buf->mva, buf->size);
			dprec_logger_done(DPREC_LOGGER_DISPMGR_CACHE_SYNC,
					  (unsigned long)buf->hnd, buf->mva);
		}
#endif
		found = 1;
		break;
	}
	mutex_unlock(&l_info->sync_lock);
	if (!found) {
		DISPPR_ERROR("cannot find buf for cache sync, session:%s%d, layer=%d,\n",
			disp_session_type_str(session_id),
			DISP_SESSION_DEV(session_id), timeline_id);
		DISPPR_ERROR("idx=%d, fence_idx=%d, timeline_idx=%d, cur_idx=%d!\n",
			idx, l_info->fence_idx, l_info->timeline_idx,
			l_info->cur_idx);
	}

	return 0;
}


unsigned int ___disp_sync_query_buf_info(unsigned int session_id,
			   unsigned int timeline_id,
			   unsigned int fence_idx, unsigned long *mva,
			   unsigned int *size, void **va, int need_sync)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	unsigned long dst_mva = 0;
	uint32_t dst_size = 0;
	struct disp_sync_info *l_info = NULL;

	l_info = _get_sync_info(session_id, timeline_id);
	if (!l_info || !mva || !size) {
		DISPPR_ERROR("layer_info is null, layer_info=%p, mva=%p, size=%p\n",
			l_info, mva, size);
		return -EINVAL;
	}

	mutex_lock(&l_info->sync_lock);
	list_for_each_entry(buf, &l_info->buf_list, list) {
		if (buf->idx == fence_idx) {
			/* use local variable here to avoid polluted pointer */
			dst_mva = buf->mva;
			dst_size = buf->size;

			if (!va)
				continue;

			if (buf->va)
				*va = (void *)buf->va;
			else {
				void *tmp_va;

				tmp_va = ion_map_kernel(ion_client, buf->hnd);
				if (!IS_ERR_OR_NULL(tmp_va)) {
					*va = tmp_va;
					buf->va = (unsigned long)tmp_va;
				}
			}
			break;
		}
	}
	mutex_unlock(&l_info->sync_lock);

	if (dst_mva) {
		*mva = dst_mva;
		*size = dst_size;
		buf->ts_create = sched_clock();
#if defined(MTK_FB_ION_SUPPORT)
		if (buf->cache_sync && need_sync) {
			dprec_logger_start(DPREC_LOGGER_DISPMGR_CACHE_SYNC,
					   (unsigned long)buf->hnd, buf->mva);
			mtkfb_ion_cache_flush(ion_client, buf->hnd,
					      buf->mva, buf->size);
			dprec_logger_done(DPREC_LOGGER_DISPMGR_CACHE_SYNC,
					  (unsigned long)buf->hnd, buf->mva);
		}
#endif
		MTKFB_FENCE_LOG("query buf mva: layer=%d, idx=%d, mva=0x%lx\n",
				timeline_id, fence_idx, buf->mva);
	} else {
		/* FIXME: non-ion buffer need cache sync here? */
		DISPPR_ERROR("cannot find this buf, session:%s%d, layer=%d,\n",
			disp_session_type_str(session_id),
			DISP_SESSION_DEV(session_id), timeline_id);
		DISPPR_ERROR("idx=%d, fence_idx=%d, timeline_idx=%d, cur_idx=%d!\n",
			fence_idx, l_info->fence_idx, l_info->timeline_idx,
			l_info->cur_idx);
	}

	return 0;
}

static unsigned int __disp_sync_query_buf_info(unsigned int session_id,
				      unsigned int timeline_id,
				      unsigned int idx, unsigned long *mva,
				      unsigned int *size, int need_sync)
{
	return ___disp_sync_query_buf_info(session_id, timeline_id, idx, mva,
				size, NULL, need_sync);
}

unsigned int disp_sync_query_buf_info(unsigned int session_id,
				unsigned int timeline_id,
				unsigned int fence_idx, unsigned long *mva,
				unsigned int *size)
{
	return __disp_sync_query_buf_info(session_id, timeline_id,
					  fence_idx, mva, size, 1);
}

unsigned int disp_sync_query_buf_info_nosync(unsigned int session_id,
				unsigned int timeline_id, unsigned int idx,
				unsigned long *mva, unsigned int *size)
{
	return __disp_sync_query_buf_info(session_id, timeline_id,
					  idx, mva, size, 0);
}

int disp_sync_get_debug_info(char *stringbuf, int buf_len)
{
	int len = 0;
	int i = 0;
	int l_id = 0;
	struct disp_session_sync_info *s_info = NULL;
	struct disp_sync_info *l_info = NULL;
	unsigned int session = 0;

	len += scnprintf(stringbuf + len, buf_len - len,
			 "|------------------------------------------------------------------|\n");
	len += scnprintf(stringbuf + len, buf_len - len,
			 "|******* Display Session Information *******\n");

	for (i = 0; i < ARRAY_SIZE(session_ctx); i++) {
		session = session_ctx[i].session_id;
		if (session == -1)
			continue;

		s_info = &session_ctx[i];
		len += scnprintf(stringbuf + len, buf_len - len,
				"|Session id\t0x%08x\n", session);
		for (l_id = 0; l_id < DISP_SESSION_TIMELINE_COUNT; l_id++) {
			l_info = &s_info->session_layer_info[l_id];
			len += scnprintf(stringbuf + len, buf_len - len,
					 "|layerinfo %d\tfence_fd(%d)\tfence_idx(%d)\ttimeline_idx(%d)\n",
					 l_id, l_info->fence_fd,
					 l_info->fence_idx,
					 l_info->timeline_idx);
		}
		len += scnprintf(stringbuf + len, buf_len - len,
				 "|------------------------------------------------------------------|\n");
	}

	return len;
}
