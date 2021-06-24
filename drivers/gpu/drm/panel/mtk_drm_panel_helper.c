// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/of.h>
#include <linux/types.h>
#include "mtk_drm_panel_helper.h"

unsigned long long mtk_lcm_total_size;

int mtk_lcm_dts_read_u32_array(struct device_node *np, char *prop,
		u32 *out, int min_len, int max_len)
{
	int len = 0;

	if (IS_ERR_OR_NULL(prop) ||
	    IS_ERR_OR_NULL(np) ||
	    IS_ERR_OR_NULL(out) ||
	    max_len == 0)
		return 0;

	len = of_property_read_variable_u32_array(np,
			prop, out, min_len, max_len);
#if MTK_LCM_DEBUG_DUMP
	if (len == 1)
		DDPMSG("%s: %s = %u\n", __func__, prop, *out);
	else if (len > 0)
		DDPMSG("%s: %s array of %d data\n", __func__, prop, len);
	else if (len == 0)
		DDPMSG("%s: %s is empty\n", __func__, prop);
	else
		DDPMSG("%s: %s is not existed or overflow, %d\n",
			__func__, prop, len);
#else
	if (len < 0)
		DDPMSG("%s: %s is not existed or overflow, %d\n",
			__func__, prop, len);
#endif

	return len;
}

void mtk_lcm_dts_read_u32(struct device_node *np, char *prop,
		u32 *out)
{
	mtk_lcm_dts_read_u32_array(np, prop, out, 0, 1);
}

void mtk_lcm_dts_read_u8(struct device_node *np, char *prop,
		u8 *out)
{
	int ret = 0;
	u32 data = 0;

	ret = mtk_lcm_dts_read_u32_array(np, prop, &data, 0, 1);
	if (ret == 0)
		*out = (u8)data;
}

int mtk_lcm_dts_read_u8_array(struct device_node *np, char *prop,
		u8 *out, int min_len, int max_len)
{
	int len = 0, i = 0;
	u32 *data = NULL;

	if (IS_ERR_OR_NULL(prop) ||
	    IS_ERR_OR_NULL(np) ||
	    IS_ERR_OR_NULL(out) ||
	    max_len == 0)
		return 0;

	LCM_KZALLOC(data, sizeof(u32) * max_len, GFP_KERNEL);
	if (IS_ERR_OR_NULL(data)) {
		DDPMSG("%s, failed to allocate buffer\n", __func__);
		return -ENOMEM;
	}

	len = of_property_read_variable_u32_array(np,
			prop, data, min_len, max_len);
	if (len == 1) {
#if MTK_LCM_DEBUG_DUMP
		DDPMSG("%s: %s = 0x%x\n", __func__, prop, *data);
#endif
		*out = (u8)(*data);
	} else if (len > 0) {
		for (i = 0; i < len; i++)
			out[i] = (u8)data[i];
#if MTK_LCM_DEBUG_DUMP
		DDPMSG("%s: %s array of %d data\n", __func__, prop, len);
#endif
	} else if (len == 0) {
#if MTK_LCM_DEBUG_DUMP
		DDPMSG("%s: %s is empty\n", __func__, prop);
#endif
	} else
		DDPMSG("%s: %s is not existed or overflow, %d\n",
			__func__, prop, len);

	kfree(data);
	data = NULL;
	return len;
}

static int parse_lcm_params_dt_node(struct device_node *np,
		struct mtk_lcm_params *params)
{
	struct device_node *type_np = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(params)) {
		DDPPR_ERR("%s: invalid lcm params\n", __func__);
		return -ENOMEM;
	}
	memset(params, 0x0, sizeof(struct mtk_lcm_params));

	LCM_KZALLOC(params->name, MTK_LCM_NAME_LENGTH, GFP_KERNEL);
	if (IS_ERR_OR_NULL(params->name)) {
		DDPPR_ERR("%s, %d, failed to allocate lcm_name\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	ret = of_property_read_string(np, "lcm-params-name",
			&params->name);

	mtk_lcm_dts_read_u32(np, "lcm-params-types",
			&params->type);
	mtk_lcm_dts_read_u32_array(np, "lcm-params-resolution",
			&params->resolution[0], 0, 2);
	mtk_lcm_dts_read_u32(np, "lcm-params-physical_width",
			&params->physical_width);
	mtk_lcm_dts_read_u32(np, "lcm-params-physical_height",
			&params->physical_height);
	dump_lcm_params_basic(params);

	switch (params->type) {
	case MTK_LCM_FUNC_DBI:
		for_each_available_child_of_node(np, type_np) {
			if (of_device_is_compatible(type_np,
					"mediatek,lcm-params-dbi")) {
				DDPMSG("%s, LCM parse dbi params\n", __func__);
				ret = parse_lcm_params_dbi(type_np,
						&params->dbi_params);
				if (ret == 0)
					dump_lcm_params_dbi(&params->dbi_params, NULL);
				else
					free_lcm_params_dbi(&params->dbi_params);
			}
		}
		break;
	case MTK_LCM_FUNC_DPI:
		for_each_available_child_of_node(np, type_np) {
			if (of_device_is_compatible(type_np,
					"mediatek,lcm-params-dpi")) {
				DDPMSG("%s, LCM parse dpi params\n", __func__);
				ret = parse_lcm_params_dpi(type_np,
						&params->dpi_params);
				if (ret == 0)
					dump_lcm_params_dpi(&params->dpi_params, NULL);
				else
					free_lcm_params_dpi(&params->dpi_params);
			}
		}
		break;
	case MTK_LCM_FUNC_DSI:
		for_each_available_child_of_node(np, type_np) {
			if (of_device_is_compatible(type_np,
					"mediatek,lcm-params-dsi")) {
				DDPMSG("%s, LCM parse dsi params\n", __func__);
				ret = parse_lcm_params_dsi(type_np,
						&params->dsi_params);
				if (ret == 0)
					dump_lcm_params_dsi(&params->dsi_params, NULL);
				else
					free_lcm_params_dsi(&params->dsi_params);
			}
		}
		break;
	default:
		DDPPR_ERR("%s, invalid lcm type:%d\n",
			__func__, params->type);
		break;
	}

	return ret;
}
#define MTK_LCM_DATA_OFFSET (2)
static int parse_lcm_ops_func_util(struct mtk_lcm_ops_data *lcm_op, u8 *dts,
		unsigned int len)
{
	if (IS_ERR_OR_NULL(lcm_op) ||
	    IS_ERR_OR_NULL(dts)) {
		DDPMSG("%s,%d: invalid parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	switch (lcm_op->type) {
	case MTK_LCM_UTIL_TYPE_RESET:
		/* func type size data */
		lcm_op->param.util_data =  (unsigned int)dts[MTK_LCM_DATA_OFFSET];
		break;
	case MTK_LCM_UTIL_TYPE_MDELAY:
	case MTK_LCM_UTIL_TYPE_UDELAY:
		/* func type size data */
		lcm_op->param.util_data = (unsigned int)dts[MTK_LCM_DATA_OFFSET];
		break;
	case MTK_LCM_UTIL_TYPE_POWER_VOLTAGE:
		/* func type size addr level */
		lcm_op->param.util_data =  (unsigned int)dts[MTK_LCM_DATA_OFFSET];
		break;
	default:
		DDPPR_ERR("%s/%d: invalid type:%d\n",
			__func__, __LINE__, lcm_op->type);
		return -EINVAL;
	}

	return 0;
}

static int parse_lcm_ops_func_cmd(struct mtk_lcm_ops_data *lcm_op, u8 *dts,
		unsigned int len)
{
	if (IS_ERR_OR_NULL(lcm_op) ||
	    IS_ERR_OR_NULL(dts)) {
		DDPMSG("%s,%d: invalid parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	switch (lcm_op->type) {
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER:
		/* func type size data0 data1 ... */
		LCM_KZALLOC(lcm_op->param.buffer_data, lcm_op->size + 1, GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcm_op->param.buffer_data)) {
			DDPPR_ERR("%s,%d: failed to allocate data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		memcpy(lcm_op->param.buffer_data,
			dts + MTK_LCM_DATA_OFFSET, lcm_op->size);
		*(lcm_op->param.buffer_data + lcm_op->size) = '\0';
		break;
	case MTK_LCM_CMD_TYPE_WRITE_CMD:
		/* func type size cmd data0 data1 ... */
		LCM_KZALLOC(lcm_op->param.cmd_data.data, lcm_op->size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcm_op->param.cmd_data.data)) {
			DDPPR_ERR("%s,%d: failed to allocate data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		lcm_op->param.cmd_data.cmd = dts[MTK_LCM_DATA_OFFSET];
		lcm_op->param.cmd_data.data_len = lcm_op->size - 1;
		memcpy(lcm_op->param.cmd_data.data,
			dts + MTK_LCM_DATA_OFFSET + 1,
			lcm_op->size - 1);
		*(lcm_op->param.cmd_data.data + lcm_op->size - 1) = '\0';
		break;
	case MTK_LCM_CMD_TYPE_READ_BUFFER:
	case MTK_LCM_CMD_TYPE_READ_CMD:
		/* func type size cmd data0 data1 ... */
		LCM_KZALLOC(lcm_op->param.cmd_data.data, lcm_op->size - 2, GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcm_op->param.cmd_data.data)) {
			DDPPR_ERR("%s,%d: failed to allocate data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		lcm_op->param.cmd_data.start_id = dts[MTK_LCM_DATA_OFFSET];
		lcm_op->param.cmd_data.data_len = dts[MTK_LCM_DATA_OFFSET + 1];
		lcm_op->param.cmd_data.cmd = dts[MTK_LCM_DATA_OFFSET + 2];
		*(lcm_op->param.cmd_data.data) = '\0';
		break;
	default:
		DDPPR_ERR("%s/%d: invalid type:%d\n",
			__func__, __LINE__, lcm_op->type);
		return -EINVAL;
	}
	return 0;
}

static int parse_lcm_ops_func_cb(struct mtk_lcm_ops_data *lcm_op, u8 *dts,
		unsigned int len)
{
	if (IS_ERR_OR_NULL(lcm_op) ||
	    IS_ERR_OR_NULL(dts)) {
		DDPMSG("%s,%d: invalid parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	switch (lcm_op->type) {
	case MTK_LCM_CB_TYPE_RUNTIME:
		/* func type size data0 data1 ... */
		LCM_KZALLOC(lcm_op->param.cb_id_data.buffer_data, lcm_op->size + 1, GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcm_op->param.cb_id_data.buffer_data)) {
			DDPPR_ERR("%s,%d: failed to allocate data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		memcpy(lcm_op->param.cb_id_data.buffer_data,
			dts + MTK_LCM_DATA_OFFSET, lcm_op->size);
		*(lcm_op->param.cb_id_data.buffer_data + lcm_op->size) = '\0';
		break;
	case MTK_LCM_CB_TYPE_RUNTIME_INPUT:
		/* func type size data0 data1 ... */
		LCM_KZALLOC(lcm_op->param.cb_id_data.buffer_data, lcm_op->size, GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcm_op->param.cb_id_data.buffer_data)) {
			DDPPR_ERR("%s,%d: failed to allocate data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		lcm_op->param.cb_id_data.id = dts[MTK_LCM_DATA_OFFSET];
		memcpy(lcm_op->param.cb_id_data.buffer_data,
			dts + MTK_LCM_DATA_OFFSET + 1,
			lcm_op->size - 1);
		*(lcm_op->param.cb_id_data.buffer_data + lcm_op->size - 1) = '\0';
		break;
	default:
		DDPPR_ERR("%s/%d: invalid type:%d\n",
			__func__, __LINE__, lcm_op->type);
		return -EINVAL;
	}
	return 0;
}

static int parse_lcm_ops_func_gpio(struct mtk_lcm_ops_data *lcm_op, u8 *dts,
		unsigned int len)
{
	if (IS_ERR_OR_NULL(lcm_op) ||
	    IS_ERR_OR_NULL(dts)) {
		DDPMSG("%s,%d: invalid parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	switch (lcm_op->type) {
	case MTK_LCM_GPIO_TYPE_MODE:
	case MTK_LCM_GPIO_TYPE_DIR_OUTPUT:
	case MTK_LCM_GPIO_TYPE_DIR_INPUT:
	case MTK_LCM_GPIO_TYPE_OUT:
		if (lcm_op->size != 2) {
			DDPMSG("%s,%d: invalid gpio size %u\n",
				__func__, __LINE__, lcm_op->size);
			return -EINVAL;
		}
		lcm_op->param.gpio_data.gpio_id = dts[MTK_LCM_DATA_OFFSET];
		lcm_op->param.gpio_data.data = dts[MTK_LCM_DATA_OFFSET + 1];
		break;
	default:
		DDPPR_ERR("%s/%d: invalid type:%d\n",
			__func__, __LINE__, lcm_op->type);
		return -EINVAL;
	}
	return 0;
}

static int parse_lcm_ops_func_cust(struct mtk_lcm_ops_data *lcm_op,
		u8 *dts, struct mtk_panel_cust *cust)
{
	if (IS_ERR_OR_NULL(lcm_op) ||
	    IS_ERR_OR_NULL(dts) || IS_ERR_OR_NULL(cust))
		return -EINVAL;

	if (atomic_read(&cust->cust_enabled) == 0 ||
	    IS_ERR_OR_NULL(cust->parse_ops))
		return -EINVAL;

	cust->parse_ops(lcm_op->func, lcm_op->type,
		 dts, lcm_op->size,	lcm_op->param.cust_data);

	return 0;
}

static int parse_lcm_ops_basic(struct mtk_lcm_ops_data *lcm_op, u8 *dts,
		struct mtk_panel_cust *cust, unsigned int len)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(lcm_op) || IS_ERR_OR_NULL(dts) || len == 0) {
		DDPPR_ERR("%s %d: invalid cmd\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (len - lcm_op->size < MTK_LCM_DATA_OFFSET) {
		DDPMSG("%s,%d: dts length is not enough %u,%u\n",
			__func__, __LINE__, len, lcm_op->size);
		return -EINVAL;
	}

	if (lcm_op->type > MTK_LCM_UTIL_TYPE_START &&
	    lcm_op->type < MTK_LCM_UTIL_TYPE_END)
		ret = parse_lcm_ops_func_util(lcm_op, dts, len);
	else if (lcm_op->type > MTK_LCM_CMD_TYPE_START &&
	    lcm_op->type < MTK_LCM_CMD_TYPE_END)
		ret = parse_lcm_ops_func_cmd(lcm_op, dts, len);
	else if (lcm_op->type > MTK_LCM_LK_TYPE_START &&
	    lcm_op->type < MTK_LCM_LK_TYPE_END)
		ret = parse_lcm_ops_func_cmd(lcm_op, dts, len);
	else if (lcm_op->type > MTK_LCM_CB_TYPE_START &&
	    lcm_op->type < MTK_LCM_CB_TYPE_END)
		ret = parse_lcm_ops_func_cb(lcm_op, dts, len);
	else if (lcm_op->type > MTK_LCM_GPIO_TYPE_START &&
	    lcm_op->type < MTK_LCM_GPIO_TYPE_END)
		ret = parse_lcm_ops_func_gpio(lcm_op, dts, len);
	else if (lcm_op->type > MTK_LCM_CUST_TYPE_START &&
	    lcm_op->type < MTK_LCM_CUST_TYPE_END)
		ret = parse_lcm_ops_func_cust(lcm_op,
				dts, cust);
	else {
		DDPPR_ERR("%s/%d: invalid type:0x%x\n",
			__func__, __LINE__, lcm_op->type);
		return -EINVAL;
	}

	return ret;
}

int parse_lcm_ops_check(struct mtk_lcm_ops_data *ops, u8 *dts,
		unsigned int len, int curr_status, unsigned int phase)
{
	unsigned int i = 0, phase_tmp = 0;
	int phase_skip_flag = curr_status;

	if (IS_ERR_OR_NULL(ops) || IS_ERR_OR_NULL(dts)) {
		DDPMSG("%s,%d: invalid parameters, len:%u\n",
			__func__, __LINE__, len);
		return phase_skip_flag;
	}

	if (len - ops->size < MTK_LCM_DATA_OFFSET) {
		DDPMSG("%s,%d: dts length is not enough %u,%u\n",
			__func__, __LINE__, len, ops->size);
		return phase_skip_flag;
	}

	if (ops->type == MTK_LCM_PHASE_TYPE_START &&
	    curr_status != MTK_LCM_PHASE_TYPE_START) {
		for (i = 0; i < ops->size; i++)
			phase_tmp |= dts[i + MTK_LCM_DATA_OFFSET];

		if ((phase_tmp & phase) != phase)
			phase_skip_flag = MTK_LCM_PHASE_TYPE_START;
	} else if (ops->type == MTK_LCM_PHASE_TYPE_END &&
	    curr_status == MTK_LCM_PHASE_TYPE_START) {
		for (i = 0; i < ops->size; i++)
			phase_tmp |= dts[i + MTK_LCM_DATA_OFFSET];

		if ((phase_tmp & phase) != phase)
			phase_skip_flag = MTK_LCM_PHASE_TYPE_END;
	} else if (curr_status == MTK_LCM_PHASE_TYPE_END) {
		phase_skip_flag = 0;
	}

	return phase_skip_flag;
}

u8 table_dts_buf[MTK_PANEL_TABLE_OPS_COUNT * 1024];
int parse_lcm_ops_func(struct device_node *np,
		struct mtk_lcm_ops_data *table,
		char *func, unsigned int size,
		unsigned int panel_type,
		struct mtk_panel_cust *cust, unsigned int phase)
{
	unsigned int i = 0, count = 0, skip_count = 0;
	u8 *tmp;
	int len = 0, tmp_len = 0, ret = 0;
	int phase_skip_flag = 0;

	if (IS_ERR_OR_NULL(func) ||
	    IS_ERR_OR_NULL(table) ||
	    panel_type >= MTK_LCM_FUNC_END) {
		DDPMSG("%s:%d: invalid parameters\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (size > MTK_PANEL_TABLE_OPS_COUNT) {
		DDPMSG("%s:%d: too big table size:%u\n",
			__func__, __LINE__, size);
		return -EINVAL;
	}

	memset(table_dts_buf, 0, sizeof(table_dts_buf));
	len = mtk_lcm_dts_read_u8_array(np,
				func, &table_dts_buf[0], 0, sizeof(table_dts_buf));
	if (len <= 0) {
		DDPMSG("%s:%d: Cannot find table of %s\n",
			__func__, __LINE__, func);
		return 0;
	} else if ((unsigned int)len < sizeof(table_dts_buf)) {
		table_dts_buf[len] = '\0';
		DDPMSG("%s: start to parse:%s, table_size:%u, dts_len:%u, phase:0x%x\n",
			__func__, func, size, len, phase);
	} else {
		table_dts_buf[ARRAY_SIZE(table_dts_buf) - 1] = '\0';
		DDPMSG("%s: start to parse:%s, len:%u has out of size, %s\n",
			__func__, func, len, table_dts_buf);
	}

	tmp = &table_dts_buf[0];
	for (i = 0; i < size; i++) {
		table[i].func = panel_type;
		table[i].type = tmp[0];
		if (table[i].type == MTK_LCM_TYPE_END) {
			len = len - 1;
			DDPMSG("%s: parsing end of %s, len:%d\n", __func__, func, len);
			break;
		}

		table[i].size = tmp[1];
		tmp_len = MTK_LCM_DATA_OFFSET + table[i].size;
		phase_skip_flag = parse_lcm_ops_check(&table[i],
					tmp, len, phase_skip_flag, phase);

		if (phase_skip_flag == 0 &&
		    table[i].type != MTK_LCM_PHASE_TYPE_START &&
		    table[i].type != MTK_LCM_PHASE_TYPE_END) {
			ret = parse_lcm_ops_basic(&table[i], tmp, cust, len);
#if MTK_LCM_DEBUG_DUMP
			DDPMSG(
				"[%s+%d] >>>func:%u,type:%u,size:%u,dts:%u,op:%u,ret:%d,phase:0x%x,skip:%d\n",
				func, i, table[i].func, table[i].type,
				table[i].size, len, tmp_len, ret,
				phase, phase_skip_flag);
#endif
			if (ret != 0) {
				DDPMSG("[%s+%d] >>>func:%u,type:%u,size:%u,dts:%u,fail:%d\n",
					func, i, table[i].func, table[i].type,
					table[i].size, len, ret);
				break;
			}
		} else {
			DDPMSG("[%s+%d] >>>func:%u,type:%u skipped:0x%x,phase:0x%x\n",
				func, i, table[i].func, table[i].type,
				phase_skip_flag, phase);
			table[i].func = 0;
			table[i].type = 0;
			table[i].size = 0;
			i--;
			skip_count++;
		}

		if (tmp_len < len) {
			tmp = tmp + tmp_len;
			len = len - tmp_len;
		} else {
			DDPMSG("%s: parsing warning of %s, len:%d\n", __func__, func, len);
			break;
		}
	}

	count = i;
	if (count + skip_count != size || len > 1)
		DDPMSG("%s:%d: warning:%s+%u,skip:%u,expect:%u:dts:%u,last%u\n",
			__func__, __LINE__, func, count,
			skip_count, size, len, tmp_len);

	return count;
}
EXPORT_SYMBOL(parse_lcm_ops_func);

static int parse_lcm_ops_dt_node(struct device_node *np,
		struct mtk_lcm_ops *ops, struct mtk_lcm_params *params,
		struct mtk_panel_cust *cust)
{
	struct device_node *type_np = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(ops) ||
	    IS_ERR_OR_NULL(params) ||
	    IS_ERR_OR_NULL(np)) {
		DDPPR_ERR("%s: invalid lcm ops\n", __func__);
		return -ENOMEM;
	}

	//DDPMSG("%s ++\n", __func__);
	switch (params->type) {
	case MTK_LCM_FUNC_DBI:
		for_each_available_child_of_node(np, type_np) {
			if (of_device_is_compatible(type_np,
					"mediatek,lcm-ops-dbi")) {
				LCM_KZALLOC(ops->dbi_ops,
					sizeof(struct mtk_lcm_ops_dbi), GFP_KERNEL);
				if (IS_ERR_OR_NULL(ops->dbi_ops)) {
					DDPMSG("%s, %d, failed\n", __func__, __LINE__);
					return -ENOMEM;
				}

				DDPMSG("%s, LCM parse dbi params\n", __func__);
				ret = parse_lcm_ops_dbi(type_np,
						ops->dbi_ops, &params->dbi_params, cust);
				if (ret == 0)
					dump_lcm_ops_dbi(ops->dbi_ops, &params->dbi_params, NULL);
				else
					free_lcm_ops_dbi(ops->dbi_ops);
			}
		}
		break;
	case MTK_LCM_FUNC_DPI:
		for_each_available_child_of_node(np, type_np) {
			if (of_device_is_compatible(type_np,
					"mediatek,lcm-ops-dpi")) {
				LCM_KZALLOC(ops->dpi_ops,
					sizeof(struct mtk_lcm_ops_dpi), GFP_KERNEL);
				if (IS_ERR_OR_NULL(ops->dpi_ops)) {
					DDPMSG("%s, %d, failed\n", __func__, __LINE__);
					return -ENOMEM;
				}
				DDPMSG("%s, LCM parse dpi params\n", __func__);
				ret = parse_lcm_ops_dpi(type_np,
						ops->dpi_ops, &params->dpi_params, cust);
				if (ret == 0)
					dump_lcm_ops_dpi(ops->dpi_ops, &params->dpi_params, NULL);
				else
					free_lcm_ops_dpi(ops->dpi_ops);
			}
		}
		break;
	case MTK_LCM_FUNC_DSI:
		for_each_available_child_of_node(np, type_np) {
			if (of_device_is_compatible(type_np,
					"mediatek,lcm-ops-dsi")) {
				LCM_KZALLOC(ops->dsi_ops,
					sizeof(struct mtk_lcm_ops_dsi), GFP_KERNEL);
				if (IS_ERR_OR_NULL(ops->dsi_ops)) {
					DDPMSG("%s, %d, failed\n", __func__, __LINE__);
					return -ENOMEM;
				}
				DDPMSG("%s, LCM parse dsi params\n", __func__);
				ret = parse_lcm_ops_dsi(type_np,
						ops->dsi_ops, &params->dsi_params, cust);
				if (ret == 0)
					dump_lcm_ops_dsi(ops->dsi_ops, &params->dsi_params, NULL);
				else
					free_lcm_ops_dsi(ops->dsi_ops);
			}
		}
		break;
	default:
		DDPPR_ERR("%s, invalid lcm type:%d\n",
			__func__, params->type);
		ret = -EINVAL;
		break;
	}

	DDPMSG("%s -- ret:%d\n", __func__, ret);
	return ret;
}

int load_panel_resource_from_dts(struct device_node *lcm_np,
		struct mtk_panel_resource *data)
{
	struct device_node *np = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(lcm_np) || IS_ERR_OR_NULL(data)) {
		DDPPR_ERR("%s:%d: invalid lcm node\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	DDPMSG("%s ++, total_size:%lluByte\n",
		__func__, mtk_lcm_total_size);
	/* Load LCM parameters from DT */
	for_each_available_child_of_node(lcm_np, np) {
		if (of_device_is_compatible(np, "mediatek,lcm-params")) {
			ret = parse_lcm_params_dt_node(np, &data->params);
			if (ret != 0)
				return ret;
			DDPMSG("%s: parsing lcm-params, total_size:%lluByte\n",
				__func__, mtk_lcm_total_size);

			if (atomic_read(&data->cust.cust_enabled) == 1 &&
			    data->cust.parse_params != NULL) {
				DDPMSG("%s: parsing cust settings, enable:%d, func:0x%lx\n",
					__func__, data->cust.cust_enabled,
					(unsigned long)data->cust.parse_params);
				ret = data->cust.parse_params(np);
				if (ret != 0)
					DDPMSG("%s, failed at cust parsing, %d\n",
						__func__, ret);
				DDPMSG("%s: parsing lcm-params-cust, total_size:%lluByte\n",
					__func__, mtk_lcm_total_size);
			}
		}
	}

	/* Load LCM ops from DT */
	for_each_available_child_of_node(lcm_np, np) {
		if (of_device_is_compatible(np, "mediatek,lcm-ops")) {
			ret = parse_lcm_ops_dt_node(np, &data->ops,
				&data->params, &data->cust);
			if (ret != 0) {
				DDPMSG("%s, failed to parse operations, %d\n", __func__, ret);
				return ret;
			}
			DDPMSG("%s: parsing lcm-ops, total_size:%lluByte\n",
				__func__, mtk_lcm_total_size);
		}
	}

	return 0;
}
EXPORT_SYMBOL(load_panel_resource_from_dts);

static void mtk_get_func_name(char func, char *out)
{
	char name[MTK_LCM_NAME_LENGTH] = { 0 };

	if (IS_ERR_OR_NULL(out)) {
		DDPMSG("%s: invalid out buffer\n", __func__);
		return;
	}

	switch (func) {
	case MTK_LCM_FUNC_DBI:
		snprintf(&name[0], MTK_LCM_NAME_LENGTH - 1, "DBI");
		break;
	case MTK_LCM_FUNC_DPI:
		snprintf(&name[0], MTK_LCM_NAME_LENGTH - 1, "DPI");
		break;
	case MTK_LCM_FUNC_DSI:
		snprintf(&name[0], MTK_LCM_NAME_LENGTH - 1, "DSI");
		break;
	default:
		snprintf(&name[0], MTK_LCM_NAME_LENGTH - 1, "unknown");
		break;
	}

	snprintf(out, MTK_LCM_NAME_LENGTH - 1, &name[0]);
}

static void mtk_get_type_name(unsigned int type, char *out)
{
	char name[MTK_LCM_NAME_LENGTH] = { 0 };

	if (IS_ERR_OR_NULL(out)) {
		DDPMSG("%s: invalid out buffer\n", __func__);
		return;
	}

	switch (type) {
	case MTK_LCM_UTIL_TYPE_RESET:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "RESET");
		break;
	case MTK_LCM_UTIL_TYPE_MDELAY:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "MDELAY");
		break;
	case MTK_LCM_UTIL_TYPE_UDELAY:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "UDELAY");
		break;
	case MTK_LCM_UTIL_TYPE_TDELAY:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "TICK_DELAY");
		break;
	case MTK_LCM_UTIL_TYPE_POWER_VOLTAGE:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1,	"POWER_VOLTAGE");
		break;
	case MTK_LCM_UTIL_TYPE_POWER_ON:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "POWER_ON");
		break;
	case MTK_LCM_UTIL_TYPE_POWER_OFF:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1,	"POWER_OFF");
		break;
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "WRITE_BUF");
		break;
	case MTK_LCM_CMD_TYPE_WRITE_CMD:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "WRITE_CMD");
		break;
	case MTK_LCM_CMD_TYPE_READ_BUFFER:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "READ_BUF");
		break;
	case MTK_LCM_CMD_TYPE_READ_CMD:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "READ_CMD");
		break;
	case MTK_LCM_CB_TYPE_RUNTIME:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "CB_RUNTIME");
		break;
	case MTK_LCM_CB_TYPE_RUNTIME_INPUT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "CB_RUNTIME_INOUT");
		break;
	case MTK_LCM_GPIO_TYPE_MODE:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "GPIO_MODE");
		break;
	case MTK_LCM_GPIO_TYPE_DIR_OUTPUT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "GPIO_DIR_OUT");
		break;
	case MTK_LCM_GPIO_TYPE_DIR_INPUT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "GPIO_DIR_IN");
		break;
	case MTK_LCM_GPIO_TYPE_OUT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "GPIO_OUT");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_COUNT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_COUNT");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_PARAM");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_FIX_BIT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_FIX");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_X0_MSB_BIT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_X0_MSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_X0_LSB_BIT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_X0_LSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_X1_MSB_BIT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_X1_MSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_X1_LSB_BIT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_X1_LSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_Y0_MSB_BIT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_Y0_MSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_Y0_LSB_BIT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_Y0_LSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_Y1_MSB_BIT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_Y1_MSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_Y1_LSB_BIT:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_Y1_LSB");
		break;
	case MTK_LCM_LK_TYPE_WRITE_PARAM:
		snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_WRITE_PARAM");
		break;
	default:
		if (type > MTK_LCM_CUST_TYPE_START &&
		    type < MTK_LCM_CUST_TYPE_END)
			snprintf(name, MTK_LCM_NAME_LENGTH - 1,
				"CUST-%d", type);
		snprintf(name, MTK_LCM_NAME_LENGTH - 1,
				"unknown");
		break;
	}

	snprintf(out, MTK_LCM_NAME_LENGTH - 1, &name[0]);
}

static void dump_lcm_ops_func_util(struct mtk_lcm_ops_data *lcm_op,
	const char *owner, unsigned int id)
{
	char func_name[MTK_LCM_NAME_LENGTH] = { 0 };
	char type_name[MTK_LCM_NAME_LENGTH] = { 0 };

	if (IS_ERR_OR_NULL(lcm_op) || IS_ERR_OR_NULL(owner))
		return;

	switch (lcm_op->type) {
	case MTK_LCM_UTIL_TYPE_RESET:
	case MTK_LCM_UTIL_TYPE_MDELAY:
	case MTK_LCM_UTIL_TYPE_UDELAY:
	case MTK_LCM_UTIL_TYPE_POWER_VOLTAGE:
		mtk_get_func_name(lcm_op->func, &func_name[0]);
		mtk_get_type_name(lcm_op->type, &type_name[0]);
		DDPDUMP("[%s-%u]: func:%s, type:%s, dts_size:%u, data:%u\n",
			owner, id, func_name, type_name,
			lcm_op->size, lcm_op->param.util_data);
		break;
	case MTK_LCM_UTIL_TYPE_POWER_ON:
	case MTK_LCM_UTIL_TYPE_POWER_OFF:
		break;
	default:
		DDPDUMP("%s: [%s-%u]: invalid type:%u\n",
			__func__, owner, id, lcm_op->type);
		break;
	}
}

static void dump_lcm_ops_func_cmd(struct mtk_lcm_ops_data *lcm_op,
	const char *owner, unsigned int id)
{
	char func_name[MTK_LCM_NAME_LENGTH] = { 0 };
	char type_name[MTK_LCM_NAME_LENGTH] = { 0 };
	unsigned int i = 0;

	if (IS_ERR_OR_NULL(lcm_op) || IS_ERR_OR_NULL(owner))
		return;

	mtk_get_func_name(lcm_op->func, &func_name[0]);
	mtk_get_type_name(lcm_op->type, &type_name[0]);
	switch (lcm_op->type) {
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER:
		DDPDUMP("[%s-%u]: func:%s, type:%s, dts_size:%u, para_count:%u\n",
			owner, id, func_name, type_name,
			lcm_op->size,
			lcm_op->size);
		for (i = 0; i < lcm_op->size; i += 4) {
			DDPDUMP("[%s-%u][data%u~%u]>>> 0x%x 0x%x 0x%x 0x%x\n",
				owner, id, i, i + 3,
				lcm_op->param.buffer_data[i],
				lcm_op->param.buffer_data[i + 1],
				lcm_op->param.buffer_data[i + 2],
				lcm_op->param.buffer_data[i + 3]);
		}
		break;
	case MTK_LCM_CMD_TYPE_WRITE_CMD:
		DDPDUMP("[%s-%u]:%s,%s,dts:%u,cmd:0x%x,data_len:%u,startid:%u\n",
			owner, id, func_name, type_name, lcm_op->size,
			lcm_op->param.cmd_data.cmd,
			lcm_op->param.cmd_data.data_len,
			lcm_op->param.cmd_data.start_id);
		for (i = 0; i < lcm_op->param.cmd_data.data_len; i += 4) {
			DDPDUMP("[%s-%u][data%u~%u]>>> 0x%x 0x%x 0x%x 0x%x\n",
				owner, id, i, i + 3,
				lcm_op->param.cmd_data.data[i],
				lcm_op->param.cmd_data.data[i + 1],
				lcm_op->param.cmd_data.data[i + 2],
				lcm_op->param.cmd_data.data[i + 3]);
		}
		break;
	case MTK_LCM_CMD_TYPE_READ_BUFFER:
	case MTK_LCM_CMD_TYPE_READ_CMD:
		DDPDUMP("[%s-%u]:%s,%s,dts:%u,cmd:0x%x,data_len:%u,startid:%u\n",
			owner, id, func_name, type_name, lcm_op->size,
			lcm_op->param.cmd_data.cmd,
			lcm_op->param.cmd_data.data_len,
			lcm_op->param.cmd_data.start_id);
		break;
	default:
		DDPDUMP("%s: [%s-%u]: invalid type:%u\n",
			__func__, owner, id, lcm_op->type);
		break;
	}
}

static void dump_lcm_ops_func_cb(struct mtk_lcm_ops_data *lcm_op,
		const char *owner, unsigned int id)
{
	char func_name[MTK_LCM_NAME_LENGTH] = { 0 };
	char type_name[MTK_LCM_NAME_LENGTH] = { 0 };
	unsigned int i = 0;

	if (IS_ERR_OR_NULL(lcm_op) || IS_ERR_OR_NULL(owner))
		return;

	mtk_get_func_name(lcm_op->func, &func_name[0]);
	mtk_get_type_name(lcm_op->type, &type_name[0]);
	switch (lcm_op->type) {
	case MTK_LCM_CB_TYPE_RUNTIME:
		DDPDUMP("[%s-%u]: func:%s, type:%s, dts_size:%u\n",
			owner, id, func_name, type_name, lcm_op->size);
		for (i = 0; i < MTK_LCM_DATA_ALIGNMENT(lcm_op->size, 4); i += 4) {
			DDPDUMP("[%s-%u][data%u~%u]>>> 0x%x 0x%x 0x%x 0x%x\n",
				owner, id, i, i + 3,
				lcm_op->param.cb_id_data.buffer_data[i],
				lcm_op->param.cb_id_data.buffer_data[i + 1],
				lcm_op->param.cb_id_data.buffer_data[i + 2],
				lcm_op->param.cb_id_data.buffer_data[i + 3]);
		}
		break;
	case MTK_LCM_CB_TYPE_RUNTIME_INPUT:
		DDPDUMP("[%s-%u]: func:%s, type:%s, dts_size:%u, id:%u\n",
			owner, id, func_name, type_name,
			lcm_op->size, lcm_op->param.cb_id_data.id);
		for (i = 0; i < MTK_LCM_DATA_ALIGNMENT(lcm_op->size - 1, 4); i += 4) {
			DDPDUMP("[%s-%u][data%u~%u]>>> 0x%x 0x%x 0x%x 0x%x\n",
				owner, id, i, i + 3,
				lcm_op->param.cb_id_data.buffer_data[i],
				lcm_op->param.cb_id_data.buffer_data[i + 1],
				lcm_op->param.cb_id_data.buffer_data[i + 2],
				lcm_op->param.cb_id_data.buffer_data[i + 3]);
		}
		break;
	default:
		DDPDUMP("%s: [%s-%u]: invalid type:%u\n",
			__func__, owner, id, lcm_op->type);
		break;
	}
}

static void dump_lcm_ops_func_gpio(struct mtk_lcm_ops_data *lcm_op,
		const char *owner, unsigned int id)
{
	char func_name[MTK_LCM_NAME_LENGTH] = { 0 };
	char type_name[MTK_LCM_NAME_LENGTH] = { 0 };

	if (IS_ERR_OR_NULL(lcm_op) || IS_ERR_OR_NULL(owner))
		return;

	switch (lcm_op->type) {
	case MTK_LCM_GPIO_TYPE_MODE:
	case MTK_LCM_GPIO_TYPE_DIR_OUTPUT:
	case MTK_LCM_GPIO_TYPE_DIR_INPUT:
	case MTK_LCM_GPIO_TYPE_OUT:
		mtk_get_func_name(lcm_op->func, &func_name[0]);
		mtk_get_type_name(lcm_op->type, &type_name[0]);
		DDPDUMP("[%s-%u]: func:%s, type:%s, dts_size:%u, gpio:%u, data:0x%x\n",
			owner, id, func_name, type_name, lcm_op->size,
			lcm_op->param.gpio_data.gpio_id,
			lcm_op->param.gpio_data.data);
		break;
	default:
		DDPDUMP("[%s-%u]: invalid type:%u\n",
			owner, id, lcm_op->type);
		break;
	}
}

void dump_lcm_ops_func(struct mtk_lcm_ops_data *table,
		unsigned int size,
		struct mtk_panel_cust *cust,
		const char *owner)
{
	struct mtk_lcm_ops_data *lcm_op = NULL;
	char owner_tmp[MTK_LCM_NAME_LENGTH] = { 0 };
	unsigned int i = 0;

	if (IS_ERR_OR_NULL(owner))
		snprintf(&owner_tmp[0], MTK_LCM_NAME_LENGTH - 1, "unknown");
	else
		snprintf(&owner_tmp[0], MTK_LCM_NAME_LENGTH - 1, owner);

	if (IS_ERR_OR_NULL(table) || size == 0) {
		DDPDUMP("%s: \"%s\" is empty\n", __func__, owner_tmp);
		return;
	}

	DDPDUMP("-------------%s(%u): 0x%lx-----------\n",
		owner_tmp, size, (unsigned long)table);
	for (i = 0; i < size; i++) {
		lcm_op = &table[i];
		if (IS_ERR_OR_NULL(lcm_op)) {
			DDPMSG("%s: table:%s, size:%u, ops:%u is null\n",
				__func__, owner_tmp, size, i);
			break;
		}
		if (lcm_op->type > MTK_LCM_UTIL_TYPE_START &&
		    lcm_op->type < MTK_LCM_UTIL_TYPE_END)
			dump_lcm_ops_func_util(lcm_op, owner_tmp, i);
		else if (lcm_op->type > MTK_LCM_CMD_TYPE_START &&
		    lcm_op->type < MTK_LCM_CMD_TYPE_END)
			dump_lcm_ops_func_cmd(lcm_op, owner_tmp, i);
		else if (lcm_op->type > MTK_LCM_CB_TYPE_START &&
		    lcm_op->type < MTK_LCM_CB_TYPE_END)
			dump_lcm_ops_func_cb(lcm_op, owner_tmp, i);
		else if (lcm_op->type > MTK_LCM_GPIO_TYPE_START &&
		    lcm_op->type < MTK_LCM_GPIO_TYPE_END)
			dump_lcm_ops_func_gpio(lcm_op, owner_tmp, i);
		else if (lcm_op->type > MTK_LCM_CUST_TYPE_START &&
		    lcm_op->type < MTK_LCM_CUST_TYPE_END) {
			if (cust != NULL &&
			    atomic_read(&cust->cust_enabled) != 0 &&
			    cust->dump_ops != NULL)
				cust->dump_ops(lcm_op, owner, i);
		} else
			DDPDUMP("[%s-%d]: invalid func:%u\n",
				owner, i, lcm_op->func);
	}
}
EXPORT_SYMBOL(dump_lcm_ops_func);

int mtk_execute_func_cmd(void *dev,
		struct mtk_lcm_ops_data *lcm_op,
		void *data, size_t size_out)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(lcm_op)) {
		DDPPR_ERR("%s %d: invalid cmd\n", __func__, __LINE__);
		return -EINVAL;
	}

	switch (lcm_op->type) {
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER:
		if (IS_ERR_OR_NULL(lcm_op->param.buffer_data) ||
		    lcm_op->size <= 0)
			return -EINVAL;
		ret = mtk_panel_dsi_dcs_write_buffer(
					(struct mipi_dsi_device *)dev,
					lcm_op->param.buffer_data,
					lcm_op->size);
		break;
	case MTK_LCM_CMD_TYPE_READ_BUFFER:
	{
		u8 *data_temp = data;
		unsigned int offset = lcm_op->param.cmd_data.start_id;
		unsigned int len = lcm_op->param.cmd_data.data_len;

		if (IS_ERR_OR_NULL(data_temp) ||
		    size_out < offset + len) {
			DDPMSG("%s: out buffer is not enough, size_out:%u, off:%u, len:%u\n",
				__func__, size_out, offset, len);
			return -ENOMEM;
		}

		LCM_KZALLOC(lcm_op->param.cmd_data.data, len + 1, GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcm_op->param.cmd_data.data)) {
			DDPMSG("%s,%d: failed to allocate data, len:%u\n",
				__func__, __LINE__, len);
			return -ENOMEM;
		}

		ret = mtk_panel_dsi_dcs_read_buffer(
					(struct mipi_dsi_device *)dev,
					lcm_op->param.cmd_data.data,
					lcm_op->size,
					&data[lcm_op->param.cmd_data.start_id],
					lcm_op->param.cmd_data.data_len);
		break;
	}
	case MTK_LCM_CMD_TYPE_WRITE_CMD:
		if (IS_ERR_OR_NULL(lcm_op->param.cmd_data.data) ||
		    lcm_op->param.cmd_data.data_len <= 0)
			return -EINVAL;
		ret = mtk_panel_dsi_dcs_write(
					(struct mipi_dsi_device *)dev,
					lcm_op->param.cmd_data.cmd,
					lcm_op->param.cmd_data.data,
					lcm_op->param.cmd_data.data_len);
		break;
	case MTK_LCM_CMD_TYPE_READ_CMD:
	{
		u8 *data_temp = data;
		unsigned int offset = lcm_op->param.cmd_data.start_id;
		unsigned int len = lcm_op->param.cmd_data.data_len;

		if (IS_ERR_OR_NULL(data_temp) ||
		    size_out < offset + len) {
			DDPMSG("%s: out buffer is not enough, size_out:%u, off:%u, len:%u\n",
				__func__, size_out, offset, len);
			return -ENOMEM;
		}

		LCM_KZALLOC(lcm_op->param.cmd_data.data, len + 1, GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcm_op->param.cmd_data.data)) {
			DDPMSG("%s,%d: failed to allocate data, len:%u\n",
				__func__, __LINE__, len);
			return -ENOMEM;
		}
		ret = mtk_panel_dsi_dcs_read(
					(struct mipi_dsi_device *)dev,
					lcm_op->param.cmd_data.cmd,
					&data[lcm_op->param.cmd_data.start_id],
					lcm_op->param.cmd_data.data_len);
		break;
	}
	default:
		ret = -EINVAL;
		DDPMSG("%s: invalid func:%u\n", __func__, lcm_op->func);
		break;
	}

	return ret;
}

int mtk_execute_func_util(struct mtk_lcm_ops_data *lcm_op)
{
	unsigned int data = 0;
	int ret = 0;

	if (IS_ERR_OR_NULL(lcm_op)) {
		DDPPR_ERR("%s %d: invalid cmd\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	data = lcm_op->param.util_data;

	switch (lcm_op->type) {
	case MTK_LCM_UTIL_TYPE_RESET:
		ret = mtk_drm_gateic_reset(data, lcm_op->func);
		if (ret != 0)
			DDPMSG("%s, reset failed, %d\n",
				__func__, ret);
		break;
	case MTK_LCM_UTIL_TYPE_POWER_ON:
		ret = mtk_drm_gateic_power_on(lcm_op->func);
		if (ret != 0)
			DDPMSG("%s, power on failed, %d\n",
				__func__, ret);
		break;
	case MTK_LCM_UTIL_TYPE_POWER_OFF:
		ret = mtk_drm_gateic_power_off(lcm_op->func);
		if (ret != 0)
			DDPMSG("%s, power off failed, %d\n",
				__func__, ret);
		break;
	case MTK_LCM_UTIL_TYPE_POWER_VOLTAGE:
		ret = mtk_drm_gateic_set_voltage(data, lcm_op->func);
		if (ret != 0)
			DDPMSG("%s, set voltage:%u failed, %d\n",
				__func__, data, ret);
		break;
	case MTK_LCM_UTIL_TYPE_MDELAY:
		mdelay(lcm_op->param.util_data);
		break;
	case MTK_LCM_UTIL_TYPE_UDELAY:
		udelay(lcm_op->param.util_data);
		break;
	default:
		ret = -EINVAL;
		DDPMSG("%s: invalid func:%u\n",
			__func__, lcm_op->func);
		break;
	}

	return ret;
}

int mtk_execute_func_gpio(struct mtk_lcm_ops_data *lcm_op,
		const struct device *device,
		const char **lcm_pinctrl_name,
		unsigned int pinctrl_count)
{
	struct pinctrl *pctrl = NULL;
	struct pinctrl_state *state = NULL;
	struct gpio_desc *gpiod = NULL;
	unsigned int id = 0, data = 0;
	int ret = 0;
	const char *name = NULL;
	struct device dev;

	if (IS_ERR_OR_NULL(lcm_op)) {
		DDPPR_ERR("%s %d: invalid cmd\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	id = lcm_op->param.gpio_data.gpio_id;
	data = lcm_op->param.gpio_data.data;
	if (IS_ERR_OR_NULL(lcm_pinctrl_name) ||
	    id >= pinctrl_count ||
	    IS_ERR_OR_NULL(device)) {
		DDPPR_ERR("%s %d: invalid parameter:%u-%u\n",
			__func__, __LINE__, id, pinctrl_count);
		return -EINVAL;
	}

	memcpy(&dev, device, sizeof(struct device));
	switch (lcm_op->type) {
	case MTK_LCM_GPIO_TYPE_MODE:
		name = lcm_pinctrl_name[id];
		if (IS_ERR_OR_NULL(name)) {
			DDPPR_ERR("%s: invalid pinctrl name:%u\n", __func__, id);
			return -EINVAL;
		}

		pctrl = devm_pinctrl_get(&dev);
		if (IS_ERR_OR_NULL(pctrl)) {
			DDPPR_ERR("%s: invalid pinctrl:%u\n", __func__, id);
			return -EINVAL;
		}

		state = pinctrl_lookup_state(pctrl, name);
		if (IS_ERR_OR_NULL(state)) {
			DDPPR_ERR("%s: invalid state:%u\n", __func__, id);
			return -EINVAL;
		}

		ret = pinctrl_select_state(pctrl, state);
		break;
	case MTK_LCM_GPIO_TYPE_DIR_OUTPUT:
		gpiod = devm_gpiod_get_index(&dev,
			"lcm_gpio_list", id, GPIOD_OUT_HIGH);
		if (IS_ERR_OR_NULL(gpiod)) {
			DDPPR_ERR("%s: invalid gpiod:%u\n", __func__, id);
			return PTR_ERR(gpiod);
		}

		ret = gpiod_direction_output(gpiod, !!data);
		devm_gpiod_put(&dev, gpiod);
		break;
	case MTK_LCM_GPIO_TYPE_DIR_INPUT:
		gpiod = devm_gpiod_get_index(&dev,
			"lcm_gpio_list", id, GPIOD_OUT_HIGH);
		if (IS_ERR_OR_NULL(gpiod)) {
			DDPPR_ERR("%s: invalid gpiod:%u\n", __func__, id);
			return PTR_ERR(gpiod);
		}

		ret = gpiod_direction_input(gpiod);
		devm_gpiod_put(&dev, gpiod);
		break;
	case MTK_LCM_GPIO_TYPE_OUT:
		gpiod = devm_gpiod_get_index(&dev,
			"lcm_gpio_list", id, GPIOD_OUT_HIGH);
		if (IS_ERR_OR_NULL(gpiod)) {
			DDPPR_ERR("%s: invalid gpiod:%u\n", __func__, id);
			return PTR_ERR(gpiod);
		}

		gpiod_set_value(gpiod, !!data);
		devm_gpiod_put(&dev, gpiod);
		break;
	default:
		ret = -EINVAL;
		DDPMSG("%s: invalid type:%u\n",
			__func__, lcm_op->type);
		break;
	}

	return ret;
}

int mtk_is_lcm_read_ops(unsigned int type)
{
	if (type == MTK_LCM_CMD_TYPE_READ_CMD ||
	    type == MTK_LCM_CMD_TYPE_READ_BUFFER)
		return 1;

	return 0;
}
EXPORT_SYMBOL(mtk_is_lcm_read_ops);

int mtk_panel_execute_operation(void *dev,
		struct mtk_lcm_ops_data *table, unsigned int table_size,
		const struct mtk_panel_resource *panel_resource,
		void *data, size_t size, char *owner)
{
	struct mtk_lcm_ops_data *op = NULL;
	unsigned int i = 0;
	char owner_tmp[MTK_LCM_NAME_LENGTH] = { 0 };
	char func_name[MTK_LCM_NAME_LENGTH] = { 0 };
	char type_name[MTK_LCM_NAME_LENGTH] = { 0 };
	int ret = 0;

	if (IS_ERR_OR_NULL(owner))
		snprintf(&owner_tmp[0], MTK_LCM_NAME_LENGTH - 1, "unknown");
	else
		snprintf(&owner_tmp[0], MTK_LCM_NAME_LENGTH - 1, owner);

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(table) ||
	    table_size == 0 ||
	    table_size >= MTK_PANEL_TABLE_OPS_COUNT) {
		DDPPR_ERR("%s: \"%s\" is empty, size:%u\n",
			__func__, owner_tmp, table_size);
		return 0;
	}

	for (i = 0; i < table_size; i++) {
		op = &table[i];
		if (IS_ERR_OR_NULL(op)) {
			DDPMSG("%s: owner:%s, invalid op\n",
				__func__, owner_tmp);
			ret = -EINVAL;
			break;
		}

		mtk_get_func_name(op->func, &func_name[0]);
		if (op->func != MTK_LCM_FUNC_DSI) {
			DDPMSG("%s, %d, not support:%s-%d\n",
				__func__, __LINE__, func_name, op->func);
			return -EINVAL;
		}

		mtk_get_type_name(op->type, &type_name[0]);
#if MTK_LCM_DEBUG_DUMP
		DDPMSG("%s: [%s+%u]: func:%s/%u, type:%s/%u\n",
			__func__, owner_tmp, i,
			func_name, op->func,
			type_name, op->type);
#endif

		if (op->type > MTK_LCM_UTIL_TYPE_START &&
		    op->type < MTK_LCM_UTIL_TYPE_END)
			ret = mtk_execute_func_util(op);
		else if (op->type > MTK_LCM_CMD_TYPE_START &&
		    op->type < MTK_LCM_CMD_TYPE_END)
			ret = mtk_execute_func_cmd(dev, op, (u8 *)data, size);
		else if (op->type > MTK_LCM_GPIO_TYPE_START &&
			op->type < MTK_LCM_GPIO_TYPE_END)
			mtk_execute_func_gpio(op,
					&panel_resource->params.dsi_params.lcm_gpio_dev,
					panel_resource->params.dsi_params.lcm_pinctrl_name,
					panel_resource->params.dsi_params.lcm_pinctrl_count);
		else if (op->type > MTK_LCM_CUST_TYPE_START &&
			op->type < MTK_LCM_CUST_TYPE_END) {
			if (atomic_read(
					&panel_resource->cust.cust_enabled) == 0 ||
				IS_ERR_OR_NULL(panel_resource->cust.func))
				return -EINVAL;

			ret = panel_resource->cust.func(op, (u8 *)data, size);
		} else {
			ret = -EINVAL;
		}

		if (ret != 0) {
			DDPMSG("%s: [%s+%u] func:%s/%u, type:%s/%u, failed:%d\n",
				__func__, owner_tmp, i,
				func_name, op->func,
				type_name, op->type, ret);
			break;
		}
	}

	return ret;
}
EXPORT_SYMBOL(mtk_panel_execute_operation);

void dump_lcm_params_basic(struct mtk_lcm_params *params)
{
	if (IS_ERR_OR_NULL(params)) {
		DDPDUMP("%s: invalid lcm params\n", __func__);
		return;
	}

	DDPDUMP("=========== LCM DUMP: basic params ==============\n");
	DDPDUMP("lcm name=%s, type=%u, resolution=(%u,%u)\n",
		params->name, params->type,
		params->resolution[0], params->resolution[1]);
	DDPDUMP("physical=(%u,%u)\n",
		params->physical_width, params->physical_height);
	DDPDUMP("================================================\n");
}

void mtk_lcm_dump_all(char func, struct mtk_panel_resource *resource,
		struct mtk_panel_cust *cust)
{
	dump_lcm_params_basic(&resource->params);

	switch (func) {
	case MTK_LCM_FUNC_DBI:
		dump_lcm_params_dbi(&resource->params.dbi_params, &resource->cust);
		dump_lcm_ops_dbi(resource->ops.dbi_ops,
				&resource->params.dbi_params, &resource->cust);
		break;
	case MTK_LCM_FUNC_DPI:
		dump_lcm_params_dpi(&resource->params.dpi_params, &resource->cust);
		dump_lcm_ops_dpi(resource->ops.dpi_ops,
				&resource->params.dpi_params, &resource->cust);
		break;
	case MTK_LCM_FUNC_DSI:
		dump_lcm_params_dsi(&resource->params.dsi_params, &resource->cust);
		dump_lcm_ops_dsi(resource->ops.dsi_ops,
				&resource->params.dsi_params, &resource->cust);
		break;
	default:
		DDPDUMP("%s, invalid func:%d\n", __func__, func);
		break;
	}
}
