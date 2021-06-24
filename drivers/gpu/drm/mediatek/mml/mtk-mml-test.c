// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/dma-fence.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/sync_file.h>

#include "mtk-mml-drm-adaptor.h"
#include "mtk-mml-color.h"
#include "mtk-mml-core.h"

/* HOWTO:
 *	1. echo $id > sys/module/mtk_mml_test/patameters/mml_case
 *	2. run userspace UnitTest bin
 *
 * UnitTest flow
 *	1. open file "/sys/kernel/debug/mml/mml-test"
 *	2. read to struct mml_test_case, store cfg_ members.
 *	3. write struct mml_test_case with [in] members
 *
 */
struct mml_test_case {
	/* [out] config next ut */
	uint32_t cfg_src_format;
	uint32_t cfg_src_w;
	uint32_t cfg_src_h;
	uint32_t cfg_dest_format;
	uint32_t cfg_dest_w;
	uint32_t cfg_dest_h;
	uint32_t cfg_dest1_format;
	uint32_t cfg_dest1_w;
	uint32_t cfg_dest1_h;

	/* [in] */
	int32_t fd_in;
	uint32_t size_in;
	int32_t fd_out;
	uint32_t size_out;
	int32_t fd_out1;
	uint32_t size_out1;
};

static struct mml_test_case the_case;

int mml_case;
EXPORT_SYMBOL(mml_case);
module_param(mml_case, int, 0644);

int mml_test_w = 1920;
EXPORT_SYMBOL(mml_test_w);
module_param(mml_test_w, int, 0644);

int mml_test_h = 1088;
EXPORT_SYMBOL(mml_test_h);
module_param(mml_test_h, int, 0644);


struct mml_test {
	struct platform_device *pdev;
	struct device *dev;
	struct platform_device *mml_plat_dev;
	struct mml_drm_ctx *drm_ctx;
	struct dentry *fs;
};

struct test_case_op {
	void (*config)(void);
	void (*run)(struct mml_test *test, struct mml_test_case *cur);
};

static void check_fence(int32_t fd, const char *func)
{
	struct dma_fence *fence = sync_file_get_fence(fd);
	long fret = dma_fence_wait(fence, true);

	mml_log("[test]%s success fence %d %p ret %ld",
		func, fd, fence, fret);

	dma_fence_put(fence);
	put_unused_fd(fd);
}

static void case_general_submit(struct mml_test *test,
	struct mml_test_case *cur,
	void (*setup)(struct mml_submit *task, struct mml_test_case *cur))
{
	struct platform_device *mml_pdev;
	struct mml_drm_ctx *mml_ctx;
	struct mml_job job = {};
	struct mml_submit task = {.job = &job};
	const u32 src_fmt = the_case.cfg_src_format;
	const u32 dest_fmt = the_case.cfg_dest_format;
	const u32 src_w = the_case.cfg_src_w;
	const u32 src_h = the_case.cfg_src_h;
	s32 ret;

	mml_log("[test]%s begin case %d", __func__, mml_case);

	mml_pdev = mml_get_plat_device(test->pdev);
	if (!mml_pdev) {
		mml_err("get mml device failed");
		return;
	}

	mml_ctx = mml_drm_get_context(mml_pdev);
	if (!mml_ctx) {
		mml_err("get mml context failed");
		return;
	}

	task.info.src.format = src_fmt;
	task.info.src.width = src_w;
	task.info.src.height = src_h;
	task.info.src.y_stride =
		mml_color_get_min_y_stride(src_fmt, task.info.src.width);
	task.info.src.uv_stride = 0;
	task.info.src.vert_stride = 0;
	task.info.src.profile = 0;
	task.info.src.plane_cnt = MML_FMT_PLANE(task.info.src.format);
	task.info.src.secure = false;
	task.info.dest_cnt = 1;
	task.info.dest[0].data.format = dest_fmt;
	task.info.dest[0].data.width = the_case.cfg_dest_w;
	task.info.dest[0].data.height = the_case.cfg_dest_h;
	task.info.dest[0].data.y_stride =
		mml_color_get_min_y_stride(task.info.dest[0].data.format,
			task.info.dest[0].data.width);
	task.info.dest[0].data.uv_stride = 0;
	task.info.dest[0].data.vert_stride = 0;
	task.info.dest[0].data.profile = 0;
	task.info.dest[0].data.plane_cnt =
		MML_FMT_PLANE(task.info.dest[0].data.format);
	task.info.dest[0].data.secure = false;
	task.info.mode = MML_MODE_MML_DECOUPLE;
	task.info.layer_id = 0;
	task.buffer.src.fd[0] = cur->fd_in;
	task.buffer.src.size[0] = cur->size_in;
	task.buffer.src.cnt = MML_FMT_PLANE(task.info.src.format);
	task.buffer.dest[0].fd[0] = cur->fd_out;
	task.buffer.dest[0].size[0] = cur->size_out;
	task.buffer.dest[0].cnt =
		MML_FMT_PLANE(task.info.dest[0].data.format);
	task.buffer.dest_cnt = 1;

	/* trigger all invalid/flush */
	task.buffer.src.flush = true;
	task.buffer.src.invalid = true;
	task.buffer.dest[0].flush = true;
	task.buffer.dest[0].invalid = true;
	task.buffer.dest[1].flush = true;
	task.buffer.dest[1].invalid = true;

	if (setup)
		setup(&task, cur);

	if (mml_drm_query_cap(&task.info) == MML_MODE_NOT_SUPPORT) {
		mml_err("%s not support", __func__);
		return;
	}

	ret = mml_drm_submit(mml_ctx, &task);
	if (ret)
		mml_err("%s submit failed: %d", __func__, ret);
	else
		check_fence(task.job->fence, __func__);

	mml_log("%s end", __func__);
}

/* case_config_rgb/case_run_general
 * most simple test case
 *
 * format in: RGB888
 * format out: RGB888
 */
static void case_config_rgb(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_RGB888;
	the_case.cfg_dest_w = mml_test_w;
	the_case.cfg_dest_h = mml_test_h;
}

static void case_run_general(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, NULL);
}

/* case_config_rgb_rot/setup_rot/case_run_rgb_rot
 * test rbg image with rotate
 *
 * format in: RGB888
 * format out: RGB888
 * rotate: 90
 */
static void case_config_rgb_rot(void)
{
	case_config_rgb();
	swap(the_case.cfg_dest_w, the_case.cfg_dest_h);
}

static void setup_rot(struct mml_submit *task, struct mml_test_case *cur)
{
	task->info.dest[0].rotate = MML_ROT_90;
}

static void case_run_rgb_rot(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_rot);
}


/* case_config_rgb_compose/setup_rgb_compose/case_run_rgb_compose
 * compose feature test
 *
 * format in: RGB888
 * format out: RGB888
 * compose: (50, 16)
 */

#define CASE_RGB_LEFT	50
#define CASE_RGB_TOP	16

static void case_config_rgb_compose(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_RGB888;
	the_case.cfg_dest_w = mml_test_w + CASE_RGB_LEFT;
	the_case.cfg_dest_h = mml_test_h + CASE_RGB_TOP;
}

static void setup_rgb_compose(struct mml_submit *task,
	struct mml_test_case *cur)
{
	task->info.dest[0].data.width = mml_test_w;
	task->info.dest[0].data.height = mml_test_h;
	task->info.dest[0].crop.r.width = mml_test_w;
	task->info.dest[0].crop.r.height = mml_test_h;
	task->info.dest[0].compose.left = CASE_RGB_LEFT;
	task->info.dest[0].compose.top = CASE_RGB_TOP;
	task->info.dest[0].compose.width = the_case.cfg_dest_w;
	task->info.dest[0].compose.height = the_case.cfg_dest_h;
	task->info.dest[0].flip = true;
}

static void case_run_rgb_compose(struct mml_test *test,
	struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_rgb_compose);
}

/* setup_relay/case_run_relay - enable pq engine to test relay mode
 *
 * format in: RGB888
 * format out: RGB888
 */
static void case_config_relay_crop(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_RGB888;
	the_case.cfg_dest_w = mml_test_w;
	the_case.cfg_dest_h = mml_test_h;
}

static void setup_relay(struct mml_submit *task, struct mml_test_case *cur)
{
	task->info.dest[0].pq_config.en = true;
	task->info.dest[0].pq_config.en_dre = true;
	task->info.dest[0].crop.r.left = 0;
	task->info.dest[0].crop.r.top = 0;
	task->info.dest[0].crop.r.width = the_case.cfg_dest_w;
	task->info.dest[0].crop.r.height = the_case.cfg_dest_h;
}

static void case_run_relay(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_relay);
}

/* case_config_rsz_up2
 * resize to up scale x2
 *
 * format in: RGB888
 * format out: RGB888
 * scale: 2
 */
static void case_config_rsz_up2(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_RGB888;
	the_case.cfg_dest_w = mml_test_w * 2;
	the_case.cfg_dest_h = mml_test_h * 2;
}

/* case_config_nv12 / setup_nv12 / setup_nv12
 * test format nv12
 *
 * format in: NV12 (YUV420/2 plane)
 * format out: NV12 (YUV420/2 plane)
 */
static void case_config_nv12(void)
{
	the_case.cfg_src_format = MML_FMT_NV12;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_NV12;
	the_case.cfg_dest_w = mml_test_w;
	the_case.cfg_dest_h = mml_test_h;
}

static void setup_nv12(struct mml_submit *task, struct mml_test_case *cur)
{
	task->info.src.uv_stride = mml_color_get_min_uv_stride(
		the_case.cfg_src_format, the_case.cfg_src_w);

	task->buffer.src.fd[0] = cur->fd_in;
	task->buffer.src.size[0] = mml_color_get_min_y_size(
		the_case.cfg_src_format,
		the_case.cfg_src_w, the_case.cfg_src_h);

	task->info.src.plane_offset[1] = task->buffer.src.size[0];

	task->buffer.src.fd[1] = cur->fd_in;
	task->buffer.src.size[1] = mml_color_get_min_uv_size(
		the_case.cfg_src_format,
		the_case.cfg_src_w, the_case.cfg_src_h);

	if (task->buffer.src.size[0] + task->buffer.src.size[1] !=
		cur->size_in)
		mml_err("%s case %d src size total %u plane %u %u",
			__func__, mml_case, cur->size_in,
			task->buffer.src.size[0] + task->buffer.src.size[1]);

	/* setup dest 0 with 2 plane */
	task->info.dest[0].data.uv_stride = mml_color_get_min_uv_stride(
		the_case.cfg_dest_format, the_case.cfg_dest_w);

	task->buffer.dest[0].fd[0] = cur->fd_out;
	task->buffer.dest[0].size[0] = mml_color_get_min_y_size(
		the_case.cfg_dest_format,
		the_case.cfg_dest_w, the_case.cfg_dest_h);

	task->info.dest[0].data.plane_offset[1] = task->buffer.dest[0].size[0];

	task->buffer.dest[0].fd[1] = cur->fd_out;
	task->buffer.dest[0].size[1] = mml_color_get_min_uv_size(
		the_case.cfg_dest_format,
		the_case.cfg_dest_w, the_case.cfg_dest_h);

	if (task->buffer.dest[0].size[0] + task->buffer.dest[0].size[1] !=
		cur->size_out)
		mml_err("%s case %d dest size total %u plane %u %u",
			__func__, mml_case, cur->size_out,
			task->buffer.dest[0].size[0] + task->buffer.dest[0].size[1]);
}

static void case_run_nv12(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_nv12);
}

/* case_config_yuyv_down2
 * test format yuyv (1 plane yuv422)
 *
 * format in: YUYV
 * format out: YUYV
 */
static void case_config_yuyv_down2(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_YUYV;
	the_case.cfg_dest_w = mml_test_w / 2;
	the_case.cfg_dest_h = mml_test_h / 2;
}

/* case_config_block_to_nv12
 * test format block to nv12
 *
 * format in: block 420
 * format out: NV12
 */
static void case_config_block_to_nv12(void)
{
	the_case.cfg_src_format = MML_FMT_BLK;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_NV12;
	the_case.cfg_dest_w = mml_test_w;
	the_case.cfg_dest_h = mml_test_h;
}

static void setup_block_to_nv12(struct mml_submit *task,
	struct mml_test_case *cur)
{
	task->info.src.uv_stride = mml_color_get_min_uv_stride(
		the_case.cfg_src_format, the_case.cfg_src_w);

	task->buffer.src.fd[0] = cur->fd_in;
	task->buffer.src.size[0] = mml_color_get_min_y_size(
		the_case.cfg_src_format,
		the_case.cfg_src_w, the_case.cfg_src_h);

	task->info.src.plane_offset[1] = task->buffer.src.size[0];

	task->buffer.src.fd[1] = cur->fd_in;
	task->buffer.src.size[1] = mml_color_get_min_uv_size(
		the_case.cfg_src_format,
		the_case.cfg_src_w, the_case.cfg_src_h);

	/* setup dest 0 with 2 plane */
	task->info.dest[0].data.uv_stride = mml_color_get_min_uv_stride(
		the_case.cfg_dest_format, the_case.cfg_dest_w);

	task->buffer.dest[0].fd[0] = cur->fd_out;
	task->buffer.dest[0].size[0] = mml_color_get_min_y_size(
		the_case.cfg_dest_format,
		the_case.cfg_dest_w, the_case.cfg_dest_h);

	task->info.dest[0].data.plane_offset[1] = task->buffer.dest[0].size[0];

	task->buffer.dest[0].fd[1] = cur->fd_out;
	task->buffer.dest[0].size[1] = mml_color_get_min_uv_size(
		the_case.cfg_dest_format,
		the_case.cfg_dest_w, the_case.cfg_dest_h);

	if (task->buffer.dest[0].size[0] + task->buffer.dest[0].size[1] !=
		cur->size_out)
		mml_err("%s case %d dest size total %u plane %u %u",
			__func__, mml_case, cur->size_out,
			task->buffer.dest[0].size[0] + task->buffer.dest[0].size[1]);
}

static void case_run_block_to_nv12(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_block_to_nv12);
}

/* case_config_afbc / setup_afbc / case_run_afbc
 * rgb to afbc. setup compose since afbc format size 32x32 block
 *
 * format in: RGB888
 * format out: AFBC_RGBA8888
 */
#define mml_afbc_align(p) (((p + 31) >> 5) << 5)

static void case_config_afbc(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_RGBA8888_AFBC;
	the_case.cfg_dest_w = mml_afbc_align(mml_test_w);
	the_case.cfg_dest_h = mml_afbc_align(mml_test_h);
}

static void setup_afbc(struct mml_submit *task, struct mml_test_case *cur)
{
	task->info.dest[0].compose.left = 0;
	task->info.dest[0].compose.top = 0;
	task->info.dest[0].compose.width = mml_test_w;
	task->info.dest[0].compose.height = mml_test_h;
	task->info.dest[0].data.width = mml_test_w;
	task->info.dest[0].data.height = mml_test_h;
	task->info.dest[0].data.vert_stride =
		mml_afbc_align(task->info.dest[0].data.height);
}

static void case_run_afbc(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_afbc);
}

/* case_config_afbc_to_rgb
 * afbc (from last case) to rgb. Source size align 32x32 and
 * this case crop content roi, which is height 1080.
 *
 * format in: AFBC_RGBA8888
 * format out: RGB888
 */
static void case_config_afbc_to_rgb(void)
{
	the_case.cfg_src_format = MML_FMT_RGBA8888_AFBC;
	the_case.cfg_src_w = mml_afbc_align(mml_test_w);
	the_case.cfg_src_h = mml_afbc_align(mml_test_h);
	the_case.cfg_dest_format = MML_FMT_RGB888;
	the_case.cfg_dest_w = mml_test_w;
	the_case.cfg_dest_h = mml_test_h;
}

static void setup_afbc_to_rgb(struct mml_submit *task,
	struct mml_test_case *cur)
{
	task->info.dest[0].crop.r.left = 0;
	task->info.dest[0].crop.r.top = 0;
	task->info.dest[0].crop.r.width = mml_test_w;
	task->info.dest[0].crop.r.height = mml_test_h;
}

static void case_run_afbc_to_rgb(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_afbc_to_rgb);
}

/* case_config_2out
 * 1 in 2 out + resize
 *
 * format in: RGB888
 * format out0: NV12
 * format out1: RGB resize 256x256
 */
static void case_config_2out(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_NV12;
	the_case.cfg_dest_w = mml_test_w;
	the_case.cfg_dest_h = mml_test_h;
	the_case.cfg_dest1_format = MML_FMT_RGB888;
	the_case.cfg_dest1_w = mml_test_w / 2;
	the_case.cfg_dest1_h = mml_test_h / 2;
}

static void setup_2out(struct mml_submit *task, struct mml_test_case *cur)
{
	/* config dest[0] uv plane nv12 */
	task->info.dest[0].data.uv_stride = mml_color_get_min_uv_stride(
		the_case.cfg_dest_format, the_case.cfg_dest_w);
	task->info.dest[0].data.plane_offset[1] = mml_color_get_min_y_size(
		the_case.cfg_dest_format,
		the_case.cfg_dest_w, the_case.cfg_dest_h);

	/* config dest[1] */
	task->info.dest_cnt = 2;
	task->info.dest[1].data.format = the_case.cfg_dest1_format;
	task->info.dest[1].data.width = the_case.cfg_dest1_w;
	task->info.dest[1].data.height = the_case.cfg_dest1_h;
	task->info.dest[1].data.y_stride =
		mml_color_get_min_y_stride(the_case.cfg_dest1_format,
			task->info.dest[1].data.width);
	task->info.dest[1].data.uv_stride = 0;
	task->info.dest[1].data.vert_stride = 0;
	task->info.dest[1].data.profile = 0;
	task->info.dest[1].data.secure = false;
	task->info.mode = MML_MODE_MML_DECOUPLE;
	task->info.layer_id = 0;
	task->buffer.dest[1].fd[0] = cur->fd_out1;
	task->buffer.dest[1].size[0] = cur->size_out1;
	task->buffer.dest[1].cnt =
		MML_FMT_PLANE(task->info.dest[1].data.format);
	task->buffer.dest_cnt = 2;
}

static void case_run_2out(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_2out);
}

/* case_config_2out_crop
 * 1 in 2 out + resize
 *
 * format in: RGB888
 * format out0: NV12
 * format out1: RGB resize 256x256 + crop
 */
static void case_config_2out_crop(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_NV12;
	the_case.cfg_dest_w = mml_test_w;
	the_case.cfg_dest_h = mml_test_h;
	the_case.cfg_dest1_format = MML_FMT_RGB888;
	the_case.cfg_dest1_w = mml_test_w / 2;
	the_case.cfg_dest1_h = mml_test_h / 2;
}

static void setup_2out_crop(struct mml_submit *task, struct mml_test_case *cur)
{
	/* config dest[0] uv plane nv12 */
	task->info.dest[0].data.uv_stride = mml_color_get_min_uv_stride(
		the_case.cfg_dest_format, the_case.cfg_dest_w);
	task->info.dest[0].data.plane_offset[1] = mml_color_get_min_y_size(
		the_case.cfg_dest_format,
		the_case.cfg_dest_w, the_case.cfg_dest_h);

	/* config dest[1] */
	task->info.dest_cnt = 2;
	task->info.dest[1].data.format = the_case.cfg_dest1_format;
	task->info.dest[1].data.width = the_case.cfg_dest1_w;
	task->info.dest[1].data.height = the_case.cfg_dest1_h;
	task->info.dest[1].data.y_stride =
		mml_color_get_min_y_stride(the_case.cfg_dest1_format,
			task->info.dest[1].data.width);
	task->info.dest[1].data.uv_stride = 0;
	task->info.dest[1].data.vert_stride = 0;
	task->info.dest[1].data.profile = 0;
	task->info.dest[1].data.plane_cnt =
		MML_FMT_PLANE(task->info.dest[1].data.format);;
	task->info.dest[1].data.secure = false;
	task->info.mode = MML_MODE_MML_DECOUPLE;
	task->info.layer_id = 0;
	task->buffer.dest[1].fd[0] = cur->fd_out1;
	task->buffer.dest[1].size[0] = cur->size_out1;
	task->buffer.dest[1].cnt =
		MML_FMT_PLANE(task->info.dest[1].data.format);
	task->buffer.dest_cnt = 2;

	/* config dest[1] crop */
	task->info.dest[1].crop.r.left = mml_test_w / 4;
	task->info.dest[1].crop.r.top = mml_test_w / 4;
	task->info.dest[1].crop.r.width = task->info.dest[1].data.width;
	task->info.dest[1].crop.r.height = task->info.dest[1].data.height;
}

static void case_run_2out_crop(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_2out_crop);
}

/* case_config_2out_crop_compose
 * 1 in 2 out + resize + compose
 *
 * format in: RGB888
 * format out0: NV12
 * format out1: RGB resize 256x256 + crop + compose
 */
#define CASE_CROP_W	256
#define CASE_CROP_H	256

static void case_config_2out_crop_compose(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_NV12;
	the_case.cfg_dest_w = mml_test_w;
	the_case.cfg_dest_h = mml_test_h;
	the_case.cfg_dest1_format = MML_FMT_RGB888;
	/* still use full size cause compose */
	the_case.cfg_dest1_w = mml_test_w;
	the_case.cfg_dest1_h = mml_test_h;
}

#define central(out, in) (out / 2 - in / 2)

static void setup_2out_crop_compose(struct mml_submit *task,
	struct mml_test_case *cur)
{
	/* config dest[0] uv plane nv12 */
	task->info.dest[0].data.uv_stride = mml_color_get_min_uv_stride(
		the_case.cfg_dest_format, the_case.cfg_dest_w);
	task->info.dest[0].data.plane_offset[1] = mml_color_get_min_y_size(
		the_case.cfg_dest_format,
		the_case.cfg_dest_w, the_case.cfg_dest_h);

	/* config dest[1] */
	task->info.dest_cnt = 2;
	task->info.dest[1].data.format = the_case.cfg_dest1_format;
	task->info.dest[1].data.width = CASE_CROP_W;
	task->info.dest[1].data.height = CASE_CROP_H;
	task->info.dest[1].data.y_stride =
		mml_color_get_min_y_stride(the_case.cfg_dest1_format,
			task->info.dest[1].data.width);
	task->info.dest[1].data.uv_stride = 0;
	task->info.dest[1].data.vert_stride = 0;
	task->info.dest[1].data.profile = 0;
	task->info.dest[1].data.plane_cnt =
		MML_FMT_PLANE(task->info.dest[1].data.format);;
	task->info.dest[1].data.secure = false;
	task->info.mode = MML_MODE_MML_DECOUPLE;
	task->info.layer_id = 0;
	task->buffer.dest[1].fd[0] = cur->fd_out1;
	task->buffer.dest[1].size[0] = cur->size_out1;
	task->buffer.dest[1].cnt =
		MML_FMT_PLANE(task->info.dest[1].data.format);
	task->buffer.dest_cnt = 2;

	/* config dest[1] crop */
	task->info.dest[1].crop.r.left = central(mml_test_w, CASE_CROP_W);
	task->info.dest[1].crop.r.top = central(mml_test_h, CASE_CROP_H);
	task->info.dest[1].crop.r.width = task->info.dest[1].data.width;
	task->info.dest[1].crop.r.height = task->info.dest[1].data.height;
	task->info.dest[1].compose.left = central(mml_test_w, CASE_CROP_W);
	task->info.dest[1].compose.top = central(mml_test_h, CASE_CROP_H);
	task->info.dest[1].compose.width = mml_test_w;
	task->info.dest[1].compose.height = mml_test_h;
}

static void case_run_2out_crop_compose(struct mml_test *test,
	struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_2out_crop_compose);
}

/* case_config_crop_offset / setup_crop / case_run_crop
 *
 * format in: RGB888
 * format out0: RGB888 crop
 */
#define RGB_CROP_OFF_W	(mml_test_w / 2)
#define RGB_CROP_OFF_H	(mml_test_h / 2)

static void case_config_crop(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_RGB888;
	the_case.cfg_dest_w = RGB_CROP_OFF_W;
	the_case.cfg_dest_h = RGB_CROP_OFF_H;
}

static void setup_crop(struct mml_submit *task, struct mml_test_case *cur)
{
	task->info.dest[0].crop.r.left = central(mml_test_w, RGB_CROP_OFF_W);
	task->info.dest[0].crop.r.top = central(mml_test_h, RGB_CROP_OFF_H);
	task->info.dest[0].crop.r.width = RGB_CROP_OFF_W;
	task->info.dest[0].crop.r.height = RGB_CROP_OFF_H;
}

static void case_run_crop(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_crop);
}

enum mml_ut_case {
	MML_UT_RGB,		/* 0 */
	MML_UT_RGB_ROTATE,	/* 1 */
	MML_UT_COMPOSE_FLIP,	/* 2 */
	MML_UT_RESIZE_RELAY,	/* 3 */
	MML_UT_RESIZE_UP2,	/* 4 */
	MML_UT_NV12,		/* 5 */
	MML_UT_YUYV_DOWN2,	/* 6 */
	MML_UT_BLOCK_TO_NV12,	/* 7 */
	MML_UT_AFBC,		/* 8 */
	MML_UT_AFBC_TO_RGB,	/* 9 */
	MML_UT_2OUT,		/* 10 */
	MML_UT_2OUT_CROP,	/* 11 */
	MML_UT_2OUT_RCC,	/* 12 */
	MML_UT_CROP,		/* 13 */
	MML_UT_TOTAL
};

static struct test_case_op cases[MML_UT_TOTAL] = {
	[MML_UT_RGB] = {
		.config = case_config_rgb,
		.run = case_run_general,
	},
	[MML_UT_RGB_ROTATE] = {
		.config = case_config_rgb_rot,
		.run = case_run_rgb_rot,
	},
	[MML_UT_COMPOSE_FLIP] = {
		.config = case_config_rgb_compose,
		.run = case_run_rgb_compose,
	},
	[MML_UT_RESIZE_RELAY] = {
		.config = case_config_relay_crop,
		.run = case_run_relay,
	},
	[MML_UT_RESIZE_UP2] = {
		.config = case_config_rsz_up2,
		.run = case_run_general,
	},
	[MML_UT_NV12] = {
		.config = case_config_nv12,
		.run = case_run_nv12,
	},
	[MML_UT_YUYV_DOWN2] = {
		.config = case_config_yuyv_down2,
		.run = case_run_general,
	},
	[MML_UT_BLOCK_TO_NV12] = {
		.config = case_config_block_to_nv12,
		.run = case_run_block_to_nv12,
	},
	[MML_UT_AFBC] = {
		.config = case_config_afbc,
		.run = case_run_afbc,
	},
	[MML_UT_AFBC_TO_RGB] = {
		.config = case_config_afbc_to_rgb,
		.run = case_run_afbc_to_rgb,
	},
	[MML_UT_2OUT] = {
		.config = case_config_2out,
		.run = case_run_2out,
	},
	[MML_UT_2OUT_CROP] = {
		.config = case_config_2out_crop,
		.run = case_run_2out_crop,
	},
	[MML_UT_2OUT_RCC] = {
		.config = case_config_2out_crop_compose,
		.run = case_run_2out_crop_compose,
	},
	[MML_UT_CROP] = {
		.config = case_config_crop,
		.run = case_run_crop,
	},
};

static ssize_t test_read(struct file *filep, char __user *buf, size_t size,
	loff_t *offset)
{
	u32 len = sizeof(the_case);

	if (size < sizeof(the_case)) {
		mml_err("[test] buf size not match %zu %u",
			sizeof(the_case), len);
		return -EFAULT;
	}

	memset(&the_case, 0, sizeof(the_case));

	if (mml_case < ARRAY_SIZE(cases) && cases[mml_case].config)
		cases[mml_case].config();
	else
		mml_err("[test]no such case %d", mml_case);

	copy_to_user(buf, &the_case, len);
	*offset += len;

	mml_log("[test]%s format src %#010x dest %#010x size %u stride %u",
		__func__, the_case.cfg_src_format, the_case.cfg_dest_format);

	return 0;
}

static ssize_t test_write(struct file *filp, const char *buf, size_t count,
	loff_t *offp)
{
	struct mml_test *test = (struct mml_test *)filp->f_inode->i_private;
	struct mml_test_case cur;

	if (copy_from_user(&cur, buf, count)) {
		mml_err("copy_from_user failed len:%zu", count);
		return -EFAULT;
	}

	if (mml_case < ARRAY_SIZE(cases) && cases[mml_case].run)
		cases[mml_case].run(test, &cur);

	return count;
}

static const struct file_operations test_fops = {
	.read = test_read,
	.write = test_write,
};

static int probe(struct platform_device *pdev)
{
	struct mml_test *test;
	struct dentry *dir;

	mml_log("mml-test %s begin", __func__);
	test = devm_kzalloc(&pdev->dev, sizeof(*test), GFP_KERNEL);
	if (!test)
		return -ENOMEM;
	test->pdev = pdev;
	test->dev = &pdev->dev;

	dir = debugfs_create_dir("mml", NULL);
	if (IS_ERR(dir) && PTR_ERR(dir) != -EEXIST) {
		mml_err("debugfs_create_dir mml failed:%ld", PTR_ERR(dir));
		return PTR_ERR(dir);
	}

	test->fs = debugfs_create_file(
		"mml-test", 0444, dir, test, &test_fops);
	if (IS_ERR(test->fs)) {
		mml_err("debugfs_create_file mml-test failed:%ld",
			PTR_ERR(test->fs));
		return PTR_ERR(test->fs);
	}

	platform_set_drvdata(pdev, test);
	mml_log("debugfs_create_file mml-test success");

	return 0;
}

static int remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id test_of_ids[] = {
	{
		.compatible = "mediatek,mml-test",
	},
	{}
};
MODULE_DEVICE_TABLE(of, test_of_ids);

struct platform_driver mtk_mml_test_drv = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mml_test",
		.of_match_table = test_of_ids,
	},
};
