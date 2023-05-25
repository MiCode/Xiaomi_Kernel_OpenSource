/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2019-02-22 File created.
 */

#if defined(CONFIG_FSM_FIRMWARE)
#include "fsm_public.h"
#include <linux/firmware.h>
#include <linux/slab.h>

static int g_fsm_fw_init = 0;

#ifdef FSM_UNUSED_CODE
static void *fsm_devm_kzalloc(struct device *dev, void *buf, size_t size)
{
	char *devm_buf = devm_kzalloc(dev, size, GFP_KERNEL);

	if (!devm_buf) {
		return devm_buf;
	} else {
		memcpy(devm_buf, buf, size);
	}

	return devm_buf;
}

static void fsm_devm_kfree(struct device *dev, void **buf)
{
	if (*buf) {
		devm_kfree(dev, *buf);
		*buf = NULL;
	}
}
#endif

static void fsm_firmware_inited(const struct firmware *cont, void *context)
{
	struct device *dev = (struct device *)context;
	int ret;

	if (dev == NULL || cont == NULL) {
		pr_err("bad parameter");
		return;
	}

	pr_info("size: %zu", cont->size);
	fsm_mutex_lock();
	ret = fsm_parse_preset(cont->data, (uint32_t)cont->size);
	fsm_mutex_unlock();
	release_firmware(cont);
	if (ret) {
		pr_err("parse firmware fail: %d", ret);
		g_fsm_fw_init = 0;
	}
}

int fsm_firmware_init(char *fw_name)
{
	struct device *dev;
	int ret;

	if (fw_name == NULL) {
		pr_err("invalid firmware name");
		return -EINVAL;
	}
	if (fsm_get_presets() || g_fsm_fw_init) {
		return MODULE_INITED;
	}
	dev = fsm_get_pdev();
	if (dev == NULL) {
		pr_err("invalid device");
		return -EINVAL;
	}

	pr_info("loading %s in nowait mode", fw_name);
	ret = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
			fw_name, dev, GFP_KERNEL,
			dev, fsm_firmware_inited);
	if (ret) {
		pr_err("request %s failed: %d\n", fw_name, ret);
		return ret;
	}
	g_fsm_fw_init = 1;

	return ret;
}

int fsm_firmware_init_sync(char *fw_name)
{
	fsm_config_t *cfg = fsm_get_config();
	const struct firmware *fw_cont;
	struct device *dev;
	int ret;

	if (fw_name == NULL) {
		pr_err("invalid firmware name");
		return -EINVAL;
	}
	if (cfg->force_fw) {
		pr_info("force loading");
		cfg->force_fw = false;
		fsm_set_presets(NULL);
	}
	if (fsm_get_presets()) {
		return MODULE_INITED;
	}
	dev = fsm_get_pdev();
	if (dev == NULL) {
		pr_err("invalid device");
		return -EINVAL;
	}

	g_fsm_fw_init = 0;
	pr_info("loading %s in sync mode", fw_name);
	ret = request_firmware(&fw_cont, fw_name, dev);
	if (ret) {
		pr_err("request %s failed: %d\n", fw_name, ret);
		return ret;
	}
	ret = fsm_parse_preset(fw_cont->data, (uint32_t)fw_cont->size);
	release_firmware(fw_cont);
	if (ret) {
		pr_err("parse firmware fail: %d", ret);
		return ret;
	}
	g_fsm_fw_init = 1;

	return ret;
}

void fsm_firmware_deinit(void)
{
	fsm_set_presets(NULL);
	g_fsm_fw_init = 0;
}
#endif
