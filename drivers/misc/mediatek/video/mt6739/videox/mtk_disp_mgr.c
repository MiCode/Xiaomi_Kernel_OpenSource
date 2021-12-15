/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/param.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/compat.h>

#include "m4u.h"

#include "mtk_sync.h"
#include "debug.h"
#include "disp_drv_log.h"
#include "disp_lcm.h"
#include "disp_utils.h"
#include "mtkfb_console.h"
#include "ddp_hal.h"
#include "ddp_dump.h"
#include "ddp_path.h"
#include "ddp_drv.h"
#include "ddp_info.h"
#include "primary_display.h"
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"
#include "ddp_manager.h"
#include "disp_drv_platform.h"
#include "display_recorder.h"
#include "mtk_disp_mgr.h"
#include "disp_session.h"
#include "mtk_ovl.h"
#include "ddp_mmp.h"
#include "mtkfb_fence.h"
#include "extd_multi_control.h"
#include "m4u.h"
#include "layering_rule.h"
#include "compat_mtk_disp_mgr.h"
#include "disp_partial.h"
#include "frame_queue.h"
#include "disp_arr.h"

#define DDP_OUTPUT_LAYID 4

/* #define NO_PQ_IOCTL */

static unsigned int session_config[MAX_SESSION_COUNT];
static DEFINE_MUTEX(disp_session_lock);
static DEFINE_MUTEX(disp_layer_lock);

static dev_t mtk_disp_mgr_devno;
static struct cdev *mtk_disp_mgr_cdev;
static struct class *mtk_disp_mgr_class;

static int mtk_disp_mgr_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t mtk_disp_mgr_read(struct file *file, char __user *data,
				 size_t len, loff_t *ppos)
{
	return 0;
}

static int mtk_disp_mgr_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int mtk_disp_mgr_flush(struct file *a_pstFile, fl_owner_t a_id)
{
	return 0;
}

int _session_inited(struct disp_session_config config)
{
#if 0
	int i, idx = -1;

	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (session_config[i] == 0 && idx == -1)
			idx = i;


		if (session_config[i] == session) {
			ret = DCP_STATUS_ALREADY_EXIST;
			DISPMSG("session(0x%x) already exists\n", session);
			break;
		}
	}
#endif
	return 0;
}

int disp_create_session(struct disp_session_config *config)
{
	int ret = 0;
	int is_session_inited = 0;
	unsigned int session = 0;
	int i, idx = -1;
	static DEFINE_RATELIMIT_STATE(ioctl_ratelimit, 1 * HZ, 10);
	/* 1.To check if this session exists already */
	session = MAKE_DISP_SESSION(config->type, config->device_id);

	mutex_lock(&disp_session_lock);
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (session_config[i] == session) {
			is_session_inited = 1;
			idx = i;
			DISPDBG("create session is exited:0x%x\n", session);
			break;
		}
		if ((DISP_SESSION_TYPE(session_config[i]) == config->type) &&
		    __ratelimit(&ioctl_ratelimit)) {
			DISPERR("session(0x%x) type(%s) existed!\n", session,
				disp_session_mode_spy(session));
			ret = -1;
			goto done;
		}
		if (config->type > DISP_SESSION_MEMORY &&
		    __ratelimit(&ioctl_ratelimit)) {
			DISPERR("Invalid session(0x%x) creation request\n",
				session);
			ret = -1;
			goto done;
		}
	}

	if (is_session_inited == 1) {
		config->session_id = session;
		goto done;
	}

	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (session_config[i] == 0 && idx == -1) {
			idx = i;
			break;
		}
	}
	/* 1.To check if support this session (mode,type,dev) */
	/* 2. Create this session */
	if (idx != -1) {
		config->session_id = session;
		session_config[idx] = session;
		DISPDBG("New session(0x%x)\n", session);
	} else {
		DISPERR("Invalid session(0x%x) creation request\n", session);
		ret = -1;
	}
done:
	mutex_unlock(&disp_session_lock);

	DISPDBG("new session done\n");
	return ret;
}

static int release_session_buffer(unsigned int session)
{
	mutex_lock(&disp_session_lock);

	if (session == 0) {
		DISPERR("%s: session id = %u !\n", __func__, session);
		mutex_unlock(&disp_session_lock);
		return -1;
	}

	mtkfb_release_session_fence(session);

	mutex_unlock(&disp_session_lock);
	return 0;
}

int disp_destroy_session(struct disp_session_config *config)
{
	int ret = -1;
	unsigned int session = config->session_id;
	int i;

	DISPMSG("disp_destroy_session, 0x%x", config->session_id);

	/* 1.To check if this session exists already, and remove it */
	mutex_lock(&disp_session_lock);
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (session_config[i] == session) {
			session_config[i] = 0;
			ret = 0;
			break;
		}
	}

	mutex_unlock(&disp_session_lock);

	if (DISP_SESSION_TYPE(config->session_id) != DISP_SESSION_PRIMARY)
#ifdef CONFIG_MTK_HDMI_SUPPORT
		external_display_switch_mode(config->mode, session_config,
					     config->session_id);
#endif
	if (DISP_SESSION_TYPE(config->session_id) != DISP_SESSION_PRIMARY)
		release_session_buffer(config->session_id);

	/* 2. Destroy this session */
	if (ret == 0)
		DISPMSG("Destroy session(0x%x)\n", session);
	else
		DISPERR("session(0x%x) does not exists\n", session);

	return ret;
}

int _ioctl_create_session(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct disp_session_config config;

	if (copy_from_user(&config, argp, sizeof(config))) {
		DISPERR("[FB]: copy_from_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	if (disp_create_session(&config) != 0)
		ret = -EFAULT;

	if (copy_to_user(argp, &config, sizeof(config))) {
		DISPERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

	return ret;
}

int _ioctl_destroy_session(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct disp_session_config config;

	if (copy_from_user(&config, argp, sizeof(config))) {
		DISPERR("[FB]: copy_from_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	if (disp_destroy_session(&config) != 0)
		ret = -EFAULT;

	return ret;
}

char *disp_session_mode_spy(unsigned int session_id)
{
	switch (DISP_SESSION_TYPE(session_id)) {
	case DISP_SESSION_PRIMARY:
		return "P";
	case DISP_SESSION_EXTERNAL:
		return "E";
	case DISP_SESSION_MEMORY:
		return "M";
	default:
		return "Unknown";
	}
}

int _ioctl_prepare_present_fence(unsigned long arg)
{
	int ret = 0;

	void __user *argp = (void __user *)arg;
	struct mtk_sync_create_fence_data data;
	struct disp_present_fence pnt_fence;
	static unsigned int fence_idx;
	struct disp_sync_info *layer_info = NULL;
	int timeline_id = disp_sync_get_present_timeline_id();

	if (copy_from_user(&pnt_fence, (void __user *)arg,
			sizeof(struct disp_present_fence))) {
		pr_info("[FB Driver]: copy_from_user failed! line:%d\n",
			__LINE__);
		return -EFAULT;
	}

	if (DISP_SESSION_TYPE(pnt_fence.session_id) !=
		DISP_SESSION_PRIMARY) {
		DISPWARN("non-primary ask for present fence! session=0x%x\n",
			pnt_fence.session_id);
		data.fence = MTK_FB_INVALID_FENCE_FD;
		data.value = 0;
	} else {
		layer_info = _get_sync_info(pnt_fence.session_id,
			timeline_id);
		if (layer_info == NULL) {
			DISPERR("layer_info is null\n");
			ret = -EFAULT;
			return ret;
		}
		/* create fence */
		data.fence = MTK_FB_INVALID_FENCE_FD;
		data.value = ++fence_idx;
		ret = fence_create(layer_info->timeline, &data);
		if (ret != 0) {
			DISPERR("%s%d,layer%d create Fence Object failed!\n",
				disp_session_mode_spy(pnt_fence.session_id),
				DISP_SESSION_DEV(pnt_fence.session_id),
				timeline_id);
			ret = -EFAULT;
		}
	}

	pnt_fence.present_fence_fd = data.fence;
	pnt_fence.present_fence_index = data.value;
	if (copy_to_user(argp, &pnt_fence,
		sizeof(pnt_fence))) {
		pr_info("[FB Driver]: copy_to_user failed! line:%d\n",
			__LINE__);
		ret = -EFAULT;
	}
	mmprofile_log_ex(ddp_mmp_get_events()->present_fence_get,
		MMPROFILE_FLAG_PULSE,
		pnt_fence.present_fence_fd,
		pnt_fence.present_fence_index);
	DISPPR_FENCE("P+/%s%d/L%d/id%d/fd%d\n",
		disp_session_mode_spy(pnt_fence.session_id),
		DISP_SESSION_DEV(pnt_fence.session_id), timeline_id,
		pnt_fence.present_fence_index,
		pnt_fence.present_fence_fd);
	return ret;
}

static void __prepare_output_buffer(struct disp_buffer_info *disp_buf,
				    struct mtkfb_fence_buf_info *output_buf)
{
	if (!(primary_display_is_decouple_mode() &&
	      primary_display_is_mirror_mode())) {
		disp_buf->interface_fence_fd = MTK_FB_INVALID_FENCE_FD;
		disp_buf->interface_index = 0;
		return;
	}

	/* create second fence for WDMA when decouple mirror mode */
	disp_buf->layer_id = disp_sync_get_output_interface_timeline_id();
	output_buf = disp_sync_prepare_buf(disp_buf);
	if (output_buf) {
		disp_buf->interface_fence_fd = output_buf->fence;
		disp_buf->interface_index = output_buf->idx;
	} else {
		DISPERR(" FAIL:P+/%s%d/L%u/e%d/ion%d/c%d/idx%d/fd%d\n",
			disp_session_mode_spy(disp_buf->session_id),
			DISP_SESSION_DEV(disp_buf->session_id),
			disp_buf->layer_id, disp_buf->layer_en,
			disp_buf->ion_fd, disp_buf->cache_sync,
			disp_buf->index, disp_buf->fence_fd);

		disp_buf->interface_fence_fd = MTK_FB_INVALID_FENCE_FD;
		disp_buf->interface_index = 0;
	}
}

int _ioctl_prepare_buffer(unsigned long arg, enum PREPARE_FENCE_TYPE type)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct disp_buffer_info info;
	struct mtkfb_fence_buf_info *buf, *buf2 = NULL;

	if (copy_from_user(&info, (void __user *)arg, sizeof(info))) {
		DISPERR("[FB Driver]: copy_from_user failed! line:%d\n",
			__LINE__);
		return -EFAULT;
	}

	if (type == PREPARE_INPUT_FENCE)
		DISPDBG("There is do nothing in input fence.\n");
	else if (type == PREPARE_PRESENT_FENCE)
		info.layer_id = disp_sync_get_present_timeline_id();
	else if (type == PREPARE_OUTPUT_FENCE)
		info.layer_id = disp_sync_get_output_timeline_id();
	else
		DISPERR("type is wrong: %d\n", type);

	if (info.layer_en) {
		buf = disp_sync_prepare_buf(&info);
		if (buf != NULL) {
			info.fence_fd = buf->fence;
			info.index = buf->idx;
		} else {
			DISPERR("P+ FAIL /%s%d/l%d/e%d/ion%d/c%d/id%d/ffd%d\n",
				disp_session_mode_spy(info.session_id),
				DISP_SESSION_DEV(info.session_id),
				info.layer_id, info.layer_en, info.ion_fd,
				info.cache_sync, info.index, info.fence_fd);
			info.fence_fd = MTK_FB_INVALID_FENCE_FD;
			info.index = 0;
		}

		if (type == PREPARE_OUTPUT_FENCE)
			__prepare_output_buffer(&info, buf2);
	} else {
		DISPERR("P+ FAIL /%s%d/l%d/e%d/ion%d/c%d/id%d/ffd%d\n",
			disp_session_mode_spy(info.session_id),
			DISP_SESSION_DEV(info.session_id), info.layer_id,
			info.layer_en, info.ion_fd, info.cache_sync,
			info.index, info.fence_fd);
		info.fence_fd = MTK_FB_INVALID_FENCE_FD; /* invalid fd */
		info.index = 0;
	}
	if (copy_to_user(argp, &info, sizeof(info))) {
		DISPERR("[FB Driver]: copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}
	return ret;
}

const char *_disp_format_spy(enum DISP_FORMAT format)
{
	switch (format) {
	case DISP_FORMAT_RGB565:
		return "RGB565";
	case DISP_FORMAT_RGB888:
		return "RGB888";
	case DISP_FORMAT_BGR888:
		return "BGR888";
	case DISP_FORMAT_ARGB8888:
		return "ARGB8888";
	case DISP_FORMAT_ABGR8888:
		return "ABGR8888";
	case DISP_FORMAT_RGBA8888:
		return "RGBA8888";
	case DISP_FORMAT_BGRA8888:
		return "BGRA8888";
	case DISP_FORMAT_YUV422:
		return "YUV422";
	case DISP_FORMAT_XRGB8888:
		return "XRGB8888";
	case DISP_FORMAT_XBGR8888:
		return "XBGR8888";
	case DISP_FORMAT_RGBX8888:
		return "RGBX8888";
	case DISP_FORMAT_BGRX8888:
		return "BGRX8888";
	case DISP_FORMAT_UYVY:
		return "UYVY";
	case DISP_FORMAT_YUV420_P:
		return "YUV420_P";
	case DISP_FORMAT_YV12:
		return "YV12";
	case DISP_FORMAT_PABGR8888:
		return "PABGR";
	case DISP_FORMAT_PARGB8888:
		return "PARGB";
	case DISP_FORMAT_PBGRA8888:
		return "PBGRA";
	case DISP_FORMAT_PRGBA8888:
		return "PRGBA";
	default:
		return "unknown";
	}
}

#if 0 /* defined but not used */
static int _sync_convert_fb_layer_to_disp_input(unsigned int session_id, struct disp_input_config *src,
						struct primary_disp_input_config *dst,
						unsigned int dst_mva)
{
	unsigned int layerpitch = 0;

	dst->layer = src->layer_id;

	if (!src->layer_enable) {
		dst->layer_en = 0;
		dst->isDirty = true;
		return 0;
	}

	dst->fmt = disp_fmt_to_unified_fmt(src->src_fmt);
	layerpitch = UFMT_GET_bpp(dst->fmt) / 8;

	dst->buffer_source = src->buffer_source;

	dst->vaddr = (unsigned long)src->src_base_addr;
	dst->security = src->security;
	if (src->src_phy_addr != NULL)
		dst->addr = (unsigned long)src->src_phy_addr;
	else
		dst->addr = dst_mva;

	dst->isTdshp = src->isTdshp;
	dst->buff_idx = src->next_buff_idx;
	dst->identity = src->identity;
	dst->connected_type = src->connected_type;

	/* set Alpha blending */
	dst->aen = src->alpha_enable;
	dst->alpha = src->alpha;
	dst->sur_aen = src->sur_aen;
	dst->src_alpha = src->src_alpha;
	dst->dst_alpha = src->dst_alpha;
#if 1
	if (DISP_FORMAT_ARGB8888 == src->src_fmt || DISP_FORMAT_ABGR8888 == src->src_fmt
	    || DISP_FORMAT_RGBA8888 == src->src_fmt || DISP_FORMAT_BGRA8888 == src->src_fmt) {
		dst->aen = TRUE;
	}
#endif

	/* set src width, src height */
	dst->src_x = src->src_offset_x;
	dst->src_y = src->src_offset_y;
	dst->src_w = src->src_width;
	dst->src_h = src->src_height;
	dst->dst_x = src->tgt_offset_x;
	dst->dst_y = src->tgt_offset_y;
	dst->dst_w = src->tgt_width;
	dst->dst_h = src->tgt_height;
	if (dst->dst_w > dst->src_w)
		dst->dst_w = dst->src_w;
	if (dst->dst_h > dst->src_h)
		dst->dst_h = dst->src_h;

	dst->src_pitch = src->src_pitch * layerpitch;

	/* set color key */
	dst->key = src->src_color_key;
	dst->keyEn = src->src_use_color_key;

	/* data transferring is triggerred in MTKFB_TRIG_OVERLAY_OUT */
	dst->layer_en = src->layer_enable;
	dst->isDirty = true;
	dst->yuv_range = src->yuv_range;
	dst->buffer_source = src->buffer_source;

	return 0;
}
#endif

void dump_input_cfg_info(struct disp_input_config *input_cfg,
			 unsigned int session_id, int is_err)
{
	_DISP_PRINT_FENCE_OR_ERR(is_err,
		"S+/%sL%d/e%d/id%d/%dx%d(%d,%d)(%d,%d)/%s/%d/mva%p/t%d/s%d\n",
		disp_session_mode_spy(session_id), input_cfg->layer_id,
		input_cfg->layer_enable, input_cfg->next_buff_idx,
		input_cfg->src_width, input_cfg->src_height,
		input_cfg->src_offset_x, input_cfg->src_offset_y,
		input_cfg->tgt_offset_x, input_cfg->tgt_offset_y,
		_disp_format_spy(input_cfg->src_fmt), input_cfg->src_pitch,
		input_cfg->src_phy_addr, get_ovl2mem_ticket(),
		input_cfg->security);
}

static int _get_max_layer(unsigned int session_id)
{
	if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_PRIMARY)
		return primary_display_get_max_layer();
#ifdef CONFIG_MTK_HDMI_SUPPORT
	else if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_EXTERNAL)
		return ext_disp_get_max_layer();
#endif
	else if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_MEMORY)
		return ovl2mem_get_max_layer();

	DISPWARN("session_id is wrong!!\n");
	return 0;
}
static int disp_validate_input_params(struct disp_input_config *cfg,
				      int layer_num)
{
	if (cfg->layer_id >= layer_num) {
		disp_aee_print("layer_id=%d > layer_num=%d\n",
			       cfg->layer_id, layer_num);
		return -1;
	}
	if (!cfg->layer_enable)
		return 0;

	if ((cfg->src_fmt <= 0) || ((cfg->src_fmt >> 8) == 15) ||
	    ((cfg->src_fmt >> 8) > (DISP_FORMAT_DIM >> 8))) {
		disp_aee_print("layer_id=%d,src_fmt=0x%x is invalid color format\n",
			cfg->layer_id, cfg->src_fmt);
		return -1;
	}
	return 0;
}

static int disp_validate_output_params(struct disp_output_config *cfg)
{
	if ((cfg->fmt <= 0) || ((cfg->fmt >> 8) == 15) ||
	    ((cfg->fmt >> 8) > (DISP_FORMAT_DIM >> 8))) {
		disp_aee_print("output fmt=0x%x is invalid color format\n",
			       cfg->fmt);
		return -1;
	}

	return 0;
}

int disp_validate_ioctl_params(struct disp_frame_cfg_t *cfg)
{
	int i;

	/* TODO: check session_id */
	int max_layer_num = _get_max_layer(cfg->session_id);

	if (max_layer_num == 0 || (cfg->input_layer_num > max_layer_num)) {
		disp_aee_print("sess:0x%x layer_num %d>%d\n", cfg->session_id,
			       cfg->input_layer_num, max_layer_num);
		return -1;
	}

	for (i = 0; i < cfg->input_layer_num; i++)
		if (disp_validate_input_params(&cfg->input_cfg[i],
					       max_layer_num) != 0)
			return -1;

	if (cfg->output_en && disp_validate_output_params(&cfg->output_cfg) != 0)
		return -1;

	return 0;
}
static int disp_input_get_dirty_roi(struct disp_frame_cfg_t *frm_cfg)
{
	int i;

	for (i = 0; i < frm_cfg->input_layer_num; i++) {
		void *addr;
		unsigned long size;
		struct disp_input_config *cfg = &frm_cfg->input_cfg[i];

		if (!cfg->layer_enable || !cfg->dirty_roi_num)
			goto layer_err;

		/* alloc mem for partial update dirty ROIs */
		if (WARN_ON(cfg->dirty_roi_num > 20)) {
			/* disable partial for this frame */
			goto layer_err;
		}

		size = cfg->dirty_roi_num * sizeof(struct layer_dirty_roi);
		addr = kmalloc(size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(addr))
			goto layer_err;

		if (copy_from_user(addr, cfg->dirty_roi_addr, size)) {
			DISPERR("[drity roi]: copy_from_user failed! line:%d\n",
				__LINE__);
			DISPERR("to=%p, from=%p, size=0x%lx\n",
				addr, cfg->dirty_roi_addr, size);
			kfree(addr);
			goto layer_err;

		}
		cfg->dirty_roi_addr = addr;

		continue;
layer_err:
		cfg->dirty_roi_num = 0;
		cfg->dirty_roi_addr = NULL;
	}
	return 0;
}

int disp_input_free_dirty_roi(struct disp_frame_cfg_t *cfg)
{
	int i;

	for (i = 0; i < cfg->input_layer_num; i++)
		kfree(cfg->input_cfg[i].dirty_roi_addr);

	return 0;
}

static int input_config_preprocess(struct disp_frame_cfg_t *cfg)
{
	int i = 0, is_err = 0;
	int layer_id = 0;
	unsigned int dst_size = 0;
	unsigned long dst_mva = 0;
	unsigned int session_id = 0;
	unsigned int mva_offset = 0;
	struct disp_session_sync_info *session_info;

	session_id = cfg->session_id;
	session_info = disp_get_session_sync_info_for_debug(session_id);

	if (cfg->input_layer_num == 0 ||
	    cfg->input_layer_num > _get_max_layer(session_id)) {
		DISPERR("set_%s_buffer, config_layer_num invalid = %d, max_layer_num = %d!\n",
			disp_session_mode_spy(session_id), cfg->input_layer_num,
			_get_max_layer(session_id));
		return -1;
	}

	disp_input_get_dirty_roi(cfg);
	for (i = 0; i < cfg->input_layer_num; i++) {
		struct disp_input_config *c = &cfg->input_cfg[i];

		dst_mva = 0;
		layer_id = c->layer_id;
		if (layer_id >= _get_max_layer(session_id)) {
			DISPERR("set_%s_buffer, invalid layer_id = %d!\n",
				disp_session_mode_spy(session_id), layer_id);
			continue;
		}

		if (c->layer_enable) {
			unsigned int Bpp, x, y, pitch;
			enum UNIFIED_COLOR_FMT fmt = UFMT_UNKNOWN;
#ifdef DISP_SYNC_ENABLE
			struct sync_fence *src_fence = NULL;
#endif
			if (c->buffer_source == DISP_BUFFER_ALPHA) {
				DISPPR_FENCE("%sL %d is dim layer,fence %d\n",
					     disp_session_mode_spy(session_id),
					     c->layer_id, c->next_buff_idx);

				c->src_offset_x = 0;
				c->src_offset_y = 0;
				c->sur_aen = 0;
				c->src_fmt = DISP_FORMAT_RGB888;
				c->src_pitch = c->src_width;
				c->src_phy_addr =
					(void *)get_dim_layer_mva_addr();
				c->next_buff_idx = 0;
				c->src_fence_struct = NULL;
				/* force dim layer as non-sec */
				c->security = DISP_NORMAL_BUFFER;
			}
			dst_mva = (unsigned long)(c->src_phy_addr);
			if (!dst_mva) {
				disp_sync_query_buf_info(
					session_id, layer_id,
					(unsigned int)c->next_buff_idx,
					&dst_mva, &dst_size);
			}

			c->src_phy_addr = (void *)dst_mva;

			if (!dst_mva) {
				DISPERR("disable layer %d because of no valid mva\n",
					c->layer_id);
				c->layer_enable = 0;
				is_err = 1;
			}
#ifdef DISP_SYNC_ENABLE
			/* get src fence */
			src_fence = sync_fence_fdget(c->src_fence_fd);
			if (!src_fence && c->src_fence_fd != -1) {
				DISPERR("error to get src_fence from fd %d\n",
					c->src_fence_fd);
				is_err = 1;
			}
			c->src_fence_struct = src_fence;
#endif
			/*
			 * OVL addr is not the start address of buffer,
			 * which is calculated by pitch and ROI.
			 */
			x = c->src_offset_x;
			y = c->src_offset_y;
			pitch = c->src_pitch;
			fmt = disp_fmt_to_unified_fmt(c->src_fmt);
			Bpp = UFMT_GET_bpp(fmt) / 8;

			mva_offset = (x + y * pitch) * Bpp;
			mtkfb_update_buf_info(
				cfg->session_id, c->layer_id,
				c->next_buff_idx, mva_offset,
				c->frm_sequence);

			if (DISP_SESSION_TYPE(session_id) ==
			    DISP_SESSION_MEMORY) {
				mtkfb_update_buf_ticket(
					session_id, layer_id,
					c->next_buff_idx,
					get_ovl2mem_ticket());
			}

			disp_sync_put_cached_layer_info(session_id, layer_id,
							&cfg->input_cfg[i],
							dst_mva);

			dump_input_cfg_info(&cfg->input_cfg[i], session_id,
					    is_err);
		} else {
			c->src_fence_struct = NULL;
			DISPPR_FENCE("S+/%sL%d/e%d/id%d\n",
				     disp_session_mode_spy(session_id),
				     c->layer_id,
				     c->layer_enable,
				     c->next_buff_idx);
		}

		if (session_info) {
			dprec_submit(
				&session_info->event_frame_cfg,
				c->next_buff_idx,
				(cfg->input_layer_num << 28) |
					(c->layer_id << 24) |
					(c->src_fmt << 12) |
					c->layer_enable);
		}
	}
	return 0;
}

static int output_config_preprocess(struct disp_frame_cfg_t *cfg)
{
	unsigned int session_id = 0;
	unsigned long dst_mva = 0;
	unsigned int dst_size;
	struct disp_session_sync_info *session_info;
#ifdef DISP_SYNC_ENABLE
	struct sync_fence *src_fence;
#endif
	session_id = cfg->session_id;
	session_info = disp_get_session_sync_info_for_debug(session_id);

	if (cfg->output_cfg.pa) {
		dst_mva = (unsigned long)(cfg->output_cfg.pa);
	} else {
		disp_sync_query_buf_info_nosync(
			session_id, disp_sync_get_output_timeline_id(),
			cfg->output_cfg.buff_idx, &dst_mva, &dst_size);
	}
	cfg->output_cfg.pa = (void *)dst_mva;

	if (!dst_mva) {
		DISPERR("%s output mva=0!!, skip it\n", __func__);
		cfg->output_en = 0;
		goto out;
	}
#ifdef DISP_SYNC_ENABLE
	/* get src fence */
	src_fence = sync_fence_fdget(cfg->output_cfg.src_fence_fd);
	if (!src_fence && cfg->output_cfg.src_fence_fd != -1) {
		DISPERR("error to get src_fence from output fd %d\n",
			cfg->output_cfg.src_fence_fd);
	}
	cfg->output_cfg.src_fence_struct = src_fence;
#endif
	if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_PRIMARY) {
		/* must be mirror mode */
		if (primary_display_is_decouple_mode()) {
			disp_sync_put_cached_layer_info_v2(session_id,
				disp_sync_get_output_interface_timeline_id(),
				cfg->output_cfg.interface_idx, 1, dst_mva);
		}
	}

	DISPPR_FENCE("S+O/%s%d/L%d(id%d)/L%d(id%d)/%dx%d(%d,%d)/%s/%d/0x%08x/0x%p/mva0x%08lx/t%d/sec%d\n",
		disp_session_mode_spy(session_id), DISP_SESSION_DEV(session_id),
		disp_sync_get_output_timeline_id(), cfg->output_cfg.buff_idx,
		disp_sync_get_output_interface_timeline_id(),
		cfg->output_cfg.interface_idx, cfg->output_cfg.width,
		cfg->output_cfg.height, cfg->output_cfg.x, cfg->output_cfg.y,
		_disp_format_spy(cfg->output_cfg.fmt), cfg->output_cfg.pitch,
		cfg->output_cfg.pitchUV, cfg->output_cfg.pa, dst_mva,
		get_ovl2mem_ticket(), cfg->output_cfg.security);

	if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_MEMORY)
		mtkfb_update_buf_ticket(
			session_id, disp_sync_get_output_timeline_id(),
			cfg->output_cfg.buff_idx, get_ovl2mem_ticket());

	mtkfb_update_buf_info(
		cfg->session_id, disp_sync_get_output_interface_timeline_id(),
		cfg->output_cfg.buff_idx, 0, cfg->output_cfg.frm_sequence);

	if (session_info) {
		dprec_submit(&session_info->event_frame_cfg,
			     cfg->output_cfg.buff_idx, dst_mva);
	}
	DISPDBG("_ioctl_set_output_buffer done idx 0x%x, mva %lx, fmt %x, w %x, h %x (%x %x), p %x\n",
		cfg->output_cfg.buff_idx, dst_mva, cfg->output_cfg.fmt,
		cfg->output_cfg.width, cfg->output_cfg.height,
		cfg->output_cfg.x, cfg->output_cfg.y, cfg->output_cfg.pitch);
out:
	return 0;
}

static int do_frame_config(struct frame_queue_t *frame_node)
{
	struct disp_frame_cfg_t *frame_cfg = &frame_node->frame_cfg;

	if (DISP_SESSION_TYPE(frame_cfg->session_id) == DISP_SESSION_PRIMARY)
		primary_display_frame_cfg(frame_cfg);
#ifdef CONFIG_MTK_HDMI_SUPPORT
	else if (DISP_SESSION_TYPE(frame_cfg->session_id) ==
		 DISP_SESSION_EXTERNAL)
		external_display_frame_cfg(frame_cfg);
#endif
	else if (DISP_SESSION_TYPE(frame_cfg->session_id) ==
		 DISP_SESSION_MEMORY)
		ovl2mem_frame_cfg(frame_cfg);

	return 0;
}

static int __frame_config(struct frame_queue_t *frame_node)
{
	struct frame_queue_head_t *head;
	struct disp_frame_cfg_t *frame_cfg = &frame_node->frame_cfg;
	struct sync_fence *present_fence = NULL;
	struct disp_session_sync_info *s_info;

	head = get_frame_queue_head(frame_cfg->session_id);
	if (!head) {
		disp_aee_print("error to get frame queue!!\n");
		return -EINVAL;
	}

	s_info = disp_get_session_sync_info_for_debug(frame_cfg->session_id);
	if (s_info)
		dprec_start(&s_info->event_frame_cfg,
			    frame_cfg->present_fence_idx, 0);

	frame_cfg->setter = SESSION_USER_HWC;
#ifdef DISP_SYNC_ENABLE
	/* get present fence */
	if (frame_cfg->prev_present_fence_fd != -1) {
		present_fence =
			sync_fence_fdget(frame_cfg->prev_present_fence_fd);
		if (!present_fence) {
			DISPERR("error to get prev_present_fence from fd %d\n",
				frame_cfg->prev_present_fence_fd);
		}
	}
#endif
	frame_cfg->prev_present_fence_struct = present_fence;

	frame_node->do_frame_cfg = do_frame_config;

	if (input_config_preprocess(frame_cfg) != 0) {
		if (s_info)
			dprec_done(&s_info->event_frame_cfg,
				   frame_cfg->present_fence_idx, 0);
		return -EINVAL;
	}

	if (frame_cfg->output_en)
		output_config_preprocess(frame_cfg);

	frame_queue_push(head, frame_node);

	if (s_info)
		dprec_done(&s_info->event_frame_cfg,
			   frame_cfg->present_fence_idx, 0);

	return 0;
}
static int _ioctl_frame_config(unsigned long arg)
{
	void *ret_val = NULL;
	struct frame_queue_t *frame_node;
	struct disp_frame_cfg_t *frame_cfg;

	frame_node = frame_queue_node_create();
	if (IS_ERR_OR_NULL(frame_node)) {
		ret_val = ERR_PTR(-ENOMEM);
		DISPERR("[FB Driver]: frame queue node create failed! line:%d\n",
			__LINE__);
		return PTR_ERR(ret_val);
	}

	/* this is initialized correctly when get node from framequeue list */
	frame_cfg = &frame_node->frame_cfg;

	if (copy_from_user(frame_cfg, (void __user *)arg, sizeof(*frame_cfg))) {
		ret_val = ERR_PTR(-EFAULT);
		DISPERR("[FB Driver]: copy_from_user failed! line:%d\n",
			__LINE__);
		goto Error;
	}

	if (disp_validate_ioctl_params(frame_cfg)) {
		ret_val = ERR_PTR(-EINVAL);
		goto Error;
	}

	return __frame_config(frame_node);

Error:
	frame_queue_node_destroy(frame_node);
	return PTR_ERR(ret_val);
}

static int _ioctl_wait_all_jobs_done(unsigned long arg)
{
	unsigned int session_id = (unsigned int)arg;
	struct frame_queue_head_t *head;
	int ret = 0;

	if (session_id > MAX_SESSION_COUNT - 1)
		return -EINVAL;

	head = get_frame_queue_head(session_id);
	if (!head) {
		disp_aee_print("%s:error to get frame queue!!\n", __func__);
		return -EINVAL;
	}

	ret = frame_queue_wait_all_jobs_done(head);
	return ret;
}
int disp_mgr_get_session_info(struct disp_session_info *info)
{
	unsigned int session_id = 0;

	session_id = info->session_id;

	if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_PRIMARY) {
		primary_display_get_info(info);
#ifdef CONFIG_MTK_HDMI_SUPPORT
	} else if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_EXTERNAL) {
		external_display_get_info(info, session_id);
#endif
	} else if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_MEMORY) {
		ovl2mem_get_info(info);
	} else {
		DISPWARN("session type is wrong:0x%08x\n", session_id);
		return -1;
	}

	return 0;
}

int _ioctl_get_info(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct disp_session_info info;

	if (copy_from_user(&info, argp, sizeof(info))) {
		DISPERR("[FB]: copy_from_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	ret = disp_mgr_get_session_info(&info);

	if (copy_to_user(argp, &info, sizeof(info))) {
		DISPERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

	return ret;
}

int _ioctl_get_is_driver_suspend(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	unsigned int is_suspend = 0;

	is_suspend = primary_display_is_sleepd();
	DISPDBG("ioctl_get_is_driver_suspend, is_suspend=%d\n", is_suspend);
	if (copy_to_user(argp, &is_suspend, sizeof(int))) {
		DISPERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

	return ret;
}

int _ioctl_get_display_caps(unsigned long arg)
{
	int ret = 0;
	struct disp_caps_info caps_info;
	void __user *argp = (void __user *)arg;

	if (copy_from_user(&caps_info, argp, sizeof(caps_info))) {
		DISPERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}
	memset(&caps_info, 0, sizeof(caps_info));
#ifdef DISP_HW_MODE_CAP
	caps_info.output_mode = DISP_HW_MODE_CAP;
#else
	caps_info.output_mode = DISP_OUTPUT_CAP_DIRECT_LINK;
#endif

#ifdef DISP_HW_PASS_MODE
	caps_info.output_pass = DISP_HW_PASS_MODE;
#else
	caps_info.output_pass = DISP_OUTPUT_CAP_SINGLE_PASS;
#endif

#ifdef DISP_HW_MAX_LAYER
	caps_info.max_layer_num = DISP_HW_MAX_LAYER;
#else
	caps_info.max_layer_num = 4;
#endif
	caps_info.is_support_frame_cfg_ioctl = 1;

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	caps_info.is_output_rotated = 1;
#endif

	if (disp_partial_is_support())
		caps_info.disp_feature |= DISP_FEATURE_PARTIAL;

	if (disp_helper_get_option(DISP_OPT_HRT))
		caps_info.disp_feature |= DISP_FEATURE_HRT;

#ifdef DISP_SYNC_ENABLE
	caps_info.disp_feature |= DISP_FEATURE_FENCE_WAIT;
#endif
	if (copy_to_user(argp, &caps_info, sizeof(caps_info))) {
		DISPERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

	return ret;
}

int _ioctl_wait_vsync(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct disp_session_vsync_config vsync_config;
	struct disp_session_sync_info *s_info;

	if (copy_from_user(&vsync_config, argp, sizeof(vsync_config))) {
		DISPERR("[FB]: copy_from_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	s_info = disp_get_session_sync_info_for_debug(vsync_config.session_id);
	if (s_info)
		dprec_start(&s_info->event_waitvsync, 0, 0);
#ifdef CONFIG_MTK_HDMI_SUPPORT
	if (DISP_SESSION_TYPE(vsync_config.session_id) == DISP_SESSION_EXTERNAL)
		ret = external_display_wait_for_vsync(&vsync_config,
						      vsync_config.session_id);
	else
#endif
		ret = primary_display_wait_for_vsync(&vsync_config);

	if (s_info)
		dprec_done(&s_info->event_waitvsync, 0, 0);

	if (copy_to_user(argp, &vsync_config, sizeof(vsync_config))) {
		DISPERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}
	return ret;
}

int _ioctl_get_vsync(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	unsigned int fps = 0;

	fps = primary_display_force_get_vsync_fps();
	DISPMSG("ioctl_get_vsync, fps=%d\n", fps);
	if (copy_to_user(argp, &fps, sizeof(int))) {
		DISPERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

	return ret;
}

int _ioctl_set_vsync(unsigned long arg)
{
	int ret = 0;
	unsigned int fps = (unsigned int)arg;

	if ((fps < primary_display_get_min_refresh_rate()) ||
	    (fps > primary_display_get_max_refresh_rate())) {
		DISPERR("%s: fps setting is out of range, fps=%d\n",
			     __func__, fps);
		return -EFAULT;
	}
	DISPMSG("_ioctl_set_vsync fps setting is %d\n", fps);
	/* second parameter means APP set FPS */
	ret = primary_display_force_set_vsync_fps(fps, 1);

	return ret;
}

int _ioctl_query_valid_layer(unsigned long arg)
{
	int ret = 0;
	struct disp_layer_info disp_info_user;
	void __user *argp = (void __user *)arg;

	if (copy_from_user(&disp_info_user, argp, sizeof(disp_info_user))) {
		DISPERR("[FB]: copy_from_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}
	/* check data from userspace is legal */
	if (disp_info_user.layer_num[0] < 0 ||
	    disp_info_user.layer_num[0] > 0x300 ||
	    disp_info_user.layer_num[1] < 0 ||
	    disp_info_user.layer_num[1] > 0x300) {
		DISPERR("[FB]: disp_info_user.layer_num[0]= %d, disp_info_user.layer_num[1]= %d!\n",
			disp_info_user.layer_num[0],
			disp_info_user.layer_num[1]);
		return -EINVAL;
	}

	mutex_lock(&disp_layer_lock);
	ret = layering_rule_start(&disp_info_user, 0);
	mutex_unlock(&disp_layer_lock);

	if (copy_to_user(argp, &disp_info_user, sizeof(disp_info_user))) {
		DISPERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	return ret;
}

int _ioctl_set_scenario(unsigned long arg)
{
	int ret = -1;
	struct disp_scenario_config_t scenario_cfg;
	void __user *argp = (void __user *)arg;

	if (copy_from_user(&scenario_cfg, argp, sizeof(scenario_cfg))) {
		DISPERR("[FB]: copy_to_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	if (DISP_SESSION_TYPE(scenario_cfg.session_id) == DISP_SESSION_PRIMARY)
		ret = primary_display_set_scenario(scenario_cfg.scenario);

	if (ret)
		DISPERR("session(0x%x) set scenario (%d) fail, ret=%d\n",
			scenario_cfg.session_id, scenario_cfg.scenario, ret);

	return ret;
}

int set_session_mode(struct disp_session_config *config_info, int force)
{
	int ret = 0;

	if (DISP_SESSION_TYPE(config_info->session_id) == DISP_SESSION_PRIMARY)
		primary_display_switch_mode(config_info->mode,
					    config_info->session_id, 0);
	else
		DISPERR("[FB]: session(0x%x) switch mode(%d) fail\n",
			config_info->session_id, config_info->mode);
#ifdef CONFIG_MTK_HDMI_SUPPORT
	external_display_switch_mode(config_info->mode, session_config,
				     config_info->session_id);
#endif
	return ret;
}

int _ioctl_set_session_mode(unsigned long arg)
{
	int ret = -1;
	void __user *argp = (void __user *)arg;
	struct disp_session_config config_info;

	if (copy_from_user(&config_info, argp, sizeof(config_info))) {
		DISPERR("[FB]: copy_from_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	if (config_info.mode > DISP_INVALID_SESSION_MODE &&
		config_info.mode < DISP_SESSION_MODE_NUM) {
		ret = set_session_mode(&config_info, 0);
	} else {
		DISPERR("[FB]: session mode is invalid: %d\n",
			config_info.mode);
	}
	return ret;
}

long mtk_disp_mgr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1;

	switch (cmd) {
	case DISP_IOCTL_CREATE_SESSION:
		return _ioctl_create_session(arg);
	case DISP_IOCTL_DESTROY_SESSION:
		return _ioctl_destroy_session(arg);
	case DISP_IOCTL_GET_PRESENT_FENCE:
		return _ioctl_prepare_present_fence(arg);
	case DISP_IOCTL_PREPARE_INPUT_BUFFER:
		return _ioctl_prepare_buffer(arg, PREPARE_INPUT_FENCE);
	case DISP_IOCTL_WAIT_FOR_VSYNC:
		return _ioctl_wait_vsync(arg);
	case DISP_IOCTL_GET_SESSION_INFO:
		return _ioctl_get_info(arg);
	case DISP_IOCTL_GET_DISPLAY_CAPS:
		return _ioctl_get_display_caps(arg);
	case DISP_IOCTL_GET_VSYNC_FPS:
		return _ioctl_get_vsync(arg);
	case DISP_IOCTL_SET_VSYNC_FPS:
		return _ioctl_set_vsync(arg);
	case DISP_IOCTL_SET_SESSION_MODE:
		return _ioctl_set_session_mode(arg);
	case DISP_IOCTL_PREPARE_OUTPUT_BUFFER:
		return _ioctl_prepare_buffer(arg, PREPARE_OUTPUT_FENCE);
	case DISP_IOCTL_FRAME_CONFIG:
		return _ioctl_frame_config(arg);
	case DISP_IOCTL_WAIT_ALL_JOBS_DONE:
		return _ioctl_wait_all_jobs_done(arg);
	case DISP_IOCTL_GET_LCMINDEX:
		return primary_display_get_lcm_index();
	case DISP_IOCTL_QUERY_VALID_LAYER:
		return _ioctl_query_valid_layer(arg);
	case DISP_IOCTL_SET_SCENARIO:
		return _ioctl_set_scenario(arg);
	case DISP_IOCTL_AAL_EVENTCTL:
	case DISP_IOCTL_AAL_GET_HIST:
	case DISP_IOCTL_AAL_INIT_REG:
	case DISP_IOCTL_AAL_SET_PARAM:
	case DISP_IOCTL_SET_GAMMALUT:
	case DISP_IOCTL_SET_CCORR:
	case DISP_IOCTL_CCORR_EVENTCTL:
	case DISP_IOCTL_CCORR_GET_IRQ:
	case DISP_IOCTL_SUPPORT_COLOR_TRANSFORM:
	case DISP_IOCTL_SET_PQPARAM:
	case DISP_IOCTL_GET_PQPARAM:
	case DISP_IOCTL_SET_PQINDEX:
	case DISP_IOCTL_GET_PQINDEX:
	case DISP_IOCTL_SET_COLOR_REG:
	case DISP_IOCTL_SET_TDSHPINDEX:
	case DISP_IOCTL_GET_TDSHPINDEX:
	case DISP_IOCTL_SET_PQ_CAM_PARAM:
	case DISP_IOCTL_GET_PQ_CAM_PARAM:
	case DISP_IOCTL_SET_PQ_GAL_PARAM:
	case DISP_IOCTL_GET_PQ_GAL_PARAM:
	case DISP_IOCTL_PQ_SET_BYPASS_COLOR:
	case DISP_IOCTL_PQ_SET_WINDOW:
	case DISP_IOCTL_OD_CTL:
	case DISP_IOCTL_WRITE_REG:
	case DISP_IOCTL_READ_REG:
	case DISP_IOCTL_MUTEX_CONTROL:
	case DISP_IOCTL_PQ_GET_TDSHP_FLAG:
	case DISP_IOCTL_PQ_SET_TDSHP_FLAG:
	case DISP_IOCTL_PQ_GET_DC_PARAM:
	case DISP_IOCTL_PQ_SET_DC_PARAM:
	case DISP_IOCTL_PQ_GET_DS_PARAM:
	case DISP_IOCTL_PQ_GET_MDP_COLOR_CAP:
	case DISP_IOCTL_PQ_GET_MDP_TDSHP_REG:
	case DISP_IOCTL_WRITE_SW_REG:
	case DISP_IOCTL_READ_SW_REG:
	{
#ifndef NO_PQ_IOCTL
		ret = primary_display_user_cmd(cmd, arg);
#endif
		break;
	}
	default:
		DISPWARN("[session]ioctl not supported, 0x%08x\n", cmd);
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
const char *_session_compat_ioctl_spy(unsigned int cmd)
{
	switch (cmd) {
	case COMPAT_DISP_IOCTL_CREATE_SESSION:
		return "DISP_IOCTL_CREATE_SESSION";
	case COMPAT_DISP_IOCTL_DESTROY_SESSION:
		return "DISP_IOCTL_DESTROY_SESSION";
	case COMPAT_DISP_IOCTL_TRIGGER_SESSION:
		return "DISP_IOCTL_TRIGGER_SESSION";
	case COMPAT_DISP_IOCTL_SET_INPUT_BUFFER:
		return "DISP_IOCTL_SET_INPUT_BUFFER";
	case COMPAT_DISP_IOCTL_PREPARE_INPUT_BUFFER:
		return "DISP_IOCTL_PREPARE_INPUT_BUFFER";
	case COMPAT_DISP_IOCTL_WAIT_FOR_VSYNC:
		return "DISP_IOCL_WAIT_FOR_VSYNC";
	case COMPAT_DISP_IOCTL_GET_SESSION_INFO:
		return "DISP_IOCTL_GET_SESSION_INFO";
	case COMPAT_DISP_IOCTL_PREPARE_OUTPUT_BUFFER:
		return "DISP_IOCTL_PREPARE_OUTPUT_BUFFER";
	case COMPAT_DISP_IOCTL_SET_OUTPUT_BUFFER:
		return "DISP_IOCTL_SET_OUTPUT_BUFFER";
	case COMPAT_DISP_IOCTL_SET_SESSION_MODE:
		return "DISP_IOCTL_SET_SESSION_MODE";
	default:
		break;
	}
	return "unknown";
}

static long mtk_disp_mgr_compat_ioctl(struct file *file, unsigned int cmd,
				      unsigned long arg)
{
	long ret = -ENOIOCTLCMD;
	void __user *data32 = compat_ptr(arg);

	switch (cmd) {
	case COMPAT_DISP_IOCTL_CREATE_SESSION:
		return _compat_ioctl_create_session(file, arg);
	case COMPAT_DISP_IOCTL_DESTROY_SESSION:
		return _compat_ioctl_destroy_session(file, arg);
	case COMPAT_DISP_IOCTL_TRIGGER_SESSION:
		return _compat_ioctl_trigger_session(file, arg);
	case COMPAT_DISP_IOCTL_GET_PRESENT_FENCE:
		return _compat_ioctl_prepare_present_fence(file, arg);
	case COMPAT_DISP_IOCTL_PREPARE_INPUT_BUFFER:
		return _compat_ioctl_prepare_buffer(file, arg,
						    PREPARE_INPUT_FENCE);
	case COMPAT_DISP_IOCTL_SET_INPUT_BUFFER:
		return _compat_ioctl_set_input_buffer(file, arg);
	case COMPAT_DISP_IOCTL_FRAME_CONFIG:
		return _compat_ioctl_frame_config(file, arg);
	case COMPAT_DISP_IOCTL_WAIT_FOR_VSYNC:
		return _compat_ioctl_wait_vsync(file, arg);
	case COMPAT_DISP_IOCTL_GET_SESSION_INFO:
		return _compat_ioctl_get_info(file, arg);
	case COMPAT_DISP_IOCTL_GET_DISPLAY_CAPS:
		return _compat_ioctl_get_display_caps(file, arg);
	case COMPAT_DISP_IOCTL_GET_VSYNC_FPS:
		return _compat_ioctl_get_vsync(file, arg);
	case COMPAT_DISP_IOCTL_SET_VSYNC_FPS:
		return _compat_ioctl_set_vsync(file, arg);
	case COMPAT_DISP_IOCTL_SET_SESSION_MODE:
		return _compat_ioctl_set_session_mode(file, arg);
	case COMPAT_DISP_IOCTL_PREPARE_OUTPUT_BUFFER:
		return _compat_ioctl_prepare_buffer(file, arg,
						    PREPARE_OUTPUT_FENCE);
	case COMPAT_DISP_IOCTL_SET_OUTPUT_BUFFER:
		return _compat_ioctl_set_output_buffer(file, arg);
	case DISP_IOCTL_SET_SCENARIO:
		/*
		 * arg of this ioctl is all unsigned int,
		 * don't need special compat ioctl
		 */
		return file->f_op->unlocked_ioctl(file, cmd,
						  (unsigned long)data32);

	case DISP_IOCTL_AAL_GET_HIST:
	case DISP_IOCTL_AAL_EVENTCTL:
	case DISP_IOCTL_AAL_INIT_REG:
	case DISP_IOCTL_AAL_SET_PARAM:
#ifndef NO_PQ_IOCTL
	{
		void __user *data32;

		data32 = compat_ptr(arg);
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)data32);
		return ret;
	}
#endif
	case DISP_IOCTL_SET_GAMMALUT:
	case DISP_IOCTL_SET_CCORR:
	case DISP_IOCTL_CCORR_EVENTCTL:
	case DISP_IOCTL_CCORR_GET_IRQ:
	case DISP_IOCTL_SUPPORT_COLOR_TRANSFORM:
	case DISP_IOCTL_SET_PQPARAM:
	case DISP_IOCTL_GET_PQPARAM:
	case DISP_IOCTL_SET_PQINDEX:
	case DISP_IOCTL_GET_PQINDEX:
	case DISP_IOCTL_SET_COLOR_REG:
	case DISP_IOCTL_SET_TDSHPINDEX:
	case DISP_IOCTL_GET_TDSHPINDEX:
	case DISP_IOCTL_SET_PQ_CAM_PARAM:
	case DISP_IOCTL_GET_PQ_CAM_PARAM:
	case DISP_IOCTL_SET_PQ_GAL_PARAM:
	case DISP_IOCTL_GET_PQ_GAL_PARAM:
	case DISP_IOCTL_PQ_SET_BYPASS_COLOR:
	case DISP_IOCTL_PQ_SET_WINDOW:
	case DISP_IOCTL_OD_CTL:
	case DISP_IOCTL_WRITE_REG:
	case DISP_IOCTL_READ_REG:
	case DISP_IOCTL_MUTEX_CONTROL:
	case DISP_IOCTL_PQ_GET_TDSHP_FLAG:
	case DISP_IOCTL_PQ_SET_TDSHP_FLAG:
	case DISP_IOCTL_PQ_GET_DC_PARAM:
	case DISP_IOCTL_PQ_GET_DS_PARAM:
	case DISP_IOCTL_PQ_SET_DC_PARAM:
	case DISP_IOCTL_PQ_GET_MDP_COLOR_CAP:
	case DISP_IOCTL_PQ_GET_MDP_TDSHP_REG:
	case DISP_IOCTL_WRITE_SW_REG:
	case DISP_IOCTL_READ_SW_REG:
	{
#ifndef NO_PQ_IOCTL
		ret = primary_display_user_cmd(cmd, arg);
#endif
		break;
	}

	default:
		DISPWARN("[%s]ioctl not supported, 0x%08x\n", __func__, cmd);
		return -ENOIOCTLCMD;
	}

	return ret;
}
#endif

static const struct file_operations mtk_disp_mgr_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mtk_disp_mgr_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mtk_disp_mgr_compat_ioctl,
#endif

	.open = mtk_disp_mgr_open,
	.release = mtk_disp_mgr_release,
	.flush = mtk_disp_mgr_flush,
	.read = mtk_disp_mgr_read,
};

static int mtk_disp_mgr_probe(struct platform_device *pdev)
{
	struct class_device;
	struct class_device *class_dev = NULL;
	int ret;

	pr_debug("mtk_disp_mgr_probe called!\n");

	if (alloc_chrdev_region(&mtk_disp_mgr_devno, 0, 1, DISP_SESSION_DEVICE))
		return -EFAULT;

	mtk_disp_mgr_cdev = cdev_alloc();
	mtk_disp_mgr_cdev->owner = THIS_MODULE;
	mtk_disp_mgr_cdev->ops = &mtk_disp_mgr_fops;

	ret = cdev_add(mtk_disp_mgr_cdev, mtk_disp_mgr_devno, 1);
	if (ret) {
		DISPERR("cdev_add failed!\n");
		unregister_chrdev_region(mtk_disp_mgr_devno, 1);
		return ret;
	}

	mtk_disp_mgr_class = class_create(THIS_MODULE, DISP_SESSION_DEVICE);
	class_dev = (struct class_device *)device_create(mtk_disp_mgr_class,
						NULL, mtk_disp_mgr_devno,
						NULL, DISP_SESSION_DEVICE);
	disp_sync_init();
#ifdef CONFIG_MTK_HDMI_SUPPORT
	external_display_control_init();
#endif

	return 0;
}

static int mtk_disp_mgr_remove(struct platform_device *pdev)
{
	return 0;
}

static void mtk_disp_mgr_shutdown(struct platform_device *pdev)
{
}

static int mtk_disp_mgr_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int mtk_disp_mgr_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver mtk_disp_mgr_driver = {
	.probe = mtk_disp_mgr_probe,
	.remove = mtk_disp_mgr_remove,
	.shutdown = mtk_disp_mgr_shutdown,
	.suspend = mtk_disp_mgr_suspend,
	.resume = mtk_disp_mgr_resume,
	.driver = {

			.name = DISP_SESSION_DEVICE,
		},
};

static void mtk_disp_mgr_device_release(struct device *dev)
{
}

static u64 mtk_disp_mgr_dmamask = ~(u32)0;

static struct platform_device mtk_disp_mgr_device = {
	.name = DISP_SESSION_DEVICE,
	.id = 0,
	.dev = {

			.release = mtk_disp_mgr_device_release,
			.dma_mask = &mtk_disp_mgr_dmamask,
			.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = 0,
};

static int __init mtk_disp_mgr_init(void)
{
	pr_debug("mtk_disp_mgr_init\n");
	if (platform_device_register(&mtk_disp_mgr_device))
		return -ENODEV;

	if (platform_driver_register(&mtk_disp_mgr_driver)) {
		platform_device_unregister(&mtk_disp_mgr_device);
		return -ENODEV;
	}
	return 0;
}

static void __exit mtk_disp_mgr_exit(void)
{
	cdev_del(mtk_disp_mgr_cdev);
	unregister_chrdev_region(mtk_disp_mgr_devno, 1);

	platform_driver_unregister(&mtk_disp_mgr_driver);
	platform_device_unregister(&mtk_disp_mgr_device);

	device_destroy(mtk_disp_mgr_class, mtk_disp_mgr_devno);
	class_destroy(mtk_disp_mgr_class);
}
module_init(mtk_disp_mgr_init);
module_exit(mtk_disp_mgr_exit);

MODULE_DESCRIPTION("MediaTek Display Manager");
