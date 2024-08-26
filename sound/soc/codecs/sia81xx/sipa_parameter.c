/*
 * Copyright (C) 2020 SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DEBUG
#define LOG_FLAG	"sipa_param "

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include "sipa_tuning_if.h"
#include "sipa_parameter.h"

#define LOAD_FW_BY_WORK_DELAY
#define LOAD_INTERVAL_TIMES 	(2000)
#define LOAD_FW_RETRIES			(15)

char *g_fw_name;
static SIPA_PARAM *sipa_parameters = NULL;
static uint32_t sipa_fw_loaded = 0;
static struct mutex sipa_fw_load_lock;

extern int sipa_pending_actions(sipa_dev_t *si_pa);

static void sipa_fw_load_retry(sipa_dev_t *si_pa)
{
	si_pa->fw_load_count++;
	if (si_pa->fw_load_count < LOAD_FW_RETRIES) {
		queue_delayed_work(si_pa->sipa_wq, 
			&si_pa->fw_load_work, msecs_to_jiffies(LOAD_INTERVAL_TIMES));
	} else {
		pr_err("[  err][%s] %s: load retries failed !\r\n",
			LOG_FLAG, __func__);
		si_pa->fw_load_count = 0;
	}
}

#ifndef LOAD_FW_BY_WORK_DELAY
static void sipa_container_loaded(
	const struct firmware *cont,
	void *context)
{
	uint32_t sz = 0;
	const SIPA_PARAM_FW *param = NULL;
	sipa_dev_t *si_pa = context;

	pr_debug("[debug][%s] %s: enter load\r\n", LOG_FLAG, __func__);

	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: NULL == si_pa \r\n", LOG_FLAG, __func__);
		return;
	}

	if (NULL == cont) {
		pr_debug("[debug][%s] %s: load fw failed, need retry!\r\n",
			LOG_FLAG, __func__);
		sipa_fw_load_retry(si_pa);
		return;
	}

	// 多声道的fw_load动作是并行处理
	mutex_lock(&sipa_fw_load_lock);

	if (1 == sipa_fw_loaded || NULL != sipa_parameters) {
		goto pending_actions;
	}

	sz = sizeof(SIPA_PARAM_FW);
	if (sz > cont->size) {
		pr_err("[  err][%s] %s: sizeof(SIPA_PARAM_FW)(%u) > cont->size(%lu) \r\n",
			LOG_FLAG, __func__, sz, cont->size);
		goto load_error;
	}

	param = (SIPA_PARAM_FW *)cont->data;
	sz += param->data_size;

	if (sz > cont->size) {
		pr_err("[  err][%s] %s: sz(%u) > cont->size(%lu) \r\n",
			LOG_FLAG, __func__, sz, cont->size);
		goto load_error;
	} else if (sz < cont->size) {
		pr_warn("[ warn][%s] %s: sz(%u) < cont->size(%lu) \r\n",
			LOG_FLAG, __func__, sz, cont->size);
	}

	if (SIPA_FW_VER != param->version) {
		pr_err("[  err][%s] %s: SIPA_FW_VER(0x%08x) != param->version(0x%08x) \r\n",
			LOG_FLAG, __func__, SIPA_FW_VER, param->version);
		goto load_error;
	}

	if (param->crc != crc32((uint8_t *)&param->version, sz - sizeof(uint32_t))) {
		pr_err("[  err][%s] %s: param crc(0x%x) check failed! \r\n",
		   LOG_FLAG, __func__, param->crc);
		goto load_error;
	}

	sipa_parameters = kzalloc(sizeof(SIPA_PARAM_WRITEABLE) + sz, GFP_KERNEL);
	if (NULL == sipa_parameters) {
		pr_err("[  err][%s] %s: kmalloc failed \r\n",
			LOG_FLAG, __func__);
		goto load_error;
	}

	memcpy(&sipa_parameters->fw, cont->data, sz);
	sipa_fw_loaded = 1;

pending_actions:
	mutex_unlock(&sipa_fw_load_lock);
	release_firmware(cont);
	sipa_pending_actions(si_pa);
	pr_debug("[debug][%s] %s: out load\r\n", LOG_FLAG, __func__);
	return;

load_error:
	if (NULL != sipa_parameters) {
		kfree(sipa_parameters);
		sipa_parameters = NULL;
	}

	sipa_fw_loaded = 0;
	mutex_unlock(&sipa_fw_load_lock);

	release_firmware(cont);
}

static void sipa_fw_load_work_routine(struct work_struct *work)
{
	sipa_dev_t *si_pa = container_of(work, sipa_dev_t, fw_load_work.work);

	pr_debug("[debug][%s] %s: enter load work routine!\r\n", LOG_FLAG, __func__);

	request_firmware_nowait(
			THIS_MODULE,
			FW_ACTION_HOTPLUG,
			g_fw_name,
			&(si_pa->pdev->dev),
			GFP_KERNEL,
			si_pa,
			sipa_container_loaded);
}

#else
static void sipa_fw_load_work_delay(struct work_struct *work)
{
	int ret = 0;
	uint32_t sz = 0;
	const SIPA_PARAM_FW *param = NULL;
	sipa_dev_t *si_pa = container_of(work, sipa_dev_t, fw_load_work.work);
	const struct firmware *cont = NULL;

	pr_debug("[debug][%s] %s: enter load\r\n", LOG_FLAG, __func__);

	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: NULL == si_pa \r\n", LOG_FLAG, __func__);
		return;
	}

	// 多声道的fw_load动作是并行处理
	mutex_lock(&sipa_fw_load_lock);

	if (1 == sipa_fw_loaded || NULL != sipa_parameters) {
		goto pending_actions;
	}

	ret = request_firmware(&cont, g_fw_name, &si_pa->pdev->dev);
	if (ret) {
		pr_debug("[debug][%s] %s: load fw failed, need retry! ret = %d.\r\n",
			LOG_FLAG, __func__, ret);
		mutex_unlock(&sipa_fw_load_lock);
		sipa_fw_load_retry(si_pa);
		return;
	}

	sz = sizeof(SIPA_PARAM_FW);
	if (sz > cont->size) {
		pr_err("[  err][%s] %s: sizeof(SIPA_PARAM_FW)(%u) > cont->size(%lu) \r\n",
			LOG_FLAG, __func__, sz, cont->size);
		goto load_error;
	}

	param = (SIPA_PARAM_FW *)cont->data;
	sz += param->data_size;

	if (sz > cont->size) {
		pr_err("[  err][%s] %s: sz(%u) > cont->size(%lu) \r\n",
			LOG_FLAG, __func__, sz, cont->size);
		goto load_error;
	} else if (sz < cont->size) {
		pr_warn("[ warn][%s] %s: sz(%u) < cont->size(%lu) \r\n",
			LOG_FLAG, __func__, sz, cont->size);
	}

	if (SIPA_FW_VER != param->version) {
		pr_err("[  err][%s] %s: SIPA_FW_VER(0x%08x) != param->version(0x%08x) \r\n",
			LOG_FLAG, __func__, SIPA_FW_VER, param->version);
		goto load_error;
	}

	if (param->crc != crc32((uint8_t *)&param->version, sz - sizeof(uint32_t))) {
		pr_err("[  err][%s] %s: param crc(0x%x) check failed! \r\n",
		   LOG_FLAG, __func__, param->crc);
		goto load_error;
	}

	sipa_parameters = kzalloc(sizeof(SIPA_PARAM_WRITEABLE) + sz, GFP_KERNEL);
	if (NULL == sipa_parameters) {
		pr_err("[  err][%s] %s: kmalloc failed \r\n",
			LOG_FLAG, __func__);
		goto load_error;
	}

	memcpy(&sipa_parameters->fw, cont->data, sz);
	release_firmware(cont);
	sipa_fw_loaded = 1;

pending_actions:
	mutex_unlock(&sipa_fw_load_lock);
	sipa_pending_actions(si_pa);
	pr_debug("[debug][%s] %s: out load\r\n", LOG_FLAG, __func__);
	return;

load_error:
	if (NULL != sipa_parameters) {
		kfree(sipa_parameters);
		sipa_parameters = NULL;
	}

	sipa_fw_loaded = 0;
	mutex_unlock(&sipa_fw_load_lock);
	release_firmware(cont);
}
#endif

void sipa_param_load_fw(struct device *dev, char *fwname)
{
	sipa_dev_t *si_pa = dev_get_drvdata(dev);
	static bool load_lock_init_flag = false;

	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: NULL == si_pa \r\n", LOG_FLAG, __func__);
		return;
	}

	if (false == load_lock_init_flag) {
		mutex_init(&sipa_fw_load_lock);
		load_lock_init_flag = true;
	}
	g_fw_name = fwname;

#ifndef LOAD_FW_BY_WORK_DELAY
	INIT_DELAYED_WORK(&si_pa->fw_load_work, sipa_fw_load_work_routine);
	queue_delayed_work(si_pa->sipa_wq, 
		&si_pa->fw_load_work, msecs_to_jiffies(0));
#else
	INIT_DELAYED_WORK(&si_pa->fw_load_work, sipa_fw_load_work_delay);
	queue_delayed_work(si_pa->sipa_wq, 
		&si_pa->fw_load_work, msecs_to_jiffies(LOAD_INTERVAL_TIMES));
#endif
}

void sipa_param_release(void)
{
	sipa_fw_loaded = 0;

	if (NULL != sipa_parameters) {
		kfree(sipa_parameters);
		sipa_parameters = NULL;
	}
}

void *sipa_param_read_chip_cfg(
	uint32_t ch, uint32_t chip_type,
	SIPA_CHIP_CFG *cfg)
{
	int i;
	const SIPA_PARAM_LIST *chip_cfg_list = NULL;
	SIPA_CHIP_CFG *chips_cfg = NULL;

	if (1 != sipa_fw_loaded || NULL == sipa_parameters) {
		pr_warn("[ warn][%s] %s: firmware unload \r\n", LOG_FLAG, __func__);
		return NULL;
	}

	if (ch >= SIPA_CHANNEL_NUM) {
		pr_err("[  err][%s] %s: ch(%u) >= SIPA_CHANNEL_NUM(%u) \r\n",
			LOG_FLAG, __func__, ch, SIPA_CHANNEL_NUM);
		return NULL;
	}

	if (1 != sipa_parameters->fw.ch_en[ch]) {
		pr_err("[  err][%s] %s: 1 != sipa_parameters.fw.ch_en[%u](%u) \r\n",
			LOG_FLAG, __func__, ch, sipa_parameters->fw.ch_en[ch]);
		return NULL;
	}

	if (NULL == cfg) {
		pr_err("[  err][%s] %s: NULL == cfg \r\n", LOG_FLAG, __func__);
		return NULL;
	}

	chip_cfg_list = &sipa_parameters->fw.chip_cfg[ch];
	if (chip_cfg_list->node_size != sizeof(SIPA_CHIP_CFG)) {
		pr_err("[  err][%s] %s: node_size(%d) != sizeof(SIPA_CHIP_CFG)(%lu)",
			LOG_FLAG, __func__, chip_cfg_list->node_size, sizeof(SIPA_CHIP_CFG));
		return NULL;
	}

	chips_cfg = (SIPA_CHIP_CFG *)(sipa_parameters->fw.data + chip_cfg_list->offset);
	for (i = 0; i < chip_cfg_list->num; i++) {
		if (chip_type == chips_cfg[i].chip_type) {
			memcpy(cfg,	&chips_cfg[i], sizeof(SIPA_CHIP_CFG));
			break;
		}
	}

	if (i >= chip_cfg_list->num) {
		pr_err("[  err][%s] %s: chip_type(%u) mismatch chip_cfg_list->num = %d version = %x\r\n", 
			LOG_FLAG, __func__, chip_type, chip_cfg_list->num, sipa_parameters->fw.version);
		return NULL;
	}

	return sipa_parameters->fw.data;
}

int sipa_param_read_extra_cfg(
	uint32_t ch,
	SIPA_EXTRA_CFG *cfg)
{
	if (1 != sipa_fw_loaded || NULL == sipa_parameters) {
		pr_err("[  err][%s] %s: firmware unload \r\n", LOG_FLAG, __func__);
		return -EINVAL;
	}

	if (ch >= SIPA_CHANNEL_NUM) {
		pr_err("[  err][%s] %s: ch(%u) >= SIPA_CHANNEL_NUM(%u) \r\n",
			LOG_FLAG, __func__, ch, SIPA_CHANNEL_NUM);
		return -EINVAL;
	}

	if (1 != sipa_parameters->fw.ch_en[ch]) {
		pr_err("[  err][%s] %s: 1 != sipa_parameters.fw.ch_en[%u](%u) \r\n",
			LOG_FLAG, __func__, ch, sipa_parameters->fw.ch_en[ch]);
		return -EINVAL;
	}

	if (NULL == cfg) {
		pr_err("[  err][%s] %s: NULL == cfg \r\n", LOG_FLAG, __func__);
		return -EINVAL;
	}

	memcpy(cfg, &sipa_parameters->fw.extra_cfg[ch], sizeof(SIPA_EXTRA_CFG));

	return 0;
}

const SIPA_PARAM *sipa_param_instance()
{
	if (1 != sipa_fw_loaded)
		return NULL;

	return sipa_parameters;
}

bool sipa_param_is_loaded(void)
{
	if (1 == sipa_fw_loaded && NULL != sipa_parameters)
		return true;

	return false;
}

