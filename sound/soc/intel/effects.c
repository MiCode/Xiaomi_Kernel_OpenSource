/*
 *  effects.c - platform file for effects interface
 *
 *  Copyright (C) 2013 Intel Corporation
 *  Authors:	Samreen Nilofer <samreen.nilofer@intel.com>
 *		Vinod Koul <vinod.koul@intel.com>
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
#include <linux/slab.h>
#include <asm/platform_sst_audio.h>
#include "platform_ipc_v2.h"
#include "sst_platform.h"
#include "sst_platform_pvt.h"

extern struct sst_device *sst_dsp;
extern struct device *sst_pdev;

struct effect_uuid {
	uint32_t timeLow;
	uint16_t timeMid;
	uint16_t timeHiAndVersion;
	uint16_t clockSeq;
	uint8_t node[6];
};

#define EFFECT_STRING_LEN_MAX 64

enum sst_effect {
	EFFECTS_CREATE = 0,
	EFFECTS_DESTROY,
	EFFECTS_SET_PARAMS,
	EFFECTS_GET_PARAMS,
};

enum sst_mixer_output_mode {
	SST_MEDIA0_OUT,
	SST_MEDIA1_OUT,
};

static inline void sst_fill_byte_stream(struct snd_sst_bytes_v2 *bytes, u8 type,
			u8 msg, u8 block, u8 task, u8 pipe_id, u16 len,
			struct ipc_effect_payload *payload)
{
	u32 size = sizeof(struct ipc_effect_dsp_hdr);

	bytes->type = type;
	bytes->ipc_msg = msg;
	bytes->block = block;
	bytes->task_id = task;
	bytes->pipe_id = pipe_id;
	bytes->len = len;

	/* Copy the ipc_effect_dsp_hdr followed by the data */
	memcpy(bytes->bytes, payload, size);
	memcpy(bytes->bytes + size, payload->data, len - size);
}

static int sst_send_effects(struct ipc_effect_payload *dsp_payload, int data_len,
					enum sst_effect effect_type)
{
	struct snd_sst_bytes_v2 *bytes;
	u32 len;
	int ret;
	u8 type, msg = IPC_INVALID, pipe, payload_len;
	struct sst_data *sst;

	if (!sst_pdev)
		return -ENODEV;
	sst =  dev_get_drvdata(sst_pdev);

	len = sizeof(*bytes) + sizeof(struct ipc_effect_dsp_hdr) + data_len;

	bytes = kzalloc(len, GFP_KERNEL);
	if (!bytes) {
		pr_err("kzalloc failed allocate bytes\n");
		return -ENOMEM;
	}

	switch (effect_type) {
	case EFFECTS_CREATE:
	case EFFECTS_DESTROY:
		type = SND_SST_BYTES_SET;
		msg = IPC_CMD;
		break;

	case EFFECTS_SET_PARAMS:
		type = SND_SST_BYTES_SET;
		msg = IPC_SET_PARAMS;
		break;

	case EFFECTS_GET_PARAMS:
		type = SND_SST_BYTES_GET;
		msg =  IPC_GET_PARAMS;
		break;
	default:
		pr_err("No such effect %#x", effect_type);
		ret = -EINVAL;
		goto free_bytes;
	}

	pipe = dsp_payload->dsp_hdr.pipe_id;
	payload_len = sizeof(struct ipc_effect_dsp_hdr) + data_len;
	sst_fill_byte_stream(bytes, type, msg, 1, SST_TASK_ID_MEDIA,
				pipe, payload_len, dsp_payload);

	mutex_lock(&sst->lock);
	ret = sst_dsp->ops->set_generic_params(SST_SET_BYTE_STREAM, bytes);
	mutex_unlock(&sst->lock);

	if (ret) {
		pr_err("byte_stream failed err %d pipe_id %#x\n", ret,
				dsp_payload->dsp_hdr.pipe_id);
		goto free_bytes;
	}

	/* Copy only the data - skip the dsp header */
	if (msg == IPC_GET_PARAMS)
		memcpy(dsp_payload->data, bytes->bytes, data_len);

free_bytes:
	kfree(bytes);
	return ret;
}

static int sst_get_algo_id(const struct sst_dev_effects *pdev_effs,
					char *uuid, u16 *algo_id)
{
	int i, len;

	len = pdev_effs->effs_num_map;

	for (i = 0; i < len; i++) {
		if (!strncmp(pdev_effs->effs_map[i].uuid, uuid, sizeof(struct effect_uuid))) {
			*algo_id = pdev_effs->effs_map[i].algo_id;
			return 0;
		}
	}
	pr_err("no such uuid\n");
	return -EINVAL;
}

static int sst_fill_effects_info(const struct sst_dev_effects *pdev_effs,
					char *uuid, u16 pos,
					struct ipc_dsp_effects_info *effs_info, u16 cmd_id)
{
	int i, len;

	len = pdev_effs->effs_num_map;

	for (i = 0; i < len; i++) {
		if (!strncmp(pdev_effs->effs_map[i].uuid, uuid, sizeof(struct effect_uuid))) {

			effs_info->cmd_id = cmd_id;
			effs_info->length = (sizeof(struct ipc_dsp_effects_info) -
						offsetof(struct ipc_dsp_effects_info, sel_pos));
			effs_info->sel_pos = pos;
			effs_info->sel_algo_id = pdev_effs->effs_map[i].algo_id;
			effs_info->cpu_load = pdev_effs->effs_res_map[i].cpuLoad;
			effs_info->memory_usage = pdev_effs->effs_res_map[i].memoryUsage;
			effs_info->flags = pdev_effs->effs_res_map[i].flags;

			return 0;
		}
	}

	pr_err("no such uuid\n");
	return -EINVAL;
}

static inline void sst_fill_dsp_payload(struct ipc_effect_payload *dsp_payload,
					u8 pipe_id, u16 mod_id, char *data)
{
	dsp_payload->dsp_hdr.mod_index_id = 0xFF;
	dsp_payload->dsp_hdr.pipe_id = pipe_id;
	dsp_payload->dsp_hdr.mod_id = mod_id;
	dsp_payload->data = data;
}

static int sst_get_pipe_id(struct sst_dev_stream_map *map, int map_size,
				u32 dev, u32 mode, u8 *pipe_id)
{
	int index;

	if (map == NULL)
		return -EINVAL;

	/* In case of global effects, dev will be 0xff */
	if (dev == 0xFF) {
		*pipe_id = (mode == SST_MEDIA0_OUT) ? PIPE_MEDIA0_OUT : PIPE_MEDIA1_OUT;
		return 0;
	}

	for (index = 1; index < map_size; index++) {
		if (map[index].dev_num == dev) {
			*pipe_id = map[index].device_id;
			break;
		}
	}

	if (index == map_size) {
		pr_err("no such device %d\n", dev);
		return -ENODEV;
	}
	return 0;
}

static int sst_effects_create(struct snd_card *card, struct snd_effect *effect)
{
	int ret = 0;
	u8 pipe_id;
	struct ipc_effect_payload dsp_payload;
	struct ipc_dsp_effects_info effects_info;
	struct sst_data *sst;

	if (!sst_pdev)
		return -ENODEV;
	sst =  dev_get_drvdata(sst_pdev);

	ret = sst_fill_effects_info(&sst->pdata->pdev_effs, effect->uuid, effect->pos,
				 &effects_info, IPC_EFFECTS_CREATE);
	if (ret < 0)
		return ret;

	ret = sst_get_pipe_id(sst->pdata->pdev_strm_map,
				sst->pdata->strm_map_size,
				effect->device, effect->mode, &pipe_id);
	if (ret < 0)
		return ret;

	sst_fill_dsp_payload(&dsp_payload, pipe_id, 0xFF, (char *)&effects_info);

	ret = sst_send_effects(&dsp_payload, sizeof(effects_info), EFFECTS_CREATE);

	if (ret < 0)
		return ret;

	return 0;
}

static int sst_effects_destroy(struct snd_card *card, struct snd_effect *effect)
{
	int ret = 0;
	u8 pipe_id;
	struct ipc_effect_payload dsp_payload;
	struct ipc_dsp_effects_info effects_info;
	struct sst_data *sst;

	if (!sst_pdev)
		return -ENODEV;
	sst =  dev_get_drvdata(sst_pdev);

	ret = sst_fill_effects_info(&sst->pdata->pdev_effs, effect->uuid, effect->pos,
				&effects_info, IPC_EFFECTS_DESTROY);
	if (ret < 0)
		return ret;

	ret = sst_get_pipe_id(sst->pdata->pdev_strm_map,
				sst->pdata->strm_map_size,
				effect->device, effect->mode, &pipe_id);
	if (ret < 0)
		return ret;

	sst_fill_dsp_payload(&dsp_payload, pipe_id, 0xFF, (char *)&effects_info);

	ret = sst_send_effects(&dsp_payload, sizeof(effects_info), EFFECTS_DESTROY);

	if (ret < 0)
		return ret;

	return 0;
}

static int sst_effects_set_params(struct snd_card *card,
					struct snd_effect_params *params)
{
	int ret = 0;
	u8 pipe_id;
	u16 algo_id;
	struct ipc_effect_payload dsp_payload;
	struct sst_data *sst;

	if (!sst_pdev)
		return -ENODEV;
	sst =  dev_get_drvdata(sst_pdev);

	ret = sst_get_algo_id(&sst->pdata->pdev_effs, params->uuid, &algo_id);
	if (ret < 0)
		return ret;

	ret = sst_get_pipe_id(sst->pdata->pdev_strm_map,
				sst->pdata->strm_map_size,
				params->device, SST_MEDIA0_OUT, &pipe_id);
	if (ret < 0)
		return ret;

	sst_fill_dsp_payload(&dsp_payload, pipe_id, algo_id,
			(void *)(unsigned long)params->buffer_ptr);

	ret = sst_send_effects(&dsp_payload, params->size, EFFECTS_SET_PARAMS);

	if (ret < 0)
		return ret;

	return 0;
}

static int sst_effects_get_params(struct snd_card *card,
					struct snd_effect_params *params)
{
	int ret = 0;
	u8 pipe_id;
	u16 algo_id;
	struct ipc_effect_payload dsp_payload;
	struct sst_data *sst;

	if (!sst_pdev)
		return -ENODEV;
	sst =  dev_get_drvdata(sst_pdev);

	ret = sst_get_algo_id(&sst->pdata->pdev_effs, params->uuid, &algo_id);
	if (ret < 0)
		return ret;

	ret = sst_get_pipe_id(sst->pdata->pdev_strm_map,
				sst->pdata->strm_map_size,
				params->device, SST_MEDIA0_OUT, &pipe_id);
	if (ret < 0)
		return ret;

	sst_fill_dsp_payload(&dsp_payload, pipe_id, algo_id,
			(void *)(unsigned long)params->buffer_ptr);

	ret = sst_send_effects(&dsp_payload, params->size, EFFECTS_GET_PARAMS);

	if (ret < 0)
		return ret;

	return 0;
}

static int sst_query_num_effects(struct snd_card *card)
{
	struct sst_data *sst;

	if (!sst_pdev)
		return -ENODEV;
	sst =  dev_get_drvdata(sst_pdev);

	return sst->pdata->pdev_effs.effs_num_map;
}

static int sst_query_effects_caps(struct snd_card *card,
					struct snd_effect_caps *caps)
{
	struct sst_data *sst;
	struct sst_dev_effects_map *effs_map;
	unsigned int num_effects, offset = 0;
	char *dstn;
	int i;

	if (!sst_pdev)
		return -ENODEV;
	sst =  dev_get_drvdata(sst_pdev);

	effs_map = sst->pdata->pdev_effs.effs_map;
	num_effects = sst->pdata->pdev_effs.effs_num_map;

	if (caps->size < (num_effects * MAX_DESCRIPTOR_SIZE)) {
		pr_err("buffer size is insufficient\n");
		return -ENOMEM;
	}

	dstn = (void *)(unsigned long)caps->buffer_ptr;
	for (i = 0; i < num_effects; i++) {
		memcpy(dstn + offset, effs_map[i].descriptor, MAX_DESCRIPTOR_SIZE);
		offset += MAX_DESCRIPTOR_SIZE;
	}
	caps->size = offset;

	return 0;
}

struct snd_effect_ops effects_ops = {
	.create = sst_effects_create,
	.destroy = sst_effects_destroy,
	.set_params = sst_effects_set_params,
	.get_params = sst_effects_get_params,
	.query_num_effects = sst_query_num_effects,
	.query_effect_caps = sst_query_effects_caps,
};
