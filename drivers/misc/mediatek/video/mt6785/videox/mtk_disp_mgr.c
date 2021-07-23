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
#include <linux/sched/clock.h>
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

#ifdef CONFIG_MTK_M4U
#include "m4u.h"
#endif
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

/* 2nd display */
#include "extd_multi_control.h"
#include "external_display.h"
#include "extd_platform.h"

#include "layering_rule.h"
#include "compat_mtk_disp_mgr.h"
#include "disp_partial.h"
#include "frame_queue.h"
#include "disp_lowpower.h"
#include "ddp_rsz.h"
#include "disp_tphint.h"


#define DDP_OUTPUT_LAYID 4

#if defined(MTK_FB_SHARE_WDMA0_SUPPORT)
static int idle_flag = 1;
static int smartovl_flag;
/* wfd connected(session is existing whereas ext mode or dcm mode),
 * or screenrecord
 */
static int has_memory_session;
#endif

/* @g_session: SESSION_TYPE | DEVICE_ID */
static unsigned int g_session[MAX_SESSION_COUNT];
static DEFINE_MUTEX(disp_session_lock);
static DEFINE_MUTEX(disp_layer_lock);

static dev_t mtk_disp_mgr_devno;
static struct cdev *mtk_disp_mgr_cdev;
static struct class *mtk_disp_mgr_class;

/*---------------- variable for repaint start ------------------*/
static DEFINE_MUTEX(repaint_queue_lock);
static DECLARE_WAIT_QUEUE_HEAD(repaint_wq);
static LIST_HEAD(repaint_job_queue);
static LIST_HEAD(repaint_job_pool);

static int HWC_gpid;

inline int get_HWC_gpid(void)
{
	return HWC_gpid;
}

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

static int mtk_disp_mgr_mmap(struct file *file, struct vm_area_struct *vma)
{
	static const unsigned long addr_min = 0x14000000;
	static const unsigned long addr_max = 0x14025000;
	static const unsigned long size = addr_max - addr_min;
	const unsigned long require_size = vma->vm_end - vma->vm_start;
	unsigned long pa_start = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pa_end = pa_start + require_size;

	DISPDBG(
		"mmap size %ld, vm_pg0ff 0x%08lx, pa_start 0x%08lx, pa_end 0x%08lx\n",
		require_size, vma->vm_pgoff, pa_start, pa_end);

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (require_size > size || (pa_start < addr_min || pa_end > addr_max)) {
		DISP_PR_ERR("mmap size range over flow!\n");
		return -EAGAIN;
	}
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    (vma->vm_end - vma->vm_start), vma->vm_page_prot)) {
		DISP_PR_ERR("display mmap failed!\n");
		return -EAGAIN;
	}

	return 0;
}

int _session_inited(struct disp_session_config config)
{
	return 0;
}

int disp_mgr_has_mem_session(void)
{
#if defined(MTK_FB_SHARE_WDMA0_SUPPORT)
	return has_memory_session;
#else
	return 0;
#endif
}

/**
 * return value: 0 on success, -1 on failure
 */
int disp_create_session(struct disp_session_config *config)
{
	int ret = 0;
	int is_session_inited = 0;
	unsigned int session = 0;
	int i, idx = -1;

	session = MAKE_DISP_SESSION(config->type, config->device_id);

	/* 1.Check if this session exists already */
	mutex_lock(&disp_session_lock);
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (g_session[i] == session) {
			is_session_inited = 1;
			idx = i;
			DISPDBG("%s: session already existed:0x%08x\n",
				__func__, session);
			break;
		}
	}

	if (is_session_inited == 1) {
		config->session_id = session;
		goto done;
	}

	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (g_session[i] == 0 && idx == -1) {
			idx = i;
			break;
		}
	}

	if (idx == -1) {
		DISP_PR_ERR("invalid session creation request\n");
		ret = -1;
		goto done;
	}

	/* 1.Check if support this session (mode,type,dev) */
	/* 2.Create this session */
	config->session_id = session;
	g_session[idx] = session;

#if defined(MTK_FB_SHARE_WDMA0_SUPPORT)
	if (session == MAKE_DISP_SESSION(DISP_SESSION_MEMORY, DEV_WFD)) {
		/*
		 * disable dynamic switch for screen idle,
		 * avoid conflict it needs lock, if set_idlemgr.
		 *
		 * backup and disable idle manager and smart_ovl
		 */
		if (idle_flag)
			idle_flag = set_idlemgr(0, 1);

		smartovl_flag = disp_helper_get_option(DISP_OPT_SMART_OVL);
		disp_helper_set_option(DISP_OPT_SMART_OVL, 0);

		has_memory_session = 1;
	}
#endif
	DISPDBG("new session(0x%08x)\n", session);

done:
	mutex_unlock(&disp_session_lock);

	DISPDBG("%s done\n", __func__);
	return ret;
}

static int release_session_buffer(unsigned int session)
{
	mutex_lock(&disp_session_lock);

	if (session == 0) {
		DISP_PR_ERR("%s: session id:0x%08x\n", __func__, session);
		mutex_unlock(&disp_session_lock);
		return -1;
	}

	mtkfb_release_session_fence(session);

	mutex_unlock(&disp_session_lock);
	return 0;
}

/**
 * return value: 0 on success, -1 on failure
 */
int disp_destroy_session(struct disp_session_config *config)
{
	int ret = -1;
	unsigned int session = config->session_id;
	int i;

	DISPMSG("%s: session(0x%08x)\n", __func__, config->session_id);

	/* 1. check if this session exists already. If yes, remove it */
	mutex_lock(&disp_session_lock);
	for (i = 0; i < MAX_SESSION_COUNT; i++) {
		if (g_session[i] != session)
			continue;

		g_session[i] = 0;

		ret = 0;
		break;
	}
	mutex_unlock(&disp_session_lock);

	/* 2. destroy this session */
	if (DISP_SESSION_TYPE(config->session_id) != DISP_SESSION_PRIMARY) {
		external_display_switch_mode(config->mode, g_session,
					     config->session_id);
		release_session_buffer(config->session_id);
	}

	if (ret == 0) {
#ifdef MTK_FB_SHARE_WDMA0_SUPPORT
		if (session == MAKE_DISP_SESSION(DISP_SESSION_MEMORY,
			    DEV_WFD)) {
			/*it need lock, if set_idlemgr.*/
			if (idle_flag)
				set_idlemgr(idle_flag, 1);
			if (smartovl_flag)
				disp_helper_set_option(DISP_OPT_SMART_OVL,
					smartovl_flag);
			has_memory_session = 0;
		}
#endif
		DISPMSG("destroy session(0x%08x)\n", session);
	} else
		DISP_PR_ERR("session(0x%08x) does not exist\n", session);

	return ret;
}

int _ioctl_create_session(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct disp_session_config config;

	if (copy_from_user(&config, argp, sizeof(config))) {
		DISP_PR_ERR("[FB] copy_from_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	if (disp_create_session(&config))
		ret = -EFAULT;

	if (copy_to_user(argp, &config, sizeof(config))) {
		DISP_PR_ERR("[FB] copy_to_user failed! line:%d\n", __LINE__);
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
		DISP_PR_ERR("[FB] copy_from_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	if (disp_destroy_session(&config))
		ret = -EFAULT;

	return ret;
}

char *disp_session_type_str(unsigned int session)
{
	switch (DISP_SESSION_TYPE(session)) {
	case DISP_SESSION_PRIMARY:
		return "P";
	case DISP_SESSION_EXTERNAL:
		return "E";
	case DISP_SESSION_MEMORY:
		return "M";
	default:
		break;
	}
	return "unknown-session_type";
}

int _ioctl_prepare_present_fence(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct fence_data data;
	struct disp_present_fence pf;
	static unsigned int fence_idx;
	struct disp_sync_info *l_info = NULL;
	int tl = disp_sync_get_present_timeline_id();

	if (copy_from_user(&pf, (void __user *)arg, sizeof(pf))) {
		pr_err("[FB Driver] copy_from_user failed! line:%d\n",
		       __LINE__);
		return -EFAULT;
	}

	if (DISP_SESSION_TYPE(pf.session_id) != DISP_SESSION_PRIMARY) {
		DISP_PR_INFO(
			"non-primary ask for present fence! session=0x%08x\n",
			     pf.session_id);
		data.fence = MTK_FB_INVALID_FENCE_FD;
		data.value = 0;
	} else {
		l_info = disp_sync_get_layer_info(pf.session_id, tl);
		if (!l_info) {
			DISP_PR_ERR("layer_info is null\n");
			ret = -EFAULT;
			return ret;
		}
		/* create fence */
		data.fence = MTK_FB_INVALID_FENCE_FD;
		data.value = ++fence_idx;
		ret = fence_create(l_info->timeline, &data);
		if (ret) {
			DISP_PR_ERR("%s%d,L%d create Fence Object failed!\n",
				    disp_session_type_str(pf.session_id),
				    DISP_SESSION_DEV(pf.session_id), tl);
			ret = -EFAULT;
		}
	}

	pf.present_fence_fd = data.fence;
	pf.present_fence_index = data.value;
	if (copy_to_user(argp, &pf, sizeof(pf))) {
		pr_err("[FB Driver] copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

	mmprofile_log_ex(ddp_mmp_get_events()->primary_present_fence_get,
			 MMPROFILE_FLAG_PULSE, pf.present_fence_fd,
			 pf.present_fence_index);
	DISPFENCE("P+/%s%d/L%d/idx%d/fd%d\n",
		  disp_session_type_str(pf.session_id),
		  DISP_SESSION_DEV(pf.session_id), tl,
		  pf.present_fence_index, pf.present_fence_fd);
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
		DISP_PR_ERR(" FAIL:P+/%s%d/L%u/e%d/ion%d/c%d/idx%d/fd%d\n",
			    disp_session_type_str(disp_buf->session_id),
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
	struct disp_buffer_info disp_buf;
	struct mtkfb_fence_buf_info *fence_buf = NULL, *fence_buf2 = NULL;

	if (copy_from_user(&disp_buf, (void __user *)arg, sizeof(disp_buf))) {
		pr_err("[FB Driver] copy_from_user failed! line:%d\n",
		       __LINE__);
		return -EFAULT;
	}

	if (type == PREPARE_INPUT_FENCE)
		DISPDBG("There is do nothing in input fence.\n");
	else if (type == PREPARE_PRESENT_FENCE)
		disp_buf.layer_id = disp_sync_get_present_timeline_id();
	else if (type == PREPARE_OUTPUT_FENCE)
		disp_buf.layer_id = disp_sync_get_output_timeline_id();
	else
		DISP_PR_INFO("type is wrong: %d\n", type);

	if (disp_buf.layer_en) {
		fence_buf = disp_sync_prepare_buf(&disp_buf);
		if (fence_buf) {
			disp_buf.fence_fd = fence_buf->fence;
			disp_buf.index = fence_buf->idx;
		} else {
			DISP_PR_ERR(
				" FAIL:P+/%s%d/L%d/e%d/ion%d/c%d/idx%d/fd%d\n",
				    disp_session_type_str(disp_buf.session_id),
				    DISP_SESSION_DEV(disp_buf.session_id),
				    disp_buf.layer_id, disp_buf.layer_en,
				    disp_buf.ion_fd, disp_buf.cache_sync,
				    disp_buf.index, disp_buf.fence_fd);
			disp_buf.fence_fd = MTK_FB_INVALID_FENCE_FD;
			disp_buf.index = 0;
		}

		if (type == PREPARE_OUTPUT_FENCE)
			__prepare_output_buffer(&disp_buf, fence_buf2);
	} else {
		DISP_PR_ERR(" FAIL:P+/%s%d/L%d/e%d/ion%d/c%d/idx%d/fd%d\n",
			    disp_session_type_str(disp_buf.session_id),
			    DISP_SESSION_DEV(disp_buf.session_id),
			    disp_buf.layer_id, disp_buf.layer_en,
			    disp_buf.ion_fd, disp_buf.cache_sync,
			    disp_buf.index, disp_buf.fence_fd);
		disp_buf.fence_fd = MTK_FB_INVALID_FENCE_FD;
		disp_buf.index = 0;
	}

	if (copy_to_user(argp, &disp_buf, sizeof(disp_buf))) {
		pr_err("[FB Driver] copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}
	return ret;
}

const char *_disp_format_str(enum DISP_FORMAT format)
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
	case DISP_FORMAT_RGBA1010102:
		return "RGBA1010102";
	case DISP_FORMAT_PRGBA1010102:
		return "PRGBA1010102";
	case DISP_FORMAT_RGBA_FP16:
		return "RGBA_FP16";
	case DISP_FORMAT_PRGBA_FP16:
		return "PRGBA_FP16";
	default:
		break;
	}
	return "unknown-format";
}

void dump_input_cfg_info(struct disp_input_config *input_cfg,
			 unsigned int session, int is_err)
{
	_DISP_PRINT_FENCE_OR_ERR(is_err,
		"S+/%s%d/L%u/idx%u/(%u,%u,%ux%u)(%u,%u,%ux%u)/%s/ds%d/%u/mva0x%08lx/compr:%u/v_p:%u/t%d/s%d\n",
		disp_session_type_str(session), DISP_SESSION_DEV(session),
		input_cfg->layer_id, input_cfg->next_buff_idx,
		input_cfg->src_offset_x, input_cfg->src_offset_y,
		input_cfg->src_width, input_cfg->src_height,
		input_cfg->tgt_offset_x, input_cfg->tgt_offset_y,
		input_cfg->tgt_width, input_cfg->tgt_height,
		_disp_format_str(input_cfg->src_fmt), input_cfg->dataspace,
		input_cfg->src_pitch, (unsigned long)(input_cfg->src_phy_addr),
		input_cfg->compress, input_cfg->src_v_pitch,
		get_ovl2mem_ticket(), input_cfg->security);
}

static int _get_layer_cnt(unsigned int session)
{
	if (DISP_SESSION_TYPE(session) == DISP_SESSION_PRIMARY)
		return primary_display_get_max_layer();
#if ((defined CONFIG_MTK_HDMI_SUPPORT) || \
	(defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)))
	else if (DISP_SESSION_TYPE(session) == DISP_SESSION_EXTERNAL)
		return ext_disp_get_max_layer();
#endif
	else if (DISP_SESSION_TYPE(session) == DISP_SESSION_MEMORY)
		return ovl2mem_get_max_layer();

	DISP_PR_INFO("invalid session_id(0x%08x)\n", session);
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
	    ((cfg->src_fmt >> 8) >= (DISP_FORMAT_NUM >> 8))) {
		disp_aee_print("L%d,src_fmt=0x%x is invalid color format\n",
			       cfg->layer_id, cfg->src_fmt);
		return -1;
	}

	return 0;
}

static int disp_validate_output_params(struct disp_output_config *cfg)
{
	if ((cfg->fmt <= 0) || ((cfg->fmt >> 8) == 15) ||
	    ((cfg->fmt >> 8) >= (DISP_FORMAT_NUM >> 8))) {
		disp_aee_print("output fmt=0x%x is invalid color format\n",
			       cfg->fmt);
		return -1;
	}

	return 0;
}

static int _get_max_layer(unsigned int session_id)
{
	if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_PRIMARY)
		return primary_display_get_max_layer();
#if ((defined CONFIG_MTK_HDMI_SUPPORT) || \
	(defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)))
	else if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_EXTERNAL)
		return ext_disp_get_max_layer();
#endif
	else if (DISP_SESSION_TYPE(session_id) == DISP_SESSION_MEMORY)
		return ovl2mem_get_max_layer();

	DISP_PR_INFO("session_id is wrong!!\n");
	return 0;
}


int disp_validate_ioctl_params(struct disp_frame_cfg_t *cfg)
{
	int i, max_layer_num;

	max_layer_num = _get_max_layer(cfg->session_id);
	if (max_layer_num <= 0)
		return -1;

	if (cfg->input_layer_num > max_layer_num) {
		disp_aee_print("sess:0x%x layer_num %d>%d\n",
			       cfg->session_id, cfg->input_layer_num,
			       max_layer_num);
		return -1;
	}

	for (i = 0; i < cfg->input_layer_num; i++)
		if (disp_validate_input_params(&cfg->input_cfg[i],
			    max_layer_num))
			return -1;

	if (cfg->output_en && disp_validate_output_params(&cfg->output_cfg))
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
			goto error;

		/* alloc mem for partial update dirty ROIs */
		if (WARN_ON(cfg->dirty_roi_num > 20)) {
			/* disable partial for this frame */
			goto error;
		}

		size = cfg->dirty_roi_num * sizeof(struct layer_dirty_roi);
		addr = kmalloc(size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(addr))
			goto error;

		if (copy_from_user(addr, cfg->dirty_roi_addr, size)) {
			DISP_PR_ERR(
				"[dirty roi] copy_from_user failed! line:%d\n",
				    __LINE__);
			DISP_PR_ERR("to=0x%p, from=0x%p, size=0x%lx\n",
				    addr, cfg->dirty_roi_addr, size);
			kfree(addr);
			goto error;
		}

		cfg->dirty_roi_addr = addr;

		continue;

error:
		cfg->dirty_roi_num = 0;
		cfg->dirty_roi_addr = NULL;
	}
	return 0;
}

int disp_input_free_dirty_roi(struct disp_frame_cfg_t *cfg)
{
	int i;

	if (cfg == NULL)
		return 0;

	for (i = 0; i < cfg->input_layer_num; i++) {
		if (i >= _get_max_layer(cfg->session_id))
			break;
		if (!cfg->input_cfg[i].layer_enable ||
			!cfg->input_cfg[i].dirty_roi_num)
			continue;
		if (cfg->input_cfg[i].dirty_roi_addr != NULL) {
			kfree(cfg->input_cfg[i].dirty_roi_addr);
			cfg->input_cfg[i].dirty_roi_addr = NULL;
		}
	}

	return 0;
}

static int input_config_preprocess(struct disp_frame_cfg_t *cfg)
{
	int i = 0, is_err = 0;
	int layer_id = 0;
	unsigned int dst_size = 0;
	unsigned int session = 0;
	unsigned int mva_offset = 0;
	struct disp_session_sync_info *s_info = NULL;
	enum DISP_FORMAT src_fmt;

	session = cfg->session_id;
	s_info = disp_get_session_sync_info_for_debug(session);

	if (cfg->input_layer_num == 0 || cfg->input_layer_num >
	    _get_layer_cnt(session)) {
		DISP_PR_ERR(
			"set_%s_buffer, config_layer_num invalid = %d, max_layer_num = %d!\n",
			    disp_session_type_str(session),
			    cfg->input_layer_num, _get_layer_cnt(session));
		return 0;
	}

	disp_input_get_dirty_roi(cfg);
	_DISP_PRINT_FENCE_OR_ERR(0, "HRT_idx %u\n", cfg->hrt_idx);
	for (i = 0; i < cfg->input_layer_num; i++) {
		struct disp_input_config *c = &cfg->input_cfg[i];
		unsigned long dst_mva = 0;

		layer_id = c->layer_id;
		if (layer_id >= _get_layer_cnt(session)) {
			DISP_PR_ERR("set_%s_buffer, invalid layer_id = %d!\n",
				    disp_session_type_str(session), layer_id);
			continue;
		}

		if (c->layer_enable) {
			unsigned int Bpp, x, y, pitch;
			enum UNIFIED_COLOR_FMT fmt = UFMT_UNKNOWN;
#ifdef DISP_SYNC_ENABLE
			struct sync_fence *src_fence = NULL;
#endif

			if (c->buffer_source == DISP_BUFFER_ALPHA) {
				DISPFENCE("%sL%d is dim layer,idx%u\n",
					  disp_session_type_str(session),
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
				disp_sync_query_buf_info(session, layer_id,
						(unsigned int)c->next_buff_idx,
						&dst_mva, &dst_size);
			}
			c->src_phy_addr = (void *)dst_mva;
			if (!dst_mva) {
				DISP_PR_ERR(
					"disable L%d because of no valid mva\n",
					    c->layer_id);
				c->layer_enable = 0;
				is_err = 1;
			}

#ifdef DISP_SYNC_ENABLE
			/* get src fence */
			src_fence = sync_fence_fdget(c->src_fence_fd);
			if (!src_fence && c->src_fence_fd != -1) {
				DISP_PR_ERR(
					"error to get src_fence from fd%d\n",
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
			src_fmt = c->src_fmt;
			fmt = disp_fmt_to_unified_fmt(src_fmt);
			Bpp = UFMT_GET_bpp(fmt) / 8;

			mva_offset = (x + y * pitch) * Bpp;
			mtkfb_update_buf_info(cfg->session_id, c->layer_id,
					      c->next_buff_idx, mva_offset,
					      c->frm_sequence);

			dump_input_cfg_info(&cfg->input_cfg[i], session,
					    is_err);
		} else {
			c->src_fence_struct = NULL;
		}

		disp_sync_put_cached_layer_info(session, layer_id,
						&cfg->input_cfg[i], dst_mva);

		if (s_info) {
			dprec_submit(&s_info->event_frame_cfg, c->next_buff_idx,
				     (cfg->input_layer_num << 28) |
				     (c->layer_id << 24) | (c->src_fmt << 12) |
				     c->layer_enable);
			dprec_submit(&s_info->event_frame_cfg, c->next_buff_idx,
				     (unsigned long)c->src_phy_addr);
		}
	}
	return 0;
}

static int output_config_preprocess(struct disp_frame_cfg_t *cfg)
{
	unsigned int session = 0;
	unsigned long dst_mva = 0;
	unsigned int dst_size;
	struct disp_session_sync_info *s_info;
#ifdef DISP_SYNC_ENABLE
	struct sync_fence *src_fence;
#endif

	session = cfg->session_id;
	s_info = disp_get_session_sync_info_for_debug(session);

	if (cfg->output_cfg.pa) {
		dst_mva = (unsigned long)(cfg->output_cfg.pa);
	} else {
		disp_sync_query_buf_info_nosync(session,
				disp_sync_get_output_timeline_id(),
				cfg->output_cfg.buff_idx, &dst_mva, &dst_size);
	}
	cfg->output_cfg.pa = (void *)dst_mva;
	if (!dst_mva) {
		DISP_PR_INFO("%s output mva=0! skip it\n", __func__);
		cfg->output_en = 0;
		goto out;
	}

#ifdef DISP_SYNC_ENABLE
	/* get src fence */
	src_fence = sync_fence_fdget(cfg->output_cfg.src_fence_fd);
	if (!src_fence && cfg->output_cfg.src_fence_fd != -1) {
		DISP_PR_ERR("error to get src_fence from output fd %d\n",
			    cfg->output_cfg.src_fence_fd);
	}
	cfg->output_cfg.src_fence_struct = src_fence;
#endif
	if (DISP_SESSION_TYPE(session) == DISP_SESSION_PRIMARY) {
		/* must be mirror mode */
		if (primary_display_is_decouple_mode()) {
			disp_sync_put_cached_layer_info_v2(session,
				disp_sync_get_output_interface_timeline_id(),
				cfg->output_cfg.interface_idx, 1, dst_mva);
		}
	}

	DISPFENCE(
		"S+O/%s%d/L%d/idx%u/L%d/idx%u/(%u,%u,%ux%u)/%s/%u/%u/pa0x%08lx/mva0x%08lx/t%d/sec%d\n",
		  disp_session_type_str(session), DISP_SESSION_DEV(session),
		  disp_sync_get_output_timeline_id(), cfg->output_cfg.buff_idx,
		  disp_sync_get_output_interface_timeline_id(),
		  cfg->output_cfg.interface_idx,
		  cfg->output_cfg.x, cfg->output_cfg.y,
		  cfg->output_cfg.width, cfg->output_cfg.height,
		  _disp_format_str(cfg->output_cfg.fmt),
		  cfg->output_cfg.pitch, cfg->output_cfg.pitchUV,
		  (unsigned long)cfg->output_cfg.pa, dst_mva,
		  get_ovl2mem_ticket(), cfg->output_cfg.security);

	mtkfb_update_buf_info(cfg->session_id,
			      disp_sync_get_output_interface_timeline_id(),
			      cfg->output_cfg.buff_idx, 0,
			      cfg->output_cfg.frm_sequence);

	if (s_info) {
		dprec_submit(&s_info->event_frame_cfg,
			     cfg->output_cfg.buff_idx, dst_mva);
	}

out:
	return 0;
}

static int do_frame_config(struct frame_queue_t *frame_node)
{
	struct disp_frame_cfg_t *cfg = &frame_node->frame_cfg;
	int s_type = DISP_SESSION_TYPE(cfg->session_id);

	if (s_type == DISP_SESSION_PRIMARY) {
		primary_display_frame_cfg(cfg);
#if ((defined CONFIG_MTK_HDMI_SUPPORT) || \
	(defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)))
	} else if (s_type == DISP_SESSION_EXTERNAL) {
		external_display_frame_cfg(cfg);
#endif
	} else if (s_type == DISP_SESSION_MEMORY) {
		ovl2mem_frame_cfg(cfg);
	} else {
		DISP_PR_INFO("invalid session:0x%08x\n", cfg->session_id);
		return -1;
	}

	return 0;
}

static long __frame_queue_config(unsigned long arg)
{
	void *ret_val = NULL;
	struct frame_queue_head_t *head;
	struct disp_frame_cfg_t *cfg;
	struct sync_fence *present_fence = NULL;
	struct disp_session_sync_info *s_info;
	struct frame_queue_t *frame_node;

	frame_node = frame_queue_node_create();
	if (IS_ERR_OR_NULL(frame_node)) {
		ret_val = ERR_PTR(-ENOMEM);
		goto Error;
	}

	/* this is initialized correctly when get node from framequeue list */
	cfg = &frame_node->frame_cfg;

	if (copy_from_user(cfg, (void __user *)arg,
		    sizeof(*cfg))) {
		ret_val = ERR_PTR(-EFAULT);
		DISP_PR_INFO("copy_from_user failed! line:%d\n", __LINE__);
		goto Error;
	}

	if (disp_validate_ioctl_params(cfg)) {
		ret_val = ERR_PTR(-EINVAL);
		goto Error;
	}

	head = get_frame_queue_head(cfg->session_id);
	if (!head) {
		disp_aee_print("error to get frame queue!!\n");
		return -EINVAL;
	}

	s_info = disp_get_session_sync_info_for_debug(cfg->session_id);
	if (s_info)
		dprec_start(&s_info->event_frame_cfg,
			    cfg->present_fence_idx, 0);

	cfg->setter = SESSION_USER_HWC;

#ifdef DISP_SYNC_ENABLE
	/* get present fence */
	if (cfg->prev_present_fence_fd != -1) {
		present_fence = sync_fence_fdget(cfg->prev_present_fence_fd);
		if (!present_fence) {
			DISP_PR_ERR(
				"error to get prev_present_fence from fd %d\n",
				    cfg->prev_present_fence_fd);
		}
	}
#endif
	cfg->prev_present_fence_struct = present_fence;

	frame_node->do_frame_cfg = do_frame_config;

	input_config_preprocess(cfg);
	if (cfg->output_en)
		output_config_preprocess(cfg);

	frame_queue_push(head, frame_node);

	if (s_info)
		dprec_done(&s_info->event_frame_cfg,
			   cfg->present_fence_idx, 0);

	return 0;

Error:
	frame_queue_node_destroy(frame_node, 0);
	return PTR_ERR(ret_val);
}

long __frame_config(unsigned long arg)
{
	struct disp_frame_cfg_t *cfg = kzalloc(sizeof(*cfg), GFP_KERNEL);
	int ret = 0;

	if (!cfg) {
		pr_info("error: kzalloc %zu memory fail!\n", sizeof(*cfg));
		return -ENOMEM;
	}

	if (copy_from_user(cfg, (void __user *)arg, sizeof(*cfg))) {
		ret = -EFAULT;
		goto error1;
	}

	cfg->setter = SESSION_USER_HWC;

	input_config_preprocess(cfg);
	if (cfg->output_en)
		output_config_preprocess(cfg);

	if (disp_validate_ioctl_params(cfg)) {
		ret = -EINVAL;
		goto error2;
	}

	switch (DISP_SESSION_TYPE(cfg->session_id)) {
	case DISP_SESSION_PRIMARY:
		primary_display_frame_cfg(cfg);
		break;
	case DISP_SESSION_EXTERNAL:
#if ((defined CONFIG_MTK_HDMI_SUPPORT) || \
	(defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)))
		external_display_frame_cfg(cfg);
#endif
		break;
	case DISP_SESSION_MEMORY:
		ovl2mem_frame_cfg(cfg);
		break;
	}

error2:
	disp_input_free_dirty_roi(cfg);
error1:
	kfree(cfg);
	return ret;
}

static long _ioctl_frame_config(unsigned long arg)
{
	if (disp_helper_get_option(DISP_OPT_FRAME_QUEUE))
		return __frame_queue_config(arg);

	return __frame_config(arg);
}

static int _ioctl_wait_all_jobs_done(unsigned long arg)
{
	unsigned int session_id = (unsigned int)arg;
	struct frame_queue_head_t *head;
	int ret = 0;

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
	int s_type = 0;

	session_id = info->session_id;
	s_type = DISP_SESSION_TYPE(session_id);

	if (s_type == DISP_SESSION_PRIMARY) {
		primary_display_get_info(info);
#if ((defined CONFIG_MTK_HDMI_SUPPORT) || \
	(defined(CONFIG_MTK_DUAL_DISPLAY_SUPPORT) && \
	(CONFIG_MTK_DUAL_DISPLAY_SUPPORT == 2)))
	} else if (s_type == DISP_SESSION_EXTERNAL) {
		external_display_get_info(info, session_id);
#endif
	} else if (s_type == DISP_SESSION_MEMORY) {
		ovl2mem_get_info(info);
	} else {
		DISP_PR_INFO("invalid session type:0x%08x\n", session_id);
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
		DISP_PR_ERR("[FB] copy_from_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	ret = disp_mgr_get_session_info(&info);

	if (copy_to_user(argp, &info, sizeof(info))) {
		DISP_PR_ERR("[FB] copy_to_user failed! line:%d\n", __LINE__);
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
	DISPDBG("%s: is_suspend=%d\n", __func__, is_suspend);
	if (copy_to_user(argp, &is_suspend, sizeof(int))) {
		DISP_PR_ERR("[FB] copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

	return ret;
}

int _ioctl_get_display_caps(unsigned long arg)
{
	int ret = 0;
	struct disp_caps_info caps_info;
	void __user *argp = (void __user *)arg;
	struct LCM_PARAMS *params = disp_lcm_get_params(primary_get_lcm());

	if (copy_from_user(&caps_info, argp, sizeof(caps_info))) {
		DISP_PR_ERR("[FB] copy_from_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

	HWC_gpid = task_tgid_nr(current);

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
	caps_info.lcm_degree = 180;
#endif

	if (disp_partial_is_support())
		caps_info.disp_feature |= DISP_FEATURE_PARTIAL;

	if (disp_helper_get_option(DISP_OPT_HRT))
		caps_info.disp_feature |= DISP_FEATURE_HRT;

	if (disp_helper_get_option(DISP_OPT_FRAME_QUEUE))
		caps_info.disp_feature |= DISP_FEATURE_FENCE_WAIT;

	caps_info.disp_feature |= DISP_FEATURE_DISP_SELF_REFRESH;

	caps_info.disp_feature |= DISP_FEATURE_FBDC;

	caps_info.lcm_color_mode = HAL_COLOR_MODE_NATIVE;
	if (disp_helper_get_option(DISP_OPT_OVL_WCG)) {
		if (params)
			caps_info.lcm_color_mode = params->lcm_color_mode;
		else
			DISP_PR_ERR("%s: failed to get lcm color mode\n",
				__func__);
	}

	if (params) {
		caps_info.min_luminance = params->min_luminance;
		caps_info.average_luminance = params->average_luminance;
		caps_info.max_luminance = params->max_luminance;
	} else {
		DISP_PR_ERR("%s: failed to get lcm luminance\n",
			__func__);
	}
	if (disp_helper_get_option(DISP_OPT_RSZ))
		caps_info.disp_feature |= DISP_FEATURE_RSZ;
	if (disp_helper_get_option(DISP_OPT_RPO))
		caps_info.disp_feature |= DISP_FEATURE_RPO;

	if (disp_helper_get_option(DISP_OPT_RSZ) ||
	    disp_helper_get_option(DISP_OPT_RPO)) {
		caps_info.rsz_in_max[0] = RSZ_TILE_LENGTH -
						RSZ_ALIGNMENT_MARGIN;
		caps_info.rsz_in_max[1] = RSZ_IN_MAX_HEIGHT;
	}

	if (primary_display_is_support_ARR()) {
		caps_info.disp_feature |= DISP_FEATURE_ARR;
		DISPMSG("%s,support ARR feature\n", __func__);
	}
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	/*DynFPS*/
	if (primary_display_is_support_DynFPS()) {
		caps_info.disp_feature |= DISP_FEATURE_DYNFPS;
		DISPMSG("%s,support DynFPS feature\n", __func__);
	}
#endif
	if (copy_to_user(argp, &caps_info, sizeof(caps_info))) {
		DISP_PR_ERR("[FB] copy_to_user failed! line:%d\n", __LINE__);
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
		DISP_PR_ERR("[FB] copy_from_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	s_info = disp_get_session_sync_info_for_debug(vsync_config.session_id);
	if (s_info)
		dprec_start(&s_info->event_waitvsync, 0, 0);

	if (DISP_SESSION_TYPE(vsync_config.session_id) == DISP_SESSION_EXTERNAL)
		ret = external_display_wait_for_vsync(&vsync_config,
						      vsync_config.session_id);
	else
		ret = primary_display_wait_for_vsync(&vsync_config);

	if (s_info)
		dprec_done(&s_info->event_waitvsync, 0, 0);

	if (copy_to_user(argp, &vsync_config, sizeof(vsync_config))) {
		DISP_PR_ERR("[FB] copy_to_user failed! line:%d\n", __LINE__);
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
	DISPMSG("%s: fps=%d\n", __func__, fps);
	if (copy_to_user(argp, &fps, sizeof(int))) {
		DISP_PR_ERR("[FB] copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

	return ret;
}

int _ioctl_set_vsync(unsigned long arg)
{
	int ret = 0;
	unsigned int fps = (unsigned int)arg;

	DISPMSG("%s: set fps to %d\n", __func__, fps);

	ret = primary_display_force_set_vsync_fps(fps, 0);

	if (ret != 0) {
		DISP_PR_INFO("%s: fail to set fps=%d\n",
			    __func__, fps);
		return -EFAULT;
	}
	return ret;
}

static long _ioctl_query_valid_layer(unsigned long arg)
{
	struct disp_layer_info disp_info_user;
	void __user *argp = (void __user *)arg;

	if (copy_from_user(&disp_info_user, argp, sizeof(disp_info_user))) {
		DISP_PR_ERR("[FB] copy_to_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	/* check data from userspace is legal */
	if (disp_info_user.layer_num[0] < 0 ||
		disp_info_user.layer_num[0] > 0x300 ||
		disp_info_user.layer_num[1] < 0 ||
		disp_info_user.layer_num[1] > 0x300) {
		DISP_PR_INFO(
			"%s error, layer_num[0]= %d, layer_num[1]= %d!\n",
			__func__, disp_info_user.layer_num[0],
			disp_info_user.layer_num[1]);
		return -EINVAL;
	}

	mutex_lock(&disp_layer_lock);
	layering_rule_start(&disp_info_user, 0);
	mutex_unlock(&disp_layer_lock);

	if (copy_to_user(argp, &disp_info_user, sizeof(disp_info_user))) {
		DISP_PR_ERR("[FB] copy_to_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	return 0;
}

int _ioctl_set_scenario(unsigned long arg)
{
	int ret = -1;
	struct disp_scenario_config_t scenario_cfg;
	void __user *argp = (void __user *)arg;

	if (copy_from_user(&scenario_cfg, argp, sizeof(scenario_cfg))) {
		DISP_PR_ERR("[FB] copy_to_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}

	if (DISP_SESSION_TYPE(scenario_cfg.session_id) == DISP_SESSION_PRIMARY)
		ret = primary_display_set_scenario(scenario_cfg.scenario);
	if (ret)
		DISP_PR_ERR("session(0x%08x) set scenario(%d) fail, ret=%d\n",
			    scenario_cfg.session_id, scenario_cfg.scenario,
			    ret);

	return ret;
}

int set_session_mode(struct disp_session_config *cfg, int force)
{
	int ret = 0;
	int s_type = DISP_SESSION_TYPE(cfg->session_id);

#if defined(MTK_FB_SHARE_WDMA0_SUPPORT)
	if (cfg->mode == DISP_SESSION_DIRECT_LINK_MIRROR_MODE ||
	    cfg->mode == DISP_SESSION_DECOUPLE_MIRROR_MODE) {
		/* EXT mode -> DC_M mode: disconnect EXT path first */
		external_display_switch_mode(cfg->mode, g_session,
					     cfg->session_id);

		if (has_memory_session)
			primary_display_switch_mode_blocked(cfg->mode,
						cfg->session_id, 0);
		else if (s_type == DISP_SESSION_PRIMARY)
			primary_display_switch_mode(cfg->mode,
						    cfg->session_id, 0);
		else
			DISP_PR_ERR("%s: session(0x%08x) set mode(%d) fail\n",
				    __func__, cfg->session_id, cfg->mode);
	} else {
		if (has_memory_session)
			primary_display_switch_mode_blocked(cfg->mode,
						cfg->session_id, 0);
		else if (s_type == DISP_SESSION_PRIMARY)
			primary_display_switch_mode(cfg->mode,
						    cfg->session_id, 0);
		else
			DISP_PR_ERR("%s: session(0x%08x) set mode(%d) fail\n",
				    __func__, cfg->session_id, cfg->mode);

		/*
		 * DC_M -> EXT mode, switch to Directlink first,
		 * then create EXT path
		 */
		external_display_switch_mode(cfg->mode, g_session,
					     cfg->session_id);
	}
#else /* !MTK_FB_SHARE_WDMA0_SUPPORT */
	if (s_type == DISP_SESSION_PRIMARY)
		primary_display_switch_mode(cfg->mode, cfg->session_id, 0);
	else
		DISP_PR_ERR("%s: session(0x%08x) set mode(%d) fail\n",
			    __func__, cfg->session_id, cfg->mode);

	external_display_switch_mode(cfg->mode, g_session, cfg->session_id);
#endif /* MTK_FB_SHARE_WDMA0_SUPPORT */
	return ret;
}

int _ioctl_set_session_mode(unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct disp_session_config config_info;

	if (copy_from_user(&config_info, argp, sizeof(config_info))) {
		DISP_PR_ERR("[FB] copy_from_user failed! line:%d\n", __LINE__);
		return -EFAULT;
	}
	return set_session_mode(&config_info, 0);
}

/*---------------- function for repaint start ------------------*/
void trigger_repaint(int type)
{
	if (type > WAIT_FOR_REFRESH && type < REFRESH_TYPE_NUM) {
		struct repaint_job_t *repaint_job;

		/* get a repaint_job_t from pool */
		mutex_lock(&repaint_queue_lock);
		if (!list_empty(&repaint_job_pool)) {
			repaint_job = list_first_entry(&repaint_job_pool,
				struct repaint_job_t, link);
			list_del_init(&repaint_job->link);
		} else { /* create repaint_job_t if pool is empty */
			repaint_job = kzalloc(sizeof(struct repaint_job_t),
				GFP_KERNEL);
			if (IS_ERR_OR_NULL(repaint_job)) {
				disp_aee_print("allocate repaint_job_t fail\n");
				return;
			}
			INIT_LIST_HEAD(&repaint_job->link);
			DISPMSG("[REPAINT] allocate a new repaint_job_t\n");
		}

		/* init & insert repaint_job_t into queue */
		repaint_job->type = type;
		list_add_tail(&repaint_job->link, &repaint_job_queue);
		mutex_unlock(&repaint_queue_lock);

		DISPMSG("[REPAINT] insert repaint_job in queue, type:%d\n",
				type);
		wake_up_interruptible(&repaint_wq);
	}
}

int _ioctl_wait_self_refresh_trigger(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	unsigned int type;
	struct repaint_job_t *repaint_job;

	/* reset status & wake-up threads which wait for repainting */
	DISPMSG("[REPAINT] HWC waits for repaint\n");

	/*  wait for repaint */
	ret = wait_event_interruptible(repaint_wq,
			!list_empty(&repaint_job_queue));
	if (ret < 0) {
		DISP_PR_ERR("[REPAINT] wait_event unexpectedly, ret:%d\n", ret);
		return ret;
	}

	/* retrieve first repaint_job_t from queue */
	mutex_lock(&repaint_queue_lock);
	repaint_job = list_first_entry(&repaint_job_queue,
		struct repaint_job_t, link);
	type = repaint_job->type;

	/* remove from queue & add repaint_job_t in pool */
	list_del_init(&repaint_job->link);
	list_add_tail(&repaint_job->link, &repaint_job_pool);
	mutex_unlock(&repaint_queue_lock);

	DISPMSG("[REPAINT] trigger repaint, type: %d\n", type);
	if (copy_to_user(argp, &type, sizeof(unsigned int))) {
		DISP_PR_ERR("[FB]: copy to user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}
	return ret;
}
/*---------------- function for repaint end ------------------*/

/*-----------------function for ARR notify fps changed start--------*/
int _ioctl_wait_fps_change(unsigned long arg)
{
	int ret = 0;
	unsigned int new_fps = 60;
	void __user *argp = (void __user *)arg;

	/* reset status & wake-up threads which wait for repainting */
	DISPMSG("[fps] waits for fps changed\n");

	ret = primary_display_wait_fps_change(&new_fps);

	if (copy_to_user(argp, &new_fps, sizeof(unsigned int))) {
		DISP_PR_INFO("[fps]: copy to user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}
	return ret;

}

/*ToDo: ARR, add ioctl to replay supported dynamic fps to HWC*/
/*think about the data structure to use*/
int _ioctl_get_supported_fps(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct dynamic_fps_levels dynamic_fps_info;

	if (copy_from_user(&dynamic_fps_info,
			argp, sizeof(dynamic_fps_info))) {
		DISP_PR_INFO("[fps] copy_from_user failed! line:%d\n",
			__LINE__);
		return -EFAULT;
	}

	ret = primary_display_get_dynamic_fps_info(&dynamic_fps_info);

	if (ret != 0) {
		DISP_PR_INFO("[fps] %s fail! line:%d\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	if (copy_to_user(argp, &dynamic_fps_info, sizeof(dynamic_fps_info))) {
		DISP_PR_INFO("[fps] copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

	return ret;
}

int _ioctl_wait_touch_hint(unsigned long arg)
{
	int ret = 0;
	unsigned int tphint = 60;
	void __user *argp = (void __user *)arg;

	/* reset status & wake-up threads which wait for repainting */
	ret = disp_tphint_wait_trigger(&tphint);

	if (copy_to_user(argp, &tphint, sizeof(unsigned int))) {
		DISP_PR_INFO("[fps]: copy to user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}
	return ret;

}

/*-----------------function for ARR notify fps changed end--------*/

#ifdef CONFIG_MTK_HIGH_FRAME_RATE
/*--------------------------DynFPS start-------------------*/
int _ioctl_get_multi_configs(unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct multi_configs multi_cfgs;

	if (copy_from_user(&multi_cfgs,
			argp, sizeof(multi_cfgs))) {
		DISP_PR_INFO("[dfps] copy_from_user failed! line:%d\n",
			__LINE__);
		return -EFAULT;
	}

	ret = primary_display_get_multi_configs(&multi_cfgs);

	if (ret != 0) {
		DISP_PR_INFO("[dfps] %s fail! line:%d\n", __func__, __LINE__);
		ret = -EFAULT;
		return ret;
	}
	if (copy_to_user(argp, &multi_cfgs, sizeof(multi_cfgs))) {
		DISP_PR_INFO("[dfps] copy_to_user failed! line:%d\n", __LINE__);
		ret = -EFAULT;
	}

	return ret;

}
/*--------------------------DynFPS end-------------------*/
#endif


const char *_session_ioctl_str(unsigned int cmd)
{
	switch (cmd) {
	case DISP_IOCTL_CREATE_SESSION:
		return "DISP_IOCTL_CREATE_SESSION";
	case DISP_IOCTL_DESTROY_SESSION:
		return "DISP_IOCTL_DESTROY_SESSION";
	case DISP_IOCTL_TRIGGER_SESSION:
		return "DISP_IOCTL_TRIGGER_SESSION";
	case DISP_IOCTL_SET_INPUT_BUFFER:
		return "DISP_IOCTL_SET_INPUT_BUFFER";
	case DISP_IOCTL_PREPARE_INPUT_BUFFER:
		return "DISP_IOCTL_PREPARE_INPUT_BUFFER";
	case DISP_IOCTL_WAIT_FOR_VSYNC:
		return "DISP_IOCL_WAIT_FOR_VSYNC";
	case DISP_IOCTL_GET_SESSION_INFO:
		return "DISP_IOCTL_GET_SESSION_INFO";
	case DISP_IOCTL_AAL_EVENTCTL:
		return "DISP_IOCTL_AAL_EVENTCTL";
	case DISP_IOCTL_AAL_GET_HIST:
		return "DISP_IOCTL_AAL_GET_HIST";
	case DISP_IOCTL_AAL_INIT_REG:
		return "DISP_IOCTL_AAL_INIT_REG";
	case DISP_IOCTL_AAL_SET_PARAM:
		return "DISP_IOCTL_AAL_SET_PARAM";
	case DISP_IOCTL_AAL_INIT_DRE30:
		return "DISP_IOCTL_AAL_INIT_DRE30";
	case DISP_IOCTL_AAL_GET_SIZE:
		return "DISP_IOCTL_AAL_GET_SIZE";
	case DISP_IOCTL_SET_GAMMALUT:
		return "DISP_IOCTL_SET_GAMMALUT";
	case DISP_IOCTL_SET_CCORR:
		return "DISP_IOCTL_SET_CCORR";
	case DISP_IOCTL_CCORR_EVENTCTL:
		return "DISP_IOCTL_CCORR_EVENTCTL";
	case DISP_IOCTL_CCORR_GET_IRQ:
		return "DISP_IOCTL_CCORR_GET_IRQ";
	case DISP_IOCTL_SUPPORT_COLOR_TRANSFORM:
		return "DISP_IOCTL_SUPPORT_COLOR_TRANSFORM";
	case DISP_IOCTL_SET_PQPARAM:
		return "DISP_IOCTL_SET_PQPARAM";
	case DISP_IOCTL_GET_PQPARAM:
		return "DISP_IOCTL_GET_PQPARAM";
	case DISP_IOCTL_GET_PQINDEX:
		return "DISP_IOCTL_GET_PQINDEX";
	case DISP_IOCTL_SET_PQINDEX:
		return "DISP_IOCTL_SET_PQINDEX";
	case DISP_IOCTL_SET_COLOR_REG:
		return "DISP_IOCTL_SET_COLOR_REG";
	case DISP_IOCTL_SET_TDSHPINDEX:
		return "DISP_IOCTL_SET_TDSHPINDEX";
	case DISP_IOCTL_GET_TDSHPINDEX:
		return "DISP_IOCTL_GET_TDSHPINDEX";
	case DISP_IOCTL_SET_PQ_CAM_PARAM:
		return "DISP_IOCTL_SET_PQ_CAM_PARAM";
	case DISP_IOCTL_GET_PQ_CAM_PARAM:
		return "DISP_IOCTL_GET_PQ_CAM_PARAM";
	case DISP_IOCTL_SET_PQ_GAL_PARAM:
		return "DISP_IOCTL_SET_PQ_GAL_PARAM";
	case DISP_IOCTL_GET_PQ_GAL_PARAM:
		return "DISP_IOCTL_GET_PQ_GAL_PARAM";
	case DISP_IOCTL_OD_CTL:
		return "DISP_IOCTL_OD_CTL";
	case DISP_IOCTL_GET_DISPLAY_CAPS:
		return "DISP_IOCTL_GET_DISPLAY_CAPS";
	case DISP_IOCTL_QUERY_VALID_LAYER:
		return "DISP_IOCTL_QUERY_VALID_LAYER";
	case DISP_IOCTL_FRAME_CONFIG:
		return "DISP_IOCTL_FRAME_CONFIG";
	case DISP_IOCTL_TOUCH_HINT:
		return "DISP_IOCTL_TOUCH_HINT";
	case DISP_IOCTL_GET_MULTI_CONFIGS:
		return "DISP_IOCTL_GET_MULTI_CONFIGS";
	default:
		break;
	}
	return "unknown-ioctl";
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
	case DISP_IOCTL_WAIT_DISP_SELF_REFRESH:
		return _ioctl_wait_self_refresh_trigger(arg);
	case DISP_IOCTL_WAIT_FPS_CHANGE:
		return _ioctl_wait_fps_change(arg);
	case DISP_IOCTL_TOUCH_HINT:
		return _ioctl_wait_touch_hint(arg);
	case DISP_IOCTL_GET_SUPPORTED_FPS:
		return _ioctl_get_supported_fps(arg);
#ifdef CONFIG_MTK_HIGH_FRAME_RATE
	case DISP_IOCTL_GET_MULTI_CONFIGS:
		return _ioctl_get_multi_configs(arg);
#endif
	case DISP_IOCTL_AAL_EVENTCTL:
	case DISP_IOCTL_AAL_GET_HIST:
	case DISP_IOCTL_AAL_INIT_REG:
	case DISP_IOCTL_AAL_SET_PARAM:
	case DISP_IOCTL_AAL_INIT_DRE30:
	case DISP_IOCTL_AAL_GET_SIZE:
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
		ret = primary_display_user_cmd(cmd, arg);
		break;
	default:
		DISP_PR_INFO("[session]ioctl not supported, 0x%08x\n", cmd);
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
const char *_session_compat_ioctl_str(unsigned int cmd)
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
	case DISP_IOCTL_AAL_INIT_DRE30:
	case DISP_IOCTL_AAL_GET_SIZE:
	{
		void __user *data32;

		data32 = compat_ptr(arg);
		ret = file->f_op->unlocked_ioctl(file, cmd,
						 (unsigned long)data32);
		return ret;
	}
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
		ret = primary_display_user_cmd(cmd, arg);
		break;
	default:
		DISP_PR_INFO("[%s]ioctl not supported, 0x%08x\n",
			     __func__, cmd);
		return -ENOIOCTLCMD;
	}

	return ret;
}
#endif

static const struct file_operations mtk_disp_mgr_fops = {
	.owner = THIS_MODULE,
	.mmap = mtk_disp_mgr_mmap,
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

	pr_debug("%s called\n", __func__);

	if (alloc_chrdev_region(&mtk_disp_mgr_devno, 0, 1, DISP_SESSION_DEVICE))
		return -EFAULT;

	mtk_disp_mgr_cdev = cdev_alloc();
	mtk_disp_mgr_cdev->owner = THIS_MODULE;
	mtk_disp_mgr_cdev->ops = &mtk_disp_mgr_fops;

	ret = cdev_add(mtk_disp_mgr_cdev, mtk_disp_mgr_devno, 1);
	if (ret) {
		DISP_PR_ERR("cdev_add failed!\n");
		unregister_chrdev_region(mtk_disp_mgr_devno, 1);
		return ret;
	}

	mtk_disp_mgr_class = class_create(THIS_MODULE, DISP_SESSION_DEVICE);
	class_dev = (struct class_device *)device_create(mtk_disp_mgr_class,
						NULL, mtk_disp_mgr_devno,
						NULL, DISP_SESSION_DEVICE);
	disp_sync_init();

	external_display_control_init();

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
	pr_debug("%s\n", __func__);
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
