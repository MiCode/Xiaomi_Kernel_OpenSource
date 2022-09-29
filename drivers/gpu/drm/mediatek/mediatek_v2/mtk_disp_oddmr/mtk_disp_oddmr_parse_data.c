// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include "mtk_disp_oddmr_parse_data.h"
#include "mtk_disp_oddmr_tuning.h"

#ifdef APP_DEBUG
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#define DDPINFO printf
#define kfree free
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
#else
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/ratelimit.h>
#include "../mtk_drm_crtc.h"
#include "../mtk_drm_ddp_comp.h"
#include "../mtk_dump.h"
#include "../mtk_drm_mmp.h"
#include "../mtk_drm_gem.h"
#include "../mtk_drm_fb.h"
#undef DDPINFO
#define DDPINFO DDPMSG
#endif
struct mtk_oddmr_dmr_param g_dmr_param;
struct mtk_oddmr_od_param g_od_param;
int is_dmr_basic_info_loaded;
int is_od_basic_info_loaded;
/* return loaded size */
static uint32_t mtk_oddmr_parse_table_pq(uint32_t table_idx,
		uint8_t *data, uint32_t len, struct mtk_oddmr_pq_param *pq_param)
{
	void *buffer_alloc;
	uint32_t counts;

	if (len > 0) {
		counts = *(uint32_t *)data;
		data += 4;
		if (len != (4 + counts * 8)) {
			DDPINFO("%s:%d, size is invalid size:%d,cnts:%d\n",
					__func__, __LINE__, len, counts);
			return 0;
		}
		if (pq_param == NULL) {
			DDPINFO("%s:%d, pq_param is NULL\n",
					__func__, __LINE__);
			return 0;
		}
		if (pq_param->param != NULL) {
			kfree(pq_param->param);
			pq_param->param = NULL;
			pq_param->counts = 0;
		}
#ifndef APP_DEBUG
		buffer_alloc = kzalloc(len - 4, GFP_KERNEL);
#else
		buffer_alloc = malloc(len - 4);
#endif
		if (!buffer_alloc) {
			DDPINFO("%s:%d, param buffer alloc fail\n",
					__func__, __LINE__);
			return 0;
		}
		pq_param->param = buffer_alloc;
		pq_param->counts = counts;
		memcpy(pq_param->param, data, len - 4);
	} else
		DDPINFO("%s:%d, len = 0 skip\n", __func__, __LINE__);
	return len;
}
static uint32_t mtk_oddmr_parse_raw_table(uint32_t table_idx,
		uint8_t *data, uint32_t len, struct mtk_oddmr_table_raw *raw_table)
{
	void *buffer_alloc;

	if (raw_table == NULL) {
		DDPINFO("%s:%d, raw_table is NULL\n",
				__func__, __LINE__);
		return 0;
	}
	if (raw_table->value != NULL) {
		kfree(raw_table->value);
		raw_table->value = NULL;
		raw_table->size = 0;
	}
#ifndef APP_DEBUG
	buffer_alloc = kzalloc(len, GFP_KERNEL);
#else
	buffer_alloc = malloc(len);
#endif
	if (!buffer_alloc) {
		DDPINFO("%s:%d, param buffer alloc fail\n",
				__func__, __LINE__);
		return 0;
	}
	raw_table->value = buffer_alloc;
	memcpy(raw_table->value, data, len);
	raw_table->size = len;
	return len;
}
/* return loaded size */
static uint32_t mtk_oddmr_parse_dmr_table_basic_info(uint32_t table_idx, uint8_t *data)
{
	struct mtk_oddmr_dmr_table_basic_info *tmp;
	uint32_t size;

	size = sizeof(struct mtk_oddmr_dmr_table_basic_info);
	tmp = &g_dmr_param.dmr_tables[table_idx]->table_basic_info;
	memcpy(tmp, data, size);
	return size;
}
static uint32_t mtk_oddmr_parse_od_table_basic_info(uint32_t table_idx, uint8_t *data)
{
	struct mtk_oddmr_od_table_basic_info *tmp;
	uint32_t size;

	size = sizeof(struct mtk_oddmr_od_table_basic_info);
	tmp = &g_od_param.od_tables[table_idx]->table_basic_info;
	memcpy(tmp, data, size);
	return size;
}

/*
 * common way to describe od weight, not used
 * return loaded size
 */
static uint32_t mtk_oddmr_parse_od_table_gain(uint32_t table_idx, uint8_t *data, uint32_t len)
{
	void *buffer_alloc;

	if (g_od_param.od_tables[table_idx] != NULL &&
		g_od_param.od_tables[table_idx]->gain_table_raw == NULL) {
#ifndef APP_DEBUG
		buffer_alloc = kzalloc(len, GFP_KERNEL);
#else
		buffer_alloc = malloc(len);
#endif
		if (!buffer_alloc) {
			DDPINFO("%s:%d, param buffer alloc fail\n", __func__, __LINE__);
			return 0;
		}
		g_od_param.od_tables[table_idx]->gain_table_raw = buffer_alloc;
		memcpy(g_od_param.od_tables[table_idx]->gain_table_raw, data, len);
	} else {
		len = 0;
	}
	return len;
}

/*
 * another way to describe od fps/dbv weight
 * return loaded size
 */
static uint32_t mtk_oddmr_parse_od_table_bl_gain(uint32_t table_idx, uint8_t *data, uint32_t len)
{
	void *tmp;

	g_od_param.od_tables[table_idx]->bl_cnt = *(uint32_t *)data;
	data += 4;
	tmp = g_od_param.od_tables[table_idx]->bl_table;
	if (len > 4)
		memcpy(tmp, data, len - 4);
	return len;
}

/*
 * another way to describe od fps/dbv weight
 * return loaded size
 */
static uint32_t mtk_oddmr_parse_od_table_fps_gain(uint32_t table_idx, uint8_t *data, uint32_t len)
{
	void *tmp;

	g_od_param.od_tables[table_idx]->fps_cnt = *(uint32_t *)data;
	data += 4;
	tmp = g_od_param.od_tables[table_idx]->fps_table;
	if (len > 4)
		memcpy(tmp, data, len - 4);
	return len;
}

/* return loaded size */
static uint32_t mtk_oddmr_parse_dmr_table_fps_gain(uint32_t table_idx, uint8_t *data, uint32_t len)
{
	void *tmp;

	g_dmr_param.dmr_tables[table_idx]->fps_cnt = *(uint32_t *)data;
	data += 4;
	tmp = g_dmr_param.dmr_tables[table_idx]->fps_table;
	if (len > 4)
		memcpy(tmp, data, len - 4);
	return len;
}
/* return loaded size */
static uint32_t mtk_oddmr_parse_dmr_table_bl_gain(uint32_t table_idx, uint8_t *data, uint32_t len)
{
	void *tmp;

	g_dmr_param.dmr_tables[table_idx]->bl_cnt = *(uint32_t *)data;
	data += 4;
	tmp = g_dmr_param.dmr_tables[table_idx]->bl_table;

	if (len > 4)
		memcpy(tmp, data, len - 4);
	return len;
}

static int _mtk_oddmr_load_param(struct mtk_drm_oddmr_param *param)
{
	int ret = -EFAULT;
	uint32_t table_idx = 0;
	uint8_t *data, *p;
	uint32_t counting_size, sub_head_id, data_type_id, tmp_head_id;

	if (param == NULL || param->data == NULL) {
		DDPINFO("%s:%d, param is NULL\n", __func__, __LINE__);
		return -EFAULT;
	}
#ifndef APP_DEBUG
	data = kzalloc(param->size, GFP_KERNEL);
#else
	data = malloc(param->size);
	memset(data, 0, param->size);
#endif
	if (!data) {
		DDPINFO("%s:%d, param buffer alloc fail\n", __func__, __LINE__);
		return -EFAULT;
	}
#ifndef APP_DEBUG
	if (copy_from_user(data, param->data, param->size)) {
		DDPINFO("%s:%d, copy_from_user fail\n", __func__, __LINE__);
		ret = -EFAULT;
		goto fail;
	}
#else
	memcpy(data, param->data, param->size);
#endif
	table_idx = param->head_id >> 16 & 0xFF;
	data_type_id = param->head_id >> 24;
	DDPINFO("%s:%d, table_idx 0x%x data_type_id 0x%x size %d\n", __func__, __LINE__,
			table_idx, data_type_id, param->size);
	if ((param->head_id & 0xFFFF) == ODDMR_SECTION_WHOLE) {
		p = data;
		counting_size = 0;
		sub_head_id = *(uint32_t *)p;

		/* sub section head + size + data_body */
		DDPINFO("%s:%d, ++++++++ table%d 0x%x parsing begin,%d/%d\n", __func__, __LINE__,
				table_idx, param->head_id, counting_size, param->size);
		while (counting_size < param->size && sub_head_id != ODDMR_SECTION_END) {
			uint32_t tmp_size = 0;
			/* p is now pointing to sub head */
			tmp_head_id = *(uint32_t *)p;
			sub_head_id = tmp_head_id & 0xFFFF;
			p += 4;
			counting_size += 4;
			/* p is now pointing to sub size */
			tmp_size = *(uint32_t *)p;
			p += 4;
			counting_size += 4;
			DDPINFO("%s:%d, parsing 0x%x size %d\n", __func__, __LINE__,
					tmp_head_id, tmp_size);
			/* p is now pointing to sub data_body */
			if (tmp_size == 0) {
				DDPINFO("%s:%d, tmp_head_id 0x%x skip due to size = 0\n",
						__func__, __LINE__, tmp_head_id);
				goto skip_loop;
			}
			if (sub_head_id == DMR_BASIC_PARAM &&
					data_type_id == ODDMR_DMR_BASIC_INFO) {
				/* p is now pointing to sub data_body */
				if (tmp_size != sizeof(struct mtk_oddmr_dmr_basic_param)) {
					DDPINFO("%s:%d, 0x%x size error\n",
							__func__, __LINE__, sub_head_id);
					ret = -EFAULT;
					goto fail;
				}
				memcpy(&g_dmr_param.dmr_basic_info.basic_param, p, tmp_size);
			} else if (sub_head_id == DMR_BASIC_PQ &&
					data_type_id == ODDMR_DMR_BASIC_INFO) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_table_pq(table_idx, p, tmp_size,
						&g_dmr_param.dmr_basic_info.basic_pq);
				if (tmp_size == 0) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == OD_BASIC_PARAM &&
					data_type_id == ODDMR_OD_BASIC_INFO) {
				/* p is now pointing to sub data_body */
				if (tmp_size != sizeof(struct mtk_oddmr_od_basic_param)) {
					DDPINFO("%s:%d, 0x%x size error\n",
							__func__, __LINE__, sub_head_id);
					ret = -EFAULT;
					goto fail;
				}
				memcpy(&g_od_param.od_basic_info.basic_param, p, tmp_size);
			} else if (sub_head_id == OD_BASIC_PQ &&
					data_type_id == ODDMR_OD_BASIC_INFO) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_table_pq(table_idx, p, tmp_size,
						&g_od_param.od_basic_info.basic_pq);
				if (tmp_size == 0) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == DMR_TABLE_BASIC_INFO &&
					data_type_id == ODDMR_DMR_TABLE) {
				/* p is now pointing to sub size */
				if (tmp_size != sizeof(struct mtk_oddmr_dmr_table_basic_info)) {
					DDPINFO("%s:%d, 0x%x size error\n",
							__func__, __LINE__, tmp_head_id);
					ret = -EFAULT;
					goto fail;
				}
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_dmr_table_basic_info(table_idx, p);
			} else if (sub_head_id == DMR_TABLE_FPS_GAIN_TABLE &&
					data_type_id == ODDMR_DMR_TABLE) {
				/* p is now pointing to sub data_body */
				uint32_t counts = *(uint32_t *)p;

				if (tmp_size < 4
					|| counts > DMR_GAIN_MAX
					|| (tmp_size != counts * 16 + 4)) {
					DDPINFO("%s:%d, table%d 0x%x size error,size %d,count %d\n",
							__func__, __LINE__,
							table_idx, tmp_head_id, tmp_size, counts);
					ret = -EFAULT;
					goto fail;
				}
				if (mtk_oddmr_parse_dmr_table_fps_gain(table_idx, p, tmp_size) !=
					tmp_size) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == DMR_TABLE_BL_GAIN_TABLE &&
					data_type_id == ODDMR_DMR_TABLE) {
				/* p is now pointing to sub data_body */
				uint32_t counts = *(uint32_t *)p;

				if (tmp_size < 4
					|| counts > DMR_GAIN_MAX
					|| (tmp_size != counts * 8 + 4)) {
					DDPINFO("%s:%d, table%d 0x%x size error,size %d,count %d\n",
							__func__, __LINE__,
							table_idx, tmp_head_id, tmp_size, counts);
					ret = -EFAULT;
					goto fail;
				}
				if (mtk_oddmr_parse_dmr_table_bl_gain(table_idx, p, tmp_size) !=
					tmp_size) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == DMR_TABLE_PQ_COMMON &&
					data_type_id == ODDMR_DMR_TABLE) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_table_pq(table_idx, p, tmp_size,
						&g_dmr_param.dmr_tables[table_idx]->pq_common);
				if (tmp_size == 0) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == DMR_TABLE_PQ_SINGLE &&
					data_type_id == ODDMR_DMR_TABLE) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_table_pq(table_idx, p, tmp_size,
						&g_dmr_param.dmr_tables[table_idx]->pq_single_pipe);
				if (tmp_size == 0) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == DMR_TABLE_DATA_SINGLE &&
					data_type_id == ODDMR_DMR_TABLE) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_raw_table(table_idx, p, tmp_size,
					&g_dmr_param.dmr_tables[table_idx]->raw_table_single);
				if (tmp_size == 0) {
					DDPINFO("%s:%d, table%d 0x%x size error,size %d\n",
							__func__, __LINE__,
							table_idx, tmp_head_id, tmp_size);
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == DMR_TABLE_PQ_LEFT &&
					data_type_id == ODDMR_DMR_TABLE) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_table_pq(table_idx, p, tmp_size,
						&g_dmr_param.dmr_tables[table_idx]->pq_left_pipe);
				if (tmp_size == 0) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == DMR_TABLE_DATA_LEFT &&
					data_type_id == ODDMR_DMR_TABLE) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_raw_table(table_idx, p, tmp_size,
						&g_dmr_param.dmr_tables[table_idx]->raw_table_left);
				if (tmp_size == 0) {
					DDPINFO("%s:%d, table%d 0x%x size error,size %d\n",
							__func__, __LINE__,
							table_idx, tmp_head_id, tmp_size);
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == DMR_TABLE_PQ_RIGHT &&
					data_type_id == ODDMR_DMR_TABLE) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_table_pq(table_idx, p, tmp_size,
						&g_dmr_param.dmr_tables[table_idx]->pq_right_pipe);
				if (tmp_size == 0) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == DMR_TABLE_DATA_RIGHT &&
					data_type_id == ODDMR_DMR_TABLE) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_raw_table(table_idx, p, tmp_size,
					&g_dmr_param.dmr_tables[table_idx]->raw_table_right);
				if (tmp_size == 0) {
					DDPINFO("%s:%d, table%d 0x%x size error,size %d\n",
							__func__, __LINE__,
							table_idx, tmp_head_id, tmp_size);
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == OD_TABLE_BASIC_INFO &&
					data_type_id == ODDMR_OD_TABLE) {
				/* p is now pointing to sub data_body */
				if (tmp_size != sizeof(struct mtk_oddmr_od_table_basic_info)) {
					DDPINFO("%s:%d, 0x%x size error\n",
							__func__, __LINE__, sub_head_id);
					ret = -EFAULT;
					goto fail;
				}
				tmp_size = mtk_oddmr_parse_od_table_basic_info(table_idx, p);
			} else if (sub_head_id == OD_TABLE_GAIN_TABLE &&
					data_type_id == ODDMR_OD_TABLE) {
				/* p is now pointing to sub data_body */
				uint32_t counts_fps = *(uint32_t *)p;
				uint32_t counts_bl = *(uint32_t *)(p + 4);

				if (tmp_size != 8 + counts_fps * 4 + counts_bl * 4 +
						counts_fps * counts_bl) {
					DDPINFO("%s:%d, table%d 0x%x size error,",
						__func__, __LINE__, table_idx, sub_head_id);
					DDPINFO("size %d,fps_cnt %d dbv_cnt %d\n",
						tmp_size, counts_fps, counts_bl);
					ret = -EFAULT;
					goto fail;
				}
				if (mtk_oddmr_parse_od_table_gain(table_idx, p, tmp_size) !=
					tmp_size) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == OD_TABLE_PQ_OD &&
					data_type_id == ODDMR_OD_TABLE) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_table_pq(table_idx, p, tmp_size,
						&g_od_param.od_tables[table_idx]->pq_od);
				if (tmp_size == 0) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == OD_TABLE_FPS_GAIN_TABLE &&
					data_type_id == ODDMR_OD_TABLE) {
				/* p is now pointing to sub data_body */
				uint32_t counts = *(uint32_t *)p;

				if (tmp_size < 4
					|| counts > OD_GAIN_MAX
					|| (tmp_size != counts * 8 + 4)) {
					DDPINFO("%s:%d, table%d 0x%x size error,size %d,count %d\n",
							__func__, __LINE__,
							table_idx, tmp_head_id, tmp_size, counts);
					ret = -EFAULT;
					goto fail;
				}
				if (mtk_oddmr_parse_od_table_fps_gain(table_idx, p, tmp_size) !=
					tmp_size) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == OD_TABLE_DBV_GAIN_TABLE &&
					data_type_id == ODDMR_OD_TABLE) {
				/* p is now pointing to sub data_body */
				uint32_t counts = *(uint32_t *)p;

				if (tmp_size < 4 ||
					counts > OD_GAIN_MAX
					|| (tmp_size != counts * 8 + 4)) {
					DDPINFO("%s:%d, table%d 0x%x size error,size %d,count %d\n",
							__func__, __LINE__,
							table_idx, tmp_head_id, tmp_size, counts);
					ret = -EFAULT;
					goto fail;
				}
				if (mtk_oddmr_parse_od_table_bl_gain(table_idx, p, tmp_size) !=
					tmp_size) {
					ret = -EFAULT;
					goto fail;
				}
			} else if (sub_head_id == OD_TABLE_DATA && data_type_id == ODDMR_OD_TABLE) {
				/* p is now pointing to sub data_body */
				tmp_size = mtk_oddmr_parse_raw_table(table_idx, p, tmp_size,
						&g_od_param.od_tables[table_idx]->raw_table);
				if (tmp_size == 0) {
					DDPINFO("%s:%d, table%d 0x%x size error,size %d\n",
							__func__, __LINE__,
							table_idx, tmp_head_id, tmp_size);
					ret = -EFAULT;
					goto fail;
				}
			} else {
				ret = -EFAULT;
				DDPINFO("%s:%d, 0x%x, not support counting %d/%d\n",
						__func__, __LINE__,
						tmp_head_id, counting_size, param->size);
				break;
			}
			p += tmp_size;
			counting_size += tmp_size;
			/* p is now pointing to next sub head */
skip_loop:
			DDPINFO("%s:%d, table%d 0x%x size %d, counting %d/%d\n",
					__func__, __LINE__, table_idx, tmp_head_id,
					tmp_size, counting_size, param->size);
			sub_head_id = *(uint32_t *)p & 0xFFFF;
			if (sub_head_id == ODDMR_SECTION_END || counting_size == param->size) {
				ret = 0;
				DDPINFO("%s:%d, -------- table%d 0x%x parsing end,(%d + 4)/%d\n",
						__func__, __LINE__, table_idx, param->head_id,
						counting_size, param->size);
				break;
			}
		}
	} else {
		DDPINFO("%s:%d, table%d 0x%x not support\n",
				__func__, __LINE__, table_idx, param->head_id);
	}
fail:
	kfree(data);
	data = NULL;
	return ret;
}
int mtk_oddmr_load_param(struct mtk_disp_oddmr *priv, struct mtk_drm_oddmr_param *param)
{
	int ret = -1;
	uint32_t table_idx, size_alloc;
	void *data;

	if (param == NULL) {
		DDPINFO("%s:%d, param is NULL\n",
				__func__, __LINE__);
		return -EFAULT;
	}
	switch (param->head_id >> 24) {
	case ODDMR_DMR_BASIC_INFO:
		if (is_dmr_basic_info_loaded) {
			DDPINFO("%s:%d, basic info is already loaded\n",
					__func__, __LINE__);
			return -EFAULT;
		}
		ret = _mtk_oddmr_load_param(param);
		if (ret >= 0) {
			is_dmr_basic_info_loaded = 1;
			DDPINFO("%s:%d, dmr basic info load success\n", __func__, __LINE__);
		}
		break;
	case ODDMR_DMR_TABLE:
		table_idx = param->head_id >> 16 & 0xFF;
		if (table_idx >= DMR_TABLE_MAX ||
				table_idx >= g_dmr_param.dmr_basic_info.basic_param.table_cnt) {
			DDPINFO("%s:%d, table_idx %d is invalid\n",
					__func__, __LINE__, table_idx);
			return -EFAULT;
		}
		size_alloc = sizeof(struct mtk_oddmr_dmr_table);
		if (g_dmr_param.dmr_tables[table_idx] == NULL) {
			DDPINFO("%s:%d, dmr_table%d is NULL\n",
					__func__, __LINE__, table_idx);
#ifndef APP_DEBUG
			data = kzalloc(size_alloc, GFP_KERNEL);
#else
			data = malloc(size_alloc);
			memset(data, 0, size_alloc);
#endif
			if (!data) {
				DDPINFO("%s:%d, param buffer alloc fail\n",
						__func__, __LINE__);
				return -EFAULT;
			}
			g_dmr_param.dmr_tables[table_idx] = data;
		} else {
			memset(g_dmr_param.dmr_tables[table_idx], 0, size_alloc);
		}
		priv->dmr_state = ODDMR_INVALID;
		ret = _mtk_oddmr_load_param(param);
		if (ret == 0) {
			if (0 == (g_dmr_param.valid_table & (1 << table_idx)))
				g_dmr_param.valid_table_cnt += 1;
			g_dmr_param.valid_table |= (1 << table_idx);
			priv->dmr_state = ODDMR_LOAD_PARTS;
		}
		DDPINFO("%s:%d, dmr table cnt %d, valid 0x%x\n",
				__func__, __LINE__,
				g_dmr_param.valid_table_cnt, g_dmr_param.valid_table);
		break;
	case ODDMR_OD_BASIC_INFO:
		if (is_od_basic_info_loaded) {
			DDPINFO("%s:%d, basic info is already loaded\n", __func__, __LINE__);
			return -EFAULT;
		}
		ret = _mtk_oddmr_load_param(param);
		if (ret >= 0) {
			is_od_basic_info_loaded = 1;
			DDPINFO("%s:%d, od basic info load success\n", __func__, __LINE__);
		}
		break;
	case ODDMR_OD_TABLE:
		table_idx = param->head_id >> 16 & 0xFF;
		if (table_idx >= OD_TABLE_MAX ||
				table_idx >= g_od_param.od_basic_info.basic_param.table_cnt) {
			DDPINFO("%s:%d, table_idx %d is invalid\n",
					__func__, __LINE__, table_idx);
			return -EFAULT;
		}
		priv->od_state = ODDMR_INVALID;
		size_alloc = sizeof(struct mtk_oddmr_od_table);
		if (g_od_param.od_tables[table_idx] == NULL) {
			DDPINFO("%s:%d, od_table%d is NULL\n",
					__func__, __LINE__, table_idx);
#ifndef APP_DEBUG
			data = kzalloc(size_alloc, GFP_KERNEL);
#else
			data = malloc(size_alloc);
			memset(data, 0, size_alloc);
#endif
			if (!data) {
				DDPINFO("%s:%d, param buffer alloc fail\n",
						__func__, __LINE__);
				return -EFAULT;
			}
			g_od_param.od_tables[table_idx] = data;
		} else {
			memset(g_od_param.od_tables[table_idx], 0, size_alloc);
		}
		ret = _mtk_oddmr_load_param(param);
		if (ret == 0) {
			if (0 == (g_od_param.valid_table & (1 << table_idx)))
				g_od_param.valid_table_cnt += 1;
			g_od_param.valid_table |= (1 << table_idx);
			priv->od_state = ODDMR_LOAD_PARTS;
		}
		DDPINFO("%s:%d, od table cnt %d, valid 0x%x\n",
				__func__, __LINE__,
				g_od_param.valid_table_cnt, g_od_param.valid_table);
		break;
	default:
		DDPINFO("%s:%d, param is invalid 0x%x\n",
				__func__, __LINE__, param->head_id);
		ret = -EINVAL;
		break;
	}
	return ret;
}

int mtk_oddmr_od_tuning_read(struct mtk_ddp_comp *comp, uint32_t table_idx,
		struct mtk_oddmr_sw_reg *sw_reg, struct mtk_oddmr_od_param *pparam)
{
	uint32_t map_id, *val;
	int ret = 0;

	map_id = (sw_reg->reg & 0xFFFF) / 4;
	val = &sw_reg->val;
	switch (map_id) {
	case OD_BASIC_RESOLUTION_SWITCH_MODE:
		*val = pparam->od_basic_info.basic_param.resolution_switch_mode;
		break;
	case OD_BASIC_PANEL_WIDTH:
		*val = pparam->od_basic_info.basic_param.panel_width;
		break;
	case OD_BASIC_PANEL_HEIGHT:
		*val = pparam->od_basic_info.basic_param.panel_height;
		break;
	case OD_BASIC_TABLE_CNT:
		*val = pparam->od_basic_info.basic_param.table_cnt;
		break;
	case OD_BASIC_OD_MODE:
		*val = pparam->od_basic_info.basic_param.od_mode;
		break;
	case OD_BASIC_DITHER_SEL:
		*val = pparam->od_basic_info.basic_param.dither_sel;
		break;
	case OD_BASIC_DITHER_CTL:
		*val = pparam->od_basic_info.basic_param.dither_ctl;
		break;
	case OD_BASIC_SCALING_MODE:
		*val = pparam->od_basic_info.basic_param.scaling_mode;
		break;
	case OD_BASIC_OD_HSK2:
		*val = pparam->od_basic_info.basic_param.od_hsk_2;
		break;
	case OD_BASIC_OD_HSK3:
		*val = pparam->od_basic_info.basic_param.od_hsk_3;
		break;
	case OD_BASIC_OD_HSK4:
		*val = pparam->od_basic_info.basic_param.od_hsk_4;
		break;
	case OD_BASIC_RESERVED:
		break;
	case OD_TABLE_WIDTH:
		*val = pparam->od_tables[table_idx]->table_basic_info.width;
		break;
	case OD_TABLE_HEIGHT:
		*val = pparam->od_tables[table_idx]->table_basic_info.height;
		break;
	case OD_TABLE_FPS:
		*val = pparam->od_tables[table_idx]->table_basic_info.fps;
		break;
	case OD_TABLE_BL:
		*val = pparam->od_tables[table_idx]->table_basic_info.dbv;
		break;
	case OD_TABLE_MIN_FPS:
		*val = pparam->od_tables[table_idx]->table_basic_info.min_fps;
		break;
	case OD_TABLE_MAX_FPS:
		*val = pparam->od_tables[table_idx]->table_basic_info.max_fps;
		break;
	case OD_TABLE_MIN_BL:
		*val = pparam->od_tables[table_idx]->table_basic_info.min_dbv;
		break;
	case OD_TABLE_MAX_BL:
		*val = pparam->od_tables[table_idx]->table_basic_info.max_dbv;
		break;
	case OD_TABLE_RESERVED:
		break;
	case OD_TABLE_FPS_CNT:
		*val = pparam->od_tables[table_idx]->fps_cnt;
		break;
	case OD_TABLE_BL_CNT:
		*val = pparam->od_tables[table_idx]->bl_cnt;
		break;
	case OD_TABLE_FPS0:
	case OD_TABLE_FPS1:
	case OD_TABLE_FPS2:
	case OD_TABLE_FPS3:
	case OD_TABLE_FPS4:
	case OD_TABLE_FPS5:
	case OD_TABLE_FPS6:
	case OD_TABLE_FPS7:
	case OD_TABLE_FPS8:
	case OD_TABLE_FPS9:
	case OD_TABLE_FPS10:
	case OD_TABLE_FPS11:
	case OD_TABLE_FPS12:
	case OD_TABLE_FPS13:
	case OD_TABLE_FPS14:
	{
		int idx;

		idx = map_id - OD_TABLE_FPS0;
		*val = pparam->od_tables[table_idx]->fps_table[idx].item;
		break;
	}
	case OD_TABLE_FPS_WEIGHT0:
	case OD_TABLE_FPS_WEIGHT1:
	case OD_TABLE_FPS_WEIGHT2:
	case OD_TABLE_FPS_WEIGHT3:
	case OD_TABLE_FPS_WEIGHT4:
	case OD_TABLE_FPS_WEIGHT5:
	case OD_TABLE_FPS_WEIGHT6:
	case OD_TABLE_FPS_WEIGHT7:
	case OD_TABLE_FPS_WEIGHT8:
	case OD_TABLE_FPS_WEIGHT9:
	case OD_TABLE_FPS_WEIGHT10:
	case OD_TABLE_FPS_WEIGHT11:
	case OD_TABLE_FPS_WEIGHT12:
	case OD_TABLE_FPS_WEIGHT13:
	case OD_TABLE_FPS_WEIGHT14:
	{
		int idx;

		idx = map_id - OD_TABLE_FPS_WEIGHT0;
		*val = pparam->od_tables[table_idx]->fps_table[idx].value;
		break;
	}
	case OD_TABLE_BL0:
	case OD_TABLE_BL1:
	case OD_TABLE_BL2:
	case OD_TABLE_BL3:
	case OD_TABLE_BL4:
	case OD_TABLE_BL5:
	case OD_TABLE_BL6:
	case OD_TABLE_BL7:
	case OD_TABLE_BL8:
	case OD_TABLE_BL9:
	case OD_TABLE_BL10:
	case OD_TABLE_BL11:
	case OD_TABLE_BL12:
	case OD_TABLE_BL13:
	case OD_TABLE_BL14:
	{
		int idx;

		idx = map_id - OD_TABLE_BL0;
		*val = pparam->od_tables[table_idx]->bl_table[idx].item;
		break;
	}
	case OD_TABLE_BL_WEIGHT0:
	case OD_TABLE_BL_WEIGHT1:
	case OD_TABLE_BL_WEIGHT2:
	case OD_TABLE_BL_WEIGHT3:
	case OD_TABLE_BL_WEIGHT4:
	case OD_TABLE_BL_WEIGHT5:
	case OD_TABLE_BL_WEIGHT6:
	case OD_TABLE_BL_WEIGHT7:
	case OD_TABLE_BL_WEIGHT8:
	case OD_TABLE_BL_WEIGHT9:
	case OD_TABLE_BL_WEIGHT10:
	case OD_TABLE_BL_WEIGHT11:
	case OD_TABLE_BL_WEIGHT12:
	case OD_TABLE_BL_WEIGHT13:
	case OD_TABLE_BL_WEIGHT14:
	{
		int idx;

		idx = map_id - OD_TABLE_BL_WEIGHT0;
		*val = pparam->od_tables[table_idx]->bl_table[idx].value;
		break;
	}
	default:
		ret = -EFAULT;
		break;
	}
	DDPINFO("%s:%d, ret %d 0x%x,0x%x\n",
			__func__, __LINE__, ret, map_id, *val);
	return ret;
}

int mtk_oddmr_od_tuning_write(struct mtk_ddp_comp *comp, uint32_t table_idx,
		struct mtk_oddmr_sw_reg *sw_reg, struct mtk_oddmr_od_param *pparam)
{
	uint32_t map_id, val;
	int ret = 0;

	map_id = (sw_reg->reg & 0xFFFF) / 4;
	val = sw_reg->val;
	switch (map_id) {
	case OD_BASIC_RESOLUTION_SWITCH_MODE:
		pparam->od_basic_info.basic_param.resolution_switch_mode = val;
		break;
	case OD_BASIC_PANEL_WIDTH:
		pparam->od_basic_info.basic_param.panel_width = val;
		break;
	case OD_BASIC_PANEL_HEIGHT:
		pparam->od_basic_info.basic_param.panel_height = val;
		break;
	case OD_BASIC_TABLE_CNT:
		pparam->od_basic_info.basic_param.table_cnt = val;
		break;
	case OD_BASIC_OD_MODE:
		pparam->od_basic_info.basic_param.od_mode = val;
		break;
	case OD_BASIC_DITHER_SEL:
		pparam->od_basic_info.basic_param.dither_sel = val;
		break;
	case OD_BASIC_DITHER_CTL:
		pparam->od_basic_info.basic_param.dither_ctl = val;
		break;
	case OD_BASIC_SCALING_MODE:
		pparam->od_basic_info.basic_param.scaling_mode = val;
		break;
	case OD_BASIC_OD_HSK2:
		pparam->od_basic_info.basic_param.od_hsk_2 = val;
		break;
	case OD_BASIC_OD_HSK3:
		pparam->od_basic_info.basic_param.od_hsk_3 = val;
		break;
	case OD_BASIC_OD_HSK4:
		pparam->od_basic_info.basic_param.od_hsk_4 = val;
		break;
	case OD_BASIC_RESERVED:
		break;
	case OD_TABLE_WIDTH:
		pparam->od_tables[table_idx]->table_basic_info.width = val;
		break;
	case OD_TABLE_HEIGHT:
		pparam->od_tables[table_idx]->table_basic_info.height = val;
		break;
	case OD_TABLE_FPS:
		pparam->od_tables[table_idx]->table_basic_info.fps = val;
		break;
	case OD_TABLE_BL:
		pparam->od_tables[table_idx]->table_basic_info.dbv = val;
		break;
	case OD_TABLE_MIN_FPS:
		pparam->od_tables[table_idx]->table_basic_info.min_fps = val;
		break;
	case OD_TABLE_MAX_FPS:
		pparam->od_tables[table_idx]->table_basic_info.max_fps = val;
		break;
	case OD_TABLE_MIN_BL:
		pparam->od_tables[table_idx]->table_basic_info.min_dbv = val;
		break;
	case OD_TABLE_MAX_BL:
		pparam->od_tables[table_idx]->table_basic_info.max_dbv = val;
		break;
	case OD_TABLE_RESERVED:
		break;
	case OD_TABLE_FPS_CNT:
		pparam->od_tables[table_idx]->fps_cnt = val;
		break;
	case OD_TABLE_BL_CNT:
		pparam->od_tables[table_idx]->bl_cnt = val;
		break;
	case OD_TABLE_FPS0:
	case OD_TABLE_FPS1:
	case OD_TABLE_FPS2:
	case OD_TABLE_FPS3:
	case OD_TABLE_FPS4:
	case OD_TABLE_FPS5:
	case OD_TABLE_FPS6:
	case OD_TABLE_FPS7:
	case OD_TABLE_FPS8:
	case OD_TABLE_FPS9:
	case OD_TABLE_FPS10:
	case OD_TABLE_FPS11:
	case OD_TABLE_FPS12:
	case OD_TABLE_FPS13:
	case OD_TABLE_FPS14:
	{
		int idx;

		idx = map_id - OD_TABLE_FPS0;
		pparam->od_tables[table_idx]->fps_table[idx].item = val;
		break;
	}
	case OD_TABLE_FPS_WEIGHT0:
	case OD_TABLE_FPS_WEIGHT1:
	case OD_TABLE_FPS_WEIGHT2:
	case OD_TABLE_FPS_WEIGHT3:
	case OD_TABLE_FPS_WEIGHT4:
	case OD_TABLE_FPS_WEIGHT5:
	case OD_TABLE_FPS_WEIGHT6:
	case OD_TABLE_FPS_WEIGHT7:
	case OD_TABLE_FPS_WEIGHT8:
	case OD_TABLE_FPS_WEIGHT9:
	case OD_TABLE_FPS_WEIGHT10:
	case OD_TABLE_FPS_WEIGHT11:
	case OD_TABLE_FPS_WEIGHT12:
	case OD_TABLE_FPS_WEIGHT13:
	case OD_TABLE_FPS_WEIGHT14:
	{
		int idx;

		idx = map_id - OD_TABLE_FPS_WEIGHT0;
		pparam->od_tables[table_idx]->fps_table[idx].value = val;
		break;
	}
	case OD_TABLE_BL0:
	case OD_TABLE_BL1:
	case OD_TABLE_BL2:
	case OD_TABLE_BL3:
	case OD_TABLE_BL4:
	case OD_TABLE_BL5:
	case OD_TABLE_BL6:
	case OD_TABLE_BL7:
	case OD_TABLE_BL8:
	case OD_TABLE_BL9:
	case OD_TABLE_BL10:
	case OD_TABLE_BL11:
	case OD_TABLE_BL12:
	case OD_TABLE_BL13:
	case OD_TABLE_BL14:
	{
		int idx;

		idx = map_id - OD_TABLE_BL0;
		pparam->od_tables[table_idx]->bl_table[idx].item = val;
		break;
	}
	case OD_TABLE_BL_WEIGHT0:
	case OD_TABLE_BL_WEIGHT1:
	case OD_TABLE_BL_WEIGHT2:
	case OD_TABLE_BL_WEIGHT3:
	case OD_TABLE_BL_WEIGHT4:
	case OD_TABLE_BL_WEIGHT5:
	case OD_TABLE_BL_WEIGHT6:
	case OD_TABLE_BL_WEIGHT7:
	case OD_TABLE_BL_WEIGHT8:
	case OD_TABLE_BL_WEIGHT9:
	case OD_TABLE_BL_WEIGHT10:
	case OD_TABLE_BL_WEIGHT11:
	case OD_TABLE_BL_WEIGHT12:
	case OD_TABLE_BL_WEIGHT13:
	case OD_TABLE_BL_WEIGHT14:
	{
		int idx;

		idx = map_id - OD_TABLE_BL_WEIGHT0;
		pparam->od_tables[table_idx]->bl_table[idx].value = val;
		break;
	}
	default:
		ret = -EFAULT;
		break;
	}
	DDPINFO("%s:%d, ret %d 0x%x,0x%x\n",
			__func__, __LINE__, ret, map_id, val);
	return ret;
}


