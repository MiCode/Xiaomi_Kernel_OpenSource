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
#define LOG_FLAG	"sipa_parameter"

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include "sipa_tuning_if.h"
#include "sipa_timer_task.h"
#include "sipa_tuning_cmd.h"
#include "sipa_parameter.h"

#define LOAD_TIME_OUT 	(5000)

static const char *sipa_fw_name = "sipa.bin";
static const char *sipa_fw_name_w = "sipa_w.bin";
#ifdef PLATFORM_TYPE_MTK
static const char *sipa_fw_path_w = "/vendor/firmware";
#else
static const char *sipa_fw_path_w = "/etc/firmware";
#endif
static SIPA_PARAM *sipa_parameters;
static uint32_t sipa_fw_loaded;
static struct mutex sipa_fw_load_lock;

void sipa_fw_loaded_work(struct work_struct *work)
{
	int ret = 0;
	uint32_t sz = 0;
	const SIPA_PARAM_FW *param = NULL;
	sipa_dev_t *si_pa = container_of(work, sipa_dev_t, fw_load_work.work);
	const struct firmware *cont = NULL;
#ifdef SIA91XX_TYPE
	const SIPA_PARAM_WRITEABLE *param_w = NULL;
	const struct firmware *fw_w = NULL;
#endif

	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: NULL == si_pa \r\n", LOG_FLAG, __func__);
		return;
	}

	// 多声道的fw_load动作是并行处�?	mutex_lock(&sipa_fw_load_lock);

	if (1 == sipa_fw_loaded || NULL != sipa_parameters) {
		goto pending_actions;
	}

	ret = request_firmware(&cont, sipa_fw_name, &si_pa->pdev->dev);
	if (ret) {
		pr_err("[  err][%s]: request_firmware err ret = %d\r\n", LOG_FLAG, ret);
		goto load_error;
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

	/* load writeable fw */
#ifdef SIA91XX_TYPE
	ret = request_firmware(&fw_w, sipa_fw_name_w, &si_pa->pdev->dev);
	if (ret) {
		pr_err("[  err][%s]: request_firmware err ret = %d\r\n", LOG_FLAG, ret);
		goto load_error;
	}

	if (sizeof(SIPA_PARAM_WRITEABLE) != fw_w->size) {
		pr_err("[  err][%s]: fw_w->size = %lu\r\n", LOG_FLAG, fw_w->size);
		goto load_error;
	}

	param_w = (SIPA_PARAM_WRITEABLE *)fw_w->data;

	if (SIPA_FW_VER != param_w->version) {
		pr_err("[  err][%s] %s: SIPA_FW_VER(0x%08x) != param_w->version(0x%08x) \r\n",
			LOG_FLAG, __func__, SIPA_FW_VER, param_w->version);
		goto load_error;
	}

#if 0
    //sipa_w.bin文件涉及到喇叭模型的参数，值会有修改，修改后会使得crc的值与原始值不等�?	if (param_w->crc != crc32((uint8_t *)&param_w->version, sizeof(SIPA_PARAM_WRITEABLE) - sizeof(uint32_t))) {
		pr_err("[  err][%s] %s: param_w crc(0x%x) check failed! \r\n",
		   LOG_FLAG, __func__, param_w->crc);
		goto load_error;
	}
#endif

	memcpy(&sipa_parameters->writeable, fw_w->data, sizeof(SIPA_PARAM_WRITEABLE));
	release_firmware(fw_w);
#endif

	sipa_fw_loaded = 1;
pending_actions:
	mutex_unlock(&sipa_fw_load_lock);
	sipa_pending_actions(si_pa);
	return;

load_error:
	if (NULL != sipa_parameters) {
		kfree(sipa_parameters);
		sipa_parameters = NULL;
	}

	sipa_fw_loaded = 0;
	mutex_unlock(&sipa_fw_load_lock);
	release_firmware(cont);

#ifdef SIA91XX_TYPE
	release_firmware(fw_w);
#endif
}

void sipa_param_load_fw(struct device *dev)
{
	sipa_dev_t *si_pa = dev_get_drvdata(dev);

	if (NULL == si_pa) {
		pr_err("[  err][%s] %s: NULL == si_pa \r\n", LOG_FLAG, __func__);
		return;
	}

	if (0 == si_pa->channel_num)
		mutex_init(&sipa_fw_load_lock);

	INIT_DELAYED_WORK(&si_pa->fw_load_work, sipa_fw_loaded_work);
	queue_delayed_work(si_pa->sia91xx_wq, &si_pa->fw_load_work, msecs_to_jiffies(LOAD_TIME_OUT));
}

void sipa_param_release(void)
{
	sipa_fw_loaded = 0;

	if (NULL != sipa_parameters) {
		kfree(sipa_parameters);
		sipa_parameters = NULL;
	}
}

int sipa_param_read_spk_calibration(
	uint32_t ch,
	SIPA_PARAM_CAL_SPK *cal_spk)
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

	if (NULL == cal_spk) {
		pr_err("[  err][%s] %s: NULL == cal_spk \r\n", LOG_FLAG, __func__);
		return -EINVAL;
	}

	memcpy(cal_spk,
		&sipa_parameters->writeable.cal_spk[ch], sizeof(SIPA_PARAM_CAL_SPK));

	return 0;
}

int sipa_param_write_spk_calibration(
	uint32_t ch,
	uint32_t is_ok,
	int32_t r0,
	int32_t t0,
	int32_t wire_r0,
	int32_t a)
{
	struct file *file;
	mm_segment_t fs;
	loff_t pos = 0;
	char fw_path[256];
	//int ret = 0;

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

	sipa_parameters->writeable.cal_spk[ch].cal_ok = is_ok;
	sipa_parameters->writeable.cal_spk[ch].r0 = r0;
	sipa_parameters->writeable.cal_spk[ch].t0 = t0;
	sipa_parameters->writeable.cal_spk[ch].wire_r0 = wire_r0;
	sipa_parameters->writeable.cal_spk[ch].a = a;

	pos = (loff_t)((void *)&sipa_parameters->writeable.cal_spk[ch] -
		(void *)&sipa_parameters->writeable);

	sprintf(fw_path, "%s/%s", sipa_fw_path_w, sipa_fw_name_w);
	file = filp_open(fw_path, O_WRONLY, 0);
	if (IS_ERR(file)) {
		pr_err("[  err][%s] %s: open file err %d \r\n",
			LOG_FLAG, __func__, (int)(PTR_ERR(file)));
		return -EEXEC;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

	//ret = kernel_write(file, (const char *)&sipa_parameters->writeable.cal_spk[ch],
		//sizeof(SIPA_PARAM_CAL_SPK), &pos);

	set_fs(fs);

	filp_close(file, NULL);

	pr_info("[ info][%s] %s done : cal_ok(%u)"
		"r0(%d), t0(%d), wire_r0(%d), a(%d),  \r\n",
		LOG_FLAG, __func__, is_ok, r0, t0, wire_r0, a);

	return 0;
}

int sipa_param_write_spk_r0(
	uint32_t ch,
	uint32_t r0,
	uint32_t cal_ok)
{
	struct file *file;
	mm_segment_t fs;
	loff_t pos = 0;
	char fw_path[256];
	//int ret = 0;

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
		pr_err("[  err][%s] %s: 1 != sipa_parameters.ch_en[%u](%u) \r\n",
			LOG_FLAG, __func__, ch, sipa_parameters->fw.ch_en[ch]);
		return -EINVAL;
	}

	sipa_parameters->writeable.cal_spk[ch].r0 = r0;
	sipa_parameters->writeable.cal_spk[ch].cal_ok = cal_ok;

	pos = (loff_t)((void *)&sipa_parameters->writeable.cal_spk[ch] -
		(void *)&sipa_parameters->writeable);

	sprintf(fw_path, "%s/%s", sipa_fw_path_w, sipa_fw_name_w);
	file = filp_open(fw_path, O_WRONLY, 0);
	if (IS_ERR(file)) {
		pr_err("[  err][%s] %s: open file err %d \r\n",
			LOG_FLAG, __func__, (int)(PTR_ERR(file)));
		return -EEXEC;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

	//ret = kernel_write(file, (const char *)&sipa_parameters->writeable.cal_spk[ch],
		//sizeof(SIPA_PARAM_CAL_SPK), &pos);

	set_fs(fs);

	filp_close(file, NULL);

	pr_info("[ info][%s] %s done : r0(%d) \r\n",
		LOG_FLAG, __func__, r0);

	return 0;
}

int sipa_param_read_spk_model(
	uint32_t ch,
	SIPA_PARAM_SPK_MODEL_PARAM *spk_model)
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

	if (NULL == spk_model) {
		pr_err("[  err][%s] %s: NULL == spk_model \r\n", LOG_FLAG, __func__);
		return -EINVAL;
	}

	memcpy(spk_model, &sipa_parameters->writeable.spk_model[ch],
		sizeof(SIPA_PARAM_SPK_MODEL_PARAM));

	return 0;
}

int sipa_param_write_spk_model(
	uint32_t ch,
	SIPA_PARAM_SPK_MODEL_PARAM *spk_model)
{

	struct file *file;
	mm_segment_t fs;
	loff_t pos = 0;
	char fw_path[256];
	//int ret = 0;

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

	sipa_parameters->writeable.spk_model[ch].f0 = spk_model->f0;
	sipa_parameters->writeable.spk_model[ch].q  = spk_model->q;
	sipa_parameters->writeable.spk_model[ch].xthresh = spk_model->xthresh;
	sipa_parameters->writeable.spk_model[ch].xthresh_rdc = spk_model->xthresh_rdc;

	pos = (loff_t)((void *)&sipa_parameters->writeable.spk_model[ch] -
		(void *)&sipa_parameters->writeable);

	sprintf(fw_path, "%s/%s", sipa_fw_path_w, sipa_fw_name_w);
	file = filp_open(fw_path, O_WRONLY, 0);
	if (IS_ERR(file)) {
		pr_err("[  err][%s] %s: open file err %d \r\n",
			LOG_FLAG, __func__, (int)(PTR_ERR(file)));
		return -EEXEC;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

	//ret = kernel_write(file, (const char *)&sipa_parameters->writeable.spk_model[ch],
		//sizeof(SIPA_PARAM_SPK_MODEL_PARAM), &pos);

	set_fs(fs);

	filp_close(file, NULL);

	pr_info("[ info][%s] %s done \r\n",
		LOG_FLAG, __func__);

	return 0;
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


/* only for debug or test */
void sipa_param_create_default_param(void)
{
#if 0
	struct file *file;
	mm_segment_t fs;
	loff_t pos = 0;
	char fw_path[256];
	int ret = 0, i = 0;

	memset((void *)&sipa_parameters, 0, sizeof(sipa_parameters));

	for (i = 0; i < SIPA_CHANNEL_NUM; i++) {
		sipa_parameters.ch_en[i] = 1;
		sipa_parameters.chip_cfg[i].chip_type = CHIP_TYPE_INVALID;
		sipa_parameters.cal_spk[i].cal_ok = 0;
		sipa_parameters.cal_spk[i].max_delta_r0 = 1000;/* 1ohm */
		sipa_parameters.cal_spk[i].max_r0 = 9000;/* 9ohm */
		sipa_parameters.cal_spk[i].min_r0 = 4000;/* 4ohm */

		sipa_parameters.spk_model[i].f0 = 760;
		sipa_parameters.spk_model[i].q = 1980;
		sipa_parameters.spk_model[i].xthresh = 500;
		sipa_parameters.spk_model[i].xthresh_rdc = 7000;

	}

	sprintf(fw_path, "%s/%s", sipa_fw_path, sipa_fw_name);
	file = filp_open(fw_path, O_WRONLY | O_CREAT, 0);
	if (IS_ERR(file)) {
		pr_err("[  err][%s] %s: open file(%s) err %d \r\n",
			LOG_FLAG, __func__, fw_path, (int)(PTR_ERR(file)));
		return ;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

	//ret = kernel_write(file, (const char *)&sipa_parameters,
		//sizeof(sipa_parameters), &pos);

	set_fs(fs);

	filp_close(file, NULL);

	sipa_fw_loaded = 1;

	//pr_info("[ info][%s] %s: kernel_write done ret = %d \r\n", LOG_FLAG, __func__, ret);
#endif
}

/* only for debug or test */
void sipa_param_print(void)
{
	int i = 0;

	if (1 != sipa_fw_loaded || NULL == sipa_parameters) {
		pr_err("[  err][%s] %s: firmware unload \r\n", LOG_FLAG, __func__);
		return;
	}

	for (i = 0; i < SIPA_CHANNEL_NUM; i++) {
		pr_info("[ info][%s] %s: ch_en[%d](%u) \r\n",
			LOG_FLAG, __func__, i, sipa_parameters->fw.ch_en[i]);

		pr_info("[ info][%s] %s: cal_ok[%d](%u)"
			"r0[%d](%d), t0[%d](%d), "
			"wire_r0[%d](%d), a[%d](%d),"
			"min_r0[%d](%d), max_r0[%d](%d), max_delta_r0[%d](%d),"
			"f0[%d](%d), q[%d](%d), xthresh[%d](%d), xthresh_rdc[%d](%d) \r\n",
			LOG_FLAG, __func__,
			i, sipa_parameters->writeable.cal_spk[i].cal_ok,
			i, sipa_parameters->writeable.cal_spk[i].r0,
			i, sipa_parameters->writeable.cal_spk[i].t0,
			i, sipa_parameters->writeable.cal_spk[i].wire_r0,
			i, sipa_parameters->writeable.cal_spk[i].a,
			i, sipa_parameters->fw.extra_cfg[i].spk_min_r0,
			i, sipa_parameters->fw.extra_cfg[i].spk_max_r0,
			i, sipa_parameters->fw.extra_cfg[i].spk_max_delta_r0,
			i, sipa_parameters->writeable.spk_model[i].f0,
			i, sipa_parameters->writeable.spk_model[i].q,
			i, sipa_parameters->writeable.spk_model[i].xthresh,
			i, sipa_parameters->writeable.spk_model[i].xthresh_rdc);
	}
}
