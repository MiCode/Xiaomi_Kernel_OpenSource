// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include "mtk_drm_panel_cust.h"

static struct cust_lcm_params cust_params;
static struct cust_lcm_ops_table cust_ops_dsi;
static const struct mtk_panel_cust simple_panel_cust;
static bool g_init_done;

static int simple_parse_params(struct device_node *np)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(np)) {
		DDPPR_ERR("%s: invalid dts node\n", __func__);
		return -EINVAL;
	}
	DDPMSG("%s, %d\n", __func__, __LINE__);

	memset(&cust_params, 0x0, sizeof(struct cust_lcm_params));
	cust_params.name = kzalloc(128, GFP_KERNEL);
	if (cust_params.name == NULL) {
		DDPMSG("%s,%d: failed to allocate data\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	ret = of_property_read_string(np, "lcm-params-cust_name",
			(const char **)&cust_params.name);
	if (ret < 0) {
		DDPPR_ERR("%s, failed to get cust lcm drv name, ret:%d\n",
			__func__, ret);
		ret = snprintf(cust_params.name, 127, "unknown");
		if (ret < 0)
			DDPMSG("%s, failed to set name\n", __func__);
	}

	mtk_lcm_dts_read_u32(np, "lcm-params-cust_type",
			&cust_params.type);

	DDPMSG("%s, name:%s, type:%u, ret:%d\n", __func__,
		cust_params.name == NULL ? "unknown" : cust_params.name,
		cust_params.type, ret);

	return 0;
}

static int simple_parse_ops(struct mtk_lcm_ops_data *lcm_op,
		u8 *dts, unsigned int flag_len)
{
	unsigned int i = 0;
	struct cust_lcm_data *data = NULL;
	unsigned int flag_off = MTK_LCM_DATA_OFFSET + flag_len;

	DDPMSG("%s, %d, flag_len:%u\n", __func__, __LINE__, flag_len);
	if (IS_ERR_OR_NULL(lcm_op) ||
		IS_ERR_OR_NULL(dts)) {
		DDPMSG("%s,%d: invalid parameters\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	data = (struct cust_lcm_data *)&lcm_op->param.cust_data;
	switch (lcm_op->type) {
	case MTK_SAMPLE_CUST_TYPE_OP0:
		/* type size flag3 flag2 flag1 flag0 data0 data1 ... */
		for (i = 0; i < flag_len; i++)
			data->flag |=
				(dts[MTK_LCM_DATA_OFFSET + i] <<
					((flag_len - i - 1) * 8));
		data->data_len = lcm_op->size - flag_len;
		data->data = kzalloc(roundup(data->data_len + 1, 4), GFP_KERNEL);
		if (IS_ERR_OR_NULL(data->data)) {
			DDPPR_ERR("%s,%d: failed to allocate data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		memcpy(data->data, dts + flag_off, data->data_len);
		*(data->data + data->data_len) = '\0';
		break;
	case MTK_SAMPLE_CUST_TYPE_OP1:
		/* type size flag3 flag2 flag1 flag0 id data0 data1 ... */
		for (i = 0; i < flag_len; i++)
			data->flag |=
				(dts[MTK_LCM_DATA_OFFSET + i] <<
					((flag_len - i - 1) * 8));
		data->id = dts[flag_off];
		data->data_len = lcm_op->size - flag_len - 1;
		data->data = kzalloc(roundup(data->data_len + 1, 4), GFP_KERNEL);
		if (IS_ERR_OR_NULL(data->data)) {
			DDPPR_ERR("%s,%d: failed to allocate data\n",
				__func__, __LINE__);
			return -ENOMEM;
		}
		memcpy(data->data, dts + flag_off + 1, data->data_len);
		*(data->data + data->data_len) = '\0';
		break;
	default:
		DDPPR_ERR("%s %d: invalid type:%d\n",
			__func__, __LINE__, lcm_op->type);
		return -EINVAL;
	}

	dump_lcm_ops_func(lcm_op, &simple_panel_cust, 0, __func__);
	DDPMSG("%s--\n", __func__);
	return 0;
}

static int simple_parse_ops_table(struct device_node *np,
		unsigned int flag_len)
{
	int ret = 0;
	struct device_node *type_np = NULL;

	if (IS_ERR_OR_NULL(np)) {
		DDPPR_ERR("%s: invalid dts node\n", __func__);
		return -EINVAL;
	}
	DDPMSG("%s, %d\n", __func__, __LINE__);

	if (cust_ops_dsi.pre_prepare.size > 0)
		return 0;

	for_each_available_child_of_node(np, type_np) {
		if (of_device_is_compatible(type_np,
				"mediatek,lcm-ops-dsi")) {
			ret = parse_lcm_ops_func(type_np,
						&cust_ops_dsi.pre_prepare, "cust_pre_prepare_table",
						flag_len, MTK_LCM_FUNC_DSI,
						&simple_panel_cust, MTK_LCM_PHASE_KERNEL);
			if (ret <= 0)
				DDPMSG("%s, %d failed to parsing prepare_table, ret:%d\n",
					__func__, __LINE__, ret);
			else
				dump_lcm_ops_table(&cust_ops_dsi.pre_prepare,
					&simple_panel_cust, "cust_pre_prepare_table");
		}
	}

	DDPMSG("%s, %d, ret:%d\n", __func__, __LINE__, ret);
	return ret;
}

static void simple_dump_params(void)
{
	if (IS_ERR_OR_NULL(cust_params.name))
		DDPDUMP("%s, >>> name:unknown\n", __func__);
	else
		DDPDUMP("%s, >>> name:%s\n",
			__func__, cust_params.name);

	DDPDUMP("%s, >>> type:%u\n",
		__func__, cust_params.type);
}

static void simple_dump_ops(struct mtk_lcm_ops_data *op,
		const char *owner, unsigned int id)
{
	unsigned int i = 0;
	struct cust_lcm_data *data = NULL;

	if (IS_ERR_OR_NULL(op) ||
		IS_ERR_OR_NULL(owner) ||
		op->type < MTK_LCM_CUST_TYPE_START ||
		op->type > MTK_LCM_CUST_TYPE_END)
		return;

	data = (struct cust_lcm_data *)&op->param.cust_data;
	switch (op->type) {
	case MTK_SAMPLE_CUST_TYPE_OP0:
		DDPDUMP("[%s-%u] CUST_OP0 flag:0x%x,len:%u\n",
			owner, id, data->flag, data->data_len);
		for (i = 0; i < data->data_len; i++)
			DDPDUMP(">>> data:0x%x\n", data->data[i]);
		break;
	case MTK_SAMPLE_CUST_TYPE_OP1:
		DDPDUMP("[%s-%u] CUST_OP1 flag:0x%x,id:0x%x,len:%u\n",
			owner, id, data->flag,
			data->id, data->data_len);
		for (i = 0; i < data->data_len; i++)
			DDPDUMP(">>> data:0x%x\n", data->data[i]);
		break;
	default:
		DDPDUMP("[%s-%u] invalid op:%u\n",
			owner, id, op->type);
		break;
	}
}

static void simple_dump_ops_table(const char *owner, char func)
{
	DDPDUMP("====== %s func:%d ========\n", __func__, func);
	if (func != MTK_LCM_FUNC_DSI)
		goto end;

	if (cust_ops_dsi.pre_prepare.size > 0)
		dump_lcm_ops_table(&cust_ops_dsi.pre_prepare,
				&simple_panel_cust, "cust_pre_prepare");

end:
	DDPDUMP("========================\n", __func__);
}

static int simple_execute_ops(struct mtk_lcm_ops_data *op,
		struct mtk_lcm_ops_input_packet *input)
{
	int ret = 0;
	struct cust_lcm_data *data = NULL;

	if (IS_ERR_OR_NULL(op) ||
		op->type < MTK_LCM_CUST_TYPE_START ||
		op->type > MTK_LCM_CUST_TYPE_END)
		return -EINVAL;

	data = (struct cust_lcm_data *)&op->param.cust_data;
	switch (op->type) {
	case MTK_SAMPLE_CUST_TYPE_OP0:
		DDPMSG("%s, %d, execute op0 flag:0x%x,data_len:%u\n",
			__func__, __LINE__, data->flag, data->data_len);
		simple_dump_ops(op, __func__, 0);
		break;
	case MTK_SAMPLE_CUST_TYPE_OP1:
		DDPMSG("%s, %d, execute op1 flag:0x%x,id:0x%x,data_len:%u\n",
			__func__, __LINE__, data->flag,
			data->id, data->data_len);
		simple_dump_ops(op, __func__, 1);
		break;
	default:
		DDPMSG("%s, %d, invalid op:%u\n",
			__func__, __LINE__, op->type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void simple_free_ops(struct mtk_lcm_ops_data *op)
{
	struct cust_lcm_data *data = NULL;

	if (IS_ERR_OR_NULL(op) ||
		op->type < MTK_LCM_CUST_TYPE_START ||
		op->type > MTK_LCM_CUST_TYPE_END)
		return;

	data = (struct cust_lcm_data *)&op->param.cust_data;
	switch (op->type) {
	case MTK_SAMPLE_CUST_TYPE_OP0:
	case MTK_SAMPLE_CUST_TYPE_OP1:
		if (data->data_len == 0 &&
			IS_ERR_OR_NULL(data->data))
			break;
		kfree(data->data);
		data->data = NULL;
		data->data_len = 0;
		break;
	default:
		DDPMSG("%s:invalid op:%u\n", __func__, op->type);
		break;
	}
	DDPMSG("%s, %d\n", __func__, __LINE__);
}

static void simple_free_ops_table(void)
{
	DDPMSG("%s, %d\n", __func__, __LINE__);
	if (cust_ops_dsi.pre_prepare.size == 0)
		return;

	free_lcm_ops_table(&cust_ops_dsi.pre_prepare, &simple_panel_cust);
}

static void simple_free_params(unsigned int func)
{
	kfree(cust_params.name);
	cust_params.name = NULL;
	memset(&cust_params, 0x0, sizeof(struct cust_lcm_params));
	DDPMSG("%s, %d\n", __func__, __LINE__);
}

static int simple_panel_unprepare(struct drm_panel *panel)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	struct mipi_dsi_device *dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	int ret = 0;

	DDPMSG("%s+\n", __func__);
	if (IS_ERR_OR_NULL(ops))
		return -EINVAL;

	if (ctx_dsi->backlight) {
		ctx_dsi->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx_dsi->backlight);
	}

	ret = mtk_panel_execute_operation(dsi_dev,
			&ops->unprepare, ctx_dsi->panel_resource,
			NULL, NULL, NULL, prop, "cust_panel_unprepare");

	if (ret < 0) {
		DDPPR_ERR("%s,%d: failed to do panel unprepare\n",
			__func__, __LINE__);
		return ret;
	}

	ret = mtk_drm_gateic_power_off(MTK_LCM_FUNC_DSI);
	if (ret < 0)
		DDPPR_ERR("%s, gate ic power off failed, %d\n",
			__func__, ret);

	DDPMSG("%s- %d\n", __func__, ret);
	return 0;
}

static int simple_panel_reset(struct drm_panel *panel, int on)
{
	DDPMSG("%s skip reset:%d\n", __func__, on);
	return 0;
}

static int simple_cust_funcs(struct drm_panel *panel,
	int cmd, void *params, void *handle, void **output)
{
	struct mtk_panel_context *ctx_dsi = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	unsigned int prop = 0;
	int ret = 0;
	struct lcm_sample_cust_data *data =
			(struct lcm_sample_cust_data *)(*output);

	DDPMSG("%s++, cmd:%d\n", __func__, cmd);
	if (IS_ERR_OR_NULL(data))
		return -EFAULT;

	switch (cmd) {
	case LCM_CUST_CMD_GET_NAME:
		if (IS_ERR_OR_NULL(data->name))
			return -EINVAL;
		ret = snprintf(data->name, MTK_LCM_NAME_LENGTH - 1,
			"%s", cust_params.name);
		break;
	case LCM_CUST_CMD_GET_TYPE:
		data->type = cust_params.type;
		break;
	case LCM_CUST_CMD_PRE_PREPARE:
		if (cust_ops_dsi.pre_prepare.size == 0)
			break;

		ctx_dsi = panel_to_lcm(panel);
		dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
		prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
		ret = mtk_panel_execute_operation(dsi_dev,
				&cust_ops_dsi.pre_prepare, ctx_dsi->panel_resource,
				NULL, handle, NULL, prop, "cust_pre_prepare");
		if (ret < 0)
			DDPPR_ERR("%s, failed at pre_prepare, ret:%d\n",
				__func__, ret);

		break;
	default:
		DDPMSG("%s: invalid cmd:%d\n", __func__, cmd);
		break;

	}
	DDPMSG("%s--, cmd:%d, ret:%d\n", __func__, cmd, ret);
	return ret;
}

static const struct mtk_panel_cust simple_panel_cust = {
	.funcs = {
		.unprepare = simple_panel_unprepare,
	},
	.ext_funcs = {
		.reset = simple_panel_reset,
	},
	.cust_funcs = simple_cust_funcs,
	.parse_params = simple_parse_params,
	.parse_ops_table = simple_parse_ops_table,
	.parse_ops = simple_parse_ops,
	.execute_ops = simple_execute_ops,
	.dump_params = simple_dump_params,
	.dump_ops_table = simple_dump_ops_table,
	.dump_ops = simple_dump_ops,
	.free_params = simple_free_params,
	.free_ops_table = simple_free_ops_table,
	.free_ops = simple_free_ops,
};

int mtk_simple_lcm_cust_drv_deinit(char func)
{
	int ret = 0;

	DDPMSG("%s+\n", __func__);
	if (g_init_done == false)
		return 0;

	ret = mtk_panel_deregister_drv_customization_funcs(
			func, &simple_panel_cust);
	if (ret < 0)
		DDPMSG("%s, Failed to unregister cust panel funcs: %d\n",
			__func__, ret);
	else
		g_init_done = false;

	DDPMSG("%s- ret:%d, deinit_done:%d\n",
		__func__, ret, g_init_done);
	return ret;
}

int mtk_simple_lcm_cust_drv_init(char func)
{
	int ret = 0;

	DDPMSG("%s+\n", __func__);
	if (g_init_done == true)
		return 0;

	ret = mtk_panel_register_drv_customization_funcs(
			MTK_LCM_FUNC_DSI, &simple_panel_cust);
	if (ret < 0 && ret != -EPROBE_DEFER)
		DDPPR_ERR("%s, Failed to register cust panel funcs: %d\n",
			__func__, ret);
	else
		g_init_done = true;

	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static int simple_lcm_cust_drv_probe(struct platform_device *pdev)
{
	int ret = 0;

	ret = mtk_simple_lcm_cust_drv_init(MTK_LCM_FUNC_DSI);

	DDPMSG("%s, ret:%d\n", __func__, ret);
	return ret;
}

static int simple_lcm_cust_drv_remove(struct platform_device *pdev)
{
	int ret = 0;

	ret = mtk_simple_lcm_cust_drv_deinit(MTK_LCM_FUNC_DSI);

	DDPMSG("%s, ret:%d\n", __func__, ret);
	return ret;
}

static const struct of_device_id simple_lcm_cust_of_match[] = {
	{ .compatible = "mediatek,cust-lcm-drv-sample", },
	{ }
};

MODULE_DEVICE_TABLE(of, simple_lcm_cust_of_match);

struct platform_driver simple_lcm_cust_driver = {
	.probe = simple_lcm_cust_drv_probe,
	.remove = simple_lcm_cust_drv_remove,
	.driver = {
		.name = "cust-lcm-drv-sample",
		.owner = THIS_MODULE,
		.of_match_table = simple_lcm_cust_of_match,
	},
};

static int __init simple_lcm_cust_drv_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&simple_lcm_cust_driver);
	if (ret < 0)
		DDPPR_ERR("%s: Failed to register driver: %d\n",
			  __func__, ret);
	return ret;
}

static void __exit simple_lcm_cust_drv_exit(void)
{
	DDPMSG("%s,%d\n", __func__, __LINE__);
	platform_driver_unregister(&simple_lcm_cust_driver);
}
module_init(simple_lcm_cust_drv_init);
module_exit(simple_lcm_cust_drv_exit);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, sample of customized panel driver");
MODULE_LICENSE("GPL v2");
