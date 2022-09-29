// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_gem.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_drv.h>
#include <drm/drm_vblank.h>
#include <linux/delay.h>
#include <linux/component.h>
#include <linux/iommu.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/kmemleak.h>
#include <uapi/linux/sched/types.h>
#include <drm/drm_auth.h>
#define NONE DEL_NONE
#include <linux/scmi_protocol.h>
#undef NONE

#include "tinysys-scmi.h"
#include "drm_internal.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_debugfs.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_session.h"
#include "mtk_fence.h"
#include "mtk_debug.h"
#include "mtk_layering_rule.h"
#include "mtk_rect.h"
#include "mtk_drm_helper.h"
#include "mtk_drm_assert.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_panel_ext.h"
#include <linux/clk.h>
#include "mtk_disp_pmqos.h"
#include "mtk_disp_recovery.h"
#include "mtk_drm_arr.h"
#include "mtk_disp_ccorr.h"
#include "mtk_disp_tdshp.h"
#include "mtk_disp_color.h"
#include "mtk_disp_gamma.h"
#include "mtk_disp_aal.h"
#include "mtk_disp_c3d.h"
#include "mtk_disp_chist.h"
#include "mtk_lease.h"
#include "mtk_disp_oddmr/mtk_disp_oddmr.h"

#include "mtk_drm_mmp.h"
/* *******Panel Master******** */
#include "mtk_fbconfig_kdebug.h"
#include "mtk_dp_api.h"
//#include "swpm_me.h"
//#include "include/pmic_api_buck.h"
#include <../drivers/gpu/drm/mediatek/mml/mtk-mml.h>

#include "../mml/mtk-mml.h"
#include "../mml/mtk-mml-drm-adaptor.h"
#include "../mml/mtk-mml-driver.h"

#include "slbc_ops.h"
#include <linux/syscalls.h>
#define CLKBUF_COMMON_H
#include <mtk_clkbuf_ctl.h>

#define DRIVER_NAME "mediatek"
#define DRIVER_DESC "Mediatek SoC DRM"
#define DRIVER_DATE "20150513"
#define DRIVER_MAJOR 1
#define DRIVER_MINOR 0

void disp_dbg_deinit(void);
void disp_dbg_probe(void);
void disp_dbg_init(struct drm_device *dev);

static atomic_t top_isr_ref; /* irq power status protection */
static atomic_t top_clk_ref; /* top clk status protection*/
static spinlock_t top_clk_lock; /* power status protection*/

unsigned long long mutex_time_start;
unsigned long long mutex_time_end;
long long mutex_time_period;
const char *mutex_locker;

unsigned long long mutex_nested_time_start;
unsigned long long mutex_nested_time_end;
long long mutex_nested_time_period;
const char *mutex_nested_locker;
static int aod_scp_flag;
static unsigned int g_disp_plat_dbg_addr;
static unsigned int g_disp_plat_dbg_size;
static void __iomem *g_disp_plat_dbg_buf_addr;
static struct scmi_tinysys_info_st *tinfo;
static int feature_id;

struct lcm_fps_ctx_t lcm_fps_ctx[MAX_CRTC];

static int manual_shift;
static bool no_shift;

#ifdef DRM_OVL_SELF_PATTERN
struct drm_crtc *test_crtc;
void *test_va;
dma_addr_t test_pa;
#endif

struct aod_scp_send_ipi_msg {
	int (*send_ipi)(int value);
};

static struct aod_scp_send_ipi_msg aod_scp_ipi;

void **mtk_aod_scp_ipi_init(void)
{
	return (void **)&aod_scp_ipi.send_ipi;
}
EXPORT_SYMBOL(mtk_aod_scp_ipi_init);

static ssize_t read_disp_plat_dbg_buf(struct file *filp, struct kobject *kobj,
	struct bin_attribute *bin_attr, char *buff, loff_t pos, size_t count)
{
	ssize_t bytes = 0, ret;

	if (!(g_disp_plat_dbg_addr))
		return 0;

	if (!(g_disp_plat_dbg_size))
		return 0;

	ret = memory_read_from_buffer(buff, count, &pos,
		g_disp_plat_dbg_buf_addr, g_disp_plat_dbg_size);

	if (ret < 0)
		return ret;
	else
		return bytes + ret;
}

static struct bin_attribute disp_plat_dbg_buf_attr = {
	.attr = {.name = "disp_plat_dbg_buf", .mode = 0444},
	.read = read_disp_plat_dbg_buf,
};

void disp_plat_dbg_init(void)
{

	int err;

	DDPMSG("addr=0x%x, size=0x%x, %s\n", g_disp_plat_dbg_addr, g_disp_plat_dbg_size, __func__);

	if ((g_disp_plat_dbg_addr > 0) && (g_disp_plat_dbg_size > 0)) {
		if (!tinfo) {
			tinfo = get_scmi_tinysys_info();

			if ((IS_ERR_OR_NULL(tinfo)) || (IS_ERR_OR_NULL(tinfo->ph))) {
				DDPMSG("%s: tinfo or tinfo->ph is wrong!!\n", __func__);
				tinfo = NULL;
				} else {
					of_property_read_u32(tinfo->sdev->dev.of_node,
						"scmi_dispplatdbg", &feature_id);
					DDPMSG("%s: get scmi_smi succeed id=%d!!\n",
						__func__, feature_id);

					err = scmi_tinysys_common_set(tinfo->ph, feature_id,
					g_disp_plat_dbg_addr, g_disp_plat_dbg_size, 0, 0, 0);
					if (err)
						DDPMSG("%s: call scmi_tinysys_common_set err=%d\n",
						__func__, err);
				}
			}
		}
	DDPMSG("%s++\n", __func__);
}

struct mtk_drm_disp_sec_cb disp_sec_cb;
EXPORT_SYMBOL(disp_sec_cb);

void **mtk_drm_disp_sec_cb_init(void)
{
	DDPMSG("%s+\n", __func__);
	disp_plat_dbg_init();
	return (void **)&disp_sec_cb.cb;
}
EXPORT_SYMBOL(mtk_drm_disp_sec_cb_init);

int mtk_atoi(const char *str)
{
	int len = strlen(str);
	int num = 0, i = 0;
	int sign = (str[0] == '-') ? -1 : 1;

	if (sign == -1)
		i = 1;
	else
		i = 0;
	for (; i < len; i++) {
		if (str[i] < '0' || str[i] > '9')
			break;
		num *= 10;
		num += (int)(str[i] - '0');
	}

	pr_notice("[debug] num=%d sign=%d\n",
			num, sign);

	return num * sign;
}

void disp_drm_debug(const char *opt)
{
	pr_notice("[debug] opt=%s\n", opt);
	if (strncmp(opt, "shift:", 6) == 0) {

		int len = strlen(opt);
		char buf[100];

		strcpy(buf, opt + 6);
		buf[len - 6] = '\0';

		pr_notice("[debug] buf=%s\n",
			buf);

		manual_shift = mtk_atoi(buf);

		pr_notice("[debug] manual_shift=%d\n",
			manual_shift);
	} else if (strncmp(opt, "no_shift:", 9) == 0) {
		no_shift = strncmp(opt + 9, "1", 1) == 0;
		pr_notice("[debug] no_shift=%d\n",
			no_shift);
	}
}

static inline struct mtk_atomic_state *to_mtk_state(struct drm_atomic_state *s)
{
	return container_of(s, struct mtk_atomic_state, base);
}

static void mtk_atomic_state_free(struct kref *k)
{
	struct mtk_atomic_state *mtk_state =
		container_of(k, struct mtk_atomic_state, kref);
	struct drm_atomic_state *state = &mtk_state->base;

	drm_atomic_state_clear(state);
	drm_atomic_state_default_release(state);
	kfree(mtk_state);
}

static void mtk_atomic_state_put(struct drm_atomic_state *state)
{
	struct mtk_atomic_state *mtk_state = to_mtk_state(state);

	kref_put(&mtk_state->kref, mtk_atomic_state_free);
}

void mtk_atomic_state_put_queue(struct drm_atomic_state *state)
{
	struct drm_device *drm = state->dev;
	struct mtk_drm_private *mtk_drm = drm->dev_private;
	struct mtk_atomic_state *mtk_state = to_mtk_state(state);
	unsigned long flags;

	spin_lock_irqsave(&mtk_drm->unreference.lock, flags);
	list_add_tail(&mtk_state->list, &mtk_drm->unreference.list);
	spin_unlock_irqrestore(&mtk_drm->unreference.lock, flags);

	schedule_work(&mtk_drm->unreference.work);
}

static uint32_t mtk_atomic_crtc_mask(struct drm_device *drm,
				     struct drm_atomic_state *state)
{
	uint32_t crtc_mask = 0;
	int i;

	for (i = 0, crtc_mask = 0; i < drm->mode_config.num_crtc; i++) {
		struct __drm_crtcs_state *crtc = &(state->crtcs[i]);

		if (!crtc->ptr)
			continue;

		crtc_mask |= (1 << drm_crtc_index(crtc->ptr));
	}

	return crtc_mask;
}

static void mtk_unreference_work(struct work_struct *work)
{
	struct mtk_drm_private *mtk_drm =
		container_of(work, struct mtk_drm_private, unreference.work);
	unsigned long flags;
	struct mtk_atomic_state *state, *tmp;

	/*
	 * framebuffers cannot be unreferenced in atomic context.
	 * Therefore, only hold the spinlock when iterating unreference_list,
	 * and drop it when doing the unreference.
	 */
	spin_lock_irqsave(&mtk_drm->unreference.lock, flags);
	list_for_each_entry_safe(state, tmp, &mtk_drm->unreference.list, list) {
		list_del(&state->list);
		spin_unlock_irqrestore(&mtk_drm->unreference.lock, flags);
		drm_atomic_state_put(&state->base);
		spin_lock_irqsave(&mtk_drm->unreference.lock, flags);
	}
	spin_unlock_irqrestore(&mtk_drm->unreference.lock, flags);
}

static void mtk_atomic_schedule(struct mtk_drm_private *private,
				struct drm_atomic_state *state)
{
	private->commit.state = state;
	schedule_work(&private->commit.work);
}

static void mtk_atomic_wait_for_fences(struct drm_atomic_state *state)
{
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	int i;

	for_each_old_plane_in_state(state, plane, plane_state, i)
		mtk_fb_wait(plane->state->fb);
}

#define UNIT 32768
static void mtk_atomic_rsz_calc_dual_params(
	struct drm_crtc *crtc, struct mtk_rect *src_roi,
	struct mtk_rect *dst_roi,
	struct mtk_rsz_param param[])
{
	int left = dst_roi->x;
	int right = dst_roi->x + dst_roi->width - 1;
	int tile_idx = 0;
	int tile_loss = 4;
	u32 step = 0;
	s32 init_phase = 0;
	s32 offset[2] = {0};
	s32 int_offset[2] = {0};
	s32 sub_offset[2] = {0};
	u32 tile_in_len[2] = {0};
	u32 tile_out_len[2] = {0};
	u32 out_x[2] = {0};
	bool is_dual = false;
	int width = crtc->state->adjusted_mode.hdisplay;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp && drm_crtc_index(crtc) == 0)
		width = mtk_ddp_comp_io_cmd(
				output_comp, NULL,
				DSI_GET_VIRTUAL_WIDTH, NULL);

	if (right < width / 2)
		tile_idx = 0;
	else if (left >= width / 2)
		tile_idx = 1;
	else
		is_dual = true;


	DDPINFO("%s :loss:%d,idx:%d,width:%d,src_w:%d,dst_w:%d,src_x:%d\n", __func__,
	       tile_loss, tile_idx, width, src_roi->width, dst_roi->width, src_roi->x);
	step = (UNIT * (src_roi->width - 1) + (dst_roi->width - 2)) /
			(dst_roi->width - 1); /* for ceil */

	offset[0] = (step * (dst_roi->width - 1) -
		UNIT * (src_roi->width - 1)) / 2;
	init_phase = UNIT - offset[0];
	sub_offset[0] = -offset[0];

	if (sub_offset[0] < 0) {
		int_offset[0]--;
		sub_offset[0] = UNIT + sub_offset[0];
	}
	if (sub_offset[0] >= UNIT) {
		int_offset[0]++;
		sub_offset[0] = sub_offset[0] - UNIT;
	}

	if (is_dual) {
		/*left side*/
		/*right bound - left bound + tile loss*/
		tile_in_len[0] = (((width / 2) * src_roi->width * 10) /
			dst_roi->width + 5) / 10 - src_roi->x + tile_loss;
		tile_out_len[0] = width / 2 - dst_roi->x;
		out_x[0] = dst_roi->x;
	} else {
		tile_in_len[0] = src_roi->width;
		tile_out_len[0] = dst_roi->width;
		if (tile_idx == 0)
			out_x[0] = dst_roi->x;
		else
			out_x[0] = dst_roi->x - width / 2;
	}

	param[tile_idx].out_x = out_x[0];
	param[tile_idx].step = step;
	param[tile_idx].int_offset = (u32)(int_offset[0] & 0xffff);
	param[tile_idx].sub_offset = (u32)(sub_offset[0] & 0x1fffff);
	param[tile_idx].in_len = tile_in_len[0];
	param[tile_idx].out_len = tile_out_len[0];
	DDPINFO("%s,%d:%s:step:%u,offset:%u.%u,len:%u->%u,out_x:%u\n", __func__, __LINE__,
	       is_dual ? "dual" : "single",
	       param[0].step,
	       param[0].int_offset,
	       param[0].sub_offset,
	       param[0].in_len,
	       param[0].out_len,
	       param[0].out_x);

	if (!is_dual)
		return;

	/* right half */
	tile_out_len[1] = dst_roi->width - tile_out_len[0];
	tile_in_len[1] = ((tile_out_len[1] * src_roi->width * 10) /
		dst_roi->width + 5) / 10 + tile_loss + (offset[0] ? 1 : 0);

	offset[1] = (-offset[0]) + (tile_out_len[0] * step) -
			(src_roi->width - tile_in_len[1]) * UNIT + manual_shift;
	/*
	 * offset[1] = (init_phase + dst_roi->width / 2 * step) -
	 *	(src_roi->width / 2 - tile_loss - (offset[0] ? 1 : 0) + 1) * UNIT +
	 *	UNIT + manual_shift;
	 */
	if (no_shift)
		offset[1] = 0;
	DDPINFO("%s,in_ph:%d,man_sh:%d,off[1]:%d\n", __func__, init_phase, manual_shift, offset[1]);
	int_offset[1] = offset[1] / UNIT;
	sub_offset[1] = offset[1] - UNIT * int_offset[1];
	/*
	*	if (int_offset[1] & 0x1) {
	*	int_offset[1]++;
	*	tile_in_len[1]++;
	*	DDPINFO("%s :right tile int_offset: make odd to even\n", __func__);
	*	}
	*/
	param[1].step = step;
	param[1].out_x = 0;
	param[1].int_offset = (u32)(int_offset[1] & 0xffff);
	param[1].sub_offset = (u32)(sub_offset[1] & 0x1fffff);
	param[1].in_len = tile_in_len[1];
	param[1].out_len = tile_out_len[1];

	DDPINFO("%s,%d :%s:step:%u,offset:%u.%u,len:%u->%u,out_x:%u\n", __func__, __LINE__,
	       is_dual ? "dual" : "single",
	       param[1].step,
	       param[1].int_offset,
	       param[1].sub_offset,
	       param[1].in_len,
	       param[1].out_len,
	       param[1].out_x);
}

static void mtk_atomic_disp_rsz_roi(struct drm_device *dev,
				    struct drm_atomic_state *old_state)
{
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state;
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *old_crtc_state;
	int i, j;
	struct mtk_rect dst_layer_roi = {0};
	struct mtk_rect dst_total_roi[MAX_CRTC] = {0};
	struct mtk_rect src_layer_roi = {0};
	struct mtk_rect src_total_roi[MAX_CRTC] = {0};
	bool rsz_enable[MAX_CRTC] = {false};
	struct mtk_plane_comp_state comp_state[MAX_CRTC][OVL_LAYER_NR];

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, j) {
		struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

		for (i = 0 ; i < mtk_crtc->layer_nr; i++) {
			struct drm_plane *plane = &mtk_crtc->planes[i].base;

			mtk_plane_get_comp_state(
				plane, &comp_state[drm_crtc_index(crtc)][i],
				crtc, 1);
		}
	}

	for_each_old_plane_in_state(old_state, plane, old_plane_state, i) {
		struct drm_plane_state *plane_state = plane->state;
		int src_w = drm_rect_width(&plane_state->src) >> 16;
		int src_h = drm_rect_height(&plane_state->src) >> 16;
		int dst_x = plane_state->dst.x1;
		int dst_y = plane_state->dst.y1;
		int dst_w = drm_rect_width(&plane_state->dst);
		int dst_h = drm_rect_height(&plane_state->dst);
		int tmp_w = 0, tmp_h = 0;
		int idx;

		if (!plane_state->crtc)
			continue;

		if (i < OVL_LAYER_NR)
			idx = i;
		else if (i < (OVL_LAYER_NR + EXTERNAL_INPUT_LAYER_NR))
			idx = i - OVL_LAYER_NR;
		else if (i < (OVL_LAYER_NR + EXTERNAL_INPUT_LAYER_NR + SP_INPUT_LAYER_NR))
			idx = i - (OVL_LAYER_NR + EXTERNAL_INPUT_LAYER_NR);
		else
			idx = i - (OVL_LAYER_NR + EXTERNAL_INPUT_LAYER_NR + SP_INPUT_LAYER_NR);

		if (crtc && comp_state[drm_crtc_index(crtc)][idx].layer_caps
			& MTK_DISP_RSZ_LAYER) {
			if (dst_w != 0)
				tmp_w = ((dst_x * src_w * 10) / dst_w + 5) / 10;
			else
				tmp_w = 0;
			if (dst_h != 0)
				tmp_h = ((dst_y * src_h * 10) / dst_h + 5) / 10;
			else
				tmp_h = 0;
			mtk_rect_make(&src_layer_roi,
				tmp_w, tmp_h,
				src_w, src_h);
			mtk_rect_make(&dst_layer_roi,
				dst_x, dst_y,
				dst_w, dst_h);

			/* not enable resizer for the case that plane ROI is different from HRT
			 * (e.g. layer composed by GPU
			 */
			if (src_layer_roi.width == dst_layer_roi.width &&
				src_layer_roi.height == dst_layer_roi.height)
				continue;
			mtk_rect_join(&src_layer_roi,
				      &src_total_roi[drm_crtc_index(
					      plane_state->crtc)],
				      &src_total_roi[drm_crtc_index(
					      plane_state->crtc)]);
			mtk_rect_join(&dst_layer_roi,
				      &dst_total_roi[drm_crtc_index(
					      plane_state->crtc)],
				      &dst_total_roi[drm_crtc_index(
					      plane_state->crtc)]);
			rsz_enable[drm_crtc_index(plane_state->crtc)] = true;
		}
	}

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
		struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

		if (!rsz_enable[i] || mtk_crtc->fake_layer.fake_layer_mask) {
			src_total_roi[i].x = 0;
			src_total_roi[i].y = 0;
			src_total_roi[i].width =
				crtc->state->adjusted_mode.hdisplay;
			src_total_roi[i].height =
					crtc->state->adjusted_mode.vdisplay;
			dst_total_roi[i].x = 0;
			dst_total_roi[i].y = 0;
			dst_total_roi[i].width =
				crtc->state->adjusted_mode.hdisplay;
			dst_total_roi[i].height =
				crtc->state->adjusted_mode.vdisplay;
		}
		state->rsz_src_roi = src_total_roi[i];
		state->rsz_dst_roi = dst_total_roi[i];
		if (mtk_crtc->is_dual_pipe && rsz_enable[i])
			mtk_atomic_rsz_calc_dual_params(crtc,
				&state->rsz_src_roi,
				&state->rsz_dst_roi,
				state->rsz_param);
		DDPINFO("[RPO] crtc[%d] (%d,%d,%d,%d)->(%d,%d,%d,%d)\n",
			drm_crtc_index(crtc), src_total_roi[i].x,
			src_total_roi[i].y, src_total_roi[i].width,
			src_total_roi[i].height, dst_total_roi[i].x,
			dst_total_roi[i].y, dst_total_roi[i].width,
			dst_total_roi[i].height);
	}
}

static void
mtk_atomic_calculate_plane_enabled_number(struct drm_device *dev,
					  struct drm_atomic_state *old_state)
{
	int i;
	struct drm_plane *plane;
	struct drm_plane_state *old_plane_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	unsigned int cnt[MAX_CRTC] = {0};

	for_each_old_plane_in_state(old_state, plane, old_plane_state, i) {
		if (plane->state->crtc) {
			struct mtk_drm_crtc *mtk_crtc =
					to_mtk_crtc(plane->state->crtc);
			cnt[drm_crtc_index(&mtk_crtc->base)]++;
		}
	}

	for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);

		atomic_set(&state->plane_enabled_num, cnt[i]);
	}
}

static void mtk_atomic_check_plane_sec_state(struct drm_device *dev,
				     struct drm_atomic_state *new_state)
{
	int i;
	int sec_on[MAX_CRTC] = {0};
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;

	for_each_old_plane_in_state(new_state, plane, new_plane_state, i) {
		if (plane->state->crtc) {
			struct mtk_drm_crtc *mtk_crtc =
					to_mtk_crtc(plane->state->crtc);

			if (plane->state->fb
				&& plane->state->fb->format->format
					!= DRM_FORMAT_C8
				&& mtk_drm_fb_is_secure(plane->state->fb))
				sec_on[drm_crtc_index(&mtk_crtc->base)] = true;
		}
	}

	for_each_old_crtc_in_state(new_state, crtc, new_crtc_state, i) {
		struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

		/* check output buffer is secure or not */
		if (mtk_crtc_check_out_sec(crtc))
			sec_on[drm_crtc_index(crtc)] = true;

		/* Leave secure sequence */
		if (mtk_crtc->sec_on && !sec_on[i])
			mtk_crtc_disable_secure_state(&mtk_crtc->base);

		/* When the engine switch to secure, we don't let system
		 * enter LP idle mode. Because it may make more secure risks
		 * with tiny power benefit.
		 */
		if (!mtk_crtc->sec_on && sec_on[i])
			mtk_drm_idlemgr_kick(__func__, crtc, false);

		mtk_crtc->sec_on = sec_on[i];
	}
}

static bool mtk_atomic_need_force_doze_switch(struct drm_crtc *crtc)
{
	struct mtk_crtc_state *mtk_state;

	mtk_state = to_mtk_crtc_state(crtc->state);
	if (!mtk_state->doze_changed ||
	    drm_atomic_crtc_needs_modeset(crtc->state))
		return false;

	DDPINFO("%s crtc%d, active:%d, doze_active:%d\n", __func__,
		drm_crtc_index(crtc), crtc->state->active,
		mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]);
	return true;
}

static void mtk_atomic_force_doze_switch(struct drm_device *dev,
					 struct drm_atomic_state *old_state,
					 struct drm_connector *connector,
					 struct drm_crtc *crtc)
{
	const struct drm_encoder_helper_funcs *funcs;
	struct drm_encoder *encoder;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_funcs *panel_funcs;
	struct cmdq_pkt *handle;
#ifndef DRM_CMDQ_DISABLE
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];
#endif

	/*
	 * If CRTC doze_active state change but the active state
	 * keep with the same value, the enable/disable cb of the
	 * encorder would not be executed. So we have to force
	 * run those cbs to change the doze directly.
	 */
	if (!mtk_atomic_need_force_doze_switch(crtc))
		return;

	DDPINFO("%s\n", __func__);

	encoder = connector->state->best_encoder;
	funcs = encoder->helper_private;

	panel_funcs = mtk_drm_get_lcm_ext_funcs(crtc);
	mtk_drm_idlemgr_kick(__func__, crtc, false);
	if (panel_funcs && panel_funcs->doze_get_mode_flags) {
		/* blocking flush before stop trigger loop */
		mtk_crtc_pkt_create(&handle, &mtk_crtc->base,
			mtk_crtc->gce_obj.client[CLIENT_CFG]);
		if (mtk_crtc_is_frame_trigger_mode(crtc))
			cmdq_pkt_wait_no_clear(handle,
				mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
		else
			cmdq_pkt_wait_no_clear(handle,
				mtk_crtc->gce_obj.event[EVENT_CMD_EOF]);
		cmdq_pkt_flush(handle);
		cmdq_pkt_destroy(handle);

#ifndef DRM_CMDQ_DISABLE
		cmdq_mbox_enable(client->chan); /* GCE clk refcnt + 1 */
		mtk_crtc_stop_trig_loop(crtc);
#endif

		if (mtk_crtc_with_sodi_loop(crtc) &&
			(!mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_stop_sodi_loop(crtc);

		if (mtk_crtc_with_event_loop(crtc) &&
			(mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_stop_event_loop(crtc);

		if (mtk_crtc_is_frame_trigger_mode(crtc)) {
			mtk_disp_mutex_disable(mtk_crtc->mutex[0]);
			mtk_disp_mutex_src_set(mtk_crtc, false);
			mtk_disp_mutex_enable(mtk_crtc->mutex[0]);
		}
	}
	/*
	 * No matter what the target crct power state it is,
	 * the encorder should be enabled for register controlling
	 * purpose.
	 */
	if (!funcs)
		return;
	if (funcs->enable)
		funcs->enable(encoder);
	else if (funcs->commit)
		funcs->commit(encoder);

	/*
	 * If the target crtc power state is POWER_OFF or
	 * DOZE_SUSPEND, call the encorder disable cb here.
	 */
	if (!crtc->state->active) {
		if (connector->state->crtc && funcs->prepare)
			funcs->prepare(encoder);
		else if (funcs->disable)
			funcs->disable(encoder);
		else if (funcs->dpms)
			funcs->dpms(encoder, DRM_MODE_DPMS_OFF);
	}

	if (panel_funcs && panel_funcs->doze_get_mode_flags) {
		if (mtk_crtc_is_frame_trigger_mode(crtc)) {
			mtk_disp_mutex_disable(mtk_crtc->mutex[0]);
			mtk_disp_mutex_src_set(mtk_crtc, true);
		}

		if (mtk_crtc_with_sodi_loop(crtc) &&
			(!mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_sodi_loop(crtc);

		if (mtk_crtc_with_event_loop(crtc) &&
			(mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_event_loop(crtc);

#ifndef DRM_CMDQ_DISABLE
		mtk_crtc_start_trig_loop(crtc);
		cmdq_mbox_disable(client->chan); /* GCE clk refcnt - 1 */
#endif

		mtk_crtc_hw_block_ready(crtc);
	}
}

static void mtk_atomic_aod_scp_ipi(struct drm_crtc *crtc, bool prepare)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_crtc_state *mtk_state;
	bool need_modeset;

	if (!aod_scp_flag || !aod_scp_ipi.send_ipi)
		return;

	mtk_state = to_mtk_crtc_state(crtc->state);
	if (!mtk_state->doze_changed || !mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE])
		return;

	if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT)) {
		//bw for dual pipe 2 layers.
		mtk_disp_set_hrt_bw(mtk_crtc, 3360);
	}

	//TODO: After the power control implementation in SCP is completed,
	//it only needs to judge prepare==1 to send IPI to SCP
	need_modeset = drm_atomic_crtc_needs_modeset(crtc->state);
	if (!need_modeset || (need_modeset && !crtc->state->active)) {
		//needs_modeset:0
		//needs_modeset:1 on->off
		if (prepare) {
			aod_scp_ipi.send_ipi(0);
			DDPMSG("mtk_aod_scp_ipi_send sent IPI to SCP in prepare done\n");
			mdelay(10000);
		}
	} else if (need_modeset && crtc->state->active) {
		//needs_modeset:1 off->on
		if (!prepare) {
			aod_scp_ipi.send_ipi(0);
			DDPMSG("mtk_aod_scp_ipi_send sent IPI to SCP in !prepare done\n");
			mdelay(10000);
		}
	}
}

static void mtk_atomic_doze_update_dsi_state(struct drm_device *dev,
					 struct drm_crtc *crtc, bool prepare)
{
	struct mtk_crtc_state *mtk_state;
	struct mtk_drm_private *priv = dev->dev_private;

	mtk_state = to_mtk_crtc_state(crtc->state);
	DDPINFO("%s doze_changed:%d, needs_modeset:%d, doze_active:%d\n",
		__func__, mtk_state->doze_changed,
		drm_atomic_crtc_needs_modeset(crtc->state),
		mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]);

	mtk_atomic_aod_scp_ipi(crtc, prepare);

	if (mtk_state->doze_changed && priv->data->doze_ctrl_pmic) {
		if (mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE] && prepare) {
			DDPMSG("enter AOD, disable PMIC LPMODE\n");
			//pmic_ldo_vio18_lp(SRCLKEN0, 0, 1, HW_LP);
			//pmic_ldo_vio18_lp(SRCLKEN2, 0, 1, HW_LP);
			/* DWFS, drivers/misc/mediatek/clkbuf/v1/src/mtk_clkbuf_ctl.c */
			//clk_buf_voter_ctrl_by_id(12, SW_BBLPM);
		} else if (!mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]
				&& !prepare) {
			DDPMSG("exit AOD, enable PMIC LPMODE\n");
			//pmic_ldo_vio18_lp(SRCLKEN0, 1, 1, HW_LP);
			//pmic_ldo_vio18_lp(SRCLKEN2, 1, 1, HW_LP);
			/* DWFS, drivers/misc/mediatek/clkbuf/v1/src/mtk_clkbuf_ctl.c */
			//clk_buf_voter_ctrl_by_id(12, SW_OFF);
		}
	}
	if (!mtk_state->doze_changed ||
		!drm_atomic_crtc_needs_modeset(crtc->state))
		return;

	/* consider suspend->doze, doze_suspend->resume when doze prepare
	 * consider doze->suspend, resume->doze_supend when doze finish
	 */
	if ((crtc->state->active && prepare) ||
		(!crtc->state->active && !prepare))
		mtk_crtc_change_output_mode(crtc,
			mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]);
}
	/* TODO: need PQ team's help to fix the following flow that will lead to doze issue.*/
#ifdef IF_ZERO
static void pq_bypass_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);
}
#endif
static void mtk_atomit_doze_update_pq(struct drm_crtc *crtc, unsigned int stage, bool old_state)
{
	/* TODO: need PQ team's help to fix the following flow that will lead to doze issue.*/
#ifdef IF_ZERO
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *mtk_state;
	struct mtk_ddp_comp *comp;
	struct cmdq_pkt *cmdq_handle;
	struct mtk_cmdq_cb_data *cb_data;
	int i, j;
	unsigned int bypass = 0;
#ifndef DRM_CMDQ_DISABLE
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];
#endif
#endif
	/* skip this stage avoid cmdq_mbox control abnormal */
	return;

	/* TODO: need PQ team's help to fix the following flow that will lead to doze issue.*/
#ifdef IF_ZERO
	DDPINFO("%s+: new crtc state = %d, old crtc state = %d, stage = %d\n", __func__,
		crtc->state->active, old_state, stage);
	mtk_state = to_mtk_crtc_state(crtc->state);

	if (!crtc->state->active) {
		if (mtk_state->doze_changed &&
			!mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
			DDPINFO("%s: doze switch to suspend, need enable pq first\n", __func__);
		} else {
			DDPINFO("%s: crtc is not active\n", __func__);
			return;
		}
	}

	if (mtk_state->doze_changed) {
		if (!mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
			if (!crtc->state->active && stage == 1)
				return;
			else if (crtc->state->active && stage == 0)
				return;
			bypass = 0;
		} else {
			if (!old_state && stage == 0)
				return;
			else if (old_state && stage == 1)
				return;
			bypass = 1;
		}
	} else {
		DDPINFO("%s: doze not change, skip update pq\n", __func__);
		return;
	}

	cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
	if (!cb_data) {
		DDPPR_ERR("cb data creation failed\n");
		return;
	}

#ifndef DRM_CMDQ_DISABLE
	if (bypass)
		cmdq_mbox_enable(client->chan); /* GCE clk refcnt + 1 */
#endif

	mtk_crtc_pkt_create(&cmdq_handle, &mtk_crtc->base,
		mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);
	cb_data->crtc = crtc;
	cb_data->cmdq_handle = cmdq_handle;

	if (mtk_crtc_is_frame_trigger_mode(crtc))
		cmdq_pkt_wait_no_clear(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
	else
		cmdq_pkt_wait_no_clear(cmdq_handle,
			mtk_crtc->gce_obj.event[EVENT_CMD_EOF]);

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (comp && (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_AAL ||
				mtk_ddp_comp_get_type(comp->id) == MTK_DISP_CCORR)) {
			if (comp->funcs && comp->funcs->bypass)
				mtk_ddp_comp_bypass(comp, bypass, cmdq_handle);
		}
	}

	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
			if (comp && (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_AAL ||
				mtk_ddp_comp_get_type(comp->id) == MTK_DISP_CCORR)) {
				if (comp->funcs && comp->funcs->bypass)
					mtk_ddp_comp_bypass(comp, bypass, cmdq_handle);
			}
		}
	}

	if (cmdq_pkt_flush_threaded(cmdq_handle, pq_bypass_cmdq_cb, cb_data) < 0)
		DDPPR_ERR("failed to flush user_cmd\n");

#ifndef DRM_CMDQ_DISABLE
	if (bypass)
		cmdq_mbox_disable(client->chan); /* GCE clk refcnt - 1 */
#endif
#endif
}

static void mtk_atomic_doze_preparation(struct drm_device *dev,
					 struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	int i;

	for_each_new_connector_in_state(old_state, connector,
		old_conn_state, i) {

		crtc = connector->state->crtc;
		if (!crtc) {
			DDPPR_ERR("%s connector has no crtc\n", __func__);
			continue;
		}

		if (old_state && old_state->crtcs[i].old_state)
			mtk_atomit_doze_update_pq(crtc, 0, old_state->crtcs[i].old_state->active);

		mtk_atomic_doze_update_dsi_state(dev, crtc, 1);

		mtk_atomic_force_doze_switch(dev, old_state, connector, crtc);

	}
}

static void mtk_atomic_doze_finish(struct drm_device *dev,
					 struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	int i;

	for_each_new_connector_in_state(old_state, connector,
		old_conn_state, i) {

		crtc = connector->state->crtc;
		if (!crtc) {
			DDPPR_ERR("%s connector has no crtc\n", __func__);
			continue;
		}

		mtk_atomic_doze_update_dsi_state(dev, crtc, 0);

		if (old_state && old_state->crtcs[i].old_state)
			mtk_atomit_doze_update_pq(crtc, 1, old_state->crtcs[i].old_state->active);
	}
}

static bool mtk_drm_is_enable_from_lk(struct drm_crtc *crtc)
{
	/* TODO: check if target CRTC has been turn on in LK */
	if (drm_crtc_index(crtc) == 0)
#ifndef CONFIG_MTK_DISP_NO_LK
		return true;
#else
		return false;
#endif
	return false;
}

static bool mtk_atomic_skip_plane_update(struct mtk_drm_private *private,
					 struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	/* If the power state goes to or leave to doze mode, the display
	 * engine should not be self-refresh it again to avoid updating the
	 * unexpected content. The middleware is only change one CRTC
	 * power state at a time and there has no new frame updating
	 * when power state change. So here we can traverse all the
	 * CRTC state and skip the whole frame update. If the behavior
	 * above changed, the statement below should be modified.
	 */
	for_each_old_crtc_in_state(state, crtc, old_crtc_state, i) {
		struct mtk_crtc_state *mtk_state =
			to_mtk_crtc_state(crtc->state);
		if (mtk_state->doze_changed ||
			(drm_atomic_crtc_needs_modeset(crtc->state) &&
			mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE])) {
			DDPINFO("%s doze changed, skip self-update\n",
				__func__);
			return true;
		}
	}

	return false;
#ifdef IF_ZERO
	/* The CRTC would be enabled in LK stage and the content of
	 * corresponding display
	 *  may modified unexpected when first set crtc. If the case occur, we
	 * skip the plane
	 * update to make sure the display correctly.
	 */
	if (!state->legacy_set_config)
		return false;

	/* In general case, the set crtc stage control one crtc only. If the
	 * multiple CRTCs set
	 * at the same time, we have to make sure all the CRTC has been enabled
	 * from LK, then
	 * skip the plane update.
	 */
	for_each_old_crtc_in_state(state, crtc, old_crtc_state, i) {
		if (!mtk_drm_is_enable_from_lk(crtc))
			return false;
	}

	return true;
#endif
}

bool mtk_drm_lcm_is_connect(void)
{
	struct device_node *chosen_node;

	chosen_node = of_find_node_by_path("/chosen");
	if (chosen_node) {
		struct tag_videolfb *videolfb_tag = NULL;
		unsigned long size = 0;

		videolfb_tag = (struct tag_videolfb *)of_get_property(
			chosen_node,
			"atag,videolfb",
			(int *)&size);
		if (videolfb_tag)
			return videolfb_tag->islcmfound;

		DDPINFO("[DT][videolfb] videolfb_tag not found\n");
	} else {
		DDPINFO("[DT][videolfb] of_chosen not found\n");
	}

	return false;
}

static void drm_atomic_esd_chk_first_enable(struct drm_device *dev,
				     struct drm_atomic_state *old_state)
{
	static bool is_first = true;
	int i;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;


	if (is_first) {
		for_each_old_crtc_in_state(old_state, crtc, old_crtc_state, i) {
			if (drm_crtc_index(crtc) == 0) {
				if  (mtk_drm_lcm_is_connect())
					mtk_disp_esd_check_switch(crtc, true);
				break;
			}
		}

		is_first = false;
	}
}

static void mtk_drm_enable_trig(struct drm_device *drm,
		struct drm_atomic_state *old_state)
{
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	int i;

	for_each_new_connector_in_state(old_state, connector,
				new_conn_state, i) {
		if (!new_conn_state->best_encoder)
			continue;

		if (!new_conn_state->crtc->state->active ||
		    !drm_atomic_crtc_needs_modeset(new_conn_state->crtc->state))
			continue;

		mtk_crtc_hw_block_ready(new_conn_state->crtc);
	}
}

static struct mml_submit *mtk_alloc_mml_submit(void)
{
	struct mml_submit *temp = NULL;
	unsigned int i = 0;

	temp = kzalloc(sizeof(struct mml_submit), GFP_KERNEL);
	temp->job = kzalloc(sizeof(struct mml_job), GFP_KERNEL);
	for (i = 0; i < MML_MAX_OUTPUTS; ++i)
		temp->pq_param[i] =	kzalloc(sizeof(struct mml_pq_param), GFP_KERNEL);

	return temp;
}

void mtk_free_mml_submit(struct mml_submit *temp)
{
	unsigned int i = 0;

	if (!temp)
		return;

	for (i = 0; i < MML_MAX_PLANES && i < temp->buffer.src.cnt; ++i) {
		if (temp->buffer.src.use_dma &&	temp->buffer.src.dmabuf[i]) {
			DDPINFO("%s dmabuf:0x%x\n", __func__, temp->buffer.src.dmabuf[i]);
			dma_buf_put(temp->buffer.src.dmabuf[i]);
		}
	}

	kfree(temp->job);
	for (i = 0; i < MML_MAX_OUTPUTS; ++i)
		kfree(temp->pq_param[i]);
	kfree(temp);
}

int copy_mml_submit(struct mml_submit *src, struct mml_submit *dst)
{
	struct mml_job *temp_job = NULL;
	struct mml_pq_param *temp_pq_param[MML_MAX_OUTPUTS] = {NULL, NULL};
	int i = 0;

	temp_job = dst->job;
	for (i = 0; i < MML_MAX_OUTPUTS; ++i)
		temp_pq_param[i] = dst->pq_param[i];

	memcpy(dst, src, sizeof(struct mml_submit));

	if (temp_job) {
		memmove(temp_job, dst->job, sizeof(struct mml_job));
		dst->job = temp_job;
	}

	for (i = 0; i < MML_MAX_OUTPUTS; ++i) {
		if (temp_pq_param[i]) {
			memcpy(temp_pq_param[i], dst->pq_param[i], sizeof(struct mml_pq_param));
			dst->pq_param[i] = temp_pq_param[i];
		}
	}

	return 0;
}

static int copy_mml_submit_from_user(struct mml_submit *src,
	struct mml_submit *dst)
{
	struct mml_job *temp_job = NULL;
	struct mml_pq_param *temp_pq_param[MML_MAX_OUTPUTS] = {NULL, NULL};
	int i = 0;

	temp_job = dst->job;
	for (i = 0; i < MML_MAX_OUTPUTS; ++i)
		temp_pq_param[i] = dst->pq_param[i];

	if (copy_from_user(dst, src, sizeof(struct mml_submit))) {
		DDPINFO("%s copy_from_user all fail\n", __func__);
		return -EINVAL;
	}

	if (copy_from_user(temp_job, dst->job, sizeof(struct mml_job))) {
		DDPMSG("%s copy_from_user mml_job fail\n", __func__);
		return -EINVAL;
	}
	dst->job = temp_job;

	for (i = 0; i < MML_MAX_OUTPUTS; ++i) {
		if (copy_from_user(temp_pq_param[i],
			dst->pq_param[i], sizeof(struct mml_pq_param))) {
			DDPMSG("%s copy_from_user mml_pq_param fail\n", __func__);
			return -EINVAL;
		}
		dst->pq_param[i] = temp_pq_param[i];
	}

	return 0;
}

static void *fd_to_dma_buf(int32_t fd)
{
	struct dma_buf *dmabuf = NULL;

	if (fd <= 0)
		return NULL;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf)) {
		DDPMSG("%s fail to get dma_buf by fd %d err %ld",
			__func__, fd, PTR_ERR(dmabuf));
		return NULL;
	}
	return (void *)dmabuf;
}

static bool _mtk_atomic_mml_plane(struct drm_device *dev,
	struct mtk_plane_state *mtk_plane_state)
{
	struct mml_submit *submit_kernel = NULL;
	struct mml_submit *submit_pq = NULL;
	struct mml_drm_ctx *mml_ctx = NULL;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(mtk_plane_state->crtc);
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(mtk_plane_state->crtc->state);
	int i = 0, j = 0;
	int ret = 0;
	unsigned int fps = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);
	unsigned int vtotal = mtk_crtc->base.state->adjusted_mode.vtotal;
	unsigned int line_time = 0;

	mml_ctx = mtk_drm_get_mml_drm_ctx(dev, &(mtk_crtc->base));
	if (!mml_ctx)
		return false;

	submit_pq = mtk_alloc_mml_submit();
	if (!submit_pq)
		goto err_alloc_submit_pq;

	submit_kernel = mtk_alloc_mml_submit();
	if (!submit_kernel)
		goto err_alloc_submit_kernel;

	ret = copy_mml_submit_from_user(
	    (struct mml_submit *)(mtk_plane_state->prop_val[PLANE_PROP_MML_SUBMIT]), submit_kernel);
	if (ret < 0)
		goto err_copy_submit;

	submit_kernel->update = false;
	submit_kernel->info.mode = MML_MODE_RACING;

	for (i = 0; i < MML_MAX_OUTPUTS; ++i) {
		for (j = 0; j < MML_MAX_PLANES; ++j) {
			submit_kernel->buffer.dest[i].fd[j] = -1;
			submit_kernel->buffer.dest[i].size[j] = 0;
		}
	}

	for (i = 0; i < MML_MAX_PLANES && i < submit_kernel->buffer.src.cnt; ++i) {
		int32_t fd = submit_kernel->buffer.src.fd[i];

		submit_kernel->buffer.src.use_dma = true;
		submit_kernel->buffer.src.dmabuf[i] = fd_to_dma_buf(fd);
	}

	mml_drm_split_info(submit_kernel, submit_pq);

	if (mtk_crtc_is_frame_trigger_mode(mtk_plane_state->crtc)) {
		struct mtk_ddp_comp *comp = mtk_ddp_comp_request_output(mtk_crtc);

		if (mtk_drm_is_idle(mtk_plane_state->crtc))
			mtk_drm_idlemgr_kick(__func__, mtk_plane_state->crtc, false);

		if (comp)
			mtk_ddp_comp_io_cmd(comp, NULL, DSI_GET_CMD_MODE_LINE_TIME, &line_time);
	} else
		line_time = (1000000000 / fps) / vtotal;

	submit_kernel->info.act_time = line_time * submit_pq->info.dest[0].data.height;

	DDPINFO("fps=%d vtotal=%d line_time=%d dst_h=%d act_time=%d dma=0x%x src_fmt=%#010x\n",
			fps, vtotal, line_time,
			submit_pq->info.dest[0].data.height,
			submit_kernel->info.act_time,
			submit_kernel->buffer.src.dmabuf[0],
			submit_kernel->info.src.format);

	ret = mml_drm_submit(mml_ctx, submit_kernel, &(mtk_crtc->mml_cb));
	if (ret)
		goto err_submit;

	CRTC_MMP_MARK(0, mml_dbg, crtc_state->prop_val[CRTC_PROP_LYE_IDX], MMP_MML_SUBMIT);

	/* release previous mml_cfg */
	mtk_free_mml_submit(mtk_crtc->mml_cfg);
	mtk_free_mml_submit(mtk_crtc->mml_cfg_pq);

	mtk_crtc->mml_cfg = submit_kernel;
	mtk_crtc->mml_cfg_pq = submit_pq;

	mtk_plane_state->mml_mode = MML_MODE_RACING;
	mtk_plane_state->mml_cfg = mtk_crtc->mml_cfg_pq;

	crtc_state->mml_dst_roi.x = mtk_plane_state->base.dst.x1;
	crtc_state->mml_dst_roi.y = mtk_plane_state->base.dst.y1;
	crtc_state->mml_dst_roi.width = submit_pq->info.dest[0].compose.width;
	crtc_state->mml_dst_roi.height = submit_pq->info.dest[0].compose.height;

	return true;

err_submit:
err_copy_submit:
err_alloc_submit_kernel:
	mtk_free_mml_submit(submit_kernel);
err_alloc_submit_pq:
	mtk_free_mml_submit(submit_pq);
	return false;
}

static void mtk_atomic_mml(struct drm_device *dev,
	struct drm_atomic_state *state)
{
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *plane_state, *old_plane_state;
	struct mtk_plane_state *mtk_plane_state;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	int i = 0;
	bool last_is_mml = false;

	for_each_old_crtc_in_state(state, crtc, old_crtc_state, i) {
		if (drm_crtc_index(crtc) == 0)
			break;
		else
			return;
	}
	mtk_crtc = to_mtk_crtc(crtc);

	last_is_mml = (mtk_crtc->mml_ir_state == MML_IR_IDLE) ? false : mtk_crtc->is_mml;
	mtk_crtc->is_mml = false;
	for_each_old_plane_in_state(state, plane, old_plane_state, i) {
		plane_state = plane->state;
		if (plane_state && plane_state->crtc && drm_crtc_index(plane_state->crtc) == 0) {
			mtk_plane_state = to_mtk_plane_state(plane_state);
			if (mtk_plane_state->prop_val[PLANE_PROP_IS_MML]) {
				mtk_crtc->is_mml = _mtk_atomic_mml_plane(dev, mtk_plane_state);
				break;
			}
		}
	}

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		struct mtk_crtc_state *s = to_mtk_crtc_state(new_crtc_state);

		if (drm_atomic_crtc_effectively_active(old_crtc_state) &&
		    drm_atomic_crtc_needs_modeset(new_crtc_state)) {
			if (s->lye_state.scn[i] == MML_RSZ ||
			    s->lye_state.scn[i] == MML_SRAM_ONLY) {
				s->lye_state.scn[i] = NONE;
				DDPMSG("%s:%d clear MML scn after suspend\n", __func__, __LINE__);
			}
		}

		/* if resume from ir idle, the old addon should not be disconnect again */
		if (mtk_crtc->mml_ir_state == MML_IR_IDLE)
			s->lye_state.scn[i] = NONE;
	}

	if (!last_is_mml && mtk_crtc->is_mml)
		mtk_crtc->mml_ir_state = MML_IR_ENTERING;
	else if (last_is_mml && mtk_crtc->is_mml)
		mtk_crtc->mml_ir_state = MML_IR_RACING;
	else if (last_is_mml && !mtk_crtc->is_mml)
		mtk_crtc->mml_ir_state = MML_IR_LEAVING;
	else
		mtk_crtc->mml_ir_state = NOT_MML_IR;
}

static void mtk_set_first_config(struct drm_device *dev,
					struct drm_atomic_state *old_state)
{
	struct drm_connector *connector;
	struct mtk_drm_private *private = dev->dev_private;
	struct drm_connector_state *new_conn_state;
	int i;

	for_each_new_connector_in_state(old_state, connector, new_conn_state, i) {
		if (private->already_first_config == false &&
				connector->encoder && connector->encoder->crtc) {
			private->already_first_config = true;
			DDPMSG("%s, set first config true\n", __func__);
		}
	}
}

static void mtk_atomic_complete(struct mtk_drm_private *private,
				struct drm_atomic_state *state)
{
	struct drm_device *drm = private->drm;

	mtk_atomic_wait_for_fences(state);

	/*
	 * Mediatek drm supports runtime PM, so plane registers cannot be
	 * written when their crtc is disabled.
	 *
	 * The comment for drm_atomic_helper_commit states:
	 *     For drivers supporting runtime PM the recommended sequence is
	 *
	 *     drm_atomic_helper_commit_modeset_disables(dev, state);
	 *     drm_atomic_helper_commit_modeset_enables(dev, state);
	 *     drm_atomic_helper_commit_planes(dev, state,
	 *                                     DRM_PLANE_COMMIT_ACTIVE_ONLY);
	 *
	 * See the kerneldoc entries for these three functions for more details.
	 */
	/*
	 * To change the CRTC doze state, call the encorder enable/disable
	 * directly if the actvie state doesn't change.
	 */
	mtk_atomic_doze_preparation(drm, state);

	drm_atomic_helper_commit_modeset_disables(drm, state);
	drm_atomic_helper_commit_modeset_enables(drm, state);

	mtk_set_first_config(drm, state);

	mtk_drm_enable_trig(drm, state);

	mtk_atomic_disp_rsz_roi(drm, state);

	mtk_atomic_calculate_plane_enabled_number(drm, state);

	mtk_atomic_check_plane_sec_state(drm, state);

	mtk_atomic_mml(drm, state);

	if (!mtk_atomic_skip_plane_update(private, state)) {
		drm_atomic_helper_commit_planes(drm, state,
						DRM_PLANE_COMMIT_ACTIVE_ONLY);
		drm_atomic_esd_chk_first_enable(drm, state);
	}

	mtk_atomic_doze_finish(drm, state);

	if (!mtk_drm_helper_get_opt(private->helper_opt,
				    MTK_DRM_OPT_COMMIT_NO_WAIT_VBLANK))
		drm_atomic_helper_wait_for_vblanks(drm, state);

	drm_atomic_helper_cleanup_planes(drm, state);
	drm_atomic_state_put(state);
}

static void mtk_atomic_work(struct work_struct *work)
{
	struct mtk_drm_private *private =
		container_of(work, struct mtk_drm_private, commit.work);

	mtk_atomic_complete(private, private->commit.state);
}

static int mtk_atomic_check(struct drm_device *dev,
			    struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct mtk_crtc_state *old_state, *new_state;
	int i, ret = 0;

	ret = drm_atomic_helper_check(dev, state);
	if (ret)
		return ret;

	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		old_state = to_mtk_crtc_state(crtc->state);
		new_state = to_mtk_crtc_state(crtc_state);

		if (drm_crtc_index(crtc) == 0)
			mtk_drm_crtc_mode_check(crtc, crtc->state, crtc_state);

		if (old_state->prop_val[CRTC_PROP_DOZE_ACTIVE] ==
		    new_state->prop_val[CRTC_PROP_DOZE_ACTIVE])
			continue;
		DDPINFO("[CRTC:%d:%s] doze active changed\n", crtc->base.id,
				crtc->name);
		new_state->doze_changed = true;
		ret = drm_atomic_add_affected_connectors(state, crtc);
		if (ret) {
			DDPINFO("DRM add conn failed! state:%p, ret:%d\n",
					state, ret);
			return ret;
		}
	}

	return ret;
}

static int mtk_atomic_commit(struct drm_device *drm,
			     struct drm_atomic_state *state, bool async)
{
	struct mtk_drm_private *private = drm->dev_private;
	uint32_t crtc_mask;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	int ret, i = 0;
	int index;

	DDP_PROFILE("[PROFILE] %s+\n", __func__);
	ret = drm_atomic_helper_prepare_planes(drm, state);
	if (ret)
		return ret;

	mutex_lock(&private->commit.lock);
	flush_work(&private->commit.work);

	crtc_mask = mtk_atomic_crtc_mask(drm, state);

	DRM_MMP_EVENT_START(mutex_lock, 0, 0);
	for (i = 0; i < MAX_CRTC; i++) {
		if (!(crtc_mask >> i & 0x1))
			continue;

		crtc = private->crtc[i];
		mtk_crtc = to_mtk_crtc(crtc);
		index = drm_crtc_index(crtc);

		// if last frame is mml, need to wait job done before holding lock
		if (mtk_crtc->is_mml) {
			ret = wait_event_interruptible(
				mtk_crtc->signal_mml_last_job_is_flushed_wq
				, atomic_read(&mtk_crtc->wait_mml_last_job_is_flushed));
		}
		atomic_set(&(mtk_crtc->wait_mml_last_job_is_flushed), 0);

		DRM_MMP_MARK(mutex_lock, (unsigned long)&mtk_crtc->lock, i);

		DDP_MUTEX_LOCK_NESTED(&mtk_crtc->lock, i, __func__, __LINE__);
		CRTC_MMP_EVENT_START(index, atomic_commit, 0, 0);
	}
	mutex_nested_time_start = sched_clock();

	ret = drm_atomic_helper_swap_state(state, 0);
	if (ret) {
		DDPPR_ERR("DRM swap state failed! state:%p, ret:%d\n",
				state, ret);
		goto err_mutex_unlock;
	}

	drm_atomic_state_get(state);
	if (async)
		mtk_atomic_schedule(private, state);
	else
		mtk_atomic_complete(private, state);

	mutex_nested_time_end = sched_clock();
	mutex_nested_time_period =
			mutex_nested_time_end - mutex_nested_time_start;
	if (mutex_nested_time_period > 1000000000) {
		DDPPR_ERR("M_ULOCK_NESTED:%s[%d] timeout:<%lld ns>!\n",
			__func__, __LINE__, mutex_nested_time_period);
		DRM_MMP_MARK(mutex_lock,
			(unsigned long)mutex_time_period, 0);
		dump_stack();
	}

err_mutex_unlock:
	for (i = MAX_CRTC - 1; i >= 0; i--) {
		if (!(crtc_mask >> i & 0x1))
			continue;

		crtc = private->crtc[i];
		mtk_crtc = to_mtk_crtc(crtc);
		index = drm_crtc_index(crtc);

		CRTC_MMP_EVENT_END(index, atomic_commit, 0, 0);
		DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->lock, i, __func__, __LINE__);
		DRM_MMP_MARK(mutex_lock, (unsigned long)&mtk_crtc->lock,
				i + (1 << 8));
	}
	DRM_MMP_EVENT_END(mutex_lock, 0, 0);

	mutex_unlock(&private->commit.lock);
	DDP_PROFILE("[PROFILE] %s-\n", __func__);

	return 0;
}

static struct drm_atomic_state *
mtk_drm_atomic_state_alloc(struct drm_device *dev)
{
	struct mtk_atomic_state *mtk_state;

	mtk_state = kzalloc(sizeof(*mtk_state), GFP_KERNEL);
	if (!mtk_state)
		return NULL;

	if (drm_atomic_state_init(dev, &mtk_state->base) < 0) {
		kfree(mtk_state);
		return NULL;
	}

	INIT_LIST_HEAD(&mtk_state->list);
	kref_init(&mtk_state->kref);

	return &mtk_state->base;
}

static const struct drm_mode_config_funcs mtk_drm_mode_config_funcs = {
	.fb_create = mtk_drm_mode_fb_create,
	.atomic_check = mtk_atomic_check,
	.atomic_commit = mtk_atomic_commit,
	.atomic_state_alloc = mtk_drm_atomic_state_alloc,
	.atomic_state_free = mtk_atomic_state_put,
};

static const enum mtk_ddp_comp_id mt2701_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_RDMA0, DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_BLS,  DDP_COMPONENT_DSI0,
};

static const enum mtk_ddp_comp_id mt2701_mtk_ddp_ext[] = {
	DDP_COMPONENT_RDMA1, DDP_COMPONENT_DPI0,
};

static const enum mtk_ddp_comp_id mt2712_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0,  DDP_COMPONENT_COLOR0, DDP_COMPONENT_AAL0,
	DDP_COMPONENT_OD,    DDP_COMPONENT_RDMA0,  DDP_COMPONENT_DPI0,
	DDP_COMPONENT_WDMA0, DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt2712_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL1,  DDP_COMPONENT_COLOR1, DDP_COMPONENT_AAL1,
	DDP_COMPONENT_OD1,   DDP_COMPONENT_RDMA1,  DDP_COMPONENT_DPI1,
	DDP_COMPONENT_WDMA1, DDP_COMPONENT_PWM1,
};

static enum mtk_ddp_comp_id mt8173_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_COLOR0, DDP_COMPONENT_AAL0,
	DDP_COMPONENT_OD,   DDP_COMPONENT_RDMA0,  DDP_COMPONENT_UFOE,
	DDP_COMPONENT_DSI0, DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt8173_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL1,  DDP_COMPONENT_COLOR1, DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_RDMA1, DDP_COMPONENT_DPI0,
};

static const enum mtk_ddp_comp_id mt6779_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,  DDP_COMPONENT_OVL0,
	DDP_COMPONENT_RDMA0,    DDP_COMPONENT_RDMA0_VIRTUAL0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6779_mtk_ddp_ext[] = {
// Todo: Just for create a simple external session
// and Can create crtc1 successfully use this path.
#ifdef IF_ZERO
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_WDMA0,
#else
	DDP_COMPONENT_OVL1_2L, DDP_COMPONENT_RDMA0,
/*DDP_COMPONENT_DPI0,*/
#endif
};

static const enum mtk_ddp_comp_id mt6779_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL1_2L, DDP_COMPONENT_WDMA_VIRTUAL0,
	DDP_COMPONENT_WDMA_VIRTUAL1, DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6779_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA_VIRTUAL0, DDP_COMPONENT_WDMA_VIRTUAL1,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6779_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,    DDP_COMPONENT_RDMA0_VIRTUAL0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6779_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA_VIRTUAL0,
	DDP_COMPONENT_WDMA_VIRTUAL1, DDP_COMPONENT_WDMA0,
};


static const enum mtk_ddp_comp_id mt6885_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,		DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL0_VIRTUAL0,	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_RDMA0_VIRTUAL0,	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,		DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0,	DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,		DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6885_mtk_ddp_dual_main[] = {
	DDP_COMPONENT_OVL1_2L,		DDP_COMPONENT_OVL1,
	DDP_COMPONENT_OVL1_VIRTUAL0,	DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_RDMA1_VIRTUAL0, DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,		DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1,	DDP_COMPONENT_DITHER1,
};

static const enum mtk_ddp_comp_id mt6885_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0,	DDP_COMPONENT_OVL0_VIRTUAL0,
	DDP_COMPONENT_WDMA0,
};


static const enum mtk_ddp_comp_id mt6885_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,		DDP_COMPONENT_OVL0,
	DDP_COMPONENT_OVL0_VIRTUAL0,	DDP_COMPONENT_WDMA0,
};


static const enum mtk_ddp_comp_id mt6885_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,		DDP_COMPONENT_RDMA0_VIRTUAL0,
	DDP_COMPONENT_COLOR0,		DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,		DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0,	DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,		DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6885_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_RDMA4,
	DDP_COMPONENT_DP_INTF0,
// path to DSI1
//	DDP_COMPONENT_OVL1_2L,		DDP_COMPONENT_OVL1,
//	DDP_COMPONENT_OVL1_VIRTUAL0,	DDP_COMPONENT_RDMA1,
//	DDP_COMPONENT_RDMA1_VIRTUAL0, DDP_COMPONENT_DSI1
};

static const enum mtk_ddp_comp_id mt6885_dual_data_ext[] = {
	DDP_COMPONENT_OVL3_2L,
	DDP_COMPONENT_RDMA5,
	/*DDP_COMPONENT_MERGE1,*/
	DDP_COMPONENT_DSC0,
};
static const enum mtk_ddp_comp_id mt6885_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL2_2L, DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L, /*DDP_COMPONENT_OVL1_2L,*/
	DDP_COMPONENT_OVL0, DDP_COMPONENT_OVL0_VIRTUAL0,
	DDP_COMPONENT_OVL0_VIRTUAL1, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_CM0,       DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#else
	DDP_COMPONENT_RDMA0_OUT_RELAY,
#endif
	DDP_COMPONENT_DSI0,      DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,    DDP_COMPONENT_CHIST1,
#endif
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_main_bringup[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC3,
	DDP_COMPONENT_DLI_ASYNC0,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ0_OUT_CB3,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB1,
#else
#if defined(MT6985_FULL_PQ_PATH1)
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
#elif defined(MT6985_FULL_PQ_PATH2)
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_C3D0,
	DDP_COMPONENT_COLOR0,    DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_CCORR1,    DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
#elif defined(MT6985_FULL_PQ_PATH3)
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_DMDP_AAL0, DDP_COMPONENT_AAL0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
#elif defined(MT6985_FULL_PQ_PATH4)
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_C3D0,
	DDP_COMPONENT_COLOR0,    DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_CCORR1,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
#elif defined(MT6985_FULL_PQ_PATH5)
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
#elif defined(MT6985_FULL_PQ_PATH6)
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_C3D0,
	DDP_COMPONENT_COLOR0,    DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_CCORR1,    DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
#endif
	DDP_COMPONENT_SPR0,      DDP_COMPONENT_ODDMR0,
	DDP_COMPONENT_DITHER1,   DDP_COMPONENT_POSTALIGN0,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB0,
#endif
	DDP_COMPONENT_COMP0_OUT_CB3,
	DDP_COMPONENT_MERGE0_OUT_CB0,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,    DDP_COMPONENT_CHIST1,
#endif
};

/* CRTC0 */
static const enum mtk_ddp_comp_id mt6985_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
//	DDP_COMPONENT_OVL0_BG_CB0,
	DDP_COMPONENT_OVL1_2L,
//	DDP_COMPONENT_OVL0_BG_CB1,
	DDP_COMPONENT_OVL2_2L,
//	DDP_COMPONENT_OVL0_BLEND_CB0,
//	DDP_COMPONENT_DLO_RELAY,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC3,
	DDP_COMPONENT_DLI_ASYNC0,
	DDP_COMPONENT_PQ0_IN_CB0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ0_OUT_CB0,
#else
	DDP_COMPONENT_PQ0_OUT_CB3,
#endif
	DDP_COMPONENT_SPR0,		DDP_COMPONENT_ODDMR0,
	DDP_COMPONENT_DITHER0,	DDP_COMPONENT_POSTALIGN0,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB0,
	DDP_COMPONENT_DSC0,
	DDP_COMPONENT_COMP0_OUT_CB1,
	DDP_COMPONENT_MERGE0_OUT_CB0,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_dual_main_bringup[] = {
	DDP_COMPONENT_OVL4_2L,
	DDP_COMPONENT_OVL5_2L,
	DDP_COMPONENT_OVL6_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC10,
	DDP_COMPONENT_DLI_ASYNC6,
#ifdef DRM_BYPASS_PQ
	DDP_COMPONENT_PQ1_OUT_CB3,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB1,
#else
#if defined(MT6985_FULL_PQ_PATH1)
	DDP_COMPONENT_RSZ2,
	DDP_COMPONENT_TDSHP2,    DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_CCORR2,    DDP_COMPONENT_CCORR3,
	DDP_COMPONENT_C3D1,      DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,      DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER2,
	DDP_COMPONENT_PQ1_OUT_CB0,
#elif defined(MT6985_FULL_PQ_PATH2)
	DDP_COMPONENT_RSZ2,
	DDP_COMPONENT_TDSHP2,    DDP_COMPONENT_C3D1,
	DDP_COMPONENT_COLOR1,    DDP_COMPONENT_CCORR2,
	DDP_COMPONENT_CCORR3,    DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,      DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER2,
	DDP_COMPONENT_PQ1_OUT_CB0,
#elif defined(MT6985_FULL_PQ_PATH3)
	DDP_COMPONENT_RSZ2,
	DDP_COMPONENT_TDSHP2,    DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_DMDP_AAL1, DDP_COMPONENT_AAL1,
	DDP_COMPONENT_CCORR2,    DDP_COMPONENT_CCORR3,
	DDP_COMPONENT_C3D1,      DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER2,
	DDP_COMPONENT_PQ1_OUT_CB0,
#elif defined(MT6985_FULL_PQ_PATH4)
	DDP_COMPONENT_RSZ2,
	DDP_COMPONENT_TDSHP2,    DDP_COMPONENT_C3D1,
	DDP_COMPONENT_COLOR1,    DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,      DDP_COMPONENT_CCORR2,
	DDP_COMPONENT_CCORR3,      DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER2,
	DDP_COMPONENT_PQ1_OUT_CB0,
#elif defined(MT6985_FULL_PQ_PATH5)
	DDP_COMPONENT_RSZ2,
	DDP_COMPONENT_TDSHP2,    DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,      DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_CCORR2,    DDP_COMPONENT_CCORR3,
	DDP_COMPONENT_C3D1,      DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER2,
	DDP_COMPONENT_PQ1_OUT_CB0,
#elif defined(MT6985_FULL_PQ_PATH6)
	DDP_COMPONENT_RSZ2,
	DDP_COMPONENT_TDSHP2,    DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,      DDP_COMPONENT_C3D1,
	DDP_COMPONENT_COLOR1,    DDP_COMPONENT_CCORR2,
	DDP_COMPONENT_CCORR3,    DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER2,
	DDP_COMPONENT_PQ1_OUT_CB0,
#endif
	DDP_COMPONENT_SPR1,      DDP_COMPONENT_ODDMR1,
	DDP_COMPONENT_DITHER3,   DDP_COMPONENT_POSTALIGN1,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB0,
#endif
	DDP_COMPONENT_COMP1_OUT_CB3,
	DDP_COMPONENT_MERGE1_OUT_CB0,
	DDP_COMPONENT_DLO_ASYNC2,
	DDP_COMPONENT_DLI_ASYNC4,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST2,    DDP_COMPONENT_CHIST3,
#endif
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_dual_main[] = {
	DDP_COMPONENT_OVL4_2L,
//	DDP_COMPONENT_OVL1_BG_CB0,
	DDP_COMPONENT_OVL5_2L,
//	DDP_COMPONENT_OVL1_BG_CB1,
	DDP_COMPONENT_OVL6_2L,
//	DDP_COMPONENT_OVL0_BLEND_CB0,
//	DDP_COMPONENT_DLO_RELAY,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC10,
	DDP_COMPONENT_DLI_ASYNC6,
	DDP_COMPONENT_PQ1_IN_CB0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_RSZ0,
	DDP_COMPONENT_TDSHP0,    DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,    DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,      DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_PQ1_OUT_CB0,
#else
	DDP_COMPONENT_PQ1_OUT_CB3,
#endif
	DDP_COMPONENT_SPR1,		DDP_COMPONENT_ODDMR1,
	DDP_COMPONENT_DITHER2,	DDP_COMPONENT_POSTALIGN1,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB0,
	DDP_COMPONENT_VDCM1,
	DDP_COMPONENT_COMP1_OUT_CB0,
	DDP_COMPONENT_MERGE1_OUT_CB0,
	DDP_COMPONENT_DLO_ASYNC2,
};
/* CRTC0 */

/* CRTC1 */
static const enum mtk_ddp_comp_id mt6985_mtk_ddp_ext_dp[] = {
	DDP_COMPONENT_OVL3_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC5,
	DDP_COMPONENT_DLI_ASYNC2,
	DDP_COMPONENT_PQ0_OUT_CB4,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB2,
	DDP_COMPONENT_COMP0_OUT_CB4,
	DDP_COMPONENT_MERGE0_OUT_CB1,
	DDP_COMPONENT_DP_INTF0,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_dual_ext_dp[] = {
	DDP_COMPONENT_OVL7_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC12,
	DDP_COMPONENT_DLI_ASYNC8,
	DDP_COMPONENT_PQ1_OUT_CB4,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB2,
	DDP_COMPONENT_COMP1_OUT_CB4,
	DDP_COMPONENT_MERGE1_OUT_CB1,
	DDP_COMPONENT_DLO_ASYNC3,
	DDP_COMPONENT_DLI_ASYNC5,
	DDP_COMPONENT_MERGE1,
};
/* CRTC1 */

/* CRTC2 */
static const enum mtk_ddp_comp_id mt6985_mtk_ddp_mem_dp_w_tdshp[] = {
	DDP_COMPONENT_OVL7_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC13,
	DDP_COMPONENT_DLI_ASYNC3,
	DDP_COMPONENT_PQ0_IN_CB3,
	DDP_COMPONENT_PQ0_OUT_CB4,
	//DDP_COMPONENT_TDSHP1,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB2,
	DDP_COMPONENT_COMP0_OUT_CB4,
	DDP_COMPONENT_MERGE0_OUT_CB2,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_mem_dp_wo_tdshp[] = {
	DDP_COMPONENT_OVL7_2L,
	DDP_COMPONENT_OVL7_2L_VIRTUAL0,
	DDP_COMPONENT_OVLSYS_WDMA2,
};

/* CRTC2 */

/* CRTC3 */
static const enum mtk_ddp_comp_id mt6985_mtk_ddp_secondary_dp[] = {
	DDP_COMPONENT_OVL7_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC12,
	DDP_COMPONENT_DLI_ASYNC8,
	DDP_COMPONENT_PQ1_OUT_CB4,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB3,
	DDP_COMPONENT_COMP1_OUT_CB5,
	DDP_COMPONENT_MERGE1_OUT_CB3,
	DDP_COMPONENT_DSI1,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_dual_secondary_dp[] = {
	DDP_COMPONENT_OVL2_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC5,
	DDP_COMPONENT_DLI_ASYNC2,
	DDP_COMPONENT_PQ0_IN_CB2,
	DDP_COMPONENT_PQ0_OUT_CB4,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB3,
	DDP_COMPONENT_COMP0_OUT_CB5,
	DDP_COMPONENT_MERGE0_OUT_CB3,
	DDP_COMPONENT_DLO_ASYNC0,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_discrete_chip[] = {
	DDP_COMPONENT_OVL7_2L,
	DDP_COMPONENT_OVLSYS_DLO_ASYNC12,
	DDP_COMPONENT_DLI_ASYNC8,
	DDP_COMPONENT_PQ1_OUT_CB4,
	DDP_COMPONENT_PANEL1_COMP_OUT_CB3,
	DDP_COMPONENT_COMP1_OUT_CB5,
	DDP_COMPONENT_MERGE1_OUT_CB3,
	DDP_COMPONENT_DSI1,
};

static const enum mtk_ddp_comp_id mt6985_mtk_ddp_dual_discrete_chip[] = {
	DDP_COMPONENT_MDP_RDMA0,
	DDP_COMPONENT_Y2R0,
	DDP_COMPONENT_PQ0_IN_CB4,
	DDP_COMPONENT_PQ0_OUT_CB4,
	DDP_COMPONENT_PANEL0_COMP_OUT_CB3,
	DDP_COMPONENT_COMP0_OUT_CB5,
	DDP_COMPONENT_MERGE0_OUT_CB3,
	DDP_COMPONENT_DLO_ASYNC1,
};

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_dual_main[] = {
	/* Can't enable dual pipe with bypass PQ */
	DDP_COMPONENT_OVL2_2L, /*DDP_COMPONENT_OVL3_2L,*/
	DDP_COMPONENT_OVL1, DDP_COMPONENT_OVL1_VIRTUAL0,
	DDP_COMPONENT_OVL1_VIRTUAL1, DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_TDSHP1,	 DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_CCORR2,	 DDP_COMPONENT_CCORR3,
	DDP_COMPONENT_C3D1,	 DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,	 DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER1,
	DDP_COMPONENT_CM1,		 DDP_COMPONENT_SPR1,
	DDP_COMPONENT_PQ1_VIRTUAL, DDP_COMPONENT_DLO_ASYNC4,
	DDP_COMPONENT_DLI_ASYNC0,

	DDP_COMPONENT_PWM1, /* This PWM is for connect CHIST */
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST2,	 DDP_COMPONENT_CHIST3,
#endif
};

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_main_wb_path[] = {
	// todo ..
	//DDP_COMPONENT_OVL0,	DDP_COMPONENT_OVL0_VIRTUAL0,
	//DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_ext_dp[] = {
	DDP_COMPONENT_OVL2_2L_NWCG,
	DDP_COMPONENT_OVL2_2L_NWCG_VIRTUAL0, DDP_COMPONENT_RDMA3,
	DDP_COMPONENT_SUB1_VIRTUAL1,
	DDP_COMPONENT_DP_INTF0,
};

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_ext_dsi[] = {
	DDP_COMPONENT_OVL0_2L_NWCG,
	DDP_COMPONENT_OVL0_2L_NWCG_VIRTUAL0, DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_TV0_VIRTUAL,
	DDP_COMPONENT_DLO_ASYNC2, DDP_COMPONENT_DLI_ASYNC6,
	DDP_COMPONENT_MERGE1,
	DDP_COMPONENT_MAIN1_VIRTUAL,
	DDP_COMPONENT_DSI1,
};

static const enum mtk_ddp_comp_id mt6983_dual_data_ext[] = {
	DDP_COMPONENT_OVL0_2L_NWCG,
	DDP_COMPONENT_OVL0_2L_NWCG_VIRTUAL0, DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_TV0_VIRTUAL,
	DDP_COMPONENT_DLO_ASYNC2, DDP_COMPONENT_DLI_ASYNC6,
	DDP_COMPONENT_MERGE1,
	/*DDP_COMPONENT_DSC1,*/
};

static const enum mtk_ddp_comp_id mt6983_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL1_2L_NWCG, DDP_COMPONENT_WDMA1,
};

static const enum mtk_ddp_comp_id mt6895_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_OVL0_VIRTUAL0,
	DDP_COMPONENT_OVL0_VIRTUAL1, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP0,	 DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,	 DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,	 DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,	 DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_CM0,		 DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#else
	DDP_COMPONENT_RDMA0_OUT_RELAY,
#endif
	DDP_COMPONENT_DSI0,	 DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,	 DDP_COMPONENT_CHIST1,
#endif
};

static const enum mtk_ddp_comp_id mt6895_mtk_ddp_dual_main[] = {
	/* Can't enable dual pipe with bypass PQ */
	DDP_COMPONENT_OVL3_2L,
	DDP_COMPONENT_OVL1, DDP_COMPONENT_OVL1_VIRTUAL0,
	DDP_COMPONENT_OVL1_VIRTUAL1, DDP_COMPONENT_RDMA2,
	DDP_COMPONENT_TDSHP1,	 DDP_COMPONENT_COLOR1,
	DDP_COMPONENT_CCORR2,	 DDP_COMPONENT_CCORR3,
	DDP_COMPONENT_C3D1,  DDP_COMPONENT_DMDP_AAL1,
	DDP_COMPONENT_AAL1,  DDP_COMPONENT_GAMMA1,
	DDP_COMPONENT_POSTMASK1, DDP_COMPONENT_DITHER1,
	DDP_COMPONENT_CM1,		 DDP_COMPONENT_SPR1,
	DDP_COMPONENT_PQ1_VIRTUAL, DDP_COMPONENT_DLO_ASYNC4,
	DDP_COMPONENT_DLI_ASYNC0,

	DDP_COMPONENT_PWM1, /* This PWM is for connect CHIST */
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST2,	 DDP_COMPONENT_CHIST3,
#endif
};

/* mt6886 is the same as mt6895 */
static const enum mtk_ddp_comp_id mt6895_mtk_ddp_main_wb_path[] = {
	// todo ..
	//DDP_COMPONENT_OVL0,	DDP_COMPONENT_OVL0_VIRTUAL0,
	//DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6895_mtk_ddp_ext[] = {
	DDP_COMPONENT_OVL2_2L, DDP_COMPONENT_OVL2_2L_VIRTUAL0,
	DDP_COMPONENT_SUB_OVL_DISP1_PQ0_VIRTUAL,
	DDP_COMPONENT_OVL2_2L_NWCG_VIRTUAL0, DDP_COMPONENT_RDMA3,
	DDP_COMPONENT_SUB1_VIRTUAL1,
	DDP_COMPONENT_DP_INTF0,
};

static const enum mtk_ddp_comp_id mt6895_dual_data_ext[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_OVL0_2L_VIRTUAL0,
	DDP_COMPONENT_SUB_OVL_DISP0_PQ0_VIRTUAL,
	DDP_COMPONENT_OVL0_2L_NWCG_VIRTUAL0, DDP_COMPONENT_RDMA1,
	DDP_COMPONENT_TV0_VIRTUAL,
	DDP_COMPONENT_DLO_ASYNC2, DDP_COMPONENT_DLI_ASYNC6,
	/*DDP_COMPONENT_MERGE1,*/
	DDP_COMPONENT_DSC1,
};

static const enum mtk_ddp_comp_id mt6895_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_OVL0_2L_VIRTUAL0,
	DDP_COMPONENT_SUB_OVL_DISP0_PQ0_VIRTUAL, DDP_COMPONENT_WDMA1,
};

static const enum mtk_ddp_comp_id mt6886_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_OVL0_2L_VIRTUAL0,
	DDP_COMPONENT_SUB_OVL_DISP0_PQ0_VIRTUAL, DDP_COMPONENT_WDMA1,
};

static const enum mtk_ddp_comp_id mt6886_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_OVL0_VIRTUAL0,
	DDP_COMPONENT_OVL0_VIRTUAL1, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP0_VIRTUAL,
	DDP_COMPONENT_COLOR0,
	DDP_COMPONENT_CCORR0,	 DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,
	DDP_COMPONENT_AAL0,	 DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_CM0,		 DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#else
	DDP_COMPONENT_RDMA0_OUT_RELAY,
#endif
	DDP_COMPONENT_DSI0,	 DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,
#endif
};

static const struct mtk_addon_module_data addon_rsz_data[] = {
	{DISP_RSZ, ADDON_BETWEEN, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data addon_rsz_data_v2[] = {
	{DISP_RSZ_v2, ADDON_BETWEEN, DDP_COMPONENT_OVL0_2L},
};
static const struct mtk_addon_module_data addon_rsz_data_v3[] = {
	{DISP_RSZ_v3, ADDON_BETWEEN, DDP_COMPONENT_OVL1_2L},
};

static const struct mtk_addon_module_data addon_rsz_data_v4[] = {
	{DISP_RSZ_v4, ADDON_BETWEEN, DDP_COMPONENT_OVL2_2L},
};

static const struct mtk_addon_module_data addon_rsz_data_v5[] = {
	{DISP_RSZ_v5, ADDON_BETWEEN, DDP_COMPONENT_OVL1_2L},
};

static const struct mtk_addon_module_data addon_rsz_data_v6[] = {
	{DISP_RSZ_v6, ADDON_BETWEEN, DDP_COMPONENT_OVL3_2L},
};

static const struct mtk_addon_module_data addon_ovl_rsz_data[] = {
	{OVL_RSZ, ADDON_EMBED, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data addon_ovl_rsz_data_1[] = {
	{OVL_RSZ_1, ADDON_EMBED, DDP_COMPONENT_OVL4_2L},
};

static const struct mtk_addon_module_data addon_wdma0_data[] = {
	{DISP_WDMA0, ADDON_AFTER, DDP_COMPONENT_DITHER0},
};

static const struct mtk_addon_module_data addon_wdma1_data[] = {
	{DISP_WDMA1, ADDON_AFTER, DDP_COMPONENT_DITHER1},
};

static const struct mtk_addon_module_data mt6983_addon_wdma0_data[] = {
	{DISP_WDMA0, ADDON_AFTER, DDP_COMPONENT_SPR0},
};

static const struct mtk_addon_module_data mt6983_addon_wdma0_data_v2[] = {
	{DISP_WDMA0_v2, ADDON_AFTER, DDP_COMPONENT_OVL0_VIRTUAL0},
};

static const struct mtk_addon_module_data mt6983_addon_wdma2_data[] = {
	{DISP_WDMA2, ADDON_AFTER, DDP_COMPONENT_SPR1},
};

static const struct mtk_addon_module_data mt6983_addon_wdma2_data_v2[] = {
	{DISP_WDMA2_v2, ADDON_AFTER, DDP_COMPONENT_OVL1_VIRTUAL0},
};

static const struct mtk_addon_module_data mt6985_addon_wdma0_data[] = {
	/* real case: need wait PQ */
	/* {DISP_WDMA0_v3, ADDON_AFTER, DDP_COMPONENT_PQ0_OUT_CB0}, */
	{DISP_WDMA0_v3, ADDON_AFTER, DDP_COMPONENT_PANEL0_COMP_OUT_CB0},
};

static const struct mtk_addon_module_data mt6985_addon_wdma1_data[] = {
	{DISP_WDMA1, ADDON_AFTER, DDP_COMPONENT_MERGE1_OUT_CB0},
};

static const struct mtk_addon_module_data mt6985_addon_ovlsys_wdma0_data[] = {
	{DISP_OVLSYS_WDMA0, ADDON_AFTER, DDP_COMPONENT_OVL2_2L},
};

static const struct mtk_addon_module_data mt6985_addon_ovlsys_wdma2_data[] = {
	{DISP_OVLSYS_WDMA2, ADDON_AFTER, DDP_COMPONENT_OVL6_2L},
};

/* mt6886 is the same as mt6895 */
static const struct mtk_addon_module_data mt6895_addon_wdma0_data[] = {
	{DISP_WDMA0, ADDON_AFTER, DDP_COMPONENT_SPR0},
};

static const struct mtk_addon_module_data mt6895_addon_wdma2_data[] = {
	{DISP_WDMA2, ADDON_AFTER, DDP_COMPONENT_SPR1},
};

static const struct mtk_addon_module_data mt6855_addon_rsz_data[] = {
	{DISP_RSZ, ADDON_BETWEEN, DDP_COMPONENT_OVL1_2L},
};
static const struct mtk_addon_module_data mt6855_addon_wdma0_data[] = {
	{DISP_WDMA0, ADDON_AFTER, DDP_COMPONENT_SPR0_VIRTUAL},
};

static const struct mtk_addon_module_data addon_mml_sram_only_data[] = {
	{DISP_MML_SRAM_ONLY, ADDON_BETWEEN, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data mt6886_addon_mml_sram_only_data[] = {
	{DISP_MML_SRAM_ONLY, ADDON_BETWEEN, DDP_COMPONENT_OVL1_2L},
};


static const struct mtk_addon_module_data mt6895_addon_mml_sram_only_data_1[] = {
	{DISP_MML_SRAM_ONLY_1, ADDON_BETWEEN, DDP_COMPONENT_OVL2_2L},
};

static const struct mtk_addon_module_data mt6983_addon_mml_sram_only_data_1[] = {
	{DISP_MML_SRAM_ONLY_1, ADDON_BETWEEN, DDP_COMPONENT_OVL2_2L},
};

static const struct mtk_addon_module_data mt6985_addon_mml_sram_only_data_1[] = {
	{DISP_MML_SRAM_ONLY_1, ADDON_BETWEEN, DDP_COMPONENT_OVL4_2L},
};

static const struct mtk_addon_module_data mt6886_addon_mml_rsz_data[] = {
	{DISP_MML_IR_PQ_v3, ADDON_BETWEEN, DDP_COMPONENT_OVL1_2L},
};

static const struct mtk_addon_module_data mt6983_addon_mml_rsz_data[] = {
	{DISP_MML_IR_PQ, ADDON_BETWEEN, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data mt6983_addon_mml_rsz_data_1[] = {
	{DISP_MML_IR_PQ_1, ADDON_BETWEEN, DDP_COMPONENT_OVL2_2L},
};

static const struct mtk_addon_module_data mt6985_addon_mml_rsz_data[] = {
	{DISP_MML_IR_PQ_v2, ADDON_EMBED, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data mt6985_addon_mml_rsz_data_1[] = {
	{DISP_MML_IR_PQ_v2_1, ADDON_EMBED, DDP_COMPONENT_OVL4_2L},
};

static const struct mtk_addon_module_data mt6985_addon_mml_dl_data[] = {
	{DISP_MML_DL, ADDON_BEFORE, DDP_COMPONENT_OVL0_2L},
};

static const struct mtk_addon_module_data mt6985_addon_mml_dl_data_1[] = {
	{DISP_MML_DL_1, ADDON_BEFORE, DDP_COMPONENT_OVL4_2L},
};




static const struct mtk_addon_scenario_data mt6779_addon_main[ADDON_SCN_NR] = {
		[NONE] = {

				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {

				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {

				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6779_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6885_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v2),
				.module_data = addon_rsz_data_v2,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v2),
				.module_data = addon_rsz_data_v2,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(addon_wdma0_data),
				.module_data = addon_wdma0_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6885_addon_main_dual[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v3),
				.module_data = addon_rsz_data_v3,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v3),
				.module_data = addon_rsz_data_v3,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(addon_wdma1_data),
				.module_data = addon_wdma1_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};


static const struct mtk_addon_scenario_data mt6885_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v2),
				.module_data = addon_rsz_data_v2,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v2),
				.module_data = addon_rsz_data_v2,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(mt6983_addon_wdma0_data),
				.module_data = mt6983_addon_wdma0_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK_OVL] = {
				.module_num = ARRAY_SIZE(mt6983_addon_wdma0_data_v2),
				.module_data = mt6983_addon_wdma0_data_v2,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[MML_RSZ] = {
				.module_num = ARRAY_SIZE(mt6983_addon_mml_rsz_data),
				.module_data = mt6983_addon_mml_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[MML_SRAM_ONLY] = {
				.module_num = ARRAY_SIZE(addon_mml_sram_only_data),
				.module_data = addon_mml_sram_only_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
};

static const struct mtk_addon_scenario_data mt6983_addon_main_dual[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v4),
				.module_data = addon_rsz_data_v4,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v4),
				.module_data = addon_rsz_data_v4,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(mt6983_addon_wdma2_data),
				.module_data = mt6983_addon_wdma2_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK_OVL] = {
				.module_num = ARRAY_SIZE(mt6983_addon_wdma2_data_v2),
				.module_data = mt6983_addon_wdma2_data_v2,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[MML_RSZ] = {
				.module_num = ARRAY_SIZE(mt6983_addon_mml_rsz_data_1),
				.module_data = mt6983_addon_mml_rsz_data_1,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[MML_SRAM_ONLY] = {
				.module_num = ARRAY_SIZE(mt6983_addon_mml_sram_only_data_1),
				.module_data = mt6983_addon_mml_sram_only_data_1,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};


static const struct mtk_addon_scenario_data mt6983_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_main[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK] = {
		.module_num = ARRAY_SIZE(mt6985_addon_wdma0_data),
		.module_data = mt6985_addon_wdma0_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK_OVL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_ovlsys_wdma0_data),
		.module_data = mt6985_addon_ovlsys_wdma0_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
		.module_num = ARRAY_SIZE(addon_ovl_rsz_data),
		.module_data = addon_ovl_rsz_data,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[MML_RSZ] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_rsz_data),
		.module_data = mt6985_addon_mml_rsz_data,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[MML_SRAM_ONLY] = {
		.module_num = ARRAY_SIZE(addon_mml_sram_only_data),
		.module_data = addon_mml_sram_only_data,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[MML_DL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_dl_data),
		.module_data = mt6985_addon_mml_dl_data,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_main_dual[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK] = {
		.module_num = ARRAY_SIZE(mt6985_addon_wdma1_data),
		.module_data = mt6985_addon_wdma1_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK_OVL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_ovlsys_wdma2_data),
		.module_data = mt6985_addon_ovlsys_wdma2_data,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
		.module_num = ARRAY_SIZE(addon_ovl_rsz_data_1),
		.module_data = addon_ovl_rsz_data_1,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[MML_RSZ] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_rsz_data_1),
		.module_data = mt6985_addon_mml_rsz_data_1,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[MML_SRAM_ONLY] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_sram_only_data_1),
		.module_data = mt6985_addon_mml_sram_only_data_1,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[MML_DL] = {
		.module_num = ARRAY_SIZE(mt6985_addon_mml_dl_data_1),
		.module_data = mt6985_addon_mml_dl_data_1,
		.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6985_dual_data_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_dp_w_tdshp[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_dp_wo_tdshp[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_secondary_path[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_secondary_path_dual[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_discrete_path[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6985_addon_discrete_path_dual[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_secondary_path[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_secondary_path_dual[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_discrete_path[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6983_addon_discrete_path_dual[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6895_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v5),
				.module_data = addon_rsz_data_v5,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v5),
				.module_data = addon_rsz_data_v5,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(mt6895_addon_wdma0_data),
				.module_data = mt6895_addon_wdma0_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[MML_SRAM_ONLY] = {
				.module_num = ARRAY_SIZE(addon_mml_sram_only_data),
				.module_data = addon_mml_sram_only_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
};


static const struct mtk_addon_scenario_data mt6895_addon_main_dual[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v6),
				.module_data = addon_rsz_data_v6,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v6),
				.module_data = addon_rsz_data_v6,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(mt6895_addon_wdma2_data),
				.module_data = mt6895_addon_wdma2_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[MML_SRAM_ONLY] = {
				.module_num = ARRAY_SIZE(mt6895_addon_mml_sram_only_data_1),
				.module_data = mt6895_addon_mml_sram_only_data_1,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
};


static const struct mtk_addon_scenario_data mt6895_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6886_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_addon_scenario_data mt6886_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v5),
				.module_data = addon_rsz_data_v5,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data_v5),
				.module_data = addon_rsz_data_v5,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[WDMA_WRITE_BACK] = {
				.module_num = ARRAY_SIZE(mt6895_addon_wdma0_data),
				.module_data = mt6895_addon_wdma0_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[MML_SRAM_ONLY] = {
				.module_num = ARRAY_SIZE(mt6886_addon_mml_sram_only_data),
				.module_data = mt6886_addon_mml_sram_only_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[MML_RSZ] = {
				.module_num = ARRAY_SIZE(mt6886_addon_mml_rsz_data),
				.module_data = mt6886_addon_mml_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
};

static const enum mtk_ddp_comp_id mt6873_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6873_mtk_ddp_ext[] = {
#ifdef CONFIG_DRM_MEDIATEK_HDMI
	DDP_COMPONENT_OVL2_2L, DDP_COMPONENT_RDMA4,
	DDP_COMPONENT_DPI0,
#else
	DDP_COMPONENT_OVL2_2L, DDP_COMPONENT_RDMA4,
#endif
};

static const enum mtk_ddp_comp_id mt6873_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL2_2L, DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6873_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6873_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_DMDP_AAL0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6873_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA0,
};

static const struct mtk_addon_scenario_data mt6873_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6873_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};
static const enum mtk_ddp_comp_id mt6853_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_SPR0_VIRTUAL,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6853_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_WDMA0,
};
#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6853_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};
#endif
static const enum mtk_ddp_comp_id mt6853_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};
#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6853_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA0,
};
#endif
static const struct mtk_addon_scenario_data mt6853_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6853_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const enum mtk_ddp_comp_id mt6833_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6833_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_WDMA0,
};
#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6833_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L,       DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};
#endif
static const enum mtk_ddp_comp_id mt6833_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_COLOR0,   DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0,      DDP_COMPONENT_GAMMA0,
	DDP_COMPONENT_POSTMASK0, DDP_COMPONENT_DITHER0,
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};
#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6833_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0, DDP_COMPONENT_WDMA0,
};
#endif

static const struct mtk_addon_scenario_data mt6833_addon_main[ADDON_SCN_NR] = {
		[NONE] = {
				.module_num = 0,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
		[ONE_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_RPO_L0,
			},
		[TWO_SCALING] = {
				.module_num = ARRAY_SIZE(addon_rsz_data),
				.module_data = addon_rsz_data,
				.hrt_type = HRT_TB_TYPE_GENERAL1,
			},
};

static const struct mtk_addon_scenario_data mt6833_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const enum mtk_ddp_comp_id mt6879_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_MAIN_OVL_DISP_PQ0_VIRTUAL,
	DDP_COMPONENT_PQ0_RDMA0_POS_VIRTUAL, DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP0,
	DDP_COMPONENT_COLOR0, DDP_COMPONENT_CCORR0, DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,
	DDP_COMPONENT_AAL0, DDP_COMPONENT_GAMMA0, DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_DITHER0, DDP_COMPONENT_CM0, DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#endif
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
#ifndef DRM_BYPASS_PQ
	/* the chist connect by customer config*/
	DDP_COMPONENT_CHIST0,
#endif
};

static const enum mtk_ddp_comp_id mt6879_mtk_ddp_ext[] = {
	//todo: ovl0_2l_nwcg -> dp
};

static const enum mtk_ddp_comp_id mt6879_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL0_2L_NWCG,	DDP_COMPONENT_WDMA1,
};

#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6879_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL0_2L, DDP_COMPONENT_OVL0,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6879_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP0,
	DDP_COMPONENT_COLOR0, DDP_COMPONENT_CCORR0, DDP_COMPONENT_CCORR1,
	DDP_COMPONENT_C3D0,
	DDP_COMPONENT_AAL0, DDP_COMPONENT_GAMMA0, DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_DITHER0, DDP_COMPONENT_CM0, DDP_COMPONENT_SPR0,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#endif
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6879_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL0_2L,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL,
	DDP_COMPONENT_WDMA0,
};
#endif

static const struct mtk_addon_scenario_data mt6879_addon_main[ADDON_SCN_NR] = {
	[NONE] = {
			.module_num = 0,
			.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
			.module_num = ARRAY_SIZE(addon_rsz_data),
			.module_data = addon_rsz_data,
			.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[TWO_SCALING] = {
			.module_num = ARRAY_SIZE(addon_rsz_data),
			.module_data = addon_rsz_data,
			.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK] = {
			.module_num = ARRAY_SIZE(mt6983_addon_wdma0_data),
			.module_data = mt6983_addon_wdma0_data,
			.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
};

static const struct mtk_addon_scenario_data mt6879_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const enum mtk_ddp_comp_id mt6855_mtk_ddp_main[] = {
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL0, DDP_COMPONENT_MAIN_OVL_DISP_PQ0_VIRTUAL,
	DDP_COMPONENT_PQ0_RDMA0_POS_VIRTUAL, DDP_COMPONENT_RDMA0,
	DDP_COMPONENT_DSI0,
	DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6855_mtk_ddp_third[] = {
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL,
	DDP_COMPONENT_WDMA0,
};

#ifdef IF_ZERO
static const enum mtk_ddp_comp_id mt6855_mtk_ddp_main_minor[] = {
	DDP_COMPONENT_OVL1_2L, DDP_COMPONENT_OVL0,
	DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL,
	DDP_COMPONENT_WDMA0,
};

static const enum mtk_ddp_comp_id mt6855_mtk_ddp_main_minor_sub[] = {
	DDP_COMPONENT_RDMA0,
#ifndef DRM_BYPASS_PQ
	DDP_COMPONENT_TDSHP0,
	DDP_COMPONENT_COLOR0, DDP_COMPONENT_CCORR0,
	DDP_COMPONENT_AAL0, DDP_COMPONENT_GAMMA0, DDP_COMPONENT_POSTMASK0,
	DDP_COMPONENT_DITHER0, DDP_COMPONENT_SPR0_VIRTUAL,
	DDP_COMPONENT_PQ0_VIRTUAL, DDP_COMPONENT_MAIN0_VIRTUAL,
#endif
	DDP_COMPONENT_DSI0,     DDP_COMPONENT_PWM0,
};

static const enum mtk_ddp_comp_id mt6855_mtk_ddp_main_wb_path[] = {
	DDP_COMPONENT_OVL1_2L,
	DDP_COMPONENT_OVL0,
	DDP_COMPONENT_MAIN_OVL_DISP_WDMA_VIRTUAL,
	DDP_COMPONENT_WDMA0,
};
#endif

static const struct mtk_addon_scenario_data mt6855_addon_main[ADDON_SCN_NR] = {
	[NONE] = {
			.module_num = 0,
			.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[ONE_SCALING] = {
			.module_num = ARRAY_SIZE(mt6855_addon_rsz_data),
			.module_data = mt6855_addon_rsz_data,
			.hrt_type = HRT_TB_TYPE_RPO_L0,
	},
	[TWO_SCALING] = {
			.module_num = ARRAY_SIZE(mt6855_addon_rsz_data),
			.module_data = mt6855_addon_rsz_data,
			.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
	[WDMA_WRITE_BACK] = {
			.module_num = ARRAY_SIZE(mt6855_addon_wdma0_data),
			.module_data = mt6855_addon_wdma0_data,
			.hrt_type = HRT_TB_TYPE_GENERAL1,
	},
};

static const struct mtk_addon_scenario_data mt6855_addon_ext[ADDON_SCN_NR] = {
	[NONE] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
	[TRIPLE_DISP] = {
		.module_num = 0,
		.hrt_type = HRT_TB_TYPE_GENERAL0,
	},
};

static const struct mtk_crtc_path_data mt6779_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6779_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6779_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = mt6779_mtk_ddp_main_wb_path,
	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6779_mtk_ddp_main_wb_path),
	.path[DDP_MINOR][0] = mt6779_mtk_ddp_main_minor,
	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6779_mtk_ddp_main_minor),
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = mt6779_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6779_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6779_addon_main,
};

static const struct mtk_crtc_path_data mt6779_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6779_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6779_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6779_addon_ext,
};

static const struct mtk_crtc_path_data mt6779_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6779_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6779_mtk_ddp_third),
	.addon_data = mt6779_addon_ext,
};

static const struct mtk_crtc_path_data mt6885_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6885_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6885_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.dual_path[0] = mt6885_mtk_ddp_dual_main,
	.dual_path_len[0] = ARRAY_SIZE(mt6885_mtk_ddp_dual_main),
	.wb_path[DDP_MAJOR] = mt6885_mtk_ddp_main_wb_path,
	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6885_mtk_ddp_main_wb_path),
	.path[DDP_MINOR][0] = mt6885_mtk_ddp_main_minor,
	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6885_mtk_ddp_main_minor),
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = mt6885_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6885_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6885_addon_main,
	.addon_data_dual = mt6885_addon_main_dual,
};

static const struct mtk_crtc_path_data mt6885_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6885_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6885_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6885_addon_ext,
	.dual_path[0] = mt6885_dual_data_ext,
	.dual_path_len[0] = ARRAY_SIZE(mt6885_dual_data_ext),
};

static const struct mtk_crtc_path_data mt6885_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6885_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6885_mtk_ddp_third),
	.addon_data = mt6885_addon_ext,
};

static const struct mtk_crtc_path_data mt6983_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6983_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6983_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.dual_path[0] = mt6983_mtk_ddp_dual_main,
	.dual_path_len[0] = ARRAY_SIZE(mt6983_mtk_ddp_dual_main),
	.wb_path[DDP_MAJOR] = mt6983_mtk_ddp_main_wb_path,
	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6983_mtk_ddp_main_wb_path),
	.addon_data = mt6983_addon_main,
	.addon_data_dual = mt6983_addon_main_dual,
};

static const struct mtk_crtc_path_data mt6983_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6983_mtk_ddp_ext_dp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6983_mtk_ddp_ext_dp),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6983_addon_ext,
	.dual_path[0] = mt6983_dual_data_ext,
	.dual_path_len[0] = ARRAY_SIZE(mt6983_dual_data_ext),
};

static const struct mtk_crtc_path_data mt6983_mtk_ext_dsi_path_data = {
	.path[DDP_MAJOR][0] = mt6983_mtk_ddp_ext_dsi,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6983_mtk_ddp_ext_dsi),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6983_addon_ext,
	.dual_path[0] = mt6983_dual_data_ext,
	.dual_path_len[0] = ARRAY_SIZE(mt6983_dual_data_ext),
};

static const struct mtk_crtc_path_data mt6983_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6983_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6983_mtk_ddp_third),
	.addon_data = mt6983_addon_ext,
};

static const struct mtk_crtc_path_data mt6985_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6985_mtk_ddp_main_bringup,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_main_bringup),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.dual_path[0] = mt6985_mtk_ddp_dual_main_bringup,
	.dual_path_len[0] = ARRAY_SIZE(mt6985_mtk_ddp_dual_main_bringup),
//	.wb_path[DDP_MAJOR] = mt6983_mtk_ddp_main_wb_path,
//	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6983_mtk_ddp_main_wb_path),
	.addon_data = mt6985_addon_main,
	.addon_data_dual = mt6985_addon_main_dual,
};

static const struct mtk_crtc_path_data mt6985_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6985_mtk_ddp_ext_dp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_ext_dp),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.dual_path[0] = mt6985_mtk_ddp_dual_ext_dp,
	.dual_path_len[0] = ARRAY_SIZE(mt6985_mtk_ddp_dual_ext_dp),
	.addon_data = mt6985_addon_ext,
};

static const struct mtk_crtc_path_data mt6985_mtk_dp_w_tdshp_path_data = {
	.path[DDP_MAJOR][0] = mt6985_mtk_ddp_mem_dp_w_tdshp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_mem_dp_w_tdshp),
	.addon_data = mt6983_addon_dp_w_tdshp,
};

static const struct mtk_crtc_path_data mt6985_mtk_dp_wo_tdshp_path_data = {
	.path[DDP_MAJOR][0] = mt6985_mtk_ddp_mem_dp_wo_tdshp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_mem_dp_wo_tdshp),
	.addon_data = mt6983_addon_dp_wo_tdshp,
};

static const struct mtk_crtc_path_data mt6985_mtk_secondary_path_data = {
	.path[DDP_MAJOR][0] = mt6985_mtk_ddp_secondary_dp,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_secondary_dp),
//	.dual_path[0] = mt6985_mtk_ddp_dual_secondary_dp,
//	.dual_path_len[0] = ARRAY_SIZE(mt6985_mtk_ddp_dual_secondary_dp),
	.addon_data = mt6985_addon_secondary_path,
//	.addon_data_dual = mt6985_addon_secondary_path_dual,
};

static const struct mtk_crtc_path_data mt6985_mtk_discrete_path_data = {
	.path[DDP_MAJOR][0] = mt6985_mtk_ddp_discrete_chip,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6985_mtk_ddp_discrete_chip),
//	.dual_path[0] = mt6985_mtk_ddp_dual_discrete_chip,
//	.dual_path_len[0] = ARRAY_SIZE(mt6985_mtk_ddp_dual_discrete_chip),
	.addon_data = mt6983_addon_discrete_path,
//	.addon_data_dual = mt6985_addon_discrete_path_dual,
};

static const struct mtk_crtc_path_data mt6895_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6895_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6895_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.dual_path[0] = mt6895_mtk_ddp_dual_main,
	.dual_path_len[0] = ARRAY_SIZE(mt6895_mtk_ddp_dual_main),
	.wb_path[DDP_MAJOR] = mt6895_mtk_ddp_main_wb_path,
	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6895_mtk_ddp_main_wb_path),
	.addon_data = mt6895_addon_main,
	.addon_data_dual = mt6895_addon_main_dual,
};

static const struct mtk_crtc_path_data mt6895_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6895_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6895_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6895_addon_ext,
	.dual_path[0] = mt6895_dual_data_ext,
	.dual_path_len[0] = ARRAY_SIZE(mt6895_dual_data_ext),
};

static const struct mtk_crtc_path_data mt6886_mtk_ext_path_data = {
	.is_fake_path = true,
	.path[DDP_MAJOR][0] = mt6886_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6886_mtk_ddp_third),
	.addon_data = mt6886_addon_ext,
};

static const struct mtk_crtc_path_data mt6895_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6895_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6895_mtk_ddp_third),
	.addon_data = mt6895_addon_ext,
};

static const struct mtk_crtc_path_data mt6886_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6886_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6886_mtk_ddp_third),
	.addon_data = mt6886_addon_ext,
};

static const struct mtk_crtc_path_data mt6886_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6886_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6886_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = mt6895_mtk_ddp_main_wb_path,
	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6895_mtk_ddp_main_wb_path),
	.addon_data = mt6886_addon_main,
};

static const struct mtk_crtc_path_data mt2701_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt2701_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt2701_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt2701_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt2701_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt2701_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt2712_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt2712_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt2712_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt2712_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt2712_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt2712_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt8173_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt8173_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt8173_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt8173_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt8173_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt8173_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
};

static const struct mtk_crtc_path_data mt6873_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6873_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6873_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = mt6873_mtk_ddp_main_wb_path,
	.wb_path_len[DDP_MAJOR] = ARRAY_SIZE(mt6873_mtk_ddp_main_wb_path),
	.path[DDP_MINOR][0] = mt6873_mtk_ddp_main_minor,
	.path_len[DDP_MINOR][0] = ARRAY_SIZE(mt6873_mtk_ddp_main_minor),
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = mt6873_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6873_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6873_addon_main,
};

static const struct mtk_crtc_path_data mt6873_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6873_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6873_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6873_addon_ext,
};

static const struct mtk_crtc_path_data mt6873_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6873_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6873_mtk_ddp_third),
	.addon_data = mt6873_addon_ext,
};

static const struct mtk_crtc_path_data mt6853_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6853_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6853_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = mt6853_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6853_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6853_addon_main,
};

static const struct mtk_crtc_path_data mt6853_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6853_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6853_mtk_ddp_third),
	.addon_data = mt6853_addon_ext,
};

static const struct mtk_crtc_path_data mt6833_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6833_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6833_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
	.path[DDP_MINOR][1] = mt6833_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6833_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
	.addon_data = mt6833_addon_main,
};

static const struct mtk_crtc_path_data mt6833_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6833_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6833_mtk_ddp_third),
	.addon_data = mt6833_addon_ext,
};

static const struct mtk_crtc_path_data mt6879_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6879_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6879_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
#ifdef IF_ZERO
	.path[DDP_MINOR][1] = mt6879_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6879_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
#endif
	.addon_data = mt6879_addon_main,
};

static const struct mtk_crtc_path_data mt6879_mtk_ext_path_data = {
	.path[DDP_MAJOR][0] = mt6879_mtk_ddp_ext,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6879_mtk_ddp_ext),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.addon_data = mt6879_addon_ext,
};

static const struct mtk_crtc_path_data mt6879_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6879_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6879_mtk_ddp_third),
	.addon_data = mt6879_addon_ext,
};

static const struct mtk_crtc_path_data mt6855_mtk_main_path_data = {
	.path[DDP_MAJOR][0] = mt6855_mtk_ddp_main,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6855_mtk_ddp_main),
	.path_req_hrt[DDP_MAJOR][0] = true,
	.wb_path[DDP_MAJOR] = NULL,
	.wb_path_len[DDP_MAJOR] = 0,
	.path[DDP_MINOR][0] = NULL,
	.path_len[DDP_MINOR][0] = 0,
	.path_req_hrt[DDP_MINOR][0] = false,
#ifdef IF_ZERO
	.path[DDP_MINOR][1] = mt6855_mtk_ddp_main_minor_sub,
	.path_len[DDP_MINOR][1] = ARRAY_SIZE(mt6855_mtk_ddp_main_minor_sub),
	.path_req_hrt[DDP_MINOR][1] = true,
#endif
	.addon_data = mt6855_addon_main,
};

static const struct mtk_crtc_path_data mt6855_mtk_third_path_data = {
	.path[DDP_MAJOR][0] = mt6855_mtk_ddp_third,
	.path_len[DDP_MAJOR][0] = ARRAY_SIZE(mt6855_mtk_ddp_third),
	.addon_data = mt6855_addon_ext,
};

const struct mtk_session_mode_tb mt6779_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

static const struct mtk_fake_eng_reg mt6779_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 19, .share_port = false},
		{.CG_idx = 1, .CG_bit = 4, .share_port = false},
};

static const struct mtk_fake_eng_data mt6779_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6779_fake_eng_reg),
	.fake_eng_reg = mt6779_fake_eng_reg,
};

const struct mtk_session_mode_tb mt6885_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6983_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6985_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
};

/* mt6886 is the same as mt6895 */
const struct mtk_session_mode_tb mt6895_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6873_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};
const struct mtk_session_mode_tb mt6853_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6833_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6879_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {

				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {

				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {

				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

const struct mtk_session_mode_tb mt6855_mode_tb[MTK_DRM_SESSION_NUM] = {
		[MTK_DRM_SESSION_DL] = {
				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_NO_USE, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DOUBLE_DL] = {
				.en = 1,
				.ddp_mode = {DDP_MAJOR, DDP_MAJOR, DDP_MAJOR},
			},
		[MTK_DRM_SESSION_DC_MIRROR] = {
				.en = 1,
				.ddp_mode = {DDP_MINOR, DDP_MAJOR, DDP_NO_USE},
			},
		[MTK_DRM_SESSION_TRIPLE_DL] = {
				.en = 0,
				.ddp_mode = {DDP_MAJOR, DDP_MINOR, DDP_MAJOR},
			},
};

static const struct mtk_fake_eng_reg mt6885_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 14, .share_port = true},
		{.CG_idx = 0, .CG_bit = 15, .share_port = true},
};

static const struct mtk_fake_eng_data mt6885_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6885_fake_eng_reg),
	.fake_eng_reg = mt6885_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6983_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 14, .share_port = true},
		{.CG_idx = 0, .CG_bit = 15, .share_port = true},
};

static const struct mtk_fake_eng_data mt6983_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6983_fake_eng_reg),
	.fake_eng_reg = mt6983_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6985_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 14, .share_port = true},
		{.CG_idx = 0, .CG_bit = 15, .share_port = true},
};

static const struct mtk_fake_eng_data mt6985_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6985_fake_eng_reg),
	.fake_eng_reg = mt6985_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6895_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 3, .share_port = true},
		{.CG_idx = 0, .CG_bit = 6, .share_port = true},
};

/* mt6886 is the same as mt6895 */
static const struct mtk_fake_eng_data mt6895_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6895_fake_eng_reg),
	.fake_eng_reg = mt6895_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6873_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 18, .share_port = true},
		{.CG_idx = 0, .CG_bit = 19, .share_port = true},
};

static const struct mtk_fake_eng_data mt6873_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6873_fake_eng_reg),
	.fake_eng_reg = mt6873_fake_eng_reg,
};
static const struct mtk_fake_eng_reg mt6853_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 20, .share_port = true},
		{.CG_idx = 0, .CG_bit = 21, .share_port = true},
};

static const struct mtk_fake_eng_data mt6853_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6853_fake_eng_reg),
	.fake_eng_reg = mt6853_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6833_fake_eng_reg[] = {
		{.CG_idx = 0, .CG_bit = 20, .share_port = true},
		{.CG_idx = 0, .CG_bit = 21, .share_port = true},
};

static const struct mtk_fake_eng_data mt6833_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6833_fake_eng_reg),
	.fake_eng_reg = mt6833_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6879_fake_eng_reg[] = {
	{.CG_idx = 0, .CG_bit = 3, .share_port = false},
	{.CG_idx = 0, .CG_bit = 6, .share_port = false},
};

static const struct mtk_fake_eng_data mt6879_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6879_fake_eng_reg),
	.fake_eng_reg = mt6879_fake_eng_reg,
};

static const struct mtk_fake_eng_reg mt6855_fake_eng_reg[] = {
	{.CG_idx = 0, .CG_bit = 3, .share_port = false},
	{.CG_idx = 0, .CG_bit = 6, .share_port = false},
};

static const struct mtk_fake_eng_data mt6855_fake_eng_data = {
	.fake_eng_num =  ARRAY_SIZE(mt6855_fake_eng_reg),
	.fake_eng_reg = mt6855_fake_eng_reg,
};

static const struct mtk_mmsys_driver_data mt2701_mmsys_driver_data = {
	.main_path_data = &mt2701_mtk_main_path_data,
	.ext_path_data = &mt2701_mtk_ext_path_data,
	.mmsys_id = MMSYS_MT2701,
	.shadow_register = true,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = false,
};

static const struct mtk_mmsys_driver_data mt2712_mmsys_driver_data = {
	.main_path_data = &mt2712_mtk_main_path_data,
	.ext_path_data = &mt2712_mtk_ext_path_data,
	.mmsys_id = MMSYS_MT2712,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = false,
};

static const struct mtk_mmsys_driver_data mt8173_mmsys_driver_data = {
	.main_path_data = &mt8173_mtk_main_path_data,
	.ext_path_data = &mt8173_mtk_ext_path_data,
	.mmsys_id = MMSYS_MT8173,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = false,
};

static const struct mtk_mmsys_driver_data mt6779_mmsys_driver_data = {
	.main_path_data = &mt6779_mtk_main_path_data,
	.ext_path_data = &mt6779_mtk_ext_path_data,
	.third_path_data = &mt6779_mtk_third_path_data,
	.mmsys_id = MMSYS_MT6779,
	.mode_tb = mt6779_mode_tb,
	.sodi_config = mt6779_mtk_sodi_config,
	.fake_eng_data = &mt6779_fake_eng_data,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = false,
};

static const struct mtk_mmsys_driver_data mt6885_mmsys_driver_data = {
	.main_path_data = &mt6885_mtk_main_path_data,
	.ext_path_data = &mt6885_mtk_ext_path_data,
	.third_path_data = &mt6885_mtk_third_path_data,
	.fake_eng_data = &mt6885_fake_eng_data,
	.mmsys_id = MMSYS_MT6885,
	.mode_tb = mt6885_mode_tb,
	.sodi_config = mt6885_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = false,
};

static const struct mtk_mmsys_driver_data mt6983_mmsys_driver_data = {
	.main_path_data = &mt6983_mtk_main_path_data,
	.ext_path_data = &mt6983_mtk_ext_path_data,
	.ext_alter_path_data = &mt6983_mtk_ext_dsi_path_data,
	.third_path_data = &mt6983_mtk_third_path_data,
	.fourth_path_data_secondary = &mt6983_mtk_ext_dsi_path_data,
	.fake_eng_data = &mt6983_fake_eng_data,
	.mmsys_id = MMSYS_MT6983,
	.mode_tb = mt6983_mode_tb,
	.sodi_config = mt6983_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = false,
	.bypass_infra_ddr_control = true,
};

static const struct mtk_mmsys_driver_data mt6985_mmsys_driver_data = {
	.main_path_data = &mt6985_mtk_main_path_data,
	.ext_path_data = &mt6985_mtk_ext_path_data,
	.third_path_data = &mt6985_mtk_dp_w_tdshp_path_data,
	.third_path_data_wo_tdshp = &mt6985_mtk_dp_wo_tdshp_path_data,
	.fourth_path_data_secondary = &mt6985_mtk_secondary_path_data,
	.fourth_path_data_discrete = &mt6985_mtk_discrete_path_data,
	.fake_eng_data = &mt6985_fake_eng_data,
	.mmsys_id = MMSYS_MT6985,
	.mode_tb = mt6985_mode_tb,
	.sodi_config = mt6985_mtk_sodi_config,
	.sodi_apsrc_config = mt6985_mtk_sodi_apsrc_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = false,
	.bypass_infra_ddr_control = true,
	.disable_merge_irq = mtk_ddp_disable_merge_irq,
	.pf_ts_type = IRQ_CMDQ_CB,
};

static const struct mtk_mmsys_driver_data mt6895_mmsys_driver_data = {
	.main_path_data = &mt6895_mtk_main_path_data,
	.ext_path_data = &mt6895_mtk_ext_path_data,
	.third_path_data = &mt6895_mtk_third_path_data,
	.fake_eng_data = &mt6895_fake_eng_data,
	.mmsys_id = MMSYS_MT6895,
	.mode_tb = mt6895_mode_tb,
	.sodi_config = mt6895_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = true,
	.bypass_infra_ddr_control = true,
};

static const struct mtk_mmsys_driver_data mt6886_mmsys_driver_data = {
	.main_path_data = &mt6886_mtk_main_path_data,
	/*
	 * mt6886 not support dp path will use third path as ext_path_data
	 * to avoid crtc_id is 1 for third path
	 */
	.ext_path_data = &mt6886_mtk_ext_path_data,
	/* WFD path */
	.third_path_data = &mt6886_mtk_third_path_data,
	.fake_eng_data = &mt6895_fake_eng_data,
	.mmsys_id = MMSYS_MT6886,
	.mode_tb = mt6895_mode_tb,
	/* sodi is same as mt6895 */
	.sodi_config = mt6895_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = true,
	.bypass_infra_ddr_control = true,
};

static const struct mtk_mmsys_driver_data mt6873_mmsys_driver_data = {
	.main_path_data = &mt6873_mtk_main_path_data,
	.ext_path_data = &mt6873_mtk_ext_path_data,
	.third_path_data = &mt6873_mtk_third_path_data,
	.fake_eng_data = &mt6873_fake_eng_data,
	.mmsys_id = MMSYS_MT6873,
	.mode_tb = mt6873_mode_tb,
	.sodi_config = mt6873_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = true,
};

static const struct mtk_mmsys_driver_data mt6853_mmsys_driver_data = {
	.main_path_data = &mt6853_mtk_main_path_data,
	.ext_path_data = &mt6853_mtk_third_path_data,
	.third_path_data = &mt6853_mtk_third_path_data,
	.fake_eng_data = &mt6853_fake_eng_data,
	.mmsys_id = MMSYS_MT6853,
	.mode_tb = mt6853_mode_tb,
	.sodi_config = mt6853_mtk_sodi_config,
	.has_smi_limitation = true,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = true,
};

static const struct mtk_mmsys_driver_data mt6833_mmsys_driver_data = {
	.main_path_data = &mt6833_mtk_main_path_data,
	.ext_path_data = &mt6833_mtk_third_path_data,
	.third_path_data = &mt6833_mtk_third_path_data,
	.fake_eng_data = &mt6833_fake_eng_data,
	.mmsys_id = MMSYS_MT6833,
	.mode_tb = mt6833_mode_tb,
	.sodi_config = mt6833_mtk_sodi_config,
	.bypass_infra_ddr_control = true,
	.has_smi_limitation = true,
	.doze_ctrl_pmic = false,
	.can_compress_rgb565 = false,
};

static const struct mtk_mmsys_driver_data mt6879_mmsys_driver_data = {
	.main_path_data = &mt6879_mtk_main_path_data,
	.ext_path_data = &mt6879_mtk_third_path_data,
	.third_path_data = &mt6879_mtk_third_path_data,
	.fake_eng_data = &mt6879_fake_eng_data,
	.mmsys_id = MMSYS_MT6879,
	.mode_tb = mt6879_mode_tb,
	.sodi_config = mt6879_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = false,
	.bypass_infra_ddr_control = true,
};

static const struct mtk_mmsys_driver_data mt6855_mmsys_driver_data = {
	.main_path_data = &mt6855_mtk_main_path_data,
	.ext_path_data = &mt6855_mtk_third_path_data,
	.third_path_data = &mt6855_mtk_third_path_data,
	.fake_eng_data = &mt6855_fake_eng_data,
	.mmsys_id = MMSYS_MT6855,
	.mode_tb = mt6855_mode_tb,
	.sodi_config = mt6855_mtk_sodi_config,
	.has_smi_limitation = false,
	.doze_ctrl_pmic = true,
	.can_compress_rgb565 = false,
	.bypass_infra_ddr_control = true,
};

#ifdef MTK_DRM_FENCE_SUPPORT
void mtk_drm_suspend_release_present_fence(struct device *dev,
					   unsigned int index)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);

	mtk_release_present_fence(private->session_id[index],
				  atomic_read(&private->crtc_present[index]), 0);
}

void mtk_drm_suspend_release_sf_present_fence(struct device *dev,
					      unsigned int index)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);

	mtk_release_sf_present_fence(private->session_id[index],
			atomic_read(&private->crtc_sf_present[index]));
}

int mtk_drm_suspend_release_fence(struct device *dev)
{
	unsigned int i = 0;
	struct mtk_drm_private *private = dev_get_drvdata(dev);

	for (i = 0; i < MTK_TIMELINE_OUTPUT_TIMELINE_ID; i++) {
		DDPINFO("%s layerid=%d\n", __func__, i);
		mtk_release_layer_fence(private->session_id[0], i);
	}
	/* release present fence */
	mtk_drm_suspend_release_present_fence(dev, 0);
	mtk_drm_suspend_release_sf_present_fence(dev, 0);

	return 0;
}
#endif

/*---------------- function for repaint start ------------------*/
void drm_trigger_repaint(enum DRM_REPAINT_TYPE type, struct drm_device *drm_dev)
{
	if (type > DRM_WAIT_FOR_REPAINT && type < DRM_REPAINT_TYPE_NUM) {
		struct mtk_drm_private *pvd = drm_dev->dev_private;
		struct repaint_job_t *repaint_job;

		/* get a repaint_job_t from pool */
		spin_lock(&pvd->repaint_data.wq.lock);
		if (!list_empty(&pvd->repaint_data.job_pool)) {
			repaint_job =
				list_first_entry(&pvd->repaint_data.job_pool,
						 struct repaint_job_t, link);
			list_del_init(&repaint_job->link);
		} else { /* create repaint_job_t if pool is empty */
			repaint_job = kzalloc(sizeof(struct repaint_job_t),
					      GFP_ATOMIC);
			if (IS_ERR_OR_NULL(repaint_job)) {
				DDPPR_ERR("allocate repaint_job_t fail\n");
				spin_unlock(&pvd->repaint_data.wq.lock);
				return;
			}
			INIT_LIST_HEAD(&repaint_job->link);
			DDPINFO("[REPAINT] allocate a new repaint_job_t\n");
		}

		/* init & insert repaint_job_t into queue */
		repaint_job->type = (unsigned int)type;
		list_add_tail(&repaint_job->link, &pvd->repaint_data.job_queue);

		DDPINFO("[REPAINT] insert repaint_job in queue, type:%u\n",
			type);
		spin_unlock(&pvd->repaint_data.wq.lock);
		wake_up_interruptible(&pvd->repaint_data.wq);
	}
}

int mtk_drm_wait_repaint_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file_priv)
{
	int ret = 0;
	unsigned int *type = data;
	struct repaint_job_t *repaint_job;
	struct mtk_drm_private *pvd = dev->dev_private;

	/* reset status & wake-up threads which wait for repainting */
	DDPINFO("[REPAINT] HWC waits for repaint\n");

	/*  wait for repaint */
	ret = wait_event_interruptible(
		pvd->repaint_data.wq,
		!list_empty(&pvd->repaint_data.job_queue));

	if (ret >= 0) {
		/* retrieve first repaint_job_t from queue */
		spin_lock(&pvd->repaint_data.wq.lock);
		repaint_job = list_first_entry(&pvd->repaint_data.job_queue,
					       struct repaint_job_t, link);

		*type = repaint_job->type;

		/* remove from queue & add repaint_job_t in pool */
		list_del_init(&repaint_job->link);
		list_add_tail(&repaint_job->link, &pvd->repaint_data.job_pool);
		spin_unlock(&pvd->repaint_data.wq.lock);
		DDPINFO("[REPAINT] trigger repaint, type: %u\n", *type);
	} else {
		*type = DRM_WAIT_FOR_REPAINT;
		DDPINFO("[REPAINT] interrupted unexpected\n");
	}

	return 0;
}
/*---------------- function for repaint end ------------------*/
static void mtk_drm_get_top_clk(struct mtk_drm_private *priv)
{
	struct device *dev = priv->mmsys_dev;
	struct device_node *node = dev->of_node;
	struct clk *clk;
	int ret, i;

	if (disp_helper_get_stage() != DISP_HELPER_STAGE_NORMAL) {
		spin_lock_init(&top_clk_lock);
		/* TODO: check display enable from lk */
		atomic_set(&top_isr_ref, 0);
		atomic_set(&top_clk_ref, 1);
		priv->power_state = true;
	}

	if (of_property_read_u32(node, "clock-num", &priv->top_clk_num)) {
		priv->top_clk_num = -1;
		priv->top_clk = NULL;
		DDPINFO("%s, no clock-num\n", __func__);
		return;
	}

	priv->top_clk = devm_kmalloc_array(dev, priv->top_clk_num,
					   sizeof(*priv->top_clk), GFP_KERNEL);

	pm_runtime_get_sync(dev);
	if (priv->side_mmsys_dev)
		pm_runtime_get_sync(priv->side_mmsys_dev);
	if (priv->ovlsys_dev)
		pm_runtime_get_sync(priv->ovlsys_dev);
	if (priv->side_ovlsys_dev)
		pm_runtime_get_sync(priv->side_ovlsys_dev);
	for (i = 0; i < priv->top_clk_num; i++) {
		clk = of_clk_get(node, i);

		if (IS_ERR(clk)) {
			DDPPR_ERR("%s get %d clk failed\n", __func__, i);
			priv->top_clk_num = -1;
			return;
		}

		priv->top_clk[i] = clk;
		/* TODO: check display enable from lk */
		/* Because of align lk hw power status,
		 * we power on mtcmos at the beginning of
		 * the display initialization.
		 * We will power off mtcmos at the end of
		 * the display initialization.
		 */
		ret = clk_prepare_enable(priv->top_clk[i]);
		if (ret)
			DDPPR_ERR("top clk prepare enable failed:%d\n", i);
	}

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		spin_lock_init(&top_clk_lock);
		/* TODO: check display enable from lk */
		atomic_set(&top_isr_ref, 0);
		atomic_set(&top_clk_ref, 1);
		priv->power_state = true;
	}
}

void mtk_drm_top_clk_prepare_enable(struct drm_device *drm)
{
	struct mtk_drm_private *priv = drm->dev_private;
	int i;
	bool en = 1;
	int ret;
	unsigned long flags = 0;

	if (priv->top_clk_num <= 0)
		return;

	//set_swpm_disp_active(true);
	pm_runtime_get_sync(priv->mmsys_dev);
	if (priv->side_mmsys_dev)
		pm_runtime_get_sync(priv->side_mmsys_dev);
	if (priv->ovlsys_dev)
		pm_runtime_get_sync(priv->ovlsys_dev);
	if (priv->side_ovlsys_dev)
		pm_runtime_get_sync(priv->side_ovlsys_dev);
	for (i = 0; i < priv->top_clk_num; i++) {
		if (IS_ERR(priv->top_clk[i])) {
			DDPPR_ERR("%s invalid %d clk\n", __func__, i);
			return;
		}
		ret = clk_prepare_enable(priv->top_clk[i]);
		if (ret)
			DDPPR_ERR("top clk prepare enable failed:%d\n", i);
	}

	spin_lock_irqsave(&top_clk_lock, flags);
	atomic_inc(&top_clk_ref);
	if (atomic_read(&top_clk_ref) == 1)
		priv->power_state = true;

	spin_unlock_irqrestore(&top_clk_lock, flags);

	DRM_MMP_MARK(top_clk, atomic_read(&top_clk_ref),
			atomic_read(&top_isr_ref));
	if (priv->data->sodi_config)
		priv->data->sodi_config(drm, DDP_COMPONENT_ID_MAX, NULL, &en);

	if (priv->data->disable_merge_irq)
		priv->data->disable_merge_irq(drm);
}

void mtk_drm_top_clk_disable_unprepare(struct drm_device *drm)
{
	struct mtk_drm_private *priv = drm->dev_private;
	int i = 0, cnt = 0;
	unsigned long flags = 0;

	if (priv->top_clk_num <= 0)
		return;

	//set_swpm_disp_active(false);
	spin_lock_irqsave(&top_clk_lock, flags);
	atomic_dec(&top_clk_ref);
	if (atomic_read(&top_clk_ref) == 0) {
		while (atomic_read(&top_isr_ref) > 0 &&
		       cnt++ < 10) {
			spin_unlock_irqrestore(&top_clk_lock, flags);
			pr_notice("%s waiting for isr job, %d\n",
				  __func__, cnt);
			usleep_range(20, 40);
			spin_lock_irqsave(&top_clk_lock, flags);
		}
		priv->power_state = false;
	}
	spin_unlock_irqrestore(&top_clk_lock, flags);

	for (i = priv->top_clk_num - 1; i >= 0; i--) {
		if (IS_ERR(priv->top_clk[i])) {
			DDPPR_ERR("%s invalid %d clk\n", __func__, i);
			return;
		}
		clk_disable_unprepare(priv->top_clk[i]);
	}

	if (priv->ovlsys_dev)
		pm_runtime_put_sync(priv->ovlsys_dev);
	if (priv->side_ovlsys_dev)
		pm_runtime_put_sync(priv->side_ovlsys_dev);
	pm_runtime_put_sync(priv->mmsys_dev);
	if (priv->side_mmsys_dev)
		pm_runtime_put_sync(priv->side_mmsys_dev);
	DRM_MMP_MARK(top_clk, atomic_read(&top_clk_ref),
			atomic_read(&top_isr_ref));
}

bool mtk_drm_top_clk_isr_get(char *master)
{
	unsigned long flags = 0;

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		spin_lock_irqsave(&top_clk_lock, flags);
		if (atomic_read(&top_clk_ref) <= 0) {
			DDPPR_ERR("%s, top clk off at %s\n",
				  __func__, master ? master : "NULL");
			spin_unlock_irqrestore(&top_clk_lock, flags);
			return false;
		}
		atomic_inc(&top_isr_ref);
		spin_unlock_irqrestore(&top_clk_lock, flags);
	}
	return true;
}

void mtk_drm_top_clk_isr_put(char *master)
{
	unsigned long flags = 0;

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {

		spin_lock_irqsave(&top_clk_lock, flags);

		/* when timeout of polling isr ref in unpreare top clk*/
		if (atomic_read(&top_clk_ref) <= 0)
			DDPPR_ERR("%s, top clk off at %s\n",
				  __func__, master ? master : "NULL");

		atomic_dec(&top_isr_ref);
		spin_unlock_irqrestore(&top_clk_lock, flags);
	}
}

static void mtk_drm_first_enable(struct drm_device *drm)
{
	struct drm_crtc *crtc;

	drm_for_each_crtc(crtc, drm) {
		if (mtk_drm_is_enable_from_lk(crtc))
			mtk_drm_crtc_first_enable(crtc);
		else
			mtk_drm_crtc_init_para(crtc);
		lcm_fps_ctx_init(crtc);
	}
}

bool mtk_drm_session_mode_is_decouple_mode(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;

	if (priv->session_mode == MTK_DRM_SESSION_DC_MIRROR)
		return true;

	return false;
}

bool mtk_drm_session_mode_is_mirror_mode(struct drm_device *dev)
{
	struct mtk_drm_private *priv = dev->dev_private;

	if (priv->session_mode == MTK_DRM_SESSION_DC_MIRROR)
		return true;

	return false;
}

struct mtk_panel_params *mtk_drm_get_lcm_ext_params(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_ext *panel_ext = NULL;
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp)
		mtk_ddp_comp_io_cmd(comp, NULL, REQ_PANEL_EXT, &panel_ext);

	if (!panel_ext || !panel_ext->params)
		return NULL;

	return panel_ext->params;
}

struct mtk_panel_funcs *mtk_drm_get_lcm_ext_funcs(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_panel_ext *panel_ext = NULL;
	struct mtk_ddp_comp *comp;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp)
		mtk_ddp_comp_io_cmd(comp, NULL, REQ_PANEL_EXT, &panel_ext);

	if (!panel_ext || !panel_ext->funcs)
		return NULL;

	return panel_ext->funcs;
}

int mtk_drm_get_display_caps_ioctl(struct drm_device *dev, void *data,
				   struct drm_file *file_priv)
{
	int ret = 0;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_drm_disp_caps_info *caps_info = data;
	struct mtk_panel_params *params =
		mtk_drm_get_lcm_ext_params(private->crtc[0]);
	struct mtk_ddp_comp *ddp_comp;

	memset(caps_info, 0, sizeof(*caps_info));

	caps_info->hw_ver = private->data->mmsys_id;
	caps_info->disp_feature_flag |= DRM_DISP_FEATURE_DISP_SELF_REFRESH;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_HRT))
		caps_info->disp_feature_flag |= DRM_DISP_FEATURE_HRT;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_RPO)) {
		caps_info->disp_feature_flag |= DRM_DISP_FEATURE_RPO;
		caps_info->rsz_in_max[0] = private->rsz_in_max[0];
		caps_info->rsz_in_max[1] = private->rsz_in_max[1];
	}
	caps_info->disp_feature_flag |= DRM_DISP_FEATURE_FBDC;

#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION_HW
	caps_info->lcm_degree = 180;
#endif

	caps_info->lcm_color_mode = MTK_DRM_COLOR_MODE_NATIVE;
	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_OVL_WCG)) {
		if (params)
			caps_info->lcm_color_mode = params->lcm_color_mode;
		else
			DDPPR_ERR("%s: failed to get lcm color mode\n",
				  __func__);
	}

	if (params) {
		caps_info->min_luminance = params->min_luminance;
		caps_info->average_luminance = params->average_luminance;
		caps_info->max_luminance = params->max_luminance;
	} else
		DDPPR_ERR("%s: failed to get lcm luminance\n", __func__);

#ifdef DRM_MMPATH
	private->HWC_gpid = task_tgid_nr(current);
#endif

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_SF_PF) &&
	    !mtk_crtc_is_frame_trigger_mode(private->crtc[0]))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_SF_PRESENT_FENCE;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_PQ_34_COLOR_MATRIX))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_PQ_34_COLOR_MATRIX;
	/* Msync 2.0
	 * according to panel
	 */
	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_MSYNC2_0)) {
		if (params && params->msync2_enable) {
			caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_MSYNC2_0;
			caps_info->msync_level_num =
				params->msync_cmd_table.msync_level_num;
		}
	}

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_MML_PRIMARY))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_MML_PRIMARY;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_VIRTUAL_DISP))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_VIRUTAL_DISPLAY;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_USE_M4U))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_IOMMU;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_OVL_BW_MONITOR))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_OVL_BW_MONITOR;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_GPU_CACHE))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_GPU_CACHE;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_SPHRT))
		caps_info->disp_feature_flag |=
				DRM_DISP_FEATURE_SPHRT;

	ddp_comp = private->ddp_comp[DDP_COMPONENT_CHIST0];
	if (ddp_comp) {
		struct mtk_disp_chist *chist_data = comp_to_chist(ddp_comp);

		if (chist_data && chist_data->data) {
			caps_info->color_format = chist_data->data->color_format;
			caps_info->max_channel = chist_data->data->max_channel;
			caps_info->max_bin = chist_data->data->max_bin;
		}
	}
	return ret;
}

/* runtime calculate vsync fps*/
int lcm_fps_ctx_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	unsigned int index;

	if (!crtc || crtc->index >= MAX_CRTC) {
		DDPPR_ERR("%s:invalid crtc or invalid index :%d\n",
			  __func__, crtc ? crtc->index : -EINVAL);
		return -EINVAL;
	}
	index = crtc->index;

	if (atomic_read(&lcm_fps_ctx[index].is_inited))
		return 0;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s:CRTC:%d invalid output comp\n",
			  __func__, index);
		return -EINVAL;
	}

	memset(&lcm_fps_ctx[index], 0, sizeof(struct lcm_fps_ctx_t));
	spin_lock_init(&lcm_fps_ctx[index].lock);
	atomic_set(&lcm_fps_ctx[index].skip_update, 0);
	if ((mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI) &&
			!mtk_dsi_is_cmd_mode(output_comp))
		lcm_fps_ctx[index].dsi_mode = 1;
	else
		lcm_fps_ctx[index].dsi_mode = 0;
	atomic_set(&lcm_fps_ctx[index].is_inited, 1);

	DDPINFO("%s CRTC:%d done\n", __func__, index);
	return 0;
}

int lcm_fps_ctx_reset(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	unsigned long flags = 0;
	unsigned int index;

	if (!crtc || crtc->index >= MAX_CRTC)
		return -EINVAL;
	index = crtc->index;

	if (!atomic_read(&lcm_fps_ctx[index].is_inited))
		return 0;

	if (atomic_read(&lcm_fps_ctx[index].skip_update))
		return 0;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (unlikely(!output_comp)) {
		DDPPR_ERR("%s:CRTC:%d invalid output comp\n",
			  __func__, index);
		return -EINVAL;
	}

	spin_lock_irqsave(&lcm_fps_ctx[index].lock, flags);
	if (!mtk_dsi_is_cmd_mode(output_comp))
		lcm_fps_ctx[index].dsi_mode = 1;
	else
		lcm_fps_ctx[index].dsi_mode = 0;
	lcm_fps_ctx[index].head_idx = 0;
	lcm_fps_ctx[index].num = 0;
	lcm_fps_ctx[index].last_ns = 0;
	memset(&lcm_fps_ctx[index].array, 0, sizeof(lcm_fps_ctx[index].array));
	atomic_set(&lcm_fps_ctx[index].skip_update, 0);
	spin_unlock_irqrestore(&lcm_fps_ctx[index].lock, flags);

	DDPINFO("%s CRTC:%d done\n", __func__, index);

	return 0;
}

int lcm_fps_ctx_update(unsigned long long cur_ns,
		unsigned int crtc_id, unsigned int mode)
{
	unsigned int idx, index = crtc_id;
	unsigned long long delta;
	unsigned long flags = 0;

	if (index >= MAX_CRTC)
		return -EINVAL;

	if (!atomic_read(&lcm_fps_ctx[index].is_inited))
		return 0;

	if (lcm_fps_ctx[index].dsi_mode != mode)
		return 0;

	if (atomic_read(&lcm_fps_ctx[index].skip_update))
		return 0;

	spin_lock_irqsave(&lcm_fps_ctx[index].lock, flags);

	delta = cur_ns - lcm_fps_ctx[index].last_ns;
	if (delta == 0 || lcm_fps_ctx[index].last_ns == 0) {
		lcm_fps_ctx[index].last_ns = cur_ns;
		spin_unlock_irqrestore(&lcm_fps_ctx[index].lock, flags);
		return 0;
	}

	idx = (lcm_fps_ctx[index].head_idx +
		lcm_fps_ctx[index].num) % LCM_FPS_ARRAY_SIZE;
	lcm_fps_ctx[index].array[idx] = delta;

	if (lcm_fps_ctx[index].num < LCM_FPS_ARRAY_SIZE)
		lcm_fps_ctx[index].num++;
	else
		lcm_fps_ctx[index].head_idx = (lcm_fps_ctx[index].head_idx +
			 1) % LCM_FPS_ARRAY_SIZE;

	lcm_fps_ctx[index].last_ns = cur_ns;

	spin_unlock_irqrestore(&lcm_fps_ctx[index].lock, flags);

	/* DDPINFO("%s CRTC:%d update %lld to index %d\n",
	 *	__func__, index, delta, idx);
	 */

	return 0;
}

unsigned int lcm_fps_ctx_get(unsigned int crtc_id)
{
	unsigned int i;
	unsigned long long duration_avg = 0;
	unsigned long long duration_min = (1ULL << 63) - 1ULL;
	unsigned long long duration_max = 0;
	unsigned long long duration_sum = 0;
	unsigned long long fps = 100000000000;
	unsigned long flags = 0;
	unsigned int index = crtc_id;
	unsigned int fps_num = 0;
	unsigned long long fps_array[LCM_FPS_ARRAY_SIZE] = {0};
	unsigned long long start_time = 0, diff = 0;

	if (crtc_id >= MAX_CRTC) {
		DDPPR_ERR("%s:invalid crtc:%u\n",
			  __func__, crtc_id);
		return 0;
	}

	if (!atomic_read(&lcm_fps_ctx[index].is_inited))
		return 0;

	start_time = sched_clock();
	spin_lock_irqsave(&lcm_fps_ctx[index].lock, flags);

	if (atomic_read(&lcm_fps_ctx[index].skip_update) &&
	    lcm_fps_ctx[index].fps) {
		spin_unlock_irqrestore(&lcm_fps_ctx[index].lock, flags);
		return lcm_fps_ctx[index].fps;
	}

	if (lcm_fps_ctx[index].num <= 6) {
		unsigned int ret = (lcm_fps_ctx[index].fps ?
			lcm_fps_ctx[index].fps : 0);

		DDPINFO("%s CRTC:%d num %d < 6, return fps:%u\n",
			__func__, index, lcm_fps_ctx[index].num, ret);
		spin_unlock_irqrestore(&lcm_fps_ctx[index].lock, flags);
		return ret;
	}

	fps_num = lcm_fps_ctx[index].num;
	memcpy(fps_array, lcm_fps_ctx[index].array, sizeof(fps_array));

	spin_unlock_irqrestore(&lcm_fps_ctx[index].lock, flags);
	diff = sched_clock() - start_time;
	if (diff > 1000000)
		DDPMSG("%s diff = %llu, > 1 ms\n", __func__, diff);

	for (i = 0; i < fps_num; i++) {
		duration_sum += fps_array[i];
		duration_min = min(duration_min, fps_array[i]);
		duration_max = max(duration_max, fps_array[i]);
	}
	duration_sum -= duration_min + duration_max;
	duration_avg = duration_sum / (fps_num - 2);
	do_div(fps, duration_avg);
	lcm_fps_ctx[index].fps = (unsigned int)fps;

	DDPMSG("%s CRTC:%d max=%lld, min=%lld, sum=%lld, num=%d, fps=%u\n",
		__func__, index, duration_max, duration_min,
		duration_sum, fps_num, (unsigned int)fps);

	if (fps_num >= LCM_FPS_ARRAY_SIZE) {
		atomic_set(&lcm_fps_ctx[index].skip_update, 1);
		DDPINFO("%s set skip_update\n", __func__);
	}

	return (unsigned int)fps;
}

#ifdef DRM_OVL_SELF_PATTERN
static struct mtk_ddp_comp *mtk_drm_disp_test_get_top_plane(struct mtk_drm_crtc *mtk_crtc)
{
	int i, j, type;
	int lay_num;
	struct drm_plane *plane;
	struct mtk_plane_state *plane_state;
	struct mtk_plane_comp_state comp_state;
	struct mtk_ddp_comp *ovl_comp = NULL;
	struct mtk_ddp_comp *comp;

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		type = mtk_ddp_comp_get_type(comp->id);
		if (type == MTK_DISP_OVL)
			ovl_comp = comp;
		else if (type == MTK_DISP_RDMA)
			break;
	}

	if (ovl_comp == NULL) {
		DDPPR_ERR("No proper ovl module in CRTC %s\n",
			  mtk_crtc->base.name);
		return NULL;
	}

	lay_num = mtk_ovl_layer_num(ovl_comp);
	if (lay_num < 0) {
		DDPPR_ERR("invalid layer number:%d\n", lay_num);
		return NULL;
	}

	for (i = 0 ; i < mtk_crtc->layer_nr; ++i) {
		plane = &mtk_crtc->planes[i].base;
		plane_state = to_mtk_plane_state(plane->state);
		comp_state = plane_state->comp_state;

		/*find been cascaded ovl comp */
		if (comp_state.comp_id == ovl_comp->id &&
		    comp_state.lye_id == lay_num - 1) {
			plane_state->pending.enable = false;
			plane_state->pending.dirty = 1;
		}
	}

	return ovl_comp;
}

void mtk_drm_disp_test_init(struct drm_device *dev)
{
	struct mtk_drm_gem_obj *mtk_gem;
	struct drm_crtc *crtc;
	u32 width, height, size;
	unsigned int i, j;

	DDPINFO("%s+\n", __func__);

	crtc = list_first_entry(&(dev)->mode_config.crtc_list,
		typeof(*crtc), head);

	width = crtc->mode.hdisplay;
	height = crtc->mode.vdisplay;
	size = ALIGN_TO(width, 32) * height * 4;

	mtk_gem = mtk_drm_gem_create(dev, size, true);
	if (IS_ERR(mtk_gem)) {
		DDPINFO("alloc buffer fail\n");
		drm_gem_object_release(&mtk_gem->base);
		return;
	}

	//Avoid kmemleak to scan
	kmemleak_ignore(mtk_gem);

	test_va = mtk_gem->kvaddr;
	test_pa = mtk_gem->dma_addr;

	if (!test_pa || !test_va) {
		DDPPR_ERR("init DAL without proper buffer\n");
		return;
	}
	DDPINFO("%s[%d] test_va=0x%llx, test_pa=0x%llx\n",
		__func__, __LINE__, test_va, test_pa);

	for (i = 0; i < height; i++)
		for (j = 0; j < width; j++) {
			int x = (i * ALIGN_TO(width, 32) + j) * 4;

			*((unsigned char *)test_va + x++) = (i + j) % 256;
			*((unsigned char *)test_va + x++) = (i + j) % 256;
			*((unsigned char *)test_va + x++) = (i + j) % 256;
			*((unsigned char *)test_va + x++) = 255;
		}

	test_crtc = crtc;
}

static struct mtk_plane_state *mtk_drm_disp_test_set_plane_state(struct drm_crtc *crtc,
						       bool enable)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct drm_plane *plane;
	struct mtk_plane_state *plane_state;
	struct mtk_plane_pending_state *pending;
	struct mtk_ddp_comp *ovl_comp = mtk_drm_disp_test_get_top_plane(mtk_crtc);
	int layer_id = -1;

	if (ovl_comp)
		layer_id = mtk_ovl_layer_num(ovl_comp) - 1;

	if (layer_id < 0) {
		DDPPR_ERR("%s invalid layer id:%d\n", __func__, layer_id);
		return NULL;
	}

	plane = &mtk_crtc->planes[mtk_crtc->layer_nr - 1].base;
	plane_state = to_mtk_plane_state(plane->state);
	pending = &plane_state->pending;

	plane_state->pending.addr = test_pa;
	plane_state->pending.pitch = ALIGN_TO(crtc->mode.hdisplay, 32) * 4;
	plane_state->pending.format = DRM_FORMAT_ARGB8888;
	plane_state->pending.src_x = 0;
	plane_state->pending.src_y = 0;
	plane_state->pending.dst_x = 0;
	plane_state->pending.dst_y = 0;
	plane_state->pending.height = crtc->mode.vdisplay;
	plane_state->pending.width = crtc->mode.hdisplay;
	plane_state->pending.config = 1;
	plane_state->pending.dirty = 1;
	plane_state->pending.enable = !!enable;
	plane_state->pending.is_sec = false;

	//pending->prop_val[PLANE_PROP_ALPHA_CON] = 0x1;
	//pending->prop_val[PLANE_PROP_PLANE_ALPHA] = 0x80;
	pending->prop_val[PLANE_PROP_COMPRESS] = 0;

	plane_state->comp_state.comp_id = ovl_comp->id;
	plane_state->comp_state.lye_id = layer_id;
	plane_state->comp_state.ext_lye_id = 0;
	plane_state->base.crtc = crtc;

	return plane_state;
}

int mtk_drm_disp_test_show(struct drm_crtc *crtc, bool enable)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_plane_state *plane_state;
	struct mtk_ddp_comp *ovl_comp = mtk_drm_disp_test_get_top_plane(mtk_crtc);
	struct cmdq_pkt *cmdq_handle;
	int layer_id;

	DDPINFO("%s+\n", __func__);

	if (ovl_comp == NULL) {
		DDPPR_ERR("%s: can't find ovl comp\n", __func__);
		return 0;
	}
	layer_id = mtk_ovl_layer_num(ovl_comp) - 1;
	if (layer_id < 0) {
		DDPPR_ERR("%s invalid layer id:%d\n", __func__, layer_id);
		return 0;
	}

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	if (!mtk_crtc->enabled) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	plane_state = mtk_drm_disp_test_set_plane_state(crtc, enable);
	if (!plane_state) {
		DDPPR_ERR("%s: can't set dal plane_state\n", __func__);
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	mtk_drm_idlemgr_kick(__func__, crtc, 0);

	cmdq_handle = mtk_crtc_gce_commit_begin(crtc, NULL, NULL, false);

	if (mtk_crtc->is_dual_pipe) {
		struct mtk_drm_private *priv = mtk_crtc->base.dev->dev_private;
		struct mtk_plane_state plane_state_l;
		struct mtk_plane_state plane_state_r;
		struct mtk_ddp_comp *comp;

		if (plane_state->comp_state.comp_id == 0)
			plane_state->comp_state.comp_id = ovl_comp->id;

		mtk_drm_layer_dispatch_to_dual_pipe(priv->data->mmsys_id, plane_state,
			&plane_state_l, &plane_state_r,
			crtc->state->adjusted_mode.hdisplay);

		comp = priv->ddp_comp[plane_state_r.comp_state.comp_id];
		mtk_ddp_comp_layer_config(comp, layer_id,
					&plane_state_r, cmdq_handle);
		DDPINFO("%s+ comp_id:%d, comp_id:%d\n",
			__func__, comp->id,
			plane_state_r.comp_state.comp_id);

		mtk_ddp_comp_layer_config(ovl_comp, layer_id, &plane_state_l,
					  cmdq_handle);
	} else {
		mtk_ddp_comp_layer_config(ovl_comp, layer_id, plane_state, cmdq_handle);
	}

#ifndef DRM_CMDQ_DISABLE
	mtk_crtc_gce_flush(crtc, NULL, cmdq_handle, cmdq_handle);
#endif

#ifdef MTK_DRM_CMDQ_ASYNC
	cmdq_pkt_wait_complete(cmdq_handle);
#endif

	cmdq_pkt_destroy(cmdq_handle);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

#ifdef DRM_CMDQ_DISABLE
	DDPMSG("%s: trigger without cmdq\n", __func__);
	trigger_without_cmdq(crtc);
	// double check display trigger works fine
	trigger_without_cmdq(crtc);
	trigger_without_cmdq(crtc);
#else
	DDPMSG("%s: trigger cmdq\n", __func__);
	mtk_crtc_hw_block_ready(crtc);
	mtk_crtc_set_dirty(mtk_crtc);
#endif

	mtk_drm_crtc_analysis(crtc);
	mtk_drm_crtc_dump(crtc);

	DDPINFO("%s-\n", __func__);
	return 0;
}
#endif

int _parse_tag_videolfb(unsigned int *vramsize, phys_addr_t *fb_base,
			unsigned int *fps)
{
#ifndef CONFIG_MTK_DISP_NO_LK
	struct device_node *chosen_node;

	*fps = 6000;
	chosen_node = of_find_node_by_path("/chosen");

	if (chosen_node) {
		struct tag_videolfb *videolfb_tag = NULL;
		unsigned long size = 0;

		videolfb_tag = (struct tag_videolfb *)of_get_property(
			chosen_node, "atag,videolfb",
			(int *)&size);
		if (videolfb_tag) {
			*vramsize = videolfb_tag->vram;
			*fb_base = videolfb_tag->fb_base;
			*fps = videolfb_tag->fps;
			if (*fps == 0)
				*fps = 6000;
			goto found;
		} else {
			DDPINFO("[DT][videolfb] videolfb_tag not found\n");
			return -1;
		}
	} else {
		DDPINFO("[DT][videolfb] of_chosen not found\n");
		return -1;
	}

found:
	DDPINFO("[DT][videolfb] fb_base    = 0x%lx\n", (unsigned long)*fb_base);
	DDPINFO("[DT][videolfb] vram       = 0x%x (%d)\n", *vramsize,
		*vramsize);
	DDPINFO("[DT][videolfb] fps	   = %d\n", *fps);

	return 0;
#else
	return -1;
#endif
}

int mtk_drm_get_panel_info(struct drm_device *dev,
			     struct drm_mtk_session_info *info, unsigned int crtc_id)
{
	int ret = 0;
	unsigned int vramsize = 0, fps = 0;
	phys_addr_t fb_base = 0;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_panel_params *params =
		mtk_drm_get_lcm_ext_params(private->crtc[crtc_id]);

	if (params) {
		info->physicalWidthUm = params->physical_width_um;
		info->physicalHeightUm = params->physical_height_um;
	} else
		DDPPR_ERR("Cannot get lcm_ext_params\n");

	info->vsyncFPS = lcm_fps_ctx_get(crtc_id);
	if (!info->vsyncFPS) {
		_parse_tag_videolfb(&vramsize, &fb_base, &fps);
		info->vsyncFPS = fps;
	}

	return ret;
}

int mtk_drm_get_info_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file_priv)
{
	int ret = 0;
	struct drm_mtk_session_info *info = data;
	int s_type = MTK_SESSION_TYPE(info->session_id);

	if (s_type == MTK_SESSION_PRIMARY) {
		ret = mtk_drm_get_panel_info(dev, info, 0);
		return ret;
	} else if (s_type == MTK_SESSION_EXTERNAL) {
		struct mtk_drm_private *priv = dev->dev_private;
		struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(priv->crtc[1]);
		struct mtk_ddp_comp *output_comp;
		enum mtk_ddp_comp_type type;

		if (!mtk_crtc) {
			DDPPR_ERR("invalid CRTC1\n");
			return -EINVAL;
		}

		output_comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (unlikely(!output_comp)) {
			DDPPR_ERR("%s:CRTC1 invalid output comp\n",
				  __func__);
			return -EINVAL;
		}
		type = mtk_ddp_comp_get_type(output_comp->id);
		if (type == MTK_DSI)
			ret = mtk_drm_get_panel_info(dev, info, 1);
		else if (type == MTK_DP_INTF)
			ret = mtk_drm_dp_get_info(dev, info);
		else {
			DDPPR_ERR("invalid comp type: %d\n", type);
			ret = -EINVAL;
		}
		return ret;
	} else if (s_type == MTK_SESSION_MEMORY) {
		return ret;
	} else if (s_type == MTK_SESSION_SP) {
		ret = mtk_drm_get_panel_info(dev, info, 3);
		return ret;
	}
	DDPPR_ERR("invalid session type:0x%08x\n", info->session_id);

	return -EINVAL;
}

int mtk_drm_get_master_info_ioctl(struct drm_device *dev,
			   void *data, struct drm_file *file_priv)
{
	int *ret = (int *)data;

	*ret = drm_is_current_master(file_priv);

	return 0;
}


int mtk_drm_set_ddp_mode(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	int ret = 0;
	unsigned int *session_mode = data;

	ret = mtk_session_set_mode(dev, *session_mode);
	return ret;
}

int mtk_drm_ioctl_get_lcm_index(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	int ret = 0;
	unsigned int *info = data;
	struct mtk_drm_private *private = dev->dev_private;
	struct mtk_panel_params *params =
		mtk_drm_get_lcm_ext_params(private->crtc[0]);

	if (params) {
		*info = params->lcm_index;
	} else {
		*info = 0;
		DDPPR_ERR("Cannot get lcm_ext_params\n");
	}

	return ret;
}

int mtk_drm_ioctl_get_all_connector_panel_info(struct drm_device *dev, void *data,
		struct drm_file *file_priv)
{
	struct mtk_ddp_comp *dsi_comp;
	struct mtk_drm_private *private;
	struct mtk_drm_panels_info *panel_ctx =
			(struct mtk_drm_panels_info *)data;
	struct mtk_drm_panels_info __panel_ctx = {0};
	bool check_only_mode;
	void *ptr;
	void **uptr;
	int i, ret = 0;

	if (!dev)
		return -ENODEV;

	private = dev->dev_private;
	/* just need the struct mtk_dsi definition, would not really access HW or SW state */
	dsi_comp = private->ddp_comp[DDP_COMPONENT_DSI0];
	if (!dsi_comp)
		dsi_comp = private->ddp_comp[DDP_COMPONENT_DSI1];
	if (!dsi_comp)
		return -ENXIO;

	if (!panel_ctx) {
		DDPPR_ERR("%s panel_info alloc failed\n", __func__);
		return -EINVAL;
	}

	__panel_ctx.connector_cnt = panel_ctx->connector_cnt;
	check_only_mode = (__panel_ctx.connector_cnt == -1);
	/* need copy_from_user */
	if (check_only_mode == false) {
		ptr = panel_ctx->connector_obj_id;
		__panel_ctx.connector_obj_id =
			vmalloc(sizeof(unsigned int) * __panel_ctx.connector_cnt);
		__panel_ctx.panel_name = vmalloc(sizeof(char *) * __panel_ctx.connector_cnt);
		if (!__panel_ctx.connector_obj_id || !__panel_ctx.panel_name) {
			DDPPR_ERR("%s ojb_id panel_id or panel_name alloc fail\n", __func__);
			ret = -ENOMEM;
			goto exit0;
		}

		for (i = 0 ; i < __panel_ctx.connector_cnt ; ++i) {
			__panel_ctx.panel_name[i] = vzalloc(sizeof(char) * GET_PANELS_STR_LEN);
			if (!__panel_ctx.panel_name[i]) {
				DDPPR_ERR("%s alloc panel_name fail\n", __func__);
				ret = -ENOMEM;
				goto exit1;
			}
		}
	}

	mtk_ddp_comp_io_cmd(dsi_comp, NULL, GET_ALL_CONNECTOR_PANEL_NAME, &__panel_ctx);

	/* need copy_to_user */
	if (check_only_mode == false) {
		ptr = panel_ctx->connector_obj_id;
		if (copy_to_user((void __user *)ptr, __panel_ctx.connector_obj_id,
				sizeof(unsigned int) * __panel_ctx.connector_cnt)) {
			DDPPR_ERR("%s copy_to_user connector_obj_id fail\n", __func__);
			ret = -EINVAL;
			goto exit1;
		}

		uptr = vmalloc(sizeof(void __user *) * __panel_ctx.connector_cnt);
		if (!uptr) {
			DDPPR_ERR("%s alloc panel_name fail\n", __func__);
			ret = -ENOMEM;
			goto exit2;
		}

		if (copy_from_user(uptr, panel_ctx->panel_name,
				sizeof(void __user *) * __panel_ctx.connector_cnt)) {
			DDPPR_ERR("%s copy_from_user panel_name fail\n", __func__);
			ret = -EINVAL;
			goto exit2;
		}
		for (i = 0 ; i < __panel_ctx.connector_cnt; ++i) {
			ptr = panel_ctx->panel_name + (sizeof(void __user *) * i);
			if (copy_to_user((void __user *)uptr[i], __panel_ctx.panel_name[i],
					sizeof(char) * GET_PANELS_STR_LEN)) {
				DDPPR_ERR("%s copy_to_user panel_name fail\n", __func__);
				ret = -EINVAL;
				goto exit2;
			}
		}
	} else {
		panel_ctx->connector_cnt = __panel_ctx.connector_cnt;
		panel_ctx->default_connector_id = __panel_ctx.default_connector_id;
		return ret;
	}
exit2:
	vfree(uptr);
exit1:
	for (i = 0 ; i < __panel_ctx.connector_cnt; ++i)
		vfree(__panel_ctx.panel_name[i]);
exit0:
	vfree(__panel_ctx.connector_obj_id);
	vfree(__panel_ctx.panel_name);

	return ret;
}

void mtk_drm_mmlsys_submit_done_cb(void *cb_param)
{
	struct mtk_mml_cb_para *cb_para = (struct mtk_mml_cb_para *)cb_param;

	if (cb_para == NULL)
		return;

	atomic_set(&(cb_para->mml_job_submit_done), 1);
	wake_up_interruptible(&(cb_para->mml_job_submit_wq));
	DDPINFO("%s-\n", __func__);
}

void mtk_drm_wait_mml_submit_done(struct mtk_mml_cb_para *cb_para)
{
	int ret = 0;

	DDPINFO("%s+ 0x%x 0x%x, 0x%x\n", __func__,
		cb_para,
		&(cb_para->mml_job_submit_wq),
		&(cb_para->mml_job_submit_done));
	ret = wait_event_interruptible(
		cb_para->mml_job_submit_wq,
		atomic_read(&cb_para->mml_job_submit_done));
	atomic_set(&(cb_para->mml_job_submit_done), 0);
	DDPINFO("%s- ret:%d\n", __func__, ret);
}

struct mml_drm_ctx *mtk_drm_get_mml_drm_ctx(struct drm_device *dev,
	struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = dev->dev_private;
	struct platform_device *plat_dev = NULL;
	struct platform_device *mml_pdev = NULL;
	struct mml_drm_ctx *mml_ctx = NULL;
	struct mml_drm_param disp_param = {};
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (priv->mml_ctx != NULL)
		return priv->mml_ctx;

	plat_dev = of_find_device_by_node(priv->mutex_node);
	if (!plat_dev) {
		DDPPR_ERR("%s of_find_device_by_node open fail\n", __func__);
		goto err_handle_mtk_drm_get_mml_drm_ctx;
	}

	mml_pdev = mml_get_plat_device(plat_dev);
	if (!mml_pdev) {
		DDPPR_ERR("mtk_drm_ioctl_mml_gem_submit mml_get_plat_device open fail\n");
		goto err_handle_mtk_drm_get_mml_drm_ctx;
	}

	disp_param.dual = mtk_crtc->is_dual_pipe;
	disp_param.racing_height = 64;
	disp_param.vdo_mode =  (!mtk_crtc_is_frame_trigger_mode(crtc));
	disp_param.submit_cb = mtk_drm_mmlsys_submit_done_cb;

	mml_ctx = mml_drm_get_context(mml_pdev, &disp_param);
	if (!mml_ctx) {
		DDPPR_ERR("mml_drm_get_context fail. mml_ctx:%p\n", mml_ctx);
		goto err_handle_mtk_drm_get_mml_drm_ctx;
	}
	priv->mml_ctx = mml_ctx;
	DDPMSG("%s 2 0x%x", __func__, priv->mml_ctx);

	if (drm_crtc_index(crtc) == 0) {
		struct mtk_ddp_comp *output_comp = NULL;
		u32 panel_w = 0, panel_h = 0;
		u32 pixels = 0;

		output_comp = mtk_ddp_comp_request_output(mtk_crtc);
		if (output_comp) {
			panel_w =
			    mtk_ddp_comp_io_cmd(output_comp, NULL, DSI_GET_VIRTUAL_WIDTH, NULL);
			panel_h =
			    mtk_ddp_comp_io_cmd(output_comp, NULL, DSI_GET_VIRTUAL_HEIGH, NULL);
		}

		pixels = panel_w * panel_h;
		if (pixels > 0) {
			mml_drm_set_panel_pixel(mml_ctx, pixels);
			DDPMSG("%s set panel pixels %u\n", __func__, pixels);
		}
	}

	return priv->mml_ctx;

err_handle_mtk_drm_get_mml_drm_ctx:
	priv->mml_ctx = NULL;
	return priv->mml_ctx;
}

static void mtk_drm_init_dummy_table(struct mtk_drm_private *priv)
{
	struct dummy_mapping *table;
	size_t size;
	int i;

	switch (priv->data->mmsys_id) {
	case MMSYS_MT6983:
		table = mt6983_dispsys_dummy_register;
		size = MT6983_DUMMY_REG_CNT;
		break;
	case MMSYS_MT6895:
		/* mt6895 same with mt6983 */
		table = mt6983_dispsys_dummy_register;
		size = MT6983_DUMMY_REG_CNT;
		break;
	case MMSYS_MT6879:
		table = mt6879_dispsys_dummy_register;
		size = MT6879_DUMMY_REG_CNT;
		break;
	default:
		DDPMSG("no dummy table\n");
		return;
	}

	for (i = 0; i < size; i++) {
		if (table[i].comp_id == DDP_COMPONENT_ID_MAX) {
			if (priv->data->mmsys_id == MMSYS_MT6879) {
				//MT6879 can't use dispsys dummy reg, so change to use mutex
				struct mtk_ddp *ddp = dev_get_drvdata(priv->mutex_dev);

				table[i].pa_addr = ddp->regs_pa;
				table[i].addr = ddp->regs;
			} else {
				table[i].pa_addr = priv->config_regs_pa;
				table[i].addr = priv->config_regs;
			}
		/* TODO: BWM just for Dujac SW readyGo, Into SQC need to del and use new method */
		} else if (table[i].comp_id == (DDP_COMPONENT_ID_MAX | BIT(29))) {
			struct mtk_ddp *ddp = dev_get_drvdata(priv->mutex_dev);

			table[i].pa_addr = ddp->regs_pa;
			table[i].addr = ddp->regs;
		} else if (table[i].comp_id == (DDP_COMPONENT_ID_MAX | BIT(30))) {
			struct mtk_ddp *ddp = dev_get_drvdata(priv->mutex_dev);

			table[i].pa_addr = ddp->side_regs_pa;
			table[i].addr = ddp->side_regs;
		} else if (table[i].comp_id == (DDP_COMPONENT_ID_MAX | BIT(31))) {
			table[i].pa_addr = priv->side_config_regs_pa;
			table[i].addr = priv->side_config_regs;
		} else {
			struct mtk_ddp_comp *comp;

			comp = priv->ddp_comp[table[i].comp_id];
			table[i].pa_addr = comp->regs_pa;
			table[i].addr = comp->regs;
		}
	}
}

static int mtk_drm_kms_init(struct drm_device *drm)
{
	struct mtk_drm_private *private = drm->dev_private;
	struct platform_device *pdev;
	struct device_node *np;
	struct device *dma_dev;
	int ret, i;
	u32 condition_num;

	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_USE_M4U)) {
		if (!iommu_present(&platform_bus_type)) {
			DDPINFO("%s, iommu not ready\n", __func__);
			return -EPROBE_DEFER;
		}
	}

	DDPINFO("%s+\n", __func__);

	pdev = of_find_device_by_node(private->mutex_node);
	if (!pdev) {
		dev_err(drm->dev, "Waiting for disp-mutex device %s\n",
			private->mutex_node->full_name);
		of_node_put(private->mutex_node);
		return -EPROBE_DEFER;
	}

	private->mutex_dev = &pdev->dev;

	drm_mode_config_init(drm);

	drm->mode_config.min_width = 1;
	drm->mode_config.min_height = 1;

	/*
	 * set max width and height as default value(4096x4096).
	 * this value would be used to check framebuffer size limitation
	 * at drm_mode_addfb().
	 */
	drm->mode_config.max_width = 4096;
	drm->mode_config.max_height = 4096;
	drm->mode_config.funcs = &mtk_drm_mode_config_funcs;

	ret = component_bind_all(drm->dev, drm);
	if (ret)
		goto err_config_cleanup;

	ret = drm_vblank_init(drm, MAX_CRTC);
	if (ret < 0)
		goto err_component_unbind;
	/*
	 * We currently support two fixed data streams, each optional,
	 * and each statically assigned to a crtc:
	 * OVL0 -> COLOR0 -> AAL -> OD -> RDMA0 -> UFOE -> DSI0 ...
	 */
	ret = mtk_drm_crtc_create(drm, private->data->main_path_data);
	if (ret < 0)
		goto err_component_unbind;

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		/* ... and OVL1 -> COLOR1 -> GAMMA -> RDMA1 -> DPI0. */
		if (of_property_read_bool(private->mmsys_dev->of_node, "enable_ext_alter_path"))
			ret = mtk_drm_crtc_create(drm, private->data->ext_alter_path_data);
		else
			ret = mtk_drm_crtc_create(drm, private->data->ext_path_data);
		if (ret < 0)
			goto err_component_unbind;

		if (of_property_read_u32(private->mmsys_dev->of_node,
			"condition-num", &condition_num)) {
			DDPINFO("CRTC2 condition_num is 0, NO condition_num in dts\n");
			ret = mtk_drm_crtc_create(drm, private->data->third_path_data);
		} else {
			DDPDBG("CRTC2 condition_num is %d\n", condition_num);
			if (condition_num == 2)
				ret = mtk_drm_crtc_create(drm,
					private->data->third_path_data_wo_tdshp);
			else
				ret = mtk_drm_crtc_create(drm, private->data->third_path_data);
		}
		if (ret < 0)
			goto err_component_unbind;
		/*TODO: Need to check path rule*/
		if (private->data->fourth_path_data_secondary
			|| private->data->fourth_path_data_discrete) {
			DDPMSG("CRTC3 Path\n");
			if (of_property_read_bool(private->mmsys_dev->of_node,
				"enable_secondary_path"))
				ret = mtk_drm_crtc_create(drm,
					private->data->fourth_path_data_secondary);
			if (ret < 0)
				goto err_component_unbind;

			if (of_property_read_bool(private->mmsys_dev->of_node,
				"enable_discrete_path"))
				ret = mtk_drm_crtc_create(drm,
					private->data->fourth_path_data_discrete);
			if (ret < 0)
				goto err_component_unbind;
		}
	}
	/* TODO: allow_fb_modifiers = 1 and format_modifiers = null make drm_warn_on.
	 * so we set allow_fb_modifiers = 1 after mtk_plane_init
	 */
	drm->mode_config.allow_fb_modifiers = true;

	/* Use OVL device for all DMA memory allocations */
	np = private->comp_node[private->data->main_path_data
					->path[DDP_MAJOR][0][0]];
	if (np == NULL) {
		if (of_property_read_bool(private->mmsys_dev->of_node, "enable_ext_alter_path"))
			np = private->comp_node[private->data->ext_alter_path_data
						->path[DDP_MAJOR][0][0]];
		else
			np = private->comp_node[private->data->ext_path_data
						->path[DDP_MAJOR][0][0]];
	}
	pdev = of_find_device_by_node(np);
	if (!pdev) {
		ret = -ENODEV;
		dev_err(drm->dev, "Need at least one OVL device\n");
		goto err_component_unbind;
	}

	private->dma_dev = &pdev->dev;

	/*
	 * Configure the DMA segment size to make sure we get contiguous IOVA
	 * when importing PRIME buffers.
	 */
	dma_dev = drm->dev;
	if (!dma_dev->dma_parms) {
		private->dma_parms_allocated = true;
		dma_dev->dma_parms =
			devm_kzalloc(drm->dev, sizeof(*dma_dev->dma_parms),
				     GFP_KERNEL);
	}
	if (!dma_dev->dma_parms) {
		ret = -ENOMEM;
		goto put_dma_dev;
	}

	ret = dma_set_max_seg_size(dma_dev, (unsigned int)DMA_BIT_MASK(32));
	if (ret) {
		dev_err(dma_dev, "Failed to set DMA segment size\n");
		goto err_unset_dma_parms;
	}

	drm_kms_helper_poll_init(drm);
	drm_mode_config_reset(drm);

	INIT_WORK(&private->unreference.work, mtk_unreference_work);
	INIT_LIST_HEAD(&private->unreference.list);
	INIT_LIST_HEAD(&private->lyeblob_head);
	spin_lock_init(&private->unreference.lock);
	mutex_init(&private->commit.lock);
	mutex_init(&private->lyeblob_list_mutex);

	init_waitqueue_head(&private->repaint_data.wq);
	INIT_LIST_HEAD(&private->repaint_data.job_queue);
	INIT_LIST_HEAD(&private->repaint_data.job_pool);
	for (i = 0; i < MAX_CRTC ; ++i)
		atomic_set(&private->crtc_present[i], 0);
	atomic_set(&private->rollback_all, 0);

#ifdef CONFIG_DRM_MEDIATEK_DEBUG_FS
	mtk_drm_debugfs_init(drm, private);
#endif
	disp_dbg_init(drm);
	PanelMaster_Init(drm);
	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_MMDVFS_SUPPORT))
		mtk_drm_mmdvfs_init(drm->dev);
	DDPINFO("%s-\n", __func__);
	mtk_drm_init_dummy_table(private);

	mtk_drm_first_enable(drm);

	/*
	 * When kernel init, SMI larb will get once for keeping
	 * MTCMOS on. Then, this keeping will be released after
	 * display keep MTCMOS by itself.
	 */
	mtk_smi_init_power_off();

	return 0;

err_unset_dma_parms:
	if (private->dma_parms_allocated)
		dma_dev->dma_parms = NULL;
put_dma_dev:
	put_device(drm->dev);
err_component_unbind:
	component_unbind_all(drm->dev, drm);
err_config_cleanup:
	drm_mode_config_cleanup(drm);

	return ret;
}

static void mtk_drm_kms_deinit(struct drm_device *drm)
{
	struct mtk_drm_private *private = drm->dev_private;

	drm_kms_helper_poll_fini(drm);

	if (private->dma_parms_allocated)
		drm->dev->dma_parms = NULL;

	//drm_vblank_cleanup(drm);
	component_unbind_all(drm->dev, drm);
	drm_mode_config_cleanup(drm);

	disp_dbg_deinit();
	PanelMaster_Deinit();

	if (private->mml_ctx) {
		mml_drm_put_context(private->mml_ctx);
		private->mml_ctx = NULL;
	}
}

int mtk_drm_fm_lcm_auto_test(struct drm_device *dev, void *data,
			 struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *private;
	int *result = data;
	int ret = 0;

	/* this debug cmd only for crtc0 */
	crtc = list_first_entry(&(dev)->mode_config.crtc_list,
			typeof(*crtc), head);
	if (!crtc) {
		DDPPR_ERR("find crtc fail\n");
		return -1;
	}
	mtk_crtc = to_mtk_crtc(crtc);

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (!mtk_crtc->enabled || mtk_crtc->ddp_mode == DDP_NO_USE) {
		DDPINFO("crtc 0 is already sleep, skip\n");
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return 0;
	}

	private = dev->dev_private;
	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_IDLE_MGR)) {
		mtk_drm_set_idlemgr(crtc, 0, 0);
	}

	ret = mtk_crtc_lcm_ATA(crtc);

	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_IDLE_MGR))
		mtk_drm_set_idlemgr(crtc, 1, 0);

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	/* ret = 0 fail, ret = 1 pass */
	if (ret == 0) {
		DDPPR_ERR("ATA LCM failed\n");
		*result = 0;
	} else {
		DDPMSG("ATA LCM passed\n");
		*result = 1;
	}
	return 0;
}

static const struct drm_ioctl_desc mtk_ioctls[] = {
	DRM_IOCTL_DEF_DRV(MTK_GEM_CREATE, mtk_gem_create_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GEM_MAP_OFFSET, mtk_gem_map_offset_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GEM_SUBMIT, mtk_gem_submit_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_SESSION_CREATE, mtk_drm_session_create_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_SESSION_DESTROY, mtk_drm_session_destroy_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_LAYERING_RULE, mtk_layering_rule_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_CRTC_GETFENCE, mtk_drm_crtc_getfence_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_CRTC_FENCE_REL, mtk_drm_crtc_fence_release_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_CRTC_GETSFFENCE,
			  mtk_drm_crtc_get_sf_fence_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_SET_MSYNC_PARAMS,
			  mtk_drm_set_msync_params_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GET_MSYNC_PARAMS,
			  mtk_drm_get_msync_params_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_WAIT_REPAINT, mtk_drm_wait_repaint_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GET_DISPLAY_CAPS, mtk_drm_get_display_caps_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_SET_DDP_MODE, mtk_drm_set_ddp_mode,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GET_SESSION_INFO, mtk_drm_get_info_ioctl,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_GET_MASTER_INFO, mtk_drm_get_master_info_ioctl,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_PQ_PERSIST_PROPERTY,
				mtk_drm_ioctl_pq_get_persist_property,
				DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SET_CCORR, mtk_drm_ioctl_set_ccorr,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_CCORR_EVENTCTL, mtk_drm_ioctl_ccorr_eventctl,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_CCORR_GET_IRQ, mtk_drm_ioctl_ccorr_get_irq,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_AIBLD_CV_MODE, mtk_drm_ioctl_aibld_cv_mode,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SUPPORT_COLOR_TRANSFORM,
				mtk_drm_ioctl_support_color_matrix,
				DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SET_GAMMALUT, mtk_drm_ioctl_set_gammalut,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SET_12BIT_GAMMALUT,
			  mtk_drm_ioctl_set_12bit_gammalut,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SET_PQPARAM, mtk_drm_ioctl_set_pqparam,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SET_PQINDEX, mtk_drm_ioctl_set_pqindex,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SET_COLOR_REG, mtk_drm_ioctl_set_color_reg,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_MUTEX_CONTROL, mtk_drm_ioctl_mutex_control,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_READ_REG, mtk_drm_ioctl_read_reg,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_WRITE_REG, mtk_drm_ioctl_write_reg,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_BYPASS_COLOR, mtk_drm_ioctl_bypass_color,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_PQ_SET_WINDOW, mtk_drm_ioctl_pq_set_window,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_WRITE_SW_REG, mtk_drm_ioctl_write_sw_reg,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_READ_SW_REG, mtk_drm_ioctl_read_sw_reg,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_GET_LCM_INDEX, mtk_drm_ioctl_get_lcm_index,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_GET_PANELS_INFO, mtk_drm_ioctl_get_all_connector_panel_info,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_AAL_INIT_REG, mtk_drm_ioctl_aal_init_reg,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_AAL_GET_HIST, mtk_drm_ioctl_aal_get_hist,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_AAL_SET_PARAM, mtk_drm_ioctl_aal_set_param,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_AAL_SET_ESS20_SPECT_PARAM, mtk_drm_ioctl_aal_set_ess20_spect_param,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_AAL_EVENTCTL, mtk_drm_ioctl_aal_eventctl,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_AAL_INIT_DRE30, mtk_drm_ioctl_aal_init_dre30,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_AAL_GET_SIZE, mtk_drm_ioctl_aal_get_size,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_HDMI_GET_DEV_INFO, mtk_drm_dp_get_dev_info,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_HDMI_AUDIO_ENABLE, mtk_drm_dp_audio_enable,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_HDMI_AUDIO_CONFIG, mtk_drm_dp_audio_config,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_HDMI_GET_CAPABILITY, mtk_drm_dp_get_cap,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_MML_GEM_SUBMIT, mtk_drm_ioctl_mml_gem_submit,
			  DRM_UNLOCKED | DRM_AUTH | DRM_RENDER_ALLOW),
	DRM_IOCTL_DEF_DRV(MTK_SET_DISP_TDSHP_REG, mtk_drm_ioctl_tdshp_set_reg,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_DISP_TDSHP_GET_SIZE, mtk_drm_ioctl_tdshp_get_size,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_C3D_GET_BIN_NUM, mtk_drm_ioctl_c3d_get_bin_num,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_C3D_GET_IRQ, mtk_drm_ioctl_c3d_get_irq,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_C3D_EVENTCTL, mtk_drm_ioctl_c3d_eventctl,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_C3D_SET_LUT, mtk_drm_ioctl_c3d_set_lut,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SET_BYPASS_C3D, mtk_drm_ioctl_bypass_c3d,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_GET_CHIST, mtk_drm_ioctl_get_chist,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_GET_CHIST_CAPS, mtk_drm_ioctl_get_chist_caps,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SET_CHIST_CONFIG, mtk_drm_ioctl_set_chist_config,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SET_DITHER_PARAM, mtk_drm_ioctl_set_dither_param,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_BYPASS_DISP_GAMMA, mtk_drm_ioctl_bypass_disp_gamma,
		DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_FACTORY_LCM_AUTO_TEST, mtk_drm_fm_lcm_auto_test,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_GET_PQ_CAPS, mtk_drm_ioctl_get_pq_caps,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SET_PQ_CAPS, mtk_drm_ioctl_set_pq_caps,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_DISP_CLARITY_SET_REG, mtk_drm_ioctl_clarity_set_reg,
				  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_AAL_SET_TRIGGER_STATE, mtk_drm_ioctl_aal_set_trigger_state,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_DRM_SET_LEASE_INFO, mtk_drm_set_lease_info_ioctl,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_DRM_GET_LEASE_INFO, mtk_drm_get_lease_info_ioctl,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_ODDMR_LOAD_PARAM, mtk_drm_ioctl_oddmr_load_param,
			  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_ODDMR_CTL, mtk_drm_ioctl_oddmr_ctl,
				  DRM_UNLOCKED),
	DRM_IOCTL_DEF_DRV(MTK_SET_GAMMA_MUL_DISABLE, mtk_drm_ioctl_gamma_mul_disable,
				  DRM_UNLOCKED),
};

static const struct file_operations mtk_drm_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = mtk_drm_gem_mmap,
	.poll = drm_poll,
	.read = drm_read,
#ifdef CONFIG_COMPAT
	.compat_ioctl = drm_compat_ioctl,
#endif
};

static struct drm_driver mtk_drm_driver = {
	.driver_features =
		DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,

	/* .get_vblank_counter = drm_vblank_no_hw_counter, */

	.dumb_create = mtk_drm_gem_dumb_create,
	.dumb_map_offset = mtk_drm_gem_dumb_map_offset,
	.prime_handle_to_fd = drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle = drm_gem_prime_fd_to_handle,
	.gem_prime_import = drm_gem_prime_import,
	.gem_prime_import_sg_table = mtk_gem_prime_import_sg_table,
	.gem_prime_mmap = mtk_drm_gem_mmap_buf,
	.ioctls = mtk_ioctls,
	.num_ioctls = ARRAY_SIZE(mtk_ioctls),
	.fops = &mtk_drm_fops,

	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
};

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int mtk_drm_bind(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm;
	int ret;

	DDPINFO("%s+\n", __func__);
	drm = drm_dev_alloc(&mtk_drm_driver, dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	drm->dev_private = private;
	private->drm = drm;

	ret = mtk_drm_kms_init(drm);
	if (ret < 0)
		goto err_free;

	ret = drm_dev_register(drm, 0);
	if (ret < 0)
		goto err_deinit;

	mtk_layering_rule_init(drm);
#ifdef DRM_OVL_SELF_PATTERN
	mtk_drm_disp_test_init(drm);
	mtk_drm_disp_test_show(test_crtc, 1);
#else
	mtk_drm_assert_init(drm);
#endif

	DDPINFO("%s-\n", __func__);

	return 0;

err_deinit:
	mtk_drm_kms_deinit(drm);
err_free:
	drm_dev_put(drm);
	return ret;
}

static void mtk_drm_unbind(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);

	drm_dev_unregister(private->drm);
	drm_dev_put(private->drm);
	private->drm = NULL;
}

static const struct component_master_ops mtk_drm_ops = {
	.bind = mtk_drm_bind, .unbind = mtk_drm_unbind,
};

static const struct of_device_id mtk_ddp_comp_dt_ids[] = {
	{.compatible = "mediatek,mt2701-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6779-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6873-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6853-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6833-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6879-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6855-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt8173-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6885-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6983-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6985-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6895-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt6886-disp-ovl",
	 .data = (void *)MTK_DISP_OVL},
	{.compatible = "mediatek,mt2701-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6779-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6873-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6853-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6833-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6879-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6855-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt8173-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6885-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6983-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
//	{.compatible = "mediatek,mt6985-disp-rdma",
//	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6895-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt6886-disp-rdma",
	 .data = (void *)MTK_DISP_RDMA},
	{.compatible = "mediatek,mt8173-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6779-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6885-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6873-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6853-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6833-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6879-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6855-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6983-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6985-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6895-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6886-disp-wdma",
	 .data = (void *)MTK_DISP_WDMA},
	{.compatible = "mediatek,mt6779-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6885-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6983-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6985-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6895-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6886-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6879-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6855-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6873-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6853-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6833-disp-ccorr",
	 .data = (void *)MTK_DISP_CCORR},
	{.compatible = "mediatek,mt6983-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6985-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6895-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6886-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6879-disp-chist",
	 .data = (void *)MTK_DISP_CHIST},
	{.compatible = "mediatek,mt6983-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6985-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6895-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6886-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6879-disp-c3d",
	 .data = (void *)MTK_DISP_C3D},
	{.compatible = "mediatek,mt6983-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt6985-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt6895-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt6879-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt6855-disp-tdshp",
	 .data = (void *)MTK_DISP_TDSHP},
	{.compatible = "mediatek,mt2701-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6779-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6873-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6853-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6983-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6985-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6895-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6886-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6879-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6855-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6833-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt8173-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6885-disp-color",
	 .data = (void *)MTK_DISP_COLOR},
	{.compatible = "mediatek,mt6779-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6873-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6853-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6833-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt8173-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6885-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6983-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6985-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6895-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6886-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6879-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6855-disp-aal",
	 .data = (void *)MTK_DISP_AAL},
	{.compatible = "mediatek,mt6779-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt8173-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6885-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6873-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6853-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6833-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6983-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6985-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6895-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6886-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6879-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6855-disp-gamma",
	 .data = (void *)MTK_DISP_GAMMA},
	{.compatible = "mediatek,mt6779-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6885-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6983-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6985-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6895-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6886-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6879-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6855-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6873-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6853-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt6833-disp-dither",
	 .data = (void *)MTK_DISP_DITHER},
	{.compatible = "mediatek,mt8173-disp-ufoe",
	 .data = (void *)MTK_DISP_UFOE},
	{.compatible = "mediatek,mt2701-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6779-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt8173-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6885-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6885-dp-intf",
	 .data = (void *)MTK_DP_INTF},
	{.compatible = "mediatek,mt6983-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6985-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6895-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6886-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6983-dp-intf",
	 .data = (void *)MTK_DP_INTF},
	{.compatible = "mediatek,mt6985-dp-intf",
	 .data = (void *)MTK_DP_INTF},
	{.compatible = "mediatek,mt6895-dp-intf",
	 .data = (void *)MTK_DP_INTF},
	{.compatible = "mediatek,mt6873-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6853-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6833-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6879-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt6855-dsi",
	 .data = (void *)MTK_DSI},
	{.compatible = "mediatek,mt2712-dpi",
	 .data = (void *)MTK_DPI},
	{.compatible = "mediatek,mt6779-dpi",
	 .data = (void *)MTK_DPI},
	{.compatible = "mediatek,mt8173-dpi",
	 .data = (void *)MTK_DPI},
	{.compatible = "mediatek,mt2701-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt2712-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6779-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt8173-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6885-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6983-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6985-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6895-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6886-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6873-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6853-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6833-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6879-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt6855-disp-mutex",
	 .data = (void *)MTK_DISP_MUTEX},
	{.compatible = "mediatek,mt2701-disp-pwm",
	 .data = (void *)MTK_DISP_BLS},
	{.compatible = "mediatek,mt6779-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt8173-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6885-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6983-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6985-disp-pwm0",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6895-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6886-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6873-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6853-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6833-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6879-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt6855-disp-pwm",
	 .data = (void *)MTK_DISP_PWM},
	{.compatible = "mediatek,mt8173-disp-od",
	 .data = (void *)MTK_DISP_OD},
	{.compatible = "mediatek,mt6779-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6885-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6873-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6853-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6833-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6879-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6855-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6983-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6985-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6895-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6886-disp-rsz",
	 .data = (void *)MTK_DISP_RSZ},
	{.compatible = "mediatek,mt6779-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6885-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6983-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6985-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6895-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6886-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6879-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6855-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6873-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6853-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6833-disp-postmask",
	 .data = (void *)MTK_DISP_POSTMASK},
	{.compatible = "mediatek,mt6983-disp-cm",
	 .data = (void *)MTK_DISP_CM},
//	{.compatible = "mediatek,mt6985-disp-cm",
//	 .data = (void *)MTK_DISP_CM},
	{.compatible = "mediatek,mt6895-disp-cm",
	 .data = (void *)MTK_DISP_CM},
	{.compatible = "mediatek,mt6886-disp-cm",
	 .data = (void *)MTK_DISP_CM},
	{.compatible = "mediatek,mt6879-disp-cm",
	 .data = (void *)MTK_DISP_CM},
	{.compatible = "mediatek,mt6983-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6985-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6895-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6886-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6879-disp-spr",
	 .data = (void *)MTK_DISP_SPR},
	{.compatible = "mediatek,mt6985-disp-oddmr",
	 .data = (void *)MTK_DISP_ODDMR},
	{.compatible = "mediatek,mt6885-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6983-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6985-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6895-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6886-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6879-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6855-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6873-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6853-disp-dsc",
	 .data = (void *)MTK_DISP_DSC},
	{.compatible = "mediatek,mt6885-disp-merge",
	 .data = (void *)MTK_DISP_MERGE},
	{.compatible = "mediatek,mt6983-disp-merge",
	 .data = (void *)MTK_DISP_MERGE},
	{.compatible = "mediatek,mt6985-disp-merge",
	 .data = (void *)MTK_DISP_MERGE},
	{.compatible = "mediatek,mt6895-disp-merge",
	 .data = (void *)MTK_DISP_MERGE},
	{.compatible = "mediatek,mt6885-dp_tx",
	 .data = (void *)MTK_DISP_DPTX},
	{.compatible = "mediatek,mt6983-dp_tx",
	 .data = (void *)MTK_DISP_DPTX},
	{.compatible = "mediatek,mt6985-dp_tx",
	 .data = (void *)MTK_DISP_DPTX},
	{.compatible = "mediatek,mt6895-dp_tx",
	 .data = (void *)MTK_DISP_DPTX},
	{.compatible = "mediatek,mt6885-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6983-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6985-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6895-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6873-dmdp-aal",
	 .data = (void *)MTK_DMDP_AAL},
	{.compatible = "mediatek,mt6983-disp-y2r",
	 .data = (void *)MTK_DISP_Y2R},
	{.compatible = "mediatek,mt6985-disp-y2r",
	 .data = (void *)MTK_DISP_Y2R},
	{.compatible = "mediatek,mt6886-disp-y2r",
	 .data = (void *)MTK_DISP_Y2R},
	/* MML */
	{.compatible = "mediatek,mt6983-mml_rsz",
	 .data = (void *)MTK_MML_RSZ},
	{.compatible = "mediatek,mt6983-mml_hdr",
	 .data = (void *)MTK_MML_HDR},
	{.compatible = "mediatek,mt6983-mml_aal",
	 .data = (void *)MTK_MML_AAL},
	{.compatible = "mediatek,mt6983-mml_tdshp",
	 .data = (void *)MTK_MML_TDSHP},
	{.compatible = "mediatek,mt6983-mml_color",
	 .data = (void *)MTK_MML_COLOR},
	{.compatible = "mediatek,mt6886-mml_rsz",
	 .data = (void *)MTK_MML_RSZ},
	{.compatible = "mediatek,mt6886-mml_hdr",
	 .data = (void *)MTK_MML_HDR},
	{.compatible = "mediatek,mt6886-mml_aal",
	 .data = (void *)MTK_MML_AAL},
	{.compatible = "mediatek,mt6886-mml_tdshp",
	 .data = (void *)MTK_MML_TDSHP},
	{.compatible = "mediatek,mt6886-mml_color",
	 .data = (void *)MTK_MML_COLOR},
	{.compatible = "mediatek,mt6983-mml",
	 .data = (void *)MTK_MML_MML},
	{.compatible = "mediatek,mt6985-mml",
	 .data = (void *)MTK_MML_MML},
	{.compatible = "mediatek,mt6886-mml",
	 .data = (void *)MTK_MML_MML},
	{.compatible = "mediatek,mt6983-mml_mutex",
	 .data = (void *)MTK_MML_MUTEX},
	{.compatible = "mediatek,mt6985-mml_mutex",
	 .data = (void *)MTK_MML_MUTEX},
	{.compatible = "mediatek,mt6886-mml_mutex",
	 .data = (void *)MTK_MML_MUTEX},
	{.compatible = "mediatek,mt6983-mml_wrot",
	 .data = (void *)MTK_MML_WROT},
	{.compatible = "mediatek,mt6886-mml_wrot",
	 .data = (void *)MTK_MML_WROT},
	{.compatible = "mediatek,mt6983-disp-dlo-async3",
	 .data = (void *)MTK_DISP_DLO_ASYNC},
	{.compatible = "mediatek,mt6886-disp-dlo-async3",
	 .data = (void *)MTK_DISP_DLO_ASYNC},
	{.compatible = "mediatek,mt6985-disp-dlo-async",
	 .data = (void *)MTK_DISP_DLO_ASYNC},
	{.compatible = "mediatek,mt6983-disp-dli-async3",
	 .data = (void *)MTK_DISP_DLI_ASYNC},
	{.compatible = "mediatek,mt6886-disp-dli-async3",
	 .data = (void *)MTK_DISP_DLI_ASYNC},
	{.compatible = "mediatek,mt6985-disp-dli-async",
	 .data = (void *)MTK_DISP_DLI_ASYNC},
	{.compatible = "mediatek,mt6983-disp-inlinerotate",
	 .data = (void *)MTK_DISP_INLINE_ROTATE},
	{.compatible = "mediatek,mt6895-disp-inlinerotate",
	 .data = (void *)MTK_DISP_INLINE_ROTATE},
	{.compatible = "mediatek,mt6985-disp-inlinerotate",
	 .data = (void *)MTK_DISP_INLINE_ROTATE},
	{.compatible = "mediatek,mt6886-disp-inlinerotate",
	 .data = (void *)MTK_DISP_INLINE_ROTATE},
	{.compatible = "mediatek,mt6983-mmlsys-bypass",
	 .data = (void *)MTK_MMLSYS_BYPASS},
	{.compatible = "mediatek,mt6985-disp-postalign",
	 .data = (void *)MTK_DISP_POSTALIGN},
	{} };

static struct disp_iommu_device disp_iommu;
static struct platform_device mydev;

struct disp_iommu_device *disp_get_iommu_dev(void)
{
	struct device_node *larb_node[DISP_LARB_COUNT];
	struct platform_device *larb_pdev[DISP_LARB_COUNT];
	int larb_idx = 0;
	struct device_node *np;

	if (disp_iommu.inited)
		return &disp_iommu;

	for (larb_idx = 0; larb_idx < DISP_LARB_COUNT; larb_idx++) {

		larb_node[larb_idx] = of_parse_phandle(mydev.dev.of_node,
						"mediatek,larb", larb_idx);
		if (!larb_node[larb_idx]) {
			DDPINFO("disp driver get larb %d fail\n", larb_idx);
			return NULL;
		}
		larb_pdev[larb_idx] =
			of_find_device_by_node(larb_node[larb_idx]);
		of_node_put(larb_node[larb_idx]);
		if ((!larb_pdev[larb_idx]) ||
		    (!larb_pdev[larb_idx]->dev.driver)) {
			if (!larb_pdev[larb_idx])
				DDPINFO("earlier than SMI, larb_pdev null\n");
			else
				DDPINFO("earlier than SMI, larb drv null\n");
		}

		disp_iommu.larb_pdev[larb_idx] = larb_pdev[larb_idx];
	}
	/* add for mmp dump mva->pa */
	np = of_find_compatible_node(NULL, NULL, "mediatek,mt-pseudo_m4u-port");
	if (np == NULL) {
		DDPINFO("DT,mediatek,mt-pseudo_m4u-port is not found\n");
	} else {
		disp_iommu.iommu_pdev = of_find_device_by_node(np);
		of_node_put(np);
		if (!disp_iommu.iommu_pdev)
			DDPINFO("get iommu device failed\n");
	}
	disp_iommu.inited = 1;
	return &disp_iommu;
}

struct device *mtk_drm_get_pd_device(struct device *dev, const char *id)
{
	int index;
	struct device_node *np = NULL;
	struct platform_device *pd_pdev;
	struct device *pd_dev;

	index = of_property_match_string(dev->of_node, "pd-names", id);
	np = of_parse_phandle(dev->of_node, "pd-others", index);
	if (!np) {
		DDPPR_ERR("can't find %s device node\n", id);
		return NULL;
	}

	DDPINFO("get %s power-domain at %s\n", id, np->full_name);

	pd_pdev = of_find_device_by_node(np);
	if (!pd_pdev) {
		DDPPR_ERR("can't get %s pdev\n", id);
		return NULL;
	}

	pd_dev = get_device(&pd_pdev->dev);
	of_node_put(np);

	return pd_dev;
}

static int mtk_drm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mtk_drm_private *private;
	struct resource *mem;
	struct device_node *node;
	struct component_match *match = NULL;
	unsigned int dispsys_num = 0, ovlsys_num = 0;
	int ret, len;
	int i;
	struct platform_device *side_pdev;
	struct device *side_dev = NULL;
	struct device_node *side_node = NULL;
	struct device_node *aod_scp_node = NULL;
	struct device_node *disp_plat_dbg_node = pdev->dev.of_node;
	const __be32 *ranges = NULL;
	bool mml_found = false;

	disp_dbg_probe();
	PanelMaster_probe();
	DDPINFO("%s+\n", __func__);

	//drm_debug = 0x2; /* DRIVER messages */
	private = devm_kzalloc(dev, sizeof(*private), GFP_KERNEL);
	if (!private)
		return -ENOMEM;

	private->data = of_device_get_match_data(dev);

	private->reg_data = mtk_ddp_get_mmsys_reg_data(private->data->mmsys_id);
	if (IS_ERR(private->reg_data)) {
		ret = PTR_ERR(private->config_regs);
		DDPPR_ERR("Failed to get mmsys register data: %d\n", ret);
		return ret;
	}

	mutex_init(&private->commit.lock);
	INIT_WORK(&private->commit.work, mtk_atomic_work);

	mtk_drm_helper_init(dev, &private->helper_opt);

	/* Init disp_global_stage from platform dts */
	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_STAGE) == DISP_HELPER_STAGE_NORMAL)
		disp_helper_set_stage(DISP_HELPER_STAGE_NORMAL);
	else
		disp_helper_set_stage(DISP_HELPER_STAGE_BRING_UP);

	ranges = of_get_property(dev->of_node, "dma-ranges", &len);
	if (ranges) {
		ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34));
		if (ret)
			DDPPR_ERR("Failed to set_coherent_mask: %d\n", ret);
	}

	ret = of_property_read_u32(dev->of_node,
				"dispsys_num", &dispsys_num);
	if (ret) {
		dev_err(dev,
			"no dispsys_config dispsys_num\n", ret);
		dispsys_num = 1;
	}

	private->dispsys_num = dispsys_num;

	ret = of_property_read_u32(dev->of_node,
				"ovlsys_num", &ovlsys_num);
	private->ovlsys_num = ovlsys_num;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	private->config_regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(private->config_regs)) {
		ret = PTR_ERR(private->config_regs);
		dev_err(dev, "Failed to ioremap mmsys-config resource: %d\n",
			ret);
		return ret;
	}
	private->config_regs_pa = mem->start;
	private->mmsys_dev = dev;

	if (dispsys_num <= 1)
		goto SKIP_SIDE_DISP;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (mem) {
		private->side_config_regs = devm_ioremap_resource(dev, mem);
		if (IS_ERR(private->side_config_regs)) {
			ret = PTR_ERR(private->side_config_regs);
			dev_err(dev, "Failed to ioremap mmsys-config resource: %d\n",
				ret);
			return ret;
		}
		private->side_config_regs_pa = mem->start;
	}

	side_dev = mtk_drm_get_pd_device(dev, "side_dispsys");
	if (!side_dev) {
		/* tricky method to handle dispsys1 power domain */
		side_node = of_find_compatible_node(NULL, NULL, "mediatek,disp_mutex0");
		if (side_node) {
			side_pdev = of_find_device_by_node(side_node);
			if (!side_pdev)
				DDPPR_ERR("can't get side_mmsys_dev\n");
			else
				side_dev = get_device(&side_pdev->dev);
		} else {
			DDPPR_ERR("can't find side_mmsys node");
		}
		of_node_put(side_node);
	}
	private->side_mmsys_dev = side_dev;

SKIP_SIDE_DISP:

	if (private->ovlsys_num == 0)
		goto SKIP_OVLSYS_CONFIG;

	/* ovlsys config */
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (!mem)
		goto SKIP_OVLSYS_CONFIG;

	private->ovlsys0_regs_pa = mem->start;
	private->ovlsys0_regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(private->ovlsys0_regs)) {
		ret = PTR_ERR(private->ovlsys0_regs);
		dev_err(dev, "Failed to ioremap ovlsys0-config resource: %d\n",
			ret);
		return ret;
	}

	side_dev = mtk_drm_get_pd_device(dev, "ovlsys");
	private->ovlsys_dev = side_dev;

	if (private->ovlsys_num == 1)
		goto SKIP_OVLSYS_CONFIG;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	if (!mem)
		goto SKIP_OVLSYS_CONFIG;

	private->ovlsys1_regs_pa = mem->start;
	private->ovlsys1_regs = devm_ioremap_resource(dev, mem);
	if (IS_ERR(private->ovlsys1_regs)) {
		ret = PTR_ERR(private->ovlsys1_regs);
		dev_err(dev, "Failed to ioremap ovlsys1-config resource: %d\n",
			ret);
		return ret;
	}

	side_dev = mtk_drm_get_pd_device(dev, "side_ovlsys");
	private->side_ovlsys_dev = side_dev;

SKIP_OVLSYS_CONFIG:

	if (private->data->bypass_infra_ddr_control) {
		struct device_node *infra_node;
		struct platform_device *infra_pdev;
		struct device *infra_dev;
		struct resource *infra_mem;

		infra_node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao_mem");
		if (infra_node == NULL) {
			DDPPR_ERR("mediatek,infracfg_ao_mem is not found\n");
		} else {
			infra_pdev = of_find_device_by_node(infra_node);
			if (!infra_pdev) {
				dev_err(dev, "Waiting for infra-ao-mem device %s\n",
					private->mutex_node->full_name);
				of_node_put(infra_node);
				return -EPROBE_DEFER;
			}
			infra_dev = get_device(&infra_pdev->dev);
			infra_mem = platform_get_resource(infra_pdev, IORESOURCE_MEM, 0);
			private->infra_regs_pa = infra_mem->start;
			private->infra_regs = devm_ioremap_resource(infra_dev, infra_mem);
			if (IS_ERR(private->infra_regs))
				DDPPR_ERR("%s: infra_ao_base of_iomap failed\n", __func__);
			else
				DDPMSG("%s, infra_regs:0x%p, infra_regs_pa:0x%lx\n",
					__func__, (void *)private->infra_regs,
					private->infra_regs_pa);
		}
		of_node_put(infra_node);
	}

	pm_runtime_enable(dev);
	if (private->side_mmsys_dev)
		pm_runtime_enable(private->side_mmsys_dev);
	if (private->ovlsys_dev)
		pm_runtime_enable(private->ovlsys_dev);
	if (private->side_ovlsys_dev)
		pm_runtime_enable(private->side_ovlsys_dev);

	for (i = 0 ; i < MAX_CRTC ; ++i) {
		unsigned int value;

		ret = of_property_read_u32_index(dev->of_node, "pre-define-bw", i, &value);
		if (ret < 0)
			value = 0;

		private->pre_defined_bw[i] = value;
		DDPINFO("CRTC %d available BW:%x\n", i, value);
	}

	/* Get and enable top clk align to HW */
	mtk_drm_get_top_clk(private);

	/* Get AOD-SCP config */
	aod_scp_node = of_find_node_by_name(NULL, "AOD_SCP_ON");
	if (!aod_scp_node) {
		aod_scp_flag = 0;
		DDPMSG("%s AOD-SCP OFF\n", __func__);
	}	else {
		aod_scp_flag = 1;
		DDPMSG("%s AOD-SCP ON\n", __func__);
	}

	ret = of_property_read_u32(disp_plat_dbg_node, "disp_plat_dbg_addr", &g_disp_plat_dbg_addr);
	if (ret) {
		g_disp_plat_dbg_addr = 0;
		pr_info("%s: get disp_plat_dbg_addr fail\n", __func__);
	}

	ret = of_property_read_u32(disp_plat_dbg_node, "disp_plat_dbg_size", &g_disp_plat_dbg_size);
	if (ret) {
		g_disp_plat_dbg_size = 0;
		pr_info("%s: get disp_plat_dbg_size fail\n", __func__);
	}

	if ((g_disp_plat_dbg_addr > 0) && (g_disp_plat_dbg_size > 0)) {
		g_disp_plat_dbg_buf_addr = ioremap_wc((phys_addr_t)g_disp_plat_dbg_addr,
			g_disp_plat_dbg_size);
		DDPMSG("disp_plat_dbg addr=0x%x, size=0x%x, buf_addr=0x%lx, %s\n",
			g_disp_plat_dbg_addr, g_disp_plat_dbg_size,
			(unsigned long)g_disp_plat_dbg_buf_addr, __func__);
		disp_plat_dbg_buf_attr.size = g_disp_plat_dbg_size;
		ret = sysfs_create_bin_file(&pdev->dev.kobj, &disp_plat_dbg_buf_attr);
		if (ret) {
			dev_err(&pdev->dev, "Failed to create disp_plat_dbg buf file\n");
			return ret;
		}
	}

	if (mtk_drm_helper_get_opt(private->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT))
		private->hrt_bw_request =
			of_mtk_icc_get(dev, "disp_hrt_qos");

	/* Iterate over sibling DISP function blocks */
	for_each_child_of_node(dev->of_node->parent, node) {
		const struct of_device_id *of_id;
		enum mtk_ddp_comp_type comp_type;
		enum mtk_ddp_comp_id comp_id;

		of_id = of_match_node(mtk_ddp_comp_dt_ids, node);
		if (!of_id)
			continue;

		if (!of_device_is_available(node)) {
			dev_dbg(dev, "Skipping disabled component %s\n",
				node->full_name);
			continue;
		}

		comp_type = (enum mtk_ddp_comp_type)of_id->data;

		if (comp_type == MTK_DISP_MUTEX) {
			private->mutex_node = of_node_get(node);
			continue;
		}

		DDPINFO("%s line:%d, comp_type:%d\n", __func__, __LINE__,
			comp_type);

		comp_id = mtk_ddp_comp_get_id(node, comp_type);
		if ((int)comp_id < 0) {
			dev_warn(dev, "Skipping unknown component %s\n",
				 node->full_name);
			continue;
		}
		DDPINFO("%s line:%d, comp_id:%d\n", __func__, __LINE__,
			comp_id);
		/*comp node is registered here including PQ module*/
		private->comp_node[comp_id] = of_node_get(node);

		/*
		 * Currently only the COLOR, OVL, RDMA, DSI, and DPI blocks have
		 * separate component platform drivers and initialize their own
		 * DDP component structure. The others are initialized here.
		 */
		if (comp_type == MTK_DISP_OVL ||
		    comp_type == MTK_DISP_MERGE ||
		    comp_type == MTK_DISP_RDMA || comp_type == MTK_DISP_WDMA ||
		    comp_type == MTK_DISP_RSZ ||
		    comp_type == MTK_DISP_POSTMASK || comp_type == MTK_DSI
		    || comp_type == MTK_DISP_DSC || comp_type == MTK_DPI
#ifndef DRM_BYPASS_PQ
		    || comp_type == MTK_DISP_TDSHP || comp_type == MTK_DISP_CHIST
		    || comp_type == MTK_DISP_C3D || comp_type == MTK_DISP_COLOR ||
		    comp_type == MTK_DISP_CCORR ||
		    comp_type == MTK_DISP_GAMMA || comp_type == MTK_DISP_AAL ||
		    comp_type == MTK_DISP_DITHER ||
		    comp_type == MTK_DISP_CM || comp_type == MTK_DISP_SPR ||
		    comp_type == MTK_DISP_POSTALIGN || comp_type == MTK_DMDP_AAL
		    || comp_type == MTK_DISP_ODDMR
#endif
		    || comp_type == MTK_DP_INTF || comp_type == MTK_DISP_DPTX
		    || comp_type == MTK_DISP_Y2R || comp_type == MTK_DISP_INLINE_ROTATE
		    || comp_type == MTK_DISP_DLI_ASYNC || comp_type == MTK_DISP_DLO_ASYNC
		) {
			dev_info(dev, "Adding component match for %s, comp_id:%d\n",
				 node->full_name, comp_id);
			component_match_add(dev, &match, compare_of, node);
		} else if (comp_type == MTK_MML_MML || comp_type == MTK_MML_MUTEX ||
			   comp_type == MTK_MML_RSZ || comp_type == MTK_MML_HDR ||
			   comp_type == MTK_MML_AAL || comp_type == MTK_MML_TDSHP ||
			   comp_type == MTK_MML_COLOR || comp_type == MTK_MML_WROT) {
			dev_info(dev, "Adding component match for %s, comp_id:%d\n",
				 node->full_name, comp_id);
			component_match_add(dev, &match, compare_of, node);
			mml_found = true;
		} else {
			struct mtk_ddp_comp *comp;

			comp = devm_kzalloc(dev, sizeof(*comp), GFP_KERNEL);
			if (!comp) {
				ret = -ENOMEM;
				goto err_node;
			}

			ret = mtk_ddp_comp_init(dev, node, comp, comp_id, NULL);
			if (ret)
				goto err_node;

		      private
			->ddp_comp[comp_id] = comp;
		}
	}

	if (!mml_found) { /* if mmlsys is not a sibling of dispsys */
		const char *path;
		struct device_node *alias_node = of_find_node_by_path("/aliases");

		if (!of_property_read_string(alias_node, "mmlsys-cfg", &path)) {
			node = of_find_node_by_path(path);
			dev_info(dev, "Adding component match for %s\n", node->full_name);
			component_match_add(dev, &match, compare_of, node);
			of_node_put(node);
		}
		if (!of_property_read_string(alias_node, "mml-mutex", &path)) {
			node = of_find_node_by_path(path);
			dev_info(dev, "Adding component match for %s\n", node->full_name);
			component_match_add(dev, &match, compare_of, node);
			of_node_put(node);
		}
	}

	if (!private->mutex_node) {
		dev_err(dev, "Failed to find disp-mutex node\n");
		ret = -ENODEV;
		goto err_node;
	}

	platform_set_drvdata(pdev, private);

	ret = component_master_add_with_match(dev, &mtk_drm_ops, match);
	DDPINFO("%s- ret:%d\n", __func__, ret);
	if (ret)
		goto err_pm;

	mtk_fence_init();
	DDPINFO("%s-\n", __func__);

	disp_dts_gpio_init(dev, private);

	memcpy(&mydev, pdev, sizeof(mydev));

	return 0;

err_pm:
	if (private->ovlsys_dev)
		pm_runtime_disable(private->ovlsys_dev);
	if (private->side_ovlsys_dev)
		pm_runtime_disable(private->side_ovlsys_dev);
	pm_runtime_disable(dev);
	if (private->side_mmsys_dev)
		pm_runtime_disable(private->side_mmsys_dev);
err_node:
	of_node_put(private->mutex_node);
	for (i = 0; i < DDP_COMPONENT_ID_MAX; i++)
		of_node_put(private->comp_node[i]);

	sysfs_remove_bin_file(&pdev->dev.kobj, &disp_plat_dbg_buf_attr);

	return ret;
}

static void mtk_drm_shutdown(struct platform_device *pdev)
{
	struct mtk_drm_private *private = platform_get_drvdata(pdev);
	struct drm_device *drm = private->drm;

	if (drm) {
		DDPMSG("%s\n", __func__);
		drm_atomic_helper_shutdown(drm);
	}
}

static int mtk_drm_remove(struct platform_device *pdev)
{
	struct mtk_drm_private *private = platform_get_drvdata(pdev);
	struct drm_device *drm = private->drm;
	int i;

	drm_dev_unregister(drm);
	mtk_drm_kms_deinit(drm);
	drm_dev_put(drm);

	component_master_del(&pdev->dev, &mtk_drm_ops);
	if (private->ovlsys_dev)
		pm_runtime_disable(private->ovlsys_dev);
	if (private->side_ovlsys_dev)
		pm_runtime_disable(private->side_ovlsys_dev);
	pm_runtime_disable(&pdev->dev);
	if (private->side_mmsys_dev)
		pm_runtime_disable(private->side_mmsys_dev);
	of_node_put(private->mutex_node);
	for (i = 0; i < DDP_COMPONENT_ID_MAX; i++)
		of_node_put(private->comp_node[i]);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mtk_drm_sys_suspend(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm = private->drm;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	int i;
	bool wake_state = false;

	/* check each CRTC suspend already */
	for (i = 0; i < 3; i++) {
		crtc = private->crtc[i];
		if (!crtc)
			continue;
		mtk_crtc = to_mtk_crtc(crtc);
		wake_state |= mtk_crtc->wk_lock->active;
	}

	if (wake_state == false)
		goto OUT;

	DDPMSG("%s\n");
	drm_kms_helper_poll_disable(drm);

	private->suspend_state = drm_atomic_helper_suspend(drm);
	if (IS_ERR(private->suspend_state)) {
		drm_kms_helper_poll_enable(drm);
		return PTR_ERR(private->suspend_state);
	}
OUT:
#ifdef MTK_DRM_FENCE_SUPPORT
	mtk_drm_suspend_release_fence(dev);
#endif

	DRM_DEBUG_DRIVER("%s\n", __func__);
	return 0;
}

static int mtk_drm_sys_resume(struct device *dev)
{
	struct mtk_drm_private *private = dev_get_drvdata(dev);
	struct drm_device *drm;

	drm = private->drm;
	if (!drm)
		return 0;

	if (!private->suspend_state) {
		DDPINFO("suspend_state is null\n");
		return 0;
	}

	drm_atomic_helper_resume(drm, private->suspend_state);
	drm_kms_helper_poll_enable(drm);

	private->suspend_state = NULL;

	DRM_DEBUG_DRIVER("%s\n", __func__);
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mtk_drm_pm_ops, mtk_drm_sys_suspend,
			 mtk_drm_sys_resume);

static const struct of_device_id mtk_drm_of_ids[] = {
	{.compatible = "mediatek,mt2701-mmsys",
	 .data = &mt2701_mmsys_driver_data},
	{.compatible = "mediatek,mt2712-display",
	 .data = &mt2712_mmsys_driver_data},
	{.compatible = "mediatek,mt6779-mmsys",
	 .data = &mt6779_mmsys_driver_data},
	{.compatible = "mediatek,mt8173-mmsys",
	 .data = &mt8173_mmsys_driver_data},
	{.compatible = "mediatek,mt6885-mmsys",
	 .data = &mt6885_mmsys_driver_data},
	{.compatible = "mediatek,mt6893-disp",
	 .data = &mt6885_mmsys_driver_data},
	{.compatible = "mediatek,mt6983-disp",
	 .data = &mt6983_mmsys_driver_data},
	{.compatible = "mediatek,mt6985-disp",
	 .data = &mt6985_mmsys_driver_data},
	{.compatible = "mediatek,mt6895-disp",
	 .data = &mt6895_mmsys_driver_data},
	{.compatible = "mediatek,mt6886-disp",
	 .data = &mt6886_mmsys_driver_data},
	{.compatible = "mediatek,mt6873-mmsys",
	 .data = &mt6873_mmsys_driver_data},
	{.compatible = "mediatek,mt6853-mmsys",
	 .data = &mt6853_mmsys_driver_data},
	{.compatible = "mediatek,mt6833-mmsys",
	 .data = &mt6833_mmsys_driver_data},
	{.compatible = "mediatek,mt6879-mmsys",
	 .data = &mt6879_mmsys_driver_data},
	{.compatible = "mediatek,mt6855-mmsys",
	 .data = &mt6855_mmsys_driver_data},
	{} };

static struct platform_driver mtk_drm_platform_driver = {
	.probe = mtk_drm_probe,
	.remove = mtk_drm_remove,
	.shutdown = mtk_drm_shutdown,
	.driver = {

			.name = "mediatek-drm",
			.of_match_table = mtk_drm_of_ids,
			.pm = &mtk_drm_pm_ops,
		},
};

MODULE_DEVICE_TABLE(of, mtk_drm_of_ids);

static struct platform_driver *const mtk_drm_drivers[] = {
	&mtk_drm_platform_driver,
	&mtk_ddp_driver,
	&mtk_disp_tdshp_driver,
	&mtk_disp_color_driver,
	&mtk_disp_ccorr_driver,
	&mtk_disp_c3d_driver,
	&mtk_disp_gamma_driver,
	&mtk_disp_aal_driver,
	&mtk_dmdp_aal_driver,
	&mtk_disp_postmask_driver,
	&mtk_disp_dither_driver,
	&mtk_disp_chist_driver,
	&mtk_disp_ovl_driver,
	&mtk_disp_rdma_driver,
	&mtk_disp_wdma_driver,
	&mtk_disp_rsz_driver,
	&mtk_mipi_tx_driver,
	&mtk_dsi_driver,
	&mtk_dp_intf_driver,
#ifdef CONFIG_DRM_MEDIATEK_HDMI
	&mtk_dpi_driver,
	&mtk_lvds_driver,
	&mtk_lvds_tx_driver,
#endif
	&mtk_disp_cm_driver,
	&mtk_disp_spr_driver,
	&mtk_disp_postalign_driver,
	&mtk_disp_oddmr_driver,
	&mtk_disp_dsc_driver,
	&mtk_dp_tx_driver,
	&mtk_disp_y2r_driver,
	&mtk_disp_inlinerotate_driver,
	&mtk_disp_dlo_async_driver,
	&mtk_disp_dli_async_driver,
	&mtk_mmlsys_bypass_driver,
	&mtk_disp_merge_driver
};

static int __init mtk_drm_init(void)
{
	int ret;
	int i;

	DDPINFO("%s+\n", __func__);
	for (i = 0; i < ARRAY_SIZE(mtk_drm_drivers); i++) {
		DDPINFO("%s register %s driver\n",
			__func__, mtk_drm_drivers[i]->driver.name);
		ret = platform_driver_register(mtk_drm_drivers[i]);
		if (ret < 0) {
			DDPPR_ERR("Failed to register %s driver: %d\n",
				  mtk_drm_drivers[i]->driver.name, ret);
			goto err;
		}
	}
	DDPINFO("%s-\n", __func__);

	return 0;

err:
	while (--i >= 0)
		platform_driver_unregister(mtk_drm_drivers[i]);

	return ret;
}

static void __exit mtk_drm_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(mtk_drm_drivers) - 1; i >= 0; i--)
		platform_driver_unregister(mtk_drm_drivers[i]);
}
module_init(mtk_drm_init);
module_exit(mtk_drm_exit);

MODULE_AUTHOR("YT SHEN <yt.shen@mediatek.com>");
MODULE_DESCRIPTION("Mediatek SoC DRM driver");
MODULE_LICENSE("GPL v2");
