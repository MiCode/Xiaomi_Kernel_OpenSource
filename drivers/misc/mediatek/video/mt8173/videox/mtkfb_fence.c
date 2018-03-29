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

#include "m4u.h"
#include "mtk_sync.h"
#include "debug.h"
#include "ddp_ovl.h"
#include "mtkfb_fence.h"
#include "ddp_path.h"
#include "disp_drv_platform.h"
#include "display_recorder.h"
#include "ddp_mmp.h"
#include "disp_session.h"
#include "mtk_disp_mgr.h"
/************************* log*********************/

static bool mtkfb_fence_on;

#define MTKFB_FENCE_LOG(fmt, arg...) \
	do {	\
		if (mtkfb_fence_on) \
			pr_debug("DISP/fence " fmt, ##arg);	\
	} while (0)
#define MTKFB_FENCE_ERR(fmt, arg...) pr_err("DISP/fence " "error(%d):"fmt, __LINE__, ##arg)

#define MTKFB_FENCE_LOG_D(fmt, arg...) pr_debug("DISP/fence " fmt, ##arg)
#define MTKFB_FENCE_LOG_D_IF(con, fmt, arg...) \
	do {	\
		if (con)	\
			pr_debug("DISP/fence " fmt, ##arg); \
	} while (0)

void mtkfb_fence_log_enable(bool enable)
{
	mtkfb_fence_on = enable;
	MTKFB_FENCE_LOG_D("mtkfb_fence log %s\n", enable ? "enabled" : "disabled");
}

#ifndef ASSERT
#define ASSERT(expr)                                           \
do {                                                           \
	if (expr)	\
		break;                                            \
	pr_err("FENCE ASSERT FAILED %s, %d\n",                       \
	__FILE__, __LINE__); BUG();                            \
} while (0)
#endif

#if defined(MTK_FB_ION_SUPPORT)
static struct ion_client *ion_client;
#endif
/* how many counters prior to current timeline real-time counter */
#define FENCE_STEP_COUNTER         (1)
#define MTK_FB_NO_ION_FD        ((int)(~0U>>1))
#undef HW_OVERLAY_COUNT		/* fix build warning */
#define HW_OVERLAY_COUNT           (5)
/*#define DISP_DEFAULT_UI_LAYER_ID   (HW_OVERLAY_COUNT-1) */
#define DISP_SESSION_TYPE(id) (((id)>>16)&0xff)


static LIST_HEAD(info_pool_head);
static DEFINE_MUTEX(_disp_fence_mutex);
static DEFINE_MUTEX(fence_buffer_mutex);

disp_session_sync_info _disp_fence_context[MAX_SESSION_COUNT];

static disp_session_sync_info *_get_session_sync_info(unsigned int session_id)
{
	int i = 0;
	int j = 0;
	disp_session_sync_info *session_info = NULL;
	disp_sync_info *layer_info = NULL;
	char name[32];
	const char *prefix = "timeline";
	static int _disp_fence_mutex_init;

	if (_disp_fence_mutex_init == 0) {
		mutex_init(&_disp_fence_mutex);
		_disp_fence_mutex_init = 1;
	}

	mutex_lock(&_disp_fence_mutex);
	for (i = 0; i < sizeof(_disp_fence_context) / sizeof(_disp_fence_context[0]); i++) {
		if (session_id == _disp_fence_context[i].session_id) {
			/* DISPMSG("found session info for session_id:0x%08x,0x%08x\n",
			   session_id, &(_disp_fence_context[i])); */
			session_info = &(_disp_fence_context[i]);
			goto done;
		}
	}

	for (i = 0; i < sizeof(_disp_fence_context) / sizeof(_disp_fence_context[0]); i++) {
		if (_disp_fence_context[i].session_id == 0xffffffff) {
			DISPMSG
			    ("not found session info for session_id:0x%08x,insert 0x%08x to array index:%d\n",
			     session_id, (unsigned int)(unsigned long)&(_disp_fence_context[i]), i);
			_disp_fence_context[i].session_id = session_id;
			session_info = &(_disp_fence_context[i]);

			sprintf(name, "%s%d_prepare", disp_session_mode_spy(session_id),
				DISP_SESSION_DEV(session_id));
			dprec_logger_event_init(&session_info->event_prepare, name,
						DPREC_LOGGER_LEVEL_DEFAULT |
						DPREC_LOGGER_LEVEL_SYSTRACE,
						(MMP_Event *) (unsigned long)
						ddp_mmp_get_events()->session_Parent);

			sprintf(name, "%s%d_setinput", disp_session_mode_spy(session_id),
				DISP_SESSION_DEV(session_id));
			dprec_logger_event_init(&session_info->event_setinput, name,
						DPREC_LOGGER_LEVEL_DEFAULT |
						DPREC_LOGGER_LEVEL_SYSTRACE,
						(MMP_Event *) (unsigned long)
						ddp_mmp_get_events()->session_Parent);

			sprintf(name, "%s%d_setoutput", disp_session_mode_spy(session_id),
				DISP_SESSION_DEV(session_id));
			dprec_logger_event_init(&session_info->event_setoutput, name,
						DPREC_LOGGER_LEVEL_DEFAULT |
						DPREC_LOGGER_LEVEL_SYSTRACE,
						(MMP_Event *) (unsigned long)
						ddp_mmp_get_events()->session_Parent);

			sprintf(name, "%s%d_trigger", disp_session_mode_spy(session_id),
				DISP_SESSION_DEV(session_id));
			dprec_logger_event_init(&session_info->event_trigger, name,
						DPREC_LOGGER_LEVEL_DEFAULT |
						DPREC_LOGGER_LEVEL_SYSTRACE,
						(MMP_Event *) (unsigned long)
						ddp_mmp_get_events()->session_Parent);

			sprintf(name, "%s%d_findidx", disp_session_mode_spy(session_id),
				DISP_SESSION_DEV(session_id));
			dprec_logger_event_init(&session_info->event_findidx, name,
						DPREC_LOGGER_LEVEL_DEFAULT,
						(MMP_Event *) (unsigned long)
						ddp_mmp_get_events()->session_Parent);

			sprintf(name, "%s%d_release", disp_session_mode_spy(session_id),
				DISP_SESSION_DEV(session_id));
			dprec_logger_event_init(&session_info->event_release, name,
						DPREC_LOGGER_LEVEL_DEFAULT |
						DPREC_LOGGER_LEVEL_SYSTRACE,
						(MMP_Event *) (unsigned long)
						ddp_mmp_get_events()->session_Parent);

			sprintf(name, "%s%d_waitvsync", disp_session_mode_spy(session_id),
				DISP_SESSION_DEV(session_id));
			dprec_logger_event_init(&session_info->event_waitvsync, name,
						DPREC_LOGGER_LEVEL_DEFAULT |
						DPREC_LOGGER_LEVEL_SYSTRACE,
						(MMP_Event *) (unsigned long)
						ddp_mmp_get_events()->session_Parent);

			for (j = 0;
			     j <
			     (sizeof(session_info->session_layer_info) /
			      sizeof(session_info->session_layer_info[0])); j++) {
				if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_PRIMARY) {
					sprintf(name, "%s-primary-%d-%d", prefix,
						DISP_SESSION_DEV(session_id), j);
				} else if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_EXTERNAL) {
					sprintf(name, "%s-external-%d-%d", prefix,
						DISP_SESSION_DEV(session_id), j);
				} else if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_MEMORY) {
					sprintf(name, "%s-memory-%d-%d", prefix,
						DISP_SESSION_DEV(session_id), j);
				} else {
					sprintf(name, "%s-unknown-%d-%d", prefix,
						DISP_SESSION_DEV(session_id), j);
				}

				layer_info = &(session_info->session_layer_info[j]);
				mutex_init(&(layer_info->sync_lock));
				layer_info->layer_id = j;
				layer_info->fence_idx = 0;
				layer_info->timeline_idx = 0;
				layer_info->inc = 0;
				layer_info->cur_idx = 0;
				layer_info->inited = 1;
				#ifdef CONFIG_MTK_SYNC
				layer_info->timeline =
				    (struct sw_sync_timeline *)timeline_create(name);
				#endif
				if (layer_info->timeline) {
					DISPMSG
					    ("create timeline success: %s=0x%p, layer_info=0x%p\n",
					     name, layer_info->timeline, layer_info);
				}

				INIT_LIST_HEAD(&layer_info->buf_list);
			}

			goto done;
		}
	}

done:

	if (session_info == NULL)
		DISPERR("wrong session_id:%d, 0x%08x\n", session_id, session_id);


	mutex_unlock(&_disp_fence_mutex);
	return session_info;
}

disp_session_sync_info *disp_get_session_sync_info_for_debug(unsigned int session_id)
{
	return _get_session_sync_info(session_id);
}

disp_sync_info *_get_sync_info(unsigned int session_id, unsigned int timeline_id)
{
	disp_sync_info *layer_info = NULL;

	disp_session_sync_info *session_info = _get_session_sync_info(session_id);

	mutex_lock(&_disp_fence_mutex);
	if (session_info) {
		if (timeline_id >=
		    sizeof(session_info->session_layer_info) /
		    sizeof(session_info->session_layer_info[0])) {
			DISPERR("invalid timeline_id:%d\n", timeline_id);
			goto done;
		} else {
			layer_info = &(session_info->session_layer_info[timeline_id]);
		}
	}

	if (layer_info == NULL || session_info == NULL) {
		DISPERR("cant get sync info for session_id:0x%08x, timeline_id:%d\n", session_id,
			timeline_id);
		goto done;
	}

	if (layer_info->inited == 0) {
		DISPERR("layer_info[%d] not inited\n", timeline_id);
		goto done;
	}

done:

	mutex_unlock(&_disp_fence_mutex);
	return layer_info;
}

/* --------------------------------------------------------------------------- */
/* local function declarations */
/* --------------------------------------------------------------------------- */


/********************ION*****************************************************/

#if defined(MTK_FB_ION_SUPPORT)
static void mtkfb_ion_init(void)
{
	if (!ion_client && g_ion_device)
		ion_client = ion_client_create(g_ion_device, "display");


	if (!ion_client) {
		MTKFB_FENCE_ERR("create ion client failed!\n");
		return;
	}

	DISPMSG("create ion client 0x%p\n", ion_client);
}

#if 0
static void mtkfb_ion_deinit(void)
{
	if (ion_client) {
		ion_client_destroy(ion_client);
		ion_client = NULL;
		MTKFB_FENCE_LOG("destroy ion client 0x%p\n", ion_client);
	}
}
#endif

/**
 * Import ion handle and configure this buffer
 * @client
 * @fd ion shared fd
 * @return ion handle
 */
static struct ion_handle *mtkfb_ion_import_handle(struct ion_client *client, int fd)
{
	struct ion_handle *handle = NULL;
	struct ion_mm_data mm_data;
	/* If no need Ion support, do nothing! */
	if (fd == MTK_FB_NO_ION_FD) {
		MTKFB_FENCE_LOG("NO NEED ion support\n");
		return handle;
	}

	if (!ion_client) {
		MTKFB_FENCE_ERR("invalid ion client!\n");
		return handle;
	}
	if (fd == MTK_FB_INVALID_ION_FD) {
		MTKFB_FENCE_ERR("invalid ion fd!\n");
		return handle;
	}
	handle = ion_import_dma_buf(client, fd);
	if (IS_ERR_OR_NULL(handle)) {
		MTKFB_FENCE_ERR("import ion handle failed!\n");
		return handle;
	}
	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.eModuleID = 0;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 0;
/*
	mm_data.config_buffer_param.kernel_handle = handle;
    mm_data.config_buffer_param.m4u_port = M4U_PORT_DISP_OVL0;
	mm_data.config_buffer_param.prot = M4U_PROT_READ|M4U_PROT_WRITE;
	mm_data.config_buffer_param.flags = M4U_FLAGS_SEQ_ACCESS;
*/
	if (ion_kernel_ioctl(ion_client, ION_CMD_MULTIMEDIA, (unsigned long)&mm_data))
		MTKFB_FENCE_ERR("configure ion buffer failed!\n");

	MTKFB_FENCE_LOG("import ion handle fd=%d,hnd=0x%p\n", fd, handle);

	return handle;
}

static void mtkfb_ion_free_handle(struct ion_client *client, struct ion_handle *handle)
{
	if (!ion_client) {
		MTKFB_FENCE_ERR("invalid ion client!\n");
		return;
	}
	if (IS_ERR_OR_NULL(handle))
		return;

	ion_free(client, handle);
	MTKFB_FENCE_LOG("free ion handle 0x%p\n", handle);
}

static size_t mtkfb_ion_phys_mmu_addr(struct ion_client *client, struct ion_handle *handle,
				      unsigned int *mva)
{
	size_t size;

	if (!ion_client) {
		MTKFB_FENCE_ERR("invalid ion client!\n");
		return 0;
	}
	if (IS_ERR_OR_NULL(handle))
		return 0;

	ion_phys(client, handle, (ion_phys_addr_t *) mva, &size);
	MTKFB_FENCE_LOG("alloc mmu addr hnd=0x%p,mva=0x%08x\n", handle, (unsigned int)*mva);
	return size;
}

static void mtkfb_ion_cache_flush(struct ion_client *client, struct ion_handle *handle)
{
	struct ion_sys_data sys_data;

	if (!ion_client || IS_ERR_OR_NULL(handle))
		return;

	sys_data.sys_cmd = ION_SYS_CACHE_SYNC;
	sys_data.cache_sync_param.kernel_handle = handle;
	sys_data.cache_sync_param.sync_type = ION_CACHE_FLUSH_BY_RANGE;
	if (ion_kernel_ioctl(client, ION_CMD_SYSTEM, (unsigned long)&sys_data))
		MTKFB_FENCE_ERR("ion cache flush failed!\n");
}
#endif				/* #if defined (MTK_FB_ION_SUPPORT) */

unsigned int mtkfb_query_buf_mva(unsigned int session_id, unsigned int layer_id, unsigned int idx)
{
	struct mtkfb_fence_buf_info *buf;
	unsigned int mva = 0x0;
	/* ASSERT(layer_id < HW_OVERLAY_COUNT); */
	disp_sync_info *layer_info = NULL;

	layer_info = _get_sync_info(session_id, layer_id);

	if (layer_info == NULL) {
		DISPERR("layer_info is null\n");
		return 0;
	}

	mutex_lock(&layer_info->sync_lock);
	list_for_each_entry(buf, &layer_info->buf_list, list) {
		if (buf->idx == idx) {
			mva = buf->mva;
			buf->buf_state = reg_configed;
			break;
		}
	}
	mutex_unlock(&layer_info->sync_lock);
	if (mva != 0x0) {
		buf->ts_create = sched_clock();
		if (buf->cache_sync) {
			/* xuecheng, for debug */
			/* DISPMSG("attention!! we will do ion_cache_flush!!!\n"); */

			dprec_logger_start(DPREC_LOGGER_DISPMGR_CACHE_SYNC,
					   (unsigned int)(unsigned long)buf->hnd, buf->mva);

#if defined(MTK_FB_ION_SUPPORT)
			mtkfb_ion_cache_flush(ion_client, buf->hnd);
#endif
			dprec_logger_done(DPREC_LOGGER_DISPMGR_CACHE_SYNC,
					  (unsigned int)(unsigned long)buf->hnd, buf->mva);
		}
		MTKFB_FENCE_LOG("query buf mva: layer=%d, idx=%d, mva=0x%08x\n", layer_id, idx,
				buf->mva);
	} else {
		/* FIXME: non-ion buffer need cache sync here? */
		DISPERR
		    ("cannot find session(0x%x) buf, layer=%d, idx=%d, fence_idx=%d, timeline_idx=%d, cur_idx=%d!\n",
		     session_id, layer_id, idx, layer_info->fence_idx, layer_info->timeline_idx,
		     layer_info->cur_idx);
	}

	/* DISPMSG("mva query:session_id=0x%08x, layer_id=%d, mva=0x%08x\n", session_id, layer_id, mva); */

	return mva;
}

unsigned int mtkfb_query_buf_va(unsigned int session_id, unsigned int layer_id, unsigned int idx)
{
	struct mtkfb_fence_buf_info *buf;
	unsigned int va = 0x0;
	disp_session_sync_info *session_info;
	disp_sync_info *layer_info;

	ASSERT(layer_id < HW_OVERLAY_COUNT);

	session_info = _get_session_sync_info(session_id);
	layer_info = &(session_info->session_layer_info[layer_id]);
	if (layer_id != layer_info->layer_id) {
		MTKFB_FENCE_ERR("wrong layer id %d(rt), %d(in)!\n", layer_info->layer_id, layer_id);
		return 0;
	}

	mutex_lock(&layer_info->sync_lock);
	list_for_each_entry(buf, &layer_info->buf_list, list) {
		if (buf->idx == idx) {
			va = buf->va;
			break;
		}
	}
	mutex_unlock(&layer_info->sync_lock);
	if (va == 0x0) {
		/* FIXME: non-ion buffer need cache sync here? */
		MTKFB_FENCE_ERR
		    ("cannot find this buf, layer=%d, idx=%d, fence_idx=%d, timeline_idx=%d, cur_idx=%d!\n",
		     layer_id, idx, layer_info->fence_idx, layer_info->timeline_idx,
		     layer_info->cur_idx);
	}
	/* DISPMSG("mva query:session_id=0x%08x, layer_id=%d, mva=0x%08x\n", session_id, layer_id, mva); */

	return va;
}

unsigned int mtkfb_query_release_idx(unsigned int session_id, unsigned int layer_id,
				     unsigned int phy_addr)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	struct mtkfb_fence_buf_info *pre_buf = NULL;
	unsigned int idx = 0x0;

	disp_session_sync_info *session_info = _get_session_sync_info(session_id);
	disp_sync_info *layer_info = &(session_info->session_layer_info[layer_id]);

	if (layer_id != layer_info->layer_id) {
		MTKFB_FENCE_ERR("wrong layer id %d(rt), %d(in)!\n", layer_info->layer_id, layer_id);
		return 0;
	}

	mutex_lock(&layer_info->sync_lock);
	list_for_each_entry(buf, &layer_info->buf_list, list) {
		if (((buf->mva + buf->mva_offset) == phy_addr)
		    && (buf->buf_state < reg_updated && buf->buf_state > create)) {
			/* /idx = buf->idx; */
			buf->buf_state = reg_updated;
			DISPDBG("mva query1:idx=0x%x, mva=0x%x, off=%d st %x\n", buf->idx, buf->mva,
				buf->mva_offset, buf->buf_state);
		} else if (((buf->mva + buf->mva_offset) != phy_addr)
			   && (buf->buf_state == reg_updated)) {
			buf->buf_state = read_done;
			DISPDBG("mva query2:idx=0x%x, mva=0x%x, off=%d st %x\n", buf->idx, buf->mva,
				buf->mva_offset, buf->buf_state);
		} else if ((phy_addr == 0) && (buf->buf_state > create)) {
			buf->buf_state = read_done;
		}
		/* temp solution:  hwc will post same buffer with different idx sometimes. */
		if (pre_buf
		    && ((pre_buf->mva + pre_buf->mva_offset) == (buf->mva + buf->mva_offset))
		    && (pre_buf->buf_state == reg_updated)) {
			pre_buf->buf_state = read_done;
			idx = pre_buf->idx;
		}

		if (buf->buf_state == read_done)
			idx = buf->idx;


		pre_buf = buf;

	}
	mutex_unlock(&layer_info->sync_lock);
	return idx;
}


unsigned int mtkfb_update_buf_ticket(unsigned int session_id, unsigned int layer_id,
				     unsigned int idx, unsigned int ticket)
{
	struct mtkfb_fence_buf_info *buf;
	unsigned int mva = 0x0;
	disp_session_sync_info *session_info;
	disp_sync_info *layer_info;
	/* ASSERT(layer_id < HW_OVERLAY_COUNT); */
	if (layer_id >= HW_OVERLAY_COUNT) {
		DISPERR
		    ("mtkfb_update_buf_state return MVA=0x0 mtkfb_query_buf_mva layer_id %d !!!!!!!!(Warning)\n",
		     layer_id);
		return mva;
	}

	session_info = _get_session_sync_info(session_id);
	layer_info = &(session_info->session_layer_info[layer_id]);
	if (layer_id != layer_info->layer_id) {
		DISPERR("wrong layer id %d(rt), %d(in)!\n", layer_info->layer_id, layer_id);
		return 0;
	}

	mutex_lock(&layer_info->sync_lock);
	list_for_each_entry(buf, &layer_info->buf_list, list) {
		if (buf->idx == idx) {
			buf->trigger_ticket = ticket;
			break;
		}
	}

	mutex_unlock(&layer_info->sync_lock);

	/* DISPMSG("mva update:session_id=0x%08x, layer_id=%d, mva=0x%08x\n", session_id, layer_id, mva); */

	return mva;
}

unsigned int mtkfb_query_idx_by_ticket(unsigned int session_id, unsigned int layer_id,
				       unsigned int ticket)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	/* struct mtkfb_fence_buf_info *pre_buf = NULL; */
	int idx = -1;

	disp_session_sync_info *session_info = _get_session_sync_info(session_id);
	disp_sync_info *layer_info = &(session_info->session_layer_info[layer_id]);

	if (layer_id != layer_info->layer_id) {
		DISPERR("wrong layer id %d(rt), %d(in)!\n", layer_info->layer_id, layer_id);
		return 0;
	}

	mutex_lock(&layer_info->sync_lock);
	list_for_each_entry(buf, &layer_info->buf_list, list) {
		if (buf->trigger_ticket == ticket) {
			/* /DISPMSG("mva query1 layer%d, idx=0x%x, mva=0x%x, off=%d st %x\n",
			   layer_id, buf->idx, buf->mva, buf->mva_offset, buf->buf_state); */
			idx = buf->idx;
		}
	}
	mutex_unlock(&layer_info->sync_lock);

	return idx;
}

#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
bool mtkfb_update_buf_info(unsigned int session_id, unsigned int layer_id, unsigned int idx,
			   unsigned int mva_offset, unsigned int secure_handle, unsigned int isSecure)
{
	struct mtkfb_fence_buf_info *buf;
	bool ret = false;

	disp_sync_info *layer_info = NULL;

	layer_info = _get_sync_info(session_id, layer_id);

	if (layer_info == NULL) {
		DISPERR("layer_info is null\n");
		return false;
	}
	/* DISPPR_FENCE("U+/0x%08x/%d/%d/0x%08x\n", session_id, layer_id, idx, mva_offset); */
	mutex_lock(&layer_info->sync_lock);
	list_for_each_entry(buf, &layer_info->buf_list, list) {
		if (buf->idx == idx && (!buf->secure_handle)) {
			buf->mva_offset = mva_offset;
			buf->secure_handle = secure_handle;
			buf->isScure = isSecure;
			ret = true;
			if (isSecure) {
			#ifdef CONFIG_MTK_HDMI_SUPPORT
				TZ_RESULT res = KREE_ReferenceSecurechunkmem(
					(DISP_SESSION_TYPE(session_id) == DISP_SESSION_PRIMARY) ?
					primary_display_secure_memory_session_handle() :
					extd_disp_secure_memory_session_handle(), secure_handle);
			#else
				TZ_RESULT res = KREE_ReferenceSecurechunkmem(
					primary_display_secure_memory_session_handle(), secure_handle);
			#endif
				if (res != TZ_RESULT_SUCCESS)
					DISPERR("KREE_ReferenceSecurechunkmem failed (%s), sec_handle 0x%x\n",
						TZ_GetErrorString(res), secure_handle);
			}
			break;
		}
	}

	mutex_unlock(&layer_info->sync_lock);

	return ret;
}

#else

bool mtkfb_update_buf_info(unsigned int session_id, unsigned int layer_id, unsigned int idx,
			   unsigned int mva_offset)
{
	struct mtkfb_fence_buf_info *buf;
	bool ret = false;

	disp_sync_info *layer_info = NULL;

	layer_info = _get_sync_info(session_id, layer_id);

	if (layer_info == NULL) {
		DISPERR("layer_info is null\n");
		return false;
	}
	/* DISPPR_FENCE("U+/0x%08x/%d/%d/0x%08x\n", session_id, layer_id, idx, mva_offset); */

	mutex_lock(&layer_info->sync_lock);
	list_for_each_entry(buf, &layer_info->buf_list, list) {
		if (buf->idx == idx) {
			buf->mva_offset = mva_offset;
			ret = true;
			break;
		}
	}

	mutex_unlock(&layer_info->sync_lock);

	return ret;
}
#endif

#if 0
static int _sync_inited(disp_session_sync_info *info, unsigned int layer_id)
{
	if (info && info->session_id) {
		if (info->session_layer_info[layer_id].inited)
			return 1;
		else
			return 0;
	} else {
		return 0;
	}
}
#endif

int disp_sync_init(void)
{
	int i = 0;
	/* int j = 0; */
	disp_session_sync_info *session_info = NULL;
	/* disp_sync_info *layer_info = NULL; */
	/* char name[32]; */
	static int disp_sync_is_init;

	if (disp_sync_is_init)
		return 0;

	memset((void *)&_disp_fence_context, 0, sizeof(_disp_fence_context));
	for (i = 0; i < sizeof(_disp_fence_context) / sizeof(_disp_fence_context[0]); i++) {
		session_info = &_disp_fence_context[i];
		session_info->session_id = 0xffffffff;
	}

	DISPDBG("present fence timeline idx = %d\n", disp_sync_get_present_timeline_id());
	DISPDBG("output fence timeline idx = %d\n", disp_sync_get_output_timeline_id());

#if defined(MTK_FB_ION_SUPPORT)
	mtkfb_ion_init();
#endif
	disp_sync_is_init = 1;

	return 0;
}


struct mtkfb_fence_buf_info *mtkfb_init_buf_info(struct mtkfb_fence_buf_info *buf)
{
	INIT_LIST_HEAD(&buf->list);
	buf->fence = MTK_FB_INVALID_FENCE_FD;
	buf->hnd = NULL;
	buf->idx = 0;
	buf->mva = 0;
	buf->cache_sync = 0;
	return buf;
}

/**
 * Query a @mtkfb_fence_buf_info node from @info_pool_head, if empty create a new one
 */
static struct mtkfb_fence_buf_info *mtkfb_get_buf_info(void)
{
	struct mtkfb_fence_buf_info *info;

	/* we must use another mutex for buffer list because it will be operated by ALL layer info. */
	mutex_lock(&fence_buffer_mutex);
	if (!list_empty(&info_pool_head)) {
		info = list_first_entry(&info_pool_head, struct mtkfb_fence_buf_info, list);
		list_del_init(&info->list);
		mtkfb_init_buf_info(info);
		mutex_unlock(&fence_buffer_mutex);
		return info;

	} else {
		info = kzalloc(sizeof(struct mtkfb_fence_buf_info), GFP_KERNEL);
		mtkfb_init_buf_info(info);
		MTKFB_FENCE_LOG("create new mtkfb_fence_buf_info node 0x%p\n", info);
		mutex_unlock(&fence_buffer_mutex);
		return info;
	}


}

/**
 * signal fence and release buffer
 * layer: set layer
 * fence: signal fence which value is not bigger than this param
 */
void mtkfb_release_fence(unsigned int session_id, unsigned int layer_id, int fence)
{
	struct mtkfb_fence_buf_info *buf;
	struct mtkfb_fence_buf_info *n;
	int num_fence = 0;
	int current_timeline_idx = 0;
	int ion_release_count = 0;

	/* unsigned int current_ticket = dprec_get_vsync_count(); */

	disp_sync_info *layer_info = NULL;
	disp_session_sync_info *session_info = NULL;

	session_info = _get_session_sync_info(session_id);
	layer_info = _get_sync_info(session_id, layer_id);

	if (layer_info == NULL) {
		DISPERR("layer_info is null\n");
		return;
	}

	if (layer_info->timeline != NULL) {
		mutex_lock(&layer_info->sync_lock);
		current_timeline_idx = layer_info->timeline_idx;
		num_fence = fence - layer_info->timeline_idx;
		if (num_fence > 0) {
			#ifdef CONFIG_MTK_SYNC
			timeline_inc((struct sw_sync_timeline *)layer_info->timeline, num_fence);
			#endif
			layer_info->timeline_idx = fence;
		} else {
			mutex_unlock(&layer_info->sync_lock);
			/* DISPPR_FENCE("R+/%s%d/L%d/id%d/last%d/new%d\n",
			   disp_session_mode_spy(session_id), DISP_SESSION_DEV(session_id),
			   layer_id, fence, current_timeline_idx, layer_info->fence_idx); */
			return;
		}


		list_for_each_entry_safe(buf, n, &layer_info->buf_list, list) {
			if (buf->idx <= fence) {
				layer_info->fence_fd = buf->fence;

				list_del_init(&buf->list);
#if defined(MTK_FB_ION_SUPPORT)
				if (buf->va
				    && ((DISP_SESSION_TYPE(session_id) > DISP_SESSION_PRIMARY))) {
					ion_unmap_kernel(ion_client, buf->hnd);
				}

				mtkfb_ion_free_handle(ion_client, buf->hnd);
				ion_release_count++;
#endif
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
				if (buf->isScure) {
				#ifdef CONFIG_MTK_HDMI_SUPPORT
					TZ_RESULT res = KREE_UnreferenceSecurechunkmem(
						(DISP_SESSION_TYPE(session_id) == DISP_SESSION_PRIMARY) ?
						primary_display_secure_memory_session_handle() :
						extd_disp_secure_memory_session_handle(), buf->secure_handle);
				#else
					TZ_RESULT res = KREE_UnreferenceSecurechunkmem(
						primary_display_secure_memory_session_handle(), buf->secure_handle);
				#endif
					if (res != TZ_RESULT_SUCCESS)
						DISPERR("KREE_UnreferenceSecurechunkmem failed (%s), sec_handle 0x%x\n",
							TZ_GetErrorString(res), buf->secure_handle);
				}
#endif
				/* we must use another mutex for buffer list
				   because it will be operated by ALL layer info. */
				mutex_lock(&fence_buffer_mutex);
				list_add_tail(&buf->list, &info_pool_head);
				mutex_unlock(&fence_buffer_mutex);
				buf->ts_period_keep = sched_clock() - buf->ts_create;
				dprec_trigger(&session_info->event_release, buf->idx, layer_id);
				/* DISPMSG("buf->idx=%d,ts_create=%lld,ts_period_keep=%lld\n",
				   buf->idx, buf->ts_create, buf->ts_period_keep); */
				DISPPR_FENCE("R+/%s%d/L%d/id%d/last%d/new%d/free/idx%d\n",
					     disp_session_mode_spy(session_id),
					     DISP_SESSION_DEV(session_id), layer_id, fence,
					     current_timeline_idx, layer_info->fence_idx, buf->idx);

				if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_EXTERNAL
				    && layer_id < 4) {
					MMProfileLogEx(ddp_mmp_get_events()->Extd_fence_release
						       [layer_id], MMProfileFlagPulse, buf->idx,
						       fence);
				}
			}
		}

		mutex_unlock(&layer_info->sync_lock);

		if (ion_release_count != num_fence) {
			DISPERR("released %d fence but %d ion handle freed\n", num_fence,
				ion_release_count);
		}

	}
}

void mtkfb_release_layer_fence(unsigned int session_id, unsigned int layer_id)
{
	disp_sync_info *layer_info = NULL;
	int fence = 0;

	layer_info = _get_sync_info(session_id, layer_id);

	if (layer_info == NULL) {
		DISPERR("layer_info is null\n");
		return;
	}

	mutex_lock(&layer_info->sync_lock);
	fence = layer_info->fence_idx;
	mutex_unlock(&layer_info->sync_lock);

	DISPPR_FENCE("RL+/%s%d/L%d/id%d\n", disp_session_mode_spy(session_id),
		     DISP_SESSION_DEV(session_id), layer_id, fence);
	/* DISPMSG("layer%d release all fence %d\n", layer_id, fence); */
	mtkfb_release_fence(session_id, layer_id, fence);
}

#define NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_NEW_
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

int disp_sync_get_present_timeline_id(void)
{
	return DISP_SESSION_PRESENT_TIMELINE_ID;
}


int disp_sync_get_cached_layer_info(unsigned int session_id, unsigned int timeline_idx,
				    unsigned int *layer_en, unsigned int *addr,
				    unsigned int *fence_idx)
{
	int ret = -1;
	disp_sync_info *layer_info = NULL;

	layer_info = _get_sync_info(session_id, timeline_idx);

	if (layer_info == NULL) {
		DISPERR("layer_info is null\n");
		return 0;
	}

	mutex_lock(&(layer_info->sync_lock));

	if (layer_en && addr && fence_idx) {
		*layer_en = layer_info->cached_config.layer_en;
		*addr = layer_info->cached_config.addr;
		*fence_idx = layer_info->cached_config.buff_idx;
		ret = 0;
		goto done;
	} else {
		ret = -1;
		goto done;
	}

done:
	mutex_unlock(&(layer_info->sync_lock));

	return ret;
}

int disp_sync_put_cached_layer_info(unsigned int session_id, unsigned int timeline_idx,
				    disp_input_config *src, unsigned int mva)
{
	int ret = -1;
	disp_sync_info *layer_info = NULL;

	layer_info = _get_sync_info(session_id, timeline_idx);

	if (layer_info == NULL) {
		DISPERR("layer_info is null\n");
		return -1;
	}

	if (src)
		DISPPR_FENCE
			 ("S+/L%d/e%d/id%d/%dx%d(%d,%d)(%d,%d)/%s/%d/0x%lx/mva0x%08x\n",
		     src->layer_id,
		     src->layer_enable,
		     src->next_buff_idx,
		     src->src_width,
		     src->src_height,
		     src->src_offset_x,
		     src->src_offset_y,
		     src->tgt_offset_x,
		     src->tgt_offset_y,
		     _disp_format_spy(src->src_fmt), src->src_pitch, (unsigned long)src->src_phy_addr, mva);

	mutex_lock(&(layer_info->sync_lock));

	if (src) {
		disp_sync_convert_input_to_fence_layer_info(src, &(layer_info->cached_config), mva);

		ret = 0;
		goto done;
	} else {
		ret = -1;
		goto done;
	}

done:
	mutex_unlock(&(layer_info->sync_lock));

	return ret;

}

int disp_sync_convert_input_to_fence_layer_info(disp_input_config *src, FENCE_LAYER_INFO *dst,
						unsigned int dst_mva)
{
	if (src && dst) {
		dst->layer = src->layer_id;
		dst->layer_en = src->layer_enable;
		dst->addr = dst_mva;
		dst->buff_idx = src->next_buff_idx;

		return 0;
	} else {
		return -1;
	}
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
struct mtkfb_fence_buf_info *disp_sync_prepare_buf(disp_buffer_info *buf)
{
#ifdef CONFIG_MTK_SYNC
	int ret = 0;
#endif
	unsigned int mva = 0x0;
	unsigned int va = 0x0;
	unsigned int session_id = 0;
	unsigned int timeline_id = 0;

	struct mtkfb_fence_buf_info *buf_info = NULL;
	struct fence_data data;
	struct ion_handle *handle = NULL;
	disp_sync_info *layer_info = NULL;
	disp_session_sync_info *session_info = NULL;

	if (buf == NULL) {
		DISPERR("Prepare Buffer, buf is NULL!!\n");
		return NULL;
	}

	session_id = buf->session_id;
	timeline_id = buf->layer_id;

	/* DISPMSG("sync prepare, session_id=0x%08x, layer_id=%d\n", session_id, layer_id); */

	session_info = _get_session_sync_info(session_id);
	layer_info = _get_sync_info(session_id, timeline_id);

	if (layer_info == NULL) {
		DISPERR("layer_info is null\n");
		return NULL;
	}

	if (layer_info->inited == 0) {
		DISPERR("FATAL ERROR, sync info not inited, session_id=0x%08x|layer_id=%d\n",
			session_id, timeline_id);
		return NULL;
	}

	dprec_start(&session_info->event_prepare, buf->layer_id, buf->ion_fd);

	buf_info = mtkfb_get_buf_info();
	mutex_lock(&layer_info->sync_lock);
	data.fence = MTK_FB_INVALID_FENCE_FD;
	data.value = ++(layer_info->fence_idx);
	mutex_unlock(&(layer_info->sync_lock));

	sprintf(data.name, "ovl_fence-0x%x-%d-%d", session_id, timeline_id, data.value);
	#ifdef CONFIG_MTK_SYNC
	ret = fence_create((struct sw_sync_timeline *)layer_info->timeline, &data);
	if (ret != 0) {
		/* Does this really happened? */
		DISPPR_ERROR("%s%d,layer%d create Fence Object failed!\n",
			     disp_session_mode_spy(session_id), DISP_SESSION_DEV(session_id),
			     timeline_id);
	}
	#endif
	/* ION MVA */
#if defined(MTK_FB_ION_SUPPORT)
	handle = mtkfb_ion_import_handle(ion_client, buf->ion_fd);
	if (!IS_ERR_OR_NULL(handle)) {
		buf_info->size = mtkfb_ion_phys_mmu_addr(ion_client, handle, &mva);

		/* /if ((DISP_SESSION_TYPE(session_id) >DISP_SESSION_PRIMARY)) */
		/* /    va = ion_map_kernel(ion_client, handle); */
	} else {
		DISPPR_ERROR("can't import ion handle for fd:%d\n", buf->ion_fd);
	}
#endif

	buf_info->fence = data.fence;
	buf_info->idx = data.value;
	buf_info->hnd = handle;
	buf_info->mva = mva;
	buf_info->va = va;
	buf_info->mva_offset = 0;
	buf_info->trigger_ticket = 0;
	buf_info->buf_state = create;
	buf_info->cache_sync = buf->cache_sync;
#ifdef CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT
	buf_info->secure_handle = 0;
	buf_info->isScure = 0;
#endif
	mutex_lock(&layer_info->sync_lock);
	list_add_tail(&buf_info->list, &layer_info->buf_list);
	mutex_unlock(&layer_info->sync_lock);

	/* DISPMSG("prepare 0x%08x\n",
	   (buf->layer_id<<24)|(buf->layer_en<<16)|(buf->ion_fd==MTK_FB_NO_ION_FD?0:buf->ion_fd)); */

	DISPPR_FENCE("P+/%s%d/L%d/id%d/fd%d/mva0x%08x/size0x%08x\n",
		     disp_session_mode_spy(session_id), DISP_SESSION_DEV(session_id), timeline_id,
		     buf_info->idx, buf_info->fence, buf_info->mva, buf_info->size);

	dprec_done(&session_info->event_prepare, buf_info->idx, buf_info->fence);

	return buf_info;
}

int disp_sync_find_fence_idx_by_addr(unsigned int session_id, unsigned int timeline_id,
				     unsigned int phy_addr)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	int idx = -1;
	unsigned int layer_en = 0;
	unsigned int addr = 0;
	unsigned int fence_idx = -1;

	disp_sync_info *layer_info = NULL;
	disp_session_sync_info *session_info = NULL;

	session_info = _get_session_sync_info(session_id);
	layer_info = _get_sync_info(session_id, timeline_id);

	if (layer_info == NULL) {
		DISPERR("layer_info is null\n");
		return -1;
	}

	if (layer_info->fence_idx == 0)
		return -2;

	/* DISPPR_FENCE("F+/0x%08x/%d/0x%08x\n", session_id, timeline_id, phy_addr); */
	disp_sync_get_cached_layer_info(session_id, timeline_id, &layer_en, &addr, &fence_idx);

	/* DISPPR_FENCE("F+/%d/0x%08x/%d\n", layer_en, addr, fence_idx); */

	dprec_start(&session_info->event_findidx, layer_en | (timeline_id << 16), fence_idx);
	if (phy_addr) {
		mutex_lock(&layer_info->sync_lock);
		list_for_each_entry(buf, &layer_info->buf_list, list) {
			/* because buf_list stores the lates fence info from prepare,
			   so we should gurantee only release the fences that has set inputed. */
			if (buf->idx <= fence_idx) {
				/* because we use cached idx as boundary,
				   so we will continue traverse even the idx has been found. */
				if ((buf->mva + buf->mva_offset) == phy_addr) {
					if (phy_addr >= buf->mva && buf->mva + buf->size > phy_addr) {
						;
					} else
						DISPPR_ERROR("wrong addr:0x%08x, 0x%08x,0x%08x\n",
							     phy_addr, buf->mva, buf->size);
					idx = buf->idx - 1;
				}
			}
		}
		mutex_unlock(&layer_info->sync_lock);
	} else {
		if (layer_en == 0)
			idx = fence_idx;
		else
			idx = fence_idx - 1;

	}

	dprec_done(&session_info->event_findidx, phy_addr, idx);

	/* DISPPR_FENCE("F/%d\n", idx); */

	return idx;
}

unsigned int disp_sync_query_buf_info(unsigned int session_id, unsigned int timeline_id,
				      unsigned int idx, unsigned int *mva, unsigned int *size)
{
	struct mtkfb_fence_buf_info *buf = NULL;
	uint32_t dst_mva = 0;
	uint32_t dst_size = 0;
	disp_sync_info *layer_info = NULL;

	layer_info = _get_sync_info(session_id, timeline_id);

	if (layer_info == NULL || mva == NULL || size == NULL) {
		DISPERR("layer_info is null\n");
		return 0;
	}

	mutex_lock(&layer_info->sync_lock);
	list_for_each_entry(buf, &layer_info->buf_list, list) {
		if (buf->idx == idx) {
			/* use local variable here to avoid polluted pointer */
			dst_mva = buf->mva;
			dst_size = buf->size;
			break;
		}
	}
	mutex_unlock(&layer_info->sync_lock);

	if (dst_mva != 0x0) {
		*mva = dst_mva;
		*size = dst_size;
		buf->ts_create = sched_clock();
		if (buf->cache_sync) {
			dprec_logger_start(DPREC_LOGGER_DISPMGR_CACHE_SYNC,
					   (unsigned int)(unsigned long)buf->hnd, buf->mva);

		#if defined(MTK_FB_ION_SUPPORT)
			mtkfb_ion_cache_flush(ion_client, buf->hnd);
		#endif
			dprec_logger_done(DPREC_LOGGER_DISPMGR_CACHE_SYNC,
					  (unsigned int)(unsigned long)buf->hnd, buf->mva);
		}
		MTKFB_FENCE_LOG("query buf mva: layer=%d, idx=%d, mva=0x%08x\n", timeline_id, idx,
				buf->mva);
	} else {
		/* FIXME: non-ion buffer need cache sync here? */
		DISPERR
		    ("cannot find this buf, session:%s%d,layer=%d, idx=%d, ",
		     disp_session_mode_spy(session_id), DISP_SESSION_DEV(session_id), timeline_id, idx);
		DISPERR("fence_idx=%d, timeline_idx=%d, cur_idx=%d!\n",
			layer_info->fence_idx, layer_info->timeline_idx, layer_info->cur_idx);
	}

	/* DISPMSG("mva query:session_id=0x%08x, layer_id=%d, mva=0x%08x\n", session_id, layer_id, mva); */

	return 0;
}

int disp_sync_get_debug_info(char *stringbuf, int buf_len)
{
	int len = 0;
	int i = 0;
	int layer_id = 0;
	disp_session_sync_info *session_info = NULL;
	unsigned int session_id = 0;
	disp_sync_info *layer_info = NULL;

	len +=
	    scnprintf(stringbuf + len, buf_len - len,
		      "|--------------------------------------------------------------------------------------|\n");
	len +=
	    scnprintf(stringbuf + len, buf_len - len,
		      "|********Display Session Information********\n");

	for (i = 0; i < sizeof(_disp_fence_context) / sizeof(_disp_fence_context[0]); i++) {
		session_id = _disp_fence_context[i].session_id;
		session_info = &(_disp_fence_context[i]);
		len +=
		    scnprintf(stringbuf + len, buf_len - len, "|Session id\t0x%08x\n", session_id);
		for (layer_id = 0; layer_id < HW_OVERLAY_COUNT; layer_id++) {
			layer_info = &(session_info->session_layer_info[layer_id]);
			len +=
			    scnprintf(stringbuf + len, buf_len - len,
				      "|layerinfo %d\tfence_fd(%d)\tfence_idx(%d)\ttimeline_idx(%d)\n",
				      layer_id, layer_info->fence_fd, layer_info->fence_idx,
				      layer_info->timeline_idx);
		}
		len +=
		    scnprintf(stringbuf + len, buf_len - len,
			      "|--------------------------------------------------------------------------------------|\n");
	}

	return len;
}
