// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/compat.h>
#include <linux/component.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/pm_runtime.h>
#include <drm/gsp_cfg.h>
#include <uapi/drm/sprd_drm_gsp.h>
#include "gsp_core.h"
#include "gsp_dev.h"
#include "gsp_debug.h"
#include "gsp_interface.h"
#include "gsp_kcfg.h"
#include "gsp_sync.h"
#include "gsp_sysfs.h"
#include "gsp_workqueue.h"
#include "gsp_r8p0/gsp_r8p0_core.h"
#include "gsp_r9p0/gsp_r9p0_core.h"

#include "../sprd_drm.h"
#include "../sprd_drm_gsp.h"

static struct gsp_core_ops gsp_r8p0_core_ops = {
	.parse_dt = gsp_r8p0_core_parse_dt,
	.alloc = gsp_r8p0_core_alloc,
	.init = gsp_r8p0_core_init,
	.copy = gsp_r8p0_core_copy_cfg,
	.trigger = gsp_r8p0_core_trigger,
	.release = gsp_r8p0_core_release,
	.enable = gsp_r8p0_core_enable,
	.disable = gsp_r8p0_core_disable,
	.intercept = gsp_r8p0_core_intercept,
	.reset = gsp_r8p0_core_reset,
	.dump = gsp_r8p0_core_dump,
};

static struct gsp_core_ops gsp_r9p0_core_ops = {
	.parse_dt = gsp_r9p0_core_parse_dt,
	.alloc = gsp_r9p0_core_alloc,
	.init = gsp_r9p0_core_init,
	.copy = gsp_r9p0_core_copy_cfg,
	.trigger = gsp_r9p0_core_trigger,
	.release = gsp_r9p0_core_release,
	.enable = gsp_r9p0_core_enable,
	.disable = gsp_r9p0_core_disable,
	.intercept = gsp_r9p0_core_intercept,
	.reset = gsp_r9p0_core_reset,
	.dump = gsp_r9p0_core_dump,
};

static struct of_device_id gsp_dt_ids[] = {
	{.compatible = "sprd,gsp-r8p0-sharkl5pro",
	 .data = (void *)&gsp_r8p0_core_ops},
	{.compatible = "sprd,gsp-r9p0-qogirn6pro",
	.data = (void *)&gsp_r9p0_core_ops},
	{},
};
MODULE_DEVICE_TABLE(of, gsp_dt_ids);

static int boot_mode_check(void)
{
	struct device_node *np;
	const char *cmd_line;
	int ret = 0;

	np = of_find_node_by_path("/chosen");
	if (!np)
		return 0;

	ret = of_property_read_string(np, "bootargs", &cmd_line);
	if (ret < 0)
		return 0;

	if (strstr(cmd_line, "androidboot.mode=cali"))
		ret = 1;

	return ret;
}

int gsp_dev_name_cmp(struct gsp_dev *gsp)
{
	return strncmp(gsp->name, GSP_DEVICE_NAME, sizeof(gsp->name));
}

int gsp_dev_verify(struct gsp_dev *gsp)
{
	if (IS_ERR_OR_NULL(gsp)) {
		GSP_ERR("can't verify with null dev\n");
		return -1;
	}

	return gsp_dev_name_cmp(gsp);
}

void gsp_dev_set(struct gsp_dev *gsp, struct platform_device *pdev)
{
	if (gsp_dev_verify(gsp) || IS_ERR_OR_NULL(pdev)) {
		GSP_ERR("dev set params error\n");
		return;
	}

	gsp->dev = &pdev->dev;
	platform_set_drvdata(pdev, gsp);
}

void gsp_drm_dev_set(struct drm_device *drm_dev, struct device *dev)
{
	struct sprd_drm *priv = drm_dev->dev_private;

	priv->gsp_dev = dev;
}

struct gsp_core *gsp_dev_to_core(struct gsp_dev *gsp, int index)
{
	struct gsp_core *core = NULL;

	if (index < 0 || index > gsp->core_cnt) {
		GSP_ERR("index value error\n");
		return core;
	}

	for_each_gsp_core(core, gsp) {
		if (index == 0)
			break;
		index--;
	}

	return core;
}

struct device *gsp_dev_to_device(struct gsp_dev *gsp)
{
	return gsp->dev;
}

int gsp_dev_to_core_cnt(struct gsp_dev *gsp)
{
	return gsp->core_cnt;
}

int gsp_dev_to_io_cnt(struct gsp_dev *gsp)
{
	return gsp->io_cnt;
}

static void gsp_dev_add_core(struct gsp_dev *gsp, struct gsp_core *core)
{
	if (IS_ERR_OR_NULL(gsp) || IS_ERR_OR_NULL(core)) {
		GSP_ERR("add core params error\n");
		return;
	}

	list_add_tail(&core->list, &gsp->cores);
	core->parent = gsp;
}


int gsp_dev_is_idle(struct gsp_dev *gsp)
{
	int ret = 0;
	struct gsp_core *core = NULL;

	for_each_gsp_core(core, gsp) {
		ret = gsp_core_is_idle(core);
		if (!ret)
			break;
	}

	return ret;
}

int gsp_dev_is_suspend(struct gsp_dev *gsp)
{
	int ret = 0;
	struct gsp_core *core = NULL;

	for_each_gsp_core(core, gsp) {
		ret = gsp_core_is_suspend(core);
		if (ret)
			break;
	}

	return ret;
}

int gsp_dev_is_suspending(struct gsp_dev *gsp)
{
	int ret = 0;
	struct gsp_core *core = NULL;

	for_each_gsp_core(core, gsp) {
		ret = gsp_core_suspend_state_get(core);
		if (ret)
			break;
	}

	return ret;

}

struct gsp_core_ops *gsp_dev_to_core_ops(struct gsp_dev *gsp)
{
	const struct of_device_id *id = NULL;
	struct gsp_core_ops *ops = NULL;

	if (IS_ERR_OR_NULL(gsp)) {
		GSP_ERR("dev to core ops params error\n");
		return ops;
	}

	id = of_match_node(gsp_dt_ids, gsp->dev->of_node);
	if (IS_ERR_OR_NULL(id)) {
		GSP_DEV_ERR(gsp->dev, "find core ops failed\n");
		return ops;
	}

	ops = (struct gsp_core_ops *)id->data;

	return ops;
}

struct gsp_interface *gsp_dev_to_interface(struct gsp_dev *gsp)
{
	return gsp->interface;
}

int gsp_dev_prepare(struct gsp_dev *gsp)
{
	int ret = -1;

	if (gsp_dev_verify(gsp)) {
		GSP_ERR("auto gate enable invalidate core\n");
		return ret;
	}

	if (!gsp_interface_is_attached(gsp->interface)) {
		GSP_DEV_ERR(gsp->dev, "gsp has not attached interface\n");
		return ret;
	}

	ret = gsp_interface_init(gsp->interface);
	if (ret)
		GSP_DEV_ERR(gsp->dev, "gsp interface prepare failed\n");

	return ret;
}

void gsp_dev_unprepare(struct gsp_dev *gsp)
{
	int ret = -1;

	if (gsp_dev_verify(gsp)) {
		GSP_ERR("auto gate disable invalidate core\n");
		return;
	}

	if (!gsp_interface_is_attached(gsp->interface)) {
		GSP_DEV_ERR(gsp->dev, "gsp has not attached interface\n");
		return;
	}

	ret = gsp_interface_deinit(gsp->interface);
	if (ret)
		GSP_DEV_ERR(gsp->dev, "gsp interface unprepare failed\n");
}

int gsp_dev_init(struct gsp_dev *gsp)
{
	int ret = -1;
	struct gsp_core *core = NULL;

	if (gsp_dev_verify(gsp))
		return ret;

	ret = gsp_dev_sysfs_init(gsp);
	if (ret) {
		GSP_DEV_ERR(gsp->dev, "gsp sysfs initialize failed\n");
		return ret;
	}

	ret = gsp_interface_attach(&gsp->interface, gsp);
	if (ret) {
		GSP_DEV_ERR(gsp->dev, "gsp interface attach failed\n");
		return ret;
	}

	for_each_gsp_core(core, gsp) {
		if (IS_ERR_OR_NULL(core))
			return ret;

		if (gsp_core_verify(core)) {
			GSP_DEV_ERR(gsp->dev, "dev init core params error\n");
			return ret;
		}

		ret = gsp_core_init(core);
		if (ret) {
			GSP_DEV_ERR(gsp->dev, "init core[%d] failed\n",
				gsp_core_to_id(core));
			return ret;
		}
		GSP_DEV_INFO(gsp->dev, "initialize core[%d] success\n",
				gsp_core_to_id(core));
	}

	ret = gsp_dev_prepare(gsp);
	if (ret) {
		GSP_DEV_ERR(gsp->dev, "gsp interface prepare failed\n");
		goto exit;
	}

exit:
	return ret;
}

static int gsp_dev_alloc(struct device *dev, struct gsp_dev **gsp)
{
	int ret = -1;
	int i;
	u32 cnt = 0;
	const char *name = NULL;
	struct device_node *np = NULL;
	struct device_node *child = NULL;
	struct gsp_core *core = NULL;
	struct gsp_core_ops *ops = NULL;

	*gsp = devm_kzalloc(dev, sizeof(struct gsp_dev), GFP_KERNEL);
	if (IS_ERR_OR_NULL(gsp)) {
		GSP_ERR("no enough memory for dev\n");
		return ret;
	}
	memset(*gsp, 0, sizeof(struct gsp_dev));
	(*gsp)->dev = dev;
	INIT_LIST_HEAD(&(*gsp)->cores);

	np = dev->of_node;
	ret = of_property_read_u32(np, "core-cnt", &cnt);
	if (ret) {
		GSP_DEV_ERR(dev, "read core count failed\n");
		return ret;
	}
	GSP_DEV_INFO(dev, "node count: %d\n", cnt);
	(*gsp)->core_cnt = cnt;

	ret = of_property_read_string(np, "name", &name);
	if (ret) {
		GSP_DEV_ERR(dev, "read name failed\n");
		return ret;
	}
	strlcpy((*gsp)->name, name, sizeof((*gsp)->name));
	GSP_DEV_INFO(dev, "gsp device name: %s\n", (*gsp)->name);

	ret = of_property_read_u32(np, "io-cnt", &cnt);
	if (ret) {
		GSP_DEV_ERR(dev, "read io count failed\n");
		return ret;
	}
	GSP_DEV_INFO(dev, "io count: %d\n", cnt);
	(*gsp)->io_cnt = cnt;

	ops = gsp_dev_to_core_ops(*gsp);
	if (IS_ERR_OR_NULL(ops)) {
		GSP_DEV_ERR(dev, "dev to core ops failed\n");
		return ret;
	}

	for (i = 0; i < (*gsp)->core_cnt; i++) {
		child = of_parse_phandle(np, "cores", i);
		if (IS_ERR_OR_NULL(child)) {
			GSP_DEV_ERR(dev, "parse core[%d] phandle failed\n", i);
			ret = -1;
			break;
		}

		ret = gsp_core_alloc(&core, ops, child);
		if (ret)
			break;
		GSP_DEV_INFO(dev, "core[%d] allocate success\n",
				gsp_core_to_id(core));
		gsp_dev_add_core(*gsp, core);
	}

	return ret;
}

int gsp_dev_get_capability(struct gsp_dev *gsp, struct gsp_capability **capa)
{
	struct gsp_core *core = NULL;
	struct gsp_capability *capability = NULL;

	if (gsp_dev_verify(gsp)) {
		GSP_ERR("get capability with null dev or ops\n");
		return -1;
	}

	/* here set default value to zero */
	core = gsp_dev_to_core(gsp, 0);
	if (IS_ERR_OR_NULL(core) || IS_ERR_OR_NULL(core->ops)) {
		GSP_DEV_ERR(gsp->dev, "core ops has not been initialized\n");
		return -1;
	}

	capability = gsp_core_get_capability(core);
	if (IS_ERR_OR_NULL(capa) || IS_ERR_OR_NULL(capability)) {
		GSP_DEV_ERR(gsp->dev, "get core[0] capability failed\n");
		return -1;
	}

	(*capa) = capability;
	/*
	 * io_cnt attribute couples with gsp_dev
	 * so we assign thie attribute here
	 */
	(*capa)->io_cnt = gsp_dev_to_io_cnt(gsp);
	(*capa)->core_cnt = gsp_dev_to_core_cnt(gsp);
	return 0;
}

void gsp_dev_start_work(struct gsp_dev *gsp)
{
	struct gsp_core *core = NULL;

	for_each_gsp_core(core, gsp) {
		if (!gsp_core_is_idle(core))
			continue;
		GSP_DEV_DEBUG(gsp->dev, "gsp core[%d] is idle to start work\n",
			  gsp_core_to_id(core));
		gsp_core_work(core);
	}
}

int gsp_dev_resume_wait(struct gsp_dev *gsp)
{
	struct gsp_core *core = NULL;
	long time = 0;
	int resume_status = 0;

	for_each_gsp_core(core, gsp) {
		if (!gsp_core_suspend_state_get(core))
			continue;

		time = wait_for_completion_interruptible_timeout
				(&core->resume_done, GSP_CORE_RESUME_WAIT);
		if (time == -ERESTARTSYS) {
			GSP_DEV_ERR(gsp->dev,
				"core[%d] resume interrupt when resume wait\n",
				gsp_core_to_id(core));
			resume_status = -1;
		} else if (time == 0) {
			GSP_DEV_ERR(gsp->dev, "core[%d] resume wait timeout\n",
				gsp_core_to_id(core));
			resume_status = -1;
		} else if (time > 0) {
			GSP_DEV_DEBUG(gsp->dev,
				"core[%d] resume wait success\n",
				gsp_core_to_id(core));
		}

		if (resume_status == 0)
			reinit_completion(&core->resume_done);
		else {
			GSP_DEV_ERR(gsp->dev, "resume wait done fail!\n");
			break;
		}
	}

	return resume_status;
}

int sprd_gsp_get_capability_ioctl(struct drm_device *drm_dev, void *data,
			 struct drm_file *file)
{
	int ret = -1;
	struct platform_device *pdev = NULL;
	struct gsp_dev *gsp = NULL;
	struct drm_gsp_capability *drm_capa = data;
	struct sprd_drm *priv = NULL;
	struct device *dev = NULL;
	struct gsp_capability *capa = NULL;
	size_t size;

	priv = drm_dev->dev_private;

	if (IS_ERR_OR_NULL(priv)) {
		GSP_ERR("null priv\n");
		return -1;
	}

	dev = priv->gsp_dev;
	if (IS_ERR_OR_NULL(dev)) {
		GSP_ERR("null dev\n");
		return -1;
	}

	pdev = to_platform_device(dev);
	if (IS_ERR_OR_NULL(pdev)) {
		GSP_DEV_ERR(dev, "null pdev\n");
		return -1;
	}
	gsp = platform_get_drvdata(pdev);

	if (gsp_dev_verify(gsp)) {
		GSP_DEV_ERR(dev, "ioctl with err dev\n");
		ret = -EBADF;
		return ret;
	}

	size = drm_capa->size;

	if (size < sizeof(*capa)) {
		GSP_DEV_ERR(dev, "size: %zu less than request: %zu.\n",
				size, sizeof(struct gsp_capability));
		return -1;
	}

	ret = gsp_dev_get_capability(gsp, &capa);

	if (ret) {
		GSP_DEV_ERR(dev, "get capability error\n");
		return -1;
	}

	if (size > capa->capa_size) {
		GSP_DEV_ERR(dev, "get capability size exceed error\n");
		return -1;
	}

	ret = copy_to_user((void __user *)drm_capa->cap,
				(const void *)capa, size);

	if (ret)
		GSP_DEV_ERR(dev, "get capability copy error\n");

	GSP_DEV_INFO(dev, "io_cnt:%d, core_cnt:%d ,size:%zu, cap->size:%d",
		capa->io_cnt, capa->core_cnt, size, capa->capa_size);

	return ret;
}
EXPORT_SYMBOL(sprd_gsp_get_capability_ioctl);

int sprd_gsp_trigger_ioctl(struct drm_device *drm_dev, void *data,
			 struct drm_file *file)
{
	int ret = -1;
	int cnt = 0;
	bool async = false;
	bool split = false;
	struct gsp_kcfg_list kcfg_list;
	size_t size = 1;

	struct platform_device *pdev = NULL;
	struct gsp_dev *gsp = NULL;
	struct drm_gsp_cfg_user *cmd = data;
	struct sprd_drm *priv = NULL;
	struct device *dev = NULL;

	priv = drm_dev->dev_private;

	if (IS_ERR_OR_NULL(priv)) {
		GSP_ERR("null priv\n");
		return -1;
	}


	dev = priv->gsp_dev;
	if (IS_ERR_OR_NULL(dev)) {
		GSP_ERR("null dev\n");
		return -1;
	}

	pdev = to_platform_device(dev);
	if (IS_ERR_OR_NULL(pdev)) {
		GSP_DEV_ERR(dev, "null pdev\n");
		return -1;
	}
	gsp = platform_get_drvdata(pdev);

	if (gsp_dev_verify(gsp)) {
		GSP_DEV_ERR(dev, "ioctl with err dev\n");
		ret = -EBADF;
		return ret;
	}

	async = cmd->async;
	cnt = cmd->num;
	split = cmd->split;
	size = cmd->size;

	GSP_DEV_DEBUG(dev, "async: %d, split: %d, cnt: %d\n",
			async, split, cnt);
	if (cnt > GSP_MAX_IO_CNT(gsp) || cnt < 1) {
		GSP_DEV_ERR(dev, "request error number kcfgs\n");
		ret = -EINVAL;
		goto exit;
	}

	if (size < sizeof(struct gsp_cfg)) {
		GSP_DEV_ERR(dev, "error base cfg size: %zu\n", size);
		ret = -EINVAL;
		goto exit;
	}

	gsp_kcfg_list_init(&kcfg_list, async, split, size, cnt);

	ret = gsp_kcfg_list_acquire(gsp, &kcfg_list, cnt);
	if (ret) {
		GSP_DEV_ERR(dev, "kcfg list acuqire failed\n");
		if (gsp_kcfg_list_is_empty(&kcfg_list))
			goto exit;
		else
			goto kcfg_list_put;
	}

	ret = gsp_kcfg_list_fill(&kcfg_list, (void __user *)cmd->config);
	if (ret) {
		GSP_DEV_ERR(dev, "kcfg list fill failed\n");
		goto kcfg_list_put;
	}

	ret = gsp_kcfg_list_push(&kcfg_list);
	if (ret) {
		GSP_DEV_ERR(dev, "kcfg list push failed\n");
		goto kcfg_list_release;
	}

	pm_runtime_mark_last_busy(gsp->dev);
	pm_runtime_get_sync(gsp->dev);

	if (gsp_dev_resume_wait(gsp))
		goto kcfg_list_release;

	if (gsp_dev_is_suspend(gsp))
		GSP_DEV_INFO(dev, "no need to process kcfg at suspend state\n");

	gsp_dev_start_work(gsp);

	if (!async) {
		ret = gsp_kcfg_list_wait(&kcfg_list);
		if (ret)
			goto kcfg_list_release;
	}
	goto exit;

kcfg_list_release:
	gsp_kcfg_list_release(&kcfg_list);
kcfg_list_put:
	gsp_kcfg_list_put(&kcfg_list);
exit:
	return ret;
}
EXPORT_SYMBOL(sprd_gsp_trigger_ioctl);

static void gsp_miscdev_deregister(struct gsp_dev *gsp)
{
	if (gsp_dev_verify(gsp)) {
		GSP_ERR("misc dev deregister params error\n");
		return;
	}

	misc_deregister(&gsp->mdev);
}

static int gsp_miscdev_register(struct gsp_dev *gsp)
{
	if (gsp_dev_verify(gsp)) {
		GSP_ERR("register null dev\n");
		return -1;
	}

	gsp->mdev.minor = MISC_DYNAMIC_MINOR;
	gsp->mdev.name = "gsp";

	return misc_register(&gsp->mdev);
}

static void gsp_dev_free(struct gsp_dev *gsp)
{
	struct gsp_core *core = NULL;
	struct gsp_core *tmp = NULL;

	if (IS_ERR_OR_NULL(gsp)) {
		GSP_DEBUG("nothing to do for unalloc gsp\n");
		return;
	}

	if (list_empty(&gsp->cores))
		return;

	for_each_gsp_core_safe(core, tmp, gsp) {
		if (!IS_ERR_OR_NULL(core))
			gsp_core_free(core);
	}
}

static void gsp_dev_deinit(struct gsp_dev *gsp)
{
	struct gsp_core *core;
	struct gsp_core *tmp = NULL;

	if (IS_ERR_OR_NULL(gsp)) {
		GSP_DEBUG("nothing to do for uninit gsp\n");
		return;
	}

	if (list_empty(&gsp->cores))
		return;

	for_each_gsp_core_safe(core, tmp, gsp) {
		if (!IS_ERR_OR_NULL(core))
			gsp_core_deinit(core);
	}

	gsp_interface_detach(gsp->interface);

	gsp_dev_sysfs_destroy(gsp);
}

void gsp_dev_free_and_deinit(struct gsp_dev *gsp)
{
	if (IS_ERR_OR_NULL(gsp)) {
		GSP_ERR("dev free and deinit params error\n");
		return;
	}

	gsp_dev_deinit(gsp);

	gsp_dev_free(gsp);
}

static int sprd_gsp_bind(struct device *dev, struct device *master, void *data)
{
	struct platform_device *pdev = NULL;
	struct gsp_dev *gsp = NULL;
	struct drm_device *drm_dev = data;
	int ret = -1;

	if (IS_ERR_OR_NULL(dev)) {
		GSP_ERR("dev initialize params error\n");
		return ret;
	}

	pdev = to_platform_device(dev);
	gsp = platform_get_drvdata(pdev);

	ret = gsp_dev_init(gsp);
	if (ret) {
		GSP_DEV_ERR(dev, "dev init failed\n");
		goto dev_deinit;
	}

	gsp_drm_dev_set(drm_dev, dev);

	pm_runtime_set_active(&pdev->dev);
	pm_runtime_set_autosuspend_delay(&pdev->dev, PM_RUNTIME_DELAY_MS);
	pm_runtime_use_autosuspend(&pdev->dev);

	pm_runtime_enable(&pdev->dev);

	GSP_DEV_INFO(dev, "dev bind success\n");

	return ret;

dev_deinit:
	gsp_dev_deinit(gsp);
	return ret;
}

static void sprd_gsp_unbind(struct device *dev, struct device *master,
	void *data)
{
	struct drm_device *drm_dev = data;

	gsp_drm_dev_set(drm_dev, NULL);
}

static const struct component_ops gsp_component_ops = {
	.bind = sprd_gsp_bind,
	.unbind = sprd_gsp_unbind,
};

static int gsp_dev_probe(struct platform_device *pdev)
{
	struct gsp_dev *gsp = NULL;
	int ret = -1;

	if (IS_ERR_OR_NULL(pdev)) {
		GSP_ERR("probe params error\n");
		goto exit;
	}

	ret = gsp_dev_alloc(&pdev->dev, &gsp);
	if (ret) {
		GSP_ERR("dev alloc failed\n");
		goto dev_free;
	}

	ret = gsp_miscdev_register(gsp);
	if (ret) {
		GSP_DEV_ERR(gsp->dev, "mdev register failed\n");
		goto dev_free;
	}

	gsp_dev_set(gsp, pdev);

	GSP_DEV_INFO(gsp->dev, "probe success\n");

	return component_add(&pdev->dev, &gsp_component_ops);

dev_free:
	gsp_dev_free(gsp);
exit:
	return ret;
}

#if defined(CONFIG_PM_SLEEP) || defined(CONFIG_PM)
static void gsp_dev_wait_suspend(struct gsp_dev *gsp)
{
	struct gsp_core *core = NULL;
	bool need_wait = false;
	int ret = 0;

	for_each_gsp_core(core, gsp) {
		if (gsp_core_verify(core)) {
			GSP_DEV_ERR(gsp->dev, "wait work done params error\n");
			break;
		}

		need_wait = gsp_core_is_idle(core) ? false : true;
		gsp_core_suspend_state_set(core, CORE_STATE_SUSPEND_WAIT);
		if (need_wait)
			ret = gsp_core_suspend_wait(core);

		if (ret == 0)
			gsp_core_suspend(core);
		else
			GSP_DEV_ERR(gsp->dev, "gsp_dev_wait suspend fail !\n");
	}
}

static void gsp_dev_enter_suspending(struct gsp_dev *gsp)
{
	struct gsp_core *core = NULL;

	for_each_gsp_core(core, gsp) {
		if (gsp_core_verify(core)) {
			GSP_DEV_ERR(gsp->dev,
				"check core verify params error\n");
			break;
		}
		gsp_core_suspend_state_set(core, CORE_STATE_SUSPEND_BEGIN);
	}
}

static void gsp_dev_enter_suspend(struct gsp_dev *gsp)
{
	struct gsp_core *core = NULL;

	for_each_gsp_core(core, gsp) {
		if (gsp_core_verify(core)) {
			GSP_DEV_ERR(gsp->dev,
				"check core verify params error\n");
			break;
		}

		gsp_core_suspend(core);
	}
}

static int gsp_dev_stop(struct gsp_dev *gsp)
{
	int ret = -1;
	struct gsp_core *core = NULL;

	for_each_gsp_core(core, gsp) {
		if (gsp_core_verify(core)) {
			GSP_DEV_ERR(gsp->dev, "stop device params error\n");
			break;
		}

		ret = gsp_core_stop(core);
		if (ret) {
			GSP_DEV_ERR(gsp->dev, "gsp core stop fail!\n");
			goto exit;
		}
	}
	if (gsp_dev_is_suspend(gsp))
		gsp_dev_unprepare(gsp);

exit:
	return ret;
}
#endif

static int gsp_dev_remove(struct platform_device *pdev)
{
	struct gsp_dev *gsp = NULL;

	GSP_INFO("remove gsp device\n");
	gsp = platform_get_drvdata(pdev);
	if (gsp_dev_verify(gsp)) {
		GSP_ERR("get a error gsp device\n");
		return -EINVAL;
	}

	gsp_miscdev_deregister(gsp);
	gsp_dev_free_and_deinit(gsp);

	return 0;
}

#if defined(CONFIG_PM_SLEEP) || defined(CONFIG_PM)
static int gsp_dev_suspend(struct device *dev)
{
	int ret = -1;
	struct platform_device *pdev = NULL;
	struct gsp_dev *gsp = NULL;

	pdev = to_platform_device(dev);
	gsp = platform_get_drvdata(pdev);

	if (gsp_dev_is_suspend(gsp)) {
		GSP_DEV_ERR(dev, "gsp dev already suspend, skip !\n");
		return 0;
	}

	gsp_dev_enter_suspending(gsp);

	if (!gsp_dev_is_idle(gsp))
		gsp_dev_wait_suspend(gsp);
	else
		gsp_dev_enter_suspend(gsp);

	ret = gsp_dev_stop(gsp);
	if (ret)
		GSP_DEV_ERR(dev, "stop device failed\n");

	return 0;
}

static int gsp_dev_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = NULL;
	struct gsp_dev *gsp = NULL;
	struct gsp_core *core = NULL;

	pdev = to_platform_device(dev);
	gsp = platform_get_drvdata(pdev);

	for_each_gsp_core(core, gsp) {
		if (gsp_core_verify(core)) {
			GSP_DEV_ERR(dev, "resume error core\n");
			ret = -1;
			break;
		}
		if (!gsp_core_is_suspend(core)) {
			long time = 0;
			int err = 0;

			time = wait_for_completion_interruptible_timeout
				(&core->suspend_done, GSP_CORE_SUSPEND_WAIT);
			if (time == -ERESTARTSYS) {
				GSP_DEV_ERR(dev,
				    "core[%d] interrupt in suspend wait\n",
				    gsp_core_to_id(core));
				err++;
			} else if (time == 0) {
				GSP_DEV_ERR(dev,
				    "core[%d] suspend wait timeout\n",
				    gsp_core_to_id(core));
				err++;
			} else if (time > 0) {
				GSP_DEV_DEBUG(dev,
				    "core[%d] suspend wait success\n",
				    gsp_core_to_id(core));
			}

			if (err) {
				ret = -1;
				break;
			}
		}
	}

	if (ret)
		GSP_DEV_INFO(dev,
			"resume wait not success, force exec resume!\n");

	ret = gsp_dev_prepare(gsp);
	if (ret) {
		GSP_DEV_ERR(dev, "gsp dev resume prepare fail !\n");
		goto exit;
	}

	for_each_gsp_core(core, gsp) {
		gsp_core_resume(core);
		gsp_core_suspend_state_set(core, CORE_STATE_SUSPEND_EXIT);
		complete(&core->resume_done);
		ret = 0;
	}

exit:
	return ret;
}
#endif

#ifdef CONFIG_PM_SLEEP
static int gsp_dev_pm_suspend(struct device *dev)
{
	int ret;

	if (pm_runtime_suspended(dev))
		ret = 0;
	else
		ret = gsp_dev_suspend(dev);

	return ret;
}

static int gsp_dev_pm_resume(struct device *dev)
{
	int ret;

	if (!pm_runtime_suspended(dev)) {
		ret = gsp_dev_resume(dev);

		pm_runtime_disable(dev);
		pm_runtime_set_active(dev);
		pm_runtime_enable(dev);
	} else
		ret = 0;

	return ret;
}
#endif

#ifdef CONFIG_PM
static int gsp_dev_runtime_suspend(struct device *dev)
{
	return gsp_dev_suspend(dev);
}

static int gsp_dev_runtime_resume(struct device *dev)
{
	return gsp_dev_resume(dev);
}

static int gsp_dev_runtime_idle(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops gsp_dev_pmops = {
#ifdef CONFIG_PM_SLEEP
	SET_SYSTEM_SLEEP_PM_OPS(gsp_dev_pm_suspend, gsp_dev_pm_resume)
#endif
#ifdef CONFIG_PM
	SET_RUNTIME_PM_OPS(gsp_dev_runtime_suspend,
					gsp_dev_runtime_resume,
					gsp_dev_runtime_idle)
#endif
};

static struct platform_driver gsp_drv = {
	.probe = gsp_dev_probe,
	.remove = gsp_dev_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd-gsp",
		.of_match_table = of_match_ptr(gsp_dt_ids),
		.pm = &gsp_dev_pmops,
	},
};

static int __init gsp_drv_init(void)
{
	int ret = -1;

	if (boot_mode_check()) {
		GSP_WARN("Calibration Mode! Don't register sprd gsp driver");
		return 0;
	}

	GSP_INFO("gsp device init begin\n");

	ret = platform_driver_register(&gsp_drv);

	if (ret)
		GSP_ERR("gsp platform driver register failed\n");

	GSP_INFO("gsp device init end\n");

	return ret;
}
module_init(gsp_drv_init);

static void __exit gsp_drv_deinit(void)
{
	platform_driver_unregister(&gsp_drv);
}
module_exit(gsp_drv_deinit);

MODULE_AUTHOR("yintian.tao <yintian.tao@spreadtrum.com>");
MODULE_AUTHOR("Chen He <chen.he@unisoc.com>");
MODULE_DESCRIPTION("SPRD DRM GSP Driver");
MODULE_LICENSE("GPL v2");
