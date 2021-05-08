/*
 * awinic_dsp.c  aw87xxx pa module
 *
 * Copyright (c) 2020 AWINIC Technology CO., LTD
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Author: Alex <zhaozhongbo@awinic.com>
 *
 */
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/gameport.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include "aw87xxx.h"
#include "aw87xxx_monitor.h"
#include "awinic_dsp.h"

static DEFINE_MUTEX(g_msg_dsp_lock);

#ifdef AW_MTK_OPEN_DSP_PLATFORM
extern int mtk_spk_send_ipi_buf_to_dsp(void *data_buffer, int32_t data_size);
extern int mtk_spk_recv_ipi_buf_from_dsp(int8_t *buffer,
					int16_t size, int32_t *buf_len);
#else

static int mtk_spk_send_ipi_buf_to_dsp(void *data_buffer, int32_t data_size)
{
	return 0;
}

static int mtk_spk_recv_ipi_buf_from_dsp(int8_t *buffer,
					int16_t size, int32_t *buf_len)
{
	return 0;
}
#endif

bool aw87xx_platform_init(void)
{
#ifdef AW_MTK_OPEN_DSP_PLATFORM
	return true;
#else
	return false;
#endif
}

static int aw_mtk_write_data_to_dsp(int param_id,
				void *data, int data_size, int channel)
{
	int32_t *dsp_data = NULL;
	struct aw_dsp_msg_hdr *hdr = NULL;
	int ret;

	pr_debug("%s: param id = 0x%x", __func__, param_id);

	dsp_data = kzalloc(sizeof(struct aw_dsp_msg_hdr) + data_size,
			GFP_KERNEL);
	if (!dsp_data) {
		pr_err("%s: kzalloc dsp_msg error\n", __func__);
		return -ENOMEM;
	}

	hdr = (struct aw_dsp_msg_hdr *)dsp_data;
	hdr->type = DSP_MSG_TYPE_DATA;
	hdr->opcode_id = param_id;
	hdr->version = AWINIC_DSP_MSG_HDR_VER;

	memcpy(((char *)dsp_data) + sizeof(struct aw_dsp_msg_hdr),
			data, data_size);
	ret = mtk_spk_send_ipi_buf_to_dsp(dsp_data,
			sizeof(struct aw_dsp_msg_hdr) + data_size);
	if (ret < 0) {
		pr_err("%s:write data failed\n", __func__);
		kfree(dsp_data);
		dsp_data = NULL;
		return ret;
	}

	kfree(dsp_data);
	dsp_data = NULL;
	return 0;
}

static int aw_mtk_read_data_from_dsp(int param_id,
			void *data, int data_size, int channel)
{
	int ret;
	struct aw_dsp_msg_hdr hdr;


	pr_debug("%s: param id = 0x%x", __func__, param_id);
	hdr.type = DSP_MSG_TYPE_CMD;
	hdr.opcode_id = param_id;
	hdr.version = AWINIC_DSP_MSG_HDR_VER;

	mutex_lock(&g_msg_dsp_lock);

	ret = mtk_spk_send_ipi_buf_to_dsp(&hdr, sizeof(struct aw_dsp_msg_hdr));
	if (ret < 0) {
		pr_err("%s:send cmd failed\n", __func__);
		goto dsp_msg_failed;
	}

	ret = mtk_spk_recv_ipi_buf_from_dsp(data, data_size, &data_size);
	if (ret < 0) {
		pr_err("%s:get data failed\n", __func__);
		goto dsp_msg_failed;
	}
	mutex_unlock(&g_msg_dsp_lock);
	return 0;

dsp_msg_failed:
	mutex_unlock(&g_msg_dsp_lock);
	return ret;
}

int aw_get_vmax_from_dsp(uint32_t *vmax, int32_t channel)
{
	int ret;
	int param_id;

	if (channel == AW_CHANNEL_LEFT) {
		param_id = AFE_PARAM_ID_AWDSP_RX_VMAX_L;
	} else if (channel == AW_CHANNEL_RIGHT) {
		param_id = AFE_PARAM_ID_AWDSP_RX_VMAX_R;
	} else {
		pr_err("%s : channel error\n", __func__);
		return -EINVAL;
	}

	ret = aw_mtk_read_data_from_dsp(param_id,
			(void *)vmax, sizeof(uint32_t), channel);
	if (ret < 0) {
		pr_err("%s: get vmax failed\n", __func__);
		return ret;
	}

	return 0;
}

int aw_set_vmax_to_dsp(uint32_t vmax, int32_t channel)
{
	int ret;
	int param_id;

	if (channel == AW_CHANNEL_LEFT) {
		param_id = AFE_PARAM_ID_AWDSP_RX_VMAX_L;
	} else if (channel == AW_CHANNEL_RIGHT) {
		param_id = AFE_PARAM_ID_AWDSP_RX_VMAX_R;
	} else {
		pr_err("%s : channel error\n", __func__);
		return -EINVAL;
	}

	ret =  aw_mtk_write_data_to_dsp(param_id,
			&vmax, sizeof(uint32_t), channel);
	if (ret < 0) {
		pr_err("%s : set vmax failed\n", __func__);
		return ret;
	}

	return 0;
}

