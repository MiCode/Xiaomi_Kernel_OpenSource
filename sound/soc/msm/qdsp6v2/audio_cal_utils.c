/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <sound/audio_cal_utils.h>

static int unmap_memory(struct cal_type_data *cal_type,
			struct cal_block_data *cal_block);

size_t get_cal_info_size(int32_t cal_type)
{
	size_t size = 0;

	switch (cal_type) {
	case CVP_VOC_RX_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_info_voc_top);
		break;
	case CVP_VOC_TX_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_info_voc_top);
		break;
	case CVP_VOCPROC_STATIC_CAL_TYPE:
		size = sizeof(struct audio_cal_info_vocproc);
		break;
	case CVP_VOCPROC_DYNAMIC_CAL_TYPE:
		size = sizeof(struct audio_cal_info_vocvol);
		break;
	case CVS_VOCSTRM_STATIC_CAL_TYPE:
		size = 0;
		break;
	case CVP_VOCDEV_CFG_CAL_TYPE:
		size = sizeof(struct audio_cal_info_vocdev_cfg);
		break;
	case CVP_VOCPROC_STATIC_COL_CAL_TYPE:
		size = sizeof(struct audio_cal_info_voc_col);
		break;
	case CVP_VOCPROC_DYNAMIC_COL_CAL_TYPE:
		size = sizeof(struct audio_cal_info_voc_col);
		break;
	case CVS_VOCSTRM_STATIC_COL_CAL_TYPE:
		size = sizeof(struct audio_cal_info_voc_col);
		break;
	case ADM_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_info_adm_top);
		break;
	case ADM_CUST_TOPOLOGY_CAL_TYPE:
	case CORE_CUSTOM_TOPOLOGIES_CAL_TYPE:
		size = 0;
		break;
	case ADM_AUDPROC_CAL_TYPE:
		size = sizeof(struct audio_cal_info_audproc);
		break;
	case ADM_AUDVOL_CAL_TYPE:
	case ADM_RTAC_AUDVOL_CAL_TYPE:
		size = sizeof(struct audio_cal_info_audvol);
		break;
	case ASM_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_info_asm_top);
		break;
	case ASM_CUST_TOPOLOGY_CAL_TYPE:
		size = 0;
		break;
	case ASM_AUDSTRM_CAL_TYPE:
		size = sizeof(struct audio_cal_info_audstrm);
		break;
	case AFE_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_info_afe_top);
		break;
	case AFE_CUST_TOPOLOGY_CAL_TYPE:
		size = 0;
		break;
	case AFE_COMMON_RX_CAL_TYPE:
		size = sizeof(struct audio_cal_info_afe);
		break;
	case AFE_COMMON_TX_CAL_TYPE:
		size = sizeof(struct audio_cal_info_afe);
		break;
	case AFE_FB_SPKR_PROT_CAL_TYPE:
		size = sizeof(struct audio_cal_info_spk_prot_cfg);
		break;
	case AFE_FB_SPKR_PROT_TH_VI_CAL_TYPE:
		/*
		 * Since get and set parameter structures are different in size
		 * use the maximum size of get and set parameter structure
		 */
		size = max(sizeof(struct audio_cal_info_sp_th_vi_ftm_cfg),
			   sizeof(struct audio_cal_info_sp_th_vi_param));
		break;
	case AFE_FB_SPKR_PROT_EX_VI_CAL_TYPE:
		/*
		 * Since get and set parameter structures are different in size
		 * use the maximum size of get and set parameter structure
		 */
		size = max(sizeof(struct audio_cal_info_sp_ex_vi_ftm_cfg),
			   sizeof(struct audio_cal_info_sp_ex_vi_param));
		break;
	case AFE_ANC_CAL_TYPE:
		size = 0;
		break;
	case AFE_AANC_CAL_TYPE:
		size = sizeof(struct audio_cal_info_aanc);
		break;
	case AFE_HW_DELAY_CAL_TYPE:
		size = sizeof(struct audio_cal_info_hw_delay);
		break;
	case AFE_SIDETONE_CAL_TYPE:
		size = sizeof(struct audio_cal_info_sidetone);
		break;
	case LSM_CUST_TOPOLOGY_CAL_TYPE:
		size = 0;
		break;
	case LSM_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_info_lsm_top);
		break;
	case ULP_LSM_TOPOLOGY_ID_CAL_TYPE:
		size = sizeof(struct audio_cal_info_lsm_top);
		break;
	case LSM_CAL_TYPE:
		size = sizeof(struct audio_cal_info_lsm);
		break;
	case ADM_RTAC_INFO_CAL_TYPE:
		size = 0;
		break;
	case VOICE_RTAC_INFO_CAL_TYPE:
		size = 0;
		break;
	case ADM_RTAC_APR_CAL_TYPE:
		size = 0;
		break;
	case ASM_RTAC_APR_CAL_TYPE:
		size = 0;
		break;
	case VOICE_RTAC_APR_CAL_TYPE:
		size = 0;
		break;
	case MAD_CAL_TYPE:
		size = 0;
		break;
	case ULP_AFE_CAL_TYPE:
		size = sizeof(struct audio_cal_info_afe);
		break;
	case ULP_LSM_CAL_TYPE:
		size = sizeof(struct audio_cal_info_lsm);
		break;
	case AUDIO_CORE_METAINFO_CAL_TYPE:
		size = sizeof(struct audio_cal_info_metainfo);
		break;
	case SRS_TRUMEDIA_CAL_TYPE:
		size = 0;
		break;
	default:
		pr_err("%s:Invalid cal type %d!",
			__func__, cal_type);
	}
	return size;
}

size_t get_user_cal_type_size(int32_t cal_type)
{
	size_t size = 0;

	switch (cal_type) {
	case CVP_VOC_RX_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_type_voc_top);
		break;
	case CVP_VOC_TX_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_type_voc_top);
		break;
	case CVP_VOCPROC_STATIC_CAL_TYPE:
		size = sizeof(struct audio_cal_type_vocproc);
		break;
	case CVP_VOCPROC_DYNAMIC_CAL_TYPE:
		size = sizeof(struct audio_cal_type_vocvol);
		break;
	case CVS_VOCSTRM_STATIC_CAL_TYPE:
		size = sizeof(struct audio_cal_type_basic);
		break;
	case CVP_VOCDEV_CFG_CAL_TYPE:
		size = sizeof(struct audio_cal_type_vocdev_cfg);
		break;
	case CVP_VOCPROC_STATIC_COL_CAL_TYPE:
	case CVP_VOCPROC_DYNAMIC_COL_CAL_TYPE:
	case CVS_VOCSTRM_STATIC_COL_CAL_TYPE:
		size = sizeof(struct audio_cal_type_voc_col);
		break;
	case ADM_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_type_adm_top);
		break;
	case ADM_CUST_TOPOLOGY_CAL_TYPE:
	case CORE_CUSTOM_TOPOLOGIES_CAL_TYPE:
		size = sizeof(struct audio_cal_type_basic);
		break;
	case ADM_AUDPROC_CAL_TYPE:
		size = sizeof(struct audio_cal_type_audproc);
		break;
	case ADM_AUDVOL_CAL_TYPE:
	case ADM_RTAC_AUDVOL_CAL_TYPE:
		size = sizeof(struct audio_cal_type_audvol);
		break;
	case ASM_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_type_asm_top);
		break;
	case ASM_CUST_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_type_basic);
		break;
	case ASM_AUDSTRM_CAL_TYPE:
		size = sizeof(struct audio_cal_type_audstrm);
		break;
	case AFE_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_type_afe_top);
		break;
	case AFE_CUST_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_type_basic);
		break;
	case AFE_COMMON_RX_CAL_TYPE:
		size = sizeof(struct audio_cal_type_afe);
		break;
	case AFE_COMMON_TX_CAL_TYPE:
		size = sizeof(struct audio_cal_type_afe);
		break;
	case AFE_FB_SPKR_PROT_CAL_TYPE:
		size = sizeof(struct audio_cal_type_fb_spk_prot_cfg);
		break;
	case AFE_FB_SPKR_PROT_TH_VI_CAL_TYPE:
		/*
		 * Since get and set parameter structures are different in size
		 * use the maximum size of get and set parameter structure
		 */
		size = max(sizeof(struct audio_cal_type_sp_th_vi_ftm_cfg),
			   sizeof(struct audio_cal_type_sp_th_vi_param));
		break;
	case AFE_FB_SPKR_PROT_EX_VI_CAL_TYPE:
		/*
		 * Since get and set parameter structures are different in size
		 * use the maximum size of get and set parameter structure
		 */
		size = max(sizeof(struct audio_cal_type_sp_ex_vi_ftm_cfg),
			   sizeof(struct audio_cal_type_sp_ex_vi_param));
		break;
	case AFE_ANC_CAL_TYPE:
		size = 0;
		break;
	case AFE_AANC_CAL_TYPE:
		size = sizeof(struct audio_cal_type_aanc);
		break;
	case AFE_HW_DELAY_CAL_TYPE:
		size = sizeof(struct audio_cal_type_hw_delay);
		break;
	case AFE_SIDETONE_CAL_TYPE:
		size = sizeof(struct audio_cal_type_sidetone);
		break;
	case LSM_CUST_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_type_basic);
		break;
	case LSM_TOPOLOGY_CAL_TYPE:
		size = sizeof(struct audio_cal_type_lsm_top);
		break;
	case ULP_LSM_TOPOLOGY_ID_CAL_TYPE:
		size = sizeof(struct audio_cal_type_lsm_top);
		break;
	case LSM_CAL_TYPE:
		size = sizeof(struct audio_cal_type_lsm);
		break;
	case ADM_RTAC_INFO_CAL_TYPE:
		size = 0;
		break;
	case VOICE_RTAC_INFO_CAL_TYPE:
		size = 0;
		break;
	case ADM_RTAC_APR_CAL_TYPE:
		size = 0;
		break;
	case ASM_RTAC_APR_CAL_TYPE:
		size = 0;
		break;
	case VOICE_RTAC_APR_CAL_TYPE:
		size = 0;
		break;
	case MAD_CAL_TYPE:
		size = 0;
		break;
	case ULP_AFE_CAL_TYPE:
		size = sizeof(struct audio_cal_type_afe);
		break;
	case ULP_LSM_CAL_TYPE:
		size = sizeof(struct audio_cal_type_lsm);
		break;
	case AUDIO_CORE_METAINFO_CAL_TYPE:
		size = sizeof(struct audio_cal_type_metainfo);
		break;
	case SRS_TRUMEDIA_CAL_TYPE:
		size = 0;
		break;
	default:
		pr_err("%s:Invalid cal type %d!",
			__func__, cal_type);
	}
	return size;
}

int32_t cal_utils_get_cal_type_version(void *cal_type_data)
{
	struct audio_cal_type_basic *data = NULL;

	data = (struct audio_cal_type_basic *)cal_type_data;

	return data->cal_hdr.version;
}

static struct cal_type_data *create_cal_type_data(
				struct cal_type_info *info)
{
	struct cal_type_data	*cal_type = NULL;

	if ((info->reg.cal_type < 0) ||
		(info->reg.cal_type >= MAX_CAL_TYPES)) {
		pr_err("%s: cal type %d is Invalid!\n",
			__func__, info->reg.cal_type);
		goto done;
	}

	if (info->cal_util_callbacks.match_block == NULL) {
		pr_err("%s: cal type %d no method to match blocks!\n",
			__func__, info->reg.cal_type);
		goto done;
	}

	cal_type = kmalloc(sizeof(*cal_type), GFP_KERNEL);
	if (cal_type == NULL) {
		pr_err("%s: could not allocate cal_type!\n", __func__);
		goto done;
	}
	INIT_LIST_HEAD(&cal_type->cal_blocks);
	mutex_init(&cal_type->lock);
	memcpy(&cal_type->info, info,
		sizeof(cal_type->info));
done:
	return cal_type;
}

int cal_utils_create_cal_types(int num_cal_types,
			struct cal_type_data **cal_type,
			struct cal_type_info *info)
{
	int				ret = 0;
	int				i;
	pr_debug("%s\n", __func__);

	if (cal_type == NULL) {
		pr_err("%s: cal_type is NULL!\n", __func__);
		ret = -EINVAL;
		goto done;
	} else if (info == NULL) {
		pr_err("%s: info is NULL!\n", __func__);
		ret = -EINVAL;
		goto done;
	} else if ((num_cal_types <= 0) ||
		(num_cal_types > MAX_CAL_TYPES)) {
		pr_err("%s: num_cal_types of %d is Invalid!\n",
			__func__, num_cal_types);
		ret = -EINVAL;
		goto done;
	}

	for (i = 0; i < num_cal_types; i++) {
		if ((info[i].reg.cal_type < 0) ||
			(info[i].reg.cal_type >= MAX_CAL_TYPES)) {
			pr_err("%s: cal type %d at index %d is Invalid!\n",
				__func__, info[i].reg.cal_type, i);
			ret = -EINVAL;
			goto done;
		}

		cal_type[i] = create_cal_type_data(&info[i]);
		if (cal_type[i] == NULL) {
			pr_err("%s: Could not allocate cal_type of index %d!\n",
				__func__, i);
			ret = -EINVAL;
			goto done;
		}

		ret = audio_cal_register(1, &info[i].reg);
		if (ret < 0) {
			pr_err("%s: audio_cal_register failed, ret = %d!\n",
				__func__, ret);
			ret = -EINVAL;
			goto done;
		}
		pr_debug("%s: cal type %d at index %d!\n",
			__func__, info[i].reg.cal_type, i);
	}
done:
	return ret;
}

static void delete_cal_block(struct cal_block_data *cal_block)
{
	pr_debug("%s\n", __func__);

	if (cal_block == NULL)
		goto done;

	list_del(&cal_block->list);
	kfree(cal_block->client_info);
	cal_block->client_info = NULL;
	kfree(cal_block->cal_info);
	cal_block->cal_info = NULL;
	if (cal_block->map_data.ion_client  != NULL) {
		msm_audio_ion_free(cal_block->map_data.ion_client,
			cal_block->map_data.ion_handle);
		cal_block->map_data.ion_client = NULL;
		cal_block->map_data.ion_handle = NULL;
	}
	kfree(cal_block);
done:
	return;
}

static void destroy_all_cal_blocks(struct cal_type_data *cal_type)
{
	int				ret = 0;
	struct list_head		*ptr, *next;
	struct cal_block_data		*cal_block;

	list_for_each_safe(ptr, next,
		&cal_type->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);

		ret = unmap_memory(cal_type, cal_block);
		if (ret < 0) {
			pr_err("%s: unmap_memory failed, cal type %d, ret = %d!\n",
				__func__,
			       cal_type->info.reg.cal_type,
				ret);
		}
		delete_cal_block(cal_block);
		cal_block = NULL;
	}

	return;
}

static void destroy_cal_type_data(struct cal_type_data *cal_type)
{
	if (cal_type == NULL)
		goto done;

	destroy_all_cal_blocks(cal_type);
	list_del(&cal_type->cal_blocks);
	kfree(cal_type);
done:
	return;
}

void cal_utils_destroy_cal_types(int num_cal_types,
			struct cal_type_data **cal_type)
{
	int				i;
	pr_debug("%s\n", __func__);

	if (cal_type == NULL) {
		pr_err("%s: cal_type is NULL!\n", __func__);
		goto done;
	} else if ((num_cal_types <= 0) ||
		(num_cal_types > MAX_CAL_TYPES)) {
		pr_err("%s: num_cal_types of %d is Invalid!\n",
			__func__, num_cal_types);
		goto done;
	}

	for (i = 0; i < num_cal_types; i++) {
		audio_cal_deregister(1, &cal_type[i]->info.reg);
		destroy_cal_type_data(cal_type[i]);
		cal_type[i] = NULL;
	}
done:
	return;
}

struct cal_block_data *cal_utils_get_only_cal_block(
			struct cal_type_data *cal_type)
{
	struct list_head		*ptr, *next;
	struct cal_block_data		*cal_block = NULL;

	if (cal_type == NULL)
		goto done;

	list_for_each_safe(ptr, next,
		&cal_type->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);
		break;
	}
done:
	return cal_block;
}

bool cal_utils_match_buf_num(struct cal_block_data *cal_block,
					void *user_data)
{
	bool ret = false;
	struct audio_cal_type_basic	*data = user_data;

	if (cal_block->buffer_number == data->cal_hdr.buffer_number)
		ret = true;

	return ret;
}

static struct cal_block_data *get_matching_cal_block(
					struct cal_type_data *cal_type,
					void *data)
{
	struct list_head		*ptr, *next;
	struct cal_block_data		*cal_block = NULL;

	list_for_each_safe(ptr, next,
		&cal_type->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);

		if (cal_type->info.cal_util_callbacks.
			match_block(cal_block, data))
			return cal_block;
	}

	return NULL;
}

static int cal_block_ion_alloc(struct cal_block_data *cal_block)
{
	int	ret = 0;

	if (cal_block == NULL) {
		pr_err("%s: cal_block is NULL!\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	ret = msm_audio_ion_import("audio_cal_client",
		&cal_block->map_data.ion_client,
		&cal_block->map_data.ion_handle,
		cal_block->map_data.ion_map_handle,
		NULL, 0,
		&cal_block->cal_data.paddr,
		&cal_block->map_data.map_size,
		&cal_block->cal_data.kvaddr);
	if (ret) {
		pr_err("%s: audio ION import failed, rc = %d\n",
			__func__, ret);
		ret = -ENOMEM;
		goto done;
	}
done:
	return ret;
}

static struct cal_block_data *create_cal_block(struct cal_type_data *cal_type,
				struct audio_cal_type_basic *basic_cal,
				size_t client_info_size, void *client_info)
{
	struct cal_block_data	*cal_block = NULL;

	if (cal_type == NULL) {
		pr_err("%s: cal_type is NULL!\n", __func__);
		goto done;
	} else if (basic_cal == NULL) {
		pr_err("%s: basic_cal is NULL!\n", __func__);
		goto done;
	}

	cal_block = kzalloc(sizeof(*cal_block),
		GFP_KERNEL);
	if (cal_block == NULL) {
		pr_err("%s: could not allocate cal_block!\n", __func__);
		goto done;
	}

	INIT_LIST_HEAD(&cal_block->list);

	cal_block->map_data.ion_map_handle = basic_cal->cal_data.mem_handle;
	if (basic_cal->cal_data.mem_handle > 0) {
		if (cal_block_ion_alloc(cal_block)) {
			pr_err("%s: cal_block_ion_alloc failed!\n",
				__func__);
			goto err;
		}
	}
	if (client_info_size > 0) {
		cal_block->client_info_size = client_info_size;
		cal_block->client_info = kmalloc(client_info_size, GFP_KERNEL);
		if (cal_block->client_info == NULL) {
			pr_err("%s: could not allocats client_info!\n",
				__func__);
			goto err;
		}
		if (client_info != NULL)
			memcpy(cal_block->client_info, client_info,
				client_info_size);
	}

	cal_block->cal_info = kzalloc(
		get_cal_info_size(cal_type->info.reg.cal_type),
		GFP_KERNEL);
	if (cal_block->cal_info == NULL) {
		pr_err("%s: could not allocats cal_info!\n",
			__func__);
		goto err;
	}
	cal_block->buffer_number = basic_cal->cal_hdr.buffer_number;
	list_add_tail(&cal_block->list, &cal_type->cal_blocks);
	pr_debug("%s: created block for cal type %d, buf num %d, map handle %d, map size %zd paddr 0x%pK!\n",
		__func__, cal_type->info.reg.cal_type,
		cal_block->buffer_number,
		cal_block->map_data.ion_map_handle,
		cal_block->map_data.map_size,
		&cal_block->cal_data.paddr);
done:
	return cal_block;
err:
	kfree(cal_block->cal_info);
	kfree(cal_block->client_info);
	kfree(cal_block);
	cal_block = NULL;
	return cal_block;
}

void cal_utils_clear_cal_block_q6maps(int num_cal_types,
					struct cal_type_data **cal_type)
{
	int				i = 0;
	struct list_head		*ptr, *next;
	struct cal_block_data		*cal_block;
	pr_debug("%s\n", __func__);

	if (cal_type == NULL) {
		pr_err("%s: cal_type is NULL!\n", __func__);
		goto done;
	} else if ((num_cal_types <= 0) ||
		(num_cal_types > MAX_CAL_TYPES)) {
		pr_err("%s: num_cal_types of %d is Invalid!\n",
			__func__, num_cal_types);
		goto done;
	}

	for (; i < num_cal_types; i++) {
		if (cal_type[i] == NULL)
			continue;

		mutex_lock(&cal_type[i]->lock);
		list_for_each_safe(ptr, next,
			&cal_type[i]->cal_blocks) {

			cal_block = list_entry(ptr,
				struct cal_block_data, list);

			cal_block->map_data.q6map_handle = 0;
		}
		mutex_unlock(&cal_type[i]->lock);
	}
done:
	return;
}



static int realloc_memory(struct cal_block_data *cal_block)
{
	int ret = 0;

	msm_audio_ion_free(cal_block->map_data.ion_client,
		cal_block->map_data.ion_handle);
	cal_block->map_data.ion_client = NULL;
	cal_block->map_data.ion_handle = NULL;
	cal_block->cal_data.size = 0;

	ret = cal_block_ion_alloc(cal_block);
	if (ret < 0)
		pr_err("%s: realloc_memory failed!\n",
			__func__);
	return ret;
}

static int map_memory(struct cal_type_data *cal_type,
			struct cal_block_data *cal_block)
{
	int ret = 0;


	if (cal_type->info.cal_util_callbacks.map_cal != NULL) {
		if ((cal_block->map_data.ion_map_handle < 0) ||
			(cal_block->map_data.map_size <= 0) ||
			(cal_block->map_data.q6map_handle != 0)) {
			goto done;
		}

		pr_debug("%s: cal type %d call map\n",
			__func__, cal_type->info.reg.cal_type);
		ret = cal_type->info.cal_util_callbacks.
			map_cal(cal_type->info.reg.cal_type, cal_block);
		if (ret < 0) {
			pr_err("%s: map_cal failed, cal type %d, ret = %d!\n",
				__func__, cal_type->info.reg.cal_type,
				ret);
			goto done;
		}
	}
done:
	return ret;
}

static int unmap_memory(struct cal_type_data *cal_type,
			struct cal_block_data *cal_block)
{
	int ret = 0;

	if (cal_type->info.cal_util_callbacks.unmap_cal != NULL) {
		if ((cal_block->map_data.ion_map_handle < 0) ||
			(cal_block->map_data.map_size <= 0) ||
			(cal_block->map_data.q6map_handle == 0)) {
			goto done;
		}
		pr_debug("%s: cal type %d call unmap\n",
			__func__, cal_type->info.reg.cal_type);
		ret = cal_type->info.cal_util_callbacks.
			unmap_cal(cal_type->info.reg.cal_type, cal_block);
		if (ret < 0) {
			pr_err("%s: unmap_cal failed, cal type %d, ret = %d!\n",
				__func__, cal_type->info.reg.cal_type,
				ret);
			goto done;
		}
	}
done:
	return ret;
}

int cal_utils_alloc_cal(size_t data_size, void *data,
			struct cal_type_data *cal_type,
			size_t client_info_size, void *client_info)
{
	int				ret = 0;
	struct cal_block_data		*cal_block;
	struct audio_cal_type_alloc	*alloc_data = data;
	pr_debug("%s\n", __func__);

	if (cal_type == NULL) {
		pr_err("%s: cal_type is NULL!\n",
			__func__);
		ret = -EINVAL;
		goto done;
	}
	if (data_size < sizeof(struct audio_cal_type_alloc)) {
		pr_err("%s: data_size of %zd does not equal alloc struct size of %zd!\n",
			__func__, data_size,
		       sizeof(struct audio_cal_type_alloc));
		ret = -EINVAL;
		goto done;
	}
	if ((client_info_size > 0) && (client_info == NULL)) {
		pr_err("%s: User info pointer is NULL but size is %zd!\n",
			__func__, client_info_size);
		ret = -EINVAL;
		goto done;
	}

	if (alloc_data->cal_data.mem_handle < 0) {
		pr_err("%s: mem_handle %d invalid!\n",
			__func__, alloc_data->cal_data.mem_handle);
		ret = -EINVAL;
		goto done;
	}

	mutex_lock(&cal_type->lock);

	cal_block = get_matching_cal_block(cal_type,
		data);
	if (cal_block != NULL) {
		ret = unmap_memory(cal_type, cal_block);
		if (ret < 0)
			goto err;
		ret = realloc_memory(cal_block);
		if (ret < 0)
			goto err;
	} else {
		cal_block = create_cal_block(cal_type,
			(struct audio_cal_type_basic *)alloc_data,
			client_info_size, client_info);
		if (cal_block == NULL) {
			pr_err("%s: create_cal_block failed for %d!\n",
				__func__, alloc_data->cal_data.mem_handle);
			ret = -EINVAL;
			goto err;
		}
	}

	ret = map_memory(cal_type, cal_block);
	if (ret < 0)
		goto err;
err:
	mutex_unlock(&cal_type->lock);
done:
	return ret;
}

int cal_utils_dealloc_cal(size_t data_size, void *data,
			struct cal_type_data *cal_type)
{
	int				ret = 0;
	struct cal_block_data		*cal_block;
	struct audio_cal_type_dealloc	*dealloc_data = data;
	pr_debug("%s\n", __func__);


	if (cal_type == NULL) {
		pr_err("%s: cal_type is NULL!\n",
			__func__);
		ret = -EINVAL;
		goto done;
	}

	if (data_size < sizeof(struct audio_cal_type_dealloc)) {
		pr_err("%s: data_size of %zd does not equal struct size of %zd!\n",
			__func__, data_size,
			sizeof(struct audio_cal_type_dealloc));
		ret = -EINVAL;
		goto done;
	}

	if ((dealloc_data->cal_data.mem_handle == -1) &&
		(dealloc_data->cal_hdr.buffer_number == ALL_CAL_BLOCKS)) {
		destroy_all_cal_blocks(cal_type);
		goto done;
	}

	if (dealloc_data->cal_data.mem_handle < 0) {
		pr_err("%s: mem_handle %d invalid!\n",
			__func__, dealloc_data->cal_data.mem_handle);
		ret = -EINVAL;
		goto done;
	}

	mutex_lock(&cal_type->lock);
	cal_block = get_matching_cal_block(
		cal_type,
		data);
	if (cal_block == NULL) {
		pr_err("%s: allocation does not exist for %d!\n",
			__func__, dealloc_data->cal_data.mem_handle);
		ret = -EINVAL;
		goto err;
	}

	ret = unmap_memory(cal_type, cal_block);
	if (ret < 0)
		goto err;

	delete_cal_block(cal_block);
err:
	mutex_unlock(&cal_type->lock);
done:
	return ret;
}

int cal_utils_set_cal(size_t data_size, void *data,
			struct cal_type_data *cal_type,
			size_t client_info_size, void *client_info)
{
	int ret = 0;
	struct cal_block_data		*cal_block;
	struct audio_cal_type_basic	*basic_data = data;
	pr_debug("%s\n", __func__);

	if (cal_type == NULL) {
		pr_err("%s: cal_type is NULL!\n",
			__func__);
		ret = -EINVAL;
		goto done;
	}

	if ((client_info_size > 0) && (client_info == NULL)) {
		pr_err("%s: User info pointer is NULL but size is %zd!\n",
			__func__, client_info_size);
		ret = -EINVAL;
		goto done;
	}

	if ((data_size > get_user_cal_type_size(
		cal_type->info.reg.cal_type)) || (data_size < 0)) {
		pr_err("%s: cal_type %d, data_size of %zd is invalid, expecting %zd!\n",
			__func__, cal_type->info.reg.cal_type, data_size,
			get_user_cal_type_size(cal_type->info.reg.cal_type));
		ret = -EINVAL;
		goto done;
	}

	mutex_lock(&cal_type->lock);
	cal_block = get_matching_cal_block(
		cal_type,
		data);
	if (cal_block == NULL) {
		if (basic_data->cal_data.mem_handle > 0) {
			pr_err("%s: allocation does not exist for %d!\n",
				__func__, basic_data->cal_data.mem_handle);
			ret = -EINVAL;
			goto err;
		} else {
			cal_block = create_cal_block(
				cal_type,
				basic_data,
				client_info_size, client_info);
			if (cal_block == NULL) {
				pr_err("%s: create_cal_block failed for cal type %d!\n",
					__func__,
				       cal_type->info.reg.cal_type);
				ret = -EINVAL;
				goto err;
			}
		}
	}

	ret = map_memory(cal_type, cal_block);
	if (ret < 0)
		goto err;

	cal_block->cal_data.size = basic_data->cal_data.cal_size;

	if (client_info_size > 0) {
		memcpy(cal_block->client_info,
			client_info,
			client_info_size);
	}

	memcpy(cal_block->cal_info,
		((uint8_t *)data + sizeof(struct audio_cal_type_basic)),
		data_size - sizeof(struct audio_cal_type_basic));

err:
	mutex_unlock(&cal_type->lock);
done:
	return ret;
}
