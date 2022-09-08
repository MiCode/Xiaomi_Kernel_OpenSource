// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/dma-fence.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>
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

unsigned int mml_case;
module_param(mml_case, uint, 0644);

/* how many submit for each ut */
int mml_test_round = 1;
module_param(mml_test_round, int, 0644);

/* interval for each test run, in ms */
int mml_test_interval = 16;
module_param(mml_test_interval, int, 0644);


int mml_test_w = 1920;
module_param(mml_test_w, int, 0644);

int mml_test_h = 1088;
module_param(mml_test_h, int, 0644);

int mml_test_out_w = 1920;
module_param(mml_test_out_w, int, 0644);

int mml_test_out_h = 1088;
module_param(mml_test_out_h, int, 0644);

int mml_test_crop_left;
module_param(mml_test_crop_left, int, 0644);

int mml_test_crop_top;
module_param(mml_test_crop_top, int, 0644);

int mml_test_crop_width = 1920;
module_param(mml_test_crop_width, int, 0644);

int mml_test_crop_height = 1088;
module_param(mml_test_crop_height, int, 0644);

int mml_test_rot;
module_param(mml_test_rot, int, 0644);

int mml_test_flip;
module_param(mml_test_flip, int, 0644);

int mml_test_pq;
module_param(mml_test_pq, int, 0644);

int mml_test_in_fmt = MML_FMT_RGB888;
module_param(mml_test_in_fmt, int, 0644);

int mml_test_out_fmt = MML_FMT_RGB888;
module_param(mml_test_out_fmt, int, 0644);

struct mml_test {
	struct platform_device *pdev;
	struct device *dev;
	struct platform_device *mml_plat_dev;
	struct mml_drm_ctx *drm_ctx;
	struct dentry *fs;
	struct dentry *fs_inst;
	struct dentry *fs_frame_in;
	struct dentry *fs_frame_out;
};

struct test_case_op {
	void (*config)(void);
	void (*run)(struct mml_test *test, struct mml_test_case *cur);
};

#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
static void check_fence(int32_t fd, const char *func)
{
	struct dma_fence *fence = sync_file_get_fence(fd);
	long fret;

	if (unlikely(!fence)) {
		mml_err("sync_file_get_fence return null");
		return;
	}

	fret = dma_fence_wait(fence, true);

	mml_log("[test]%s success fence %d %p ret %ld",
		func, fd, fence, fret);

	dma_fence_put(fence);
	put_unused_fd(fd);
}
#endif

#define mml_afbc_align(p) (((p + 31) >> 5) << 5)

static void fillin_info_data(u32 format, u32 width, u32 height,
	struct mml_frame_data *data)
{
	data->format = format;
	data->width = width;
	data->height = height;
	data->plane_cnt = MML_FMT_PLANE(data->format);
	data->y_stride = mml_color_get_min_y_stride(data->format, data->width);
	if (data->plane_cnt >= 2)
		data->uv_stride = mml_color_get_min_uv_stride(
			data->format, data->width);
	data->vert_stride = mml_afbc_align(height);
	data->profile = 0;
	data->secure = false;
}

static void fillin_buf(struct mml_frame_data *data, s32 fd_out, u32 fd_size,
	struct mml_buffer *buf)
{
	buf->cnt = MML_FMT_PLANE(data->format);
	buf->fd[0] = fd_out;
	buf->size[0] = fd_size;
	if (buf->cnt >= 2) {
		buf->size[0] = mml_color_get_min_y_size(
			data->format, data->width, data->height);
		buf->fd[1] = fd_out;
		buf->size[1] = mml_color_get_min_uv_size(
			data->format, data->width, data->height);
	}
	if (buf->cnt >= 3) {
		buf->fd[2] = fd_out;
		buf->size[2] = mml_color_get_min_uv_size(
			data->format, data->width, data->height);
	}
}

static void case_general_submit(struct mml_test *test,
	struct mml_test_case *cur,
	void (*setup)(struct mml_submit *task, struct mml_test_case *cur))
{
#if IS_ENABLED(CONFIG_ARCH_DMA_ADDR_T_64BIT)
	struct platform_device *mml_pdev;
	struct mml_drm_ctx *mml_ctx;
	struct mml_job job = {};
	struct mml_pq_param pq_param = {};
	struct mml_submit task = {.job = &job};
	u32 run_cnt = mml_test_round <= 0 ? 1 : (u32)mml_test_round;
	struct mml_drm_param disp = {
		.vdo_mode = true,
	};
	const u32 max_running = 10;
	int *fences;
	u32 i;
	s32 ret;
	int8_t mode;

	mml_log("[test]%s begin case %d", __func__, mml_case);

	mml_pdev = mml_get_plat_device(test->pdev);
	if (!mml_pdev) {
		mml_err("get mml device failed");
		return;
	}

	mml_ctx = mml_drm_get_context(mml_pdev, &disp);
	if (!mml_ctx) {
		mml_err("get mml context failed");
		return;
	}

	/* srouce info and buffer */
	fillin_info_data(the_case.cfg_src_format,
		the_case.cfg_src_w, the_case.cfg_src_h,
		&task.info.src);
	fillin_buf(&task.info.src, cur->fd_in, cur->size_in, &task.buffer.src);

	/* destination info and buffer */
	fillin_info_data(the_case.cfg_dest_format,
		the_case.cfg_dest_w, the_case.cfg_dest_h,
		&task.info.dest[0].data);
	fillin_buf(&task.info.dest[0].data, cur->fd_out, cur->size_out,
		&task.buffer.dest[0]);

	task.info.dest_cnt = 1;
	task.info.mode = MML_MODE_MML_DECOUPLE;
	task.info.layer_id = 0;
	task.buffer.dest_cnt = 1;

	/* trigger all invalid/flush */
	task.buffer.src.flush = true;
	task.buffer.src.invalid = true;
	task.buffer.src.fence = -1;
	task.buffer.dest[0].flush = true;
	task.buffer.dest[0].invalid = true;
	task.buffer.dest[0].fence = -1;
	task.buffer.dest[1].flush = true;
	task.buffer.dest[1].invalid = true;
	task.buffer.dest[1].fence = -1;

	if (mml_test_pq == 1) {
		pq_param.enable = 1;
		pq_param.scenario = MML_PQ_MEDIA_VIDEO;
		task.pq_param[0] = &pq_param;
		task.info.dest[0].pq_config.en = 1;
		task.info.dest[0].pq_config.en_dre = 1;
	}

	if (setup)
		setup(&task, cur);
	mode = task.info.mode;

	mml_drm_try_frame(mml_ctx, &task.info);
	if (mml_drm_query_cap(mml_ctx, &task.info) == MML_MODE_NOT_SUPPORT) {
		mml_err("%s not support", __func__);
		return;
	}

	/* for ut do not fall to inline rotate unless force use */
	if (mode == MML_MODE_RACING || mode == MML_MODE_SRAM_READ)
		task.info.mode = mode;
	else
		task.info.mode = MML_MODE_MML_DECOUPLE;

	if (mode == MML_MODE_RACING) {
		struct mml_submit nouse_submit = {0};

		mml_drm_split_info(&task, &nouse_submit);
	}

	fences = kcalloc(run_cnt, sizeof(*fences), GFP_KERNEL);
	for (i = 0; i < run_cnt; i++) {
		ktime_get_real_ts64((struct timespec64 *)&task.end);
		timespec64_add_ns((struct timespec64 *)&task.end,
			mml_test_interval * 1000000);
		ret = mml_drm_submit(mml_ctx, &task, NULL);
		if (ret) {
			mml_err("%s submit failed: %d round: %u",
				__func__, ret, i);
			fences[i] = -1;
		} else {
			fences[i] = task.job->fence;
		}
		msleep_interruptible(mml_test_interval);
		if (i > max_running && fences[i-max_running] >= 0) {
			check_fence(fences[i-max_running], __func__);
			fences[i-max_running] = -1;
		}

		if (mml_racing_ut == 2 || mml_racing_ut == 3)
			mml_drm_stop(mml_ctx, &task, false);
	}

	for (i = 0; i < run_cnt; i++) {
		if (fences[i] >= 0)
			check_fence(fences[i], __func__);
	}

	kfree(fences);
	for (i = 0; i < 5 && !mml_drm_ctx_idle(mml_ctx); i++) {
		mml_log("wait for ctx idle...");
		msleep_interruptible(1000);	/* make sure mml stops */
	}
	if (mml_drm_ctx_idle(mml_ctx))
		mml_drm_put_context(mml_ctx);
	else
		mml_err("fail to put ctx");
	mml_log("%s end", __func__);
#endif
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
	/* check src with 2 plane size */
	if (task->buffer.src.size[0] + task->buffer.src.size[1] !=
		cur->size_in)
		mml_err("%s case %d src size total %u plane %u %u",
			__func__, mml_case, cur->size_in,
			task->buffer.src.size[0], task->buffer.src.size[1]);

	/* check dest 0 with 2 plane size */
	if (task->buffer.dest[0].size[0] + task->buffer.dest[0].size[1] !=
		cur->size_out)
		mml_err("%s case %d dest size total %u plane %u %u",
			__func__, mml_case, cur->size_out,
			task->buffer.dest[0].size[0], task->buffer.dest[0].size[1]);
}

static void case_run_nv12(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_nv12);
}

/* case_config_yuyv_down2
 * test format yuyv (1 plane yuv422)
 *
 * format in: RGB888
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
	/* check dest 0 with 2 plane size */
	if (task->buffer.dest[0].size[0] + task->buffer.dest[0].size[1] !=
		cur->size_out)
		mml_err("%s case %d dest size total %u plane %u %u",
			__func__, mml_case, cur->size_out,
			task->buffer.dest[0].size[0], task->buffer.dest[0].size[1]);
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

/* case_config_yuv_afbc_to_rgb
 * yuv afbc (from last case) to rgb. Source size align 16x16 and
 * this case crop content roi, which is height 640.
 *
 * format in: NV12_AFBC_RGBA8888
 * format out: RGB888
 */
#define mml_yuv_afbc_align(p) (((p + 15) >> 4) << 4)

static void case_config_yuv_afbc_to_rgb(void)
{
	the_case.cfg_src_format = MML_FMT_YUV420_AFBC;
	the_case.cfg_src_w = mml_yuv_afbc_align(mml_test_w);
	the_case.cfg_src_h = mml_yuv_afbc_align(mml_test_h);
	the_case.cfg_dest_format = MML_FMT_RGB888;
	the_case.cfg_dest_w = mml_test_w;
	the_case.cfg_dest_h = mml_test_h;
}

static void case_config_yuv_afbc_10_to_rgb(void)
{
	the_case.cfg_src_format = MML_FMT_YUV420_10P_AFBC;
	the_case.cfg_src_w = mml_yuv_afbc_align(mml_test_w);
	the_case.cfg_src_h = mml_yuv_afbc_align(mml_test_h);
	the_case.cfg_dest_format = MML_FMT_RGB888;
	the_case.cfg_dest_w = mml_test_w;
	the_case.cfg_dest_h = mml_test_h;
}

static void setup_yuv_afbc_to_rgb(struct mml_submit *task,
	struct mml_test_case *cur)
{
	task->info.dest[0].crop.r.left = 0;
	task->info.dest[0].crop.r.top = 0;
	task->info.dest[0].crop.r.width = mml_test_w;
	task->info.dest[0].crop.r.height = mml_test_h;
}

static void case_run_yuv_afbc_to_rgb(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_yuv_afbc_to_rgb);
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
	the_case.cfg_dest1_w = 256;
	the_case.cfg_dest1_h = 256;
}

static void setup_2out(struct mml_submit *task, struct mml_test_case *cur)
{
	/* config dest[1] info data and buf */
	task->info.dest_cnt = 2;
	task->buffer.dest_cnt = 2;
	fillin_info_data(the_case.cfg_dest1_format,
		the_case.cfg_dest1_w, the_case.cfg_dest1_h,
		&task->info.dest[1].data);
	fillin_buf(&task->info.dest[1].data, cur->fd_out1, cur->size_out1,
		&task->buffer.dest[1]);
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
	setup_2out(task, cur);

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
	setup_2out(task, cur);

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

/* case_config_yv12_yuyv/setup_nv12_yuyv/case_run_nv12_yuyv
 *
 * format in: MML_FMT_YV12
 * format out: MML_FMT_YUYV
 */
static void case_config_yv12_yuyv(void)
{
	the_case.cfg_src_format = MML_FMT_YV12;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_YUYV;
	the_case.cfg_dest_w = mml_test_w;
	the_case.cfg_dest_h = mml_test_h;
}

static void setup_yv12_yuyv(struct mml_submit *task, struct mml_test_case *cur)
{
	if (task->buffer.src.size[0] + task->buffer.src.size[1]
		+ task->buffer.src.size[2] !=
		cur->size_in)
		mml_err("%s case %d src size total %u plane %u %u",
			__func__, mml_case, cur->size_in,
			task->buffer.src.size[0], task->buffer.src.size[1]);
}

static void case_run_yv12_yuyv(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_yv12_yuyv);
}

/* setup_write_sram/case_run_write_sram
 *
 * format in: MML_FMT_RGB888
 * format out: MML_FMT_RGB888
 */
static void setup_write_sram(struct mml_submit *task, struct mml_test_case *cur)
{
	task->info.mode = MML_MODE_RACING;
	task->info.dest[0].crop.r.left = 0;
	task->info.dest[0].crop.r.top = 0;
	task->info.dest[0].crop.r.width = task->info.src.width;
	task->info.dest[0].crop.r.height = task->info.src.height;
	task->buffer.dest[0].flush = false;
	task->buffer.dest[0].invalid = false;
	task->buffer.dest[0].fd[0] = -1;
	task->info.dest[0].rotate = mml_test_rot;
	task->info.dest[0].flip = mml_test_flip;
}

static void case_run_write_sram(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_write_sram);
}

/* case_config_read_sram / setup_read_sram
 *
 * format in: MML_FMT_RGB888
 * format out: MML_FMT_RGB888
 */
#define SRAM_HEIGHT	64
#define SRAM_SIZE	(512 * 1024)

static void case_config_read_sram(void)
{
	the_case.cfg_src_format = mml_test_in_fmt;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = mml_test_out_fmt;
	the_case.cfg_dest_w = mml_test_w;
	the_case.cfg_dest_h = mml_test_out_h;
}

static void setup_read_sram_bufa(struct mml_submit *task, struct mml_test_case *cur)
{
	mml_log("%s read buf", __func__);

	task->info.mode = MML_MODE_SRAM_READ;
	task->buffer.src.flush = false;
	task->buffer.src.invalid = false;
	task->buffer.src.fd[0] = -1;

	task->info.src.height = SRAM_HEIGHT;
	task->info.dest[0].data.height = SRAM_HEIGHT;
	task->buffer.src.size[0] /= 2;
	task->buffer.src.size[1] /= 2;
	task->buffer.src.size[2] /= 2;
}

static void setup_read_sram_bufb(struct mml_submit *task, struct mml_test_case *cur)
{
	u32 src_offset = SRAM_SIZE, offset;

	mml_log("%s read buf", __func__);

	task->info.mode = MML_MODE_SRAM_READ;
	task->buffer.src.flush = false;
	task->buffer.src.invalid = false;
	task->buffer.src.fd[0] = -1;

	task->info.src.height = SRAM_HEIGHT;
	task->info.dest[0].data.height = SRAM_HEIGHT;
	task->buffer.src.size[0] /= 2;
	task->buffer.src.size[1] /= 2;
	task->buffer.src.size[2] /= 2;

	task->info.src.plane_offset[0] = src_offset;
	task->info.src.plane_offset[1] = src_offset;
	task->info.src.plane_offset[2] = src_offset;

	offset = mml_color_get_min_y_size(task->info.dest[0].data.format,
		task->info.dest[0].data.width, task->info.dest[0].data.height);
	mml_log("%s dest plane offset %u", __func__, offset);
	task->info.dest[0].data.plane_offset[0] = offset;
	task->info.dest[0].data.plane_offset[1] = offset;
	task->info.dest[0].data.plane_offset[2] = offset;
}

static void case_run_read_sram(struct mml_test *test, struct mml_test_case *cur)
{
	struct platform_device *mml_pdev;
	struct device *dev;
	struct mml_drm_ctx *mml_ctx;
	struct mml_drm_param disp = {.vdo_mode = true};
	void *mml;

	/* create context */
	mml_pdev = mml_get_plat_device(test->pdev);
	mml_ctx = mml_drm_get_context(mml_pdev, &disp);

	/* hold sram, for wrot out and rdma in */
	dev = &mml_pdev->dev;
	if (unlikely(!dev)) {
		mml_err("%s dev = null", __func__);
		return;
	}
	mml = dev_get_drvdata(dev);
	mml_sram_get(mml);

	msleep_interruptible(mml_test_interval);

	/* correct the format in sram */
	the_case.cfg_src_format = the_case.cfg_dest_format;
	the_case.cfg_dest_h = SRAM_HEIGHT;

	/* sram -> dram */
	cur->fd_in = -1;
	case_general_submit(test, cur, setup_read_sram_bufa);
	case_general_submit(test, cur, setup_read_sram_bufb);

	/* release */
	mml_sram_put(mml);
	mml_drm_put_context(mml_ctx);
}

/* case_config_wr_sram / case_run_wr_sram
 *
 * format in: MML_FMT_RGB888
 * format out: MML_FMT_RGB888
 */
static void case_config_wr_sram(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_YUYV;
	the_case.cfg_dest_w = mml_test_out_w;
	the_case.cfg_dest_h = SRAM_HEIGHT * 2;
}

static void setup_write_sram_crop(struct mml_submit *task, struct mml_test_case *cur)
{
	task->info.mode = MML_MODE_RACING;
	task->info.dest[0].crop.r.left = 0;
	task->info.dest[0].crop.r.top = 0;
	task->info.dest[0].crop.r.width = mml_test_out_w;
	task->info.dest[0].crop.r.height = mml_test_out_h;
	if (mml_test_rot == 1 || mml_test_rot == 3)
		swap(task->info.dest[0].crop.r.width, task->info.dest[0].crop.r.height);
	task->buffer.dest[0].flush = false;
	task->buffer.dest[0].invalid = false;
	task->buffer.dest[0].fd[0] = -1;
	task->info.dest[0].rotate = mml_test_rot;
	task->info.dest[0].flip = mml_test_flip;
}

static void case_run_wr_sram(struct mml_test *test, struct mml_test_case *cur)
{
	struct platform_device *mml_pdev;
	struct device *dev;
	struct mml_drm_ctx *mml_ctx;
	struct mml_drm_param disp = {.vdo_mode = true};
	void *mml;
	int32_t fd = -1;

	/* create context */
	if (unlikely(!test)) {
		mml_err("%s test = null", __func__);
		return;
	}
	mml_pdev = mml_get_plat_device(test->pdev);
	if (unlikely(!mml_pdev)) {
		mml_err("%s mml_pdev = null", __func__);
		return;
	}
	mml_ctx = mml_drm_get_context(mml_pdev, &disp);
	if (unlikely(!mml_ctx)) {
		mml_err("%s mml_ctx = null", __func__);
		return;
	}
	/* hold sram, for wrot out and rdma in */
	dev = &mml_pdev->dev;
	if (unlikely(!dev)) {
		mml_drm_put_context(mml_ctx);
		mml_err("%s dev = null", __func__);
		return;
	}
	mml = dev_get_drvdata(dev);
	mml_sram_get(mml);

	/* dram -> sram */
	swap(fd, cur->fd_out);
	case_general_submit(test, cur, setup_write_sram_crop);

	/* correct the format in sram */
	the_case.cfg_src_format = the_case.cfg_dest_format;
	the_case.cfg_src_w = mml_test_out_w;
	the_case.cfg_dest_h = SRAM_HEIGHT;

	/* sram -> dram */
	cur->fd_out = fd;
	cur->fd_in = -1;
	case_general_submit(test, cur, setup_read_sram_bufa);
	case_general_submit(test, cur, setup_read_sram_bufb);

	/* release */
	mml_sram_put(mml);
	mml_drm_put_context(mml_ctx);
}

/* case_config_rgb_up1_5
 * test format rgb888 scale up 1.5
 *
 * format in: RGB888
 * format out: RGB888
 */
static void case_config_rgb_up1_5(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_RGB888;
	the_case.cfg_dest_w = mml_test_w * 3 / 2;
	the_case.cfg_dest_h = mml_test_h * 3 / 2;
}

/* case_config_rgb_down2
 * test format rgb888 scale down 2
 *
 * format in: RGB888
 * format out: RGB888
 */
static void case_config_rgb_down2(void)
{
	the_case.cfg_src_format = MML_FMT_RGB888;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = MML_FMT_RGB888;
	the_case.cfg_dest_w = mml_test_w / 2;
	the_case.cfg_dest_h = mml_test_h / 2;
}

/* case_config_blk_manual
 * test format blk scale manual
 *
 * format in: blk
 * format out: YUYV crop
 */
static void case_config_crop_manual(void)
{
	the_case.cfg_src_format = mml_test_in_fmt;
	the_case.cfg_src_w = mml_test_w;
	the_case.cfg_src_h = mml_test_h;
	the_case.cfg_dest_format = mml_test_out_fmt;
	the_case.cfg_dest_w = mml_test_out_w;
	the_case.cfg_dest_h = mml_test_out_h;
}

static void setup_crop_manual(struct mml_submit *task, struct mml_test_case *cur)
{
	task->info.dest[0].crop.r.left = mml_test_crop_left;
	task->info.dest[0].crop.r.top = mml_test_crop_top;
	task->info.dest[0].crop.r.width = mml_test_crop_width;
	task->info.dest[0].crop.r.height = mml_test_crop_height;
	task->info.dest[0].rotate = mml_test_rot;
	task->info.dest[0].flip = mml_test_flip;

	/* check dest 0 with 2 plane size */
	if (task->buffer.dest[0].size[0] + task->buffer.dest[0].size[1] !=
		cur->size_out)
		mml_err("%s case %d dest size total %u plane %u %u",
			__func__, mml_case, cur->size_out,
			task->buffer.dest[0].size[0], task->buffer.dest[0].size[1]);
}

static void case_run_crop_manual(struct mml_test *test, struct mml_test_case *cur)
{
	case_general_submit(test, cur, setup_crop_manual);
}

enum mml_ut_case {
	MML_UT_RGB,			/* 0 */
	MML_UT_RGB_ROTATE,		/* 1 */
	MML_UT_COMPOSE_FLIP,		/* 2 */
	MML_UT_RESIZE_RELAY,		/* 3 */
	MML_UT_RESIZE_UP2,		/* 4 */
	MML_UT_NV12,			/* 5 */
	MML_UT_YUYV_DOWN2,		/* 6 */
	MML_UT_BLOCK_TO_NV12,		/* 7 */
	MML_UT_AFBC,			/* 8 */
	MML_UT_AFBC_TO_RGB,		/* 9 */
	MML_UT_2OUT,			/* 10 */
	MML_UT_2OUT_CROP,		/* 11 */
	MML_UT_2OUT_RCC,		/* 12 */
	MML_UT_CROP,			/* 13 */
	MML_UT_YV12_YUYV,		/* 14 */
	MML_UT_WRITE_SRAM,		/* 15 */
	MML_UT_READ_SRAM,		/* 16 */
	MML_UT_WR_SRAM,			/* 17 */
	MML_UT_RESIZE_UP1_5,		/* 18 */
	MML_UT_RGB_DOWN2,		/* 19 */
	MML_UT_BLK_MANUAL,		/* 20 */
	MML_UT_YUV_AFBC_TO_RGB,		/* 21 */
	MML_UT_YUV_AFBC_10_TO_RGB,	/* 22 */
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
	[MML_UT_YV12_YUYV] = {
		.config = case_config_yv12_yuyv,
		.run = case_run_yv12_yuyv,
	},
	[MML_UT_WRITE_SRAM] = {
		.config = case_config_rgb,
		.run = case_run_write_sram,
	},
	[MML_UT_READ_SRAM] = {
		.config = case_config_read_sram,
		.run = case_run_read_sram,
	},
	[MML_UT_WR_SRAM] = {
		.config = case_config_wr_sram,
		.run = case_run_wr_sram,
	},
	[MML_UT_RESIZE_UP1_5] = {
		.config = case_config_rgb_up1_5,
		.run = case_run_general,
	},
	[MML_UT_RGB_DOWN2] = {
		.config = case_config_rgb_down2,
		.run = case_run_general,
	},
	[MML_UT_BLK_MANUAL] = {
		.config = case_config_crop_manual,
		.run = case_run_crop_manual,
	},
	[MML_UT_YUV_AFBC_TO_RGB] = {
		.config = case_config_yuv_afbc_to_rgb,
		.run = case_run_yuv_afbc_to_rgb,
	},
	[MML_UT_YUV_AFBC_10_TO_RGB] = {
		.config = case_config_yuv_afbc_10_to_rgb,
		.run = case_run_yuv_afbc_to_rgb,
	},
};

static ssize_t test_read(struct file *filep, char __user *buf, size_t size,
	loff_t *offset)
{
	u32 len = sizeof(the_case);
	int ret;

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

	ret = copy_to_user(buf, &the_case, len);
	if (ret) {
		mml_err("[test]%s copy case fail %d", __func__, ret);
		return -EFAULT;
	}
	*offset += len;

	mml_log("[test]%s format src %#010x dest %#010x",
		__func__, the_case.cfg_src_format, the_case.cfg_dest_format);

	return 0;
}

static ssize_t test_write(struct file *filp, const char *buf, size_t count,
	loff_t *offp)
{
	struct mml_test *test = (struct mml_test *)filp->f_inode->i_private;
	struct mml_test_case cur = {0};

	if (count > sizeof(cur)) {
		mml_err("buf count not match %zu %zu", count, sizeof(cur));
		return -EFAULT;
	}

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

static int mml_test_inst_print(struct seq_file *seq, void *data)
{
	u32 size;
	char *insts = mml_core_get_dump_inst(&size);

	mml_log("%s dump inst buf size %u", __func__, size);

	seq_printf(seq, "%s\n", insts);

	return 0;
}

static int mml_test_inst_open(struct inode *inode, struct file *file)
{
	return single_open(file, mml_test_inst_print, inode->i_private);
}

static const struct file_operations mml_inst_dump_fops = {
	.owner = THIS_MODULE,
	.open = mml_test_inst_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int mml_test_frame_in(struct seq_file *seq, void *data)
{
	struct mml_frm_dump_data *frm = mml_core_get_frame_in();

	if (!frm->size) {
		mml_log("%s no data to dump", __func__);
		return 0;
	}

	mml_log("%s dump frame %s size %u", __func__, frm->name, frm->size);
	seq_write(seq, frm->frame, frm->size);

	return 0;
}

static int mml_test_frame_in_open(struct inode *inode, struct file *file)
{
	return single_open(file, mml_test_frame_in, inode->i_private);
}

static const struct file_operations mml_frame_in_fops = {
	.owner = THIS_MODULE,
	.open = mml_test_frame_in_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int mml_test_frame_out(struct seq_file *seq, void *data)
{
	struct mml_frm_dump_data *frm = mml_core_get_frame_out();

	if (!frm->size) {
		mml_log("%s no data to dump", __func__);
		return 0;
	}

	mml_log("%s dump frame %s size %u", __func__, frm->name, frm->size);
	seq_write(seq, frm->frame, frm->size);

	return 0;
}

static int mml_test_frame_out_open(struct inode *inode, struct file *file)
{
	return single_open(file, mml_test_frame_out, inode->i_private);
}

static const struct file_operations mml_frame_out_fops = {
	.owner = THIS_MODULE,
	.open = mml_test_frame_out_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int probe(struct platform_device *pdev)
{
	struct mml_test *test;
	struct dentry *dir;
	bool exists = false;

	mml_log("mml-test %s begin", __func__);
	test = devm_kzalloc(&pdev->dev, sizeof(*test), GFP_KERNEL);
	if (!test)
		return -ENOMEM;
	test->pdev = pdev;
	test->dev = &pdev->dev;

	dir = debugfs_lookup("mml", NULL);
	if (!dir) {
		dir = debugfs_create_dir("mml", NULL);
		if (IS_ERR(dir) && PTR_ERR(dir) != -EEXIST) {
			mml_err("debugfs_create_dir mml failed:%ld", PTR_ERR(dir));
			return PTR_ERR(dir);
		}
	} else
		exists = true;

	test->fs = debugfs_create_file(
		"mml-test", 0444, dir, test, &test_fops);
	if (IS_ERR(test->fs)) {
		mml_err("debugfs_create_file mml-test failed:%ld",
			PTR_ERR(test->fs));
		return PTR_ERR(test->fs);
	}

	test->fs_inst = debugfs_create_file(
		"mml-inst-dump", 0444, dir, test, &mml_inst_dump_fops);
	if (IS_ERR(test->fs_inst))
		mml_err("debugfs_create_file mml-inst-dump failed:%ld",
			PTR_ERR(test->fs_inst));

	test->fs_frame_in = debugfs_create_file(
		"mml-frame-dump-in", 0444, dir, test, &mml_frame_in_fops);
	if (IS_ERR(test->fs_frame_in))
		mml_err("debugfs_create_file mml-frame-dump-in failed:%ld",
			PTR_ERR(test->fs_frame_in));

	test->fs_frame_out = debugfs_create_file(
		"mml-frame-dump-out", 0444, dir, test, &mml_frame_out_fops);
	if (IS_ERR(test->fs_frame_out))
		mml_err("debugfs_create_file mml-frame-dump-out failed:%ld",
			PTR_ERR(test->fs_frame_out));

	if (exists)
		dput(dir);

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
