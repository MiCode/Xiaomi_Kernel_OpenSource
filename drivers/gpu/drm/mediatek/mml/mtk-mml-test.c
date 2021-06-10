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
#include "mtk-mml-m4u.h"

/* HOWTO:
 *	1. echo $id > sys/module/mtk_mml_test/patameters/mml_case
 *	2. run userspace UnitTest bin
 *
 * UnitTest flow
 *	1. open file "/sys/kernel/debug/mml/mml-test"
 *	2. read to struct mml_test_case, store cfg_ members.
 *	3. allocate fd with "cfg_dest_size" bytes
 *	4. write struct mml_test_case with [in] members
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
	uint32_t cfg_dest_size;
	uint32_t cfg_dest_y_stride;

	/* [in] */
	int32_t fd_in;
	uint32_t size_in;
	int32_t fd_out;
	uint32_t size_out;
};

static struct mml_test_case the_case;

int mml_case;
EXPORT_SYMBOL(mml_case);
module_param(mml_case, int, 0644);

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

#define CASE_SRC_RGB_W	1920
#define CASE_SRC_RGB_H	1088

#define CASE_RGB_W	1920
#define CASE_RGB_H	1088

static void case_config_rgb(void)
{
	the_case.cfg_src_format = MML_FMT_BGR888;
	the_case.cfg_src_w = CASE_SRC_RGB_W;
	the_case.cfg_src_h = CASE_SRC_RGB_H;
	the_case.cfg_dest_format = MML_FMT_BGR888;
	the_case.cfg_dest_w = CASE_RGB_W;
	the_case.cfg_dest_h = CASE_RGB_H;
	the_case.cfg_dest_y_stride = mml_color_get_min_y_stride(
		the_case.cfg_dest_format, the_case.cfg_dest_w);
	the_case.cfg_dest_size = mml_color_get_min_y_size(
		the_case.cfg_dest_format,
		the_case.cfg_dest_w, the_case.cfg_dest_h);
}

static void case_run_rgb(struct mml_test *test, struct mml_test_case *cur)
{
	struct platform_device *mml_pdev;
	struct mml_drm_ctx *mml_ctx;
	struct mml_job job = {};
	struct mml_submit task = {.job = &job};
	const u32 src_fmt = the_case.cfg_src_format;
	const u32 dest_fmt = the_case.cfg_dest_format;
	const u32 y_stride = the_case.cfg_dest_y_stride;
	const u32 src_w = the_case.cfg_src_w;
	const u32 src_h = the_case.cfg_src_h;
	s32 ret;

	mml_log("[test]%s begin", __func__);

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

	task.info.src.width = src_w;
	task.info.src.height = src_h;
	task.info.src.y_stride =
		mml_color_get_min_y_stride(src_fmt, src_w);
	task.info.src.uv_stride = 0;
	task.info.src.vert_stride = 0;
	task.info.src.format = src_fmt;
	task.info.src.profile = 0;
	task.info.src.plane_cnt = 1;
	task.info.src.secure = false;
	task.info.dest_cnt = 1;
	task.info.dest[0].data.width = the_case.cfg_dest_w;
	task.info.dest[0].data.height = the_case.cfg_dest_h;
	task.info.dest[0].data.y_stride = y_stride;
	task.info.dest[0].data.uv_stride = 0;
	task.info.dest[0].data.vert_stride = 0;
	task.info.dest[0].data.format = dest_fmt;
	task.info.dest[0].data.profile = 0;
	task.info.dest[0].data.plane_cnt = 1;
	task.info.dest[0].data.secure = false;
	task.info.mode = MML_MODE_MML_DECOUPLE;
	task.info.layer_id = 0;
	task.buffer.src.fd[0] = cur->fd_in;
	task.buffer.src.size[0] = cur->size_in;
	task.buffer.src.cnt = 1;
	task.buffer.dest[0].fd[0] = cur->fd_out;
	task.buffer.dest[0].size[0] = cur->size_out;
	task.buffer.dest[0].cnt = 1;
	task.buffer.dest_cnt = 1;

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

static void case_config_rgb_rot(void)
{
	case_config_rgb();
	swap(the_case.cfg_dest_w, the_case.cfg_dest_h);
	the_case.cfg_dest_y_stride = mml_color_get_min_y_stride(
		the_case.cfg_dest_format, the_case.cfg_dest_w);
	the_case.cfg_dest_size = mml_color_get_min_y_size(
		the_case.cfg_dest_format,
		the_case.cfg_dest_w, the_case.cfg_dest_h);
}

static void case_run_rgb_rot(struct mml_test *test, struct mml_test_case *cur)
{
	struct platform_device *mml_pdev;
	struct mml_drm_ctx *mml_ctx;
	struct mml_job job = {};
	struct mml_submit task = {.job = &job};
	const u32 src_fmt = the_case.cfg_src_format;
	const u32 dest_fmt = the_case.cfg_dest_format;
	const u32 y_stride = the_case.cfg_dest_y_stride;
	const u32 src_w = the_case.cfg_src_w;
	const u32 src_h = the_case.cfg_src_h;
	s32 ret;

	mml_log("[test]%s begin", __func__);

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

	task.info.src.width = src_w;
	task.info.src.height = src_h;
	task.info.src.y_stride =
		mml_color_get_min_y_stride(src_fmt, src_w);
	task.info.src.uv_stride = 0;
	task.info.src.vert_stride = 0;
	task.info.src.format = src_fmt;
	task.info.src.profile = 0;
	task.info.src.plane_cnt = 1;
	task.info.src.secure = false;
	task.info.dest_cnt = 1;
	task.info.dest[0].data.width = the_case.cfg_dest_w;
	task.info.dest[0].data.height = the_case.cfg_dest_h;
	task.info.dest[0].data.y_stride = y_stride;
	task.info.dest[0].data.uv_stride = 0;
	task.info.dest[0].data.vert_stride = 0;
	task.info.dest[0].data.format = dest_fmt;
	task.info.dest[0].data.profile = 0;
	task.info.dest[0].data.plane_cnt = 1;
	task.info.dest[0].data.secure = false;
	task.info.dest[0].rotate = MML_ROT_90;
	task.info.mode = MML_MODE_MML_DECOUPLE;
	task.info.layer_id = 0;
	task.buffer.src.fd[0] = cur->fd_in;
	task.buffer.src.size[0] = cur->size_in;
	task.buffer.src.cnt = 1;
	task.buffer.dest[0].fd[0] = cur->fd_out;
	task.buffer.dest[0].size[0] = cur->size_out;
	task.buffer.dest[0].cnt = 1;
	task.buffer.dest_cnt = 1;

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

#define CASE_RGB_LEFT	50
#define CASE_RGB_TOP	16

static void case_config_rgb_compose(void)
{
	the_case.cfg_src_format = MML_FMT_BGR888;
	the_case.cfg_src_w = CASE_SRC_RGB_W;
	the_case.cfg_src_h = CASE_SRC_RGB_H;
	the_case.cfg_dest_format = MML_FMT_BGR888;
	the_case.cfg_dest_w = CASE_RGB_W + CASE_RGB_LEFT;
	the_case.cfg_dest_h = CASE_RGB_H + CASE_RGB_TOP;
	the_case.cfg_dest_y_stride = mml_color_get_min_y_stride(
		the_case.cfg_dest_format, the_case.cfg_dest_w);
	the_case.cfg_dest_size = mml_color_get_min_y_size(
		the_case.cfg_dest_format,
		the_case.cfg_dest_w, the_case.cfg_dest_h);
}

static void case_run_rgb_compose(struct mml_test *test, struct mml_test_case *cur)
{
	struct platform_device *mml_pdev;
	struct mml_drm_ctx *mml_ctx;
	struct mml_job job = {};
	struct mml_submit task = {.job = &job};
	const u32 src_fmt = the_case.cfg_src_format;
	const u32 dest_fmt = the_case.cfg_dest_format;
	const u32 y_stride = the_case.cfg_dest_y_stride;
	const u32 src_w = the_case.cfg_src_w;
	const u32 src_h = the_case.cfg_src_h;
	const u32 dest_w = CASE_RGB_W;
	const u32 dest_h = CASE_RGB_H;
	s32 ret;

	const u32 dest_left = CASE_RGB_LEFT;
	const u32 dest_top = CASE_RGB_TOP;

	mml_log("[test]%s begin", __func__);

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

	task.info.src.width = src_w;
	task.info.src.height = src_h;
	task.info.src.y_stride =
		mml_color_get_min_y_stride(src_fmt, src_w);
	task.info.src.uv_stride = 0;
	task.info.src.vert_stride = 0;
	task.info.src.format = src_fmt;
	task.info.src.profile = 0;
	task.info.src.plane_cnt = 1;
	task.info.src.secure = false;
	task.info.dest_cnt = 1;
	task.info.dest[0].data.width = dest_w;
	task.info.dest[0].data.height = dest_h;
	task.info.dest[0].data.y_stride = y_stride;
	task.info.dest[0].data.uv_stride = 0;
	task.info.dest[0].data.vert_stride = 0;
	task.info.dest[0].data.format = dest_fmt;
	task.info.dest[0].data.profile = 0;
	task.info.dest[0].data.plane_cnt = 1;
	task.info.dest[0].data.secure = false;
	task.info.dest[0].crop.r.width = src_w;
	task.info.dest[0].crop.r.height = src_h;
	task.info.dest[0].compose.left = dest_left;
	task.info.dest[0].compose.top = dest_top;
	task.info.dest[0].compose.width = the_case.cfg_dest_w;
	task.info.dest[0].compose.height = the_case.cfg_dest_h;
	task.info.dest[0].flip = true;
	task.info.mode = MML_MODE_MML_DECOUPLE;
	task.info.layer_id = 0;
	task.buffer.src.fd[0] = cur->fd_in;
	task.buffer.src.size[0] = cur->size_in;
	task.buffer.src.cnt = 1;
	task.buffer.dest[0].fd[0] = cur->fd_out;
	task.buffer.dest[0].size[0] = cur->size_out;
	task.buffer.dest[0].cnt = 1;
	task.buffer.dest_cnt = 1;

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

static void case_run_rgb_pq(struct mml_test *test, struct mml_test_case *cur)
{
	struct platform_device *mml_pdev;
	struct mml_drm_ctx *mml_ctx;
	struct mml_job job = {};
	struct mml_submit task = {.job = &job};
	const u32 src_fmt = the_case.cfg_src_format;
	const u32 dest_fmt = the_case.cfg_dest_format;
	const u32 y_stride = the_case.cfg_dest_y_stride;
	const u32 src_w = the_case.cfg_src_w;
	const u32 src_h = the_case.cfg_src_h;
	s32 ret;

	mml_log("[test]%s begin", __func__);

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

	task.info.src.width = src_w;
	task.info.src.height = src_h;
	task.info.src.y_stride =
		mml_color_get_min_y_stride(src_fmt, src_w);
	task.info.src.uv_stride = 0;
	task.info.src.vert_stride = 0;
	task.info.src.format = src_fmt;
	task.info.src.profile = 0;
	task.info.src.plane_cnt = 1;
	task.info.src.secure = false;
	task.info.dest_cnt = 1;
	task.info.dest[0].data.width = the_case.cfg_dest_w;
	task.info.dest[0].data.height = the_case.cfg_dest_h;
	task.info.dest[0].data.y_stride = y_stride;
	task.info.dest[0].data.uv_stride = 0;
	task.info.dest[0].data.vert_stride = 0;
	task.info.dest[0].data.format = dest_fmt;
	task.info.dest[0].data.profile = 0;
	task.info.dest[0].data.plane_cnt = 1;
	task.info.dest[0].data.secure = false;
	task.info.mode = MML_MODE_MML_DECOUPLE;
	task.info.layer_id = 0;
	task.info.dest[0].pq_config.en = true;
	task.info.dest[0].pq_config.aal_en = true;
	task.buffer.src.fd[0] = cur->fd_in;
	task.buffer.src.size[0] = cur->size_in;
	task.buffer.src.cnt = 1;
	task.buffer.dest[0].fd[0] = cur->fd_out;
	task.buffer.dest[0].size[0] = cur->size_out;
	task.buffer.dest[0].cnt = 1;
	task.buffer.dest_cnt = 1;

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


static struct test_case_op cases[4] = {
	{
		.config = case_config_rgb,
		.run = case_run_rgb,
	},
	{
		.config = case_config_rgb_rot,
		.run = case_run_rgb_rot,
	},
	{
		.config = case_config_rgb_compose,
		.run = case_run_rgb_compose,
	},
	{
		.config = case_config_rgb,
		.run = case_run_rgb_pq,
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

	mml_log("[test]%s run read", __func__);

	if (mml_case < ARRAY_SIZE(cases))
		cases[mml_case].config();
	else
		mml_err("[test]no such case %d", mml_case);
	copy_to_user(buf, &the_case, len);
	*offset += len;

	mml_log("[test]%s format src %#010x dest %#010x size %u stride %u",
		__func__, the_case.cfg_src_format,
		the_case.cfg_dest_format, the_case.cfg_dest_size,
		the_case.cfg_dest_y_stride);

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

	if (mml_case < ARRAY_SIZE(cases))
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
	mml_ion_create("mml-test");
	mml_log("debugfs_create_file mml-test success");

	return 0;
}

static int remove(struct platform_device *pdev)
{
	mml_ion_destroy();
	return 0;
}

static const struct of_device_id test_of_ids[] = {
	{
		.compatible = "mediatek,mml-test",
	},
	{}
};
MODULE_DEVICE_TABLE(of, test_of_ids);

static struct platform_driver test_drv = {
	.probe = probe,
	.remove = remove,
	.driver = {
		.name = "mml_test",
		.of_match_table = test_of_ids,
	},
};
module_platform_driver(test_drv);
