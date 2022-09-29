// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/slab.h>
#include <linux/of.h>
#include <linux/types.h>
#include "mtk_drm_panel_helper.h"

unsigned long long mtk_lcm_total_size;

/* read u32 array and parsing into u32 buffer */
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
		DDPDBG("%s: %s = %u\n", __func__, prop, *out);
	else if (len > 0)
		DDPDBG("%s: %s array of %d data\n", __func__, prop, len);
	else if (len == 0)
		DDPDBG("%s: %s is empty\n", __func__, prop);
	else
		DDPMSG("%s: %s is not existed or overflow, %d\n",
			__func__, prop, len);
#endif

	return len;
}
EXPORT_SYMBOL(mtk_lcm_dts_read_u32_array);

void mtk_lcm_dts_read_u32(struct device_node *np, char *prop,
		u32 *out)
{
	mtk_lcm_dts_read_u32_array(np, prop, out, 0, 1);
}
EXPORT_SYMBOL(mtk_lcm_dts_read_u32);

void mtk_lcm_dts_read_u8(struct device_node *np, char *prop,
		u8 *out)
{
	int ret = 0;
	u32 data = 0;

	ret = mtk_lcm_dts_read_u32_array(np, prop, &data, 0, 1);
	if (ret == 0)
		*out = (u8)data;
}
EXPORT_SYMBOL(mtk_lcm_dts_read_u8);

/* read u32 array and parsing into u8 buffer */
int mtk_lcm_dts_read_u8_array_from_u32(struct device_node *np, char *prop,
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
	if (data == NULL) {
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
		DDPDBG("%s: %s array of %d data\n", __func__, prop, len);
	} else if (len == 0) {
		DDPDBG("%s: %s is empty\n", __func__, prop);
	} else {
		DDPMSG("%s: %s is not existed or overflow, %d\n",
			__func__, prop, len);
#endif
	}

	LCM_KFREE(data, sizeof(u32) * max_len);
	return len;
}
EXPORT_SYMBOL(mtk_lcm_dts_read_u8_array_from_u32);

/* read u8 array and parsing into u8 buffer */
int mtk_lcm_dts_read_u8_array(struct device_node *np, char *prop,
		u8 *out, int min_len, int max_len)
{
	int len = 0;

	if (IS_ERR_OR_NULL(prop) ||
	    IS_ERR_OR_NULL(np) ||
	    IS_ERR_OR_NULL(out) ||
	    max_len == 0)
		return 0;

	len = of_property_read_variable_u8_array(np,
			prop, out, min_len, max_len);
#if MTK_LCM_DEBUG_DUMP
	if (len == 1) {
		DDPDBG("%s: %s = 0x%x\n", __func__, prop, *out);
	} else if (len > 0) {
		int i = 0;

		DDPMSG("%s: %s array of %d data\n", __func__, prop, len);
		for (i = 0; i < len - 8; i += 8)
			DDPMSG("data%u: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			i, out[i], out[i+1], out[i+2], out[i+3],
			out[i+4], out[i+5], out[i+6], out[i+7]);
	} else if (len == 0) {
		DDPDBG("%s: %s is empty\n", __func__, prop);
	} else {
		DDPMSG("%s: %s is not existed or overflow, %d\n",
			__func__, prop, len);
	}
#endif

	return len;
}
EXPORT_SYMBOL(mtk_lcm_dts_read_u8_array);

int mtk_lcm_dts_read_u32_pointer(struct device_node *np, char *prop,
	u32 **out)
{
	struct property *pp = NULL;
	u32 *data = NULL;
	int len = 0, ret = 0, count = 0;

	if (IS_ERR_OR_NULL(prop) ||
	    IS_ERR_OR_NULL(np))
		return 0;

	pp = of_find_property(np, prop, &len);
	if (pp == NULL || len <= 0) {
		DDPMSG("%s, %d, invalid %s, len%d\n",
			__func__, __LINE__, prop, len);
		*out = NULL;
		return 0;
	}

	LCM_KZALLOC(data, len + sizeof(u32), GFP_KERNEL);
	if (data == NULL) {
		DDPPR_ERR("%s,%d: failed to allocate %s\n",
			__func__, __LINE__, prop);
		*out = NULL;
		return -ENOMEM;
	}

	count = len / sizeof(u32);
	ret = mtk_lcm_dts_read_u32_array(np, prop,
				data, 0, count + 1);
	if (ret <= 0) {
		DDPPR_ERR("%s,%d: failed to parse %s\n",
			__func__, __LINE__, prop);
		LCM_KFREE(data, len + sizeof(u32));
		data = NULL;
		*out = NULL;
		return -EFAULT;
	}

	data[count] = 0;
	*out = data;
	DDPDUMP("%s, %d, %s,%d/%u\n",
		__func__, __LINE__, prop, len, count);
	return count;
}
EXPORT_SYMBOL(mtk_lcm_dts_read_u32_pointer);

int mtk_lcm_dts_read_u8_pointer(struct device_node *np, char *prop,
	u8 **out)
{
	struct property *pp = NULL;
	u8 *data = NULL;
	int len = 0, ret = 0;

	if (IS_ERR_OR_NULL(prop) ||
	    IS_ERR_OR_NULL(np))
		return 0;

	pp = of_find_property(np, prop, &len);
	if (pp == NULL || len <= 0) {
		DDPDBG("%s, %d, invalid %s, len%d\n",
			__func__, __LINE__, prop, len);
		*out = NULL;
		return 0;
	}

	LCM_KZALLOC(data, len + 1, GFP_KERNEL);
	if (data == NULL) {
		DDPPR_ERR("%s,%d: failed to allocate %s\n",
			__func__, __LINE__, prop);
		*out = NULL;
		return -ENOMEM;
	}

	ret = mtk_lcm_dts_read_u8_array(np, prop,
				data, 0, len + 1);
	if (ret <= 0) {
		DDPPR_ERR("%s,%d: failed to parse %s\n",
			__func__, __LINE__, prop);
		LCM_KFREE(data, len + 1);
		data = NULL;
		*out = NULL;
		return -EFAULT;
	}

	data[len] = '\0';
	*out = data;
	DDPDUMP("%s, %d, %s %d\n",
		__func__, __LINE__, prop, len);
	return len;
}
EXPORT_SYMBOL(mtk_lcm_dts_read_u8_pointer);

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

	ret = of_property_read_string(np, "lcm-params-name",
			&params->name);
	DDPMSG("%s, lcm name:%s\n", __func__, params->name);

	mtk_lcm_dts_read_u32(np, "lcm-params-types",
			&params->type);
	mtk_lcm_dts_read_u32_array(np, "lcm-params-resolution",
			&params->resolution[0], 0, 2);
	mtk_lcm_dts_read_u32(np, "lcm-params-physical-width",
			&params->physical_width);
	mtk_lcm_dts_read_u32(np, "lcm-params-physical-height",
			&params->physical_height);
	//dump_lcm_params_basic(params);

	switch (params->type) {
	case MTK_LCM_FUNC_DBI:
		for_each_available_child_of_node(np, type_np) {
			if (of_device_is_compatible(type_np,
					"mediatek,lcm-params-dbi")) {
				ret = parse_lcm_params_dbi(type_np,
						&params->dbi_params);
				if (ret != 0)
					dump_lcm_params_dbi(&params->dbi_params, NULL);
			}
		}
		break;
	case MTK_LCM_FUNC_DPI:
		for_each_available_child_of_node(np, type_np) {
			if (of_device_is_compatible(type_np,
					"mediatek,lcm-params-dpi")) {
				ret = parse_lcm_params_dpi(type_np,
						&params->dpi_params);
				if (ret != 0)
					dump_lcm_params_dpi(&params->dpi_params, NULL);
			}
		}
		break;
	case MTK_LCM_FUNC_DSI:
		for_each_available_child_of_node(np, type_np) {
			if (of_device_is_compatible(type_np,
					"mediatek,lcm-params-dsi")) {
				ret = parse_lcm_params_dsi(type_np,
						&params->dsi_params);
				if (ret != 0)
					dump_lcm_params_dsi(&params->dsi_params, NULL);
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
		/* type size data */
		lcm_op->param.util_data =  (unsigned int)dts[MTK_LCM_DATA_OFFSET];
		break;
	case MTK_LCM_UTIL_TYPE_MDELAY:
	case MTK_LCM_UTIL_TYPE_UDELAY:
		/* type size data */
		lcm_op->param.util_data = (unsigned int)dts[MTK_LCM_DATA_OFFSET];
		break;
	case MTK_LCM_UTIL_TYPE_POWER_VOLTAGE:
		/* type size voltage */
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
		unsigned int len, unsigned int flag_len)
{
	unsigned int i = 0;
	unsigned int flag_off = MTK_LCM_DATA_OFFSET + flag_len;

	if (IS_ERR_OR_NULL(lcm_op) ||
	    IS_ERR_OR_NULL(dts)) {
		DDPMSG("%s,%d: invalid parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	switch (lcm_op->type) {
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER:
		/* type size flag3 flag2 flag1 flag0 data0 data1 ... */
		for (i = 0; i < flag_len; i++)
			lcm_op->param.buf_data.flag |=
					(dts[MTK_LCM_DATA_OFFSET + i] <<
						((flag_len - i - 1) * 8));
		lcm_op->param.buf_data.data_len = lcm_op->size -
				flag_len;
		LCM_KZALLOC(lcm_op->param.buf_data.data,
				lcm_op->param.buf_data.data_len + 1, GFP_KERNEL);
		if (lcm_op->param.buf_data.data == NULL) {
			DDPPR_ERR("%s,%d: failed to allocate data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		memcpy(lcm_op->param.buf_data.data,
			dts + flag_off, lcm_op->param.buf_data.data_len);
		*(lcm_op->param.buf_data.data + lcm_op->param.buf_data.data_len) = '\0';
		break;
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER_CONDITION:
		/* type size flag3 flag2 flag1 flag0 condition_name condition data0 data1 ... */
		for (i = 0; i < flag_len; i++)
			lcm_op->param.buf_con_data.flag |=
					(dts[MTK_LCM_DATA_OFFSET + i] <<
						((flag_len - i - 1) * 8));
		lcm_op->param.buf_con_data.name = dts[flag_off];
		lcm_op->param.buf_con_data.condition = dts[flag_off + 1];
		lcm_op->param.buf_con_data.data_len = lcm_op->size -
				flag_len - 2;
		LCM_KZALLOC(lcm_op->param.buf_con_data.data,
				lcm_op->param.buf_con_data.data_len + 1, GFP_KERNEL);
		if (lcm_op->param.buf_con_data.data == NULL) {
			DDPPR_ERR("%s,%d: failed to allocate data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		memcpy(lcm_op->param.buf_con_data.data,
			dts + flag_off + 2,
			lcm_op->param.buf_con_data.data_len);
		*(lcm_op->param.buf_con_data.data +
			lcm_op->param.buf_con_data.data_len) = '\0';
#if MTK_LCM_DEBUG_DUMP
		DDPMSG("%s, condition:%u-%u,data:%u,flag:0x%x\n", __func__,
			lcm_op->param.buf_con_data.name,
			lcm_op->param.buf_con_data.condition,
			(unsigned int)lcm_op->param.buf_con_data.data_len,
			lcm_op->param.buf_con_data.flag);
#endif
		break;
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER_RUNTIME_INPUT:
		/* type size flag3 flag2 flag1 flag0 input_name
		 * id_count id0, id1 ... data_count data0 data1 ...
		 */
		for (i = 0; i < flag_len; i++)
			lcm_op->param.buf_runtime_data.flag |=
					(dts[MTK_LCM_DATA_OFFSET + i] <<
						((flag_len - i - 1) * 8));
		lcm_op->param.buf_runtime_data.name = dts[flag_off];
		lcm_op->param.buf_runtime_data.id_len = dts[flag_off + 1];
		LCM_KZALLOC(lcm_op->param.buf_runtime_data.id,
			lcm_op->param.buf_runtime_data.id_len + 1, GFP_KERNEL);
		if (lcm_op->param.buf_runtime_data.id == NULL) {
			DDPPR_ERR("%s,%d: failed to allocate id, %u\n",
				__func__, __LINE__, lcm_op->param.buf_runtime_data.id_len);
			return -ENOMEM;
		}

		memcpy(lcm_op->param.buf_runtime_data.id,
			dts + flag_off + 2,
			lcm_op->param.buf_runtime_data.id_len);
		*(lcm_op->param.buf_runtime_data.id +
				lcm_op->param.buf_runtime_data.id_len) = '\0';

		lcm_op->param.buf_runtime_data.data_len = dts[flag_off +
			lcm_op->param.buf_runtime_data.id_len + 2];
		LCM_KZALLOC(lcm_op->param.buf_runtime_data.data,
			lcm_op->param.buf_runtime_data.data_len + 1, GFP_KERNEL);
		if (lcm_op->param.buf_runtime_data.data == NULL) {
			DDPPR_ERR("%s,%d: failed to allocate data, %u\n",
				__func__, __LINE__, lcm_op->param.buf_runtime_data.data_len);
			return -ENOMEM;
		}
		memcpy(lcm_op->param.buf_runtime_data.data,
			dts + flag_off +
			lcm_op->param.buf_runtime_data.id_len + 3,
			lcm_op->param.buf_runtime_data.data_len);
		*(lcm_op->param.buf_runtime_data.data +
				lcm_op->param.buf_runtime_data.data_len) = '\0';
		break;
	case MTK_LCM_CMD_TYPE_WRITE_CMD:
		/* type size flag3 flag2 flag1 flag0 cmd data0 data1 ... */
		for (i = 0; i < flag_len; i++)
			lcm_op->param.cmd_data.flag |=
					(dts[MTK_LCM_DATA_OFFSET + i] <<
						((flag_len - i - 1) * 8));
		lcm_op->param.cmd_data.cmd = dts[flag_off];
		lcm_op->param.cmd_data.tx_len = lcm_op->size - flag_len - 1;
		LCM_KZALLOC(lcm_op->param.cmd_data.tx_data,
				lcm_op->param.cmd_data.tx_len + 1, GFP_KERNEL);
		if (lcm_op->param.cmd_data.tx_data == NULL) {
			DDPPR_ERR("%s,%d: failed to allocate data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		memcpy(lcm_op->param.cmd_data.tx_data,
			dts + flag_off + 1,
			lcm_op->param.cmd_data.tx_len);
		*(lcm_op->param.cmd_data.tx_data +
				lcm_op->param.cmd_data.tx_len) = '\0';
		break;
	case MTK_LCM_CMD_TYPE_READ_BUFFER:
		/* type size flag3 flag2 flag1 flag0 rx_off rx_len data0 data1 ... */
		for (i = 0; i < flag_len; i++)
			lcm_op->param.cmd_data.flag |=
					(dts[MTK_LCM_DATA_OFFSET + i] <<
						((flag_len - i - 1) * 8));
		lcm_op->param.cmd_data.rx_off = dts[flag_off];
		lcm_op->param.cmd_data.rx_len = dts[flag_off + 1];
		lcm_op->param.cmd_data.tx_len =
				lcm_op->size - flag_len - 2;
		LCM_KZALLOC(lcm_op->param.cmd_data.tx_data,
				lcm_op->param.cmd_data.tx_len + 1, GFP_KERNEL);
		if (lcm_op->param.cmd_data.tx_data == NULL) {
			DDPPR_ERR("%s,%d: failed to allocate data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		memcpy(lcm_op->param.cmd_data.tx_data,
			dts + flag_off + 2,
			lcm_op->param.cmd_data.tx_len);
		*(lcm_op->param.cmd_data.tx_data +
				lcm_op->param.cmd_data.tx_len) = '\0';
		break;
	case MTK_LCM_CMD_TYPE_READ_CMD:
		/* type size flag3 flag2 flag1 flag0 rx_off rx_len cmd */
		for (i = 0; i < flag_len; i++)
			lcm_op->param.cmd_data.flag |=
					(dts[MTK_LCM_DATA_OFFSET + i] <<
						((flag_len - i - 1) * 8));
		lcm_op->param.cmd_data.rx_off = dts[flag_off];
		lcm_op->param.cmd_data.rx_len = dts[flag_off + 1];
		lcm_op->param.cmd_data.cmd = dts[flag_off + 2];
		lcm_op->param.cmd_data.tx_len = 0;
		lcm_op->param.cmd_data.tx_data = NULL;
		break;
	default:
		DDPPR_ERR("%s %d: invalid type:%d\n",
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
		DDPPR_ERR("%s %d: invalid type:%d\n",
			__func__, __LINE__, lcm_op->type);
		return -EINVAL;
	}
	return 0;
}

static int parse_lcm_ops_func_cust(struct mtk_lcm_ops_data *lcm_op,
		u8 *dts, const struct mtk_panel_cust *cust, unsigned int flag_len)
{
	if (IS_ERR_OR_NULL(lcm_op) ||
	    IS_ERR_OR_NULL(dts) || IS_ERR_OR_NULL(cust) ||
	    IS_ERR_OR_NULL(cust->parse_ops))
		return -EINVAL;

	cust->parse_ops(lcm_op, dts, flag_len);

	return 0;
}

static int parse_lcm_ops_basic(struct mtk_lcm_ops_data *lcm_op, u8 *dts,
		const struct mtk_panel_cust *cust,
		unsigned int len, unsigned int flag_len)
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
		ret = parse_lcm_ops_func_cmd(lcm_op, dts, len, flag_len);
	else if (lcm_op->type > MTK_LCM_LK_TYPE_START &&
	    lcm_op->type < MTK_LCM_LK_TYPE_END)
		ret = parse_lcm_ops_func_cmd(lcm_op, dts, len, flag_len);
	else if (lcm_op->type > MTK_LCM_GPIO_TYPE_START &&
	    lcm_op->type < MTK_LCM_GPIO_TYPE_END)
		ret = parse_lcm_ops_func_gpio(lcm_op, dts, len);
	else if (lcm_op->type > MTK_LCM_CUST_TYPE_START &&
	    lcm_op->type < MTK_LCM_CUST_TYPE_END) {
		DDPDBG("%s, cust ops:%d\n", __func__, lcm_op->type);
		ret = parse_lcm_ops_func_cust(lcm_op,
				dts, cust, flag_len);
	}
	else {
		DDPPR_ERR("%s %d: invalid type:0x%x\n",
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

int parse_lcm_ops_func(struct device_node *np,
		struct mtk_lcm_ops_table *table, char *func,
		unsigned int flag_len, unsigned int panel_type,
		const struct mtk_panel_cust *cust, unsigned int phase)
{
	u8 *table_dts_buf = NULL;
	int table_dts_size = 0;
	unsigned int i = 0, skip_count = 0;
	u8 *tmp;
	int len = 0, tmp_len = 0, ret = 0;
	int phase_skip_flag = 0;
	struct mtk_lcm_ops_data *op = NULL;

	if (IS_ERR_OR_NULL(func) ||
	    IS_ERR_OR_NULL(table) ||
	    panel_type >= MTK_LCM_FUNC_END) {
		DDPMSG("%s:%d: invalid parameters\n", __func__, __LINE__);
		return -EFAULT;
	}

	len = mtk_lcm_dts_read_u8_pointer(np,
				func, &table_dts_buf);
	if (len <= 0) {
#if MTK_LCM_DEBUG_DUMP
		DDPMSG("%s, failed to get table dts, len:%d\n",
			__func__, len);
#endif
		return 0;
	}

	table_dts_size = len + 1;
	DDPDUMP("%s: start to parse:%s, len:%u\n",
		__func__, func, len);

	INIT_LIST_HEAD(&table->list);
	tmp = &table_dts_buf[0];
	while (len > MTK_LCM_DATA_OFFSET) {
		LCM_KZALLOC(op, sizeof(struct mtk_lcm_ops_data), GFP_KERNEL);
		if (op == NULL) {
			DDPPR_ERR("%s, %d, failed to allocate op\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto end;
		}

		op->func = panel_type;
		op->type = tmp[0];
		if (op->type == MTK_LCM_TYPE_END) {
			DDPINFO("%s: parsing end of %s, len:%d\n", __func__, func, len);
			LCM_KFREE(op, sizeof(struct mtk_lcm_ops_data));
			len = 0;
			break;
		}

		op->size = tmp[1];
		tmp_len = MTK_LCM_DATA_OFFSET + op->size;
		phase_skip_flag = parse_lcm_ops_check(op,
					tmp, len, phase_skip_flag, phase);

		if (phase_skip_flag == 0 &&
		    op->type != MTK_LCM_PHASE_TYPE_START &&
		    op->type != MTK_LCM_PHASE_TYPE_END) {
			ret = parse_lcm_ops_basic(op, tmp, cust, len, flag_len);
#if MTK_LCM_DEBUG_DUMP
			if (op->type > MTK_LCM_CUST_TYPE_START &&
			    op->type < MTK_LCM_CUST_TYPE_END)
				DDPMSG(
					"[%s+%d] >>>func:%u,type:%u,size:%u,dts:%u,op:%u,ret:%d,phase:0x%x,skip:%d\n",
					func, i, op->func, op->type,
					op->size, len, tmp_len, ret,
					phase, phase_skip_flag);
#endif
			if (ret < 0) {
				DDPMSG(
					"[%s+%d] >>>func:%u,type:%u,size:%u,dts:%u,fail:%d,flag:%u\n",
					func, i, op->func, op->type,
					op->size, len, ret, flag_len);
				LCM_KFREE(op, sizeof(struct mtk_lcm_ops_data));
				goto end;
			}
			list_add_tail(&op->node, &table->list);
			table->size++;
		} else {
#if MTK_LCM_DEBUG_DUMP
			if (op->type > MTK_LCM_CUST_TYPE_START &&
			    op->type < MTK_LCM_CUST_TYPE_END)
				DDPMSG("[%s+%d] >>>func:%u,type:%u skipped:0x%x,phase:0x%x\n",
					func, i, op->func, op->type,
					phase_skip_flag, phase);
#endif
			LCM_KFREE(op, sizeof(struct mtk_lcm_ops_data));
			skip_count++;
		}

		if (tmp_len <= len) {
			tmp = tmp + tmp_len;
			len = len - tmp_len;
		} else {
			DDPMSG("%s: parsing warning of %s, len:%d, tmp:%d\n",
				__func__, func, len, tmp_len);
			break;
		}
		i++;
	}

	if (len >= MTK_LCM_DATA_OFFSET)
		DDPMSG("%s:%d: %s, total:%u,parsing:%u,skip:%u,last_dts:%u,last_ops%u\n",
			__func__, __LINE__, func, i, table->size,
			skip_count, len, tmp_len);
	ret = table->size;

end:
	LCM_KFREE(table_dts_buf, table_dts_size);
	table_dts_buf = NULL;
	return ret;
}
EXPORT_SYMBOL(parse_lcm_ops_func);

int parse_lcm_common_ops_func(struct device_node *np,
		void **list, unsigned int *list_len, char *list_name,
		struct mtk_lcm_ops_table *table, char *func,
		unsigned int flag_len, unsigned int panel_type,
		const struct mtk_panel_cust *cust, unsigned int phase,
		unsigned int list_unit)
{
	int ret = 0;

	if (np == NULL || list_name == NULL ||
		table == NULL || func == NULL ||
		list == NULL || list_len == NULL ||
		list_unit == 0)
		return -EINVAL;

	/* parse mode list */
	switch (list_unit) {
	case 1:
	{
		ret = mtk_lcm_dts_read_u8_pointer(np, list_name,
				(u8 **)list);
		if (ret < 0) {
			DDPMSG("%s, %d, failed to parse %s, %u\n",
				__func__, __LINE__, list_name, ret);
			*list_len = 0;
			return -EFAULT;
		}
		*list_len = ret;
		break;
	}
	case 4:
	{
		ret = mtk_lcm_dts_read_u32_pointer(np, list_name,
				(u32 **)list);
		if (ret < 0) {
			DDPMSG("%s, %d, failed to parse %s, %u\n",
				__func__, __LINE__, list_name, ret);
			*list_len = 0;
			return -EFAULT;
		}
		*list_len = ret;
		break;
	}
	default:
		DDPMSG("%s, %d, not support unit:%u\n",
			__func__, __LINE__, list_unit);
		ret = -EINVAL;
		break;
	}

	/* parse ops table*/
	ret = parse_lcm_ops_func(np, table,
			func, flag_len, MTK_LCM_FUNC_DSI,
			cust, MTK_LCM_PHASE_KERNEL);
	if (ret < 0)
		DDPPR_ERR("%s, %d invalid %s,ret:%d\n",
			__func__, __LINE__, func, ret);
	DDPDUMP("%s, %d, func:%s-%u,list:%s-%u\n",
		__func__, __LINE__,
		func, ret, *list_name, *list_len);

	return 0;
}

int parse_lcm_common_ops_func_u8(struct device_node *np,
		u8 **list, unsigned int *list_len, char *list_name,
		struct mtk_lcm_ops_table *table, char *func,
		unsigned int flag_len, unsigned int panel_type,
		const struct mtk_panel_cust *cust, unsigned int phase)
{
	return parse_lcm_common_ops_func(np, (void **)list,
				list_len, list_name, table, func, flag_len,
				panel_type, cust, phase, 1);
}
EXPORT_SYMBOL(parse_lcm_common_ops_func_u8);

int parse_lcm_common_ops_func_u32(struct device_node *np,
		u32 **list, unsigned int *list_len, char *list_name,
		struct mtk_lcm_ops_table *table, char *func,
		unsigned int flag_len, unsigned int panel_type,
		const struct mtk_panel_cust *cust, unsigned int phase)
{
	return parse_lcm_common_ops_func(np, (void **)list,
				list_len, list_name, table, func, flag_len,
				panel_type, cust, phase, 4);
}
EXPORT_SYMBOL(parse_lcm_common_ops_func_u32);

static int parse_lcm_ops_dt_node(struct device_node *np,
		struct mtk_lcm_ops *ops, struct mtk_lcm_params *params,
		const struct mtk_panel_cust *cust)
{
	struct device_node *type_np = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(ops) ||
	    IS_ERR_OR_NULL(params) ||
	    IS_ERR_OR_NULL(np)) {
		DDPPR_ERR("%s: invalid lcm ops\n", __func__);
		return -ENOMEM;
	}

	switch (params->type) {
	case MTK_LCM_FUNC_DBI:
		for_each_available_child_of_node(np, type_np) {
			if (of_device_is_compatible(type_np,
					"mediatek,lcm-ops-dbi")) {
				LCM_KZALLOC(ops->dbi_ops,
					sizeof(struct mtk_lcm_ops_dbi), GFP_KERNEL);
				if (ops->dbi_ops == NULL) {
					DDPMSG("%s, %d, failed\n", __func__, __LINE__);
					return -ENOMEM;
				}

				DDPMSG("%s, LCM parse dbi params\n", __func__);
				ret = parse_lcm_ops_dbi(type_np,
						ops->dbi_ops, &params->dbi_params, cust);
				if (ret != 0)
					dump_lcm_ops_dbi(ops->dbi_ops, &params->dbi_params, NULL);
			}
		}
		break;
	case MTK_LCM_FUNC_DPI:
		for_each_available_child_of_node(np, type_np) {
			if (of_device_is_compatible(type_np,
					"mediatek,lcm-ops-dpi")) {
				LCM_KZALLOC(ops->dpi_ops,
					sizeof(struct mtk_lcm_ops_dpi), GFP_KERNEL);
				if (ops->dpi_ops == NULL) {
					DDPMSG("%s, %d, failed\n", __func__, __LINE__);
					return -ENOMEM;
				}
				DDPMSG("%s, LCM parse dpi params\n", __func__);
				ret = parse_lcm_ops_dpi(type_np,
						ops->dpi_ops, &params->dpi_params, cust);
				if (ret != 0)
					dump_lcm_ops_dpi(ops->dpi_ops, &params->dpi_params, NULL);
			}
		}
		break;
	case MTK_LCM_FUNC_DSI:
		for_each_available_child_of_node(np, type_np) {
			if (of_device_is_compatible(type_np,
					"mediatek,lcm-ops-dsi")) {
				LCM_KZALLOC(ops->dsi_ops,
					sizeof(struct mtk_lcm_ops_dsi), GFP_KERNEL);
				if (ops->dsi_ops == NULL) {
					DDPMSG("%s, %d, failed\n", __func__, __LINE__);
					return -ENOMEM;
				}
				ret = parse_lcm_ops_dsi(type_np,
						ops->dsi_ops, &params->dsi_params, cust);
				if (ret != 0)
					dump_lcm_ops_dsi(ops->dsi_ops, &params->dsi_params, NULL);
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

	mtk_lcm_dts_read_u32(lcm_np, "lcm-version", &data->version);

	/* Load LCM parameters from DT */
	for_each_available_child_of_node(lcm_np, np) {
		if (of_device_is_compatible(np, "mediatek,lcm-params")) {
			ret = parse_lcm_params_dt_node(np, &data->params);
			if (ret < 0)
				return ret;
			DDPMSG("%s: parsing lcm-params, total_size:%lluByte\n",
				__func__, mtk_lcm_total_size);

			if (data->cust != NULL &&
			    data->cust->parse_params != NULL) {
				DDPMSG("%s: parsing cust params, func:0x%lx\n",
					__func__,
					(unsigned long)data->cust->parse_params);
				ret = data->cust->parse_params(np);
				if (ret < 0)
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
				&data->params, data->cust);
			if (ret < 0) {
				DDPMSG("%s, failed to parse operations, %d\n", __func__, ret);
				return ret;
			}

			if (data->cust != NULL &&
				data->cust->parse_ops_table != NULL) {
				DDPMSG("%s: parsing cust ops table, func:0x%lx\n",
					__func__,
					(unsigned long)data->cust->parse_ops_table);
				switch (data->params.type) {
				case MTK_LCM_FUNC_DBI:
					data->cust->parse_ops_table(np,
						data->ops.dbi_ops->flag_len);
					break;
				case MTK_LCM_FUNC_DPI:
					data->cust->parse_ops_table(np,
						data->ops.dpi_ops->flag_len);
					break;
				case MTK_LCM_FUNC_DSI:
					data->cust->parse_ops_table(np,
						data->ops.dsi_ops->flag_len);
					break;
				default:
					break;
				}
			}

			DDPMSG("%s: parsing lcm-ops, total_size:%lluByte\n",
				__func__, mtk_lcm_total_size);
		}
	}

#if MTK_LCM_DEBUG_DUMP
	mtk_lcm_dump_all(MTK_LCM_FUNC_DSI, data);
#endif
	return 0;
}
EXPORT_SYMBOL(load_panel_resource_from_dts);

static void mtk_get_func_name(char func, char *out)
{
	char name[MTK_LCM_NAME_LENGTH] = { 0 };
	int ret = 0;

	if (IS_ERR_OR_NULL(out)) {
		DDPMSG("%s: invalid out buffer\n", __func__);
		return;
	}

	switch (func) {
	case MTK_LCM_FUNC_DBI:
		ret = snprintf(&name[0], MTK_LCM_NAME_LENGTH - 1, "DBI");
		break;
	case MTK_LCM_FUNC_DPI:
		ret = snprintf(&name[0], MTK_LCM_NAME_LENGTH - 1, "DPI");
		break;
	case MTK_LCM_FUNC_DSI:
		ret = snprintf(&name[0], MTK_LCM_NAME_LENGTH - 1, "DSI");
		break;
	default:
		ret = snprintf(&name[0], MTK_LCM_NAME_LENGTH - 1, "unknown");
		break;
	}
	if (ret < 0 || ret >= MTK_LCM_NAME_LENGTH)
		DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);

	ret = snprintf(out, MTK_LCM_NAME_LENGTH - 1, &name[0]);
	if (ret < 0 || ret >= MTK_LCM_NAME_LENGTH)
		DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
}

static void mtk_get_type_name(unsigned int type, char *out)
{
	char name[MTK_LCM_NAME_LENGTH] = { 0 };
	int ret = 0;

	if (IS_ERR_OR_NULL(out)) {
		DDPMSG("%s: invalid out buffer\n", __func__);
		return;
	}

	switch (type) {
	case MTK_LCM_UTIL_TYPE_RESET:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "RESET");
		break;
	case MTK_LCM_UTIL_TYPE_MDELAY:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "MDELAY");
		break;
	case MTK_LCM_UTIL_TYPE_UDELAY:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "UDELAY");
		break;
	case MTK_LCM_UTIL_TYPE_TDELAY:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "TICK_DELAY");
		break;
	case MTK_LCM_UTIL_TYPE_POWER_VOLTAGE:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1,	"POWER_VOLTAGE");
		break;
	case MTK_LCM_UTIL_TYPE_POWER_ON:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "POWER_ON");
		break;
	case MTK_LCM_UTIL_TYPE_POWER_OFF:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1,	"POWER_OFF");
		break;
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "WRITE_BUF");
		break;
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER_CONDITION:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "WRITE_BUF_CON");
		break;
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER_RUNTIME_INPUT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "WRITE_BUF_RUNTIME");
		break;
	case MTK_LCM_CMD_TYPE_WRITE_CMD:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "WRITE_CMD");
		break;
	case MTK_LCM_CMD_TYPE_READ_BUFFER:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "READ_BUF");
		break;
	case MTK_LCM_CMD_TYPE_READ_CMD:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "READ_CMD");
		break;
	case MTK_LCM_GPIO_TYPE_MODE:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "GPIO_MODE");
		break;
	case MTK_LCM_GPIO_TYPE_DIR_OUTPUT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "GPIO_DIR_OUT");
		break;
	case MTK_LCM_GPIO_TYPE_DIR_INPUT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "GPIO_DIR_IN");
		break;
	case MTK_LCM_GPIO_TYPE_OUT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "GPIO_OUT");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_COUNT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_COUNT");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_PARAM");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_FIX_BIT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_FIX");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_X0_MSB_BIT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_X0_MSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_X0_LSB_BIT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_X0_LSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_X1_MSB_BIT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_X1_MSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_X1_LSB_BIT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_X1_LSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_Y0_MSB_BIT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_Y0_MSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_Y0_LSB_BIT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_Y0_LSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_Y1_MSB_BIT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_Y1_MSB");
		break;
	case MTK_LCM_LK_TYPE_PREPARE_PARAM_Y1_LSB_BIT:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_PREPARE_Y1_LSB");
		break;
	case MTK_LCM_LK_TYPE_WRITE_PARAM:
		ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1, "LK_WRITE_PARAM");
		break;
	default:
		if (type > MTK_LCM_CUST_TYPE_START &&
		    type < MTK_LCM_CUST_TYPE_END)
			ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1,
				"CUST-%d", type);
		else
			ret = snprintf(name, MTK_LCM_NAME_LENGTH - 1,
				"unknown");
		break;
	}
	if (ret < 0 || ret >= MTK_LCM_NAME_LENGTH)
		DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);

	ret = snprintf(out, MTK_LCM_NAME_LENGTH - 1, &name[0]);
	if (ret < 0 || ret >= MTK_LCM_NAME_LENGTH)
		DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
}

void mtk_lcm_dump_u8_array(u8 *buf, unsigned int size, const char *name)
{
	unsigned int i = 0;
	int remain = 0;

	if (size == 0 || IS_ERR_OR_NULL(buf) ||
		name == NULL)
		return;

	if (size >= 8) {
		for (i = 0; i <= size - 8; i += 8)
			DDPDUMP("%s[%u], 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
				name, i, buf[i], buf[i + 1], buf[i + 2], buf[i + 3],
				buf[i + 4], buf[i + 5], buf[i + 6], buf[i + 7]);
	}

	remain = size - i;
	if (remain == 0)
		return;

	DDPDUMP("    %s[%u], 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		name, i, remain > 0 ? buf[i] : 0x0,
		remain > 1 ? buf[i + 1] : 0x0,
		remain > 2 ? buf[i + 2] : 0x0,
		remain > 3 ? buf[i + 3] : 0x0,
		remain > 4 ? buf[i + 4] : 0x0,
		remain > 5 ? buf[i + 5] : 0x0,
		remain > 6 ? buf[i + 6] : 0x0,
		remain > 7 ? buf[i + 7] : 0x0);
}
EXPORT_SYMBOL(mtk_lcm_dump_u8_array);

void mtk_lcm_dump_u32_array(u32 *buf, unsigned int size, const char *name)
{
	unsigned int i = 0;
	int remain = 0;

	if (size == 0 || IS_ERR_OR_NULL(buf) ||
		name == NULL)
		return;

	if (size >= 8) {
		for (i = 0; i <= size - 8; i += 8)
			DDPDUMP("%s[%u], 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
				name, i, buf[i], buf[i + 1], buf[i + 2], buf[i + 3],
				buf[i + 4], buf[i + 5], buf[i + 6], buf[i + 7]);
	}

	remain = size - i;
	if (remain == 0)
		return;

	DDPDUMP("%s[%u], 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
		name, i, remain > 0 ? buf[i] : 0x0,
		remain > 1 ? buf[i + 1] : 0x0,
		remain > 2 ? buf[i + 2] : 0x0,
		remain > 3 ? buf[i + 3] : 0x0,
		remain > 4 ? buf[i + 4] : 0x0,
		remain > 5 ? buf[i + 5] : 0x0,
		remain > 6 ? buf[i + 6] : 0x0,
		remain > 7 ? buf[i + 7] : 0x0);
}
EXPORT_SYMBOL(mtk_lcm_dump_u32_array);

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

	if (IS_ERR_OR_NULL(lcm_op) || IS_ERR_OR_NULL(owner))
		return;

	mtk_get_func_name(lcm_op->func, &func_name[0]);
	mtk_get_type_name(lcm_op->type, &type_name[0]);
	switch (lcm_op->type) {
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER:
		DDPDUMP("[%s-%u]:%s,%s,dts:%u,cnt:%u,flag:0x%x\n",
			owner, id, func_name, type_name, lcm_op->size,
			(unsigned int)lcm_op->param.buf_data.data_len,
			lcm_op->param.buf_data.flag);
		mtk_lcm_dump_u8_array(lcm_op->param.buf_data.data,
				lcm_op->size, "   data");
		break;
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER_CONDITION:
		DDPDUMP("[%s-%u]:%s,%s,dts:%u,name:0x%x,con:0x%x,cnt:%u,flag:0x%x\n",
			owner, id, func_name, type_name, lcm_op->size,
			lcm_op->param.buf_con_data.name,
			lcm_op->param.buf_con_data.condition,
			(unsigned int)lcm_op->param.buf_con_data.data_len,
			lcm_op->param.buf_con_data.flag);
		mtk_lcm_dump_u8_array(lcm_op->param.buf_con_data.data,
				lcm_op->param.buf_con_data.data_len, "   data");
		break;
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER_RUNTIME_INPUT:
		DDPDUMP("[%s-%u]:%s,%s, dts:%u,id_cnt:%u,data_cnt:%u,flag:0x%x\n",
			owner, id, func_name, type_name,
			lcm_op->size, lcm_op->param.buf_runtime_data.id_len,
			(unsigned int)lcm_op->param.buf_runtime_data.data_len,
			lcm_op->param.buf_runtime_data.flag);
		mtk_lcm_dump_u8_array(lcm_op->param.buf_runtime_data.id,
				lcm_op->param.buf_runtime_data.id_len, "   data");
		mtk_lcm_dump_u8_array(lcm_op->param.buf_runtime_data.data,
				lcm_op->param.buf_runtime_data.data_len, "   data");
		break;
	case MTK_LCM_CMD_TYPE_WRITE_CMD:
		DDPDUMP("[%s-%u]:%s,%s,dts:%u,cmd:0x%x,cnt:%u,flag:0x%x\n",
			owner, id, func_name, type_name, lcm_op->size,
			lcm_op->param.cmd_data.cmd,
			(unsigned int)lcm_op->param.cmd_data.tx_len,
			lcm_op->param.cmd_data.flag);
			mtk_lcm_dump_u8_array(lcm_op->param.cmd_data.tx_data,
					lcm_op->param.cmd_data.tx_len, "   data");
		break;
	case MTK_LCM_CMD_TYPE_READ_BUFFER:
		DDPDUMP("[%s-%u]:%s,%s,dts:%u,cnt:%u,startid:%u,rx:%u,flag:0x%x\n",
			owner, id, func_name, type_name, lcm_op->size,
			(unsigned int)lcm_op->param.cmd_data.tx_len,
			lcm_op->param.cmd_data.rx_off,
			lcm_op->param.cmd_data.rx_len,
			lcm_op->param.cmd_data.flag);
			mtk_lcm_dump_u8_array(lcm_op->param.cmd_data.tx_data,
					lcm_op->param.cmd_data.tx_len, "   data");
		break;
	case MTK_LCM_CMD_TYPE_READ_CMD:
		DDPDUMP("[%s-%u]:%s,%s,dts:%u,cmd:0x%x,cnt:%u,startid:%u,flag:0x%x\n",
			owner, id, func_name, type_name, lcm_op->size,
			lcm_op->param.cmd_data.cmd,
			(unsigned int)lcm_op->param.cmd_data.rx_len,
			lcm_op->param.cmd_data.rx_off,
			lcm_op->param.cmd_data.flag);
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

int dump_lcm_ops_func(struct mtk_lcm_ops_data *lcm_op,
		const struct mtk_panel_cust *cust, unsigned int id, const char *owner)
{
	if (IS_ERR_OR_NULL(lcm_op))
		return -EINVAL;

	if (lcm_op->type > MTK_LCM_UTIL_TYPE_START &&
	    lcm_op->type < MTK_LCM_UTIL_TYPE_END)
		dump_lcm_ops_func_util(lcm_op, owner, id);
	else if (lcm_op->type > MTK_LCM_CMD_TYPE_START &&
	    lcm_op->type < MTK_LCM_CMD_TYPE_END)
		dump_lcm_ops_func_cmd(lcm_op, owner, id);
	else if (lcm_op->type > MTK_LCM_GPIO_TYPE_START &&
	    lcm_op->type < MTK_LCM_GPIO_TYPE_END)
		dump_lcm_ops_func_gpio(lcm_op, owner, id);
	else if (lcm_op->type > MTK_LCM_CUST_TYPE_START &&
	    lcm_op->type < MTK_LCM_CUST_TYPE_END) {
		if (cust != NULL &&
		    cust->dump_ops != NULL)
			cust->dump_ops(lcm_op, owner, id);
	} else
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(dump_lcm_ops_func);

void dump_lcm_ops_table(struct mtk_lcm_ops_table *table,
		const struct mtk_panel_cust *cust,
		const char *owner)
{
	struct mtk_lcm_ops_data *lcm_op = NULL;
	char owner_tmp[MTK_LCM_NAME_LENGTH] = { 0 };
	unsigned int i = 0;
	int ret = 0;

	if (IS_ERR_OR_NULL(owner))
		ret = snprintf(&owner_tmp[0], MTK_LCM_NAME_LENGTH - 1, "unknown");
	else
		ret = snprintf(&owner_tmp[0], MTK_LCM_NAME_LENGTH - 1, owner);
	if (ret < 0 || ret >= MTK_LCM_NAME_LENGTH)
		DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);

	if (IS_ERR_OR_NULL(table) || table->size == 0) {
		DDPDUMP("%s: \"%s\" is empty, size:%u\n",
			__func__, owner_tmp, table->size);
		return;
	}

	DDPDUMP("-------------%s(%u): 0x%lx-----------\n",
		owner_tmp, table->size, (unsigned long)table);
	list_for_each_entry(lcm_op, &table->list, node) {
		ret = dump_lcm_ops_func(lcm_op, cust, i, owner);
		if (ret < 0)
			DDPDUMP("[%s-%d]: invalid func:%u\n",
				owner, i, lcm_op->func);
		i++;
	}
}
EXPORT_SYMBOL(dump_lcm_ops_table);

int mtk_lcm_create_input_packet(struct mtk_lcm_ops_input_packet *input,
		unsigned int data_count, unsigned int condition_count)
{
	if (IS_ERR_OR_NULL(input))
		return -EINVAL;

	if (condition_count > 0) {
		LCM_KZALLOC(input->condition,
			sizeof(struct mtk_lcm_ops_input) *
			condition_count, GFP_KERNEL);
		if (input->condition == NULL) {
			DDPMSG("%s, %d, failed to allocate condition\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
	} else {
		input->condition = NULL;
	}
	input->condition_count = condition_count;

	if (data_count > 0) {
		LCM_KZALLOC(input->data,
			sizeof(struct mtk_lcm_ops_input) *
			data_count, GFP_KERNEL);
		if (input->data == NULL) {
			DDPMSG("%s, %d, failed to allocate data\n",
				__func__, __LINE__);
			if (condition_count > 0) {
				LCM_KFREE(input->condition,
					sizeof(struct mtk_lcm_ops_input) *
					condition_count);
				input->condition_count = 0;
			}
			return -ENOMEM;
		}
	} else {
		input->data = NULL;
	}
	input->data_count = data_count;

	return 0;
}
EXPORT_SYMBOL(mtk_lcm_create_input_packet);

void mtk_lcm_destroy_input_packet(struct mtk_lcm_ops_input_packet *input)
{
	if (input == NULL)
		return;

	if (input->data_count > 0) {
		if (input->data != NULL)
			LCM_KFREE(input->data,
				sizeof(struct mtk_lcm_ops_input) *
				input->data_count);
		input->data = NULL;
		input->data_count = 0;
	}

	if (input->condition_count > 0) {
		if (input->condition != NULL)
			LCM_KFREE(input->condition,
				sizeof(struct mtk_lcm_ops_input) *
				input->condition_count);
		input->condition = NULL;
		input->condition_count = 0;
	}
}
EXPORT_SYMBOL(mtk_lcm_destroy_input_packet);

int mtk_lcm_create_input(struct mtk_lcm_ops_input *input,
		unsigned int data_len, u8 name)
{
	if (IS_ERR_OR_NULL(input) || data_len == 0)
		return -EINVAL;

	LCM_KZALLOC(input->data, data_len, GFP_KERNEL);
	if (input->data == NULL) {
		DDPMSG("%s, %d, failed to allocate input data\n",
			__func__, __LINE__);
		return -ENOMEM;
	}
	input->length = data_len;
	input->name = name;

	return 0;
}
EXPORT_SYMBOL(mtk_lcm_create_input);

void mtk_lcm_destroy_input(struct mtk_lcm_ops_input *input)
{
	if (input == NULL || input->length == 0)
		return;

	LCM_KFREE(input->data, input->length);
	input->data = NULL;
	input->length = 0;
}
EXPORT_SYMBOL(mtk_lcm_destroy_input);

static inline int mtk_lcm_get_input_data(
		struct mtk_lcm_ops_input_packet *input,
		unsigned int name)
{
	int i = 0;

	for (i = 0; i < input->data_count; i++) {
		if (input->data[i].name == name) {
			if (IS_ERR_OR_NULL(input->data[i].data))
				return -EINVAL;
			return i;
		}
	}

	return -EFAULT;
}

static inline int mtk_lcm_get_input_condition(
		struct mtk_lcm_ops_input_packet *input,
		unsigned int name)
{
	int i = 0;

	for (i = 0; i < input->condition_count; i++) {
		if (input->condition[i].name == name) {
			if (IS_ERR_OR_NULL(input->condition[i].data))
				return -EINVAL;
			return i;
		}
	}

	return -EFAULT;
}

static int mtk_lcm_update_runtime_input(
	struct mtk_lcm_ops_input_packet *input,
	struct mtk_lcm_ops_data *op, u8 *data_out)
{
	size_t count = 0, size = 0;
	int index = 0, i = 0, id = 0;
	u8 *data = NULL;

	if (IS_ERR_OR_NULL(op) || IS_ERR_OR_NULL(input) ||
	    IS_ERR_OR_NULL(data_out)) {
		DDPPR_ERR("%s:invalid params\n", __func__);
		return -EINVAL;
	}

	count = op->param.buf_runtime_data.id_len;
	data = op->param.buf_runtime_data.data;
	size = op->param.buf_runtime_data.data_len;
	index = mtk_lcm_get_input_data(input,
			op->param.buf_runtime_data.name);
	if (index < 0 || size <= 0 ||
	    IS_ERR_OR_NULL(data)) {
		DDPPR_ERR("%s, %d, input:%u invalid id:%d, size:%u or data\n",
			__func__, __LINE__,
			op->param.buf_runtime_data.name,
			index, size);
		return -EINVAL;
	}

	if (count <= 0 || count > size) {
		DDPPR_ERR("%s:invalid func:%u of count:%u\n",
			__func__, op->type, count);
		return -EINVAL;
	}

	memcpy(data_out, data, size);
	for (i = 0; i < count; i++) {
		id = op->param.buf_runtime_data.id[i];
		if (id >= size) {
			DDPPR_ERR("%s:invalid id:%u of table:%u\n",
				__func__, id, size);
			return -EINVAL;
		}
		data_out[id] = *((u8 *)(input->data[index].data) + i);
		DDPDUMP("%s, %d, name:%u i:%u, id:%u, value:0x%x\n",
			__func__, __LINE__, index, i, id, data_out[id]);
	}

	return 0;
}

static int mtk_lcm_create_operation_group(struct mtk_panel_para_table *group,
	struct mtk_lcm_ops_input_packet *input,
	struct mtk_lcm_ops_table *table)
{
	struct mtk_lcm_ops_data *op = NULL;
	size_t size = 0;
	unsigned int id = 0;
	int index = 0;

	if (IS_ERR_OR_NULL(group)) {
		DDPMSG("%s, %d, invalid group\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	if (IS_ERR_OR_NULL(table) ||
	    table->size == 0) {
		DDPMSG("%s, %d, invalid table\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	list_for_each_entry(op, &table->list, node) {
		switch (op->type) {
		case MTK_LCM_CMD_TYPE_WRITE_BUFFER:
			size = op->param.buf_data.data_len;
			if (IS_ERR_OR_NULL(op->param.buf_data.data) ||
			    size <= 0 || size > ARRAY_SIZE(group[id].para_list)) {
				DDPPR_ERR("%s,%d, len:%u, array size:%u\n",
					__func__, __LINE__, size,
					ARRAY_SIZE(group[id].para_list));
				return -EINVAL;
			}
			group[id].count = size;
			memcpy(group[id].para_list, op->param.buf_data.data,
				op->param.buf_data.data_len);
			break;
		case MTK_LCM_CMD_TYPE_WRITE_BUFFER_CONDITION:
			if (IS_ERR_OR_NULL(op->param.buf_con_data.data) ||
			    IS_ERR_OR_NULL(input) ||
			    IS_ERR_OR_NULL(input->condition))
				return -EINVAL;
			index = mtk_lcm_get_input_condition(input,
					op->param.buf_con_data.name);
			if (index < 0 || index >= input->condition_count) {
				DDPPR_ERR("%s, %d, failed to get input, %d\n",
					__func__, __LINE__, index);
				return -EINVAL;
			}
			if (op->param.buf_con_data.condition ==
			    *(u8 *)input->condition[index].data) {
				size = op->param.buf_con_data.data_len;
				DDPDUMP("%s,%d, name:0x%x, condition:%u, %u\n", __func__, __LINE__,
					op->param.buf_con_data.name,
					op->param.buf_con_data.condition,
					*(u8 *)input->condition[index].data);
				if (size > ARRAY_SIZE(group[id].para_list)) {
					DDPPR_ERR("%s, %d, invalid size, %u\n",
						__func__, __LINE__, size);
					return -EINVAL;
				}
				group[id].count = size;
				memcpy(group[id].para_list, op->param.buf_con_data.data,
					op->param.buf_con_data.data_len);
			}
			break;
		case MTK_LCM_CMD_TYPE_WRITE_BUFFER_RUNTIME_INPUT:
			size = op->param.buf_runtime_data.data_len;
			if (size > ARRAY_SIZE(group[id].para_list)) {
				DDPPR_ERR("%s, %d, invalid size, %u\n",
					__func__, __LINE__, size);
				return -EINVAL;
			}
			if (mtk_lcm_update_runtime_input(input,
					op, group[id].para_list) < 0) {
				DDPPR_ERR("%s,%d: failed to update input\n",
					__func__, __LINE__);
				return -EFAULT;
			}
			group[id].count = size;
			break;
		case MTK_LCM_CMD_TYPE_READ_BUFFER:
		case MTK_LCM_CMD_TYPE_READ_CMD:
		case MTK_LCM_CMD_TYPE_WRITE_CMD:
		default:
			group[id].count = 0;
			DDPMSG("%s: not support func:%u\n", __func__, op->func);
			return -EINVAL;
		}

		if (group[id].count > 0)
			id++;
	}

	return id;
}

int mtk_panel_execute_callback(void *dsi, dcs_write_gce cb,
	void *handle, struct mtk_lcm_ops_table *table,
	struct mtk_lcm_ops_input_packet *input, const char *master)
{
	u8 *buf = NULL;
	struct mtk_lcm_ops_data *op = NULL;
	size_t size = 0;
	char owner[MTK_LCM_NAME_LENGTH] = { 0 };
	int ret = 0, index = 0;

	if (IS_ERR_OR_NULL(master))
		ret = snprintf(owner, MTK_LCM_NAME_LENGTH - 1, "unknown");
	else
		ret = snprintf(owner, MTK_LCM_NAME_LENGTH - 1, master);
	if (ret < 0 || ret >= MTK_LCM_NAME_LENGTH)
		DDPMSG("%s, failed at snprintf, %d", __func__, ret);
	ret = 0;

	if (IS_ERR_OR_NULL(table) ||
	    table->size == 0) {
		DDPMSG("%s, %d, owner:%s invalid table\n",
			__func__, __LINE__, owner);
		return -EINVAL;
	}

	list_for_each_entry(op, &table->list, node) {
		switch (op->type) {
		case MTK_LCM_CMD_TYPE_WRITE_BUFFER:
			if (IS_ERR_OR_NULL(op->param.buf_data.data) ||
			    op->param.buf_data.data_len <= 0) {
				ret = -EINVAL;
				goto out;
			}
			cb(dsi, handle, op->param.buf_data.data,
				op->param.buf_data.data_len);
			break;
		case MTK_LCM_CMD_TYPE_WRITE_BUFFER_CONDITION:
			if (IS_ERR_OR_NULL(op->param.buf_con_data.data) ||
			    IS_ERR_OR_NULL(input) ||
			    IS_ERR_OR_NULL(input->condition)) {
				ret = -EINVAL;
				goto out;
			}
			index = mtk_lcm_get_input_condition(input,
					op->param.buf_con_data.name);
			if (index < 0 || (unsigned int)index >= input->condition_count) {
				DDPPR_ERR("%s, %d, owner:%s failed to get input, %d\n",
					__func__, __LINE__, owner, index);
				ret = -EINVAL;
				goto out;
			}
			if (op->param.buf_con_data.condition ==
			    *(u8 *)input->condition[index].data) {
				DDPPR_ERR("%s,%d, owner:%s name:0x%x, condition:%u, %u\n",
					__func__, __LINE__, owner,
					op->param.buf_con_data.name,
					op->param.buf_con_data.condition,
					*(u8 *)input->condition[index].data);
				cb(dsi, handle, op->param.buf_con_data.data,
					op->param.buf_con_data.data_len);
			}
			break;
		case MTK_LCM_CMD_TYPE_WRITE_BUFFER_RUNTIME_INPUT:
			size = op->param.buf_runtime_data.data_len;
			LCM_KZALLOC(buf, size, GFP_KERNEL);
			if (buf == NULL) {
				DDPPR_ERR("%s,%d: owner:%s failed to allocate buf\n",
					__func__, __LINE__, owner);
				ret = -ENOMEM;
				goto out;
			}

			if (mtk_lcm_update_runtime_input(input,
					op, buf) < 0) {
				LCM_KFREE(buf, size);
				DDPPR_ERR("%s,%d: owner:%s failed to update input\n",
					__func__, __LINE__, owner);
				ret = -EFAULT;
				goto out;
			}

			cb(dsi, handle, buf, size);
			LCM_KFREE(buf, size);
			break;
		case MTK_LCM_CMD_TYPE_READ_BUFFER:
		case MTK_LCM_CMD_TYPE_READ_CMD:
		case MTK_LCM_CMD_TYPE_WRITE_CMD:
		default:
			ret = -EINVAL;
			DDPMSG("%s: not support func:%u from owner:%s\n",
				__func__, op->func, owner);
			break;
		}
	}

out:
#if MTK_LCM_DEBUG_DUMP
	DDPMSG("%s, %d, owner:%s, ret:%d\n",
		__func__, __LINE__, owner, ret);
#endif
	return ret;
}
EXPORT_SYMBOL(mtk_panel_execute_callback);

int mtk_panel_execute_callback_group(void *dsi, dcs_grp_write_gce cb,
	void *handle, struct mtk_lcm_ops_table *table,
	struct mtk_lcm_ops_input_packet *input, const char *master)
{
	struct mtk_panel_para_table *group = NULL;
	char owner[MTK_LCM_NAME_LENGTH] = { 0 };
	int ret = 0, group_size = 0;

	if (IS_ERR_OR_NULL(master))
		ret = snprintf(owner, MTK_LCM_NAME_LENGTH - 1, "unknown");
	else
		ret = snprintf(owner, MTK_LCM_NAME_LENGTH - 1, master);
	if (ret < 0 || ret >= MTK_LCM_NAME_LENGTH)
		DDPMSG("%s, failed at snprintf, %d", __func__, ret);
	ret = 0;

	if (IS_ERR_OR_NULL(table) ||
	    table->size == 0) {
		DDPPR_ERR("%s, %d, owner:%s invalid table\n",
			__func__, __LINE__, owner);
		return -EINVAL;
	}

	LCM_KZALLOC(group, sizeof(struct mtk_panel_para_table) *
		table->size, GFP_KERNEL);
	if (group == NULL) {
		DDPPR_ERR("%s:owner:%s failed to allocate group, count:%u\n",
			__func__, owner, table->size);
		return -ENOMEM;
	}

	group_size = mtk_lcm_create_operation_group(group,
		input, table);
	if (group_size <= 0) {
		DDPPR_ERR("%s:owner:%s failed to create group, ret:%d\n",
			__func__, owner, group_size);
		ret = -ENOMEM;
		goto out;
	}

	cb(dsi, handle, group, group_size);

out:
	LCM_KFREE(group, sizeof(struct mtk_panel_para_table) *
		table->size);
	return ret;
}
EXPORT_SYMBOL(mtk_panel_execute_callback_group);

static int mtk_lcm_init_ddic_msg(struct mipi_dsi_msg *msg, unsigned int cmd,
		unsigned int flag, unsigned int channel)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(msg))
		return -EINVAL;

	if (msg->rx_len > 0) {
		if (cmd == MTK_LCM_CMD_TYPE_READ_CMD) {
			msg->type = MIPI_DSI_DCS_READ;
		} else if (cmd == MTK_LCM_CMD_TYPE_READ_BUFFER) {
			switch (msg->tx_len) {
			case 0:
				msg->type = MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM;
				break;
			case 1:
				msg->type = MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM;
				break;
			case 2:
				msg->type = MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM;
				break;
			default:
				DDPPR_ERR("%s, %d, invalid read type, rx:%u, tx:%u\n",
						__func__, __LINE__, msg->rx_len, msg->tx_len);
				ret = -EINVAL;
				break;
			}
		}
	} else if (msg->tx_buf == NULL) {
		DDPPR_ERR("%s, %d, invalid write buffer, rx:%u, tx:%u\n",
			__func__, __LINE__, msg->rx_len, msg->tx_len);
		ret = -EINVAL;
	} else if (*((unsigned char *)msg->tx_buf) < 0xB0) {
		switch (msg->tx_len) {
		case 0:
			DDPPR_ERR("%s, %d, invalid dcs type, rx:%u, tx:%u\n",
				__func__, __LINE__, msg->rx_len, msg->tx_len);
			ret = -EINVAL;
			break;
		case 1:
			msg->type = MIPI_DSI_DCS_SHORT_WRITE;
			break;
		case 2:
			msg->type = MIPI_DSI_DCS_SHORT_WRITE_PARAM;
			break;
		default:
			msg->type = MIPI_DSI_DCS_LONG_WRITE;
			break;
		}
	} else {
		switch (msg->tx_len) {
		case 0:
			msg->type = MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM;
			break;
		case 1:
			msg->type = MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM;
			break;
		case 2:
			msg->type = MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM;
			break;
		default:
			msg->type = MIPI_DSI_GENERIC_LONG_WRITE;
			break;
		}
	}

	msg->channel = channel;
	if ((flag & MTK_LCM_DDIC_FLAG_HIGH_SPEED) == 0)
		msg->flags |= MIPI_DSI_MSG_USE_LPM;

	return ret;
}

static int mtk_lcm_init_ddic_packet(struct mtk_lcm_dsi_cmd_packet *packet,
	struct mtk_lcm_ops_input_packet *input,
	struct mtk_lcm_ops_table *table, struct mtk_lcm_ops_data **op_start,
	struct mtk_lcm_ops_data **op_end)
{
	struct mtk_lcm_ops_data *op = NULL;
	unsigned int prop = 0, len = 0;
	unsigned int cmd_size = 0, cmdq_size = 0, cmdq_size_cur = 0;
	int ret = 0, index = 0;

	if (IS_ERR_OR_NULL(packet)) {
		DDPMSG("%s, %d, invalid packet\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	prop = packet->prop;
	if (*op_start == NULL) {
		op = list_first_entry(&table->list,
				struct mtk_lcm_ops_data, node);
	} else
		op = *op_start;

	list_for_each_entry_from(op, &table->list, node) {
		struct mtk_lcm_dsi_cmd *cmd = NULL;
		unsigned char *buf = NULL;

		LCM_KZALLOC(cmd, sizeof(struct mtk_lcm_dsi_cmd), GFP_KERNEL);
		if (cmd == NULL) {
			DDPMSG("%s,%d: failed to allocate cmd\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		cmd->msg.rx_len = 0;
		cmd->msg.tx_len = 0;
		cmd->msg.flags = 0;
		cmd->tx_free = false;
		cmd->rx_free = false;

		switch (op->type) {
		case MTK_LCM_CMD_TYPE_READ_BUFFER:
		{
			unsigned int offset = op->param.cmd_data.rx_off;
			unsigned int len = op->param.cmd_data.rx_len;

			if (cmd_size > 0 &&
			    (prop & MTK_LCM_DSI_CMD_PROP_PACK) != 0) {
#if MTK_LCM_DEBUG_DUMP
				DDPMSG("%s, %d: pack end at read cmd:%u, sz:%u\n",
					__func__, __LINE__, op->type, cmd_size);
#endif
				break;
			}

			if (IS_ERR_OR_NULL(input) ||
				IS_ERR_OR_NULL(input->data)) {
				DDPPR_ERR("%s: err input\n", __func__, __LINE__);
				ret = -EINVAL;
				break;
			}

			index = mtk_lcm_get_input_data(input,
					MTK_LCM_INPUT_TYPE_READBACK);
			if (index < 0 || index >= input->data_count) {
				DDPPR_ERR("%s, %d, failed to get input, %d\n",
					__func__, __LINE__, index);
				ret = -EINVAL;
				break;
			}
			if (input->data[index].length < offset + len) {
				DDPPR_ERR("%s: err, data_count:%u,off:%u,len:%u\n",
					__func__, input->data_count, offset, len);
				ret = -ENOMEM;
				break;
			}

			cmd->msg.tx_len = op->param.cmd_data.tx_len;
			cmd->msg.tx_buf = op->param.cmd_data.tx_data;
			cmd->msg.rx_len = len;
			cmd->msg.rx_buf = &input->data[index].data[offset];
			ret = mtk_lcm_init_ddic_msg(&cmd->msg, op->type,
					op->param.cmd_data.flag, packet->channel);

			packet->prop &= ~MTK_LCM_DSI_CMD_PROP_PACK;
			packet->prop |= MTK_LCM_DSI_CMD_PROP_READ;
			break;
		}
		case MTK_LCM_CMD_TYPE_READ_CMD:
		{
			unsigned int offset = op->param.cmd_data.rx_off;
			unsigned int len = op->param.cmd_data.rx_len;

			if (cmd_size > 0 &&
			    (prop & MTK_LCM_DSI_CMD_PROP_PACK) != 0) {
#if MTK_LCM_DEBUG_DUMP
				DDPMSG("%s, %d: pack end at read cmd:%u, sz:%u\n",
					__func__, __LINE__, op->type, cmd_size);
#endif
				break;
			}

			if (IS_ERR_OR_NULL(input) ||
				IS_ERR_OR_NULL(input->data)) {
				DDPPR_ERR("%s: err input\n", __func__, __LINE__);
				ret = -EINVAL;
				break;
			}

			index = mtk_lcm_get_input_data(input,
					MTK_LCM_INPUT_TYPE_READBACK);
			if (index < 0 || index >= input->data_count) {
				DDPPR_ERR("%s, %d, failed to get input, %d\n",
					__func__, __LINE__, index);
				ret = -EINVAL;
				break;
			}
			if (input->data[index].length < offset + len) {
				DDPPR_ERR("%s: err, data_count:%u,off:%u,len:%u\n",
					__func__, input->data_count, offset, len);
				ret = -ENOMEM;
				break;
			}

			LCM_KZALLOC(buf, 1, GFP_KERNEL);
			if (buf == NULL) {
				DDPPR_ERR("%s,%d: failed to allocate buf\n",
					__func__, __LINE__);
				ret = -ENOMEM;
				break;
			}
			buf[0] = op->param.cmd_data.cmd;

			cmd->msg.tx_len = 1;
			cmd->msg.tx_buf = buf;
			cmd->msg.rx_len = len;
			cmd->msg.rx_buf = &input->data[index].data[offset];
			cmd->tx_free = true;
			ret = mtk_lcm_init_ddic_msg(&cmd->msg, op->type,
					op->param.cmd_data.flag, packet->channel);
			packet->prop &= ~MTK_LCM_DSI_CMD_PROP_PACK;
			packet->prop |= MTK_LCM_DSI_CMD_PROP_READ;
			break;
		}
		case MTK_LCM_CMD_TYPE_WRITE_BUFFER:
			cmd->msg.tx_len = op->param.buf_data.data_len;
			cmd->msg.tx_buf = op->param.buf_data.data;
			ret = mtk_lcm_init_ddic_msg(&cmd->msg, op->type,
					op->param.buf_data.flag, packet->channel);
			break;
		case MTK_LCM_CMD_TYPE_WRITE_CMD:
		{
			cmd->msg.tx_len = op->param.cmd_data.tx_len + 1;
			LCM_KZALLOC(buf, cmd->msg.tx_len, GFP_KERNEL);
			if (buf == NULL) {
				DDPPR_ERR("%s,%d: failed to allocate buf\n",
					__func__, __LINE__);
				ret = -ENOMEM;
				break;
			}

			buf[0] = op->param.cmd_data.cmd;
			memcpy(&buf[1], op->param.cmd_data.tx_data,
					op->param.cmd_data.tx_len);
			cmd->msg.tx_buf = buf;
			cmd->tx_free = true;
			ret = mtk_lcm_init_ddic_msg(&cmd->msg, op->type,
					op->param.buf_data.flag, packet->channel);
			break;
		}
		case MTK_LCM_CMD_TYPE_WRITE_BUFFER_CONDITION:
		{
			if (IS_ERR_OR_NULL(input) ||
				IS_ERR_OR_NULL(input->condition)) {
				DDPPR_ERR("%s: err input\n", __func__, __LINE__);
				ret = -EINVAL;
				break;
			}

			index = mtk_lcm_get_input_condition(input,
					op->param.buf_con_data.name);
			if (index < 0 || index >= input->condition_count) {
				DDPPR_ERR("%s, %d, failed to get input, %d\n",
					__func__, __LINE__, index);
				ret = -EINVAL;
				break;
			}
			if (op->param.buf_con_data.condition ==
			    *(u8 *)input->condition[index].data) {
				DDPDBG("%s,%d, name:0x%x, condition:%u, %u\n",
					__func__, __LINE__,
					op->param.buf_con_data.name,
					op->param.buf_con_data.condition,
					*(u8 *)input->condition[index].data);
				cmd->msg.tx_len = op->param.buf_con_data.data_len;
				cmd->msg.tx_buf = op->param.buf_con_data.data;
				ret = mtk_lcm_init_ddic_msg(&cmd->msg, op->type,
						op->param.buf_con_data.flag, packet->channel);
			} else
				*op_end = op;

			break;
		}
		case MTK_LCM_CMD_TYPE_WRITE_BUFFER_RUNTIME_INPUT:
		{
			len = op->param.buf_runtime_data.data_len;
			LCM_KZALLOC(buf, len, GFP_KERNEL);
			if (buf == NULL) {
				DDPPR_ERR("%s,%d: failed to allocate buf\n",
					__func__, __LINE__);
				ret = -ENOMEM;
				break;
			}

			if (mtk_lcm_update_runtime_input(input,
					op, buf) < 0) {
				LCM_KFREE(buf, len);
				DDPPR_ERR("%s,%d: failed to update input\n",
					__func__, __LINE__);
				ret = -EFAULT;
				break;
			}

			cmd->msg.tx_len = len;
			cmd->msg.tx_buf = buf;
			cmd->tx_free = true;
			ret = mtk_lcm_init_ddic_msg(&cmd->msg, op->type,
					op->param.buf_runtime_data.flag, packet->channel);
			break;
		}
		default:
			DDPDBG("%s, %d: not a valid ddic cmd:%u\n",
				__func__, __LINE__, op->type);
			break;
		}

		if ((prop & MTK_LCM_DSI_CMD_PROP_PACK) != 0) {
			if (cmd->msg.tx_len > 2)
				cmdq_size_cur = 1 + ((cmd->msg.tx_len + 3) / 4);
			else
				cmdq_size_cur = 1;
		}

		/* per package max support 512 cmdq settings*/
		if ((cmd->msg.tx_len == 0 &&
		    cmd->msg.rx_len == 0) || ret < 0 ||
		    cmdq_size_cur > (128 - cmdq_size)) {
#if MTK_LCM_DEBUG_DUMP
			DDPMSG(
				"%s/%d: stop packet tx:%u,rx:%u,ret:%d,sz:%u,cmdqsz:%u,tx_free:%d\n",
				__func__, __LINE__, cmd->msg.tx_len, cmd->msg.rx_len,
				ret, cmd_size, cmdq_size, cmd->tx_free);
#endif
			if (cmd->tx_free == true &&
				cmd->msg.tx_buf != NULL)
				LCM_KFREE(cmd->msg.tx_buf, cmd->msg.tx_len);
			LCM_KFREE(cmd, sizeof(struct mtk_lcm_dsi_cmd));
			cmd = NULL;
			break;
		}

#if MTK_LCM_DEBUG_DUMP
		DDPMSG("%s, %d, op:0x%x cmd:0x%x t:0x%x f:0x%x added, id:%u cmdqs:%u\n",
			__func__, __LINE__, (unsigned long)op, op->type, cmd->msg.type,
			cmd->msg.flags, cmd_size, cmdq_size_cur);
#endif
		list_add_tail(&cmd->list, &packet->cmd_list);
		*op_end = op;
		cmd_size++;
		cmdq_size += cmdq_size_cur;
	}

	if (ret == 0)
		ret = cmd_size;

	return ret;
}

void mtk_lcm_destroy_ddic_packet(struct mtk_lcm_dsi_cmd_packet *packet)
{
	struct mtk_lcm_dsi_cmd *cmd = NULL, *tmp = NULL;

	if (IS_ERR_OR_NULL(packet))
		return;

	list_for_each_entry_safe(cmd, tmp, &packet->cmd_list, list) {
		list_del(&cmd->list);
		if (cmd->tx_free == true && cmd->msg.tx_len > 0) {
			LCM_KFREE(cmd->msg.tx_buf, cmd->msg.tx_len);
			cmd->msg.tx_len = 0;
		}

		if (cmd->rx_free == true && cmd->msg.rx_len > 0) {
			LCM_KFREE(cmd->msg.rx_buf, cmd->msg.rx_len);
			cmd->msg.rx_len = 0;
		}
		LCM_KFREE(cmd, sizeof(struct mtk_lcm_dsi_cmd));
	}
	LCM_KFREE(packet, sizeof(struct mtk_lcm_dsi_cmd_packet));
}
EXPORT_SYMBOL(mtk_lcm_destroy_ddic_packet);

/* output: return the ddic cmd count has been executed */
int mtk_execute_func_ddic_package(struct mipi_dsi_device *dsi,
	void *handle, struct mtk_lcm_ops_table *table,
	struct mtk_lcm_ops_input_packet *input,
	mtk_dsi_ddic_handler_cb handler_cb,
	unsigned int prop,
	struct mtk_lcm_ops_data **op_start,
	struct mtk_lcm_ops_data **op_end)
{
	struct mtk_lcm_dsi_cmd_packet *packet = NULL;
	int ret = 0, size = 0;

	if (IS_ERR_OR_NULL(table) ||
	    table->size == 0) {
		DDPPR_ERR("%s, %d, invalid table\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	LCM_KZALLOC(packet, sizeof(struct mtk_lcm_dsi_cmd_packet),
			GFP_KERNEL);
	if (packet == NULL) {
		DDPPR_ERR("%s,%d: failed to allocate buf\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	packet->channel = dsi->channel;
	packet->prop = prop;
	INIT_LIST_HEAD(&packet->cmd_list);
	size = mtk_lcm_init_ddic_packet(packet, input,
				table, op_start, op_end);
	if (size < 0) {
		DDPPR_ERR("%s:failed to init packet, ret:%d\n",
			__func__, ret);
		ret = size;
		goto out;
	}

	if (size == 0 || list_empty(&packet->cmd_list)) {
		DDPDBG("%s, %d, cmd list is null, size:%u, list:0x%lx\n",
			__func__, __LINE__, size, (unsigned long)&packet->cmd_list);
		ret = 0; //it is ok when write may miss condition
		goto out;
	}

	ret = mtk_lcm_dsi_ddic_handler(dsi, handle,
				handler_cb, packet);
	if (ret < 0)
		DDPPR_ERR("%s:failed to handle packet, %d\n",
			__func__, ret);

out:
	if (packet != NULL && (ret < 0 ||
	    (packet->prop & MTK_LCM_DSI_CMD_PROP_ASYNC) == 0)) {
		mtk_lcm_destroy_ddic_packet(packet);
		packet = NULL;
	}

	if (ret >= 0)
		return size;

	return ret;
}

int mtk_execute_func_ddic_cmd(void *dev,
		struct mtk_lcm_ops_data *lcm_op,
		struct mtk_lcm_ops_input_packet *input)
{
	int ret = 0, index = 0;
	unsigned int size = 0;
	u8 *buf = NULL;

	if (IS_ERR_OR_NULL(lcm_op)) {
		DDPPR_ERR("%s %d: invalid cmd\n", __func__, __LINE__);
		return -EINVAL;
	}

	switch (lcm_op->type) {
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER:
		if (IS_ERR_OR_NULL(lcm_op->param.buf_data.data) ||
		    lcm_op->param.buf_data.data_len <= 0)
			return -EINVAL;
		ret = mtk_panel_dsi_dcs_write_buffer(
					(struct mipi_dsi_device *)dev,
					lcm_op->param.buf_data.data,
					lcm_op->param.buf_data.data_len);
		break;
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER_CONDITION:
		if (IS_ERR_OR_NULL(lcm_op->param.buf_con_data.data) ||
		    IS_ERR_OR_NULL(input) ||
		    IS_ERR_OR_NULL(input->condition))
			return -EINVAL;
		index = mtk_lcm_get_input_condition(input,
				lcm_op->param.buf_con_data.name);
		if (index < 0 || index >= input->condition_count) {
			DDPMSG("%s, %d, failed to get input, %d\n",
				__func__, __LINE__, index);
			return -EINVAL;
		}
		if (lcm_op->param.buf_con_data.condition ==
		    *(u8 *)input->condition[index].data) {
			DDPMSG("%s,%d, name:0x%x, condition:%u, %u\n", __func__, __LINE__,
				lcm_op->param.buf_con_data.name,
				lcm_op->param.buf_con_data.condition,
				*(u8 *)input->condition[index].data);
			ret = mtk_panel_dsi_dcs_write_buffer(
					(struct mipi_dsi_device *)dev,
					lcm_op->param.buf_con_data.data,
					lcm_op->param.buf_con_data.data_len);
		}
		break;
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER_RUNTIME_INPUT:
		size = lcm_op->param.buf_runtime_data.data_len;
		LCM_KZALLOC(buf, size, GFP_KERNEL);
		if (buf == NULL) {
			DDPPR_ERR("%s,%d: failed to allocate buf\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		if (mtk_lcm_update_runtime_input(input,
				lcm_op, buf) < 0) {
			LCM_KFREE(buf, size);
			DDPPR_ERR("%s,%d: failed to update input\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		ret = mtk_panel_dsi_dcs_write_buffer(
				(struct mipi_dsi_device *)dev,
				buf, size);
		LCM_KFREE(buf, size);
		break;
	case MTK_LCM_CMD_TYPE_READ_BUFFER:
	{
		unsigned int offset = lcm_op->param.cmd_data.rx_off;
		unsigned int len = lcm_op->param.cmd_data.rx_len;

		if (IS_ERR_OR_NULL(input) ||
		    IS_ERR_OR_NULL(input->data)) {
			DDPMSG("%s: err input\n", __func__, __LINE__);
			return -EINVAL;
		}

		index = mtk_lcm_get_input_data(input,
				MTK_LCM_INPUT_TYPE_READBACK);
		if (index < 0 || index >= input->data_count) {
			DDPMSG("%s, %d, failed to get input, %d\n",
				__func__, __LINE__, index);
			return -EINVAL;
		}
		if (input->data[index].length < offset + len) {
			DDPMSG("%s: err, data_count:%u,off:%u,len:%u\n",
				__func__, input->data_count, offset, len);
			return -ENOMEM;
		}
		ret = mtk_panel_dsi_dcs_read_buffer(
					(struct mipi_dsi_device *)dev,
					lcm_op->param.cmd_data.tx_data,
					lcm_op->param.cmd_data.tx_len,
					&input->data[index].data[offset],
					len);
		break;
	}
	case MTK_LCM_CMD_TYPE_WRITE_CMD:
		if (IS_ERR_OR_NULL(lcm_op->param.cmd_data.tx_data) ||
		    lcm_op->param.cmd_data.tx_len <= 0)
			return -EINVAL;
		ret = mtk_panel_dsi_dcs_write(
					(struct mipi_dsi_device *)dev,
					lcm_op->param.cmd_data.cmd,
					lcm_op->param.cmd_data.tx_data,
					lcm_op->param.cmd_data.tx_len);
		break;
	case MTK_LCM_CMD_TYPE_READ_CMD:
	{
		unsigned int offset = lcm_op->param.cmd_data.rx_off;
		unsigned int len = lcm_op->param.cmd_data.rx_len;

		if (IS_ERR_OR_NULL(input) ||
		    IS_ERR_OR_NULL(input->data)) {
			DDPMSG("%s: %d, err input\n", __func__, __LINE__);
			return -EINVAL;
		}

		index = mtk_lcm_get_input_data(input,
				MTK_LCM_INPUT_TYPE_READBACK);
		if (index < 0 || index >= input->data_count) {
			DDPMSG("%s, %d, failed to get input, %d\n",
				__func__, __LINE__, index);
			return -EINVAL;
		}
		if (input->data[index].length < offset + len) {
			DDPMSG("%s: err, data_count:%u,off:%u,len:%u\n",
				__func__, input->data_count, offset, len);
			return -ENOMEM;
		}

		ret = mtk_panel_dsi_dcs_read(
					(struct mipi_dsi_device *)dev,
					lcm_op->param.cmd_data.cmd,
					&input->data[index].data[lcm_op->param.cmd_data.rx_off],
					len);
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
		if (ret < 0)
			DDPMSG("%s, reset failed, %d\n",
				__func__, ret);
		break;
	case MTK_LCM_UTIL_TYPE_POWER_ON:
		ret = mtk_drm_gateic_power_on(lcm_op->func);
		if (ret < 0)
			DDPMSG("%s, power on failed, %d\n",
				__func__, ret);
		break;
	case MTK_LCM_UTIL_TYPE_POWER_OFF:
		ret = mtk_drm_gateic_power_off(lcm_op->func);
		if (ret < 0)
			DDPMSG("%s, power off failed, %d\n",
				__func__, ret);
		break;
	case MTK_LCM_UTIL_TYPE_POWER_VOLTAGE:
		ret = mtk_drm_gateic_set_voltage(data, lcm_op->func);
		if (ret < 0)
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
			"lcm-gpio-list", id, GPIOD_OUT_HIGH);
		if (IS_ERR_OR_NULL(gpiod)) {
			DDPPR_ERR("%s: invalid gpiod:%u\n", __func__, id);
			return PTR_ERR(gpiod);
		}

		ret = gpiod_direction_output(gpiod, !!data);
		devm_gpiod_put(&dev, gpiod);
		break;
	case MTK_LCM_GPIO_TYPE_DIR_INPUT:
		gpiod = devm_gpiod_get_index(&dev,
			"lcm-gpio-list", id, GPIOD_OUT_HIGH);
		if (IS_ERR_OR_NULL(gpiod)) {
			DDPPR_ERR("%s: invalid gpiod:%u\n", __func__, id);
			return PTR_ERR(gpiod);
		}

		ret = gpiod_direction_input(gpiod);
		devm_gpiod_put(&dev, gpiod);
		break;
	case MTK_LCM_GPIO_TYPE_OUT:
		gpiod = devm_gpiod_get_index(&dev,
			"lcm-gpio-list", id, GPIOD_OUT_HIGH);
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

#define MTK_LCM_DDIC_LEGACY_FLOW (0)
int mtk_panel_execute_operation(struct mipi_dsi_device *dev,
		struct mtk_lcm_ops_table *table,
		const struct mtk_panel_resource *panel_resource,
		struct mtk_lcm_ops_input_packet *input,
		void *handle, mtk_dsi_ddic_handler_cb handler_cb,
		unsigned int prop, const char *master)
{
	struct mtk_lcm_ops_data *op = NULL, *op_end = NULL;
	int ret = 0, i = 0;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(table) ||
	    table->size == 0) {
		DDPINFO("%s: \"%s\" is empty, size:%u\n", __func__,
			master == NULL ? "unknown" : master,
			table->size);
		return 0;
	}

#if MTK_LCM_DEBUG_DUMP
	DDPMSG("%s, %d, master:%s, size:%u\n",
		__func__, __LINE__,
		master == NULL ? "unknown" : master, table->size);
#endif
	list_for_each_entry(op, &table->list, node) {
		if (IS_ERR_OR_NULL(op)) {
			DDPMSG("%s: invalid %s-op[%d]\n", __func__,
				master == NULL ? "unknown" : master, i);
			ret = -EINVAL;
			break;
		}

		if (op->func != MTK_LCM_FUNC_DSI) {
			DDPMSG("%s, %d, not support %s-op[%d]:%d\n",
				__func__, __LINE__,
				master == NULL ? "unknown" : master,
				i, op->func);
			return -EINVAL;
		}

#if MTK_LCM_DEBUG_DUMP
		if (master != NULL &&
			strcmp(master, "msync_request_mte") == 0)
			dump_lcm_ops_func(op, NULL, i, master);
#endif

		if (op->type > MTK_LCM_UTIL_TYPE_START &&
		    op->type < MTK_LCM_UTIL_TYPE_END) {
			ret = mtk_execute_func_util(op);
#if MTK_LCM_DEBUG_DUMP
			DDPMSG("%s, %d, ret:%d\n", __func__, __LINE__, ret);
#endif
		} else if (op->type > MTK_LCM_CMD_TYPE_START &&
		    op->type < MTK_LCM_CMD_TYPE_END) {
			if (MTK_LCM_DDIC_LEGACY_FLOW) {
				ret = mtk_execute_func_ddic_cmd(dev, op, input);
			} else {
				op_end = NULL;
				ret = mtk_execute_func_ddic_package(dev, handle, table,
						input, handler_cb, prop, &op, &op_end);
				if (ret >= 0 && op_end != NULL) {
#if MTK_LCM_DEBUG_DUMP
					DDPMSG("%s/%d, do %s[%d:0x%lx-%d:0x%lx],end:0x%x\n",
						__func__, __LINE__,
						master == NULL ? "unknown" : master, i,
						(unsigned long)op, i + ret - 1,
						(unsigned long)op_end, op_end->type);
#endif
					i = i + ret - 1;
					ret = 0;
					op = op_end;
				} else if (op_end == NULL) {
					DDPPR_ERR("%s, %d, not support %s-op[%d] cmd:0x%x\n",
						__func__, __LINE__,
						master == NULL ? "unknown" : master, i, op->type);
					ret = -EFAULT;
				} else if (ret < 0) {
					DDPPR_ERR(
						"%s, %d, failed to execute %s-op[%d] cmd:0x%x, ret:%d\n",
						__func__, __LINE__,
						master == NULL ? "unknown" : master, i,
						op->type, ret);
				}
			}
		} else if (op->type > MTK_LCM_GPIO_TYPE_START &&
			op->type < MTK_LCM_GPIO_TYPE_END) {
#if MTK_LCM_DEBUG_DUMP
			DDPMSG("%s-op%d, func:%u, type:%u, execute cust ops\n",
				master == NULL ? "unknown" : master, i,
				op->func, op->type);
#endif
			mtk_execute_func_gpio(op,
					&panel_resource->params.dsi_params.lcm_gpio_dev,
					panel_resource->params.dsi_params.lcm_pinctrl_name,
					panel_resource->params.dsi_params.lcm_pinctrl_count);
		} else if (op->type > MTK_LCM_CUST_TYPE_START &&
			op->type < MTK_LCM_CUST_TYPE_END) {
			if (IS_ERR_OR_NULL(panel_resource->cust)) {
				DDPPR_ERR("%s, %d, no cust for cmd:%u\n",
					__func__, __LINE__, op->type);
				return -EINVAL;
			}
			if (IS_ERR_OR_NULL(panel_resource->cust->execute_ops)) {
				DDPPR_ERR("%s, %d, no cust ops for cmd:%u\n",
					__func__, __LINE__, op->type);
				return -EINVAL;
			}

#if MTK_LCM_DEBUG_DUMP
			DDPMSG("%s-op%d, func:%u, type:%u, execute cust ops\n",
				master == NULL ? "unknown" : master,
				i, op->func, op->type);
#endif
			ret = panel_resource->cust->execute_ops(op, input);
		} else {
			DDPMSG("%s, %d, invalid op:0x%x\n", __func__, __LINE__, op->type);
			ret = -EINVAL;
		}

		if (ret < 0) {
			DDPPR_ERR("%s: %s-op[%d] func:%u, type:%u, failed:%d, total:%u\n",
				__func__, master == NULL ? "unknown" : master,
				i, op->func, op->type, ret, table->size);
			dump_lcm_ops_func(op, NULL, i, __func__);
			break;
		}
		i++;
	}

#if MTK_LCM_DEBUG_DUMP
	DDPMSG("%s, %d, finished %s, i:%u, total:%u, ret:%d\n",
		__func__, __LINE__,
		master == NULL ? "unknown" : master,
		i, table->size, ret);
#endif
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

void mtk_lcm_dump_all(char func, struct mtk_panel_resource *resource)
{
	dump_lcm_params_basic(&resource->params);

	switch (func) {
	case MTK_LCM_FUNC_DBI:
		dump_lcm_params_dbi(&resource->params.dbi_params, resource->cust);
		dump_lcm_ops_dbi(resource->ops.dbi_ops,
				&resource->params.dbi_params, resource->cust);
		break;
	case MTK_LCM_FUNC_DPI:
		dump_lcm_params_dpi(&resource->params.dpi_params, resource->cust);
		dump_lcm_ops_dpi(resource->ops.dpi_ops,
				&resource->params.dpi_params, resource->cust);
		break;
	case MTK_LCM_FUNC_DSI:
		dump_lcm_params_dsi(&resource->params.dsi_params, resource->cust);
		dump_lcm_ops_dsi(resource->ops.dsi_ops,
				&resource->params.dsi_params, resource->cust);
		break;
	default:
		DDPDUMP("%s, invalid func:%d\n", __func__, func);
		break;
	}
}

static void free_lcm_ops_data(struct mtk_lcm_ops_data *lcm_op,
	const struct mtk_panel_cust *cust)
{
	if (IS_ERR_OR_NULL(lcm_op))
		return;

	if (lcm_op->type > MTK_LCM_CUST_TYPE_START &&
	    lcm_op->type < MTK_LCM_CUST_TYPE_END) {
		if (IS_ERR_OR_NULL(cust) ||
		    IS_ERR_OR_NULL(cust->free_ops))
			return;
		cust->free_ops(lcm_op);
		return;
	}

	switch (lcm_op->type) {
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER:
		if (lcm_op->param.buf_data.data != NULL &&
			lcm_op->param.buf_data.data_len > 0) {
			LCM_KFREE(lcm_op->param.buf_data.data,
				lcm_op->param.buf_data.data_len + 1);
			lcm_op->param.buf_data.data_len = 0;
		}
		break;
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER_CONDITION:
		if (lcm_op->param.buf_con_data.data != NULL &&
			lcm_op->param.buf_con_data.data_len > 0) {
			LCM_KFREE(lcm_op->param.buf_con_data.data,
				lcm_op->param.buf_con_data.data_len + 1);
			lcm_op->param.buf_con_data.data_len = 0;
		}
		break;
	case MTK_LCM_CMD_TYPE_WRITE_BUFFER_RUNTIME_INPUT:
		if (lcm_op->param.buf_runtime_data.id != NULL &&
			lcm_op->param.buf_runtime_data.id_len > 0) {
			LCM_KFREE(lcm_op->param.buf_runtime_data.id,
				lcm_op->param.buf_runtime_data.id_len + 1);
			lcm_op->param.buf_runtime_data.id_len = 0;
		}
		if (lcm_op->param.buf_runtime_data.data != NULL &&
			lcm_op->param.buf_runtime_data.data_len > 0) {
			LCM_KFREE(lcm_op->param.buf_runtime_data.data,
				lcm_op->param.buf_runtime_data.data_len + 1);
			lcm_op->param.buf_runtime_data.data_len = 0;
		}
		break;
	case MTK_LCM_CMD_TYPE_WRITE_CMD:
		if (lcm_op->param.cmd_data.tx_data != NULL &&
			lcm_op->size > 0) {
			LCM_KFREE(lcm_op->param.cmd_data.tx_data,
				lcm_op->param.cmd_data.tx_len + 1);
			lcm_op->size = 0;
		}
		break;
	case MTK_LCM_CMD_TYPE_READ_BUFFER:
	case MTK_LCM_CMD_TYPE_READ_CMD:
		if (lcm_op->param.cmd_data.tx_data != NULL) {
			LCM_KFREE(lcm_op->param.cmd_data.tx_data,
				lcm_op->param.cmd_data.tx_len + 1);
			lcm_op->param.cmd_data.tx_len = 0;
		}
		break;
	default:
		break;
	}
}

void free_lcm_ops_table(struct mtk_lcm_ops_table *table,
	const struct mtk_panel_cust *cust)
{
	struct mtk_lcm_ops_data *op = NULL, *tmp = NULL;

	if (IS_ERR_OR_NULL(table) || table->size == 0)
		return;

	list_for_each_entry_safe(op, tmp, &table->list, node) {
		list_del(&op->node);
		free_lcm_ops_data(op, cust);
		LCM_KFREE(op, sizeof(struct mtk_lcm_ops_data));
	}
	table->size = 0;
}
EXPORT_SYMBOL(free_lcm_ops_table);

static void free_lcm_params(char func, struct mtk_lcm_params *params,
	const struct mtk_panel_cust *cust)
{
	if (IS_ERR_OR_NULL(params))
		return;

	switch (func) {
	case MTK_LCM_FUNC_DBI:
		free_lcm_params_dbi(&params->dbi_params, cust);
		break;
	case MTK_LCM_FUNC_DPI:
		free_lcm_params_dpi(&params->dpi_params, cust);
		break;
	case MTK_LCM_FUNC_DSI:
		free_lcm_params_dsi(&params->dsi_params, cust);
		break;
	default:
		break;
	}
}

static void free_lcm_ops(char func, struct mtk_lcm_ops *ops,
	const struct mtk_panel_cust *cust)
{
	if (ops == NULL)
		return;

	switch (func) {
	case MTK_LCM_FUNC_DBI:
		free_lcm_ops_dbi(ops->dbi_ops, cust);
		ops->dbi_ops = NULL;
		break;
	case MTK_LCM_FUNC_DPI:
		free_lcm_ops_dpi(ops->dpi_ops, cust);
		ops->dpi_ops = NULL;
		break;
	case MTK_LCM_FUNC_DSI:
		free_lcm_ops_dsi(ops->dsi_ops, cust);
		ops->dsi_ops = NULL;
		break;
	default:
		break;
	}
}

void free_lcm_resource(char func, struct mtk_panel_resource *data)
{
	const struct mtk_panel_cust *cust = data->cust;
	if (IS_ERR_OR_NULL(data))
		return;

	DDPMSG("%s, %d\n", __func__, __LINE__);
	free_lcm_ops(func, &data->ops, cust);
	free_lcm_params(func, &data->params, cust);

	if (cust != NULL && cust->free_params != NULL)
		cust->free_params(func);
	LCM_KFREE(data, sizeof(struct mtk_panel_resource));
}
